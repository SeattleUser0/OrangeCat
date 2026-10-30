/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 1b (docs/orangecat/UI_BUILD.md): HOME's scene interpreter, its checker and the asset pack's scene
 * trailer (firmware/src/oc_scene.c, oc_scenes.h; tools/gen_pack.py --scenes), on the firmware's own gfx.c.
 *   cc -O1 -g -fsanitize=address,undefined -w -Ibuild/gen -Ifirmware/src -Itests -o oc_scene_test tests/oc_scene_test.c -lm
 *   ./oc_scene_test PACK_WITH_SCENES PACK_WITHOUT     (gen_pack.py --scenes firmware/src/oc_scenes.h, and without it)
 * Checked:
 *   ACCEPT    every scene there is (the three built-in engines', and the mockups' other 18 in tests/ui_scenes.h) passes scene_ok
 *             at its own length, and fails at every shorter one
 *   REFUSE    made faults: version, design box, an unknown op, five BINDs, a source or field out of range, a polyline of 0, 1
 *             or 17 points, a POP with no PUSH, three deep, a PUSH left open, STYLE 0 or 3, no END, an op cut at the end
 *   FUZZ      100,000 random changes of the three scenes, each in a block of its exact length: whatever scene_ok accepts is drawn
 *             (every source at random) without reading or writing out of bounds (the sanitizers: the scene's block, gfx.c's canvas) and in
 *             bounded time
 *   LOOKUP    scene_of by engine number; with -DOC_PACK_GATE=1 none while the pack is not there
 *   TRAILER   in the pack gen_pack.py made: the tables and header are byte for byte the pack without scenes; the trailer at the
 *             length rounded up to 4 gives each engine's scene, byte for byte oc_scenes.h's; a bad magic, a changed byte (the
 *             CRC), a trailer longer than its room and an engine not there give none
 * Revisions:
 *   (the new interface 0.1, branch ui-build) first version.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
