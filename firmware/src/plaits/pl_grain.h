/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 4, GRAIN (plaits/dsp/engine/grain_engine.cc), with its oscillators
 * (oscillator/grainlet_oscillator.h, oscillator/z_oscillator.h), in C.
 *   OUT: two grainlet oscillators (a shaped carrier times a sine formant) a ratio apart (HARMONICS), the formant at
 *        TIMBRE, the carrier's shape at MORPH, through a DC blocker.
 *   AUX: a Z oscillator (Casio CZ style resonant-filter waveforms), cutoff at TIMBRE, shape at MORPH, mode at HARMONICS.
 * The engine's VOSIM oscillator (oscillator/vosim_oscillator.h) is commented out in the C++, so it is not ported.
 * Same on the device as in the C++.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

/* ---------------------------------------------- grainlet_oscillator.h --- */
typedef struct {
    float carrier_phase, formant_phase, next_sample;
    float carrier_frequency, formant_frequency, carrier_shape, carrier_bleed;   /* (interpolated) */
} pl_grain_grainlet_t;

static inline void pl_grain_grainlet_init(pl_grain_grainlet_t *o)
{
    o->carrier_phase = 0.0f;
    o->formant_phase = 0.0f;
    o->next_sample = 0.0f;
    o->carrier_frequency = 0.0f;
    o->formant_frequency = 0.0f;
    o->carrier_shape = 0.0f;
    o->carrier_bleed = 0.0f;
}

static inline float pl_grain_carrier(float phase, float shape)
{
    shape *= 3.0f;
    PL_MAKE_INTEGRAL_FRACTIONAL(shape)
    float t = 1.0f - shape_fractional;
    if (shape_integral == 0) {
        phase = phase * (1.0f + t * t * t * 15.0f);
        if (phase >= 1.0f) phase = 1.0f;
        phase += 0.75f;
    } else if (shape_integral == 1) {
        float breakpoint = 0.001f + 0.499f * t * t * t;
        if (phase < breakpoint) phase *= (0.5f / breakpoint);
        else phase = 0.5f + (phase - breakpoint) * 0.5f / (1.0f - breakpoint);
        phase += 0.75f;
    } else {
        t = 1.0f - t;
        phase = 0.25f + phase * (0.5f + t * t * t * 14.5f);
        if (phase >= 0.75f) phase = 0.75f;
    }
    return (pl_sine(phase) + 1.0f) * 0.25f;
}

static inline float pl_grain_grainlet(float carrier_phase, float formant_phase, float shape, float bleed)
{
    float carrier = pl_grain_carrier(carrier_phase, shape);
    float formant = pl_sine(formant_phase);
    return carrier * (formant + bleed) / (1.0f + bleed);
}

static void pl_grain_grainlet_render(pl_grain_grainlet_t *o, float carrier_frequency, float formant_frequency,
                                     float carrier_shape, float carrier_bleed, float *out, size_t size)
{
    if (carrier_frequency >= PL_MAX_FREQUENCY * 0.5f) carrier_frequency = PL_MAX_FREQUENCY * 0.5f;
    if (formant_frequency >= PL_MAX_FREQUENCY) formant_frequency = PL_MAX_FREQUENCY;

    pl_interp_t carrier_frequency_modulation, formant_frequency_modulation, carrier_shape_modulation,
        carrier_bleed_modulation;
    pl_interp_init(&carrier_frequency_modulation, &o->carrier_frequency, carrier_frequency, size);
    pl_interp_init(&formant_frequency_modulation, &o->formant_frequency, formant_frequency, size);
    pl_interp_init(&carrier_shape_modulation, &o->carrier_shape, carrier_shape, size);
    pl_interp_init(&carrier_bleed_modulation, &o->carrier_bleed, carrier_bleed, size);

    float next_sample = o->next_sample;
    while (size--) {
        int reset;
        float reset_time = 0.0f;
        float this_sample = next_sample;
        next_sample = 0.0f;

        const float f0 = pl_interp_next(&carrier_frequency_modulation);
        const float f1 = pl_interp_next(&formant_frequency_modulation);

        o->carrier_phase += f0;
        reset = o->carrier_phase >= 1.0f;
        if (reset) {
            o->carrier_phase -= 1.0f;
            reset_time = o->carrier_phase / f0;
            float before = pl_grain_grainlet(1.0f, o->formant_phase + (1.0f - reset_time) * f1,
                                             pl_interp_sub(&carrier_shape_modulation, 1.0f - reset_time),
                                             pl_interp_sub(&carrier_bleed_modulation, 1.0f - reset_time));
            float after = pl_grain_grainlet(0.0f, 0.0f, pl_interp_sub(&carrier_shape_modulation, 1.0f),
                                            pl_interp_sub(&carrier_bleed_modulation, 1.0f));
            float discontinuity = after - before;
            this_sample += discontinuity * pl_this_blep(reset_time);
            next_sample += discontinuity * pl_next_blep(reset_time);
            o->formant_phase = reset_time * f1;
        } else {
            o->formant_phase += f1;
            if (o->formant_phase >= 1.0f) o->formant_phase -= 1.0f;
        }
        /* (the two Next() calls are on separate interpolators: their order does not matter) */
        const float shape = pl_interp_next(&carrier_shape_modulation);
        const float bleed = pl_interp_next(&carrier_bleed_modulation);
        next_sample += pl_grain_grainlet(o->carrier_phase, o->formant_phase, shape, bleed);
        *out++ = this_sample;
    }
    o->next_sample = next_sample;
    pl_interp_done(&carrier_frequency_modulation);
    pl_interp_done(&formant_frequency_modulation);
    pl_interp_done(&carrier_shape_modulation);
    pl_interp_done(&carrier_bleed_modulation);
}

