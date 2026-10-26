/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: ORBIT's CLOUD (https://github.com/dspaudio/orbit, commit c07bd1d: firmware/src/eng_cloud.c, GPL-3.0-only; the file carries only its SPDX
 * line: ORBIT's authors, "ORBIT contributors" in its other files), the copy in tools/module/orbit/ up to its engine_t; its grain helpers (Felucca's,
 * as ORBIT has them) in tools/module/orbit/orbit_dsp.h.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"): CLOUD (oscillator grains, no samples:
 * twelve grains a part shared fairly by three voices, each a sine morphing to a triangle and to a three-harmonic spectrum under a Hann window;
 * SIZE, DENS, SPRD (timing and phase jitter), DTUN, CHRP (a sweep a grain), STCK (fifths and octaves by chance), TONE (a one-pole low-pass)) as a
 * loadable module translation unit (stage 0). The proposed engine number is 43 (core.h ENGI_ORBIT_CLOUD; OC_CANDIDATES).
 *   - EDIT 1 (GRAINS: WAVE SIZE DENS SPRD), EDIT 2 (SPECTRUM: DTUN CHRP STCK TONE): ORBIT's eight; presets ORBIT's, through cl_preset_make (their
 *     effect sends dropped); poly 3, as ORBIT's (the twelve grains are shared by three voices);
 *   - its part state (cloud_part_t: the twelve grains, the generator) in the part's arena block, as ORBIT's eng_arena_of gives it; the module
 *     clears it the first time the block is the module's (a magic word: ORBIT clears it when the part's engine changes);
 *   - per sample SINE (three reads a grain, osc_sine: the base hot set) and TANH_Q15 (soft_knee); the Hann table once a block a grain.
 */
#include "clipper/clipper_unit.h"
#include "orbit/orbit_dsp.h"
#define ENGI_CLOUD ENGI_ORBIT_CLOUD
struct cloud_arena;
static void *cloud_arena(const track_t *t);
#define eng_arena_of(t, e) cloud_arena(t)
#define preset_t cl_preset_t
#include "orbit/eng_cloud.c"
#undef preset_t
#undef eng_arena_of
#define CLOUD_MAGIC 0x44554C43u            /* "CLUD" */
typedef struct { cloud_part_t p; uint32_t magic; } cloud_arena_t;
static void *cloud_arena(const track_t *t)
{
    cloud_arena_t *a = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(cloud_arena_t));
    if (a && a->magic != CLOUD_MAGIC) {    /* (ORBIT's eng_arena_of: cleared when the block was another engine's) */
        memset(a, 0, sizeof *a);
        a->magic = CLOUD_MAGIC;
    }
    return a ? &a->p : 0;
}
static void cloud_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(CLOUD_PRESETS, NELEM(CLOUD_PRESETS), k, p, name); }
static const engine_t ENG_CLOUD = {          /* ORBIT's ENG_CLOUD in Orange Cat's fields (its fil_page 1, colour 0x87F0 and macro: knob) */
    .name = "CLOUD", .page_title = {"GRAINS", "SPECTRUM"},
    .edit = {
        {"WAVE", F_PCT, 0, 127, 0, 0, 0},
        {"SIZE", F_PCT, 0, 127, 100, 0, 0},
        {"DENS", F_PCT, 0, 127, 100, 0, 0},
        {"SPRD", F_PCT, 0, 127, 55, 0, 0},
        {"DTUN", F_PCT, 0, 127, 18, 0, 0},
        {"CHRP", F_BIPCT, -64, 63, 0, 0, 0},
        {"STCK", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 100, 0, 0},
    },
    .preset_make = cloud_preset_make, .npresets = NELEM(CLOUD_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = 3,
    .note_on = cloud_note_on, .render = cloud_render, .block = cloud_block,
};
CL_MODULE_HEADER(ENGI_ORBIT_CLOUD, ENG_CLOUD, sizeof(cloud_arena_t))
