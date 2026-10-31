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
 * On the unit each of these is in its module's image (or the asset pack, for an engine or effect not yet a module), never
 * in the firmware: this file is the host's copy for the mockups (UI_DESIGN.md section 3, "Where a picture lives").
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version.
 *   (second pass) the Prophet, SEM, HADES, FMBELL, SUPERSAW, DDDD, CZ-1, SID and DUB TAPE's performance view.
 */
#pragma once
#include "ui_scene.h"
#include "oc_scenes.h"                                  /* (UI_BUILD.md 1b) MACRO, SHAPES, DRUM-X: the firmware's scenes, in firmware/src
                                                        * since the new interface 0.1 (the asset pack's trailer on the unit) */

static const uint8_t SCENE_WHEEL[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the drawbars hanging from their rail: SUB (K1) two, BODY (K2) two, TOP (K3) five */
    S_LINE(SCC_INK, 12, 10, 124, 10),
    S_STYLE(2),
    S_REP(2, 12, 0), S_BIND(SS_K1 | SS_CENTERED, SF_Y2, 22), S_LINE(SCC_K1, 20, 11, 20, 30),
    S_REP(2, 12, 0), S_BIND(SS_K2 | SS_CENTERED, SF_Y2, 22), S_LINE(SCC_K2, 44, 11, 44, 30),
    S_REP(5, 12, 0), S_BIND(SS_K3 | SS_CENTERED, SF_Y2, 22), S_LINE(SCC_K3, 68, 11, 68, 30),
    S_STYLE(1),
    /* their handles, at the bars' ends */
    S_REP(2, 12, 0), S_BIND(SS_K1 | SS_CENTERED, SF_Y, 22), S_FILL(SCC_K1, 17, 30, 7, 4),
    S_REP(2, 12, 0), S_BIND(SS_K2 | SS_CENTERED, SF_Y, 22), S_FILL(SCC_K2, 41, 30, 7, 4),
    S_REP(5, 12, 0), S_BIND(SS_K3 | SS_CENTERED, SF_Y, 22), S_FILL(SCC_K3, 65, 30, 7, 4),
    /* the organ: the cabinet, two manuals and their keys */
    S_STYLE(2),
    S_RECT(SCC_INK, 8, 68, 120, 58),
    S_STYLE(1),
    S_RECT(SCC_INK, 12, 50, 112, 9), S_RECT(SCC_INK, 12, 60, 112, 9),
    S_REP(13, 8, 0), S_LINE(SCC_DIM, 20, 61, 20, 67),
    /* the key you play lights */
    S_BIND(SS_NOTE, SF_X, 96), S_BIND(SS_GATE, SF_HIDE, 60), S_FILL(SCC_TRK, 14, 61, 6, 7),
    /* the rotary speaker: the cabinet, its louvres; the horn and the drum (K4 SPIN) turning at the SPIN rate */
    S_STYLE(2),
    S_RRECT(SCC_INK, 158, 24, 66, 102, 5),
    S_STYLE(1),
    S_REP(3, 0, 6), S_LINE(SCC_DIM, 166, 76, 216, 76),
    S_BIND(SS_PH4, SF_ANG, 127), S_PUSH(191, 56),
    S_STYLE(2),
    S_LINE(SCC_K4, 172, 56, 210, 56),
    S_STYLE(1),
    S_PGON(SCC_K4, 4), 210, 53, 217, 50, 217, 62, 210, 59,
    S_PGON(SCC_K4, 4), 172, 53, 165, 50, 165, 62, 172, 59,
    S_POP,
    S_BIND(SS_PH4, SF_ANG, -127), S_PUSH(191, 106),
    S_LINE(SCC_K4, 177, 106, 205, 106), S_LINE(SCC_K4, 191, 94, 191, 118),
    S_POP,
    S_CIRC(SCC_INK, 191, 106, 30),
    /* sound off the cabinet while a note sounds */
    S_BIND(SS_ENV, SF_R, 10), S_BIND(SS_ENV, SF_HIDE, 6), S_ARC(SCC_DIM, 222, 52, 8, 224, 32),
    S_BIND(SS_ENV, SF_R, 14), S_BIND(SS_ENV, SF_HIDE, 6), S_ARC(SCC_DIM, 222, 52, 14, 228, 28),
    S_END,
};

/* ---- (0.15 design, second pass) the other engines' pictures, as UI_DESIGN.md's "the others" table ---- */

/* Prophet: a campfire and a kettle. CUTOFF the flame's height, RES the kettle's whistle, ENV AMT the sparks, SHAPE the
 * smoke's length; the flame leaps with the envelope, the sparks fly while it sounds */
static const uint8_t SCENE_PROPHET[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    S_DISC(SCC_DIM, 30, 14, 2), S_DISC(SCC_DIM, 60, 30, 2), S_DISC(SCC_DIM, 214, 12, 2), S_DISC(SCC_DIM, 196, 34, 2),
    S_ARC(SCC_DIM, 34, 30, 10, 64, 192),
    /* the logs */
    S_STYLE(2),
    S_LINE(SCC_INK, 84, 126, 136, 112), S_LINE(SCC_INK, 84, 112, 136, 126),
    /* the flame (K1): taller with CUTOFF, leaping with the envelope */
    S_BIND(SS_K1, SF_P0Y + 3, -26), S_BIND(SS_ENV, SF_P0Y + 3, -10), S_BIND(SS_K1, SF_P0Y + 2, -12), S_BIND(SS_K1, SF_P0Y + 4, -10),
    S_PGON(SCC_K1, 7), 94, 114, 92, 98, 100, 90, 110, 80, 118, 92, 126, 96, 124, 114,
    S_STYLE(1),
    /* the stone and the kettle */
    S_RRECT(SCC_INK, 150, 112, 64, 14, 6),
    S_STYLE(2),
    S_RRECT(SCC_INK, 160, 82, 44, 30, 10),
    S_STYLE(1),
    S_LINE(SCC_INK, 162, 92, 146, 78), S_LINE(SCC_INK, 146, 78, 142, 78),
    S_ARC(SCC_INK, 182, 82, 14, 128, 0), S_DISC(SCC_INK, 182, 79, 4),
    /* its whistle (K2 RES): one to five lines off the spout */
    S_BIND(SS_K2, SF_CNT, 4), S_REP(1, -5, -4), S_ARC(SCC_K2, 140, 74, 6, 136, 216),
    /* the sparks (K3 ENV AMT): more of them, rising while the track sounds */
    S_BIND(SS_K3, SF_CNT, 4), S_REP(1, 9, -7), S_BIND(SS_TIME, SF_Y, -16), S_BIND(SS_ENV, SF_HIDE, 8),
    S_DISC(SCC_K3, 96, 70, 3),
    /* the smoke (K4 SHAPE): longer with SHAPE */
    S_BIND(SS_K4, SF_P0Y + 3, -14), S_BIND(SS_K4, SF_P0Y + 4, -26), S_BIND(SS_K4, SF_P0X + 4, 8),
    S_PLINE(SCC_K4, 5), 186, 70, 180, 60, 190, 52, 184, 44, 192, 38,
    S_END,
};

/* SEM: a submarine. CUT the periscope's height, RES the sonar rings, STAT the dive angle, DRIFT the bubbles; a ping on
 * each note; a fish goes by */
static const uint8_t SCENE_SEM[] = {
    SC_VERSION, 240, 132, 0,
    /* the surface */
    S_REP(10, 24, 0), S_ARC(SCC_DIM, 12, 26, 12, 16, 112),
    /* the fish */
    S_PGON(SCC_INK, 4), 20, 116, 34, 110, 44, 116, 34, 122,
    S_PGON(SCC_INK, 3), 20, 116, 12, 110, 12, 122,
    S_DISC(SCC_INK, 38, 115, 2),
    /* the bubbles (K4 DRIFT): more of them, drifting up */
    S_BIND(SS_K4, SF_CNT, 5), S_REP(1, 5, -9), S_BIND(SS_TIME, SF_Y, -8), S_CIRC(SCC_K4, 102, 50, 5),
    /* the sub, diving with STAT (K3) */
    S_BIND(SS_K3 | SS_CENTERED, SF_ANG, -6), S_PUSH(118, 86),
    S_STYLE(2),
    S_RRECT(SCC_K3, 62, 70, 112, 32, 15),
    S_STYLE(1),
    S_PGON(SCC_INK, 4), 102, 70, 106, 56, 128, 56, 132, 70,
    S_REP(3, 22, 0), S_CIRC(SCC_INK, 84, 86, 8),
    S_LINE(SCC_INK, 62, 86, 54, 86), S_LINE(SCC_INK, 54, 78, 54, 94),
    /* the periscope (K1 CUT): higher as the filter opens */
    S_BIND(SS_K1, SF_Y2, -26), S_LINE(SCC_K1, 120, 56, 120, 46),
    S_BIND(SS_K1, SF_Y, -26), S_LINE(SCC_K1, 120, 46, 128, 46),
    /* the sonar rings (K2 RES), off the nose, a ping on each note */
    S_BIND(SS_K2, SF_CNT, 4), S_REP(1, 6, 0), S_BIND(SS_HIT, SF_R, 5), S_ARC(SCC_K2, 176, 86, 8, 224, 32),
    S_POP,
    S_END,
};

/* HADES: a volcano. CUT the lava, RES the sparks, DRV the smoke, SUBS the rumble at its foot; it rumbles with the
 * envelope; a palm tree */
static const uint8_t SCENE_HADES[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the palm */
    S_LINE(SCC_INK, 206, 127, 210, 92),
    S_ARC(SCC_INK, 198, 96, 12, 176, 240), S_ARC(SCC_INK, 222, 96, 12, 144, 208), S_ARC(SCC_INK, 210, 102, 14, 160, 224),
    /* the volcano */
    S_STYLE(2),
    S_PLINE(SCC_INK, 6), 14, 127, 70, 62, 78, 58, 94, 58, 102, 62, 166, 127,
    S_STYLE(1),
    /* the lava (K1 CUT): further down the side as the filter opens */
    S_BIND(SS_K1, SF_P0Y + 2, 36), S_BIND(SS_K1, SF_P0X + 2, -14), S_BIND(SS_K1, SF_P0Y + 1, 10),
    S_PLINE(SCC_K1, 3), 84, 60, 80, 70, 84, 80,
    S_BIND(SS_K1, SF_P0Y + 2, 30), S_BIND(SS_K1, SF_P0X + 2, 12),
    S_PLINE(SCC_K1, 3), 92, 60, 96, 68, 94, 76,
    S_ARC(SCC_K1, 86, 58, 7, 0, 128),
    /* the sparks (K2 RES): more of them, flying while it sounds */
    S_BIND(SS_K2, SF_CNT, 5), S_REP(1, 7, -3), S_BIND(SS_TIME, SF_Y, -14), S_BIND(SS_ENV, SF_HIDE, 8),
    S_DISC(SCC_K2, 72, 50, 3),
    /* the smoke (K3 DRV): bigger puffs with more drive, rising while it sounds */
    S_BIND(SS_K3, SF_R, 8), S_BIND(SS_ENV, SF_Y, -6), S_CIRC(SCC_K3, 88, 40, 10),
    S_BIND(SS_K3, SF_R, 10), S_BIND(SS_ENV, SF_Y, -6), S_CIRC(SCC_K3, 98, 26, 14),
    S_BIND(SS_K3, SF_R, 12), S_BIND(SS_ENV, SF_Y, -6), S_CIRC(SCC_K3, 90, 10, 14),
    /* the rumble (K4 SUBS): one to five marks either side of the foot, jumping with the envelope */
    S_BIND(SS_K4, SF_CNT, 4), S_REP(1, 6, 0), S_BIND(SS_ENV, SF_Y, -4), S_LINE(SCC_K4, 170, 124, 174, 116),
    S_BIND(SS_K4, SF_CNT, 4), S_REP(1, -6, 0), S_BIND(SS_ENV, SF_Y, -4), S_LINE(SCC_K4, 10, 124, 6, 116),
    S_END,
};

/* FMBELL: a bell in its tower. RAT the bell's size, INDX the strike rings, DCAY how far they reach, FDBK the bird on the
 * roof (it leans); the clapper swings and the rings pulse on a note */
static const uint8_t SCENE_FMBELL[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the tower and its roof */
    S_STYLE(2),
    S_LINE(SCC_INK, 76, 127, 82, 46), S_LINE(SCC_INK, 164, 127, 158, 46),
    S_PLINE(SCC_INK, 3), 70, 46, 120, 16, 170, 46,
    S_STYLE(1),
    S_LINE(SCC_INK, 82, 46, 158, 46), S_LINE(SCC_INK, 80, 96, 160, 96),
    /* the bell (K1 RAT): wider as the ratio rises */
    S_BIND(SS_K1 | SS_CENTERED, SF_P0X + 1, -6), S_BIND(SS_K1 | SS_CENTERED, SF_P0X + 2, -12),
    S_BIND(SS_K1 | SS_CENTERED, SF_P0X + 3, 12), S_BIND(SS_K1 | SS_CENTERED, SF_P0X + 4, 6),
    S_PLINE(SCC_K1, 6), 110, 56, 108, 68, 100, 82, 140, 82, 132, 68, 130, 56,
    S_ARC(SCC_K1, 120, 56, 10, 128, 0),
    /* the clapper, swinging on a note */
    S_BIND(SS_HIT, SF_ANG, 10), S_PUSH(120, 58),
    S_LINE(SCC_INK, 120, 58, 120, 78), S_DISC(SCC_INK, 120, 80, 5),
    S_POP,
    /* the strike rings (K2 INDX): one to four each side, pulsing on a note; DCAY (K3) how far round they reach */
    S_BIND(SS_K2, SF_CNT, 3), S_REP(1, 6, 0), S_BIND(SS_K3, SF_A0, -18), S_BIND(SS_K3, SF_A1, 18),
    S_ARC(SCC_K2, 146, 70, 10, 232, 24),
    S_BIND(SS_K2, SF_CNT, 3), S_REP(1, -6, 0), S_BIND(SS_K3, SF_A0, -18), S_BIND(SS_K3, SF_A1, 18),
    S_ARC(SCC_K2, 94, 70, 10, 104, 152),
    /* DCAY's own mark: a ring round the bell's lip, wider as the decay lengthens */
    S_BIND(SS_K3, SF_X2, 30), S_BIND(SS_K3, SF_X, -15), S_LINE(SCC_K3, 104, 88, 136, 88),
    /* the bird on the roof (K4 FDBK): leans with feedback, hops on a note */
    S_BIND(SS_K4 | SS_CENTERED, SF_ANG, 8), S_BIND(SS_HIT, SF_Y, -4), S_PUSH(148, 28),
    S_CIRC(SCC_K4, 148, 22, 10), S_DISC(SCC_K4, 156, 15, 6), S_LINE(SCC_K4, 159, 15, 164, 16),
    S_LINE(SCC_K4, 140, 22, 134, 18), S_LINE(SCC_K4, 147, 27, 147, 31),
    S_POP,
    S_END,
};

/* SUPERSAW: a beehive on a branch. DTUN how far the bees spread, MRPH their wings, SUB the hive's size, DRIV the buzz;
 * the bees swarm out with the envelope; two flowers */
static const uint8_t SCENE_SUPERSAW[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the branch */
    S_STYLE(2), S_LINE(SCC_INK, 0, 8, 150, 14), S_STYLE(1), S_LINE(SCC_INK, 96, 12, 96, 22),
    /* the hive (K3 SUB): bigger with more sub */
    S_BIND(SS_K3, SF_X, -6), S_BIND(SS_K3, SF_X2, 12), S_RRECT(SCC_K3, 82, 22, 28, 14, 6),
    S_BIND(SS_K3, SF_X, -9), S_BIND(SS_K3, SF_X2, 18), S_RRECT(SCC_K3, 74, 36, 44, 16, 7),
    S_BIND(SS_K3, SF_X, -10), S_BIND(SS_K3, SF_X2, 20), S_RRECT(SCC_K3, 70, 52, 52, 18, 8),
    S_BIND(SS_K3, SF_X, -9), S_BIND(SS_K3, SF_X2, 18), S_RRECT(SCC_K3, 74, 70, 44, 16, 7),
    S_DISC(SCC_INK, 96, 78, 7),
    /* the flowers */
    S_LINE(SCC_INK, 30, 127, 30, 108), S_CIRC(SCC_INK, 30, 104, 8), S_ARC(SCC_INK, 26, 116, 5, 96, 192),
    S_LINE(SCC_INK, 212, 127, 212, 112), S_CIRC(SCC_INK, 212, 108, 8),
    /* the bees (K1 DTUN): further apart with more detune, out of the hive with the envelope */
    S_BIND(SS_ENV, SF_X, 40), S_BIND(SS_ENV, SF_Y, 10), S_PUSH(130, 70),
    S_BIND(SS_K1, SF_DX, 10), S_BIND(SS_K1, SF_DY, -4), S_REP(6, 6, -3), S_DISC(SCC_K1, 134, 84, 5),
    /* their wings (K2 MRPH) */
    S_BIND(SS_K1, SF_DX, 10), S_BIND(SS_K1, SF_DY, -4), S_REP(6, 6, -3), S_BIND(SS_K2, SF_R, 4), S_CIRC(SCC_K2, 133, 79, 4),
    /* the buzz (K4 DRIV): zigzags behind them */
    S_BIND(SS_K4, SF_CNT, 3), S_REP(1, 0, 8), S_PLINE(SCC_K4, 4), 196, 54, 202, 50, 208, 54, 214, 50,
    S_POP,
    S_END,
};

/* DDDD: an old TV with static. SRC the channel dial, FDBK the picture rolling (at its rate), DSTR the snow, HAND the
 * antenna; the screen flares on a note; a plant on top */
static const uint8_t SCENE_DDDD[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    S_STYLE(2), S_RRECT(SCC_INK, 50, 34, 132, 86, 10), S_STYLE(1),
    S_RRECT(SCC_INK, 60, 44, 86, 66, 8),
    S_LINE(SCC_INK, 64, 120, 58, 127), S_LINE(SCC_INK, 168, 120, 174, 127),
    /* the plant */
    S_LINE(SCC_INK, 166, 34, 166, 22), S_ARC(SCC_INK, 160, 22, 6, 192, 64), S_ARC(SCC_INK, 172, 20, 6, 64, 192),
    S_RECT(SCC_INK, 160, 28, 13, 6),
    /* the antenna (K4 HAND): leans with the hand */
    S_BIND(SS_K4 | SS_CENTERED, SF_ANG, 10), S_PUSH(110, 34),
    S_LINE(SCC_K4, 110, 34, 92, 8), S_LINE(SCC_K4, 110, 34, 128, 8), S_DISC(SCC_K4, 92, 8, 4), S_DISC(SCC_K4, 128, 8, 4),
    S_POP,
    /* the channel dial (K1 SRC) */
    S_CIRC(SCC_K1, 164, 60, 18),
    S_BIND(SS_K1, SF_ANG, 90), S_PUSH(164, 60), S_LINE(SCC_K1, 164, 60, 164, 52), S_POP,
    S_REP(3, 0, 10), S_LINE(SCC_DIM, 156, 80, 172, 80),
    /* the snow (K3 DSTR): more of it with more destruction, never still */
    S_BIND(SS_K3, SF_CNT, 4), S_REP(2, 11, 6), S_BIND(SS_TIME, SF_X, 9), S_DISC(SCC_K3, 66, 52, 2),
    S_BIND(SS_K3, SF_CNT, 3), S_REP(2, 12, 3), S_BIND(SS_TIME, SF_X, -9), S_DISC(SCC_K3, 80, 76, 2),
    S_BIND(SS_K3, SF_CNT, 4), S_REP(2, 11, -5), S_BIND(SS_TIME, SF_X, 7), S_DISC(SCC_K3, 66, 100, 2),
    /* the roll bar (K2 FDBK): it rolls down the screen at the feedback's rate */
    S_BIND(SS_PH2, SF_Y, 60), S_LINE(SCC_K2, 64, 47, 142, 47),
    /* the flare: the screen's edge lit on a note */
    S_BIND(SS_HIT, SF_HIDE, 40), S_RRECT(SCC_TRK, 63, 47, 80, 60, 6),
    S_END,
};

/* CZ-1: a windmill on a hill. BRIGHT the sails' length, WARP how fast they turn, DETUNE a second windmill behind (further
 * off as it rises), SHAPE the wind; the sails catch a gust on a note */
static const uint8_t SCENE_CZ1[] = {
    SC_VERSION, 240, 132, 0,
    S_ARC(SCC_DIM, 120, 210, 112, 160, 224),
    /* the second windmill (K3 DETUNE): there when detuned, further off the more */
    S_BIND(SS_K3, SF_X, 30), S_BIND(SS_K3, SF_HIDE, 4), S_PGON(SCC_K3, 4), 180, 112, 184, 80, 194, 80, 198, 112,
    S_BIND(SS_K3, SF_X, 30), S_BIND(SS_K3, SF_HIDE, 4), S_BIND(SS_PH2, SF_ANG, 127), S_PUSH(189, 76),
    S_LINE(SCC_K3, 172, 76, 206, 76), S_LINE(SCC_K3, 189, 59, 189, 93),
    S_POP,
    /* the mill */
    S_STYLE(2),
    S_PGON(SCC_INK, 4), 98, 122, 106, 62, 134, 62, 142, 122,
    S_PLINE(SCC_INK, 3), 102, 62, 120, 46, 138, 62,
    S_STYLE(1),
    S_ARC(SCC_INK, 120, 122, 8, 128, 0), S_RECT(SCC_INK, 114, 84, 12, 10),
    /* the sails (K1 BRIGHT longer, turning at WARP's rate (K2), a gust on a note) */
    S_BIND(SS_PH2, SF_ANG, 127), S_BIND(SS_HIT, SF_ANG, 12), S_PUSH(120, 54),
    S_BIND(SS_K1, SF_Y2, -16), S_LINE(SCC_K2, 120, 54, 120, 30),
    S_BIND(SS_K1, SF_X2, 16), S_LINE(SCC_K2, 120, 54, 144, 54),
    S_BIND(SS_K1, SF_Y2, 16), S_LINE(SCC_K2, 120, 54, 120, 78),
    S_BIND(SS_K1, SF_X2, -16), S_LINE(SCC_K2, 120, 54, 96, 54),
    S_BIND(SS_K1, SF_P0Y + 1, -16), S_BIND(SS_K1, SF_P0Y + 2, -16), S_PGON(SCC_K1, 4), 121, 48, 121, 32, 129, 32, 129, 48,
    S_BIND(SS_K1, SF_P0X + 1, 16), S_BIND(SS_K1, SF_P0X + 2, 16), S_PGON(SCC_K1, 4), 126, 55, 142, 55, 142, 63, 126, 63,
    S_BIND(SS_K1, SF_P0Y + 1, 16), S_BIND(SS_K1, SF_P0Y + 2, 16), S_PGON(SCC_K1, 4), 119, 60, 119, 76, 111, 76, 111, 60,
    S_BIND(SS_K1, SF_P0X + 1, -16), S_BIND(SS_K1, SF_P0X + 2, -16), S_PGON(SCC_K1, 4), 114, 53, 98, 53, 98, 45, 114, 45,
    S_POP,
    S_DISC(SCC_INK, 120, 54, 4),
    /* the wind (K4 SHAPE): more lines, blowing through */
    S_BIND(SS_K4, SF_CNT, 3), S_REP(1, 6, 14), S_BIND(SS_TIME, SF_X, 16), S_LINE(SCC_K4, 10, 30, 34, 30),
    S_END,
};

/* SID: a rocket on its pad. CUT the flame, RES the countdown lights, COLOR the stripes, DRIVE the smoke; it lifts a
 * little while it sounds */
static const uint8_t SCENE_SID[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    S_DISC(SCC_DIM, 24, 18, 2), S_DISC(SCC_DIM, 200, 12, 2), S_DISC(SCC_DIM, 224, 40, 2), S_DISC(SCC_DIM, 170, 30, 2),
    /* the gantry and its countdown lights (K2 RES: one to five) */
    S_LINE(SCC_INK, 60, 127, 60, 36), S_LINE(SCC_INK, 76, 127, 76, 36),
    S_REP(9, 0, -10), S_LINE(SCC_DIM, 60, 122, 76, 117),
    S_BIND(SS_K2, SF_CNT, 4), S_REP(1, 0, -12), S_DISC(SCC_K2, 68, 108, 6),
    /* the pad */
    S_RECT(SCC_INK, 92, 118, 56, 9),
    /* the smoke (K4 DRIVE): more puffs, bigger while it sounds */
    S_BIND(SS_K4, SF_CNT, 3), S_REP(1, -14, 0), S_BIND(SS_ENV, SF_R, 6), S_CIRC(SCC_K4, 98, 116, 12),
    S_BIND(SS_K4, SF_CNT, 3), S_REP(1, 14, 0), S_BIND(SS_ENV, SF_R, 6), S_CIRC(SCC_K4, 142, 116, 12),
    /* the rocket, lifting with the envelope */
    S_BIND(SS_ENV, SF_Y, -10), S_PUSH(120, 100),
    S_STYLE(2),
    S_PGON(SCC_INK, 5), 120, 14, 132, 36, 132, 98, 108, 98, 108, 36,
    S_STYLE(1),
    S_PGON(SCC_INK, 3), 108, 78, 98, 100, 108, 98, S_PGON(SCC_INK, 3), 132, 78, 142, 100, 132, 98,
    S_CIRC(SCC_INK, 120, 46, 10),
    /* its stripes (K3 COLOR) */
    S_BIND(SS_K3, SF_CNT, 3), S_REP(1, 0, 7), S_LINE(SCC_K3, 109, 62, 131, 62),
    /* its flame (K1 CUT): longer as the filter opens, flickering with the envelope */
    S_BIND(SS_K1, SF_P0Y + 2, 20), S_BIND(SS_ENV, SF_P0Y + 2, 6),
    S_PGON(SCC_K1, 5), 112, 100, 116, 108, 120, 112, 124, 108, 128, 100,
    S_POP,
    S_END,
};

/* ---- DUB TAPE's performance view (SPEC.md 9.3.2 on branch dub-tape): the tape loop and the spring tank ----
 * The picture of an EFFECT. Its box is 240 x 80: the octave of performance keys is drawn under it by the page (real
 * text: the throw levels, CUT, KICK, HOLD, CRASH), not by the scene. For an effect the UI fills the sources so:
 *   SS_K1..K4  FEEDBACK (0 .. 1.3 over the knob's travel: 1.0 at 197), SEND (the SEND in force: the knob, or the throw
 *              key held), TONE, SPACE
 *   SS_PH1     the loop's turn; its rate rises slowly up to FEEDBACK 1.0 and fast above it (0.3 Hz at 0, 0.8 Hz at 1.0,
 *              3.2 Hz at 1.3); HOLD holds it at 1.0's
 *   SS_HIT     KICK (or CRASH) struck, falling over 160 ms
 *   SS_GATE    the loop's input open: 0 while CUT is held
 *   SS_ENV     the wet's level
 * FEEDBACK (K1, blue): the rollers turn and the tape's marks run at the loop's rate; past 1.0 a second loop lights round
 * the tape (it builds). SEND (K2, green): the input arrow into the record head and its level bar; CUT takes the arrow away.
 * TONE (K3, white): the filter's tilt drawn on the machine's face, low-pass at the left to high-pass at the right.
 * SPACE (K4, orange): the play head slides away from the record head over the first half (the delay time); past the
 * middle the spring in the tank lights (the spring mix). KICK shakes the tank and bulges the spring's coils. */
static const uint8_t SCENE_DUB[] = {
    SC_VERSION, 240, 80, 0,
    /* SEND (K2): the level bar at the left and the input arrow into the record head (gone while CUT is held) */
    S_LINE(SCC_DIM, 4, 74, 4, 34),
    S_STYLE(2), S_BIND(SS_K2, SF_Y2, -40), S_LINE(SCC_K2, 4, 74, 4, 74), S_STYLE(1),
    S_BIND(SS_GATE, SF_HIDE, 60), S_LINE(SCC_K2, 10, 70, 56, 70),
    S_BIND(SS_GATE, SF_HIDE, 60), S_PLINE(SCC_K2, 3), 50, 66, 56, 70, 50, 74,
    /* the tape loop round its two rollers */
    S_STYLE(2), S_RRECT(SCC_INK, 14, 8, 140, 52, 24), S_STYLE(1),
    /* past FEEDBACK 1.0 it builds: a second loop lights round the tape */
    S_BIND(SS_K1, SF_HIDE, 98), S_RRECT(SCC_K1, 10, 4, 148, 60, 28),
    /* the tape's marks running at the loop's rate: right along the top, left along the bottom */
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, 16), S_LINE(SCC_DIM, 36, 5, 36, 11),
    S_REP(6, 16, 0), S_BIND(SS_PH1, SF_X, -16), S_LINE(SCC_DIM, 52, 57, 52, 63),
    /* the rollers: rings with spokes turning at the loop's rate */
    S_CIRC(SCC_INK, 38, 34, 30), S_CIRC(SCC_INK, 130, 34, 30),
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(38, 34),
    S_LINE(SCC_K1, 26, 34, 50, 34), S_LINE(SCC_K1, 38, 22, 38, 46), S_POP,
    S_BIND(SS_PH1, SF_ANG, 127), S_PUSH(130, 34),
    S_LINE(SCC_K1, 118, 34, 142, 34), S_LINE(SCC_K1, 130, 22, 130, 46), S_POP,
    /* the heads under the bottom run: record (fixed), play (K4: slides away over SPACE's first half) */
    S_RRECT(SCC_INK, 58, 62, 10, 10, 2),
    S_BIND(SS_K4, SF_X, 52), S_RRECT(SCC_K4, 72, 62, 10, 10, 2),
    /* TONE (K3): the tilt on the machine's face, between the rollers */
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 0, 10), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 1, 4),
    S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 2, -4), S_BIND(SS_K3 | SS_CENTERED, SF_P0Y + 3, -10),
    S_PLINE(SCC_K3, 4), 62, 34, 76, 34, 92, 34, 106, 34,
    /* the spring tank, shaking on KICK */
    S_BIND(SS_HIT, SF_ANG, 2), S_BIND(SS_HIT, SF_Y, -3), S_PUSH(200, 74),
    S_STYLE(2), S_RRECT(SCC_INK, 168, 20, 66, 44, 5), S_STYLE(1),
    S_LINE(SCC_INK, 174, 30, 174, 54), S_LINE(SCC_INK, 228, 30, 228, 54),
    S_LINE(SCC_DIM, 176, 42, 226, 42),
    /* the spring (K4): lit past SPACE's middle (the spring mix), its coils bulging on KICK */
    S_BIND(SS_K4, SF_HIDE, 64), S_REP(7, 7, 0), S_BIND(SS_HIT, SF_R, 4), S_CIRC(SCC_K4, 180, 42, 8),
    S_POP,
    S_END,
};

