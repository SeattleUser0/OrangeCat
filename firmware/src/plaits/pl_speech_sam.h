/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the SAM-inspired half of Plaits' SPEECH model (pl_speech.h), in C.
 *   plaits/dsp/speech/sam_speech_synth.{h,cc}   pl_speech_sam_t: a glottal saw resetting three sine formants
 *                                               (pl_sine_raw, pl_lut_sine)
 * Every float operation in the order of the C++. No deliberate differences.
 */
#pragma once
#include "pl_osc.h"

#define PL_SPEECH_SAM_NUM_FORMANTS 3
#define PL_SPEECH_SAM_NUM_VOWELS 9
#define PL_SPEECH_SAM_NUM_CONSONANTS 8
#define PL_SPEECH_SAM_NUM_PHONEMES (PL_SPEECH_SAM_NUM_VOWELS + PL_SPEECH_SAM_NUM_CONSONANTS)

typedef struct { uint8_t frequency, amplitude; } pl_speech_sam_formant_t;
typedef struct { pl_speech_sam_formant_t formant[PL_SPEECH_SAM_NUM_FORMANTS]; } pl_speech_sam_phoneme_t;

static const pl_speech_sam_phoneme_t pl_speech_sam_phonemes[PL_SPEECH_SAM_NUM_PHONEMES + 1] = {
    { { { 60, 15 }, { 90, 13 }, { 200, 1 } } },
    { { { 40, 13 }, { 114, 12 }, { 139, 6 } } },
    { { { 33, 14 }, { 155, 12 }, { 209, 7 } } },
    { { { 22, 13 }, { 189, 10 }, { 247, 8 } } },
    { { { 51, 15 }, { 99, 12 }, { 195, 1 } } },
    { { { 29, 13 }, { 65, 8 }, { 180, 0 } } },
    { { { 13, 12 }, { 103, 3 }, { 182, 0 } } },
    { { { 20, 15 }, { 114, 3 }, { 213, 0 } } },
    { { { 13, 7 }, { 164, 3 }, { 222, 14 } } },
    { { { 13, 9 }, { 121, 9 }, { 254, 0 } } },
    { { { 40, 12 }, { 112, 10 }, { 114, 5 } } },
    { { { 24, 13 }, { 54, 8 }, { 157, 0 } } },
    { { { 33, 14 }, { 155, 12 }, { 166, 7 } } },
    { { { 36, 14 }, { 83, 8 }, { 249, 1 } } },
    { { { 40, 14 }, { 114, 12 }, { 139, 6 } } },
    { { { 13, 5 }, { 58, 5 }, { 182, 5 } } },
    { { { 13, 7 }, { 164, 10 }, { 222, 14 } } },
    { { { 13, 7 }, { 164, 10 }, { 222, 14 } } }   /* GUARD */
};

static const float pl_speech_sam_formant_amplitude_lut[16] = {
    0.03125000f, 0.03756299f, 0.04515131f, 0.05427259f, 0.06523652f,
    0.07841532f, 0.09425646f, 0.11329776f, 0.13618570f, 0.16369736f,
    0.19676682f, 0.23651683f, 0.28429697f, 0.34172946f, 0.41076422f,
    0.49374509f
};

typedef struct {
    float phase, frequency;
    float pulse_next_sample, pulse_lp;
    uint32_t formant_phase[3];
    size_t consonant_samples;
    float consonant_index;
} pl_speech_sam_t;

static void pl_speech_sam_init(pl_speech_sam_t *s)
{
    s->phase = 0.0f;
    s->frequency = 0.0f;
    s->pulse_next_sample = 0.0f;
    s->pulse_lp = 0.0f;
    s->formant_phase[0] = s->formant_phase[1] = s->formant_phase[2] = 0;
    s->consonant_samples = 0;
    s->consonant_index = 0.0f;
}

