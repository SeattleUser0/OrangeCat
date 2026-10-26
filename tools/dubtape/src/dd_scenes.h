/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay research: deterministic test sources, head patterns, presets and the four scenes' automation.
 * Host-side only (uses libm freely: none of this runs in the audio loop). */
#ifndef DD_SCENES_H
#define DD_SCENES_H
#include "dubdelay.h"
#include <math.h>
#include <string.h>

#define BPM 75.0f
#define BEAT (60.0f / BPM)                         /* 0.8 s */
#define S16 (BEAT / 4.0f)                          /* a sixteenth: 0.2 s */

static uint32_t sc_rng = 12345u;
static float sc_noise(void) { sc_rng = sc_rng * 1664525u + 1013904223u; return (float)(int32_t)sc_rng * (1.0f / 2147483648.0f); }

/* band-limited saw (additive to 12 kHz) */
static float saw_bl(float f, float t)
{
    float s = 0.0f;
    int k;
    for (k = 1; (float)k * f < 12000.0f; k++) s += sinf(2.0f * 3.14159265f * f * (float)k * t) / (float)k;
    return s * 0.55f;
}
/* Cm7 stab at t0: C4 Eb4 G4 Bb4, 2 ms attack, 70 ms decay, gated at 160 ms */
static void add_stab(float *x, int n, float t0, float gain)
{
    static const float nf[4] = {261.63f, 311.13f, 392.00f, 466.16f};
    int i0 = (int)(t0 * DD_FS), i, k;
    for (i = 0; i < (int)(0.2f * DD_FS) && i0 + i < n; i++) {
        float t = (float)i / DD_FS, e = (t < 0.002f ? t / 0.002f : expf(-(t - 0.002f) / 0.07f)), s = 0.0f;
        if (t > 0.16f) e *= expf(-(t - 0.16f) / 0.008f);
        for (k = 0; k < 4; k++) s += saw_bl(nf[k], t);
        x[i0 + i] += gain * 0.25f * e * s;
    }
}
/* snare: pitched body (185 -> 160 Hz, 330 Hz), band-passed noise ~4 kHz, a click */
static void add_snare(float *x, int n, float t0, float gain)
{
    int i0 = (int)(t0 * DD_FS), i;
    float b1 = 0.0f, b2 = 0.0f, ph = 0.0f, ph2 = 0.0f;
    const float f = 4000.0f, q = 1.2f, g = tanf(3.14159265f * f / DD_FS), k = 1.0f / q;
    for (i = 0; i < (int)(0.4f * DD_FS) && i0 + i < n; i++) {
        float t = (float)i / DD_FS, nz = sc_noise(), v1, v2, v3;
        ph += 2.0f * 3.14159265f * (160.0f + 25.0f * expf(-t / 0.01f)) / DD_FS;
        ph2 += 2.0f * 3.14159265f * 330.0f / DD_FS;
        v3 = nz - b2; v1 = (b1 + g * v3) / (1.0f + g * (g + k)); v2 = b2 + g * v1;   /* SVF BP */
        b1 = 2.0f * v1 - b1; b2 = 2.0f * v2 - b2;
        x[i0 + i] += gain * (0.55f * expf(-t / 0.05f) * (sinf(ph) + 0.4f * sinf(ph2))
                             + 1.4f * expf(-t / 0.09f) * v1 + (i < 40 ? 0.6f * nz : 0.0f));
    }
}
/* a vowel "ah" (140 Hz pulse through three formants 700 / 1220 / 2600 Hz), dur seconds */
static void add_vowel(float *x, int n, float t0, float dur, float gain)
{
    static const float ff[3] = {700.0f, 1220.0f, 2600.0f}, fa[3] = {1.0f, 0.5f, 0.25f};
    float st1[3] = {0}, st2[3] = {0}, ph = 0.0f;
    int i0 = (int)(t0 * DD_FS), i, k;
    for (i = 0; i < (int)((dur + 0.05f) * DD_FS) && i0 + i < n; i++) {
        float t = (float)i / DD_FS, e = t < 0.01f ? t / 0.01f : t < dur ? 1.0f : expf(-(t - dur) / 0.01f), src, y = 0.0f;
        ph += (140.0f * (1.0f + 0.03f * sinf(2.0f * 3.14159265f * 5.0f * t))) / DD_FS;
        if (ph >= 1.0f) ph -= 1.0f;
        src = ph < 0.08f ? 1.0f : -0.087f;                                         /* a narrow pulse */
        for (k = 0; k < 3; k++) {
            float g = tanf(3.14159265f * ff[k] / DD_FS), kk = 1.0f / 8.0f, v3 = src - st2[k];
            float v1 = (st1[k] + g * v3) / (1.0f + g * (g + kk)), v2 = st2[k] + g * v1;
            st1[k] = 2.0f * v1 - st1[k]; st2[k] = 2.0f * v2 - st2[k];
            y += fa[k] * v1;
        }
        x[i0 + i] += gain * e * y * 0.35f;
    }
}
static void add_click(float *x, int n, float t0, float gain)       /* 5 ms noise burst */
{
    int i0 = (int)(t0 * DD_FS), i;
    for (i = 0; i < 220 && i0 + i < n; i++) x[i0 + i] += gain * sc_noise() * (1.0f - (float)i / 220.0f);
}