/* ---- (0.15 design, third pass) the effects' pictures: one per effect on the FX button, its first page's four knobs ----
 * For an effect the UI fills: SS_K1..K4 the first page's four values, SS_ENV the effect's output level (the shared
 * effects: their return; the track's: the track's level after them), SS_HIT a note on the track (shared: on any track),
 * SS_LFO the effect's own modulator where it has one (AUTO PAN's pan, CHORUS's sweep, already scaled by its depth),
 * SS_PH1..PH4 the knob-rate phases, SS_TIME, SS_WOB. Every scene runs at 10 frames a second. */

/* FX (the track's sends): a teapot and three cups. DIST (K1) the steam off the pot; CHO (K2), DLY (K3) and REV (K4) how
 * full each cup is; the pot pours while the track sounds */
static const uint8_t SCENE_FX_SENDS[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the pot */
    S_STYLE(2), S_RRECT(SCC_INK, 30, 70, 56, 44, 18), S_STYLE(1),
    S_ARC(SCC_INK, 58, 70, 12, 128, 0), S_DISC(SCC_INK, 58, 56, 5),
    S_ARC(SCC_INK, 30, 92, 12, 64, 192),
    S_PLINE(SCC_INK, 3), 86, 96, 102, 80, 110, 76,
    /* the steam (K1 DIST): one to five curls */
    S_BIND(SS_K1, SF_CNT, 4), S_REP(1, 7, 0), S_BIND(SS_WOB, SF_X, 2),
    S_PLINE(SCC_K1, 4), 48, 52, 52, 44, 48, 36, 52, 28,
    /* the pour, while the track sounds */
    S_BIND(SS_ENV, SF_HIDE, 8), S_PLINE(SCC_TRK, 3), 112, 76, 122, 86, 128, 104,
    /* the cups and how full: CHO (K2), DLY (K3), REV (K4) */
    S_STYLE(2),
    S_RRECT(SCC_INK, 118, 98, 30, 28, 5), S_RRECT(SCC_INK, 160, 98, 30, 28, 5), S_RRECT(SCC_INK, 202, 98, 30, 28, 5),
    S_STYLE(1),
    S_ARC(SCC_INK, 148, 110, 6, 192, 64), S_ARC(SCC_INK, 190, 110, 6, 192, 64), S_ARC(SCC_INK, 232, 110, 6, 192, 64),
    S_BIND(SS_K2, SF_Y, -22), S_BIND(SS_K2, SF_Y2, 22), S_FILL(SCC_K2, 122, 122, 22, 1),
    S_BIND(SS_K3, SF_Y, -22), S_BIND(SS_K3, SF_Y2, 22), S_FILL(SCC_K3, 164, 122, 22, 1),
    S_BIND(SS_K4, SF_Y, -22), S_BIND(SS_K4, SF_Y2, 22), S_FILL(SCC_K4, 206, 122, 22, 1),
    S_END,
};

