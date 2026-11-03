/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 3 step 4 (FX_MODULES_PLAN.md 2, 3): the descriptor an effect module's header points at (fw_mod_hdr_t.engine_ptr, kind 1..3).
 *
 * The module is a BANK of one to eight effects with one module id, one version and one install. Every field is a little-endian 32 bit word or
 * two 16 bit halves at a fixed offset, read through a reader (fw_pages_rd) and checked pointer by pointer against the image before anything is
 * registered, as fw_pages_check does for an engine's pages; nothing here depends on the host's pointer size, so the checks run on the host builds too.
 *
 *   fx_desc (FX_DESC_WIRE = 20 bytes, 4 aligned, inside the image)
 *     0   n          number of effects, 1 .. FX_BANK_MAX (8)
 *     4   module_id  32 bits, not 0. Orange Cat's own: 1 .. 0x7FFFFFFF (docs/orangecat/MODULE_IDS.md); anyone else: bit 31 set
 *     8   version    0x00MMmmpp
 *     12  entries    address of n entries of FX_ENTRY_WIRE (16) bytes, 4 aligned, inside the image
 *     16  scene      0, or an address inside the image: RESERVED for the interface's picture (Session C's scene_ok validates it before a draw; this
 *                    check only keeps the pointer inside the image). Where it sits relative to fw_pages_t's own scene field: R3_STEP4_NOTES.md
 *   entry (16 bytes)
 *     0   effect_id  8 bits, stable, unique in the bank
 *     1   kind       1 insert, 2 send, 3 master: equal to the header's kind
 *     2   flags      16 bits, 0 for now (reserved: a nonzero value is refused so the bits can be given a meaning later)
 *     4   nparams    0 .. FX_PARAMS_MAX (64)
 *     6   rsv        0
 *     8   params     0 when nparams is 0, else the address of nparams parameter rows of FX_PARAM_WIRE (16) bytes, 4 aligned, inside the image
 *     12  pages      0: RESERVED for the effect's pages (open: FX_MODULES_PLAN.md 2, decided with fw_pages version 3); a nonzero value is refused for now
 *   parameter row (16 bytes)
 *     0   id         16 bits, stable for the life of the module, strictly increasing down the table (ids are never reused)
 *     2   legacy     16 bits: the Release 2 track slot (P_* id) that holds it, 0xFFFF for none
 *     4   min        int16
 *     6   max        int16
 *     8   def        int16, min <= def <= max
 *     10  fmt        16 bits (a param format number; not interpreted here)
 *     12  label      0 (reserved id, not shown) or the address of a NUL terminated string of at most 24 bytes inside the image
 * Not part of fw_api_t and not part of fw_pages_t. The interface identifier (FW_API_IFACE) is NOT changed by this file: it changes once, in the release that ships the
 * first effect module (R3_STEP4_NOTES.md), because then an older firmware must refuse the module on the identifier before it ever reads the kind. */
#pragma once
#include "fw_pages.h"
#define FX_BANK_MAX 8u
#define FX_PARAMS_MAX 64u
#define FX_DESC_WIRE 20u
#define FX_ENTRY_WIRE 16u
#define FX_PARAM_WIRE 16u
enum { FXD_OK = 0, FXD_ADDR = 1, FXD_READ, FXD_COUNT, FXD_ID, FXD_ENTRY, FXD_KIND, FXD_PARAMS, FXD_ROW, FXD_STR, FXD_SCENE, FXD_RSV };
static inline uint32_t fxd_u16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }
/* desc: the address of the fx_desc in the module's own address space; [lo, hi) the code part of the image (link + FW_MOD_CODE_OFF .. link + length);
 * kind: the header's kind (1..3). 0 FXD_OK, else why not. *id / *version / *n get the descriptor's fields when it is good (may be 0). */
static inline int fx_desc_check(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi, uint32_t kind, uint32_t *id, uint32_t *version, uint32_t *n_out)
{
    uint8_t d[FX_DESC_WIRE], e[FX_ENTRY_WIRE], r[FX_PARAM_WIRE];
    uint32_t n, entries, scene, i, k, seen[8];
    if (!fwp_in(desc, FX_DESC_WIRE, lo, hi) || (desc & 3u)) return FXD_ADDR;
    if (rd(ctx, desc, d, FX_DESC_WIRE)) return FXD_READ;
    n = fwp_u32(d);
    if (!n || n > FX_BANK_MAX) return FXD_COUNT;
    if (!fwp_u32(d + 4)) return FXD_ID;
    entries = fwp_u32(d + 12); scene = fwp_u32(d + 16);
    if (!fwp_in(entries, n * FX_ENTRY_WIRE, lo, hi) || (entries & 3u)) return FXD_ENTRY;
    if (scene && (!fwp_in(scene, 1u, lo, hi) || (scene & 3u))) return FXD_SCENE;
    for (i = 0; i < n; i++) {
        uint32_t np, params;
        if (rd(ctx, entries + i * FX_ENTRY_WIRE, e, FX_ENTRY_WIRE)) return FXD_READ;
        if (e[1] != kind || e[1] < 1u || e[1] > 3u) return FXD_KIND;
        if (fxd_u16(e + 2) || fxd_u16(e + 6) || fwp_u32(e + 12)) return FXD_RSV;
        seen[i] = e[0];
        for (k = 0; k < i; k++) if (seen[k] == e[0]) return FXD_ENTRY;       /* effect ids are unique in the bank */
        np = fxd_u16(e + 4); params = fwp_u32(e + 8);
        if (np > FX_PARAMS_MAX || (!np) != (!params)) return FXD_PARAMS;
        if (!np) continue;
        if (!fwp_in(params, np * FX_PARAM_WIRE, lo, hi) || (params & 3u)) return FXD_PARAMS;
        {   uint32_t prev = 0xFFFFFFFFu;
            for (k = 0; k < np; k++) {
                int16_t mn, mx, df;
                uint32_t pid, label;
                if (rd(ctx, params + k * FX_PARAM_WIRE, r, FX_PARAM_WIRE)) return FXD_READ;
                pid = fxd_u16(r); label = fwp_u32(r + 12);
                mn = (int16_t)fxd_u16(r + 4); mx = (int16_t)fxd_u16(r + 6); df = (int16_t)fxd_u16(r + 8);
                if (prev != 0xFFFFFFFFu && pid <= prev) return FXD_ROW;       /* strictly increasing: never reused */
                prev = pid;
                if (mn > mx || df < mn || df > mx) return FXD_ROW;
                if (label && !fwp_str(rd, ctx, label, lo, hi)) return FXD_STR;
            }
        }
    }
    if (id) *id = fwp_u32(d + 4);
    if (version) *version = fwp_u32(d + 8);
    if (n_out) *n_out = n;
    return FXD_OK;
}
