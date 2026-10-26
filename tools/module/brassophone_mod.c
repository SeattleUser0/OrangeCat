/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's brassophone, synth_sdk/plugins/brassophone/brass.c and warp_table.h (https://github.com/Hundekraft/op1r-src, commit bf808cfa, a
 * fork of pcppcp/op1r-src), Copyright (C) 2026 nick (nvmsocool), GPL-3.0-or-later with the OP-1 firmware permission (op1r's NOTICE), taken here under
 * GPL-3.0. The copies in tools/module/op1r/plugins/brassophone/ are op1r's: warp_table.h unchanged, brass.c with its sine and warp-table reads
 * changed (the lines marked Orange Cat, and a note at its top); its screen, horn.c, is not copied.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): brassophone (a sine whose phase is warped, more as the
 * envelope rises, toward a spread, brassy wave; a pitch bend driven by the envelope; a vibrato that fades in once the envelope holds still; a
 * one-pole low-pass) as a loadable module translation unit, built to measure it (stage 0: feasibility). The proposed engine number is 28 (core.h
 * ENGI_BRASS; registrable only in an OC_CANDIDATES build).
 *
 * The plugin runs through tools/module/op1r/op1_glue.h (its header says what op1r's firmware gives a plugin and what the glue puts there). Here:
 *   - EDIT 1 (HORN: STRT SWP VIBD VIBR) are op1r's four knobs (params[0..3], x 258): the warp's start, the envelope's sweep, the vibrato depth and
 *     rate. brassophone has no second page; EDIT 2 is empty;
 *   - its two sine reads (the oscillator through the warped phase, the vibrato) are OP1_SINE, here op1r's own sine (OP1G_EXACT_SINE: a quarter
 *     wave, 4,098 B, the hot set's second table): with the glue's default (SINE[1025] interpolated, 1 LSB off) the vibrato's sine moved the pitch
 *     enough that the renders drifted to -58 dB RMS of op1r's at C6 (tests/op1r_parity), past the tolerance;
 *   - its warp table (warp_table[64][257], 32,896 B, read four times a sample) is read as BRASS_WARP4 (tools/module/op1r/brass_warp4.h, from
 *     gen_brass_warp4.py: every fourth column and the last again, 64 x 66, 8,448 B), the module's hot set, a column between its two neighbours'
 *     linear interpolation: at most 6 LSB of the Q15 warped phase from op1r's (ENGINE_CANDIDATES.md gives what that does to the sound). The full
 *     table would be 32.9 KB of arena (half of it) or per-sample flash reads; the tests/op1r_parity build that checks the glue reads op1r's own;
 *   - it reads OP1_VOICE_ENV's last value per block as its timbre and vibrato source (the glue's: the voice's amplitude at the block, so its own
 *     ramp from the block before to this one is what it is on the OP-1) and calls note_off (the glue's, at the release's first block);
 *   - presets: 0 is the plugin.json's defaults (19660, 16384, 3932, 7536 -> 76, 64, 15, 29); 1 to 3 are Orange Cat's;
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default (2 a voice).
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

#define OP1G_ENGINE ENGI_BRASS
#define OP1G_EXACT_SINE 1                  /* op1r's own sine: the hot set's second table (op1_glue.h) */
#include "op1r/op1_glue.h"
#ifdef OP1G_PARITY_SINE
#define BRASS_WARP(s, c) ((int32_t)warp_table[(s)][(c)])   /* tests/op1r_parity's glue check: op1r's own table (warp_table.h, included by brass.c) */
#else
#include "op1r/brass_warp4.h"
static const eng_hot_t BRASS_HOT[2] = { { BRASS_WARP4, sizeof BRASS_WARP4 }, { OP1G_SINE_Q, sizeof OP1G_SINE_Q } };
/* warp_table[s][c] for c = 0..256: the hot set's copy (ENG_HOT: the arena), columns 4 apart, interpolated */
static inline int32_t brass_warp(int32_t s, int32_t c)
{
    const int16_t *r = (const int16_t *)ENG_HOT(0, ENGI_BRASS, 0) + s * 66 + (c >> 2);
    return r[0] + (((r[1] - r[0]) * (c & 3) + 2) >> 2);
}
#define BRASS_WARP(s, c) brass_warp((s), (c))
#endif
#include "op1r/plugins/brassophone/brass.c"
_Static_assert(sizeof(brass_voice_t) <= OP1_VOICE_SIZE, "brassophone's voice is op1r's 0x24 bytes");
OP1G_ENGINE_FUNCS(ENGINE_brass)

static const preset_t BRASS_PRESETS[] = {
    {"BRASS", {76, 64, 15, 29, 0, 0, 0, 0}, {6, 80, 100, 40}, 0, 0},      /* the plugin's defaults */
    {"MUTED", {110, 30, 10, 25, 0, 0, 0, 0}, {10, 70, 90, 50}, 0, 0},     /* a narrow sweep from near the sine */
    {"BLARE", {20, 120, 30, 40, 0, 0, 0, 0}, {2, 60, 110, 30}, 0, 0},     /* the whole sweep: bright at the peak */
    {"HORN LD", {60, 90, 60, 45, 0, 0, 0, 0}, {14, 90, 120, 50}, 0, 1},   /* a slow attack, deep vibrato; MONO */
};
static const engine_t ENG_BRASS = {
    .name = "BRASS", .page_title = {"HORN", "-"},
    .edit = {
        {"STRT", F_PCT, 0, 127, 76, 0, 0},      /* op1r's blue: the warp's start (up = sine) */
        {"SWP", F_PCT, 0, 127, 64, 0, 0},
        {"VIBD", F_PCT, 0, 127, 15, 0, 0},
        {"VIBR", F_PCT, 0, 127, 29, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = BRASS_PRESETS, .npresets = NELEM(BRASS_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
#ifndef OP1G_PARITY_SINE
    .hot = BRASS_HOT, .nhot = 2,
#endif
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_BRASS, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_BRASS; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_BRASS, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_BRASS, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
