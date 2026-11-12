/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: Plaits 1.2's STRING MACHINE (plaits/dsp/engine2/string_machine_engine.{h,cc}) with
 * its ensemble chorus (plaits/dsp/fx/ensemble.h on plaits/dsp/fx/fx_engine.h, FxEngine<1024, FORMAT_32_BIT>), in C.
 * The chord bank and the divide-down string synth oscillator are CHORD's, reused from pl_chord.h (pl_chord_bank_t,
 * pl_chord_ss_osc_t); the two VCFs are stmlib's NaiveSvf (pl_naive_svf_t, pl_dsp2.h).
 *   HARMONICS: the chord. TIMBRE: the VCF cutoff, and with it the chorus (deepest and strongest away from 12
 *       o'clock). MORPH: the registration (saw to square, through the 11 of pl_strmach_registrations).
 *   OUT and AUX: the two chorus channels (the chord's four notes alternate between the two VCFs, then are mixed
 *       66/33 and chorused crosswise).
 * Every float operation in the order of the C++.
 *
 * Deliberate differences:
 *   The ensemble's delay memory (1024 floats, 4096 bytes: two lines of 511 at 0 and 512, from the allocator in
 *   the C++) is an array in the struct. init clears it and the write pointer (the C++ leaves them to Reset) and
 *   sets amount and depth to 0 (the C++ leaves them unset until the first render sets them).
 *   FxEngine's two CosineOscillator LFOs are left out: the ensemble never sets their frequency and never reads
 *   their values (Start() steps them every 32 samples, to no effect).
 *   No user data (the C++ LoadUserData is empty for this engine anyway).
 */
#pragma once
#include "pl_chord.h"
#include "pl_dsp2.h"

/* --------------------------------------------------------- fx/ensemble.h --- */
#define PL_STRMACH_ENSEMBLE_SIZE 1024           /* FxEngine<1024, FORMAT_32_BIT> */
#define PL_STRMACH_ENSEMBLE_MASK (PL_STRMACH_ENSEMBLE_SIZE - 1)
#define PL_STRMACH_LINE_L 0                     /* E::Reserve<511, E::Reserve<511> >: line_l's base */
#define PL_STRMACH_LINE_R 512                   /* line_r's base (511 + 1 after line_l) */

typedef struct {
    float buffer[PL_STRMACH_ENSEMBLE_SIZE];     /* (the allocator's buffer in the C++) */
    int32_t write_ptr;
    float amount, depth;
    uint32_t phase_1, phase_2;
} pl_strmach_ensemble_t;

