/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix 7 (docs/orangecat/UI_BUILD.md "ui-fix 7"): one authority for every note, with OC_ONE_AUTH=1, through tests/ui_test.c's host UI.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen
 *      -Ifirmware/src -Itests -o one_auth_test tests/one_auth_test.c -lm
 * The invariant (recording.c recording_reconcile): every event with a velocity has a NOTE cell flagged SF_RECORDED at its view, in its
 * owner's bank; every flagged NOTE cell has an event viewing it. Checked:
 *   RECONCILE  an orphan event in the current bank goes; a flagged empty cell is cleared; a flagged cell with a manual note becomes
 *              manual, its note kept; an orphan flag in another bank is mended there (pattern_read, pattern_write); a project saved with
 *              an orphan planted loads mended (the one place the old lazy prune survives)
 *   PATHS      the invariant after every edit path, and recording_prune finding nothing: keys on a manual and (the entry code) on a
 *              recorded step, the tracker's entry, a grid hit on DRUM-X, EDIT on NOTES (a step delete) and on a recorded note, LEN on a
 *              manual note (converts), the tracker's KNOB 4 on a chord, START on a recorded note, a move of a recorded note across a cell,
 *              EDIT held (undo) and redo after each
 * Revisions:
 *   (ui-fix 7, branch ui-fix) first version: step 1 of the build order (the check and the reconcile).
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("one_auth: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
/* the invariant over every owner bank; 0 with a line said */
static int auth_ok(const char *where)
{
    uint32_t i, k, b, n, bad = 0;
    for (i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel && !recording_present(&recording[i])) {
            printf("one_auth:   %s: event %u (owner %u step %u) has no flagged cell\n", where, (unsigned)i, recording[i].owner & 31u, recording[i].step & 63u);
            bad++;
        }
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++)
            for (n = 0; n < NSTEP; n++) {
                step_t st = pattern_step(k, b, n);
                uint32_t have = 0;
                if (!(st.flags & SF_RECORDED) || st.time != ST_NOTE) continue;
                for (i = recording_head[k * NPAT + b]; i < RECORD_MAX; i = recording_next[i])
                    have |= recording_view(&trk[k], &recording[i]) == n;
                if (!have) { printf("one_auth:   %s: track %u bank %u cell %u flagged with no event\n", where, (unsigned)k, (unsigned)b, (unsigned)n); bad++; }
            }
    return bad == 0;
}
static uint32_t pool_count(void) { uint32_t i, n = 0; for (i = 0; i < RECORD_MAX; i++) n += recording[i].vel != 0; return n; }
static int prune_noop(track_t *t)                      /* recording_prune has nothing left to do */
{
    uint32_t before = pool_count();
    fm1_irq_off(); recording_prune(t); fm1_irq_on();
    return pool_count() == before;
}
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
static void recorded(uint32_t s, uint32_t note)         /* a recorded step of one event, the cell flagged as the pool's load does */
{
    put_note(s, s, 0, note, 100, RECORD_UNIT / 2u);
    note_rebuild_all();
}
static int path(const char *what, track_t *t)
{
    int ok = auth_ok(what) && prune_noop(t);
    chk(what, ok);
    return ok;
}

