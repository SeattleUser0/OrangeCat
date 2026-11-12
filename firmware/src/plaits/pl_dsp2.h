/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits, stmlib). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the stmlib and Plaits helpers the ten further models share and pl_dsp.h does not have.
 *   stmlib/dsp/hysteresis_quantizer.h        pl_hysq_t (HysteresisQuantizer2; pl_chord.h keeps its own copy, pl_chord_hysq_t)
 *   stmlib/dsp/filter.h                      pl_naive_svf_t (NaiveSvf)
 *   plaits/dsp/oscillator/wavetable_oscillator.h  pl_diff_t (Differentiator), pl_interpolate_wave, pl_interpolate_wave_hermite
 * Every float operation in the order of the C++.
 */
#pragma once
#include "pl_dsp.h"

/* ------------------------------------------- stmlib HysteresisQuantizer2 --- */
typedef struct { int num_steps; float hysteresis, scale, offset; int quantized_value; } pl_hysq_t;
static inline void pl_hysq_init(pl_hysq_t *h, int num_steps, float hysteresis, int symmetric)
{
    h->num_steps = num_steps;
    h->hysteresis = hysteresis;
    h->scale = (float)(symmetric ? num_steps - 1 : num_steps);
    h->offset = symmetric ? 0.0f : -0.5f;
    h->quantized_value = 0;
}
static inline int pl_hysq_process_base(pl_hysq_t *h, int base, float value)
{
    value *= h->scale;
    value += h->offset;
    value += (float)base;
    float hysteresis_sign = value > (float)h->quantized_value ? -1.0f : +1.0f;
    int q = (int)(value + hysteresis_sign * h->hysteresis + 0.5f);
    PL_CONSTRAIN(q, 0, h->num_steps - 1);
    h->quantized_value = q;
    return q;
}
static inline int pl_hysq_process(pl_hysq_t *h, float value) { return pl_hysq_process_base(h, 0, value); }

/* ------------------------------------------------------- stmlib NaiveSvf --- */
typedef struct { float f, damp, lp, bp; } pl_naive_svf_t;
static inline void pl_naive_svf_set_f_q(pl_naive_svf_t *s, int approximation, float f, float resonance)
{
    (void)approximation;                        /* (FREQUENCY_EXACT needs sinf, which the firmware has not got: no model uses it) */
    f = f < 0.158f ? f : 0.158f;
    s->f = 2.0f * PL_PI_F * f;
    s->damp = 1.0f / resonance;
}
static inline void pl_naive_svf_reset(pl_naive_svf_t *s) { s->lp = s->bp = 0.0f; }
static inline void pl_naive_svf_init(pl_naive_svf_t *s) { pl_naive_svf_set_f_q(s, PL_FREQ_DIRTY, 0.01f, 100.0f); pl_naive_svf_reset(s); }
static inline float pl_naive_svf_process(pl_naive_svf_t *s, int mode, float in)
{
    float hp, notch, bp_normalized;
    bp_normalized = s->bp * s->damp;
    notch = in - bp_normalized;
    s->lp += s->f * s->bp;
    hp = notch - s->lp;
    s->bp += s->f * hp;
    if (mode == PL_LP) return s->lp;
    if (mode == PL_BP) return s->bp;
    if (mode == PL_BP_NORMALIZED) return bp_normalized;
    return hp;
}
static inline void pl_naive_svf_process_buf(pl_naive_svf_t *s, int mode, const float *in, float *out, size_t size)
{
    float hp, notch, bp_normalized;
    float lp = s->lp;
    float bp = s->bp;
    while (size--) {
        bp_normalized = bp * s->damp;
        notch = *in++ - bp_normalized;
        lp += s->f * bp;
        hp = notch - lp;
        bp += s->f * hp;
        if (mode == PL_LP) *out++ = lp;
        else if (mode == PL_BP) *out++ = bp;
        else if (mode == PL_BP_NORMALIZED) *out++ = bp_normalized;
        else *out++ = hp;
    }
    s->lp = lp;
    s->bp = bp;
}
static inline void pl_flush_naive_svf(pl_naive_svf_t *s) { pl_flush(&s->lp); pl_flush(&s->bp); }

/* ---------------------------------- wavetable_oscillator.h Differentiator --- */
typedef struct { float lp, previous; } pl_diff_t;
static inline void pl_diff_init(pl_diff_t *d) { d->previous = 0.0f; d->lp = 0.0f; }
static inline float pl_diff_process(pl_diff_t *d, float coefficient, float s)
{
    PL_ONE_POLE(d->lp, s - d->previous, coefficient);
    d->previous = s;
    return d->lp;
}
static inline void pl_flush_diff(pl_diff_t *d) { pl_flush(&d->lp); pl_flush(&d->previous); }

/* InterpolateWave<int16_t> and InterpolateWaveHermite<int16_t> */
static inline float pl_interpolate_wave(const int16_t *table, int32_t index_integral, float index_fractional)
{
    float a = (float)table[index_integral];
    float b = (float)table[index_integral + 1];
    float t = index_fractional;
    return a + (b - a) * t;
}
static inline float pl_interpolate_wave_hermite(const int16_t *table, int32_t index_integral, float index_fractional)
{
    const float xm1 = (float)table[index_integral];
    const float x0 = (float)table[index_integral + 1];
    const float x1 = (float)table[index_integral + 2];
    const float x2 = (float)table[index_integral + 3];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = index_fractional;
    return (((a * f) - b_neg) * f + c) * f + x0;
}
