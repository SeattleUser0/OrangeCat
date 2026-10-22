/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the first group of Braids' DigitalOscillator render functions (braids/digital_oscillator.cc), in
 * C. Included by pb_digital.h after the state and helpers; defines PB_DIG_B.
 *
 *   RenderTripleRingMod      pb_dig_render_triple_ring_mod
 *   RenderSawSwarm           pb_dig_render_saw_swarm
 *   RenderComb               pb_dig_render_comb (reached through the SAW_COMB macro shape)
 *   RenderToy                pb_dig_render_toy
 *   RenderDigitalFilter      pb_dig_render_digital_filter (LP, PK, BP, HP), with kPhaseReset as pbb_phase_reset
 *   RenderVosim              pb_dig_render_vosim
 *   RenderVowel              pb_dig_render_vowel, with vowels_data / consonant_data as pbb_vowels_data /
 *                            pbb_consonant_data
 *   RenderVowelFof           pb_dig_render_vowel_fof, with InterpolateFormantParameter as
 *                            pbb_interpolate_formant_parameter and formant_f_data / formant_a_data as
 *                            pbb_formant_f_data / pbb_formant_a_data
 *   RenderHarmonics          pb_dig_render_harmonics
 *   RenderFm                 pb_dig_render_fm
 *   RenderFeedbackFm         pb_dig_render_feedback_fm
 *   RenderChaoticFeedbackFm  pb_dig_render_chaotic_feedback_fm
 *
 * Same integer arithmetic as the C++, so the output is bit-exact (tests/braids_parity). Where the C++ shifts a
 * negative int left or lets a signed product wrap (the FM phase offsets, the swarm's detune interpolation, the
 * filter's window product), the C does the same multiply or shift in uint32_t and converts back: the same bits the
 * C++ gets on its targets, without relying on undefined behaviour.
 *
 * On the device:
 *   - the comb's delay line is PB_COMB_DELAY_LENGTH (4,096 samples, 8,192 in the C++ and under PB_PARITY); the
 *     shape clamps its delay to the line, so its lowest pitch rises one octave (see pb_digital.h).
 *   - the C++'s kNumAdditiveHarmonics is a size_t; on the 64-bit test host that makes HARMONICS' peak position
 *     64-bit arithmetic, on Braids' 32-bit ARM it is 32-bit. The port uses 32 bits, as Braids does. The two only
 *     differ for a negative TIMBRE, which the module never sends.
 *
 * Revisions:
 *   0.5.1: first version.
 *   0.5.2: o->delay_lines.x is o->delay_lines->x (pb_digital.h pb_delay_lines_t).
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

#define PB_DIG_B 1

/* ------------------------------------------------------- RenderTripleRingMod --- */
static void pb_dig_render_triple_ring_mod(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint32_t phase = o->phase + (1u << 30);
    uint32_t increment = o->phase_increment;
    uint32_t modulator_phase = o->state.vow.formant_phase[0];
    uint32_t modulator_phase_2 = o->state.vow.formant_phase[1];
    uint32_t modulator_phase_increment = pb_dig_compute_phase_increment(
        (int16_t)(o->pitch + ((o->parameter[0] - 16384) >> 2)));
    uint32_t modulator_phase_increment_2 = pb_dig_compute_phase_increment(
        (int16_t)(o->pitch + ((o->parameter[1] - 16384) >> 2)));

    while (size--) {
        phase += increment;
        if (*sync++) {
            phase = 0;
            modulator_phase = 0;
            modulator_phase_2 = 0;
        }
        modulator_phase += modulator_phase_increment;
        modulator_phase_2 += modulator_phase_increment_2;
        int16_t result = pb_interpolate824_s16(pb_wav_sine, phase);
        result = (int16_t)(result * pb_interpolate824_s16(pb_wav_sine, modulator_phase) >> 16);
        result = (int16_t)(result * pb_interpolate824_s16(pb_wav_sine, modulator_phase_2) >> 16);
        result = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(result + 32768));
        *buffer++ = result;
    }
    o->phase = phase - (1u << 30);
    o->state.vow.formant_phase[0] = modulator_phase;
    o->state.vow.formant_phase[1] = modulator_phase_2;
}

