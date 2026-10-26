/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: ORBIT's PULSE (https://github.com/dspaudio/orbit, commit c07bd1d: firmware/src/eng_orbit.c, Copyright (C) 2026 ORBIT contributors,
 * GPL-3.0-only), the copy in tools/module/orbit/ up to its engine_ts (the file holds SWARM, PULSE and FM4: each its own module; the link keeps one).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"): ORBIT's PULSE (a dual pulse oscillator:
 * one pulse whose width its own LFO moves (PWM, RATE) and a square detuned by DETUNE, mixed; polynomial step corrections at the edges; a one-pole
 * TONE) as a
 * loadable module translation unit (stage 0). The proposed engine number is 50 (core.h ENGI_ORBIT_PULSE; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (OSC) and EDIT 2 (TONE): ORBIT's controls (its "--" unused); presets ORBIT's four, through cl_preset_make (their sends dropped);
 *   - voice state in voice_t (ph[0..2], s[0..7]): no arena block (the 4-byte token); poly 4 as ORBIT's; per sample SINE (sine_i: the base hot set);
 *     no hot set of its own.
 */
#include "clipper/clipper_unit.h"
#define preset_t cl_preset_t
#include "orbit/eng_orbit.c"
#undef preset_t
static void orbit_pulse_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(ORBIT_PULSE_PRESETS, NELEM(ORBIT_PULSE_PRESETS), k, p, name); }
static const engine_t ENG_ORBIT_PULSE = {               /* ORBIT's ENG_ORBIT_PULSE in Orange Cat's order (its fil_page 1, colour and macro: knob) */
    .name = "PULSE", .page_title = {"OSC", "TONE"},
    .edit = {
        {"WIDTH", F_INT, 0, 127, 64, 0, 0},
        {"DETUNE", F_INT, 0, 100, 0, 0, 0},
        {"PWM", F_INT, 0, 127, 0, 0, 0},
        {"RATE", F_INT, 0, 127, 20, 0, 0},
        {"TONE", F_PCT, 0, 127, 100, 0, 0},
        {"MIX", F_INT, 0, 127, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
        {"--", F_INT, 0, 0, 0, 0, 0},
    },
    .preset_make = orbit_pulse_preset_make, .npresets = NELEM(ORBIT_PULSE_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = 4,
    .note_on = orbit_osc_note_on, .render = orbit_pulse_render,
};
CL_MODULE_HEADER(ENGI_ORBIT_PULSE, ENG_ORBIT_PULSE, OC_STATE_TOKEN)
