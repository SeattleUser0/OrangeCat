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
 * through filters and a diffuser), STRING (plucked strings), MODAL (struck resonators), KICK, SNARE, HI-HAT.
 *
 * One voice per track (MONO in all voice modes, as the module): its state is the part's (mc_part_t, from the audio
 * arena: the largest model, STRING, sets its size). A note starts the model's trigger; the key held keeps it high.
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
       MC_KICK, MC_SNARE, MC_HIHAT, MC_N };
static const char *const N_MC_MODEL[MC_N] = {"VA", "WAVSHP", "FM", "GRAIN", "ADDTV", "CHORD", "SWARM", "NOISE",
                                             "PARTCL", "STRING", "MODAL", "KICK", "SNARE", "HI-HAT"};
/* the module's output levels per model (OUT, AUX; below 0: through a limiter at that drive) and the models that make
 * their own envelope (the low-pass gate is skipped there, as on the module) */
static const struct { float out, aux; uint8_t enveloped; } MC_POST[MC_N] = {
    {0.8f, 0.8f, 0}, {0.7f, 0.6f, 0}, {0.6f, 0.6f, 0}, {0.7f, 0.6f, 0}, {0.8f, 0.8f, 0}, {0.8f, 0.8f, 0},
    {-3.0f, 1.0f, 0}, {-1.0f, -1.0f, 0}, {-2.0f, 1.0f, 0}, {-1.0f, 0.8f, 1}, {-1.0f, 0.8f, 1}, {0.8f, 0.8f, 1},
    {0.8f, 0.8f, 1}, {0.8f, 0.8f, 1},
};

typedef struct {
    uint8_t model;                                    /* the model whose state is in m (MC_N: none yet) */
    uint8_t rise;                                     /* a note started: the next render is a rising edge */
    float note;                                       /* last note (the module averages two in a row) */
    float accent;
    float decay_env;                                  /* DECAY's envelope (1 at a note, falls) */
    float lpg_gain;
    pl_svf_t lpg;
    pl_limiter_t limit_out, limit_aux;
    float out[MC_SUB], aux[MC_SUB];
    union {
        pl_va_t va; pl_ws_t ws; pl_fm_t fm; pl_grain_t grain; pl_add_t add; pl_chord_t chord; pl_swarm_t swarm;
        pl_noise_t noise; pl_particle_t particle; pl_string_t string; pl_modal_t modal; pl_bd_t bd; pl_sd_t sd;
        pl_hh_t hh;
    } m;
} mc_part_t;
static mc_part_t *mc_part(uint32_t part);             /* engines.c */

/* a model's state from scratch (the model changed) */
static MC_XIP void mc_start(mc_part_t *s, uint32_t model)
{
    switch (model) {
    case MC_VA: pl_va_init(&s->m.va); pl_va_reset(&s->m.va); break;
    case MC_WAVSHP: pl_ws_init(&s->m.ws); pl_ws_reset(&s->m.ws); break;
    case MC_FM: pl_fm_init(&s->m.fm); pl_fm_reset(&s->m.fm); break;
    case MC_GRAIN: pl_grain_init(&s->m.grain); pl_grain_reset(&s->m.grain); break;
    case MC_ADDTV: pl_add_init(&s->m.add); pl_add_reset(&s->m.add); break;
    case MC_CHORD: pl_chord_init(&s->m.chord); pl_chord_reset(&s->m.chord); break;
    case MC_SWARM: pl_swarm_init(&s->m.swarm); pl_swarm_reset(&s->m.swarm); break;
    case MC_NOISE: pl_noise_init(&s->m.noise); pl_noise_reset(&s->m.noise); break;
    case MC_PARTCL: pl_particle_init(&s->m.particle); pl_particle_reset(&s->m.particle); break;
    case MC_STRING: pl_string_init(&s->m.string); pl_string_reset(&s->m.string); break;
    case MC_MODAL: pl_modal_init(&s->m.modal); pl_modal_reset(&s->m.modal); break;
    case MC_KICK: pl_bd_init(&s->m.bd); pl_bd_reset(&s->m.bd); break;
    case MC_SNARE: pl_sd_init(&s->m.sd); pl_sd_reset(&s->m.sd); break;
    default: pl_hh_init(&s->m.hh); pl_hh_reset(&s->m.hh); break;
    }
    s->model = (uint8_t)model;
    pl_limiter_init(&s->limit_out);
    pl_limiter_init(&s->limit_aux);
    pl_svf_init(&s->lpg);
    s->lpg_gain = 0.0f;
}

