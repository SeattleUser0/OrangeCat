/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The main track screen, three layouts (docs/orangecat/UI_DESIGN.md "The main screen"). Only drawing calls of gfx.c and icons.c:
 * the host mockups (tests/ui_main_mockup.c) and the size measure (tools/ui_main_size.sh) both include this file, so what is drawn
 * on the host is the code whose bytes are counted.
 *
 * What a row says: the track's number, the sound's name, and one state word when it applies:
 *   MUTE (red: the track is silenced), OFF (grey: the track gives back its memory, it plays nothing), FROZEN (blue: its notes
 *   cannot be edited), WAIT (the accent: its next pattern waits for the bar).
 * The header is one line: the transport mark (a play triangle while the music runs, a square when it is stopped), the song
 * and the bar, the tempo.
 */
enum { MS_NONE, MS_MUTE, MS_OFF, MS_FROZEN, MS_WAIT };
typedef struct { const char *name; uint8_t state; } ms_row_t;
typedef struct {
    uint8_t playing, sel, song, bar, page;
    uint16_t bpm;
    ms_row_t r[4];
} ms_t;

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
/* the header: transport mark, SONG n BAR m (a short form for want of room), the tempo at the right */
static void ms_head(const ms_t *m, int32_t h, int dots)
{
    char b[24], t[8];
    int32_t x = 30;
    cv_begin(240, h, T_BG);
    cv_icon_mid(6, h / 2, 16, m->playing ? ICON_X_PLAY : ICON_X_STOP, m->playing ? T_ACCENT : T_MID, T_BG);
    (void)dots;
    strcpy(b, "S"); ms_num(b + 1, m->song);
    x = cv_text(x, ty_y(h / 2, TY_LB), TY_LB, b, T_MID) + 8;
    strcpy(b, "BAR "); ms_num(b + 4, m->bar);
    cv_text(x, ty_y(h / 2, TY_TX), TY_TX, b, T_TEXT);
    ms_num(t, m->bpm);
    cv_text_r(236, ty_y(h / 2, TY_TX), TY_TX, t, T_TEXT, T_BG);
    cv_blit(0, 0);
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

/* ----------------------------------------------------------------- A: the list --- */
/* four equal rows, 50 px: the number on the track's colour, the name, the state word at the right. The selected row is filled
 * in the track's colour with the text dark on it; the others are on the ground. A muted or OFF row's name is dim. */
#define MA_Y0 34
#define MA_H 50
static void ms_row_a(const ms_t *m, uint32_t k)
{
    const ms_row_t *r = &m->r[k];
    int sel = m->sel == k, st = r->state;
    int32_t avail = 240 - 56 - 8, w;
    uint16_t fill = sel ? T_TRK(k) : T_BG, ink = sel ? T_INK : st == MS_MUTE || st == MS_OFF || st == MS_FROZEN ? T_MID : T_TEXT;
    char nb[4];
    const aafont_t *f;
    cv_begin(240, MA_H, T_BG);
    cv_rrect(4, 0, 232, MA_H - 2, 8, sel ? fill : T_SURF, T_BG);
    cv_rrect(8, 4, 40, MA_H - 10, 6, T_TRK(k), sel ? fill : T_SURF);
    nb[0] = (char)('1' + k); nb[1] = 0;
    cv_text_c(28, ty_y(MA_H / 2 - 1, TY_VL), TY_VL, nb, T_INK, T_TRK(k));
    if (st) avail -= text_w(TY_LB, ms_word(st)) + 14 + 6;
    f = text_w(TY_VL, r->name) <= avail && text_has(TY_VL, r->name) ? TY_VL : ty_fit(TY_TX, r->name, avail);
    w = avail;
    cv_text_fit(56, ty_y(MA_H / 2 - 1, f), f, r->name, ink, fill, w);
    if (st) ms_pill(232, MA_H / 2 - 1, st, sel ? fill : T_SURF);
    cv_blit(0, (uint32_t)(MA_Y0 + (int32_t)k * (MA_H + 2)));
}
static void ms_draw_a(const ms_t *m)
{
    ms_head(m, MA_Y0, 0);
    for (uint32_t k = 0; k < 4u; k++) ms_row_a(m, k);
}

/* -------------------------------------------------------------- B: the focus --- */
/* the selected track is a tall row (80 px): its number in the hero face, its name in the value face, the state word under it;
 * the other three are short rows (36 px), the number, the name, the state word. The rows stay in their order. */
#define MB_SEL_H 78
#define MB_ROW_H 36
static void ms_row_b(const ms_t *m, uint32_t k, int32_t y)
{
    const ms_row_t *r = &m->r[k];
    int sel = m->sel == k, st = r->state;
    char nb[4];
    int32_t h = sel ? MB_SEL_H : MB_ROW_H;
    uint16_t ink = st == MS_MUTE || st == MS_OFF || st == MS_FROZEN ? T_MID : T_TEXT;
    nb[0] = (char)('1' + k); nb[1] = 0;
    cv_begin(240, h, T_BG);
    if (sel) {
        int32_t avail = 240 - 72 - 8;
        const aafont_t *f = text_w(TY_VL, r->name) <= avail && text_has(TY_VL, r->name) ? TY_VL : TY_TX;
        cv_rrect(4, 0, 232, h - 2, 8, T_TRK(k), T_BG);
        cv_text_c(32, ty_y(h / 2 - 1, TY_HR), TY_HR, nb, T_INK, T_TRK(k));
        cv_text_fit(64, ty_y(st ? 24 : h / 2 - 1, f), f, r->name, T_INK, T_TRK(k), avail);
        if (st) {
            int32_t pw = text_w(TY_LB, ms_word(st)) + 14;
            cv_rrect(64, 46, pw, 22, 11, T_BG, T_TRK(k));
            cv_text_c(64 + pw / 2, ty_y(57, TY_LB), TY_LB, ms_word(st), ms_ink(st), T_BG);
        }
    } else {
        int32_t avail = 240 - 40 - 8 - (st ? text_w(TY_LB, ms_word(st)) + 14 + 6 : 0);
        cv_rrect(4, 0, 232, h - 2, 8, T_SURF, T_BG);
        cv_rrect(8, 4, 24, h - 10, 5, T_TRK(k), T_SURF);
        cv_text_c(20, ty_y(h / 2 - 1, TY_TX), TY_TX, nb, T_INK, T_TRK(k));
        cv_text_fit(40, ty_y(h / 2 - 1, ty_fit(TY_TX, r->name, avail)), ty_fit(TY_TX, r->name, avail), r->name, ink, T_SURF, avail);
        if (st) ms_pill(232, h / 2 - 1, st, T_SURF);
    }
    cv_blit(0, (uint32_t)y);
}
static void ms_draw_b(const ms_t *m)
{
    int32_t y = 32;
    ms_head(m, 32, 0);
    for (uint32_t k = 0; k < 4u; k++) {
        ms_row_b(m, k, y);
        y += (m->sel == k ? MB_SEL_H : MB_ROW_H) + 2;
    }
}

/* ------------------------------------------------- C: the picture is the main screen --- */
/* page 1: the header (the bar, the four track numbers as chips, the tempo), then the sound line (the number, the name, the state
 * word) and the picture page as it is today (dials and the engine's picture: tests/ui_mockup.c's home()); page 2 is layout A.
 * The chips: the selected track's number on its colour; the others in the text colour; a MUTE, OFF or FROZEN track's dim. */
#define MC_HEAD 30
static void ms_head_c(const ms_t *m)
{
    char b[24], t[8];
    int32_t x = 0;
    cv_begin(240, MC_HEAD, T_BG);
    cv_icon_mid(6, MC_HEAD / 2, 16, m->playing ? ICON_X_PLAY : ICON_X_STOP, m->playing ? T_ACCENT : T_MID, T_BG);
    strcpy(b, "BAR "); ms_num(b + 4, m->bar);
    cv_text(28, ty_y(MC_HEAD / 2, TY_TX), TY_TX, b, T_TEXT);
    for (uint32_t k = 0; k < 4u; k++) {            /* the four chips */
        int sel = m->sel == k, st = m->r[k].state, dim = st == MS_MUTE || st == MS_OFF || st == MS_FROZEN;
        char nb[2] = {(char)('1' + k), 0};
        x = 112 + (int32_t)k * 20;
        if (sel) cv_rrect(x, 5, 18, 20, 5, T_TRK(k), T_BG);
        cv_text_c(x + 9, ty_y(15, TY_LB), TY_LB, nb, sel ? T_INK : dim ? T_DIM : T_TEXT, sel ? T_TRK(k) : T_BG);
    }
    ms_num(t, m->bpm);
    cv_text_r(236, ty_y(MC_HEAD / 2, TY_TX), TY_TX, t, T_TEXT, T_BG);
    cv_blit(0, 0);
}
/* the sound line of page 1 (30 .. 54): the number, the name, the state word where the place in the list was */
static void ms_line_c(const ms_t *m, const char *name)
{
    const ms_row_t *r = &m->r[m->sel];
    char nb[2] = {(char)('1' + m->sel), 0};
    int32_t avail = 240 - 40 - 8 - (r->state ? text_w(TY_LB, ms_word(r->state)) + 14 + 6 : 0);
    cv_begin(240, 24, T_BG);
    cv_rrect(4, 2, 20, 20, 5, r->state ? T_DIM : T_TRK(m->sel), T_BG);
    cv_text_c(14, ty_y(12, TY_TX), TY_TX, nb, T_INK, r->state ? T_DIM : T_TRK(m->sel));
    cv_text_fit(32, ty_y(12, ty_fit(TY_TX, name, avail)), ty_fit(TY_TX, name, avail), name, r->state ? T_MID : T_TEXT, T_BG, avail);
    if (r->state) ms_pill(236, 12, r->state, T_BG);
    cv_blit(0, 30);
}
