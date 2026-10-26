/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The engine pictures of the UI design mockups (docs/orangecat/UI_DESIGN.md, "The pictures"), as scene data
 * (tests/ui_scene.h). Each is drawn in a 240 x 132 box. K1 blue, K2 green, K3 white, K4 orange, as the knobs.
 * The knobs (UI_DESIGN.md's table):
 *   MACRO   HARM  TIMB  MRPH  PLUCK     a lighthouse: the waves' crests (HARM), the lamp and its beam (TIMB), the cloud's
 *                                        shape (MRPH), the boat's sail (PLUCK); the lamp flashes on a note, the beam shines
 *                                        and the sea shows the track's own waveform while it sounds, the boat rides the
 *                                        envelope
 *   SHAPES  TIMB  COLR  TONE  PLUCK     a cat and its yarn: the yarn's loops and its roll (TIMB), the tail's curl
 *                                        (COLR), the moon in the window (TONE), the paw (PLUCK) that swats on a note
 *   DRUM-X  MRPH  TUNE  TONE  DECY      the kit: the kick's head from square to round (MRPH), the snare's lugs (TUNE),
 *                                        the hi-hat's tilt (TONE), the rings off the kick (DECY); the kick pulses and the
 *                                        hat wobbles on a hit
 *   WHEEL   SUB   BODY  TOP   SPIN      an organ and its rotary speaker: the drawbars out (SUB, BODY, TOP), the horn and
 *                                        drum turning at the SPIN rate; the key played lights, sound comes off the cabinet
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version.
 */
#pragma once
#include "ui_scene.h"

static const uint8_t SCENE_MACRO[] = {
    SC_VERSION, 240, 132, 0,
    /* sky: a few stars */
    S_DISC(SC_DIM, 150, 14, 2), S_DISC(SC_DIM, 210, 26, 2), S_DISC(SC_DIM, 118, 30, 2), S_DISC(SC_DIM, 228, 8, 2),
    /* the cloud (K3 MRPH): three puffs that trade size as it morphs */
    S_CIRC(SC_K3, 164, 26, 16),
    S_BIND(SS_K3 | SS_CENTRED, SF_R, 14), S_CIRC(SC_K3, 180, 22, 24),
    S_BIND(SS_K3 | SS_CENTRED, SF_R, -12), S_CIRC(SC_K3, 198, 28, 16),
    /* the rock */
    S_STYLE(2),
    S_PGON(SC_INK, 6), 6, 131, 14, 110, 30, 101, 62, 99, 86, 108, 96, 131,
    /* the tower, its two stripes, the gallery and the roof */
    S_PGON(SC_INK, 4), 40, 100, 46, 46, 62, 46, 68, 100,
    S_STYLE(1),
    S_LINE(SC_INK, 43, 82, 65, 82), S_LINE(SC_INK, 44, 64, 64, 64),
    S_RECT(SC_INK, 43, 38, 23, 8),
    S_STYLE(2),
    S_PLINE(SC_INK, 3), 40, 38, 54, 26, 69, 38,
    S_STYLE(1),
    /* the lamp (K2 TIMB): bigger with TIMB, a flash on each note */
    S_BIND(SS_K2, SF_R, 6), S_BIND(SS_HIT, SF_R, 5), S_DISC(SC_K2, 54, 42, 3),
    /* the beam (K2): shines while the track sounds, reaches further with TIMB */
    S_BIND(SS_K2, SF_X2, 70), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SC_K2, 58, 40, 110, 28),
    S_BIND(SS_K2, SF_X2, 70), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SC_K2, 58, 44, 110, 56),
    /* the sea: the track's waveform as its surface, while it sounds */
    S_BIND(SS_ENV, SF_HIDE, 8), S_SCOPE(SC_DIM, 104, 114, 120, 12),
    /* the crests (K1 HARM): two to ten of them, travelling */
    S_BIND(SS_K1, SF_CNT, 8), S_REP(2, 14, 0),
    S_BIND(SS_TIME, SF_X, 14), S_ARC(SC_K1, 100, 106, 6, 128, 0),
    /* the boat (K4 PLUCK): rides the envelope; its sail grows with PLUCK */
    S_BIND(SS_ENV, SF_Y, -5), S_PUSH(192, 104),
    S_STYLE(2),
    S_PGON(SC_K4, 4), 176, 100, 208, 100, 201, 108, 183, 108,
    S_LINE(SC_K4, 192, 99, 192, 70),
    S_STYLE(1),
    S_BIND(SS_K4 | SS_CENTRED, SF_P0X + 2, 12), S_BIND(SS_K4 | SS_CENTRED, SF_P0Y + 0, -8),
    S_PGON(SC_K4, 3), 194, 74, 194, 96, 210, 96,
    S_POP,
    S_END,
};

