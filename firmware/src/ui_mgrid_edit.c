/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The melodic step grid, the input (branch melodic-grid; docs/orangecat/MELODIC_GRID_PLAN.md and the owner's answers of November 3, 2026; behind OC_MGRID).
 * Picture and state: ui_mgrid.c.
 *
 * Two stages, said on the screen (the pill, the frame) and on the keys (the lights):
 *   STEP  the keys are silent. A white key picks the step it stands for (16 to a page; a step past LEN does nothing); the first four black keys show pages 1 to 4 (a page past
 *         LEN does nothing); the other black keys do nothing. Picking never changes a step. When the picking key is let go the keyboard goes to NOTE.
 *   NOTE  the keys play, and what is played is the picked step's notes: the first key down starts the step afresh, the keys held with it are its chord (up to four, black keys
 *         are sharps), as step entry has always worked (seq_entry). When every key is up the keyboard goes back to STEP, the cursor on the same step (it never moves by itself:
 *         seq_entry_finish does not advance on this page). OCT- before that cancels: the entry is undone, the step as it was.
 * External MIDI notes go into the picked step in either stage (seq_midi_events). KNOB 1 moves the cursor (pages follow it) in either stage, KNOB 2 picks a note of the chord,
 * KNOB 3 its pitch, KNOB 4 its length (the NOTES page's edits of the same step: step_edit, notes_event_edit). EDIT tapped deletes the picked note, or the step; EDIT held
 * undoes (the page is a step page: seq_undo.c). A frozen phrase refuses what NOTES refuses. Live recording armed and playing: the page is inert and the keys record live.
 * Revisions:
 *   (melodic-grid) first version.
 */
#if OC_MGRID
static int mgrid_on(void) { return mgrid_page() && !ui.menu && !ui.confirm && mgrid_track(TSEL) && !name_on(); }
/* STEP stage with the keys the UI's (seq.c keyboard_block reads it through song.grid 3, ui_name.c keys_mode) */
static int mgrid_stage_step(void) { return mgrid_on() && !mg.stage && !live_rec_sel(); }
static void mgrid_reset(void) { mg.stage = mg.arm = mg.played = 0; }
/* the stage changes: the keys are the UI's or the player's from this pass on (seq.c keyboard_block reads song.grid), not from the next */
static void mgrid_stage(uint32_t v)
{
    mg.stage = (uint8_t)v;
    mg.arm = mg.played = 0;
    song.grid = (uint8_t)keys_mode();
    ui.force = 1;
}

/* the key LEDs in STEP stage: bright the picked step and the page shown, dim the steps that hold something and the other pages in LEN; the step playing inverted */
static uint32_t mgrid_leds(uint32_t *dim)
{
    const track_t *t = TSEL;
    uint32_t k, m = 0, d = 0, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), base = mgrid_bank(t) * 16u;
    uint32_t ph = song.playing && t->seq_idx < len && t->seq_idx / 16u == base / 16u ? t->seq_idx % 16u : 0xFFu;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), i = base + p, on = 0, dm = 0;
        if (!key_black(k)) {
            if (i < len) {
                on = i == ui.cursor;
                dm = step_on(&t->step[i]) || t->step[i].time == ST_TIE;
                on ^= (uint32_t)(p == ph);
            }
        } else if (p < 4u && p * 16u < len) {
            on = p == base / 16u;
            dm = !on;
        }
        m |= on << k;
        d |= (dm & !on) << k;
    }
    *dim = d;
    return m;
}

/* one UI pass on the page: the presses of the keys (not a layer's), the MIDI notes, the stage change */
static void mgrid_frame(uint32_t pressed)
{
    uint32_t held = fm1_in.notes & ~kb_layer, k, len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    if (mg.trk != song.sel) { mgrid_reset(); mg.trk = song.sel; }   /* another track: the stage was the other's */
    if (live_rec_sel()) {                                   /* armed and playing: the keys and MIDI record live, not into the step */
        ui.entry_open = 0;
        mgrid_reset();
        seq_midi_events(0);
        return;
    }
    if (mg.stage && mg.played && !ui.entry_open && !step_midi_held)   /* every note let go: back to STEP, the cursor where it was */
        mgrid_stage(0);
    if (!mg.stage) {
        uint32_t base = mgrid_bank(TSEL) * 16u;
        for (k = 0; k < 27u; k++) {
            uint32_t p;
            if (!((pressed >> k) & 1u)) continue;
            p = key_place(k);
            if (!key_black(k)) {
                if (base + p < len) {                       /* a step of this page: picked (nothing is written) */
                    cursor_set((int32_t)(base + p));
                    mg.arm = 1;
                }
            } else if (p < 4u && p * 16u < len) {           /* a page; the cursor stays */
                ui.bank = (uint8_t)p;
                ui.force = 1;
            }
        }
        if (chain_busy()) seq_midi_events(0); else seq_midi_events(1);   /* MIDI goes into the picked step */
        if (mg.arm && !held)                                /* the picking key is up: the keyboard plays */
            mgrid_stage(1);
        return;
    }
    if (chain_busy()) {
        seq_midi_events(0);
        if (pressed) ui_message("STOP TO EDIT");
        return;
    }
    for (k = 0; k < 27u; k++)
        if (((pressed >> k) & 1u) && kb_map(TSEL, k) != KB_SILENT) mg.played = 1;
    seq_entry(pressed);
    if (ui.entry_open) mg.played = 1;
}

/* OCT- in NOTE stage: back to STEP, the step as it was. 1 = taken here. A finished entry (every note up) is not undone: that stage is over */
static int mgrid_cancel(void)
{
    if (!mgrid_on() || !mg.stage) return 0;
    if (mg.played && !ui.entry_open && !step_midi_held) {   /* it ended with the last note: OCT- is Esc, as everywhere */
        mgrid_stage(0);
        return 0;
    }
    if (ui.entry_open) {
        step_history_end();
        if (step_history.pending || step_history.has_removed) step_history_apply(0);   /* the entry changed the step: that one edit back */
        ui.entry_open = 0;
    }
    mgrid_stage(0);
    ui_message("CANCELLED");
    return 1;
}

/* KNOB k (0..3): STEP (the cursor, pages follow), NOTE (the chord's next or previous note, a recorded step's events), PITCH, LEN of the picked note */
static void mgrid_knob(uint32_t k, int32_t s)
{
    track_t *t = TSEL;
    if (k == 1u) {
        uint32_t chosen = notes_selected(t), at = notes_manual_start(t);
        if (chosen < RECORD_MAX) notes_cycle(s > 0 ? 1 : -1);
        else if (at < NSTEP && t->step[at].n) ui.note_slot = (uint8_t)clamp((int32_t)ui.note_slot + s, 0, (int32_t)t->step[at].n - 1);
        notes_preview();
        ui.force = 1;
        return;
    }
    edit_param(k == 0u ? 0u : k == 2u ? 1u : 2u, s);
}
#endif
