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

#include <stdio.h>
#include <stdlib.h>

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gprintf.h>

#include <mccr.h>

#include "mui-app.h"

#define PROGRAM_NAME    "mccr-ui"
#define PROGRAM_VERSION PACKAGE_VERSION

/******************************************************************************/
/* Context */

static gboolean  version_flag;
static gchar    *log_level_str;

static const GOptionEntry entries[] = {
    {
        "log-level", 0, 0, G_OPTION_ARG_STRING, &log_level_str,
        "Log level: one of [error, warning, info, debug]",
        "[LEVEL]"
    },
    {
        "version", 'v', 0, G_OPTION_ARG_NONE, &version_flag,
        "Print version",
        NULL
    },
    { NULL }
};

static void
print_version (void)
{
    g_print ("\n"
             PROGRAM_NAME " " PROGRAM_VERSION "\n"
             "Copyright (2014-2015) Zodiac Inflight Innovations\n"
             "\n");
}

/******************************************************************************/
/* Logging */

static GLogLevelFlags default_log_level = (G_LOG_LEVEL_ERROR    |
                                           G_LOG_LEVEL_CRITICAL |
                                           G_LOG_LEVEL_WARNING);

static void
log_handler (const gchar    *log_domain,
             GLogLevelFlags  log_level,
             const gchar    *message,
             gpointer        user_data)
{
    const gchar *log_level_prefix;
    gboolean err = FALSE;

    if (!(default_log_level & log_level))
        return;

    switch (log_level) {
    case G_LOG_LEVEL_CRITICAL:
    case G_LOG_FLAG_FATAL:
    case G_LOG_LEVEL_ERROR:
        log_level_prefix = "-Error **";
        err = TRUE;
        break;

    case G_LOG_LEVEL_WARNING:
        log_level_prefix = "-Warning **";
        err = TRUE;
        break;

    case G_LOG_LEVEL_DEBUG:
        log_level_prefix = "[Debug]";
        break;

    default:
        log_level_prefix = "";
        break;
    }

    g_fprintf (err ? stderr : stdout,
               "[%s] %s\n",
               log_level_prefix,
               message);
}

static pthread_t main_tid;

static void
mccr_log_handler (pthread_t    thread_id,
                       const gchar *message)
{
    flockfile (stdout);
    if (thread_id == main_tid)
        g_debug ("[mccr] %s", message);
    else
        g_debug ("[mccr,%u] %s", (unsigned int) thread_id, message);
    funlockfile (stdout);
}

static gboolean
log_setup (void)
{
    /* Use user-provided log level if given */
    if (log_level_str) {
        if (g_ascii_strcasecmp (log_level_str, "error") == 0)
            default_log_level = (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL);
        else if (g_ascii_strcasecmp (log_level_str, "warning") == 0)
            default_log_level = (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
        else if (g_ascii_strcasecmp (log_level_str, "info") == 0)
            default_log_level = (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO);
        else if (g_ascii_strcasecmp (log_level_str, "debug") == 0)
            default_log_level = (G_LOG_LEVEL_ERROR | G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO | G_LOG_LEVEL_DEBUG);
        else if (g_ascii_strcasecmp (log_level_str, "none") == 0)
            default_log_level = 0;
        else {
            g_printerr ("error: invalid log level string given: '%s'", log_level_str);
            return FALSE;
        }
    }

    g_log_set_handler (NULL,
                       G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL | G_LOG_FLAG_RECURSION,
                       log_handler,
                       NULL);

    if (default_log_level & G_LOG_LEVEL_DEBUG) {
        main_tid = pthread_self ();
        mccr_log_set_handler (mccr_log_handler);
    }

    return TRUE;
}

/******************************************************************************/

int main (int argc, char **argv)
{
    MuiApp             *app;
    gint                run_status;
    GError             *error = NULL;
    mccr_status_t  st;

    if (!gtk_init_with_args (&argc, &argv, NULL, entries, NULL, &error)) {
        g_printerr ("%s\n", error->message);
        exit (EXIT_FAILURE);
    }

    if (version_flag) {
        print_version ();
        exit (EXIT_SUCCESS);
    }

    if (!log_setup ())
        exit (EXIT_FAILURE);

    if ((st = mccr_init ()) != MCCR_STATUS_OK) {
        g_printerr ("mccr initialization failed: %s\n", mccr_status_to_string (st));
        exit (EXIT_FAILURE);
    }
    g_debug ("mccr initialized");

    app = mui_app_new ();
    g_application_set_default (G_APPLICATION (app));
    run_status = g_application_run (G_APPLICATION (app), argc, argv);
    g_object_unref (app);

    mccr_exit ();

    return run_status;
}
