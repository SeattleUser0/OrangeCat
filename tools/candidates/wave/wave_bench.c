/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * WAVE prototype: host timing of one voice (ns a voice-sample), by configuration. Best of R repetitions of 2 s of audio, timed by the thread's own CPU clock (the PC is shared:
 * the minimum is the figure that does not carry the other sessions' load). Usage: wave_bench [R] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "wave_proto.h"
#define NF 16u
static int16_t T[2][NF * WV_LEN];
static double now(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static void tables(void)
{   /* our own: table 0 a saw morphing to a square (band-limited additive, 127 harmonics), table 1 a sine growing odd harmonics */
    for (uint32_t f = 0; f < NF; f++)
        for (uint32_t i = 0; i < WV_LEN; i++) {
            double a = 0, b = 0, x = 2 * M_PI * i / WV_LEN, m = f / (double)(NF - 1);
            for (int h = 1; h < 128; h++) {
                a += sin(h * x) / h * ((h & 1) ? 1.0 : 1.0 - m);
                if (h & 1) b += sin(h * x) / (h * h) * (h == 1 ? 1.0 : m);
            }
            T[0][f * WV_LEN + i] = (int16_t)(a * 16000); T[1][f * WV_LEN + i] = (int16_t)(b * 26000);
        }
}
static double run(uint32_t uni, uint32_t warp, uint32_t mode, int lfo_pos, uint32_t R, double *sum)
{
    static wv_voice_t v; wv_patch_t p; static int32_t out[WV_CTL];
    const int16_t *tb[2] = {T[0], T[1]}; uint32_t nf[2] = {NF, NF};
    double best = 1e9; *sum = 0;
    for (uint32_t r = 0; r < R; r++) {
        memset(&v, 0, sizeof v); memset(&p, 0, sizeof p);
        p.uni[0] = p.uni[1] = (uint8_t)uni; p.warp[0] = p.warp[1] = (uint8_t)warp; p.mode = (uint8_t)mode; p.sub = 8000; p.noise = 2000;
        p.base[D_POS1] = 20000; p.base[D_POS2] = 40000; p.base[D_WARP1] = 9000; p.base[D_WARP2] = 4000; p.base[D_LVL1] = p.base[D_LVL2] = 12000;
        p.base[D_CUT] = 20000; p.base[D_RES] = 12000; p.base[D_AMP] = 30000;
        p.slot[0] = (wv_slot_t){1, D_CUT, 9000}; p.slot[1] = (wv_slot_t){2, lfo_pos ? D_POS1 : D_WARP2, 12000}; p.slot[2] = (wv_slot_t){3, lfo_pos ? D_POS2 : D_RES, 9000};
        p.slot[3] = (wv_slot_t){4, D_AMP, 2000}; p.slot[4] = (wv_slot_t){5, D_CUT, 3000}; p.slot[5] = (wv_slot_t){6, D_WARP1, 5000};
        p.slot[6] = (wv_slot_t){7, D_LVL2, 4000}; p.slot[7] = (wv_slot_t){0, D_LVL1, 2000};
        v.gate = 1; v.vel = 100; v.note = 48; v.level = 0; v.rng = 1;
        for (int e = 0; e < 2; e++) { v.env[e].a = 400000; v.env[e].s = 20000; v.env[e].r = 10; }
        v.lfo[0].inc = 20000; v.lfo[1].inc = 7000; v.lfo[1].shape = 1;
        for (uint32_t j = 0; j < 2; j++) for (uint32_t k = 0; k < uni; k++) v.osc[j].inc[k] = (uint32_t)(130.81 / 44100.0 * 4294967296.0 * (1.0 + 0.004 * k - 0.002 * uni) * (j ? 1.5 : 1.0));
        uint32_t blocks = 44100u * 2u / WV_CTL;
        double t0 = now();
        for (uint32_t b = 0; b < blocks; b++) { wv_block(&v, &p, tb, nf); wv_kernel(&v, &p, out, WV_CTL); }
        double ns = (now() - t0) * 1e9 / (blocks * WV_CTL);
        if (ns < best) best = ns;
        for (uint32_t i = 0; i < WV_CTL; i++) *sum += out[i];
    }
    return best;
}
int main(int argc, char **argv)
{
    uint32_t R = argc > 1 ? (uint32_t)atoi(argv[1]) : 15;
    static const char *W[] = {"BEND", "SYNC"}, *F[] = {"LP", "BP", "HP", "NOTCH", "LADDER"};
    double s;
    tables();
    printf("uni warp filter pos      ns/voice-sample\n");
    for (uint32_t u = 1; u <= 8; u *= 2) printf("%3u %-4s %-6s static   %6.1f\n", u, W[0], F[0], run(u, W_BEND, F_LP, 0, R, &s));
    for (uint32_t u = 1; u <= 8; u *= 2) printf("%3u %-4s %-6s lfo      %6.1f\n", u, W[0], F[0], run(u, W_BEND, F_LP, 1, R, &s));
    printf("%3u %-4s %-6s static   %6.1f\n", 1u, W[1], F[0], run(1, W_SYNC, F_LP, 0, R, &s));
    printf("%3u %-4s %-6s static   %6.1f\n", 4u, W[1], F[0], run(4, W_SYNC, F_LP, 0, R, &s));
    printf("%3u %-4s %-6s static   %6.1f\n", 1u, W[0], F[4], run(1, W_BEND, F_LADDER, 0, R, &s));
    printf("%3u %-4s %-6s static   %6.1f\n", 4u, W[0], F[4], run(4, W_BEND, F_LADDER, 0, R, &s));
    printf("sizeof(wv_voice_t) %zu B (of which the two frame caches %zu B and the halving scratch %zu B)\n", sizeof(wv_voice_t), 2 * sizeof(int16_t) * WV_LEN, 2 * sizeof(int16_t) * WV_LEN / 2);
    return 0;
}
