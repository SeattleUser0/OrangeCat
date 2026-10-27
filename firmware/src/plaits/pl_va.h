/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 1, VIRTUAL ANALOG (plaits/dsp/engine/virtual_analog_engine.cc, VA_VARIANT 2, the
 * firmware's), with its oscillators (oscillator/variable_shape_oscillator.h, variable_saw_oscillator.h), in C.
 *   OUT: a variable square (TIMBRE: its width, then hard sync) plus a variable saw (MORPH), detuned by HARMONICS.
 *   AUX: two variable-shape oscillators in "monster" sync (MORPH: the shape, TIMBRE: the sync).
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

/* ------------------------------------------ variable_saw_oscillator.h --- */
#define PL_VARIABLE_SAW_NOTCH_DEPTH 0.2f
typedef struct { float phase, next_sample, previous_pw; int high; float frequency, pw, waveshape; } pl_vsaw_t;
static inline void pl_vsaw_init(pl_vsaw_t *o)
{
    o->phase = 0.0f;
    o->next_sample = 0.0f;
    o->previous_pw = 0.5f;
    o->high = 0;
    o->frequency = 0.01f;
    o->pw = 0.5f;
    o->waveshape = 0.0f;
}
static inline float pl_vsaw_naive(float phase, float pw, float slope_up, float slope_down, float triangle_amount, float notch_amount)
{
    float notch_saw = phase < pw ? phase : 1.0f + PL_VARIABLE_SAW_NOTCH_DEPTH;
    float triangle = phase < pw ? phase * slope_up : 1.0f - (phase - pw) * slope_down;
    return notch_saw * notch_amount + triangle * triangle_amount;
}
static __attribute__((section(".dsp_text"))) void pl_vsaw_render(pl_vsaw_t *o, float frequency, float pw, float waveshape, float *out, size_t size)
{
    if (frequency >= PL_MAX_FREQUENCY) frequency = PL_MAX_FREQUENCY;
    if (frequency >= 0.25f) pw = 0.5f;
    else PL_CONSTRAIN(pw, frequency * 2.0f, 1.0f - 2.0f * frequency);
    pl_interp_t fm, pwm, wsm;
    pl_interp_init(&fm, &o->frequency, frequency, size);
    pl_interp_init(&pwm, &o->pw, pw, size);
    pl_interp_init(&wsm, &o->waveshape, waveshape, size);
    float next_sample = o->next_sample;
    while (size--) {
        float this_sample = next_sample;
        next_sample = 0.0f;
        const float f = pl_interp_next(&fm);
        const float w = pl_interp_next(&pwm);
        const float ws = pl_interp_next(&wsm);
        const float triangle_amount = ws;
        const float notch_amount = 1.0f - ws;
        const float slope_up = 1.0f / (w);
        const float slope_down = 1.0f / (1.0f - w);
        o->phase += f;
        if (!o->high && o->phase >= w) {
            const float triangle_step = (slope_up + slope_down) * f * triangle_amount;
            const float notch = (PL_VARIABLE_SAW_NOTCH_DEPTH + 1.0f - w) * notch_amount;
            const float t = (o->phase - w) / (o->previous_pw - w + f);
            this_sample += notch * pl_this_blep(t);
            next_sample += notch * pl_next_blep(t);
            this_sample -= triangle_step * pl_this_integrated_blep(t);
            next_sample -= triangle_step * pl_next_integrated_blep(t);
            o->high = 1;
        } else if (o->phase >= 1.0f) {
            o->phase -= 1.0f;
            const float triangle_step = (slope_up + slope_down) * f * triangle_amount;
            const float notch = (PL_VARIABLE_SAW_NOTCH_DEPTH + 1.0f) * notch_amount;
            const float t = o->phase / f;
            this_sample -= notch * pl_this_blep(t);
            next_sample -= notch * pl_next_blep(t);
            this_sample += triangle_step * pl_this_integrated_blep(t);
            next_sample += triangle_step * pl_next_integrated_blep(t);
            o->high = 0;
        }
        next_sample += pl_vsaw_naive(o->phase, w, slope_up, slope_down, triangle_amount, notch_amount);
        o->previous_pw = w;
        *out++ = (2.0f * this_sample - 1.0f) / (1.0f + PL_VARIABLE_SAW_NOTCH_DEPTH);
    }
    o->next_sample = next_sample;
    pl_interp_done(&fm);
    pl_interp_done(&pwm);
    pl_interp_done(&wsm);
}

/* ---------------------------------------- variable_shape_oscillator.h --- */
typedef struct {
    float master_phase, slave_phase, next_sample, previous_pw;
    int high;
    float master_frequency, slave_frequency, pw, waveshape, phase_modulation;
} pl_vshape_t;
static inline void pl_vshape_init(pl_vshape_t *o)
{
    o->master_phase = 0.0f;
    o->slave_phase = 0.0f;
    o->next_sample = 0.0f;
    o->previous_pw = 0.5f;
    o->high = 0;
    o->master_frequency = 0.0f;
    o->slave_frequency = 0.01f;
    o->pw = 0.5f;
    o->waveshape = 0.0f;
    o->phase_modulation = 0.0f;
}
static inline float pl_vshape_naive(float phase, float pw, float slope_up, float slope_down, float triangle_amount, float square_amount)
{
    float saw = phase;
    float square = phase < pw ? 0.0f : 1.0f;
    float triangle = phase < pw ? phase * slope_up : 1.0f - (phase - pw) * slope_down;
    saw += (square - saw) * square_amount;
    saw += (triangle - saw) * triangle_amount;
    return saw;
}
/* Render<enable_sync, output_phase> */
static __attribute__((section(".dsp_text"))) void pl_vshape_render_t(pl_vshape_t *o, int enable_sync, int output_phase, float master_frequency, float frequency,
                               float pw, float waveshape, float phase_modulation_amount, float *out, size_t size)
{
    if (master_frequency >= PL_MAX_FREQUENCY) master_frequency = PL_MAX_FREQUENCY;
    if (frequency >= PL_MAX_FREQUENCY) frequency = PL_MAX_FREQUENCY;
    if (frequency >= 0.25f) pw = 0.5f;
    else PL_CONSTRAIN(pw, frequency * 2.0f, 1.0f - 2.0f * frequency);
    pl_interp_t master_fm, fm, pwm, wsm, pmm;
    pl_interp_init(&master_fm, &o->master_frequency, master_frequency, size);
    pl_interp_init(&fm, &o->slave_frequency, frequency, size);
    pl_interp_init(&pwm, &o->pw, pw, size);
    pl_interp_init(&wsm, &o->waveshape, waveshape, size);
    pl_interp_init(&pmm, &o->phase_modulation, phase_modulation_amount, size);
    float next_sample = o->next_sample;
    while (size--) {
        int reset = 0, transition_during_reset = 0;
        float reset_time = 0.0f;
        float this_sample = next_sample;
        next_sample = 0.0f;
        const float mf = pl_interp_next(&master_fm);
        const float sf = pl_interp_next(&fm);
        const float w = pl_interp_next(&pwm);
        const float ws = pl_interp_next(&wsm);
        const float square_amount = pl_maxf(ws - 0.5f, 0.0f) * 2.0f;
        const float triangle_amount = pl_maxf(1.0f - ws * 2.0f, 0.0f);
        const float slope_up = 1.0f / (w);
        const float slope_down = 1.0f / (1.0f - w);
        if (enable_sync) {
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
                if (!o->high && slave_phase_at_reset >= w) transition_during_reset = 1;
                float value = pl_vshape_naive(slave_phase_at_reset, w, slope_up, slope_down, triangle_amount, square_amount);
                this_sample -= value * pl_this_blep(reset_time);
                next_sample -= value * pl_next_blep(reset_time);
            }
        } else if (output_phase) {
            o->master_phase += mf;
            if (o->master_phase >= 1.0f) o->master_phase -= 1.0f;
        }
        o->slave_phase += sf;
        while (transition_during_reset || !reset) {
            if (!o->high) {
                if (o->slave_phase < w) break;
                float t = (o->slave_phase - w) / (o->previous_pw - w + sf);
                float triangle_step = (slope_up + slope_down) * sf;
                triangle_step *= triangle_amount;
                this_sample += square_amount * pl_this_blep(t);
                next_sample += square_amount * pl_next_blep(t);
                this_sample -= triangle_step * pl_this_integrated_blep(t);
                next_sample -= triangle_step * pl_next_integrated_blep(t);
                o->high = 1;
            }
            if (o->high) {
                if (o->slave_phase < 1.0f) break;
                o->slave_phase -= 1.0f;
                float t = o->slave_phase / sf;
                float triangle_step = (slope_up + slope_down) * sf;
                triangle_step *= triangle_amount;
                this_sample -= (1.0f - triangle_amount) * pl_this_blep(t);
                next_sample -= (1.0f - triangle_amount) * pl_next_blep(t);
                this_sample += triangle_step * pl_this_integrated_blep(t);
                next_sample += triangle_step * pl_next_integrated_blep(t);
                o->high = 0;
            }
        }
        if (enable_sync && reset) {
            o->slave_phase = reset_time * sf;
            o->high = 0;
        }
        next_sample += pl_vshape_naive(o->slave_phase, w, slope_up, slope_down, triangle_amount, square_amount);
        o->previous_pw = w;
        if (output_phase) {
            float phasor = o->master_phase;
            if (enable_sync) {
                const float ww = 4.0f * (1.0f - o->master_phase) * o->master_phase;
                this_sample *= ww * (2.0f - ww);
                const float p2 = phasor * phasor;
                phasor += (p2 * p2 - phasor) * pl_fabsf(w - 0.5f) * 2.0f;
            }
            *out++ = phasor + pl_interp_next(&pmm) * this_sample;
        } else {
            *out++ = (2.0f * this_sample - 1.0f);
        }
    }
    o->next_sample = next_sample;
    pl_interp_done(&master_fm);
    pl_interp_done(&fm);
    pl_interp_done(&pwm);
    pl_interp_done(&wsm);
    pl_interp_done(&pmm);
}
/* Render(frequency, pw, waveshape, out, size) */
static inline void pl_vshape_render(pl_vshape_t *o, float frequency, float pw, float waveshape, float *out, size_t size)
{
    pl_vshape_render_t(o, 0, 0, 0.0f, frequency, pw, waveshape, 0.0f, out, size);
}
/* Render(master_frequency, frequency, pw, waveshape, out, size): hard sync */
static inline void pl_vshape_render_sync(pl_vshape_t *o, float master_frequency, float frequency, float pw, float waveshape,
                                         float *out, size_t size)
{
    pl_vshape_render_t(o, 1, 0, master_frequency, frequency, pw, waveshape, 0.0f, out, size);
}

