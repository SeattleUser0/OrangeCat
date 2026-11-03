/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Orange Cat Release 3 step 6: DUB TAPE as an effect module (kind 1, insert; docs/orangecat/FX_MODULES_PLAN.md 15). The kernel is the one in the image: dubdelay.c and
 * dub_kern.h (the legacy-slot table, the parameter snapshot, the sizing, the signatures, the KICK edge), included as they are; what is the module's own is the descriptor around them:
 *   - fx_desc_wire_t / fx_entry_wire_t (fw_fx.h): module id 1, version 1.0.0, one effect (id 1, kind 1);
 *   - fx_caps_t (fx_kernel.h, 56 bytes): the capability words, the call table, the legacy-slot rows and the pages, and the abi (FX_ABI_ID), reached from the entry's `pages` word;
 *   - fw_pages_t (version 3, scope SC_FX): DUB SETUP, DUB and DUB 2 as the firmware shows them today (MODULE_FORMAT_R3.md 15.2), the stable ids, the legacy slots, and the two
 *     hooks (shown, put) that compute SYNC, PING, DRIVE and WOW from MODE and TAPE.
 * The module has no RAM (no .data, no .bss: the build checks) and imports only fw_api_t as it is: mem_copy and mem_set for memcpy and memset, and trk for the page hooks. The tempo and the
 * performance triggers come in the call (fx_ictx_t), the state and the line are the host's arena. The host (fx_insert.c) runs it beside the resident copy: fx_modk_set.
 *
 * Revisions:
 *   (Release 3 step 6) first version.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u    /* the module's own version, 1.0.0 (the header's version word; dub_kern.h's DUBK_VERSION is the same number) */
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST       /* the host build (tests/module_host_dub_tu.c): libc has memcpy and memset, so the module's own go by other names (<string.h> first: dubdelay.c includes it, and a fortified libc would define the renamed names inline) */
#include <string.h>
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#define OC_SZ __SIZE_TYPE__
#else
#define OC_SZ unsigned                                 /* the device's <string.h> (firmware/src/fatfs/sys/string.h, which dubdelay.c includes) declares the count as unsigned */
#endif
void *memcpy(void *d, const void *s, OC_SZ n);
void *memset(void *d, int c, OC_SZ n);
#include "melodee_tables.h"
#include "core.h"
#include "fw_api.h"
#include "fw_pages.h"
#include "fw_fx.h"
#include "audio_profile.h"
#include "oc_pack.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_fw;
#define OC_FW() (oc_host_fw)
#define PTR32(x) 0u                                    /* a host pointer is not a device address: the host build hands the host the call table and the rows directly (oc_host_fx_*) */
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#define PTR32(x) ((uint32_t)(uintptr_t)(x))
#endif
#define trk (OC_FW()->trk)
void *memcpy(void *d, const void *s, OC_SZ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, OC_SZ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* the kernel: the image's own text (no placement attributes: the module's flash) */
#define JFX_XIP
#define DUB_HOT
#define DD_NOINL
#define DD_HOT
#include "dubdelay.c"
_Static_assert(DD_BLOCK == CTL, "a job a block");
_Static_assert(DD_BLK_N == 2048u && DD_MAXBLK <= FX_INS_MAXSEG, "the kernel's line block is the arena's (JFX_SEG)");
static inline __attribute__((always_inline)) int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }   /* dsp.c's, word for word (the image's kernel calls the same one); dsp.c itself is not included: it carries the engines' tables */
#include "dub_kern.h"
_Static_assert(OC_MODULE_VERSION == DUBK_VERSION, "one version");

