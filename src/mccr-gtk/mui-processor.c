/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mccr-gtk - GTK+ tool to manage MagTek Credit Card Readers
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 * Copyright (C) 2017 Zodiac Inflight Innovations
 * Copyright (C) 2017 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <glib-object.h>
#include <gio/gio.h>
#include <wchar.h>
#include <string.h>

#include <common.h>

#include <mccr.h>

#include "mui-processor.h"

#define DEFAULT_WAIT_SWIPE_TIMEOUT_MS 1000

G_DEFINE_TYPE (MuiProcessor, mui_processor, G_TYPE_OBJECT)

enum {
    PROP_0,
    PROP_PATH,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

enum {
    REPORT_ITEM,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _MuiProcessorPrivate {
    /* Properties */
    gchar      *path;

    /* Internal thread */
    GThread      *thread;
    GMainContext *thread_context;
    GMainLoop    *thread_loop;
    GIOChannel   *thread_channel;
    GAsyncQueue  *thread_queue;

    /* The device */
    mccr_device_t *device;
};

/*****************************************************************************/

static const gchar *processor_item_str[] = {
    [MUI_PROCESSOR_ITEM_STATUS]                        = "status",
    [MUI_PROCESSOR_ITEM_STATUS_ERROR]                  = "status-error",
    [MUI_PROCESSOR_ITEM_MANUFACTURER]                  = "manufacturer",
    [MUI_PROCESSOR_ITEM_PRODUCT]                       = "product",
    [MUI_PROCESSOR_ITEM_SOFTWARE_ID]                   = "software-id",
    [MUI_PROCESSOR_ITEM_USB_SN]                        = "usb-s/n",
    [MUI_PROCESSOR_ITEM_DEVICE_SN]                     = "device-s/n",
    [MUI_PROCESSOR_ITEM_MAGNESAFE_VERSION]             = "magnesafe-version",
    [MUI_PROCESSOR_ITEM_SUPPORTED_CARDS]               = "supported-cards",
    [MUI_PROCESSOR_ITEM_POLLING_INTERVAL]              = "polling-interval",
    [MUI_PROCESSOR_ITEM_MAX_PACKET_SIZE]               = "max-packet-size",
    [MUI_PROCESSOR_ITEM_TRACK_1_STATUS]                = "track-1-status",
    [MUI_PROCESSOR_ITEM_TRACK_2_STATUS]                = "track-2-status",
    [MUI_PROCESSOR_ITEM_TRACK_3_STATUS]                = "track-3-status",
    [MUI_PROCESSOR_ITEM_ISO_TRACK_MASK]                = "iso-track-mask",
    [MUI_PROCESSOR_ITEM_AAMVA_TRACK_MASK]              = "aamva-track-mask",
    [MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER]         = "dukpt-ksn-and-counter",
    [MUI_PROCESSOR_ITEM_READER_STATE]                  = "reader-state",
    [MUI_PROCESSOR_ITEM_ANTECEDENT]                    = "antecedent",
    [MUI_PROCESSOR_ITEM_SECURITY_LEVEL]                = "security-level",
    [MUI_PROCESSOR_ITEM_ENCRYPTION_COUNTER]            = "encryption-counter",
    [MUI_PROCESSOR_ITEM_MAGTEK_UPDATE_TOKEN]           = "magtek-update-token",
    [MUI_PROCESSOR_ITEM_SWIPE_STARTED]                 = "swipe-started",
    [MUI_PROCESSOR_ITEM_CARD_ENCODE_TYPE]              = "card-encode-type",
    [MUI_PROCESSOR_ITEM_TRACK_1_DECODE_STATUS]         = "track-1-decode-status",
    [MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA_LENGTH] = "track-1-encrypted-data-length",
    [MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA]        = "track-1-encrypted-data",
    [MUI_PROCESSOR_ITEM_TRACK_2_DECODE_STATUS]         = "track-2-decode-status",
    [MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA_LENGTH] = "track-2-encrypted-data-length",
    [MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA]        = "track-2-encrypted-data",
    [MUI_PROCESSOR_ITEM_TRACK_3_DECODE_STATUS]         = "track-3-decode-status",
    [MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA_LENGTH] = "track-3-encrypted-data-length",
    [MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA]        = "track-3-encrypted-data",
    [MUI_PROCESSOR_ITEM_SWIPE_FINISHED]                = "swipe-finished",
};

G_STATIC_ASSERT (G_N_ELEMENTS (processor_item_str) == MUI_PROCESSOR_ITEM_LAST);

const gchar *
mui_processot_item_to_string (MuiProcessorItem item)
{
    return processor_item_str[item];
}

/*****************************************************************************/

static void
report_item (MuiProcessor     *self,
             MuiProcessorItem  item,
             const gchar      *value)
{
    g_debug ("[processor] report item %s%s%s",
             processor_item_str[item],
             value ? ": "  : "",
             value ? value : "");
    g_signal_emit (self, signals[REPORT_ITEM], 0, (guint) item, value);
}

/*****************************************************************************/

const gchar *
mui_processor_get_path (MuiProcessor *self)
{
    g_return_val_if_fail (MUI_IS_PROCESSOR (self), NULL);
    return self->priv->path;
}

/*****************************************************************************/

static void
report_wide_string (MuiProcessor     *self,
                    MuiProcessorItem  item,
                    const wchar_t    *wstr)
{
    gchar  *dstr;
    GError *error = NULL;

    g_assert (self->priv->device);
    dstr = g_convert ((const gchar *)wstr,
                      wcslen (wstr) * sizeof (wchar_t),
                      "UTF-8",
                      "WCHAR_T",
                      NULL,
                      NULL,
                      &error);
    if (!dstr) {
        g_warning ("[processor] couldn't convert wide char string to utf-8: %s", error->message);
        g_clear_error (&error);
    }
    report_item (self, item, (dstr && dstr[0]) ? dstr : "n/a");
    g_free (dstr);
}

static void
load_device_properties (MuiProcessor *self)
{
    mccr_status_t  st;
    gboolean       is_open;
    const wchar_t *wstr;
    char          *str;
    uint8_t        val;
    uint8_t       *array;
    size_t         array_size;

    g_assert (self->priv->device);

    report_item (self, MUI_PROCESSOR_ITEM_STATUS, "Loading device properties...");

    wstr = mccr_device_get_manufacturer (self->priv->device);
    report_wide_string (self, MUI_PROCESSOR_ITEM_MANUFACTURER, wstr);

    wstr = mccr_device_get_product (self->priv->device);
    report_wide_string (self, MUI_PROCESSOR_ITEM_PRODUCT, wstr);

    if ((st = mccr_device_open (self->priv->device)) != MCCR_STATUS_OK) {
        str = g_strdup_printf ("Couldn't open mccr device: %s", mccr_status_to_string (st));
        report_item (self, MUI_PROCESSOR_ITEM_STATUS_ERROR, str);
        g_free (str);
    }

    is_open = mccr_device_is_open (self->priv->device);

    str = NULL;
    if (is_open)
        st = mccr_device_read_software_id (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_SOFTWARE_ID, (str && str[0]) ? str : "n/a");
    g_free (str);

    str = NULL;
    if (is_open)
        st = mccr_device_read_usb_serial_number (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_USB_SN, (str && str[0]) ? str  : "n/a");
    g_free (str);

    str = NULL;
    if (is_open)
        st = mccr_device_read_device_serial_number (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_DEVICE_SN, (str && str[0]) ? str  : "n/a");
    g_free (str);

    str = NULL;
    if (is_open)
        st = mccr_device_read_magnesafe_version_number (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_MAGNESAFE_VERSION, (str && str[0]) ? str  : "n/a");
    g_free (str);

    {
        bool               aamva_supported;
        mccr_track_state_t track_1;
        mccr_track_state_t track_2;
        mccr_track_state_t track_3;

        if (is_open && ((st = mccr_device_read_track_id_enable (self->priv->device, &aamva_supported, &track_1, &track_2, &track_3)) == MCCR_STATUS_OK)) {
            report_item (self, MUI_PROCESSOR_ITEM_SUPPORTED_CARDS, aamva_supported ? "ISO and AAMVA" : "ISO only");
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_1_STATUS, mccr_track_state_to_string (track_1));
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_2_STATUS, mccr_track_state_to_string (track_2));
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_3_STATUS, mccr_track_state_to_string (track_3));
        } else {
            report_item (self, MUI_PROCESSOR_ITEM_SUPPORTED_CARDS, "n/a");
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_1_STATUS,  "n/a");
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_2_STATUS,  "n/a");
            report_item (self, MUI_PROCESSOR_ITEM_TRACK_3_STATUS,  "n/a");
        }
    }

    str = NULL;
    if (is_open)
        st = mccr_device_read_iso_track_mask (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_ISO_TRACK_MASK, (str && str[0]) ? str  : "n/a");
    g_free (str);

    str = NULL;
    if (is_open)
        st = mccr_device_read_aamva_track_mask (self->priv->device, &str);
    report_item (self, MUI_PROCESSOR_ITEM_AAMVA_TRACK_MASK, (str && str[0]) ? str  : "n/a");
    g_free (str);

    str = NULL;
    if (is_open && ((st = mccr_device_read_max_packet_size (self->priv->device, &val)) == MCCR_STATUS_OK))
        str = g_strdup_printf ("%u bytes", val);
    report_item (self, MUI_PROCESSOR_ITEM_MAX_PACKET_SIZE, (str && str[0]) ? str  : "n/a");
    g_free (str);

    str = NULL;
    if (is_open && ((st = mccr_device_get_dukpt_ksn_and_counter (self->priv->device, &array, &array_size)) == MCCR_STATUS_OK)) {
        str = strhex (array, array_size, " ");
        g_free (array);
    }
    report_item (self, MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER, (str && str[0]) ? str : "n/a");
    g_free (str);

    str = NULL;
    if (is_open && ((st = mccr_device_read_polling_interval (self->priv->device, &val)) == MCCR_STATUS_OK))
        str = g_strdup_printf ("%ums", val);
    report_item (self, MUI_PROCESSOR_ITEM_POLLING_INTERVAL, (str && str[0]) ? str  : "n/a");
    g_free (str);

    {
        mccr_reader_state_t            state;
        mccr_reader_state_antecedent_t antecedent;

        if (is_open && (st = mccr_device_get_reader_state (self->priv->device, &state, &antecedent)) == MCCR_STATUS_OK) {
            report_item (self, MUI_PROCESSOR_ITEM_READER_STATE, mccr_reader_state_to_string (state));
            report_item (self, MUI_PROCESSOR_ITEM_ANTECEDENT,   mccr_reader_state_antecedent_to_string (antecedent));
        } else {
            report_item (self, MUI_PROCESSOR_ITEM_READER_STATE, "n/a");
            report_item (self, MUI_PROCESSOR_ITEM_ANTECEDENT,   "n/a");
        }
    }

    str = NULL;
    if (is_open) {
        mccr_security_level_t level;

        if ((st = mccr_device_get_security_level (self->priv->device, &level)) == MCCR_STATUS_OK)
            str = g_strdup_printf ("%u", (guint) level);
    }
    report_item (self, MUI_PROCESSOR_ITEM_SECURITY_LEVEL, (str && str[0]) ? str : "n/a");
    g_free (str);

    str = NULL;
    if (is_open) {
        uint32_t val32;

        if ((st = mccr_device_get_encryption_counter (self->priv->device, NULL, &val32)) == MCCR_STATUS_OK) {
            if (val32 == MCCR_ENCRYPTION_COUNTER_DISABLED)
                str = g_strdup ("disabled");
            else if (val32 == MCCR_ENCRYPTION_COUNTER_EXPIRED)
                str = g_strdup ("expired");
            else if (val32 >= MCCR_ENCRYPTION_COUNTER_MIN && val32 <= MCCR_ENCRYPTION_COUNTER_MAX)
                str = g_strdup_printf ("%u", val32);
            else
                g_warning ("[processor] encryption counter: unexpected value: %u", (unsigned int) val32);
        }
    }
    report_item (self, MUI_PROCESSOR_ITEM_ENCRYPTION_COUNTER, (str && str[0]) ? str : "n/a");
    g_free (str);

    str = NULL;
    if (is_open && ((st = mccr_device_get_magtek_update_token (self->priv->device, &array, &array_size)) == MCCR_STATUS_OK)) {
        str = strhex_multiline (array, array_size, 10, NULL, ":");
        g_free (array);
    }
    report_item (self, MUI_PROCESSOR_ITEM_MAGTEK_UPDATE_TOKEN, (str && str[0]) ? str : "n/a");
    g_free (str);
}

