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

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include <common.h>

#include "mui-resources.h"
#include "mui-remote-service.h"
#include "mui-remote-service-utils.h"

G_DEFINE_TYPE (MuiRemoteService, mui_remote_service, G_TYPE_OBJECT)

struct _MuiRemoteServicePrivate {
    SoupSession *soup_session;

    gchar *url;
    gchar *customer_code;
    gchar *username;
    gchar *password;
    gchar *billing_label;
    gchar *customer_transaction_id;
};

/*****************************************************************************/

void
mui_remote_service_set_common_request_fields (MuiRemoteService *self,
                                              const gchar      *url,
                                              const gchar      *customer_code,
                                              const gchar      *username,
                                              const gchar      *password,
                                              const gchar      *billing_label,
                                              const gchar      *customer_transaction_id)
{
#define REPLACE_FIELD(FIELD) do {                           \
        if (g_strcmp0 (self->priv->FIELD, FIELD) != 0) {    \
            g_free (self->priv->FIELD);                     \
            self->priv->FIELD = g_strdup (FIELD);           \
        }                                                   \
    } while (0)

    REPLACE_FIELD (url);
    REPLACE_FIELD (customer_code);
    REPLACE_FIELD (username);
    REPLACE_FIELD (password);
    REPLACE_FIELD (billing_label);
    REPLACE_FIELD (customer_transaction_id);

#undef REPLACE_FIELD
}

/*****************************************************************************/

static gchar *
remote_service_http_request_finish (MuiRemoteService  *self,
                                    GAsyncResult      *res,
                                    GError           **error)
{
    return (gchar *) g_task_propagate_pointer (G_TASK (res), error);
}

static void
session_send_async_ready (SoupSession  *soup_session,
                          GAsyncResult *res,
                          GTask        *task)
{
    SoupMessage  *msg;
    GInputStream *istream = NULL;
    GError       *error = NULL;
    GString      *str;
    goffset       body_length;

    msg = (SoupMessage *) g_task_get_task_data (task);
    str = g_string_new ("");

    g_debug ("HTTP response status: %u: %s", msg->status_code, soup_status_get_phrase (msg->status_code));

    body_length = soup_message_headers_get_content_length (msg->response_headers);
    g_debug ("HTTP response body size: %" G_GOFFSET_FORMAT, body_length);

    istream = soup_session_send_finish (soup_session, res, &error);
    if (!istream)
        goto out;

    while (1) {
        gchar  buffer [1024];
        gssize read_size;

        read_size = g_input_stream_read (istream, buffer, sizeof (buffer), g_task_get_cancellable (task), &error);
        if (read_size <= 0)
            break;

        str = g_string_append_len (str, buffer, read_size);
    };

out:
    if (error) {
        g_string_free (str, TRUE);
        g_task_return_error (task, error);
    } else {
        g_debug ("[remote service] received http response: %s", str->str);
        g_task_return_pointer (task, g_string_free (str, FALSE), g_free);
    }
    g_object_unref (task);
    g_clear_object (&istream);
}

static void
remote_service_http_request (MuiRemoteService    *self,
                             const gchar         *soap_action,
                             const gchar         *request_body,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GTask       *task;
    SoupMessage *msg;

    task = g_task_new (self, cancellable, callback, user_data);

    g_debug ("[remote service] sending http request: %s", soap_action);
    g_debug ("[remote service] %s", request_body);

    msg = soup_message_new ("POST", self->priv->url);
    if (!msg) {
        g_task_return_new_error (task, G_IO_ERROR, G_IO_ERROR_FAILED, "Invalid URI format");
        g_object_unref (task);
        return;
    }

    soup_message_headers_append (msg->request_headers, "SOAPAction", soap_action);

    soup_message_set_request (msg,
                              "text/xml;charset=UTF-8",
                              SOUP_MEMORY_COPY,
                              request_body,
                              strlen (request_body));

    g_task_set_task_data (task, msg, (GDestroyNotify) g_object_unref);
    soup_session_send_async (self->priv->soup_session,
                             msg,
                             cancellable,
                             (GAsyncReadyCallback) session_send_async_ready,
                             task);
}

