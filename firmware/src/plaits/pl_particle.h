/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat 0.5.0: Plaits' model 10, PARTICLE (plaits/dsp/engine/particle_engine.cc), with its particles
 * (noise/particle.h), its diffuser (fx/diffuser.h) and the part of fx/fx_engine.h the diffuser uses (12-bit delay
 * memory, LFO_1), in C.
 *   OUT: random impulses through resonant band-passes, then a low-pass and the diffuser.
 *   AUX: the raw impulses. TIMBRE: the density. HARMONICS: the spread of the filters' frequencies.
 *   MORPH: below the middle, diffusion; above it, resonance.
 *
 * Device difference: the C++ diffuser has 8192 words (16 KB) of delay memory. The FM-1 gives an engine 12 KB, so
 * without PL_PARITY the diffuser has 4096 words (8 KB) with every delay line half as long (126 -> 63, 180 -> 90,
 * 269 -> 134, 444 -> 222, 1653 -> 827, 2010 -> 1005, 3411 -> 1706; the odd ones rounded so the lines share few
 * factors and the modulated reads reach just the end of their line, as in the C++) and every read offset and
 * modulation depth in pl_particle_diffuser_process() halved (400 -> 200, 43 -> 21.5, 3070 -> 1535, 340 -> 170).
 * The diffusion is about twice as dense and its tail half as long. The LFO is unchanged. PL_PARITY keeps the C++
 * sizes exactly; both builds run the same code with the sizes from the PL_PARTICLE_* macros.
 * The LFO's rate is 0.3f / 48000.0f in the C++ (a literal, not kSampleRate), kept as is: 0.28 Hz at 44.1 kHz.
 * The fx engine's second LFO is not used by the diffuser and is left out.
 *
 * Revisions:
 *   0.5.0: first version.
 */
#pragma once
#include "pl_osc.h"

#define PL_PARTICLE_NUM_PARTICLES 6

/* ---------------------------------------------------------- particle.h --- */
typedef struct { float pre_gain; pl_svf_t filter; } pl_particle_one_t;
static inline void pl_particle_one_init(pl_particle_one_t *p)
{
    p->pre_gain = 0.0f;
    pl_svf_init(&p->filter);
}
static inline void pl_particle_one_render(pl_particle_one_t *p, int sync, float density, float gain, float frequency,
                                          float spread, float q, float *out, float *aux, size_t size)
{
    float u = pl_rng_float();
    if (sync) u = density;
    int can_randomize_frequency = 1;
    while (size--) {
        float s = 0.0f;
        if (u <= density) {
            s = u * gain;
            if (can_randomize_frequency) {
                const float v = 2.0f * pl_rng_float() - 1.0f;
                const float f = pl_minf(pl_semitones_to_ratio(spread * v) * frequency, 0.25f);
                p->pre_gain = 0.5f / pl_sqrt(q * f * pl_sqrt(density));
                pl_svf_set_f_q(&p->filter, PL_FREQ_DIRTY, f, q);
                can_randomize_frequency = 0;   /* keep the cutoff constant for this whole block */
            }
        }
        *aux++ += s;
        *out++ += pl_svf_process(&p->filter, PL_BP, p->pre_gain * s);
        u = pl_rng_float();
    }
}

/* ------------------------------------------- diffuser.h, fx_engine.h --- */
/* Delay memory and line lengths (FxEngine<size, FORMAT_12_BIT>, the Reserve<> chain) and the read offsets. */
#ifdef PL_PARITY
#define PL_PARTICLE_DIFF_SIZE 8192
#define PL_PARTICLE_AP1_LEN 126
#define PL_PARTICLE_AP2_LEN 180
#define PL_PARTICLE_AP3_LEN 269
#define PL_PARTICLE_AP4_LEN 444
#define PL_PARTICLE_DAPA_LEN 1653
#define PL_PARTICLE_DAPB_LEN 2010
#define PL_PARTICLE_DEL_LEN 3411
#define PL_PARTICLE_AP4_OFFSET 400.0f
#define PL_PARTICLE_AP4_DEPTH 43.0f
#define PL_PARTICLE_DEL_OFFSET 3070.0f
#define PL_PARTICLE_DEL_DEPTH 340.0f
#else
#define PL_PARTICLE_DIFF_SIZE 4096
#define PL_PARTICLE_AP1_LEN 63
#define PL_PARTICLE_AP2_LEN 90
#define PL_PARTICLE_AP3_LEN 134
#define PL_PARTICLE_AP4_LEN 222
#define PL_PARTICLE_DAPA_LEN 827
#define PL_PARTICLE_DAPB_LEN 1005
#define PL_PARTICLE_DEL_LEN 1706
#define PL_PARTICLE_AP4_OFFSET 200.0f
#define PL_PARTICLE_AP4_DEPTH 21.5f
#define PL_PARTICLE_DEL_OFFSET 1535.0f
#define PL_PARTICLE_DEL_DEPTH 170.0f
#endif
#define PL_PARTICLE_DIFF_MASK (PL_PARTICLE_DIFF_SIZE - 1)
/* DelayLine<Memory, i>::base: each line starts one word after the end of the one before */
#define PL_PARTICLE_AP1_BASE 0
#define PL_PARTICLE_AP2_BASE (PL_PARTICLE_AP1_BASE + PL_PARTICLE_AP1_LEN + 1)
#define PL_PARTICLE_AP3_BASE (PL_PARTICLE_AP2_BASE + PL_PARTICLE_AP2_LEN + 1)
#define PL_PARTICLE_AP4_BASE (PL_PARTICLE_AP3_BASE + PL_PARTICLE_AP3_LEN + 1)
#define PL_PARTICLE_DAPA_BASE (PL_PARTICLE_AP4_BASE + PL_PARTICLE_AP4_LEN + 1)
#define PL_PARTICLE_DAPB_BASE (PL_PARTICLE_DAPA_BASE + PL_PARTICLE_DAPA_LEN + 1)
#define PL_PARTICLE_DEL_BASE (PL_PARTICLE_DAPB_BASE + PL_PARTICLE_DAPB_LEN + 1)
_Static_assert(PL_PARTICLE_DEL_BASE + PL_PARTICLE_DEL_LEN <= PL_PARTICLE_DIFF_SIZE, "diffuser delay memory full");
_Static_assert((PL_PARTICLE_DIFF_SIZE & PL_PARTICLE_DIFF_MASK) == 0, "diffuser memory size must be a power of 2");