/*****************************************************************************/
/* Helper for the command operation.
 *
 * It is refcounted in order to clarify that each user will hold a strong
 * reference on the context.
 */

typedef struct {
    volatile gint            ref_count;
    gchar                   *command;
    gchar                   *response;
    MuiProcessorCommandFlag  flags;
} CommandInfo;

static void
command_info_unref (CommandInfo *command_info)
{
    if (g_atomic_int_dec_and_test (&command_info->ref_count)) {
        g_free (command_info->response);
        g_free (command_info->command);
        g_slice_free (CommandInfo, command_info);
    }
}

static CommandInfo *
command_info_ref (CommandInfo *command_info)
{
    g_atomic_int_inc (&command_info->ref_count);
    return command_info;
}

static CommandInfo *
command_info_new (const gchar             *command,
                  MuiProcessorCommandFlag  flags)
{
    CommandInfo *command_info;

    command_info = g_slice_new0 (CommandInfo);
    command_info->ref_count = 1;
    command_info->command   = g_strdup (command);
    command_info->flags     = flags;

    return command_info;
}

/*****************************************************************************/
/* Operations scheduled on the thread */

typedef enum {
    OPERATION_TYPE_START,
    OPERATION_TYPE_RESET,
    OPERATION_TYPE_LOAD_PROPERTIES,
    OPERATION_TYPE_WAIT_SWIPE,
    OPERATION_TYPE_RUN_COMMAND,
    OPERATION_TYPE_STOP,
} OperationType;