int main(void)
{
#if !OC_ONE_AUTH
    printf("one_auth: built without OC_ONE_AUTH\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* RECONCILE */
        track_t *t;
        int ok;
        pattern_t p;
        uint32_t k;
        fresh("NOTES"); t = TSEL; k = trk_index(t);
        recorded(2, 60);
        ok = auth_ok("fresh");
        put_note(7, 9, 0, 64, 100, RECORD_UNIT / 2u);          /* an orphan: an event with no flag on its cell */
        ok &= !auth_ok("planted (expected)");
        fm1_irq_off(); recording_reconcile(); fm1_irq_on();
        ok &= auth_ok("mended") && !recording[7].vel && recording[2].vel && (t->step[2].flags & SF_RECORDED);
        chk("RECONCILE an orphan event in the current bank goes; the recorded step stays", ok);
        t->step[5].time = ST_NOTE; t->step[5].flags |= SF_RECORDED;   /* a flagged empty cell */
        manual(6, 62); t->step[6].flags |= SF_RECORDED;  /* a flagged cell with a manual note */
        ok = !auth_ok("planted (expected)");
        fm1_irq_off(); recording_reconcile(); fm1_irq_on();
        ok &= auth_ok("mended") && t->step[5].time == ST_REST && !(t->step[5].flags & SF_RECORDED) &&
              !(t->step[6].flags & SF_RECORDED) && t->step[6].n == 1u && t->step[6].note[0] == 62u && t->step[6].time == ST_NOTE;
        chk("RECONCILE a flagged empty cell is cleared; a flagged cell with a manual note becomes manual, the note kept", ok);
        pattern_read(k, 1, &p);                         /* another bank: a flagged cell with no event, and an orphan event */
        p.step[4].time = ST_NOTE; p.step[4].flags |= SF_RECORDED;
        fm1_irq_off(); pattern_write(k, 1, &p); fm1_irq_on();
        {
            recorded_note_t r = {.step = 11, .on = 0, .note = 65, .vel = 100, .duration = RECORD_UNIT / 2u, .owner = (uint8_t)(k * NPAT + 1u), .length = 1};
            fm1_irq_off(); recording[9] = r; recording_reindex(); fm1_irq_on();
        }
        ok = !auth_ok("planted (expected)");
        fm1_irq_off(); recording_reconcile(); fm1_irq_on();
        ok &= auth_ok("mended") && !recording[9].vel && !(pattern_step(k, 1, 4).flags & SF_RECORDED) && pattern_step(k, 1, 4).time == ST_REST;
        chk("RECONCILE another bank: its flagged empty cell and its orphan event are mended there", ok);
    }
    {   /* RECONCILE at a project load: an orphan planted, saved, loaded mended */
        track_t *t;
        int ok;
        fresh("NOTES"); t = TSEL;
        recorded(2, 60);
        put_note(7, 9, 0, 64, 100, RECORD_UNIT / 2u);          /* the orphan: saved with the project (every event with a velocity is) */
        ok = !auth_ok("planted (expected)") && project_save(0) == 0;
        track_defaults_steps(t); fm1_irq_off(); recording_clear(t); fm1_irq_on();
        project_load(0); frames(100);
        t = TSEL;
        ok &= auth_ok("loaded") && (t->step[2].flags & SF_RECORDED) && pool_count() == 1u;
        chk("RECONCILE a project saved with an orphan loads mended: the recorded step kept, the orphan gone", ok);
    }
    {   /* LOAD (round 11 review, 1): a consistent recorded note in another bank survives a save and a load (the reconcile runs once the
         * banks are back, never against the banks pattern_init emptied) */
        track_t *t;
        int ok;
        pattern_t p;
        uint32_t k, i, found;
        fresh("NOTES"); t = TSEL; k = trk_index(t);
        recorded(2, 60);
        pattern_read(k, 1, &p);
        p.step[4].time = ST_NOTE; p.step[4].flags |= SF_RECORDED; p.step[4].note[0] = 65; p.step[4].n = 1; p.step[4].vel = 100;
        fm1_irq_off(); pattern_write(k, 1, &p); fm1_irq_on();
        {
            recorded_note_t r = {.step = 4, .on = 0, .note = 65, .vel = 100, .duration = RECORD_UNIT / 2u, .owner = (uint8_t)(k * NPAT + 1u), .length = 1};
            fm1_irq_off(); recording[9] = r; recording_reindex(); fm1_irq_on();
        }
        ok = recording_present(&recording[9]) && pool_count() == 2u && auth_ok("two banks");
        fm1_irq_off(); recording_reconcile(); fm1_irq_on();
        ok &= pool_count() == 2u && recording[9].vel == 100u;
        ok &= project_save(0) == 0;
        track_defaults_steps(t); fm1_irq_off(); recording_clear(t); fm1_irq_on();
        project_load(0); frames(100);
        t = TSEL; k = trk_index(t);
        for (found = RECORD_MAX, i = 0; i < RECORD_MAX; i++) if (recording[i].vel && recording[i].note == 65u && (recording[i].owner & 31u) == k * NPAT + 1u) found = i;
        ok &= auth_ok("loaded") && found < RECORD_MAX && (pattern_step(k, 1, 4).flags & SF_RECORDED) && pattern_step(k, 1, 4).n == 1u &&
              (t->step[2].flags & SF_RECORDED) && pool_count() == 2u;
        chk("LOAD a consistent recorded note in another bank survives a project save and load (event kept, cell flagged)", ok);
    }
    {   /* PATHS */
        track_t *t;
        uint32_t key = 62;
        fresh("NOTES"); t = TSEL;
        song.seq_mode = 1;
        cursor_set(1); frame();
        tap_key(white(3)); frame();
        path("PATHS   a key on a manual step (NOTES)", t);
        recorded(4, 60);
        cursor_set(4); frame();
        seq_entry_notes((uint8_t *)&key, 1); seq_entry_finish(); frame();   /* (the entry code on a recorded step: the dispatcher's gate is step 3's) */
        path("PATHS   the step page's entry on a recorded step (events_delete_view)", t);
        hold(B_EDIT); frame();                          /* undo: the recording back */
        path("PATHS   EDIT held after it (the events back)", t);
        go_title("TRACKER"); frame(); cursor_set(4); frame();
        seq_entry_notes((uint8_t *)&key, 1); seq_entry_finish(); frame();
        path("PATHS   the tracker's entry on a recorded step", t);
        go_title("NOTES"); frame();
        recorded(8, 67);
        cursor_set(8); frame();
        press(B_EDIT); frame();                         /* a recorded note deleted */
        path("PATHS   EDIT on a recorded note", t);
        manual(10, 64); cursor_set(10); frame();
        turn(EN_K3, 1);                                 /* LEN on a manual note: converts */
        path("PATHS   LEN on a manual note (the step converts)", t);
        hold(B_EDIT); frame();
        path("PATHS   EDIT held after the conversion", t);
        cursor_set(10); frame();
        press(B_EDIT); frame();                         /* EDIT on NOTES: the step */
        path("PATHS   EDIT on NOTES (a step delete)", t);
        recorded(12, 60); cursor_set(12); frame();
        turn(EN_K2, 4);                                 /* START across a sixteenth */
        path("PATHS   START on a recorded note", t);
        btn_down(B_SCL); frame(); turn(EN_SELECT, 1); btn_up(B_SCL); frame();   /* a recorded note moved a step (SCL + SELECT) */
        path("PATHS   SCL + SELECT on a recorded note (a move)", t);
        go_title("TRACKER"); frame();
        track_defaults_steps(t);
        t->step[3].note[0] = 60; t->step[3].note[1] = 64; t->step[3].n = 2; t->step[3].time = ST_NOTE; t->step[3].vel = 96;
        cursor_set(3); frame(); ui.tk_col = TKC_N2; turn(EN_K4, 1);   /* the tracker's KNOB 4 on a chord: converts */
        path("PATHS   the tracker's KNOB 4 on a manual chord", t);
        hold(B_EDIT); frame();
        path("PATHS   EDIT held after it", t);
        {   /* a grid hit on DRUM-X over a recorded cell */
            set_engine_of(TSEL, ENGI_DRUMX); frames(100); t = TSEL;
            go_title("NOTES"); frame();
            track_defaults_steps(t);
            put_note(20, 2, 0, DRUM_LANE_NOTE[0], 100, RECORD_UNIT / 2u); note_rebuild_all();
            path("PATHS   a recorded drum cell", t);
            fm1_irq_off(); grid_hit(t, 2, 1, 1); fm1_irq_on(); frame();
            path("PATHS   a grid hit over a recorded drum cell", t);
            cursor_set(2); frame(); press(B_EDIT); frame();   /* the grid's EDIT: the cell cleared (page_tap) */
            path("PATHS   the grid's EDIT on a drum cell", t);
        }
    }
    {   /* START on a manual note (ui-fix 7 point 1): the step converts, the note moves, picked; one undo; a chord entered out of
         * order moves the note under the cursor; STEP OCCUPIED rolls the conversion back; STOP TO EDIT while playing; a full pool
         * leaves the step manual */
        track_t *t;
        int ok;
        uint32_t i, found;
        fresh("NOTES"); t = TSEL;
        manual(3, 60); cursor_set(3); frame();
#if !OC_START_NUDGE                                       /* (ui-fix2 5: K2 on a typed note is NUDGE then: tests/start_nudge_test.c) */
        turn(EN_K2, 2);                                 /* START: two sixteenths */
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        ok = found < RECORD_MAX && (t->step[3].flags & SF_RECORDED) && recording[found].on == RECORD_UNIT / 8u && (recording[found].step & 63u) == 3u &&
             notes_selected(t) == found && ui.cursor == 3u && auth_ok("start");
        hold(B_EDIT); frame();                          /* undo: the manual step back */
        ok &= !(t->step[3].flags & SF_RECORDED) && t->step[3].n == 1u && t->step[3].note[0] == 60u && pool_count() == 0u && auth_ok("undone");
        chk("START   a manual note: the step converts, the note moves and is picked; EDIT held: the manual step back", ok);
        turn(EN_K2, 2);
        turn(EN_K2, 1);                                 /* on with the same note (an event edit: its own undo level from here, as LEN) */
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        ok = found < RECORD_MAX && recording[found].on == 3u * RECORD_UNIT / 16u && pool_count() == 1u;
        hold(B_EDIT); frame();
        ok &= recording[found].on == RECORD_UNIT / 8u && (t->step[3].flags & SF_RECORDED);
        chk("START   the next detent goes on with the picked note; EDIT held then undoes that detent (the conversion's undo is gone with it, as LEN's)", ok);
        fresh("NOTES"); t = TSEL;
        {
            uint8_t ch[3] = {67, 60, 64};
            song.seq_mode = 1; cursor_set(5); frame();
            seq_entry_notes(ch, 3); seq_entry_finish(); frame();
            cursor_set(5); frame();                     /* (the entry moved the cursor on) */
        }
        ui.note_slot = 1;                               /* the 60 under the cursor */
        turn(EN_K2, 1);
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        ok = pool_count() == 3u && found < RECORD_MAX && recording[found].on == RECORD_UNIT / 16u && notes_selected(t) == found;
        for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note != 60u) ok &= recording[i].on == 0u;
        chk("START   a chord entered 67, 60, 64: the note under the cursor moves, the other two stay on the grid", ok && auth_ok("chord"));
        fresh("NOTES"); t = TSEL;
        manual(3, 60); manual(4, 62); cursor_set(3); frame();
        turn(EN_K2, 16);                                /* a whole step at once: into the manual note's cell */
        ok = !(t->step[3].flags & SF_RECORDED) && t->step[3].n == 1u && t->step[4].n == 1u && pool_count() == 0u && str_eq(ui.msg, "STEP OCCUPIED") && auth_ok("occupied");
        chk("START   into a cell that holds a separate manual note: STEP OCCUPIED, nothing converted", ok);
        song.playing = 1; turn(EN_K2, 1); song.playing = 0;
        ok = str_eq(ui.msg, "STOP TO EDIT") && !(t->step[3].flags & SF_RECORDED);
        chk("START   while the song plays: STOP TO EDIT, the step manual", ok);
        for (i = 0; i < RECORD_MAX; i++) if (!recording[i].vel) { recording[i].vel = 1; recording[i].owner = 31; }   /* the pool full */
        turn(EN_K2, 1);
        ok = str_eq(ui.msg, "RECORDING FULL") && !(t->step[3].flags & SF_RECORDED) && t->step[3].n == 1u;
        chk("START   the pool full: RECORDING FULL, the step manual", ok);
#endif
        for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel == 1u && recording[i].owner == 31u) recording[i].vel = 0;
    }
    {   /* the tracker's gestures on a recorded step (point 3): SCL + SELECT moves its events; ENV + SELECT says K4 */
        track_t *t;
        int ok;
        fresh("TRACKER"); t = TSEL;
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT / 2u); put_note(1, 2, 3000, 64, 90, RECORD_UNIT / 2u); note_rebuild_all();
        cursor_set(2); frame();
        btn_down(B_SCL); frame(); turn(EN_SELECT, 1); btn_up(B_SCL); frame();
        ok = (recording[0].step & 63u) == 3u && recording[0].on == 1000u && (recording[1].step & 63u) == 3u && recording[1].on == 3000u &&
             (t->step[3].flags & SF_RECORDED) && !(t->step[2].flags & SF_RECORDED) && !step_on(&t->step[2]) && pool_count() == 2u && auth_ok("moved");
        chk("TRACKER SCL + SELECT on a recorded step: both events a step over, their onsets kept, nothing flattened", ok);
        hold(B_EDIT); frame();
        ok = (recording[0].step & 63u) == 2u && (t->step[2].flags & SF_RECORDED) && auth_ok("move undone");
        chk("TRACKER EDIT held: the move undone whole", ok);
        btn_down(B_ENV); frame(); turn(EN_SELECT, 1); btn_up(B_ENV); frame();
        ok = str_eq(ui.msg, "K4 SETS A NOTE'S LENGTH") && recording[0].duration == RECORD_UNIT / 2u && (t->step[2].flags & SF_RECORDED);
        chk("TRACKER ENV + SELECT on a recorded step: refused with where the length is; nothing flattened", ok);
    }
    {   /* a key on a recorded step (point 4): replaces it; EDIT held brings the events back; the pick is cleared */
        track_t *t;
        int ok;
        uint32_t holds = 0;
        fresh("NOTES"); t = TSEL;
        song.seq_mode = 1;
        put_note(0, 4, 1000, 60, 70, RECORD_UNIT / 2u); put_note(1, 4, 2000, 64, 90, RECORD_UNIT / 2u); note_rebuild_all();
        cursor_set(4); frame();
        tap_key(white(3)); frame();
        ok = !(t->step[4].flags & SF_RECORDED) && t->step[4].n == 1u && pool_count() == 0u && !ui.note_pick && auth_ok("key");
        chk("KEY     a key on a recorded step replaces it (the dispatcher's gate gone); its events gone from the pool", ok);
        while (holds < 4u && pool_count() < 2u) { hold(B_EDIT); frame(); holds++; }
        ok = pool_count() == 2u && (t->step[4].flags & SF_RECORDED) && auth_ok("key undone");
        chk("KEY     EDIT held brings the two events back (one level each, then the overwrite)", ok && holds <= 3u);
        go_title("TRACKER"); frame(); cursor_set(4); frame();
        tap_key(white(5)); frame();
        ok = !(t->step[4].flags & SF_RECORDED) && t->step[4].n == 1u && pool_count() == 0u && auth_ok("tracker key");
        chk("KEY     the same on the TRACKER", ok);
    }
    {   /* the web editor's step put on a recorded cell (point 3): the cell's events go */
        track_t *t;
        int ok;
        fresh("NOTES"); t = TSEL;
        put_note(0, 6, 0, 60, 70, RECORD_UNIT / 2u); put_note(1, 7, 0, 65, 70, RECORD_UNIT / 2u); note_rebuild_all();
        ok = (t->step[6].flags & SF_RECORDED) != 0u;
        events_drop_view(t, 6);                         /* (editor.c calls it before ed_step_put writes the cell; the harness has no editor.c) */
        t->step[6].flags = 0; t->step[6].note[0] = 62; t->step[6].n = 1; t->step[6].time = ST_NOTE; frame();
        ok &= pool_count() == 1u && recording[1].vel && !(t->step[6].flags & SF_RECORDED) && t->step[6].note[0] == 62u && auth_ok("editor");
        chk("EDITOR  a step put on a recorded cell: its events dropped first (no history), the next cell's kept, the cell as sent", ok);
    }
    {   /* the diff review's cases: the event list ordered by the scheduler (a move must collect first); the cursor on a recorded note's
         * TIE cell (SCL + SELECT moves the note, ENV + SELECT is refused); a load with a flagged empty cell in another bank; LEN
         * below a recorded cell keeps its dormant events; a key beside a rounded-forward event; DRUM-X's LEN on a recorded hit as before */
        track_t *t;
        int ok;
        uint32_t i, k;
        pattern_t p;
        fresh("TRACKER"); t = TSEL;
        put_note(0, 2, 1000, 60, 70, RECORD_UNIT / 2u); put_note(1, 2, 2000, 64, 90, RECORD_UNIT / 2u); put_note(2, 2, 3000, 67, 90, RECORD_UNIT / 2u);
        note_rebuild_all();
        fm1_irq_off(); recording_sync(t, seq_div_samples((uint32_t)t->p[P_SDIV])); fm1_irq_on();   /* the list ordered, as after any play */
        cursor_set(2); frame();
        btn_down(B_SCL); frame(); turn(EN_SELECT, 1); btn_up(B_SCL); frame();
        ok = 1;
        for (i = 0; i < 3u; i++) ok &= (recording[i].step & 63u) == 3u;
        ok &= recording[0].on == 1000u && recording[1].on == 2000u && recording[2].on == 3000u && !step_on(&t->step[2]) && (t->step[3].flags & SF_RECORDED) && ui.cursor == 3u && auth_ok("ordered move");
        chk("REVIEW  SCL + SELECT with the event list ordered: all three events move together (the review's 1)", ok);
        fresh("TRACKER"); t = TSEL;
        put_note(0, 5, 0, 60, 70, RECORD_UNIT / 2u); note_rebuild_all();
        t->step[6].time = ST_TIE;                        /* a TIE cell after the recorded onset, as a live take leaves */
        cursor_set(6); frame();
        btn_down(B_ENV); frame(); turn(EN_SELECT, 1); btn_up(B_ENV); frame();
        ok = str_eq(ui.msg, "K4 SETS A NOTE'S LENGTH") && (t->step[5].flags & SF_RECORDED) && recording_present(&recording[0]) && auth_ok("tie resize");
        chk("REVIEW  ENV + SELECT with the cursor on a recorded note's TIE cell: refused, nothing flattened (the review's 2)", ok);
        btn_down(B_SCL); frame(); turn(EN_SELECT, 1); btn_up(B_SCL); frame();
        ok = (recording[0].step & 63u) == 6u && (t->step[6].flags & SF_RECORDED) && !(t->step[5].flags & SF_RECORDED) && auth_ok("tie move") && str_eq(ui.msg, "NOTES MOVED");
        chk("REVIEW  SCL + SELECT on the TIE cell: the note moves a step (the review's 3)", ok);
        fresh("NOTES"); t = TSEL; k = trk_index(t);
        recorded(2, 60);
        pattern_read(k, 1, &p); p.step[4].time = ST_NOTE; p.step[4].flags |= SF_RECORDED; p.step[5].time = ST_TIE;
        fm1_irq_off(); pattern_write(k, 1, &p); fm1_irq_on();
        ok = !auth_ok("planted (expected)") && project_save(0) == 0;
        track_defaults_steps(t); fm1_irq_off(); recording_clear(t); fm1_irq_on();
        project_load(0); frames(100); t = TSEL; k = trk_index(t);
        ok &= auth_ok("loaded") && pattern_step(k, 1, 4).time == ST_REST && pattern_step(k, 1, 5).time == ST_REST && (t->step[2].flags & SF_RECORDED);
        chk("REVIEW  a project with a flagged empty cell and its tie in another bank loads mended there", ok);
        fresh("NOTES"); t = TSEL;
        recorded(2, 60); recorded(12, 64);
        t->p[P_SLEN] = 8; frame();                      /* LEN below the second: dormant, kept */
        ok = auth_ok("len below") && prune_noop(t) && recording[12].vel && pool_count() == 2u;
        t->p[P_SLEN] = 16; frame();
        ok &= auth_ok("len back") && pool_count() == 2u;
        chk("REVIEW  LEN below a recorded cell: its events stay dormant and come back with LEN", ok);
        fresh("NOTES"); t = TSEL; song.seq_mode = 1;
        put_note(0, 4 | 64u, 40000, 60, 70, RECORD_UNIT / 2u); note_rebuild_all();   /* rounded into cell 5 (the 64 bit: the view is the next cell) */
        manual(4, 55);
        cursor_set(4); frame();
        tap_key(white(3)); frame();
        ok = t->step[4].n == 1u && !(t->step[4].flags & SF_RECORDED) && recording[0].vel && (t->step[5].flags & SF_RECORDED) && !ui.note_pick && auth_ok("rounded key");
        chk("REVIEW  a key on a manual step beside an event rounded into the next cell: the step replaced, the event kept, no pick", ok);
        fresh("NOTES"); t = TSEL;
        set_engine_of(t, ENGI_DRUMX); frames(100); t = TSEL; track_defaults_steps(t);
        put_note(0, 2, 0, DRUM_LANE_NOTE[0], 100, RECORD_UNIT / 2u); note_rebuild_all();
        cursor_set(2); frame(); notes_selected(t);
        turn(EN_K3, -4);                                /* LEN down past a sixteenth: clamps, length stays 1 (as 0.16.0) */
        ok = recording[0].length == 1u && recording[0].duration >= RECORD_UNIT / 16u;
        chk("REVIEW  DRUM-X: NOTES' LEN on a recorded hit clamps as before (the natural length is the pads' page's)", ok);
    }
