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

#include "mui-page-basic.h"
#include "mui-processor.h"

G_DEFINE_TYPE (MuiPageBasic, mui_page_basic, MUI_TYPE_PAGE)

typedef enum {
    INFO_PAGE_BASIC_ITEM_TRACK_1_DEC_HEX,
    INFO_PAGE_BASIC_ITEM_TRACK_1_ASCII,
    INFO_PAGE_BASIC_ITEM_TRACK_1_MASKED,
    INFO_PAGE_BASIC_ITEM_TRACK_2_DEC_HEX,
    INFO_PAGE_BASIC_ITEM_TRACK_2_ASCII,
    INFO_PAGE_BASIC_ITEM_TRACK_2_MASKED,
    INFO_PAGE_BASIC_ITEM_TRACK_3_DEC_HEX,
    INFO_PAGE_BASIC_ITEM_TRACK_3_ASCII,
    INFO_PAGE_BASIC_ITEM_TRACK_3_MASKED,
    INFO_PAGE_BASIC_ITEM_LAST,
} InfoPageBasicItem;

struct _MuiPageBasicPrivate {
    GtkWidget *footer;
    GtkWidget *decryption_switch;
    GtkWidget *bdk_entry;
    GtkWidget *key_variant_combobox;
    GtkWidget *common_item_labels[MUI_PROCESSOR_ITEM_LAST];
    GtkWidget *basic_item_labels[INFO_PAGE_BASIC_ITEM_LAST];

    /* Decryption support */
    gint             security_level;
    dukpt_key_t      bdk;
    gboolean         bdk_set;
    dukpt_ksn_t      ksn;
    gboolean         ksn_set;
    dukpt_key_t      ipek;
    gboolean         ipek_set;
    dukpt_key_type_t key_type;
    dukpt_key_t      decryption_key;
    gboolean         decryption_key_set;
    dukpt_ksn_t      next_ksn;
    gboolean         next_ksn_set;
};

/******************************************************************************/
/* Decrypted data */

static void
security_level_updated (MuiPageBasic *self,
                        const gchar  *value)
{
    self->priv->security_level = atoi (value);
    if (self->priv->security_level == 0)
        g_debug ("error parsing security level '%s'", value);
    else
        g_debug ("security level updated: %d", self->priv->security_level);

    if (self->priv->security_level <= 2)
        gtk_widget_hide (self->priv->footer);
    else
        gtk_widget_show (self->priv->footer);
}

static void
reload_decryption_key (MuiPageBasic *self)
{
    gchar *str;

    if (self->priv->decryption_key_set)
        return;

    g_debug ("reloading decryption key...");

    if (!self->priv->ksn_set) {
        g_debug ("Cannot initialize IPEK or DUKPT encryption key: no valid KSN set");
        return;
    }

    if (!self->priv->bdk_set) {
        g_debug ("Cannot initialize IPEK or DUKPT encryption key: no valid BDK set");
        return;
    }

    /* IPEK */
    if (!self->priv->ipek_set) {
        self->priv->ipek_set = TRUE;
        dukpt_compute_ipek ((const dukpt_key_t *) &self->priv->bdk, (const dukpt_ksn_t *) &self->priv->ksn, &self->priv->ipek);
        str = strhex ((uint8_t *) &self->priv->ipek, sizeof (self->priv->ipek), ":");
        g_debug ("IPEK initialized: %s", str);
        g_free (str);
    }

    /* Compute the key we'll use to decrypt */
    dukpt_compute_key ((const dukpt_key_t *) &self->priv->ipek, (const dukpt_ksn_t *) &self->priv->ksn, self->priv->key_type, &self->priv->decryption_key);
    str = strhex ((guint8 *) &self->priv->decryption_key, sizeof (self->priv->decryption_key), ":");
    g_debug ("DUKPT decryption key computed: %s", str);
    g_free (str);
    self->priv->decryption_key_set = TRUE;
}

