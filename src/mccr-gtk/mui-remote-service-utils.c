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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "mui-remote-service-utils.h"

/******************************************************************************/

typedef struct {
    const gchar *prefix;
    const gchar *href;
} NamespaceDefinition;

typedef enum {
    NAMESPACE_SOAP_ENVELOPE,
    NAMESPACE_REMOTE_SERVICES_V2,
    NAMESPACE_REMOTE_SERVICES_V2_CORE,
    NAMESPACE_N
} Namespace;

static const NamespaceDefinition namespace_definitions[NAMESPACE_N] = {
    [NAMESPACE_SOAP_ENVELOPE]           = { "senv", "http://schemas.xmlsoap.org/soap/envelope/"                     },
    [NAMESPACE_REMOTE_SERVICES_V2]      = { "rs2",  "http://www.magensa.net/RemoteServices/v2/"                     },
    [NAMESPACE_REMOTE_SERVICES_V2_CORE] = { "rs2c", "http://schemas.datacontract.org/2004/07/RemoteServicesv2.Core" },
};

static gboolean
xpath_register_namespaces (xmlXPathContext  *xpath_ctx,
                           GError          **error)
{
    guint i;

    for (i = 0; i < NAMESPACE_N; i++) {
        if (xmlXPathRegisterNs (xpath_ctx, BAD_CAST namespace_definitions[i].prefix, BAD_CAST namespace_definitions[i].href) != 0) {
            g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't register namespace: %s", namespace_definitions[i].href);
            return FALSE;
        }
	}

    return TRUE;
}

static void
node_register_namespaces (xmlNode *node,
                          xmlNs   *ns[NAMESPACE_N])
{
    guint i;

    for (i = 0; i < NAMESPACE_N; i++)
        ns[i] = xmlNewNs (node, BAD_CAST namespace_definitions[i].href, BAD_CAST namespace_definitions[i].prefix);
}

/******************************************************************************/

static void
common_key_info_clear (MuiKeyInfo *key_info)
{
    xmlFree (key_info->id);
    xmlFree (key_info->key_name);
    xmlFree (key_info->description);
    xmlFree (key_info->ksi);
    xmlFree (key_info->key_slot_name_prefix);
}

static void
common_command_info_clear (MuiCommandInfo *command_info)
{
    xmlFree (command_info->command_type);
    xmlFree (command_info->description);
    xmlFree (command_info->execution_type);
    xmlFree (command_info->id);
    xmlFree (command_info->name);
    xmlFree (command_info->value);
}

static GArray *
common_command_info_build_array (const xmlXPathObject  *xpath_obj,
                                 GError               **error)
{
    GArray *commands  = NULL;
    guint   n_commands;
    guint   i;

    n_commands = (xpath_obj->nodesetval ? xpath_obj->nodesetval->nodeNr : 0);

    commands = g_array_sized_new (FALSE, FALSE, sizeof (MuiCommandInfo), n_commands);
    g_array_set_clear_func (commands, (GDestroyNotify) common_command_info_clear);

    for (i = 0; i < n_commands; i++) {
        xmlNode        *node;
        xmlNode        *child;
        MuiCommandInfo  command_info = { 0 };
        guint           j;

        node = xpath_obj->nodesetval->nodeTab[i];
        if (node->type != XML_ELEMENT_NODE)
            continue;

        for (j = 0, child = node->children; child; child = child->next, j++) {
            xmlChar **target = NULL;

            if (child->type != XML_ELEMENT_NODE || !child->name)
                continue;

            if (xmlStrcmp (child->name, BAD_CAST "ID") == 0)
                target = &command_info.id;
            else if (xmlStrcmp (child->name, BAD_CAST "CommandType") == 0)
                target = &command_info.command_type;
            else if (xmlStrcmp (child->name, BAD_CAST "Description") == 0)
                target = &command_info.description;
            else if (xmlStrcmp (child->name, BAD_CAST "ExecutionTypeEnum") == 0)
                target = &command_info.execution_type;
            else if (xmlStrcmp (child->name, BAD_CAST "Name") == 0)
                target = &command_info.name;
            else if (xmlStrcmp (child->name, BAD_CAST "Value") == 0)
                target = &command_info.value;

            if (!target) {
                g_debug ("[command %u, child %u] unknown element node found: %s", i, j, child->name);
                continue;
            }

            if (*target) {
                g_warning ("[command %u, child %u] element node set multiple times: %s", i, j, child->name);
                continue;
            }

            *target = xmlNodeGetContent (child);
        }

        if (!command_info.name) {
            g_warning ("[command %u] missing field in command: Name", i);
            common_command_info_clear (&command_info);
            continue;
        }

        if (!command_info.value) {
            g_warning ("[command %u] missing field in command: Value", i);
            common_command_info_clear (&command_info);
            continue;
        }

        g_debug ("--------------------------------");
        g_debug ("[command %u] command reported", i);
        g_debug ("   id:                   %s", command_info.id);
        g_debug ("   command type:         %s", command_info.command_type);
        g_debug ("   description:          %s", command_info.description);
        g_debug ("   execution type:       %s", command_info.execution_type);
        g_debug ("   name:                 %s", command_info.name);
        g_debug ("   value:                %s", command_info.value);

        g_array_append_val (commands, command_info);
    }

    return commands;
}

