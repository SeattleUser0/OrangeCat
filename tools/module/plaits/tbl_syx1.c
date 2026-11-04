/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * MODELS (the model picker's module): DX7 bank 1 and the descriptor of model 6OP B, one region of the image: the model is installed when its
 * bank is; its code is the six-op region (mdl_sixop.c), which the pick brings with any bank (tools/pick_module.py). */
#define PL_T2_DEF_pl_syx_bank_1 1
#define PL_WANT_syx1 1
#include "pl_mod_common.h"
#include "plaits/pl_models.h"
PL_KEEP(pl_syx_bank_1);
