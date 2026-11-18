/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The main track screen's three layouts (docs/orangecat/UI_DESIGN.md "The main screen"), drawn through the firmware's own
 * drawing code (tests/ui_main_screens.h, gfx.c) at 240 x 240, each resting, playing, and with a MUTE, an OFF, a FROZEN and a WAIT,
 * and every one linted (text off the screen, cut, overlapping) and checked for contrast in every palette.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o ui_main_mockup tests/ui_main_mockup.c -lm
 *   ./ui_main_mockup OUTDIR ; python3 tests/ui_main_mockup.py OUTDIR docs/orangecat/ui
 */
#define UI_TEST_PALETTE 1
#define main ui_mockup_main
#include "ui_mockup.c"
#undef main
#include "ui_main_screens.h"

static const ms_row_t NAMES[4] = {{"VA SYNC LEAD", 0}, {"SEM BRASS", 0}, {"909 CLUB", 0}, {"FM BELL", 0}};
typedef struct { const char *tag; uint8_t playing, mute, off, frozen, wait; } scn_t;   /* the track with the state: 3 mute, 4 off, 1 frozen, 4 wait */
static const scn_t SCN[6] = {{"rest", 0, 0, 0, 0, 0}, {"play", 1, 0, 0, 0, 0}, {"mute", 1, 1, 0, 0, 0}, {"off", 1, 0, 1, 0, 0},
                             {"frozen", 1, 0, 0, 1, 0}, {"wait", 1, 0, 0, 0, 1}};
static void make(ms_t *m, const scn_t *s)
{
    memset(m, 0, sizeof *m);
    for (uint32_t k = 0; k < 4u; k++) m->r[k] = NAMES[k];
    m->playing = s->playing; m->sel = 1; m->song = 2; m->bar = s->playing ? 5 : 1; m->bpm = 124;
    if (s->mute) m->r[2].state = MS_MUTE;
    if (s->off) m->r[3].state = MS_OFF;
    if (s->frozen) m->r[0].state = MS_FROZEN;
    if (s->wait) m->r[3].state = MS_WAIT;
}
/* C page 1: the state track is the selected one (the picture shows one track) */
static sound_t *SND[4] = {&S_MACRO, &S_SEM, &S_DRUMX, &S_FMBELL};
static void draw_c1(ms_t *m)
{
    sound_t *s = SND[m->sel];
    s->track = m->sel;
    palette_track(m->sel);
    ms_head_c(m);
    ms_line_c(m, m->r[m->sel].name);
    dial_strip(s, -1);
    picture(s, Y_DIALS + H_DIALS, 240 - Y_DIALS - H_DIALS, m->playing ? &F_NOTE : &F_REST);
}
static uint32_t lum(uint16_t c) { return ux_luma(c); }
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/ui_main";
    char path[512], name[96];
    uint32_t pals = UI_PAL_ENTRIES, low = 0;
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rep = fopen(path, "w");
    if (!rep) { fprintf(stderr, "ui_main_mockup: cannot write %s\n", path); return 1; }
    KCOL[0] = RGB(77, 158, 255); KCOL[1] = RGB(0, 226, 140); KCOL[2] = RGB(236, 236, 236); KCOL[3] = RGB(255, 128, 32);
    fprintf(rep, "The main screen's three layouts (tests/ui_main_mockup.c): %u palettes, 6 states, 4 screens\n\n", pals);
    for (uint32_t p = 0; p < pals; p++) {
        palette_set(p);
        for (uint32_t s = 0; s < 6u; s++) {
            ms_t m;
            make(&m, &SCN[s]);
            palette_track(m.sel);
            begin_screen(); ms_draw_a(&m);
            snprintf(name, sizeof name, "A_%s_p%u", SCN[s].tag, p); shot(dir, name);
            begin_screen(); ms_draw_b(&m);
            snprintf(name, sizeof name, "B_%s_p%u", SCN[s].tag, p); shot(dir, name);
            if (s >= 2u) {                                       /* the state is on the selected track for C's picture */
                uint32_t k = s == 2 ? 2 : s == 3 ? 3 : s == 4 ? 0 : 3;
                m.sel = (uint8_t)k;
            }
            begin_screen(); draw_c1(&m);
            snprintf(name, sizeof name, "C1_%s_p%u", SCN[s].tag, p); shot(dir, name);
            make(&m, &SCN[s]); palette_track(m.sel);
            begin_screen(); ms_draw_a(&m);                      /* C's second page is layout A */
            snprintf(name, sizeof name, "C2_%s_p%u", SCN[s].tag, p); shot(dir, name);
        }
        /* contrast: each ink of the screens against the ground, the luma difference (0..255); 90 is the floor that read at arm's length */
        {
            uint16_t ink[7] = {T_TEXT, T_MID, T_DIM, ms_ink(MS_MUTE), ms_ink(MS_OFF), ms_ink(MS_FROZEN), ms_ink(MS_WAIT)};
            static const char *const IN[7] = {"text", "mid", "dim", "MUTE", "OFF", "FROZEN", "WAIT"};
            fprintf(rep, "palette %u:", p);
            for (uint32_t i = 0; i < 7u; i++) {
                int32_t d = (int32_t)lum(ink[i]) - (int32_t)lum(T_BG);
                if (d < 0) d = -d;
                fprintf(rep, " %s %d", IN[i], d);
                if (i != 2u && d < 90) { low++; fprintf(rep, "(LOW)"); }       /* (dim is the one ink that is meant to recede) */
            }
            fprintf(rep, "\n");
        }
    }
    fprintf(rep, "\nlint findings: %u; low contrast inks: %u\n", nfind, low);
    fclose(rep);
    printf("ui_main_mockup: %u lint findings, %u low-contrast inks\n", nfind, low);
    return nfind || low ? 1 : 0;
}
