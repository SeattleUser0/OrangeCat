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
    /* bass */
    {"WAVE BASS", {60, 20, 0, 0, 62, 30, 0, 60}, {0, 64, 110, 20}, 0, 1},
    {"SUB WOBBLE", {10, 0, 0, 0, 50, 60, 0, 90}, {0, 64, 110, 20}, 63, 1},
    {"PULSE BASS", {40, 0, 0, 0, 60, 30, 0, 50}, {0, 64, 100, 20}, 63, 1},
    {"GROWL BASS", {20, 60, 0, 0, 64, 52, 10, 40}, {0, 70, 100, 24}, 63, 1},
    {"SYNC BASS", {70, 40, 20, 0, 70, 30, 0, 40}, {0, 60, 90, 20}, 63, 1},
    /* lead */
    {"WAVE LEAD", {70, 50, 20, 0, 96, 30, 30, 0}, {0, 64, 120, 40}, 0, 1},
    {"SYNC LEAD", {60, 40, 30, 0, 90, 20, 0, 0}, {0, 64, 120, 35}, 0, 1},
    {"TALK LEAD", {30, 0, 0, 0, 96, 30, 15, 0}, {0, 64, 120, 40}, 63, 1},
    {"SCREAM SYNC", {80, 0, 40, 0, 88, 60, 0, 0}, {0, 64, 120, 40}, 63, 1},
    /* keys */
    {"WAVE KEYS", {30, 40, 0, 0, 72, 20, 15, 20}, {0, 72, 50, 50}, 0, 0},
    {"GLASS KEYS", {100, 20, 0, 0, 100, 10, 10, 0}, {0, 76, 30, 60}, 63, 0},
    {"WAVE TINE", {0, 0, 0, 0, 96, 10, 0, 30}, {0, 80, 20, 64}, 63, 0},
    {"VOWEL KEYS", {30, 0, 0, 0, 84, 20, 10, 0}, {0, 72, 50, 50}, 63, 0},
    {"CLAV WAVE", {100, 0, 0, 0, 72, 50, 0, 0}, {0, 64, 20, 34}, 63, 0},
    /* pad */
    {"WAVE PAD", {60, 30, 0, 0, 92, 20, 50, 0}, {70, 90, 115, 95}, 0, 0},
    {"PWM STRINGS", {30, 50, 0, 0, 90, 10, 40, 0}, {50, 80, 110, 80}, 0, 0},
    {"MORPH PAD", {20, 60, 0, 0, 86, 20, 50, 0}, {80, 90, 115, 95}, 63, 0},
    {"CHOIR TABLE", {10, 40, 0, 0, 88, 20, 40, 0}, {70, 90, 115, 95}, 63, 0},
    {"GLASS PAD", {0, 30, 0, 0, 96, 10, 30, 0}, {60, 90, 115, 100}, 63, 0},
    {"PWM PAD", {30, 50, 0, 0, 84, 20, 40, 0}, {70, 90, 115, 90}, 63, 0},
    {"DARK DRONE", {10, 20, 0, 0, 60, 60, 30, 40}, {90, 90, 120, 100}, 63, 0},
    /* pluck */
    {"WAVE PLUCK", {40, 30, 0, 0, 60, 45, 20, 0}, {0, 66, 0, 50}, 0, 0},
    {"PULSE PLUCK", {10, 0, 0, 0, 78, 30, 15, 0}, {0, 70, 0, 48}, 63, 0},
    {"WAVE MALLET", {0, 0, 0, 0, 100, 10, 0, 0}, {0, 70, 0, 56}, 63, 0},
    {"VOWEL PLUCK", {20, 0, 0, 0, 66, 50, 10, 0}, {0, 70, 0, 48}, 63, 0},
    /* texture */
    {"VOX MORPH", {64, 64, 30, 60, 70, 70, 40, 0}, {40, 90, 110, 90}, 0, 0},
    {"S&H SWARM", {40, 40, 0, 0, 90, 30, 80, 0}, {20, 90, 110, 80}, 63, 0},
    {"WIND TABLE", {0, 0, 0, 0, 80, 90, 0, 0}, {60, 90, 120, 90}, 63, 0},
    {"METAL SCAN", {60, 50, 60, 30, 92, 30, 40, 0}, {10, 90, 110, 90}, 63, 0},
    {"FORMANT SWEEP", {0, 60, 0, 0, 62, 30, 30, 0}, {40, 90, 115, 90}, 63, 0},
    /* (wave-bank) the user tables of the installer page (the sample slot's room): POS A and POS B sweep them; without a bank they play BASIC */
    {"USER TABLE A", {0, 0, 0, 0, 100, 10, 20, 0}, {0, 64, 110, 40}, 0, 0},
    {"USER TABLE B", {0, 64, 0, 0, 100, 10, 30, 0}, {10, 70, 110, 60}, 0, 0},
    /* (November 23) the owner's maybes, after the rest: reworked for round 3 (docs/candidates/wave_engine.md 5) */
    {"ACID WAVE", {100, 0, 0, 0, 44, 96, 0, 0}, {0, 64, 100, 20}, 63, 1},  /* maybe (round 3) */
    {"MORPH LEAD", {40, 60, 0, 0, 92, 30, 25, 0}, {0, 64, 120, 40}, 63, 1},  /* maybe (round 3) */
    {"SQUARE LEAD", {0, 20, 0, 0, 94, 20, 20, 0}, {0, 64, 120, 40}, 63, 1},  /* maybe (round 3) */
    {"HARP TABLE", {80, 0, 0, 0, 80, 20, 10, 0}, {0, 74, 0, 58}, 63, 0},  /* maybe (round 3) */
};
#define S(src, dst, amt) {WS_##src, WD_##dst, (uint8_t)((amt) + (int)WV_AMT0), 0}
#define S0 {0, 0, WV_AMT0, 0}
#define OC(x) (uint8_t)((x) + (int)WV_OCT0)
#define SE(x) (uint8_t)((x) + (int)WV_SEMI0)
static const wv_patch_t WV_PATCH[] = {
    /* tab, warp, uni, oct, semi, lvl, noise, mode, kbd, ENV 2 {A D S R}, LFO rates, LFO shapes, the four slots, the stereo width, the level (the render analysis) */
    /* bass */
    {{0, 1}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(-1)}, {SE(0), SE(0)}, {127, 60}, 0, WF_LADDER, 64, {0, 70, 20, 40}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 50), S(VEL, CUT, 20), S0, S0}, 0, 87},   /* WAVE BASS */
    {{0, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LADDER, 64, {0, 70, 40, 40}, {62, 60}, {4, 0},
     {S(LFO1, CUT, 55), S0, S0, S0}, 0, 100},   /* SUB WOBBLE */
    {{1, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LP, 64, {0, 62, 10, 30}, {60, 60}, {0, 0},
     {S(ENV2, POSA, 70), S(ENV2, CUT, 40), S0, S0}, 0, 89},   /* PULSE BASS */
    {{2, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(-1)}, {SE(0), SE(0)}, {120, 60}, 0, WF_LADDER, 64, {0, 70, 40, 40}, {50, 60}, {0, 0},
     {S(LFO1, POSA, 60), S(ENV2, CUT, 40), S0, S0}, 0, 127},   /* GROWL BASS */
    {{0, 3}, {WARP_SYNC, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 40}, 0, WF_LP, 64, {0, 60, 10, 30}, {60, 60}, {0, 0},
     {S(ENV2, WARPA, 90), S(ENV2, CUT, 30), S0, S0}, 0, 75},   /* SYNC BASS */
    /* lead */
    {{0, 3}, {WARP_BEND, WARP_BEND}, {4, 2}, {OC(0), OC(0)}, {SE(0), SE(0)}, {110, 70}, 0, WF_LP, 64, {0, 60, 80, 40}, {70, 50}, {0, 0},
     {S(LFO1, POSA, 20), S(ENV2, CUT, 30), S0, S0}, 90, 65},   /* WAVE LEAD */
    {{0, 0}, {WARP_SYNC, WARP_BEND}, {1, 1}, {OC(0), OC(1)}, {SE(0), SE(0)}, {120, 40}, 0, WF_LP, 64, {0, 70, 30, 40}, {60, 60}, {0, 0},
     {S(ENV2, WARPA, 80), S(ENV2, CUT, 25), S0, S0}, 0, 67},   /* SYNC LEAD */
    {{2, 0}, {WARP_BEND, WARP_BEND}, {2, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LP, 64, {0, 70, 40, 40}, {46, 66}, {4, 0},
     {S(LFO1, POSA, 90), S(ENV2, CUT, 20), S0, S0}, 0, 91},   /* TALK LEAD */
    {{0, 0}, {WARP_SYNC, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LADDER, 64, {0, 70, 40, 40}, {40, 60}, {0, 0},
     {S(LFO1, WARPA, 50), S(ENV2, WARPA, 40), S0, S0}, 0, 96},   /* SCREAM SYNC */
    /* keys */
    {{2, 0}, {WARP_BEND, WARP_BEND}, {1, 2}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 50}, 0, WF_LP, 100, {0, 72, 40, 50}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 40), S(VEL, CUT, 20), S0, S0}, 40, 77},   /* WAVE KEYS */
    {{3, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(1)}, {SE(0), SE(0)}, {120, 60}, 0, WF_LP, 90, {0, 74, 0, 60}, {60, 60}, {0, 0},
     {S(ENV2, POSA, -80), S(VEL, POSA, 30), S0, S0}, 0, 87},   /* GLASS KEYS */
    {{0, 3}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(2)}, {SE(0), SE(0)}, {120, 30}, 0, WF_LP, 64, {0, 66, 0, 50}, {60, 60}, {0, 0},
     {S(ENV2, WARPA, 70), S(ENV2, LVLB, 60), S(VEL, WARPA, 30), S0}, 0, 86},   /* WAVE TINE */
    {{2, 0}, {WARP_BEND, WARP_BEND}, {2, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LP, 80, {0, 70, 40, 40}, {60, 60}, {0, 0},
     {S(VEL, POSA, 60), S(ENV2, CUT, 30), S0, S0}, 0, 78},   /* VOWEL KEYS */
    {{1, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {127, 0}, 0, WF_LP, 110, {0, 56, 0, 30}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 50), S(VEL, CUT, 20), S0, S0}, 0, 111},   /* CLAV WAVE */
    /* pad */
    {{0, 2}, {WARP_BEND, WARP_BEND}, {4, 4}, {OC(0), OC(0)}, {SE(0), SE(0)}, {100, 80}, 0, WF_LP, 40, {80, 90, 100, 90}, {25, 18}, {0, 0},
     {S(LFO1, POSB, 40), S(LFO2, POSA, 25), S0, S0}, 110, 63},   /* WAVE PAD */
    {{1, 1}, {WARP_BEND, WARP_BEND}, {4, 4}, {OC(0), OC(0)}, {SE(0), SE(0)}, {110, 70}, 0, WF_LP, 40, {50, 80, 110, 80}, {28, 22}, {0, 0},
     {S(LFO1, POSA, 50), S(LFO2, POSB, 50), S0, S0}, 110, 61},   /* PWM STRINGS */
    {{0, 0}, {WARP_BEND, WARP_BEND}, {4, 4}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 90}, 0, WF_LP, 64, {0, 70, 40, 40}, {22, 16}, {0, 0},
     {S(LFO1, POSA, 60), S(LFO2, POSB, 50), S0, S0}, 110, 56},   /* MORPH PAD */
    {{2, 2}, {WARP_BEND, WARP_BEND}, {4, 3}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 80}, 0, WF_LP, 64, {0, 70, 40, 40}, {18, 24}, {0, 0},
     {S(LFO1, POSA, 70), S(LFO2, POSB, 60), S0, S0}, 100, 70},   /* CHOIR TABLE */
    {{3, 0}, {WARP_BEND, WARP_BEND}, {3, 2}, {OC(0), OC(1)}, {SE(0), SE(0)}, {120, 70}, 0, WF_LP, 64, {100, 90, 127, 90}, {60, 60}, {0, 0},
     {S(ENV2, POSA, 90), S(LFO1, POSB, 30), S0, S0}, 110, 64},   /* GLASS PAD */
    {{1, 1}, {WARP_BEND, WARP_BEND}, {4, 4}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 100}, 0, WF_LP, 64, {0, 70, 40, 40}, {28, 22}, {0, 0},
     {S(LFO1, POSA, 60), S(LFO2, POSB, 60), S0, S0}, 110, 58},   /* PWM PAD */
    {{0, 0}, {WARP_BEND, WARP_BEND}, {3, 1}, {OC(0), OC(-1)}, {SE(0), SE(0)}, {120, 70}, 0, WF_LADDER, 64, {0, 70, 40, 40}, {20, 30}, {3, 0},
     {S(LFO1, CUT, 30), S(LFO2, POSA, 30), S0, S0}, 90, 73},   /* DARK DRONE */
    /* pluck */
    {{3, 1}, {WARP_BEND, WARP_BEND}, {2, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 40}, 0, WF_LP, 80, {0, 62, 0, 45}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 60), S(ENV2, POSA, 40), S0, S0}, 60, 94},   /* WAVE PLUCK */
    {{1, 0}, {WARP_BEND, WARP_BEND}, {2, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LP, 80, {0, 56, 0, 40}, {60, 60}, {0, 0},
     {S(ENV2, POSA, 90), S(ENV2, CUT, 50), S0, S0}, 0, 108},   /* PULSE PLUCK */
    {{0, 3}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(2)}, {SE(0), SE(7)}, {120, 50}, 0, WF_LP, 64, {0, 54, 0, 40}, {60, 60}, {0, 0},
     {S(ENV2, WARPA, 60), S(ENV2, LVLB, 50), S0, S0}, 0, 92},   /* WAVE MALLET */
    {{2, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {127, 0}, 0, WF_LP, 100, {0, 62, 0, 40}, {60, 60}, {0, 0},
     {S(ENV2, POSA, 80), S(ENV2, CUT, 30), S0, S0}, 0, 127},   /* VOWEL PLUCK */
    /* texture */
    {{2, 3}, {WARP_BEND, WARP_MIRROR}, {3, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {110, 70}, 10, WF_BP, 40, {40, 90, 110, 90}, {40, 15}, {3, 0},
     {S(LFO1, POSA, 60), S(LFO2, WARPB, 40), S0, S0}, 100, 70},   /* VOX MORPH */
    {{3, 2}, {WARP_BEND, WARP_BEND}, {6, 4}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 80}, 10, WF_LP, 64, {0, 70, 40, 40}, {70, 40}, {3, 3},
     {S(LFO1, POSA, 90), S(LFO2, POSB, 70), S0, S0}, 127, 59},   /* S&H SWARM */
    {{0, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {30, 0}, 120, WF_BP, 64, {0, 70, 40, 40}, {20, 30}, {0, 0},
     {S(LFO1, CUT, 50), S(LFO2, RES, 20), S0, S0}, 60, 97},   /* WIND TABLE */
    {{3, 3}, {WARP_MIRROR, WARP_SYNC}, {3, 3}, {OC(0), OC(0)}, {SE(0), SE(7)}, {120, 80}, 0, WF_LP, 64, {0, 70, 40, 40}, {30, 20}, {0, 0},
     {S(LFO1, WARPA, 60), S(LFO2, POSB, 60), S0, S0}, 110, 59},   /* METAL SCAN */
    {{2, 2}, {WARP_BEND, WARP_BEND}, {3, 3}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 70}, 0, WF_BP, 64, {0, 70, 40, 40}, {16, 26}, {1, 0},
     {S(LFO1, POSA, 120), S(LFO2, POSB, -60), S0, S0}, 100, 127},   /* FORMANT SWEEP */
    /* USER TABLE A: bank table 0 on both oscillators (B an octave up, quieter); B: bank tables 0 and 1 */
    {{WV_NTAB + 0, WV_NTAB + 0}, {WARP_BEND, WARP_BEND}, {2, 1}, {OC(0), OC(1)}, {SE(0), SE(0)}, {120, 40}, 0, WF_LP, 60, {0, 70, 100, 40}, {60, 60}, {0, 0},
     {S(VEL, CUT, 20), S0, S0, S0}, 60, 64},   /* USER TABLE A */
    {{WV_NTAB + 0, WV_NTAB + 1}, {WARP_BEND, WARP_BEND}, {2, 2}, {OC(0), OC(0)}, {SE(0), SE(0)}, {110, 90}, 0, WF_LP, 40, {0, 70, 100, 50}, {30, 20}, {0, 0},
     {S(LFO1, POSA, 30), S(LFO2, POSB, 30), S0, S0}, 90, 64},   /* USER TABLE B */
    /* maybe */
    {{0, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LADDER, 40, {0, 58, 0, 30}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 70), S(VEL, CUT, 30), S0, S0}, 0, 127},   /* ACID WAVE */
    {{0, 3}, {WARP_BEND, WARP_BEND}, {3, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 60}, 0, WF_LP, 64, {0, 70, 40, 40}, {30, 66}, {0, 0},
     {S(LFO1, POSA, 60), S(LFO2, PITCH, 2), S0, S0}, 0, 68},   /* MORPH LEAD */
    {{1, 1}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(1)}, {SE(0), SE(0)}, {120, 80}, 0, WF_LP, 64, {0, 70, 40, 40}, {60, 66}, {0, 0},
     {S(LFO1, POSA, 25), S(LFO2, PITCH, 2), S0, S0}, 0, 58},   /* SQUARE LEAD */
    {{3, 0}, {WARP_BEND, WARP_BEND}, {1, 1}, {OC(0), OC(0)}, {SE(0), SE(0)}, {120, 0}, 0, WF_LP, 100, {0, 66, 0, 40}, {60, 60}, {0, 0},
     {S(ENV2, CUT, 70), S(ENV2, POSA, 30), S0, S0}, 0, 127},   /* HARP TABLE */
};
#undef S
#undef S0
#undef OC
#undef SE
_Static_assert(NELEM(WV_PATCH) == NELEM(WAVE_PRESETS), "a patch a preset");

