/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the DX7 units of Plaits 1.2's six-operator FM engine (pl_sixop.h), in C.
 *   plaits/dsp/fm/patch.h        pl_dx_patch_t (fm::Patch: one unpacked 156-byte patch), pl_dx_patch_unpack (Unpack)
 *   plaits/dsp/fm/dx_units.h/.cc pl_dx_pow2_fast (Pow2Fast<order>), pl_dx_operator_level .. pl_dx_frequency_ratio
 *                                (the tables lut_coarse, lut_amp_mod_sensitivity, lut_pitch_mod_sensitivity and
 *                                lut_cube_root are pl_tables2.h's pl_lut_*)
 *   plaits/dsp/fm/envelope.h     pl_dx_env_t (Envelope<4, reshape_ascending_segments>: the template argument is the
 *                                `reshape` argument, 1 for an OperatorEnvelope, 0 for a PitchEnvelope),
 *                                pl_dx_op_env_set (OperatorEnvelope::Set), pl_dx_pitch_env_set (PitchEnvelope::Set)
 *   plaits/dsp/fm/lfo.h          pl_dx_lfo_t (fm::Lfo)
 * Every float operation in the order of the C++.
 */
#pragma once
#include "pl_osc.h"
#include "pl_tables2.h"

/* ---------------------------------------------------------- patch.h --- */
#define PL_DX_SYX_SIZE 128                      /* Patch::SYX_SIZE: one packed patch of a 32-patch DX7 bank */
#define PL_DX_NUM_OPERATORS 6
typedef struct { uint8_t rate[4], level[4]; } pl_dx_patch_envelope_t;
typedef struct { uint8_t break_point, left_depth, right_depth, left_curve, right_curve; } pl_dx_keyboard_scaling_t;
typedef struct {
    pl_dx_patch_envelope_t envelope;
    pl_dx_keyboard_scaling_t keyboard_scaling;
    uint8_t rate_scaling, amp_mod_sensitivity, velocity_sensitivity, level, mode, coarse;
    uint8_t fine;                               /* x frequency by 1 + 0.01 x fine */
    uint8_t detune;
} pl_dx_patch_operator_t;
typedef struct {
    uint8_t rate, delay, pitch_mod_depth, amp_mod_depth, reset_phase, waveform, pitch_mod_sensitivity;
} pl_dx_modulation_params_t;
typedef struct {                                /* fm::Patch (156 bytes, as the SysEx layout) */
    pl_dx_patch_operator_t op[PL_DX_NUM_OPERATORS];
    pl_dx_patch_envelope_t pitch_envelope;
    uint8_t algorithm, feedback, reset_phase;
    pl_dx_modulation_params_t modulations;
    uint8_t transpose;
    uint8_t name[10];
    uint8_t active_operators;
} pl_dx_patch_t;

static inline uint8_t pl_dx_min_u8(int a, int b) { return (uint8_t)(b < a ? b : a); }   /* std::min(int, int) */
/* Patch::Unpack: the 128 packed bytes to the 156 of the struct (deterministic: the same bytes give the same patch) */
static void pl_dx_patch_unpack(pl_dx_patch_t *p, const uint8_t *data)
{
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ++i) {
        pl_dx_patch_operator_t *o = &p->op[i];
        const uint8_t *op_data = &data[i * 17];
        for (int j = 0; j < 4; ++j) {
            o->envelope.rate[j] = pl_dx_min_u8(op_data[j] & 0x7f, 99);
            o->envelope.level[j] = pl_dx_min_u8(op_data[4 + j] & 0x7f, 99);
        }
        o->keyboard_scaling.break_point = pl_dx_min_u8(op_data[8] & 0x7f, 99);
        o->keyboard_scaling.left_depth = pl_dx_min_u8(op_data[9] & 0x7f, 99);
        o->keyboard_scaling.right_depth = pl_dx_min_u8(op_data[10] & 0x7f, 99);
        o->keyboard_scaling.left_curve = op_data[11] & 0x3;
        o->keyboard_scaling.right_curve = (op_data[11] >> 2) & 0x3;
        o->rate_scaling = op_data[12] & 0x7;
        o->amp_mod_sensitivity = op_data[13] & 0x3;
        o->velocity_sensitivity = (op_data[13] >> 2) & 0x7;
        o->level = pl_dx_min_u8(op_data[14] & 0x7f, 99);
        o->mode = op_data[15] & 0x1;
        o->coarse = (op_data[15] >> 1) & 0x1f;
        o->fine = pl_dx_min_u8(op_data[16] & 0x7f, 99);
        o->detune = pl_dx_min_u8((op_data[12] >> 3) & 0xf, 14);
    }
    for (int j = 0; j < 4; ++j) {
        p->pitch_envelope.rate[j] = pl_dx_min_u8(data[102 + j] & 0x7f, 99);
        p->pitch_envelope.level[j] = pl_dx_min_u8(data[106 + j] & 0x7f, 99);
    }
    p->algorithm = data[110] & 0x1f;
    p->feedback = data[111] & 0x7;
    p->reset_phase = (data[111] >> 3) & 0x1;
    p->modulations.rate = pl_dx_min_u8(data[112] & 0x7f, 99);
    p->modulations.delay = pl_dx_min_u8(data[113] & 0x7f, 99);
    p->modulations.pitch_mod_depth = pl_dx_min_u8(data[114] & 0x7f, 99);
    p->modulations.amp_mod_depth = pl_dx_min_u8(data[115] & 0x7f, 99);
    p->modulations.reset_phase = data[116] & 0x1;
    p->modulations.waveform = pl_dx_min_u8((data[116] >> 1) & 0x7, 5);
    p->modulations.pitch_mod_sensitivity = data[116] >> 4;
    p->transpose = pl_dx_min_u8(data[117] & 0x7f, 48);
    for (size_t i = 0; i < sizeof(p->name); ++i) p->name[i] = data[118 + i] & 0x7f;
    p->active_operators = 0x3f;
}

