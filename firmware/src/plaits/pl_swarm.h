/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 8, SWARM (plaits/dsp/engine/swarm_engine.cc and .h: GrainEnvelope,
 * AdditiveSawOscillator, SwarmVoice), in C. Uses pl_fast_sine_osc_t (sine_oscillator.h) from pl_osc.h.
 *   Eight voices, each a saw (OUT) and a sine (AUX) with its own grain envelope.
 *   HARMONICS: the spread of the voices. TIMBRE: the grain density. MORPH: the grain size (from short grains to
 *   overlapping glissandi). Triggered (TRIGGER patched): each trigger starts a burst.
 * The C++ allocates its eight voices from the engine's buffer; here they are an array in the struct. As in the C++,
 * the voices are set up by reset, not init.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

#define PL_SWARM_NUM_VOICES 8

/* ------------------------------------------------------- GrainEnvelope --- */
typedef struct { float from, interval, phase, fm, amplitude, previous_size_ratio, filter_coefficient; } pl_swarm_env_t;
static inline void pl_swarm_env_init(pl_swarm_env_t *g)
{
    g->from = 0.0f;
    g->interval = 1.0f;
    g->phase = 1.0f;
    g->fm = 0.0f;
    g->amplitude = 0.5f;
    g->previous_size_ratio = 0.0f;
    g->filter_coefficient = 0.0f;
}
static inline void pl_swarm_env_step(pl_swarm_env_t *g, float rate, int burst_mode, int start_burst)
{
    int randomize = 0;
    if (start_burst) {
        g->phase = 0.5f;
        g->fm = 16.0f;
        randomize = 1;
    } else {
        g->phase += rate * g->fm;
        if (g->phase >= 1.0f) {
            g->phase -= (float)((int)g->phase);
            randomize = 1;
        }
    }
    if (randomize) {
        g->from += g->interval;
        g->interval = pl_rng_float() - g->from;
        /* the grain's duration */
        if (burst_mode) g->fm *= 0.8f + 0.2f * pl_rng_float();
        else g->fm = 0.5f + 1.5f * pl_rng_float();
    }
}
static inline float pl_swarm_env_frequency(const pl_swarm_env_t *g, float size_ratio)
{
    /* Two overlapping grains (f1, f2) become one tone ramping from f1 to f2: a continuous transition from the
     * grain cloud to the swarm of glissandi. */
    if (size_ratio < 1.0f) return 2.0f * (g->from + g->interval * g->phase) - 1.0f;
    else return g->from;
}
static inline float pl_swarm_env_amplitude(pl_swarm_env_t *g, float size_ratio)
{
    float target_amplitude = 1.0f;
    if (size_ratio >= 1.0f) {
        float phase = (g->phase - 0.5f) * size_ratio;
        PL_CONSTRAIN(phase, -1.0f, 1.0f);
        float e = pl_sine(0.5f * phase + 1.25f);
        target_amplitude = 0.5f * (e + 1.0f);
    }
    if ((size_ratio >= 1.0f) ^ (g->previous_size_ratio >= 1.0f)) g->filter_coefficient = 0.5f;
    g->filter_coefficient *= 0.95f;
    g->previous_size_ratio = size_ratio;
    PL_ONE_POLE(g->amplitude, target_amplitude, 0.5f - g->filter_coefficient);
    return g->amplitude;
}

/* ----------------------------------------------- AdditiveSawOscillator --- */
typedef struct { float phase, next_sample, frequency, gain; } pl_swarm_saw_t;
static inline void pl_swarm_saw_init(pl_swarm_saw_t *o)
{
    o->phase = 0.0f;
    o->next_sample = 0.0f;
    o->frequency = 0.01f;
    o->gain = 0.0f;
}
/* Render(frequency, level, out, size): adds to out */
static inline void pl_swarm_saw_render(pl_swarm_saw_t *o, float frequency, float level, float *out, size_t size)
{
    if (frequency >= PL_MAX_FREQUENCY) frequency = PL_MAX_FREQUENCY;
    pl_interp_t fm, gain;
    pl_interp_init(&fm, &o->frequency, frequency, size);
    pl_interp_init(&gain, &o->gain, level, size);
    float next_sample = o->next_sample;
    float phase = o->phase;
    while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;
        const float f = pl_interp_next(&fm);
        phase += f;
        if (phase >= 1.0f) {
            phase -= 1.0f;
            float t = phase / f;
            this_sample -= pl_this_blep(t);
            next_sample -= pl_next_blep(t);
        }
        next_sample += phase;
        *out++ += (2.0f * this_sample - 1.0f) * pl_interp_next(&gain);
    }
    o->phase = phase;
    o->next_sample = next_sample;
    pl_interp_done(&fm);
    pl_interp_done(&gain);
}

