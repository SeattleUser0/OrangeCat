/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 2) DUB TAPE, the per-track insert (docs/orangecat/BUILD.md, "DUB TAPE as the per-track insert", design 3). Included by mix.c after jfx.c.
 *
 * Each of the four tracks can hold an instance of the kernel (dubdelay.c, LITE). The track's mono x, after DIST, mute, solo and LEVEL and before PAN, is captured
 * where the sends are taken (mix.c mix_send, into dub_x[dub_cur][track]); the instance's job, the first descriptor of the voice batch, runs on the PREVIOUS
 * block's capture (dub_x[dub_cur ^ 1]) and its wet is added into this block's dry mix and, in mono, through the track's own sends (mix.c dub_add_wet).
 * The memory (jfx.c jfx_dub_mem) is first come, first served: an instance whose memory is not all there plays dry and the page says DUB FULL.
 * A change of LEN, SPRP or the rate stage never cuts: the wet is ramped out (the kernel's cut, 3 ms), the memory changes (or the line is cleared a block a
 * block), and the instance starts again from silence with its input ramped in. */
#if MELODEE_DUB
#ifndef OC_DUB_SRAM
#define OC_DUB_SRAM 0
#endif
#if OC_DUB_SRAM                                         /* (round 3, a measurement build only: DUB's code in SRAM instead of flash, for the cost of one against the other) */
#pragma push_macro("JFX_XIP")
#undef JFX_XIP
#define JFX_XIP __attribute__((noinline))
#endif
#ifndef OC_DUB_KSRAM
#define OC_DUB_KSRAM 0
#endif
#if OC_DUB_KSRAM                                        /* (round 4) only DUB's per-block kernel in SRAM (dub_run, dd_run_lite, dd_sp_run); the control path stays in flash */
#define DUB_HOT __attribute__((noinline))
#else
#define DUB_HOT JFX_XIP
#endif
#define DD_NOINL JFX_XIP
#define DD_HOT DUB_HOT
#include "dubdelay.c"
#undef DD_NOINL
#undef DD_HOT
_Static_assert(DD_BLOCK == CTL, "a job a block");

#define DUB_SCALE 524288.0f                             /* the mix domain: 1.0 = 524,288 (x is clamped to +-524,287; the master shifts by 4 to Q15) */
#define DUB_HOLD 24u                                    /* blocks past the farthest head that an instance keeps running after the last audible input or wet */
#define DUB_THR 132                                     /* -72 dB of the mix domain */
#define DUB_RAMP 6u                                     /* blocks of cut before a change (the kernel's ramp is 3 ms = 4.1 blocks) */
#define DUB_FLOOR 4u                                    /* blocks: the smallest line (round 13: the line is sized from the times, a floor of 16 KB) */
#define DUB_MAXB 16u                                    /* DD_MAXBLK */
#ifndef DUB_IDLE
#define DUB_IDLE 11000u                                 /* blocks (8 s) an instance stays after its ECHO went to 0: muted, memory kept, so ECHO back up has the repeats still there */
#endif
enum { DUBS_OFF, DUBS_WAIT, DUBS_RUN, DUBS_RAMP, DUBS_CLEAR };
typedef struct {
    uint8_t state, R, ramp, clr, run, kick_prev, was_on, pend_R, evict, held_off, gov;   /* evict: ramp out and let go (a bus needs the memory, the CPU is short); held_off: not asking again (see dub_held_off) */
    uint8_t gov_n;                                      /* governor releases in a row: the wait doubles (8, 16, 32 s); cleared by a setting change or 47 s of running */
    uint16_t run_ok;
    uint16_t gov_wait;                                  /* blocks before a governor release may end (8 s), and then only after DUB_QUIET blocks without a late half */
    uint16_t rdeb;                                      /* blocks the rate stage has wanted to change: it changes after 344 (a tempo still settling must not clear the line) */
    uint8_t pvalid;                                     /* the job's parameter copy (dub_p[k]) is current for psig */
    uint32_t psig, rsig;                                /* a hash of what the parameter copy / the rate stage were made from: unchanged, not made again */
    float rneed;                                        /* the farthest head's samples at the half rate, from rsig */
    uint8_t gov_sig;                                    /* the (blocks, spring) a governor eviction was made for: a different setting asks again */
    uint16_t idle;                                      /* blocks ECHO has been 0 while the instance stayed */
    uint8_t st_R, st_blk;                               /* the sticky size: the lowest rate and the most blocks this instance has needed (turning TIME down never shrinks the line) */
    uint16_t hold, hold_len;                            /* blocks the instance runs and its track stays mixed past the last audible input or wet; the refresh (the farthest head and 24) */
    uint32_t crash;
} dub_i_t;
static __attribute__((section(".pool"))) dub_i_t dub_i[DUB_N];
#define dub_p (dub_cm->p)                              /* the job's parameter copies, the capture pair, the wet and the batch's sink: the instances' common arena block (jfx.c dub_common_t) */
#define dub_x (dub_cm->x)
#define dub_wet (dub_cm->wet)
#define dub_sink (dub_cm->sink)
static __attribute__((section(".pool"))) uint8_t dub_cur, dub_job_buf, dub_mask, dub_done;
static __attribute__((section(".pool"))) uint8_t dub_gov_req;                             /* audio.c: late halves while an instance ran: the governor asks for one to give way */
static __attribute__((section(".pool"))) uint8_t dub_force_degrade;                       /* a host test's CPU1 retirement */
static __attribute__((section(".pool"))) uint16_t dub_cpu_wait;                             /* blocks still to wait before DUB is admitted (CPU1 retired or forced off sets it to DUB_CPU_OK_BLOCKS; it counts down while CPU1 is online; 0 at boot: .pool is zero) */
#define DUB_CPU_OK_BLOCKS 2750u                         /* 2 s */

