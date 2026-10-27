/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 14, SNARE DRUM (plaits/dsp/engine/snare_drum_engine.cc), with its voices
 * (drums/analog_snare_drum.h, drums/synthetic_snare_drum.h), in C.
 *   OUT: the 808-style analog model (a pulse ringing five resonant modes, plus band-passed noise).
 *   AUX: the 909-style synthetic model (two coupled distorted sines plus filtered noise).
 * Every time constant the C++ writes with kSampleRate is written with PL_SAMPLE_RATE: the C++'s figures at 48 kHz
 * (PL_PARITY), the device's at 44.1 kHz. The constants the C++ gives per sample (the analog model's noise envelope
 * 0.0017, the one-pole coefficients) stay as they are, so on the device those run 8.8 % longer, as pl_dsp.h says.
 * stmlib::Sqrt is pl_sqrt (sqrtf in the parity build, a Newton approximation on the device).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_grp_d.h"

/* ----------------------------------------------- analog_snare_drum.h --- */
#define PL_SD_NUM_MODES 5
static const float pl_sd_mode_frequencies[PL_SD_NUM_MODES] = { 1.00f, 2.00f, 3.18f, 4.16f, 5.62f };

typedef struct {
    int pulse_remaining_samples;
    float pulse, pulse_height, pulse_lp, noise_envelope, sustain_gain;
    pl_svf_t resonator[PL_SD_NUM_MODES];
    pl_svf_t noise_filter;
    pl_sine_osc_t oscillator[PL_SD_NUM_MODES]; /* replace the resonators in "free running" (sustain) mode */
} pl_sd_analog_t;

static inline void pl_sd_analog_init(pl_sd_analog_t *d)
{
    d->pulse_remaining_samples = 0;
    d->pulse = 0.0f;
    d->pulse_height = 0.0f;
    d->pulse_lp = 0.0f;
    d->noise_envelope = 0.0f;
    d->sustain_gain = 0.0f;
    for (int i = 0; i < PL_SD_NUM_MODES; ++i) {
        pl_svf_init(&d->resonator[i]);
        pl_sine_osc_init(&d->oscillator[i]);
    }
    pl_svf_init(&d->noise_filter);
}

static void pl_sd_analog_render(pl_sd_analog_t *d, int sustain, int trigger, float accent, float f0, float tone,
                                float decay, float snappy, float *out, size_t size)
{
    const float decay_xt = decay * (1.0f + decay * (decay - 1.0f));
    const int k_trigger_pulse_duration = (int)(1.0e-3f * PL_SAMPLE_RATE);
    const float k_pulse_decay_time = 0.1e-3f * PL_SAMPLE_RATE;
    const float q = 2000.0f * pl_semitones_to_ratio(decay_xt * 84.0f);
    const float noise_envelope_decay = 1.0f - 0.0017f * pl_semitones_to_ratio(-decay * (50.0f + snappy * 10.0f));
    const float exciter_leak = snappy * (2.0f - snappy) * 0.1f;

    snappy = snappy * 1.1f - 0.05f;
    PL_CONSTRAIN(snappy, 0.0f, 1.0f);

    if (trigger) {
        d->pulse_remaining_samples = k_trigger_pulse_duration;
        d->pulse_height = 3.0f + 7.0f * accent;
        d->noise_envelope = 2.0f;
    }

    float f[PL_SD_NUM_MODES];
    float gain[PL_SD_NUM_MODES];
    for (int i = 0; i < PL_SD_NUM_MODES; ++i) {
        f[i] = pl_minf(f0 * pl_sd_mode_frequencies[i], 0.499f);
        pl_svf_set_f_q(&d->resonator[i], PL_FREQ_FAST, f[i], 1.0f + f[i] * (i == 0 ? q : q * 0.25f));
    }

    if (tone < 0.666667f) {
        /* 808-style (2 modes) */
        tone *= 1.5f;
        gain[0] = 1.5f + (1.0f - tone) * (1.0f - tone) * 4.5f;
        gain[1] = 2.0f * tone + 0.15f;
        for (int i = 2; i < PL_SD_NUM_MODES; ++i) gain[i] = 0.0f;
    } else {
        /* What the 808 could have been if there were extra modes! */
        tone = (tone - 0.666667f) * 3.0f;
        gain[0] = 1.5f - tone * 0.5f;
        gain[1] = 2.15f - tone * 0.7f;
        for (int i = 2; i < PL_SD_NUM_MODES; ++i) {
            gain[i] = tone;
            tone *= tone;
        }
    }

    float f_noise = f0 * 16.0f;
    PL_CONSTRAIN(f_noise, 0.0f, 0.499f);
    pl_svf_set_f_q(&d->noise_filter, PL_FREQ_FAST, f_noise, 1.0f + f_noise * 1.5f);

    pl_interp_t sustain_gain;
    pl_interp_init(&sustain_gain, &d->sustain_gain, accent * decay, size);

    while (size--) {
        /* Q45 / Q46 */
        float pulse = 0.0f;
        if (d->pulse_remaining_samples) {
            --d->pulse_remaining_samples;
            pulse = d->pulse_remaining_samples ? d->pulse_height : d->pulse_height - 1.0f;
            d->pulse = pulse;
        } else {
            d->pulse *= 1.0f - 1.0f / k_pulse_decay_time;
            pulse = d->pulse;
        }

        float sustain_gain_value = pl_interp_next(&sustain_gain);

        /* R189 / C57 / R190 + C58 / C59 / R197 / R196 / IC14 */
        PL_ONE_POLE(d->pulse_lp, pulse, 0.75f);

        float shell = 0.0f;
        for (int i = 0; i < PL_SD_NUM_MODES; ++i) {
            float excitation = i == 0 ? (pulse - d->pulse_lp) + 0.006f * pulse : 0.026f * pulse;
            shell += gain[i] * (sustain
                ? pl_sine_osc_next(&d->oscillator[i], f[i]) * sustain_gain_value * 0.25f
                : pl_svf_process(&d->resonator[i], PL_BP, excitation) + excitation * exciter_leak);
        }
        shell = pl_soft_clip(shell);

        /* C56 / R194 / Q48 / C54 / R188 / D54 */
        float noise = 2.0f * pl_rng_float() - 1.0f;
        if (noise < 0.0f) noise = 0.0f;
        d->noise_envelope *= noise_envelope_decay;
        noise *= (sustain ? sustain_gain_value : d->noise_envelope) * snappy * 2.0f;

        /* C66 / R201 / C67 / R202 / R203 / Q49 */
        noise = pl_svf_process(&d->noise_filter, PL_BP, noise);

        /* IC13 */
        *out++ = noise + shell * (1.0f - snappy);
    }
    pl_interp_done(&sustain_gain);
}

