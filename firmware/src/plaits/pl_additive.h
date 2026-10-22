/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 5, ADDITIVE (plaits/dsp/engine/additive_engine.cc), with its harmonic oscillator
 * (oscillator/harmonic_oscillator.h), in C.
 *   OUT: 24 integer harmonics. TIMBRE: the spectrum's centroid, MORPH: its slope, HARMONICS: bumps in it.
 *   AUX: the same envelope over the 8 Hammond drawbar harmonics (1, 2, 3, 4, 6, 8, 10, 12).
 * HarmonicOscillator<num_harmonics>::Render<first_harmonic_index> becomes pl_add_harm_render(..., num_harmonics,
 * first_harmonic_index, ...); its amplitude array is sized for the engine's batch (12). The allocator's amplitude
 * buffer becomes an array in the struct. Same on the device as in the C++.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

#define PL_ADD_BATCH 12                         /* kHarmonicBatchSize */
#define PL_ADD_NUM_HARMONICS 36                 /* kNumHarmonics */
#define PL_ADD_NUM_OSCILLATORS (PL_ADD_NUM_HARMONICS / PL_ADD_BATCH)

/* ---------------------------------------------- harmonic_oscillator.h --- */
typedef struct {
    float phase;
    float frequency;                            /* (interpolated) */
    float amplitude[PL_ADD_BATCH];              /* (interpolated) */
} pl_add_harm_t;

static inline void pl_add_harm_init(pl_add_harm_t *o, int num_harmonics)
{
    o->phase = 0.0f;
    o->frequency = 0.0f;
    for (int i = 0; i < num_harmonics; ++i) o->amplitude[i] = 0.0f;
}

/* Render<first_harmonic_index>(frequency, amplitudes, out, size) of HarmonicOscillator<num_harmonics>
 * (num_harmonics <= PL_ADD_BATCH). The first batch (first_harmonic_index 1) writes out, the others add to it. */
static inline __attribute__((always_inline)) void pl_add_harm_render(pl_add_harm_t *o, int num_harmonics,
    int first_harmonic_index, float frequency, const float *amplitudes, float *out, size_t size)
{
    if (frequency >= 0.5f) frequency = 0.5f;

    pl_interp_t am[PL_ADD_BATCH];
    pl_interp_t fm;
    pl_interp_init(&fm, &o->frequency, frequency, size);
    for (int i = 0; i < num_harmonics; ++i) {
        float f = frequency * (float)(first_harmonic_index + i);
        if (f >= 0.5f) f = 0.5f;
        pl_interp_init(&am[i], &o->amplitude[i], amplitudes[i] * (1.0f - f * 2.0f), size);
    }

    while (size--) {
        o->phase += pl_interp_next(&fm);
        if (o->phase >= 1.0f) o->phase -= 1.0f;
        const float two_x = 2.0f * pl_sine_no_wrap(o->phase);
        float previous, current;
        if (first_harmonic_index == 1) {
            previous = 1.0f;
            current = two_x * 0.5f;
        } else {
            const float k = (float)first_harmonic_index;
            previous = pl_sine(o->phase * (k - 1.0f) + 0.25f);
            current = pl_sine(o->phase * k);
        }
        /* Chebyshev recurrence: each harmonic from the two below it */
        float sum = 0.0f;
        for (int i = 0; i < num_harmonics; ++i) {
            sum += pl_interp_next(&am[i]) * current;
            float temp = current;
            current = two_x * current - previous;
            previous = temp;
        }
        if (first_harmonic_index == 1) *out++ = sum;
        else *out++ += sum;
    }
    /* (the C++ destructors: fm, then the am[] array; each writes back its own state, so the order does not matter) */
    pl_interp_done(&fm);
    for (int i = 0; i < num_harmonics; ++i) pl_interp_done(&am[i]);
}

/* ------------------------------------------------ additive_engine.cc --- */
typedef struct {
    pl_add_harm_t harmonic_oscillator[PL_ADD_NUM_OSCILLATORS];
    float amplitudes[PL_ADD_NUM_HARMONICS];     /* (the C++ takes these from the allocator) */
} pl_add_t;

static void pl_add_init(pl_add_t *e)
{
    for (int i = 0; i < PL_ADD_NUM_OSCILLATORS; ++i) pl_add_harm_init(&e->harmonic_oscillator[i], PL_ADD_BATCH);
}
static void pl_add_reset(pl_add_t *e)
{
    for (int i = 0; i < PL_ADD_NUM_HARMONICS; ++i) e->amplitudes[i] = 0.0f;
}

static void pl_add_update_amplitudes(float centroid, float slope, float bumps, float *amplitudes,
                                     const int *harmonic_indices, size_t num_harmonics)
{
    const float n = ((float)num_harmonics - 1.0f);
    const float margin = (1.0f / slope - 1.0f) / (1.0f + bumps);
    const float center = centroid * (n + margin) - 0.5f * margin;

    float sum = 0.001f;
    for (size_t i = 0; i < num_harmonics; ++i) {
        float order = pl_fabsf((float)i - center) * slope;
        float gain = 1.0f - order;
        gain += pl_fabsf(gain);
        gain *= gain;

        float b = 0.25f + order * bumps;
        float bump_factor = 1.0f + pl_sine(b);

        gain *= bump_factor;
        gain *= gain;
        gain *= gain;

        int j = harmonic_indices[i];
        /* (not a proper low-pass because of the normalisation, but the C++ says it is essential) */
        PL_ONE_POLE(amplitudes[j], gain, 0.001f);
        sum += amplitudes[j];
    }
    sum = 1.0f / sum;
    for (size_t i = 0; i < num_harmonics; ++i) amplitudes[harmonic_indices[i]] *= sum;
}

static const int pl_add_integer_harmonics[24] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23
};
static const int pl_add_organ_harmonics[8] = { 0, 1, 2, 3, 5, 7, 9, 11 };

static void pl_add_render(pl_add_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);

    const float centroid = p->timbre;
    const float raw_bumps = p->harmonics;
    const float raw_slope = (1.0f - 0.6f * raw_bumps) * p->morph;
    const float slope = 0.01f + 1.99f * raw_slope * raw_slope * raw_slope;
    const float bumps = 16.0f * raw_bumps * raw_bumps;

    pl_add_update_amplitudes(centroid, slope, bumps, &e->amplitudes[0], pl_add_integer_harmonics, 24);
    pl_add_harm_render(&e->harmonic_oscillator[0], PL_ADD_BATCH, 1, f0, &e->amplitudes[0], out, size);
    pl_add_harm_render(&e->harmonic_oscillator[1], PL_ADD_BATCH, 13, f0, &e->amplitudes[12], out, size);

    pl_add_update_amplitudes(centroid, slope, bumps, &e->amplitudes[24], pl_add_organ_harmonics, 8);
    pl_add_harm_render(&e->harmonic_oscillator[2], PL_ADD_BATCH, 1, f0, &e->amplitudes[24], aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_add_flush(pl_add_t *e)
{
    for (int i = 0; i < PL_ADD_NUM_OSCILLATORS; i++) pl_flush_n(e->harmonic_oscillator[i].amplitude, PL_ADD_BATCH);
    pl_flush_n(e->amplitudes, PL_ADD_NUM_HARMONICS);
}
