/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The engine picture ("scene") of the new HOME page: its format and its interpreter (docs/orangecat/UI_DESIGN.md,
 * "The pictures" and "The scene format"). Host only for now (tests/ui_mockup.c draws the mockups with it); written
 * against firmware/src/gfx.c's canvas and primitives so a later session can move it into firmware/src unchanged.
 *
 * A scene is DATA: a short byte stream an engine carries (a built-in engine in its engine_t, a loadable engine in its
 * pages descriptor, fw_pages.h version 3), interpreted by the firmware every frame. No engine code runs to draw it, and
 * it keeps no state: everything that moves is a BINDING from a SOURCE (a knob, the sound) to a field of a primitive.
 *
 * The stream:
 *   byte 0  SC_VERSION (1)
 *   byte 1  w, byte 2 h   the design box (240 x 132: the picture of HOME B; HOME A's taller box gets the extra rows as
 *                         sky above it: the scene is anchored to the bottom of the region)
 *   byte 3  flags (0)
 *   then ops, ended by SO_END. An op's first byte: bits 0..4 the op, bits 5..7 its colour slot (SC_*).
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
 *                                                               polylines, arcs, RRECT and circles' centres turn
 *                                                               with it; RECT and FILL only move (they stay upright)
 *   POP                                                         the group ends
 *   REP       n dx dy                                           the next op drawn n times, dx dy (signed) apart
 *   STYLE     t                                                 the line weight from here on: 1 or 2 px
 *   BIND      src field depth                                   binds the NEXT drawing op (or PUSH, or REP) (up to 4)
 * Coordinates are unsigned bytes in the design box; dx, dy and depth are signed bytes.
 *
 * Colour slots: SC_INK the line art (a light neutral), SC_K1..SC_K4 the four knobs' colours (blue, green, white,
 * orange: the knob cozies'), SC_TRK the track's colour, SC_DIM a quiet line (a horizon, stars), SC_BG the ground
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
 *   | SS_CENTRED   the source minus 128 (a knob as -128 .. 128 about its middle: a macro's two halves)
 * Fields: SF_X SF_Y (position; a polyline or group: all of it), SF_X2 SF_Y2 (the second point, or w h), SF_R (diameter,
 * radius, corner), SF_A0 SF_A1 (an arc's ends), SF_ANG (a group's rotation, 1/256 turn per 128 of depth x source),
 * SF_CNT (REP's count), SF_DX SF_DY (REP's step), SF_HIDE (hidden while source < 2 x depth; depth < 0: while
 * source > -2 x depth), SF_P0X + i, SF_P0Y + i (point i of a polyline).
 * A bound field: value + depth x source / 256 (SF_ANG: 2 x depth x source / 256).
 *
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version.
 */
#pragma once

#define SC_VERSION 1u
enum { SO_END, SO_LINE, SO_PLINE, SO_PGON, SO_RECT, SO_RRECT, SO_CIRC, SO_DISC, SO_ARC, SO_FILL, SO_SCOPE, SO_PUSH,
       SO_POP, SO_REP, SO_STYLE, SO_BIND = 31 };
enum { SC_INK, SC_K1, SC_K2, SC_K3, SC_K4, SC_TRK, SC_DIM, SC_BG };
enum { SS_K1, SS_K2, SS_K3, SS_K4, SS_ENV, SS_HIT, SS_GATE, SS_LFO, SS_NOTE, SS_TIME, SS_WOB, SS_PH1, SS_PH2, SS_PH3,
       SS_PH4, SS_N, SS_CENTRED = 0x80 };
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
    uint16_t col[8];             /* the colour slots, RGB565 */
    int16_t scope[64];           /* SCOPE: the last 64 samples of the track's output, -256 .. 256 */
} scene_in_t;

