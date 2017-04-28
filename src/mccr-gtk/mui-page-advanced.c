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

#include "mui-page-advanced.h"
#include "mui-processor.h"

G_DEFINE_TYPE (MuiPageAdvanced, mui_page_advanced, MUI_TYPE_PAGE)

struct _MuiPageAdvancedPrivate {
    /* Common labels */
    GtkWidget *common_item_labels[MUI_PROCESSOR_ITEM_LAST];
};

/******************************************************************************/
/* Item reporting */

static void
report_item (MuiPage          *_self,
             MuiProcessorItem  item,
             const gchar      *value)
{
    MuiPageAdvanced *self;

    self = MUI_PAGE_ADVANCED (_self);

    g_assert (item < MUI_PROCESSOR_ITEM_LAST);

    if (GTK_IS_LABEL (self->priv->common_item_labels[item]))
        gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[item]), value);

    switch (item) {
        case MUI_PROCESSOR_ITEM_STATUS:
            gtk_widget_hide (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]);
            gtk_widget_show (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]);
            break;
        case MUI_PROCESSOR_ITEM_STATUS_ERROR:
            gtk_widget_hide (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]);
            gtk_widget_show (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]);
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
    MuiPageAdvanced *self;
    guint         i;

    self = MUI_PAGE_ADVANCED (_self);

    /* Reset all labels to n/a */
    for (i = MUI_PROCESSOR_ITEM_MANUFACTURER; i < MUI_PROCESSOR_ITEM_LAST; i++) {
        if (GTK_IS_LABEL (self->priv->common_item_labels[i]))
            gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[i]), "n/a");
    }

    gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]), "n/a");
    gtk_widget_hide (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]);

    gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]), "Initializing...");
    gtk_widget_show (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]);
}

/******************************************************************************/

GtkWidget *
mui_page_advanced_new (void)
{
    MuiPageAdvanced *self;
    GError       *error = NULL;
    GtkBuilder   *builder;
    GtkWidget    *box;

    self = MUI_PAGE_ADVANCED (gtk_widget_new (MUI_TYPE_PAGE_ADVANCED, NULL));

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder,
                                        "/es/aleksander/mccr-gtk/mui-page-advanced.ui",
                                        &error))
        g_error ("error: cannot load advanced page builder file: %s", error->message);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
    gtk_widget_show (box);
    gtk_box_pack_start (GTK_BOX (self), box, TRUE, TRUE, 0);

    /* Status labels */
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]       = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-status"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR] = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-status-error"));

    /* Device info */
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_MANUFACTURER]          = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-manufacturer"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_PRODUCT]               = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-product"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_SOFTWARE_ID]           = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-software-id"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_USB_SN]                = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-usb-sn"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_DEVICE_SN]             = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-device-sn"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_MAGNESAFE_VERSION]     = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-magnesafe-version"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_SUPPORTED_CARDS]       = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-supported-cards"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_POLLING_INTERVAL]      = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-polling-interval"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_MAX_PACKET_SIZE]       = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-max-packet-size"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_STATUS]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-track-1-status"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_STATUS]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-track-2-status"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_STATUS]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-track-3-status"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_ISO_TRACK_MASK]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-iso-track-mask"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_AAMVA_TRACK_MASK]      = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-aamva-track-mask"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER] = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-dukpt"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_READER_STATE]          = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-reader-state"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_ANTECEDENT]            = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-antecedent"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_SECURITY_LEVEL]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-security-level"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_ENCRYPTION_COUNTER]    = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-encryption-counter"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_MAGTEK_UPDATE_TOKEN]   = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-mut"));

    /* Swipe info */
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_CARD_ENCODE_TYPE]              = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-card-encode-type"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_DECODE_STATUS]         = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-decoding-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA_LENGTH] = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-length-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_MASKED_DATA_LENGTH]    = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-length-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_MASKED_DATA]           = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_DECODE_STATUS]         = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-decoding-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA_LENGTH] = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-length-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_MASKED_DATA_LENGTH]    = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-length-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_MASKED_DATA]           = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_DECODE_STATUS]         = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-decoding-3"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA_LENGTH] = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-length-3"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA]        = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-data-3"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_MASKED_DATA_LENGTH]    = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-length-3"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_MASKED_DATA]           = GTK_WIDGET (gtk_builder_get_object (builder, "advanced-label-masked-data-3"));

    g_object_unref (builder);

    return GTK_WIDGET (self);
}

/******************************************************************************/

static void
mui_page_advanced_init (MuiPageAdvanced *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_PAGE_ADVANCED, MuiPageAdvancedPrivate);
}

static void
mui_page_advanced_class_init (MuiPageAdvancedClass *klass)
{
    MuiPageClass *page_class = MUI_PAGE_CLASS (klass);

    g_type_class_add_private (klass, sizeof (MuiPageAdvancedPrivate));

    page_class->report_item = report_item;
    page_class->reset       = reset;
}
