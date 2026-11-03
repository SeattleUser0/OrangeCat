/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit ADDTV, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_additive.h"
PL_MDL_FUNCS(addtv, pl_add_t, pl_add)
const pl_mdl_t pl_mdl_addtv = PL_MDL_DESC(addtv, pl_add_t, 0, 0, 0, 9, 0.8f, 0.8f);
PL_KEEP(&pl_mdl_addtv);
