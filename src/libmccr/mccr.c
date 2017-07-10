/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * libmccr - Support library for MagTek Credit Card Readers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2017 Zodiac Inflight Innovations
 * Copyright (C) 2017 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <config.h>

#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>

#include <hidapi.h>

#include "common.h"

#include "mccr.h"
#include "mccr-log.h"
#include "mccr-hid.h"
#include "mccr-input-report.h"
#include "mccr-feature-report.h"

#if defined HIDAPI_BACKEND_USB
# include "mccr-usb.h"
#endif

#if defined HIDAPI_BACKEND_RAW
# include "mccr-raw.h"
#endif

#define MAGTEK_VID 0x0801

/******************************************************************************/
/* Status */

static const char *status_str[] = {
    [MCCR_STATUS_OK]                = "success",
    [MCCR_STATUS_FAILED]            = "failed",
    [MCCR_STATUS_NOT_FOUND]         = "not found",
    [MCCR_STATUS_INTERNAL]          = "internal",
    [MCCR_STATUS_NOT_OPEN]          = "device not open",
    [MCCR_STATUS_WRITE_FAILED]      = "write operation failed",
    [MCCR_STATUS_READ_FAILED]       = "read operation failed",
    [MCCR_STATUS_REPORT_FAILED]     = "report operation failed",
    [MCCR_STATUS_DELAYED]           = "operation delayed",
    [MCCR_STATUS_INVALID_OPERATION] = "invalid operation",
    [MCCR_STATUS_INVALID_INPUT]     = "invalid input",
    [MCCR_STATUS_UNEXPECTED_FORMAT] = "unexpected format",
};

const char *
mccr_status_to_string (mccr_status_t st)
{
    return ((st < (sizeof (status_str) / sizeof (status_str[0]))) ? status_str[st] : "unknown");
}

/******************************************************************************/
/* Device enumeration and disposal */

struct mccr_device_s {
#if !defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
    pthread_mutex_t reflock;
#endif
    volatile int      refcount;
    char             *path;
    uint16_t          vid;
    uint16_t          pid;
    wchar_t          *serial_number;
    wchar_t          *manufacturer;
    wchar_t          *product;
    hid_device       *hid;
    mccr_report_descriptor_context_t *desc;
    mccr_feature_report_t            *feature_report;
};

static mccr_device_t *
device_new (struct hid_device_info *hid_info)
{
    mccr_device_t *device;

    assert (hid_info);

    device = calloc (sizeof (struct mccr_device_s), 1);
    if (!device)
        return NULL;

    mccr_log ("new device created:");
    mccr_log ("  path:           %s",     hid_info->path);
    mccr_log ("  vendor id:      0x%04x", hid_info->vendor_id);
    mccr_log ("  product id:     0x%04x", hid_info->product_id);
    mccr_log ("  serial number:  %ls",    hid_info->serial_number);
    mccr_log ("  release number: 0x%04x", hid_info->release_number);
    mccr_log ("  manufacturer:   %ls",    hid_info->manufacturer_string);
    mccr_log ("  product:        %ls",    hid_info->product_string);
    mccr_log ("  interface:      %d",     hid_info->interface_number);

#if !defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
    pthread_mutex_init (&device->reflock, NULL);
#endif

    device->refcount      = 1;
    device->vid           = hid_info->vendor_id;
    device->pid           = hid_info->product_id;
    device->path          = hid_info->path                ? strdup (hid_info->path)                : NULL;
    device->serial_number = hid_info->serial_number       ? wcsdup (hid_info->serial_number)       : NULL;
    device->manufacturer  = hid_info->manufacturer_string ? wcsdup (hid_info->manufacturer_string) : NULL;
    device->product       = hid_info->product_string      ? wcsdup (hid_info->product_string)      : NULL;

    if (!device->path) {
        mccr_device_unref (device);
        return NULL;
    }

    return device;
}

