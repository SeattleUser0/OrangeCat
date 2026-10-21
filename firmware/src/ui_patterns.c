/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* SEQ > PATTERNS and SEQ > SONG, screens of their own drawn to the redesign's mockups (docs/design: concept_screens 3
 * and 4, ref/patterns, ref/song).
 * PATTERNS (SEQ held, or its page): the header, then a row per track: its number in a circle (the selected one
 * filled), a cell per pattern: the one playing filled in the track's colour (how far through it at its foot), the one
 * waiting for the bar dashed, the others with something in them as three bars (their thirds' notes), empty ones an
 * outline. At the foot the song as its sections' letters (the rows with the same four patterns share one), the row
 * playing (else the one SONG picked) lit; no song yet: the rows the jam logged. KNOB k queues track k's pattern.
 * OCT+ the selected track's pattern's sheet (Copy to.., Delete pattern). Copy to: the place it goes framed in the
 * theme's colour on the track's row (the first empty one first), any knob moves it, OCT+ copies (over a pattern in
 * use: the question first), OCT- leaves.
 * SONG: its title, its length, the jam at the right (logging while patterns play without a song; REC held: TAKE);
 * the sections as columns (their letters; four at a time around the row picked, one more to add a row), a track's
 * patterns as blocks in its colour, one pattern over sections in a row as one block; the row picked marked, the
 * selected track's block in it outlined; a song playing: its playhead. At the foot the sections and bars, the row's
 * letter and repeats, how far the song has played. KNOB 1 the row, 2 the track, 3 its pattern in the row, 4 the
 * row's repeats; OCT+ plays, REC held takes the jam (ui_input.c).
 * Each part remembers what it drew. Included by ui_draw.c */

static int ptc_on(void);                                /* PATTERNS' Copy to (below) */
static int dq_defer(uint32_t cf, uint32_t k, uint32_t a, uint32_t b);   /* ui_input.c: playing, at the loop end */
static uint32_t ptc_next(uint32_t from, int32_t dir);

/* ------------------------------------------------------------ shared --- */
/* pattern b of track k holds something: a note, a hit, a recorded note's step */
static int pattern_used(uint32_t k, uint32_t b)
{
    pattern_t p;
    pattern_read(k, b, &p);
    uint32_t i;
    for (i = 0; i < NSTEP; i++)
        if (step_on(&p.step[i]) || (p.step[i].flags & SF_RECORDED))
            return 1;
    return 0;
}
/* pattern b of track k holds nothing at all: no note, no hit, no motion (Copy to there asks nothing) */
static int pattern_empty(uint32_t k, uint32_t b)
{
    uint32_t i;
    if (pattern_used(k, b))
        return 0;
    for (i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) == k && motion_pattern[i] == b)
            return 0;
    return 1;
}
/* track k's pattern b: its steps, length and division (the one playing: the track's own; its bank lags behind) */
static void pat_timing(uint32_t k, uint32_t b, uint32_t *len, uint32_t *div)
{
    *len = (uint32_t)pattern_timing(k, b, 0);
    *div = (uint32_t)pattern_timing(k, b, 1);
}
/* its bars, whole ones up (N_DIV: a step of 1/4 .. 4BAR; steps in 4 bars) */
static uint32_t pat_bars(uint32_t k, uint32_t b)
{
    static const uint8_t SPB4[] = {16, 32, 64, 128, 48, 96, 8, 4, 2, 1};
    uint32_t len, div, s;
    pat_timing(k, b, &len, &div);
    s = SPB4[div < NELEM(SPB4) ? div : 2u];
    len = len ? len : 1u;
    return (len * 4u + s - 1u) / s;
}
/* row r's letter: its four patterns' set; A the first set the song plays, B the next new one .. */
static char row_letter(const uint8_t (*pat)[NTRK], uint32_t r)
{
    uint32_t f, i, j, n = 0;
    for (f = 0; f < r && memcmp(pat[f], pat[r], NTRK); f++)
        ;
    for (i = 0; i < f; i++) {
        for (j = 0; j < i && memcmp(pat[j], pat[i], NTRK); j++)
            ;
        n += j == i;
    }
    return (char)('A' + n % 26u);
}
static uint32_t song_hash(const uint8_t (*pat)[NTRK], const uint8_t *rep, uint32_t n)
{
    uint32_t h = 2166136261u, i, k;
    for (i = 0; i < n; i++)
        for (k = 0; k <= NTRK; k++)
            h = (h ^ (k < NTRK ? pat[i][k] : 64u + rep[i])) * 16777619u;
    return h ^ n * 40503u;
}
static uint32_t fnv1(uint32_t h, uint32_t v) { return (h ^ v) * 16777619u; }

