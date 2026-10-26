/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate: the JUNO module's unit (tools/module/juno_mod.c, Clipper's engine)
 * built for the host, as tests/module_host_sem_tu.c is for SEM.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/juno_mod.c"
