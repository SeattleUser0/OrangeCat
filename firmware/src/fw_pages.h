/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.0 (Part 2b): the page descriptor of a loadable engine. A module brings its pages with it: the rows the UI shows (the PAGES row format),
 * the table of its native parameters (what motion, the editor and the pages read: param_desc_t), and the few hooks that need the engine's own code. All of it
 * is in the module's flash and is read in place: nothing is copied into RAM at registration. engine_t.pages (core.h) points at it; the loader
 * (oc_module.c) checks every pointer in it against the module's image before it registers the engine (fw_pages_check, below), so the UI never follows one
 * that leaves the image.
 *
 * The page lookup (params.c page_at, page_n): the firmware's PAGES come first (indices 0 .. NPAGES - 1). Each module engine that may bring pages (MODPAGE_ENG)
 * has FW_PAGES_MAX indices of its own after them, in the order of that list, so a page keeps its index while another module comes and goes. A module that is
 * not registered, or has fewer rows, leaves its indices empty (a page that is never shown).
 *
 * The rows: fam as a built-in page (FAM_EDIT for sound pages), scope SC_MOD, graph GR_NONE; id[k] is an index into the panel (0xFF = empty). The panel:
 * panel[i].label 0 means no parameter i. A native parameter is one byte of the track's native block, byte i = panel index i (the Prophet's patch bytes);
 * `native(t)` returns the block, or 0 when the track has none. A value outside panel[i].min .. max is never written.
 *
 * Hooks (all optional, 0 = the default; they run in the UI context, never in the audio interrupt):
 *   native(t)       the track's native block (index = panel index); the default has none: the pages show nothing and motion moves nothing
 *   shown(t,id,&v)  the descriptor and value to SHOW for id now when they differ from panel[id] and the byte (the Prophet's keyed tuning, a default unison
 *                   count, a bend shown from 1); 0 = panel[id] and the byte
 *   put(t,id,v)     a new value for id (the engine takes it into account: the Prophet re-reads its voices); 0 = the clamped byte
 *   active(t,id)    0: the parameter has no effect on the sound as it is, and is drawn dim; 0 hook = always active
 * motion_skip: bit i set = motion does not move parameter i (the Prophet's voice assignment); 0 = none skipped.
 * env_titles, lfo_titles: 0-terminated lists of title prefixes of the pages that take the ENV and LFO tabs for this engine (ui.c native_titles); 0 = none.
 * knob: the panel indices of HOME's four knobs (0xFF = none).
 *
 * The preset list and the names are the engine_t's own (npresets, preset_make, in the module's code): nothing more is needed here.
 *
 * Revisions:
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 * (Orange Cat 0.12.2) fw_pages_check bounds what param_format reads; fuzz_fw_pages; fuzzers in run_tests.sh; the module link overflow lists the biggest symbols
 *   0.12.0: first version. FAM_, SC_, GR_ and page_t moved here from params.c (unchanged); SC_MOD added at the end of SC_.
 *   0.12.1: version 2 (68 bytes): the hooks loaded, env_source and bend and the cc list, which the Prophet needs to be a module (its patch follows the preset, its
 *           envelope is the matrix's ENV source, its patch sets the MIDI wheel's range, MIDI CC 72, 73, 75 edit its amplitude envelope).
 */
#pragma once
#include <stdint.h>

enum { FAM_HOME, FAM_ENV, FAM_LFO, FAM_SCL, FAM_EDIT, FAM_GLO, FAM_SAVE, FAM_ARP, FAM_SEQ, FAM_TRK,
       FAM_FX, FAM_COUNT };
enum { SC_TRACK, SC_GLOBAL, SC_ENGINE, SC_STEP, SC_TRK,   /* SC_TRK: the TRACKS page (ui_input.c tracks_edit) */
       SC_FM6, SC_FMOP, SC_CZ, SC_CZ1, SC_P5, SC_P5STORE, SC_DRUM, SC_DRUMHIT,                         /* FM6: its patch, functions; operator fm6_opsel;
                                                                  * CZ1: a native CZ-1 tone (cz_edit.h) */
       SC_MOD,                                            /* (0.12.0) a page of a module's descriptor (fw_pages_t) */
       SC_DXLANE };                                       /* (Orange Cat 0.14.2) DRUM-X's lane pages: the kit's values of the lane the keys
                                                           * picked (params.c dxl_desc). After SC_MOD: a module's numbers stay */
enum { GR_NONE, GR_ADSR, GR_LFO, GR_STEPS, GR_ARP, GR_SCALE, GR_ROLL, GR_BROWSE, GR_SLOTS, GR_USER, GR_TRK,
       GR_MOD, GR_PATS, GR_SONG, GR_MOTION, GR_CHORD, GR_SLICES,
       GR_FMEG, GR_FMPEG, GR_FMSTORE, GR_CZTOOLS, GR_SCALE_PICKER, GR_DRUMHIT, GR_PATGRID,
       GR_FXRACK, GR_TRACKER, GR_POOL, GR_GROOVE }; /* NOTES shares GR_ROLL for steps and recorded events; (Orange Cat
                                                    * 0.4.2) TRACKER, (0.4.4) PHRASES, (0.4.5) GROOVE */

