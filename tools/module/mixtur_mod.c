/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's mixtur, synth_sdk/plugins/mixtur/mixtur.c and mixtur_tables.h (https://github.com/Hundekraft/op1r-src, commit bf808cfa, a fork
 * of pcppcp/op1r-src), Copyright (C) 2026 ultralazr (Musica Obscura #1), GPL-3.0-or-later with the OP-1 firmware permission (op1r's NOTICE), taken
 * here under GPL-3.0. The copies in tools/module/op1r/plugins/mixtur/ are op1r's: mixtur_tables.h unchanged, mixtur.c up to its screen code (the
 * OP-1's display, left out) with its file-scope array own[] taken out (the line marked Orange Cat, and a note at its top).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): mixtur (a subharmonic organ after Oskar Sala's
 * Mixtur-Trautonium: the key is the top note; four sawtooth dividers sound its undertones f/n, n = 1..24, phase-locked to one master phase, with one
 * polyBLEP at the master's wrap; two resonant formant band-passes in parallel; a soft knee) as a loadable module translation unit, built to measure
 * it (stage 0: feasibility). The proposed engine number is 29 (core.h ENGI_MIXTUR; registrable only in an OC_CANDIDATES build).
 *
 * The plugin runs through tools/module/op1r/op1_glue.h (its header says what op1r's firmware gives a plugin and what the glue puts there). Here:
 *   - EDIT 1 (DIV: A B C D) are op1r's four knobs (params[0..3], x 258): each divider's undertone, off and 1..24 in 25 steps of the knob (five
 *     of our 128 values a step); EDIT 2 (FORM: F1 F2 RES) its second page's words (params[5..7]): formant 1 180..4070 Hz, formant 2 500..5660 Hz,
 *     the resonance Q 0.7..22; 0 is op1r's "unset", which mixtur reads as 8192 (392 Hz, 917 Hz, Q 3.2), as on the OP-1. P_E7 unused;
 *   - its Blackfin MACs are under #ifdef __bfin__ in op1r's file, with op1r's portable C beside them: the JieLi build and the host take the C;
 *   - its own clamp(x, m) has the name of dsp.c's clamp(x, lo, hi): renamed (mx_clamp) while its file is included, the file unchanged there;
 *   - its file-scope own[6] (a signature per voice telling its own state from another engine's) is the part's: mixtur_part_t below, through the
 *     glue's OP1G_PART_STATE (a module has no RAM of its own; each track has its own);
 *   - presets: 0 is the plugin.json's defaults (1966, 3277, 4588, 7209 -> 8, 13, 18, 28: undertones 1, 2, 3, 5); 1 to 3 are Orange Cat's;
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default (2 a voice).
 * Per sample it reads no table (DIV_D at a master wrap only; the formant curves once a block): no hot set (nhot 0). Its divisions are per block
 * (one per audible divider, and the polyBLEP's reciprocal), as op1r's rules ask; its sample loops use a 256-byte stack buffer (xbuf).
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

typedef struct {
    uint32_t mx_own[6];                    /* mixtur.c's own[OP1_VOICES] (op1r: file scope; the glue checks the length below) */
} mixtur_part_t;
#define OP1G_PART_STATE mixtur_part_t
#include "op1r/op1_glue.h"
#define own (OP1G_OWN(v)->mx_own)          /* (each of mixtur's uses names its voice object v: adopt's argument, or self cast) */
#define clamp mx_clamp
#include "op1r/plugins/mixtur/mixtur.c"
#undef clamp
#undef own
_Static_assert(sizeof(mx_voice_t) <= OP1_VOICE_SIZE, "mixtur's voice is op1r's 0x24 bytes");
_Static_assert(sizeof(((mixtur_part_t *)0)->mx_own) == OP1_VOICES * sizeof(uint32_t), "own[] is OP1_VOICES long");
OP1G_ENGINE_FUNCS(ENGINE_mixtur)

static const preset_t MIXTUR_PRESETS[] = {
    {"MIXTUR", {8, 13, 18, 28, 0, 0, 0, 0}, {0, 80, 40, 60}, 0, 0},       /* the plugin's defaults: undertones 1 2 3 5, formants unset */
    {"SUBHARM", {8, 13, 23, 33, 20, 30, 60, 0}, {0, 72, 30, 70}, 0, 0},   /* 1 2 4 6, a darker formant pair, more Q */
    {"VOWEL O", {8, 18, 0, 0, 12, 18, 90, 0}, {0, 64, 90, 50}, 0, 0},     /* 1 3, low F1 and F2: an "o" */
    {"BIRDS", {8, 48, 63, 98, 70, 90, 110, 0}, {0, 60, 0, 40}, 0, 1},     /* 1 9 12 19, bright and resonant, MONO (Hitchcock's Birds) */
};
static const engine_t ENG_MIXTUR = {
    .name = "MIXTUR", .page_title = {"DIV", "FORM"},
    .edit = {
        {"DIVA", F_INT, 0, 127, 8, 0, 0},       /* op1r's knob 0: undertone (k x 25) >> 15 of the knob, 0 off, 1..24 */
        {"DIVB", F_INT, 0, 127, 13, 0, 0},
        {"DIVC", F_INT, 0, 127, 18, 0, 0},
        {"DIVD", F_INT, 0, 127, 28, 0, 0},
        {"F1", F_INT, 0, 127, 0, 0, 0},         /* op1r's page word 5: 0 unset (392 Hz) */
        {"F2", F_INT, 0, 127, 0, 0, 0},
        {"RES", F_INT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = MIXTUR_PRESETS, .npresets = NELEM(MIXTUR_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MIXTUR, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_MIXTUR; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MIXTUR, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_MIXTUR, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
