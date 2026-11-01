/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format, OC_R3_FORMAT): the project's own phrases in packed chunks.
 * Included by oc_pool.c after its packer and reader; the design is docs/orangecat/PROJECT_FORMAT_R3.md section 3 and
 * UI_BUILD.md "Release 3 format revision, Part 1" (the choices and their review).
 *
 * What it keeps
 *   The pool is one project's: the phrases 1..255 of the slot the music was loaded from (or saved to), or of the
 *   SCRATCH (slot 4: music that has no slot yet; its chunks die at the next boot). A phrase's bytes are oc_pool.c's
 *   (version 2), packed into chunks: a chunk is a record of type OCS_POOL whose id is
 *       0x8000 | slot << 9 | generation << 8 | chunk number (1..255)
 *   and whose payload is a head table, `u8 n` then n x (`u8 phrase id + 1`, `u16 length`), then the phrases' bytes in
 *   that order. A chunk holds several phrases of at most one part (OCS_PAYLOAD_MAX, 3,808 B) together, or one phrase of
 *   any size. The directory (which chunk each phrase is in, the generation bit of each chunk) lives in the project
 *   record's PHRD section (oc_ocj4.c) and, for the live project, in ocp.dir below.
 *
 * The generation bit is the save protocol
 *   The record names, for each chunk number, the generation it reads. A write to chunk C goes under the OTHER
 *   generation when C's live generation equals the committed one (the record reads this copy), and under the live
 *   generation again when they already differ (kept since the last save and not yet committed: the store's own
 *   copy-on-write keeps the previous copy until the new one commits). So a copy the record names is never written to
 *   until the record moves off it; the record, written last, names the live generations; after its commit the live
 *   tables are read again from the flash (ocp_r3_switch) and the SWEEP erases every chunk record of the slot the record
 *   does not name (either generation, any number). The sweep also runs at boot for every slot and for the scratch. A
 *   failed save does not sweep: the live-generation chunks are the music's kept state (the banks' signatures say
 *   "kept").
 *
 * Placing a kept phrase (ocp_r3_place)
 *   Its chunk is rewritten in place when it holds the phrase alone or its other live phrases plus the new bytes fit a
 *   part; else the phrase goes to the lowest-numbered chunk with room (first fit), else to a free number. The chunk it
 *   left is NOT rewritten: its bytes for the phrase go stale and are dropped when that chunk is next written (a
 *   writer emits only the phrases whose directory entry names its chunk). ocp.used[] keeps each chunk's live bytes so
 *   no chunk is opened to ask.
 *
 * Room
 *   A chunk write is refused (rc 2, PROJECT FULL) unless at least OCJ4_RESERVE free sectors remain after it: the
 *   project record needs them (up to 4 parts). The inequality, before the write:
 *       free - parts(new chunk) + (same id ? parts(the copy it replaces) : 0) >= OCJ4_RESERVE
 *
 * RAM: ocp (the directory 289 B, the committed bits 64 B, used[] 512 B, len[] 255 B, hash[] 1,020 B), the two head
 * tables (513 B each), the sweep's directory (321 B), the stream (about 60 B). The record format's own reader for the
 * sixteen shared chunks (the code above this file) stays for the conversion (Part 3).
 *
 * Revisions:
 *   Part 1 (October 31, 2026): first version.
 */
#define OCP_SCRATCH 4u                         /* the slot of music that has no slot yet */
#define OCP_NCHUNK 256u                        /* chunk numbers 1..255 (0: none) */
#define OCP_TAB_MAX 128u                       /* phrases a chunk may hold (a head table of 385 B on the flash) */
#define OCP_PART OCS_PAYLOAD_MAX               /* a chunk of several phrases fits one part */
#define OCJ4_RESERVE 4u                        /* free sectors a keep leaves for the project record */
#define OCJ4_RID(slot) (0x8000u | (slot))      /* the project record's id (type OCS_PROJECT) */
#ifndef OC_FEED
#define OC_FEED() ((void)0)
#endif

/* a slot's directory: for phrase id (0-based) the chunk number it is in (0: none), and a generation bit a chunk */
typedef struct {
    uint8_t slot;
    uint8_t chunk[OCP_N];
    uint8_t gen[OCP_NCHUNK / 8u];
} ocp_dir_t;

