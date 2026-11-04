/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MODELS (the model picker's module, Release 3): the module's translation unit, built for the host as its own object (tools/module/plaits_mod.c
 * with OC_MODULE_HOST: the table and, through tools/module/plaits/pl_mod_host_units.h, every model unit). Built again with -DPL_HOST_NO_<unit> for
 * the units a pick leaves out (tests/plaits_test.c's second build): those models read as not installed, which is what the device's packed module
 * looks like to the engine.
 */
#define OC_MODULE_HOST 1
#include "../tools/module/plaits_mod.c"
