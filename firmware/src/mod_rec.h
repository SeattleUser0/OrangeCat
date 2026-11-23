/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (wave-engine, M3) A module engine's native block in a user preset: the record's extension (docs/orangecat/ADDING_AN_ENGINE.md 6a), the way DRUM-X's kit is
 * one (dxk_rec.h). Plain C over bytes, so upreset.c's host harness (UP_HOST, no engines) reads and writes it as the firmware does.
 *
 * Which engines: a module engine (not the Prophet, whose patch has a record version of its own, UP_VER_P5) whose page descriptor has a native block
 * (fw_pages_t.native). Today WAVE (engine 32). The block is the module's (its arena state: the owner's answer of November 22, docs/candidates/wave_engine.md 3):
 * the firmware never keeps it, only the copy the track's user preset holds (params.c mod_ext), which it gives back to the module when the module's state is
 * new (a load, an engine change, a release of the state: voice.c mod_ext_apply).
 *
 * The record (4 + n bytes, at most MXR_ROOM):
 *   0      MXR_MARK 'M'
 *   1      the version, MXR_VER (1). A reader refuses a version it does not know: the preset then plays its engine's factory preset 0's patch
 *   2      n, the body's length: the native block's first n bytes (the panel's entries 0 .. n - 1). A reader takes min(n, the module's npanel) bytes and
 *          clamps each to its panel entry's range (a record can come from a file); a module that grows its block raises n, an older record keeps the rest
 *          at the module's defaults
 *   3      CRC-8 (polynomial 0x07, initial 0) of the engine number, then the n body bytes (a record of another engine does not read)
 *   4..    the body
 * In a user preset record: its first 46 bytes in cz_extra, the rest in packed after the np values (upreset.c up_mxr_put, up_mxr_get), as DRUM-X's kit. */
#pragma once
#include <stdint.h>

#define MXR_MARK 0x4Du                          /* 'M' */
#define MXR_VER 1u
#define MXR_HEAD 4u
#define MXR_ROOM 63u                            /* a record's room at its least (dxk_rec.h DXK_ROOM: cz_extra 46 + packed's 17 after np 127) */
#define MXR_BODY_MAX (MXR_ROOM - MXR_HEAD)      /* 59 */

static inline uint8_t mxr_crc8(uint32_t engine, const uint8_t *b, uint32_t n)
{
    uint32_t c = 0, i, k;
    for (i = 0; i <= n; i++) {
        c ^= i ? b[i - 1u] : (uint8_t)engine;
        for (k = 0; k < 8u; k++) c = (c & 0x80u) ? ((c << 1) ^ 0x07u) & 0xFFu : (c << 1) & 0xFFu;
    }
    return (uint8_t)c;
}
/* body (n bytes, 1..MXR_BODY_MAX) of engine -> rec (4 + n bytes); returns 4 + n, 0 when n does not fit */
static inline uint32_t mxr_pack(uint8_t *rec, uint32_t engine, const uint8_t *body, uint32_t n)
{
    uint32_t i;
    if (!n || n > MXR_BODY_MAX) return 0;
    rec[0] = MXR_MARK; rec[1] = MXR_VER; rec[2] = (uint8_t)n; rec[3] = mxr_crc8(engine, body, n);
    for (i = 0; i < n; i++) rec[MXR_HEAD + i] = body[i];
    return MXR_HEAD + n;
}
/* rec (have bytes of it) of engine -> its body's length (the body at rec + MXR_HEAD), 0 when it does not read (no mark, another version, too short, a CRC
 * that does not match: another engine's, or damaged) */
static inline uint32_t mxr_unpack(const uint8_t *rec, uint32_t have, uint32_t engine)
{
    uint32_t n;
    if (have < MXR_HEAD + 1u || rec[0] != MXR_MARK || rec[1] != MXR_VER) return 0;
    n = rec[2];
    if (!n || n > MXR_BODY_MAX || MXR_HEAD + n > have || mxr_crc8(engine, rec + MXR_HEAD, n) != rec[3]) return 0;
    return n;
}
