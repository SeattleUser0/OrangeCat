/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay research: host CPU proxy. Times dd_process per 128-frame block for 1 and 4 instances. Host x86 timing is
 * a proxy for relative cost only, not FM-1 timing. */
#define _POSIX_C_SOURCE 199309L
#include "../src/dubdelay.h"
#include "../src/dd_scenes.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct { const char *name; int algo, R, aa, lines, pat, spring, fx; } bcfg_t;   /* spring: (0.4) 1 in the loop, 2 on the wet, 3 the tank alone;
 * fx: (0.5) 1 the limiter at -9 dBFS, 2 the input cut held, 3 the cut (input and wet) toggled every 64 blocks (the ramps running), 4 1 and 3 */
static const bcfg_t B[] = {
    {"DIGITAL 44k, 1 head", DD_ALGO_DIGITAL, 1, DD_AA_HB, 1, 0, 0, 0},
    {"LITE 44k, 2 heads (SKANK)", DD_ALGO_LITE, 1, DD_AA_HB, 1, 1, 0, 0},
    {"LITE 44k, 4 heads (SPREAD)", DD_ALGO_LITE, 1, DD_AA_HB, 1, 5, 0, 0},
    {"LITE 22k HB, 2 heads", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 0, 0},
    {"LITE 22k HB, 4 heads", DD_ALGO_LITE, 2, DD_AA_HB, 1, 5, 0, 0},
    {"LITE 22k box/lin, 2 heads", DD_ALGO_LITE, 2, DD_AA_CHEAP, 1, 1, 0, 0},
    {"LITE 11k HB, 2 heads", DD_ALGO_LITE, 4, DD_AA_HB, 1, 1, 0, 0},
    {"TAPE 44k, 3 heads (TRIPLE)", DD_ALGO_TAPE, 1, DD_AA_HB, 1, 3, 0, 0},
    {"TAPE 22k HB, 3 heads", DD_ALGO_TAPE, 2, DD_AA_HB, 1, 3, 0, 0},
    {"TAPE 11k HB, 3 heads", DD_ALGO_TAPE, 4, DD_AA_HB, 1, 3, 0, 0},
    {"TAPE 22k HB stereo (2 lines), 3 heads", DD_ALGO_TAPE, 2, DD_AA_HB, 2, 3, 0, 0},
    {"(0.4) the spring alone, 22.05 kHz line", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 3, 0},
    {"(0.4) LITE 22k HB, 2 heads + the spring in the loop", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 1, 0},
    {"(0.4) LITE 22k HB, 2 heads + the spring on the wet", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 2, 0},
    {"(0.4) TAPE 22k HB, 3 heads + the spring in the loop", DD_ALGO_TAPE, 2, DD_AA_HB, 1, 3, 1, 0},
    {"(0.4) TAPE 22k HB, 3 heads + the spring on the wet", DD_ALGO_TAPE, 2, DD_AA_HB, 1, 3, 2, 0},
    {"(0.5) LITE 22k HB, 2 heads + the limiter", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 0, 1},
    {"(0.5) LITE 22k HB, 2 heads, the input cut held", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 0, 2},
    {"(0.5) LITE 22k HB, 2 heads, the cut toggled (ramps)", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 0, 3},
    {"(0.5) LITE 22k HB, 2 heads + the limiter + the cut toggled", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 0, 4},
    {"(0.5) TAPE 22k HB, 3 heads + the limiter + the cut toggled", DD_ALGO_TAPE, 2, DD_AA_HB, 1, 3, 0, 4},
    {"(0.5) LITE 22k HB, 2 heads + the spring in the loop + the limiter + the cut toggled", DD_ALGO_LITE, 2, DD_AA_HB, 1, 1, 1, 4},
};
#define NB (int)(sizeof B / sizeof B[0])
#define NBLK 10336                           /* 30 s */
static int16_t mem[8][1 << 15];
static int16_t spmem[4][DD_SP_BUF];
static double now(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e9 + t.tv_nsec; }
static int cmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }

int main(void)
{
    static float x[NBLK * DD_BLOCK], ol[DD_BLOCK], orr[DD_BLOCK];
    static double tb[NBLK];
    int c, k, b, inst;
    printf("sizeof(dd_t) = %zu bytes (state, all options; a slimmer LITE-only struct is smaller: see docs)\n\n", sizeof(dd_t));
    printf("| config | instances | median us/block | p99 us/block | mean us/block | %% of 2901 us (median) | ns/sample/inst (median) |\n|---|---|---|---|---|---|---|\n");
    sc_rng = 3u;
    for (k = 0; k < NBLK * DD_BLOCK; k++) x[k] = 0.3f * sc_noise();
    for (c = 0; c < NB; c++)
        for (inst = 1; inst <= 4; inst += 3) {
            dd_t *s = calloc((size_t)inst, sizeof(dd_t));
            double sum = 0;
            for (k = 0; k < inst; k++) {
                dd_init(&s[k], B[c].algo, B[c].R, B[c].aa, B[c].lines, mem[2 * k], mem[2 * k + 1], 1u << 15);
                if (B[c].spring) dd_spring_attach(&s[k], spmem[k]);
            }
            for (b = 0; b < NBLK; b++) {
                dd_params_t p;
                double t0;
                scene_params(SC_THROW, (float)b * DD_BLOCK / DD_FS, &p);
                dd_apply_pattern(&p, B[c].pat);
                p.time = 0.1f; p.send = 1.0f; p.wow = 0.4f; p.flutter = 0.4f; p.xfeed = 0.4f;
                if (b % 512 == 0) p.time = 0.11f;                 /* heads gliding now and then */
                if (B[c].spring) { p.spring = B[c].spring == 3 ? 2 : B[c].spring; p.sp_mix = 0.7f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f; p.sp_in = 0.3f; }
                if (B[c].fx == 1 || B[c].fx == 4) p.lim_ceil = 0.355f;
                if (B[c].fx == 2) p.cut = 1;
                if (B[c].fx >= 3) { p.cut = (b >> 6) & 1; p.cut_wet = 1; }
                t0 = now();
                if (B[c].spring == 3)                           /* the tank alone: a block's worth at the line's rate (64 samples) */
                    for (k = 0; k < inst; k++) { dd_spring_block(&s[k], &p, x + b * DD_BLOCK, ol, DD_CTL); dd_spring_block(&s[k], &p, x + b * DD_BLOCK + DD_CTL, ol + DD_CTL, DD_CTL); }
                else
                    for (k = 0; k < inst; k++) dd_process(&s[k], &p, x + b * DD_BLOCK, ol, orr);
                tb[b] = now() - t0;
                sum += tb[b];
            }
            qsort(tb, NBLK, sizeof(double), cmp);
            {   double med = tb[NBLK / 2];
                printf("| %s | %d | %.2f | %.2f | %.2f | %.2f | %.1f |\n", B[c].name, inst, med / 1e3, tb[NBLK * 99 / 100] / 1e3,
                       sum / NBLK / 1e3, 100.0 * med / 1e3 / 2901.33, med / DD_BLOCK / inst); }
            free(s);
        }
    return 0;
}
