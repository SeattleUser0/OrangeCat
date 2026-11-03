/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 8 (MODULE_FORMAT_R3.md 16): the four reverb modes as module descriptors, as data, shared by tests/mod_ui_test.c (the checker) and tests/fx_legacy_test.c (the Release 2 import).
 * Needs fw_pages.h, the tests' param tables (core.h/params.c) and an image to write into by the includer. */
#pragma once
typedef struct { const char *label; uint8_t fmt; int16_t mn, mx, def; const char *unit; const char *const *names; } exp_t;
static const char *const X_FOLW[] = {"OFF", "T1", "T2", "T3", "T4"};
static const char *const X_GSIZE[] = {"LONG", "MID", "SHORT", "TINY"}, *const X_TTIME[] = {"1/64", "1/32", "1/16", "1/8", "1/4"};
static const char *const X_SSIZE[] = {"1/4", "1/8", "1/16"}, *const X_FSIZE[] = {"1/128", "1/64", "1/32", "1/16", "1/8"};
static param_desc_t xpanel[8];
static fw_pid_t xpid[8];
static page_t xrows[2];
static uint32_t xbytes;
static void x_fill(fw_pages_t *d, const exp_t *e, uint32_t n, const page_t *rows, uint32_t nrows)
{
    memset(d, 0, sizeof *d);
    for (uint32_t i = 0; i < n; i++) xpanel[i] = (param_desc_t){e[i].label, e[i].fmt, e[i].mn, e[i].mx, e[i].def, e[i].names, e[i].unit};
    for (uint32_t i = 0; i < nrows; i++) xrows[i] = rows[i];
    d->magic = FW_PAGES_MAGIC; d->size = FW_PAGES_WIRE; d->version = FW_PAGES_VERSION; d->npages = (uint16_t)nrows; d->npanel = (uint16_t)n;
    d->rows = xrows; d->panel = xpanel; d->pid = xpid;
    memset(d->knob, 0xFF, 4); memset(d->cc, 0xFF, 4);
}
static int desc_same(const param_desc_t *a, const param_desc_t *b)
{
    if (strcmp(a->label, b->label) || a->fmt != b->fmt || a->min != b->min || a->max != b->max || a->def != b->def) return 0;
    if ((a->unit == 0) != (b->unit == 0) || (a->unit && strcmp(a->unit, b->unit))) return 0;
    if (a->fmt == F_ENUM) for (int v = a->min; v <= a->max; v++) if (strcmp(a->names[v], b->names[v])) return 0;
    return 1;
}
/* the reverb modes as data; the same table drives tests/fx_legacy_test.c's Release 2 import through make_reverb() */
static const page_t REV_ROWS[2] = {{"REVERB", FAM_FX, SC_FX, GR_NONE, {0xFF, 0, 1, 2}}, {"REVERB 2", FAM_FX, SC_FX, GR_NONE, {3, 4, 5, 6}}};
static const char *const REV_NM[4] = {"SPRING", "SHIMMER", "RESO", "CLOUD"};
static const uint32_t REV_TYPE[4] = {1, 2, 3, 4};                  /* G_RTYPE: ROOM 0, SPRING 1, SHIMR 2, RESO 3, CLOUD 4 */
static const uint32_t REV_G[7] = {G_RSIZE, G_RDAMP, G_RPRE, G_RMOD, G_RRATE, G_RFILT, G_RWIDE};
static exp_t rev_m[4][7];
static void make_reverb(fw_pages_t *d, uint32_t k)
{
    static const exp_t SPRING[7] = {{"SIZE", F_PCT, 0, 127, 90, 0, 0}, {"DAMP", F_PCT, 0, 127, 60, 0, 0}, {"PRE", F_INT, 0, 100, 0, "ms", 0},
                                    {"MOD", F_PCT, 0, 127, 0, 0, 0}, {"RATE", F_LFOHZ, 0, 127, 30, 0, 0}, {"FILT", F_BIPCT, -64, 63, 0, 0, 0}, {"WIDE", F_PCT, 0, 127, 0, 0, 0}};
    memcpy(rev_m[k], SPRING, sizeof SPRING);
    if (k == 1 || k == 3) rev_m[k][2] = (exp_t){"AIR", F_SEMI, -24, 24, 0, 0, 0};
    if (k == 2) { rev_m[k][1] = (exp_t){"FOLW", F_ENUM, 0, 4, 0, 0, X_FOLW}; rev_m[k][2] = (exp_t){"PITCH", F_INT, 0, 14, 0, 0, 0}; }
    x_fill(d, rev_m[k], 7, REV_ROWS, 2);
    for (uint32_t i = 0; i < 7; i++) { xpid[i].id = (uint16_t)(1 + i); xpid[i].legacy = (uint16_t)(FW_LEG_GLOBAL | REV_G[i]); }   /* (L1) the values are song.g's */
    d->ext = (G_RTYPE + 1u) | (REV_TYPE[k] << 16);                  /* (L1) imported only while TYPE is this mode */
}
