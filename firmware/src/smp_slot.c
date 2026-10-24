/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.0: the internal user sample slot (one, as Felucca's USR1..USR3 were three).
 *
 * Why it is here
 *   Felucca kept three user sample slots of 80 KiB each at 0xA0000..0xDBFFF (header 512 B, then up to 81,408 B of
 *   4-bit IMA ADPCM, about 7 s mono at 22,050 Hz). Melodee gave that flash to its bank projects and user presets, and
 *   Orange Cat made the projects' part the record store (oc_store.c). This keeps one slot of the same capacity: the
 *   other two stay reclaimed (the record store's sectors 0..17 and the preset banks at 0xC8000..0xDBFFF).
 *
 * Where it is
 *   22 sectors of the record store's first area, 0xB2000..0xC7FFF (the store's sectors 18..39). Each sector keeps its
 *   last 256 bytes erased, as every store in 0x93000..0xFBFFF does: the update loader erases any sector whose tail
 *   reads as an update record (ldr_core.c), so raw sample bytes there could be wiped by an update. That leaves 3,840
 *   bytes a sector, 84,480 in all: the claim mark (32), Felucca's header (512) and 83,936 bytes of room, more than the
 *   81,408 a Felucca slot held (the limit kept: SMP_SLOT_MAX). The record store keeps 34 sectors (its sectors 0..17 and
 *   40..55).
 *
 * The claim
 *   At every boot (smp_slot_boot) the record store is read over all 56 sectors. Every record whose current copy has a
 *   part in the slot's 22 sectors is saved again into the rest (copy on write: a power cut leaves the copy before in
 *   charge, the next boot tries again). Old copies left there are erased (a deleted record's old copy must not come back
 *   at the next boot's scan); sectors with no record header (blank, or the sample's) are not touched. Then the 22
 *   sectors are the slot's: the store never looks at them again until the next boot (oc_store.c ocs_resv). A boot with
 *   nothing there writes nothing. When the rest has no room for the moves, nothing is
 *   moved and the slot is not there (SMP_UNCLAIMED) until the store has room (a project or patterns deleted; the next
 *   boot tries again).
 *   (0.6.0 rev 1 erased the 22 sectors and wrote a claim mark at the first boot. That broke the rule that a boot with
 *   nothing to repair writes nothing, which the first boot tests check: rev 2 dropped it.)
 *
 * The layout (bytes into the slot's room, the tails skipped: smp_slot_addr)
 *   0 .. 31      the slot's tag: magic "OCSL", version 1, then 0xFF, CRC-32 of the 28 bytes before (written as a
 *                sample begins)
 *   32 .. 543    Felucca's header (smp_user_hdr_t: "FSMP", version 1, zones 1..16, name, data_len, the data's CRC-32,
 *                the zones), written LAST: it is the commit
 *   544 ..       the data, data_len bytes
 *
 * Writing (the web editor's SysEx: editor.c ED_SLOT_*; only while stopped, as every flash write)
 *   smp_slot_begin(len): the first sector erased and marked again, the rest erased as the data reaches them; the data
 *   in order (smp_slot_write); smp_slot_end(header): the header's length and CRC must be the data's, then it is
 *   programmed and read back. A write torn anywhere leaves no header: the slot reads empty (never half a sample). The
 *   sample there before is gone once a new one begins (there is room for one copy only).
 *
 * Kept from everything else
 *   The record store skips the 22 sectors once claimed (saves, deletes). Presets, settings, the template
 *   and backups have their own sectors (storage.c). Restoring a backup writes records and presets, never the slot.
 *   The update loader programs only the app area and erases a data sector only for an update record in its tail: the
 *   tails here stay erased. The old sample SysEx (ED_SMP_BEGIN .. ED_SMP_ERASE, which wrote at 0xA0000 over what is
 *   now the record store) stays retired.
 *
 * Reading: smp_slot_read (any offset into the data); smp_slot_info. Playing it in place through the plaintext XIP
 * window (fl_plain_window_init: flash above 0x93000 reads as plaintext) maps each offset with smp_slot_addr.
 *
 * Revisions:
 *   0.6.0: first version.
 *   0.6.0 rev 2: no mark at boot, only sectors holding record copies erased (see The claim); a boot with nothing
 *                there writes nothing.
 *   0.12.2: SMP_FEED is storage.c's OC_FEED (the host tests count the feeds of the long flash operations).
 */
#define SMP_SLOT_BASE 0xB2000u
#define SMP_SLOT_NSECT 22u
#define SMP_SLOT_S0 18u                          /* the record store's sector the slot starts at */
#define SMP_SLOT_SPAN 3840u                      /* each sector's bytes (its last 256 erased) */
#define SMP_SLOT_ROOM (SMP_SLOT_NSECT * SMP_SLOT_SPAN)
#define SMP_SLOT_MARK 32u
#define SMP_SLOT_HDR 512u
#define SMP_SLOT_DATA (SMP_SLOT_MARK + SMP_SLOT_HDR)
#define SMP_SLOT_MAX 81408u                      /* a Felucca slot's data (80 KiB less its 512-byte header) */
#define SMP_SLOT_MAGIC 0x4C53434Fu               /* "OCSL" */
#define SMP_FSMP_MAGIC 0x504D5346u               /* "FSMP" (Felucca eng_sample.c) */
_Static_assert(SMP_SLOT_DATA + SMP_SLOT_MAX <= SMP_SLOT_ROOM, "one Felucca slot's data fits");
_Static_assert(SMP_SLOT_BASE == 0xA0000u + SMP_SLOT_S0 * OCS_SECTOR &&
               SMP_SLOT_S0 + SMP_SLOT_NSECT == 40u, "the slot: the record store's sectors 18..39");
enum { SMP_UNCLAIMED, SMP_EMPTY, SMP_FULL };

typedef struct {                                 /* Felucca's header (eng_sample.c smp_user_hdr_t: 480 B, the data at
                                                  * +512): the zones as its SAMPLE engine read them */
    uint32_t magic;
    uint16_t version;
    uint8_t nz, rsv;
    char name[8];
    uint32_t data_len, crc, rsv2[2];
    uint8_t zones[16][28];                       /* (off, n, ls, le, rate, root16, pred, idx, lo, hi, looped) */
} smp_hdr_t;
_Static_assert(sizeof(smp_hdr_t) == 480u, "Felucca's header: 32 + 16 x 28 bytes");

static struct {
    uint8_t state;                               /* SMP_* */
    uint8_t writing;                             /* a write begun, not ended */
    uint32_t len, done, crc;                     /* the write's length, the bytes written, their CRC so far */
    smp_hdr_t hdr;                               /* the header (SMP_FULL) */
} smp;

/* byte off of the slot's room -> its flash offset (the tails skipped) */
static uint32_t smp_slot_addr(uint32_t off)
{
    return SMP_SLOT_BASE + off / SMP_SLOT_SPAN * OCS_SECTOR + off % SMP_SLOT_SPAN;
}
/* n bytes of the room from off, through the tails */
static int smp_room_rd(uint32_t off, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = SMP_SLOT_SPAN - off % SMP_SLOT_SPAN;
        if (k > n) k = n;
        if (OCS_RD(smp_slot_addr(off), d, k)) return -1;
        d += k; off += k; n -= k;
    }
    return 0;
}
static int smp_room_pg(uint32_t off, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t k = SMP_SLOT_SPAN - off % SMP_SLOT_SPAN;
        int rc;
        if (k > n) k = n;
        if ((rc = ocs_prog(smp_slot_addr(off), src, k)) != 0) return rc;
        src += k; off += k; n -= k;
    }
    return 0;
}
static void smp_mark_make(uint32_t m[8])
{
    memset(m, 0xFF, 32);
    m[0] = SMP_SLOT_MAGIC;
    m[1] = 1u;
    m[7] = ocs_crc32(m, 28);
}
static int smp_marked(void)                      /* (the slot's tag: written as a sample begins) */
{
    uint32_t m[8], want[8];
    smp_mark_make(want);
    return !OCS_RD(SMP_SLOT_BASE, m, sizeof m) && !memcmp(m, want, sizeof m);
}
static int smp_mark(void)                         /* the first sector erased, the mark programmed and read back */
{
    uint32_t m[8];
    int rc;
    if ((rc = OCS_ER(SMP_SLOT_BASE)) != 0) return rc;
    smp_mark_make(m);
    if ((rc = ocs_prog(SMP_SLOT_BASE, (const uint8_t *)m, sizeof m)) != 0) return rc;
    return smp_marked() ? 0 : -7;
}
#ifndef OC_FEED
#define OC_FEED() ((void)0)            /* (0.12.2: a host unit that includes this file without storage.c, ui_test.c: no feed to count; clang's analyzer found the undeclared name) */
#endif
#define SMP_FEED() OC_FEED()                             /* (the claim at boot: up to ~56 erases, the 8 s watchdog; 0.12.2: storage.c's OC_FEED, which the host tests can count) */
static uint64_t smp_resv_mask(void) { return ((1ull << SMP_SLOT_NSECT) - 1ull) << SMP_SLOT_S0; }