/* ------------------------------------------------------------------ patterns (heads in steps of TIME) --- */
typedef struct { const char *name; float head[DD_TAPS], level[DD_TAPS], pan[DD_TAPS], fbw[DD_TAPS]; } dd_pattern_t;
static const dd_pattern_t DD_PATTERNS[] = {
    {"SINGLE", {1, 0, 0, 0}, {0.8f, 0, 0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}},
    {"SKANK",  {2, 3, 0, 0}, {0.55f, 0.8f, 0, 0}, {-0.6f, 0.6f, 0, 0}, {0, 1, 0, 0}},      /* 1/8 + dotted 1/8 (step 1/16) */
    {"SLAP",   {1, 3, 0, 0}, {0.75f, 0.5f, 0, 0}, {0.3f, -0.4f, 0, 0}, {0.2f, 0.8f, 0, 0}},
    {"TRIPLE", {1, 2, 3, 0}, {0.5f, 0.6f, 0.7f, 0}, {-0.7f, 0.7f, 0, 0}, {0.15f, 0.25f, 0.6f, 0}},  /* evenly spaced heads */
    {"GALLOP", {1, 3, 4, 0}, {0.45f, 0.6f, 0.75f, 0}, {0.6f, -0.6f, 0, 0}, {0, 0.3f, 0.7f, 0}},
    {"SPREAD", {2, 3, 4, 6}, {0.4f, 0.5f, 0.6f, 0.7f}, {-0.8f, 0.8f, -0.3f, 0.3f}, {0, 0, 0.4f, 0.6f}},
};
static void dd_apply_pattern(dd_params_t *p, int pat)
{
    const dd_pattern_t *q = &DD_PATTERNS[pat];
    memcpy(p->head, q->head, sizeof p->head); memcpy(p->level, q->level, sizeof p->level);
    memcpy(p->pan, q->pan, sizeof p->pan); memcpy(p->fbw, q->fbw, sizeof p->fbw);
}
/* the conventional comparison: one head at the pattern's feedback head, flat feedback */
static void dd_to_digital(dd_params_t *p)
{
    int i, best = 0;
    for (i = 1; i < DD_TAPS; i++) if (p->fbw[i] > p->fbw[best]) best = i;
    p->head[0] = p->head[best]; p->level[0] = 0.8f; p->pan[0] = 0.0f; p->fbw[0] = 1.0f;
    for (i = 1; i < DD_TAPS; i++) p->head[i] = 0.0f;
}

/* ------------------------------------------------------------------ the scenes --- */
enum { SC_SKANK, SC_SLAP, SC_THROW, SC_OSC, SC_LONG, SC_N };
static const char *const SC_NAME[SC_N] = {"skank", "slap", "throw", "oscillate", "atmos"};
static const float SC_DUR[SC_N] = {11.0f, 8.0f, 12.0f, 18.0f, 12.0f};
static const float SC_MAXHEAD[SC_N] = {0.6f, 0.6f, 0.6f, 0.36f, 1.2f};      /* seconds: the longest head */

