/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * WAVE, the measurement prototype's types (docs/candidates/wave.md). */
#pragma once
#include <stdint.h>
#define WV_LEN 256u        /* samples of a stored frame (the top level: 128 harmonics) */
#define WV_CTL 32u         /* the control block (CTL) */
#define WV_UNI 8u          /* unison voices an oscillator, at most */
#define WV_SLOTS 8u
#define WV_SRC 8u
enum { D_POS1, D_POS2, D_WARP1, D_WARP2, D_LVL1, D_LVL2, D_CUT, D_RES, D_AMP, D_PITCH, WV_DST };
enum { W_BEND, W_SYNC };
enum { F_LP, F_BP, F_HP, F_NOTCH, F_LADDER };
typedef struct { uint8_t src, dst; int16_t amt; } wv_slot_t;
typedef struct {
    uint8_t uni[2], warp[2], mode;
    int16_t sub, noise;
    int32_t base[WV_DST];
    int32_t macro[4];
    wv_slot_t slot[WV_SLOTS];
} wv_patch_t;
typedef struct { int32_t v, a, s; uint8_t stage, r; } wv_env_t;
typedef struct { uint32_t ph, inc; uint8_t shape; } wv_lfo_t;
typedef struct {
    uint32_t ph[WV_UNI], inc[WV_UNI];
    int32_t warp, amp;
    uint32_t len, shift, cached;
    int16_t frame[WV_LEN];
    int16_t tmp[WV_LEN / 2];   /* the halving's scratch (in the engine: the kernel's stack, not the state) */
} wv_osc_t;
typedef struct { float g, k, a1, ic1, ic2, l[4]; } wv_filt_t;
typedef struct {
    wv_osc_t osc[2];
    wv_env_t env[2];
    wv_lfo_t lfo[2];
    wv_filt_t f;
    int32_t amp0, amp1;
    uint32_t rng, gate, vel, note, level;
} wv_voice_t;
void wv_block(wv_voice_t *v, const wv_patch_t *p, const int16_t *const tables[2], const uint32_t nframes[2]);
void wv_kernel(wv_voice_t *v, const wv_patch_t *p, int32_t *out, uint32_t n);
