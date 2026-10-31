/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface (OC_HOME_PIC; docs/orangecat/UI_BUILD.md step 1b): the interpreter of HOME's pictures (the scene format:
 * oc_scene.h), on gfx.c's canvas, its checker, and where a built-in engine's scene is found.
 *
 * The interpreter (scene_draw) is the one the design's mockups were drawn with (tests/ui_mockup.c includes this file), moved
 * here unchanged but for a cap on REP's count. It keeps no state between frames; its working state (the group transforms,
 * the line weight) is static. It trusts its scene: scene_ok walks it first, every time, with the interpreter's own length rule.
 *
 * Where a scene is (UI_DESIGN.md section 3; the owner's decision 5): never in the image. On the unit the built-in engines'
 * scenes are in the asset pack's SCENE TRAILER (tools/gen_pack.py --scenes), after the pack's tables and outside its table
 * hash, length and payload CRC, so a pack with or without it is the same pack to every check and every module:
 *     0   magic "OCSC" (u32)         4  version 1 (u16)          6  count (u16)
 *     8   length: head, entries and scenes (u32)                12  CRC-32 of bytes 16 .. length (u32)
 *     16  entries, 6 bytes each: engine (u8), 0 (u8), offset from the trailer's first byte (u16), length (u16); 0xFF padding
 *         (gen_pack.py moves the scenes on by 4 bytes until no sector tail looks like an update record); the scenes.
 * The trailer starts at the pack's length rounded up to 4. It is read in place through the plaintext window, checked (magic, version, inside the
 * pack area, CRC) at every lookup: about 600 bytes, a few microseconds at 10 frames a second, and no state to go stale when a
 * pack is installed or erased. On the host the scenes are oc_scenes.h's arrays.
 *
 * Included from ui_draw.c under OC_HOME_PIC.
 *
 * Revisions:
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1b) first version: the interpreter of tests/ui_scene.h, scene_ok, the
 *   pack's scene trailer; coordinates scaled by * 16, not << 4 (a negative shifted left is undefined in C: the sanitizer);
 *   the interpreter ends on op 0 whatever its color bits, as the checker reads it (the fuzz: the two walks parted at 0x20);
 *   after the review: the sources clamped to -256 .. 256 and a disc's and an arc's size to 255 (a frame's cost is bounded:
 *   UI_BUILD.md 1b), the trailer found through FL_XIP (oc_pack.c pk_xip), not a generated address.
 */
#ifndef OC_SCENE_C                                      /* (once: tests include it through tests/ui_scene.h as well) */
#define OC_SCENE_C 1
#include "oc_scene.h"
static inline int32_t sc_clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }   /* (dsp.c's clamp is not
                                                                 * in the mockups' build) */
