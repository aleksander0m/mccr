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

#include <malloc.h>
#include <string.h>
#include <assert.h>

#include <hidapi.h>

#include "mccr.h"
#include "mccr-hid.h"
#include "mccr-log.h"
#include "mccr-input-report.h"

#define DEFAULT_IN_PROGRESS_TIMEOUT_MS 500

struct mccr_input_report_s {
    uint8_t *report_data;
    size_t   report_size;
};

mccr_input_report_t *
mccr_input_report_new (mccr_report_descriptor_context_t *desc)
{
    mccr_input_report_t *report;

    report = (mccr_input_report_t *) calloc (sizeof (mccr_input_report_t), 1);
    if (!report)
        return NULL;

    report->report_size = mccr_report_descriptor_get_input_report_size (desc);
    report->report_data = (uint8_t *) calloc (report->report_size, 1);
    if (!report->report_data) {
        mccr_input_report_free (report);
        return NULL;
    }
    return report;
}

void
mccr_input_report_free (mccr_input_report_t *report)
{
    if (!report)
        return;

    free (report->report_data);
    free (report);
}

mccr_status_t
mccr_input_report_receive (mccr_input_report_t *report,
                           hid_device          *hid,
                           int                  timeout_ms)
{
    int    n_read;
    size_t total_read = 0;

    if (!hid)
        return MCCR_STATUS_NOT_OPEN;

    mccr_log ("waiting for input report (%u bytes): timeout %d ms", report->report_size, timeout_ms);

    do {
        n_read = hid_read_timeout (hid,
                                   &report->report_data[total_read],
                                   report->report_size - total_read,
                                   total_read == 0 ? timeout_ms : DEFAULT_IN_PROGRESS_TIMEOUT_MS);
        if (n_read < 0) {
            mccr_log ("error reported reading input report: %ls", hid_error (hid));
            return MCCR_STATUS_REPORT_FAILED;
        }

        if (!n_read)
            break;

        total_read += n_read;
        mccr_log ("read %u bytes... (total %u)", n_read, total_read);
    } while (total_read != report->report_size);

    mccr_log_raw ("<<<<", report->report_data, total_read);

    if (timeout_ms >= 0 && !total_read)
        return MCCR_STATUS_TIMED_OUT;

    if (total_read != report->report_size) {
        mccr_log ("error: only read %u bytes, expected %u bytes", total_read, report->report_size);
        return MCCR_STATUS_UNEXPECTED_FORMAT;
    }

    return MCCR_STATUS_OK;
}

void
mccr_input_report_get_data (mccr_input_report_t  *report,
                            const uint8_t       **data,
                            size_t               *data_size)
{
    *data      = report->report_data;
    *data_size = report->report_size;
}
