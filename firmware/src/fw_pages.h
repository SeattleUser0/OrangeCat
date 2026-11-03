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
 * (Release 3, step 3, branch r3-pages) version 3 (80 bytes): ui (fw_ui_t: the picture and HOME's four knobs), pid (stable parameter ids and legacy slots), ext;
 *   SC_FX; fw_pages_check takes the scope and the build's P_COUNT; fw_ui_check (a bad picture or knob table is not a refusal); docs/orangecat/MODULE_FORMAT_R3.md 4, 5, 7
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
       SC_DXLANE,                                         /* (Orange Cat 0.14.2) DRUM-X's lane pages: the kit's values of the lane the keys
                                                           * picked (params.c dxl_desc). After SC_MOD: a module's numbers stay */
       SC_FX };                                           /* (Release 3, version 3) a page of an effect module (kinds 1 to 3): after SC_DXLANE, no number moves */
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
#define FW_PAGES_VERSION 3u                              /* (Release 3) 3: ui, pid, ext appended; (0.12.1) 2: loaded, env_source, bend and cc added; version 1 was never in a released module */
#define FW_PAGES_MAX 24u                                 /* rows a module may bring: its indices after PAGES */
#define FW_PANEL_MAX 128u                                /* native parameters a module may describe (motion's MO_NATIVE_N) */
#define FW_PAGES_WIRE 80u                                /* sizeof(fw_pages_t) on the device (4 byte pointers); the loader reads this layout */
#define FW_PAGES_ROW_WIRE 12u                            /* sizeof(page_t) on the device */
#define FW_PAGES_DESC_WIRE 20u                           /* sizeof(param_desc_t) on the device */
#define FW_UI_WIRE 20u                                   /* sizeof(fw_ui_t) on the device */
#define FW_KDEF_WIRE 28u                                 /* sizeof(fw_kdef_t) on the device */
#define FW_PID_WIRE 4u                                   /* sizeof(fw_pid_t) on the device */
#define FW_UI_MAGIC 0x31495547u                          /* "GUI1" little endian */
#define FW_UI_SCENE_MAX 1024u                            /* bytes of scene a module may carry (the interpreter's own SC_MAX is the other bound) */
#define FW_KDEF_N 4u                                     /* HOME's knobs */
#define FW_PID_VOLATILE 0x4000u                          /* (L3) a pid with this bit is NOT STORED: a RAM-only value (the FX layer's knobs, "never saved"): the host keeps the byte in the instance, the project writer and reader skip it, no hook is needed; no legacy slot */
#define FW_PID_RELATIVE 0x2000u                          /* (L4) a RELATIVE knob (TAPE STOP's SCRATCH moves the tape by hand): it has no value; `put` receives the detents turned (signed) instead of a value; never stored; needs the put hook; no legacy slot */
#define FW_PID_FLAGS 0xE000u                             /* the three bits above and the view's: at most one of them on a pid; a stable id proper is below 0x2000 */
#define FW_LEG_GLOBAL 0x8000u                            /* (L1) bit 15 of fw_pid_t.legacy: the slot is song.g[legacy & 0x7FFF] (the effects' globals), not a track parameter */
#define FW_PID_VIEW 0x8000u                              /* a pid with this bit is a VIEW (a cell computed from stored ids by shown/put: DUB's SYNC, PING, DRIVE, WOW): never stored, never a motion or macro target; it has no legacy slot and needs both hooks */
#define FW_PID_NONE 0xFFFFu                              /* fw_pid_t.legacy: no track slot; fw_kdef_t target pid: unused */
/* The stable parameter id of a panel entry (MODULE_FORMAT_R3.md 4): id never reused, never changes meaning; legacy the track parameter slot an older project stored the value in, FW_PID_NONE for none */
typedef struct { uint16_t id, legacy; } fw_pid_t;
/* The picture and HOME's four knobs (MODULE_FORMAT_R3.md 5), reached from fw_pages_t.ui. 20 bytes: the document's list had a reserved u16 that made it 22 and
 * unaligned; flags (0, nonzero refused) is the reserve. scene: the bytes of oc_scene.h (scene_len of them), kdef: nk fw_kdef_t, or 0 */
