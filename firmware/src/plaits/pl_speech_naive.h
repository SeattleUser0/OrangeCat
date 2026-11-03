/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the "naive" formant half of Plaits' SPEECH model (pl_speech.h), in C.
 *   plaits/dsp/speech/naive_speech_synth.{h,cc}   pl_speech_naive_t: a pulse oscillator (pl_osc.h) coloured by one
 *                                                 band-pass, through five band-pass formants (pl_svf_t)
 * Every float operation in the order of the C++.
 * Deliberate difference: the C++'s frequency_ field (set to 0 in Init, never read) is left out.
 */
#pragma once
#include "pl_osc.h"

#define PL_SPEECH_NAIVE_NUM_FORMANTS 5
#define PL_SPEECH_NAIVE_NUM_PHONEMES 5
#define PL_SPEECH_NAIVE_NUM_REGISTERS 5

typedef struct { uint8_t frequency, amplitude; } pl_speech_naive_formant_t;
typedef struct { pl_speech_naive_formant_t formant[PL_SPEECH_NAIVE_NUM_FORMANTS]; } pl_speech_naive_phoneme_t;

static const pl_speech_naive_phoneme_t
pl_speech_naive_phonemes[PL_SPEECH_NAIVE_NUM_PHONEMES][PL_SPEECH_NAIVE_NUM_REGISTERS] = {
    {
        { { { 74, 255 }, { 83, 114 }, { 97, 90 }, { 98, 90 }, { 100, 25 } } },
        { { { 75, 255 }, { 84, 128 }, { 100, 114 }, { 101, 101 }, { 103, 20 } } },
        { { { 76, 255 }, { 85, 128 }, { 100, 18 }, { 102, 16 }, { 104, 3 } } },
        { { { 79, 255 }, { 85, 161 }, { 101, 25 }, { 104, 4 }, { 110, 0 } } },
        { { { 79, 255 }, { 85, 128 }, { 101, 6 }, { 106, 25 }, { 110, 0 } } }
    },
    {
        { { { 67, 255 }, { 91, 64 }, { 98, 90 }, { 101, 64 }, { 102, 32 } } },
        { { { 67, 255 }, { 92, 51 }, { 99, 64 }, { 103, 51 }, { 105, 25 } } },
        { { { 69, 255 }, { 93, 51 }, { 100, 32 }, { 102, 25 }, { 103, 25 } } },
        { { { 67, 255 }, { 91, 16 }, { 100, 8 }, { 103, 4 }, { 110, 0 } } },
        { { { 65, 255 }, { 95, 25 }, { 101, 45 }, { 105, 2 }, { 110, 0 } } }
    },
    {
        { { { 59, 255 }, { 92, 8 }, { 99, 40 }, { 102, 20 }, { 104, 10 } } },
        { { { 61, 255 }, { 94, 45 }, { 101, 32 }, { 103, 25 }, { 105, 8 } } },
        { { { 60, 255 }, { 93, 16 }, { 101, 16 }, { 104, 4 }, { 105, 4 } } },
        { { { 65, 255 }, { 92, 25 }, { 100, 8 }, { 105, 4 }, { 110, 0 } } },
        { { { 60, 255 }, { 96, 64 }, { 101, 12 }, { 106, 12 }, { 110, 1 } } }
    },
    {
        { { { 67, 255 }, { 78, 72 }, { 98, 22 }, { 99, 25 }, { 101, 2 } } },
        { { { 67, 255 }, { 79, 80 }, { 99, 64 }, { 101, 64 }, { 102, 12 } } },
        { { { 68, 255 }, { 79, 80 }, { 100, 12 }, { 102, 20 }, { 103, 5 } } },
        { { { 69, 255 }, { 79, 90 }, { 101, 40 }, { 104, 10 }, { 110, 0 } } },
        { { { 69, 255 }, { 79, 72 }, { 101, 20 }, { 106, 20 }, { 110, 0 } } }
    },
    {
        { { { 65, 255 }, { 74, 25 }, { 98, 6 }, { 100, 10 }, { 101, 4 } } },
        { { { 65, 255 }, { 74, 25 }, { 100, 36 }, { 101, 51 }, { 103, 12 } } },
        { { { 66, 255 }, { 75, 25 }, { 100, 18 }, { 102, 8 }, { 104, 5 } } },
        { { { 63, 255 }, { 77, 64 }, { 99, 8 }, { 104, 2 }, { 110, 0 } } },
        { { { 63, 255 }, { 77, 40 }, { 100, 4 }, { 106, 2 }, { 110, 0 } } }
    },
};

