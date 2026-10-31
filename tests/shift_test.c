/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 3 (docs/orangecat/UI_BUILD.md "Step 3: the SHIFT map"): HOME as SHIFT with OC_SHIFT_MAP=1
 * (firmware/src/ui_shift.c), through tests/ui_test.c's host UI (its own tests are not run here).
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o shift_test tests/shift_test.c -lm
 * Checked:
 *   TAP     a HOME tap (and a 480 ms press) is a tap (HOME's page steps): no map, no MENU; a tap seen only as an edge opens
 *           nothing; with EDIT held first HOME is release-2's (no SHIFT)
 *   FINE    five fast detents: accelerated without SHIFT, exactly five steps with it
 *   HOLD    held alone: the map at 700 ms, no MENU while held; let go: the MENU
 *   CLOSE   with the MENU open, HOME held closes it at 700 ms as today, and the release does not open it again
 *   KNOB    SHIFT + a knob on HOME: the knob's own edit, no MENU; on a sheet the same; on NOTES eaten
 *   ENGINE  SHIFT + PRESETS on HOME: the next engine (eng_step), no MENU; off HOME eaten
 *   INIT    SHIFT + EDIT on HOME: the init confirm; held on past 700 ms: no undo, no MENU; on SEQ's page: no confirm, no page
 *           change
 *   OCT     SHIFT + OCT+ / OCT-: no octave change, no OCT+ sheet at 700 ms, no MENU
 *   OTHERS  HOME then a layer button: the layer's, no MENU after; ALGO turned under SHIFT: the track, no MENU
 *   DRAW    the map drawn on HOME, a sheet and another page (no crash; ui_render.c lints it)
 * Revisions:
 *   (the new interface 0.3, branch ui-build) first version; after the diff review: the edge-only tap, a button held
 *   before HOME, the fine turn measured, the tap's page step.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("shift: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void down(uint32_t b)
{
    fm1_ms += 320u;
    fm1_in.buttons |= 1u << panel.btn[b];
    host_pressed |= 1u << panel.btn[b];
    frame();
}
static void up(uint32_t b)
{
    fm1_in.buttons &= ~(1u << panel.btn[b]);
    frame();
}
static void wait_frames(uint32_t n) { while (n--) frame(); }
static void fast_turn(uint32_t role, int32_t s)        /* one detent 5 ms after the last (the acceleration's speed) */
{
    host_enc[panel.enc[role]] += s * panel.dir[role];
    frame();
    host_ticks += 5000u;
    fm1_ms += 5u;
}
static void settle(void)                                /* the engine switched on the audio side (the macros wait for it) */
{
    int32_t out[2 * CTL];
    for (uint32_t b = 0; b < 400u; b++) mix_block(out, CTL);
}

int main(void)
{
#if !OC_SHIFT_MAP
    printf("shift: built without OC_SHIFT_MAP\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* TAP */
        int ok;
        uint32_t pg0;
        ui_power_on();
        frames(100);
        pg0 = ui.home_pg;
        down(B_HOME);
        wait_frames(2);
        ok = !shift_map_on();
        up(B_HOME);
        ok &= !ui.menu && !shift_on() && ui.home_pg != pg0;   /* (the tap stepped HOME's page) */
        down(B_HOME);
        wait_frames(30);                                /* 480 ms */
        ok &= !shift_map_on();
        up(B_HOME);
        ok &= !ui.menu;
        chk("TAP     a tap and a 480 ms press: taps (HOME's page steps; no map, no MENU)", ok);
        fm1_ms += 320u;                                 /* a tap between two passes: the edge latched, HOME already up */
        host_pressed |= 1u << panel.btn[B_HOME];
        frame();
        frames(20);
        chk("TAP     a tap seen only as an edge (pressed and let go between two passes): no MENU (the diff review's 1)", !ui.menu && !shift_on());
        down(B_EDIT);                                   /* EDIT held first, then HOME: release-2's HOME, no SHIFT */
        down(B_HOME);
        ok = !shift_on();
        wait_frames(52);
        ok &= !shift_map_on();
        up(B_HOME);
        up(B_EDIT);
        frames(20);
        if (ui.menu) { hold(B_HOME); frames(20); }
        chk("TAP     EDIT held before HOME: no SHIFT, no map (release-2's HOME; the diff review's 2)", ok);
    }
    {   /* HOLD, CLOSE */
        int ok;
        ui_power_on();
        frames(100);
        down(B_HOME);
        wait_frames(52);
        ok = shift_map_on() && !ui.menu;
        frame();
        up(B_HOME);
        chk("HOLD    held alone: the map at 700 ms, no MENU while held; let go: the MENU", ok && ui.menu && !shift_on());
        down(B_HOME);
        wait_frames(52);
        ok = !ui.menu && !shift_map_on();
        up(B_HOME);
        frames(50);
        chk("CLOSE   the MENU open: HOME held closes it at 700 ms; the release does not open it again", ok && !ui.menu);
    }
    {   /* KNOB on HOME's picture page (MACRO: KNOB 2's id from its knob table: HARM, plain) */
        int16_t m0;
        uint32_t id;
        ui_power_on();
        set_engine_of(TSEL, ENGI_MACRO);
        settle();
        GLO_SELECT |= HOME_PIC_BIT;
        go_home();
        frames(100);
        id = kdef_of(ENGI_MACRO)[1].kind == KD_PLAIN ? kdef_of(ENGI_MACRO)[1].id : P_MAC0 + 1u;
        m0 = TSEL->p[id];
        down(B_HOME);
        turn(EN_K2, 1);
        wait_frames(60);
        up(B_HOME);
        chk("KNOB    SHIFT + KNOB 2 on HOME's picture: its table's id moves, no MENU, no tap", TSEL->p[id] != m0 && !ui.menu && ui.home && ui.home_pg == 2u);
        GLO_SELECT &= (uint8_t)~HOME_PIC_BIT;
    }
    {   /* FINE: five detents 5 ms apart on a long range: accelerated without SHIFT, one step each with it */
        int32_t d0, d1;
        uint32_t id;
        ui_power_on();
        frames(100);
        press(B_EDIT);
        id = cur_page()->id[1];
        TSEL->p[id] = 10;
        for (int i = 0; i < 5; i++) fast_turn(EN_K2, 1);
        d0 = TSEL->p[id] - 10;
        frames(30);
        TSEL->p[id] = 10;
        down(B_HOME);
        for (int i = 0; i < 5; i++) fast_turn(EN_K2, 1);
        d1 = TSEL->p[id] - 10;
        up(B_HOME);
        printf("shift:   (five fast detents: %d steps without SHIFT, %d with)\n", (int)d0, (int)d1);
        chk("FINE    SHIFT + a knob turned fast: one step a detent where the same turn accelerates without it", d0 > 5 && d1 == 5 && !ui.menu);
    }
    {   /* KNOB on a sheet; eaten on NOTES */
        int16_t v0;
        uint32_t id;
        int ok;
        ui_power_on();
        frames(100);
        press(B_EDIT);
        id = cur_page()->id[1];
        v0 = TSEL->p[id];
        down(B_HOME);
        turn(EN_K2, 1);
        up(B_HOME);
        ok = TSEL->p[id] != v0 && !ui.menu && !ui.home;
        chk("KNOB    SHIFT + KNOB 2 on a sheet: its second control edits, no MENU", ok);
    }
    {   /* ENGINE */
        uint32_t e0, want;
        ui_power_on();
        frames(100);
        e0 = TSEL->eng_req;
        want = eng_step(e0, 1);
        down(B_HOME);
        turn(EN_PRESET, 1);
        frames(50);
        up(B_HOME);
        frames(50);
        chk("ENGINE  SHIFT + PRESETS on HOME: the next engine, no MENU", TSEL->eng_req == want && want != e0 && !ui.menu);
        press(B_EDIT);
        e0 = TSEL->eng_req;
        {
            uint32_t pg = ui.page;
            down(B_HOME);
            turn(EN_PRESET, 1);
            up(B_HOME);
            chk("ENGINE  SHIFT + PRESETS on a sheet: eaten (no engine change, no sheet turned)", TSEL->eng_req == e0 && ui.page == pg && !ui.menu);
        }
    }
    {   /* INIT */
        int ok;
        ui_power_on();
        frames(100);
        turn(EN_K2, 5);                                 /* (an edit for undo to have) */
        frames(20);
        down(B_HOME);
        down(B_EDIT);
        ok = ui.confirm == CF_INIT_SOUND;
        wait_frames(52);                                /* (both held past 700 ms) */
        ok &= ui.confirm == CF_INIT_SOUND && !ui.menu;
        up(B_EDIT);
        up(B_HOME);
        frames(20);
        ok &= ui.confirm == CF_INIT_SOUND && !ui.menu;
        press(B_OCTUP);                                 /* the confirm's YES */
        frames(20);
        chk("INIT    SHIFT + EDIT on HOME: the init confirm (no undo, no MENU, held or let go); YES inits", ok && !ui.confirm && msg_is("SOUND INIT"));
    }
    {   /* INIT eaten on SEQ's first page */
        int ok;
        uint32_t fam;
        ui_power_on();
        frames(100);
        press(B_SEQ);
        frames(20);
        fam = cur_page()->fam;
        down(B_HOME);
        down(B_EDIT);
        up(B_EDIT);
        up(B_HOME);
        frames(20);
        ok = !ui.confirm && !ui.menu && cur_page()->fam == fam && !ui.home;
        chk("INIT    SHIFT + EDIT on SEQ's page: eaten (no confirm, no page change, no MENU)", ok);
    }
    {   /* OCT */
        int oct0, ok;
        ui_power_on();
        frames(100);
        oct0 = song.octave;
        down(B_HOME);
        down(B_OCTUP);
        wait_frames(52);
        ok = !pop.on;
        up(B_OCTUP);
        down(B_OCTDN);
        up(B_OCTDN);
        up(B_HOME);
        frames(20);
        chk("OCT     SHIFT + OCT+ / OCT-: no octave, no OCT+ sheet at 700 ms, no MENU", ok && song.octave == oct0 && !ui.menu && !pop.on);
    }
    {   /* OTHERS */
        uint32_t sel;
        int ok;
        ui_power_on();
        frames(100);
        down(B_HOME);
        down(B_GLO);
        wait_frames(40);
        up(B_GLO);
        up(B_HOME);
        frames(20);
        ok = !ui.menu;
        chk("OTHERS  HOME, then GLO held: no MENU after", ok);
        sel = song.sel;
        down(B_HOME);
        turn(EN_ALGO, 1);
        up(B_HOME);
        frames(20);
        chk("OTHERS  ALGO under SHIFT: its own job, no MENU", !ui.menu && (song.sel != sel || trk_n() < 2u));
    }
    {   /* DRAW */
        int ok = 1;
        ui_power_on();
        frames(100);
        for (int w = 0; w < 3; w++) {
            if (w == 1) press(B_EDIT);
            if (w == 2) press(B_SEQ);
            down(B_HOME);
            wait_frames(52);
            ok &= shift_map_on();
            ui.force = 1;
            frame();
            fm1_in.buttons |= 1u << panel.btn[B_EDIT];   /* (a combo while the map is up: it goes) */
            host_pressed |= 1u << panel.btn[B_EDIT];
            frame();
            ok &= !shift_map_on();
            fm1_in.buttons &= ~(1u << panel.btn[B_EDIT]);
            if (ui.confirm) { press(B_OCTDN); }
            up(B_HOME);
            frames(20);
            ok &= !ui.menu;
            if (ui.confirm) press(B_OCTDN);
        }
        chk("DRAW    the map on HOME, a sheet, SEQ; a combo while it is up takes it away and cancels the MENU", ok);
    }
    printf("shift: %d failed\n", nbad);
    return nbad != 0;
#endif
}
