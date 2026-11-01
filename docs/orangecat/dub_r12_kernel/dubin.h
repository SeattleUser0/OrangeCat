/* round 12: the two inputs of the DUB renders: a skank (off-beat organ stabs, 75 BPM) and a throw (a snare-like hit and a bass note, the send opened for the hit). Mono float, 44.1 kHz. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define RFS 44100
#define BPM 75.0
static float rnd_f(uint32_t *s) { *s = *s * 1664525u + 1013904223u; return (float)(int32_t)*s / 2147483648.0f; }
static void gen_skank(float *x, uint32_t n)      /* a stab (C minor chord, organ-ish) on every off-beat 8th: beat 0.5, 1.5, ... */
{
    double beat = 60.0 / BPM; memset(x, 0, n * sizeof *x);
    for (int k = 0; k < 8; k++) {
        double t0 = (k + 0.5) * beat; uint32_t s0 = (uint32_t)(t0 * RFS);
        for (uint32_t i = 0; i < (uint32_t)(0.14 * RFS) && s0 + i < n; i++) {
            double t = i / (double)RFS, env = exp(-t * 18.0) * (t < 0.003 ? t / 0.003 : 1.0), v = 0;
            static const double f[3] = {261.63, 311.13, 392.0};
            for (int c = 0; c < 3; c++) v += sin(2 * M_PI * f[c] * t) + 0.4 * sin(2 * M_PI * f[c] * 2 * t) + 0.15 * sin(2 * M_PI * f[c] * 3 * t);
            x[s0 + i] += (float)(0.16 * v * env);
        }
    }
}
static void gen_throw(float *x, float *sendmask, uint32_t n)   /* a snare on beat 1 of bar 2 (and a bass note on beat 1 of bar 1); sendmask 1 for 220 ms around the snare */
{
    double beat = 60.0 / BPM; uint32_t s = 12345; memset(x, 0, n * sizeof *x); memset(sendmask, 0, n * sizeof *sendmask);
    for (int k = 0; k < 2; k++) {                  /* bass: bar 1 beat 1, bar 2 beat 3 */
        uint32_t s0 = (uint32_t)(((k ? 6 : 0)) * beat * RFS);
        for (uint32_t i = 0; i < (uint32_t)(0.5 * RFS) && s0 + i < n; i++) { double t = i / (double)RFS; x[s0 + i] += (float)(0.5 * sin(2 * M_PI * (55.0 + 10.0 * exp(-t * 20)) * t) * exp(-t * 5)); }
    }
    { uint32_t s0 = (uint32_t)(4 * beat * RFS);    /* snare at bar 2 beat 1 */
      for (uint32_t i = 0; i < (uint32_t)(0.25 * RFS) && s0 + i < n; i++) { double t = i / (double)RFS; x[s0 + i] += (float)(0.5 * (rnd_f(&s) * exp(-t * 22) + 0.6 * sin(2 * M_PI * 190 * t) * exp(-t * 30))); }
      for (uint32_t i = 0; i < (uint32_t)(0.22 * RFS); i++) if (s0 + i < n) sendmask[s0 + i] = 1.0f; }
}
static void wavw(const char *path, const float *l, const float *r, uint32_t n)
{
    FILE *f = fopen(path, "wb"); uint32_t dl = n * 4, v;
    uint16_t h;
    fwrite("RIFF", 1, 4, f); v = 36 + dl; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    h = 1; fwrite(&h, 2, 1, f); h = 2; fwrite(&h, 2, 1, f); v = RFS; fwrite(&v, 4, 1, f); v = RFS * 4; fwrite(&v, 4, 1, f); h = 4; fwrite(&h, 2, 1, f); h = 16; fwrite(&h, 2, 1, f);
    fwrite("data", 1, 4, f); fwrite(&dl, 4, 1, f);
    for (uint32_t i = 0; i < n; i++) { float a[2] = {l[i], r[i]}; for (int c = 0; c < 2; c++) { float q = a[c] * 0.8f; q = q > 1 ? 1 : q < -1 ? -1 : q; int16_t s = (int16_t)lrintf(q * 32767.0f); fwrite(&s, 2, 1, f); } }
    fclose(f);
}