/* ---- the pages (MODULE_FORMAT_R3.md 15.2): the descriptor around the module's own code ---- */
static const param_desc_t DUBD_SYNC = {"SYNC", F_ENUM, 0, 1, 1, 0, 0}, DUBD_PING = {"PING", F_ENUM, 0, 1, 0, 0, 0}, DUBD_DRIVE = {"DRIVE", F_INT, 0, 15, 6, 0, 0},
                         DUBD_WOW = {"WOW", F_INT, 0, 11, 0, 0, 0}, DUBD_TS = {"TIM L", F_DUBTS, 0, 127, 53, 0, 0}, DUBD_TF = {"TIM L", F_DUBTF, 0, 127, 53, 0, 0},
                         DUBD_RS = {"TIM R", F_DUBTS, 0, 127, 67, 0, 0}, DUBD_RF = {"TIM R", F_DUBTF, 0, 127, 67, 0, 0};
/* shown: the cell a view or the TIME pair shows now (0: the panel entry and the stored value). instance = the track */
static const param_desc_t *dub_shown(uint32_t slot, uint32_t instance, uint32_t id, int16_t *value)
{
    const int16_t *p;
    (void)slot;
    if (instance >= NTRK) return 0;
    p = trk[instance].p;
    switch (id) {
    case 0x8001: *value = (int16_t)dub_get_field(p, DUBF_SYNC); return &DUBD_SYNC;
    case 0x8002: *value = (int16_t)dub_get_field(p, DUBF_PING); return &DUBD_PING;
    case 0x8003: *value = (int16_t)dub_get_field(p, DUBF_DRIVE); return &DUBD_DRIVE;
    case 0x8004: *value = (int16_t)dub_get_field(p, DUBF_WOW); return &DUBD_WOW;
    case 5: *value = p[P_DUB_TIML]; return dub_get_sync(p) ? &DUBD_TS : &DUBD_TF;
    case 6: *value = p[P_DUB_TIMR]; return dub_get_sync(p) ? &DUBD_RS : &DUBD_RF;
    default: return 0;
    }
}
/* put: a view written back into MODE or TAPE with one 16 bit store (the audio pass reads either whole) */
static void dub_put(uint32_t slot, uint32_t instance, uint32_t id, int32_t value)
{
    (void)slot;
    if (instance >= NTRK) return;
    if (id >= 0x8001 && id <= 0x8004) dub_put_field(trk[instance].p, id == 0x8001 ? DUBF_SYNC : id == 0x8002 ? DUBF_PING : id == 0x8003 ? DUBF_DRIVE : DUBF_WOW, value);
}
static const char *const DUB_NAMES_SYNC[] = {"FREE", "SYNC"}, *const DUB_NAMES_ONOFF[] = {"OFF", "ON"};
static const page_t DUB_ROWS[3] = {
    {"DUB SETUP", FAM_FX, SC_FX, GR_NONE, {10, 4, 5, 11}}, {"DUB", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}}, {"DUB 2", FAM_FX, SC_FX, GR_NONE, {6, 12, 13, 7}}};
static const param_desc_t DUB_PANEL[14] = {
    {"FDBK", F_INT, 0, 127, 55, 0, 0}, {"SEND", F_INT, 0, 127, 127, 0, 0}, {"FILTER", F_INT, 0, 127, 60, 0, 0}, {"ECHO", F_INT, 0, 127, 0, 0, 0},
    {"TIM L", F_DUBTS, 0, 127, 53, 0, 0}, {"TIM R", F_DUBTS, 0, 127, 67, 0, 0}, {"SPRNG", F_PCT, 0, 127, 0, 0, 0}, {"SPEED", F_DUBSPD, 0, 127, DUB_SPD_MID, 0, 0},
    {"", F_INT, DUB_MODE_MARK, 127, DUB_MODE_MARK | 1, 0, 0}, {"", F_INT, -64, 127, 6 * DUB_WOW_N - 64, 0, 0},                 /* MODE and TAPE: stored, packed, on no page */
    {"SYNC", F_ENUM, 0, 1, 1, DUB_NAMES_SYNC, 0}, {"PING", F_ENUM, 0, 1, 0, DUB_NAMES_ONOFF, 0}, {"DRIVE", F_INT, 0, 15, 6, 0, 0}, {"WOW", F_INT, 0, 11, 0, 0, 0}};
