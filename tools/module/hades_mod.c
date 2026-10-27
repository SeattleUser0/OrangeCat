/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Caravel's firmware/src/eng_hades.c and va_blocks.c (https://github.com/bonszaimusic/caravel, commit 4a1770c0851f83acdfeb07b9c40a1dfc2b0c3dcc,
 * GPL-3.0-only, copyright bonszaimusic), unchanged copies in tools/module/caravel/; its two oscillators from Felucca-Engines' dsp.c (MIT, Copyright (C) 2026 Leo
 * Kuroshita (@kurogedelic), Hügelton Instruments; tools/module/caravel/felucca_osc.h).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "HADES"): HADES (Caravel's Hades-RE-inspired bass voice: saw or pulse and two divider subs into a three-pole ladder, a bass shelf and a drive) as a loadable module translation unit, on the
 * Prophet's pattern, built to measure it (stage 0: feasibility). The proposed engine number is 22 (core.h ENGI_HADES; registrable only in an OC_CANDIDATES
 * build, see core.h).
 *
 * Caravel's engine_t is Felucca's, and Orange Cat's extends it (alloc, legato, mono_key, post, units, cap, preset_make, pages, hot), so the engine file is
 * included as it is. What Orange Cat's types do not have, and how the unit bridges it:
 *   - osc_saw and osc_pulse: Felucca's polyBLEP oscillators, which Orange Cat's dsp.c dropped (felucca_osc.h, unchanged); blep, osc_tri, mulq15, mulq16,
 *     clamp, softclip, soft_knee, noise32, tsvf_coef_k, cents_inc, voice_amp and the tables SINE, SVF_G, TANH_Q15 are Orange Cat's dsp.c and tables, the
 *     same text and values (softclip and soft_knee read TANH_Q15 through the firmware's base hot set);
 *   - the presets' FX(..) and PAT(..): Felucca's effect sends and suggested pattern, fields Orange Cat's preset_t does not have (its effects are the
 *     track's). They expand to 0 here: two excess initializers a preset, which C drops;
 * Pages: EDIT 1 (OSC SUB: WAVE PW SUB1 SUB2) and EDIT 2 (FLT DRV: CUT RES DRV BODY), the engine_t's own eight controls.
 * The engine keeps its whole state in voice_t (s[8], ph[]): no arena block of its own;
 * the header's state_size is OC_STATE_TOKEN, 4 bytes nothing reads, because the loader refuses 0 (oc_module.c, a sanity check of the header). Its own tables are its presets and names; per sample it reads
 * no table but TANH_Q15 (softclip, through soft_knee and va_drive), which is the firmware's base hot set: no hot set of its own (nhot 0). SVF_G and PITCH_OCT are read
 * once a block (the filter's coefficients, the detune). Voices: poly 0 (NPOLY, 8); units the firmware's default for an engine without a units hook (2 a voice).
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

#include "caravel/felucca_osc.h"
#define OC_STATE_TOKEN 4u                  /* the header's state_size: the loader refuses 0; the engine keeps its state in voice_t */
#define FX(d, c, dl, r) 0                  /* Felucca's effect sends: not in Orange Cat's preset_t (see above) */
#define PAT(n) 0                           /* Felucca's suggested pattern: the same */
#include "caravel/va_blocks.c"
#include "caravel/eng_hades.c"
#undef FX
#undef PAT

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_HADES, OC_STATE_TOKEN, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_HADES; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_HADES, OC_STATE_TOKEN, (uint32_t)(uintptr_t)&ENG_HADES, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