/* ---- drawing helpers (on gfx.c's canvas) ---- */
/* an anti-aliased line (Wu's), 1 px; in 1/16 px coordinates so a rotated part moves smoothly */
static void sc_plot(int32_t x, int32_t y, uint16_t c, int32_t a)        /* a: 0 .. 256 coverage */
{
    uint16_t *p;
    y += CV_OY;
    if ((uint32_t)x >= cv_w || (uint32_t)y >= cv_h || a <= 0)
        return;
    p = &cv_px[(uint32_t)y * cv_w + (uint32_t)x];
    *p = swap16(mix565(swap16(*p), c, (uint32_t)(a > 255 ? 32 : a / 8)));
}
static void sc_aaline(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)   /* 1/16 px */
{
    int32_t dx = x1 - x0, dy = y1 - y0, steep = (dy < 0 ? -dy : dy) > (dx < 0 ? -dx : dx), t, n, i;
    if (steep) { t = x0; x0 = y0; y0 = t; t = x1; x1 = y1; y1 = t; }
    if (x0 > x1) { t = x0; x0 = x1; x1 = t; t = y0; y0 = y1; y1 = t; }
    dx = x1 - x0; dy = y1 - y0;
    n = (x1 >> 4) - (x0 >> 4);
    for (i = 0; i <= n; i++) {
        int32_t x = (x0 >> 4) + i, y = dx ? y0 + dy * ((x << 4) + 8 - x0) / dx : y0;   /* 1/16 (fits 32 bits: 3,840 x 3,840) */
        int32_t yi = (y - 8) >> 4, f = (y - 8) & 15;
        if (steep) { sc_plot(yi, x, c, (16 - f) * 16); sc_plot(yi + 1, x, c, f * 16); }
        else { sc_plot(x, yi, c, (16 - f) * 16); sc_plot(x, yi + 1, c, f * 16); }
    }
}
static int32_t sc_weight = 1;
static void sc_line16(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)
{
    sc_aaline(x0, y0, x1, y1, c);
    if (sc_weight > 1) {                                 /* 2 px: a second line beside it, across its direction */
        int32_t dx = x1 - x0, dy = y1 - y0, ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
        if (ax >= ay) sc_aaline(x0, y0 + 12, x1, y1 + 12, c);
        else sc_aaline(x0 + 12, y0, x1 + 12, y1, c);
    }
}
/* sine and cosine of a in 1/256 turn, Q12, from a quarter wave (130 B; the firmware could read its SINE table instead) */
static const int16_t SC_SINQ[65] = {
    0, 101, 201, 301, 401, 501, 601, 700, 799, 897, 995, 1092, 1189, 1285, 1380, 1474, 1567, 1660, 1751, 1842, 1931, 2019,
    2106, 2191, 2276, 2359, 2440, 2520, 2598, 2675, 2751, 2824, 2896, 2967, 3035, 3102, 3166, 3229, 3290, 3349, 3406, 3461,
    3513, 3564, 3612, 3659, 3703, 3745, 3784, 3822, 3857, 3889, 3920, 3948, 3973, 3996, 4017, 4036, 4052, 4065, 4076, 4085,
    4091, 4095, 4096};
static int32_t sc_sin(int32_t a)
{
    a &= 255;
    return a < 64 ? SC_SINQ[a] : a < 128 ? SC_SINQ[128 - a] : a < 192 ? -SC_SINQ[a - 128] : -SC_SINQ[256 - a];
}
static int32_t sc_cos(int32_t a) { return sc_sin(a + 64); }

typedef struct { int32_t px, py, tx, ty, c, s, ang; } sc_xf_t; /* a group: pivot, then rotate (c, s Q12), then move */
static sc_xf_t sc_xf[3];
static uint32_t sc_depth;
static void sc_map(int32_t x, int32_t y, int32_t *ox, int32_t *oy)  /* design px -> region 1/16 px */
{
    int32_t X = x << 4, Y = y << 4;
    for (int32_t d = (int32_t)sc_depth; d >= 1; d--) {
        const sc_xf_t *g = &sc_xf[d];
        int32_t rx = X - (g->px << 4), ry = Y - (g->py << 4);
        X = (g->px << 4) + (rx * g->c - ry * g->s) / 4096 + (g->tx << 4);
        Y = (g->py << 4) + (rx * g->s + ry * g->c) / 4096 + (g->ty << 4);
    }
    *ox = X + (sc_xf[0].tx << 4);
    *oy = Y + (sc_xf[0].ty << 4);
}
static int32_t sc_ang(void)                          /* the groups' rotation, 1/256 turn */
{
    int32_t a = 0;
    for (uint32_t d = 1; d <= sc_depth; d++)
        a += sc_xf[d].ang;
    return a;
}

