/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: Braids' AnalogOscillator (braids/analog_oscillator.h, .cc) in C. This file has the state, Init,
 * the setters, ComputePhaseIncrement and Render (the dispatch); the render functions (RenderSaw ... RenderBuzz, and
 * ThisBlepSample / NextBlepSample) are in pb_analog_r.h, which defines PB_ANALOG_R. A render function is
 * `static void pb_analog_render_<name>(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out,
 * size_t size)`.
 *
 * Revisions:
 *   0.5.1: first version.
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

enum {
    PB_ANALOG_SAW, PB_ANALOG_VARIABLE_SAW, PB_ANALOG_CSAW, PB_ANALOG_SQUARE, PB_ANALOG_TRIANGLE, PB_ANALOG_SINE,
    PB_ANALOG_TRIANGLE_FOLD, PB_ANALOG_SINE_FOLD, PB_ANALOG_BUZZ
};
#define PB_ANALOG_NUM_ZONES 15u                         /* kNumZones */
#define PB_ANALOG_HIGHEST_NOTE (128 * 128)              /* (the analog oscillator's own kHighestNote) */
#define PB_ANALOG_PITCH_TABLE_START (128 * 128)
#define PB_ANALOG_OCTAVE (12 * 128)

typedef struct {
    uint32_t phase, phase_increment, previous_phase_increment;
    int high;
    int16_t parameter, previous_parameter, aux_parameter, discontinuity_depth, pitch;
    int32_t next_sample;
    int shape, previous_shape;
} pb_analog_t;

static inline void pb_analog_init(pb_analog_t *o)
{
    o->phase = 0;
    o->phase_increment = 1;
    o->high = 0;
    o->parameter = o->previous_parameter = 0;
    o->aux_parameter = 0;
    o->discontinuity_depth = -16383;
    o->pitch = 60 << 7;
    o->next_sample = 0;
}
static inline void pb_analog_set_shape(pb_analog_t *o, int shape) { o->shape = shape; }
static inline void pb_analog_set_pitch(pb_analog_t *o, int16_t pitch) { o->pitch = pitch; }
static inline void pb_analog_set_parameter(pb_analog_t *o, int16_t parameter) { o->parameter = parameter; }
static inline void pb_analog_set_aux_parameter(pb_analog_t *o, int16_t parameter) { o->aux_parameter = parameter; }
static inline uint32_t pb_analog_phase_increment(const pb_analog_t *o) { return o->phase_increment; }
static inline void pb_analog_reset(pb_analog_t *o) { o->phase = 0u - o->phase_increment; }   /* (phase_ = -phase_increment_) */

static inline uint32_t pb_analog_compute_phase_increment(int16_t midi_pitch)
{
    if (midi_pitch >= PB_ANALOG_HIGHEST_NOTE) midi_pitch = PB_ANALOG_HIGHEST_NOTE - 1;
    int32_t ref_pitch = midi_pitch;
    ref_pitch -= PB_ANALOG_PITCH_TABLE_START;
    size_t num_shifts = 0;
    while (ref_pitch < 0) {
        ref_pitch += PB_ANALOG_OCTAVE;
        ++num_shifts;
    }
    uint32_t a = pb_lut_oscillator_increments[ref_pitch >> 4];
    uint32_t b = pb_lut_oscillator_increments[(ref_pitch >> 4) + 1];
    uint32_t phase_increment = a + (uint32_t)((int32_t)(b - a) * (ref_pitch & 0xf) >> 4);
    phase_increment >>= num_shifts;
    return phase_increment;
}

#if __has_include("pb_analog_r.h")
#include "pb_analog_r.h"
#endif

static void pb_analog_render(pb_analog_t *o, const uint8_t *sync_in, int16_t *buffer, uint8_t *sync_out, size_t size)
{
    if (o->shape != o->previous_shape) {
        pb_analog_init(o);
        o->previous_shape = o->shape;
    }
    o->phase_increment = pb_analog_compute_phase_increment(o->pitch);
    if (o->pitch > PB_ANALOG_HIGHEST_NOTE) o->pitch = PB_ANALOG_HIGHEST_NOTE;
    else if (o->pitch < 0) o->pitch = 0;
#ifdef PB_ANALOG_R
    switch (o->shape) {
    case PB_ANALOG_SAW: pb_analog_render_saw(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_VARIABLE_SAW: pb_analog_render_variable_saw(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_CSAW: pb_analog_render_csaw(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_SQUARE: pb_analog_render_square(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_TRIANGLE: pb_analog_render_triangle(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_SINE: pb_analog_render_sine(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_TRIANGLE_FOLD: pb_analog_render_triangle_fold(o, sync_in, buffer, sync_out, size); return;
    case PB_ANALOG_SINE_FOLD: pb_analog_render_sine_fold(o, sync_in, buffer, sync_out, size); return;
    default: pb_analog_render_buzz(o, sync_in, buffer, sync_out, size); return;
    }
#else
    (void)sync_in; (void)sync_out;
    while (size--) *buffer++ = 0;
#endif
}
