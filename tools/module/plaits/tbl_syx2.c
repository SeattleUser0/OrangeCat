/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): DX7 bank 2 and the descriptor of model 6OP C, one region of the image: the model is installed when its
 * bank is; its code is the six-op region (mdl_sixop.c), which the pick brings with any bank (tools/pick_module.py). */
#define PL_T2_DEF_pl_syx_bank_2 1
#include "pl_mod_common.h"
#include "plaits/pl_sixop.h"
extern void pl_mdl_6op_start2(void *m) __attribute__((weak));
extern int pl_mdl_6op_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n) __attribute__((weak));
const pl_mdl_t pl_mdl_6opc = { sizeof(pl_sixop_t), 1, 1, 1, 6, 1.0f, 1.0f, pl_mdl_6op_start2, pl_mdl_6op_render };
PL_KEEP(&pl_mdl_6opc, pl_syx_bank_2);