typedef struct {
    uint16_t buffer[PL_PARTICLE_DIFF_SIZE];     /* (the C++'s allocator buffer) */
    int32_t write_ptr;
    pl_cosine_osc_t lfo;                        /* LFO_1 */
    float lp_decay;
} pl_particle_diffuser_t;

/* DataType<FORMAT_12_BIT> */
static inline float pl_particle_decompress(uint16_t value) { return (float)((int16_t)value) / 4096.0f; }
static inline uint16_t pl_particle_compress(float value) { return (uint16_t)pl_clip16((int32_t)(value * 4096.0f)); }

static inline void pl_particle_diffuser_init(pl_particle_diffuser_t *d)
{
    /* SetLFOFrequency(LFO_1, 0.3f / 48000.0f): Init<COSINE_OSCILLATOR_APPROXIMATE>(frequency * 32.0f) */
    pl_cosine_osc_init_approximate(&d->lfo, 0.3f / 48000.0f * 32.0f);
    d->lp_decay = 0.0f;
}
static inline void pl_particle_diffuser_reset(pl_particle_diffuser_t *d)
{
    int i;
    for (i = 0; i < PL_PARTICLE_DIFF_SIZE; ++i) d->buffer[i] = 0;   /* FxEngine::Clear() */
    d->write_ptr = 0;
}

/* The FxEngine Context, with the accumulator and the last read as locals. A read at offset -1 (TAIL) is the line's
 * last word, base + length - 1. */
#define PL_PARTICLE_AT(base, offset) d->buffer[(write_ptr + (base) + (offset)) & PL_PARTICLE_DIFF_MASK]
#define PL_PARTICLE_READ_TAIL(base, len, scale) do { \
    previous_read = pl_particle_decompress(PL_PARTICLE_AT(base, (len) - 1)); \
    acc += previous_read * (scale); \
} while (0)
#define PL_PARTICLE_WRITE_ALL_PASS(base, scale) do { \
    PL_PARTICLE_AT(base, 0) = pl_particle_compress(acc); \
    acc *= (scale); \
    acc += previous_read; \
} while (0)
#define PL_PARTICLE_INTERPOLATE(base, offset0, amplitude, scale) do { \
    float pl_off_ = (offset0); \
    pl_off_ += (amplitude) * lfo_value; \
    PL_MAKE_INTEGRAL_FRACTIONAL(pl_off_) \
    float pl_a_ = pl_particle_decompress(PL_PARTICLE_AT(base, pl_off__integral)); \
    float pl_b_ = pl_particle_decompress(PL_PARTICLE_AT(base, pl_off__integral + 1)); \
    float pl_x_ = pl_a_ + (pl_b_ - pl_a_) * pl_off__fractional; \
    previous_read = pl_x_; \
    acc += pl_x_ * (scale); \
} while (0)

