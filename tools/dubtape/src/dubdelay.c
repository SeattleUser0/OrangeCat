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
    float dn = (s->denorm & 1u) ? 1e-20f : -1e-20f;
    int i, j;
    for (i = 0; i < n; i++) {
        float af = 0.0f, l = 0.0f, r = 0.0f, u, v, y;
        fb += t->dfb; sd += t->dsend;
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
            if (rec) af = sd * x[i] + fb * af;                                   /* (record path: filter the sum) */
            v = (af - hp_s) * t->G; u = v + hp_s; hp_s = u + v; af -= u;           /* HP (TPT one-pole) */
            {   float v3 = af - ic2, v1 = t->a1 * ic1 + t->a2 * v3, v2 = ic2 + t->a2 * ic1 + t->a3 * v3;   /* LP SVF */
                ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2; af = v2 * t->mk; }
            u = rec ? af + dn : sd * x[i] + fb * af + dn;
            b[w] = to_q15(sat3(t->gd * u));
        } else {
            b[w] = to_q15(sd * x[i] + fb * af);                                  /* the plain delay: a hard clamp */
        }
        dn = -dn;
        w = (w + 1u) & mask;
        wl[i] = l; wr[i] = r;
    }
    s->w = w; s->hp_s[0] = hp_s; s->lp1[0] = ic1; s->lp2[0] = ic2; s->fb_cur = fb; s->send_cur = sd;
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
    float dn = (s->denorm & 1u) ? 1e-20f : -1e-20f;
    int i, j, c;
    for (i = 0; i < n; i++) {
        float af[2] = {0.0f, 0.0f}, l = 0.0f, r = 0.0f, fi = (float)(i + 1) * dm;
        fb += t->dfb; sd += t->dsend;
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
            float in = c ? (1.0f - xf) * sd * x[i] : sd * x[i], u, z, y, F;
            if (t->route) a = in + fb * a;
            {   float v3 = a - s->hp2[c], v1 = t->ha1 * s->hp1[c] + t->ha2 * v3, v2 = s->hp2[c] + t->ha2 * s->hp1[c] + t->ha3 * v3;
                s->hp1[c] = 2.0f * v1 - s->hp1[c]; s->hp2[c] = 2.0f * v2 - s->hp2[c]; a = a - 1.2f * v1 - v2; }   /* HP */
            {   float v3 = a - s->lp2[c], v1 = t->a1 * s->lp1[c] + t->a2 * v3, v2 = s->lp2[c] + t->a2 * s->lp1[c] + t->a3 * v3;
                s->lp1[c] = 2.0f * v1 - s->lp1[c]; s->lp2[c] = 2.0f * v2 - s->lp2[c]; a = v2 * t->mk; }           /* LP */
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
        wl[i] = l; wr[i] = r;
    }
    s->w = w; s->fb_cur = fb; s->send_cur = sd;
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