/* (M3) the pages (fw_pages_t version 3): the native block (eng_wave.c wv_native_t) is the panel, byte i entry i. Entries 0..7 are the eight edit values
 * (the engine's edit[], pids 1..8 with their legacy slots P_E0..P_E7: HOME's macro knobs name them); the firmware's EDIT 1 and EDIT 2 show and edit those,
 * so no row of WAVE's names them. Entries 8.. are the patch, a byte each, every one stored with a user preset (mod_rec.h). A signed value is F_OFS (0 at the
 * middle of its range: OCT, SEMI, a slot's AMT); the bank's tables are USER 1..16 (wave-bank: without a bank they play BASIC). */
#include "cand_pages.h"
static const engine_t ENG_WAVE;
#define WVO(f) ((uint8_t)(WV_NB_EV + __builtin_offsetof(wv_patch_t, f)))   /* a patch field's panel index */
enum {
    WP_TABA = WVO(tab[0]), WP_TABB, WP_WARPA, WP_WARPB, WP_UNIA, WP_UNIB, WP_OCTA, WP_OCTB, WP_SEMIA, WP_SEMIB, WP_LVLA, WP_LVLB, WP_NOISE, WP_MODE, WP_KBD,
    WP_E2A, WP_E2D, WP_E2S, WP_E2R, WP_LRATE1, WP_LRATE2, WP_LSHAPE1, WP_LSHAPE2, WP_SLOT0, WP_WIDTH = WP_SLOT0 + 16, WP_LEVEL, WP_N
};
_Static_assert(WP_TABA == 8 && WP_LSHAPE2 == WVO(lshape[1]) && WP_SLOT0 == WVO(slot) && WP_WIDTH == WVO(width) && WP_LEVEL == WVO(level) &&
               WP_N == sizeof(wv_native_t), "the panel indices are the native block's bytes");
