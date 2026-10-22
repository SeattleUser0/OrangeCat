/* SPDX-License-Identifier: MIT
 * Copyright 2012-2021 Emilie Gillet (Mutable Instruments stmlib and Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: the PLAITS engine's shared tools, ported from C++ to C (the firmware has no C++ runtime, no libc
 * and no doubles). Each function here does what its stmlib / Plaits namesake does, in the same order of operations,
 * so the port can be checked against the original sample for sample (tests/plaits_parity: PL_PARITY builds it at
 * Plaits' own 48 kHz).
 *
 *   stmlib/dsp/dsp.h            pl_interpolate, pl_interpolate_hermite, pl_interpolate_wrap, pl_smooth_step,
 *                               PL_ONE_POLE, PL_SLOPE, PL_SLEW, pl_crossfade, pl_soft_limit, pl_soft_clip, pl_sqrt
 *   stmlib/dsp/units.h          pl_semitones_to_ratio (+ _safe), pl_exp2_safe
 *   stmlib/dsp/parameter_interpolator.h   pl_interp_t: init, next, sub, done (done is the C++ destructor: call it)
 *   stmlib/dsp/polyblep.h       pl_this_blep .. pl_next_integrated_blep
 *   stmlib/utils/random.h       pl_rng_word, pl_rng_sample, pl_rng_float (one generator, as stmlib's static one)
 *   stmlib/dsp/filter.h         pl_onepole_t, pl_svf_t (PL_FREQ_DIRTY / FAST / ACCURATE approximations), pl_dcblock_t
 *   stmlib/dsp/limiter.h        pl_limiter_t
 *   stmlib/dsp/cosine_oscillator.h   pl_cosine_osc_t (the approximate mode)
 *   stmlib/dsp/rsqrt.h          pl_rsqrt_carmack, pl_rsqrt_accurate
 *   plaits/dsp/dsp.h, engine.h  PL_SAMPLE_RATE, PL_A0, PL_MAX_BLOCK, pl_note_to_frequency, pl_params_t, triggers
 *   (Orange Cat's own)          pl_flush and friends: tiny states set to 0 (below)
 *
 * Sample rate: the FM-1 runs at 44.1 kHz. Plaits' frequencies are fractions of the sample rate (pl_note_to_frequency),
 * so pitch is right at any rate; its time constants are per sample, so its envelopes and decays run 8.8 % longer than
 * on the module. PL_PARITY uses Plaits' own figures (48 kHz, corrected to 47,872.34 Hz).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "pl_tables.h"

#ifdef PL_PARITY
#define PL_SAMPLE_RATE 48000.0f
#define PL_CORRECTED_SAMPLE_RATE 47872.34f
#else
#define PL_SAMPLE_RATE 44100.0f
#define PL_CORRECTED_SAMPLE_RATE 44100.0f
#endif
#define PL_A0 ((440.0f / 8.0f) / PL_CORRECTED_SAMPLE_RATE)
#define PL_MAX_BLOCK 24u                        /* kMaxBlockSize: the most frames one render call takes */
#define PL_BLOCK 12u                            /* kBlockSize */
#define PL_PI 3.14159274101257f                 /* float(M_PI) */
#define PL_PI_F PL_PI

