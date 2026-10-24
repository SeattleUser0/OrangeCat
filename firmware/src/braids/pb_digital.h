/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: Braids' DigitalOscillator (braids/digital_oscillator.h, .cc), its Excitation (excitation.h) and
 * Svf (svf.h), in C. This file has the state, Init, Strike, ComputePhaseIncrement, ComputeDelay and Render (the
 * dispatch); the render functions themselves are in pb_dig_b.h, pb_dig_c.h and pb_dig_d.h:
 *   pb_dig_b.h  RenderTripleRingMod, RenderSawSwarm, RenderComb, RenderToy, RenderDigitalFilter, RenderVosim,
 *               RenderVowel (with InterpolateFormantParameter), RenderVowelFof, RenderHarmonics, RenderFm,
 *               RenderFeedbackFm, RenderChaoticFeedbackFm
 *   pb_dig_c.h  RenderPlucked, RenderBowed, RenderBlown, RenderFluted, RenderStruckBell, RenderStruckDrum, RenderKick,
 *               RenderCymbal, RenderSnare
 *   pb_dig_d.h  RenderWavetables, RenderWaveMap, RenderWaveLine, RenderWaveParaphonic, RenderFilteredNoise,
 *               RenderTwinPeaksNoise, RenderClockedNoise, RenderGranularCloud, RenderParticleNoise,
 *               RenderDigitalModulation, RenderQuestionMark
 * Each defines PB_DIG_B (C, D) as 1; a group not there yet renders silence (pb_dig_supported says which shapes are).
 * A render function is `static void pb_dig_render_<name>(pb_dig_t *o, const uint8_t *sync, int16_t *buffer,
 * size_t size)`; o->field is the C++'s field_ (phase_ is o->phase, state_.vow is o->state.vow, delay_lines_.comb is
 * o->delay_lines->comb, pulse_[i] is o->pulse[i], svf_[i] is o->svf[i]).
 *
 * The shape numbers: the C++'s DigitalOscillatorShape enum mislabels 15 to 23 (its names say STRUCK_BELL ... SNARE,
 * PLUCKED ... FLUTED; its fn_table_ says PLUCKED, BOWED, BLOWN, FLUTED, STRUCK_BELL, STRUCK_DRUM, KICK, CYMBAL,
 * SNARE). The table is what plays: PB_DIG_* below follow the table, and PB_OSC_SHAPE_* keep the enum's own values
 * for the comparisons the C++ makes with them.
 *
 * Device sizes: the comb's delay line is 4,096 samples on the device (8,192 in the C++ and under PB_PARITY); the
 * shape clamps its delay to the line already, so the lowest comb pitch rises from 11.7 Hz to 23 Hz. The other delay
 * lines keep their sizes. The state is then 8.2 KB (the plucked strings' lines).
 *
 * Revisions:
 *   0.5.1: first version.
 *   0.5.2: the delay lines out of pb_dig_t, behind a pointer (pb_delay_lines_t): SHAPES' voices share one set.
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

/* the table order (fn_table_), = MacroOscillatorShape - MACRO_OSC_SHAPE_TRIPLE_RING_MOD */
enum {
    PB_DIG_TRIPLE_RING_MOD, PB_DIG_SAW_SWARM, PB_DIG_COMB_FILTER, PB_DIG_TOY, PB_DIG_DIGITAL_FILTER_LP,
    PB_DIG_DIGITAL_FILTER_PK, PB_DIG_DIGITAL_FILTER_BP, PB_DIG_DIGITAL_FILTER_HP, PB_DIG_VOSIM, PB_DIG_VOWEL,
    PB_DIG_VOWEL_FOF, PB_DIG_HARMONICS, PB_DIG_FM, PB_DIG_FEEDBACK_FM, PB_DIG_CHAOTIC_FEEDBACK_FM, PB_DIG_PLUCKED,
    PB_DIG_BOWED, PB_DIG_BLOWN, PB_DIG_FLUTED, PB_DIG_STRUCK_BELL, PB_DIG_STRUCK_DRUM, PB_DIG_KICK, PB_DIG_CYMBAL,
    PB_DIG_SNARE, PB_DIG_WAVETABLES, PB_DIG_WAVE_MAP, PB_DIG_WAVE_LINE, PB_DIG_WAVE_PARAPHONIC, PB_DIG_FILTERED_NOISE,
    PB_DIG_TWIN_PEAKS_NOISE, PB_DIG_CLOCKED_NOISE, PB_DIG_GRANULAR_CLOUD, PB_DIG_PARTICLE_NOISE,
    PB_DIG_DIGITAL_MODULATION, PB_DIG_QUESTION_MARK, PB_DIG_N
};
/* the C++ enum's own values (for the comparisons with shape_ in the C++) */
enum {
    PB_OSC_SHAPE_TRIPLE_RING_MOD, PB_OSC_SHAPE_SAW_SWARM, PB_OSC_SHAPE_COMB_FILTER, PB_OSC_SHAPE_TOY,
    PB_OSC_SHAPE_DIGITAL_FILTER_LP, PB_OSC_SHAPE_DIGITAL_FILTER_PK, PB_OSC_SHAPE_DIGITAL_FILTER_BP,
    PB_OSC_SHAPE_DIGITAL_FILTER_HP, PB_OSC_SHAPE_VOSIM, PB_OSC_SHAPE_VOWEL, PB_OSC_SHAPE_VOWEL_FOF,
    PB_OSC_SHAPE_HARMONICS, PB_OSC_SHAPE_FM, PB_OSC_SHAPE_FEEDBACK_FM, PB_OSC_SHAPE_CHAOTIC_FEEDBACK_FM,
    PB_OSC_SHAPE_STRUCK_BELL, PB_OSC_SHAPE_STRUCK_DRUM, PB_OSC_SHAPE_KICK, PB_OSC_SHAPE_HAT, PB_OSC_SHAPE_SNARE,
    PB_OSC_SHAPE_PLUCKED, PB_OSC_SHAPE_BOWED, PB_OSC_SHAPE_BLOWN, PB_OSC_SHAPE_FLUTED, PB_OSC_SHAPE_WAVETABLES,
    PB_OSC_SHAPE_WAVE_MAP, PB_OSC_SHAPE_WAVE_LINE, PB_OSC_SHAPE_WAVE_PARAPHONIC, PB_OSC_SHAPE_FILTERED_NOISE,
    PB_OSC_SHAPE_TWIN_PEAKS_NOISE, PB_OSC_SHAPE_CLOCKED_NOISE, PB_OSC_SHAPE_GRANULAR_CLOUD,
    PB_OSC_SHAPE_PARTICLE_NOISE, PB_OSC_SHAPE_DIGITAL_MODULATION, PB_OSC_SHAPE_QUESTION_MARK_LAST
};

#define PB_WG_BRIDGE_LENGTH 1024u                       /* kWGBridgeLength */
#define PB_WG_NECK_LENGTH 4096u
#define PB_WG_BORE_LENGTH 2048u
#define PB_WG_JET_LENGTH 1024u
#define PB_WG_FBORE_LENGTH 4096u
#ifdef PB_PARITY
#define PB_COMB_DELAY_LENGTH 8192u                      /* kCombDelayLength */
#else
#define PB_COMB_DELAY_LENGTH 4096u                      /* (the device: see the header) */
#endif
#define PB_NUM_FORMANTS 5u
#define PB_NUM_PLUCK_VOICES 3u
#define PB_NUM_OVERLAPPING_FOF 3u
#define PB_NUM_BELL_PARTIALS 11u
#define PB_NUM_DRUM_PARTIALS 6u
#define PB_NUM_ADDITIVE_HARMONICS 12u

/* -------------------------------------------------------------- excitation.h --- */
typedef struct { uint32_t delay, decay; int32_t counter, state, level; } pb_excitation_t;
static inline void pb_excitation_init(pb_excitation_t *e) { e->delay = 0; e->decay = 4093; e->counter = 0; e->state = 0; }
static inline void pb_excitation_set_delay(pb_excitation_t *e, uint16_t delay) { e->delay = delay; }
static inline void pb_excitation_set_decay(pb_excitation_t *e, uint16_t decay) { e->decay = decay; }
static inline void pb_excitation_trigger(pb_excitation_t *e, int32_t level) { e->level = level; e->counter = (int32_t)(e->delay + 1u); }
static inline int pb_excitation_done(const pb_excitation_t *e) { return e->counter == 0; }
static inline int32_t pb_excitation_process(pb_excitation_t *e)
{
    e->state = (int32_t)((uint32_t)e->state * e->decay >> 12);   /* (int32 * uint32: unsigned in C++ too) */
    if (e->counter > 0) {
        --e->counter;
        if (e->counter == 0) e->state += e->level < 0 ? -e->level : e->level;
    }
    return e->level < 0 ? -e->state : e->state;
}

/* --------------------------------------------------------------------- svf.h --- */
enum { PB_SVF_MODE_LP, PB_SVF_MODE_BP, PB_SVF_MODE_HP };
typedef struct {
    int dirty;
    int16_t frequency, resonance;
    int32_t punch, f, damp, lp, bp;
    int mode;
} pb_svf_t;
static inline void pb_svf_init(pb_svf_t *s)
{
    s->lp = 0;
    s->bp = 0;
    s->frequency = 33 << 7;
    s->resonance = 16384;
    s->dirty = 1;
    s->punch = 0;
    s->mode = PB_SVF_MODE_BP;
}
static inline void pb_svf_set_frequency(pb_svf_t *s, int16_t frequency)
{
    s->dirty = s->dirty || (s->frequency != frequency);
    s->frequency = frequency;
}
static inline void pb_svf_set_resonance(pb_svf_t *s, int16_t resonance) { s->resonance = resonance; s->dirty = 1; }
static inline void pb_svf_set_punch(pb_svf_t *s, uint16_t punch) { s->punch = (int32_t)(((uint32_t)punch * punch) >> 24); }
static inline void pb_svf_set_mode(pb_svf_t *s, int mode) { s->mode = mode; }
static inline int32_t pb_svf_process(pb_svf_t *s, int32_t in)
{
    if (s->dirty) {
        s->f = pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)((uint32_t)s->frequency << 17));
        s->damp = pb_interpolate824_u16(pb_lut_svf_damp, (uint32_t)s->resonance << 17);
        s->dirty = 0;
    }
    int32_t f = s->f, damp = s->damp;
    if (s->punch) {
        int32_t punch_signal = s->lp > 4096 ? s->lp : 2048;
        f += ((punch_signal >> 4) * s->punch) >> 9;
        damp += ((punch_signal - 2048) >> 3);
    }
    int32_t notch = in - (s->bp * damp >> 15);
    s->lp += f * s->bp >> 15;
    PB_CLIP(s->lp);
    int32_t hp = notch - s->lp;
    s->bp += f * hp >> 15;
    PB_CLIP(s->bp);
    return s->mode == PB_SVF_MODE_BP ? s->bp : (s->mode == PB_SVF_MODE_HP ? hp : s->lp);
}

