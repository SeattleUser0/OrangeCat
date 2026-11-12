/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Orange Cat Release 3 step 8a: the reverb modes SPRING, SHIMMER and RESO as ONE effect module (kind 2, send; a bank of three; docs/orangecat/FX_MODULES_PLAN.md 16). The kernels are the
 * image's own text, firmware/src/rev_kern.h, included as it is; what is the module's own is the descriptor around them:
 *   - fx_desc_wire_t / fx_entry_wire_t (fw_fx.h): module id 2 (docs/orangecat/MODULE_IDS.md), version 1.0.0, three effects whose effect id is the reverb's mode (G_RTYPE: 1 SPRING, 2 SHIMMER,
 *     3 RESO), kind 2;
 *   - one fx_caps_t (fx_kernel.h, 56 bytes) an effect: the capability words, the call table (fx_send.h fx2_kcalls_t: one call, `process`), the pages, and the abi (FX2_ABI_ID: the send host's);
 *   - one fw_pages_t (version 3, scope SC_FX) an effect: REVERB and REVERB 2 as the firmware shows them for that TYPE (MODULE_FORMAT_R3.md 16.2): K1 of REVERB is 0xFF, the host's TYPE chooser
 *     (L2); the seven knobs with stable ids 1 .. 7 on song.g's legacy slots (FW_LEG_GLOBAL | G_RSIZE ..), imported only while G_RTYPE is the effect's mode (`ext`, L1). No hooks, no views.
 * The module has no RAM (no .data, no .bss: the build checks) and imports only fw_api_t as it is: base_hot (the sine and tanh tables dsp.c's helpers read), pitch_oct (pitch_inc), mem_copy and
 * mem_set. The state, the tank, the model line, the parameter snapshot, RESO's inputs, the resident ROOM network (SHIMMER runs it) and the LFO rate table come in the call (fx2_ctx_t). The host
 * (jfx.c rev_run, fx2_modk_set) runs it in place of the resident copy of the same text, which stays in the image this Nightly and plays when the module is not installed.
 *
 * Revisions:
 *   (Release 3 step 8a) first version.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u    /* the module's own version, 1.0.0 (the header's version word; rev_kern.h's REVK_VERSION is the same number) */
#include "build_options.h"
#include <stdint.h>
/* PITCH_OCT is the firmware's, through the table (fw_api.h pitch_oct), as the Prophet module's: melodee_tables.h's static and its pitch_inc are moved aside and pitch_inc is written again below */
#define PITCH_OCT PITCH_OCT_unused
#define pitch_inc pitch_inc_unused
#ifdef OC_MODULE_HOST       /* the host build (tests/module_host_reverb_tu.c): libc has memcpy and memset, so the module's own go by other names */
#define memcpy oc_rev_memcpy
#define memset oc_rev_memset
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#undef PITCH_OCT
#undef pitch_inc
static inline uint32_t pitch_inc(uint32_t p);       /* (defined below, over the firmware's table) */
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#include "fw_pages.h"
#include "fw_fx.h"
#include "audio_profile.h"
#include "oc_pack.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_rev_fw;                  /* (its own name: a test may link DUB TAPE's host unit too) */
#define OC_FW() (oc_host_rev_fw)
#define PTR32(x) 0u                                    /* a host pointer is not a device address: the host build hands the host its caps blocks and call tables directly (oc_host_rev_*) */
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#define PTR32(x) ((uint32_t)(uintptr_t)(x))
#endif
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }   /* dsp.c: SINE (osc_sine: SPRING's wobble) and TANH_Q15 (softclip: the lines' knee), the firmware's hot set */
#define PITCH_OCT (OC_FW()->pitch_oct)
static inline uint32_t pitch_inc(uint32_t p)         /* melodee_tables.h's, over the firmware's table (RESO's strings) */
{
    uint32_t i = p + 64u, o = (i * 21846u) >> 22;
    return PITCH_OCT[i - o * 192u] >> (10u - o);
}
#define tuning_log() (OC_FW()->tuning_log())
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* the kernels: the image's own text (no placement attribute: the module's flash) */
#define REVK_FN
#include "rev_kern.h"
_Static_assert(OC_MODULE_VERSION == REVK_VERSION, "one version");

/* ---- the pages (MODULE_FORMAT_R3.md 16.2; tests/fx_reverb_data.h is the same data as the checker's test has it) ---- */
static const char *const REV_NAMES_FOLW[] = {"OFF", "T1", "T2", "T3", "T4"};
static const page_t REV_ROWS[2] = {{"REVERB", FAM_FX, SC_FX, GR_NONE, {0xFF, 0, 1, 2}}, {"REVERB 2", FAM_FX, SC_FX, GR_NONE, {3, 4, 5, 6}}};   /* K1 of REVERB: the host's TYPE (L2) */
#define REV_P_SIZE {"SIZE", F_PCT, 0, 127, 90, 0, 0}
#define REV_P_TAIL {"MOD", F_PCT, 0, 127, 0, 0, 0}, {"RATE", F_LFOHZ, 0, 127, 30, 0, 0}, {"FILT", F_BIPCT, -64, 63, 0, 0, 0}, {"WIDE", F_PCT, 0, 127, 0, 0, 0}
static const param_desc_t REV_PANEL_SPRING[7] = {REV_P_SIZE, {"DAMP", F_PCT, 0, 127, 60, 0, 0}, {"PRE", F_INT, 0, 100, 0, 0, "ms"}, REV_P_TAIL};
static const param_desc_t REV_PANEL_SHIMMER[7] = {REV_P_SIZE, {"DAMP", F_PCT, 0, 127, 60, 0, 0}, {"AIR", F_SEMI, -24, 24, 0, 0, 0}, REV_P_TAIL};
static const param_desc_t REV_PANEL_RESO[7] = {REV_P_SIZE, {"FOLW", F_ENUM, 0, 4, 0, REV_NAMES_FOLW, 0}, {"PITCH", F_INT, 0, 14, 0, 0, 0}, REV_P_TAIL};
/* the stable ids 1 .. 7 (SIZE, DAMP | FOLW, PRE | AIR | PITCH, MOD, RATE, FILT, WIDE) on the Release 2 slots: song.g (L1) */
static const fw_pid_t REV_PID[7] = {{1, FW_LEG_GLOBAL | G_RSIZE}, {2, FW_LEG_GLOBAL | G_RDAMP}, {3, FW_LEG_GLOBAL | G_RPRE}, {4, FW_LEG_GLOBAL | G_RMOD},
                                    {5, FW_LEG_GLOBAL | G_RRATE}, {6, FW_LEG_GLOBAL | G_RFILT}, {7, FW_LEG_GLOBAL | G_RWIDE}};
