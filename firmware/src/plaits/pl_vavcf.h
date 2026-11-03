/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2: Plaits 1.2's VIRTUAL ANALOG VCF (plaits/dsp/engine2/virtual_analog_vcf_engine.cc and .h),
 * in C. Its oscillators (oscillator/variable_shape_oscillator.h) are pl_vshape_t in pl_va.h; stmlib's Svf
 * (set_f_q<FREQUENCY_FAST>, Process<LOW_PASS, HIGH_PASS>) is pl_svf_t in pl_dsp.h; SoftClip is pl_soft_clip.
 *   A variable-shape oscillator (saw to square to PWM) plus a square sub an octave down, through two state
 *   variable filters in series, each soft-clipped.
 *   MORPH: the waveform (saw, square, pulse width; the sub comes in at both ends). TIMBRE: the cutoff.
 *   HARMONICS: the resonance (symmetric about 12 o'clock), and above 12 o'clock the second filter stage fades out.
 *   OUT: the low-pass. AUX: the high-pass of the first stage.
 * Same as the C++ (no user data, no allocator buffers).
 */
#pragma once
#include "pl_va.h"

typedef struct {
    pl_svf_t svf[2];
    pl_vshape_t oscillator, sub_oscillator;
    float previous_cutoff, previous_stage2_gain, previous_q, previous_gain, previous_sub_gain;
} pl_vavcf_t;

static void pl_vavcf_init(pl_vavcf_t *e)
{
    pl_vshape_init(&e->oscillator);
    pl_vshape_init(&e->sub_oscillator);
    pl_svf_init(&e->svf[0]);
    pl_svf_init(&e->svf[1]);
    e->previous_sub_gain = 0.0f;
    e->previous_cutoff = 0.0f;
    e->previous_stage2_gain = 0.0f;
    e->previous_q = 0.0f;
    e->previous_gain = 0.0f;
}
static void pl_vavcf_reset(pl_vavcf_t *e) { (void)e; }

static void pl_vavcf_render(pl_vavcf_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    /* VA oscillator (saw or PW square) + sub */
    const float f0 = pl_note_to_frequency(p->note);
    size_t i;

    float shape = (p->morph - 0.25f) * 2.0f + 0.5f;
    PL_CONSTRAIN(shape, 0.5f, 1.0f);

    float pw = (p->morph - 0.5f) * 2.0f + 0.5f;
    if (p->morph > 0.75f) pw = 2.5f - p->morph * 2.0f;
    PL_CONSTRAIN(pw, 0.5f, 0.98f);

    float sub_gain = pl_maxf(pl_fabsf(p->morph - 0.5f) - 0.3f, 0.0f) * 5.0f;

    pl_vshape_render(&e->oscillator, f0, pw, shape, out, size);
    pl_vshape_render(&e->sub_oscillator, f0 * 0.501f, 0.5f, 1.0f, aux, size);

    const float cutoff = f0 * pl_semitones_to_ratio((p->timbre - 0.2f) * 120.0f);

    float stage2_gain = 1.0f - (p->harmonics - 0.4f) * 4.0f;
    PL_CONSTRAIN(stage2_gain, 0.0f, 1.0f);

    const float resonance = 2.667f * pl_maxf(pl_fabsf(p->harmonics - 0.5f) - 0.125f, 0.0f);
    const float resonance_sqr = resonance * resonance;
    const float q = resonance_sqr * resonance_sqr * 48.0f;
    float gain = (p->harmonics - 0.7f) + 0.85f;
    PL_CONSTRAIN(gain, 0.7f - resonance_sqr * 0.3f, 1.0f);

    pl_interp_t sub_gain_modulation, cutoff_modulation, stage2_gain_modulation, q_modulation, gain_modulation;
    pl_interp_init(&sub_gain_modulation, &e->previous_sub_gain, sub_gain, size);
    pl_interp_init(&cutoff_modulation, &e->previous_cutoff, cutoff, size);
    pl_interp_init(&stage2_gain_modulation, &e->previous_stage2_gain, stage2_gain, size);
    pl_interp_init(&q_modulation, &e->previous_q, q, size);
    pl_interp_init(&gain_modulation, &e->previous_gain, gain, size);

    for (i = 0; i < size; ++i) {
        const float cutoff_i = pl_minf(pl_interp_next(&cutoff_modulation), 0.25f);
        const float q_i = pl_interp_next(&q_modulation);
        const float stage2_gain_i = pl_interp_next(&stage2_gain_modulation);

        pl_svf_set_f_q(&e->svf[0], PL_FREQ_FAST, cutoff_i, 0.5f + q_i);
        pl_svf_set_f_q(&e->svf[1], PL_FREQ_FAST, cutoff_i, 0.5f + 0.025f * q_i);

        const float gain_i = pl_interp_next(&gain_modulation);
        const float input = pl_soft_clip((out[i] + aux[i] * pl_interp_next(&sub_gain_modulation)) * gain_i);

        float lp, hp;
        pl_svf_process2(&e->svf[0], PL_LP, PL_HP, input, &lp, &hp);

        lp = pl_soft_clip(lp * gain_i);
        lp += stage2_gain_i * (pl_soft_clip(pl_svf_process(&e->svf[1], PL_LP, lp)) - lp);

        out[i] = lp;
        aux[i] = pl_soft_clip(hp * gain_i);
    }
    /* (the C++ objects go out of scope in reverse order of construction) */
    pl_interp_done(&gain_modulation);
    pl_interp_done(&q_modulation);
    pl_interp_done(&stage2_gain_modulation);
    pl_interp_done(&cutoff_modulation);
    pl_interp_done(&sub_gain_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_vavcf_flush(pl_vavcf_t *e) { pl_flush_svf(&e->svf[0]); pl_flush_svf(&e->svf[1]); }
