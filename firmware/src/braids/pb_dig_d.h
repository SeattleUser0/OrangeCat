/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the last group of Braids' DigitalOscillator render functions (braids/digital_oscillator.cc), in
 * C. Included by pb_digital.h after the state and helpers; pb_dig_render dispatches here.
 *
 *   RenderWavetables          pb_dig_render_wavetables          (with wavetable_definitions: pbd_wavetable_defs)
 *   RenderWaveMap             pb_dig_render_wave_map
 *   RenderWaveLine            pb_dig_render_wave_line           (with wave_line: pbd_wave_line)
 *   RenderWaveParaphonic      pb_dig_render_wave_paraphonic     (with mini_wave_line and chords: pbd_mini_wave_line,
 *                                                                pbd_chords)
 *   RenderFilteredNoise       pb_dig_render_filtered_noise
 *   RenderTwinPeaksNoise      pb_dig_render_twin_peaks_noise
 *   RenderClockedNoise        pb_dig_render_clocked_noise
 *   RenderGranularCloud       pb_dig_render_granular_cloud
 *   RenderParticleNoise       pb_dig_render_particle_noise      (kParticleNoiseDecay, kResonanceSquared,
 *                                                                kResonanceFactor: PBD_*)
 *   RenderDigitalModulation   pb_dig_render_digital_modulation  (kConstellationQ, kConstellationI: pbd_constellation_*)
 *   RenderQuestionMark        pb_dig_render_question_mark       (reads resources.cc's wt_code: PBD_WT_CODE)
 *
 * The four wavetable shapes, and the tables only they use, are inside #if PB_WAVETABLES: they need pb_wt_waves
 * (33 KB), which the device build leaves out unless flash allows (pb_dsp.h). The parity build always has them.
 *
 * Differences from the C++, and why:
 *   - RenderWaveLine reads wave_line[(scan >> 10) + 1], which is wave_line[64] (one past the end of the 64 entries)
 *     when scan >= 64512 (TIMBRE near the top). The C++ then reads whatever byte the compiler put after the array:
 *     in the reference build (g++ -O2, and GCC's usual reverse order of statics) that is wavetable_definitions[0]
 *     .num_steps, 16. pbd_wave_line has that 16 as a 65th entry, so the port plays wave 16 there, as the C++ does.
 *   - Where the C++ shifts a signed value left past bit 31 (pitch_ << 17, parameter_[0] << 17 in
 *     RenderFilteredNoise) or multiplies two uint16_t past INT_MAX (amplitude * kParticleNoiseDecay), the port does
 *     the same arithmetic in uint32_t, which gives the same bits without the undefined behavior.
 *   - C++ bool strike_ is the int o->strike; Random::GetWord / GetSample are pb_rng_word / pb_rng_sample, drawn in
 *     the C++'s order.
 *
 * Revisions:
 *   0.5.1: first version.
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

#define PB_DIG_D 1

#if PB_WAVETABLES
/* ------------------------------------------------------- the wavetable shapes --- */

/* struct WavetableDefinition, wavetable_definitions[] */
typedef struct {
    uint8_t num_steps;
    uint8_t wave_index[17];
} pbd_wavetable_def_t;

static const pbd_wavetable_def_t pbd_wavetable_defs[] = {
    /* 01 male */
    { 16, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 15 } },
    /* 02 female */
    { 16, { 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 31 } },
    /* 03 choir */
    { 16, { 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 47 } },
    /* 04 space_voice */
    { 16, { 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 63 } },
    /* 05 tampura */
    { 16, { 64, 65, 66, 67, 68, 68, 69, 70, 71, 72, 73, 73, 74, 75, 75, 76, 76 } },
    /* 06 shamus */
    { 16, { 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 92 } },
    /* 07 swept_string */
    { 16, { 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108 } },
    /* 08 bowed */
    { 16, { 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 124 } },
    /* 09 cello */
    { 16, { 125, 126, 127, 128, 129, 130, 131, 132, 132, 132, 132, 132, 132, 132, 132, 132, 132 } },
    /* 10 vibes */
    { 16, { 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 144, 144, 145, 145, 145 } },
    /* 11 slap */
    { 16, { 146, 147, 148, 149, 150, 151, 151, 151, 152, 152, 152, 152, 153, 153, 153, 153, 153 } },
    /* 12 piano */
    { 8, { 154, 154, 154, 154, 154, 154, 155, 156, 156 } },
    /* 13 organ! */
    { 16, { 176, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 171 } },
    /* 14 waves! */
    { 16, { 172, 173, 174, 175, 176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 187 } },
    /* 15 digital */
    { 16, { 176, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 202 } },
    /* 16 drone 1 */
    { 16, { 203, 205, 204, 205, 212, 206, 207, 208, 208, 209, 210, 210, 211, 211, 212, 212, 212 } },
    /* 17 drone 2 */
    { 8, { 213, 213, 213, 214, 215, 216, 217, 218, 219 } },
    /* 18 metallic */
    { 16, { 220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 235 } },
    /* 19 fantasy */
    { 16, { 236, 237, 238, 239, 240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 251 } },
    /* 20 bell */
    { 4, { 252, 253, 254, 255, 254 } },
};

/* wave_line[]: 64 entries as in the C++, plus a 65th (16) for the read one past the end (see the header) */
static const uint8_t pbd_wave_line[65] = {
    187, 179, 154, 155, 135, 134, 137, 19, 24, 3, 8, 66, 79, 25, 180, 174, 64,
    127, 198, 15, 10, 7, 11, 0, 191, 192, 115, 238, 237, 236, 241, 47, 70, 76,
    235, 26, 133, 208, 34, 175, 183, 146, 147, 148, 150, 151, 152, 153, 117,
    138, 32, 33, 35, 125, 199, 201, 30, 31, 193, 27, 29, 21, 18, 182,
    16
};

static const uint8_t pbd_mini_wave_line[33] = {
    157, 161, 171, 188, 189, 191, 192, 193, 196, 198, 201, 234, 232,
    229, 226, 224, 1, 2, 3, 4, 5, 8, 12, 32, 36, 42, 47, 252, 254, 141, 139,
    135, 174
};

/* chords[17][3] (the C++ writes n SEMI for n * 128) */
static const uint16_t pbd_chords[17][3] = {
    { 2, 4, 6 },
    { 16, 32, 48 },
    { 2 * 128, 7 * 128, 12 * 128 },
    { 3 * 128, 7 * 128, 10 * 128 },
    { 3 * 128, 7 * 128, 12 * 128 },
    { 3 * 128, 7 * 128, 14 * 128 },
    { 3 * 128, 7 * 128, 17 * 128 },
    { 7 * 128, 12 * 128, 19 * 128 },
    { 7 * 128, 3 + 12 * 128, 5 + 19 * 128 },
    { 4 * 128, 7 * 128, 17 * 128 },
    { 4 * 128, 7 * 128, 14 * 128 },
    { 4 * 128, 7 * 128, 12 * 128 },
    { 4 * 128, 7 * 128, 11 * 128 },
    { 5 * 128, 7 * 128, 12 * 128 },
    { 4, 7 * 128, 12 * 128 },
    { 4, 4 + 12 * 128, 12 * 128 },
    { 4, 4 + 12 * 128, 12 * 128 },
};

/* RenderWavetables: 20 tables of up to 16 waves; COLOR picks the table (with hysteresis), TIMBRE scans it */
static void pb_dig_render_wavetables(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* hysteresis on the second parameter, so one DAC bit cannot flip the table */
    if ((o->parameter[1] > o->previous_parameter[1] + 64) || (o->parameter[1] < o->previous_parameter[1] - 64)) {
        o->previous_parameter[1] = o->parameter[1];
    }
    uint32_t wavetable_index = (uint32_t)o->previous_parameter[1] * 20;
    wavetable_index >>= 15;

    uint32_t wave_pointer;
    const uint8_t *wave[2];
    const pbd_wavetable_def_t *wt = &pbd_wavetable_defs[wavetable_index];

    wave_pointer = (uint32_t)((o->parameter[0] << 1) * wt->num_steps);
    for (size_t i = 0; i < 2; ++i) {
        size_t wave_index = wt->wave_index[(wave_pointer >> 16) + i];
        wave[i] = pb_wt_waves + wave_index * 129;
    }

    uint32_t phase_increment = o->phase_increment >> 1;
    while (size--) {
        int16_t sample;
        /* 2x naive oversampling */
        o->phase += phase_increment;
        if (*sync++) {
            o->phase = 0;
        }
        sample = (int16_t)(pb_crossfade_u8(wave[0], wave[1], o->phase >> 1, (uint16_t)wave_pointer) >> 1);
        o->phase += phase_increment;
        sample = (int16_t)(sample + (pb_crossfade_u8(wave[0], wave[1], o->phase >> 1, (uint16_t)wave_pointer) >> 1));
        *buffer++ = sample;
    }
}

/* RenderWaveMap: a 16 x 16 grid of waves (wt_map), TIMBRE and COLOR are the two coordinates */
static void pb_dig_render_wave_map(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* the grid is 16 x 16, so there are 15 interpolation squares */
    uint16_t p[2];
    uint16_t wave_xfade[2];
    uint16_t wave_coordinate[2];

    p[0] = (uint16_t)(o->parameter[0] * 15 >> 4);
    p[1] = (uint16_t)(o->parameter[1] * 15 >> 4);
    wave_xfade[0] = (uint16_t)(p[0] << 5);
    wave_xfade[1] = (uint16_t)(p[1] << 5);
    wave_coordinate[0] = p[0] >> 11;
    wave_coordinate[1] = p[1] >> 11;

    const uint8_t *wave[2][2];
    for (size_t i = 0; i < 2; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            uint16_t wave_index = (uint16_t)((wave_coordinate[0] + i) * 16 + (wave_coordinate[1] + j));
            wave[i][j] = pb_wt_waves + pb_wt_map[wave_index] * 129;
        }
    }

    uint32_t phase_increment = o->phase_increment >> 1;
    while (size--) {
        int16_t sample;
        /* 2x naive oversampling */
        o->phase += phase_increment;
        if (*sync++) {
            o->phase = 0;
        }
        sample = (int16_t)(pb_mix_s16(
            pb_crossfade_u8(wave[0][0], wave[0][1], o->phase >> 1, wave_xfade[1]),
            pb_crossfade_u8(wave[1][0], wave[1][1], o->phase >> 1, wave_xfade[1]),
            wave_xfade[0]) >> 1);
        o->phase += phase_increment;
        sample = (int16_t)(sample + (pb_mix_s16(
            pb_crossfade_u8(wave[0][0], wave[0][1], o->phase >> 1, wave_xfade[1]),
            pb_crossfade_u8(wave[1][0], wave[1][1], o->phase >> 1, wave_xfade[1]),
            wave_xfade[0]) >> 1));
        *buffer++ = sample;
    }
}

