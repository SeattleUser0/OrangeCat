/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* What HOME's old Stage left that other screens use (step 9, UI_DESIGN.md "Step 9"): the rows' geometry and the greyed "Add track"
 * row of MIXER (ui_pages.c), the output's waveform (the picture page, the pages' pictures), the knob just turned, and the words of
 * a knob: its label and its value. Included by ui_draw.c where the Stage was; the main screen itself is ui_main.c. */
#define ST_ROW_Y 32                                     /* the rows: 32 .. 205, 2 px apart */
#define ST_ROWS_H 174
#define ST_KNOB_MS 1500u                                /* the knobs stay this long after the last turn */
static int32_t st_row_h(void)
{
    int32_t n = (int32_t)stage_rows();
    return (ST_ROWS_H - 2 * (n - 1)) / n;
}
static int32_t st_row_y(uint32_t k) { return ST_ROW_Y + (int32_t)k * (st_row_h() + 2); }
/* the x of a text centred on cx in at most maxw px */
static int32_t st_mid_x(int32_t cx, const aafont_t *f, const char *s, int32_t maxw)
{
    int32_t w = text_w(f, s);
    return cx - (w < maxw ? w : maxw) / 2;
}
static int pattern_empty(uint32_t k, uint32_t b);       /* ui_patterns.c */
#define ST_BAND_Y 210                                   /* (the chord view's foot: the keys; the knobs drop in over the rows above it) */
#define ST_DROP 4u                                      /* the knobs drop in over this many frames */
static struct {
    uint32_t knob_ms;                                   /* the last knob turn on HOME (| 1), 0 none: the knob just turned is the hot one */
    uint8_t out, drop;                                  /* the chord view's knobs out; frames still dropping */
} stage;
static void view_reset(void);                           /* ui_view.c */
static void stage_knob_touch(void) { stage.knob_ms = fm1_ms | 1u; }   /* (ui_input.c: a knob on HOME) */
static int stage_knobs_out(void) { return stage.knob_ms && fm1_ms - (stage.knob_ms & ~1u) < ST_KNOB_MS; }
/* ------------------------------------------------------- the knobs --- */
/* a knob's label: the panel's, plain words for CZ-1's (its panel says R1, L1 ..) */
static const char *stage_label(uint32_t k, const param_desc_t *d)
{
    static const char *const CZ_L[4] = {"WAVE", "DCW", "DETUNE", "VIBRATO"};
    return stage_pg.scope == SC_CZ1 ? CZ_L[k & 3u] : d->label;
}
/* a knob's value as Stage shows it: the Prophet's 0..127 panel values in % (the mockup's CUTOFF 62 %) */
static void stage_value(const page_t *sp, const param_desc_t *d, int32_t v, char *val, const char **unit)
{
    if ((MELODEE_P5 && sp->scope == SC_P5) && d->fmt == F_INT && d->min == 0 && d->max == 127) {
        fmt_int(val, (v * 100 + 63) / 127);
        *unit = "%";
        return;
    }
    param_format(d, v, val, unit);
}
/* the output's waveform over w px from x 8, smooth: triggered on a rising zero crossing, auto-scaled, each sample
 * the mean of three, 2 px anti-aliased (gfx.c cv_vspan_aa); rows top .. top + h */
static void stage_wave(uint16_t c, int32_t top, int32_t h)
{
    int16_t *snap = scope_snap;
    uint32_t w = scope_w, i, trig = 1;
    int32_t peak = 1500, x, py = 0, cy = (top + h / 2) * 16 - 16;
    for (i = 0; i < SCOPE_N; i++) {
        snap[i] = scope_buf[(w + i) & (SCOPE_N - 1u)];
        if (snap[i] > peak) peak = snap[i];
        else if (-snap[i] > peak) peak = -snap[i];
    }
    for (i = 2; i < SCOPE_N - 240u; i++)
        if (snap[i - 1] < 0 && snap[i] >= 0) { trig = i; break; }
    for (x = 0; x < 224; x++) {
        uint32_t k = trig + (uint32_t)x;
        int32_t v = ((int32_t)snap[k - 1u] + 2 * snap[k] + snap[k + 1u]) / 4;
        int32_t y = cy - v * (h / 2 - 4) * 16 / peak;
        cv_vspan_aa(8 + x, x ? py : y, y, c);
        py = y;
    }
}
/* a place not in use (UNUSED TRACKS SHOW): its row greyed, the numeral and "Add track" (Stage's sheet adds it). Stage
 * and MIXER: *drawn their row's signature */
