/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's dddd, synth_sdk/plugins/dddd/dddd.c and synth_sdk/dddd/dd_core.c, dd_core.h (https://github.com/Hundekraft/op1r-src, commit
 * bf808cfa, a fork of pcppcp/op1r-src), Copyright (C) 2026 pcppcp, GPL-3.0-or-later with the OP-1 firmware permission (op1r's NOTICE), taken here
 * under GPL-3.0. The copies in tools/module/op1r/ are op1r's: dd_core.c and dd_core.h unchanged, dddd.c up to its screen code (the OP-1's display,
 * left out) with its file-scope state taken out (the lines marked Orange Cat, below).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): dddd (harsh noise, paraphonic: one wall for every key of the
 * part; four sources (noise, crackle, a logistic map, three ring-modulated squares) into a feedback loop with a tap per held key, a state-variable
 * filter, fold, clip and crush, random "hands" and a stutter, a DC blocker and a limiter) as a loadable module translation unit, built to measure it
 * (stage 0: feasibility). The proposed engine number is 26 (core.h ENGI_DDDD; registrable only in an OC_CANDIDATES build).
 *
 * The plugin runs through tools/module/op1r/op1_glue.h (what op1r's firmware gives a plugin and what the glue puts there: its header). Here:
 *   - EDIT 1 (WALL: SRC FDBK DSTR HAND) are op1r's four knobs (params[0..3], x 258); EDIT 2 (PAGE 2: FILT SPRD CRSH) its second page's words
 *     (params[5..7]: filter LP -> BP -> HP, the loop spread, the rate crush). op1r turns SRC and FILT in eight steps (plugin.json "coarse", a
 *     selector's feel on the OP-1's encoder); here they are 0..127 like the others: the engine reads them as continuous values either way;
 *   - its file-scope state (the wall dd_t with its 4,096-sample loop, the render count and note order, the taps' increments, the screen's level) is
 *     the part's: dddd_part_t below, in the glue's part block (OP1G_PART_STATE), reached through self (OP1G_OWN), so each track has its own wall
 *     and the module no RAM of its own. Its one helper that has no self, alive(v), is a macro in the copy (marked);
 *   - the leader: the lowest voice index that has rendered within the last seven render calls renders the wall, the others silence (op1r's rule,
 *     unchanged: a voice counts while Orange Cat renders it, so a voice held at SUS 0 and skipped stops counting, as a finished release in op1r);
 *   - presets: 0 is the plugin.json's defaults (0, 24000, 20000, 8000 -> 0, 93, 78, 31; the second page 0); 1 to 3 are Orange Cat's;
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default (2 a voice), though only the leader costs a wall: see the gate's table.
 * Per sample it reads no table (the loop is the part's RAM; per_c and inv_n, 60 B, once a block or a key): no hot set (nhot 0). Its divisions are
 * per block or per key change (dd_set_incs, the limiter's gain) and one per stutter cut (g % 1800), as op1r's rules ask.
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

#include "op1r/dddd/dd_core.c"
typedef struct {                           /* dddd.c's file-scope state in op1r, a part's here (its fields keep op1r's names: the macros below) */
    dd_t dd;
    uint32_t calls, last_call[6], order[6], order_n;
    uint32_t voice_inc[6], taps_inc[DD_TAPS];
    int taps_n;
    uint8_t inited, alive_seen[6];
    int32_t level;                         /* (op1r's screen's view of the wall: written, read by nothing here) */
} dddd_part_t;
#define OP1G_PART_STATE dddd_part_t
#include "op1r/op1_glue.h"
_Static_assert(sizeof(((dddd_part_t *)0)->last_call) == OP1_VOICES * sizeof(uint32_t), "dddd's per-voice arrays are OP1_VOICES long");
#define dd (OP1G_OWN(self)->dd)
#define calls (OP1G_OWN(self)->calls)
#define last_call (OP1G_OWN(self)->last_call)
#define order (OP1G_OWN(self)->order)
#define order_n (OP1G_OWN(self)->order_n)
#define voice_inc (OP1G_OWN(self)->voice_inc)
#define taps_inc (OP1G_OWN(self)->taps_inc)
#define taps_n (OP1G_OWN(self)->taps_n)
#define inited (OP1G_OWN(self)->inited)
#define alive_seen (OP1G_OWN(self)->alive_seen)
#define level (OP1G_OWN(self)->level)
#include "op1r/plugins/dddd/dddd.c"
#undef dd
#undef calls
#undef last_call
#undef order
#undef order_n
#undef voice_inc
#undef taps_inc
#undef taps_n
#undef inited
#undef alive_seen
#undef level
OP1G_ENGINE_FUNCS(ENGINE_dddd)

static const preset_t DDDD_PRESETS[] = {
    {"WALL", {0, 93, 78, 31, 0, 0, 0, 0}, {0, 64, 127, 30}, 0, 0},       /* the plugin's defaults */
    {"CRACKLE", {43, 60, 40, 20, 20, 0, 0, 0}, {0, 64, 127, 40}, 0, 0},  /* the crackle source, a low-pass, calm hands */
    {"SCREAM", {127, 115, 100, 60, 64, 30, 0, 0}, {0, 64, 127, 36}, 0, 0}, /* metal, feedback past 80 %, band-pass */
    {"STUTTER", {85, 90, 70, 110, 100, 60, 40, 0}, {0, 64, 127, 30}, 0, 0}, /* the hands at the top (stutter), crushed */
};
static const engine_t ENG_DDDD = {
    .name = "DDDD", .page_title = {"WALL", "PAGE 2"},
    .edit = {
        {"SRC", F_INT, 0, 127, 0, 0, 0},        /* noise -> crackle -> chaos -> metal */
        {"FDBK", F_PCT, 0, 127, 93, 0, 0},
        {"DSTR", F_PCT, 0, 127, 78, 0, 0},
        {"HAND", F_PCT, 0, 127, 31, 0, 0},
        {"FILT", F_INT, 0, 127, 0, 0, 0},       /* LP -> BP -> HP */
        {"SPRD", F_PCT, 0, 127, 0, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = DDDD_PRESETS, .npresets = NELEM(DDDD_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_DDDD, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_DDDD; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_DDDD, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_DDDD, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