static void
reset_all_track_decryption (MuiPageBasic *self)
{
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_1_DEC_HEX]), "n/a");
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_1_ASCII]),   "n/a");
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_2_DEC_HEX]), "n/a");
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_2_ASCII]),   "n/a");
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_3_DEC_HEX]), "n/a");
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_3_ASCII]),   "n/a");
}

static void
reload_track_decryption (MuiPageBasic      *self,
                         const gchar       *track,
                         MuiProcessorItem   common_item_orig_hex,
                         InfoPageBasicItem  basic_item_dec_hex,
                         InfoPageBasicItem  basic_item_ascii)
{
    const gchar *original;
    guint8      *input = NULL;
    guint8      *output = NULL;
    gsize        buffer_size;
    gssize       data_size;
    gchar       *str = NULL;

    /* 1: factory-only, should never happen
     * 2: unencrypted
     * 3: encrypted
     * 4: encrypted + auth
     */
    if (self->priv->security_level < 2) {
        g_debug ("[%s] no decryption possible: invalid security level: %u", track, self->priv->security_level);
        goto out;
    }

    /* Get original hex from label */
    original = gtk_label_get_text (GTK_LABEL (self->priv->common_item_labels[common_item_orig_hex]));
    if (g_strcmp0 (original, "n/a") == 0)
        goto out;

    buffer_size = 1 + (strlen (original) / 2);

    /* Build original bin */
    input = (guint8 *) g_malloc0 (buffer_size);
    if ((data_size = strbin (original, input, buffer_size)) < 0) {
        g_warning ("[%s] couldn't convert hex string '%s' to binary", track, original);
        goto out;
    }

    /* Check whether decryption is necessary */
    if (self->priv->security_level == 2 || !gtk_switch_get_active (GTK_SWITCH (self->priv->decryption_switch))) {
        str = strascii (input, data_size);
        g_debug ("[%s] decryption not applicable, original ASCII: %s", track, str);
        gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[basic_item_dec_hex]), "n/a");
        gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[basic_item_ascii]), str);
        g_free (str);
        goto out;
    }

    /* Decryption not possible? */
    if (!self->priv->decryption_key_set) {
        g_warning ("[%s] couldn't decrypt: decryption key not set", track);
        goto out;
    }

    /* Decrypt */
    output = (guint8 *) g_malloc0 (buffer_size);
    dukpt_decrypt ((const dukpt_key_t *) &self->priv->decryption_key, input, data_size, output, data_size);

    str = strhex (output, data_size, " ");
    g_debug ("[%s] decrypted: %s", track, str);
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[basic_item_dec_hex]), str);
    g_free (str);

    str = strascii (output, data_size);
    g_debug ("[%s] decrypted ASCII: %s", track, str);
    gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[basic_item_ascii]), str);
    g_free (str);

out:
    g_free (input);
    g_free (output);
}

static void
reload_decryption (MuiPageBasic *self)
{
    /* Support relaunching decryption when the keys change but not the
     * encrypted data to decrypt (e.g. when changing the BDK or the key type
     * in the basic page) */

    reload_decryption_key (self);

    reload_track_decryption (self, "track 1", MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_1_DEC_HEX, INFO_PAGE_BASIC_ITEM_TRACK_1_ASCII);
    reload_track_decryption (self, "track 2", MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_2_DEC_HEX, INFO_PAGE_BASIC_ITEM_TRACK_2_ASCII);
    reload_track_decryption (self, "track 3", MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_3_DEC_HEX, INFO_PAGE_BASIC_ITEM_TRACK_3_ASCII);
}

/******************************************************************************/
/* BDK */

