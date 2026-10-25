/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.14.0: the WHEEL module's translation unit, built for the host as its own object (tools/module/wheel_mod.c with OC_MODULE_HOST), as
 * tests/module_host_tu.c is for SHAPES: the same engine code, reaching the firmware only through the services table (oc_host_fw, set by
 * tests/module_host_wheel_test.c to the firmware's own FW_API). It is built with the default engine set (WHEEL is not in it), as the device's module is.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard      /* core.h defines this global (non-static); the firmware's is in the other unit */
#include "../tools/module/wheel_mod.c"