/* ------------------------------------------------------------ RenderSawSwarm --- */
static void pb_dig_render_saw_swarm(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int32_t detune = o->parameter[0] + 1024;
    detune = (detune * detune) >> 9;
    uint32_t increments[7];
    for (int16_t i = 0; i < 7; ++i) {
        int32_t saw_detune = detune * (i - 3);
        int32_t detune_integral = saw_detune >> 16;
        int32_t detune_fractional = saw_detune & 0xffff;
        int32_t increment_a = (int32_t)pb_dig_compute_phase_increment((int16_t)(o->pitch + detune_integral));
        int32_t increment_b = (int32_t)pb_dig_compute_phase_increment((int16_t)(o->pitch + detune_integral + 1));
        /* (increment_b - increment_a) * detune_fractional can pass 2^31 at high pitches: wrap it as the C++ does */
        increments[i] = (uint32_t)(increment_a +
            ((int32_t)((uint32_t)(increment_b - increment_a) * (uint32_t)detune_fractional) >> 16));
    }
    if (o->strike) {
        for (size_t i = 0; i < 6; ++i) {
            o->state.saw.phase[i] = pb_rng_word();
        }
        o->strike = 0;
    }
    int32_t hp_cutoff = o->pitch;
    if (o->parameter[1] < 10922) {
        hp_cutoff += ((o->parameter[1] - 10922) * 24) >> 5;
    } else {
        hp_cutoff += ((o->parameter[1] - 10922) * 12) >> 5;
    }
    if (hp_cutoff < 0) {
        hp_cutoff = 0;
    } else if (hp_cutoff > 32767) {
        hp_cutoff = 32767;
    }

    int32_t f = pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)hp_cutoff << 17);
    int32_t damp = pb_lut_svf_damp[0];
    int32_t bp = o->state.saw.bp;
    int32_t lp = o->state.saw.lp;

    while (size--) {
        if (*sync++) {
            for (size_t i = 0; i < 6; ++i) {
                o->state.saw.phase[i] = 0;
            }
        }
        int32_t notch, hp, sample;

        o->phase += increments[0];
        o->state.saw.phase[0] += increments[1];
        o->state.saw.phase[1] += increments[2];
        o->state.saw.phase[2] += increments[3];
        o->state.saw.phase[3] += increments[4];
        o->state.saw.phase[4] += increments[5];
        o->state.saw.phase[5] += increments[6];

        /* compute a sample */
        sample = -28672;
        sample += (int32_t)(o->phase >> 19);
        sample += (int32_t)(o->state.saw.phase[0] >> 19);
        sample += (int32_t)(o->state.saw.phase[1] >> 19);
        sample += (int32_t)(o->state.saw.phase[2] >> 19);
        sample += (int32_t)(o->state.saw.phase[3] >> 19);
        sample += (int32_t)(o->state.saw.phase[4] >> 19);
        sample += (int32_t)(o->state.saw.phase[5] >> 19);
        sample = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(sample + 32768));

        notch = sample - (bp * damp >> 15);
        lp += f * bp >> 15;
        PB_CLIP(lp);
        hp = notch - lp;
        bp += f * hp >> 15;

        int32_t result = hp;
        PB_CLIP(result);
        *buffer++ = (int16_t)result;
    }
    o->state.saw.lp = lp;
    o->state.saw.bp = bp;
}

/* ---------------------------------------------------------------- RenderComb --- */
/* kCombDelayLength is PB_COMB_DELAY_LENGTH (a power of two, so the % are masks) */
static void pb_dig_render_comb(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* filter the delay time to avoid clicks / glitches */
    int32_t pitch = o->pitch + ((o->parameter[0] - 16384) >> 1);
    int32_t filtered_pitch = o->state.ffm.previous_sample;
    filtered_pitch = (15 * filtered_pitch + pitch) >> 4;
    o->state.ffm.previous_sample = (int16_t)filtered_pitch;

    int16_t *dl = o->delay_lines->comb;
    uint32_t delay = pb_dig_compute_delay((int16_t)filtered_pitch);
    if (delay > (PB_COMB_DELAY_LENGTH << 16)) {
        delay = PB_COMB_DELAY_LENGTH << 16;
    }
    uint32_t delay_integral = delay >> 16;
    int32_t delay_fractional = (int32_t)(delay & 0xffff);

    /* warp the resonance curve to have a more precise adjustment in the extrema */
    int16_t resonance = (int16_t)((o->parameter[1] << 1) - 32768);
    resonance = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(resonance + 32768));

    uint32_t delay_ptr = o->phase;
    delay_ptr = delay_ptr % PB_COMB_DELAY_LENGTH;
    (void)sync;
    while (size--) {
        int32_t in = *buffer;
        uint32_t offset = delay_ptr + 2 * PB_COMB_DELAY_LENGTH - delay_integral;
        int32_t a = dl[offset % PB_COMB_DELAY_LENGTH];
        int32_t b = dl[(offset - 1) % PB_COMB_DELAY_LENGTH];
        int32_t delayed_sample = a + (((b - a) * (delay_fractional >> 1)) >> 15);
        int32_t feedback = (delayed_sample * resonance >> 15) + (in >> 1);
        PB_CLIP(feedback);
        dl[delay_ptr] = (int16_t)feedback;
        int32_t out = (in + (delayed_sample << 1)) >> 1;
        PB_CLIP(out);
        *buffer++ = (int16_t)out;
        delay_ptr = (delay_ptr + 1) % PB_COMB_DELAY_LENGTH;
    }
    o->phase = delay_ptr;
}

