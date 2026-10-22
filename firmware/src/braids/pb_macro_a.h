/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the MacroOscillator shapes built on the analog oscillators (braids/macro_oscillator.cc) in C:
 *   RenderCSaw         pb_macro_render_csaw          CSAW
 *   RenderMorph        pb_macro_render_morph         MORPH
 *   RenderSawSquare    pb_macro_render_saw_square    SAW_SQUARE
 *   RenderSineTriangle pb_macro_render_sine_triangle SINE_TRIANGLE
 *   RenderBuzz         pb_macro_render_buzz          BUZZ
 *   RenderSub          pb_macro_render_sub           SQUARE_SUB, SAW_SUB
 *   RenderDualSync     pb_macro_render_dual_sync     SQUARE_SYNC, SAW_SYNC
 *   RenderTriple       pb_macro_render_triple        TRIPLE_SAW, TRIPLE_SQUARE, TRIPLE_TRIANGLE, TRIPLE_SINE
 * with the C++'s `intervals` table (the TRIPLE detune steps) as pbm_intervals. (In the Braids version this is
 * ported from, RenderTriple sets up its three oscillators itself; there is no separate ConfigureTriple.)
 * pb_macro.h includes this file after the state and before its dispatcher; it defines PB_MACRO_A so the dispatcher
 * knows these functions are here. The analog render functions they call are in pb_analog_r.h.
 *
 * Same integer arithmetic as the C++, so the parity test (tests/braids_parity) shows the same samples. The casts
 * spell out the C++'s implicit conversions (an int stored into an int16_t or uint16_t truncates). Nothing differs
 * on the device.
 *
 * Revisions:
 *   0.5.1: first version.
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

#define PB_MACRO_A 1

/* ------------------------------------------------------------------------------------------- RenderCSaw --- */
static void pb_macro_render_csaw(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a = &o->analog_oscillator[0];
    pb_analog_set_pitch(a, o->pitch);
    pb_analog_set_shape(a, PB_ANALOG_CSAW);
    pb_analog_set_parameter(a, o->parameter[0]);
    pb_analog_set_aux_parameter(a, o->parameter[1]);
    pb_analog_render(a, sync, buffer, NULL, size);
    int16_t shift = (int16_t)(-(o->parameter[1] - 32767) >> 4);
    while (size--) {
        int32_t s = *buffer + shift;
        *buffer++ = (int16_t)((s * 13) >> 3);
    }
}

/* ------------------------------------------------------------------------------------------ RenderMorph --- */
static void pb_macro_render_morph(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    pb_analog_set_pitch(a0, o->pitch);
    pb_analog_set_pitch(a1, o->pitch);

    uint16_t balance;
    if (o->parameter[0] <= 10922) {
        pb_analog_set_parameter(a0, 0);
        pb_analog_set_parameter(a1, 0);
        pb_analog_set_shape(a0, PB_ANALOG_TRIANGLE);
        pb_analog_set_shape(a1, PB_ANALOG_SAW);
        balance = (uint16_t)(o->parameter[0] * 6);
    } else if (o->parameter[0] <= 21845) {
        pb_analog_set_parameter(a0, 0);
        pb_analog_set_parameter(a1, 0);
        pb_analog_set_shape(a0, PB_ANALOG_SQUARE);
        pb_analog_set_shape(a1, PB_ANALOG_SAW);
        balance = (uint16_t)(65535 - (o->parameter[0] - 10923) * 6);
    } else {
        pb_analog_set_parameter(a0, (int16_t)((o->parameter[0] - 21846) * 3));
        pb_analog_set_parameter(a1, 0);
        pb_analog_set_shape(a0, PB_ANALOG_SQUARE);
        pb_analog_set_shape(a1, PB_ANALOG_SINE);
        balance = 0;
    }

    int16_t *shape_1 = buffer;
    int16_t *shape_2 = o->temp_buffer;
    pb_analog_render(a0, sync, shape_1, NULL, size);
    pb_analog_render(a1, sync, shape_2, NULL, size);

    int32_t lp_cutoff = o->pitch - (o->parameter[1] >> 1) + 128 * 128;
    if (lp_cutoff < 0) lp_cutoff = 0;
    else if (lp_cutoff > 32767) lp_cutoff = 32767;
    /* (lut_svf_cutoff is a uint16_t table: the uint16_t Interpolate824. The shift is done unsigned, which is what
     * the C++'s int32_t << 17 gives for these 0 .. 32767 values.) */
    int32_t f = pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)lp_cutoff << 17);
    int32_t lp_state = o->lp_state;
    int32_t fuzz_amount = o->parameter[1] << 1;
    if (o->pitch > (80 << 7)) {
        fuzz_amount -= (o->pitch - (80 << 7)) << 4;
        if (fuzz_amount < 0) fuzz_amount = 0;
    }
    while (size--) {
        int16_t sample = pb_mix_s16(*shape_1++, *shape_2++, balance);
        int32_t shifted_sample = sample;
        lp_state += (shifted_sample - lp_state) * f >> 15;
        PB_CLIP(lp_state);
        shifted_sample = lp_state + 32768;

        int16_t fuzzed = pb_interpolate88_s16(pb_ws_violent_overdrive, (uint16_t)shifted_sample);
        *buffer++ = pb_mix_s16(sample, fuzzed, (uint16_t)fuzz_amount);
    }
    o->lp_state = lp_state;
}

