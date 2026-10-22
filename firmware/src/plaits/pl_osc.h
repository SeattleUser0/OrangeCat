/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' shared oscillators, ported to C.
 *   plaits/dsp/oscillator/oscillator.h       pl_osc_t, pl_osc_render (Oscillator::Render<shape, fm, tzfm>)
 *   plaits/dsp/oscillator/sine_oscillator.h  pl_sine, pl_sine_no_wrap, pl_sine_pm, pl_sine_raw, pl_sine_osc_t,
 *                                            pl_fast_sine_osc_t
 * The C++ templates become plain arguments; the callers pass constants, so the compiler still specialises them.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_dsp.h"

enum {
    PL_SHAPE_IMPULSE_TRAIN, PL_SHAPE_SAW, PL_SHAPE_TRIANGLE, PL_SHAPE_SLOPE, PL_SHAPE_SQUARE, PL_SHAPE_SQUARE_BRIGHT,
    PL_SHAPE_SQUARE_DARK, PL_SHAPE_SQUARE_TRIANGLE
};
#define PL_MAX_FREQUENCY 0.25f
#define PL_MIN_FREQUENCY 0.000001f

typedef struct {
    float phase, next_sample, lp_state, hp_state;
    int high;
    float frequency, pw;                        /* (interpolated) */
} pl_osc_t;

static inline void pl_osc_init(pl_osc_t *o)
{
    o->phase = 0.5f;
    o->next_sample = 0.0f;
    o->lp_state = 1.0f;
    o->hp_state = 0.0f;
    o->high = 1;
    o->frequency = 0.001f;
    o->pw = 0.5f;
}

/* Render<shape, has_external_fm, through_zero_fm>(frequency, pw, external_fm, out, size). Render<shape>(f, pw, fm,
 * out, size) with fm given is <shape, true, true>; without, <shape, false, false>. */