/* ---------------------------------------------------------- PATTERNS --- */
#define PT_NUM_Y H_HEAD                                 /* the patterns' numbers: rows 30 .. 49 */
#define PT_ROW_Y 50                                     /* a track a row: 38 px, to 201 */
#define PT_ROW_H 38
#define PT_SONG_Y (PT_ROW_Y + 4 * PT_ROW_H)             /* the song: 202 .. 239 */
#define PT_CELL_X(b) (30 + 26 * (int32_t)(b))           /* a cell: 23 x 32 at row y 2 */
static struct { uint32_t num, row[NTRK], prog[NTRK], song; } ptv;

static uint32_t pt_prog(const track_t *t)               /* px through the pattern playing (of 17), 0 stopped */
{
    uint32_t len = (uint32_t)t->p[P_SLEN];
    return song.playing && len ? (t->seq_idx % len + 1u) * 17u / len : 0u;
}
static void pt_prog_bar(int32_t x, int32_t y, uint32_t prog, uint16_t tc)   /* at the playing cell's foot */
{
    uint16_t tr = ux_mix(tc, T_BG, 35);
    cv_rrect(x, y, 17, 3, 1, tr, tc);
    cv_rrect(x, y, prog < 2u ? 2 : (int32_t)prog, 3, 1, T_BG, tr);
}
/* pattern b's notes as three bars, its thirds (up to 14 px; none: a dot) */
static void pt_bars(uint32_t k, uint32_t b, int32_t x, int32_t y, uint16_t c)
{
    uint32_t len, div, i, n[3] = {0, 0, 0}, m = 1;
    pattern_t p;
    pattern_read(k, b, &p);
    const step_t *s = p.step;
    len = (uint32_t)p.timing[0]; div = (uint32_t)p.timing[1];
    len = len > NSTEP ? NSTEP : len ? len : 1u;
    for (i = 0; i < len; i++)
        if (step_on(&s[i]))
            n[i * 3u / len]++;
    for (i = 0; i < 3u; i++)
        if (n[i] > m) m = n[i];
    for (i = 0; i < 3u; i++) {
        int32_t h = n[i] ? 4 + (int32_t)(n[i] * 10u / m) : 2;
        cv_rrect(x + 5 + 5 * (int32_t)i, y + 26 - h, 3, h, 1, c, T_SURF);
    }
}
/* (Orange Cat 0.3, 4c) the entries shown: A..H, or I..P when the selected track is on one of those */
static uint32_t pt_first(void) { return lists.cur[song.sel % NTRK] % OCL_NENT >= 8u ? 8u : 0u; }
static uint32_t ocl_staged_ent(uint32_t k);
static void pt_numbers(void)
{
    uint32_t b, first = pt_first(), sig = ux.gen * 977u + ux.pal * 31u + 1u + first * 7u;
    if (!ui.force && sig == ptv.num)
        return;
    ptv.num = sig;
    cv_begin(240, PT_ROW_Y - PT_NUM_Y, T_BG);
    for (b = 0; b < 8u; b++) {
        char n[2] = {(char)('A' + first + b), 0};
        cv_text_c(PT_CELL_X(b) + 11, ty_y((PT_ROW_Y - PT_NUM_Y) / 2, TY_LB), TY_LB, n, T_MID, T_BG);
    }
    cv_blit(0, PT_NUM_Y);
}
/* track k's row: its numeral, then its list's entries (the one it is on filled, how far through it at its foot; one
 * staged dashed; the others their pattern's number; empty ones an outline) */
