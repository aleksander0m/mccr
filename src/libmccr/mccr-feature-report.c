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

#include <malloc.h>
#include <string.h>
#include <assert.h>

#include <hidapi.h>

#include "mccr.h"
#include "mccr-hid.h"
#include "mccr-log.h"
#include "mccr-feature-report.h"

struct feature_report_request_s {
    uint8_t report_id; /* always 0 */
    uint8_t command;
    uint8_t data_length;
    uint8_t data[];
} __attribute__((packed));

struct feature_report_response_s {
    uint8_t report_id; /* always 0 */
    uint8_t result_code;
    uint8_t data_length;
    uint8_t data[];
} __attribute__((packed));

struct mccr_feature_report_s {
    size_t                            report_size;
    struct feature_report_request_s  *request;
    struct feature_report_response_s *response;
};

enum {
    FEATURE_REPORT_RESULT_SUCCESS            = 0x00,
    FEATURE_REPORT_RESULT_FAILURE            = 0x01,
    FEATURE_REPORT_RESULT_BAD_PARAMETER      = 0x02,
    FEATURE_REPORT_RESULT_DELAYED            = 0x05,
    FEATURE_REPORT_RESULT_INVALID_OPERATION  = 0x07,
};

static mccr_status_t
feature_report_result_to_mccr_status (uint8_t result_code)
{
    switch (result_code) {
    case FEATURE_REPORT_RESULT_SUCCESS:
        return MCCR_STATUS_OK;
    case FEATURE_REPORT_RESULT_BAD_PARAMETER:
        return MCCR_STATUS_INTERNAL;
    case FEATURE_REPORT_RESULT_DELAYED:
        return MCCR_STATUS_DELAYED;
    case FEATURE_REPORT_RESULT_INVALID_OPERATION:
        return MCCR_STATUS_INVALID_OPERATION;
    case FEATURE_REPORT_RESULT_FAILURE:
    default:
        return MCCR_STATUS_FAILED;
    }
}

mccr_feature_report_t *
mccr_feature_report_new (mccr_report_descriptor_context_t *desc)
{
    mccr_feature_report_t *report;

    report = (mccr_feature_report_t *) calloc (sizeof (mccr_feature_report_t), 1);
    if (!report)
        return NULL;

    /* One extra byte for report id */
    report->report_size = 1 + mccr_report_descriptor_get_feature_report_size (desc);
    report->request     = (struct feature_report_request_s *)  calloc (report->report_size, 1);
    report->response    = (struct feature_report_response_s *) calloc (report->report_size, 1);

    if (!report->request || !report->response) {
        mccr_feature_report_free (report);
        return NULL;
    }

    return report;
}

void
mccr_feature_report_free (mccr_feature_report_t *report)
{
    if (!report)
        return;

    free (report->request);
    free (report->response);
    free (report);
}

void
mccr_feature_report_reset (mccr_feature_report_t *report)
{
    memset (report->request,  0, report->report_size);
    memset (report->response, 0, report->report_size);
}

void
mccr_feature_report_set_request (mccr_feature_report_t *report,
                                 uint8_t                command,
                                 const uint8_t         *data,
                                 size_t                 data_size)
{
    assert (data_size <= (report->report_size - sizeof (struct feature_report_request_s)));
    report->request->command = command;
    if (data && data_size) {
        report->request->data_length = data_size;
        memcpy (report->request->data, data, data_size);
    }
}

void
mccr_feature_report_get_response (mccr_feature_report_t  *report,
                                  const uint8_t         **data,
                                  size_t                 *data_size)
{
    *data      = report->response->data;
    *data_size = report->response->data_length;
}

mccr_status_t
mccr_feature_report_send_receive (mccr_feature_report_t *report,
                                  hid_device            *hid)
{
    int sent;

    assert (hid);

    mccr_log ("sending feature report request: command 0x%02x...", report->request->command);
    mccr_log_raw (">>>>", report->request, report->report_size);
    sent = hid_send_feature_report (hid, (const unsigned char *) report->request, report->report_size);
    if (sent != report->report_size) {
        if (sent < 0)
            mccr_log ("error reported sending feature report: %ls", hid_error (hid));
        else
            mccr_log ("wrote only %d/%zu bytes of feature report: %ls", sent, report->report_size, hid_error (hid));
        return MCCR_STATUS_WRITE_FAILED;
    }

    mccr_log ("receiving feature report response...");
    if (hid_get_feature_report (hid, (unsigned char *) report->response, report->report_size) != report->report_size)
        return MCCR_STATUS_READ_FAILED;
    mccr_log_raw ("<<<<", report->response, report->report_size);

    return feature_report_result_to_mccr_status (report->response->result_code);
}
