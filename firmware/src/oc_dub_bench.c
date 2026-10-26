/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (branch measure/core-split: a measurement build, never merged) DUB TAPE in the two-core runs: the research kernel (tools/dubtape/src,
 * from dub-tape de78843, with the spring) run as the plan's alternative A schedules it: a 32-sample block at a time (DD_BLOCK 32), as a
 * CPU1 job submitted before each block's tracks on the previous block's input and joined after them, before the effects' bus job. Its job
 * is counted as its own kind in core_stats.h (cs_kind CS_K_DUB). OC_DUB_BENCH=1 builds only; included by mix.c.
 *
 * Input: each instance's track's mono engine output (part_buf). Output: the instances' wet into the dry mix; route 1 also into the REVERB
 * send (DUB TAPE into the shared REVERB in series: CLOUD when G_RTYPE is 4). The spring: off, in the loop or on the wet (dd_params_t).
 * Memory (the arena's floor, app.ld, leaves about 35 KB beside the core counters): an 8 KB line pool (one instance 4,096 samples, 0.19 s at
 * the half rate; four 1,024, 46 ms; the work a sample does not depend on the length), four spring buffers of DD_SP_BUF samples, the four
 * states in .pool. Editor command 104 (web/EDITOR_PROTOCOL.md) configures and reads it; the cores' numbers are AUDIO_STATS schema 9. */
#if OC_DUB_BENCH
#define DD_BLOCK 32
#define DD_SP_TANK 1024u                               /* (37 ms at the half rate is 816 samples) */
#define DD_SP_D1 128u
#define DD_SP_D2 256u
#pragma push_macro("Q16")
#undef Q16
#define hb_dec dd_hb_dec                               /* (jfx.c has its own half-band stages by these names) */
#define hb_int dd_hb_int
#define DD_NOINL JFX_XIP                               /* (the kernel's loops in flash; the rest of it too: only db_run, in flash, calls it) */
#include "../../tools/dubtape/src/dubdelay.c"
#undef DD_NOINL
#undef hb_dec
#undef hb_int
#undef Q16
#pragma pop_macro("Q16")
_Static_assert(DD_BLOCK == CTL, "a job a block");

#define DB_POOL 4096u                                  /* int16 line samples shared by the instances (8 KB) */
static int16_t db_line[DB_POOL] __attribute__((section(".pool"), aligned(32)));
static int16_t db_spb[4][DD_SP_BUF] __attribute__((section(".pool"), aligned(32)));
static dd_t db_dd[4] __attribute__((section(".pool"), aligned(32)));
static float db_in[2][4][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));
static float db_out[2][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));
static float db_scr[2][DD_BLOCK] __attribute__((section(".pool"), aligned(32)));
static dd_params_t db_p[4];
static volatile uint8_t db_n;
static uint8_t db_b, db_cur, db_job_buf, db_pending, db_cpu0, db_route;
static uint32_t db_cfg[10];
static volatile uint32_t audio_halves;                 /* (audio.c's, defined after this file) */

JFX_XIP static void db_run(void *unused)
{
    uint32_t k, i, n = db_n;
    const float (*in)[DD_BLOCK] = db_in[db_job_buf & 1u];   /* (a misread on CPU1 must not index past the buffers) */
    (void)unused;
    if (n > 4u) n = 4u;
    memset(db_out, 0, sizeof db_out);
    for (k = 0; k < n; k++) {
        dd_process(&db_dd[k], &db_p[k], in[k], db_scr[0], db_scr[1]);
        for (i = 0; i < DD_BLOCK; i++) { db_out[0][i] += db_scr[0][i]; db_out[1][i] += db_scr[1][i]; }
    }
}
static inline void db_take(uint32_t ti, const int32_t *x, uint32_t n)
{
    if (ti < db_n && n)
        for (uint32_t i = 0; i < n && i < CTL; i++) db_in[db_cur][ti][i] = (float)x[i] * (1.0f / 32768.0f);
}
/* before the tracks, every block: the buffers swapped, the previous block's input to the job */
static void db_start(void)
{
    if (!db_n) return;
    db_job_buf = db_cur;
    db_cur ^= 1u;
    memset(db_in[db_cur], 0, sizeof db_in[db_cur]);
#if MELODEE_DUAL_CORE
    if (!db_cpu0) {
#if OC_CORE_STATS
        cs_kind = CS_K_DUB;
#endif
        if (audio_worker_submit(db_run, 0)) { db_pending = 1; return; }
#if OC_CORE_STATS
        cs_kind = CS_K_VOICE;
#endif
    }
#endif
    db_run(0);                                         /* (CPU0: forced, or CPU1 busy or retired) */
}
/* after the tracks: joined (its wait the core counters'), the wet into the dry mix, and with route 1 into the REVERB send too */
static void db_end(int32_t *l, int32_t *r, int32_t *rev, uint32_t n)
{
    uint32_t i;
    if (!db_n) return;
#if MELODEE_DUAL_CORE
    if (db_pending) {
        int ok = audio_worker_join();
        db_pending = 0;
        if (!ok) { memset(db_out, 0, sizeof db_out); return; }
    }
#endif
    for (i = 0; i < n && i < DD_BLOCK; i++) {
        int32_t a = (int32_t)(db_out[0][i] * 8192.0f), b = (int32_t)(db_out[1][i] * 8192.0f);
        l[i] += a; r[i] += b;
        if (db_route) rev[i] += (a + b) >> 1;
    }
}
/* op 1: n R algo pattern feedback cpu0 [route spring sp_mix tap_cut]: n instances on tracks 1..n; spring 0 off, 1 the loop, 2 the wet;
 * sp_mix and tap_cut in percent. 0 done, 1 refused */