/* -------------------------------------------- synthetic_snare_drum.h --- */
typedef struct {
    float phase[2];
    float drum_amplitude, snare_amplitude, fm, sustain_gain;
    int hold_counter;
    pl_onepole_t drum_lp, snare_hp;
    pl_svf_t snare_lp;
} pl_sd_synth_t;

static inline void pl_sd_synth_init(pl_sd_synth_t *d)
{
    d->phase[0] = 0.0f;
    d->phase[1] = 0.0f;
    d->drum_amplitude = 0.0f;
    d->snare_amplitude = 0.0f;
    d->fm = 0.0f;
    d->hold_counter = 0;
    d->sustain_gain = 0.0f;
    pl_onepole_init(&d->drum_lp);
    pl_onepole_init(&d->snare_hp);
    pl_svf_init(&d->snare_lp);
}

static inline float pl_sd_distorted_sine(float phase)
{
    float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.3f;
    return 2.0f * triangle / (1.0f + pl_fabsf(triangle));
}

static void pl_sd_synth_render(pl_sd_synth_t *d, int sustain, int trigger, float accent, float f0, float fm_amount,
                               float decay, float snappy, float *out, size_t size)
{
    const float decay_xt = decay * (1.0f + decay * (decay - 1.0f));
    fm_amount *= fm_amount;
    const float drum_decay = 1.0f - 1.0f / (0.015f * PL_SAMPLE_RATE)
        * pl_semitones_to_ratio(-decay_xt * 72.0f - fm_amount * 12.0f + snappy * 7.0f);
    const float snare_decay = 1.0f - 1.0f / (0.01f * PL_SAMPLE_RATE)
        * pl_semitones_to_ratio(-decay * 60.0f - snappy * 7.0f);
    const float fm_decay = 1.0f - 1.0f / (0.007f * PL_SAMPLE_RATE);

    snappy = snappy * 1.1f - 0.05f;
    PL_CONSTRAIN(snappy, 0.0f, 1.0f);

    const float drum_level = pl_sqrt(1.0f - snappy);
    const float snare_level = pl_sqrt(snappy);

    const float snare_f_min = pl_minf(10.0f * f0, 0.5f);
    const float snare_f_max = pl_minf(35.0f * f0, 0.5f);

    pl_onepole_set_f(&d->snare_hp, PL_FREQ_FAST, snare_f_min);
    pl_svf_set_f_q(&d->snare_lp, PL_FREQ_FAST, snare_f_max, 0.5f + 2.0f * snappy);
    pl_onepole_set_f(&d->drum_lp, PL_FREQ_FAST, 3.0f * f0);

    if (trigger) {
        d->snare_amplitude = d->drum_amplitude = 0.3f + 0.7f * accent;
        d->fm = 1.0f;
        d->phase[0] = d->phase[1] = 0.0f;
        d->hold_counter = (int)((0.04f + decay * 0.03f) * PL_SAMPLE_RATE);
    }

    pl_interp_t sustain_gain;
    pl_interp_init(&sustain_gain, &d->sustain_gain, accent * decay, size);
    while (size--) {
        if (sustain) {
            d->snare_amplitude = pl_interp_next(&sustain_gain);
            d->drum_amplitude = d->snare_amplitude;
            d->fm = 0.0f;
        } else {
            /* The drum's envelope has a very long tail; the snare's has a "hold" stage of 40 to 70 ms. */
            d->drum_amplitude *= (d->drum_amplitude > 0.03f || !(size & 1)) ? drum_decay : 1.0f;
            if (d->hold_counter) {
                --d->hold_counter;
            } else {
                d->snare_amplitude *= snare_decay;
            }
            d->fm *= fm_decay;
        }

        /* The 909's oscillator coupling: the signal leaving Q40's collector and resetting all oscillators allows
         * some intermodulation. */
        float reset_noise = 0.0f;
        float reset_noise_amount = (0.125f - f0) * 8.0f;
        PL_CONSTRAIN(reset_noise_amount, 0.0f, 1.0f);
        reset_noise_amount *= reset_noise_amount;
        reset_noise_amount *= fm_amount;
        reset_noise += d->phase[0] > 0.5f ? -1.0f : 1.0f;
        reset_noise += d->phase[1] > 0.5f ? -1.0f : 1.0f;
        reset_noise *= reset_noise_amount * 0.025f;

        float f = f0 * (1.0f + fm_amount * (4.0f * d->fm));
        d->phase[0] += f;
        d->phase[1] += f * 1.47f;
        if (reset_noise_amount > 0.1f) {
            if (d->phase[0] >= 1.0f + reset_noise) d->phase[0] = 1.0f - d->phase[0];
            if (d->phase[1] >= 1.0f + reset_noise) d->phase[1] = 1.0f - d->phase[1];
        } else {
            if (d->phase[0] >= 1.0f) d->phase[0] -= 1.0f;
            if (d->phase[1] >= 1.0f) d->phase[1] -= 1.0f;
        }

        float drum = -0.1f;
        drum += pl_sd_distorted_sine(d->phase[0]) * 0.60f;
        drum += pl_sd_distorted_sine(d->phase[1]) * 0.25f;
        drum *= d->drum_amplitude * drum_level;
        drum = pl_onepole_process(&d->drum_lp, PL_LP, drum);

        float noise = pl_rng_float();
        float snare = pl_svf_process(&d->snare_lp, PL_LP, noise);
        snare = pl_onepole_process(&d->snare_hp, PL_HP, snare);
        snare = (snare + 0.1f) * (d->snare_amplitude + d->fm) * snare_level;

        *out++ = snare + drum;                  /* It's a snare, it's a drum, it's a snare drum. */
    }
    pl_interp_done(&sustain_gain);
}