typedef struct {
    uint32_t magic;                                      /* FW_UI_MAGIC */
    uint16_t size, version;                              /* FW_UI_WIRE, 1 */
    uint16_t scene_len;                                  /* 0 .. FW_UI_SCENE_MAX; 0 = no picture */
    uint8_t nk, flags;                                   /* knob declarations that follow: 0 or 4; flags 0 */
    const uint8_t *scene;
    const struct fw_kdef *kdef;
} fw_ui_t;
/* One knob (knob_def_t of oc_knobs.c with the label inline and stable ids). kind 0 none, 1 plain (pid), 2 macro (t[]: pid FW_PID_NONE = unused) */
typedef struct fw_kdef {
    char label[6];
    uint8_t kind, pad;
    uint16_t pid;
    struct { uint16_t pid; uint8_t shape; int8_t depth; } t[4];
    uint16_t reserved;
} fw_kdef_t;
typedef struct fw_pages {
    uint32_t magic;                                      /* FW_PAGES_MAGIC */
    uint16_t size, version;                              /* sizeof(fw_pages_t), FW_PAGES_VERSION */
    uint16_t npages, npanel;                             /* rows (<= FW_PAGES_MAX), panel entries (<= FW_PANEL_MAX) */
    const page_t *rows;
    const param_desc_t *panel;
    uint8_t *(*native)(struct track *t);
    union {                                              /* (Release 3) the hooks by kind: an engine's (kind 0, SC_MOD) take the track, an effect's (kinds 1 to 3, SC_FX) the instance. The same four bytes at the same offset */
        const param_desc_t *(*shown)(const struct track *t, uint32_t id, int16_t *value);
        const param_desc_t *(*fx_shown)(uint32_t slot, uint32_t instance, uint32_t id, int16_t *value);
    };
    union {
        void (*put)(struct track *t, uint32_t id, int32_t value);
        void (*fx_put)(uint32_t slot, uint32_t instance, uint32_t id, int32_t value);
    };
    int (*active)(const struct track *t, uint32_t id);
    const uint8_t *motion_skip;                          /* (npanel + 7) / 8 bytes, or 0 */
    const char *const *env_titles;
    const char *const *lfo_titles;
    uint8_t knob[4];
    void (*loaded)(struct track *t, uint32_t preset);    /* (0.12.1) a preset was loaded on the track: the engine takes its own patch from it (ui.c preset_apply) */
    int32_t (*env_source)(struct track *t, const voice_t *v);   /* (0.12.1) the modulation matrix's ENV source for an engine with envelopes of its own (voice.c) */
    int32_t (*bend)(const struct track *t);              /* (0.12.1) the MIDI wheel's range in 1/256 semitones when the patch sets it; 0 = the channel's own */
    uint8_t cc[4];                                       /* (0.12.1) panel indices that MIDI CC 72 (release), 73 (attack), 75 (decay) set on the amplitude envelope; 0xFF = none */
    const fw_ui_t *ui;                                   /* (Release 3, offset 68) the picture and the four knobs, or 0 (the old HOME for the engine) */
    const fw_pid_t *pid;                                 /* (72) npanel entries, or 0: the id is the panel index, no legacy slot (engines only: required for kinds 1 to 3) */
    uint32_t ext;                                        /* (76) 0 in version 3; a later use is version 4 (MODULE_FORMAT_R3.md 4) */
} fw_pages_t;
_Static_assert(sizeof(void *) != 4 || sizeof(fw_pages_t) == FW_PAGES_WIRE, "the descriptor's device layout");
_Static_assert(sizeof(void *) != 4 || sizeof(page_t) == FW_PAGES_ROW_WIRE, "the row's device layout");
_Static_assert(sizeof(void *) != 4 || sizeof(param_desc_t) == FW_PAGES_DESC_WIRE, "the panel entry's device layout");
_Static_assert(sizeof(void *) != 4 || sizeof(fw_ui_t) == FW_UI_WIRE, "the picture's device layout");
_Static_assert(sizeof(fw_kdef_t) == FW_KDEF_WIRE, "the knob's layout");
_Static_assert(sizeof(void *) != 4 || (__builtin_offsetof(fw_pages_t, fx_shown) == 24 && __builtin_offsetof(fw_pages_t, fx_put) == 28 && __builtin_offsetof(fw_pages_t, ui) == 68), "the hooks of an effect sit where an engine's do");
_Static_assert(sizeof(fw_pid_t) == FW_PID_WIRE, "the parameter id's layout");

