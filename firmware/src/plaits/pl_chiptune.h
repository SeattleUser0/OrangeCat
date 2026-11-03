/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2: Plaits 1.2's CHIPTUNE (plaits/dsp/engine2/chiptune_engine.cc and .h), with its arpeggiator
 * (engine2/arpeggiator.h) and oscillators (oscillator/super_square_oscillator.h, oscillator/
 * nes_triangle_oscillator.h, NESTriangleOscillator<5>), in C. The chord bank (chords/chord_bank.cc) is
 * pl_chord_bank_t in pl_chord.h, shared with the CHORD model; stmlib's HysteresisQuantizer2 is pl_hysq_t
 * (pl_dsp2.h) and Random::GetWord pl_rng_word (pl_dsp.h, the one generator all the models share).
 *   Five "super square" voices (two hard-synced squares each) and a 32-step NES triangle bass an octave down.
 *   HARMONICS: the chord. MORPH: the squares' sync ratio (below 12 o'clock it fakes PWM).
 *   TRIGGER unpatched: TIMBRE is the chord's inversion; OUT: the five voices (alternate ones inverted).
 *   TRIGGER patched: each trigger steps an arpeggio over the chord's notes; TIMBRE picks one of 12 patterns
 *   (up, down, up-down, random, each over 1, 2 or 4 octaves); OUT: the one voice; already_enveloped is set.
 *   AUX: the bass.
 * Differences from the C++:
 *   - the C++ Init runs its voice loop to kChordNumNotes (4), not kChordNumVoices (5), so the fifth voice is
 *     never Init()ed: on the module it is what .bss gives, all zero (slave_frequency 0, not 0.01). pl_chiptune_init
 *     sets it to zero too, so the first block is the same.
 *   - the C++ Init leaves the arpeggiator's range_ unset (set_range comes before every Clock); here it is 0.
 *   - the envelope (envelope_shape, set by Plaits' voice.cc from the patch's timbre modulation amount; the C++'s
 *     set_envelope_shape) stays at PL_CHIPTUNE_NO_ENVELOPE unless the caller sets e->envelope_shape.
 */
#pragma once
#include "pl_chord.h"
#include "pl_dsp2.h"

#define PL_CHIPTUNE_NO_ENVELOPE 2.0f            /* ChiptuneEngine::NO_ENVELOPE */

/* ------------------------------------------- super_square_oscillator.h --- */
/* Two hard-synced squares with one meta-parameter: VariableShapeOscillator with pw 0.5, square only, sync on */
typedef struct {
    float master_phase, slave_phase, next_sample;
    int high;
    float master_frequency, slave_frequency;    /* (interpolated) */
} pl_chiptune_ssq_t;
static inline void pl_chiptune_ssq_init(pl_chiptune_ssq_t *o)
{
    o->master_phase = 0.0f;
    o->slave_phase = 0.0f;
    o->next_sample = 0.0f;
    o->high = 0;
    o->master_frequency = 0.0f;
    o->slave_frequency = 0.01f;
}
static void pl_chiptune_ssq_render(pl_chiptune_ssq_t *o, float frequency, float shape, float *out, size_t size)
{
    float master_frequency = frequency;
    frequency *= shape < 0.5f ? (0.51f + 0.98f * shape) : 1.0f + 16.0f * (shape - 0.5f) * (shape - 0.5f);

    if (master_frequency >= PL_MAX_FREQUENCY) master_frequency = PL_MAX_FREQUENCY;
    if (frequency >= PL_MAX_FREQUENCY) frequency = PL_MAX_FREQUENCY;

    pl_interp_t master_fm, fm;
    pl_interp_init(&master_fm, &o->master_frequency, master_frequency, size);
    pl_interp_init(&fm, &o->slave_frequency, frequency, size);

    float next_sample = o->next_sample;
    while (size--) {
        int reset = 0, transition_during_reset = 0;
        float reset_time = 0.0f;

        float this_sample = next_sample;
        next_sample = 0.0f;

        const float mf = pl_interp_next(&master_fm);
        const float sf = pl_interp_next(&fm);

        o->master_phase += mf;
        if (o->master_phase >= 1.0f) {
            o->master_phase -= 1.0f;
            reset_time = o->master_phase / mf;

            float slave_phase_at_reset = o->slave_phase + (1.0f - reset_time) * sf;
            reset = 1;
            if (slave_phase_at_reset >= 1.0f) {
                slave_phase_at_reset -= 1.0f;
                transition_during_reset = 1;
            }
            if (!o->high && slave_phase_at_reset >= 0.5f) transition_during_reset = 1;
            float value = slave_phase_at_reset < 0.5f ? 0.0f : 1.0f;
            this_sample -= value * pl_this_blep(reset_time);
            next_sample -= value * pl_next_blep(reset_time);
        }

        o->slave_phase += sf;
        while (transition_during_reset || !reset) {
            if (!o->high) {
                if (o->slave_phase < 0.5f) break;
                float t = (o->slave_phase - 0.5f) / sf;
                this_sample += pl_this_blep(t);
                next_sample += pl_next_blep(t);
                o->high = 1;
            }
            if (o->high) {
                if (o->slave_phase < 1.0f) break;
                o->slave_phase -= 1.0f;
                float t = o->slave_phase / sf;
                this_sample -= pl_this_blep(t);
                next_sample -= pl_next_blep(t);
                o->high = 0;
            }
        }

        if (reset) {
            o->slave_phase = reset_time * sf;
            o->high = 0;
        }

        next_sample += o->slave_phase < 0.5f ? 0.0f : 1.0f;
        *out++ = 2.0f * this_sample - 1.0f;
    }
    o->next_sample = next_sample;
    pl_interp_done(&fm);
    pl_interp_done(&master_fm);
}

