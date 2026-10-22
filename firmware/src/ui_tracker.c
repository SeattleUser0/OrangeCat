/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ > TRACKER, the selected track's phrase as a tracker's rows (build doc phase 5, specification 6;
 * BEETLE 1.01 is the reference: K1 the row, the keys enter notes, the columns the knobs edit).
 * A row per step of the phrase (LEN of them), nine shown around the cursor: the step's number, # (every fourth, the
 * beat, brighter), N1 .. N4 (the step's four notes: a chord's notes one to a column, a drum track's lane hits after
 * its notes; a held note's rows a bar under each of its notes, an empty step --, a step of recorded notes REC), VEL
 * (its velocity), FX (its first effect: RATCH x2..x4 or FL, CHANCE %, the condition, NUDGE, SL slide, AC accent).
 * The cursor's row on the surface colour, its cell in the column KNOB 2 picked framed; the step playing marked in
 * the track's colour at the left. The foot: what KNOB 3 and KNOB 4 do in that column.
 * Editing (ui_input.c tracker_edit, a SC_STEP page): KNOB 1 the row, KNOB 2 the column; KNOB 3 a note's pitch (on an
 * empty note column: adds a note), the velocity, or in FX which effect; KNOB 4 the step's length, or in FX the
 * effect's amount. The keys write notes from the picked note column on and move down; EDIT deletes the note in the
 * column (in VEL the step, in FX the effect); EDIT held undoes. The note sheet (OCT+ held) still has them all too.
 * PRESET turned right: every track in use as a lane (as BEETLE's), a cell its step's note (* a chord; a held note's
 * bar; -- empty), the selected track's cell on the cursor's row framed, each track's step playing marked in its colour
 * (ALGORITHM picks the track the knobs and keys edit); left: the selected track's columns again.
 * Included by ui_draw.c (after ui_patterns.c).
 *
 * Revisions:
 *   0.4.2: first version; the lanes (PRESET).
 *   0.5.2: four note columns (N1 .. N4) in place of NOTE (the first note, +n), KNOB 2 picks the column; the FX column
 *          edits its effects (KNOB 3 which, KNOB 4 how much); LEN's column gone (a held note's bars show it, KNOB 4
 *          sets it, the foot says it); the accent moved from VEL ("127!") to FX (AC).
 */
#define TK_HEAD_Y H_HEAD                                /* the columns' names: rows 30 .. 47 */
#define TK_ROW_Y 48                                     /* a row: 18 px, nine, 48 .. 209 */
#define TK_ROW_H 18
#define TK_ROWS 9u
#define TK_FOOT_Y (TK_ROW_Y + (int32_t)TK_ROWS * TK_ROW_H)   /* the foot: 210 .. 239 */
#define TK_X_STEP 5                                    /* (0.5.2) the columns: # 5, N1 .. N4 34 px apart from 28, */
#define TK_X_NOTE(c) (28 + 34 * (int32_t)(c))           /* VEL 167, FX 203 (a sharp note's name is 32 px, "127" 30, */
#define TK_X_VEL 167                                    /* "50%" 33) */
#define TK_X_FX 203
static const int16_t TK_COL_X[TKC_N] = {28, 62, 96, 130, TK_X_VEL, TK_X_FX};
static const int16_t TK_COL_W[TKC_N] = {33, 33, 33, 33, 33, 35};
static struct { uint32_t head, row[TK_ROWS], foot; } tkv;

#define TK_LANE_X(k) (32 + 52 * (int32_t)(k))           /* the lanes: 50 px each */
static uint32_t tk_len(void)                            /* the rows: the selected track's LEN (the lanes: the longest) */
{
    uint32_t len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    for (uint32_t k = 0; ui.tk_all && k < trk_n(); k++)
        if ((uint32_t)clamp(trk[k].p[P_SLEN], 1, NSTEP) > len) len = (uint32_t)clamp(trk[k].p[P_SLEN], 1, NSTEP);
    return len;
}
static uint32_t tk_first(void)                          /* the first row shown: the cursor's, four rows down */
{
    uint32_t len = tk_len(), c = ui.cursor % NSTEP;
    if (len <= TK_ROWS || c < 4u) return 0;
    return c - 4u > len - TK_ROWS ? len - TK_ROWS : c - 4u;
}
/* (0.5.2) the effects: names, and an effect's amount on a step as the FX column shows it (3 characters at most: "x3",
 * "FL", "50%", "1:2", "FIL", "!FL", "+3", "SL", "AC"; off: "") or as the foot says it ("x3", "FLAM", "50%", "FILL",
 * "!FILL", "1:2", "+3", "ON"; off: "OFF") */