/* ---- the loader's check of a descriptor, on the bytes of the module's image ------------------------------------------------------------------------------
 * `rd(ctx, addr, buf, n)` reads n bytes of the image at the XIP address addr (0 on success); [lo, hi) is the image (code from FW_MOD_CODE_OFF to the end of
 * the image). Returns 0 for a descriptor the UI may follow, else a nonzero code (the first thing found wrong). It reads the DEVICE layout (4 byte words), so
 * it is the same on the host: a test builds the wire bytes by hand. */
enum { FWP_OK = 0, FWP_ADDR = 1, FWP_READ, FWP_MAGIC, FWP_SIZE, FWP_COUNT, FWP_ROW, FWP_PANEL, FWP_STR, FWP_HOOK, FWP_SKIP, FWP_TITLES, FWP_KNOB,
       FWP_EXT, FWP_PID, FWP_SCOPE };            /* (Release 3) ext nonzero; the pid table; an effect page descriptor that uses what only an engine has */
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
/* scope: SC_MOD for an engine's descriptor (kind 0), SC_FX for an effect's (kinds 1 to 3: the rules of MODULE_FORMAT_R3.md 4); pcount, gcount: the build's P_COUNT and G_COUNT (the legacy
 * slots must lie below them: a track slot, or with FW_LEG_GLOBAL a song.g id of the effects' globals, G_DTIME up, not G_BUDGET); *ui: the picture's address (0 none) for fw_ui_check, which is separate because a bad picture does not refuse the module */
