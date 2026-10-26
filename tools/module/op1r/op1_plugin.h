/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat: the name op1r's plugins include ("op1_plugin.h", op1r's synth_sdk/op1_plugin.h). In an Orange Cat module it is this file, and the
 * API it stands for is op1_glue.h, which the module unit has included already (tools/module/<name>_mod.c): op1r's header is never read.
 */
#ifndef OP1_GLUE_H
#error "an op1r plugin is built through its module unit (tools/module/<name>_mod.c), which includes op1_glue.h first"
#endif
