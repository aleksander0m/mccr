
# MCCR

## libmccr

`libmccr` is a small **C library** that allows controlling MagTek credit card
readers in HID mode.

## mccr-cli

`mccr-cli` is a simple program that uses libmccr to query device information or
process user swipes, printing out the swipe report information in standard
output.

## mccr-gtk

`mccr-gtk` is a GTK+ based graphical user interface program that provides swipe
information support, including track data decryption if the Base Derivation Key
is known. The program also allows performing device setting changes and key
updates using MagTek's 'Magensa' remote service system.

## License

The libmccr library is licensed under the LGPLv2.1+ license, and the mccr-cli
and mccr-gtk programs under the GPLv2+ license.

* Copyright © 2017 Zodiac Inflight Innovations
* Copyright © 2017 Aleksander Morgado <aleksander@aleksander.es>

---

* MagTek® is a registered trademark of [MagTek, Inc](https://www.magtek.com).
* MagneSafe™ and Magensa™ are trademarks of [MagTek, Inc](https://www.magtek.com).
