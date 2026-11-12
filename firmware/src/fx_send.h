/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Release 3 step 8a (FX_MODULES_PLAN.md 16): what a SEND kernel (kind 2: a mode of the shared reverb bus) is to its host, in one header the host (jfx.c) and an effect module
 * (tools/module/reverb_mod.c) both include. fx_kernel.h is the same for kind 1 (an insert); the two share the caps block (fx_caps_t) and nothing else.
 *
 * The host is jfx.c's reverb bus. It keeps everything that is not a mode's arithmetic: the TYPE chooser and ROOM (resident for good), the send's tone (FILT) and pre-delay, the tank and the model
 * line (taken and released by jfx_mem as before), the fade and the clearing at a mode change, the gate and the rest, and the modes' STATE (rev_kst_t and the feedback block: host RAM, cleared by
 * the host exactly where it always was). A kernel is one function a mode: `process`, called from rev_run (the bus job: CPU1 when it is there, CPU0 otherwise) where the resident function of the
 * mode is called when no module is registered. Everything it needs comes in the call (fx2_ctx_t): the parameter snapshot, the state, the tank, the model line's blocks, the side outputs, RESO's
 * inputs, and the host's services (the resident ROOM network, which SHIMMER runs inside itself; the LFO rate table). A module has no RAM of its own and imports nothing beyond fw_api_t as it is
 * (its sine and tanh tables through base_hot, PITCH_OCT through pitch_oct): fw_api_t does not grow and FW_API_ID does not move.
 *
 * The identifier. These structures are NOT in FW_API_ID (what ANY module may call and read) and NOT in FX_ABI_ID (the insert host's calling convention: moving it would refuse every installed
 * DUB TAPE module for a change that is not DUB's). They have their own, FX2_ABI_ID (tools/gen_fw_api_id.py FX2_SOURCES: rev_tank_t, rev_kst_t, fx2_ctx_t, fx2_kcalls_t here, and fx_caps_t, whose
 * layout a kind 2 entry's caps block also has), written to build/gen/fw_api_id.h. A kind 2 module carries it in its caps block's `abi` word; the loader (fw_fx.h fx_caps_check) and the host
 * (fx2_modk_set) refuse a module built with another.
 *
 * What the hash cannot see, and is part of the convention all the same: the indices of the reverb's values in the snapshot (core.h G_RTYPE .. G_RWIDE, G_COUNT words). They are the project
 * format's (song.g is stored by index), so they do not move; the module's caps word param_bytes is 2 * G_COUNT of its build and the host refuses another size. */
#pragma once
#include <stdint.h>
#include "fw_api_id.h"                                  /* generated: FX2_ABI_ID */
#define SH_LEN 2048u                                    /* SHIMMER's line: the model line's first 4 KB block (it was jfx.c's define; jfx.c asserts it is one arena block, JFX_SEG) */
#define FX2_MODES 5u                                    /* the reverb's modes (G_RTYPE): 0 ROOM (resident for good), 1 SPRING, 2 SHIMMER, 3 RESO, 4 CLOUD. A send kernel's effect id IS its mode */
/* the tank (the reverb bus's 11.9 KB, one arena block): ROOM's four combs and two allpasses; SPRING's loop and its allpass chain (int32); RESO's four strings. The numbers are written out
 * (they are in the identifier): 1116 + 1188 + 1277 + 1356 + 1 and 556 + 441 + 1 */
typedef struct rev_tank {
    int16_t comb[4938];
    union { int16_t ap[998]; int32_t sp[499]; } u;
} rev_tank_t;
/* the modes' state between blocks: the host's RAM (jfx.c `fx.k`), which the host clears (rev_state_clear: a mode change, the rest, a project load) and never reads */
typedef struct rev_kst {
    uint32_t sh_w, sh_ph;                               /* SHIMMER: its line's write count, the grains' phase (Q16) */
    int32_t sh_tlp;                                     /* .. AIR's low-pass / its shelf */
    int32_t rs_lp[4];                                   /* RESO: the combs' loop low-passes */
    uint32_t rs_per[4];                                 /* .. their periods now (Q8), gliding to the chord's */
    uint8_t rs_root;                                    /* .. the note FOLW last heard + 1 (0: none yet) */
    uint16_t rs_w;                                      /* .. their write index (each its own line, 1024) */
    uint16_t sp_w;                                      /* SPRING: the loop's write index */
    uint8_t sp_ns;                                      /* .. its allpass stages last block */
    int32_t sp_lp, sp_hp, sp_he, sp_size;               /* .. its loop low-pass, low cut (and its remainder), the loop length (Q8) */
    uint32_t sp_ph;                                     /* .. the output tap's wobble */
} rev_kst_t;
/* what the host hands a send kernel with every block. Nothing here is the kernel's to keep: the pointers are valid for the call */
typedef struct fx2_ctx {
    const int16_t *g;                                   /* the parameter snapshot: song.g as the bus job reads it (fxp.g, copied on CPU0 at bus start) */
    rev_kst_t *st;                                      /* the modes' state */
    int32_t *fb;                                        /* SHIMMER's (and CLOUD's) feedback block: CTL words, the host's, cleared with the state */
    rev_tank_t *tank;                                   /* the tank (held: the host does not call a kernel without it) */
    int16_t *const *line;                               /* the model line's 4 KB blocks (2048 int16 each) */
    uint32_t nline;                                     /* .. and how many are held from the first (SHIMMER: 1; the host does not call a mode whose ram_work_min is not held) */
    int32_t *sl, *sr;                                   /* the side out (left +, right -): n words each, added to */
    uint32_t reso_mask, reso_lo;                        /* RESO: the scale (12 bits) and the followed track's lowest held note (0xFF: none), read on CPU0 */
    int32_t reso_root;
    void (*room)(const int32_t *in, int32_t *out, uint32_t n);                   /* the host's ROOM network, plain: n samples of `in` through it, added to `out` (SHIMMER) */
    void (*room_mod)(const int32_t *in, int32_t *out, uint32_t n, int32_t wd);   /* .. and modulated / wide (its side into sl / sr): for CLOUD, not called by the bank of step 8a */
    const uint32_t *lfo_inc;                            /* melodee_tables.h LFO_INC[128]: the rate table (512 B the module does not carry) */
} fx2_ctx_t;
/* the calls: in a module, FX2_KCALLS_N words in its flash that the caps block's `kernel` word points at */
typedef struct fx2_kcalls {
    void (*process)(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n);   /* the bus job: the mode's block, n samples of the send `in` added into the mono return `out` */
} fx2_kcalls_t;
#define FX2_KCALLS_N 1u
#define FX2_KCALLS_WIRE 4u                              /* sizeof(fx2_kcalls_t) on the device */
_Static_assert(sizeof(void *) != 4 || sizeof(fx2_kcalls_t) == FX2_KCALLS_WIRE, "the send call table's device layout");
#define FX2_MODULE_ID 2u                                /* the module id the reverb host takes SPRING, SHIMMER and RESO from (docs/orangecat/MODULE_IDS.md): the bank of step 8a */
enum { FX2M_OK = 0, FX2M_VER, FX2M_RANGE };             /* fx2_modk_set's answer (the Effects row's `why` is 0x40 + the code, as the insert host's) */
