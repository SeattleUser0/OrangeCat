/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface (OC_CLIP; docs/orangecat/UI_BUILD.md step 10): copy and paste of a sound. SHIFT + OCT- copies the selected track's
 * sound into one clipboard, SHIFT + OCT+ pastes it on the selected track (any track, any time the sound can be loaded).
 *
 * What a sound is here is what EDIT held (undo) swaps for a sound load (ui.c undo_step): the engine, the preset and the user slot, every
 * parameter that param_kept does not keep (the mix, ARP, the pattern parameters, the effects, DUB's setup stay the track's own), the native
 * FM6, CZ-1 and Prophet patches, the DRUM patch or DRUM-X's kit. Not the steps, not motion (a sound load resets a track's motion, and so
 * does a paste), and the macros' dials come across as a user preset load leaves them (0, the center).
 *
 * A paste is a sound load like any other: load_begin / load_end, so EDIT held undoes it whole and one undo level is used. An engine that is
 * not there (a module not installed, a retired slot) or that the unit cannot carry (eng_admit) refuses the paste whole and says why.
 *
 * Included by ui_input.c before ui_shift.c.
 *
 * Revisions:
 *   (the new interface 0.10, branch ui-build, UI_BUILD.md step 10) first version.
 */
typedef struct {
    uint8_t valid, eng, preset, user_native;
    uint16_t user;
    int16_t p[P_COUNT];
    uint8_t fm6[FP_SIZE + 1u];
    cz_patch_t cz;
    p5_patch_t p5;
    union { drum_patch_t drum; dxk_t dxk; } k;          /* which, eng says (as undo_t's) */
} snd_clip_t;
static snd_clip_t snd_clip;

static void sound_copy(void)
{
    track_t *t = TSEL;
    uint32_t i = trk_index(t);
    if (!eng_ok(t->eng_req)) { ui_message("ENGINE NOT HERE"); return; }   /* (a retired slot: nothing to paste later) */
    fm1_irq_off();                                      /* the audio ISR must not see half a sound */
    snd_clip.eng = t->eng_req;
    snd_clip.preset = t->preset;
    snd_clip.user = t->user;
    snd_clip.user_native = t->user_native;
#if OC_HOME_PIC
    for (uint32_t j = 0; j < P_COUNT; j++) snd_clip.p[j] = mac_saved_value(t, j);   /* (round 9, the review of the interface branch: the sound as it sounds, the knob macros committed into their targets as a save does; a plain copy lost an off-centre macro on the paste) */
#else
    memcpy(snd_clip.p, t->p, sizeof snd_clip.p);
#endif
    memset(snd_clip.fm6, 0, sizeof snd_clip.fm6);
    memcpy(snd_clip.fm6, fm6_patch[i], FP_SIZE);
    snd_clip.cz = cz_patch[i];
    snd_clip.p5 = *p5_patch_of(t);
    if (MELODEE_DRUMX && t->eng_req == ENGI_DRUMX) snd_clip.k.dxk = dxk[i];
    else snd_clip.k.drum = drum_patch[i];
    fm1_irq_on();
    snd_clip.valid = 1;
    ui_message("SOUND COPIED");
}