static inline dd_t *dub_dd(uint32_t k) { return (dd_t *)dub_state[k]; }
/* the capture buffer track k writes this block, or 0 (no instance running) */
static inline int32_t *dub_capture(uint32_t k)
{
    return k < DUB_N && (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP) ? dub_x[dub_cur][k] : 0;
}
static inline int dub_alive(uint32_t k) { return k < DUB_N && dub_i[k].hold && (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP); }

/* what a governor eviction was made for: the user's settings (the times, the mode, the speed, the tempo), not the sizes, which fall back when the instance ends */
static inline uint8_t dub_gov_key(const dub_i_t *d) { return (uint8_t)(d->rsig ^ d->rsig >> 8 ^ d->rsig >> 16 ^ d->rsig >> 24); }
/* the job: every instance flagged in dub_mask, on the previous block's capture */
DUB_HOT static void dub_run(void)
{
    uint32_t k, i;
    for (k = 0; k < DUB_N; k++) {
        float in[DD_BLOCK];
        if (!((dub_mask >> k) & 1u)) continue;
        for (i = 0; i < DD_BLOCK; i++) in[i] = (float)dub_x[dub_job_buf][k][i] * (1.0f / DUB_SCALE);
        dk_process(dub_dd(k), &dub_p[k], in, dub_wet[k][0], dub_wet[k][1]);
    }
}
static void dub_kernel(const void *data, int32_t *pcm) { (void)data; (void)pcm; dub_run(); }
static void dub_merge(const void *data, const int32_t *pcm, uint32_t n) { (void)data; (void)pcm; (void)n; dub_done = 1; }