static xmlChar *
common_list_request_build (const gchar *command,
                           const gchar *customer_code,
                           const gchar *username,
                           const gchar *password,
                           const gchar *billing_label,
                           const gchar *customer_transaction_id,
                           const gchar *execution_type,
                           const gchar *command_id,
                           const gchar *ksn,
                           const gchar *key_id,
                           const gchar *mut)

{
    xmlDoc  *doc;
    xmlNode *node, *request, *authentication;
    xmlChar *s = NULL;
    int      size;
    xmlNs   *ns[NAMESPACE_N];

    doc = xmlNewDoc (BAD_CAST "1.0");

    node = xmlNewNode (NULL, BAD_CAST "Envelope");
    node_register_namespaces (node, ns);
    xmlSetNs (node, ns[NAMESPACE_SOAP_ENVELOPE]);
    xmlDocSetRootElement (doc, node);

    node    = xmlNewChild (node, ns[NAMESPACE_SOAP_ENVELOPE],      BAD_CAST "Body", NULL);
    node    = xmlNewChild (node, ns[NAMESPACE_REMOTE_SERVICES_V2], BAD_CAST command, NULL);
    request = xmlNewChild (node, ns[NAMESPACE_REMOTE_SERVICES_V2], BAD_CAST "request", NULL);

    authentication = xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Authentication", NULL);

    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "CustomerCode", BAD_CAST customer_code);
    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Password",     BAD_CAST password);
    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Username",     BAD_CAST username);

    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "BillingLabel",          BAD_CAST billing_label);
    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "CustomerTransactionID", BAD_CAST customer_transaction_id);

    if (execution_type)
        xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "ExecutionType", BAD_CAST execution_type);

    if (command_id)
        xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "CommandID", BAD_CAST command_id);

    if (ksn)
        xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "KSN", BAD_CAST ksn);

    if (key_id)
        xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "KeyID", BAD_CAST key_id);

    if (mut)
        xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "UpdateToken", BAD_CAST mut);

    xmlDocDumpMemory (doc, &s, &size);

    if (doc)
        xmlFreeDoc (doc);

    return s;
}

/******************************************************************************/

xmlChar *
mui_remote_service_utils_build_get_key_list_request (const gchar *customer_code,
                                                     const gchar *username,
                                                     const gchar *password,
                                                     const gchar *billing_label,
                                                     const gchar *customer_transaction_id)
{
    return common_list_request_build ("GetKeyList",
                                      customer_code,
                                      username,
                                      password,
                                      billing_label,
                                      customer_transaction_id,
                                      NULL,  /* execution type */
                                      NULL,  /* command id */
                                      NULL,  /* ksn */
                                      NULL,  /* key id */
                                      NULL); /* mut */
}

/******************************************************************************/

