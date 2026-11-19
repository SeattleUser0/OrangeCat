/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (ui-fix2, close-out 2) NOTES' knobs as the build has them, for the tests of the note editing that run both ways:
 *   the step pages (always on since step 9): PRESETS browses the notes and steps, KNOB 1 PITCH, 2 START, 3 LEN, 4 VEL;
 *                   ENV held + PRESETS the roll's zoom
 * Needs ui_test.c's turn / frame / press. A gesture a test means is named here once, not spelt twice. */
#define NK_BROWSE EN_PRESET
#define NK_PITCH  EN_K1
#define NK_LEN EN_K3
#define NK_VEL EN_K4
static void nk_zoom(int32_t s)                    /* the roll's zoom */
{
    fm1_in.buttons |= 1u << panel.btn[B_ENV]; host_pressed |= 1u << panel.btn[B_ENV]; frame();
    turn(EN_PRESET, s);
    fm1_in.buttons &= ~(1u << panel.btn[B_ENV]); frame();
}