#if MELODEE_DRUM
    {   /* PADS (point 2): DRUM HIT under the shared editor: the first edit of a manual hit converts the cell whole (one undo), a hit's
         * pitch and length are notes_event_edit's, the natural length is reachable, a delete is the shared delete, a pad makes a hit */
        track_t *t;
        int ok;
        uint32_t i, kick, snare;
        fresh("NOTES"); t = TSEL;
        set_engine_of(t, ENGI_DRUM); apply_preset_to(t, 1); t->engine = t->eng_req; panic_req = 0; frames(50);
        ui.home = 0; for (i = 0; i < NPAGES; i++) if (PAGES[i].scope == SC_DRUMHIT) { ui.page = (uint8_t)i; break; }
        ui.force = 1; frame();
        ui.drum_sound = D9_BD; cursor_set(3); frame();
        t->step[3] = (step_t){{0}, 0, ST_NOTE, 0, 100, 3, 2, 0};   /* the kick and the snare, the snare accented, manual */
        drum_hit_edit(2, 7);                            /* the kick's pitch: the cell converts, then the edit */
        for (kick = snare = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) {
            if (recording[i].note == 36u) kick = i;
            if (recording[i].note == 38u) snare = i;
        }
        ok = kick < RECORD_MAX && snare < RECORD_MAX && pool_count() == 2u && recording[kick].pitch == 7 && recording[snare].pitch == 0 &&
             recording[snare].vel == 127u && (t->step[3].flags & SF_RECORDED) && t->step[3].hit == 3u && auth_ok("pad pitch") && prune_noop(t);
        chk("PADS    a manual kick and snare: the kick's pitch edit converts the cell whole; both hits events, the snare's accent kept", ok);
        drum_hit_edit(3, -8);                           /* the kick's length: a half step */
        ok = recording[kick].length && recording[kick].duration * (1u << (recording[kick].owner >> 5)) == RECORD_UNIT / 2u;
        drum_hit_edit(3, -9);                           /* below a sixteenth: the natural length */
        ok &= !recording[kick].length && recording[kick].pitch == 7;
        drum_hit_edit(3, 2);                            /* up from natural: two sixteenths */
        ok &= recording[kick].length && recording[kick].duration * (1u << (recording[kick].owner >> 5)) == RECORD_UNIT / 8u;
        chk("PADS    the kick's length through the shared editor; below a sixteenth it plays its natural length; up from it again", ok && auth_ok("pad length"));
        drum_hit_delete();
        ok = !recording[kick].vel && recording[snare].vel && t->step[3].hit == 2u && (t->step[3].flags & SF_RECORDED) && auth_ok("pad delete") && prune_noop(t);
        step_history_end(); step_history_finish();
        ok &= step_history_apply(0) && recording[kick].vel && t->step[3].hit == 3u && auth_ok("pad delete undone");   /* (EDIT held on DRUM HIT is the sound's undo, as the pads' test drives it) */
        chk("PADS    the kick deleted: the snare's event stays, the cell rebuilt; the history brings the kick back", ok);
        cursor_set(6); frame(); ui.drum_sound = D9_SD;
        drum_hit_key(0);                                /* a pad on an empty step: a snare hit, an event */
        ok = (t->step[6].flags & SF_RECORDED) && t->step[6].hit == 2u && auth_ok("pad key") && prune_noop(t);
        drum_hit_key(1);                                /* the pad with the accent: the velocity to 127 */
        for (snare = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording_view(t, &recording[i]) == 6u) snare = i;
        ok &= snare < RECORD_MAX && recording[snare].vel == 127u;
        drum_hit_key(0);                                /* the pad again: the hit cleared */
        ok &= !step_on(&t->step[6]) && auth_ok("pad cleared");
        chk("PADS    a pad on an empty step makes a hit (an event); the accent pad sets 127; the pad again clears it", ok);
    }
    {   /* (round 12) NOTES' START with the audio arena taken by the engines and the effects: the edit's undo snapshot was one 11 KB block from the arena for
         * any edit (the whole 1,024-slot array), and "NO EDIT MEMORY" came with a mix whose states had filled it. It is the notes in use now (a few hundred bytes
         * with the steps and the motion): every START below runs with less arena than the old snapshot, on typed notes, a chord, a recorded note and a full loop */
        track_t *t;
        int ok;
        uint32_t i, k, found, pos0;
        fresh("NOTES"); t = TSEL;
        {
            uint32_t sz;
            for (k = 0; k < RES_JFX_N; k++) for (sz = 100000u; sz >= 512u; sz = sz * 3u / 4u) if (resource_allocate(RES_JFX0 + k, sz)) break;   /* (each id the biggest block that fits, then smaller) */
            for (;;) {                                                               /* give back the smallest block, until 4 KB fits */
                uint32_t small = RES_JFX_N;
                if (resource_allocate(RES_NOTE_HISTORY, 4096u)) break;
                for (k = 0; k < RES_JFX_N; k++) if (resource[RES_JFX0 + k].size && (small == RES_JFX_N || resource[RES_JFX0 + k].size < resource[RES_JFX0 + small].size)) small = k;
                if (small == RES_JFX_N) break;
                resource_release(RES_JFX0 + small);
            }
            resource_release(RES_NOTE_HISTORY);
            ok = !resource_allocate(RES_NOTE_HISTORY, 11776u);                       /* the old snapshot (10,240 B of notes and the steps) does not fit */
            resource_release(RES_NOTE_HISTORY);
            chk("EDITMEM the arena holds less than the old snapshot's 11.7 KB and room for a small one (the setting of the cells below)", ok);
        }
        manual(3, 60); cursor_set(3); frame();
        ok = 1;
        for (k = 0; k < 40u; k++) { turn(EN_K2, 1); ok &= !str_eq(ui.msg, "NO EDIT MEMORY"); }
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        pos0 = found < RECORD_MAX ? (recording[found].step & 63u) * RECORD_UNIT + recording[found].on : 0u;
        ok &= found < RECORD_MAX && pos0 == 3u * RECORD_UNIT + 40u * (RECORD_UNIT / 16u) && auth_ok("40 detents");
        chk("EDITMEM START on a typed note, 40 detents with the arena short: no NO EDIT MEMORY, the note 2.5 steps on", ok);
        for (k = 0; k < 40u; k++) turn(EN_K2, -1);
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        ok = found < RECORD_MAX && (recording[found].step & 63u) * RECORD_UNIT + recording[found].on == 3u * RECORD_UNIT && auth_ok("back");
        chk("EDITMEM START the other way: back on the note's own place", ok);
        for (k = 0; k < 16u * 16u; k++) turn(EN_K2, 1);                          /* the whole loop: 16 steps of 16 detents */
        for (found = RECORD_MAX, i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i]) if (recording[i].note == 60u) found = i;
        ok = found < RECORD_MAX && (recording[found].step & 63u) * RECORD_UNIT + recording[found].on == 3u * RECORD_UNIT && auth_ok("loop") && !str_eq(ui.msg, "NO EDIT MEMORY");
        chk("EDITMEM START a whole loop of detents: the note is where it began (it wraps at the phrase's end)", ok);
        fresh("NOTES"); t = TSEL;
        {   /* a chord of three, the conversion takes the three events: the undo list holds them */
            uint8_t ch[3] = {67, 60, 64};
            song.seq_mode = 1; cursor_set(5); frame();
            seq_entry_notes(ch, 3); seq_entry_finish(); frame();
            cursor_set(5); frame();
        }
        ui.note_slot = 1;
        for (k = 0; k < 8u; k++) turn(EN_K2, 1);
        ok = pool_count() == 3u && !str_eq(ui.msg, "NO EDIT MEMORY") && auth_ok("chord 8");
        chk("EDITMEM START on one note of a chord, 8 detents with the arena short: all three events, none lost", ok);
        hold(B_EDIT); frame();
        ok = (pool_count() == 3u || pool_count() == 0u) && auth_ok("chord undo");
        chk("EDITMEM the undo after it leaves a consistent pattern", ok);
        fresh("NOTES"); t = TSEL;
        for (i = 0; i < 16u; i++) put_note(i, i, 0, 40u + i, 100, RECORD_UNIT / 2u);   /* a full loop: a recorded note on every step */
        note_rebuild_all(); cursor_set(4); frame();
        notes_selected(t);
        for (k = 0; k < 20u; k++) turn(EN_K2, 1);
        ok = pool_count() == 16u && !str_eq(ui.msg, "NO EDIT MEMORY") && auth_ok("recorded 20");
        chk("EDITMEM START on a recorded note of a full loop (16 notes), 20 detents: the sixteen stay", ok);
        hold(B_EDIT); frame();
        ok = pool_count() == 16u && auth_ok("recorded undo");
        chk("EDITMEM the undo after it keeps the sixteen", ok);
        for (k = 0; k < RES_JFX_N; k++) resource_release(RES_JFX0 + k);
    }
#endif
    printf("one_auth: %d failed\n", nbad);
    return nbad != 0;
#endif
}