GArray *
mui_remote_service_utils_parse_get_key_list_response (const gchar  *str,
                                                      GError      **error)
{
    GArray          *keys      = NULL;
    xmlDoc          *doc       = NULL;
    xmlXPathContext *xpath_ctx = NULL;
    xmlXPathObject  *xpath_obj = NULL;
    guint            n_keys;
    guint            i;

    doc = xmlReadMemory (str, strlen (str), "noname.xml", NULL, 0);
    if (!doc) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read response into XML memory");
        goto out;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (!xpath_register_namespaces (xpath_ctx, error))
        goto out;

    xpath_obj = xmlXPathEvalExpression (BAD_CAST
                                        "/senv:Envelope"
                                        "/senv:Body"
                                        "/rs2:GetKeyListResponse"
                                        "/rs2:GetKeyListResult"
                                        "/rs2c:Keys"
                                        "/rs2c:Key",
                                        xpath_ctx);
    if (!xpath_obj) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't evaluate XPATH expression");
        goto out;
    }

    n_keys = (xpath_obj->nodesetval ? xpath_obj->nodesetval->nodeNr : 0);

    keys = g_array_sized_new (FALSE, FALSE, sizeof (MuiKeyInfo), n_keys);
    g_array_set_clear_func (keys, (GDestroyNotify) common_key_info_clear);

    for (i = 0; i < n_keys; i++) {
        xmlNode    *node;
        xmlNode    *child;
        MuiKeyInfo  key_info = { 0 };
        guint       j;

        node = xpath_obj->nodesetval->nodeTab[i];
        if (node->type != XML_ELEMENT_NODE)
            continue;

        for (j = 0, child = node->children; child; child = child->next, j++) {
            xmlChar **target = NULL;

            if (child->type != XML_ELEMENT_NODE || !child->name)
                continue;

            if (xmlStrcmp (child->name, BAD_CAST "ID") == 0)
                target = &key_info.id;
            else if (xmlStrcmp (child->name, BAD_CAST "Description") == 0)
                target = &key_info.description;
            else if (xmlStrcmp (child->name, BAD_CAST "KSI") == 0)
                target = &key_info.ksi;
            else if (xmlStrcmp (child->name, BAD_CAST "KeyName") == 0)
                target = &key_info.key_name;
            else if (xmlStrcmp (child->name, BAD_CAST "KeySlotNamePrefix") == 0)
                target = &key_info.key_slot_name_prefix;

            if (!target) {
                g_debug ("[key %u, child %u] unknown element node found: %s", i, j, child->name);
                continue;
            }

            if (*target) {
                g_warning ("[key %u, child %u] element node set multiple times: %s", i, j, child->name);
                continue;
            }

            *target = xmlNodeGetContent (child);
        }

        if (!key_info.id) {
            g_warning ("[key %u] missing field in Key: ID", i);
            common_key_info_clear (&key_info);
            continue;
        }

        g_debug ("--------------------------------");
        g_debug ("[key %u] key reported", i);
        g_debug ("   id:                   %s", key_info.id);
        g_debug ("   description:          %s", key_info.description);
        g_debug ("   ksi:                  %s", key_info.ksi);
        g_debug ("   key name:             %s", key_info.key_name);
        g_debug ("   key slot name prefix: %s", key_info.key_slot_name_prefix);

        g_array_append_val (keys, key_info);
    }

out:
    if (xpath_obj)
        xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx)
        xmlXPathFreeContext (xpath_ctx);
    if (doc)
        xmlFreeDoc (doc);

    return keys;
}

/******************************************************************************/

