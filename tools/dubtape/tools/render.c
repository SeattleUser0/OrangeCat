/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay research: render the scenes through every variant to 16-bit stereo WAVs (TPDF dither), 128-frame blocks.
 * usage: render OUTDIR */
#include "../src/dubdelay.h"
#include "../src/dd_scenes.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct { const char *name; int algo, R, aa, lines; } variant_t;
static const variant_t V[] = {
    {"0-dry", -1, 1, 0, 1},
    {"1-digital", DD_ALGO_DIGITAL, 1, DD_AA_HB, 1},
    {"2-lite44k", DD_ALGO_LITE, 1, DD_AA_HB, 1},
    {"3-lite22k", DD_ALGO_LITE, 2, DD_AA_HB, 1},
    {"4-lite11k", DD_ALGO_LITE, 4, DD_AA_HB, 1},
    {"5-tape44k", DD_ALGO_TAPE, 1, DD_AA_HB, 1},
    {"6-tape22k", DD_ALGO_TAPE, 2, DD_AA_HB, 1},
    {"7-tape11k", DD_ALGO_TAPE, 4, DD_AA_HB, 1},
    {"8-tape22k-stereo", DD_ALGO_TAPE, 2, DD_AA_HB, 2},
    {"9-lite22k-cheapAA", DD_ALGO_LITE, 2, DD_AA_CHEAP, 1},
};
#define NV (int)(sizeof V / sizeof V[0])

static void wr32(FILE *f, uint32_t v) { fputc((int)(v & 255u), f); fputc((int)((v >> 8) & 255u), f); fputc((int)((v >> 16) & 255u), f); fputc((int)(v >> 24), f); }
static void wr16(FILE *f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }
static uint32_t drng = 777u;
static float dith(void) { drng = drng * 1664525u + 1013904223u; return (float)(drng >> 8) * (1.0f / 16777216.0f); }
static int write_wav(const char *path, const float *l, const float *r, int n)
{
    FILE *f = fopen(path, "wb");
    int i, c;
    if (!f) return -1;
    fwrite("RIFF", 1, 4, f); wr32(f, 36u + (uint32_t)n * 4u); fwrite("WAVEfmt ", 1, 8, f);
    wr32(f, 16); wr16(f, 1); wr16(f, 2); wr32(f, DD_FS); wr32(f, DD_FS * 4u); wr16(f, 4); wr16(f, 16);
    fwrite("data", 1, 4, f); wr32(f, (uint32_t)n * 4u);
    for (i = 0; i < n; i++)
        for (c = 0; c < 2; c++) {
            float v = (c ? r[i] : l[i]) * 32767.0f + dith() - dith();
            long q = lrintf(v);
            if (q > 32767) q = 32767;
            if (q < -32768) q = -32768;
            wr16(f, (uint16_t)(int16_t)q);
        }
    fclose(f);
    return 0;
}

static int16_t lines[2][1 << 17];

