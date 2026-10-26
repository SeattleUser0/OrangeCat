/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat, branch dub-tape) DUB TAPE's first hardware row (docs/orangecat/DUB_TAPE_PLAN.md section 8, rows 1 and 2): the research
 * kernel (tools/dubtape/src/dubdelay.c, included unchanged) run as a CPU1 job beside the track render, to measure what one instance and
 * four cost there. A measurement build only (OC_DUB_BENCH=1, tools/build.py); no other build has any of it. Included by mix.c.
 *
 * The kernel's block is 128 frames (DD_BLOCK), the audio interrupt's half. So once a half, at its first 32-sample block (db_b 0, set by
 * audio.c): the job is submitted before the tracks render (mix_block, before mix_prepare_batch) on the previous half's input, and
 * joined after them, before the effects' bus job (CPU1 takes one job at a time, audio_worker.h). Its input: each bench instance's
 * track's mono engine output (part_buf, mix_part_engine, before DIST and LEVEL: the cost does not depend on the signal; the plan's
 * insert point is after LEVEL); its output, the instances' wet summed, is added into the dry mix at a quarter of full scale one half
 * late, so the run is heard. With CPU1 retired, or forced off (op 1, cpu0), the job runs on CPU0 in the same place, before the tracks.
 *
 * Memory: one 16 KB line pool in .pool (taken from the arena's main bank in this build; 32 KB would put the bank under its floor,
 * app.ld _resource_minimum), split between the instances: one gets 8,192 samples (0.37 s at the half rate, the plan's LEN 16K; 0.19 s
 * at the full, 0.74 s at the quarter), two 4,096, three or four 2,048 each. The work a sample does not depend on the line's length. The states
 * (dd_t, 2.7 KB each) in .pool too; the kernel's own tables (1 KB) in .bss. Its code in flash (.text.xip_jfx, db_run and the kernel's
 * loops; everything only they call stays there: tools/audio_ram.py), as the shared effects' and as the plan places it; the tables it reads
 * every sample are in RAM (its dd_t, hb_h, sintab).
 *
 * Counters (editor command 104 DUB_BENCH, web/EDITOR_PROTOCOL.md): jobs run on CPU1 and on CPU0, the job's ticks (min, mean, max and
 * a histogram of 16 us bins to 3,072 us for the 99th percentile), CPU0's wait at the join (count, mean, max), submits refused.
 * Reviewed before the first flash (an independent pass: the join-wait sum 64 bits, the histogram over the whole half, the counters
 * copied with interrupts off, compiler barriers around db_n, CPU1's count and index clamped). */
#if OC_DUB_BENCH
#pragma push_macro("Q16")
#undef Q16
#define hb_dec dd_hb_dec                               /* (jfx.c has its own half-band stages by these names) */
#define hb_int dd_hb_int
#define DD_NOINL JFX_XIP                               /* (the kernel's three loops in flash; the rest of it too: only db_run, in flash, calls it) */
#include "../../tools/dubtape/src/dubdelay.c"
#undef DD_NOINL
#undef hb_dec
#undef hb_int
#undef Q16
#pragma pop_macro("Q16")

#define DB_POOL 8192u                                  /* int16 line samples shared by the instances (16 KB: what the arena's floor
                                                        * leaves, app.ld _resource_minimum) */
