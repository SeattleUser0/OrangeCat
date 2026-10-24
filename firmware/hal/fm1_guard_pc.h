/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Jeremiah Conner.
 * (Orange Cat 0.10.1) The one fact of the PC guard that is arithmetic, kept apart from the registers so a host test can run it.
 *
 * The chip has two PC limit register pairs (SDK check, CI run 40047398043: LIMIT0 and LIMIT1, no more). LIMIT1 is taken by the SRAM code window
 * (POOL code), so instruction fetch from flash has ONE window, LIMIT0 = [0x02000120, hi].
 *   hi = _etext                         when no module is registered
 *   hi = the last byte of the highest   when one is: the window then also holds the flash between _etext and the module (the pack, the library
 *        registered module                groups). That is the cost of loading a module: a wild jump into the pack or the library is no longer caught by
 *                                        the guard while a module is registered. Writes there are still caught (XIP is not writable).
 * It must come back to _etext when the last module goes (remove, ban), in a running session too: oc_mod_boot calls fm1_guard_pc_window() at its end.
 *
 * Revisions:
 *   0.10.1: first version (0.10.0 set the window only at boot, so an install or a remove left the old window until the next reset).
 */
#pragma once
#include <stdint.h>

static inline uint32_t fm1_guard_pc_hi(uint32_t code_end, uint32_t etext)
{
    return code_end > etext ? code_end : etext;
}
