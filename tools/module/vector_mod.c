/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's VECTOR (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_vector.c and its waves vector_tables.h, from
 * tools/gen_vector.py, Copyright (C) 2026 3dSam (Clipper), GPL-3.0-only), the copies in tools/module/clipper/ (the engine up to its engine_t, its
 * four wave reads through the frame cache; the waves unchanged); the filter models from Clipper's dsp.c (clipper_dsp.h).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): VECTOR (vector synthesis after the Prophet VS and the
 * Wavestation's joystick: four single-cycle waves at a square's corners mixed by a point's nearness, bilinear; MOVE sends the point round a circle, a
 * figure eight, a sweep from A, the square's edge or a random walk; a 12 dB low-pass) as a loadable module translation unit (stage 0), on the shared
 * wavetable backend (tools/module/clipper/wt_shared.h). The proposed engine number is 35 (core.h ENGI_VECTOR; OC_CANDIDATES).
 *   - EDIT 1 (WAVES: A B C D), EDIT 2 (JOYSTICK: X Y MOVE CUT): Clipper's eight;
 *   - the waves: VEC_WAVE (16 waves x 672 int16 in three copies: 21,504 B, in the module's flash) is wt_shared.h's bank format; the four corners a
 *     part plays are copied into the part's arena block (vector_part_t: wts_cache_t, 4 x 1,344 B) when a corner changes, and the voices read those
 *     (21,504 B as a hot set otherwise, or per-sample reads of the module area's flash: the cache rule);
 *   - the filter: Clipper's FX > FILTER > MODL (P_FMODEL, a track parameter Orange Cat has not got) picks a model; here FM_SVF always, as in
 *     Clipper's groovebox build (fm_model, below);
 *   - presets: Clipper's, through cl_preset_make (their sends dropped);
 *   - voice state in voice_t (ph[0..2], s[0..7]); per sample SINE (MOVE's circle and eight: osc_sine, once a block) and TANH_Q15 (soft_knee: the base
 *     hot set); no hot set of its own.
 */
#include "clipper/clipper_unit.h"
#include "clipper/wt_shared.h"
static inline int32_t fm_model(const track_t *t) { (void)t; return FM_SVF; }   /* Clipper's engines.c without CLIPPER_SYNTH */
typedef struct { wts_cache_t c; } vector_part_t;
static vector_part_t *vector_part(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(vector_part_t)); }
#define VEC_FRAME(t, s, idx) wts_frame(&vector_part(t)->c, (s), VEC_WAVE, VEC_NW, (idx))
#define preset_t cl_preset_t
#include "clipper/eng_vector.c"
#undef preset_t
_Static_assert(VEC_PTS == WTS_PTS && sizeof VEC_OFF == sizeof WTS_OFF, "VECTOR's waves are wt_shared.h's bank format");
static void vector_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(VECTOR_PRESETS, NELEM(VECTOR_PRESETS), k, p, name); }
static const engine_t ENG_VECTOR = {         /* Clipper's ENG_VECTOR in Orange Cat's order */
    .name = "VECTOR", .page_title = {"WAVES", "JOYSTICK"},
    .edit = {
        {"A", F_ENUM, 0, VEC_NW - 1, 2, N_VEC_WAVE, 0},
        {"B", F_ENUM, 0, VEC_NW - 1, 7, N_VEC_WAVE, 0},
        {"C", F_ENUM, 0, VEC_NW - 1, 14, N_VEC_WAVE, 0},
        {"D", F_ENUM, 0, VEC_NW - 1, 9, N_VEC_WAVE, 0},
        {"X", F_BIPCT, -64, 63, 0, 0, 0},
        {"Y", F_BIPCT, -64, 63, 0, 0, 0},
        {"MOVE", F_ENUM, 0, VM_N - 1, VM_CIRC, N_VMOVE, 0},
        {"CUT", F_CUTOFF, 0, 127, 100, 0, 0},
    },
    .preset_make = vector_preset_make, .npresets = NELEM(VECTOR_PRESETS),
    .knob = {P_E4, P_E5, P_E7, P_E6},
    .note_on = vector_note_on, .render = vector_render,
};
CL_MODULE_HEADER(ENGI_VECTOR, ENG_VECTOR, sizeof(vector_part_t))