/* ------------------------------------------- virtual_analog_engine.cc --- */
typedef struct {
    pl_vshape_t primary, auxiliary, sync;
    pl_vsaw_t variable_saw;
    float auxiliary_amount, xmod_amount;
    float temp_buffer[PL_MAX_BLOCK];
} pl_va_t;

static void pl_va_init(pl_va_t *e)
{
    pl_vshape_init(&e->primary);
    pl_vshape_init(&e->auxiliary);
    e->auxiliary.master_phase = 0.25f;
    pl_vshape_init(&e->sync);
    pl_vsaw_init(&e->variable_saw);
    e->auxiliary_amount = 0.0f;
    e->xmod_amount = 0.0f;
}
static void pl_va_reset(pl_va_t *e) { (void)e; }

static const float pl_va_intervals[5] = { 0.0f, 7.01f, 12.01f, 19.01f, 24.01f };
static inline float pl_va_squash(float x) { return x * x * (3.0f - 2.0f * x); }
static float pl_va_detuning(float detune)
{
    detune = 2.05f * detune - 1.025f;
    PL_CONSTRAIN(detune, -1.0f, 1.0f);
    float sign = detune < 0.0f ? -1.0f : 1.0f;
    detune = detune * sign * 3.9999f;
    PL_MAKE_INTEGRAL_FRACTIONAL(detune)
    float a = pl_va_intervals[detune_integral];
    float b = pl_va_intervals[detune_integral + 1];
    return (a + (b - a) * pl_va_squash(pl_va_squash(detune_fractional))) * sign;
}

