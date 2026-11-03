/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit GRAIN, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_grain.h"
PL_MDL_FUNCS(grain, pl_grain_t, pl_grain)
const pl_mdl_t pl_mdl_grain = PL_MDL_DESC(grain, pl_grain_t, 0, 0, 0, 4, 0.7f, 0.6f);
PL_KEEP(&pl_mdl_grain);
