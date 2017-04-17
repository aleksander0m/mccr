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

#if !defined MCCR_INPUT_REPORT_H
# define MCCR_INPUT_REPORT_H

#include <hidapi.h>

#include "mccr.h"
#include "mccr-hid.h"

typedef struct mccr_input_report_s mccr_input_report_t;

mccr_input_report_t *mccr_input_report_new      (mccr_report_descriptor_context_t  *desc);
void                 mccr_input_report_free     (mccr_input_report_t               *report);
mccr_status_t        mccr_input_report_receive  (mccr_input_report_t               *report,
                                                 hid_device                        *hid,
                                                 int                                timeout_ms);
void                 mccr_input_report_get_data (mccr_input_report_t               *report,
                                                 const uint8_t                    **data,
                                                 size_t                            *data_size);

#endif /* MCCR_INPUT_REPORT_H */
