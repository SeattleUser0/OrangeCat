/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.5.0: MACRO, engine 8 (the slot of Melodee's retired GRAIN). Sound models in one engine, each shaped by three
 * knobs (HARM, TIMBRE, MORPH), ported from the open source code of a well known Eurorack macro oscillator (firmware/src/plaits/,
 * MIT: LICENSES/MIT-Mutable.txt). The models themselves are line for line ports (tests/plaits_parity checks them sample for
 * sample against the original); this file is the glue to Melodee's voices.
 *
 * (Release 3, the model picker: docs/candidates/model_picker.md) The glue dispatches through a table of model descriptors
 * (plaits/pl_mdl.h, the list plaits/pl_models.h: 25 models, stable ids). Built into the firmware: VA, the fallback sound (and, on the
 * host, every model: MELODEE_MACRO_PICK). Every other model is a unit of the MODELS module the installer page assembles from the
 * models the user picked (tools/module/plaits/, fw_models.h): the loader hands this file the module's table (mc_ext_set, through
 * engines.c eng_models_set) and a model whose descriptor is null, in both tables, is not installed: the MODL knob steps over it and its
 * value reads MISSG (params.c), a track that names it keeps every setting, plays VA and the screen says MODULE MISSING (ui.c). A project
 * stores the id in P_E0 as always; a preset's e[0] is the id.
 *
 * The edit pages:
 *   EDIT 1 MODEL: MODL (the model), HARM, TIMB (TIMBRE), MRPH (MORPH). HOME's four knobs are these too.
 *   EDIT 2 SHAPE: AUX (blend to the model's second output: 0 the main one, 127 the other), LPG (a low-pass gate: the
 *                 ADSR also closes a filter, 0 = off, 127 = darkest), DECY (an envelope of its own, started by each
 *                 note), TMOD (how much that envelope moves TIMBRE, either way). (Labels are four letters at most:
 *                 the screen's cells.)
 * The models: VA (two analog style oscillators), WAVSHP (waveshaper and folder), FM (two operators), GRAIN (formant
 * grains), ADDTV (additive), CHORD (four voice chords), SWARM (eight sawtooths), NOISE (filtered noise), PARTCL (dust
 * through filters and a diffuser), STRING (plucked strings), MODAL (struck resonators), KICK, SNARE, HI-HAT, (0.5.2, Orange
 * Cat's own) HARP: STRING's string, one a voice, so its chords ring; and (Release 3) VA-VCF, PD (phase distortion), 6OP A, B, C
 * (six-operator FM with three DX7 banks), TERRAN (wave terrain), STRMCH (string machine), CHIP (chiptune), WAVTBL (wavetable, with
 * the wave banks A, B, C) and SPEECH.
 *
 * Voices: four per track in POLY (and UNISON), each a model of its own (0.5.2), for chords; a large model (the descriptor's big:
 * PARTCL, STRING, 6OP, STRMCH, SPEECH, states of 2.6 to 8 KB) plays one at a time. The voices' states are the part's (pl_part_t, from
 * the audio arena: the voice heads, then the states laid out by the installed model's descriptor; the block is PL_STATE_MAX, enough for
 * the largest model). A note starts its voice's trigger; the key held keeps it high. Pitch comes from the voice (glide, bend, TUNE, A4
 * and the pitch modulations: pitch16 and fine). The voice's ADSR is the VCA as for every engine; for STRING, MODAL and the drums
 * (models with their own decay) the presets hold the ADSR open and let the model decay. ENV -> SHAPE and LFO -> SHAPE move TIMBRE;
 * ENV -> FILTER and LFO -> FILTER move MORPH; velocity is the models' accent.
 *
 * Where it runs: the models are too big for the SRAM the audio code lives in, so the engine's callbacks and everything they call stay
 * in flash (section .text.xip_macro: tools/audio_ram.py; a module's models run from the module area). They render in 16-frame pieces
 * (the models' parameters glide over each piece, as the module's 12-frame blocks at 48 kHz). Sample rate: 44.1 kHz. Pitch is exact;
 * the models' own times (decays) are 8.8 % longer than on the module.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.5.0: first version.
 *   0.5.2: polyphonic: four voices (MC_POLY) but in PARTCL and STRING (mc_cap); a voice's state apart (mc_vhead_t, the
 *          model's in mc_part_t.m.small[voice]); the pad, keys, chord and bell presets play POLY. HARP (a 15th model,
 *          one string a voice) with presets HARP and STRING CHORDS. Voice units by model from measured costs (MC_UNITS):
 *          0.5.2's flat 3 let a four note ORGAN DRAWBAR chord overload the unit.
 *   0.8.0: mc_render is silent while the asset pack (Plaits' tables, oc_pack.h) is not there (OC_PACK_READY).
 *   round 18: a fresh part starts the model it names (the part's `started` byte; VA's states were never started before).
 *   Release 3 (the model picker): the model switch is a table of descriptors; the models beyond VA come from the MODELS module; the
 *          ten further Plaits models and their presets; the units from the unit's measured costs (pl_models.h).
 */
#include "plaits/pl_tables.h"
/* (Orange Cat 0.13.8) MACRO's hot set (core.h engine_t hot): the tables its models read every sample (tools/pack_hot.py), copied into the arena at boot
 * (engines.c eng_hot_take). The names below are then the copies (ENG_HOT); the waveshaper's curves through PL_WS_CURVE (pl_waveshaping.h). The MODELS
 * module reads the same copies through the services table (fw_api_t hot[ENGI_MACRO]: engines.c keeps a built-in engine's pointers there too) */
#define MC_HOT_LIST(X) X(pl_lut_pitch_ratio_high, PL_LUT_PITCH_RATIO_HIGH_SIZE * 4u, float) X(pl_lut_pitch_ratio_low, PL_LUT_PITCH_RATIO_LOW_SIZE * 4u, float) \
    X(pl_lut_sine, PL_LUT_SINE_SIZE * 4u, float) X(pl_lut_fold, PL_LUT_FOLD_SIZE * 4u, float) X(pl_lut_fold_2, PL_LUT_FOLD_2_SIZE * 4u, float) \
    X(pl_lut_4x_downsampler_fir, PL_LUT_4X_DOWNSAMPLER_FIR_SIZE * 4u, float) X(pl_chord_waves, 15u * 132u * 2u, int16_t) \
    X(pl_lut_ws_inverse_tan, PL_LUT_WS_INVERSE_TAN_SIZE * 2u, int16_t) X(pl_lut_ws_inverse_sin, PL_LUT_WS_INVERSE_SIN_SIZE * 2u, int16_t) \
    X(pl_lut_ws_linear, PL_LUT_WS_LINEAR_SIZE * 2u, int16_t) X(pl_lut_ws_bump, PL_LUT_WS_BUMP_SIZE * 2u, int16_t) \
    X(pl_lut_ws_double_bump, PL_LUT_WS_DOUBLE_BUMP_SIZE * 2u, int16_t)
#define MC_HOT_ENTRY(t, b, ty) {t, b},
#define MC_HOT_SRC(t, b, ty) t,
#define MC_HOT_IDX(t, b, ty) MCH_##t,
enum { MC_HOT_LIST(MC_HOT_IDX) MC_HOT_N };
static const eng_hot_t MC_HOT[MC_HOT_N] = { MC_HOT_LIST(MC_HOT_ENTRY) };
static const void *mc_hot[MC_HOT_N] = { MC_HOT_LIST(MC_HOT_SRC) };
#undef pl_lut_pitch_ratio_high
#undef pl_lut_pitch_ratio_low
#undef pl_lut_sine
#undef pl_lut_fold
#undef pl_lut_fold_2
#undef pl_lut_4x_downsampler_fir
#undef pl_chord_waves
#define pl_lut_pitch_ratio_high ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_pitch_ratio_high))
#define pl_lut_pitch_ratio_low ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_pitch_ratio_low))
#define pl_lut_sine ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_sine))
#define pl_lut_fold ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_fold))
#define pl_lut_fold_2 ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_fold_2))
#define pl_lut_4x_downsampler_fir ((const float *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_4x_downsampler_fir))
#define pl_chord_waves ((const int16_t *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_chord_waves))
#define PL_WS_CURVE(k) ((const int16_t *)ENG_HOT(mc_hot, ENGI_MACRO, MCH_pl_lut_ws_inverse_tan + ((k) > 4 ? 4 : (k))))
#include "plaits/pl_dsp.h"
#include "plaits/pl_dsp2.h"
#include "plaits/pl_osc.h"

#define MC_XIP __attribute__((section(".text.xip_macro"), noinline))
#define MC_SUB 16u                                    /* frames per model render (<= PL_MAX_BLOCK) */
#define MC_POLY PL_POLY                               /* (0.5.2) voices per track; a large model (pl_mdl_t big): 1 */

/* Which models this firmware carries (a bit an id, 0..24; 25..27 the wave banks): on the device VA alone (MELODEE_MACRO_PICK, the CI build
 * set: tools/orangecat_engines.sh), the rest picked on the installer page; on the host every model, so the suite's tests see MACRO whole
 * (tests/plaits_test.c builds the device's way, with the MODELS module registered, to prove that path) */
#ifndef MELODEE_MACRO_PICK
#define MELODEE_MACRO_PICK 0
#endif
#ifndef MC_BUILTIN
#define MC_BUILTIN (MELODEE_MACRO_PICK ? 0x1u : 0x0FFFFFFFu)
#endif
#define MC_HAS(id) ((MC_BUILTIN >> (id)) & 1u)
#define PL_WANT_va MC_HAS(0)
#define PL_WANT_wavshp MC_HAS(1)
#define PL_WANT_fm MC_HAS(2)
#define PL_WANT_grain MC_HAS(3)
#define PL_WANT_addtv MC_HAS(4)
#define PL_WANT_chord MC_HAS(5)
#define PL_WANT_swarm MC_HAS(6)
#define PL_WANT_noise MC_HAS(7)
#define PL_WANT_partcl MC_HAS(8)
#define PL_WANT_string MC_HAS(9)
#define PL_WANT_modal MC_HAS(10)
#define PL_WANT_kick MC_HAS(11)
#define PL_WANT_snare MC_HAS(12)
#define PL_WANT_hihat MC_HAS(13)
#define PL_WANT_harp MC_HAS(14)
#define PL_WANT_vavcf MC_HAS(15)
#define PL_WANT_pd MC_HAS(16)
#define PL_WANT_syx0 MC_HAS(17)
#define PL_WANT_syx1 MC_HAS(18)
#define PL_WANT_syx2 MC_HAS(19)
#define PL_WANT_sixop (PL_WANT_syx0 || PL_WANT_syx1 || PL_WANT_syx2)
#define PL_WANT_terran MC_HAS(20)
#define PL_WANT_strmch MC_HAS(21)
#define PL_WANT_chip MC_HAS(22)
#define PL_WANT_wavtbl MC_HAS(23)
#define PL_WANT_speech MC_HAS(24)
#define MC_BANKS_BUILTIN (MC_HAS(25) || MC_HAS(26) || MC_HAS(27))
#if PL_WANT_wavtbl || PL_WANT_terran || PL_WANT_sixop || PL_WANT_speech
#include "plaits/pl_tables2.h"                        /* the further models' own tables, the wave and DX7 banks (plain arrays: the image's, not the pack's) */
#endif
#define PL_MDL_LINKAGE static
#define PL_MDL_ATTR MC_XIP
#include "plaits/pl_mdl.h"
#include "plaits/pl_models.h"
static const pl_mdl_t *const MC_BUILTIN_TAB[PL_N] = { PL_MODEL_LIST(PL_MODEL_PTR) };
static const char *const N_MC_MODEL[PL_N] = { PL_MODEL_LIST(PL_MODEL_NAME) };
#define MC_N PL_N
enum { MC_VA, MC_WAVSHP, MC_FM, MC_GRAIN, MC_ADDTV, MC_CHORD, MC_SWARM, MC_NOISE, MC_PARTCL, MC_STRING, MC_MODAL,
       MC_KICK, MC_SNARE, MC_HIHAT, MC_HARP };         /* MACRO's own numbers (0.5.2: HARP at the end, so stored sounds keep theirs): voice.c's cost caps, the tests */
#define MC_STATE_BYTES PL_STATE_MAX                   /* a part's arena block (engines.c mc_part, eng_state_size): enough for any model */
typedef pl_part_t mc_part_t;
typedef pl_vhead_t mc_vhead_t;
static mc_part_t *mc_part(uint32_t part);             /* engines.c */

/* the MODELS module's tables, while one is registered (engines.c eng_models_set from the loader): n descriptor pointers by model id (null: the pick
 * left the model out) and the wave banks present. Set and cleared with the interrupts off (oc_module.c), so a render sees one table or the other */
static const pl_mdl_t *const *mc_ext_models;
static uint32_t mc_ext_n;
static const void *const *mc_ext_banks;
static void mc_ext_set(const pl_mdl_t *const *models, uint32_t n, const void *const *banks)
{
    mc_ext_models = models; mc_ext_n = models ? n : 0u; mc_ext_banks = banks;
}
static inline int mc_banks_any(void)
{
    return MC_BANKS_BUILTIN || (mc_ext_banks && (mc_ext_banks[0] || mc_ext_banks[1] || mc_ext_banks[2]));
}
/* the model, if it is installed: its descriptor (built in, or the module's; WAVTBL: a wave bank too), else 0 */
static inline const pl_mdl_t *mc_model(uint32_t model)
{
    const pl_mdl_t *d = model < PL_N ? MC_BUILTIN_TAB[model] : 0;
    if (!d && model < mc_ext_n) d = mc_ext_models[model];
    if (model == PL_WAVTBL && !mc_banks_any()) return 0;
    return d;                                         /* (a module's descriptor: the loader checked its start and render are inside the module's code) */
}
/* the model a track plays: the one it names, or VA (always built in) when that one is not installed (the settings stay; ui.c says MODULE MISSING) */
static inline const pl_mdl_t *mc_playing(uint32_t *model)
{
    const pl_mdl_t *d = mc_model(*model);
    if (!d) { *model = PL_VA; d = mc_model(PL_VA); }
    return d;
}
static inline uint32_t mc_want(const track_t *t) { return (uint32_t)clamp(t->p[P_E0], 0, MC_N - 1); }
/* (the model picker) EDIT 1's MODL offers the installed models only: the knob steps over the others, which show as MISSG (params.c param_offered) */
static int mc_offered(int32_t v) { return v >= 0 && mc_model((uint32_t)v) != 0; }
static int mc_track_missing(const track_t *t) { return mc_model(mc_want(t)) == 0; }   /* the track names a model that is not installed (ui.c MODULE MISSING) */
static int mc_track_ext(const track_t *t)             /* the track plays a model the MODELS module supplies (oc_module.c: the crash mark) */
{
    uint32_t model = mc_want(t);
    return mc_model(model) != 0 && MC_BUILTIN_TAB[model] == 0;
}

static MC_XIP uint32_t mc_cap(const track_t *t)
{
    uint32_t model = mc_want(t);
    const pl_mdl_t *d = mc_playing(&model);
    return d && d->big ? 1u : MC_POLY;
}

/* a model's states from scratch (the model changed, or a fresh part): every voice's */
static MC_XIP void mc_start(mc_part_t *s, uint32_t model, const pl_mdl_t *d)
{
    uint32_t i, nv = d->big ? 1u : MC_POLY;
    s->model = (uint8_t)model;
    s->started = 1;
    for (i = 0; i < nv; i++) {
        mc_vhead_t *h = &s->h[i];
        memset(pl_state(s, d, i), 0, pl_stride(d->state));   /* (from zero, as the arena hands a fresh part its memory: a model's init does not write every field) */
        d->start(pl_state(s, d, i));
        pl_limiter_init(&h->limit_out);
        pl_limiter_init(&h->limit_aux);
        pl_svf_init(&h->lpg);
        h->note = 0.0f;                               /* (the note's rise and DECAY envelope stay: a note may have just started) */
    }
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

/* (Orange Cat 0.14.3) a voice's block in two parts (audio_batch.h): mc_render, on CPU0 at the voice's turn, the control (the model, which starts
 * every voice's state; the knobs and the modulation read from t->p, which is only modulated until the track's mod_end) into a descriptor;
 * mc_voice, the sample loop, from the descriptor alone: on CPU1 or CPU0 as the batch splits the block's voices, or at once (no batch; the
 * models that draw plaits' shared generator, pl_rng_state: their draws stay in voice order on CPU0, bit for bit). It touches its voice's
 * state (h[vi], the model's) and read-only tables only */
typedef struct {
    mc_part_t *s;
    const pl_mdl_t *d;
    vmod_t m;
    uint32_t vi, n;
    uint8_t gate, lpg;
    float note, timbre, morph, harm, mixf, colour, tmod, decay, g_out, g_aux;
} mc_job_t;
_Static_assert(sizeof(mc_job_t) <= AUDIO_BATCH_BYTES, "MACRO's batch descriptor");
static MC_XIP void mc_voice(const void *data, int32_t *out)
{
    const mc_job_t *j = data;
    mc_part_t *s = j->s;
    const pl_mdl_t *d = j->d;
    mc_vhead_t *h = &s->h[j->vi % MC_POLY];
    void *ms = pl_state(s, d, j->vi);
    const vmod_t *m = &j->m;
    float o[MC_SUB], a[MC_SUB], note = j->note, decay = j->decay, mixf = j->mixf, colour = j->colour;
    float g_out = j->g_out, g_aux = j->g_aux;
    uint32_t done = 0, i, n = j->n;
    pl_params_t q;
    q.harmonics = j->harm;
    q.accent = h->accent;
    while (done < n) {
        uint32_t k = n - done < MC_SUB ? n - done : MC_SUB;
        int env;
        q.trigger = (h->rise ? PL_TRIGGER_RISING_EDGE : 0) | (j->gate ? PL_TRIGGER_HIGH : 0);
        h->rise = 0;
        q.note = (note + h->note) * 0.5f;             /* (voice.cc: the module averages the last two) */
        h->note = note;
        h->decay_env *= 1.0f - decay * 2.0f;
        q.timbre = j->timbre + j->tmod * h->decay_env;
        PL_CONSTRAIN(q.timbre, 0.0f, 1.0f);
        q.morph = j->morph;
        PL_CONSTRAIN(q.morph, 0.0f, 1.0f);
        env = d->render(ms, &q, o, a, k);             /* (the model's render, then its flushes: tiny decaying states to 0, pl_dsp.h pl_flush) */
        pl_flush_svf(&h->lpg);
        pl_flush(&h->decay_env);
        if (g_out < 0.0f) pl_limiter_process(&h->limit_out, -g_out, o, k);
        if (g_aux < 0.0f) pl_limiter_process(&h->limit_aux, -g_aux, a, k);
        {
            float go = g_out < 0.0f ? 1.0f : g_out, ga = g_aux < 0.0f ? 1.0f : g_aux;
            for (i = 0; i < k; i++) o[i] = o[i] * go + (a[i] * ga - o[i] * go) * mixf;
        }
        if (j->lpg && !env) {                         /* (envelope.h LPGEnvelope: the ADSR as the vactrol's level; not when the model made its own this block) */
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

static MC_XIP void mc_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    mc_part_t *s = mc_part((uint32_t)(t - trk));
    const int16_t *p = t->p;
    uint32_t model = mc_want(t), vi = (uint32_t)(v - t->v) % MC_POLY;
    const pl_mdl_t *d = mc_playing(&model);
    mc_job_t j;
    if (!s || !d || !OC_PACK_READY()) return;         /* (Orange Cat 0.8.0: the asset pack, with Plaits' tables, is not there: silent) */
    if (s->model != model || !s->started)
        mc_start(s, model, d);
    if (d->big && vi)                                 /* (voices left from a model of four: silent to their end) */
        return;
    /* the voice's pitch as a MIDI note: 1/16 semitones, the residual (1/4096 of the frequency: 12 / (4096 ln 2)
     * semitones each), the A4 tuning (Q24 octaves) */
    j.note = (float)m->pitch16 * (1.0f / 16.0f) + (float)m->fine * 0.00422689f
             + (float)tuning_log() * (12.0f / 16777216.0f);
    j.timbre = (float)p[P_E2] * (1.0f / 127.0f) + (float)(m->shape - (64 << 8)) * (1.0f / 32768.0f);
    j.morph = (float)p[P_E3] * (1.0f / 127.0f) + (float)m->cutoff * (1.0f / 32512.0f);
    j.harm = (float)p[P_E1] * (1.0f / 127.0f);
    j.mixf = (float)p[P_E4] * (1.0f / 127.0f);
    j.colour = (float)p[P_E5] * (1.0f / 127.0f);
    j.tmod = mc_amount(p[P_E7]);
    /* DECAY: 0 about 10 ms .. 127 about 6 s, as the module's (voice.cc short_decay, per 16 frames) */
    j.decay = (200.0f * (float)MC_SUB) / PL_SAMPLE_RATE * pl_semitones_to_ratio(-96.0f * (float)p[P_E6] * (1.0f / 127.0f));
    j.g_out = d->out;
    j.g_aux = d->aux;
    j.lpg = (uint8_t)(p[P_E5] > 0 && !d->enveloped);
    PL_CONSTRAIN(j.harm, 0.0f, 1.0f);
    if (s->h[vi].note == 0.0f) s->h[vi].note = j.note;
    j.s = s; j.d = d; j.m = *m; j.vi = vi; j.n = n; j.gate = (uint8_t)(v->gate != 0);
    if (audio_batch_active && !d->draws) {
        audio_batch_push_cost(mc_voice, &j, sizeof j, out, n, AP_MACRO, d->units * audio_batch_cost[AP_MACRO], 0);
        return;
    }
    mc_voice(&j, out);
}

/* the voice budget's units a voice takes (voice.c): by its model's cost (pl_models.h: the rule and the unit's numbers) */
static MC_XIP uint32_t mc_units(const track_t *t)
{
    uint32_t model = mc_want(t);
    const pl_mdl_t *d = mc_playing(&model);
    return d ? d->units : 1u;
}

/* (0.5.5) The string presets (PLUCKED STRING, HARP, STRING CHORDS) at HARM 32: the string's non-linearity is zero
 * there (0.24 to 0.26), its partials whole multiples, a string. At 64 (0.5.0 to 0.5.4) it was the module's dispersion,
 * its partials stretched (2.92, 3.78, 5.46 times the note): a wooden bar, as heard on the unit. HARP's MRPH 100: it
 * rings on.
 * name, {MODEL, HARM, TIMBRE, MORPH, AUX, LPG, DECAY, T.MOD}, {A D S R}, ENV -> FILTER (MORPH), mono (0.5.2: 0, POLY,
 * for the pads, keys and bells: they play a step's four notes; 0.5.3: the CHORD presets LEG again, the model makes its
 * own four note chord from one key, and four of those would take 20 voice units). (Release 3) the ten further models' presets
 * after MACRO's 27 (macro2.md): a preset's model may be one the unit has not installed: it loads, shows MISSG and plays VA. */
static const preset_t MC_PRESETS[] = {
    {"VA BASS", {0, 64, 30, 70, 0, 40, 50, 20}, {0, 60, 90, 20}, 10, 1},
    {"VA SYNC LEAD", {0, 76, 90, 40, 100, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"FOLD PLUCK", {1, 40, 40, 80, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"FOLD LEAD", {1, 90, 70, 60, 40, 0, 70, 0}, {2, 70, 100, 40}, 0, 1},
    {"FM BELL", {2, 90, 60, 30, 0, 0, 80, 30}, {0, 96, 0, 90}, 0, 0},
    {"FM BASS", {2, 30, 70, 50, 0, 50, 45, 40}, {0, 60, 70, 20}, 0, 1},
    {"FORMANT VOX", {3, 50, 60, 64, 0, 0, 70, 0}, {20, 80, 110, 60}, 0, 1},
    {"GRAIN SWEEP", {3, 80, 90, 100, 60, 0, 90, -30}, {10, 90, 100, 70}, 0, 1},
    {"ORGAN DRAWBAR", {4, 40, 50, 20, 0, 0, 70, 0}, {2, 60, 120, 30}, 0, 0},
    {"ADDITIVE PAD", {4, 90, 64, 90, 64, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"CHORD MINOR", {5, 36, 40, 30, 0, 0, 70, 0}, {20, 80, 110, 70}, 0, 1},
    {"CHORD STAB", {5, 70, 64, 60, 0, 80, 40, 0}, {0, 60, 0, 50}, 0, 1},
    {"SWARM PAD", {6, 50, 40, 70, 0, 0, 90, 0}, {70, 90, 110, 90}, 0, 0},
    {"NOISE WIND", {7, 70, 50, 90, 0, 0, 90, 0}, {80, 90, 100, 90}, 0, 1},
    {"NOISE SWEEP", {7, 40, 90, 30, 64, 0, 80, -40}, {10, 80, 100, 70}, 0, 1},
    {"DUST RAIN", {8, 64, 70, 80, 0, 0, 90, 0}, {10, 90, 110, 90}, 0, 1},
    {"PLUCKED STRING", {9, 32, 70, 64, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 1},
    {"LOW STRING", {9, 40, 40, 110, 0, 0, 80, 0}, {0, 127, 127, 96}, 0, 1},
    {"MALLET", {10, 50, 50, 70, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},
    {"GLASS BELL", {10, 100, 90, 100, 0, 0, 60, 0}, {0, 127, 127, 104}, 0, 0},
    {"KICK DEEP", {11, 30, 40, 80, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"KICK 909", {11, 90, 64, 60, 127, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"SNARE", {12, 50, 64, 64, 0, 0, 50, 0}, {0, 127, 127, 90}, 0, 1},
    {"HI-HAT", {13, 64, 90, 30, 0, 0, 50, 0}, {0, 127, 127, 80}, 0, 1},
    {"OPEN HAT", {13, 64, 90, 90, 0, 0, 50, 0}, {0, 127, 127, 96}, 0, 1},
    {"HARP", {14, 32, 70, 100, 0, 0, 60, 0}, {0, 127, 127, 100}, 0, 0},         /* (0.5.3; 0.5.5: HARM 32, MRPH 100) */
    {"STRING CHORDS", {14, 32, 50, 110, 0, 0, 80, 0}, {0, 127, 127, 104}, 0, 0},
    {"VCF BASS", {15, 30, 40, 50, 0, 0, 50, 20}, {0, 60, 90, 20}, 20, 1},       /* (Release 3) the further models' presets, 27 on */
    {"VCF BRASS", {15, 50, 64, 20, 0, 0, 70, 0}, {10, 80, 110, 50}, 30, 0},   /* (picker-r3: HARM 50, TIMB 64, MRPH 20; macro2.md's 70 / 60 / 90 carried a DC offset past the regression's health limit at the low notes) */
    {"PD KEYS", {16, 40, 60, 30, 0, 0, 60, 30}, {0, 80, 60, 50}, 0, 0},
    {"PD RESO LEAD", {16, 90, 80, 70, 0, 0, 70, 0}, {0, 70, 110, 40}, 0, 1},
    {"DX EPIANO", {17, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK A 11", {17, 42, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK B 1", {18, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"DX BANK C 1", {19, 0, 64, 64, 0, 0, 64, 0}, {0, 127, 127, 60}, 0, 1},
    {"TERRAIN DRONE", {20, 50, 60, 40, 0, 0, 90, 0}, {40, 90, 110, 80}, 0, 1},
    {"TERRAIN PLUCK", {20, 90, 90, 70, 0, 90, 40, 40}, {0, 64, 0, 50}, 0, 0},
    {"STRING ENSEMBLE", {21, 36, 64, 90, 0, 0, 90, 0}, {40, 90, 110, 90}, 0, 1},
    {"STRING ORGAN", {21, 70, 30, 20, 0, 0, 70, 0}, {5, 80, 120, 30}, 0, 1},
    {"CHIP ARP", {22, 36, 100, 64, 0, 0, 50, 0}, {0, 100, 100, 20}, 0, 1},
    {"CHIP LEAD", {22, 0, 20, 90, 100, 0, 60, 0}, {0, 70, 110, 30}, 0, 1},
    {"WAVETABLE PAD", {23, 20, 50, 70, 0, 0, 100, 20}, {60, 90, 110, 90}, 0, 0},
    {"WAVETABLE KEYS", {23, 64, 30, 40, 0, 80, 50, 40}, {0, 70, 60, 50}, 0, 0},
    {"WAVETABLE 8BIT", {23, 110, 64, 64, 127, 0, 60, 0}, {0, 70, 110, 40}, 0, 1},
    {"VOWEL", {24, 10, 64, 64, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"SAM VOICE", {24, 30, 64, 40, 0, 0, 70, 0}, {10, 80, 110, 60}, 0, 1},
    {"LPC WORDS", {24, 70, 64, 30, 0, 0, 70, 0}, {0, 127, 127, 60}, 0, 1},
};
#define MC_PRESETS_MACRO 27u                          /* MACRO's own, 0.5.x: what golden.txt's preset/MACRO lines 0..26 are */

/* MACRO's stars are favorites.c row 8 bytes 0..7: bytes 8..15 hold GLO > WEIGHTS and 16..31 the FX key map (settings_persist.c, favorites.c) */
_Static_assert(NELEM(MC_PRESETS) <= 64u, "MACRO's favorites stay in row 8 bytes 0..7, below the WEIGHTS and FX bytes");

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
    .cap = mc_cap,                                     /* (0.5.2) the large models: 1 */
    .note_on = mc_note_on,
    .render = mc_render,
    .units = mc_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .hot = MC_HOT, .nhot = MC_HOT_N, .hot_at = mc_hot,   /* (0.13.8) the hot set, above */
};
