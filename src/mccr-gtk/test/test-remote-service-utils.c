
/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2014 Zodiac Inflight Innovations, Inc.
 * All rights reserved.
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <config.h>
#include <glib.h>

#include "mui-remote-service-utils.h"

/******************************************************************************/

static void
common_cmp_command_list (GArray               *command_list,
                         const MuiCommandInfo *expected_command_list,
                         gsize                 expected_command_list_size)
{
    guint   i;

    for (i = 0; i < expected_command_list_size; i++) {
        guint j;

        for (j = 0; j < command_list->len; j++) {
            MuiCommandInfo *command_info;

            command_info = &g_array_index (command_list, MuiCommandInfo, j);
            if (xmlStrcmp (expected_command_list[i].name, command_info->name) == 0) {
                g_assert_cmpstr ((const gchar *) expected_command_list[i].command_type,   ==, (const gchar *) command_info->command_type);
                g_assert_cmpstr ((const gchar *) expected_command_list[i].description,    ==, (const gchar *) command_info->description);
                g_assert_cmpstr ((const gchar *) expected_command_list[i].execution_type, ==, (const gchar *) command_info->execution_type);
                g_assert_cmpstr ((const gchar *) expected_command_list[i].id,             ==, (const gchar *) command_info->id);
                g_assert_cmpstr ((const gchar *) expected_command_list[i].value,          ==, (const gchar *) command_info->value);
                break;
            }
        }

        g_assert (j != command_list->len);
    }

    g_assert_cmpuint (command_list->len, ==, expected_command_list_size);
}

/******************************************************************************/

static MuiKeyInfo expected_key_info[] = {
    {
        .id                   = BAD_CAST "-1",
        .key_name             = BAD_CAST "Current Key",
        .description          = BAD_CAST "Currently Loaded Key",
        .ksi                  = BAD_CAST "0000000",
        .key_slot_name_prefix = BAD_CAST "Prod",
    },
    {
        .id                   = BAD_CAST "1",
        .key_name             = BAD_CAST "Default Magensa Key",
        .description          = BAD_CAST "Default Magensa Key",
        .ksi                  = BAD_CAST "9011880",
        .key_slot_name_prefix = BAD_CAST "Prod",
    },
    {
        .id                   = BAD_CAST "5",
        .key_name             = BAD_CAST "AABBCC Key",
        .description          = BAD_CAST "Some Other Test Key",
        .ksi                  = BAD_CAST "1234567",
        .key_slot_name_prefix = BAD_CAST "Prod",
    },
};

static void
test_get_key_list_response (void)
{
    gchar  *response = NULL;
    gchar  *path;
    GArray *key_info_array;
    guint   i;
    GError *error = NULL;

    path = g_test_build_filename (G_TEST_DIST, "get-key-list-response.xml", NULL);
    g_assert (g_file_get_contents (path, &response, NULL, NULL));
    g_free (path);

    key_info_array = mui_remote_service_utils_parse_get_key_list_response (response, &error);

    g_assert_no_error (error);
    g_assert (key_info_array);

    for (i = 0; i < G_N_ELEMENTS (expected_key_info); i++) {
        guint j;

        for (j = 0; j < key_info_array->len; j++) {
            MuiKeyInfo *key_info;

            key_info = &g_array_index (key_info_array, MuiKeyInfo, j);
            if (xmlStrcmp (expected_key_info[i].id, key_info->id) == 0) {
                g_assert_cmpstr ((const gchar *) expected_key_info[i].key_name,             ==, (const gchar *) key_info->key_name);
                g_assert_cmpstr ((const gchar *) expected_key_info[i].description,          ==, (const gchar *) key_info->description);
                g_assert_cmpstr ((const gchar *) expected_key_info[i].ksi,                  ==, (const gchar *) key_info->ksi);
                g_assert_cmpstr ((const gchar *) expected_key_info[i].key_slot_name_prefix, ==, (const gchar *) key_info->key_slot_name_prefix);
                break;
            }
        }

        g_assert (j != key_info_array->len);
    }

    g_assert_cmpuint (key_info_array->len, ==, G_N_ELEMENTS (expected_key_info));

    g_free (response);
}

/******************************************************************************/

static MuiCommandInfo expected_command_info[] = {
    {
        .id                   = BAD_CAST "0",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "To SCRA Device",
        .execution_type       = BAD_CAST "ALL",
        .name                 = BAD_CAST "ChangeKey",
        .value                = BAD_CAST "223142333933373837303231333137414100019011880B393787000000EB31758E0DF16C9DDFFB11FA5F245406B1180A9C2759",
    },
    {
        .id                   = BAD_CAST "0",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "Key Check Value",
        .execution_type       = BAD_CAST "ALL",
        .name                 = BAD_CAST "KCV",
        .value                = BAD_CAST "1D1DF857B3",
    },
};

