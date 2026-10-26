/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Copyright 2026 Jeremiah Conner (the changes for Orange Cat's engine table, sequencer and pages: Orange Cat 0.9.0; the drum kits: 0.14.2) */
/* DRUM-X, Jiant FM's drum engine (juanjiant-bit/jiant, firmware/src/eng_drum.c and drumx_voice.c, commit dc8d025, GPL-3.0-only,
 * after the idea of Sonic Charge's Microtonic): a kit of eight synthesized sounds, KICK, SNARE, CLAP, HAT CL, HAT OP, TOM,
 * RIM, BELL, each with two patches A and B and a MORPH between them. Nine factory kits (drumx_voice.c DX_KITS); the kit a
 * track plays is its preset (t->preset), so a user preset, a project and the PRESETS browser keep it as they keep any
 * engine's preset. Engine slot 9 (ENGI_DRUMX), the slot of the retired PHYS; off when MELODEE_LEGACY_EXTRAS keeps PHYS there.
 *
 * Keys and notes: the General MIDI drum map. The keys play it with the kick on the first C (drum_keys); a note 35..81 plays
 * its GM drum on its lane at its pitch from the lane's (DXE_GM: kicks, snares, toms, hats, cymbals and bells on BELL, congas
 * on TOM, claves on RIM); other notes as their octave of 36..47. The step grid (SEQ > STEP on a drum track: seq.c
 * drum_track now includes this engine) puts a lane's hit on its GM note (DRUM_LANE_NOTE), so the grid, the LANES page
 * and the Stage's lane flash are the DRUM engine's own.
 *
 * Parameters (EDIT 1 and 2, Jiant's names): MRPH the MORPH A..B of every lane; TUNE (+-12 semitones), TONE (the colour),
 * DECY (the decay) and NOIS (the noise) move every lane from its patch (64: as the kit has it); FM the FM struck with each
 * hit and gone with its pitch envelope (a click, a beater, a metal edge); FOLD every sound's oscillator through a
 * wavefolder after its envelope (0: as the kit has it); DRV a soft clip on the kit's sum, x1..x4, its level kept
 * (post: Jiant's drum_bus). Each lane has its LEVEL (P_LN0..P_LN7, the LANES pages, square law, 127 the default) on the
 * voice amplitude after the knee. The knobs move a hit while it rings.
 *
 * (0.14.2) Drum kits, after Skiff 2.0's drum lanes ("tune them, L/C/R and a volume on each piece"): each lane has its LEVEL (P_LN*), and
 * the track's kit (dxk[], dxk_rec.h) its TUNE (+-12 semitones), DECAY, PAN (the track's PAN law lane by lane, before the track's own) and
 * the sound values of Jiant's that are a lane's: LAYER (its second oscillator, with its own FDBK), PUNCH (Jiant's drum bus PUNCH, a lane's),
 * ATK+ (its hit faded in) and BEND (each hit gliding to another pitch over a step: Jiant's step BEND, a lane's); and the kit's own: TONE (BP
 * on the metal lanes, FDBK on every lane), CLAP's HANDS and DUCK (its kick ducks the parts that are not drum tracks: Jiant's G_DUCK, a
 * kit's). Every value 0, LAYER the factory kit's: the factory kit as it plays. A kit is saved to the library as a user preset of
 * engine 9 (upreset.c, the record's extension) and loaded back whole; the nine factory kits stay as they are (DX_KITS) and become
 * editable copies there. A project keeps naming the preset (t->preset, t->user), as before: SPEC.md "Drum kits".
 * Lanes panned: the voices still add into the part's mono block, and a panned one adds what the side away from it loses into side[]
 * (dxe_pan_out); dxe_post makes the kit's two sides of them, each through the bus, and the mixer (mix.c mix_lanes) takes the left from
 * the block and the right from side[1] (dxe_right) through DIST, the FX mute, solo, the track's level and PAN. SPREAD is off on such a
 * kit (its voices are lanes, each where its PAN puts it).
 *
 * Polyphony: a lane per drum and part, mono: a hit reuses its lane's voice even at another pitch, the 8 lanes ring together.
 * A sounding voice plays one lane (s[0]); the lane remembers its voice (owner), and a voice whose lane was struck again by
 * another voice, or has rung out (-84 dB), ends itself (dxe_amp). The voices come from the shared budget, up to 8 a part. A
 * closed hat chokes the open one.
 *
 * What applies (engine_t.oneshot): the hit is the envelope. VOICE (always POLY), GLIDE, the ADSR, ENV DEST, the global TUNE
 * and the matrix's PITCH CUT SHP do nothing; LEVEL, PAN, the sends, DIST, the SLICER, LFO DEST AMP, velocity, the matrix's
 * AMP and its E1..E8 (the knobs, per block) do. LFO DEST PIT tunes the kit and LFO DEST SHP moves MORPH (Jiant 0.6.81). A
 * released key does not end a hit. The step's accent (SF_ACCENT, a lane's acc bit) is the velocity 127, as for every engine;
 * its ratchet and flam (the sequencer's) are more hits in a step.
 *
 * Jiant's step fields on Orange Cat's step_t (core.h), field by field. Same field, same meaning: note[4], n, time, vel, hit (bit l:
 * lane l hits) and acc (bit l: accented, velocity 127), flags SF_ACCENT and SF_SLIDE, probability. RATCH (x1..x4, flags
 * bits 3..4) is the same bits. FLAM: Jiant has it as a lane mode (LM_FLAM); Orange Cat has it as the step's SF_FLAM (bit 5, Skiff's):
 * the whole step, not one lane. Orange Cat has two fields Jiant does not (cond: the trig condition; nudge: the step later in
 * 64ths); they work on a DRUM-X track as on any. NO EQUIVALENT, fixed default: lm[3], each lane's own way of playing on a
 * step (LM_NONE, X2, X4, X8, SHORT, GHOST, FLAM, ONCE): fixed LM_NONE, every lane plays what the step plays (the step's
 * RATCH and FLAM apply to all its lanes); comp, carg, comp2, carg2, cwhen, ctime, the step components (ARPN, ECHO, TRIG,
 * RAND, OCT, JUMP, REV, FILL, KILL, TOUCH and the rest, with their DIV and LEN): fixed none; ST_MICRO and SF_MFINE (the
 * sixteenth's 64ths): nudge does the timing. Orange Cat has no import of Jiant's patterns; these fields have no place in its pool.
 *
 * What of Jiant's DRUM-X is NOT here, with the fixed value it takes (BUILD.md, "DRUM-X as built"):
 *   - the sound editor (EDIT > SOUND, every lane's two patches A and B): a kit's sounds are a factory kit's (its base), read-only. A user
 *     kit's record has no room for the 88 bytes of patches (SPEC.md "Drum kits": a record of its own, later).
 *   - X-MOD (the kit's tempo-synced modulator of MORPH and its per-hit RAND): none (0.14.2 took the kit values the lanes have; X-MOD's
 *     four bytes fit the record's room to grow).
 *   - IMAGE (P_DXIMG) and each sound's SEND: no SEND, and IMAGE's place is taken by each lane's PAN (0.14.2). The track's sends take the
 *     kit's sum; a send per sound needs a send bus per lane, which the mixer does not have. The buses the SENDs fed (chorus, delay,
 *     reverb per sound) are Jiant's fx.c.
 *   - group mutes (Jiant's GLO layer): none. Lane mutes are in (0.13.1, below).
 *   - the octaves of the kit (OCT: ratchet above, SHORT / GHOST / FLAM below, TOUCH): OCT moves the keys by octaves, as for
 *     DRUM. The sequencer's ratchets and flams do the same job on a step; TOUCH has no equivalent in this tree's pads (they send a note and a
 *     velocity), and what it does in Jiant is in nothing this tree holds (its source was not at hand for 0.13.1), so it is not mapped onto SF_RATCH on a guess.
 *   - (0.14.2: the web editor's Sounds view has the kit, commands 128 and 129, web/EDITOR_PROTOCOL.md)
 *
 * Lane mutes (0.13.1), PERFORMANCE STATE: ACC held and a lane key on the grid mutes the lane (again: back on), the Stage shows MUTE beside the lane's name.
 * A muted lane's hits are not started (dxe_note_on: no voice, no flash), a ringing one is cut when the lane is struck. The mask (dxe_mute, four bytes of RAM)
 * is not stored: not in the project, the user presets or the backup; power-on and a project load clear it.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.13.1: lane mutes as performance state (not stored); the per-kit settings, IMAGE/SEND, group mutes and TOUCH looked at and left fixed, with the reasons above.
 *   0.14.2: drum kits (above): each lane's TUNE, DECAY, PAN, LAYER, PUNCH, ATK+, BEND, the kit's TONE, HANDS, DUCK; saved as a user preset.
 *   0.9.0: first version. The voice (drumx_voice.c) is Jiant's; this file is Jiant's eng_drum.c reduced to what Orange
 *          Cat's engine_t, sequencer and parameter store have a place for.
 */
#include "drumx_voice.c"
#include "dxk_rec.h"
_Static_assert(DXK_NLANE == DV_NLANE && DXK_NKIT == DX_NKIT, "dxk_rec.h: the lanes and the factory kits");

typedef struct {
    dx_voice_t x;                /* the hit */
    uint8_t owner;               /* the voice playing the lane: index + 1, 0 = none */
    int8_t st;                   /* its semitones from the lane's pitch (the GM map) */
    uint8_t age;                 /* (0.14.2) blocks since the hit (255: and more): PUNCH's shape, ATK+'s fade (Jiant's drum_lane_t) */
    int16_t bc, bd, bs;          /* (0.14.2) BEND: the hit's glide now, where to, a block's step (1/16 semitones; Jiant 0.6.82's) */
} dxe_lane_t;
typedef struct {                 /* the part's eight lanes: engines.c eng_state_size, the arena */
    dxe_lane_t l[DV_NLANE];
    uint8_t sides;               /* (0.14.2) this block: 1 a panned lane added to side[], 2 dxe_post made them the kit's two sides */
    int32_t side[2][CTL];        /* (0.14.2) what the left (0) and the right (1) lose of the panned lanes (dxe_pan_out); after
                                  * dxe_post: the kit's right side in side[1] (its left in the part's block) */
} dxe_part_t;
static dxe_part_t *dxe_part(uint32_t part);              /* engines.c */
static uint32_t seq_div_samples(uint32_t div);           /* mix.c (BEND: a step of the track) */

/* (Orange Cat 0.14.2) the tracks' kits (dxk_rec.h). on 0: the factory kit t->preset as it is, every value its own (the sound of
 * 0.9.0 to 0.14.1, sample for sample: a factory preset's load, a user preset of before 0.14.2, a new track). The first edit expands it
 * (dxk_take); a user kit's load sets it whole (dxk_put). In the pool: 61 bytes a track */
static dxk_t dxk[NTRK] __attribute__((section(".pool")));
static dxk_t *dxk_of(const track_t *t) { return &dxk[(uint32_t)(t - trk) % NTRK]; }
/* the factory kit k as a kit's values: its LAYERs, every other value 0 */
static void dxk_factory(dxk_t *K, uint32_t k)
{
    memset(K, 0, sizeof *K);
    K->base = (uint8_t)(k % DX_NKIT);
    memcpy(K->lay, DX_KITS[K->base].lay, sizeof K->lay);
}
/* track tr's kit as it plays, into *K (on 0: the factory kit its preset names, on 1 in *K) */
static void dxk_view(uint32_t tr, dxk_t *K)
{
    if (dxk[tr % NTRK].on)
        *K = dxk[tr % NTRK];
    else {
        dxk_factory(K, trk[tr % NTRK].preset);
        K->on = 1;
    }
}
/* track tr's kit as *k (a user kit's load, an undo, the editor's KIT_PUT; on 0: the factory kit again). The audio side reads on last:
 * while the values change it plays the factory kit t->preset */
static void dxk_put(uint32_t tr, const dxk_t *k)
{
    dxk_t *K = &dxk[tr % NTRK];
    K->on = 0;
    RING_PUBLISH();
    memcpy((uint8_t *)K + 1, (const uint8_t *)k + 1, sizeof *K - 1u);
    RING_PUBLISH();
    K->on = k->on ? 1u : 0u;
}
/* track tr's kit, to change a value of: expanded first (on 0: the factory kit, which is how it sounds) */
static dxk_t *dxk_take(uint32_t tr)
{
    if (!dxk[tr % NTRK].on) {
        dxk_t x;
        dxk_view(tr, &x);
        dxk_put(tr, &x);
    }
    return &dxk[tr % NTRK];
}
static void dxk_off(uint32_t tr) { dxk[tr % NTRK].on = 0; }   /* the factory kit t->preset (a preset's load) */
/* (0.14.2) the kit's values as the pages and the editor name them (params.c DXL, dxl_desc; editor.c KIT_GET / KIT_PUT): a lane's, then the kit's */
enum { DXL_LEVEL, DXL_TUNE, DXL_DECAY, DXL_PAN, DXL_LAYER, DXL_PUNCH, DXL_ATK, DXL_BEND, DXL_TONE, DXL_FDBK, DXL_HANDS, DXL_DUCK, DXL_N };
static int dxk_panned(const track_t *t)                   /* a lane off the centre: the kit has two sides (mix.c) */
{
    const dxk_t *K = dxk_of(t);
    uint32_t l, any = 0;
    if (!K->on)
        return 0;
    for (l = 0; l < DV_NLANE; l++)
        any |= (uint32_t)(uint8_t)K->pan[l];
    return any != 0;
}

/* (0.14.2) DUCK, Jiant's (fx.c duck_block) as a kit's value: a kick of a kit with DUCK takes the parts that are not drum tracks down
 * by up to 30 dB in ~2 ms, holds them ~25 ms and lets them back with Jiant's release (its default, ~210 ms), as a sidechain pump: the
 * gain follows 1 - (1 - env)^2. dxe_duck_block once a block after the events (mix.c mix_block); dxe_duck.g0 / g1 the gain at the
 * block's start and end (Q15), ramped over the part by mix.c mix_duck. A muted kick does not duck */
static struct {                                         /* (one struct a feature: ADDING_AN_ENGINE.md) */
    int32_t env, dep, g0, g1;                           /* the envelope, the depth (Q15), the gain at the block's start and end */
    uint8_t att, hold;
    volatile uint8_t req;                               /* the depth (1..100) a kick asked for since the last block, 0 none */
} dxe_duck = {0, 0, 32767, 32767, 0, 0, 0};
static void dxe_duck_block(void)
{
    int32_t e;
    if (dxe_duck.req) {
        dxe_duck.dep = (int32_t)dxe_duck.req * 31731 / 100;   /* (1 - 0.032: -30 dB at 100) */
        dxe_duck.req = 0;
        dxe_duck.att = 1;
        dxe_duck.hold = (uint8_t)(1103u / CTL);          /* (25 ms in blocks) */
    }
    dxe_duck.g0 = dxe_duck.g1;
    if (!dxe_duck.env && !dxe_duck.att) {
        dxe_duck.g1 = 32767;
        return;
    }
    if (dxe_duck.att) {
        dxe_duck.env += (32767 - dxe_duck.env) >> 1;
        if (dxe_duck.env > 31000)
            dxe_duck.att = 0;
    } else if (dxe_duck.hold) {
        dxe_duck.hold--;
    } else {
        dxe_duck.env = (int32_t)(((uint32_t)dxe_duck.env * dv_kb(208000u)) >> 16);   /* (Jiant's REL 30: 40 + 30 x 5.6 ms) */
    }
    e = (dxe_duck.env * (65536 - dxe_duck.env)) >> 15;  /* 1 - (1 - env)^2 */
    dxe_duck.g1 = 32767 - mulq15(dxe_duck.dep, e > 32767 ? 32767 : e);
}

/* (Orange Cat 0.13.1) lane mutes, PERFORMANCE STATE: bit l of dxe_mute[part] mutes lane l of the part. A muted lane's hits are not started (dxe_note_on), a
 * ringing one is cut. Not stored: a project, a user preset and a backup do not carry it, power-on and a project load clear it. The ACC key held with a lane key
 * toggles it (ui_input.c grid_keys). Four bytes of RAM. */
static volatile uint8_t dxe_mute[NTRK];
static uint32_t dxe_muted(uint32_t part, uint32_t lane) { return (uint32_t)(dxe_mute[part % NTRK] >> (lane & 7u)) & 1u; }
static uint32_t dxe_mute_toggle(uint32_t part, uint32_t lane)    /* the lane's new state */
{
    dxe_mute[part % NTRK] ^= (uint8_t)(1u << (lane & 7u));
    return dxe_muted(part, lane);
}
static void dxe_mute_clear(void) { for (uint32_t k = 0; k < NTRK; k++) dxe_mute[k] = 0; }
static uint32_t dxe_mute_all_on(uint32_t part)             /* (0.14.2) every lane of the part on (ACC + OCT+): how many were muted */
{
    uint32_t m = dxe_mute[part % NTRK], n = 0;
    for (; m; m &= m - 1u) n++;
    dxe_mute[part % NTRK] = 0;
    return n;
}

/* General MIDI notes 35..81 -> the lane (DV_*) and semitones from its pitch (Jiant's DRUM_GM) */
static const int8_t DXE_GM[47][2] = {
    {DV_KICK, -2}, {DV_KICK, 0}, {DV_RIM, 0}, {DV_SNARE, 0}, {DV_CLAP, 0}, {DV_SNARE, 2},             /* 35 */
    {DV_TOM, -7}, {DV_HATC, 0}, {DV_TOM, -4}, {DV_HATC, -2}, {DV_TOM, 0}, {DV_HATO, 0},               /* 41 */
    {DV_TOM, 3}, {DV_TOM, 5}, {DV_BELL, 0}, {DV_TOM, 8}, {DV_BELL, -3}, {DV_BELL, 2},                 /* 47 */
    {DV_BELL, 5}, {DV_HATC, 5}, {DV_BELL, 4}, {DV_BELL, 0}, {DV_BELL, 1}, {DV_RIM, -12},              /* 53 */
    {DV_BELL, -2}, {DV_TOM, 10}, {DV_TOM, 7}, {DV_TOM, 5}, {DV_TOM, 5}, {DV_TOM, 0},                  /* 59 */
    {DV_TOM, 10}, {DV_TOM, 7}, {DV_BELL, 7}, {DV_BELL, 3}, {DV_HATC, 3}, {DV_HATC, 7},                /* 65 */
    {DV_RIM, 7}, {DV_RIM, 5}, {DV_HATC, -4}, {DV_HATC, -6}, {DV_RIM, 0}, {DV_RIM, -4},                /* 71 */
    {DV_RIM, -7}, {DV_TOM, 12}, {DV_TOM, 8}, {DV_BELL, 12}, {DV_BELL, 12},                            /* 77 */
};
static uint32_t dxe_gm_ix(uint32_t note) { return (note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u) - 35u; }
static uint32_t dxe_lane(uint32_t note) { return (uint32_t)DXE_GM[dxe_gm_ix(note)][0]; }

/* the kit whose sounds the track plays: its preset (a factory kit each); (0.14.2) a kit's base when it has one */
static const dx_kit_t *dxe_kit(const track_t *t) { const dxk_t *K = dxk_of(t); return &DX_KITS[(K->on ? K->base : t->preset) % DX_NKIT]; }

/* the lane voice v plays, 0 when it plays none (any more) */
static dxe_lane_t *dxe_lane_of(track_t *t, const voice_t *v)
{
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    uint32_t i = (uint32_t)(v - t->v);
    dxe_lane_t *L;
    if (!s || i >= NVOICE)
        return 0;
    L = &s->l[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    return L->owner == i + 1u ? L : 0;
}

/* Different GM pitches on a lane still share its one sounding voice (voice.c voice_alloc) */
static DX_XIP voice_t *dxe_reuse(track_t *t, uint32_t note)
{
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    uint32_t lane = dxe_lane(note), owner = s ? s->l[lane].owner : 0;
    voice_t *v;
    if (!owner || owner > NVOICE)
        return 0;
    v = &t->v[owner - 1u];
    return v->active && (uint32_t)v->s[0] == lane ? v : 0;
}

static DX_XIP void dxe_note_on(track_t *t, voice_t *v)
{
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    dxe_lane_t *L;
    uint32_t i = (uint32_t)(v - t->v), lane = dxe_lane(v->note);
    if (dxe_muted((uint32_t)(t - trk), lane)) {          /* (0.13.1) a muted lane: no hit, nothing flashes, a ringing one is cut */
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return;
    }
    drum_flash[(uint32_t)(t - trk) % NTRK] |= (uint8_t)(1u << lane);   /* the Stage's lane names flash */
    if (!s || i >= NVOICE)
        return;
    L = &s->l[(uint32_t)v->s[0] & (DV_NLANE - 1u)];
    if (L->owner == i + 1u)                              /* this voice played another lane: it stops there */
        L->owner = 0;
    L = &s->l[lane];
    L->st = (int8_t)clamp(DXE_GM[dxe_gm_ix(v->note)][1], -60, 60);
    L->owner = (uint8_t)(i + 1u);                        /* (its last voice, if another, ends: dxe_amp) */
    {                                                    /* (0.14.2) the kit's lane values the hit starts with */
        const dxk_t *K = dxk_of(t);
        L->age = 0;
        L->bc = 0;
        L->bd = (int16_t)(K->on ? K->bend[lane] * 16 : 0);   /* BEND: there over a step of the track (Jiant: over the step) */
        if (L->bd) {
            uint32_t dbt = seq_div_samples((uint32_t)t->p[P_SDIV]) / CTL, d = (uint32_t)(L->bd < 0 ? -L->bd : L->bd);
            dbt = dbt ? dbt : 1u;
            L->bs = (int16_t)((d + dbt - 1u) / dbt);
        } else {
            L->bs = 0;
        }
        if (lane == DV_KICK && K->on && K->duck && K->duck > dxe_duck.req)   /* DUCK: the kick, the deepest of a block's */
            dxe_duck.req = K->duck;
    }
    v->s[0] = (int32_t)lane;
    v->env_out = v->vel * 258;                           /* the hit starts at its level (no ramp from 0) */
    v->env = 1 << 24;                                    /* the ADSR held at full from here, not from dxe_amp: a key-off
                                                          * before the first block (zero-length MIDI notes) would end a
                                                          * fresh voice at env 0 in env_tick */
    dx_trigger(&L->x);
    if (lane == DV_HATC)                                 /* a closed hat chokes the open one */
        dx_choke(&s->l[DV_HATO].x);
}

/* the voice's amplitude: the hit, not the ADSR (which still runs, held at full so a release never ends it). A voice that
 * plays no lane any more, or whose drum has rung out, ends here */
static DX_XIP int32_t dxe_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const dxe_lane_t *L = dxe_lane_of(t, v);
    if (!v->active)                                      /* (taken for another part: env_tick ended it) */
        return 0;
    if (v->stage == 4u)                                  /* given up: env_tick's fade, not held at full */
        return adsr;
    if (!L || (!L->x.live && !L->x.trig)) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    v->env = 1 << 24;
    return 32767;
}

/* (0.14.2) PUNCH, Jiant's drum bus PUNCH (p 0..100) by the hit's age: its first ~3 ms up to x1.45, its body (~15 .. 115 ms) up to
 * x1.7, back to unity by ~180 ms; driven into the knee (dxe_post), so the peak is held and the body denser. Q15 */
static int32_t dxe_punch_gain(uint32_t age, int32_t p)
{
    int32_t up = 32768 + p * 150, body = 32768 + p * 230;
    return age < 4u ? up : age < 20u ? up + (body - up) * (int32_t)(age - 4u) / 16
                       : age < 160u ? body : age < 250u ? body + (32768 - body) * (int32_t)(age - 160u) / 90 : 32768;
}
/* (0.14.2) .. its gain over the block, g0 to g1 (Q12) */
static DX_XIP void dxe_boost(int32_t *y, uint32_t n, int32_t g0, int32_t g1)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        y[i] = (y[i] * (g0 + (((g1 - g0) * (int32_t)i) >> CTL_LOG2))) >> 12;
}
/* (0.14.2) a panned lane's hit into the kit (out, as every lane's) and what the side away from it loses into sd: the track's PAN law
 * (mix.c) lane by lane, the far side down by |pan| / 64, the near side as it is. dxe_post turns them into the kit's two sides */