/* a chunk's head table as read or as about to be written: n entries, each a phrase id + 1 and its length; src says where
 * a new chunk's entry's bytes come from (OCP_SRC_*) */
typedef struct {
    uint8_t n;
    uint8_t id[OCP_TAB_MAX];
    uint16_t len[OCP_TAB_MAX];
    uint8_t src[OCP_TAB_MAX];
} ocp_tab_t;
enum { OCP_SRC_BASE, OCP_SRC_GEN, OCP_SRC_COPY };

static struct {
    ocp_dir_t dir;                             /* the live project's phrases: where each is, the live generation bits */
    uint8_t cgen[OCP_NCHUNK / 8u];             /* the slot's record: the generation it names for each chunk .. */
    uint8_t cnamed[OCP_NCHUNK / 8u];           /* .. and whether it names the chunk at all */
    uint16_t used[OCP_NCHUNK];                 /* each chunk's live bytes (its head table counted); 0: no live phrase */
    uint8_t len[OCP_N];                        /* each phrase's LEN in steps (0: empty), for the PATTERNS page */
    uint32_t hash[OCP_N];                      /* each phrase's bytes' CRC-32 (0: empty): the directory's, a save
                                                * writes only what differs */
    uint32_t gen;                              /* any change (the page redraws) */
} ocp __attribute__((section(".pool")));
static ocp_tab_t ocp_otab __attribute__((section(".pool")));   /* a chunk's table as it is on the flash */
static ocp_tab_t ocp_ntab __attribute__((section(".pool")));   /* the table of the chunk being written */
static const ocp_dir_t *ocp_rd_dir = &ocp.dir; /* the directory a read goes through (another slot's while it is read:
                                                * oc_ocj4.c; 0: the shared pool of the format before, for the conversion) */

static uint32_t ocp_bit(const uint8_t *m, uint32_t n) { return (m[n >> 3] >> (n & 7u)) & 1u; }
static void ocp_bit_set(uint8_t *m, uint32_t n, uint32_t v)
{
    if (v) m[n >> 3] |= (uint8_t)(1u << (n & 7u));
    else m[n >> 3] &= (uint8_t)~(1u << (n & 7u));
}
/* chunk c of slot `slot` under generation g: its record id */
static uint32_t ocp_rid(uint32_t slot, uint32_t g, uint32_t c) { return 0x8000u | slot << 9 | (g & 1u) << 8 | (c & 255u); }
static uint32_t ocp_parts_of(uint32_t bytes) { return bytes ? (bytes + OCS_PAYLOAD_MAX - 1u) / OCS_PAYLOAD_MAX : 1u; }

/* the directory of slot `slot` as its record names it: d filled (empty when there is no record), named[] the chunks
 * it names, len[] / hash[] each phrase's when given. 0 a record read, -1 none. Defined in oc_ocj4.c (the record's reader) */
static int ocj4_phrd_read(uint32_t slot, ocp_dir_t *d, uint8_t named[OCP_NCHUNK / 8u], uint8_t *len, uint32_t *hash);

/* ------------------------------------------------------------ the head table --- */
/* record r's head table into t (its entries' lengths checked against the record's total). 0, or -1 (not a chunk) */
static int ocp_tab_read(const ocs_rec_t *r, ocp_tab_t *t)
{
    uint8_t b[3u * OCP_TAB_MAX];
    uint32_t sum, i;
    memset(t, 0, sizeof *t);
    if (r->total < 1u || ocs_at(r, 0, &t->n, 1) || t->n > OCP_TAB_MAX) return -1;
    if (ocs_at(r, 1, b, 3u * t->n)) return -1;
    sum = 1u + 3u * t->n;
    for (i = 0; i < t->n; i++) {
        t->id[i] = b[3u * i];
        t->len[i] = (uint16_t)ocp_get16(b + 3u * i + 1u);
        if (!t->id[i] || t->id[i] > OCP_N || !t->len[i]) return -1;
        sum += t->len[i];
    }
    return sum == r->total ? 0 : -1;
}
/* where entry e's bytes start in a chunk with table t */
static uint32_t ocp_tab_off(const ocp_tab_t *t, uint32_t e)
{
    uint32_t off = 1u + 3u * t->n;
    for (uint32_t i = 0; i < e; i++) off += t->len[i];
    return off;
}
/* entry of phrase id (0-based) in table t, or t->n */
static uint32_t ocp_tab_find(const ocp_tab_t *t, uint32_t id)
{
    uint32_t e;
    for (e = 0; e < t->n && t->id[e] != id + 1u; e++)
        ;
    return e;
}