/* -------------------------------------------------------------------------------------- RenderSawSquare --- */
static void pb_macro_render_saw_square(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    pb_analog_set_parameter(a0, o->parameter[0]);
    pb_analog_set_parameter(a1, o->parameter[0]);
    pb_analog_set_pitch(a0, o->pitch);
    pb_analog_set_pitch(a1, o->pitch);

    pb_analog_set_shape(a0, PB_ANALOG_VARIABLE_SAW);
    pb_analog_set_shape(a1, PB_ANALOG_SQUARE);

    int16_t *saw_buffer = buffer;
    int16_t *square_buffer = o->temp_buffer;

    pb_analog_render(a0, sync, saw_buffer, NULL, size);
    pb_analog_render(a1, sync, square_buffer, NULL, size);

    PB_BEGIN_INTERPOLATE_PARAMETER_1(o)
    while (size--) {
        PB_INTERPOLATE_PARAMETER_1
        uint16_t balance = (uint16_t)(parameter_1 << 1);
        int16_t attenuated_square = (int16_t)((int32_t)*square_buffer++ * 148 >> 8);
        *buffer++ = pb_mix_s16(*saw_buffer++, attenuated_square, balance);
    }
    PB_END_INTERPOLATE_PARAMETER_1(o)
}

/* ------------------------------------------------------------------------------------------ RenderTriple --- */
#define PBM_SEMI * 128

/* intervals[65]: the detune of the second and third oscillators, by TIMBRE / COLOR (in 1/128 semitones) */
static const int16_t pbm_intervals[65] = {
    -24 PBM_SEMI, -24 PBM_SEMI, -24 PBM_SEMI + 4,
    -23 PBM_SEMI, -22 PBM_SEMI, -21 PBM_SEMI, -20 PBM_SEMI, -19 PBM_SEMI, -18 PBM_SEMI,
    -17 PBM_SEMI - 4, -17 PBM_SEMI,
    -16 PBM_SEMI, -15 PBM_SEMI, -14 PBM_SEMI, -13 PBM_SEMI,
    -12 PBM_SEMI - 4, -12 PBM_SEMI,
    -11 PBM_SEMI, -10 PBM_SEMI, -9 PBM_SEMI, -8 PBM_SEMI,
    -7 PBM_SEMI - 4, -7 PBM_SEMI,
    -6 PBM_SEMI, -5 PBM_SEMI, -4 PBM_SEMI, -3 PBM_SEMI, -2 PBM_SEMI, -1 PBM_SEMI,
    -24, -8, -4, 0, 4, 8, 24,
    1 PBM_SEMI, 2 PBM_SEMI, 3 PBM_SEMI, 4 PBM_SEMI, 5 PBM_SEMI, 6 PBM_SEMI,
    7 PBM_SEMI, 7 PBM_SEMI + 4,
    8 PBM_SEMI, 9 PBM_SEMI, 10 PBM_SEMI, 11 PBM_SEMI,
    12 PBM_SEMI, 12 PBM_SEMI + 4,
    13 PBM_SEMI, 14 PBM_SEMI, 15 PBM_SEMI, 16 PBM_SEMI,
    17 PBM_SEMI, 17 PBM_SEMI + 4,
    18 PBM_SEMI, 19 PBM_SEMI, 20 PBM_SEMI, 21 PBM_SEMI, 22 PBM_SEMI, 23 PBM_SEMI,
    24 PBM_SEMI - 4, 24 PBM_SEMI, 24 PBM_SEMI
};

