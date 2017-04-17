/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA.
 *
 * Copyright (C) 2017 Zodiac Inflight Innovations
 * Copyright (C) 2017 Aleksander Morgado <aleksander@aleksander.es>
 *
 * Compile with:
 *  $ gcc -o example-info `pkg-config --cflags --libs mccr` example-info.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <mccr.h>

int main (int argc, char **argv)
{
  mccr_status_t        st;
  mccr_device_t       *device;
  mccr_swipe_report_t *swipe_report;
  uint8_t              track_length;
  int                  exit_code = EXIT_FAILURE;

  if ((st = mccr_init ()) != MCCR_STATUS_OK) {
    fprintf (stderr, "error: couldn't initialize MCCR library: %s\n", mccr_status_to_string (st));
    return exit_code;
  }

  device = mccr_device_new (NULL);
  if (!device) {
    fprintf (stderr, "error: couldn't find a MagTek credit card reader\n");
    goto out_exit;
  }

  if ((st = mccr_device_open (device)) != MCCR_STATUS_OK) {
    fprintf (stderr, "error: couldn't open MagTek credit card reader: %s\n", mccr_status_to_string (st));
    goto out_device_unref;
  }

  if ((st = mccr_device_wait_swipe_report (device, -1, &swipe_report)) != MCCR_STATUS_OK) {
    fprintf (stderr, "error: cannot get swipe report: %s\n", mccr_status_to_string (st));
    goto out_device_close;
  }

  printf ("swipe detected\n");
  if ((st = mccr_swipe_report_get_track_1_encrypted_data_length (swipe_report, &track_length)) != MCCR_STATUS_OK)
    printf ("  %u bytes detected in track 1\n", track_length);
  if ((st = mccr_swipe_report_get_track_2_encrypted_data_length (swipe_report, &track_length)) != MCCR_STATUS_OK)
    printf ("  %u bytes detected in track 2\n", track_length);
  if ((st = mccr_swipe_report_get_track_3_encrypted_data_length (swipe_report, &track_length)) != MCCR_STATUS_OK)
    printf ("  %u bytes detected in track 3\n", track_length);

  st = EXIT_SUCCESS;

  mccr_swipe_report_free (swipe_report);
 out_device_close:
  mccr_device_close (device);
 out_device_unref:
  mccr_device_unref (device);
 out_exit:
  mccr_exit ();

  return exit_code;
}