typedef struct {
    OperationType  type;
    CommandInfo   *command_info;
} OperationContext;

static void
operation_context_free (OperationContext *operation_context)
{
    g_clear_pointer (&operation_context->command_info, command_info_unref);
    g_slice_free (OperationContext, operation_context);
}

static GTask *
operation_task_new (MuiProcessor        *self,
                    OperationType        type,
                    CommandInfo         *command_info,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
    GTask            *operation_task;
    OperationContext *operation_context;

    operation_task = g_task_new (self, cancellable, callback, user_data);
    operation_context = g_slice_new0 (OperationContext);
    operation_context->type = type;
    operation_context->command_info = command_info ? command_info_ref (command_info) : NULL;
    g_task_set_task_data (operation_task, operation_context, (GDestroyNotify) operation_context_free);
    return operation_task;
}

static void notify_operation_available (MuiProcessor *self);

/* Start */

static void
schedule_operation_start (MuiProcessor *self)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (self, OPERATION_TYPE_START, NULL, NULL, NULL, NULL));
    notify_operation_available (self);
}

/* Load properties */

static void
schedule_operation_load_properties (MuiProcessor *self)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (self, OPERATION_TYPE_LOAD_PROPERTIES, NULL, NULL, NULL, NULL));
    notify_operation_available (self);
}