#undef PBM_SEMI

static void pb_macro_render_triple(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int base_shape;
    switch (o->shape) {
    case PB_SHAPE_TRIPLE_SAW: base_shape = PB_ANALOG_SAW; break;
    case PB_SHAPE_TRIPLE_TRIANGLE: base_shape = PB_ANALOG_TRIANGLE; break;
    case PB_SHAPE_TRIPLE_SQUARE: base_shape = PB_ANALOG_SQUARE; break;
    default: base_shape = PB_ANALOG_SINE; break;
    }

    pb_analog_set_parameter(&o->analog_oscillator[0], 0);
    pb_analog_set_parameter(&o->analog_oscillator[1], 0);
    pb_analog_set_parameter(&o->analog_oscillator[2], 0);

    pb_analog_set_pitch(&o->analog_oscillator[0], o->pitch);
    for (size_t i = 0; i < 2; ++i) {
        int16_t detune_1 = pbm_intervals[o->parameter[i] >> 9];
        int16_t detune_2 = pbm_intervals[((o->parameter[i] >> 8) + 1) >> 1];
        uint16_t xfade = (uint16_t)(o->parameter[i] << 8);
        int16_t detune = (int16_t)(detune_1 + ((detune_2 - detune_1) * xfade >> 16));
        pb_analog_set_pitch(&o->analog_oscillator[i + 1], (int16_t)(o->pitch + detune));
    }

    pb_analog_set_shape(&o->analog_oscillator[0], base_shape);
    pb_analog_set_shape(&o->analog_oscillator[1], base_shape);
    pb_analog_set_shape(&o->analog_oscillator[2], base_shape);

    for (size_t j = 0; j < size; ++j) buffer[j] = 0;   /* (std::fill) */
    for (size_t i = 0; i < 3; ++i) {
        pb_analog_render(&o->analog_oscillator[i], sync, o->temp_buffer, NULL, size);
        for (size_t j = 0; j < size; ++j) buffer[j] = (int16_t)(buffer[j] + (o->temp_buffer[j] * 21 >> 6));
    }
}

/* -------------------------------------------------------------------------------------------- RenderSub --- */
static void pb_macro_render_sub(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    int base_shape = o->shape == PB_SHAPE_SQUARE_SUB ? PB_ANALOG_SQUARE : PB_ANALOG_VARIABLE_SAW;
    pb_analog_set_parameter(a0, o->parameter[0]);
    pb_analog_set_shape(a0, base_shape);
    pb_analog_set_pitch(a0, o->pitch);

    pb_analog_set_parameter(a1, 0);
    pb_analog_set_shape(a1, PB_ANALOG_SQUARE);
    int16_t octave = o->parameter[1] < 16384 ? (24 << 7) : (12 << 7);
    pb_analog_set_pitch(a1, (int16_t)(o->pitch - octave));

    pb_analog_render(a0, sync, buffer, NULL, size);
    pb_analog_render(a1, sync, o->temp_buffer, NULL, size);

    PB_BEGIN_INTERPOLATE_PARAMETER_1(o)

    int16_t *temp_buffer = o->temp_buffer;
    while (size--) {
        PB_INTERPOLATE_PARAMETER_1
        uint16_t sub_gain = (uint16_t)((parameter_1 < 16384 ? (16383 - parameter_1) : (parameter_1 - 16384)) << 1);
        *buffer = pb_mix_s16(*buffer, *temp_buffer, sub_gain);
        buffer++;
        temp_buffer++;
    }

    PB_END_INTERPOLATE_PARAMETER_1(o)
}

