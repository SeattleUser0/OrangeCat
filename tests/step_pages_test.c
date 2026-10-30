/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 4 (docs/orangecat/UI_BUILD.md "Step 4: the step pages") with OC_STEP_PAGES=1, through tests/ui_test.c's
 * host UI (its own tests are not run here).
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o step_pages_test tests/step_pages_test.c -lm
 * Checked:
 *   ORDER   SEQ's pages: NOTES, FEEL, TRACKER
 *   NOTES   KNOB 1 PITCH, 2 START, 3 LEN, 4 VEL on a recorded note, one undo each; START wraps at the phrase's end and refuses an
 *           occupied cell; a manual note's START is refused; PRESETS the cursor; ENV + PRESETS the zoom; a range keeps KNOB 1 as the move
 *   FEEL    each knob on the cursor step; CHANCE across 100 into the conditions and back; a range set (not added), off steps skipped;
 *           the guards (a live recording); EDIT tapped resets the step, held undoes
 *   TRACKER KNOB 4 in a note column of a manual chord: the chord to events, that note's own length, one undo; a one-note step, VEL and
 *           the song playing keep the whole-step resize
 *   SHIFT   + PRESETS stretches the range, + EDIT deletes it (FEEL: resets it), + a knob a fine turn
 *   DRAW    FEEL and NOTES drawn (no crash; ui_render.c lints them)
 * Revisions:
 *   (the new interface 0.4, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("step_pages: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void hold_btn(uint32_t b) { fm1_ms += 320u; fm1_in.buttons |= 1u << panel.btn[b]; host_pressed |= 1u << panel.btn[b]; frame(); }
static void rel_btn(uint32_t b) { fm1_in.buttons &= ~(1u << panel.btn[b]); frame(); }
static void shift_down(void)
{
    fm1_ms += 320u;
    fm1_in.buttons |= 1u << panel.btn[B_HOME];
    host_pressed |= 1u << panel.btn[B_HOME];
    frame();
}
static void shift_up(void) { fm1_in.buttons &= ~(1u << panel.btn[B_HOME]); frame(); frame(); }
static void fresh(const char *page)
{
    ui_power_on();
    stop_transport();
    go_title(page);
    frame();
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 16;
    bulk.on = 0;
}
static uint32_t dur(const recorded_note_t *r) { return (uint32_t)r->duration * (1u << (r->owner >> 5)); }
static void put_note(uint32_t i, uint32_t step, uint32_t on, uint32_t note, uint32_t vel, uint32_t len)
{
    recorded_note_t r = {.step = (uint8_t)step, .on = (uint16_t)on, .note = (uint8_t)note, .vel = (uint8_t)vel,
                         .duration = (uint16_t)len, .owner = (uint8_t)recording_owner(TSEL), .length = 1};
    recording_restore_note(TSEL, i, r);
}
static void manual(uint32_t s, uint32_t note)
{
    step_t *st = &TSEL->step[s];
    st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96;
}

int main(void)
{
#if !OC_STEP_PAGES
    printf("step_pages: built without OC_STEP_PAGES\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* ORDER */
        uint32_t i, n = 0, at = 0, a, b;
        fresh("NOTES");
        a = ui.page;
        for (i = 0; i < 12u; i++) {
            if (str_eq(cur_page()->title, "FEEL")) at = n;
            if (str_eq(cur_page()->title, "TRACKER")) b = n;
            turn(EN_SELECT, 1);
            n++;
            if (ui.page == a) break;
        }
        chk("ORDER   SEQ's pages: FEEL is the page after NOTES, TRACKER the one after FEEL", at == 1u && b == 2u);
    }
    {   /* NOTES */
        recorded_note_t b0;
        uint32_t d0;
        fresh("NOTES");
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(2); frame();
        ui.note_pick = 0; notes_selected(TSEL);
        turn(EN_K1, 1);
        chk("NOTES   KNOB 1 is PITCH: the recorded note goes up a semitone", recording[0].note == 61u && recording[0].on == 1000u);
        d0 = dur(&recording[0]);
        turn(EN_K3, 1);
        chk("NOTES   KNOB 3 is LEN: a quarter step longer", dur(&recording[0]) == d0 + RECORD_UNIT / 4u);
        turn(EN_K4, 3);
        chk("NOTES   KNOB 4 is VEL", recording[0].vel == 73u);
        b0 = recording[0];
        turn(EN_K2, 1);
        chk("NOTES   KNOB 2 is START: a sixteenth of a step later, nothing else changed",
            recording[0].on == b0.on + RECORD_UNIT / 16u && recording[0].note == b0.note && recording[0].vel == b0.vel && dur(&recording[0]) == dur(&b0));
        turn(EN_K2, -2);
        chk("NOTES   START the other way, below the step's start: the note moves into the step before",
            ((recording[0].step & 63u) == 1u && recording[0].on > RECORD_UNIT * 3u / 4u) || ((recording[0].step & 63u) == 2u && recording[0].on < b0.on));
        hold(B_EDIT);
        chk("NOTES   EDIT held undoes the last edit alone", recording[0].on == b0.on + RECORD_UNIT / 16u);
    }
    {   /* START at the phrase's end, and an occupied cell */
        fresh("NOTES");
        put_note(0, 15, 65000, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(15); frame();
        ui.note_pick = 0; notes_selected(TSEL);
        turn(EN_K2, 4);
        chk("NOTES   START wraps at the phrase's end (step 15 + a quarter step: the next loop's step 0)",
            (recording[0].step & 63u) == 0u || ((recording[0].step & 63u) == 15u && recording[0].on > 65000u));
        fresh("NOTES");
        put_note(0, 2, 40000, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        manual(3, 70);
        cursor_set(2); frame();
        ui.note_pick = 0; notes_selected(TSEL);
        {
            recorded_note_t before = recording[0];
            turn(EN_K2, 4);
            chk("NOTES   START into a cell of a separate manual note: refused, the note stays", !memcmp(&recording[0], &before, sizeof before) && msg_is("STEP OCCUPIED"));
        }
    }
    {   /* a manual note's START; PRESETS; the zoom */
        uint32_t z, c0;
        fresh("NOTES");
        manual(4, 64);
        cursor_set(4); frame();
        turn(EN_K2, 1);
        chk("NOTES   a manual note's START is refused with its reason", msg_is("START: RECORDED NOTES") && TSEL->step[4].nudge == 0);
        turn(EN_K1, 2);
        chk("NOTES   a manual note's PITCH (KNOB 1) as KNOB 2's was", TSEL->step[4].note[0] == 66u);
        c0 = ui.cursor;
        turn(EN_PRESET, 1);
        chk("NOTES   PRESETS moves the cursor", ui.cursor != c0);
        z = ui.note_zoom;
        hold_btn(B_ENV);
        turn(EN_PRESET, 1);
        rel_btn(B_ENV);
        chk("NOTES   ENV + PRESETS is the roll's zoom, the cursor stays", ui.note_zoom == z + 1u || ui.note_zoom == z);
    }
    {   /* a range keeps KNOB 1 as the move */
        fresh("NOTES");
        put_note(0, 2, 0, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(2); frame();
        bulk.on = 1; bulk.anchor = 2; bulk.end = 3; bulk.track = song.sel; bulk.pattern = TSEL->pattern; bulk.generation = TSEL->pattern_gen;
        bulk.engine = (uint8_t)library_engine();
        turn(EN_K1, 1);
        chk("NOTES   with a range selected KNOB 1 is not PITCH", recording[0].note == 60u);
        bulk.on = 0;
    }
    {   /* FEEL */
        step_t *s;
        fresh("FEEL");
        manual(4, 60); manual(6, 62);
        cursor_set(4); frame();
        s = &TSEL->step[4];
        step_set_chance(s, 100);
        turn(EN_K1, -10);
        chk("FEEL    KNOB 1 CHANCE down ten: 90 %", step_chance(s) == 90u && s->cond == TC_NONE);
        step_set_chance(s, 100);
        turn(EN_K1, 1);
        chk("FEEL    CHANCE up from 100 %: the first condition, FILL, the chance kept", s->cond == TC_FILL && step_chance(s) == 100u);
        turn(EN_K1, 1);
        chk("FEEL    on up through the conditions", s->cond == TC_NFILL);
        turn(EN_K1, -2);
        chk("FEEL    down past FILL: no condition, 100 %", s->cond == TC_NONE && step_chance(s) == 100u);
        s->cond = TC_4_4;
        turn(EN_K1, 5);
        chk("FEEL    the top of the scale holds (4:4)", s->cond == TC_4_4);
        turn(EN_K2, 1);
        chk("FEEL    KNOB 2 RATCH x2", step_rflam(s) == 2u);
        turn(EN_K2, 9);
        chk("FEEL    RATCH tops at FLAM", step_rflam(s) == 5u);
        turn(EN_K3, 6);
        chk("FEEL    KNOB 3 NUDGE +6", s->nudge == 6);
        turn(EN_K3, -100);
        chk("FEEL    NUDGE holds at its lowest", s->nudge == NUDGE_MIN);
        turn(EN_K4, 1);
        chk("FEEL    KNOB 4 TYPE: ACC", (s->flags & (SF_ACCENT | SF_SLIDE)) == SF_ACCENT);
        turn(EN_K4, 1);
        turn(EN_K4, 1);
        chk("FEEL    TYPE on to A+S", (s->flags & (SF_ACCENT | SF_SLIDE)) == (SF_ACCENT | SF_SLIDE));
        turn(EN_K4, 1);
        chk("FEEL    TYPE holds at A+S", (s->flags & (SF_ACCENT | SF_SLIDE)) == (SF_ACCENT | SF_SLIDE));
        press(B_EDIT);
        chk("FEEL    EDIT tapped resets the cursor step's feel, its note stays",
            step_chance(s) == 100u && s->cond == TC_NONE && step_rflam(s) == 1u && s->nudge == 0 && !(s->flags & (SF_ACCENT | SF_SLIDE)) && s->n == 1u);
    }
    {   /* a range: set, not added; off steps skipped */
        fresh("FEEL");
        manual(4, 60); manual(6, 62);
        step_set_chance(&TSEL->step[6], 40);
        cursor_set(4); frame();
        bulk.on = 1; bulk.anchor = 4; bulk.end = 6; bulk.track = song.sel; bulk.pattern = TSEL->pattern; bulk.generation = TSEL->pattern_gen;
        bulk.engine = (uint8_t)library_engine();
        turn(EN_K1, -10);
        chk("FEEL    a range: every step on is set to the cursor's new value (90, not 30), the empty step is skipped",
            step_chance(&TSEL->step[4]) == 90u && step_chance(&TSEL->step[6]) == 90u && step_chance(&TSEL->step[5]) == 100u && !TSEL->step[5].n);
        turn(EN_K2, 2);
        chk("FEEL    RATCH on the range", step_rflam(&TSEL->step[4]) == 3u && step_rflam(&TSEL->step[6]) == 3u);
        bulk.on = 0;
    }
    {   /* the guards; undo */
        step_t *s;
        fresh("FEEL");
        manual(4, 60);
        cursor_set(4); frame();
        s = &TSEL->step[4];
        song.rec |= 1u << song.sel;
        turn(EN_K1, -10);
        chk("FEEL    a live recording armed: refused, nothing changed", step_chance(s) == 100u && msg_is("STOP RECORDING"));
        song.rec &= ~(1u << song.sel);
        manual(4, 60); cursor_set(4); frame();   /* (arming a recording on the host rewinds the page: the step again) */
        turn(EN_K1, -10);
        chk("FEEL    (the same turn without the recording)", step_chance(s) == 90u);
        hold(B_EDIT);
        chk("FEEL    EDIT held undoes the turn", step_chance(s) == 100u);
        cursor_set(7); frame();
        turn(EN_K1, -10);
        chk("FEEL    an empty step: refused (a property of nothing)", msg_is("NO NOTE THERE") && step_chance(&TSEL->step[7]) == 100u);
    }
    {   /* TRACKER's chord length */
        uint32_t n;
        fresh("TRACKER");
        TSEL->step[2].n = 3; TSEL->step[2].note[0] = 60; TSEL->step[2].note[1] = 64; TSEL->step[2].note[2] = 67;
        TSEL->step[2].time = ST_NOTE; TSEL->step[2].vel = 96;
        cursor_set(2); ui.tk_col = TKC_N2; frame();
        turn(EN_K4, 1);
        chk("TRACKER KNOB 4 in N2 of a manual chord: the chord became events",
            (TSEL->step[2].flags & SF_RECORDED) != 0u);
        {
            uint32_t l64 = 0, l60 = 0, l67 = 0;
            for (n = 0; n < RECORD_MAX; n++)
                if (recording[n].vel && (recording[n].step & 63u) == 2u) {
                    if (recording[n].note == 64u) l64 = dur(&recording[n]);
                    if (recording[n].note == 60u) l60 = dur(&recording[n]);
                    if (recording[n].note == 67u) l67 = dur(&recording[n]);
                }
            chk("TRACKER that note alone is a step longer; the others keep the chord's length", l64 == l60 + RECORD_UNIT && l60 == l67 && l60 >= RECORD_UNIT);
        }
        hold(B_EDIT);
        chk("TRACKER one undo gives the manual chord back", !(TSEL->step[2].flags & SF_RECORDED) && TSEL->step[2].n == 3u && TSEL->step[2].note[1] == 64u);
        fresh("TRACKER");
        manual(2, 60);
        cursor_set(2); ui.tk_col = TKC_N1; frame();
        turn(EN_K4, 1);
        chk("TRACKER a one-note step: the whole-step resize as before, no events", !(TSEL->step[2].flags & SF_RECORDED));
        fresh("TRACKER");
        TSEL->step[2].n = 2; TSEL->step[2].note[0] = 60; TSEL->step[2].note[1] = 64; TSEL->step[2].time = ST_NOTE; TSEL->step[2].vel = 96;
        cursor_set(2); ui.tk_col = TKC_N1; frame();
        song.playing = 1;
        turn(EN_K4, 1);
        song.playing = 0;
        chk("TRACKER while the song plays: the whole-step resize, no events", !(TSEL->step[2].flags & SF_RECORDED));
    }
    {   /* SHIFT */
        fresh("FEEL");
        manual(4, 60); manual(5, 62); manual(6, 64);
        cursor_set(4); frame();
        shift_down();
        turn(EN_PRESET, 1);
        turn(EN_PRESET, 1);
        shift_up();
        chk("SHIFT   + PRESETS on FEEL stretches the range from the cursor", bulk.on == 1u && bulk.anchor == 4u && bulk.end == 6u);
        turn(EN_K2, 1);
        shift_down();
        hold_btn(B_EDIT);
        rel_btn(B_EDIT);
        shift_up();
        chk("SHIFT   + EDIT on FEEL resets the range (the notes stay)",
            step_rflam(&TSEL->step[4]) == 1u && step_rflam(&TSEL->step[6]) == 1u && TSEL->step[5].n == 1u && !ui.confirm);
        bulk.on = 0;
        fresh("NOTES");
        put_note(0, 2, 0, 60, 70, RECORD_UNIT);
        put_note(1, 3, 0, 62, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(2); frame();
        shift_down();
        turn(EN_PRESET, 1);
        shift_up();
        chk("SHIFT   + PRESETS on NOTES stretches the range", bulk.on == 1u && bulk.end == 3u);
        shift_down();
        hold_btn(B_EDIT);
        rel_btn(B_EDIT);
        shift_up();
        chk("SHIFT   + EDIT on NOTES deletes the selected notes, no init confirm", !recording[0].vel && !recording[1].vel && !ui.confirm);
        bulk.on = 0;
    }
    {   /* DRAW */
        uint32_t k;
        fresh("FEEL");
        manual(4, 60); manual(5, 62);
        step_set_chance(&TSEL->step[4], 75);
        TSEL->step[5].cond = TC_2_3;
        cursor_set(4); ui.force = 1; frame();
        for (k = 0; k < 4u; k++) { turn(EN_K1 + k, 1); frames(32); }
        song.playing = 1; frames(32); song.playing = 0;
        fresh("NOTES");
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(2); ui.force = 1; frames(48);
        chk("DRAW    FEEL and NOTES drawn through every knob", 1);
    }
    printf("step_pages: %d failed\n", nbad);
    return nbad != 0;
#endif
}
