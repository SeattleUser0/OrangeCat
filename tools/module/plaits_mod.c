/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS (candidate; docs/candidates/model_picker.md): the model picker's module, under the proposed number ENGI_MACRO2 (32, an OC_CANDIDATES
 * build's) for the study. This unit is the glue's (tools/module/plaits/eng_plaits.c: the engine); the models are units of their own
 * (tools/module/plaits/mdl_*.c, tbl_*.c), each a region of the image: tools/build.py MODULES "plaits" links them in a fixed order, the master
 * package carries all of them, and the page's packer keeps the picked ones (tools/pick_module.py, web/fm1pick.js).
 *
 * The header's state_size is 0 here: the build (and the packer) write the largest state the pick's models need (eng_plaits.c pl_state_bytes), and
 * pl_part reads it back from the header in flash, so a pick of small models takes a small arena block. The host unit (OC_MODULE_HOST,
 * tests/module_host_plaits_tu.c) has every model: PL_STATE_HOST.
 *
 * Revisions:
 *   macro2-candidate (model picker): first version.
 */
#include "pl_mod_common.h"
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }   /* (the host: oc_mod_memcpy) */
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }
#include "eng_plaits.c"

#ifdef OC_MODULE_HOST
#include "pl_mod_host_state.h"
static pl_part_t *pl_part(uint32_t part) { return resource_get(OC_FW()->res_engine0 + part % NPART, PL_STATE_HOST); }
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO2, PL_STATE_HOST, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_PLAITS; }
uint32_t oc_host_module_state(void) { return pl_state_bytes(); }   /* (what the device header gets for this set of units) */
#else
extern char _modstart[];
static pl_part_t *pl_part(uint32_t part)
{
    return resource_get(OC_FW()->res_engine0 + part % NPART, ((const fw_mod_hdr_t *)_modstart)->state_size);
}
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO2, 0, (uint32_t)(uintptr_t)&ENG_PLAITS, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
/* the part layout, for the build's manifest (tools/build.py reads the two words from the master image, section .plaits_meta, kept by the link
 * script: the packer sizes a pick's state from them) */
__attribute__((section(".plaits_keep"), used)) const uint32_t pl_mod_meta[2] = {PL_PART_HEAD, PL_POLY};
#endif
