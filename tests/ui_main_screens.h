/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The main track screen, three layouts (docs/orangecat/UI_DESIGN.md "The main screen"). Only drawing calls of gfx.c and icons.c:
 * the host mockups (tests/ui_main_mockup.c) and the size measure (tools/ui_main_size.sh) both include this file, so what is drawn
 * on the host is the code whose bytes are counted.
 *
 * What a row says: the track's number, the sound's name, and one state word when it applies:
 *   MUTE (red: the track is silenced), OFF (grey: the track gives back its memory, it plays nothing), FROZEN (blue: its notes
 *   cannot be edited), WAIT (the accent: its next pattern waits for the bar).
 * The header is one line: the tempo, the transport mark (a play triangle while the music runs, a square when it is stopped), the
 * bar, the song. Four dials along the bottom are the selected track's four knobs (the picture page's), each with its label and value
 * under it. Three layouts, in the spirit of SLOOP's TRACKS screen (a header band, four track rows, four dials at the foot) and
 * drawn with Orange Cat's own faces, colours and words.
 */
enum { MS_NONE, MS_MUTE, MS_OFF, MS_FROZEN, MS_WAIT };
typedef struct { const char *name; uint8_t state; } ms_row_t;
typedef struct { const char *lab; char val[8]; int16_t q8; uint8_t macro; } ms_knob_t;
typedef struct {
    uint8_t playing, sel, song, bar, pos;        /* pos: the playhead's step 0..15 */
    uint16_t bpm;
    ms_row_t r[4];
    uint16_t steps[4];                           /* the 16 steps of each track in view, bit i = step i has a note */
    const char *engine[4];
    ms_knob_t k[4];                              /* the selected track's four knobs: the label, the value, the dial's position (0..256), a macro's dial starts at the top */
} ms_t;
static void ms_dial(int32_t cx, int32_t cy, int32_t d, uint32_t k, int32_t q8, int macro);   /* (the picture page's dial: ui_home_pic.c hp_dial; the host mockup's own) */

static const char *ms_word(uint32_t st) { return st == MS_MUTE ? "MUTE" : st == MS_OFF ? "OFF" : st == MS_FROZEN ? "FROZEN" : st == MS_WAIT ? "WAIT" : ""; }
/* the state's ink: red, grey, blue (deeper on a light ground), the accent */
static int ms_far(uint16_t c)               /* c reads on the ground: the luma difference of at least 90 of 255 */
{
    int32_t d = (int32_t)ux_luma(c) - (int32_t)ux_luma(T_BG);
    return d > 89 || d < -89;
}
static uint16_t ms_ink(uint32_t st)
{
    return st == MS_MUTE ? (ms_far(T_REC) ? T_REC : T_TEXT) : st == MS_OFF ? T_MID : st == MS_FROZEN ? (ux.light ? RGB(16, 84, 196) : RGB(110, 170, 255)) : T_ACCENT;
}
static void ms_num(char *b, uint32_t v)       /* v 0..999 */
{
    uint32_t n = 0;
    if (v >= 100u) b[n++] = (char)('0' + v / 100u);
    if (v >= 10u) b[n++] = (char)('0' + v / 10u % 10u);
    b[n++] = (char)('0' + v % 10u);
    b[n] = 0;
}
/* the state word in a small pill (dark on a selected row so its own colour reads on the track's colour) */
static int32_t ms_pill(int32_t xr, int32_t cy, uint32_t st, uint16_t rowfill)
{
    const char *w = ms_word(st);
    int32_t pw = text_w(TY_LB, w) + 14;
    cv_rrect(xr - pw, cy - 11, pw, 22, 11, T_BG, rowfill);
    cv_text_c(xr - pw / 2, ty_y(cy, TY_LB), TY_LB, w, ms_ink(st), T_BG);
    return xr - pw;
}


