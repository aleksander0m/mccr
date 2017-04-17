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

#if !defined MCCR_HID_H
# define MCCR_HID_H

/******************************************************************************/
/* Report descriptor */

#define MCCR_USAGE_PAGE 0xFF00
#define MCCR_USAGE      0x01

/* Input report usage IDs */
enum {
    MCCR_INPUT_USAGE_ID_TRACK_1_DECODE_STATUS           = 0x20,
    MCCR_INPUT_USAGE_ID_TRACK_2_DECODE_STATUS           = 0x21,
    MCCR_INPUT_USAGE_ID_TRACK_3_DECODE_STATUS           = 0x22,
    MCCR_INPUT_USAGE_ID_MAGNEPRINT_STATUS               = 0x23,
    MCCR_INPUT_USAGE_ID_TRACK_1_ENCRYPTED_DATA_LENGTH   = 0x28,
    MCCR_INPUT_USAGE_ID_TRACK_2_ENCRYPTED_DATA_LENGTH   = 0x29,
    MCCR_INPUT_USAGE_ID_TRACK_3_ENCRYPTED_DATA_LENGTH   = 0x2a,
    MCCR_INPUT_USAGE_ID_MAGNEPRINT_DATA_LENGTH          = 0x2b,
    MCCR_INPUT_USAGE_ID_TRACK_1_ENCRYPTED_DATA          = 0x30,
    MCCR_INPUT_USAGE_ID_TRACK_2_ENCRYPTED_DATA          = 0x31,
    MCCR_INPUT_USAGE_ID_TRACK_3_ENCRYPTED_DATA          = 0x32,
    MCCR_INPUT_USAGE_ID_MAGNEPRINT_DATA                 = 0x33,
    MCCR_INPUT_USAGE_ID_CARD_ENCODE_TYPE                = 0x38,
    MCCR_INPUT_USAGE_ID_CARD_STATUS                     = 0x39,
    MCCR_INPUT_USAGE_ID_DEVICE_SERIAL_NUMBER            = 0x40,
    MCCR_INPUT_USAGE_ID_READER_ENCRYPTION_STATUS        = 0x42,
    MCCR_INPUT_USAGE_ID_DUKPT_SERIAL_NUMBER_COUNTER     = 0x46,
    MCCR_INPUT_USAGE_ID_TRACK_1_MASKED_DATA_LENGTH      = 0x47,
    MCCR_INPUT_USAGE_ID_TRACK_2_MASKED_DATA_LENGTH      = 0x48,
    MCCR_INPUT_USAGE_ID_TRACK_3_MASKED_DATA_LENGTH      = 0x49,
    MCCR_INPUT_USAGE_ID_TRACK_1_MASKED_DATA             = 0x4a,
    MCCR_INPUT_USAGE_ID_TRACK_2_MASKED_DATA             = 0x4b,
    MCCR_INPUT_USAGE_ID_TRACK_3_MASKED_DATA             = 0x4c,
    MCCR_INPUT_USAGE_ID_ENCRYPTED_SESSION_ID            = 0x50,
    MCCR_INPUT_USAGE_ID_TRACK_1_ABSOLUTE_DATA_LENGTH    = 0x51,
    MCCR_INPUT_USAGE_ID_TRACK_2_ABSOLUTE_DATA_LENGTH    = 0x52,
    MCCR_INPUT_USAGE_ID_TRACK_3_ABSOLUTE_DATA_LENGTH    = 0x53,
    MCCR_INPUT_USAGE_ID_MAGNEPRINT_ABSOLUTE_DATA_LENGTH = 0x54,
    MCCR_INPUT_USAGE_ID_ENCRYPTION_COUNTER              = 0x55,
    MCCR_INPUT_USAGE_ID_MAGNESAFE_VERSION_NUMBER        = 0x56,
    MCCR_INPUT_USAGE_ID_HASHED_TRACK_2_DATA             = 0x57,
};

/* Feature report usage IDs */
enum {
    MCCR_FEATURE_USAGE_ID_COMMAND_MESSAGE = 0x20,
};

typedef struct mccr_report_descriptor_context_s mccr_report_descriptor_context_t;

bool   mccr_report_descriptor_get_input_report_usage   (mccr_report_descriptor_context_t *ctx,
                                                        uint8_t                           usage_id,
                                                        uint32_t                         *usage_offset,
                                                        uint32_t                         *usage_size);
size_t mccr_report_descriptor_get_input_report_size    (mccr_report_descriptor_context_t *ctx);
bool   mccr_report_descriptor_get_feature_report_usage (mccr_report_descriptor_context_t *ctx,
                                                        uint8_t                           usage_id,
                                                        uint32_t                         *usage_offset,
                                                        uint32_t                         *usage_size);
size_t mccr_report_descriptor_get_feature_report_size  (mccr_report_descriptor_context_t *ctx);

mccr_report_descriptor_context_t *mccr_report_descriptor_context_ref   (mccr_report_descriptor_context_t *ctx);
void                              mccr_report_descriptor_context_unref (mccr_report_descriptor_context_t *ctx);

mccr_status_t mccr_parse_report_descriptor (const uint8_t                     *desc,
                                            size_t                             desc_size,
                                            mccr_report_descriptor_context_t **out_ctx);

#endif /* MCCR_HID_H */
