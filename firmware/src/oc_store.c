/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the record store (build doc phase 4a, part 3).
 *
 * What it is
 *   One store of committed records in the flash Melodee gave its four bank projects: 0xA0000..0xC7FFF (40 sectors)
 *   and their extension copies 0xEA000..0xF9FFF (16 sectors), 56 sectors of 4 KiB in all. Orange Cat keeps its
 *   projects (OCS_PROJECT, one per slot), its pattern pool (OCS_POOL, sixteen chunks of four patterns) and, later,
 *   samples and bounces here. Melodee's bank projects are not read from this area any more (the build doc, 4a part 3).
 *
 * A record
 *   type (OCS_*), id (its number within the type), and a payload of up to OCS_PARTS_MAX * OCS_PAYLOAD_MAX bytes (45,696),
 *   split over that many sectors, its parts. Each part's sector holds:
 *     0 .. 31        the commit header (ocs_hdr_t): magic, type, part, parts, id, seq, this part's length and CRC,
 *                    the whole record's length, the header's own CRC
 *     32 .. 3839     this part's payload (OCS_PAYLOAD_MAX = 3808 bytes)
 *     3840 .. 4095   left erased: the boot loader scans sector tails for update records (as storage.c keeps them)
 *
 * Power-loss safety (as storage.c, and Skiff's bank.c)
 *   A save writes a new copy to free sectors: erase, the payload pages, then the commit headers LAST, all parts' after
 *   all parts' payloads. A copy counts only when every part's header and payload check and they agree (same seq,
 *   parts, total). The copy before stays where it was until its sectors are reused, so a write torn anywhere leaves
 *   the copy before in charge. The newest complete copy wins (seq compared with wrap, as storage.c).
 *
 * Free sectors
 *   A sector is free when it holds no part of any record's current copy. Saves take free sectors round from a cursor
 *   (wear spread), never one of the current copy they replace. ocs_delete erases every copy of a record, old ones too
 *   (else an older copy would come back).
 *
 * RAM
 *   ocs_hdr[]: each sector's header as the boot scan read it (0 magic: none valid, or its payload did not check),
 *   kept up to date by saves and deletes; 56 x 32 bytes in the pool section. Payload CRCs are checked again when a
 *   record is read or found.
 *
 * Flash access is storage.c's: st_read, st_erase, st_prog (the host tests give a simulated NOR); without the flash
 * driver (host builds of the UI) the sectors are kept in RAM.
 *
 * Revisions:
 *   0.3.0: first version.
 *   0.3.1: saves stream their payload from a source (ocs_src_t): no RAM image of a record. ocs_open / ocs_at read
 *          pieces of one. CRC by nibbles (ocs_crc_add).
 *   0.3.2: its own CRC for the headers too (no storage.c needed); without the flash driver the sectors are in RAM.
 *   0.6.0: ocs_resv: sectors 18..39 (0xB2000..0xC7FFF) can be the internal sample slot's (smp_slot.c): once claimed the
 *          store has 34 sectors and never reads, hands out or erases those.
 */
#define OCS_MAGIC 0x3153434Fu                  /* "OCS1" */
#define OCS_SECTOR 4096u
#define OCS_HDR 32u
#define OCS_TAIL 256u
#define OCS_PAYLOAD_MAX (OCS_SECTOR - OCS_HDR - OCS_TAIL)   /* 3808 */
#define OCS_PARTS_MAX 12u                      /* (a pool chunk of four patterns full of timed notes: 45 KB) */
#define OCS_NSECT 56u
enum { OCS_NONE, OCS_PROJECT, OCS_POOL, OCS_SAMPLE, OCS_FREEZE, OCS_TYPES };   /* (0.5.6) OCS_FREEZE: the frozen phrases
                                                * (usb_files.c frz_table_save), one record */

typedef struct {
    uint32_t magic;
    uint8_t type, part, parts, rsv;
    uint16_t id, rsv2;
    uint32_t seq;
    uint32_t len;                              /* this part's payload */
    uint32_t crc;                              /* .. its CRC-32 */
    uint32_t total;                            /* the whole record's payload */
    uint32_t hcrc;                             /* CRC-32 of the 28 bytes before */
} ocs_hdr_t;
_Static_assert(sizeof(ocs_hdr_t) == OCS_HDR, "record store commit header");

static ocs_hdr_t ocs_hdr[OCS_NSECT] __attribute__((section(".pool")));
static uint8_t ocs_cursor;                     /* where the next save starts looking for free sectors */
static uint64_t ocs_held;                      /* (0.14.2) sectors an open stream (ocs_stream_t) is writing: not free to
                                                * another save until it commits or is dropped */
static uint64_t ocs_resv;                      /* (0.6.0) sectors that are not the store's: the sample slot's
                                                * (smp_slot.c), once claimed. Never read, handed out or erased here */

/* sector s's flash offset: 0..39 at 0xA0000, 40..55 at 0xEA000 */
static uint32_t ocs_addr(uint32_t s) { return s < 40u ? 0xA0000u + s * OCS_SECTOR : 0xEA000u + (s - 40u) * OCS_SECTOR; }

#if MELODEE_FLASH
#define OCS_RD st_read
#define OCS_ER st_erase
#define OCS_PG st_prog
#else
/* (host builds without the flash driver: the UI tests) the store's 56 sectors in RAM, as erased flash at first */
static uint8_t ocs_ram[OCS_NSECT][OCS_SECTOR];
static uint8_t *ocs_ram_at(uint32_t off)
{
    uint32_t s = off < 0xEA000u ? (off - 0xA0000u) / OCS_SECTOR : 40u + (off - 0xEA000u) / OCS_SECTOR;
    return ocs_ram[s % OCS_NSECT] + off % OCS_SECTOR;
}
static int OCS_RD(uint32_t off, void *dst, uint32_t n) { memcpy(dst, ocs_ram_at(off), n); return 0; }
static int OCS_ER(uint32_t off) { memset(ocs_ram_at(off), 0xFF, OCS_SECTOR); return 0; }
static int OCS_PG(uint32_t off, const void *src, uint32_t n)
{
    uint8_t *d = ocs_ram_at(off);
    const uint8_t *q = src;
    for (uint32_t i = 0; i < n; i++) d[i] &= q[i];
    return 0;
}
#endif

/* CRC-32 (zlib's, as st_crc32) carried over pieces: start at 0xFFFFFFFF, the result is ~crc */
static uint32_t ocs_crc_add(uint32_t crc, const uint8_t *p, uint32_t n)
{
    static const uint32_t T[16] = {0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u,
                                   0x4DB26158u, 0x5005713Cu, 0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu,
                                   0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    while (n--) {
        crc ^= *p++;
        crc = (crc >> 4) ^ T[crc & 15u];
        crc = (crc >> 4) ^ T[crc & 15u];
    }
    return crc;
}

static uint32_t ocs_crc32(const void *p, uint32_t n) { return ~ocs_crc_add(0xFFFFFFFFu, p, n); }

static int ocs_hdr_ok(const ocs_hdr_t *h)
{
    return h->magic == OCS_MAGIC && h->type > OCS_NONE && h->type < OCS_TYPES && h->parts >= 1u &&
           h->parts <= OCS_PARTS_MAX && h->part < h->parts && h->len <= OCS_PAYLOAD_MAX &&
           h->total <= (uint32_t)h->parts * OCS_PAYLOAD_MAX && h->hcrc == ocs_crc32(h, OCS_HDR - 4u);
}


/* a's seq newer than b's (with wrap, as storage.c st_current) */
static int ocs_newer(uint32_t a, uint32_t b) { return a != b && a - b < 0x80000000u; }

/* the payload of part sector s checks against its header */
static int ocs_body_ok(uint32_t s)
{
    uint8_t buf[256];
    uint32_t crc = 0xFFFFFFFFu, len = ocs_hdr[s].len;
    for (uint32_t off = 0; off < len; off += sizeof buf) {
        uint32_t n = len - off > sizeof buf ? sizeof buf : len - off;
        if (OCS_RD(ocs_addr(s) + OCS_HDR + off, buf, n)) return 0;
        crc = ocs_crc_add(crc, buf, n);
    }
    return ~crc == ocs_hdr[s].crc;
}

/* boot: every sector's header, kept only when it and its payload check (about 210 KB read, some 20 ms). A part whose
 * payload has rotted is dropped here, so its copy is incomplete and the copy before stays in charge: its sectors are
 * not handed out as free */
static void ocs_boot(void)
{
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        if (((ocs_resv >> s) & 1u) ||
            OCS_RD(ocs_addr(s), &ocs_hdr[s], OCS_HDR) || !ocs_hdr_ok(&ocs_hdr[s]) || !ocs_body_ok(s))
            memset(&ocs_hdr[s], 0, OCS_HDR);
    }
    ocs_cursor = 0;
}

/* the sectors of copy seq of record (type, id), by part (sect[part]); its part count, 0 when not complete
 * (a part missing, or the parts disagreeing) */
static uint32_t ocs_copy(uint32_t type, uint32_t id, uint32_t seq, uint8_t *sect)
{
    uint32_t parts = 0, have = 0, total = 0;
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *h = &ocs_hdr[s];
        if (h->magic != OCS_MAGIC || h->type != type || h->id != id || h->seq != seq)
            continue;
        if (!parts) { parts = h->parts; total = h->total; }
        if (h->parts != parts || h->total != total || ((have >> h->part) & 1u))
            return 0;                          /* (two sectors claiming one part: not a copy to trust) */
        have |= 1u << h->part;
        sect[h->part] = (uint8_t)s;
    }
    if (!parts || have != (1u << parts) - 1u)
        return 0;
    {   /* the lengths add up: every part full but the last */
        uint32_t sum = 0;
        for (uint32_t p = 0; p < parts; p++) {
            const ocs_hdr_t *h = &ocs_hdr[sect[p]];
            if (p + 1u < parts && h->len != OCS_PAYLOAD_MAX) return 0;
            sum += h->len;
        }
        if (sum != total) return 0;
    }
    return parts;
}

