/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* SEQ > CHAIN (was PATTERNS) and SEQ > SONG, screens of their own.
 * Melodee 1.4.5 drew them to the redesign's mockups (docs/design: concept_screens 3 and 4): PATTERNS a track's 8 banks,
 * SONG its sections. Orange Cat 0.3 (build doc 4c) made them Skiff's lists; 0.4 made them a tracker's levels (M8,
 * LSDJ): CHAIN shows the chain each track is on, SONG the song's rows of chains (below, at each).
 * Revisions:
 *   0.3: PATTERNS the tracks' list entries, SONG the list editor.
 *   0.4.0: CHAIN (the chains, its knobs: step, track, phrase, REPS) and SONG (the rows, a column per track, its knobs:
 *          row, track, chain). The bank Copy to (ptc_*) is kept for the pattern dialog's paste.
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
#define PT_ROW_Y 50                                     /* a track a row: 36 px, to 193 */
#define PT_ROW_H 36
#define PT_SONG_Y (PT_ROW_Y + 4 * PT_ROW_H)             /* the foot: 194 .. 239 */
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
/* (Orange Cat 0.4, build doc 4c) CHAIN: a row per track, the chain it is on (its number in the track's chip, the
 * selected track's filled); eight of the chain's sixteen steps, A..H or I..P (the half the step picked is in), each
 * the phrase it names: the step the track is on filled in its colour (how far through it at its foot), one staged to
 * come in dashed, the others their phrase's number, empty ones an outline; the step picked (the selected track's)
 * framed. The foot: the step picked (its phrase, its REPS), what PLAY plays. KNOB 1 the step, 2 the track, 3 the
 * step's phrase (-- none, 01..64), 4 its REPS (x1..x16); OCT+ the track to that step (playing: at its loop end), OCT+
 * held its sheet; PLAY here plays the chains (ui_input.c) */