static void
test_get_key_load_command_response (void)
{
    gchar  *response = NULL;
    gchar  *path;
    GArray *command_info_array;
    GError *error = NULL;

    path = g_test_build_filename (G_TEST_DIST, "get-key-load-command-response.xml", NULL);
    g_assert (g_file_get_contents (path, &response, NULL, NULL));
    g_free (path);

    command_info_array = mui_remote_service_utils_parse_get_key_load_command_response (response, &error);

    g_assert_no_error (error);
    g_assert (command_info_array);

    common_cmp_command_list (command_info_array, expected_command_info, G_N_ELEMENTS (expected_command_info));

    g_array_unref (command_info_array);
    g_free (response);
}

/******************************************************************************/

static MuiCommandInfo expected_command_list[] = {
    {
        .id                   = BAD_CAST "34",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "Encrypt using Data Variant",
        .execution_type       = BAD_CAST "MUT",
        .name                 = BAD_CAST "ENC_VARIANT_DATA",
        .value                = BAD_CAST "015401",
    },
    {
        .id                   = BAD_CAST "33",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "Encrypt using PIN Variant",
        .execution_type       = BAD_CAST "MUT",
        .name                 = BAD_CAST "ENC_VARIANT_PIN",
        .value                = BAD_CAST "015400",
    },
};

static void
test_get_command_list_response (void)
{
    gchar  *response = NULL;
    gchar  *path;
    GArray *command_info_array;
    GError *error = NULL;

    path = g_test_build_filename (G_TEST_DIST, "get-command-list-response.xml", NULL);
    g_assert (g_file_get_contents (path, &response, NULL, NULL));
    g_free (path);

    command_info_array = mui_remote_service_utils_parse_get_command_list_response (response, &error);

    g_assert_no_error (error);
    g_assert (command_info_array);

    common_cmp_command_list (command_info_array, expected_command_list, G_N_ELEMENTS (expected_command_list));

    g_array_unref (command_info_array);
    g_free (response);
}

/******************************************************************************/

static MuiCommandInfo expected_command_by_ksn[] = {
    {
        .id                   = BAD_CAST "10",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "Masking to 1234********1234",
        .execution_type       = BAD_CAST "KSN",
        .name                 = BAD_CAST "MASK_4*4",
        .value                = BAD_CAST "010B07303430342A4EC62BAD2B",
    },
};

static void
test_get_command_by_ksn_response (void)
{
    gchar  *response = NULL;
    gchar  *path;
    GArray *command_info_array;
    GError *error = NULL;

    path = g_test_build_filename (G_TEST_DIST, "get-command-by-ksn-response.xml", NULL);
    g_assert (g_file_get_contents (path, &response, NULL, NULL));
    g_free (path);

    command_info_array = mui_remote_service_utils_parse_get_command_by_ksn_response (response, &error);

    g_assert_no_error (error);
    g_assert (command_info_array);

    common_cmp_command_list (command_info_array, expected_command_by_ksn, G_N_ELEMENTS (expected_command_by_ksn));

    g_array_unref (command_info_array);
    g_free (response);
}

/******************************************************************************/

static MuiCommandInfo expected_command_by_mut[] = {
    {
        .id                   = BAD_CAST "34",
        .command_type         = BAD_CAST "0",
        .description          = BAD_CAST "Encrypt using Data Variant",
        .execution_type       = BAD_CAST "MUT",
        .name                 = BAD_CAST "ENC_VARIANT_DATA",
        .value                = BAD_CAST "01065401062B01AE",
    },
};

static void
test_get_command_by_mut_response (void)
{
    gchar  *response = NULL;
    gchar  *path;
    GArray *command_info_array;
    GError *error = NULL;

    path = g_test_build_filename (G_TEST_DIST, "get-command-by-mut-response.xml", NULL);
    g_assert (g_file_get_contents (path, &response, NULL, NULL));
    g_free (path);

    command_info_array = mui_remote_service_utils_parse_get_command_by_mut_response (response, &error);

    g_assert_no_error (error);
    g_assert (command_info_array);

    common_cmp_command_list (command_info_array, expected_command_by_mut, G_N_ELEMENTS (expected_command_by_mut));

    g_array_unref (command_info_array);
    g_free (response);
}

/******************************************************************************/

int main (int argc, char **argv)
{
    gint ret;

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/parser/get-key-list-response",         test_get_key_list_response);
    g_test_add_func ("/parser/get-key-load-command-response", test_get_key_load_command_response);
    g_test_add_func ("/parser/get-command-list-response",     test_get_command_list_response);
    g_test_add_func ("/parser/get-command-by-ksn-response",   test_get_command_by_ksn_response);
    g_test_add_func ("/parser/get-command-by-mut-response",   test_get_command_by_mut_response);

    ret = g_test_run ();

    return ret;
}