static const char *const TFX_NAME[TFX_N] = {"RATCHET", "CHANCE", "CONDITION", "NUDGE", "SLIDE", "ACCENT"};
static int tk_fx_on(const step_t *st, uint32_t fx)
{
    switch (fx) {
    case TFX_RATCH: return step_rflam(st) > 1u;
    case TFX_CHANCE: return step_chance(st) < 100u;
    case TFX_COND: return st->cond && st->cond < TC_COUNT;
    case TFX_NUDGE: return st->nudge != 0;
    case TFX_SLIDE: return (st->flags & SF_SLIDE) != 0;
    default: return (st->flags & SF_ACCENT) != 0;
    }
}
static void tk_fx_text(const step_t *st, uint32_t fx, char *b, uint32_t full)
{
    static const char *const COND[TC_COUNT] = {"", "FIL", "!FL", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4",
                                               "3:4", "4:4"};
    uint32_t r = step_rflam(st);
    b[0] = 0;
    if (!tk_fx_on(st, fx)) {
        if (full) str_cpy(b, "OFF", 4);
        return;
    }
    switch (fx) {
    case TFX_RATCH:
        if (r == 5u) { str_cpy(b, full ? "FLAM" : "FL", 5); return; }
        b[0] = 'x'; b[1] = (char)('0' + r); b[2] = 0; return;
    case TFX_CHANCE: fmt_int(b, (int32_t)step_chance(st)); str_cpy(b + str_len(b), "%", 2); return;
    case TFX_COND:
        if (full && st->cond == TC_FILL) { str_cpy(b, "FILL", 5); return; }
        if (full && st->cond == TC_NFILL) { str_cpy(b, "!FILL", 6); return; }
        str_cpy(b, COND[st->cond], 4); return;
    case TFX_NUDGE:
        b[0] = st->nudge > 0 ? '+' : '-';
        fmt_int(b + 1, st->nudge > 0 ? st->nudge : -st->nudge); return;
    case TFX_SLIDE: str_cpy(b, full ? "ON" : "SL", 3); return;
    default: str_cpy(b, full ? "ON" : "AC", 3); return;
    }
}
/* a row's FX cell: the cursor's row in the FX column the picked effect (-- off), any other the first one on */
static void tk_fx(const step_t *st, char *b, uint32_t picked)
{
    uint32_t fx;
    if (picked) {
        tk_fx_text(st, ui.tk_fx % TFX_N, b, 0);
        if (!b[0]) str_cpy(b, "--", 3);
        return;
    }
    for (fx = 0; fx < TFX_N; fx++)
        if (tk_fx_on(st, fx)) { tk_fx_text(st, fx, b, 0); return; }
    b[0] = 0;
}
static void tk_head(void)
{
    static const char *const NAME[TKC_N] = {"N1", "N2", "N3", "N4", "VEL", "FX"};
    uint32_t i, col = ui.tk_col % TKC_N, sig = ux.gen * 977u + ux.pal * 31u + col * 7u + 1u +
                                                ui.tk_all * 40503u + song.sel * 613u + trk_n() * 31u;
    if (!ui.force && sig == tkv.head)
        return;
    tkv.head = sig;
    cv_begin(240, TK_ROW_Y - TK_HEAD_Y, T_BG);
    if (ui.tk_all) {                                    /* the lanes: T1..T4, the selected one underlined */
        cv_text_on(TK_X_STEP, ty_y(9, TY_LB), TY_LB, "#", T_MID, T_BG);
        for (i = 0; i < trk_n(); i++) {
            char n[3] = {'T', (char)('1' + i), 0};
            cv_text_on(TK_LANE_X(i) + 4, ty_y(8, TY_LB), TY_LB, n, i == song.sel ? T_TEXT : T_TRK(i), T_BG);
            if (i == song.sel)
                cv_rrect(TK_LANE_X(i) + 24, 6, 4, 4, 2, T_TRK(i), T_BG);   /* (a dot beside its name) */
        }
        cv_rect(4, TK_ROW_Y - TK_HEAD_Y - 1, 232, 1, T_LINE);
        cv_blit(0, TK_HEAD_Y);
        return;
    }
    cv_text_on(TK_X_STEP, ty_y(9, TY_LB), TY_LB, "#", T_MID, T_BG);
    for (i = 0; i < TKC_N; i++)                         /* (the column KNOB 2 picked in the theme's colour) */
        cv_text_on(TK_COL_X[i], ty_y(9, TY_LB), TY_LB, NAME[i], i == col ? T_THEME : T_MID, T_BG);
    cv_rect(4, TK_ROW_Y - TK_HEAD_Y - 1, 232, 1, T_LINE);
    cv_blit(0, TK_HEAD_Y);
}
/* the lanes' row j: each track's step there */
static void tk_lanes_row(uint32_t j)
{
    uint32_t i = tk_first() + j, len = tk_len(), cur = i == ui.cursor, h = 2166136261u, k;
    char nb[12];
    if (i >= len) {
        h = 0xFFFFFFF0u - j;
        if (!ui.force && h == tkv.row[j]) return;
        tkv.row[j] = h;
        cv_begin(240, TK_ROW_H, T_BG);
        cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
        return;
    }
    h = fnv1(fnv1(h, i + 64u * cur + 128u * song.sel + 1024u * trk_n() + 8192u), ux.gen * 977u + ux.pal * 31u);
    for (k = 0; k < trk_n(); k++) {
        const track_t *t = &trk[k];
        const step_t *st = &seq_steps(t)[i];
        uint32_t tl = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
        h = fnv1(h, (uint32_t)st->note[0] | (uint32_t)st->n << 8 | (uint32_t)st->time << 12 | (uint32_t)st->hit << 16 |
                        (uint32_t)(st->flags & SF_RECORDED) << 24 | (uint32_t)(i < tl) << 31);
        h = fnv1(h, (uint32_t)(song.playing && t->seq_pos < 0x7FFFFFFFu && t->seq_idx == i) +
                        2u * (uint32_t)(t->p[P_MUTE] != 0) + 4u * (st->time == ST_TIE && step_note_start(t, i) < NSTEP));
    }
    if (!ui.force && h == tkv.row[j])
        return;
    tkv.row[j] = h;
    cv_begin(240, TK_ROW_H, T_BG);
    if (cur)
        cv_rrect(3, 0, 234, TK_ROW_H, 4, T_SURF, T_BG);
    fmt_int(nb, (int32_t)i + 1);
    if (i + 1u < 10u) { nb[1] = nb[0]; nb[0] = '0'; nb[2] = 0; }
    cv_text_on(TK_X_STEP, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, i % 4u == 0u ? T_TEXT : T_MID, cur ? T_SURF : T_BG);
    for (k = 0; k < trk_n(); k++) {
        const track_t *t = &trk[k];
        const step_t *st = &seq_steps(t)[i];
        uint16_t tc = t->p[P_MUTE] ? T_DIM : T_TRK(k), bg = cur ? T_SURF : T_BG;
        int32_t x = TK_LANE_X(k);
        uint32_t cnt = st->n, first = st->note[0], l;
        if (i >= (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP))   /* (past this track's LEN: nothing) */
            continue;
        if (cur && k == song.sel) {                     /* the cell the knobs and keys edit: framed */
            cv_rrect(x, 1, 48, TK_ROW_H - 2, 3, T_TEXT, T_SURF);
            cv_rrect(x + 1, 2, 46, TK_ROW_H - 4, 2, T_SURF, T_TEXT);
        }
        if (song.playing && t->seq_pos < 0x7FFFFFFFu && t->seq_idx == i)   /* its step playing */
            cv_rrect(x + 2, 3, 2, TK_ROW_H - 6, 1, tc, bg);
        for (l = NLANE; l-- > 0;)
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n) first = DRUM_LANE_NOTE[l];
            }
        if (step_on(st) && cnt) {
            note_name(nb, first);
            if (cnt > 1u) str_cpy(nb + str_len(nb), "*", 2);
            cv_text_on(x + 6, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, tc, bg);
        } else if (st->time == ST_TIE && step_note_start(t, i) < NSTEP) {
            cv_rect(x + 8, 3, 3, TK_ROW_H - 6, tc);
        } else if (st->flags & SF_RECORDED) {
            cv_text_on(x + 6, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "REC", T_MID, bg);
        } else {
            cv_text_on(x + 6, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "--", T_DIM, bg);
        }
    }
    cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
}
/* (0.5.2) a step's notes as the note columns show them: its notes, then a drum track's lane hits; past four the
 * fourth column "+n" (*more: the hits past four). Returns how many columns */