#define PL_CONSTRAIN(v, lo, hi) do { if ((v) < (lo)) (v) = (lo); else if ((v) > (hi)) (v) = (hi); } while (0)
static inline float pl_minf(float a, float b) { return a < b ? a : b; }
static inline float pl_maxf(float a, float b) { return a > b ? a : b; }
static inline float pl_fabsf(float x) { return x < 0.0f ? -x : x; }
static inline int32_t pl_mini(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t pl_maxi(int32_t a, int32_t b) { return a > b ? a : b; }

/* ------------------------------------------------------------ dsp.h --- */
#define PL_MAKE_INTEGRAL_FRACTIONAL(x) \
    int32_t x##_integral = (int32_t)(x); \
    float x##_fractional = (x) - (float)(x##_integral);

static inline float pl_interpolate(const float *table, float index, float size)
{
    index *= size;
    PL_MAKE_INTEGRAL_FRACTIONAL(index)
    float a = table[index_integral];
    float b = table[index_integral + 1];
    return a + (b - a) * index_fractional;
}
static inline float pl_interpolate_hermite(const float *table, float index, float size)
{
    index *= size;
    PL_MAKE_INTEGRAL_FRACTIONAL(index)
    const float xm1 = table[index_integral - 1];
    const float x0 = table[index_integral + 0];
    const float x1 = table[index_integral + 1];
    const float x2 = table[index_integral + 2];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = index_fractional;
    return (((a * f) - b_neg) * f + c) * f + x0;
}
static inline float pl_interpolate_wrap(const float *table, float index, float size)
{
    index -= (float)((int32_t)index);
    index *= size;
    PL_MAKE_INTEGRAL_FRACTIONAL(index)
    float a = table[index_integral];
    float b = table[index_integral + 1];
    return a + (b - a) * index_fractional;
}
static inline float pl_smooth_step(float value) { return value * value * (3.0f - 2.0f * value); }
#define PL_ONE_POLE(out, in, coefficient) (out) += (coefficient) * ((in) - (out))
#define PL_SLOPE(out, in, positive, negative) do { \
    float pl_error_ = (in) - (out); \
    (out) += (pl_error_ > 0 ? (positive) : (negative)) * pl_error_; \
} while (0)
#define PL_SLEW(out, in, delta) do { \
    float pl_error_ = (in) - (out); \
    float pl_d_ = (delta); \
    if (pl_error_ > pl_d_) pl_error_ = pl_d_; \
    else if (pl_error_ < -pl_d_) pl_error_ = -pl_d_; \
    (out) += pl_error_; \
} while (0)
static inline float pl_crossfade(float a, float b, float fade) { return a + (b - a) * fade; }
static inline float pl_soft_limit(float x) { return x * (27.0f + x * x) / (27.0f + 9.0f * x * x); }
static inline float pl_soft_clip(float x)
{
    if (x < -3.0f) return -1.0f;
    else if (x > 3.0f) return 1.0f;
    else return pl_soft_limit(x);
}
static inline int32_t pl_clip16(int32_t x) { return x < -32768 ? -32768 : x > 32767 ? 32767 : x; }

/* ---------------------------------------------------------- rsqrt.h --- */
typedef union { float f; uint32_t u; } pl_bits_t;
static inline float pl_rsqrt_carmack(float x)
{
    pl_bits_t b;
    float x2, y;
    const float threehalfs = 1.5f;
    b.f = x;
    b.u = 0x5f3759dfu - (b.u >> 1);
    y = b.f;
    x2 = x * 0.5f;
    y = y * (threehalfs - (x2 * y * y));
    return y;
}
static inline float pl_rsqrt_accurate(float fp0)
{
    const float min_ = 1.0e-38f, onep5 = 1.5f;
    float fp1, fp2, fp3;
    pl_bits_t b;
    b.f = fp0;
    b.u = 0x5F3997BBu - ((b.u >> 1) & 0x3FFFFFFFu);
    fp2 = b.f;
    fp1 = onep5 * fp0 - fp0;
    fp3 = fp2 * fp2;
    if (fp0 < min_) return fp0 > 0 ? fp2 : 1000.0f;
    fp3 = onep5 - fp1 * fp3;
    fp2 = fp2 * fp3;
    fp3 = fp2 * fp2;
    fp3 = onep5 - fp1 * fp3;
    fp2 = fp2 * fp3;
    fp3 = fp2 * fp2;
    fp3 = onep5 - fp1 * fp3;
    return fp2 * fp3;
}
/* stmlib::Sqrt (sqrtf on the host, vsqrt on the module). The FM-1 has no square root instruction we rely on: three
 * Newton steps of the reciprocal root, then one more on the root itself (within one or two units of the last place).
 * PL_PARITY uses the host's own, so the port compares exactly. */
#ifdef PL_PARITY
float sqrtf(float);
static inline float pl_sqrt(float x) { return sqrtf(x); }
#else
static inline float pl_sqrt(float x)
{
    if (x <= 0.0f) return 0.0f;
    float y = x * pl_rsqrt_accurate(x);
    return 0.5f * (y + x / y);
}
#endif

