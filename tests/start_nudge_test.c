/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix2 item 5 (docs/orangecat/UI_BUILD.md "ui-fix2 5"): NOTES' KNOB 2 (START) on a typed note moves it in time as the tracker's NUDGE does,
 * with OC_START_NUDGE=1.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_START_NUDGE=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen
 *      -Ifirmware/src -Itests -o start_nudge_test tests/start_nudge_test.c -lm
 * Checked:
 *   NUDGE   K2 on a typed note: the step's nudge moves a 64th a detent, the note stays a typed step (nothing recorded, no flag), the song's pool
 *           untouched, and the edit memory (the note transaction) never asked for: so a build where it is not there still moves the note
 *   LIMITS  half a step earlier .. half a step later, the end said; a chord moves together
 *   UNDO    EDIT held brings the nudge back (the step history)
 *   PLAYING it moves while the song plays (no STOP TO EDIT), it is refused while a chain runs
 *   RECORDED a recorded note's K2 is still its onset (the event), the step's nudge untouched
 *   SAME    the tracker's NUDGE and K2 write the same field
 * Revisions:
 *   (ui-fix2 5, branch ui-fix2) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("start_nudge: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static uint32_t pool_n(void) { uint32_t i, n = 0; for (i = 0; i < RECORD_MAX; i++) n += recording[i].vel != 0; return n; }
static void fresh(void)
{
    ui_power_on();
    stop_transport();
    go_title("NOTES");
    frame();
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = 16;
    bulk.on = 0;
}
static void typed(uint32_t s, uint32_t note)
{
    step_t *st = &TSEL->step[s];
    memset(st, 0, sizeof *st);
    st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96;
}

int main(void)
{
#if !OC_START_NUDGE
    printf("start_nudge: built without OC_START_NUDGE\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* NUDGE */
        fresh();
        typed(3, 60);
        cursor_set(3);
        turn(EN_K2, 3);
        chk("NUDGE   K2 +3 on a typed note: the step's nudge 3", TSEL->step[3].nudge == 3);
        chk("NUDGE   it stays a typed step: nothing recorded, no flag", pool_n() == 0u && !(TSEL->step[3].flags & SF_RECORDED) && TSEL->step[3].n == 1);
        chk("NUDGE   the edit memory was never asked for", note_transaction_ptr == 0 || !note_transaction.valid);
        turn(EN_K2, -5);
        chk("NUDGE   K2 -5: -2", TSEL->step[3].nudge == -2);
        chk("NUDGE   the message says START -2", msg_has("START -2"));
    }
    {   /* LIMITS */
        fresh();
        typed(3, 60);
        typed(4, 64); TSEL->step[4].note[1] = 67; TSEL->step[4].n = 2;
        cursor_set(3);
        { int i; for (i = 0; i < 40; i++) turn(EN_K2, 2); }
        chk("LIMITS  half a step later at most (+31), the end said", TSEL->step[3].nudge == NUDGE_MAX && msg_has("LATEST"));
        { int i; for (i = 0; i < 40; i++) turn(EN_K2, -2); }
        chk("LIMITS  half a step earlier at most (-32), the end said", TSEL->step[3].nudge == NUDGE_MIN && msg_has("EARLIEST"));
        cursor_set(4);
        turn(EN_K2, 4);
        chk("LIMITS  a chord moves together (the step's nudge)", TSEL->step[4].nudge == 4 && TSEL->step[4].n == 2);
    }
    {   /* UNDO */
        fresh();
        typed(3, 60);
        cursor_set(3);
        turn(EN_K2, 4);
        hold(B_EDIT);
        chk("UNDO    EDIT held brings the nudge back", TSEL->step[3].nudge == 0 && TSEL->step[3].n == 1);
    }
    {   /* PLAYING */
        fresh();
        typed(3, 60);
        cursor_set(3);
        song.playing = 1;
        turn(EN_K2, 2);
        chk("PLAYING it moves while the song plays", TSEL->step[3].nudge == 2);
        song.playing = 0;
        chain.running = 1;
        turn(EN_K2, 2);
        chain.running = 0;
        chk("PLAYING refused while a chain runs (STOP TO EDIT), the value kept", TSEL->step[3].nudge == 2);
    }
    {   /* RECORDED */
        fresh();
        {
            recorded_note_t r = {.step = 5, .on = 0, .note = 64, .vel = 100, .duration = RECORD_UNIT / 2u, .owner = (uint8_t)recording_owner(TSEL), .length = 1};
            recording_restore_note(TSEL, 0, r);
        }
        note_rebuild_all();
        cursor_set(5);
        turn(EN_K2, 4);
        chk("RECORDED a recorded note's K2 is its onset: the event moved, the step's nudge untouched",
            recording[0].on == RECORD_UNIT / 4u && TSEL->step[5].nudge == 0);
    }
    {   /* SAME */
        fresh();
        typed(3, 60);
        cursor_set(3);
        turn(EN_K2, 3);
        {
            char b[8];
            tk_fx_text(&TSEL->step[3], TFX_NUDGE, b, 1);
            chk("SAME    the tracker's NUDGE cell reads what K2 wrote (+3)", !strcmp(b, "+3"));
        }
    }
    printf("start_nudge: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
#endif
}
