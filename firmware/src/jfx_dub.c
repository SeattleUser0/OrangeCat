/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 2 as the insert; Release 3 step 5) DUB TAPE, the kernel side of the insert host's cut (docs/orangecat/FX_MODULES_PLAN.md 14). Included by mix.c after jfx.c and fx_insert.c.
 *
 * What is DUB's and not the host's: the kernel itself (dubdelay.c, LITE); the ten stored ids and where Release 2 keeps them (dub_rows: the legacy-slot table, the stable ids of
 * MODULE_FORMAT_R3.md 15.2 beside the track slots P_DUB_* of dub_fields.h); the parameter snapshot from the track's values THROUGH that table (dub_snap); the sizing (the farthest tap, the
 * rate stage's blocks, the hold); the signatures; the KICK edge; the batch cost's extras; and the one fx_kernel_t that hands all of it to the host (dub_kernel, resident today: step 6 builds the
 * same struct inside a module's image from its fx_desc_t). Nothing here reads the host's records (fx_ins, the memory arrays): the host passes what the kernel needs (fx_ictx_t). */
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
#if OC_DUB_KSRAM                                        /* (round 4) only DUB's per-block kernel in SRAM (dd_run_lite, dd_sp_run, dk_process); the control path stays in flash */
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
_Static_assert(DD_BLK_N == JFX_SEG && DD_MAXBLK <= FX_INS_MAXSEG, "the kernel's line block is the arena's");

/* the common block as it was named before the cut: the snapshots are dd_params_t here (the tests read dub_cm->p[k].time_l). Same layout as fx_common_t (jfx.c) by assertion */
typedef struct { dd_params_t p[FX_INS_N]; int32_t x[2][FX_INS_N][32]; float wet[FX_INS_N][2][32]; int32_t sink[32];
                 uint8_t pool[FX_INS_N][FX_INS_MAXSEG]; int16_t *line[FX_INS_N][FX_INS_MAXSEG]; } dub_common_t;
_Static_assert(sizeof(dub_common_t) == sizeof(fx_common_t) && FX_PARAM_BYTES == sizeof(dd_params_t) && offsetof(dub_common_t, x) == offsetof(fx_common_t, x) &&
               offsetof(dub_common_t, line) == offsetof(fx_common_t, line), "the common block: one layout");

#include "dub_kern.h"                                   /* the kernel side, shared with the module (Release 3 step 6) */

/* DUB TAPE's descriptor (resident): the plan's section 4 column, the words of dub_kern.h. The module's is the same words in an fx_caps_t (tools/module/dub_mod.c) */
static const fx_kernel_t dub_kernel = {
    .module_id = DUBK_MODULE_ID, .version = DUBK_VERSION, .legacy_module = DUBK_LEGACY, .effect_id = 1u, .nrows = DUBP_N, .rows = dub_rows,
    .flags = DUBK_FLAGS, .rates = DUBK_RATES, .max_inst = FX_INS_N, .prof = AP_DUB,
    .ram_work_min = DUB_FLOOR /* as built; the plan's 2 was the old LEN 8K */, .ram_work_max = DUB_MAXB, .ram_extra = DUBK_RAM_EXTRA,
    .ram_state = DUB_STATE_BYTES, .latency = DUBK_LATENCY, .tail_base = DUB_HOLD, .cost = DUBK_COST_US,
    .ram_extra_bytes = DUB_SPRING_BYTES, .gov_us = DUBK_GOV_US,
    DUBK_CALLS,
};
/* the kernel bound to instance k: the resident DUB's, or the module's (fx_insert.c: the instance was made with it) */
#if OC_FX_MODK
static const fx_kernel_t *fx_kernel_of(uint32_t k) { return FX_MODK_BOUND(k) ? &fx_modk : &dub_kernel; }
#else
static const fx_kernel_t *fx_kernel_of(uint32_t k) { (void)k; return &dub_kernel; }
#endif
#if OC_DUB_SRAM
#pragma pop_macro("JFX_XIP")
#endif
#else
static const fx_kernel_t fx_kernel_none;
static const fx_kernel_t *fx_kernel_of(uint32_t k) { (void)k; return &fx_kernel_none; }
#endif
