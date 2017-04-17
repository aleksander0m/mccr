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

#ifndef MUI_PROCESSOR_H
#define MUI_PROCESSOR_H

#include <glib-object.h>
#include <gio/gio.h>

#define MUI_TYPE_PROCESSOR            (mui_processor_get_type ())
#define MUI_PROCESSOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUI_TYPE_PROCESSOR, MuiProcessor))
#define MUI_PROCESSOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MUI_TYPE_PROCESSOR, MuiProcessorClass))
#define MUI_IS_PROCESSOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUI_TYPE_PROCESSOR))
#define MUI_IS_PROCESSOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), MUI_TYPE_PROCESSOR))
#define MUI_PROCESSOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MUI_TYPE_PROCESSOR, MuiProcessorClass))

typedef struct _MuiProcessor        MuiProcessor;
typedef struct _MuiProcessorClass   MuiProcessorClass;
typedef struct _MuiProcessorPrivate MuiProcessorPrivate;

typedef enum {
    /* Status information */
    MUI_PROCESSOR_ITEM_STATUS,
    MUI_PROCESSOR_ITEM_STATUS_ERROR,
    /* Device information */
    MUI_PROCESSOR_ITEM_MANUFACTURER,
    MUI_PROCESSOR_ITEM_PRODUCT,
    MUI_PROCESSOR_ITEM_SOFTWARE_ID,
    MUI_PROCESSOR_ITEM_USB_SN,
    MUI_PROCESSOR_ITEM_DEVICE_SN,
    MUI_PROCESSOR_ITEM_MAGNESAFE_VERSION,
    MUI_PROCESSOR_ITEM_SUPPORTED_CARDS,
    MUI_PROCESSOR_ITEM_POLLING_INTERVAL,
    MUI_PROCESSOR_ITEM_MAX_PACKET_SIZE,
    MUI_PROCESSOR_ITEM_TRACK_1_STATUS,
    MUI_PROCESSOR_ITEM_TRACK_2_STATUS,
    MUI_PROCESSOR_ITEM_TRACK_3_STATUS,
    MUI_PROCESSOR_ITEM_ISO_TRACK_MASK,
    MUI_PROCESSOR_ITEM_AAMVA_TRACK_MASK,
    MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER,
    MUI_PROCESSOR_ITEM_READER_STATE,
    MUI_PROCESSOR_ITEM_ANTECEDENT,
    MUI_PROCESSOR_ITEM_SECURITY_LEVEL,
    MUI_PROCESSOR_ITEM_ENCRYPTION_COUNTER,
    MUI_PROCESSOR_ITEM_MAGTEK_UPDATE_TOKEN,
    /* Swipe information */
    MUI_PROCESSOR_ITEM_SWIPE_STARTED,
    MUI_PROCESSOR_ITEM_CARD_ENCODE_TYPE,
    MUI_PROCESSOR_ITEM_TRACK_1_DECODE_STATUS,
    MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA_LENGTH,
    MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA,
    MUI_PROCESSOR_ITEM_TRACK_2_DECODE_STATUS,
    MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA_LENGTH,
    MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA,
    MUI_PROCESSOR_ITEM_TRACK_3_DECODE_STATUS,
    MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA_LENGTH,
    MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA,
    MUI_PROCESSOR_ITEM_SWIPE_FINISHED,
    MUI_PROCESSOR_ITEM_LAST
} MuiProcessorItem;

const gchar *mui_processot_item_to_string (MuiProcessorItem item);

struct _MuiProcessor {
    GObject              parent;
    MuiProcessorPrivate *priv;
};

struct _MuiProcessorClass {
    GObjectClass parent;

    void (* report_item) (MuiProcessor     *self,
                          MuiProcessorItem  item,
                          const gchar      *value);
};

GType         mui_processor_get_type           (void);
MuiProcessor *mui_processor_new                (const gchar          *path);
const gchar  *mui_processor_get_path           (MuiProcessor         *self);
void          mui_processor_start              (MuiProcessor         *self);
void          mui_processor_reset              (MuiProcessor         *self);
void          mui_processor_load_properties    (MuiProcessor         *self);
void          mui_processor_wait_swipe         (MuiProcessor         *self,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
gboolean      mui_processor_wait_swipe_finish  (MuiProcessor         *self,
                                                GAsyncResult         *res,
                                                GError              **error);

typedef enum {
    MUI_PROCESSOR_COMMAND_FLAG_NONE,
    MUI_PROCESSOR_COMMAND_FLAG_RESPONSE_EXPECTED,
} MuiProcessorCommandFlag;

void          mui_processor_run_command        (MuiProcessor             *self,
                                                const gchar              *command,
                                                MuiProcessorCommandFlag   flags,
                                                GCancellable             *cancellable,
                                                GAsyncReadyCallback       callback,
                                                gpointer                  user_data);
gboolean      mui_processor_run_command_finish (MuiProcessor             *self,
                                                GAsyncResult             *res,
                                                gchar                   **response,
                                                GError                  **error);

#endif /* MUI_PROCESSOR_H */
