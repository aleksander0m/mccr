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
#include <stdint.h>
#include <string.h>
#include <endian.h>
#include <malloc.h>
#include <assert.h>

#include "mccr.h"
#include "mccr-log.h"
#include "mccr-hid.h"

static const char *type_str[] = {
    [0b00] = "main",
    [0b01] = "global",
    [0b10] = "local",
    [0b11] = "reserved"
};

static const char *main_tag_str[] = {
    [0b0000] = "Reserved",
    [0b0001] = "Reserved",
    [0b0010] = "Reserved",
    [0b0011] = "Reserved",
    [0b0100] = "Reserved",
    [0b0101] = "Reserved",
    [0b0110] = "Reserved",
    [0b0111] = "Reserved",
    [0b1000] = "Input",
    [0b1001] = "Output",
    [0b1010] = "Collection",
    [0b1011] = "Feature",
    [0b1100] = "End Collection",
    [0b1101] = "Reserved",
    [0b1110] = "Reserved",
    [0b1111] = "Reserved",
};

static const char *main_collection_str[] = {
    [0x00] = "Physical",
    [0x01] = "Application",
    [0x02] = "Logical",
    [0x03] = "Report",
    [0x04] = "Named array",
    [0x05] = "Usage switch",
    [0x06] = "Usage modifier"
};

static const char *global_tag_str[] = {
    [0b0000] = "Usage page",
    [0b0001] = "Logical minimum",
    [0b0010] = "Logical maximum",
    [0b0011] = "Physical minimum",
    [0b0100] = "Physical maximum",
    [0b0101] = "Unit exponent",
    [0b0110] = "Unit",
    [0b0111] = "Report size",
    [0b1000] = "Report ID",
    [0b1001] = "Report count",
    [0b1010] = "Push",
    [0b1011] = "Pop",
    [0b1100] = "Reserved",
    [0b1101] = "Reserved",
    [0b1110] = "Reserved",
    [0b1111] = "Reserved"
};

static const char *local_tag_str[] = {
    [0b0000] = "Usage",
    [0b0001] = "Usage minimum",
    [0b0010] = "Usage maximum",
    [0b0011] = "Designator index",
    [0b0100] = "Designator minimum",
    [0b0101] = "Designator maximum",
    [0b0110] = "Reserved",
    [0b0111] = "String index",
    [0b1000] = "String minimum",
    [0b1001] = "String maximum",
    [0b1010] = "Delimiter",
    [0b1011] = "Reserved",
    [0b1100] = "Reserved",
    [0b1101] = "Reserved",
    [0b1110] = "Reserved",
    [0b1111] = "Reserved"
};

