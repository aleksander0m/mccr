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

#ifndef MUI_PAGE_H
#define MUI_PAGE_H

#include <gtk/gtk.h>

#include "mui-processor.h"

G_BEGIN_DECLS

#define MUI_TYPE_PAGE         (mui_page_get_type ())
#define MUI_PAGE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MUI_TYPE_PAGE, MuiPage))
#define MUI_PAGE_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MUI_TYPE_PAGE, MuiPageClass))
#define MUI_IS_PAGE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MUI_TYPE_PAGE))
#define MUI_IS_PAGE_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MUI_TYPE_PAGE))
#define MUI_PAGE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MUI_TYPE_PAGE, MuiPageClass))

typedef struct _MuiPage        MuiPage;
typedef struct _MuiPageClass   MuiPageClass;
typedef struct _MuiPagePrivate MuiPagePrivate;

struct _MuiPage {
    GtkBox          parent_instance;
    MuiPagePrivate *priv;
};

struct _MuiPageClass {
    GtkBoxClass parent_class;

    void (* report_item) (MuiPage          *self,
                          MuiProcessorItem  item,
                          const gchar      *value);
    void (* reset)       (MuiPage          *self);

    /* signals */
    void (* user_error)       (MuiPage     *self,
                               const gchar *error);
    void (* clear_user_error) (MuiPage     *self);
};

GType mui_page_get_type (void) G_GNUC_CONST;

MuiProcessor *mui_page_peek_processor          (MuiPage          *self);
MuiProcessor *mui_page_get_processor           (MuiPage          *self);
void          mui_page_reset                   (MuiPage          *self);
void          mui_page_signal_user_error       (MuiPage          *self,
                                                const gchar      *fmt,
                                                ...)  __attribute__((__format__ (__printf__, 2, 3)));
void          mui_page_signal_clear_user_error (MuiPage          *self);

G_END_DECLS

#endif /* MUI_PAGE_H */
