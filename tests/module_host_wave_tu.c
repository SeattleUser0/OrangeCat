/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat (wave-engine): the WAVE module's unit (tools/module/wave_mod.c) built for the host, as tests/module_host_sem_tu.c is for SEM.
 * (wave-bank) The host has no XIP window: the module reads the sample slot's room from wvb_host_flash (the flash's first 0xC2000 bytes, zero = no bank),
 * which tests/wave_bank_test.c fills with a bank.
 */
#include <stdint.h>
uint8_t wvb_host_flash[0xC2000u];
#define WVB_PTR(off) (wvb_host_flash + (off))
#define OC_MODULE_HOST 1
#define bootguard module_unit_bootguard
#include "../tools/module/wave_mod.c"