/*****************************************************************************/

#define DEFAULT_TEST_DELAY_SECS 3

static gchar *
remote_service_test_request_finish (MuiRemoteService  *self,
                                    GAsyncResult      *res,
                                    GError           **error)
{
    return (gchar *) g_task_propagate_pointer (G_TASK (res), error);
}

static gboolean
remote_service_test_request_cb (GTask *task)
{
    const gchar *response;

    response = g_task_get_task_data (task);
    g_task_return_pointer (task, g_strdup (response), g_free);
    g_object_unref (task);
    return G_SOURCE_REMOVE;
}

static void
remote_service_test_request (MuiRemoteService    *self,
                             const gchar         *response_resource_path,
                             GCancellable        *cancellable,
                             GAsyncReadyCallback  callback,
                             gpointer             user_data)
{
    GBytes    *bytes;
    gchar     *response = NULL;
    GError    *error     = NULL;
    GTask     *task;
    GResource *resource;
    GByteArray *byte_array;

    task = g_task_new (self, cancellable, callback, user_data);

    resource = mui_get_resource ();
    g_assert (resource);

    if (!(bytes = g_resource_lookup_data (resource, response_resource_path, G_RESOURCE_LOOKUP_FLAGS_NONE, &error))) {
        g_task_return_error (task, error);
        g_object_unref (task);
        return;
    }

    byte_array = g_bytes_unref_to_array (bytes);
    g_byte_array_append (byte_array, (const guint8 *) "\0", 1);
    response = (gchar *) g_byte_array_free (byte_array, FALSE);

    g_task_set_task_data (task, response, g_free);
    g_timeout_add_seconds (DEFAULT_TEST_DELAY_SECS, (GSourceFunc) remote_service_test_request_cb, task);
}

/*****************************************************************************/
/* GetKeyList */

#define GET_KEY_LIST_SOAP_ACTION "http://www.magensa.net/RemoteServices/v2/ISCRAv2/GetKeyList"

GArray *
mui_remote_service_get_key_list_finish (MuiRemoteService  *self,
                                        GAsyncResult      *res,
                                        GError           **error)
{
    gchar  *body;
    GArray *keys;

    if (g_getenv ("MUI_TEST_NO_CONNECTION"))
        body = remote_service_test_request_finish (self, res, error);
    else
        body = remote_service_http_request_finish (self, res, error);
    if (!body)
        return FALSE;

    keys = mui_remote_service_utils_parse_get_key_list_response (body, error);
    g_free (body);

    return keys;
}

void
mui_remote_service_get_key_list (MuiRemoteService    *self,
                                 GCancellable        *cancellable,
                                 GAsyncReadyCallback  callback,
                                 gpointer             user_data)
{
    xmlChar *body;

    if (g_getenv ("MUI_TEST_NO_CONNECTION")) {
        remote_service_test_request (self, "/es/aleksander/mccr-gtk/test/get-key-list-response.xml", cancellable, callback, user_data);
        return;
    }

    body = mui_remote_service_utils_build_get_key_list_request (self->priv->customer_code,
                                                                self->priv->username,
                                                                self->priv->password,
                                                                self->priv->billing_label,
                                                                self->priv->customer_transaction_id);

    remote_service_http_request (self,
                                 GET_KEY_LIST_SOAP_ACTION,
                                 (const gchar *) body,
                                 cancellable,
                                 callback,
                                 user_data);
    xmlFree (body);
}

/*****************************************************************************/
/* GetKeyLoadCommand */

#define GET_KEY_LOAD_COMMAND_SOAP_ACTION "http://www.magensa.net/RemoteServices/v2/ISCRAv2/GetKeyLoadCommand"

