/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * WAVE (engine 32) as a loadable module: the engine (tools/module/wave/eng_wave.c), its starter wavetables (tools/module/wave/wave_tables.h, ours: four tables of
 * eight 256-sample frames, 16 KB), its presets and their patches, its pages and HOME's four macro knobs (fw_pages_t version 3, tools/module/cand_pages.h).
 * Registrable in a build with OC_WAVE (core.h). docs/candidates/wave.md (the design), docs/candidates/wave_engine.md (as built).
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0 */
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_fw;
#define OC_FW() (oc_host_fw)
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#endif
#include "oc_pack.h"
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }
#define trk (OC_FW()->trk)
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }
#include "audio_profile.h"
#if !OC_WAVE
#error "WAVE is built with OC_WAVE=1 (core.h): its kernel's profile id AP_WAVE and FW_API_VERSION 7"
#endif
typedef void (*audio_batch_fn)(const void *data, int32_t *pcm);
#define AUDIO_BATCH_BYTES 168u
#define audio_batch_active (*OC_FW()->batch_active)
#define audio_batch_push(run, data, bytes, out, n, id) (OC_FW()->batch_push((run), (data), (bytes), (out), (n), (id)))
/* (stereo) the firmware renders a module's right lane from FW_API_VERSION 8 (fw_api.h FW_MOD_COST_STEREO); on an older one WAVE folds to mono */
extern const fw_mod_hdr_t OC_MODULE_HDR;
static inline int wave_stereo_fw(void)       /* the firmware takes the lane (8 or later) AND this module's header says it renders one (tools/build.py writes the cost word after the
                                              * link: read through a volatile, so the compiler does not take the initializer's 0) */
{
    return OC_FW()->version >= 8u && (*(volatile const uint32_t *)&OC_MODULE_HDR.cost & FW_MOD_COST_STEREO);
}
#define WAVE_API_VERSION 7u                  /* the header's: what WAVE needs (AP_WAVE); stereo is asked at run time, so the module also loads on a 7 */
#include "wave/eng_wave.c"

/* the presets (the eight values, voice.c's ADSR, mono) and, a row each in the same order, their patches. Tables: 0 BASIC, 1 PULSE, 2 VOWEL, 3 BRIGHT.
 * {POS A, POS B, WARP A, WARP B, CUT, RES, DTUN, SUB} */
static const preset_t WAVE_PRESETS[] = {
    {"WAVE BASS", {60, 20, 0, 0, 62, 30, 0, 60}, {0, 64, 110, 20}, 0, 1},
    {"WAVE LEAD", {70, 50, 20, 0, 96, 30, 30, 0}, {0, 64, 120, 40}, 0, 1},
    {"SYNC LEAD", {60, 40, 30, 0, 90, 20, 0, 0}, {0, 64, 120, 35}, 0, 1},
    {"WAVE KEYS", {30, 40, 0, 0, 72, 20, 15, 20}, {0, 72, 50, 50}, 0, 0},
    {"WAVE PAD", {60, 30, 0, 0, 92, 20, 50, 0}, {70, 90, 115, 95}, 0, 0},
    {"PWM STRINGS", {30, 50, 0, 0, 90, 10, 40, 0}, {50, 80, 110, 80}, 0, 0},
    {"WAVE PLUCK", {40, 30, 0, 0, 60, 45, 20, 0}, {0, 66, 0, 50}, 0, 0},
    {"VOX MORPH", {64, 64, 30, 60, 70, 70, 40, 0}, {40, 90, 110, 90}, 0, 0},
    /* (wave-bank) the user tables of the installer page (the sample slot's room): POS A and POS B sweep them; without a bank they play BASIC */
    {"USER TABLE A", {0, 0, 0, 0, 100, 10, 20, 0}, {0, 64, 110, 40}, 0, 0},
    {"USER TABLE B", {0, 64, 0, 0, 100, 10, 30, 0}, {10, 70, 110, 60}, 0, 0},
};
#define S(src, dst, amt) {WS_##src, WD_##dst, amt, 0}
#define S0 {0, 0, 0, 0}
static const wv_patch_t WV_PATCH[] = {
    /* tab, warp, uni, oct, semi, lvl, noise, mode, kbd, ENV 2 {A D S R}, LFO rates, LFO shapes, the four slots, the stereo width, the level (the render analysis) */
    {{0, 1}, {WARP_BEND, WARP_BEND}, {1, 1}, {0, -1}, {0, 0}, {127, 60}, 0, WF_LADDER, 64, {0, 70, 20, 40}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 50), S(VEL, CUT, 20), S0, S0}, 0, 87},
    {{0, 3}, {WARP_BEND, WARP_BEND}, {4, 2}, {0, 0}, {0, 0}, {110, 70}, 0, WF_LP, 64, {0, 60, 80, 40}, {70, 50}, {0, 0},
     {S(LFO1, POSA, 20), S(ENV2, CUT, 30), S0, S0}, 90, 65},
    {{0, 0}, {WARP_SYNC, WARP_BEND}, {1, 1}, {0, 1}, {0, 0}, {120, 40}, 0, WF_LP, 64, {0, 70, 30, 40}, {60, 60}, {0, 0},
     {S(ENV2, WARPA, 80), S(ENV2, CUT, 25), S0, S0}, 0, 67},
    {{2, 0}, {WARP_BEND, WARP_BEND}, {1, 2}, {0, 0}, {0, 0}, {120, 50}, 0, WF_LP, 100, {0, 72, 40, 50}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 40), S(VEL, CUT, 20), S0, S0}, 40, 77},
    {{0, 2}, {WARP_BEND, WARP_BEND}, {4, 4}, {0, 0}, {0, 0}, {100, 80}, 0, WF_LP, 40, {80, 90, 100, 90}, {25, 18}, {0, 0},
     {S(LFO1, POSB, 40), S(LFO2, POSA, 25), S0, S0}, 110, 63},
    {{1, 1}, {WARP_BEND, WARP_BEND}, {4, 4}, {0, 0}, {0, 0}, {110, 70}, 0, WF_LP, 40, {50, 80, 110, 80}, {28, 22}, {0, 0},
     {S(LFO1, POSA, 50), S(LFO2, POSB, 50), S0, S0}, 110, 61},
    {{3, 1}, {WARP_BEND, WARP_BEND}, {2, 1}, {0, 0}, {0, 0}, {120, 40}, 0, WF_LP, 80, {0, 62, 0, 45}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 60), S(ENV2, POSA, 40), S0, S0}, 60, 94},
    {{2, 3}, {WARP_BEND, WARP_MIRROR}, {3, 1}, {0, 0}, {0, 0}, {110, 70}, 10, WF_BP, 40, {40, 90, 110, 90}, {40, 15}, {3, 0},
     {S(LFO1, POSA, 60), S(LFO2, WARPB, 40), S0, S0}, 100, 70},
    /* (wave-bank) USER TABLE A: bank table 0 on both oscillators (B an octave up, quieter); B: bank tables 0 and 1 */
    {{WV_NTAB + 0, WV_NTAB + 0}, {WARP_BEND, WARP_BEND}, {2, 1}, {0, 1}, {0, 0}, {120, 40}, 0, WF_LP, 60, {0, 70, 100, 40}, {60, 60}, {0, 0},
     {S(VEL, CUT, 20), S0, S0, S0}, 60, 64},
    {{WV_NTAB + 0, WV_NTAB + 1}, {WARP_BEND, WARP_BEND}, {2, 2}, {0, 0}, {0, 0}, {110, 90}, 0, WF_LP, 40, {0, 70, 100, 50}, {30, 20}, {0, 0},
     {S(LFO1, POSA, 30), S(LFO2, POSB, 30), S0, S0}, 90, 64},
};
#undef S
#undef S0
_Static_assert(NELEM(WV_PATCH) == NELEM(WAVE_PRESETS), "a patch a preset");