/* ------------------------------------------------------- dx_units.h --- */
typedef union { float f; int32_t w; } pl_dx_bits_t;
/* Pow2Fast<order>: 2^x from a polynomial of 2^frac(x) and the exponent field of the IEEE 754 result. The C++'s
 * `r.w += x_integral << 23` is a signed shift (x_integral is often negative here): written as a multiplication. */
static inline float pl_dx_pow2_fast(int order, float x)
{
    pl_dx_bits_t r;
    if (order == 1) {
        r.w = (int32_t)((float)(1 << 23) * (127.0f + x));
        return r.f;
    }
    int32_t x_integral = (int32_t)x;
    if (x < 0.0f) --x_integral;
    x -= (float)x_integral;
    if (order == 2) r.f = 1.0f + x * (0.6565f + x * 0.3435f);
    else r.f = 1.0f + x * (0.6958f + x * (0.2251f + x * 0.0791f));    /* (order 3) */
    r.w += x_integral * (1 << 23);
    return r.f;
}
/* OperatorLevel: an operator (envelope) level 0-99 to the complement of the "TL" value (0 = 0, 99 = 127) */
static inline int pl_dx_operator_level(int level)
{
    int tlc = level;
    if (level < 20) tlc = tlc < 15 ? (tlc * (36 - tlc)) >> 3 : 27 + tlc;
    else tlc += 28;
    return tlc;
}
/* PitchEnvelopeLevel: an envelope level 0-99 to an octave shift (0 = -4, 50 = 0, 99 = +4) */
static inline float pl_dx_pitch_envelope_level(int level)
{
    float l = ((float)level - 50.0f) / 32.0f;
    float tail = pl_maxf(pl_fabsf(l + 0.02f) - 1.0f, 0.0f);
    return l * (1.0f + tail * tail * 5.3056f);
}
/* OperatorEnvelopeIncrement: an operator envelope rate 0-99 to a frequency */
static inline float pl_dx_operator_envelope_increment(int rate)
{
    int rate_scaled = (rate * 41) >> 6;
    int mantissa = 4 + (rate_scaled & 3);
    int exponent = 2 + (rate_scaled >> 2);
    return (float)(mantissa << exponent) / (float)(1 << 24);
}
/* PitchEnvelopeIncrement: a pitch envelope rate 0-99 to a frequency */
static inline float pl_dx_pitch_envelope_increment(int rate)
{
    float r = (float)rate * 0.01f;
    return (1.0f + 192.0f * r * (r * r * r * r + 0.3333f)) / (21.3f * 44100.0f);
}
#define PL_DX_MIN_LFO_FREQUENCY 0.005865f       /* kMinLFOFrequency */
/* LFOFrequency: an LFO rate 0-99 to a frequency */
static inline float pl_dx_lfo_frequency(int rate)
{
    int rate_scaled = rate == 0 ? 1 : (rate * 165) >> 6;
    rate_scaled *= rate_scaled < 160 ? 11 : (11 + ((rate_scaled - 160) >> 4));
    return (float)rate_scaled * PL_DX_MIN_LFO_FREQUENCY;
}
/* LFODelay: an LFO delay 0-99 to the two increments */
static inline void pl_dx_lfo_delay(int delay, float increments[2])
{
    if (delay == 0) {
        increments[0] = increments[1] = 100000.0f;
    } else {
        int d = 99 - delay;
        d = (16 + (d & 15)) << (1 + (d >> 4));
        increments[0] = (float)d * PL_DX_MIN_LFO_FREQUENCY;
        increments[1] = (float)pl_maxi(0x80, d & 0xff80) * PL_DX_MIN_LFO_FREQUENCY;
    }
}
/* NormalizeVelocity: pre-processes the velocity for the velocity scaling. (As in the C++, a velocity of exactly 1.0
 * reads one float past lut_cube_root's 17, times a fraction of 0.) */