static inline __attribute__((always_inline)) void pl_osc_render_t(pl_osc_t *o, int shape, int has_external_fm,
    int through_zero_fm, float frequency, float pw, const float *external_fm, float *out, size_t size)
{
    if (!has_external_fm) {
        if (!through_zero_fm) PL_CONSTRAIN(frequency, PL_MIN_FREQUENCY, PL_MAX_FREQUENCY);
        else PL_CONSTRAIN(frequency, -PL_MAX_FREQUENCY, PL_MAX_FREQUENCY);
        PL_CONSTRAIN(pw, pl_fabsf(frequency) * 2.0f, 1.0f - 2.0f * pl_fabsf(frequency));
    }
    pl_interp_t fm, pwm;
    pl_interp_init(&fm, &o->frequency, frequency, size);
    pl_interp_init(&pwm, &o->pw, pw, size);
    float next_sample = o->next_sample;
    while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;
        float f = pl_interp_next(&fm);
        if (has_external_fm) {
            f *= (1.0f + *external_fm++);
            if (!through_zero_fm) PL_CONSTRAIN(f, PL_MIN_FREQUENCY, PL_MAX_FREQUENCY);
            else PL_CONSTRAIN(f, -PL_MAX_FREQUENCY, PL_MAX_FREQUENCY);
        }
        float w = (shape == PL_SHAPE_SQUARE_TRIANGLE || shape == PL_SHAPE_TRIANGLE) ? 0.5f : pl_interp_next(&pwm);
        if (has_external_fm) PL_CONSTRAIN(w, pl_fabsf(f) * 2.0f, 1.0f - 2.0f * pl_fabsf(f));
        o->phase += f;

        if (shape <= PL_SHAPE_SAW) {
            if (o->phase >= 1.0f) {
                o->phase -= 1.0f;
                float t = o->phase / f;
                this_sample -= pl_this_blep(t);
                next_sample -= pl_next_blep(t);
            } else if (through_zero_fm && o->phase < 0.0f) {
                float t = o->phase / f;
                o->phase += 1.0f;
                this_sample += pl_this_blep(t);
                next_sample += pl_next_blep(t);
            }
            next_sample += o->phase;
            if (shape == PL_SHAPE_SAW) {
                *out++ = 2.0f * this_sample - 1.0f;
            } else {
                o->lp_state += 0.25f * ((o->hp_state - this_sample) - o->lp_state);
                *out++ = 4.0f * o->lp_state;
                o->hp_state = this_sample;
            }
        } else if (shape <= PL_SHAPE_SLOPE) {
            float slope_up = 2.0f, slope_down = 2.0f;
            if (shape == PL_SHAPE_SLOPE) {
                slope_up = 1.0f / (w);
                slope_down = 1.0f / (1.0f - w);
            }
            if (o->high ^ (o->phase < w)) {
                float t = (o->phase - w) / f;
                float discontinuity = (slope_up + slope_down) * f;
                if (through_zero_fm && f < 0.0f) discontinuity = -discontinuity;
                this_sample -= pl_this_integrated_blep(t) * discontinuity;
                next_sample -= pl_next_integrated_blep(t) * discontinuity;
                o->high = o->phase < w;
            }
            if (o->phase >= 1.0f) {
                o->phase -= 1.0f;
                float t = o->phase / f;
                float discontinuity = (slope_up + slope_down) * f;
                this_sample += pl_this_integrated_blep(t) * discontinuity;
                next_sample += pl_next_integrated_blep(t) * discontinuity;
                o->high = 1;
            } else if (through_zero_fm && o->phase < 0.0f) {
                float t = o->phase / f;
                o->phase += 1.0f;
                float discontinuity = (slope_up + slope_down) * f;
                this_sample -= pl_this_integrated_blep(t) * discontinuity;
                next_sample -= pl_next_integrated_blep(t) * discontinuity;
                o->high = 0;
            }
            next_sample += o->high ? o->phase * slope_up : 1.0f - (o->phase - w) * slope_down;
            *out++ = 2.0f * this_sample - 1.0f;
        } else {
            if (o->high ^ (o->phase >= w)) {
                float t = (o->phase - w) / f;
                float discontinuity = 1.0f;
                if (through_zero_fm && f < 0.0f) discontinuity = -discontinuity;
                this_sample += pl_this_blep(t) * discontinuity;
                next_sample += pl_next_blep(t) * discontinuity;
                o->high = o->phase >= w;
            }
            if (o->phase >= 1.0f) {
                o->phase -= 1.0f;
                float t = o->phase / f;
                this_sample -= pl_this_blep(t);
                next_sample -= pl_next_blep(t);
                o->high = 0;
            } else if (through_zero_fm && o->phase < 0.0f) {
                float t = o->phase / f;
                o->phase += 1.0f;
                this_sample += pl_this_blep(t);
                next_sample += pl_next_blep(t);
                o->high = 1;
            }
            next_sample += o->phase < w ? 0.0f : 1.0f;
            if (shape == PL_SHAPE_SQUARE_TRIANGLE) {
                const float integrator_coefficient = f * 0.0625f;
                this_sample = 128.0f * (this_sample - 0.5f);
                o->lp_state += integrator_coefficient * (this_sample - o->lp_state);
                *out++ = o->lp_state;
            } else if (shape == PL_SHAPE_SQUARE_DARK) {
                const float integrator_coefficient = f * 2.0f;
                this_sample = 4.0f * (this_sample - 0.5f);
                o->lp_state += integrator_coefficient * (this_sample - o->lp_state);
                *out++ = o->lp_state;
            } else if (shape == PL_SHAPE_SQUARE_BRIGHT) {
                const float integrator_coefficient = f * 2.0f;
                this_sample = 2.0f * this_sample - 1.0f;
                o->lp_state += integrator_coefficient * (this_sample - o->lp_state);
                *out++ = (this_sample - o->lp_state) * 0.5f;
            } else {
                this_sample = 2.0f * this_sample - 1.0f;
                *out++ = this_sample;
            }
        }
    }
    o->next_sample = next_sample;
    pl_interp_done(&fm);
    pl_interp_done(&pwm);
}
/* Render<shape>(frequency, pw, fm, out, size): fm 0 = no external FM */
static inline void pl_osc_render(pl_osc_t *o, int shape, float frequency, float pw, const float *fm, float *out, size_t size)
{
    if (!fm) pl_osc_render_t(o, shape, 0, 0, frequency, pw, 0, out, size);
    else pl_osc_render_t(o, shape, 1, 1, frequency, pw, fm, out, size);
}

/* ------------------------------------------------- sine_oscillator.h --- */
#define PL_SINE_LUT_SIZE 512.0f
#define PL_SINE_LUT_QUADRATURE 128u
#define PL_SINE_LUT_BITS 9u
static inline float pl_sine(float phase) { return pl_interpolate_wrap(pl_lut_sine, phase, PL_SINE_LUT_SIZE); }
static inline float pl_sine_no_wrap(float phase) { return pl_interpolate(pl_lut_sine, phase, PL_SINE_LUT_SIZE); }
static inline float pl_sine_pm(uint32_t phase, float pm)
{
    const float max_uint32 = 4294967296.0f;
    const int max_index = 32;
    const float offset = (float)max_index;
    const float scale = max_uint32 / (float)(max_index * 2);
    phase += (uint32_t)((pm + offset) * scale) * (uint32_t)(max_index * 2);
    uint32_t integral = phase >> (32 - PL_SINE_LUT_BITS);
    float fractional = (float)(phase << PL_SINE_LUT_BITS) / max_uint32;
    float a = pl_lut_sine[integral];
    float b = pl_lut_sine[integral + 1];
    return a + (b - a) * fractional;
}
static inline float pl_sine_raw(uint32_t phase) { return pl_lut_sine[phase >> (32 - PL_SINE_LUT_BITS)]; }

