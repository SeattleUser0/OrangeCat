/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 4 (docs/orangecat/UI_DESIGN.md Part 1 section 4 "FEEL", Part 2 feel.png; docs/orangecat/UI_BUILD.md
 * "Step 4: the step pages"): SEQ > FEEL, behind OC_STEP_PAGES (core.h; it needs OC_SHIFT_MAP).
 *
 * The page: the four dials (CHANCE, RATCH, NUDGE, TYPE: the cursor step's values, drawn by ui_draw.c draw_columns from
 * feel_text) over the 16 steps of the cursor's page as tiles, four by four. A tile: the step's number and its value of
 * the knob turned last (feel_k); a step that is off is dim and shows no value; the cursor's tile is lifted; the selected
 * range (OCT+ held with KNOB 1 or 2, or SHIFT + PRESETS: bulk, as NOTES') is framed in the accent; the step playing has a
 * bar over it. The values are the step's own (step_t): a manual step's and a recorded step's view cell alike, as the
 * tracker's FX column reads and writes them (ui_input.c ui_feel_edit.c).
 *
 * This file is the drawing and the reading, included by ui_graph.c (draw_graph calls graph_feel; ui_draw.c reads feel_text). The
 * editing is ui_feel_edit.c, included by ui_input.c.
 * State: feel_k, 1 B.
 * Revisions:
 *   (the new interface 0.4, branch ui-build) first version.
 */
#if OC_STEP_PAGES
static uint8_t feel_k;                                  /* the knob turned last, 0 .. 3: what the tiles show */

static const char *const FEEL_NAME[4] = {"CHNC", "RATCH", "NUDGE", "TYPE"};   /* (the card has room for four or five capitals) */
/* the condition names, as the tracker's (ui_tracker.c tk_fx_text): short for a tile, long for a dial */
static const char *const FEEL_COND_S[TC_COUNT] = {"", "FIL", "!FL", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4"};
static const char *const FEEL_COND_L[TC_COUNT] = {"", "FILL", "!FILL", "1:2", "2:2", "1:3", "2:3", "3:3", "1:4", "2:4", "3:4", "4:4"};

/* CHANCE and the conditions are one scale of 112 places: 0 .. 100 % then FILL .. 4:4 (TC_FILL .. TC_4_4). A step with a
 * condition is at its condition (its chance is kept, not shown); a step without is at its chance */
#define FEEL_POS_MAX (100u + TC_COUNT - 1u)
static uint32_t feel_pos(const step_t *st)
{
    return st->cond && st->cond < TC_COUNT ? 100u + st->cond : step_chance(st);
}
/* a step's value of the knob k as text (b: 8 bytes), "" where it is the default (100 %, x1, 0, none). full: a dial's words */
static void feel_text(const step_t *st, uint32_t k, char *b, uint32_t full)
{
    static const char *const TYPE[4] = {"", "AC", "SL", "AS"};   /* (a tile has room for two) */
    static const char *const TYPE_L[4] = {"-", "ACC", "SLD", "A+S"};
    uint32_t type = (st->flags & SF_ACCENT ? 1u : 0u) | (st->flags & SF_SLIDE ? 2u : 0u);
    b[0] = 0;
    switch (k & 3u) {
    case 0:
        if (st->cond && st->cond < TC_COUNT) { str_cpy(b, (full ? FEEL_COND_L : FEEL_COND_S)[st->cond], 8); return; }
        if (full || step_chance(st) < 100u) fmt_int(b, (int32_t)step_chance(st));
        return;
    case 1: {
        uint32_t r = step_rflam(st);
        if (r == 5u) { str_cpy(b, full ? "FLAM" : "FL", 8); return; }
        if (full || r > 1u) { b[0] = 'x'; b[1] = (char)('0' + r); b[2] = 0; }
        return;
    }
    case 2:
        if (full || st->nudge) {
            b[0] = st->nudge < 0 ? '-' : '+';
            fmt_int(b + 1, st->nudge < 0 ? -st->nudge : st->nudge);
        }
        return;
    default:
        str_cpy(b, (full ? TYPE_L : TYPE)[type], 8);
        return;
    }
}
/* the selected range of steps: OCT+ held with KNOB 1 or 2 selected steps (bulk.on == 1); none: the cursor's step alone */
static void feel_range(uint32_t *lo, uint32_t *hi)
{
    uint32_t a = ui.cursor % NSTEP, b = a;
    if (bulk.on == 1u) { a = bulk.anchor % NSTEP; b = bulk.end % NSTEP; }
    *lo = a < b ? a : b;
    *hi = a < b ? b : a;
}
/* the page's picture changes: the signature of what the tiles show (steps_hash leaves out the chance, condition and nudge) */
static uint32_t feel_sig(const track_t *t)
{
    uint32_t h = 2166136261u, i, lo, hi, base = ui.cursor / 16u * 16u, len = (uint32_t)t->p[P_SLEN];
    feel_range(&lo, &hi);
    for (i = 0; i < 16u && base + i < len; i++) {
        const step_t *st = &t->step[base + i];
        h = (h ^ (st->probability | (uint32_t)st->cond << 8 | (uint32_t)(uint8_t)st->nudge << 16 | (uint32_t)st->flags << 24)) * 16777619u;
        h = (h ^ (st->time | (uint32_t)st->n << 4 | (uint32_t)st->hit << 8)) * 16777619u;
    }
    h ^= ui.cursor * 613u + feel_k * 7919u + bulk.on * 37u + lo * 41u + hi * 43u;
    if (song.playing) h ^= (t->seq_idx + 1u) * 104729u;
    return h;
}
/* the tiles, on the panel draw_graph made (cv_oy 0, 234 x graph_h from x 3): 4 x 4, 55 x 26 each, 3 px apart. The number
 * at the left, the value at the right, in the knob's cozy color */
#define FEEL_TX 7
#define FEEL_TY 5
#define FEEL_TW 55
#define FEEL_TH 26
static void graph_feel(const track_t *t, uint16_t c)
{
    uint32_t i, lo, hi, base = ui.cursor / 16u * 16u, len = (uint32_t)t->p[P_SLEN];
    int32_t playing = song.playing && t->seq_idx < len && t->seq_idx / 16u == base / 16u ? (int32_t)(t->seq_idx % 16u) : -1;
    (void)c;
    feel_range(&lo, &hi);
    for (i = 0; i < 16u && base + i < len; i++) {
        const step_t *st = &t->step[base + i];
        int32_t x = FEEL_TX + (int32_t)(i % 4u) * (FEEL_TW + 3), y = FEEL_TY + (int32_t)(i / 4u) * (FEEL_TH + 3);
        int on = step_on(st), cur = base + i == ui.cursor, sel = bulk.on == 1u && base + i >= lo && base + i <= hi;
        uint16_t fill = cur ? T_LIFT : T_PANEL, ink = on ? T_TEXT : T_DIM;
        char n[4], v[8];
        if (sel)
            cv_rrect(x - 2, y - 2, FEEL_TW + 4, FEEL_TH + 4, 6, T_ACCENT, T_SURF);   /* (the range's frame) */
        cv_rrect(x, y, FEEL_TW, FEEL_TH, 5, fill, sel ? T_ACCENT : T_SURF);
        if ((int32_t)i == playing)
            cv_rect(x + 8, y, FEEL_TW - 16, 3, T_TEXT);
        fmt_int(n, (int32_t)(base + i) + 1);
        cv_text_on(x + 5, ty_y(y + FEEL_TH / 2, TY_LB), TY_LB, n, cur ? T_TEXT : on ? T_MID : T_DIM, fill);
        if (on) {
            feel_text(st, feel_k, v, 0);
            if (v[0])
                cv_text_r(x + FEEL_TW - 5, ty_y(y + FEEL_TH / 2, TY_TX), TY_TX, v, T_COZY(feel_k), fill);
        }
        (void)ink;
    }
}
/* hundredths of a step as text: 3 or 3.50 (always the decimals when always: START's 5.00) */
static void feel_hund(char *b, uint32_t h, int always)
{
    fmt_int(b, (int32_t)(h / 100u));
    if (always || h % 100u) {
        uint32_t f = h % 100u;
        str_cpy(b + str_len(b), ".", 2);
        if (f < 10u) str_cpy(b + str_len(b), "0", 2);
        fmt_int(b + str_len(b), (int32_t)f);
    }
}
#endif