static void pt_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint16_t tc = t->p[P_MUTE] ? T_DIM : T_TRK(k);
    uint32_t b, prog = pt_prog(t), sel = k == song.sel, h, first = pt_first(), cur = lists.cur[k] % OCL_NENT;
    uint32_t staged = ocl_staged_ent(k);
    int32_t y0 = PT_ROW_Y + (int32_t)k * PT_ROW_H;
    char n[3] = {(char)('1' + k), 0, 0};
    h = fnv1(fnv1(2166136261u, cur + 16u * staged + 256u * first + 8192u * sel), 16384u * (uint32_t)(t->p[P_MUTE] != 0));
    for (b = 0; b < 8u; b++)
        h = fnv1(h, lists.ent[k][first + b][0]);
    h = fnv1(h, ux.gen * 977u + ux.pal * 31u + (uint32_t)song.playing);
    if (!ui.force && h == ptv.row[k]) {
        if (prog != ptv.prog[k] && cur >= first && cur < first + 8u) {   /* playing: the bar at the cell's foot only */
            ptv.prog[k] = prog;
            cv_begin(17, 3, tc);
            pt_prog_bar(0, 0, prog, tc);
            cv_blit((uint32_t)(PT_CELL_X(cur - first) + 3), (uint32_t)(y0 + 2 + 25));
        }
        return;
    }
    ptv.row[k] = h;
    ptv.prog[k] = prog;
    cv_begin(240, PT_ROW_H, T_BG);
    cv_rrect(2, 2, 24, 32, 6, tc, T_BG);                /* the track: its numeral in a chip, the selected one filled */
    if (!sel)
        cv_rrect(4, 4, 20, 28, 4, T_BG, tc);
    cv_text_c(14, ty_y(18, TY_TX), TY_TX, n, sel ? T_INK : tc, sel ? tc : T_BG);
    for (b = 0; b < 8u; b++) {
        uint32_t e = first + b, v = lists.ent[k][e][0];
        int32_t x = PT_CELL_X(b), y = 2;
        n[0] = (char)('0' + v / 10u); n[1] = (char)('0' + v % 10u);
        if (v && e == cur) {                            /* playing / on: filled, how far through it */
            cv_rrect(x, y, 23, 32, 4, tc, T_BG);
            cv_text_c(x + 11, ty_y(y + 13, TY_LB), TY_LB, n, T_INK, tc);
            if (prog)
                pt_prog_bar(x + 3, y + 25, prog, tc);
        } else if (v) {
            cv_rrect(x, y, 23, 32, 4, T_SURF, T_BG);
            cv_text_c(x + 11, ty_y(y + 13, TY_LB), TY_LB, n, ux_mix(T_SURF, tc, 70), T_SURF);
        } else {                                        /* empty: an outline */
            cv_rrect(x, y, 23, 32, 4, T_LINE, T_BG);
            cv_rrect(x + 1, y + 1, 21, 30, 3, T_BG, T_LINE);
        }
        if (e == staged)                                /* waiting for its loop end: dashed */
            cv_dashed(x, y, 23, 32, 4, tc);
    }
    cv_blit(0, (uint32_t)y0);
}
/* (Orange Cat 0.3, 4c) the foot: the selected track's entry and its pattern, SONG on or off */
static void pt_song(void)
{
    char b[32];
    uint32_t k = song.sel % NTRK, e = lists.cur[k] % OCL_NENT, v = lists.ent[k][e][0];
    uint32_t sig = k * 7u + e * 131u + v * 1031u + (uint32_t)lists.song * 40503u + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == ptv.song)
        return;
    ptv.song = sig;
    cv_begin(240, 240 - PT_SONG_Y, T_BG);
    b[0] = 'T'; b[1] = (char)('1' + k); b[2] = ' '; b[3] = (char)('A' + e); b[4] = 0;
    if (v) {
        str_cpy(b + 4, " \xb7 ", 4);
        b[7] = (char)('0' + v / 10u); b[8] = (char)('0' + v % 10u); b[9] = 0;
    }
    cv_text_on(8, ty_y(19, TY_LB), TY_LB, b, T_MID, T_BG);
    cv_text_r(232, ty_y(19, TY_LB), TY_LB, lists.song ? "SONG ON" : "SONG OFF", lists.song ? T_TEXT : T_DIM, T_BG);
    cv_blit(0, PT_SONG_Y);
}
static void patterns_draw(void)
{
    uint32_t k;
    pt_numbers();
    pt_song();
    for (k = 0; k < trk_n(); k++)
        pt_row(k);
    if (ui.force && trk_n() < NTRK)                     /* (the rows of tracks not in use: empty) */
        lcd_fill(0, PT_ROW_Y + (int32_t)trk_n() * PT_ROW_H, 240, (NTRK - trk_n()) * PT_ROW_H, T_BG);
}
/* a pattern queued on any track (PATTERNS' OCT- clears them before it leaves) */
static int patterns_queued(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (trk[k].pattern_next < NPAT)
            return 1;
    return 0;
}
/* KNOB k: track k's next pattern (stopped: at once; playing: at the end of its bar; a song plays: refused); Copy to:
 * any knob the place */
