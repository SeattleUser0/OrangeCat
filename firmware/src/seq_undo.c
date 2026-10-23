/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Eight manual STEP edits of the selected track (SEQ > NOTES; CHANCE: the note's sheet): note / chord entry with its held length,
 * SCL + SELECT moves, ENV + SELECT lengths, PITCH, LENGTH, VEL, SLIDE and CHANCE turns. EDIT held undoes the last one (with nothing to undo here:
 * the sound / pattern load undo, ui.c undo_swap); OCT- / OCT+ with EDIT held undo further / redo
 * (ui_input.c). The frames of a held entry or move are one edit; a new edit drops the redo. Exact comparisons start
 * a fresh history after anything else changed the steps: live recording, the editor, a pattern load or clear, another
 * track or LEN. */
/* Recorded edits keep one event before and after each edit. Its storage
 * generation prevents undo from overwriting slots reused by another take. */
/* Revisions:
 * (Orange Cat 0.7.2) event_open / event_replace / event_delete at the end: the one copy of "history, snapshot, remove,
 * restore, rebuild, generation" that NOTES' editors and the pads' used to carry each (docs/orangecat/TRACKER_REVIEW.md);
 * events_delete_view: a manual overwrite of a recorded step deletes its events through the history. */
#define STEP_HISTORY 8u
static struct {
    struct {
        step_snapshot_t step[NSTEP];
        recorded_note_t removed, replacement;
        uint16_t removed_index, selection;
        uint8_t cursor, note_slot, has_removed;
    } state[STEP_HISTORY + 1u];
    step_t live[NSTEP];                          /* the steps as the last frame left them */
    uint32_t pattern_gen;
    uint32_t recording_gen;
    recorded_note_t removed, replacement;
    uint16_t removed_index;
    uint8_t has_removed;
    uint8_t valid, track, len;
    uint8_t head, count, pos, pending, cursor_before, slot_before;
} step_history __attribute__((section(".pool")));

static void step_history_clear(void) { step_history.valid = 0; }

static uint32_t step_history_index(void)
{
    return (step_history.head + step_history.pos) % (STEP_HISTORY + 1u);
}

static int step_history_context(void)
{
    return step_history.valid && step_history.recording_gen == recording_generation &&
           step_history.pattern_gen == TSEL->pattern_gen && step_history.track == song.sel && step_history.len == step_pattern_len(TSEL) &&
           !(song.playing && (song.rec & (1u << song.sel)));
}

/* before the input: a change from outside the manual editor starts a fresh history. The audio ISR may record: the
 * steps are compared and copied with the IRQs off */
static void step_history_sync_locked(void)
{
    if (!step_history_context() || memcmp(step_history.live, TSEL->step, sizeof step_history.live)) {
        step_history.head = step_history.pos = step_history.pending = 0;
        step_history.count = 1;
        step_history.track = song.sel;
        step_history.pattern_gen = TSEL->pattern_gen;
        step_history.recording_gen = recording_generation;
        step_history.has_removed = step_history.state[0].has_removed = 0;
        step_history.len = (uint8_t)step_pattern_len(TSEL);
        step_snapshot_save(step_history.state[0].step, TSEL->step);
        memcpy(step_history.live, TSEL->step, sizeof step_history.live);
        step_history.state[0].cursor = ui.cursor;
        step_history.state[0].selection = ui.note_pick;
        step_history.state[0].note_slot = ui.note_slot;
        step_history.valid = !(song.playing && (song.rec & (1u << song.sel)));
    }
    step_history.cursor_before = ui.cursor;
    step_history.slot_before = ui.note_slot;
}
static void step_history_sync(void)
{
    fm1_irq_off();
    step_history_sync_locked();
    fm1_irq_on();
}

/* an edit done (no keys or edit modifier held any more): kept, if the steps changed (moving the cursor keeps the redo) */
static void step_history_finish(void)
{
    uint32_t at = step_history_index();
    if (!step_history_context()) {
        step_history_clear();
        return;
    }
    if (step_history.has_removed || !step_snapshot_equal(step_history.state[at].step, step_history.live)) {
        step_history.count = (uint8_t)(step_history.pos + 1u);   /* the redo goes */
        if (step_history.count == STEP_HISTORY + 1u) {
            step_history.head = (uint8_t)((step_history.head + 1u) % (STEP_HISTORY + 1u));
            step_history.pos--;
            step_history.count--;
        }
        step_history.pos++;
        step_history.count++;
        at = step_history_index();
        step_snapshot_save(step_history.state[at].step, step_history.live);
        step_history.state[at].cursor = ui.cursor;
        step_history.state[at].selection = ui.note_pick;
        step_history.state[at].note_slot = ui.note_slot;
        step_history.state[at].has_removed = step_history.has_removed;
        note_assign(&step_history.state[at].removed,&step_history.removed);
        note_assign(&step_history.state[at].replacement,&step_history.replacement);
        step_history.state[at].removed_index = step_history.removed_index;
    }
    step_history.has_removed = 0;
    step_history.pending = 0;
}

/* after the input: the steps as this frame left them, so a change from outside on the next frame is no part of an
 * edit on the panel */
static void step_history_end(void)
{
    fm1_irq_off();
    if (!step_history_context()) {
        step_history_clear();
        fm1_irq_on();
        return;
    }
    if (step_history.has_removed || memcmp(step_history.live, TSEL->step, sizeof step_history.live)) {
        if (ui.home || (cur_page()->scope != SC_STEP && (!MELODEE_DRUM || cur_page()->scope != SC_DRUMHIT))) {  /* a pattern load, a clear, the editor: not a STEP edit */
            step_history_clear();
            fm1_irq_on();
            return;
        }
        if (!step_history.pending) {
            step_history.state[step_history_index()].cursor = step_history.cursor_before;
            step_history.state[step_history_index()].note_slot = step_history.slot_before;
        }
        step_history.pending = 1;
        memcpy(step_history.live, TSEL->step, sizeof step_history.live);
    }
    fm1_irq_on();
    if (!ui.entry_open && !ui.step_move)
        step_history_finish();
}

/* 1 = undone / redone, 0 = nothing there (the caller may undo something else) */
static int step_history_apply(int redo)
{
    uint32_t at;
    if(note_tools_history(redo))return 1;
    fm1_irq_off();
    step_history_finish();                       /* a gesture still held is an edit too */
    if (!step_history_context() || (redo ? step_history.pos + 1u >= step_history.count : !step_history.pos)) {
        fm1_irq_on();
        return recording_history(redo);
    }
    uint32_t change = redo ? (step_history.head + step_history.pos + 1u) % (STEP_HISTORY + 1u) : step_history_index();
    if (step_history.state[change].has_removed) {
        uint32_t i = step_history.state[change].removed_index;
        recording_remove(TSEL, i);
        recorded_note_t r = redo ? step_history.state[change].replacement : step_history.state[change].removed;
        if (r.vel) recording_restore_note(TSEL, i, r);
    }
    step_history.pos = (uint8_t)(step_history.pos + (redo ? 1 : -1));
    at = step_history_index();
    step_snapshot_load(TSEL->step, step_history.state[at].step);
    memcpy(step_history.live, TSEL->step, sizeof step_history.live);
    step_history.recording_gen = recording_generation;
    fm1_irq_on();
    cursor_set(step_history.state[at].cursor);
    ui.note_pick = step_history.state[at].selection;
    ui.note_slot = step_history.state[at].note_slot;
    ui.note_generation = recording_generation;
    ui.hot_t = 0;
    ui.force = 1;
    ui_message(redo ? "REDO" : "UNDO");
    return 1;
}

/* (Orange Cat 0.7.2) The one edit primitive of a recorded note. notes_event_edit, notes_delete_edit, drum_hit_edit and
 * drum_hit_delete (ui_input.c) each wrote out the same sequence: end and close the pending edit, interrupts off,
 * resync the history, record the event before and after in the history (removed_index, has_removed), remove it and
 * restore the replacement, bring the overview cell(s) up to date, accept the new recording generation. Now:
 *   event_open()                  the beginning: the editor chooses its event and computes the new one after it, with
 *                                 the interrupts off, as before
 *   event_replace(t, i, after, f) the event in slot i becomes *after (a free slot: a new event; undo removes it)
 *   event_delete(t, i, f)         the event in slot i is gone (the history keeps its bytes)
 *   event_close()                 the interrupts on, the screen redrawn
 * The primitives leave the interrupts off: an editor that patches the overview cell itself (the pads) does so before
 * event_close, in the same window, as before. Flags (what the callers differed in):
 *   EV_MARK     remember the pick and the cursor for the undo (NOTES' editors; the pads never did)
 *   EV_REBUILD  rebuild the overview cell(s) from the events (notes_rebuild); the pads patch their cell instead
 *   EV_FOLLOW   the cursor follows the moved note's step (before the pick is set: cursor_set clears it)
 *   EV_NEXT     the pick moves on to the next note before this one goes (NOTES' deletion)
 * The history holds one event delta an entry, so each call is one undo step. */
enum { EV_MARK = 1, EV_REBUILD = 2, EV_FOLLOW = 4, EV_NEXT = 8 };
static void event_open(void)
{
    step_history_end();
    step_history_finish();
    fm1_irq_off();
    step_history_sync_locked();                 /* a recorder ISR could have run since the frame's first sync */
}
static void event_close(void)
{
    fm1_irq_on();
    ui.force = 1;
}
static void event_mark(void)
{
    step_history.state[step_history_index()].selection = ui.note_pick;
    step_history.state[step_history_index()].cursor = ui.cursor;
}
static void event_replace(track_t *t, uint32_t i, const recorded_note_t *after, uint32_t how)
{
    recorded_note_t before = recording_snapshot(i);
    if (!before.vel) memset(&before, 0, sizeof before);   /* (a free slot: undo removes the new event) */
    uint32_t oldview = recording_view(t, &before), view = recording_view(t, after);
    if (how & EV_MARK) event_mark();
    note_assign(&step_history.removed, &before);
    note_assign(&step_history.replacement, after);
    step_history.removed_index = (uint16_t)i;
    step_history.has_removed = 1;
    if (before.vel) recording_remove(t, i);
    recording_restore_note(t, i, *after);
    t->step[view].time = ST_NOTE;
    t->step[view].flags |= SF_RECORDED;
    if (how & EV_REBUILD) {
        notes_rebuild(t, oldview);
        if (view != oldview) notes_rebuild(t, view);
    }
    step_history.recording_gen = recording_generation;
    if (how & EV_FOLLOW) cursor_set(after->step & 63u);
    ui.note_pick = (uint16_t)(i + 1u);
    note_assign(&ui.note_identity, after);
    ui.note_generation = recording_generation;
}
static void event_delete(track_t *t, uint32_t i, uint32_t how)
{
    uint32_t view = recording_view(t, &recording[i]);
    if (how & EV_MARK) event_mark();
    step_history.removed = recording_snapshot(i);
    memset(&step_history.replacement, 0, sizeof step_history.replacement);
    step_history.removed_index = (uint16_t)i;
    step_history.has_removed = 1;
    if (how & EV_NEXT) notes_cycle(1);
    recording_remove(t, i);
    if (how & EV_REBUILD) notes_rebuild(t, view);
    step_history.recording_gen = recording_generation;   /* accept only this protected UI mutation */
    ui.note_generation = recording_generation;
}

/* (Orange Cat 0.7.2) A manual overwrite of a recorded step (a key on the step page or the tracker, a grid hit, a step
 * delete) used to clear SF_RECORDED and leave the events in the pool for recording_prune. Now the events of the
 * cell are deleted first, each through the history (one entry each: the history holds one event delta an entry), so
 * EDIT held brings the recording back, an event at a time. Call it before the cell is changed: an event is active
 * only while its cell is a NOTE step flagged SF_RECORDED. The track is the selected one (the history is its) */
static void events_delete_view(track_t *t, uint32_t view)
{
    for (;;) {
        uint32_t i;
        for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
            if (recording_active(t, i) && recording_view(t, &recording[i]) == view)
                break;
        if (i >= RECORD_MAX)
            return;                             /* (the last delta stays open: it joins the overwrite's own history entry) */
        event_open();
        if (recording_active(t, i) && recording_view(t, &recording[i]) == view)   /* (a recorder ISR may have been first) */
            event_delete(t, i, 0);
        fm1_irq_on();
    }
}