/* ----------------------------------------------------- z_oscillator.h --- */
typedef struct {
    float carrier_phase, discontinuity_phase, formant_phase, next_sample;
    float carrier_frequency, formant_frequency, carrier_shape, mode;            /* (interpolated) */
} pl_grain_z_t;

static inline void pl_grain_z_init(pl_grain_z_t *o)
{
    o->carrier_phase = 0.0f;
    o->discontinuity_phase = 0.0f;
    o->formant_phase = 0.0f;
    o->next_sample = 0.0f;
    o->carrier_frequency = 0.0f;
    o->formant_frequency = 0.0f;
    o->carrier_shape = 0.0f;
    o->mode = 0.0f;
}

static inline float pl_grain_z(float c, float d, float f, float shape, float mode)
{
    float ramp_down = 0.5f * (1.0f + pl_sine(0.5f * d + 0.25f));
    float offset, phase_shift;
    if (mode < 0.333f) {
        offset = 1.0f;
        phase_shift = 0.25f + mode * 1.50f;
    } else if (mode < 0.666f) {
        phase_shift = 0.7495f - (mode - 0.33f) * 0.75f;
        offset = -pl_sine(phase_shift);
    } else {
        phase_shift = 0.7495f - (mode - 0.33f) * 0.75f;
        offset = 0.001f;
    }
    float discontinuity = pl_sine(f + phase_shift);
    float contour;
    if (shape < 0.5f) {
        shape *= 2.0f;
        if (c >= 0.5f) ramp_down *= shape;
        contour = 1.0f + (pl_sine(c + 0.25f) - 1.0f) * shape;
    } else {
        contour = pl_sine(c + shape * 0.5f);
    }
    return (ramp_down * (offset + discontinuity) - offset) * contour;
}

