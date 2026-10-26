/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's TERRAIN (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_terrain.c, Copyright (C) 2026 3dSam
 * (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t; the pieces of Clipper's dsp.c it reads from
 * tools/module/clipper/clipper_dsp.h.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): TERRAIN (wave terrain synthesis, lite: an orbit
 * circles over a periodic surface, 32 x 32, read bilinearly, one turn a period; RAD, X, Y place it, ORB squeezes it to an ellipse or turns it
 * into a figure eight, DRIFT moves its centre; a 12 dB low-pass) as a loadable module translation unit (stage 0).
 * The proposed engine number is 37 (core.h ENGI_TERRAIN; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (ORBIT) and EDIT 2 (MOTION): Clipper's eight; presets: Clipper's, through cl_preset_make (their sends, arp and extras dropped);
 *   - the surfaces (TERRAIN: 8 x 32 x 32 int16, 16,384 B, terrain_tables.h, committed in Clipper from its tools/gen_terrain.py, unchanged) are
 * read four
 *     times a sample at scattered points: the part keeps a copy of the one it plays in its arena block (terrain_part_t, 2,048 B, copied when TERR
 *     changes: TER_SURFACE, the eng_terrain.c line marked Orange Cat), the cache idea of tools/module/clipper/wt_shared.h for one 2 KB frame;
 *   - voice state in voice_t (ph[0..1], s[0..2]); per sample SINE (osc_sine, three reads: the base hot set) and TANH_Q15 (soft_knee).
 */
#include "clipper/clipper_unit.h"
#include "clipper/terrain_tables.h"
typedef struct { int16_t s[TER_N * TER_N]; uint8_t idx; } terrain_part_t;   /* the surface the part plays (idx + 1; 0: none) */
static const int16_t *terrain_surface(const track_t *t, uint32_t k)
{
    terrain_part_t *P = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(terrain_part_t));
    if (P->idx != k + 1u) {
        memcpy(P->s, TERRAIN[k], sizeof P->s);
        P->idx = (uint8_t)(k + 1u);
    }
    return P->s;
}
#define TER_SURFACE(t, k) terrain_surface((t), (k))
#define preset_t cl_preset_t
#include "clipper/eng_terrain.c"
#undef preset_t
static void terrain_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(TERRAIN_PRESETS, NELEM(TERRAIN_PRESETS), k, p, name); }
static const engine_t ENG_TERRAIN = {           /* Clipper's ENG_TERRAIN in Orange Cat's order (its fil_page 1 and colour 0x2E8B have no field here) */
    .name = "TERRAIN", .page_title = {"ORBIT", "MOTION"},
    .edit = {
        {"TERR", F_ENUM, 0, TER_COUNT - 1, 0, N_TERRAIN, 0},
        {"RAD", F_PCT, 0, 127, 50, 0, 0},
        {"X", F_PCT, 0, 127, 0, 0, 0},
        {"Y", F_PCT, 0, 127, 0, 0, 0},
        {"ORB", F_PCT, 0, 127, 64, 0, 0},
        {"DRIFT", F_PCT, 0, 127, 20, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 0, 0, 0},
    },
    .preset_make = terrain_preset_make, .npresets = NELEM(TERRAIN_PRESETS),
    .knob = {P_E1, P_E2, P_E3, P_E5},
    .note_on = terrain_note_on, .render = terrain_render,
};
CL_MODULE_HEADER(ENGI_TERRAIN, ENG_TERRAIN, sizeof(terrain_part_t))
