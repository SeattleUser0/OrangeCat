/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat, the new interface (OC_HOME_PIC; docs/orangecat/UI_BUILD.md step 1a): HOME's knob macros applied in the audio
 * interrupt, once a block, exactly as PERFORM's throws are (oc_throw.c): mac_on() puts each turned macro's targets on over
 * their stored values after the block's events (so motion's base never takes a macro in) and before the mix copies the
 * parameters for CPU1 (mix.c mix_prepare_batch); mac_off() takes them off once the block is mixed, in reverse order, and
 * only where the value is still the one put (a value written meanwhile stays). The main loop never runs inside the
 * interrupt, so everything it shows, saves and edits is the stored value; the engines read their parameters as they always
 * did and need no code (UI_DESIGN.md section 2, "data macros").
 *
 * The call site (mix.c is Session A's and is not touched here): mix_block calls throw_on() only while thr_out has a bit
 * set and throw_off() only while thr_on has. The macros arm with bit 31 of both (MAC_ARM; the throws use bits 0 .. 11).
 * mac_poll(), once a main-loop pass (ui_input.c), sets it while any track in use has a table and a macro off center.
 * FOR SESSION A'S REVIEW AND TIMING BEFORE ANY MERGE INTO RELEASE-2 (UI_BUILD.md "Waiting on Session A"): a direct call
 * from mix_block would replace the bit.
 *
 * Saves until the Release 3 format revision (UI_BUILD.md 1a): the stores zero P_RESERVED33 .. 36 on every load (old
 * projects hold inert effect values there), so a save COMMITS the macros: mac_saved_value() gives each target as it sounds
 * and the macro itself as 0. A saved sound reloads sounding as it was saved, its macros at the center.
 *
 * Included by oc_throw.c, after oc_knobs.c.
 *
 * Revisions:
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1a) first version; after the design review: a target motion plays is
 *   left alone (live and saved), an engine crossfade (xf_on) puts nothing on, mac_desc only for a macro knob.
 */
#define MAC_ARM 0x80000000u                             /* thr_out / thr_on: the macros armed / put on this block */
#define MAC_N (4u * KD_TGT)                             /* a track's targets: knob k's target j is k x KD_TGT + j */
_Static_assert(P_COUNT < KD_NOT, "a target id fits a byte below KD_NOT");
static struct {
    int16_t keep[MAC_N];                                /* this block: the stored values under what was put */
    uint16_t on;                                        /* .. the targets put on, a bit each */
    int8_t m[4];                                        /* .. each knob's offset as it was put (mac_off recomputes the put) */
    uint8_t eng;                                        /* .. the engine whose table it was */
} mac[NTRK];
static int16_t motion_base_value(const track_t *t, uint32_t id);   /* (motion.c) */
static uint32_t motion_active[NTRK][(P_COUNT + 31u) / 32u];       /* (motion.c's: a C tentative definition, the same object; motion.c
                                                                    * comes after mix.c in the unity build) */
/* 1: parameter id of track k plays a motion lane now. Such a target is left alone by the macros, live and in a save: motion
 * writes it absolutely every step and its saved events carry no offset, so a macro over it would sound one way now and
 * another way after a reload (the design review, UI_BUILD.md 1a) */
static int mac_motion(uint32_t k, uint32_t id) { return (motion_active[k % NTRK][id / 32u] >> (id % 32u)) & 1u; }

/* the move of a target of shape `shape` and depth `depth` at offset m (C division: toward 0) */
static int32_t mac_delta(uint32_t shape, int32_t depth, int32_t m)
{
    int32_t x = shape == KS_UP ? (m > 0 ? m : 0) : shape == KS_DOWN ? (m < 0 ? m : 0) : m;
    return depth * x / 64;
}
/* knob i / KD_TGT's target i % KD_TGT of table kd over `from` at offset m, in its range (engine e's descriptors) */
static int32_t mac_value(const knob_def_t *kd, uint32_t i, int32_t from, int32_t m, uint32_t e)
{
    const kd_tgt_t *g = &kd[i / KD_TGT].t[i % KD_TGT];
    const param_desc_t *d = param_desc_of(e, g->id);
    return clamp(from + mac_delta(g->shape, g->depth, m), d->min, d->max);
}
/* knob k's offset on track t, -64 .. 63 (anything else stored there counts as its nearest end) */
static int32_t mac_offset(const track_t *t, uint32_t k) { return clamp(t->p[P_MAC0 + k], -64, 63); }

