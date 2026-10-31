/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The mockups' scene interpreter: since the new interface 0.1 (branch ui-build, docs/orangecat/UI_BUILD.md 1b) the firmware's
 * own, firmware/src/oc_scene.h (the format) and oc_scene.c (the interpreter and its checker), so a mockup and the unit draw
 * with one interpreter, drawing only (OC_SCENE_DRAW_ONLY: no scene_of, no checker's lookup). Included after gfx.c.
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version (the format and the interpreter were here).
 *   (the new interface 0.1) moved into firmware/src; this file includes them.
 */
#pragma once
#define OC_SCENE_DRAW_ONLY 1
#include "oc_scene.h"
#include "oc_scene.c"
