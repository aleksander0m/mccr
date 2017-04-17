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

#define _GNU_SOURCE
#include <config.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <gtk/gtk.h>

#include <common.h>

#include "mui-page.h"

G_DEFINE_ABSTRACT_TYPE (MuiPage, mui_page, GTK_TYPE_BOX)

enum {
    PROP_0,
    PROP_PROCESSOR,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

enum {
    USER_ERROR,
    CLEAR_USER_ERROR,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

struct _MuiPagePrivate {
    MuiProcessor *processor;
    gulong        report_item_id;
};

/******************************************************************************/

MuiProcessor *
mui_page_peek_processor (MuiPage *self)
{
    return self->priv->processor;
}

MuiProcessor *
mui_page_get_processor (MuiPage *self)
{
    MuiProcessor *processor = NULL;

    g_object_get (self,
                  "processor", &processor,
                  NULL);

    return processor;
}

/******************************************************************************/

void
mui_page_reset (MuiPage *self)
{
    if (MUI_PAGE_GET_CLASS (self)->reset)
        MUI_PAGE_GET_CLASS (self)->reset (self);
}

/******************************************************************************/

void
mui_page_signal_user_error (MuiPage     *self,
                            const gchar *fmt,
                            ...)
{
    va_list  args;
    char    *msg;

    va_start (args, fmt);
    if (vasprintf (&msg, fmt, args) == -1)
        return;
    va_end (args);
    g_signal_emit (self, signals[USER_ERROR], 0, msg);
    free (msg);
}

void
mui_page_signal_clear_user_error (MuiPage *self)
{
    g_signal_emit (self, signals[CLEAR_USER_ERROR], 0);
}

/******************************************************************************/
/* Processor handling */

typedef struct {
    MuiPage          *self;
    MuiProcessorItem  item;
    gchar            *value;
} ReportItemContext;

static void
report_item_context_free (ReportItemContext *ctx)
{
    g_free (ctx->value);
    g_object_unref (ctx->self);
    g_slice_free (ReportItemContext, ctx);
}

static gboolean
report_item_cb (ReportItemContext *ctx)
{
    MUI_PAGE_GET_CLASS (ctx->self)->report_item (ctx->self, ctx->item, ctx->value);
    report_item_context_free (ctx);
    return G_SOURCE_REMOVE;
}

static void
inthread_report_item (MuiPage          *self,
                      MuiProcessorItem  item,
                      const gchar      *value)
{
    ReportItemContext *ctx;

    ctx = g_slice_new0 (ReportItemContext);
    ctx->self  = g_object_ref (self);
    ctx->item  = item;
    ctx->value = g_strdup (value);

    g_idle_add ((GSourceFunc) report_item_cb, ctx);
}

static void
setup_processor (MuiPage *self)
{
    if (!self->priv->processor)
        return;

    g_assert (!self->priv->report_item_id);

    if (MUI_PAGE_GET_CLASS (self)->report_item)
        self->priv->report_item_id = g_signal_connect_swapped (self->priv->processor,
                                                               "report-item",
                                                               G_CALLBACK (inthread_report_item),
                                                               self);
}

static void
cleanup_processor (MuiPage *self)
{
    if (self->priv->report_item_id) {
        g_assert (self->priv->processor);
        g_signal_handler_disconnect (self->priv->processor, self->priv->report_item_id);
        self->priv->report_item_id = 0;
    }
    g_clear_object (&self->priv->processor);
}

/******************************************************************************/

static void
mui_page_init (MuiPage *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_PAGE, MuiPagePrivate);
}

static void
set_property (GObject      *object,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    MuiPage *self = MUI_PAGE (object);

    switch (prop_id) {
    case PROP_PROCESSOR:
        cleanup_processor (self);
        self->priv->processor = g_value_dup_object (value);
        setup_processor (self);
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
    MuiPage *self = MUI_PAGE (object);

    switch (prop_id) {
    case PROP_PROCESSOR:
        g_value_set_object (value, self->priv->processor);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static void
dispose (GObject *object)
{
    MuiPage *self = MUI_PAGE (object);

    cleanup_processor (self);

    G_OBJECT_CLASS (mui_page_parent_class)->dispose (object);
}

static void
mui_page_class_init (MuiPageClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MuiPagePrivate));

    object_class->set_property = set_property;
    object_class->get_property = get_property;
    object_class->dispose      = dispose;

    properties[PROP_PROCESSOR] =
        g_param_spec_object ("processor",
                             "Processor",
                             "MCCR operation processor",
                             MUI_TYPE_PROCESSOR,
                             G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);

    signals[USER_ERROR] =
        g_signal_new ("user-error",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MuiPageClass, user_error),
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 1, G_TYPE_STRING);

    signals[CLEAR_USER_ERROR] =
        g_signal_new ("clear-user-error",
                      G_OBJECT_CLASS_TYPE (object_class),
                      G_SIGNAL_RUN_FIRST,
                      G_STRUCT_OFFSET (MuiPageClass, clear_user_error),
                      NULL, NULL, NULL,
                      G_TYPE_NONE, 0);
}
