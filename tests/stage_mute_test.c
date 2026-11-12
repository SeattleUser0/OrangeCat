/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* (round 24) a muted track says MUTE (firmware/src/ui_stage.c stage_row, ui_home_pic.c hp_sound_line). The owner's finding, "Track 4 graying out": a track muted by accident showed
 * only as a grey row, which read as a fault. The texts are read as the drawing code hands them to the screen (gfx.c GFX_HOOK_TEXT, placed by GFX_HOOK_BLIT, as tests/ui_render.c does).
 *   ROWS     HOME's TRACKS page with four tracks (the short rows), two (the tall rows) and one (the wide card; UNUSED TRACKS = HIDE): the muted track's row holds the word MUTE, no
 *            other row does; unmuted again: gone; a mute that arrives with nothing forced draws the row again
 *   FROZEN   a frozen phrase's row says FROZEN; frozen and muted: MUTE (the mute is what silences it now)
 *   LONG     four rows and a sound name too long for the name and the role on one line (the role word is dropped): muted, the row still says MUTE and the name is cut; frozen, FROZEN
 *   PICTURE  HOME PAGE = PICTURE: the picture page's and the DUB page's sound line say MUTE while the selected track is muted, not otherwise
 * Run by tests/run_tests.sh. */
#include <stdint.h>
#include <string.h>
typedef struct { int16_t x0, y0, x1, y1; char s[28]; } sm_box_t;
static sm_box_t sm_pend[128], sm_scr[2048];
static uint32_t sm_npend, sm_nscr;
static void sm_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s)
{
    if (sm_npend >= 128u) return;
    sm_pend[sm_npend] = (sm_box_t){(int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, {0}};
    strncpy(sm_pend[sm_npend].s, s, 27);
    sm_npend++;
}
static void sm_blit(uint32_t x, uint32_t y)             /* the canvas sent: its texts are on the screen at (x, y) */
{
    for (uint32_t i = 0; i < sm_npend && sm_nscr < 2048u; i++) {
        sm_box_t b = sm_pend[i];
        b.x0 += (int16_t)x; b.x1 += (int16_t)x; b.y0 += (int16_t)y; b.y1 += (int16_t)y;
        sm_scr[sm_nscr++] = b;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) sm_text(x0, y0, x1, y1, s)
#define GFX_HOOK_BLIT(x, y, r0) sm_blit(x, y)
#define GFX_HOOK_BEGIN() (sm_npend = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("stage_mute: %-150s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void redraw(void)                                /* the whole screen again, its texts from nothing */
{
    ui.msg_t = 0;
    sm_nscr = 0;
    ui.force = 1;
    frame(); frame();
}
/* the texts equal to `s` whose middle row is in screen rows y0 .. y1 - 1 (a tall row is sent in slices: a text in two of them counts once) */
static uint32_t said(const char *s, int32_t y0, int32_t y1)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < sm_nscr; i++) {
        int32_t m = (sm_scr[i].y0 + sm_scr[i].y1) / 2;
        if (m >= y0 && m < y1 && !strcmp(sm_scr[i].s, s)) n = 1;
    }
    return n;
}
static uint32_t row_says(uint32_t k, const char *s) { return said(s, st_row_y(k), st_row_y(k) + st_row_h()); }
static uint32_t row_texts(uint32_t k)                   /* the different texts in row k */
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < sm_nscr; i++) {
        int32_t m = (sm_scr[i].y0 + sm_scr[i].y1) / 2;
        uint32_t j;
        if (m < st_row_y(k) || m >= st_row_y(k) + st_row_h()) continue;
        for (j = 0; j < i; j++) {
            int32_t mj = (sm_scr[j].y0 + sm_scr[j].y1) / 2;
            if (mj >= st_row_y(k) && mj < st_row_y(k) + st_row_h() && !strcmp(sm_scr[j].s, sm_scr[i].s) && sm_scr[j].x0 == sm_scr[i].x0) break;
        }
        n += j == i;
    }
    return n;
}
static void tracks(uint32_t n)
{
    ui_power_on();
    stop_transport();
    frz_host_frozen = 0;
    song.ntrk = (uint8_t)n;
    GLO_SELECT = (uint8_t)(n < NTRK ? GLO_SELECT | TRACKS_HIDE : GLO_SELECT & ~TRACKS_HIDE);   /* (MENU > UNUSED TRACKS = HIDE: two tracks the tall rows, one the wide card) */
    for (uint32_t k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0;
    go_home();
    frames(100);
}
static void pic_on(void)
{
    hold(B_HOME);
    ui.menu_sel = MI_HOME_PAGE;
    if (!home_pic_on()) press(B_OCTUP);
    hold(B_HOME);
    go_home();
    frames(100);
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    /* ---- ROWS ---- */
    for (uint32_t n = 4; n >= 1u; n = n == 4u ? 2u : n - 1u) {
        char what[160];
        int ok = 1, none;
        tracks(n);
        redraw();
        none = ui.home && !ui.home_pg && stage_rows() == n;
        for (uint32_t k = 0; k < n; k++) none &= !row_says(k, "MUTE");
        for (uint32_t m = 0; m < n; m++) {
            trk[m].p[P_MUTE] = 1;
            redraw();
            for (uint32_t k = 0; k < n; k++) ok &= row_says(k, "MUTE") == (k == m);
            trk[m].p[P_MUTE] = 0;
            redraw();
            ok &= !row_says(m, "MUTE");
        }
        snprintf(what, sizeof what, "ROWS     TRACKS with %u track%s: no row says MUTE; each track muted in turn: its row says MUTE and no other does; unmuted: gone", (unsigned)n, n == 1u ? "" : "s");
        chk(what, none && ok);
    }
    {   /* a mute that arrives with nothing forced (the layer's key, the launch point): the row is drawn again by its own signature */
        tracks(4);
        redraw();
        trk[3].p[P_MUTE] = 1;
        sm_nscr = 0;
        frames(64);
        chk("ROWS     a mute arriving between frames (nothing forced): the row is drawn again with MUTE", row_says(3, "MUTE") && !row_says(2, "MUTE"));
    }
    /* ---- FROZEN ---- */
    {
        int ok;
        tracks(4);
        bank_pid[0][trk[0].pattern % NPAT] = 5; frz_host_frozen = 1ull << 4;   /* (track 1 on phrase 5, frozen: ui_test.c's frozen test) */
        redraw();
        ok = row_says(0, "FROZEN") && !row_says(0, "MUTE") && !row_says(1, "FROZEN");
        chk("FROZEN   a frozen phrase's row says FROZEN", ok);
        trk[0].p[P_MUTE] = 1;
        redraw();
        chk("FROZEN   frozen and muted: the row says MUTE", row_says(0, "MUTE") && !row_says(0, "FROZEN"));
        trk[0].p[P_MUTE] = 0; frz_host_frozen = 0; bank_pid[0][trk[0].pattern % NPAT] = 0;
    }
    /* ---- LONG ---- */
    {
        uint32_t e, p, found = 0;
        char nm[24];
        tracks(4);
        for (e = 0; e < NENGINES && !found; e++) {       /* a factory sound whose name leaves the role no room on a four-track row */
            set_engine_of(&trk[1], e); trk[1].engine = trk[1].eng_req;
            for (p = 0; p < ENGINES[e]->npresets && p < 300u && !found; p++) {
                apply_preset_to(&trk[1], p);
                sound_name(&trk[1], nm);
                if (text_w(TY_TX, nm) > 150) {
                    redraw();
                    found = row_texts(1) == 3u;         /* (the numeral, the name, the pattern's letter: no role word) */
                }
            }
        }
        chk("LONG     a sound whose name takes the role's room on a four-track row is found (the row: numeral, name, pattern, no role)", found);
        printf("stage_mute:   (the sound: %s)\n", found ? nm : "none");
        trk[1].p[P_MUTE] = 1;
        redraw();
        chk("LONG     that track muted: its row still says MUTE", found && row_says(1, "MUTE"));
        trk[1].p[P_MUTE] = 0;
        bank_pid[1][trk[1].pattern % NPAT] = 5; frz_host_frozen = 1ull << 4;
        redraw();
        chk("LONG     that track's phrase frozen: its row still says FROZEN", found && row_says(1, "FROZEN"));
        frz_host_frozen = 0; bank_pid[1][trk[1].pattern % NPAT] = 0;
    }
    /* ---- PICTURE ---- */
#if OC_HOME_PIC
    {
        int ok;
        tracks(4);
        pic_on();
        while (ui.home_pg != 2u) press(B_HOME);
        redraw();
        ok = home_pic_on() && !said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        TSEL->p[P_MUTE] = 1;
        sm_nscr = 0; frames(64);
        ok &= said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        TSEL->p[P_MUTE] = 0;
        redraw();
        ok &= !said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        chk("PICTURE  the picture page's sound line says MUTE while the selected track is muted (drawn again as the mute arrives), not otherwise", ok);
        TSEL->p[P_DUB_ECHO] = 100;
        press(B_HOME);
        redraw();
        ok = ui.home_pg == DUB_PG && !said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        TSEL->p[P_MUTE] = 1;
        sm_nscr = 0; frames(64);
        ok &= said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H) && said("BLACK: RECORDED", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        chk("PICTURE  the DUB page's sound line says MUTE beside its help line while the selected track is muted, not otherwise", ok);
        TSEL->p[P_MUTE] = 0;
    }
#else
    printf("stage_mute: built without OC_HOME_PIC: no picture page to check\n");
#endif
    printf("stage_mute: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
