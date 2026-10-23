/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.9.0, step 5b (throwaway experiment, branch exp/sonnet-module): SHAPES as a separate object.
 * This file is the module's translation unit: it includes the engine's own source (eng_shapes.c and, through it, braids/)
 * and replaces the firmware calls the audit found (docs/orangecat/ENGINE_LOADING_PLAN.md, fw_api.h) by the services table:
 *   trk, resource_get, tuning_log, the pack's ready flag, memcpy / memset.
 * clamp, voice_amp and amp_at are header-only (dsp.c) and are compiled into the module as they are.
 * Nothing here is linked into the firmware image. tools/module_exp/run.sh builds it for several position strategies.
 */
#include "build_options.h"
#include <stdint.h>
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "oc_pack.h"
#include "fw_api.h"

enum { RES_ENGINE0 = 0 };                                /* resources.c: the first of the arena ids (a constant the API must carry) */
static const fw_api_t *fw;                               /* given at alloc time (oc_module_entry) */

/* the services, by the table */
#undef OC_PACK_READY
#define OC_PACK_READY() (*fw->pack_ready)
#define trk (fw->trk)
#define tuning_log() (fw->tuning_log())
#define resource_get(id, n) (fw->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return fw->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return fw->mem_set(d, c, (uint32_t)n); }

#include "eng_shapes.c"
static sh_part_t *sh_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(sh_part_t)); }

/* the module's header: first in the image */
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION};

/* the entry: the firmware calls it once with its table and gets the engine */
__attribute__((section(".modentry"), used)) const engine_t *oc_module_entry(const fw_api_t *api)
{
    fw = api;
    return &ENG_SHAPES;
}
