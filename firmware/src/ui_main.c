/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HOME's main screen (UI_DESIGN.md "The main screen", layout 1, strips; the owner's pick of November 18, 2026): under the shared header
 * (the tempo, the transport mark, REC, the messages, the page's name) four rows of 34 px, a track a row, and at the foot the selected
 * track's four knobs as dials. Included by ui_draw.c after ui_home_pic.c, whose dial and knob words it draws.
 *
 * A row: the track's number on its colour (the selected one's in full, the others' dim), the sound's name, at the right one state word
 * when it applies (OFF, WAIT, MUTE, FROZEN, in that order of precedence), and under the name the 16 steps in view: a cell a step in the
 * track's colour where there is a note, the step playing marked by a white bar under its cell. The bar in view is the playing one
 * (the first while it is stopped). The selected row is on a faint wash of its colour with a white name.
 * The dials: each knob's ring in the knob's own colour (the picture page's, hp_dial), its label (grey) under it and its value (white)
 * under that. A knob turning is the one hot dial (a larger pointer).
 * Redraws: a row when its name, state or selection change, its steps alone when they or the playhead change, the dials when a value
 * changes (every frame is looked at, as the picture page's; the rows every other frame, as the Stage's were).
 * The Stage's step lights, scope band, knob drop-in and REC border are not kept (the owner, November 18). A place not in use (UNUSED
 * TRACKS SHOW) is a greyed row "Add track". */
#define MN_Y 32                                         /* the first row's top */
#define MN_ROW 34                                       /* a row, its 2 px gap included */
#define MN_DIAL_Y (MN_Y + 4 * MN_ROW + 2)               /* the dials: 170 .. 217, their values 218 .. 239 */
#define MN_DIAL_H 48
#define MN_STEP_X 38                                    /* the steps' left edge in a row, their cell, the playhead's bar */
#define MN_STEP_Y 22
#define MN_STEP_W (16 * 9 + 3 * 2)                      /* 16 cells of 8 px, 1 px apart, a beat (4) 2 px further from the next */
enum { MN_NONE, MN_MUTE, MN_OFF, MN_FROZEN, MN_WAIT };

static struct {
    uint32_t col[NTRK], grid[NTRK];                     /* each row's drawn state, its steps' */
    uint32_t sig_dials, sig_vals;
    uint8_t rows;                                       /* the rows drawn the last time (a change clears the screen below the last) */
} mn;

static const char *mn_word(uint32_t st) { return st == MN_MUTE ? "MUTE" : st == MN_OFF ? "OFF" : st == MN_FROZEN ? "FROZEN" : st == MN_WAIT ? "WAIT" : ""; }
static int mn_far(uint16_t c)                           /* c reads on the ground: the luma difference of at least 90 of 255 */
{
    int32_t d = (int32_t)ux_luma(c) - (int32_t)ux_luma(T_BG);
    return d > 89 || d < -89;
}
/* the state's ink: red (the text's where red would not read), grey, blue (deeper on a light ground), the accent */
static uint16_t mn_ink(uint32_t st)
{
    return st == MN_MUTE ? (mn_far(T_REC) ? T_REC : T_TEXT) : st == MN_OFF ? T_MID :
           st == MN_FROZEN ? (ux.mono ? T_TEXT : ux.light ? RGB(16, 84, 196) : RGB(110, 170, 255)) : T_ACCENT;   /* (the MONO palette is gray whatever) */
}
/* track k's state word: OFF (Round 27), WAIT (turned ON, short of memory), MUTE, FROZEN (Orange Cat 0.5.6) */
static uint32_t mn_state(uint32_t k)
{
    return trk_off(k) ? MN_OFF : ((trk_on_wait >> k) & 1u) ? MN_WAIT : trk[k].p[P_MUTE] ? MN_MUTE : frz_track_frozen(k) ? MN_FROZEN : MN_NONE;
}
static int mn_quiet(uint32_t st) { return st == MN_MUTE || st == MN_OFF || st == MN_FROZEN; }

/* the 16 steps in view of track k: bar b of its pattern (a step past its length is no cell). Bit j of *on: a note, of *tie: a tie */
static void mn_steps_bits(const track_t *t, uint32_t b, uint32_t *on, uint32_t *tie)
{
    uint32_t j, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    *on = *tie = 0;
    for (j = 0; j < 16u && b * 16u + j < len; j++) {
        const step_t *st = &seq_steps(t)[b * 16u + j];
        if (step_on(st)) *on |= 1u << j;
        else if (st->time == ST_TIE) *tie |= 1u << j;
    }
}
static void mn_steps_draw(uint32_t k, uint32_t len16, uint32_t on, uint32_t tie, int32_t ph, int sel, int quiet, int32_t x, int32_t y)
{
    uint32_t j;
    for (j = 0; j < len16; j++) {
        int32_t cx = x + (int32_t)j * 9 + (int32_t)(j / 4u) * 2;
        uint16_t full = quiet ? T_MID : sel ? T_TRK(k) : ux_mix(T_BG, T_TRK(k), 55);
        cv_rect(cx, y, 8, 6, (on >> j) & 1u ? full : (tie >> j) & 1u ? ux_mix(T_LINE, full, 45) : T_LINE);
        if ((int32_t)j == ph) cv_rect(cx, y + 7, 8, 2, T_TEXT);
    }
}
static void mn_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint32_t sel = k == song.sel, st = mn_state(k), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), quiet = (uint32_t)mn_quiet(st);
    uint32_t pl = song.playing && t->seq_idx < len, bar = pl ? t->seq_idx / 16u : 0u, on, tie, g, sig, src, kk;
    int32_t ph = pl ? (int32_t)(t->seq_idx % 16u) : -1, avail, y = MN_Y + (int32_t)k * MN_ROW, len16 = (int32_t)(len - bar * 16u < 16u ? len - bar * 16u : 16u);
    int pending = sel && browse_pending();
    char nm[24], d[2] = {(char)('1' + k), 0};
    uint16_t bg = sel ? ux_mix(T_BG, T_TRK(k), 16) : T_BG, tile = sel ? T_TRK(k) : ux_mix(T_BG, T_TRK(k), 28);
    const aafont_t *f;
    if (pending) {                                      /* browsing: the sound the list shows, not yet loaded */
        char tag[6];
        src = browse_shown(&kk);
        entry_label(src, kk, tag, nm);
        if (!nm[0]) str_cpy(nm, tag, sizeof nm);
    } else {
        sound_name(t, nm);
        if (!nm[0]) str_cpy(nm, ENGINES[eng_idx(t->eng_req)]->name, sizeof nm);
    }
    mn_steps_bits(t, bar, &on, &tie);
    sig = str_hash(1u + sel * 2u + st * 4u + (uint32_t)pending * 64u, nm) + ux.gen * 977u + ux.pal * 31u + (uint32_t)stage_rows() * 7u;
    g = on * 2654435761u + tie * 40503u + (uint32_t)(ph + 1) * 104729u + (uint32_t)len16 * 613u + bar * 31u;
    if (!ui.force && sig == mn.col[k]) {
        if (g != mn.grid[k]) {                          /* the steps or the playhead: the steps only */
            mn.grid[k] = g;
            cv_begin(MN_STEP_W, 10, bg);
            mn_steps_draw(k, (uint32_t)len16, on, tie, ph, (int)sel, (int)quiet, 0, 0);
            cv_blit(MN_STEP_X, (uint32_t)(y + MN_STEP_Y));
        }
        return;
    }
    mn.col[k] = sig;
    mn.grid[k] = g;
    avail = 240 - 40 - 6 - (st ? text_w(TY_LB, mn_word(st)) + 14 + 4 : 0);
    f = ty_fit(TY_TX, nm, avail);
    cv_begin(240, MN_ROW, T_BG);
    cv_rrect(2, 0, 236, MN_ROW - 2, 6, bg, T_BG);
    cv_rrect(5, 2, 26, MN_ROW - 6, 5, tile, bg);
    cv_text_c(18, ty_y(2 + (MN_ROW - 6) / 2, TY_TX), TY_TX, d, sel ? T_INK : T_TRK(k), tile);
    cv_text_fit(38, ty_y(11, f), f, nm, pending ? T_ACCENT : quiet ? T_MID : sel ? T_TEXT : T_SEC, bg, avail);
    if (st) {                                           /* the state word in a pill: dark, its own colour on it */
        const char *w = mn_word(st);
        int32_t pw = text_w(TY_LB, w) + 14;
        cv_rrect(234 - pw, 0, pw, 22, 11, T_BG, bg);
        cv_text_c(234 - pw / 2, ty_y(11, TY_LB), TY_LB, w, mn_ink(st), T_BG);
    }
    mn_steps_draw(k, (uint32_t)len16, on, tie, ph, (int)sel, (int)quiet, MN_STEP_X, MN_STEP_Y);
    cv_blit(0, (uint32_t)y);
}
/* a place not in use: its row greyed, the number and "Add track" (HOME's sheet adds it) */
static void mn_free(uint32_t k)
{
    int32_t y = MN_Y + (int32_t)k * MN_ROW;
    uint16_t bg = ux_mix(T_BG, T_DIM, 8), nc = ux_mix(T_BG, T_DIM, 28);
    char d[2] = {(char)('1' + k), 0};
    uint32_t sig = 0xADDu + ux.gen * 977u + ux.pal * 31u + (uint32_t)stage_rows() * 7u;
    if (!ui.force && sig == mn.col[k])
        return;
    mn.col[k] = sig;
    cv_begin(240, MN_ROW, T_BG);
    cv_rrect(2, 0, 236, MN_ROW - 2, 6, bg, T_BG);
    cv_rrect(5, 2, 26, MN_ROW - 6, 5, nc, bg);
    cv_text_c(18, ty_y(2 + (MN_ROW - 6) / 2, TY_TX), TY_TX, d, T_DIM, nc);
    cv_text_fit(38, ty_y(MN_ROW / 2 - 1, TY_TX), TY_TX, "Add track", T_DIM, bg, 240 - 38 - 6);
    cv_blit(0, (uint32_t)y);
}
/* the dials, their labels and their values */
static void mn_dials(void)
{
    int32_t q[4];
    int mac[4], hot = stage_knobs_out() ? (int)ui.hot_col : -1;
    char v[4][12];
    const char *lab[4];
    uint32_t k, sig = 2166136261u, sv = 2166136261u;
    for (k = 0; k < 4u; k++) {
        q[k] = hp_knob_q8(k, &mac[k]);
        lab[k] = hp_knob_text(k, v[k]);
        sv = hp_hash(sv, v[k], str_len(v[k]));
        if (lab[k]) sv = hp_hash(sv, lab[k], str_len(lab[k]));
    }
    sig = hp_hash(sig, q, sizeof q) + (uint32_t)(hot + 2) * 7919u + ux.gen * 977u + ux.pal * 31u + (uint32_t)(mac[0] + 2 * mac[1] + 4 * mac[2] + 8 * mac[3]);
    sv += (uint32_t)(hot + 2) * 7919u + ux.gen * 977u + ux.pal * 31u;
    if (ui.force || sig != mn.sig_dials || sv != mn.sig_vals) {
        mn.sig_dials = sig;
        mn.sig_vals = sv;
        cv_begin(240, MN_DIAL_H, T_BG);
        for (k = 0; k < 4u; k++) {
            int32_t cx = HP_CELL_X(k) + 28;
            hp_dial(cx, 15, 26, k, q[k], mac[k], hot == (int)k);
            if (lab[k]) {                               /* (a label wider than its cell: its short form, ui_draw.c cell_label; wider still: cut, in free text) */
                const char *l = cell_label(lab[k], 58);
                int32_t w = text_w(TY_LB, l);
                if (w > 58) w = 58;
                cv_free_text(cx - w / 2, ty_y(38, TY_LB), TY_LB, l, T_MID, T_BG, 58);
            }
        }
        cv_blit(0, MN_DIAL_Y);
        cv_begin(240, 240 - (MN_DIAL_Y + MN_DIAL_H), T_BG);
        for (k = 0; k < 4u; k++)
            if (lab[k]) cv_text_c(HP_CELL_X(k) + 28, ty_y(9, TY_LB), TY_LB, v[k], T_TEXT, T_BG);
        cv_blit(0, MN_DIAL_Y + MN_DIAL_H);
    }
}
/* every frame the dials, every other frame the rows */
static void main_draw(void)
{
    uint32_t k, n = stage_rows();
    mn_dials();
    if (!ui.force && (ui.frame & 1u))
        return;
    if (n != mn.rows || ui.force) {                     /* fewer rows than before: the places below them are empty */
        if (n < NTRK)
            lcd_fill(0, (uint32_t)(MN_Y + (int32_t)n * MN_ROW), 240, (uint32_t)((int32_t)(NTRK - n) * MN_ROW), T_BG);
        mn.rows = (uint8_t)n;
    }
    for (k = 0; k < n; k++)
        if (k < trk_n()) mn_row(k);
        else mn_free(k);
}
