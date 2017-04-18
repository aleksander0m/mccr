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

#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include <dukpt.h>

#include <common.h>

#include "mui-page-remote-services.h"
#include "mui-processor.h"
#include "mui-remote-service.h"
#include "mui-remote-service-utils.h"

/* Test key info */
static const gchar       *test_key_ksi = "9010010";
static const dukpt_key_t  test_key_bdk = {
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef,
    0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54, 0x32, 0x10
};

G_DEFINE_TYPE (MuiPageRemoteServices, mui_page_remote_services, MUI_TYPE_PAGE)

enum {
    PROP_0,
    PROP_OPERATION_ONGOING,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

#define MUT_SIZE 36

typedef enum {
    SETTINGS_FIELD_ID_RS_URL,
    SETTINGS_FIELD_ID_RS_CUSTOMER_CODE,
    SETTINGS_FIELD_ID_RS_USERNAME,
    SETTINGS_FIELD_ID_RS_PASSWORD,
    SETTINGS_FIELD_ID_RS_BILLING_LABEL,
    SETTINGS_FIELD_ID_RS_CUSTOMER_TRID,
    SETTINGS_FIELD_ID_LAST
} SettingsFieldId;

struct _MuiPageRemoteServicesPrivate {
    /* Device info */
    dukpt_ksn_t ksn;
    gboolean    ksn_set;
    guint8      mut[MUT_SIZE];
    gboolean    mut_set;

    /* Current key id, if known */
    gchar *key_id;

    /* Remote service operations */
    MuiRemoteService *remote_service;
    GCancellable     *remote_service_cancellable;

    /* Widgets */
    GtkWidget    *remote_service_results_listbox;
    GtkWidget    *remote_service_results_scrolled_window;
    GtkWidget    *remote_service_ongoing_box;
    GtkWidget    *remote_service_ongoing_status_label;
    GtkWidget    *remote_service_ongoing_spinner;
    GtkSizeGroup *remote_service_results_size_group_center;
    GtkSizeGroup *remote_service_results_size_group_edges;
    GtkWidget    *remote_service_footer;
    GtkWidget    *remote_service_footer_label;

    /* Settings */
    gchar     *settings_path;
    GKeyFile  *settings;
    GtkWidget *settings_entries[SETTINGS_FIELD_ID_LAST];
    guint      settings_store_id;

    /* Commands */
    guint n_commands_executed;
};

/******************************************************************************/

static gboolean
ksi_matches_ksn (const dukpt_ksn_t *ksn,
                 const gchar       *ksi)
{
    gboolean matches = FALSE;

    if (ksn && *ksn && ksi) {
        gchar *hexksn;

        /* We convert the whole KSN to hex, but we only want the topmost 3,5 bytes */
        hexksn = strhex ((const uint8_t *) *ksn, sizeof (dukpt_ksn_t), NULL);
        matches = (g_ascii_strncasecmp (hexksn, (const gchar *) ksi, strlen ((const gchar *) ksi)) == 0);
        g_free (hexksn);
    }

    return matches;
}

/******************************************************************************/

static void
remote_service_reset_button_clicked (MuiPageRemoteServices *self)
{
    g_debug ("Reset requested");
    mui_processor_reset (mui_page_peek_processor (MUI_PAGE (self)));
}

static void
common_update_footer (MuiPageRemoteServices *self)
{
    gchar *str;

    if (self->priv->n_commands_executed == 0) {
        gtk_widget_hide (self->priv->remote_service_footer);
        return;
    }

    gtk_widget_show (self->priv->remote_service_footer);

    if (self->priv->n_commands_executed == 1) {
        gtk_label_set_text (GTK_LABEL (self->priv->remote_service_footer_label),
                            "There is 1 command applied, a RESET is required to start using the new settings");
        return;
    }

    str = g_strdup_printf ("There are %u commands applied, a RESET is required to start using the new settings", self->priv->n_commands_executed);
    gtk_label_set_text (GTK_LABEL (self->priv->remote_service_footer_label), str);
    g_free (str);
}

/******************************************************************************/
/* Common operation utilities */

static void common_results_listbox_clear (MuiPageRemoteServices *self);

static void
start_main_operation (MuiPageRemoteServices *self,
                      const gchar           *status_msg)
{
    /* Clear any pending user error */
    mui_page_signal_clear_user_error (MUI_PAGE (self));

    /* Clear results list */
    common_results_listbox_clear (self);

    /* Setup operation cancellable */
    g_assert (!self->priv->remote_service_cancellable);
    self->priv->remote_service_cancellable = g_cancellable_new ();
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATION_ONGOING]);

    /* Setup ongoing operation information */
    gtk_label_set_text (GTK_LABEL (self->priv->remote_service_ongoing_status_label), status_msg);
    gtk_spinner_start (GTK_SPINNER (self->priv->remote_service_ongoing_spinner));
    gtk_widget_hide (self->priv->remote_service_results_scrolled_window);
    gtk_widget_show (self->priv->remote_service_ongoing_box);

    mui_remote_service_set_common_request_fields (self->priv->remote_service,
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_URL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_CODE])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_USERNAME])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_PASSWORD])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_BILLING_LABEL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_TRID])));
}

static void
stop_main_operation (MuiPageRemoteServices *self)
{
    /* Clear cancellable */
    g_clear_object (&self->priv->remote_service_cancellable);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATION_ONGOING]);

    /* Clear ongoing information */
    gtk_label_set_text (GTK_LABEL (self->priv->remote_service_ongoing_status_label), "");
    gtk_spinner_stop (GTK_SPINNER (self->priv->remote_service_ongoing_spinner));
    gtk_widget_hide (self->priv->remote_service_ongoing_box);
}

/******************************************************************************/
/* Row results */

typedef enum {
    RESULT_TYPE_KEY,
    RESULT_TYPE_COMMAND,
} ResultType;

typedef struct {
    /* Common */
    ResultType  type;
    gchar      *id;
    gchar      *name;
    gchar      *description;
    GtkWidget  *check_image;  /* soft */
    GtkWidget  *spinner;      /* soft */
    GtkWidget  *status_label; /* soft */

    /* Key list specific */
    gchar *ksi;

    /* Command list specific */
    gchar *execution_type;
    gchar *command;
} RowContext;

static void
row_context_free (RowContext *ctx)
{
    g_free (ctx->command);
    g_free (ctx->execution_type);
    g_free (ctx->ksi);
    g_free (ctx->description);
    g_free (ctx->name);
    g_free (ctx->id);
    g_slice_free (RowContext, ctx);
}