/* ---------------------------------------------------------- units.h --- */
static inline float pl_semitones_to_ratio(float semitones)
{
    float pitch = semitones + 128.0f;
    PL_MAKE_INTEGRAL_FRACTIONAL(pitch)
    return pl_lut_pitch_ratio_high[pitch_integral] * pl_lut_pitch_ratio_low[(int32_t)(pitch_fractional * 256.0f)];
}
static inline float pl_semitones_to_ratio_safe(float semitones)
{
    float scale = 1.0f;
    while (semitones > 120.0f) { semitones -= 120.0f; scale *= 1024.0f; }
    while (semitones < -120.0f) { semitones += 120.0f; scale *= 1.0f / 1024.0f; }
    return scale * pl_semitones_to_ratio(semitones);
}
static inline float pl_exp2_safe(float value) { return pl_semitones_to_ratio_safe(value * 12.0f); }

/* ------------------------------------------- parameter_interpolator.h --- */
typedef struct { float *state; float value, increment; } pl_interp_t;
static inline void pl_interp_init(pl_interp_t *p, float *state, float new_value, size_t size)
{
    p->state = state;
    p->value = *state;
    p->increment = (new_value - *state) / (float)size;
}
static inline void pl_interp_init_step(pl_interp_t *p, float *state, float new_value, float step)
{
    p->state = state;
    p->value = *state;
    p->increment = (new_value - *state) * step;
}
static inline float pl_interp_next(pl_interp_t *p) { p->value += p->increment; return p->value; }
static inline float pl_interp_sub(const pl_interp_t *p, float t) { return p->value + p->increment * t; }
static inline void pl_interp_done(pl_interp_t *p) { *p->state = p->value; }   /* (the destructor) */

/* --------------------------------------------------------- polyblep.h --- */
static inline float pl_this_blep(float t) { return 0.5f * t * t; }
static inline float pl_next_blep(float t) { t = 1.0f - t; return -0.5f * t * t; }
static inline float pl_next_integrated_blep(float t)
{
    const float t1 = 0.5f * t;
    const float t2 = t1 * t1;
    const float t4 = t2 * t2;
    return 0.1875f - t1 + 1.5f * t2 - t4;
}
static inline float pl_this_integrated_blep(float t) { return pl_next_integrated_blep(1.0f - t); }

/* ----------------------------------------------------------- random.h --- */
static uint32_t pl_rng_state = 0x21u;
static inline uint32_t pl_rng_word(void) { pl_rng_state = pl_rng_state * 1664525u + 1013904223u; return pl_rng_state; }
static inline int16_t pl_rng_sample(void) { return (int16_t)(pl_rng_word() >> 16); }
static inline float pl_rng_float(void) { return (float)pl_rng_word() / 4294967296.0f; }

/* ----------------------------------------------------------- filter.h --- */
enum { PL_FREQ_EXACT, PL_FREQ_ACCURATE, PL_FREQ_FAST, PL_FREQ_DIRTY };
enum { PL_LP, PL_BP, PL_BP_NORMALIZED, PL_HP };
/* OnePole::tan<approximation>; the constants are the C++ ones as it rounds them (float * double, then float) */
static inline float pl_tan_dirty(float f) { const float a = 11.58394527f; return f * (PL_PI_F + a * f * f); }
static inline float pl_tan_fast(float f)
{
    const float a = 10.10804653f, b = 55.78738785f;
    float f2 = f * f;
    return f * (PL_PI_F + f2 * (a + b * f2));
}
static inline float pl_tan_accurate(float f)
{
    const float a = 10.33536625f, b = 40.82069778f, c = 161.2053070f, d = 86.46203613f, e = 2799.883545f;
    float f2 = f * f;
    return f * (PL_PI_F + f2 * (a + f2 * (b + f2 * (c + f2 * (d + f2 * e)))));
}
static inline float pl_tan(int approximation, float f)
{
    return approximation == PL_FREQ_DIRTY ? pl_tan_dirty(f) : approximation == PL_FREQ_FAST ? pl_tan_fast(f)
         : pl_tan_accurate(f);   /* (FREQUENCY_EXACT, tanf, is not used by the models ported) */
}

