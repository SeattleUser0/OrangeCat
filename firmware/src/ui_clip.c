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
    memcpy(snd_clip.p, t->p, sizeof snd_clip.p);
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
