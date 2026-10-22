/* SPDX-License-Identifier: MIT
 * Copyright 2012 Emilie Gillet (Mutable Instruments Braids). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.1: the physical models and the drums of Braids' DigitalOscillator (braids/digital_oscillator.cc) in
 * C. Ported from RenderPlucked, RenderBowed, RenderBlown, RenderFluted, RenderStruckBell, RenderStruckDrum,
 * RenderKick, RenderCymbal and RenderSnare, with the constants and tables they use from that file (the bell and drum
 * partials, their amplitudes and decays; the bowed, blown and fluted models' constants). pb_digital.h includes this
 * file after the state and its helpers, and its pb_dig_render calls these functions by the fn_table_ order
 * (PB_DIG_PLUCKED .. PB_DIG_SNARE).
 *
 * The arithmetic is the C++'s, type for type: the same integer widths for every local, the same truncations into
 * int16_t / uint16_t / int8_t, the same unsigned mixes, and the global random generator drawn in the same order. So
 * it is bit-exact with the C++ (tests/braids_parity, shapes 28 to 36).
 *
 * One difference on purpose: RenderFluted reads lut_flute_body_filter[pitch_ >> 7], and that table has 128 entries
 * while pitch_ can reach 140 << 7, so the C++ reads past its end above MIDI note 127. Here the index stops at 127. The
 * parity test does not go above note 127, and past it the C++'s value is whatever follows the table in its flash.
 *
 * Revisions:
 *   0.5.1: first version.
 *   0.5.2: o->delay_lines.x is o->delay_lines->x (pb_digital.h pb_delay_lines_t).
 */
#pragma once
#include "pb_dsp.h"
#include "pb_tables.h"

#define PB_DIG_C 1

/* ------------------------------------------------------- bell and drum data --- */
static const int16_t pbc_bell_partials[PB_NUM_BELL_PARTIALS] = {           /* kBellPartials */
    -1284, -1283, -184, -183, 385, 1175, 1536, 2233, 2434, 2934, 3110
};
static const int16_t pbc_bell_partial_amplitudes[PB_NUM_BELL_PARTIALS] = { /* kBellPartialAmplitudes */
    8192, 5488, 8192, 14745, 21872, 13680, 11960, 10895, 10895, 6144, 10895
};
static const uint16_t pbc_bell_partial_decay_long[PB_NUM_BELL_PARTIALS] = {  /* kBellPartialDecayLong */
    65533, 65533, 65533, 65532, 65531, 65531, 65530, 65529, 65527, 65523, 65519
};
static const uint16_t pbc_bell_partial_decay_short[PB_NUM_BELL_PARTIALS] = { /* kBellPartialDecayShort */
    65308, 65283, 65186, 65123, 64839, 64889, 64632, 64409, 64038, 63302, 62575
};
static const int16_t pbc_drum_partials[PB_NUM_DRUM_PARTIALS] = {           /* kDrumPartials */
    0, 0, 1041, 1747, 1846, 3072
};
static const int16_t pbc_drum_partial_amplitude[PB_NUM_DRUM_PARTIALS] = {  /* kDrumPartialAmplitude */
    16986, 2654, 3981, 5308, 3981, 2985
};
static const uint16_t pbc_drum_partial_decay_long[PB_NUM_DRUM_PARTIALS] = {  /* kDrumPartialDecayLong */
    65533, 65531, 65531, 65531, 65531, 65516
};
static const uint16_t pbc_drum_partial_decay_short[PB_NUM_DRUM_PARTIALS] = { /* kDrumPartialDecayShort */
    65083, 64715, 64715, 64715, 64715, 62312
};

/* ------------------------------------------------- physical model constants --- */
static const int32_t pbc_bridge_lp_gain = 14008;              /* kBridgeLPGain */
static const int32_t pbc_bridge_lp_pole1 = 18022;             /* kBridgeLPPole1 */
static const int32_t pbc_biquad_gain = 6553;                  /* kBiquadGain */
static const int32_t pbc_biquad_pole1 = 6948;                 /* kBiquadPole1 */
static const int32_t pbc_biquad_pole2 = -2959;                /* kBiquadPole2 */
static const uint16_t pbc_breath_pressure = 26214;            /* kBreathPressure */
static const int16_t pbc_reflection_coefficient = -3891;      /* kReflectionCoefficient */
static const int16_t pbc_reed_slope = -1229;                  /* kReedSlope */
static const int16_t pbc_reed_offset = 22938;                 /* kReedOffset */
static const uint16_t pbc_dc_blocking_pole = 4055;            /* kDCBlockingPole = 0.99 * 4096, truncated */

/* (memset of a byte range, which the firmware has no libc for) */
static inline void pbc_zero(void *p, size_t n)
{
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < n; i++) b[i] = 0;
}

