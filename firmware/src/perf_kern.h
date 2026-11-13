/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 3 step 8b, FX_MODULES_PLAN.md 17) The FX layer's ring effects GLITCH, TAPE STOP, SHUFFLE and FREEZE: ONE text that the image's resident copy (jfx_perform.c) and the effect module
 * (tools/module/fxlayer_mod.c, orangecat-fxlayer.ocm: kind 3, one bank of four) both include. It was jfx_perform.c's perf_glt_seg, perf_glt_step, perf_glt, perf_tape, perf_scratch,
 * perf_frz_grain, perf_frz_eval, perf_frz, the ADPCM coder and reader, perf_shuf_go, perf_shuf_step, perf_shuf, pf_hash and pb_ring_f (Jiant's perform.c, its comments kept), cut out word for
 * word but for where a value comes from: the state, the 32 KB, the card's knobs, the tempo and SCRATCH's count are read through the context of the call (fx_master.h fx3_ctx_t: `kc`; `ks` is
 * its state) where the old text named jfx_perform.c's statics (pf, sl_buf, perf_gp, beat_samples(), perf_scr). What is new text is at the end: each effect's `start` (its part of the old
 * perf_buf_start) and `run` (the old perf_buf's ring branch and fade over a run of samples, where the old text was called a sample at a time through PERF_RING).
 * tests/fx_layer_module.sh proves the cut changed no sample of the resident render (the printout pinned before it) and that the module's render is the resident's.
 *
 * The includer defines, before this text: PFK_FN (the placement of a kernel function: .text.xip_jfx in the image, nothing in the module), clamp and mulq16 (dsp.c), fx_master.h. With
 * PFK_NO_FX defined only the first part is taken (the loop's constants, the mix and the recorder: what the resident REPEAT and CRUSH use too). */
#define PFK_RAMP_LOG2 7
#define PFK_RAMP (1 << PFK_RAMP_LOG2)                   /* 128 samples, 2.9 ms: the fades (the SLICER's SL_RAMP: jfx_perform.c asserts they are one number) */
#define PFK_SLOPE (32768 / PFK_RAMP)
#define PB_FRAMES FX3_FRAMES
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
enum { GX_GLT, GX_TAPE, GX_SHUF, GX_FRZ };    /* the ring effects: a master kernel's effect id is 1 + this */

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}
/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(fx3_buf_t *pb, uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &pb->f[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(fx3_st_t *st, fx3_buf_t *pb, uint32_t f, int32_t l, int32_t r)
{
    if (!st->odd) {
        st->hl = l;
        st->hr = r;
        st->odd = 1;
        return f;
    }
    st->odd = 0;
    pb_put(pb, f, (st->hl + l) >> 2, (st->hr + r) >> 2);
    return f + 1u;
}

#ifndef PFK_NO_FX
#define ks (kc->st)
#define GL_SPAN 7680u                         /* GLITCH: the longest slice it reads back */
#define TP_LAG 15000u                         /* TAPE STOP: how far it may fall behind */
#define AD_BYTES (FX3_FRAMES * 4u - 256u)     /* SHUFFLE's ADPCM ring: 32512 B, 65024 nibbles, 2.95 s */
#define AD_N (2u * AD_BYTES)
#define AD_TBL (&kc->buf->f[FX3_FRAMES - 64u][0])   /* .. the coder at each 1/16's start: 32 x (place, value, step index) */
_Static_assert(AD_N <= 65536u, "SHUFFLE's ring places in 16 bits");
enum { GA_PASS, GA_RPT, GA_REV, GA_UP, GA_DN, GA_GATE, GA_SKIP, GA_CRU };
enum { SK_NORM, SK_HALF, SK_DBL, SK_STUT };

/* a slice's place in its 2-bar cycle (and its variation) to 32 random bits */
static uint32_t pf_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h = (h ^ (h >> 15)) * 0x2C1B3C6Du;
    return h ^ (h >> 13);
}
/* the loop as a ring (GLITCH, TAPE, FREEZE) at the place s (Q16 of a 44.1 kHz sample), between its two frames */
PFK_FN static void pb_ring_f(const fx3_ctx_t *kc, uint32_t s, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 17;
    int32_t fr = (int32_t)((s >> 2) & 0x7FFFu);
    const int16_t *p = &kc->buf->f[f & (PB_FRAMES - 1u)][0], *q = &kc->buf->f[(f + 1u) & (PB_FRAMES - 1u)][0];
    *l = 2 * (p[0] + (((q[0] - p[0]) * fr) >> 15));
    *r = 2 * (p[1] + (((q[1] - p[1]) * fr) >> 15));
}
/* ------------------------------------------------------------------ GLITCH --- */
static const uint8_t GA_OF[16] = {GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_REV, GA_REV, GA_UP, GA_DN,
                                  GA_GATE, GA_GATE, GA_SKIP, GA_SKIP, GA_CRU, GA_CRU};
/* a segment's start: where it reads (Q16) and how fast */
PFK_FN static void pfk_glt_seg(const fx3_ctx_t *kc)
{
    uint32_t a = ks->u.g.a, L = ks->u.g.l, g0 = ks->u.g.g0, s = g0 - L;
    ks->u.g.rate = 65536;
    if (a == GA_RPT) {
        ks->u.g.rate = (int32_t)kc->semi(clamp((int32_t)(ks->u.g.k < 24u ? ks->u.g.k : 24u) * kc->gp[1], -24, 24));
    } else if (a == GA_REV) {
        s = g0 - 1u;
        ks->u.g.rate = -65536;
    } else if (a == GA_UP) {
        s = g0 - 2u * L;
        ks->u.g.rate = 131072;
    } else if (a == GA_DN) {
        ks->u.g.rate = 32768;
    } else if (a == GA_SKIP) {
        s = g0 - ks->u.g.sp + pf_hash(ks->gs * 64u + (ks->u.g.k >> 1)) % (ks->u.g.sp - L + 1u);
    }
    ks->u.g.src = s << 16;
}
/* GLITCH: the next 1/16's slice, by the hash of its place in two bars and VARI */
PFK_FN static void pfk_glt_step(const fx3_ctx_t *kc)
{
    uint32_t h = pf_hash((ks->gs & 31u) + 0x51u + ((uint32_t)(kc->gp[3] - 1) << 5)), s16 = ks->P ? ks->P : 1u,
             sp = s16 < GL_SPAN ? s16 : GL_SPAN, d = (uint32_t)clamp(kc->gp[0], 0, 100),
             z = (uint32_t)clamp(kc->gp[2], 0, 3), j = (h >> 12) % 3u, a, L;
    a = ((h >> 24) * 100u >> 8) < (ks->gs & 3u ? d : d * d / 100u) ? GA_OF[(h >> 8) & 15u] : GA_PASS;
    ks->u.g.g0 = 2u * ks->wr + ks->odd;                /* (the loop's sample now) */
    if ((s16 + sp > PB_MAX - 512u || ks->u.g.g0 < sp) && a != GA_GATE && a != GA_CRU)
        a = GA_PASS;                                /* (too slow: what it would read is gone; or not recorded yet) */
    L = a == GA_RPT ? sp >> (z + j) : a == GA_GATE ? (s16 / 2u) >> (z / 2u + j % 2u) : a == GA_SKIP ? sp >> (3u + z / 2u + j % 2u)
      : a == GA_UP ? sp / 2u : a == GA_CRU ? s16 : sp;
    ks->u.g.a = (uint8_t)a;
    ks->u.g.l = L < 2u * PFK_RAMP ? 2u * PFK_RAMP : L;
    ks->u.g.sp = sp;
    ks->u.g.h = (uint8_t)(1u + (h >> 14) % 6u);
    ks->u.g.sh = (uint8_t)(6u + j + (h >> 20) % 2u);
    ks->u.g.n = 255u;
    ks->u.g.u = ks->u.g.k = ks->gph = 0;
    pfk_glt_seg(kc);
}
/* GLITCH on one sample: 1 = it plays (ks->yl yr), 0 = live */
PFK_FN static int pfk_glt(const fx3_ctx_t *kc, int32_t l, int32_t r)
{
    uint32_t u = ks->u.g.u, L = ks->u.g.l, a = ks->u.g.a, e = L - 1u - u < u ? L - 1u - u : u, g = 32768u;
    ks->wr = pb_rec(ks, kc->buf, ks->wr, l, r);
    if (a != GA_PASS) {
        if (a == GA_SKIP) {                         /* (each segment windowed; SKIP's edges 0.7 ms: a tick, shaped) */
            if (e < PFK_RAMP / 4u)
                g = e << (17 - PFK_RAMP_LOG2);
        } else if (e < (uint32_t)PFK_RAMP) {
            g = e << (15 - PFK_RAMP_LOG2);
        }
        if (a == GA_GATE) {
            g = ks->u.g.k & 1u ? 0u : g;
            ks->yl = mulq16(l, g << 1);
            ks->yr = mulq16(r, g << 1);
        } else if (a == GA_CRU) {                   /* (live, crushed; windowed as the others) */
            if (++ks->u.g.n >= ks->u.g.h) {
                int32_t b = 1 << (ks->u.g.sh - 1u);
                ks->u.g.n = 0;
                ks->u.g.cl = ((l + b) >> ks->u.g.sh) << ks->u.g.sh;
                ks->u.g.cr = ((r + b) >> ks->u.g.sh) << ks->u.g.sh;
            }
            ks->yl = mulq16(ks->u.g.cl, g << 1);
            ks->yr = mulq16(ks->u.g.cr, g << 1);
        } else {
            pb_ring_f(kc, ks->u.g.src + (uint32_t)((int32_t)u * (ks->u.g.rate >> 2)) * 4u, &ks->yl, &ks->yr);
            ks->yl = mulq16(ks->yl, g << 1);
            ks->yr = mulq16(ks->yr, g << 1);
        }
    }
    if (++ks->u.g.u >= L) {
        ks->u.g.u = 0;
        ks->u.g.k++;
        pfk_glt_seg(kc);
    }
    if (++ks->gph >= ks->P) {
        ks->gs++;
        pfk_glt_step(kc);
    }
    return a != GA_PASS;
}

/* --------------------------------------------------------------- TAPE STOP --- */
PFK_FN static int pfk_tape(const fx3_ctx_t *kc, int32_t l, int32_t r)
{
    uint32_t g;
    if (!ks->state) {
        uint32_t xq = ks->u.t.x >> 9, v = (32768u - xq) * (uint32_t)(32768 + ((ks->u.t.c * (int32_t)xq) >> 15)) >> 15;
        ks->wr = pb_rec(ks, kc->buf, ks->wr, l, r);
        g = v < 8192u ? v << 2 : 32768u;
        pb_ring_f(kc, ks->u.t.p, &ks->yl, &ks->yr);
        ks->u.t.p += v << 1;
        if ((ks->u.t.x += ks->u.t.dx) >= 1u << 24) { /* stopped: the loop kept, the hand on it */
            ks->state = 1;
            ks->u.t.q = ks->u.t.p1 = ks->u.t.p;
        }
    } else {
        uint32_t p0 = ks->u.t.p;
        int32_t d;
        ks->u.t.p1 += (uint32_t)((int32_t)(ks->u.t.q - ks->u.t.p1) >> 8);
        ks->u.t.p += (uint32_t)((int32_t)(ks->u.t.p1 - ks->u.t.p) >> 8);
        d = (int32_t)(ks->u.t.p - p0);
        d = d < 0 ? -d : d;                         /* (its speed, Q16: the level up to a quarter of it) */
        ks->u.t.gn += ((d >= 16384 ? 32768 : d > 1024 ? (d - 1024) << 1 : 0) - ks->u.t.gn) >> 5;
        g = (uint32_t)ks->u.t.gn;
        pb_ring_f(kc, ks->u.t.p, &ks->yl, &ks->yr);
    }
    ks->yl = mulq16(ks->yl, g << 1);
    ks->yr = mulq16(ks->yr, g << 1);
    return 1;
}
/* each block, stopped: KNOB 1's detents since the last move the hand 1/48 beat each, within what the loop holds */
PFK_FN static void pfk_scratch(const fx3_ctx_t *kc)
{
    uint32_t b = kc->beat / 48u;
    int32_t d = clamp((int32_t)(kc->scr - ks->u.t.sc), -64, 64), r0, r1;
    r0 = (int32_t)(((ks->u.t.q >> 16) - 2u * ks->wr) & (PB_MAX - 1u)) - (int32_t)PB_MAX;
    ks->u.t.sc = kc->scr;
    r1 = clamp(r0 + d * (int32_t)(b < 512u ? b : 512u), -(int32_t)(PB_MAX - 1024u), -4);
    ks->u.t.q += (uint32_t)(r1 - r0) << 16;
}
/* ----------------------------------------------------------------- FREEZE --- */
static const int8_t FZ_JIT[4] = {12, -12, 7, -5};
static uint32_t frz_ov(const fx3_ctx_t *kc) { return 100u + 2u * (uint32_t)clamp(kc->gp[0], 0, 100); }
PFK_FN static void pfk_frz_grain(const fx3_ctx_t *kc)
{
    fx3_grain_t *v = &ks->u.f.v[0];
    uint32_t i, r = ks->u.f.rnd = ks->u.f.rnd * 1664525u + 1013904223u, ov = frz_ov(kc),
             sp = (uint32_t)clamp(kc->gp[2], 0, 100), len = kc->beat >> (5u - (uint32_t)clamp(kc->gp[3], 0, 4)),
             span, room, j;
    int32_t n = kc->gp[1], rate;
    for (i = 1; i < 4u; i++)
        if (ks->u.f.v[i].x > v->x)
            v = &ks->u.f.v[i];
    if (((r >> 24) * 100u >> 8) < sp / 2u)
        n += FZ_JIT[(r >> 22) & 3u];
    rate = (int32_t)kc->semi(n);
    if (len < 256u)
        len = 256u;
    ks->u.f.iv = len * 100u / ov;
    ks->u.f.gain = ov > 150u ? (int32_t)(4915200u / ov) : 32768;
    if ((len >> 4) * ((uint32_t)rate >> 12) + 64u > ks->u.f.c)
        len = ((ks->u.f.c - 64u) << 4) / ((uint32_t)rate >> 12);
    v->dx = 65536u / len;
    len = 65535u / v->dx + 1u;
    span = (len >> 4) * ((uint32_t)rate >> 12) + 2u;
    room = span + 48u < ks->u.f.c ? ks->u.f.c - span - 48u : 0u;
    j = room * sp / 100u;
    if (ks->u.f.scan > room)
        ks->u.f.scan = room;
    i = ks->u.f.scan + (((r >> 6) & 0xFFFFu) * j >> 16);
    i = i > j / 2u ? i - j / 2u : 0u;
    v->pos = (16u + (i < room ? i : room)) << 16;
    v->rate = (uint32_t)rate;
    v->x = 0;
    j = ks->u.f.iv / 8u + 1u;
    if (ks->u.f.sd > 0)
        ks->u.f.scan += j;
    else
        ks->u.f.scan = ks->u.f.scan > j ? ks->u.f.scan - j : 0u;
    if (ks->u.f.scan >= room || !ks->u.f.scan)
        ks->u.f.sd = ks->u.f.scan ? -1 : 1;
}
PFK_FN static void pfk_frz_eval(const fx3_ctx_t *kc)
{
    uint32_t i;
    int32_t al = 0, ar = 0;
    if ((ks->u.f.fs += 2u) >= ks->u.f.iv) {
        ks->u.f.fs = 0;
        pfk_frz_grain(kc);
    }
    for (i = 0; i < 4u; i++) {
        fx3_grain_t *v = &ks->u.f.v[i];
        if (v->x < 65536u) {
            int32_t gl, gr, w = (int32_t)((v->x * (65536u - v->x)) >> 15);
            pb_ring_f(kc, v->pos, &gl, &gr);
            al += (gl * w) >> 15;
            ar += (gr * w) >> 15;
            v->pos += 2u * v->rate;
            v->x += 2u * v->dx;
        }
    }
    ks->u.f.nl = mulq16(al, (uint32_t)ks->u.f.gain << 1);
    ks->u.f.nr = mulq16(ar, (uint32_t)ks->u.f.gain << 1);
}
PFK_FN static int pfk_frz(const fx3_ctx_t *kc, int32_t l, int32_t r)
{
    if (!ks->state) {
        uint32_t i, ov;
        ks->wr = pb_rec(ks, kc->buf, ks->wr, l, r);
        if (2u * ks->wr < ks->u.f.c)
            return 0;
        ov = frz_ov(kc) / 100u;
        ks->state = 1;                               /* frozen: DENS grains at once, staggered */
        for (i = 0; i < ov; i++)
            pfk_frz_grain(kc);
        for (i = 0; i < ov; i++)
            ks->u.f.v[i].x = i * (6553600u / frz_ov(kc));
        ks->u.f.fs = 0;
        ks->u.f.ph = 1;
        pfk_frz_eval(kc);
    }
    if ((ks->u.f.ph ^= 1u) != 0u) {
        int32_t pl = ks->u.f.nl, pr = ks->u.f.nr;
        pfk_frz_eval(kc);
        ks->yl = (pl + ks->u.f.nl) >> 1;
        ks->yr = (pr + ks->u.f.nr) >> 1;
    } else {
        ks->yl = ks->u.f.nl;
        ks->yr = ks->u.f.nr;
    }
    return 1;
}
/* ---------------------------------------------------------------- SHUFFLE --- */
/* ADPCM, 4 bits, IMA's way (its index moves, its codes) with its steps 2^(1/8) apart, 8 .. 29696 */
static const uint8_t AD_M[8] = {16, 17, 19, 21, 23, 25, 27, 29};
static const int8_t AD_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static int32_t ad_stp(uint32_t ix) { return (int32_t)((uint32_t)AD_M[ix & 7u] << (ix >> 3)) >> 1; }
/* the value after code c from p (the coder and the reader alike); codes 0 and 8 move nothing, so silence stays 0 */
static int32_t ad_next(uint32_t c, int32_t p, uint8_t *ix)
{
    int32_t s = ad_stp(*ix), d = c & 7u ? s >> 3 : 0;
    if (c & 4u) d += s;
    if (c & 2u) d += s >> 1;
    if (c & 1u) d += s >> 2;
    *ix = (uint8_t)clamp((int32_t)*ix + AD_IDX[c & 7u], 0, 95);
    return clamp(c & 8u ? p - d : p + d, -32768, 32767);
}
static void ad_put(const fx3_ctx_t *kc, int32_t x)
{
    uint32_t w = ks->u.s.ew, c = 0;
    int32_t d = x - ks->u.s.ep, s = ad_stp(ks->u.s.ei);
    uint8_t *b = (uint8_t *)kc->buf->f + (w >> 1);
    if (d < 0) {
        c = 8u;
        d = -d;
    }
    if (d >= s) { c |= 4u; d -= s; }
    if (d >= s >> 1) { c |= 2u; d -= s >> 1; }
    if (d >= s >> 2) c |= 1u;
    ks->u.s.ep = ad_next(c, ks->u.s.ep, &ks->u.s.ei);
    *b = (uint8_t)(w & 1u ? (*b & 0x0Fu) | c << 4 : (*b & 0xF0u) | c);
    ks->u.s.ew = w + 1u < AD_N ? w + 1u : 0u;
}
static int32_t shr_out(const fx3_ctx_t *kc, fx3_shr_t *q)
{
    int32_t y = q->a + (((q->b - q->a) * (int32_t)(q->fr >> 1)) >> 15);
    if ((q->fr += q->rate) >= 65536u) {
        uint32_t w = q->pos;
        q->fr -= 65536u;
        q->a = q->b;
        q->b = (int16_t)ad_next((uint32_t)((const uint8_t *)kc->buf->f)[w >> 1] >> (w & 1u ? 4 : 0) & 15u, q->b, &q->idx);
        q->pos = (uint16_t)(w + 1u < AD_N ? w + 1u : 0u);
    }
    return 2 * y;
}
PFK_FN static void pfk_shuf_go(const fx3_ctx_t *kc, uint32_t j, uint32_t rate)
{
    fx3_shr_t *c = &ks->u.s.r[0];
    const int16_t *t = AD_TBL + 3u * (j & 31u);
    if (j > 31u && c->live)
        return;
    ks->u.s.r[1] = *c;
    ks->u.s.xn = PFK_RAMP;
    c->live = j > 31u;
    c->pos = (uint16_t)t[0];
    c->a = c->b = t[1];
    c->idx = (uint8_t)t[2];
    c->fr = 0;
    c->rate = rate;
}
PFK_FN static void pfk_shuf_step(const fx3_ctx_t *kc)
{
    uint32_t gs = ks->gs, ch = 4u >> clamp(kc->gp[1], 0, 2), s16 = ks->P / 2u ? ks->P / 2u : 1u,
             A = (uint32_t)clamp(kc->gp[0], 0, 100), h, thr, dm, d, k;
    int16_t *t = AD_TBL + 3u * (gs & 31u);
    t[0] = (int16_t)ks->u.s.ew;
    t[1] = (int16_t)ks->u.s.ep;
    t[2] = (int16_t)ks->u.s.ei;
    ks->u.s.nrec++;
    ks->gph = 0;
    if (gs % ch)
        return;
    ks->u.s.cu = 0;
    h = pf_hash((gs & 31u) + 0x3C7u + ((uint32_t)(kc->gp[3] - 1) << 5));
    thr = !(gs & 15u) ? A * A / 100u * A / 100u : !(gs & 3u) ? A * A / 100u : A;
    dm = (AD_N - 2048u) / s16;
    dm = dm > ch + 16u ? 16u : dm > ch ? dm - ch : 0u;
    dm = dm < ks->u.s.nrec - 1u ? dm : ks->u.s.nrec - 1u;
    ks->u.s.kind = SK_NORM;
    if (((h >> 24) * 100u >> 8) >= thr || dm < ch) {
        pfk_shuf_go(kc, 32u, 0);
        return;
    }
    d = ch < 4u && dm >= 4u && (h & 0x100u) ? 4u * (1u + (h >> 9) % (dm / 4u)) : ch * (1u + (h >> 9) % (dm / ch));
    k = (((h >> 16) & 0xFFu) * 100u >> 8) < (uint32_t)clamp(kc->gp[2], 0, 100) ? 1u + (h >> 13) % 3u : SK_NORM;
    ks->u.s.kind = (uint8_t)k;
    ks->u.s.j0 = (uint8_t)((gs - d) & 31u);
    ks->u.s.mid = ch * ks->P / 2u;
    pfk_shuf_go(kc, ks->u.s.j0, k == SK_HALF ? 16384u : k == SK_DBL ? 65536u : 32768u);
}
PFK_FN static int pfk_shuf(const fx3_ctx_t *kc, int32_t l, int32_t r)
{
    fx3_shr_t *c = &ks->u.s.r[0];
    if (ks->odd) {
        ad_put(kc, clamp((ks->hl + l + r) >> 3, -32768, 32767));
        ks->odd = 0;
    } else {
        ks->hl = l + r;
        ks->odd = 1;
    }
    if (c->live) {
        ks->yl = l;
        ks->yr = r;
    } else {
        ks->yl = ks->yr = shr_out(kc, c);
    }
    if (ks->u.s.xn) {
        fx3_shr_t *o = &ks->u.s.r[1];
        int32_t g = (int32_t)ks->u.s.xn--, ol = l, orr = r;
        if (!o->live)
            ol = orr = shr_out(kc, o);
        ks->yl += ((ol - ks->yl) * g) >> PFK_RAMP_LOG2;
        ks->yr += ((orr - ks->yr) * g) >> PFK_RAMP_LOG2;
    }
    if (++ks->u.s.cu == ks->u.s.mid && ks->u.s.kind >= SK_DBL)
        pfk_shuf_go(kc, ks->u.s.j0, c->rate);
    if (++ks->gph >= ks->P) {
        ks->gs++;
        pfk_shuf_step(kc);
    }
    return 1;
}

/* ---- (step 8b) the calls: what the host's perf_buf_start and perf_buf did for a ring effect, as the kernel's `start` and `run` ---- */
/* start: the effect begins. The host has cleared state, odd, wr and u, set P and (GLITCH, SHUFFLE) gs: its first 1/16, the transport's */
PFK_FN static void pfk_glt_start(const fx3_ctx_t *kc) { pfk_glt_step(kc); }
PFK_FN static void pfk_shuf_start(const fx3_ctx_t *kc)
{
    ks->u.s.r[0].live = 1;                /* (live until a chunk moves) */
    pfk_shuf_step(kc);
}
PFK_FN static void pfk_tape_start(const fx3_ctx_t *kc)
{                                         /* two frames behind (quiet), at full speed falling to 0 over TIME */
    uint32_t b = kc->beat;
    int32_t c = kc->gp[2];
    uint32_t t = b >> (4u - (uint32_t)clamp(kc->gp[1], 0, 4)), m = TP_LAG * 600u / (uint32_t)(300 - c);
    ks->u.t.p = (PB_MAX - 4u) << 16;
    ks->u.t.c = c * 327;
    ks->u.t.dx = (1u << 24) / (t < m ? (t > 64u ? t : 64u) : m);
    ks->u.t.sc = kc->scr;
}
PFK_FN static void pfk_frz_start(const fx3_ctx_t *kc)
{                                         /* the eighth after the press, then the grains */
    uint32_t b = kc->beat;
    ks->u.f.c = b / 2u < PB_MAX - 1024u ? (b / 2u > 4096u ? b / 2u : 4096u) : PB_MAX - 1024u;
    ks->u.f.rnd = 0x2545F491u;
    ks->u.f.sd = 1;
    ks->u.f.v[0].x = ks->u.f.v[1].x = ks->u.f.v[2].x = ks->u.f.v[3].x = 65536u;
}
/* run: n samples of the master in place: the share ks->w of what the effect plays, the rest live (the old perf_buf, its ring branch: the effect on the sample, the share towards its target
 * by the fade's slope, the mix). It returns after the sample at which, fading out (go 0), the share is 0: the host then starts the next effect or gives the buffer back (perf_buf_done), as
 * the old text did at that sample */
#define PFK_RUN(name_, one_, head_) \
PFK_FN static uint32_t name_(const fx3_ctx_t *kc, int32_t *bl, int32_t *br, uint32_t n) \
{ \
    uint32_t i; \
    head_ \
    for (i = 0; i < n; i++) { \
        int32_t l = bl[i], r = br[i], yl = 0, yr = 0, on, tw; \
        if ((on = one_(kc, l, r)) != 0) { \
            yl = ks->yl; \
            yr = ks->yr; \
        } \
        tw = on && kc->go ? 32768 : 0; \
        ks->w += clamp(tw - ks->w, -PFK_SLOPE, PFK_SLOPE); \
        if (ks->w) { \
            bl[i] = pf_mix(l, yl, ks->w); \
            br[i] = pf_mix(r, yr, ks->w); \
        } else if (!kc->go) {                           /* faded out */ \
            return i + 1u; \
        } \
    } \
    return n; \
}
PFK_RUN(pfk_glt_run, pfk_glt, )
PFK_RUN(pfk_tape_run, pfk_tape, if (kc->head && ks->state) pfk_scratch(kc);)   /* (the old perf_block: each block, stopped, before its samples) */
PFK_RUN(pfk_shuf_run, pfk_shuf, )
PFK_RUN(pfk_frz_run, pfk_frz, )
#undef PFK_RUN
#undef ks
#define PFK_CALLS_GLT pfk_glt_start, pfk_glt_run
#define PFK_CALLS_TAPE pfk_tape_start, pfk_tape_run
#define PFK_CALLS_SHUF pfk_shuf_start, pfk_shuf_run
#define PFK_CALLS_FRZ pfk_frz_start, pfk_frz_run
/* the bank's capability words (the module's caps blocks; the host's check, fx3_modk_set) */
#define PFK_MODULE_ID FX3_MODULE_ID                     /* 3: docs/orangecat/MODULE_IDS.md */
#define PFK_VERSION (1u << 16)                          /* 0x00MMmmpp: 1.0.0 (written as a shift: the build's hardware-address check reads the plain number as a register address, as REVK_VERSION) */
#define PFK_LEGACY 0u                                   /* no Release 2 key: a project holds nothing of the layer (the knobs are RAM, never stored) */
#define PFK_FLAGS 0u                                    /* no seed (FREEZE's generator starts from a constant in its state), no tail (the host fades the effect out), the cost in the engine unit (0: not measured) */
#define PFK_RATES 1u                                    /* 44.1 kHz: the master (the loop itself holds 22.05 kHz frames) */
#define PFK_WORK 8u                                     /* ram_work_min = ram_work_max: the 32 KB in 4 KB blocks (the host's one arena block JB_SL, lent from the SLICER) */
#define PFK_PARAM_BYTES 4u                              /* the card's four knob bytes (perf_gp's row) */
#endif
