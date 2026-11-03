/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Release 3 step 5 (FX_MODULES_PLAN.md 5, 11 step 2; section 14 as built): the generic insert host's vocabulary.
 *
 * The insert host (fx_insert.c: the instance state machine, the governor, the capture and the wet; jfx.c: the memory layer jfx_ins_mem) knows nothing of DUB TAPE.
 * What a kernel is to it is one fx_kernel_t: capability words (the plan's section 4 table, the names and widths of MODULE_FORMAT_R3.md section 6's fx_entry_t where
 * they overlap) and the calls the host makes. DUB TAPE's kernel (jfx_dub.c, over dubdelay.c) fills it as a resident const object today; step 6 fills the same struct
 * from a module's fx_desc_t (fw_fx.h) with the calls pointing into the module's image. The host reaches a kernel only through fx_kernel_of(k).
 *
 * Not part of fw_api_t, fw_pages_t or any structure tools/gen_fw_api_id.py hashes: the interface identifier does not move for this file. */
#pragma once
#include "dubdelay.h"
#define FX_INS_N 4u                                     /* instances: one a track (DUB_N) */
#define FX_INS_NSEG 24u                                 /* the pool: 96 KB of 4 KB blocks, shared (four lines of 24K, or one of 64K and two of 16K) */
#define FX_INS_MAXSEG 16u                               /* one line: 64 KB (a kernel's ram_work_max may not exceed it) */
/* the parameter snapshot's room per instance in the common block: the largest snapshot of a resident kernel (dd_params_t today). Step 6 sets it from the largest module's declared
 * size; until then it is DUB's, so the common block is the size it was before the cut (sizeof asserted in jfx_dub.c) */
