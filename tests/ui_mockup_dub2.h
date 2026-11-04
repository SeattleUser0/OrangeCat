/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Round 9, item 3 (docs/orangecat/UI_DESIGN.md section 7, "Options for DUB as it is today"): the DUB TAPE performance view redrawn for the DUB of the 0.16.0 Nightlies
 * (pages DUB SETUP, DUB, DUB 2; CC 16 .. 18 THRW CUT KICK). A design: nothing here is in the firmware. Included by tests/ui_mockup.c before main().
 * Three layouts, one key map and one knob map each:
 *   A  THROWS   the DUB page's four knobs (FDBK SEND FILTER ECHO); white keys the six throws and CUT, one black key KICK. Only what DUB has today.
 *   B  SWEEPS   A's keys; the knobs are the four lanes a dub player rides (FDBK FILTER SPEED ECHO); SEND is the throws'.
 *   C  KEYS+    A's knobs and white keys; the black keys add HOLD, DIVE (SPEED down an octave while held) and RISE (up an octave): three triggers DUB does not have.
 * Key bits as frame_t.keys: white 0 .. 6 (C D E F G A B), black 8 .. 12 (C# D# F# G# A#). */
#define DUB2_BW_N 5
static const char *const D2_WL[7] = {"10", "30", "50", "70", "85", "100", "CUT"};
static const int D2_BW[DUB2_BW_N] = {0, 1, 3, 4, 5};   /* a black key after the white keys C D F G A */
/* the colors of the black keys by layout: KICK (the spring's: orange), HOLD (FEEDBACK's blue), DIVE and RISE (SPEED's: the third card, white) */
static const char *const D2_BL[3][DUB2_BW_N] = {{"KICK", "", "", "", ""}, {"KICK", "", "", "", ""}, {"KICK", "HOLD", "", "DIVE", "RISE"}};
static void dub2_keys(int lay, uint32_t keys)
{
    uint16_t bc[DUB2_BW_N];
    int32_t bx[DUB2_BW_N], edge = 0;
    bc[0] = KCOL[3]; bc[1] = lay == 2 ? KCOL[0] : T_DIM; bc[2] = T_DIM; bc[3] = lay == 2 ? KCOL[2] : T_DIM; bc[4] = lay == 2 ? KCOL[2] : T_DIM;
    CV_BANDS(0, Y_KEYS, 240, 240 - Y_KEYS, T_BG) {
        for (int k = 0; k < 7; k++) {                   /* the white keys: the throws in SEND's color (the sends' green), CUT in REC's red */
            int32_t x = 2 + k * 34, on = (keys >> k) & 1;
            uint16_t c = k == 6 ? T_REC : KCOL[1], f = on ? c : T_QUIET;
            cv_rrect(x, 20, 32, 52, 4, on ? c : T_LINE, T_BG);
            cv_rrect(x + 1, 21, 30, 50, 3, f, on ? c : T_LINE);
            cv_text_c(x + 16, ty_y(61, TY_LB), TY_LB, D2_WL[k], on ? T_INK : c, f);
        }
        for (int b = 0; b < DUB2_BW_N; b++) {           /* the black keys */
            int32_t on = (keys >> (8 + b)) & 1, live = D2_BL[lay][b][0] != 0;
            bx[b] = 2 + (D2_BW[b] + 1) * 34 - 1;
            cv_rrect(bx[b] - 11, 18, 22, 30, 3, T_BG, T_QUIET);
            cv_rrect(bx[b] - 10, 18, 20, 29, 3, on ? bc[b] : ux_mix(T_BG, bc[b], live ? 28 : 8), T_BG);
            if (live) {                                 /* its name above, centered on it, with a tick down */
                int32_t w = text_w(TY_LB, D2_BL[lay][b]), x0 = bx[b] - w / 2;
                if (x0 < edge + 3) x0 = edge + 3;           /* (a name is wider than a key: pushed right of the one before, the tick stays on the key) */
                if (x0 < 1) x0 = 1;
                edge = x0 + w;
                cv_text(x0, ty_y(8, TY_LB), TY_LB, D2_BL[lay][b], on ? T_TEXT : bc[b]);
                cv_rect(bx[b], 15, 1, 4, bc[b]);
            }
        }
    }
}
/* THE PICTURES (240 x 80). K1 FDBK: the rollers and marks turn at the loop's rate, a second loop past 100 (1.00). SS_HIT: KICK shakes the tank. SS_GATE: CUT takes the input arrow away.
 * A: K2 SEND (the level bar and arrow in), K3 FILTER (the tilt on the machine's face), K4 ECHO (a level bar at the tank's side: the return).
 * B: K2 FILTER (the tilt), K3 SPEED (a scale above the tank, the marker off its middle: slower to the left, faster to the right), K4 ECHO (as A). SEND is not on a knob: the arrow shows only CUT. */
#define D2_LOOP \
    S_STYLE(2), S_RRECT(SCC_INK, 14, 8, 140, 52, 24), S_STYLE(1), \
    S_BIND(SS_K1, SF_HIDE, 98), S_RRECT(SCC_K1, 10, 4, 148, 60, 28), \
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, 16), S_LINE(SCC_DIM, 36, 5, 36, 11), \
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, -16), S_LINE(SCC_DIM, 52, 57, 52, 63), \
    S_CIRC(SCC_INK, 38, 34, 30), S_CIRC(SCC_INK, 130, 34, 30), \
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(38, 34), S_LINE(SCC_K1, 26, 34, 50, 34), S_LINE(SCC_K1, 38, 22, 38, 46), S_POP, \
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(130, 34), S_LINE(SCC_K1, 118, 34, 142, 34), S_LINE(SCC_K1, 130, 22, 130, 46), S_POP, \
    S_RRECT(SCC_INK, 58, 62, 10, 10, 2), S_RRECT(SCC_INK, 72, 62, 10, 10, 2)