/* the header: Felucca's, its zones and length within the slot */
static int smp_hdr_ok(const smp_hdr_t *h)
{
    return h->magic == SMP_FSMP_MAGIC && h->version == 1u && h->nz >= 1u && h->nz <= 16u && h->data_len >= 1u &&
           h->data_len <= SMP_SLOT_MAX;
}
/* the data's CRC-32 as it is in flash */
static uint32_t smp_data_crc(uint32_t len)
{
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t off = 0; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (smp_room_rd(SMP_SLOT_DATA + off, buf, n)) return 0;
        crc = ocs_crc_add(crc, buf, n);
    }
    return ~crc;
}
/* the slot as the flash has it (a mark there): its header read and checked against its data */
static void smp_slot_scan(void)
{
    smp.writing = 0;
    smp.state = SMP_EMPTY;
    if (smp_room_rd(SMP_SLOT_MARK, &smp.hdr, sizeof smp.hdr) == 0 && smp_hdr_ok(&smp.hdr) &&
        smp_data_crc(smp.hdr.data_len) == smp.hdr.crc)
        smp.state = SMP_FULL;
    else
        memset(&smp.hdr, 0, sizeof smp.hdr);
}

/* a record's current copy, read as a save's source (the claim's moves: no RAM image of it) */
typedef struct { ocs_src_t src; ocs_rec_t rec; uint32_t at; } smp_mv_t;
static int smp_mv_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    smp_mv_t *m = (smp_mv_t *)s;
    if (ocs_at(&m->rec, m->at, dst, n)) return -3;
    m->at += n;
    return 0;
}
/* sector s holds a part of its record's current copy (by headers; the reservation aside) */
static int smp_live(uint32_t s)
{
    const ocs_hdr_t *h = &ocs_hdr[s];
    uint8_t sect[OCS_PARTS_MAX];
    uint32_t seq;
    return h->magic == OCS_MAGIC && ocs_current(h->type, h->id, sect, &seq, 0) && seq == h->seq;
}
/* the slot's sectors taken from the store: every record with a part there saved again elsewhere, then the sectors
 * erased and marked. 0 done; -1 no room (nothing moved past what fits, nothing erased); another negative: flash */