xmlChar *
mui_remote_service_utils_build_get_key_load_command_request (const gchar *customer_code,
                                                             const gchar *username,
                                                             const gchar *password,
                                                             const gchar *billing_label,
                                                             const gchar *customer_transaction_id,
                                                             const gchar *ksn,
                                                             const gchar *key_id,
                                                             const gchar *mut)
{
    xmlDoc  *doc;
    xmlNode *node, *request, *authentication;
    xmlChar *s = NULL;
    int      size;
    xmlNs   *ns[NAMESPACE_N];

    doc = xmlNewDoc (BAD_CAST "1.0");

    node = xmlNewNode (NULL, BAD_CAST "Envelope");
    node_register_namespaces (node, ns);
    xmlSetNs (node, ns[NAMESPACE_SOAP_ENVELOPE]);
    xmlDocSetRootElement (doc, node);

    node    = xmlNewChild (node, ns[NAMESPACE_SOAP_ENVELOPE],      BAD_CAST "Body", NULL);
    node    = xmlNewChild (node, ns[NAMESPACE_REMOTE_SERVICES_V2], BAD_CAST "GetKeyLoadCommand", NULL);
    request = xmlNewChild (node, ns[NAMESPACE_REMOTE_SERVICES_V2], BAD_CAST "request", NULL);

    authentication = xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Authentication", NULL);

    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "CustomerCode", BAD_CAST customer_code);
    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Password",     BAD_CAST password);
    xmlNewChild (authentication, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "Username",     BAD_CAST username);

    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "BillingLabel",          BAD_CAST billing_label);
    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "CustomerTransactionID", BAD_CAST customer_transaction_id);
    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "KSN",                   BAD_CAST ksn);
    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "KeyID",                 BAD_CAST key_id);
    xmlNewChild (request, ns[NAMESPACE_REMOTE_SERVICES_V2_CORE], BAD_CAST "UpdateToken",           BAD_CAST mut);

    xmlDocDumpMemory (doc, &s, &size);

    if (doc)
        xmlFreeDoc (doc);

    return s;
}

/******************************************************************************/

GArray *
mui_remote_service_utils_parse_get_key_load_command_response (const gchar  *str,
                                                              GError      **error)
{
    GArray          *commands  = NULL;
    xmlDoc          *doc       = NULL;
    xmlXPathContext *xpath_ctx = NULL;
    xmlXPathObject  *xpath_obj = NULL;

    doc = xmlReadMemory (str, strlen (str), "noname.xml", NULL, 0);
    if (!doc) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read response into XML memory");
        goto out;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (!xpath_register_namespaces (xpath_ctx, error))
        goto out;

    xpath_obj = xmlXPathEvalExpression (BAD_CAST
                                        "/senv:Envelope"
                                        "/senv:Body"
                                        "/rs2:GetKeyLoadCommandResponse"
                                        "/rs2:GetKeyLoadCommandResult"
                                        "/rs2c:Commands"
                                        "/rs2c:Command",
                                        xpath_ctx);
    if (!xpath_obj) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't evaluate XPATH expression");
        goto out;
    }

    commands = common_command_info_build_array (xpath_obj, error);

out:
    if (xpath_obj)
        xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx)
        xmlXPathFreeContext (xpath_ctx);
    if (doc)
        xmlFreeDoc (doc);

    return commands;
}

/******************************************************************************/

xmlChar *
mui_remote_service_utils_build_get_command_list_request (const gchar *customer_code,
                                                         const gchar *username,
                                                         const gchar *password,
                                                         const gchar *billing_label,
                                                         const gchar *customer_transaction_id)
{
    return common_list_request_build ("GetCommandList",
                                      customer_code,
                                      username,
                                      password,
                                      billing_label,
                                      customer_transaction_id,
                                      "ALL", /* execution type */
                                      NULL,  /* command id */
                                      NULL,  /* ksn */
                                      NULL,  /* key id */
                                      NULL); /* mut */
}

/* MuiCommandInfo */
GArray *
mui_remote_service_utils_parse_get_command_list_response (const gchar  *str,
                                                          GError      **error)
{
    GArray          *commands  = NULL;
    xmlDoc          *doc       = NULL;
    xmlXPathContext *xpath_ctx = NULL;
    xmlXPathObject  *xpath_obj = NULL;

    doc = xmlReadMemory (str, strlen (str), "noname.xml", NULL, 0);
    if (!doc) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read response into XML memory");
        goto out;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (!xpath_register_namespaces (xpath_ctx, error))
        goto out;

    xpath_obj = xmlXPathEvalExpression (BAD_CAST
                                        "/senv:Envelope"
                                        "/senv:Body"
                                        "/rs2:GetCommandListResponse"
                                        "/rs2:GetCommandListResult"
                                        "/rs2c:Commands"
                                        "/rs2c:Command",
                                        xpath_ctx);
    if (!xpath_obj) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't evaluate XPATH expression");
        goto out;
    }

    commands = common_command_info_build_array (xpath_obj, error);

