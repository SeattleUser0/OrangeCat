/* SPDX-License-Identifier: GPL-3.0-only
 * The skank and throw scenes of the dub research prototype (sources and the filter and send moves), played through dubdelay.c at the quarter rate (R = 4, 32-sample blocks).
 * Shared by tests/jfx_dub_test.c (converter B: the hash of the render) and ~/dubtape-research/conv/conv_render.c (the WAVs the owner listened to). Needs <math.h>, <string.h>, dubdelay.h. */
#include <stdlib.h>
#define BPM 75.0f
#define BEAT (60.0f / BPM)
static uint32_t sc_rng = 12345u;
static float sc_noise(void) { sc_rng = sc_rng * 1664525u + 1013904223u; return (float)(int32_t)sc_rng * (1.0f / 2147483648.0f); }
static float saw_bl(float f, float t) { float s = 0.0f; int k; for (k = 1; (float)k * f < 12000.0f; k++) s += sinf(2.0f * 3.14159265f * f * (float)k * t) / (float)k; return s * 0.55f; }
static void add_stab(float *x, int n, float t0, float gain)
{
    static const float nf[4] = {261.63f, 311.13f, 392.00f, 466.16f};
    int i0 = (int)(t0 * 44100.0f), i, k;
    for (i = 0; i < (int)(0.2f * 44100.0f) && i0 + i < n; i++) {
        float t = (float)i / 44100.0f, e = (t < 0.002f ? t / 0.002f : expf(-(t - 0.002f) / 0.07f)), s = 0.0f;
        if (t > 0.16f) e *= expf(-(t - 0.16f) / 0.008f);
        for (k = 0; k < 4; k++) s += saw_bl(nf[k], t);
        x[i0 + i] += gain * 0.25f * e * s;
    }
}
static void add_vowel(float *x, int n, float t0, float dur, float gain)
{
    static const float ff[3] = {700.0f, 1220.0f, 2600.0f}, fa[3] = {1.0f, 0.5f, 0.25f};
    float st1[3] = {0}, st2[3] = {0}, ph = 0.0f;
    int i0 = (int)(t0 * 44100.0f), i, k;
    for (i = 0; i < (int)((dur + 0.05f) * 44100.0f) && i0 + i < n; i++) {
        float t = (float)i / 44100.0f, e = t < 0.01f ? t / 0.01f : t < dur ? 1.0f : expf(-(t - dur) / 0.01f), src, y = 0.0f;
        ph += 140.0f / 44100.0f; if (ph >= 1.0f) ph -= 1.0f;
        src = (ph < 0.08f ? 1.0f : -0.087f) + 0.02f * sc_noise();
        for (k = 0; k < 3; k++) {
            float w = 2.0f * 3.14159265f * ff[k] / 44100.0f, r = 0.985f, a1 = 2.0f * r * cosf(w), a2 = -r * r, v = src + a1 * st1[k] + a2 * st2[k];
            st2[k] = st1[k]; st1[k] = v; y += fa[k] * (v - st1[k] * 0.0f) * (1.0f - r);
        }
        x[i0 + i] += gain * e * y * 1.2f;
    }
}
static void scene_params(int sc, float t, dd_params_t *p)
{
    int i;
    memset(p, 0, sizeof *p);
    p->dry = 1.0f; p->wet = 0.8f; p->send = 1.0f; p->glide = 0.12f; p->drive = 0.35f; p->res = 0.2f; p->route = 1; p->cut_wet = 1; p->lim_ceil = 0.891f;
    if (sc == 0) {          /* skank: the SKANK heads (1 and 1.5 steps, panned) at a 1/8 step, the feedback ridden up in bar 2, the tone closing over the tail */
        p->time = BEAT / 2.0f; p->head[0] = 1.0f; p->head[1] = 1.5f; p->level[0] = 0.8f; p->level[1] = 0.5f; p->pan[0] = -0.3f; p->pan[1] = 0.3f; p->fbw[0] = 0.6f; p->fbw[1] = 0.4f;
        p->feedback = t < BEAT * 4.0f ? 0.5f : 0.78f;
        p->hp = 42.0f; p->lp = t < 6.4f ? 92.0f : 92.0f - 22.0f * fminf(1.0f, (t - 6.4f) / 4.0f); p->res = 0.3f;
    } else {                /* throw: the send open 0.25 .. 0.6 s only, the loop then on its own, its filter swept; the TRIPLE heads at 0.2 s */
        p->time = 0.2f; p->head[0] = 1.0f; p->head[1] = 2.0f; p->head[2] = 3.0f; p->level[0] = 0.7f; p->level[1] = 0.55f; p->level[2] = 0.4f; p->pan[0] = 0.3f; p->pan[1] = -0.3f;
        p->fbw[0] = 0.3f; p->fbw[1] = 0.3f; p->fbw[2] = 0.4f; p->feedback = 0.88f;
        p->send = (t > 0.25f && t < 0.6f) ? 1.0f : 0.0f; p->hp = 38.0f; p->res = 0.65f; p->drive = 0.5f;
        {   float u = t < 1.0f ? 0.0f : (t - 1.0f) / 10.0f;
            p->lp = u < 0.33f ? 95.0f - 37.0f * u / 0.33f : u < 0.66f ? 58.0f + 50.0f * (u - 0.33f) / 0.33f : 108.0f - 28.0f * fminf(1.0f, (u - 0.66f) / 0.34f); }
    }
    (void)i;
}

static uint64_t sc_fnv(uint64_t h, const float *x, int n) { for (int i = 0; i < n; i++) { uint32_t u; memcpy(&u, &x[i], 4); for (int b = 0; b < 4; b++) { h ^= (uint64_t)((u >> (8 * b)) & 255u); h *= 1099511628211ull; } } return h; }
/* scene sc (0 skank, 1 throw) through a fresh instance at R = 4: the stereo output in ol / or_ (n = 44100 x seconds samples), dry gain 1 */
static void sc_render(int sc, float *ol, float *or_, int n)
{
    float *x = (float *)calloc((size_t)n, sizeof(float));
    static int16_t blk[16][2048]; int16_t *bp[16]; static dd_t d; static int16_t sb[DD_SP_BUF]; int b;
    sc_rng = 12345u;
    if (sc == 0) for (b = 0; b < 8; b += 2) add_stab(x, n, BEAT * (float)(b + 1), 0.7f); else add_vowel(x, n, 0.3f, 0.25f, 0.9f);
    memset(blk, 0, sizeof blk); memset(&d, 0, sizeof d); for (b = 0; b < 16; b++) bp[b] = blk[b];
    dk_init(&d, 4, bp, 16); dk_spring_attach(&d, sb);
    for (int i = 0; i + DD_BLOCK <= n; i += DD_BLOCK) {
        dd_params_t p; scene_params(sc, (float)i / 44100.0f, &p);
        dk_process(&d, &p, x + i, ol + i, or_ + i);
    }
    free(x);
}