/* the current copy of (type, id): the newest complete one whose payloads check. Its part count and sectors (sect),
 * its seq (*seq); 0 when there is none. check 0: headers only (the boot's free map, ocs_free) */
static uint32_t ocs_current(uint32_t type, uint32_t id, uint8_t *sect, uint32_t *seq, int check)
{
    uint32_t tried[OCS_NSECT], ntried = 0;
    for (;;) {
        uint32_t best = 0, found = 0;
        for (uint32_t s = 0; s < OCS_NSECT; s++) {         /* the newest seq not tried yet */
            const ocs_hdr_t *h = &ocs_hdr[s];
            uint32_t t, skip = 0;
            if (h->magic != OCS_MAGIC || h->type != type || h->id != id)
                continue;
            for (t = 0; t < ntried; t++)
                if (tried[t] == h->seq) skip = 1;
            if (skip) continue;
            if (!found || ocs_newer(h->seq, best)) { best = h->seq; found = 1; }
        }
        if (!found)
            return 0;
        tried[ntried++] = best;
        uint32_t parts = ocs_copy(type, id, best, sect), p;
        if (!parts)
            continue;
        for (p = 0; check && p < parts && ocs_body_ok(sect[p]); p++)
            ;
        if (!check || p == parts) {
            *seq = best;
            return parts;
        }
    }
}