static const char *input_usage_id_str[] = {
    [MCCR_INPUT_USAGE_ID_TRACK_1_DECODE_STATUS]           = "Track 1 decode status",
    [MCCR_INPUT_USAGE_ID_TRACK_2_DECODE_STATUS]           = "Track 2 decode status",
    [MCCR_INPUT_USAGE_ID_TRACK_3_DECODE_STATUS]           = "Track 3 decode status",
    [MCCR_INPUT_USAGE_ID_MAGNEPRINT_STATUS]               = "Magneprint status",
    [MCCR_INPUT_USAGE_ID_TRACK_1_ENCRYPTED_DATA_LENGTH]   = "Track 1 encrypted data length",
    [MCCR_INPUT_USAGE_ID_TRACK_2_ENCRYPTED_DATA_LENGTH]   = "Track 2 encrypted data length",
    [MCCR_INPUT_USAGE_ID_TRACK_3_ENCRYPTED_DATA_LENGTH]   = "Track 3 encrypted data length",
    [MCCR_INPUT_USAGE_ID_MAGNEPRINT_DATA_LENGTH]          = "Magneprint data length",
    [MCCR_INPUT_USAGE_ID_TRACK_1_ENCRYPTED_DATA]          = "Track 1 encrypted data",
    [MCCR_INPUT_USAGE_ID_TRACK_2_ENCRYPTED_DATA]          = "Track 2 encrypted data",
    [MCCR_INPUT_USAGE_ID_TRACK_3_ENCRYPTED_DATA]          = "Track 3 encrypted data",
    [MCCR_INPUT_USAGE_ID_MAGNEPRINT_DATA]                 = "Magneprint data",
    [MCCR_INPUT_USAGE_ID_CARD_ENCODE_TYPE]                = "Card encode type",
    [MCCR_INPUT_USAGE_ID_CARD_STATUS]                     = "Card status",
    [MCCR_INPUT_USAGE_ID_DEVICE_SERIAL_NUMBER]            = "Device serial number",
    [MCCR_INPUT_USAGE_ID_READER_ENCRYPTION_STATUS]        = "Reader encryption status",
    [MCCR_INPUT_USAGE_ID_DUKPT_SERIAL_NUMBER_COUNTER]     = "DUKPT serial number/counter",
    [MCCR_INPUT_USAGE_ID_TRACK_1_MASKED_DATA_LENGTH]      = "Track 1 masked data length",
    [MCCR_INPUT_USAGE_ID_TRACK_2_MASKED_DATA_LENGTH]      = "Track 2 masked data length",
    [MCCR_INPUT_USAGE_ID_TRACK_3_MASKED_DATA_LENGTH]      = "Track 3 masked data length",
    [MCCR_INPUT_USAGE_ID_TRACK_1_MASKED_DATA]             = "Track 1 masked data",
    [MCCR_INPUT_USAGE_ID_TRACK_2_MASKED_DATA]             = "Track 2 masked data",
    [MCCR_INPUT_USAGE_ID_TRACK_3_MASKED_DATA]             = "Track 3 masked data",
    [MCCR_INPUT_USAGE_ID_ENCRYPTED_SESSION_ID]            = "Encrypted session id",
    [MCCR_INPUT_USAGE_ID_TRACK_1_ABSOLUTE_DATA_LENGTH]    = "Track 1 data absolute length",
    [MCCR_INPUT_USAGE_ID_TRACK_2_ABSOLUTE_DATA_LENGTH]    = "Track 2 data absolute length",
    [MCCR_INPUT_USAGE_ID_TRACK_3_ABSOLUTE_DATA_LENGTH]    = "Track 3 data absolute length",
    [MCCR_INPUT_USAGE_ID_MAGNEPRINT_ABSOLUTE_DATA_LENGTH] = "Magneprint data absolute length",
    [MCCR_INPUT_USAGE_ID_ENCRYPTION_COUNTER]              = "Encryption counter",
    [MCCR_INPUT_USAGE_ID_MAGNESAFE_VERSION_NUMBER]        = "MagneSafe version number",
    [MCCR_INPUT_USAGE_ID_HASHED_TRACK_2_DATA]             = "Hashed track 2 data",
};

static const char *
input_usage_id_to_string (uint8_t usage_id)
{
    const char *str = NULL;

    if (usage_id < (sizeof (input_usage_id_str) / sizeof (input_usage_id_str[0])))
        str = input_usage_id_str[usage_id];

    return (str ? str : "unknown");
}

static const char *feature_usage_id_str[] = {
    [MCCR_FEATURE_USAGE_ID_COMMAND_MESSAGE] = "command message"
};

static const char *
feature_usage_id_to_string (uint8_t usage_id)
{
    const char *str = NULL;

    if (usage_id < (sizeof (feature_usage_id_str) / sizeof (feature_usage_id_str[0])))
        str = feature_usage_id_str[usage_id];

    return (str ? str : "unknown");
}

/******************************************************************************/

typedef struct {
    uint32_t id;
    uint32_t size_bits;
    uint32_t offset_bits;
} usage_t;

static void
append_to_usage_array (usage_t **array,
                       size_t   *array_size,
                       size_t   *array_allocated,
                       usage_t  *items,
                       size_t    n_items)
{
    if ((*array_size + n_items) > *array_allocated) {
        do
            *array_allocated = (*array_allocated > 0 ? (*array_allocated * 2) : 2);
        while ((*array_size + n_items) > *array_allocated);
        *array = realloc (*array, sizeof (usage_t) * (*array_allocated));
        if (!(*array)) {
            *array_size = *array_allocated = 0;
            return;
        }
    }

    assert ((*array_size + n_items) <= *array_allocated);
    assert (*array);
    memcpy (&((*array)[*array_size]), items, sizeof (usage_t) * n_items);
    (*array_size) += n_items;
}