static inline int fw_pages_check_ex(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi, uint32_t scope, uint32_t pcount, uint32_t gcount, uint32_t *ui)
{
    uint8_t d[FW_PAGES_WIRE], r[FW_PAGES_ROW_WIRE], p[FW_PAGES_DESC_WIRE], w[4];
    uint32_t i, n, np, rows, panel, skip, env, lfo, h, pidp;
    uint32_t views = 0;                                  /* (Release 3) entries whose id has FW_PID_VIEW set */
    uint16_t ids[FW_PANEL_MAX];                          /* (Release 3) the pid table, for the unique-id check */
    uint8_t legseen[32], gseen[32];                      /* .. and the legacy slots seen, track and song.g (each below 256) */
    uint32_t rels = 0, ext;
    if (ui) *ui = 0;
    if (pcount > 256u || gcount > 256u || (scope != SC_MOD && scope != SC_FX)) return FWP_SCOPE;
    if (!fwp_in(desc, FW_PAGES_WIRE, lo, hi) || (desc & 3u)) return FWP_ADDR;
    if (rd(ctx, desc, d, FW_PAGES_WIRE)) return FWP_READ;
    if (fwp_u32(d) != FW_PAGES_MAGIC) return FWP_MAGIC;
    if ((d[4] | d[5] << 8) != FW_PAGES_WIRE || (d[6] | d[7] << 8) != FW_PAGES_VERSION) return FWP_SIZE;
    n = d[8] | d[9] << 8; np = d[10] | d[11] << 8;
    if (!n || n > FW_PAGES_MAX || !np || np > FW_PANEL_MAX) return FWP_COUNT;
    ext = fwp_u32(d + 76);                               /* ext: 0, or (L1) an effect's legacy condition: bits 0..15 song.g index + 1, bits 16..31 the value it must hold for the legacy slots to be imported (the reverb: TYPE == this mode) */
    if (ext && (scope != SC_FX || (ext & 0xFFFFu) == 0u || (ext & 0xFFFFu) - 1u < G_DTIME || (ext & 0xFFFFu) - 1u >= gcount || (ext & 0xFFFFu) - 1u == G_BUDGET)) return FWP_EXT;
    rows = fwp_u32(d + 12); panel = fwp_u32(d + 16); pidp = fwp_u32(d + 72);
    if (ui) *ui = fwp_u32(d + 68);
    if (!fwp_in(rows, n * FW_PAGES_ROW_WIRE, lo, hi) || (rows & 3u)) return FWP_ROW;
    if (!fwp_in(panel, np * FW_PAGES_DESC_WIRE, lo, hi) || (panel & 3u)) return FWP_PANEL;
    for (i = 0; i < 7u; i++) {                           /* the seven hooks that may be code: inside the image after the header page, or 0 */
        h = fwp_u32(d + (i < 4u ? 20u + 4u * i : 52u + 4u * (i - 4u)));
        if (h && !fwp_in(h, 2u, lo, hi)) return FWP_HOOK;
    }
    if (scope == SC_FX) {                                /* an effect: what only an engine has must be absent; its parameters have stable ids */
        static const uint8_t no_hook[] = {20, 32, 36, 40, 44, 52, 56, 60};   /* native active motion_skip env lfo loaded env_source bend: an effect has none. `shown` (24) and `put` (28) are allowed, with the instance signature (fx_shown, fx_put) */
        for (i = 0; i < sizeof no_hook; i++) if (fwp_u32(d + no_hook[i])) return FWP_SCOPE;
        for (i = 0; i < 4u; i++) if (d[48 + i] != 0xFFu || d[64 + i] != 0xFFu) return FWP_SCOPE;
        if (!pidp) return FWP_PID;
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
        if (r[4] >= FAM_COUNT || r[5] != scope || r[6] != GR_NONE) return FWP_ROW;
        for (k = 0; k < 4u; k++)
            if (r[7 + k] != 0xFFu && r[7 + k] >= np) return FWP_ROW;
        /* (L2) 0xFF in a row is "not the module's": for an engine an empty place; for an effect also the HOST's knob (the reverb's type chooser), which the host draws there. Either way an effect's page has at least one knob of its own */
        if (scope == SC_FX && r[7] == 0xFFu && r[8] == 0xFFu && r[9] == 0xFFu && r[10] == 0xFFu) return FWP_ROW;
    }
    for (i = 0; i < sizeof legseen; i++) legseen[i] = gseen[i] = 0;
    if (pidp && (!fwp_in(pidp, np * FW_PID_WIRE, lo, hi) || (pidp & 3u))) return FWP_PID;
    for (i = 0; i < np; i++) {
        uint32_t label, names, unit;
        int mn, mx;
        if (rd(ctx, panel + i * FW_PAGES_DESC_WIRE, p, FW_PAGES_DESC_WIRE)) return FWP_READ;
        label = fwp_u32(p); names = fwp_u32(p + 12); unit = fwp_u32(p + 16);
        mn = (int16_t)(p[6] | p[7] << 8); mx = (int16_t)(p[8] | p[9] << 8);
        if (pidp) {                                      /* (Release 3) the stable id and the legacy slot of entry i */
            uint8_t q[FW_PID_WIRE];
            uint32_t id, leg, j;
            if (rd(ctx, pidp + i * FW_PID_WIRE, q, FW_PID_WIRE)) return FWP_READ;
            id = q[0] | q[1] << 8; leg = q[2] | q[3] << 8;
            if (id == FW_PID_NONE) return FWP_PID;
            if (id & FW_PID_FLAGS) {                                     /* a view, a volatile or a relative pid: at most one of the bits, and no slot of its own */
                uint32_t f = id & FW_PID_FLAGS;
                if ((f & (f - 1u)) || leg != FW_PID_NONE) return FWP_PID;
                if (f == FW_PID_VIEW) views++;
                if (f == FW_PID_RELATIVE) rels++;
            }
            for (j = 0; j < i; j++) if (ids[j] == id) return FWP_PID;   /* never two entries of one id (a removed entry keeps its row) */
            ids[i] = (uint16_t)id;
            if (!label && leg != FW_PID_NONE) return FWP_PID;           /* a removed parameter has no slot */
            if (leg != FW_PID_NONE && (leg & FW_LEG_GLOBAL)) {           /* (L1) a song.g id of the effects' globals, each once */
                uint32_t g = leg & 0x7FFFu;
                if (scope != SC_FX || g < G_DTIME || g >= gcount || g == G_BUDGET || (gseen[g >> 3] >> (g & 7u) & 1u)) return FWP_PID;
                gseen[g >> 3] |= (uint8_t)(1u << (g & 7u));
            } else if (leg != FW_PID_NONE) {
                if (leg >= pcount || (leg >= P_RESERVED33 && leg <= P_RESERVED36) || (legseen[leg >> 3] >> (leg & 7u) & 1u)) return FWP_PID;
                legseen[leg >> 3] |= (uint8_t)(1u << (leg & 7u));
            }
        }
        if (!label) continue;                            /* no parameter i */
        if (!fwp_str(rd, ctx, label, lo, hi) || (unit && !fwp_str(rd, ctx, unit, lo, hi))) return FWP_STR;
        if (mn > mx) return FWP_PANEL;
        /* (0.12.2, found by tests/fuzz_fw_pages.c) what params.c's param_format reads, which the check above did not bound: the format is one it knows (up to F_DUBSPD: Release 3 raised the limit from F_TONE for DUB TAPE's TIME and SPEED, and F_PANLR and F_ST16 come with it); an F_ENUM has its names
         * (it indexes names[] by the value clamped to min..max, so min is not below 0); a name list on an F_INT is 0-terminated inside the image (param_format counts to the 0);
         * no other format has one */
        if (p[4] > F_DUBSPD || (p[4] == F_ENUM && (!names || mn < 0)) || (names && p[4] != F_ENUM && p[4] != F_INT)) return FWP_PANEL;
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
    if (rels && !fwp_u32(d + 28)) return FWP_PID;       /* a relative knob is delivered by put: no hook, no knob */
    if (views && !(fwp_u32(d + 24) && fwp_u32(d + 28))) return FWP_PID;   /* a view is produced and written back by hooks: no hooks, no views */
    return FWP_OK;
}
static inline int fw_pages_check(fw_pages_rd rd, void *ctx, uint32_t desc, uint32_t lo, uint32_t hi)   /* an engine's descriptor (kind 0) in this build */
{
    return fw_pages_check_ex(rd, ctx, desc, lo, hi, SC_MOD, P_COUNT, G_COUNT, 0);
}

/* ---- the picture and the knobs (MODULE_FORMAT_R3.md 5): NOT a refusal --------------------------------------------------------------------------------------
 * A module whose pages pass fw_pages_check_ex but whose fw_ui_t does not still loads, with the old HOME for its engine; the Engines table says why (FWU_ code).
 * `sok(bytes, n)`: the interpreter's own check (oc_scene.c scene_ok), called on a copy of at most FW_UI_SCENE_MAX bytes (the boot check is the authority; the card's
 * and the page's install-time result is advisory); 0 = no scene check (a build without pictures: the scene is then refused as FWU_SCENE when there is one).
 * Returns FWU_OK (0) with the result in *o: the scene's address and length (0, 0 for none), the knobs' address (0 for none) and nk. */
enum { FWU_OK = 0, FWU_ADDR = 1, FWU_READ, FWU_MAGIC, FWU_SIZE, FWU_RES, FWU_SCENE, FWU_KDEF };
typedef struct { uint32_t scene, kdef; uint16_t scene_len; uint8_t nk; } fw_ui_info_t;
typedef int (*fw_scene_ok_fn)(const uint8_t *s, uint32_t n);
static inline int fw_ui_check(fw_pages_rd rd, void *ctx, uint32_t ui, uint32_t lo, uint32_t hi, fw_scene_ok_fn sok, fw_ui_info_t *o)
{
    uint8_t u[FW_UI_WIRE], k[FW_KDEF_WIRE];
    uint32_t i, j, len, scene, kdef;
    o->scene = o->kdef = 0; o->scene_len = 0; o->nk = 0;
    if (!ui) return FWU_OK;                              /* none: the old HOME */
    if (!fwp_in(ui, FW_UI_WIRE, lo, hi) || (ui & 3u)) return FWU_ADDR;
    if (rd(ctx, ui, u, FW_UI_WIRE)) return FWU_READ;
    if (fwp_u32(u) != FW_UI_MAGIC) return FWU_MAGIC;
    if ((u[4] | u[5] << 8) != FW_UI_WIRE || (u[6] | u[7] << 8) != 1u) return FWU_SIZE;
    len = u[8] | u[9] << 8;
    if (u[11] || (u[10] != 0u && u[10] != FW_KDEF_N) || len > FW_UI_SCENE_MAX) return FWU_RES;   /* flags 0; nk 0 or 4; scene_len <= 1024 */
    scene = fwp_u32(u + 12); kdef = fwp_u32(u + 16);
    if (!len != !scene) return FWU_SCENE;                /* a length and an address, or neither */
    if (len) {
        uint8_t buf[FW_UI_SCENE_MAX];
        if (!sok || !fwp_in(scene, len, lo, hi)) return FWU_SCENE;
        if (rd(ctx, scene, buf, len)) return FWU_READ;
        if (!sok(buf, len)) return FWU_SCENE;
    }
    if (!u[10] != !kdef) return FWU_KDEF;
    if (u[10]) {
        if (!fwp_in(kdef, FW_KDEF_N * FW_KDEF_WIRE, lo, hi) || (kdef & 3u)) return FWU_KDEF;
        for (i = 0; i < FW_KDEF_N; i++) {
            uint32_t kind, nl = 0;
            if (rd(ctx, kdef + i * FW_KDEF_WIRE, k, FW_KDEF_WIRE)) return FWU_READ;
            kind = k[6];
            if (kind > 2u || k[7] || k[26] || k[27]) return FWU_KDEF;
            for (j = 0; j < 6u && k[j]; j++) {           /* upper case ASCII or digits, at most 5, NUL padded to the end */
                if (!((k[j] >= 'A' && k[j] <= 'Z') || (k[j] >= '0' && k[j] <= '9'))) return FWU_KDEF;
                nl++;
            }
            for (; j < 6u; j++) if (k[j]) return FWU_KDEF;
            if (nl > 5u) return FWU_KDEF;
            if (kind && !nl) return FWU_KDEF;            /* a knob that turns has a name */
            if (kind == 1u && (k[8] | k[9] << 8) == FW_PID_NONE) return FWU_KDEF;
            if (kind == 2u)
                for (j = 0; j < 4u; j++) {
                    const uint8_t *t = k + 10 + 4u * j;
                    if (t[2] > 2u || (int8_t)t[3] == -128) return FWU_KDEF;   /* shape KS_LIN .. KS_DOWN; depth -127 .. 127 */
                }
        }
    }
    o->scene = scene; o->scene_len = (uint16_t)len; o->kdef = kdef; o->nk = u[10];
    return FWU_OK;
}
