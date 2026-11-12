/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2: Plaits' model 5, WAVETABLE (plaits/dsp/engine/wavetable_engine.cc and .h), in C. Its
 * Differentiator and InterpolateWaveHermite (oscillator/wavetable_oscillator.h) are pl_diff_t and
 * pl_interpolate_wave_hermite in pl_dsp2.h; the 192 waves (wav_integrated_waves, 128 samples + 4 wrapped each,
 * stored integrated) are pl_wav_bank_0..2 in pl_tables2.h (pl_wav_banks[]: a bank the pick left out is 0).
 *   An 8 x 8 x 4 terrain of waves, read with Hermite interpolation and mixed trilinearly, then differentiated.
 *   TIMBRE: X (the column), MORPH: Y (the row), HARMONICS: Z (the bank; past bank 3 the Z axis folds back and
 *   the three coordinates snap to whole waves).
 *   OUT: the mix. AUX: the mix quantized to 32 levels.
 * Differences from the C++:
 *   - no user data (LoadUserData(NULL)): banks 0..2 are the waves as stored, bank 3 the waves (i * 101 % 192).
 *     The C++ keeps that map as 64 pointers from the allocator (wave_map_); pl_wt_wave computes the address from
 *     (x, y, z) instead, so the state holds no pointers.
 */
#pragma once
#include "pl_dsp2.h"
#include "pl_tables2.h"

#define PL_WT_NUM_BANKS 4
#define PL_WT_NUM_WAVES_PER_BANK 64
#define PL_WT_NUM_WAVES 192
#define PL_WT_TABLE_SIZE 128u
#define PL_WT_TABLE_SIZE_F 128.0f               /* kTableSizeF */

typedef struct {
    float phase;
    float x_pre_lp, y_pre_lp, z_pre_lp;
    float x_lp, y_lp, z_lp;
    float previous_x, previous_y, previous_z, previous_f0;
    pl_diff_t diff_out;
} pl_wt_t;

static void pl_wt_init(pl_wt_t *e)
{
    e->phase = 0.0f;
    e->x_lp = 0.0f;
    e->y_lp = 0.0f;
    e->z_lp = 0.0f;
    e->x_pre_lp = 0.0f;
    e->y_pre_lp = 0.0f;
    e->z_pre_lp = 0.0f;
    e->previous_x = 0.0f;
    e->previous_y = 0.0f;
    e->previous_z = 0.0f;
    e->previous_f0 = PL_A0;
    pl_diff_init(&e->diff_out);
}
static void pl_wt_reset(pl_wt_t *e) { (void)e; }

static inline float pl_wt_clamp(float x, float amount)
{
    x = x - 0.5f;
    x *= amount;
    PL_CONSTRAIN(x, -0.5f, 0.5f);
    x += 0.5f;
    return x;
}
/* wave_map_[x + y * 8 + z * 64] as LoadUserData(NULL) fills it: banks 0..2 as stored, bank 3 reshuffled */
static inline const int16_t *pl_wt_wave(int x, int y, int z)
{
    int i = x + y * 8 + z * PL_WT_NUM_WAVES_PER_BANK;
    int w = z == PL_WT_NUM_BANKS - 1 ? i * 101 % PL_WT_NUM_WAVES : i;
    const int16_t *b = pl_wav_banks[w / PL_WT_NUM_WAVES_PER_BANK];
    /* (model picker) a wave bank the pick left out reads as a null pointer: its waves are the first bank present (the pick keeps at least one) */
    if (!b) b = pl_wav_banks[0] ? pl_wav_banks[0] : pl_wav_banks[1] ? pl_wav_banks[1] : pl_wav_banks[2];
    return b + (size_t)(w % PL_WT_NUM_WAVES_PER_BANK) * (PL_WT_TABLE_SIZE + 4);
}
/* ReadWave */
static inline float pl_wt_read_wave(int x, int y, int z, int phase_integral, float phase_fractional)
{
    return pl_interpolate_wave_hermite(pl_wt_wave(x, y, z), phase_integral, phase_fractional);
}