static void
common_results_listbox_clear (MuiPageRemoteServices *self)
{
    GList *children, *l;

    g_clear_object (&self->priv->remote_service_results_size_group_edges);
    g_clear_object (&self->priv->remote_service_results_size_group_center);

    children = gtk_container_get_children (GTK_CONTAINER (self->priv->remote_service_results_listbox));
    for (l = children; l; l = g_list_next (l))
        gtk_container_remove (GTK_CONTAINER (self->priv->remote_service_results_listbox), GTK_WIDGET (l->data));
    g_list_free (children);
}

static void
common_results_listbox_init (MuiPageRemoteServices *self)
{
    g_assert (!self->priv->remote_service_results_size_group_center);
    self->priv->remote_service_results_size_group_center = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
    g_assert (!self->priv->remote_service_results_size_group_edges);
    self->priv->remote_service_results_size_group_edges = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
}

static void
common_results_listbox_set_sensitivity (MuiPageRemoteServices *self,
                                        GtkWidget             *single_row)
{
    GList *children, *l;

    children = gtk_container_get_children (GTK_CONTAINER (self->priv->remote_service_results_listbox));
    for (l = children; l; l = g_list_next (l)) {
        GtkWidget  *row;

        row = (GtkWidget *)(l->data);
        gtk_widget_set_sensitive (row, (!single_row || (single_row == row)));
    }
    g_list_free (children);
}

static void
common_results_listbox_select_current (MuiPageRemoteServices *self,
                                       const dukpt_ksn_t     *ksn)
{
    GList *children, *l;

    children = gtk_container_get_children (GTK_CONTAINER (self->priv->remote_service_results_listbox));
    for (l = children; l; l = g_list_next (l)) {
        GtkWidget  *row;
        RowContext *ctx;

        row = (GtkWidget *)(l->data);

        ctx = g_object_get_data (G_OBJECT (row), "RowContext");
        g_assert (ctx);

        if (ksi_matches_ksn (ksn, ctx->ksi))
            gtk_widget_show (ctx->check_image);
        else
            gtk_widget_hide (ctx->check_image);
    }
    g_list_free (children);
}

static GtkWindow *
find_parent_window (MuiPageRemoteServices *self)
{
    GtkWidget *widget;

    widget = GTK_WIDGET (self);
    while (widget) {
        widget = gtk_widget_get_parent (widget);
        if (GTK_IS_WINDOW (widget))
            return GTK_WINDOW (widget);
    }
    return NULL;
}

static gint
result_row_confirmation_dialog_run (MuiPageRemoteServices *self,
                                    const gchar           *title,
                                    const gchar           *operation_description,
                                    const gchar           *operation,
                                    const gchar           *warning,
                                    const gchar           *message)
{
    GtkWidget *dialog;
    GtkWidget *area;
    GtkWidget *box;
    GtkWidget *label;
    gint       result;
    gchar     *str;

    dialog = gtk_dialog_new_with_buttons (title,
                                          find_parent_window (self),
                                          GTK_DIALOG_MODAL,
                                          "Yes", GTK_RESPONSE_YES,
                                          "No",  GTK_RESPONSE_NO,
                                          NULL);

    area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

    box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
    g_object_set (box,
                  "margin-top",    18,
                  "margin-bottom", 18,
#if GTK_CHECK_VERSION (3,12,0)
                  "margin-start",  18,
                  "margin-end",    18,
#else
                  "margin-left",   18,
                  "margin-right",  18,
#endif
                  NULL);
    gtk_container_add (GTK_CONTAINER (area), box);

    label = gtk_label_new (operation_description);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

    label = gtk_label_new ("");
    str = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>", operation);
    gtk_label_set_markup (GTK_LABEL (label), str);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
    g_free (str);

    if (warning) {
        label = gtk_label_new ("");
        str = g_markup_printf_escaped ("<span color=\"red\">%s</span>", warning);
        gtk_label_set_markup (GTK_LABEL (label), str);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
        g_free (str);
    }

    if (message) {
        label = gtk_label_new (message);
        gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
    }

    label = gtk_label_new ("Are you sure you want to continue?");
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

    gtk_widget_show_all (box);

    result = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    return result;
}

static void result_row_activated_key     (MuiPageRemoteServices *self,
                                          GtkListBoxRow         *row);
static void result_row_activated_command (MuiPageRemoteServices *self,
                                          GtkListBoxRow         *row);

static void
common_result_row_activated (MuiPageRemoteServices *self,
                             GtkListBoxRow         *row)
{
    RowContext *ctx;

    /* Do nothing if there is already an ongoing operation */
    if (self->priv->remote_service_cancellable)
        return;

    ctx = g_object_get_data (G_OBJECT (row), "RowContext");
    g_return_if_fail (ctx);

    switch (ctx->type) {
        case RESULT_TYPE_KEY:
            result_row_activated_key (self, row);
            break;
        case RESULT_TYPE_COMMAND:
            result_row_activated_command (self, row);
            break;
        default:
            g_assert_not_reached ();
            break;
    }
}

