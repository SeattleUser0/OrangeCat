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
static inline float ddm_sqrt(float x)
{
    float y = x > 1.0f ? x : 1.0f;
    int i;
    if (x <= 0.0f) return 0.0f;
    for (i = 0; i < 24; i++) y = 0.5f * (y + x / y);
    return y;
}
#endif
