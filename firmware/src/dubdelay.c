/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * dubdelay: DUB TAPE's kernel for Orange Cat Release 2 (see dubdelay.h; from the research prototype 0.5, LITE only). Single precision throughout;
 * no tables: the coefficients are computed at the control tick. Written for -ffp-contract=off -Werror=double-promotion. */
#include "dubdelay.h"
#include "dd_math.h"
#if __STDC_HOSTED__
#include <string.h>
#else
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
#endif

#ifndef DD_POOL
#define DD_POOL __attribute__((section(".pool")))   /* the tables outside .bss (it shares the arena's data bank) */
#endif
#ifndef DD_NOINL
#define DD_NOINL
#endif
#ifndef DD_HOT
#define DD_HOT DD_NOINL                                   /* the per-block kernel (jfx_dub.c: OC_DUB_KSRAM puts it in SRAM) */
#endif
#define DD_HB_M 8                                   /* half-band: 4M-1 = 31 taps */
#define DD_HB_C (2 * DD_HB_M - 1)
#define DD_SIN_N 256
#define DD_PI 3.14159265f
#define DD_Q16 32767.0f
#define DD_IQ16 (1.0f / 32767.0f)

/* the half-band's odd taps (offsets 1, 3, 5 ..; the centre is 0.5): the Kaiser design of the prototype (31 taps, beta 7), computed once on the host, so no table is built at run time */
static const float dd_hb_h[DD_HB_M] = {0.314333438873291f, -0.09460322558879852f, 0.04605904966592789f, -0.023742549121379852f,
                                       0.011624842882156372f, -0.005037464667111635f, 0.0017602996667847037f, -0.000394397386116907f};
/* (Release 2, round 6) converter B: the 44.1 to 22.05 kHz stage at 11 taps (3 odd taps: Kaiser beta 6 over N+2 points, the odd taps scaled so all sum to 1), the 22.05 to 11.025 kHz stage as it was (31 taps). The
 * echoes land about 0.45 ms earlier (shorter filters, outside the loop). The renders of ~/dubtape-research/conv/conv_render.c (DD_CONV=1) are this code, sample for sample (tests/jfx_dub_test.c) */
static const float dd_hb0_h[3] = {0.29532000796148306f, -0.051322375145705076f, 0.0060023671842220966f};
typedef struct { int M; const float *h; } dd_hbc_t;
static const dd_hbc_t DD_ST0 = {3, dd_hb0_h}, DD_ST1 = {DD_HB_M, dd_hb_h};
#define DD_KNOB_LN 0.0543918f                  /* ln(1000) / 127: 20 Hz .. 20 kHz over the knob's 0 .. 127 */
/* (Release 2) the filter coefficients are computed at the control tick, not read from tables: the tables were 2 KB of the arena for every build. tan(pi f / fs) of the knob's
 * frequency, with the cutoff held below 0.45 of the line's rate */
static float dd_knob_g(const dd_t *s, float k)
{
    float f;
    if (k < 0.0f) k = 0.0f;
    if (k > (float)(DD_KNOB - 1)) k = (float)(DD_KNOB - 1);
    f = 20.0f * ddm_exp(k * DD_KNOB_LN);
    if (f > 0.45f * s->fsl) f = 0.45f * s->fsl;
    return ddm_tan(DD_PI * f / s->fsl);
}

void dk_clear_block(dd_t *s, uint32_t k) { if (k < s->nblk) memset(s->blk[k], 0, DD_BLK_N * sizeof(int16_t)); }

void dk_reset(dd_t *s)
{
    int i;
    s->w = 0;
    for (i = 0; i < DD_TAPS; i++) { s->d[i] = 0.0f; s->gl[i] = s->gr[i] = s->fw[i] = 0.0f; }
    s->hp_s = s->lp1 = s->lp2 = 0.0f;
    s->fb_cur = s->send_cur = s->wet_cur = s->dry_cur = 0.0f;
    s->ctl_left = 0;
    memset(s->dec, 0, sizeof s->dec); memset(s->intl, 0, sizeof s->intl); memset(s->intr, 0, sizeof s->intr);
    s->denorm = 0;
    s->primed = 0;
    s->cut_g = s->cutw_g = 1.0f; s->lim_env = 0.0f; s->lim_g = 1.0f;
}