/* ---------------------------------------------------- DigitalOscillatorState --- */
typedef struct { uint32_t modulator_phase_increment, modulator_phase, square_modulator_phase; int32_t integrator; int polarity; } pb_reso_square_state_t;
typedef struct { uint32_t formant_increment[3], formant_phase[3], formant_amplitude[3]; uint16_t consonant_frames, noise; } pb_vowel_state_t;
typedef struct { uint32_t phase[6]; int32_t filter_state[2][2]; int32_t dc_blocked, lp, bp; } pb_saw_swarm_state_t;
typedef struct { int32_t amplitude[PB_NUM_ADDITIVE_HARMONICS]; } pb_harmonics_state_t;
typedef struct {
    uint32_t partial_phase[PB_NUM_BELL_PARTIALS], partial_phase_increment[PB_NUM_BELL_PARTIALS];
    int32_t partial_amplitude[PB_NUM_BELL_PARTIALS], target_partial_amplitude[PB_NUM_BELL_PARTIALS];
    int16_t previous_sample;
    size_t current_partial;
    int32_t lp_noise[3];
} pb_additive_state_t;
typedef struct {
    size_t size, write_ptr, shift, mask, pluck_position, initialization_ptr;
    uint32_t phase, phase_increment, max_phase_increment;
    int16_t previous_sample;
    uint8_t polyphony_assigner;
} pb_pluck_state_t;
typedef struct { uint32_t modulator_phase; int16_t previous_sample; } pb_feedback_fm_state_t;
typedef struct { uint16_t amplitude; int32_t filter_state[3][2], filter_scale[3], filter_coefficient[3]; } pb_particle_noise_state_t;
typedef struct { uint16_t delay_ptr, excitation_ptr; int32_t lp_state; int32_t filter_state[2]; int16_t previous_sample; } pb_physical_modelling_state_t;
typedef struct { uint32_t phase, phase_increment, envelope_phase, envelope_phase_increment; } pb_grain_t;
typedef struct { int32_t next_saw_sample; int16_t previous_sample; int32_t svf_lp[PB_NUM_FORMANTS], svf_bp[PB_NUM_FORMANTS]; } pb_fof_state_t;
typedef struct { uint8_t held_sample; uint16_t decimation_counter; } pb_toy_state_t;
typedef struct { int32_t bp, lp; } pb_svf_state_t;
typedef struct { uint32_t symbol_phase; uint16_t symbol_count; int32_t filter_state; uint8_t data_byte; } pb_digital_modulation_state_t;
typedef struct { uint32_t cycle_phase, cycle_phase_increment, rng_state; int32_t seed; int16_t sample; } pb_clocked_noise_state_t;
typedef struct { uint32_t phase[6]; uint32_t rng_state; } pb_hat_state_t;
typedef union {
    pb_reso_square_state_t res;
    pb_vowel_state_t vow;
    pb_saw_swarm_state_t saw;
    pb_pluck_state_t plk[4];
    pb_feedback_fm_state_t ffm;
    pb_particle_noise_state_t pno;
    pb_physical_modelling_state_t phy;
    pb_grain_t grain[4];
    pb_fof_state_t fof;
    pb_toy_state_t toy;
    pb_svf_state_t svf;
    pb_additive_state_t add;
    pb_digital_modulation_state_t dmd;
    pb_clocked_noise_state_t clk;
    pb_hat_state_t hat;
    pb_harmonics_state_t hrm;
    uint32_t modulator_phase;
} pb_dig_state_t;