static void
common_results_listbox_append_row (MuiPageRemoteServices *self,
                                   ResultType             type,
                                   const gchar           *id,
                                   const gchar           *name,
                                   const gchar           *description,
                                   const gchar           *ksi,
                                   const gchar           *execution_type,
                                   const gchar           *command)
{
    GtkWidget  *row;
    GtkWidget  *box;
    GtkWidget  *inner_box;
    GtkWidget  *label;
    RowContext *ctx;
    GIcon      *icon;

    row = gtk_list_box_row_new ();
    gtk_widget_show (row);

    ctx = g_slice_new0 (RowContext);
    ctx->type            = type;
    ctx->id              = g_strdup (id);
    ctx->name            = g_strdup (name);
    ctx->description     = g_strdup (description);
    ctx->ksi             = g_strdup (ksi);
    ctx->execution_type  = g_strdup (execution_type);
    ctx->command         = g_strdup (command);

    g_object_set_data_full (G_OBJECT (row),
                            "RowContext",
                            ctx,
                            (GDestroyNotify) row_context_free);

    box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 18);
    gtk_widget_show (box);
    gtk_widget_set_halign (box, GTK_ALIGN_CENTER);
    gtk_container_add (GTK_CONTAINER (row), box);

    inner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_show (inner_box);
    gtk_widget_set_halign (inner_box, GTK_ALIGN_END);
    gtk_widget_set_valign (inner_box, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (box), inner_box, FALSE, TRUE, 0);
    gtk_size_group_add_widget (self->priv->remote_service_results_size_group_edges, inner_box);

    icon = g_themed_icon_new_with_default_fallbacks ("object-select-symbolic");
    ctx->check_image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign (ctx->check_image, GTK_ALIGN_END);
    gtk_widget_set_valign (ctx->check_image, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (inner_box), ctx->check_image, TRUE, TRUE, 0);

    ctx->spinner = gtk_spinner_new ();
    gtk_widget_set_halign (ctx->spinner, GTK_ALIGN_END);
    gtk_widget_set_valign (ctx->spinner, GTK_ALIGN_CENTER);
    gtk_box_pack_start (GTK_BOX (inner_box), ctx->spinner, TRUE, TRUE, 0);

    switch (type) {
        case RESULT_TYPE_KEY: {
            gchar *str;

            label = gtk_label_new ("");
            str = g_markup_printf_escaped ("<span weight=\"bold\">%s</span>\n"
                                           "\t<span style=\"italic\">%s</span>\n"
                                           "\tksi: %s",
                                           name,
                                           description,
                                           ksi);
            gtk_label_set_markup (GTK_LABEL (label), str);
            g_free (str);

            /* Align left all items returned */
#if GTK_CHECK_VERSION (3,16,0)
            gtk_label_set_xalign (GTK_LABEL (label), 0.0);
            gtk_label_set_yalign (GTK_LABEL (label), 0.5);
#else
            gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
#endif

            break;
        }
        case RESULT_TYPE_COMMAND:
            label = gtk_label_new (description);

            /* Center all items returned */
#if GTK_CHECK_VERSION (3,16,0)
            gtk_label_set_xalign (GTK_LABEL (label), 0.5);
            gtk_label_set_yalign (GTK_LABEL (label), 0.5);
#else
            gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
#endif
            break;
        default:
            g_assert_not_reached ();
            break;
    }
    gtk_widget_show (label);
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_size_group_add_widget (self->priv->remote_service_results_size_group_center, label);
    gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);

    inner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_show (inner_box);
    gtk_box_pack_start (GTK_BOX (box), inner_box, FALSE, TRUE, 0);
    gtk_size_group_add_widget (self->priv->remote_service_results_size_group_edges, inner_box);

    ctx->status_label = gtk_label_new ("Status...");
    gtk_label_set_line_wrap_mode (GTK_LABEL (ctx->status_label), PANGO_WRAP_WORD);
    gtk_widget_set_halign (ctx->status_label, GTK_ALIGN_START);
    gtk_widget_set_valign (ctx->status_label, GTK_ALIGN_CENTER);
    gtk_widget_hide (ctx->status_label);
    gtk_box_pack_start (GTK_BOX (inner_box), ctx->status_label, FALSE, TRUE, 0);

    gtk_list_box_insert (GTK_LIST_BOX (self->priv->remote_service_results_listbox), row, -1);
}

/******************************************************************************/
/* Key update */

#define KEY_UPDATE_ERROR_PREFIX "Key update operation failed: "

typedef struct {
    MuiPageRemoteServices *self;
    gchar                 *id;
    gchar                 *ksn;
    gchar                 *mut;
    gchar                 *command;
    gchar                 *kcv;
    GtkWidget             *check_image;  /* soft */
    GtkWidget             *spinner;      /* soft */
    GtkWidget             *status_label; /* soft */
} KeyUpdateContext;

static void
key_update_context_free (KeyUpdateContext *ctx)
{
    g_free (ctx->kcv);
    g_free (ctx->command);
    g_free (ctx->mut);
    g_free (ctx->ksn);
    g_free (ctx->id);
    g_object_unref (ctx->self);
    g_slice_free (KeyUpdateContext, ctx);
}

static void
key_update_finished (KeyUpdateContext *ctx,
                     gboolean          success)
{
    g_debug ("Key update operation finished successfully");

    gtk_widget_hide (ctx->spinner);
    gtk_widget_hide (ctx->status_label);
    if (success)
        gtk_widget_show (ctx->check_image);

    g_clear_object (&ctx->self->priv->remote_service_cancellable);
    g_object_notify_by_pspec (G_OBJECT (ctx->self), properties[PROP_OPERATION_ONGOING]);

    if (success) {
        /* Store new key id */
        g_free (ctx->self->priv->key_id);
        ctx->self->priv->key_id = ctx->id;
        ctx->id = NULL;
        g_debug ("New key id: %s", ctx->self->priv->key_id);
    } else {
        /* Operation failed, reload with latest KSN set */
        common_results_listbox_select_current (ctx->self, ctx->self->priv->ksn_set ? (const dukpt_ksn_t *) &ctx->self->priv->ksn : NULL);
    }

    common_results_listbox_set_sensitivity (ctx->self, NULL);

    key_update_context_free (ctx);
}

static void
run_key_update_command_ready (MuiProcessor     *processor,
                              GAsyncResult     *res,
                              KeyUpdateContext *ctx)
{
    GError *error = NULL;
    gchar  *response = NULL;

    if (!mui_processor_run_command_finish (processor, res, &response, &error)) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "%s", error->message);
        g_error_free (error);
        key_update_finished (ctx, FALSE);
        return;
    }

    if (!g_str_has_suffix (response, ctx->kcv)) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "Key validation check failed");
        key_update_finished (ctx, FALSE);
    } else
        key_update_finished (ctx, TRUE);

    g_free (response);
}

static void
get_key_load_command_ready (MuiRemoteService *remote_service,
                            GAsyncResult     *res,
                            KeyUpdateContext *ctx)
{
    GError         *error = NULL;
    GArray         *commands;
    MuiCommandInfo *to_scra;
    MuiCommandInfo *kcv;

    if (!(commands = mui_remote_service_get_key_load_command_finish (remote_service, res, &error))) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "%s", error->message);
        key_update_finished (ctx, FALSE);
        goto out;
    }

    /* We need 2 fields reported */
    if (commands->len != 2) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "Invalid number of commands received: %u", commands->len);
        key_update_finished (ctx, FALSE);
        goto out;
    }

    to_scra = &g_array_index (commands, MuiCommandInfo, 0);
    kcv     = &g_array_index (commands, MuiCommandInfo, 1);

    if (g_strcmp0 ((const gchar *) to_scra->name, "ChangeKey") != 0) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "Invalid command received (expecting ChangeKey): %s", (const gchar *) to_scra->name);
        key_update_finished (ctx, FALSE);
        goto out;
    }
    ctx->command = g_strdup ((const gchar *) to_scra->value);

    if (g_strcmp0 ((const gchar *) kcv->name, "KCV") != 0) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), KEY_UPDATE_ERROR_PREFIX "Invalid command received (expecting KCV): %s", (const gchar *) kcv->name);
        key_update_finished (ctx, FALSE);
        goto out;
    }
    ctx->kcv = g_strdup ((const gchar *) kcv->value);

    if (g_getenv ("MUI_TEST_NO_DEVICE")) {
        g_debug ("MUI_TEST_NO_DEVICE is set, no command is run");
        key_update_finished (ctx, TRUE);
        goto out;
    }

    gtk_label_set_text (GTK_LABEL (ctx->status_label), "Updating key...");
    mui_processor_run_command (mui_page_peek_processor (MUI_PAGE (ctx->self)),
                               ctx->command,
                               MUI_PROCESSOR_COMMAND_FLAG_RESPONSE_EXPECTED,
                               ctx->self->priv->remote_service_cancellable,
                               (GAsyncReadyCallback) run_key_update_command_ready,
                               ctx);