/* RenderWaveLine: TIMBRE scans a line of 64 waves; COLOR goes from stepped scanning to smooth, then to a
 * bit-crushed wave. Mix() is the int16_t overload, so its balance (parameter_[1] << 3) is cut to 16 bits. */
static void pb_dig_render_wave_line(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    o->smoothed_parameter = (3 * o->smoothed_parameter + (o->parameter[0] << 1)) >> 2;

    uint16_t scan = (uint16_t)o->smoothed_parameter;
    const uint8_t *wave_0 = pb_wt_waves + pbd_wave_line[o->previous_parameter[0] >> 9] * 129;
    const uint8_t *wave_1 = pb_wt_waves + pbd_wave_line[scan >> 10] * 129;
    const uint8_t *wave_2 = pb_wt_waves + pbd_wave_line[(scan >> 10) + 1] * 129;

    uint16_t smooth_xfade = (uint16_t)(scan << 6);
    uint16_t rough_xfade = 0;
    uint16_t rough_xfade_increment = (uint16_t)(32768 / size);
    uint32_t balance = (uint32_t)(o->parameter[1] << 3);
    uint16_t balance_16 = (uint16_t)balance;            /* Mix(int16_t, int16_t, uint16_t balance) */

    uint32_t phase = o->phase;
    uint32_t phase_increment = o->phase_increment >> 1;

    int16_t rough, smooth;

    if (o->parameter[1] < 8192) {
        while (size--) {
            if (*sync++) {
                phase = 0;
            }
            int32_t sample = 0;

            rough = pb_crossfade_u8(wave_0, wave_1, (phase >> 1) & 0xfe000000u, rough_xfade);
            smooth = pb_crossfade_u8(wave_0, wave_1, phase >> 1, rough_xfade);
            sample += pb_mix_s16(rough, smooth, balance_16);
            phase += phase_increment;
            rough_xfade = (uint16_t)(rough_xfade + rough_xfade_increment);

            rough = pb_crossfade_u8(wave_0, wave_1, (phase >> 1) & 0xfe000000u, rough_xfade);
            smooth = pb_crossfade_u8(wave_0, wave_1, phase >> 1, rough_xfade);
            sample += pb_mix_s16(rough, smooth, balance_16);
            phase += phase_increment;
            rough_xfade = (uint16_t)(rough_xfade + rough_xfade_increment);

            *buffer++ = (int16_t)(sample >> 1);
        }
    } else if (o->parameter[1] < 16384) {
        while (size--) {
            if (*sync++) {
                phase = 0;
            }
            int32_t sample = 0;

            rough = pb_crossfade_u8(wave_0, wave_1, phase >> 1, rough_xfade);
            smooth = pb_crossfade_u8(wave_1, wave_2, phase >> 1, smooth_xfade);
            sample += pb_mix_s16(rough, smooth, balance_16);
            phase += phase_increment;
            rough_xfade = (uint16_t)(rough_xfade + rough_xfade_increment);

            rough = pb_crossfade_u8(wave_0, wave_1, phase >> 1, rough_xfade);
            smooth = pb_crossfade_u8(wave_1, wave_2, phase >> 1, smooth_xfade);
            sample += pb_mix_s16(rough, smooth, balance_16);
            phase += phase_increment;
            rough_xfade = (uint16_t)(rough_xfade + rough_xfade_increment);

            *buffer++ = (int16_t)(sample >> 1);
        }
    } else if (o->parameter[1] < 24576) {
        while (size--) {
            if (*sync++) {
                phase = 0;
            }
            int32_t sample = 0;

            smooth = pb_crossfade_u8(wave_1, wave_2, phase >> 1, smooth_xfade);
            rough = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xfe000000u, smooth_xfade);
            sample += pb_mix_s16(smooth, rough, balance_16);
            phase += phase_increment;

            smooth = pb_crossfade_u8(wave_1, wave_2, phase >> 1, smooth_xfade);
            rough = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xfe000000u, smooth_xfade);
            sample += pb_mix_s16(smooth, rough, balance_16);
            phase += phase_increment;

            *buffer++ = (int16_t)(sample >> 1);
        }
    } else {
        while (size--) {
            if (*sync++) {
                phase = 0;
            }
            int32_t sample = 0;
            smooth = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xfe000000u, smooth_xfade);
            rough = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xf8000000u, smooth_xfade);
            sample += pb_mix_s16(smooth, rough, balance_16);
            phase += phase_increment;

            smooth = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xfe000000u, smooth_xfade);
            rough = pb_crossfade_u8(wave_1, wave_2, (phase >> 1) & 0xf8000000u, smooth_xfade);
            sample += pb_mix_s16(smooth, rough, balance_16);
            phase += phase_increment;

            *buffer++ = (int16_t)(sample >> 1);
        }
    }
    o->phase = phase;
    o->previous_parameter[0] = (int16_t)(o->smoothed_parameter >> 1);
}

