/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's STRINGS (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_strings.c, Copyright (C) 2026 3dSam (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t, its file-scope LFOs the part's; hy_saw (HYBRID's) and Felucca's tsvf_coef from
 * tools/module/clipper/clipper_dsp.h.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): STRINGS (a 1970s string machine: divide-down registers 16'
 * 8' 4' from one phase a tap, so they stay locked; three taps whose pitch two LFOs move 120 degrees apart, the BBD ensemble's slow sweep and fast
 * shimmer; a 12 dB low-pass) as a loadable module translation unit (stage 0). The proposed engine number is 34 (core.h ENGI_STRINGS; OC_CANDIDATES).
 *   - EDIT 1 (REG: 16' 8' 4' ENS), EDIT 2 (TONE: CUT RATE SHP RES): Clipper's eight; HOME's fourth knob is the track's ATK, as Clipper's;
 *   - the ensemble's two LFOs are the part's (Clipper: str_lfo[NPART][2], file scope, advanced by its block hook): a module has no RAM of its own,
 *     so each part's arena block holds its own copy of that array (strings_part_t, 32 B), of which the part reads its row, as the file indexes it;
 *   - presets: Clipper's sixteen, through cl_preset_make (the sends dropped; LOW STR's transpose -12 and TAPE STR's LFO on the pitch dropped);
 *   - voice state in voice_t (ph[0..2], s[0..1]); per block SVF_G (tsvf_coef); per sample SINE (osc_sine, the taps' LFOs: the base hot set) and
 *     TANH_Q15 (soft_knee): no hot set of its own.
 */
#include "clipper/clipper_unit.h"
typedef struct { uint32_t lfo[NPART][2]; } strings_part_t;   /* eng_strings.c's str_lfo (each part reads its row) */
static strings_part_t *strings_part(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(strings_part_t)); }
#define str_lfo (strings_part(t)->lfo)     /* (strings_block and strings_render name their track t) */
#define preset_t cl_preset_t
#include "clipper/eng_strings.c"
#undef preset_t
#undef str_lfo
static void strings_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(STRINGS_PRESETS, NELEM(STRINGS_PRESETS), k, p, name); }
static const engine_t ENG_STRINGS = {        /* Clipper's ENG_STRINGS in Orange Cat's order */
    .name = "STRINGS", .page_title = {"REG", "TONE"},
    .edit = {
        {"16'", F_PCT, 0, 127, 40, 0, 0},
        {"8'", F_PCT, 0, 127, 127, 0, 0},
        {"4'", F_PCT, 0, 127, 70, 0, 0},
        {"ENS", F_PCT, 0, 127, 100, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 92, 0, 0},
        {"RATE", F_PCT, 0, 127, 50, 0, 0},
        {"SHP", F_PCT, 0, 127, 0, 0, 0},
        {"RES", F_PCT, 0, 127, 10, 0, 0},
    },
    .preset_make = strings_preset_make, .npresets = NELEM(STRINGS_PRESETS),
    .knob = {P_E3, P_E4, P_E1, P_ATK},
    .note_on = strings_note_on, .render = strings_render, .block = strings_block,
};
CL_MODULE_HEADER(ENGI_STRINGS, ENG_STRINGS, sizeof(strings_part_t))