/* ------------------------------------------- nes_triangle_oscillator.h --- */
/* NESTriangleOscillator<5>: a triangle in 32 steps, fading to a band-limited triangle at high frequencies */
#define PL_CHIPTUNE_NES_BITS 5
typedef struct {
    float phase, next_sample;
    int step, ascending;
    float frequency;                            /* (interpolated) */
} pl_chiptune_nes_t;
static inline void pl_chiptune_nes_init(pl_chiptune_nes_t *o)
{
    o->phase = 0.0f;
    o->step = 0;
    o->ascending = 1;
    o->next_sample = 0.0f;
    o->frequency = 0.001f;
}
static void pl_chiptune_nes_render(pl_chiptune_nes_t *o, float frequency, float *out, size_t size)
{
    /* the constants scaling the waveform and its discontinuities (num_steps is not 2) */
    const int num_steps = 1 << PL_CHIPTUNE_NES_BITS;
    const int half = num_steps / 2;
    const int top = num_steps - 1;
    const float num_steps_f = (float)num_steps;
    const float scale = 4.0f / (float)(top - 1);

    frequency = pl_minf(frequency, 0.25f);

    pl_interp_t fm;
    pl_interp_init(&fm, &o->frequency, frequency, size);

    float next_sample = o->next_sample;
    while (size--) {
        const float f = pl_interp_next(&fm);
        o->phase += f;

        /* where the "full resolution" NES triangle gives way to a naive band-limited one */
        float fade_to_tri = (f - 0.5f / num_steps_f) * 2.0f * num_steps_f;
        PL_CONSTRAIN(fade_to_tri, 0.0f, 1.0f);

        const float nes_gain = 1.0f - fade_to_tri;
        const float tri_gain = fade_to_tri * 2.0f / scale;

        float this_sample = next_sample;
        next_sample = 0.0f;

        /* the discontinuity at the top of the naive triangle */
        if (o->ascending && o->phase >= 0.5f) {
            float discontinuity = 4.0f * f * tri_gain;
            if (discontinuity != 0.0f) {
                float t = (o->phase - 0.5f) / f;
                this_sample -= pl_this_integrated_blep(t) * discontinuity;
                next_sample -= pl_next_integrated_blep(t) * discontinuity;
            }
            o->ascending = 0;
        }

        int next_step = (int)(o->phase * num_steps_f);
        if (next_step != o->step) {
            int wrap = 0;
            if (next_step >= num_steps) {
                o->phase -= 1.0f;
                next_step -= num_steps;
                wrap = 1;
            }

            float discontinuity = next_step < half ? 1.0f : -1.0f;
            if (next_step == 0 || next_step == half) discontinuity = 0.0f;

            /* the discontinuity at each step of the NES triangle */
            discontinuity *= nes_gain;
            if (discontinuity != 0.0f) {
                float frac = (o->phase * num_steps_f - (float)next_step);
                float t = frac / (f * num_steps_f);
                this_sample += pl_this_blep(t) * discontinuity;
                next_sample += pl_next_blep(t) * discontinuity;
            }

            /* the discontinuity at the bottom of the naive triangle */
            if (wrap) {
                float wrap_discontinuity = 4.0f * f * tri_gain;
                if (wrap_discontinuity != 0.0f) {
                    float t = o->phase / f;
                    this_sample += pl_this_integrated_blep(t) * wrap_discontinuity;
                    next_sample += pl_next_integrated_blep(t) * wrap_discontinuity;
                }
                o->ascending = 1;
            }
        }
        o->step = next_step;

        /* the NES triangle */
        next_sample += nes_gain * (float)(o->step < half ? o->step : top - o->step);

        /* the naive triangle */
        next_sample += tri_gain * (o->phase < 0.5f ? 2.0f * o->phase : 2.0f - 2.0f * o->phase);

        *out++ = this_sample * scale - 1.0f;
    }
    o->next_sample = next_sample;
    pl_interp_done(&fm);
}

