/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's arc, synth_sdk/plugins/arc/arc.c and arc_tables.h (https://github.com/Hundekraft/op1r-src, commit bf808cfa, a fork of
 * pcppcp/op1r-src), Copyright (C) 2026 ultralazr (Musica Obscura #1), GPL-3.0-or-later with the OP-1 firmware permission (op1r's NOTICE), taken here
 * under GPL-3.0. The copies in tools/module/op1r/plugins/arc/ are op1r's: arc_tables.h unchanged, arc.c up to its screen code (the OP-1's display,
 * left out) with its file-scope state taken out and its G table decoded at build time (the lines marked Orange Cat, and a note at its top).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): arc (Duddell's singing arc, 1899: each voice a Van der Pol
 * oscillator, a "lamp", integrated by semi-implicit Euler in its own time so the waveform holds across the keyboard, tuned by a measured 2-D table;
 * a second coupled lamp at a ratio; plasma hiss; a strike with pitch sag and random sputter; carbon asymmetry, a DC blocker, a key-tracked tilt)
 * as a loadable module translation unit, built to measure it (stage 0: feasibility). The proposed engine number is 30 (core.h ENGI_ARC; registrable
 * only in an OC_CANDIDATES build).
 *
 * The plugin runs through tools/module/op1r/op1_glue.h (its header says what op1r's firmware gives a plugin and what the glue puts there). Here:
 *   - EDIT 1 (LAMP: CURR CARB HISS STRK) are op1r's four knobs (params[0..3], x 258): current mu (sine -> reed -> relaxation), carbon, hiss,
 *     strike; EDIT 2 (PAIR: RATIO COUPL BRITE) its second page's words (params[5..7]): the second lamp's ratio 1/2..2 with detents, the coupling
 *     (0: the second lamp off), the brightness (0 = op1r's unset: neutral). P_E7 unused;
 *   - its Blackfin MACs are under #ifdef __bfin__ in op1r's file, with op1r's portable C beside them: the JieLi build and the host take the C;
 *   - its G table (arc_gp, 33 x 97 uint16) is decoded by tools/module/op1r/gen_arc_gp.py into arc_gp.h, in the module's flash (6,402 B, read once
 *     a block by lamp_setup), instead of decoded at run time into a file-scope RAM array (op1r: by the screen, or on first use): a module has no
 *     RAM, and the functions that read it have no voice object to reach the part through. The compressed form (ARC_GPZ, 2,859 B) is then unread;
 *   - the screen's shared state (the phase-portrait trace, 256 x 4 int16, its position and voice; each voice's level), written by render and read
 *     by nothing here, is the part's: arc_part_t below (2,064 B a track: kept so that render runs as op1r's does; five marked lines would drop it);
 *   - its noise generator is seeded at a note-on from the voice object's address when its word is 0 (op1r's arithmetic, kept: here the address in
 *     the arena);
 *   - presets: 0 is the plugin.json's defaults (12000, 6000, 5000, 9000 -> 47, 23, 19, 35); 1 to 3 are Orange Cat's;
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default (2 a voice).
 * Per sample it reads no table: no hot set (nhot 0). Its divisions are per block (the level, the portrait's scale, the ratio), as op1r's rules ask.
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

typedef struct {                           /* arc.c's screen state (op1r: file scope), the part's; written by render, read by nothing here */
    int16_t trace[256][4];
    uint16_t tpos;
    uint8_t tvoice;
    int16_t level[6];
} arc_part_t;
#define OP1G_PART_STATE arc_part_t
#include "op1r/op1_glue.h"
#define arc_trace (OP1G_OWN(self)->trace)
#define arc_tpos (OP1G_OWN(self)->tpos)
#define arc_tvoice (OP1G_OWN(self)->tvoice)
#define arc_level (OP1G_OWN(self)->level)
#include "op1r/plugins/arc/arc.c"
#undef arc_trace
#undef arc_tpos
#undef arc_tvoice
#undef arc_level
_Static_assert(sizeof(arc_voice_t) == OP1_VOICE_SIZE, "arc's voice is op1r's 0x24 bytes");
_Static_assert(sizeof(((arc_part_t *)0)->trace) == ARC_TN * 4 * sizeof(int16_t) && sizeof(((arc_part_t *)0)->level) == OP1_VOICES * sizeof(int16_t),
               "the part's copies of arc's arrays have op1r's lengths");
OP1G_ENGINE_FUNCS(ENGINE_arc)

static const preset_t ARC_PRESETS[] = {
    {"ARC", {47, 23, 19, 35, 0, 0, 0, 0}, {0, 80, 110, 50}, 0, 0},        /* the plugin's defaults */
    {"REED", {80, 10, 8, 20, 0, 0, 0, 0}, {2, 72, 100, 40}, 0, 0},        /* more current: a reedy buzz, clean */
    {"BEATING", {40, 30, 20, 30, 64, 20, 64, 0}, {4, 80, 110, 60}, 0, 0}, /* the second lamp near 1:1, weak coupling: beats */
    {"SCREAM", {127, 90, 70, 110, 100, 127, 100, 0}, {0, 64, 127, 30}, 0, 1},   /* relaxation, strong coupling (chaos), sputter; MONO */
};
static const engine_t ENG_ARC = {
    .name = "ARC", .page_title = {"LAMP", "PAIR"},
    .edit = {
        {"CURR", F_PCT, 0, 127, 47, 0, 0},      /* op1r's knob 0: current mu, eps 0.04 .. 7 */
        {"CARB", F_PCT, 0, 127, 23, 0, 0},
        {"HISS", F_PCT, 0, 127, 19, 0, 0},
        {"STRK", F_PCT, 0, 127, 35, 0, 0},
        {"RATI", F_INT, 0, 127, 0, 0, 0},       /* op1r's page word 5: 0 unset (1:1) */
        {"COUP", F_PCT, 0, 127, 0, 0, 0},
        {"BRIT", F_INT, 0, 127, 0, 0, 0},       /* 0 unset (neutral, as 32) */
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = ARC_PRESETS, .npresets = NELEM(ARC_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_ARC, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_ARC; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_ARC, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_ARC, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