void dk_init(dd_t *s, int R, int16_t *const *blk, uint32_t nblk)
{
    uint32_t k;
    memset(s, 0, sizeof *s);
    if (nblk > DD_MAXBLK) nblk = DD_MAXBLK;
    s->R = R == 4 ? 4 : 2;
    s->nblk = nblk; s->len = nblk << DD_BLK_SH;
    for (k = 0; k < nblk; k++) s->blk[k] = blk[k];
    s->fsl = (float)DD_FS / (float)s->R;
        dk_reset(s);
}

/* the longest head the line holds: its length less the read guard, in seconds (the converters' latency is outside the loop) */
float dk_max_seconds(const dd_t *s) { return ((float)s->len - 8.0f - 40.0f) / s->fsl; }

static inline float dd_clip15(float x) { return x > 1.5f ? 1.5f : x < -1.5f ? -1.5f : x; }
static inline float dd_sat3(float x) { x = dd_clip15(x); return x - 0.148148148f * x * x * x; }
static inline float dd_lim_out(float x)
{
    float a = x < 0.0f ? -x : x, t2;
    if (a <= 0.5f) return x;
    t2 = a - 0.5f;
    a = t2 >= 1.0f ? 1.0f : 0.5f + t2 - 0.5f * t2 * t2;
    return x < 0.0f ? -a : a;
}
static inline int16_t dd_to_q15(float v)
{
    v = v > 1.0f ? 1.0f : v < -1.0f ? -1.0f : v;
    v *= DD_Q16;
    return (int16_t)(v >= 0.0f ? v + 0.5f : v - 0.5f);
}
/* the line through its block table: sample i (0 .. len - 1) */
static inline int16_t *dd_ln_at(const dd_t *s, uint32_t i) { return &s->blk[i >> DD_BLK_SH][i & (DD_BLK_N - 1u)]; }
static inline float dd_rd_lin(const dd_t *s, uint32_t w, float d)
{
    int ip = (int)d;
    float fr = d - (float)ip, x0, x1;
    uint32_t i0 = w >= (uint32_t)ip ? w - (uint32_t)ip : w + s->len - (uint32_t)ip, i1 = i0 ? i0 - 1u : s->len - 1u;
    x0 = (float)*dd_ln_at(s, i0); x1 = (float)*dd_ln_at(s, i1);
    return x0 + fr * (x1 - x0);
}

static inline int dd_hb_dec(dd_hb_t *h, float x, float *y, const dd_hbc_t *c)
{
    int j, C = 2 * c->M - 1;
    float a;
    h->p = (h->p + 1u) & 31u; h->st[h->p] = x;
    h->ph ^= 1u;
    if (h->ph) return 0;
    a = 0.5f * h->st[(h->p - C) & 31u];
    for (j = 0; j < c->M; j++)
        a += c->h[j] * (h->st[(h->p - C + 2u * j + 1u) & 31u] + h->st[(h->p - C - 2u * j - 1u) & 31u]);
    *y = a;
    return 1;
}
static inline void dd_hb_int(dd_hb_t *h, float x, float *y0, float *y1, const dd_hbc_t *c)
{
    int j, M = c->M;
    float a = 0.0f;
    h->p = (h->p + 1u) & 15u; h->st[h->p] = x;
    for (j = 0; j < M; j++)
        a += c->h[j] * (h->st[(h->p - M - j) & 15u] + h->st[(h->p - M + 1u + j) & 15u]);
    *y0 = 2.0f * a;
    *y1 = h->st[(h->p - M + 1u) & 15u];
}