/* ------------------------------------------------------------ reading --- */
/* phrase id of directory d opened for reading at its start (rd). 1 there, 0 empty, -1 unreadable */
static int ocp_r3_open(const ocp_dir_t *d, uint32_t id, ocs_rec_t *r, ocp_rd_t *rd)
{
    uint32_t c, e;
    memset(rd, 0, sizeof *rd);
    if (id >= OCP_N || !(c = d->chunk[id])) return 0;
    if (ocs_open(OCS_POOL, ocp_rid(d->slot, ocp_bit(d->gen, c), c), r) || ocp_tab_read(r, &ocp_otab)) return -1;
    if ((e = ocp_tab_find(&ocp_otab, id)) == ocp_otab.n) return -1;   /* (the directory names a chunk without it) */
    rd->r = r; rd->off = ocp_tab_off(&ocp_otab, e); rd->end = rd->off + ocp_otab.len[e];
    return 1;
}
/* the bytes of phrase id in its live chunk (0: none, or a chunk that does not read) */
static uint32_t ocp_r3_len_in_chunk(uint32_t id)
{
    ocs_rec_t r;
    ocp_rd_t rd;
    return ocp_r3_open(&ocp.dir, id, &r, &rd) == 1 ? rd.end - rd.off : 0u;
}

/* ------------------------------------------------------------ writing a chunk --- */
/* a chunk streamed: its head table (ocp_ntab), then each entry's bytes from the copy before (base, found by its own
 * table ocp_otab), from the generator, or from another record (cp) */
static uint32_t ocp_r3_parts_live(void);
typedef struct ocp_cw {
    ocs_src_t src;
    ocs_rec_t base;
    int have_base;
    uint32_t pos, e, epos, cur_off;            /* the stream's position; the entry being given, how far, its source's offset */
    ocp_gen_t *g;                              /* the generator (the phrase kept); rewound at its entry's start */
    int (*regen)(struct ocp_cw *w, uint32_t id);   /* (several generated phrases: a foreign project's) g set up for phrase id */
    const ocs_rec_t *cp;                       /* a phrase copied from another record (ocp_copy) .. */
    uint32_t cp_off;                           /* .. at this offset */
} ocp_cw_t;
static ocp_cw_t ocp_cw __attribute__((section(".pool")));

static int ocp_cw_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    ocp_cw_t *w = (ocp_cw_t *)s;
    ocp_tab_t *t = &ocp_ntab;
    uint32_t hdr = 1u + 3u * t->n;
    while (n) {
        uint32_t k;
        if (w->pos < hdr) {                    /* the head table, built as it goes */
            uint8_t b[3];
            uint32_t i, o;
            if (!w->pos) { *dst++ = t->n; n--; w->pos++; continue; }
            i = (w->pos - 1u) / 3u; o = (w->pos - 1u) % 3u;
            b[0] = t->id[i]; ocp_put16(b + 1, t->len[i]);
            k = 3u - o < n ? 3u - o : n;
            memcpy(dst, b + o, k);
        } else {
            if (w->e >= t->n) return -1;       /* (asked past the length: a bug, the save stops) */
            if (w->epos == 0u) {               /* an entry's start: its source made ready */
                uint32_t id = (uint32_t)t->id[w->e] - 1u;
                if (t->src[w->e] == OCP_SRC_BASE) {
                    uint32_t oe = w->have_base ? ocp_tab_find(&ocp_otab, id) : ocp_otab.n;
                    if (oe == ocp_otab.n || ocp_otab.len[oe] != t->len[w->e]) return -11;
                    w->cur_off = ocp_tab_off(&ocp_otab, oe);
                } else if (t->src[w->e] == OCP_SRC_GEN) {
                    if (w->regen && w->regen(w, id)) return -1;
                    if (!w->g || w->g->len != t->len[w->e]) return -1;
                    ocp_gen_rewind(w->g);
                } else {
                    w->cur_off = w->cp_off;
                }
            }
            k = t->len[w->e] - w->epos < n ? t->len[w->e] - w->epos : n;
            if (t->src[w->e] == OCP_SRC_BASE) {
                if (ocs_at(&w->base, w->cur_off + w->epos, dst, k)) return -11;
            } else if (t->src[w->e] == OCP_SRC_GEN) {
                if (ocp_gen_get(&w->g->src, dst, k)) return -1;
            } else if (ocs_at(w->cp, w->cur_off + w->epos, dst, k)) {
                return -11;
            }
            w->epos += k;
            if (w->epos == t->len[w->e]) { w->e++; w->epos = 0; }
        }
        w->pos += k; dst += k; n -= k;
    }
    return 0;
}