static int smp_claim(void)
{
    uint32_t s, k, room = 0, need = 0, most = 0;
    uint64_t mask = smp_resv_mask();
    for (s = 0; s < OCS_NSECT; s++)                          /* the room outside the slot */
        if (!((mask >> s) & 1u) && ocs_free(s)) room++;
    for (s = 0; s < OCS_NSECT; s++) {                        /* what must move: each record with a part inside, once
                                                              * (its first part there); all its parts are written again,
                                                              * the ones outside freed after its move */
        uint8_t sect[OCS_PARTS_MAX];
        uint32_t seq, parts, p, in = 0, first = 1;
        if (!smp_live(s)) continue;
        parts = ocs_current(ocs_hdr[s].type, ocs_hdr[s].id, sect, &seq, 0);
        for (p = 0; p < parts; p++) {
            if ((mask >> sect[p]) & 1u) in++;
            if (sect[p] < s) first = 0;
        }
        if (!in || !first) continue;
        need += in;
        if (parts - in > most) most = parts - in;
    }
    if (need + most > room) return -1;                       /* (room for every move, the one held longest too) */
    ocs_resv = mask;                                          /* (saves go elsewhere from here) */
    for (k = 0; k <= SMP_SLOT_NSECT; k++) {                   /* each record with its current copy there, moved */
        int rc, found = 0;
        for (s = SMP_SLOT_S0; s < SMP_SLOT_S0 + SMP_SLOT_NSECT && !found; s++)
            if (smp_live(s)) found = 1;
        if (!found) break;
        s--;
        SMP_FEED();
        {
            smp_mv_t mv = {{smp_mv_get}, {{0}, 0, 0}, 0};
            uint32_t type = ocs_hdr[s].type, id = ocs_hdr[s].id;
            if (ocs_open(type, id, &mv.rec)) {                /* (its payload rotted: not a copy anyone reads) */
                memset(&ocs_hdr[s], 0, OCS_HDR);
                continue;
            }
            rc = ocs_write_src(type, id, &mv.src, mv.rec.total);
            if (rc) { ocs_resv = 0; return rc; }
        }
    }
    for (s = SMP_SLOT_S0; s < SMP_SLOT_S0 + SMP_SLOT_NSECT; s++)     /* (every move made, or the slot not taken) */
        if (smp_live(s)) { ocs_resv = 0; return -1; }
    for (s = SMP_SLOT_S0; s < SMP_SLOT_S0 + SMP_SLOT_NSECT; s++) {   /* only old copies left there: erased (a deleted
                                                                      * record's old copy would come back as its
                                                                      * current one at the next boot); a sector with
                                                                      * no record header (blank, or the sample's) is
                                                                      * left alone */
        int rc;
        if (ocs_hdr[s].magic != OCS_MAGIC) continue;
        SMP_FEED();
        if ((rc = OCS_ER(ocs_addr(s))) != 0) { ocs_resv = 0; return rc; }
        memset(&ocs_hdr[s], 0, OCS_HDR);
    }
    return 0;
}

