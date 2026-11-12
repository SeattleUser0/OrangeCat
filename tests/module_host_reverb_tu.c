/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Release 3 step 8a: the reverb module's translation unit, built for the host as its own object (tools/module/reverb_mod.c with OC_MODULE_HOST), as tests/module_host_dub_tu.c is for DUB TAPE:
 * the same kernel text (firmware/src/rev_kern.h) and descriptor, reaching the firmware only through the services table (oc_host_rev_fw, set by the test to the firmware's own FW_API) and the
 * context of each call. Its statics are its own; its externals have names of their own, so a test may link DUB TAPE's unit beside it.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_rev_bootguard  /* core.h defines this global (non-static); the firmware's is in the other unit */
#include "../tools/module/reverb_mod.c"
