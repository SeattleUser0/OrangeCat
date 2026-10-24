/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the render functions of Braids' AnalogOscillator (braids/analog_oscillator.cc, .h) in C:
 *   RenderSaw          pb_analog_render_saw
 *   RenderVariableSaw  pb_analog_render_variable_saw
 *   RenderCSaw         pb_analog_render_csaw
 *   RenderSquare       pb_analog_render_square
 *   RenderTriangle     pb_analog_render_triangle
 *   RenderSine         pb_analog_render_sine
 *   RenderTriangleFold pb_analog_render_triangle_fold
 *   RenderSineFold     pb_analog_render_sine_fold
 *   RenderBuzz         pb_analog_render_buzz
 * and the header's ThisBlepSample / NextBlepSample as pba_this_blep / pba_next_blep.
 * pb_analog.h includes this file after the state and before its dispatcher (pb_analog_render). This file defines
 * PB_ANALOG_R so the dispatcher knows the render functions are here.
 *
 * The arithmetic is the C++'s, line for line, with the same integer types, so the parity test
 * (tests/braids_parity) shows the same samples. Where the C++ mixes signed and unsigned (a ternary of an int16_t and
 * a uint32_t, an int32_t += uint32_t), C's usual conversions are the same as C++'s, and the casts written here only
 * spell out what the C++ does implicitly.
 *
 * Difference from the C++ on the device: the band limited step code divides by (phase_increment >> 16) and
 * (phase_increment >> 7). Those are 0 only far below the playable range (pitch under about -3600, some 28
 * semitones below MIDI note 0, which a SUB or TRIPLE detune can reach only from a pitch that low). On the STM32 the
 * original runs on, an unsigned divide by zero gives 0; on a host it traps. pba_div gives 0 for a zero divisor, the
 * same as the original hardware, so a stray low pitch cannot crash the AC791N. Inside the range it is the plain
 * division, so the parity test is unaffected.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.5.1: first version.
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

#define PB_ANALOG_R 1

/* ThisBlepSample / NextBlepSample (analog_oscillator.h): the two halves of the polynomial band limited step */
static inline int32_t pba_this_blep(uint32_t t)
{
    if (t > 65535) t = 65535;
    return (int32_t)(t * t >> 18);
}
static inline int32_t pba_next_blep(uint32_t t)
{
    if (t > 65535) t = 65535;
    t = 65535 - t;
    return -(int32_t)(t * t >> 18);
}

/* unsigned divide that gives 0 for a zero divisor, as the Cortex-M4's UDIV does (see the header comment) */
static inline uint32_t pba_div(uint32_t a, uint32_t b) { return b ? a / b : 0u; }

/* BEGIN_INTERPOLATE_PARAMETER of parameter_interpolation.h, on the analog oscillator's own parameter /
 * previous_parameter fields (pb_dsp.h's version names them parameter_single) */
#define PBA_BEGIN_INTERPOLATE_PARAMETER(o) \
    int32_t parameter_start = (o)->previous_parameter; \
    int32_t parameter_delta = (o)->parameter - (o)->previous_parameter; \
    int32_t parameter_increment = 32767 / (int32_t)size; \
    int32_t parameter_xfade = 0;
#define PBA_INTERPOLATE_PARAMETER \
    parameter_xfade += parameter_increment; \
    int32_t parameter = parameter_start + (parameter_delta * parameter_xfade >> 15);
#define PBA_END_INTERPOLATE_PARAMETER(o) \
    (o)->previous_parameter = (o)->parameter;

/* the sync output of the saw, square and CSAW: the fractional position of a wrap in this sample, or 0 */
#define PBA_SYNC_OUT \
    if (sync_out) { \
        if (o->phase < phase_increment) *sync_out++ = (uint8_t)(pba_div(o->phase, phase_increment >> 7) + 1); \
        else *sync_out++ = 0; \
    }

