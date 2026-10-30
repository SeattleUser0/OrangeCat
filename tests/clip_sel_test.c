/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 11 (docs/orangecat/UI_BUILD.md "Step 11: copy and paste of notes and of a step's feel") with OC_CLIP_SEL=1, through
 * tests/ui_test.c's host UI.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DOC_CLIP_SEL=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o clip_sel_test tests/clip_sel_test.c -lm
 * Checked:
 *   NOTES   a range of recorded notes copied and pasted at the cursor: the same notes, velocities, lengths and distances; one undo; a manual
 *           chord step pasted as events at its length; the clipboard survives the paste (twice); a paste past the loop's end wraps
 *   REFUSED nothing copied; the song playing; a landing on a manual note; no recording memory; more notes than the clipboard holds
 *   FEEL    a step's feel copied and set on every step of a range that is on (off steps untouched); an empty step; nothing copied
 * Revisions:
 *   (the new interface 0.11, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("clip_sel: %-100s %s\n", what, ok ? "ok" : "FAIL");
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
                         .owner = (uint8_t)recording_owner(TSEL), .length = 1};
    note_duration(&r, len);                              /* (a duration is 16 bits and an exponent) */
    recording_restore_note(TSEL, i, r);
}
static void manual(uint32_t s, uint32_t note)
{
    step_t *st = &TSEL->step[s];
    st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96;
}


static uint32_t nrec(void) { uint32_t n = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) n += recording[i].vel && (recording[i].owner & 31u) == recording_owner(TSEL); return n; }
static uint32_t at_step(uint32_t step, uint32_t note, uint32_t *on)
{
    for (uint32_t i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel && (recording[i].owner & 31u) == recording_owner(TSEL) && (recording[i].step & 63u) == step && recording[i].note == note) { if (on) *on = recording[i].on; return i + 1u; }
    return 0;
}
static void select_range(uint32_t a, uint32_t b)
{
    bulk.on = 1; bulk.anchor = (uint16_t)a; bulk.end = (uint16_t)b; bulk.engine = (uint8_t)library_engine(); bulk.track = song.sel; bulk.pattern = TSEL->pattern; bulk.generation = TSEL->pattern_gen;
}
static void oct(uint32_t b)                             /* SHIFT + OCT-/OCT+ */
{
    shift_down();
    hold_btn(b);
    rel_btn(b);
    shift_up();
}