static void
cleanup_usage_array (usage_t **array,
                     size_t   *array_size,
                     size_t   *array_allocated)
{
    free (*array);
    *array = NULL;
    *array_size = *array_allocated = 0;
}

static usage_t *
lookup_usage_array (usage_t *array,
                    size_t   array_size,
                    uint8_t  usage_id)
{
    size_t i;

    for (i = 0; i < array_size; i++) {
        usage_t *usage;

        usage = &array[i];
        if (usage->id == usage_id)
            return usage;
    }
    return NULL;
}

typedef struct {
    usage_t *usages;
    size_t   usages_size;
    size_t   usages_allocated;
    size_t   size;
} report_t;

struct mccr_report_descriptor_context_s {
    volatile int refcount;
    report_t     input;
    report_t     feature;
};

void
mccr_report_descriptor_context_unref (mccr_report_descriptor_context_t *ctx)
{
    assert (ctx);
    if (__sync_fetch_and_sub (&ctx->refcount, 1) != 1)
        return;

    free (ctx->input.usages);
    free (ctx->feature.usages);
    free (ctx);
}

mccr_report_descriptor_context_t *
mccr_report_descriptor_context_ref (mccr_report_descriptor_context_t *ctx)
{
    assert (ctx);
    __sync_fetch_and_add (&ctx->refcount, 1);
    return ctx;
}

bool
mccr_report_descriptor_get_input_report_usage (mccr_report_descriptor_context_t *ctx,
                                               uint8_t                           usage_id,
                                               uint32_t                         *usage_offset,
                                               uint32_t                         *usage_size)
{
    usage_t *usage;

    usage = lookup_usage_array (ctx->input.usages, ctx->input.usages_size, usage_id);
    if (!usage)
        return false;

    if (usage_offset)
        *usage_offset = usage->offset_bits;
    if (usage_size)
        *usage_size = usage->size_bits;
    return true;
}

size_t
mccr_report_descriptor_get_input_report_size (mccr_report_descriptor_context_t *ctx)
{
    return ctx->input.size;
}

bool
mccr_report_descriptor_get_feature_report_usage (mccr_report_descriptor_context_t *ctx,
                                                 uint8_t                           usage_id,
                                                 uint32_t                         *usage_offset,
                                                 uint32_t                         *usage_size)
{
    usage_t *usage;

    usage = lookup_usage_array (ctx->feature.usages, ctx->feature.usages_size, usage_id);
    if (!usage)
        return false;

    if (usage_offset)
        *usage_offset = usage->offset_bits;
    if (usage_size)
        *usage_size = usage->size_bits;
    return true;
}

size_t
mccr_report_descriptor_get_feature_report_size (mccr_report_descriptor_context_t *ctx)
{
    return ctx->feature.size;
}

/******************************************************************************/

typedef struct {
    mccr_report_descriptor_context_t *desc_ctx;

    /* global */
    uint32_t usage_page;
    uint32_t report_size;
    uint32_t report_count;
    /* main */
    uint32_t collection;
    /* local */

    /* helpers */
    unsigned int log_indent;
    bool         fatal_error;
    usage_t     *wip_usages;
    size_t       wip_usages_size;
    size_t       wip_usages_allocated;
} parse_context_t;

static void
parse_context_clear (parse_context_t *ctx)
{
    mccr_report_descriptor_context_unref (ctx->desc_ctx);
    free (ctx->wip_usages);
}