typedef struct {
    const char *title;
    uint8_t fam, scope, graph;
    uint8_t id[4];               /* param ids; 0xFF = empty slot */
} page_t;

#define FW_PAGES_MAGIC 0x47415047u                       /* "GPAG" little endian */
#define FW_PAGES_VERSION 2u                              /* (0.12.1) 2: loaded, env_source, bend and cc added; 0.12.0's descriptor (version 1) was never in a released module */
#define FW_PAGES_MAX 24u                                 /* rows a module may bring: its indices after PAGES */
#define FW_PANEL_MAX 128u                                /* native parameters a module may describe (motion's MO_NATIVE_N) */
#define FW_PAGES_WIRE 68u                                /* sizeof(fw_pages_t) on the device (4 byte pointers); the loader reads this layout */
#define FW_PAGES_ROW_WIRE 12u                            /* sizeof(page_t) on the device */
#define FW_PAGES_DESC_WIRE 20u                           /* sizeof(param_desc_t) on the device */
typedef struct fw_pages {
    uint32_t magic;                                      /* FW_PAGES_MAGIC */
    uint16_t size, version;                              /* sizeof(fw_pages_t), FW_PAGES_VERSION */
    uint16_t npages, npanel;                             /* rows (<= FW_PAGES_MAX), panel entries (<= FW_PANEL_MAX) */
    const page_t *rows;
    const param_desc_t *panel;
    uint8_t *(*native)(struct track *t);
    const param_desc_t *(*shown)(const struct track *t, uint32_t id, int16_t *value);
    void (*put)(struct track *t, uint32_t id, int32_t value);
    int (*active)(const struct track *t, uint32_t id);
    const uint8_t *motion_skip;                          /* (npanel + 7) / 8 bytes, or 0 */
    const char *const *env_titles;
    const char *const *lfo_titles;
    uint8_t knob[4];
    void (*loaded)(struct track *t, uint32_t preset);    /* (0.12.1) a preset was loaded on the track: the engine takes its own patch from it (ui.c preset_apply) */
    int32_t (*env_source)(struct track *t, const voice_t *v);   /* (0.12.1) the modulation matrix's ENV source for an engine with envelopes of its own (voice.c) */
    int32_t (*bend)(const struct track *t);              /* (0.12.1) the MIDI wheel's range in 1/256 semitones when the patch sets it; 0 = the channel's own */
    uint8_t cc[4];                                       /* (0.12.1) panel indices that MIDI CC 72 (release), 73 (attack), 75 (decay) set on the amplitude envelope; 0xFF = none */
} fw_pages_t;
_Static_assert(sizeof(void *) != 4 || sizeof(fw_pages_t) == FW_PAGES_WIRE, "the descriptor's device layout");
_Static_assert(sizeof(void *) != 4 || sizeof(page_t) == FW_PAGES_ROW_WIRE, "the row's device layout");
_Static_assert(sizeof(void *) != 4 || sizeof(param_desc_t) == FW_PAGES_DESC_WIRE, "the panel entry's device layout");

/* ---- the loader's check of a descriptor, on the bytes of the module's image ------------------------------------------------------------------------------
 * `rd(ctx, addr, buf, n)` reads n bytes of the image at the XIP address addr (0 on success); [lo, hi) is the image (code from FW_MOD_CODE_OFF to the end of
 * the image). Returns 0 for a descriptor the UI may follow, else a nonzero code (the first thing found wrong). It reads the DEVICE layout (4 byte words), so
 * it is the same on the host: a test builds the wire bytes by hand. */