/* ----------------------------------------------------------------- RenderToy --- */
static void pb_dig_render_toy(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* 4 times oversampling (the C++ shifts the member itself, kept) */
    o->phase_increment >>= 2;

    uint32_t phase_increment = o->phase_increment;
    uint32_t phase = o->phase;

    uint16_t decimation_counter = o->state.toy.decimation_counter;
    uint16_t decimation_count = (uint16_t)(512 - (o->parameter[0] >> 6));

    uint8_t held_sample = o->state.toy.held_sample;
    while (size--) {
        int32_t filtered_sample = 0;
        if (*sync++) {
            phase = 0;
        }
        for (size_t tap = 0; tap < 4; ++tap) {
            phase += phase_increment;
            if (decimation_counter >= decimation_count) {
                uint8_t x = (uint8_t)(o->parameter[1] >> 8);
                held_sample = (uint8_t)((((phase >> 24) ^ (uint32_t)(x << 1)) & (uint32_t)(~x)) + (uint32_t)(x >> 1));
                decimation_counter = 0;
            }
            filtered_sample = (int32_t)((uint32_t)filtered_sample + pb_fir4_coefficients[tap] * held_sample);
            ++decimation_counter;
        }
        *buffer++ = (int16_t)((uint32_t)(filtered_sample >> 8) - PB_FIR4_DC_OFFSET);
    }
    o->state.toy.held_sample = held_sample;
    o->state.toy.decimation_counter = decimation_counter;
    o->phase = phase;
}

/* ------------------------------------------------------- RenderDigitalFilter --- */
static const uint32_t pbb_phase_reset[] = {                 /* kPhaseReset */
    0,
    0x80000000u,
    0x40000000u,
    0x80000000u
};

static void pb_dig_render_digital_filter(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int16_t shifted_pitch = (int16_t)(o->pitch + ((o->parameter[0] - 2048) >> 1));
    if (shifted_pitch > 16383) {
        shifted_pitch = 16383;
    }
    uint32_t modulator_phase = o->state.res.modulator_phase;
    uint32_t square_modulator_phase = o->state.res.square_modulator_phase;
    int32_t square_integrator = o->state.res.integrator;

    uint8_t filter_type = (uint8_t)(o->shape - PB_OSC_SHAPE_DIGITAL_FILTER_LP);

    uint32_t modulator_phase_increment = o->state.res.modulator_phase_increment;
    uint32_t target_increment = pb_dig_compute_phase_increment(shifted_pitch);
    uint32_t modulator_phase_increment_increment =
        modulator_phase_increment < target_increment
        ? (target_increment - modulator_phase_increment) / (uint32_t)size
        : ~((modulator_phase_increment - target_increment) / (uint32_t)size);

    while (size--) {
        o->phase += o->phase_increment;
        modulator_phase_increment += modulator_phase_increment_increment;
        modulator_phase += modulator_phase_increment;
        uint16_t integrator_gain = (uint16_t)(modulator_phase_increment >> 14);

        if (*sync++) {
            o->state.res.polarity = 1;
            o->phase = 0;
            modulator_phase = 0;
            square_modulator_phase = 0;
            square_integrator = 0;
        }

        square_modulator_phase += modulator_phase_increment;
        if (o->phase < o->phase_increment) {
            modulator_phase = pbb_phase_reset[filter_type];
        }
        if ((o->phase << 1) < (o->phase_increment << 1)) {
            o->state.res.polarity = !o->state.res.polarity;
            square_modulator_phase = pbb_phase_reset[(filter_type & 1) + 2];
        }

        int32_t carrier = pb_interpolate824_s16(pb_wav_sine, modulator_phase);
        int32_t square_carrier = pb_interpolate824_s16(pb_wav_sine, square_modulator_phase);

        uint16_t saw = (uint16_t)~(o->phase >> 16);
        uint16_t double_saw = (uint16_t)~(o->phase >> 15);
        uint16_t triangle = (uint16_t)((o->phase >> 15) ^ (o->phase & 0x80000000u ? 0xffffu : 0x0000u));
        uint16_t window = o->parameter[1] < 16384 ? saw : triangle;

        int32_t pulse = (square_carrier * double_saw) >> 16;
        if (o->state.res.polarity) {
            pulse = -pulse;
        }
        square_integrator += (pulse * integrator_gain) >> 16;
        PB_CLIP(square_integrator);

        int16_t saw_tri_signal;
        int16_t square_signal;

        if (filter_type & 2) {
            saw_tri_signal = (int16_t)((carrier * window) >> 16);
            square_signal = (int16_t)pulse;
        } else {
            /* window * (carrier + 32768) can pass 2^31: wrap it as the C++ does */
            saw_tri_signal = (int16_t)(((int32_t)((uint32_t)window * (uint32_t)(carrier + 32768)) >> 16) - 32768);
            square_signal = (int16_t)square_integrator;
            if (filter_type == 1) {
                square_signal = (int16_t)((pulse + square_integrator) >> 1);
            }
        }
        uint16_t balance = (uint16_t)((uint32_t)(o->parameter[1] < 16384 ? o->parameter[1] : ~o->parameter[1]) << 2);
        *buffer++ = pb_mix_s16(saw_tri_signal, square_signal, balance);
    }
    o->state.res.modulator_phase = modulator_phase;
    o->state.res.square_modulator_phase = square_modulator_phase;
    o->state.res.integrator = square_integrator;
    o->state.res.modulator_phase_increment = modulator_phase_increment;
}