static const char *const WN_TAB[] = {"BASIC", "PULSE", "VOWEL", "BRIGHT", "USER 1", "USER 2", "USER 3", "USER 4", "USER 5", "USER 6", "USER 7", "USER 8",
                                     "USER 9", "USER 10", "USER 11", "USER 12", "USER 13", "USER 14", "USER 15", "USER 16"};
_Static_assert(NELEM(WN_TAB) == WV_NTAB + WVB_NTAB, "a name a table");
static const char *const WN_WARP[] = {"BEND", "SYNC", "MIRROR"};
static const char *const WN_MODE[] = {"LP", "BP", "HP", "NOTCH", "LADDER"};
static const char *const WN_LSHAPE[] = {"TRI", "SAW", "SQUARE", "S&H", "TRI KEY", "SAW KEY", "SQR KEY", "S&H KEY"};   /* KEY: restarted at the note */
static const char *const WN_SRC[] = {"ENV 2", "LFO 1", "LFO 2", "VELO", "KEY"};
static const char *const WN_DST[] = {"POS A", "POS B", "WARP A", "WARP B", "CUT", "RES", "PITCH", "LVL A", "LVL B"};
_Static_assert(NELEM(WN_SRC) == WS_N && NELEM(WN_DST) == WD_N, "a name a source, a destination");
#define WD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}
#define WE(l, n, df) {l, F_ENUM, 0, (int16_t)(NELEM(n) - 1), df, n, 0}
#define WSLOT {"SRC", F_ENUM, 0, WS_N - 1, 0, WN_SRC, 0}, {"DEST", F_ENUM, 0, WD_N - 1, 0, WN_DST, 0}, {"AMT", F_OFS, 0, 2 * WV_AMT0, WV_AMT0, 0, 0}, {0, 0, 0, 0, 0, 0, 0}
static const param_desc_t WV_PANEL[WP_N] = {
    {"POSA", F_PCT, 0, 127, 64, 0, 0}, {"POSB", F_PCT, 0, 127, 64, 0, 0}, {"WRPA", F_PCT, 0, 127, 0, 0, 0}, {"WRPB", F_PCT, 0, 127, 0, 0, 0},
    {"CUT", F_PCT, 0, 127, 90, 0, 0}, {"RES", F_PCT, 0, 127, 20, 0, 0}, {"DTUN", F_PCT, 0, 127, 20, 0, 0}, {"SUB", F_PCT, 0, 127, 0, 0, 0},
    WE("TAB A", WN_TAB, 0), WE("TAB B", WN_TAB, 1), WE("WARP A", WN_WARP, 0), WE("WARP B", WN_WARP, 0),
    WD("UNI A", F_INT, 1, WV_UNI, 1), WD("UNI B", F_INT, 1, WV_UNI, 1),
    {"OCT A", F_OFS, 0, 2 * WV_OCT0, WV_OCT0, 0, 0}, {"OCT B", F_OFS, 0, 2 * WV_OCT0, WV_OCT0, 0, 0},
    {"SEMI A", F_OFS, 0, 2 * WV_SEMI0, WV_SEMI0, 0, "st"}, {"SEMI B", F_OFS, 0, 2 * WV_SEMI0, WV_SEMI0, 0, "st"},
    WD("LVL A", F_PCT, 0, 127, 120), WD("LVL B", F_PCT, 0, 127, 60), WD("NOISE", F_PCT, 0, 127, 0), WE("FILTER", WN_MODE, WF_LP), WD("KEYTRK", F_PCT, 0, 127, 64),
    WD("ATTACK", F_PCT, 0, 127, 0), WD("DECAY", F_PCT, 0, 127, 70), WD("SUSTN", F_PCT, 0, 127, 40), WD("RELEAS", F_PCT, 0, 127, 40),
    WD("RATE 1", F_PCT, 0, 127, 60), WD("RATE 2", F_PCT, 0, 127, 60), WE("SHAPE1", WN_LSHAPE, 0), WE("SHAPE2", WN_LSHAPE, 0),
    WSLOT, WSLOT, WSLOT, WSLOT,
    WD("WIDTH", F_PCT, 0, 127, 0), WD("LEVEL", F_INT, 0, 127, 64),
};
#undef WD
#undef WE
#undef WSLOT
/* the stable ids: 1..8 the edit values (as every candidate's, cand_pages.h), 100 + the byte for the patch (never reused; a slot's spare byte keeps its id) */
#define WPID(i) {(uint16_t)(100u + (i)), FW_PID_NONE}
#define WPID4(i) WPID(i), WPID(i + 1), WPID(i + 2), WPID(i + 3)
static const fw_pid_t WV_PID[WP_N] = {
    {1, P_E0}, {2, P_E1}, {3, P_E2}, {4, P_E3}, {5, P_E4}, {6, P_E5}, {7, P_E6}, {8, P_E7},
    WPID4(8), WPID4(12), WPID4(16), WPID4(20), WPID4(24), WPID4(28), WPID4(32), WPID4(36), WPID4(40), WPID4(44), WPID(48),
};
#undef WPID
#undef WPID4
_Static_assert(NELEM(WV_PID) == WP_N, "a pid an entry");
#define WROW(title, a, b, c, d) {title, FAM_EDIT, SC_MOD, GR_NONE, {a, b, c, d}}
#define WMOD(title, k) WROW(title, WP_SLOT0 + 4 * (k), WP_SLOT0 + 4 * (k) + 1, WP_SLOT0 + 4 * (k) + 2, 0xFF)
static const page_t WAVE_ROWS[] = {
    WROW("WAVE OSC A", WP_TABA, WP_WARPA, WP_UNIA, WP_LVLA), WROW("WAVE OSC B", WP_TABB, WP_WARPB, WP_UNIB, WP_LVLB),
    WROW("WAVE TUNE", WP_OCTA, WP_SEMIA, WP_OCTB, WP_SEMIB),  WROW("WAVE FILTER", WP_MODE, WP_KBD, WP_NOISE, WP_LEVEL),
    WROW("WAVE ENV 2", WP_E2A, WP_E2D, WP_E2S, WP_E2R),       WROW("WAVE LFO", WP_LRATE1, WP_LSHAPE1, WP_LRATE2, WP_LSHAPE2),
    WMOD("WAVE MOD 1", 0), WMOD("WAVE MOD 2", 1), WMOD("WAVE MOD 3", 2), WMOD("WAVE MOD 4", 3),
    WROW("WAVE STEREO", WP_WIDTH, 0xFF, 0xFF, 0xFF),
};
#undef WROW
#undef WMOD
static const fw_kdef_t WAVE_KDEF[4] = {
    CAND_MACRO("MORPH", CAND_T(0, CAND_LIN, 127), CAND_T(1, CAND_LIN, 90), CAND_T0, CAND_T0),
    CAND_MACRO("WARP", CAND_T(2, CAND_UP, 127), CAND_T(3, CAND_UP, 80), CAND_T(4, CAND_LIN, -30), CAND_T0),
    CAND_MACRO("TONE", CAND_T(4, CAND_LIN, 127), CAND_T(5, CAND_UP, 50), CAND_T0, CAND_T0),
    CAND_MACRO("WIDE", CAND_T(6, CAND_UP, 127), CAND_T(7, CAND_DOWN, -60), CAND_T0, CAND_T0),
};
static const fw_ui_t WAVE_UI = {FW_UI_MAGIC, FW_UI_WIRE, 1, 0, 4, 0, 0, WAVE_KDEF};
static wv_part_t *wave_part_held(const track_t *t);
/* the track's native block: its part's, while WAVE plays on the track (never another engine's state, during a change); 0 otherwise. From the UI the
 * firmware's resource_get gives only a state already held (engines.c fw_resource_get): nothing is taken outside the audio interrupt */