/* Reset */

static void
schedule_operation_reset (MuiProcessor *self)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (self, OPERATION_TYPE_RESET, NULL, NULL, NULL, NULL));
    notify_operation_available (self);
}

/* Wait for swipe */

static gboolean
schedule_operation_wait_swipe_finish (MuiProcessor  *self,
                                      GAsyncResult  *res,
                                      GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
schedule_operation_wait_swipe (MuiProcessor        *self,
                               GCancellable        *cancellable,
                               GAsyncReadyCallback  callback,
                               gpointer             user_data)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (self,
                                            OPERATION_TYPE_WAIT_SWIPE,
                                            NULL,
                                            cancellable,
                                            callback,
                                            user_data));
    notify_operation_available (self);
}

/* Run Command */

static gboolean
schedule_operation_run_command_finish (MuiProcessor  *self,
                                       GAsyncResult  *res,
                                       GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
schedule_operation_run_command (MuiProcessor            *self,
                                CommandInfo             *command_info,
                                GCancellable            *cancellable,
                                GAsyncReadyCallback      callback,
                                gpointer                 user_data)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (self,
                                            OPERATION_TYPE_RUN_COMMAND,
                                            command_info,
                                            cancellable,
                                            callback,
                                            user_data));
    notify_operation_available (self);
}

/* Stop */

static void
schedule_operation_stop (MuiProcessor *self)
{
    g_async_queue_push (self->priv->thread_queue,
                        operation_task_new (NULL, OPERATION_TYPE_STOP, NULL, NULL, NULL, NULL));
    notify_operation_available (self);
}

/*****************************************************************************/

