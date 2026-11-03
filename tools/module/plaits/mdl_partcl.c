/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit PARTCL, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_particle.h"
PL_MDL_FUNCS(partcl, pl_particle_t, pl_particle)
const pl_mdl_t pl_mdl_partcl = PL_MDL_DESC(partcl, pl_particle_t, 1, 1, 0, 4, -2.0f, 1.0f);
PL_KEEP(&pl_mdl_partcl);
