/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments stmlib and Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the SHAPES engine's shared tools, ported from C++ to C. Each does what its namesake does, in the
 * same integer arithmetic, so the port can be checked against the original sample for sample (tests/braids_parity).
 *
 *   stmlib/utils/dsp.h        pb_interpolate824 (int16 table: _s16, uint16: _u16, uint8: _u8), pb_interpolate88
 *                             (_u16, _s16), pb_interpolate1022, pb_interpolate115, pb_crossfade (_s16, _u8),
 *                             pb_crossfade1022, pb_crossfade115, pb_mix (_s16, _u16)
 *   stmlib/utils/random.h     pb_rng_word, pb_rng_sample (Braids' own generator, apart from the MACRO engine's)
 *   stmlib/stmlib.h           PB_CONSTRAIN, PB_CLIP
 *   braids/parameter_interpolation.h   the PB_BEGIN_INTERPOLATE_* macros (with the C++'s own quirk: its
 *                             INTERPOLATE_PARAMETERS computes parameter_1 from parameter 0, kept as it is)
 *
 * Revisions:
 *   0.5.1: first version.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* the wavetable shapes' 33 KB of waves: in the parity test always, on the device when flash allows (eng_shapes.c) */
#ifndef PB_WAVETABLES
#ifdef PB_PARITY
#define PB_WAVETABLES 1
#else
#define PB_WAVETABLES 0
#endif
#endif

#define PB_CONSTRAIN(var, min, max) do { if ((var) < (min)) (var) = (min); else if ((var) > (max)) (var) = (max); } while (0)
#define PB_CLIP(x) do { if ((x) < -32767) (x) = -32767; if ((x) > 32767) (x) = 32767; } while (0)

/* ------------------------------------------------------ stmlib/utils/dsp.h --- */
static inline int16_t pb_interpolate824_s16(const int16_t *table, uint32_t phase)
{
    int32_t a = table[phase >> 24];
    int32_t b = table[(phase >> 24) + 1];
    return (int16_t)(a + ((b - a) * (int32_t)((phase >> 8) & 0xffff) >> 16));
}
static inline uint16_t pb_interpolate824_u16(const uint16_t *table, uint32_t phase)
{
    uint32_t a = table[phase >> 24];
    uint32_t b = table[(phase >> 24) + 1];
    return (uint16_t)(a + ((b - a) * (uint32_t)((phase >> 8) & 0xffff) >> 16));
}
static inline int16_t pb_interpolate824_u8(const uint8_t *table, uint32_t phase)
{
    int32_t a = table[phase >> 24];
    int32_t b = table[(phase >> 24) + 1];
    return (int16_t)((a << 8) + ((b - a) * (int32_t)(phase & 0xffffff) >> 16) - 32768);
}
static inline uint16_t pb_interpolate88_u16(const uint16_t *table, uint16_t index)
{
    int32_t a = table[index >> 8];
    int32_t b = table[(index >> 8) + 1];
    return (uint16_t)(a + ((b - a) * (int32_t)(index & 0xff) >> 8));
}
static inline int16_t pb_interpolate88_s16(const int16_t *table, uint16_t index)
{
    int32_t a = table[index >> 8];
    int32_t b = table[(index >> 8) + 1];
    return (int16_t)(a + ((b - a) * (int32_t)(index & 0xff) >> 8));
}
static inline int16_t pb_interpolate1022(const int16_t *table, uint32_t phase)
{
    int32_t a = table[phase >> 22];
    int32_t b = table[(phase >> 22) + 1];
    return (int16_t)(a + ((b - a) * (int32_t)((phase >> 6) & 0xffff) >> 16));
}
static inline int16_t pb_interpolate115(const int16_t *table, uint16_t phase)
{
    int32_t a = table[phase >> 5];
    int32_t b = table[(phase >> 5) + 1];
    return (int16_t)(a + ((b - a) * (int32_t)(phase & 0x1f) >> 5));
}
static inline int16_t pb_crossfade_s16(const int16_t *table_a, const int16_t *table_b, uint32_t phase, uint16_t balance)
{
    int32_t a = pb_interpolate824_s16(table_a, phase);
    int32_t b = pb_interpolate824_s16(table_b, phase);
    return (int16_t)(a + ((b - a) * (int32_t)balance >> 16));
}
static inline int16_t pb_crossfade_u8(const uint8_t *table_a, const uint8_t *table_b, uint32_t phase, uint16_t balance)
{
    int32_t a = pb_interpolate824_u8(table_a, phase);
    int32_t b = pb_interpolate824_u8(table_b, phase);
    return (int16_t)(a + ((b - a) * (int32_t)balance >> 16));
}
static inline int16_t pb_crossfade1022(const int16_t *table_a, const int16_t *table_b, uint32_t phase, uint16_t balance)
{
    int32_t a = pb_interpolate1022(table_a, phase);
    int32_t b = pb_interpolate1022(table_b, phase);
    return (int16_t)(a + ((b - a) * (int32_t)balance >> 16));
}
static inline int16_t pb_crossfade115(const int16_t *table_a, const int16_t *table_b, uint16_t phase, uint16_t balance)
{
    int32_t a = pb_interpolate115(table_a, phase);
    int32_t b = pb_interpolate115(table_b, phase);
    return (int16_t)(a + ((b - a) * (int32_t)balance >> 16));
}
static inline int16_t pb_mix_s16(int16_t a, int16_t b, uint16_t balance)
{
    return (int16_t)((a * (65535 - balance) + b * balance) >> 16);
}
static inline uint16_t pb_mix_u16(uint16_t a, uint16_t b, uint16_t balance)
{
    return (uint16_t)((a * (65535 - balance) + b * balance) >> 16);
}

