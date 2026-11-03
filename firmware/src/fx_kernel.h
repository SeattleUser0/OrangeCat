/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Release 3 step 6 (FX_MODULES_PLAN.md 15): what a kernel is to the insert host, in one header the host (fx_insert.h) and an effect module (tools/module/dub_mod.c) both include.
 *
 * fx_kernel_t is the host's view of a kernel: the capability words (the plan's section 4 column) and the calls. The resident kernel (jfx_dub.c) is a const object of it; a module's kernel is
 * built in RAM from the module's checked descriptor (fx_caps_t, below: the words) and its call table (fx_kcalls_t: the pointers into the module's own code) by fx_modk_set (fx_insert.c).
 * Everything a kernel needs from the host that is not in the track it reads comes in the call (fx_ictx_t: the tempo and the performance triggers), so a module imports NOTHING from the
 * firmware beyond fw_api_t as it is (memcpy / memset): fw_api_t does not grow and the interface identifier does not move.
 *
 * Not in FW_API_ID's hash (tools/gen_fw_api_id.py SOURCES): that identifier says what ANY module image may call (fw_api_t) and read (track_t); this is what the HOST calls in an effect module.
 * They have their own, FX_ABI_ID (gen_fw_api_id.py FX_SOURCES: fx_ictx_t, fx_kcalls_t, fx_caps_t, written to build/gen/fw_api_id.h): the module's fx_caps_t carries it in `abi` and the loader refuses a
 * module built with another (MODULE_FORMAT_R3.md 6, Decision). FX_CAPS_VERSION is for what a hash cannot see: a changed meaning of a word in the same layout. The host refuses a caps block of a
 * version it does not know (the module is registered, its Effects row says why, the resident kernel plays). */
#pragma once
#include <stdint.h>
#include "fw_api_id.h"                               /* generated: FX_ABI_ID, the identifier of this file's host-to-module structures (tools/gen_fw_api_id.py FX_SOURCES) */
#define FX_INS_N 4u                                     /* instances: one a track (DUB_N) */
#define FX_INS_NSEG 24u                                 /* the pool: 96 KB of 4 KB blocks, shared (four lines of 24K, or one of 64K and two of 16K) */
#define FX_NTRIG 6u                                     /* the performance triggers a track has (fx_ictx_t.trig): THRW CUT KICK HOLD DIVE RISE */
#define FX_INS_MAXSEG 16u                               /* one line: 64 KB (a kernel's ram_work_max may not exceed it) */
/* a stored parameter of a kernel: fw_fx.h's parameter row without the label. `legacy` is the Release 2 track slot (P_* id) the value is stored in (0xFFFF none) */
typedef struct { uint16_t id, legacy; int16_t min, max, def; } fx_prow_t;
/* what the host tells a kernel when it asks for a size, a tick, a parameter signature or a snapshot. kw0: the instance's kernel word 0 (kw[0]); k: the instance (= the track); R: the line's
 * rate divisor; nblk: line blocks held; ramp: ramping out (the cut); beat: samples a beat at the tempo in use (beat_samples()); trig: the track's performance triggers (MIDI CC 16 to 18: THRW,
 * CUT, KICK, then (dub-perf) HOLD, DIVE, RISE: MIDI CC 16 to 21, 0 .. 127, RAM only, the host's: fx_trig). kw2: the instance's kernel word 2 (kw[2]), the ONLY word a kernel changes
 * in the context: `snap` may write it and the host keeps it with the instance (DUB: the wet of the last ECHO above 0, as float bits: the module has no RAM of its own). A field a call does not
 * document is not set for it (size_sig and size_need: k and beat; tick: k, beat and trig) */
