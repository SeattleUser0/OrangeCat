/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit HI-HAT, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_hihat.h"
PL_MDL_FUNCS(hihat, pl_hh_t, pl_hh)
const pl_mdl_t pl_mdl_hihat = PL_MDL_DESC(hihat, pl_hh_t, 0, 1, 1, 3, 0.8f, 0.8f);
PL_KEEP(&pl_mdl_hihat);
