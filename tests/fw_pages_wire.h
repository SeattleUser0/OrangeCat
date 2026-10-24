/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2: a page descriptor (fw_pages_t, host pointers) written out in the DEVICE's wire layout (4 byte pointers, fw_pages.h) into a flat image, as the
 * linker lays a module out: what the loader's check (fw_pages_check) reads from the flash. Shared by tests/module_host_prophet_test.c (the real Prophet
 * descriptor must pass the check) and tests/fuzz_fw_pages.c (mutations of that image). Strings, rows, the panel, the name lists, the skip table and the title
 * lists are copied in; the hooks become addresses inside the image.
 *
 *   fwp_wire(d, img, cap, base, &desc_addr)    returns the image length (the first 0x100 bytes stand for the header page), 0 if it does not fit
 *
 * Revisions: 0.12.2: first version. */
#ifndef FW_PAGES_WIRE_H
#define FW_PAGES_WIRE_H
typedef struct { uint8_t *img; uint32_t cap, n, base; const void *k[512]; uint32_t at[512]; uint32_t nk; } fwpw_t;
static uint32_t fwpw_alloc(fwpw_t *w, uint32_t n)
{
    uint32_t at = (w->n + 3u) & ~3u;
    if (at + n > w->cap) return 0;
    memset(w->img + at, 0, n);
    w->n = at + n;
    return w->base + at;
}
static void fwpw_w32(fwpw_t *w, uint32_t addr, uint32_t v) { uint8_t *p = w->img + (addr - w->base); p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint32_t fwpw_str(fwpw_t *w, const char *s)             /* interned: one copy of a string */
{
    uint32_t a, n;
    if (!s) return 0;
    for (uint32_t i = 0; i < w->nk; i++) if (w->k[i] == s) return w->at[i];
    n = (uint32_t)strlen(s) + 1u;
    a = fwpw_alloc(w, n);
    if (!a || w->nk >= 512u) return 0;
    memcpy(w->img + (a - w->base), s, n);
    w->k[w->nk] = s; w->at[w->nk++] = a;
    return a;
}
static uint32_t fwpw_list(fwpw_t *w, const char *const *l, uint32_t count, int zero_end)   /* count strings (and a 0 pointer after them) */
{
    uint32_t a = fwpw_alloc(w, 4u * (count + 1u));
    if (!l) return 0;
    for (uint32_t i = 0; i < count; i++) fwpw_w32(w, a + 4u * i, fwpw_str(w, l[i]));
    (void)zero_end;
    return a;
}
static uint32_t fwp_wire(const fw_pages_t *d, uint8_t *img, uint32_t cap, uint32_t base, uint32_t *desc_addr)
{
    fwpw_t w = { img, cap, 0x100u, base, {0}, {0}, 0 };
    uint32_t da, rows, panel, hook, skip = 0, env = 0, lfo = 0, i;
    memset(img, 0, cap);
    hook = base + 0x104u;                                       /* an address inside the code, for every hook that is set */
    rows = fwpw_alloc(&w, 12u * d->npages);
    panel = fwpw_alloc(&w, 20u * d->npanel);
    da = fwpw_alloc(&w, FW_PAGES_WIRE);
    if (!rows || !panel || !da) return 0;
    for (i = 0; i < d->npages; i++) {
        uint8_t *r = img + (rows - base) + 12u * i;
        fwpw_w32(&w, rows + 12u * i, fwpw_str(&w, d->rows[i].title));
        r[4] = d->rows[i].fam; r[5] = d->rows[i].scope; r[6] = d->rows[i].graph;
        memcpy(r + 7, d->rows[i].id, 4);
    }
    for (i = 0; i < d->npanel; i++) {
        const param_desc_t *p = &d->panel[i];
        uint8_t *q = img + (panel - base) + 20u * i;
        uint32_t names = 0;
        if (p->label) {
            if (p->names) {
                uint32_t k = 0;
                if (p->fmt == F_ENUM) { names = fwpw_list(&w, p->names, (uint32_t)p->max + 1u, 0); }
                else { while (p->names[k]) k++; names = fwpw_list(&w, p->names, k, 1); }
            }
            fwpw_w32(&w, panel + 20u * i, fwpw_str(&w, p->label));
            fwpw_w32(&w, panel + 20u * i + 12u, names);
            fwpw_w32(&w, panel + 20u * i + 16u, fwpw_str(&w, p->unit));
        }
        q[4] = p->fmt;
        q[6] = (uint8_t)p->min; q[7] = (uint8_t)((uint16_t)p->min >> 8);
        q[8] = (uint8_t)p->max; q[9] = (uint8_t)((uint16_t)p->max >> 8);
        q[10] = (uint8_t)p->def; q[11] = (uint8_t)((uint16_t)p->def >> 8);
    }
    if (d->motion_skip) {
        skip = fwpw_alloc(&w, (d->npanel + 7u) / 8u);
        if (!skip) return 0;
        memcpy(img + (skip - base), d->motion_skip, (d->npanel + 7u) / 8u);
    }
    for (int h = 0; h < 2; h++) {
        const char *const *l = h ? d->lfo_titles : d->env_titles;
        uint32_t n = 0, a;
        if (!l) continue;
        while (l[n]) n++;
        a = fwpw_list(&w, l, n, 1);
        if (h) lfo = a; else env = a;
    }
    fwpw_w32(&w, da, FW_PAGES_MAGIC);
    img[da - base + 4] = (uint8_t)FW_PAGES_WIRE; img[da - base + 5] = (uint8_t)(FW_PAGES_WIRE >> 8);
    img[da - base + 6] = (uint8_t)d->version; img[da - base + 7] = (uint8_t)(d->version >> 8);
    img[da - base + 8] = (uint8_t)d->npages; img[da - base + 10] = (uint8_t)d->npanel;
    fwpw_w32(&w, da + 12, rows); fwpw_w32(&w, da + 16, panel);
    fwpw_w32(&w, da + 20, d->native ? hook : 0); fwpw_w32(&w, da + 24, d->shown ? hook : 0);
    fwpw_w32(&w, da + 28, d->put ? hook : 0); fwpw_w32(&w, da + 32, d->active ? hook : 0);
    fwpw_w32(&w, da + 36, skip); fwpw_w32(&w, da + 40, env); fwpw_w32(&w, da + 44, lfo);
    memcpy(img + (da - base) + 48, d->knob, 4);
    fwpw_w32(&w, da + 52, d->loaded ? hook : 0); fwpw_w32(&w, da + 56, d->env_source ? hook : 0); fwpw_w32(&w, da + 60, d->bend ? hook : 0);
    memcpy(img + (da - base) + 64, d->cc, 4);
    *desc_addr = da;
    return (w.n + 3u) & ~3u;
}
#endif
