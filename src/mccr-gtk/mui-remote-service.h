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

#ifndef MUI_REMOTE_SERVICE_H
#define MUI_REMOTE_SERVICE_H

#include <glib-object.h>
#include <gio/gio.h>

#define MUI_TYPE_REMOTE_SERVICE            (mui_remote_service_get_type ())
#define MUI_REMOTE_SERVICE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), MUI_TYPE_REMOTE_SERVICE, MuiRemoteService))
#define MUI_REMOTE_SERVICE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), MUI_TYPE_REMOTE_SERVICE, MuiRemoteServiceClass))
#define MUI_IS_REMOTE_SERVICE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), MUI_TYPE_REMOTE_SERVICE))
#define MUI_IS_REMOTE_SERVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), MUI_TYPE_REMOTE_SERVICE))
#define MUI_REMOTE_SERVICE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), MUI_TYPE_REMOTE_SERVICE, MuiRemoteServiceClass))

typedef struct _MuiRemoteService        MuiRemoteService;
typedef struct _MuiRemoteServiceClass   MuiRemoteServiceClass;
typedef struct _MuiRemoteServicePrivate MuiRemoteServicePrivate;


struct _MuiRemoteService {
    GObject                  parent;
    MuiRemoteServicePrivate *priv;
};

struct _MuiRemoteServiceClass {
    GObjectClass parent;
};

GType             mui_remote_service_get_type                  (void);
MuiRemoteService *mui_remote_service_new                       (void);
void              mui_remote_service_set_common_request_fields (MuiRemoteService *self,
                                                                const gchar      *url,
                                                                const gchar      *customer_code,
                                                                const gchar      *username,
                                                                const gchar      *password,
                                                                const gchar      *billing_label,
                                                                const gchar      *customer_transaction_id);

void    mui_remote_service_get_key_list        (MuiRemoteService     *self,
                                                GCancellable         *cancellable,
                                                GAsyncReadyCallback   callback,
                                                gpointer              user_data);
GArray *mui_remote_service_get_key_list_finish (MuiRemoteService     *self,
                                                GAsyncResult         *res,
                                                GError              **error);

void    mui_remote_service_get_key_load_command        (MuiRemoteService     *self,
                                                        const gchar          *ksn,
                                                        const gchar          *key_id,
                                                        const gchar          *mut,
                                                        GCancellable         *cancellable,
                                                        GAsyncReadyCallback   callback,
                                                        gpointer              user_data);
GArray *mui_remote_service_get_key_load_command_finish (MuiRemoteService     *self,
                                                        GAsyncResult         *res,
                                                        GError              **error);

void    mui_remote_service_get_command_list        (MuiRemoteService     *self,
                                                    GCancellable         *cancellable,
                                                    GAsyncReadyCallback   callback,
                                                    gpointer              user_data);
GArray *mui_remote_service_get_command_list_finish (MuiRemoteService     *self,
                                                    GAsyncResult         *res,
                                                    GError              **error);

void    mui_remote_service_get_command_by_ksn        (MuiRemoteService     *self,
                                                      const gchar          *command_id,
                                                      const gchar          *ksn,
                                                      const gchar          *key_id,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
GArray *mui_remote_service_get_command_by_ksn_finish (MuiRemoteService     *self,
                                                      GAsyncResult         *res,
                                                      GError              **error);

void    mui_remote_service_get_command_by_mut        (MuiRemoteService     *self,
                                                      const gchar          *command_id,
                                                      const gchar          *ksn,
                                                      const gchar          *mut,
                                                      GCancellable         *cancellable,
                                                      GAsyncReadyCallback   callback,
                                                      gpointer              user_data);
GArray *mui_remote_service_get_command_by_mut_finish (MuiRemoteService     *self,
                                                      GAsyncResult         *res,
                                                      GError              **error);

#endif /* MUI_REMOTE_SERVICE_H */
