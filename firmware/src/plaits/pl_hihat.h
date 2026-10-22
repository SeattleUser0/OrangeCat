/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 15, HI-HAT (plaits/dsp/engine/hi_hat_engine.cc), with drums/hi_hat.h, in C.
 *   OUT: HiHat<SquareNoise, SwingVCA, resonance, one-stage envelope>: six square oscillators (808 metallic noise).
 *   AUX: HiHat<RingModNoise, LinearVCA, no resonance, two-stage envelope>: three ring-modulated oscillator pairs.
 * The C++ template arguments become plain arguments of pl_hh_voice_render (the noise source, the VCA, resonance and
 * the two-stage envelope); each voice keeps its noise source in a union, since it only ever uses one kind.
 * Every frequency the C++ writes with kSampleRate is written with PL_SAMPLE_RATE: the C++'s figures at 48 kHz
 * (PL_PARITY), the device's at 44.1 kHz, so the ring modulator's frequencies and the filters' cutoffs stay in Hz.
 * The decays the C++ gives per sample (0.003, 0.0025) stay as they are, so on the device they run 8.8 % longer.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_grp_d.h"

/* SquareNoise: 808 style "metallic noise" with 6 square oscillators. */
typedef struct { uint32_t phase[6]; } pl_hh_square_noise_t;
static inline void pl_hh_square_noise_init(pl_hh_square_noise_t *n)
{
    for (int i = 0; i < 6; ++i) n->phase[i] = 0;
}
static void pl_hh_square_noise_render(pl_hh_square_noise_t *n, float f0, float *out, size_t size)
{
    const float ratios[6] = {
        /* Nominal f0: 414 Hz */
        1.0f, 1.304f, 1.466f, 1.787f, 1.932f, 2.536f
    };
    uint32_t increment[6];
    uint32_t phase[6];
    for (int i = 0; i < 6; ++i) {
        float f = f0 * ratios[i];
        if (f >= 0.499f) f = 0.499f;
        increment[i] = (uint32_t)(f * 4294967296.0f);
        phase[i] = n->phase[i];
    }
    while (size--) {
        phase[0] += increment[0];
        phase[1] += increment[1];
        phase[2] += increment[2];
        phase[3] += increment[3];
        phase[4] += increment[4];
        phase[5] += increment[5];
        uint32_t noise = 0;
        noise += (phase[0] >> 31);
        noise += (phase[1] >> 31);
        noise += (phase[2] >> 31);
        noise += (phase[3] >> 31);
        noise += (phase[4] >> 31);
        noise += (phase[5] >> 31);
        *out++ = 0.33f * (float)noise - 1.0f;
    }
    for (int i = 0; i < 6; ++i) n->phase[i] = phase[i];
}

/* RingModNoise: three pairs of oscillators (a square times a saw). */
typedef struct { pl_osc_t oscillator[6]; } pl_hh_ring_mod_noise_t;
static inline void pl_hh_ring_mod_noise_init(pl_hh_ring_mod_noise_t *n)
{
    for (int i = 0; i < 6; ++i) pl_osc_init(&n->oscillator[i]);
}
static inline void pl_hh_ring_mod_noise_render_pair(pl_osc_t *osc, const float *f, float *temp_1, float *temp_2,
                                                    float *out, size_t size)
{
    pl_osc_render(&osc[0], PL_SHAPE_SQUARE, f[0], 0.5f, NULL, temp_1, size);
    pl_osc_render(&osc[1], PL_SHAPE_SAW, f[1], 0.5f, NULL, temp_2, size);
    while (size--) *out++ += *temp_1++ * *temp_2++;
}
static void pl_hh_ring_mod_noise_render(pl_hh_ring_mod_noise_t *n, float f0, float *temp_1, float *temp_2, float *out,
                                        size_t size)
{
    const float ratio = f0 / (0.01f + f0);
    const float f1a = 200.0f / PL_SAMPLE_RATE * ratio;
    const float f1b = 7530.0f / PL_SAMPLE_RATE * ratio;
    const float f2a = 510.0f / PL_SAMPLE_RATE * ratio;
    const float f2b = 8075.0f / PL_SAMPLE_RATE * ratio;
    const float f3a = 730.0f / PL_SAMPLE_RATE * ratio;
    const float f3b = 10500.0f / PL_SAMPLE_RATE * ratio;
    const float f[3][2] = { { f1a, f1b }, { f2a, f2b }, { f3a, f3b } };
    for (size_t i = 0; i < size; ++i) out[i] = 0.0f;
    for (int i = 0; i < 3; ++i) pl_hh_ring_mod_noise_render_pair(&n->oscillator[2 * i], f[i], temp_1, temp_2, out, size);
}