/* the bytes ocp_ntab describes (table and phrases) */
static uint32_t ocp_ntab_total(void)
{
    uint32_t total = 1u + 3u * ocp_ntab.n;
    for (uint32_t i = 0; i < ocp_ntab.n; i++) total += ocp_ntab.len[i];
    return total;
}
/* an entry added to ocp_ntab (0, or -1 full) */
static int ocp_ntab_add(uint32_t id, uint32_t len, uint32_t src)
{
    if (ocp_ntab.n >= OCP_TAB_MAX) return -1;
    ocp_ntab.id[ocp_ntab.n] = (uint8_t)(id + 1u);
    ocp_ntab.len[ocp_ntab.n] = (uint16_t)len;
    ocp_ntab.src[ocp_ntab.n++] = (uint8_t)src;
    return 0;
}
/* ocp_ntab: chunk c's live phrases of directory d (from the copy before, whose table is ocp_otab), in id order, plus
 * phrase gid anew (newlen bytes from source src; OCP_N none; 0 bytes: gid leaves the chunk). 0, or -1 (a phrase the
 * directory puts in c is not in the chunk) */
static int ocp_ntab_build(const ocp_dir_t *d, uint32_t c, uint32_t gid, uint32_t newlen, uint32_t src)
{
    memset(&ocp_ntab, 0, sizeof ocp_ntab);
    for (uint32_t id = 0; id < OCP_N; id++) {
        if (id == gid) {
            if (newlen && ocp_ntab_add(id, newlen, src)) return -1;
            continue;
        }
        if (d->chunk[id] != c) continue;
        {
            uint32_t oe = ocp_tab_find(&ocp_otab, id);
            if (oe == ocp_otab.n || ocp_ntab_add(id, ocp_otab.len[oe], OCP_SRC_BASE)) return -1;
        }
    }
    return 0;
}
/* the copy before of chunk c of directory d opened as ocp_cw's base (its table into ocp_otab). 0; -1 the chunk does
 * not read (a record there whose table is wrong) */
static int ocp_cw_base(const ocp_dir_t *d, uint32_t c)
{
    memset(&ocp_cw, 0, sizeof ocp_cw);
    ocp_cw.src.get = ocp_cw_get;
    ocp_cw.have_base = ocs_open(OCS_POOL, ocp_rid(d->slot, ocp_bit(d->gen, c), c), &ocp_cw.base) == 0;
    if (ocp_cw.have_base) return ocp_tab_read(&ocp_cw.base, &ocp_otab);
    memset(&ocp_otab, 0, sizeof ocp_otab);
    return 0;
}

/* the generation chunk c of the live pool is written under next: the other one when the record reads the live copy,
 * else the live one again (see the top) */
static uint32_t ocp_next_gen(uint32_t c)
{
    uint32_t live = ocp_bit(ocp.dir.gen, c);
    return ocp_bit(ocp.cnamed, c) && ocp_bit(ocp.cgen, c) == live ? !live : live;
}

/* chunk c of the live pool written: its live phrases as they are, and phrase gid's bytes anew (from g, or copied from
 * cp at cp_off, cp_len bytes; gid OCP_N: none; 0 bytes: gid leaves the chunk). The directory, used[c] and the live
 * generation follow a success; the chunk gid may have left is the caller's (ocp_r3_place). 0 done; 2 no room (the
 * reserve); -1 a chunk that does not read; another: a flash error */