/* RenderWaveParaphonic: four voices (the note and a chord picked by COLOR) on a wave picked by TIMBRE.
 * Renders two samples per pass, like the C++ (size is even). */
static void pb_dig_render_wave_paraphonic(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    if (o->strike) {
        for (size_t i = 0; i < 4; ++i) {
            o->state.saw.phase[i] = pb_rng_word();
        }
        o->strike = 0;
    }

    uint32_t phase_0, phase_1, phase_2, phase_3;
    uint32_t phase_increment[3];
    uint32_t phase_increment_0;

    phase_increment_0 = o->phase_increment;
    phase_0 = o->state.saw.phase[0];
    phase_1 = o->state.saw.phase[1];
    phase_2 = o->state.saw.phase[2];
    phase_3 = o->state.saw.phase[3];

    uint16_t chord_integral = (uint16_t)(o->parameter[1] >> 11);
    uint16_t chord_fractional = (uint16_t)(o->parameter[1] << 5);
    if (chord_fractional < 30720) {
        chord_fractional = 0;
    } else if (chord_fractional >= 34816) {
        chord_fractional = 65535;
    } else {
        chord_fractional = (uint16_t)((chord_fractional - 30720) * 16);
    }

    for (size_t i = 0; i < 3; ++i) {
        uint16_t detune_1 = pbd_chords[chord_integral][i];
        uint16_t detune_2 = pbd_chords[chord_integral + 1][i];
        uint16_t detune = (uint16_t)(detune_1 + ((detune_2 - detune_1) * chord_fractional >> 16));
        phase_increment[i] = pb_dig_compute_phase_increment((int16_t)(o->pitch + detune));
    }

    const uint8_t *wave_1 = pb_wt_waves + pbd_mini_wave_line[o->parameter[0] >> 10] * 129;
    const uint8_t *wave_2 = pb_wt_waves + pbd_mini_wave_line[(o->parameter[0] >> 10) + 1] * 129;
    uint16_t wave_xfade = (uint16_t)(o->parameter[0] << 6);

    (void)sync;
    while (size) {
        int32_t sample = 0;

        phase_0 += phase_increment_0;
        phase_1 += phase_increment[0];
        phase_2 += phase_increment[1];
        phase_3 += phase_increment[2];

        sample += pb_crossfade_u8(wave_1, wave_2, phase_0 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_1 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_2 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_3 >> 1, wave_xfade);
        *buffer++ = (int16_t)(sample >> 2);

        phase_0 += phase_increment_0;
        phase_1 += phase_increment[0];
        phase_2 += phase_increment[1];
        phase_3 += phase_increment[2];

        sample = 0;
        sample += pb_crossfade_u8(wave_1, wave_2, phase_0 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_1 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_2 >> 1, wave_xfade);
        sample += pb_crossfade_u8(wave_1, wave_2, phase_3 >> 1, wave_xfade);
        *buffer++ = (int16_t)(sample >> 2);
        size -= 2;
    }

    o->state.saw.phase[0] = phase_0;
    o->state.saw.phase[1] = phase_1;
    o->state.saw.phase[2] = phase_2;
    o->state.saw.phase[3] = phase_3;
}
#endif /* PB_WAVETABLES */

