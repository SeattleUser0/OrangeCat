/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO2 (candidate; docs/candidates/macro2.md) as a loadable engine module under the proposed number ENGI_MACRO2 (32, an OC_CANDIDATES build's):
 * the ten Plaits models MACRO lacks (firmware/src/eng_macro2.c, the ports in firmware/src/plaits/), built the way wheel_mod.c and shapes_mod.c build
 * theirs: the engine's source is included as it is; the firmware's services are the table's members (fw_api.h): trk, resource_get, tuning_log, the
 * pack's ready flag, the batch queue, memcpy and memset, and the hot set's copies (ENG_HOT through fw_api_t.hot).
 *
 * What is the module's own and what is the firmware's:
 *   - its state: mc2_part_t, one arena block a track (the header's state_size);
 *   - its tables: the ten models' own (plaits/pl_tables2.h: the 192 waves, the three DX7 banks, the five word banks, the LPC pulse, the DX unit
 *     tables) in the module's image; Plaits' shared tables (pl_lut_sine, the pitch ratio tables: plaits/pl_tables.h) in the asset pack, as MACRO's;
 *   - its hot set (eng_macro2.c MC2_HOT_LIST): the tables read per sample, copied into the arena at registration, read through the table (or none:
 *     MACRO2_HOT=0, the A/B for the unit).
 * Its pages are the firmware's EDIT 1 and EDIT 2 for its eight controls (MACRO's), so it brings no fw_pages_t descriptor. The kernel pushes to the
 * batch under AP_MACRO (the firmware's id: a new id would move FW_API_VERSION).
 *
 * Revisions:
 *   macro2-candidate: first version.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a candidate */
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
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }   /* dsp.c: SINE and TANH_Q15, the firmware's (its hot set) */

#define trk (OC_FW()->trk)
#define tuning_log() (OC_FW()->tuning_log())
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* plaits' shared random generator (pl_dsp.h pl_rng_state) is .data; a module has none: the word is the firmware's rng_cell (fw_api_t: the one generator
 * word it keeps for modules, SHAPES' pb_rng_state). MACRO2's drawing models render on CPU0 in voice order (eng_macro2.c mc2_draws), so sharing the
 * word with SHAPES (whose drawing shapes do the same) keeps every draw deterministic */
#define PL_RNG_EXTERN 1
#define pl_rng_state (*OC_FW()->rng_cell)
#include "audio_profile.h"
typedef void (*audio_batch_fn)(const void *data, int32_t *pcm);
#define AUDIO_BATCH_BYTES 168u
#define audio_batch_active (*OC_FW()->batch_active)
#define audio_batch_push(run, data, bytes, out, n, id) (OC_FW()->batch_push((run), (data), (bytes), (out), (n), (id)))
#include "eng_macro2.c"
static mc2_part_t *mc2_part(uint32_t part) { return resource_get(OC_FW()->res_engine0 + part % NPART, sizeof(mc2_part_t)); }

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO2, sizeof(mc2_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_MACRO2; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO2, sizeof(mc2_part_t), (uint32_t)(uintptr_t)&ENG_MACRO2, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
