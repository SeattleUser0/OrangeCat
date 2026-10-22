/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.5.3: every page Orange Cat's own build can reach, walked as a player would, under the address and
 * undefined-behaviour sanitizers.
 *   Built with the engine switches of tools/orangecat_engines.sh (SID, CZ-1, FM6, PROPHET, DRUM out; MACRO, SHAPES in):
 *   the other host tests build every engine in, which is how a crash on the unit got past them. 0.5.2 on the unit:
 *   track 1 on CHORD MINOR, EDIT, SELECT two clicks back: the Prophet's pages (unhooked) were shown and read past the
 *   parameters (ui.c page_visible).
 * For each engine shown and a spread of its presets: each family button (EDIT, ENV, LFO, SCL, GLO, SAVE, ARP, SEQ,
 * FX), then SELECT back through 40 pages and forward through 40, every frame drawn. Checked: every page reached is
 * one page_visible allows and none belongs to an unhooked engine (a sanitizer stops the run on any bad read). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int oc_page_ok(void)
{
    const page_t *p;
    if (ui.home || ui.menu) return 1;
    p = cur_page();
    if (!page_visible(ui.page)) return 0;
    if (!MELODEE_P5 && (p->scope == SC_P5 || p->scope == SC_P5STORE)) return 0;
    if (!MELODEE_FM6 && (p->scope == SC_FM6 || p->scope == SC_FMOP)) return 0;
    if (!MELODEE_DRUM && (p->scope == SC_DRUM || p->scope == SC_DRUMHIT)) return 0;
    if (!MELODEE_CZ && p->scope == SC_CZ1) return 0;
    return 1;
}

int main(void)
{
    static const uint32_t B[] = {B_EDIT, B_ENV, B_LFO, B_SCL, B_GLO, B_SAVE, B_ARP, B_SEQ, B_FX};
    uint32_t n, k, i, b, frames = 0, pages = 0, bad = 0;
    uint8_t seen[NPAGES];
    setvbuf(stdout, NULL, _IONBF, 0);
    memset(seen, 0, sizeof seen);
    ui_power_on();
    stop_transport();
    for (n = 0; n < NENG_SHOWN; n++) {
        uint32_t e = eng_vis(n), np = ENGINES[e]->npresets ? ENGINES[e]->npresets : 1u;
        for (k = 0; k < np; k += k + 4u < np ? 4u : (np - k > 1u ? np - k - 1u : 1u)) {
            host_preset(TSEL, e, k);
            go_home();
            frame();
            for (b = 0; b < NELEM(B); b++) {
                press(B[b]);
                for (i = 0; i < 80u; i++) {
                    turn(EN_SELECT, i < 40u ? -1 : 1);
                    frames++;
                    if (!oc_page_ok()) {
                        if (bad < 10u) printf("oc_pages: %s preset %u: page %u (%s) should not be shown\n",
                                              ENGINES[e]->name, k, ui.page, cur_page()->title);
                        bad++;
                    }
                    if (!ui.home && !seen[ui.page]) { seen[ui.page] = 1; pages++; }
                }
                go_home();
                frame();
            }
        }
    }
    printf("oc_pages: %u engines, %u frames, %u pages reached, %s\n", (unsigned)NENG_SHOWN, frames, pages,
           bad ? "FAILED" : "all allowed");
    return bad != 0;
}