void
mccr_device_unref (mccr_device_t *device)
{
    assert (device);

#if !defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
    pthread_mutex_lock (&device->reflock);
    if (--device->refcount > 0) {
        pthread_mutex_unlock (&device->reflock);
        return;
    }
    pthread_mutex_unlock (&device->reflock);
    pthread_mutex_destroy (&device->reflock);
#else
    if (__sync_fetch_and_sub (&device->refcount, 1) != 1)
        return;
#endif

    assert (!device->hid);
    assert (!device->feature_report);
    assert (!device->desc);

    free (device->path);
    free (device->serial_number);
    free (device->manufacturer);
    free (device->product);
    free (device);
}

mccr_device_t *
mccr_device_ref (mccr_device_t *device)
{
    assert (device);
#if !defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_4
    pthread_mutex_lock (&device->reflock);
    device->refcount++;
    pthread_mutex_unlock (&device->reflock);
#else
    __sync_fetch_and_add (&device->refcount, 1);
#endif
    return device;
}

mccr_device_t **
mccr_enumerate_devices (void)
{
    struct hid_device_info  *devs, *cur_dev;
    mccr_device_t          **devices = NULL;
    unsigned int             n_devices, i;

    /* Enumerate all HID devices with the expected VID */
    devs = hid_enumerate (MAGTEK_VID, 0x0);
    if (!devs) {
        mccr_log ("couldn't enumerate HID devices");
        return NULL;
    }

    /* Count number of devices enumerated */
    for (n_devices = 0, cur_dev = devs; cur_dev; cur_dev = cur_dev->next, n_devices++);
    mccr_log ("found %u devices during enumeration", n_devices);

    if (n_devices == 0)
        goto out;

    /* Create the devices, one per HID info */
    devices = (mccr_device_t **) calloc (n_devices + 1, sizeof (mccr_device_t *));
    if (!devices)
        goto out;

    for (i = 0, cur_dev = devs; cur_dev; cur_dev = cur_dev->next, i++) {
        mccr_log ("device %u/%u at %s", i + 1, n_devices, cur_dev->path);
        devices[i] = device_new (cur_dev);
    }

out:
    hid_free_enumeration (devs);
    return devices;
}

mccr_device_t *
mccr_device_new (const char *path)
{
    struct hid_device_info *devs, *cur_dev;
    mccr_device_t          *device = NULL;

    /* Enumerate all HID devices with the expected VID */
    devs = hid_enumerate (MAGTEK_VID, 0x0);
    if (!devs) {
        mccr_log ("couldn't enumerate HID devices");
        return NULL;
    }

    /* Create the devices, one per HID info */
    for (cur_dev = devs; cur_dev; cur_dev = cur_dev->next) {
        if (!path || strcmp (cur_dev->path, path) == 0) {
            device = device_new (cur_dev);
            break;
        }
    }
    hid_free_enumeration (devs);

    return device;
}

/******************************************************************************/
/* Device info */

const char *
mccr_device_get_path (mccr_device_t *device)
{
    return device->path;
}

uint16_t
mccr_device_get_vid (mccr_device_t *device)
{
    return device->vid;
}

uint16_t
mccr_device_get_pid (mccr_device_t *device)
{
    return device->pid;
}

const wchar_t *
mccr_device_get_serial_number (mccr_device_t *device)
{
    return device->serial_number;
}

const wchar_t *
mccr_device_get_manufacturer (mccr_device_t *device)
{
    return device->manufacturer;
}

const wchar_t *
mccr_device_get_product (mccr_device_t *device)
{
    return device->product;
}

/******************************************************************************/
/* Device open/close */

static void
device_clear_open_info (mccr_device_t *device)
{
    if (device->feature_report) {
        mccr_feature_report_free (device->feature_report);
        device->feature_report = NULL;
    }

    if (device->desc) {
        mccr_report_descriptor_context_unref (device->desc);
        device->desc = NULL;
    }

    if (device->hid) {
        hid_close (device->hid);
        device->hid = NULL;
    }
}

