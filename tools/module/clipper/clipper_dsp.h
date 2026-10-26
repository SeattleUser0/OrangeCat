/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca's dsp.c); the Clipper parts Copyright (C) 2026 3dSam (Clipper).
 * (Orange Cat, this file) Pieces of Clipper's firmware/src/dsp.c (https://github.com/isod89/clipper-fm1, commit e690672), extracted as they are: the ones
 * Clipper added to Felucca's dsp.c that Orange Cat's dsp.c does not have and the Clipper modules read (tools/module/<name>_mod.c, after Orange Cat's
 * dsp.c and tools/module/caravel/felucca_osc.h). Each block is Clipper's text unchanged; only the selection and this note are Orange Cat's. Where a
 * name of Clipper's is also one of Orange Cat's dsp.c with other code, the module renames Clipper's (the module unit says which).
 *   - the four-pole TPT ladder (ladder_t, mul_sh, ladder_coef, ladder_tick): JUNO, ACID's filter, VECTOR's and WAVE's MOOG model
 *   - from Clipper's eng_hybrid.c (3dSam): hy_saw, HYBRID's band-limited saw, which ACID reads too
 *   - Felucca's tsvf_coef (Orange Cat's dsp.c has its arithmetic as tsvf_coef_k): STRINGS
 *   - the filter models (fmod_t, fmod_coef, fmod_tick and their helpers; core.h's FM_ enum): VECTOR, WAVE (the model is Clipper's track
 *     parameter P_FMODEL, which Orange Cat's track has not got: the modules play FM_SVF, as Clipper's groovebox build does)
 */
#ifndef CLIPPER_DSP_H
#define CLIPPER_DSP_H 1
/* Clipper: a four-pole transistor ladder (the Moog type), after V. Zavalishin's TPT ladder ("The Art of VA
 * Filter Design", ch. 5): four one-pole trapezoidal low-passes, the feedback solved in the same sample (zero-delay
 * feedback: the cutoff and the resonance stay right up to the top), and a tanh at the ladder's input (its
 * differential pair): a hot signal drives it warm, the resonance saturates softly, and from k = 4 it sings on its
 * own. The passband is partly kept at high resonance (the input x (1 + k / 2)), as on the later Moogs.
 * Inside: signals Q12 (no dead band even at 30 Hz, where G is 1/500), coefficients Q16, products and sums in
 * 64 bits (as FM6, PHYS), rounded; G <= 0.75 (g <= 3, 17.6 kHz): the states stay within 3 times the tanh's
 * ceiling (65536), 8e8 in Q12. */