static inline float pl_dx_normalize_velocity(float velocity)
{
    const float cube_root = pl_interpolate(pl_lut_cube_root, velocity, 16.0f);
    return 16.0f * (cube_root - 0.918f);
}
/* RateScaling: MIDI note to envelope increment ratio */
static inline float pl_dx_rate_scaling(float note, int rate_scaling)
{
    return pl_dx_pow2_fast(1, (float)rate_scaling * (note * 0.33333f - 7.0f) * 0.03125f);
}
static inline float pl_dx_amp_mod_sensitivity(int amp_mod_sensitivity) { return pl_lut_amp_mod_sensitivity[amp_mod_sensitivity]; }
static inline float pl_dx_pitch_mod_sensitivity(int pitch_mod_sensitivity) { return pl_lut_pitch_mod_sensitivity[pitch_mod_sensitivity]; }
/* KeyboardScaling: keyboard tracking to TL adjustment */
static inline float pl_dx_keyboard_scaling(float note, const pl_dx_keyboard_scaling_t *ks)
{
    const float x = note - (float)ks->break_point - 15.0f;
    const int curve = x > 0.0f ? ks->right_curve : ks->left_curve;
    float t = pl_fabsf(x);
    if (curve == 1 || curve == 2) {
        t = pl_minf(t * 0.010467f, 1.0f);
        t = t * t * t;
        t *= 96.0f;
    }
    if (curve < 2) t = -t;
    float depth = (float)(x > 0.0f ? ks->right_depth : ks->left_depth);
    return t * depth * 0.02677f;
}
/* FrequencyRatio */
static inline float pl_dx_frequency_ratio(const pl_dx_patch_operator_t *op)
{
    const float detune = op->mode == 0 && op->fine ? 1.0f + 0.01f * (float)op->fine : 1.0f;
    float base = op->mode == 0 ? pl_lut_coarse[op->coarse] : (float)((int)(op->coarse & 3) * 100 + op->fine) * 0.39864f;
    base += ((float)op->detune - 7.0f) * 0.015f;
    return pl_semitones_to_ratio_safe(base) * detune;
}

