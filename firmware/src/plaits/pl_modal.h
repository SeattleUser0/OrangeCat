/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 12, MODAL (plaits/dsp/engine/modal_engine.cc), with its voice and resonator
 * (physical_modelling/modal_voice.cc, resonator.cc/.h) and noise/dust.h (pl_grp_c.h), in C.
 *   A mallet (a click through a low pass, or dust when the trigger is unpatched) striking 24 band-pass modes.
 *   HARMONICS: the structure (mode stretching, from lut_stiffness), TIMBRE: brightness, MORPH: damping.
 *   OUT: the resonator. AUX: the exciter.
 * No difference from the C++ on the device (its state is under 1 KB).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_grp_c.h"

#define PL_MODAL_MAX_NUM_MODES 24               /* kMaxNumModes */
#define PL_MODAL_MODE_BATCH_SIZE 4              /* kModeBatchSize */

/* ------------------------------------------------- ResonatorSvf<batch> --- */
/* The template's batch_size is an argument (1 or 4); the state arrays hold the largest. */
typedef struct {
    float state_1[PL_MODAL_MODE_BATCH_SIZE];
    float state_2[PL_MODAL_MODE_BATCH_SIZE];
} pl_modal_svf_t;

static inline void pl_modal_svf_init(pl_modal_svf_t *r, int batch_size)
{
    int i;
    for (i = 0; i < batch_size; ++i) r->state_1[i] = r->state_2[i] = 0.0f;
}

/* Process<mode, add>(f, q, gain, in, out, size); mode PL_LP or PL_BP */
static void pl_modal_svf_process(pl_modal_svf_t *r, int batch_size, int mode, int add, const float *f, const float *q,
                                 const float *gain, const float *in, float *out, size_t size)
{
    float g[PL_MODAL_MODE_BATCH_SIZE];
    float rr[PL_MODAL_MODE_BATCH_SIZE];        /* (r[] in the C++) */
    float r_plus_g[PL_MODAL_MODE_BATCH_SIZE];
    float h[PL_MODAL_MODE_BATCH_SIZE];
    float state_1[PL_MODAL_MODE_BATCH_SIZE];
    float state_2[PL_MODAL_MODE_BATCH_SIZE];
    float gains[PL_MODAL_MODE_BATCH_SIZE];
    int i;
    for (i = 0; i < batch_size; ++i) {
        g[i] = pl_tan_fast(f[i]);
        rr[i] = 1.0f / q[i];
        h[i] = 1.0f / (1.0f + rr[i] * g[i] + g[i] * g[i]);
        r_plus_g[i] = rr[i] + g[i];
        state_1[i] = r->state_1[i];
        state_2[i] = r->state_2[i];
        gains[i] = gain[i];
    }

    while (size--) {
        float s_in = *in++;
        float s_out = 0.0f;
        for (i = 0; i < batch_size; ++i) {
            const float hp = (s_in - r_plus_g[i] * state_1[i] - state_2[i]) * h[i];
            const float bp = g[i] * hp + state_1[i];
            state_1[i] = g[i] * hp + bp;
            const float lp = g[i] * bp + state_2[i];
            state_2[i] = g[i] * bp + lp;
            s_out += gains[i] * (mode == PL_LP ? lp : bp);
        }
        if (add) *out++ += s_out;
        else *out++ = s_out;
    }
    for (i = 0; i < batch_size; ++i) {
        r->state_1[i] = state_1[i];
        r->state_2[i] = state_2[i];
    }
}

/* ---------------------------------------------------------- resonator.cc --- */
typedef struct {
    int resolution;
    float mode_amplitude[PL_MODAL_MAX_NUM_MODES];
    pl_modal_svf_t mode_filters[PL_MODAL_MAX_NUM_MODES / PL_MODAL_MODE_BATCH_SIZE];
} pl_modal_res_t;

static void pl_modal_res_init(pl_modal_res_t *r, float position, int resolution)
{
    int i;
    r->resolution = resolution < PL_MODAL_MAX_NUM_MODES ? resolution : PL_MODAL_MAX_NUM_MODES;

    pl_cosine_osc_t amplitudes;
    pl_cosine_osc_init_approximate(&amplitudes, position);

    for (i = 0; i < resolution; ++i) r->mode_amplitude[i] = pl_cosine_osc_next(&amplitudes) * 0.25f;

    for (i = 0; i < PL_MODAL_MAX_NUM_MODES / PL_MODAL_MODE_BATCH_SIZE; ++i)
        pl_modal_svf_init(&r->mode_filters[i], PL_MODAL_MODE_BATCH_SIZE);
}

static inline float pl_modal_nth_harmonic_compensation(int n, float stiffness)
{
    float stretch_factor = 1.0f;
    int i;
    for (i = 0; i < n - 1; ++i) {
        stretch_factor += stiffness;
        if (stiffness < 0.0f) stiffness *= 0.93f;
        else stiffness *= 0.98f;
    }
    return 1.0f / stretch_factor;
}