/* sector s is free: no record's current copy (by headers) has a part there */
static int ocs_free(uint32_t s)
{
    const ocs_hdr_t *h = &ocs_hdr[s];
    uint8_t sect[OCS_PARTS_MAX];
    uint32_t seq, parts;
    if (((ocs_resv | ocs_held) >> s) & 1u)
        return 0;                              /* (0.6.0) the sample slot's; (0.14.2) an open stream's */
    if (h->magic != OCS_MAGIC)
        return 1;
    parts = ocs_current(h->type, h->id, sect, &seq, 0);
    return !parts || seq != h->seq;
}
static uint32_t ocs_free_count(void)
{
    uint32_t n = 0;
    for (uint32_t s = 0; s < OCS_NSECT; s++) n += ocs_free(s);
    return n;
}

/* read record (type, id) whole into dst (max bytes): its length, -1 none (or too big for dst) */
static int ocs_read(uint32_t type, uint32_t id, void *dst, uint32_t max)
{
    uint8_t sect[OCS_PARTS_MAX];
    uint32_t seq, parts = ocs_current(type, id, sect, &seq, 1), total = 0;
    if (!parts) return -1;
    for (uint32_t p = 0; p < parts; p++) total += ocs_hdr[sect[p]].len;
    if (total > max) return -1;
    for (uint32_t p = 0, off = 0; p < parts; p++) {
        if (OCS_RD(ocs_addr(sect[p]) + OCS_HDR, (uint8_t *)dst + off, ocs_hdr[sect[p]].len)) return -1;
        off += ocs_hdr[sect[p]].len;
    }
    return (int)total;
}