/* ---------------------------------------------------------- SwarmVoice --- */
typedef struct {
    float rank;
    pl_swarm_env_t envelope;
    pl_swarm_saw_t saw;
    pl_fast_sine_osc_t sine;
} pl_swarm_voice_t;
static inline void pl_swarm_voice_init(pl_swarm_voice_t *v, float rank)
{
    v->rank = rank;
    pl_swarm_env_init(&v->envelope);
    pl_swarm_saw_init(&v->saw);
    pl_fast_sine_osc_init(&v->sine);
}
static inline void pl_swarm_voice_render(pl_swarm_voice_t *v, float f0, float density, int burst_mode, int start_burst,
                                         float spread, float size_ratio, float *saw, float *sine, size_t size)
{
    pl_swarm_env_step(&v->envelope, density, burst_mode, start_burst);

    const float scale = 1.0f / PL_SWARM_NUM_VOICES;
    const float amplitude = pl_swarm_env_amplitude(&v->envelope, size_ratio) * scale;

    const float expo_amount = pl_swarm_env_frequency(&v->envelope, size_ratio);
    f0 *= pl_semitones_to_ratio(48.0f * expo_amount * spread * v->rank);

    const float linear_amount = v->rank * (v->rank + 0.01f) * spread * 0.25f;
    f0 *= 1.0f + linear_amount;

    pl_swarm_saw_render(&v->saw, f0, amplitude, saw, size);
    pl_fast_sine_osc_render_add(&v->sine, f0, amplitude, sine, size);
}

/* ----------------------------------------------------- swarm_engine.cc --- */
typedef struct {
    pl_swarm_voice_t swarm_voice[PL_SWARM_NUM_VOICES];
} pl_swarm_t;

static void pl_swarm_init(pl_swarm_t *e) { (void)e; }   /* (the C++ only allocates the voices here) */

static void pl_swarm_reset(pl_swarm_t *e)
{
    const float n = (float)((PL_SWARM_NUM_VOICES - 1) / 2);   /* integer division, as in the C++: 3 */
    int i;
    for (i = 0; i < PL_SWARM_NUM_VOICES; ++i) {
        float rank = ((float)i - n) / n;
        pl_swarm_voice_init(&e->swarm_voice[i], rank);
    }
}

static __attribute__((section(".text.xip_macro"))) void pl_swarm_render(pl_swarm_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    size_t j;
    int i;
    const float f0 = pl_note_to_frequency(p->note);
    const float control_rate = (float)size;
    const float density = pl_note_to_frequency(p->timbre * 120.0f) * 0.025f * control_rate;
    const float spread = p->harmonics * p->harmonics * p->harmonics;
    float size_ratio = 0.25f * pl_semitones_to_ratio((1.0f - p->morph) * 84.0f);

    const int burst_mode = !(p->trigger & PL_TRIGGER_UNPATCHED);
    const int start_burst = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;

    for (j = 0; j < size; ++j) { out[j] = 0.0f; aux[j] = 0.0f; }

    for (i = 0; i < PL_SWARM_NUM_VOICES; ++i) {
        pl_swarm_voice_render(&e->swarm_voice[i], f0, density, burst_mode, start_burst, spread, size_ratio, out, aux, size);
        size_ratio *= 0.97f;
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_swarm_flush(pl_swarm_t *e)
{
    for (int i = 0; i < PL_SWARM_NUM_VOICES; i++) {
        pl_flush(&e->swarm_voice[i].envelope.filter_coefficient);
        pl_flush(&e->swarm_voice[i].envelope.amplitude);
        pl_flush(&e->swarm_voice[i].saw.gain);
        pl_flush(&e->swarm_voice[i].sine.amplitude);
    }
}
