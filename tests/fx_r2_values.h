/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The values inside the committed Release 2 project fixtures (tests/fixtures/r2_projects/): written by tests/gen_r2_fixtures.c, expected by tests/fx_legacy_test.c.
 * Changing a number here means regenerating the fixtures (see the README there); the test then fails until both agree. */
#pragma once
/* the reverb fixtures: TYPE = REV_TYPE[k] (SPRING 1, SHIMMER 2, RESO 3, CLOUD 4); every knob away from its default and inside its mode's range:
 * SIZE, DAMP (FOLW 3 under RESO), PRE (AIR -7 under SHIMMER, PITCH 5 under RESO), MOD, RATE, FILT, WIDE */
static const int16_t FXR2_KN[4][7] = {{77, 41, 33, 55, 88, -20, 66}, {100, 25, -7, 44, 12, 30, 90}, {64, 3, 5, 99, 50, -45, 10}, {120, 80, 11, 7, 101, 12, 127}};
static const char *const FXR2_REV_FILE[4] = {"reverb_spring.bin", "reverb_shimmer.bin", "reverb_reso.bin", "reverb_cloud.bin"};
/* the CLAMP fixture: RESO with G_RPRE 30 (a PRE of 30 ms stored before the player chose RESO; RESO's PITCH is 0 .. 14) */
static const int16_t FXR2_CLAMP[7] = {64, 3, 30, 99, 50, -45, 10};
#define FXR2_CLAMP_FILE "reverb_reso_clamp.bin"
/* the DUB fixture: track 1 holds DUB with a value on every page. The ids are the stored P_DUB_* (FDBK SEND FILT ECHO TIML TIMR SPRG SPD) and the four fields packed in MODE and TAPE */
#define FXR2_DUB_FILE "dub_all_pages.bin"
static const int16_t FXR2_DUB[8] = {91, 100, 33, 77, 40, 90, 55, 100};   /* the DUB and DUB 2 pages' knobs and TIM L / TIM R of DUB SETUP, in the order above */
#define FXR2_DUB_SYNC 0                                                  /* FREE: TIM L / TIM R are the free times */
#define FXR2_DUB_PING 1
#define FXR2_DUB_DRIVE 11
#define FXR2_DUB_WOW 7