/* ------------------------------------------------------- envelope.h --- */
#define PL_DX_ENV_STAGES 4                      /* num_stages (NUM_STAGES) */
#define PL_DX_ENV_PREVIOUS_LEVEL (-100.0f)      /* PREVIOUS_LEVEL (an int enum the C++ compares with a float) */
typedef struct {
    int stage;
    float phase, start;
    float increment[PL_DX_ENV_STAGES], level[PL_DX_ENV_STAGES], scale;
} pl_dx_env_t;
static inline void pl_dx_env_init(pl_dx_env_t *e, float scale)
{
    e->scale = scale;
    e->stage = PL_DX_ENV_STAGES - 1;
    e->phase = 1.0f;
    e->start = 0.0f;
    for (int i = 0; i < PL_DX_ENV_STAGES; ++i) {
        e->increment[i] = 0.001f;
        e->level[i] = 1.0f / (float)(1 << i);
    }
    e->level[PL_DX_ENV_STAGES - 1] = 0.0f;
}
/* value(stage, phase, start_level); reshape: the template's reshape_ascending_segments */
static inline float pl_dx_env_value_at(const pl_dx_env_t *e, int reshape, int stage, float phase, float start_level)
{
    float from = start_level == PL_DX_ENV_PREVIOUS_LEVEL
        ? e->level[(stage - 1 + PL_DX_ENV_STAGES) % PL_DX_ENV_STAGES] : start_level;
    float to = e->level[stage];
    if (reshape && from < to) {
        from = pl_maxf(6.7f, from);
        to = pl_maxf(6.7f, to);
        phase *= (2.5f - phase) * 0.666667f;
    }
    return phase * (to - from) + from;
}
static inline float pl_dx_env_value(const pl_dx_env_t *e, int reshape) { return pl_dx_env_value_at(e, reshape, e->stage, e->phase, e->start); }
/* RenderAtSample (the release branch calls itself once, as the C++) */
static float pl_dx_env_render_at_sample(const pl_dx_env_t *e, int reshape, float t, const float gate_duration)
{
    if (t > gate_duration) {
        /* check how far we are into the release phase */
        const float phase = (t - gate_duration) * e->increment[PL_DX_ENV_STAGES - 1];
        return phase >= 1.0f
            ? e->level[PL_DX_ENV_STAGES - 1]
            : pl_dx_env_value_at(e, reshape, PL_DX_ENV_STAGES - 1, phase,
                                 pl_dx_env_render_at_sample(e, reshape, gate_duration, gate_duration));
    }
    int stage = 0;
    for (; stage < PL_DX_ENV_STAGES - 1; ++stage) {
        const float stage_duration = 1.0f / e->increment[stage];
        if (t < stage_duration) break;
        t -= stage_duration;
    }
    if (stage == PL_DX_ENV_STAGES - 1) {
        t -= gate_duration;
        if (t <= 0.0f) {
            /* TODO(pichenettes): this should always be true. */
            return e->level[PL_DX_ENV_STAGES - 2];
        } else if (t * e->increment[PL_DX_ENV_STAGES - 1] > 1.0f) {
            return e->level[PL_DX_ENV_STAGES - 1];
        }
    }
    return pl_dx_env_value_at(e, reshape, stage, t * e->increment[stage], PL_DX_ENV_PREVIOUS_LEVEL);
}
/* Render(gate, rate, ad_scale, release_scale) */
static float pl_dx_env_render(pl_dx_env_t *e, int reshape, int gate, float rate, float ad_scale, float release_scale)
{
    if (gate) {
        if (e->stage == PL_DX_ENV_STAGES - 1) {
            e->start = pl_dx_env_value(e, reshape);
            e->stage = 0;
            e->phase = 0.0f;
        }
    } else {
        if (e->stage != PL_DX_ENV_STAGES - 1) {
            e->start = pl_dx_env_value(e, reshape);
            e->stage = PL_DX_ENV_STAGES - 1;
            e->phase = 0.0f;
        }
    }
    e->phase += e->increment[e->stage] * rate * (e->stage == PL_DX_ENV_STAGES - 1 ? release_scale : ad_scale);
    if (e->phase >= 1.0f) {
        if (e->stage >= PL_DX_ENV_STAGES - 2) {
            e->phase = 1.0f;
        } else {
            e->phase = 0.0f;
            ++e->stage;
        }
        e->start = PL_DX_ENV_PREVIOUS_LEVEL;
    }
    return pl_dx_env_value(e, reshape);
}
/* OperatorEnvelope::Set (reshape 1) */
static void pl_dx_op_env_set(pl_dx_env_t *e, const uint8_t rate[PL_DX_ENV_STAGES], const uint8_t level[PL_DX_ENV_STAGES], uint8_t global_level)
{
    /* configure levels */
    for (int i = 0; i < PL_DX_ENV_STAGES; ++i) {
        int level_scaled = pl_dx_operator_level(level[i]);
        level_scaled = (level_scaled & ~1) + global_level - 133;    /* 125 ? */
        e->level[i] = 0.125f * (level_scaled < 1 ? 0.5f : (float)level_scaled);
    }
    /* configure increments */
    for (int i = 0; i < PL_DX_ENV_STAGES; ++i) {
        float increment = pl_dx_operator_envelope_increment(rate[i]);
        float from = e->level[(i - 1 + PL_DX_ENV_STAGES) % PL_DX_ENV_STAGES];
        float to = e->level[i];
        if (from == to) {
            /* quirk: for plateaux, the increment is scaled */
            increment *= 0.6f;
            if (i == 0 && !level[i]) increment *= 20.0f;       /* quirk: the attack plateau is faster */
        } else if (from < to) {
            from = pl_maxf(6.7f, from);
            to = pl_maxf(6.7f, to);
            if (from == to) increment = 1.0f;                  /* quirk: because of the jump, the attack might disappear */
            else increment *= 7.2f / (to - from);              /* quirk: because of the weird shape, the rate is adjusted */
        } else {
            increment *= 1.0f / (from - to);
        }
        e->increment[i] = increment * e->scale;
    }
}
/* PitchEnvelope::Set (reshape 0) */
static void pl_dx_pitch_env_set(pl_dx_env_t *e, const uint8_t rate[PL_DX_ENV_STAGES], const uint8_t level[PL_DX_ENV_STAGES])
{
    for (int i = 0; i < PL_DX_ENV_STAGES; ++i) e->level[i] = pl_dx_pitch_envelope_level(level[i]);
    for (int i = 0; i < PL_DX_ENV_STAGES; ++i) {
        float from = e->level[(i - 1 + PL_DX_ENV_STAGES) % PL_DX_ENV_STAGES];
        float to = e->level[i];
        float increment = pl_dx_pitch_envelope_increment(rate[i]);
        if (from != to) increment *= 1.0f / pl_fabsf(from - to);
        else if (i != PL_DX_ENV_STAGES - 1) increment = 0.2f;
        e->increment[i] = increment * e->scale;
    }
}