/* ------------------------------------------------------------ noise and the rest --- */

/* RenderFilteredNoise: white noise through a state variable filter at the note's pitch; TIMBRE is the resonance,
 * COLOR morphs LP, BP, HP */
static void pb_dig_render_filtered_noise(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    /* (pitch_ << 17 and parameter_[0] << 17 in the C++: the same bits, shifted as uint32_t) */
    int32_t f = pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)o->pitch << 17);
    int32_t damp = pb_interpolate824_u16(pb_lut_svf_damp, (uint32_t)o->parameter[0] << 17);
    int32_t scale = pb_interpolate824_u16(pb_lut_svf_scale, (uint32_t)o->parameter[0] << 17);
    int32_t bp = o->state.svf.bp;
    int32_t lp = o->state.svf.lp;
    int32_t bp_gain, lp_gain, hp_gain;

    /* morph between LP, BP, HP */
    if (o->parameter[1] < 16384) {
        bp_gain = o->parameter[1];
        lp_gain = 16384 - bp_gain;
        hp_gain = 0;
    } else {
        bp_gain = 32767 - o->parameter[1];
        hp_gain = o->parameter[1] - 16384;
        lp_gain = 0;
    }

    int32_t gain_correction = f > scale ? scale * 32767 / f : 32767;
    (void)sync;
    while (size--) {
        int32_t notch, hp, in;

        in = pb_rng_sample() >> 1;
        notch = in - (bp * damp >> 15);
        lp += f * bp >> 15;
        PB_CLIP(lp);
        hp = notch - lp;
        bp += f * hp >> 15;

        int32_t result = 0;
        result += (lp_gain * lp) >> 14;
        result += (bp_gain * bp) >> 14;
        result += (hp_gain * hp) >> 14;
        PB_CLIP(result);
        result = result * gain_correction >> 15;
        *buffer++ = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(result + 32768));
    }
    o->state.svf.lp = lp;
    o->state.svf.bp = bp;
}

