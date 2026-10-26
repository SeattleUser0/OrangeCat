/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.13.8) Host test of the hardware test surface's "say state" tones (firmware/src/oc_hw_test.c, OC_HW_TEST builds): the plan
 * of a state and the ISR's tone code itself (hwt_tone), rendered block by block as the audio interrupt calls it. Checked: the marker and five
 * digits in order, each tone's length, its frequency from its zero crossings within 1 % of 400 + 50 d Hz (the decoder's spacing is 50 Hz),
 * silence between them, the output untouched once the plan ends, and the notice code of a few texts.
 *   hw_say_test [OUT.wav]      the WAV of one state, for tools/hw/capture.py say (checked by hand: numpy is not in CI) */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FS 44100u
#define HWT_TONES_ONLY
#include "../firmware/src/oc_hw_test.c"

static int fails;
static void check(const char *what, int ok)
{
    printf("%-78s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

#define MAXS (FS * 2u)
static int32_t buf[2u * MAXS];

static uint32_t render(uint32_t page, uint32_t engine, const char *msg)
{
    uint32_t n = hwt_plan(page, engine, hwt_code(msg), hwt_say.freq, hwt_say.len), f;
    hwt_say.n = n; hwt_say.i = 0; hwt_say.left = hwt_say.len[0]; hwt_say.phase = 0;
    for (f = 0; f < MAXS; f++) buf[2u * f] = buf[2u * f + 1u] = 12345;    /* "music": replaced while the plan plays */
    for (f = 0; f < MAXS; f += 16u) hwt_tone(buf + 2u * f, 16u);
    return n;
}

int main(int argc, char **argv)
{
    static const struct { uint32_t page, engine; const char *msg; } S[] = {{127u, 8u, "PACK MISSING"}, {0u, 14u, ""}, {37u, 19u, "STEP CLEARED"}, {63u, 0u, "MOTION FULL"}};
    for (uint32_t c = 0; c < sizeof S / sizeof S[0]; c++) {
        uint32_t n = render(S[c].page, S[c].engine, S[c].msg), at = 0, ok = n == 11u, code = hwt_code(S[c].msg);
        const uint32_t want[6] = {1760u, 400u + 50u * (S[c].page >> 6), 400u + 50u * (S[c].page & 63u), 400u + 50u * S[c].engine,
                                  400u + 50u * (code >> 6), 400u + 50u * (code & 63u)};
        for (uint32_t k = 0; k < n && ok; k++) {
            uint32_t len = hwt_say.len[k], zc = 0, nz = 0;
            for (uint32_t i = at + 1u; i < at + len; i++) {
                zc += (buf[2u * i - 2u] < 0) != (buf[2u * i] < 0);
                nz += buf[2u * i] != 0;
            }
            if (k % 2u == 0u) {                        /* a tone: the marker, then the digits */
                double f = zc * (double)FS / (2.0 * len), w = want[k / 2u];
                ok &= f > w * 0.99 - 2.0 && f < w * 1.01 + 2.0;
                if (!ok) printf("  state %u tone %u: %.1f Hz, want %.0f\n", (unsigned)c, (unsigned)k, f, w);
            } else {
                ok &= nz == 0u;                        /* the gap is silent */
            }
            at += len;
        }
        ok &= buf[2u * at] == 12345 && buf[2u * (MAXS - 1u)] == 12345;
        char what[96];
        snprintf(what, sizeof what, "say state: page %u, engine %u, \"%s\" (code %u): marker, five digits, gaps, then the music", (unsigned)S[c].page,
                 (unsigned)S[c].engine, S[c].msg, (unsigned)code);
        check(what, ok);
    }
    check("the notice code: 12 bits, different texts differ", hwt_code("PACK MISSING") < 4096u && hwt_code("PACK MISSING") != hwt_code("STEP CLEARED") &&
          hwt_code("") != hwt_code("MOTION FULL"));
    if (argc > 1) {                                    /* the WAV of state 0, 16-bit stereo */
        FILE *w = fopen(argv[1], "wb");
        uint32_t frames = FS, v;
        render(S[0].page, S[0].engine, S[0].msg);
        fwrite("RIFF", 1, 4, w); v = 36u + frames * 4u; fwrite(&v, 4, 1, w); fwrite("WAVEfmt ", 1, 8, w);
        v = 16; fwrite(&v, 4, 1, w); { uint16_t h[2] = {1, 2}; fwrite(h, 2, 2, w); } v = FS; fwrite(&v, 4, 1, w); v = FS * 4u; fwrite(&v, 4, 1, w);
        { uint16_t h[2] = {4, 16}; fwrite(h, 2, 2, w); } fwrite("data", 1, 4, w); v = frames * 4u; fwrite(&v, 4, 1, w);
        for (uint32_t i = 0; i < frames; i++) { int16_t s[2] = {(int16_t)(buf[2u * i] == 12345 ? 0 : buf[2u * i]), (int16_t)(buf[2u * i] == 12345 ? 0 : buf[2u * i])}; fwrite(s, 2, 2, w); }
        fclose(w);
    }
    if (fails) { printf("SAY STATE TEST FAILED (%d)\n", fails); return 1; }
    printf("say state: all ok\n");
    return 0;
}
