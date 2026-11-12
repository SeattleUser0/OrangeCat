/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * MODELS (the model picker's module, docs/candidates/model_picker.md): the six-op FM code, one region shared by the three models 6OP A, B and C (whose descriptors sit with their DX7 banks, tbl_syx0..2.c: a model is installed when its bank is); the DX unit tables (pl_tables2.h) are this region's own (pl_mod_common.h; the descriptor, its
 * levels and units: firmware/src/plaits/pl_models.h, the one list for the firmware and the module). */
#define PL_T2_DEF_pl_lut_coarse 1
#define PL_T2_DEF_pl_lut_amp_mod_sensitivity 1
#define PL_T2_DEF_pl_lut_pitch_mod_sensitivity 1
#define PL_T2_DEF_pl_lut_cube_root 1
#define PL_WANT_sixop 1
#include "pl_mod_common.h"
#include "plaits/pl_models.h"
