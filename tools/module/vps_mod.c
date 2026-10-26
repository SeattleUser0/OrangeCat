/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's VPS (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_vps.c, Copyright (C) 2026 3dSam (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t; the pieces of Clipper's dsp.c it reads from
 * tools/module/clipper/clipper_dsp.h.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): VPS (vector phaseshaping after Kleimola, Lazzarini,
 * Timoney and Valimaki, DAFx-11: a sine read through a phase bent at an inflection point (D, V): below V 1 a phase-distorted pulse, above it a
 * formant at V x f0; floor(V) and floor(V) + 1 crossfaded; a second oscillator D mirrored, a sub, a 12 dB low-pass) as a loadable module
 * translation unit (stage 0).
 * The proposed engine number is 36 (core.h ENGI_VPS; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (VECTOR) and EDIT 2 (TONE): Clipper's eight; presets: Clipper's, through cl_preset_make (their sends, arp and extras dropped);
 *   - voice state in voice_t (ph[0..2], s[0..2]): no arena block (the 4-byte token); per sample SINE (osc_sine, two reads: the base hot set) and
 *     TANH_Q15 (soft_knee); SVF_G once a block (tsvf_coef); no hot set of its own.
 */
#include "clipper/clipper_unit.h"
#define preset_t cl_preset_t
#include "clipper/eng_vps.c"
#undef preset_t
static void vps_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(VPS_PRESETS, NELEM(VPS_PRESETS), k, p, name); }
static const engine_t ENG_VPS = {           /* Clipper's ENG_VPS in Orange Cat's order (its fil_page 1 and colour 0xFB2C have no field here) */
    .name = "VPS", .page_title = {"VECTOR", "TONE"},
    .edit = {
        {"D", F_PCT, 0, 127, 50, 0, 0},
        {"V", F_INT, 0, 127, 32, 0, 0},
        {"VENV", F_BIPCT, -64, 63, 0, 0, 0},
        {"OSC2", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 10, 0, 0},
        {"DTN", F_PCT, 0, 127, 20, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
    },
    .preset_make = vps_preset_make, .npresets = NELEM(VPS_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E4},
    .note_on = vps_note_on, .render = vps_render,
};
CL_MODULE_HEADER(ENGI_VPS, ENG_VPS, OC_STATE_TOKEN)
