/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.14.1, branch linux-hw) Host test of the EDIT tap on SEQ > TRACKER, through the UI test's stubs (ui_test.c).
 *
 * EDIT is a step modifier there: its release turns the picked effect off (ui_input.c, the step-mods release, step_delete_edit), and a tap
 * with no step modifier held opens the EDIT pages (page_tap). btn_hold and the release read fm1_in.buttons at two points of one UI frame,
 * and the scan interrupt (TIMER5) changes it whenever its debounce ends. On the unit (hw/LOG.md, Part 3) a release that landed between the
 * two did both: "CHANCE OFF" and the page EDIT 1. Here the release is published by a stand-in for that interrupt at an encoder read the
 * frame makes after btn_hold and before the release (EN_ALGO, ui_input.c's ALGORITHM knob), and at the frame's start as the control. Either
 * way exactly one thing must happen: the effect off, the page still TRACKER. */
#include <stdint.h>
static void race_take_hook(uint32_t e);
#define UI_ENC_TAKE_HOOK(e) race_take_hook(e)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t race_release;                     /* a button bit the "interrupt" lets go at the next ALGORITHM read */
static void race_take_hook(uint32_t e)
{
    if (race_release && e == panel.enc[EN_ALGO]) {
        fm1_in.buttons &= ~race_release;
        race_release = 0;
    }
}

/* the tracker on a step with CHANCE picked and set (as test_tracker leaves its FX column) */
static track_t *tracker_chance(void)
{
    track_t *t;
    ui_power_on();
    stop_transport();
    go_title("TRACKER"); frame();
    t = TSEL;
    track_defaults_steps(t);
    ui.tk_col = TKC_N1; ui.tk_fx = TFX_RATCH;
    tap_key(white(0)); frame();
    turn(EN_K1, -1);
    turn(EN_K2, 9);                                     /* (FX) */
    turn(EN_K3, 1);                                     /* CHANCE */
    turn(EN_K4, -5);
    return t;
}

static int edit_tap(int mid_frame)
{
    track_t *t = tracker_chance();
    uint32_t ed = 1u << panel.btn[B_EDIT], k;          /* (after ui_power_on: the panel's map) */
    int ok = ui.tk_col == TKC_FX && ui.tk_fx == TFX_CHANCE && step_chance(&t->step[0]) < 100u && cur_page()->graph == GR_TRACKER;
    fm1_ms += 320u;
    fm1_in.buttons |= ed;                               /* EDIT down: a frame */
    host_pressed |= ed;
    frame();
    if (mid_frame)
        race_release = ed;                              /* the release lands inside the next frame */
    else
        fm1_in.buttons &= ~ed;                          /* the release before the next frame */
    for (k = 0; k < 3u; k++)
        frame();
    ok &= !race_release;                                /* (the hook ran: the frame read ALGORITHM) */
    ok &= step_chance(&t->step[0]) == 100u && step_on(&t->step[0]);
    ok &= cur_page()->graph == GR_TRACKER && cur_page()->scope == SC_STEP && !pop.on;
    return ok;
}

int main(void)
{
    int bad = 0;
    bad += check("TRACKER: an EDIT tap turns the picked effect off and stays on TRACKER (the release between frames)", edit_tap(0));
    bad += check("TRACKER: an EDIT tap whose release lands mid-frame does the same, and does not open the EDIT pages", edit_tap(1));
    printf("tracker EDIT tap: %s\n", bad ? "FAILED" : "passed");
    return bad != 0;
}