static int ocp_r3_write_chunk(uint32_t c, uint32_t gid, ocp_gen_t *g, const ocs_rec_t *cp, uint32_t cp_off, uint32_t cp_len)
{
    uint32_t live = ocp_bit(ocp.dir.gen, c), next = ocp_next_gen(c), total, parts, keep = 0, newlen, free;
    int rc;
    newlen = g ? g->len : cp_len;
    if (ocp_cw_base(&ocp.dir, c)) return -1;
    ocp_cw.g = g; ocp_cw.cp = cp; ocp_cw.cp_off = cp_off;
    if (ocp_ntab_build(&ocp.dir, c, gid, newlen, g ? OCP_SRC_GEN : OCP_SRC_COPY)) return -1;
    if (!ocp_ntab.n) {                         /* nothing left in it: not written; unnamed, the sweep takes it */
        ocp.used[c] = 0;
        if (gid < OCP_N) { ocp.dir.chunk[gid] = 0; ocp.len[gid] = 0; ocp.hash[gid] = 0; }
        ocp.gen++;
        return 0;
    }
    total = ocp_ntab_total();
    parts = ocp_parts_of(total);
    if (ocp_cw.have_base && next == live) keep = ocp_cw.base.parts;   /* (the same id again: its copy before is freed) */
    free = ocs_free_count();
    /* The scratch's chunks are copied into a slot at the first save, which needs room for them as well as the record: a
     * keep into the scratch leaves that room (music above about half the store is saved before it grows further; PROJECT
     * FULL says so). A project in a slot keeps with the plain reserve */
    if (free + keep < parts + OCJ4_RESERVE + (ocp.dir.slot == OCP_SCRATCH ? ocp_r3_parts_live() : 0u)) return 2;
    rc = ocs_write_src(OCS_POOL, ocp_rid(ocp.dir.slot, next, c), &ocp_cw.src, total);
    if (rc) return rc == -1 ? 2 : rc;
    ocp_bit_set(ocp.dir.gen, c, next);
    ocp.used[c] = (uint16_t)total;
    if (gid < OCP_N) {
        ocp.dir.chunk[gid] = newlen ? (uint8_t)c : 0u;
        if (!newlen) { ocp.len[gid] = 0; ocp.hash[gid] = 0; }
    }
    ocp.gen++;
    return 0;
}

/* how many phrases the live directory puts in chunk c */
static uint32_t ocp_r3_count(uint32_t c)
{
    uint32_t n = 0;
    for (uint32_t id = 0; id < OCP_N; id++) n += ocp.dir.chunk[id] == c;
    return n;
}

/* phrase id of the live pool is now the bytes of g (its length g->len; 0: emptied) or the cp_len bytes of cp at cp_off
 * (ocp_copy). The chunk is chosen as the top of the file says. 0 done; 2 no room; -1 a chunk that does not read;
 * another: a flash error (the pool as it was) */
static int ocp_r3_place(uint32_t id, ocp_gen_t *g, const ocs_rec_t *cp, uint32_t cp_off, uint32_t cp_len)
{
    uint32_t L = g ? g->len : cp_len, was, oldlen = 0, others = 0, need = 3u + L, target = 0, c;
    int rc;
    if (id >= OCP_N) return 1;
    was = ocp.dir.chunk[id];
    if (was) {
        oldlen = ocp_r3_len_in_chunk(id);
        others = ocp.used[was] > 3u + oldlen ? ocp.used[was] - 3u - oldlen : 1u;   /* (1: the table alone) */
    }
    if (!L) {                                  /* emptied: the directory forgets it; its bytes go stale */
        if (was) { ocp.used[was] = others > 1u ? (uint16_t)others : 0u; ocp.dir.chunk[id] = 0; }
        ocp.len[id] = 0; ocp.hash[id] = 0;
        ocp.gen++;
        return 0;
    }
    if (was && (others <= 1u || others + need <= OCP_PART)) target = was;   /* alone, or it still fits with the others */
    if (!target && 1u + need <= OCP_PART)      /* first fit among the chunks that hold something */
        for (c = 1; c < OCP_NCHUNK && !target; c++)
            if (c != was && ocp.used[c] && ocp.used[c] + need <= OCP_PART && ocp_r3_count(c) < OCP_TAB_MAX) target = c;
    for (c = 1; c < OCP_NCHUNK && !target; c++) /* a free number (a phrase larger than a part gets one of its own) */
        if (!ocp.used[c]) target = c;
    if (!target) return 2;
    if ((rc = ocp_r3_write_chunk(target, id, g, cp, cp_off, L)) != 0) return rc;
    if (was && was != target) ocp.used[was] = others > 1u ? (uint16_t)others : 0u;   /* (its old bytes there are stale) */
    if (g) {
        ocp.len[id] = (uint8_t)clamp(g->pat.timing[0], 1, NSTEP);
        ocp.hash[id] = ocp_gen_hash(g);
    } else {
        ocp.len[id] = (uint8_t)ocp_len_of(cp, cp_off, L);
        ocp.hash[id] = ocp_hash_of(cp, cp_off, L);
    }
    ocp.gen++;
    return 0;
}