/* ------------------------------------------------------------ RenderPlucked --- */
static void pb_dig_render_plucked(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    o->phase_increment <<= 1;
    if (o->strike) {
        ++o->active_voice;
        if (o->active_voice >= PB_NUM_PLUCK_VOICES) o->active_voice = 0;
        /* find the optimal oversampling rate */
        pb_pluck_state_t *p = &o->state.plk[o->active_voice];
        int32_t increment = (int32_t)o->phase_increment;
        p->shift = 0;
        while (increment > (2 << 22)) {
            increment >>= 1;
            ++p->shift;
        }
        p->size = 1024 >> p->shift;
        p->mask = p->size - 1;
        p->write_ptr = 0;
        p->max_phase_increment = o->phase_increment << 1;
        p->phase_increment = o->phase_increment;
        int32_t width = o->parameter[1];
        width = (3 * width) >> 1;
        p->initialization_ptr = p->size * (size_t)(8192 + width) >> 16;   /* (size_t arithmetic, as the C++) */
        o->strike = 0;
    }

    pb_pluck_state_t *current_string = &o->state.plk[o->active_voice];
    /* update the phase increment of the latest note, but not too far above the original pitch */
    current_string->phase_increment = o->phase_increment < current_string->max_phase_increment
        ? o->phase_increment : current_string->max_phase_increment;

    /* loss and stretching factors */
    uint32_t update_probability = (uint32_t)(o->parameter[0] < 16384 ? 65535 : 131072 - (o->parameter[0] >> 3) * 31);
    int16_t loss = (int16_t)(4096u - (o->phase_increment >> 14));     /* (unsigned, then truncated: the C++'s) */
    if (loss < 256) loss = 256;
    if (o->parameter[0] < 16384) loss = (int16_t)(loss * (16384 - o->parameter[0]) >> 14);
    else loss = 0;

    int16_t previous_sample = o->state.plk[0].previous_sample;
    while (size) {
        int32_t sample = 0;
        for (size_t i = 0; i < PB_NUM_PLUCK_VOICES; ++i) {
            pb_pluck_state_t *p = &o->state.plk[i];
            int16_t *dl = o->delay_lines->ks + i * 1025;
            if (p->initialization_ptr) {
                /* initialization: white noise into the delay line */
                --p->initialization_ptr;
                int32_t excitation_sample = (dl[p->initialization_ptr] + 3 * pb_rng_sample()) >> 2;
                dl[p->initialization_ptr] = (int16_t)excitation_sample;
                sample += excitation_sample;
            } else {
                p->phase += p->phase_increment;
                size_t read_ptr = ((p->phase >> (22 + p->shift)) + 2) & p->mask;
                size_t write_ptr = p->write_ptr;
                while (write_ptr != read_ptr) {
                    size_t next = (write_ptr + 1) & p->mask;
                    int32_t a = dl[write_ptr];
                    int32_t b = dl[next];
                    uint32_t probability = pb_rng_word();
                    if ((probability & 0xffff) <= update_probability) {
                        int32_t sum = a + b;
                        sum = sum < 0 ? -(-sum >> 1) : (sum >> 1);
                        if (loss) sum = sum * (32768 - loss) >> 15;
                        dl[write_ptr] = (int16_t)sum;
                    }
                    if (write_ptr == 0) dl[p->size] = dl[0];
                    write_ptr = next;
                }
                p->write_ptr = write_ptr;
                sample += pb_interpolate1022(dl, p->phase >> p->shift);
            }
        }
        PB_CLIP(sample);
        *buffer++ = (int16_t)((previous_sample + sample) >> 1);
        *buffer++ = (int16_t)sample;
        previous_sample = (int16_t)sample;
        size -= 2;
    }
    o->state.plk[0].previous_sample = previous_sample;
}