/* the page descriptor (fw_pages_t version 3): the rows, the pid table, HOME's four macro knobs (wave.md 6) */
#include "cand_pages.h"
static const engine_t ENG_WAVE;
static const page_t WAVE_ROWS[] = {CAND_ROW("OSC", 0, 1, 2, 3), CAND_ROW("TONE", 4, 5, 6, 7)};
static const fw_kdef_t WAVE_KDEF[4] = {
    CAND_MACRO("MORPH", CAND_T(0, CAND_LIN, 127), CAND_T(1, CAND_LIN, 90), CAND_T0, CAND_T0),
    CAND_MACRO("WARP", CAND_T(2, CAND_UP, 127), CAND_T(3, CAND_UP, 80), CAND_T(4, CAND_LIN, -30), CAND_T0),
    CAND_MACRO("TONE", CAND_T(4, CAND_LIN, 127), CAND_T(5, CAND_UP, 50), CAND_T0, CAND_T0),
    CAND_MACRO("WIDE", CAND_T(6, CAND_UP, 127), CAND_T(7, CAND_DOWN, -60), CAND_T0, CAND_T0),
};
CAND_PAGES(WAVE, ENG_WAVE, WAVE_ROWS, 8, WAVE_KDEF)
static const engine_t ENG_WAVE = {
    .name = "WAVE", .page_title = {"OSC", "TONE"},
    .edit = {
        {"POSA", F_PCT, 0, 127, 64, 0, 0},
        {"POSB", F_PCT, 0, 127, 64, 0, 0},
        {"WRPA", F_PCT, 0, 127, 0, 0, 0},
        {"WRPB", F_PCT, 0, 127, 0, 0, 0},
        {"CUT", F_PCT, 0, 127, 90, 0, 0},
        {"RES", F_PCT, 0, 127, 20, 0, 0},
        {"DTUN", F_PCT, 0, 127, 20, 0, 0},
        {"SUB", F_PCT, 0, 127, 0, 0, 0},
    },
    .pages = &WAVE_PAGES,
    .presets = WAVE_PRESETS, .npresets = NELEM(WAVE_PRESETS),
    .knob = {P_E0, P_E2, P_E4, P_E6}, .poly = WV_NV,
    .note_on = wave_note_on, .render = wave_render, .units = wave_units, .block = wave_block, .post = wave_post, .keep = 0,
};
static wv_part_t *wave_part(const track_t *t)
{
    wv_part_t *P = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(wv_part_t));
    if (P && P->magic != WV_MAGIC) {
        memset(P, 0, sizeof *P);
        P->magic = WV_MAGIC;
        for (uint32_t i = 0; i < WV_NV; i++) P->v[i].cached[0] = P->v[i].cached[1] = ~0u;
    }
    return P;
}
static const wv_patch_t *wave_patch(const track_t *t) { return &WV_PATCH[t->preset < NELEM(WV_PATCH) ? t->preset : 0u]; }

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), WAVE_API_VERSION, FW_API_ADDR,
    ENGI_WAVE, sizeof(wv_part_t), 0, 0, OC_PACK_HASH,
    FW_MOD_COST_STEREO, OC_MODULE_VERSION, 0, 0, 0,   /* (the host's header: the cost word as tools/build.py writes it from tools/module/wave.cost) */
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_WAVE; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), WAVE_API_VERSION, FW_API_ADDR,
    ENGI_WAVE, sizeof(wv_part_t), (uint32_t)(uintptr_t)&ENG_WAVE, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