/* the two taps' delays in seconds at normal speed (round 13: TIME L and TIME R, on the beat from the tempo in use, or free in milliseconds) */
JFX_XIP static void dub_times(const track_t *t, float *tl, float *tr)
{
    float beat = (float)beat_samples() * (1.0f / (float)FS);
    if (dub_get_sync(t->p)) { *tl = dub_sync_beats[dub_t_idx(t->p[P_DUB_TIML])] * beat; *tr = dub_sync_beats[dub_t_idx(t->p[P_DUB_TIMR])] * beat; }
    else { *tl = (float)dub_free_ms10(t->p[P_DUB_TIML]) * 1e-4f; *tr = (float)dub_free_ms10(t->p[P_DUB_TIMR]) * 1e-4f; }
}
/* track t's parameters for its instance: the ten ids (dub_fields.h) and the triggers */
JFX_XIP static void dub_map(uint32_t k, const track_t *t, dd_params_t *p)
{
    int32_t filt = clamp(t->p[P_DUB_FILT], 0, 127), thr = dub_trig[k & 3u][0], echo = clamp(t->p[P_DUB_ECHO], 0, 127);
    dub_i_t *d = &dub_i[k];
    memset(p, 0, sizeof *p);
    dub_times(t, &p->time_l, &p->time_r);
    p->ping = dub_get_ping(t->p);
    p->speed = ddm_exp((float)(clamp(t->p[P_DUB_SPD], 0, 127) - DUB_SPD_MID) * (0.69314718f / (float)DUB_SPD_MID));   /* 2^((v - 64) / 64) */
    p->wow = (float)dub_get_wow(t->p) * (1.0f / (float)(DUB_WOW_N - 1));
    p->feedback = (float)clamp(t->p[P_DUB_FDBK], 0, 127) * 0.01f;
    p->send = (float)(thr ? thr : clamp(t->p[P_DUB_SEND], 0, 127)) * (1.0f / 127.0f);   /* a throw replaces SEND */
    if (filt < 64) { p->lp = 30.0f + 1.4f * (float)filt; p->hp = 15.0f; }                  /* one sweep: low-pass down .. */
    else { p->lp = 119.0f; p->hp = 15.0f + 1.6f * (float)(filt - 64); }                    /* .. high-pass up */
    p->res = 0.3f; p->drive = (float)dub_get_drive(t->p) * (1.0f / (float)(DUB_DRIVE_N - 1)); p->wet = (float)echo * 0.01f; p->dry = 0.0f; p->glide = 0.12f; p->route = 1;
    p->spring = 2; p->sp_mix = (float)clamp(t->p[P_DUB_SPRG], 0, 127) * (1.0f / 127.0f); p->sp_decay = 2.0f; p->sp_drive = 0.4f; p->sp_tone = 0.3f; p->sp_in = 0.3f;   /* the spring on the return, always there */
    p->tap_cut = 0.0f;
    p->crash = d->crash;
    p->cut = dub_trig[k & 3u][1] >= 64 || !echo;                                        /* CUT, and ECHO 0: muted, the line kept */
    p->cut_wet = 1;
    p->lim_ceil = 0.891f;                                  /* -1 dBFS */
}
/* the farthest tap in half-rate loop samples (the line wants this and 64 more), at the slow side of SPEED up to the line's own limit */
JFX_XIP static float dub_far(const track_t *t)
{
    float tl, tr, f;
    dub_times(t, &tl, &tr);
    f = tl > tr ? tl : tr;
    {   int32_t sv = clamp(t->p[P_DUB_SPD], 0, 127);
        if (sv < DUB_SPD_MID) f *= ddm_exp((float)(DUB_SPD_MID - sv) * (0.69314718f / (float)DUB_SPD_MID)); }     /* a slower tape: the delay is longer */
    return f * ((float)DD_FS / 2.0f) + 64.0f;
}
/* the blocks the line wants at the rate R (2: 22,050 Hz, 4: 11,025 Hz) for a half-rate need */
static uint32_t dub_blocks_at(float need2, uint32_t R)
{
    float x = (R == 2u ? need2 : (need2 - 64.0f) * 0.5f + 64.0f);
    uint32_t n = (uint32_t)((x + (float)(DD_BLK_N - 1u)) * (1.0f / (float)DD_BLK_N));
    n = (n + 1u) & ~1u;                                 /* even: a line grows in steps of 8 KB */
    return n < DUB_FLOOR ? DUB_FLOOR : n;
}

/* before the tracks, every block (mix.c): the pair swapped, each instance's state advanced, the job pushed first */
#if MELODEE_DUAL_CORE && defined(FM1_IRQ_TARGET)
#if OC_CORE_STATS
#define DUB_CPU1_OK() (audio_worker_online != 0 && !cs_single)
#else
#define DUB_CPU1_OK() (audio_worker_online != 0)
#endif
#else
#define DUB_CPU1_OK() 1                                  /* (a host build: no second core to lose; tests use dub_force_degrade) */
#endif
/* audio.c, the end of every half: a late half while an instance ran. Two late halves within about a second (the score falls by 1/344 a half) and the highest running instance
 * gives way: ramped out, DUB FULL, and it stays out until its track's setting changes (or a new project): the safety net under the voice caps' third column */