static __attribute__((section(".dsp_text"))) void pl_va_render(pl_va_t *e, const pl_params_t *p, float *out, float *aux, size_t size, int *already_enveloped)
{
    (void)already_enveloped;
    const float sync_amount = p->timbre * p->timbre;
    const float auxiliary_detune = pl_va_detuning(p->harmonics);
    const float primary_f = pl_note_to_frequency(p->note);
    const float auxiliary_f = pl_note_to_frequency(p->note + auxiliary_detune);
    const float primary_sync_f = pl_note_to_frequency(p->note + sync_amount * 48.0f);
    const float auxiliary_sync_f = pl_note_to_frequency(p->note + auxiliary_detune + sync_amount * 48.0f);
    size_t i;

    float shape = p->morph * 1.5f;
    PL_CONSTRAIN(shape, 0.0f, 1.0f);
    float pw = 0.5f + (p->morph - 0.66f) * 1.46f;
    PL_CONSTRAIN(pw, 0.5f, 0.995f);

    /* monster sync to AUX */
    pl_vshape_render_sync(&e->primary, primary_f, primary_sync_f, pw, shape, out, size);
    pl_vshape_render_sync(&e->auxiliary, auxiliary_f, auxiliary_sync_f, pw, shape, aux, size);
    for (i = 0; i < size; ++i) aux[i] = (aux[i] - out[i]) * 0.5f;

    /* double varishape to OUT */
    float square_pw = 1.3f * p->timbre - 0.15f;
    PL_CONSTRAIN(square_pw, 0.005f, 0.5f);
    const float square_sync_ratio = p->timbre < 0.5f ? 0.0f : (p->timbre - 0.5f) * (p->timbre - 0.5f) * 4.0f * 48.0f;
    const float square_gain = pl_minf(p->timbre * 8.0f, 1.0f);
    float saw_pw = p->morph < 0.5f ? p->morph + 0.5f : 1.0f - (p->morph - 0.5f) * 2.0f;
    saw_pw *= 1.1f;
    PL_CONSTRAIN(saw_pw, 0.005f, 1.0f);
    float saw_shape = 10.0f - 21.0f * p->morph;
    PL_CONSTRAIN(saw_shape, 0.0f, 1.0f);
    float saw_gain = 8.0f * (1.0f - p->morph);
    PL_CONSTRAIN(saw_gain, 0.02f, 1.0f);
    const float square_sync_f = pl_note_to_frequency(p->note + square_sync_ratio);

    pl_vshape_render_sync(&e->sync, primary_f, square_sync_f, square_pw, 1.0f, e->temp_buffer, size);
    pl_vsaw_render(&e->variable_saw, auxiliary_f, saw_pw, saw_shape, out, size);

    float norm = 1.0f / (pl_maxf(square_gain, saw_gain));
    pl_interp_t square_gain_modulation, saw_gain_modulation;
    pl_interp_init(&square_gain_modulation, &e->auxiliary_amount, square_gain * 0.3f * norm, size);
    pl_interp_init(&saw_gain_modulation, &e->xmod_amount, saw_gain * 0.5f * norm, size);
    for (i = 0; i < size; ++i)
        out[i] = out[i] * pl_interp_next(&saw_gain_modulation) + pl_interp_next(&square_gain_modulation) * e->temp_buffer[i];
    pl_interp_done(&square_gain_modulation);
    pl_interp_done(&saw_gain_modulation);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_va_flush(pl_va_t *e) { (void)e; }
