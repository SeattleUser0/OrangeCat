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
static const char *const ENGS[4] = {"MACRO", "SEM", "DRUM-X", "FMBELL"};
typedef struct { const char *tag; uint8_t playing, mute, off, frozen, wait; } scn_t;   /* the track with the state: 3 mute, 4 off, 1 frozen, 4 wait */
static const scn_t SCN[6] = {{"rest", 0, 0, 0, 0, 0}, {"play", 1, 0, 0, 0, 0}, {"mute", 1, 1, 0, 0, 0}, {"off", 1, 0, 1, 0, 0},
                             {"frozen", 1, 0, 0, 1, 0}, {"wait", 1, 0, 0, 0, 1}};
static void make(ms_t *m, const scn_t *s)
{
    static const uint16_t STEPS[4] = {0x1111u, 0x0A5Au, 0x8888u | 0x0808u, 0xFFFFu & 0x6DB6u};   /* a bass line, a pattern, a four on the floor, a bell */
    static const knob_t *const K[4] = {0};
    memset(m, 0, sizeof *m);
    for (uint32_t k = 0; k < 4u; k++) { m->r[k] = NAMES[k]; m->steps[k] = STEPS[k]; m->engine[k] = ENGS[k]; }
    m->playing = s->playing; m->sel = 1; m->song = 2; m->bar = s->playing ? 5 : 1; m->bpm = 124; m->pos = s->playing ? 6 : 0;
    if (s->mute) m->r[2].state = MS_MUTE;
    if (s->off) m->r[3].state = MS_OFF;
    if (s->frozen) m->r[0].state = MS_FROZEN;
    if (s->wait) m->r[3].state = MS_WAIT;
    for (uint32_t k = 0; k < 4u; k++) {              /* the selected track's (SEM) four knobs */
        const knob_t *kn = &S_SEM.k[k];
        m->k[k].lab = kn->name; m->k[k].macro = kn->macro; m->k[k].q8 = (int16_t)knob_q8(kn);
        knob_text(kn, m->k[k].val);
    }
}
static void ms_dial(int32_t cx, int32_t cy, int32_t d, uint32_t k, int32_t q8, int macro)
{
    knob_t kn = {"K", 0, 256, (uint8_t)macro, (int16_t)q8, 0, 0};
    dial(cx, cy, d, k, &kn, 0);
}
static uint16_t cozy(uint32_t k)     /* the knobs' plastic: blue, green, white, orange; deeper on a light ground (gfx.c palette_set under OC_HOME_PIC) */
{
    static const uint16_t DARK[4] = {RGB(77, 158, 255), RGB(0, 226, 140), RGB(236, 236, 236), RGB(255, 128, 32)};
    static const uint16_t LIGHT[4] = {RGB(16, 84, 196), RGB(0, 112, 64), 0, RGB(176, 72, 0)};
    return ux.mono ? ux_mix(T_BG, T_TEXT, 100 - 12 * (int32_t)k) : ux.light ? (k == 2u ? T_TEXT : LIGHT[k]) : DARK[k];
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
    fprintf(rep, "The main screen's three layouts (tests/ui_main_mockup.c): %u palettes, 6 states, 3 layouts\n\n", pals);
    for (uint32_t p = 0; p < pals; p++) {
        palette_set(p);
        for (uint32_t s = 0; s < 6u; s++) {
            ms_t m;
            make(&m, &SCN[s]);
            palette_track(m.sel);
            for (uint32_t k = 0; k < 4u; k++) KCOL[k] = cozy(k);   /* the knobs' plastic (1 and 3) */
            begin_screen(); ms_draw_1(&m);
            snprintf(name, sizeof name, "M1_%s_p%u", SCN[s].tag, p); shot(dir, name);
            for (uint32_t k = 0; k < 4u; k++) KCOL[k] = T_KNOB(k);   /* the selected track's own shades (2) */
            begin_screen(); ms_draw_2(&m);
            snprintf(name, sizeof name, "M2_%s_p%u", SCN[s].tag, p); shot(dir, name);
            for (uint32_t k = 0; k < 4u; k++) KCOL[k] = cozy(k);
            begin_screen(); ms_draw_3(&m);
            snprintf(name, sizeof name, "M3_%s_p%u", SCN[s].tag, p); shot(dir, name);
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