/* DIST: a toaster. Drive (K1) the coils glowing, TYPE (K2) the lever, TONE (K3) the dial, SLC (K4) the cord plugged into
 * the wall (into the SLICER); the toast pops on a note */
static const uint8_t SCENE_FX_DIST[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the toast, popping on a note */
    S_BIND(SS_HIT, SF_Y, -16), S_RRECT(SCC_INK, 72, 40, 26, 30, 6),
    S_BIND(SS_HIT, SF_Y, -16), S_RRECT(SCC_INK, 118, 40, 26, 30, 6),
    /* the toaster */
    S_STYLE(2), S_RRECT(SCC_INK, 56, 58, 108, 64, 14), S_STYLE(1),
    S_LINE(SCC_INK, 70, 64, 100, 64), S_LINE(SCC_INK, 116, 64, 146, 64),
    S_LINE(SCC_INK, 64, 122, 60, 127), S_LINE(SCC_INK, 156, 122, 160, 127),
    /* the coils (K1 drive): one to five glowing */
    S_BIND(SS_K1, SF_CNT, 4), S_REP(1, 0, 8),
    S_PLINE(SCC_K1, 7), 70, 76, 76, 72, 82, 76, 88, 72, 94, 76, 100, 72, 106, 76,
    /* the lever (K2 TYPE): down a notch a type */
    S_LINE(SCC_INK, 166, 70, 166, 112),
    S_BIND(SS_K2, SF_Y, 40), S_RRECT(SCC_K2, 162, 68, 14, 6, 2),
    /* the dial (K3 TONE) */
    S_CIRC(SCC_K3, 136, 98, 18),
    S_BIND(SS_K3 | SS_CENTERED, SF_ANG, 60), S_PUSH(136, 98), S_LINE(SCC_K3, 136, 98, 136, 90), S_POP,
    /* the cord and the socket (K4 SLC): plugged in only when the track goes through the SLICER */
    S_RRECT(SCC_INK, 214, 84, 16, 22, 3), S_DISC(SCC_INK, 219, 92, 2), S_DISC(SCC_INK, 225, 92, 2),
    S_PLINE(SCC_K4, 4), 56, 110, 40, 118, 180, 124, 198, 108,
    S_BIND(SS_K4, SF_HIDE, 64), S_LINE(SCC_K4, 198, 108, 214, 95),
    S_END,
};