/* ----------------------------------------------------------- DigitalOscillator --- */
/* (0.5.2) the delay lines of the comb, plucked, bowed, blown and fluted shapes: 8 KB, kept apart from the oscillator
 * (the C++ holds them inline) so that the SHAPES engine can give its four voices one shared set (the shapes that use
 * them play one voice at a time) and the oscillators stay small. Whoever makes a pb_dig_t points delay_lines at one
 * before it renders those shapes (eng_shapes.c, tests/braids_parity/port.c) */
typedef union {
    int16_t comb[PB_COMB_DELAY_LENGTH];
    int16_t ks[1025 * 4];
    struct { int8_t bridge[PB_WG_BRIDGE_LENGTH]; int8_t neck[PB_WG_NECK_LENGTH]; } bowed;
    int16_t bore[PB_WG_BORE_LENGTH];
    struct { int8_t jet[PB_WG_JET_LENGTH]; int8_t bore[PB_WG_FBORE_LENGTH]; } fluted;
} pb_delay_lines_t;
typedef struct {
    uint32_t phase, phase_increment, delay;
    int16_t parameter[2], previous_parameter[2];
    int32_t smoothed_parameter;
    int16_t pitch;
    uint8_t active_voice;
    int init, strike;
    int shape, previous_shape;
    pb_dig_state_t state;
    pb_excitation_t pulse[4];
    pb_svf_t svf[3];
    pb_delay_lines_t *delay_lines;                      /* (0.5.2: the C++'s delay_lines_, now apart: see above) */
} pb_dig_t;