/* ---- the interpreter ---- */
typedef struct { uint8_t src, field; int8_t depth; } sc_bind_t;
static int32_t sc_val(const scene_in_t *in, const sc_bind_t *b)
{
    int32_t v = in->s[b->src & 0x7Fu];
    return (b->src & SS_CENTRED) ? v - 128 : v;
}
/* the bound offset of field f (0 when unbound) */
static int32_t sc_mod(const scene_in_t *in, const sc_bind_t *b, uint32_t nb, uint32_t f, int32_t *hide)
{
    int32_t o = 0;
    for (uint32_t i = 0; i < nb; i++) {
        int32_t v = sc_val(in, &b[i]);
        if (b[i].field == SF_HIDE) {
            if (b[i].depth >= 0 ? v < 2 * b[i].depth : v > -2 * b[i].depth) *hide = 1;
        } else if (b[i].field == f) {
            o += f == SF_ANG ? 2 * b[i].depth * v / 256 : b[i].depth * v / 256;
        }
    }
    return o;
}
static void sc_arc16(int32_t cx, int32_t cy, int32_t r, int32_t a0, int32_t a1, uint16_t c)   /* design px, 1/256 turn */
{
    int32_t a, x0, y0, x1, y1, step = 8, rot = sc_ang();
    if (a1 <= a0) a1 += 256;                            /* (an arc across 3 o'clock: a1 below a0) */
    sc_map(cx, cy, &cx, &cy);                           /* (the centre mapped; the arc turns with the group) */
    a0 += rot; a1 += rot;
    x0 = cx + r * 16 * sc_cos(a0) / 4096; y0 = cy + r * 16 * sc_sin(a0) / 4096;
    for (a = a0 + step;; a += step) {
        if (a > a1) a = a1;
        x1 = cx + r * 16 * sc_cos(a) / 4096; y1 = cy + r * 16 * sc_sin(a) / 4096;
        sc_line16(x0, y0, x1, y1, c);
        x0 = x1; y0 = y1;
        if (a == a1) break;
    }
}
/* bytes the op takes after its op byte (0xFF: the polyline's count decides) */
static uint32_t sc_len(const uint8_t *p)
{
    switch (p[0] & 31u) {
    case SO_LINE: case SO_RECT: case SO_FILL: case SO_SCOPE: return 4;
    case SO_RRECT: case SO_ARC: return 5;
    case SO_CIRC: case SO_DISC: case SO_REP: return 3;
    case SO_PUSH: return 2;
    case SO_STYLE: return 1;
    case SO_BIND: return 3;
    case SO_PLINE: case SO_PGON: return 1u + 2u * p[1];
    default: return 0;
    }
}
/* draw scene `s` with its box's top-left at region (ox, oy); returns the bytes read (the scene's size) */
static uint32_t scene_draw(const uint8_t *s, int32_t ox, int32_t oy, const scene_in_t *in)
{
    const uint8_t *p = s + 4;
    sc_bind_t b[4];
    uint32_t nb = 0, rep = 1;
    int32_t rdx = 0, rdy = 0;
    sc_depth = 0;
    sc_weight = 1;
    sc_xf[0] = (sc_xf_t){0, 0, ox, oy, 4096, 0, 0};
    while (*p != SO_END) {
        uint32_t op = p[0] & 31u, cs = p[0] >> 5, len = sc_len(p), k;
        uint16_t c = in->col[cs];
        int32_t hide = 0;
        if (op == SO_BIND) {
            if (nb < 4u) b[nb++] = (sc_bind_t){p[1], p[2], (int8_t)p[3]};
            p += 4;
            continue;
        }
        if (op == SO_STYLE) { sc_weight = p[1]; p += 2; continue; }
        if (op == SO_REP) {                             /* the next op n times */
            rep = (uint32_t)(p[1] + sc_mod(in, b, nb, SF_CNT, &hide));
            rdx = (int8_t)p[2] + sc_mod(in, b, nb, SF_DX, &hide);
            rdy = (int8_t)p[3] + sc_mod(in, b, nb, SF_DY, &hide);
            if (hide || (int32_t)rep < 0) rep = 0;
            nb = 0;
            p += 4;
            continue;
        }
        if (op == SO_PUSH) {
            if (sc_depth < 2u) {
                int32_t a = sc_mod(in, b, nb, SF_ANG, &hide);
                sc_xf_t *g = &sc_xf[++sc_depth];
                g->px = p[1]; g->py = p[2];
                g->tx = sc_mod(in, b, nb, SF_X, &hide);
                g->ty = sc_mod(in, b, nb, SF_Y, &hide);
                g->c = sc_cos(a); g->s = sc_sin(a); g->ang = a;
            }
            nb = 0;
            p += 3;
            continue;
        }
        if (op == SO_POP) { if (sc_depth) sc_depth--; p++; continue; }
        for (k = 0; k < rep && !hide; k++) {
            int32_t dx = (int32_t)k * rdx + sc_mod(in, b, nb, SF_X, &hide), dy = (int32_t)k * rdy + sc_mod(in, b, nb, SF_Y, &hide);
            int32_t x0, y0, x1, y1;
            if (hide) break;
            switch (op) {
            case SO_LINE:
                sc_map(p[1] + dx, p[2] + dy, &x0, &y0);
                sc_map(p[3] + dx + sc_mod(in, b, nb, SF_X2, &hide), p[4] + dy + sc_mod(in, b, nb, SF_Y2, &hide), &x1, &y1);
                sc_line16(x0, y0, x1, y1, c);
                break;
            case SO_PLINE: case SO_PGON: {
                uint32_t n = p[1], i;
                int32_t fx = 0, fy = 0, lx = 0, ly = 0;
                for (i = 0; i < n; i++) {
                    int32_t X = p[2 + 2 * i] + dx + sc_mod(in, b, nb, SF_P0X + i, &hide);
                    int32_t Y = p[3 + 2 * i] + dy + sc_mod(in, b, nb, SF_P0Y + i, &hide);
                    sc_map(X, Y, &x1, &y1);
                    if (i) sc_line16(lx, ly, x1, y1, c); else { fx = x1; fy = y1; }
                    lx = x1; ly = y1;
                }
                if (op == SO_PGON && n > 2u) sc_line16(lx, ly, fx, fy, c);
                break;
            }
            case SO_RECT: case SO_FILL: case SO_RRECT: {
                int32_t w = p[3] + sc_mod(in, b, nb, SF_X2, &hide), h = p[4] + sc_mod(in, b, nb, SF_Y2, &hide);
                sc_map(p[1] + dx, p[2] + dy, &x0, &y0);
                x0 >>= 4; y0 >>= 4;
                if (w <= 0 || h <= 0) break;
                if (op == SO_FILL) { cv_rect(x0, y0, w, h, c); break; }
                if (op == SO_RECT) { cv_frame(x0, y0, w, h, c); if (sc_weight > 1) cv_frame(x0 + 1, y0 + 1, w - 2, h - 2, c); break; }
                {   /* RRECT: an outline with quarter-arc corners */
                    int32_t r = p[5] + sc_mod(in, b, nb, SF_R, &hide), X = p[1] + dx, Y = p[2] + dy;
                    if (r > w / 2) r = w / 2;
                    if (r > h / 2) r = h / 2;
                    if (r < 0) r = 0;
                    int32_t a, bx, by, ex, ey;
                    sc_map(X + r, Y, &a, &by); sc_map(X + w - 1 - r, Y, &ex, &ey); sc_line16(a, by, ex, ey, c);
                    sc_map(X + r, Y + h - 1, &a, &by); sc_map(X + w - 1 - r, Y + h - 1, &ex, &ey); sc_line16(a, by, ex, ey, c);
                    sc_map(X, Y + r, &bx, &a); sc_map(X, Y + h - 1 - r, &ex, &ey); sc_line16(bx, a, ex, ey, c);
                    sc_map(X + w - 1, Y + r, &bx, &a); sc_map(X + w - 1, Y + h - 1 - r, &ex, &ey); sc_line16(bx, a, ex, ey, c);
                    if (r > 0) {
                        sc_arc16(X + r, Y + r, r, 128, 192, c);
                        sc_arc16(X + w - 1 - r, Y + r, r, 192, 256, c);
                        sc_arc16(X + w - 1 - r, Y + h - 1 - r, r, 0, 64, c);
                        sc_arc16(X + r, Y + h - 1 - r, r, 64, 128, c);
                    }
                }
                break;
            }
            case SO_CIRC: case SO_DISC: {
                int32_t d = p[3] + sc_mod(in, b, nb, SF_R, &hide);
                if (d < 1) break;
                sc_map(p[1] + dx, p[2] + dy, &x0, &y0);
                cv_disc((x0 + 8) >> 4, (y0 + 8) >> 4, d, c, op == SO_CIRC);
                if (op == SO_CIRC && sc_weight > 1 && d > 3) cv_disc((x0 + 8) >> 4, (y0 + 8) >> 4, d - 2, c, 1);
                break;
            }
            case SO_ARC: {
                int32_t r = p[3] + sc_mod(in, b, nb, SF_R, &hide);
                int32_t a0 = p[4] + sc_mod(in, b, nb, SF_A0, &hide), a1 = p[5] + sc_mod(in, b, nb, SF_A1, &hide);
                if (r > 0 && a0 != a1) sc_arc16(p[1] + dx, p[2] + dy, r, a0, a1, c);
                break;
            }
            case SO_SCOPE: {
                int32_t w = p[3], h = p[4], i, px = 0, py = 0;
                for (i = 0; i < 64; i++) {
                    int32_t X = p[1] + dx + i * w / 63, Y = p[2] + dy + h / 2 - in->scope[i] * (h / 2) / 256;
                    sc_map(X, Y, &x1, &y1);
                    if (i) sc_line16(px, py, x1, y1, c);
                    px = x1; py = y1;
                }
                break;
            }
            default:
                break;
            }
        }
        rep = 1; rdx = rdy = 0;
        nb = 0;
        p += 1u + len;
    }
    return (uint32_t)(p - s) + 1u;
}
