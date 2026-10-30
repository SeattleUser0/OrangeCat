/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 5 (docs/orangecat/UI_BUILD.md "Step 5: EDIT > LANES") with OC_LANES=1, through tests/ui_test.c's host UI.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o lanes_test tests/lanes_test.c -lm
 * Checked:
 *   PAGE    DRUM-X's LANES page is the table page (GR_LANES); LANES 2 and 3 are as before; no other engine has it
 *   PICK    PRESETS picks the lane (both ways, held at the ends); the knobs edit the picked lane's LEVEL, TUNE, DECAY, PAN and
 *           no other lane's
 *   KEYS    a key struck on the track still picks its lane
 *   DRAW    the table drawn for every lane picked (the page's signature changes with a lane's value)
 * Revisions:
 *   (the new interface 0.5, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("lanes: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
int main(void)
{
#if !OC_LANES || !MELODEE_DRUMX
    printf("lanes: built without OC_LANES\n");
    return 0;
#else
    uint32_t l, k, i, x, y;
    int32_t before[8][4];
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    stop_transport();
    set_engine_of(TSEL, ENGI_DRUMX);
    TSEL->engine = TSEL->eng_req;
    frames(100);
    for (i = 0; i < NPAGES && !(PAGES[i].scope == SC_DXLANE && str_eq(PAGES[i].title, "LANES")); i++) ;
    chk("PAGE    DRUM-X's LANES page is the table page; LANES 2 and 3 keep release-2's", i < NPAGES && PAGES[i].graph == GR_LANES && PAGES[i + 1u].graph == GR_NONE && PAGES[i + 2u].graph == GR_NONE);
    ui.home = 0; ui.page = (uint8_t)i; page_entered(); ui.lane = 0; ui.force = 1; frame();
    chk("PAGE    the page is shown for DRUM-X", lanes_page() && page_visible(i));
    for (l = 0; l < 8u; l++) { drum_pick_lane(l); frame(); }
    drum_pick_lane(0);
    turn(EN_PRESET, 1);
    chk("PICK    PRESETS picks the next lane", ui.lane == 1u);
    turn(EN_PRESET, -1); turn(EN_PRESET, -1);
    chk("PICK    PRESETS held at the first lane", ui.lane == 0u);
    for (k = 0; k < 10u; k++) turn(EN_PRESET, 1);
    chk("PICK    PRESETS held at the eighth lane", ui.lane == 7u);
    drum_pick_lane(2); frame();
    for (l = 0; l < 8u; l++) for (k = 0; k < 4u; k++) before[l][k] = dxl_value(song.sel, l, k);
    turn(EN_K1, -5); turn(EN_K2, 3); turn(EN_K3, -4); turn(EN_K4, 6);
    {
        int ok = dxl_value(song.sel, 2, DXL_LEVEL) == before[2][0] - 5 && dxl_value(song.sel, 2, DXL_TUNE) != before[2][1] &&
                 dxl_value(song.sel, 2, DXL_DECAY) != before[2][2] && dxl_value(song.sel, 2, DXL_PAN) != before[2][3];
        for (l = 0; l < 8u; l++) if (l != 2u) for (k = 0; k < 4u; k++) ok &= dxl_value(song.sel, l, k) == before[l][k];
        chk("PICK    the four knobs edit lane 3's LEVEL, TUNE, DECAY and PAN, no other lane's", ok);
    }
    {
        uint32_t s0 = lanes_sig();
        turn(EN_K2, 2);
        chk("DRAW    the page's signature follows a kit value", lanes_sig() != s0);
    }
    {
        int ok = 1;
        uint32_t prev = 0;
        for (l = 0; l < 8u; l++) {
            uint32_t h = 2166136261u;
            drum_pick_lane(l); ui.force = 1; frames(32);
            for (y = 100; y < 210u; y++) for (x = 0; x < 240u; x++) h = (h ^ host_screen[y * 240u + x]) * 16777619u;
            ok &= h != prev;                            /* (the picked lane's row moves: the table is drawn and redrawn) */
            prev = h;
        }
        chk("DRAW    the table under the sheet's tabs changes with each lane picked", ok);
        ok = 0;
        for (y = 30; y < 60u; y++) for (x = 0; x < 240u; x++) ok |= host_screen[y * 240u + x] != host_screen[40u * 240u + 1u] ;
        chk("DRAW    the sheets' tab strip is still drawn above the table", ok);
    }
    {   /* KEYS: the lane a key struck picks */
        uint32_t key = 5, want = dxe_lane((uint32_t)ENG_DRUMX.keys(TSEL, key));
        drum_pick_lane(want == 0u ? 1u : 0u);
        frame();
        fm1_in.notes = host_notes = 1u << key;
        frame();
        fm1_in.notes = 0;
        frame();
        chk("KEYS    a key struck still picks its lane (release-2's rule)", ui.lane == want);
    }
    printf("lanes: %d failed\n", nbad);
    return nbad != 0;
#endif
}