/* RenderTwinPeaksNoise: noise through two resonators, one at the note, one offset by COLOR; TIMBRE is the Q.
 * Two samples per pass (size is even). */
static void pb_dig_render_twin_peaks_noise(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int32_t sample;
    int32_t y10, y20;
    int32_t y11 = o->state.pno.filter_state[0][0];
    int32_t y12 = o->state.pno.filter_state[0][1];
    int32_t y21 = o->state.pno.filter_state[1][0];
    int32_t y22 = o->state.pno.filter_state[1][1];
    uint32_t q = (uint32_t)(65240 + (o->parameter[0] >> 7));
    int32_t q_squared = (int32_t)(q * q >> 17);
    int16_t p1 = o->pitch;

    PB_CONSTRAIN(p1, 0, 16383);
    int32_t c1 = pb_interpolate824_u16(pb_lut_resonator_coefficient, (uint32_t)(p1 << 17));
    int32_t s1 = pb_interpolate824_u16(pb_lut_resonator_scale, (uint32_t)(p1 << 17));

    int16_t p2 = (int16_t)(o->pitch + ((o->parameter[1] - 16384) >> 1));
    PB_CONSTRAIN(p2, 0, 16383);
    int32_t c2 = pb_interpolate824_u16(pb_lut_resonator_coefficient, (uint32_t)(p2 << 17));
    int32_t s2 = pb_interpolate824_u16(pb_lut_resonator_scale, (uint32_t)(p2 << 17));

    /* int32_t * uint32_t: unsigned in the C++ */
    c1 = (int32_t)((uint32_t)c1 * q >> 16);
    c2 = (int32_t)((uint32_t)c2 * q >> 16);

    int32_t makeup_gain = 8191 - (o->parameter[0] >> 2);

    (void)sync;
    while (size) {
        sample = pb_rng_sample() >> 1;

        if (sample > 0) {
            y10 = sample * s1 >> 16;
            y20 = sample * s2 >> 16;
        } else {
            y10 = -((-sample) * s1 >> 16);
            y20 = -((-sample) * s2 >> 16);
        }

        y10 += y11 * c1 >> 15;
        y10 -= y12 * q_squared >> 15;
        PB_CLIP(y10);
        y12 = y11;
        y11 = y10;

        y20 += y21 * c2 >> 15;
        y20 -= y22 * q_squared >> 15;
        PB_CLIP(y20);
        y22 = y21;
        y21 = y20;

        y10 += y20;
        y10 += (y10 * makeup_gain >> 13);
        PB_CLIP(y10);
        sample = y10;
        sample = pb_interpolate88_s16(pb_ws_moderate_overdrive, (uint16_t)(sample + 32768));

        *buffer++ = (int16_t)sample;
        *buffer++ = (int16_t)sample;
        size -= 2;
    }

    o->state.pno.filter_state[0][0] = y11;
    o->state.pno.filter_state[0][1] = y12;
    o->state.pno.filter_state[1][0] = y21;
    o->state.pno.filter_state[1][1] = y22;
}

/* RenderClockedNoise: sample and hold noise clocked at the note's pitch (3 octaves up); TIMBRE sets the period of
 * the repeating random sequence, COLOR the number of quantization steps. Both have hysteresis. */