mccr_status_t
mccr_device_open (mccr_device_t *device)
{
    uint8_t            *hid_descriptor = NULL;
    size_t              hid_descriptor_size = 0;
    mccr_status_t  st;

    if (device->desc) {
        /* Every successful operation increases refcount */
        mccr_device_ref (device);
        return MCCR_STATUS_OK;
    }

    if (mccr_read_report_descriptor (device->path,
                                     &hid_descriptor,
                                     &hid_descriptor_size) != MCCR_STATUS_OK) {
        mccr_log ("error: couldn't read hid descriptor");
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    mccr_log_raw ("  report desc:", hid_descriptor, hid_descriptor_size);

    if (mccr_parse_report_descriptor (hid_descriptor,
                                      hid_descriptor_size,
                                      &device->desc) != MCCR_STATUS_OK) {
        mccr_log ("error: couldn't parse hid descriptor");
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    device->hid = hid_open_path (device->path);
    if (!device->hid) {
        mccr_log ("couldn't open device at path '%s'", device->path);
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    device->feature_report = mccr_feature_report_new (device->desc);
    if (!device->feature_report) {
        mccr_log ("couldn't allocate feature report context");
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    mccr_log ("device at path '%s' now open", device->path);

    /* Every successful operation increases refcount */
    mccr_device_ref (device);
    st = MCCR_STATUS_OK;

out:
    if (st != MCCR_STATUS_OK)
        device_clear_open_info (device);
    free (hid_descriptor);
    return st;
}

bool
mccr_device_is_open (mccr_device_t *device)
{
    return !!device->desc;
}

void
mccr_device_close (mccr_device_t *device)
{
    if (!device->desc)
        return;

    device_clear_open_info (device);

    mccr_log ("device at path '%s' now closed", device->path);

    /* Close operation decreases refcount */
    mccr_device_unref (device);
}

/******************************************************************************/
/* Device commands: reset */

mccr_status_t
mccr_device_reset (mccr_device_t *device)
{
    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_RESET_DEVICE, NULL, 0);
    return mccr_feature_report_send_receive (device->feature_report, device->hid);
}

/******************************************************************************/
/* Device commands: get property */

enum {
    PROPERTY_SOFTWARE_ID              = 0x00,
    PROPERTY_USB_SERIAL_NUMBER        = 0x01,
    PROPERTY_POLLING_INTERVAL         = 0x02,
    PROPERTY_DEVICE_SERIAL_NUMBER     = 0x03,
    PROPERTY_MAGNESAFE_VERSION_NUMBER = 0x04,
    PROPERTY_TRACK_ID_ENABLE          = 0x05,
    PROPERTY_ISO_TRACK_MASK           = 0x07,
    PROPERTY_AAMVA_TRACK_MASK         = 0x08,
    PROPERTY_MAX_PACKET_SIZE          = 0x0A,
};

static mccr_status_t
common_device_read_property_string (mccr_device_t  *device,
                                    uint8_t         property_id,
                                    char          **out_str)
{
    mccr_status_t st;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_PROPERTY, &property_id, 1);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    if (out_str) {
        size_t         response_size;
        const uint8_t *response;

        mccr_feature_report_get_response (device->feature_report, &response, &response_size);

        *out_str = calloc (response_size + 1, 1);
        if (!(*out_str))
            return MCCR_STATUS_FAILED;
        memcpy (*out_str, response, response_size);
    }

    return MCCR_STATUS_OK;
}

static mccr_status_t
common_device_read_property_byte (mccr_device_t  *device,
                                  uint8_t         property_id,
                                  uint8_t        *out_val)
{
    mccr_status_t st;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_PROPERTY, &property_id, 1);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    if (out_val) {
        size_t         response_size;
        const uint8_t *response;

        mccr_feature_report_get_response (device->feature_report, &response, &response_size);
        if (response_size == 1)
            *out_val = response[0];
        else
            mccr_log ("warning: unexpected response data size (%u): 1 byte expected", response_size);
    }

    return MCCR_STATUS_OK;
}

mccr_status_t
mccr_device_read_software_id (mccr_device_t  *device,
                              char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_SOFTWARE_ID, out_str);
}