#define FX_PARAM_BYTES ((uint32_t)((sizeof(dd_params_t) + 3u) & ~3u))
enum { FXS_OFF, FXS_WAIT, FXS_RUN, FXS_RAMP, FXS_CLEAR };   /* the instance states (the editor's schema-14 reply and the tests read the numbers) */
/* a stored parameter of a kernel: fw_fx.h's parameter row without the label. `legacy` is the Release 2 track slot (P_* id) the value is stored in (0xFFFF none) */
typedef struct { uint16_t id, legacy; int16_t min, max, def; } fx_prow_t;
/* one instance's record (CPU0). The host owns every field but kw */
typedef struct {
    uint8_t state, R, ramp, clr, run, pend_R, evict, held_off, gov;     /* evict: ramp out and let go (a bus needs the memory, the CPU is short); held_off: not asking again */
    uint8_t gov_n;                                      /* governor releases in a row: the wait doubles (8, 16, 32 s); cleared by a setting change or 47 s of running */
    uint16_t run_ok;
    uint16_t gov_wait;                                  /* blocks before a governor release may end (8 s), and then only after FX_QUIET blocks without a late half */
    uint16_t rdeb;                                      /* blocks the rate stage has wanted to change: it changes after 344 (a tempo still settling must not clear the line) */
    uint8_t pvalid;                                     /* the job's parameter copy is current for psig */
    uint32_t psig, rsig;                                /* a hash of what the parameter copy / the rate stage were made from: unchanged, not made again */
    float rneed;                                        /* the farthest head's samples at the half rate, from rsig */
    uint8_t gov_sig;                                    /* the (blocks, spring) a governor eviction was made for: a different setting asks again */
    uint16_t idle;                                      /* blocks the effect has been off while the instance stayed */
    uint8_t st_R, st_blk;                               /* the sticky size: the lowest rate and the most blocks this instance has needed (turning TIME down never shrinks the line) */
    uint16_t hold, hold_len;                            /* blocks the instance runs and its track stays mixed past the last audible input or wet; the refresh (the farthest head and 24) */
    uint32_t kw[2];                                     /* the kernel's own words (DUB: the KICK edge count and the last edge state), cleared with the record, passed to fx_kernel_t.tick */
} fx_ins_t;
/* what the host knows of an instance when it asks the kernel for a parameter signature or a snapshot */
typedef struct { uint32_t kw0; uint8_t k, R, nblk, ramp; } fx_ictx_t;   /* kw0: kw[0]; k: the instance (= the track); R: the line's rate divisor; nblk: line blocks held; ramp: ramping out (the cut) */
enum { FXK_SEED = 1, FXK_US = 2, FXK_TAIL_FAR = 4, FXK_TAIL_SILENT = 8 };   /* fx_kernel_t.flags: MODULE_FORMAT_R3.md 6's bits (uses the seed; cost in microseconds; tail adds the farthest tap; tail until silent) */
typedef struct fx_kernel {
    /* identity (the descriptor's fx_desc_t / fx_entry_t words) */
    uint32_t module_id, version, legacy_module;         /* 1 for DUB TAPE; 0x00MMmmpp; the Release 2 key (nonzero: its values sit in the legacy slots of `rows`) */
    uint8_t effect_id, nrows;
    const fx_prow_t *rows;                              /* the stored parameters, in id order: the legacy-slot table */
    /* capabilities (FX_MODULES_PLAN.md 4) */
    uint8_t flags, rates, max_inst, prof;               /* rates: bit 0 44.1 .. 3 5.5125 kHz; prof: the audio_batch_cost class (AP_DUB) the job is pushed under */
    uint8_t ram_work_min, ram_work_max, ram_extra;      /* 4 KB blocks: the least that lets it run, the most it uses, the option's (the spring) */
    uint16_t ram_state, latency, tail_base, cost;       /* bytes of arena state; samples; blocks; microseconds a block (flag FXK_US) */
    uint16_t ram_extra_bytes, gov_us;                   /* the option's bytes; what an instance adds to the heaviest half (the governor's headroom) */
    /* the calls (CPU0 unless said) */
    uint32_t (*on)(const track_t *t);                   /* the track wants the effect (DUB: ECHO != 0) */
    uint32_t (*size_sig)(const track_t *t);             /* a hash of what the sizing is made from (the times, the mode, the speed, the tempo) */
    float (*size_need)(const track_t *t);               /* the farthest tap, in half-rate loop samples */
    uint32_t (*blocks_at)(float need, uint32_t R);      /* the line blocks the need takes at the rate divisor R (2 or 4) */
    uint32_t (*hold_len)(float need, uint32_t blocks, uint32_t R);   /* blocks the instance runs past the last audible input or wet */
    void (*tick)(uint32_t *kw, uint32_t k);             /* once a block for every instance, running or not (DUB: the KICK edge) */
    uint32_t (*param_sig)(const track_t *t, const fx_ictx_t *c);   /* a hash of what the snapshot is made from, host facts included */
    void (*snap)(void *params, const track_t *t, const fx_ictx_t *c);   /* the parameter snapshot (FX_PARAM_BYTES), from the track's stored ids through `rows` */
    uint32_t (*cost_extra)(const track_t *t, uint32_t extra);   /* units added to the batch cost (DUB: the spring attached, WOW) */
    void (*start)(void *state, uint32_t R, int16_t *const *blk, uint32_t nblk, int16_t *extra);   /* (re)start from silence on the blocks given; extra: the option's memory or 0 */
    void (*process)(void *state, const void *params, const float *in, float *outL, float *outR);   /* CPU1: one block */
    void (*reset)(void);                                /* a new project, power-on: the kernel's RAM triggers released */
} fx_kernel_t;
static const fx_kernel_t *fx_kernel_of(uint32_t k);     /* the kernel bound to instance k (jfx_dub.c: DUB's, resident; step 6: by the track's slot) */

/* the names the tests and older readers use for the host's state (the code itself uses fx_ins_*) */
#define DUB_N FX_INS_N
#define DUB_NSEG FX_INS_NSEG
#define DUB_MAXSEG FX_INS_MAXSEG
#define DUBS_OFF FXS_OFF
#define DUBS_WAIT FXS_WAIT
#define DUBS_RUN FXS_RUN
#define DUBS_RAMP FXS_RAMP
#define DUBS_CLEAR FXS_CLEAR
#define JFX_FULL_DUB0 JFX_FULL_INS0
#define dub_i fx_ins
#define dub_cm ((dub_common_t *)fx_cm)
#define dub_state fx_ins_state
#define dub_spbuf fx_ins_spbuf
#define dub_want fx_ins_want
#define dub_spring fx_ins_spring
#define dub_have fx_ins_have
#define dub_ready fx_ins_ready
#define dub_ev_full fx_ins_ev_full
#define dub_force_degrade fx_ins_force_degrade
#define dub_gov_req fx_ins_gov_req
#define dub_gov_fires fx_ins_gov_fires
#define dub_late_half fx_ins_late_half
#define dub_headroom fx_ins_headroom
#define dub_reset fx_ins_reset
#define jfx_dub_mem jfx_ins_mem
#define JB_DUBS0 JB_INSS0
#define JB_DUBP0 JB_INSP0
#define JB_DUBG0 JB_INSG0
#define JB_DUBC JB_INSC
