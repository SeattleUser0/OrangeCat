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
 *   (the new interface 0.1, branch ui-build, OC_HOME_PIC only; UI_BUILD.md 1b) an install, an image check and the card take a pack longer than the
 *          tables (up to OC_PACK_MAXLEN): HOME's scene trailer rides after them. The header, its length (still OC_PACK_LEN), hash and payload CRC
 *          cover the tables alone, as before; the trailer is checked by its own CRC where it is read (oc_scene.c). The sector tails are tested over all of it.
 *   0.13.4: the checks of an image (header, payload CRC, sector tails) take a reader, so the card checks the file it is about to write with the same code the
 *          installer and the boot check use (oc_rule_tails, pk_payload_crc, pk_image_check): nothing is erased for a file that would be refused at the end.
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
/* (0.13.4) the bytes of an image at off, for the checks below: 0 ok. The flash (st_read from a base), a buffer or a file on the card all give one. */
typedef int (*oc_rd_t)(void *ctx, uint32_t off, void *dst, uint32_t n);
static int oc_rd_flash(void *ctx, uint32_t off, void *dst, uint32_t n) { return st_read(*(const uint32_t *)ctx + off, dst, n); }
static int oc_rd_buf(void *ctx, uint32_t off, void *dst, uint32_t n) { memcpy(dst, (const uint8_t *)ctx + off, n); return 0; }
/* The update loader's rule on the sectors of an image laid from a sector boundary and `len` bytes long (erased flash, 0xFF, after its end), nsec sectors counted
 * from the first: 0 ok; else the sector, counted from 1, whose last 256 bytes would be taken for an update record; -1 when the image would not read. One copy
 * for the pack (all its sectors, from the flash after the install or from the file before it), a module (oc_mod_tails, the same) and the card's check. */
static int oc_rule_tails(oc_rd_t rd, void *ctx, uint32_t len, uint32_t nsec)
{
    uint8_t r[80];
    for (uint32_t s = 0; s < nsec; s++) {
        uint32_t at = s * 0x1000u + 0x1000u - 0x100u, k;
        memset(r, 0xFF, sizeof r);
        if (at < len) {
            k = len - at < sizeof r ? len - at : (uint32_t)sizeof r;
            if (rd(ctx, at, r, k)) return -1;
        }
        if (pk_record_like(r)) return (int)(s + 1u);
    }
    return 0;
}
static int pk_tails(void)                                    /* every sector's, as the flash has it: 0 ok */
{
    uint32_t base = OC_PACK_LO;
    return oc_rule_tails(oc_rd_flash, &base, OC_PACK_AREA, PK_SECTORS);
}
/* a header read from the area: the build's pack? 0 yes; else the state it makes */
static uint32_t pk_header_state(const uint8_t *h)
{
    if (pk_rd32(h) != OC_PACK_MAGIC) return PK_MISSING;
    if (ocs_crc32(h, OC_PACK_HDR - 4u) != pk_rd32(h + OC_PACK_HDR - 4u) || pk_rd16(h + 4) != OC_PACK_VERSION) return PK_DAMAGED;
    if (pk_rd32(h + 8) != OC_PACK_HASH || pk_rd32(h + 12) != OC_PACK_LEN) return PK_OTHER;
    return PK_OK;
}
/* the payload as the image has it (rd gives the pack's bytes from offset 0, as the area has them): its CRC-32 equals the header's */
static int pk_payload_crc(oc_rd_t rd, void *ctx, const uint8_t *h)
{
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu, len = pk_rd32(h + 12), off;
    if (len < OC_PACK_HDR || len > OC_PACK_AREA) return 0;
    for (off = OC_PACK_HDR; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (rd(ctx, off, buf, n)) return 0;
        crc = ocs_crc_add(crc, buf, n);
        SMP_FEED();
    }
    return ~crc == pk_rd32(h + 16);
}
static int pk_payload_ok(const uint8_t *h)                   /* ... as the flash has it */
{
    uint32_t base = OC_PACK_LO;
    return pk_payload_crc(oc_rd_flash, &base, h);
}
/* (0.13.4) a whole pack image, `size` bytes as rd gives them, checked as oc_pack_end and the boot check will: this build's header (magic, version, header CRC,
 * hash, length), the size, the payload CRC, the sector tails. 0 ok; 1 not this build's pack; 2 the payload does not match its CRC; 3 a sector tail would be
 * erased by an update; 4 the image would not read. Writes nothing. */
