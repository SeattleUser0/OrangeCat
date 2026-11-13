/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Orange Cat Release 3 step 8b: the FX layer's buffer effects GLITCH, TAPE STOP, SHUFFLE and FREEZE as ONE effect module (kind 3, master; a bank of four; docs/orangecat/FX_MODULES_PLAN.md 17).
 * The kernels are the image's own text, firmware/src/perf_kern.h, included as it is; what is the module's own is the descriptor around them:
 *   - fx_desc_wire_t / fx_entry_wire_t (fw_fx.h): module id 3 (docs/orangecat/MODULE_IDS.md), version 1.0.0, four effects whose effect id is 1 + the layer's place (jfx_perform.c GX_*:
 *     1 GLITCH, 2 TAPE STOP, 3 SHUFFLE, 4 FREEZE), kind 3;
 *   - one fx_caps_t (fx_kernel.h, 56 bytes) an effect: the capability words, the call table (fx_master.h fx3_kcalls_t: two calls, `start` and `run`), the pages, and the abi (FX3_ABI_ID: the
 *     master host's);
 *   - one fw_pages_t (version 3, scope SC_FX) an effect: ONE page, the effect's card as the quick layer shows it while its key is held (MODULE_FORMAT_R3.md 16.3, 16.5): the card's knobs with
 *     volatile ids 0x4001 .. (L3: the host keeps the byte in RAM, perf_gp; never stored, no legacy slot, no hook); TAPE STOP's SCRATCH a relative id 0x2001 (L4) with the one `put` hook; a
 *     place with no knob 0xFF (L2). The surface stays the quick layer's cards: the player does not reach these pages through EDIT.
 * The module has no RAM (no .data, no .bss: the build checks) and imports only fw_api_t as it is (mem_copy and mem_set, for what the compiler may emit). The state, the 32 KB, the card's knobs,
 * the tempo, SCRATCH's count and the one host service (semi_q16) come in the call (fx3_ctx_t). The host (jfx_perform.c perf_block, fx3_modk_set) runs it in place of the resident copy of the
 * same text, which stays in the image this Nightly and plays when the module is not installed.
 *
 * SCRATCH's `put` hook does NOTHING, and says so: the detents are the host's count (perf_scr, which ui_layer.c's knob handler adds to and the host hands the kernel as fx3_ctx_t.scr); a hook
 * has no context, the module has no RAM, and fw_api_t (which must not grow) has no path to that word. The checker requires the hook for a relative id (no hook, no knob), and nothing in the
 * firmware calls an effect module's hooks yet (the cards are drawn by ui_layer.c from its own tables: FX_MODULES_PLAN.md 17.6). A consumer built later must keep adding the detents on the host's
 * side for id 0x2001, or the master host's convention gets a path for it (which moves FX3_ABI_ID only).
 *
 * Revisions:
 *   (Release 3 step 8b) first version.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u    /* the module's own version, 1.0.0 (the header's version word; perf_kern.h's PFK_VERSION is the same number) */
#include "build_options.h"
#include <stdint.h>
/* PITCH_OCT is the firmware's, through the table (fw_api.h pitch_oct), as the Prophet module's: melodee_tables.h's static and its pitch_inc are moved aside and pitch_inc is written again below */
#define PITCH_OCT PITCH_OCT_unused
#define pitch_inc pitch_inc_unused
#ifdef OC_MODULE_HOST       /* the host build (tests/module_host_fxlayer_tu.c): libc has memcpy and memset, so the module's own go by other names */
#define memcpy oc_fxl_memcpy
#define memset oc_fxl_memset
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
extern const fw_api_t *oc_host_fxl_fw;                  /* (its own name: a test may link the other modules' host units too) */
#define OC_FW() (oc_host_fxl_fw)
#define PTR32(x) 0u                                    /* a host pointer is not a device address: the host build hands the host its caps blocks and call tables directly (oc_host_rev_*) */
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#define PTR32(x) ((uint32_t)(uintptr_t)(x))
#endif
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }   /* dsp.c's helpers read the firmware's hot set through it (none of them is called here: the kernels use clamp and mulq16 only) */
#define PITCH_OCT (OC_FW()->pitch_oct)
static inline uint32_t pitch_inc(uint32_t p)         /* melodee_tables.h's, over the firmware's table (dsp.c names it; not called here) */
{
    uint32_t i = p + 64u, o = (i * 21846u) >> 22;
    return PITCH_OCT[i - o * 192u] >> (10u - o);
}
#define tuning_log() (OC_FW()->tuning_log())
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* the kernels: the image's own text (no placement attribute: the module's flash) */
#define PFK_FN
#include "perf_kern.h"
_Static_assert(OC_MODULE_VERSION == PFK_VERSION, "one version");