static MC_XIP void mc_model_render(mc_part_t *s, const pl_params_t *q, uint32_t n)
{
    int env = 0;
    float *o = s->out, *a = s->aux;
    switch (s->model) {
    case MC_VA: pl_va_render(&s->m.va, q, o, a, n, &env); break;
    case MC_WAVSHP: pl_ws_render(&s->m.ws, q, o, a, n, &env); break;
    case MC_FM: pl_fm_render(&s->m.fm, q, o, a, n, &env); break;
    case MC_GRAIN: pl_grain_render(&s->m.grain, q, o, a, n, &env); break;
    case MC_ADDTV: pl_add_render(&s->m.add, q, o, a, n, &env); break;
    case MC_CHORD: pl_chord_render(&s->m.chord, q, o, a, n, &env); break;
    case MC_SWARM: pl_swarm_render(&s->m.swarm, q, o, a, n, &env); break;
    case MC_NOISE: pl_noise_render(&s->m.noise, q, o, a, n, &env); break;
    case MC_PARTCL: pl_particle_render(&s->m.particle, q, o, a, n, &env); break;
    case MC_STRING: pl_string_render(&s->m.string, q, o, a, n, &env); break;
    case MC_MODAL: pl_modal_render(&s->m.modal, q, o, a, n, &env); break;
    case MC_KICK: pl_bd_render(&s->m.bd, q, o, a, n, &env); break;
    case MC_SNARE: pl_sd_render(&s->m.sd, q, o, a, n, &env); break;
    default: pl_hh_render(&s->m.hh, q, o, a, n, &env); break;
    }
    switch (s->model) {                               /* tiny decaying states to 0 (plaits/pl_dsp.h pl_flush) */
    case MC_VA: pl_va_flush(&s->m.va); break;
    case MC_WAVSHP: pl_ws_flush(&s->m.ws); break;
    case MC_FM: pl_fm_flush(&s->m.fm); break;
    case MC_GRAIN: pl_grain_flush(&s->m.grain); break;
    case MC_ADDTV: pl_add_flush(&s->m.add); break;
    case MC_CHORD: pl_chord_flush(&s->m.chord); break;
    case MC_SWARM: pl_swarm_flush(&s->m.swarm); break;
    case MC_NOISE: pl_noise_flush(&s->m.noise); break;
    case MC_PARTCL: pl_particle_flush(&s->m.particle); break;
    case MC_STRING: pl_string_flush(&s->m.string); break;
    case MC_MODAL: pl_modal_flush(&s->m.modal); break;
    case MC_KICK: pl_bd_flush(&s->m.bd); break;
    case MC_SNARE: pl_sd_flush(&s->m.sd); break;
    default: pl_hh_flush(&s->m.hh); break;
    }
    pl_flush_svf(&s->lpg);
    pl_flush(&s->decay_env);
}

