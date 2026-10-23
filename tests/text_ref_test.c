/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.7.0) The firmware's text (src/gfx.c cv_text, Melodee Pixel alone) against the font's own source,
 * assets/fonts/melodee_pixel.txt, read here without the generator (tools/gen_aa_font.py): every glyph of every role
 * (LB its condensed set where it has one, TX, VL, HR) and some strings, drawn by the firmware and by the reference
 * below on a 240 x 62 canvas: every pixel and the end pen must be equal. A font pixel is a px x px square; a glyph's
 * row 0 is the capitals' top, at y + asc - 7 px; its advance is its width + 1, in font pixels. A character the face
 * lacks is drawn as '?' (the ellipsis as '.'), and the string itself is left as it is.
 * (To 0.6.2 this compared the Rubik text path with the redesign prototype's; that path is gone.) */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __attribute__(x)
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"

typedef struct { int top, w, h; char rows[11][9]; } src_glyph_t;
static src_glyph_t reg[256], nar[256];
static uint8_t has_reg[256], has_nar[256];

static void load(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[256];
    src_glyph_t *cur = 0;
    assert(f);
    while (fgets(line, sizeof line, f)) {
        unsigned cp;
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "# ", 2) || !strcmp(line, "#!"))
            continue;                                    /* a comment (as tools/gen_aa_font.py pixel_glyphs reads them) */
        if (sscanf(line, "glyph U+%x", &cp) == 1) {
            unsigned code = cp == 0x2026u ? 0x85u : cp;   /* (the ellipsis: its one-byte code, gfx.c ELLIPSIS) */
            int narrow = strstr(line, " narrow") != 0;
            const char *t = strstr(line, " top ");
            assert(code < 256u);
            cur = narrow ? &nar[code] : &reg[code];
            (narrow ? has_nar : has_reg)[code] = 1;
            memset(cur, 0, sizeof *cur);
            cur->top = t ? atoi(t + 5) : 0;
            continue;
        }
        if (cur && line[0] && strspn(line, "#.") > 0 && line[strspn(line, "#.")] <= ' ') {
            int n = (int)strspn(line, "#.");
            assert(cur->h < 11 && n < 9);
            memcpy(cur->rows[cur->h], line, (size_t)n);
            cur->w = n;
            cur->h++;
        } else {
            cur = 0;
        }
    }
    fclose(f);
}

static uint16_t ref[240 * 62];
/* s in face f at (x, y) by the source's glyphs; returns the pen */
static int32_t ref_text(int32_t x, int32_t y, const aafont_t *f, const char *s, uint16_t ink)
{
    int32_t px = f->px, narrow = f == TY_LB;
    for (; *s; s++) {
        unsigned c = (uint8_t)*s;
        const src_glyph_t *g;
        if (c >= 'a' && c <= 'z' && !has_reg[c]) c -= 32u;
        if (!has_reg[c]) c = c == 0x85u && has_reg['.'] ? '.' : '?';
        g = narrow && has_nar[c] ? &nar[c] : &reg[c];
        for (int r = 0; r < g->h; r++)
            for (int k = 0; k < g->w; k++)
                if (g->rows[r][k] == '#')
                    for (int j = 0; j < px; j++)
                        for (int i = 0; i < px; i++) {
                            int32_t X = x + k * px + i, Y = y + f->asc - 7 * px + (g->top + r) * px + j;
                            if (X >= 0 && X < 240 && Y >= 0 && Y < 62) ref[Y * 240 + X] = ink;
                        }
        x += (g->w + 1) * px;
    }
    return x;
}

static void same(const aafont_t *f, const char *name, const char *s, int32_t x, int32_t y)
{
    const uint16_t ink = swap16(T_TEXT), bg = swap16(T_BG);
    int32_t e1, e2;
    cv_begin(240, 62, T_BG);
    e1 = cv_text(x, y, f, s, T_TEXT);
    for (unsigned i = 0; i < 240u * 62u; i++) ref[i] = bg;
    e2 = ref_text(x, y, f, s, ink);
    if (e1 != e2 || memcmp(ref, cv_px, sizeof ref)) {
        fprintf(stderr, "text_ref: %s '%s': pen %d, reference %d%s\n", name, s, (int)e1, (int)e2,
                memcmp(ref, cv_px, sizeof ref) ? ", pixels differ" : "");
        exit(1);
    }
    assert(text_w(f, s) == e1 - x);
}

int main(void)
{
    static const struct { const aafont_t *f; const char *name; } F[] = {
        {TY_LB, "LB"}, {TY_TX, "TX"}, {TY_VL, "VL"}, {TY_HR, "HR"}};
    static const char *const STR[] = {"CUTOFF 127", "Hello, world", "-12.5 dB", "C#4 / Ab3", "A minor", "100 %",
                                      "\xC4\xD6\xDC \xE4\xF6\xFC \xA9 \xB0 \xB7", "LONG NAME\x85", "~{}|", "WWWW",
                                      "a\x01z"};   /* (\x01: no glyph, drawn as '?') */
    unsigned n = 0;
    load("assets/fonts/melodee_pixel.txt");
    palette_set(0);
    for (unsigned k = 0; k < 4; k++) {
        const int32_t y = F[k].f->h <= 62 ? (62 - F[k].f->h) / 2 : 0;
        assert(F[k].f->h <= 62);
        for (unsigned c = 32; c < 256; c++) {            /* every glyph, alone */
            char s[2] = {(char)c, 0};
            if (!has_reg[c] && c != 0x85u) continue;
            same(F[k].f, F[k].name, s, 7, y);
            n++;
        }
        for (unsigned i = 0; i < sizeof STR / sizeof STR[0]; i++) {
            char s[24];
            snprintf(s, sizeof s, "%s", STR[i]);
            if (F[k].f->px >= 5) s[6] = 0;              /* (the hero: what fits 240 px) */
            same(F[k].f, F[k].name, s, 2, y);
            n++;
        }
    }
    printf("text: %u glyphs and strings in LB TX VL HR equal to assets/fonts/melodee_pixel.txt, pixels and pens\n", n);
    return 0;
}