static void
process_track (MuiProcessor          *self,
               mccr_swipe_report_t   *report,
               mccr_status_t       (* get_decode_status)         (mccr_swipe_report_t *report, uint8_t        *status),
               mccr_status_t       (* get_encrypted_data_length) (mccr_swipe_report_t *report, uint8_t        *status),
               mccr_status_t       (* get_encrypted_data)        (mccr_swipe_report_t *report, const uint8_t **out_data),
               MuiProcessorItem       decode_status_item,
               MuiProcessorItem       encrypted_data_length_item,
               MuiProcessorItem       encrypted_data_item)
{
    mccr_status_t  st;
    uint8_t        status;
    const uint8_t *data;
    uint8_t        length;
    char          *decode_status         = NULL;
    char          *encrypted_data_length = NULL;
    char          *encrypted_data        = NULL;

    if ((st = get_decode_status (report, &status)) != MCCR_STATUS_OK)
        g_warning ("Cannot get track decode status: %s", mccr_status_to_string (st));
    else if (status == MCCR_SWIPE_TRACK_DECODE_STATUS_SUCCESS)
        decode_status = g_strdup ("success");
    else if (status & MCCR_SWIPE_TRACK_DECODE_STATUS_ERROR)
        decode_status = g_strdup ("error");
    else
        decode_status = g_strdup_printf ("unknown status (0x%02x)", status);

    if ((st = get_encrypted_data_length (report, &length)) != MCCR_STATUS_OK)
        g_warning ("Cannot get track encrypted data length: %s", mccr_status_to_string (st));
    else
        encrypted_data_length = g_strdup_printf ("%u bytes", length);

    if (length > 0) {
        if ((st = get_encrypted_data (report, &data)) != MCCR_STATUS_OK)
            g_warning ("Cannot get track data: %s", mccr_status_to_string (st));
        else
            encrypted_data = strhex (data, length, " ");
    }

    report_item (self, decode_status_item,         decode_status         ? decode_status         : "n/a");
    report_item (self, encrypted_data_length_item, encrypted_data_length ? encrypted_data_length : "n/a");
    report_item (self, encrypted_data_item,        encrypted_data        ? encrypted_data        : "n/a");

    g_free (decode_status);
    g_free (encrypted_data_length);
    g_free (encrypted_data);
}

static gboolean
run_wait_swipe (MuiProcessor  *self,
                GError       **error)
{
    mccr_status_t            st;
    mccr_swipe_report_t     *report;
    mccr_card_encode_type_t  card_encode_type;
    gchar                   *str;

    report_item (self, MUI_PROCESSOR_ITEM_STATUS, "Waiting for swipe...");

    st = mccr_device_wait_swipe_report (self->priv->device, DEFAULT_WAIT_SWIPE_TIMEOUT_MS, &report);
    if (st != MCCR_STATUS_OK) {
        if (st == MCCR_STATUS_TIMED_OUT)
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT, "Timeout");
        else
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Cannot get swipe report: %s", mccr_status_to_string (st));
        return FALSE;
    }

    /* We're going to report a new swipe */
    report_item (self, MUI_PROCESSOR_ITEM_SWIPE_STARTED, NULL);

    str = NULL;
    if ((st = mccr_swipe_report_get_card_encode_type (report, &card_encode_type)) != MCCR_STATUS_OK)
        g_warning ("Cannot get card encode type: %s", mccr_status_to_string (st));
    else
        str = g_strdup (mccr_card_encode_type_to_string (card_encode_type));
    report_item (self, MUI_PROCESSOR_ITEM_CARD_ENCODE_TYPE, str);
    g_free (str);

    process_track (self,
                   report,
                   mccr_swipe_report_get_track_1_decode_status,
                   mccr_swipe_report_get_track_1_encrypted_data_length,
                   mccr_swipe_report_get_track_1_encrypted_data,
                   MUI_PROCESSOR_ITEM_TRACK_1_DECODE_STATUS,
                   MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA_LENGTH,
                   MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA);

    process_track (self,
                   report,
                   mccr_swipe_report_get_track_2_decode_status,
                   mccr_swipe_report_get_track_2_encrypted_data_length,
                   mccr_swipe_report_get_track_2_encrypted_data,
                   MUI_PROCESSOR_ITEM_TRACK_2_DECODE_STATUS,
                   MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA_LENGTH,
                   MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA);

    process_track (self,
                   report,
                   mccr_swipe_report_get_track_3_decode_status,
                   mccr_swipe_report_get_track_3_encrypted_data_length,
                   mccr_swipe_report_get_track_3_encrypted_data,
                   MUI_PROCESSOR_ITEM_TRACK_3_DECODE_STATUS,
                   MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA_LENGTH,
                   MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA);

    /* We finished reporting a swipe */
    report_item (self, MUI_PROCESSOR_ITEM_SWIPE_FINISHED, NULL);

    mccr_swipe_report_free (report);
    return TRUE;
}