#define REV_PAGES(panel_, mode_) { .magic = FW_PAGES_MAGIC, .size = FW_PAGES_WIRE, .version = FW_PAGES_VERSION, .npages = 2, .npanel = 7, .rows = REV_ROWS, .panel = panel_, .pid = REV_PID, \
                                   .knob = {0xFF, 0xFF, 0xFF, 0xFF}, .cc = {0xFF, 0xFF, 0xFF, 0xFF}, .ext = (G_RTYPE + 1u) | ((uint32_t)(mode_) << 16) }   /* ext (L1): imported only while TYPE is this mode */
static const fw_pages_t REV_PAGES_SPRING = REV_PAGES(REV_PANEL_SPRING, 1), REV_PAGES_SHIMMER = REV_PAGES(REV_PANEL_SHIMMER, 2), REV_PAGES_RESO = REV_PAGES(REV_PANEL_RESO, 3);

/* ---- the call tables, the caps blocks, the bank (three effects), the header ---- */
static const fx2_kcalls_t REV_CALLS_SPRING = { REVK_CALLS_SPRING }, REV_CALLS_SHIMMER = { REVK_CALLS_SHIMMER }, REV_CALLS_RESO = { REVK_CALLS_RESO };
#define REV_CAPS(calls_, pages_, line_) { .magic = FX_CAPS_MAGIC, .size = FX_CAPS_WIRE, .version = FX_CAPS_VERSION, .kernel = PTR32(&calls_), .pages = PTR32(&pages_), .hot = 0, \
    .legacy_module = REVK_LEGACY, .rows = 0, .flags = REVK_FLAGS, .rates = REVK_RATES, .max_inst = 1, .prof = AP_FX, .ram_work_min = line_, .ram_work_max = line_, .ram_extra = 0, .nhot = 0, \
    .nrows = 0, .param_bytes = 2u * G_COUNT, .ram_state = 0, .latency = 0, .tail_base = 0, .cost = 0, .ram_extra_bytes = 0, .gov_us = 0, .abi = FX2_ABI_ID }
static const fx_caps_t REV_CAPS_SPRING = REV_CAPS(REV_CALLS_SPRING, REV_PAGES_SPRING, REVK_LINE_SPRING), REV_CAPS_SHIMMER = REV_CAPS(REV_CALLS_SHIMMER, REV_PAGES_SHIMMER, REVK_LINE_SHIMMER),
                       REV_CAPS_RESO = REV_CAPS(REV_CALLS_RESO, REV_PAGES_RESO, REVK_LINE_RESO);
static const fx_entry_wire_t REV_ENTRY[3] = {
    { .effect_id = 1, .kind = FW_MOD_KIND_SEND, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&REV_CAPS_SPRING) },
    { .effect_id = 2, .kind = FW_MOD_KIND_SEND, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&REV_CAPS_SHIMMER) },
    { .effect_id = 3, .kind = FW_MOD_KIND_SEND, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&REV_CAPS_RESO) },
};
static const fx_desc_wire_t REV_DESC = { .n = 3, .module_id = REVK_MODULE_ID, .version = REVK_VERSION, .entries = PTR32(REV_ENTRY), .scene = 0 };

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_REVERB_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_SEND << 8, 0, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
const fw_api_t *oc_host_rev_fw;
/* what a host test needs of the module that the 4 byte words cannot carry: entry k (0 SPRING, 1 SHIMMER, 2 RESO) */
const fx_caps_t *oc_host_rev_caps(uint32_t k) { return k == 0u ? &REV_CAPS_SPRING : k == 1u ? &REV_CAPS_SHIMMER : &REV_CAPS_RESO; }
const fx2_kcalls_t *oc_host_rev_calls(uint32_t k) { return k == 0u ? &REV_CALLS_SPRING : k == 1u ? &REV_CALLS_SHIMMER : &REV_CALLS_RESO; }
const fw_pages_t *oc_host_rev_pages(uint32_t k) { return k == 0u ? &REV_PAGES_SPRING : k == 1u ? &REV_PAGES_SHIMMER : &REV_PAGES_RESO; }
const fx_desc_wire_t *oc_host_rev_desc(void) { return &REV_DESC; }
const fx_entry_wire_t *oc_host_rev_entry(uint32_t k) { return &REV_ENTRY[k]; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_SEND << 8, 0, (uint32_t)(uintptr_t)&REV_DESC, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
#endif