GArray *
mui_remote_service_get_key_load_command_finish (MuiRemoteService  *self,
                                                GAsyncResult      *res,
                                                GError           **error)
{
    gchar  *body;
    GArray *commands;

    if (g_getenv ("MUI_TEST_NO_CONNECTION"))
        body = remote_service_test_request_finish (self, res, error);
    else
        body = remote_service_http_request_finish (self, res, error);
    if (!body)
        return FALSE;

    commands = mui_remote_service_utils_parse_get_key_load_command_response (body, error);
    g_free (body);

    return commands;
}

void
mui_remote_service_get_key_load_command (MuiRemoteService    *self,
                                         const gchar         *ksn,
                                         const gchar         *key_id,
                                         const gchar         *mut,
                                         GCancellable        *cancellable,
                                         GAsyncReadyCallback  callback,
                                         gpointer             user_data)
{
    xmlChar *body;


    if (g_getenv ("MUI_TEST_NO_CONNECTION")) {
        remote_service_test_request (self, "/es/aleksander/mccr-gtk/test/get-key-load-command-response.xml", cancellable, callback, user_data);
        return;
    }

    body = mui_remote_service_utils_build_get_key_load_command_request (self->priv->customer_code,
                                                                        self->priv->username,
                                                                        self->priv->password,
                                                                        self->priv->billing_label,
                                                                        self->priv->customer_transaction_id,
                                                                        ksn,
                                                                        key_id,
                                                                        mut);
    remote_service_http_request (self,
                                 GET_KEY_LOAD_COMMAND_SOAP_ACTION,
                                 (const gchar *) body,
                                 cancellable,
                                 callback,
                                 user_data);
    xmlFree (body);
}

/*****************************************************************************/
/* GetCommandList */

#define GET_COMMAND_LIST_SOAP_ACTION "http://www.magensa.net/RemoteServices/v2/ISCRAv2/GetCommandList"

GArray *
mui_remote_service_get_command_list_finish (MuiRemoteService  *self,
                                            GAsyncResult      *res,
                                            GError           **error)
{
    gchar  *body;
    GArray *commands;

    if (g_getenv ("MUI_TEST_NO_CONNECTION"))
        body = remote_service_test_request_finish (self, res, error);
    else
        body = remote_service_http_request_finish (self, res, error);
    if (!body)
        return FALSE;

    commands = mui_remote_service_utils_parse_get_command_list_response (body, error);
    g_free (body);

    return commands;
}

void
mui_remote_service_get_command_list (MuiRemoteService    *self,
                                     GCancellable        *cancellable,
                                     GAsyncReadyCallback  callback,
                                     gpointer             user_data)
{
    xmlChar *body;

    if (g_getenv ("MUI_TEST_NO_CONNECTION")) {
        remote_service_test_request (self, "/es/aleksander/mccr-gtk/test/get-command-list-response.xml", cancellable, callback, user_data);
        return;
    }

    body = mui_remote_service_utils_build_get_command_list_request (self->priv->customer_code,
                                                                    self->priv->username,
                                                                    self->priv->password,
                                                                    self->priv->billing_label,
                                                                    self->priv->customer_transaction_id);

    remote_service_http_request (self,
                                 GET_COMMAND_LIST_SOAP_ACTION,
                                 (const gchar *) body,
                                 cancellable,
                                 callback,
                                 user_data);
    xmlFree (body);
}

/*****************************************************************************/
/* GetCommandByKSN */

#define GET_COMMAND_BY_KSN_SOAP_ACTION "http://www.magensa.net/RemoteServices/v2/ISCRAv2/GetCommandByKSN"

GArray *
mui_remote_service_get_command_by_ksn_finish (MuiRemoteService  *self,
                                              GAsyncResult      *res,
                                              GError           **error)
{
    gchar  *body;
    GArray *commands;

    if (g_getenv ("MUI_TEST_NO_CONNECTION"))
        body = remote_service_test_request_finish (self, res, error);
    else
        body = remote_service_http_request_finish (self, res, error);
    if (!body)
        return FALSE;

    commands = mui_remote_service_utils_parse_get_command_by_ksn_response (body, error);
    g_free (body);

    return commands;
}

