/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.10: the <string.h> FatFs's ff.c includes, for the firmware build only (tools/build.py puts this
 * folder on the include path; the host tests use their C library's). The firmware has no C library: memset, memcpy
 * and memcmp are firmware/src/libc.c's, strchr and strlen firmware/src/usb_files.c's. */
#pragma once
void *memset(void *d, int c, unsigned n);
void *memcpy(void *d, const void *s, unsigned n);
int memcmp(const void *a, const void *b, unsigned n);
char *strchr(const char *s, int c);
unsigned strlen(const char *s);