int main(void)
{
#if !OC_CLIP_SEL
    printf("clip_sel: built without OC_CLIP_SEL\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* NOTES: a range of recorded notes */
        uint32_t on1, on2, i1, i2;
        fresh("NOTES");
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT);
        put_note(1, 3, 2000, 64, 80, RECORD_UNIT / 2u);
        put_note(2, 5, 3000, 67, 90, RECORD_UNIT * 2u);
        note_rebuild_all();
        select_range(2, 5);
        cursor_set(2); frame();
        oct(B_OCTDN);
        chk("NOTES   copy: says so, three notes", msg_is("NOTES COPIED") && notes_clip.n == 3u);
        bulk.on = 0;
        cursor_set(8); frame();
        oct(B_OCTUP);
        i1 = at_step(8, 60, &on1); i2 = at_step(11, 67, &on2);
        chk("NOTES   paste at the cursor: six notes, the first at step 8, the others at their distances",
            nrec() == 6u && i1 && on1 == 1000u && at_step(9, 64, 0) && i2 && on2 == 3000u && msg_is("NOTES PASTED"));
        chk("NOTES   velocities and lengths come across", recording[i1 - 1u].vel == 70u && dur(&recording[i1 - 1u]) == RECORD_UNIT && recording[i2 - 1u].vel == 90u && dur(&recording[i2 - 1u]) == RECORD_UNIT * 2u);
        hold(B_EDIT);
        chk("NOTES   EDIT held undoes the paste whole", nrec() == 3u && !at_step(8, 60, 0));
        cursor_set(8); frame();
        oct(B_OCTUP);
        cursor_set(12); frame();
        oct(B_OCTUP);
        chk("NOTES   the clipboard survives a paste (twice)", nrec() == 9u && at_step(12, 60, 0) && at_step(15, 67, 0));
        cursor_set(15); frame();
        oct(B_OCTUP);
        chk("NOTES   a paste past the loop's end wraps to its start", at_step(15, 60, 0) && at_step(0, 64, 0));
    }
    {   /* NOTES: a manual chord */
        uint32_t on = 0;
        fresh("NOTES");
        manual(4, 60); TSEL->step[4].note[1] = 64; TSEL->step[4].n = 2;
        cursor_set(4); frame();
        ui.note_pick = 0; notes_selected(TSEL);
        oct(B_OCTDN);
        cursor_set(10); frame();
        oct(B_OCTUP);
        chk("NOTES   a manual chord (no selection) pastes as two events at the step's length", nrec() == 2u && at_step(10, 60, &on) && on == 0u && at_step(10, 64, 0) &&
            dur(&recording[at_step(10, 60, 0) - 1u]) == RECORD_UNIT && (TSEL->step[4].flags & SF_RECORDED) == 0 && TSEL->step[4].n == 2u);
    }
    {   /* refusals */
        fresh("NOTES");
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT);
        note_rebuild_all();
        cursor_set(2); frame();
        ui.note_pick = 0; notes_selected(TSEL);
        notes_clip.n = 0;
        oct(B_OCTUP);
        chk("REFUSED nothing copied", msg_is("NOTHING COPIED") && nrec() == 1u);
        oct(B_OCTDN);
        manual(9, 70);
        cursor_set(9); frame();
        oct(B_OCTUP);
        chk("REFUSED a landing on a manual note: says so, nothing written", msg_is("STEP OCCUPIED") && nrec() == 1u);
        cursor_set(11); frame();
        song.playing = 1;
        oct(B_OCTUP);
        song.playing = 0;
        chk("REFUSED the song playing: STOP TO EDIT", msg_is("STOP TO EDIT") && nrec() == 1u);
        for (uint32_t i = 1; i < RECORD_MAX; i++) if (!recording[i].vel) { recording[i].vel = 1; recording[i].owner = 31; }
        oct(B_OCTUP);
        chk("REFUSED no recording memory: RECORDING FULL", msg_is("RECORDING FULL") && nrec() == 1u);
        fresh("NOTES");
        for (uint32_t i = 0; i < 65u; i++) put_note(i, i % 16u, 100u + i, (uint32_t)(30 + i % 40), 70, RECORD_UNIT / 4u);
        note_rebuild_all();
        select_range(0, 15);
        cursor_set(0); frame();
        oct(B_OCTDN);
        chk("REFUSED more notes than the clipboard holds: TOO MANY NOTES", msg_is("TOO MANY NOTES") && notes_clip.n == 0u);
    }
    {   /* FEEL */
        step_t *s;
        fresh("FEEL");
        manual(4, 60); manual(6, 62); manual(8, 64);
        s = &TSEL->step[4];
        step_set_chance(s, 75); step_set_rflam(s, 2); step_set_nudge(s, 6); s->flags |= SF_ACCENT | SF_SLIDE;
        cursor_set(4); frame();
        oct(B_OCTDN);
        chk("FEEL    copy: says so", msg_is("FEEL COPIED") && feel_clip.valid && feel_clip.chance == 75u && feel_clip.rflam == 2u && feel_clip.nudge == 6);
        select_range(5, 9);
        cursor_set(7); frame();
        oct(B_OCTUP);
        chk("FEEL    paste sets every step of the range that is on, not the off steps",
            step_chance(&TSEL->step[6]) == 75u && step_rflam(&TSEL->step[8]) == 2u && TSEL->step[6].nudge == 6 && (TSEL->step[8].flags & SF_ACCENT) && (TSEL->step[8].flags & SF_SLIDE) &&
            !step_on(&TSEL->step[7]) && step_chance(&TSEL->step[4]) == 75u);
        bulk.on = 0;
        feel_clip.valid = 0;
        oct(B_OCTUP);
        chk("FEEL    nothing copied", msg_is("NOTHING COPIED"));
        cursor_set(1); frame();
        oct(B_OCTDN);
        chk("FEEL    an empty step: NO NOTE THERE", msg_is("NO NOTE THERE") && !feel_clip.valid);
    }
    printf("clip_sel: %s\n", nbad ? "FAILED" : "all ok");
    return nbad != 0;
#endif
}