/* the header band (36 px): the tempo in the value face, BPM small, the play mark and the bar, the song at the right */
static void ms_head(const ms_t *m, int32_t h)
{
    char b[24], t[8];
    int32_t x;
    cv_begin(240, h, T_BG);
    ms_num(t, m->bpm);
    x = cv_text(6, ty_y(h / 2, TY_VL), TY_VL, t, T_TEXT) + 4;
    x = cv_text(x, ty_y(h / 2 + 7, TY_LB), TY_LB, "BPM", T_MID) + 14;
    cv_icon_mid(x, h / 2, 16, m->playing ? ICON_X_PLAY : ICON_X_STOP, m->playing ? T_ACCENT : T_MID, T_BG);
    strcpy(b, "BAR "); ms_num(b + 4, m->bar);
    cv_text(x + 22, ty_y(h / 2, TY_TX), TY_TX, b, T_TEXT);
    strcpy(b, "S"); ms_num(b + 1, m->song);
    cv_text_r(236, ty_y(h / 2, TY_TX), TY_TX, b, T_MID, T_BG);
    cv_blit(0, 0);
}
/* the four dials: the ring and its arc in the knob's colour (KCOL: the knobs' plastic, or the selected track's own shades), then the label (grey) and the value (white) under each */
static void ms_dials(const ms_t *m, int32_t y, int labels)
{
    uint32_t k;
    cv_begin(240, labels ? 48 : 240 - y, T_BG);
    for (k = 0; k < 4u; k++) {
        int32_t cx = 2 + 60 * (int32_t)k + 28;
        ms_dial(cx, 15, 26, k, m->k[k].q8, m->k[k].macro);
        if (labels) cv_text_c(cx, ty_y(38, TY_LB), TY_LB, m->k[k].lab, T_MID, T_BG);
        else cv_text_c(cx, ty_y(40, TY_LB), TY_LB, m->k[k].val, T_TEXT, T_BG);
    }
    cv_blit(0, (uint32_t)y);
    if (!labels) return;
    cv_begin(240, 240 - (y + 48), T_BG);
    for (k = 0; k < 4u; k++) cv_text_c(2 + 60 * (int32_t)k + 28, ty_y(9, TY_LB), TY_LB, m->k[k].val, T_TEXT, T_BG);
    cv_blit(0, (uint32_t)(y + 48));
}
/* the track tile: the number on the track's colour (the selected track's in full, the others' colour dim with the number in it) */
static void ms_tile(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t k, int sel, const aafont_t *f, uint16_t under)
{
    char nb[2] = {(char)('1' + k), 0};
    uint16_t dim = ux_mix(T_BG, T_TRK(k), 28);
    cv_rrect(x, y, w, h, 5, sel ? T_TRK(k) : dim, under);
    cv_text_c(x + w / 2, ty_y(y + h / 2, f), f, nb, sel ? T_INK : T_TRK(k), sel ? T_TRK(k) : dim);
}
/* the 16 steps in view: a cell per step, the track's colour where there is a note, the playhead a white bar under the cell playing */
static void ms_steps(const ms_t *m, uint32_t k, int32_t x, int32_t y, int32_t cw, int32_t ch, int sel, int dim)
{
    for (uint32_t j = 0; j < 16u; j++) {
        int on = (m->steps[k] >> j) & 1u;
        int32_t cx = x + (int32_t)j * (cw + 1) + (int32_t)(j / 4u) * 2;
        cv_rect(cx, y, cw, ch, on ? (dim ? T_MID : sel ? T_TRK(k) : ux_mix(T_BG, T_TRK(k), 55)) : T_LINE);
        if (m->playing && j == m->pos) cv_rect(cx, y + ch + 1, cw, 2, T_TEXT);
    }
}
static uint16_t ms_rowbg(const ms_t *m, uint32_t k) { return m->sel == k ? ux_mix(T_BG, T_TRK(k), 16) : T_BG; }
static int ms_quiet(uint32_t st) { return st == MS_MUTE || st == MS_OFF || st == MS_FROZEN; }

/* ------------------------------------------------------------- 1: strips --- */
/* the closest to SLOOP's spirit: a row a track (34 px): the tile, the name, the state word at the right, and under the name the 16 steps in
 * view with the playhead; the selected row on a faint wash of its colour with a full tile and a white name; the dials with labels. */
#define M1_HEAD 36
#define M1_ROW 34
static void ms_row_1(const ms_t *m, uint32_t k)
{
    const ms_row_t *r = &m->r[k];
    int sel = m->sel == k, st = r->state, quiet = ms_quiet(st);
    int32_t avail = 240 - 40 - 6 - (st ? text_w(TY_LB, ms_word(st)) + 14 + 4 : 0);
    const aafont_t *f = ty_fit(TY_TX, r->name, avail);
    uint16_t bg = ms_rowbg(m, k);
    cv_begin(240, M1_ROW, T_BG);
    cv_rrect(2, 0, 236, M1_ROW - 2, 6, bg, T_BG);
    ms_tile(5, 2, 26, M1_ROW - 6, k, sel, TY_TX, bg);
    cv_text_fit(38, ty_y(11, f), f, r->name, quiet ? T_MID : sel ? T_TEXT : T_SEC, bg, avail);
    if (st) ms_pill(234, 11, st, bg);
    ms_steps(m, k, 38, 22, 8, 6, sel, quiet);
    cv_blit(0, (uint32_t)(M1_HEAD + (int32_t)k * M1_ROW));
}
static void ms_draw_1(const ms_t *m)
{
    ms_head(m, M1_HEAD);
    for (uint32_t k = 0; k < 4u; k++) ms_row_1(m, k);
    ms_dials(m, M1_HEAD + 4 * M1_ROW + 2, 1);
}

/* --------------------------------------------------------------- 2: plain --- */
/* no steps: a row a track (36 px) with the name in the value face where it fits; the selected track is the tile in full and a white name,
 * the others a dim tile and a grey name, nothing else; the dials in the selected track's own shades. For a screen that says only what it must. */
