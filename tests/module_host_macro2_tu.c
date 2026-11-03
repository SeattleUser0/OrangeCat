/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO2 (candidate): the module's translation unit, built for the host as its own object (tools/module/macro2_mod.c with OC_MODULE_HOST), as
 * tests/module_host_wheel_tu.c is for WHEEL: the same engine code, reaching the firmware only through the services table.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/macro2_mod.c"