static const uint8_t SCENE_SHAPES[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SC_DIM, 0, 125, 239, 125),
    /* the window and the moon (K3 TONE): the moon fuller as the tone opens */
    S_RECT(SC_INK, 150, 8, 66, 54), S_LINE(SC_INK, 183, 8, 183, 61), S_LINE(SC_INK, 150, 35, 215, 35),
    S_BIND(SS_K3, SF_R, 14), S_DISC(SC_K3, 199, 21, 4),
    S_DISC(SC_DIM, 162, 16, 2), S_DISC(SC_DIM, 172, 26, 2), S_DISC(SC_DIM, 160, 48, 2),
    /* the cat */
    S_STYLE(2),
    S_PLINE(SC_INK, 9), 64, 124, 60, 110, 62, 97, 68, 87, 76, 80, 96, 80, 103, 90, 107, 104, 108, 124,
    S_LINE(SC_INK, 64, 124, 108, 124),
    S_CIRC(SC_INK, 86, 62, 30),
    S_PLINE(SC_INK, 3), 74, 53, 75, 40, 84, 48,
    S_PLINE(SC_INK, 3), 89, 48, 98, 40, 99, 54,
    S_STYLE(1),
    /* its eyes follow the yarn */
    S_BIND(SS_PH1 | SS_CENTRED, SF_X, -2), S_DISC(SC_INK, 81, 61, 3),
    S_BIND(SS_PH1 | SS_CENTRED, SF_X, -2), S_DISC(SC_INK, 92, 61, 3),
    S_LINE(SC_INK, 84, 70, 88, 70),
    /* the tail (K2 COLR): curls with COLR, sways */
    S_STYLE(2),
    S_BIND(SS_K2 | SS_CENTRED, SF_P0X + 5, -26), S_BIND(SS_K2 | SS_CENTRED, SF_P0X + 4, -12), S_BIND(SS_WOB, SF_P0X + 5, 4),
    S_PLINE(SC_K2, 6), 107, 121, 118, 121, 128, 116, 134, 106, 135, 96, 131, 88,
    /* the paw (K4 PLUCK): raised with PLUCK, swats on each note */
    S_BIND(SS_K4 | SS_CENTRED, SF_ANG, 10), S_BIND(SS_HIT, SF_ANG, -18), S_PUSH(66, 98),
    S_PLINE(SC_K4, 3), 66, 98, 52, 92, 46, 94,
    S_STYLE(1),
    S_CIRC(SC_K4, 44, 95, 6),
    S_POP,
    /* the yarn (K1 TIMB): more loops with TIMB, rolling at its rate */
    S_STYLE(2),
    S_CIRC(SC_K1, 28, 112, 24),
    S_STYLE(1),
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(28, 112),
    S_BIND(SS_K1, SF_CNT, 4), S_REP(1, 3, 2), S_ARC(SC_K1, 24, 108, 7, 180, 60),
    S_POP,
    S_LINE(SC_K1, 38, 106, 47, 97),
    S_END,
};

static const uint8_t SCENE_DRUMX[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SC_DIM, 0, 127, 239, 127),
    /* the kick, pulsing on a hit; its head (K1 MRPH) from square to round */
    S_STYLE(2),
    S_BIND(SS_HIT, SF_R, 4), S_CIRC(SC_INK, 120, 90, 62),
    S_STYLE(1),
    S_LINE(SC_INK, 98, 118, 92, 126), S_LINE(SC_INK, 142, 118, 148, 126),
    S_BIND(SS_K1, SF_R, 22), S_RRECT(SC_K1, 100, 70, 41, 41, 0),
    /* the rings off the kick (K4 DECY): one to five */
    S_BIND(SS_K4, SF_CNT, 4), S_REP(1, 5, -2),
    S_BIND(SS_HIT, SF_R, 4), S_ARC(SC_K4, 128, 88, 38, 228, 28),
    /* the crash on its stand, wobbling on a hit */
    S_LINE(SC_INK, 15, 40, 15, 126), S_LINE(SC_INK, 7, 126, 23, 126),
    S_PUSH(15, 38),
    S_STYLE(2), S_LINE(SC_INK, 2, 42, 36, 34), S_STYLE(1),
    S_POP,
    /* the tom on the kick */
    S_STYLE(2),
    S_RRECT(SC_INK, 100, 38, 40, 16, 4),
    S_STYLE(1),
    S_LINE(SC_INK, 120, 54, 120, 59),
    /* the snare and its stand; its lugs (K2 TUNE) taller as it is tuned up */
    S_STYLE(2),
    S_RRECT(SC_INK, 30, 82, 46, 18, 3),
    S_STYLE(1),
    S_LINE(SC_INK, 53, 100, 44, 126), S_LINE(SC_INK, 53, 100, 62, 126),
    S_REP(4, 12, 0), S_BIND(SS_K2, SF_Y2, -10), S_LINE(SC_K2, 36, 92, 36, 86),
    /* the hi-hat (K3 TONE): opens and tilts with TONE, wobbles on a hit */
    S_LINE(SC_INK, 200, 66, 200, 126), S_LINE(SC_INK, 192, 126, 208, 126),
    S_BIND(SS_K3 | SS_CENTRED, SF_ANG, -8), S_BIND(SS_HIT, SF_ANG, 3), S_PUSH(200, 64),
    S_STYLE(2),
    S_LINE(SC_K3, 178, 64, 222, 64),
    S_BIND(SS_K3, SF_Y, -8), S_LINE(SC_K3, 178, 66, 222, 66),
    S_STYLE(1),
    S_POP,
    S_END,
};