mccr_status_t
mccr_device_read_usb_serial_number (mccr_device_t  *device,
                                    char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_USB_SERIAL_NUMBER, out_str);
}

mccr_status_t
mccr_device_read_polling_interval (mccr_device_t *device,
                                   uint8_t       *out_val)
{
    return common_device_read_property_byte (device, PROPERTY_POLLING_INTERVAL, out_val);
}

mccr_status_t
mccr_device_read_device_serial_number (mccr_device_t  *device,
                                       char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_DEVICE_SERIAL_NUMBER, out_str);
}

mccr_status_t
mccr_device_read_magnesafe_version_number (mccr_device_t  *device,
                                           char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_MAGNESAFE_VERSION_NUMBER, out_str);
}

static const char *track_state_str[] = {
    [MCCR_TRACK_STATE_DISABLED]         = "disabled",
    [MCCR_TRACK_STATE_ENABLED]          = "enabled",
    [MCCR_TRACK_STATE_ENABLED_REQUIRED] = "enabled/required"
};

const char *
mccr_track_state_to_string (mccr_track_state_t st)
{
    return (st < (sizeof (track_state_str) / sizeof (track_state_str[0])) ? track_state_str[st] : "unknown");
}

mccr_status_t
mccr_device_read_track_id_enable (mccr_device_t      *device,
                                  bool               *out_aamva_supported,
                                  mccr_track_state_t *out_track_1,
                                  mccr_track_state_t *out_track_2,
                                  mccr_track_state_t *out_track_3)
{
    mccr_status_t st;
    uint8_t       val;

    if ((st = common_device_read_property_byte (device, PROPERTY_TRACK_ID_ENABLE, &val)) != MCCR_STATUS_OK)
        return st;

    if (out_aamva_supported)
        *out_aamva_supported = !!(val & 0b10000000);
    if (out_track_1)
        *out_track_1 = (mccr_track_state_t) (val & 0b00000011);
    if (out_track_2)
        *out_track_2 = (mccr_track_state_t) (val & 0b00001100) >> 2;
    if (out_track_3)
        *out_track_3 = (mccr_track_state_t) (val & 0b00110000) >> 4;

    return MCCR_STATUS_OK;
}

mccr_status_t
mccr_device_read_iso_track_mask (mccr_device_t  *device,
                                 char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_ISO_TRACK_MASK, out_str);
}

mccr_status_t
mccr_device_read_aamva_track_mask (mccr_device_t  *device,
                                   char          **out_str)
{
    return common_device_read_property_string (device, PROPERTY_AAMVA_TRACK_MASK, out_str);
}

mccr_status_t
mccr_device_read_max_packet_size (mccr_device_t *device,
                                  uint8_t       *out_val)
{
    return common_device_read_property_byte (device, PROPERTY_MAX_PACKET_SIZE, out_val);
}

/******************************************************************************/
/* Device commands: get dukpt ksn and counter */

mccr_status_t
mccr_device_get_dukpt_ksn_and_counter (mccr_device_t  *device,
                                       uint8_t       **out_ksn_and_counter,
                                       size_t         *out_ksn_and_counter_size)
{
    mccr_status_t  st;
    const uint8_t *response;
    size_t         response_size;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_DUKPT_KSN_AND_COUNTER, NULL, 0);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    mccr_feature_report_get_response (device->feature_report, &response, &response_size);

    /* We expect 10 bytes as response */
    if (response_size != 10)
        return MCCR_STATUS_UNEXPECTED_FORMAT;

    if (out_ksn_and_counter_size)
        *out_ksn_and_counter_size = response_size;

    if (out_ksn_and_counter) {
        *out_ksn_and_counter = malloc (response_size);
        if (!(*out_ksn_and_counter))
            return MCCR_STATUS_FAILED;
        memcpy (*out_ksn_and_counter, response, response_size);
    }

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Device commands: set session id */