#define D2_TANK \
    S_BIND(SS_HIT, SF_ANG, 2), S_BIND(SS_HIT, SF_Y, -3), S_PUSH(200, 74), \
    S_STYLE(2), S_RRECT(SCC_INK, 168, 20, 66, 44, 5), S_STYLE(1), \
    S_LINE(SCC_INK, 174, 30, 174, 54), S_LINE(SCC_INK, 228, 30, 228, 54), S_LINE(SCC_DIM, 176, 42, 226, 42), \
    S_REP(7, 7, 0), S_BIND(SS_HIT, SF_R, 4), S_CIRC(SCC_DIM, 180, 42, 8), \
    S_POP
#define D2_ECHO_BAR \
    S_LINE(SCC_DIM, 163, 74, 163, 34), S_STYLE(2), S_BIND(SS_K4, SF_Y2, -40), S_LINE(SCC_K4, 163, 74, 163, 74), S_STYLE(1)
static const uint8_t SCENE_DUB_A[] = {
    SC_VERSION, 240, 80, 0,
    S_LINE(SCC_DIM, 4, 74, 4, 34), S_STYLE(2), S_BIND(SS_K2, SF_Y2, -40), S_LINE(SCC_K2, 4, 74, 4, 74), S_STYLE(1),
    S_BIND(SS_GATE, SF_HIDE, 60), S_LINE(SCC_K2, 10, 70, 56, 70),
    S_BIND(SS_GATE, SF_HIDE, 60), S_PLINE(SCC_K2, 3), 50, 66, 56, 70, 50, 74,
    D2_LOOP,
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 0, 10), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 1, 4),
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 2, -4), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 3, -10),
    S_PLINE(SCC_K3, 4), 62, 34, 76, 34, 92, 34, 106, 34,
    D2_ECHO_BAR, D2_TANK,
    S_END,
};
static const uint8_t SCENE_DUB_B[] = {
    SC_VERSION, 240, 80, 0,
    S_BIND(SS_GATE, SF_HIDE, 60), S_LINE(SCC_TRK, 10, 70, 56, 70),
    S_BIND(SS_GATE, SF_HIDE, 60), S_PLINE(SCC_TRK, 3), 50, 66, 56, 70, 50, 74,
    D2_LOOP,
    S_BIND(SS_K2 | SS_CENTERED, SF_P0Y + 0, 10), S_BIND(SS_K2 | SS_CENTERED, SF_P0Y + 1, 4),
    S_BIND(SS_K2 | SS_CENTERED, SF_P0Y + 2, -4), S_BIND(SS_K2 | SS_CENTERED, SF_P0Y + 3, -10),
    S_PLINE(SCC_K2, 4), 62, 34, 76, 34, 92, 34, 106, 34,
    S_LINE(SCC_DIM, 168, 8, 234, 8), S_LINE(SCC_DIM, 201, 3, 201, 13), S_BIND(SS_K3 | SS_CENTERED, SF_X, 33), S_DISC(SCC_K3, 201, 8, 3),
    D2_ECHO_BAR, D2_TANK,
    S_END,
};
static sound_t S_DA = {"DUB TAPE", "DUB", "", 0, SCENE_DUB_A, {{"FDBK", 0, 127, 0, 55, 1}, {"SEND", 0, 127, 0, 127}, {"FILTER", 0, 127, 0, 60}, {"ECHO", 0, 127, 0, 100}}, 1};
static sound_t S_DB = {"DUB TAPE", "DUB", "", 0, SCENE_DUB_B, {{"FDBK", 0, 127, 0, 55, 1}, {"FILTER", 0, 127, 0, 60}, {"SPEED", 0, 127, 0, 64}, {"ECHO", 0, 127, 0, 100}}, 2};
static void dub2_view(int lay, int hot, const frame_t *f)
{
    sound_t *s = lay == 1 ? &S_DB : &S_DA;
    palette_track(s->track);
    head("DUB TAPE", T_THEME, 1, 0, 3, 1);
    sound_line(s, hot, 0, 0);
    dial_strip(s, hot);
    picture(s, Y_DUBPIC, H_DUBPIC, f);
    dub2_keys(lay, (uint32_t)f->keys);
}
/* three screens a layout (resting; a throw held while FEEDBACK builds past 1.00; the layout's own moves) and the cost of each: scene bytes, and the pixels a played frame sends */
static void dub2_shots(const char *dir)
{
    static const char *const NM[3] = {"a", "b", "c"};
    static const int LV[6] = {10, 30, 50, 70, 85, 100};
    for (int lay = 0; lay < 3; lay++) {
        sound_t *s = lay == 1 ? &S_DB : &S_DA;
        char name[40];
        frame_t f1 = {120, 0, 256, 0, 1000, 0, 0};
        begin_screen(); dub2_view(lay, -1, &f1); snprintf(name, sizeof name, "dub2_%s_1", NM[lay]); shot(dir, name);
        s->k[0].v = 115;                                          /* FEEDBACK past 1.00: the second loop is lit */
        frame_t f2 = {220, 0, 256, 0, 1300, 1u << 4, 85};         /* the 85 throw held (G) */
        begin_screen(); dub2_view(lay, 0, &f2); snprintf(name, sizeof name, "dub2_%s_2", NM[lay]); shot(dir, name);
        if (lay == 0) { s->k[3].v = 40; s->k[2].v = 100; }        /* A: the filter swept up and the echo ridden down, CUT and KICK together */
        if (lay == 1) { s->k[1].v = 110; s->k[2].v = 30; }        /* B: the filter high, SPEED dived */
        if (lay == 2) { s->k[2].v = 20; }                         /* C: the filter closed while DIVE and HOLD are held */
        frame_t f3 = {200, lay == 2 ? 0 : 256, lay == 1 ? 256 : 256, 0, 1600,
                      lay == 0 ? (int32_t)((1u << 6) | (1u << 8)) : lay == 1 ? (int32_t)((1u << 2) | (1u << 8)) : (int32_t)((1u << 9) | (1u << 11)), 0};
        begin_screen(); dub2_view(lay, -1, &f3); snprintf(name, sizeof name, "dub2_%s_3", NM[lay]); shot(dir, name);
        /* cost: as the existing dubtape line: two seconds, FEEDBACK 1.15, a throw key every 300 ms and KICK every 500 ms */
        {   uint64_t sum = 0, worst = 0, still;
            frame_t g = {200, 0, 256, 0, 3000, 0, 0};
            s->k[0].v = 115;
            begin_screen(); dub2_view(lay, -1, &g);
            host_sent = 0; picture(s, Y_DUBPIC, H_DUBPIC, &g); dub2_keys(lay, 0); still = host_sent;
            for (int i = 0; i < 20; i++) {
                int32_t since = (i * 100) % 500, k = (i / 3) % 2 ? 1 << ((i / 3) % 6) : 0;
                frame_t h = {200, since < 160 ? 256 - since * 256 / 160 : 0, 256, 0, 3000 + i * 100, k | (since < 100 ? 1 << 8 : 0), k ? LV[(i / 5) % 6] : 0};
                host_sent = 0;
                picture(s, Y_DUBPIC, H_DUBPIC, &h);
                dub2_keys(lay, (uint32_t)h.keys);
                sum += host_sent;
                if (host_sent > worst) worst = host_sent;
            }
            fprintf(rep, "dub2_%s  scene %u B, still %llu px, played: mean %llu px worst %llu px per frame (%.1f ms, %.1f ms of SPI)\n", NM[lay], scene_bytes,
                    (unsigned long long)still, (unsigned long long)(sum / 20u), (unsigned long long)worst, sum / 20.0 * 77.0 / 57600.0, worst * 77.0 / 57600.0);
        }
        s->k[0].v = 55; s->k[1].v = lay == 1 ? 60 : 127; s->k[2].v = lay == 1 ? 64 : 60; s->k[3].v = 100;
    }
}