/* --------------------------------------------------------------- RenderVosim --- */
static void pb_dig_render_vosim(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    for (size_t i = 0; i < 2; ++i) {
        o->state.vow.formant_increment[i] = pb_dig_compute_phase_increment((int16_t)(o->parameter[i] >> 1));
    }
    while (size--) {
        o->phase += o->phase_increment;
        if (*sync++) {
            o->phase = 0;
        }
        int32_t sample = 16384 + 8192;
        o->state.vow.formant_phase[0] += o->state.vow.formant_increment[0];
        sample += pb_interpolate824_s16(pb_wav_sine, o->state.vow.formant_phase[0]) >> 1;

        o->state.vow.formant_phase[1] += o->state.vow.formant_increment[1];
        sample += pb_interpolate824_s16(pb_wav_sine, o->state.vow.formant_phase[1]) >> 2;

        sample = sample * (pb_interpolate824_u16(pb_lut_bell, o->phase) >> 1) >> 15;
        if (o->phase < o->phase_increment) {
            o->state.vow.formant_phase[0] = 0;
            o->state.vow.formant_phase[1] = 0;
            sample = 0;
        }
        sample -= 16384 + 8192;
        *buffer++ = (int16_t)sample;
    }
}

/* --------------------------------------------------------------- RenderVowel --- */
typedef struct {                                            /* PhonemeDefinition */
    uint8_t formant_frequency[3];
    uint8_t formant_amplitude[3];
} pbb_phoneme_t;

static const pbb_phoneme_t pbb_vowels_data[9] = {           /* vowels_data */
    { { 27,  40,  89 }, { 15,  13,  1 } },
    { { 18,  51,  62 }, { 13,  12,  6 } },
    { { 15,  69,  93 }, { 14,  12,  7 } },
    { { 10,  84, 110 }, { 13,  10,  8 } },
    { { 23,  44,  87 }, { 15,  12,  1 } },
    { { 13,  29,  80 }, { 13,   8,  0 } },
    { {  6,  46,  81 }, { 12,   3,  0 } },
    { {  9,  51,  95 }, { 15,   3,  0 } },
    { {  6,  73,  99 }, {  7,   3,  14 } }
};

static const pbb_phoneme_t pbb_consonant_data[8] = {        /* consonant_data */
    { { 6, 54, 121 }, { 9,  9,  0 } },
    { { 18, 50, 51 }, { 12,  10,  5 } },
    { { 11, 24, 70 }, { 13,  8,  0 } },
    { { 15, 69, 74 }, { 14,  12,  7 } },
    { { 16, 37, 111 }, { 14,  8,  1 } },
    { { 18, 51, 62 }, { 14,  12,  6 } },
    { { 6, 26, 81 }, { 5,  5,  5 } },
    { { 6, 73, 99 }, { 7,  10,  14 } },
};

