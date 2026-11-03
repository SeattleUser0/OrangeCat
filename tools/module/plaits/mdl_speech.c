/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): model unit SPEECH, one region of the image (tools/module/plaits/pl_mod_common.h). Its own tables, the
 * five word banks and the LPC pulse (pl_tables2.h), are defined here: SPEECH alone reads them, so they are part of its region. */
#define PL_T2_DEF_pl_lut_lpc_excitation_pulse 1
#define PL_T2_DEF_pl_lpc_bank_0 1
#define PL_T2_DEF_pl_lpc_bank_1 1
#define PL_T2_DEF_pl_lpc_bank_2 1
#define PL_T2_DEF_pl_lpc_bank_3 1
#define PL_T2_DEF_pl_lpc_bank_4 1
#include "pl_mod_common.h"
#include "plaits/pl_speech.h"
PL_MDL_FUNCS(speech, pl_speech_t, pl_speech)
const pl_mdl_t pl_mdl_speech = PL_MDL_DESC(speech, pl_speech_t, 1, 1, 0, 5, -0.7f, 0.8f);   /* (enveloped as each block says: a replayed word is, a held vowel is not) */
PL_KEEP(&pl_mdl_speech);