static uint8_t *wave_native(track_t *t)
{
    wv_part_t *S = wave_part_held(t);
    return S ? (uint8_t *)&S->nb : 0;
}
/* a factory preset was loaded on the track (ui.c preset_apply): its patch, when the part is there (else the part takes it when it is: wave_part) */
static void wave_loaded(track_t *t, uint32_t preset)
{
    wv_part_t *S = wave_part_held(t);
    if (S) S->nb.p = WV_PATCH[preset < NELEM(WV_PATCH) ? preset : 0u];
}
/* drawn dim: WIDTH on a firmware without the right lane; a slot's DEST and AMT while its AMT is 0 */
static int wave_active(const track_t *t, uint32_t id)
{
    wv_part_t *S;
    if (id == WP_WIDTH) return wave_stereo_fw();
    if (id < WP_SLOT0 || id >= WP_SLOT0 + 16u || (id - WP_SLOT0) % 4u == 2u || !(S = wave_part_held(t))) return 1;
    return S->nb.p.slot[(id - WP_SLOT0) / 4u].amt != WV_AMT0;
}
static const fw_pages_t WAVE_PAGES = {
    .magic = FW_PAGES_MAGIC, .size = FW_PAGES_WIRE, .version = FW_PAGES_VERSION,
    .npages = (uint16_t)NELEM(WAVE_ROWS), .npanel = WP_N, .rows = WAVE_ROWS, .panel = WV_PANEL,
    .native = wave_native, .active = wave_active, .loaded = wave_loaded,
    .knob = {0, 1, 2, 3}, .cc = {0xFF, 0xFF, 0xFF, 0xFF},
    .ui = &WAVE_UI, .pid = WV_PID, .ext = 0};
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
/* the track's part; a new one (its magic not there) starts from the factory patch t->preset (a user preset's patch is given by the firmware right after:
 * voice.c mod_ext_apply) */
