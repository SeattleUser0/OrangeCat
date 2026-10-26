/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate: the CZ-1 module's unit (tools/module/cz_mod.c) built for the host, as tests/module_host_wheel_tu.c is for WHEEL.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/cz_mod.c"