static void sound_paste(void)
{
    track_t *t = TSEL;
    uint32_t i = trk_index(t), k;
    int rc;
    if (!snd_clip.valid) { ui_message("NOTHING COPIED"); return; }
    if (chain_busy() || live_rec_sel()) { ui_message("STOP TO EDIT"); return; }   /* (as INIT: a load resets the track's motion) */
    if (frz_locked()) { ui_message("FROZEN"); return; }                          /* (a frozen phrase keeps its sound) */
    if (!eng_ok(snd_clip.eng)) { ui_message("ENGINE NOT HERE"); return; }
    rc = eng_admit(t, snd_clip.eng);
    if (rc) { ui_message(eng_admit_text(rc)); return; }   /* refused whole: the track keeps its sound */
    undo_seal();                                        /* (a level of its own: EDIT held returns to before the paste, not before a browse) */
    load_begin(t, UNDO_SOUND);                          /* (the copy for EDIT held = undo) */
    panic_req |= (uint8_t)(1u << i);
    fm1_irq_off();
    t->eng_req = snd_clip.eng;
    t->preset = snd_clip.preset;
    t->user = snd_clip.user && up_used(snd_clip.user - 1u) ? snd_clip.user : 0u;   /* (a user slot erased since: no stale name) */
    t->user_native = snd_clip.user_native;
    for (k = 0; k < P_COUNT; k++)
        if (!param_kept(k)) {
            const param_desc_t *d = param_desc_of(snd_clip.eng, k);   /* (a module replaced in its slot since: the values clamp to its ranges) */
            int32_t v = param_reserved(k) ? 0 : snd_clip.p[k];
            t->p[k] = (int16_t)(d && d->max > d->min ? clamp(v, d->min, d->max) : v);
        }
    if (MELODEE_DRUMX && snd_clip.eng == ENGI_DRUMX) { dxk_put(i, &snd_clip.k.dxk); memset(&drum_patch[i], 0, sizeof drum_patch[i]); }
    else { drum_patch[i] = snd_clip.k.drum; dxk_off(i); }
    p5_patch[i] = snd_clip.p5; p5_ready[i] = 1;
    cz_patch[i] = snd_clip.cz;
    if (MELODEE_CZ) cz_track_accept(t);
    fm1_irq_on();
    fm6_set_patch(i, snd_clip.fm6);
    load_end(t);
    sync_reload = 1;
    ui.force = 1;
    ui_message("SOUND PASTED");
}

#if OC_CLIP_SEL
/* ---- (the new interface 0.11, UI_BUILD.md step 11) copy and paste on NOTES and FEEL -------------------------------------------------
 * NOTES: SHIFT + OCT- copies the selected notes (a range, an event selection, the picked note, or the manual step under the cursor) into
 * a clipboard of at most CLIPN_MAX notes, each with its distance from the start of the earliest one's step; SHIFT + OCT+ writes them at the cursor, on this
 * track and pattern or any other, as one note transaction (EDIT held undoes it whole). A paste that would land on a manual note or would
 * not fit the recording memory refuses whole and says so. The other recorded steps keep their chance and slide (the overview rebuild
 * clears the rest of a rebuilt step's feel, as a move does); a landing step that was recorded keeps its own, one that was not takes the
 * copied notes'. A copy that fails (too many notes, none selected) leaves the clipboard as it was. Manual steps are copied as the notes
 * note_convert would make of them (each note of the chord at the step's length, an accent as velocity 127); a step's chance and slide
 * ride with each note. FEEL: SHIFT + OCT- copies the cursor step's feel (chance or condition, ratchet or flam, nudge, accent, slide),
 * SHIFT + OCT+ sets it on every step of the range (or the cursor's) that is on, as feel_reset does. Melodic tracks only (a drum track's
 * roll and grid keep release-2's tools). */
