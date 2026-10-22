/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 11, STRING (plaits/dsp/engine/string_engine.cc), with its voice and string
 * (physical_modelling/string_voice.cc, string.cc, delay_line.h) and noise/dust.h (pl_grp_c.h), in C.
 *   Three Karplus-Strong strings, played in turn by each trigger (or bowed by dust when the trigger is unpatched).
 *   HARMONICS: the non-linearity (curved bridge below 0.25, dispersion above), TIMBRE: brightness, MORPH: damping.
 *   OUT: the strings. AUX: their excitation.
 *
 * Device difference: each string has a 1024-sample delay line plus a 256-sample allpass ("stretch") line. In float
 * that is 15 KB for the three strings, over the 12 KB a track's engine may use. Without PL_PARITY the two lines store
 * int16_t samples (pl_string_sample_t), written by pl_string_store (scaled by PL_STRING_STORE_SCALE, rounded,
 * saturated) and read back to float by pl_string_load; all the arithmetic stays float. PL_PARITY stores floats and
 * is the C++ exactly. Same voices, same lengths, same code in both builds. The scale and its limit are noted below.
 *
 * Revisions:
 *   0.5.0: first version.
 *   0.5.2: pl_harp_t, for MACRO's HARP model (Orange Cat's own, not the module's): one of these strings alone, so
 *          that each of a track's four voices can have one (2.7 KB each; STRING's three take 8 KB). The string and
 *          its excitation are the same code as STRING's.
 */
#pragma once
#include "pl_grp_c.h"

#define PL_STRING_NUM_STRINGS 3                  /* kNumStrings */
#define PL_STRING_DELAY_SIZE 1024u               /* kDelayLineSize */
#define PL_STRING_STRETCH_SIZE (PL_STRING_DELAY_SIZE / 4u)
#define PL_STRING_F0_DELAY_SIZE 16u

/* ------------------------------------------------- delay line storage --- */
#ifdef PL_PARITY
typedef float pl_string_sample_t;
static inline float pl_string_load(pl_string_sample_t s) { return s; }
static inline pl_string_sample_t pl_string_store(float x) { return x; }
#else
/* int16 storage, 16384 per unit: a range of +/-2.0 and a step of 6.1e-5.
 * Measured (host, 48 kHz, the parity scenarios): the largest |value| written was 0.777 in the string line and 0.593
 * in the stretch line, so +/-2.0 leaves 2.6x headroom. 1000 random struck settings (accent up to 1) wrote at most
 * 1.34. Struck and plucked, the int16 version follows the float one within -58 dB (error to signal RMS).
 * Known limit: bowed (trigger unpatched) with a high TIMBRE, the C++ loop runs up to its +/-20 clamp (about 30 % of
 * random bowed settings go past +/-2). Here those samples saturate at +/-2.0, so such a setting is up to about 20 dB
 * quieter than the C++ (which is far over full scale there anyway). A scale of 1024 would cover +/-32 but makes the
 * normal case 16 times coarser (struck error up to -17 dB in the parity's extreme-note scenario). */
typedef int16_t pl_string_sample_t;
#define PL_STRING_STORE_SCALE 16384.0f
static inline float pl_string_load(pl_string_sample_t s) { return (float)s * (1.0f / PL_STRING_STORE_SCALE); }
static inline pl_string_sample_t pl_string_store(float x)
{
    x *= PL_STRING_STORE_SCALE;
    if (x >= 32767.0f) return 32767;
    if (x <= -32767.0f) return -32767;
    return (pl_string_sample_t)(int32_t)(x < 0.0f ? x - 0.5f : x + 0.5f);   /* (round to nearest) */
}
#endif