static void pl_modal_res_process(pl_modal_res_t *r, float f0, float structure, float brightness, float damping,
                                 const float *in, float *out, size_t size)
{
    float stiffness = pl_interpolate(pl_lut_stiffness, structure, 64.0f);
    f0 *= pl_modal_nth_harmonic_compensation(3, stiffness);

    float harmonic = f0;
    float stretch_factor = 1.0f;
    float q_sqrt = pl_semitones_to_ratio(damping * 79.7f);
    float q = 500.0f * q_sqrt * q_sqrt;
    brightness *= 1.0f - structure * 0.3f;
    brightness *= 1.0f - damping * 0.3f;
    float q_loss = brightness * (2.0f - brightness) * 0.85f + 0.15f;

    float mode_q[PL_MODAL_MODE_BATCH_SIZE];
    float mode_f[PL_MODAL_MODE_BATCH_SIZE];
    float mode_a[PL_MODAL_MODE_BATCH_SIZE];
    int batch_counter = 0;
    int i;

    pl_modal_svf_t *batch_processor = &r->mode_filters[0];

    for (i = 0; i < r->resolution; ++i) {
        float mode_frequency = harmonic * stretch_factor;
        if (mode_frequency >= 0.499f) mode_frequency = 0.499f;
        const float mode_attenuation = 1.0f - mode_frequency * 2.0f;

        mode_f[batch_counter] = mode_frequency;
        mode_q[batch_counter] = 1.0f + mode_frequency * q;
        mode_a[batch_counter] = r->mode_amplitude[i] * mode_attenuation;
        ++batch_counter;

        if (batch_counter == PL_MODAL_MODE_BATCH_SIZE) {
            batch_counter = 0;
            pl_modal_svf_process(batch_processor, PL_MODAL_MODE_BATCH_SIZE, PL_BP, 1, mode_f, mode_q, mode_a, in, out,
                                 size);
            ++batch_processor;
        }

        stretch_factor += stiffness;
        if (stiffness < 0.0f) stiffness *= 0.93f;   /* partials must not fold back into negative frequencies */
        else stiffness *= 0.98f;                    /* a few extra partials in the highest frequencies */
        harmonic += f0;
        q *= q_loss;
    }
}

/* -------------------------------------------------------- modal_voice.cc --- */
typedef struct {
    pl_modal_svf_t excitation_filter;           /* ResonatorSvf<1> */
    pl_modal_res_t resonator;
} pl_modal_voice_t;

static void pl_modal_voice_init(pl_modal_voice_t *v)
{
    pl_modal_svf_init(&v->excitation_filter, 1);
    pl_modal_res_init(&v->resonator, 0.015f, PL_MODAL_MAX_NUM_MODES);
}

static void pl_modal_voice_render(pl_modal_voice_t *v, int sustain, int trigger, float accent, float f0,
                                  float structure, float brightness, float damping, float *temp, float *out,
                                  float *aux, size_t size)
{
    const float density = brightness * brightness;
    size_t i;

    brightness += 0.25f * accent * (1.0f - brightness);
    damping += 0.25f * accent * (1.0f - damping);

    const float range = sustain ? 36.0f : 60.0f;
    const float f = sustain ? 4.0f * f0 : 2.0f * f0;
    const float cutoff = pl_minf(f * pl_semitones_to_ratio((brightness * (2.0f - brightness) - 0.5f) * range), 0.499f);
    const float q = sustain ? 0.7f : 1.5f;

    /* Synthesize excitation signal. */
    if (sustain) {
        const float dust_f = 0.00005f + 0.99995f * density * density;
        for (i = 0; i < size; ++i) temp[i] = plc_dust(dust_f) * (4.0f - dust_f * 3.0f) * accent;
    } else {
        for (i = 0; i < size; ++i) temp[i] = 0.0f;
        if (trigger) {
            const float attenuation = 1.0f - damping * 0.5f;
            const float amplitude = (0.12f + 0.08f * accent) * attenuation;
            temp[0] = amplitude * pl_semitones_to_ratio(cutoff * cutoff * 24.0f) / cutoff;
        }
    }
    const float one = 1.0f;
    pl_modal_svf_process(&v->excitation_filter, 1, PL_LP, 0, &cutoff, &q, &one, temp, temp, size);
    for (i = 0; i < size; ++i) aux[i] += temp[i];

    pl_modal_res_process(&v->resonator, f0, structure, brightness, damping, temp, out, size);
}

/* ------------------------------------------------------- modal_engine.cc --- */
typedef struct {
    pl_modal_voice_t voice;
    float temp_buffer[PL_MAX_BLOCK];
    float harmonics_lp;
} pl_modal_t;

static void pl_modal_reset(pl_modal_t *e) { pl_modal_voice_init(&e->voice); }
static void pl_modal_init(pl_modal_t *e)
{
    e->harmonics_lp = 0.0f;
    pl_modal_reset(e);
}

static void pl_modal_render(pl_modal_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                            int *already_enveloped)
{
    size_t i;
    (void)already_enveloped;
    for (i = 0; i < size; ++i) out[i] = 0.0f;
    for (i = 0; i < size; ++i) aux[i] = 0.0f;

    PL_ONE_POLE(e->harmonics_lp, p->harmonics, 0.01f);

    pl_modal_voice_render(&e->voice, (p->trigger & PL_TRIGGER_UNPATCHED) != 0, (p->trigger & PL_TRIGGER_RISING_EDGE) != 0,
                          p->accent, pl_note_to_frequency(p->note), e->harmonics_lp, p->timbre, p->morph,
                          e->temp_buffer, out, aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_modal_flush(pl_modal_t *e)
{
    pl_flush_n(e->voice.excitation_filter.state_1, PL_MODAL_MODE_BATCH_SIZE);
    pl_flush_n(e->voice.excitation_filter.state_2, PL_MODAL_MODE_BATCH_SIZE);
    for (int i = 0; i < PL_MODAL_MAX_NUM_MODES / PL_MODAL_MODE_BATCH_SIZE; i++) {
        pl_flush_n(e->voice.resonator.mode_filters[i].state_1, PL_MODAL_MODE_BATCH_SIZE);
        pl_flush_n(e->voice.resonator.mode_filters[i].state_2, PL_MODAL_MODE_BATCH_SIZE);
    }
}
