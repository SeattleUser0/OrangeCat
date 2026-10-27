/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 9, NOISE (plaits/dsp/engine/noise_engine.cc), with its clocked noise source
 * (noise/clocked_noise.h), in C.
 *   OUT: clocked noise through a filter swept from low-pass to high-pass (HARMONICS).
 *   AUX: two clocked noises through two band-passes, the second tuned by HARMONICS.
 *   TIMBRE: the clock rate. MORPH: the resonance. A trigger resyncs the clocks.
 * The C++'s temp buffer (from the allocator) is an array in the struct.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

/* ------------------------------------------------------ clocked_noise.h --- */
typedef struct { float phase, sample, next_sample, frequency; } pl_noise_clocked_t;
static inline void pl_noise_clocked_init(pl_noise_clocked_t *c)
{
    c->phase = 0.0f;
    c->sample = 0.0f;
    c->next_sample = 0.0f;
    c->frequency = 0.001f;
}
static void pl_noise_clocked_render(pl_noise_clocked_t *c, int sync, float frequency, float *out, size_t size)
{
    PL_CONSTRAIN(frequency, 0.0f, 1.0f);
    pl_interp_t fm;
    pl_interp_init(&fm, &c->frequency, frequency, size);
    float next_sample = c->next_sample;
    float sample = c->sample;
    if (sync) c->phase = 1.0f;
    while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;
        const float f = pl_interp_next(&fm);
        const float raw_sample = pl_rng_float() * 2.0f - 1.0f;
        float raw_amount = 4.0f * (f - 0.25f);
        PL_CONSTRAIN(raw_amount, 0.0f, 1.0f);
        c->phase += f;
        if (c->phase >= 1.0f) {
            c->phase -= 1.0f;
            float t = c->phase / f;
            float new_sample = raw_sample;
            float discontinuity = new_sample - sample;
            this_sample += discontinuity * pl_this_blep(t);
            next_sample += discontinuity * pl_next_blep(t);
            sample = new_sample;
        }
        next_sample += sample;
        *out++ = this_sample + raw_amount * (raw_sample - this_sample);
    }
    c->next_sample = next_sample;
    c->sample = sample;
    pl_interp_done(&fm);
}

/* ----------------------------------------------------- noise_engine.cc --- */
typedef struct {
    pl_noise_clocked_t clocked_noise[2];
    pl_svf_t lp_hp_filter;
    pl_svf_t bp_filter[2];
    float previous_f0, previous_f1, previous_q, previous_mode;
    float temp_buffer[PL_MAX_BLOCK];
} pl_noise_t;

static void pl_noise_init(pl_noise_t *e)
{
    pl_noise_clocked_init(&e->clocked_noise[0]);
    pl_noise_clocked_init(&e->clocked_noise[1]);
    pl_svf_init(&e->lp_hp_filter);
    pl_svf_init(&e->bp_filter[0]);
    pl_svf_init(&e->bp_filter[1]);
    e->previous_f0 = 0.0f;
    e->previous_f1 = 0.0f;
    e->previous_q = 0.0f;
    e->previous_mode = 0.0f;
}
static void pl_noise_reset(pl_noise_t *e) { (void)e; }

static __attribute__((section(".text.xip_macro"))) void pl_noise_render(pl_noise_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);
    const float f1 = pl_note_to_frequency(p->note + p->harmonics * 48.0f - 24.0f);
    const float clock_lowest_note = (p->trigger & PL_TRIGGER_UNPATCHED) ? 0.0f : -24.0f;
    const float clock_f = pl_note_to_frequency(p->timbre * (128.0f - clock_lowest_note) + clock_lowest_note);
    const float q = 0.5f * pl_semitones_to_ratio(p->morph * 120.0f);
    const int sync = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;
    pl_noise_clocked_render(&e->clocked_noise[0], sync, clock_f, aux, size);
    pl_noise_clocked_render(&e->clocked_noise[1], sync, clock_f * f1 / f0, e->temp_buffer, size);

    pl_interp_t f0_modulation, f1_modulation, q_modulation, mode_modulation;
    pl_interp_init(&f0_modulation, &e->previous_f0, f0, size);
    pl_interp_init(&f1_modulation, &e->previous_f1, f1, size);
    pl_interp_init(&q_modulation, &e->previous_q, q, size);
    pl_interp_init(&mode_modulation, &e->previous_mode, p->harmonics, size);

    const float *in_1 = aux;
    const float *in_2 = e->temp_buffer;
    while (size--) {
        const float f0_i = pl_interp_next(&f0_modulation);
        const float f1_i = pl_interp_next(&f1_modulation);
        const float q_i = pl_interp_next(&q_modulation);
        const float gain = 1.0f / pl_sqrt((0.5f + q_i) * 40.0f * f0_i);
        pl_svf_set_f_q(&e->lp_hp_filter, PL_FREQ_ACCURATE, f0_i, q_i);
        pl_svf_set_f_q(&e->bp_filter[0], PL_FREQ_ACCURATE, f0_i, q_i);
        pl_svf_set_f_q(&e->bp_filter[1], PL_FREQ_ACCURATE, f1_i, q_i);

        float input_1 = *in_1++ * gain;
        float input_2 = *in_2++ * gain;
        /* (aux holds the first noise and is overwritten in place, each sample after it is read, as in the C++) */
        pl_svf_process_multimode_lp_to_hp(&e->lp_hp_filter, &input_1, out++, 1, pl_interp_next(&mode_modulation));
        *aux++ = pl_svf_process(&e->bp_filter[0], PL_BP, input_1) + pl_svf_process(&e->bp_filter[1], PL_BP, input_2);
    }
    pl_interp_done(&f0_modulation);
    pl_interp_done(&f1_modulation);
    pl_interp_done(&q_modulation);
    pl_interp_done(&mode_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_noise_flush(pl_noise_t *e) { pl_flush_svf(&e->lp_hp_filter); pl_flush_svf(&e->bp_filter[0]); pl_flush_svf(&e->bp_filter[1]); }
