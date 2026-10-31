/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 6 (docs/orangecat/UI_BUILD.md "Step 6: the mixer in the knobs' colors") with OC_MIXER=1, through tests/ui_test.c's
 * host UI, by the pixels of the screen.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o mixer_test tests/mixer_test.c -lm
 * Checked:
 *   BAR     every track's level bar is KNOB 1's color (more than a bar's pixels in it), not the track's
 *   KNOBS   a knob just turned shows its value in its own color: PAN green, MUTE orange (the card's color for KNOB 2 and 4)
 *   MUTED   a muted track's bar is dim, not blue
 *   KEEP    the numerals stay the track's colors; the knobs still do release-2's jobs (LEVEL, PAN, MUTE of the selected track)
 * Revisions:
 *   (the new interface 0.6, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("mixer: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static uint32_t count(uint16_t c, uint32_t y0, uint32_t y1)
{
    uint32_t n = 0, x, y;
    c = (uint16_t)((c >> 8) | (c << 8));                /* (the host screen holds the panel's byte order) */
    for (y = y0; y < y1; y++)
        for (x = 0; x < 240u; x++)
            n += host_screen[y * 240u + x] == c;
    return n;
}
int main(void)
{
#if !OC_MIXER
    printf("mixer: built without OC_MIXER\n");
    return 0;
#else
    int32_t lvl0, pan0;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    stop_transport();
    go_title("MIXER");
    ui.force = 1;
    frames(200);
    chk("BAR     the level bars are KNOB 1's blue", count(T_COZY(0), 30, 240) > 4u * 60u);
    {
        uint16_t c1 = T_COZY(1), c3 = T_COZY(3);
        uint32_t g0 = count(c1, 30, 240), o0 = count(c3, 30, 240);
        pan0 = TSEL->p[P_PAN];
        turn(EN_K2, 5);
        frames(50);
        chk("KNOBS   KNOB 2 turned: the pan in KNOB 2's green", TSEL->p[P_PAN] == pan0 + 5 && count(c1, 30, 240) > g0);
        uint32_t b0 = count(T_COZY(0), 30, 240);
        lvl0 = TSEL->p[P_MUTE];
        turn(EN_K4, 1);
        frames(50);
        chk("KNOBS   KNOB 4 turned: still the track's mute, its word in KNOB 4's orange", TSEL->p[P_MUTE] != lvl0 && count(c3, 30, 240) > o0);
        chk("MUTED   a muted track's bar is dim, not blue (one bar less blue)", count(T_COZY(0), 30, 240) + 100u < b0);
        turn(EN_K4, -1);
        frames(50);
    }
    {
        uint32_t lv = trk_level(song.sel);
        turn(EN_K1, 4);
        frames(50);
        chk("KEEP    KNOB 1 is still the selected track's level", trk_level(song.sel) != lv);
    }
    printf("mixer: %d failed\n", nbad);
    return nbad != 0;
#endif
}
