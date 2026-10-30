/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 4 (docs/orangecat/UI_BUILD.md "Step 4: the step pages"): the editing of FEEL, NOTES' knob map and
 * the tracker's chord length, behind OC_STEP_PAGES. Included by ui_input.c before ui_input() (it needs edit_param, the
 * bulk selection and note_tools.c). The page and its reading are ui_feel.c.
 *   - feel_edit: FEEL's four knobs, each set on every selected step (the value worked out from the cursor's step, set,
 *     not added), with the frozen-phrase, chain and live-recording guards of its own (edit_param exempts KNOB 1 of the
 *     step pages from the first: the design review's B3);
 *   - feel_reset: EDIT tapped on FEEL (the cursor's step), SHIFT + EDIT (the selected range);
 *   - steppg_knob: NOTES' knobs PITCH, START, LEN, VEL (slots 1, 6, 2, 3 of notes_event_edit) when no range is selected
 *     and no move is armed; FEEL's knobs; 1: taken;
 *   - bulk_stretch: SHIFT + PRESETS, the selected range's end a step, as OCT+ held with KNOB 2 does;
 *   - tracker_chord_length: the tracker's KNOB 4 in a note column of a manual chord: the chord becomes events and the
 *     column's note takes its own length, in one transaction (one undo).
 * Revisions:
 *   (the new interface 0.4, branch ui-build) first version.
 */
#if OC_STEP_PAGES
static int feel_page(void) { return !ui.home && cur_page()->graph == GR_FEEL; }
/* a melodic NOTES page (the drum grid and a drum track's roll keep release-2's knobs) */
static int notes_page(void) { return !ui.home && cur_page()->graph == GR_ROLL && !grid_on() && !drum_track(TSEL); }

/* every step of the range (or the cursor's) that is on; fn gets the step and the value worked out from the cursor's */
static int feel_guard(void)
{
    if (frz_locked()) return 0;                         /* (a frozen phrase: nothing says why, as the other step edits) */
    if (chain_busy()) { ui_message("STOP TO EDIT"); return 0; }
    if (live_rec_sel()) { ui_message("STOP RECORDING"); return 0; }
    return 1;
}
static void feel_edit(uint32_t k, int32_t steps)
{
    track_t *t = TSEL;
    uint32_t cur = ui.cursor % NSTEP, lo, hi, i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), recorded = 0;
    step_t src;
    uint32_t np = 0;
    if (!feel_guard()) return;
    feel_k = (uint8_t)(k & 3u);
    ui.force = 1;
    if (cur >= len) return;
    feel_range(&lo, &hi);
    if (hi >= len) hi = len - 1u;
    fm1_irq_off();
    src = t->step[cur];
    if (!step_on(&src)) {                               /* a property of nothing */
        fm1_irq_on();
        ui_message("NO NOTE THERE");
        return;
    }
    switch (k & 3u) {
    case 0: {                                           /* CHANCE, then the conditions */
        int32_t d = list_accel(EN_K1, steps, FEEL_POS_MAX + 1u, 0);
        np = (uint32_t)clamp((int32_t)feel_pos(&src) + d, 0, (int32_t)FEEL_POS_MAX);
        break;
    }
    case 1:
        np = (uint32_t)clamp((int32_t)step_rflam(&src) + steps, 1, 5);
        break;
    case 2:
        np = (uint32_t)(clamp((int32_t)src.nudge + steps, NUDGE_MIN, NUDGE_MAX) - NUDGE_MIN);   /* (kept unsigned) */
        break;
    default:
        np = (uint32_t)clamp((int32_t)((src.flags & SF_ACCENT ? 1u : 0u) | (src.flags & SF_SLIDE ? 2u : 0u)) + (steps > 0 ? 1 : -1), 0, 3);
        break;
    }
    for (i = lo; i <= hi; i++) {
        step_t *st = &t->step[i];
        if (!step_on(st)) continue;
        recorded |= st->flags & SF_RECORDED;
        switch (k & 3u) {
        case 0:
            if (np > 100u) step_set_cond(st, np - 100u);
            else { step_set_cond(st, TC_NONE); step_set_chance(st, np); }
            break;
        case 1: step_set_rflam(st, np); break;
        case 2: step_set_nudge(st, (int32_t)np + NUDGE_MIN); break;
        default:
            st->flags = (uint8_t)((st->flags & ~(SF_ACCENT | SF_SLIDE)) | ((np & 1u) ? SF_ACCENT : 0u) | ((np & 2u) ? SF_SLIDE : 0u));
            break;
        }
    }
    if ((k & 3u) == 2u && recorded) recording_resched(t);   /* (a recorded step's notes play the nudge: recording.c) */
    fm1_irq_on();
}
/* the defaults of a step's feel: 100 %, no condition, x1, no nudge, no accent or slide */
static void feel_reset(int range)
{
    track_t *t = TSEL;
    uint32_t cur = ui.cursor % NSTEP, lo = cur, hi = cur, i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), recorded = 0;
    if (!feel_guard()) return;
    if (range) feel_range(&lo, &hi);
    if (hi >= len) hi = len - 1u;
    fm1_irq_off();
    for (i = lo; i <= hi; i++) {
        step_t *st = &t->step[i];
        if (!step_on(st)) continue;
        recorded |= st->flags & SF_RECORDED;
        step_set_chance(st, 100u);
        step_set_cond(st, TC_NONE);
        step_set_rflam(st, 1u);
        step_set_nudge(st, 0);
        st->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE);
    }
    if (recorded) recording_resched(t);
    fm1_irq_on();
    ui.force = 1;
    ui_message(range && hi != lo ? "FEEL RESET" : "STEP RESET");
}
/* KNOB k on a step page: 1 when taken here */
static int steppg_knob(uint32_t k, int32_t s)
{
    static const uint8_t SLOT[4] = {1u, 6u, 2u, 3u};    /* PITCH, START, LEN, VEL */
    if (feel_page()) {
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
        feel_edit(k, s);
        return 1;
    }
    if (notes_page() && !bulk.on && !ui.note_move && !ui.note_copy) {
        ui.hot_col = (uint8_t)k;
        ui.hot_t = 40;
        edit_param(SLOT[k & 3u], s);
        return 1;
    }
    return 0;
}
/* SHIFT + PRESETS on NOTES and FEEL: the range's end a step; with none, one is begun at the cursor (as bulk_jog) */
static void bulk_stretch(int32_t delta)
{
    uint32_t cur = ui.cursor, limit = step_pattern_len(TSEL), to;
    if (frz_locked()) return;
    if (!bulk.on || bulk.on == 2u) {
        bulk.anchor = (uint16_t)cur; bulk.engine = (uint8_t)library_engine(); bulk.track = song.sel;
        bulk.pattern = TSEL->pattern; bulk.generation = TSEL->pattern_gen;
    }
    bulk.on = 1;
    to = (uint32_t)clamp((int32_t)cur + delta, 0, (int32_t)limit - 1);
    cursor_set((int32_t)to);
    bulk.end = (uint16_t)to;
    ui.force = 1;
    ui_message("STEPS SELECTED");
}
/* the tracker's KNOB 4 in a note column of a manual chord: the chord to events and that note's own length, one transaction.
 * 0: not done here (a one-note step, the length unchanged, the song playing, a chain or a recording: the whole-step resize) */
static int tracker_chord_length(uint32_t at, uint32_t col, int32_t steps)
{
    track_t *t = TSEL;
    uint32_t n, want, pitch, i;
    if (at >= NSTEP || song.playing || chain_busy() || live_rec_sel() || frz_locked()) return 0;
    if (t->step[at].n < 2u || col >= t->step[at].n || (t->step[at].flags & SF_RECORDED)) return 0;
    n = step_note_length(t, at);
    want = (uint32_t)clamp((int32_t)n + steps, 1, 128);
    if (want == n) return 0;
    pitch = t->step[at].note[col];
    fm1_irq_off();
    if (!note_transaction_begin()) { fm1_irq_on(); return 1; }
    if (!note_convert(at)) { note_transaction.valid = 0; fm1_irq_on(); return 1; }
    for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
        if ((recording[i].step & 63u) == at && recording[i].note == pitch) {
            note_duration(&recording[i], want * RECORD_UNIT);
            break;
        }
    recording_generation++;
    ui.note_pick = 0;
    notes_selected(t);
    note_transaction_end();
    fm1_irq_on();
    return 1;
}
#endif