/* program n bytes at addr without crossing a 256-byte page (st_prog's rule) */
static int ocs_prog(uint32_t addr, const uint8_t *src, uint32_t n)
{
    while (n) {
        uint32_t k = 256u - (addr & 255u);
        int rc;
        if (k > n) k = n;
        if ((rc = OCS_PG(addr, src, k)) != 0) return rc;
        addr += k; src += k; n -= k;
    }
    return 0;
}

/* where a save's payload comes from: get fills dst with the next n bytes, in order (0 done, else the save stops and
 * returns that). A record is written straight from where its bytes are (a pattern packed as it goes, another record's
 * old copy), with no RAM image of it */
typedef struct ocs_src { int (*get)(struct ocs_src *, uint8_t *dst, uint32_t n); } ocs_src_t;
typedef struct { ocs_src_t src; const uint8_t *p; } ocs_mem_t;
static int ocs_mem_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    ocs_mem_t *m = (ocs_mem_t *)s;
    memcpy(dst, m->p, n);
    m->p += n;
    return 0;
}

/* (0.14.2) a record written as its bytes arrive, in order, across calls (a backup restore's pieces over USB: editor_backup.c),
 * with no RAM image of it; ocs_write_copy is the same writer with all its bytes at hand. begin takes the free sectors (held
 * from other saves meanwhile: ocs_held); put erases each part's sector as its first byte comes and programs the bytes a
 * page at a time; commit writes every part's commit header, last, with a seq past every copy's then (another save of the
 * record may have come between), and reads them back. Until the commit nothing counts: dropped (ocs_stream_drop, or
 * never committed), the sectors are blank or hold payload without a header, which is free ground, and the copy before
 * stays in charge */