static void
bdk_entry_activated (MuiPageBasic *self)
{
    const gchar   *str;
    gsize          data_size;
    PangoAttrList *attrs;

    /* BDK changed, we need to reload BDK, IPEK, decryption key and decrypted text */
    self->priv->bdk_set            = FALSE;
    self->priv->ipek_set           = FALSE;
    self->priv->decryption_key_set = FALSE;
    reset_all_track_decryption (self);

    attrs = pango_attr_list_new ();
    pango_attr_list_insert (attrs, pango_attr_weight_new (PANGO_WEIGHT_BOLD));

    str = gtk_entry_get_text (GTK_ENTRY (self->priv->bdk_entry));

    if (((data_size = strbin (str, (guint8 *)&self->priv->bdk, sizeof (self->priv->bdk))) < 0) ||
        (data_size != sizeof (self->priv->bdk))) {
        pango_attr_list_insert (attrs, pango_attr_foreground_new (0xFFFF, 0x0000, 0x0000));
        g_warning ("Couldn't convert hex string '%s' to BDK binary key", str);
    } else
        self->priv->bdk_set = TRUE;

    gtk_entry_set_attributes (GTK_ENTRY (self->priv->bdk_entry), attrs);
    pango_attr_list_unref (attrs);

    reload_decryption (self);
}

static gboolean
bdk_entry_focus_out (MuiPageBasic *self)
{
    bdk_entry_activated (self);
    return FALSE;
}

/******************************************************************************/
/* Decryption key type */

static void
key_type_updated (MuiPageBasic *self)
{
    const gchar *key_variant_str;

    /* Key type changed, we need to reload decryption key and decrypted text */
    self->priv->decryption_key_set = FALSE;

    /* Decryption key */
    key_variant_str = gtk_combo_box_get_active_id (GTK_COMBO_BOX (self->priv->key_variant_combobox));
    if (g_strcmp0 (key_variant_str, "pin") == 0)
        self->priv->key_type = DUKPT_KEY_TYPE_PIN_ENCRYPTION;
    else if (g_strcmp0 (key_variant_str, "mac-request") == 0)
        self->priv->key_type = DUKPT_KEY_TYPE_MAC_REQUEST;
    else if (g_strcmp0 (key_variant_str, "mac-response") == 0)
        self->priv->key_type = DUKPT_KEY_TYPE_MAC_RESPONSE;
    else if (g_strcmp0 (key_variant_str, "data-request") == 0)
        self->priv->key_type = DUKPT_KEY_TYPE_DATA_REQUEST;
    else if (g_strcmp0 (key_variant_str, "data-response") == 0)
        self->priv->key_type = DUKPT_KEY_TYPE_DATA_RESPONSE;
    else
        g_assert_not_reached ();

    /* Update decryption in basic page */
    reload_decryption (self);
}

/******************************************************************************/
/* KSN
 *
 * The new KSN will be always stored in the "next KSN" field. Only when a swipe
 * happens we move the "next KSN" to the "KSN" field, so that all decryption happens
 * with the correct one.
 */

static void
dukpt_ksn_and_counter_updated (MuiPageBasic *self,
                               const gchar  *value)
{
    gchar *str;

    self->priv->next_ksn_set = FALSE;

    /* Re-set KSN */
    if (strbin (value, (uint8_t *) &self->priv->next_ksn, sizeof (self->priv->next_ksn)) != sizeof (self->priv->next_ksn)) {
        g_debug ("error parsing KSN '%s'", value);
        return;
    }

    self->priv->next_ksn_set = TRUE;

    str = strhex ((uint8_t *) &self->priv->next_ksn, sizeof (self->priv->next_ksn), ":");
    g_debug ("Next KSN set: %s", str);
    g_free (str);
}

static void
dukpt_ksn_and_counter_assigned_to_swipe (MuiPageBasic *self)
{
    /* Move KSN from 'next KSN' to 'current KSN' */
    if (self->priv->next_ksn_set) {
        g_debug ("New swipe data received: next KSN --> KSN");
        memcpy (&self->priv->ksn, &self->priv->next_ksn, sizeof (self->priv->next_ksn));
        self->priv->ksn_set      = TRUE;
        self->priv->next_ksn_set = FALSE;

        /* KSN changed, so we need to reload IPEK, decryption key and decrypted text */
        self->priv->ipek_set = FALSE;
        self->priv->decryption_key_set = FALSE;
        reset_all_track_decryption (self);
        reload_decryption_key (self);
    }
}