static void pb_dig_render_clocked_noise(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_clocked_noise_state_t *st = &o->state.clk;

    if ((o->parameter[1] > o->previous_parameter[1] + 64) || (o->parameter[1] < o->previous_parameter[1] - 64)) {
        o->previous_parameter[1] = o->parameter[1];
    }
    if ((o->parameter[0] > o->previous_parameter[0] + 16) || (o->parameter[0] < o->previous_parameter[0] - 16)) {
        o->previous_parameter[0] = o->parameter[0];
    }

    if (o->strike) {
        st->seed = (int32_t)pb_rng_word();
        o->strike = 0;
    }

    /* shift the range of the Coarse knob to reach higher clock rates, close to the sample rate */
    uint32_t phase = o->phase;
    uint32_t phase_increment = o->phase_increment;
    for (size_t i = 0; i < 3; ++i) {
        if (phase_increment < 0x80000000u) {
            phase_increment <<= 1;
        }
    }

    /* the period of the random generator */
    st->cycle_phase_increment = pb_dig_compute_phase_increment((int16_t)(o->previous_parameter[0] - 16384)) << 1;

    /* the number of quantization steps */
    uint32_t num_steps = (uint32_t)(1 + (o->previous_parameter[1] >> 10));
    if (num_steps == 1) {
        num_steps = 2;
    }
    uint32_t quantizer_divider = 65536 / num_steps;
    while (size--) {
        phase += phase_increment;
        if (*sync++) {
            phase = 0;
        }

        /* clock */
        if (phase < phase_increment) {
            /* (the C++'s rng_state * 1664525L + 1013904223L, kept to 32 bits by the uint32_t field) */
            st->rng_state = st->rng_state * 1664525u + 1013904223u;
            st->cycle_phase += st->cycle_phase_increment;
            /* enforce the period */
            if (st->cycle_phase < st->cycle_phase_increment) {
                st->rng_state = (uint32_t)st->seed;
                /* make the period an integer */
                st->cycle_phase = st->cycle_phase_increment;
            }
            uint16_t sample = (uint16_t)st->rng_state;
            sample = (uint16_t)(sample - sample % quantizer_divider);
            sample = (uint16_t)(sample + (quantizer_divider >> 1));
            st->sample = (int16_t)sample;
            /* make the clock rate an exact divisor of the sample rate */
            phase = phase_increment;
        }
        *buffer++ = st->sample;
    }
    o->phase = phase;
}

/* RenderGranularCloud: four sine grains; TIMBRE is the grain rate (envelope length), COLOR the pitch spread */
static void pb_dig_render_granular_cloud(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    for (size_t i = 0; i < 4; ++i) {
        pb_grain_t *g = &o->state.grain[i];
        /* a grain at the end of its envelope is reset */
        if (g->envelope_phase > (1u << 24) || g->envelope_phase_increment == 0) {
            g->envelope_phase_increment = 0;
            if ((pb_rng_word() & 0xffff) < 0x4000) {
                g->envelope_phase_increment = (uint32_t)(pb_lut_granular_envelope_rate[o->parameter[0] >> 7] << 3);
                g->envelope_phase = 0;
                g->phase_increment = o->phase_increment;
                int32_t pitch_mod = pb_rng_sample() * o->parameter[1] >> 16;
                int32_t phi = (int32_t)(o->phase_increment >> 8);
                if (pitch_mod < 0) {
                    g->phase_increment += (uint32_t)(phi * (pitch_mod >> 8));
                } else {
                    g->phase_increment += (uint32_t)(phi * (pitch_mod >> 7));
                }
            }
        }
    }

    (void)sync;
    pb_grain_t *g0 = &o->state.grain[0], *g1 = &o->state.grain[1];
    pb_grain_t *g2 = &o->state.grain[2], *g3 = &o->state.grain[3];
    while (size--) {
        int32_t sample = 0;
        g0->phase += g0->phase_increment;
        g0->envelope_phase += g0->envelope_phase_increment;
        sample += pb_interpolate824_s16(pb_wav_sine, g0->phase) * pb_lut_granular_envelope[g0->envelope_phase >> 16] >> 17;

        g1->phase += g1->phase_increment;
        g1->envelope_phase += g1->envelope_phase_increment;
        sample += pb_interpolate824_s16(pb_wav_sine, g1->phase) * pb_lut_granular_envelope[g1->envelope_phase >> 16] >> 17;

        g2->phase += g2->phase_increment;
        g2->envelope_phase += g2->envelope_phase_increment;
        sample += pb_interpolate824_s16(pb_wav_sine, g2->phase) * pb_lut_granular_envelope[g2->envelope_phase >> 16] >> 17;

        g3->phase += g3->phase_increment;
        g3->envelope_phase += g3->envelope_phase_increment;
        sample += pb_interpolate824_s16(pb_wav_sine, g3->phase) * pb_lut_granular_envelope[g3->envelope_phase >> 16] >> 17;

        if (sample < -32768) {
            sample = -32768;
        }
        if (sample > 32767) {
            sample = 32767;
        }
        *buffer++ = (int16_t)sample;
    }
}

#define PBD_PARTICLE_NOISE_DECAY 64763u     /* kParticleNoiseDecay */
#define PBD_RESONANCE_SQUARED 32506         /* kResonanceSquared = (int32_t)(32768 * 0.996 * 0.996) */
#define PBD_RESONANCE_FACTOR 32636          /* kResonanceFactor = (int32_t)(32768 * 0.996) */

/* RenderParticleNoise: random impulses (density from TIMBRE) through three resonators retuned at each impulse
 * (spread from COLOR). Two samples per pass (size is even). */
