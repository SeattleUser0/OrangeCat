/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat Release 3 (the model picker, docs/candidates/model_picker.md): the descriptor a MODELS module's header points at
 * (fw_mod_hdr_t.engine_ptr, kind FW_MOD_KIND_MODELS: "sound models for engine N", the engine number in the word's low byte). The
 * module carries no engine of its own: it adds models to a built-in engine (MACRO, eng_macro.c), one unit a model, each a region
 * of the master image the build links (tools/build.py --module plaits), from which the installer page packs the models the user
 * picked (tools/pick_module.py, web/fm1pick.js). The loader (oc_module.c) checks this block pointer by pointer against the image
 * before it registers anything, as fw_fx.h does for an effect module, and hands the engine the table of model descriptors
 * (engines.c eng_models_set). Every field is a little-endian 32 bit word at a fixed offset, read through a reader, so the checks run
 * on the host builds too (a host build of a module has no 4 byte pointers and fills the words by hand).
 *
 *   fw_models (FW_MODELS_WIRE = 160 bytes, 4 aligned, inside the image)
 *     0    magic      "OMDL"
 *     4    size       16 bits: 160
 *     6    version    16 bits: 1
 *     8    abi        PL_MDL_ABI_ID of the build the module was made with (tools/gen_fw_api_id.py: the layout of pl_mdl_t, the part a
 *                     model renders into, and this block): a module of another layout is refused, and FW_API_ID does not move for it
 *     12   engine     the engine number the models are for: the header word's low byte
 *     16   n          models the table has, 1 .. FW_MODELS_MAX (the engine's own count, PL_N: a table may be shorter or longer than
 *                     the firmware's; the engine reads the ids both have)
 *     20   models[32] n used: 0 (the pick left the model out) or the address of its pl_mdl_t (plaits/pl_mdl.h: PL_MDL_WIRE bytes,
 *                     4 aligned, inside the code), whose start and render words are addresses inside the code
 *     148  banks[3]   0 or the address of a wave bank inside the code (WAVTBL needs one, TERRAN uses those present)
 * The header's state_size is the arena a part needs for the largest picked model (the packer writes it): the loader computes the
 * same from the descriptors (fw_models_need) and refuses a header that says less, or more than the engine's block (eng_models_max).
 * The pick block (tools/pick_module.py, at 64 in the header page) is the page's and the card's: the loader does not read it. */
#pragma once
#include "fw_pages.h"                                   /* fw_pages_rd, fwp_in, fwp_u32 */
#define FW_MODELS_MAGIC 0x4C444D4Fu                     /* "OMDL" little endian */
#define FW_MODELS_WIRE 160u
#define FW_MODELS_VERSION 1u
#define FW_MODELS_MAX 32u
#define FW_MODELS_AT 20u                                /* the models table's offset in the block */
#define FW_MODELS_BANKS_AT 148u
#define FW_MODELS_NBANKS 3u
/* pl_mdl_t as the module's source writes it (plaits/pl_mdl.h): the words the loader reads */
#define PL_MDL_WIRE 24u
#define PL_MDL_STATE_AT 0u
#define PL_MDL_BIG_AT 4u
#define PL_MDL_UNITS_AT 7u
#define PL_MDL_START_AT 16u
#define PL_MDL_RENDER_AT 20u
/* the part a model renders into (plaits/pl_mdl.h pl_part_t): the voice heads, then the model states; what a part needs for a model */
#define PL_POLY 4u
#define PL_PART_HEAD 184u                               /* offsetof(pl_part_t, m); checked by a _Static_assert in pl_mdl.h */
#define PL_STATE_MAX 11264u                             /* the arena block a MACRO part holds, enough for the largest of the 25 models (HARP: four voices of 2,744 B);
                                                         * a module whose pick needs more is refused (eng_models_max) */
static inline uint32_t pl_stride(uint32_t state) { return (state + 7u) & ~7u; }
static inline uint32_t pl_need(uint32_t state, uint32_t big) { return PL_PART_HEAD + (big ? 1u : PL_POLY) * pl_stride(state); }
/* the block as the module's source writes it (a device build: the addresses; a host build: 0, the harness hands the engine its pointers) */
typedef struct { uint32_t magic; uint16_t size, version; uint32_t abi, engine, n, models[FW_MODELS_MAX], banks[FW_MODELS_NBANKS]; } fw_models_wire_t;
_Static_assert(sizeof(fw_models_wire_t) == FW_MODELS_WIRE, "the models block's layout");
/* the module id a models module goes by in the installer (the Effects table, FX_INFO, FX_BEGIN, FX_REMOVE: the id-keyed install path,
 * MODULE_IDS.md): "models for engine e". Orange Cat's own range (bit 31 clear). */
#define FW_MODELS_ID(e) (0x4D00u | ((e) & 0xFFu))
#define FW_MODELS_ID_P(id) (((id) & ~0xFFu) == 0x4D00u)
enum { FWM_OK = 0, FWM_ADDR = 1, FWM_READ, FWM_MAGIC, FWM_VERSION, FWM_ABI, FWM_ENGINE, FWM_COUNT, FWM_MODEL, FWM_CODE, FWM_BANK, FWM_STATE };
/* desc: the block's address in the module's own address space; [lo, hi) the code part of the image (link + FW_MOD_CODE_OFF .. link + length);
 * engine: the header word's low byte; abi: PL_MDL_ABI_ID of this firmware. 0 FWM_OK, else why not. When it is good: *n the table's length,
 * *need the arena a part needs for the largest model present (0 with none present), *present how many models the table has. */
static inline int fw_models_check(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi, uint32_t engine, uint32_t abi,
                                  uint32_t *n, uint32_t *need, uint32_t *present)
{
    uint8_t b[FW_MODELS_WIRE], d[PL_MDL_WIRE];
    uint32_t cnt, i, mx = 0, have = 0;
    if (!fwp_in(desc, FW_MODELS_WIRE, lo, hi) || (desc & 3u)) return FWM_ADDR;
    if (rd(ctx, desc, b, FW_MODELS_WIRE)) return FWM_READ;
    if (fwp_u32(b) != FW_MODELS_MAGIC) return FWM_MAGIC;
    if (((uint32_t)b[4] | (uint32_t)b[5] << 8) != FW_MODELS_WIRE || ((uint32_t)b[6] | (uint32_t)b[7] << 8) != FW_MODELS_VERSION) return FWM_VERSION;
    if (fwp_u32(b + 8) != abi) return FWM_ABI;
    if (fwp_u32(b + 12) != engine) return FWM_ENGINE;
    cnt = fwp_u32(b + 16);
    if (!cnt || cnt > FW_MODELS_MAX) return FWM_COUNT;
    for (i = 0; i < FW_MODELS_MAX; i++) {
        uint32_t a = fwp_u32(b + FW_MODELS_AT + 4u * i), st, bg;
        if (i >= cnt) { if (a) return FWM_COUNT; continue; }   /* past the table: zero */
        if (!a) continue;
        if (!fwp_in(a, PL_MDL_WIRE, lo, hi) || (a & 3u)) return FWM_MODEL;
        if (rd(ctx, a, d, PL_MDL_WIRE)) return FWM_READ;
        st = fwp_u32(d + PL_MDL_STATE_AT); bg = d[PL_MDL_BIG_AT];
        if (!fwp_in(fwp_u32(d + PL_MDL_START_AT), 2u, lo, hi) || !fwp_in(fwp_u32(d + PL_MDL_RENDER_AT), 2u, lo, hi)) return FWM_CODE;
        if (!st || st > 0xFFFFu || bg > 1u || !d[PL_MDL_UNITS_AT]) return FWM_STATE;
        if (pl_need(st, bg) > mx) mx = pl_need(st, bg);
        have++;
    }
    for (i = 0; i < FW_MODELS_NBANKS; i++) {
        uint32_t a = fwp_u32(b + FW_MODELS_BANKS_AT + 4u * i);
        if (a && (!fwp_in(a, 4u, lo, hi) || (a & 1u))) return FWM_BANK;
    }
    if (n) *n = cnt;
    if (need) *need = mx;
    if (present) *present = have;
    return FWM_OK;
}
