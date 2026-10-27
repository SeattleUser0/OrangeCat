/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 13, BASS DRUM (plaits/dsp/engine/bass_drum_engine.cc), with its voices
 * (drums/analog_bass_drum.h, drums/synthetic_bass_drum.h) and fx/overdrive.h (in pl_grp_d.h), in C.
 *   OUT: the 808-style analog model (a pulse ringing a resonant filter), through the overdrive.
 *   AUX: the synthetic model (a distorted sine with pitch and amplitude envelopes, a click and noise).
 * Every time constant is written with PL_SAMPLE_RATE: the C++'s figures at 48 kHz (PL_PARITY), the device's at
 * 44.1 kHz (so the drum's decays and pulse lengths keep their durations in seconds on the FM-1).
 * The C++ SyntheticBassDrum never sets transient_env_ / transient_env_lp_ in Init (they are set on the first trigger,
 * and only read after one); the port sets them to 0.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_grp_d.h"

/* ------------------------------------------------ analog_bass_drum.h --- */
typedef struct {
    int pulse_remaining_samples, fm_pulse_remaining_samples;
    float pulse, pulse_height, pulse_lp, fm_pulse_lp, retrig_pulse, lp_out, tone_lp, sustain_gain;
    pl_svf_t resonator;
    pl_sine_osc_t oscillator;                   /* replaces the resonator in "free running" (sustain) mode */
} pl_bd_analog_t;

static inline void pl_bd_analog_init(pl_bd_analog_t *d)
{
    d->pulse_remaining_samples = 0;
    d->fm_pulse_remaining_samples = 0;
    d->pulse = 0.0f;
    d->pulse_height = 0.0f;
    d->pulse_lp = 0.0f;
    d->fm_pulse_lp = 0.0f;
    d->retrig_pulse = 0.0f;
    d->lp_out = 0.0f;
    d->tone_lp = 0.0f;
    d->sustain_gain = 0.0f;
    pl_svf_init(&d->resonator);
    pl_sine_osc_init(&d->oscillator);
}

static inline float pl_bd_diode(float x)
{
    if (x >= 0.0f) {
        return x;
    } else {
        x *= 2.0f;
        return 0.7f * x / (1.0f + pl_fabsf(x));
    }
}

static void pl_bd_analog_render(pl_bd_analog_t *d, int sustain, int trigger, float accent, float f0, float tone,
                                float decay, float attack_fm_amount, float self_fm_amount, float *out, size_t size)
{
    const int k_trigger_pulse_duration = (int)(1.0e-3f * PL_SAMPLE_RATE);
    const int k_fm_pulse_duration = (int)(6.0e-3f * PL_SAMPLE_RATE);
    const float k_pulse_decay_time = 0.2e-3f * PL_SAMPLE_RATE;
    const float k_pulse_filter_time = 0.1e-3f * PL_SAMPLE_RATE;
    const float k_retrig_pulse_duration = 0.05f * PL_SAMPLE_RATE;

    const float scale = 0.001f / f0;
    const float q = 1500.0f * pl_semitones_to_ratio(decay * 80.0f);
    const float tone_f = pl_minf(4.0f * f0 * pl_semitones_to_ratio(tone * 108.0f), 1.0f);
    const float exciter_leak = 0.08f * (tone + 0.25f);

    if (trigger) {
        d->pulse_remaining_samples = k_trigger_pulse_duration;
        d->fm_pulse_remaining_samples = k_fm_pulse_duration;
        d->pulse_height = 3.0f + 7.0f * accent;
        d->lp_out = 0.0f;
    }

    pl_interp_t sustain_gain;
    pl_interp_init(&sustain_gain, &d->sustain_gain, accent * decay, size);

    while (size--) {
        /* Q39 / Q40 */
        float pulse = 0.0f;
        if (d->pulse_remaining_samples) {
            --d->pulse_remaining_samples;
            pulse = d->pulse_remaining_samples ? d->pulse_height : d->pulse_height - 1.0f;
            d->pulse = pulse;
        } else {
            d->pulse *= 1.0f - 1.0f / k_pulse_decay_time;
            pulse = d->pulse;
        }
        if (sustain) pulse = 0.0f;

        /* C40 / R163 / R162 / D83 */
        PL_ONE_POLE(d->pulse_lp, pulse, 1.0f / k_pulse_filter_time);
        pulse = pl_bd_diode((pulse - d->pulse_lp) + pulse * 0.044f);

        /* Q41 / Q42 */
        float fm_pulse = 0.0f;
        if (d->fm_pulse_remaining_samples) {
            --d->fm_pulse_remaining_samples;
            fm_pulse = 1.0f;
            d->retrig_pulse = d->fm_pulse_remaining_samples ? 0.0f : -0.8f;     /* C39 / C52 */
        } else {
            d->retrig_pulse *= 1.0f - 1.0f / k_retrig_pulse_duration;          /* C39 / R161 */
        }
        if (sustain) fm_pulse = 0.0f;
        PL_ONE_POLE(d->fm_pulse_lp, fm_pulse, 1.0f / k_pulse_filter_time);

        /* Q43 and R170 leakage */
        float punch = 0.7f + pl_bd_diode(10.0f * d->lp_out - 1.0f);

        /* Q43 / R165 */
        float attack_fm = d->fm_pulse_lp * 1.7f * attack_fm_amount;
        float self_fm = punch * 0.08f * self_fm_amount;
        float f = f0 * (1.0f + attack_fm + self_fm);
        PL_CONSTRAIN(f, 0.0f, 0.4f);

        float resonator_out;
        if (sustain) {
            pl_sine_osc_next_quad(&d->oscillator, f, pl_interp_next(&sustain_gain), &resonator_out, &d->lp_out);
        } else {
            pl_svf_set_f_q(&d->resonator, PL_FREQ_DIRTY, f, 1.0f + q * f);
            pl_svf_process2(&d->resonator, PL_BP, PL_LP, (pulse - d->retrig_pulse * 0.2f) * scale,
                            &resonator_out, &d->lp_out);
        }

        PL_ONE_POLE(d->tone_lp, pulse * exciter_leak + resonator_out, tone_f);
        *out++ = d->tone_lp;
    }
    pl_interp_done(&sustain_gain);
}

/* --------------------------------------------- synthetic_bass_drum.h --- */
typedef struct { float lp, hp; pl_svf_t filter; } pl_bd_click_t;            /* SyntheticBassDrumClick */
static inline void pl_bd_click_init(pl_bd_click_t *c)
{
    c->lp = 0.0f;
    c->hp = 0.0f;
    pl_svf_init(&c->filter);
    pl_svf_set_f_q(&c->filter, PL_FREQ_FAST, 5000.0f / PL_SAMPLE_RATE, 2.0f);
}
static inline float pl_bd_click_process(pl_bd_click_t *c, float in)
{
    PL_SLOPE(c->lp, in, 0.5f, 0.1f);
    PL_ONE_POLE(c->hp, c->lp, 0.04f);
    return pl_svf_process(&c->filter, PL_LP, c->lp - c->hp);
}

typedef struct { float lp, hp; } pl_bd_attack_noise_t;                       /* SyntheticBassDrumAttackNoise */
static inline void pl_bd_attack_noise_init(pl_bd_attack_noise_t *n) { n->lp = 0.0f; n->hp = 0.0f; }
static inline float pl_bd_attack_noise_render(pl_bd_attack_noise_t *n)
{
    float sample = pl_rng_float();
    PL_ONE_POLE(n->lp, sample, 0.05f);
    PL_ONE_POLE(n->hp, n->lp, 0.005f);
    return n->lp - n->hp;
}

typedef struct {
    float f0, phase, phase_noise;
    float fm, fm_lp, body_env, body_env_lp, transient_env, transient_env_lp;
    float sustain_gain;
    float tone_lp;
    pl_bd_click_t click;
    pl_bd_attack_noise_t noise;
    int body_env_pulse_width, fm_pulse_width;
} pl_bd_synth_t;

static inline void pl_bd_synth_init(pl_bd_synth_t *d)
{
    d->phase = 0.0f;
    d->phase_noise = 0.0f;
    d->f0 = 0.0f;
    d->fm = 0.0f;
    d->fm_lp = 0.0f;
    d->body_env_lp = 0.0f;
    d->body_env = 0.0f;
    d->body_env_pulse_width = 0;
    d->fm_pulse_width = 0;
    d->tone_lp = 0.0f;
    d->sustain_gain = 0.0f;
    d->transient_env = 0.0f;                    /* (not set by the C++ Init: see the header comment) */
    d->transient_env_lp = 0.0f;
    pl_bd_click_init(&d->click);
    pl_bd_attack_noise_init(&d->noise);
}

static inline float pl_bd_distorted_sine(float phase, float phase_noise, float dirtiness)
{
    phase += phase_noise * dirtiness;
    PL_MAKE_INTEGRAL_FRACTIONAL(phase)
    (void)phase_integral;
    phase = phase_fractional;
    float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.0f;
    float sine = 2.0f * triangle / (1.0f + pl_fabsf(triangle));
    float clean_sine = pl_sine(phase + 0.75f);
    return sine + (1.0f - dirtiness) * (clean_sine - sine);
}

static inline float pl_bd_transistor_vca(float s, float gain)
{
    s = (s - 0.6f) * gain;
    return 3.0f * s / (2.0f + pl_fabsf(s)) + gain * 0.3f;
}

static void pl_bd_synth_render(pl_bd_synth_t *d, int sustain, int trigger, float accent, float f0, float tone,
                               float decay, float dirtiness, float fm_envelope_amount, float fm_envelope_decay,
                               float *out, size_t size)
{
    decay *= decay;
    fm_envelope_decay *= fm_envelope_decay;

    pl_interp_t f0_mod;
    pl_interp_init(&f0_mod, &d->f0, f0, size);

    dirtiness *= pl_maxf(1.0f - 8.0f * f0, 0.0f);

    const float fm_decay = 1.0f - 1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * PL_SAMPLE_RATE);
    const float body_env_decay = 1.0f - 1.0f / (0.02f * PL_SAMPLE_RATE) * pl_semitones_to_ratio(-decay * 60.0f);
    const float transient_env_decay = 1.0f - 1.0f / (0.005f * PL_SAMPLE_RATE);
    const float tone_f = pl_minf(4.0f * f0 * pl_semitones_to_ratio(tone * 108.0f), 1.0f);
    const float transient_level = tone;

    if (trigger) {
        d->fm = 1.0f;
        d->body_env = d->transient_env = 0.3f + 0.7f * accent;
        d->body_env_pulse_width = (int)(PL_SAMPLE_RATE * 0.001f);
        d->fm_pulse_width = (int)(PL_SAMPLE_RATE * 0.0013f);
    }

    pl_interp_t sustain_gain;
    pl_interp_init(&sustain_gain, &d->sustain_gain, accent * decay, size);

    while (size--) {
        PL_ONE_POLE(d->phase_noise, pl_rng_float() - 0.5f, 0.002f);

        float mix = 0.0f;
        if (sustain) {
            d->phase += pl_interp_next(&f0_mod);
            if (d->phase >= 1.0f) d->phase -= 1.0f;
            float body = pl_bd_distorted_sine(d->phase, d->phase_noise, dirtiness);
            mix -= pl_bd_transistor_vca(body, pl_interp_next(&sustain_gain));
        } else {
            if (d->fm_pulse_width) {
                --d->fm_pulse_width;
                d->phase = 0.25f;
            } else {
                d->fm *= fm_decay;
                float fm = 1.0f + fm_envelope_amount * 3.5f * d->fm_lp;
                d->phase += pl_minf(pl_interp_next(&f0_mod) * fm, 0.5f);
                if (d->phase >= 1.0f) d->phase -= 1.0f;
            }

            if (d->body_env_pulse_width) {
                --d->body_env_pulse_width;
            } else {
                d->body_env *= body_env_decay;
                d->transient_env *= transient_env_decay;
            }

            const float envelope_lp_f = 0.1f;
            PL_ONE_POLE(d->body_env_lp, d->body_env, envelope_lp_f);
            PL_ONE_POLE(d->transient_env_lp, d->transient_env, envelope_lp_f);
            PL_ONE_POLE(d->fm_lp, d->fm, envelope_lp_f);

            float body = pl_bd_distorted_sine(d->phase, d->phase_noise, dirtiness);
            float transient = pl_bd_click_process(&d->click, d->body_env_pulse_width ? 0.0f : 1.0f)
                            + pl_bd_attack_noise_render(&d->noise);

            mix -= pl_bd_transistor_vca(body, d->body_env_lp);
            mix -= transient * d->transient_env_lp * transient_level;
        }

        PL_ONE_POLE(d->tone_lp, mix, tone_f);
        *out++ = d->tone_lp;
    }
    /* (the C++ destructors run in reverse order: sustain_gain, then f0_mod) */
    pl_interp_done(&sustain_gain);
    pl_interp_done(&f0_mod);
}

