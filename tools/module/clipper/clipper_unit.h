/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): what every Clipper module unit (tools/module/<name>_mod.c)
 * starts with, once: the firmware's headers and dsp.c, the services table (fw_api.h: the arena, memcpy, memset, the base hot set), then the Felucca and
 * Clipper pieces Orange Cat's dsp.c does not have (caravel/felucca_osc.h: osc_saw, osc_pulse; clipper/clipper_dsp.h: the ladder and the rest Clipper
 * added) and Clipper's preset format (clipper/clipper_preset.h). The unit then includes the engine's copy (clipper/eng_<name>.c) with preset_t
 * defined as cl_preset_t, writes its engine_t in Orange Cat's field order (Clipper's is Felucca's, positional), and ends with CL_MODULE_HEADER.
 *
 * Clipper's engines and Orange Cat's: the same voice_t fields (ph[3], s[8], env, env_out, note, vel, pitch16), the same vmod_t (inc, amp0 and amp1,
 * cutoff, shape, pitch16, envq15), the same VOICE_FS, amp_at, mulq15, softclip (TANH_Q15), SVF_G and its generator's formula (tools/gen_tables.py,
 * Clipper's and Orange Cat's: the same line), blep, osc_saw, osc_pulse. Clipper's voice_amp and soft_knee are its engines.c's, Orange Cat's dsp.c's: the
 * same text. What differs is said where a module meets it.
 */
#ifndef CLIPPER_UNIT_H
#define CLIPPER_UNIT_H 1
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
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }
#include "../caravel/felucca_osc.h"
#include "clipper_dsp.h"
#include "clipper_preset.h"
#undef XP
#define XP(...) .x = {0}                    /* the extras name Clipper's track parameters, some of which Orange Cat has not got; they are dropped (cl_preset_make) */
#define OC_STATE_TOKEN 4u                   /* the header's state_size for an engine whose state is all in voice_t: the loader refuses 0 */

/* the module's header: engine number, its engine_t, the state a part takes */
#ifdef OC_MODULE_HOST
#define CL_MODULE_HEADER(engi, eng, state)                                                                         \
    const fw_mod_hdr_t OC_MODULE_HDR = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,  \
                                        (engi), (state), 0, 0, OC_PACK_HASH, 0, OC_MODULE_VERSION, 0, 0, 0};       \
    const fw_api_t *oc_host_fw;                                                                                    \
    const engine_t *oc_host_module_engine(void) { return &(eng); }
#else
#define CL_MODULE_HEADER(engi, eng, state)                                                                         \
    extern char _modstart[];                                                                                       \
    __attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {                                 \
        FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,                                 \
        (engi), (state), (uint32_t)(uintptr_t)&(eng), (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,                \
        0, OC_MODULE_VERSION, 0, 0, 0};
#endif
#endif