static void pl_strmach_ensemble_reset(pl_strmach_ensemble_t *e)
{
    int i;
    for (i = 0; i < PL_STRMACH_ENSEMBLE_SIZE; ++i) e->buffer[i] = 0.0f;   /* engine_.Clear() */
    e->write_ptr = 0;
}
static void pl_strmach_ensemble_init(pl_strmach_ensemble_t *e)
{
    pl_strmach_ensemble_reset(e);               /* (not done by the C++ Init) */
    e->amount = 0.0f;                           /* (unset in the C++ until set_amount) */
    e->depth = 0.0f;
    e->phase_1 = 0;
    e->phase_2 = 0;
}
/* Context::Interpolate(line, offset, scale) less its accumulate: returns the read x (the caller adds x * scale) */
static inline float pl_strmach_ensemble_interpolate(const pl_strmach_ensemble_t *e, int32_t write_ptr, int32_t base,
                                                    float offset)
{
    PL_MAKE_INTEGRAL_FRACTIONAL(offset)
    float a = e->buffer[(write_ptr + offset_integral + base) & PL_STRMACH_ENSEMBLE_MASK];
    float b = e->buffer[(write_ptr + offset_integral + base + 1) & PL_STRMACH_ENSEMBLE_MASK];
    return a + (b - a) * offset_fractional;
}
static void pl_strmach_ensemble_process(pl_strmach_ensemble_t *e, float *left, float *right, size_t size)
{
    while (size--) {
        /* engine_.Start(&c) */
        --e->write_ptr;
        if (e->write_ptr < 0) e->write_ptr += PL_STRMACH_ENSEMBLE_SIZE;
        const int32_t write_ptr = e->write_ptr;
        float accumulator = 0.0f;
        float dry_amount = 1.0f - e->amount * 0.5f;

        /* Update LFO. */
        const uint32_t one_third = 1417339207u;
        const uint32_t two_third = 2834678415u;

        e->phase_1 += 67289u;                   /* 0.75 Hz */
        e->phase_2 += 589980u;                  /* 6.57 Hz */
        float slow_0 = pl_sine_raw(e->phase_1);
        float slow_120 = pl_sine_raw(e->phase_1 + one_third);
        float slow_240 = pl_sine_raw(e->phase_1 + two_third);
        float fast_0 = pl_sine_raw(e->phase_2);
        float fast_120 = pl_sine_raw(e->phase_2 + one_third);
        float fast_240 = pl_sine_raw(e->phase_2 + two_third);

        /* Max deviation: 176 */
        float a = e->depth * 160.0f;
        float b = e->depth * 16.0f;

        float mod_1 = slow_0 * a + fast_0 * b;
        float mod_2 = slow_120 * a + fast_120 * b;
        float mod_3 = slow_240 * a + fast_240 * b;

        float wet = 0.0f;

        /* Sum L & R channel to send to chorus line. */
        accumulator += *left * 1.0f;                                                    /* c.Read(*left, 1.0f) */
        e->buffer[(write_ptr + PL_STRMACH_LINE_L) & PL_STRMACH_ENSEMBLE_MASK] = accumulator;   /* c.Write(line_l, 0.0f) */
        accumulator *= 0.0f;
        accumulator += *right * 1.0f;
        e->buffer[(write_ptr + PL_STRMACH_LINE_R) & PL_STRMACH_ENSEMBLE_MASK] = accumulator;
        accumulator *= 0.0f;

        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_L, mod_1 + 192.0f) * 0.33f;
        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_L, mod_2 + 192.0f) * 0.33f;
        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_R, mod_3 + 192.0f) * 0.33f;
        wet = accumulator;                                                              /* c.Write(wet, 0.0f) */
        accumulator *= 0.0f;
        *left = wet * e->amount + *left * dry_amount;

        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_R, mod_1 + 192.0f) * 0.33f;
        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_R, mod_2 + 192.0f) * 0.33f;
        accumulator += pl_strmach_ensemble_interpolate(e, write_ptr, PL_STRMACH_LINE_L, mod_3 + 192.0f) * 0.33f;
        wet = accumulator;
        accumulator *= 0.0f;
        *right = wet * e->amount + *right * dry_amount;
        left++;
        right++;
    }
}

/* ------------------------------------------------ string_machine_engine.cc --- */
#define PL_STRMACH_REGISTRATION_TABLE_SIZE 11
static const float pl_strmach_registrations[PL_STRMACH_REGISTRATION_TABLE_SIZE][PL_CHORD_NUM_HARMONICS * 2] = {
    { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },     /* Saw */
    { 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f },     /* Saw + saw */
    { 0.4f, 0.0f, 0.2f, 0.0f, 0.4f, 0.0f },     /* Full saw */
    { 0.3f, 0.0f, 0.0f, 0.3f, 0.0f, 0.4f },     /* Full saw + square hybrid */
    { 0.3f, 0.0f, 0.0f, 0.0f, 0.0f, 0.7f },     /* Saw + high square harmo */
    { 0.2f, 0.0f, 0.0f, 0.2f, 0.0f, 0.6f },     /* Weird hybrid */
    { 0.0f, 0.2f, 0.1f, 0.0f, 0.2f, 0.5f },     /* Sawsquare high harmo */
    { 0.0f, 0.3f, 0.0f, 0.3f, 0.0f, 0.4f },     /* Square high armo */
    { 0.0f, 0.4f, 0.0f, 0.3f, 0.0f, 0.3f },     /* Full square */
    { 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f },     /* Square + Square */
    { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },     /* Square */
};
/* ComputeRegistration (the engine's own: CHORD's has 8 registrations, this one 11) */
static inline void pl_strmach_compute_registration(float registration, float *amplitudes)
{
    int i;
    registration *= (PL_STRMACH_REGISTRATION_TABLE_SIZE - 1.001f);
    PL_MAKE_INTEGRAL_FRACTIONAL(registration)
    for (i = 0; i < PL_CHORD_NUM_HARMONICS * 2; ++i) {
        float a = pl_strmach_registrations[registration_integral][i];
        float b = pl_strmach_registrations[registration_integral + 1][i];
        amplitudes[i] = a + (b - a) * registration_fractional;
    }
}
/* ChordBank::ratio(note) (pl_chord.h has no accessor for it) */
static inline float pl_strmach_chord_ratio(const pl_chord_bank_t *b, int note)
{
    return b->ratios[pl_chord_bank_chord_index(b) * PL_CHORD_NUM_NOTES + note];
}

