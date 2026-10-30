/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The engine picture ("scene") of the new HOME page: its format (docs/orangecat/UI_DESIGN.md, "The pictures" and "The
 * scene format"; docs/orangecat/UI_BUILD.md step 1b). The interpreter is oc_scene.c; the built-in engines' scenes are
 * oc_scenes.h, carried in the asset pack's scene trailer (tools/gen_pack.py --scenes), never in the image.
 *
 * A scene is DATA: a short byte stream a module carries in its own image (its pages descriptor, fw_pages.h version 3; an
 * engine or effect not yet a module: an entry in the asset pack), interpreted by the firmware every frame, read in place
 * from flash. The firmware image holds no scene, and nothing is copied into RAM: the picture is there only while its module
 * is (UI_DESIGN.md 0.2, the owner's rule). No engine code runs to draw it, and it keeps no state: everything that moves is
 * a BINDING from a SOURCE (a knob, the sound) to a field of a primitive.
 *
 * The stream:
 *   byte 0  SC_VERSION (1)
 *   byte 1  w, byte 2 h   the design box (240 x 132: the picture of HOME B; HOME A's taller box gets the extra rows as
 *                         sky above it: the scene is anchored to the bottom of the region)
 *   byte 3  flags (0)
 *   then ops, ended by SO_END. An op's first byte: bits 0..4 the op, bits 5..7 its color slot (SC_*).
 *
 *   op        bytes after the op byte                           draws
 *   LINE      x0 y0 x1 y1                                       a line
 *   PLINE     n, then n points (x y)                            an open polyline (n <= 16)
 *   PGON      n, then n points                                  a closed polyline (outline only: no fills)
 *   RECT      x y w h                                           an outline
 *   RRECT     x y w h r                                         an outline with round corners
 *   CIRC      cx cy d                                           a ring, diameter d
 *   DISC      cx cy d                                           a filled disc
 *   ARC       cx cy r a0 a1                                     an arc, angles in 1/256 turn clockwise from 3 o'clock;
 *                                                               a1 <= a0: it runs on past 3 o'clock
 *   FILL      x y w h                                           a filled rectangle (a small part: a lamp, a key)
 *   SCOPE     x y w h                                           the track's live waveform in the box
 *   PUSH      px py                                             a group: a pivot; the group's binds rotate it about the
 *                                                               pivot (ANG) and move it (X, Y); up to 2 deep. Lines,
 *                                                               polylines, arcs, RRECT and circles' centers turn
 *                                                               with it; RECT and FILL only move (they stay upright)
 *   POP                                                         the group ends
 *   REP       n dx dy                                           the next op drawn n times, dx dy (signed) apart
 *   STYLE     t                                                 the line weight from here on: 1 or 2 px
 *   BIND      src field depth                                   binds the NEXT drawing op (or PUSH, or REP) (up to 4)
 * Coordinates are unsigned bytes in the design box; dx, dy and depth are signed bytes.
 *
 * Color slots: SCC_INK the line art (a light neutral), SCC_K1..SCC_K4 the four knobs' colors (blue, green, white,
 * orange: the knob cozies'), SCC_TRK the track's color, SCC_DIM a quiet line (a horizon, stars), SCC_BG the ground
 * (draws as erase).
 *
 * Sources (Q8: 0 .. 256 unless said; the UI fills them each frame from the track):
 *   SS_K1..SS_K4   the knob's position, 0 .. 256 (a macro's: its offset -64 .. 63 mapped so 128 is the preset)
 *   SS_ENV         the loudest voice's amplitude envelope
 *   SS_HIT         256 at a note-on, falling to 0 over about 160 ms
 *   SS_GATE        256 while a key (or step) holds a note, else 0
 *   SS_LFO         the track's LFO, -256 .. 256
 *   SS_NOTE        the last note's pitch class, 0 .. 256 (C .. B)
 *   SS_TIME        a 1 s sawtooth, 0 .. 256, the same for every scene (the UI's clock)
 *   SS_WOB         a 0.5 Hz sine of the clock, -256 .. 256 (bobbing)
 *   SS_PH1..SS_PH4 a sawtooth whose RATE is knob 1..4 (0.1 Hz at 0 to 3 Hz at full): a spinning rotor, a rolling ball
 *                  (the only state: four 16-bit phases a track, kept by the UI)
 *   | SS_CENTERED   the source minus 128 (a knob as -128 .. 128 about its middle: a macro's two halves)
 * Fields: SF_X SF_Y (position; a polyline or group: all of it), SF_X2 SF_Y2 (the second point, or w h), SF_R (diameter,
 * radius, corner), SF_A0 SF_A1 (an arc's ends), SF_ANG (a group's rotation, 1/256 turn per 128 of depth x source),
 * SF_CNT (REP's count), SF_DX SF_DY (REP's step), SF_HIDE (hidden while source < 2 x depth; depth < 0: while
 * source > -2 x depth), SF_P0X + i, SF_P0Y + i (point i of a polyline).
 * A bound field: value + depth x source / 256 (SF_ANG: 2 x depth x source / 256).
 *
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version.
 *   (second pass) scenes live in modules (or the asset pack), never in the image; an effect's sources (SCENE_DUB, ui_scenes.h).
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1b) moved from tests/ui_scene.h into firmware/src: the format here, the
 *   interpreter and its checker in oc_scene.c.
 */
#pragma once
#include <stdint.h>

#define SC_VERSION 1u
enum { SO_END, SO_LINE, SO_PLINE, SO_PGON, SO_RECT, SO_RRECT, SO_CIRC, SO_DISC, SO_ARC, SO_FILL, SO_SCOPE, SO_PUSH,
       SO_POP, SO_REP, SO_STYLE, SO_BIND = 31 };
enum { SCC_INK, SCC_K1, SCC_K2, SCC_K3, SCC_K4, SCC_TRK, SCC_DIM, SCC_BG };
enum { SS_K1, SS_K2, SS_K3, SS_K4, SS_ENV, SS_HIT, SS_GATE, SS_LFO, SS_NOTE, SS_TIME, SS_WOB, SS_PH1, SS_PH2, SS_PH3,
       SS_PH4, SS_N, SS_CENTERED = 0x80 };
enum { SF_X, SF_Y, SF_X2, SF_Y2, SF_R, SF_A0, SF_A1, SF_ANG, SF_CNT, SF_DX, SF_DY, SF_HIDE, SF_P0X = 16, SF_P0Y = 32 };

/* authoring macros: a scene is a const uint8_t array of these */
#define S_OP(op, c) (uint8_t)((op) | ((c) << 5))
#define S_LINE(c, x0, y0, x1, y1) S_OP(SO_LINE, c), x0, y0, x1, y1
#define S_RECT(c, x, y, w, h) S_OP(SO_RECT, c), x, y, w, h
#define S_RRECT(c, x, y, w, h, r) S_OP(SO_RRECT, c), x, y, w, h, r
#define S_CIRC(c, x, y, d) S_OP(SO_CIRC, c), x, y, d
#define S_DISC(c, x, y, d) S_OP(SO_DISC, c), x, y, d
#define S_ARC(c, x, y, r, a0, a1) S_OP(SO_ARC, c), x, y, r, a0, a1
#define S_FILL(c, x, y, w, h) S_OP(SO_FILL, c), x, y, w, h
#define S_SCOPE(c, x, y, w, h) S_OP(SO_SCOPE, c), x, y, w, h
#define S_PUSH(px, py) S_OP(SO_PUSH, 0), px, py
#define S_POP S_OP(SO_POP, 0)
#define S_REP(n, dx, dy) S_OP(SO_REP, 0), n, (uint8_t)(int8_t)(dx), (uint8_t)(int8_t)(dy)
#define S_STYLE(t) S_OP(SO_STYLE, 0), t
#define S_BIND(src, f, d) SO_BIND, (uint8_t)(src), (uint8_t)(f), (uint8_t)(int8_t)(d)
#define S_PLINE(c, n) S_OP(SO_PLINE, c), n
#define S_PGON(c, n) S_OP(SO_PGON, c), n
#define S_END SO_END

typedef struct {
    int16_t s[SS_N];             /* the sources, Q8 */
    uint16_t col[8];             /* the color slots, RGB565 */
    int16_t scope[64];           /* SCOPE: the last 64 samples of the track's output, -256 .. 256 */
} scene_in_t;
