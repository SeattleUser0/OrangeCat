/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay: the few functions dd_init needs, freestanding (the FM-1 firmware has no libm). Init-time only; accuracy
 * ~1e-6 is ample for table design. Host and target use the same code, so renders match. */
#ifndef DD_MATH_H
#define DD_MATH_H
#define DDM_PI 3.14159265f
static inline float ddm_sin(float x)            /* any x: reduce to [-pi, pi], then odd polynomial (Taylor to x^11) */
{
    float k = x * (1.0f / (2.0f * DDM_PI)), x2;
    int n = (int)(k >= 0.0f ? k + 0.5f : k - 0.5f);
    x -= (float)n * (2.0f * DDM_PI);
    if (x > 0.5f * DDM_PI) x = DDM_PI - x;       /* fold to [-pi/2, pi/2] */
    else if (x < -0.5f * DDM_PI) x = -DDM_PI - x;
    x2 = x * x;
    return x * (1.0f - x2 / 6.0f * (1.0f - x2 / 20.0f * (1.0f - x2 / 42.0f * (1.0f - x2 / 72.0f * (1.0f - x2 / 110.0f)))));
}
static inline float ddm_cos(float x) { return ddm_sin(x + 0.5f * DDM_PI); }
static inline float ddm_tan(float x) { return ddm_sin(x) / ddm_cos(x); }
/* (0.4) e^x for the control tick only (the spring's gains): 2^(x / ln 2), the integer part by halving / doubling, the fraction by a
 * polynomial (degree 6, under 1e-6 relative over [0, 1)) */
#ifndef DD_RAM
#define DD_RAM                                          /* (measure/batch-s1-ram: the section, oc_dub_bench.c) */
#endif
DD_RAM static inline float ddm_exp(float x)
{
    float y = x * 1.44269504f, f, r = 1.0f;
    int n = (int)y;
    if (y < (float)n) n--;
    f = y - (float)n;
    r = 1.0f + f * (0.69314718f + f * (0.24022651f + f * (0.05550411f + f * (0.00961813f + f * (0.00133336f + f * 0.00015404f)))));
    for (; n > 0; n--) r *= 2.0f;
    for (; n < 0; n++) r *= 0.5f;
    return r;
}
DD_RAM static inline float ddm_sqrt(float x)
{
    float y = x > 1.0f ? x : 1.0f;
    int i;
    if (x <= 0.0f) return 0.0f;
    for (i = 0; i < 24; i++) y = 0.5f * (y + x / y);
    return y;
}
#endif
