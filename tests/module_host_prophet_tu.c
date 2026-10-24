/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.1 (Part 1b): the Prophet module's translation unit, built for the host as its own object. It is the device module's source
 * (tools/module/prophet_mod.c) with OC_MODULE_HOST, as tests/module_host_tu.c is for SHAPES: the same engine code, reaching the firmware only through the
 * services table (oc_host_fw, set by tests/module_host_prophet_test.c to the firmware's own FW_API). Its statics are its own, as a module's are.
 *
 * Revisions:
 *   0.12.1: first version.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard      /* core.h defines this global (non-static); the firmware's is in the other unit. On the device the module's copy is not linked at all. */
#include "../tools/module/prophet_mod.c"