/* The VCAs (the C++ functor classes SwingVCA and LinearVCA) */
static inline float pl_hh_swing_vca(float s, float gain)
{
    s *= s > 0.0f ? 4.0f : 0.1f;
    s = s / (1.0f + pl_fabsf(s));
    return (s + 0.1f) * gain;
}
static inline float pl_hh_linear_vca(float s, float gain) { return s * gain; }

/* HiHat<MetallicNoiseSource, VCA, resonance, two_stage_envelope> */
enum { PL_HH_SQUARE_NOISE, PL_HH_RING_MOD_NOISE };        /* MetallicNoiseSource */
enum { PL_HH_SWING_VCA, PL_HH_LINEAR_VCA };                /* VCA */
typedef struct {
    float envelope, noise_clock, noise_sample, sustain_gain;
    union { pl_hh_square_noise_t square; pl_hh_ring_mod_noise_t ring_mod; } metallic_noise;
    pl_svf_t noise_coloration_svf, hpf;
} pl_hh_voice_t;

static inline void pl_hh_voice_init(pl_hh_voice_t *h, int noise_source)
{
    h->envelope = 0.0f;
    h->noise_clock = 0.0f;
    h->noise_sample = 0.0f;
    h->sustain_gain = 0.0f;
    if (noise_source == PL_HH_SQUARE_NOISE) pl_hh_square_noise_init(&h->metallic_noise.square);
    else pl_hh_ring_mod_noise_init(&h->metallic_noise.ring_mod);
    pl_svf_init(&h->noise_coloration_svf);
    pl_svf_init(&h->hpf);
}

