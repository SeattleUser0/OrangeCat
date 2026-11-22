/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * WAVE, a measurement prototype for docs/candidates/wave.md (NOT an engine: no engine number, no module, not in tools/build.py MODULES).
 * One voice of the proposed wavetable engine, written to measure what it costs: two wavetable oscillators (each a band-limited frame cached in the
 * voice's RAM, re-blended from two stored frames when the table position moves), a warp per oscillator (BEND or SYNC), unison 1..8 per oscillator,
 * a sub (sine an octave down) and noise, a filter (TPT state-variable LP / BP / HP / NOTCH, or a four-pole ladder), two envelopes, two LFOs, an
 * eight-slot matrix at the control rate (every 32 samples), and the amplitude ramp.
 *   - the oscillators are fixed point (uint32 phase, int16 frame, Q15 interpolation), as SHAPES and WHEEL are; the filter and the mix are float, as
 *     the Prophet is (the FM-1 has a single-precision FPU: -mfprev1);
 *   - wv_block() is the control part (CPU0 in the engine: the matrix, the envelopes, the LFOs, the coefficients, the frame cache);
 *     wv_kernel() is the sample loop (the batch kernel a voice would push: it reads the voice's own state only).
 * Build on the host:   cc -O2 -o wave_bench wave_proto.c wave_bench.c -lm
 * Build for the target (the loop cost): see run_cost.sh. */
#include <stdint.h>
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
#include "wave_proto.h"

static inline int32_t lerp_q15(int32_t a, int32_t b, uint32_t f15) { return a + (((b - a) * (int32_t)f15) >> 15); }

/* the frame cache: blend frames f0 and f0+1 of the table (stored at WV_LEN samples, the top level) by the fraction, then halve it down to the
 * level the note's pitch allows (each halving: a 7-tap half-band, so a level holds the harmonics below its Nyquist) */
static void wv_cache(wv_osc_t *o, const int16_t *table, uint32_t nframes, uint32_t pos16, uint32_t level)
{
    uint32_t f0 = (pos16 * (nframes - 1u)) >> 16, frac = ((pos16 * (nframes - 1u)) & 0xFFFFu) >> 1, i, n = WV_LEN, k;
    const int16_t *a = table + f0 * WV_LEN, *b = table + (f0 + 1u < nframes ? f0 + 1u : f0) * WV_LEN;
    int16_t *d = o->frame;
    for (i = 0; i < WV_LEN; i++) d[i] = (int16_t)lerp_q15(a[i], b[i], frac);
    for (k = 0; k < level && n > 16u; k++) {           /* halve: h = [-1 0 9 16 9 0 -1] / 32, circular (the frame is one cycle) */
        uint32_t m = n / 2u;
        for (i = 0; i < m; i++) {
            int32_t c = d[2 * i], l1 = d[(2 * i + n - 1u) & (n - 1u)], r1 = d[(2 * i + 1u) & (n - 1u)], l3 = d[(2 * i + n - 3u) & (n - 1u)], r3 = d[(2 * i + 3u) & (n - 1u)];
            o->tmp[i] = (int16_t)((16 * c + 9 * (l1 + r1) - (l3 + r3)) >> 5);
        }
        memcpy(d, o->tmp, m * sizeof(int16_t));
        n = m;
    }
    o->len = n;
    o->shift = 32u - (uint32_t)__builtin_ctz(n);
    o->cached = pos16 | level << 24;
}

static inline int32_t adsr_step(wv_env_t *e, uint32_t gate)
{
    if (!gate) e->stage = 3;
    switch (e->stage) {
    case 0: e->v += e->a; if (e->v >= 32767 << 8) { e->v = 32767 << 8; e->stage = 1; } break;
    case 1: e->v -= ((e->v - (e->s << 8)) >> 6) + 1; if (e->v <= e->s << 8) { e->v = e->s << 8; e->stage = 2; } break;
    case 2: break;
    default: e->v -= (e->v >> e->r) + 1; if (e->v < 0) e->v = 0; break;
    }
    return e->v >> 8;                                    /* Q15 */
}
static inline int32_t lfo_step(wv_lfo_t *l)
{
    l->ph += l->inc * WV_CTL;
    int32_t tri = (int32_t)(l->ph >> 16) - 32768;       /* a triangle: |saw| */
    tri = (tri < 0 ? -tri : tri) * 2 - 32768;
    return l->shape ? (int32_t)(l->ph >> 17) - 16384 : tri;   /* 0 triangle, 1 saw (half range) */
}
static inline float tanf_approx(float x) { float x2 = x * x; return x * (1.0f + x2 * (0.3333333f + x2 * 0.1333333f)); }

/* the control part, once a 32-sample block: sources, the matrix, envelopes, LFOs, coefficients, the frame cache */
void wv_block(wv_voice_t *v, const wv_patch_t *p, const int16_t *const tables[2], const uint32_t nframes[2])
{
    int32_t src[WV_SRC], dst[WV_DST];
    uint32_t i, k;
    src[0] = adsr_step(&v->env[0], v->gate);
    src[1] = adsr_step(&v->env[1], v->gate);
    src[2] = lfo_step(&v->lfo[0]);
    src[3] = lfo_step(&v->lfo[1]);
    src[4] = v->vel << 8;
    src[5] = v->note << 8;
    src[6] = p->macro[0] << 8;
    src[7] = p->macro[1] << 8;
    for (i = 0; i < WV_DST; i++) dst[i] = p->base[i];
    for (k = 0; k < WV_SLOTS; k++) {                     /* the matrix: source x amount into a destination, Q15 */
        const wv_slot_t *s = &p->slot[k];
        if (s->amt) dst[s->dst] += (src[s->src] * s->amt) >> 15;
    }
    for (i = 0; i < 2u; i++) {
        wv_osc_t *o = &v->osc[i];
        uint32_t pos = (uint32_t)(dst[D_POS1 + i] < 0 ? 0 : dst[D_POS1 + i] > 65535 ? 65535 : dst[D_POS1 + i]);
        uint32_t level = v->level;
        if ((o->cached & 0xFFFFFFu) != pos || o->cached >> 24 != level) wv_cache(o, tables[i], nframes[i], pos, level);
        o->warp = dst[D_WARP1 + i];
        o->amp = dst[D_LVL1 + i];
    }
    v->amp0 = v->amp1;
    v->amp1 = (src[0] * dst[D_AMP]) >> 15;
    /* the filter: cutoff from the matrix (Q15 of 0..20 kHz on a rough exponential), the TPT coefficient once a block */
    {
        float fc = 20.0f * (float)(1u << ((uint32_t)(dst[D_CUT] < 0 ? 0 : dst[D_CUT] > 32767 ? 32767 : dst[D_CUT]) >> 12)) * (1.0f + (float)(dst[D_CUT] & 4095) * (1.0f / 4096.0f));
        if (fc > 18000.0f) fc = 18000.0f;
        float g = tanf_approx(3.14159265f * fc / 44100.0f);
        float r = 2.0f - 1.9f * (float)(dst[D_RES] < 0 ? 0 : dst[D_RES]) * (1.0f / 32768.0f);
        v->f.g = g;
        v->f.k = r;
        v->f.a1 = 1.0f / (1.0f + g * (g + r));
    }
}

/* the sample loop: n <= WV_CTL samples of the voice into out (added), from the voice's own state */
void wv_kernel(wv_voice_t *v, const wv_patch_t *p, int32_t *out, uint32_t n)
{
    uint32_t i, j, k;
    float y, ic1 = v->f.ic1, ic2 = v->f.ic2, l0 = v->f.l[0], l1 = v->f.l[1], l2 = v->f.l[2], l3 = v->f.l[3];
    const float g = v->f.g, kk = v->f.k, a1 = v->f.a1;
    const float mlp = p->mode == F_LP || p->mode == F_NOTCH ? 1.0f : 0.0f, mbp = p->mode == F_BP ? 1.0f : 0.0f, mhp = p->mode == F_HP || p->mode == F_NOTCH ? 1.0f : 0.0f;
    int32_t amp = v->amp0, damp = (v->amp1 - v->amp0) / (int32_t)WV_CTL;
    uint32_t rng = v->rng;
    for (i = 0; i < n; i++) {
        int32_t acc = 0;
        for (j = 0; j < 2u; j++) {
            wv_osc_t *o = &v->osc[j];
            const int16_t *fr = o->frame;
            const uint32_t sh = o->shift, mask = o->len - 1u;
            int32_t s = 0;
            for (k = 0; k < p->uni[j]; k++) {
                uint32_t ph = o->ph[k] += o->inc[k], q;
                if (p->warp[j] == W_SYNC) {              /* SYNC: the frame read faster than the cycle, restarted at its end */
                    q = (uint32_t)(((uint64_t)ph * (uint32_t)(65536 + o->warp * 3)) >> 16);
                } else {                                 /* BEND: the phase bent toward one end, p + a p (1 - p) */
                    uint32_t x = ph >> 16;
                    q = ph + (uint32_t)(((int32_t)((x * (65535u - x)) >> 16) * o->warp) >> 0);
                }
                uint32_t idx = q >> sh, f = (q << (32u - sh)) >> 17;
                s += lerp_q15(fr[idx], fr[(idx + 1u) & mask], f);
            }
            acc += (s * o->amp) >> 15;
        }
        /* sub (a sine an octave under oscillator 1, from its first phase) and noise */
        {
            uint32_t sp = v->osc[0].ph[0] >> 1;
            int32_t x = (int32_t)(sp >> 16) - 32768;     /* a parabolic sine */
            int32_t sine = (x * (32767 - (x < 0 ? -x : x)) >> 13);
            rng = rng * 1664525u + 1013904223u;
            acc += (sine * p->sub + (int32_t)(rng >> 17) * p->noise - 16384 * p->noise) >> 15;
        }
        float in = (float)acc * (1.0f / 32768.0f);
        if (p->mode == F_LADDER) {                       /* four one-pole stages with a cubic soft clip on the feedback */
            float fb = in - kk * l3, c = fb - fb * fb * fb * 0.1481f;
            l0 += g * (c - l0); l1 += g * (l0 - l1); l2 += g * (l1 - l2); l3 += g * (l2 - l3);
            y = l3;
        } else {                                         /* TPT SVF (Zavalishin): one sample */
            float hp = (in - (kk + g) * ic1 - ic2) * a1;
            float bp = g * hp + ic1; ic1 = g * hp + bp;
            float lp = g * bp + ic2; ic2 = g * bp + lp;
            y = mlp * lp + mbp * bp + mhp * hp;
        }
        out[i] += (int32_t)(y * (float)amp);
        amp += damp;
    }
    v->f.ic1 = ic1; v->f.ic2 = ic2; v->f.l[0] = l0; v->f.l[1] = l1; v->f.l[2] = l2; v->f.l[3] = l3;
    v->rng = rng;
}