static void
process_input_output_feature (parse_context_t *ctx,
                              uint8_t          tag,
                              uint32_t         value)
{
    report_t *target = NULL;

    if (mccr_log_is_enabled ()) {
        char value_str[255] = { '\0' };

        strcat (value_str, (value & (1 << 0)) ? "constant," : "data,");
        strcat (value_str, (value & (1 << 1)) ? "variable," : "array,");
        strcat (value_str, (value & (1 << 2)) ? "relative," : "absolute,");
        strcat (value_str, (value & (1 << 3)) ? "wrap," : "no wrap,");
        strcat (value_str, (value & (1 << 4)) ? "non linear," : "linear,");
        strcat (value_str, (value & (1 << 5)) ? "no preferred," : "preferred state,");
        strcat (value_str, (value & (1 << 6)) ? "null state," : "no null position,");
        if (tag != 0b1000)
            strcat (value_str, (value & (1 << 7)) ? "volatile," : "non volatile,");
        strcat (value_str, (value & (1 << 8)) ? "buffered bytes" : "bitfield");
        mccr_log ("%*s%s (0x%x: %s)", ctx->log_indent, "", main_tag_str[tag], value, value_str);
    }

    switch (tag) {
    case 0b1000:
        target = &(ctx->desc_ctx->input);
        break;
    case 0b1011:
        target = &(ctx->desc_ctx->feature);
        break;
    default:
        break;
    }

    /* Copy to report */
    if (target)
        append_to_usage_array (&(target->usages), &(target->usages_size), &(target->usages_allocated),
                               ctx->wip_usages, ctx->wip_usages_size);

    /* And cleanup previous usages */
    cleanup_usage_array (&ctx->wip_usages, &ctx->wip_usages_size, &ctx->wip_usages_allocated);
}

static void
process_collection (parse_context_t *ctx,
                    uint32_t         value)
{
    if (mccr_log_is_enabled ()) {
        char value_str[64] = { '\0' };

        if (value < (sizeof (main_collection_str) / sizeof (main_collection_str[0])))
            strcat (value_str, main_collection_str[value]);
        else if (value <= 0x7F)
            strcat (value_str, "reserved");
        else if (value <= 0xFF)
            strcat (value_str, "vendor-defined");
        else
            strcat (value_str, "invalid");
        mccr_log ("%*s%s (0x%x: %s)", ctx->log_indent, "", main_tag_str[0b1010], value, value_str);
        /* increase indent */
        ctx->log_indent += 2;
    }

    if (!ctx->wip_usages_size) {
        mccr_log ("error: collection defined with no associated usage");
        ctx->fatal_error = true;
        return;
    }
    if (ctx->wip_usages_size != 1) {
        mccr_log ("error: collection defined associated to multiple usages");
        ctx->fatal_error = true;
        return;
    }

    /* We expect a single collection, part of the default usage */
    if (ctx->wip_usages->id != MCCR_USAGE) {
        mccr_log ("error: collection not defined on the default mccr usage");
        ctx->fatal_error = true;
        return;
    }

    /* The single collection should be of type Application */
    if (value != 0x01) {
        mccr_log ("error: unexpected collection type");
        ctx->fatal_error = true;
        return;
    }

    /* store */
    ctx->collection = value;

    /* And cleanup previous usages */
    cleanup_usage_array (&ctx->wip_usages, &ctx->wip_usages_size, &ctx->wip_usages_allocated);
}

static void
process_collection_end (parse_context_t *ctx)
{
    if (mccr_log_is_enabled ()) {
        ctx->log_indent = (ctx->log_indent >= 2 ? (ctx->log_indent - 2) : 0);
        mccr_log ("%*s%s", ctx->log_indent, "", main_tag_str[0b1100]);
    }

    if (ctx->wip_usages_size) {
        mccr_log ("error: usages defined out of input/output/report inside the collection");
        ctx->fatal_error = true;
    }

    /* We don't really support nested collections, we don't care right now */
    ctx->collection = 0;
}

static void
parse_item_main (parse_context_t *ctx,
                 uint8_t          tag,
                 uint32_t         value)
{
    switch (tag) {
    case 0b1000:
    case 0b1001:
    case 0b1011:
        process_input_output_feature (ctx, tag, value);
        break;
    case 0b1010:
        process_collection (ctx, value);
        break;
    case 0b1100:
        process_collection_end (ctx);
        break;
    default:
        mccr_log ("%*s%s (0x%x)",
                  ctx->log_indent, "",
                  (tag < (sizeof (main_tag_str) / sizeof (main_tag_str[0]))) ? main_tag_str[tag] : "Unknown main item",
                  value);
        /* Cleanup previous usages, if any (ignored) */
        cleanup_usage_array (&ctx->wip_usages, &ctx->wip_usages_size, &ctx->wip_usages_allocated);
        break;
    }
}