static int db_config(const uint32_t *c, uint32_t nc)
{
    uint32_t n = c[0], R = c[1], algo = c[2], pat = c[3], fb = c[4], k, len, h0;
    uint32_t route = nc > 6u ? c[6] : 0u, sp = nc > 7u ? c[7] : 0u, mix = nc > 8u ? c[8] : 70u, cut = nc > 9u ? c[9] : 0u;
    if (n > 4u || (R != 1u && R != 2u && R != 4u) || (algo != 1u && algo != 2u) || pat > 1u || fb > 120u || route > 1u || sp > 2u ||
        mix > 100u || cut > 100u) return 1;
    db_n = 0;
    __asm__ volatile("" ::: "memory");
    h0 = audio_halves;
    while ((uint32_t)(audio_halves - h0) < 2u) fm1_wdt_feed();
    __asm__ volatile("" ::: "memory");
    memset(db_in, 0, sizeof db_in); memset(db_out, 0, sizeof db_out);
    len = n <= 1u ? DB_POOL : n == 2u ? DB_POOL / 2u : DB_POOL / 4u;
    for (k = 0; k < n; k++) {
        dd_params_t *p = &db_p[k];
        dd_init(&db_dd[k], (int)algo, (int)R, DD_AA_HB, 1, db_line + k * len, 0, len);
        dd_spring_attach(&db_dd[k], db_spb[k]);
        memset(p, 0, sizeof *p);
        p->head[0] = 1.0f; p->head[1] = pat ? 2.0f : 3.0f; p->head[2] = pat ? 3.0f : 0.0f;
        p->level[0] = 0.75f; p->level[1] = 0.5f; p->level[2] = pat ? 0.4f : 0.0f;
        p->pan[0] = 0.3f; p->pan[1] = -0.4f;
        p->fbw[0] = 0.2f; p->fbw[1] = pat ? 0.3f : 0.8f; p->fbw[2] = pat ? 0.5f : 0.0f;
        p->time = dd_max_seconds(&db_dd[k]) * 0.98f / 3.0f;
        p->feedback = (float)fb / 100.0f;
        p->hp = 20.0f; p->lp = 80.0f; p->res = 0.3f; p->drive = 0.4f;
        p->wow = 0.25f; p->flutter = 0.25f; p->send = 1.0f; p->wet = 1.0f; p->dry = 0.0f; p->glide = 0.12f; p->route = 1;
        p->spring = (int)sp; p->sp_mix = (float)mix / 100.0f; p->sp_decay = 2.0f; p->sp_drive = 0.4f; p->sp_tone = 0.3f;
        p->sp_in = sp == 2u ? 0.3f : 0.0f; p->tap_cut = (float)cut / 100.0f;
    }
    for (k = 0; k < 10u; k++) db_cfg[k] = k < nc ? c[k] : 0u;
    db_cpu0 = (uint8_t)c[5]; db_route = (uint8_t)route;
    db_cur = 0; db_pending = 0;
    __asm__ volatile("" ::: "memory");
    db_n = (uint8_t)n;
    return 0;
}
#else
static inline void db_take(uint32_t ti, const int32_t *x, uint32_t n) { (void)ti; (void)x; (void)n; }
static inline void db_start(void) {}
static inline void db_end(int32_t *l, int32_t *r, int32_t *rev, uint32_t n) { (void)l; (void)r; (void)rev; (void)n; }
#endif
