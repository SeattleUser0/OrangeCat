/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The pictures of the built-in engines with a HOME picture (docs/orangecat/UI_DESIGN.md section 3, docs/orangecat/UI_BUILD.md
 * step 1b), as scene data (oc_scene.h). Each is drawn in a 240 x 132 box; K1 blue, K2 green, K3 white, K4 orange, as the knobs
 * (oc_knobs.c):
 *   MACRO   HARM  TIMB  MRPH  PLUCK     a lighthouse: the waves' crests (HARM), the lamp and its beam (TIMB), the cloud's
 *                                        shape (MRPH), the boat's sail (PLUCK); the lamp flashes on a note, the beam shines
 *                                        and the sea shows the waveform while it sounds, the boat rides the envelope
 *   SHAPES  TIMB  COLR  TONE  PLUCK     a cat and its yarn: the yarn's size, its loops and its roll (TIMB), the tail
 *                                        raised and flicking at COLR's rate (COLR), the moon in the window (TONE), the
 *                                        paw (PLUCK) raised with it, swatting on a note
 *   DRUM-X  MRPH  TUNE  TONE  DECY      the kit: the kick's head from square to round (MRPH), the snare's lugs (TUNE),
 *                                        the hi-hat's tilt (TONE), the rings off the kick (DECY); the kick pulses and the
 *                                        hat wobbles on a hit
 * NOT IN THE IMAGE (the owner's decision 5, October 29, 2026): the firmware never includes this file. tools/gen_pack.py --scenes
 * compiles it on the host and writes the bytes into the asset pack's scene trailer, read in place on the unit (oc_scene.c
 * scene_of). The host tests and the mockups (tests/ui_mockup.c, tests/ui_scenes.h) include it directly. SCENES lists them for
 * the pack, by engine number.
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version, in tests/ui_scenes.h.
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1b) the three built-in engines' scenes moved here, unchanged.
 *   (ui-fix2 6, November 1, 2026, the owner's finding) SHAPES: COLR sets the tail's wag RATE, PLUCK the paw's bat RATE (the paw striking the
 *   yarn, which is knocked round each time), TIMB the yarn's size alone; the phases run 0.5 Hz to 3 Hz (ui_home_pic.c).
 *   (ui-fix 3, October 31, 2026, the owner's finding "the cat's green hand barely moves") SHAPES: every part moves with its
 *   knob's position, in size and in speed: the yarn grows with TIMB, the tail stands up with COLR and flicks at COLR's rate
 *   (PH2), the moon fills more, the paw rises through 70 degrees each side of its middle with PLUCK and swats through 80 on a note.
 */
#pragma once
#include "oc_scene.h"

static const uint8_t SCENE_MACRO[] = {
    SC_VERSION, 240, 132, 0,
    /* sky: a few stars */
    S_DISC(SCC_DIM, 150, 14, 2), S_DISC(SCC_DIM, 210, 26, 2), S_DISC(SCC_DIM, 118, 30, 2), S_DISC(SCC_DIM, 228, 8, 2),
    /* the cloud (K3 MRPH): three puffs that trade size as it morphs */
    S_CIRC(SCC_K3, 164, 26, 16),
    S_BIND(SS_K3 | SS_CENTERED, SF_R, 14), S_CIRC(SCC_K3, 180, 22, 24),
    S_BIND(SS_K3 | SS_CENTERED, SF_R, -12), S_CIRC(SCC_K3, 198, 28, 16),
    /* the rock */
    S_STYLE(2),
    S_PGON(SCC_INK, 6), 6, 131, 14, 110, 30, 101, 62, 99, 86, 108, 96, 131,
    /* the tower, its two stripes, the gallery and the roof */
    S_PGON(SCC_INK, 4), 40, 100, 46, 46, 62, 46, 68, 100,
    S_STYLE(1),
    S_LINE(SCC_INK, 43, 82, 65, 82), S_LINE(SCC_INK, 44, 64, 64, 64),
    S_RECT(SCC_INK, 43, 38, 23, 8),
    S_STYLE(2),
    S_PLINE(SCC_INK, 3), 40, 38, 54, 26, 69, 38,
    S_STYLE(1),
    /* the lamp (K2 TIMB): bigger with TIMB, a flash on each note */
    S_BIND(SS_K2, SF_R, 6), S_BIND(SS_HIT, SF_R, 5), S_DISC(SCC_K2, 54, 42, 3),
    /* the beam (K2): shines while the track sounds, reaches further with TIMB */
    S_BIND(SS_K2, SF_X2, 70), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SCC_K2, 58, 40, 110, 28),
    S_BIND(SS_K2, SF_X2, 70), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SCC_K2, 58, 44, 110, 56),
    /* the sea: the track's waveform as its surface, while it sounds */
    S_BIND(SS_ENV, SF_HIDE, 8), S_SCOPE(SCC_DIM, 104, 114, 120, 12),
    /* the crests (K1 HARM): two to ten of them, traveling */
    S_BIND(SS_K1, SF_CNT, 8), S_REP(2, 14, 0),
    S_BIND(SS_TIME, SF_X, 14), S_ARC(SCC_K1, 100, 106, 6, 128, 0),
    /* the boat (K4 PLUCK): rides the envelope; its sail grows with PLUCK */
    S_BIND(SS_ENV, SF_Y, -5), S_PUSH(192, 104),
    S_STYLE(2),
    S_PGON(SCC_K4, 4), 176, 100, 208, 100, 201, 108, 183, 108,
    S_LINE(SCC_K4, 192, 99, 192, 70),
    S_STYLE(1),
    S_BIND(SS_K4 | SS_CENTERED, SF_P0X + 2, 12), S_BIND(SS_K4 | SS_CENTERED, SF_P0Y + 0, -8),
    S_PGON(SCC_K4, 3), 194, 74, 194, 96, 210, 96,
    S_POP,
    S_END,
};