/* DelayLine<pl_string_sample_t, size> (delay_line.h). size is a power of two, so `% size` becomes `& (size - 1)`. */
static inline void pl_string_dl_reset(pl_string_sample_t *line, uint32_t *write_ptr, uint32_t size)
{
    uint32_t i;
    for (i = 0; i < size; ++i) line[i] = pl_string_store(0.0f);
    *write_ptr = 0;
}
static inline void pl_string_dl_write(pl_string_sample_t *line, uint32_t *write_ptr, uint32_t size, float sample)
{
    line[*write_ptr] = pl_string_store(sample);
    *write_ptr = (*write_ptr - 1u + size) & (size - 1u);
}
static inline float pl_string_dl_allpass(pl_string_sample_t *line, uint32_t *write_ptr, uint32_t size, float sample,
                                         uint32_t delay, float coefficient)
{
    float read = pl_string_load(line[(*write_ptr + delay) & (size - 1u)]);
    float write = sample + coefficient * read;
    pl_string_dl_write(line, write_ptr, size, write);
    return -write * coefficient + read;
}
static inline float pl_string_dl_read(const pl_string_sample_t *line, uint32_t write_ptr, uint32_t size, float delay)
{
    PL_MAKE_INTEGRAL_FRACTIONAL(delay)
    const float a = pl_string_load(line[(write_ptr + (uint32_t)delay_integral) & (size - 1u)]);
    const float b = pl_string_load(line[(write_ptr + (uint32_t)delay_integral + 1u) & (size - 1u)]);
    return a + (b - a) * delay_fractional;
}
static inline float pl_string_dl_read_hermite(const pl_string_sample_t *line, uint32_t write_ptr, uint32_t size,
                                              float delay)
{
    PL_MAKE_INTEGRAL_FRACTIONAL(delay)
    uint32_t t = write_ptr + (uint32_t)delay_integral + size;
    const float xm1 = pl_string_load(line[(t - 1u) & (size - 1u)]);
    const float x0 = pl_string_load(line[(t) & (size - 1u)]);
    const float x1 = pl_string_load(line[(t + 1u) & (size - 1u)]);
    const float x2 = pl_string_load(line[(t + 2u) & (size - 1u)]);
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = delay_fractional;
    return (((a * f) - b_neg) * f + c) * f + x0;
}

/* ------------------------------------------------------------ string.cc --- */
enum { PL_STRING_NON_LINEARITY_CURVED_BRIDGE, PL_STRING_NON_LINEARITY_DISPERSION };
typedef struct {
    pl_string_sample_t string[PL_STRING_DELAY_SIZE];
    pl_string_sample_t stretch[PL_STRING_STRETCH_SIZE];
    uint32_t string_write_ptr, stretch_write_ptr;
    pl_svf_t iir_damping_filter;
    pl_dcblock_t dc_blocker;
    float delay, dispersion_noise, curved_bridge;
    float src_phase;                            /* (the "very crappy" upsampler for notes below 11.7 Hz) */
    float out_sample[2];
} pl_string_str_t;

static void pl_string_str_reset(pl_string_str_t *s)
{
    pl_string_dl_reset(s->string, &s->string_write_ptr, PL_STRING_DELAY_SIZE);
    pl_string_dl_reset(s->stretch, &s->stretch_write_ptr, PL_STRING_STRETCH_SIZE);
    pl_svf_init(&s->iir_damping_filter);
    pl_dcblock_init(&s->dc_blocker, 1.0f - 20.0f / PL_SAMPLE_RATE);
    s->dispersion_noise = 0.0f;
    s->curved_bridge = 0.0f;
    s->out_sample[0] = s->out_sample[1] = 0.0f;
    s->src_phase = 0.0f;
}
static void pl_string_str_init(pl_string_str_t *s)
{
    s->delay = 100.0f;
    pl_string_str_reset(s);
}