static void pl_wt_render(pl_wt_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);

    PL_ONE_POLE(e->x_pre_lp, p->timbre * 6.9999f, 0.2f);
    PL_ONE_POLE(e->y_pre_lp, p->morph * 6.9999f, 0.2f);
    PL_ONE_POLE(e->z_pre_lp, p->harmonics * 6.9999f, 0.05f);

    /* (the C++ calls these x, y, z too, and the per-sample ones below again: renamed here, nothing else) */
    const float x_pre = e->x_pre_lp;
    const float y_pre = e->y_pre_lp;
    const float z_pre = e->z_pre_lp;

    const float quantization = pl_minf(pl_maxf(z_pre - 3.0f, 0.0f), 1.0f);
    const float lp_coefficient = pl_minf(pl_maxf(2.0f * f0 * (4.0f - 3.0f * quantization), 0.01f), 0.1f);

    PL_MAKE_INTEGRAL_FRACTIONAL(x_pre)
    PL_MAKE_INTEGRAL_FRACTIONAL(y_pre)
    PL_MAKE_INTEGRAL_FRACTIONAL(z_pre)

    x_pre_fractional += quantization * (pl_wt_clamp(x_pre_fractional, 16.0f) - x_pre_fractional);
    y_pre_fractional += quantization * (pl_wt_clamp(y_pre_fractional, 16.0f) - y_pre_fractional);
    z_pre_fractional += quantization * (pl_wt_clamp(z_pre_fractional, 16.0f) - z_pre_fractional);

    pl_interp_t x_modulation, y_modulation, z_modulation, f0_modulation;
    pl_interp_init(&x_modulation, &e->previous_x, (float)x_pre_integral + x_pre_fractional, size);
    pl_interp_init(&y_modulation, &e->previous_y, (float)y_pre_integral + y_pre_fractional, size);
    pl_interp_init(&z_modulation, &e->previous_z, (float)z_pre_integral + z_pre_fractional, size);
    pl_interp_init(&f0_modulation, &e->previous_f0, f0, size);

    while (size--) {
        const float f = pl_interp_next(&f0_modulation);    /* (the C++'s per-sample f0) */

        const float gain = (1.0f / (f * 131072.0f)) * (0.95f - f);
        const float cutoff = pl_minf(PL_WT_TABLE_SIZE_F * f, 1.0f);

        PL_ONE_POLE(e->x_lp, pl_interp_next(&x_modulation), lp_coefficient);
        PL_ONE_POLE(e->y_lp, pl_interp_next(&y_modulation), lp_coefficient);
        PL_ONE_POLE(e->z_lp, pl_interp_next(&z_modulation), lp_coefficient);

        const float x = e->x_lp;
        const float y = e->y_lp;
        const float z = e->z_lp;

        PL_MAKE_INTEGRAL_FRACTIONAL(x)
        PL_MAKE_INTEGRAL_FRACTIONAL(y)
        PL_MAKE_INTEGRAL_FRACTIONAL(z)

        e->phase += f;
        if (e->phase >= 1.0f) e->phase -= 1.0f;

        const float phase = e->phase * PL_WT_TABLE_SIZE_F;
        PL_MAKE_INTEGRAL_FRACTIONAL(phase)

        {
            int x0 = x_integral;
            int x1 = x_integral + 1;
            int y0 = y_integral;
            int y1 = y_integral + 1;
            int z0 = z_integral;
            int z1 = z_integral + 1;

            if (z0 >= 4) z0 = 7 - z0;
            if (z1 >= 4) z1 = 7 - z1;

            float x0y0z0 = pl_wt_read_wave(x0, y0, z0, phase_integral, phase_fractional);
            float x1y0z0 = pl_wt_read_wave(x1, y0, z0, phase_integral, phase_fractional);
            float xy0z0 = x0y0z0 + (x1y0z0 - x0y0z0) * x_fractional;

            float x0y1z0 = pl_wt_read_wave(x0, y1, z0, phase_integral, phase_fractional);
            float x1y1z0 = pl_wt_read_wave(x1, y1, z0, phase_integral, phase_fractional);
            float xy1z0 = x0y1z0 + (x1y1z0 - x0y1z0) * x_fractional;

            float xyz0 = xy0z0 + (xy1z0 - xy0z0) * y_fractional;

            float x0y0z1 = pl_wt_read_wave(x0, y0, z1, phase_integral, phase_fractional);
            float x1y0z1 = pl_wt_read_wave(x1, y0, z1, phase_integral, phase_fractional);
            float xy0z1 = x0y0z1 + (x1y0z1 - x0y0z1) * x_fractional;

            float x0y1z1 = pl_wt_read_wave(x0, y1, z1, phase_integral, phase_fractional);
            float x1y1z1 = pl_wt_read_wave(x1, y1, z1, phase_integral, phase_fractional);
            float xy1z1 = x0y1z1 + (x1y1z1 - x0y1z1) * x_fractional;

            float xyz1 = xy0z1 + (xy1z1 - xy0z1) * y_fractional;

            float mix = xyz0 + (xyz1 - xyz0) * z_fractional;
            mix = pl_diff_process(&e->diff_out, cutoff, mix) * gain;
            *out++ = mix;
            *aux++ = (float)(int)(mix * 32.0f) / 32.0f;
        }
    }
    /* (the C++ objects go out of scope in reverse order of construction) */
    pl_interp_done(&f0_modulation);
    pl_interp_done(&z_modulation);
    pl_interp_done(&y_modulation);
    pl_interp_done(&x_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_wt_flush(pl_wt_t *e)
{
    pl_flush(&e->x_pre_lp); pl_flush(&e->y_pre_lp); pl_flush(&e->z_pre_lp);
    pl_flush(&e->x_lp); pl_flush(&e->y_lp); pl_flush(&e->z_lp);
    pl_flush(&e->previous_x); pl_flush(&e->previous_y); pl_flush(&e->previous_z);
    pl_flush_diff(&e->diff_out);
}
