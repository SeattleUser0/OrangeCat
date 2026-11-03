/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3 (MODULE_FORMAT_R3.md 15): DUB TAPE's module descriptor as data (3 pages, 14 panel entries, the four views), shared by tests/mod_ui_test.c (the checker) and
 * tests/fx_legacy_test.c (the Release 2 fixture with DUB values). dubp is the instance's stored ids, indexed by their legacy slots (P_ ids).
 * Needs fw_pages.h, dub_fields.h and the tests' param tables, as fx_reverb_data.h does. */
#pragma once
static int16_t dubp[256];                                 /* an instance's stored ids, indexed by their legacy slots (P_ ids) */
static const param_desc_t DUBD_SYNC = {"SYNC", F_ENUM, 0, 1, 1, 0, 0}, DUBD_PING = {"PING", F_ENUM, 0, 1, 0, 0, 0}, DUBD_DRIVE = {"DRIVE", F_INT, 0, 15, 6, 0, 0},
                         DUBD_WOW = {"WOW", F_INT, 0, 11, 0, 0, 0}, DUBD_TS = {"TIM L", F_DUBTS, 0, 127, 53, 0, 0}, DUBD_TF = {"TIM L", F_DUBTF, 0, 127, 53, 0, 0},
                         DUBD_RS = {"TIM R", F_DUBTS, 0, 127, 67, 0, 0}, DUBD_RF = {"TIM R", F_DUBTF, 0, 127, 67, 0, 0};
static const param_desc_t *dub_shown(uint32_t slot, uint32_t instance, uint32_t id, int16_t *value)
{
    (void)slot; (void)instance;
    switch (id) {
    case 0x8001: *value = (int16_t)dub_get_field(dubp, DUBF_SYNC); return &DUBD_SYNC;
    case 0x8002: *value = (int16_t)dub_get_field(dubp, DUBF_PING); return &DUBD_PING;
    case 0x8003: *value = (int16_t)dub_get_field(dubp, DUBF_DRIVE); return &DUBD_DRIVE;
    case 0x8004: *value = (int16_t)dub_get_field(dubp, DUBF_WOW); return &DUBD_WOW;
    case 5: *value = dubp[P_DUB_TIML]; return dub_get_sync(dubp) ? &DUBD_TS : &DUBD_TF;
    case 6: *value = dubp[P_DUB_TIMR]; return dub_get_sync(dubp) ? &DUBD_RS : &DUBD_RF;
    default: return 0;                                     /* the panel's own descriptor and the stored value */
    }
}
static void dub_put(uint32_t slot, uint32_t instance, uint32_t id, int32_t value)
{
    (void)slot; (void)instance;
    if (id >= 0x8001 && id <= 0x8004) dub_put_field(dubp, id == 0x8001 ? DUBF_SYNC : id == 0x8002 ? DUBF_PING : id == 0x8003 ? DUBF_DRIVE : DUBF_WOW, value);
}
static const char *const DUB_NAMES_SYNC[] = {"FREE", "SYNC"}, *const DUB_NAMES_ONOFF[] = {"OFF", "ON"};
static const page_t DUB_ROWS[3] = {
    {"DUB SETUP", FAM_FX, SC_FX, GR_NONE, {10, 4, 5, 11}}, {"DUB", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}}, {"DUB 2", FAM_FX, SC_FX, GR_NONE, {6, 12, 13, 7}}};
static const param_desc_t DUB_PANEL[14] = {
    {"FDBK", F_INT, 0, 127, 55, 0, 0}, {"SEND", F_INT, 0, 127, 127, 0, 0}, {"FILTER", F_INT, 0, 127, 60, 0, 0}, {"ECHO", F_INT, 0, 127, 0, 0, 0},
    {"TIM L", F_DUBTS, 0, 127, 53, 0, 0}, {"TIM R", F_DUBTS, 0, 127, 67, 0, 0}, {"SPRNG", F_PCT, 0, 127, 0, 0, 0}, {"SPEED", F_DUBSPD, 0, 127, 64, 0, 0},
    {"", F_INT, 64, 127, 65, 0, 0}, {"", F_INT, -64, 127, 8, 0, 0},                                  /* MODE and TAPE: stored, packed, on no page */
    {"SYNC", F_ENUM, 0, 1, 1, DUB_NAMES_SYNC, 0}, {"PING", F_ENUM, 0, 1, 0, DUB_NAMES_ONOFF, 0}, {"DRIVE", F_INT, 0, 15, 6, 0, 0}, {"WOW", F_INT, 0, 11, 0, 0, 0}};
static const fw_pid_t DUB_PID[14] = {{1, P_DUB_FDBK}, {2, P_DUB_SEND}, {3, P_DUB_FILT}, {4, P_DUB_ECHO}, {5, P_DUB_TIML}, {6, P_DUB_TIMR}, {7, P_DUB_SPRG}, {8, P_DUB_SPD},
                                     {9, P_DUB_MODE}, {10, P_DUB_TAPE}, {0x8001, FW_PID_NONE}, {0x8002, FW_PID_NONE}, {0x8003, FW_PID_NONE}, {0x8004, FW_PID_NONE}};
static void make_dub(fw_pages_t *d)
{
    memset(d, 0, sizeof *d);
    d->magic = FW_PAGES_MAGIC; d->size = FW_PAGES_WIRE; d->version = FW_PAGES_VERSION; d->npages = 3; d->npanel = 14;
    d->rows = DUB_ROWS; d->panel = DUB_PANEL; d->pid = DUB_PID; d->fx_shown = dub_shown; d->fx_put = dub_put;
    memset(d->knob, 0xFF, 4); memset(d->cc, 0xFF, 4);
}
