/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* HOME's main screen says what a track is (firmware/src/ui_main.c mn_row, mn_state; ui_home_pic.c hp_sound_line for the picture page). The round-24 finding, "Track 4 graying out": a track muted by accident
 * showed only as a grey row, which read as a fault; a track says so in a word. The texts are read as the drawing code hands them to the screen (gfx.c GFX_HOOK_TEXT, placed by GFX_HOOK_BLIT, as
 * tests/ui_render.c does).
 *   ROWS     four tracks, two and one (UNUSED TRACKS = HIDE: the rows of the tracks in use): the muted track's row holds the word MUTE, no other row does; unmuted again: gone; a mute that arrives
 *            with nothing forced draws the row again
 *   STATES   OFF, WAIT, MUTE and FROZEN each on its own row at once: each says its word and no other; the precedence (OFF before WAIT before MUTE before FROZEN) when two meet on a row
 *   LONG     a sound name too long for its row beside the word: muted, the row still says MUTE (the name gives way); frozen, FROZEN
 *   STEPS    the 16 cells: a note lights the cell of its step in the track's colour, the playhead is a bar under the cell playing, a pattern shorter than 16 steps draws only its steps
 *   CELLS    an empty cell, a note's and the row's background apart in every palette (luma margin), a tie apart from both
 *   BARS     playing past bar 1: the header's BAR chip, the bar's steps, the playhead's cell; a length cut below the playhead
 *   DIALS    the selected track's four knobs: each label and value on the screen under its dial, the value changed by a turn
 *   PAGES    HOME's turn: the main screen, the picture, DUB where its echo is on, the chord view, the main screen again; power-on lands on the main screen
 *   PICTURE  the picture page's and the DUB page's sound line say MUTE (OFF, WAIT) while the selected track is muted, not otherwise
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
static void chk(const char *what, int ok) { nchk++; printf("main_state: %-150s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
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
static int32_t row_y(uint32_t k) { return MN_Y + (int32_t)k * MN_ROW; }
/* a pixel inside step cell j (0 .. 15) of a row */
static uint32_t cell_x(uint32_t j) { return (uint32_t)(MN_STEP_X + (int32_t)j * 9 + (int32_t)(j / 4u) * 2 + 3); }
/* track k's row: cell j lit as a note (the selected row's ink, the track's colour) */
static int cell_lit(uint32_t k, uint32_t j) { return host_screen[(uint32_t)(row_y(k) + MN_STEP_Y + 2) * 240u + cell_x(j)] == swap16(T_TRK(k)); }
/* track k's row: the cell with the playhead's bar under it, -1 for none, -2 for more than one */
static int playhead_at(uint32_t k)
{
    int at = -1;
    for (uint32_t j = 0; j < 16u; j++)
        if (host_screen[(uint32_t)(row_y(k) + MN_STEP_Y + 7) * 240u + cell_x(j)] == swap16(T_TEXT)) at = at == -1 ? (int)j : -2;
    return at;
}
static uint32_t row_says(uint32_t k, const char *s) { return said(s, row_y(k), row_y(k) + MN_ROW); }
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
    /* ---- STATES ---- */
    {
        int ok;
        tracks(4);
        bank_pid[0][trk[0].pattern % NPAT] = 5; frz_host_frozen = 1ull << 4;   /* (track 1 on phrase 5, frozen: ui_test.c's frozen test) */
        redraw();
        ok = row_says(0, "FROZEN") && !row_says(0, "MUTE") && !row_says(1, "FROZEN");
        chk("STATES   a frozen phrase's row says FROZEN", ok);
        trk[0].p[P_MUTE] = 1;
        redraw();
        chk("STATES   frozen and muted: the row says MUTE", row_says(0, "MUTE") && !row_says(0, "FROZEN"));
        trk[0].p[P_MUTE] = 0; frz_host_frozen = 0; bank_pid[0][trk[0].pattern % NPAT] = 0;
        trk_off_set(2u, 1);
        sm_nscr = 0; frames(64);
        chk("STATES   a track turned OFF (nothing forced): its row says OFF, no other row does", row_says(2, "OFF") && !row_says(0, "OFF") && !row_says(3, "OFF"));
        trk[2].p[P_MUTE] = 1;
        redraw();
        chk("STATES   OFF and muted: OFF only", row_says(2, "OFF") && !row_says(2, "MUTE"));
        trk_off_set(2u, 0); trk_on_wait |= 1u << 2;
        redraw();
        chk("STATES   turned ON, short of memory and muted: WAIT before MUTE", row_says(2, "WAIT") && !row_says(2, "MUTE") && !row_says(2, "OFF"));
        trk_on_wait &= ~(1u << 2); trk[2].p[P_MUTE] = 0;
        redraw();
        chk("STATES   all of it gone: no row says a word", !row_says(0, "OFF") && !row_says(2, "WAIT") && !row_says(2, "MUTE") && !row_says(0, "FROZEN"));
    }
    /* ---- LONG ---- */
    {
        uint32_t e, p, found = 0;
        char nm[24];
        tracks(4);
        for (e = 0; e < NENGINES && !found; e++) {       /* a factory sound whose name does not fit its row beside the longest word at the text size */
            set_engine_of(&trk[1], e); trk[1].engine = trk[1].eng_req;
            for (p = 0; p < ENGINES[e]->npresets && p < 300u && !found; p++) {
                apply_preset_to(&trk[1], p);
                sound_name(&trk[1], nm);
                found = text_w(TY_TX, nm) > 240 - 40 - 6 - (text_w(TY_LB, "FROZEN") + 14 + 4);
            }
        }
        chk("LONG     a sound whose name takes the word's room on a row is found", found);
        printf("main_state:   (the sound: %s)\n", found ? nm : "none");
        trk[1].p[P_MUTE] = 1;
        redraw();
        chk("LONG     that track muted: its row still says MUTE", found && row_says(1, "MUTE"));
        trk[1].p[P_MUTE] = 0;
        bank_pid[1][trk[1].pattern % NPAT] = 5; frz_host_frozen = 1ull << 4;
        redraw();
        chk("LONG     that track's phrase frozen: its row still says FROZEN", found && row_says(1, "FROZEN"));
        frz_host_frozen = 0; bank_pid[1][trk[1].pattern % NPAT] = 0;
    }
    /* ---- STEPS ---- */
    {
        int ok;
        uint32_t cx3, cx12, cx9, cy;
        uint16_t on = swap16(T_TRK(0)), wash = swap16(ux_mix(T_BG, T_TRK(0), 16));
        tracks(4);
        track_defaults_steps(&trk[0]);
        trk[0].p[P_SLEN] = 16;
        step_clear(&trk[0].step[3]);  trk[0].step[3].time = ST_NOTE;  trk[0].step[3].n = 1;  trk[0].step[3].note[0] = 60;  trk[0].step[3].vel = 100;
        step_clear(&trk[0].step[12]); trk[0].step[12].time = ST_NOTE; trk[0].step[12].n = 1; trk[0].step[12].note[0] = 62; trk[0].step[12].vel = 100;
        redraw();
        cx3 = (uint32_t)(MN_STEP_X + 3 * 9 + 0 * 2 + 3); cx12 = (uint32_t)(MN_STEP_X + 12 * 9 + 3 * 2 + 3); cx9 = (uint32_t)(MN_STEP_X + 9 * 9 + 2 * 2 + 3);
        cy = (uint32_t)(MN_Y + MN_STEP_Y + 2);
        ok = host_screen[cy * 240u + cx3] == on && host_screen[cy * 240u + cx12] == on && host_screen[cy * 240u + cx9] != on;
        chk("STEPS    a note on step 4 and one on step 13: their cells are lit in the track's colour, the cell between is not", ok);
        song.playing = 1; trk[0].seq_idx = 5;
        redraw();
        ok = host_screen[(cy + 5u) * 240u + (uint32_t)(MN_STEP_X + 5 * 9 + 1 * 2 + 3)] == swap16(T_TEXT);
        chk("STEPS    playing, step 6 sounding: the bar under its cell is the text's colour", ok);
        song.playing = 0;
        trk[0].p[P_SLEN] = 8;
        redraw();
        ok = mn_cell_empty(0, 1) != mn_row_bg(0, 1) && wash == swap16(mn_row_bg(0, 1));
        for (uint32_t j = 0; j < 16u; j++)              /* (r31 review) the 8 cells drawn, each its ink (cell 4 the note, the others empty), and the row's wash past them */
            ok &= host_screen[cy * 240u + cell_x(j)] == (j >= 8u ? wash : j == 3u ? on : swap16(mn_cell_empty(0, 1)));
        chk("STEPS    a pattern of 8 steps draws 8 cells (the note's and seven empty ones, not the wash): where cells 9 .. 16 would be the row's own wash", ok);
        trk[0].p[P_SLEN] = 16;
        track_defaults_steps(&trk[0]);
    }
    /* ---- CELLS ---- (r31 review) an empty cell, a note's and the row's background stand apart in every palette, every track, selected or not, quiet (muted) or not:
     * at least CELL_MARGIN of 255 in luma pairwise (the worst case is 28: LO-FI's and NEON's unselected rows); a tie at least TIE_MARGIN from both inks. Read off the screen. */
    {
        enum { CELL_MARGIN = 24, TIE_MARGIN = 10 };
        int ok = 1;
        uint32_t worst = 255, wtie = 255, keep = settings.palette;
        char what[200];
        tracks(4);
        for (uint32_t k = 0; k < 4u; k++) {
            track_defaults_steps(&trk[k]);
            trk[k].p[P_SLEN] = 16;
            step_clear(&trk[k].step[0]); trk[k].step[0].time = ST_NOTE; trk[k].step[0].n = 1; trk[k].step[0].note[0] = 60; trk[k].step[0].vel = 100;
            step_clear(&trk[k].step[1]); trk[k].step[1].time = ST_TIE;
            step_clear(&trk[k].step[2]);
        }
        for (uint32_t p = 0; p < UI_PAL_ENTRIES; p++) {
            settings.palette = (uint8_t)p; palette_set(p);
            for (uint32_t s = 0; s < 4u; s++)
                for (uint32_t q = 0; q < 2u; q++) {
                    song.sel = (uint8_t)s;
                    for (uint32_t k = 0; k < 4u; k++) trk[k].p[P_MUTE] = (uint8_t)q;
                    redraw();
                    for (uint32_t k = 0; k < 4u; k++) {
                        uint32_t y = (uint32_t)(row_y(k) + MN_STEP_Y + 2);
                        int32_t bg = (int32_t)ux_luma(swap16(host_screen[y * 240u + (uint32_t)(MN_STEP_X + 8)])), nt = (int32_t)ux_luma(swap16(host_screen[y * 240u + cell_x(0)]));
                        int32_t ti = (int32_t)ux_luma(swap16(host_screen[y * 240u + cell_x(1)])), em = (int32_t)ux_luma(swap16(host_screen[y * 240u + cell_x(2)]));
                        uint32_t d = (uint32_t)abs(bg - em), dt = (uint32_t)abs(ti - em);
                        if ((uint32_t)abs(em - nt) < d) d = (uint32_t)abs(em - nt);
                        if ((uint32_t)abs(bg - nt) < d) d = (uint32_t)abs(bg - nt);
                        if ((uint32_t)abs(ti - nt) < dt) dt = (uint32_t)abs(ti - nt);
                        if (d < worst) worst = d;
                        if (dt < wtie) wtie = dt;
                        if (d < CELL_MARGIN || dt < TIE_MARGIN) {
                            ok = 0;
                            printf("main_state:   palette %u (%s) track %u%s%s: background %d, empty %d, note %d, tie %d\n", (unsigned)p, UI_PALETTES[p].name, (unsigned)k + 1u, k == s ? " selected" : "", q ? " muted" : "", bg, em, nt, ti);
                        }
                    }
                }
        }
        for (uint32_t k = 0; k < 4u; k++) { trk[k].p[P_MUTE] = 0; track_defaults_steps(&trk[k]); }
        song.sel = 0;
        settings.palette = (uint8_t)keep; palette_set(keep);
        snprintf(what, sizeof what, "CELLS    every palette, track, selection and mute: empty cell, note cell, background >= %d of 255 apart in luma (worst %u), a tie >= %d from both (worst %u)",
                 CELL_MARGIN, (unsigned)worst, TIE_MARGIN, (unsigned)wtie);
        chk(what, ok);
    }
    /* ---- BARS ---- (r31 review) the bar in view while playing: the header's BAR chip, the steps of that bar and the playhead's cell */
    {
        int ok;
        tracks(4);
        song.sel = 0;
        track_defaults_steps(&trk[0]);
        trk[0].p[P_SLEN] = 32;
        step_clear(&trk[0].step[16]); trk[0].step[16].time = ST_NOTE; trk[0].step[16].n = 1; trk[0].step[16].note[0] = 60; trk[0].step[16].vel = 100;
        song.playing = 1; trk[0].seq_idx = 16;
        redraw();
        ok = said("BAR 2", 0, MN_Y) && !said("BAR 1", 0, MN_Y) && cell_lit(0, 0) && playhead_at(0) == 0;
        chk("BARS     LEN 32, step 17 playing: the header says BAR 2, its note lights cell 1, the playhead is under cell 1", ok);
        trk[0].p[P_SLEN] = 64; trk[0].seq_idx = 62;
        redraw();
        ok = said("BAR 4", 0, MN_Y) && playhead_at(0) == 14 && !cell_lit(0, 0);
        chk("BARS     LEN 64, step 63 playing: the header says BAR 4, the playhead is under cell 15 (bar 4's cell 1 has no note)", ok);
        trk[0].p[P_SLEN] = 32; trk[0].seq_idx = 40;
        redraw();
        ok = said("BAR 1", 0, MN_Y) && playhead_at(0) == -1 && cell_lit(0, 0) == 0;
        chk("BARS     LEN cut to 32 below the playhead (step 41): no playhead, the header says BAR 1", ok);
        song.playing = 0; trk[0].seq_idx = 0; trk[0].p[P_SLEN] = 16;
        track_defaults_steps(&trk[0]);
    }
    /* ---- DIALS ---- */
    {
        int ok = 1;
        char v[12];
        tracks(4);
        redraw();
        for (uint32_t k = 0; k < 4u; k++) {
            const char *lab = hp_knob_text(k, v);
            if (lab) { int a = said(cell_label(lab, 58), MN_DIAL_Y, MN_DIAL_Y + MN_DIAL_H), b = said(v, MN_DIAL_Y + MN_DIAL_H, 240); ok &= a && b; }
        }
        chk("DIALS    the four knobs: each label under its dial and each value under that", ok);
        hp_knob_text(0, v);
        {
            char w[12];
            turn(EN_K1, 3);
            frames(8);
            hp_knob_text(0, w);
            sm_nscr = 0; redraw();
            chk("DIALS    KNOB 1 turned: its value is another, and on the screen", strcmp(v, w) != 0 && said(w, MN_DIAL_Y + MN_DIAL_H, 240));
        }
    }
    /* ---- PAGES ---- */
    {
        int ok;
        tracks(4);
        ui_power_on();
        go_home();
        frames(8);
        ok = ui.home && ui.home_pg == 0u;
        press(B_HOME); ok &= ui.home_pg == 2u;
        press(B_HOME); ok &= ui.home_pg == 1u;
        press(B_HOME); ok &= ui.home_pg == 0u;
        TSEL->p[P_DUB_ECHO] = 100;
        press(B_HOME); press(B_HOME); ok &= ui.home_pg == DUB_PG;
        press(B_HOME); ok &= ui.home_pg == 1u;
        press(B_HOME); ok &= ui.home_pg == 0u;
        TSEL->p[P_DUB_ECHO] = 0;
        chk("PAGES    HOME on HOME: the main screen, the picture, the chord view, the main screen; DUB between the picture and the chord view where its echo is on; power-on lands on the main screen", ok);
    }
    /* ---- PICTURE ---- */
    {
        int ok;
        tracks(4);
        while (ui.home_pg != 2u) press(B_HOME);
        redraw();
        ok = !said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
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
        press(B_HOME);
        while (ui.home_pg != 2u) press(B_HOME);
        trk_off_set(song.sel, 1);
        sm_nscr = 0; frames(64);
        ok = said("OFF", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H) && !said("MUTE", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        trk_off_set(song.sel, 0); trk_on_wait |= 1u << song.sel;
        redraw();
        ok &= said("WAIT", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H) && !said("OFF", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        trk_on_wait &= ~(1u << song.sel);
        redraw();
        ok &= !said("WAIT", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H) && !said("OFF", HP_SOUND_Y, HP_SOUND_Y + HP_SOUND_H);
        chk("PICTURE  the picture page's sound line says OFF while the selected track is OFF, WAIT while it waits for memory, neither otherwise", ok);
    }
    printf("main_state: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