out:
    g_clear_error (&error);
    if (commands)
        g_array_unref (commands);
}

static void
run_key_update (MuiPageRemoteServices *self,
                GtkWidget             *row,
                const gchar           *id,
                GtkWidget             *check_image,
                GtkWidget             *spinner,
                GtkWidget             *status_label)
{
    KeyUpdateContext *ctx;

    if (!g_getenv ("MUI_TEST_NO_DEVICE") && !self->priv->ksn_set) {
        g_warning ("KSN not set, cannot get key load command");
        return;
    }

    /* Setup key update context */
    ctx = g_slice_new0 (KeyUpdateContext);
    ctx->self         = g_object_ref (self);
    ctx->id           = g_strdup (id);
    ctx->check_image  = check_image;
    ctx->spinner      = spinner;
    ctx->status_label = status_label;

    /* Cleanup current flag in all rows */
    common_results_listbox_select_current  (self, NULL);
    common_results_listbox_set_sensitivity (self, row);

    /* Start progress reporting */
    gtk_widget_hide (ctx->check_image);
    gtk_widget_show (ctx->status_label);
    gtk_label_set_text (GTK_LABEL (ctx->status_label), "Downloading key...");
    gtk_widget_show (ctx->spinner);
    gtk_spinner_start (GTK_SPINNER (ctx->spinner));

    g_assert (!self->priv->remote_service_cancellable);
    self->priv->remote_service_cancellable = g_cancellable_new ();
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATION_ONGOING]);

    mui_remote_service_set_common_request_fields (self->priv->remote_service,
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_URL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_CODE])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_USERNAME])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_PASSWORD])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_BILLING_LABEL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_TRID])));

    ctx->ksn = strhex ((uint8_t *) &self->priv->ksn, sizeof (self->priv->ksn), NULL);
    ctx->mut = strhex ((uint8_t *) &self->priv->mut, sizeof (self->priv->mut), NULL);

    mui_remote_service_get_key_load_command (self->priv->remote_service,
                                             ctx->ksn,
                                             ctx->id,
                                             ctx->mut,
                                             self->priv->remote_service_cancellable,
                                             (GAsyncReadyCallback) get_key_load_command_ready,
                                             ctx);
}

/******************************************************************************/
/* Key list */

static void
result_row_activated_key (MuiPageRemoteServices *self,
                          GtkListBoxRow         *row)
{
    RowContext *ctx;

    ctx = g_object_get_data (G_OBJECT (row), "RowContext");
    g_return_if_fail (ctx);

    /* Fow now, only keys */
    g_assert (ctx->type == RESULT_TYPE_KEY);

    if (result_row_confirmation_dialog_run (self,
                                            "Key update",
                                            "Updating device to the following encryption key:",
                                            ctx->name,
                                            "This is a billable event",
                                            NULL) != GTK_RESPONSE_YES)
        return;

    run_key_update (self, GTK_WIDGET (row), ctx->id, ctx->check_image, ctx->spinner, ctx->status_label);
}

static void
get_key_list_ready (MuiRemoteService      *remote_service,
                    GAsyncResult          *res,
                    MuiPageRemoteServices *self)
{
    GError *error = NULL;
    GArray *keys;
    guint   i;

    if ((keys = mui_remote_service_get_key_list_finish (remote_service, res, &error)) == NULL) {
        mui_page_signal_user_error (MUI_PAGE (self), "Couldn't get key list: %s", error->message);
        g_clear_error (&error);
    }

    g_debug ("Key list loading finished");
    stop_main_operation (self);

    if (!keys)
        goto out;

    /* Show key results */
    gtk_widget_show (self->priv->remote_service_results_scrolled_window);
    common_results_listbox_init (self);

    for (i = 0; i < keys->len; i++) {
        MuiKeyInfo *key_info;

        key_info = &g_array_index (keys, MuiKeyInfo, i);

        /* Ignore currenty key (id -1) */
        if (atoi ((const gchar *) key_info->id) < 0) {
            g_debug ("Ignored key info with id: %s", (const gchar *) key_info->id);
            continue;
        }

        common_results_listbox_append_row (self,
                                           RESULT_TYPE_KEY,
                                           (const gchar *) key_info->id,
                                           (const gchar *) key_info->key_name,
                                           (const gchar *) key_info->description,
                                           (const gchar *) key_info->ksi,
                                           NULL,  /* execution type */
                                           NULL); /* command */
    }
    g_array_unref (keys);

    /* Select current key */
    common_results_listbox_select_current (self, self->priv->ksn_set ? (const dukpt_ksn_t *) &self->priv->ksn : NULL);

out:
    g_object_unref (self);
}

static void
remote_service_get_key_list_button_clicked (MuiPageRemoteServices *self)
{
    start_main_operation (self, "Downloading keys...");

    mui_remote_service_get_key_list (self->priv->remote_service,
                                     self->priv->remote_service_cancellable,
                                     (GAsyncReadyCallback) get_key_list_ready,
                                     g_object_ref (self));
}

/******************************************************************************/
/* Generic command */

#define GENERIC_COMMAND_ERROR_PREFIX "Generic command operation failed: "

typedef struct {
    MuiPageRemoteServices *self;
    gchar                 *id;
    gchar                 *ksn;
    gchar                 *mut;
    gchar                 *command;
    GtkWidget             *check_image;  /* soft */
    GtkWidget             *spinner;      /* soft */
    GtkWidget             *status_label; /* soft */
} GenericCommandContext;

static void
generic_command_context_free (GenericCommandContext *ctx)
{
    g_free (ctx->command);
    g_free (ctx->mut);
    g_free (ctx->ksn);
    g_free (ctx->id);
    g_object_unref (ctx->self);
    g_slice_free (GenericCommandContext, ctx);
}

static void
generic_command_finished (GenericCommandContext *ctx,
                          gboolean               success)
{
    g_debug ("Generic command operation %s", success ? "finished successfully" : "failed");

    gtk_widget_hide (ctx->spinner);
    gtk_widget_hide (ctx->status_label);
    if (success) {
        gtk_widget_show (ctx->check_image);

        ctx->self->priv->n_commands_executed++;
        common_update_footer (ctx->self);
    }

    g_clear_object (&ctx->self->priv->remote_service_cancellable);
    g_object_notify_by_pspec (G_OBJECT (ctx->self), properties[PROP_OPERATION_ONGOING]);

    common_results_listbox_set_sensitivity (ctx->self, NULL);

    generic_command_context_free (ctx);
}