static const uint8_t SCENE_SHAPES[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 125, 239, 125),
    /* the window and the moon (K3 TONE): the moon fuller as the tone opens */
    S_RECT(SCC_INK, 150, 8, 66, 54), S_LINE(SCC_INK, 183, 8, 183, 61), S_LINE(SCC_INK, 150, 35, 215, 35),
    S_BIND(SS_K3, SF_R, 20), S_DISC(SCC_K3, 199, 21, 4),
    S_DISC(SCC_DIM, 162, 16, 2), S_DISC(SCC_DIM, 172, 26, 2), S_DISC(SCC_DIM, 160, 48, 2),
    /* the cat */
    S_STYLE(2),
    S_PLINE(SCC_INK, 9), 64, 124, 60, 110, 62, 97, 68, 87, 76, 80, 96, 80, 103, 90, 107, 104, 108, 124,
    S_LINE(SCC_INK, 64, 124, 108, 124),
    S_CIRC(SCC_INK, 86, 62, 30),
    S_PLINE(SCC_INK, 3), 74, 53, 75, 40, 84, 48,
    S_PLINE(SCC_INK, 3), 89, 48, 98, 40, 99, 54,
    S_STYLE(1),
    /* its eyes look at the yarn (still: TIMB's phase draws nothing now; they followed it rolling) */
    S_DISC(SCC_INK, 79, 62, 3),
    S_DISC(SCC_INK, 90, 62, 3),
    S_LINE(SCC_INK, 84, 70, 88, 70),
    /* the tail (K2 COLR): standing up and curled over, wagging side to side about its root at COLR's RATE (PH2, centered: a sweep 42 degrees
     * each side and a flick back, 0.5 Hz at COLR's left end to 3 Hz at its right: ui-fix2 6, the owner's "COLR sets how fast the tail wags") */
    S_STYLE(2),
    /* a triangle wave from the sawtooth: two copies of the tail, the first shown while PH2 is in its first half (swinging out 21 degrees), the
     * second in its second half (swinging back): no snap, a wag */
    S_BIND(SS_PH2, SF_HIDE, -64), S_BIND(SS_PH2 | SS_CENTERED, SF_ANG, -15), S_PUSH(107, 121),
    S_PLINE(SCC_K2, 6), 107, 121, 118, 121, 128, 116, 134, 106, 135, 96, 131, 88,
    S_POP,
    S_BIND(SS_PH2, SF_HIDE, 64), S_BIND(SS_PH2 | SS_CENTERED, SF_ANG, 15), S_PUSH(107, 121),
    S_PLINE(SCC_K2, 6), 107, 121, 118, 121, 128, 116, 134, 106, 135, 96, 131, 88,
    S_POP,
    /* the paw (K4 PLUCK): bats the yarn at PLUCK's RATE (PH4: rises slowly away from the yarn through 67 degrees, then comes down on it, once a
     * turn: 0.5 Hz to 3 Hz), the swing's end resting ON the yarn's edge; a note swats it too (HIT). The yarn recoils at each strike (below).
     * ui-fix2 6, the owner's "PLUCK sets how fast the paw bats the ball of yarn" */
    S_BIND(SS_PH4, SF_ANG, 24), S_BIND(SS_HIT, SF_ANG, 6), S_PUSH(66, 98),
    S_PLINE(SCC_K4, 3), 66, 98, 52, 100, 42, 106,
    S_STYLE(1),
    S_CIRC(SCC_K4, 38, 108, 6),
    S_POP,
    /* the yarn (K1 TIMB): its size, and its loops, with TIMB alone (it no longer rolls at TIMB's rate); it is knocked round a quarter turn
     * by each strike of the paw and rolls slowly back (PH4: the same sawtooth, so it moves with the paw, not on its own) */
    S_STYLE(2),
    S_BIND(SS_K1 | SS_CENTERED, SF_R, 28), S_CIRC(SCC_K1, 30, 112, 24),
    S_STYLE(1),
    S_BIND(SS_PH4, SF_ANG, -32), S_PUSH(30, 112),
    S_BIND(SS_K1, SF_CNT, 4), S_REP(1, 3, 2), S_ARC(SCC_K1, 26, 108, 7, 180, 60),
    S_POP,
    S_LINE(SCC_K1, 40, 106, 49, 97),
    S_END,
};

