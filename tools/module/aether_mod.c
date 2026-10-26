/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: op1r's aether, synth_sdk/plugins/aether/aether.c (https://github.com/Hundekraft/op1r-src, commit bf808cfa, a fork of pcppcp/op1r-src),
 * by pcppcp (its plugin.json's author; the file has no copyright line of its own), GPL-3.0-or-later with the OP-1 firmware permission (op1r's
 * NOTICE), taken here under GPL-3.0. The copy in tools/module/op1r/plugins/aether/ is op1r's file up to its screen code (the OP-1's display, left
 * out), with its accelerometer, its file-scope state and its sine reads changed (the lines marked Orange Cat, and a note at its top).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): aether (a tilt theremin: the key sets the note, tilting
 * the OP-1 forward or back bends it, raising its right side fades it, shaking it adds vibrato; a sine with even harmonics and a soft drive) as a
 * loadable module translation unit, built to measure it (stage 0: feasibility). The proposed engine number is 27 (core.h ENGI_AETHER; registrable
 * only in an OC_CANDIDATES build).
 *
 * The FM-1 has no accelerometer. aether reads the OP-1's (an ADXL345, X Y Z, 1 g = 16384) through AE_ACCEL; the copy reads the part's stand-in
 * instead, three words the glue fills from the track at each note-on and each block (OP1G_PARAMS_HOOK):
 *   - X (the volume tilt, right side up) 0 and Z 16384: the unit lying flat, as op1r's harness has it. The volume tilt (its red knob) has nothing to
 *     act on, so it does nothing;
 *   - Y (the pitch tilt, front edge up is negative) from the TILT control, P_E7 (-64..63; 0 level): Y = -TILT x 256, TILT 32 about 30 degrees
 *     forward. aether takes the angle at note-on as home, so TILT bends a note when it moves after the note started: by hand on EDIT 2, or by the
 *     modulation matrix (an LFO or the mod wheel on E8, once a block), and its speed is what aether's shake detector hears (the vibrato).
 * Otherwise through tools/module/op1r/op1_glue.h as the other op1r modules:
 *   - EDIT 1 (TILT: RNG TIMB VIB VOL) are op1r's four knobs (params[0..3], x 258): the pitch range at full tilt (1..24 semitones), the timbre, the
 *     shake vibrato's depth, the volume tilt; EDIT 2 (HOME: DEAD FULL RATE TILT) its second page's words (params[5..7]: the dead zone around home,
 *     the tilt of the full range, the vibrato's rate; 0 = op1r's unset: 2.5 degrees, 30 degrees, 4.25 Hz) and the stand-in's TILT;
 *   - its per-voice state (op1r: a file-scope ae_t st[6], past the voice's 32 bytes) and the screen's last voice are the part's: aether_part_t;
 *   - its three sine reads (the tilt angle's sine, per block; the vibrato, per block; the oscillator, per sample) are OP1_SINE (SINE through the
 *     base hot set);
 *   - presets: 0 is the plugin.json's defaults (15671, 11469, 16384, 32767 -> 61, 44, 64, 127); 1 to 3 are Orange Cat's;
 *   - voices: OP1_VOICES 6 (poly 6); units the firmware's default (2 a voice). aether is meant MONO (legato, portamento: the stock play modes).
 * Per sample it reads no table but SINE: no hot set (nhot 0). Its divisions are per block, as op1r's rules ask.
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
    int16_t accel[3];                      /* the accelerometer's stand-in: X, Y (from TILT), Z */
    int16_t ae_cur;                        /* aether.c's ae_cur (op1r: file scope; the screen's) */
    uint32_t st[102];                      /* aether.c's ae_t st[OP1_VOICES] (op1r: file scope; its size checked below) */
} aether_part_t;
#define OP1G_PART_STATE aether_part_t
#define OP1G_PARAMS_HOOK(P, t) ((P)->own.accel[0] = 0, (P)->own.accel[1] = (int16_t)(-clamp((t)->p[P_E7], -64, 63) * 256), (P)->own.accel[2] = 16384)
#include "op1r/op1_glue.h"
#define st ((ae_t *)OP1G_OWN(self)->st)
#define ae_cur (OP1G_OWN(self)->ae_cur)
#include "op1r/plugins/aether/aether.c"
#undef st
#undef ae_cur
_Static_assert(sizeof(((aether_part_t *)0)->st) == OP1_VOICES * sizeof(ae_t), "the part's st[] is aether's ae_t st[OP1_VOICES]");
OP1G_ENGINE_FUNCS(ENGINE_aether)

static const preset_t AETHER_PRESETS[] = {
    {"AETHER", {61, 44, 64, 127, 0, 0, 0, 0}, {6, 80, 110, 50}, 0, 1},    /* the plugin's defaults; MONO */
    {"THEREMIN", {64, 40, 80, 0, 0, 0, 40, 0}, {10, 80, 120, 60}, 0, 1},   /* 12 semitones at full tilt, a little reed, shake vibrato */
    {"REEDY", {32, 110, 40, 0, 20, 0, 0, 0}, {4, 70, 100, 40}, 0, 0},      /* the timbre up: even harmonics and drive */
    {"SIREN", {127, 64, 0, 0, 0, 30, 0, 0}, {2, 80, 127, 30}, 0, 1},       /* 24 semitones at a 25 degree tilt: put an LFO on E8 (TILT) */
};
static const engine_t ENG_AETHER = {
    .name = "AETHER", .page_title = {"TILT", "HOME"},
    .edit = {
        {"RNG", F_INT, 0, 127, 61, 0, 0},       /* op1r's knob 0: the pitch range at full tilt, 1 .. 24 semitones */
        {"TIMB", F_PCT, 0, 127, 44, 0, 0},
        {"VIB", F_PCT, 0, 127, 64, 0, 0},
        {"VOL", F_PCT, 0, 127, 127, 0, 0},        /* the volume tilt: no X axis here (the stand-in lies flat) */
        {"DEAD", F_INT, 0, 127, 0, 0, 0},       /* op1r's page word 5: 0 unset (2.5 degrees) */
        {"FULL", F_INT, 0, 127, 0, 0, 0},
        {"RATE", F_INT, 0, 127, 0, 0, 0},
        {"TILT", F_BIPCT, -64, 63, 0, 0, 0},    /* the stand-in for the OP-1's forward tilt (Y): 0 level, 32 about 30 degrees */
    },
    .presets = AETHER_PRESETS, .npresets = NELEM(AETHER_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E7}, .poly = OP1_VOICES,
    .note_on = op1g_eng_note_on, .render = op1g_eng_render, .keep = 0,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_AETHER, sizeof(op1g_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_AETHER; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_AETHER, sizeof(op1g_part_t), (uint32_t)(uintptr_t)&ENG_AETHER, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
