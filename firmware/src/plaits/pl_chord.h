/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 6, CHORD (plaits/dsp/engine/chord_engine.cc), with its chord bank
 * (chords/chord_bank.cc, the 11 stock chords: JON_CHORDS is not defined), its oscillators
 * (oscillator/string_synth_oscillator.h, oscillator/wavetable_oscillator.h) and stmlib's HysteresisQuantizer2, in C.
 *   HARMONICS: the chord. TIMBRE: the inversion. MORPH: the divide-down registration, then the wavetable.
 *   OUT: all five voices. AUX: the root note alone.
 * The chord bank's allocator buffers (ratios, note counts, sorted ratios) are arrays in the struct. The wavetable
 * pointers point into pl_chord_waves (pl_tables.h), which holds just the 15 waves the engine reads.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

#define PL_CHORD_NUM_NOTES 4
#define PL_CHORD_NUM_VOICES (PL_CHORD_NUM_NOTES + 1)
#define PL_CHORD_NUM_CHORDS 11
#define PL_CHORD_NUM_HARMONICS 3
#define PL_CHORD_WAVETABLE_SIZE 128
#define PL_CHORD_NUM_WAVES 15

/* ------------------------------------- stmlib HysteresisQuantizer2 --- */
typedef struct { int num_steps; float hysteresis, scale, offset; int quantized_value; } pl_chord_hysq_t;
static inline void pl_chord_hysq_init(pl_chord_hysq_t *h, int num_steps, float hysteresis, int symmetric)
{
    h->num_steps = num_steps;
    h->hysteresis = hysteresis;
    h->scale = (float)(symmetric ? num_steps - 1 : num_steps);
    h->offset = symmetric ? 0.0f : -0.5f;
    h->quantized_value = 0;
}
static inline int pl_chord_hysq_process(pl_chord_hysq_t *h, int base, float value)
{
    value *= h->scale;
    value += h->offset;
    value += (float)base;
    float hysteresis_sign = value > (float)h->quantized_value ? -1.0f : +1.0f;
    int q = (int)(value + hysteresis_sign * h->hysteresis + 0.5f);
    PL_CONSTRAIN(q, 0, h->num_steps - 1);
    h->quantized_value = q;
    return q;
}