static uint16_t host_screen[240 * 240];
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = 0; j < h && y + j < 240u; j++)
        for (uint32_t i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = c;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    for (uint32_t j = 0; j < h && y + j < 240u; j++)
        for (uint32_t i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}
#include "gfx.c"
#include "oc_pack.h"
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#include "oc_scene.c"
#include "ui_scenes.h"                                  /* (the mockups' other 18 scenes) */

static uint32_t crc_tab[256];
static uint32_t ocs_crc32(const void *v, uint32_t n)    /* (oc_store.c's: CRC-32, zlib's) */
{
    const uint8_t *p = v;
    uint32_t c = 0xFFFFFFFFu;
    if (!crc_tab[1])
        for (uint32_t i = 0; i < 256u; i++) {
            uint32_t r = i;
            for (int k = 0; k < 8; k++) r = r & 1u ? 0xEDB88320u ^ (r >> 1) : r >> 1;
            crc_tab[i] = r;
        }
    while (n--) c = crc_tab[(c ^ *p++) & 0xFFu] ^ (c >> 8);
    return ~c;
}

static int bad, nchk;
static void check(const char *what, int ok)
{
    nchk++;
    printf("oc_scene: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t rng = 12345u;
static uint32_t rnd(void) { rng = rng * 1664525u + 1013904223u; return rng >> 8; }
static void draw(const uint8_t *s)
{
    scene_in_t in;
    for (uint32_t i = 0; i < SS_N; i++) in.s[i] = (int16_t)((int32_t)(rnd() % 769u) - 256);
    for (uint32_t i = 0; i < 8u; i++) in.col[i] = (uint16_t)rnd();
    for (uint32_t i = 0; i < 64u; i++) in.scope[i] = (int16_t)((int32_t)(rnd() % 513u) - 256);
    CV_BANDS(0, 108, 240, 132, 0) {
        scene_draw(s, 0, 132 - s[2], &in);
    }
}
static uint8_t *load(const char *path, uint32_t *n)
{
    FILE *f = fopen(path, "rb");
    static uint8_t buf[2][65536];
    static int k;
    if (!f) return 0;
    *n = (uint32_t)fread(buf[k & 1], 1, sizeof buf[0], f);
    fclose(f);
    return buf[k++ & 1];
}
/* a copy of n bytes in a block of exactly n (the address sanitizer traps a read past its end) */
static uint8_t *exact(const uint8_t *src, uint32_t n)
{
    uint8_t *b = malloc(n ? n : 1u);
    memcpy(b, src, n);
    return b;
}
static int ok_exact(const uint8_t *src, uint32_t n)     /* scene_ok on such a copy */
{
    uint8_t *b = exact(src, n);
    int r = scene_ok(b, n);
    free(b);
    return r;
}
static void put16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, v); put16(p + 2, v >> 16); }
static uint32_t rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

int main(int argc, char **argv)
{
    static const struct { const uint8_t *s; uint32_t n; const char *name; } ALL[] = {
#define S(x) {x, sizeof x, #x}
        S(SCENE_MACRO), S(SCENE_SHAPES), S(SCENE_DRUMX), S(SCENE_WHEEL), S(SCENE_PROPHET), S(SCENE_SEM), S(SCENE_HADES),
        S(SCENE_FMBELL), S(SCENE_SUPERSAW), S(SCENE_DDDD), S(SCENE_CZ1), S(SCENE_SID), S(SCENE_DUB), S(SCENE_FX_SENDS),
        S(SCENE_FX_DIST), S(SCENE_FX_AUTOPAN), S(SCENE_FX_DELAY), S(SCENE_FX_REVERB), S(SCENE_FX_CHORUS), S(SCENE_FX_SLICER),
        S(SCENE_FX_MASTER),
#undef S
    };
    static uint8_t m[SC_MAX + 64];
    char line[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    palette_set(0);
    {   /* ACCEPT */
        int all = 1, cut = 1;
        for (uint32_t i = 0; i < NELEM(ALL); i++) {
            if (!scene_ok(ALL[i].s, ALL[i].n)) { printf("oc_scene:   %s refused\n", ALL[i].name); all = 0; }
            for (uint32_t n = 0; n < ALL[i].n; n++)
                if (ok_exact(ALL[i].s, n)) { printf("oc_scene:   %s accepted cut at %u\n", ALL[i].name, (unsigned)n); cut = 0; break; }
            draw(ALL[i].s);
        }
        snprintf(line, sizeof line, "ACCEPT  all %u scenes pass at their length, none at any shorter one, each draws", (unsigned)NELEM(ALL));
        check(line, all && cut);
    }
    {   /* REFUSE: each made fault on a small good scene */
        static const uint8_t GOOD[] = {SC_VERSION, 240, 132, 0, S_PUSH(10, 10), S_BIND(SS_K1, SF_ANG, 20), S_LINE(SCC_INK, 0, 0, 9, 9),
                                       S_POP, S_PLINE(SCC_K1, 2), 1, 2, 3, 4, S_STYLE(2), S_CIRC(SCC_K2, 50, 50, 9), S_END};
        struct { const char *what; int at; int val; int cut; } F[] = {
            {"version 2", 0, 2, 0}, {"box 241 wide", 1, 241, 0}, {"box 133 high", 2, 133, 0}, {"flags not 0", 3, 1, 0},
            {"an unknown op (15)", 4, 15, 0}, {"a source past SS_N", 8, SS_N, 0}, {"a centered source past SS_N", 8, 0x80 | SS_N, 0},
            {"a field 12 (none)", 9, 12, 0}, {"a field past SF_P0Y + 15", 9, SF_P0Y + 16, 0},
            {"a polyline of 1 point", 18, 1, 0}, {"STYLE 3", 24, 3, 0}, {"STYLE 0", 24, 0, 0},
            {"no END (cut before it)", -1, 0, 1}, {"an END with color bits (0x20)", 29, 0x20, 0}, {"an op cut at the end", -1, 0, 3},
        };
        int ok = scene_ok(GOOD, sizeof GOOD);
        for (uint32_t i = 0; i < NELEM(F); i++) {
            uint32_t n = sizeof GOOD - (uint32_t)F[i].cut;
            memcpy(m, GOOD, sizeof GOOD);
            if (F[i].at >= 0) m[F[i].at] = (uint8_t)F[i].val;
            if (ok_exact(m, n)) { printf("oc_scene:   accepted: %s\n", F[i].what); ok = 0; }
        }
        {   /* five BINDs before an op; a POP with no PUSH; three deep; a PUSH left open */
            static const uint8_t B5[] = {SC_VERSION, 240, 132, 0, S_BIND(0, 0, 1), S_BIND(0, 0, 1), S_BIND(0, 0, 1), S_BIND(0, 0, 1),
                                         S_BIND(0, 0, 1), S_LINE(0, 0, 0, 1, 1), S_END};
            static const uint8_t P0[] = {SC_VERSION, 240, 132, 0, S_POP, S_END};
            static const uint8_t P3[] = {SC_VERSION, 240, 132, 0, S_PUSH(1, 1), S_PUSH(1, 1), S_PUSH(1, 1), S_POP, S_POP, S_POP, S_END};
            static const uint8_t PO[] = {SC_VERSION, 240, 132, 0, S_PUSH(1, 1), S_LINE(0, 0, 0, 1, 1), S_END};
            static const uint8_t B4[] = {SC_VERSION, 240, 132, 0, S_BIND(0, 0, 1), S_BIND(0, 0, 1), S_BIND(0, 0, 1), S_BIND(0, 0, 1),
                                         S_LINE(0, 0, 0, 1, 1), S_END};
            static const uint8_t PP[] = {SC_VERSION, 240, 132, 0, S_PUSH(1, 1), S_PUSH(1, 1), S_POP, S_END};   /* (whole, one left open) */
            static const uint8_t L17[] = {SC_VERSION, 240, 132, 0, S_PLINE(0, 17), 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10,
                                          11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, S_END};   /* (whole: 17 points) */
            static uint8_t L16[sizeof L17 - 2u];
            memcpy(L16, L17, sizeof L16 - 1u); L16[5] = 16; L16[sizeof L16 - 1u] = SO_END;
            ok &= !ok_exact(B5, sizeof B5) && !ok_exact(P0, sizeof P0) && !ok_exact(P3, sizeof P3) && !ok_exact(PO, sizeof PO);
            ok &= !ok_exact(PP, sizeof PP) && !ok_exact(L17, sizeof L17) && ok_exact(L16, sizeof L16);
            ok &= ok_exact(B4, sizeof B4);
            ok &= !scene_ok(0, 100) && !scene_ok(GOOD, 4);
        }
        snprintf(line, sizeof line, "REFUSE  %u made faults refused (version, box, ops, sources, fields, points, groups, STYLE, END, cut)",
                 (unsigned)NELEM(F) + 6u);
        check(line, ok);
    }
    {   /* FUZZ */
        uint32_t taken = 0, i;
        clock_t t0 = clock();
        double worst = 0;
        for (i = 0; i < 100000u; i++) {
            const uint8_t *s = ALL[i % 3u].s;
            uint32_t n = ALL[i % 3u].n, k, nm = 1u + rnd() % 4u;
            memcpy(m, s, n);
            for (k = 0; k < nm; k++) m[4u + rnd() % (n - 4u)] = (uint8_t)rnd();
            if (rnd() % 8u == 0u) n = 4u + rnd() % (n - 4u);
            if (ok_exact(m, n)) {
                uint8_t *x = exact(m, n);
                clock_t a = clock();
                draw(x);
                free(x);
                double ms = (double)(clock() - a) * 1000.0 / CLOCKS_PER_SEC;
                if (ms > worst) worst = ms;
                taken++;
            }
        }
        snprintf(line, sizeof line, "FUZZ    100,000 changed scenes: %u accepted and drawn in bounds, the slowest %.1f ms (%.1f s in all)",
                 (unsigned)taken, worst, (double)(clock() - t0) / CLOCKS_PER_SEC);
        check(line, taken > 1000u && worst < 2000.0);   /* (a write past the canvas, gfx.c's cv_px, is the sanitizer's to trap) */
    }
    {   /* TRAILER */
        uint32_t na = 0, nb = 0;
        const uint8_t *a = argc > 2 ? load(argv[1], &na) : 0, *b = argc > 2 ? load(argv[2], &nb) : 0;
        int ok = a && b && nb >= 512u && na > nb && !memcmp(a, b, nb) && rd32(a + 12) == nb;
        if (ok) {
            uint32_t at = (nb + 3u) & ~3u, room = OC_PACK_AREA - at, len = 0;
            static uint8_t t[OC_PACK_AREA];
            const uint8_t *s;
            memset(t, 0xFF, sizeof t);
            memcpy(t, a + at, na - at);
            for (uint32_t i = 0; i < NELEM(SCENES); i++) {
                s = scene_in_trailer(t, room, SCENES[i].engine, &len);
                ok &= s && len == SCENES[i].len && !memcmp(s, SCENES[i].scene, len) && scene_ok(s, len);
            }
            ok &= !scene_in_trailer(t, room, 3u, &len);                      /* (an engine with no scene) */
            t[0] ^= 1u; ok &= !scene_in_trailer(t, room, 8u, &len); t[0] ^= 1u;   /* the magic */
            t[40] ^= 0x10u; ok &= !scene_in_trailer(t, room, 8u, &len); t[40] ^= 0x10u;   /* a byte: the CRC */
            {   /* in a block of exactly its room, and of one byte less than its length */
                uint8_t *x = exact(t, room);
                ok &= scene_in_trailer(x, room, 8u, &len) != 0;
                free(x);
                x = exact(t, rd32(t + 8) - 1u);
                ok &= !scene_in_trailer(x, rd32(t + 8) - 1u, 8u, &len);
                free(x);
            }
            {   /* the structure, each fault with its CRC made good again (the CRC is not what keeps the reader in bounds) */
                uint32_t tl = rd32(t + 8), n = t[6] | (uint32_t)t[7] << 8;
                static uint8_t u[OC_PACK_AREA];
                for (int f = 0; f < 4; f++) {
                    memcpy(u, t, sizeof u);
                    if (f == 0) put16(u + 16 + 2, 4);                         /* an entry's offset inside the head */
                    if (f == 1) put16(u + 16 + 4, tl);                        /* offset + length past the trailer */
                    if (f == 2) put16(u + 16 + 4, 0);                         /* a length of 0 */
                    if (f == 3) put32(u + 8, 16u + 6u * n - 1u);              /* the length shorter than the head */
                    put32(u + 12, ocs_crc32(u + 16, rd32(u + 8) - 16u));
                    uint8_t *x = exact(u, room);
                    if (scene_in_trailer(x, room, u[16], &len)) { printf("oc_scene:   trailer fault %d accepted\n", f); ok = 0; }
                    free(x);
                }
            }
            ok &= scene_in_trailer(t, room, 8u, &len) != 0;                   /* (and whole again) */
            memset(t, 0xFF, sizeof t);
            ok &= !scene_in_trailer(t, room, 8u, &len);                       /* erased flash: no trailer */
        }
        check("TRAILER the tables and header as without; each scene byte for byte; bad magic, CRC, room, engine, offsets, lengths: none", ok);
    }
    {   /* scene_of (the host: oc_scenes.h's arrays), and the pack's gate */
        uint32_t len = 0;
        int ok;
        oc_pack_good = 1;                               /* (the pack there: with the gate on it starts missing) */
        ok = scene_of(8u, &len) == SCENE_MACRO && len == sizeof SCENE_MACRO && scene_of(14u, &len) == SCENE_SHAPES &&
                 scene_of(9u, &len) == SCENE_DRUMX && !scene_of(3u, &len) && !len;
#if OC_PACK_GATE
        oc_pack_good = 0;
        ok &= !scene_of(8u, &len);                      /* (the pack not there: no picture) */
        oc_pack_good = 1;
        ok &= scene_of(8u, &len) == SCENE_MACRO;
#endif
        check(OC_PACK_GATE ? "LOOKUP  scene_of finds the three by engine number, none for another, none without the pack" : "LOOKUP  scene_of finds the three by engine number, none for another", ok);
    }
    printf("oc_scene: %d of %d checks failed\n", bad, nchk);
    return bad != 0;
}