static __attribute__((section(".pool"))) uint16_t dub_gov_score, dub_gov_cool, dub_late_recent;      /* dub_late_recent: blocks left of the quiet a governor release waits for, set by every late half */
#define DUB_GOV_WAIT 11000u                             /* 8 s */
#define DUB_QUIET 2750u                                 /* 2 s */
static __attribute__((section(".pool"))) uint32_t dub_gov_fires;                          /* governor evictions since power-on (the readout) */
#ifndef OC_DUB_NOGOV
#define OC_DUB_NOGOV 0
#endif
JFX_XIP static void dub_late_half(int late)
{
    uint32_t k, running = 0;
#if OC_DUB_NOGOV                                        /* (round 5, a measurement build only: the governor never releases an instance, so a load over the budget can be timed with DUB running) */
    (void)late; (void)k; (void)running;
    return;
#endif
    if (late) dub_late_recent = DUB_QUIET;
    for (k = 0; k < DUB_N; k++) running |= dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP;
    if (!running) {                                     /* a late half with no instance running is no business of DUB's: the score and the cool-down start over */
        dub_gov_score = dub_gov_cool = 0;
        return;
    }
    dub_gov_score = (uint16_t)(dub_gov_score - (dub_gov_score + 343u) / 344u);        /* (rounded up: a lone late half does not stay for good) */
    if (late && dub_gov_score < 60000u) dub_gov_score = (uint16_t)(dub_gov_score + 256u);
    if (dub_gov_cool) dub_gov_cool--;
    else if (dub_gov_score >= 480u) {
        dub_gov_req = 1;
        dub_gov_score = 0;
        dub_gov_cool = 172u;                            /* half a second before it judges again */
        dub_gov_fires++;
    }
}
/* an instance running: the voice caps' third column (voice.c vc_limit) */
static int dub_running(void) { return dub_mask != 0; }

