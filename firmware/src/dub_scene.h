/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* DUB TAPE's performance view picture (round 9, docs/orangecat/UI_DESIGN.md section 7, layout C; the owner's pick): the picture of an effect, 240 x 80, under HOME's DUB page.
 * IN THE IMAGE, 239 B, against the owner's decision 5 (scenes live in the pack or a module): the pack's scene trailer has 93 B spare (the build refuses 41,112 B in the 40,960 B area
 * with it as a trailer entry). It goes to the DUB module's own picture (plan step 6, oc_modui.c) when DUB is a module. For it the UI fills the sources so:
 *   SS_K1..K4  FDBK (1.00 at 202), SEND (the SEND in force: the knob, or the throw held), FILTER, ECHO
 *   SS_PH1     the loop's turn: 0.3 Hz at FDBK 0, 0.8 Hz at 1.00, 2.96 Hz at 1.27; HOLD holds it at 1.00's
 *   SS_HIT     KICK struck, falling over 160 ms
 *   SS_GATE    the loop's input open: 0 while CUT is held
 * FDBK (K1, blue): the rollers turn and the tape's marks run at the loop's rate; past 1.00 a second loop lights round the tape. SEND (K2, green): the level bar and the input arrow into the
 * record head; CUT takes the arrow away. FILTER (K3, white): the tilt on the machine's face, low-pass at the left to high-pass at the right. ECHO (K4, orange): a level bar beside the tank.
 * KICK shakes the tank and bulges the spring's coils. Included by ui_home_pic.c; tests/ui_mockup_dub2.h draws the same bytes' design. */
#pragma once
#include "oc_scene.h"
static const uint8_t SCENE_DUBTAPE[] = {
    SC_VERSION, 240, 80, 0,
    S_LINE(SCC_DIM, 4, 74, 4, 34), S_STYLE(2), S_BIND(SS_K2, SF_Y2, -40), S_LINE(SCC_K2, 4, 74, 4, 74), S_STYLE(1),
    S_BIND(SS_GATE, SF_HIDE, 60), S_LINE(SCC_K2, 10, 70, 56, 70),
    S_BIND(SS_GATE, SF_HIDE, 60), S_PLINE(SCC_K2, 3), 50, 66, 56, 70, 50, 74,
    S_STYLE(2), S_RRECT(SCC_INK, 14, 8, 140, 52, 24), S_STYLE(1),
    S_BIND(SS_K1, SF_HIDE, 98), S_RRECT(SCC_K1, 10, 4, 148, 60, 28),
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, 16), S_LINE(SCC_DIM, 36, 5, 36, 11),
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, -16), S_LINE(SCC_DIM, 52, 57, 52, 63),
    S_CIRC(SCC_INK, 38, 34, 30), S_CIRC(SCC_INK, 130, 34, 30),
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(38, 34), S_LINE(SCC_K1, 26, 34, 50, 34), S_LINE(SCC_K1, 38, 22, 38, 46), S_POP,
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(130, 34), S_LINE(SCC_K1, 118, 34, 142, 34), S_LINE(SCC_K1, 130, 22, 130, 46), S_POP,
    S_RRECT(SCC_INK, 58, 62, 10, 10, 2), S_RRECT(SCC_INK, 72, 62, 10, 10, 2),
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 0, 10), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 1, 4),
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 2, -4), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 3, -10),
    S_PLINE(SCC_K3, 4), 62, 34, 76, 34, 92, 34, 106, 34,
    S_LINE(SCC_DIM, 163, 74, 163, 34), S_STYLE(2), S_BIND(SS_K4, SF_Y2, -40), S_LINE(SCC_K4, 163, 74, 163, 74), S_STYLE(1),
    S_BIND(SS_HIT, SF_ANG, 2), S_BIND(SS_HIT, SF_Y, -3), S_PUSH(200, 74),
    S_STYLE(2), S_RRECT(SCC_INK, 168, 20, 66, 44, 5), S_STYLE(1),
    S_LINE(SCC_INK, 174, 30, 174, 54), S_LINE(SCC_INK, 228, 30, 228, 54), S_LINE(SCC_DIM, 176, 42, 226, 42),
    S_REP(7, 7, 0), S_BIND(SS_HIT, SF_R, 4), S_CIRC(SCC_DIM, 180, 42, 8),
    S_POP,
    S_END,
};
