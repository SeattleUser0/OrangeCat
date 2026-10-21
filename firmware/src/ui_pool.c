/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: SEQ > PHRASES, the pool's 64 phrases at a glance (build doc 4e; Skiff 1.3.3's pattern pool page).
 * An 8 x 8 grid, 01..64: a phrase with something in it on the surface colour (its number bright when this project names
 * it: a chain's slot or a track's bank; dim when only other projects do, or none), an empty one an outline; the one the
 * selected track plays filled in its colour; the one picked framed. The foot: the phrase picked (its LEN, the slots of
 * this project naming it), what OCT+ does. KNOB 1 picks the phrase (KNOB 2 a row of eight at a time); OCT+ puts it in
 * the selected track's chain slot picked on CHAIN and the track goes there (stopped: at once; playing: at the launch
 * point); OCT+ held, its sheet: Copy to new, Clear phrase (one nothing names). Included by ui_draw.c.
 *
 * Revisions:
 *   0.4.4: first version.
 */
#define PL_Y 34                                         /* the grid: 8 rows of 21 px, 34 .. 201 */
#define PL_ROW_H 21
#define PL_CELL_X(c) (12 + 28 * (int32_t)(c))           /* a cell: 26 x 19 */
#define PL_FOOT_Y (PL_Y + 8 * PL_ROW_H)                 /* the foot: 202 .. 239 */
static struct { uint32_t row[8], foot; } plv;
static uint32_t ocl_pat_len(uint32_t id);               /* oc_lists.c */
static int ocl_pat_named(uint32_t id);
static uint32_t ocl_pat_slots(uint32_t id);
static int ocl_pat_used(uint32_t id);
static uint32_t ocl_pool_copy(uint32_t id);
static int ocl_pool_clear(uint32_t id);
static int ocl_pool_put(uint32_t k, uint32_t e, uint32_t id);

static uint32_t pl_playing(void) { return bank_pid[song.sel % NTRK][TSEL->pattern]; }   /* + 1, 0 none */
static void pl_row(uint32_t r)
{
    uint32_t c, h = fnv1(fnv1(2166136261u, r + 8u * (ui.pool_sel % 64u) + 1024u * pl_playing() + 65536u * song.sel),
                         ux.gen * 977u + ux.pal * 31u);
    for (c = 0; c < 8u; c++) {
        uint32_t id = r * 8u + c;
        h = fnv1(h, (uint32_t)ocl_pat_used(id) + 2u * (uint32_t)ocl_pat_named(id) + 4u * ocl_pat_len(id));
    }
    if (!ui.force && h == plv.row[r])
        return;
    plv.row[r] = h;
    cv_begin(240, PL_ROW_H, T_BG);
    for (c = 0; c < 8u; c++) {
        uint32_t id = r * 8u + c, used = (uint32_t)ocl_pat_used(id), named = (uint32_t)ocl_pat_named(id);
        uint32_t pick = id == ui.pool_sel % 64u, play = pl_playing() == id + 1u;
        int32_t x = PL_CELL_X(c);
        uint16_t f = play ? T_TRK(song.sel % NTRK) : used ? T_SURF : T_BG;
        char n[3] = {(char)('0' + (id + 1u) / 10u), (char)('0' + (id + 1u) % 10u), 0};
        if (pick)
            cv_rrect(x - 2, 0, 30, PL_ROW_H, 5, T_TEXT, T_BG);
        if (used || play) {
            cv_rrect(x, 1, 26, PL_ROW_H - 2, 4, f, pick ? T_TEXT : T_BG);
        } else {
            cv_rrect(x, 1, 26, PL_ROW_H - 2, 4, T_LINE, pick ? T_TEXT : T_BG);
            cv_rrect(x + 1, 2, 24, PL_ROW_H - 4, 3, T_BG, T_LINE);
        }
        cv_text_c(x + 13, ty_y(PL_ROW_H / 2, TY_LB), TY_LB, n, play ? T_INK : used && named ? T_TEXT : used ? T_MID :
                  T_DIM, f);
    }
    cv_blit(0, (uint32_t)(PL_Y + (int32_t)r * PL_ROW_H));
}
static void pl_foot(void)
{
    char b[40];
    uint32_t id = ui.pool_sel % 64u, len = ocl_pat_len(id), used = (uint32_t)ocl_pat_used(id), s = ocl_pat_slots(id);
    uint32_t sig = id * 7u + len * 131u + used * 1031u + s * 40503u + ux.gen * 977u + ux.pal * 31u + song.sel * 3u +
                   ui.chain_step * 613u;
    if (!ui.force && sig == plv.foot)
        return;
    plv.foot = sig;
    cv_begin(240, 240 - PL_FOOT_Y, T_BG);
    str_cpy(b, "01 \xb7 ", sizeof b);
    b[0] = (char)('0' + (id + 1u) / 10u); b[1] = (char)('0' + (id + 1u) % 10u);
    if (used) {
        str_cpy(b + str_len(b), "LEN ", 5);
        fmt_int(b + str_len(b), (int32_t)len);
        str_cpy(b + str_len(b), " \xb7 ", 4);
        fmt_int(b + str_len(b), (int32_t)s);
        str_cpy(b + str_len(b), s == 1u ? " SLOT" : " SLOTS", 7);
    } else {
        str_cpy(b + str_len(b), "EMPTY", 6);
    }
    cv_text_on(8, ty_y(13, TY_LB), TY_LB, b, T_TEXT, T_BG);
    str_cpy(b, "OCT+ INTO T1 SLOT A", sizeof b);
    b[11] = (char)('1' + song.sel % NTRK); b[18] = (char)('A' + ui.chain_step % OCL_NENT);
    cv_text_on(8, ty_y(30, TY_LB), TY_LB, b, T_MID, T_BG);
    cv_blit(0, PL_FOOT_Y);
}
static void pool_draw(void)
{
    uint32_t r;
    draw_head();
    for (r = 0; r < 8u; r++)
        pl_row(r);
    pl_foot();
}
/* PHRASES' knobs: 1 the phrase, 2 a row of eight */
static void pool_edit(uint32_t slot, int32_t steps)
{
    int32_t v = (int32_t)(ui.pool_sel % 64u);
    if (slot == 0u) v += steps;
    else if (slot == 1u) v += 8 * (steps > 0 ? 1 : -1);
    else return;
    ui.pool_sel = (uint8_t)clamp(v, 0, 63);
}
/* OCT+: the phrase picked into the selected track's chain slot picked on CHAIN, the track there */
static void pool_put(void)
{
    uint32_t k = song.sel % NTRK, e = ui.chain_step % OCL_NENT, id = ui.pool_sel % 64u;
    char m[24];
    int rc;
    if (k >= trk_n()) return;
    rc = ocl_pool_put(k, e, id);
    if (rc) {
        ui_message(rc == 4 ? (song.playing ? "STOP TO KEEP EDITS" : "PHRASES FULL") : rc == 5 ? "PATTERN DATA FULL" :
                   rc == 1 ? "PHRASE DOES NOT READ" : "STOP FIRST");
        return;
    }
    str_cpy(m, "T1 SLOT A: 01", sizeof m);
    m[1] = (char)('1' + k); m[8] = (char)('A' + e);
    m[11] = (char)('0' + (id + 1u) / 10u); m[12] = (char)('0' + (id + 1u) % 10u);
    ui_message(m);
}