/* before the tracks, every block (mix.c): the pair swapped, each instance's state advanced, the job pushed first */
JFX_XIP static void dub_start(uint32_t nt)
{
    uint32_t k, i, cost = 0;
    int degrade;
    dub_job_buf = dub_cur;
    dub_cur ^= 1u;
    if (dub_cm) memset(dub_x[dub_cur], 0, sizeof dub_x[dub_cur]);
    dub_mask = 0;
    dub_done = 0;
    dub_ev_full = 0;
    if (dub_late_recent) dub_late_recent--;
    if (dub_force_degrade || !DUB_CPU1_OK()) dub_cpu_wait = DUB_CPU_OK_BLOCKS;      /* CPU1 retired or single-core forced: the instances are released, DUB FULL shows */
    else if (dub_cpu_wait) dub_cpu_wait--;
    degrade = dub_cpu_wait != 0;
    if (dub_gov_req) {                                  /* the governor: the highest running instance gives way */
        dub_gov_req = 0;
        for (k = DUB_N; k-- > 0;)
            if (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP) {
                dub_i[k].evict = 1;
                dub_i[k].gov = 1;
                break;
            }
    }
    for (k = 0; k < DUB_N; k++) {
        dub_i_t *d = &dub_i[k];
        const track_t *t = &trk[k];
        uint32_t blocks = 0u, sp, R = 2u;
        float need = 0.0f;
        int on = k < nt && t->p[P_DUB_ECHO] != 0, deb = 0, live = d->state == DUBS_RUN || d->state == DUBS_RAMP;
        d->run = 0;
        if (on) d->idle = 0;
        else if (k < nt && (d->state == DUBS_RUN || d->state == DUBS_RAMP) && d->idle < DUB_IDLE) { on = 1; d->idle++; }   /* ECHO 0: muted, the instance and its memory stay a while */
        if (!live) { d->st_R = 0; d->st_blk = 0; d->idle = 0; }           /* (the size is sticky only while the line runs: one that is waiting for memory asks for what the times need now) */
        if (on) {                                           /* the size from the times: the rate stage (half rate while the farthest tap fits 16 blocks, quarter rate when it does not) and the blocks, sticky */
            uint32_t rs = (uint32_t)(t->p[P_DUB_TIML] & 127) * 0x9E3779B1u ^ (uint32_t)(t->p[P_DUB_TIMR] & 127) * 0x85EBCA6Bu ^ (uint32_t)(t->p[P_DUB_MODE] & 1) * 0xC2B2AE35u ^ (uint32_t)(t->p[P_DUB_SPD] & 127) * 0x165667B1u ^ beat_samples() * 0x27D4EB2Fu ^ 1u;
            if (rs != d->rsig) { d->rneed = dub_far(t); d->rsig = rs; }       /* (made again only when a time, the mode, the speed or the tempo changed) */
            need = d->rneed;
            if (!d->st_R) d->st_R = 2u;
            if (dub_blocks_at(need, 2u) > DUB_MAXB) R = 4u;
            if (d->st_R == 4u) R = 4u;
            if (d->state == DUBS_RUN && R != d->R) { if (d->rdeb < 344u) { d->rdeb++; R = d->R; deb = 1; } } else d->rdeb = 0;
            if (R > d->st_R) { d->st_R = (uint8_t)R; d->st_blk = 0; }
            blocks = dub_blocks_at(need, d->st_R);
            if (blocks > DUB_MAXB) blocks = DUB_MAXB;
            if (deb || blocks < d->st_blk) blocks = d->st_blk;    /* (a rate change still being waited for: the line as it is; the sticky size) */
            if ((d->state == DUBS_RUN || d->state == DUBS_RAMP) && blocks > dub_have[k] && dub_pool_room(k) < blocks) blocks = dub_have[k] ? dub_have[k] : blocks;   /* the pool cannot grow it: the echo that is running stays as it is (its taps clamp to the line) */
            d->st_blk = (uint8_t)blocks;
            R = d->st_R;
            {   float farsec = (need - 64.0f) / ((float)DD_FS / 2.0f), lim = ((float)(blocks << DD_BLK_SH) - 48.0f) / ((float)DD_FS / (float)R);   /* seconds to the farthest tap and the line's limit */
                if (farsec > lim) farsec = lim;
                d->hold_len = (uint16_t)(farsec * ((float)DD_FS / (float)CTL) + (float)DUB_HOLD); }
        }
        sp = blocks ? 1u : 0u;                              /* the spring's memory is on every running instance (SPRING never cuts the echo to attach it) */
        if (dub_evict & (1u << k)) {                    /* a bus needs the memory (jfx.c jfx_dub_mem): ramp out and let go */
            dub_evict &= (uint8_t)~(1u << k);
            if (d->state == DUBS_RUN || d->state == DUBS_RAMP) d->evict = 1;
            else if (d->state == DUBS_WAIT || d->state == DUBS_CLEAR) { dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF; }
        }
        if (degrade && (d->state == DUBS_RUN || d->state == DUBS_RAMP)) { d->evict = 1; d->gov = 0; }
        else if (degrade && (d->state == DUBS_WAIT || d->state == DUBS_CLEAR)) { dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF; d->held_off = 1; d->gov = 0; }
        {   int rise = dub_trig[k & 3u][2] >= 64;
            if (rise && !d->kick_prev) d->crash++;
            d->kick_prev = (uint8_t)rise; }
        switch (d->state) {
        case DUBS_OFF:
            if (blocks) {
                if (d->held_off && d->gov && dub_gov_key(d) != d->gov_sig) { d->gov = 0; d->gov_n = 0; }      /* the setting changed: it asks again, from the first wait */
                if (d->held_off && d->gov) {                                                              /* a governor release is not for good: after 8 s, and 2 s without a late half, it asks again (a transient is forgiven, a lasting overload is not) */
                    if (d->gov_wait) d->gov_wait--;
                    if (!d->gov_wait && !dub_late_recent) d->gov = 0;
                }
                if (d->held_off && !d->gov && !degrade) d->held_off = 0;
                else if (!d->held_off && degrade) d->held_off = 1;
                if (d->held_off) { dub_ev_full |= (uint8_t)(JFX_FULL_DUB0 << k); break; }               /* CPU short: dry, DUB FULL */
                dub_want[k] = (uint8_t)blocks; dub_spring[k] = (uint8_t)sp; d->R = (uint8_t)R; d->state = DUBS_WAIT;
            } else d->held_off = d->gov = d->gov_n = 0;
            break;
        case DUBS_WAIT:                                       /* the memory not all there: dry */
            if (!blocks) { dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF; break; }
            dub_want[k] = (uint8_t)blocks; dub_spring[k] = (uint8_t)sp; d->R = (uint8_t)R;
            if (dub_ready[k] && !dub_fresh[k]) { d->pend_R = d->R; d->clr = 0; d->state = DUBS_CLEAR; break; }   /* the memory is there but was in use (it is not fresh): the line cleared, then started */
            if (dub_ready[k] && dub_fresh[k]) {
                dd_t *dd = dub_dd(k);
                dk_init(dd, (int)d->R, dub_line[k], dub_have[k]);
                if (dub_spbuf[k]) dk_spring_attach(dd, dub_spbuf[k]);
                dd->cut_g = dd->cutw_g = 0.0f;              /* the input ramped in */
                memset(dub_wet[k], 0, sizeof dub_wet[k]);
                memset(dub_x[0][k], 0, sizeof dub_x[0][k]); memset(dub_x[1][k], 0, sizeof dub_x[1][k]);
                dub_fresh[k] = 0;
                d->hold = 0;
                d->pvalid = 0;
                d->state = DUBS_RUN;
            }
            break;
        case DUBS_RUN:
            if (!dub_ready[k]) { d->state = DUBS_WAIT; break; }          /* the memory was taken back (the card, an engine, a project load) */
            if (d->evict) { d->state = DUBS_RAMP; d->ramp = DUB_RAMP; d->pend_R = 0; }
            else if (blocks != dub_want[k] || sp != dub_spring[k]) { d->state = DUBS_RAMP; d->ramp = DUB_RAMP; d->pend_R = 0; }
            else if (R != d->R) { d->state = DUBS_RAMP; d->ramp = DUB_RAMP; d->pend_R = (uint8_t)R; }
            break;
        case DUBS_RAMP:
            if (!dub_ready[k]) { d->state = DUBS_WAIT; d->evict = 0; d->gov = 0; break; }
            if (d->ramp) d->ramp--;
            if (!d->ramp) {
                if (d->evict) {                              /* let go: the memory is freed (jfx_dub_mem), the instance waits as asked */
                    d->evict = 0;
                    dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF;
                    d->held_off = (uint8_t)(degrade || d->gov);
                    d->gov_sig = dub_gov_key(d);
                    if (d->gov) {
                        d->gov_wait = (uint16_t)(DUB_GOV_WAIT << (d->gov_n > 2 ? 2 : d->gov_n));
                        if (d->gov_n < 3) d->gov_n++;
                        d->run_ok = 0;
                    }
                }
                else if (d->pend_R && (uint8_t)R == d->pend_R && blocks == dub_want[k] && sp == dub_spring[k]) { d->state = DUBS_CLEAR; d->clr = 0; }   /* a new rate: the line cleared a block a block, then started again */
                else if (blocks == dub_want[k] && sp == dub_spring[k] && (uint8_t)R == d->R) d->state = DUBS_RUN;   /* the setting came back during the ramp: nothing changed, the wet ramps in again */
                else if (blocks == dub_want[k] && sp == dub_spring[k]) { d->pend_R = (uint8_t)R; d->state = DUBS_CLEAR; d->clr = 0; }
                else { dub_want[k] = (uint8_t)blocks; dub_spring[k] = (uint8_t)sp; d->state = blocks ? DUBS_WAIT : DUBS_OFF; d->R = (uint8_t)R; }
            }
            break;
        case DUBS_CLEAR:
            if (!dub_ready[k]) { d->state = DUBS_WAIT; break; }
            if (d->clr < dub_have[k]) memset(dub_line[k][d->clr++], 0, DD_BLK_N * sizeof(int16_t));   /* a block a block (4 KB), bounded */
            else {
                dd_t *dd = dub_dd(k);
                d->R = d->pend_R;
                dk_init(dd, (int)d->R, dub_line[k], dub_have[k]);
                if (dub_spbuf[k]) dk_spring_attach(dd, dub_spbuf[k]);
                dd->cut_g = dd->cutw_g = 0.0f;
                memset(dub_wet[k], 0, sizeof dub_wet[k]);
                d->pvalid = 0;
                d->state = DUBS_RUN;
            }
            break;
        }
        if (d->state == DUBS_RUN && d->gov_n && ++d->run_ok > 60000u) d->gov_n = 0;     /* 47 s running without a governor release: the next one starts from the first wait */
        if (d->state == DUBS_RUN || d->state == DUBS_RAMP) {
            int32_t pk = 0;
            const int32_t *x = dub_x[dub_job_buf][k];
            for (i = 0; i < CTL; i++) { int32_t a = x[i] < 0 ? -x[i] : x[i]; if (a > pk) pk = a; }
            if (pk > DUB_THR) d->hold = d->hold_len;
            if (d->hold) {
                dd_params_t *p = &dub_p[k];
                uint32_t ps = ((uint32_t)(t->p[P_DUB_MODE] & 3) | (uint32_t)d->R << 2 | (uint32_t)dub_have[k] << 5 | (uint32_t)(d->state == DUBS_RAMP) << 10 | (uint32_t)(dub_trig[k][1] >= 64) << 11 | (uint32_t)(t->p[P_DUB_ECHO] == 0) << 12 | (uint32_t)(t->p[P_DUB_TAPE] + 64) << 13) * 0x9E3779B1u
                              ^ ((uint32_t)t->p[P_DUB_FILT] | (uint32_t)t->p[P_DUB_ECHO] << 8 | (uint32_t)t->p[P_DUB_FDBK] << 16 | (uint32_t)t->p[P_DUB_SEND] << 24) * 0x85EBCA6Bu
                              ^ ((uint32_t)t->p[P_DUB_TIML] | (uint32_t)t->p[P_DUB_TIMR] << 8 | (uint32_t)t->p[P_DUB_SPRG] << 16 | (uint32_t)(t->p[P_DUB_SPD] & 127) << 24) * 0x27D4EB2Fu
                              ^ ((uint32_t)dub_trig[k][0] | d->crash << 8) * 0xC2B2AE35u ^ beat_samples() * 0x165667B1u;
                if (!d->pvalid || ps != d->psig) {          /* (the parameter copy is made again only when something it is made from changed) */
                    dub_map(k, t, p);
                    p->crash = d->crash;
                    if (d->state == DUBS_RAMP) p->cut = 1;
                    d->psig = ps; d->pvalid = 1;
                }
                d->run = 1;
                dub_mask |= (uint8_t)(1u << k);
                cost += audio_batch_cost[AP_DUB] + (dub_spbuf[k] && t->p[P_DUB_SPRG] > 0 ? 800u : 0u) + (dub_get_wow(t->p) ? 400u : 0u);
            }
        }
    }
    if (!dub_mask) return;
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    if (audio_batch_active) {
        uint32_t unused = 0;
        audio_batch_push_cost(dub_kernel, &unused, sizeof unused, dub_sink, CTL, AP_DUB, cost, dub_merge);
        return;
    }
#endif
    dub_run();
    dub_done = 1;
}
/* the instances from silence, their memory given back (a new project, power-on); the next block asks again for each track's LEN */
JFX_XIP static void dub_reset(void)
{
    if (dub_cm) { memset(dub_x, 0, sizeof dub_x); memset(dub_wet, 0, sizeof dub_wet); }
    memset(dub_i, 0, sizeof dub_i);
    memset(dub_want, 0, sizeof dub_want);
    memset(dub_spring, 0, sizeof dub_spring);
    dub_evict = dub_ev_full = dub_gov_req = 0;
    dub_gov_score = dub_gov_cool = 0;
    memset(dub_trig, 0, sizeof dub_trig);                 /* (the performance triggers, RAM only, start released) */
}
static void dub_tables(void) {}
#if OC_DUB_SRAM
#pragma pop_macro("JFX_XIP")
#endif
#else
static void dub_tables(void) {}
static inline void dub_late_half(int late) { (void)late; }
static inline int dub_running(void) { return 0; }
static inline int32_t *dub_capture(uint32_t k) { (void)k; return 0; }
static inline int dub_alive(uint32_t k) { (void)k; return 0; }
static inline void dub_start(uint32_t nt) { (void)nt; }
static void dub_reset(void) {}
#endif
