/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit WAVSHP, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_waveshaping.h"
PL_MDL_FUNCS(wavshp, pl_ws_t, pl_ws)
const pl_mdl_t pl_mdl_wavshp = PL_MDL_DESC(wavshp, pl_ws_t, 0, 0, 0, 5, 0.7f, 0.6f);
PL_KEEP(&pl_mdl_wavshp);
