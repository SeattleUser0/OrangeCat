/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 1b (docs/orangecat/UI_BUILD.md): with OC_HOME_PIC=1 the asset pack's install path takes the pack with HOME's
 * scene trailer after its tables (firmware/src/oc_pack.c, oc_pack.h OC_PACK_MAXLEN), on tests/persistence_test.c's simulated NOR
 * (tests/oc_pack_test.c is the same path with the switch off).
 *   cc -O1 -w -DOC_HOME_PIC=1 -Ibuild/gen -Ifirmware/src -o oc_pack_trailer_test tests/oc_pack_trailer_test.c -lm
 * Checked:
 *   BASE      the tables alone install as before (the header's length, hash and CRC the tables')
 *   TRAILER   the tables and a trailer after them install whole: every byte in flash, the pack good, pk_trailer_ok
 *   TABLES    a changed table byte with a trailer after it is refused (the header's payload CRC covers the tables)
 *   TRAILBYTE a changed trailer byte is installed (the trailer's own CRC is checked where it is read: pk_trailer_ok says no)
 *   TAILS     a trailer whose bytes make a sector's last 256 bytes look like an update record is refused
 *   LENGTH    a length under the tables' or over the area is refused at begin, erasing nothing; a header saying another length too
 *   IMAGE     pk_image_check over a buffer agrees: the trailer pack 0, a bad table byte 2, a short file 1
 * Revisions:
 *   (the new interface 0.1, branch ui-build) first version.
 */
#define OC_PACK_GATE 1
#define OC_PACK_LEN 9000u              /* (as tests/oc_pack_test.c: three sectors of a made pack) */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"

static int bad;
static void chk(const char *what, int ok)
{
    printf("pack_trailer: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint8_t PAK[OC_PACK_AREA];
static uint32_t pak_len;
/* the made pack (tests/oc_pack_test.c's), then (tl > 0) a trailer of tl bytes at the length rounded up to 4 */
static void make(uint32_t tl)
{
    uint32_t i, at = (OC_PACK_LEN + 3u) & ~3u;
    memset(PAK, 0xFF, sizeof PAK);
    for (i = OC_PACK_HDR; i < OC_PACK_LEN; i++) PAK[i] = (uint8_t)(i * 31u + (i >> 8) * 7u);
    wr32(PAK, OC_PACK_MAGIC);
    PAK[4] = OC_PACK_VERSION; PAK[5] = 0; PAK[6] = PAK[7] = 0;
    wr32(PAK + 8, OC_PACK_HASH);
    wr32(PAK + 12, OC_PACK_LEN);
    wr32(PAK + 16, ocs_crc32(PAK + OC_PACK_HDR, OC_PACK_LEN - OC_PACK_HDR));
    PAK[20] = PAK[21] = PAK[22] = PAK[23] = 0;
    wr32(PAK + 508, ocs_crc32(PAK, 508));
    pak_len = OC_PACK_LEN;
    if (tl) {
        uint8_t *t = PAK + at;
        wr32(t, 0x4353434Fu); t[4] = 1; t[5] = 0; t[6] = 1; t[7] = 0;   /* "OCSC", version 1, one entry */
        wr32(t + 8, tl);
        t[16] = 8; t[17] = 0; t[18] = 22; t[19] = 0; t[20] = (uint8_t)(tl - 22u); t[21] = (uint8_t)((tl - 22u) >> 8);
        for (i = 22; i < tl; i++) t[i] = (uint8_t)(i * 13u);
        wr32(t + 12, ocs_crc32(t + 16, tl - 16u));
        pak_len = at + tl;
    }
}
static int install(void)                              /* 0 the whole of PAK; else the call that failed (1 begin, 2 write, 3 end) */
{
    if (oc_pack_begin(pak_len)) return 1;
    for (uint32_t off = OC_PACK_HDR; off < pak_len; off += 256u)
        if (oc_pack_write(off, PAK + off, pak_len - off > 256u ? 256u : pak_len - off)) return 2;
    return oc_pack_end(PAK) ? 3 : 0;
}
static void fresh_flash(void)
{
    reset();
    memset(&pk, 0, sizeof pk);
    oc_pack_good = 0;
    st_retire_engines(7);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    fresh_flash();
    make(0);
    chk("BASE      the tables alone: installed, good, no trailer", install() == 0 && oc_pack_good && !pk_trailer_ok());
    fresh_flash();
    make(300);
    chk("TRAILER   the tables and a 300-byte trailer: installed whole, good, the trailer whole",
        install() == 0 && oc_pack_good && !memcmp(nor + OC_PACK_LO, PAK, pak_len) && pk_trailer_ok());
    fresh_flash();
    make(300);
    PAK[OC_PACK_HDR + 100] ^= 1u;
    chk("TABLES    a table byte changed (the header's CRC): refused at the end, no pack", install() == 3 && !oc_pack_good);
    fresh_flash();
    make(300);
    PAK[((OC_PACK_LEN + 3u) & ~3u) + 100] ^= 1u;
    chk("TRAILBYTE a trailer byte changed: the pack installs (its tables are whole); the trailer is refused where it is read",
        install() == 0 && oc_pack_good && !pk_trailer_ok());
    {   /* TAILS: the trailer runs into a sector's last 256 bytes, laid out there as an update record */
        uint32_t sec_end = 3u * 0x1000u, rec = sec_end - 256u;
        int rc;
        fresh_flash();
        make(sec_end - ((OC_PACK_LEN + 3u) & ~3u));
        memset(PAK + rec, 0, 256);
        PAK[rec + 6] = 0x41; PAK[rec + 7] = 0x54;           /* the loader's record: bytes 6 .. 7 0x5441, bytes 0 .. 1 the CRC-16 of 2 .. 79 */
        {
            uint32_t c = pk_crc16(PAK + rec + 2, 78);
            PAK[rec] = (uint8_t)c; PAK[rec + 1] = (uint8_t)(c >> 8);
        }
        wr32(PAK + ((OC_PACK_LEN + 3u) & ~3u) + 12, ocs_crc32(PAK + ((OC_PACK_LEN + 3u) & ~3u) + 16, sec_end - ((OC_PACK_LEN + 3u) & ~3u) - 16u));
        rc = install();
        chk("TAILS     a trailer making a sector tail read as an update record: refused at the end (the tail rule)", rc == 3 && !oc_pack_good);
    }
    {   /* LENGTH */
        uint32_t before;
        fresh_flash();
        make(0);
        before = erases;
        chk("LENGTH    begin refuses a length under the tables' or over the area, erasing nothing",
            oc_pack_begin(OC_PACK_LEN - 1u) == -1 && oc_pack_begin(OC_PACK_AREA + 1u) == -1 && oc_pack_begin(0) == -1 && erases == before);
        make(300);
        wr32(PAK + 12, pak_len);                        /* (a header that says the whole file: not this build's tables) */
        wr32(PAK + 508, ocs_crc32(PAK, 508));
        chk("LENGTH    a header whose length is not the tables' is refused", install() != 0 && !oc_pack_good);
    }
    {   /* IMAGE */
        make(300);
        int a = pk_image_check(oc_rd_buf, PAK, pak_len);
        PAK[OC_PACK_HDR + 5] ^= 1u;
        int b = pk_image_check(oc_rd_buf, PAK, pak_len);
        PAK[OC_PACK_HDR + 5] ^= 1u;
        int c = pk_image_check(oc_rd_buf, PAK, OC_PACK_LEN - 1u);
        chk("IMAGE     pk_image_check: the trailer pack 0, a bad table byte 2, a short file 1", a == 0 && b == 2 && c == 1);
    }
    printf("pack_trailer: %d failed\n", bad);
    return bad != 0;
}
