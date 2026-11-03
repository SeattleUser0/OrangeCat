/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 3 (docs/orangecat/MODULE_FORMAT_R3.md 4 and 16): what an effect module's pid table means for the values it holds, on the descriptor's wire bytes (the reader of fw_pages.h), one place:
 *   fx_legacy_import   a Release 2 project's values into the module's: each panel entry whose pid has a legacy slot takes the value from the track's p[] or (L1, FW_LEG_GLOBAL) from song.g[],
 *                      clamped to the entry's range; the others take their defaults. The descriptor's `ext` (L1) is the condition: if it names a song.g index and a value, the legacy slots are imported
 *                      only while song.g[index] equals the value (the reverb: the module of a mode takes the one stored SIZE, DAMP, PRE ... only when TYPE is its mode; the others stay at their defaults,
 *                      the Release 2 byte had no value for them)
 *   fx_legacy_export   the module's values back into the legacy slots (the host drives the kernel, or a resident stand-in, from the slots: the first implementation of FX_MODULES_PLAN.md 3's store)
 *   fx_store_pack/unpack  the values a project STORES: (pid, value) pairs, for the entries that are stored. Not stored: a view (computed by the hooks from stored ids), a volatile pid (L3: RAM only) and a
 *                      relative pid (L4: a turn, no value), and an entry with no label (removed). The project writer and reader of plan step 7 use these two.
 * Host and device alike (header only, no libc); not included by the firmware until the host of plan step 4/5 uses it. */
#pragma once
#include "fw_pages.h"
static inline int fw_pid_stored(uint32_t id) { return !(id & FW_PID_FLAGS) && id != FW_PID_NONE; }
static inline int fxs_i16(const uint8_t *b) { return (int16_t)(b[0] | b[1] << 8); }
/* the panel entry i and its pid row of the descriptor at `desc`: 0 on success */
static inline int fxs_entry(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t i, uint8_t *pe, uint32_t *id, uint32_t *leg, uint32_t *np)
{
    uint8_t d[FW_PAGES_WIRE], q[FW_PID_WIRE];
    uint32_t panel, pidp;
    if (rd(ctx, desc, d, FW_PAGES_WIRE)) return 1;
    *np = d[10] | d[11] << 8; panel = fwp_u32(d + 16); pidp = fwp_u32(d + 72);
    if (i >= *np) return 1;
    if (rd(ctx, panel + i * FW_PAGES_DESC_WIRE, pe, FW_PAGES_DESC_WIRE)) return 1;
    *id = FW_PID_NONE; *leg = FW_PID_NONE;
    if (pidp) {
        if (rd(ctx, pidp + i * FW_PID_WIRE, q, FW_PID_WIRE)) return 1;
        *id = q[0] | q[1] << 8; *leg = q[2] | q[3] << 8;
    }
    return 0;
}
/* values[0 .. np): the module's value of each panel entry. Returns np, or 0 when the descriptor cannot be read */
static inline uint32_t fx_legacy_import(fw_pages_rd rd, void *ctx, uint32_t desc, const int16_t *p, const int16_t *g, int16_t *values)
{
    uint8_t d[FW_PAGES_WIRE], pe[FW_PAGES_DESC_WIRE];
    uint32_t i, np, id, leg, ext, cond_ok = 1;
    if (rd(ctx, desc, d, FW_PAGES_WIRE)) return 0;
    np = d[10] | d[11] << 8; ext = fwp_u32(d + 76);
    if (ext) cond_ok = g[(ext & 0xFFFFu) - 1u] == (int16_t)(ext >> 16);
    for (i = 0; i < np; i++) {
        int32_t v, mn, mx;
        if (fxs_entry(rd, ctx, desc, i, pe, &id, &leg, &np)) return 0;
        mn = fxs_i16(pe + 6); mx = fxs_i16(pe + 8); v = fxs_i16(pe + 10);
        if (leg != FW_PID_NONE && cond_ok) v = (leg & FW_LEG_GLOBAL) ? g[leg & 0x7FFFu] : p[leg];
        values[i] = (int16_t)(v < mn ? mn : v > mx ? mx : v);
    }
    return np;
}
/* the values back into the legacy slots (those entries that have one) */
static inline uint32_t fx_legacy_export(fw_pages_rd rd, void *ctx, uint32_t desc, const int16_t *values, int16_t *p, int16_t *g)
{
    uint8_t pe[FW_PAGES_DESC_WIRE];
    uint32_t i, np = 1, id, leg, n = 0;
    for (i = 0; i < np; i++) {
        if (fxs_entry(rd, ctx, desc, i, pe, &id, &leg, &np)) return 0;
        if (leg == FW_PID_NONE) continue;
        if (leg & FW_LEG_GLOBAL) g[leg & 0x7FFFu] = values[i]; else p[leg] = values[i];
        n++;
    }
    return n;
}
/* what a project stores of this instance: (pid u16, value i16) little endian pairs; the number of bytes, or 0 when cap is too small or the descriptor cannot be read */
static inline uint32_t fx_store_pack(fw_pages_rd rd, void *ctx, uint32_t desc, const int16_t *values, uint8_t *out, uint32_t cap)
{
    uint8_t pe[FW_PAGES_DESC_WIRE];
    uint32_t i, np = 1, id, leg, n = 0;
    for (i = 0; i < np; i++) {
        if (fxs_entry(rd, ctx, desc, i, pe, &id, &leg, &np)) return 0;
        if (!fwp_u32(pe) || !fw_pid_stored(id)) continue;          /* removed, a view, volatile, relative: not stored */
        if (n + 4u > cap) return 0;
        out[n] = (uint8_t)id; out[n + 1] = (uint8_t)(id >> 8); out[n + 2] = (uint8_t)values[i]; out[n + 3] = (uint8_t)((uint16_t)values[i] >> 8);
        n += 4u;
    }
    return n;
}
/* the stored pairs into values[] (the caller starts them at the defaults): a pid the module does not have, or one it does not store, is ignored. Returns the pairs taken */
static inline uint32_t fx_store_unpack(fw_pages_rd rd, void *ctx, uint32_t desc, const uint8_t *in, uint32_t n, int16_t *values)
{
    uint8_t pe[FW_PAGES_DESC_WIRE];
    uint32_t k, i, np, id, leg, taken = 0;
    for (k = 0; k + 4u <= n; k += 4u) {
        uint32_t want = in[k] | in[k + 1] << 8;
        np = 1;
        for (i = 0; i < np; i++) {
            if (fxs_entry(rd, ctx, desc, i, pe, &id, &leg, &np)) return taken;
            if (id == want && fwp_u32(pe) && fw_pid_stored(id)) {
                int32_t mn = fxs_i16(pe + 6), mx = fxs_i16(pe + 8), v = fxs_i16(in + k + 2);
                values[i] = (int16_t)(v < mn ? mn : v > mx ? mx : v);
                taken++;
                break;
            }
        }
    }
    return taken;
}