/* ----------------------------------------------- snare_drum_engine.cc --- */
typedef struct {
    pl_sd_analog_t analog_snare_drum;
    pl_sd_synth_t synthetic_snare_drum;
} pl_sd_t;

static void pl_sd_init(pl_sd_t *e)
{
    pl_sd_analog_init(&e->analog_snare_drum);
    pl_sd_synth_init(&e->synthetic_snare_drum);
}
static void pl_sd_reset(pl_sd_t *e) { (void)e; }

static __attribute__((section(".text.xip_macro"))) void pl_sd_render(pl_sd_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);
    const int sustain = (p->trigger & PL_TRIGGER_UNPATCHED) != 0;
    const int trigger = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;

    pl_sd_analog_render(&e->analog_snare_drum, sustain, trigger, p->accent, f0, p->timbre, p->morph, p->harmonics,
                        out, size);
    pl_sd_synth_render(&e->synthetic_snare_drum, sustain, trigger, p->accent, f0, p->timbre, p->morph, p->harmonics,
                       aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_sd_flush(pl_sd_t *e)
{
    pl_sd_analog_t *a = &e->analog_snare_drum;
    pl_sd_synth_t *s = &e->synthetic_snare_drum;
    pl_flush(&a->pulse); pl_flush(&a->pulse_lp); pl_flush(&a->noise_envelope);
    for (int i = 0; i < PL_SD_NUM_MODES; i++) pl_flush_svf(&a->resonator[i]);
    pl_flush_svf(&a->noise_filter);
    pl_flush(&s->drum_amplitude); pl_flush(&s->snare_amplitude); pl_flush(&s->fm);
    pl_flush_onepole(&s->drum_lp); pl_flush_onepole(&s->snare_hp); pl_flush_svf(&s->snare_lp);
}
