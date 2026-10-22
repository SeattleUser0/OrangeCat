/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 3, FM (plaits/dsp/engine/fm_engine.cc), with its 4x downsampler
 * (plaits/dsp/downsampler/4x_downsampler.h), in C.
 *   OUT: two sine operators, 4x oversampled. HARMONICS: the modulator's ratio (lut_fm_frequency_quantizer), TIMBRE:
 *        the FM index, MORPH: feedback (below 12 o'clock on the modulator's phase, above on its frequency).
 *   AUX: a sub-octave sine, phase modulated by the carrier.
 * Same on the device as in the C++. One small difference in form: the C++ leaves carrier_fir_ and sub_fir_ (the
 * downsamplers' carried state) unset in Init(); here init sets them to 0.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

#define PL_FM_OVERSAMPLING 4u                   /* kOversampling */

/* Downsampler: carries one output's FIR state between blocks. _done() is the C++ destructor (writes the state back). */
typedef struct { float head, tail; float *state; } pl_fm_downsampler_t;
static inline void pl_fm_downsampler_init(pl_fm_downsampler_t *d, float *state)
{
    d->head = *state;
    d->tail = 0.0f;
    d->state = state;
}
static inline void pl_fm_downsampler_done(pl_fm_downsampler_t *d) { *d->state = d->head; }
static inline void pl_fm_downsampler_accumulate(pl_fm_downsampler_t *d, int i, float sample)
{
    d->head += sample * pl_lut_4x_downsampler_fir[3 - (i & 3)];
    d->tail += sample * pl_lut_4x_downsampler_fir[i & 3];
}
static inline float pl_fm_downsampler_read(pl_fm_downsampler_t *d)
{
    float value = d->head;
    d->head = d->tail;
    d->tail = 0.0f;
    return value;
}

typedef struct {
    uint32_t carrier_phase, modulator_phase, sub_phase;
    float previous_carrier_frequency, previous_modulator_frequency, previous_amount, previous_feedback, previous_sample;
    float sub_fir, carrier_fir;
} pl_fm_t;

static void pl_fm_init(pl_fm_t *e)
{
    e->carrier_phase = 0;
    e->modulator_phase = 0;
    e->sub_phase = 0;
    e->previous_carrier_frequency = PL_A0;
    e->previous_modulator_frequency = PL_A0;
    e->previous_amount = 0.0f;
    e->previous_feedback = 0.0f;
    e->previous_sample = 0.0f;
    e->sub_fir = 0.0f;                          /* (not set by the C++ Init) */
    e->carrier_fir = 0.0f;
}
static void pl_fm_reset(pl_fm_t *e) { (void)e; }

static void pl_fm_render(pl_fm_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    /* 4x oversampling */
    const float note = p->note - 24.0f;
    const float ratio = pl_interpolate(pl_lut_fm_frequency_quantizer, p->harmonics, 128.0f);

    float modulator_note = note + ratio;
    float target_modulator_frequency = pl_note_to_frequency(modulator_note);
    PL_CONSTRAIN(target_modulator_frequency, 0.0f, 0.5f);

    /* reduce the maximum FM index for high pitched notes, to prevent aliasing */
    float hf_taming = 1.0f - (modulator_note - 72.0f) * 0.025f;
    PL_CONSTRAIN(hf_taming, 0.0f, 1.0f);
    hf_taming *= hf_taming;

    pl_interp_t carrier_frequency, modulator_frequency, amount_modulation, feedback_modulation;
    pl_interp_init(&carrier_frequency, &e->previous_carrier_frequency, pl_note_to_frequency(note), size);
    pl_interp_init(&modulator_frequency, &e->previous_modulator_frequency, target_modulator_frequency, size);
    pl_interp_init(&amount_modulation, &e->previous_amount, 2.0f * p->timbre * p->timbre * hf_taming, size);
    pl_interp_init(&feedback_modulation, &e->previous_feedback, 2.0f * p->morph - 1.0f, size);

    pl_fm_downsampler_t carrier_downsampler, sub_downsampler;
    pl_fm_downsampler_init(&carrier_downsampler, &e->carrier_fir);
    pl_fm_downsampler_init(&sub_downsampler, &e->sub_fir);

    while (size--) {
        const float max_uint32 = 4294967296.0f;
        const float amount = pl_interp_next(&amount_modulation);
        const float feedback = pl_interp_next(&feedback_modulation);
        float phase_feedback = feedback < 0.0f ? 0.5f * feedback * feedback : 0.0f;
        const uint32_t carrier_increment = (uint32_t)(max_uint32 * pl_interp_next(&carrier_frequency));
        float modulator_f = pl_interp_next(&modulator_frequency);

        for (int j = 0; j < (int)PL_FM_OVERSAMPLING; ++j) {
            /* (the float is in 0 .. 0.75 * 2^32 here, so the cast to uint32_t is defined) */
            e->modulator_phase += (uint32_t)(max_uint32 * modulator_f * (1.0f + e->previous_sample * phase_feedback));
            e->carrier_phase += carrier_increment;
            e->sub_phase += carrier_increment >> 1;
            float modulator_fb = feedback > 0.0f ? 0.25f * feedback * feedback : 0.0f;
            float modulator = pl_sine_pm(e->modulator_phase, modulator_fb * e->previous_sample);
            float carrier = pl_sine_pm(e->carrier_phase, amount * modulator);
            float sub = pl_sine_pm(e->sub_phase, amount * carrier * 0.25f);
            PL_ONE_POLE(e->previous_sample, carrier, 0.05f);
            pl_fm_downsampler_accumulate(&carrier_downsampler, j, carrier);
            pl_fm_downsampler_accumulate(&sub_downsampler, j, sub);
        }
        *out++ = pl_fm_downsampler_read(&carrier_downsampler);
        *aux++ = pl_fm_downsampler_read(&sub_downsampler);
    }
    /* (the C++ objects go out of scope in reverse order of construction) */
    pl_fm_downsampler_done(&sub_downsampler);
    pl_fm_downsampler_done(&carrier_downsampler);
    pl_interp_done(&feedback_modulation);
    pl_interp_done(&amount_modulation);
    pl_interp_done(&modulator_frequency);
    pl_interp_done(&carrier_frequency);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_fm_flush(pl_fm_t *e) { pl_flush(&e->previous_sample); pl_flush(&e->sub_fir); pl_flush(&e->carrier_fir); }
