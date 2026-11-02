/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 2: DUB TAPE's kernel (firmware/src/dubdelay.c), the host tests of its behaviour: the repeats at TIME, 2 TIME and 3 TIME, each darker; (round 13) two taps at their own times, PING crossing the channels, SPEED bending the repeats' pitch, WOW, the wet spring gated at a mix of 0; FEEDBACK
 * above 1 bounded; the line through its block table the same as one buffer; TIME changes and the filter sweep continuous; the cut's ramps; the spring in both
 * placements stable; the quarter rate. Run by tests/run_tests.sh. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/dubdelay.c"
#include "dub_scenes.h"

static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static dd_t dd;
static int16_t *blk[DD_MAXBLK];
static int16_t *pool;
static void setup(int R, uint32_t nblk, int contiguous)
{
    uint32_t k;
    free(pool);
    pool = calloc(nblk * (DD_BLK_N + 64u), sizeof(int16_t));     /* (padding between blocks when not contiguous: the table is what finds them) */
    for (k = 0; k < nblk; k++) blk[k] = contiguous ? pool + k * DD_BLK_N : pool + k * (DD_BLK_N + 64u);
    dk_init(&dd, R, blk, nblk);
}
static void params(dd_params_t *p, float time, float fb)
{
    memset(p, 0, sizeof *p);
    p->time_l = p->time_r = time; p->speed = 1.0f;       /* (round 13: two taps, both at TIME: the same echo in both channels, the feedback the sum of the two at half each) */
    p->feedback = fb; p->hp = 15.0f; p->lp = 119.0f; p->res = 0.3f; p->drive = 0.4f; p->send = 1.0f; p->wet = 1.0f; p->glide = 0.12f; p->route = 1;
    p->lim_ceil = 0.891f;
}
/* n blocks, the input from in[] (or 0), the left output into out[] */
static void run(const dd_params_t *p, const float *in, float *out, uint32_t nb)
{
    float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK];
    for (uint32_t b = 0; b < nb; b++) {
        for (uint32_t i = 0; i < DD_BLOCK; i++) x[i] = in ? in[b * DD_BLOCK + i] : 0.0f;
        dk_process(&dd, p, x, l, r);
        for (uint32_t i = 0; i < DD_BLOCK; i++) out[b * DD_BLOCK + i] = 0.5f * (l[i] + r[i]);
    }
}
static float peak_in(const float *o, uint32_t a, uint32_t b, uint32_t *at)
{
    float m = 0.0f;
    for (uint32_t i = a; i < b; i++) if (fabsf(o[i]) > m) { m = fabsf(o[i]); if (at) *at = i; }
    return m;
}

