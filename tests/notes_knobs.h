/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (ui-fix2, close-out 2) NOTES' knobs as the build has them, for the tests of the note editing that run both ways:
 *   OC_STEP_PAGES=1 (the interface as it ships): PRESETS browses the notes and steps, KNOB 1 PITCH, 2 START, 3 LEN, 4 VEL;
 *                   ENV held + PRESETS the roll's zoom
 *   OC_STEP_PAGES=0 (release-2's, the legacy job): KNOB 1 browses, 2 PITCH, 3 LEN, 4 VEL; PRESETS the zoom
 * Needs ui_test.c's turn / frame / press. A gesture a test means is named here once, not spelt twice. */
#if OC_STEP_PAGES
#define NK_BROWSE EN_PRESET
#define NK_PITCH  EN_K1
#else
#define NK_BROWSE EN_K1
#define NK_PITCH  EN_K2
#endif
#define NK_LEN EN_K3
#define NK_VEL EN_K4
static void nk_zoom(int32_t s)                    /* the roll's zoom */
{
#if OC_STEP_PAGES
    fm1_in.buttons |= 1u << panel.btn[B_ENV]; host_pressed |= 1u << panel.btn[B_ENV]; frame();
    turn(EN_PRESET, s);
    fm1_in.buttons &= ~(1u << panel.btn[B_ENV]); frame();
#else
    turn(EN_PRESET, s);
#endif
}