static void pb_dig_render_vowel(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    size_t vowel_index = (size_t)(o->parameter[0] >> 12);
    uint16_t balance = (uint16_t)(o->parameter[0] & 0x0fff);
    uint16_t formant_shift = (uint16_t)(200 + (o->parameter[1] >> 6));
    (void)sync;
    if (o->strike) {
        o->strike = 0;
        o->state.vow.consonant_frames = 160;
        uint16_t index = (uint16_t)((pb_rng_sample() + 1) & 7);
        for (size_t i = 0; i < 3; ++i) {
            o->state.vow.formant_increment[i] =
                (uint32_t)pbb_consonant_data[index].formant_frequency[i] * 0x1000 * formant_shift;
            o->state.vow.formant_amplitude[i] = pbb_consonant_data[index].formant_amplitude[i];
        }
        o->state.vow.noise = index >= 6 ? 4095 : 0;
    }

    if (o->state.vow.consonant_frames) {
        --o->state.vow.consonant_frames;
    } else {
        for (size_t i = 0; i < 3; ++i) {
            o->state.vow.formant_increment[i] = (uint32_t)(
                (pbb_vowels_data[vowel_index].formant_frequency[i] * (0x1000 - balance) +
                 pbb_vowels_data[vowel_index + 1].formant_frequency[i] * balance) *
                formant_shift);
            o->state.vow.formant_amplitude[i] = (uint32_t)(
                (pbb_vowels_data[vowel_index].formant_amplitude[i] * (0x1000 - balance) +
                 pbb_vowels_data[vowel_index + 1].formant_amplitude[i] * balance) >> 12);
        }
        o->state.vow.noise = 0;
    }
    int32_t noise = o->state.vow.noise;

    while (size--) {
        o->phase += o->phase_increment;
        size_t phaselet;
        int16_t sample = 0;
        o->state.vow.formant_phase[0] += o->state.vow.formant_increment[0];
        phaselet = (o->state.vow.formant_phase[0] >> 24) & 0xf0;
        sample = (int16_t)(sample + pb_wav_formant_sine[phaselet | o->state.vow.formant_amplitude[0]]);

        o->state.vow.formant_phase[1] += o->state.vow.formant_increment[1];
        phaselet = (o->state.vow.formant_phase[1] >> 24) & 0xf0;
        sample = (int16_t)(sample + pb_wav_formant_sine[phaselet | o->state.vow.formant_amplitude[1]]);

        o->state.vow.formant_phase[2] += o->state.vow.formant_increment[2];
        phaselet = (o->state.vow.formant_phase[2] >> 24) & 0xf0;
        sample = (int16_t)(sample + pb_wav_formant_square[phaselet | o->state.vow.formant_amplitude[2]]);

        /* (sample *= a uint32_t: unsigned multiply, truncated to 16 bits) */
        sample = (int16_t)((uint32_t)sample * (255 - (o->phase >> 24)));
        int32_t phase_noise = pb_rng_sample() * noise;
        if ((o->phase + (uint32_t)phase_noise) < o->phase_increment) {
            o->state.vow.formant_phase[0] = 0;
            o->state.vow.formant_phase[1] = 0;
            o->state.vow.formant_phase[2] = 0;
            sample = 0;
        }
        sample = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(sample + 32768));
        *buffer++ = sample;
    }
}

/* ------------------------------------------------------------ RenderVowelFof --- */
static const int16_t pbb_formant_f_data[PB_NUM_FORMANTS][PB_NUM_FORMANTS][PB_NUM_FORMANTS] = {   /* formant_f_data */
    /* bass */
    {
        { 9519, 10738, 12448, 12636, 12892 },   /* a */
        { 8620, 11720, 12591, 12932, 13158 },   /* e */
        { 7579, 11891, 12768, 13122, 13323 },   /* i */
        { 8620, 10013, 12591, 12768, 13010 },   /* o */
        { 8324, 9519, 12591, 12831, 13048 }     /* u */
    },
    /* tenor */
    {
        { 9696, 10821, 12810, 13010, 13263 },
        { 8620, 11827, 12768, 13228, 13477 },
        { 7908, 12038, 12932, 13263, 13452 },
        { 8620, 10156, 12768, 12932, 13085 },
        { 8324, 9519, 12852, 13010, 13296 }
    },
    /* countertenor */
    {
        { 9730, 10902, 12892, 13085, 13330 },
        { 8832, 11953, 12852, 13085, 13296 },
        { 7749, 12014, 13010, 13330, 13483 },
        { 8781, 10211, 12852, 13085, 13296 },
        { 8448, 9627, 12892, 13085, 13363 }
    },
    /* alto */
    {
        { 10156, 10960, 12932, 13427, 14195 },
        { 8620, 11692, 12852, 13296, 14195 },
        { 8324, 11827, 12852, 13550, 14195 },
        { 8881, 10156, 12956, 13427, 14195 },
        { 8160, 9860, 12708, 13427, 14195 }
    },
    /* soprano */
    {
        { 10156, 10960, 13010, 13667, 14195 },
        { 8324, 12187, 12932, 13489, 14195 },
        { 7749, 12337, 13048, 13667, 14195 },
        { 8881, 10156, 12956, 13609, 14195 },
        { 8160, 9860, 12852, 13609, 14195 }
    }
};