static int ocl_launch(uint32_t k, uint32_t e);
static void patgrid_edit(uint32_t k, int32_t steps)
{
    /* (Orange Cat 0.3, 4c) KNOB k: track k to its next (or previous) entry naming a pattern (stopped: at once;
     * playing: at its loop end) */
    uint32_t e, i, from;
    int rc;
    if (k >= trk_n())                                   /* (no track there) */
        return;
    from = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_ent(k) : lists.cur[k] % OCL_NENT;
    for (i = 1, e = OCL_NENT; i < OCL_NENT; i++) {
        uint32_t c = (from + (steps > 0 ? i : OCL_NENT - i)) % OCL_NENT;
        if (lists.ent[k][c][0]) { e = c; break; }
    }
    if (e == OCL_NENT) { ui_message("NO OTHER ENTRY"); return; }
    rc = ocl_launch(k, e);
    if (rc) ui_message(rc == 1 ? "EMPTY ENTRY" : rc == 4 ? (song.playing ? "STOP TO KEEP EDITS" : "PATTERNS FULL") :
                       rc == 5 ? "PATTERN DATA FULL" : "STOP FIRST");
    ui.force = 1;
}

/* ----------------------------------------------- PATTERNS: Copy to --- */
static void confirm_open(uint32_t kind, uint32_t trk);  /* ui_input.c */
static int ptc_on(void) { return ui.ptc_on && !ui.home && cur_page()->graph == GR_PATGRID && ui.ptc_trk == song.sel; }
static uint32_t ptc_next(uint32_t from, int32_t dir)    /* the next place that is not the pattern copied (ends kept) */
{
    int32_t b = (int32_t)from + dir;
    if (b == (int32_t)ui.ptc_src)
        b += dir;
    return b < 0 || b >= (int32_t)NPAT ? from : (uint32_t)b;
}
static void ptc_start(void)                             /* the pattern sheet's Copy to: the selected track's pattern */
{
    uint32_t k = song.sel % NTRK, src = trk[k].pattern, i, b = (src + 1u) % NPAT;
    if (chain_busy()) {
        ui_message("STOP SONG TO COPY");
        return;
    }
    if (pattern_empty(k, src)) {
        ui_message("NOTHING TO COPY");
        return;
    }
    for (i = 1; i < NPAT && !pattern_empty(k, (src + i) % NPAT); i++)   /* the first empty place after it */
        ;
    if (i < NPAT)
        b = (src + i) % NPAT;
    ui.ptc_on = 1;
    ui.ptc_trk = (uint8_t)k;
    ui.ptc_src = (uint8_t)src;
    ui.ptc_dst = (uint8_t)b;
    ui.force = 1;
}
static void ptc_paste(int asked)                        /* OCT+: copy it there (over one in use: asked first;
                                                         * that or the one playing, playing: at its loop end) */
{
    uint32_t k = ui.ptc_trk % NTRK;
    int rc;
    if (!asked && !pattern_empty(k, ui.ptc_dst)) {
        confirm_open(CF_PASTE_PAT, k);
        return;
    }
    if (ui.ptc_src != ui.ptc_dst && (ui.ptc_dst == trk[k].pattern || !pattern_empty(k, ui.ptc_dst)) &&
        dq_defer(CF_PASTE_PAT, k, ui.ptc_dst, ui.ptc_src)) {
        ui.ptc_on = 0;
        return;
    }
    rc = pattern_copy(&trk[k], ui.ptc_src, ui.ptc_dst);
    if (!rc)
        ui.ptc_on = 0;
    ui_message(!rc ? "PATTERN COPIED" : rc == 2 ? "PATTERN DATA FULL" : chain_busy() ? "STOP SONG TO COPY" :
               "PATTERN IS PLAYING");
    ui.force = 1;
}

