/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate: tuning_a4 for a module unit. An engine that reads the firmware's tuning_a4 (Hz, tuning.h: the CZ-1, SID)
 * cannot reach it from a module, and its own copy would be RAM; the services table gives tuning_log(), A4_LOG[tuning_a4 - A4_MIN], so the module
 * finds tuning_a4 as that value's index in A4_LOG (81 entries, one a Hz). Included after melodee_tables.h, core.h and the unit's OC_FW(); then
 * `#define tuning_a4 (oc_mod_a4())` before the engine's file.
 */
#pragma once
static int16_t oc_mod_a4(void)
{
    int32_t l = OC_FW()->tuning_log();
    uint32_t i;
    for (i = 0; i < NELEM(A4_LOG); i++)
        if (A4_LOG[i] == l)
            return (int16_t)(A4_MIN + i);
    return A4_DEFAULT;
}