void
mui_remote_service_get_command_by_ksn (MuiRemoteService    *self,
                                       const gchar         *command_id,
                                       const gchar         *ksn,
                                       const gchar         *key_id,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    xmlChar *body;

    if (g_getenv ("MUI_TEST_NO_CONNECTION")) {
        remote_service_test_request (self, "/es/aleksander/mccr-gtk/test/get-command-by-ksn-response.xml", cancellable, callback, user_data);
        return;
    }

    body = mui_remote_service_utils_build_get_command_by_ksn_request (self->priv->customer_code,
                                                                      self->priv->username,
                                                                      self->priv->password,
                                                                      self->priv->billing_label,
                                                                      self->priv->customer_transaction_id,
                                                                      command_id,
                                                                      ksn,
                                                                      key_id);

    remote_service_http_request (self,
                                 GET_COMMAND_BY_KSN_SOAP_ACTION,
                                 (const gchar *) body,
                                 cancellable,
                                 callback,
                                 user_data);
    xmlFree (body);
}

/*****************************************************************************/
/* GetCommandByMUT */

#define GET_COMMAND_BY_MUT_SOAP_ACTION "http://www.magensa.net/RemoteServices/v2/ISCRAv2/GetCommandByMUT"

GArray *
mui_remote_service_get_command_by_mut_finish (MuiRemoteService  *self,
                                              GAsyncResult      *res,
                                              GError           **error)
{
    gchar  *body;
    GArray *commands;

    if (g_getenv ("MUI_TEST_NO_CONNECTION"))
        body = remote_service_test_request_finish (self, res, error);
    else
        body = remote_service_http_request_finish (self, res, error);
    if (!body)
        return FALSE;

    commands = mui_remote_service_utils_parse_get_command_by_mut_response (body, error);
    g_free (body);

    return commands;
}

void
mui_remote_service_get_command_by_mut (MuiRemoteService    *self,
                                       const gchar         *command_id,
                                       const gchar         *ksn,
                                       const gchar         *mut,
                                       GCancellable        *cancellable,
                                       GAsyncReadyCallback  callback,
                                       gpointer             user_data)
{
    xmlChar *body;

    if (g_getenv ("MUI_TEST_NO_CONNECTION")) {
        remote_service_test_request (self, "/es/aleksander/mccr-gtk/test/get-command-by-mut-response.xml", cancellable, callback, user_data);
        return;
    }

    body = mui_remote_service_utils_build_get_command_by_mut_request (self->priv->customer_code,
                                                                      self->priv->username,
                                                                      self->priv->password,
                                                                      self->priv->billing_label,
                                                                      self->priv->customer_transaction_id,
                                                                      command_id,
                                                                      ksn,
                                                                      mut);

    remote_service_http_request (self,
                                 GET_COMMAND_BY_MUT_SOAP_ACTION,
                                 (const gchar *) body,
                                 cancellable,
                                 callback,
                                 user_data);
    xmlFree (body);
}

/*****************************************************************************/

MuiRemoteService *
mui_remote_service_new (void)
{
    return MUI_REMOTE_SERVICE (g_object_new (MUI_TYPE_REMOTE_SERVICE, NULL));
}

static void
mui_remote_service_init (MuiRemoteService *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, MUI_TYPE_REMOTE_SERVICE, MuiRemoteServicePrivate);
    self->priv->soup_session = soup_session_new ();
}

static void
dispose (GObject *object)
{
    MuiRemoteService *self = MUI_REMOTE_SERVICE (object);

    g_clear_object (&self->priv->soup_session);

    G_OBJECT_CLASS (mui_remote_service_parent_class)->dispose (object);
}

static void
mui_remote_service_class_init (MuiRemoteServiceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    g_type_class_add_private (object_class, sizeof (MuiRemoteServicePrivate));

    object_class->dispose = dispose;
}
