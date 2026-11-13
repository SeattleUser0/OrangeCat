/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 3 step 8b) the FX layer module's host build (tests/module_host_fxlayer_tu.c over tools/module/fxlayer_mod.c) for a host test: its externs, and fxl_host_module_set(), which registers
 * its bank with the master host (jfx_perform.c fx3_modk_set) the way the unit's loader does for the installed .ocm. Include after the firmware (the unity build) and link the unit's object. */
#ifndef FXL_MODULE_HOST_H
#define FXL_MODULE_HOST_H
#include "fw_fx.h"
extern const fw_mod_hdr_t OC_FXLAYER_MODULE_HDR;
extern const fw_api_t *oc_host_fxl_fw;
extern const fx_caps_t *oc_host_fxl_caps(uint32_t k);        /* k: 0 GLITCH, 1 TAPE STOP, 2 SHUFFLE, 3 FREEZE */
extern const fx3_kcalls_t *oc_host_fxl_calls(uint32_t k);
extern const fw_pages_t *oc_host_fxl_pages(uint32_t k);
extern const fx_desc_wire_t *oc_host_fxl_desc(void);
extern const fx_entry_wire_t *oc_host_fxl_entry(uint32_t k);
static int fxl_host_module_set(void)
{
    fx3_reg_t r[4];
    oc_host_fxl_fw = &FW_API;
    for (uint32_t k = 0; k < 4u; k++) { r[k].effect_id = oc_host_fxl_entry(k)->effect_id; r[k].caps = oc_host_fxl_caps(k); r[k].calls = oc_host_fxl_calls(k); }
    return fx3_modk_set(oc_host_fxl_desc()->module_id, oc_host_fxl_desc()->version, 4, r);
}
static void fxl_host_module_clear(void) { fx3_modk_clear(); }
#endif
