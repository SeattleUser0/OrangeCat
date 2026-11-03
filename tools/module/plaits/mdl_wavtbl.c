/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit WAVTBL, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_wavetable.h"
PL_MDL_FUNCS(wavtbl, pl_wt_t, pl_wt)
const pl_mdl_t pl_mdl_wavtbl = PL_MDL_DESC(wavtbl, pl_wt_t, 0, 0, 0, 8, 0.6f, 0.6f);
PL_KEEP(&pl_mdl_wavtbl);
