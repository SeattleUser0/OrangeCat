/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 2, WAVESHAPING (plaits/dsp/engine/waveshaping_engine.cc), in C.
 *   OUT: a bandlimited slope (MORPH: its width) through a waveshaper (HARMONICS picks and blends the lut_ws_* curves)
 *        and the lut_fold wavefolder (TIMBRE: the folder's gain).
 *   AUX: a triangle turned into a sine, crossfaded with the second folder curve (lut_fold_2) by TIMBRE.
 * Same on the device as in the C++ (no buffers of its own).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

typedef struct {
    pl_osc_t slope, triangle;
    float previous_shape, previous_wavefolder_gain, previous_overtone_gain;
} pl_ws_t;

static void pl_ws_init(pl_ws_t *e)
{
    pl_osc_init(&e->slope);
    pl_osc_init(&e->triangle);
    e->previous_shape = 0.0f;
    e->previous_wavefolder_gain = 0.0f;
    e->previous_overtone_gain = 0.0f;
}
static void pl_ws_reset(pl_ws_t *e) { (void)e; }

/* lookup_table_i16_table[] in resources.cc (its last curve is there twice: shape_integral + 1 can reach 4). (Orange Cat 0.13.8) An engine may read
 * the curves from its hot set instead: it defines PL_WS_CURVE(k) first (eng_macro.c) */
#ifndef PL_WS_CURVE
static const int16_t *const pl_ws_curves[6] = {
    pl_lut_ws_inverse_tan, pl_lut_ws_inverse_sin, pl_lut_ws_linear, pl_lut_ws_bump, pl_lut_ws_double_bump,
    pl_lut_ws_double_bump
};

#define PL_WS_CURVE(k) pl_ws_curves[k]
#endif

/* Tame(): how much of a control's range to keep before the spectrum aliases */
static inline float pl_ws_tame(float f0, float harmonics, float order)
{
    f0 *= harmonics;
    float max_f = 0.5f / order;
    float max_amount = 1.0f - (f0 - max_f) / (0.5f - max_f);
    PL_CONSTRAIN(max_amount, 0.0f, 1.0f);
    return max_amount * max_amount * max_amount;
}

static __attribute__((section(".text.xip_macro"))) void pl_ws_render(pl_ws_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float root = p->note;
    const float f0 = pl_note_to_frequency(root);
    const float pw = p->morph * 0.45f + 0.5f;

    /* start from bandlimited slope signals */
    pl_osc_render(&e->slope, PL_SHAPE_SLOPE, f0, pw, 0, out, size);
    pl_osc_render(&e->triangle, PL_SHAPE_SLOPE, f0, 0.5f, 0, aux, size);

    /* estimate how rich the spectrum is and reduce the range of the waveshaping control accordingly */
    const float slope = 3.0f + pl_fabsf(p->morph - 0.5f) * 5.0f;
    const float shape_amount = pl_fabsf(p->harmonics - 0.5f) * 2.0f;
    const float shape_amount_attenuation = pl_ws_tame(f0, slope, 16.0f);
    const float wavefolder_gain = p->timbre;
    const float wavefolder_gain_attenuation = pl_ws_tame(
        f0, slope * (3.0f + shape_amount * shape_amount_attenuation * 5.0f), 12.0f);

    pl_interp_t shape_modulation, wf_gain_modulation, overtone_gain_modulation;
    pl_interp_init(&shape_modulation, &e->previous_shape,
                   0.5f + (p->harmonics - 0.5f) * shape_amount_attenuation, size);
    pl_interp_init(&wf_gain_modulation, &e->previous_wavefolder_gain,
                   0.03f + 0.46f * wavefolder_gain * wavefolder_gain_attenuation, size);
    const float overtone_gain = p->timbre * (2.0f - p->timbre);
    pl_interp_init(&overtone_gain_modulation, &e->previous_overtone_gain,
                   overtone_gain * (2.0f - overtone_gain), size);

    for (size_t i = 0; i < size; ++i) {
        float shape = pl_interp_next(&shape_modulation) * 3.9999f;
        PL_MAKE_INTEGRAL_FRACTIONAL(shape)
        const int16_t *shape_1 = PL_WS_CURVE(shape_integral);
        const int16_t *shape_2 = PL_WS_CURVE(shape_integral + 1);

        float ws_index = 127.0f * out[i] + 128.0f;
        PL_MAKE_INTEGRAL_FRACTIONAL(ws_index)
        ws_index_integral &= 255;

        float x0 = (float)shape_1[ws_index_integral] / 32768.0f;
        float x1 = (float)shape_1[ws_index_integral + 1] / 32768.0f;
        float x = x0 + (x1 - x0) * ws_index_fractional;

        float y0 = (float)shape_2[ws_index_integral] / 32768.0f;
        float y1 = (float)shape_2[ws_index_integral + 1] / 32768.0f;
        float y = y0 + (y1 - y0) * ws_index_fractional;

        float mix = x + (y - x) * shape_fractional;
        float index = mix * pl_interp_next(&wf_gain_modulation) + 0.5f;
        float fold = pl_interpolate_hermite(pl_lut_fold + 1, index, 512.0f);
        float fold_2 = -pl_interpolate_hermite(pl_lut_fold_2 + 1, index, 512.0f);

        float sine = pl_sine(aux[i] * 0.25f + 0.5f);
        out[i] = fold;
        aux[i] = sine + (fold_2 - sine) * pl_interp_next(&overtone_gain_modulation);
    }
    /* (the three ParameterInterpolators go out of scope here) */
    pl_interp_done(&shape_modulation);
    pl_interp_done(&wf_gain_modulation);
    pl_interp_done(&overtone_gain_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_ws_flush(pl_ws_t *e) { pl_flush(&e->slope.lp_state); pl_flush(&e->triangle.lp_state); pl_flush(&e->slope.hp_state); pl_flush(&e->triangle.hp_state); }