/* ------------------------------------------------------------------------------------------- RenderCSaw --- */
static void pb_analog_render_csaw(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                  size_t size)
{
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    int32_t next_sample = o->next_sample;
    while (size--) {
        int sync_reset = 0;
        int self_reset = 0;
        int transition_during_reset = 0;
        uint32_t reset_time = 0;
        PB_INTERPOLATE_PHASE_INCREMENT
        uint32_t pw = (uint32_t)o->parameter * 49152u;
        if (pw < 8 * phase_increment) pw = 8 * phase_increment;

        int32_t this_sample = next_sample;
        next_sample = 0;

        if (*sync_in) {
            /* sync_in contains the fractional reset time */
            reset_time = (uint32_t)(*sync_in - 1) << 9;
            uint32_t phase_at_reset = o->phase + (65535 - reset_time) * (phase_increment >> 16);
            sync_reset = 1;
            transition_during_reset = 0;
            if (phase_at_reset < o->phase || (!o->high && phase_at_reset >= pw)) transition_during_reset = 1;
            if (o->phase >= pw) {
                o->discontinuity_depth = (int16_t)(-2048 + (o->aux_parameter >> 2));
                int32_t before = (int32_t)(phase_at_reset >> 18);
                int16_t after = o->discontinuity_depth;
                int32_t discontinuity = after - before;
                this_sample += discontinuity * pba_this_blep(reset_time) >> 15;
                next_sample += discontinuity * pba_next_blep(reset_time) >> 15;
            }
        }
        sync_in++;

        o->phase += phase_increment;
        if (o->phase < phase_increment) self_reset = 1;
        PBA_SYNC_OUT

        while (transition_during_reset || !sync_reset) {
            if (!o->high) {
                if (o->phase < pw) break;
                uint32_t t = pba_div(o->phase - pw, phase_increment >> 16);
                int16_t before = o->discontinuity_depth;
                int16_t after = (int16_t)(o->phase >> 18);
                int16_t discontinuity = (int16_t)(after - before);
                this_sample += discontinuity * pba_this_blep(t) >> 15;
                next_sample += discontinuity * pba_next_blep(t) >> 15;
                o->high = 1;
            }
            if (o->high) {
                if (!self_reset) break;
                self_reset = 0;
                o->discontinuity_depth = (int16_t)(-2048 + (o->aux_parameter >> 2));
                uint32_t t = pba_div(o->phase, phase_increment >> 16);
                int16_t before = 16383;
                int16_t after = o->discontinuity_depth;
                int16_t discontinuity = (int16_t)(after - before);
                this_sample += discontinuity * pba_this_blep(t) >> 15;
                next_sample += discontinuity * pba_next_blep(t) >> 15;
                o->high = 0;
            }
        }

        if (sync_reset) {
            o->phase = reset_time * (phase_increment >> 16);
            o->high = 0;
        }

        /* (the C++'s ternary has an int16_t and a uint32_t arm: its type is uint32_t) */
        next_sample = (int32_t)((uint32_t)next_sample +
                                (o->phase < pw ? (uint32_t)o->discontinuity_depth : o->phase >> 18));
        *buffer++ = (int16_t)((this_sample - 8192) * 2);
    }
    o->next_sample = next_sample;
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
}

