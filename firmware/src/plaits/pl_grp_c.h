/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: helpers shared by the physical models (STRING pl_string.h, MODAL pl_modal.h), in C.
 *   plaits/dsp/noise/dust.h                    plc_dust
 *   plaits/dsp/physical_modelling/delay_line.h plc_delay_write, plc_delay_read: DelayLine<float, N> for float lines
 *                                              (the STRING engine's f0 delay). The line is a float array of a power of
 *                                              two length plus a write index; `% max_delay` becomes `& (size - 1)`,
 *                                              which gives the same index for every value the C++ can see (its
 *                                              size_t arithmetic wraps modulo 2^32 or 2^64, both multiples of size).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_dsp.h"

/* ------------------------------------------------------------- dust.h --- */
static inline float plc_dust(float frequency)
{
    float inv_frequency = 1.0f / frequency;
    float u = pl_rng_float();
    if (u < frequency) return u * inv_frequency;
    else return 0.0f;
}

/* ------------------------------------------------------- delay_line.h --- */
/* DelayLine<float, size>: line[size] (size a power of two) and *write_ptr. Reset: zero the line, write_ptr = 0. */
static inline void plc_delay_reset(float *line, uint32_t *write_ptr, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; ++i) line[i] = 0.0f;
    *write_ptr = 0;
}
static inline void plc_delay_write(float *line, uint32_t *write_ptr, uint32_t size, float sample)
{
    line[*write_ptr] = sample;
    *write_ptr = (*write_ptr - 1u + size) & (size - 1u);
}
static inline float plc_delay_read(const float *line, uint32_t write_ptr, uint32_t size, float delay)
{
    PL_MAKE_INTEGRAL_FRACTIONAL(delay)
    const float a = line[(write_ptr + (uint32_t)delay_integral) & (size - 1u)];
    const float b = line[(write_ptr + (uint32_t)delay_integral + 1u) & (size - 1u)];
    return a + (b - a) * delay_fractional;
}
