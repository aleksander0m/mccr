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
#include <gudev/gudev.h>

#include <common.h>
#include <mccr.h>
#include <dukpt.h>

#include "mui-window.h"
#include "mui-processor.h"
#include "mui-page-basic.h"
#include "mui-page-advanced.h"
#include "mui-page-remote-services.h"

#define MAGTEK_VID 0x0801

G_DEFINE_TYPE (MuiWindow, mui_window, GTK_TYPE_APPLICATION_WINDOW)

enum {
    PROP_0,
    PROP_PROCESSOR,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

struct _MuiWindowPrivate {
    /* Properties */
    MuiProcessor *processor;

    /* Common */
    GtkWidget    *headerbar;
    GtkSizeGroup *headerbar_buttons_size;
    GtkWidget    *headerbar_stack_switcher;
    GtkWidget    *box_no_devices;
    GtkWidget    *main_stack;
    GtkWidget    *info_stack;

    /* Pages */
    GtkWidget *page_basic;
    GtkWidget *page_advanced;
    GtkWidget *page_remote_services;

    /* Error message reporting */
    GtkWidget *info_bar_box;
    GtkWidget *info_bar;

    /* Detected device */
    gchar *device_path;

    /* Device monitoring */
    GUdevClient *udev;

    /* Swipe support */
    guint scheduled_id;
};

/******************************************************************************/
/* Report error */

static void
clear_user_error (MuiWindow *self)
{
    if (self->priv->info_bar) {
        gtk_container_remove (GTK_CONTAINER (self->priv->info_bar_box), self->priv->info_bar);
        self->priv->info_bar = NULL;
    }
}

static void
report_user_error (MuiWindow   *self,
                   const gchar *error_message)
{
    GtkWidget *info_bar_label;

    clear_user_error (self);

    g_warning ("error: %s", error_message);

    /* User errors are ONLY shown if main stack is visible */
    if (!gtk_widget_get_visible (self->priv->main_stack))
        return;

    self->priv->info_bar = gtk_info_bar_new ();
    gtk_info_bar_set_message_type (GTK_INFO_BAR (self->priv->info_bar), GTK_MESSAGE_WARNING);
    gtk_info_bar_set_show_close_button (GTK_INFO_BAR (self->priv->info_bar), TRUE);
    g_signal_connect_swapped (self->priv->info_bar, "response", G_CALLBACK (clear_user_error), self);
    gtk_widget_show (self->priv->info_bar);
    gtk_box_pack_start (GTK_BOX (self->priv->info_bar_box), self->priv->info_bar, FALSE, TRUE, 0);

    info_bar_label = gtk_label_new (error_message);
    gtk_widget_show (info_bar_label);
    gtk_label_set_line_wrap (GTK_LABEL (info_bar_label), TRUE);

#if GTK_CHECK_VERSION (3,16,0)
    gtk_label_set_xalign (GTK_LABEL (info_bar_label), 0.0);
    gtk_label_set_yalign (GTK_LABEL (info_bar_label), 0.5);
#else
    gtk_misc_set_alignment (GTK_MISC (info_bar_label), 0.0, 0.5);
#endif

    gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (self->priv->info_bar))), info_bar_label, FALSE, FALSE, 0);

    gtk_widget_show (self->priv->info_bar_box);
}

/******************************************************************************/
/* Swipe support */

static void wait_for_swipe (MuiWindow *self,
                            gboolean   reload_info);

static gboolean
wait_for_swipe_reschedule_cb (MuiWindow *self)
{
    self->priv->scheduled_id = 0;
    wait_for_swipe (self, TRUE);
    return G_SOURCE_REMOVE;
}

static void
wait_for_swipe_ready (MuiProcessor *processor,
                      GAsyncResult *res,
                      MuiWindow    *self)
{
    GError *error = NULL;

    if (!mui_processor_wait_swipe_finish (processor, res, &error)) {
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_TIMED_OUT)) {
            /* Ignore error if device gone */
            if (!self->priv->device_path)
                report_user_error (self, error->message);
            self->priv->scheduled_id = g_timeout_add_seconds (1, (GSourceFunc) wait_for_swipe_reschedule_cb, self);
        } else
            /* On timeout error, just reschedule silently without reloading any
             * info */
            wait_for_swipe (self, FALSE);
        g_error_free (error);
        return;
    }

    wait_for_swipe (self, TRUE);
}