/* AUTO PAN: a swing between two trees, a speaker under each. The swing goes left and right as the sound does (DEPTH,
 * K1, how far); birds fly across at the RATE (K2); a metronome stands under it in BEAT sync (K3); the sun's rays are the
 * WAVE (K4). The speaker on the side the swing is on gets louder */
static const uint8_t SCENE_FX_AUTOPAN[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the trees and the branch */
    S_STYLE(2), S_LINE(SCC_INK, 30, 127, 30, 36), S_LINE(SCC_INK, 210, 127, 210, 36), S_STYLE(1),
    S_CIRC(SCC_INK, 22, 26, 34), S_CIRC(SCC_INK, 218, 26, 34),
    S_LINE(SCC_INK, 30, 34, 210, 34),
    /* the sun and its rays (K4 WAVE: one to five) */
    S_CIRC(SCC_K4, 120, 14, 10),
    S_BIND(SS_K4, SF_CNT, 4), S_REP(1, 8, 0), S_LINE(SCC_K4, 104, 6, 100, 2),
    /* the birds (K2 RATE) */
    S_BIND(SS_PH2, SF_X, 200), S_PLINE(SCC_K2, 3), 10, 52, 16, 56, 22, 52,
    S_BIND(SS_PH2, SF_X, 200), S_PLINE(SCC_K2, 3), 26, 46, 30, 49, 34, 46,
    /* the speakers, the one on the swing's side louder */
    S_RECT(SCC_INK, 50, 104, 22, 22), S_BIND(SS_LFO | SS_CENTERED, SF_R, -6), S_CIRC(SCC_INK, 61, 115, 12),
    S_RECT(SCC_INK, 168, 104, 22, 22), S_BIND(SS_LFO | SS_CENTERED, SF_R, 6), S_CIRC(SCC_INK, 179, 115, 12),
    /* the metronome (K3 SYNC: there in BEAT) */
    S_BIND(SS_K3, SF_HIDE, 64), S_PGON(SCC_K3, 3), 112, 126, 120, 104, 128, 126,
    S_BIND(SS_K3, SF_HIDE, 64), S_BIND(SS_LFO, SF_ANG, -5), S_PUSH(120, 122), S_LINE(SCC_K3, 120, 122, 120, 108), S_POP,
    /* the swing (K1 DEPTH): the ropes and the seat, with the sound's pan */
    S_BIND(SS_LFO, SF_ANG, -16), S_PUSH(120, 34),
    S_LINE(SCC_K1, 108, 34, 108, 88), S_LINE(SCC_K1, 132, 34, 132, 88),
    S_STYLE(2), S_LINE(SCC_K1, 104, 88, 136, 88), S_STYLE(1),
    S_CIRC(SCC_INK, 120, 70, 10), S_LINE(SCC_INK, 120, 75, 120, 86),
    S_POP,
    S_END,
};