static void
processor_generic_command_ready (MuiProcessor          *processor,
                                 GAsyncResult          *res,
                                 GenericCommandContext *ctx)
{
    GError *error = NULL;

    if (!mui_processor_run_command_finish (processor, res, NULL, &error)) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "%s", error->message);
        g_error_free (error);
        generic_command_finished (ctx, FALSE);
        return;
    }

    generic_command_finished (ctx, TRUE);
}

static void
run_command_received (GenericCommandContext *ctx,
                      const GArray          *commands)
{
    MuiCommandInfo *command;

    /* We need 1 field reported only */
    if (commands->len != 1) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "Invalid number of commands received: %u", commands->len);
        generic_command_finished (ctx, FALSE);
        return;
    }

    command = &g_array_index (commands, MuiCommandInfo, 0);

    if (g_getenv ("MUI_TEST_NO_DEVICE")) {
        g_debug ("MUI_TEST_NO_DEVICE is set, no command is run");
        generic_command_finished (ctx, TRUE);
        return;
    }

    gtk_label_set_text (GTK_LABEL (ctx->status_label), "Running command...");
    mui_processor_run_command (mui_page_peek_processor (MUI_PAGE (ctx->self)),
                               (const gchar *) command->value,
                               MUI_PROCESSOR_COMMAND_FLAG_NONE,
                               ctx->self->priv->remote_service_cancellable,
                               (GAsyncReadyCallback) processor_generic_command_ready,
                               ctx);
}

static void
get_command_by_mut_ready (MuiRemoteService      *remote_service,
                          GAsyncResult          *res,
                          GenericCommandContext *ctx)
{
    GError *error = NULL;
    GArray *commands;

    if (!(commands = mui_remote_service_get_command_by_mut_finish (remote_service, res, &error))) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "%s", error->message);
        g_error_free (error);
        generic_command_finished (ctx, FALSE);
        return;
    }

    run_command_received (ctx, commands);
    g_array_unref (commands);
}

static void
get_command_by_mut (GenericCommandContext *ctx)
{
    g_debug ("Downloading command by MUT: ID '%s', KSN '%s', MUT '%s'", ctx->id, ctx->ksn, ctx->mut);
    mui_remote_service_get_command_by_mut (ctx->self->priv->remote_service,
                                           ctx->id,
                                           ctx->ksn,
                                           ctx->mut,
                                           ctx->self->priv->remote_service_cancellable,
                                           (GAsyncReadyCallback) get_command_by_mut_ready,
                                           ctx);
}

static void
get_command_by_ksn_ready (MuiRemoteService      *remote_service,
                          GAsyncResult          *res,
                          GenericCommandContext *ctx)
{
    GError *error = NULL;
    GArray *commands;

    if (!(commands = mui_remote_service_get_command_by_ksn_finish (remote_service, res, &error))) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "%s", error->message);
        g_error_free (error);
        generic_command_finished (ctx, FALSE);
        return;
    }

    run_command_received (ctx, commands);
    g_array_unref (commands);
}

static void get_command_by_ksn (GenericCommandContext *ctx);

static void
command_by_ksn_get_key_list_ready (MuiRemoteService      *remote_service,
                                   GAsyncResult          *res,
                                   GenericCommandContext *ctx)
{
    GError   *error = NULL;
    GArray   *keys;
    guint     i;
    gboolean  found = FALSE;

    if ((keys = mui_remote_service_get_key_list_finish (remote_service, res, &error)) == NULL) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "Couldn't get key list: %s", error->message);
        g_clear_error (&error);
        generic_command_finished (ctx, FALSE);
        return;
    }

    for (i = 0; i < keys->len; i++) {
        MuiKeyInfo *key_info;

        key_info = &g_array_index (keys, MuiKeyInfo, i);
        if (ksi_matches_ksn ((ctx->self->priv->ksn_set ? (const dukpt_ksn_t *) &ctx->self->priv->ksn : NULL), (const gchar *) key_info->ksi)) {
            g_free (ctx->self->priv->key_id);
            ctx->self->priv->key_id = g_strdup ((const gchar *) key_info->id);
            g_debug ("Current key id found: %s", ctx->self->priv->key_id);
            found = TRUE;
            break;
        }
    }
    g_array_unref (keys);

    if (!found) {
        mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "Couldn't find current key ID");
        generic_command_finished (ctx, FALSE);
        return;
    }

    g_assert (ctx->self->priv->key_id);
    get_command_by_ksn (ctx);
}

static gchar *
build_test_privileged_command (const gchar       *command,
                               const dukpt_ksn_t *ksn)
{
    gsize        command_length;
    gsize        command_bin_size;
    gsize        command_data_size;
    gsize        padded_command_bin_size;
    gsize        privileged_command_bin_size;
    gsize        buffer_size;
    guint8      *buffer = NULL;
    guint8      *encbuffer = NULL;
    dukpt_key_t  ipek;
    dukpt_key_t  key;
    gchar       *privileged_command = NULL;
    gchar        command_id[3];

    command_length = strlen (command);

    if (command_length <= 2) {
        g_debug ("Command too short");
        goto out;
    }
    if ((command_length % 2) != 0) {
        g_debug ("Command not full hex");
        goto out;
    }

    /* NOTE: the commands returned by GetCommandList() don't have the length
     * embedded, we must add it ourselves (1 byte) */
    command_bin_size = (command_length / 2) + 1;
    command_data_size = command_bin_size - 2;
    padded_command_bin_size = 8 * ((command_bin_size / 8) + (command_bin_size % 8 != 0));
    privileged_command_bin_size = command_bin_size + 4;
    buffer_size = MAX (padded_command_bin_size, privileged_command_bin_size);

    buffer    = g_malloc0 (buffer_size);
    encbuffer = g_malloc0 (buffer_size);

    /* Convert to binary the command id (1 byte) */
    strncpy (command_id, command, 2);
    command_id[2] = '\0';
    if (strbin (command_id, &buffer[0], 2) != 1) {
        g_debug ("Couldn't convert hex command id to binary");
        goto out;
    }

    /* Set size including MAC */
    buffer[1] = command_data_size + 4;

    /* Convert to binary the command data */
    g_assert (command_data_size <= (buffer_size - 2));
    if (strbin (&command[2], &buffer[2], command_data_size) != command_data_size) {
        g_debug ("Couldn't convert hex command data to binary");
        goto out;
    }

    /* Compute MAC request key and encrypt */
    dukpt_compute_ipek ((const dukpt_key_t *) &test_key_bdk, ksn, &ipek);
    dukpt_compute_key ((const dukpt_key_t *) &ipek, ksn, DUKPT_KEY_TYPE_MAC_REQUEST, &key);
    dukpt_encrypt ((const dukpt_key_t *) &key, buffer, padded_command_bin_size, encbuffer, padded_command_bin_size);

    /* MAC is first 4 bytes in encbuffer */
    memcpy (&buffer[command_bin_size], &encbuffer[0], 4);

    privileged_command = strhex (buffer, privileged_command_bin_size, "");
    g_debug ("Test privileged command built: '%s' --> '%s'", command, privileged_command);

out:
    g_free (encbuffer);
    g_free (buffer);

    return privileged_command;
}