/* ProcessInternal<non_linearity> */
static void pl_string_str_process_internal(pl_string_str_t *s, int non_linearity, float f0, float non_linearity_amount,
                                           float brightness, float damping, const float *in, float *out, size_t size)
{
    float delay = 1.0f / f0;
    PL_CONSTRAIN(delay, 4.0f, (float)PL_STRING_DELAY_SIZE - 4.0f);

    /* Not enough delay time (f0 below 11.7 Hz): play the lowest note and upsample with a linear interpolator. */
    float src_ratio = delay * f0;
    if (src_ratio >= 0.9999f) {
        s->src_phase = 1.0f;
        src_ratio = 1.0f;
    }

    float damping_cutoff = pl_minf(12.0f + damping * damping * 60.0f + brightness * 24.0f, 84.0f);
    float damping_f = pl_minf(f0 * pl_semitones_to_ratio(damping_cutoff), 0.499f);

    /* Crossfade to infinite decay. */
    if (damping >= 0.95f) {
        float to_infinite = 20.0f * (damping - 0.95f);
        brightness += to_infinite * (1.0f - brightness);
        damping_f += to_infinite * (0.4999f - damping_f);
        damping_cutoff += to_infinite * (128.0f - damping_cutoff);
    }

    pl_svf_set_f_q(&s->iir_damping_filter, PL_FREQ_FAST, damping_f, 0.5f);

    float damping_compensation = pl_interpolate(pl_lut_svf_shift, damping_cutoff, 1.0f);

    /* Linearly interpolate delay time. */
    pl_interp_t delay_modulation;
    pl_interp_init(&delay_modulation, &s->delay, delay * damping_compensation, size);

    float stretch_point = non_linearity_amount * (2.0f - non_linearity_amount) * 0.225f;
    float stretch_correction = (160.0f / PL_SAMPLE_RATE) * delay;
    PL_CONSTRAIN(stretch_correction, 1.0f, 2.1f);

    float noise_amount_sqrt = non_linearity_amount > 0.75f ? 4.0f * (non_linearity_amount - 0.75f) : 0.0f;
    float noise_amount = noise_amount_sqrt * noise_amount_sqrt * 0.1f;
    float noise_filter = 0.06f + 0.94f * brightness * brightness;

    float bridge_curving_sqrt = non_linearity_amount;
    float bridge_curving = bridge_curving_sqrt * bridge_curving_sqrt * 0.01f;

    float ap_gain = -0.618f * non_linearity_amount / (0.15f + pl_fabsf(non_linearity_amount));

    while (size--) {
        s->src_phase += src_ratio;
        if (s->src_phase > 1.0f) {
            s->src_phase -= 1.0f;

            float d = pl_interp_next(&delay_modulation);     /* (the C++'s inner `delay`) */
            float x = 0.0f;

            if (non_linearity == PL_STRING_NON_LINEARITY_DISPERSION) {
                float noise = pl_rng_float() - 0.5f;
                PL_ONE_POLE(s->dispersion_noise, noise, noise_filter);
                d *= 1.0f + s->dispersion_noise * noise_amount;
            } else {
                d *= 1.0f - s->curved_bridge * bridge_curving;
            }

            if (non_linearity == PL_STRING_NON_LINEARITY_DISPERSION) {
                float ap_delay = d * stretch_point;
                float main_delay = d - ap_delay * (0.408f - stretch_point * 0.308f) * stretch_correction;
                if (ap_delay >= 4.0f && main_delay >= 4.0f) {
                    x = pl_string_dl_read(s->string, s->string_write_ptr, PL_STRING_DELAY_SIZE, main_delay);
                    /* (Allpass takes a size_t delay: the float truncates, as here) */
                    x = pl_string_dl_allpass(s->stretch, &s->stretch_write_ptr, PL_STRING_STRETCH_SIZE, x,
                                             (uint32_t)(int32_t)ap_delay, ap_gain);
                } else {
                    x = pl_string_dl_read_hermite(s->string, s->string_write_ptr, PL_STRING_DELAY_SIZE, d);
                }
            } else {
                x = pl_string_dl_read_hermite(s->string, s->string_write_ptr, PL_STRING_DELAY_SIZE, d);
            }

            if (non_linearity == PL_STRING_NON_LINEARITY_CURVED_BRIDGE) {
                float value = pl_fabsf(x) - 0.025f;
                float sign = x > 0.0f ? 1.0f : -1.5f;
                s->curved_bridge = (pl_fabsf(value) + value) * sign;
            }

            x += *in;
            PL_CONSTRAIN(x, -20.0f, +20.0f);

            pl_dcblock_process(&s->dc_blocker, &x, 1);
            x = pl_svf_process(&s->iir_damping_filter, PL_LP, x);
            pl_string_dl_write(s->string, &s->string_write_ptr, PL_STRING_DELAY_SIZE, x);

            s->out_sample[1] = s->out_sample[0];
            s->out_sample[0] = x;
        }
        *out++ += pl_crossfade(s->out_sample[1], s->out_sample[0], s->src_phase);
        in++;
    }
    pl_interp_done(&delay_modulation);
}

