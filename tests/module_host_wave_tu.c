/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat (wave-engine): the WAVE module's unit (tools/module/wave_mod.c) built for the host, as tests/module_host_sem_tu.c is for SEM.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/wave_mod.c"
