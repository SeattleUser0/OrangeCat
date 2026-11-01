/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat, Release 3 (branch r3-format), Part 7: the store and recall of SCENES VERSION 2 (oc_scenes2.c holds the data and the record's bytes). Behind
 * OC_R3_SCENES. Included by project.c after oc_lists.c (it uses ocl_go_at, ocl_launch, ocl_mute).
 *
 * A recall applies the parts its mask names: tempo (not under an external clock: the scene says so on its sheet), mixer (level, pan, mute), effects (the globals
 * and each track's sends), chains and mutes (the path of today's scene: stopped at once, playing staged for the launch point so that with BAR they land together),
 * and sounds. Tempo, mixer and effects are RAM writes the audio reads at its next block, like a knob turned. A track whose phrase cannot be loaded does not stop
 * the others: the recall goes on and returns the first failure (SCENE PARTIAL names the track). A track whose scene sound names an engine that is not installed keeps
 * the sound it has (T2 NEEDS SHAPES); its other parts land.
 *
 * Sounds, until the audio part is merged (R3-AUDIO-INTERRUPT): STOPPED only. scn2_apply_sound loads the sound as a preset change does today (the engine request, the
 * preset, the values but the strip's: level, pan, sends, mute), at once. While playing a track's sound part is skipped (*skipped has a bit a track; the caller says
 * SCENE SOUND: STOP FIRST), because a same-engine change has no fade and would cut held notes, and the old phrase would play on under the new sound. Tempo, mixer and
 * effects are applied at the call even while playing (the document lands them on the boundary: a stated deviation until the audio part). The boundary field is
 * stored and ignored (seq_launch_mode reads the project's launch).
 * Revisions:
 *   Part 7 (November 1, 2026): first version. */
#if OC_R3_SCENES
static int scn2_used(uint32_t s) { return (scn2.used >> (s % SCN2_N)) & 1u; }
static uint8_t scn2_val8(int32_t v) { return (uint8_t)clamp(v + 64, 0, 255); }
/* the part values of the live state into scene s, for the parts the mask names; the sound table gets a new entry (or reuses an equal one) */
static uint32_t scn2_sound_index(uint32_t k)
{
    scn2_snd_t cand;
    memset(&cand, 0, sizeof cand);
    cand.engine = trk[k].eng_req; cand.preset = trk[k].preset; cand.np = P_COUNT; cand.used = 1;
    for (uint32_t i = 0; i < P_COUNT; i++) cand.p[i] = trk[k].p[i];
    for (uint32_t i = 0; i < scn2.nsnd; i++)
        if (!memcmp(&scn2.snd[i], &cand, sizeof cand)) return i + 1u;
    if (scn2.nsnd >= SCN2_MAXSND) return 0;                    /* (full: the track keeps no sound in this scene) */
    scn2.snd[scn2.nsnd] = cand;
    return ++scn2.nsnd;
}
static void scn2_store(uint32_t s, uint32_t mask)
{
    scn2_t *e;
    s %= SCN2_N;
    e = &scn2.s[s];
    memset(e, 0, sizeof *e);
    e->mask = (uint8_t)(mask & SCM_ALL);
    if (e->mask & SCM_TEMPO) { e->bpm = song.g[G_BPM]; e->swing = song.g[G_SWING]; }
    for (uint32_t k = 0; k < NTRK; k++) {
        if (e->mask & SCM_SOUND) e->snd[k] = (uint8_t)(k < trk_n() ? scn2_sound_index(k) : 0u);
        if (e->mask & SCM_MIXER) { e->lvl[k] = scn2_val8(trk[k].p[P_LEVEL]); e->pan[k] = scn2_val8(trk[k].p[P_PAN]); e->mute[k] = trk[k].mute_arm ? (uint8_t)(trk[k].mute_arm - 1u) : trk[k].p[P_MUTE] != 0; }
        if (e->mask & SCM_FX) { e->send[k][0] = scn2_val8(trk[k].p[P_REV]); e->send[k][1] = scn2_val8(trk[k].p[P_DLY]); e->send[k][2] = scn2_val8(trk[k].p[P_CHOR]); }
        if (e->mask & (SCM_CHAIN | SCM_MUTE)) {
            uint32_t en = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_ent(k) : lists.cur[k] % OCL_NENT;
            uint32_t c = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_chain[k] : lists.tchain[k];
            uint32_t m = trk[k].mute_arm ? trk[k].mute_arm - 1u : trk[k].p[P_MUTE] != 0;
            e->chain[k] = (uint8_t)(k < trk_n() ? c % OCL_NCHAIN + 1u : 0u);
            e->stepmute[k] = (uint8_t)(en | (m ? 0x80u : 0u));
        }
    }
    if (e->mask & SCM_FX) for (uint32_t i = 0; i < PROJ_JFX_N; i++) e->fx[i] = scn2_val8(song.g[G_DTIME + i]);
    scn2.used |= (uint16_t)(1u << s);
    scn2_sync_to_lists(&scn2, &lists);
}
/* a sound applied as a preset change is today (see the head of this file) */
static int scn2_apply_sound(uint32_t k, const scn2_snd_t *d)
{
    if (!eng_ok(d->engine)) return 1;                           /* (the engine is not installed: the track keeps its sound) */
    trk[k].eng_req = d->engine; trk[k].preset = d->preset;
    for (uint32_t i = 0; i < d->np && i < P_COUNT; i++) {
        if (i == P_LEVEL || i == P_PAN || i == P_MUTE || i == P_REV || i == P_DLY || i == P_CHOR) continue;   /* (the strip is the mixer part's) */
        trk[k].p[i] = d->p[i];
    }
    return 0;
}
/* scene s recalled. 0 done; 1 an empty scene; else the first failure's (ocl_go_at: 4 no bank, 5 no room). *missing: the first track whose sound's engine is
 * not installed + 1 (0 none): the caller says T2 NEEDS SHAPES */
static int scn2_recall(uint32_t s, uint32_t *missing, uint32_t *skipped)
{
    int rc = 0, r;
    scn2_t *e;
    s %= SCN2_N;
    if (missing) *missing = 0;
    if (skipped) *skipped = 0;
    if (!scn2_used(s)) return 1;
    e = &scn2.s[s];
    if (e->mask & SCM_TEMPO && song.g[G_CLOCK] == 0) { song.g[G_BPM] = e->bpm; song.g[G_SWING] = e->swing; }
    if (lists.mode == OCL_SONG && (e->mask & (SCM_CHAIN | SCM_MUTE))) lists.mode = OCL_CHAIN;
    for (uint32_t k = 0; k < trk_n(); k++) {
        if (e->mask & SCM_MIXER) {
            trk[k].p[P_LEVEL] = (int16_t)((int32_t)e->lvl[k] - 64); trk[k].p[P_PAN] = (int16_t)((int32_t)e->pan[k] - 64);
            if ((uint32_t)(trk[k].p[P_MUTE] != 0) != e->mute[k] || trk[k].mute_arm) {
                if (!song.playing) { trk[k].p[P_MUTE] = e->mute[k]; trk[k].mute_arm = 0; } else trk[k].mute_arm = (uint8_t)(e->mute[k] + 1u);
            }
        }
        if (e->mask & SCM_FX) { trk[k].p[P_REV] = (int16_t)((int32_t)e->send[k][0] - 64); trk[k].p[P_DLY] = (int16_t)((int32_t)e->send[k][1] - 64); trk[k].p[P_CHOR] = (int16_t)((int32_t)e->send[k][2] - 64); }
        if ((e->mask & SCM_SOUND) && e->snd[k] && e->snd[k] <= scn2.nsnd && song.playing) { if (skipped) *skipped |= 1u << k; }   /* (interim: a sound is loaded stopped only) */
        else if ((e->mask & SCM_SOUND) && e->snd[k] && e->snd[k] <= scn2.nsnd) {
            if (scn2_apply_sound(k, &scn2.snd[e->snd[k] - 1u]) && missing && !*missing) *missing = k + 1u;
        }
        if (e->mask & SCM_CHAIN) {
            uint32_t c = e->chain[k], en = e->stepmute[k] & (OCL_NENT - 1u);
            if (c) {
                c--;
                if (c != lists.tchain[k] || en != lists.cur[k] % OCL_NENT || ocl_staged_ent(k) < OCL_NENT || bank_pid[k][trk[k].pattern] != lists.chain[c][en][0]) {
                    if (!song.playing && c == lists.tchain[k] && en == lists.cur[k] % OCL_NENT) r = 0;
                    else if (c == lists.tchain[k]) r = ocl_launch(k, en);
                    else r = ocl_go_at(k, c, en, OCL_NOROW, 0, 0);
                    if (r && !rc) rc = r;
                }
            }
        }
        if ((e->mask & SCM_MUTE) && !(e->mask & SCM_MIXER) && e->chain[k]) {
            uint32_t m = e->stepmute[k] >> 7;
            if ((uint32_t)(trk[k].p[P_MUTE] != 0) != m || trk[k].mute_arm) {
                if (!song.playing) { trk[k].p[P_MUTE] = (int16_t)m; trk[k].mute_arm = 0; } else trk[k].mute_arm = (uint8_t)(m + 1u);
            }
        }
    }
    if (e->mask & SCM_FX) for (uint32_t i = 0; i < PROJ_JFX_N; i++) song.g[G_DTIME + i] = (int16_t)((int32_t)e->fx[i] - 64);
    return rc;
}
#endif