static gboolean
run_command (MuiProcessor  *self,
             CommandInfo   *command_info,
             GError       **error)
{
    gssize         bin_size;
    guint8         command_id;
    guint8         data_size;
    guint8        *buffer = NULL;
    const guint8  *data;
    gsize          buffer_size;
    mccr_status_t  st;
    GError        *inner_error = NULL;
    gchar         *str;

    g_assert (command_info);

    buffer_size = strlen (command_info->command);

    /* Build bin; which 'compresses' the input ascii by 2, so the buffer_size
     * should be always enough */
    buffer = (guint8 *) g_malloc0 (buffer_size);
    if ((bin_size = strbin (command_info->command, buffer, buffer_size)) < 0) {
        inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                   "Couldn't convert hex string '%s' to binary",
                                   command_info->command);
        goto out;
    }

    /* The command ID is the first byte always */
    if (bin_size < 1) {
        inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                   "Command id not given, too short: %" G_GSSIZE_FORMAT " < 2",
                                   bin_size);
        goto out;
    }
    command_id = buffer[0];

    data_size = buffer[1];
    data      = data_size > 0 ? &buffer[2] : NULL;

    /* If data size given, it must be equal to the full binary size minus
     * 2 bytes (1 byte command id, 1 byte data size) */
    if (data_size != bin_size - 2) {
        inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                                   "Command data size invalid: %u != (%" G_GSSIZE_FORMAT " - 2)",
                                   data_size, bin_size);
        goto out;
    }

    str = strhex (data, data_size, ":");
    g_debug ("[processor] requested to run command 0x%02x: %s (%u bytes)", command_id, str, data_size);
    g_free (str);

    /* If a response is expected, we will fail if we didn't get any */
    if (command_info->flags & MUI_PROCESSOR_COMMAND_FLAG_RESPONSE_EXPECTED) {
        guint8 *response = NULL;
        gsize   response_size = 0;

        if ((st = mccr_device_run_generic (self->priv->device,
                                                command_id,
                                                data,
                                                data_size,
                                                &response,
                                                &response_size)) != MCCR_STATUS_OK) {
            inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                       "Cannot run generic command '0x%02x': %s",
                                       command_id, mccr_status_to_string (st));
            goto out;
        }

        if (!response || !response_size) {
            inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                       "No response received for command '0x%02x'", command_id);
            goto out;
        }

        /* Store response in command info */
        command_info->response = strhex (response, response_size, NULL);
        g_free (response);
    }
    /* No response expected, so just run */
    else {
        if ((st = mccr_device_run_generic (self->priv->device,
                                           command_id,
                                           data,
                                           data_size,
                                           NULL,
                                           NULL)) != MCCR_STATUS_OK) {
            inner_error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                                       "Cannot run generic command '0x%02x': %s",
                                       command_id, mccr_status_to_string (st));
            goto out;
        }
    }

out:

    g_free (buffer);

    if (inner_error) {
        g_propagate_error (error, inner_error);
        return FALSE;
    }

    return TRUE;
}