#define M2_HEAD 30
#define M2_ROW 34
static void ms_head_2(const ms_t *m)
{
    char b[24], t[8];
    int32_t x;
    cv_begin(240, M2_HEAD, T_BG);
    ms_num(t, m->bpm);
    x = cv_text(6, ty_y(15, TY_TX), TY_TX, t, T_TEXT) + 4;
    x = cv_text(x, ty_y(15, TY_LB), TY_LB, "BPM", T_MID) + 16;
    cv_icon_mid(x, 15, 16, m->playing ? ICON_X_PLAY : ICON_X_STOP, m->playing ? T_ACCENT : T_MID, T_BG);
    strcpy(b, "BAR "); ms_num(b + 4, m->bar);
    cv_text(x + 22, ty_y(15, TY_TX), TY_TX, b, T_TEXT);
    cv_blit(0, 0);
}
static void ms_row_2(const ms_t *m, uint32_t k)
{
    const ms_row_t *r = &m->r[k];
    int sel = m->sel == k, st = r->state, quiet = ms_quiet(st);
    int32_t avail = 240 - 44 - 6 - (st ? text_w(TY_LB, ms_word(st)) + 14 + 4 : 0);
    const aafont_t *f = text_w(TY_VL, r->name) <= avail && text_has(TY_VL, r->name) ? TY_VL : ty_fit(TY_TX, r->name, avail);
    cv_begin(240, M2_ROW, T_BG);
    ms_tile(4, 2, 32, M2_ROW - 6, k, sel, TY_VL, T_BG);
    cv_text_fit(44, ty_y(16, f), f, r->name, quiet ? T_MID : sel ? T_TEXT : T_SEC, T_BG, avail);
    if (st) ms_pill(236, 16, st, T_BG);
    cv_blit(0, (uint32_t)(M2_HEAD + 2 + (int32_t)k * M2_ROW));
}
static void ms_draw_2(const ms_t *m)
{
    ms_head_2(m);
    for (uint32_t k = 0; k < 4u; k++) ms_row_2(m, k);
    ms_dials(m, M2_HEAD + 2 + 4 * M2_ROW + 2, 1);
}

/* -------------------------------------------------------------- 3: panel --- */
/* the selected track is a panel (62 px) in the track's colour wash: the tile, the name in the value face, under it the engine and the state word,
 * then its 16 steps large with the playhead; the other three are short rows (26 px: the tile, the name, the state word). Dials, values only. */
#define M3_HEAD 36
#define M3_PANEL 62
#define M3_ROW 27
static void ms_panel_3(const ms_t *m, uint32_t k, int32_t y)
{
    const ms_row_t *r = &m->r[k];
    int st = r->state, quiet = ms_quiet(st);
    int32_t avail = 240 - 52 - 6;
    const aafont_t *f = text_w(TY_VL, r->name) <= avail && text_has(TY_VL, r->name) ? TY_VL : ty_fit(TY_TX, r->name, avail);
    uint16_t bg = ms_rowbg(m, k);
    cv_begin(240, M3_PANEL, T_BG);
    cv_rrect(2, 0, 236, M3_PANEL - 2, 6, bg, T_BG);
    ms_tile(5, 3, 38, 38, k, 1, TY_VL, bg);
    cv_text_fit(52, ty_y(15, f), f, r->name, quiet ? T_MID : T_TEXT, bg, avail);
    cv_text(52, ty_y(36, TY_LB), TY_LB, m->engine[k], T_MID);
    if (st) ms_pill(234, 36, st, bg);
    ms_steps(m, k, 8, 48, 11, 9, 1, quiet);
    cv_blit(0, (uint32_t)y);
}
static void ms_row_3(const ms_t *m, uint32_t k, int32_t y)
{
    const ms_row_t *r = &m->r[k];
    int st = r->state, quiet = ms_quiet(st);
    int32_t avail = 240 - 38 - 6 - (st ? text_w(TY_LB, ms_word(st)) + 14 + 4 : 0);
    const aafont_t *f = ty_fit(TY_TX, r->name, avail);
    cv_begin(240, M3_ROW, T_BG);
    ms_tile(5, 1, 24, M3_ROW - 4, k, 0, TY_TX, T_BG);
    cv_text_fit(38, ty_y(12, f), f, r->name, quiet ? T_MID : T_SEC, T_BG, avail);
    if (st) ms_pill(236, 12, st, T_BG);
    cv_blit(0, (uint32_t)y);
}
static void ms_draw_3(const ms_t *m)
{
    int32_t y = M3_HEAD;
    ms_head(m, M3_HEAD);
    for (uint32_t k = 0; k < 4u; k++) {
        if (m->sel == k) { ms_panel_3(m, k, y); y += M3_PANEL; }
        else { ms_row_3(m, k, y); y += M3_ROW; }
    }
    ms_dials(m, y + 2, 0);
}
