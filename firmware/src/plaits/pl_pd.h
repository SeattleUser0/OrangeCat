/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2: Plaits 1.2's PHASE DISTORTION (plaits/dsp/engine2/phase_distortion_engine.cc and .h), in C.
 * Its oscillator (oscillator/variable_shape_oscillator.h, Render<enable_sync, output_phase> with phase output)
 * is pl_vshape_t / pl_vshape_render_t in pl_va.h; the Sine is pl_sine (pl_osc.h).
 *   A phasor, phase-modulated by an asymmetric triangle, read through a sine; rendered 2x oversampled and
 *   averaged down.
 *   HARMONICS: the modulator's ratio (lut_fm_frequency_quantizer). TIMBRE: the modulation depth. MORPH: the
 *   triangle's asymmetry.
 *   OUT: the modulator hard-synced to the carrier (phase distortion). AUX: the modulator free running (phase
 *   modulation).
 * Differences from the C++:
 *   - the C++'s temp_buffer_ (kMaxBlockSize * 4 floats from the allocator) is on the render's stack.
 */
#pragma once
#include "pl_va.h"

typedef struct {
    pl_vshape_t shaper, modulator;
} pl_pd_t;

static void pl_pd_init(pl_pd_t *e)
{
    pl_vshape_init(&e->modulator);
    pl_vshape_init(&e->shaper);
}
static void pl_pd_reset(pl_pd_t *e) { (void)e; }

static void pl_pd_render(pl_pd_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = 0.5f * pl_note_to_frequency(p->note);
    const float modulator_f = pl_minf(0.25f, f0 * pl_semitones_to_ratio(pl_interpolate(pl_lut_fm_frequency_quantizer, p->harmonics, 128.0f)));
    const float pw = 0.5f + p->morph * 0.49f;
    const float amount = 8.0f * p->timbre * p->timbre * (1.0f - modulator_f * 3.8f);
    size_t i;

    /* upsample by 2x */
    float temp_buffer[PL_MAX_BLOCK * 4];        /* (the allocator's temp_buffer_) */
    float *synced = &temp_buffer[0];
    float *free_running = &temp_buffer[2 * size];
    pl_vshape_render_t(&e->shaper, 1, 1, f0, modulator_f, pw, 0.0f, amount, synced, 2 * size);
    pl_vshape_render_t(&e->modulator, 0, 1, f0, modulator_f, pw, 0.0f, amount, free_running, 2 * size);

    for (i = 0; i < size; ++i) {
        /* naive 0.5x downsampling */
        out[i] = 0.5f * pl_sine(*synced++ + 0.25f);
        out[i] += 0.5f * pl_sine(*synced++ + 0.25f);

        aux[i] = 0.5f * pl_sine(*free_running++ + 0.25f);
        aux[i] += 0.5f * pl_sine(*free_running++ + 0.25f);
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render.
 * Nothing here decays (two oscillators whose interpolator states follow the knobs). */
static inline void pl_pd_flush(pl_pd_t *e) { (void)e; }