/* ------------------------------------------------ bass_drum_engine.cc --- */
typedef struct {
    pl_bd_analog_t analog_bass_drum;
    pl_bd_synth_t synthetic_bass_drum;
    pld_overdrive_t overdrive;
} pl_bd_t;

static void pl_bd_init(pl_bd_t *e)
{
    pl_bd_analog_init(&e->analog_bass_drum);
    pl_bd_synth_init(&e->synthetic_bass_drum);
    pld_overdrive_init(&e->overdrive);
}
static void pl_bd_reset(pl_bd_t *e) { (void)e; }

static __attribute__((section(".text.xip_macro"))) void pl_bd_render(pl_bd_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);

    const float attack_fm_amount = pl_minf(p->harmonics * 4.0f, 1.0f);
    const float self_fm_amount = pl_maxf(pl_minf(p->harmonics * 4.0f - 1.0f, 1.0f), 0.0f);
    const float drive = pl_maxf(p->harmonics * 2.0f - 1.0f, 0.0f) * pl_maxf(1.0f - 16.0f * f0, 0.0f);

    const int sustain = (p->trigger & PL_TRIGGER_UNPATCHED) != 0;
    const int trigger = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;

    pl_bd_analog_render(&e->analog_bass_drum, sustain, trigger, p->accent, f0, p->timbre, p->morph,
                        attack_fm_amount, self_fm_amount, out, size);

    pld_overdrive_process(&e->overdrive, 0.5f + 0.5f * drive, out, size);

    pl_bd_synth_render(&e->synthetic_bass_drum, sustain, trigger, p->accent, f0, p->timbre, p->morph,
                       sustain ? p->harmonics : 0.4f - 0.25f * p->morph * p->morph,
                       pl_minf(p->harmonics * 2.0f, 1.0f),
                       pl_maxf(p->harmonics * 2.0f - 1.0f, 0.0f),
                       aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_bd_flush(pl_bd_t *e)
{
    pl_bd_analog_t *a = &e->analog_bass_drum;
    pl_bd_synth_t *s = &e->synthetic_bass_drum;
    pl_flush(&a->pulse); pl_flush(&a->pulse_lp); pl_flush(&a->fm_pulse_lp); pl_flush(&a->retrig_pulse);
    pl_flush(&a->lp_out); pl_flush(&a->tone_lp); pl_flush_svf(&a->resonator);
    pl_flush(&s->fm); pl_flush(&s->fm_lp); pl_flush(&s->body_env); pl_flush(&s->body_env_lp);
    pl_flush(&s->transient_env); pl_flush(&s->transient_env_lp); pl_flush(&s->tone_lp);
    pl_flush(&s->click.lp); pl_flush(&s->click.hp); pl_flush_svf(&s->click.filter);
    pl_flush(&s->noise.lp); pl_flush(&s->noise.hp);
}