typedef struct {
    uint8_t use[OCS_PARTS_MAX], open;
    uint32_t type, id, len, pos, parts, crc;   /* crc: the part being written's, as far as it goes */
    ocs_hdr_t h[OCS_PARTS_MAX];
    uint64_t mask;                             /* use[] as a mask (ocs_held) */
} ocs_stream_t;
static void ocs_stream_drop(ocs_stream_t *w)
{
    if (w->open) ocs_held &= ~w->mask;
    w->open = 0;
}
/* 0 begun; -1 too big or no room (nothing written) */
static int ocs_stream_begin(ocs_stream_t *w, uint32_t type, uint32_t id, uint32_t len)
{
    uint8_t cur[OCS_PARTS_MAX];
    uint32_t parts = (len + OCS_PAYLOAD_MAX - 1u) / OCS_PAYLOAD_MAX, n = 0, s, p, ncur, seq;
    ocs_stream_drop(w);
    if (!parts) parts = 1u;                     /* (an empty record still has one part: its header) */
    if (type <= OCS_NONE || type >= OCS_TYPES || parts > OCS_PARTS_MAX) return -1;
    /* the copy a read gives now (payloads checked) is kept until the new one is in: a newer copy whose payload has
     * rotted counts as current by its headers alone, and would otherwise leave this one free to reuse */
    ncur = ocs_current(type, id, cur, &seq, 1);
    for (uint32_t k = 0; k < OCS_NSECT && n < parts; k++) {   /* free sectors, round from the cursor */
        uint32_t keep = 0;
        s = (ocs_cursor + k) % OCS_NSECT;
        for (p = 0; p < ncur; p++) keep |= cur[p] == s;
        if (!keep && ocs_free(s)) w->use[n++] = (uint8_t)s;
    }
    if (n < parts) return -1;
    ocs_cursor = (uint8_t)((w->use[parts - 1u] + 1u) % OCS_NSECT);
    w->type = type; w->id = id; w->len = len; w->pos = 0; w->parts = parts; w->crc = 0xFFFFFFFFu;
    w->mask = 0;
    for (p = 0; p < parts; p++) w->mask |= (uint64_t)1u << w->use[p];
    ocs_held |= w->mask;
    w->open = 1;
    return 0;
}
/* part p's header, its payload all written (k bytes, CRC crc) */
static void ocs_stream_head(ocs_stream_t *w, uint32_t p, uint32_t k, uint32_t crc)
{
    ocs_hdr_t *h = &w->h[p];
    h->magic = OCS_MAGIC; h->type = (uint8_t)w->type; h->part = (uint8_t)p; h->parts = (uint8_t)w->parts;
    h->rsv = 0xFF; h->id = (uint16_t)w->id; h->rsv2 = 0xFFFFu; h->len = k; h->crc = ~crc; h->total = w->len;
}
/* the next n bytes. 0 done; a negative: a flash error (the stream dropped) or bytes past its length */
static int ocs_stream_put(ocs_stream_t *w, const uint8_t *src, uint32_t n)
{
    int rc;
    if (!w->open || n > w->len - w->pos) return -1;
    while (n) {
        uint32_t p = w->pos / OCS_PAYLOAD_MAX, o = w->pos % OCS_PAYLOAD_MAX, addr = ocs_addr(w->use[p]) + OCS_HDR + o;
        uint32_t k = OCS_PAYLOAD_MAX - o, m = 256u - (addr & 255u);
        if (o == 0u) {                         /* a part's first byte: its sector erased */
            memset(&ocs_hdr[w->use[p]], 0, OCS_HDR);   /* (erased: no longer what the scan saw) */
            if ((rc = OCS_ER(ocs_addr(w->use[p]))) != 0) { ocs_stream_drop(w); return rc; }
            w->crc = 0xFFFFFFFFu;
        }
        if (m > k) m = k;
        if (m > n) m = n;
        w->crc = ocs_crc_add(w->crc, src, m);
        if ((rc = OCS_PG(addr, src, m)) != 0) { ocs_stream_drop(w); return rc; }
        src += m; n -= m; w->pos += m;
        if (w->pos % OCS_PAYLOAD_MAX == 0u || w->pos == w->len)
            ocs_stream_head(w, p, (w->pos - 1u) % OCS_PAYLOAD_MAX + 1u, w->crc);
    }
    return 0;
}
/* the commit (every byte put). 0 done; -1 not every byte there (nothing committed); another negative: a flash error */
static int ocs_stream_commit(ocs_stream_t *w)
{
    uint32_t seq = 0, have = 0, s, p;
    int rc;
    if (!w->open || w->pos != w->len) { ocs_stream_drop(w); return -1; }
    if (!w->len) {                             /* (an empty record: its one part erased now, a header and no payload) */
        memset(&ocs_hdr[w->use[0]], 0, OCS_HDR);
        if ((rc = OCS_ER(ocs_addr(w->use[0]))) != 0) { ocs_stream_drop(w); return rc; }
        ocs_stream_head(w, 0, 0, 0xFFFFFFFFu);
    }
    /* the new seq is past every copy's, torn ones too: a torn copy sharing the new seq would spoil it */
    for (s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *o = &ocs_hdr[s];
        if (o->magic != OCS_MAGIC || o->type != w->type || o->id != w->id) continue;
        if (!have || ocs_newer(o->seq, seq)) seq = o->seq;
        have = 1;
    }
    for (p = 0; p < w->parts; p++) {
        w->h[p].seq = seq + 1u;
        w->h[p].hcrc = ocs_crc32(&w->h[p], OCS_HDR - 4u);
    }
    ocs_stream_drop(w);                        /* (from here the sectors are the copy's, by their headers) */
    for (p = 0; p < w->parts; p++)             /* the commit headers, last */
        if ((rc = ocs_prog(ocs_addr(w->use[p]), (const uint8_t *)&w->h[p], OCS_HDR)) != 0) return rc;
    for (p = 0; p < w->parts; p++) {           /* read back: a failing part must not report saved */
        ocs_hdr_t chk;
        if (OCS_RD(ocs_addr(w->use[p]), &chk, OCS_HDR) || memcmp(&chk, &w->h[p], OCS_HDR)) return -7;
        ocs_hdr[w->use[p]] = w->h[p];
        if (!ocs_body_ok(w->use[p])) { memset(&ocs_hdr[w->use[p]], 0, OCS_HDR); return -7; }
    }
    return 0;
}