static inline __attribute__((always_inline)) void pl_hh_voice_render(pl_hh_voice_t *h, int noise_source, int vca,
    int resonance, int two_stage_envelope, int sustain, int trigger, float accent, float f0, float tone, float decay,
    float noisiness, float *temp_1, float *temp_2, float *out, size_t size)
{
    const float envelope_decay = 1.0f - 0.003f * pl_semitones_to_ratio(-decay * 84.0f);
    const float cut_decay = 1.0f - 0.0025f * pl_semitones_to_ratio(-decay * 36.0f);

    if (trigger) h->envelope = (1.5f + 0.5f * (1.0f - decay)) * (0.3f + 0.7f * accent);

    /* Render the metallic noise. */
    if (noise_source == PL_HH_SQUARE_NOISE)
        pl_hh_square_noise_render(&h->metallic_noise.square, 2.0f * f0, out, size);
    else
        pl_hh_ring_mod_noise_render(&h->metallic_noise.ring_mod, 2.0f * f0, temp_1, temp_2, out, size);

    /* Apply BPF on the metallic noise. */
    float cutoff = 150.0f / PL_SAMPLE_RATE * pl_semitones_to_ratio(tone * 72.0f);
    PL_CONSTRAIN(cutoff, 0.0f, 16000.0f / PL_SAMPLE_RATE);
    pl_svf_set_f_q(&h->noise_coloration_svf, PL_FREQ_ACCURATE, cutoff, resonance ? 3.0f + 3.0f * tone : 1.0f);
    pl_svf_process_buf(&h->noise_coloration_svf, PL_BP, out, out, size);

    /* Not at all part of the 808 circuit, but for more variety: a variable amount of clocked noise added to the
     * output of the 6 schmitt trigger oscillators. */
    noisiness *= noisiness;
    float noise_f = f0 * (16.0f + 16.0f * (1.0f - noisiness));
    PL_CONSTRAIN(noise_f, 0.0f, 0.5f);
    for (size_t i = 0; i < size; ++i) {
        h->noise_clock += noise_f;
        if (h->noise_clock >= 1.0f) {
            h->noise_clock -= 1.0f;
            h->noise_sample = pl_rng_float() - 0.5f;
        }
        out[i] += noisiness * (h->noise_sample - out[i]);
    }

    /* Apply VCA. */
    pl_interp_t sustain_gain;
    pl_interp_init(&sustain_gain, &h->sustain_gain, accent * decay, size);
    for (size_t i = 0; i < size; ++i) {
        h->envelope *= h->envelope > 0.5f || !two_stage_envelope ? envelope_decay : cut_decay;
        float gain = sustain ? pl_interp_next(&sustain_gain) : h->envelope;
        out[i] = vca == PL_HH_SWING_VCA ? pl_hh_swing_vca(out[i], gain) : pl_hh_linear_vca(out[i], gain);
    }

    pl_svf_set_f_q(&h->hpf, PL_FREQ_ACCURATE, cutoff, 0.5f);
    pl_svf_process_buf(&h->hpf, PL_HP, out, out, size);
    pl_interp_done(&sustain_gain);              /* (the destructor, at the end of Render) */
}

/* -------------------------------------------------- hi_hat_engine.cc --- */
typedef struct {
    pl_hh_voice_t hi_hat_1;                     /* HiHat<SquareNoise, SwingVCA, true, false> */
    pl_hh_voice_t hi_hat_2;                     /* HiHat<RingModNoise, LinearVCA, false, true> */
    float temp_buffer[PL_MAX_BLOCK * 2];        /* (the allocator's kMaxBlockSize * 2) */
} pl_hh_t;

static void pl_hh_init(pl_hh_t *e)
{
    pl_hh_voice_init(&e->hi_hat_1, PL_HH_SQUARE_NOISE);
    pl_hh_voice_init(&e->hi_hat_2, PL_HH_RING_MOD_NOISE);
    for (size_t i = 0; i < PL_MAX_BLOCK * 2; ++i) e->temp_buffer[i] = 0.0f;
}
static void pl_hh_reset(pl_hh_t *e) { (void)e; }

static void pl_hh_render(pl_hh_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float f0 = pl_note_to_frequency(p->note);
    const int sustain = (p->trigger & PL_TRIGGER_UNPATCHED) != 0;
    const int trigger = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;

    pl_hh_voice_render(&e->hi_hat_1, PL_HH_SQUARE_NOISE, PL_HH_SWING_VCA, 1, 0, sustain, trigger, p->accent, f0,
                       p->timbre, p->morph, p->harmonics, e->temp_buffer, e->temp_buffer + size, out, size);
    pl_hh_voice_render(&e->hi_hat_2, PL_HH_RING_MOD_NOISE, PL_HH_LINEAR_VCA, 0, 1, sustain, trigger, p->accent, f0,
                       p->timbre, p->morph, p->harmonics, e->temp_buffer, e->temp_buffer + size, aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_hh_flush(pl_hh_t *e)
{
    pl_hh_voice_t *v[2] = { &e->hi_hat_1, &e->hi_hat_2 };
    for (int i = 0; i < 2; i++) {
        pl_flush(&v[i]->envelope);
        pl_flush_svf(&v[i]->noise_coloration_svf);
        pl_flush_svf(&v[i]->hpf);
    }
}
