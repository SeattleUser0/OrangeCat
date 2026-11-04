/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 20) DUB TAPE's kernel is a MODULE only: the image has no copy (FX_MODULES_PLAN.md 15.9). A host test that needs the echo to sound registers the module's host build with the insert host,
 * the way the unit's loader does for the installed .ocm. Include after the firmware (the unity build) and link tests/module_host_dub_tu.c's object (tools/module/dub_mod.c with OC_MODULE_HOST:
 * tests/run_tests.sh dub_t does both). dub_host_module_set() registers it for every track; dub_host_module_clear() is the module going away (the tracks play dry). */
#ifndef DUB_MODULE_HOST_H
#define DUB_MODULE_HOST_H
#include "fw_fx.h"
extern const fw_api_t *oc_host_fw;
extern const fx_caps_t *oc_host_fx_caps(void);
extern const fx_kcalls_t *oc_host_fx_calls(void);
extern const fx_prow_t *oc_host_fx_rows(void);
extern const fx_desc_wire_t *oc_host_fx_desc(void);
extern const fx_entry_wire_t *oc_host_fx_entry(void);
#define DUB_STATE_BYTES ((uint32_t)((sizeof(dd_t) + 3u) & ~3u))   /* the state a kernel instance holds on this build (the module's caps word ram_state is the same number) */
static int dub_host_module_set(void)
{
    oc_host_fw = &FW_API;
    return fx_modk_set(oc_host_fx_desc()->module_id, oc_host_fx_desc()->version, oc_host_fx_entry()->effect_id, oc_host_fx_caps(), oc_host_fx_calls(), oc_host_fx_rows());
}
static void dub_host_module_clear(void) { fx_modk_clear(); }
#endif
