/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: Plaits 1.2's six-operator FM engine (plaits/dsp/engine2/six_op_engine.cc, the
 * DX7-style engine registered three times in Plaits, once a bank of 32 patches), in C.
 *   OUT = AUX: two FM voices, each the six-operator DX7 voice (pl_dx_voice.h), soft-clipped. HARMONICS picks the
 *        patch (1 of the bank's 32, with hysteresis), TIMBRE the brightness (the modulators' level, +/- 4 units of
 *        the DX7's TL), MORPH the envelope control: triggered, the attack / decay speed (below 12 o'clock slower,
 *        above faster) and the release; free-running (no trigger), the position in the envelopes and in the LFO
 *        (the "scrub"), so a slow sweep plays the note's whole evolution. ACCENT is the velocity.
 *   The DX7 units (patch, envelopes, LFO) are pl_dx_units.h, the voice and the algorithms pl_dx_voice.h.
 *   Two voices: each trigger takes the other voice, and a voice renders every other block, two blocks at a time
 *   (the staggered rendering: acc_buffer carries the second half to the next block).
 *   The banks: pl_tables2.h's pl_syx_bank_0/1/2 (Plaits' fm_patches_table), packed, 32 x 128 bytes each.
 *   kCorrectedSampleRate is PL_CORRECTED_SAMPLE_RATE (48 kHz under PL_PARITY, 44.1 kHz on the device: the DX7
 *   envelope rates and LFO rates are in Hz, so they are right at either).
 *
 * Deliberate differences from the C++ (Orange Cat):
 *   - The C++ unpacks all 32 patches of the bank at LoadUserData (32 x 156 bytes). Here the engine keeps a pointer to
 *     the packed bank and each voice keeps the one patch it plays, unpacked from its 128 packed bytes at the moment
 *     FMVoice::LoadPatch would take a new patch pointer; LoadPatch's "same patch, return" test compares the patch's
 *     index in the bank (patch_index, -1 for none) where the C++ compares pointers. Patch::Unpack is deterministic,
 *     so the voice sees exactly the patch the C++ sees: the sound is identical.
 *   - No user patch banks (Plaits' UserData): the three built-in banks only.
 *   - The render calls of the 32 algorithms are compiled at init as in the C++, but in 5 bytes each, not 16
 *     (pl_dx_voice.h). The scratch buffer (temp_buffer_, 96 floats) is on the render's stack.
 *   - init zeroes acc_buffer and the unpacked patches (the C++'s come from a zeroed allocator) and takes bank 0
 *     until pl_sixop_load_bank is called.
 *
 * Revisions:
 *   MACRO2: first version.
 */
#pragma once
#include "pl_dsp2.h"
#include "pl_dx_voice.h"

#define PL_SIXOP_NUM_VOICES 2                   /* kNumSixOpVoices */
#define PL_SIXOP_PATCHES_PER_BANK 32            /* kNumPatchesPerBank */

/* ------------------------------------------------------------ FMVoice --- */
typedef struct {
    int patch_index;                            /* the loaded patch's index in the bank, -1 none (the C++'s patch_) */
    pl_dx_lfo_t lfo;
    pl_dx_voice_t voice;
    pl_dx_voice_params_t parameters;
} pl_sixop_voice_t;

static void pl_sixop_voice_init(pl_sixop_voice_t *v, float sample_rate)
{
    pl_dx_voice_init(&v->voice, sample_rate);
    pl_dx_lfo_init(&v->lfo, sample_rate);
    v->parameters.sustain = 0;
    v->parameters.gate = 0;
    v->parameters.note = 48.0f;
    v->parameters.velocity = 0.5f;
    v->parameters.brightness = 0.5f;
    v->parameters.envelope_control = 0.5f;
    v->parameters.pitch_mod = 0.0f;
    v->parameters.amp_mod = 0.0f;
    v->patch_index = -1;
}
static void pl_sixop_voice_render(pl_sixop_voice_t *v, const pl_dx_algorithms_t *algorithms, float *buffer, size_t size)
{
    if (v->patch_index < 0) return;
    pl_dx_voice_render_temp(&v->voice, algorithms, &v->parameters, buffer, size);
}
/* LoadPatch(&patches_[index]): bank is the packed bank, index the patch's index in it */
static void pl_sixop_voice_load_patch(pl_sixop_voice_t *v, const uint8_t *bank, int index)
{
    if (index == v->patch_index) return;
    v->patch_index = index;
    pl_dx_voice_set_patch(&v->voice, bank + index * PL_DX_SYX_SIZE);
    pl_dx_lfo_set(&v->lfo, &v->voice.patch.modulations);
}
static inline void pl_sixop_voice_unload_patch(pl_sixop_voice_t *v) { v->patch_index = -1; }
static inline void pl_sixop_voice_set_modulations(pl_sixop_voice_t *v, const pl_dx_lfo_t *lfo)
{
    v->parameters.pitch_mod = pl_dx_lfo_pitch_mod(lfo);
    v->parameters.amp_mod = pl_dx_lfo_amp_mod(lfo);
}

/* -------------------------------------------------------- SixOpEngine --- */
typedef struct {
    pl_hysq_t patch_index_quantizer;
    pl_dx_algorithms_t algorithms;
    const uint8_t *bank;                        /* the packed bank (pl_syx_banks[]), where the C++ has patches_ */
    pl_sixop_voice_t voice[PL_SIXOP_NUM_VOICES];
    float acc_buffer[PL_MAX_BLOCK * PL_SIXOP_NUM_VOICES];
    int active_voice, rendered_voice;
} pl_sixop_t;

static void pl_sixop_init(pl_sixop_t *e)
{
    pl_hysq_init(&e->patch_index_quantizer, 32, 0.005f, 0);
    pl_dx_algorithms_init(&e->algorithms);
    for (int i = 0; i < PL_SIXOP_NUM_VOICES; ++i) pl_sixop_voice_init(&e->voice[i], PL_CORRECTED_SAMPLE_RATE);
    for (size_t i = 0; i < PL_MAX_BLOCK * PL_SIXOP_NUM_VOICES; ++i) e->acc_buffer[i] = 0.0f;
    e->bank = pl_syx_banks[0];
    e->active_voice = PL_SIXOP_NUM_VOICES - 1;
    e->rendered_voice = 0;
}
static void pl_sixop_reset(pl_sixop_t *e) { (void)e; }
/* LoadUserData(fm_patches_table[bank]), bank 0..2: the bank's patches are unpacked by the voices as they load them */
static void pl_sixop_load_bank(pl_sixop_t *e, int bank)
{
    PL_CONSTRAIN(bank, 0, 2);
    e->bank = pl_syx_banks[bank];
    for (int i = 0; i < PL_SIXOP_NUM_VOICES; ++i) pl_sixop_voice_unload_patch(&e->voice[i]);
}

static void pl_sixop_render(pl_sixop_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    float temp_buffer[PL_MAX_BLOCK * 4];        /* (the C++'s temp_buffer_: scratch, nothing carried over) */
    int patch_index = pl_hysq_process(&e->patch_index_quantizer, p->harmonics * 1.02f);

    if (p->trigger & PL_TRIGGER_UNPATCHED) {
        const float t = p->morph;
        pl_dx_lfo_scrub(&e->voice[0].lfo, 2.0f * PL_CORRECTED_SAMPLE_RATE * t);
        for (int i = 0; i < PL_SIXOP_NUM_VOICES; ++i) {
            pl_sixop_voice_load_patch(&e->voice[i], e->bank, patch_index);
            pl_dx_voice_params_t *vp = &e->voice[i].parameters;
            vp->sustain = i == 0 ? 1 : 0;
            vp->gate = 0;
            vp->note = p->note;
            vp->velocity = p->accent;
            vp->brightness = p->timbre;
            vp->envelope_control = t;
            pl_sixop_voice_set_modulations(&e->voice[i], &e->voice[0].lfo);
        }
    } else {
        if (p->trigger & PL_TRIGGER_RISING_EDGE) {
            e->active_voice = (e->active_voice + 1) % PL_SIXOP_NUM_VOICES;
            pl_sixop_voice_load_patch(&e->voice[e->active_voice], e->bank, patch_index);
            pl_dx_lfo_reset(&e->voice[e->active_voice].lfo);
        }
        pl_dx_voice_params_t *ap = &e->voice[e->active_voice].parameters;
        ap->note = p->note;
        ap->velocity = p->accent;
        ap->envelope_control = p->morph;
        pl_dx_lfo_step(&e->voice[e->active_voice].lfo, (float)size);

        for (int i = 0; i < PL_SIXOP_NUM_VOICES; ++i) {
            pl_dx_voice_params_t *vp = &e->voice[i].parameters;
            vp->brightness = p->timbre;
            vp->sustain = 0;
            vp->gate = (p->trigger & PL_TRIGGER_HIGH) && (i == e->active_voice);
            if (e->voice[i].patch_index != e->voice[e->active_voice].patch_index) {
                pl_dx_lfo_step(&e->voice[i].lfo, (float)size);
                pl_sixop_voice_set_modulations(&e->voice[i], &e->voice[i].lfo);
            } else {
                pl_sixop_voice_set_modulations(&e->voice[i], &e->voice[e->active_voice].lfo);
            }
        }
    }

    /* staggered rendering: one voice a block, two blocks' worth; the second half waits in acc_buffer */
    for (size_t i = 0; i < (PL_SIXOP_NUM_VOICES - 1) * size; ++i) temp_buffer[i] = e->acc_buffer[i];
    for (size_t i = (PL_SIXOP_NUM_VOICES - 1) * size; i < PL_SIXOP_NUM_VOICES * size; ++i) temp_buffer[i] = 0.0f;
    e->rendered_voice = (e->rendered_voice + 1) % PL_SIXOP_NUM_VOICES;
    pl_sixop_voice_render(&e->voice[e->rendered_voice], &e->algorithms, temp_buffer, size * PL_SIXOP_NUM_VOICES);

    for (size_t i = 0; i < size; ++i) aux[i] = out[i] = pl_soft_clip(temp_buffer[i] * 0.25f);
    for (size_t i = 0; i < (PL_SIXOP_NUM_VOICES - 1) * size; ++i) e->acc_buffer[i] = temp_buffer[size + i];
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render.
 * The signal states only: each voice's two feedback samples and the half block waiting in acc_buffer (both are sums
 * of operator outputs; a value below 2^-97 adds nothing to a phase and under 1e-29 to the output). The operator
 * amplitudes never fall below 2^-14 (an envelope level has a floor), the envelopes and LFOs do not decay: untouched. */
static inline void pl_sixop_flush(pl_sixop_t *e)
{
    for (int i = 0; i < PL_SIXOP_NUM_VOICES; ++i) pl_flush_n(e->voice[i].voice.feedback_state, 2);
    pl_flush_n(e->acc_buffer, PL_MAX_BLOCK * PL_SIXOP_NUM_VOICES);
}
