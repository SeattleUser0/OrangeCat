/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 2 (docs/orangecat/UI_DESIGN.md Part 1 section 1, Part 3 step 2; docs/orangecat/UI_BUILD.md
 * "Step 2: EDIT's sheets"): EDIT's sheets, behind OC_EDIT_SHEETS (core.h; it needs OC_HOME_PIC).
 *
 * A sheet is a group of pages (its rows) drawn under one tab: the engine's own sections (SOUND: EDIT 1 and 2; the
 * Prophet's Osc .. Wheel; FM6's Algo, OP1 .. OP6, Pitch, LFO, Ctrl; CZ-1's lines; DRUM-X's Kit and Lanes), then Env
 * (the FAM_ENV pages), Mod (the FAM_LFO pages), a link to FX (the FX family's first page: the track's sends), Voice,
 * and Store / Tools last. EDIT turns the sheets, SELECT walks the rows on into the next sheet, PRESETS turns the
 * sheets; ENV and LFO keep visiting their pages, which are rows of Env and Mod (on the Prophet, FM6 and CZ-1, rows of
 * the engine's own sheets): the shortcuts the design asks for.
 *
 * Nothing is stored about the sheets (the design review, UI_BUILD.md b1, b6, b7): a page's sheet is worked out from
 * the page itself (its family, its title by ui_sections.c's rule, a rank), and the list of sheets is made again on
 * every use (once a frame while drawing: sh_now), so a module coming or going, an engine change or the Prophet hiding ENV never leaves it stale. RAM: sh_fx
 * (EDIT went through the FX link), sh_now (this frame's sheets) and the two signatures of the drawing, 13 B.
 *
 * Included by ui_draw.c after ui_list.c (the capture of a row: lcap, draw_columns' CS_CAPTURE).
 * Revisions:
 *   (the new interface 0.2, branch ui-build) first version.
 */
enum { SH_SOUND, SH_ENV, SH_MOD, SH_FX, SH_VOICE, SH_STORE, SH_RANKS };   /* a sheet's rank: its place in the order */
#define SH_MAX 24u                                      /* sheets made at most (FM6, the most today: 15); past it a page
                                                         * is a row of no sheet, and the family's scroll is the fallback */
#define SH_LINK 0xFFu                                   /* sheet_t.op of the FX link */
#define SH_NONE 0xFFu                                   /* sh_ctx_t.of: not a row (or a sheet past SH_MAX) */
#define SH_OPS 0xFEu                                    /* sh_ctx_t.of: one of FM6's operator pages (every operator's) */
typedef struct {
    const char *name;                                   /* its tab */
    uint8_t first, op, rank;                            /* its first page; FM6's operator + 1 (0: none; SH_LINK: FX) */
} sheet_t;
/* the sheets of the selected track, made in one walk over the pages (the diff review's 1: each page's visibility and
 * title rule once, then no string work): on the stack, about 380 B; one a frame (sh_now) and one a button */
typedef struct {
    sheet_t s[SH_MAX];
    uint8_t of[NPAGES_ALL];                             /* each page's sheet: an index into s, SH_OPS, SH_NONE */
    uint8_t n, cur, made;                               /* sheets; the page shown's (n: none); made yet */
} sh_ctx_t;
static uint8_t sh_fx;                                   /* 1: the FX pages were opened by EDIT through the link */
static sh_ctx_t *sh_now;                                /* this frame's (ui_draw), made when first asked for; 0 between frames */

static int sh_fmop(uint32_t i) { return MELODEE_FM6 && page_at(i)->scope == SC_FMOP; }
/* page i: a row of some sheet (an EDIT, ENV or LFO page that shows on this track) */
static int sh_row(uint32_t i)
{
    uint32_t f;
    if (i >= page_n())
        return 0;
    f = page_at(i)->fam;
    return (f == FAM_EDIT || f == FAM_ENV || f == FAM_LFO) && page_visible(i);
}
/* page i's sheet: its rank, and its name in *nm (not for FM6's operator pages) */
static uint32_t sh_key(uint32_t i, const char **nm)
{
    uint32_t f = page_at(i)->fam;
    if (f == FAM_ENV) {
        *nm = "Env";
        return SH_ENV;
    }
    if (f == FAM_LFO) {                                 /* ("Trk mod" where the engine has its own LFO: the tabs never
                                                         * show Mod twice, the Prophet having a Mod section of its own) */
        *nm = native_titles(FAM_LFO) ? "Trk mod" : "Mod";
        return SH_MOD;
    }
    *nm = sec_rule(i, 0);
    if (str_eq(*nm, "Voice"))
        return SH_VOICE;
    if (sec_store(*nm))
        return SH_STORE;
    if (MELODEE_DRUMX && TSEL->eng_req == ENGI_DRUMX && str_eq(*nm, "Sound"))
        *nm = "Kit";                                    /* (DRUM-X: EDIT 1 and 2 are KIT and KIT 2) */
    return SH_SOUND;
}
static uint32_t sh_add(sh_ctx_t *c, const char *nm, uint32_t first, uint32_t op, uint32_t rank)
{
    if (c->n >= SH_MAX)
        return SH_NONE;
    c->s[c->n].name = nm;
    c->s[c->n].first = (uint8_t)first;
    c->s[c->n].op = (uint8_t)op;
    c->s[c->n].rank = (uint8_t)rank;
    return c->n++;
}
/* the sheets in order: by rank, within a rank by first page (sheets merge by name within a rank only) */
static void sh_make(sh_ctx_t *c)
{
    sheet_t t[SH_MAX];
    uint8_t to[SH_MAX];
    uint32_t i, k, r, n, ops = 0;
    c->n = 0;
    for (i = 0; i < NPAGES_ALL; i++)
        c->of[i] = SH_NONE;
    for (i = 0; i < page_n(); i++) {
        const char *nm;
        if (!sh_row(i))
            continue;
        if (sh_fmop(i)) {                               /* FM6's operators: a sheet each, where the first one is */
            c->of[i] = SH_OPS;
            if (!ops)
                for (ops = 1; ops <= 6u; ops++)
                    sh_add(c, SEC_OP[ops - 1u], i, ops, SH_SOUND);
            continue;
        }
        r = sh_key(i, &nm);
        for (k = 0; k < c->n && (c->s[k].op || c->s[k].rank != r || !str_eq(c->s[k].name, nm)); k++)
            ;
        c->of[i] = (uint8_t)(k < c->n ? k : sh_add(c, nm, i, 0, r));
    }
    if (page_visible(page_first(FAM_FX)))
        sh_add(c, "FX", page_first(FAM_FX), SH_LINK, SH_FX);
    for (r = 0, n = 0; r < SH_RANKS; r++)               /* (stable: within a rank the order they were met in) */
        for (k = 0; k < c->n; k++)
            if (c->s[k].rank == r) {
                to[k] = (uint8_t)n;
                t[n++] = c->s[k];
            }
    for (k = 0; k < c->n; k++)
        c->s[k] = t[k];
    for (i = 0; i < NPAGES_ALL; i++)
        if (c->of[i] < SH_MAX)
            c->of[i] = to[c->of[i]];
    c->cur = c->n;
    if (!ui.home && ui.page < NPAGES_ALL) {
        if (c->of[ui.page] == SH_OPS) {
            for (k = 0; k < c->n && c->s[k].op != fm6_opsel % 6u + 1u; k++)
                ;
            c->cur = (uint8_t)k;
        } else if (c->of[ui.page] < c->n) {
            c->cur = c->of[ui.page];
        }
    }
    c->made = 1;
}
/* this frame's sheets (made once a frame, the diff review's 1), or made into c when no frame is being drawn */
static const sh_ctx_t *sh_get(sh_ctx_t *c)
{
    if (sh_now) {
        if (!sh_now->made)
            sh_make(sh_now);
        return sh_now;
    }
    sh_make(c);
    return c;
}
/* page i: a row of sheet k */
static int sh_in(const sh_ctx_t *c, uint32_t k, uint32_t i)
{
    if (k >= c->n || i >= NPAGES_ALL || c->s[k].op == SH_LINK)
        return 0;
    return c->s[k].op ? c->of[i] == SH_OPS : c->of[i] == k;
}
/* sheet k's rows in page order into pg (at most max), the count; *at: the page shown's place among them */
static uint32_t sh_rows(const sh_ctx_t *c, uint32_t k, uint8_t *pg, uint32_t max, uint32_t *at)
{
    uint32_t i, n = 0;
    *at = 0;
    for (i = 0; i < page_n() && n < max; i++)
        if (sh_in(c, k, i)) {
            if (i == ui.page)
                *at = n;
            pg[n++] = (uint8_t)i;
        }
    return n;
}
/* to page p, a row (its own family's last page: never FAM_EDIT's with another family's, the design review's b2) */
static void sh_show(uint32_t p)
{
    ui.home = 0;
    ui.page = (uint8_t)p;
    ui.fam_last[page_at(p)->fam] = ui.page;
    page_entered();
}
/* to sheet k: its first row (last: its last); the FX link opens the FX family */
static void sh_land(const sh_ctx_t *c, uint32_t k, int last)
{
    uint8_t pg[48];
    uint32_t at, n;
    if (c->s[k].op == SH_LINK) {
        open_family(FAM_FX);
        sh_fx = 1;
        return;
    }
    sh_fx = 0;
    if (c->s[k].op)
        fm6_opsel = (uint8_t)(c->s[k].op - 1u);
    n = sh_rows(c, k, pg, sizeof pg, &at);
    sh_show(n ? pg[last ? n - 1u : 0u] : c->s[k].first);
}
/* EDIT tapped (page_tap, after EDIT's other jobs): on a row the next sheet, the FX link included; on the track's FX pages
 * reached through the link the sheet after it; anywhere else (the shared effects too) the first sheet */
static void sh_edit(void)
{
    sh_ctx_t m;
    const sh_ctx_t *c = sh_get(&m);
    uint32_t k = c->cur;
    if (!c->n)
        return;
    if (k < c->n) {
        k = (k + 1u) % c->n;
    } else if (sh_fx && !ui.home && cur_page()->fam == FAM_FX && cur_page()->scope != SC_GLOBAL) {
        for (k = 0; k < c->n && c->s[k].op != SH_LINK; k++)
            ;
        k = k < c->n ? (k + 1u) % c->n : 0u;
    } else {
        k = 0;
    }
    sh_land(c, k, 0);
}
/* SELECT on a row: the next row, past the last the next sheet's first (backwards: the previous sheet's last); the FX
 * link skipped. 0: not on a row of a listed sheet (the caller scrolls the family, as today) */
static int sh_select(int32_t d)
{
    sh_ctx_t m;
    const sh_ctx_t *c = sh_get(&m);
    uint8_t pg[48];
    uint32_t k = c->cur, at, nr, j;
    if (k >= c->n)
        return 0;
    nr = sh_rows(c, k, pg, sizeof pg, &at);
    if (d > 0 && at + 1u < nr) {
        sh_show(pg[at + 1u]);
    } else if (d < 0 && at > 0u && at < nr) {
        sh_show(pg[at - 1u]);
    } else {
        for (j = 0; j < c->n; j++) {                    /* (n turns at most: the link is never the only sheet) */
            k = (k + (d > 0 ? 1u : c->n - 1u)) % c->n;
            if (c->s[k].op != SH_LINK)
                break;
        }
        sh_land(c, k, d < 0);
    }
    return 1;
}
/* PRESETS on a row: the next or previous sheet, the link skipped, no wrap (as the sections' turn); from one of FM6's
 * operators to the next the row is kept (only the operator changes, as release-2's operator turn: the diff review's 4).
 * 0: not on a row */
static int sh_turn(int32_t d)
{
    sh_ctx_t m;
    const sh_ctx_t *c = sh_get(&m);
    int32_t j;
    if (c->cur >= c->n)
        return 0;
    for (j = (int32_t)c->cur + (d > 0 ? 1 : -1); j >= 0 && j < (int32_t)c->n; j += d > 0 ? 1 : -1)
        if (c->s[j].op != SH_LINK) {
            if (c->s[j].op && c->s[c->cur].op && sh_fmop(ui.page)) {
                fm6_opsel = (uint8_t)(c->s[j].op - 1u);
                ui.force = 1;
            } else {
                sh_land(c, (uint32_t)j, 0);
            }
            return 1;
        }
    return 1;
}
/* the header's dots on a row: the sheet's rows (n) and the page's place (k); 0: not on a row */
static int sh_dots(uint32_t *n, uint32_t *k)
{
    sh_ctx_t m;
    const sh_ctx_t *c = sh_get(&m);
    uint8_t pg[48];
    if (c->cur >= c->n)
        return 0;
    *n = sh_rows(c, c->cur, pg, sizeof pg, k);
    return 1;
}
/* the tabs under the header (rows 30 .. 55); 0: not on a row */
static int sh_tabs(void)
{
    sh_ctx_t m;
    const sh_ctx_t *c = sh_get(&m);
    const char *nm[SH_MAX];
    uint32_t k, sig;
    if (c->cur >= c->n)
        return 0;
    for (k = 0, sig = c->n * 131u + c->cur * 7u; k < c->n; k++) {
        nm[k] = c->s[k].name;
        sig = sig * 33u + c->s[k].first + c->s[k].op;
    }
    tabs_draw(nm, c->n, c->cur, sig + ux.gen * 977u + ux.pal * 31u + song.sel + 0x5E7u);
    return 1;
}

/* ------------------------------------------------ the second row and the box (rows 108 .. 207) --- */
static uint32_t sh_sig2, sh_sigb;                       /* what they show (redrawn when it changes, or on ui.force) */
static uint32_t sh_hash(uint32_t h, const char *t)
{
    while (*t)
        h = h * 33u + (uint8_t)*t++;
    return h * 33u;
}
/* the RINGS row shown has another row in its sheet: that row (the next; on the last, the previous) in *other */
static int sh_other(uint32_t *other, uint32_t *k0)
{
    sh_ctx_t m;
    const sh_ctx_t *c;
    uint8_t pg[48];
    uint32_t at, nr;
    if (page_over)
        return 0;                                       /* (a layer's page over the row: no capture, the design review's c4) */
    c = sh_get(&m);
    if (c->cur >= c->n)
        return 0;
    nr = sh_rows(c, c->cur, pg, sizeof pg, &at);
    if (nr < 2u)
        return 0;
    *other = pg[at + 1u < nr ? at + 1u : at - 1u];
    *k0 = c->cur;
    return 1;
}
/* the other row, dimmed, at rows 108 .. 159: its four cells as its page draws them (captured, not drawn: the strip's
 * per-column caches are the shown row's, the review's c3) */
static void sh_second(uint32_t other)
{
    uint8_t pg = ui.page, cs = col_style;
    uint32_t c, sig;
    memset(lcap, 0, sizeof lcap);
    ui.page = (uint8_t)other;
    col_style = CS_CAPTURE;
    draw_columns();
    col_style = cs;
    ui.page = pg;
    for (c = 0, sig = other * 977u + ux.gen * 31u + ux.pal; c < 4u; c++)
        sig = sh_hash(sh_hash(sh_hash(sig + lcap[c].on, lcap[c].label), lcap[c].val), lcap[c].unit);
    if (!ui.force && sig == sh_sig2)
        return;
    sh_sig2 = sig;
    cv_begin(240, 52, T_BG);
    for (c = 0; c < 4u; c++) {
        int32_t x = CARD_X(c);
        char lb[12], v[16];
        if (!lcap[c].on)
            continue;
        str_cpy(lb, cell_label(lcap[c].label, COL_W - 4), sizeof lb);
        str_cpy(v, lcap[c].val, sizeof v);
        if (lcap[c].unit[0] && str_len(v) + str_len(lcap[c].unit) < sizeof v &&
            text_w(TY_TX, v) + text_w(TY_LB, lcap[c].unit) <= COL_W - 4)
            str_cpy(v + str_len(v), lcap[c].unit, sizeof v - str_len(v));   /* (the unit, as the framed row shows it) */
        cv_rrect(x, 2, COL_W, 48, 6, T_QUIET, T_BG);
        cv_text_c(x + COL_W / 2, ty_y(13, TY_LB), TY_LB, lb, T_DIM, T_QUIET);
        if (text_w(TY_TX, v) <= COL_W - 4)
            cv_text_c(x + COL_W / 2, ty_y(34, TY_TX), TY_TX, v, T_DIM, T_QUIET);
        else
            cv_text_c(x + COL_W / 2, ty_y(34, TY_LB), TY_LB, v, T_DIM, T_QUIET);
    }
    cv_blit(0, 108);
}
/* the box at rows 160 .. 207 on the first sheet of an engine with a knob table: HOME's four knobs in their colors,
 * and the first macro knob's targets (what the knob is made of: the mockup's "PLUCK: LPG DECY ATK REL") */
static void sh_box(uint32_t k0)
{
    const knob_def_t *kd = kdef_of(TSEL->eng_req);
    uint32_t k, t, sig;
    int32_t x;
    if (!kd || k0 != 0u)
        return;
    for (k = 0, sig = TSEL->eng_req * 131u + ux.gen * 31u + ux.pal; k < 4u; k++)
        sig = sh_hash(sig, kd[k].label ? kd[k].label : "");
    if (!ui.force && sig == sh_sigb)
        return;
    sh_sigb = sig;
    cv_begin(240, 48, T_BG);
    cv_rrect(4, 2, 232, 44, 6, T_QUIET, T_BG);
    for (k = 0, x = 14; k < 4u; k++)
        if (kd[k].label && x + text_w(TY_LB, kd[k].label) <= 228)
            x = cv_text(x, ty_y(14, TY_LB), TY_LB, kd[k].label, T_COZY(k)) + 8;
    for (k = 0; k < 4u && kd[k].kind != KD_MACRO; k++)
        ;
    if (k < 4u && kd[k].label) {                        /* "PLUCK: LPG DECY ATK REL" (each target's own label) */
        x = cv_text(12, ty_y(34, TY_LB), TY_LB, kd[k].label, T_SEC);
        x = cv_text(x, ty_y(34, TY_LB), TY_LB, ":", T_SEC);
        for (t = 0; t < KD_TGT; t++) {
            const param_desc_t *d;
            const char *lb;
            if (kd[k].t[t].id == KD_NOT || !(d = track_desc(TSEL, kd[k].t[t].id)) || !d->label)
                continue;
            lb = cell_label(d->label, 48);
            if (x + 6 + text_w(TY_LB, lb) > 228)
                break;
            x = cv_text(x + 6, ty_y(34, TY_LB), TY_LB, lb, T_SEC);
        }
    }
    cv_blit(0, 160);
}
/* pv_draw's picture on a RINGS row: the other row and the box when the sheet has another row; 0: the picture as today */
static int sh_lower(void)
{
    uint32_t other, k0;
    if (!sh_other(&other, &k0)) {
        sh_sig2 = sh_sigb = 0;                          /* (the next time they show, drawn whole: the diff review's 5) */
        return 0;
    }
    sh_second(other);
    sh_box(k0);
    return 1;
}
