/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix2 item 2 (docs/orangecat/UI_BUILD.md "ui-fix2 2"): REC held with the sequencer stopped counts down and clears, with OC_REC_CLEAR=1.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_REC_CLEAR=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen
 *      -Ifirmware/src -Itests -o rec_clear_test tests/rec_clear_test.c -lm
 * Checked:
 *   TIMING   a tap arms REC as ever (no clear); held 0.8 s (inside the count) is KEPT: nothing cleared, REC not armed, the release no tap;
 *            held on past the count it clears; the count says 3, 2, 1 in the header; with the transport playing REC held is Capture
 *            and clears nothing; another button or a key during the count cancels it
 *   HOME     the selected track's steps and notes (manual and recorded) go, the other tracks stay; EDIT held brings them back
 *   SEQ      FEEL: the 16 steps of the cursor's page back to 100 %, no condition, x1, no nudge, no accent or slide (the others stay);
 *            PHRASE: LEN DIV SWING GATE; TEMPO, CHAIN: nothing, said; one undo each
 *   FX       a track page (SENDS, DIST, DUB SETUP ..): the selected track's effects to their defaults, the sound and the steps stay, the
 *            other tracks' effects stay; a shared page (DELAY): nothing cleared, said; one undo, one redo
 *   GLO      every track's steps, recorded notes and effects; the shared effects, mix and tempo stay; ONE undo (EDIT held) brings all
 *            back (levels linked, the note history's group), one redo clears again; frozen tracks stay
 *   AUTO     MOTION: the selected track's lanes go, undo brings them back
 * Revisions:
 *   (ui-fix2 2, branch ui-fix2) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("rec_clear: %-108s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void hold_ms(uint32_t label, uint32_t ms)       /* held ms milliseconds (16 ms frames), then let go and a frame */
{
    uint32_t k;
    fm1_ms += 320u;
    btn_down(label);
    for (k = 0; k < ms / 16u; k++)
        frame();
    btn_up(label);
    frame();
}
static void pstep(track_t *t, uint32_t i, uint32_t note)
{
    t->step[i].n = 1; t->step[i].note[0] = (uint8_t)note; t->step[i].time = ST_NOTE; t->step[i].vel = 96;
}
static uint32_t steps_on(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NSTEP; i++) n += t->step[i].n != 0; return n; }
static void fresh(void)
{
    uint32_t k;
    ui_power_on();
    stop_transport();
    for (k = 0; k < NTRK; k++) { track_defaults_steps(&trk[k]); trk[k].p[P_SLEN] = 16; }
    song.rec = 0; transport_req = 0;
    ui.menu = 0; ui.confirm = 0;
}
static void put_event(track_t *t, uint32_t i, uint32_t step, uint32_t note)
{
    recorded_note_t r = {.step = (uint8_t)step, .on = 0, .note = (uint8_t)note, .vel = 100, .duration = RECORD_UNIT / 2u,
                         .owner = (uint8_t)recording_owner(t), .length = 1};
    recording_restore_note(t, i, r);
}
static uint32_t pool_n(void) { uint32_t i, n = 0; for (i = 0; i < RECORD_MAX; i++) n += recording[i].vel != 0; return n; }

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* TIMING */
        fresh();
        go_home();
        pstep(&trk[0], 2, 60);
        press(B_REC);
        chk("TIMING  a tap arms REC as ever, clears nothing", (song.rec & 1u) && steps_on(&trk[0]) == 1u);
        press(B_REC);
        song.rec = 0;
        hold_ms(B_REC, 800);
        chk("TIMING  held 0.8 s: KEPT, nothing cleared, REC not armed, the release no tap", steps_on(&trk[0]) == 1u && !song.rec && msg_is("KEPT"));
        {
            uint32_t k, saw3 = 0, saw2 = 0, saw1 = 0;
            fm1_ms += 320u;
            btn_down(B_REC);
            for (k = 0; k < 120u && steps_on(&trk[0]); k++) {
                frame();
                saw3 |= msg_has("CLEARING TRACK 1 3");
                saw2 |= msg_has("CLEARING TRACK 1 2");
                saw1 |= msg_has("CLEARING TRACK 1 1");
            }
            btn_up(B_REC);
            frame();
            chk("TIMING  held on: the header counts 3, 2, 1 and the track clears", saw3 && saw2 && saw1 && steps_on(&trk[0]) == 0u && msg_has("TRACK 1 CLEARED"));
            chk("TIMING  the release of the clearing hold is no tap", !song.rec);
        }
        pstep(&trk[0], 2, 60);
        song.playing = 1;
        hold_ms(B_REC, 900);
        song.playing = 0;
        chk("TIMING  playing: REC held is Capture, nothing cleared", steps_on(&trk[0]) == 1u && msg_is("NOTHING TO CAPTURE"));
        {   /* another button during the count cancels it */
            uint32_t k;
            fm1_ms += 320u;
            btn_down(B_REC);
            for (k = 0; k < 30u; k++) frame();
            btn_down(B_ARP); frame(); btn_up(B_ARP);
            for (k = 0; k < 120u; k++) frame();
            btn_up(B_REC); frame();
            chk("TIMING  another button during the count cancels it", steps_on(&trk[0]) == 1u);
        }
    }
    {   /* HOME */
        fresh();
        go_home();
        pstep(&trk[0], 2, 60); pstep(&trk[0], 5, 64);
        pstep(&trk[1], 3, 62);
        hold_ms(B_REC, 1500);
        chk("HOME    the selected track's steps go, another track's stay", steps_on(&trk[0]) == 0u && steps_on(&trk[1]) == 1u);
        hold(B_EDIT);
        chk("HOME    EDIT held brings them back", steps_on(&trk[0]) == 2u && trk[0].step[5].note[0] == 64);
        fresh();
        go_home();
        put_event(&trk[0], 0, 1, 60); put_event(&trk[0], 1, 4, 67);
        note_rebuild_all();
        pstep(&trk[0], 8, 61);
        hold_ms(B_REC, 1500);
        chk("HOME    recorded notes go too", pool_n() == 0u && steps_on(&trk[0]) == 0u);
        hold(B_EDIT);
        chk("HOME    EDIT held brings the recorded notes back", pool_n() == 2u && trk[0].step[8].note[0] == 61);
    }
    {   /* SEQ pages */
        uint32_t i;
        fresh();
        go_title("FEEL");
        for (i = 0; i < 20u; i++) { pstep(&trk[0], i, 60); step_set_chance(&trk[0].step[i], 50u); step_set_nudge(&trk[0].step[i], 3); }
        trk[0].p[P_SLEN] = 32;
        ui.cursor = 3;
        hold_ms(B_REC, 1500);
        chk("FEEL    the cursor's 16 steps back to the defaults", step_chance(&trk[0].step[3]) == 100u && trk[0].step[3].nudge == 0 && trk[0].step[15].nudge == 0);
        chk("FEEL    the steps of the next page stay", step_chance(&trk[0].step[16]) == 50u && trk[0].step[16].nudge == 3);
        chk("FEEL    the notes themselves stay", steps_on(&trk[0]) == 20u);
        hold(B_EDIT);
        chk("FEEL    EDIT held brings the values back", step_chance(&trk[0].step[3]) == 50u && trk[0].step[3].nudge == 3);
        fresh();
        go_title("PHRASE");
        trk[0].p[P_SLEN] = 32; trk[0].p[P_SDIV] = 3; trk[0].p[P_SSWING] = 20; trk[0].p[P_SGATE] = 40;
        hold_ms(B_REC, 1500);
        chk("PHRASE  LEN DIV SWING GATE to their defaults", trk[0].p[P_SLEN] == param_desc_of(eng_idx(trk[0].eng_req), P_SLEN)->def &&
            trk[0].p[P_SSWING] == param_desc_of(eng_idx(trk[0].eng_req), P_SSWING)->def && msg_is("PHRASE RESET"));
        hold(B_EDIT);
        chk("PHRASE  EDIT held brings them back", trk[0].p[P_SLEN] == 32 && trk[0].p[P_SDIV] == 3 && trk[0].p[P_SSWING] == 20 && trk[0].p[P_SGATE] == 40);
        fresh();
        go_title("TEMPO");
        song.g[G_BPM] = 99;
        hold_ms(B_REC, 1500);
        chk("TEMPO   the project's own page: nothing reset", song.g[G_BPM] == 99);
    }
    {   /* FX */
        fresh();
        go_title("DIST");
        pstep(&trk[0], 1, 60);
        trk[0].p[P_DIST] = 70; trk[0].p[P_REV] = 55; trk[0].p[P_JRES0] = 33; trk[0].p[P_DUBLEN] = 2; trk[0].p[P_APDEP] = 40;
        trk[1].p[P_DIST] = 66; trk[0].p[P_LEVEL] = 80;
        hold_ms(B_REC, 1500);
        chk("FX      the track's effects to their defaults", trk[0].p[P_DIST] == param_desc_of(eng_idx(trk[0].eng_req), P_DIST)->def &&
            trk[0].p[P_REV] == param_desc_of(eng_idx(trk[0].eng_req), P_REV)->def && trk[0].p[P_DUBLEN] == param_desc_of(eng_idx(trk[0].eng_req), P_DUBLEN)->def &&
            trk[0].p[P_APDEP] == param_desc_of(eng_idx(trk[0].eng_req), P_APDEP)->def);
        chk("FX      the steps, the mix and another track's effects stay", steps_on(&trk[0]) == 1u && trk[0].p[P_LEVEL] == 80 && trk[1].p[P_DIST] == 66);
        hold(B_EDIT);
        chk("FX      EDIT held brings them back", trk[0].p[P_DIST] == 70 && trk[0].p[P_REV] == 55 && trk[0].p[P_JRES0] == 33 && trk[0].p[P_DUBLEN] == 2 && trk[0].p[P_APDEP] == 40);
        edit_redo();
        chk("FX      EDIT + OCT+ redoes the reset", trk[0].p[P_DIST] == param_desc_of(eng_idx(trk[0].eng_req), P_DIST)->def);
        fresh();
        go_title("DELAY");
        trk[0].p[P_DIST] = 70; song.g[G_DTIME] = 5;
        hold_ms(B_REC, 900);
        chk("FX      a shared page: nothing cleared, the message says where its reset is", trk[0].p[P_DIST] == 70 && song.g[G_DTIME] == 5 && msg_has("SHARED"));
    }
    {   /* GLO */
        uint32_t k;
        fresh();
        go_title("GLOBAL");
        for (k = 0; k < NTRK; k++) { pstep(&trk[k], 1 + k, 60 + k); trk[k].p[P_DIST] = 30 + (int16_t)k; trk[k].p[P_LEVEL] = 70 + (int16_t)k; }
        song.g[G_DTIME] = 5; song.g[G_BPM] = 111;
        hold_ms(B_REC, 1500);
        chk("GLO     every track's steps and effects go", steps_on(&trk[0]) + steps_on(&trk[1]) + steps_on(&trk[2]) + steps_on(&trk[3]) == 0u &&
            trk[2].p[P_DIST] == param_desc_of(eng_idx(trk[2].eng_req), P_DIST)->def);
        chk("GLO     the mix, the shared effects and the tempo stay", trk[3].p[P_LEVEL] == 73 && song.g[G_DTIME] == 5 && song.g[G_BPM] == 111);
        hold(B_EDIT);
        chk("GLO     ONE EDIT held brings all four tracks back", steps_on(&trk[0]) == 1u && steps_on(&trk[1]) == 1u && steps_on(&trk[2]) == 1u && steps_on(&trk[3]) == 1u &&
            trk[3].p[P_DIST] == 33 && trk[0].p[P_DIST] == 30);
        edit_redo();
        chk("GLO     one redo clears again", steps_on(&trk[0]) + steps_on(&trk[1]) + steps_on(&trk[2]) + steps_on(&trk[3]) == 0u && trk[1].p[P_DIST] == param_desc_of(eng_idx(trk[1].eng_req), P_DIST)->def);
        /* with recorded notes on two tracks */
        fresh();
        go_title("GLOBAL");
        put_event(&trk[1], 0, 2, 60); put_event(&trk[2], 1, 5, 64); put_event(&trk[2], 2, 6, 66);
        song.sel = 1; note_rebuild_all(); song.sel = 2; note_rebuild_all(); song.sel = 0;
        pstep(&trk[3], 4, 70); trk[1].p[P_REV] = 44;
        hold_ms(B_REC, 1500);
        chk("GLO     recorded notes of two tracks go", pool_n() == 0u && steps_on(&trk[1]) == 0u && steps_on(&trk[2]) == 0u && steps_on(&trk[3]) == 0u);
        hold(B_EDIT);
        chk("GLO     ONE EDIT held brings the recorded notes of both back, the manual step, the effects", pool_n() == 3u && steps_on(&trk[3]) == 1u && trk[1].p[P_REV] == 44 &&
            steps_on(&trk[2]) == 2u);
        edit_redo();
        chk("GLO     one redo clears them again", pool_n() == 0u && steps_on(&trk[2]) == 0u);
    }
    {   /* AUTOMATION */
        fresh();
        go_title("MOTION");
        motion_set_event(&trk[0], 4, P_PAN, 10);
        motion_set_event(&trk[1], 6, P_PAN, 20);
        hold_ms(B_REC, 1500);
        chk("AUTO    the selected track's lanes go, another's stay", motion_count(&trk[0]) == 0u && motion_count(&trk[1]) == 1u);
        hold(B_EDIT);
        chk("AUTO    EDIT held brings them back", motion_count(&trk[0]) == 1u);
    }
    printf("rec_clear: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
}