static void pb_dig_render_particle_noise(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint16_t amplitude = o->state.pno.amplitude;
    uint32_t density = (uint32_t)(1024 + o->parameter[0]);
    int32_t sample;

    int32_t y10, y20, y30;
    int32_t y11 = o->state.pno.filter_state[0][0];
    int32_t y12 = o->state.pno.filter_state[0][1];
    int32_t s1 = o->state.pno.filter_scale[0];
    int32_t c1 = o->state.pno.filter_coefficient[0];
    int32_t y21 = o->state.pno.filter_state[1][0];
    int32_t y22 = o->state.pno.filter_state[1][1];
    int32_t s2 = o->state.pno.filter_scale[1];
    int32_t c2 = o->state.pno.filter_coefficient[1];
    int32_t y31 = o->state.pno.filter_state[2][0];
    int32_t y32 = o->state.pno.filter_state[2][1];
    int32_t s3 = o->state.pno.filter_scale[2];
    int32_t c3 = o->state.pno.filter_coefficient[2];

    (void)sync;
    while (size) {
        uint32_t noise = pb_rng_word();
        if ((noise & 0x7fffff) < density) {
            amplitude = 65535;
            int16_t noise_a = (int16_t)((noise & 0x0fff) - 0x800);
            int16_t noise_b = (int16_t)(((noise >> 15) & 0x1fff) - 0x1000);
            int16_t p1 = (int16_t)(o->pitch + (3 * noise_a * o->parameter[1] >> 17) + 0x600);

            PB_CONSTRAIN(p1, 0, 16383);
            c1 = pb_interpolate824_u16(pb_lut_resonator_coefficient, (uint32_t)(p1 << 17));
            s1 = pb_interpolate824_u16(pb_lut_resonator_scale, (uint32_t)(p1 << 17));

            int16_t p2 = (int16_t)(o->pitch + (noise_a * o->parameter[1] >> 15) + 0x980);
            PB_CONSTRAIN(p2, 0, 16383);
            c2 = pb_interpolate824_u16(pb_lut_resonator_coefficient, (uint32_t)(p2 << 17));
            s2 = pb_interpolate824_u16(pb_lut_resonator_scale, (uint32_t)(p2 << 17));

            int16_t p3 = (int16_t)(o->pitch + (noise_b * o->parameter[1] >> 16) + 0x790);
            PB_CONSTRAIN(p3, 0, 16383);
            c3 = pb_interpolate824_u16(pb_lut_resonator_coefficient, (uint32_t)(p3 << 17));
            s3 = pb_interpolate824_u16(pb_lut_resonator_scale, (uint32_t)(p3 << 17));

            c1 = c1 * PBD_RESONANCE_FACTOR >> 15;
            c2 = c2 * PBD_RESONANCE_FACTOR >> 15;
            c3 = c3 * PBD_RESONANCE_FACTOR >> 15;
        }
        sample = ((int16_t)noise * amplitude) >> 16;
        /* (uint16_t * uint16_t goes past INT_MAX in the C++'s int; uint32_t gives the same low 16 bits) */
        amplitude = (uint16_t)(((uint32_t)amplitude * PBD_PARTICLE_NOISE_DECAY) >> 16);

        if (sample > 0) {
            y10 = sample * s1 >> 16;
            y20 = sample * s2 >> 16;
            y30 = sample * s3 >> 16;
        } else {
            y10 = -((-sample) * s1 >> 16);
            y20 = -((-sample) * s2 >> 16);
            y30 = -((-sample) * s3 >> 16);
        }

        y10 += y11 * c1 >> 15;
        y10 -= y12 * PBD_RESONANCE_SQUARED >> 15;
        PB_CLIP(y10);
        y12 = y11;
        y11 = y10;

        y20 += y21 * c2 >> 15;
        y20 -= y22 * PBD_RESONANCE_SQUARED >> 15;
        PB_CLIP(y20);
        y22 = y21;
        y21 = y20;

        y30 += y31 * c3 >> 15;
        y30 -= y32 * PBD_RESONANCE_SQUARED >> 15;
        PB_CLIP(y30);
        y32 = y31;
        y31 = y30;

        y10 += y20 + y30;
        PB_CLIP(y10);
        *buffer++ = (int16_t)y10;
        *buffer++ = (int16_t)y10;
        size -= 2;
    }

    o->state.pno.amplitude = amplitude;
    o->state.pno.filter_state[0][0] = y11;
    o->state.pno.filter_state[0][1] = y12;
    o->state.pno.filter_scale[0] = s1;
    o->state.pno.filter_coefficient[0] = c1;
    o->state.pno.filter_state[1][0] = y21;
    o->state.pno.filter_state[1][1] = y22;
    o->state.pno.filter_scale[1] = s2;
    o->state.pno.filter_coefficient[1] = c2;
    o->state.pno.filter_state[2][0] = y31;
    o->state.pno.filter_state[2][1] = y32;
    o->state.pno.filter_scale[2] = s3;
    o->state.pno.filter_coefficient[2] = c3;
}

static const int32_t pbd_constellation_q[] = { 23100, -23100, -23100, 23100 };    /* kConstellationQ */
static const int32_t pbd_constellation_i[] = { 23100, 23100, -23100, -23100 };    /* kConstellationI */