#define DB_BINS 192u                                   /* 16 us each, to 3,072 us (the half is 2,902 us); the last takes the rest */
static int16_t db_line[DB_POOL] __attribute__((section(".pool"), aligned(32)));
static dd_t db_dd[4] __attribute__((section(".pool"), aligned(32)));
static float db_in[2][4][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));   /* [the half's buffer][instance][frame] */
static float db_out[2][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));     /* the instances' wet summed, L and R */
static float db_scr[2][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));     /* one instance's output */
static dd_params_t db_p[4];
static volatile uint8_t db_n;                          /* instances running (0: the bench off) */
static uint8_t db_b, db_cur, db_job_buf, db_pending, db_cpu0, db_src[4];
static uint32_t db_cfg[5] = {0, 2, 1, 0, 55};          /* n, R, algo, pattern, feedback %: as op 1 set them */
static volatile uint32_t db_job_ticks;                 /* the last job's, written by the core that ran it */
typedef struct {
    uint32_t jobs1, jobs0, refused, waits, tmin, tmax, wmax, hist[DB_BINS];
    uint64_t tsum, wsum;                               /* (64 bits: a soak's sums of ticks pass 2^32 in minutes) */
} db_st_t;
static db_st_t db_st __attribute__((section(".pool")));

static volatile uint32_t audio_halves;                 /* (audio.c's, defined after this file) */
JFX_XIP static void db_run(void *unused)               /* the job: every instance on the previous half's input (in flash, as
                                                        * the plan places the kernel: tools/audio_ram.py leaves .text.xip* there) */
{
    uint32_t t0 = fm1_ticks(), k, i, n = db_n;
    const float (*in)[DD_BLOCK] = db_in[db_job_buf & 1u];   /* (CPU1 can misread shared SRAM, keremimo/melodee#17: an index or a count
                                                             * out of range must never reach memory past the buffers) */
    (void)unused;
    if (n > 4u) n = 4u;
    memset(db_out, 0, sizeof db_out);
    for (k = 0; k < n; k++) {
        dd_process(&db_dd[k], &db_p[k], in[k], db_scr[0], db_scr[1]);
        for (i = 0; i < DD_BLOCK; i++) { db_out[0][i] += db_scr[0][i]; db_out[1][i] += db_scr[1][i]; }
    }
    db_job_ticks = fm1_ticks() - t0;
}
static void db_count(uint32_t ticks)
{
    uint32_t us = ticks / FM1_TICKS_PER_US;
    if (!db_st.tmin || ticks < db_st.tmin) db_st.tmin = ticks;
    if (ticks > db_st.tmax) db_st.tmax = ticks;
    db_st.tsum += ticks;
    db_st.hist[us / 16u < DB_BINS ? us / 16u : DB_BINS - 1u]++;
}
/* mix_part_engine: track ti's mono engine output for this block (n 0: silent) */
static inline void db_take(uint32_t ti, const int32_t *x, uint32_t n)
{
    uint32_t k, i;
    for (k = 0; k < db_n; k++)
        if (db_src[k] == ti && n)
            for (i = 0; i < n && i < CTL; i++) db_in[db_cur][k][db_b * CTL + i] = (float)x[i] * (1.0f / 32768.0f);
}
/* mix_block, before the tracks: at a half's first block, the buffers swapped and the job on its way */
static void db_start(void)
{
    if (db_b || !db_n) return;
    db_job_buf = db_cur;
    db_cur ^= 1u;
    memset(db_in[db_cur], 0, sizeof db_in[db_cur]);
#if MELODEE_DUAL_CORE
    if (!db_cpu0) {
        if (audio_worker_submit(db_run, 0)) { db_pending = 1; return; }
        db_st.refused++;
    }
#endif
    db_run(0);                                         /* (CPU0: forced, or CPU1 busy or retired) */
    db_st.jobs0++;
    db_count(db_job_ticks);
}
/* mix_block, after the tracks: joined at the first block (CPU0's wait counted), then the wet into the dry mix every block */
static void db_end(int32_t *l, int32_t *r, uint32_t n)
{
    uint32_t i, o = db_b * CTL;
    if (!db_n) return;
#if MELODEE_DUAL_CORE
    if (!db_b && db_pending) {
        uint32_t t0 = fm1_ticks(), w;
        int ok = audio_worker_join();
        w = fm1_ticks() - t0;
        db_pending = 0;
        db_st.waits++; db_st.wsum += w;
        if (w > db_st.wmax) db_st.wmax = w;
        if (!ok) { memset(db_out, 0, sizeof db_out); return; }
        db_st.jobs1++;
        db_count(db_job_ticks);
    }
#endif
    for (i = 0; i < n && o + i < DD_BLOCK; i++) {
        l[i] += (int32_t)(db_out[0][o + i] * 8192.0f);
        r[i] += (int32_t)(db_out[1][o + i] * 8192.0f);
    }
}
/* op 1 (the main loop): n instances (0..4) on tracks 1..n, the line rate's divisor R (1, 2, 4), algo (1 LITE, 2 TAPE), pattern (0 SLAP:
 * heads 1 and 3; 1 TRIPLE: 1, 2, 3), feedback %, cpu0 (1: run it on CPU0). The bench stops first and waits two halves (no job of the
 * old setting left), then every instance is set up from silence. 0 done, 1 refused */