static gboolean
operation_available_cb (MuiProcessor *self)
{
    GTask            *operation_task;
    OperationContext *operation_context;

    operation_task = g_async_queue_try_pop (self->priv->thread_queue);
    if (!operation_task)
        return G_SOURCE_REMOVE;

    operation_context = g_task_get_task_data (operation_task);
    g_assert (operation_context);

    /* Early cancellation? */
    if (g_task_return_error_if_cancelled (operation_task)) {
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Start */
    if (operation_context->type == OPERATION_TYPE_START) {
        g_debug ("[processor] operation task: start");
        g_assert (!self->priv->device);
        self->priv->device = mccr_device_new (self->priv->path);
        g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Load properties */
    if (operation_context->type == OPERATION_TYPE_LOAD_PROPERTIES) {
        g_debug ("[processor] operation task: load properties");
        load_device_properties (self);
        g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Reset */
    if (operation_context->type == OPERATION_TYPE_RESET) {
        g_debug ("[processor] operation task: reset");
        mccr_device_reset (self->priv->device);
        g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Wait for swipe */
    if (operation_context->type == OPERATION_TYPE_WAIT_SWIPE) {
        GError *error = NULL;

        g_debug ("[processor] operation task: wait swipe");
        if (!run_wait_swipe (self, &error))
            g_task_return_error (operation_task, error);
        else
            g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Run command */
    if (operation_context->type == OPERATION_TYPE_RUN_COMMAND) {
        GError *error = NULL;

        g_debug ("[processor] operation task: run command");
        if (!run_command (self, operation_context->command_info, &error))
            g_task_return_error (operation_task, error);
        else
            g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Process stop */
    if (operation_context->type == OPERATION_TYPE_STOP) {
        g_debug ("[processor] operation task: stop");
        g_clear_pointer (&self->priv->device, mccr_device_unref);
        g_main_loop_quit (self->priv->thread_loop);
        g_task_return_boolean (operation_task, TRUE);
        g_object_unref (operation_task);
        return G_SOURCE_REMOVE;
    }

    /* Unknown task type? */
    g_assert_not_reached ();
    return FALSE;
}

static void
notify_operation_available (MuiProcessor *self)
{
    g_main_context_invoke (self->priv->thread_context, (GSourceFunc) operation_available_cb, self);
}

/*****************************************************************************/
/* Thread timing control */

#define THREAD_TIMER_TIMEOUT_SECS 30

typedef struct {
    MuiProcessor *self;
    GTimer       *timer;
} ThreadTimerContext;

static void
thread_timer_context_free (ThreadTimerContext *ctx)
{
    g_timer_destroy (ctx->timer);
    g_slice_free (ThreadTimerContext, ctx);
}

static ThreadTimerContext *
thread_timer_context_new (MuiProcessor *self)
{
    ThreadTimerContext *ctx;

    ctx = g_slice_new0 (ThreadTimerContext);
    ctx->self  = self;
    ctx->timer = g_timer_new ();
    return ctx;
}

static gboolean
thread_timer_context_step (ThreadTimerContext *ctx)
{
    g_debug ("[processor] support running for %.0lf seconds", g_timer_elapsed (ctx->timer, NULL));
    return G_SOURCE_CONTINUE;
}

static void
thread_timer_setup (MuiProcessor *self)
{
    GSource *source;

    source = g_timeout_source_new_seconds (THREAD_TIMER_TIMEOUT_SECS);
    g_source_set_callback (source,
                           (GSourceFunc) thread_timer_context_step,
                           thread_timer_context_new (self),
                           (GDestroyNotify) thread_timer_context_free);
    g_source_attach (source, self->priv->thread_context);
    g_source_unref (source);
}

/*****************************************************************************/
/* Setup/teardown inner thread */

static void
inner_thread_teardown (MuiProcessor *self)
{
    if (!self->priv->thread)
        return;

    schedule_operation_stop (self);

    g_clear_pointer (&self->priv->thread,         g_thread_join);
    g_clear_pointer (&self->priv->thread_context, g_main_context_unref);
    g_clear_pointer (&self->priv->thread_queue,   g_async_queue_unref);
}

static gpointer
inner_thread_func (MuiProcessor *self)
{
    g_assert (self->priv->thread_context);
    g_assert (!self->priv->thread_loop);

    g_debug ("[processor] started");

    g_main_context_push_thread_default (self->priv->thread_context);

    /* Create loop */
    g_assert (!self->priv->thread_loop);
    self->priv->thread_loop = g_main_loop_new (self->priv->thread_context, FALSE);

    /* Setup thread timer control */
    thread_timer_setup (self);

    /* Launch loop */
    g_main_loop_run (self->priv->thread_loop);
    g_main_loop_unref (self->priv->thread_loop);
    self->priv->thread_loop = NULL;

    g_main_context_pop_thread_default (self->priv->thread_context);

    g_debug ("[processor] finished");

    return NULL;
}

static void
inner_thread_setup (MuiProcessor *self)
{
    g_assert (!self->priv->thread);
    g_assert (!self->priv->thread_context);
    g_assert (!self->priv->thread_queue);

    self->priv->thread_context = g_main_context_new ();
    self->priv->thread_queue   = g_async_queue_new_full ((GDestroyNotify) g_object_unref);
    self->priv->thread         = g_thread_new (NULL, (GThreadFunc) inner_thread_func, self);
}

/*****************************************************************************/
/* Start */

void
mui_processor_start (MuiProcessor *self)
{
    inner_thread_setup       (self);
    schedule_operation_start (self);
}

/*****************************************************************************/
/* Load properties */

void
mui_processor_load_properties (MuiProcessor *self)
{
    schedule_operation_load_properties (self);
}

/*****************************************************************************/
/* Reset */

void
mui_processor_reset (MuiProcessor *self)
{
    schedule_operation_reset (self);
}

/*****************************************************************************/
/* Wait swipe */

gboolean
mui_processor_wait_swipe_finish (MuiProcessor  *self,
                                 GAsyncResult  *res,
                                 GError       **error)
{
    return g_task_propagate_boolean (G_TASK (res), error);
}

static void
wait_swipe_ready (MuiProcessor *self,
                  GAsyncResult *res,
                  GTask        *task)
{
    GError *error = NULL;

    if (!schedule_operation_wait_swipe_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

void
mui_processor_wait_swipe (MuiProcessor        *self,
                          GCancellable        *cancellable,
                          GAsyncReadyCallback  callback,
                          gpointer             user_data)
{
    GTask *task;

    task = g_task_new (self, cancellable, callback, user_data);
    schedule_operation_wait_swipe (self, cancellable, (GAsyncReadyCallback) wait_swipe_ready, task);
}

/*****************************************************************************/
/* Run command */

gboolean
mui_processor_run_command_finish (MuiProcessor  *self,
                                  GAsyncResult  *res,
                                  gchar        **out_response,
                                  GError       **error)
{
    CommandInfo *command_info;

    command_info = (CommandInfo *) g_task_get_task_data (G_TASK (res));

    if (!g_task_propagate_boolean (G_TASK (res), error))
        return FALSE;

    if (command_info->flags & MUI_PROCESSOR_COMMAND_FLAG_RESPONSE_EXPECTED) {
        g_assert (command_info->response);
        /* Steal response from command context */
        if (out_response) {
            *out_response = command_info->response;
            command_info->response = NULL;
        }
    }

    return TRUE;
}

static void
run_command_ready (MuiProcessor *self,
                   GAsyncResult *res,
                   GTask        *task)
{

    GError *error = NULL;

    if (!schedule_operation_run_command_finish (self, res, &error))
        g_task_return_error (task, error);
    else
        g_task_return_boolean (task, TRUE);
    g_object_unref (task);
}

void
mui_processor_run_command (MuiProcessor            *self,
                           const gchar             *command,
                           MuiProcessorCommandFlag  flags,
                           GCancellable            *cancellable,
                           GAsyncReadyCallback      callback,
                           gpointer                 user_data)
{
    GTask       *task;
    CommandInfo *command_info;

    task = g_task_new (self, cancellable, callback, user_data);

    command_info = command_info_new (command, flags);
    g_task_set_task_data (task, command_info, (GDestroyNotify) command_info_unref);

    schedule_operation_run_command (self,
                                    command_info,
                                    cancellable,
                                    (GAsyncReadyCallback) run_command_ready,
                                    task);
}

/*****************************************************************************/

MuiProcessor *
mui_processor_new (const gchar *path)
{
    return MUI_PROCESSOR (g_object_new (MUI_TYPE_PROCESSOR,
                                        "path", path,
                                        NULL));
}

static void
mui_processor_init (MuiProcessor *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_PROCESSOR, MuiProcessorPrivate);
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    MuiProcessor *self = MUI_PROCESSOR (object);

    switch (prop_id) {
    case PROP_PATH:
        g_assert (!self->priv->path);
        self->priv->path = g_value_dup_string (value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    MuiProcessor *self = MUI_PROCESSOR (object);

    switch (prop_id) {
    case PROP_PATH:
        g_value_set_string (value, self->priv->path);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
finalize (GObject *object)
{
    MuiProcessor *self = MUI_PROCESSOR (object);

    inner_thread_teardown (self);

    g_free (self->priv->path);

    G_OBJECT_CLASS (mui_processor_parent_class)->finalize (object);
}

static void
mui_processor_class_init (MuiProcessorClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MuiProcessorPrivate));

    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->finalize     = finalize;

    properties[PROP_PATH] =
        g_param_spec_string ("path",
                             "Path",
                             "Path of the device",
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[REPORT_ITEM] =
        g_signal_new ("report-item",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MuiProcessorClass, report_item),
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_STRING);
}
