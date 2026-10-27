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

#include "op1r/op1_glue.h"
#define voice_t ss_voice_t                 /* op1r's supersaw names its voice voice_t: Orange Cat's (core.h) is the other one */
#include "op1r/plugins/supersaw/supersaw.c"
#undef voice_t
_Static_assert(sizeof(ss_voice_t) <= OP1_VOICE_SIZE, "supersaw's voice is op1r's 0x24 bytes");
OP1G_ENGINE_FUNCS(ENGINE_supersaw)

static const preset_t SUPERSAW_PRESETS[] = {
    {"SUPERSAW", {31, 0, 47, 23, 0, 0, 0, 0}, {4, 60, 110, 50}, 0, 0},     /* the plugin's defaults */
    {"SAW PAD", {100, 0, 30, 10, 0, 0, 0, 0}, {70, 90, 112, 92}, 0, 0},   /* wide detune, a slow swell */
    {"SQR LEAD", {20, 127, 60, 60, 0, 0, 0, 0}, {0, 60, 100, 30}, 0, 1},  /* the morph at pulse, MONO */
    {"DRIVE BAS", {10, 40, 110, 127, 0, 0, 0, 0}, {0, 70, 90, 24}, 0, 1}, /* the sub up, the drive full, MONO */
};
static const engine_t ENG_SUPERSAW = {
    .name = "SUPERSAW", .page_title = {"SAW", "-"},
    .edit = {
        {"DTUN", F_PCT, 0, 127, 31, 0, 0},
        {"MRPH", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 47, 0, 0},
        {"DRIV", F_PCT, 0, 127, 23, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
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
