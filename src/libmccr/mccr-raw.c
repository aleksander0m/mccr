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
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <linux/hidraw.h>

#include "mccr.h"
#include "mccr-log.h"
#include "mccr-raw.h"

/******************************************************************************/

mccr_status_t
mccr_read_report_descriptor (const char  *path,
                             uint8_t    **out_desc,
                             size_t      *out_desc_size)
{
    struct hidraw_report_descriptor rpt_desc;
    int                             fd, desc_size;
    mccr_status_t                   st;

    fd = open (path, O_RDWR|O_NONBLOCK);
    if (fd < 0) {
        mccr_log ("error: couldn't open raw device: %s", strerror (errno));
        return MCCR_STATUS_FAILED;
    }

    /* Get Report Descriptor Size */
    if (ioctl (fd, HIDIOCGRDESCSIZE, &desc_size) < 0) {
        mccr_log ("error: couldn't read report descriptor size: %s", strerror (errno));
        return MCCR_STATUS_FAILED;
    }

    /* Get Report Descriptor */
    memset(&rpt_desc, 0, sizeof (rpt_desc));
    rpt_desc.size = desc_size;
    if (ioctl (fd, HIDIOCGRDESC, &rpt_desc) < 0) {
        mccr_log ("error: couldn't read report descriptor: %s", strerror (errno));
        return MCCR_STATUS_FAILED;
    }

    *out_desc = malloc (desc_size);
    if (!(*out_desc)) {
        mccr_log ("error: couldn't allocate descriptor buffer");
        st = MCCR_STATUS_FAILED;
        goto out;
    }
    *out_desc_size = desc_size;

    memcpy (*out_desc, rpt_desc.value, rpt_desc.size);

    st = MCCR_STATUS_OK;

out:
    if (fd >= 0)
        close (fd);

    return st;
}