static int pk_image_check(oc_rd_t rd, void *ctx, uint32_t size)
{
    uint8_t h[OC_PACK_HDR];
    int t;
#if OC_HOME_PIC
    if (size < OC_PACK_LEN || size > OC_PACK_MAXLEN) return 1;
    if (rd(ctx, 0, h, sizeof h)) return 4;
    if (pk_header_state(h) != PK_OK || pk_rd32(h + 12) != OC_PACK_LEN) return 1;   /* (the tables' length; the trailer after) */
#else
    if (size != OC_PACK_LEN) return 1;
    if (rd(ctx, 0, h, sizeof h)) return 4;
    if (pk_header_state(h) != PK_OK || pk_rd32(h + 12) != size) return 1;
#endif
    if (!pk_payload_crc(rd, ctx, h)) return 2;
    t = oc_rule_tails(rd, ctx, size, PK_SECTORS);
    return t < 0 ? 4 : t > 0 ? 3 : 0;
}
/* the state of the area now; oc_pack_good follows */
static void oc_pack_scan(void)
{
    uint8_t h[OC_PACK_HDR];
#if OC_HOME_PIC
    oc_pack_gen++;                                           /* (the new interface: HOME's picture finds its scene again) */
#endif
    pk.state = PK_MISSING;
    if (!st_read(OC_PACK_LO, h, sizeof h)) {
        pk.state = (uint8_t)pk_header_state(h);
        if (pk.state == PK_OK && !pk_payload_ok(h)) pk.state = PK_DAMAGED;
    }
    /* (0.14.0) the copies first, then the flag: a MACRO note sequenced during a pack install renders only once its hot set is whole. A pack that is no
     * longer good is silent before its copies change (an unchanged good pack's copy rewrites equal bytes) */
    if (pk.state != PK_OK) oc_pack_good = 0;
    if (oc_pack_changed) oc_pack_changed();                  /* (0.13.7) the engines' hot sets copy their tables again (engines.c eng_hot_refresh) */
    oc_pack_good = pk.state == PK_OK;
}
static void oc_pack_boot(void)                               /* persist_boot: after the flash is known and the library group is in place (lib_group.c) */
{
    memset(&pk, 0, sizeof pk);
#if OC_PACK_BUILTIN
    oc_pack_good = 1;                                        /* (0.13.6) the tables are in the image: no pack, no check */
#else
    oc_pack_scan();
#endif
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
#if OC_HOME_PIC
    if (len < OC_PACK_LEN || len > OC_PACK_MAXLEN) return -1;   /* (the tables, and the scene trailer if it is there) */
#else
    if (len != OC_PACK_LEN || len > OC_PACK_AREA) return -1;
#endif
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
#if OC_HOME_PIC
        if (off < OC_PACK_LEN)                           /* (the header's payload CRC covers the tables only, not the trailer) */
            pk.crc = ocs_crc_add(pk.crc, p, off + k <= OC_PACK_LEN ? k : OC_PACK_LEN - off);
#else
        pk.crc = ocs_crc_add(pk.crc, p, k);
#endif
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
#if OC_HOME_PIC
    if (!pk.writing || pk.done != pk.len || pk_header_state(h) != PK_OK || pk_rd32(h + 12) != OC_PACK_LEN ||
#else
    if (!pk.writing || pk.done != pk.len || pk_header_state(h) != PK_OK || pk_rd32(h + 12) != pk.len ||
#endif
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
#if OC_HOME_PIC
/* (the new interface, UI_BUILD.md 1b) byte off of the pack as the firmware reads it in place (oc_scene.c: the scene trailer) */
static __attribute__((unused)) const uint8_t *pk_xip(uint32_t off) { return (const uint8_t *)FL_XIP(OC_PACK_LO + off); }
/* 1: the pack area holds a whole scene trailer after the tables (its magic, version and CRC), as the flash has it. A set's install
 * (card_install.c) puts the set's pack in again when its file carries a trailer and the area has none */
static int pk_trailer_ok(void)
{
    uint8_t b[256];
    uint32_t at = (OC_PACK_LEN + 3u) & ~3u, tl, crc = 0xFFFFFFFFu, off;
    if (at + 16u > OC_PACK_AREA || st_read(OC_PACK_LO + at, b, 16u) || pk_rd32(b) != 0x4353434Fu || pk_rd16(b + 4) != 1u) return 0;
    tl = pk_rd32(b + 8);
    if (tl < 16u || tl > OC_PACK_AREA - at) return 0;
    {
        uint32_t want = pk_rd32(b + 12);
        for (off = 16u; off < tl; off += sizeof b) {
            uint32_t n = tl - off > sizeof b ? sizeof b : tl - off;
            if (st_read(OC_PACK_LO + at + off, b, n)) return 0;
            crc = ocs_crc_add(crc, b, n);
        }
        return ~crc == want;
    }
}
#endif
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