mccr_status_t
mccr_device_set_session_id (mccr_device_t *device,
                            uint64_t       session_id)
{
    uint64_t value_be;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    /* The session id may be anything that fits in 8 bytes, so we use a 64bit
     * uint in our API to manage it. The only thing we need to take care of is
     * to use the same endianness when setting it and when retrieving it later
     * on. In this case, we'll encode in BE (most significant byte first) */
    value_be = htobe64 (session_id);

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_SET_SESSION_ID, (uint8_t *)&value_be, sizeof (value_be));
    return mccr_feature_report_send_receive (device->feature_report, device->hid);
}

static const char *reader_state_str[] = {
    [MCCR_READER_STATE_WAIT_ACTIVATE_AUTHENTICATION] = "waiting to activate authentication",
    [MCCR_READER_STATE_WAIT_ACTIVATION_REPLY]        = "waiting for activation challenge reply",
    [MCCR_READER_STATE_WAIT_SWIPE]                   = "waiting for swipe",
    [MCCR_READER_STATE_WAIT_DELAY]                   = "waiting for anti-hacking timer",
};

/******************************************************************************/
/* Device commands: get reader state */

const char *
mccr_reader_state_to_string (mccr_reader_state_t st)
{
    return ((st < (sizeof (reader_state_str) / sizeof (reader_state_str[0]))) ? reader_state_str[st] : "unknown");
}

static const char *reader_state_antecedent_str[] = {
    [MCCR_READER_STATE_ANTECEDENT_POWERED_UP]               = "powered up",
    [MCCR_READER_STATE_ANTECEDENT_GOOD_AUTHENTICATION]      = "good authentication",
    [MCCR_READER_STATE_ANTECEDENT_GOOD_SWIPE]               = "good swipe",
    [MCCR_READER_STATE_ANTECEDENT_BAD_SWIPE]                = "bad swipe",
    [MCCR_READER_STATE_ANTECEDENT_FAILED_AUTHENTICATION]    = "failed authentication",
    [MCCR_READER_STATE_ANTECEDENT_FAILED_DEACTIVATION]      = "failed deactivation",
    [MCCR_READER_STATE_ANTECEDENT_TIMED_OUT_AUTHENTICATION] = "authentication timed out",
    [MCCR_READER_STATE_ANTECEDENT_TIMED_OUT_SWIPE]          = "swipe timed out",
    [MCCR_READER_STATE_ANTECEDENT_KEY_SYNC_ERROR]           = "key sync error",
};

const char *
mccr_reader_state_antecedent_to_string (mccr_reader_state_antecedent_t val)
{
    return ((val < (sizeof (reader_state_antecedent_str) / sizeof (reader_state_antecedent_str[0]))) ? reader_state_antecedent_str[val] : "unknown");
}

mccr_status_t
mccr_device_get_reader_state (mccr_device_t                  *device,
                              mccr_reader_state_t            *out_state,
                              mccr_reader_state_antecedent_t *out_antecedent)
{
    mccr_status_t  st;
    const uint8_t *response;
    size_t         response_size;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_READER_STATE, NULL, 0);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    mccr_feature_report_get_response (device->feature_report, &response, &response_size);

    /* We expect 2 bytes as response */
    if (response_size != 2)
        return MCCR_STATUS_UNEXPECTED_FORMAT;

    if (out_state)
        *out_state = (mccr_reader_state_t) response[0];
    if (out_antecedent)
        *out_antecedent = (mccr_reader_state_antecedent_t) response[1];

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Device commands: get security level */

