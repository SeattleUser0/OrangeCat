/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.5.0: MACRO, engine 8 (the slot of Melodee's retired GRAIN). Fourteen sound models in one engine, each
 * shaped by three knobs (HARM, TIMBRE, MORPH), ported from the open source code of a well known Eurorack macro
 * oscillator (firmware/src/plaits/, MIT: LICENSES/MIT-Mutable.txt). The models themselves are line for line ports
 * (tests/plaits_parity checks them sample for sample against the original); this file is the glue to Melodee's
 * voices.
 *
 * The edit pages:
 *   EDIT 1 MODEL: MODL (the model), HARM, TIMB (TIMBRE), MRPH (MORPH). HOME's four knobs are these too.
 *   EDIT 2 SHAPE: AUX (blend to the model's second output: 0 the main one, 127 the other), LPG (a low-pass gate: the
 *                 ADSR also closes a filter, 0 = off, 127 = darkest), DECY (an envelope of its own, started by each
 *                 note), TMOD (how much that envelope moves TIMBRE, either way). (Labels are four letters at most:
 *                 the screen's cells.)
 * The models: VA (two analog style oscillators), WAVSHP (waveshaper and folder), FM (two operators), GRAIN (formant
 * grains), ADDTV (additive), CHORD (four voice chords), SWARM (eight sawtooths), NOISE (filtered noise), PARTCL (dust
 * through filters and a diffuser), STRING (plucked strings), MODAL (struck resonators), KICK, SNARE, HI-HAT, and
 * (0.5.2, Orange Cat's own) HARP: STRING's string, one a voice, so its chords ring.
 *
 * Voices: four per track in POLY (and UNISON), each a model of its own (0.5.2), for chords; PARTCL and STRING, whose
 * states are 8 KB each, play one at a time (STRING's three strings ring in turn, note after note; HARP has a string
 * a voice for chords). The voices' states are the part's (mc_part_t, from the audio arena: one STRING, four HARPs
 * (11 KB, the largest) or four of the others). A note starts its voice's trigger; the key held keeps it high.
 * Pitch comes from the voice (glide, bend, TUNE, A4 and the pitch modulations: pitch16 and fine). The voice's ADSR
 * is the VCA as for every engine; for STRING, MODAL and the drums (models with their own decay) the presets hold
 * the ADSR open and let the model decay. ENV -> SHAPE and LFO -> SHAPE move TIMBRE; ENV -> FILTER and LFO -> FILTER
 * move MORPH; velocity is the models' accent.
 *
 * Where it runs: the models (about 40 KB of code) are too big for the SRAM the audio code lives in, so the engine's
 * callbacks and everything they call stay in flash (section .text.xip_macro: tools/audio_ram.py). They render in
 * 16-frame pieces (the models' parameters glide over each piece, as the module's 12-frame blocks at 48 kHz).
 * Sample rate: 44.1 kHz. Pitch is exact; the models' own times (decays) are 8.8 % longer than on the module.
 *
 * Revisions:
 *   0.5.0: first version.
 *   0.5.2: polyphonic: four voices (MC_POLY) but in PARTCL and STRING (mc_cap); a voice's state apart (mc_vhead_t, the
 *          model's in mc_part_t.m.small[voice]); the pad, keys, chord and bell presets play POLY. HARP (a 15th model,
 *          one string a voice) with presets HARP and STRING CHORDS.
 */
#include "plaits/pl_dsp.h"
#include "plaits/pl_osc.h"
#include "plaits/pl_va.h"
#include "plaits/pl_waveshaping.h"
#include "plaits/pl_fm.h"
#include "plaits/pl_grain.h"
#include "plaits/pl_additive.h"
#include "plaits/pl_chord.h"
#include "plaits/pl_swarm.h"
#include "plaits/pl_noise.h"
#include "plaits/pl_particle.h"
#include "plaits/pl_string.h"
#include "plaits/pl_modal.h"
#include "plaits/pl_bassdrum.h"
#include "plaits/pl_snare.h"
#include "plaits/pl_hihat.h"

#define MC_XIP __attribute__((section(".text.xip_macro"), noinline))
#define MC_SUB 16u                                    /* frames per model render (<= PL_MAX_BLOCK) */

enum { MC_VA, MC_WAVSHP, MC_FM, MC_GRAIN, MC_ADDTV, MC_CHORD, MC_SWARM, MC_NOISE, MC_PARTCL, MC_STRING, MC_MODAL,
       MC_KICK, MC_SNARE, MC_HIHAT, MC_HARP, MC_N };   /* (0.5.2: HARP at the end, so stored sounds keep theirs) */
static const char *const N_MC_MODEL[MC_N] = {"VA", "WAVSHP", "FM", "GRAIN", "ADDTV", "CHORD", "SWARM", "NOISE",
                                             "PARTCL", "STRING", "MODAL", "KICK", "SNARE", "HI-HAT", "HARP"};
/* the module's output levels per model (OUT, AUX; below 0: through a limiter at that drive) and the models that make
 * their own envelope (the low-pass gate is skipped there, as on the module) */
static const struct { float out, aux; uint8_t enveloped; } MC_POST[MC_N] = {
    {0.8f, 0.8f, 0}, {0.7f, 0.6f, 0}, {0.6f, 0.6f, 0}, {0.7f, 0.6f, 0}, {0.8f, 0.8f, 0}, {0.8f, 0.8f, 0},
    {-3.0f, 1.0f, 0}, {-1.0f, -1.0f, 0}, {-2.0f, 1.0f, 0}, {-1.0f, 0.8f, 1}, {-1.0f, 0.8f, 1}, {0.8f, 0.8f, 1},
    {0.8f, 0.8f, 1}, {0.8f, 0.8f, 1}, {-1.0f, 0.8f, 1},   /* (HARP: STRING's levels) */
};

#define MC_POLY 4u                                    /* (0.5.2) voices per track, PARTCL and STRING: 1 */
/* (0.5.2) a voice's own: what the module keeps beside its model */
typedef struct {
    uint8_t rise;                                     /* a note started: the next render is a rising edge */
    float note;                                       /* last note (the module averages two in a row) */
    float accent;
    float decay_env;                                  /* DECAY's envelope (1 at a note, falls) */
    pl_svf_t lpg;
    pl_limiter_t limit_out, limit_aux;
} mc_vhead_t;
typedef union {                                       /* a voice's model (all but the two large ones) */
    pl_va_t va; pl_ws_t ws; pl_fm_t fm; pl_grain_t grain; pl_add_t add; pl_chord_t chord; pl_swarm_t swarm;
    pl_noise_t noise; pl_modal_t modal; pl_bd_t bd; pl_sd_t sd; pl_hh_t hh;
} mc_small_t;
typedef struct {
    uint8_t model;                                    /* the model whose states are in m (MC_N: none yet) */
    float out[MC_SUB], aux[MC_SUB];                   /* (the voices render one after another: one buffer) */
    mc_vhead_t h[MC_POLY];
    union {
        mc_small_t small[MC_POLY];
        pl_harp_t harp[MC_POLY];                      /* (0.5.2) HARP: a string a voice */
        pl_particle_t particle;                       /* PARTCL and STRING: one voice */
        pl_string_t string;
    } m;
} mc_part_t;
static mc_part_t *mc_part(uint32_t part);             /* engines.c */
static inline int mc_big(uint32_t model) { return model == MC_PARTCL || model == MC_STRING; }
static MC_XIP uint32_t mc_cap(const track_t *t)
{
    return mc_big((uint32_t)clamp(t->p[P_E0], 0, MC_N - 1)) ? 1u : MC_POLY;
}
/* voice i's model state (void: the model's own type, by s->model) */
static inline void *mc_state(mc_part_t *s, uint32_t i)
{
    if (s->model == MC_PARTCL) return &s->m.particle;
    if (s->model == MC_STRING) return &s->m.string;
    if (s->model == MC_HARP) return &s->m.harp[i % MC_POLY];
    return &s->m.small[i % MC_POLY];
}

/* a model's states from scratch (the model changed): every voice's */
static MC_XIP void mc_start(mc_part_t *s, uint32_t model)
{
    uint32_t i, nv = mc_big(model) ? 1u : MC_POLY;
    s->model = (uint8_t)model;
    for (i = 0; i < nv; i++) {
        void *m = mc_state(s, i);
        mc_vhead_t *h = &s->h[i];
        switch (model) {
        case MC_VA: pl_va_init(m); pl_va_reset(m); break;
        case MC_WAVSHP: pl_ws_init(m); pl_ws_reset(m); break;
        case MC_FM: pl_fm_init(m); pl_fm_reset(m); break;
        case MC_GRAIN: pl_grain_init(m); pl_grain_reset(m); break;
        case MC_ADDTV: pl_add_init(m); pl_add_reset(m); break;
        case MC_CHORD: pl_chord_init(m); pl_chord_reset(m); break;
        case MC_SWARM: pl_swarm_init(m); pl_swarm_reset(m); break;
        case MC_NOISE: pl_noise_init(m); pl_noise_reset(m); break;
        case MC_PARTCL: pl_particle_init(m); pl_particle_reset(m); break;
        case MC_STRING: pl_string_init(m); pl_string_reset(m); break;
        case MC_MODAL: pl_modal_init(m); pl_modal_reset(m); break;
        case MC_KICK: pl_bd_init(m); pl_bd_reset(m); break;
        case MC_SNARE: pl_sd_init(m); pl_sd_reset(m); break;
        case MC_HARP: pl_harp_init(m); pl_harp_reset(m); break;
        default: pl_hh_init(m); pl_hh_reset(m); break;
        }
        pl_limiter_init(&h->limit_out);
        pl_limiter_init(&h->limit_aux);
        pl_svf_init(&h->lpg);
        h->note = 0.0f;
    }
}

static MC_XIP void mc_model_render(mc_part_t *s, uint32_t vi, const pl_params_t *q, uint32_t n)
{
    int env = 0;
    float *o = s->out, *a = s->aux;
    void *m = mc_state(s, vi);
    switch (s->model) {
    case MC_VA: pl_va_render(m, q, o, a, n, &env); pl_va_flush(m); break;
    case MC_WAVSHP: pl_ws_render(m, q, o, a, n, &env); pl_ws_flush(m); break;
    case MC_FM: pl_fm_render(m, q, o, a, n, &env); pl_fm_flush(m); break;
    case MC_GRAIN: pl_grain_render(m, q, o, a, n, &env); pl_grain_flush(m); break;
    case MC_ADDTV: pl_add_render(m, q, o, a, n, &env); pl_add_flush(m); break;
    case MC_CHORD: pl_chord_render(m, q, o, a, n, &env); pl_chord_flush(m); break;
    case MC_SWARM: pl_swarm_render(m, q, o, a, n, &env); pl_swarm_flush(m); break;
    case MC_NOISE: pl_noise_render(m, q, o, a, n, &env); pl_noise_flush(m); break;
    case MC_PARTCL: pl_particle_render(m, q, o, a, n, &env); pl_particle_flush(m); break;
    case MC_STRING: pl_string_render(m, q, o, a, n, &env); pl_string_flush(m); break;
    case MC_MODAL: pl_modal_render(m, q, o, a, n, &env); pl_modal_flush(m); break;
    case MC_KICK: pl_bd_render(m, q, o, a, n, &env); pl_bd_flush(m); break;
    case MC_SNARE: pl_sd_render(m, q, o, a, n, &env); pl_sd_flush(m); break;
    case MC_HARP: pl_harp_render(m, q, o, a, n, &env); pl_harp_flush(m); break;
    default: pl_hh_render(m, q, o, a, n, &env); pl_hh_flush(m); break;
    }                                                 /* (the flushes: tiny decaying states to 0, pl_dsp.h pl_flush) */
    pl_flush_svf(&s->h[vi].lpg);
    pl_flush(&s->h[vi].decay_env);
}

static MC_XIP void mc_note_on(track_t *t, voice_t *v)
{
    mc_part_t *s = mc_part((uint32_t)(t - trk));
    uint32_t vi = (uint32_t)(v - t->v) % MC_POLY;
    if (!s) return;
    s->h[vi].rise = 1;
    s->h[vi].accent = (float)v->vel * (1.0f / 127.0f);
    s->h[vi].decay_env = 1.0f;
}

/* the module's attenuverter curve (voice.h ApplyModulations): a dead zone around the middle */
static inline float mc_amount(int32_t v)
{
    float a = (float)v * (1.0f / 64.0f);
    return a * pl_maxf(pl_fabsf(a) - 0.05f, 0.05f) * 1.05f;
}

static MC_XIP void mc_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    mc_part_t *s = mc_part((uint32_t)(t - trk));
    const int16_t *p = t->p;
    uint32_t model = (uint32_t)clamp(p[P_E0], 0, MC_N - 1), done = 0, i, vi = (uint32_t)(v - t->v) % MC_POLY;
    mc_vhead_t *h;
    if (!s) return;
    if (s->model != model)
        mc_start(s, model);
    if (mc_big(model) && vi)                          /* (voices left from a model of four: silent to their end) */
        return;
    h = &s->h[vi];
    /* the voice's pitch as a MIDI note: 1/16 semitones, the residual (1/4096 of the frequency: 12 / (4096 ln 2)
     * semitones each), the A4 tuning (Q24 octaves) */
    float note = (float)m->pitch16 * (1.0f / 16.0f) + (float)m->fine * 0.00422689f
                 + (float)tuning_log() * (12.0f / 16777216.0f);
    float timbre = (float)p[P_E2] * (1.0f / 127.0f) + (float)(m->shape - (64 << 8)) * (1.0f / 32768.0f);
    float morph = (float)p[P_E3] * (1.0f / 127.0f) + (float)m->cutoff * (1.0f / 32512.0f);
    float harm = (float)p[P_E1] * (1.0f / 127.0f), mixf = (float)p[P_E4] * (1.0f / 127.0f);
    float colour = (float)p[P_E5] * (1.0f / 127.0f), tmod = mc_amount(p[P_E7]);
    /* DECAY: 0 about 10 ms .. 127 about 6 s, as the module's (voice.cc short_decay, per 16 frames) */
    float decay = (200.0f * (float)MC_SUB) / PL_SAMPLE_RATE * pl_semitones_to_ratio(-96.0f * (float)p[P_E6] * (1.0f / 127.0f));
    float g_out = MC_POST[model].out, g_aux = MC_POST[model].aux;
    int lpg = p[P_E5] > 0 && !MC_POST[model].enveloped;
    pl_params_t q;
    PL_CONSTRAIN(harm, 0.0f, 1.0f);
    q.harmonics = harm;
    q.accent = h->accent;
    if (h->note == 0.0f) h->note = note;
    while (done < n) {
        uint32_t k = n - done < MC_SUB ? n - done : MC_SUB;
        q.trigger = (h->rise ? PL_TRIGGER_RISING_EDGE : 0) | (v->gate ? PL_TRIGGER_HIGH : 0);
        h->rise = 0;
        q.note = (note + h->note) * 0.5f;             /* (voice.cc: the module averages the last two) */
        h->note = note;
        h->decay_env *= 1.0f - decay * 2.0f;
        q.timbre = timbre + tmod * h->decay_env;
        PL_CONSTRAIN(q.timbre, 0.0f, 1.0f);
        q.morph = morph;
        PL_CONSTRAIN(q.morph, 0.0f, 1.0f);
        mc_model_render(s, vi, &q, k);
        if (g_out < 0.0f) pl_limiter_process(&h->limit_out, -g_out, s->out, k);
        if (g_aux < 0.0f) pl_limiter_process(&h->limit_aux, -g_aux, s->aux, k);
        {
            float go = g_out < 0.0f ? 1.0f : g_out, ga = g_aux < 0.0f ? 1.0f : g_aux;
            for (i = 0; i < k; i++) s->out[i] = s->out[i] * go + (s->aux[i] * ga - s->out[i] * go) * mixf;
        }
        if (lpg) {                                    /* (envelope.h LPGEnvelope: the ADSR as the vactrol's level) */
            float e = (float)m->envq15 * (1.0f / 32767.0f), e2, e4, tail, tail2, f, bleed;
            PL_CONSTRAIN(e, 0.0f, 1.0f);
            e2 = e * e;
            e4 = e2 * e2;
            tail = 1.0f - e;
            tail2 = tail * tail;
            f = 0.003f + 0.3f * e4 + (1.0f - colour) * 0.04f;
            bleed = (tail2 + (1.0f - tail2) * (1.0f - colour)) * (1.0f - colour) * (1.0f - colour);
            pl_svf_set_f_q(&h->lpg, PL_FREQ_DIRTY, f, 0.4f);
            for (i = 0; i < k; i++) {
                float lp = pl_svf_process(&h->lpg, PL_LP, s->out[i]);
                s->out[i] = lp + (s->out[i] - lp) * bleed;
            }
        }
        for (i = 0; i < k; i++) {
            float x = s->out[i];
            int32_t y;
            x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
            y = (int32_t)(x * 32767.0f);
            out[done + i] += voice_amp(y << 1, m, done + i) << 1;
        }
        done += k;
    }
}

/* the voice budget's units a voice takes (voice.c): (0.5.2) by its model's cost, now that a track may sound four.
 * VA, WAVSHP and HARP (one string) about a CZ-1 voice: 2; ADDTV the dearest: 4; the rest 3 (host figures:
 * tests/macro_test) */
static MC_XIP uint32_t mc_units(const track_t *t)
{
    uint32_t model = (uint32_t)clamp(t->p[P_E0], 0, MC_N - 1);
    return model <= MC_WAVSHP || model == MC_HARP ? 2u : model == MC_ADDTV ? 4u : 3u;
}

/* name, {MODEL, HARM, TIMBRE, MORPH, AUX, LPG, DECAY, T.MOD}, {A D S R}, ENV -> FILTER (MORPH), mono (0.5.2: 0, POLY,
 * for the pads, keys, chords and bells: they play a step's four notes) */
static const preset_t MC_PRESETS[] = {
    {"VA BASS", {MC_VA, 64, 30, 70, 0, 40, 50, 20}, {0, 60, 90, 20}, 10, 1},
    {"VA SYNC LEAD", {MC_VA, 76, 90, 40, 100, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"FOLD PLUCK", {MC_WAVSHP, 40, 40, 80, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"FOLD LEAD", {MC_WAVSHP, 90, 70, 60, 40, 0, 70, 0}, {2, 70, 100, 40}, 0, 1},
    {"FM BELL", {MC_FM, 90, 60, 30, 0, 0, 80, 30}, {0, 96, 0, 90}, 0, 0},
    {"FM BASS", {MC_FM, 30, 70, 50, 0, 50, 45, 40}, {0, 60, 70, 20}, 0, 1},
    {"FORMANT VOX", {MC_GRAIN, 50, 60, 64, 0, 0, 70, 0}, {20, 80, 110, 60}, 0, 1},
    {"GRAIN SWEEP", {MC_GRAIN, 80, 90, 100, 60, 0, 90, -30}, {10, 90, 100, 70}, 0, 1},
    {"ORGAN DRAWBAR", {MC_ADDTV, 40, 50, 20, 0, 0, 70, 0}, {2, 60, 120, 30}, 0, 0},
    {"ADDITIVE PAD", {MC_ADDTV, 90, 64, 90, 64, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"CHORD MINOR", {MC_CHORD, 36, 40, 30, 0, 0, 70, 0}, {20, 80, 110, 70}, 0, 0},
    {"CHORD STAB", {MC_CHORD, 70, 64, 60, 0, 80, 40, 0}, {0, 60, 0, 50}, 0, 0},
    {"SWARM PAD", {MC_SWARM, 50, 40, 70, 0, 0, 90, 0}, {70, 90, 110, 90}, 0, 0},
    {"NOISE WIND", {MC_NOISE, 70, 50, 90, 0, 0, 90, 0}, {80, 90, 100, 90}, 0, 1},
    {"NOISE SWEEP", {MC_NOISE, 40, 90, 30, 64, 0, 80, -40}, {10, 80, 100, 70}, 0, 1},
    {"DUST RAIN", {MC_PARTCL, 64, 70, 80, 0, 0, 90, 0}, {10, 90, 110, 90}, 0, 1},
    {"PLUCKED STRING", {MC_STRING, 64, 70, 64, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 1},
    {"LOW STRING", {MC_STRING, 40, 40, 110, 0, 0, 80, 0}, {0, 127, 127, 96}, 0, 1},
    {"MALLET", {MC_MODAL, 50, 50, 70, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},
    {"GLASS BELL", {MC_MODAL, 100, 90, 100, 0, 0, 60, 0}, {0, 127, 127, 104}, 0, 0},
    {"KICK DEEP", {MC_KICK, 30, 40, 80, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"KICK 909", {MC_KICK, 90, 64, 60, 127, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"SNARE", {MC_SNARE, 50, 64, 64, 0, 0, 50, 0}, {0, 127, 127, 90}, 0, 1},
    {"HI-HAT", {MC_HIHAT, 64, 90, 30, 0, 0, 50, 0}, {0, 127, 127, 80}, 0, 1},
    {"OPEN HAT", {MC_HIHAT, 64, 90, 90, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"HARP", {MC_HARP, 64, 80, 70, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},          /* (0.5.2) */
    {"STRING CHORDS", {MC_HARP, 40, 50, 100, 0, 0, 80, 0}, {0, 127, 127, 104}, 0, 0},
};

static const engine_t ENG_MACRO = {
    .name = "MACRO",
    .page_title = {"MODEL", "SHAPE"},
    .edit = {
        {"MODL", F_ENUM, 0, MC_N - 1, 0, N_MC_MODEL, 0},
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
        {"TIMB", F_PCT, 0, 127, 64, 0, 0},
        {"MRPH", F_PCT, 0, 127, 64, 0, 0},
        {"AUX", F_PCT, 0, 127, 0, 0, 0},
        {"LPG", F_PCT, 0, 127, 0, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"TMOD", F_BIPCT, -64, 63, 0, 0, 0},
    },
    .presets = MC_PRESETS,
    .npresets = NELEM(MC_PRESETS),
    .poly = MC_POLY,
    .cap = mc_cap,                                     /* (0.5.2) PARTCL, STRING: 1 */
    .note_on = mc_note_on,
    .render = mc_render,
    .units = mc_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