int main(void)
{
    const uint32_t NB = 1500, N = NB * DD_BLOCK;
    float *in = calloc(N, sizeof(float)), *out = calloc(N, sizeof(float)), *out2 = calloc(N, sizeof(float));
    dd_params_t p;
    uint32_t i, at1 = 0, at2 = 0, at3 = 0;
    float a1, a2, a3;

    /* 1. the repeats: an impulse, SINGLE head at TIME 0.25 s, half rate, feedback 0.6 */
    setup(2, 8, 1);
    params(&p, 0.25f, 0.6f);
    in[0] = 0.5f;
    run(&p, in, out, NB);
    a1 = peak_in(out, 9000, 14000, &at1);
    a2 = peak_in(out, 20000, 25000, &at2);
    a3 = peak_in(out, 31000, 36000, &at3);
    printf("dub: repeats at %u, %u, %u samples (TIME 11025), peaks %.4f %.4f %.4f\n", at1, at2, at3, a1, a2, a3);
    CHECK(at1 > 11025 - 100 && at1 < 11025 + 160, "the first repeat at %u", at1);
    CHECK(at2 > 2 * 11025 - 100 && at2 < 2 * 11025 + 260, "the second repeat at %u", at2);
    CHECK(at3 > 3 * 11025 - 100 && at3 < 3 * 11025 + 360, "the third repeat at %u", at3);
    CHECK(a1 > 0.02f && a2 < a1 && a3 < a2, "each repeat quieter: %f %f %f", a1, a2, a3);
    CHECK(at2 - at1 > 11025 - 8 && at2 - at1 < 11025 + 8 + 20, "the repeats %u apart (the converters' offset is constant)", at2 - at1);

    /* 2. the line through its block table equals one buffer */
    for (i = 0; i < N; i++) in[i] = 0.3f * sinf(0.05f * (float)i) * ((i / 3000) & 1u ? 1.0f : 0.0f);
    setup(2, 6, 1); params(&p, 0.1f, 0.8f); p.time_r = 0.2f; run(&p, in, out, NB);
    setup(2, 6, 0); run(&p, in, out2, NB);
    CHECK(!memcmp(out, out2, N * sizeof(float)), "blocks apart in memory are not the same as one buffer");
    printf("dub: the same output on 6 blocks side by side and apart: %s\n", !memcmp(out, out2, N * sizeof(float)) ? "yes" : "NO");

    /* 3. FEEDBACK 1.27 is bounded by the loop's saturator and the limiter */
    setup(2, 4, 1); params(&p, 0.05f, 1.27f);
    for (i = 0; i < N; i++) in[i] = i < 4000 ? 0.7f * sinf(0.02f * (float)i) : 0.0f;
    run(&p, in, out, NB);
    a1 = peak_in(out, 0, N, 0);
    printf("dub: FEEDBACK 1.27, the loudest wet sample %.4f\n", a1);
    CHECK(a1 <= 1.0001f, "unbounded: %f", a1);
    CHECK(peak_in(out, N - 2000, N, 0) > 0.05f, "the build should be audible at the end");

    /* 4. nothing in, nothing out; WET 0 */
    setup(2, 4, 1); params(&p, 0.1f, 0.5f);
    run(&p, 0, out, 100);
    CHECK(peak_in(out, 0, 100 * DD_BLOCK, 0) == 0.0f, "silence in, sound out");
    for (i = 0; i < N; i++) in[i] = 0.5f * sinf(0.1f * (float)i);
    params(&p, 0.1f, 0.5f); p.wet = 0.0f;
    setup(2, 4, 1); run(&p, in, out, 200);
    CHECK(peak_in(out, 0, 200 * DD_BLOCK, 0) == 0.0f, "WET 0 is not silent");

    /* 5. TIME changes are continuous (glide): no step beyond what the signal itself does */
    setup(2, 8, 1); params(&p, 0.1f, 0.7f);
    for (i = 0; i < N; i++) in[i] = 0.4f * sinf(0.03f * (float)i);
    {   float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK], last = 0.0f, maxd = 0.0f;
        for (uint32_t b = 0; b < 1000; b++) {
            if (b == 300) p.time_l = p.time_r = 0.2f;
            if (b == 600) p.time_l = p.time_r = 0.05f;
            for (i = 0; i < DD_BLOCK; i++) x[i] = in[b * DD_BLOCK + i];
            dk_process(&dd, &p, x, l, r);
            for (i = 0; i < DD_BLOCK; i++) { float d = fabsf(l[i] - last); if (b > 100 && d > maxd) maxd = d; last = l[i]; }
        }
        printf("dub: the largest sample step across two TIME jumps %.4f\n", maxd);
        CHECK(maxd < 0.25f, "a TIME jump clicks: %f", maxd);
    }

    /* 6. the cut: the wet goes to 0 and comes back with no step */
    setup(2, 4, 1); params(&p, 0.05f, 0.8f); p.cut_wet = 1;
    for (i = 0; i < N; i++) in[i] = 0.4f * sinf(0.03f * (float)i);
    {   float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK], last = 0.0f, maxd = 0.0f, tail = 1.0f;
        for (uint32_t b = 0; b < 600; b++) {
            p.cut = b >= 200 && b < 400;
            for (i = 0; i < DD_BLOCK; i++) x[i] = in[b * DD_BLOCK + i];
            dk_process(&dd, &p, x, l, r);
            for (i = 0; i < DD_BLOCK; i++) { float d = fabsf(l[i] - last); if (b > 100 && d > maxd) maxd = d; last = l[i]; }
            if (b == 399) tail = peak_in(l, 0, DD_BLOCK, 0);
        }
        printf("dub: the cut: wet after 200 blocks cut %.6f, the largest step %.4f\n", tail, maxd);
        CHECK(tail < 1e-4f, "the cut leaves %f", tail);
        CHECK(maxd < 0.25f, "the cut clicks: %f", maxd);
    }

    /* 7. the spring in both placements: stable and decaying */
    for (int place = 1; place <= 2; place++) {
        int16_t *sb = calloc(DD_SP_BUF, sizeof(int16_t));
        setup(2, 4, 1); dk_spring_attach(&dd, sb);
        params(&p, 0.08f, 0.5f); p.spring = place; p.sp_mix = 0.7f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f; p.sp_in = place == 2 ? 0.3f : 0.0f;
        for (i = 0; i < N; i++) in[i] = i < 3000 ? 0.6f * sinf(0.04f * (float)i) : 0.0f;
        run(&p, in, out, NB);
        a1 = peak_in(out, 0, N, 0);
        a2 = peak_in(out, N - 4000, N, 0);
        printf("dub: spring %s, peak %.4f, the last 4000 samples %.5f\n", place == 1 ? "in the loop" : "on the wet", a1, a2);
        CHECK(a1 <= 1.0001f && a1 > 0.05f, "spring placement %d peak %f", place, a1);
        CHECK(a2 < 0.5f * a1, "spring placement %d does not decay: %f then %f", place, a1, a2);
        free(sb);
    }

    /* 7b. the loop spring crossfades: at a mix of 0 the output is the spring-off output bit for bit; the mix swept 0 -> 0.7 -> 0 steps no more than the signal */
    {   int16_t *sb = calloc(DD_SP_BUF, sizeof(int16_t));
        float *o1 = calloc(N, sizeof(float)), maxd = 0.0f, last = 0.0f;
        for (i = 0; i < N; i++) in[i] = 0.4f * sinf(0.03f * (float)i) * ((i / 4000) & 1u ? 1.0f : 0.5f);
        setup(2, 4, 1); params(&p, 0.05f, 0.7f); run(&p, in, out, 600);          /* spring off */
        setup(2, 4, 1); dk_spring_attach(&dd, sb); params(&p, 0.05f, 0.7f); p.spring = 1; p.sp_mix = 0.0f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f;
        run(&p, in, o1, 600);
        CHECK(!memcmp(out, o1, 600u * DD_BLOCK * sizeof(float)), "the loop spring at a mix of 0 is not the spring-off output");
        printf("dub: the loop spring at a mix of 0 equals no spring: %s\n", !memcmp(out, o1, 600u * DD_BLOCK * sizeof(float)) ? "yes" : "NO");
        setup(2, 4, 1); memset(sb, 0, DD_SP_BUF * sizeof(int16_t)); dk_spring_attach(&dd, sb); params(&p, 0.05f, 0.7f); p.spring = 1; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f;
        {   float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK];
            for (uint32_t b = 0; b < 1200; b++) {
                p.sp_mix = b < 300 ? 0.0f : b < 600 ? 0.7f * (float)(b - 300) / 300.0f : b < 900 ? 0.7f : 0.7f * (float)(1200 - b) / 300.0f;
                for (i = 0; i < DD_BLOCK; i++) x[i] = in[(b * DD_BLOCK + i) % N];
                dk_process(&dd, &p, x, l, r);
                for (i = 0; i < DD_BLOCK; i++) { float d = fabsf(l[i] - last); if (b > 100 && d > maxd) maxd = d; last = l[i]; }
            }
        }
        {   float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK], dmax = 0.0f;       /* and the mix thrown from 0.7 to 0 in one block (a hard bypass would step by the tank's whole difference from the loop) */
            for (uint32_t b = 0; b < 400; b++) {
                p.sp_mix = b < 100 ? 0.7f : 0.0f;
                for (i = 0; i < DD_BLOCK; i++) x[i] = in[(b * DD_BLOCK + i) % N];
                dk_process(&dd, &p, x, l, r);
                for (i = 0; i < DD_BLOCK; i++) { float d = fabsf(l[i] - last); if (b >= 100 && b < 140 && d > dmax) dmax = d; last = l[i]; }
            }
            printf("dub: the loop spring's mix thrown from 0.7 to 0: the largest sample step in the 40 blocks after %.4f\n", dmax);
            CHECK(dmax < 0.12f, "the spring's bypass steps: %f", dmax); }
        printf("dub: the loop spring's mix swept 0 to 0.7 and back: the largest sample step %.4f\n", maxd);
        CHECK(maxd < 0.25f, "the spring's crossfade clicks: %f", maxd);
        free(o1); free(sb);
    }

    /* 7c. (round 4) the wet spring's toggle: placement 2 on and off while a tone plays steps no more than the same stretch without the toggle (the mix glides as the loop's does; the tank is cleared only after it has faded) */
    {   int16_t *sb = calloc(DD_SP_BUF, sizeof(int16_t));
        float step_ref = 0.0f, step_off = 0.0f, step_on = 0.0f;
        for (i = 0; i < N; i++) in[i] = 0.4f * sinf(0.03f * (float)i);
        for (int cut = 0; cut < 2; cut++)
        for (int mode = 0; mode < 3; mode++) {                       /* 0: spring on throughout; 1: on, then off at block 200; 2: off, then on at block 200 */
            float x[DD_BLOCK], l[DD_BLOCK], r[DD_BLOCK], last = 0.0f, mx = 0.0f;
            setup(2, 4, 1); memset(sb, 0, DD_SP_BUF * sizeof(int16_t)); dk_spring_attach(&dd, sb); params(&p, 0.05f, 0.7f);
            p.sp_mix = 0.7f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f; p.sp_in = 0.3f; p.tap_cut = cut ? 0.5f : 0.0f;
            for (uint32_t b = 0; b < 400; b++) {
                p.spring = mode == 0 ? 2 : mode == 1 ? (b < 200 ? 2 : 0) : (b < 200 ? 0 : 2);
                for (i = 0; i < DD_BLOCK; i++) x[i] = in[(b * DD_BLOCK + i) % N];
                dk_process(&dd, &p, x, l, r);
                for (i = 0; i < DD_BLOCK; i++) { float d = fabsf(l[i] - last); if (b >= 200 && b < 240 && d > mx) mx = d; last = l[i]; }
            }
            if (mode == 0) step_ref = mx; else if (mode == 1) step_off = mx; else step_on = mx;
            if (mode == 2) {
                printf("dub: the wet spring toggled (tap cut %.1f): the largest sample step in the 40 blocks after, off %.4f, on %.4f, never toggled %.4f\n", p.tap_cut, step_off, step_on, step_ref);
                CHECK(step_off < 1.3f * step_ref + 0.01f, "the wet spring's off steps (cut %d): %f against %f", cut, step_off, step_ref);
                CHECK(step_on < 1.3f * step_ref + 0.01f, "the wet spring's on steps (cut %d): %f against %f", cut, step_on, step_ref);
            }
        }
        printf("dub: the wet spring toggled: the largest sample step in the 40 blocks after, off %.4f, on %.4f, never toggled %.4f\n", step_off, step_on, step_ref);
        CHECK(step_off < 1.3f * step_ref + 0.01f, "the wet spring's off steps: %f against %f", step_off, step_ref);
        CHECK(step_on < 1.3f * step_ref + 0.01f, "the wet spring's on steps: %f against %f", step_on, step_ref);
        free(sb);
    }

    /* 7d. (round 13) two taps at their own times: an impulse, no feedback: the left tap's echo in the left channel at TIME L only, the right tap's in the right at TIME R only */
    {   float *l2 = calloc(N, sizeof(float)), *r2 = calloc(N, sizeof(float)), xb[DD_BLOCK], lb[DD_BLOCK], rb[DD_BLOCK];
        uint32_t al = 0, ar = 0;
        float pl1, pl2, pr1, pr2;
        setup(2, 8, 1); params(&p, 0.1f, 0.0f); p.time_r = 0.25f;
        for (uint32_t b = 0; b < 600; b++) {
            for (i = 0; i < DD_BLOCK; i++) xb[i] = b == 0 && i == 0 ? 0.5f : 0.0f;
            dk_process(&dd, &p, xb, lb, rb);
            for (i = 0; i < DD_BLOCK; i++) { l2[b * DD_BLOCK + i] = lb[i]; r2[b * DD_BLOCK + i] = rb[i]; }
        }
        pl1 = peak_in(l2, 3500, 5500, &al); pl2 = peak_in(l2, 10000, 12500, 0);
        pr1 = peak_in(r2, 10000, 12500, &ar); pr2 = peak_in(r2, 3500, 5500, 0);
        printf("dub: two taps: left echo at %u (TIME L 4410) %.4f, right echo at %u (TIME R 11025) %.4f; cross: %.5f %.5f\n", al, pl1, ar, pr1, pl2, pr2);
        CHECK(al > 4410 - 100 && al < 4410 + 200 && pl1 > 0.02f, "the left tap's echo at %u", al);
        CHECK(ar > 11025 - 100 && ar < 11025 + 300 && pr1 > 0.02f, "the right tap's echo at %u", ar);
        CHECK(pl2 < 0.002f && pr2 < 0.002f, "the taps leak into the other channel: %f %f", pl2, pr2);
        free(l2); free(r2);
    }

    /* 7e. PING: the feedback is the right tap alone, so the left channel's second echo comes at TIME R + TIME L, not at 2 TIME L */
    {   float *l2 = calloc(N, sizeof(float)), *r2 = calloc(N, sizeof(float)), xb[DD_BLOCK], lb[DD_BLOCK], rb[DD_BLOCK], dbl[2], sum[2];
        for (int ping = 0; ping < 2; ping++) {
            setup(2, 8, 1); params(&p, 0.05f, 0.9f); p.time_r = 0.1f; p.ping = ping; p.wet = 1.0f;
            for (uint32_t b = 0; b < 300; b++) {
                for (i = 0; i < DD_BLOCK; i++) xb[i] = b == 0 && i == 0 ? 0.5f : 0.0f;
                dk_process(&dd, &p, xb, lb, rb);
                for (i = 0; i < DD_BLOCK; i++) { l2[b * DD_BLOCK + i] = lb[i]; r2[b * DD_BLOCK + i] = rb[i]; }
            }
            dbl[ping] = peak_in(l2, 4300, 4900, 0);                    /* 2 TIME L = 0.1 s = 4410 samples */
            sum[ping] = peak_in(l2, 6500, 7200, 0);                    /* TIME R + TIME L = 0.15 s = 6615 */
        }
        printf("dub: PING off: the left channel at 2 TIME L %.4f, at TIME R + TIME L %.4f; PING on: %.4f, %.4f\n", dbl[0], sum[0], dbl[1], sum[1]);
        CHECK(dbl[0] > 0.02f && dbl[1] < 0.25f * dbl[0], "PING should take the left channel's 2 TIME L echo away: %f then %f", dbl[0], dbl[1]);
        CHECK(sum[1] > 0.02f && sum[1] > 1.5f * sum[0], "PING should bring the TIME R + TIME L echo to the left: %f then %f", sum[0], sum[1]);
        free(l2); free(r2);
    }

    /* 7f. SPEED: at twice the speed the echo arrives in half the time (once the head glide has settled); at half the speed, in twice */
    {   static const float spd[3] = {1.0f, 2.0f, 0.5f};
        float xb[DD_BLOCK], lb[DD_BLOCK], rb[DD_BLOCK];
        uint32_t at[3] = {0, 0, 0};
        for (int k = 0; k < 3; k++) {
            float *l2 = calloc(N, sizeof(float));
            setup(2, 8, 1); params(&p, 0.2f, 0.0f); p.speed = spd[k];
            for (uint32_t b = 0; b < 300; b++) { for (i = 0; i < DD_BLOCK; i++) xb[i] = 0.0f; dk_process(&dd, &p, xb, lb, rb); }     /* the heads settle */
            for (uint32_t b = 0; b < 700; b++) {
                for (i = 0; i < DD_BLOCK; i++) xb[i] = b == 0 && i == 0 ? 0.5f : 0.0f;
                dk_process(&dd, &p, xb, lb, rb);
                for (i = 0; i < DD_BLOCK; i++) l2[b * DD_BLOCK + i] = lb[i];
            }
            peak_in(l2, 200, 700 * DD_BLOCK, &at[k]);
            free(l2);
        }
        printf("dub: SPEED 1, 2 and 0.5 on TIME 0.2 s: the echo at %u, %u and %u samples (8820, 4410, 17640)\n", at[0], at[1], at[2]);
        CHECK(at[0] > 8820 - 100 && at[0] < 8820 + 300, "speed 1: %u", at[0]);
        CHECK(at[1] > 4410 - 100 && at[1] < 4410 + 300, "speed 2: %u", at[1]);
        CHECK(at[2] > 17640 - 150 && at[2] < 17640 + 400, "speed 0.5: %u", at[2]);
    }

    /* 7g. WOW: a steady tone through the echo: with WOW the period of the echo wobbles (a pitch wobble of a few tenths of a percent), without it the period is constant */
    {   float sd[2];
        for (int w = 0; w < 2; w++) {
            float *l2 = calloc(N, sizeof(float)), xb[DD_BLOCK], lb[DD_BLOCK], rb[DD_BLOCK], last = 0.0f, per[200], mean = 0.0f, var = 0.0f, lz = -1.0f;
            uint32_t np = 0;
            setup(2, 8, 1); params(&p, 0.05f, 0.0f); p.wow = w ? 1.0f : 0.0f;
            for (uint32_t b = 0; b < 1400; b++) {
                for (i = 0; i < DD_BLOCK; i++) xb[i] = 0.4f * sinf(2.0f * 3.14159265f * 1000.0f / 44100.0f * (float)(b * DD_BLOCK + i));
                dk_process(&dd, &p, xb, lb, rb);
                for (i = 0; i < DD_BLOCK; i++) {
                    uint32_t n = b * DD_BLOCK + i;
                    if (n > 8000 && last < 0.0f && lb[i] >= 0.0f) { float z = (float)n - lb[i] / (lb[i] - last); if (lz > 0.0f && np < 200) per[np++] = z - lz; lz = z; }   /* (the crossing to a fraction of a sample) */
                    last = lb[i];
                }
            }
            for (uint32_t j = 0; j < np; j++) mean += per[j];
            mean /= (float)np;
            for (uint32_t j = 0; j < np; j++) var += (per[j] - mean) * (per[j] - mean);
            sd[w] = sqrtf(var / (float)np) / mean;
            free(l2);
        }
        printf("dub: WOW 0 and 1: the echo's period varies by %.5f and %.5f of the period\n", sd[0], sd[1]);
        CHECK(sd[0] < 0.003f && sd[1] > 2.0f * sd[0] + 0.001f, "WOW does not bend the echo's pitch: %f then %f", sd[0], sd[1]);
    }

    /* 7h. the wet spring at a mix of 0 is the spring-off output bit for bit (the tank does not run) */
    {   int16_t *sb = calloc(DD_SP_BUF, sizeof(int16_t));
        float *o1 = calloc(N, sizeof(float));
        for (i = 0; i < N; i++) in[i] = 0.4f * sinf(0.03f * (float)i) * ((i / 4000) & 1u ? 1.0f : 0.5f);
        setup(2, 4, 1); params(&p, 0.05f, 0.7f); run(&p, in, out, 600);
        setup(2, 4, 1); dk_spring_attach(&dd, sb); params(&p, 0.05f, 0.7f); p.spring = 2; p.sp_mix = 0.0f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f; p.sp_in = 0.3f;
        run(&p, in, o1, 600);
        printf("dub: the wet spring at a mix of 0 equals no spring: %s\n", !memcmp(out, o1, 600u * DD_BLOCK * sizeof(float)) ? "yes" : "NO");
        CHECK(!memcmp(out, o1, 600u * DD_BLOCK * sizeof(float)), "the wet spring at a mix of 0 is not the spring-off output");
        free(o1); free(sb);
    }

    /* 9. (round 6) converter B (round 13: the hashes regenerated for the two-tap kernel, the scenes' heads now the two taps of dub_scenes.h; the converters themselves are unchanged): the skank and throw scenes of the research prototype at the quarter rate through this dubdelay.c hash to the same 64-bit value as the renders the owner listened to
     * (~/dubtape-research/conv/conv_render.c with DD_CONV=1, the same tests/dub_scenes.h): the first converter stage at 11 taps (beta 6), the second as before. A change of either converter changes the hash on purpose. */
    {   static const uint64_t want[2] = {0x09ce1bf1a8301046ull, 0x53333ab9b80ad8e8ull};
        static const float dur[2] = {11.0f, 12.0f};
        for (int sc = 0; sc < 2; sc++) {
            int n = (int)(dur[sc] * 44100.0f);
            float *ol = (float *)calloc((size_t)n, sizeof(float)), *orr = (float *)calloc((size_t)n, sizeof(float));
            uint64_t h;
            sc_render(sc, ol, orr, n);
            h = sc_fnv(sc_fnv(1469598103934665603ull, ol, n), orr, n);
            printf("dub: converter B, scene %s through dubdelay.c: hash %016llx (round 13's: %016llx)\n", sc ? "throw" : "skank", (unsigned long long)h, (unsigned long long)want[sc]);
            CHECK(h == want[sc], "converter B's %s render differs from conv_render's", sc ? "throw" : "skank");
            free(ol); free(orr);
        }
        /* and the half rate (R = 2, one stage) is not touched: the same scenes hash to what release-2's dubdelay.c (the 31-tap stage) gave */
        {   static const uint64_t want2[2] = {0x31de0926e0affe34ull, 0x14ec6a457534bf1dull};
            for (int sc = 0; sc < 2; sc++) {
                int n = (int)(dur[sc] * 44100.0f);
                float *ol = (float *)calloc((size_t)n, sizeof(float)), *orr = (float *)calloc((size_t)n, sizeof(float));
                uint64_t h;
                sc_render_r(sc, ol, orr, n, 2);
                h = sc_fnv(sc_fnv(1469598103934665603ull, ol, n), orr, n);
                printf("dub: half rate, scene %s: hash %016llx (round 13's: %016llx)\n", sc ? "throw" : "skank", (unsigned long long)h, (unsigned long long)want2[sc]);
                CHECK(h == want2[sc], "the half rate's %s render changed", sc ? "throw" : "skank");
                free(ol); free(orr);
            }
        }
    }

    /* 8. the quarter rate: the repeat still at TIME */
    setup(4, 4, 1); params(&p, 0.3f, 0.5f);
    memset(in, 0, N * sizeof(float)); in[0] = 0.5f;
    run(&p, in, out, NB);
    a1 = peak_in(out, 0, 20000, &at1);
    printf("dub: quarter rate: first repeat at %u samples (TIME 13230), peak %.4f\n", at1, a1);
    CHECK(at1 > 13230 - 100 && at1 < 13230 + 400 && a1 > 0.02f, "the quarter rate's repeat at %u", at1);

    /* 9. the line holds what dk_max_seconds says */
    setup(2, 2, 1);
    CHECK(fabsf(dk_max_seconds(&dd) - (4096.0f - 48.0f) / 22050.0f) < 1e-4f, "max seconds %f", dk_max_seconds(&dd));
    printf(bad ? "jfx_dub: %d FAILED\n" : "jfx_dub: all passed\n", bad);
    return bad ? 1 : 0;
}