/* --------------------------------------------------- stmlib/utils/random.h --- */
static uint32_t pb_rng_state = 0x21u;
static inline uint32_t pb_rng_word(void) { pb_rng_state = pb_rng_state * 1664525u + 1013904223u; return pb_rng_state; }
static inline int16_t pb_rng_sample(void) { return (int16_t)(pb_rng_word() >> 16); }

/* ------------------------------------------ braids/parameter_interpolation.h --- */
/* In the C++ these read the members parameter_, previous_parameter_ (and previous_phase_increment_, phase_increment_)
 * of `this`: here of the struct pointer named by the first argument. */
#define PB_BEGIN_INTERPOLATE_PARAMETERS(o) \
    int32_t parameter_0_start = (o)->previous_parameter[0]; \
    int32_t parameter_1_start = (o)->previous_parameter[1]; \
    int32_t parameter_0_delta = (o)->parameter[0] - (o)->previous_parameter[0]; \
    int32_t parameter_1_end = (o)->parameter[1] - (o)->previous_parameter[1]; \
    int32_t parameter_increment = 32767 / (int32_t)size; \
    int32_t parameter_xfade = 0; \
    (void)parameter_1_start; (void)parameter_1_end;
#define PB_INTERPOLATE_PARAMETERS \
    parameter_xfade += parameter_increment; \
    int32_t parameter_0 = parameter_0_start + (parameter_0_delta * parameter_xfade >> 15); \
    int32_t parameter_1 = parameter_0_start + (parameter_0_delta * parameter_xfade >> 15); \
    (void)parameter_0; (void)parameter_1;
#define PB_END_INTERPOLATE_PARAMETERS(o) \
    (o)->previous_parameter[0] = (o)->parameter[0]; \
    (o)->previous_parameter[1] = (o)->parameter[1];

#define PB_BEGIN_INTERPOLATE_PARAMETER_0(o) \
    int32_t parameter_0_start = (o)->previous_parameter[0]; \
    int32_t parameter_0_delta = (o)->parameter[0] - (o)->previous_parameter[0]; \
    int32_t parameter_increment = 32767 / (int32_t)size; \
    int32_t parameter_xfade = 0;
#define PB_INTERPOLATE_PARAMETER_0 \
    parameter_xfade += parameter_increment; \
    int32_t parameter_0 = parameter_0_start + (parameter_0_delta * parameter_xfade >> 15);
#define PB_END_INTERPOLATE_PARAMETER_0(o) \
    (o)->previous_parameter[0] = (o)->parameter[0];

#define PB_BEGIN_INTERPOLATE_PARAMETER_1(o) \
    int32_t parameter_1_start = (o)->previous_parameter[1]; \
    int32_t parameter_1_delta = (o)->parameter[1] - (o)->previous_parameter[1]; \
    int32_t parameter_increment = 32767 / (int32_t)size; \
    int32_t parameter_xfade = 0;
#define PB_INTERPOLATE_PARAMETER_1 \
    parameter_xfade += parameter_increment; \
    int32_t parameter_1 = parameter_1_start + (parameter_1_delta * parameter_xfade >> 15);
#define PB_END_INTERPOLATE_PARAMETER_1(o) \
    (o)->previous_parameter[1] = (o)->parameter[1];

/* (BEGIN_INTERPOLATE_PARAMETER for classes with one parameter_: previous_parameter_single / parameter_single) */
#define PB_BEGIN_INTERPOLATE_PARAMETER(o) \
    int32_t parameter_start = (o)->previous_parameter_single; \
    int32_t parameter_delta = (o)->parameter_single - (o)->previous_parameter_single; \
    int32_t parameter_increment = 32767 / (int32_t)size; \
    int32_t parameter_xfade = 0;
#define PB_INTERPOLATE_PARAMETER \
    parameter_xfade += parameter_increment; \
    int32_t parameter = parameter_start + (parameter_delta * parameter_xfade >> 15);
#define PB_END_INTERPOLATE_PARAMETER(o) \
    (o)->previous_parameter_single = (o)->parameter_single;

#define PB_BEGIN_INTERPOLATE_PHASE_INCREMENT(o) \
    uint32_t phase_increment = (o)->previous_phase_increment; \
    uint32_t phase_increment_increment = \
        (o)->previous_phase_increment < (o)->phase_increment \
        ? ((o)->phase_increment - (o)->previous_phase_increment) / (uint32_t)size \
        : ~(((o)->previous_phase_increment - (o)->phase_increment) / (uint32_t)size);
#define PB_INTERPOLATE_PHASE_INCREMENT \
    phase_increment += phase_increment_increment;
#define PB_END_INTERPOLATE_PHASE_INCREMENT(o) \
    (o)->previous_phase_increment = phase_increment;
