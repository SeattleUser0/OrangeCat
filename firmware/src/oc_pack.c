/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0: the asset pack at run time (oc_pack.h says what it is; tools/gen_pack.py makes it).
 *
 * Where it is
 *   The PACK area of storage.c's flash map: 10 sectors at 0x93000..0x9CFFF. Until 0.9.0 these were the Prophet banks' five A/B
 *   pairs (OBJ_P5BANK0..+4, banks 16..20 of the user preset library), and a pack could be installed only by taking those banks.
 *   Since 0.9.1 the library lives in the group of lib_group.c (23 sectors outside this area and the module area); the boot moves
 *   the old pairs there (once, before the pack check), and the area is the pack's alone. The installer asks nothing and erases
 *   no preset. The Prophet objects are retired by the build (storage.c ST_RETIRE_BY_BUILD: refused for writing, no record on
 *   reads), so nothing can write a pack sector through them. One condition: while the move of the old pairs has not finished
 *   (a flash error at boot, retried at the next) the installer refuses (-4), since an old pair may still be in this area.
 *
 * What is stored
 *   byte 0..511 the header, 512.. the tables (the layout in tools/gen_pack.py). Nothing else; the 256 bytes at the end of each
 *   sector are used, not left erased as the other stores do, so the tables can be read in place as arrays. The update loader erases a
 *   sector whose last 256 bytes look like one of its update records (ldr_core.c); gen_pack.py shifts the layout until none does, and
 *   the installer tests the same rule on what it was given (pk_tails) and refuses a pack that fails it.
 *
 * The check (oc_pack_boot, at boot after the flash is known, and after every install or erase)
 *   magic, version 1, header CRC-32, the table hash and length this firmware was built with (OC_PACK_HASH, OC_PACK_LEN), then the
 *   payload CRC-32 over the flash. Any part wrong: the state says what (PK_MISSING none there, PK_OTHER a pack for another
 *   build, PK_DAMAGED a pack of this build with bad bytes), oc_pack_good stays 0 and SHAPES and MACRO are silent. 40 KB through the
 *   flash reader at boot, a few milliseconds.
 *
 * Installing (the web editor's ED_PACK_*, or a .pak on the card in the file browser; only while stopped, as every flash write)
 *   oc_pack_begin(len, take): the length must be this build's (OC_PACK_LEN); the banks asked about; the area erased; the engines
 *   silent from here. oc_pack_write(off, data, n): the payload in order, from byte 512 (programmed in 256-byte pages). oc_pack_end(hdr):
 *   the header must agree with the data and with the build, the tails pass the loader's rule; then the header is programmed LAST
 *   (the commit), the area checked as at boot, the engines back (the driver's erase and program drop the XIP cache lines they touch, fm1_flash.h fl_inval). A write torn anywhere leaves no header: the
 *   pack reads as missing, never half installed. oc_pack_erase(): the 10 sectors erased.
 *
 * Revisions:
 * (Orange Cat 0.9.1) Library in groups with a shared spare sector (0.9.1)
 *   0.8.0: first version.
 *   0.9.1: the library is no longer in this area (lib_group.c): no question about banks, no preset erased, the Prophet objects retired
 *          by the build instead of by the pack's presence; oc_pack_begin(len), oc_pack_erase(), oc_pack_info(len, hash).
 */
enum { PK_MISSING, PK_OK, PK_OTHER, PK_DAMAGED };           /* oc_pack_state */
#define PK_SECTORS (OC_PACK_AREA / 0x1000u)

static struct {
    uint8_t state;                                           /* PK_* */
    uint8_t writing;                                         /* an install begun, not ended */
    uint32_t len, done, crc;                                 /* the install's length, bytes written (from OC_PACK_HDR), their CRC so far */
} pk;

static uint32_t pk_rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t pk_rd16(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8; }
static uint32_t pk_crc16(const uint8_t *p, uint32_t n)       /* the loader's ota_crc16: poly 0x1021, init 0 */
{
    uint32_t c = 0, i;
    while (n--) {
        c ^= (uint32_t)*p++ << 8;
        for (i = 0; i < 8u; i++) c = c & 0x8000u ? (c << 1) ^ 0x1021u : c << 1;
        c &= 0xFFFFu;
    }
    return c;
}
/* the loader's rule on the 80 bytes at a sector's end less 256: 1 when they would read as an update record */
static int pk_record_like(const uint8_t *r)
{
    uint32_t w = pk_rd16(r);
    return pk_rd16(r + 6) == 0x5441u && w && w == pk_crc16(r + 2, 78u);
}
/* sector s of the area: its tail passes the rule. 0 ok, 1 would be erased by an update, -1 the flash would not read */
static int pk_tail(uint32_t s)
{
    uint8_t r[80];
    if (st_read(OC_PACK_LO + s * 0x1000u + 0x1000u - 0x100u, r, sizeof r)) return -1;
    return pk_record_like(r);
}
static int pk_tails(void)                                    /* every sector's: 0 ok */
{
    for (uint32_t s = 0; s < PK_SECTORS; s++) { int r = pk_tail(s); if (r) return r; }
    return 0;
}
/* a header read from the area: the build's pack? 0 yes; else the state it makes */
static uint32_t pk_header_state(const uint8_t *h)
{
    if (pk_rd32(h) != OC_PACK_MAGIC) return PK_MISSING;
    if (ocs_crc32(h, OC_PACK_HDR - 4u) != pk_rd32(h + OC_PACK_HDR - 4u) || pk_rd16(h + 4) != OC_PACK_VERSION) return PK_DAMAGED;
    if (pk_rd32(h + 8) != OC_PACK_HASH || pk_rd32(h + 12) != OC_PACK_LEN) return PK_OTHER;
    return PK_OK;
}
/* the payload as the flash has it: its CRC-32 equals the header's */
static int pk_payload_ok(const uint8_t *h)
{
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu, len = pk_rd32(h + 12), off;
    if (len < OC_PACK_HDR || len > OC_PACK_AREA) return 0;
    for (off = OC_PACK_HDR; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (st_read(OC_PACK_LO + off, buf, n)) return 0;
        crc = ocs_crc_add(crc, buf, n);
        SMP_FEED();
    }
    return ~crc == pk_rd32(h + 16);
}
/* the state of the area now; oc_pack_good follows */
static void oc_pack_scan(void)
{
    uint8_t h[OC_PACK_HDR];
    pk.state = PK_MISSING;
    if (!st_read(OC_PACK_LO, h, sizeof h)) {
        pk.state = (uint8_t)pk_header_state(h);
        if (pk.state == PK_OK && !pk_payload_ok(h)) pk.state = PK_DAMAGED;
    }
    oc_pack_good = pk.state == PK_OK;
}
static void oc_pack_boot(void)                               /* persist_boot: after the flash is known and the library group is in place (lib_group.c) */
{
    memset(&pk, 0, sizeof pk);
    oc_pack_scan();
}
/* ------------------------------------------------------------------ the interface --- */
/* state (PK_*), length, hash of the pack there (0 when none) */
static uint32_t oc_pack_info(uint32_t *len, uint32_t *hash)
{
    uint8_t h[OC_PACK_HDR];
    if (len) *len = 0;
    if (hash) *hash = 0;
    if (!st_read(OC_PACK_LO, h, sizeof h) && pk_rd32(h) == OC_PACK_MAGIC) {
        if (len) *len = pk_rd32(h + 12);
        if (hash) *hash = pk_rd32(h + 8);
    }
    return pk.state;
}
static int pk_erase_all(void)
{
    for (uint32_t s = 0; s < PK_SECTORS; s++) {
        int rc;
        SMP_FEED();
        if ((rc = st_erase(OC_PACK_LO + s * 0x1000u)) != 0) return rc;
    }
    return 0;
}
/* an install of len bytes begins: 0 ok; -1 refused (not this build's length, one under way is replaced: no); -4 the move of the old
 * preset pairs into the group has not finished (an old pair may still be in the area); else flash. From here the pack is gone and
 * the engines silent. Presets are never in the way (0.9.1). */
static int oc_pack_begin(uint32_t len)
{
    int rc;
    if (len != OC_PACK_LEN || len > OC_PACK_AREA) return -1;
    if (lg_incomplete) return -4;
    pk.writing = 0;
    pk.state = PK_MISSING;
    oc_pack_good = 0;
    rc = pk_erase_all();
    if (rc) return rc;
    pk.writing = 1;
    pk.len = len;
    pk.done = OC_PACK_HDR;
    pk.crc = 0xFFFFFFFFu;
    return 0;
}
/* the payload in order: n bytes at off (where the last write ended; the first at OC_PACK_HDR). 0 ok; -1 refused; else flash */
static int oc_pack_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    if (!pk.writing || off != pk.done || n > pk.len - pk.done) return -1;
    while (n) {
        uint32_t k = 256u - (off & 0xFFu);
        int rc;
        if (k > n) k = n;
        if ((rc = st_prog(OC_PACK_LO + off, p, k)) != 0) { pk.writing = 0; return rc; }
        pk.crc = ocs_crc_add(pk.crc, p, k);
        p += k; off += k; n -= k;
        pk.done = off;
    }
    return 0;
}
/* the install ends: the 512-byte header, when it agrees with the data and the build, programmed last. 0 ok; -1 refused; else flash */
static int oc_pack_end(const void *hdr512)
{
    uint8_t h[OC_PACK_HDR];
    int rc;
    memcpy(h, hdr512, sizeof h);
    if (!pk.writing || pk.done != pk.len || pk_header_state(h) != PK_OK || pk_rd32(h + 12) != pk.len ||
        pk_rd32(h + 16) != ~pk.crc || !pk_payload_ok(h) || pk_tails() != 0) {
        pk.writing = 0;
        return -1;
    }
    pk.writing = 0;
    for (uint32_t off = 0; off < OC_PACK_HDR; off += 256u)    /* the commit, last */
        if ((rc = st_prog(OC_PACK_LO + off, h + off, 256u)) != 0) return rc;
    oc_pack_scan();
    return pk.state == PK_OK ? 0 : -7;
}
/* the area erased: no pack. 0 ok; -4 the move of the old preset pairs has not finished; else flash */
static int oc_pack_erase(void)
{
    int rc;
    if (lg_incomplete) return -4;
    pk.writing = 0;
    oc_pack_good = 0;
    rc = pk_erase_all();
    pk.state = PK_MISSING;
    return rc;
}