static const fw_pid_t DUB_PID[14] = {{1, P_DUB_FDBK}, {2, P_DUB_SEND}, {3, P_DUB_FILT}, {4, P_DUB_ECHO}, {5, P_DUB_TIML}, {6, P_DUB_TIMR}, {7, P_DUB_SPRG}, {8, P_DUB_SPD},
                                     {9, P_DUB_MODE}, {10, P_DUB_TAPE}, {0x8001, FW_PID_NONE}, {0x8002, FW_PID_NONE}, {0x8003, FW_PID_NONE}, {0x8004, FW_PID_NONE}};
static const fw_pages_t DUB_PAGES = {
    .magic = FW_PAGES_MAGIC, .size = FW_PAGES_WIRE, .version = FW_PAGES_VERSION, .npages = 3, .npanel = 14,
    .rows = DUB_ROWS, .panel = DUB_PANEL, .pid = DUB_PID, .fx_shown = dub_shown, .fx_put = dub_put,
    .knob = {0xFF, 0xFF, 0xFF, 0xFF}, .cc = {0xFF, 0xFF, 0xFF, 0xFF},
};

/* ---- the call table, the caps block, the bank (one effect), the header ---- */
static const fx_kcalls_t DUB_CALLS = { DUBK_CALLS };
static const fx_caps_t DUB_CAPS = {
    .magic = FX_CAPS_MAGIC, .size = FX_CAPS_WIRE, .version = FX_CAPS_VERSION, .kernel = PTR32(&DUB_CALLS), .pages = PTR32(&DUB_PAGES), .hot = 0, .legacy_module = DUBK_LEGACY,
    .rows = PTR32(dub_rows), .flags = DUBK_FLAGS, .rates = DUBK_RATES, .max_inst = FX_INS_N, .prof = AP_DUB, .ram_work_min = DUB_FLOOR, .ram_work_max = DUB_MAXB,
    .ram_extra = DUBK_RAM_EXTRA, .nhot = 0, .nrows = DUBP_N, .param_bytes = sizeof(dd_params_t), .ram_state = DUB_STATE_BYTES, .latency = DUBK_LATENCY, .tail_base = DUB_HOLD, .cost = DUBK_COST_US,
    .ram_extra_bytes = DUB_SPRING_BYTES, .gov_us = DUBK_GOV_US, .abi = FX_ABI_ID,
};
static const fx_entry_wire_t DUB_ENTRY = { .effect_id = 1, .kind = FW_MOD_KIND_INSERT, .flags = 0, .nparams = 0, .rsv = 0, .params = 0, .pages = PTR32(&DUB_CAPS) };
static const fx_desc_wire_t DUB_DESC = { .n = 1, .module_id = DUBK_MODULE_ID, .version = DUBK_VERSION, .entries = PTR32(&DUB_ENTRY), .scene = 0 };

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_INSERT << 8, 0, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
const fw_api_t *oc_host_fw;
/* what a host test needs of the module that the 4 byte words cannot carry */
const fx_caps_t *oc_host_fx_caps(void) { return &DUB_CAPS; }
const fx_kcalls_t *oc_host_fx_calls(void) { return &DUB_CALLS; }
const fx_prow_t *oc_host_fx_rows(void) { return dub_rows; }
const fw_pages_t *oc_host_fx_pages(void) { return &DUB_PAGES; }
const fx_desc_wire_t *oc_host_fx_desc(void) { return &DUB_DESC; }
const fx_entry_wire_t *oc_host_fx_entry(void) { return &DUB_ENTRY; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    FW_MOD_KIND_INSERT << 8, 0, (uint32_t)(uintptr_t)&DUB_DESC, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,   /* cost (build.py fills it), version, length, crc, hcrc */
};
#endif
