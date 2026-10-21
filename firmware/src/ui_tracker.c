/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ > TRACKER, the selected track's phrase as a tracker's rows (build doc phase 5, specification 6;
 * BEETLE 1.01 is the reference: K1 the row, the keys enter notes, the columns the knobs edit).
 * A row per step of the phrase (LEN of them), nine shown around the cursor: the step's number, # (every fourth, the
 * beat, brighter), NOTE (its first note, +n more in a chord or lane hits; a tie's rows a bar, an empty step --, a step
 * of recorded notes REC), LEN (a note's length in steps, on the row it starts), VEL (its velocity; ! an accent),
 * FX (RATCH x2..x4 or FLAM, else CHANCE %, else the condition, else NUDGE). The cursor's row on the surface colour,
 * the step playing marked in the track's colour at the left. The foot: the track, the phrase, the keys.
 * Editing is Melodee's step entry (a SC_STEP page: ui_input.c step_edit, seq_entry): KNOB 1 the row, KNOB 2 the note,
 * KNOB 3 its length, KNOB 4 its velocity (ENV held: SLIDE); the keys write notes at the cursor and move on; EDIT
 * deletes the note there; EDIT held undoes. The note sheet (OCT+ held) has CHANCE, RATCH, the condition, NUDGE.
 * PRESET turned right: every track in use as a lane (as BEETLE's), a cell its step's note (* a chord; a held note's
 * bar; -- empty), the selected track's cell on the cursor's row framed, each track's step playing marked in its colour
 * (ALGORITHM picks the track the knobs and keys edit); left: the selected track's columns again.
 * Included by ui_draw.c (after ui_patterns.c).
 *
 * Revisions:
 *   0.4.2: first version; the lanes (PRESET).
 */
#define TK_HEAD_Y H_HEAD                                /* the columns' names: rows 30 .. 47 */
#define TK_ROW_Y 48                                     /* a row: 18 px, nine, 48 .. 209 */
#define TK_ROW_H 18
#define TK_ROWS 9u
#define TK_FOOT_Y (TK_ROW_Y + (int32_t)TK_ROWS * TK_ROW_H)   /* the foot: 210 .. 239 */
#define TK_X_STEP 8
#define TK_X_NOTE 40
#define TK_X_LEN 112
#define TK_X_VEL 146
#define TK_X_FX 186
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
/* step i's FX column ("x3", "FL", "50%", "1:2", "+3"; none: "") into b */
static void tk_fx(const step_t *st, char *b)
{
    static const char *const COND[TC_COUNT] = {"", "FILL", "!FIL", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4",
                                               "3:4", "4:4"};
    uint32_t r = step_rflam(st), ch = step_chance(st);
    b[0] = 0;
    if (r == 5u) { str_cpy(b, "FL", 4); return; }
    if (r > 1u) { b[0] = 'x'; b[1] = (char)('0' + r); b[2] = 0; return; }
    if (ch < 100u) { fmt_int(b, (int32_t)ch); str_cpy(b + str_len(b), "%", 2); return; }
    if (st->cond && st->cond < TC_COUNT) { str_cpy(b, COND[st->cond], 6); return; }
    if (st->nudge) {
        b[0] = st->nudge > 0 ? '+' : '-';
        fmt_int(b + 1, st->nudge > 0 ? st->nudge : -st->nudge);
    }
}
static void tk_head(void)
{
    static const char *const NAME[5] = {"#", "NOTE", "LEN", "VEL", "FX"};
    static const int16_t X[5] = {TK_X_STEP, TK_X_NOTE, TK_X_LEN, TK_X_VEL, TK_X_FX};
    uint32_t i, hot = ui.hot_t ? ui.hot_col : 9u, sig = ux.gen * 977u + ux.pal * 31u + hot * 7u + 1u +
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
    for (i = 0; i < 5u; i++) {                          /* (the knob's own column brighter while it turns: K1 STEP,
                                                         * K2 NOTE, K3 LEN, K4 VEL) */
        uint32_t on = i < 4u && i == hot;
        cv_text_on(X[i], ty_y(9, TY_LB), TY_LB, NAME[i], on ? T_THEME : T_MID, T_BG);
    }
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
static void tk_row(uint32_t j)
{
    const track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = tk_first() + j, cur = i == ui.cursor,
             play = song.playing && t->seq_pos < 0x7FFFFFFFu && t->seq_idx == i, h, l;
    uint16_t tc = T_TRK(song.sel), bg = cur ? T_SURF : T_BG;
    const step_t *st;
    char nb[12], lb[6], vb[6], fb[6];
    if (i >= len) {
        h = 0xFFFFFFFFu - j;
        if (!ui.force && h == tkv.row[j]) return;
        tkv.row[j] = h;
        cv_begin(240, TK_ROW_H, T_BG);
        cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
        return;
    }
    st = &seq_steps(t)[i];
    h = fnv1(fnv1(fnv1(2166136261u, i + 64u * cur + 128u * play + 256u * song.sel), ux.gen * 977u + ux.pal * 31u),
             (uint32_t)st->note[0] | (uint32_t)st->n << 8 | (uint32_t)st->time << 12 | (uint32_t)st->flags << 16 |
                 (uint32_t)st->vel << 24);
    h = fnv1(fnv1(h, (uint32_t)st->hit | (uint32_t)st->probability << 8 | (uint32_t)st->cond << 16 |
                         (uint32_t)(uint8_t)st->nudge << 24),
             (uint32_t)st->note[1] | (uint32_t)st->note[2] << 8 | (uint32_t)st->note[3] << 16);
    h = fnv1(h, step_note_length(t, i) + 256u * (st->time == ST_TIE ? step_note_start(t, i) : 0u));
    if (!ui.force && h == tkv.row[j])
        return;
    tkv.row[j] = h;
    cv_begin(240, TK_ROW_H, T_BG);
    if (cur)
        cv_rrect(3, 0, 234, TK_ROW_H, 4, T_SURF, T_BG);
    if (play)                                           /* the step playing: the track's colour at the left */
        cv_rrect(3, 2, 3, TK_ROW_H - 4, 1, tc, bg);
    fmt_int(nb, (int32_t)i + 1);
    if (i + 1u < 10u) { nb[1] = nb[0]; nb[0] = '0'; nb[2] = 0; }
    cv_text_on(TK_X_STEP, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, i % 4u == 0u ? T_TEXT : T_MID, bg);
    lb[0] = vb[0] = 0;
    {
        uint32_t cnt = st->n, first = st->note[0];
        for (l = NLANE; l-- > 0;)                       /* (lane hits count as their notes) */
            if ((st->hit >> l) & 1u) {
                cnt++;
                if (!st->n) first = DRUM_LANE_NOTE[l];
            }
        if (step_on(st) && cnt) {
            note_name(nb, first);
            if (cnt > 1u) {
                str_cpy(nb + str_len(nb), "+", 2);
                fmt_int(nb + str_len(nb), (int32_t)cnt - 1);
            }
            cv_text_on(TK_X_NOTE, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, nb, tc, bg);
            fmt_int(lb, (int32_t)step_note_length(t, i));
            fmt_int(vb, (int32_t)(st->flags & SF_ACCENT ? 127 : st->vel ? st->vel : 96));
            if (st->flags & SF_ACCENT) str_cpy(vb + str_len(vb), "!", 2);
        } else if (st->time == ST_TIE && step_note_start(t, i) < NSTEP) {   /* (a note held on: a bar) */
            cv_rect(TK_X_NOTE + 2, 3, 3, TK_ROW_H - 6, tc);
        } else if (st->flags & SF_RECORDED) {
            cv_text_on(TK_X_NOTE, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "REC", T_MID, bg);
        } else {
            cv_text_on(TK_X_NOTE, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, "--", T_DIM, bg);
        }
    }
    if (lb[0]) cv_text_on(TK_X_LEN, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, lb, T_TEXT, bg);
    if (vb[0]) cv_text_on(TK_X_VEL, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, vb, T_TEXT, bg);
    tk_fx(st, fb);
    if (fb[0] && step_on(st)) cv_text_on(TK_X_FX, ty_y(TK_ROW_H / 2, TY_LB), TY_LB, fb, T_SEC, bg);
    cv_blit(0, (uint32_t)(TK_ROW_Y + (int32_t)j * TK_ROW_H));
}
static void tk_foot(void)                               /* the track, the phrase it plays and its length */
{
    char b[40];
    uint32_t k = song.sel % NTRK, v = bank_pid[k][TSEL->pattern], len = (uint32_t)clamp(TSEL->p[P_SLEN], 1, NSTEP);
    uint32_t sig = k * 7u + v * 131u + len * 1031u + ux.gen * 977u + ux.pal * 31u + ui.tk_all * 40503u;
    if (!ui.force && sig == tkv.foot)
        return;
    tkv.foot = sig;
    cv_begin(240, 240 - TK_FOOT_Y, T_BG);
    cv_rect(4, 0, 232, 1, T_LINE);
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
