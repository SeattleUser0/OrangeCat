/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Copyright 2026 Jeremiah Conner (the changes for Orange Cat's engine table, sequencer and pages: Orange Cat 0.9.0) */
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
 *   - the kit editor (EDIT > SOUND, every lane's two patches): the kit is a factory kit, read-only. Jiant keeps the edited
 *     kit in its project; Orange Cat has no place in its project format for it (a format change, left for a later version).
 *   - Every per-kit setting Jiant has beyond the eight knobs stays at the kit's own value (0.13.1 looked for a place for them): the preset
 *     (core.h preset_t) holds eight engine values, P_E0..P_E7, and all eight are DRUM-X's knobs (MRPH TUNE TONE DECY NOIS FM FOLD DRV); a ninth value
 *     is a change of the preset, user-preset, project and backup formats, which this version does not make. So: LAYER (each sound's second
 *     oscillator and FEEDBK) is the kit's own value (DX_KITS lay), TONE's BP and FEEDBK and CLAP's HANDS are 0 (dx_tone, dx_hands, drumx_voice.c),
 *     PUNCH, the DUCK of the other parts on a kick, ATK+ (a hit faded in), the step's BEND and X-MOD (the kit's tempo-synced modulator and per-hit
 *     RAND) are none. Making any of them a constant of a factory kit would change no sound until a kit sets it, and the reference renders
 *     (tests/drumx_ref.txt) are Jiant's with them at these values.
 *   - IMAGE (P_DXIMG) and each sound's SEND: IMAGE 0 and no SEND. Orange Cat's part is one mono sum through its chain (engine_t.render adds to a
 *     mono block), and its mixer has the sends and the pan of the TRACK; a send or a position per sound needs a stereo, per-lane output of the engine, which
 *     the engine interface does not have. The track's own sends and pan are there and work on the whole kit. The buses the SENDs fed (chorus,
 *     delay, reverb per sound) are Jiant's fx.c.
 *   - group mutes (Jiant's GLO layer): none. Lane mutes are in (0.13.1, below).
 *   - the octaves of the kit (OCT: ratchet above, SHORT / GHOST / FLAM below, TOUCH): OCT moves the keys by octaves, as for
 *     DRUM. The sequencer's ratchets and flams do the same job on a step; TOUCH has no equivalent in this tree's pads (they send a note and a
 *     velocity), and what it does in Jiant is in nothing this tree holds (its source was not at hand for 0.13.1), so it is not mapped onto SF_RATCH on a guess.
 *   - the web editor's kit pages: the engine's EDIT pages (the eight values), the lane levels and the presets are there; no page was added because
 *     no value was.
 *
 * Lane mutes (0.13.1), PERFORMANCE STATE: ACC held and a lane key on the grid mutes the lane (again: back on), the Stage shows MUTE beside the lane's name.
 * A muted lane's hits are not started (dxe_note_on: no voice, no flash), a ringing one is cut when the lane is struck. The mask (dxe_mute, four bytes of RAM)
 * is not stored: not in the project, the user presets or the backup; power-on and a project load clear it.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.13.1: lane mutes as performance state (not stored); the per-kit settings, IMAGE/SEND, group mutes and TOUCH looked at and left fixed, with the reasons above.
 *   0.9.0: first version. The voice (drumx_voice.c) is Jiant's; this file is Jiant's eng_drum.c reduced to what Orange
 *          Cat's engine_t, sequencer and parameter store have a place for.
 */
#include "drumx_voice.c"

typedef struct {
    dx_voice_t x;                /* the hit */
    uint8_t owner;               /* the voice playing the lane: index + 1, 0 = none */
    int8_t st;                   /* its semitones from the lane's pitch (the GM map) */
} dxe_lane_t;
typedef struct { dxe_lane_t l[DV_NLANE]; } dxe_part_t;   /* the part's eight lanes: engines.c eng_state_size, the arena */
static dxe_part_t *dxe_part(uint32_t part);              /* engines.c */

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

/* the kit the track plays: its preset (a factory kit each) */
static const dx_kit_t *dxe_kit(const track_t *t) { return &DX_KITS[t->preset % DX_NKIT]; }

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

static DX_XIP void dxe_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    dxe_lane_t *L = dxe_lane_of(t, v);
    uint32_t ln = (uint32_t)v->s[0] & (DV_NLANE - 1u), i;
    int32_t y[CTL], lv = clamp(p[P_LN0 + ln], 0, 127), lfo, lfo_pit, lfo_shp;
    vmod_t ml;                                           /* (only its amplitude ramp is read: voice_amp) */
    if (!L)
        return;
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
    dx_run(dxe_kit(t), ln, &L->x, p[P_E0] + lfo_shp, L->st * 16 + (p[P_E1] - 64) * 3 + lfo_pit, p[P_E3] - 64, p[P_E2] - 64,
           p[P_E4] - 64, p[P_E6], p[P_E5], y, n);
    for (i = 0; i < n; i++)                              /* x1.25 */
        out[i] += voice_amp(y[i] + (y[i] >> 2), m, i) * 2;
}

/* (Jiant 0.6.16, drum_bus) the kit's sum, once a block (voice.c track_render, after the part's voices): DRV x1..x4 into the
 * soft clip, its level kept, and the knee (were each hit's: eight lanes, two curves a sample each; now the bus's, a glue) */
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
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = DV_NLANE,
    .oneshot = 1,
    .keys = dxe_keys,
};
