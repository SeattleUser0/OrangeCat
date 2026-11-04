/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 3 step 6; round 20) DUB TAPE's kernel side of the insert host's cut: the module's source (tools/module/dub_mod.c, orangecat-dub.ocm). Until round 20 it was ONE text that the image's resident kernel
 * (jfx_dub.c) and the module both included; the image has no copy any more (FX_MODULES_PLAN.md 15.9), and the host test that proves the module's render is the resident kernel's of d620160d is tests/fx_insert_diff.sh.
 *
 * The includer defines, before this text: JFX_XIP (the placement of a control-path function: .text.xip_jfx in the image, nothing in the module), DUB_HOT (the per-block job's), the
 * types of core.h, dubdelay.h / dubdelay.c, dub_fields.h, fx_kernel.h, and AP_DUB (audio_profile.h). It imports nothing else: the tempo and the performance triggers come in fx_ictx_t, the
 * only service is memset (the module's own, through fw_api_t mem_set). What is DUB's and not the host's: the ten stored ids and where Release 2 keeps them (dub_rows: the legacy-slot table, the
 * stable ids of MODULE_FORMAT_R3.md 15.2 beside the track slots P_DUB_* of dub_fields.h); the parameter snapshot from the track's values THROUGH that table (dub_snap); the sizing (the
 * farthest tap, the rate stage's blocks, the hold); the signatures; the KICK edge; the batch cost's extras. */
#define DUB_HOLD 24u                                    /* blocks past the farthest head that an instance keeps running after the last audible input or wet */
#define DUB_FLOOR 4u                                    /* blocks: the smallest line (round 13: the line is sized from the times, a floor of 16 KB) */
#define DUB_MAXB 16u                                    /* DD_MAXBLK */
#define DUB_STATE_BYTES ((uint32_t)((sizeof(dd_t) + 3u) & ~3u))   /* dd_t on the target (the review's 32-bit size, not the host's), rounded */
#define DUB_SPRING_BYTES 5632u                          /* the spring's rings (DD_SP_BUF int16) */
/* the legacy-slot table: DUB TAPE's ten stored parameters, each with its stable id (MODULE_FORMAT_R3.md 15.2) and the track slot Release 2 stores it in (dub_fields.h). A Release 2 project's
 * values are read through `legacy`, whatever else a later format does with the ids; dub_convert_old (dub_fields.h) is unchanged and makes today's layout from the layout of before */
enum { DUBP_FDBK, DUBP_SEND, DUBP_FILT, DUBP_ECHO, DUBP_TIML, DUBP_TIMR, DUBP_SPRG, DUBP_SPD, DUBP_MODE, DUBP_TAPE, DUBP_N };
static const fx_prow_t dub_rows[DUBP_N] = {
    [DUBP_FDBK] = {1, P_DUB_FDBK, 0, 127, 55},   [DUBP_SEND] = {2, P_DUB_SEND, 0, 127, 127}, [DUBP_FILT] = {3, P_DUB_FILT, 0, 127, 60}, [DUBP_ECHO] = {4, P_DUB_ECHO, 0, 127, 0},
    [DUBP_TIML] = {5, P_DUB_TIML, 0, 127, 53},   [DUBP_TIMR] = {6, P_DUB_TIMR, 0, 127, 67},  [DUBP_SPRG] = {7, P_DUB_SPRG, 0, 127, 0},
    [DUBP_SPD] = {8, P_DUB_SPD, 0, 127, DUB_SPD_MID},
    [DUBP_MODE] = {9, P_DUB_MODE, DUB_MODE_MARK, 127, DUB_MODE_MARK | 1}, [DUBP_TAPE] = {10, P_DUB_TAPE, -64, 127, 6 * DUB_WOW_N - 64},
};
#define DUB_V(t, ix) ((t)->p[dub_rows[ix].legacy])      /* a stored value of track t, through the table (the compiler folds it to the fixed slot for the resident kernel) */

/* the two taps' delays in seconds at normal speed (round 13: TIME L and TIME R, on the beat from the tempo in use, or free in milliseconds) */
JFX_XIP static void dub_times(const track_t *t, uint32_t beat_n, float *tl, float *tr)
{
    float beat = (float)beat_n * (1.0f / (float)FS);
    if (dub_mode_sync(DUB_V(t, DUBP_MODE))) { *tl = dub_sync_beats[dub_t_idx(DUB_V(t, DUBP_TIML))] * beat; *tr = dub_sync_beats[dub_t_idx(DUB_V(t, DUBP_TIMR))] * beat; }
    else { *tl = (float)dub_free_ms10(DUB_V(t, DUBP_TIML)) * 1e-4f; *tr = (float)dub_free_ms10(DUB_V(t, DUBP_TIMR)) * 1e-4f; }
}
/* (dub-perf) the wet of the last ECHO above 0 lives in the instance's kernel word 2 (c->kw2: float bits, kept by the host with the instance, 0 at a new project): ECHO 0 closes the loop's
 * output through the cut and leaves the wet where it was (a CUT recorded as ECHO 0 plays as CUT). The module has no RAM of its own, so it cannot be a static. */
/* fx_kernel_t.snap: track t's parameters for its instance: the ten ids through the table, and the triggers. c->ramp: the instance is ramping out (the cut) */
JFX_XIP static void dub_snap(void *vp, const track_t *t, fx_ictx_t *c)
{
    dd_params_t *p = (dd_params_t *)vp;
    union { uint32_t u; float f; } keep;
    int32_t filt = clamp(DUB_V(t, DUBP_FILT), 0, 127), thr = c->trig[0], echo = clamp(DUB_V(t, DUBP_ECHO), 0, 127);
    memset(p, 0, sizeof *p);
    dub_times(t, c->beat, &p->time_l, &p->time_r);
    p->ping = dub_mode_ping(DUB_V(t, DUBP_MODE));
    {   int32_t sv = clamp(DUB_V(t, DUBP_SPD), 0, 127);
        if (c->trig[4] >= 64) sv = c->trig[5] >= 64 ? DUB_SPD_MID : 0;      /* DIVE (dub-perf): an octave down while held; with RISE both: normal */
        else if (c->trig[5] >= 64) sv = 127;                                /* RISE: an octave up (127 is a semitone short of it) */
        p->speed = ddm_exp((float)(sv - DUB_SPD_MID) * (0.69314718f / (float)DUB_SPD_MID)); }   /* 2^((v - 64) / 64) */
    p->wow = (float)dub_tape_wow(DUB_V(t, DUBP_TAPE)) * (1.0f / (float)(DUB_WOW_N - 1));
    p->feedback = (c->trig[3] >= 64 ? 100.0f : (float)clamp(DUB_V(t, DUBP_FDBK), 0, 127)) * 0.01f;   /* HOLD (dub-perf): the loop neither builds nor decays */
    p->send = (float)(thr ? thr : clamp(DUB_V(t, DUBP_SEND), 0, 127)) * (1.0f / 127.0f);   /* a throw replaces SEND */
    if (filt < 64) { p->lp = 30.0f + 1.4f * (float)filt; p->hp = 15.0f; }                  /* one sweep: low-pass down .. */
    else { p->lp = 119.0f; p->hp = 15.0f + 1.6f * (float)(filt - 64); }                    /* .. high-pass up */
    p->res = 0.3f; p->drive = (float)dub_tape_drive(DUB_V(t, DUBP_TAPE)) * (1.0f / (float)(DUB_DRIVE_N - 1)); keep.u = c->kw2; keep.f = echo ? (float)echo * 0.01f : keep.f; c->kw2 = keep.u; p->wet = keep.f; p->dry = 0.0f; p->glide = 0.12f; p->route = 1;
    p->spring = 2; p->sp_mix = (float)clamp(DUB_V(t, DUBP_SPRG), 0, 127) * (1.0f / 127.0f); p->sp_decay = 2.0f; p->sp_drive = 0.4f; p->sp_tone = 0.3f; p->sp_in = 0.3f;   /* the spring on the return, always there */
    p->tap_cut = 0.0f;
    p->crash = c->kw0;
    p->cut = c->trig[1] >= 64 || !echo;                                    /* CUT, and ECHO 0: muted, the line kept */
    p->cut_wet = 1;
    p->lim_ceil = 0.891f;                                  /* -1 dBFS */
    if (c->ramp) p->cut = 1;                               /* a change is coming: the wet ramped out */
}
/* fx_kernel_t.param_sig: a hash of what the snapshot is made from (the host's facts in c included): unchanged, not made again */
static uint32_t dub_param_sig(const track_t *t, const fx_ictx_t *c)
{
    return ((uint32_t)(DUB_V(t, DUBP_MODE) & 3) | (uint32_t)c->R << 2 | (uint32_t)c->nblk << 5 | (uint32_t)c->ramp << 10 | (uint32_t)(c->trig[1] >= 64) << 11 | (uint32_t)(DUB_V(t, DUBP_ECHO) == 0) << 12 | (uint32_t)(DUB_V(t, DUBP_TAPE) + 64) << 13) * 0x9E3779B1u
         ^ ((uint32_t)DUB_V(t, DUBP_FILT) | (uint32_t)DUB_V(t, DUBP_ECHO) << 8 | (uint32_t)DUB_V(t, DUBP_FDBK) << 16 | (uint32_t)DUB_V(t, DUBP_SEND) << 24) * 0x85EBCA6Bu
         ^ ((uint32_t)DUB_V(t, DUBP_TIML) | (uint32_t)DUB_V(t, DUBP_TIMR) << 8 | (uint32_t)DUB_V(t, DUBP_SPRG) << 16 | (uint32_t)(DUB_V(t, DUBP_SPD) & 127) << 24) * 0x27D4EB2Fu
         ^ ((uint32_t)c->trig[0] | c->kw0 << 8 | (uint32_t)(c->trig[3] >= 64) << 16 | (uint32_t)(c->trig[4] >= 64) << 17 | (uint32_t)(c->trig[5] >= 64) << 18) * 0xC2B2AE35u ^ c->beat * 0x165667B1u;
}
static uint32_t dub_on(const track_t *t) { return DUB_V(t, DUBP_ECHO) != 0; }
/* what the sizing is made from: the times, the mode (SYNC), the speed and the tempo */
static uint32_t dub_size_sig(const track_t *t, const fx_ictx_t *c)
{
    return (uint32_t)(DUB_V(t, DUBP_TIML) & 127) * 0x9E3779B1u ^ (uint32_t)(DUB_V(t, DUBP_TIMR) & 127) * 0x85EBCA6Bu ^ (uint32_t)(DUB_V(t, DUBP_MODE) & 1) * 0xC2B2AE35u ^ (uint32_t)(DUB_V(t, DUBP_SPD) & 127) * 0x165667B1u ^ c->beat * 0x27D4EB2Fu ^ 1u;
}
/* the farthest tap in half-rate loop samples (the line wants this and 64 more), at the slow side of SPEED up to the line's own limit */
JFX_XIP static float dub_far(const track_t *t, const fx_ictx_t *c)
{
    float tl, tr, f;
    dub_times(t, c->beat, &tl, &tr);
    f = tl > tr ? tl : tr;
    {   int32_t sv = clamp(DUB_V(t, DUBP_SPD), 0, 127);
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
/* blocks the instance runs past the last audible input or wet: the seconds to the farthest tap (the line's own limit at most) and the 24 blocks */
static uint32_t dub_hold_len(float need, uint32_t blocks, uint32_t R)
{
    float farsec = (need - 64.0f) / ((float)DD_FS / 2.0f), lim = ((float)(blocks << DD_BLK_SH) - 48.0f) / ((float)DD_FS / (float)R);   /* seconds to the farthest tap and the line's limit */
    if (farsec > lim) farsec = lim;
    return (uint16_t)(farsec * ((float)DD_FS / (float)CTL) + (float)DUB_HOLD);
}
/* the KICK trigger's rising edges are counted (the tank is kicked once per new count), every block, running or not */
static void dub_tick(uint32_t *kw, const fx_ictx_t *c)
{
    int rise = c->trig[2] >= 64;
    if (rise && !kw[1]) kw[0]++;
    kw[1] = (uint32_t)rise;
}
static uint32_t dub_cost_extra(const track_t *t, uint32_t spring)
{
    return (spring && DUB_V(t, DUBP_SPRG) > 0 ? 800u : 0u) + (dub_tape_wow(DUB_V(t, DUBP_TAPE)) ? 400u : 0u);
}
/* (re)start from silence on the blocks given; the input ramped in */
static void dub_start_k(void *state, uint32_t R, int16_t *const *blk, uint32_t nblk, int16_t *extra)
{
    dd_t *dd = (dd_t *)state;
    dk_init(dd, (int)R, blk, nblk);
    if (extra) dk_spring_attach(dd, extra);
    dd->cut_g = dd->cutw_g = 0.0f;
}
DUB_HOT static void dub_process(void *state, const void *params, const float *in, float *outL, float *outR)
{
    dk_process((dd_t *)state, (const dd_params_t *)params, in, outL, outR);
}
static void dub_reset_k(void) {}                        /* (nothing of the kernel's own is in RAM: the performance triggers are the host's, fx_trig, released by fx_ins_reset) */

/* the capability words (FX_MODULES_PLAN.md 4) and the call list: one text for the resident descriptor (jfx_dub.c dub_kernel) and the module's (tools/module/dub_mod.c fx_caps_t, fx_kcalls_t) */
#define DUBK_MODULE_ID 1u
#define DUBK_VERSION (1u << 16)                         /* 0x00MMmmpp: 1.0.0 (not written as 0x00010000: the build's hardware-address check reads that as a register window) */
#define DUBK_LEGACY ((uint32_t)('D' << 24 | 'U' << 16 | 'B' << 8 | '2'))   /* the Release 2 key, provisional (the format revision decides it) */
#define DUBK_FLAGS (FXK_US | FXK_TAIL_FAR)              /* cost in microseconds; the tail adds the farthest tap; no seed */
#define DUBK_RATES 0x06u                                /* 22.05 and 11.025 kHz, the fallback automatic */
#define DUBK_RAM_EXTRA 2u                               /* the spring: 5,632 B in two blocks' worth */
#define DUBK_LATENCY 61u                                /* a block and the half-rate converters' 29 */
#define DUBK_COST_US 190u                               /* us a block, the worst setting (plan: 95 to 190) */
#define DUBK_GOV_US 450u                                /* what an instance adds to the heaviest half (round 13: the load meter 16 -> 30 %) */
#define DUBK_CALLS .on = dub_on, .size_sig = dub_size_sig, .size_need = dub_far, .blocks_at = dub_blocks_at, .hold_len = dub_hold_len, .tick = dub_tick, .param_sig = dub_param_sig, \
    .snap = dub_snap, .cost_extra = dub_cost_extra, .start = dub_start_k, .process = dub_process, .reset = dub_reset_k
_Static_assert(DUB_STATE_BYTES < 65536u && DUB_SPRING_BYTES < 65536u, "the capability words are 16 bits");
