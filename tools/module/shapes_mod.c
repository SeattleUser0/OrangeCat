/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.10.0 (step 1 of the module proof of concept): SHAPES as a loadable module. Moved here from the throwaway
 * experiment (branch exp/sonnet-module, tools/module_exp/shapes_mod.c, docs/orangecat/ENGINE_LOADING_PLAN.md "The module
 * experiment"), changed in one way: the module keeps no pointer to the firmware's table. It reads the table at the fixed address
 * FW_API_ADDR (fw_api.h), so it has no .data and no .bss (tools/module_check.sh, and tools/build.py --module, fail if it has one).
 *
 * This file is the module's translation unit. It includes the engine's own source (eng_shapes.c and, through it, braids/) and
 * replaces the five firmware services the audit found (fw_api.h) by the table's members: trk, resource_get, tuning_log, the
 * asset pack's ready flag (oc_pack.h, with OC_MODULE defined), memcpy and memset. clamp, voice_amp and amp_at are header-only
 * (dsp.c) and are compiled into the module as they are. The first arena id (RES_ENGINE0) comes from the table too.
 *
 * Revisions:
 *   0.10.0: moved from the experiment; the table by its fixed address, no .data / .bss. Step 2: the header carries the engine
 *          number, the state size, the engine_t's address, the address it is linked for and the pack's hash (fw_mod_hdr_t); there
 *          is no entry function: the header says where the engine_t is.
 */
#define OC_MODULE 1
#include "build_options.h"
#include <stdint.h>
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)          /* the firmware's services: at a fixed address, so no pointer of our own */
#include "oc_pack.h"

/* the services, by the table */
#define trk (OC_FW()->trk)
#define tuning_log() (OC_FW()->tuning_log())
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

#include "eng_shapes.c"
static sh_part_t *sh_part(uint32_t part) { return resource_get(OC_FW()->res_engine0 + part % NPART, sizeof(sh_part_t)); }

/* the module's header: first in the image (fw_api.h). The linker script sets _modstart to the address the variant is linked for;
 * tools/build.py --module fills length, crc and hcrc after the link. */
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_SHAPES, sizeof(sh_part_t), (uint32_t)(uintptr_t)&ENG_SHAPES, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, 0, 0,
};
