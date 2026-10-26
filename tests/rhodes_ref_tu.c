/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (Frodhes): Frodhes' own engine, firmware/src/rhodes.c of https://github.com/leonardoMirabal/frodhes at commit
 * a8320e860c2307a6faa7266b9f59763dc3acb786 (tools/module/frodhes/rhodes.c, an unchanged copy, GPL-3.0-only), built as Frodhes' host build builds it
 * (DINGHY_HOST: one thread, no barrier), in a unit of its own. tests/rhodes_parity_test.c drives it beside the port (tools/module/rhodes_mod.c).
 */
#define DINGHY_HOST 1
#include <stdint.h>
#include <string.h>
#include "../tools/module/frodhes/rhodes.c"
#include "../tools/module/frodhes/rhodes_knobs.h"

/* Frodhes params.c param_apply for the voicing knobs (TYPE, TONE, BARK, BELL, HAMMER, DECAY, RELEASE, NOISE; TUNE stays 0) */
void ref_voicing(const int *k)
{
    rh_type = (uint8_t)k[0];
    rh_tone_off = (k[1] - 64) * (1 << 18);
    rh_bark_s = (int32_t)KN_P64[k[2]]; rh_bark_g = (int32_t)KN_P64I[k[2]];
    rh_bell = (int32_t)KN_P32[k[3]];
    rh_ham = (int32_t)KN_HAM[k[4]];
    rh_k_decay = (int32_t)DECAY_K[k[5]];
    rh_k_rel = (int32_t)DECAY_K[k[6]];
    rh_thump = KN_THUMP[k[7]]; rh_felt = KN_FELT[k[7]];
}
void ref_note_on(uint32_t note, uint32_t vel) { rh_note_on(note, vel); }
void ref_note_off(uint32_t note) { rh_note_off(note); }
void ref_render(int32_t *mono, uint32_t n) { rh_render(mono, n); }
uint32_t ref_active(void) { return rh_active_bits; }
void ref_reset(void)                       /* every voice, the filters, the counters, the ring: as at power-on */
{
    memset(rv, 0, sizeof rv);
    rlc_m1 = rlc_m2 = rlc_o1 = rlc_o2 = rlc_e = 0;
    memset(bd_m, 0, sizeof bd_m); memset(bd_o, 0, sizeof bd_o); memset(bd_e, 0, sizeof bd_e);
    rh_ages = 0; rh_lfsr = 0xACE1u; rh_pedal_w = 32768; ev_r = ev_w; rh_shed_req = 0;
    rh_active_bits = rh_held_bits = 0; rh_quiet_blocks = 0;
}