/* --------------------------------------------------------------------------------------- RenderDualSync --- */
static void pb_macro_render_dual_sync(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    int base_shape = o->shape == PB_SHAPE_SQUARE_SYNC ? PB_ANALOG_SQUARE : PB_ANALOG_SAW;
    pb_analog_set_parameter(a0, 0);
    pb_analog_set_shape(a0, base_shape);
    pb_analog_set_pitch(a0, o->pitch);

    pb_analog_set_parameter(a1, 0);
    pb_analog_set_shape(a1, base_shape);
    pb_analog_set_pitch(a1, (int16_t)(o->pitch + (o->parameter[0] >> 2)));

    /* the first oscillator's wraps (sync_buffer) hard sync the second */
    pb_analog_render(a0, sync, buffer, o->sync_buffer, size);
    pb_analog_render(a1, o->sync_buffer, o->temp_buffer, NULL, size);

    PB_BEGIN_INTERPOLATE_PARAMETER_1(o)

    int16_t *temp_buffer = o->temp_buffer;
    while (size--) {
        PB_INTERPOLATE_PARAMETER_1
        uint16_t balance = (uint16_t)(parameter_1 << 1);

        *buffer = (int16_t)((pb_mix_s16(*buffer, *temp_buffer, balance) >> 2) * 3);
        buffer++;
        temp_buffer++;
    }

    PB_END_INTERPOLATE_PARAMETER_1(o)
}

/* ----------------------------------------------------------------------------------- RenderSineTriangle --- */
static void pb_macro_render_sine_triangle(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    int32_t attenuation_sine = 32767 - 6 * (o->pitch - (92 << 7));
    int32_t attenuation_tri = 32767 - 7 * (o->pitch - (80 << 7));
    if (attenuation_tri < 0) attenuation_tri = 0;
    if (attenuation_sine < 0) attenuation_sine = 0;
    if (attenuation_tri > 32767) attenuation_tri = 32767;
    if (attenuation_sine > 32767) attenuation_sine = 32767;

    int32_t timbre = o->parameter[0];

    pb_analog_set_parameter(a0, (int16_t)(timbre * attenuation_sine >> 15));
    pb_analog_set_parameter(a1, (int16_t)(timbre * attenuation_tri >> 15));
    pb_analog_set_pitch(a0, o->pitch);
    pb_analog_set_pitch(a1, o->pitch);

    pb_analog_set_shape(a0, PB_ANALOG_SINE_FOLD);
    pb_analog_set_shape(a1, PB_ANALOG_TRIANGLE_FOLD);

    pb_analog_render(a0, sync, buffer, NULL, size);
    pb_analog_render(a1, sync, o->temp_buffer, NULL, size);

    int16_t *temp_buffer = o->temp_buffer;

    PB_BEGIN_INTERPOLATE_PARAMETER_1(o)

    while (size--) {
        PB_INTERPOLATE_PARAMETER_1
        uint16_t balance = (uint16_t)(parameter_1 << 1);

        *buffer = pb_mix_s16(*buffer, *temp_buffer, balance);
        buffer++;
        temp_buffer++;
    }

    PB_END_INTERPOLATE_PARAMETER_1(o)
}

/* ------------------------------------------------------------------------------------------- RenderBuzz --- */
static void pb_macro_render_buzz(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_t *a0 = &o->analog_oscillator[0];
    pb_analog_t *a1 = &o->analog_oscillator[1];
    pb_analog_set_parameter(a0, o->parameter[0]);
    pb_analog_set_shape(a0, PB_ANALOG_BUZZ);
    pb_analog_set_pitch(a0, o->pitch);

    pb_analog_set_parameter(a1, o->parameter[0]);
    pb_analog_set_shape(a1, PB_ANALOG_BUZZ);
    pb_analog_set_pitch(a1, (int16_t)(o->pitch + (o->parameter[1] >> 8)));

    pb_analog_render(a0, sync, buffer, NULL, size);
    pb_analog_render(a1, sync, o->temp_buffer, NULL, size);
    int16_t *temp_buffer = o->temp_buffer;
    while (size--) {
        *buffer = (int16_t)(*buffer >> 1);
        *buffer = (int16_t)(*buffer + (*temp_buffer >> 1));
        buffer++;
        temp_buffer++;
    }
}
