/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's ACID (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_acid.c, Copyright (C) 2026 3dSam (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t; the ladder and hy_saw (HYBRID's saw) from Clipper's dsp.c and eng_hybrid.c
 * (tools/module/clipper/clipper_dsp.h).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): ACID (a TB-303 manner of bass line: a saw or a 45 % square
 * into the four-pole ladder, a high-pass and a drive; the filter envelope's decay from the note's start, the accent at velocity >= 120 (louder, a
 * shorter deeper envelope, and the sweep: a run of accents charges a capacitor that pushes the cutoff up); a slide is a legato note, which voice.c
 * glides without a note_on) as a loadable module translation unit (stage 0). The proposed engine number is 33 (core.h ENGI_ACID; OC_CANDIDATES).
 *   - EDIT 1 (VCF: WAVE CUT RES ENV), EDIT 2 (ENV: DEC ACC DIST SWP): Clipper's eight;
 *   - presets: Clipper's, through cl_preset_make: their extras dropped, among them P_TRANS -12 (Clipper's presets play an octave down) and its glide
 *     mode TIME (the slides): ours play at the keys' octave with the track's own glide (the gate's "lost" row);
 *   - voice state in voice_t (ph[0], s[0..7]): no arena block (the 4-byte token); poly 1 (one voice, as Clipper's), units the default;
 *   - per block: DECAY_K (the envelopes' rates: the same generator line as Clipper's), SVF_G (ladder_coef); per sample TANH_Q15 (the base hot set).
 */
#include "clipper/clipper_unit.h"
#define preset_t cl_preset_t
#include "clipper/eng_acid.c"
#undef preset_t
static void acid_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(ACID_PRESETS, NELEM(ACID_PRESETS), k, p, name); }
static const engine_t ENG_ACID = {           /* Clipper's ENG_ACID in Orange Cat's order */
    .name = "ACID", .page_title = {"VCF", "ENV"},
    .edit = {
        {"WAVE", F_ENUM, 0, 1, 0, N_ACID_WAVE, 0},
        {"CUT", F_CUTOFF, 0, 127, 30, 0, 0},
        {"RES", F_PCT, 0, 127, 100, 0, 0},
        {"ENV", F_PCT, 0, 127, 70, 0, 0},
        {"DEC", F_PCT, 0, 127, 60, 0, 0},
        {"ACC", F_PCT, 0, 127, 90, 0, 0},
        {"DIST", F_PCT, 0, 127, 20, 0, 0},
        {"SWP", F_PCT, 0, 127, 60, 0, 0},
    },
    .preset_make = acid_preset_make, .npresets = NELEM(ACID_PRESETS),
    .knob = {P_E1, P_E2, P_E3, P_E4}, .poly = 1,
    .note_on = acid_note_on, .render = acid_render,
};
CL_MODULE_HEADER(ENGI_ACID, ENG_ACID, OC_STATE_TOKEN)