/* -------------------------------------------------------------- RenderBowed --- */
static void pb_dig_render_bowed(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    int8_t *dl_b = o->delay_lines->bowed.bridge;
    int8_t *dl_n = o->delay_lines->bowed.neck;

    if (o->strike) {
        pbc_zero(dl_b, sizeof o->delay_lines->bowed.bridge);
        pbc_zero(dl_n, sizeof o->delay_lines->bowed.neck);
        pbc_zero(&o->state, sizeof o->state);
        o->strike = 0;
    }
    int16_t parameter_0 = (int16_t)(172 - (o->parameter[0] >> 8));
    int16_t parameter_1 = (int16_t)(6 + (o->parameter[1] >> 9));

    uint16_t delay_ptr = o->state.phy.delay_ptr;
    uint16_t excitation_ptr = o->state.phy.excitation_ptr;
    int32_t lp_state = o->state.phy.lp_state;
    int32_t biquad_y0 = o->state.phy.filter_state[0];
    int32_t biquad_y1 = o->state.phy.filter_state[1];

    /* delay times and interpolation coefficients */
    uint32_t delay = (o->delay >> 1) - (2 << 16);                      /* compensation for the 1-pole delay */
    uint32_t bridge_delay = (delay >> 8) * (uint32_t)parameter_1;
    /* an octave up when the note is too low to fit in the delay lines */
    while ((delay - bridge_delay) > ((PB_WG_NECK_LENGTH - 1) << 16) || bridge_delay > ((PB_WG_BRIDGE_LENGTH - 1) << 16)) {
        delay >>= 1;
        bridge_delay >>= 1;
    }
    uint16_t bridge_delay_integral = (uint16_t)(bridge_delay >> 16);
    uint16_t bridge_delay_fractional = (uint16_t)(bridge_delay & 0xffff);
    uint32_t neck_delay = delay - bridge_delay;
    uint32_t neck_delay_integral = neck_delay >> 16;
    uint16_t neck_delay_fractional = (uint16_t)(neck_delay & 0xffff);
    int16_t previous_sample = o->state.phy.previous_sample;

    /* at half the sample rate (the body filter's coefficients round badly at the full rate) */
    while (size) {
        o->phase += o->phase_increment;

        int32_t new_velocity, friction;
        uint16_t bridge_delay_ptr = (uint16_t)(delay_ptr + 2 * PB_WG_BRIDGE_LENGTH - bridge_delay_integral);
        uint16_t neck_delay_ptr = (uint16_t)(delay_ptr + 2 * PB_WG_NECK_LENGTH - neck_delay_integral);
        int16_t bridge_dl_a = dl_b[bridge_delay_ptr % PB_WG_BRIDGE_LENGTH];
        int16_t bridge_dl_b = dl_b[(bridge_delay_ptr - 1) % PB_WG_BRIDGE_LENGTH];
        int16_t nut_dl_a = dl_n[neck_delay_ptr % PB_WG_NECK_LENGTH];
        int16_t nut_dl_b = dl_n[(neck_delay_ptr - 1) % PB_WG_NECK_LENGTH];
        int32_t bridge_value = pb_mix_s16(bridge_dl_a, bridge_dl_b, bridge_delay_fractional) << 8;
        int32_t nut_value = pb_mix_s16(nut_dl_a, nut_dl_b, neck_delay_fractional) << 8;
        lp_state = (bridge_value * pbc_bridge_lp_gain + lp_state * pbc_bridge_lp_pole1) >> 15;
        int32_t bridge_reflection = -lp_state;
        int32_t nut_reflection = -nut_value;
        int32_t string_velocity = bridge_reflection + nut_reflection;
        int32_t bow_velocity = pb_lut_bowing_envelope[excitation_ptr >> 1];
        bow_velocity += pb_lut_bowing_envelope[(excitation_ptr + 1) >> 1];
        bow_velocity >>= 1;
        int32_t velocity_delta = bow_velocity - string_velocity;

        friction = velocity_delta * parameter_0 >> 5;
        if (friction < 0) friction = -friction;
        if (friction >= (1 << 17)) friction = (1 << 17) - 1;
        friction = pb_lut_bowing_friction[friction >> 9];
        new_velocity = friction * velocity_delta >> 15;
        dl_n[delay_ptr % PB_WG_NECK_LENGTH] = (int8_t)((bridge_reflection + new_velocity) >> 8);
        dl_b[delay_ptr % PB_WG_BRIDGE_LENGTH] = (int8_t)((nut_reflection + new_velocity) >> 8);
        ++delay_ptr;

        int32_t temp = bridge_value * pbc_biquad_gain >> 15;
        temp += biquad_y0 * pbc_biquad_pole1 >> 12;
        temp += biquad_y1 * pbc_biquad_pole2 >> 12;
        int32_t out = temp - biquad_y1;
        biquad_y1 = biquad_y0;
        biquad_y0 = temp;

        PB_CLIP(out);
        *buffer++ = (int16_t)((out + previous_sample) >> 1);
        *buffer++ = (int16_t)out;
        previous_sample = (int16_t)out;
        ++excitation_ptr;
        size -= 2;
    }
    if ((excitation_ptr >> 1) >= (int)PB_LUT_BOWING_ENVELOPE_SIZE - 32)
        excitation_ptr = (uint16_t)(((int)PB_LUT_BOWING_ENVELOPE_SIZE - 32) << 1);
    o->state.phy.delay_ptr = (uint16_t)(delay_ptr % PB_WG_NECK_LENGTH);
    o->state.phy.excitation_ptr = excitation_ptr;
    o->state.phy.lp_state = lp_state;
    o->state.phy.filter_state[0] = biquad_y0;
    o->state.phy.filter_state[1] = biquad_y1;
    o->state.phy.previous_sample = previous_sample;
}