static const int16_t pbb_formant_a_data[PB_NUM_FORMANTS][PB_NUM_FORMANTS][PB_NUM_FORMANTS] = {   /* formant_a_data */
    /* bass */
    {
        { 16384, 7318, 5813, 5813, 1638 },      /* a */
        { 16384, 4115, 5813, 4115, 2062 },      /* e */
        { 16384, 518, 2596, 1301, 652 },        /* i */
        { 16384, 4617, 1460, 1638, 163 },       /* o */
        { 16384, 1638, 411, 652, 259 }          /* u */
    },
    /* tenor */
    {
        { 16384, 8211, 7318, 6522, 1301 },
        { 16384, 3269, 4115, 3269, 1638 },
        { 16384, 2913, 2062, 1638, 518 },
        { 16384, 5181, 4115, 4115, 821 },
        { 16384, 1638, 2314, 3269, 821 }
    },
    /* countertenor */
    {
        { 16384, 8211, 1159, 1033, 206 },
        { 16384, 3269, 2062, 1638, 1638 },
        { 16384, 1033, 1033, 259, 259 },
        { 16384, 5181, 821, 1301, 326 },
        { 16384, 1638, 1159, 518, 326 }
    },
    /* alto */
    {
        { 16384, 10337, 1638, 259, 16 },
        { 16384, 1033, 518, 291, 16 },
        { 16384, 1638, 518, 259, 16 },
        { 16384, 5813, 2596, 652, 29 },
        { 16384, 4115, 518, 163, 10 }
    },
    /* soprano */
    {
        { 16384, 8211, 411, 1638, 51 },
        { 16384, 1638, 2913, 163, 25 },
        { 16384, 4115, 821, 821, 103 },
        { 16384, 4617, 1301, 1301, 51 },
        { 16384, 2596, 291, 163, 16 }
    }
};

/* InterpolateFormantParameter */
static inline int16_t pbb_interpolate_formant_parameter(const int16_t table[][PB_NUM_FORMANTS][PB_NUM_FORMANTS],
                                                        int16_t x, int16_t y, uint8_t formant)
{
    uint16_t x_index = (uint16_t)(x >> 13);
    uint16_t x_mix = (uint16_t)(x << 3);
    uint16_t y_index = (uint16_t)(y >> 13);
    uint16_t y_mix = (uint16_t)(y << 3);
    int16_t a = table[x_index][y_index][formant];
    int16_t b = table[x_index + 1][y_index][formant];
    int16_t c = table[x_index][y_index + 1][formant];
    int16_t d = table[x_index + 1][y_index + 1][formant];
    a = (int16_t)(a + ((b - a) * x_mix >> 16));
    c = (int16_t)(c + ((d - c) * x_mix >> 16));
    return (int16_t)(a + ((c - a) * y_mix >> 16));
}

