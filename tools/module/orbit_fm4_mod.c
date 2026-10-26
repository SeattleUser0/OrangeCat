/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: ORBIT's FM4 (https://github.com/dspaudio/orbit, commit c07bd1d: firmware/src/eng_orbit.c, Copyright (C) 2026 ORBIT contributors,
 * GPL-3.0-only), the copy in tools/module/orbit/ up to its engine_ts (the file holds SWARM, PULSE and FM4: each its own module; the link keeps one).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"): ORBIT's FM4 (four sine operators at
 * whole ratios, three routings (a chain, two pairs, three modulators into one carrier), feedback on the fourth, the depth contracted by the
 * envelope (ENV); a one-pole TONE) as a
 * loadable module translation unit (stage 0). The proposed engine number is 51 (core.h ENGI_ORBIT_FM4; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (OPS) and EDIT 2 (ROUTE): ORBIT's controls (its "--" unused); presets ORBIT's four, through cl_preset_make (their sends dropped);
 *   - voice state in voice_t (ph[0..2], s[0..7]): no arena block (the 4-byte token); poly 4 as ORBIT's; per sample SINE (sine_i: the base hot set);
 *     no hot set of its own.
 */
#include "clipper/clipper_unit.h"
#define preset_t cl_preset_t
#include "orbit/eng_orbit.c"
#undef preset_t
static void orbit_fm4_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(ORBIT_FM4_PRESETS, NELEM(ORBIT_FM4_PRESETS), k, p, name); }
static const engine_t ENG_FM4 = {               /* ORBIT's ENG_FM4 in Orange Cat's order (its fil_page 1, colour and macro: knob) */
    .name = "FM4", .page_title = {"OPS", "ROUTE"},
    .edit = {
        {"DEPTH", F_INT, 0, 127, 45, 0, 0},
        {"RATIO2", F_INT, 1, 8, 1, 0, 0},
        {"RATIO3", F_INT, 1, 8, 3, 0, 0},
        {"RATIO4", F_INT, 1, 8, 7, 0, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
        {"ROUTE", F_ENUM, 0, 2, 1, ORBIT_FM_ROUTING, 0},
        {"ENV", F_INT, 0, 127, 100, 0, 0},
        {"FEEDBACK", F_INT, 0, 127, 0, 0, 0},
    },
    .preset_make = orbit_fm4_preset_make, .npresets = NELEM(ORBIT_FM4_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = 4,
    .note_on = orbit_osc_note_on, .render = orbit_fm4_render,
};
CL_MODULE_HEADER(ENGI_ORBIT_FM4, ENG_FM4, OC_STATE_TOKEN)