/* pool phrase src copied to dst (stopped: an erase mutes the audio). 0 done; 1 src empty or unreadable; 2 no room;
 * else a flash error */
static int ocp_copy(uint32_t src, uint32_t dst)
{
    ocs_rec_t r;
    ocp_rd_t rd;
    if (src >= OCP_N || dst >= OCP_N || src == dst) return 1;
    if (ocp_r3_open(&ocp.dir, src, &r, &rd) != 1) return 1;
    return ocp_r3_place(dst, 0, &r, rd.off, rd.end - rd.off);
}

/* save track k's bank as pool phrase id (an empty bank empties it): as ocp_r3_place */
static int ocp_save(uint32_t id, uint32_t k, uint32_t bank)
{
    static ocp_gen_t g __attribute__((section(".pool")));
    if (id >= OCP_N || k >= NTRK || bank >= NPAT) return 1;
    ocp_gen_bank(&g, k, bank);
    return ocp_r3_place(id, &g, 0, 0, 0);
}
/* phrase id emptied (the directory forgets it) */
static int ocp_clear(uint32_t id) { return ocp_r3_place(id, 0, 0, 0, 0); }

/* ------------------------------------------------------------ the pool of a slot --- */
/* the live pool is slot `slot`'s, read from its record: the directory, the committed bits, each phrase's LEN and CRC,
 * each named chunk's live bytes (its table read). No record (the scratch, an empty slot): an empty pool */
static void ocp_r3_switch(uint32_t slot)
{
    uint32_t gen = ocp.gen, c;
    memset(&ocp, 0, sizeof ocp);
    ocp.gen = gen + 1u;
    ocp.dir.slot = (uint8_t)slot;
    if (slot >= OCP_SCRATCH || ocj4_phrd_read(slot, &ocp.dir, ocp.cnamed, ocp.len, ocp.hash)) {
        memset(&ocp.dir, 0, sizeof ocp.dir);
        ocp.dir.slot = (uint8_t)slot;
        memset(ocp.cnamed, 0, sizeof ocp.cnamed);
        return;
    }
    memcpy(ocp.cgen, ocp.dir.gen, sizeof ocp.cgen);
    for (c = 1; c < OCP_NCHUNK; c++) {         /* each named chunk's live bytes */
        ocs_rec_t r;
        uint32_t used = 1u, any = 0, e;
        if (!ocp_bit(ocp.cnamed, c)) continue;
        if (ocs_open(OCS_POOL, ocp_rid(slot, ocp_bit(ocp.dir.gen, c), c), &r) || ocp_tab_read(&r, &ocp_otab)) {
            for (uint32_t id = 0; id < OCP_N; id++)   /* (a chunk that does not read: its phrases are gone) */
                if (ocp.dir.chunk[id] == c) { ocp.dir.chunk[id] = 0; ocp.len[id] = 0; ocp.hash[id] = 0; }
            continue;
        }
        for (e = 0; e < ocp_otab.n; e++)
            if (ocp.dir.chunk[ocp_otab.id[e] - 1u] == c) { used += 3u + ocp_otab.len[e]; any = 1; }
        for (uint32_t id = 0; id < OCP_N; id++)   /* (a phrase the directory puts here that is not here: gone) */
            if (ocp.dir.chunk[id] == c && ocp_tab_find(&ocp_otab, id) == ocp_otab.n) { ocp.dir.chunk[id] = 0; ocp.len[id] = 0; ocp.hash[id] = 0; }
        ocp.used[c] = any ? (uint16_t)used : 0u;
    }
}

/* every chunk record of slot `slot` its record does not name erased (either generation, any number; a slot with no
 * record: all of them). One pass over the store's headers; the watchdog fed at each erase */
