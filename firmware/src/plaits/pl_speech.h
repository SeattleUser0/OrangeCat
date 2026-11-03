/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: Plaits' model 7, SPEECH (plaits/dsp/engine/speech_engine.{h,cc}), with its three
 * synths, in C: pl_speech_naive.h (naive_speech_synth), pl_speech_sam.h (sam_speech_synth) and pl_speech_lpc.h
 * (lpc_speech_synth, its controller, word bank, phonemes; the word banks' bytes are in pl_tables2.h).
 *   HARMONICS: the synth. Below 1/3, a crossfade between the naive formant synth and SAM; 1/3 to 2/3, between SAM
 *       and the LPC10 synth scanning its vowels; above 2/3, the LPC synth on one of the five word banks.
 *   TIMBRE: the formant shift (and in the LPC synth the speed). MORPH: the phoneme or, in a word bank, the word.
 *   Trigger (below 2/3): a consonant. Trigger in a word bank: the word is spoken; while it is, *already_enveloped
 *       is set (the word's own energy envelope is the envelope, scaled by ACCENT) unless the trigger is unpatched.
 *   OUT: the voice. AUX: the raw excitation.
 * Every float operation in the order of the C++.
 *
 * Deliberate differences:
 *   The word bank decodes on the fly (pl_speech_lpc.h): the C++ keeps a whole decoded bank (1024 frames of 14 bytes
 *   and 32 boundaries from the allocator; an exact-array port sized to the largest bank, 928 frames, cost 13,116
 *   bytes per engine). Here Load keeps only where each word starts, and PlayFrame decodes the two frames it needs
 *   from the word's start, going on from the last read when it can: 160 bytes, the same frame bytes.
 *   temp_buffer_[2] (2 x kMaxBlockSize floats, from the allocator in the C++) is on the render's stack.
 *   prosody_amount and speed are plain fields (the C++ setters; Plaits' voice never sets them: both stay 0).
 *   No user data (the C++ LoadUserData is empty for this engine anyway).
 */
#pragma once
#include "pl_speech_lpc.h"
#include "pl_speech_naive.h"
#include "pl_speech_sam.h"

typedef struct {
    pl_hysq_t word_bank_quantizer;
    pl_speech_naive_t naive_speech_synth;
    pl_speech_sam_t sam_speech_synth;
    pl_speech_lpc_ctl_t lpc_speech_synth_controller;
    pl_speech_word_bank_t lpc_speech_synth_word_bank;
    float prosody_amount;                       /* set_prosody_amount (0: the words keep their own pitch) */
    float speed;                                /* set_speed */
} pl_speech_t;

static void pl_speech_init(pl_speech_t *e)
{
    pl_speech_sam_init(&e->sam_speech_synth);
    pl_speech_naive_init(&e->naive_speech_synth);
    pl_speech_word_bank_init(&e->lpc_speech_synth_word_bank);
    pl_speech_lpc_ctl_init(&e->lpc_speech_synth_controller);
    pl_hysq_init(&e->word_bank_quantizer, (int)PL_LPC_NUM_WORD_BANKS + 1, 0.1f, 0);
    e->prosody_amount = 0.0f;
    e->speed = 0.0f;
}
static void pl_speech_reset(pl_speech_t *e) { pl_speech_word_bank_reset(&e->lpc_speech_synth_word_bank); }

static void pl_speech_render(pl_speech_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                             int *already_enveloped)
{
    float temp_buffer[2][PL_MAX_BLOCK];        /* (the C++'s temp_buffer_, from the allocator) */
    size_t i;
    const float f0 = pl_note_to_frequency(p->note);

    const float group = p->harmonics * 6.0f;

    /* Interpolates between the 3 models: naive, SAM, LPC. */
    if (group <= 2.0f) {
        *already_enveloped = 0;

        float blend = group;
        if (group <= 1.0f) {
            pl_speech_naive_render(&e->naive_speech_synth, p->trigger == PL_TRIGGER_RISING_EDGE, f0, p->morph,
                                   p->timbre, temp_buffer[0], aux, out, size);
        } else {
            pl_speech_lpc_ctl_render(&e->lpc_speech_synth_controller, &e->lpc_speech_synth_word_bank,
                                     p->trigger & PL_TRIGGER_UNPATCHED, p->trigger & PL_TRIGGER_RISING_EDGE, -1, f0,
                                     0.0f, 0.0f, p->morph, p->timbre, 1.0f, aux, out, size);
            blend = 2.0f - blend;
        }

        pl_speech_sam_render(&e->sam_speech_synth, p->trigger == PL_TRIGGER_RISING_EDGE, f0, p->morph, p->timbre,
                             temp_buffer[0], temp_buffer[1], size);

        blend *= blend * (3.0f - 2.0f * blend);
        blend *= blend * (3.0f - 2.0f * blend);
        for (i = 0; i < size; ++i) {
            aux[i] += (temp_buffer[0][i] - aux[i]) * blend;
            out[i] += (temp_buffer[1][i] - out[i]) * blend;
        }
    } else {
        /* Change phonemes/words for LPC. */
        const int word_bank = pl_hysq_process(&e->word_bank_quantizer, (group - 2.0f) * 0.275f) - 1;

        const int replay_prosody = word_bank >= 0 && !(p->trigger & PL_TRIGGER_UNPATCHED);

        *already_enveloped = replay_prosody;

        pl_speech_lpc_ctl_render(&e->lpc_speech_synth_controller, &e->lpc_speech_synth_word_bank,
                                 p->trigger & PL_TRIGGER_UNPATCHED, p->trigger & PL_TRIGGER_RISING_EDGE, word_bank,
                                 f0, e->prosody_amount, e->speed, p->morph, p->timbre,
                                 replay_prosody ? p->accent : 1.0f, aux, out, size);
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_speech_flush(pl_speech_t *e)
{
    pl_speech_naive_flush(&e->naive_speech_synth);
    pl_speech_sam_flush(&e->sam_speech_synth);
    pl_speech_lpc_ctl_flush(&e->lpc_speech_synth_controller);
}