out:
    if (xpath_obj)
        xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx)
        xmlXPathFreeContext (xpath_ctx);
    if (doc)
        xmlFreeDoc (doc);

    return commands;
}

/******************************************************************************/

xmlChar *
mui_remote_service_utils_build_get_command_by_ksn_request (const gchar *customer_code,
                                                           const gchar *username,
                                                           const gchar *password,
                                                           const gchar *billing_label,
                                                           const gchar *customer_transaction_id,
                                                           const gchar *command_id,
                                                           const gchar *ksn,
                                                           const gchar *key_id)
{
    return common_list_request_build ("GetCommandByKSN",
                                      customer_code,
                                      username,
                                      password,
                                      billing_label,
                                      customer_transaction_id,
                                      NULL, /* execution type */
                                      command_id,
                                      ksn,
                                      key_id,
                                      NULL); /* mut */
}

/* MuiCommandInfo */
GArray *
mui_remote_service_utils_parse_get_command_by_ksn_response (const gchar  *str,
                                                            GError      **error)
{
    GArray          *commands  = NULL;
    xmlDoc          *doc       = NULL;
    xmlXPathContext *xpath_ctx = NULL;
    xmlXPathObject  *xpath_obj = NULL;

    doc = xmlReadMemory (str, strlen (str), "noname.xml", NULL, 0);
    if (!doc) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read response into XML memory");
        goto out;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (!xpath_register_namespaces (xpath_ctx, error))
        goto out;

    xpath_obj = xmlXPathEvalExpression (BAD_CAST
                                        "/senv:Envelope"
                                        "/senv:Body"
                                        "/rs2:GetCommandByKSNResponse"
                                        "/rs2:GetCommandByKSNResult"
                                        "/rs2c:Command",
                                        xpath_ctx);
    if (!xpath_obj) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't evaluate XPATH expression");
        goto out;
    }

    commands = common_command_info_build_array (xpath_obj, error);

out:
    if (xpath_obj)
        xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx)
        xmlXPathFreeContext (xpath_ctx);
    if (doc)
        xmlFreeDoc (doc);

    return commands;
}

/******************************************************************************/

xmlChar *
mui_remote_service_utils_build_get_command_by_mut_request (const gchar *customer_code,
                                                           const gchar *username,
                                                           const gchar *password,
                                                           const gchar *billing_label,
                                                           const gchar *customer_transaction_id,
                                                           const gchar *command_id,
                                                           const gchar *ksn,
                                                           const gchar *mut)
{
    return common_list_request_build ("GetCommandByMUT",
                                      customer_code,
                                      username,
                                      password,
                                      billing_label,
                                      customer_transaction_id,
                                      NULL, /* execution type */
                                      command_id,
                                      ksn,
                                      NULL, /* key id */
                                      mut);
}

/* MuiCommandInfo */
GArray *
mui_remote_service_utils_parse_get_command_by_mut_response (const gchar  *str,
                                                            GError      **error)
{
    GArray          *commands  = NULL;
    xmlDoc          *doc       = NULL;
    xmlXPathContext *xpath_ctx = NULL;
    xmlXPathObject  *xpath_obj = NULL;

    doc = xmlReadMemory (str, strlen (str), "noname.xml", NULL, 0);
    if (!doc) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't read response into XML memory");
        goto out;
    }

    xpath_ctx = xmlXPathNewContext (doc);
    if (!xpath_register_namespaces (xpath_ctx, error))
        goto out;

    xpath_obj = xmlXPathEvalExpression (BAD_CAST
                                        "/senv:Envelope"
                                        "/senv:Body"
                                        "/rs2:GetCommandByMUTResponse"
                                        "/rs2:GetCommandByMUTResult"
                                        "/rs2c:Command",
                                        xpath_ctx);
    if (!xpath_obj) {
        g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Couldn't evaluate XPATH expression");
        goto out;
    }

    commands = common_command_info_build_array (xpath_obj, error);

out:
    if (xpath_obj)
        xmlXPathFreeObject (xpath_obj);
    if (xpath_ctx)
        xmlXPathFreeContext (xpath_ctx);
    if (doc)
        xmlFreeDoc (doc);

    return commands;
}
