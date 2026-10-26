/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner (Orange Cat 0.14.2: DRUM-X drum kits, after Skiff 2.0's drum lanes) */
/* A DRUM-X kit (eng_drumx.c: dxk[], one a track) and its record, the extension of a user preset of engine 9 (upreset.c up_kit_put,
 * up_kit_get). Plain C over bytes, so upreset.c's host harness (UP_HOST, no engines) reads and writes it as the firmware does.
 *
 * The kit: the factory kit whose sounds the lanes play (base: DX_KITS' row, each lane's patches A and B, read-only), then per lane
 * TUNE, DECAY, PAN, LAYER (its byte as Jiant's: LAYER low nibble, FDBK high), PUNCH, ATK+ and BEND, then the kit's TONE (BP low
 * nibble, FDBK high), HANDS and DUCK. Each lane's LEVEL is the track's (P_LN0..P_LN7), stored with the preset's values as before.
 * Every value at 0 (and LAYER at the factory kit's own) is the factory kit as it plays, sample for sample.
 *
 * The record (56 bytes; SPEC.md "Drum kits"):
 *   0      DXK_MARK 'X' (Skiff's mark for its project's DRUM-X kit)
 *   1      the version, DXK_VER (1). A reader refuses a version it does not know: the preset then plays the factory kit 0, as a
 *          preset of engine 9 always did before 0.14.2
 *   2      n, the body's length (52 in version 1). Room to grow: a later version 1 may add fields after the 52 and raise n; a
 *          reader takes the fields it knows and leaves the rest. n below 52 is refused
 *   3      CRC-8 (polynomial 0x07, initial 0) of the n body bytes
 *   4..    the body: base, tone, hands, duck; then eight bytes each (KICK .. BELL) of tune, decay, pan, layer, punch | atk << 4, bend
 * In a user preset record: its first 46 bytes in cz_extra (the bytes DRUM's patch takes for engine 10), the rest in packed after the
 * np values (upreset.c). DXK_ROOM: what a record has at its least (np 127, the largest P_COUNT the record allows). */
#pragma once
#include <stdint.h>

#define DXK_NLANE 8u
typedef struct {
    uint8_t on;                  /* 0: the factory kit t->preset as it is (the values below not read); 1: these */
    uint8_t base;                /* the factory kit (0..8) whose sounds the lanes play */
    uint8_t tone;                /* TONE: low nibble BP (a band-pass on the hats, the rim, the bell), high nibble FDBK (every lane's
                                  * oscillator fed back), Jiant's EDIT > TONE */
    uint8_t hands;               /* CLAP's HANDS 0..15 (Jiant's KIT 3 > HANDS) */
    uint8_t duck;                /* DUCK 0..100: the kick takes the parts that are not drum tracks down (Jiant's G_DUCK) */
    int8_t tune[DXK_NLANE];      /* -64..63, 3/16 semitone a step (+-12 semitones, as the kit's TUNE and Skiff's lane TUNE) */
    int8_t decay[DXK_NLANE];     /* -64..63 on the lane's DECAY (as the kit's DECY) */
    int8_t pan[DXK_NLANE];       /* -64..63 (the track's PAN's range and law), 0 the centre */
    int8_t bend[DXK_NLANE];      /* -12..12 semitones: each hit glides there over a step of the track (Jiant's step BEND) */
    uint8_t lay[DXK_NLANE];      /* LAYER 0..15 low nibble, its FDBK 0..15 high nibble (Jiant's EDIT > LAYER) */
    uint8_t punch[DXK_NLANE];    /* 0..15: Jiant's PUNCH (0..100) a lane's, 15 its 100 */
    uint8_t atk[DXK_NLANE];      /* ATK+ 0..15: the hit faded in over atk x 6 blocks (8: Jiant's ATK+, ~35 ms) */
} dxk_t;

#define DXK_MARK 0x58u
#define DXK_VER 1u
#define DXK_HEAD 4u
#define DXK_BODY 52u
#define DXK_BYTES (DXK_HEAD + DXK_BODY)
#define DXK_ROOM 63u
#define DXK_NKIT 9u                                   /* the factory kits (drumx_voice.c DX_KITS) */
#define DXK_DUCK_MAX 100
#define DXK_BEND_MAX 12

static uint8_t dxk_crc(const uint8_t *b, uint32_t n)
{
    uint32_t c = 0, i, k;
    for (i = 0; i < n; i++) {
        c ^= b[i];
        for (k = 0; k < 8u; k++)
            c = c & 0x80u ? ((c << 1) ^ 0x07u) & 255u : (c << 1) & 255u;
    }
    return (uint8_t)c;
}

/* kit k -> its record b (DXK_BYTES) */
static void dxk_pack(const dxk_t *k, uint8_t *b)
{
    uint8_t *d = b + DXK_HEAD;
    uint32_t l;
    d[0] = k->base;
    d[1] = k->tone;
    d[2] = k->hands;
    d[3] = k->duck;
    for (l = 0; l < DXK_NLANE; l++) {
        d[4u + l] = (uint8_t)k->tune[l];
        d[12u + l] = (uint8_t)k->decay[l];
        d[20u + l] = (uint8_t)k->pan[l];
        d[28u + l] = k->lay[l];
        d[36u + l] = (uint8_t)((k->punch[l] & 15u) | (k->atk[l] & 15u) << 4);
        d[44u + l] = (uint8_t)k->bend[l];
    }
    b[0] = DXK_MARK;
    b[1] = DXK_VER;
    b[2] = DXK_BODY;
    b[3] = dxk_crc(d, DXK_BODY);
}

static int dxk_s7(uint8_t v, int lim) { int x = (int8_t)v; return x >= -lim - (lim == 63) && x <= lim; }   /* (-64..63, or -lim..lim) */

/* record b (room bytes of it there) -> *k, on 1; 0 when it holds no kit this firmware reads (no mark: a preset of before 0.14.2;
 * another version, a short body, a bad CRC, a value out of range): *k as it was */
static int dxk_unpack(dxk_t *k, const uint8_t *b, uint32_t room)
{
    const uint8_t *d = b + DXK_HEAD;
    dxk_t x;
    uint32_t l, n;
    if (room < DXK_BYTES || b[0] != DXK_MARK || b[1] != DXK_VER)
        return 0;
    n = b[2];
    if (n < DXK_BODY || DXK_HEAD + n > room || dxk_crc(d, n) != b[3])
        return 0;
    if (d[0] >= DXK_NKIT || d[2] > 15u || d[3] > DXK_DUCK_MAX)
        return 0;
    for (l = 0; l < DXK_NLANE; l++)
        if (!dxk_s7(d[4u + l], 63) || !dxk_s7(d[12u + l], 63) || !dxk_s7(d[20u + l], 63) || !dxk_s7(d[44u + l], DXK_BEND_MAX))
            return 0;
    x.on = 1;
    x.base = d[0];
    x.tone = d[1];
    x.hands = d[2];
    x.duck = d[3];
    for (l = 0; l < DXK_NLANE; l++) {
        x.tune[l] = (int8_t)d[4u + l];
        x.decay[l] = (int8_t)d[12u + l];
        x.pan[l] = (int8_t)d[20u + l];
        x.lay[l] = d[28u + l];
        x.punch[l] = d[36u + l] & 15u;
        x.atk[l] = d[36u + l] >> 4;
        x.bend[l] = (int8_t)d[44u + l];
    }
    *k = x;
    return 1;
}