/* ------------------------------------------- string_synth_oscillator.h --- */
typedef struct {
    float phase, next_sample;
    int segment;
    float frequency, saw_8_gain, saw_4_gain, saw_2_gain, saw_1_gain;   /* (interpolated) */
} pl_chord_ss_osc_t;
static inline void pl_chord_ss_osc_init(pl_chord_ss_osc_t *o)
{
    o->phase = 0.0f;
    o->next_sample = 0.0f;
    o->segment = 0;
    o->frequency = 0.001f;
    o->saw_8_gain = 0.0f;
    o->saw_4_gain = 0.0f;
    o->saw_2_gain = 0.0f;
    o->saw_1_gain = 0.0f;
}
/* Render(frequency, unshifted_registration[7], gain, out, size): adds to out */
static void pl_chord_ss_osc_render(pl_chord_ss_osc_t *o, float frequency, const float *unshifted_registration,
                                   float gain, float *out, size_t size)
{
    frequency *= 8.0f;
    /* Very high notes: play the 2nd harmonic of a wave an octave down (shift by one register pair). */
    size_t shift = 0, i;
    while (frequency > 0.5f) {
        shift += 2;
        frequency *= 0.5f;
    }
    if (shift >= 8) return;     /* too high */

    float registration[7];
    for (i = 0; i < shift; ++i) registration[i] = 0.0f;
    for (i = 0; i < 7 - shift; ++i) registration[shift + i] = unshifted_registration[i];

    pl_interp_t fm, saw_8_gain_modulation, saw_4_gain_modulation, saw_2_gain_modulation, saw_1_gain_modulation;
    pl_interp_init(&fm, &o->frequency, frequency, size);
    pl_interp_init(&saw_8_gain_modulation, &o->saw_8_gain, (registration[0] + 2.0f * registration[1]) * gain, size);
    pl_interp_init(&saw_4_gain_modulation, &o->saw_4_gain,
                   (registration[2] - registration[1] + 2.0f * registration[3]) * gain, size);
    pl_interp_init(&saw_2_gain_modulation, &o->saw_2_gain,
                   (registration[4] - registration[3] + 2.0f * registration[5]) * gain, size);
    pl_interp_init(&saw_1_gain_modulation, &o->saw_1_gain, (registration[6] - registration[5]) * gain, size);

    float phase = o->phase;
    float next_sample = o->next_sample;
    int segment = o->segment;
    while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;
        const float f = pl_interp_next(&fm);
        const float saw_8_gain = pl_interp_next(&saw_8_gain_modulation);
        const float saw_4_gain = pl_interp_next(&saw_4_gain_modulation);
        const float saw_2_gain = pl_interp_next(&saw_2_gain_modulation);
        const float saw_1_gain = pl_interp_next(&saw_1_gain_modulation);

        phase += f;
        int next_segment = (int)phase;
        if (next_segment != segment) {
            float discontinuity = 0.0f;
            if (next_segment == 8) {
                phase -= 8.0f;
                next_segment -= 8;
                discontinuity -= saw_8_gain;
            }
            if ((next_segment & 3) == 0) discontinuity -= saw_4_gain;
            if ((next_segment & 1) == 0) discontinuity -= saw_2_gain;
            discontinuity -= saw_1_gain;
            if (discontinuity != 0.0f) {
                float fraction = phase - (float)next_segment;
                float t = fraction / f;
                this_sample += pl_this_blep(t) * discontinuity;
                next_sample += pl_next_blep(t) * discontinuity;
            }
        }
        segment = next_segment;

        next_sample += (phase - 4.0f) * saw_8_gain * 0.125f;
        next_sample += (phase - (float)(segment & 4) - 2.0f) * saw_4_gain * 0.25f;
        next_sample += (phase - (float)(segment & 6) - 1.0f) * saw_2_gain * 0.5f;
        next_sample += (phase - (float)(segment & 7) - 0.5f) * saw_1_gain;
        *out++ += 2.0f * this_sample;
    }
    o->next_sample = next_sample;
    o->phase = phase;
    o->segment = segment;
    pl_interp_done(&fm);
    pl_interp_done(&saw_8_gain_modulation);
    pl_interp_done(&saw_4_gain_modulation);
    pl_interp_done(&saw_2_gain_modulation);
    pl_interp_done(&saw_1_gain_modulation);
}

/* ---------------------------------------------- wavetable_oscillator.h --- */
/* WavetableOscillator<128, 15> (approximate_scale and attenuate_high_frequencies on), with its Differentiator */
typedef struct {
    float phase;
    float frequency, amplitude, waveform, lp;   /* (interpolated, and the output low-pass) */
    float diff_lp, diff_previous;               /* Differentiator */
} pl_chord_wt_osc_t;
static inline void pl_chord_wt_osc_init(pl_chord_wt_osc_t *o)
{
    o->phase = 0.0f;
    o->frequency = 0.0f;
    o->amplitude = 0.0f;
    o->waveform = 0.0f;
    o->lp = 0.0f;
    o->diff_previous = 0.0f;
    o->diff_lp = 0.0f;
}
/* InterpolateWave<int16_t> */
static inline float pl_chord_interpolate_wave(const int16_t *table, int32_t index_integral, float index_fractional)
{
    float a = (float)table[index_integral];
    float b = (float)table[index_integral + 1];
    return a + (b - a) * index_fractional;
}
/* Render(frequency, amplitude, waveform, wavetable, out, size): adds to out. wavetable[i] is &pl_chord_waves[i * 132]. */
static void pl_chord_wt_osc_render(pl_chord_wt_osc_t *o, float frequency, float amplitude, float waveform, float *out,
                                   size_t size)
{
    PL_CONSTRAIN(frequency, 0.0000001f, PL_MAX_FREQUENCY);
    amplitude *= 1.0f - 2.0f * frequency;               /* attenuate_high_frequencies */
    amplitude *= 1.0f / (frequency * 131072.0f);        /* approximate_scale */

    pl_interp_t frequency_modulation, amplitude_modulation, waveform_modulation;
    pl_interp_init(&frequency_modulation, &o->frequency, frequency, size);
    pl_interp_init(&amplitude_modulation, &o->amplitude, amplitude, size);
    pl_interp_init(&waveform_modulation, &o->waveform, waveform * (15.0f - 1.0001f), size);   /* float(num_waves - 1.0001f) */

    float lp = o->lp;
    float phase = o->phase;
    while (size--) {
        const float f0 = pl_interp_next(&frequency_modulation);
        const float cutoff = pl_minf((float)PL_CHORD_WAVETABLE_SIZE * f0, 1.0f);
        const float scale = 1.0f;                       /* (approximate_scale) */

        phase += f0;
        if (phase >= 1.0f) phase -= 1.0f;

        const float wf = pl_interp_next(&waveform_modulation);
        PL_MAKE_INTEGRAL_FRACTIONAL(wf)
        const float p = phase * (float)PL_CHORD_WAVETABLE_SIZE;
        PL_MAKE_INTEGRAL_FRACTIONAL(p)

        const float x0 = pl_chord_interpolate_wave(&pl_chord_waves[wf_integral * 132], p_integral, p_fractional);
        const float x1 = pl_chord_interpolate_wave(&pl_chord_waves[(wf_integral + 1) * 132], p_integral, p_fractional);

        /* Differentiator::Process(cutoff, s) */
        const float s_in = (x0 + (x1 - x0) * wf_fractional) * scale;
        PL_ONE_POLE(o->diff_lp, s_in - o->diff_previous, cutoff);
        o->diff_previous = s_in;
        const float s = o->diff_lp;

        PL_ONE_POLE(lp, s, cutoff);
        *out++ += pl_interp_next(&amplitude_modulation) * lp;
    }
    o->lp = lp;
    o->phase = phase;
    pl_interp_done(&frequency_modulation);
    pl_interp_done(&amplitude_modulation);
    pl_interp_done(&waveform_modulation);
}