/* ----------------------------------------------------------------------------------------- RenderSquare --- */
static void pb_analog_render_square(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                    size_t size)
{
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    if (o->parameter > 32000) o->parameter = 32000;

    int32_t next_sample = o->next_sample;
    while (size--) {
        int sync_reset = 0;
        int self_reset = 0;
        int transition_during_reset = 0;
        uint32_t reset_time = 0;

        PB_INTERPOLATE_PHASE_INCREMENT
        uint32_t pw = (uint32_t)(32768 - o->parameter) << 16;

        int32_t this_sample = next_sample;
        next_sample = 0;

        if (*sync_in) {
            /* sync_in contains the fractional reset time */
            reset_time = (uint32_t)(*sync_in - 1) << 9;
            uint32_t phase_at_reset = o->phase + (65535 - reset_time) * (phase_increment >> 16);
            sync_reset = 1;
            if (phase_at_reset < o->phase || (!o->high && phase_at_reset >= pw)) transition_during_reset = 1;
            if (phase_at_reset >= pw) {
                this_sample -= pba_this_blep(reset_time);
                next_sample -= pba_next_blep(reset_time);
            }
        }
        sync_in++;

        o->phase += phase_increment;
        if (o->phase < phase_increment) self_reset = 1;
        PBA_SYNC_OUT

        while (transition_during_reset || !sync_reset) {
            if (!o->high) {
                if (o->phase < pw) break;
                uint32_t t = pba_div(o->phase - pw, phase_increment >> 16);
                this_sample += pba_this_blep(t);
                next_sample += pba_next_blep(t);
                o->high = 1;
            }
            if (o->high) {
                if (!self_reset) break;
                self_reset = 0;
                uint32_t t = pba_div(o->phase, phase_increment >> 16);
                this_sample -= pba_this_blep(t);
                next_sample -= pba_next_blep(t);
                o->high = 0;
            }
        }

        if (sync_reset) {
            o->phase = reset_time * (phase_increment >> 16);
            o->high = 0;
        }

        next_sample += o->phase < pw ? 0 : 32767;
        *buffer++ = (int16_t)((this_sample - 16384) * 2);
    }
    o->next_sample = next_sample;
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
}

/* -------------------------------------------------------------------------------------------- RenderSaw --- */
static void pb_analog_render_saw(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                 size_t size)
{
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    int32_t next_sample = o->next_sample;
    while (size--) {
        int sync_reset = 0;
        int self_reset = 0;
        int transition_during_reset = 0;
        uint32_t reset_time = 0;

        PB_INTERPOLATE_PHASE_INCREMENT
        int32_t this_sample = next_sample;
        next_sample = 0;

        if (*sync_in) {
            /* sync_in contains the fractional reset time */
            reset_time = (uint32_t)(*sync_in - 1) << 9;
            uint32_t phase_at_reset = o->phase + (65535 - reset_time) * (phase_increment >> 16);
            sync_reset = 1;
            if (phase_at_reset < o->phase) transition_during_reset = 1;
            int32_t discontinuity = (int32_t)(phase_at_reset >> 17);
            this_sample -= discontinuity * pba_this_blep(reset_time) >> 15;
            next_sample -= discontinuity * pba_next_blep(reset_time) >> 15;
        }
        sync_in++;

        o->phase += phase_increment;
        if (o->phase < phase_increment) self_reset = 1;
        PBA_SYNC_OUT

        if ((transition_during_reset || !sync_reset) && self_reset) {
            uint32_t t = pba_div(o->phase, phase_increment >> 16);
            this_sample -= pba_this_blep(t);
            next_sample -= pba_next_blep(t);
        }

        if (sync_reset) {
            o->phase = reset_time * (phase_increment >> 16);
            o->high = 0;
        }

        next_sample += (int32_t)(o->phase >> 17);
        *buffer++ = (int16_t)((this_sample - 16384) * 2);
    }
    o->next_sample = next_sample;
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
}