/* boot (project.c, in place of ocs_boot): the record store read over all 56 sectors, then the slot's 22 taken from it.
 * Nothing is written unless a record's current copy lies there (only a store 0.5.x filled that far): that record is
 * saved again in the rest first. A boot with nothing to move writes nothing (0.6.0 rev 2). */
static void smp_slot_boot(void)
{
    memset(&smp, 0, sizeof smp);
    if (smp_marked()) {                                       /* (a sample was begun here: written only once the slot
                                                               * was claimed) the 22 sectors are the slot's: not read
                                                               * as the store's (a sample's bytes can never pass for a
                                                               * record) */
        ocs_resv = smp_resv_mask();
        ocs_boot();
        smp_slot_scan();
        return;
    }
    ocs_resv = 0;
    ocs_boot();
    if (smp_claim() == 0) {
        smp_slot_scan();
    } else {                                                  /* (the store keeps every sector it had) */
        uint8_t cursor = ocs_cursor;
        ocs_resv = 0;
        ocs_boot();
        ocs_cursor = cursor;
        smp.state = SMP_UNCLAIMED;
    }
}

/* ------------------------------------------------------------------ the interface --- */
/* the slot now: SMP_UNCLAIMED, SMP_EMPTY or SMP_FULL; name (9 bytes) and len filled when FULL */
static uint32_t smp_slot_info(char *name, uint32_t *len, uint32_t *nz)
{
    if (name) { memset(name, 0, 9); if (smp.state == SMP_FULL) memcpy(name, smp.hdr.name, 8); }
    if (len) *len = smp.state == SMP_FULL ? smp.hdr.data_len : 0u;
    if (nz) *nz = smp.state == SMP_FULL ? smp.hdr.nz : 0u;
    return smp.state;
}
/* a new sample of len bytes begins: the one there is gone. 0 ok; -1 refused (no slot, a bad length); else flash */
static int smp_slot_begin(uint32_t len)
{
    int rc;
    if (smp.state == SMP_UNCLAIMED || len < 1u || len > SMP_SLOT_MAX) return -1;
    smp.state = SMP_EMPTY;
    memset(&smp.hdr, 0, sizeof smp.hdr);
    smp.writing = 0;
    if ((rc = smp_mark()) != 0) return rc;       /* (the header's sector: erased, no header) */
    smp.writing = 1;
    smp.len = len;
    smp.done = 0;
    smp.crc = 0xFFFFFFFFu;
    return 0;
}
/* the data in order: n bytes at off (off must be where the last write ended). 0 ok; -1 refused; else flash */
static int smp_slot_write(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    if (!smp.writing || off != smp.done || n > smp.len - smp.done) return -1;
    while (n) {
        uint32_t room = SMP_SLOT_DATA + off, k = SMP_SLOT_SPAN - room % SMP_SLOT_SPAN;
        int rc;
        if (k > n) k = n;
        if (room % SMP_SLOT_SPAN == 0u && room >= SMP_SLOT_SPAN &&   /* a sector's first byte: erased first */
            (rc = OCS_ER(SMP_SLOT_BASE + room / SMP_SLOT_SPAN * OCS_SECTOR)) != 0) { smp.writing = 0; return rc; }
        if ((rc = smp_room_pg(room, p, k)) != 0) { smp.writing = 0; return rc; }
        smp.crc = ocs_crc_add(smp.crc, p, k);
        p += k; off += k; n -= k;
        smp.done = off;
    }
    return 0;
}
/* the write ends: the header (Felucca's, 512 bytes) committed when it agrees with the data. 0 ok; -1 refused; else
 * flash (the slot empty) */