/* -------------------------------------------------------------- SONG --- */
/* (Orange Cat 0.3, build doc 4c) SONG is Skiff's lists (oc_lists.c): a column per entry, A..P (four at a time around
 * the one picked), a row per track; a cell, the pool pattern the entry names (its number, its REPS under it), the
 * entry the track is on filled in the track's colour, one staged to come in dashed, the cell picked (the selected
 * track's, in the entry picked) outlined. The title: SONG, ON while the song plays. The foot: the cell picked, and the
 * keys. KNOB 1 the entry, 2 the track, 3 its pattern, 4 its REPS; OCT+ goes to it (playing: at the loop end); PLAY here
 * plays the song (each track down its list), PLAY on any other page loops each track's entry. */
#define SG_TOP_H H_HEAD                                 /* the title: rows 0 .. 29 */
#define SG_LAB_H 22                                     /* the entries' letters: 30 .. 51 */
#define SG_ROW_Y (SG_TOP_H + SG_LAB_H)                  /* a track a row: 36 px, 52 .. 195 */
#define SG_ROW_H 36
#define SG_FOOT_Y (SG_ROW_Y + 4 * SG_ROW_H)             /* the foot: 196 .. 239 */
#define SG_COL_W 56                                     /* an entry: x 8 + 56 j, four shown */
#define SG_COLS 4u
static struct { uint32_t top, lab, row[NTRK], foot; } sgv;
static struct { uint32_t first, sel; } sg;              /* this frame's */
static uint32_t ocl_pat_len(uint32_t id);               /* oc_lists.c */
static uint32_t ocl_staged_ent(uint32_t k);

