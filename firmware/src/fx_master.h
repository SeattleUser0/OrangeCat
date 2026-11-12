/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Release 3 step 8b (FX_MODULES_PLAN.md 17): what a MASTER kernel (kind 3: a buffer effect of the FX layer, on the master after its level) is to its host, in one header the host
 * (jfx.c / jfx_perform.c) and an effect module (tools/module/fxlayer_mod.c) both include. fx_kernel.h is the same for kind 1 (an insert), fx_send.h for kind 2 (a mode of the reverb bus); the
 * three share the caps block (fx_caps_t) and nothing else.
 *
 * The host is the FX layer (jfx_perform.c). It keeps everything that is not a ring effect's arithmetic: the keys (perf_press, read by the key scan), what is held, latched and running, the
 * press order and which effect plays (perf_pick, perf_switch), the 1/16 the effects start on (perf_begin), the 32 KB (taken and released by jfx_sl_mem, lent from the SLICER, lost: perf_buf_lost),
 * REPEAT 1/8 1/16 1/32 (the plain loop), the sweeps CRUSH and THROW, the mutes, the FILTER macro, and the effects' STATE (fx3_st_t: host RAM, cleared by the host where it always was). A kernel
 * is two functions an effect: `start` (the effect begins on a cleared state) and `run` (a run of samples of the block, in place, with the fade of the buffer's share), called from perf_block
 * in the audio interrupt on CPU0 where the resident functions are called when no module is registered. Everything it needs comes in the call (fx3_ctx_t): the state, the 32 KB, the card's four
 * knobs (perf_gp's row, RAM, never stored), the tempo, SCRATCH's count, and one host service (semi_q16). A module has no RAM of its own and imports nothing beyond fw_api_t as it is: fw_api_t
 * does not grow and FW_API_ID does not move.
 *
 * The identifier. These structures are NOT in FW_API_ID (what ANY module may call and read), NOT in FX_ABI_ID (the insert host's convention: DUB TAPE's installed modules) and NOT in FX2_ABI_ID
 * (the send host's: the reverb module). They have their own, FX3_ABI_ID (tools/gen_fw_api_id.py FX3_SOURCES: fx3_buf_t, fx3_shr_t, fx3_grain_t, fx3_st_t, fx3_ctx_t, fx3_kcalls_t here, and
 * fx_caps_t, whose layout a kind 3 entry's caps block also has), written to build/gen/fw_api_id.h. A kind 3 module carries it in its caps block's `abi` word; the loader (fw_fx.h fx_caps_check)
 * and the host (fx3_modk_set) refuse a module built with another.
 *
 * What the hash cannot see, and is part of the convention all the same: the meaning of the four knob bytes of each effect (ui_layer.c FXK_MIN / FXK_MAX: the module's own pages carry the same
 * ranges, held equal by tests/fx_layer_module_test.c), the fade's length (perf_kern.h PFK_RAMP: the kernel's own, the host asserts it is the SLICER's), and that `start` is called on a state
 * the host has cleared. */
#pragma once
#include <stdint.h>
#include "fw_api_id.h"                                  /* generated: FX3_ABI_ID */
#define FX3_FRAMES 8192u                                /* the 32 KB: stereo frames (jfx.c SL_FR: the SLICER's recording, which the layer borrows; jfx_perform.c asserts they are one number) */
#define FX3_FX 4u                                       /* the layer's ring effects. A master kernel's effect id is 1 + its place: 1 GLITCH, 2 TAPE STOP, 3 SHUFFLE, 4 FREEZE (jfx_perform.c GX_*) */
/* the 32 KB (one arena block, JB_SL): GLITCH, TAPE STOP and FREEZE use it as a ring of 8192 stereo frames at 22.05 kHz; SHUFFLE as 32,512 bytes of ADPCM and, in its last 64 frames, the
 * coder's state at each 1/16. The number is written out (it is in the identifier) */
typedef struct fx3_buf {
    int16_t f[8192][2];
} fx3_buf_t;
typedef struct fx3_shr {                                /* SHUFFLE: a reader of the ADPCM ring, or live */
    uint32_t fr, rate;
    uint16_t pos;
    int16_t a, b;
    uint8_t idx, live;
} fx3_shr_t;
typedef struct fx3_grain {                              /* FREEZE: read place (Q16), rate, window 0..65536 */
    uint32_t pos, rate, x, dx;
} fx3_grain_t;
/* the buffer effect's state between blocks: the host's RAM (jfx_perform.c `pf.k`; it was pf.P .. pf.u there). The host writes P (the 1/16 in samples, each block), gs (the transport's 1/16
 * when GLITCH or SHUFFLE starts) and clears state, odd, wr and u before `start`; REPEAT (resident) uses wr, odd, hl, hr, state and w too (one buffer effect plays at a time). (The share's
 * target, pf.tw of the old text, was written and read within one sample: it is a local of `run` now and the state is the size it was) */
typedef struct fx3_st {
    uint32_t P;                                         /* the 1/16, samples (not 0) */
    uint32_t wr;                                        /* frames written */
    int32_t w, hl, hr;                                  /* the buffer's share, Q15; the first of a pair */
    int32_t yl, yr;                                     /* what the effect plays this sample */
    uint32_t gs, gph;                                   /* GLITCH, SHUFFLE: the transport's 1/16 (a count), samples into it */
    uint8_t state, odd;                                 /* TAPE: 1 stopped; FREEZE: 1 frozen (REPEAT: 1 playing); a pair's half */
    union {
        struct { uint8_t a, n, h, sh; uint32_t l, u, k, g0, sp, src; int32_t rate, cl, cr; } g;   /* GLITCH */
        struct { uint32_t x, dx, p, p1, q, sc; int32_t c, gn; } t;                                 /* TAPE */
        struct { uint32_t c, fs, iv, scan, rnd, ph; int32_t sd, gain, nl, nr; fx3_grain_t v[4]; } f;   /* FREEZE */
        struct { uint32_t ew, nrec, cu, mid; int32_t ep; uint8_t ei, kind, j0, xn; fx3_shr_t r[2]; } s;   /* SHUFFLE */
    } u;
} fx3_st_t;
/* what the host hands a master kernel with every call. Nothing here is the kernel's to keep: the pointers are valid for the call */
typedef struct fx3_ctx {
    fx3_st_t *st;                                       /* the effect's state */
    fx3_buf_t *buf;                                     /* the 32 KB (held: the host does not call a kernel without it) */
    const volatile int8_t *gp;                          /* the effect's four knobs (perf_gp's row: written by the main loop, read where the resident text read it) */
    uint32_t beat;                                      /* beat_samples() as the audio interrupt reads it at the call */
    uint32_t scr;                                       /* SCRATCH: the knob's detents, a running count (TAPE STOP) */
    uint32_t go;                                        /* 1: the effect is the one that plays (its share rises); 0: it is fading out (run returns when its share is 0) */
    uint32_t head;                                      /* 1: this run begins the block (TAPE STOP, stopped, takes SCRATCH's detents once a block there) */
    uint32_t (*semi)(int32_t st);                       /* host service: a semitone count as a speed, Q16 (rev_kern.h semi_q16: GLITCH's and FREEZE's PITCH) */
} fx3_ctx_t;
/* the calls: in a module, FX3_KCALLS_N words in its flash that the caps block's `kernel` word points at */
typedef struct fx3_kcalls {
    void (*start)(const fx3_ctx_t *c);                  /* the effect begins: st->state, odd, wr and u are 0, P and gs are set */
    uint32_t (*run)(const fx3_ctx_t *c, int32_t *l, int32_t *r, uint32_t n);   /* n samples of the master (Q15, stereo) in place: the effect's share of what it plays, the rest live. Returns
                                                         * how many it did: n, or fewer when, fading out (go 0), its share reached 0 at the last of them (the host then lets the buffer go) */
} fx3_kcalls_t;
#define FX3_KCALLS_N 2u
#define FX3_KCALLS_WIRE 8u                              /* sizeof(fx3_kcalls_t) on the device */
_Static_assert(sizeof(void *) != 4 || sizeof(fx3_kcalls_t) == FX3_KCALLS_WIRE, "the master call table's device layout");
#define FX3_MODULE_ID 3u                                /* the module id the layer takes GLITCH, TAPE STOP, SHUFFLE and FREEZE from (docs/orangecat/MODULE_IDS.md): the bank of step 8b */
enum { FX3M_OK = 0, FX3M_VER, FX3M_RANGE };             /* fx3_modk_set's answer (the Effects row's `why` is 0x40 + the code, as the other hosts') */