static int smp_slot_end(const void *hdr512)
{
    smp_hdr_t h;
    int rc;
    memcpy(&h, hdr512, sizeof h);
    if (!smp.writing || smp.done != smp.len || !smp_hdr_ok(&h) || h.data_len != smp.len || h.crc != ~smp.crc ||
        smp_data_crc(smp.len) != h.crc) {
        smp.writing = 0;
        return -1;
    }
    smp.writing = 0;
    if ((rc = smp_room_pg(SMP_SLOT_MARK, hdr512, SMP_SLOT_HDR)) != 0) return rc;
    smp_slot_scan();
    return smp.state == SMP_FULL ? 0 : -7;
}
/* the slot emptied (kept, marked). 0 ok */
static int smp_slot_erase(void)
{
    int rc;
    if (smp.state == SMP_UNCLAIMED) return -1;
    smp.writing = 0;
    for (uint32_t k = 1; k < SMP_SLOT_NSECT; k++) {
        SMP_FEED();
        if ((rc = OCS_ER(SMP_SLOT_BASE + k * OCS_SECTOR)) != 0) return rc;
    }
    if ((rc = smp_mark()) != 0) return rc;
    smp_slot_scan();
    return 0;
}
/* n bytes of the sample's data from off. 0 ok */
static int smp_slot_read(uint32_t off, void *dst, uint32_t n)
{
    if (smp.state != SMP_FULL || off > smp.hdr.data_len || n > smp.hdr.data_len - off) return -1;
    return smp_room_rd(SMP_SLOT_DATA + off, dst, n);
}
