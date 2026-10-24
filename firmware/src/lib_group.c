/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.9.1: the user preset library in a group with one shared spare sector. Included at the end of storage.c.
 *
 * Why
 *   The 22 library banks (upreset.c UP_BANKS) each had an A/B pair of sectors: 44 sectors in all, and the pairs sat in the
 *   PACK area (the Prophet banks, 0x93000) and the MODULE area (CZ-1 and native FM6 banks, 0xC8000), so a pack or a module
 *   could only be installed by erasing user presets. A pair is a spare for each bank, 22 spares for 22 banks. One spare
 *   shared by the whole group does the same job: 23 sectors hold the 22 banks and the one that is free.
 *
 * The layout (the sector table lg_addr, 23 sectors, all outside the two areas)
 *   0x9D000 0x9E000                      the old shared-extension pair (bank 21's)
 *   0x9F000 0xFE000                      the old FM6 patch bank pair (bank 6's)
 *   0xD4000..0xD7000 (4)                 the old CZ banks 6 and 7 pairs (banks 14, 15)
 *   0xD8000..0xDB000 (4)                 the old native FM6 pairs (banks 4, 5)
 *   0xDC000..0xDF000 (4)                 the old user preset pairs (banks 0, 1)
 *   0xE5000..0xE8000 (4)                 the old extension pairs (banks 2, 3)
 *   0xE9000 0xFA000                      the old FM6 extension pair (bank 7's)
 *   0xFB000                              free before the first migration: the first spare
 *   0xFE000                              (listed above with 0x9F000)
 * Which sector holds which bank is not fixed. A sector says what it is in its own commit record (st_hdr_t): type LG_TYPE
 * (0x100, above every object number, so firmware without groups never takes it for an object), slot = the bank number
 * 0..21, seq = that bank's save counter. The payload is the bank image as before: at byte 32 (compact tag) or 256, the last
 * 256 bytes of the sector stay erased (the update loader scans 0x93000..0xFC000 for records and must not meet data).
 *
 * Every image of 3808 bytes or less (all real banks) has its payload at byte 32 (compact tag), so the last 256 bytes stay erased;
 * a longer one (never made by the firmware) keeps the payload at 256 as the old objects did.
 * Save: choose a free sector (one no bank owns), erase it, program the image, program the header last, read it back, then
 * the sector the bank left is free. It is not erased then: the next save that picks it erases it. The free sector rotates
 * (lg_cursor goes round the table), so wear is spread over all 23 sectors.
 * Boot (lg_scan): read the 23 headers; for each bank the newest valid sequence wins (wrap-aware, body CRC checked), and the
 * RAM table lg_map / lg_seq is built (22 + 88 bytes). A save cut anywhere leaves the old copy in charge: the new sector has
 * no valid header until its last program completes. Two valid copies after a cut (header written, old one not yet reused):
 * the newer sequence wins, the older sits in a free sector and is erased LAZILY, when a later save picks that sector.
 *
 * Migration (lg_migrate), once, from the old pairs. A bank with a valid old pair and no group copy (or an older one) is
 * copied: free sector, payload, header last, read back and verify; only then are the old pair's two sectors erased. In-group
 * pairs go first (banks 0..7, 14, 15, 21), so each erase hands a sector to the next copy; the only free sector at the start
 * is 0xFB000. The sectors of a pair that is still waiting are protected (lg_busy) from being chosen. After a cut anywhere:
 * the old pair is still whole (copy not committed), or both exist and the group copy is newer or equal (the leftover is
 * erased at the next boot, lg_leftovers). The pass is safe to repeat and does nothing once it has nothing to do. It runs
 * before the pack check and before anything else touches the retired areas.
 *
 * Downgrade: firmware without groups does not know these sectors. The banks migrated are not found by it (it sees empty
 * banks); take a full backup (the editor backup) before installing 0.9.1.
 *
 * Downgrade below 0.9.1 and upgrade again (0.11.1): a downgrade DESTROYS the library. The old firmware's st_save sees the group's
 * sectors as empty, so it erases some of them (the copy of a pair it writes) and writes fresh pair records there, with a sequence
 * that restarts at 1. What it did not overwrite is still in the group, what it did is gone. Take a full backup (the editor backup)
 * BEFORE going below 0.9.1 and restore it AFTER coming back. What the migration does with what is left (0.11.1):
 *   - A sector is busy (protected from lg_pick) only if it holds the CURRENT old record of a bank still waiting to move. Before
 *     0.11.1 both sectors of every waiting pair were busy whatever they held; after a downgrade that was 22 sectors, and a group
 *     copy surviving in the 23rd left no free sector at all.
 *   - A bank that needs moving (its pair record is newer than its group copy by the sequence rule) and still owns a group copy has
 *     a STALE copy. When lg_pick finds no free sector, those stale copies are given up (the bank being moved first, then the
 *     others waiting): the pair's record wins because it is the newer save, and the sector is erased by the pick that takes it.
 *   - The count: a waiting bank has no group copy (it was given up) and at most one protected sector; a bank that is not waiting
 *     has at most one copy; so at most 22 of the 23 sectors can be taken and one is always free. From flash contents alone the
 *     picker cannot fail. If it ever does (a layout change, a flash that behaves differently) lg_needwipe is set: LIBRARY NEEDS WIPE
 *     (ui_boot.c), and the wipe has a way through (lg_drop_pairs, called by shared_wipe when the move is incomplete).
 *   - The sequence rule cannot tell a pair written by the old firmware (sequence from 1) from a leftover of the first migration. A
 *     bank whose group copy survived with a sequence at or above the pair's keeps the group copy, and the pair is erased as a leftover.
 *
 * Revisions:
 *   0.9.1: first version.
 *   0.11.1: a save whose read-back fails after the header was programmed erases the sector (or, if that fails too, moves lg_seq past it):
 *           the failed save no longer appears after a reset and no later save shares its sequence; lg_seq is 0 for a bank with no copy and
 *           every save takes lg_seq + 1.
 *   0.11.1: after a downgrade below 0.9.1 and an upgrade the migration could never finish (22 busy sectors, no free one: -8 on every
 *           boot, writes locked for good). A sector is busy only for the current record of a waiting bank; stale group copies of
 *           waiting banks are given up when there is no free sector (lg_pick_move); the state that remains impossible by count is
 *           reported as LIBRARY NEEDS WIPE (lg_needwipe) and shared_wipe clears the old records first (lg_drop_pairs).
 *   0.12.2: every erase of the group feeds the watchdog first (OC_FEED, storage.c): the migration of 22 banks, the leftovers, a save, the wipe.
 */
#define LG_BANKS 22u
#define LG_SECTORS 23u
#define LG_NONE 0xFFu
#define LG_TYPE 0x0100u                        /* the group's object type in a sector header (>= OBJ_COUNT of old firmware) */

static const uint32_t lg_addr[LG_SECTORS] = {
    0x9D000u, 0x9E000u, 0x9F000u, 0xD4000u, 0xD5000u, 0xD6000u, 0xD7000u, 0xD8000u, 0xD9000u, 0xDA000u, 0xDB000u, 0xDC000u,
    0xDD000u, 0xDE000u, 0xDF000u, 0xE5000u, 0xE6000u, 0xE7000u, 0xE8000u, 0xE9000u, 0xFA000u, 0xFB000u, 0xFE000u};

static uint8_t lg_map[LG_BANKS];               /* bank -> sector index, LG_NONE when it has no copy */
static uint32_t lg_seq[LG_BANKS];              /* bank -> the sequence of that copy */
static uint8_t lg_ready, lg_cursor;
static uint8_t lg_incomplete;                  /* the last lg_boot could not finish the move of the old pairs (retried at the next boot) */
static uint8_t lg_needwipe;                    /* (0.11.1) ... and it found no free sector even after giving up the stale copies: LIBRARY NEEDS WIPE */
#ifdef LG_TEST_HOOKS
static uint32_t lg_test_reserve;               /* host tests only: sectors lg_pick must not return (a stand-in for a state the data cannot make) */
#endif
static uint32_t lg_busy;                       /* sectors (bit per index) holding an old pair that is not migrated yet */
static uint32_t lg_erases, lg_progs;           /* flash operations done by this layer (migration cost, tests) */
static void (*lg_progress)(uint32_t done, uint32_t total);

static int lg_erase(uint32_t s) { lg_erases++; OC_FEED(); return st_erase(lg_addr[s]); }   /* (0.12.2) fed before every erase: the migration, a save, the wipe */
static int lg_prog(uint32_t a, const void *p, uint32_t n) { lg_progs++; return st_prog(a, p, n); }
static int lg_newer(uint32_t a, uint32_t b) { return a != b && a - b < 0x80000000u; }
static uint32_t lg_off(const st_hdr_t *h) { return h->rsv[0] == ST_COMPACT_TAG ? (uint32_t)sizeof(st_hdr_t) : ST_PAYLOAD_OFF; }

static uint32_t lg_index_of(uint32_t addr)
{
    for (uint32_t s = 0; s < LG_SECTORS; s++) if (lg_addr[s] == addr) return s;
    return LG_NONE;
}
static uint32_t lg_owned(void)
{
    uint32_t m = 0;
    for (uint32_t b = 0; b < LG_BANKS; b++) if (lg_map[b] != LG_NONE) m |= 1u << lg_map[b];
    return m;
}

static int lg_rd_hdr(uint32_t s, st_hdr_t *h)  /* a valid commit record in sector s: 0 */
{
    if (s >= LG_SECTORS || st_read(lg_addr[s], h, sizeof *h)) return -1;
    if (h->magic != ST_MAGIC || h->type != LG_TYPE || h->slot >= LG_BANKS || h->len > ST_PAYLOAD_MAX ||
        h->hcrc != st_crc32(h, sizeof *h - 4u) ||
        (h->rsv[0] == ST_COMPACT_TAG && h->len > ST_SECTOR - 256u - sizeof(st_hdr_t)))
        return -1;
    return 0;
}
static int lg_rd_body(uint32_t s, const st_hdr_t *h)   /* the payload's CRC-32 matches: 0 */
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t off = 0; off < h->len; off += sizeof st_buf) {
        uint32_t n = h->len - off > sizeof st_buf ? (uint32_t)sizeof st_buf : h->len - off;
        if (st_read(lg_addr[s] + lg_off(h) + off, st_buf, n)) return -1;
        for (uint32_t i = 0; i < n; i++) {
            crc ^= st_buf[i];
            for (uint32_t j = 0; j < 8u; j++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc == h->crc ? 0 : -1;
}

/* Build the RAM table from the 23 sectors: the newest valid copy of each bank. (A bank with no copy has lg_seq 0.) */
static void lg_scan(void)
{
    for (uint32_t b = 0; b < LG_BANKS; b++) { lg_map[b] = LG_NONE; lg_seq[b] = 0; }
    for (uint32_t s = 0; s < LG_SECTORS; s++) {
        st_hdr_t h;
        if (lg_rd_hdr(s, &h)) continue;
        if (lg_map[h.slot] != LG_NONE && !lg_newer(h.seq, lg_seq[h.slot])) continue;   /* an older (or equal) copy: skip */
        if (lg_rd_body(s, &h)) continue;
        lg_map[h.slot] = (uint8_t)s;
        lg_seq[h.slot] = h.seq;
    }
    lg_ready = 1;
}
static void lg_ensure(void) { if (!lg_ready) lg_scan(); }

/* A free sector: none owns it, no waiting old pair is in it. Round the table from the cursor (wear). */
static uint32_t lg_pick(void)
{
    uint32_t avoid = lg_owned() | lg_busy;
#ifdef LG_TEST_HOOKS
    avoid |= lg_test_reserve;
#endif
    for (uint32_t i = 0; i < LG_SECTORS; i++) {
        uint32_t s = (lg_cursor + i) % LG_SECTORS;
        if (!((avoid >> s) & 1u)) return s;
    }
    return LG_NONE;
}

/* (0.11.1) A free sector for the move of bank b. With none, the group copies of the banks still waiting to move are STALE (their
 * pair is newer: that is why they wait) and are given up one by one, b's own first; the sector is erased by whoever takes it. */
static uint32_t lg_pick_move(uint32_t need, uint32_t b)
{
    uint32_t s = lg_pick();
    for (uint32_t k = 0; s == LG_NONE && k <= LG_BANKS; k++) {
        uint32_t x = k == 0 ? b : k - 1u;
        if (k && x == b) continue;
        if (((need >> x) & 1u) && lg_map[x] != LG_NONE) {
            lg_map[x] = LG_NONE;
            lg_seq[x] = 0;
            s = lg_pick();
        }
    }
    return s;
}

static uint32_t lg_sector(uint32_t bank)
{
    lg_ensure();
    return lg_addr[bank < LG_BANKS && lg_map[bank] != LG_NONE ? lg_map[bank] : 0];
}
static int lg_current(uint32_t bank, st_hdr_t *h)   /* the bank's committed copy, body verified: 0 (there is one copy index: 0) */
{
    lg_ensure();
    if (bank >= LG_BANKS || lg_map[bank] == LG_NONE) return -1;
    if (lg_rd_hdr(lg_map[bank], h) || h->slot != bank || lg_rd_body(lg_map[bank], h)) return -1;
    return 0;
}
static int lg_head(uint32_t bank, st_hdr_t *h)      /* header only */
{
    lg_ensure();
    if (bank >= LG_BANKS || lg_map[bank] == LG_NONE || lg_rd_hdr(lg_map[bank], h) || h->slot != bank) return -1;
    return 0;
}
static int lg_read_range(uint32_t bank, uint32_t off, void *dst, uint32_t n)
{
    st_hdr_t h;
    if (lg_head(bank, &h)) return -1;
    return st_read(lg_addr[lg_map[bank]] + lg_off(&h) + off, dst, n);
}

static int lg_save(uint32_t bank, const void *src, uint32_t len)
{
    uint32_t s, off, payload = ST_PAYLOAD_OFF, compact = 0, old;
    int rc;
    st_hdr_t h, chk;
    if (bank >= LG_BANKS || len > ST_PAYLOAD_MAX) return -1;
    lg_ensure();
    compact = len <= ST_SECTOR - 256u - sizeof(st_hdr_t);                /* every real bank image (3716 B and under): the payload at byte 32, */
    if (compact) payload = sizeof(st_hdr_t);                               /* the last 256 bytes of the sector stay erased */
    if ((s = lg_pick()) == LG_NONE) return -8;
    old = lg_map[bank];
    if ((rc = lg_erase(s)) != 0) return rc;
    for (off = 0; off < len; off += sizeof st_buf) {
        uint32_t n = len - off > sizeof st_buf ? (uint32_t)sizeof st_buf : len - off;
        memcpy(st_buf, (const uint8_t *)src + off, n);
        if ((rc = lg_prog(lg_addr[s] + payload + off, st_buf, n)) != 0) return rc;
    }
    h.magic = ST_MAGIC;
    h.type = LG_TYPE;
    h.slot = (uint16_t)bank;
    h.seq = lg_seq[bank] + 1u;                                           /* (0.11.1: lg_seq is 0 for a bank with no copy, so one rule; it also moves past a failed save, below) */
    h.len = len;
    h.crc = st_crc32(src, len);
    h.rsv[0] = compact ? ST_COMPACT_TAG : 0xFFFFFFFFu;
    h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = lg_prog(lg_addr[s], &h, sizeof h)) != 0) return rc;       /* the commit record, last */
    if (lg_rd_hdr(s, &chk) || memcmp(&chk, &h, sizeof h) || lg_rd_body(s, &chk)) {   /* read back */
        /* (0.11.1) The header is programmed, so the sector may hold a complete record with the next sequence: it would win at the next
         * boot over the copy this save reported as still in charge, and a later save would share its sequence. Erase it (the previous
         * copy stays in charge for good). If the erase fails too, move lg_seq past it, so that no later save of this bank can share the sequence. */
        if (lg_erase(s) != 0) lg_seq[bank] = h.seq;
        return -7;
    }
    lg_map[bank] = (uint8_t)s;
    lg_seq[bank] = h.seq;
    lg_cursor = (uint8_t)((s + 1u) % LG_SECTORS);
    return 0;
}

/* Erase every free sector that still carries a valid record: the stale copies a save leaves behind (the wipe calls this, so
 * old presets are not left in the group after it). A power cut here changes nothing that matters: stale copies never win. */
static void lg_scrub(void)
{
    lg_ensure();
    uint32_t keep = lg_owned() | lg_busy;
    for (uint32_t s = 0; s < LG_SECTORS; s++) {
        st_hdr_t h;
        if (!((keep >> s) & 1u) && !lg_rd_hdr(s, &h)) lg_erase(s);
    }
}

/* ---- migration from the old A/B pairs ---------------------------------------------------------------------------------- */
static uint32_t lg_legacy_obj(uint32_t b)       /* the object that held bank b as a pair (the old up_obj) */
{
    if (b < 2u) return OBJ_UPRESET0 + b;
    if (b < 4u) return OBJ_UPRESET_EXT0 + b - 2u;
    if (b < 8u) return b == 4u ? OBJ_NATIVEFM0 : b == 5u ? OBJ_NATIVEFM0 + 1u : b == 6u ? OBJ_FM6BANK : OBJ_UPFM6_EXT;
    if (b < 16u) return OBJ_CZBANK0 + b - 8u;
    if (b < 21u) return OBJ_P5BANK0 + b - 16u;
    return OBJ_SHARED_EXT;
}
static uint32_t lg_legacy_bank(uint32_t obj)    /* the bank an old object held, LG_NONE for any other */
{
    for (uint32_t b = 0; b < LG_BANKS; b++) if (lg_legacy_obj(b) == obj) return b;
    return LG_NONE;
}

static int lg_legacy_cur(uint32_t b, st_hdr_t *h)   /* the old pair's current copy (retirement does not hide it here) */
{
    int cur;
    st_legacy_io = 1;
    cur = st_current(lg_legacy_obj(b), h);
    st_legacy_io = 0;
    return cur;
}

/* Erase what an old pair left, for a bank the group holds: only sectors that still carry that object's own record. */
static void lg_leftovers(uint32_t b)
{
    uint32_t obj = lg_legacy_obj(b), own = lg_owned();
    for (uint32_t c = 0; c < 2u; c++) {
        uint32_t a = st_sector(obj, c), i = lg_index_of(a);
        st_hdr_t h;
        if (i != LG_NONE && ((own >> i) & 1u)) continue;               /* a group copy lives there now */
        if (st_read(a, &h, sizeof h) || h.magic != ST_MAGIC || h.type != obj) continue;
        OC_FEED();
        st_erase(a);
        lg_erases++;
    }
}

/* Bring every bank with an old pair into the group. Returns the number copied (< 0: a flash error stopped it, to be retried). */
static int lg_migrate(void)
{
    static const uint8_t order[LG_BANKS] = {0, 1, 2, 3, 4, 5, 6, 7, 14, 15, 21, 8, 9, 10, 11, 12, 13, 16, 17, 18, 19, 20};
    st_hdr_t lh[LG_BANKS];
    uint32_t need = 0, total = 0, done = 0;
    int copied = 0;
    lg_busy = 0;
    for (uint32_t b = 0; b < LG_BANKS; b++) {
        int cur = lg_legacy_cur(b, &lh[b]);
        if (cur < 0) continue;
        if (lg_map[b] != LG_NONE && !lg_newer(lh[b].seq, lg_seq[b])) continue;   /* the group's copy is the newer or the same */
        need |= 1u << b;
        total++;
        {   /* (0.11.1) busy: the sector of the CURRENT old record only. The other sector of the pair holds nothing the move needs
             * (an older record, a group copy of another bank which is owned anyway, or nothing) and may be taken */
            uint32_t i = lg_index_of(st_sector(lg_legacy_obj(b), (uint32_t)cur));
            if (i != LG_NONE) lg_busy |= 1u << i;
        }
    }
    if (total && lg_progress) lg_progress(0, total);
    for (uint32_t k = 0; k < LG_BANKS; k++) {
        uint32_t b = order[k], obj = lg_legacy_obj(b), s, off, po, cur;
        st_hdr_t h;
        int rc = 0, c0;
        if (!((need >> b) & 1u)) continue;
        c0 = lg_legacy_cur(b, &h);                                   /* (read again: the pair may have been erased since) */
        if (c0 < 0) continue;
        cur = (uint32_t)c0;
        po = st_payload_off_header(obj, &h);                            /* where the old copy's payload is */
        uint32_t pd = h.len <= ST_SECTOR - 256u - sizeof(st_hdr_t) ? (uint32_t)sizeof(st_hdr_t) : po;   /* and where the group's goes */
        if ((s = lg_pick_move(need, b)) == LG_NONE) return -8;
        if ((rc = lg_erase(s)) != 0) return rc;
        for (off = 0; off < h.len && !rc; off += sizeof st_buf) {
            uint32_t n = h.len - off > sizeof st_buf ? (uint32_t)sizeof st_buf : h.len - off;
            if (st_read_range(obj, cur, off, st_buf, n)) return -1;
            rc = lg_prog(lg_addr[s] + pd + off, st_buf, n);
        }
        if (rc) return rc;
        {
            st_hdr_t n = h;
            n.type = LG_TYPE;
            n.slot = (uint16_t)b;
            n.rsv[0] = pd == sizeof(st_hdr_t) ? ST_COMPACT_TAG : 0xFFFFFFFFu;     /* the payload offset, as the record says it */
            n.hcrc = st_crc32(&n, sizeof n - 4u);
            if ((rc = lg_prog(lg_addr[s], &n, sizeof n)) != 0) return rc;   /* the commit record, last */
            if (lg_rd_hdr(s, &h) || memcmp(&h, &n, sizeof n) || lg_rd_body(s, &h)) return -7;
            lg_map[b] = (uint8_t)s;
            lg_seq[b] = n.seq;
        }
        lg_cursor = (uint8_t)((s + 1u) % LG_SECTORS);
        for (uint32_t c = 0; c < 2u; c++) {                             /* the copy verified: the old pair is free to go */
            uint32_t i = lg_index_of(st_sector(obj, c));
            if (i != LG_NONE) lg_busy &= ~(1u << i);
        }
        lg_leftovers(b);
        copied++;
        done++;
        if (lg_progress) lg_progress(done, total);
    }
    for (uint32_t b = 0; b < LG_BANKS; b++) if (lg_map[b] != LG_NONE) lg_leftovers(b);   /* a cut erase from an earlier boot */
    return copied;
}

/* (0.11.1) The way out when the move cannot finish (lg_needwipe, shared_wipe): erase every old pair record that still waits, in
 * the group's sectors and outside them (only sectors that carry that object's own record, as lg_leftovers). The records are what
 * hold sectors busy and what would bring the old presets back at the next boot; with them gone the wipe saves its empty banks like
 * any save, and the migration has nothing left to do. A power loss here leaves some records: the next boot moves those in (or
 * reports again), and the wipe can be run again. */
static void lg_drop_pairs(void)
{
    for (uint32_t b = 0; b < LG_BANKS; b++) {
        uint32_t obj = lg_legacy_obj(b);
        for (uint32_t c = 0; c < 2u; c++) {
            uint32_t a = st_sector(obj, c);
            st_hdr_t h;
            if (st_read(a, &h, sizeof h) || h.magic != ST_MAGIC || h.type != obj) continue;
            OC_FEED();
            if (st_erase(a) == 0) lg_erases++;
        }
    }
    lg_busy = 0;
    lg_incomplete = 0;
    lg_needwipe = 0;
#ifdef LG_TEST_HOOKS
    lg_test_reserve = 0;                           /* (the stand-in goes with the records it stands for) */
#endif
    lg_ready = 0;
}

/* Boot: scan the group, then migrate. Call once before the pack check; safe to call again (it rescans). */
static int lg_boot(void)
{
    int rc;
    lg_scan();
    rc = lg_migrate();
    lg_incomplete = rc < 0;
    lg_needwipe = rc == -8;
    return rc;
}