#define CLIPN_MAX 64u
static struct {
    uint8_t n, prob, slide[CLIPN_MAX];
    uint8_t pr[CLIPN_MAX];
    int32_t rel[CLIPN_MAX];
    recorded_note_t r[CLIPN_MAX];
} notes_clip;
static uint32_t clip_seen;                               /* (the first pass of a copy only counts, so a failed copy keeps the old clipboard) */
static uint8_t clip_dry;
static int notes_clip_add(const recorded_note_t *r, uint32_t pos, uint8_t prob, uint8_t slide)
{
    clip_seen++;
    if (clip_dry) return 1;
    if (notes_clip.n >= CLIPN_MAX) return 0;
    notes_clip.r[notes_clip.n] = *r;
    notes_clip.rel[notes_clip.n] = (int32_t)pos;         /* (made relative below) */
    notes_clip.pr[notes_clip.n] = prob;
    notes_clip.slide[notes_clip.n] = slide;
    notes_clip.n++;
    return 1;
}
static int notes_gather(void)                            /* the selection's notes into the clipboard (or only counted, clip_dry) */
{
    track_t *t = TSEL;
    uint32_t i, s, chosen = notes_selected(t), len = step_pattern_len(t), at = notes_manual_start(t);
    int ok = 1;
    for (i = recording_head[recording_owner(t)]; i < RECORD_MAX; i = recording_next[i])
        if (note_transform_has(i, chosen)) {
            const step_t *v = &t->step[recording_view(t, &recording[i])];
            ok &= notes_clip_add(&recording[i], (recording[i].step & 63u) * RECORD_UNIT + recording[i].on, v->probability, (uint8_t)(v->flags & SF_SLIDE));
        }
    for (s = 0; s < len; s++) {                          /* manual steps: as note_convert makes them */
        const step_t *st = &t->step[s];
        if ((st->flags & SF_RECORDED) || !step_on(st)) continue;
        if (bulk.on ? !(bulk.on == 1u && bulk_has(s)) : s != at) continue;
        for (uint32_t j = 0; j < st->n; j++) {
            recorded_note_t r = {0};
            r.note = st->note[j]; r.vel = (st->flags & SF_ACCENT) ? 127 : st->vel ? st->vel : 96;
            r.owner = (uint8_t)recording_owner(t); r.step = (uint8_t)s; r.length = 0;
            note_duration(&r, step_note_length(t, s) * RECORD_UNIT);
            ok &= notes_clip_add(&r, s * RECORD_UNIT, st->probability, (uint8_t)(st->flags & SF_SLIDE));
        }
    }
    return ok;
}
static void notes_copy(void)
{
    track_t *t = TSEL;
    uint32_t i;
    int32_t ref = 0x7fffffff;
    if (!bulk.on && notes_selected(t) >= RECORD_MAX && notes_manual_start(t) >= NSTEP) { ui_message("SELECT A NOTE"); return; }
    fm1_irq_off();
    clip_dry = 1; clip_seen = 0;
    notes_gather();
    clip_dry = 0;
    if (clip_seen && clip_seen <= CLIPN_MAX) { notes_clip.n = 0; notes_gather(); }
    fm1_irq_on();
    if (!clip_seen) { ui_message("SELECT A NOTE"); return; }
    if (clip_seen > CLIPN_MAX) { ui_message("TOO MANY NOTES"); return; }
    for (i = 0; i < notes_clip.n; i++) if (notes_clip.rel[i] < ref) ref = notes_clip.rel[i];
    ref -= ref % (int32_t)RECORD_UNIT;                   /* (from the start of the earliest note's step: its onset inside the step comes across) */
    for (i = 0; i < notes_clip.n; i++) notes_clip.rel[i] -= ref;
    ui_message("NOTES COPIED");
}
static void notes_paste(void)
{
    track_t *t = TSEL;
    uint32_t i, k, len = step_pattern_len(t), free = 0, first = NSTEP;
    recorded_note_t r[CLIPN_MAX];
    uint8_t dest[CLIPN_MAX / 8u + 1u];
    if (!notes_clip.n) { ui_message("NOTHING COPIED"); return; }
    if (song.playing || chain_busy() || live_rec_sel()) { ui_message("STOP TO EDIT"); return; }
    if (frz_locked()) return;
    for (i = 0; i < RECORD_MAX; i++) free += !recording[i].vel;
    if (notes_clip.n > free) { ui_message("RECORDING FULL"); return; }
    for (k = 0; k < notes_clip.n; k++) {                 /* where each lands: the cursor plus its distance (wrapping at the loop's end) */
        r[k] = notes_clip.r[k];
        r[k].owner = (uint8_t)((r[k].owner & 0xE0u) | recording_owner(t));
        r[k].step = 0; r[k].on = 0;
        note_onset(&r[k], (int32_t)((ui.cursor % NSTEP) * RECORD_UNIT) + notes_clip.rel[k], len, 0);
        if (notes_manual_occupied(t, recording_view(t, &r[k]))) { ui_message("STEP OCCUPIED"); return; }
    }
    fm1_irq_off();
    if (!note_transaction_begin()) { fm1_irq_on(); return; }
    uint16_t meta[NSTEP];                                /* (only chance and slide survive an overview rebuild; as note_transform keeps them) */
    uint8_t was[NSTEP];
    memset(dest, 0, sizeof dest);
    for (k = 0; k < NSTEP; k++) { meta[k] = (uint16_t)(t->step[k].probability | ((uint16_t)t->step[k].flags << 8)); was[k] = (t->step[k].flags & SF_RECORDED) != 0; }
    for (k = 0; k < notes_clip.n; k++) {
        for (i = 0; i < RECORD_MAX && recording[i].vel; i++);
        if (i >= RECORD_MAX) break;
        recording_restore_note(t, i, r[k]);
        if (first == NSTEP || (r[k].step & 63u) < first) first = r[k].step & 63u;
    }
    note_rebuild_all();
    for (k = 0; k < NSTEP; k++)
        if (t->step[k].flags & SF_RECORDED) { t->step[k].probability = (uint8_t)meta[k]; t->step[k].flags = (uint8_t)((t->step[k].flags & ~SF_SLIDE) | ((meta[k] >> 8) & SF_SLIDE)); }
    for (k = 0; k < notes_clip.n; k++) {                 /* a step that was not recorded takes the copied notes' chance and slide */
        uint32_t vi = recording_view(t, &r[k]);
        step_t *v = &t->step[vi];
        if ((v->flags & SF_RECORDED) && !was[vi]) { v->probability = notes_clip.pr[k]; v->flags = (uint8_t)((v->flags & ~SF_SLIDE) | notes_clip.slide[k]); }
    }
    cursor_set((int32_t)first);
    ui.note_pick = 0;
    bulk.on = 0;
    note_transaction_end();
    fm1_irq_on();
    ui_message("NOTES PASTED");
}

