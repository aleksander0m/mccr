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

#ifndef MUI_APP_H
#define MUI_APP_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define MUI_TYPE_APP         (mui_app_get_type ())
#define MUI_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), MUI_TYPE_APP, MuiApp))
#define MUI_APP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), MUI_TYPE_APP, MuiAppClass))
#define MUI_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), MUI_TYPE_APP))
#define MUI_IS_APP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), MUI_TYPE_APP))
#define MUI_APP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), MUI_TYPE_APP, MuiAppClass))

typedef struct _MuiApp      MuiApp;
typedef struct _MuiAppClass MuiAppClass;

struct _MuiApp {
    GtkApplication parent_instance;
};

struct _MuiAppClass {
    GtkApplicationClass parent_class;
};

GType mui_app_get_type (void) G_GNUC_CONST;

MuiApp   *mui_app_new  (void);
void      mui_app_quit (MuiApp *self);

G_END_DECLS

#endif /* MUI_APP_H */
