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

#if !defined MCCR_LOG_H
# define MCCR_LOG_H

#include <stdbool.h>

/******************************************************************************/
/* Logging */

void mccr_log_full     (pthread_t   thread_id,
                        const char *fmt,
                        ...);
void mccr_log_raw_full (pthread_t   thread_id,
                        const char *prefix,
                        const void *mem,
                        size_t      size);

#define mccr_log(...) mccr_log_full (pthread_self (), ## __VA_ARGS__ )
#define mccr_log_raw(prefix, mem,size) mccr_log_raw_full (pthread_self (), prefix, mem, size)

bool mccr_log_is_enabled (void);

#endif /* MCCR_LOG_H */