static void
get_command_by_ksn (GenericCommandContext *ctx)
{
    /* Key ID already known? */
    if (ctx->self->priv->key_id) {
        g_debug ("Downloading command by KSN: ID '%s', KSN '%s', KEY ID: '%s'", ctx->id, ctx->ksn, ctx->self->priv->key_id);
        mui_remote_service_get_command_by_ksn (ctx->self->priv->remote_service,
                                               ctx->id,
                                               ctx->ksn,
                                               ctx->self->priv->key_id,
                                               ctx->self->priv->remote_service_cancellable,
                                               (GAsyncReadyCallback) get_command_by_ksn_ready,
                                               ctx);
        return;
    }

    /* Test key? */
    if (ksi_matches_ksn (ctx->self->priv->ksn_set ? (const dukpt_ksn_t *) &ctx->self->priv->ksn : NULL, test_key_ksi)) {
        gchar *privileged;

        privileged = build_test_privileged_command (ctx->command, (const dukpt_ksn_t *) &ctx->self->priv->ksn);
        if (!privileged) {
            mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "Couldn't generate privileged command from: %s", ctx->command);
            generic_command_finished (ctx, FALSE);
            return;
        }

        gtk_label_set_text (GTK_LABEL (ctx->status_label), "Running command...");
        mui_processor_run_command (mui_page_peek_processor (MUI_PAGE (ctx->self)),
                                   privileged,
                                   MUI_PROCESSOR_COMMAND_FLAG_NONE,
                                   ctx->self->priv->remote_service_cancellable,
                                   (GAsyncReadyCallback) processor_generic_command_ready,
                                   ctx);
        return;
    }

    g_debug ("Need to find current key id...");
    mui_remote_service_get_key_list (ctx->self->priv->remote_service,
                                     ctx->self->priv->remote_service_cancellable,
                                     (GAsyncReadyCallback) command_by_ksn_get_key_list_ready,
                                     ctx);
}

static void
run_generic_command (MuiPageRemoteServices *self,
                     GtkWidget             *row,
                     const gchar           *id,
                     const gchar           *execution_type,
                     const gchar           *command,
                     GtkWidget             *check_image,
                     GtkWidget             *spinner,
                     GtkWidget             *status_label)
{
    GenericCommandContext *ctx;

    if (g_getenv ("MUI_TEST_NO_DEVICE")) {
        g_debug ("MUI_TEST_NO_DEVICE is set, no command is run");
        return;
    }

    /* Setup generic command context */
    ctx = g_slice_new0 (GenericCommandContext);
    ctx->self         = g_object_ref (self);
    ctx->check_image  = check_image;
    ctx->spinner      = spinner;
    ctx->status_label = status_label;
    ctx->id           = g_strdup (id);
    ctx->command      = g_strdup (command);

    /* Cleanup current flag in all rows */
    common_results_listbox_select_current  (self, NULL);
    common_results_listbox_set_sensitivity (self, row);

    /* Start progress reporting */
    gtk_widget_show (ctx->status_label);
    gtk_label_set_text (GTK_LABEL (ctx->status_label), "Downloading command...");
    gtk_widget_show (ctx->spinner);
    gtk_spinner_start (GTK_SPINNER (ctx->spinner));

    g_assert (!self->priv->remote_service_cancellable);
    self->priv->remote_service_cancellable = g_cancellable_new ();
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_OPERATION_ONGOING]);

    mui_remote_service_set_common_request_fields (self->priv->remote_service,
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_URL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_CODE])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_USERNAME])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_PASSWORD])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_BILLING_LABEL])),
                                                  gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_TRID])));

    ctx->ksn = strhex ((uint8_t *) &self->priv->ksn, sizeof (self->priv->ksn), NULL);
    if (!g_strcmp0 (execution_type, "KSN")) {
        get_command_by_ksn (ctx);
        return;
    }

    if (!g_strcmp0 (execution_type, "MUT")) {
        ctx->mut = strhex ((uint8_t *) &self->priv->mut, sizeof (self->priv->mut), NULL);
        get_command_by_mut (ctx);
        return;
    }

    mui_page_signal_user_error (MUI_PAGE (ctx->self), GENERIC_COMMAND_ERROR_PREFIX "Invalid execution type received: %s", (const gchar *) execution_type);
    generic_command_finished (ctx, FALSE);
}

/******************************************************************************/
/* Command list */

typedef struct {
    const gchar *command_name;
    const gchar *warning;
    const gchar *message;
} CommandSetting;

static const CommandSetting command_settings[] = {
    { .command_name = "MODE_HID", .message = "The reader is already in HID mode." },
    { .command_name = "MODE_KBE", .warning = "MCCR GTK+ won't able to manage the reader in keyboard emulation mode." },
    { .command_name = "SECLEV_3", .warning = "This operation is not reversible." },
    { .command_name = "SECLEV_4", .warning = "This operation is not reversible." },
};

static void
result_row_activated_command (MuiPageRemoteServices *self,
                              GtkListBoxRow         *row)
{
    RowContext  *ctx;
    const gchar *warning = NULL;
    const gchar *message = NULL;
    guint        i;

    ctx = g_object_get_data (G_OBJECT (row), "RowContext");
    g_return_if_fail (ctx);

    /* Fow now, only command */
    g_assert (ctx->type == RESULT_TYPE_COMMAND);

    for (i = 0; i < G_N_ELEMENTS (command_settings); i++) {
        if (g_strcmp0 (ctx->name, command_settings[i].command_name) == 0) {
            warning = command_settings[i].warning;
            message = command_settings[i].message;
            break;
        }
    }

    /* Don't show any dialog if no warning or message to be shown for this operation,
     * assume no confirmation is needed. */

    if ((message || warning) &&
        (result_row_confirmation_dialog_run (self,
                                             "Settings update",
                                             "Updating device settings:",
                                             ctx->description,
                                             warning,
                                             message) != GTK_RESPONSE_YES))
        return;

    run_generic_command (self, GTK_WIDGET (row), ctx->id, ctx->execution_type, ctx->command, ctx->check_image, ctx->spinner, ctx->status_label);
}