/******************************************************************************/
/* Masked data */

static void
masked_data_updated (MuiPageBasic      *self,
                     const gchar       *track,
                     MuiProcessorItem   item,
                     InfoPageBasicItem  basic_item,
                     const gchar       *value)
{
    GtkWidget *label;
    gsize      buffer_size;
    gssize     bin_size;
    guint8    *bin = NULL;
    gchar     *ascii = NULL;

    label = self->priv->basic_item_labels[basic_item];
    g_assert (GTK_IS_LABEL (label));

    /* Build original bin */
    buffer_size = 1 + (strlen (value) / 2);
    bin = (guint8 *) g_malloc0 (buffer_size);
    if ((bin_size = strbin (value, bin, buffer_size)) < 0) {
        g_warning ("[%s] couldn't convert hex string '%s' to binary", track, value);
        goto out;
    }

    ascii = strascii (bin, bin_size);
    gtk_label_set_text (GTK_LABEL (label), ascii);

out:
    g_free (bin);
    g_free (ascii);
}

/******************************************************************************/
/* Item reporting */

static void
report_item (MuiPage          *_self,
             MuiProcessorItem  item,
             const gchar      *value)
{
    MuiPageBasic *self;

    self = MUI_PAGE_BASIC (_self);

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
        case MUI_PROCESSOR_ITEM_SECURITY_LEVEL:
            security_level_updated (self, value);
            break;
        case MUI_PROCESSOR_ITEM_DUKPT_KSN_AND_COUNTER:
            dukpt_ksn_and_counter_updated (self, value);
            break;
        case MUI_PROCESSOR_ITEM_SWIPE_STARTED:
            reset_all_track_decryption (self);
            break;
        case MUI_PROCESSOR_ITEM_TRACK_1_MASKED_DATA:
            masked_data_updated (self, "track 1", MUI_PROCESSOR_ITEM_TRACK_1_MASKED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_1_MASKED, value);
            break;
        case MUI_PROCESSOR_ITEM_TRACK_2_MASKED_DATA:
            masked_data_updated (self, "track 2", MUI_PROCESSOR_ITEM_TRACK_2_MASKED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_2_MASKED, value);
            break;
        case MUI_PROCESSOR_ITEM_TRACK_3_MASKED_DATA:
            masked_data_updated (self, "track 3", MUI_PROCESSOR_ITEM_TRACK_3_MASKED_DATA, INFO_PAGE_BASIC_ITEM_TRACK_3_MASKED, value);
            break;
        case MUI_PROCESSOR_ITEM_SWIPE_FINISHED:
            dukpt_ksn_and_counter_assigned_to_swipe (self);
            reload_decryption (self);
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
    MuiPageBasic *self;
    guint         i;

    self = MUI_PAGE_BASIC (_self);

    /* Reset all labels to n/a */
    for (i = MUI_PROCESSOR_ITEM_MANUFACTURER; i < MUI_PROCESSOR_ITEM_LAST; i++) {
        if (GTK_IS_LABEL (self->priv->common_item_labels[i]))
            gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[i]), "n/a");
    }
    for (i = 0; i < INFO_PAGE_BASIC_ITEM_LAST; i++) {
        if (GTK_IS_LABEL (self->priv->basic_item_labels[i]))
            gtk_label_set_text (GTK_LABEL (self->priv->basic_item_labels[i]), "n/a");
    }

    gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]), "n/a");
    gtk_widget_hide (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]);

    gtk_label_set_text (GTK_LABEL (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]), "Initializing...");
    gtk_widget_show (self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]);
}

/******************************************************************************/