/* ------------------------------------------------------------------------------------ RenderVariableSaw --- */
static void pb_analog_render_variable_saw(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer,
                                          uint8_t *sync_out, size_t size)
{
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    int32_t next_sample = o->next_sample;
    if (o->parameter < 1024) o->parameter = 1024;
    while (size--) {
        int sync_reset = 0;
        int self_reset = 0;
        int transition_during_reset = 0;
        uint32_t reset_time = 0;

        PB_INTERPOLATE_PHASE_INCREMENT
        uint32_t pw = (uint32_t)o->parameter << 16;

        int32_t this_sample = next_sample;
        next_sample = 0;

        if (*sync_in) {
            /* sync_in contains the fractional reset time */
            reset_time = (uint32_t)(*sync_in - 1) << 9;
            uint32_t phase_at_reset = o->phase + (65535 - reset_time) * (phase_increment >> 16);
            sync_reset = 1;
            if (phase_at_reset < o->phase || (!o->high && phase_at_reset >= pw)) transition_during_reset = 1;
            int32_t before = (int32_t)((phase_at_reset >> 18) + ((phase_at_reset - pw) >> 18));
            int32_t after = (int32_t)((0 >> 18) + ((0u - pw) >> 18));
            int32_t discontinuity = after - before;
            this_sample += discontinuity * pba_this_blep(reset_time) >> 15;
            next_sample += discontinuity * pba_next_blep(reset_time) >> 15;
        }
        sync_in++;

        o->phase += phase_increment;
        if (o->phase < phase_increment) self_reset = 1;
        PBA_SYNC_OUT

        while (transition_during_reset || !sync_reset) {
            if (!o->high) {
                if (o->phase < pw) break;
                uint32_t t = pba_div(o->phase - pw, phase_increment >> 16);
                this_sample -= pba_this_blep(t) >> 1;
                next_sample -= pba_next_blep(t) >> 1;
                o->high = 1;
            }
            if (o->high) {
                if (!self_reset) break;
                self_reset = 0;
                uint32_t t = pba_div(o->phase, phase_increment >> 16);
                this_sample -= pba_this_blep(t) >> 1;
                next_sample -= pba_next_blep(t) >> 1;
                o->high = 0;
            }
        }

        if (sync_reset) {
            o->phase = reset_time * (phase_increment >> 16);
            o->high = 0;
        }

        next_sample += (int32_t)(o->phase >> 18);
        next_sample += (int32_t)((o->phase - pw) >> 18);
        *buffer++ = (int16_t)((this_sample - 16384) << 1);
    }
    o->next_sample = next_sample;
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
}

/* one triangle step of RenderTriangle / RenderTriangleFold: the int16_t the C++ computes from phase_16 */
static inline int16_t pba_triangle(uint32_t phase)
{
    uint16_t phase_16 = (uint16_t)(phase >> 16);
    int16_t triangle = (int16_t)((phase_16 << 1) ^ (phase_16 & 0x8000 ? 0xffff : 0x0000));
    triangle = (int16_t)(triangle + 32768);
    return triangle;
}

/* --------------------------------------------------------------------------------------- RenderTriangle --- */
static void pb_analog_render_triangle(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                      size_t size)
{
    (void)sync_out;
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    uint32_t phase = o->phase;
    while (size--) {
        PB_INTERPOLATE_PHASE_INCREMENT
        int16_t triangle;

        if (*sync_in++) phase = 0;

        phase += phase_increment >> 1;
        triangle = pba_triangle(phase);
        *buffer = (int16_t)(triangle >> 1);

        phase += phase_increment >> 1;
        triangle = pba_triangle(phase);
        *buffer = (int16_t)(*buffer + (triangle >> 1));
        buffer++;
    }
    o->phase = phase;
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
}

/* ------------------------------------------------------------------------------------------- RenderSine --- */
static void pb_analog_render_sine(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                  size_t size)
{
    (void)sync_out;
    uint32_t phase = o->phase;
    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    while (size--) {
        PB_INTERPOLATE_PHASE_INCREMENT
        phase += phase_increment;
        if (*sync_in++) phase = 0;
        *buffer++ = pb_interpolate824_s16(pb_wav_sine, phase);
    }
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)
    o->phase = phase;
}