typedef struct { float x, y, pole; } pl_dcblock_t;
static inline void pl_dcblock_init(pl_dcblock_t *d, float pole) { d->x = 0.0f; d->y = 0.0f; d->pole = pole; }
static inline void pl_dcblock_process(pl_dcblock_t *d, float *in_out, size_t size)
{
    float x = d->x, y = d->y;
    const float pole = d->pole;
    while (size--) {
        float old_x = x;
        x = *in_out;
        *in_out++ = y = y * pole + x - old_x;
    }
    d->x = x;
    d->y = y;
}

typedef struct { float g, gi, state; } pl_onepole_t;
static inline void pl_onepole_set_f(pl_onepole_t *o, int approximation, float f)
{
    o->g = pl_tan(approximation, f);
    o->gi = 1.0f / (1.0f + o->g);
}
static inline void pl_onepole_reset(pl_onepole_t *o) { o->state = 0.0f; }
static inline void pl_onepole_init(pl_onepole_t *o) { pl_onepole_set_f(o, PL_FREQ_DIRTY, 0.01f); pl_onepole_reset(o); }
static inline float pl_onepole_process(pl_onepole_t *o, int mode, float in)
{
    float lp;
    lp = (o->g * in + o->state) * o->gi;
    o->state = o->g * (in - lp) + lp;
    if (mode == PL_LP) return lp;
    else if (mode == PL_HP) return in - lp;
    else return 0.0f;
}
static inline void pl_onepole_process_buf(pl_onepole_t *o, int mode, float *in_out, size_t size)
{
    while (size--) { *in_out = pl_onepole_process(o, mode, *in_out); ++in_out; }
}

typedef struct { float g, r, h, state_1, state_2; } pl_svf_t;
static inline void pl_svf_set_g_r_h(pl_svf_t *s, float g, float r, float h) { s->g = g; s->r = r; s->h = h; }
static inline void pl_svf_set_g_r(pl_svf_t *s, float g, float r)
{
    s->g = g;
    s->r = r;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}
static inline void pl_svf_set_g_q(pl_svf_t *s, float g, float resonance)
{
    s->g = g;
    s->r = 1.0f / resonance;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}
static inline void pl_svf_set_f_q(pl_svf_t *s, int approximation, float f, float resonance)
{
    s->g = pl_tan(approximation, f);
    s->r = 1.0f / resonance;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}
