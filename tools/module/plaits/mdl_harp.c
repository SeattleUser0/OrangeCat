/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit HARP, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_string.h"
PL_MDL_FUNCS(harp, pl_harp_t, pl_harp)
const pl_mdl_t pl_mdl_harp = PL_MDL_DESC(harp, pl_harp_t, 0, 1, 1, 4, -1.0f, 0.8f);
PL_KEEP(&pl_mdl_harp);
