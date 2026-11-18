#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# The flash bytes of the main screen's three layouts (docs/orangecat/UI_DESIGN.md "The main screen"): tests/ui_main_screens.h compiled for the
# unit (pi32v2, -Os, as the firmware is) with gfx.c and icons.c beside it, then the sizes of the layouts' own functions (-fno-inline keeps each apart).
#   JIELI_TOOLCHAIN=.../jieli/toolchain tools/ui_main_size.sh
set -e
TC=${JIELI_TOOLCHAIN:?}; T=$(mktemp -d)
cat > $T/p.c <<'EOF'
#include <stdint.h>
#include <string.h>
typedef struct { int x; } track_t; typedef struct { int x; } param_desc_t; typedef struct { int x; } page_t;
#define NTRK 4u
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p) { (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "gfx.c"
#include "icons.c"
#include "ui_main_screens.h"
static void ms_dial(int32_t cx, int32_t cy, int32_t d, uint32_t k, int32_t q8, int macro) { (void)cx; (void)cy; (void)d; (void)k; (void)q8; (void)macro; }   /* (the picture page's dial is in the image already: hp_dial) */
void probe_1(const ms_t *m) { ms_draw_1(m); }
void probe_2(const ms_t *m) { ms_draw_2(m); }
void probe_3(const ms_t *m) { ms_draw_3(m); }
EOF
$TC/pi32v2/bin/clang -target pi32v2 -mcpu=r3 -mfprev1 -ffp-contract=off -Os -fno-inline -ffunction-sections -fno-builtin -w -Ibuild/gen -Ifirmware/hal -Ifirmware/src -Ifirmware/src/fatfs/sys -Itests -I$T -c $T/p.c -o $T/p.o
$TC/pi32v2/bin/nm -S $T/p.o | python3 -c '
import sys
for l in sys.stdin:
    p = l.split()
    if len(p) == 4 and p[2] in "tT" and p[3].startswith(("ms_", "probe_")): print("%6d  %s" % (int(p[1], 16), p[3]))' | sort -k2
rm -rf $T