static inline void pl_svf_set(pl_svf_t *s, const pl_svf_t *f) { s->g = f->g; s->r = f->r; s->h = f->h; }
static inline void pl_svf_reset(pl_svf_t *s) { s->state_1 = s->state_2 = 0.0f; }
static inline void pl_svf_init(pl_svf_t *s) { pl_svf_set_f_q(s, PL_FREQ_DIRTY, 0.01f, 100.0f); pl_svf_reset(s); }
/* one sample; mode PL_LP / PL_BP / PL_BP_NORMALIZED / PL_HP */
static inline float pl_svf_process(pl_svf_t *s, int mode, float in)
{
    float hp, bp, lp;
    hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
    bp = s->g * hp + s->state_1;
    s->state_1 = s->g * hp + bp;
    lp = s->g * bp + s->state_2;
    s->state_2 = s->g * bp + lp;
    return mode == PL_LP ? lp : mode == PL_BP ? bp : mode == PL_BP_NORMALIZED ? bp * s->r : hp;
}
/* Process<mode_1, mode_2>(in, &out_1, &out_2) */
static inline void pl_svf_process2(pl_svf_t *s, int mode_1, int mode_2, float in, float *out_1, float *out_2)
{
    float hp, bp, lp;
    hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
    bp = s->g * hp + s->state_1;
    s->state_1 = s->g * hp + bp;
    lp = s->g * bp + s->state_2;
    s->state_2 = s->g * bp + lp;
    *out_1 = mode_1 == PL_LP ? lp : mode_1 == PL_BP ? bp : mode_1 == PL_BP_NORMALIZED ? bp * s->r : hp;
    *out_2 = mode_2 == PL_LP ? lp : mode_2 == PL_BP ? bp : mode_2 == PL_BP_NORMALIZED ? bp * s->r : hp;
}
/* Process<mode>(in, out, size) (in and out may be the same buffer) */
static inline void pl_svf_process_buf(pl_svf_t *s, int mode, const float *in, float *out, size_t size)
{
    float hp, bp, lp, state_1 = s->state_1, state_2 = s->state_2;
    while (size--) {
        hp = (*in - s->r * state_1 - s->g * state_1 - state_2) * s->h;
        bp = s->g * hp + state_1;
        state_1 = s->g * hp + bp;
        lp = s->g * bp + state_2;
        state_2 = s->g * bp + lp;
        *out++ = mode == PL_LP ? lp : mode == PL_BP ? bp : mode == PL_BP_NORMALIZED ? bp * s->r : hp;
        ++in;
    }
    s->state_1 = state_1;
    s->state_2 = state_2;
}
/* ProcessAdd<mode>(in, out, size, gain) */
static inline void pl_svf_process_add(pl_svf_t *s, int mode, const float *in, float *out, size_t size, float gain)
{
    float hp, bp, lp, state_1 = s->state_1, state_2 = s->state_2;
    while (size--) {
        hp = (*in - s->r * state_1 - s->g * state_1 - state_2) * s->h;
        bp = s->g * hp + state_1;
        state_1 = s->g * hp + bp;
        lp = s->g * bp + state_2;
        state_2 = s->g * bp + lp;
        *out++ += gain * (mode == PL_LP ? lp : mode == PL_BP ? bp : mode == PL_BP_NORMALIZED ? bp * s->r : hp);
        ++in;
    }
    s->state_1 = state_1;
    s->state_2 = state_2;
}
/* Process<mode>(in, out, size, stride) */
static inline void pl_svf_process_stride(pl_svf_t *s, int mode, const float *in, float *out, size_t size, size_t stride)
{
    float hp, bp, lp, state_1 = s->state_1, state_2 = s->state_2;
    while (size--) {
        hp = (*in - s->r * state_1 - s->g * state_1 - state_2) * s->h;
        bp = s->g * hp + state_1;
        state_1 = s->g * hp + bp;
        lp = s->g * bp + state_2;
        state_2 = s->g * bp + lp;
        *out = mode == PL_LP ? lp : mode == PL_BP ? bp : mode == PL_BP_NORMALIZED ? bp * s->r : hp;
        out += stride;
        in += stride;
    }
    s->state_1 = state_1;
    s->state_2 = state_2;
}
static inline void pl_svf_process_multimode(pl_svf_t *s, const float *in, float *out, size_t size, float mode)
{
    float hp, bp, lp, state_1 = s->state_1, state_2 = s->state_2;
    float hp_gain = mode < 0.5f ? -mode * 2.0f : -2.0f + mode * 2.0f;
    float lp_gain = mode < 0.5f ? 1.0f - mode * 2.0f : 0.0f;
    float bp_gain = mode < 0.5f ? 0.0f : mode * 2.0f - 1.0f;
    while (size--) {
        hp = (*in - s->r * state_1 - s->g * state_1 - state_2) * s->h;
        bp = s->g * hp + state_1;
        state_1 = s->g * hp + bp;
        lp = s->g * bp + state_2;
        state_2 = s->g * bp + lp;
        *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
        ++in;
        ++out;
    }
    s->state_1 = state_1;
    s->state_2 = state_2;
}
static inline void pl_svf_process_multimode_lp_to_hp(pl_svf_t *s, const float *in, float *out, size_t size, float mode)
{
    float hp, bp, lp, state_1 = s->state_1, state_2 = s->state_2;
    float hp_gain = pl_minf(-mode * 2.0f + 1.0f, 0.0f);
    float bp_gain = 1.0f - 2.0f * pl_fabsf(mode - 0.5f);
    float lp_gain = pl_maxf(1.0f - mode * 2.0f, 0.0f);
    while (size--) {
        hp = (*in - s->r * state_1 - s->g * state_1 - state_2) * s->h;
        bp = s->g * hp + state_1;
        state_1 = s->g * hp + bp;
        lp = s->g * bp + state_2;
        state_2 = s->g * bp + lp;
        *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
        ++in;
        ++out;
    }
    s->state_1 = state_1;
    s->state_2 = state_2;
}