static void pb_dig_render_vowel_fof(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* The original implementation used FOF, but a bank of 5 SVF is cheaper (the C++'s own note). */
    int16_t amplitudes[PB_NUM_FORMANTS];
    int32_t svf_lp[PB_NUM_FORMANTS];
    int32_t svf_bp[PB_NUM_FORMANTS];
    int16_t svf_f[PB_NUM_FORMANTS];
    (void)sync;

    for (size_t i = 0; i < PB_NUM_FORMANTS; ++i) {
        int32_t frequency = pbb_interpolate_formant_parameter(
            pbb_formant_f_data, o->parameter[1], o->parameter[0], (uint8_t)i) + (12 << 7);
        svf_f[i] = (int16_t)pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)frequency << 17);
        amplitudes[i] = pbb_interpolate_formant_parameter(
            pbb_formant_a_data, o->parameter[1], o->parameter[0], (uint8_t)i);
        if (o->init) {
            svf_lp[i] = 0;
            svf_bp[i] = 0;
        } else {
            svf_lp[i] = o->state.fof.svf_lp[i];
            svf_bp[i] = o->state.fof.svf_bp[i];
        }
    }

    if (o->init) {
        o->init = 0;
    }

    uint32_t phase = o->phase;
    int32_t previous_sample = o->state.fof.previous_sample;
    int32_t next_saw_sample = o->state.fof.next_saw_sample;
    uint32_t increment = o->phase_increment << 1;
    while (size) {
        int32_t this_saw_sample = next_saw_sample;
        next_saw_sample = 0;
        phase += increment;
        if (phase < increment) {
            uint32_t t = phase / (increment >> 16);
            if (t > 65535) {
                t = 65535;
            }
            this_saw_sample -= (int32_t)(t * t >> 18);
            t = 65535 - t;
            next_saw_sample -= -(int32_t)(t * t >> 18);
        }
        next_saw_sample += (int32_t)(phase >> 17);
        int32_t in = this_saw_sample;
        int32_t out = 0;
        for (int32_t i = 0; i < 5; ++i) {
            int32_t notch = in - (svf_bp[i] >> 6);
            svf_lp[i] += svf_f[i] * svf_bp[i] >> 15;
            PB_CLIP(svf_lp[i]);
            int32_t hp = notch - svf_lp[i];
            svf_bp[i] += svf_f[i] * hp >> 15;
            PB_CLIP(svf_bp[i]);
            out += svf_bp[i] * amplitudes[0] >> 17;     /* (amplitudes[0] for every formant: the C++'s, kept) */
        }
        PB_CLIP(out);
        *buffer++ = (int16_t)((out + previous_sample) >> 1);
        *buffer++ = (int16_t)out;
        previous_sample = out;
        size -= 2;
    }
    o->phase = phase;
    o->state.fof.next_saw_sample = next_saw_sample;
    o->state.fof.previous_sample = (int16_t)previous_sample;
    for (size_t i = 0; i < PB_NUM_FORMANTS; ++i) {
        o->state.fof.svf_lp[i] = svf_lp[i];
        o->state.fof.svf_bp[i] = svf_bp[i];
    }
}

/* ----------------------------------------------------------- RenderHarmonics --- */
static void pb_dig_render_harmonics(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint32_t phase = o->phase;
    int16_t previous_sample = o->state.add.previous_sample;
    uint32_t phase_increment = o->phase_increment << 1;
    int32_t target_amplitude[PB_NUM_ADDITIVE_HARMONICS];
    int32_t amplitude[PB_NUM_ADDITIVE_HARMONICS];

    /* (kNumAdditiveHarmonics is a size_t: unsigned arithmetic, 32-bit as on Braids' ARM; see the header) */
    int32_t peak = (int32_t)((PB_NUM_ADDITIVE_HARMONICS * (uint32_t)o->parameter[0]) >> 7);
    int32_t second_peak = (int32_t)((uint32_t)(peak >> 1) + PB_NUM_ADDITIVE_HARMONICS * 128);
    int32_t second_peak_amount = o->parameter[1] * o->parameter[1] >> 15;

    int32_t sqrtsqrt_width = o->parameter[1] < 16384
        ? o->parameter[1] >> 6 : 511 - (o->parameter[1] >> 6);
    int32_t sqrt_width = sqrtsqrt_width * sqrtsqrt_width >> 10;
    int32_t width = sqrt_width * sqrt_width + 4;
    int32_t total = 0;
    for (size_t i = 0; i < PB_NUM_ADDITIVE_HARMONICS; ++i) {
        int32_t x = (int32_t)(i << 8);
        int32_t d, g;

        d = (x - peak);
        g = 32768 * 128 / (128 + d * d / width);

        d = (x - second_peak);
        g += second_peak_amount * 128 / (128 + d * d / width);
        total += g;
        target_amplitude[i] = g;
    }

    int32_t attenuation = 2147483647 / total;
    for (size_t i = 0; i < PB_NUM_ADDITIVE_HARMONICS; ++i) {
        if ((phase_increment >> 16) * (uint32_t)(i + 1) > 0x4000) {
            target_amplitude[i] = 0;
        } else {
            target_amplitude[i] = target_amplitude[i] * attenuation >> 16;
        }
        amplitude[i] = o->state.hrm.amplitude[i];
    }

    while (size) {
        int32_t out;

        phase += phase_increment;
        if (*sync++ || *sync++) {                       /* (the C++'s short circuit: one or two sync bytes) */
            phase = 0;
        }
        out = 0;
        for (size_t i = 0; i < PB_NUM_ADDITIVE_HARMONICS; ++i) {
            out += pb_interpolate824_s16(pb_wav_sine, phase * (uint32_t)(i + 1)) * amplitude[i] >> 15;
            amplitude[i] += (target_amplitude[i] - amplitude[i]) >> 8;
        }
        PB_CLIP(out);
        *buffer++ = (int16_t)((out + previous_sample) >> 1);
        *buffer++ = (int16_t)out;
        previous_sample = (int16_t)out;
        size -= 2;
    }
    o->state.add.previous_sample = previous_sample;
    o->phase = phase;
    for (size_t i = 0; i < PB_NUM_ADDITIVE_HARMONICS; ++i) {
        o->state.hrm.amplitude[i] = amplitude[i];
    }
}