/* ------------------------------------------------------- chord_bank.cc --- */
static const float pl_chord_chords[PL_CHORD_NUM_CHORDS][PL_CHORD_NUM_NOTES] = {
    { 0.00f, 0.01f, 11.99f, 12.00f },  /* OCT */
    { 0.00f, 7.00f,  7.01f, 12.00f },  /* 5 */
    { 0.00f, 5.00f,  7.00f, 12.00f },  /* sus4 */
    { 0.00f, 3.00f,  7.00f, 12.00f },  /* m */
    { 0.00f, 3.00f,  7.00f, 10.00f },  /* m7 */
    { 0.00f, 3.00f, 10.00f, 14.00f },  /* m9 */
    { 0.00f, 3.00f, 10.00f, 17.00f },  /* m11 */
    { 0.00f, 2.00f,  9.00f, 16.00f },  /* 69 */
    { 0.00f, 4.00f, 11.00f, 14.00f },  /* M9 */
    { 0.00f, 4.00f,  7.00f, 11.00f },  /* M7 */
    { 0.00f, 4.00f,  7.00f, 12.00f },  /* M */
};
typedef struct {
    pl_chord_hysq_t chord_index_quantizer;
    float ratios[PL_CHORD_NUM_CHORDS * PL_CHORD_NUM_NOTES];    /* (allocator buffers in the C++) */
    float sorted_ratios[PL_CHORD_NUM_NOTES];
    int note_count[PL_CHORD_NUM_CHORDS];
} pl_chord_bank_t;
static inline void pl_chord_bank_init(pl_chord_bank_t *b)
{
    pl_chord_hysq_init(&b->chord_index_quantizer, PL_CHORD_NUM_CHORDS, 0.075f, 0);
}
static inline int pl_chord_bank_chord_index(const pl_chord_bank_t *b) { return b->chord_index_quantizer.quantized_value; }
static inline void pl_chord_bank_set_chord(pl_chord_bank_t *b, float parameter)
{
    pl_chord_hysq_process(&b->chord_index_quantizer, 0, parameter * 1.02f);
}
/* Sort(): std::sort of 4 floats becomes an insertion sort (same result) */
static inline void pl_chord_bank_sort(pl_chord_bank_t *b)
{
    int i, j;
    for (i = 0; i < PL_CHORD_NUM_NOTES; ++i) {
        float r = b->ratios[pl_chord_bank_chord_index(b) * PL_CHORD_NUM_NOTES + i];
        while (r > 2.0f) r *= 0.5f;
        b->sorted_ratios[i] = r;
    }
    for (i = 1; i < PL_CHORD_NUM_NOTES; ++i) {
        float v = b->sorted_ratios[i];
        for (j = i; j > 0 && v < b->sorted_ratios[j - 1]; --j) b->sorted_ratios[j] = b->sorted_ratios[j - 1];
        b->sorted_ratios[j] = v;
    }
}
static void pl_chord_bank_reset(pl_chord_bank_t *b)
{
    int i, j;
    for (i = 0; i < PL_CHORD_NUM_CHORDS; ++i) {
        int count = 0;
        for (j = 0; j < PL_CHORD_NUM_NOTES; ++j) {
            const float c = pl_chord_chords[i][j];
            b->ratios[i * PL_CHORD_NUM_NOTES + j] = pl_semitones_to_ratio(c);
            if (c != 0.01f && c != 7.01f && c != 11.99f && c != 12.00f) ++count;
        }
        b->note_count[i] = count;
    }
    pl_chord_bank_sort(b);
}
static int pl_chord_bank_compute_inversion(const pl_chord_bank_t *b, float inversion, float *ratios, float *amplitudes)
{
    const float *base_ratio = &b->ratios[pl_chord_bank_chord_index(b) * PL_CHORD_NUM_NOTES];
    inversion = inversion * (float)(PL_CHORD_NUM_NOTES * PL_CHORD_NUM_VOICES);
    PL_MAKE_INTEGRAL_FRACTIONAL(inversion)

    int num_rotations = inversion_integral / PL_CHORD_NUM_NOTES;
    int rotated_note = inversion_integral % PL_CHORD_NUM_NOTES;
    const float base_gain = 0.25f;
    int mask = 0, i;
    for (i = 0; i < PL_CHORD_NUM_NOTES; ++i) {
        float transposition = 0.25f * (float)(1 << ((PL_CHORD_NUM_NOTES - 1 + inversion_integral - i) / PL_CHORD_NUM_NOTES));
        int target_voice = (i - num_rotations + PL_CHORD_NUM_VOICES) % PL_CHORD_NUM_VOICES;
        int previous_voice = (target_voice - 1 + PL_CHORD_NUM_VOICES) % PL_CHORD_NUM_VOICES;
        if (i == rotated_note) {
            ratios[target_voice] = base_ratio[i] * transposition;
            ratios[previous_voice] = ratios[target_voice] * 2.0f;
            amplitudes[previous_voice] = base_gain * inversion_fractional;
            amplitudes[target_voice] = base_gain * (1.0f - inversion_fractional);
        } else if (i < rotated_note) {
            ratios[previous_voice] = base_ratio[i] * transposition;
            amplitudes[previous_voice] = base_gain;
        } else {
            ratios[target_voice] = base_ratio[i] * transposition;
            amplitudes[target_voice] = base_gain;
        }
        if (i == 0) {
            if (i >= rotated_note) mask |= 1 << target_voice;
            if (i <= rotated_note) mask |= 1 << previous_voice;
        }
    }
    return mask;
}