static void
get_command_list_ready (MuiRemoteService      *remote_service,
                        GAsyncResult          *res,
                        MuiPageRemoteServices *self)
{
    GError *error = NULL;
    GArray *commands;
    guint   i;

    if ((commands = mui_remote_service_get_command_list_finish (remote_service, res, &error)) == NULL) {
        mui_page_signal_user_error (MUI_PAGE (self), "Couldn't get command list: %s", error->message);
        g_clear_error (&error);
    }

    g_debug ("Command list loading finished");
    stop_main_operation (self);

    if (!commands)
        goto out;

    /* Show command results */
    gtk_widget_show (self->priv->remote_service_results_scrolled_window);
    common_results_listbox_init (self);

    for (i = 0; i < commands->len; i++) {
        MuiCommandInfo *command_info;

        command_info = &g_array_index (commands, MuiCommandInfo, i);
        common_results_listbox_append_row (self,
                                           RESULT_TYPE_COMMAND,
                                           (const gchar *) command_info->id,
                                           (const gchar *) command_info->name,
                                           (const gchar *) command_info->description,
                                           NULL, /* ksi */
                                           (const gchar *) command_info->execution_type,
                                           (const gchar *) command_info->value);
    }
    g_array_unref (commands);

    common_results_listbox_select_current (self, NULL);

out:
    g_object_unref (self);
}

static void
remote_service_get_command_list_button_clicked (MuiPageRemoteServices *self)
{
    start_main_operation (self, "Downloading commands...");

    mui_remote_service_get_command_list (self->priv->remote_service,
                                         self->priv->remote_service_cancellable,
                                         (GAsyncReadyCallback) get_command_list_ready,
                                         g_object_ref (self));
}

/******************************************************************************/
/* Settings */

typedef struct {
    const gchar *group;
    const gchar *key;
    const gchar *default_value;
} SettingsField;

static const SettingsField settings_fields[SETTINGS_FIELD_ID_LAST] = {
    [SETTINGS_FIELD_ID_RS_URL]           = { "remote-service", "url",                     "https://rainbow.aleksander.es/SCRAv2.svc" },
    [SETTINGS_FIELD_ID_RS_CUSTOMER_CODE] = { "remote-service", "customer-code",           "AM" },
    [SETTINGS_FIELD_ID_RS_USERNAME]      = { "remote-service", "username",                "aleksander" },
    [SETTINGS_FIELD_ID_RS_PASSWORD]      = { "remote-service", "password",                "super secret" },
    [SETTINGS_FIELD_ID_RS_BILLING_LABEL] = { "remote-service", "billing-label",           "Testing" },
    [SETTINGS_FIELD_ID_RS_CUSTOMER_TRID] = { "remote-service", "customer-transaction-id", "1" },
};

static void
store_settings (MuiPageRemoteServices *self)
{
    guint   i;
    GError *error = NULL;
    guint   n_updates = 0;

    if (!self->priv->settings_path || !self->priv->settings)
        return;

    for (i = 0; i < SETTINGS_FIELD_ID_LAST; i++) {
        gchar       *previous;
        const gchar *entry_text;

        previous = g_key_file_get_string (self->priv->settings,
                                          settings_fields[i].group,
                                          settings_fields[i].key,
                                          NULL);

        entry_text = gtk_entry_get_text (GTK_ENTRY (self->priv->settings_entries[i]));

        if (g_strcmp0 (previous, entry_text) != 0) {
            g_key_file_set_string (self->priv->settings,
                                   settings_fields[i].group,
                                   settings_fields[i].key,
                                   entry_text);
            n_updates++;
        }
    }

    if (!n_updates)
        return;

    if (!g_key_file_save_to_file (self->priv->settings, self->priv->settings_path, &error)) {
        g_warning ("Couldn't save settings file: %s", error->message);
        g_clear_error (&error);
    } else
        g_debug ("Settings file saved (%u fields updated)", n_updates);
}

#define SETTINGS_STORE_ID_TIMEOUT 5

static gboolean
settings_store_cb (MuiPageRemoteServices *self)
{
    self->priv->settings_store_id = 0;
    store_settings (self);
    return G_SOURCE_REMOVE;
}

static void
settings_entry_activated (MuiPageRemoteServices *self)
{
    /* reload timeout to store file */
    if (self->priv->settings_store_id)
        g_source_remove (self->priv->settings_store_id);
    self->priv->settings_store_id = g_timeout_add_seconds (SETTINGS_STORE_ID_TIMEOUT, (GSourceFunc) settings_store_cb, self);
}

static void
load_settings (MuiPageRemoteServices *self)
{
    GError   *error = NULL;
    gboolean  needs_store = FALSE;
    guint     i;

    if (G_UNLIKELY (!self->priv->settings))
        self->priv->settings = g_key_file_new ();

    if (G_UNLIKELY (!self->priv->settings_path)) {
        self->priv->settings_path = g_build_filename (g_get_user_config_dir (), "mccr-gtk.conf", NULL);
        g_debug ("Loading settings from: %s", self->priv->settings_path);
    }

    if (!g_key_file_load_from_file (self->priv->settings,
                                    self->priv->settings_path,
                                    G_KEY_FILE_NONE,
                                    &error)) {
        if (!g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
            g_warning ("Couldn't load settings: %s", error->message);
            return;
        }
        g_clear_error (&error);

        g_debug ("No settings file available: creating a new one");

        for (i = 0; i < SETTINGS_FIELD_ID_LAST; i++)
            g_key_file_set_string (self->priv->settings,
                                   settings_fields[i].group,
                                   settings_fields[i].key,
                                   settings_fields[i].default_value);

        needs_store = TRUE;
    }

    for (i = 0; i < G_N_ELEMENTS (settings_fields); i++) {
        gchar *str;

        str = g_key_file_get_string (self->priv->settings,
                                     settings_fields[i].group,
                                     settings_fields[i].key,
                                     &error);
        if (!str) {
            g_warning ("Couldn't read settings %s:%s item: %s", settings_fields[i].group, settings_fields[i].key, error->message);
            g_clear_error (&error);
            g_debug ("Resetting settings %s:%s item to defaults", settings_fields[i].group, settings_fields[i].key);
            g_key_file_set_string (self->priv->settings,
                                   settings_fields[i].group,
                                   settings_fields[i].key,
                                   settings_fields[i].default_value);
            needs_store = TRUE;
        }

        gtk_entry_set_text (GTK_ENTRY (self->priv->settings_entries[i]), str ? str : settings_fields[i].default_value);
        g_free (str);
    }

    if (needs_store && !g_key_file_save_to_file (self->priv->settings, self->priv->settings_path, &error)) {
        g_warning ("Couldn't save settings file: %s", error->message);
        g_clear_error (&error);
    }
}

/******************************************************************************/
/* KSN */

static void
dukpt_ksn_and_counter_updated (MuiPageRemoteServices *self,
                               const gchar           *value)
{
    self->priv->ksn_set = FALSE;

    /* Re-set KSN */
    if (strbin (value, (uint8_t *) &self->priv->ksn, sizeof (self->priv->ksn)) != sizeof (self->priv->ksn)) {
        g_debug ("error parsing KSN '%s'", value);
        return;
    }

    self->priv->ksn_set = TRUE;

    /* Reload current listbox selected, if any */
    common_results_listbox_select_current (self, (const dukpt_ksn_t *) &self->priv->ksn);
}