/* -------------------------------------------------------------- RenderBlown --- */
static void pb_dig_render_blown(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint16_t delay_ptr = o->state.phy.delay_ptr;
    int32_t lp_state = o->state.phy.lp_state;

    int16_t *dl = o->delay_lines->bore;
    if (o->strike) {
        pbc_zero(dl, sizeof o->delay_lines->bore);
        o->strike = 0;
    }

    uint32_t delay = (o->delay >> 1) - (1 << 16);
    while (delay > ((PB_WG_BORE_LENGTH - 1) << 16)) delay >>= 1;
    uint16_t bore_delay_integral = (uint16_t)(delay >> 16);
    uint16_t bore_delay_fractional = (uint16_t)(delay & 0xffff);
    uint16_t parameter = (uint16_t)(28000 - (o->parameter[0] >> 1));
    int16_t filter_state = (int16_t)o->state.phy.filter_state[0];
    int16_t normalized_pitch = (int16_t)((o->pitch - 8192 + (o->parameter[1] >> 1)) >> 7);
    if (normalized_pitch < 0) normalized_pitch = 0;
    else if (normalized_pitch > 127) normalized_pitch = 127;
    uint16_t filter_coefficient = pb_lut_flute_body_filter[normalized_pitch];
    while (size--) {
        o->phase += o->phase_increment;

        int32_t breath_pressure = pb_rng_sample() * parameter >> 15;
        breath_pressure = breath_pressure * pbc_breath_pressure >> 15;
        breath_pressure += pbc_breath_pressure;

        uint16_t bore_delay_ptr = (uint16_t)(delay_ptr + 2 * PB_WG_BORE_LENGTH - bore_delay_integral);
        int16_t dl_a = dl[bore_delay_ptr % PB_WG_BORE_LENGTH];
        int16_t dl_b = dl[(bore_delay_ptr - 1) % PB_WG_BORE_LENGTH];
        int32_t dl_value = pb_mix_s16(dl_a, dl_b, bore_delay_fractional);

        int32_t pressure_delta = (dl_value >> 1) + lp_state;
        lp_state = dl_value >> 1;

        pressure_delta = pbc_reflection_coefficient * pressure_delta >> 12;
        pressure_delta -= breath_pressure;
        int32_t reed = (pressure_delta * pbc_reed_slope >> 12) + pbc_reed_offset;
        PB_CLIP(reed);
        int32_t out = pressure_delta * reed >> 15;
        out += breath_pressure;
        PB_CLIP(out);
        dl[delay_ptr++ % PB_WG_BORE_LENGTH] = (int16_t)out;
        filter_state = (int16_t)((filter_coefficient * out + (4096 - filter_coefficient) * filter_state) >> 12);
        *buffer++ = filter_state;
    }
    o->state.phy.filter_state[0] = filter_state;
    o->state.phy.delay_ptr = (uint16_t)(delay_ptr % PB_WG_BORE_LENGTH);
    o->state.phy.lp_state = lp_state;
}