static void pl_particle_diffuser_process(pl_particle_diffuser_t *d, float amount, float rt, float *in_out, size_t size)
{
    const float kap = 0.625f;
    const float klp = 0.75f;
    float lp = d->lp_decay;
    while (size--) {
        float wet;
        /* FxEngine::Start() */
        int32_t write_ptr = --d->write_ptr;
        if (write_ptr < 0) {
            write_ptr += PL_PARTICLE_DIFF_SIZE;
            d->write_ptr = write_ptr;
        }
        float acc = 0.0f, previous_read = 0.0f;
        const float lfo_value = (write_ptr & 31) == 0 ? pl_cosine_osc_next(&d->lfo) : pl_cosine_osc_value(&d->lfo);

        acc += *in_out;                                                    /* c.Read(*in_out) */
        PL_PARTICLE_READ_TAIL(PL_PARTICLE_AP1_BASE, PL_PARTICLE_AP1_LEN, kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_AP1_BASE, -kap);
        PL_PARTICLE_READ_TAIL(PL_PARTICLE_AP2_BASE, PL_PARTICLE_AP2_LEN, kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_AP2_BASE, -kap);
        PL_PARTICLE_READ_TAIL(PL_PARTICLE_AP3_BASE, PL_PARTICLE_AP3_LEN, kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_AP3_BASE, -kap);
        PL_PARTICLE_INTERPOLATE(PL_PARTICLE_AP4_BASE, PL_PARTICLE_AP4_OFFSET, PL_PARTICLE_AP4_DEPTH, kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_AP4_BASE, -kap);
        PL_PARTICLE_INTERPOLATE(PL_PARTICLE_DEL_BASE, PL_PARTICLE_DEL_OFFSET, PL_PARTICLE_DEL_DEPTH, rt);
        lp += klp * (acc - lp);                                            /* c.Lp(lp, klp) */
        acc = lp;
        PL_PARTICLE_READ_TAIL(PL_PARTICLE_DAPA_BASE, PL_PARTICLE_DAPA_LEN, -kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_DAPA_BASE, kap);
        PL_PARTICLE_READ_TAIL(PL_PARTICLE_DAPB_BASE, PL_PARTICLE_DAPB_LEN, kap);
        PL_PARTICLE_WRITE_ALL_PASS(PL_PARTICLE_DAPB_BASE, -kap);
        PL_PARTICLE_AT(PL_PARTICLE_DEL_BASE, 0) = pl_particle_compress(acc);   /* c.Write(del, 2.0f) */
        acc *= 2.0f;
        wet = acc;                                                         /* c.Write(wet, 0.0f) */
        acc *= 0.0f;
        (void)acc;
        *in_out += amount * (wet - *in_out);
        ++in_out;
    }
    d->lp_decay = lp;
}

/* -------------------------------------------------- particle_engine.cc --- */
typedef struct {
    pl_particle_one_t particle[PL_PARTICLE_NUM_PARTICLES];
    pl_particle_diffuser_t diffuser;
    pl_svf_t post_filter;
} pl_particle_t;

static void pl_particle_init(pl_particle_t *e)
{
    int i;
    for (i = 0; i < PL_PARTICLE_NUM_PARTICLES; ++i) pl_particle_one_init(&e->particle[i]);
    pl_particle_diffuser_init(&e->diffuser);
    pl_svf_init(&e->post_filter);
}
static void pl_particle_reset(pl_particle_t *e) { pl_particle_diffuser_reset(&e->diffuser); }

static __attribute__((section(".text.xip_macro"))) void pl_particle_render(pl_particle_t *e, const pl_params_t *p, float *out, float *aux, size_t size,
                               int *already_enveloped)
{
    (void)already_enveloped;
    size_t j;
    int i;
    const float f0 = pl_note_to_frequency(p->note);
    const float density_sqrt = pl_note_to_frequency(60.0f + p->timbre * p->timbre * 72.0f);
    const float density = density_sqrt * density_sqrt * (1.0f / PL_PARTICLE_NUM_PARTICLES);
    const float gain = 1.0f / density;
    const float q_sqrt = pl_semitones_to_ratio(p->morph >= 0.5f ? (p->morph - 0.5f) * 120.0f : 0.0f);
    const float q = 0.5f + q_sqrt * q_sqrt;
    const float spread = 48.0f * p->harmonics * p->harmonics;
    const float raw_diffusion_sqrt = 2.0f * pl_fabsf(p->morph - 0.5f);
    const float raw_diffusion = raw_diffusion_sqrt * raw_diffusion_sqrt;
    const float diffusion = p->morph < 0.5f ? raw_diffusion : 0.0f;
    const int sync = (p->trigger & PL_TRIGGER_RISING_EDGE) != 0;

    for (j = 0; j < size; ++j) { out[j] = 0.0f; aux[j] = 0.0f; }

    for (i = 0; i < PL_PARTICLE_NUM_PARTICLES; ++i)
        pl_particle_one_render(&e->particle[i], sync, density, gain, f0, spread, q, out, aux, size);

    pl_svf_set_f_q(&e->post_filter, PL_FREQ_DIRTY, pl_minf(f0, 0.49f), 0.5f);
    pl_svf_process_buf(&e->post_filter, PL_LP, out, out, size);

    pl_particle_diffuser_process(&e->diffuser, 0.8f * diffusion * diffusion, 0.5f * diffusion + 0.25f, out, size);
}

/* (Orange Cat) the decaying states to 0 once tiny (pl_dsp.h pl_flush): eng_macro.c calls this after each render */
static inline void pl_particle_flush(pl_particle_t *e)
{
    for (int i = 0; i < PL_PARTICLE_NUM_PARTICLES; i++) pl_flush_svf(&e->particle[i].filter);
    pl_flush_svf(&e->post_filter);
    pl_flush(&e->diffuser.lp_decay);
}