static inline void pb_dig_init(pb_dig_t *o)
{
    uint8_t *p = (uint8_t *)&o->state;                  /* memset(&state_, 0, sizeof(state_)) */
    for (size_t i = 0; i < sizeof o->state; i++) p[i] = 0;
    for (int i = 0; i < 4; i++) pb_excitation_init(&o->pulse[i]);
    for (int i = 0; i < 3; i++) pb_svf_init(&o->svf[i]);
    o->phase = 0;
    o->strike = 1;
    o->init = 1;
}
static inline void pb_dig_set_shape(pb_dig_t *o, int shape) { o->shape = shape; }
static inline void pb_dig_set_pitch(pb_dig_t *o, int16_t pitch)
{
    if (o->pitch > (90 << 7) && pitch > (90 << 7)) o->pitch = (int16_t)(((int32_t)o->pitch + pitch) >> 1);
    else o->pitch = pitch;
}
static inline void pb_dig_set_parameters(pb_dig_t *o, int16_t parameter_1, int16_t parameter_2)
{
    o->parameter[0] = parameter_1;
    o->parameter[1] = parameter_2;
}
static inline void pb_dig_strike(pb_dig_t *o) { o->strike = 1; }

#define PB_HIGHEST_NOTE (140 * 128)
#define PB_PITCH_TABLE_START (128 * 128)
#define PB_OCTAVE (12 * 128)
static const uint32_t pb_fir4_coefficients[4] = { 10530, 14751, 16384, 14751 };   /* kFIR4Coefficients */
#define PB_FIR4_DC_OFFSET 28208u                                                    /* kFIR4DcOffset */