/* ----------------------------------------------------- chord_engine.cc --- */
typedef struct {
    pl_chord_ss_osc_t divide_down_voice[PL_CHORD_NUM_VOICES];
    pl_chord_wt_osc_t wavetable_voice[PL_CHORD_NUM_VOICES];
    pl_chord_bank_t chords;
    float morph_lp, timbre_lp;
} pl_chord_t;

static void pl_chord_init(pl_chord_t *e)
{
    int i;
    for (i = 0; i < PL_CHORD_NUM_VOICES; ++i) {
        pl_chord_ss_osc_init(&e->divide_down_voice[i]);
        pl_chord_wt_osc_init(&e->wavetable_voice[i]);
    }
    pl_chord_bank_init(&e->chords);
    e->morph_lp = 0.0f;
    e->timbre_lp = 0.0f;
}
static void pl_chord_reset(pl_chord_t *e) { pl_chord_bank_reset(&e->chords); }

static const float pl_chord_fade_point[PL_CHORD_NUM_VOICES] = { 0.55f, 0.47f, 0.49f, 0.51f, 0.53f };
#define PL_CHORD_REGISTRATION_TABLE_SIZE 8
static const float pl_chord_registrations[PL_CHORD_REGISTRATION_TABLE_SIZE][PL_CHORD_NUM_HARMONICS * 2] = {
    { 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f },     /* Square */
    { 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },     /* Saw */
    { 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f },     /* Saw + saw */
    { 0.33f, 0.0f, 0.33f, 0.0f, 0.33f, 0.0f },  /* Full saw */
    { 0.33f, 0.0f, 0.0f, 0.33f, 0.0f, 0.33f },  /* Full saw + square hybrid */
    { 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f },     /* Saw + high square harmo */
    { 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.5f },     /* Square + high square harmo */
    { 0.0f, 0.1f, 0.1f, 0.0f, 0.2f, 0.6f },     /* Saw+square + high harmo */
};
static inline void pl_chord_compute_registration(float registration, float *amplitudes)
{
    int i;
    registration *= ((float)PL_CHORD_REGISTRATION_TABLE_SIZE - 1.001f);
    PL_MAKE_INTEGRAL_FRACTIONAL(registration)
    for (i = 0; i < PL_CHORD_NUM_HARMONICS * 2; ++i) {
        float a = pl_chord_registrations[registration_integral][i];
        float b = pl_chord_registrations[registration_integral + 1][i];
        amplitudes[i] = a + (b - a) * registration_fractional;
    }
}