static DX_XIP void dxe_pan_out(const int32_t *y, int32_t *out, int32_t *sd, uint32_t n, const vmod_t *m, int32_t w)
{
    uint32_t i;
    for (i = 0; i < n; i++) {                            /* x1.25, as dxe_render */
        int32_t s = voice_amp(y[i] + (y[i] >> 2), m, i) * 2;
        out[i] += s;
        sd[i] += (s * w) >> 6;                           /* (|s| < 2^18, w <= 64) */
    }
}

static DX_XIP void dxe_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    dxe_lane_t *L = dxe_lane_of(t, v);
    uint32_t ln = (uint32_t)v->s[0] & (DV_NLANE - 1u), i;
    int32_t y[CTL], lv = clamp(p[P_LN0 + ln], 0, 127), lfo, lfo_pit, lfo_shp;
    vmod_t ml;                                           /* (only its amplitude ramp is read: voice_amp) */
    const dxk_t *K = dxk_of(t);
    uint32_t lay = K->on ? K->lay[ln] : dxe_kit(t)->lay[ln], tone = K->on ? K->tone : 0u, hands = K->on ? K->hands : 0u;
    int32_t ltune = 0, ldec = 0, pan = 0, pb0 = 4096, pb1 = 4096;
    if (!L)
        return;
    if (K->on) {                                         /* (0.14.2) the lane's own values (dxk_rec.h) */
        ltune = K->tune[ln] * 3;
        ldec = K->decay[ln];
        pan = K->pan[ln];
    }
    if (ln == DV_BELL)                                   /* (Jiant 0.6.49) the cowbell 4 dB under the others: it stood out of
                                                          * every kit (its LEVEL at 100 % is this) */
        lv = lv * 100 / 127;
    if (lv < 127) {                                      /* the lane's LEVEL (square law) on the block's amplitude ramp, not
                                                          * per sample; 127: the ramp as it was */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);       /* Q15 */
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    if (n > CTL)
        n = CTL;
    lfo = mulq15(t->lfo_val, t->lfo_fade);              /* (Jiant 0.6.81) the part's LFO on the kit: PITCH tunes it, SHAPE moves MORPH */
    lfo_pit = (lfo * p[P_LD_PIT] * 3) >> 15;
    lfo_shp = (lfo * p[P_LD_SHP]) >> 15;
    if (L->bc != L->bd) {                                /* (0.14.2) BEND: towards its pitch over the step (Jiant 0.6.82) */
        int32_t d = L->bd - L->bc;
        L->bc = (int16_t)(L->bc + (d > 0 ? (d < L->bs ? d : L->bs) : (-d < L->bs ? d : -L->bs)));
    }
    L->x.layer = (uint8_t)lay;                           /* (0.14.2) the lane's LAYER, the kit's TONE and HANDS: the hit's */
    L->x.tone = (uint8_t)(DX_TONE_METAL(ln) ? tone : tone & 0xF0u);   /* (BP on the metal lanes, FDBK on all) */
    L->x.hands = (uint8_t)hands;
    dx_run(dxe_kit(t), ln, &L->x, p[P_E0] + lfo_shp, L->st * 16 + (p[P_E1] - 64) * 3 + lfo_pit + ltune + L->bc,
           p[P_E3] - 64 + ldec, p[P_E2] - 64, p[P_E4] - 64, p[P_E6], p[P_E5], y, n);
    if (K->on && (K->punch[ln] || K->atk[ln])) {         /* (0.14.2) PUNCH and ATK+, the lane's (Jiant's, eng_drum.c drum_render) */
        if (K->punch[ln]) {
            int32_t pu = K->punch[ln] * 20 / 3;          /* (15: Jiant's 100) */
            pb0 = dxe_punch_gain(L->age, pu) >> 3;       /* (Q12; >= 1: the boost, never a cut) */
            pb1 = dxe_punch_gain(L->age + 1u, pu) >> 3;
        }
        if (K->atk[ln] && L->age < K->atk[ln] * 6u) {   /* ATK+: the hit faded in, atk x 6 blocks (8: Jiant's 48, ~35 ms) */
            int32_t na = (int32_t)K->atk[ln] * 6;
            ml.amp0 = m->amp0 * (int32_t)L->age / na;
            ml.amp1 = m->amp1 * (int32_t)(L->age + 1u) / na;
            m = &ml;
        }
    }
    if (L->age < 255u)
        L->age++;
    if (pb0 != 4096 || pb1 != 4096)
        dxe_boost(y, n, pb0, pb1);
    if (pan) {                                           /* (0.14.2) a lane off the centre: its share of the sides */
        dxe_part_t *s = dxe_part((uint32_t)(t - trk));
        if (s) {
            if (!s->sides) {
                memset(s->side, 0, sizeof s->side);
                s->sides = 1;
            }
            dxe_pan_out(y, out, s->side[pan < 0], n, m, pan < 0 ? -pan : pan);
            return;
        }
    }
    for (i = 0; i < n; i++)                              /* x1.25 */
        out[i] += voice_amp(y[i] + (y[i] >> 2), m, i) * 2;
}