static const uint8_t SCENE_DRUMX[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the kick, pulsing on a hit; its head (K1 MRPH) from square to round */
    S_STYLE(2),
    S_BIND(SS_HIT, SF_R, 4), S_CIRC(SCC_INK, 120, 90, 62),
    S_STYLE(1),
    S_LINE(SCC_INK, 98, 118, 92, 126), S_LINE(SCC_INK, 142, 118, 148, 126),
    S_BIND(SS_K1, SF_R, 22), S_RRECT(SCC_K1, 100, 70, 41, 41, 0),
    /* the rings off the kick (K4 DECY): one to five */
    S_BIND(SS_K4, SF_CNT, 4), S_REP(1, 5, -2),
    S_BIND(SS_HIT, SF_R, 4), S_ARC(SCC_K4, 128, 88, 38, 228, 28),
    /* the crash on its stand, wobbling on a hit */
    S_LINE(SCC_INK, 15, 40, 15, 126), S_LINE(SCC_INK, 7, 126, 23, 126),
    S_PUSH(15, 38),
    S_STYLE(2), S_LINE(SCC_INK, 2, 42, 36, 34), S_STYLE(1),
    S_POP,
    /* the tom on the kick */
    S_STYLE(2),
    S_RRECT(SCC_INK, 100, 38, 40, 16, 4),
    S_STYLE(1),
    S_LINE(SCC_INK, 120, 54, 120, 59),
    /* the snare and its stand; its lugs (K2 TUNE) taller as it is tuned up */
    S_STYLE(2),
    S_RRECT(SCC_INK, 30, 82, 46, 18, 3),
    S_STYLE(1),
    S_LINE(SCC_INK, 53, 100, 44, 126), S_LINE(SCC_INK, 53, 100, 62, 126),
    S_REP(4, 12, 0), S_BIND(SS_K2, SF_Y2, -10), S_LINE(SCC_K2, 36, 92, 36, 86),
    /* the hi-hat (K3 TONE): opens and tilts with TONE, wobbles on a hit */
    S_LINE(SCC_INK, 200, 66, 200, 126), S_LINE(SCC_INK, 192, 126, 208, 126),
    S_BIND(SS_K3 | SS_CENTERED, SF_ANG, -8), S_BIND(SS_HIT, SF_ANG, 3), S_PUSH(200, 64),
    S_STYLE(2),
    S_LINE(SCC_K3, 178, 64, 222, 64),
    S_BIND(SS_K3, SF_Y, -8), S_LINE(SCC_K3, 178, 66, 222, 66),
    S_STYLE(1),
    S_POP,
    S_END,
};

/* the pack's scene trailer, in this order: engine number, scene (tools/gen_pack.py --scenes reads this table) */
static const struct { uint8_t engine; const uint8_t *scene; uint32_t len; } SCENES[] = {
    {8, SCENE_MACRO, sizeof SCENE_MACRO},               /* ENGI_MACRO */
    {14, SCENE_SHAPES, sizeof SCENE_SHAPES},            /* ENGI_SHAPES */
    {9, SCENE_DRUMX, sizeof SCENE_DRUMX},               /* ENGI_DRUMX */
};