static void pl_string_str_process(pl_string_str_t *s, float f0, float non_linearity_amount, float brightness,
                                  float damping, const float *in, float *out, size_t size)
{
    if (non_linearity_amount <= 0.0f)
        pl_string_str_process_internal(s, PL_STRING_NON_LINEARITY_CURVED_BRIDGE, f0, -non_linearity_amount,
                                       brightness, damping, in, out, size);
    else
        pl_string_str_process_internal(s, PL_STRING_NON_LINEARITY_DISPERSION, f0, non_linearity_amount,
                                       brightness, damping, in, out, size);
}

/* ------------------------------------------------------ string_voice.cc --- */
typedef struct {
    pl_svf_t excitation_filter;
    pl_string_str_t string;
    size_t remaining_noise_samples;
} pl_string_voice_t;

static void pl_string_voice_init(pl_string_voice_t *v)
{
    pl_svf_init(&v->excitation_filter);
    pl_string_str_init(&v->string);
    v->remaining_noise_samples = 0;
}
static void pl_string_voice_reset(pl_string_voice_t *v) { pl_string_str_reset(&v->string); }

static void pl_string_voice_render(pl_string_voice_t *v, int sustain, int trigger, float accent, float f0,
                                   float structure, float brightness, float damping, float *temp, float *out,
                                   float *aux, size_t size)
{
    const float density = brightness * brightness;
    size_t i;

    brightness += 0.25f * accent * (1.0f - brightness);
    damping += 0.25f * accent * (1.0f - damping);

    /* Synthesize excitation signal. */
    if (trigger || sustain) {
        const float range = 72.0f;
        const float f = 4.0f * f0;
        const float cutoff = pl_minf(f * pl_semitones_to_ratio((brightness * (2.0f - brightness) - 0.5f) * range),
                                     0.499f);
        const float q = sustain ? 1.0f : 0.5f;
        /* (static_cast<size_t>(1.0f / f0); through int32_t so the device needs no float to unsigned helper. The
         * value is at most a few thousand.) */
        v->remaining_noise_samples = (size_t)(int32_t)(1.0f / f0);
        pl_svf_set_f_q(&v->excitation_filter, PL_FREQ_DIRTY, cutoff, q);
    }

    if (sustain) {
        const float dust_f = 0.00005f + 0.99995f * density * density;
        for (i = 0; i < size; ++i) temp[i] = plc_dust(dust_f) * (8.0f - dust_f * 6.0f) * accent;
    } else if (v->remaining_noise_samples) {
        size_t noise_samples = v->remaining_noise_samples < size ? v->remaining_noise_samples : size;
        v->remaining_noise_samples -= noise_samples;
        size_t tail = size - noise_samples;
        float *start = temp;
        while (noise_samples--) *start++ = 2.0f * pl_rng_float() - 1.0f;
        while (tail--) *start++ = 0.0f;
    } else {
        for (i = 0; i < size; ++i) temp[i] = 0.0f;
    }

    pl_svf_process_buf(&v->excitation_filter, PL_LP, temp, temp, size);
    for (i = 0; i < size; ++i) aux[i] += temp[i];

    float non_linearity = structure < 0.24f ? (structure - 0.24f) * 4.166f
                        : (structure > 0.26f ? (structure - 0.26f) * 1.35135f : 0.0f);
    pl_string_str_process(&v->string, f0, non_linearity, brightness, damping, temp, out, size);
}

/* ----------------------------------------------------- string_engine.cc --- */
typedef struct {
    pl_string_voice_t voice[PL_STRING_NUM_STRINGS];
    float f0[PL_STRING_NUM_STRINGS];
    float f0_delay[PL_STRING_F0_DELAY_SIZE];    /* DelayLine<float, 16> f0_delay_ */
    uint32_t f0_delay_write_ptr;
    int active_string;
    float temp_buffer[PL_MAX_BLOCK];
} pl_string_t;