typedef struct { int32_t G, H, G4, inv, k; } ladder_t;   /* G, 1 - G, G^4, 1 / (1 + k G^4): Q16; k: Q12 */
static inline int32_t mul_sh(int32_t a, int32_t b, int sh) { return (int32_t)(((int64_t)a * b) >> sh); }
static inline void ladder_coef(ladder_t *c, int32_t cut, int32_t res)   /* cut 0..127 << 8, res 0..127 */
{
    int32_t i, g, G, G2;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    g = clamp(g, 1, 3 * 4096);
    G = (int32_t)(((uint32_t)g << 16) / (uint32_t)(4096 + g));   /* g / (1 + g) */
    G2 = mul_sh(G, G, 16);
    c->G = G;
    c->H = 65536 - G;
    c->G4 = mul_sh(G2, G2, 16);
    c->k = clamp(res, 0, 127) * 16900 / 127;                     /* 0 .. 4.13 */
    c->inv = (int32_t)(0xFFFFFFFFu / (uint32_t)(65536 + mul_sh(c->k, c->G4, 12)));   /* (32 bits: the FM-1 has
                                                                                      * no 64-bit divide) */
}
/* one sample: x (clamped to +-131072); s[4] the stages (Q12) */
static inline int32_t ladder_tick(const ladder_t *c, int32_t x, int32_t *s)
{
    int64_t in = (int64_t)clamp(x, -131072, 131072) << 12, S, y, u;
    int32_t G = c->G, uq, v, j;
    in += (in * c->k) >> 13;                                     /* x (1 + k / 2) */
    S = ((int64_t)G * s[0] >> 16) + s[1];
    S = ((G * S) >> 16) + s[2];
    S = ((G * S) >> 16) + s[3];
    S = (c->H * S) >> 16;
    y = ((((int64_t)c->G4 * in) >> 16) + S) * c->inv >> 16;    /* the ladder's output, solved */
    u = in - ((c->k * y) >> 12);                                 /* the ladder's input (Q12) */
    {                                                            /* tanh at 65536: on u / 2 (softclip's range), the
                                                                  * bits below a unit added back (its slope ~1 there) */
        int64_t h = u >> 13;
        if (h <= -65536 || h >= 65536)
            uq = softclip(h < 0 ? -65536 : 65536) << 13;
        else
            uq = (softclip((int32_t)h) << 13) + (int32_t)(u - (h << 13));
    }
    for (j = 0; j < 4; j++) {
        v = (int32_t)(((int64_t)G * (uq - s[j]) + 32768) >> 16);
        uq = v + s[j];
        s[j] = uq + v;
    }
    return (uq + 2048) >> 12;
}
/* (Felucca's, in Clipper's dsp.c) */
static inline void tsvf_coef(tsvf_t *c, int32_t cut, int32_t reso)   /* cut: 0..127 << 8 */
{
    int32_t i, g, den, k = 8192 - reso * 7600 / 127;   /* damping 2.0 .. ~0.15 (Q12) */
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)                                       /* between table points: sweeps without 128 steps */
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    den = 4096 + ((g * (g + k)) >> 12);
    c->a1 = (int32_t)((4096u << 13) / (uint32_t)den);
    c->a2 = (c->a1 * g) >> 12;
    c->a3 = (c->a2 * g) >> 12;
}
/* (Clipper's core.h) */
enum { FM_SVF = 1, FM_MOOG = 2, FM_MS20, FM_DIODE, FM_SEM, FM_NOTCH, FM_BP, FM_HP, FM_LAST = FM_HP };   /* (0.9: dsp.c fmod_*) */
/* Clipper 0.9: the filter MODELs of FX > FILTER > MODL (P_FMODEL), for the engines with a low-pass of their own
 * (ANALOG, WAVE, VECTOR): their cutoff and resonance, another circuit. SVF and MOOG as before, bit for bit; then
 *   MS20   two one-pole low-passes (12 dB) with the feedback through a saturation, after the Korg MS-20's Sallen-Key
 *          (Zavalishin's Korg-35 idea, ZDF as the ladder): bright, a screaming resonance that bites back on loud notes
 *   DIODE  four poles, the first at half the cutoff (the diode ladder's coupled first stage, the TB-303's idea): 18 dB
 *          or so, a rubbery resonance, the input driven
 *   SEM    the state-variable low-pass driven softly, its resonance short of oscillating (the Oberheim SEM's)
 *   NOTCH BP HP  the same SVF's other outputs (the SEM's mode knob)
 * fmod_tick takes the engine's signal (full scale) and gives the filtered one at half scale, as tsvf_lp(x / 2) and
 * ladder_tick(x) / 2 did. State: sv[2] (the SVF family) and ls[4] (the ladder family), the engine's */
