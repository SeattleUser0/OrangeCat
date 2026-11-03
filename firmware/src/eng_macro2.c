/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO2 (candidate, branch macro2-candidate; docs/candidates/macro2.md): the ten sound models of the same Eurorack macro oscillator that MACRO
 * (eng_macro.c) does not carry, as one engine, built only as a loadable module (tools/module/macro2_mod.c) under the proposed number ENGI_MACRO2
 * (32, an OC_CANDIDATES build's). The models are ports of the original C++ (firmware/src/plaits/pl_wavetable.h, pl_speech.h, pl_vavcf.h, pl_pd.h,
 * pl_sixop.h, pl_terrain.h, pl_strmach.h, pl_chiptune.h; MIT: LICENSES/MIT-Mutable.txt), checked sample for sample by tests/plaits_parity; this
 * file is eng_macro.c's glue with another model list, so a MACRO2 track plays exactly as a MACRO track does: the same pages, knobs, voices,
 * LPG, DECAY envelope and batch kernel.
 *
 * The edit pages (eng_macro.c's):
 *   EDIT 1 MODEL: MODL, HARM, TIMB, MRPH (HOME's four knobs too).  EDIT 2 SHAPE: AUX, LPG, DECY, TMOD.
 * The models, in the order of the module's second bank, then the two left out of MACRO:
 *   VA-VCF  two oscillators (saw / square) through a resonant low-pass (HARM: cutoff, TIMB: resonance / drive, MRPH: shape and sub)
 *   PD      phase distortion: a shaper oscillator driven by a modulator's phase (HARM: shaper shape, TIMB: amount, MRPH: modulator ratio)
 *   6OP A/B/C  six-operator FM, a DX7-style voice with the module's three built-in banks of 32 patches each (HARM: the patch, TIMB:
 *           brightness, MRPH: envelope / LFO control); two internal voices, so a new note overlaps the last one's release
 *   TERRAN  wave terrain: a 2-D path through a landscape of functions (HARM: the terrain, TIMB: the path's radius, MRPH: its offset)
 *   STRMCH  string machine: divide-down chords through a formant filter and a chorus ensemble (HARM: chord, TIMB: registration, MRPH: ensemble)
 *   CHIP    chiptune: NES-style square chords with an arpeggiator and a triangle bass (HARM: chord, TIMB: arpeggio / envelope, MRPH: pulse width)
 *   WAVTBL  the 192-wave wavetable (3 banks of 8 x 8; HARM: bank and interpolation, TIMB: row, MRPH: column)
 *   SPEECH  three speech synthesizers (HARM: formant / SAM / LPC and the word bank, TIMB: formant shift, MRPH: the vowel or word)
 * What differs from the module, on purpose: no user data (no custom wavetable, terrain or patch banks: the loader has no path for them), 44.1 kHz
 * (as MACRO: pitch exact, the models' own times 8.8 % longer), and the state sizes written in the headers (the speech word frames, the six-op
 * patch copies).
 *
 * Voices: four a track in POLY for the small models (VA-VCF, PD, TERRAN, CHIP, WAVTBL); the models whose state is large play one voice a track
 * (6OP, STRMCH, SPEECH: MC2_BIG), as STRING and PARTCL do in MACRO. 6OP keeps the module's own two internal voices, so a new note on its one
 * voice still overlaps the last one's release.
 *
 * Where it runs: the module area (flash, XIP); the tables the models read per sample are the engine's hot set (MC2_HOT_LIST), copied into the
 * arena when the module registers, read through the services table (ENG_HOT). MACRO2_HOT=0 builds the module without a hot set (every table read
 * from the module's own flash): the A/B the unit session measures (ADDING_AN_ENGINE.md: a module declares a hot set only where it pays).
 * The wavetable's 50,688 B of waves cannot be a hot table (16 KB a table, and the arena): WAVTBL reads them from flash through the D-cache.
 *
 * Revisions:
 *   macro2-candidate: first version, a study: the module, the host tests, no manual (docs/candidates/macro2.md).
 */
#include "plaits/pl_tables.h"
#include "plaits/pl_tables2.h"
#ifndef MACRO2_HOT
#define MACRO2_HOT 1
#endif
/* MACRO2_SET (the size variants of docs/candidates/macro2.md): 0 all ten models; 1 the second bank alone (no WAVTBL, no SPEECH, and TERRAN without its
 * three wavetable terrains: PL_TERRAIN_NO_WT), so no wave or word table is in the image; 2 WAVTBL and SPEECH alone. A model left out renders silence. */
#ifndef MACRO2_SET
#define MACRO2_SET 0
#endif
#define MC2_HAS_BANK2 (MACRO2_SET != 2)
#define MC2_HAS_WT (MACRO2_SET != 1)
#define MC2_HAS_SPEECH (MACRO2_SET != 1)
#if MACRO2_SET == 1
#define PL_TERRAIN_NO_WT 1
#endif
#ifndef MC2_UNITS_TABLE                               /* (the host's ratios, tests/plaits_parity/bench.sh: the comment at MC2_UNITS) */
#define MC2_UNITS_TABLE 5, 5, 6, 6, 6, 7, 5, 2, 8, 5
#endif
/* The hot set: the tables the models read every sample (docs/candidates/macro2.md, "Hot set"; found by reading the sample loops as tools/pack_hot.py does).
 * The tables of pl_tables.h live in the asset pack, those of pl_tables2.h in the module's own image: the loader accepts either source. */
#define MC2_HOT_LIST(X) X(pl_lut_sine, PL_LUT_SINE_SIZE * 4u, float) X(pl_lut_pitch_ratio_high, PL_LUT_PITCH_RATIO_HIGH_SIZE * 4u, float) \
    X(pl_lut_pitch_ratio_low, PL_LUT_PITCH_RATIO_LOW_SIZE * 4u, float) X(pl_lut_lpc_excitation_pulse, PL_LUT_LPC_EXCITATION_PULSE_SIZE, int8_t)
#define MC2_HOT_ENTRY(t, b, ty) {t, b},
#define MC2_HOT_SRC(t, b, ty) t,
#define MC2_HOT_IDX(t, b, ty) MC2H_##t,
enum { MC2_HOT_LIST(MC2_HOT_IDX) MC2_HOT_N };
static const eng_hot_t MC2_HOT[MC2_HOT_N] = { MC2_HOT_LIST(MC2_HOT_ENTRY) };
#if !OC_MODULE
static const void *mc2_hot[MC2_HOT_N] = { MC2_HOT_LIST(MC2_HOT_SRC) };   /* (a built-in engine's own pointer array; a module reads through fw_api_t.hot) */
#else
#define mc2_hot 0
#endif
#if MACRO2_HOT
#undef pl_lut_sine
#undef pl_lut_pitch_ratio_high
#undef pl_lut_pitch_ratio_low
#define pl_lut_sine ((const float *)ENG_HOT(mc2_hot, ENGI_MACRO2, MC2H_pl_lut_sine))
#define pl_lut_pitch_ratio_high ((const float *)ENG_HOT(mc2_hot, ENGI_MACRO2, MC2H_pl_lut_pitch_ratio_high))
#define pl_lut_pitch_ratio_low ((const float *)ENG_HOT(mc2_hot, ENGI_MACRO2, MC2H_pl_lut_pitch_ratio_low))
#define pl_lut_lpc_excitation_pulse ((const int8_t *)ENG_HOT(mc2_hot, ENGI_MACRO2, MC2H_pl_lut_lpc_excitation_pulse))
#endif
#include "plaits/pl_dsp.h"
#include "plaits/pl_dsp2.h"
#include "plaits/pl_osc.h"
#if MC2_HAS_BANK2
#include "plaits/pl_vavcf.h"
#include "plaits/pl_pd.h"
#include "plaits/pl_sixop.h"
#include "plaits/pl_terrain.h"
#include "plaits/pl_strmach.h"
#include "plaits/pl_chiptune.h"
#endif
#if MC2_HAS_WT
#include "plaits/pl_wavetable.h"
#endif
#if MC2_HAS_SPEECH
#include "plaits/pl_speech.h"
#endif
typedef struct { float z; } mc2_none_t;               /* a model left out of this set */
static inline void mc2_none_render(mc2_none_t *e, const pl_params_t *p, float *o, float *a, size_t n, int *env)
{
    (void)e; (void)p; (void)env;
    while (n--) { *o++ = 0.0f; *a++ = 0.0f; }
}
#define MC2_NONE(prefix, T) typedef mc2_none_t T; static inline void prefix##_init(T *e) { e->z = 0.0f; } static inline void prefix##_reset(T *e) { (void)e; } \
    static inline void prefix##_flush(T *e) { (void)e; } \
    static inline void prefix##_render(T *e, const pl_params_t *p, float *o, float *a, size_t n, int *env) { mc2_none_render(e, p, o, a, n, env); }
#if !MC2_HAS_BANK2
MC2_NONE(pl_vavcf, pl_vavcf_t) MC2_NONE(pl_pd, pl_pd_t) MC2_NONE(pl_sixop, pl_sixop_t) MC2_NONE(pl_terrain, pl_terrain_t) MC2_NONE(pl_strmach, pl_strmach_t)
MC2_NONE(pl_chiptune, pl_chiptune_t)
static inline void pl_sixop_load_bank(pl_sixop_t *e, int bank) { (void)e; (void)bank; }
#endif
#if !MC2_HAS_WT
MC2_NONE(pl_wt, pl_wt_t)
#endif
#if !MC2_HAS_SPEECH
MC2_NONE(pl_speech, pl_speech_t)
#endif

#define MC2_XIP __attribute__((section(".text.xip_macro2"), noinline))
#define MC2_SUB 16u                                   /* frames per model render (<= PL_MAX_BLOCK) */

enum { MC2_VAVCF, MC2_PD, MC2_6OP_A, MC2_6OP_B, MC2_6OP_C, MC2_TERRAN, MC2_STRMCH, MC2_CHIP, MC2_WAVTBL, MC2_SPEECH, MC2_N };
static const char *const N_MC2_MODEL[MC2_N] = {"VA-VCF", "PD", "6OP A", "6OP B", "6OP C", "TERRAN", "STRMCH", "CHIP", "WAVTBL", "SPEECH"};
/* the module's output levels per model (voice.cc: OUT, AUX; below 0: through a limiter at that drive) and the models that make their own envelope
 * (the low-pass gate is skipped there; SPEECH says so per render: a replayed word is enveloped, a held vowel is not) */
static const struct { float out, aux; uint8_t enveloped; } MC2_POST[MC2_N] = {
    {1.0f, 1.0f, 0}, {0.7f, 0.7f, 0}, {1.0f, 1.0f, 1}, {1.0f, 1.0f, 1}, {1.0f, 1.0f, 1}, {0.7f, 0.7f, 0}, {0.8f, 0.8f, 0}, {0.5f, 0.5f, 0},
    {0.6f, 0.6f, 0}, {-0.7f, 0.8f, 0},
};

#define MC2_POLY 4u                                   /* voices per track; the large models (MC2_BIG): 1 */
typedef struct {                                      /* a voice's own (eng_macro.c mc_vhead_t) */
    uint8_t rise;
    float note;
    float accent;
    float decay_env;
    pl_svf_t lpg;
    pl_limiter_t limit_out, limit_aux;
} mc2_vhead_t;
typedef union {                                       /* a voice's model (the small ones) */
    pl_vavcf_t vavcf; pl_pd_t pd; pl_terrain_t terrain; pl_chiptune_t chiptune; pl_wt_t wt;
} mc2_small_t;
typedef struct {
    uint8_t model;                                    /* the model whose states are in m (MC2_N: none yet) */
    mc2_vhead_t h[MC2_POLY];
    union {
        mc2_small_t small[MC2_POLY];
        pl_sixop_t sixop;                             /* the large models: one voice a track */
        pl_strmach_t strmach;
        pl_speech_t speech;
    } m;
} mc2_part_t;
static mc2_part_t *mc2_part(uint32_t part);           /* the module unit (tools/module/macro2_mod.c) */
static inline int mc2_big(uint32_t model) { return (model >= MC2_6OP_A && model <= MC2_6OP_C) || model == MC2_STRMCH || model == MC2_SPEECH; }
static MC2_XIP uint32_t mc2_cap(const track_t *t)
{
    return mc2_big((uint32_t)clamp(t->p[P_E0], 0, MC2_N - 1)) ? 1u : MC2_POLY;
}
static inline void *mc2_state(mc2_part_t *s, uint32_t i)
{
    if (s->model >= MC2_6OP_A && s->model <= MC2_6OP_C) return &s->m.sixop;
    if (s->model == MC2_STRMCH) return &s->m.strmach;
    if (s->model == MC2_SPEECH) return &s->m.speech;
    return &s->m.small[i % MC2_POLY];
}

/* a model's states from scratch (the model changed): every voice's */
static MC2_XIP void mc2_start(mc2_part_t *s, uint32_t model)
{
    uint32_t i, nv = mc2_big(model) ? 1u : MC2_POLY;
    s->model = (uint8_t)model;
    for (i = 0; i < nv; i++) {
        void *m = mc2_state(s, i);
        mc2_vhead_t *h = &s->h[i];
        switch (model) {
        case MC2_VAVCF: pl_vavcf_init(m); pl_vavcf_reset(m); break;
        case MC2_PD: pl_pd_init(m); pl_pd_reset(m); break;
        case MC2_6OP_A: case MC2_6OP_B: case MC2_6OP_C:
            pl_sixop_init(m); pl_sixop_load_bank(m, (int)(model - MC2_6OP_A)); pl_sixop_reset(m); break;
        case MC2_TERRAN: pl_terrain_init(m); pl_terrain_reset(m); break;
        case MC2_STRMCH: pl_strmach_init(m); pl_strmach_reset(m); break;
        case MC2_CHIP: pl_chiptune_init(m); pl_chiptune_reset(m); break;
        case MC2_WAVTBL: pl_wt_init(m); pl_wt_reset(m); break;
        default: pl_speech_init(m); pl_speech_reset(m); break;
        }
        pl_limiter_init(&h->limit_out);
        pl_limiter_init(&h->limit_aux);
        pl_svf_init(&h->lpg);
        h->note = 0.0f;
    }
}

/* one model render; returns the model's own "already enveloped" flag (SPEECH changes it per render) */
static MC2_XIP int mc2_model_render(mc2_part_t *s, uint32_t vi, const pl_params_t *q, uint32_t n, float *o, float *a)
{
    int env = 0;
    void *m = mc2_state(s, vi);
    switch (s->model) {
    case MC2_VAVCF: pl_vavcf_render(m, q, o, a, n, &env); pl_vavcf_flush(m); break;
    case MC2_PD: pl_pd_render(m, q, o, a, n, &env); pl_pd_flush(m); break;
    case MC2_6OP_A: case MC2_6OP_B: case MC2_6OP_C: pl_sixop_render(m, q, o, a, n, &env); pl_sixop_flush(m); env = 1; break;
    case MC2_TERRAN: pl_terrain_render(m, q, o, a, n, &env); pl_terrain_flush(m); break;
    case MC2_STRMCH: pl_strmach_render(m, q, o, a, n, &env); pl_strmach_flush(m); break;
    case MC2_CHIP: pl_chiptune_render(m, q, o, a, n, &env); pl_chiptune_flush(m); break;
    case MC2_WAVTBL: pl_wt_render(m, q, o, a, n, &env); pl_wt_flush(m); break;
    default: pl_speech_render(m, q, o, a, n, &env); pl_speech_flush(m); break;
    }
    pl_flush_svf(&s->h[vi].lpg);
    pl_flush(&s->h[vi].decay_env);
    return env;
}

static MC2_XIP void mc2_note_on(track_t *t, voice_t *v)
{
    mc2_part_t *s = mc2_part((uint32_t)(t - trk));
    uint32_t vi = (uint32_t)(v - t->v) % MC2_POLY;
    if (!s) return;
    s->h[vi].rise = 1;
    s->h[vi].accent = (float)v->vel * (1.0f / 127.0f);
    s->h[vi].decay_env = 1.0f;
}

static inline float mc2_amount(int32_t v)             /* the module's attenuverter curve (eng_macro.c mc_amount) */
{
    float a = (float)v * (1.0f / 64.0f);
    return a * pl_maxf(pl_fabsf(a) - 0.05f, 0.05f) * 1.05f;
}

/* the voice's block in two parts, as eng_macro.c (0.14.3): mc2_render on CPU0 (the control, into a descriptor), mc2_voice the sample loop from the
 * descriptor alone, on either core. The models that draw plaits' shared generator (pl_rng_state) render at once on CPU0, in voice order */
typedef struct {
    mc2_part_t *s;
    vmod_t m;
    uint32_t vi, n;
    uint8_t gate, lpg;
    float note, timbre, morph, harm, mixf, colour, tmod, decay, g_out, g_aux;
} mc2_job_t;
_Static_assert(sizeof(mc2_job_t) <= AUDIO_BATCH_BYTES, "MACRO2's batch descriptor");
static const uint8_t MC2_UNITS[MC2_N];
static inline int mc2_draws(uint32_t model)           /* plaits' shared generator in the render: CHIP's arpeggiator (random mode), SPEECH's LPC noise, 6OP's LFO
                                                       * (its sample-and-hold waveform); not VA-VCF, PD, TERRAN, STRMCH, WAVTBL */
{
    return model == MC2_CHIP || model == MC2_SPEECH || (model >= MC2_6OP_A && model <= MC2_6OP_C);
}
static MC2_XIP void mc2_voice(const void *data, int32_t *out)
{
    const mc2_job_t *j = data;
    mc2_part_t *s = j->s;
    mc2_vhead_t *h = &s->h[j->vi % MC2_POLY];
    const vmod_t *m = &j->m;
    float o[MC2_SUB], a[MC2_SUB], note = j->note, decay = j->decay, mixf = j->mixf, colour = j->colour;
    float g_out = j->g_out, g_aux = j->g_aux;
    uint32_t done = 0, i, n = j->n;
    pl_params_t q;
    q.harmonics = j->harm;
    q.accent = h->accent;
    while (done < n) {
        uint32_t k = n - done < MC2_SUB ? n - done : MC2_SUB;
        int env;
        q.trigger = (h->rise ? PL_TRIGGER_RISING_EDGE : 0) | (j->gate ? PL_TRIGGER_HIGH : 0);
        h->rise = 0;
        q.note = (note + h->note) * 0.5f;
        h->note = note;
        h->decay_env *= 1.0f - decay * 2.0f;
        q.timbre = j->timbre + j->tmod * h->decay_env;
        PL_CONSTRAIN(q.timbre, 0.0f, 1.0f);
        q.morph = j->morph;
        PL_CONSTRAIN(q.morph, 0.0f, 1.0f);
        env = mc2_model_render(s, j->vi, &q, k, o, a);
        if (g_out < 0.0f) pl_limiter_process(&h->limit_out, -g_out, o, k);
        if (g_aux < 0.0f) pl_limiter_process(&h->limit_aux, -g_aux, a, k);
        {
            float go = g_out < 0.0f ? 1.0f : g_out, ga = g_aux < 0.0f ? 1.0f : g_aux;
            for (i = 0; i < k; i++) o[i] = o[i] * go + (a[i] * ga - o[i] * go) * mixf;
        }
        if (j->lpg && !env) {                         /* (envelope.h LPGEnvelope: the ADSR as the vactrol's level; not for a model's own envelope) */
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
                float lp = pl_svf_process(&h->lpg, PL_LP, o[i]);
                o[i] = lp + (o[i] - lp) * bleed;
            }
        }
        for (i = 0; i < k; i++) {
            float x = o[i];
            int32_t y;
            x = x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x;
            y = (int32_t)(x * 32767.0f);
            out[done + i] += voice_amp(y * 2, m, done + i) * 2;
        }
        done += k;
    }
}

static MC2_XIP void mc2_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    mc2_part_t *s = mc2_part((uint32_t)(t - trk));
    const int16_t *p = t->p;
    uint32_t model = (uint32_t)clamp(p[P_E0], 0, MC2_N - 1), vi = (uint32_t)(v - t->v) % MC2_POLY;
    mc2_job_t j;
    if (!s || !OC_PACK_READY()) return;               /* (the pack holds pl_lut_sine and the pitch tables: silent without it, as MACRO) */
    if (s->model != model)
        mc2_start(s, model);
    if (mc2_big(model) && vi)
        return;
    j.note = (float)m->pitch16 * (1.0f / 16.0f) + (float)m->fine * 0.00422689f
             + (float)tuning_log() * (12.0f / 16777216.0f);
    j.timbre = (float)p[P_E2] * (1.0f / 127.0f) + (float)(m->shape - (64 << 8)) * (1.0f / 32768.0f);
    j.morph = (float)p[P_E3] * (1.0f / 127.0f) + (float)m->cutoff * (1.0f / 32512.0f);
    j.harm = (float)p[P_E1] * (1.0f / 127.0f);
    j.mixf = (float)p[P_E4] * (1.0f / 127.0f);
    j.colour = (float)p[P_E5] * (1.0f / 127.0f);
    j.tmod = mc2_amount(p[P_E7]);
    j.decay = (200.0f * (float)MC2_SUB) / PL_SAMPLE_RATE * pl_semitones_to_ratio(-96.0f * (float)p[P_E6] * (1.0f / 127.0f));
    j.g_out = MC2_POST[model].out;
    j.g_aux = MC2_POST[model].aux;
    j.lpg = (uint8_t)(p[P_E5] > 0 && !MC2_POST[model].enveloped);
    PL_CONSTRAIN(j.harm, 0.0f, 1.0f);
    if (s->h[vi].note == 0.0f) s->h[vi].note = j.note;
    j.s = s; j.m = *m; j.vi = vi; j.n = n; j.gate = (uint8_t)(v->gate != 0);
    if (audio_batch_active && !mc2_draws(model)) {   /* (the module's push has no cost argument: the batch charges AP_MACRO's one-unit cost) */
        audio_batch_push(mc2_voice, &j, sizeof j, out, n, AP_MACRO);
        return;
    }
    mc2_voice(&j, out);
}

/* the voice budget's units a voice takes: from the host's cost of each model's render beside MACRO's (tests/plaits_parity/bench.sh: VA 74 ns a sample,
 * FM 167, ADDTV 200, whose unit-measured units are 4, 8 and 9: about one unit per 20 host ns), rounded; 6OP one more than its host cost says
 * (its loop is the module's heaviest by the gate's count: 1,837, the Prophet's 1,075) and WAVTBL one more for its wave reads from flash. The unit's
 * own costs replace them when it has measured them (docs/candidates/macro2.md, section 7). VA-VCF 5, PD 5, 6OP 6, TERRAN 7, STRMCH 5, CHIP 2, WAVTBL 8,
 * SPEECH 5 */
static const uint8_t MC2_UNITS[MC2_N] = {MC2_UNITS_TABLE};
static MC2_XIP uint32_t mc2_units(const track_t *t)
{
    return MC2_UNITS[clamp(t->p[P_E0], 0, MC2_N - 1)];
}

/* name, {MODEL, HARM, TIMBRE, MORPH, AUX, LPG, DECAY, T.MOD}, {A D S R}, ENV -> FILTER (MORPH), mono */
static const preset_t MC2_PRESETS[] = {
    {"VCF BASS", {MC2_VAVCF, 30, 40, 50, 0, 0, 50, 20}, {0, 60, 90, 20}, 20, 1},
    {"VCF BRASS", {MC2_VAVCF, 70, 60, 90, 0, 0, 70, 0}, {10, 80, 110, 50}, 30, 0},
    {"PD KEYS", {MC2_PD, 40, 60, 30, 0, 0, 60, 30}, {0, 80, 60, 50}, 0, 0},
    {"PD RESO LEAD", {MC2_PD, 90, 80, 70, 0, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"DX EPIANO", {MC2_6OP_A, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK A 11", {MC2_6OP_A, 42, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK B 1", {MC2_6OP_B, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK C 1", {MC2_6OP_C, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"TERRAIN DRONE", {MC2_TERRAN, 50, 60, 40, 0, 0, 90, 0}, {40, 90, 110, 80}, 0, 1},
    {"TERRAIN PLUCK", {MC2_TERRAN, 90, 90, 70, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"STRING ENSEMBLE", {MC2_STRMCH, 36, 64, 90, 0, 0, 90, 0}, {40, 90, 110, 90}, 0, 1},
    {"STRING ORGAN", {MC2_STRMCH, 70, 30, 20, 0, 0, 70, 0}, {5, 80, 120, 30}, 0, 1},
    {"CHIP ARP", {MC2_CHIP, 36, 100, 64, 0, 0, 50, 0}, {0, 100, 100, 20}, 0, 1},
    {"CHIP LEAD", {MC2_CHIP, 0, 20, 90, 100, 0, 60, 0}, {0, 70, 110, 30}, 0, 1},
    {"WAVETABLE PAD", {MC2_WAVTBL, 20, 50, 70, 0, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"WAVETABLE KEYS", {MC2_WAVTBL, 64, 30, 40, 0, 80, 50, 40}, {0, 70, 60, 50}, 0, 0},
    {"WAVETABLE 8BIT", {MC2_WAVTBL, 110, 64, 64, 127, 0, 60, 0}, {0, 70, 110, 40}, 0, 1},
    {"VOWEL", {MC2_SPEECH, 10, 64, 64, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"SAM VOICE", {MC2_SPEECH, 30, 64, 40, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"LPC WORDS", {MC2_SPEECH, 70, 64, 30, 0, 0, 70, 0}, {0, 127, 127, 60}, 0, 1},
};
_Static_assert(NELEM(MC2_PRESETS) <= 64u, "presets");

static const engine_t ENG_MACRO2 = {
    .name = "MACRO2",
    .page_title = {"MODEL", "SHAPE"},
    .edit = {
        {"MODL", F_ENUM, 0, MC2_N - 1, 0, N_MC2_MODEL, 0},
        {"HARM", F_PCT, 0, 127, 64, 0, 0},
        {"TIMB", F_PCT, 0, 127, 64, 0, 0},
        {"MRPH", F_PCT, 0, 127, 64, 0, 0},
        {"AUX", F_PCT, 0, 127, 0, 0, 0},
        {"LPG", F_PCT, 0, 127, 0, 0, 0},
        {"DECY", F_PCT, 0, 127, 64, 0, 0},
        {"TMOD", F_BIPCT, -64, 63, 0, 0, 0},
    },
    .presets = MC2_PRESETS,
    .npresets = NELEM(MC2_PRESETS),
    .poly = MC2_POLY,
    .cap = mc2_cap,
    .note_on = mc2_note_on,
    .render = mc2_render,
    .units = mc2_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
#if MACRO2_HOT && !OC_MODULE
    .hot = MC2_HOT, .nhot = MC2_HOT_N, .hot_at = mc2_hot,
#elif MACRO2_HOT
    .hot = MC2_HOT, .nhot = MC2_HOT_N,
#endif
};