mccr_status_t
mccr_device_get_security_level (mccr_device_t         *device,
                                mccr_security_level_t *out_level)
{
    mccr_status_t  st;
    const uint8_t      *response;
    size_t              response_size;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_SET_SECURITY_LEVEL, NULL, 0);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    mccr_feature_report_get_response (device->feature_report, &response, &response_size);

    /* We expect 1 byte as response */
    if (response_size != 1)
        return MCCR_STATUS_UNEXPECTED_FORMAT;

    if (out_level)
        *out_level = (mccr_security_level_t) response[0];

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Device commands: get encryption counter */

mccr_status_t
mccr_device_get_encryption_counter (mccr_device_t  *device,
                                    char          **out_device_serial_number,
                                    uint32_t       *out_encryption_counter)
{
    mccr_status_t  st;
    const uint8_t      *response;
    size_t              response_size;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_ENCRYPTION_COUNTER, NULL, 0);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    mccr_feature_report_get_response (device->feature_report, &response, &response_size);

    /* We expect 19 bytes as response */
    if (response_size != 19)
        return MCCR_STATUS_UNEXPECTED_FORMAT;

    if (out_device_serial_number) {
        *out_device_serial_number = calloc (17, 1);
        if (!(*out_device_serial_number))
            return MCCR_STATUS_FAILED;
        memcpy (*out_device_serial_number, response, 16);
    }

    if (out_encryption_counter) {
        uint32_t value = 0;

        memcpy (&value, &response[16], 3);
        *out_encryption_counter = le32toh (value);
    }

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Device commands: get magtek update token */

mccr_status_t
mccr_device_get_magtek_update_token (mccr_device_t  *device,
                                     uint8_t       **out_mut,
                                     size_t         *out_mut_size)
{
    mccr_status_t  st;
    const uint8_t      *response;
    size_t              response_size;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, MCCR_FEATURE_REPORT_COMMAND_GET_MAGTEK_UPDATE_TOKEN, NULL, 0);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    mccr_feature_report_get_response (device->feature_report, &response, &response_size);

    /* We expect 36 bytes as response */
    if (response_size != 36)
        return MCCR_STATUS_UNEXPECTED_FORMAT;

    if (out_mut_size)
        *out_mut_size = response_size;

    if (out_mut) {
        *out_mut = malloc (response_size);
        if (!(*out_mut))
            return MCCR_STATUS_FAILED;
        memcpy (*out_mut, response, response_size);
    }

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Device commands: generic */

mccr_status_t
mccr_device_run_generic (mccr_device_t  *device,
                         uint8_t         command_id,
                         const uint8_t  *blob,
                         size_t          blob_size,
                         uint8_t       **out_blob,
                         size_t         *out_blob_size)
{
    mccr_status_t st;

    if (!device->feature_report)
        return MCCR_STATUS_NOT_OPEN;

    mccr_feature_report_reset (device->feature_report);
    mccr_feature_report_set_request (device->feature_report, command_id, blob, blob_size);
    if ((st = mccr_feature_report_send_receive (device->feature_report, device->hid)) != MCCR_STATUS_OK)
        return st;

    if (out_blob || out_blob_size) {
        const uint8_t *response;
        size_t         response_size;

        mccr_feature_report_get_response (device->feature_report, &response, &response_size);

        if (out_blob_size)
            *out_blob_size = response_size;

        if (out_blob) {
            *out_blob = malloc (response_size);
            if (!(*out_blob))
                return MCCR_STATUS_FAILED;
            memcpy (*out_blob, response, response_size);
        }
    }

    return MCCR_STATUS_OK;
}

/******************************************************************************/
/* Swipe report */

struct mccr_swipe_report_s {
    mccr_input_report_t              *input_report;
    mccr_report_descriptor_context_t *desc;
};