/* ----------------------------------------------------------------------------------- RenderTriangleFold --- */
static void pb_analog_render_triangle_fold(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer,
                                           uint8_t *sync_out, size_t size)
{
    (void)sync_out;
    uint32_t phase = o->phase;

    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    PBA_BEGIN_INTERPOLATE_PARAMETER(o)

    while (size--) {
        PBA_INTERPOLATE_PARAMETER
        PB_INTERPOLATE_PHASE_INCREMENT

        int16_t triangle;
        int16_t gain = (int16_t)(2048 + (parameter * 30720 >> 15));

        if (*sync_in++) phase = 0;

        /* 2x oversampled wavefolder */
        phase += phase_increment >> 1;
        triangle = pba_triangle(phase);
        triangle = (int16_t)(triangle * gain >> 15);
        triangle = pb_interpolate88_s16(pb_ws_tri_fold, (uint16_t)(triangle + 32768));
        *buffer = (int16_t)(triangle >> 1);

        phase += phase_increment >> 1;
        triangle = pba_triangle(phase);
        triangle = (int16_t)(triangle * gain >> 15);
        triangle = pb_interpolate88_s16(pb_ws_tri_fold, (uint16_t)(triangle + 32768));
        *buffer = (int16_t)(*buffer + (triangle >> 1));
        buffer++;
    }

    PBA_END_INTERPOLATE_PARAMETER(o)
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)

    o->phase = phase;
}

/* --------------------------------------------------------------------------------------- RenderSineFold --- */
static void pb_analog_render_sine_fold(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                       size_t size)
{
    (void)sync_out;
    uint32_t phase = o->phase;

    PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o)
    PBA_BEGIN_INTERPOLATE_PARAMETER(o)

    while (size--) {
        PBA_INTERPOLATE_PARAMETER
        PB_INTERPOLATE_PHASE_INCREMENT

        int16_t sine;
        int16_t gain = (int16_t)(2048 + (parameter * 30720 >> 15));

        if (*sync_in++) phase = 0;

        /* 2x oversampled wavefolder */
        phase += phase_increment >> 1;
        sine = pb_interpolate824_s16(pb_wav_sine, phase);
        sine = (int16_t)(sine * gain >> 15);
        sine = pb_interpolate88_s16(pb_ws_sine_fold, (uint16_t)(sine + 32768));
        *buffer = (int16_t)(sine >> 1);

        phase += phase_increment >> 1;
        sine = pb_interpolate824_s16(pb_wav_sine, phase);
        sine = (int16_t)(sine * gain >> 15);
        sine = pb_interpolate88_s16(pb_ws_sine_fold, (uint16_t)(sine + 32768));
        *buffer = (int16_t)(*buffer + (sine >> 1));
        buffer++;
    }

    PBA_END_INTERPOLATE_PARAMETER(o)
    PB_END_INTERPOLATE_PHASE_INCREMENT(o)

    o->phase = phase;
}

/* ------------------------------------------------------------------------------------------- RenderBuzz --- */
/* (uses phase_increment_ as computed, without the interpolation, and leaves previous_phase_increment_ alone,
 * as the C++ does) */
static void pb_analog_render_buzz(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
                                  size_t size)
{
    (void)sync_out;
    int32_t shifted_pitch = o->pitch + ((32767 - o->parameter) >> 1);
    uint16_t crossfade = (uint16_t)(shifted_pitch << 6);
    size_t index = (size_t)(shifted_pitch >> 10);
    if (index >= PB_ANALOG_NUM_ZONES) index = PB_ANALOG_NUM_ZONES - 1;
    const int16_t *wave_1 = pb_wav_bandlimited_comb[index];
    index += 1;
    if (index >= PB_ANALOG_NUM_ZONES) index = PB_ANALOG_NUM_ZONES - 1;
    const int16_t *wave_2 = pb_wav_bandlimited_comb[index];
    while (size--) {
        o->phase += o->phase_increment;
        if (*sync_in++) o->phase = 0;
        *buffer++ = pb_crossfade_s16(wave_1, wave_2, o->phase, crossfade);
    }
}
