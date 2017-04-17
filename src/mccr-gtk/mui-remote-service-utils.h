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

#ifndef MUI_REMOTE_SERVICE_UTILS_H
#define MUI_REMOTE_SERVICE_UTILS_H

#include <glib-object.h>
#include <gio/gio.h>

#include <libxml/xmlstring.h>
#include <libxml/globals.h>

/******************************************************************************/

typedef struct {
    xmlChar *id;
    xmlChar *key_name;
    xmlChar *description;
    xmlChar *ksi;
    xmlChar *key_slot_name_prefix;
} MuiKeyInfo;

typedef struct {
    xmlChar *id;
    xmlChar *command_type;
    xmlChar *description;
    xmlChar *execution_type;
    xmlChar *name;
    xmlChar *value;
} MuiCommandInfo;

/******************************************************************************/

xmlChar *mui_remote_service_utils_build_get_key_list_request (const gchar *customer_code,
                                                              const gchar *username,
                                                              const gchar *password,
                                                              const gchar *billing_label,
                                                              const gchar *customer_transaction_id);

/* MuiKeyInfo */
GArray *mui_remote_service_utils_parse_get_key_list_response (const gchar  *str,
                                                              GError      **error);

/******************************************************************************/

xmlChar *mui_remote_service_utils_build_get_key_load_command_request (const gchar *customer_code,
                                                                      const gchar *username,
                                                                      const gchar *password,
                                                                      const gchar *billing_label,
                                                                      const gchar *customer_transaction_id,
                                                                      const gchar *ksn,
                                                                      const gchar *key_id,
                                                                      const gchar *mut);

/* MuiCommandInfo */
GArray *mui_remote_service_utils_parse_get_key_load_command_response (const gchar  *str,
                                                                      GError      **error);

/******************************************************************************/

xmlChar *mui_remote_service_utils_build_get_command_list_request (const gchar *customer_code,
                                                                  const gchar *username,
                                                                  const gchar *password,
                                                                  const gchar *billing_label,
                                                                  const gchar *customer_transaction_id);

/* MuiCommandInfo */
GArray *mui_remote_service_utils_parse_get_command_list_response (const gchar  *str,
                                                                  GError      **error);

/******************************************************************************/

/* MuiCommandInfo */
GArray *mui_remote_service_utils_parse_get_command_by_ksn_response (const gchar  *str,
                                                                    GError      **error);

xmlChar *mui_remote_service_utils_build_get_command_by_ksn_request (const gchar *customer_code,
                                                                    const gchar *username,
                                                                    const gchar *password,
                                                                    const gchar *billing_label,
                                                                    const gchar *customer_transaction_id,
                                                                    const gchar *command_id,
                                                                    const gchar *ksn,
                                                                    const gchar *key_id);

/******************************************************************************/

/* MuiCommandInfo */
GArray *mui_remote_service_utils_parse_get_command_by_mut_response (const gchar  *str,
                                                                    GError      **error);

xmlChar *mui_remote_service_utils_build_get_command_by_mut_request (const gchar *customer_code,
                                                                    const gchar *username,
                                                                    const gchar *password,
                                                                    const gchar *billing_label,
                                                                    const gchar *customer_transaction_id,
                                                                    const gchar *command_id,
                                                                    const gchar *ksn,
                                                                    const gchar *mut);


#endif /* MUI_REMOTE_SERVICE_UTILS_H */