/* ---------------------------------------------------------- limiter.h --- */
typedef struct { float peak; } pl_limiter_t;
static inline void pl_limiter_init(pl_limiter_t *l) { l->peak = 0.5f; }
static inline void pl_limiter_process(pl_limiter_t *l, float pre_gain, float *in_out, size_t size)
{
    while (size--) {
        float s = *in_out * pre_gain;
        PL_SLOPE(l->peak, pl_fabsf(s), 0.05f, 0.00002f);
        float gain = (l->peak <= 1.0f ? 1.0f : 1.0f / l->peak);
        *in_out++ = s * gain * 0.8f;
    }
}

/* ------------------------------------------------ cosine_oscillator.h --- */
typedef struct { float y1, y0, iir_coefficient, initial_amplitude; } pl_cosine_osc_t;
static inline void pl_cosine_osc_start(pl_cosine_osc_t *c) { c->y1 = c->initial_amplitude; c->y0 = 0.5f; }
static inline void pl_cosine_osc_init_approximate(pl_cosine_osc_t *c, float frequency)
{
    float sign = 16.0f;
    frequency -= 0.25f;
    if (frequency < 0.0f) {
        frequency = -frequency;
    } else {
        if (frequency > 0.5f) frequency -= 0.5f;
        else sign = -16.0f;
    }
    c->iir_coefficient = sign * frequency * (1.0f - 2.0f * frequency);
    c->initial_amplitude = c->iir_coefficient * 0.25f;
    pl_cosine_osc_start(c);       /* (Init<COSINE_OSCILLATOR_APPROXIMATE> starts it) */
}
static inline float pl_cosine_osc_value(const pl_cosine_osc_t *c) { return c->y1 + 0.5f; }
static inline float pl_cosine_osc_next(pl_cosine_osc_t *c)
{
    float temp = c->y0;
    c->y0 = c->iir_coefficient * c->y0 - c->y1;
    c->y1 = temp;
    return temp + 0.5f;
}

/* ---------------------------------------------------- plaits engine.h --- */
enum { PL_TRIGGER_LOW = 0, PL_TRIGGER_RISING_EDGE = 1, PL_TRIGGER_UNPATCHED = 2, PL_TRIGGER_HIGH = 4 };
typedef struct {
    int trigger;
    float note;                                 /* MIDI note number (60 = C4) */
    float timbre, morph, harmonics, accent;     /* 0..1 */
} pl_params_t;
static inline float pl_note_to_frequency(float midi_note)
{
    midi_note -= 9.0f;
    PL_CONSTRAIN(midi_note, -128.0f, 127.0f);
    return PL_A0 * 0.25f * pl_semitones_to_ratio(midi_note);
}

/* ------------------------------------------------- (Orange Cat) flushes --- */
/* Not in the original. A decaying filter or envelope ends in denormal numbers (below 1.2e-38), which many FPUs (and
 * the host's) handle many times slower than normal ones; Melodee clears its own tails for the same reason (fx.c).
 * Each model has a pl_X_flush that sets its decaying states to 0 once they are below 2^-97 (6e-30, 580 dB under full
 * scale): eng_macro.c calls it after each render. The sound does not change (tests/plaits_parity with
 * PARITY_FLUSH=1 runs the original against the port with the flushes). */
static inline void pl_flush(float *x)
{
    pl_bits_t b;
    b.f = *x;
    if ((b.u & 0x7F800000u) < (30u << 23)) *x = 0.0f;
}
static inline void pl_flush_n(float *x, size_t n) { while (n--) pl_flush(x++); }
static inline void pl_flush_svf(pl_svf_t *s) { pl_flush(&s->state_1); pl_flush(&s->state_2); }
static inline void pl_flush_onepole(pl_onepole_t *o) { pl_flush(&o->state); }