/* (Jiant 0.6.16, drum_bus) the kit's sum, once a block (voice.c track_render, after the part's voices): DRV x1..x4 into the
 * soft clip, its level kept, and the knee (were each hit's: eight lanes, two curves a sample each; now the bus's, a glue) */
/* (0.14.2) the bus (as dxe_post's loop) on one side of a kit with lanes panned */
static DX_XIP void dxe_bus(int32_t *b, uint32_t n, int32_t g, int32_t mk)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t s = b[i] >> 1, y = s;
        if (g)
            y = (softclip((clamp(s, -131071, 131071) * g) >> 12) * mk) >> 15;
        y = soft_knee(y, 24000);
        b[i] = y * 2;
    }
}
/* (0.14.2) lanes panned this block: the kit's left (out) and right (side[1]), the kit less what each side loses, each through the bus. 1: done */
static DX_XIP int dxe_post_sides(track_t *t, int32_t *out, uint32_t n, int32_t g, int32_t mk)
{
    dxe_part_t *sp = dxe_part((uint32_t)(t - trk));
    uint32_t i;
    if (!sp || sp->sides != 1u)
        return 0;
    for (i = 0; i < n; i++) {
        int32_t k = out[i];
        out[i] = k - sp->side[0][i];
        sp->side[1][i] = k - sp->side[1][i];
    }
    dxe_bus(out, n, g, mk);
    dxe_bus(sp->side[1], n, g, mk);
    sp->sides = 2;
    return 1;
}
static DX_XIP void dxe_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    int32_t drv = t->p[P_E7], g = 0, mk = 0;
    uint32_t i;
    if (!nr)
        return;
    if (drv > 0) {
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
    }
    if (dxe_post_sides(t, out, n, g, mk))               /* (0.14.2) a kit with lanes panned: its two sides */
        return;
    for (i = 0; i < n; i++) {
        int32_t s = out[i] >> 1, y = s;
        if (g)                                           /* (s held to +-131071: s * g fits; several loud hits at once with DRV
                                                          * high wrapped it: a full-scale click) */
            y = (softclip((clamp(s, -131071, 131071) * g) >> 12) * mk) >> 15;
        y = soft_knee(y, 24000);
        out[i] = y * 2;
    }
}