/* ---- the pages (MODULE_FORMAT_R3.md 16.3; tests/mod_ui_test.c t_step8_layer is the same data as the checker's test has it; ui_layer.c's FXK_LABEL, FXK_MIN, FXK_MAX and perf_gp's initial
 * values are what the cards show today: tests/fx_layer_module_test.c holds the three equal) ---- */
static const char *const FXL_NAMES_GSIZE[] = {"LONG", "MID", "SHORT", "TINY"}, *const FXL_NAMES_TTIME[] = {"1/64", "1/32", "1/16", "1/8", "1/4"};
static const char *const FXL_NAMES_SSIZE[] = {"1/4", "1/8", "1/16"}, *const FXL_NAMES_FSIZE[] = {"1/128", "1/64", "1/32", "1/16", "1/8"};
static const page_t FXL_ROW_GLT = {"GLITCH", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}}, FXL_ROW_TAPE = {"TAPE STOP", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 0xFF}},   /* (L2) TAPE's K4: no knob */
                    FXL_ROW_SHUF = {"SHUFFLE", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}}, FXL_ROW_FRZ = {"FREEZE", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}};
static const param_desc_t FXL_PANEL_GLT[4] = {{"DENS", F_INT, 0, 100, 50, 0, "%"}, {"PITCH", F_INT, -12, 12, 0, 0, "st"}, {"SIZE", F_ENUM, 0, 3, 0, FXL_NAMES_GSIZE, 0}, {"VARI", F_INT, 1, 16, 1, 0, 0}};
static const param_desc_t FXL_PANEL_TAPE[3] = {{"SCRATCH", F_INT, 0, 0, 0, 0, 0}, {"TIME", F_ENUM, 0, 4, 4, FXL_NAMES_TTIME, 0}, {"CURVE", F_INT, -100, 100, 0, 0, 0}};
static const param_desc_t FXL_PANEL_SHUF[4] = {{"AMOUNT", F_INT, 0, 100, 60, 0, "%"}, {"SIZE", F_ENUM, 0, 2, 2, FXL_NAMES_SSIZE, 0}, {"SPEED", F_INT, 0, 100, 20, 0, "%"}, {"VARI", F_INT, 1, 16, 1, 0, 0}};
static const param_desc_t FXL_PANEL_FRZ[4] = {{"DENS", F_INT, 0, 100, 50, 0, "%"}, {"PITCH", F_INT, -12, 12, 0, 0, "st"}, {"SPRAY", F_INT, 0, 100, 30, 0, "%"}, {"SIZE", F_ENUM, 0, 4, 2, FXL_NAMES_FSIZE, 0}};
/* the ids: volatile (L3: 0x4001 .., the knob's place + 1), SCRATCH relative (L4: 0x2001); no legacy slot: a project holds nothing of them */
static const fw_pid_t FXL_PID[4] = {{FW_PID_VOLATILE + 1u, FW_PID_NONE}, {FW_PID_VOLATILE + 2u, FW_PID_NONE}, {FW_PID_VOLATILE + 3u, FW_PID_NONE}, {FW_PID_VOLATILE + 4u, FW_PID_NONE}};
static const fw_pid_t FXL_PID_TAPE[3] = {{FW_PID_RELATIVE + 1u, FW_PID_NONE}, {FW_PID_VOLATILE + 2u, FW_PID_NONE}, {FW_PID_VOLATILE + 3u, FW_PID_NONE}};
/* SCRATCH's put: the signed detents of the turn. Nothing to do here (the comment at the top of this file): the count is the host's */
static void fxl_put(uint32_t slot, uint32_t instance, uint32_t id, int32_t value)
{
    (void)slot; (void)instance; (void)id; (void)value;
}
#define FXL_PAGES(row_, panel_, n_, pid_, put_) { .magic = FW_PAGES_MAGIC, .size = FW_PAGES_WIRE, .version = FW_PAGES_VERSION, .npages = 1, .npanel = n_, .rows = &row_, .panel = panel_, .pid = pid_, \
                                                  .fx_put = put_, .knob = {0xFF, 0xFF, 0xFF, 0xFF}, .cc = {0xFF, 0xFF, 0xFF, 0xFF}, .ext = 0 }