/* ------------------------------------------------------------ lfo.h --- */
enum { PL_DX_LFO_TRIANGLE, PL_DX_LFO_RAMP_DOWN, PL_DX_LFO_RAMP_UP, PL_DX_LFO_SQUARE, PL_DX_LFO_SINE, PL_DX_LFO_S_AND_H };
typedef struct {
    float phase, frequency, delay_phase, delay_increment[2], value, random_value, one_hz, amp_mod_depth, pitch_mod_depth;
    int waveform, reset_phase, phase_integral;
} pl_dx_lfo_t;
static inline void pl_dx_lfo_init(pl_dx_lfo_t *l, float sample_rate)
{
    l->phase = 0.0f;
    l->frequency = 0.1f;
    l->delay_phase = 0.0f;
    l->delay_increment[0] = l->delay_increment[1] = 0.1f;
    l->random_value = l->value = 0.0f;
    l->one_hz = 1.0f / sample_rate;
    l->amp_mod_depth = 0.0f;
    l->pitch_mod_depth = 0.0f;
    l->waveform = PL_DX_LFO_TRIANGLE;
    l->reset_phase = 0;
    l->phase_integral = 0;
}
static inline void pl_dx_lfo_set(pl_dx_lfo_t *l, const pl_dx_modulation_params_t *modulations)
{
    l->frequency = pl_dx_lfo_frequency(modulations->rate) * l->one_hz;
    pl_dx_lfo_delay(modulations->delay, l->delay_increment);
    l->delay_increment[0] *= l->one_hz;
    l->delay_increment[1] *= l->one_hz;
    l->waveform = modulations->waveform;
    l->reset_phase = modulations->reset_phase != 0;
    l->amp_mod_depth = (float)modulations->amp_mod_depth * 0.01f;
    l->pitch_mod_depth = (float)modulations->pitch_mod_depth * 0.01f * pl_dx_pitch_mod_sensitivity(modulations->pitch_mod_sensitivity);
}
static inline void pl_dx_lfo_reset(pl_dx_lfo_t *l)
{
    if (l->reset_phase) l->phase = 0.0f;
    l->delay_phase = 0.0f;
}
static inline float pl_dx_lfo_value(const pl_dx_lfo_t *l)
{
    switch (l->waveform) {
    case PL_DX_LFO_TRIANGLE: return 2.0f * (l->phase < 0.5f ? 0.5f - l->phase : l->phase - 0.5f);
    case PL_DX_LFO_RAMP_DOWN: return 1.0f - l->phase;
    case PL_DX_LFO_RAMP_UP: return l->phase;
    case PL_DX_LFO_SQUARE: return l->phase < 0.5f ? 0.0f : 1.0f;
    case PL_DX_LFO_SINE: return 0.5f + 0.5f * pl_sine(l->phase + 0.5f);
    case PL_DX_LFO_S_AND_H: return l->random_value;
    default: return 0.0f;
    }
}
static inline void pl_dx_lfo_step(pl_dx_lfo_t *l, float scale)
{
    l->phase += scale * l->frequency;
    if (l->phase >= 1.0f) {
        l->phase -= 1.0f;
        l->random_value = pl_rng_float();
    }
    l->value = pl_dx_lfo_value(l);
    l->delay_phase += scale * l->delay_increment[(l->delay_phase < 0.5f) ? 0 : 1];
    if (l->delay_phase >= 1.0f) l->delay_phase = 1.0f;
}
/* Scrub(sample): the LFO as it would be `sample` samples after a reset */
static inline void pl_dx_lfo_scrub(pl_dx_lfo_t *l, float sample)
{
    float phase = sample * l->frequency;
    PL_MAKE_INTEGRAL_FRACTIONAL(phase)
    l->phase = phase_fractional;
    if (phase_integral != l->phase_integral) {
        l->phase_integral = phase_integral;
        l->random_value = pl_rng_float();
    }
    l->value = pl_dx_lfo_value(l);
    l->delay_phase = sample * l->delay_increment[0];
    if (l->delay_phase > 0.5f) {
        sample -= 0.5f / l->delay_increment[0];
        l->delay_phase = 0.5f + sample * l->delay_increment[1];
        if (l->delay_phase >= 1.0f) l->delay_phase = 1.0f;
    }
}
static inline float pl_dx_lfo_delay_ramp(const pl_dx_lfo_t *l) { return l->delay_phase < 0.5f ? 0.0f : (l->delay_phase - 0.5f) * 2.0f; }
static inline float pl_dx_lfo_pitch_mod(const pl_dx_lfo_t *l) { return (l->value - 0.5f) * pl_dx_lfo_delay_ramp(l) * l->pitch_mod_depth; }
static inline float pl_dx_lfo_amp_mod(const pl_dx_lfo_t *l) { return (1.0f - l->value) * pl_dx_lfo_delay_ramp(l) * l->amp_mod_depth; }