/* RenderDigitalModulation: a QPSK carrier at the note; the symbol rate follows TIMBRE, the data is a preamble then
 * COLOR (smoothed). The output sum can pass 16 bits; the cast wraps it, as the C++'s int16_t buffer does. */
static void pb_dig_render_digital_modulation(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint32_t phase = o->phase;
    uint32_t increment = o->phase_increment;

    uint32_t symbol_stream_phase = o->state.dmd.symbol_phase;
    uint32_t symbol_stream_phase_increment = pb_dig_compute_phase_increment(
        (int16_t)(o->pitch - 1536 + ((o->parameter[0] - 32767) >> 3)));
    uint8_t data_byte = o->state.dmd.data_byte;

    if (o->strike) {
        o->state.dmd.symbol_count = 0;
        o->strike = 0;
    }

    (void)sync;
    while (size--) {
        phase += increment;
        symbol_stream_phase += symbol_stream_phase_increment;
        if (symbol_stream_phase < symbol_stream_phase_increment) {
            ++o->state.dmd.symbol_count;
            if (!(o->state.dmd.symbol_count & 3)) {
                if (o->state.dmd.symbol_count >= (64 + 4 * 256)) {
                    o->state.dmd.symbol_count = 0;
                }
                if (o->state.dmd.symbol_count < 32) {
                    data_byte = 0x00;
                } else if (o->state.dmd.symbol_count < 48) {
                    data_byte = 0x99;
                } else if (o->state.dmd.symbol_count < 64) {
                    data_byte = 0xcc;
                } else {
                    o->state.dmd.filter_state = (o->state.dmd.filter_state * 3 + (int32_t)o->parameter[1]) >> 2;
                    data_byte = (uint8_t)(o->state.dmd.filter_state >> 7);
                }
            } else {
                data_byte >>= 2;
            }
        }
        int16_t i = pb_interpolate824_s16(pb_wav_sine, phase);
        int16_t q = pb_interpolate824_s16(pb_wav_sine, phase + (1u << 30));
        *buffer++ = (int16_t)((pbd_constellation_q[data_byte & 3] * q >> 15) +
                              (pbd_constellation_i[data_byte & 3] * i >> 15));
    }
    o->phase = phase;
    o->state.dmd.symbol_phase = symbol_stream_phase;
    o->state.dmd.data_byte = data_byte;
}

/* wt_code (resources.cc), the Morse message QUESTION_MARK plays: pb_tables.h has it with or without the wavetables */
#define PBD_WT_CODE pb_wt_code

/* RenderQuestionMark: a Morse message (wt_code) on a sine at the note, with radio noise; TIMBRE is the speed,
 * COLOR the noise floor and distortion */
static void pb_dig_render_question_mark(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_clocked_noise_state_t *st = &o->state.clk;

    if (o->strike) {
        st->rng_state = 0;
        st->cycle_phase = 0;
        st->sample = 10;
        st->cycle_phase_increment = 0xffffffffu;        /* (-1 in the C++) */
        st->seed = 32767;
        o->strike = 0;
    }

    uint32_t phase = o->phase;
    uint32_t increment = o->phase_increment;
    uint32_t dit_duration = (uint32_t)(3600 + ((32767 - o->parameter[0]) >> 2));
    int32_t noise_threshold = 1024 + (o->parameter[1] >> 3);
    (void)sync;
    while (size--) {
        phase += increment;
        int32_t sample;
        if (st->rng_state) {
            sample = (pb_interpolate824_s16(pb_wav_sine, phase) * 3) >> 2;
        } else {
            sample = 0;
        }
        if (++st->cycle_phase > dit_duration) {
            --st->sample;
            if (st->sample == 0) {
                ++st->cycle_phase_increment;
                st->rng_state = !st->rng_state;

                size_t address = st->cycle_phase_increment >> 2;
                size_t shift = (st->cycle_phase_increment & 0x3) << 1;
                st->sample = (int16_t)((2 << ((PBD_WT_CODE[address] >> shift) & 3)) - 1);
                if (st->sample == 15) {
                    st->sample = 100;
                    st->rng_state = 0;
                    st->cycle_phase_increment = 0xffffffffu;
                }
                phase = 1u << 30;
            }
            st->cycle_phase = 0;
        }
        st->seed += pb_rng_sample() >> 2;
        int32_t noise_intensity = st->seed >> 8;
        if (noise_intensity < 0) {
            noise_intensity = -noise_intensity;
        }
        if (noise_intensity < noise_threshold) {
            noise_intensity = noise_threshold;
        }
        if (noise_intensity > 16000) {
            noise_intensity = 16000;
        }
        int32_t noise = (pb_rng_sample() * noise_intensity >> 15);
        noise = noise * pb_wav_sine[(phase >> 22) & 0xff] >> 15;
        sample += noise;
        PB_CLIP(sample);
        int32_t distorted = sample * sample >> 14;
        sample += distorted * o->parameter[1] >> 15;
        PB_CLIP(sample);
        *buffer++ = (int16_t)sample;
    }
    o->phase = phase;
}