typedef struct { uint32_t kw0; uint32_t beat; uint32_t kw2; uint8_t k, R, nblk, ramp; uint8_t trig[6]; uint8_t pad[2]; } fx_ictx_t;
_Static_assert(sizeof(((fx_ictx_t *)0)->trig) == FX_NTRIG, "FX_NTRIG is fx_ictx_t's trig[6]: the literal is what FX_ABI_ID hashes");
enum { FXK_SEED = 1, FXK_US = 2, FXK_TAIL_FAR = 4, FXK_TAIL_SILENT = 8 };   /* fx_kernel_t.flags: MODULE_FORMAT_R3.md 6's bits (uses the seed; cost in microseconds; tail adds the farthest tap; tail until silent) */
/* the calls (CPU0 unless said): in a module, twelve words in its flash that the descriptor's caps block points at (FX_KCALLS_WIRE bytes on the device) */
typedef struct fx_kcalls {
    uint32_t (*on)(const track_t *t);                   /* the track wants the effect (DUB: ECHO != 0) */
    uint32_t (*size_sig)(const track_t *t, const fx_ictx_t *c);   /* a hash of what the sizing is made from (the times, the mode, the speed, the tempo) */
    float (*size_need)(const track_t *t, const fx_ictx_t *c);     /* the farthest tap, in half-rate loop samples */
    uint32_t (*blocks_at)(float need, uint32_t R);      /* the line blocks the need takes at the rate divisor R (2 or 4) */
    uint32_t (*hold_len)(float need, uint32_t blocks, uint32_t R);   /* blocks the instance runs past the last audible input or wet */
    void (*tick)(uint32_t *kw, const fx_ictx_t *c);     /* once a block for every instance, running or not (DUB: the KICK edge) */
    uint32_t (*param_sig)(const track_t *t, const fx_ictx_t *c);   /* a hash of what the snapshot is made from, host facts included */
    void (*snap)(void *params, const track_t *t, fx_ictx_t *c);   /* the parameter snapshot (FX_PARAM_BYTES), from the track's stored ids through `rows`; may set c->kw2 */
    uint32_t (*cost_extra)(const track_t *t, uint32_t extra);   /* units added to the batch cost (DUB: the spring attached, WOW) */
    void (*start)(void *state, uint32_t R, int16_t *const *blk, uint32_t nblk, int16_t *extra);   /* (re)start from silence on the blocks given; extra: the option's memory or 0 */
    void (*process)(void *state, const void *params, const float *in, float *outL, float *outR);   /* CPU1: one block */
    void (*reset)(void);                                /* a new project, power-on (nothing of the kernel's own survives in RAM: the host's inputs are the host's to clear) */
} fx_kcalls_t;
#define FX_KCALLS_N 12u
#define FX_KCALLS_WIRE 48u                              /* sizeof(fx_kcalls_t) on the device: twelve 4 byte code addresses */
_Static_assert(sizeof(void *) != 4 || sizeof(fx_kcalls_t) == FX_KCALLS_WIRE, "the call table's device layout");
typedef struct fx_kernel {
    /* identity (the descriptor's fx_desc_t / fx_entry_t words) */
    uint32_t module_id, version, legacy_module;         /* 1 for DUB TAPE; 0x00MMmmpp; the Release 2 key (nonzero: its values sit in the legacy slots of `rows`) */
    uint8_t effect_id, nrows;
    const fx_prow_t *rows;                              /* the stored parameters, in id order: the legacy-slot table */
    /* capabilities (FX_MODULES_PLAN.md 4) */
    uint8_t flags, rates, max_inst, prof;               /* rates: bit 0 44.1 .. 3 5.5125 kHz; prof: the audio_batch_cost class (AP_DUB) the job is pushed under */
    uint8_t ram_work_min, ram_work_max, ram_extra;      /* 4 KB blocks: the least that lets it run, the most it uses, the option's (the spring) */
    uint16_t ram_state, latency, tail_base, cost;       /* bytes of arena state; samples; blocks; microseconds a block (flag FXK_US) */
    uint16_t ram_extra_bytes, gov_us;                   /* the option's bytes; what an instance adds to the heaviest half (the governor's headroom) */
    /* the calls (fx_kcalls_t's members, in its order) */
    uint32_t (*on)(const track_t *t);
    uint32_t (*size_sig)(const track_t *t, const fx_ictx_t *c);
    float (*size_need)(const track_t *t, const fx_ictx_t *c);
    uint32_t (*blocks_at)(float need, uint32_t R);
    uint32_t (*hold_len)(float need, uint32_t blocks, uint32_t R);
    void (*tick)(uint32_t *kw, const fx_ictx_t *c);
    uint32_t (*param_sig)(const track_t *t, const fx_ictx_t *c);
    void (*snap)(void *params, const track_t *t, fx_ictx_t *c);
    uint32_t (*cost_extra)(const track_t *t, uint32_t extra);
    void (*start)(void *state, uint32_t R, int16_t *const *blk, uint32_t nblk, int16_t *extra);
    void (*process)(void *state, const void *params, const float *in, float *outL, float *outR);
    void (*reset)(void);
} fx_kernel_t;

/* ---- the module side: the caps block an fx_entry's `pages` word points at (fw_fx.h checks it; fx_insert.c fx_modk_set makes a fx_kernel_t from it) ------------------------------------
 * 56 bytes, word aligned, inside the module's image. Pointers are 4 byte addresses of the module's own address space (0 in a host build of the module, which hands the host its call table and
 * rows directly). The words are MODULE_FORMAT_R3.md 6's fx_entry_t words (names and widths) that the step 4 entry had no room for. */
#define FX_CAPS_MAGIC 0x4B584647u                       /* "GFXK" little endian */
#define FX_CAPS_VERSION 1u
#define FX_CAPS_WIRE 56u
typedef struct fx_caps {
    uint32_t magic;                                     /* FX_CAPS_MAGIC */
    uint16_t size, version;                             /* FX_CAPS_WIRE, FX_CAPS_VERSION */
    uint32_t kernel;                                    /* -> fx_kcalls_t, in the code */
    uint32_t pages;                                     /* -> fw_pages_t (version 3, scope SC_FX) or 0: the effect's pages and stable ids */
    uint32_t hot;                                       /* -> eng_hot_t[nhot]: the tables to copy to the arena. 0 and nhot 0 in version 1 (a nonzero value is refused: DUB needs none, FX_MODULES_PLAN.md 15) */
    uint32_t legacy_module;                             /* 0, or the Release 2 key of the resident effect this one stands for */
    uint32_t rows;                                      /* -> fx_prow_t[nrows]: the legacy-slot table, or 0 */
    uint8_t flags, rates, max_inst, prof;
    uint8_t ram_work_min, ram_work_max, ram_extra, nhot;
    uint8_t nrows, pad;
    uint16_t param_bytes;                               /* the size of the parameter snapshot `snap` writes (the host's room is FX_PARAM_BYTES: a larger one is refused) */
    uint16_t ram_state, latency;
    uint16_t tail_base, cost;
    uint16_t ram_extra_bytes, gov_us;
    uint32_t abi;                                       /* FX_ABI_ID of the build the module was made with: the layout of fx_ictx_t, fx_kcalls_t and this block */
} fx_caps_t;
_Static_assert(sizeof(fx_caps_t) == FX_CAPS_WIRE, "the caps block's layout");