static void pl_grain_z_render(pl_grain_z_t *o, float carrier_frequency, float formant_frequency, float carrier_shape,
                              float mode, float *out, size_t size)
{
    if (carrier_frequency >= PL_MAX_FREQUENCY * 0.5f) carrier_frequency = PL_MAX_FREQUENCY * 0.5f;
    if (formant_frequency >= PL_MAX_FREQUENCY) formant_frequency = PL_MAX_FREQUENCY;

    pl_interp_t carrier_frequency_modulation, formant_frequency_modulation, carrier_shape_modulation, mode_modulation;
    pl_interp_init(&carrier_frequency_modulation, &o->carrier_frequency, carrier_frequency, size);
    pl_interp_init(&formant_frequency_modulation, &o->formant_frequency, formant_frequency, size);
    pl_interp_init(&carrier_shape_modulation, &o->carrier_shape, carrier_shape, size);
    pl_interp_init(&mode_modulation, &o->mode, mode, size);

    float next_sample = o->next_sample;
    while (size--) {
        int reset;
        float reset_time = 0.0f;
        float this_sample = next_sample;
        next_sample = 0.0f;

        const float f0 = pl_interp_next(&carrier_frequency_modulation);
        const float f1 = pl_interp_next(&formant_frequency_modulation);

        o->discontinuity_phase += 2.0f * f0;
        o->carrier_phase += f0;
        reset = o->discontinuity_phase >= 1.0f;
        if (reset) {
            o->discontinuity_phase -= 1.0f;
            reset_time = o->discontinuity_phase / (2.0f * f0);
            float carrier_phase_before = o->carrier_phase >= 1.0f ? 1.0f : 0.5f;
            float carrier_phase_after = o->carrier_phase >= 1.0f ? 0.0f : 0.5f;
            float before = pl_grain_z(carrier_phase_before, 1.0f, o->formant_phase + (1.0f - reset_time) * f1,
                                      pl_interp_sub(&carrier_shape_modulation, 1.0f - reset_time),
                                      pl_interp_sub(&mode_modulation, 1.0f - reset_time));
            float after = pl_grain_z(carrier_phase_after, 0.0f, 0.0f, pl_interp_sub(&carrier_shape_modulation, 1.0f),
                                     pl_interp_sub(&mode_modulation, 1.0f));
            float discontinuity = after - before;
            this_sample += discontinuity * pl_this_blep(reset_time);
            next_sample += discontinuity * pl_next_blep(reset_time);
            o->formant_phase = reset_time * f1;
            if (o->carrier_phase > 1.0f) o->carrier_phase = o->discontinuity_phase * 0.5f;
        } else {
            o->formant_phase += f1;
            if (o->formant_phase >= 1.0f) o->formant_phase -= 1.0f;
        }
        if (o->carrier_phase >= 1.0f) o->carrier_phase -= 1.0f;

        const float shape = pl_interp_next(&carrier_shape_modulation);
        const float m = pl_interp_next(&mode_modulation);
        next_sample += pl_grain_z(o->carrier_phase, o->discontinuity_phase, o->formant_phase, shape, m);
        *out++ = this_sample;
    }
    o->next_sample = next_sample;
    pl_interp_done(&carrier_frequency_modulation);
    pl_interp_done(&formant_frequency_modulation);
    pl_interp_done(&carrier_shape_modulation);
    pl_interp_done(&mode_modulation);
}

/* --------------------------------------------------- grain_engine.cc --- */
typedef struct {
    pl_grain_grainlet_t grainlet[2];
    pl_grain_z_t z_oscillator;
    pl_onepole_t dc_blocker[2];
} pl_grain_t;

static void pl_grain_init(pl_grain_t *e)
{
    pl_grain_grainlet_init(&e->grainlet[0]);
    pl_grain_grainlet_init(&e->grainlet[1]);
    pl_grain_z_init(&e->z_oscillator);
    pl_onepole_init(&e->dc_blocker[0]);
    pl_onepole_init(&e->dc_blocker[1]);
}
static void pl_grain_reset(pl_grain_t *e) { (void)e; }

static __attribute__((section(".text.xip_macro"))) void pl_grain_render(pl_grain_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                            int *already_enveloped)
{
    (void)already_enveloped;
    const float root = p->note;
    const float f0 = pl_note_to_frequency(root);

    const float f1 = pl_note_to_frequency(24.0f + 84.0f * p->timbre);
    const float ratio = pl_semitones_to_ratio(-24.0f + 48.0f * p->harmonics);
    const float carrier_bleed = p->harmonics < 0.5f ? 1.0f - 2.0f * p->harmonics : 0.0f;
    const float carrier_bleed_fixed = carrier_bleed * (2.0f - carrier_bleed);
    const float carrier_shape = 0.33f + (p->morph - 0.33f) * pl_maxf(1.0f - f0 * 24.0f, 0.0f);

    pl_grain_grainlet_render(&e->grainlet[0], f0, f1, carrier_shape, carrier_bleed_fixed, out, size);
    pl_grain_grainlet_render(&e->grainlet[1], f0, f1 * ratio, carrier_shape, carrier_bleed_fixed, aux, size);
    pl_onepole_set_f(&e->dc_blocker[0], PL_FREQ_DIRTY, 0.3f * f0);
    for (size_t i = 0; i < size; ++i) out[i] = pl_onepole_process(&e->dc_blocker[0], PL_HP, out[i] + aux[i]);

    const float cutoff = pl_note_to_frequency(root + 96.0f * p->timbre);
    pl_grain_z_render(&e->z_oscillator, f0, cutoff, p->morph, p->harmonics, aux, size);

    pl_onepole_set_f(&e->dc_blocker[1], PL_FREQ_DIRTY, 0.3f * f0);
    pl_onepole_process_buf(&e->dc_blocker[1], PL_HP, aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_grain_flush(pl_grain_t *e) { pl_flush_onepole(&e->dc_blocker[0]); pl_flush_onepole(&e->dc_blocker[1]); }