/* (the interrupt, end of throw_on) every track's turned macros on; 1 when anything was put on */
static __attribute__((noinline)) uint32_t mac_on(void)
{
    uint32_t k, n = trk_n(), any = 0;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const knob_def_t *kd;
        uint32_t i, on = 0;
        mac[k].on = 0;
        if (k >= n || t->engine != t->eng_req || t->xf_on || !(kd = kdef_of(t->engine)))   /* (an engine fade under way: its
                                                         * values belong to two engines; nothing is put on until it ends) */
            continue;
        for (i = 0; i < 4u; i++)
            mac[k].m[i] = (int8_t)(kd[i].kind == KD_MACRO ? mac_offset(t, i) : 0);
        for (i = 0; i < MAC_N; i++) {
            uint32_t id = kd[i / KD_TGT].t[i % KD_TGT].id;
            int32_t m = mac[k].m[i / KD_TGT];
            if (!m || id == KD_NOT || mac_motion(k, id))
                continue;
            mac[k].keep[i] = t->p[id];
            t->p[id] = (int16_t)mac_value(kd, i, mac[k].keep[i], m, t->engine);
            on |= 1u << i;
        }
        mac[k].on = (uint16_t)on;
        mac[k].eng = t->engine;
        any |= on;
    }
    return any != 0;
}
/* (the interrupt, start of throw_off) every target put on taken off, the last put first (two macros on one target come off
 * exactly); a value something else wrote during the block stays */
static __attribute__((noinline)) void mac_off(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        const knob_def_t *kd = kdef_of(mac[k].eng);
        uint32_t i, on = mac[k].on;
        mac[k].on = 0;
        if (!on || !kd)
            continue;
        for (i = MAC_N; i-- > 0;) {
            uint32_t id = kd[i / KD_TGT].t[i % KD_TGT].id;
            if (((on >> i) & 1u) && t->p[id] == mac_value(kd, i, mac[k].keep[i], mac[k].m[i / KD_TGT], mac[k].eng))
                t->p[id] = mac[k].keep[i];
        }
    }
}
/* (the main loop, once a pass: ui_input.c) MAC_ARM set while any track in use has a table and a macro off center */
static void mac_poll(void)
{
    uint32_t k, want = 0, f;
    for (k = 0; k < trk_n() && !want; k++) {
        const track_t *t = &trk[k];
        const knob_def_t *kd = kdef_of(t->eng_req);
        for (uint32_t j = 0; kd && j < 4u; j++)
            if (kd[j].kind == KD_MACRO && mac_offset(t, j))
                want = 1;
    }
    if (!!(thr_out & MAC_ARM) == want)
        return;
    f = motion_guard();                                 /* (the interrupt not in between) */
    if (want) thr_out |= MAC_ARM;
    else thr_out &= ~MAC_ARM;
    motion_unguard(f);
}
/* (the saves: project.c, upreset.c) parameter id of track t as a save keeps it: the patch's value (motion's base) with the
 * track's macros committed into it, in the order the interrupt puts them on; the macros themselves as 0 */
static int16_t mac_saved_value(const track_t *t, uint32_t id)
{
    const knob_def_t *kd = kdef_of(t->eng_req);
    int32_t v = motion_base_value(t, id);
    uint32_t i;
    if (id >= P_MAC0 && id <= P_MAC3)
        return 0;
    for (i = 0; kd && i < MAC_N; i++) {
        int32_t m = kd[i / KD_TGT].kind == KD_MACRO ? mac_offset(t, i / KD_TGT) : 0;
        if (m && kd[i / KD_TGT].t[i % KD_TGT].id == id && !mac_motion((uint32_t)(t - trk), id))   /* ((t - trk): seq.c's
                                                         * trk_index, which comes later in the unity build) */
            v = mac_value(kd, i, v, m, t->eng_req);
    }
    return (int16_t)v;
}
/* (params.c track_desc) P_MACk's descriptor on HOME when knob k of the track's table is a macro: the knob's name, -64 .. 63,
 * 0 the sound as stored; else 0 and the caller keeps TP[]'s (an engine without a table, or one that uses the id for
 * something else: the Prophet test engine's HOME knobs, eng_prophet_test.c). One descriptor a knob, so two can be held */
static const param_desc_t *mac_desc(const track_t *t, uint32_t id)
{
    static param_desc_t d[4];
    const knob_def_t *kd = kdef_of(t->eng_req);
    uint32_t k = (id - P_MAC0) & 3u;
    if (!kd || kd[k].kind != KD_MACRO)
        return 0;
    d[k] = (param_desc_t){kd[k].label, F_INT, -64, 63, 0, 0, 0};
    return &d[k];
}
