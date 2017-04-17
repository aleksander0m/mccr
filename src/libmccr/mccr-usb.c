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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <libusb.h>

#include "mccr.h"
#include "mccr-log.h"
#include "mccr-usb.h"

/******************************************************************************/

static bool
parse_path (const char *path,
            uint16_t   *bus_number,
            uint16_t   *device_address,
            uint8_t    *interface_number)
{
    /* Note: in the libusb backend of hidapi, path is given as
     *   %04x:%04x:%02x (bus-number:device-address:interface-number)
     */
    unsigned int a, b, c;

    if (sscanf (path, "%x:%x:%x", &a, &b, &c) != 3)
        return false;

    if (a > 0xFFFF || b > 0xFFFF || c > 0xFF)
        return false;

    *bus_number       = (uint16_t) a;
    *device_address   = (uint16_t) b;
    *interface_number = (uint8_t)  c;
    return true;
}

static libusb_device *
find_device (libusb_context *usb_context,
             uint16_t        bus_number,
             uint16_t        device_address,
             uint8_t         interface_number)
{
    libusb_device  *found_device = NULL;
    libusb_device **devices = NULL;
    ssize_t         n_devices = 0;
    unsigned int    i;

    n_devices = libusb_get_device_list (usb_context, &devices);
    if (!n_devices || !devices) {
        mccr_log ("error: libusb device enumeration failed");
        return NULL;
    }

    /* Go over the devices and find the one we want */
    for (i = 0; !found_device && i < n_devices; i++) {
        /* Match bus number and device address */
        if ((libusb_get_bus_number (devices[i]) != bus_number) ||
            (libusb_get_device_address (devices[i]) != device_address))
            continue;

        mccr_log ("usb device in bus 0x%04x and address 0x%04x found",
                       bus_number, device_address);
        found_device = libusb_ref_device (devices[i]);
    }

    libusb_free_device_list (devices, 1);
    return found_device;
}

/*
 * Note: Reading the HID report descriptor involves claiming the interface;
 * and therefore we must detach the kernel driver and re-attach it afterwards.
 * This operation is invasive and it may end up re-enumerating /dev entry names,
 * although we don't care about that because we're not relying on those.
 */
mccr_status_t
mccr_read_report_descriptor (const char  *path,
                             uint8_t    **out_desc,
                             size_t      *out_desc_size)
{
    libusb_context       *usb_context = NULL;
    uint16_t              bus_number = 0;
    uint16_t              device_address = 0;
    uint8_t               interface_number = 0;
    libusb_device        *device = NULL;
    libusb_device_handle *handle = NULL;
    mccr_status_t         st;
    bool                  reattach = false;
    int                   desc_size;
    uint8_t               data[256];

    if (!parse_path (path, &bus_number, &device_address, &interface_number)) {
        mccr_log ("error: couldn't parse hidapi device path: %s", path);
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    if (libusb_init (&usb_context) != 0) {
        mccr_log ("error: libusb initialization failed");
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    device = find_device (usb_context, bus_number, device_address, interface_number);
    if (!device) {
        mccr_log ("error: device not found at hidapi path: %s", path);
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    if (libusb_open (device, &handle) < 0) {
        mccr_log ("error: couldn't open usb device");
        st = MCCR_STATUS_FAILED;
        goto out;
    }

    if (libusb_kernel_driver_active (handle, interface_number)) {
        if (libusb_detach_kernel_driver (handle, interface_number) < 0) {
            mccr_log ("error: couldn't detach kernel driver");
            st = MCCR_STATUS_FAILED;
            goto out;
        }
        reattach = true;
    }

    if (libusb_claim_interface (handle, interface_number) < 0) {
        mccr_log ("error: couldn't claim interface");
        st = MCCR_STATUS_FAILED;
        goto out_reattach;
    }

    /* Get the HID Report Descriptor. */
    if ((desc_size = libusb_control_transfer (handle,
                                              LIBUSB_ENDPOINT_IN | LIBUSB_RECIPIENT_INTERFACE,
                                              LIBUSB_REQUEST_GET_DESCRIPTOR,
                                              (LIBUSB_DT_REPORT << 8) | interface_number,
                                              0,
                                              data,
                                              sizeof (data),
                                              5000)) < 0) {
        mccr_log ("error: couldn't get HID descriptor");
        st = MCCR_STATUS_FAILED;
        goto out_release;
    }

    /* Set it as output */
    *out_desc = malloc (desc_size);
    if (!(*out_desc)) {
        mccr_log ("error: couldn't allocate descriptor buffer");
        st = MCCR_STATUS_FAILED;
        goto out_release;
    }
    *out_desc_size = desc_size;

    memcpy (*out_desc, data, desc_size);
    st = MCCR_STATUS_OK;

out_release:
    if (libusb_release_interface (handle, interface_number) < 0)
        mccr_log ("error: couldn't release interface");

out_reattach:
    if (reattach && libusb_attach_kernel_driver (handle, interface_number) < 0)
        mccr_log ("error: couldn't reattach kernel driver");

out:
    if (handle)
        libusb_close (handle);
    if (device)
        libusb_unref_device (device);
    if (usb_context)
        libusb_exit (usb_context);

    return st;
}