/* ------------------------------------------------------- arpeggiator.h --- */
enum { PL_CHIPTUNE_ARP_UP, PL_CHIPTUNE_ARP_DOWN, PL_CHIPTUNE_ARP_UP_DOWN, PL_CHIPTUNE_ARP_RANDOM };
typedef struct { int mode, range, note, octave, direction; } pl_chiptune_arp_t;
static inline void pl_chiptune_arp_reset(pl_chiptune_arp_t *a)
{
    a->note = 0;
    a->octave = 0;
    a->direction = 1;
}
static inline void pl_chiptune_arp_init(pl_chiptune_arp_t *a)
{
    a->mode = PL_CHIPTUNE_ARP_UP;
    a->range = 0;                               /* (not set by the C++ Init: set_range comes before every Clock) */
    pl_chiptune_arp_reset(a);
}
static void pl_chiptune_arp_clock(pl_chiptune_arp_t *a, int num_notes)
{
    if (num_notes == 1 && a->range == 1) {
        a->note = a->octave = 0;
        return;
    }

    if (a->mode == PL_CHIPTUNE_ARP_RANDOM) {
        while (1) {
            uint32_t w = pl_rng_word();
            int octave = (int)((w >> 4) % (uint32_t)a->range);
            int note = (int)((w >> 20) % (uint32_t)num_notes);
            if (octave != a->octave || note != a->note) {
                a->octave = octave;
                a->note = note;
                break;
            }
        }
        return;
    }

    if (a->mode == PL_CHIPTUNE_ARP_UP) a->direction = 1;
    if (a->mode == PL_CHIPTUNE_ARP_DOWN) a->direction = -1;

    a->note += a->direction;

    int done = 0;
    while (!done) {
        done = 1;
        if (a->note >= num_notes || a->note < 0) {
            a->octave += a->direction;
            a->note = a->direction > 0 ? 0 : num_notes - 1;
        }
        if (a->octave >= a->range || a->octave < 0) {
            a->octave = a->direction > 0 ? 0 : a->range - 1;
            if (a->mode == PL_CHIPTUNE_ARP_UP_DOWN) {
                a->direction = -a->direction;
                a->note = a->direction > 0 ? 1 : num_notes - 2;
                a->octave = a->direction > 0 ? 0 : a->range - 1;
                done = 0;
            }
        }
    }
}

/* --------------------------------------------------- chiptune_engine.cc --- */
typedef struct {
    pl_chiptune_ssq_t voice[PL_CHORD_NUM_VOICES];
    pl_chiptune_nes_t bass;
    pl_chord_bank_t chords;
    pl_chiptune_arp_t arpeggiator;
    pl_hysq_t arpeggiator_pattern_selector;
    float envelope_shape, envelope_state, aux_envelope_amount;
} pl_chiptune_t;

/* ChordBank::num_notes() and sorted_ratio(note) (pl_chord.h has no accessors for these) */
static inline int pl_chiptune_num_notes(const pl_chord_bank_t *b) { return b->note_count[pl_chord_bank_chord_index(b)]; }
static inline float pl_chiptune_sorted_ratio(const pl_chord_bank_t *b, int note) { return b->sorted_ratios[note]; }

