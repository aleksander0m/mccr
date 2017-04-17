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

#define _GNU_SOURCE
#include <malloc.h>
#include <stdarg.h>
#include <stdint.h>

#include <common.h>

#include "mccr.h"
#include "mccr-log.h"

/******************************************************************************/
/* Logging */

static mccr_log_handler_t default_handler;

bool
mccr_log_is_enabled (void)
{
    return !!default_handler;
}

void
mccr_log_set_handler (mccr_log_handler_t handler)
{
    default_handler = handler;
}

void
mccr_log_full (pthread_t   thread_id,
               const char *fmt,
               ...)
{
    char    *message;
    va_list  args;

    if (!default_handler)
        return;

    va_start (args, fmt);
    if (vasprintf (&message, fmt, args) == -1)
        return;
    va_end (args);

    default_handler (thread_id, message);

    free (message);
}

void
mccr_log_raw_full (pthread_t   thread_id,
                   const char *prefix,
                   const void *mem,
                   size_t      size)
{
    char *memstr;

    if (!default_handler || !mem || !size)
        return;

    memstr = strhex (mem, size, ":");
    if (!memstr)
        return;

    mccr_log_full (thread_id, "%s (%zu bytes) %s", prefix, size, memstr);
    free (memstr);
}