static inline uint32_t pb_dig_compute_phase_increment(int16_t midi_pitch)
{
    if (midi_pitch >= PB_PITCH_TABLE_START) midi_pitch = PB_PITCH_TABLE_START - 1;
    int32_t ref_pitch = midi_pitch;
    ref_pitch -= PB_PITCH_TABLE_START;
    size_t num_shifts = 0;
    while (ref_pitch < 0) {
        ref_pitch += PB_OCTAVE;
        ++num_shifts;
    }
    uint32_t a = pb_lut_oscillator_increments[ref_pitch >> 4];
    uint32_t b = pb_lut_oscillator_increments[(ref_pitch >> 4) + 1];
    uint32_t phase_increment = a + (uint32_t)((int32_t)(b - a) * (ref_pitch & 0xf) >> 4);
    phase_increment >>= num_shifts;
    return phase_increment;
}
static inline uint32_t pb_dig_compute_delay(int16_t midi_pitch)
{
    if (midi_pitch >= PB_HIGHEST_NOTE - PB_OCTAVE) midi_pitch = PB_HIGHEST_NOTE - PB_OCTAVE;
    int32_t ref_pitch = midi_pitch;
    ref_pitch -= PB_PITCH_TABLE_START;
    size_t num_shifts = 0;
    while (ref_pitch < 0) {
        ref_pitch += PB_OCTAVE;
        ++num_shifts;
    }
    uint32_t a = pb_lut_oscillator_delays[ref_pitch >> 4];
    uint32_t b = pb_lut_oscillator_delays[(ref_pitch >> 4) + 1];
    uint32_t delay = a + (uint32_t)((int32_t)(b - a) * (ref_pitch & 0xf) >> 4);
    delay >>= 12 - num_shifts;
    return delay;
}

/* the render functions, by group (each header defines its PB_DIG_x) */
#if __has_include("pb_dig_b.h")
#include "pb_dig_b.h"
#endif
#if __has_include("pb_dig_c.h")
#include "pb_dig_c.h"
#endif
#if __has_include("pb_dig_d.h")
#include "pb_dig_d.h"
#endif

/* the shapes whose render function is here (by group; the wavetable ones also need PB_WAVETABLES on the device) */
static inline int pb_dig_supported(int shape)
{
    if (shape <= PB_DIG_CHAOTIC_FEEDBACK_FM) {
#ifdef PB_DIG_B
        return 1;
#endif
    } else if (shape <= PB_DIG_SNARE) {
#ifdef PB_DIG_C
        return 1;
#endif
    } else {
#ifdef PB_DIG_D
#if !PB_WAVETABLES
        if (shape >= PB_DIG_WAVETABLES && shape <= PB_DIG_WAVE_PARAPHONIC) return 0;
#endif
        return 1;
#endif
    }
    return 0;
}