typedef struct { float phase, frequency, amplitude; } pl_sine_osc_t;
static inline void pl_sine_osc_init(pl_sine_osc_t *s) { s->phase = 0.0f; s->frequency = 0.0f; s->amplitude = 0.0f; }
static inline float pl_sine_osc_next(pl_sine_osc_t *s, float frequency)
{
    if (frequency >= 0.5f) frequency = 0.5f;
    s->phase += frequency;
    if (s->phase >= 1.0f) s->phase -= 1.0f;
    return pl_sine_no_wrap(s->phase);
}
static inline void pl_sine_osc_next_quad(pl_sine_osc_t *s, float frequency, float amplitude, float *sn, float *cs)
{
    if (frequency >= 0.5f) frequency = 0.5f;
    s->phase += frequency;
    if (s->phase >= 1.0f) s->phase -= 1.0f;
    *sn = amplitude * pl_sine_no_wrap(s->phase);
    *cs = amplitude * pl_sine_no_wrap(s->phase + 0.25f);
}
static inline void pl_sine_osc_render_internal(pl_sine_osc_t *s, int additive, float frequency, float amplitude, float *out, size_t size)
{
    if (frequency >= 0.5f) frequency = 0.5f;
    pl_interp_t fm, am;
    pl_interp_init(&fm, &s->frequency, frequency, size);
    pl_interp_init(&am, &s->amplitude, amplitude, size);
    while (size--) {
        s->phase += pl_interp_next(&fm);
        if (s->phase >= 1.0f) s->phase -= 1.0f;
        float v = pl_sine_no_wrap(s->phase);
        if (additive) *out++ += pl_interp_next(&am) * v;
        else *out++ = v;
    }
    pl_interp_done(&fm);
    pl_interp_done(&am);
}
/* Render(frequency, amplitude, out, size) adds; Render(frequency, out, size) writes */
static inline void pl_sine_osc_render_add(pl_sine_osc_t *s, float frequency, float amplitude, float *out, size_t size)
{
    pl_sine_osc_render_internal(s, 1, frequency, amplitude, out, size);
}
static inline void pl_sine_osc_render(pl_sine_osc_t *s, float frequency, float *out, size_t size)
{
    pl_sine_osc_render_internal(s, 0, frequency, 1.0f, out, size);
}

typedef struct { float x, y, epsilon, amplitude; } pl_fast_sine_osc_t;
enum { PL_FSO_NORMAL, PL_FSO_ADDITIVE, PL_FSO_QUADRATURE };
static inline void pl_fast_sine_osc_init(pl_fast_sine_osc_t *s) { s->x = 1.0f; s->y = 0.0f; s->epsilon = 0.0f; s->amplitude = 0.0f; }
static inline float pl_fast_2sin(float f)
{
    const float f_pi = f * PL_PI;
    return f_pi * (2.0f - (2.0f * 0.96f / 6.0f) * f_pi * f_pi);
}
static inline void pl_fast_sine_osc_render_internal(pl_fast_sine_osc_t *s, int mode, float frequency, float amplitude,
                                                    float *out, float *out_2, size_t size)
{
    if (frequency >= 0.25f) {
        frequency = 0.25f;
        amplitude = 0.0f;
    } else {
        amplitude *= 1.0f - frequency * 4.0f;
    }
    pl_interp_t epsilon, am;
    pl_interp_init(&epsilon, &s->epsilon, pl_fast_2sin(frequency), size);
    pl_interp_init(&am, &s->amplitude, amplitude, size);
    float x = s->x, y = s->y;
    const float norm = x * x + y * y;
    if (norm <= 0.5f || norm >= 2.0f) {
        const float scale = pl_rsqrt_carmack(norm);
        x *= scale;
        y *= scale;
    }
    while (size--) {
        const float e = pl_interp_next(&epsilon);
        x += e * y;
        y -= e * x;
        if (mode == PL_FSO_ADDITIVE) {
            *out++ += pl_interp_next(&am) * x;
        } else if (mode == PL_FSO_NORMAL) {
            *out++ = x;
        } else {
            const float a = pl_interp_next(&am);
            *out++ = x * a;
            *out_2++ = y * a;
        }
    }
    s->x = x;
    s->y = y;
    pl_interp_done(&epsilon);
    pl_interp_done(&am);
}
static inline void pl_fast_sine_osc_render(pl_fast_sine_osc_t *s, float frequency, float *out, size_t size)
{
    pl_fast_sine_osc_render_internal(s, PL_FSO_NORMAL, frequency, 1.0f, out, 0, size);
}
static inline void pl_fast_sine_osc_render_add(pl_fast_sine_osc_t *s, float frequency, float amplitude, float *out, size_t size)
{
    pl_fast_sine_osc_render_internal(s, PL_FSO_ADDITIVE, frequency, amplitude, out, 0, size);
}
static inline void pl_fast_sine_osc_render_quadrature(pl_fast_sine_osc_t *s, float frequency, float amplitude, float *x,
                                                      float *y, size_t size)
{
    pl_fast_sine_osc_render_internal(s, PL_FSO_QUADRATURE, frequency, amplitude, x, y, size);
}
