/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): the six-op FM code, one region shared by the three models 6OP A, B and C (whose descriptors sit with
 * their DX7 banks, tbl_syx0..2.c: a model is installed when its bank is). The DX unit tables (pl_tables2.h) are this region's own. */
#define PL_T2_DEF_pl_lut_coarse 1
#define PL_T2_DEF_pl_lut_amp_mod_sensitivity 1
#define PL_T2_DEF_pl_lut_pitch_mod_sensitivity 1
#define PL_T2_DEF_pl_lut_cube_root 1
#include "pl_mod_common.h"
#include "plaits/pl_sixop.h"
void pl_mdl_6op_start0(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 0); pl_sixop_reset((pl_sixop_t *)m); }
void pl_mdl_6op_start1(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 1); pl_sixop_reset((pl_sixop_t *)m); }
void pl_mdl_6op_start2(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 2); pl_sixop_reset((pl_sixop_t *)m); }
int pl_mdl_6op_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n)
{
    int env = 0;
    pl_sixop_render((pl_sixop_t *)m, q, o, a, n, &env);
    pl_sixop_flush((pl_sixop_t *)m);
    return 1;                                         /* (the voice's own envelopes, eng_macro2.c: env = 1) */
}
PL_KEEP(pl_mdl_6op_start0, pl_mdl_6op_start1, pl_mdl_6op_start2, pl_mdl_6op_render);
