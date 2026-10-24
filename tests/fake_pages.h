/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.0 (Part 2b): a fake module's page descriptor (firmware/src/fw_pages.h), the one tests/mod_pages_test.c and tests/editor_test.c use.
 * Included after the firmware (it needs track_t, param_desc_t and the F_ formats). A "Prophet in the Orange Cat set" stand-in: eight native
 * parameters (one a hole), four pages, the ENV and LFO tab lists, the hooks (a shown value, a put, a dim). Host pointers (8 bytes): the UI reads these
 * as structs; the wire check (fw_pages_check) is tested on bytes built by hand in mod_pages_test.c. */
#pragma once
static uint8_t fp_blk[16][96];                    /* the tracks' native blocks, byte i = panel index i */
static uint32_t fp_put_calls, fp_put_id;
static int32_t fp_put_val;
static uint8_t *fp_native(track_t *t) { return fp_blk[trk_index(t) & 15u]; }
static const param_desc_t FP_SHOWN_BEND = {"BEND", F_INT, 1, 13, 3, 0, "st"};
static const param_desc_t *fp_shown(const track_t *t, uint32_t id, int16_t *value)
{
    (void)t;
    if (id != 3u) return 0;                       /* BEND is shown from 1 */
    *value = (int16_t)(*value + 1);
    return &FP_SHOWN_BEND;
}
static void fp_put(track_t *t, uint32_t id, int32_t v)
{
    fp_put_calls++; fp_put_id = id; fp_put_val = v;
    if (id == 3u) v -= 1;                         /* the shown value back to the byte */
    fp_native(t)[id] = (uint8_t)v;
}
static int fp_active(const track_t *t, uint32_t id) { return id != 1u || fp_blk[trk_index((track_t *)t) & 15u][0] > 0; }   /* RES does nothing at CUTOFF 0 */
static const char *const FP_MODES[] = {"LP", "BP", "HP"};
static const param_desc_t FP_PANEL[8] = {
    {"CUTOFF", F_INT, 0, 127, 64, 0, 0},
    {"RES", F_INT, 0, 127, 0, 0, 0},
    {"MODE", F_ENUM, 0, 2, 0, FP_MODES, 0},
    {"BEND", F_INT, 0, 12, 2, 0, "st"},
    {"UNISON", F_INT, 0, 8, 0, 0, 0},
    {"TUNE", F_INT, 0, 96, 48, 0, 0},
    {0, 0, 0, 0, 0, 0, 0},                        /* a hole: no parameter 6 */
    {"LEVEL", F_INT, 0, 127, 100, 0, 0},
};
static const page_t FP_ROWS[4] = {
    {"X FILTER", FAM_EDIT, SC_MOD, GR_NONE, {0, 1, 2, 0xFF}},
    {"X MISC", FAM_EDIT, SC_MOD, GR_NONE, {3, 4, 5, 7}},
    {"X ENV", FAM_EDIT, SC_MOD, GR_NONE, {0, 7, 0xFF, 0xFF}},
    {"X LFO", FAM_EDIT, SC_MOD, GR_NONE, {5, 0xFF, 0xFF, 0xFF}},
};
static const uint8_t FP_SKIP[1] = {0x10};         /* UNISON (4): motion leaves it */
static const char *const FP_ENV[] = {"X ENV", 0};
static const char *const FP_LFO[] = {"X LFO", 0};
static const fw_pages_t FP_DESC = {FW_PAGES_MAGIC, sizeof(fw_pages_t), FW_PAGES_VERSION, 4, 8, FP_ROWS, FP_PANEL, fp_native, fp_shown, fp_put, fp_active,
                                   FP_SKIP, FP_ENV, FP_LFO, {0, 1, 2, 7}};
