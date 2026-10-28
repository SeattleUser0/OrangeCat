/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 2, stage 2 (firmware/src/jfx.c, MELODEE_FX_DEFER): the buses' returns reach the master one block (CTL samples) after the block that
 * sent to them. Built twice by tests/run_tests.sh (fx_defer): with MELODEE_FX_DEFER=0 it writes, per scenario, the returns the master mixes
 * (wet, side left, side right a block); with 1 it reads them and requires:
 *   SHIFT   a pure shift: every reverb model (PRE too) and the GRAIN delay (not compensated): block b equals the reference's block b - 1, bit for bit
 *   EXACT   the compensated taps: CHORUS (its LFO phase leads by CTL samples too, with and without the sweep): block b equals the reference's
 *           block b, bit for bit (the reads are CTL samples nearer)
 *   TAIL    DELAY at the full rate, with a right echo (WIDE), PING-PONG, DIGITAL, at a half and a quarter rate: bit for bit but for the gate's
 *           fade at the end of the tail (fx_gate runs on the bus's block clock: one block, below -85 dB of the reference)
 *   NEAR    where the compensation is not sample-exact: MOD (the wow's drift is on the bus's clock; extrapolated), a TIME change in flight (the
 *           crossfade runs on the bus's clock too): the error energy against the reference, in dB below it, printed and bounded
 *   RESET   after a reset (jfx_reset_now) the returns are silent in the very next master, and the sends set aside is cleared
 * Run with the build directory as argument 1 (the reference files). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <math.h>

#define NB 4800u                                  /* blocks a scenario: 3.5 s */
typedef struct { int32_t wet[CTL], sl[CTL], sr[CTL]; } ret_t;
static ret_t rec[NB];
static ret_t ref[NB];
static int32_t xo[2u * CTL], jt_rng_s = 1;
static int32_t fd_noise(void) { jt_rng_s = jt_rng_s * 1664525 + 1013904223; return (int32_t)((uint32_t)jt_rng_s >> 16) - 32768; }
static int bad;

static void fd_fresh(void)
{
    ui_power_on();
    song.master_q12 = 4096;
    song.g[G_LEVEL] = 0;
    song.g[G_BPM] = 120;
    song.playing = 0;
    jt_rng_s = 1;
}
static void fd_block(const int32_t *c, const int32_t *d, const int32_t *r)
{
    uint32_t i;
    if (jfx_reset_req) jfx_reset_now();
    jfx_lend_check();
    for (i = 0; i < CTL; i++) {
        mix_l[i] = mix_r[i] = sl_l[i] = sl_r[i] = 0;
        send_c[i] = c ? c[i] : 0;
        send_d[i] = d ? d[i] : 0;
        send_r[i] = r ? r[i] : 0;
    }
    jfx_clock_begin();
    master_begin();
    jfx_bus_start(CTL);
    jfx_master(xo, CTL, 0);
}
/* a scenario: bus (1 chorus, 2 delay, 3 reverb); set(): the song's settings; mid(b): a change at block b */
static void fd_run(const char *name, uint32_t bus, void (*set)(void), void (*mid)(uint32_t))
{
    static int32_t x[CTL];
    uint32_t b, i;
    fd_fresh();
    set();
    trk[0].p[bus == 1 ? P_CHOR : bus == 2 ? P_DLY : P_REV] = 100;
    for (b = 0; b < 8u; b++) fd_block(0, 0, 0);       /* the bus's memory comes in over a few blocks */
    for (b = 0; b < NB; b++) {
        int on = b < 64u || (b >= 1500u && b < 1520u);
        for (i = 0; i < CTL; i++) x[i] = on ? (int32_t)((int64_t)fd_noise() * 9000 >> 15) : 0;
        if (mid) mid(b);
        fd_block(bus == 1 ? x : 0, bus == 2 ? x : 0, bus == 3 ? x : 0);
        memcpy(rec[b].wet, fx_ret[fx_rr].wet, sizeof rec[b].wet);
        memcpy(rec[b].sl, fx_ret[fx_rr].sl, sizeof rec[b].sl);
        memcpy(rec[b].sr, fx_ret[fx_rr].sr, sizeof rec[b].sr);
    }
    (void)name;
}
static double fd_energy(const ret_t *a, uint32_t from, uint32_t to)
{
    double e = 0;
    for (uint32_t b = from; b < to; b++)
        for (uint32_t i = 0; i < CTL; i++) e += (double)a[b].wet[i] * a[b].wet[i] + (double)a[b].sl[i] * a[b].sl[i] + (double)a[b].sr[i] * a[b].sr[i];
    return e;
}
static uint32_t fd_first, fd_last;                 /* the first and last block that differs */
static double fd_err(const ret_t *a, const ret_t *r, int shift, uint32_t from, uint32_t to, uint32_t *nz)
{
    double e = 0;
    *nz = 0;
    fd_first = fd_last = 0;
    for (uint32_t b = from; b < to; b++) {
        const ret_t *q = &r[b - (uint32_t)shift];
        for (uint32_t i = 0; i < CTL; i++) {
            double d0 = (double)a[b].wet[i] - q->wet[i], d1 = (double)a[b].sl[i] - q->sl[i], d2 = (double)a[b].sr[i] - q->sr[i];
            e += d0 * d0 + d1 * d1 + d2 * d2;
            if (d0 != 0 || d1 != 0 || d2 != 0) {
                if (!*nz) fd_first = b;
                fd_last = b;
                (*nz)++;
            }
        }
    }
    return e;
}
#define EXACT 0
#define SHIFT 1
#define NEAR 2
#define TAIL 3                                    /* EXACT but for the gate's fade at the end of the tail (fx_gate runs on the bus's clock) */
static void fd_case(const char *dir, const char *name, uint32_t bus, int kind, double bound_db, void (*set)(void), void (*mid)(uint32_t))
{
    char path[512];
    FILE *f;
    fd_run(name, bus, set, mid);
#if MELODEE_FX_DEFER == 0
    snprintf(path, sizeof path, "%s/fxd_%s.bin", dir, name);
    f = fopen(path, "wb");
    assert(f && fwrite(rec, sizeof rec[0], NB, f) == NB);
    fclose(f);
    printf("  %-34s reference written\n", name);
    (void)kind; (void)bound_db; (void)ref;
#else
    uint32_t nz, from = kind == SHIFT ? 1u : 0u;
    double er, en;
    snprintf(path, sizeof path, "%s/fxd_%s.bin", dir, name);
    f = fopen(path, "rb");
    assert(f && fread(ref, sizeof ref[0], NB, f) == NB);
    fclose(f);
    en = fd_energy(ref, 0, NB);
    er = fd_err(rec, ref, kind == SHIFT, from, NB, &nz);
    if (kind == SHIFT) {
        for (uint32_t i = 0; i < CTL; i++) assert(!rec[0].wet[i] && !rec[0].sl[i] && !rec[0].sr[i]);
    }
    double db = er > 0 ? 10.0 * log10(er / en) : -999.0;
    int ok = en > 0 && (kind == NEAR ? db <= bound_db : kind == TAIL ? (nz == 0 || (db <= -85.0 && fd_first >= 3000u)) : nz == 0);
    printf("  %-34s %s: reference energy %.3g, error %s%.1f dB, %u samples differ (blocks %u .. %u)  %s\n", name, kind == EXACT ? "EXACT" : kind == SHIFT ? "SHIFT" : kind == TAIL ? "TAIL " : "NEAR ",
           en, er > 0 ? "" : "none ", er > 0 ? db : 0.0, nz, fd_first, fd_last, ok ? "ok" : "FAIL");
    bad += !ok;
#endif
}
/* the settings */
static void s_delay(void) { song.g[G_DTYPE] = 0; song.g[G_DTIME] = 1; song.g[G_DFDBK] = 55; song.g[G_DMIX] = 127; }
static void s_delay_wide(void) { s_delay(); song.g[G_WIDTH] = 90; }
static void s_delay_ping(void) { s_delay(); song.g[G_DPING] = 1; }
static void s_delay_long(void) { s_delay(); song.g[G_DTIME] = 7; song.g[G_DFDBK] = 40; }       /* 1/1: a quarter rate */
static void s_delay_long_wide(void) { s_delay_long(); song.g[G_WIDTH] = 90; }
static void s_delay_half(void) { s_delay(); song.g[G_DTIME] = 6; }                           /* 1/2: 44,100 samples, a half rate */
static void s_delay_mod(void) { s_delay(); song.g[G_DMOD] = 60; }
static void s_delay_grain(void) { s_delay(); song.g[G_DPIT] = 7; song.g[G_DSPRY] = 30; }
static void s_delay_digital(void) { s_delay(); song.g[G_DTYPE] = 1; song.g[G_DCOLOR] = 90; }
static void s_chorus0(void) { song.g[G_CDEPTH] = 0; song.g[G_CRATE] = 30; }
static void s_chorus(void) { song.g[G_CDEPTH] = 60; song.g[G_CRATE] = 40; }
static void s_rev0(void) { song.g[G_RTYPE] = 0; }
static void s_rev1(void) { song.g[G_RTYPE] = 1; song.g[G_RPRE] = 40; }
static void s_rev2(void) { song.g[G_RTYPE] = 2; }
static void s_rev3(void) { song.g[G_RTYPE] = 3; }
static void s_rev4(void) { song.g[G_RTYPE] = 4; }
static void m_time(uint32_t b) { if (b == 1503u) song.g[G_DTIME] = 2; }                   /* TIME 1/8 -> 1/16 while the tail rings: a crossfade */
static void s_delay_xf(void) { s_delay(); }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/host";
    printf("fx_defer: MELODEE_FX_DEFER=%d\n", MELODEE_FX_DEFER);
    fd_case(dir, "reverb_room", 3, SHIFT, 0, s_rev0, 0);
    fd_case(dir, "reverb_room_pre", 3, SHIFT, 0, s_rev1, 0);
    fd_case(dir, "reverb_spring", 3, SHIFT, 0, s_rev2, 0);
    fd_case(dir, "reverb_shimmer", 3, SHIFT, 0, s_rev3, 0);
    fd_case(dir, "reverb_cloud", 3, SHIFT, 0, s_rev4, 0);
    fd_case(dir, "delay_plain", 2, TAIL, 0, s_delay, 0);
    fd_case(dir, "delay_digital", 2, TAIL, 0, s_delay_digital, 0);
    fd_case(dir, "delay_wide", 2, TAIL, 0, s_delay_wide, 0);
    fd_case(dir, "delay_ping", 2, TAIL, 0, s_delay_ping, 0);
    fd_case(dir, "delay_half_rate", 2, TAIL, 0, s_delay_half, 0);
    fd_case(dir, "delay_quarter_rate", 2, TAIL, 0, s_delay_long, 0);
    fd_case(dir, "delay_quarter_rate_wide", 2, TAIL, 0, s_delay_long_wide, 0);
    fd_case(dir, "delay_mod", 2, NEAR, -38.0, s_delay_mod, 0);
    fd_case(dir, "delay_grain", 2, SHIFT, 0, s_delay_grain, 0);
    fd_case(dir, "chorus_depth0", 1, EXACT, 0, s_chorus0, 0);
    fd_case(dir, "chorus_sweep", 1, EXACT, 0, s_chorus, 0);
    fd_case(dir, "delay_time_change", 2, NEAR, -20.0, s_delay_xf, m_time);
#if MELODEE_FX_DEFER
    {   /* a reset: the next master has no returns of the old effects, and a second drain changes nothing */
        static int32_t x[CTL];
        uint32_t b, i, loud = 0;
        fd_fresh();
        s_rev0();
        trk[0].p[P_REV] = 100;
        for (b = 0; b < 8u; b++) fd_block(0, 0, 0);
        for (b = 0; b < 200u; b++) {
            for (i = 0; i < CTL; i++) x[i] = (int32_t)((int64_t)fd_noise() * 9000 >> 15);
            fd_block(0, 0, x);
        }
        for (i = 0; i < CTL; i++) loud |= fx_ret[fx_rr].wet[i] != 0;
        jfx_clear_all();
        for (i = 0; i < CTL; i++) x[i] = 0;
        fd_block(0, 0, 0);
        {
            uint32_t any = 0;
            for (i = 0; i < CTL; i++) any |= fx_ret[fx_rr].wet[i] || fx_ret[fx_rr].sl[i] || fx_ret[fx_rr].sr[i];
            int ok = loud && !any;
            printf("  reset: returns before it %s, none in the next master %s\n", loud ? "sounding" : "SILENT", any ? "FAIL" : "ok");
            bad += !ok;
        }
        jfx_bus_drain();
        jfx_bus_drain();
    }
#endif
    printf("fx_defer: %s\n", bad ? "FAILED" : "ok");
    return bad != 0;
}
