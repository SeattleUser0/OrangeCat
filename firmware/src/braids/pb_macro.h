/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: Braids' MacroOscillator (braids/macro_oscillator.h, .cc) in C: the state, Init, the setters,
 * Strike, Render (the dispatch, fn_table_), RenderDigital and RenderSawComb. The renders built on the analog
 * oscillators (RenderCSaw, RenderMorph, RenderSawSquare, RenderSineTriangle, RenderBuzz, RenderSub, RenderDualSync,
 * RenderTriple with ConfigureTriple) are in pb_macro_a.h, which defines PB_MACRO_A. A render function is
 * `static void pb_macro_render_<name>(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)`.
 *
 * Revisions:
 *   0.5.1: first version.
 */
#pragma once
#include "pb_analog.h"
#include "pb_digital.h"

/* settings.h MacroOscillatorShape */
enum {
    PB_SHAPE_CSAW, PB_SHAPE_MORPH, PB_SHAPE_SAW_SQUARE, PB_SHAPE_SINE_TRIANGLE, PB_SHAPE_BUZZ, PB_SHAPE_SQUARE_SUB,
    PB_SHAPE_SAW_SUB, PB_SHAPE_SQUARE_SYNC, PB_SHAPE_SAW_SYNC, PB_SHAPE_TRIPLE_SAW, PB_SHAPE_TRIPLE_SQUARE,
    PB_SHAPE_TRIPLE_TRIANGLE, PB_SHAPE_TRIPLE_SINE, PB_SHAPE_TRIPLE_RING_MOD, PB_SHAPE_SAW_SWARM, PB_SHAPE_SAW_COMB,
    PB_SHAPE_TOY, PB_SHAPE_DIGITAL_FILTER_LP, PB_SHAPE_DIGITAL_FILTER_PK, PB_SHAPE_DIGITAL_FILTER_BP,
    PB_SHAPE_DIGITAL_FILTER_HP, PB_SHAPE_VOSIM, PB_SHAPE_VOWEL, PB_SHAPE_VOWEL_FOF, PB_SHAPE_HARMONICS, PB_SHAPE_FM,
    PB_SHAPE_FEEDBACK_FM, PB_SHAPE_CHAOTIC_FEEDBACK_FM, PB_SHAPE_PLUCKED, PB_SHAPE_BOWED, PB_SHAPE_BLOWN,
    PB_SHAPE_FLUTED, PB_SHAPE_STRUCK_BELL, PB_SHAPE_STRUCK_DRUM, PB_SHAPE_KICK, PB_SHAPE_CYMBAL, PB_SHAPE_SNARE,
    PB_SHAPE_WAVETABLES, PB_SHAPE_WAVE_MAP, PB_SHAPE_WAVE_LINE, PB_SHAPE_WAVE_PARAPHONIC, PB_SHAPE_FILTERED_NOISE,
    PB_SHAPE_TWIN_PEAKS_NOISE, PB_SHAPE_CLOCKED_NOISE, PB_SHAPE_GRANULAR_CLOUD, PB_SHAPE_PARTICLE_NOISE,
    PB_SHAPE_DIGITAL_MODULATION, PB_SHAPE_QUESTION_MARK, PB_SHAPE_N
};

typedef struct {
    int16_t parameter[2], previous_parameter[2], pitch;
    uint8_t sync_buffer[24];
    int16_t temp_buffer[24];
    int32_t lp_state;
    pb_analog_t analog_oscillator[3];
    pb_dig_t digital_oscillator;
    int shape;
} pb_macro_t;

static inline void pb_macro_init(pb_macro_t *o)
{
    pb_analog_init(&o->analog_oscillator[0]);
    pb_analog_init(&o->analog_oscillator[1]);
    pb_analog_init(&o->analog_oscillator[2]);
    pb_dig_init(&o->digital_oscillator);
    o->lp_state = 0;
    o->previous_parameter[0] = 0;
    o->previous_parameter[1] = 0;
}
static inline void pb_macro_strike(pb_macro_t *o) { pb_dig_strike(&o->digital_oscillator); }
static inline void pb_macro_set_shape(pb_macro_t *o, int shape)
{
    if (shape != o->shape) pb_macro_strike(o);
    o->shape = shape;
}
static inline void pb_macro_set_pitch(pb_macro_t *o, int16_t pitch) { o->pitch = pitch; }
static inline void pb_macro_set_parameters(pb_macro_t *o, int16_t parameter_1, int16_t parameter_2)
{
    o->parameter[0] = parameter_1;
    o->parameter[1] = parameter_2;
}