/* DELAY: a yodeler on a cliff and the echoes off the far wall of the canyon. TIME (K1) how far apart the echoes are,
 * FDBK (K2) how many, TONE (K3) the sun (high and bright, low and dark), MIX (K4) how big they are; the echoes travel
 * while the delay sounds, the yodeler sings on a note */
static const uint8_t SCENE_FX_DELAY[] = {
    SC_VERSION, 240, 132, 0,
    S_STYLE(2),
    S_PLINE(SCC_INK, 3), 0, 62, 44, 62, 50, 131,
    S_PLINE(SCC_INK, 3), 239, 48, 198, 48, 192, 131,
    S_STYLE(1),
    S_ARC(SCC_DIM, 120, 160, 70, 160, 224),
    /* the sun (K3 TONE) */
    S_BIND(SS_K3 | SS_CENTERED, SF_Y, -20), S_CIRC(SCC_K3, 150, 30, 12),
    /* the yodeler */
    S_CIRC(SCC_INK, 26, 36, 10), S_LINE(SCC_INK, 26, 41, 26, 54), S_LINE(SCC_INK, 26, 54, 20, 62), S_LINE(SCC_INK, 26, 54, 32, 62),
    S_LINE(SCC_INK, 26, 45, 34, 38), S_LINE(SCC_INK, 26, 45, 18, 50),
    S_BIND(SS_HIT, SF_HIDE, 30), S_CIRC(SCC_TRK, 32, 37, 4),
    /* the echoes (K2 FDBK: how many; K1 TIME: how far apart; K4 MIX: how big), travelling while it sounds */
    S_BIND(SS_K2, SF_CNT, 7), S_BIND(SS_K1, SF_DX, 14), S_REP(1, 10, 2),
    S_BIND(SS_K4, SF_R, 6), S_BIND(SS_TIME, SF_X, 10), S_BIND(SS_ENV, SF_HIDE, 8), S_ARC(SCC_K2, 50, 40, 6, 224, 32),
    /* the far wall answers: the same echoes coming back, smaller */
    S_BIND(SS_K2, SF_CNT, 4), S_BIND(SS_K1, SF_DX, -14), S_REP(1, -10, 4),
    S_BIND(SS_TIME, SF_X, -10), S_BIND(SS_ENV, SF_HIDE, 8), S_ARC(SCC_K1, 188, 60, 5, 96, 160),
    S_END,
};