/* ------------------------------------------------------------------ RenderFm --- */
static void pb_dig_render_fm(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint32_t modulator_phase = o->state.modulator_phase;
    uint32_t modulator_phase_increment = pb_dig_compute_phase_increment(
        (int16_t)((12 << 7) + o->pitch + ((o->parameter[1] - 16384) >> 1))) >> 1;

    PB_BEGIN_INTERPOLATE_PARAMETER_0(o)

    while (size--) {
        PB_INTERPOLATE_PARAMETER_0

        o->phase += o->phase_increment;
        if (*sync++) {
            o->phase = modulator_phase = 0;
        }
        modulator_phase += modulator_phase_increment;

        uint32_t pm = (uint32_t)(pb_interpolate824_s16(pb_wav_sine, modulator_phase) * parameter_0) << 2;
        *buffer++ = pb_interpolate824_s16(pb_wav_sine, o->phase + pm);
    }

    PB_END_INTERPOLATE_PARAMETER_0(o)

    o->state.modulator_phase = modulator_phase;
}

/* ---------------------------------------------------------- RenderFeedbackFm --- */
static void pb_dig_render_feedback_fm(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int16_t previous_sample = o->state.ffm.previous_sample;
    uint32_t modulator_phase = o->state.ffm.modulator_phase;

    int32_t attenuation = o->pitch - (72 << 7) + ((o->parameter[1] - 16384) >> 1);
    attenuation = 32767 - attenuation * 4;
    if (attenuation < 0) attenuation = 0;
    if (attenuation > 32767) attenuation = 32767;

    uint32_t modulator_phase_increment = pb_dig_compute_phase_increment(
        (int16_t)((12 << 7) + o->pitch + ((o->parameter[1] - 16384) >> 1))) >> 1;

    PB_BEGIN_INTERPOLATE_PARAMETER_0(o)

    while (size--) {
        PB_INTERPOLATE_PARAMETER_0

        o->phase += o->phase_increment;
        if (*sync++) {
            o->phase = modulator_phase = 0;
        }

        modulator_phase += modulator_phase_increment;

        int32_t pm;
        int32_t p = parameter_0 * attenuation >> 15;
        pm = (int32_t)((uint32_t)previous_sample << 14);
        pm = (int32_t)((uint32_t)(pb_interpolate824_s16(pb_wav_sine, modulator_phase + (uint32_t)pm) * p) << 1);
        previous_sample = pb_interpolate824_s16(pb_wav_sine, o->phase + (uint32_t)pm);
        *buffer++ = previous_sample;
    }

    PB_END_INTERPOLATE_PARAMETER_0(o)

    o->state.ffm.previous_sample = previous_sample;
    o->state.ffm.modulator_phase = modulator_phase;
}

/* --------------------------------------------------- RenderChaoticFeedbackFm --- */
static void pb_dig_render_chaotic_feedback_fm(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint32_t modulator_phase_increment = pb_dig_compute_phase_increment(
        (int16_t)((12 << 7) + o->pitch + ((o->parameter[1] - 16384) >> 1))) >> 1;
    int16_t previous_sample = o->state.ffm.previous_sample;
    uint32_t modulator_phase = o->state.ffm.modulator_phase;

    PB_BEGIN_INTERPOLATE_PARAMETER_0(o)

    while (size--) {
        PB_INTERPOLATE_PARAMETER_0

        o->phase += o->phase_increment;
        if (*sync++) {
            o->phase = modulator_phase = 0;
        }

        int32_t pm;
        pm = (int32_t)((uint32_t)(pb_interpolate824_s16(pb_wav_sine, modulator_phase) * parameter_0) << 1);
        previous_sample = pb_interpolate824_s16(pb_wav_sine, o->phase + (uint32_t)pm);
        *buffer++ = previous_sample;
        modulator_phase += (modulator_phase_increment >> 8) * (uint32_t)(129 + (previous_sample >> 9));
    }

    PB_END_INTERPOLATE_PARAMETER_0(o)

    o->state.ffm.previous_sample = previous_sample;
    o->state.ffm.modulator_phase = modulator_phase;
}