typedef struct {
    pl_osc_t pulse;
    size_t click_duration;
    pl_svf_t filter[PL_SPEECH_NAIVE_NUM_FORMANTS];
    pl_svf_t pulse_coloration;
} pl_speech_naive_t;

static void pl_speech_naive_init(pl_speech_naive_t *n)
{
    int i;
    pl_osc_init(&n->pulse);
    n->click_duration = 0;
    for (i = 0; i < PL_SPEECH_NAIVE_NUM_FORMANTS; ++i) pl_svf_init(&n->filter[i]);
    pl_svf_init(&n->pulse_coloration);
    pl_svf_set_f_q(&n->pulse_coloration, PL_FREQ_DIRTY, 800.0f / PL_SAMPLE_RATE, 0.5f);
}

/* Render(click, frequency, phoneme, vocal_register, temp, excitation, output, size) (temp is not used by the C++) */
static void pl_speech_naive_render(pl_speech_naive_t *n, int click, float frequency, float phoneme,
                                   float vocal_register, float *temp, float *excitation, float *output, size_t size)
{
    (void)temp;
    size_t i;
    int f_i;
    if (click) n->click_duration = (size_t)(PL_SAMPLE_RATE * 0.05f);
    n->click_duration -= n->click_duration < size ? n->click_duration : size;

    if (n->click_duration) frequency *= 0.5f;

    /* Generate excitation signal (glottal pulse). */
    pl_osc_render(&n->pulse, PL_SHAPE_IMPULSE_TRAIN, frequency, 0.5f, 0, excitation, size);
    pl_svf_process_buf(&n->pulse_coloration, PL_BP, excitation, excitation, size);
    for (i = 0; i < size; ++i) excitation[i] *= 4.0f;

    float p = phoneme * (PL_SPEECH_NAIVE_NUM_PHONEMES - 1.001f);
    float r = vocal_register * (PL_SPEECH_NAIVE_NUM_REGISTERS - 1.001f);

    PL_MAKE_INTEGRAL_FRACTIONAL(p)
    PL_MAKE_INTEGRAL_FRACTIONAL(r)

    for (i = 0; i < size; ++i) output[i] = 0.0f;
    for (f_i = 0; f_i < PL_SPEECH_NAIVE_NUM_FORMANTS; ++f_i) {
        const pl_speech_naive_formant_t *p0r0 = &pl_speech_naive_phonemes[p_integral][r_integral].formant[f_i];
        const pl_speech_naive_formant_t *p0r1 = &pl_speech_naive_phonemes[p_integral][r_integral + 1].formant[f_i];
        const pl_speech_naive_formant_t *p1r0 = &pl_speech_naive_phonemes[p_integral + 1][r_integral].formant[f_i];
        const pl_speech_naive_formant_t *p1r1 = &pl_speech_naive_phonemes[p_integral + 1][r_integral + 1].formant[f_i];

        /* (uint8_t - uint8_t is an int, converted to float by the multiplication, as in the C++) */
        float p0r_f = (float)p0r0->frequency + (float)(p0r1->frequency - p0r0->frequency) * r_fractional;
        float p1r_f = (float)p1r0->frequency + (float)(p1r1->frequency - p1r0->frequency) * r_fractional;
        float f = p0r_f + (p1r_f - p0r_f) * p_fractional;

        float p0r_a = (float)p0r0->amplitude + (float)(p0r1->amplitude - p0r0->amplitude) * r_fractional;
        float p1r_a = (float)p1r0->amplitude + (float)(p1r1->amplitude - p1r0->amplitude) * r_fractional;
        float a = (p0r_a + (p1r_a - p0r_a) * p_fractional) / 256.0f;

        if (f >= 160.0f) f = 160.0f;
        f = PL_A0 * pl_semitones_to_ratio(f - 33.0f);
        if (n->click_duration && f_i == 0) f *= 0.5f;
        pl_svf_set_f_q(&n->filter[f_i], PL_FREQ_DIRTY, f, 20.0f);
        pl_svf_process_add(&n->filter[f_i], PL_BP, excitation, output, size, a);
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush) */
static inline void pl_speech_naive_flush(pl_speech_naive_t *n)
{
    int i;
    pl_flush(&n->pulse.next_sample);
    pl_flush(&n->pulse.lp_state);
    pl_flush(&n->pulse.hp_state);
    for (i = 0; i < PL_SPEECH_NAIVE_NUM_FORMANTS; ++i) pl_flush_svf(&n->filter[i]);
    pl_flush_svf(&n->pulse_coloration);
}
