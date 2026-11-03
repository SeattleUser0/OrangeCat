/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Release 3 step 6: the DUB TAPE module's translation unit, built for the host as its own object (tools/module/dub_mod.c with OC_MODULE_HOST), as tests/module_host_wheel_tu.c is for WHEEL:
 * the same kernel text and descriptor, reaching the firmware only through the services table (oc_host_fw, set by tests/fx_dub_module_test.c to the firmware's own FW_API). Its statics are its own.
 * The kernel's one external symbols (dubdelay.c's dk_*) are renamed: the firmware's translation unit has the resident copy of the same functions.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard      /* core.h defines this global (non-static); the firmware's is in the other unit */
#define dk_clear_block mdub_dk_clear_block
#define dk_reset mdub_dk_reset
#define dk_init mdub_dk_init
#define dk_max_seconds mdub_dk_max_seconds
#define dk_process mdub_dk_process
#define dk_spring_attach mdub_dk_spring_attach
#include "../tools/module/dub_mod.c"
