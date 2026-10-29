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
#define DD_NOINL JFX_XIP
#include "dubdelay.c"
#undef DD_NOINL
_Static_assert(DD_BLOCK == CTL, "a job a block");

#define DUB_SCALE 524288.0f                             /* the mix domain: 1.0 = 524,288 (x is clamped to +-524,287; the master shifts by 4 to Q15) */
#define DUB_HOLD 24u                                    /* blocks past the farthest head that an instance keeps running after the last audible input or wet */
#define DUB_THR 132                                     /* -72 dB of the mix domain */
#define DUB_RAMP 6u                                     /* blocks of cut before a change (the kernel's ramp is 3 ms = 4.1 blocks) */
static const uint8_t dub_len_blocks[7] = {0, 2, 4, 6, 8, 12, 16};   /* P_DUBLEN: OFF 8K 16K 24K 32K 48K 64K, in blocks of 4 KB */
/* the 14 synced divisions of SPACE's first half, in beats (1/32 .. 1/1) */
static const float dub_div_beats[14] = {0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 4.0f / 3.0f, 1.5f, 2.0f, 3.0f, 4.0f};
typedef struct { uint8_t n; float h[DD_TAPS], lv[DD_TAPS], pan[DD_TAPS], fb[DD_TAPS], m; } dub_ptrn_t;
static const dub_ptrn_t dub_ptrn[6] = {                 /* heads in steps of TIME; m: the farthest */
    {2, {1, 3, 0, 0}, {.75f, .5f, 0, 0}, {.3f, -.4f, 0, 0}, {.2f, .8f, 0, 0}, 3.0f},              /* SLAP */
    {1, {1, 0, 0, 0}, {.9f, 0, 0, 0}, {0, 0, 0, 0}, {1, 0, 0, 0}, 1.0f},                          /* SINGLE */
    {2, {1, 1.5f, 0, 0}, {.8f, .5f, 0, 0}, {-.3f, .3f, 0, 0}, {.6f, .4f, 0, 0}, 1.5f},            /* SKANK */
    {3, {1, 2, 3, 0}, {.7f, .55f, .4f, 0}, {.3f, -.3f, 0, 0}, {.3f, .3f, .4f, 0}, 3.0f},          /* TRIPLE */
    {2, {.75f, 1, 0, 0}, {.6f, .8f, 0, 0}, {-.4f, .4f, 0, 0}, {.3f, .7f, 0, 0}, 1.0f},            /* GALLOP */
    {4, {1, 2, 4, 6}, {.6f, .5f, .4f, .3f}, {-.6f, .6f, -.3f, .3f}, {.25f, .25f, .25f, .25f}, 6.0f}   /* SPREAD */
};
enum { DUBS_OFF, DUBS_WAIT, DUBS_RUN, DUBS_RAMP, DUBS_CLEAR };
typedef struct {
    uint8_t state, R, ramp, clr, run, kick_prev, was_on, pend_R, evict, held_off, gov;   /* evict: ramp out and let go (a bus needs the memory, the CPU is short); held_off: not asking again (see dub_held_off) */
    uint8_t gov_sig;                                    /* the (blocks, spring) a governor eviction was made for: a different setting asks again */
    uint16_t hold, hold_len;                            /* blocks the instance runs and its track stays mixed past the last audible input or wet; the refresh (the farthest head and 24) */
    uint32_t crash;
} dub_i_t;
static dub_i_t dub_i[DUB_N];
#define dub_p (dub_cm->p)                              /* the job's parameter copies, the capture pair, the wet and the batch's sink: the instances' common arena block (jfx.c dub_common_t) */
#define dub_x (dub_cm->x)
#define dub_wet (dub_cm->wet)
#define dub_sink (dub_cm->sink)
static uint8_t dub_cur, dub_job_buf, dub_mask, dub_done;
static uint8_t dub_gov_req;                             /* audio.c: late halves while an instance ran: the governor asks for one to give way */
static uint8_t dub_force_degrade;                       /* a host test's CPU1 retirement */
static uint16_t dub_cpu_ok;                             /* blocks CPU1 has been online and not forced off: DUB is admitted again after DUB_CPU_OK_BLOCKS */
#define DUB_CPU_OK_BLOCKS 2750u                         /* 2 s */