enum { FWP_OK = 0, FWP_ADDR = 1, FWP_READ, FWP_MAGIC, FWP_SIZE, FWP_COUNT, FWP_ROW, FWP_PANEL, FWP_STR, FWP_HOOK, FWP_SKIP, FWP_TITLES, FWP_KNOB };
typedef int (*fw_pages_rd)(void *ctx, uint32_t addr, void *buf, uint32_t n);
static inline uint32_t fwp_u32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
static inline int fwp_in(uint32_t a, uint32_t n, uint32_t lo, uint32_t hi) { return a >= lo && a <= hi && n <= hi - a; }
/* a NUL terminated string of at most 24 bytes wholly inside the image */
static inline int fwp_str(fw_pages_rd rd, void *ctx, uint32_t a, uint32_t lo, uint32_t hi)
{
    uint8_t c;
    for (uint32_t i = 0; i < 24u; i++) {
        if (!fwp_in(a + i, 1u, lo, hi) || rd(ctx, a + i, &c, 1u)) return 0;
        if (!c) return 1;
    }
    return 0;
}
static inline int fw_pages_check(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi)
{
    uint8_t d[FW_PAGES_WIRE], r[FW_PAGES_ROW_WIRE], p[FW_PAGES_DESC_WIRE], w[4];
    uint32_t i, n, np, rows, panel, skip, env, lfo, h;
    if (!fwp_in(desc, FW_PAGES_WIRE, lo, hi) || (desc & 3u)) return FWP_ADDR;
    if (rd(ctx, desc, d, FW_PAGES_WIRE)) return FWP_READ;
    if (fwp_u32(d) != FW_PAGES_MAGIC) return FWP_MAGIC;
    if ((d[4] | d[5] << 8) != FW_PAGES_WIRE || (d[6] | d[7] << 8) != FW_PAGES_VERSION) return FWP_SIZE;
    n = d[8] | d[9] << 8; np = d[10] | d[11] << 8;
    if (!n || n > FW_PAGES_MAX || !np || np > FW_PANEL_MAX) return FWP_COUNT;
    rows = fwp_u32(d + 12); panel = fwp_u32(d + 16);
    if (!fwp_in(rows, n * FW_PAGES_ROW_WIRE, lo, hi) || (rows & 3u)) return FWP_ROW;
    if (!fwp_in(panel, np * FW_PAGES_DESC_WIRE, lo, hi) || (panel & 3u)) return FWP_PANEL;
    for (i = 0; i < 7u; i++) {                           /* the seven hooks that may be code: inside the image after the header page, or 0 */
        h = fwp_u32(d + (i < 4u ? 20u + 4u * i : 52u + 4u * (i - 4u)));
        if (h && !fwp_in(h, 2u, lo, hi)) return FWP_HOOK;
    }
    skip = fwp_u32(d + 36);
    if (skip && !fwp_in(skip, (np + 7u) / 8u, lo, hi)) return FWP_SKIP;
    env = fwp_u32(d + 40); lfo = fwp_u32(d + 44);
    for (h = 0; h < 2u; h++) {                           /* the title lists: up to 8 string pointers and a 0 */
        uint32_t list = h ? lfo : env;
        if (!list) continue;
        for (i = 0;; i++) {
            if (i >= 9u || !fwp_in(list + 4 * i, 4u, lo, hi) || (list & 3u) || rd(ctx, list + 4 * i, w, 4u)) return FWP_TITLES;
            if (!fwp_u32(w)) break;
            if (!fwp_str(rd, ctx, fwp_u32(w), lo, hi)) return FWP_TITLES;
        }
    }
    for (i = 0; i < 4u; i++)
        if ((d[48 + i] != 0xFFu && d[48 + i] >= np) || (d[64 + i] != 0xFFu && d[64 + i] >= np)) return FWP_KNOB;
    for (i = 0; i < n; i++) {
        uint32_t k;
        if (rd(ctx, rows + i * FW_PAGES_ROW_WIRE, r, FW_PAGES_ROW_WIRE)) return FWP_READ;
        if (!fwp_str(rd, ctx, fwp_u32(r), lo, hi)) return FWP_STR;
        if (r[4] >= FAM_COUNT || r[5] != SC_MOD || r[6] != GR_NONE) return FWP_ROW;
        for (k = 0; k < 4u; k++)
            if (r[7 + k] != 0xFFu && r[7 + k] >= np) return FWP_ROW;
    }
    for (i = 0; i < np; i++) {
        uint32_t label, names, unit;
        int mn, mx;
        if (rd(ctx, panel + i * FW_PAGES_DESC_WIRE, p, FW_PAGES_DESC_WIRE)) return FWP_READ;
        label = fwp_u32(p); names = fwp_u32(p + 12); unit = fwp_u32(p + 16);
        mn = (int16_t)(p[6] | p[7] << 8); mx = (int16_t)(p[8] | p[9] << 8);
        if (!label) continue;                            /* no parameter i */
        if (!fwp_str(rd, ctx, label, lo, hi) || (unit && !fwp_str(rd, ctx, unit, lo, hi))) return FWP_STR;
        if (mn > mx) return FWP_PANEL;
        /* (0.12.2, found by tests/fuzz_fw_pages.c) what params.c's param_format reads, which the check above did not bound: the format is one it knows; an F_ENUM has its names
         * (it indexes names[] by the value clamped to min..max, so min is not below 0); a name list on an F_INT is 0-terminated inside the image (param_format counts to the 0);
         * no other format has one */
        if (p[4] > F_TONE || (p[4] == F_ENUM && (!names || mn < 0)) || (names && p[4] != F_ENUM && p[4] != F_INT)) return FWP_PANEL;
        if (names && p[4] == F_INT) {
            if (mx < 0 || mx >= 127 || !fwp_in(names, (uint32_t)(mx + 2) * 4u, lo, hi) || (names & 3u) || rd(ctx, names + 4u * (uint32_t)(mx + 1), w, 4u)) return FWP_PANEL;
            if (fwp_u32(w)) return FWP_PANEL;
        }
        if (names) {                                     /* names[0 .. max]: pointers to strings */
            if (mx < 0 || mx >= 128 || !fwp_in(names, (uint32_t)(mx + 1) * 4u, lo, hi) || (names & 3u)) return FWP_PANEL;
            for (uint32_t j = 0; j <= (uint32_t)mx; j++) {
                if (rd(ctx, names + 4 * j, w, 4u)) return FWP_READ;
                if (!fwp_str(rd, ctx, fwp_u32(w), lo, hi)) return FWP_STR;
            }
        }
    }
    return FWP_OK;
}
