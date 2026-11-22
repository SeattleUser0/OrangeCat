/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's supersaw, synth_sdk/plugins/supersaw/supersaw.c (https://github.com/Hundekraft/op1r-src, commit bf808cfa, a fork of
 * pcppcp/op1r-src), by pcppcp (its plugin.json's author; the file has no copyright line of its own), GPL-3.0-or-later with the OP-1 firmware
 * permission (op1r's NOTICE), taken here under GPL-3.0. The copy in tools/module/op1r/plugins/supersaw/ is op1r's file up to its screen code (the
 * OP-1's display, left out), with two lines marked Orange Cat (its sine, below).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): supersaw (two detuned saws morphing to a pulse, a sine sub an
 * octave down, a drive into a cubic soft clip and a one-pole low-pass) as a loadable module translation unit, built to measure it (stage 0:
 * feasibility). The proposed engine number is 25 (core.h ENGI_SUPERSAW; registrable only in an OC_CANDIDATES build).
 *
 * The plugin runs through tools/module/op1r/op1_glue.h (what op1r's firmware gives a plugin and what the glue puts there: its header). Here:
 *   - EDIT 1 (SAW: DTUN MRPH SUB DRIV) are op1r's four knobs (params[0..3], x 258); supersaw has no second page, so EDIT 2 is empty;
 *   - its sub reads OP1_SINE (the base hot set's SINE, interpolated) instead of op1r's 8192-point table: the two lines marked Orange Cat;
 *   - its voice type is named voice_t, as Orange Cat's own: the name is ss_voice_t while its file is included (the macro below), nothing else;
 *     it is op1r's 36 bytes over voice_t's ph[2] and s[8]; the part's state is the glue's (op1g_part_t): supersaw keeps no file-scope state of its
 *     own outside its screen;
 *   - presets: 0 is the plugin.json's defaults (8000, 0, 12000, 6000 -> 31, 0, 47, 23); 1 to 3 are Orange Cat's (supersaw has none of its own);
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default for an engine without a units hook (2 a voice).
 * Per sample it reads no table but SINE (the base hot set): no hot set of its own (nhot 0).
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a measurement, not a release */
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
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

#define OP1G_LEVEL_SLOT 7                  /* (engines-b) P_E7 is its LEVL (op1_glue.h): 64 the plugin's own level */
#include "op1r/op1_glue.h"
#define voice_t ss_voice_t                 /* op1r's supersaw names its voice voice_t: Orange Cat's (core.h) is the other one */
#include "op1r/plugins/supersaw/supersaw.c"
#undef voice_t
_Static_assert(sizeof(ss_voice_t) <= OP1_VOICE_SIZE, "supersaw's voice is op1r's 0x24 bytes");
OP1G_ENGINE_FUNCS(ENGINE_supersaw)

static const preset_t SUPERSAW_PRESETS[] = {
    {"SUPERSAW", {31, 0, 47, 23, 0, 0, 0, 66}, {4, 60, 110, 50}, 0, 0},   /* the plugin's defaults; (engines-b) LEVL 66 */
    {"SAW PAD", {100, 0, 30, 10, 0, 0, 0, 70}, {70, 90, 112, 92}, 0, 0},   /* wide detune, a slow swell; (engines-b) LEVL 70 */
    {"SQR LEAD", {20, 127, 60, 60, 0, 0, 0, 86}, {0, 60, 100, 30}, 0, 1},   /* the morph at pulse, MONO; (engines-b) LEVL 86 */
    {"DRIVE BAS", {10, 40, 110, 127, 0, 0, 0, 94}, {0, 70, 90, 24}, 0, 1},   /* the sub up, the drive full, MONO; (engines-b) LEVL 94 */
    {"TRANCE LEAD", {60, 0, 20, 40, 0, 0, 0, 84}, {0, 70, 110, 34}, 0, 1},   /* (engines-b) a detuned mono lead */
    {"SAW KEYS", {40, 10, 30, 15, 0, 0, 0, 87}, {0, 62, 40, 40}, 0, 0},   /* (engines-b) a short chord sound */
    {"SAW PLUCK", {30, 0, 20, 30, 0, 0, 0, 113}, {0, 60, 0, 40}, 0, 0},   /* (engines-b) plucked saws */
    {"SUB BASS", {0, 100, 127, 20, 0, 0, 0, 99}, {0, 60, 110, 20}, 0, 1},   /* (engines-b) the pulse and the sub, MONO */
    {"HOOVER", {127, 60, 90, 80, 0, 0, 0, 75}, {10, 80, 120, 50}, 0, 1},   /* (engines-b) wide, morphed, driven: a texture, MONO */
};
/* (engines-b) the page descriptor (fw_pages_t version 3): the rows, the pid table, HOME's four macro knobs (tools/module/cand_pages.h) */
#include "cand_pages.h"
static const engine_t ENG_SUPERSAW;
static const page_t SUPERSAW_ROWS[] = {CAND_ROW("SAW", 0, 1, 2, 3), CAND_ROW("OUT", 7, 0xFF, 0xFF, 0xFF)};
/* (engines-b) HOME's four knobs: WIDTH the detune (and the sub a little down with it: the knob audit heard detune alone weakly on a mono bass), MRPH the saw-to-pulse morph, SUB the sine an octave down, DRIVE the clip (and a little sub with it) */
static const fw_kdef_t SUPERSAW_KDEF[4] = {
    CAND_MACRO("WIDTH", CAND_T(0, CAND_LIN, 127), CAND_T(2, CAND_LIN, -40), CAND_T0, CAND_T0),
    CAND_MACRO("MRPH", CAND_T(1, CAND_LIN, 127), CAND_T0, CAND_T0, CAND_T0),
    CAND_MACRO("SUB", CAND_T(2, CAND_LIN, 127), CAND_T0, CAND_T0, CAND_T0),
    CAND_MACRO("DRIVE", CAND_T(3, CAND_LIN, 127), CAND_T(2, CAND_UP, 30), CAND_T0, CAND_T0),
};
CAND_PAGES(SUPERSAW, ENG_SUPERSAW, SUPERSAW_ROWS, 8, SUPERSAW_KDEF)
static const engine_t ENG_SUPERSAW = {
    .name = "SUPERSAW", .page_title = {"SAW", "OUT"},
    .edit = {
        {"DTUN", F_PCT, 0, 127, 31, 0, 0},
        {"MRPH", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 47, 0, 0},
        {"DRIV", F_PCT, 0, 127, 23, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"LEVL", F_INT, 0, 127, 64, 0, 0},     /* (engines-b) the module's level: (v / 64)^3, 64 the plugin's own */
    },
    .pages = &SUPERSAW_PAGES,
    .presets = SUPERSAW_PRESETS, .npresets = NELEM(SUPERSAW_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_SUPERSAW, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_SUPERSAW; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_SUPERSAW, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_SUPERSAW, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