/* ------------------------------------------------------------- RenderFluted --- */
static void pb_dig_render_fluted(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    uint16_t delay_ptr = o->state.phy.delay_ptr;
    uint16_t excitation_ptr = o->state.phy.excitation_ptr;
    int32_t lp_state = o->state.phy.lp_state;
    int32_t dc_blocking_x0 = o->state.phy.filter_state[0];
    int32_t dc_blocking_y0 = o->state.phy.filter_state[1];

    int8_t *dl_b = o->delay_lines->fluted.bore;
    int8_t *dl_j = o->delay_lines->fluted.jet;

    if (o->strike) {
        excitation_ptr = 0;
        pbc_zero(dl_b, sizeof o->delay_lines->fluted.bore);
        pbc_zero(dl_j, sizeof o->delay_lines->fluted.jet);
        lp_state = 0;
        o->strike = 0;
    }

    /* delay times and interpolation coefficients */
    uint32_t bore_delay = (o->delay << 1) - (2 << 16);
    uint32_t jet_delay = (bore_delay >> 8) * (uint32_t)(48 + (o->parameter[1] >> 10));
    bore_delay -= jet_delay;
    while (bore_delay > ((PB_WG_FBORE_LENGTH - 1) << 16) || jet_delay > ((PB_WG_JET_LENGTH - 1) << 16)) {
        bore_delay >>= 1;
        jet_delay >>= 1;
    }
    uint16_t bore_delay_integral = (uint16_t)(bore_delay >> 16);
    uint16_t bore_delay_fractional = (uint16_t)(bore_delay & 0xffff);
    uint32_t jet_delay_integral = jet_delay >> 16;
    uint16_t jet_delay_fractional = (uint16_t)(jet_delay & 0xffff);

    uint16_t breath_intensity = (uint16_t)(2100 - (o->parameter[0] >> 4));
    int32_t body_index = o->pitch >> 7;
    if (body_index > 127) body_index = 127;             /* (the C++ reads past the table here: see the header) */
    uint16_t filter_coefficient = pb_lut_flute_body_filter[body_index];
    while (size--) {
        o->phase += o->phase_increment;

        uint16_t bore_delay_ptr = (uint16_t)(delay_ptr + 2 * PB_WG_FBORE_LENGTH - bore_delay_integral);
        uint16_t jet_delay_ptr = (uint16_t)(delay_ptr + 2 * PB_WG_JET_LENGTH - jet_delay_integral);
        int16_t bore_dl_a = dl_b[bore_delay_ptr % PB_WG_FBORE_LENGTH];
        int16_t bore_dl_b = dl_b[(bore_delay_ptr - 1) % PB_WG_FBORE_LENGTH];
        int16_t jet_dl_a = dl_j[jet_delay_ptr % PB_WG_JET_LENGTH];
        int16_t jet_dl_b = dl_j[(jet_delay_ptr - 1) % PB_WG_JET_LENGTH];
        int32_t bore_value = pb_mix_s16(bore_dl_a, bore_dl_b, bore_delay_fractional) << 9;
        int32_t jet_value = pb_mix_s16(jet_dl_a, jet_dl_b, jet_delay_fractional) << 9;

        int32_t breath_pressure = pb_lut_blowing_envelope[excitation_ptr];
        breath_pressure <<= 1;
        int32_t random_pressure = pb_rng_sample() * breath_intensity >> 12;
        random_pressure = random_pressure * breath_pressure >> 15;
        breath_pressure += random_pressure;

        lp_state = (-filter_coefficient * bore_value + (4096 - filter_coefficient) * lp_state) >> 12;
        int32_t reflection = lp_state;
        dc_blocking_y0 = (pbc_dc_blocking_pole * dc_blocking_y0 >> 12);
        dc_blocking_y0 += reflection - dc_blocking_x0;
        dc_blocking_x0 = reflection;
        reflection = dc_blocking_y0;

        int32_t pressure_delta = breath_pressure - (reflection >> 1);
        dl_j[delay_ptr % PB_WG_JET_LENGTH] = (int8_t)(pressure_delta >> 9);

        pressure_delta = jet_value;
        int32_t jet_table_index = pressure_delta;
        if (jet_table_index < 0) jet_table_index = 0;
        if (jet_table_index > 65535) jet_table_index = 65535;
        pressure_delta = (int16_t)pb_lut_blowing_jet[jet_table_index >> 8] + (reflection >> 1);
        dl_b[delay_ptr % PB_WG_FBORE_LENGTH] = (int8_t)(pressure_delta >> 9);
        ++delay_ptr;

        int32_t out = bore_value >> 1;
        PB_CLIP(out);
        *buffer++ = (int16_t)out;
        if (size & 3) ++excitation_ptr;
    }
    if (excitation_ptr >= PB_LUT_BLOWING_ENVELOPE_SIZE - 32) excitation_ptr = PB_LUT_BLOWING_ENVELOPE_SIZE - 32;
    o->state.phy.delay_ptr = delay_ptr;
    o->state.phy.excitation_ptr = excitation_ptr;
    o->state.phy.lp_state = lp_state;
    o->state.phy.filter_state[0] = dc_blocking_x0;
    o->state.phy.filter_state[1] = dc_blocking_y0;
}

/* --------------------------------------------------------- RenderStruckBell --- */
static void pb_dig_render_struck_bell(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_additive_state_t *a = &o->state.add;
    /* to save cycles, refresh the frequencies of 3 partials per block (an "arpeggiation" with fast CV) */
    size_t first_partial = a->current_partial;
    size_t last_partial = a->current_partial + 3 < PB_NUM_BELL_PARTIALS ? a->current_partial + 3 : PB_NUM_BELL_PARTIALS;
    a->current_partial = (first_partial + 3) % PB_NUM_BELL_PARTIALS;

    if (o->strike) {
        for (size_t i = 0; i < PB_NUM_BELL_PARTIALS; ++i) {
            a->partial_amplitude[i] = pbc_bell_partial_amplitudes[i];
            a->partial_phase[i] = 1u << 30;
        }
        o->strike = 0;
        first_partial = 0;
        last_partial = PB_NUM_BELL_PARTIALS;
    }

    for (size_t i = first_partial; i < last_partial; ++i) {
        int16_t partial_pitch = (int16_t)(o->pitch + pbc_bell_partials[i]);
        if (i & 1) partial_pitch = (int16_t)(partial_pitch + (o->parameter[1] >> 7));
        else partial_pitch = (int16_t)(partial_pitch - (o->parameter[1] >> 7));
        a->partial_phase_increment[i] = pb_dig_compute_phase_increment(partial_pitch) << 1;
    }

    /* a "droning" bell, with no energy loss, at the parameter's top */
    if (o->parameter[0] < 32000) {
        for (size_t i = 0; i < PB_NUM_BELL_PARTIALS; ++i) {
            int32_t decay_long = pbc_bell_partial_decay_long[i];
            int32_t decay_short = pbc_bell_partial_decay_short[i];
            int16_t balance = (int16_t)((32767 - o->parameter[0]) >> 8);
            balance = (int16_t)(balance * balance >> 7);
            int32_t decay = decay_long - ((decay_long - decay_short) * balance >> 7);
            a->partial_amplitude[i] = a->partial_amplitude[i] * decay >> 16;
        }
    }

    int16_t previous_sample = a->previous_sample;
    while (size--) {
        int32_t out = 0;
        for (size_t i = 0; i < PB_NUM_BELL_PARTIALS; ++i) {
            a->partial_phase[i] += a->partial_phase_increment[i];
            int32_t partial = pb_interpolate824_s16(pb_wav_sine, a->partial_phase[i]);
            out += partial * a->partial_amplitude[i] >> 17;
        }
        PB_CLIP(out);
        *buffer++ = (int16_t)((out + previous_sample) >> 1);
        *buffer++ = (int16_t)out; size--;
        previous_sample = (int16_t)out;
    }
    a->previous_sample = previous_sample;
}