GtkWidget *
mui_page_basic_new (void)
{
    MuiPageBasic *self;
    GError       *error = NULL;
    GtkBuilder   *builder;
    GtkWidget    *box;

    self = MUI_PAGE_BASIC (gtk_widget_new (MUI_TYPE_PAGE_BASIC, NULL));

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder,
                                        "/es/aleksander/mccr-gtk/mui-page-basic.ui",
                                        &error))
        g_error ("error: cannot load basic page builder file: %s", error->message);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
    gtk_widget_show (box);
    gtk_box_pack_start (GTK_BOX (self), box, TRUE, TRUE, 0);

    /* Common labels */
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS]                 = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-status"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_STATUS_ERROR]           = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-status-error"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_1_ENCRYPTED_DATA] = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-orig-hex-track-1"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_2_ENCRYPTED_DATA] = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-orig-hex-track-2"));
    self->priv->common_item_labels[MUI_PROCESSOR_ITEM_TRACK_3_ENCRYPTED_DATA] = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-orig-hex-track-3"));

    /* Swipe info labels */
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_1_DEC_HEX]  = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-dec-hex-track-1"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_1_ASCII]    = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-ascii-track-1"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_1_MASKED]   = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-masked-track-1"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_2_DEC_HEX]  = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-dec-hex-track-2"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_2_ASCII]    = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-ascii-track-2"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_2_MASKED]   = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-masked-track-2"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_3_DEC_HEX]  = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-dec-hex-track-3"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_3_ASCII]    = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-ascii-track-3"));
    self->priv->basic_item_labels[INFO_PAGE_BASIC_ITEM_TRACK_3_MASKED]   = GTK_WIDGET (gtk_builder_get_object (builder, "basic-label-masked-track-3"));

    /* Decryption options footer */

    self->priv->decryption_switch = GTK_WIDGET (gtk_builder_get_object (builder, "basic-decryption-switch"));
    g_object_bind_property (self->priv->decryption_switch, "active",
                            GTK_WIDGET (gtk_builder_get_object (builder, "basic-footer-center")), "sensitive",
                            G_BINDING_SYNC_CREATE);

    g_object_bind_property (self->priv->decryption_switch, "active",
                            GTK_WIDGET (gtk_builder_get_object (builder, "basic-footer-right")), "sensitive",
                            G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped (self->priv->decryption_switch,
                              "notify::active", G_CALLBACK (reload_decryption),
                              self);

    self->priv->bdk_entry = GTK_WIDGET (gtk_builder_get_object (builder, "basic-bdk-entry"));
    g_signal_connect_swapped (self->priv->bdk_entry,
                              "notify::text", G_CALLBACK (bdk_entry_activated),
                              self);
    g_signal_connect_swapped (self->priv->bdk_entry,
                              "activate", G_CALLBACK (bdk_entry_activated),
                              self);
    g_signal_connect_swapped (self->priv->bdk_entry,
                              "focus-out-event", G_CALLBACK (bdk_entry_focus_out),
                              self);
    bdk_entry_activated (self);

    self->priv->key_variant_combobox = GTK_WIDGET (gtk_builder_get_object (builder, "basic-key-variant-combobox"));
    g_signal_connect_swapped (self->priv->key_variant_combobox,
                              "notify::active-id", G_CALLBACK (key_type_updated),
                              self);
    key_type_updated (self);

    self->priv->footer = GTK_WIDGET (gtk_builder_get_object (builder, "basic-footer"));

    g_object_unref (builder);

    return GTK_WIDGET (self);
}

/******************************************************************************/

static void
mui_page_basic_init (MuiPageBasic *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_PAGE_BASIC, MuiPageBasicPrivate);
}

static void
mui_page_basic_class_init (MuiPageBasicClass *klass)
{
    MuiPageClass *page_class = MUI_PAGE_CLASS (klass);

    g_type_class_add_private (klass, sizeof (MuiPageBasicPrivate));

    page_class->report_item = report_item;
    page_class->reset       = reset;
}