static uint32_t pt_first(void) { return ui.chain_step % OCL_NENT >= 8u ? 8u : 0u; }
static uint32_t ocl_staged_ent(uint32_t k);
static uint32_t ocl_staged_chain_of(uint32_t k);
static uint32_t ocl_pat_len(uint32_t id);
static void pt_numbers(void)                            /* the steps' letters, the one picked marked */
{
    uint32_t b, first = pt_first(), pick = ui.chain_step % OCL_NENT;
    uint32_t sig = ux.gen * 977u + ux.pal * 31u + 1u + first * 7u + pick * 131u;
    if (!ui.force && sig == ptv.num)
        return;
    ptv.num = sig;
    cv_begin(240, PT_ROW_Y - PT_NUM_Y, T_BG);
    for (b = 0; b < 8u; b++) {
        char n[2] = {(char)('A' + first + b), 0};
        cv_text_c(PT_CELL_X(b) + 11, ty_y(9, TY_LB), TY_LB, n, first + b == pick ? T_TEXT : T_MID, T_BG);
        if (first + b == pick)
            cv_rrect(PT_CELL_X(b) + 5, 17, 13, 3, 1, T_THEME, T_BG);
    }
    cv_blit(0, PT_NUM_Y);
}
/* "07" (and " x2" when REPS > 1) of step e of track k's chain into b */
static void pt_cell_text(uint32_t k, uint32_t e, char *b)
{
    uint32_t v = OCL_ENT(k, e)[0], r = OCL_ENT(k, e)[1] + 1u;
    b[0] = (char)('0' + v / 10u); b[1] = (char)('0' + v % 10u); b[2] = 0;
    if (r > 1u) {
        str_cpy(b + 2, " x", 3);
        fmt_int(b + 4, (int32_t)r);
    }
}
static void pt_row(uint32_t k)
{
    const track_t *t = &trk[k];
    uint16_t tc = t->p[P_MUTE] ? T_DIM : T_TRK(k);
    uint32_t b, prog = pt_prog(t), sel = k == song.sel, h, first = pt_first(), cur = lists.cur[k] % OCL_NENT;
    uint32_t staged = ocl_staged_chain_of(k) == lists.tchain[k] ? ocl_staged_ent(k) : OCL_NENT;
    uint32_t pick = sel ? ui.chain_step % OCL_NENT : OCL_NENT, c = lists.tchain[k] % OCL_NCHAIN + 1u;
    int32_t y0 = PT_ROW_Y + (int32_t)k * PT_ROW_H;
    char n[3] = {(char)('0' + c / 10u), (char)('0' + c % 10u), 0};
    h = fnv1(fnv1(2166136261u, cur + 16u * staged + 256u * first + 8192u * sel), 16384u * (uint32_t)(t->p[P_MUTE] != 0));
    h = fnv1(fnv1(h, pick + 32u * c), ux.gen * 977u + ux.pal * 31u + (uint32_t)song.playing);
    for (b = 0; b < 8u; b++)
        h = fnv1(h, OCL_ENT(k, first + b)[0]);
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
    cv_rrect(2, 2, 24, 32, 6, tc, T_BG);                /* the chain: its number in a chip, the selected track's filled */
    if (!sel)
        cv_rrect(4, 4, 20, 28, 4, T_BG, tc);
    cv_text_c(14, ty_y(18, TY_LB), TY_LB, n, sel ? T_INK : tc, sel ? tc : T_BG);
    for (b = 0; b < 8u; b++) {
        uint32_t e = first + b, v = OCL_ENT(k, e)[0];
        int32_t x = PT_CELL_X(b), y = 2;
        n[0] = (char)('0' + v / 10u); n[1] = (char)('0' + v % 10u);
        if (e == pick)                                  /* the step picked: framed */
            cv_rrect(x - 2, y - 2, 27, 36, 5, T_TEXT, T_BG);
        if (v && e == cur) {                            /* the track on it: filled, how far through it */
            cv_rrect(x, y, 23, 32, 4, tc, e == pick ? T_TEXT : T_BG);
            cv_text_c(x + 11, ty_y(y + 13, TY_LB), TY_LB, n, T_INK, tc);
            if (prog)
                pt_prog_bar(x + 3, y + 25, prog, tc);
        } else if (v) {
            cv_rrect(x, y, 23, 32, 4, T_SURF, e == pick ? T_TEXT : T_BG);
            cv_text_c(x + 11, ty_y(y + 13, TY_LB), TY_LB, n, ux_mix(T_SURF, tc, 70), T_SURF);
        } else {                                        /* empty: an outline */
            cv_rrect(x, y, 23, 32, 4, T_LINE, e == pick ? T_TEXT : T_BG);
            cv_rrect(x + 1, y + 1, 21, 30, 3, T_BG, T_LINE);
        }
        if (e == staged)                                /* waiting for its loop end: dashed */
            cv_dashed(x, y, 23, 32, 4, tc);
    }
    cv_blit(0, (uint32_t)y0);
}
/* what PLAY plays (playing: what it does) */
static const char *pt_mode_text(int here_chain)
{
    if (song.playing)
        return lists.mode == OCL_SONG ? "PLAYING THE SONG" : lists.mode == OCL_CHAIN ? "PLAYING THE CHAINS" :
               "LOOPING THE PHRASES";
    return here_chain ? "PLAY: THE CHAINS" : "PLAY: THE SONG";
}
/* the foot: the step picked, its phrase and REPS; what PLAY plays */
static void pt_song(void)
{
    char b[40];
    uint32_t k = song.sel % NTRK, e = ui.chain_step % OCL_NENT, v = OCL_ENT(k, e)[0], c = lists.tchain[k] % OCL_NCHAIN;
    uint32_t sig = k * 7u + e * 131u + v * 1031u + OCL_ENT(k, e)[1] * 40503u + c * 613u + ux.gen * 977u + ux.pal * 31u +
                   (uint32_t)song.playing * 5u + lists.mode * 11u;
    if (!ui.force && sig == ptv.song)
        return;
    ptv.song = sig;
    cv_begin(240, 240 - PT_SONG_Y, T_BG);
    str_cpy(b, "CHAIN 01 \xb7 STEP A \xb7 ", sizeof b);
    b[6] = (char)('0' + (c + 1u) / 10u); b[7] = (char)('0' + (c + 1u) % 10u);
    b[16] = (char)('A' + e);
    if (v) {
        pt_cell_text(k, e, b + 20);
        if (v <= OCL_NPOOL && !ocl_pat_len(v - 1u) && OCL_ENT(k, e)[1] == 0u)
            str_cpy(b + str_len(b), " NEW", 5);    /* (nothing in that phrase yet) */
    } else {
        str_cpy(b + 20, "--", 3);
    }
    cv_text_on(8, ty_y(13, TY_LB), TY_LB, b, T_TEXT, T_BG);
    cv_text_on(8, ty_y(33, TY_LB), TY_LB, pt_mode_text(1), T_MID, T_BG);
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
/* a pattern queued on any track (CHAIN's OCT- clears them before it leaves) */
static int patterns_queued(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (trk[k].pattern_next < NPAT)
            return 1;
    return 0;
}
/* CHAIN's knobs: 1 the step, 2 the track, 3 the step's phrase, 4 its REPS */
static void patgrid_edit(uint32_t slot, int32_t steps)
{
    uint32_t k = song.sel % NTRK, e = ui.chain_step % OCL_NENT;
    if (slot == 0u) {
        ui.chain_step = (uint8_t)clamp((int32_t)e + steps, 0, OCL_NENT - 1u);
        return;
    }
    if (slot == 1u) {
        track_select((uint32_t)clamp((int32_t)song.sel + (steps > 0 ? 1 : -1), 0, (int32_t)trk_n() - 1));
        return;
    }
    if (slot == 2u)
        OCL_ENT(k, e)[0] = (uint8_t)clamp((int32_t)OCL_ENT(k, e)[0] + steps, 0, OCL_NPOOL);
    else if (OCL_ENT(k, e)[0])
        OCL_ENT(k, e)[1] = (uint8_t)clamp((int32_t)OCL_ENT(k, e)[1] + steps, 0, 15);
}
/* (the hold-select's PATTERN) track k one step of its chain right or left, A up to one past its last phrase (an
 * empty step there: a new empty phrase, ready to record). Stopped: at once; playing: at its loop end */
static int ocl_launch(uint32_t k, uint32_t e);
static int ocl_go_empty(uint32_t k, uint32_t e);
static void pt_step_go(uint32_t k, int32_t steps)
{
    uint32_t from, last, e;
    int rc;
    char m[16];
    if (k >= trk_n())                                   /* (no track there) */
        return;
    from = ocl_staged_ent(k) < OCL_NENT ? ocl_staged_ent(k) : lists.cur[k] % OCL_NENT;
    for (last = OCL_NENT; last > 0u && !OCL_ENT(k, last - 1u)[0]; last--)
        ;
    if (last >= OCL_NENT) last = OCL_NENT - 1u;         /* (the furthest it goes: one past the last phrase) */
    e = (uint32_t)clamp((int32_t)from + (steps > 0 ? 1 : -1), 0, (int32_t)(last > from ? last : from));
    if (e == from) return;
    rc = OCL_ENT(k, e)[0] ? ocl_launch(k, e) : ocl_go_empty(k, e);
    if (rc) {
        ui_message(rc == 1 ? "EMPTY STEP" : rc == 4 ? (song.playing ? "STOP TO KEEP EDITS" : "PHRASES FULL") :
                   rc == 5 ? "PATTERN DATA FULL" : "STOP FIRST");
    } else {
        str_cpy(m, "T1 STEP A", sizeof m);
        m[1] = (char)('1' + k); m[8] = (char)('A' + e);
        if (!OCL_ENT(k, e)[0]) str_cpy(m + 9, " NEW", 5);
        ui_message(m);
    }
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
/* (Orange Cat 0.4, build doc 4c) SONG: the song's rows, 01..64, six at a time around the one picked; a column per
 * track, each cell the chain it plays in that row (its number), empty cells blank. Playing the song, the row each track
 * is in filled in the track's colour, one it goes to next dashed; the cell picked (the selected track's, in the row
 * picked) framed. The foot: the cell picked, the keys. KNOB 1 the row, 2 the track, 3 the cell's chain (-- none,
 * 01..64); OCT+ the track to that chain and CHAIN (an empty cell: a new chain there first); PLAY here plays the song
 * from the row picked, each track down its own column at its own pace (ui_input.c) */
#define SG_TOP_H H_HEAD                                 /* the title: rows 0 .. 29 */
#define SG_LAB_H 20                                     /* the tracks' numbers: 30 .. 49 */
#define SG_ROW_Y (SG_TOP_H + SG_LAB_H)                  /* a song row: 24 px, six, 50 .. 193 */
#define SG_ROW_H 24
#define SG_ROWS 6u
#define SG_FOOT_Y (SG_ROW_Y + (int32_t)SG_ROWS * SG_ROW_H)   /* the foot: 194 .. 239 */
#define SG_CELL_X(k) (40 + 50 * (int32_t)(k))           /* a cell: 46 x 20 */
static struct { uint32_t top, lab, row[SG_ROWS], foot; } sgv;
static struct { uint32_t first, sel; } sg;              /* this frame's */
static uint32_t ocl_staged_row_of(uint32_t k);          /* oc_lists.c */

static void sg_frame(void)
{
    sg.sel = ui.song_row % OCL_NROW;
    sg.first = sg.sel > 2u ? sg.sel - 2u : 0u;
    if (sg.first > OCL_NROW - SG_ROWS)
        sg.first = OCL_NROW - SG_ROWS;
}
static int sg_song_on(void) { return lists.mode == OCL_SONG && song.playing; }
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
static void sg_labels(void)                             /* the tracks' numbers, the selected one marked */
{
    uint32_t k, sig = song.sel * 7u + trk_n() * 131u + ux.gen * 977u + ux.pal * 31u;
    for (k = 0; k < NTRK; k++) sig = fnv1(sig, (uint32_t)(trk[k].p[P_MUTE] != 0));
    if (!ui.force && sig == sgv.lab)
        return;
    sgv.lab = sig;
    cv_begin(240, SG_LAB_H, T_BG);
    for (k = 0; k < trk_n(); k++) {
        char l[2] = {(char)('1' + k), 0};
        uint16_t tc = trk[k].p[P_MUTE] ? T_DIM : T_TRK(k);
        cv_text_c(SG_CELL_X(k) + 23, ty_y(9, TY_LB), TY_LB, l, k == song.sel ? T_TEXT : tc, T_BG);
        if (k == song.sel)
            cv_rrect(SG_CELL_X(k) + 16, 17, 14, 3, 1, tc, T_BG);
    }
    cv_blit(0, SG_TOP_H);
}
static void sg_row(uint32_t j)
{
    uint32_t r = sg.first + j, k, on, h = fnv1(2166136261u, r + 64u * (r == sg.sel) + 128u * song.sel + 1024u * trk_n());
    char l[4];
    h = fnv1(fnv1(h, ux.gen * 977u + ux.pal * 31u), (uint32_t)sg_song_on());
    for (k = 0; k < NTRK; k++)
        h = fnv1(h, lists.song[r][k] + 256u * (lists.row[k] == r) + 512u * (ocl_staged_row_of(k) == r) +
                        1024u * (uint32_t)(trk[k].p[P_MUTE] != 0));
    if (!ui.force && h == sgv.row[j])
        return;
    sgv.row[j] = h;
    cv_begin(240, SG_ROW_H, T_BG);
    l[0] = (char)('0' + (r + 1u) / 10u); l[1] = (char)('0' + (r + 1u) % 10u); l[2] = 0;
    cv_text_on(8, ty_y(12, TY_LB), TY_LB, l, r == sg.sel ? T_TEXT : T_MID, T_BG);
    for (k = 0; k < trk_n(); k++) {
        uint16_t tc = trk[k].p[P_MUTE] ? T_DIM : T_TRK(k);
        uint32_t v = lists.song[r][k], pick = k == song.sel && r == sg.sel;
        int32_t x = SG_CELL_X(k);
        on = sg_song_on() && lists.row[k] == r;
        if (!v) {
            if (pick)
                cv_dashed(x, 2, 46, 20, 4, T_MID);
            continue;
        }
        l[0] = (char)('0' + v / 10u); l[1] = (char)('0' + v % 10u);
        if (pick)
            cv_rrect(x - 2, 0, 50, 24, 5, T_TEXT, T_BG);
        if (on) {
            cv_rrect(x, 2, 46, 20, 4, tc, pick ? T_TEXT : T_BG);
            cv_text_c(x + 23, ty_y(12, TY_LB), TY_LB, l, T_INK, tc);
        } else {
            cv_rrect(x, 2, 46, 20, 4, T_SURF, pick ? T_TEXT : T_BG);
            cv_text_c(x + 23, ty_y(12, TY_LB), TY_LB, l, tc, T_SURF);
        }
        if (ocl_staged_row_of(k) == r && !on)
            cv_dashed(x + 2, 4, 42, 16, 3, tc);
    }
    cv_blit(0, (uint32_t)(SG_ROW_Y + (int32_t)j * SG_ROW_H));
}
static void sg_foot(void)                               /* the cell picked: row, track, chain; the keys */
{
    char b[40];
    uint32_t k = song.sel % NTRK, r = sg.sel, v = lists.song[r][k];
    uint32_t sig = k * 7u + r * 131u + v * 1031u + ux.gen * 977u + ux.pal * 31u + (uint32_t)song.playing * 5u +
                   lists.mode * 11u;
    if (!ui.force && sig == sgv.foot)
        return;
    sgv.foot = sig;
    cv_begin(240, 240 - SG_FOOT_Y, T_BG);
    str_cpy(b, "ROW 01 \xb7 T1 \xb7 ", sizeof b);
    b[4] = (char)('0' + (r + 1u) / 10u); b[5] = (char)('0' + (r + 1u) % 10u); b[10] = (char)('1' + k);
    if (v) {
        str_cpy(b + 14, "CHAIN 01", 9);
        b[20] = (char)('0' + v / 10u); b[21] = (char)('0' + v % 10u);
    } else {
        str_cpy(b + 14, "NO CHAIN", 9);
    }
    cv_text_on(8, ty_y(13, TY_LB), TY_LB, b, T_TEXT, T_BG);
    cv_text_on(8, ty_y(33, TY_LB), TY_LB, song.playing ? pt_mode_text(0) : "OCT+ CHAIN \xb7 PLAY: SONG", T_MID,
               T_BG);
    cv_blit(0, SG_FOOT_Y);
}
static void song_draw(void)
{
    uint32_t j;
    sg_frame();
    sg_head();
    sg_foot();
    sg_labels();
    for (j = 0; j < SG_ROWS; j++)
        sg_row(j);
}
/* SONG's knobs: 1 the row, 2 the track, 3 the cell's chain (-- none, 01..64) */
static void song_edit(uint32_t slot, int32_t steps)
{
    uint32_t k = song.sel % NTRK, r = ui.song_row % OCL_NROW;
    if (slot == 0u) {
        ui.song_row = (uint8_t)clamp((int32_t)r + steps, 0, OCL_NROW - 1u);
    } else if (slot == 1u) {
        track_select((uint32_t)clamp((int32_t)song.sel + (steps > 0 ? 1 : -1), 0, (int32_t)trk_n() - 1));
    } else if (slot == 2u && k < trk_n()) {
        lists.song[r][k] = (uint8_t)clamp((int32_t)lists.song[r][k] + steps, 0, OCL_NCHAIN);
    }
}