static void scene_source(int sc, float *x, int n)
{
    int b;
    memset(x, 0, sizeof(float) * (size_t)n);
    sc_rng = 12345u;
    switch (sc) {
    case SC_SKANK:  for (b = 0; b < 8; b += 2) add_stab(x, n, BEAT * (float)(b + 1), 0.7f);  /* beats 2 and 4, two bars */
                    break;
    case SC_SLAP:   add_snare(x, n, BEAT * 2.0f, 0.5f); add_snare(x, n, BEAT * 6.0f, 0.5f); break;   /* one-drop: beat 3 */
    case SC_THROW:  add_vowel(x, n, 0.3f, 0.25f, 0.9f); break;
    case SC_OSC:    add_click(x, n, 0.2f, 0.7f); break;
    case SC_LONG:   add_stab(x, n, 0.3f, 0.6f); add_vowel(x, n, 2.5f, 0.4f, 0.7f); break;
    }
}
/* parameters at time t (seconds), for the scene */
static void scene_params(int sc, float t, dd_params_t *p)
{
    memset(p, 0, sizeof *p);
    p->dry = 1.0f; p->wet = 0.8f; p->send = 1.0f; p->glide = 0.12f;
    p->wow = 0.25f; p->flutter = 0.2f; p->drive = 0.35f; p->res = 0.2f; p->xfeed = 0.0f; p->route = 1;
    switch (sc) {
    case SC_SKANK:    /* the dotted echo, the feedback ridden up in bar 2, the tone closing over the tail */
        dd_apply_pattern(p, 1); p->time = S16;
        p->feedback = t < BEAT * 4.0f ? 0.5f : 0.78f;
        p->hp = 42.0f; p->lp = t < 6.4f ? 92.0f : 92.0f - 22.0f * fminf(1.0f, (t - 6.4f) / 4.0f);
        p->res = 0.3f;
        break;
    case SC_SLAP:     /* first hit thin and bright, the second dark and full */
        dd_apply_pattern(p, 2); p->time = S16; p->feedback = 0.35f;
        if (t < BEAT * 5.0f) { p->hp = 62.0f; p->lp = 104.0f; p->res = 0.55f; }
        else { p->hp = 28.0f; p->lp = 80.0f; p->res = 0.25f; }
        p->wet = 0.9f;
        break;
    case SC_THROW:    /* the send open 0.25 .. 0.6 s only; the loop then on its own, its filter swept */
        dd_apply_pattern(p, 3); p->time = 0.2f; p->feedback = 0.88f;
        p->send = (t > 0.25f && t < 0.6f) ? 1.0f : 0.0f;
        p->hp = 38.0f; p->res = 0.65f; p->drive = 0.5f;
        {   float u = t < 1.0f ? 0.0f : (t - 1.0f) / 10.0f;   /* 95 -> 58 -> 108 -> 80 */
            p->lp = u < 0.33f ? 95.0f - 37.0f * u / 0.33f : u < 0.66f ? 58.0f + 50.0f * (u - 0.33f) / 0.33f
                                                                       : 108.0f - 28.0f * fminf(1.0f, (u - 0.66f) / 0.34f); }
        break;
    case SC_OSC:      /* feedback walked past unity, held, then pulled back */
        dd_apply_pattern(p, 3); p->time = 0.12f;
        p->feedback = t < 1.0f ? 0.95f : t < 3.0f ? 0.95f + 0.25f * (t - 1.0f) / 2.0f : t < 13.0f ? 1.2f : 0.5f;
        p->hp = 40.0f; p->res = 0.5f; p->drive = 0.6f; p->route = 0;
        p->lp = 88.0f + 12.0f * sinf(2.0f * 3.14159265f * 0.15f * t);
        p->wet = 0.9f;
        break;
    case SC_LONG:     /* long atmospheric: slow, dark, wide, more wow */
        dd_apply_pattern(p, 3); p->time = 0.4f; p->feedback = 0.72f;
        p->hp = 35.0f; p->lp = 82.0f; p->res = 0.15f; p->wow = 0.6f; p->flutter = 0.3f; p->drive = 0.25f;
        p->xfeed = 0.5f; p->glide = 0.3f;
        break;
    }
}
#endif