static uint32_t tk_notes(const step_t *st, uint8_t *n, uint32_t *more)
{
    uint32_t c = 0, l, extra = 0;
    for (l = 0; l < st->n && l < 4u; l++) n[c++] = st->note[l];
    for (l = 0; l < NLANE; l++)
        if ((st->hit >> l) & 1u) {
            if (c < 4u) n[c++] = (uint8_t)DRUM_LANE_NOTE[l];
            else extra++;
        }
    *more = extra;
    return c;
}
static void tk_row(uint32_t j)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = tk_first() + j, cur = i == ui.cursor,
             play = song.playing && t->seq_pos < 0x7FFFFFFFu && t->seq_idx == i, h, c, col = ui.tk_col % TKC_N,
             start = NSTEP, held = 0, cnt, more;
    uint16_t tc = T_TRK(song.sel), bg = cur ? T_SURF : T_BG;
    const step_t *st;
    uint8_t nn[4];
    char nb[12], fb[6];
    if (i >= len) {
        h = 0xFFFFFFFFu - j;
        if (!ui.force && h == tkv.row[j]) return;
        tkv.row[j] = h;
        cv_begin(240, TK_ROW_H, T_BG);
        cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
        return;
    }
    st = &seq_steps(t)[i];
    if (st->time == ST_TIE && (start = step_note_start(t, i)) < NSTEP) {   /* (a held note: bars under its notes) */
        held = tk_notes(&seq_steps(t)[start], nn, &more);
        if (!held) held = 1;
    }
    h = fnv1(fnv1(fnv1(2166136261u, i + 64u * cur + 128u * play + 256u * song.sel + 4096u * (cur ? col + 1u : 0u) +
                                        65536u * (cur ? ui.tk_fx : 0u)), ux.gen * 977u + ux.pal * 31u),
             (uint32_t)st->note[0] | (uint32_t)st->n << 8 | (uint32_t)st->time << 12 | (uint32_t)st->flags << 16 |
                 (uint32_t)st->vel << 24);
    h = fnv1(fnv1(h, (uint32_t)st->hit | (uint32_t)st->probability << 8 | (uint32_t)st->cond << 16 |
                         (uint32_t)(uint8_t)st->nudge << 24),
             (uint32_t)st->note[1] | (uint32_t)st->note[2] << 8 | (uint32_t)st->note[3] << 16);
    h = fnv1(h, held + 8u * (start < NSTEP ? start : 0u));
    if (!ui.force && h == tkv.row[j])
        return;
    tkv.row[j] = h;
    cv_begin(240, TK_ROW_H, T_BG);
    if (cur) {
        cv_rrect(3, 0, 234, TK_ROW_H, 4, T_SURF, T_BG);
        cv_rrect(TK_COL_X[col] - 2, 1, TK_COL_W[col] + 2, TK_ROW_H - 2, 3, T_TEXT, T_SURF);   /* (the cell the knobs
                                                                                               * edit: framed) */
        cv_rrect(TK_COL_X[col] - 1, 2, TK_COL_W[col], TK_ROW_H - 4, 2, T_SURF, T_TEXT);
    }
    if (play)                                           /* the step playing: the track's colour at the left */
        cv_rrect(1, 2, 3, TK_ROW_H - 4, 1, tc, bg);
    fmt_int(nb, (int32_t)i + 1);
    if (i + 1u < 10u) { nb[1] = nb[0]; nb[0] = '0'; nb[2] = 0; }
    cv_text_on(TK_X_STEP, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, i % 4u == 0u ? T_TEXT : T_MID, bg);
    cnt = tk_notes(st, nn, &more);
    if (step_on(st) && cnt) {
        for (c = 0; c < cnt; c++) {
            if (c == 3u && more) {                      /* (a drum step of more than four hits: +n) */
                nb[0] = '+';
                fmt_int(nb + 1, (int32_t)more + 1);
            } else {
                note_name(nb, nn[c]);
            }
            cv_text_on(TK_X_NOTE(c), ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, tc, bg);
        }
        fmt_int(nb, (int32_t)(st->vel ? st->vel : 96));
        cv_text_on(TK_X_VEL, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, T_TEXT, bg);
    } else if (held) {
        for (c = 0; c < held && c < 4u; c++)
            cv_rect(TK_X_NOTE(c) + 2, 3, 3, TK_ROW_H - 6, tc);
    } else if (st->flags & SF_RECORDED) {
        cv_text_on(TK_X_NOTE(0), ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "REC", T_MID, bg);
    } else {
        cv_text_on(TK_X_NOTE(0), ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "--", T_DIM, bg);
    }
    if (step_on(st) && cnt) {
        uint32_t picked = cur && col == TKC_FX;
        tk_fx(st, fb, picked);
        if (fb[0])
            cv_text_on(TK_X_FX, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, fb, picked && !tk_fx_on(st, ui.tk_fx % TFX_N) ?
                                                                    T_DIM : T_SEC, bg);
    }
    cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
}
/* (0.5.2) the foot: what KNOB 3 and KNOB 4 do in the picked column, with the cell's value ("K3 PITCH · K4 LENGTH 3",
 * "K3 VELOCITY · K4 LENGTH 3", "K3 CHANCE · K4 50%"). The lanes: the track, the phrase it plays and its length */
