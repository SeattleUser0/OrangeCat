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
 *     12  pages      0, or (Release 3 step 6) the address of the effect's CAPS BLOCK (fx_caps_t, fx_kernel.h: 52 bytes, 4 aligned, inside the image): the capability words of
 *                    MODULE_FORMAT_R3.md 6's fx_entry_t, the kernel's call table (FX_KCALLS_N code addresses), the effect's fw_pages_t (version 3, scope SC_FX: its pages, stable ids and
 *                    legacy slots) and the legacy-slot rows. fx_caps_check says what is checked. 0: an effect module with no kernel (registered and listed; nothing runs). Any other
 *                    word is refused. (The step 4 entry had no room for the words of section 6's 32 byte entry; this keeps the 16 byte entry, and every earlier image, valid.)
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
#include "fx_kernel.h"                                  /* (step 6) the caps block and the call table */
#define FX_BANK_MAX 8u
#define FX_PARAMS_MAX 64u
#define FX_DESC_WIRE 20u
#define FX_ENTRY_WIRE 16u
#define FX_PARAM_WIRE 16u
enum { FXD_OK = 0, FXD_ADDR = 1, FXD_READ, FXD_COUNT, FXD_ID, FXD_ENTRY, FXD_KIND, FXD_PARAMS, FXD_ROW, FXD_STR, FXD_SCENE, FXD_RSV, FXD_CAPS, FXD_KERN, FXD_PAGES };   /* (step 6) FXD_CAPS: the caps block; FXD_KERN: the call table; FXD_PAGES: the effect's fw_pages_t */
static inline uint32_t fxd_u16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }
/* the descriptor and its entry as the module's source writes them (4 byte words, the device layout; a host build of a module has no 4 byte pointers and fills these by hand) */
typedef struct { uint32_t n, module_id, version, entries, scene; } fx_desc_wire_t;
typedef struct { uint8_t effect_id, kind; uint16_t flags, nparams, rsv; uint32_t params, pages; } fx_entry_wire_t;
_Static_assert(sizeof(fx_desc_wire_t) == FX_DESC_WIRE && sizeof(fx_entry_wire_t) == FX_ENTRY_WIRE, "the descriptor's layout");
#define FX_PROW_WIRE 10u                                /* one legacy-slot row: id, legacy, min, max, def (fx_prow_t) */
#define FX_ROWS_MAX 16u
/* the caps block at `caps` (an entry's `pages`): magic, size 52, version FX_CAPS_VERSION; the call table inside the image, 4 aligned, each of its FX_KCALLS_N words a nonzero address inside the
 * code; pages 0 or a version 3 effect descriptor that fw_pages_check_ex accepts as SC_FX in this build; hot and nhot 0 (a hot set is not in version 1); rows 0, or 1 .. FX_ROWS_MAX rows of
 * FX_PROW_WIRE bytes (2 aligned) with ids strictly increasing, legacy below P_COUNT (or 0xFFFF), min <= def <= max; the padding 0. The capability numbers (RAM, cost, instances) are range-checked
 * by the host when it builds the kernel (fx_modk_set), not here. 0 FXD_OK, else why not */
static inline int fx_caps_check(fw_pages_rd rd, void *ctx, uint32_t caps, uint32_t lo, uint32_t hi)
{
    uint8_t c[FX_CAPS_WIRE], t[FX_KCALLS_WIRE], r[FX_PROW_WIRE];
    uint32_t kernel, pages, rows, nrows, i, prev = 0xFFFFFFFFu;
    if (!fwp_in(caps, FX_CAPS_WIRE, lo, hi) || (caps & 3u)) return FXD_CAPS;
    if (rd(ctx, caps, c, FX_CAPS_WIRE)) return FXD_READ;
    if (fwp_u32(c) != FX_CAPS_MAGIC || fxd_u16(c + 4) != FX_CAPS_WIRE || fxd_u16(c + 6) != FX_CAPS_VERSION) return FXD_CAPS;
    kernel = fwp_u32(c + 8); pages = fwp_u32(c + 12); rows = fwp_u32(c + 24); nrows = c[36];
    if (fwp_u32(c + 16) || c[35] || c[37] || !fxd_u16(c + 38)) return FXD_CAPS;   /* hot, nhot, the padding, the snapshot's size (not 0) */
    if (!fwp_in(kernel, FX_KCALLS_WIRE, lo, hi) || (kernel & 3u)) return FXD_KERN;
    if (rd(ctx, kernel, t, FX_KCALLS_WIRE)) return FXD_READ;
    for (i = 0; i < FX_KCALLS_N; i++) {
        uint32_t a = fwp_u32(t + 4u * i);
        if (!a || !fwp_in(a, 2u, lo, hi)) return FXD_KERN;
    }
    if (pages && fw_pages_check_ex(rd, ctx, pages, lo, hi, SC_FX, P_COUNT, G_COUNT, 0)) return FXD_PAGES;
    if (!rows != !nrows || nrows > FX_ROWS_MAX) return FXD_CAPS;
    if (rows && (!fwp_in(rows, nrows * FX_PROW_WIRE, lo, hi) || (rows & 1u))) return FXD_CAPS;
    for (i = 0; i < nrows; i++) {
        uint32_t id, leg;
        int mn, mx, df;
        if (rd(ctx, rows + i * FX_PROW_WIRE, r, FX_PROW_WIRE)) return FXD_READ;
        id = fxd_u16(r); leg = fxd_u16(r + 2); mn = (int16_t)fxd_u16(r + 4); mx = (int16_t)fxd_u16(r + 6); df = (int16_t)fxd_u16(r + 8);
        if ((prev != 0xFFFFFFFFu && id <= prev) || (leg != 0xFFFFu && leg >= P_COUNT) || mn > mx || df < mn || df > mx) return FXD_CAPS;
        prev = id;
    }
    return FXD_OK;
}
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
        if (fxd_u16(e + 2) || fxd_u16(e + 6)) return FXD_RSV;
        if (fwp_u32(e + 12)) { int rc = fx_caps_check(rd, ctx, fwp_u32(e + 12), lo, hi); if (rc) return rc; }   /* (step 6) the caps block; step 4 refused any word */
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
