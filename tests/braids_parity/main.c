/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.1: Braids parity. Each shape of the C port (firmware/src/braids/) runs beside Braids' own C++
 * MacroOscillator with the same settings, block by block (24 samples, as Braids renders), and their 16-bit outputs
 * are compared sample for sample: they must be equal.
 *
 *   tests/braids_parity/run.sh <eurorack checkout with stmlib> [shape] [-v]
 *
 * Scenarios per shape, each from a fresh oscillator and random seed: held at a middle setting; a sweep of TIMBRE
 * and COLOR with a pitch glide and a strike every 400 blocks; 24 random settings, each struck; the lowest and highest
 * notes; a sync pulse train (the sync input).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
void *ref_new(void); void ref_set(void *, int, int, int, int); void ref_strike(void *);
void ref_render(void *, const unsigned char *, short *, unsigned); void ref_seed(unsigned);
void *port_new(void); void port_set(void *, int, int, int, int); void port_strike(void *);
void port_render(void *, const unsigned char *, short *, unsigned); void port_seed(unsigned);
int port_supported(int); size_t port_size(void);

static const char *NAMES[48] = {
    "CSAW", "MORPH", "SAW_SQUARE", "SINE_TRIANGLE", "BUZZ", "SQUARE_SUB", "SAW_SUB", "SQUARE_SYNC", "SAW_SYNC",
    "TRIPLE_SAW", "TRIPLE_SQUARE", "TRIPLE_TRIANGLE", "TRIPLE_SINE", "TRIPLE_RING_MOD", "SAW_SWARM", "SAW_COMB", "TOY",
    "FILTER_LP", "FILTER_PK", "FILTER_BP", "FILTER_HP", "VOSIM", "VOWEL", "VOWEL_FOF", "HARMONICS", "FM", "FEEDBACK_FM",
    "CHAOTIC_FM", "PLUCKED", "BOWED", "BLOWN", "FLUTED", "STRUCK_BELL", "STRUCK_DRUM", "KICK", "CYMBAL", "SNARE",
    "WAVETABLES", "WAVE_MAP", "WAVE_LINE", "WAVE_PARAPHONIC", "FILTERED_NOISE", "TWIN_PEAKS_NOISE", "CLOCKED_NOISE",
    "GRANULAR_CLOUD", "PARTICLE_NOISE", "DIGITAL_MOD", "QUESTION_MARK"
};
#define BLOCK 24
static int verbose;
static uint32_t lcg = 1;
static int rnd(int n) { lcg = lcg * 1103515245u + 12345u; return (int)((lcg >> 8) % (uint32_t)n); }

static int scenario(int shape, int which)
{
    void *r = ref_new(), *p = port_new();
    short ro[BLOCK], po[BLOCK];
    unsigned char sync[BLOCK];
    long t = 0, first = -1, diffs = 0;
    int peak = 0, maxd = 0;
    int pitch = 60 << 7, timbre = 16384, color = 16384, blocks = which == 0 ? 2000 : 4000;
    ref_seed(0x21); port_seed(0x21);
    lcg = 99u + (uint32_t)which;
    memset(sync, 0, sizeof sync);
    for (int b = 0; b < blocks; b++) {
        if (which == 1) {
            timbre = b * 32767 / blocks;
            color = 32767 - b * 32767 / blocks;
            pitch = (36 << 7) + b * (48 << 7) / blocks;
        } else if (which == 2 && b % 160 == 0) {
            timbre = rnd(32768); color = rnd(32768); pitch = (24 << 7) + rnd(72 << 7);
        } else if (which == 3) {
            pitch = (b / 1000) % 2 ? 127 << 7 : 0;
            timbre = (b / 2000) ? 32767 : 0;
            color = 16384;
        } else if (which == 4) {
            for (int i = 0; i < BLOCK; i++) sync[i] = ((b * BLOCK + i) % 157) == 0;
            pitch = 50 << 7;
            timbre = (b * 7) % 32768;
        }
        ref_set(r, shape, pitch, timbre, color);
        port_set(p, shape, pitch, timbre, color);
        if ((which == 1 && b % 400 == 0) || (which == 2 && b % 160 == 0) || (which == 3 && b % 1000 == 0) || b == 0) {
            ref_strike(r);
            port_strike(p);
        }
        ref_render(r, sync, ro, BLOCK);
        port_render(p, sync, po, BLOCK);
        for (int i = 0; i < BLOCK; i++) {
            int d = abs(ro[i] - po[i]);
            if (abs(ro[i]) > peak) peak = abs(ro[i]);
            if (d > maxd) maxd = d;
            if (d) {
                diffs++;
                if (first < 0) {
                    first = t + i;
                    if (verbose) printf("    first difference at sample %ld: %d / %d\n", first, ro[i], po[i]);
                }
            }
        }
        t += BLOCK;
    }
    static const char *L[5] = { "held", "sweep", "random", "extremes", "sync" };
    printf("braids_parity: %-16s %-8s peak %5d  %6ld samples differ (max %d)  %s\n", NAMES[shape], L[which], peak, diffs,
           maxd, diffs ? "FAIL" : "ok");
    free(r); free(p);
    return diffs != 0;
}

int main(int argc, char **argv)
{
    int only = -1, bad = 0, n = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-v")) verbose = 1;
        else only = atoi(argv[i]);
    }
    printf("braids_parity: the port's MacroOscillator is %zu bytes\n", port_size());
    for (int s = 0; s < 48; s++) {
        if (only >= 0 && s != only) continue;
        if (!port_supported(s)) { printf("braids_parity: %-16s not ported\n", NAMES[s]); continue; }
        n++;
        for (int w = 0; w < 5; w++) bad += scenario(s, w);
    }
    printf("braids_parity: %d shape(s) compared, %d scenario(s) failed\n", n, bad);
    return bad != 0;
}
