/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ > GROOVE, the selected track's groove template and amount (build doc 4f; pattern.c GROOVE_*).
 * The template's name, the track and the amount; a bar of 16 steps: each step's straight place a thin line, where the
 * groove puts it a block in the track's colour, taller for a louder step (its accent), lower for a softer one. The foot:
 * the knobs (the shifts are drawn twice as wide as they are). KNOB 1 the template (OFF, LAID BACK, PUSH, DILLA, 2-STEP, DUB, HUMAN), KNOB 2 the amount (0..100 %);
 * ALGORITHM the track. Swing (PHRASE, TEMPO) still applies on top. Included by ui_draw.c.
 *
 * Revisions:
 *   0.4.5: first version.
 */
#define GV_NAME_Y H_HEAD                                /* the name and the amount: 30 .. 89 */
#define GV_BAR_Y 90                                     /* the steps: 90 .. 199 */
#define GV_BAR_H 110
#define GV_FOOT_Y (GV_BAR_Y + GV_BAR_H)                 /* the foot: 200 .. 239 */
static struct { uint32_t name, bar, foot; } gvv;
static void gv_name(void)
{
    uint32_t k = song.sel % NTRK, g = lists.groove[k] % GROOVE_N, a = lists.gamt[k];
    uint32_t sig = k * 7u + g * 131u + a * 1031u + ux.gen * 977u + ux.pal * 31u;
    char b[24];
    if (!ui.force && sig == gvv.name)
        return;
    gvv.name = sig;
    cv_begin(240, GV_BAR_Y - GV_NAME_Y, T_BG);
    cv_text_c(120, ty_y(20, TY_TX), TY_TX, GROOVE_NAME[g], g ? T_TRK(k) : T_MID, T_BG);
    str_cpy(b, "T1 \xb7 ", sizeof b);
    b[1] = (char)('1' + k);
    fmt_int(b + str_len(b), (int32_t)a);
    str_cpy(b + str_len(b), "%", 2);
    cv_text_c(120, ty_y(46, TY_LB), TY_LB, b, g ? T_TEXT : T_DIM, T_BG);
    cv_blit(0, GV_NAME_Y);
}
static void gv_bar(void)
{
    uint32_t k = song.sel % NTRK, g = lists.groove[k] % GROOVE_N, a = lists.gamt[k], i;
    uint32_t sig = k * 7u + g * 131u + a * 1031u + ux.gen * 977u + ux.pal * 31u + 3u;
    uint16_t tc = T_TRK(k);
    if (!ui.force && sig == gvv.bar)
        return;
    gvv.bar = sig;
    CV_BANDS(0, GV_BAR_Y, 240, GV_BAR_H, T_BG) {   /* (0.7.0: in bands of the canvas's rows) */
        for (i = 0; i < 16u; i++) {
            int32_t x = 8 + 14 * (int32_t)i, off = GROOVE_OFF[g][i] * (int32_t)a / 100, acc = GROOVE_ACC[g][i] * (int32_t)a / 100;
            int32_t h = 56 + acc * 2, gx = x + off * 28 / 100;   /* (the shift drawn twice as wide: it shows) */
            cv_rect(x + 5, 8, 1, GV_BAR_H - 16, i % 4u == 0u ? T_MID : T_LINE);   /* (the straight place; a beat brighter) */
            if (h < 16) h = 16;
            if (h > GV_BAR_H - 20) h = GV_BAR_H - 20;
            cv_rrect(gx + 1, GV_BAR_H - 10 - h, 9, h, 3, g ? tc : T_LINE, T_BG);
        }
    }
}
static void gv_foot(void)
{
    uint32_t sig = ux.gen * 977u + ux.pal * 31u + 7u;
    if (!ui.force && sig == gvv.foot)
        return;
    gvv.foot = sig;
    cv_begin(240, 240 - GV_FOOT_Y, T_BG);
    cv_text_on(8, ty_y(12, TY_LB), TY_LB, "K1 GROOVE \xb7 K2 AMOUNT", T_MID, T_BG);
    cv_text_on(8, ty_y(30, TY_LB), TY_LB, "ALGO: THE TRACK", T_DIM, T_BG);
    cv_blit(0, GV_FOOT_Y);
}
static void groove_draw(void)
{
    draw_head();
    gv_name();
    gv_bar();
    gv_foot();
}
/* GROOVE's knobs: 1 the template, 2 the amount */
static void groove_edit(uint32_t slot, int32_t steps)
{
    uint32_t k = song.sel % NTRK;
    if (slot == 0u)
        lists.groove[k] = (uint8_t)clamp((int32_t)lists.groove[k] + steps, 0, GROOVE_N - 1u);
    else if (slot == 1u)
        lists.gamt[k] = (uint8_t)clamp((int32_t)lists.gamt[k] + steps * 5, 0, 100);
}
