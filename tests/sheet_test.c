/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 2 (docs/orangecat/UI_BUILD.md "Step 2: EDIT's sheets"): EDIT's sheets with OC_EDIT_SHEETS=1
 * (firmware/src/ui_sheets.c), through tests/ui_test.c's host UI (its own tests are not run here: main is ours).
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o sheet_test tests/sheet_test.c -lm
 * Checked, on every engine the build has:
 *   COVER   SELECT from SOUND's first row walks every page release-2's navigation reached (its own page_scroll over the
 *           EDIT, ENV and LFO families, and the ENV and LFO buttons' lists) and comes back to where it started; on FM6
 *           every one of the six operators; turned back the same; nothing else is visited
 *   EDIT    EDIT walks the sheets in order (the FX link opens the FX family; EDIT from there the sheet after it) and
 *           comes back to SOUND; EDIT from HOME and from FX opened by its own button: SOUND
 *   FAMLAST fam_last never holds another family's page (the review's b2)
 *   PRESETS PRESETS turns the sheets (the link skipped), back, and stops at the ends; on FM6's operators it keeps the row
 *   FXLINK  EDIT from the track's FX pages reached through the link: the sheet after it; from a shared effect, or after
 *           another button: the first sheet
 *   SHORT   ENV and LFO land on rows of sheets
 *   KNOBS   KNOB 1 on SOUND's first row edits its first id; EDIT held undoes it
 *   OTHERS  EDIT's other jobs first: SAVE > USER's rename; SEQ's first page keeps EDIT
 *   ENGINE  the engine changed under a row (the Prophet, FM6 or CZ-1 hiding ENV): no stale sheet, SELECT still moves
 *   DRAW    every row drawn (no crash; the second row's capture leaves the page shown as it was)
 * Revisions:
 *   (the new interface 0.2, branch ui-build) first version; after the diff review: release-2's own walk as the oracle,
 *   the operators, PRESETS' ends and FM6's row, the FX link's lifetime, ENGINE not falling to HOME.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("sheets: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static int engine_in(uint32_t e)
{
    return e < NENGINES && ENGINES[e] && eng_ok(e) && e != ENGI_SLICE;
}
/* the pages release-2's navigation reached on this track: its own page_scroll round each of EDIT, ENV and LFO from the
 * family's first page, and the ENV and LFO buttons' lists (fam_pages: the engine's own pages first) */
static void release2_reach(uint8_t *r)
{
    static const uint32_t F[3] = {FAM_EDIT, FAM_ENV, FAM_LFO};
    uint8_t pg[NPAGES_ALL];
    uint32_t f, k, n, first;
    memset(r, 0, NPAGES_ALL);
    for (f = 0; f < 3u; f++) {
        first = page_first(F[f]);
        if (page_at(first)->fam != F[f] || !page_visible(first))
            continue;
        ui.home = 0;
        ui.page = (uint8_t)first;
        for (k = 0; k < 400u; k++) {
            r[ui.page] = 1;
            page_scroll(1);
            if (ui.page == first)
                break;
        }
        if (F[f] != FAM_EDIT)
            for (n = fam_pages(F[f], pg), k = 0; k < n; k++)
                r[pg[k]] = 1;
    }
    go_home();
}
static int on_sheet(const sh_ctx_t *c, uint32_t k)     /* the page shown is a row of sheet k (an operator's: its operator) */
{
    if (c->s[k].op && c->s[k].op != SH_LINK)
        return sh_fmop(ui.page) && fm6_opsel == c->s[k].op - 1u;
    return !ui.home && sh_in(c, k, ui.page);
}

int main(void)
{
#if !OC_EDIT_SHEETS
    printf("sheets: built without OC_EDIT_SHEETS\n");
    return 0;
#else
    uint32_t e, engines = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (e = 0; e < NENGINES; e++) {
        static uint8_t seen[NPAGES_ALL], want[NPAGES_ALL];
        static sh_ctx_t c;
        char what[140];
        const char *en;
        uint32_t n, i, k, steps, start, bad_vis = 0, miss = 0, back = 0, fl = 1, ops = 0, has_ops = 0;
        if (!engine_in(e))
            continue;
        engines++;
        en = ENGINES[e]->name;
        ui_power_on();
        set_engine_of(TSEL, e);
        frames(100);
        release2_reach(want);
        go_home();
        sh_make(&c);
        n = c.n;
        /* COVER: SELECT forward from EDIT's landing (the first sheet: never FM6's operator pages, so the page alone says
         * the walk came round; fm6_opsel stays as left) */
        press(B_EDIT);
        start = ui.page;
        memset(seen, 0, sizeof seen);
        for (steps = 0; steps < 600u; steps++) {
            seen[ui.page] = 1;
            if (sh_fmop(ui.page))
                ops |= 1u << (fm6_opsel % 6u), has_ops = 1;
            if (!want[ui.page] || ui.home)
                bad_vis++;
            for (k = 0; k < FAM_COUNT; k++)
                if (ui.fam_last[k] && page_at(ui.fam_last[k])->fam != k)
                    fl = 0;
            turn(EN_SELECT, 1);
            if (ui.page == start)
                break;
        }
        for (i = 0; i < page_n(); i++)
            if (want[i] && !seen[i])
                miss++;
        snprintf(what, sizeof what, "COVER   %-10s %2u sheets: SELECT walks release-2's every page, back in %u (missed %u, other %u)", en, n, steps + 1u, miss, bad_vis);
        chk(what, n >= 2u && steps < 600u && !miss && !bad_vis);
        if (has_ops) {
            snprintf(what, sizeof what, "COVER   %-10s every operator's rows walked (OP1 .. OP6)", en);
            chk(what, ops == 0x3Fu);
        }
        for (steps = 0; steps < 600u; steps++) {
            turn(EN_SELECT, -1);
            if (ui.page == start) {
                back = 1;
                break;
            }
        }
        snprintf(what, sizeof what, "COVER   %-10s SELECT turned back comes round too", en);
        chk(what, back);
        /* EDIT: the sheets in order */
        {
            int ok = 1;
            go_home();
            frames(50);
            press(B_EDIT);
            ok &= on_sheet(&c, 0);
            for (k = 1; k <= n; k++) {
                press(B_EDIT);
                if (k == n)
                    ok &= on_sheet(&c, 0);
                else if (c.s[k].op == SH_LINK)
                    ok &= cur_page()->fam == FAM_FX && sh_fx;
                else
                    ok &= on_sheet(&c, k);
            }
            for (k = 0; k < FAM_COUNT; k++)
                if (ui.fam_last[k] && page_at(ui.fam_last[k])->fam != k)
                    fl = 0;
            snprintf(what, sizeof what, "EDIT    %-10s EDIT walks the %u sheets (FX the link) and back to the first", en, n);
            chk(what, ok);
            press(B_FX);
            press(B_EDIT);
            snprintf(what, sizeof what, "EDIT    %-10s FX by its own button, then EDIT: the first sheet", en);
            chk(what, on_sheet(&c, 0));
        }
        snprintf(what, sizeof what, "FAMLAST %-10s fam_last holds only its own family's pages", en);
        chk(what, fl);
        /* FXLINK */
        {
            uint32_t link, after;
            int ok = 1;
            for (link = 0; link < n && c.s[link].op != SH_LINK; link++)
                ;
            after = (link + 1u) % n;
            go_home();
            press(B_EDIT);
            while (!(cur_page()->fam == FAM_FX) && ok) {   /* EDIT up to the link */
                press(B_EDIT);
                ok &= !ui.home;
            }
            press(B_EDIT);
            ok &= on_sheet(&c, after);
            go_home();                                  /* through the link, then SELECT onto a shared effect */
            press(B_EDIT);
            while (cur_page()->fam != FAM_FX && !ui.home)
                press(B_EDIT);
            for (k = 0; k < 40u && cur_page()->scope != SC_GLOBAL; k++)
                turn(EN_SELECT, 1);
            ok &= cur_page()->fam == FAM_FX && cur_page()->scope == SC_GLOBAL;
            press(B_EDIT);
            ok &= on_sheet(&c, 0);
            go_home();                                  /* through the link, another button, then FX's own: EDIT the first */
            press(B_EDIT);
            while (cur_page()->fam != FAM_FX && !ui.home)
                press(B_EDIT);
            press(B_LFO);
            press(B_FX);
            press(B_EDIT);
            ok &= on_sheet(&c, 0);
            snprintf(what, sizeof what, "FXLINK  %-10s EDIT after the link: the next sheet; from a shared effect or another button: the first", en);
            chk(what, link < n && ok);
        }
        /* PRESETS */
        {
            uint32_t want1 = 1;
            int ok;
            go_home();
            press(B_EDIT);
            while (want1 < n && c.s[want1].op == SH_LINK)
                want1++;
            turn(EN_PRESET, 1);
            ok = want1 < n && on_sheet(&c, want1);
            turn(EN_PRESET, -1);
            ok &= on_sheet(&c, 0);
            turn(EN_PRESET, -1);
            ok &= on_sheet(&c, 0);                      /* (the first end: no wrap) */
            for (k = 0; k < n + 2u; k++)
                turn(EN_PRESET, 1);
            ok &= on_sheet(&c, n - 1u - (c.s[n - 1u].op == SH_LINK));   /* (the last end) */
            snprintf(what, sizeof what, "PRESETS %-10s PRESETS: the next sheet (the link skipped), back, no wrap at either end", en);
            chk(what, ok);
        }
        if (has_ops) {                                  /* FM6: PRESETS from an operator's row keeps the row */
            uint32_t pg, op1;
            for (op1 = 0; op1 < n && c.s[op1].op != 1u; op1++)
                ;
            go_home();
            press(B_EDIT);
            while (!on_sheet(&c, op1) && !ui.home)
                press(B_EDIT);
            turn(EN_SELECT, 1);
            turn(EN_SELECT, 1);
            pg = ui.page;
            turn(EN_PRESET, 1);
            snprintf(what, sizeof what, "PRESETS %-10s on OP1's third row PRESETS: OP2, the same row", en);
            chk(what, ui.page == pg && fm6_opsel == 1u && sh_fmop(ui.page));
        }
        /* ENV, LFO */
        go_home();
        press(B_ENV);
        {
            int ok = sh_row(ui.page) && !ui.home;
            press(B_LFO);
            ok &= sh_row(ui.page) && !ui.home;
            snprintf(what, sizeof what, "SHORT   %-10s ENV and LFO land on rows of sheets", en);
            chk(what, ok);
        }
        /* DRAW: every row of every sheet, the page shown unchanged by the capture */
        {
            int ok = 1;
            go_home();
            press(B_EDIT);
            for (steps = 0; steps < 600u; steps++) {
                uint32_t pg = ui.page;
                ui.force = 1;
                frame();
                frame();
                ok &= ui.page == pg && !sh_now;
                turn(EN_SELECT, 1);
                if (ui.page == start)
                    break;
            }
            snprintf(what, sizeof what, "DRAW    %-10s every row drawn; the capture leaves the page shown as it was", en);
            chk(what, ok);
        }
    }
    chk("engines: at least MACRO, SHAPES and DRUM-X tried", engines >= 3u);
    {   /* KNOBS, undo */
        uint32_t id;
        int16_t v0;
        ui_power_on();
        set_engine_of(TSEL, ENGI_MACRO);
        frames(100);
        press(B_EDIT);
        id = cur_page()->id[0];
        v0 = TSEL->p[id];
        turn(EN_K1, 3);
        frames(50);
        {
            int moved = TSEL->p[id] != v0;
            hold(B_EDIT);
            frames(50);
            chk("KNOBS   KNOB 1 on SOUND's first row edits its first id; EDIT held undoes it", moved && TSEL->p[id] == v0 && !ui.home);
        }
    }
    {   /* OTHERS: SAVE > USER, EDIT renames */
        ui_power_on();
        frames(100);
        press(B_SAVE);
        frames(50);
        {
            int user = cur_page()->graph == GR_USER;
            press(B_EDIT);
            frames(50);
            chk("OTHERS  SAVE > USER: EDIT is still the rename (an empty slot: EMPTY SLOT), not a sheet",
                user && cur_page()->graph == GR_USER && msg_is("EMPTY SLOT"));
        }
    }
    {   /* OTHERS: SEQ's pages (NOTES, the DRUM grid: EDIT clears or is the step modifier there): EDIT leaves them be */
        int ok;
        ui_power_on();
        frames(100);
        press(B_SEQ);
        frames(50);
        ok = !ui.home && cur_page()->fam == FAM_SEQ;
        press(B_EDIT);
        frames(50);
        chk("OTHERS  SEQ's first page: EDIT keeps its job there, no sheet opened", ok && cur_page()->fam == FAM_SEQ && !sh_row(ui.page));
    }
    {   /* ENGINE: the engine changed under an Env row */
        uint32_t to = MELODEE_P5 ? ENGI_PROPHET : MELODEE_FM6 ? ENGI_FM6 : MELODEE_CZ ? ENGI_CZ : NENGINES;
        ui_power_on();
        set_engine_of(TSEL, ENGI_MACRO);
        frames(100);
        press(B_ENV);
        if (to < NENGINES && engine_in(to)) {
            uint32_t pg;
            set_engine_of(TSEL, to);
            frames(100);
            pg = ui.page;
            turn(EN_SELECT, 1);
            frames(50);
            chk("ENGINE  an engine with its own ENV under an Env row: drawn, SELECT still moves (not to HOME)", ui.page != pg && !ui.home);
        } else {
            uint32_t pg;
            set_engine_of(TSEL, ENGI_DRUMX);
            frames(100);
            pg = ui.page;
            turn(EN_SELECT, 1);
            frames(50);
            chk("ENGINE  (no Prophet, FM6 or CZ-1 here) DRUM-X under an Env row: drawn, SELECT still moves (not to HOME)", ui.page != pg && !ui.home);
        }
    }
    printf("sheets: %d failed\n", nbad);
    return nbad != 0;
#endif
}