static const fw_pages_t FXL_PAGES_GLT = FXL_PAGES(FXL_ROW_GLT, FXL_PANEL_GLT, 4, FXL_PID, 0), FXL_PAGES_TAPE = FXL_PAGES(FXL_ROW_TAPE, FXL_PANEL_TAPE, 3, FXL_PID_TAPE, fxl_put),
                        FXL_PAGES_SHUF = FXL_PAGES(FXL_ROW_SHUF, FXL_PANEL_SHUF, 4, FXL_PID, 0), FXL_PAGES_FRZ = FXL_PAGES(FXL_ROW_FRZ, FXL_PANEL_FRZ, 4, FXL_PID, 0);

/* ---- the call tables, the caps blocks, the bank (four effects), the header ---- */
static const fx3_kcalls_t FXL_CALLS_GLT = { PFK_CALLS_GLT }, FXL_CALLS_TAPE = { PFK_CALLS_TAPE }, FXL_CALLS_SHUF = { PFK_CALLS_SHUF }, FXL_CALLS_FRZ = { PFK_CALLS_FRZ };
#define FXL_CAPS(calls_, pages_) { .magic = FX_CAPS_MAGIC, .size = FX_CAPS_WIRE, .version = FX_CAPS_VERSION, .kernel = PTR32(&calls_), .pages = PTR32(&pages_), .hot = 0, \
    .legacy_module = PFK_LEGACY, .rows = 0, .flags = PFK_FLAGS, .rates = PFK_RATES, .max_inst = 1, .prof = AP_FX, .ram_work_min = PFK_WORK, .ram_work_max = PFK_WORK, .ram_extra = 0, .nhot = 0, \
    .nrows = 0, .param_bytes = PFK_PARAM_BYTES, .ram_state = 0, .latency = 0, .tail_base = 0, .cost = 0, .ram_extra_bytes = 0, .gov_us = 0, .abi = FX3_ABI_ID }
static const fx_caps_t FXL_CAPS_GLT = FXL_CAPS(FXL_CALLS_GLT, FXL_PAGES_GLT), FXL_CAPS_TAPE = FXL_CAPS(FXL_CALLS_TAPE, FXL_PAGES_TAPE),
                       FXL_CAPS_SHUF = FXL_CAPS(FXL_CALLS_SHUF, FXL_PAGES_SHUF), FXL_CAPS_FRZ = FXL_CAPS(FXL_CALLS_FRZ, FXL_PAGES_FRZ);
static const fx_entry_wire_t FXL_ENTRY[4] = {
    { .effect_id = 1u + GX_GLT, .kind = FW_MOD_KIND_MASTER, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&FXL_CAPS_GLT) },
    { .effect_id = 1u + GX_TAPE, .kind = FW_MOD_KIND_MASTER, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&FXL_CAPS_TAPE) },
    { .effect_id = 1u + GX_SHUF, .kind = FW_MOD_KIND_MASTER, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&FXL_CAPS_SHUF) },
    { .effect_id = 1u + GX_FRZ, .kind = FW_MOD_KIND_MASTER, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&FXL_CAPS_FRZ) },
};
static const fx_desc_wire_t FXL_DESC = { .n = 4, .module_id = PFK_MODULE_ID, .version = PFK_VERSION, .entries = PTR32(FXL_ENTRY), .scene = 0 };

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_FXLAYER_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_MASTER << 8, 0, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
const fw_api_t *oc_host_fxl_fw;
/* what a host test needs of the module that the 4 byte words cannot carry: entry k (0 GLITCH, 1 TAPE STOP, 2 SHUFFLE, 3 FREEZE) */
const fx_caps_t *oc_host_fxl_caps(uint32_t k) { return k == 0u ? &FXL_CAPS_GLT : k == 1u ? &FXL_CAPS_TAPE : k == 2u ? &FXL_CAPS_SHUF : &FXL_CAPS_FRZ; }
const fx3_kcalls_t *oc_host_fxl_calls(uint32_t k) { return k == 0u ? &FXL_CALLS_GLT : k == 1u ? &FXL_CALLS_TAPE : k == 2u ? &FXL_CALLS_SHUF : &FXL_CALLS_FRZ; }
const fw_pages_t *oc_host_fxl_pages(uint32_t k) { return k == 0u ? &FXL_PAGES_GLT : k == 1u ? &FXL_PAGES_TAPE : k == 2u ? &FXL_PAGES_SHUF : &FXL_PAGES_FRZ; }
const fx_desc_wire_t *oc_host_fxl_desc(void) { return &FXL_DESC; }
const fx_entry_wire_t *oc_host_fxl_entry(uint32_t k) { return &FXL_ENTRY[k]; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_MASTER << 8, 0, (uint32_t)(uintptr_t)&FXL_DESC, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
#endif