/* REVERB: a cave. TYPE (K1) the bat hanging in it, SIZE (K2) the cave's mouth and its rings, DAMP (K3) the moss hanging from
 * the roof, PRE (K4) how high the drip falls from; the drip falls and the rings spread while it sounds */
static const uint8_t SCENE_FX_REVERB[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the cave's mouth (K2 SIZE) */
    S_STYLE(2), S_BIND(SS_K2, SF_R, 24), S_ARC(SCC_INK, 120, 127, 72, 128, 0), S_STYLE(1),
    /* the moss (K3 DAMP): more of it the more damped */
    S_BIND(SS_K3, SF_CNT, 7), S_REP(1, 12, 0), S_LINE(SCC_K3, 78, 74, 78, 84),
    /* the bat (K1 TYPE): wings wider as the type changes */
    S_LINE(SCC_INK, 120, 56, 120, 62),
    S_BIND(SS_K1, SF_P0X + 0, -10), S_BIND(SS_K1, SF_P0X + 4, 10),
    S_PLINE(SCC_K1, 5), 106, 64, 114, 70, 120, 64, 126, 70, 134, 64,
    /* the drip (K4 PRE: higher, the longer the pre-delay), falling */
    S_BIND(SS_K4, SF_Y, -20), S_BIND(SS_TIME, SF_Y, 30), S_DISC(SCC_K4, 120, 80, 4),
    /* the rings on the floor (K2 SIZE: how many), while it sounds */
    S_BIND(SS_K2, SF_CNT, 4), S_REP(1, 0, -5), S_BIND(SS_ENV, SF_HIDE, 8), S_BIND(SS_HIT, SF_R, 6),
    S_ARC(SCC_K2, 120, 124, 12, 144, 240),
    S_END,
};