int main(int argc, char **argv)
{
    const char *out = argc > 1 ? argv[1] : "renders";
    int sc, v;
    char path[512];
    printf("scene,variant,line_len,line_bytes,max_head_s,peak_dbfs\n");
    for (sc = 0; sc < SC_N; sc++) {
        int n = (int)(SC_DUR[sc] * DD_FS) / DD_BLOCK * DD_BLOCK;
        float *x = malloc(sizeof(float) * (size_t)n), *l = malloc(sizeof(float) * (size_t)n), *r = malloc(sizeof(float) * (size_t)n);
        scene_source(sc, x, n);
        for (v = 0; v < NV; v++) {
            dd_t s;
            dd_params_t p;
            uint32_t len = dd_line_len(SC_MAXHEAD[sc] + 0.02f, V[v].R);
            float pk = 0.0f;
            int b, i;
            if (V[v].lines == 2 && sc != SC_THROW && sc != SC_LONG && sc != SC_SKANK) continue;
            if (V[v].aa == DD_AA_CHEAP && sc != SC_SKANK && sc != SC_THROW) continue;
            if (V[v].algo >= 0) dd_init(&s, V[v].algo, V[v].R, V[v].aa, V[v].lines, lines[0], lines[1], len);
            for (b = 0; b < n; b += DD_BLOCK) {
                scene_params(sc, (float)b / DD_FS, &p);
                if (V[v].algo == DD_ALGO_DIGITAL) { dd_to_digital(&p); p.lp = 127.0f; p.hp = 0.0f; }
                if (V[v].algo < 0) { for (i = 0; i < DD_BLOCK; i++) l[b + i] = r[b + i] = x[b + i]; continue; }
                dd_process(&s, &p, x + b, l + b, r + b);
            }
            for (i = 0; i < n; i++) { float a = fabsf(l[i]) > fabsf(r[i]) ? fabsf(l[i]) : fabsf(r[i]); if (a > pk) pk = a; }
            snprintf(path, sizeof path, "%s/%s_%s.wav", out, SC_NAME[sc], V[v].name);
            if (write_wav(path, l, r, n)) { perror(path); return 1; }
            printf("%s,%s,%u,%u,%.3f,%.2f\n", SC_NAME[sc], V[v].name, V[v].algo < 0 ? 0u : len,
                   V[v].algo < 0 ? 0u : len * 2u * (unsigned)V[v].lines, V[v].algo < 0 ? 0.0 : (double)dd_max_seconds(&s),
                   (double)(20.0f * log10f(pk + 1e-9f)));
        }
        free(x); free(l); free(r);
    }
    /* (0.4) the spring (a Fisher K-10 model), at the 22.05 kHz line unless said: on the wet, in the loop, the tank alone (the taps cut: "the
     * delay's mix at zero"), the kick. Its rows follow the ones above, which do not change */
    {
        static const struct { int sc, v; const char *tag; int place; float mix, decay, drive, tone, in, cut, k1, k2; } SPR[] = {
            {SC_SKANK, 3, "s1-lite22k-spring-wet", 2, 0.6f, 2.0f, 0.35f, 0.3f, 0.3f, 0.0f, -1.0f, -1.0f},
            {SC_SKANK, 3, "s2-lite22k-spring-loop", 1, 0.7f, 2.0f, 0.35f, 0.3f, 0.0f, 0.0f, -1.0f, -1.0f},
            {SC_SLAP, 3, "s3-lite22k-spring-alone", 2, 1.0f, 1.6f, 0.4f, 0.2f, 1.0f, 1.0f, -1.0f, -1.0f},
            {SC_THROW, 3, "s4-lite22k-spring-loop", 1, 0.8f, 2.5f, 0.5f, 0.3f, 0.0f, 0.0f, -1.0f, -1.0f},
            {SC_OSC, 3, "s5-lite22k-spring-loop", 1, 1.0f, 3.0f, 0.6f, 0.4f, 0.0f, 0.0f, -1.0f, -1.0f},
            {SC_SLAP, 3, "s6-lite22k-spring-crash-wet", 2, 0.9f, 2.0f, 0.7f, 0.3f, 0.6f, 0.0f, 1.2f, 5.2f},
            {SC_SLAP, 3, "s7-lite22k-spring-crash-loop", 1, 0.8f, 2.0f, 0.7f, 0.3f, 0.0f, 0.0f, 1.2f, 5.2f},
            {SC_SKANK, 6, "s8-tape22k-spring-wet", 2, 0.6f, 2.0f, 0.35f, 0.3f, 0.3f, 0.0f, -1.0f, -1.0f},
        };
        static int16_t spbuf[DD_SP_BUF];
        int k;
        for (k = 0; k < (int)(sizeof SPR / sizeof SPR[0]); k++) {
            int sc2 = SPR[k].sc, v2 = SPR[k].v, n = (int)(SC_DUR[sc2] * DD_FS) / DD_BLOCK * DD_BLOCK, b, i;
            float *x = malloc(sizeof(float) * (size_t)n), *l = malloc(sizeof(float) * (size_t)n), *r = malloc(sizeof(float) * (size_t)n), pk = 0.0f;
            uint32_t len = dd_line_len(SC_MAXHEAD[sc2] + 0.02f, V[v2].R), kicks = 0;
            dd_t s;
            dd_params_t p;
            scene_source(sc2, x, n);
            dd_init(&s, V[v2].algo, V[v2].R, V[v2].aa, V[v2].lines, lines[0], lines[1], len);
            dd_spring_attach(&s, spbuf);
            for (b = 0; b < n; b += DD_BLOCK) {
                float t = (float)b / DD_FS;
                scene_params(sc2, t, &p);
                p.spring = SPR[k].place; p.sp_mix = SPR[k].mix; p.sp_decay = SPR[k].decay; p.sp_drive = SPR[k].drive;
                p.sp_tone = SPR[k].tone; p.sp_in = SPR[k].in; p.tap_cut = SPR[k].cut;
                kicks += (SPR[k].k1 >= 0.0f && t >= SPR[k].k1 && t < SPR[k].k1 + (float)DD_BLOCK / DD_FS) ? 1u : 0u;
                kicks += (SPR[k].k2 >= 0.0f && t >= SPR[k].k2 && t < SPR[k].k2 + (float)DD_BLOCK / DD_FS) ? 1u : 0u;
                p.crash = kicks;
                dd_process(&s, &p, x + b, l + b, r + b);
            }
            for (i = 0; i < n; i++) { float a = fabsf(l[i]) > fabsf(r[i]) ? fabsf(l[i]) : fabsf(r[i]); if (a > pk) pk = a; }
            snprintf(path, sizeof path, "%s/%s_%s.wav", out, SC_NAME[sc2], SPR[k].tag);
            if (write_wav(path, l, r, n)) { perror(path); return 1; }
            printf("%s,%s,%u,%u,%.3f,%.2f\n", SC_NAME[sc2], SPR[k].tag, len, len * 2u * (unsigned)V[v2].lines + (unsigned)(DD_SP_BUF * 2u),
                   (double)dd_max_seconds(&s), (double)(20.0f * log10f(pk + 1e-9f)));
            free(x); free(l); free(r);
        }
    }
    return 0;
}