static MC_XIP void mc_note_on(track_t *t, voice_t *v)
{
    mc_part_t *s = mc_part((uint32_t)(t - trk));
    if (!s) return;
    s->rise = 1;
    s->accent = (float)v->vel * (1.0f / 127.0f);
    s->decay_env = 1.0f;
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
    uint32_t model = (uint32_t)clamp(p[P_E0], 0, MC_N - 1), done = 0, i;
    if (!s) return;
    if (s->model != model) {
        mc_start(s, model);
        s->note = 0.0f;
    }
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
    q.accent = s->accent;
    if (s->note == 0.0f) s->note = note;
    while (done < n) {
        uint32_t k = n - done < MC_SUB ? n - done : MC_SUB;
        q.trigger = (s->rise ? PL_TRIGGER_RISING_EDGE : 0) | (v->gate ? PL_TRIGGER_HIGH : 0);
        s->rise = 0;
        q.note = (note + s->note) * 0.5f;             /* (voice.cc: the module averages the last two) */
        s->note = note;
        s->decay_env *= 1.0f - decay * 2.0f;
        q.timbre = timbre + tmod * s->decay_env;
        PL_CONSTRAIN(q.timbre, 0.0f, 1.0f);
        q.morph = morph;
        PL_CONSTRAIN(q.morph, 0.0f, 1.0f);
        mc_model_render(s, &q, k);
        if (g_out < 0.0f) pl_limiter_process(&s->limit_out, -g_out, s->out, k);
        if (g_aux < 0.0f) pl_limiter_process(&s->limit_aux, -g_aux, s->aux, k);
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
            pl_svf_set_f_q(&s->lpg, PL_FREQ_DIRTY, f, 0.4f);
            for (i = 0; i < k; i++) {
                float lp = pl_svf_process(&s->lpg, PL_LP, s->out[i]);
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

static MC_XIP uint32_t mc_units(const track_t *t) { (void)t; return 3u; }

/* name, {MODEL, HARM, TIMBRE, MORPH, AUX, LPG, DECAY, T.MOD}, {A D S R}, ENV -> FILTER (MORPH), mono */
static const preset_t MC_PRESETS[] = {
    {"VA BASS", {MC_VA, 64, 30, 70, 0, 40, 50, 20}, {0, 60, 90, 20}, 10, 1},
    {"VA SYNC LEAD", {MC_VA, 76, 90, 40, 100, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"FOLD PLUCK", {MC_WAVSHP, 40, 40, 80, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 1},
    {"FOLD LEAD", {MC_WAVSHP, 90, 70, 60, 40, 0, 70, 0}, {2, 70, 100, 40}, 0, 1},
    {"FM BELL", {MC_FM, 90, 60, 30, 0, 0, 80, 30}, {0, 96, 0, 90}, 0, 1},
    {"FM BASS", {MC_FM, 30, 70, 50, 0, 50, 45, 40}, {0, 60, 70, 20}, 0, 1},
    {"FORMANT VOX", {MC_GRAIN, 50, 60, 64, 0, 0, 70, 0}, {20, 80, 110, 60}, 0, 1},
    {"GRAIN SWEEP", {MC_GRAIN, 80, 90, 100, 60, 0, 90, -30}, {10, 90, 100, 70}, 0, 1},
    {"ORGAN DRAWBAR", {MC_ADDTV, 40, 50, 20, 0, 0, 70, 0}, {2, 60, 120, 30}, 0, 1},
    {"ADDITIVE PAD", {MC_ADDTV, 90, 64, 90, 64, 0, 100, 20}, {60, 90, 110, 90}, 0, 1},
    {"CHORD MINOR", {MC_CHORD, 36, 40, 30, 0, 0, 70, 0}, {20, 80, 110, 70}, 0, 1},
    {"CHORD STAB", {MC_CHORD, 70, 64, 60, 0, 80, 40, 0}, {0, 60, 0, 50}, 0, 1},
    {"SWARM PAD", {MC_SWARM, 50, 40, 70, 0, 0, 90, 0}, {70, 90, 110, 90}, 0, 1},
    {"NOISE WIND", {MC_NOISE, 70, 50, 90, 0, 0, 90, 0}, {80, 90, 100, 90}, 0, 1},
    {"NOISE SWEEP", {MC_NOISE, 40, 90, 30, 64, 0, 80, -40}, {10, 80, 100, 70}, 0, 1},
    {"DUST RAIN", {MC_PARTCL, 64, 70, 80, 0, 0, 90, 0}, {10, 90, 110, 90}, 0, 1},
    {"PLUCKED STRING", {MC_STRING, 64, 70, 64, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 1},
    {"LOW STRING", {MC_STRING, 40, 40, 110, 0, 0, 80, 0}, {0, 127, 127, 96}, 0, 1},
    {"MALLET", {MC_MODAL, 50, 50, 70, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 1},
    {"GLASS BELL", {MC_MODAL, 100, 90, 100, 0, 0, 60, 0}, {0, 127, 127, 104}, 0, 1},
    {"KICK DEEP", {MC_KICK, 30, 40, 80, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"KICK 909", {MC_KICK, 90, 64, 60, 127, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"SNARE", {MC_SNARE, 50, 64, 64, 0, 0, 50, 0}, {0, 127, 127, 90}, 0, 1},
    {"HI-HAT", {MC_HIHAT, 64, 90, 30, 0, 0, 50, 0}, {0, 127, 127, 80}, 0, 1},
    {"OPEN HAT", {MC_HIHAT, 64, 90, 90, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
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
    .poly = 1,
    .note_on = mc_note_on,
    .render = mc_render,
    .units = mc_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