static void
wait_for_swipe (MuiWindow *self,
                gboolean   reload_info)
{
    if (!self->priv->processor)
        return;

    if (reload_info)
        mui_processor_load_properties (self->priv->processor);
    mui_processor_wait_swipe (self->priv->processor, NULL, (GAsyncReadyCallback) wait_for_swipe_ready, g_object_ref (self));
}

static void
cleanup_processor (MuiWindow *self)
{
    if (self->priv->scheduled_id) {
        g_source_remove (self->priv->scheduled_id);
        self->priv->scheduled_id = 0;
    }

    if (self->priv->processor) {
        g_clear_object (&self->priv->processor);
        g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROCESSOR]);
    }
}

static void
setup_processor (MuiWindow *self)
{
    g_assert (!self->priv->processor);

    self->priv->processor = mui_processor_new (self->priv->device_path);
    g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PROCESSOR]);

    mui_processor_start (self->priv->processor);

    wait_for_swipe (self, TRUE);
}

/******************************************************************************/
/* Window logic initialization */

static gboolean
reset_window (MuiWindow *self)
{
    mccr_device_t **devices;

    cleanup_processor (self);

    g_clear_pointer (&self->priv->device_path, g_free);

    mui_page_reset (MUI_PAGE (self->priv->page_basic));
    mui_page_reset (MUI_PAGE (self->priv->page_advanced));
    mui_page_reset (MUI_PAGE (self->priv->page_remote_services));

    devices = mccr_enumerate_devices ();
    if (!devices) {
        if (!g_getenv ("MUI_TEST_NO_DEVICE"))
            gtk_widget_hide (self->priv->main_stack);
        gtk_widget_show (self->priv->box_no_devices);
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->priv->headerbar), "no device found");
        clear_user_error (self);
    } else {
        guint i;

        gtk_widget_hide (self->priv->box_no_devices);
        gtk_widget_show (self->priv->main_stack);

        for (i = 0; devices[i]; i++) {
            if (!self->priv->device_path)
                self->priv->device_path = g_strdup (mccr_device_get_path (devices[i]));
            mccr_device_unref (devices[i]);
        }

        g_assert (self->priv->device_path);
        gtk_header_bar_set_subtitle (GTK_HEADER_BAR (self->priv->headerbar), self->priv->device_path);

        setup_processor (self);
    }

    return !!self->priv->device_path;
}

/******************************************************************************/

static gboolean
validate_device_event (MuiWindow   *self,
                       const gchar *action,
                       GUdevDevice *device)
{
    const gchar *name, *sysfs_path, *aux;
    guint16      vid, pid;

    aux = g_udev_device_get_property (device, "ID_VENDOR_ID");
    vid = (guint16) (aux ? g_ascii_strtoull (aux, NULL, 16) : 0);

    if (vid != MAGTEK_VID)
        return FALSE;

    name = g_udev_device_get_name (device);
    aux = g_udev_device_get_property (device, "ID_MODEL_ID");
    pid = (guint16) (aux ? g_ascii_strtoull (aux, NULL, 16) : 0);
    sysfs_path = g_udev_device_get_sysfs_path (device);

    g_debug ("action: %s, name: %s, sysfs: %s, vid: 0x%04x, pid: 0x%04x",
             action, name, sysfs_path, vid, pid);

    return TRUE;
}

static void
handle_uevent (GUdevClient *client,
               const char  *action,
               GUdevDevice *device,
               MuiWindow   *self)
{
    /* Note: this is just a quick solution, not ideal */
    if (validate_device_event (self, action, device))
        reset_window (self);
}

static void
start_monitoring (MuiWindow *self)
{
    static const gchar *subsystems[] = { "usb/usb_device", NULL };
    GList              *devices, *l;
    gboolean            device_found = FALSE;

    self->priv->udev = g_udev_client_new ((const gchar * const *) subsystems);
    g_signal_connect (self->priv->udev, "uevent", G_CALLBACK (handle_uevent), self);

    devices = g_udev_client_query_by_subsystem (self->priv->udev, "usb");
    for (l = devices; l; l = g_list_next (l)) {
        GUdevDevice *device = G_UDEV_DEVICE (l->data);

        if (validate_device_event (self, "found", device)) {
            if (!device_found)
                device_found = reset_window (self);
        }
        g_object_unref (device);
    }
    g_list_free (devices);
}

