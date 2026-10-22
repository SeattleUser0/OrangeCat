/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.0: Plaits parity. Each ported model (firmware/src/plaits/) is run beside the original C++ engine
 * with the same parameters, block by block, and their outputs (OUT and AUX) compared sample for sample.
 *
 *   tests/plaits_parity/run.sh <eurorack checkout with stmlib>      (prints a line per model and scenario)
 *   ./parity [model] [-v]                                          (one model; -v: the first differing samples)
 *
 * Scenarios (each from a fresh engine and random seed): free running (trigger unpatched) at a fixed setting; a
 * sweep of HARMONICS, TIMBRE and MORPH with a note glide and triggers every 150 ms; 24 random settings, each
 * triggered; very low and very high notes. 12-frame blocks, as Plaits renders.
 * A model passes when the largest difference is below 1e-4 of its peak (the port does the C++'s arithmetic in the
 * same order, so it is usually exact; a little drift is allowed for feedback models).
 */
#include "models.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *NAMES[M_COUNT] = {
    "VA", "WAVESHAPING", "FM", "GRAIN", "ADDITIVE", "WAVETABLE", "CHORD", "SPEECH",
    "SWARM", "NOISE", "PARTICLE", "STRING", "MODAL", "BASS DRUM", "SNARE", "HI-HAT"
};
#define BLOCK 12
static int verbose;

static uint32_t lcg = 12345u;
static float frand(void) { lcg = lcg * 1103515245u + 12345u; return (float)((lcg >> 8) & 0xFFFFu) / 65535.0f; }

typedef struct { double max_diff, peak; int env_mismatch; long first_bad; } result_t;

static void step(void *r, void *p, const parity_params_t *q, result_t *res, long *t)
{
    float ro[BLOCK], ra[BLOCK], po[BLOCK], pa[BLOCK];
    int re = 0, pe = 0;
    ref_render(r, q, ro, ra, BLOCK, &re);
    port_render(p, q, po, pa, BLOCK, &pe);
    if (re != pe) res->env_mismatch++;
    for (int i = 0; i < BLOCK; i++) {
        double d = fabs((double)ro[i] - po[i]), d2 = fabs((double)ra[i] - pa[i]);
        if (d2 > d) d = d2;
        if (!(d <= 1e30)) d = 1e30;                              /* (NaN) */
        if (fabs(ro[i]) > res->peak) res->peak = fabs(ro[i]);
        if (fabs(ra[i]) > res->peak) res->peak = fabs(ra[i]);
        if (d > res->max_diff) res->max_diff = d;
        if (d > 1e-6 && res->first_bad < 0) {
            res->first_bad = *t + i;
            if (verbose)
                printf("    first difference at sample %ld: out %.9g / %.9g, aux %.9g / %.9g\n", *t + i, ro[i], po[i], ra[i], pa[i]);
        }
    }
    *t += BLOCK;
}

static int scenario(int m, int which, const char *label)
{
    void *r, *p;
    result_t res = { 0, 0, 0, -1 };
    long t = 0;
    parity_params_t q = { 2, 48.0f, 0.5f, 0.5f, 0.5f, 0.8f };
    ref_seed(0x21u);
    port_seed(0x21u);
    r = ref_new(m);
    p = port_new(m);
    if (!r || !p) return -1;
    lcg = 777u + (uint32_t)which;
    if (which == 0) {                                           /* free running */
        for (int b = 0; b < 1500; b++) step(r, p, &q, &res, &t);
    } else if (which == 1) {                                    /* sweep, glide, triggers */
        for (int b = 0; b < 4000; b++) {
            float x = (float)b / 4000.0f;
            q.harmonics = x;
            q.timbre = 0.5f + 0.5f * sinf(x * 9.0f);
            q.morph = 1.0f - x;
            q.note = 36.0f + 36.0f * x;
            q.trigger = (b % 600 == 0) ? 1 | 4 : (b % 600 < 300 ? 4 : 0);
            step(r, p, &q, &res, &t);
        }
    } else if (which == 2) {                                    /* random settings, each triggered */
        for (int k = 0; k < 24; k++) {
            q.harmonics = frand(); q.timbre = frand(); q.morph = frand();
            q.note = 24.0f + 72.0f * frand(); q.accent = frand();
            for (int b = 0; b < 400; b++) {
                q.trigger = b == 0 ? 1 | 4 : b < 100 ? 4 : 0;
                step(r, p, &q, &res, &t);
            }
        }
    } else {                                                    /* extreme notes */
        const float notes[4] = { 0.0f, 12.0f, 108.0f, 127.0f };
        for (int k = 0; k < 4; k++) {
            q.note = notes[k];
            q.harmonics = k * 0.3f; q.timbre = 1.0f - k * 0.3f; q.morph = 0.5f;
            for (int b = 0; b < 500; b++) {
                q.trigger = b == 0 ? 1 | 4 : b < 100 ? 4 : 0;
                step(r, p, &q, &res, &t);
            }
        }
    }
    double rel = res.max_diff / (res.peak > 1e-3 ? res.peak : 1e-3);
    int ok = rel < 1e-4 && !res.env_mismatch;
    printf("plaits_parity: %-12s %-10s peak %8.4f  max diff %.3g (%.3g of peak)%s  %s\n", NAMES[m], label, res.peak,
           res.max_diff, rel, res.env_mismatch ? "  ENVELOPED FLAG DIFFERS" : "", ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    int only = -1, bad = 0, ported = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else only = atoi(argv[i]);
    }
    static const char *LABELS[4] = { "free", "sweep", "random", "extremes" };
    for (int m = 0; m < M_COUNT; m++) {
        if (only >= 0 && m != only) continue;
        int any = 0;
        for (int s = 0; s < 4; s++) {
            int r = scenario(m, s, LABELS[s]);
            if (r < 0) break;
            any = 1;
            bad += r;
        }
        if (!any) printf("plaits_parity: %-12s not ported\n", NAMES[m]);
        ported += any;
    }
    printf("plaits_parity: %d model(s) compared, %d scenario(s) failed\n", ported, bad);
    return bad != 0;
}
