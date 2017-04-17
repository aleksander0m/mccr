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

#include "mui-app.h"
#include "mui-window.h"

G_DEFINE_TYPE (MuiApp, mui_app, GTK_TYPE_APPLICATION)

/******************************************************************************/
/* Application menu management */

static void
set_menu (MuiApp *self)
{
    GtkBuilder *builder;
    GError     *error = NULL;
    GMenuModel *menu;

    /* Load menus */
    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder,
                                        "/es/aleksander/mccr-gtk/mui-menu.ui",
                                        &error))
        g_error ("error: cannot load menu builder file: %s", error->message);

    /* Setup app menu */
    menu = G_MENU_MODEL (gtk_builder_get_object (builder, "app-menu"));
    gtk_application_set_app_menu (GTK_APPLICATION (self), menu);

    g_object_unref (builder);
}

/******************************************************************************/
/* Quit request */

void
mui_app_quit (MuiApp *self)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "quit", NULL);
}

/******************************************************************************/
/* Application actions setup */

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
    static const gchar *authors[] = {
        "Aleksander Morgado <aleksander@aleksander.es>",
        NULL
    };
    MuiApp    *self;
    GtkWindow *parent;
    GList     *window_list;

    self = MUI_APP (user_data);
    window_list = gtk_application_get_windows (GTK_APPLICATION (self));
    parent = (window_list ? GTK_WINDOW (window_list->data) : NULL);

    gtk_show_about_dialog (parent,
                           "name",           "MCCR",
                           "version",        PACKAGE_VERSION,
                           "comments",       "Simple application to use and manage MagTek credit card readers",
                           "copyright",      "Copyright \xc2\xa9 2017 Zodiac Inflight Innovations\n"
                                             "Copyright \xc2\xa9 2017 Aleksander Morgado\n",
                           "logo-icon-name", "mccr-gtk",
                           "authors",        authors,
                           NULL);
}

static void
quit_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
    MuiApp *self = MUI_APP (user_data);
    GList  *l;

    /* Remove all windows registered in the application */
    while ((l = gtk_application_get_windows (GTK_APPLICATION (self))))
        gtk_application_remove_window (GTK_APPLICATION (self), GTK_WINDOW (l->data));
}

static GActionEntry app_entries[] = {
    { "about", about_cb, NULL, NULL, NULL },
    { "quit",  quit_cb,  NULL, NULL, NULL },
};

/******************************************************************************/
/* Activate */

static MuiWindow *
app_window_new (MuiApp *self)
{
    GtkWidget *window;

    /* Register window in app */
    window = mui_window_new ();
    gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
    gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (window), FALSE);

    /* Start centered */
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);

    /* transfer none */
    return MUI_WINDOW (window);
}

static void
activate (GApplication *application)
{
    MuiWindow *window;

    window = app_window_new (MUI_APP (application));
    gtk_window_present (GTK_WINDOW (window));
}

/******************************************************************************/

static void
startup (GApplication *application)
{
    MuiApp *self = MUI_APP (application);

    /* Chain up parent's startup */
    G_APPLICATION_CLASS (mui_app_parent_class)->startup (application);

    /* Setup actions */
    g_action_map_add_action_entries (G_ACTION_MAP (self),
                                     app_entries, G_N_ELEMENTS (app_entries),
                                     self);

    /* Setup application menu, if applicable */
    set_menu (self);
}

/******************************************************************************/

MuiApp *
mui_app_new (void)
{
    return g_object_new (MUI_TYPE_APP,
                         "application-id", "es.aleksander.MCCR",
                         NULL);
}

static void
mui_app_init (MuiApp *self)
{
    g_set_application_name ("MCCR GTK+");
    gtk_window_set_default_icon_name ("mccr-gtk");
}

static void
mui_app_class_init (MuiAppClass *klass)
{
    GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

    application_class->startup  = startup;
    application_class->activate = activate;
}
