/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS (the model picker's module, candidate): the module's translation unit, built for the host as its own object (tools/module/plaits_mod.c
 * with OC_MODULE_HOST: the glue and, through tools/module/plaits/pl_mod_host_units.h, every model unit), as tests/module_host_macro2_tu.c is for
 * MACRO2. Built again with -DPL_HOST_NO_<unit> for the units a pick leaves out (tests/plaits_test.c's second build): those models read as not
 * installed, which is what the device's packed module looks like to the glue.
 */
#define OC_MODULE_HOST 1
#include "../tools/module/plaits_mod.c"