static const uint8_t SCENE_WHEEL[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SC_DIM, 0, 127, 239, 127),
    /* the drawbars hanging from their rail: SUB (K1) two, BODY (K2) two, TOP (K3) five */
    S_LINE(SC_INK, 12, 10, 124, 10),
    S_STYLE(2),
    S_REP(2, 12, 0), S_BIND(SS_K1 | SS_CENTRED, SF_Y2, 22), S_LINE(SC_K1, 20, 11, 20, 30),
    S_REP(2, 12, 0), S_BIND(SS_K2 | SS_CENTRED, SF_Y2, 22), S_LINE(SC_K2, 44, 11, 44, 30),
    S_REP(5, 12, 0), S_BIND(SS_K3 | SS_CENTRED, SF_Y2, 22), S_LINE(SC_K3, 68, 11, 68, 30),
    S_STYLE(1),
    /* their handles, at the bars' ends */
    S_REP(2, 12, 0), S_BIND(SS_K1 | SS_CENTRED, SF_Y, 22), S_FILL(SC_K1, 17, 30, 7, 4),
    S_REP(2, 12, 0), S_BIND(SS_K2 | SS_CENTRED, SF_Y, 22), S_FILL(SC_K2, 41, 30, 7, 4),
    S_REP(5, 12, 0), S_BIND(SS_K3 | SS_CENTRED, SF_Y, 22), S_FILL(SC_K3, 65, 30, 7, 4),
    /* the organ: the cabinet, two manuals and their keys */
    S_STYLE(2),
    S_RECT(SC_INK, 8, 68, 120, 58),
    S_STYLE(1),
    S_RECT(SC_INK, 12, 50, 112, 9), S_RECT(SC_INK, 12, 60, 112, 9),
    S_REP(13, 8, 0), S_LINE(SC_DIM, 20, 61, 20, 67),
    /* the key you play lights */
    S_BIND(SS_NOTE, SF_X, 96), S_BIND(SS_GATE, SF_HIDE, 60), S_FILL(SC_TRK, 14, 61, 6, 7),
    /* the rotary speaker: the cabinet, its louvres; the horn and the drum (K4 SPIN) turning at the SPIN rate */
    S_STYLE(2),
    S_RRECT(SC_INK, 158, 24, 66, 102, 5),
    S_STYLE(1),
    S_REP(3, 0, 6), S_LINE(SC_DIM, 166, 76, 216, 76),
    S_BIND(SS_PH4, SF_ANG, 127), S_PUSH(191, 56),
    S_STYLE(2),
    S_LINE(SC_K4, 172, 56, 210, 56),
    S_STYLE(1),
    S_PGON(SC_K4, 4), 210, 53, 217, 50, 217, 62, 210, 59,
    S_PGON(SC_K4, 4), 172, 53, 165, 50, 165, 62, 172, 59,
    S_POP,
    S_BIND(SS_PH4, SF_ANG, -127), S_PUSH(191, 106),
    S_LINE(SC_K4, 177, 106, 205, 106), S_LINE(SC_K4, 191, 94, 191, 118),
    S_POP,
    S_CIRC(SC_INK, 191, 106, 30),
    /* sound off the cabinet while a note sounds */
    S_BIND(SS_ENV, SF_R, 10), S_BIND(SS_ENV, SF_HIDE, 6), S_ARC(SC_DIM, 222, 52, 8, 224, 32),
    S_BIND(SS_ENV, SF_R, 14), S_BIND(SS_ENV, SF_HIDE, 6), S_ARC(SC_DIM, 222, 52, 14, 228, 28),
    S_END,
};
