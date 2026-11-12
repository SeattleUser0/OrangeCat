/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 3 step 8a) the reverb module's host build (tests/module_host_reverb_tu.c over tools/module/reverb_mod.c) for a host test: its externs, and rev_host_module_set(), which registers its
 * bank with the send host (jfx.c fx2_modk_set) the way the unit's loader does for the installed .ocm. Include after the firmware (the unity build) and link the unit's object. */
#ifndef REV_MODULE_HOST_H
#define REV_MODULE_HOST_H
#include "fw_fx.h"
extern const fw_mod_hdr_t OC_REVERB_MODULE_HDR;
extern const fw_api_t *oc_host_rev_fw;
extern const fx_caps_t *oc_host_rev_caps(uint32_t k);        /* k: 0 SPRING, 1 SHIMMER, 2 RESO */
extern const fx2_kcalls_t *oc_host_rev_calls(uint32_t k);
extern const fw_pages_t *oc_host_rev_pages(uint32_t k);
extern const fx_desc_wire_t *oc_host_rev_desc(void);
extern const fx_entry_wire_t *oc_host_rev_entry(uint32_t k);
static int rev_host_module_set(void)
{
    fx2_reg_t r[3];
    oc_host_rev_fw = &FW_API;
    for (uint32_t k = 0; k < 3u; k++) { r[k].effect_id = oc_host_rev_entry(k)->effect_id; r[k].caps = oc_host_rev_caps(k); r[k].calls = oc_host_rev_calls(k); }
    return fx2_modk_set(oc_host_rev_desc()->module_id, oc_host_rev_desc()->version, 3, r);
}
static void rev_host_module_clear(void) { fx2_modk_clear(); }
#endif