static inline dd_t *dub_dd(uint32_t k) { return (dd_t *)dub_state[k]; }
/* the capture buffer track k writes this block, or 0 (no instance running) */
static inline int32_t *dub_capture(uint32_t k)
{
    return k < DUB_N && (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP) ? dub_x[dub_cur][k] : 0;
}
static inline int dub_alive(uint32_t k) { return k < DUB_N && dub_i[k].hold && (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP); }

/* the job: every instance flagged in dub_mask, on the previous block's capture */
JFX_XIP static void dub_run(void)
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

/* track t's parameters for its instance: FDBK SEND TONE SPACE THRW CUT KICK, and the setup */
JFX_XIP static void dub_map(uint32_t k, const track_t *t, dd_params_t *p, uint32_t R, uint32_t nblk, float *need)
{
    const dub_ptrn_t *pt = &dub_ptrn[clamp(t->p[P_DUBPTRN], 0, 5)];
    int32_t space = clamp(t->p[P_DUB_SPACE], 0, 127), tone = clamp(t->p[P_DUB_TONE], 0, 127), thr = clamp(t->p[P_DUB_THRW], 0, 127);
    int32_t sp = clamp(t->p[P_DUBSPRP], 0, 2), idx = (space < 64 ? space : 63) * 14 / 64, sm = space >= 64 ? space - 64 : 0;   /* SPACE (SPEC 9.3.2): delay time on the first half of the travel (the 14 synced divisions), the spring's mix on the second (the time stays at the last division) */
    float time = dub_div_beats[idx] * (float)beat_samples() * (1.0f / (float)FS), fsl = (float)DD_FS / (float)R, lines = (float)(nblk << DD_BLK_SH), maxt;
    uint32_t j;
    dub_i_t *d = &dub_i[k];
    maxt = (lines - 48.0f) / (pt->m * fsl);              /* the farthest head must fit the line: TIME clamps at the quarter rate */
    if (need) *need = pt->m * time * fsl + 48.0f;
    if (time > maxt) time = maxt;
    memset(p, 0, sizeof *p);
    p->time = time;
    for (j = 0; j < DD_TAPS; j++) { p->head[j] = pt->h[j]; p->level[j] = pt->lv[j]; p->pan[j] = pt->pan[j]; p->fbw[j] = pt->fb[j]; }
    p->feedback = (float)clamp(t->p[P_DUB_FDBK], 0, 127) * 0.01f;
    p->send = (float)(thr ? thr : clamp(t->p[P_DUB_SEND], 0, 127)) * (1.0f / 127.0f);   /* a throw replaces SEND */
    if (tone < 64) { p->lp = 30.0f + 1.4f * (float)tone; p->hp = 15.0f; }                  /* one sweep: low-pass down .. */
    else { p->lp = 119.0f; p->hp = 15.0f + 1.6f * (float)(tone - 64); }                    /* .. high-pass up */
    p->res = 0.3f; p->drive = 0.4f; p->wet = 1.0f; p->dry = 0.0f; p->glide = 0.12f; p->route = 1;
    p->spring = (int)sp; p->sp_mix = (float)sm * (1.0f / 63.0f); p->sp_decay = 2.0f; p->sp_drive = 0.4f; p->sp_tone = 0.3f; p->sp_in = sp == 2 ? 0.3f : 0.0f;
    p->tap_cut = 0.0f;
    p->crash = d->crash;
    p->cut = clamp(t->p[P_DUB_CUT], 0, 127) >= 64;
    p->cut_wet = 1;
    p->lim_ceil = 0.891f;                                  /* -1 dBFS */
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
static uint16_t dub_gov_score, dub_gov_cool;
JFX_XIP static void dub_late_half(int late)
{
    dub_gov_score = (uint16_t)(dub_gov_score - dub_gov_score / 344u);
    if (late && dub_gov_score < 60000u) dub_gov_score = (uint16_t)(dub_gov_score + 256u);
    if (dub_gov_cool) dub_gov_cool--;
    else if (dub_gov_score >= 480u) {
        for (uint32_t k = 0; k < DUB_N; k++)
            if (dub_i[k].state == DUBS_RUN || dub_i[k].state == DUBS_RAMP) {
                dub_gov_req = 1;
                dub_gov_score = 0;
                dub_gov_cool = 172u;                    /* half a second before it judges again */
                break;
            }
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
    if (dub_force_degrade || !DUB_CPU1_OK()) dub_cpu_ok = 0;                 /* CPU1 retired or single-core forced: the instances are released, DUB FULL shows */
    else if (dub_cpu_ok < 0xFFFFu) dub_cpu_ok++;
    degrade = dub_cpu_ok < DUB_CPU_OK_BLOCKS;
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
        uint32_t blocks = k < nt ? dub_len_blocks[clamp(t->p[P_DUBLEN], 0, 6)] : 0u, sp = k < nt ? (uint32_t)(clamp(t->p[P_DUBSPRP], 0, 2) != 0) : 0u, R = 2u;
        float need = 0.0f;
        d->run = 0;
        if (blocks && !d->was_on && !t->p[P_DUB_FDBK] && !t->p[P_DUB_SEND] && !t->p[P_DUB_TONE] && !t->p[P_DUB_SPACE]) {
            trk[k].p[P_DUB_FDBK] = param_desc_of(eng_idx(t->eng_req), P_DUB_FDBK)->def;      /* OFF to a length with all four at zero (an old project's): the defaults */
            trk[k].p[P_DUB_SEND] = param_desc_of(eng_idx(t->eng_req), P_DUB_SEND)->def;
            trk[k].p[P_DUB_TONE] = param_desc_of(eng_idx(t->eng_req), P_DUB_TONE)->def;
            trk[k].p[P_DUB_SPACE] = param_desc_of(eng_idx(t->eng_req), P_DUB_SPACE)->def;
        }
        d->was_on = blocks != 0;
        if (dub_evict & (1u << k)) {                    /* a bus needs the memory (jfx.c jfx_dub_mem): ramp out and let go */
            dub_evict &= (uint8_t)~(1u << k);
            if (d->state == DUBS_RUN || d->state == DUBS_RAMP) d->evict = 1;
            else if (d->state == DUBS_WAIT || d->state == DUBS_CLEAR) { dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF; }
        }
        if (degrade && (d->state == DUBS_RUN || d->state == DUBS_RAMP)) { d->evict = 1; d->gov = 0; }
        if (blocks) {                                       /* the rate stage: the half rate while the farthest head fits, the quarter rate when it does not */
            dd_params_t tmp;
            float lines = (float)(blocks << DD_BLK_SH);
            dub_map(k, t, &tmp, 2u, blocks, &need);
            R = (d->R == 4u ? need <= 0.9f * lines : need <= lines) ? 2u : 4u;
            {   float farsec = (need - 48.0f) / ((float)DD_FS / 2.0f), lim = (lines - 48.0f) / ((float)DD_FS / (float)R);   /* seconds to the farthest head (need is at the half rate), and the line's limit */
                if (farsec > lim) farsec = lim;
                d->hold_len = (uint16_t)(farsec * ((float)DD_FS / (float)CTL) + (float)DUB_HOLD); }
        }
        {   int rise = clamp(t->p[P_DUB_KICK], 0, 127) >= 64;
            if (rise && !d->kick_prev) d->crash++;
            d->kick_prev = (uint8_t)rise; }
        switch (d->state) {
        case DUBS_OFF:
            if (blocks) {
                if (d->held_off && d->gov && (uint8_t)(blocks | sp << 5) != d->gov_sig) d->gov = 0;      /* the setting changed: it asks again */
                if (d->held_off && !d->gov && !degrade) d->held_off = 0;
                else if (!d->held_off && degrade) d->held_off = 1;
                if (d->held_off) { dub_ev_full |= (uint8_t)(JFX_FULL_DUB0 << k); break; }               /* CPU short: dry, DUB FULL */
                dub_want[k] = (uint8_t)blocks; dub_spring[k] = (uint8_t)sp; d->R = (uint8_t)R; d->state = DUBS_WAIT;
            } else d->held_off = d->gov = 0;
            break;
        case DUBS_WAIT:                                       /* the memory not all there: dry */
            if (!blocks) { dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF; break; }
            dub_want[k] = (uint8_t)blocks; dub_spring[k] = (uint8_t)sp; d->R = (uint8_t)R;
            if (dub_ready[k] && !dub_fresh[k]) { d->pend_R = d->R; d->clr = 0; d->state = DUBS_CLEAR; break; }   /* the memory is there but was in use (it is not fresh): the line cleared, then started */
            if (dub_ready[k] && dub_fresh[k]) {
                dd_t *dd = dub_dd(k);
                dk_init(dd, (int)d->R, dub_line[k], dub_have[k]);
                if (dub_spring[k]) dk_spring_attach(dd, dub_spbuf[k]);
                dd->cut_g = dd->cutw_g = 0.0f;              /* the input ramped in */
                memset(dub_wet[k], 0, sizeof dub_wet[k]);
                memset(dub_x[0][k], 0, sizeof dub_x[0][k]); memset(dub_x[1][k], 0, sizeof dub_x[1][k]);
                dub_fresh[k] = 0;
                d->hold = 0;
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
            if (!dub_ready[k]) { d->state = DUBS_WAIT; d->evict = 0; break; }
            if (d->ramp) d->ramp--;
            if (!d->ramp) {
                if (d->evict) {                              /* let go: the memory is freed (jfx_dub_mem), the instance waits as asked */
                    d->evict = 0;
                    dub_want[k] = 0; dub_spring[k] = 0; d->state = DUBS_OFF;
                    d->held_off = (uint8_t)(degrade || d->gov);
                    d->gov_sig = (uint8_t)(blocks | sp << 5);
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
                if (dub_spring[k]) dk_spring_attach(dd, dub_spbuf[k]);
                dd->cut_g = dd->cutw_g = 0.0f;
                memset(dub_wet[k], 0, sizeof dub_wet[k]);
                d->state = DUBS_RUN;
            }
            break;
        }
        if (d->state == DUBS_RUN || d->state == DUBS_RAMP) {
            int32_t pk = 0;
            const int32_t *x = dub_x[dub_job_buf][k];
            for (i = 0; i < CTL; i++) { int32_t a = x[i] < 0 ? -x[i] : x[i]; if (a > pk) pk = a; }
            if (pk > DUB_THR) d->hold = d->hold_len;
            if (d->hold) {
                dd_params_t *p = &dub_p[k];
                dub_map(k, t, p, d->R, dub_have[k], 0);
                p->crash = d->crash;
                if (d->state == DUBS_RAMP) p->cut = 1;
                d->run = 1;
                dub_mask |= (uint8_t)(1u << k);
                cost += audio_batch_cost[AP_DUB] + (dub_spring[k] ? 450u : 0u);
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
    dub_cpu_ok = DUB_CPU1_OK() && !dub_force_degrade ? 0xFFFFu : 0u;
    for (uint32_t k = 0; k < DUB_N; k++) dub_i[k].was_on = trk[k].p[P_DUBLEN] != 0;   /* (a load is no OFF-to-length edge: the defaults are not written over a saved set) */
}
static void dub_tables(void) {}
#else
static void dub_tables(void) {}
static inline void dub_late_half(int late) { (void)late; }
static inline int dub_running(void) { return 0; }
static inline int32_t *dub_capture(uint32_t k) { (void)k; return 0; }
static inline int dub_alive(uint32_t k) { (void)k; return 0; }
static inline void dub_start(uint32_t nt) { (void)nt; }
static void dub_reset(void) {}
#endif