/* CHORUS: three birds on a wire, singing. RATE (K1) the notes rising off them, DEPTH (K2) how far they sway (the sweep),
 * WIDE (K3) how far apart the outer two sit */
static const uint8_t SCENE_FX_CHORUS[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_INK, 16, 40, 16, 127), S_LINE(SCC_INK, 224, 40, 224, 127),
    S_PLINE(SCC_INK, 3), 16, 44, 120, 64, 224, 44,
    /* the notes (K1 RATE), rising */
    S_BIND(SS_PH1, SF_Y, -30), S_BIND(SS_ENV, SF_HIDE, 8), S_DISC(SCC_K1, 126, 34, 5),
    S_BIND(SS_PH1, SF_Y, -30), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SCC_K1, 128, 34, 128, 24),
    S_BIND(SS_PH1, SF_Y, -24), S_BIND(SS_ENV, SF_HIDE, 8), S_DISC(SCC_K1, 92, 40, 5),
    S_BIND(SS_PH1, SF_Y, -24), S_BIND(SS_ENV, SF_HIDE, 8), S_LINE(SCC_K1, 94, 40, 94, 30),
    /* the middle bird, swaying with the sweep (K2 DEPTH) */
    S_BIND(SS_LFO, SF_ANG, 6), S_PUSH(120, 62),
    S_CIRC(SCC_K2, 120, 54, 16), S_DISC(SCC_K2, 126, 44, 7), S_LINE(SCC_K2, 129, 44, 135, 45),
    S_POP,
    /* the outer two (K3 WIDE: further apart), swaying the other way */
    S_BIND(SS_K3, SF_X, -40), S_BIND(SS_LFO, SF_ANG, -6), S_PUSH(84, 56),
    S_CIRC(SCC_K3, 84, 48, 14), S_DISC(SCC_K3, 79, 39, 6), S_LINE(SCC_K3, 76, 39, 71, 40),
    S_POP,
    S_BIND(SS_K3, SF_X, 40), S_BIND(SS_LFO, SF_ANG, -6), S_PUSH(156, 56),
    S_CIRC(SCC_K3, 156, 48, 14), S_DISC(SCC_K3, 161, 39, 6), S_LINE(SCC_K3, 164, 39, 169, 40),
    S_POP,
    S_END,
};