static struct { uint8_t valid, chance, cond, rflam, accent, slide; int8_t nudge; } feel_clip;
static void feel_copy(void)
{
    const step_t *s = &TSEL->step[ui.cursor % NSTEP];
    if (!step_on(s)) { ui_message("NO NOTE THERE"); return; }
    feel_clip.chance = (uint8_t)step_chance(s); feel_clip.cond = s->cond; feel_clip.rflam = (uint8_t)step_rflam(s);
    feel_clip.nudge = s->nudge; feel_clip.accent = (s->flags & SF_ACCENT) != 0; feel_clip.slide = (s->flags & SF_SLIDE) != 0;
    feel_clip.valid = 1;
    ui_message("FEEL COPIED");
}
static void feel_paste(void)
{
    track_t *t = TSEL;
    uint32_t lo, hi, i, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), n = 0, recorded = 0;
    if (!feel_clip.valid) { ui_message("NOTHING COPIED"); return; }
    if (!feel_guard()) return;
    feel_range(&lo, &hi);
    if (hi >= len) hi = len - 1u;
    fm1_irq_off();
    for (i = lo; i <= hi; i++) {
        step_t *st = &t->step[i];
        if (!step_on(st)) continue;
        recorded |= st->flags & SF_RECORDED;
        step_set_chance(st, feel_clip.chance);
        step_set_cond(st, feel_clip.cond);
        step_set_rflam(st, feel_clip.rflam);
        step_set_nudge(st, feel_clip.nudge);
        st->flags = (uint8_t)((st->flags & ~(SF_ACCENT | SF_SLIDE)) | (feel_clip.accent ? SF_ACCENT : 0u) | (feel_clip.slide ? SF_SLIDE : 0u));
        n++;
    }
    if (recorded) recording_resched(t);
    fm1_irq_on();
    ui.force = 1;
    ui_message(n ? "FEEL PASTED" : "NO NOTE THERE");
}
#endif