/******************************************************************************/
/* Switch main stack */

static void
remote_services_button_toggled (MuiWindow *self,
                                GtkWidget *remote_services_button)
{
    clear_user_error (self);

    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (remote_services_button)))
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->main_stack), "main-remote-services");
    else
        gtk_stack_set_visible_child_name (GTK_STACK (self->priv->main_stack), "main-info");
}

/******************************************************************************/
/* Remote service operations notifications */

static void
remote_services_operation_ongoing_updated (MuiWindow *self)
{
    /* If the operation is now ongoing, clear any previous user error */
    if (mui_page_remote_services_get_operation_ongoing (MUI_PAGE_REMOTE_SERVICES (self->priv->page_remote_services))) {
        clear_user_error (self);
        return;
    }

    /* If operation finished, reload properties */
    if (!g_getenv ("MUI_TEST_NO_DEVICE") && self->priv->processor)
        mui_processor_load_properties (self->priv->processor);
}

/******************************************************************************/

static void
gear_menu_cb (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
    GVariant *state;

    state = g_action_get_state (G_ACTION (action));
    g_action_change_state (G_ACTION (action), g_variant_new_boolean (!g_variant_get_boolean (state)));
    g_variant_unref (state);
}

static GActionEntry win_entries[] = {
    { "gear-menu", gear_menu_cb, NULL, "false", NULL },
};

/******************************************************************************/