static inline void pl_speech_sam_interpolate_phoneme_data(float phoneme, float formant_shift,
                                                          uint32_t *formant_frequency, float *formant_amplitude)
{
    int i;
    PL_MAKE_INTEGRAL_FRACTIONAL(phoneme)

    const pl_speech_sam_phoneme_t *p_1 = &pl_speech_sam_phonemes[phoneme_integral];
    const pl_speech_sam_phoneme_t *p_2 = &pl_speech_sam_phonemes[phoneme_integral + 1];

    formant_shift = 1.0f + formant_shift * 2.5f;
    for (i = 0; i < PL_SPEECH_SAM_NUM_FORMANTS; ++i) {
        float f_1 = (float)p_1->formant[i].frequency;
        float f_2 = (float)p_2->formant[i].frequency;
        float f = f_1 + (f_2 - f_1) * phoneme_fractional;
        f *= 8.0f * formant_shift * 4294967296.0f / PL_SAMPLE_RATE;
        formant_frequency[i] = (uint32_t)f;

        float a_1 = pl_speech_sam_formant_amplitude_lut[p_1->formant[i].amplitude];
        float a_2 = pl_speech_sam_formant_amplitude_lut[p_2->formant[i].amplitude];
        formant_amplitude[i] = a_1 + (a_2 - a_1) * phoneme_fractional;
    }
}

/* Render(consonant, frequency, vowel, formant_shift, excitation, output, size) */
static void pl_speech_sam_render(pl_speech_sam_t *s, int consonant, float frequency, float vowel,
                                 float formant_shift, float *excitation, float *output, size_t size)
{
    if (frequency >= 0.0625f) frequency = 0.0625f;

    if (consonant) {
        s->consonant_samples = (size_t)(PL_SAMPLE_RATE * 0.05f);
        int r = (int)((vowel + 3.0f * frequency + 7.0f * formant_shift) * 8.0f);
        s->consonant_index = (float)(r % PL_SPEECH_SAM_NUM_CONSONANTS);
    }
    s->consonant_samples -= s->consonant_samples < size ? s->consonant_samples : size;

    float phoneme = s->consonant_samples
        ? (s->consonant_index + (float)PL_SPEECH_SAM_NUM_VOWELS)
        : vowel * (PL_SPEECH_SAM_NUM_VOWELS - 1.0001f);

    uint32_t formant_frequency[PL_SPEECH_SAM_NUM_FORMANTS];
    float formant_amplitude[PL_SPEECH_SAM_NUM_FORMANTS];

    pl_speech_sam_interpolate_phoneme_data(phoneme, formant_shift, formant_frequency, formant_amplitude);

    pl_interp_t fm;
    pl_interp_init(&fm, &s->frequency, frequency, size);
    float pulse_next_sample = s->pulse_next_sample;

    while (size--) {
        float pulse_this_sample = pulse_next_sample;
        pulse_next_sample = 0.0f;
        const float f = pl_interp_next(&fm);
        s->phase += f;

        if (s->phase >= 1.0f) {
            s->phase -= 1.0f;
            float t = s->phase / f;
            s->formant_phase[0] = (uint32_t)(t * (float)formant_frequency[0]);
            s->formant_phase[1] = (uint32_t)(t * (float)formant_frequency[1]);
            s->formant_phase[2] = (uint32_t)(t * (float)formant_frequency[2]);
            pulse_this_sample -= pl_this_blep(t);
            pulse_next_sample -= pl_next_blep(t);
        } else {
            s->formant_phase[0] += formant_frequency[0];
            s->formant_phase[1] += formant_frequency[1];
            s->formant_phase[2] += formant_frequency[2];
        }
        pulse_next_sample += s->phase;

        float d = pulse_this_sample - 0.5f - s->pulse_lp;
        s->pulse_lp += pl_minf(16.0f * f, 1.0f) * d;
        *excitation++ = d;

        float o = 0.0f;
        o += pl_sine_raw(s->formant_phase[0]) * formant_amplitude[0];
        o += pl_sine_raw(s->formant_phase[1]) * formant_amplitude[1];
        o += pl_sine_raw(s->formant_phase[2]) * formant_amplitude[2];
        o *= (1.0f - s->phase);
        *output++ = o;
    }
    s->pulse_next_sample = pulse_next_sample;
    pl_interp_done(&fm);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush) */
static inline void pl_speech_sam_flush(pl_speech_sam_t *s) { pl_flush(&s->pulse_lp); }