static void
process_report_count (parse_context_t *ctx,
                      uint32_t         value)
{
    uint32_t size_bits, size_bits_single, i;

    size_bits = value * ctx->report_size;

    /* Warn if no usages were defined */
    if (!ctx->wip_usages_size) {
        mccr_log ("warning: report count given but not previous usage defined");
        return;
    }

    /* If we have a single usage defined previously, the report count
     * defines the length of the field. */
    if (ctx->wip_usages_size == 1) {
        assert (ctx->wip_usages);
        ctx->wip_usages->size_bits = size_bits;
        if (!ctx->wip_usages->size_bits) {
            mccr_log ("error: couldn't compute usage field size in bits");
            ctx->fatal_error = true;
        }
        return;
    }

    /* If we have more than one usage defined previously, the report
     * count defines the length of all the previous fields together, we
     * assume evenly distributed. */
    if (size_bits % ctx->wip_usages_size != 0) {
        mccr_log ("error: size given by report count doesn't match previously defined usage count");
        ctx->fatal_error = true;
        return;
    }

    /* Apply the size to all previous usages */
    size_bits_single = size_bits / ctx->wip_usages_size;
    for (i = 0; i < ctx->wip_usages_size; i++) {
        usage_t *usage;

        usage = &ctx->wip_usages[i];
        usage->size_bits = size_bits_single;
    }
}

static void
process_report_size (parse_context_t *ctx,
                     uint32_t         value)
{
    ctx->report_size = value;
    if (ctx->report_size != 8)
        mccr_log ("warning: unexpected report size: %u", ctx->report_size);
}

static void
process_usage_page (parse_context_t *ctx,
                    uint32_t         value)
{
    ctx->usage_page = value;
    if (ctx->usage_page != 0xFF00) {
        /* our logic here doesn't expect any usage page other than the
         * mccr usage page, report an error if we get any as we'd
         * require to update the library to support it.
         */
        mccr_log ("error: unsupported usage page reported: 0x%x");
        ctx->fatal_error = true;
    }
}

static void
parse_item_global (parse_context_t *ctx,
                   uint8_t          tag,
                   uint32_t         value)
{
    mccr_log ("%*s%s (0x%x: %u)",
              ctx->log_indent, "",
              (tag < (sizeof (global_tag_str) / sizeof (global_tag_str[0]))) ? global_tag_str[tag] : "reserved",
              value, value);

    /* Store the values we require.
     * Note: we ignore report id because the mccr devices don't use it */
    switch (tag) {
    case 0b0000:
        process_usage_page (ctx, value);
        break;
    case 0b0111:
        process_report_size (ctx, value);
        break;
    case 0b1001:
        process_report_count (ctx, value);
        break;
    }
}

static void
process_usage (parse_context_t *ctx,
               uint32_t         value)
{
    usage_t item;

    memset (&item, 0, sizeof (usage_t));
    item.id = value;

    append_to_usage_array (&ctx->wip_usages, &ctx->wip_usages_size, &ctx->wip_usages_allocated,
                           &item, 1);
    if (!ctx->wip_usages) {
        mccr_log ("error: couldn't append usage to array");
        ctx->fatal_error = 1;
    }
}

static void
parse_item_local (parse_context_t *ctx,
                  uint8_t          tag,
                  uint32_t         value)
{
    mccr_log ("%*s%s (0x%x: %u)",
              ctx->log_indent, "",
              (tag < (sizeof (local_tag_str) / sizeof (local_tag_str[0]))) ? local_tag_str[tag] : "reserved",
              value, value);

    /* Usage */
    if (tag == 0b0000)
        process_usage (ctx, value);
}

