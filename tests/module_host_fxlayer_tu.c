/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Release 3 step 8b: the FX layer module's translation unit, built for the host as its own object (tools/module/fxlayer_mod.c with OC_MODULE_HOST), as tests/module_host_reverb_tu.c is for the
 * reverb module: the same kernel text (firmware/src/perf_kern.h) and descriptor, reaching the firmware only through the context of each call (and the services table, oc_host_fxl_fw, for the
 * compiler's memcpy / memset). Its statics are its own; its externals have names of their own, so a test may link the other modules' units beside it.
 */
#define OC_MODULE_HOST 1
#define bootguard module_unit_fxl_bootguard  /* core.h defines this global (non-static); the firmware's is in the other unit */
#include "../tools/module/fxlayer_mod.c"
