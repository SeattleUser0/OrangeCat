/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 2 as the insert; Release 3 step 5; round 20) DUB TAPE on the image's side of the cut. Included by mix.c after jfx.c and fx_insert.c.
 *
 * There is no DUB kernel in the image any more (FX_MODULES_PLAN.md 15.9): the kernel is the DUB module (tools/module/dub_mod.c over dubdelay.c and dub_kern.h, orangecat-dub.ocm), the insert
 * host (fx_insert.c) runs whatever kernel fx_modk_set registered, and with none registered a track with DUB on plays dry (the DUB page says MODULE MISSING). What stays here is the
 * common block under its old name (the tests read dub_cm->p[k].time_l) and fx_kernel_of: the registered module's kernel for the track's instance, else an empty one (the host never calls
 * it: fx_ins_start returns before it asks, and jfx_ins_mem lends nothing for an instance that does not exist). */
#if MELODEE_DUB
_Static_assert(DD_BLOCK == CTL, "a job a block");
_Static_assert(DD_BLK_N == JFX_SEG && DD_MAXBLK <= FX_INS_MAXSEG, "the kernel's line block is the arena's");

/* the common block as it was named before the cut: the snapshots are dd_params_t here (the tests read dub_cm->p[k].time_l). Same layout as fx_common_t (jfx.c) by assertion */
typedef struct { dd_params_t p[FX_INS_N]; int32_t x[2][FX_INS_N][32]; float wet[FX_INS_N][2][32]; int32_t sink[32];
                 uint8_t pool[FX_INS_N][FX_INS_MAXSEG]; int16_t *line[FX_INS_N][FX_INS_MAXSEG]; } dub_common_t;
_Static_assert(sizeof(dub_common_t) == sizeof(fx_common_t) && FX_PARAM_BYTES == sizeof(dd_params_t) && offsetof(dub_common_t, x) == offsetof(fx_common_t, x) &&
               offsetof(dub_common_t, line) == offsetof(fx_common_t, line), "the common block: one layout");
#endif
static const fx_kernel_t fx_kernel_none;
static const fx_kernel_t *fx_kernel_of(uint32_t k)
{
#if MELODEE_DUB
    return FX_MODK_BOUND(k) ? &fx_modk : &fx_kernel_none;
#else
    (void)k;
    return &fx_kernel_none;
#endif
}