static void sg_frame(void)
{
    sg.sel = ui.song_row % OCL_NENT;
    sg.first = sg.sel > 1u ? sg.sel - 1u : 0u;
    if (sg.first > OCL_NENT - SG_COLS)
        sg.first = OCL_NENT - SG_COLS;
}
static void sg_lines(int32_t y, int32_t h)              /* the entries' rules, rows y .. y + h */
{
    for (uint32_t j = 0; j < SG_COLS; j++)
        cv_rect(8 + SG_COL_W * (int32_t)j, y, 1, h, T_LINE);
}
static int sg_song_on(void) { return lists.song && song.playing; }
static void sg_head(void)
{
    if (head_waiting()) { draw_head(); return; }
    uint32_t sig = (uint32_t)sg_song_on() * 3u + ux.gen * 977u + ux.pal * 31u + (ui.msg_t ? str_hash(3u, ui.msg) : 0u);
    if (!ui.force && sig == sgv.top)
        return;
    sgv.top = sig;
    cv_begin(240, SG_TOP_H, T_BG);
    cv_text_r(236, ty_y(SG_TOP_H / 2, TY_TX), TY_TX, "SONG", T_THEME, T_BG);
    if (ui.msg_t) {
        cv_free_text(8, ty_y(SG_TOP_H / 2, ty_fit(TY_TX, ui.msg, 166)), ty_fit(TY_TX, ui.msg, 166), ui.msg, T_ACCENT, T_BG,
                     166);
    } else if (sg_song_on()) {
        cv_rrect(8, 6, 34, 18, 5, T_THEME, T_BG);
        cv_text_c(25, ty_y(15, TY_LB), TY_LB, "ON", T_INK, T_THEME);
    }
    cv_blit(0, 0);
}
static void sg_labels(void)                             /* the entries' letters, the one picked marked */
{
    uint32_t j, sig = sg.first * 7u + sg.sel * 1031u + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == sgv.lab)
        return;
    sgv.lab = sig;
    cv_begin(240, SG_LAB_H, T_BG);
    sg_lines(2, SG_LAB_H - 2);
    for (j = 0; j < SG_COLS; j++) {
        uint32_t e = sg.first + j;
        char l[2] = {(char)('A' + e), 0};
        cv_text_on(11 + SG_COL_W * (int32_t)j, ty_y(10, TY_LB), TY_LB, l, e == sg.sel ? T_TEXT : T_MID, T_BG);
        if (e == sg.sel)
            cv_rrect(11 + SG_COL_W * (int32_t)j, 19, 14, 3, 1, T_THEME, T_BG);
    }
    cv_blit(0, SG_TOP_H);
}
/* "07" (and "x2" when REPS > 1) into b */
static void sg_cell_text(uint32_t k, uint32_t e, char *b)
{
    uint32_t v = lists.ent[k][e][0], r = lists.ent[k][e][1] + 1u;
    b[0] = (char)('0' + v / 10u); b[1] = (char)('0' + v % 10u); b[2] = 0;
    if (r > 1u) {
        str_cpy(b + 2, " x", 3);
        fmt_int(b + 4, (int32_t)r);
    }
}
static void sg_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint16_t tc = t->p[P_MUTE] ? T_DIM : T_TRK(k);
    uint32_t live = k < trk_n(), j, staged = ocl_staged_ent(k);
    uint32_t h = fnv1(2166136261u, sg.first + 16u * sg.sel + 4096u * (k == song.sel) + 65536u * (uint32_t)(t->p[P_MUTE] != 0) +
                                  131072u * live + 262144u * lists.cur[k] + 8388608u * staged);
    h = fnv1(h, ux.gen * 977u + ux.pal * 31u);
    for (j = 0; j < SG_COLS; j++)
        h = fnv1(h, lists.ent[k][sg.first + j][0] + 256u * lists.ent[k][sg.first + j][1]);
    if (!ui.force && h == sgv.row[k])
        return;
    sgv.row[k] = h;
    cv_begin(240, SG_ROW_H, T_BG);
    sg_lines(0, SG_ROW_H);
    for (j = 0; live && j < SG_COLS; j++) {
        uint32_t e = sg.first + j, pick = k == song.sel && e == sg.sel, on = e == lists.cur[k] % OCL_NENT;
        int32_t x = 9 + SG_COL_W * (int32_t)j, w = SG_COL_W - 2;
        char l[8];
        if (!lists.ent[k][e][0]) {
            if (pick)
                cv_dashed(x, 2, w, 28, 5, T_MID);
            continue;
        }
        sg_cell_text(k, e, l);
        if (pick)
            cv_rrect(x - 1, 1, w + 2, 30, 6, T_TEXT, T_BG);
        if (on) {
            cv_rrect(x, 2, w, 28, 5, tc, pick ? T_TEXT : T_BG);
            cv_text_on(x + 6, ty_y(16, TY_LB), TY_LB, l, T_INK, tc);
        } else {
            cv_rrect(x, 2, w, 28, 5, T_SURF, pick ? T_TEXT : T_BG);
            cv_text_on(x + 6, ty_y(16, TY_LB), TY_LB, l, tc, T_SURF);
        }
        if (e == staged)
            cv_dashed(x + 2, 4, w - 4, 24, 4, tc);
    }
    cv_blit(0, (uint32_t)(SG_ROW_Y + (int32_t)k * SG_ROW_H));
}
static void sg_foot(void)                               /* the cell picked: track, entry, pattern, REPS */
{
    char b[40];
    uint32_t k = song.sel % NTRK, e = sg.sel, v = lists.ent[k][e][0];
    uint32_t sig = k * 7u + e * 131u + v * 1031u + lists.ent[k][e][1] * 40503u + ux.gen * 977u + ux.pal * 31u +
                   (uint32_t)lists.song * 613u;
    if (!ui.force && sig == sgv.foot)
        return;
    sgv.foot = sig;
    cv_begin(240, 240 - SG_FOOT_Y, T_BG);
    sg_lines(0, 4);
    b[0] = 'T'; b[1] = (char)('1' + k); b[2] = ' '; b[3] = (char)('A' + e); b[4] = 0;
    if (v) {
        str_cpy(b + 4, " \xb7 pattern ", 12);
        sg_cell_text(k, e, b + str_len(b));
        if (v <= OCL_NPOOL && !ocl_pat_len(v - 1u))
            str_cpy(b + str_len(b), " (empty)", 9);
    } else {
        str_cpy(b + 4, " \xb7 no pattern", 14);
    }
    cv_text_on(8, ty_y(13, TY_LB), TY_LB, b, T_TEXT, T_BG);
    cv_text_on(8, ty_y(33, TY_LB), TY_LB, "OCT+ go \xb7 PLAY: song", T_MID, T_BG);
    cv_blit(0, SG_FOOT_Y);
}
static void song_draw(void)
{
    uint32_t k;
    sg_frame();
    sg_head();
    sg_foot();
    sg_labels();
    for (k = 0; k < NTRK; k++)
        sg_row(k);
}
