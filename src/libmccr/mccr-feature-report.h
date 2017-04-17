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

#if !defined MCCR_FEATURE_REPORT_H
# define MCCR_FEATURE_REPORT_H

#include <hidapi.h>

#include "mccr.h"
#include "mccr-hid.h"

typedef struct mccr_feature_report_s mccr_feature_report_t;

enum {
    MCCR_FEATURE_REPORT_COMMAND_GET_PROPERTY              = 0x00,
    MCCR_FEATURE_REPORT_COMMAND_SET_PROPERTY              = 0x01,
    MCCR_FEATURE_REPORT_COMMAND_RESET_DEVICE              = 0x02,
    MCCR_FEATURE_REPORT_COMMAND_GET_DUKPT_KSN_AND_COUNTER = 0x09,
    MCCR_FEATURE_REPORT_COMMAND_SET_SESSION_ID            = 0x0A,
    MCCR_FEATURE_REPORT_COMMAND_GET_READER_STATE          = 0x14,
    MCCR_FEATURE_REPORT_COMMAND_SET_SECURITY_LEVEL        = 0x15,
    MCCR_FEATURE_REPORT_COMMAND_GET_ENCRYPTION_COUNTER    = 0x1C,
    MCCR_FEATURE_REPORT_COMMAND_GET_MAGTEK_UPDATE_TOKEN   = 0x19,
    MCCR_FEATURE_REPORT_COMMAND_UPDATE_ENCRYPTION_KEY     = 0x22,
};

mccr_feature_report_t *mccr_feature_report_new          (mccr_report_descriptor_context_t  *desc);
void                   mccr_feature_report_free         (mccr_feature_report_t             *report);
void                   mccr_feature_report_reset        (mccr_feature_report_t             *report);
void                   mccr_feature_report_set_request  (mccr_feature_report_t             *report,
                                                         uint8_t                            command,
                                                         const uint8_t                     *data,
                                                         size_t                             data_size);
mccr_status_t          mccr_feature_report_send_receive (mccr_feature_report_t             *report,
                                                         hid_device                        *hid);
void                   mccr_feature_report_get_response (mccr_feature_report_t             *report,
                                                         const uint8_t                    **data,
                                                         size_t                            *data_size);

#endif /* MCCR_FEATURE_REPORT_H */
