/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit NOISE, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_noise.h"
PL_MDL_FUNCS(noise, pl_noise_t, pl_noise)
const pl_mdl_t pl_mdl_noise = PL_MDL_DESC(noise, pl_noise_t, 0, 1, 0, 2, -1.0f, -1.0f);
PL_KEEP(&pl_mdl_noise);