static void pl_chiptune_init(pl_chiptune_t *e)
{
    int i;
    pl_chiptune_nes_init(&e->bass);
    for (i = 0; i < PL_CHORD_NUM_NOTES; ++i) pl_chiptune_ssq_init(&e->voice[i]);
    /* (the C++ loop stops at kChordNumNotes: the fifth voice is never Init()ed, all zero on the module) */
    e->voice[PL_CHORD_NUM_VOICES - 1].master_phase = 0.0f;
    e->voice[PL_CHORD_NUM_VOICES - 1].slave_phase = 0.0f;
    e->voice[PL_CHORD_NUM_VOICES - 1].next_sample = 0.0f;
    e->voice[PL_CHORD_NUM_VOICES - 1].high = 0;
    e->voice[PL_CHORD_NUM_VOICES - 1].master_frequency = 0.0f;
    e->voice[PL_CHORD_NUM_VOICES - 1].slave_frequency = 0.0f;

    pl_chord_bank_init(&e->chords);

    pl_chiptune_arp_init(&e->arpeggiator);

    pl_hysq_init(&e->arpeggiator_pattern_selector, 12, 0.075f, 0);

    e->envelope_shape = PL_CHIPTUNE_NO_ENVELOPE;
    e->envelope_state = 0.0f;
    e->aux_envelope_amount = 0.0f;
}
static void pl_chiptune_reset(pl_chiptune_t *e) { pl_chord_bank_reset(&e->chords); }

static void pl_chiptune_render(pl_chiptune_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    const float f0 = pl_note_to_frequency(p->note);
    const float shape = p->morph * 0.995f;
    const int clocked = !(p->trigger & PL_TRIGGER_UNPATCHED);
    float root_transposition = 1.0f;
    size_t i, j;
    int voice;

    *already_enveloped = clocked;

    if (clocked) {
        if (p->trigger & PL_TRIGGER_RISING_EDGE) {
            pl_chord_bank_set_chord(&e->chords, p->harmonics);
            pl_chord_bank_sort(&e->chords);

            int pattern = pl_hysq_process(&e->arpeggiator_pattern_selector, p->timbre);
            e->arpeggiator.mode = pattern / 3;
            e->arpeggiator.range = 1 << (pattern % 3);
            pl_chiptune_arp_clock(&e->arpeggiator, pl_chiptune_num_notes(&e->chords));
            e->envelope_state = 1.0f;
        }
        const float octave = (float)(1 << e->arpeggiator.octave);
        const float note_f0 = f0 * pl_chiptune_sorted_ratio(&e->chords, e->arpeggiator.note) * octave;
        root_transposition = octave;
        pl_chiptune_ssq_render(&e->voice[0], note_f0, shape, out, size);
    } else {
        float ratios[PL_CHORD_NUM_VOICES];
        float amplitudes[PL_CHORD_NUM_VOICES];

        pl_chord_bank_set_chord(&e->chords, p->harmonics);
        pl_chord_bank_compute_inversion(&e->chords, p->timbre, ratios, amplitudes);
        for (voice = 1; voice < PL_CHORD_NUM_VOICES; voice += 2) amplitudes[voice] = -amplitudes[voice];

        for (i = 0; i < size; ++i) out[i] = 0.0f;
        for (voice = 0; voice < PL_CHORD_NUM_VOICES; ++voice) {
            const float voice_f0 = f0 * ratios[voice];
            pl_chiptune_ssq_render(&e->voice[voice], voice_f0, shape, aux, size);
            for (j = 0; j < size; ++j) out[j] += aux[j] * amplitudes[voice];
        }
    }

    /* the bass note */
    pl_chiptune_nes_render(&e->bass, f0 * 0.5f * root_transposition, aux, size);

    /* the envelope, if any */
    if (e->envelope_shape != PL_CHIPTUNE_NO_ENVELOPE) {
        const float envelope_shape = pl_fabsf(e->envelope_shape);
        const float decay = 1.0f - 2.0f / PL_SAMPLE_RATE * pl_semitones_to_ratio(60.0f * envelope_shape) * envelope_shape;
        float aux_envelope_amount = e->envelope_shape * 20.0f;
        PL_CONSTRAIN(aux_envelope_amount, 0.0f, 1.0f);

        for (i = 0; i < size; ++i) {
            PL_ONE_POLE(e->aux_envelope_amount, aux_envelope_amount, 0.01f);
            e->envelope_state *= decay;
            out[i] *= e->envelope_state;
            aux[i] *= 1.0f + e->aux_envelope_amount * (e->envelope_state - 1.0f);
        }
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_chiptune_flush(pl_chiptune_t *e) { pl_flush(&e->envelope_state); pl_flush(&e->aux_envelope_amount); }