/* --------------------------------------------------------- RenderStruckDrum --- */
static void pb_dig_render_struck_drum(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    pb_additive_state_t *a = &o->state.add;
    if (o->strike) {
        int reset_phase = a->partial_amplitude[0] < 1024;
        for (size_t i = 0; i < PB_NUM_DRUM_PARTIALS; ++i) {
            a->target_partial_amplitude[i] = pbc_drum_partial_amplitude[i];
            if (reset_phase) a->partial_phase[i] = 1u << 30;
        }
        o->strike = 0;
    } else {
        if (o->parameter[0] < 32000) {
            for (size_t i = 0; i < PB_NUM_DRUM_PARTIALS; ++i) {
                int32_t decay_long = pbc_drum_partial_decay_long[i];
                int32_t decay_short = pbc_drum_partial_decay_short[i];
                int16_t balance = (int16_t)((32767 - o->parameter[0]) >> 8);
                balance = (int16_t)(balance * balance >> 7);
                int32_t decay = decay_long - ((decay_long - decay_short) * balance >> 7);
                a->target_partial_amplitude[i] = a->partial_amplitude[i] * decay >> 16;
            }
        }
    }

    for (size_t i = 0; i < PB_NUM_DRUM_PARTIALS; ++i) {
        int16_t partial_pitch = (int16_t)(o->pitch + pbc_drum_partials[i]);
        a->partial_phase_increment[i] = pb_dig_compute_phase_increment(partial_pitch) << 1;
    }

    int16_t previous_sample = a->previous_sample;
    int32_t cutoff = (o->pitch - 12 * 128) + (o->parameter[1] >> 2);
    if (cutoff < 0) cutoff = 0;
    else if (cutoff > 32767) cutoff = 32767;
    int32_t f = pb_interpolate824_u16(pb_lut_svf_cutoff, (uint32_t)(cutoff << 16));
    int32_t lp_state_0 = a->lp_noise[0];
    int32_t lp_state_1 = a->lp_noise[1];
    int32_t lp_state_2 = a->lp_noise[2];
    int32_t harmonics_gain = o->parameter[1] < 12888 ? (o->parameter[1] + 4096) : 16384;
    int32_t noise_mode_gain = o->parameter[1] < 16384 ? 0 : o->parameter[1] - 16384;
    noise_mode_gain = noise_mode_gain * 12888 >> 14;

    int32_t fade_increment = (int32_t)(65536 / size);   /* (an unsigned division, as the C++'s by size_t) */
    int32_t fade = 0;
    while (size--) {
        fade += fade_increment;
        int32_t harmonics = 0;

        int32_t noise = pb_rng_sample();
        if (noise > 16384) noise = 16384;
        if (noise < -16384) noise = -16384;
        lp_state_0 += (noise - lp_state_0) * f >> 15;
        lp_state_1 += (lp_state_0 - lp_state_1) * f >> 15;
        lp_state_2 += (lp_state_1 - lp_state_2) * f >> 15;

        int32_t partials[PB_NUM_DRUM_PARTIALS];
        for (size_t i = 0; i < PB_NUM_DRUM_PARTIALS; ++i) {
            a->partial_phase[i] += a->partial_phase_increment[i];
            int32_t partial = pb_interpolate824_s16(pb_wav_sine, a->partial_phase[i]);
            int32_t amplitude = a->partial_amplitude[i]
                + (((a->target_partial_amplitude[i] - a->partial_amplitude[i]) * fade) >> 15);
            partial = partial * amplitude >> 16;
            harmonics += partial;
            partials[i] = partial;
        }
        int32_t sample = partials[0];
        int32_t noise_mode_1 = partials[1] * lp_state_2 >> 8;
        int32_t noise_mode_2 = partials[3] * lp_state_2 >> 9;
        sample += noise_mode_1 * (12288 - noise_mode_gain) >> 14;
        sample += noise_mode_2 * noise_mode_gain >> 14;
        sample += harmonics * harmonics_gain >> 14;
        PB_CLIP(sample);
        *buffer++ = (int16_t)((sample + previous_sample) >> 1);
        *buffer++ = (int16_t)sample; size--;
        previous_sample = (int16_t)sample;
    }
    a->previous_sample = previous_sample;
    a->lp_noise[0] = lp_state_0;
    a->lp_noise[1] = lp_state_1;
    a->lp_noise[2] = lp_state_2;
    for (size_t i = 0; i < PB_NUM_BELL_PARTIALS; ++i) {  /* (all 11, as the C++) */
        a->partial_amplitude[i] = a->target_partial_amplitude[i];
    }
}