static void
parse_report_descriptor (parse_context_t *ctx,
                         const uint8_t   *desc,
                         size_t           desc_size)
{
    size_t i = 0;

    mccr_log ("---------------------------------");

    while (i < desc_size && !ctx->fatal_error) {
        uint8_t  prefix, data_size, type, tag;
        uint32_t value = 0;

        prefix = desc[i];

        /* Long item */
        if (prefix == 0b11111110) {
            if (i + 1 >= desc_size) {
                mccr_log ("warning: invalid long item in report descriptor");
                break;
            }
            data_size = desc[i + 1];
            mccr_log ("long item size '%u'", data_size);
            goto next_item;
        }

        /* Short item */
        type = (prefix & 0b00001100) >> 2;
        tag  = (prefix & 0b11110000) >> 4;
        data_size = prefix & 0b11;
        if (data_size == 0b11)
            data_size = 4;

        if (i + data_size >= desc_size) {
            mccr_log ("short item type '%s', tag '0x%02x', size '%u': <invalid data>",
                      type_str[type],
                      tag,
                      data_size);
            goto next_item;
        }

        memcpy (&value, &desc[i + 1], data_size);
        value = le32toh (value);

        switch (type) {
        case 0b00:
            parse_item_main (ctx, tag, value);
            break;
        case 0b01:
            parse_item_global (ctx, tag, value);
            break;
        case 0b10:
            parse_item_local (ctx, tag, value);
            break;
        case 0b11:
            mccr_log ("short item type '%s', tag '0x%02x', size '%u': 0x%x",
                      type_str[type], tag, data_size, value);
            break;
        }

    next_item:
        i += (data_size + 1);
    };

    mccr_log ("---------------------------------");
}

static void
process_report (parse_context_t *ctx,
                const char      *report_name,
                const char      *(id_to_string) (uint8_t usage_id),
                report_t        *report)
{
    uint32_t offset_bits = 0;
    size_t i;

    if (ctx->fatal_error)
        return;

    mccr_log ("processing %s report:", report_name);

    if (report->usages_size == 0) {
        mccr_log ("error: no usages defined in %s report", report_name);
        ctx->fatal_error = true;
        return;
    }

    for (i = 0; i < report->usages_size; i++) {
        usage_t *usage;

        usage = (usage_t *)&(report->usages[i]);
        usage->offset_bits = offset_bits;

        mccr_log ("  usage 0x%02x (%s) available in %s report: offset %u bytes (+%u bits), size %u bytes (+%u bits)",
                  usage->id, id_to_string (usage->id), report_name,
                  usage->offset_bits / 8, usage->offset_bits % 8,
                  usage->size_bits / 8, usage->size_bits % 8);

        offset_bits += usage->size_bits;
    }

    if (offset_bits % 8) {
        mccr_log ("error: %s report size not multiple of bytes", report_name);
        ctx->fatal_error = true;
        return;
    }

    report->size = offset_bits / 8;
    mccr_log ("  total %s report size: %u bytes", report_name, report->size);
}

mccr_status_t
mccr_parse_report_descriptor (const uint8_t                     *desc,
                              size_t                             desc_size,
                              mccr_report_descriptor_context_t **out_ctx)
{
    parse_context_t ctx;

    /* Prepare context */
    memset (&ctx, 0, sizeof (ctx));
    ctx.desc_ctx = calloc (sizeof (mccr_report_descriptor_context_t), 1);
    ctx.desc_ctx->refcount = 1;

    /* Parse descriptor */
    parse_report_descriptor (&ctx, desc, desc_size);

    /* Process reports */
    process_report (&ctx, "input",   input_usage_id_to_string,   &(ctx.desc_ctx->input));
    process_report (&ctx, "feature", feature_usage_id_to_string, &(ctx.desc_ctx->feature));

    if (ctx.fatal_error) {
        parse_context_clear (&ctx);
        return MCCR_STATUS_FAILED;
    }

    /* On success, return the descriptor context */
    *out_ctx = mccr_report_descriptor_context_ref (ctx.desc_ctx);
    parse_context_clear (&ctx);
    return MCCR_STATUS_OK;
}
