/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit CHIP, one region of the image (tools/module/plaits/pl_mod_common.h). Made by the model table in
 * tools/build.py (PLAITS_MODELS); the sizes and levels are eng_macro.c MC_POST / MC_UNITS and eng_macro2.c MC2_POST / MC2_UNITS. */
#include "pl_mod_common.h"
#include "plaits/pl_chiptune.h"
PL_MDL_FUNCS(chip, pl_chiptune_t, pl_chiptune)
const pl_mdl_t pl_mdl_chip = PL_MDL_DESC(chip, pl_chiptune_t, 0, 1, 0, 2, 0.5f, 0.5f);
PL_KEEP(&pl_mdl_chip);