/* --------------------------------------------------------------- RenderKick --- */
static void pb_dig_render_kick(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    if (o->init) {
        pb_excitation_init(&o->pulse[0]);
        pb_excitation_set_delay(&o->pulse[0], 0);
        pb_excitation_set_decay(&o->pulse[0], 3340);

        pb_excitation_init(&o->pulse[1]);
        pb_excitation_set_delay(&o->pulse[1], 48);       /* 1.0e-3 * 48000 */
        pb_excitation_set_decay(&o->pulse[1], 3072);

        pb_excitation_init(&o->pulse[2]);
        pb_excitation_set_delay(&o->pulse[2], 192);      /* 4.0e-3 * 48000 */
        pb_excitation_set_decay(&o->pulse[2], 4093);

        pb_svf_init(&o->svf[0]);
        pb_svf_set_punch(&o->svf[0], 32768);
        pb_svf_set_mode(&o->svf[0], PB_SVF_MODE_BP);
        o->init = 0;
    }

    if (o->strike) {
        o->strike = 0;
        pb_excitation_trigger(&o->pulse[0], 275251);     /* 12 * 32768 * 0.7, truncated */
        pb_excitation_trigger(&o->pulse[1], -13763);     /* -19662 * 0.7, truncated */
        pb_excitation_trigger(&o->pulse[2], 18000);
        pb_svf_set_punch(&o->svf[0], 24000);
    }

    uint32_t decay = (uint32_t)(int32_t)o->parameter[0];
    uint32_t scaled = 65535 - (decay << 1);
    uint32_t squared = scaled * scaled >> 16;
    scaled = squared * scaled >> 18;
    pb_svf_set_resonance(&o->svf[0], (int16_t)(32768 - 128 - scaled));

    uint32_t coefficient = (uint32_t)(int32_t)o->parameter[1];
    coefficient = coefficient * coefficient >> 15;
    coefficient = coefficient * coefficient >> 15;
    int32_t lp_coefficient = (int32_t)(128 + (coefficient >> 1) * 3);
    int32_t lp_state = o->state.svf.lp;

    while (size) {
        int32_t excitation = 0;
        excitation += pb_excitation_process(&o->pulse[0]);
        excitation += !pb_excitation_done(&o->pulse[1]) ? 16384 : 0;
        excitation += pb_excitation_process(&o->pulse[1]);
        pb_excitation_process(&o->pulse[2]);
        pb_svf_set_frequency(&o->svf[0], (int16_t)(o->pitch + (pb_excitation_done(&o->pulse[2]) ? 0 : 17 << 7)));

        for (int32_t j = 0; j < 2; ++j) {
            int32_t resonator_output = (excitation >> 4) + pb_svf_process(&o->svf[0], excitation);
            lp_state += (resonator_output - lp_state) * lp_coefficient >> 15;
            PB_CLIP(lp_state);
            *buffer++ = (int16_t)lp_state;
        }
        size -= 2;
    }
    o->state.svf.lp = lp_state;
}