/* save record (type, id): len bytes from src, as a new copy. 0 done; -1 too big or no room (the store keeps what it
 * had); another negative: a flash error, or src's (the copy before is still in charge) */
static int ocs_write_copy(uint32_t type, uint32_t id, ocs_src_t *src, uint32_t len)
{
    static ocs_stream_t w __attribute__((section(".pool")));
    uint8_t buf[256];
    int rc;
    if ((rc = ocs_stream_begin(&w, type, id, len)) != 0) return rc;
    while (w.pos < len) {                      /* (a page at a time: put splits nothing further) */
        uint32_t o = w.pos % OCS_PAYLOAD_MAX, m = 256u - ((ocs_addr(w.use[w.pos / OCS_PAYLOAD_MAX]) + OCS_HDR + o) & 255u);
        if (m > OCS_PAYLOAD_MAX - o) m = OCS_PAYLOAD_MAX - o;
        if (m > len - w.pos) m = len - w.pos;
        if ((rc = src->get(src, buf, m)) != 0) { ocs_stream_drop(&w); return rc < -1 ? rc : -10; }
        if ((rc = ocs_stream_put(&w, buf, m)) != 0) return rc;
    }
    return ocs_stream_commit(&w);
}

static int ocs_write_src(uint32_t type, uint32_t id, ocs_src_t *src, uint32_t len)
{
    int rc = ocs_write_copy(type, id, src, len);
    if (rc < -1) {   /* a flash error part way: what is on the flash now is read again (a torn copy's headers, with
                      * the seq the next save must pass, would otherwise be unknown until the next boot) */
        uint8_t cursor = ocs_cursor;
        ocs_boot();
        ocs_cursor = cursor;
    }
    return rc;
}

/* save record (type, id): len bytes from RAM at src */
static int ocs_write(uint32_t type, uint32_t id, const void *src, uint32_t len)
{
    ocs_mem_t m = {{ocs_mem_get}, (const uint8_t *)src};
    return ocs_write_src(type, id, &m.src, len);
}

/* forget record (type, id): every copy's sectors erased (an older copy left would come back). 0 done */
static int ocs_delete(uint32_t type, uint32_t id)
{
    int rc = 0;
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        if (ocs_hdr[s].magic != OCS_MAGIC || ocs_hdr[s].type != type || ocs_hdr[s].id != id || ((ocs_resv >> s) & 1u))
            continue;
        memset(&ocs_hdr[s], 0, OCS_HDR);
        if (OCS_ER(ocs_addr(s))) rc = -8;
    }
    return rc;
}

/* a record's current copy, opened to read pieces of it (ocs_at) */
typedef struct { uint8_t sect[OCS_PARTS_MAX], parts; uint32_t total; } ocs_rec_t;
static int ocs_open(uint32_t type, uint32_t id, ocs_rec_t *r)
{
    uint32_t seq;
    r->parts = (uint8_t)ocs_current(type, id, r->sect, &seq, 1);
    r->total = 0;
    for (uint32_t p = 0; p < r->parts; p++) r->total += ocs_hdr[r->sect[p]].len;
    return r->parts ? 0 : -1;
}
/* n bytes of an opened record from off. 0 done */
static int ocs_at(const ocs_rec_t *r, uint32_t off, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    if (off > r->total || n > r->total - off) return -1;
    while (n) {
        uint32_t p = off / OCS_PAYLOAD_MAX, o = off % OCS_PAYLOAD_MAX, k = OCS_PAYLOAD_MAX - o;
        if (k > n) k = n;
        if (OCS_RD(ocs_addr(r->sect[p]) + OCS_HDR + o, d, k)) return -1;
        d += k; off += k; n -= k;
    }
    return 0;
}

/* a record (type, id) is there */
static int ocs_exists(uint32_t type, uint32_t id)
{
    uint8_t sect[OCS_PARTS_MAX];
    uint32_t seq;
    return ocs_current(type, id, sect, &seq, 1) != 0;
}
