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

#ifndef MUI_WINDOW_H
#define MUI_WINDOW_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MUI_TYPE_WINDOW         (mui_window_get_type ())
#define MUI_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MUI_TYPE_WINDOW, MuiWindow))
#define MUI_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MUI_TYPE_WINDOW, MuiWindowClass))
#define MUI_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MUI_TYPE_WINDOW))
#define MUI_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MUI_TYPE_WINDOW))
#define MUI_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MUI_TYPE_WINDOW, MuiWindowClass))

typedef struct _MuiWindow        MuiWindow;
typedef struct _MuiWindowClass   MuiWindowClass;
typedef struct _MuiWindowPrivate MuiWindowPrivate;

struct _MuiWindow {
    GtkApplicationWindow  parent_instance;
    MuiWindowPrivate     *priv;
};

struct _MuiWindowClass {
    GtkApplicationWindowClass parent_class;
};

GType mui_window_get_type (void) G_GNUC_CONST;

GtkWidget *mui_window_new (void);

G_END_DECLS

#endif /* MUI_WINDOW_H */