static wv_part_t *wave_part(const track_t *t)
{
    wv_part_t *P = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(wv_part_t));
    if (P && P->magic != WV_MAGIC) {
        memset(P, 0, sizeof *P);
        P->nb.p = WV_PATCH[t->preset < NELEM(WV_PATCH) ? t->preset : 0u];
        for (uint32_t i = 0; i < WV_NV; i++) P->v[i].cached[0] = P->v[i].cached[1] = ~0u;
        P->magic = WV_MAGIC;
    }
    return P;
}
/* .. only while WAVE is the track's engine, the one it plays and the one asked for (in a change the state may be the old engine's) */
static wv_part_t *wave_part_held(const track_t *t)
{
    return t->engine == ENGI_WAVE && t->eng_req == ENGI_WAVE ? wave_part(t) : 0;
}
/* the patch the track plays (the audio side: note_on, render; its part is there); the factory one when there is none */
static const wv_patch_t *wave_patch(const track_t *t)
{
    wv_part_t *S = wave_part(t);
    return S ? &S->nb.p : &WV_PATCH[t->preset < NELEM(WV_PATCH) ? t->preset : 0u];
}
/* .. for a voice-unit estimate, which may be asked from the UI (admission): the part only while WAVE plays on the track */
static const wv_patch_t *wave_patch_held(const track_t *t)
{
    wv_part_t *S = wave_part_held(t);
    return S ? &S->nb.p : &WV_PATCH[t->preset < NELEM(WV_PATCH) ? t->preset : 0u];
}

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