static void pb_dig_render(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* quantize the parameter for FM */
    if (o->shape >= PB_OSC_SHAPE_FM && o->shape <= PB_OSC_SHAPE_CHAOTIC_FEEDBACK_FM) {
        uint16_t integral = (uint16_t)(o->parameter[1] >> 8);
        uint16_t fractional = (uint16_t)(o->parameter[1] & 255);
        int16_t a = (int16_t)pb_lut_fm_frequency_quantizer[integral];
        int16_t b = (int16_t)pb_lut_fm_frequency_quantizer[integral + 1];
        o->parameter[1] = (int16_t)(a + ((b - a) * fractional >> 8));
    }
    if (o->shape != o->previous_shape) {
        pb_dig_init(o);
        o->previous_shape = o->shape;
        o->init = 1;
    }
    o->phase_increment = pb_dig_compute_phase_increment(o->pitch);
    o->delay = pb_dig_compute_delay(o->pitch);
    if (o->pitch > PB_HIGHEST_NOTE) o->pitch = PB_HIGHEST_NOTE;
    else if (o->pitch < 0) o->pitch = 0;
    if (!pb_dig_supported(o->shape)) {
        while (size--) *buffer++ = 0;
        return;
    }
    switch (o->shape) {
#ifdef PB_DIG_B
    case PB_DIG_TRIPLE_RING_MOD: pb_dig_render_triple_ring_mod(o, sync, buffer, size); break;
    case PB_DIG_SAW_SWARM: pb_dig_render_saw_swarm(o, sync, buffer, size); break;
    case PB_DIG_COMB_FILTER: pb_dig_render_comb(o, sync, buffer, size); break;
    case PB_DIG_TOY: pb_dig_render_toy(o, sync, buffer, size); break;
    case PB_DIG_DIGITAL_FILTER_LP: case PB_DIG_DIGITAL_FILTER_PK: case PB_DIG_DIGITAL_FILTER_BP:
    case PB_DIG_DIGITAL_FILTER_HP: pb_dig_render_digital_filter(o, sync, buffer, size); break;
    case PB_DIG_VOSIM: pb_dig_render_vosim(o, sync, buffer, size); break;
    case PB_DIG_VOWEL: pb_dig_render_vowel(o, sync, buffer, size); break;
    case PB_DIG_VOWEL_FOF: pb_dig_render_vowel_fof(o, sync, buffer, size); break;
    case PB_DIG_HARMONICS: pb_dig_render_harmonics(o, sync, buffer, size); break;
    case PB_DIG_FM: pb_dig_render_fm(o, sync, buffer, size); break;
    case PB_DIG_FEEDBACK_FM: pb_dig_render_feedback_fm(o, sync, buffer, size); break;
    case PB_DIG_CHAOTIC_FEEDBACK_FM: pb_dig_render_chaotic_feedback_fm(o, sync, buffer, size); break;
#endif
#ifdef PB_DIG_C
    case PB_DIG_PLUCKED: pb_dig_render_plucked(o, sync, buffer, size); break;
    case PB_DIG_BOWED: pb_dig_render_bowed(o, sync, buffer, size); break;
    case PB_DIG_BLOWN: pb_dig_render_blown(o, sync, buffer, size); break;
    case PB_DIG_FLUTED: pb_dig_render_fluted(o, sync, buffer, size); break;
    case PB_DIG_STRUCK_BELL: pb_dig_render_struck_bell(o, sync, buffer, size); break;
    case PB_DIG_STRUCK_DRUM: pb_dig_render_struck_drum(o, sync, buffer, size); break;
    case PB_DIG_KICK: pb_dig_render_kick(o, sync, buffer, size); break;
    case PB_DIG_CYMBAL: pb_dig_render_cymbal(o, sync, buffer, size); break;
    case PB_DIG_SNARE: pb_dig_render_snare(o, sync, buffer, size); break;
#endif
#ifdef PB_DIG_D
#if PB_WAVETABLES
    case PB_DIG_WAVETABLES: pb_dig_render_wavetables(o, sync, buffer, size); break;
    case PB_DIG_WAVE_MAP: pb_dig_render_wave_map(o, sync, buffer, size); break;
    case PB_DIG_WAVE_LINE: pb_dig_render_wave_line(o, sync, buffer, size); break;
    case PB_DIG_WAVE_PARAPHONIC: pb_dig_render_wave_paraphonic(o, sync, buffer, size); break;
#endif
    case PB_DIG_FILTERED_NOISE: pb_dig_render_filtered_noise(o, sync, buffer, size); break;
    case PB_DIG_TWIN_PEAKS_NOISE: pb_dig_render_twin_peaks_noise(o, sync, buffer, size); break;
    case PB_DIG_CLOCKED_NOISE: pb_dig_render_clocked_noise(o, sync, buffer, size); break;
    case PB_DIG_GRANULAR_CLOUD: pb_dig_render_granular_cloud(o, sync, buffer, size); break;
    case PB_DIG_PARTICLE_NOISE: pb_dig_render_particle_noise(o, sync, buffer, size); break;
    case PB_DIG_DIGITAL_MODULATION: pb_dig_render_digital_modulation(o, sync, buffer, size); break;
    case PB_DIG_QUESTION_MARK: pb_dig_render_question_mark(o, sync, buffer, size); break;
#endif
    default: while (size--) *buffer++ = 0; break;
    }
}
