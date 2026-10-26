/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate: the dddd module's unit (tools/module/dddd_mod.c, op1r's plugin through tools/module/op1r/op1_glue.h)
 * built for the host, as tests/module_host_sem_tu.c is for SEM. Compiled with -Itools/module/op1r (the plugin's "op1_plugin.h").
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/dddd_mod.c"
