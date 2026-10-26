/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay: research prototype (see dubdelay.h). Single precision throughout; tables and converters are designed in
 * dd_init, dd_process only reads them. Written for -ffp-contract=off -Werror=double-promotion (Orange Cat's flags). */
#include "dubdelay.h"
#include "dd_math.h"
#if __STDC_HOSTED__
#include <string.h>
#else
void *memset(void *d, int c, size_t n);          /* (the firmware's own) */
void *memcpy(void *d, const void *s, size_t n);
#endif

#ifndef DD_NOINL
#define DD_NOINL
#endif
#define HB_M 8                                   /* half-band: 4M-1 = 31 taps, M multiplies an output */
#define HB_C (2 * HB_M - 1)                      /* its centre (latency, input samples) */
#define SIN_N 256
#define PI_F 3.14159265f
#define Q16 32767.0f
#define IQ16 (1.0f / 32767.0f)

static float hb_h[HB_M];                         /* the odd taps (offsets 1, 3, 5 ..), centre 0.5 */
static float sintab[SIN_N + 1];
static int tables_ready;

/* ------------------------------------------------------------------ init-time design --- */
static float bessel_i0(float x)
{
    float s = 1.0f, t = 1.0f;
    int k;
    for (k = 1; k < 32; k++) { t *= (x * 0.5f / (float)k) * (x * 0.5f / (float)k); s += t; }
    return s;
}
static void design_tables(void)
{
    const float beta = 7.0f, n2 = (float)(HB_C + 1);   /* Kaiser over the 31 taps */
    float sum = 0.0f;
    int j;
    for (j = 0; j < HB_M; j++) {
        float o = (float)(2 * j + 1), r = o / n2;
        hb_h[j] = ((j & 1) ? -1.0f : 1.0f) / (PI_F * o) * bessel_i0(beta * ddm_sqrt(1.0f - r * r)) / bessel_i0(beta);
        sum += 2.0f * hb_h[j];
    }
    for (j = 0; j < HB_M; j++) hb_h[j] *= 0.5f / sum;    /* odd taps sum to 0.5: unity at DC */
    for (j = 0; j <= SIN_N; j++) sintab[j] = ddm_sin(2.0f * PI_F * (float)j / (float)SIN_N);
    tables_ready = 1;
}
#define KNOB_RATIO 1.0558981944f               /* 1000^(1/127): 20 Hz .. 20 kHz over 0..127 */
static float knob_hz_int(int k) { float f = 20.0f; while (k-- > 0) f *= KNOB_RATIO; return f; }
float dd_knob_hz(float knob)
{
    int i = (int)knob;
    float a = knob_hz_int(i);
    return a + (knob - (float)i) * (a * KNOB_RATIO - a);
}

uint32_t dd_line_len(float max_seconds, int R)
{
    uint32_t n = (uint32_t)(max_seconds * (float)DD_FS / (float)R) + 32u, l = 256u;
    while (l < n) l <<= 1;
    return l;
}

void dd_reset(dd_t *s)
{
    int i;
    for (i = 0; i < s->lines; i++) memset(s->line[i], 0, s->len * sizeof(int16_t));
    s->w = 0;
    for (i = 0; i < DD_TAPS; i++) { s->d[i] = 0.0f; s->gl[i] = s->gr[i] = s->fw[i] = 0.0f; s->m_prev[i] = s->m_next[i] = 0.0f; }
    memset(s->hp_s, 0, sizeof s->hp_s); memset(s->lp1, 0, sizeof s->lp1); memset(s->lp2, 0, sizeof s->lp2);
    memset(s->hp1, 0, sizeof s->hp1); memset(s->hp2, 0, sizeof s->hp2);
    memset(s->pre_x1, 0, sizeof s->pre_x1); memset(s->de_y1, 0, sizeof s->de_y1); memset(s->sat_x1, 0, sizeof s->sat_x1);
    s->fb_cur = s->send_cur = s->wet_cur = s->dry_cur = 0.0f;
    s->wo[0] = s->fo[0] = 1.0f; s->wo[1] = s->fo[1] = 0.0f;
    s->drift = s->drift_lp = 0.0f; s->rng = 22222u;
    s->ctl_left = 0;
    memset(s->dec, 0, sizeof s->dec); memset(s->intl, 0, sizeof s->intl); memset(s->intr, 0, sizeof s->intr);
    s->box_acc = 0.0f; s->box_n = 0;
    s->lin_l[0] = s->lin_l[1] = s->lin_r[0] = s->lin_r[1] = 0.0f;
    s->denorm = 0;
    s->primed = 0;
    s->cut_g = s->cutw_g = 1.0f; s->lim_env = 0.0f; s->lim_g = 1.0f;   /* (0.5) the cut open, the limiter idle */
}

void dd_init(dd_t *s, int algo, int R, int aa, int lines, int16_t *line0, int16_t *line1, uint32_t len)
{
    int k;
    float tick;
    if (!tables_ready) design_tables();
    memset(s, 0, sizeof *s);
    s->algo = algo; s->R = R; s->aa = aa; s->lines = (algo == DD_ALGO_TAPE && lines == 2) ? 2 : 1;
    s->line[0] = line0; s->line[1] = line1; s->len = len; s->mask = len - 1u;
    s->fsl = (float)DD_FS / (float)R;
    for (k = 0; k < DD_KNOB; k++) {
        float f = knob_hz_int(k);
        if (f > 0.45f * s->fsl) f = 0.45f * s->fsl;
        s->gtab[k] = ddm_tan(PI_F * f / s->fsl);
        /* RES: damping 2 (Q 0.5) .. 0.25 (Q 4); FEEDBACK is calibrated at the filter's peak, so 1.0 is the threshold of
         * regeneration at the resonance (2-pole LP peak: Q / sqrt(1 - 1 / 4Q^2) for Q > 1/sqrt 2) */
        s->ktab[k] = 2.0f - 1.75f * (float)k / 127.0f;
        {   float q = 1.0f / s->ktab[k];
            s->mktab[k] = q > 0.7072f ? ddm_sqrt(1.0f - 0.25f / (q * q)) / q : 1.0f; }
    }
    /* nothing subtracted from the heads: they sit on whole samples (a fractional static head would be a 2-tap average,
     * a tone that changed with TIME). TAPE's ADAA saturator delays each pass by half a sample (11 us at 44.1 kHz),
     * left as it is. The rate converters sit outside the loop: their latency (conv_lat) is a constant offset of every
     * echo, not compensated (subtracting it from the heads would detune the loop by it on every pass) */
    s->lat = 0.0f;
    s->conv_lat = R == 1 ? 0.0f : aa == DD_AA_HB ? (R == 2 ? 2.0f * (float)HB_C - 1.0f : 6.0f * (float)HB_C - 3.0f)
                                              : (float)(R - 1);      /* (measured: tools/measure 1) */
    tick = (float)(DD_CTL / R);                          /* loop samples a control tick */
    s->wow_w = 2.0f * PI_F * 0.55f / s->fsl;             /* wow 0.55 Hz, flutter 6.3 Hz (capstan-like) */
    s->flut_w = 2.0f * PI_F * 6.3f / s->fsl;
    s->rot_w[0] = ddm_cos(s->wow_w * tick); s->rot_w[1] = ddm_sin(s->wow_w * tick);
    s->rot_f[0] = ddm_cos(s->flut_w * tick); s->rot_f[1] = ddm_sin(s->flut_w * tick);
    dd_reset(s);
}

float dd_max_seconds(const dd_t *s) { return ((float)s->len - 8.0f - s->lat - 40.0f) / s->fsl; }

/* ------------------------------------------------------------------ helpers --- */
static inline float knob_g(const dd_t *s, float k)
{
    int i;
    float f;
    if (k < 0.0f) k = 0.0f;
    if (k > (float)(DD_KNOB - 1)) k = (float)(DD_KNOB - 1);
    i = (int)k; f = k - (float)i;
    if (i >= DD_KNOB - 1) return s->gtab[DD_KNOB - 1];
    return s->gtab[i] + f * (s->gtab[i + 1] - s->gtab[i]);
}
static inline float sin_turn(float t)                     /* sin(2 pi t), table + linear */
{
    float x, f;
    int i;
    t -= (float)(int)t; if (t < 0.0f) t += 1.0f;
    x = t * (float)SIN_N; i = (int)x; f = x - (float)i;
    return sintab[i] + f * (sintab[i + 1] - sintab[i]);
}
static inline float clip15(float x) { return x > 1.5f ? 1.5f : x < -1.5f ? -1.5f : x; }
static inline float sat3(float x) { x = clip15(x); return x - 0.148148148f * x * x * x; }   /* +-1.5 -> +-1, C1 */
static inline float sat3_F(float x)                       /* its antiderivative (ADAA) */
{
    float a = x < 0.0f ? -x : x;
    if (a >= 1.5f) return a - 0.5625f;
    return 0.5f * a * a - 0.037037037f * a * a * a * a;
}
static inline float lim_out(float x)                      /* exact to +-0.5 (-6 dBFS), a quadratic knee to +-1.0 */
{
    float a = x < 0.0f ? -x : x, t2;
    if (a <= 0.5f) return x;
    t2 = a - 0.5f;
    a = t2 >= 1.0f ? 1.0f : 0.5f + t2 - 0.5f * t2 * t2;
    return x < 0.0f ? -a : a;
}
static inline int16_t to_q15(float v)
{
    v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
    v *= Q16;
    return (int16_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}
static inline float rd_lin(const int16_t *b, uint32_t w, uint32_t mask, float d)
{
    int ip = (int)d;
    float fr = d - (float)ip;
    uint32_t i0 = (w - (uint32_t)ip) & mask;
    float x0 = (float)b[i0], x1 = (float)b[(i0 - 1u) & mask];
    return x0 + fr * (x1 - x0);
}
static inline float rd_herm(const int16_t *b, uint32_t w, uint32_t mask, float d)
{
    int ip = (int)d;
    float f = d - (float)ip;
    uint32_t i0 = (w - (uint32_t)ip) & mask;
    float xm1 = (float)b[(i0 + 1u) & mask], x0 = (float)b[i0], x1 = (float)b[(i0 - 1u) & mask], x2 = (float)b[(i0 - 2u) & mask];
    float c1 = 0.5f * (x1 - xm1);
    float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
    float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
    return ((c3 * f + c2) * f + c1) * f + x0;
}

/* half-band decimator: one input; returns 1 with *y when an output is due */
static inline int hb_dec(dd_hb_t *h, float x, float *y)
{
    int j;
    float a;
    h->p = (h->p + 1u) & 31u; h->st[h->p] = x;
    h->ph ^= 1u;
    if (h->ph) return 0;
    a = 0.5f * h->st[(h->p - HB_C) & 31u];
    for (j = 0; j < HB_M; j++)
        a += hb_h[j] * (h->st[(h->p - HB_C + 2u * j + 1u) & 31u] + h->st[(h->p - HB_C - 2u * j - 1u) & 31u]);
    *y = a;
    return 1;
}
/* half-band interpolator: one input, two outputs (gain 2 folded in) */
static inline void hb_int(dd_hb_t *h, float x, float *y0, float *y1)
{
    int j;
    float a = 0.0f;
    h->p = (h->p + 1u) & 15u; h->st[h->p] = x;
    for (j = 0; j < HB_M; j++)
        a += hb_h[j] * (h->st[(h->p - HB_M - j) & 15u] + h->st[(h->p - HB_M + 1u + j) & 15u]);
    *y0 = 2.0f * a;
    *y1 = h->st[(h->p - HB_M + 1u) & 15u];
}

/* ------------------------------------------------------------------ the spring (revision 0.4) --- */
/* A Fisher K-10 Space Expander as King Tubby used it: a tube stage into a spring tank; dark, a short dense tail, the chirp on every
 * transient. Not an impulse response: a small model, mono, at the line's rate.
 *   in -> DC block -> the tube (sat3 at a gain up to 7, half of it made up) -> (+) -> the tank's ring (37 ms) -> the dispersion chain (DD_SP_AP
 *   first-order allpasses: the chirp, low frequencies arriving later, once more on every trip round) -> the loop's low-pass (dark)
 *   -> x g back to (+). The loop's output -> two density allpasses (5.3 and 7.1 ms, 0.6) -> the output's low-pass -> x norm.
 * g from the T60 (sp_decay): 10^(-3 dt / T60). The tank's own write is soft-clipped (it never holds more than full scale).
 * norm: in the delay's loop 1 - g, which takes the tank's gain at its peaks (1 / (1 - g); the allpasses and the low-passes are at most 1)
 * to at most 1, so the delay's FEEDBACK stays the loop's gain whatever the spring does: the loop cannot run away through it. On the wet,
 * sqrt(1 - g^2) (the tail's energy: a reverb's level).
 * The kick (crash): one full-scale impulse, then 40 ms of noise from three times full scale falling 30 dB, straight into the tank (the
 * springs struck, past the tube); the tank's own clip at its write holds it to full scale: the explosion. */
static uint32_t sp_len(float sec, float fsl, uint32_t cap)
{
    uint32_t n = (uint32_t)(sec * fsl + 0.5f);
    return n < 2u ? 2u : n > cap - 2u ? cap - 2u : n;
}
void dd_spring_attach(dd_t *s, int16_t *buf)
{
    dd_spring_t *q = &s->sp;
    memset(q, 0, sizeof *q);
    q->buf = buf;
    if (!buf) return;
    memset(buf, 0, DD_SP_BUF * sizeof(int16_t));
    q->dt = sp_len(0.037f, s->fsl, DD_SP_TANK);
    q->d1 = sp_len(0.0053f, s->fsl, DD_SP_D1);
    q->d2 = sp_len(0.0071f, s->fsl, DD_SP_D2);
    q->a = s->R == 1 ? 0.78f : s->R == 2 ? 0.62f : 0.40f;   /* the chirp's band about the same in hertz at each line rate */
    q->rng = 777u;
}
static void sp_tick(dd_t *s, const dd_params_t *p)
{
    dd_spring_t *q = &s->sp;
    float T = p->sp_decay > 0.05f ? p->sp_decay : 2.0f, tone = p->sp_tone < 0.0f ? 0.0f : p->sp_tone > 1.0f ? 1.0f : p->sp_tone;
    float dr = p->sp_drive < 0.0f ? 0.0f : p->sp_drive > 1.0f ? 1.0f : p->sp_drive;
    {   int pl = q->buf ? p->spring : 0;
        if (pl != q->place && q->buf) {              /* (0.5) a new placement: the tank read another way, cleared */
            memset(q->buf, 0, DD_SP_BUF * sizeof(int16_t));
            memset(q->ap, 0, sizeof q->ap); q->w = q->w1 = q->w2 = 0; q->lp = q->olp = q->dc = 0.0f;
        }
        q->place = pl;
    }
    if (!q->place) return;
    q->g = ddm_exp(-6.90775528f * ((float)q->dt / s->fsl) / T);
    q->glp = 1.0f - ddm_exp(-2.0f * PI_F * (1600.0f + 3000.0f * tone) / s->fsl);
    q->gout = 1.0f - ddm_exp(-2.0f * PI_F * (3000.0f + 3000.0f * tone) / s->fsl);
    q->norm = q->place == 1 ? 1.0f - q->g : ddm_sqrt(1.0f - q->g * q->g);
    q->gin = 1.0f + 6.0f * dr * dr;                  /* the tube: in the loop made up whole (unity for small signals: the loop's gain stays
                                                      * FEEDBACK), on the wet half (a driven tube is louder) */
    q->igin = q->place == 1 ? 1.0f / q->gin : 1.0f / ddm_sqrt(q->gin);
    q->m = p->sp_mix < 0.0f ? 0.0f : p->sp_mix > 1.0f ? 1.0f : p->sp_mix;
    q->in = p->sp_in; q->tg = 1.0f - p->tap_cut;
    if (p->crash != q->crash_seen) {                  /* a kick: the burst from the next sample */
        q->crash_seen = p->crash;
        q->crash_n = (uint32_t)(0.040f * s->fsl);
        q->env = 3.0f; q->crash_amp = 1.0f;
    }
    q->kd = ddm_exp(-3.5f / (0.040f * s->fsl));      /* (30 dB over the 40 ms; three times full scale at first: the tank's clip holds it) */
    q->gd = 0.40f + 0.35f * q->m;                    /* (0.5) in the loop: the allpass tank's diffusion and dispersion, from the mix */
    q->al = q->a * q->m;
}
DD_NOINL static float sp_run(dd_spring_t *q, float x)
{
    int16_t *b = q->buf, *b1 = b + DD_SP_TANK, *b2 = b1 + DD_SP_D1;
    float y, z, d, u;
    int k;
    q->dc += 0.001f * (x - q->dc); x -= q->dc;
    x = sat3(q->gin * x) * q->igin;                                     /* the tube */
    if (q->crash_n) {                                                   /* the kick: the springs hit, not the tube driven */
        q->rng = q->rng * 1664525u + 1013904223u;
        x += q->crash_amp > 0.0f ? q->crash_amp : q->env * (float)(int32_t)q->rng * (1.0f / 2147483648.0f);
        q->crash_amp = 0.0f; q->env *= q->kd; q->crash_n--;
    }
    y = (float)b[(q->w - q->dt) & (DD_SP_TANK - 1u)] * IQ16;            /* the tank: its far end */
    z = y;
    for (k = 0; k < DD_SP_AP; k++) { float o = q->ap[k] - q->a * z; q->ap[k] = z + q->a * o; z = o; }   /* the chirp */
    q->lp += q->glp * (z - q->lp);
    b[q->w] = to_q15(sat3(x + q->g * q->lp));
    q->w = (q->w + 1u) & (DD_SP_TANK - 1u);
    z = q->lp;                                                          /* density: two Schroeder allpasses (stored at half) */
    d = 2.0f * (float)b1[(q->w1 - q->d1) & (DD_SP_D1 - 1u)] * IQ16; u = z + 0.6f * d;
    b1[q->w1] = to_q15(0.5f * u); q->w1 = (q->w1 + 1u) & (DD_SP_D1 - 1u); z = d - 0.6f * u;
    d = 2.0f * (float)b2[(q->w2 - q->d2) & (DD_SP_D2 - 1u)] * IQ16; u = z + 0.6f * d;
    b2[q->w2] = to_q15(0.5f * u); q->w2 = (q->w2 + 1u) & (DD_SP_D2 - 1u); z = d - 0.6f * u;
    q->olp += q->gout * (z - q->olp);
    return q->olp * q->norm;
}
/* (0.5) the spring in the loop: the tank as a Schroeder allpass with its delay and the dispersion chain nested inside it,
 *   w = x + gd D(w),  y = D(w) - gd w,  D = the 37 ms tank then the 12 allpasses (an allpass itself),
 * so its gain is exactly 1 at every frequency whatever gd and the dispersion are: the loop's gain is FEEDBACK, the tail decays below 1
 * and builds above it at the rate it does without the tank. What it does is time: the chirp and the dense repeats every 37 ms (gd, the
 * mix, sets how much goes round). No low-pass inside it (that would not be an allpass): the delay's HCUT, already in the loop, darkens
 * the repeats. The tube in front (unity for small signals) and the tank's bound only take gain away. The tank in float (DD_SP_TANK / 2 values in the caller's memory, 4-byte aligned):
 * an int16 tank at a loop's low levels gave its tail to rounding */
static float sp_loop(dd_spring_t *q, float x)
{
    int16_t *b = q->buf;
    float d, z, w;
    int k;
    q->dc += 0.001f * (x - q->dc); x -= q->dc;
    x = sat3(q->gin * x) * q->igin;
    if (q->crash_n) {                                                   /* the kick */
        q->rng = q->rng * 1664525u + 1013904223u;
        x += q->crash_amp > 0.0f ? q->crash_amp : q->env * (float)(int32_t)q->rng * (1.0f / 2147483648.0f);
        q->crash_amp = 0.0f; q->env *= q->kd; q->crash_n--;
    }
    {   float *tf = (float *)(void *)b;             /* (the tank in float here: DD_SP_TANK / 2 values in the same memory; an int16 tank at
                                                      * the loop's levels loses the tail to its rounding) */
        const uint32_t fm = DD_SP_TANK / 2u - 1u, dt = q->dt < fm - 1u ? q->dt : fm - 1u;
        d = tf[(q->w - dt) & fm];
        z = d;
        for (k = 0; k < DD_SP_AP; k++) { float o = q->ap[k] - q->al * z; q->ap[k] = z + q->al * o; z = o; }
        w = x + q->gd * z;
        w = w > 4.0f ? 4.0f : w < -4.0f ? -4.0f : w;  /* (bounded: the allpass's inner gain is at most 1 / (1 - gd)) */
        tf[q->w & fm] = w;
        q->w = (q->w + 1u) & fm;
    }
    return z - q->gd * w;
}

void dd_spring_block(dd_t *s, const dd_params_t *p, const float *in, float *out, int n)
{
    int i;
    sp_tick(s, p);
    for (i = 0; i < n; i++) out[i] = s->sp.place ? sp_run(&s->sp, in[i]) : 0.0f;
}

/* ------------------------------------------------------------------ control tick --- */
typedef struct {
    float a;                       /* head glide coefficient a loop sample */
    float dt[DD_TAPS];             /* head targets (loop samples) */
    int on[DD_TAPS];
    float ghp, glp, k, mk;         /* filter warps, LP damping, LP makeup */
    float a1, a2, a3, ha1, ha2, ha3, G;
    float gd, igd;                 /* drive in, its makeup */
    float dfb, dsend;              /* per loop sample ramps */
    float fb_end, send_end;        /* .. their values at the tick's end */
    int route;
    float ct, wt, cstep, lceil, lrel;   /* (0.5) the cut's targets (input, wet) and its step a line sample; the limiter's ceiling, release */
} tick_t;

DD_NOINL static void control_tick(dd_t *s, const dd_params_t *p, tick_t *t, int nloop)
{
    int i;
    float step = p->time * s->fsl, g, kh = 1.2f;
    t->a = p->glide > 0.0f ? 1.0f / (p->glide * s->fsl + 1.0f) : 1.0f;
    for (i = 0; i < DD_TAPS; i++) {
        float lv = p->head[i] > 0.0f ? p->level[i] : 0.0f, pn = p->pan[i], c, tl, tr, tf;
        float d = p->head[i] * step - s->lat;
        if (s->algo == DD_ALGO_DIGITAL && i > 0) lv = 0.0f;
        t->on[i] = p->head[i] > 0.0f;
        if (d < 2.0f) d = 2.0f;
        if (d > (float)s->len - 8.0f) d = (float)s->len - 8.0f;
        d = (float)(int)(d + 0.5f);                                    /* a whole sample */
        t->dt[i] = d;
        if (s->d[i] == 0.0f || t->a >= 1.0f) s->d[i] = d;           /* a head switched on (or a jump): no glide */
        c = 1.414f - 0.414f * (pn < 0.0f ? -pn : pn);
        tl = lv * 0.5f * (1.0f - pn) * c; tr = lv * 0.5f * (1.0f + pn) * c;
        tf = t->on[i] ? p->fbw[i] : 0.0f;
        if (s->algo == DD_ALGO_DIGITAL) tf = i == 0 ? 1.0f : 0.0f;
        s->gl[i] += 0.5f * (tl - s->gl[i]); s->gr[i] += 0.5f * (tr - s->gr[i]); s->fw[i] += 0.5f * (tf - s->fw[i]);
    }
    /* filters: HP one-pole (LITE) or a mildly resonant SVF HP (TAPE: a head bump at its cutoff); LP resonant SVF */
    t->route = p->route;
    t->ghp = knob_g(s, p->hp);
    t->G = t->ghp / (1.0f + t->ghp);
    t->ha1 = 1.0f / (1.0f + t->ghp * (t->ghp + kh)); t->ha2 = t->ghp * t->ha1; t->ha3 = t->ghp * t->ha2;
    g = t->glp = knob_g(s, p->lp);
    {   float rk = p->res * 127.0f, rf;
        int ri;
        rk = rk < 0.0f ? 0.0f : rk > 126.999f ? 126.999f : rk; ri = (int)rk; rf = rk - (float)ri;
        t->k = s->ktab[ri] + rf * (s->ktab[ri + 1] - s->ktab[ri]);
        t->mk = s->mktab[ri] + rf * (s->mktab[ri + 1] - s->mktab[ri]); }
    t->a1 = 1.0f / (1.0f + g * (g + t->k)); t->a2 = g * t->a1; t->a3 = g * t->a2;
    t->gd = s->algo == DD_ALGO_DIGITAL ? 1.0f : 1.0f + 3.0f * p->drive * p->drive;   /* ceiling 0 .. -12 dBFS */
    t->igd = 1.0f / t->gd;
    /* FEEDBACK and SEND: a one-pole at the tick (~7 ms, ~2 ms), linear within it */
    t->fb_end = s->fb_cur + 0.1f * (p->feedback - s->fb_cur);
    t->send_end = s->send_cur + 0.3f * (p->send - s->send_cur);
    if (t->fb_end - p->feedback < 1e-5f && p->feedback - t->fb_end < 1e-5f) t->fb_end = p->feedback;
    if (t->send_end - p->send < 1e-5f && p->send - t->send_end < 1e-5f) t->send_end = p->send;
    t->dfb = (t->fb_end - s->fb_cur) / (float)nloop;
    t->dsend = (t->send_end - s->send_cur) / (float)nloop;
    if (s->algo == DD_ALGO_TAPE) {      /* tape-consistent wow / flutter: a head D behind reads d = D - integral(speed) */
        float aw = 0.003f * p->wow, af = 0.0012f * p->flutter, c, sn, r;
        c = s->wo[0] * s->rot_w[0] - s->wo[1] * s->rot_w[1]; sn = s->wo[1] * s->rot_w[0] + s->wo[0] * s->rot_w[1];
        r = 1.5f - 0.5f * (c * c + sn * sn); s->wo[0] = c * r; s->wo[1] = sn * r;
        c = s->fo[0] * s->rot_f[0] - s->fo[1] * s->rot_f[1]; sn = s->fo[1] * s->rot_f[0] + s->fo[0] * s->rot_f[1];
        r = 1.5f - 0.5f * (c * c + sn * sn); s->fo[0] = c * r; s->fo[1] = sn * r;
        s->rng = s->rng * 1664525u + 1013904223u;                     /* slow drift: noise, two one-poles ~0.2 Hz */
        s->drift += 0.004f * ((float)(int32_t)s->rng * (1.0f / 2147483648.0f) - s->drift);
        s->drift_lp += 0.004f * (s->drift - s->drift_lp);
        for (i = 0; i < DD_TAPS; i++) {
            float D = s->d[i], tw = s->wow_w * D * (1.0f / (2.0f * PI_F)), tf2 = s->flut_w * D * (1.0f / (2.0f * PI_F));
            float cw = sin_turn(tw + 0.25f), sw = sin_turn(tw), cf = sin_turn(tf2 + 0.25f), sf = sin_turn(tf2);
            /* integral of A sin(w tau) over [t-D, t] = (A/w)(cos(w(t-D)) - cos(w t)); cos(w(t-D)) = cos cosD + sin sinD */
            float iw = (aw / s->wow_w) * (s->wo[0] * cw + s->wo[1] * sw - s->wo[0]);
            float ifl = (af / s->flut_w) * (s->fo[0] * cf + s->fo[1] * sf - s->fo[0]);
            s->m_prev[i] = s->m_next[i];
            s->m_next[i] = -(iw + ifl) - D * 20.0f * aw * s->drift_lp;
        }
    }
    t->ct = p->cut ? 0.0f : 1.0f; t->wt = p->cut && p->cut_wet ? 0.0f : 1.0f;   /* (0.5) the cut: 3 ms to 0 and back */
    t->cstep = 1.0f / (0.003f * s->fsl);
    t->lceil = p->lim_ceil; t->lrel = ddm_exp(-1.0f / (0.150f * s->fsl));        /* the limiter: 150 ms release */
    sp_tick(s, p);                                     /* (0.4) the spring's gains, a kick */
}

/* (0.5) the wet's limiter (after the loop; the loop keeps its saturator): a peak envelope with an instant attack and a 150 ms release, the
 * gain ceiling / envelope above the ceiling. The wet never passes the ceiling, so a build over FEEDBACK 1 climbs to it and holds there */
static inline void dd_limit(dd_t *s, const tick_t *t, float *l, float *r)
{
    float a = *l < 0.0f ? -*l : *l, b = *r < 0.0f ? -*r : *r, pk = a > b ? a : b, g;
    s->lim_env = pk > s->lim_env ? pk : s->lim_env * t->lrel + (1.0f - t->lrel) * pk;
    g = s->lim_env > t->lceil ? t->lceil / s->lim_env : 1.0f;
    s->lim_g = g;
    *l *= g; *r *= g;
}

/* ------------------------------------------------------------------ the loop at the line's rate --- */
DD_NOINL static void run_lite(dd_t *s, const tick_t *t, const float *x, float *wl, float *wr, int n)
{
    int16_t *b = s->line[0];
    const uint32_t mask = s->mask;
    uint32_t w = s->w;
    float hp_s = s->hp_s[0], ic1 = s->lp1[0], ic2 = s->lp2[0], fb = s->fb_cur, sd = s->send_cur;
    const float sc = IQ16 * t->igd, dig = s->algo == DD_ALGO_DIGITAL;
    const int rec = t->route;
    dd_spring_t *q = &s->sp;
    const int spl = q->place == 1, spo = q->place == 2;   /* (0.4) the spring in the loop, or on the wet */
    float dn = (s->denorm & 1u) ? 1e-20f : -1e-20f, cg = s->cut_g, wg = s->cutw_g;
    int i, j;
    for (i = 0; i < n; i++) {
        float af = 0.0f, l = 0.0f, r = 0.0f, u, v, y, xi;
        fb += t->dfb; sd += t->dsend;
        if (cg != t->ct) { cg += cg < t->ct ? t->cstep : -t->cstep; if (cg < 0.0f) cg = 0.0f; if (cg > 1.0f) cg = 1.0f; }   /* (0.5) the cut */
        if (wg != t->wt) { wg += wg < t->wt ? t->cstep : -t->cstep; if (wg < 0.0f) wg = 0.0f; if (wg > 1.0f) wg = 1.0f; }
        xi = cg * x[i];
        for (j = 0; j < DD_TAPS; j++) {
            if (!t->on[j]) continue;
            if (s->d[j] != t->dt[j]) {
                s->d[j] += t->a * (t->dt[j] - s->d[j]);
                if (s->d[j] - t->dt[j] < 1e-3f && t->dt[j] - s->d[j] < 1e-3f) s->d[j] = t->dt[j];   /* landed */
            }
            y = rd_lin(b, w, mask, s->d[j]) * sc;
            af += s->fw[j] * y; l += s->gl[j] * y; r += s->gr[j] * y;
        }
        if (!dig) {
            if (rec) af = sd * xi + fb * af;                                   /* (record path: filter the sum) */
            v = (af - hp_s) * t->G; u = v + hp_s; hp_s = u + v; af -= u;           /* HP (TPT one-pole) */
            {   float v3 = af - ic2, v1 = t->a1 * ic1 + t->a2 * v3, v2 = ic2 + t->a2 * ic1 + t->a3 * v3;   /* LP SVF */
                ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2; af = v2 * t->mk; }
            if (spl) af = sp_loop(q, af);                                        /* (0.4) every regeneration through the tank (0.5: an allpass) */
            u = rec ? af + dn : sd * xi + fb * af + dn;
            b[w] = to_q15(sat3(t->gd * u));
        } else {
            b[w] = to_q15(sd * xi + fb * af);                                  /* the plain delay: a hard clamp */
        }
        dn = -dn;
        w = (w + 1u) & mask;
        if (spo) {                                                               /* (0.4) the tank on the wet, after the taps */
            float sp = q->m * sp_run(q, 0.5f * (l + r) + q->in * xi);
            l = q->tg * l + sp; r = q->tg * r + sp;
        }
        if (t->lceil > 0.0f) dd_limit(s, t, &l, &r);                            /* (0.5) the wet's limiter, after the loop */
        wl[i] = wg * l; wr[i] = wg * r;
    }
    s->w = w; s->hp_s[0] = hp_s; s->lp1[0] = ic1; s->lp2[0] = ic2; s->fb_cur = fb; s->send_cur = sd;
    s->cut_g = cg; s->cutw_g = wg;
    s->denorm++;
}

DD_NOINL static void run_tape(dd_t *s, const tick_t *t, const float *x, float *wl, float *wr, int n, const dd_params_t *p)
{
    const uint32_t mask = s->mask;
    const int nl = s->lines;
    uint32_t w = s->w;
    float fb = s->fb_cur, sd = s->send_cur;
    const float sc = IQ16 * t->igd, ae = 0.5f, ipre = 1.0f / (1.0f - ae), xf = nl == 2 ? p->xfeed : 0.0f;
    const float dm = 1.0f / (float)n;
    dd_spring_t *q = &s->sp;
    const int spl = q->place == 1, spo = q->place == 2;   /* (0.4) the spring: in the loop of the first line (a mono tank), or on the wet */
    float dn = (s->denorm & 1u) ? 1e-20f : -1e-20f, cg = s->cut_g, wg = s->cutw_g;
    int i, j, c;
    for (i = 0; i < n; i++) {
        float af[2] = {0.0f, 0.0f}, l = 0.0f, r = 0.0f, fi = (float)(i + 1) * dm, xi;
        fb += t->dfb; sd += t->dsend;
        if (cg != t->ct) { cg += cg < t->ct ? t->cstep : -t->cstep; if (cg < 0.0f) cg = 0.0f; if (cg > 1.0f) cg = 1.0f; }   /* (0.5) the cut */
        if (wg != t->wt) { wg += wg < t->wt ? t->cstep : -t->cstep; if (wg < 0.0f) wg = 0.0f; if (wg > 1.0f) wg = 1.0f; }
        xi = cg * x[i];
        for (j = 0; j < DD_TAPS; j++) {
            float m;
            if (!t->on[j]) continue;
            if (s->d[j] != t->dt[j]) {
                s->d[j] += t->a * (t->dt[j] - s->d[j]);
                if (s->d[j] - t->dt[j] < 1e-3f && t->dt[j] - s->d[j] < 1e-3f) s->d[j] = t->dt[j];
            }
            m = s->m_prev[j] + fi * (s->m_next[j] - s->m_prev[j]);
            for (c = 0; c < nl; c++) {
                float y = rd_herm(s->line[c], w, mask, s->d[j] + (c ? -m : m)) * sc;
                af[c] += s->fw[j] * y;
                if (c) { l += s->gr[j] * y; r += s->gl[j] * y; }               /* the second line: mirrored pan */
                else { l += s->gl[j] * y; r += s->gr[j] * y; }
            }
        }
        for (c = 0; c < nl; c++) {
            float a = nl == 2 ? (1.0f - xf) * af[c] + xf * af[c ^ 1] : af[c];
            float in = c ? (1.0f - xf) * sd * xi : sd * xi, u, z, y, F;
            if (t->route) a = in + fb * a;
            {   float v3 = a - s->hp2[c], v1 = t->ha1 * s->hp1[c] + t->ha2 * v3, v2 = s->hp2[c] + t->ha2 * s->hp1[c] + t->ha3 * v3;
                s->hp1[c] = 2.0f * v1 - s->hp1[c]; s->hp2[c] = 2.0f * v2 - s->hp2[c]; a = a - 1.2f * v1 - v2; }   /* HP */
            {   float v3 = a - s->lp2[c], v1 = t->a1 * s->lp1[c] + t->a2 * v3, v2 = s->lp2[c] + t->a2 * s->lp1[c] + t->a3 * v3;
                s->lp1[c] = 2.0f * v1 - s->lp1[c]; s->lp2[c] = 2.0f * v2 - s->lp2[c]; a = v2 * t->mk; }           /* LP */
            if (spl && !c) a = sp_loop(q, a);
            u = t->route ? a + dn : in + fb * a + dn;
            z = (u - ae * s->pre_x1[c]) * ipre; s->pre_x1[c] = u;             /* pre-emphasis: HF saturates first */
            z *= t->gd;
            {   float z1 = s->sat_x1[c], dz = z - z1;                          /* ADAA (1st order) cubic soft clip */
                if (dz > 1e-4f || dz < -1e-4f) { F = sat3_F(z); y = (F - sat3_F(z1)) / dz; }
                else y = sat3(0.5f * (z + z1));
                s->sat_x1[c] = z; }
            y = (1.0f - ae) * y + ae * s->de_y1[c]; s->de_y1[c] = y;           /* de-emphasis (exact inverse) */
            s->line[c][w] = to_q15(y);
        }
        dn = -dn;
        w = (w + 1u) & mask;
        if (spo) {
            float sp = q->m * sp_run(q, 0.5f * (l + r) + q->in * xi);
            l = q->tg * l + sp; r = q->tg * r + sp;
        }
        if (t->lceil > 0.0f) dd_limit(s, t, &l, &r);
        wl[i] = wg * l; wr[i] = wg * r;
    }
    s->w = w; s->fb_cur = fb; s->send_cur = sd;
    s->cut_g = cg; s->cutw_g = wg;
    s->denorm++;
}

/* ------------------------------------------------------------------ the block --- */
void dd_process(dd_t *s, const dd_params_t *p, const float *in, float *outL, float *outR)
{
    float xl[DD_BLOCK], wl[DD_BLOCK], wr[DD_BLOCK];
    const int R = s->R, nloop = DD_BLOCK / R, nt = DD_CTL / R;
    int i, k, q;
    float wet0, dry0, dw, dd;
    tick_t t;
    if (!s->primed) {                                  /* the first block: the gains at their settings, no ramp */
        s->wet_cur = p->wet; s->dry_cur = p->dry; s->fb_cur = p->feedback; s->send_cur = p->send; s->primed = 1;
    }
    wet0 = s->wet_cur; dry0 = s->dry_cur;                /* WET / DRY: a one-pole a block (~8 ms), linear within it */
    {   float we = wet0 + 0.3f * (p->wet - wet0), de = dry0 + 0.3f * (p->dry - dry0);
        if (we - p->wet < 1e-5f && p->wet - we < 1e-5f) we = p->wet;
        if (de - p->dry < 1e-5f && p->dry - de < 1e-5f) de = p->dry;
        dw = (we - wet0) / (float)DD_BLOCK; dd = (de - dry0) / (float)DD_BLOCK; }
    /* down to the line's rate */
    if (R == 1) for (i = 0; i < DD_BLOCK; i++) xl[i] = in[i];
    else if (s->aa == DD_AA_HB) {
        float y, y2;
        for (i = 0, k = 0; i < DD_BLOCK; i++)
            if (hb_dec(&s->dec[0], in[i], &y)) {
                if (R == 2) xl[k++] = y;
                else if (hb_dec(&s->dec[1], y, &y2)) xl[k++] = y2;
            }
    } else {
        for (i = 0, k = 0; i < DD_BLOCK; i++) {
            s->box_acc += in[i];
            if (++s->box_n == R) { xl[k++] = s->box_acc / (float)R; s->box_acc = 0.0f; s->box_n = 0; }
        }
    }
    /* the loop, a control tick at a time */
    for (q = 0; q < nloop; q += nt) {
        control_tick(s, p, &t, nt);
        if (s->algo == DD_ALGO_TAPE) run_tape(s, &t, xl + q, wl + q, wr + q, nt, p);
        else run_lite(s, &t, xl + q, wl + q, wr + q, nt);
        s->fb_cur = t.fb_end; s->send_cur = t.send_end;                       /* (exact at the tick's end) */
    }
    /* back up to 44.1 kHz (in place, from the end: wl[] holds nloop samples) */
    if (R > 1) {
        float tl[DD_BLOCK], tr[DD_BLOCK];
        memcpy(tl, wl, sizeof(float) * (size_t)nloop); memcpy(tr, wr, sizeof(float) * (size_t)nloop);
        if (s->aa == DD_AA_HB) {
            if (R == 4) {            /* 11.025 -> 22.05 */
                float ul[DD_BLOCK / 2], ur[DD_BLOCK / 2];
                for (i = 0; i < nloop; i++) {
                    hb_int(&s->intl[1], tl[i], &ul[2 * i], &ul[2 * i + 1]);
                    hb_int(&s->intr[1], tr[i], &ur[2 * i], &ur[2 * i + 1]);
                }
                memcpy(tl, ul, sizeof ul); memcpy(tr, ur, sizeof ur);
            }
            for (i = 0; i < DD_BLOCK / 2; i++) {
                hb_int(&s->intl[0], tl[i], &wl[2 * i], &wl[2 * i + 1]);
                hb_int(&s->intr[0], tr[i], &wr[2 * i], &wr[2 * i + 1]);
            }
        } else {
            const float iR = 1.0f / (float)R;
            for (i = 0; i < nloop; i++)
                for (k = 0; k < R; k++) {
                    float f = (float)(k + 1) * iR;
                    wl[i * R + k] = s->lin_l[0] + f * (tl[i] - s->lin_l[0]);
                    wr[i * R + k] = s->lin_r[0] + f * (tr[i] - s->lin_r[0]);
                    if (k == R - 1) { s->lin_l[0] = tl[i]; s->lin_r[0] = tr[i]; }
                }
        }
    }
    /* out: dry untouched, the wet through a safety limit (exact to -6 dBFS, a knee to 0 dBFS at +3.5 dBFS) */
    for (i = 0; i < DD_BLOCK; i++) {
        float g = wet0 + dw * (float)(i + 1), dr = dry0 + dd * (float)(i + 1);
        outL[i] = dr * in[i] + (g != 0.0f ? lim_out(g * wl[i]) : 0.0f);
        outR[i] = dr * in[i] + (g != 0.0f ? lim_out(g * wr[i]) : 0.0f);
    }
    s->wet_cur = wet0 + dw * (float)DD_BLOCK; s->dry_cur = dry0 + dd * (float)DD_BLOCK;
    if (s->wet_cur - p->wet < 1e-5f && p->wet - s->wet_cur < 1e-5f) s->wet_cur = p->wet;
    if (s->dry_cur - p->dry < 1e-5f && p->dry - s->dry_cur < 1e-5f) s->dry_cur = p->dry;
}