typedef struct { int32_t G, H, G1, H1, PG, inv, k; } zdf_t;   /* Q16; k Q12 */
typedef struct {
    uint8_t model;
    tsvf_t sv;
    int32_t k;                                         /* the SVF's damping (Q12) */
    ladder_t lad;
    zdf_t z;
} fmod_t;
static inline int64_t clamp64(int64_t x, int64_t lo, int64_t hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int32_t zdf_sat(int64_t u)               /* the ladder's tanh, at 65536 (Q12) */
{
    int64_t h = u >> 13;
    if (h <= -65536 || h >= 65536)
        return softclip(h < 0 ? -65536 : 65536) << 13;
    return (softclip((int32_t)h) << 13) + (int32_t)(u - (h << 13));
}
static inline int32_t fmod_g(int32_t cut)              /* the prewarped gain g (Q12), as the ladder's */
{
    int32_t i, g;
    cut = clamp(cut, 0, 127 << 8);
    i = cut >> 8;
    g = SVF_G[i];
    if (i < 127)
        g += ((SVF_G[i + 1] - g) * (cut & 255)) >> 8;
    return clamp(g, 1, 3 * 4096);
}
static inline void fmod_coef(fmod_t *f, int32_t model, int32_t cut, int32_t res)   /* cut 0..127 << 8, res 0..127 */
{
    f->model = (uint8_t)model;
    res = clamp(res, 0, 127);
    if (model == FM_MOOG) {
        ladder_coef(&f->lad, cut, res);
    } else if (model == FM_MS20 || model == FM_DIODE) {
        zdf_t *z = &f->z;
        int32_t g = fmod_g(cut), G, G2;
        G = (int32_t)(((uint32_t)g << 16) / (uint32_t)(4096 + g));
        z->G = G, z->H = 65536 - G;
        G2 = mul_sh(G, G, 16);
        if (model == FM_MS20) {
            z->k = res * 9000 / 127;                   /* 0 .. 2.2: it sings from 2 */
            z->PG = G2;
        } else {
            int32_t g1 = g >> 1;
            z->G1 = (int32_t)(((uint32_t)g1 << 16) / (uint32_t)(4096 + g1));
            z->H1 = 65536 - z->G1;
            z->k = res * 19000 / 127;                  /* 0 .. 4.6 */
            z->PG = mul_sh(mul_sh(z->G1, G2, 16), G, 16);
        }
        z->inv = (int32_t)(0xFFFFFFFFu / (uint32_t)(65536 + mul_sh(z->k, z->PG, 12)));
    } else {
        int32_t r = model == FM_SEM ? res * 100 / 127 : res;   /* (the SEM: short of oscillating) */
        tsvf_coef(&f->sv, cut, r);
        f->k = 8192 - r * 7600 / 127;
    }
}
static inline int32_t fmod_tick(const fmod_t *f, int32_t x, int32_t *sv, int32_t *ls)
{
    switch (f->model) {
    case FM_MOOG:
        return ladder_tick(&f->lad, x, ls) >> 1;
    case FM_MS20: {                                    /* two poles, the feedback saturated */
        const zdf_t *z = &f->z;
        int64_t in = (int64_t)clamp(x, -131072, 131072) << 12, S, y;
        int32_t u, v;
        in += (in * z->k) >> 14;                       /* (a little of the passband back) */
        S = ((((int64_t)z->G * ls[0]) >> 16) * z->H >> 16) + ((int64_t)z->H * ls[1] >> 16);
        y = ((((int64_t)z->PG * in) >> 16) + S) * z->inv >> 16;
        u = (int32_t)clamp64(in - (((int64_t)z->k * zdf_sat(y)) >> 12), -(int64_t)400000000, 400000000);
        v = (int32_t)(((int64_t)z->G * (u - ls[0]) + 32768) >> 16);
        u = v + ls[0], ls[0] = u + v;
        v = (int32_t)(((int64_t)z->G * (u - ls[1]) + 32768) >> 16);
        u = v + ls[1], ls[1] = u + v;
        return (u + 4096) >> 13;
    }
    case FM_DIODE: {                                   /* four poles, the first at half the cutoff */
        const zdf_t *z = &f->z;
        int64_t in = (int64_t)clamp(x, -131072, 131072) << 12, S, T, y;
        int32_t u, v, j;
        in += (in * z->k) >> 13;
        T = (((int64_t)z->G * ls[1]) >> 16) + ls[2];
        T = ((z->G * T) >> 16) + ls[3];
        T = (z->H * T) >> 16;
        S = mul_sh(mul_sh(mul_sh(z->G, z->G, 16), z->G, 16), z->H1, 16);
        S = ((S * (int64_t)ls[0]) >> 16) + T;
        y = ((((int64_t)z->PG * in) >> 16) + S) * z->inv >> 16;
        u = zdf_sat(in - ((z->k * y) >> 12));
        v = (int32_t)(((int64_t)z->G1 * (u - ls[0]) + 32768) >> 16);
        u = v + ls[0], ls[0] = u + v;
        for (j = 1; j < 4; j++) {
            v = (int32_t)(((int64_t)z->G * (u - ls[j]) + 32768) >> 16);
            u = v + ls[j], ls[j] = u + v;
        }
        return (u + 4096) >> 13;
    }
    case FM_SVF:
        return tsvf_lp(&f->sv, x >> 1, &sv[0], &sv[1]);
    default: {                                         /* SEM NOTCH BP HP: one SVF, its outputs */
        const tsvf_t *c = &f->sv;
        int32_t in = x >> 1, v1, v2, v3;
        if (f->model == FM_SEM) {                      /* (driven softly) */
            int32_t a = in < 0 ? -in : in;
            if (a > 12000)
                in = in < 0 ? -(12000 + (softclip((a - 12000) * 2) >> 1)) : 12000 + (softclip((a - 12000) * 2) >> 1);
        }
        v3 = in - sv[1];
        v1 = (c->a1 * sv[0] + c->a2 * v3) >> 13;
        v2 = sv[1] + ((c->a2 * sv[0] + c->a3 * v3) >> 13);
        sv[0] = clamp(2 * v1 - sv[0], -150000, 150000);
        sv[1] = clamp(2 * v2 - sv[1], -150000, 150000);
        switch (f->model) {
        case FM_NOTCH: return in - ((f->k * v1) >> 12);
        case FM_BP: return (f->k * v1) >> 12;
        case FM_HP: return in - ((f->k * v1) >> 12) - v2;
        default: return v2;
        }
    }
    }
}
/* (from Clipper's firmware/src/eng_hybrid.c) */
/* a band-limited saw (osc_saw's polyBLEP): one test a sample for "near the wrap" (ph within inc of it, either
 * side: ph + inc wraps below 2 inc), the residual only there */
static inline int32_t hy_saw(uint32_t ph, uint32_t inc)
{
    int32_t s = (int32_t)(ph >> 16) - 32768;
    if (ph + inc < (inc << 1))
        s -= blep(ph, inc);
    return s;
}
#endif
