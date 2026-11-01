/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix 7 (docs/orangecat/UI_BUILD.md "ui-fix 7"): one authority for every note, behind OC_ONE_AUTH. The recorded note is the one
 * form of a note that has its own start or length; a step converts whole (note_convert); every manual gesture on a recorded cell
 * deletes the cell's events first; a manual step stays the compact form of notes that share their length and start on the grid.
 * Here, the edits the step pages needed for it (included by ui_input.c after ui_feel_edit.c; the reconcile at the load boundaries is
 * recording.c's recording_reconcile):
 *   - start_convert_edit: START (NOTES' KNOB 2) on a manual note converts the step and moves that note, in one note transaction
 *     (one undo; EDIT held returns the manual step); the pitch is read before the conversion and the note found by it after, so
 *     a chord entered out of order moves the note under the cursor; a move into a cell that holds a separate manual note rolls the
 *     conversion back ("STEP OCCUPIED"); refused while the song plays, a chain runs or a recording is armed ("STOP TO EDIT", as
 *     LEN); a full pool leaves the step manual ("RECORDING FULL"). The ties become the note's duration and their cells go (as LEN).
 *   - tracker_recorded_move: the tracker's SCL + SELECT on a recorded step (the cursor on it or on one of its TIE cells) moves
 *     every event of the cell exactly a step, its onset inside the step kept, in one transaction (one undo); a destination that
 *     holds a separate manual note rolls it back ("STEP OCCUPIED"); into a recorded cell the events join it (as note_transform's
 *     move). Never a flattening of the recording into a manual step (SPEC.md 6: no silent merge). ENV + SELECT there says
 *     "K4 SETS A NOTE'S LENGTH" (the tracker's KNOB 4 does it per note).
 *   - events_drop_view: the web editor's ED_STEP_SET on a recorded cell: the cell's events go (plainly: the editor has no step
 *     history) before the cell is written.
 * Revisions:
 *   (ui-fix 7, branch ui-fix) first version.
 */
#if OC_ONE_AUTH
/* event i picked for the shared editors (notes_selected keeps a pick only with the track and the bank it was made on) */
static void pick_event(const track_t *t, uint32_t i)
{
    ui.note_pick = (uint16_t)(i + 1u);
    note_assign(&ui.note_identity, &recording[i]);
    ui.note_generation = recording_generation;
    ui.note_track = (uint8_t)trk_index(t);
    ui.note_pattern_gen = t->pattern_gen;
}
static void start_convert_edit(int32_t steps)
{
    track_t *t = TSEL;
    uint32_t at, pitch, i, len = step_pattern_len(t), view0, view1;
    recorded_note_t r;
    if (frz_locked()) return;                           /* (a frozen phrase: silent, as LEN) */
    if (song.playing || chain_busy() || live_rec_sel()) { ui_message("STOP TO EDIT"); return; }
    at = notes_manual_start(t);
    if (at >= NSTEP || !t->step[at].n) { ui_message("SELECT A NOTE"); return; }
    pitch = t->step[at].note[notes_manual_slot(&t->step[at])];
    fm1_irq_off();
    if (!note_transaction_begin()) { fm1_irq_on(); return; }
    if (!note_convert(at)) { note_transaction.valid = 0; fm1_irq_on(); return; }   /* (RECORDING FULL said; the step stays manual) */
    for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
        if ((recording[i].step & 63u) == at && recording[i].note == pitch) break;
    r = i < RECORD_MAX ? recording_snapshot(i) : (recorded_note_t){0};
    view0 = recording_view(t, &r);
    note_onset(&r, steps * (int32_t)(RECORD_UNIT / 16u), len, 0);
    view1 = recording_view(t, &r);
    if (i >= RECORD_MAX || (view1 != view0 && notes_manual_occupied(t, view1))) {   /* back to the manual step, nothing converted */
        memcpy(recording, note_transaction.notes, sizeof recording);
        memcpy(t->step, note_transaction.steps, sizeof note_transaction.steps);
        recording_reindex();
        cursor_set(note_transaction.cursor);
        ui.note_pick = 0;
        note_transaction.valid = 0;
        fm1_irq_on();
        ui_message(i >= RECORD_MAX ? "SELECT A NOTE" : "STEP OCCUPIED");
        return;
    }
    recording_remove(t, i);
    recording_restore_note(t, i, r);
    t->step[view1].time = ST_NOTE;                      /* (the destination flagged before the rebuild counts it) */
    t->step[view1].flags |= SF_RECORDED;
    notes_rebuild(t, view0);
    if (view1 != view0) notes_rebuild(t, view1);
    cursor_set((int32_t)(r.step & 63u));
    pick_event(t, i);                                   /* the next detent goes on with this note (slot 6) */
    note_transaction_end();
    fm1_irq_on();
}
/* the tracker's SCL + SELECT on a recorded step: every event of cell cur (the recorded note's start cell: the caller resolves a TIE
 * cell to it) exactly delta steps over, its onset inside the step kept (note_transform would snap the first onset to the grid), in
 * one transaction; the cell's events are collected first (a move re-links an event by its time, so a walk that moved as it went
 * would skip the rest: the diff review's 1); a destination that holds a separate manual note rolls it back ("STEP OCCUPIED");
 * into a recorded cell the events join it; the cursor follows the earliest moved note. Nothing views the cell: refused before
 * any transaction (so the step history is not cleared for a no-op) */
static void tracker_recorded_move(uint32_t cur, int32_t delta)
{
    track_t *t = TSEL;
    uint32_t len = step_pattern_len(t), i, n = 0, to = NSTEP;
    uint16_t ev[64];
    uint64_t touched = (uint64_t)1 << (cur % NSTEP);
    if (frz_locked()) return;
    if (song.playing || chain_busy() || live_rec_sel()) { ui_message("STOP TO EDIT"); return; }
    for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
        if (recording_active(t, i) && recording_view(t, &recording[i]) == cur) {
            if (n == 64u) { ui_message("TOO MANY NOTES"); return; }   /* (round 11) more than the list holds: refused whole, not moved in part */
            ev[n++] = (uint16_t)i;
        }
    if (!n) { ui_message("SELECT A NOTE"); return; }
    fm1_irq_off();
    if (!note_transaction_begin()) { fm1_irq_on(); return; }
    for (i = 0; i < n; i++) {                           /* the destinations first */
        recorded_note_t r = recording_snapshot(ev[i]);
        uint32_t view;
        note_onset(&r, delta * (int32_t)RECORD_UNIT, len, 0);
        view = recording_view(t, &r);
        if (view != cur && notes_manual_occupied(t, view)) {
            note_transaction.valid = 0;
            fm1_irq_on();
            ui_message("STEP OCCUPIED");
            return;
        }
    }
    for (i = 0; i < n; i++) {
        recorded_note_t r = recording_snapshot(ev[i]);
        uint32_t view;
        note_onset(&r, delta * (int32_t)RECORD_UNIT, len, 0);
        view = recording_view(t, &r) % NSTEP;
        touched |= (uint64_t)1 << view;
        if (view < to) to = view;
        recording_remove(t, ev[i]);
        recording_restore_note(t, ev[i], r);
        t->step[view].time = ST_NOTE;                   /* (the destination flagged before the rebuild counts it) */
        t->step[view].flags |= SF_RECORDED;
    }
    for (i = 0; i < NSTEP; i++) if ((touched >> i) & 1u) notes_rebuild(t, i);
    cursor_set((int32_t)to);
    ui.note_pick = 0;
    note_transaction_end();
    fm1_irq_on();
    ui_message("NOTES MOVED");
}
/* the events of track t's cell view, gone (no history: the web editor's step put), a cell beyond LEN with dormant events too */
static void events_drop_view(track_t *t, uint32_t view)
{
    uint32_t i;
    fm1_irq_off();
    for (i = recording_head[recording_owner(t)]; i < RECORD_MAX;) {
        uint32_t next = recording_next[i];
        if (recording[i].vel && recording_view(t, &recording[i]) == view) recording_remove(t, i);
        i = next;
    }
    fm1_irq_on();
}
#endif
