/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's JUNO (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_juno.c, Copyright (C) 2026 3dSam (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t; its ladder from Clipper's dsp.c (tools/module/clipper/clipper_dsp.h).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): JUNO (the Juno-6 / 60 / 106 idea: one DCO with a saw and a
 * pulse whose width SHP moves, a square sub an octave down, noise; the 106's four HPF positions; Clipper's four-pole TPT ladder) as a loadable module
 * translation unit (stage 0). The proposed engine number is 32 (core.h ENGI_JUNO; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (DCO: WAVE PW SUB NOIS), EDIT 2 (VCF: HPF CUT RES CHOR): Clipper's eight. CHOR selects Clipper's BBD chorus, which is not the engine's:
 *     it is Clipper's part effect (fx.c juno_chorus, after the sound's insert and filter). Orange Cat's effects are the track's and have no such
 *     chorus, so CHOR does nothing here (the gate's "controls" row);
 *   - presets: Clipper's seventeen, through cl_preset_make: their sends, arp and extras (the LFO on the PW, glide, vibrato) dropped;
 *   - voice state in voice_t (ph[0..2], s[0..5]): no arena block (the 4-byte token); poly 0 (NPOLY), units the default (2 a voice);
 *   - per sample: SVF_G is read once a block (ladder_coef), TANH_Q15 per sample (softclip, the ladder's tanh: the base hot set); no hot set.
 */
#include "clipper/clipper_unit.h"
#define preset_t cl_preset_t
#include "clipper/eng_juno.c"
#undef preset_t
static void juno_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(JUNO_PRESETS, NELEM(JUNO_PRESETS), k, p, name); }
static const engine_t ENG_JUNO = {           /* Clipper's ENG_JUNO in Orange Cat's order (its fil_page 1 and colour have no field here) */
    .name = "JUNO", .page_title = {"DCO", "VCF"},
    .edit = {
        {"WAVE", F_ENUM, 0, 2, JW_SP, N_JWAVE, 0},
        {"PW", F_PCT, 0, 127, 30, 0, 0},
        {"SUB", F_PCT, 0, 127, 40, 0, 0},
        {"NOIS", F_PCT, 0, 127, 0, 0, 0},
        {"HPF", F_ENUM, 0, 3, 1, N_JHPF, 0},
        {"CUT", F_CUTOFF, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
        {"CHOR", F_ENUM, 0, 3, 1, N_JCHOR, 0},
    },
    .preset_make = juno_preset_make, .npresets = NELEM(JUNO_PRESETS),
    .knob = {P_E5, P_E6, P_E1, P_E7},
    .note_on = juno_note_on, .render = juno_render,
};
CL_MODULE_HEADER(ENGI_JUNO, ENG_JUNO, OC_STATE_TOKEN)
