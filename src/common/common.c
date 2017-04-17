/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Copyright (C) 2017 Zodiac Inflight Innovations, Inc.
 * All rights reserved.
 *
 * Author: Aleksander Morgado <aleksander@aleksander.es>
 */

#include <malloc.h>
#include <stdint.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include "common.h"

char *
strhex (const void *mem,
        size_t      size,
        const char *delimiter)
{
    const uint8_t *data = mem;
    size_t         i, j, new_str_length, delimiter_length;
    char          *new_str;

    assert (size > 0);

    /* Allow delimiters of arbitrary sizes, including 0 */
    delimiter_length = (delimiter ? strlen (delimiter) : 0);

    /* Get new string length. If input string has N bytes, we need:
     * - 1 byte for last NUL char
     * - 2N bytes for hexadecimal char representation of each byte...
     * - N-1 times the delimiter length
     * So... e.g. if delimiter is 1 byte,  a total of:
     *   (1+2N+N-1) = 3N bytes are needed...
     */
    new_str_length =  1 + (2 * size) + ((size - 1) * delimiter_length);

    /* Allocate memory for new array and initialize contents to NUL */
    new_str = calloc (new_str_length, 1);

    /* Print hexadecimal representation of each byte... */
    for (i = 0, j = 0; i < size; i++, j += (2 + delimiter_length)) {
        /* Print character in output string... */
        snprintf (&new_str[j], 3, "%02X", data[i]);
        /* And if needed, add separator */
        if (delimiter_length && i != (size - 1) )
            strncpy (&new_str[j + 2], delimiter, delimiter_length);
    }

    /* Set output string */
    return new_str;
}

char *
strhex_multiline (const void *mem,
                  size_t      size,
                  size_t      max_bytes_per_line,
                  const char *line_prefix,
                  const char *delimiter)
{
    const uint8_t *data = mem;
    size_t         i, j, new_str_length, line_prefix_length, n_line_prefixes, delimiter_length;
    char          *new_str;

    assert (size > 0);

    line_prefix_length = (line_prefix ? strlen (line_prefix) : 0);
    n_line_prefixes = size / max_bytes_per_line;

    /* Allow delimiters of arbitrary sizes, including 0 */
    delimiter_length = (delimiter ? strlen (delimiter) : 0);

    new_str_length = 1 + (2 * size) + (n_line_prefixes * line_prefix_length) + ((size - 1) * delimiter_length);
    new_str = calloc (new_str_length + 1, 1);

    /* Print hexadecimal representation of each byte... */
    for (i = 0, j = 0; i < size; i++) {
        /* Print character in output string... */
        snprintf (&new_str[j], 3, "%02X", data[i]);
        /* And if needed, add separator or EOL + prefix*/
        if (i != (size - 1)) {
            if ((i + 1) % max_bytes_per_line == 0) {
                new_str[j + 2] = '\n';
                j += 3;
                if (line_prefix_length) {
                    strncpy (&new_str[j], line_prefix, line_prefix_length);
                    j += line_prefix_length;
                }
            } else {
                j += 2;
                if (delimiter_length) {
                    strncpy (&new_str[j], delimiter, delimiter_length);
                    j += delimiter_length;
                }
            }
        }
    }

    return new_str;
}

static const long hextable[] = {
   [0 ... 255] = -1,
   ['0'] = 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
   ['A'] = 10, 11, 12, 13, 14, 15,
   ['a'] = 10, 11, 12, 13, 14, 15
};

ssize_t
strbin (const char *str,
        uint8_t    *buffer,
        size_t      buffer_size)
{
    size_t str_len;
    size_t i = 0, j = 0;

    str_len = strlen (str);

    while (i < str_len) {
        long xdigith;
        long xdigitl;

        if (str[i] == ' ' || str[i] == '\n' || str[i] == ':') {
            i++;
            continue;
        }

        if (j >= buffer_size)
            return (ssize_t) -1;

        xdigith = hextable [(int) str[i++]];
        if (xdigith < 0)
            return (ssize_t) -2;

        if (i >= str_len)
            return (ssize_t) -3;

        xdigitl = hextable [(int) str[i++]];
        if (xdigitl < 0)
            return (ssize_t) -4;

        buffer[j++] = xdigith << 4 | xdigitl;
    }

    return (ssize_t) j;
}

char *
strascii (const void *mem,
          size_t      size)
{
    const uint8_t *data = mem;
    size_t i;
    char *new_str;

    /* Allocate memory for new array and initialize contents to NUL */
    new_str = calloc (size + 1, 1);

    /* Print ASCII representation of each byte (if possible)... */
    for (i = 0; i < size; i++) {
        if (isprint (data[i]))
            new_str[i] = data[i];
        else
            new_str[i] = '#';
    }

    /* Set output string */
    return new_str;
}