static ocp_dir_t ocp_sdir __attribute__((section(".pool")));
static uint8_t ocp_snamed[OCP_NCHUNK / 8u] __attribute__((section(".pool")));
static void ocp_r3_sweep(uint32_t slot)
{
    if (slot >= OCP_SCRATCH || ocj4_phrd_read(slot, &ocp_sdir, ocp_snamed, 0, 0)) {
        memset(&ocp_sdir, 0, sizeof ocp_sdir);
        memset(ocp_snamed, 0, sizeof ocp_snamed);
    }
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *h = &ocs_hdr[s];
        uint32_t c, g;
        if (h->magic != OCS_MAGIC || h->type != OCS_POOL || !(h->id & 0x8000u) || ((h->id >> 9) & 7u) != slot) continue;
        c = h->id & 255u; g = (h->id >> 8) & 1u;
        if (c && ocp_bit(ocp_snamed, c) && ocp_bit(ocp_sdir.gen, c) == g) continue;
        (void)ocs_delete(OCS_POOL, h->id);     /* (every copy of that id; the pass goes on over cleared headers) */
        OC_FEED();
    }
}

/* boot (after ocs_boot): every slot's orphan chunks swept, the scratch's all; the live pool empty until a project loads */
static void ocp_r3_boot(void)
{
    for (uint32_t slot = 0; slot <= OCP_SCRATCH; slot++) ocp_r3_sweep(slot);
    ocp_r3_switch(OCP_SCRATCH);
}

/* the parts (sectors) the live pool's chunks take */
static uint32_t ocp_r3_parts_live(void);
static uint32_t ocp_r3_parts_live(void)
{
    uint32_t n = 0;
    for (uint32_t c = 1; c < OCP_NCHUNK; c++) if (ocp.used[c]) n += ocp_parts_of(ocp.used[c]);
    return n;
}
/* the store's free room in KB (the project page's meter, the PROJECT FULL message) */
static uint32_t ocp_r3_free_kb(void) { return ocs_free_count() * OCS_PAYLOAD_MAX / 1024u; }

/* every live chunk copied into slot dst (a save into another slot): each under the generation opposite to the one
 * dst's record names for that number (either when it names none), its stale bytes dropped. out: dst's directory as
 * written (the record writer names it). Nothing of the live pool changes. 0 done; 2 no room (checked whole before a
 * byte is written: the chunks' parts plus `more`, the record's); -1 a chunk that does not read; else a flash error */
static int ocp_r3_copy_slot(uint32_t dst, ocp_dir_t *out, uint32_t more)
{
    uint32_t c, rc;
    if (ocj4_phrd_read(dst, &ocp_sdir, ocp_snamed, 0, 0)) {
        memset(&ocp_sdir, 0, sizeof ocp_sdir);
        memset(ocp_snamed, 0, sizeof ocp_snamed);
    }
    if (ocs_free_count() < ocp_r3_parts_live() + more) return 2;
    *out = ocp.dir;
    out->slot = (uint8_t)dst;
    memset(out->gen, 0, sizeof out->gen);
    for (c = 1; c < OCP_NCHUNK; c++) {
        uint32_t g;
        if (!ocp.used[c]) continue;
        if (ocp_cw_base(&ocp.dir, c) || !ocp_cw.have_base) return -1;
        if (ocp_ntab_build(&ocp.dir, c, OCP_N, 0, 0) || !ocp_ntab.n) return -1;
        g = ocp_bit(ocp_snamed, c) ? !ocp_bit(ocp_sdir.gen, c) : 0u;
        rc = (uint32_t)ocs_write_src(OCS_POOL, ocp_rid(dst, g, c), &ocp_cw.src, ocp_ntab_total());
        if (rc) return (int)rc == -1 ? 2 : (int)rc;
        ocp_bit_set(out->gen, c, g);
        OC_FEED();
    }
    return 0;
}
/* the live pool is now directory d's (after ocp_r3_copy_slot into the scratch: music that lost its slot keeps its
 * phrases there; nothing committed) */
static void ocp_r3_adopt(const ocp_dir_t *d)
{
    ocp.dir = *d;
    memset(ocp.cgen, 0, sizeof ocp.cgen);
    memset(ocp.cnamed, 0, sizeof ocp.cnamed);
    ocp.gen++;
}