/******************************************************************************/
/* MUT */

static void
mut_updated (MuiPageRemoteServices *self,
             const gchar           *value)
{
    self->priv->mut_set = FALSE;

    /* Re-set MUT */
    if (strbin (value, (uint8_t *) &self->priv->mut, sizeof (self->priv->mut)) != sizeof (self->priv->mut)) {
        g_debug ("error parsing MUT '%s'", value);
        return;
    }

    self->priv->mut_set = TRUE;
}

/******************************************************************************/
/* Item reporting */

static void
report_item (MuiPage          *_self,
             MuiProcessorItem  item,
             const gchar      *value)
{
    MuiPageRemoteServices *self;

    self = MUI_PAGE_REMOTE_SERVICES (_self);

    g_assert (item < MUI_PROCESSOR_ITEM_LAST);

    switch (item) {
        case MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER:
            dukpt_ksn_and_counter_updated (self, value);
            break;
        case MUI_PROCESSOR_ITEM_MAGTEK_UPDATE_TOKEN:
            mut_updated (self, value);
            break;
        default:
            break;
    }
}

/******************************************************************************/
/* Reset */

static void
reset (MuiPage *_self)
{
    MuiPageRemoteServices *self;

    self = MUI_PAGE_REMOTE_SERVICES (_self);

    common_results_listbox_clear (self);

    self->priv->n_commands_executed = 0;
    common_update_footer (self);

    g_clear_pointer (&self->priv->key_id, g_free);
    self->priv->ksn_set = FALSE;
    self->priv->mut_set = FALSE;
}

/******************************************************************************/

GtkWidget *
mui_page_remote_services_new (void)
{
    MuiPageRemoteServices *self;
    GError                *error = NULL;
    GtkBuilder            *builder;
    GtkWidget             *box;
    GtkWidget             *aux;
    guint                  i;

    self = MUI_PAGE_REMOTE_SERVICES (gtk_widget_new (MUI_TYPE_PAGE_REMOTE_SERVICES, NULL));

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder,
                                        "/es/aleksander/mccr-gtk/mui-page-remote-services.ui",
                                        &error))
        g_error ("error: cannot load remote services builder file: %s", error->message);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
    gtk_widget_show (box);
    gtk_box_pack_start (GTK_BOX (self), box, TRUE, TRUE, 0);

    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_URL]           = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-url-entry"));
    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_CODE] = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-cc-entry"));
    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_USERNAME]      = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-username-entry"));
    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_PASSWORD]      = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-password-entry"));
    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_BILLING_LABEL] = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-billing-entry"));
    self->priv->settings_entries[SETTINGS_FIELD_ID_RS_CUSTOMER_TRID] = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-ctrid-entry"));

    load_settings (self);

    for (i = 0; i < SETTINGS_FIELD_ID_LAST; i++)
        g_signal_connect_swapped (self->priv->settings_entries[i],
                                  "notify::text", G_CALLBACK (settings_entry_activated),
                                  self);

    aux = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-actions"));
    g_object_bind_property (self, "operation-ongoing",
                            aux,  "sensitive",
                            G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

    aux = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-get-key-list-button"));
    g_signal_connect_swapped (aux,
                              "clicked", G_CALLBACK (remote_service_get_key_list_button_clicked),
                              self);

    aux = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-get-command-list-button"));
    g_signal_connect_swapped (aux,
                              "clicked", G_CALLBACK (remote_service_get_command_list_button_clicked),
                              self);

    self->priv->remote_service_results_listbox = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-results-listbox"));
    g_signal_connect_swapped (self->priv->remote_service_results_listbox,
                              "row-activated", G_CALLBACK (common_result_row_activated),
                              self);

    self->priv->remote_service_results_scrolled_window = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-results-scrolled-window"));
    gtk_widget_hide (self->priv->remote_service_results_scrolled_window);

    self->priv->remote_service_ongoing_box          = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-ongoing-box"));
    self->priv->remote_service_ongoing_status_label = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-ongoing-status-label"));
    self->priv->remote_service_ongoing_spinner      = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-ongoing-spinner"));
    gtk_widget_hide (self->priv->remote_service_ongoing_box);

    common_results_listbox_clear (self);

    self->priv->remote_service_footer       = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-footer"));
    self->priv->remote_service_footer_label = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-footer-label"));
    aux = GTK_WIDGET (gtk_builder_get_object (builder, "remote-services-footer-reset-button"));
    g_signal_connect_swapped (aux,
                              "clicked", G_CALLBACK (remote_service_reset_button_clicked),
                              self);

    g_object_unref (builder);

    return GTK_WIDGET (self);
}

/******************************************************************************/

gboolean
mui_page_remote_services_get_operation_ongoing (MuiPageRemoteServices *self)
{
    g_return_val_if_fail (MUI_IS_PAGE_REMOTE_SERVICES (self), FALSE);

    return !!self->priv->remote_service_cancellable;
}

/******************************************************************************/

static void
mui_page_remote_services_init (MuiPageRemoteServices *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_PAGE_REMOTE_SERVICES, MuiPageRemoteServicesPrivate);
    self->priv->remote_service = mui_remote_service_new ();
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    MuiPageRemoteServices *self = MUI_PAGE_REMOTE_SERVICES (object);

    switch (prop_id) {
    case PROP_OPERATION_ONGOING:
        g_value_set_boolean (value, mui_page_remote_services_get_operation_ongoing (self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    MuiPageRemoteServices *self = MUI_PAGE_REMOTE_SERVICES (object);

    g_clear_object (&self->priv->remote_service_results_size_group_center);
    g_clear_object (&self->priv->remote_service_results_size_group_edges);
    g_clear_object (&self->priv->remote_service);

    if (self->priv->settings_store_id) {
        g_source_remove (self->priv->settings_store_id);
        self->priv->settings_store_id = 0;
    }

    g_clear_pointer (&self->priv->key_id, g_free);

    g_clear_pointer (&self->priv->settings_path, g_free);
    g_clear_pointer (&self->priv->settings, g_key_file_unref);

    G_OBJECT_CLASS (mui_page_remote_services_parent_class)->dispose (object);
}

static void
mui_page_remote_services_class_init (MuiPageRemoteServicesClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    MuiPageClass *page_class   = MUI_PAGE_CLASS (klass);

    g_type_class_add_private (klass, sizeof (MuiPageRemoteServicesPrivate));

    object_class->get_property = get_property;
    object_class->dispose      = dispose;
    page_class->report_item    = report_item;
    page_class->reset          = reset;

    properties[PROP_OPERATION_ONGOING] =
        g_param_spec_boolean ("operation-ongoing",
                              "Operation ongoing",
                              "Whether a remote service operation is ongoing",
                              FALSE,
                              G_PARAM_READABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}
