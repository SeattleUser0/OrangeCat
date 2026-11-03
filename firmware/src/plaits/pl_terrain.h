/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2: Plaits 1.2's WAVE TERRAIN (plaits/dsp/engine2/wave_terrain_engine.cc and .h), in C. The path
 * oscillator (oscillator/sine_oscillator.h FastSineOscillator) is pl_fast_sine_osc_t in pl_osc.h; the Sine is
 * pl_sine; terrains 5..7 read the wavetable data (pl_wav_integrated_waves, pl_tables2.h) as a terrain.
 *   A 2-D function evaluated along an ellipse, 2x oversampled.
 *   HARMONICS: the terrain (five formulas, then the three wavetable banks). TIMBRE: the radius of the path.
 *   MORPH: its X offset (the path flattens as it moves off centre).
 *   OUT: the terrain along the path. AUX: the path's Y plus the terrain, through a sine.
 * Differences from the C++:
 *   - no user terrain (user_terrain_ NULL): eight terrains; the ninth (a user's 64 x 64 map) is not there.
 *   - the C++'s temp_buffer_ (kMaxBlockSize * 4 floats from the allocator) is on the render's stack.
 */
#pragma once
#include "pl_osc.h"
#include "pl_tables2.h"

typedef struct {
    pl_fast_sine_osc_t path;
    float offset, terrain;
} pl_terrain_t;

static void pl_terrain_init(pl_terrain_t *e)
{
    pl_fast_sine_osc_init(&e->path);
    e->offset = 0.0f;
    e->terrain = 0.0f;
}
static void pl_terrain_reset(pl_terrain_t *e) { (void)e; }

/* InterpolateIntegratedWave<int16_t>: the difference of two linear reads, i.e. the wave itself */
static inline float pl_terrain_interpolate_integrated_wave(const int16_t *table, int32_t index_integral, float index_fractional)
{
    float a = (float)table[index_integral];
    float b = (float)table[index_integral + 1];
    float c = (float)table[index_integral + 2];
    float t = index_fractional;
    return (b - a) + (c - b - b + a) * t;
}
/* TerrainLookupWT (DIFFERENTIATE_WAVE_DATA): a wavetable bank re-read as a terrain */
static inline float pl_terrain_lookup_wt(float x, float y, int bank)
{
    const int table_size = 128;
    const int table_size_full = table_size + 4;   /* includes 4 wrapped samples */
    const int num_waves = 64;
    const float sample = (y + 1.0f) * 0.5f * (float)table_size;
    const float wt = (x + 1.0f) * 0.5f * (float)(num_waves - 1);

    const int16_t *waves = pl_wav_integrated_waves + bank * num_waves * table_size_full;

    PL_MAKE_INTEGRAL_FRACTIONAL(sample)
    PL_MAKE_INTEGRAL_FRACTIONAL(wt)

    float xy[2];
    const float value_scale = 1.0f / 1024.0f;
    waves += wt_integral * table_size_full;
    xy[0] = pl_terrain_interpolate_integrated_wave(waves, sample_integral, sample_fractional);
    waves += table_size_full;
    xy[1] = pl_terrain_interpolate_integrated_wave(waves, sample_integral, sample_fractional);
    return (xy[0] + (xy[1] - xy[0]) * wt_fractional) * value_scale;
}
static inline float pl_terrain_squash(float x, float a)
{
    x *= a;
    return x / (1.0f + pl_fabsf(x));
}
/* Terrain(x, y, terrain_index). The Sine only works for a positive argument: every call offsets it by k. */
static inline float pl_terrain_eval(float x, float y, int terrain_index)
{
    const float k = 4.0f;
    switch (terrain_index) {
    case 0:
        return (pl_terrain_squash(pl_sine(k + x * 1.273f), 2.0f) - pl_sine(k + y * (x + 1.571f) * 0.637f)) * 0.57f;
    case 1: {
        const float xy = x * y;
        return pl_sine(k + pl_sine(k + (x + y) * 0.637f) / (0.2f + xy * xy) * 0.159f);
    }
    case 2: {
        const float xy = x * y;
        return pl_sine(k + pl_sine(k + 2.387f * xy) / (0.350f + xy * xy) * 0.159f);
    }
    case 3: {
        const float xy = x * y;
        const float xys = (x - 0.25f) * (y + 0.25f);
        return pl_sine(k + xy / (2.0f + pl_fabsf(5.0f * xys)) * 6.366f);
    }
    case 4:
        return pl_sine(0.159f / (0.170f + pl_fabsf(y - 0.25f)) + 0.477f / (0.350f + pl_fabsf((x + 0.5f) * (y + 1.5f))) + k);
    case 5:
    case 6:
    case 7:
#ifdef PL_TERRAIN_NO_WT
        return 0.0f;                            /* (unreachable: num_terrains is 5) */
#else
        return pl_terrain_lookup_wt(x, y, 2 - (terrain_index - 5));
#endif
    default:                                    /* (8 would be the user terrain: none here) */
        return 0.0f;
    }
}

static void pl_terrain_render(pl_terrain_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const size_t oversampling = 2;              /* kOversampling */
    const float scale = 1.0f / (float)oversampling;   /* kScale */
    float temp_buffer[PL_MAX_BLOCK * 4];        /* (the allocator's temp_buffer_) */
    size_t i, j, ij;

    float *path_x = &temp_buffer[0];
    float *path_y = &temp_buffer[oversampling * size];

    const float f0 = pl_note_to_frequency(p->note);
    const float attenuation = pl_maxf(1.0f - 8.0f * f0, 0.0f);
    const float radius = 0.1f + 0.9f * p->timbre * attenuation * (2.0f - attenuation);

    /* the "magic sine" gives the sin and cos of the trajectory */
    pl_fast_sine_osc_render_quadrature(&e->path, f0 * scale, radius, path_x, path_y, size * oversampling);

    pl_interp_t offset, terrain;
    pl_interp_init(&offset, &e->offset, 1.9f * p->morph - 1.0f, size);
#ifdef PL_TERRAIN_NO_WT
    const int num_terrains = 5;                 /* (eng_macro2.c MACRO2_SET 1: the three wavetable terrains are out with the waves; the knob spans terrains 0..4) */
#else
    const int num_terrains = 8;                 /* (no user terrain) */
#endif
    pl_interp_init(&terrain, &e->terrain, pl_minf(p->harmonics * 1.05f, 1.0f) * ((float)num_terrains - 1.0001f), size);

    ij = 0;
    for (i = 0; i < size; ++i) {
        const float x_offset = pl_interp_next(&offset);

        const float z = pl_interp_next(&terrain);
        PL_MAKE_INTEGRAL_FRACTIONAL(z)

        float out_s = 0.0f;
        float aux_s = 0.0f;

        for (j = 0; j < oversampling; ++j) {
            const float x = path_x[ij] * (1.0f - pl_fabsf(x_offset)) + x_offset;
            const float y = path_y[ij];
            ++ij;

            const float z0 = pl_terrain_eval(x, y, z_integral);
            const float z1 = pl_terrain_eval(x, y, z_integral + 1);
            const float zz = (z0 + (z1 - z0) * z_fractional);
            out_s += zz;
            aux_s += y + zz;
        }
        out[i] = scale * out_s;
        aux[i] = pl_sine(1.0f + 0.5f * scale * aux_s);
    }
    pl_interp_done(&terrain);
    pl_interp_done(&offset);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render.
 * Nothing here decays (the path oscillator circles, the two interpolator states follow the knobs). */
static inline void pl_terrain_flush(pl_terrain_t *e) { (void)e; }