typedef struct {
    pl_chord_bank_t chords;
    pl_strmach_ensemble_t ensemble;
    pl_chord_ss_osc_t divide_down_voice[PL_CHORD_NUM_NOTES];
    pl_naive_svf_t svf[2];
    float morph_lp, timbre_lp;
} pl_strmach_t;

static void pl_strmach_init(pl_strmach_t *e)
{
    int i;
    for (i = 0; i < PL_CHORD_NUM_NOTES; ++i) pl_chord_ss_osc_init(&e->divide_down_voice[i]);
    pl_chord_bank_init(&e->chords);
    e->morph_lp = 0.0f;
    e->timbre_lp = 0.0f;
    pl_naive_svf_init(&e->svf[0]);
    pl_naive_svf_init(&e->svf[1]);
    pl_strmach_ensemble_init(&e->ensemble);
}
static void pl_strmach_reset(pl_strmach_t *e)
{
    pl_chord_bank_reset(&e->chords);
    pl_strmach_ensemble_reset(&e->ensemble);
}

static void pl_strmach_render(pl_strmach_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                              int *already_enveloped)
{
    (void)already_enveloped;
    size_t i;
    int note;
    PL_ONE_POLE(e->morph_lp, p->morph, 0.1f);
    PL_ONE_POLE(e->timbre_lp, p->timbre, 0.1f);

    pl_chord_bank_set_chord(&e->chords, p->harmonics);

    float harmonics[PL_CHORD_NUM_HARMONICS * 2 + 2];
    float registration = e->morph_lp < 0.0f ? 0.0f : e->morph_lp;      /* std::max(morph_lp_, 0.0f) */
    pl_strmach_compute_registration(registration, harmonics);
    harmonics[PL_CHORD_NUM_HARMONICS * 2] = 0.0f;
    harmonics[PL_CHORD_NUM_HARMONICS * 2 + 1] = 0.0f;   /* (never read; left unset in the C++) */

    /* Render string/organ sound. */
    for (i = 0; i < size; ++i) { out[i] = 0.0f; aux[i] = 0.0f; }
    const float f0 = pl_note_to_frequency(p->note) * 0.998f;
    for (note = 0; note < PL_CHORD_NUM_NOTES; ++note) {
        const float note_f0 = f0 * pl_strmach_chord_ratio(&e->chords, note);
        float divide_down_gain = 4.0f - note_f0 * 32.0f;
        PL_CONSTRAIN(divide_down_gain, 0.0f, 1.0f);
        pl_chord_ss_osc_render(&e->divide_down_voice[note], note_f0, harmonics, 0.25f * divide_down_gain,
                               note & 1 ? aux : out, size);
    }

    /* Pass through VCF. */
    const float cutoff = 2.2f * f0 * pl_semitones_to_ratio(120.0f * p->timbre);
    pl_naive_svf_set_f_q(&e->svf[0], PL_FREQ_DIRTY, cutoff, 1.0f);
    pl_naive_svf_set_f_q(&e->svf[1], PL_FREQ_DIRTY, cutoff * 1.5f, 1.0f);

    /* Mixdown. */
    for (i = 0; i < size; ++i) {
        const float l = pl_naive_svf_process(&e->svf[0], PL_LP, out[i]);
        const float r = pl_naive_svf_process(&e->svf[1], PL_LP, aux[i]);
        out[i] = 0.66f * l + 0.33f * r;
        aux[i] = 0.66f * r + 0.33f * l;
    }

    /* Ensemble FX. */
    const float amount = pl_fabsf(p->timbre - 0.5f) * 2.0f;
    const float depth = 0.35f + 0.65f * p->timbre;
    e->ensemble.amount = amount;                /* set_amount */
    e->ensemble.depth = depth;                  /* set_depth */
    pl_strmach_ensemble_process(&e->ensemble, out, aux, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render.
 * (the ensemble's delay line is not flushed: it holds the VCFs' output for 512 samples at most) */
static inline void pl_strmach_flush(pl_strmach_t *e)
{
    int i;
    for (i = 0; i < PL_CHORD_NUM_NOTES; ++i) {
        pl_chord_ss_osc_t *d = &e->divide_down_voice[i];
        pl_flush(&d->saw_8_gain); pl_flush(&d->saw_4_gain); pl_flush(&d->saw_2_gain); pl_flush(&d->saw_1_gain);
    }
    pl_flush_naive_svf(&e->svf[0]);
    pl_flush_naive_svf(&e->svf[1]);
    pl_flush(&e->morph_lp);
    pl_flush(&e->timbre_lp);
}