#if __has_include("pb_macro_a.h")
#include "pb_macro_a.h"
#endif

static void pb_macro_render_digital(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_dig_set_parameters(&o->digital_oscillator, o->parameter[0], o->parameter[1]);
    pb_dig_set_pitch(&o->digital_oscillator, o->pitch);
    pb_dig_set_shape(&o->digital_oscillator, o->shape - PB_SHAPE_TRIPLE_RING_MOD);
    pb_dig_render(&o->digital_oscillator, sync, buffer, size);
}
static void pb_macro_render_saw_comb(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_analog_set_parameter(&o->analog_oscillator[0], 0);
    pb_analog_set_pitch(&o->analog_oscillator[0], o->pitch);
    pb_analog_set_shape(&o->analog_oscillator[0], PB_ANALOG_SAW);
    pb_analog_render(&o->analog_oscillator[0], sync, buffer, NULL, size);
    pb_dig_set_parameters(&o->digital_oscillator, o->parameter[0], o->parameter[1]);
    pb_dig_set_pitch(&o->digital_oscillator, o->pitch);
    pb_dig_set_shape(&o->digital_oscillator, PB_OSC_SHAPE_COMB_FILTER);
    pb_dig_render(&o->digital_oscillator, sync, buffer, size);
}

/* the shapes this build can play (the parity test skips the others) */
static inline int pb_macro_supported(int shape)
{
    if (shape == PB_SHAPE_SAW_COMB) {
#ifdef PB_ANALOG_R
        return pb_dig_supported(PB_OSC_SHAPE_COMB_FILTER);
#else
        return 0;
#endif
    }
    if (shape >= PB_SHAPE_TRIPLE_RING_MOD && shape < PB_SHAPE_N) return pb_dig_supported(shape - PB_SHAPE_TRIPLE_RING_MOD);
#if defined(PB_MACRO_A) && defined(PB_ANALOG_R)
    return shape >= 0 && shape < PB_SHAPE_TRIPLE_RING_MOD;
#else
    return 0;
#endif
}

static void pb_macro_render(pb_macro_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    switch (o->shape) {
#ifdef PB_MACRO_A
    case PB_SHAPE_CSAW: pb_macro_render_csaw(o, sync, buffer, size); return;
    case PB_SHAPE_MORPH: pb_macro_render_morph(o, sync, buffer, size); return;
    case PB_SHAPE_SAW_SQUARE: pb_macro_render_saw_square(o, sync, buffer, size); return;
    case PB_SHAPE_SINE_TRIANGLE: pb_macro_render_sine_triangle(o, sync, buffer, size); return;
    case PB_SHAPE_BUZZ: pb_macro_render_buzz(o, sync, buffer, size); return;
    case PB_SHAPE_SQUARE_SUB: case PB_SHAPE_SAW_SUB: pb_macro_render_sub(o, sync, buffer, size); return;
    case PB_SHAPE_SQUARE_SYNC: case PB_SHAPE_SAW_SYNC: pb_macro_render_dual_sync(o, sync, buffer, size); return;
    case PB_SHAPE_TRIPLE_SAW: case PB_SHAPE_TRIPLE_SQUARE: case PB_SHAPE_TRIPLE_TRIANGLE: case PB_SHAPE_TRIPLE_SINE:
        pb_macro_render_triple(o, sync, buffer, size); return;
#endif
    case PB_SHAPE_SAW_COMB: pb_macro_render_saw_comb(o, sync, buffer, size); return;
    default:
        if (o->shape >= PB_SHAPE_TRIPLE_RING_MOD && o->shape < PB_SHAPE_N) {
            pb_macro_render_digital(o, sync, buffer, size);
            return;
        }
        while (size--) *buffer++ = 0;
        return;
    }
}