void
mccr_swipe_report_free (mccr_swipe_report_t *report)
{
    mccr_report_descriptor_context_unref (report->desc);
    mccr_input_report_free (report->input_report);
    free (report);
}

static mccr_status_t
swipe_report_get_usage (mccr_swipe_report_t  *report,
                        uint8_t               usage_id,
                        size_t                expected_usage_size_bytes,
                        const uint8_t       **out_usage)
{
    uint32_t       usage_offset;
    uint32_t       usage_size;
    const uint8_t *input_report_data;
    size_t         input_report_data_size;

    assert (out_usage);

    if (!mccr_report_descriptor_get_input_report_usage (report->desc, usage_id, &usage_offset, &usage_size))
        return MCCR_STATUS_NOT_FOUND;

    /* Note that usage_size and usage_offset are given in BITs,
     * make everything simpler from now on */
    if ((usage_offset % 8 != 0) || (usage_size % 8 != 0)) {
        mccr_log ("error: bit-level offsets and sizes aren't expected nor supported (offset %u, size %u)",
                       usage_offset, usage_size);
        return MCCR_STATUS_INTERNAL;
    }
    /* convert offset/size to bytes */
    usage_offset /= 8;
    usage_size /= 8;

    if (expected_usage_size_bytes != 0 && usage_size != expected_usage_size_bytes) {
        mccr_log ("usage %u expected size %u but got size %u", usage_id, expected_usage_size_bytes, usage_size);
        return MCCR_STATUS_UNEXPECTED_FORMAT;
    }

    mccr_input_report_get_data (report->input_report, &input_report_data, &input_report_data_size);

    if ((usage_offset + usage_size) > input_report_data_size) {
        mccr_log ("usage %u goes out of bounds ((%u + %u) > %u)",
                       usage_id, usage_offset, usage_size, input_report_data_size);
        return MCCR_STATUS_UNEXPECTED_FORMAT;
    }

    *out_usage = input_report_data + usage_offset;
    return MCCR_STATUS_OK;
}

#define TRACK_API(N)                                                    \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_decode_status (mccr_swipe_report_t *report, \
                                                     uint8_t             *out_status) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t *usage;                                           \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_DECODE_STATUS, \
                                          1,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_status)                                                 \
            *out_status = *usage;                                       \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }                                                                   \
                                                                        \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_encrypted_data_length (mccr_swipe_report_t *report, \
                                                             uint8_t             *out_length) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t *usage;                                           \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_ENCRYPTED_DATA_LENGTH, \
                                          1,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_length)                                                 \
            *out_length = *usage;                                       \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }                                                                   \
                                                                        \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_absolute_data_length (mccr_swipe_report_t *report, \
                                                            uint8_t             *out_length) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t *usage;                                           \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_ABSOLUTE_DATA_LENGTH, \
                                          1,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_length)                                                 \
            *out_length = *usage;                                       \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }                                                                   \
                                                                        \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_encrypted_data (mccr_swipe_report_t  *report, \
                                                      const uint8_t       **out_data) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t      *usage;                                      \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_ENCRYPTED_DATA, \
                                          0,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_data)                                                   \
            *out_data = usage;                                          \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }                                                                   \
                                                                        \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_masked_data_length (mccr_swipe_report_t *report, \
                                                          uint8_t             *out_length) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t *usage;                                           \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_MASKED_DATA_LENGTH, \
                                          1,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_length)                                                 \
            *out_length = *usage;                                       \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }                                                                   \
                                                                        \
    mccr_status_t                                                       \
    mccr_swipe_report_get_track_##N##_masked_data (mccr_swipe_report_t  *report, \
                                                   const uint8_t       **out_data) \
    {                                                                   \
        mccr_status_t  st;                                              \
        const uint8_t *usage;                                           \
                                                                        \
        if ((st = swipe_report_get_usage (report,                       \
                                          MCCR_INPUT_USAGE_ID_TRACK_##N##_MASKED_DATA, \
                                          0,                            \
                                          &usage)) != MCCR_STATUS_OK)   \
            return st;                                                  \
                                                                        \
        if (out_data)                                                   \
            *out_data = usage;                                          \
                                                                        \
        return MCCR_STATUS_OK;                                          \
    }