static DX_XIP uint32_t dxe_units(const track_t *t) { (void)t; return 2u; }   /* as DRUM, FM6, SID: two budget units a voice */

/* (0.14.2) once a block before the voices (voice.c track_render_audio): no side this block until a panned lane renders */
static DX_XIP void dxe_block(track_t *t)
{
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    if (s)
        s->sides = 0;
}
/* (0.14.2) the kit's right side this block, when its lanes are panned (its left is the part's block): mix.c; 0 one side (mono) */
static int32_t *dxe_right(const track_t *t)
{
    dxe_part_t *s;
    if (!(MELODEE_DRUMX && t->engine == ENGI_DRUMX) || !(s = dxe_part((uint32_t)(t - trk))))
        return 0;
    return s->sides == 2u ? s->side[1] : 0;
}

/* the GM drum map, the first C (key 7) is the kick (C2, 36); OCT moves it by octaves, as for DRUM */
static DX_XIP int32_t dxe_keys(const track_t *t, uint32_t k)
{
    return clamp(29 + 12 * input_octave(t) + (int32_t)k, 0, 127);
}

/* {MRPH, TUNE, TONE, DECY, NOIS, FM, FOLD, DRV}: Jiant's DRUM_PRESETS, one a factory kit (DX_KITS, this order) */
static const preset_t DXE_PRESETS[] = {
    {"DRUM-X", {64, 64, 70, 64, 64, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"909 CLUB", {60, 64, 76, 64, 64, 0, 0, 10}, {0, 100, 127, 100}, 0, 0},
    {"808 BOOM", {58, 60, 56, 76, 60, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"LOFI DUST", {70, 64, 50, 54, 76, 0, 0, 20}, {0, 100, 127, 100}, 0, 0},
    {"INDUSTRIAL", {72, 66, 88, 70, 74, 20, 0, 40}, {0, 100, 127, 100}, 0, 0},
    {"ACOUSTIC", {56, 64, 58, 62, 64, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"DUB ROOTS", {60, 60, 52, 70, 60, 0, 0, 0}, {0, 100, 127, 100}, 0, 0},
    {"TECHNO RAW", {66, 64, 80, 62, 66, 10, 0, 30}, {0, 100, 127, 100}, 0, 0},
    {"BREAKS", {62, 66, 72, 62, 70, 0, 0, 10}, {0, 100, 127, 100}, 0, 0},
};
_Static_assert(sizeof DXE_PRESETS / sizeof DXE_PRESETS[0] == sizeof DX_KITS / sizeof DX_KITS[0], "a DRUM-X preset a kit");

static const engine_t ENG_DRUMX = {
    .name = "DRUM-X",
    .page_title = {"KIT", "KIT 2"},
    .edit = {
        {"MRPH", F_PCT, 0, 127, 64, 0, 0},
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"NOIS", F_PCT, 0, 127, 64, 0, 0},
        {"FM", F_PCT, 0, 127, 0, 0, 0},                 /* Jiant: FM ENV, the FM struck with the hit */
        {"FOLD", F_PCT, 0, 127, 0, 0, 0},               /* Jiant 0.5: a wavefolder */
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DXE_PRESETS,
    .npresets = NELEM(DXE_PRESETS),
    .note_on = dxe_note_on,
    .render = dxe_render,
    .amp = dxe_amp,
    .post = dxe_post,
    .units = dxe_units,
    .block = dxe_block,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = DV_NLANE,
    .oneshot = 1,
    .keys = dxe_keys,
};