static void tk_hint(char *b, uint32_t n)
{
    const track_t *t = TSEL;
    uint32_t col = ui.tk_col % TKC_N, at = step_note_start(t, ui.cursor % NSTEP);
    const step_t *st = at < NSTEP ? &seq_steps(t)[at] : 0;
    if (col == TKC_FX) {
        char v[8];
        str_cpy(b, "K3 ", n);
        str_cpy(b + str_len(b), TFX_NAME[ui.tk_fx % TFX_N], n - str_len(b));
        str_cpy(b + str_len(b), " \xb7 K4 ", n - str_len(b));
        if (st) tk_fx_text(st, ui.tk_fx % TFX_N, v, 1);
        else str_cpy(v, "OFF", 4);
        str_cpy(b + str_len(b), v, n - str_len(b));
        return;
    }
    if ((st && (st->flags & SF_RECORDED)) || (!st && (seq_steps(t)[ui.cursor % NSTEP].flags & SF_RECORDED))) {
        str_cpy(b, "RECORDED NOTES", n);        /* (played in live: NOTES edits them) */
        return;
    }
    if (!st || !st->n) {
        str_cpy(b, col == TKC_VEL ? "NO NOTE HERE" : "K3 OR A KEY ADDS A NOTE", n);
        return;
    }
    if (col < TKC_VEL && col >= st->n) {
        str_cpy(b, "K3 ADDS A NOTE", n);
        return;
    }
    str_cpy(b, col == TKC_VEL ? "K3 VELOCITY \xb7 K4 LENGTH " : "K3 PITCH \xb7 K4 LENGTH ", n);
    fmt_int(b + str_len(b), (int32_t)step_note_length(t, at));
}
static void tk_foot(void)
{
    char b[48];
    uint32_t k = song.sel % NTRK, v = bank_pid[k][TSEL->pattern], len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    uint32_t sig = k * 7u + v * 131u + len * 1031u + ux.gen * 977u + ux.pal * 31u + ui.tk_all * 40503u;
    if (!ui.tk_all) {                                   /* (the hint: its text's own hash) */
        tk_hint(b, sizeof b);
        sig = fnv1(fnv1(2166136261u, sig), (uint32_t)str_len(b));
        for (uint32_t i = 0; b[i]; i++) sig = fnv1(sig, (uint8_t)b[i]);
    }
    if (!ui.force && sig == tkv.foot)
        return;
    tkv.foot = sig;
    cv_begin(240, 240 - TK_FOOT_Y, T_BG);
    cv_rect(4, 0, 232, 1, T_LINE);
    if (!ui.tk_all) {
        cv_text_fit(8, ty_y((240 - TK_FOOT_Y) / 2 + 1, TY_LB), TY_LB, b, T_MID, T_BG, 226);
        cv_blit(0, TK_FOOT_Y);
        return;
    }
    str_cpy(b, "T1 \xb7 ", sizeof b);
    b[1] = (char)('1' + k);
    if (v && v <= OCL_NPOOL) {
        uint32_t n = str_len(b);
        str_cpy(b + n, "PHRASE 00", 10);
        b[n + 7] = (char)('0' + v / 10u);
        b[n + 8] = (char)('0' + v % 10u);
    } else {
        str_cpy(b + str_len(b), "NEW PHRASE", 11);
    }
    str_cpy(b + str_len(b), " \xb7 ", 4);
    fmt_int(b + str_len(b), (int32_t)len);
    str_cpy(b + str_len(b), len == 1u ? " STEP" : " STEPS", 7);
    cv_text_on(8, ty_y((240 - TK_FOOT_Y) / 2 + 1, TY_LB), TY_LB, b, T_MID, T_BG);
    cv_blit(0, TK_FOOT_Y);
}
static void tracker_draw(void)
{
    uint32_t j;
    draw_head();
    tk_head();
    for (j = 0; j < TK_ROWS; j++) {
        if (ui.tk_all) tk_lanes_row(j);
        else tk_row(j);
    }
    tk_foot();
}