/* -------------------------------------------------------------- RenderSnare --- */
static void pb_dig_render_snare(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    if (o->init) {
        pb_excitation_init(&o->pulse[0]);
        pb_excitation_set_delay(&o->pulse[0], 0);
        pb_excitation_set_decay(&o->pulse[0], 1536);

        pb_excitation_init(&o->pulse[1]);
        pb_excitation_set_delay(&o->pulse[1], 48);       /* 1e-3 * 48000 */
        pb_excitation_set_decay(&o->pulse[1], 3072);

        pb_excitation_init(&o->pulse[2]);
        pb_excitation_set_delay(&o->pulse[2], 48);       /* 1e-3 * 48000 */
        pb_excitation_set_decay(&o->pulse[2], 1200);

        pb_excitation_init(&o->pulse[3]);
        pb_excitation_set_delay(&o->pulse[3], 0);

        pb_svf_init(&o->svf[0]);
        pb_svf_init(&o->svf[1]);
        pb_svf_init(&o->svf[2]);
        pb_svf_set_resonance(&o->svf[2], 2000);
        pb_svf_set_mode(&o->svf[2], PB_SVF_MODE_BP);
        o->init = 0;
    }

    if (o->strike) {
        int32_t decay = 49152 - o->pitch;
        decay += o->parameter[1] < 16384 ? 0 : o->parameter[1] - 16384;
        if (decay > 65535) decay = 65535;
        pb_svf_set_resonance(&o->svf[0], (int16_t)(29000 + (decay >> 5)));
        pb_svf_set_resonance(&o->svf[1], (int16_t)(26500 + (decay >> 5)));
        pb_excitation_set_decay(&o->pulse[3], (uint16_t)(4092 + (decay >> 14)));

        pb_excitation_trigger(&o->pulse[0], 15 * 32768);
        pb_excitation_trigger(&o->pulse[1], -1 * 32768);
        pb_excitation_trigger(&o->pulse[2], 13107);
        int32_t snappy = o->parameter[1];
        if (snappy >= 14336) snappy = 14336;
        pb_excitation_trigger(&o->pulse[3], 512 + (snappy << 1));
        o->strike = 0;
    }

    pb_svf_set_frequency(&o->svf[0], (int16_t)(o->pitch + (12 << 7)));
    pb_svf_set_frequency(&o->svf[1], (int16_t)(o->pitch + (24 << 7)));
    pb_svf_set_frequency(&o->svf[2], (int16_t)(o->pitch + (60 << 7)));

    int32_t g_1 = 22000 - (o->parameter[0] >> 1);
    int32_t g_2 = 22000 + (o->parameter[0] >> 1);

    while (size) {
        int32_t excitation_1 = 0;
        excitation_1 += pb_excitation_process(&o->pulse[0]);
        excitation_1 += pb_excitation_process(&o->pulse[1]);
        excitation_1 += !pb_excitation_done(&o->pulse[1]) ? 2621 : 0;

        int32_t excitation_2 = 0;
        excitation_2 += pb_excitation_process(&o->pulse[2]);
        excitation_2 += !pb_excitation_done(&o->pulse[2]) ? 13107 : 0;

        /* (the C++ leaves the order of these two calls open; they touch different state) */
        int32_t noise = pb_rng_sample();
        int32_t noise_sample = noise * pb_excitation_process(&o->pulse[3]) >> 15;

        int32_t sd = 0;
        sd += (pb_svf_process(&o->svf[0], excitation_1) + (excitation_1 >> 4)) * g_1 >> 15;
        sd += (pb_svf_process(&o->svf[1], excitation_2) + (excitation_2 >> 4)) * g_2 >> 15;
        sd += pb_svf_process(&o->svf[2], noise_sample);
        PB_CLIP(sd);

        *buffer++ = (int16_t)sd;
        *buffer++ = (int16_t)sd;
        size -= 2;
    }
}

/* ------------------------------------------------------------- RenderCymbal --- */
static void pb_dig_render_cymbal(pb_dig_t *o, const uint8_t *sync, int16_t *buffer, size_t size)
{
    if (o->init) {
        pb_svf_init(&o->svf[0]);
        pb_svf_set_mode(&o->svf[0], PB_SVF_MODE_BP);
        pb_svf_set_resonance(&o->svf[0], 12000);
        pb_svf_init(&o->svf[1]);
        pb_svf_set_mode(&o->svf[1], PB_SVF_MODE_HP);
        pb_svf_set_resonance(&o->svf[1], 2000);
        o->init = 0;
    }

    pb_hat_state_t *hat = &o->state.hat;

    uint32_t increments[7];
    int32_t note = (40 << 7) + (o->pitch >> 1);
    increments[0] = pb_dig_compute_phase_increment((int16_t)note);

    uint32_t root = increments[0] >> 10;
    increments[1] = root * 24273 >> 4;
    increments[2] = root * 12561 >> 4;
    increments[3] = root * 18417 >> 4;
    increments[4] = root * 22452 >> 4;
    increments[5] = root * 31858 >> 4;
    increments[6] = increments[0] * 24;

    int32_t xfade = o->parameter[1];
    pb_svf_set_frequency(&o->svf[0], (int16_t)(o->parameter[0] >> 1));
    pb_svf_set_frequency(&o->svf[1], (int16_t)(o->parameter[0] >> 1));

    while (size--) {
        o->phase += increments[6];
        if (o->phase < increments[6]) {
            /* (the C++ multiplies by long constants; only the low 32 bits are kept, so this is the same) */
            hat->rng_state = hat->rng_state * 1664525u + 1013904223u;
        }
        hat->phase[0] += increments[0];
        hat->phase[1] += increments[1];
        hat->phase[2] += increments[2];
        hat->phase[3] += increments[3];
        hat->phase[4] += increments[4];
        hat->phase[5] += increments[5];

        int32_t hat_noise = 0;
        hat_noise += (int32_t)(hat->phase[0] >> 31);
        hat_noise += (int32_t)(hat->phase[1] >> 31);
        hat_noise += (int32_t)(hat->phase[2] >> 31);
        hat_noise += (int32_t)(hat->phase[3] >> 31);
        hat_noise += (int32_t)(hat->phase[4] >> 31);
        hat_noise += (int32_t)(hat->phase[5] >> 31);
        hat_noise -= 3;
        hat_noise *= 5461;
        hat_noise = pb_svf_process(&o->svf[0], hat_noise);
        PB_CLIP(hat_noise);

        int32_t noise = (int32_t)((hat->rng_state >> 16) - 32768);   /* (unsigned, then to int: the C++'s) */
        noise = pb_svf_process(&o->svf[1], noise >> 1);
        PB_CLIP(noise);

        *buffer++ = (int16_t)(hat_noise + ((noise - hat_noise) * xfade >> 15));
    }
}
