/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.14.0: WHEEL as a loadable engine module (engine 7, the number Melodee's WHEEL always had: core.h ENGI_WHEEL). WHEEL is Melodee's own tonewheel
 * organ (firmware/src/eng_wheel.c, Copyright Kerem Kilic (Ellic Studio), portions Leo Kuroshita (@kurogedelic), Hügelton Instruments), retired from the image in
 * Melodee 0.1.x and kept in the tree behind MELODEE_LEGACY_EXTRAS. This file is the module's translation unit, built the way shapes_mod.c and prophet_mod.c build
 * theirs: the engine's source is included as it is; the firmware's services are the table's members (fw_api.h): trk, resource_get, memcpy and memset.
 *
 * What is the module's own and what is the firmware's:
 *   - its state: drw_part_t (the part's rotor and bar gains and the eight voices' wheels), one arena block a track through the table's resource_get
 *     (engines.c's drw_of, moved here); the header's state_size is its size;
 *   - its tables: DRW_LV, DRW_REG, DRW_FOLD_LIM, DRW_ROT_SPD and the names, all read once a block (wheel_block, the partial loop's set-up), so they stay in
 *     the module's flash;
 *   - the tables it reads per sample are the firmware's own hot set (dsp.c, 0.14.0): SINE (each partial, every sample) and TANH_Q15 (the drive's softclip).
 *     SINE is redefined below to the firmware's copy (fw_api_t base_hot), so the module carries neither table and declares no hot set of its own (nhot 0):
 *     docs/orangecat/ENGINE_CANDIDATES.md, "Wheel".
 * Its pages are the firmware's EDIT 1 and EDIT 2 for its eight controls (engine_t page_title BARS and TONE, edit[]): the controls are the track's P_E0..P_E7,
 * which the firmware's own engine pages show and edit for any engine, so the module brings no fw_pages_t descriptor (that format is for native parameters
 * beyond the eight, the Prophet's patch). The drawbar graph of the legacy build (ui_graph.c graph_wheel) is not drawn for the module.
 *
 * Revisions:
 *   0.14.0: first version.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u    /* the module's own version, 1.0.0 (the header's version word) */
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST       /* the host build (tests/module_host_wheel_tu.c): libc has memcpy and memset, so the module's own go by other names */
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
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }   /* dsp.c: SINE and TANH_Q15, the firmware's (its hot set) */

#define trk (OC_FW()->trk)
#define tuning_log() (OC_FW()->tuning_log())
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

#define SINE DSP_SINE                   /* the partials' sine: the firmware's hot copy, not a table in the module area */
#include "eng_wheel.c"
#undef SINE
static drw_part_t *drw_of(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(drw_part_t)); }

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_WHEEL, sizeof(drw_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_WHEEL; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_WHEEL, sizeof(drw_part_t), (uint32_t)(uintptr_t)&ENG_WHEEL, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
#endif