/* ------------------------------------------------------------------ the spring --- */
static uint32_t dd_sp_len(float sec, float fsl, uint32_t cap)
{
    uint32_t n = (uint32_t)(sec * fsl + 0.5f);
    return n < 2u ? 2u : n > cap - 2u ? cap - 2u : n;
}
void dk_spring_attach(dd_t *s, int16_t *buf)
{
    dd_spring_t *q = &s->sp;
    memset(q, 0, sizeof *q);
    q->buf = buf;
    q->tgs = 1.0f;
    if (!buf) return;
    memset(buf, 0, DD_SP_BUF * sizeof(int16_t));
    q->dt = dd_sp_len(0.037f, s->fsl, DD_SP_TANK);
    q->d1 = dd_sp_len(0.0053f, s->fsl, DD_SP_D1);
    q->d2 = dd_sp_len(0.0071f, s->fsl, DD_SP_D2);
    q->a = s->R == 2 ? 0.62f : 0.40f;   /* the chirp's band about the same in hertz at each line rate */
    q->rng = 777u;
}
static void dd_sp_tick(dd_t *s, const dd_params_t *p)
{
    dd_spring_t *q = &s->sp;
    float T = p->sp_decay > 0.05f ? p->sp_decay : 2.0f, tone = p->sp_tone < 0.0f ? 0.0f : p->sp_tone > 1.0f ? 1.0f : p->sp_tone;
    float dr = p->sp_drive < 0.0f ? 0.0f : p->sp_drive > 1.0f ? 1.0f : p->sp_drive;
    int leaving = 0;
    {   int pl = q->buf ? p->spring : 0;
        if (pl != q->place && q->buf) {
            if (q->place == 2 && q->wm > 0.001f) {   /* (Release 2, round 4) leaving the wet: its mix fades out first (the loop's 5 ms), then the tank is cleared; a step of the whole wet spring at the toggle was -31 dBFS */
                pl = 2; leaving = 1;
            } else {                                 /* (0.5) a new placement: the tank read another way, cleared; the wet's mix starts at 0 and fades in */
                memset(q->buf, 0, DD_SP_BUF * sizeof(int16_t));
                memset(q->ap, 0, sizeof q->ap); q->w = q->w1 = q->w2 = 0; q->lp = q->olp = q->dc = 0.0f;
                q->wm = 0.0f; q->tgs = 1.0f;
            }
        }
        q->place = pl;
    }
    q->m = leaving ? 0.0f : p->sp_mix < 0.0f ? 0.0f : p->sp_mix > 1.0f ? 1.0f : p->sp_mix;
    if (q->place == 1 && q->m < 0.0005f && q->wm < 0.001f) {                    /* (Release 2) the loop spring bypassed: after about 0.5 s the tank is cleared (once), so it comes back from silence */
        if (q->idle < 0xFFFFu && ++q->idle == 700u && q->buf) {
            memset(q->buf, 0, DD_SP_BUF * sizeof(int16_t));
            memset(q->ap, 0, sizeof q->ap); q->w = q->w1 = q->w2 = 0; q->lp = q->olp = q->dc = 0.0f;
        }
    } else q->idle = 0;
    if (!q->place) return;
    q->g = ddm_exp(-6.90775528f * ((float)q->dt / s->fsl) / T);
    q->glp = 1.0f - ddm_exp(-2.0f * DD_PI * (1600.0f + 3000.0f * tone) / s->fsl);
    q->gout = 1.0f - ddm_exp(-2.0f * DD_PI * (3000.0f + 3000.0f * tone) / s->fsl);
    q->norm = q->place == 1 ? 1.0f - q->g : ddm_sqrt(1.0f - q->g * q->g);
    q->gin = 1.0f + 6.0f * dr * dr;                  /* the tube: in the loop made up whole (unity for small signals: the loop's gain stays
                                                      * FEEDBACK), on the wet half (a driven tube is louder) */
    q->igin = q->place == 1 ? 1.0f / q->gin : 1.0f / ddm_sqrt(q->gin);
    q->in = p->sp_in; q->tg = leaving ? 1.0f : 1.0f - p->tap_cut;
    if (p->crash != q->crash_seen) {                  /* a kick: the burst from the next sample */
        q->crash_seen = p->crash;
        q->crash_n = (uint32_t)(0.040f * s->fsl);
        q->env = 3.0f; q->crash_amp = 1.0f;
    }
    q->kd = ddm_exp(-3.5f / (0.040f * s->fsl));      /* (30 dB over the 40 ms; three times full scale at first: the tank's clip holds it) */
    q->gd = 0.40f + 0.35f * q->m;                    /* (0.5) in the loop: the allpass tank's diffusion and dispersion, from the mix */
    q->al = q->a * q->m;
}
DD_HOT static float dd_sp_run(dd_spring_t *q, float x)
{
    int16_t *b = q->buf, *b1 = b + DD_SP_TANK, *b2 = b1 + DD_SP_D1;
    float y, z, d, u;
    int k;
    q->dc += 0.001f * (x - q->dc); x -= q->dc;
    x = dd_sat3(q->gin * x) * q->igin;                                     /* the tube */
    if (q->crash_n) {                                                   /* the kick: the springs hit, not the tube driven */
        q->rng = q->rng * 1664525u + 1013904223u;
        x += q->crash_amp > 0.0f ? q->crash_amp : q->env * (float)(int32_t)q->rng * (1.0f / 2147483648.0f);
        q->crash_amp = 0.0f; q->env *= q->kd; q->crash_n--;
    }
    y = (float)b[(q->w - q->dt) & (DD_SP_TANK - 1u)] * DD_IQ16;            /* the tank: its far end */
    z = y;
    for (k = 0; k < DD_SP_AP; k++) { float o = q->ap[k] - q->a * z; q->ap[k] = z + q->a * o; z = o; }   /* the chirp */
    q->lp += q->glp * (z - q->lp);
    b[q->w] = dd_to_q15(dd_sat3(x + q->g * q->lp));
    q->w = (q->w + 1u) & (DD_SP_TANK - 1u);
    z = q->lp;                                                          /* density: two Schroeder allpasses (stored at half) */
    d = 2.0f * (float)b1[(q->w1 - q->d1) & (DD_SP_D1 - 1u)] * DD_IQ16; u = z + 0.6f * d;
    b1[q->w1] = dd_to_q15(0.5f * u); q->w1 = (q->w1 + 1u) & (DD_SP_D1 - 1u); z = d - 0.6f * u;
    d = 2.0f * (float)b2[(q->w2 - q->d2) & (DD_SP_D2 - 1u)] * DD_IQ16; u = z + 0.6f * d;
    b2[q->w2] = dd_to_q15(0.5f * u); q->w2 = (q->w2 + 1u) & (DD_SP_D2 - 1u); z = d - 0.6f * u;
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
static float dd_sp_loop(dd_spring_t *q, float x)
{
    int16_t *b = q->buf;
    float d, z, w;
    int k;
    q->dc += 0.001f * (x - q->dc); x -= q->dc;
    x = dd_sat3(q->gin * x) * q->igin;
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


/* ------------------------------------------------------------------ control tick --- */
typedef struct {
    float a;                       /* head glide coefficient a loop sample */
    float dt[DD_TAPS];             /* head targets (loop samples) */
    int on[DD_TAPS];
    float ghp, glp, k, mk;
    float a1, a2, a3, G;
    float gd, igd;                 /* drive in, its makeup */
    float dfb, dsend;
    float fb_end, send_end;
    int route;
    float ct, wt, cstep, lceil, lrel;
} dd_tick_t;

DD_NOINL static void dd_control_tick(dd_t *s, const dd_params_t *p, dd_tick_t *t, int nloop)
{
    int i;
    float step = p->time * s->fsl, g;
    t->a = p->glide > 0.0f ? 1.0f / (p->glide * s->fsl + 1.0f) : 1.0f;
    for (i = 0; i < DD_TAPS; i++) {
        float lv = p->head[i] > 0.0f ? p->level[i] : 0.0f, pn = p->pan[i], c, tl, tr, tf;
        float d = p->head[i] * step;
        t->on[i] = p->head[i] > 0.0f;
        if (d < 2.0f) d = 2.0f;
        if (d > (float)s->len - 8.0f) d = (float)s->len - 8.0f;
        d = (float)(int)(d + 0.5f);                                    /* a whole sample */
        t->dt[i] = d;
        if (s->d[i] == 0.0f || t->a >= 1.0f) s->d[i] = d;           /* a head switched on (or a jump): no glide */
        c = 1.414f - 0.414f * (pn < 0.0f ? -pn : pn);
        tl = lv * 0.5f * (1.0f - pn) * c; tr = lv * 0.5f * (1.0f + pn) * c;
        tf = t->on[i] ? p->fbw[i] : 0.0f;
        s->gl[i] += 0.5f * (tl - s->gl[i]); s->gr[i] += 0.5f * (tr - s->gr[i]); s->fw[i] += 0.5f * (tf - s->fw[i]);
    }
    t->route = p->route;
    t->ghp = dd_knob_g(s, p->hp);
    t->G = t->ghp / (1.0f + t->ghp);
    g = t->glp = dd_knob_g(s, p->lp);
    {   float kk = 2.0f - 1.75f * (p->res < 0.0f ? 0.0f : p->res > 1.0f ? 1.0f : p->res), q = 1.0f / kk;      /* LP damping 2 (Q 0.5) .. 0.25 (Q 4) and 1 / its resonant peak */
        t->k = kk;
        t->mk = q > 0.7072f ? ddm_sqrt(1.0f - 0.25f / (q * q)) / q : 1.0f; }
    t->a1 = 1.0f / (1.0f + g * (g + t->k)); t->a2 = g * t->a1; t->a3 = g * t->a2;
    t->gd = 1.0f + 3.0f * p->drive * p->drive;
    t->igd = 1.0f / t->gd;
    t->fb_end = s->fb_cur + 0.1f * (p->feedback - s->fb_cur);
    t->send_end = s->send_cur + 0.3f * (p->send - s->send_cur);
    if (t->fb_end - p->feedback < 1e-5f && p->feedback - t->fb_end < 1e-5f) t->fb_end = p->feedback;
    if (t->send_end - p->send < 1e-5f && p->send - t->send_end < 1e-5f) t->send_end = p->send;
    t->dfb = (t->fb_end - s->fb_cur) / (float)nloop;
    t->dsend = (t->send_end - s->send_cur) / (float)nloop;
    t->ct = p->cut ? 0.0f : 1.0f; t->wt = p->cut && p->cut_wet ? 0.0f : 1.0f;
    t->cstep = 1.0f / (0.003f * s->fsl);
    t->lceil = p->lim_ceil; t->lrel = ddm_exp(-1.0f / (0.150f * s->fsl));
    dd_sp_tick(s, p);
}

static inline void dd_limit(dd_t *s, const dd_tick_t *t, float *l, float *r)
{
    float a = *l < 0.0f ? -*l : *l, b = *r < 0.0f ? -*r : *r, pk = a > b ? a : b, g;
    s->lim_env = pk > s->lim_env ? pk : s->lim_env * t->lrel + (1.0f - t->lrel) * pk;
    g = s->lim_env > t->lceil ? t->lceil / s->lim_env : 1.0f;
    s->lim_g = g;
    *l *= g; *r *= g;
}

/* ------------------------------------------------------------------ the loop at the line's rate --- */
DD_HOT static void dd_run_lite(dd_t *s, const dd_tick_t *t, const float *x, float *wl, float *wr, int n)
{
    const uint32_t len = s->len;
    uint32_t w = s->w;
    float hp_s = s->hp_s, ic1 = s->lp1, ic2 = s->lp2, fb = s->fb_cur, sd = s->send_cur;
    const float sc = DD_IQ16 * t->igd;
    const int rec = t->route;
    dd_spring_t *q = &s->sp;
    const int spl = q->place == 1 && (q->m > 0.0f || q->wm > 0.001f), spo = q->place == 2;   /* the spring in the loop (crossfaded: the tank is an allpass even at a mix near 0, so it fades in and out), or on the wet */
    const float wk = 1.0f / (0.005f * s->fsl);
    float wm = q->wm, tgs = q->tgs;
    float dn = (s->denorm & 1u) ? 1e-20f : -1e-20f, cg = s->cut_g, wg = s->cutw_g;
    int i, j;
    for (i = 0; i < n; i++) {
        float af = 0.0f, l = 0.0f, r = 0.0f, u, v, y, xi;
        fb += t->dfb; sd += t->dsend;
        if (cg != t->ct) { cg += cg < t->ct ? t->cstep : -t->cstep; if (cg < 0.0f) cg = 0.0f; if (cg > 1.0f) cg = 1.0f; }
        if (wg != t->wt) { wg += wg < t->wt ? t->cstep : -t->cstep; if (wg < 0.0f) wg = 0.0f; if (wg > 1.0f) wg = 1.0f; }
        xi = cg * x[i];
        for (j = 0; j < DD_TAPS; j++) {
            if (!t->on[j]) continue;
            if (s->d[j] != t->dt[j]) {
                s->d[j] += t->a * (t->dt[j] - s->d[j]);
                if (s->d[j] - t->dt[j] < 1e-3f && t->dt[j] - s->d[j] < 1e-3f) s->d[j] = t->dt[j];
            }
            y = dd_rd_lin(s, w, s->d[j]) * sc;
            af += s->fw[j] * y; l += s->gl[j] * y; r += s->gr[j] * y;
        }
        if (rec) af = sd * xi + fb * af;
        v = (af - hp_s) * t->G; u = v + hp_s; hp_s = u + v; af -= u;
        {   float v3 = af - ic2, v1 = t->a1 * ic1 + t->a2 * v3, v2 = ic2 + t->a2 * ic1 + t->a3 * v3;
            ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2; af = v2 * t->mk; }
        if (spl) {
            float y = dd_sp_loop(q, af);
            wm += (q->m - wm) * wk;
            af += wm * (y - af);
        }
        u = rec ? af + dn : sd * xi + fb * af + dn;
        *dd_ln_at(s, w) = dd_to_q15(dd_sat3(t->gd * u));
        dn = -dn;
        if (++w == len) w = 0;
        if (spo) {
            float sp;
            wm += (q->m - wm) * wk;                                 /* (the wet spring's mix glides as the loop's does: 5 ms) */
            tgs += (q->tg - tgs) * wk;                              /* .. and its tap cut */
            sp = wm * dd_sp_run(q, 0.5f * (l + r) + q->in * xi);
            l = tgs * l + sp; r = tgs * r + sp;
        }
        if (t->lceil > 0.0f) dd_limit(s, t, &l, &r);
        wl[i] = wg * l; wr[i] = wg * r;
    }
    s->w = w; s->hp_s = hp_s; s->lp1 = ic1; s->lp2 = ic2; s->fb_cur = fb; s->send_cur = sd;
    s->cut_g = cg; s->cutw_g = wg;
    q->wm = wm; q->tgs = tgs;
    s->denorm++;
}

/* ------------------------------------------------------------------ the block --- */
void dk_process(dd_t *s, const dd_params_t *p, const float *in, float *outL, float *outR)
{
    float xl[DD_BLOCK], wl[DD_BLOCK], wr[DD_BLOCK];
    const int R = s->R, nloop = DD_BLOCK / R;
    const dd_hbc_t *st0 = R == 4 ? &DD_ST0 : &DD_ST1;     /* converter B is the quarter rate's first stage; the half rate's one stage stays the 31-tap filter (it has no second stage behind it to catch what 11 taps let by) */
    int i, k;
    float wet0, dry0, dw, dd;
    dd_tick_t t;
    if (!s->primed) {
        s->wet_cur = p->wet; s->dry_cur = p->dry; s->fb_cur = p->feedback; s->send_cur = p->send; s->primed = 1;
    }
    wet0 = s->wet_cur; dry0 = s->dry_cur;
    {   float we = wet0 + 0.3f * (p->wet - wet0), de = dry0 + 0.3f * (p->dry - dry0);
        if (we - p->wet < 1e-5f && p->wet - we < 1e-5f) we = p->wet;
        if (de - p->dry < 1e-5f && p->dry - de < 1e-5f) de = p->dry;
        dw = (we - wet0) / (float)DD_BLOCK; dd = (de - dry0) / (float)DD_BLOCK; }
    {   float y, y2;
        for (i = 0, k = 0; i < DD_BLOCK; i++)
            if (dd_hb_dec(&s->dec[0], in[i], &y, st0)) {
                if (R == 2) xl[k++] = y;
                else if (dd_hb_dec(&s->dec[1], y, &y2, &DD_ST1)) xl[k++] = y2;
            }
    }
    dd_control_tick(s, p, &t, nloop);
    dd_run_lite(s, &t, xl, wl, wr, nloop);
    s->fb_cur = t.fb_end; s->send_cur = t.send_end;
    {   float tl[DD_BLOCK], tr[DD_BLOCK];
        memcpy(tl, wl, sizeof(float) * (size_t)nloop); memcpy(tr, wr, sizeof(float) * (size_t)nloop);
        if (R == 4) {
            float ul[DD_BLOCK / 2], ur[DD_BLOCK / 2];
            for (i = 0; i < nloop; i++) {
                dd_hb_int(&s->intl[1], tl[i], &ul[2 * i], &ul[2 * i + 1], &DD_ST1);
                dd_hb_int(&s->intr[1], tr[i], &ur[2 * i], &ur[2 * i + 1], &DD_ST1);
            }
            memcpy(tl, ul, sizeof ul); memcpy(tr, ur, sizeof ur);
        }
        for (i = 0; i < DD_BLOCK / 2; i++) {
            dd_hb_int(&s->intl[0], tl[i], &wl[2 * i], &wl[2 * i + 1], st0);
            dd_hb_int(&s->intr[0], tr[i], &wr[2 * i], &wr[2 * i + 1], st0);
        }
    }
    for (i = 0; i < DD_BLOCK; i++) {
        float g = wet0 + dw * (float)(i + 1), dr = dry0 + dd * (float)(i + 1);
        outL[i] = dr * in[i] + (g != 0.0f ? dd_lim_out(g * wl[i]) : 0.0f);
        outR[i] = dr * in[i] + (g != 0.0f ? dd_lim_out(g * wr[i]) : 0.0f);
    }
    s->wet_cur = wet0 + dw * (float)DD_BLOCK; s->dry_cur = dry0 + dd * (float)DD_BLOCK;
    if (s->wet_cur - p->wet < 1e-5f && p->wet - s->wet_cur < 1e-5f) s->wet_cur = p->wet;
    if (s->dry_cur - p->dry < 1e-5f && p->dry - s->dry_cur < 1e-5f) s->dry_cur = p->dry;
}