static void stage_free(uint32_t k, uint32_t *drawn)
{
    int32_t h = st_row_h(), y = st_row_y(k), cs = h >= 80 ? 34 : 26, tx = 6 + cs + 10, top;
    const aafont_t *nf = h >= 80 ? TY_VL : TY_TX;
    uint16_t bg = ux_mix(T_BG, T_DIM, 8), nc = ux_mix(T_BG, T_DIM, 28);
    char d[2] = {(char)('1' + k), 0};
    uint32_t sig = 0xADDu + (uint32_t)h * 211u + (uint32_t)y * 7u + ux.gen * 977u + ux.pal * 31u;
    if ((!ui.force && sig == *drawn))
        return;
    *drawn = sig;
    for (top = 0; top < h; top += (int32_t)(CV_MAX / BOX_W)) {   /* (a tall row: in slices) */
        int32_t sh = h - top < (int32_t)(CV_MAX / BOX_W) ? h - top : (int32_t)(CV_MAX / BOX_W);
        cv_begin(BOX_W, (uint32_t)sh, T_BG);
        cv_oy = -top;
        cv_rrect(0, 0, BOX_W, h, 8, bg, T_BG);
        cv_rrect(6, h / 2 - cs / 2, cs, cs, 7, nc, bg);
        cv_text_c(6 + cs / 2, ty_y(h / 2, cs > 30 ? TY_VL : TY_TX), cs > 30 ? TY_VL : TY_TX, d, T_DIM, nc);
        cv_free_text(tx, ty_y(h / 2, nf), nf, "Add track", T_DIM, bg, BOX_W - 6 - tx);
        cv_oy = 0;
        cv_blit(BOX_X, (uint32_t)(y + top));
    }
}

/* the four cards (while the knobs are out over the chord view): a value without effect on the sound as it is drawn dim */
static void stage_columns(void)
{
    const page_t *sp = stage_page();
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const char *unit;
        char val[12];
        const param_desc_t *d = page_desc(sp, c, &vp);
        int dim = (MELODEE_CZ && sp->scope == SC_CZ1 && !cz_ed_active(cz_patch[song.sel % NTRK].raw, sp->id[c])) ||
                  ((MELODEE_P5 && sp->scope == SC_P5) && !p5_ed_active(TSEL, sp->id[c])) || mod_dim(sp, sp->id[c]);
        if (!d || !vp || !d->label || d->label[0] == '-') {
            draw_column(c, "", "", "", T_THEME, -1, ICON_NONE);
            continue;
        }
        stage_value(sp, d, *vp, val, &unit);
        draw_column(c, stage_label(c, d), val, unit, dim ? T_DIM : VAL(c), d->fmt == F_ENUM && d->max < 2 ? -1 : RATIO(d, *vp),
                    ICON_NONE);
    }
}

/* the knobs over the chord view: out while one turns (and ST_KNOB_MS after), dropping in over ST_DROP frames (each its cells' bottom rows,
 * more each frame); gone at once, the view whole again */
static void stage_knobs(void)
{
    int out = stage_knobs_out();
    if (out && !stage.out) {
        stage.out = 1;
        stage.drop = ST_DROP;
        lcd_fill(0, Y_LABEL, 240, Y_GRAPH - Y_LABEL, T_BG);   /* (the rows under the strip go at once) */
    } else if (!out && stage.out) {
        stage.out = 0;
        view_reset();
        memset(ui.col, 0, sizeof ui.col);
        lcd_fill(0, Y_LABEL, 240, ST_BAND_Y - Y_LABEL, T_BG);
        return;
    }
    if (!stage.out)
        return;
    if (stage.drop) {                                   /* each cell whole (its value snaps, no roll), its bottom rows */
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            ui.col[c][0] = 0;
            ui.roll[c].sig = (uint8_t)~ui.roll[c].sig;
        }
        stage_drop_rows = (uint8_t)((ST_DROP + 1u - stage.drop) * CARD_H / ST_DROP);
        stage_columns();
        stage_drop_rows = 0;
        stage.drop--;
        return;
    }
    stage_columns();
}