TRACK_API (1)
TRACK_API (2)
TRACK_API (3)

static const char *card_encode_type_str[] = {
    [MCCR_CARD_ENCODE_TYPE_ISO_ABA]      = "ISO/ABA",
    [MCCR_CARD_ENCODE_TYPE_AAMVA]        = "AAMVA",
    [MCCR_CARD_ENCODE_TYPE_RESERVED]     = "reserved",
    [MCCR_CARD_ENCODE_TYPE_BLANK]        = "blank",
    [MCCR_CARD_ENCODE_TYPE_OTHER]        = "other",
    [MCCR_CARD_ENCODE_TYPE_UNDETERMINED] = "undetermined",
    [MCCR_CARD_ENCODE_TYPE_NONE]         = "none",
};

const char *
mccr_card_encode_type_to_string (mccr_card_encode_type_t st)
{
    return (st < (sizeof (card_encode_type_str) / sizeof (card_encode_type_str[0])) ? card_encode_type_str[st] : "unknown");
}

mccr_status_t
mccr_swipe_report_get_card_encode_type (mccr_swipe_report_t     *report,
                                        mccr_card_encode_type_t *out)
{
    mccr_status_t  st;
    const uint8_t *usage;

    if ((st = swipe_report_get_usage (report,
                                      MCCR_INPUT_USAGE_ID_CARD_ENCODE_TYPE,
                                      1,
                                      &usage)) != MCCR_STATUS_OK)
        return st;

    if (out)
        *out = (mccr_card_encode_type_t) *usage;

    return MCCR_STATUS_OK;
}

mccr_status_t
mccr_device_wait_swipe_report (mccr_device_t        *device,
                               int                   timeout_ms,
                               mccr_swipe_report_t **out_swipe_report)
{
    mccr_input_report_t *input_report;
    mccr_status_t        st;

    if (!device->desc)
        return MCCR_STATUS_NOT_OPEN;

    input_report = mccr_input_report_new (device->desc);
    if (!input_report)
        return MCCR_STATUS_FAILED;

    if ((st = mccr_input_report_receive (input_report, device->hid, timeout_ms)) != MCCR_STATUS_OK)
        goto out;

    if (out_swipe_report) {
        *out_swipe_report = (mccr_swipe_report_t *) calloc (sizeof (struct mccr_swipe_report_s), 1);
        if (!(*out_swipe_report)) {
            st = MCCR_STATUS_FAILED;
            goto out;
        }
        (*out_swipe_report)->desc = mccr_report_descriptor_context_ref (device->desc);
        (*out_swipe_report)->input_report = input_report;
        input_report = NULL;
    }

    st = MCCR_STATUS_OK;

out:
    mccr_input_report_free (input_report);
    return st;
}

/******************************************************************************/
/* Library initialization and teardown */

mccr_status_t
mccr_init (void)
{
    if (hid_init () < 0) {
        mccr_log ("hidapi initialization failed");
        return MCCR_STATUS_FAILED;
    }

    mccr_log ("mccr support initialized");
    return MCCR_STATUS_OK;
}

void
mccr_exit (void)
{
    if (hid_exit () < 0)
        mccr_log ("hidapi support finalization failed");
    mccr_log ("mccr support finished");
}

/******************************************************************************/
/* Library version info */

unsigned int
mccr_get_major_version (void)
{
    return MCCR_MAJOR_VERSION;
}

unsigned int
mccr_get_minor_version (void)
{
    return MCCR_MINOR_VERSION;
}

unsigned int
mccr_get_micro_version (void)
{
    return MCCR_MICRO_VERSION;
}