static int db_config(uint32_t n, uint32_t R, uint32_t algo, uint32_t pat, uint32_t fb, uint32_t cpu0)
{
    uint32_t k, len, h0;
    if (n > 4u || (R != 1u && R != 2u && R != 4u) || (algo != 1u && algo != 2u) || pat > 1u || fb > 120u) return 1;
    db_n = 0;
    __asm__ volatile("" ::: "memory");                 /* (the flag first, then the wait, then the set-up: no store moved across) */
    h0 = audio_halves;
    while ((uint32_t)(audio_halves - h0) < 2u) fm1_wdt_feed();
    __asm__ volatile("" ::: "memory");
    memset(&db_st, 0, sizeof db_st);
    memset(db_in, 0, sizeof db_in); memset(db_out, 0, sizeof db_out);
    len = n <= 1u ? DB_POOL : n == 2u ? DB_POOL / 2u : DB_POOL / 4u;
    for (k = 0; k < n; k++) {
        dd_params_t *p = &db_p[k];
        dd_init(&db_dd[k], (int)algo, (int)R, DD_AA_HB, 1, db_line + k * len, 0, len);
        memset(p, 0, sizeof *p);
        p->head[0] = 1.0f; p->head[1] = pat ? 2.0f : 3.0f; p->head[2] = pat ? 3.0f : 0.0f;
        p->level[0] = 0.75f; p->level[1] = 0.5f; p->level[2] = pat ? 0.4f : 0.0f;
        p->pan[0] = 0.3f; p->pan[1] = -0.4f; p->pan[2] = 0.0f;
        p->fbw[0] = 0.2f; p->fbw[1] = pat ? 0.3f : 0.8f; p->fbw[2] = pat ? 0.5f : 0.0f;
        p->time = dd_max_seconds(&db_dd[k]) * 0.98f / (pat ? 3.0f : 3.0f);   /* the longest head (3 x TIME) at the line's end */
        p->feedback = (float)fb / 100.0f;
        p->hp = 20.0f; p->lp = 80.0f; p->res = 0.3f; p->drive = 0.4f;
        p->wow = 0.25f; p->flutter = 0.25f; p->send = 1.0f; p->wet = 1.0f; p->dry = 0.0f; p->glide = 0.12f; p->route = 1;
        db_src[k] = (uint8_t)k;
    }
    db_cfg[0] = n; db_cfg[1] = R; db_cfg[2] = algo; db_cfg[3] = pat; db_cfg[4] = fb;
    db_cpu0 = (uint8_t)cpu0;
    db_cur = 0; db_pending = 0;
    __asm__ volatile("" ::: "memory");                 /* (every set-up store before the flag the interrupt reads) */
    db_n = (uint8_t)n;
    return 0;
}
#else
static inline void db_take(uint32_t ti, const int32_t *x, uint32_t n) { (void)ti; (void)x; (void)n; }
static inline void db_start(void) {}
static inline void db_end(int32_t *l, int32_t *r, uint32_t n) { (void)l; (void)r; (void)n; }
#endif