static void pl_string_init(pl_string_t *e)
{
    int i;
    for (i = 0; i < PL_STRING_NUM_STRINGS; ++i) {
        pl_string_voice_init(&e->voice[i]);
        e->f0[i] = 0.01f;
    }
    e->active_string = PL_STRING_NUM_STRINGS - 1;
    plc_delay_reset(e->f0_delay, &e->f0_delay_write_ptr, PL_STRING_F0_DELAY_SIZE);
}
static void pl_string_reset(pl_string_t *e)
{
    int i;
    plc_delay_reset(e->f0_delay, &e->f0_delay_write_ptr, PL_STRING_F0_DELAY_SIZE);
    for (i = 0; i < PL_STRING_NUM_STRINGS; ++i) pl_string_voice_reset(&e->voice[i]);
}

static void pl_string_render(pl_string_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                             int *already_enveloped)
{
    size_t i;
    int s;
    (void)already_enveloped;
    if (p->trigger & PL_TRIGGER_RISING_EDGE) {
        /* 8 in original firmware version. 05.01.18: mic.w: problem with microbrute. */
        e->f0[e->active_string] = plc_delay_read(e->f0_delay, e->f0_delay_write_ptr, PL_STRING_F0_DELAY_SIZE, 14.0f);
        e->active_string = (e->active_string + 1) % PL_STRING_NUM_STRINGS;
    }

    const float f0 = pl_note_to_frequency(p->note);
    e->f0[e->active_string] = f0;
    plc_delay_write(e->f0_delay, &e->f0_delay_write_ptr, PL_STRING_F0_DELAY_SIZE, f0);

    for (i = 0; i < size; ++i) out[i] = 0.0f;
    for (i = 0; i < size; ++i) aux[i] = 0.0f;

    for (s = 0; s < PL_STRING_NUM_STRINGS; ++s) {
        pl_string_voice_render(&e->voice[s],
                               (p->trigger & PL_TRIGGER_UNPATCHED) && s == e->active_string,
                               (p->trigger & PL_TRIGGER_RISING_EDGE) && s == e->active_string,
                               p->accent, e->f0[s], p->harmonics, p->timbre * p->timbre, p->morph,
                               e->temp_buffer, out, aux, size);
    }
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_string_flush(pl_string_t *e)
{
    for (int i = 0; i < PL_STRING_NUM_STRINGS; i++) {
        pl_string_voice_t *v = &e->voice[i];
        pl_flush_svf(&v->excitation_filter);
        pl_flush_svf(&v->string.iir_damping_filter);
        pl_flush(&v->string.dc_blocker.x);
        pl_flush(&v->string.dc_blocker.y);
        pl_flush_n(v->string.out_sample, 2);
    }
}

/* --------------------------------------------- (Orange Cat 0.5.2) HARP --- */
/* One string of STRING's, for a voice of its own: the engine's four voices are four strings, so a chord rings on four
 * strings at once (STRING plays its three in turn, one note at a time). No f0 delay or string rotation: a new note on
 * a voice restarts its string at once (the voice's ADSR covers the step). Same parameters as STRING. */
typedef struct {
    pl_string_voice_t voice;
    float temp_buffer[PL_MAX_BLOCK];
} pl_harp_t;

static void pl_harp_init(pl_harp_t *e) { pl_string_voice_init(&e->voice); }
static void pl_harp_reset(pl_harp_t *e) { pl_string_voice_reset(&e->voice); }
static void pl_harp_render(pl_harp_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                           int *already_enveloped)
{
    size_t i;
    (void)already_enveloped;
    for (i = 0; i < size; ++i) out[i] = 0.0f;
    for (i = 0; i < size; ++i) aux[i] = 0.0f;
    pl_string_voice_render(&e->voice, (p->trigger & PL_TRIGGER_UNPATCHED) != 0,
                           (p->trigger & PL_TRIGGER_RISING_EDGE) != 0, p->accent, pl_note_to_frequency(p->note),
                           p->harmonics, p->timbre * p->timbre, p->morph, e->temp_buffer, out, aux, size);
}
static inline void pl_harp_flush(pl_harp_t *e)
{
    pl_string_voice_t *v = &e->voice;
    pl_flush_svf(&v->excitation_filter);
    pl_flush_svf(&v->string.iir_damping_filter);
    pl_flush(&v->string.dc_blocker.x);
    pl_flush(&v->string.dc_blocker.y);
    pl_flush_n(v->string.out_sample, 2);
}