static __attribute__((section(".text.xip_macro"))) void pl_chord_render(pl_chord_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    size_t i;
    int note;
    PL_ONE_POLE(e->morph_lp, p->morph, 0.1f);
    PL_ONE_POLE(e->timbre_lp, p->timbre, 0.1f);

    pl_chord_bank_set_chord(&e->chords, p->harmonics);

    float harmonics[PL_CHORD_NUM_HARMONICS * 2 + 2];
    float note_amplitudes[PL_CHORD_NUM_VOICES];
    float registration = pl_maxf(1.0f - e->morph_lp * 2.15f, 0.0f);

    pl_chord_compute_registration(registration, harmonics);
    harmonics[PL_CHORD_NUM_HARMONICS * 2] = 0.0f;
    harmonics[PL_CHORD_NUM_HARMONICS * 2 + 1] = 0.0f;   /* (never read; left unset in the C++) */

    float ratios[PL_CHORD_NUM_VOICES];
    int aux_note_mask = pl_chord_bank_compute_inversion(&e->chords, e->timbre_lp, ratios, note_amplitudes);

    for (i = 0; i < size; ++i) { out[i] = 0.0f; aux[i] = 0.0f; }

    const float f0 = pl_note_to_frequency(p->note) * 0.998f;
    const float waveform = pl_maxf((e->morph_lp - 0.535f) * 2.15f, 0.0f);

    for (note = 0; note < PL_CHORD_NUM_VOICES; ++note) {
        float wavetable_amount = 50.0f * (e->morph_lp - pl_chord_fade_point[note]);
        PL_CONSTRAIN(wavetable_amount, 0.0f, 1.0f);

        float divide_down_amount = 1.0f - wavetable_amount;
        float *destination = (1 << note) & aux_note_mask ? aux : out;

        const float note_f0 = f0 * ratios[note];
        float divide_down_gain = 4.0f - note_f0 * 32.0f;
        PL_CONSTRAIN(divide_down_gain, 0.0f, 1.0f);
        divide_down_amount *= divide_down_gain;

        if (wavetable_amount != 0.0f) {
            pl_chord_wt_osc_render(&e->wavetable_voice[note], note_f0 * 1.004f,
                                   note_amplitudes[note] * wavetable_amount, waveform, destination, size);
        }
        if (divide_down_amount != 0.0f) {
            pl_chord_ss_osc_render(&e->divide_down_voice[note], note_f0, harmonics,
                                   note_amplitudes[note] * divide_down_amount, destination, size);
        }
    }

    for (i = 0; i < size; ++i) {
        out[i] += aux[i];
        aux[i] *= 3.0f;
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_chord_flush(pl_chord_t *e)
{
    for (int i = 0; i < PL_CHORD_NUM_VOICES; i++) {
        pl_chord_ss_osc_t *d = &e->divide_down_voice[i];
        pl_chord_wt_osc_t *w = &e->wavetable_voice[i];
        pl_flush(&d->saw_8_gain); pl_flush(&d->saw_4_gain); pl_flush(&d->saw_2_gain); pl_flush(&d->saw_1_gain);
        pl_flush(&w->amplitude); pl_flush(&w->lp); pl_flush(&w->diff_lp); pl_flush(&w->diff_previous);
    }
    pl_flush(&e->morph_lp);
    pl_flush(&e->timbre_lp);
}
