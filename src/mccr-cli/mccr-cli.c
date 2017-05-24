/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * mccr-cli - Command line tool to manage MagTek Credit Card Readers
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
#include <getopt.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>

#include <common.h>

#include <mccr.h>

#define PROGRAM_NAME    "mccr-cli"
#define PROGRAM_VERSION PACKAGE_VERSION

/******************************************************************************/
/* Action: list */

static int
run_list (void)
{
    mccr_device_t **devices;
    unsigned int         i;

    /* Enumerate devices */
    devices = mccr_enumerate_devices ();
    if (!devices) {
        printf ("no devices found\n");
        return EXIT_SUCCESS;
    }

    /* Print device info */
    for (i = 0; devices[i]; i++)
        printf ("[%u] device (%04x:%04x) at path '%s'\n",
                i,
                mccr_device_get_vid  (devices[i]),
                mccr_device_get_pid  (devices[i]),
                mccr_device_get_path (devices[i]));

    /* Cleanup devices array */
    for (i = 0; devices[i]; i++)
        mccr_device_unref (devices[i]);
    free (devices);

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* Action: show */

static int
run_show (mccr_device_t *device)
{
    mccr_status_t  st;
    char          *str;
    uint8_t        val;
    uint32_t       val32;
    uint8_t       *array;
    size_t         array_size;

    printf ("device (%04x:%04x) at path '%s'\n",
            mccr_device_get_vid  (device),
            mccr_device_get_pid  (device),
            mccr_device_get_path (device));

#define VALIDATE_UNKNOWN_W(wstr) (wstr ? wstr : L"unknown")
    printf ("\tserial:       %ls\n", VALIDATE_UNKNOWN_W (mccr_device_get_serial_number (device)));
    printf ("\tmanufacturer: %ls\n", VALIDATE_UNKNOWN_W (mccr_device_get_manufacturer (device)));
    printf ("\tproduct:      %ls\n", VALIDATE_UNKNOWN_W (mccr_device_get_product (device)));
#undef VALIDATE_UNKNOWN_W

    printf ("\t----------------------------------------------------\n");
    printf ("\tproperties:\n");

    if ((st = mccr_device_read_software_id (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tsoftware id:              %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read software id: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_usb_serial_number (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tusb serial number:        %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read usb serial number: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_polling_interval (device, &val)) == MCCR_STATUS_OK)
        printf ("\t\tpolling interval:         %u ms\n", val);
    else
        fprintf (stderr, "error: cannot read polling interval: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_device_serial_number (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tdevice serial number:     %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read device serial number: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_magnesafe_version_number (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tmagnesafe version number: %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read magnesafe version number: %s\n", mccr_status_to_string (st));

    {
        bool               aamva_supported;
        mccr_track_state_t track_1;
        mccr_track_state_t track_2;
        mccr_track_state_t track_3;

        if ((st = mccr_device_read_track_id_enable (device, &aamva_supported, &track_1, &track_2, &track_3)) == MCCR_STATUS_OK) {
            printf ("\t\tsupported card types:     %s\n", aamva_supported ? "ISO and AAMVA" : "ISO only");
            printf ("\t\ttrack 1 status:           %s\n", mccr_track_state_to_string (track_1));
            printf ("\t\ttrack 2 status:           %s\n", mccr_track_state_to_string (track_2));
            printf ("\t\ttrack 3 status:           %s\n", mccr_track_state_to_string (track_3));
        } else
            fprintf (stderr, "error: cannot read track id enable fields: %s\n", mccr_status_to_string (st));
    }

    if ((st = mccr_device_read_iso_track_mask (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tISO track mask:           %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read ISO track mask: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_aamva_track_mask (device, &str)) == MCCR_STATUS_OK) {
        printf ("\t\tAAMVA track mask:         %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot read AAMVA track mask: %s\n", mccr_status_to_string (st));

    if ((st = mccr_device_read_max_packet_size (device, &val)) == MCCR_STATUS_OK)
        printf ("\t\tmax packet size:          %u bytes\n", val);
    else
        fprintf (stderr, "error: cannot read max packet size: %s\n", mccr_status_to_string (st));

    printf ("\t----------------------------------------------------\n");

    if ((st = mccr_device_get_dukpt_ksn_and_counter (device, &array, &array_size)) == MCCR_STATUS_OK) {
        char *hex;

        hex = strhex (array, array_size, ":");
        printf ("\tDUKPT KSN and counter: %s\n", hex);
        free (hex);
        free (array);
    } else
        fprintf (stderr, "error: cannot get DUKPT KSN and counter: %s\n", mccr_status_to_string (st));

    printf ("\t----------------------------------------------------\n");

    {
        mccr_reader_state_t            state;
        mccr_reader_state_antecedent_t antecedent;

        if ((st = mccr_device_get_reader_state (device, &state, &antecedent)) == MCCR_STATUS_OK) {
            printf ("\treader state: %s\n", mccr_reader_state_to_string (state));
            printf ("\tantecedent:   %s\n", mccr_reader_state_antecedent_to_string (antecedent));
        } else
            fprintf (stderr, "error: cannot get reader state: %s\n", mccr_status_to_string (st));
    }

    printf ("\t----------------------------------------------------\n");

    {
        mccr_security_level_t level;

        if ((st = mccr_device_get_security_level (device, &level)) == MCCR_STATUS_OK)
            printf ("\tsecurity level: %u\n", (unsigned int) level);
        else
            fprintf (stderr, "error: cannot get security level: %s\n", mccr_status_to_string (st));
    }

    printf ("\t----------------------------------------------------\n");

    if ((st = mccr_device_get_encryption_counter (device, &str, &val32)) == MCCR_STATUS_OK) {
        if (val32 == MCCR_ENCRYPTION_COUNTER_DISABLED)
            printf ("\tencryption counter:   disabled\n");
        else if (val32 == MCCR_ENCRYPTION_COUNTER_EXPIRED)
            printf ("\tencryption counter:   expired\n");
        else if (val32 >= MCCR_ENCRYPTION_COUNTER_MIN && val32 <= MCCR_ENCRYPTION_COUNTER_MAX)
            printf ("\tencryption counter:   %u\n", (unsigned int) val32);
        else
            printf ("\tencryption counter:   unexpected value: %u\n", (unsigned int) val32);
        printf ("\tdevice serial number: %s\n", str);
        free (str);
    } else
        fprintf (stderr, "error: cannot get encryption counter: %s\n", mccr_status_to_string (st));

    printf ("\t----------------------------------------------------\n");

    if ((st = mccr_device_get_magtek_update_token (device, &array, &array_size)) == MCCR_STATUS_OK) {
        char *hex;

        hex = strhex_multiline (array, array_size, 10, "\t                     ", ":");
        printf ("\tMagtek Update Token: %s\n", hex);
        free (hex);
        free (array);
    } else
        fprintf (stderr, "error: cannot get Magtek Update Token: %s\n", mccr_status_to_string (st));

    return EXIT_SUCCESS;
}

/******************************************************************************/
/* Action: reset */

static int
run_reset (mccr_device_t *device)
{
    mccr_status_t st;

    st = mccr_device_reset (device);
    if (st != MCCR_STATUS_OK) {
        fprintf (stderr, "error: cannot reset device: %s\n", mccr_status_to_string (st));
        return EXIT_FAILURE;
    }

    printf ("requested device reset\n");
    return EXIT_SUCCESS;
}

/******************************************************************************/
/* Action: set session id */

static int
run_set_session_id (mccr_device_t *device,
                    const char    *session_id_str)
{
    mccr_status_t st;
    unsigned long long value;

    value = strtoull (session_id_str, NULL, 16);
    if (!value) {
        fprintf (stderr, "error: invalid session id value: %s\n", session_id_str);
        return EXIT_FAILURE;
    }

    st = mccr_device_set_session_id (device, (uint64_t) value);
    if (st != MCCR_STATUS_OK) {
        fprintf (stderr, "error: cannot set session id: %s\n", mccr_status_to_string (st));
        return EXIT_FAILURE;
    }

    printf ("session id set\n");
    return EXIT_SUCCESS;
}

/******************************************************************************/
/* Action: wait swipe */

#define PROCESS_TRACK(N)                                                                                                         \
    static void                                                                                                                  \
    process_track_##N (mccr_swipe_report_t *report,                                                                              \
                       bool                      ascii)                                                                          \
    {                                                                                                                            \
        mccr_status_t  st;                                                                                                       \
        uint8_t        status;                                                                                                   \
        const uint8_t *data;                                                                                                     \
        uint8_t        length;                                                                                                   \
        char          *aux;                                                                                                      \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_decode_status (report, &status)) != MCCR_STATUS_OK) {                        \
            fprintf (stderr, "error: cannot get track %u decode status: %s\n", N, mccr_status_to_string (st));                   \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        if (status == MCCR_SWIPE_TRACK_DECODE_STATUS_SUCCESS)                                                                    \
            printf ("track %u decoding: success\n", N);                                                                          \
        else if (status & MCCR_SWIPE_TRACK_DECODE_STATUS_ERROR)                                                                  \
            printf ("track %u decoding: error\n", N);                                                                            \
        else                                                                                                                     \
            printf ("track %u decoding: unknown status\n", N);                                                                   \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_encrypted_data_length (report, &length)) != MCCR_STATUS_OK) {                \
            fprintf (stderr, "error: cannot get track %u data length: %s\n", N, mccr_status_to_string (st));                     \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_encrypted_data (report, &data)) != MCCR_STATUS_OK) {                         \
            fprintf (stderr, "error: cannot get track %u data: %s\n", N, mccr_status_to_string (st));                            \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        printf ("\tdata length:          %u bytes\n", length);                                                                   \
        if (length) {                                                                                                            \
            aux = strhex (data, length, ":");                                                                                    \
            printf ("\tdata:                 %s\n", aux);                                                                        \
            free (aux);                                                                                                          \
        }                                                                                                                        \
                                                                                                                                 \
        if (length && ascii) {                                                                                                   \
            aux = strascii (data, length);                                                                                       \
            printf ("\tascii:                %s\n", aux);                                                                        \
            free (aux);                                                                                                          \
        }                                                                                                                        \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_absolute_data_length (report, &length)) != MCCR_STATUS_OK)                   \
            printf ("\tabsolute data length: unknown\n");                                                                        \
        else                                                                                                                     \
            printf ("\tabsolute data length: %u bytes\n", length);                                                               \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_masked_data_length (report, &length)) != MCCR_STATUS_OK) {                   \
            fprintf (stderr, "error: cannot get track %u masked data length: %s\n", N, mccr_status_to_string (st));              \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        if ((st = mccr_swipe_report_get_track_##N##_masked_data (report, &data)) != MCCR_STATUS_OK) {                            \
            fprintf (stderr, "error: cannot get track %u masked data: %s\n", N, mccr_status_to_string (st));                     \
            return;                                                                                                              \
        }                                                                                                                        \
                                                                                                                                 \
        printf ("\tmasked data length:   %u bytes\n", length);                                                                   \
        if (length) {                                                                                                            \
            aux = strascii (data, length);                                                                                       \
            printf ("\tmasked data:          %s\n", aux);                                                                        \
            free (aux);                                                                                                          \
        }                                                                                                                        \
    }

PROCESS_TRACK(1)
PROCESS_TRACK(2)
PROCESS_TRACK(3)

static int
run_wait_swipe (mccr_device_t *device,
                bool                ascii)
{
    mccr_status_t            st;
    mccr_swipe_report_t     *report;
    mccr_card_encode_type_t  card_encode_type;

    st = mccr_device_wait_swipe_report (device, -1, &report);
    if (st != MCCR_STATUS_OK) {
        fprintf (stderr, "error: cannot get swipe report: %s\n", mccr_status_to_string (st));
        return EXIT_FAILURE;
    }

    printf ("swipe detected\n");

    st = mccr_swipe_report_get_card_encode_type (report, &card_encode_type);
    if (st != MCCR_STATUS_OK)
        fprintf (stderr, "warning: cannot get card encode type: %s\n", mccr_status_to_string (st));
    else
        printf ("card encode type: %s\n", mccr_card_encode_type_to_string (card_encode_type));

    process_track_1 (report, ascii);
    process_track_2 (report, ascii);
    process_track_3 (report, ascii);

    mccr_swipe_report_free (report);
    return EXIT_SUCCESS;
}

/******************************************************************************/
/* Logging */

static pthread_t main_tid;

static void
log_handler (pthread_t   thread_id,
             const char *message)
{
    flockfile (stdout);
    if (thread_id == main_tid)
        fprintf (stdout, "[mccr] %s\n", message);
    else
        fprintf (stdout, "[mccr,%u] %s\n", (unsigned int) thread_id, message);
    funlockfile (stdout);
}

/******************************************************************************/

static void
print_help (void)
{
    printf ("\n"
            "Usage: " PROGRAM_NAME " <option>\n"
            "\n"
            "Global options:\n"
            "  -l, --list                  Enumerate all devices.\n"
            "\n"
            "Device selection:\n"
            "  -f, --first                 Select the first device found.\n"
            "  -p, --path                  Select device at given path.\n"
            "\n"
            "Device options:\n"
            "  -s, --show                  Show information of a given device.\n"
            "  -r, --reset                 Reset (power cycle) device.\n"
            "  -I, --set-session-id=[H64]  Set session id.\n"
            "  -w, --wait-swipe            Wait for a credit card swipe.\n"
            "  -a, --ascii                 Try to decode ASCII in data from swipe reports.\n"
            "\n"
            "Common options:\n"
            "  -d, --debug                 Enable verbose logging.\n"
            "  -h, --help                  Show help.\n"
            "  -v, --version               Show version.\n"
            "\n"
            "Notes:\n"
            "  * [H64] is a 64bit unsigned number, in hexadecimal format (with or without 0x prefix).\n"
            "\n"
            "Examples:\n"
            "   $ " PROGRAM_NAME " --first --show\n"
            "   $ " PROGRAM_NAME " --first --set-session-id=01234abc\n"
            "   $ " PROGRAM_NAME " --first --wait-swipe\n"
            "\n");
}

static void
print_version (void)
{
    printf ("\n"
            PROGRAM_NAME " " PROGRAM_VERSION "\n");
    printf ("  Built against libmccr %u.%u.%u\n", MCCR_MAJOR_VERSION, MCCR_MINOR_VERSION, MCCR_MICRO_VERSION);
    printf ("  Running with libmmcr %u.%u.%u\n", mccr_get_major_version (), mccr_get_minor_version (), mccr_get_micro_version ());
    printf ("Copyright (2016-2017) Zodiac Inflight Innovations\n"
            "\n");
}

int main (int argc, char **argv)
{
    int                 idx, iarg = 0;
    unsigned int        n_global_actions, n_device_actions, n_actions;
    bool                action_list = false;
    bool                action_show = false;
    bool                action_reset = false;
    char               *action_set_session_id = NULL;
    bool                action_wait_swipe = false;
    bool                ascii = false;
    bool                debug = false;
    bool                first = false;
    char               *path = NULL;
    mccr_device_t *device = NULL;
    mccr_status_t  st;
    unsigned int        ret;

    const struct option longopts[] = {
        { "list",                 no_argument,       0, 'l' },
        { "first",                no_argument,       0, 'f' },
        { "path",                 required_argument, 0, 'p' },
        { "show",                 no_argument,       0, 's' },
        { "reset",                no_argument,       0, 'r' },
        { "set-session-id",       required_argument, 0, 'I' },
        { "wait-swipe",           no_argument,       0, 'w' },
        { "ascii",                no_argument,       0, 'a' },
        { "debug",                no_argument,       0, 'd' },
        { "version",              no_argument,       0, 'v' },
        { "help",                 no_argument,       0, 'h' },
        { 0,                      0,                 0, 0   },
    };

    /* turn off getopt error message */
    opterr = 1;
    while (iarg != -1) {
        iarg = getopt_long (argc, argv, "lfp:srI:wadvh", longopts, &idx);
        switch (iarg) {
        case 'l':
            action_list = true;
            break;
        case 'f':
            first = true;
            break;
        case 'p':
            if (path)
                fprintf (stderr, "warning: --path given multiple times\n");
            else
                path = strdup (optarg);
            break;
        case 's':
            action_show = true;
            break;
        case 'r':
            action_reset = true;
            break;
        case 'I':
            if (action_set_session_id)
                fprintf (stderr, "warning: --set-session-id given multiple times\n");
            else
                action_set_session_id = strdup (optarg);
            break;
        case 'w':
            action_wait_swipe = true;
            break;
        case 'a':
            ascii = true;
            break;
        case 'd':
            debug = true;
            break;
        case 'h':
            print_help ();
            return 0;
        case 'v':
            print_version ();
            return 0;
        }
    }

    /* Allow only one action at a time */
    n_global_actions = (action_list);
    n_device_actions = (action_show +
                        action_reset +
                        !!action_set_session_id +
                        action_wait_swipe);
    n_actions = (n_global_actions + n_device_actions);
    if (n_actions > 1) {
        fprintf (stderr, "error: too many actions requested\n");
        return EXIT_FAILURE;
    }
    if (n_actions == 0) {
        fprintf (stderr, "error: no actions requested\n");
        return EXIT_FAILURE;
    }

    /* Warn if options not built properly */
    if (ascii && !action_wait_swipe)
        fprintf (stderr, "warning: --ascii only applies when --wait-swipe action is requested");

    /* Allow only one device selection at a time */
    if (path && first) {
        fprintf (stderr, "error: multiple device selection operations requested\n");
        return EXIT_FAILURE;
    }

    /* Setup library logging */
    if (debug) {
        main_tid = pthread_self ();
        mccr_log_set_handler (log_handler);
    }

    /* Initialize */
    st = mccr_init ();
    if (st != MCCR_STATUS_OK) {
        fprintf (stderr, "error: mccr library initialization failed: %s\n", mccr_status_to_string (st));
        return EXIT_FAILURE;
    }

    /* Some actions require a device to be specified */
    if (n_device_actions) {
        if (!path && !first) {
            fprintf (stderr, "error: operation requires a device to be specified\n");
            return EXIT_FAILURE;
        }
        /* Try to create device for the given path (may be NULL if first requested) */
        device = mccr_device_new (path);
        if (!device) {
            if (path)
                fprintf (stderr, "error: couldn't find device at path '%s'\n", path);
            else
                fprintf (stderr, "error: no device found\n");
            return EXIT_FAILURE;
        }

        /* For all actions except for --show, open the device */
        st = mccr_device_open (device);
        if (st != MCCR_STATUS_OK) {
            fprintf (stderr, "error: mccr device open failed: %s\n", mccr_status_to_string (st));
            return EXIT_FAILURE;
        }
    }

    /* Run action */
    if (action_list)
        ret = run_list ();
    else if (action_show)
        ret = run_show (device);
    else if (action_reset)
        ret = run_reset (device);
    else if (action_set_session_id)
        ret = run_set_session_id (device, action_set_session_id);
    else if (action_wait_swipe)
        ret = run_wait_swipe (device, ascii);
    else
        assert (0);

    /* Clean exit */
    if (device) {
        mccr_device_close (device);
        mccr_device_unref (device);
    }
    mccr_exit ();

    return ret;
}