#define SC_REP_MAX 32                                   /* REP draws at most this many copies */
#define SC_MAX 1024u                                    /* a scene's bytes, at most */
#define OC_SCN_MAGIC 0x4353434Fu                        /* "OCSC" */
#define OC_SCN_HEAD 16u

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
        int32_t x = (x0 >> 4) + i, y = dx ? y0 + dy * ((x * 16) + 8 - x0) / dx : y0;   /* 1/16 (fits 32 bits: 3,840 x 3,840) */
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
    int32_t X = x * 16, Y = y * 16;
    for (int32_t d = (int32_t)sc_depth; d >= 1; d--) {
        const sc_xf_t *g = &sc_xf[d];
        int32_t rx = X - (g->px * 16), ry = Y - (g->py * 16);
        X = (g->px * 16) + (rx * g->c - ry * g->s) / 4096 + (g->tx * 16);
        Y = (g->py * 16) + (rx * g->s + ry * g->c) / 4096 + (g->ty * 16);
    }
    *ox = X + (sc_xf[0].tx * 16);
    *oy = Y + (sc_xf[0].ty * 16);
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
    int32_t v = sc_clamp(in->s[b->src & 0x7Fu], -256, 256);   /* (the sources' range: every product below stays in 32 bits) */
    return (b->src & SS_CENTERED) ? v - 128 : v;
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
    sc_map(cx, cy, &cx, &cy);                           /* (the center mapped; the arc turns with the group) */
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
    while ((*p & 31u) != SO_END) {                     /* (op 0 ends, whatever its color bits: as scene_ok reads it) */
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
            if ((int32_t)rep > SC_REP_MAX) rep = SC_REP_MAX;   /* (a bound count: a scene cannot make a frame endless) */
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
                int32_t d = sc_clamp(p[3] + sc_mod(in, b, nb, SF_R, &hide), 0, 255);   /* (bound: a frame's cost too) */
                if (d < 1) break;
                sc_map(p[1] + dx, p[2] + dy, &x0, &y0);
                cv_disc((x0 + 8) >> 4, (y0 + 8) >> 4, d, c, op == SO_CIRC);
                if (op == SO_CIRC && sc_weight > 1 && d > 3) cv_disc((x0 + 8) >> 4, (y0 + 8) >> 4, d - 2, c, 1);
                break;
            }
            case SO_ARC: {
                int32_t r = sc_clamp(p[3] + sc_mod(in, b, nb, SF_R, &hide), 0, 255);
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

/* ---- the checker ---- */
/* 1: scene s, at most `max` bytes, is safe to draw: version 1, a design box inside 240 x 132, every op known and whole inside
 * max (and SC_MAX), at most four BINDs before an op (as the interpreter counts them: STYLE and POP keep them), sources and
 * fields known, polylines of 2 .. 16 points, groups balanced and at most two deep, STYLE 1 or 2, an END */
static int scene_ok(const uint8_t *s, uint32_t max)
{
    uint32_t at = 4, nb = 0, depth = 0;
    if (!s || max > SC_MAX) max = s ? SC_MAX : 0u;
    if (max < 5u || s[0] != SC_VERSION || !s[1] || s[1] > 240u || !s[2] || s[2] > 132u || s[3])
        return 0;
    while (at < max) {
        const uint8_t *p = s + at;
        uint32_t op = p[0] & 31u, len;
        if (op == SO_END)                               /* (the END byte itself: 0, no color bits) */
            return p[0] == SO_END && depth == 0u && nb == 0u;
        if (op > SO_STYLE && op != SO_BIND)
            return 0;
        if ((op == SO_PLINE || op == SO_PGON) && at + 1u >= max)
            return 0;
        len = sc_len(p);
        if (at + 1u + len > max - 1u)                   /* (whole, and an END after it) */
            return 0;
        switch (op) {
        case SO_BIND: {
            uint32_t f = p[2];
            if (++nb > 4u || (p[1] & 0x7Fu) >= SS_N)    /* (a source, SS_CENTERED or not) */
                return 0;
            if (!(f <= SF_HIDE || (f >= SF_P0X && f < SF_P0X + 16u) || (f >= SF_P0Y && f < SF_P0Y + 16u)))
                return 0;
            break;
        }
        case SO_PLINE: case SO_PGON:
            if (p[1] < 2u || p[1] > 16u) return 0;
            nb = 0;
            break;
        case SO_STYLE:
            if (p[1] < 1u || p[1] > 2u) return 0;
            break;
        case SO_PUSH:
            if (++depth > 2u) return 0;
            nb = 0;
            break;
        case SO_POP:
            if (!depth--) return 0;
            break;
        default:
            nb = 0;
            break;
        }
        at += 1u + len;
    }
    return 0;                                           /* (no END inside max) */
}

#ifndef OC_SCENE_DRAW_ONLY                              /* (tests/ui_mockup.c draws its own scenes: no lookup) */
/* ---- where a built-in engine's scene is ---- */
static uint32_t ocs_crc32(const void *p, uint32_t n);   /* (oc_store.c) */
#if OC_PACK_XIP
static const uint8_t *pk_xip(uint32_t off);            /* (oc_pack.c: the pack's byte off as read in place) */
#endif
#if !OC_PACK_XIP
#include "oc_scenes.h"                                  /* (the host only: the arrays; the unit reads the pack's trailer) */
#endif
static uint32_t sc_rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t sc_rd32(const uint8_t *p) { return sc_rd16(p) | sc_rd16(p + 2) << 16; }
/* engine e's scene in trailer t (room: the bytes from t to the end of the pack area), its length in *len; 0 none or a bad
 * trailer */
static const uint8_t *scene_in_trailer(const uint8_t *t, uint32_t room, uint32_t e, uint32_t *len)
{
    uint32_t n, tl, i;
    if (room < OC_SCN_HEAD || sc_rd32(t) != OC_SCN_MAGIC || sc_rd16(t + 4) != 1u)
        return 0;
    n = sc_rd16(t + 6);
    tl = sc_rd32(t + 8);
    if (tl > room || tl < OC_SCN_HEAD + 6u * n || ocs_crc32(t + OC_SCN_HEAD, tl - OC_SCN_HEAD) != sc_rd32(t + 12))
        return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *en = t + OC_SCN_HEAD + 6u * i;
        uint32_t off = sc_rd16(en + 2), l = sc_rd16(en + 4);
        if (en[0] == e && off >= OC_SCN_HEAD + 6u * n && l && off + l <= tl) {
            *len = l;
            return t + off;
        }
    }
    return 0;
}
/* engine e's scene, drawable (scene_ok), its length in *len; 0: the plain picture (no pack, no trailer, no scene, a bad one) */
static const uint8_t *scene_of(uint32_t e, uint32_t *len)
{
    const uint8_t *s = 0;
    *len = 0;
    if (!OC_PACK_READY())
        return 0;
#if OC_PACK_XIP
    {   /* the trailer at the pack's length rounded up to 4, read in place (oc_pack.c pk_xip: FL_XIP, as oc_module.c reads a
         * module) */
        uint32_t at = (OC_PACK_LEN + 3u) & ~3u;
        s = at < OC_PACK_AREA ? scene_in_trailer(pk_xip(at), OC_PACK_AREA - at, e, len) : 0;
    }
#else
    for (uint32_t i = 0; i < NELEM(SCENES); i++)        /* (the host: oc_scenes.h's arrays) */
        if (SCENES[i].engine == e) { s = SCENES[i].scene; *len = SCENES[i].len; }
#endif
    if (s && !scene_ok(s, *len)) { s = 0; *len = 0; }
    return s;
}
#endif
#endif /* OC_SCENE_C */