/* SLICER: a loaf on a board and a knife. MODE (K1) the lamp (off, GATE, STUT), PAT (K2) the cuts in the loaf, RATE (K3) the
 * knife chopping, DEPTH (K4) the crumbs */
static const uint8_t SCENE_FX_SLICER[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    /* the lamp (K1 MODE) */
    S_LINE(SCC_INK, 30, 0, 30, 20), S_PLINE(SCC_INK, 3), 20, 30, 30, 20, 40, 30,
    S_BIND(SS_K1, SF_HIDE, 30), S_BIND(SS_K1, SF_R, 6), S_DISC(SCC_K1, 30, 34, 6),
    /* the board and the loaf */
    S_STYLE(2), S_RRECT(SCC_INK, 20, 108, 200, 12, 4), S_STYLE(1),
    S_RRECT(SCC_INK, 40, 70, 120, 38, 16),
    /* the cuts (K2 PAT) */
    S_BIND(SS_K2, SF_CNT, 7), S_REP(1, 13, 0), S_LINE(SCC_K2, 56, 74, 56, 106),
    /* the crumbs (K4 DEPTH) */
    S_BIND(SS_K4, SF_CNT, 6), S_REP(1, 9, 0), S_BIND(SS_WOB, SF_Y, 1), S_DISC(SCC_K4, 170, 104, 3),
    /* the knife (K3 RATE), chopping at the slicer's rate while it sounds */
    S_BIND(SS_PH3, SF_Y, 16), S_PUSH(150, 50),
    S_PGON(SCC_K3, 4), 118, 42, 162, 42, 162, 52, 118, 62,
    S_STYLE(2), S_LINE(SCC_INK, 162, 47, 190, 47), S_STYLE(1),
    S_POP,
    S_END,
};

/* MASTER: a boombox. DJF (K1) the slider in the middle (left darker, right thinner), CLIP (K2) the sparks off the top,
 * LEVEL (K3) the meter's needle, SPK (K4) the rings round the woofers; the cones pump with the output */
static const uint8_t SCENE_FX_MASTER[] = {
    SC_VERSION, 240, 132, 0,
    S_LINE(SCC_DIM, 0, 127, 239, 127),
    S_PLINE(SCC_INK, 4), 70, 50, 80, 30, 160, 30, 170, 50,
    S_STYLE(2), S_RRECT(SCC_INK, 24, 50, 192, 74, 10), S_STYLE(1),
    /* the woofers, pumping with the output */
    S_CIRC(SCC_INK, 66, 92, 44), S_BIND(SS_ENV, SF_R, 10), S_CIRC(SCC_INK, 66, 92, 14),
    S_CIRC(SCC_INK, 174, 92, 44), S_BIND(SS_ENV, SF_R, 10), S_CIRC(SCC_INK, 174, 92, 14),
    /* SPK (K4): the rings, one for LOWCUT, two for BASS+ */
    S_BIND(SS_K4, SF_HIDE, 40), S_CIRC(SCC_K4, 66, 92, 52), S_BIND(SS_K4, SF_HIDE, 40), S_CIRC(SCC_K4, 174, 92, 52),
    S_BIND(SS_K4, SF_HIDE, 100), S_CIRC(SCC_K4, 66, 92, 30), S_BIND(SS_K4, SF_HIDE, 100), S_CIRC(SCC_K4, 174, 92, 30),
    /* the DJ slider (K1) */
    S_LINE(SCC_DIM, 100, 62, 140, 62), S_LINE(SCC_DIM, 120, 58, 120, 66),
    S_BIND(SS_K1 | SS_CENTERED, SF_X, 36), S_FILL(SCC_K1, 117, 58, 6, 9),
    /* the meter (K3 LEVEL): its needle with the output, set by the level */
    S_ARC(SCC_INK, 120, 112, 16, 160, 224),
    S_BIND(SS_K3 | SS_CENTERED, SF_ANG, 14), S_BIND(SS_ENV, SF_ANG, 10), S_PUSH(120, 112),
    S_LINE(SCC_K3, 120, 112, 108, 100),
    S_POP,
    /* CLIP (K2): sparks off the handle, more of them the harder it clips */
    S_BIND(SS_K2, SF_CNT, 4), S_REP(1, 12, 0), S_BIND(SS_ENV, SF_HIDE, 8),
    S_PLINE(SCC_K2, 4), 92, 24, 96, 18, 92, 14, 96, 8,
    S_END,
};