GtkWidget *
mui_window_new (void)
{
    MuiWindow  *self;
    GError     *error = NULL;
    GtkBuilder *builder;
    GtkWidget  *box;

    self = g_object_new (MUI_TYPE_WINDOW, NULL);

    builder = gtk_builder_new ();
    if (!gtk_builder_add_from_resource (builder,
                                        "/es/aleksander/mccr-gtk/mui-window.ui",
                                        &error))
        g_error ("error: cannot load window builder file: %s", error->message);

    box = GTK_WIDGET (gtk_builder_get_object (builder, "box"));
    gtk_widget_show (box);
    gtk_container_add (GTK_CONTAINER (self), box);

    /* Main view widgets */
    self->priv->box_no_devices = GTK_WIDGET (gtk_builder_get_object (builder, "box-no-devices"));
    self->priv->main_stack     = GTK_WIDGET (gtk_builder_get_object (builder, "main-stack"));
    self->priv->info_stack     = GTK_WIDGET (gtk_builder_get_object (builder, "info-stack"));

    gtk_widget_show (self->priv->box_no_devices);

    if (!g_getenv ("MUI_TEST_NO_DEVICE"))
        gtk_widget_hide (self->priv->main_stack);

    /* Add basic page */
    self->priv->page_basic = mui_page_basic_new ();
    g_object_bind_property (self,                   "processor",
                            self->priv->page_basic, "processor",
                            G_BINDING_SYNC_CREATE);
    gtk_widget_show (self->priv->page_basic);
    gtk_stack_add_titled (GTK_STACK (self->priv->info_stack), self->priv->page_basic, "basic-box", "Basic");

    /* Add advanced page */
    self->priv->page_advanced = mui_page_advanced_new ();
    g_object_bind_property (self,                      "processor",
                            self->priv->page_advanced, "processor",
                            G_BINDING_SYNC_CREATE);
    gtk_widget_show (self->priv->page_advanced);
    gtk_stack_add_titled (GTK_STACK (self->priv->info_stack), self->priv->page_advanced, "advanced-box", "Advanced");

    /* Add remote services page */
    self->priv->page_remote_services = mui_page_remote_services_new ();
    g_object_bind_property (self,                             "processor",
                            self->priv->page_remote_services, "processor",
                            G_BINDING_SYNC_CREATE);
    gtk_widget_show (self->priv->page_remote_services);
    gtk_stack_add_named (GTK_STACK (self->priv->main_stack), self->priv->page_remote_services, "main-remote-services");
    g_signal_connect_swapped (self->priv->page_remote_services,
                              "user-error",
                              G_CALLBACK (report_user_error),
                              self);
    g_signal_connect_swapped (self->priv->page_remote_services,
                              "notify::operation-ongoing",
                              G_CALLBACK (remote_services_operation_ongoing_updated),
                              self);

    /* Menu and headerbar */
    {
        gboolean   shell_shows_app_menu;
        GtkWidget *stack_switcher;
        GtkWidget *gear_menu_button;
        GtkWidget *remote_services_button;

        /* All widgets in headerbar same height */
        self->priv->headerbar_buttons_size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);

        /* Use headerbar */
        self->priv->headerbar  = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar"));
        stack_switcher         = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar-stack-switcher"));
        gear_menu_button       = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar-gear-menu-button"));
        remote_services_button = GTK_WIDGET (gtk_builder_get_object (builder, "headerbar-remote-services-button"));

        /* All widgets same height */
        gtk_size_group_add_widget (self->priv->headerbar_buttons_size, remote_services_button);
        gtk_size_group_add_widget (self->priv->headerbar_buttons_size, gear_menu_button);
        gtk_size_group_add_widget (self->priv->headerbar_buttons_size, stack_switcher);

        gtk_header_bar_set_title (GTK_HEADER_BAR (self->priv->headerbar), "MCCR GTK+");

        g_object_bind_property (self->priv->main_stack, "visible",
                                stack_switcher,         "visible",
                                G_BINDING_SYNC_CREATE);

        g_object_bind_property (self->priv->main_stack, "visible",
                                remote_services_button, "visible",
                                G_BINDING_SYNC_CREATE);

        g_signal_connect_swapped (remote_services_button,
                                  "toggled",
                                  G_CALLBACK (remote_services_button_toggled),
                                  self);

        g_object_bind_property (remote_services_button, "active",
                                stack_switcher,         "sensitive",
                                G_BINDING_SYNC_CREATE | G_BINDING_INVERT_BOOLEAN);

#if GTK_CHECK_VERSION (3,12,0)
        gtk_menu_button_set_use_popover (GTK_MENU_BUTTON (gear_menu_button), TRUE);
#endif

        /* Select which menu to show in the gear menu */
        g_object_get (G_OBJECT (gtk_settings_get_default ()),
                      "gtk-shell-shows-app-menu", &shell_shows_app_menu,
                      NULL);
        if (shell_shows_app_menu && !g_getenv ("MUI_SHOW_GEAR_APP_MENU"))
            gtk_widget_hide (gear_menu_button);
        else {
            GMenuModel *menu;
            GtkBuilder *menu_builder;

            menu_builder = gtk_builder_new ();
            if (!gtk_builder_add_from_resource (menu_builder,
                                                "/es/aleksander/mccr-gtk/mui-menu.ui",
                                                &error))
                g_error ("error: cannot load menu builder file: %s", error->message);

            gtk_widget_show (gear_menu_button);
            menu = G_MENU_MODEL (gtk_builder_get_object (menu_builder, "gear-app-menu"));
            gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (gear_menu_button), menu);

            g_object_unref (menu_builder);
        }

        /* Headerbar now owned by the window */
        gtk_window_set_titlebar (GTK_WINDOW (self), self->priv->headerbar);
    }

    /* Error reporting */
    {
        self->priv->info_bar_box = GTK_WIDGET (gtk_builder_get_object (builder, "info-bar-box"));
        gtk_widget_hide (self->priv->info_bar_box);
    }

    g_object_unref (builder);

    gtk_window_set_default_size (GTK_WINDOW (self), 600, 400);

    start_monitoring (self);

    return GTK_WIDGET (self);
}

static void
mui_window_init (MuiWindow *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_WINDOW, MuiWindowPrivate);
    g_action_map_add_action_entries (G_ACTION_MAP (self),
                                     win_entries, G_N_ELEMENTS (win_entries),
                                     self);
}

static void
get_property (GObject    *object,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
    MuiWindow *self = MUI_WINDOW (object);

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
    MuiWindow *self = MUI_WINDOW (object);

    cleanup_processor (self);

    g_clear_object (&self->priv->headerbar_buttons_size);
    g_clear_pointer (&self->priv->device_path, g_free);
    g_clear_object (&self->priv->udev);

    G_OBJECT_CLASS (mui_window_parent_class)->dispose (object);
}

static void
mui_window_class_init (MuiWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (klass, sizeof (MuiWindowPrivate));

    object_class->get_property = get_property;
    object_class->dispose      = dispose;

    properties[PROP_PROCESSOR] =
        g_param_spec_object ("processor",
                             "Processor",
                             "MCCR operation processor",
                             MUI_TYPE_PROCESSOR,
                             G_PARAM_READABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}
