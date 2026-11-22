/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (wave-bank) A WAVE wavetable bank in the sample slot's room (the owner's answer of November 21: user wavetables live there, loaded from the installer page;
 * sampling shares the room later, one kind of content at a time). docs/candidates/wave_engine.md 2.
 *
 * The slot (smp_slot.c) is 16 sectors at 0xB2000, each sector's first 3,840 bytes in use (its last 256 stay erased: the update loader's tail rule), the room
 * addressed through them: a tag (32 bytes), a 512-byte header, the data. A bank is written as a sample is (ED_SLOT_BEGIN / WRITE / END / ERASE): the data, then
 * the header, committed only when the firmware has read the data back and its CRC matches (smp_slot_end). The header's first 32 bytes are the sample
 * header's (magic, version, a count, name, data length, CRC), so smp_slot_end's checks are the same; the magic says which kind it is.
 *
 * The data: frames of WVB_FLEN int16 samples (256: the top level, at most 128 harmonics), little endian, frame f at data byte f x 512, through the room's
 * mapping (a frame may cross a sector's tail: wvb_frame copies it in two pieces). Up to WVB_NTAB tables, each a run of frames.
 *
 * The header (512 bytes):
 *   0   u32 magic WVB_MAGIC ("WVBK")      4  u16 version 1      6  u8 ntab (1..16)      7  u8 0
 *   8   char name[8]                      16 u32 data_len (bytes: frames x 512)          20 u32 crc (CRC-32 of the data, as smp_slot.c's)
 *   24  u32 0, 0
 *   32  WVB_NTAB x 28 bytes: char name[8], u16 first frame, u8 frames (2..64), u8 0, then 16 bytes 0
 *   480 32 bytes 0
 * Read by the firmware (smp_slot.c, through its reader) and by the WAVE module (in place: the slot lies in the flash's plaintext XIP window). Not a
 * structure the interface identifier hashes: a wire layout, checked field by field. */
#pragma once
#include <stdint.h>
#define WVB_MAGIC 0x4B425657u                    /* "WVBK" little endian */
#define WVB_NTAB 16u
#define WVB_FLEN 256u
#define WVB_FBYTES (WVB_FLEN * 2u)
#define WVB_SLOT_BASE 0xB2000u                   /* smp_slot.c SMP_SLOT_BASE (it asserts the equality) */
#define WVB_SLOT_SPAN 3840u                      /* SMP_SLOT_SPAN */
#define WVB_SLOT_SECTOR 4096u
#define WVB_SLOT_NSECT 16u
#define WVB_HDR_OFF 32u                          /* SMP_SLOT_MARK: the header's room offset */
#define WVB_DATA_OFF 544u                        /* SMP_SLOT_DATA */
#define WVB_DATA_MAX 60896u                      /* SMP_SLOT_MAX */
#define WVB_MAX_FRAMES (WVB_DATA_MAX / WVB_FBYTES)   /* 118 */

/* a room offset -> the flash offset (the tails skipped) */
static inline uint32_t wvb_flash(uint32_t room) { return WVB_SLOT_BASE + room / WVB_SLOT_SPAN * WVB_SLOT_SECTOR + room % WVB_SLOT_SPAN; }
static inline uint32_t wvb_rd32(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24; }
/* the header's checks (the 512 bytes as read): 0 a bank the engine may play, else the first thing wrong */
static inline int wvb_hdr_check(const uint8_t *h)
{
    uint32_t n = h[6], len = wvb_rd32(h + 16), frames = len / WVB_FBYTES, i, j;
    if (wvb_rd32(h) != WVB_MAGIC) return 1;
    if ((h[4] | h[5] << 8) != 1u || !n || n > WVB_NTAB || h[7]) return 2;
    if (!len || len > WVB_DATA_MAX || len % WVB_FBYTES) return 3;
    for (i = 0; i < n; i++) {
        const uint8_t *t = h + 32 + 28 * i;
        uint32_t f0 = t[8] | t[9] << 8, nf = t[10];
        if (nf < 2u || nf > 64u || f0 + nf > frames || t[11]) return 4;
        for (j = 0; j < 8u; j++) if (t[j] && (t[j] < 0x20 || t[j] > 0x7E)) return 5;
    }
    return 0;
}
