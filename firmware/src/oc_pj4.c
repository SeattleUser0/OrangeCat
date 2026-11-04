/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat Release 3 (step 7, PROJECT_FORMAT_R3.md 3, 4, 9, 10): the project format core. Included after oc_store.c (it uses the store's records, sectors and
 * streams); host only in this step: nothing in the image includes it yet (OC_PJ4, core.h).
 *
 * What it does
 *   A project is a record of type OCS_PROJECT with id 0x8000 | slot ("OCJ4": a header and tagged sections, 4) and the phrases it owns, packed into chunk
 *   records of type OCS_POOL with ids 0x8000 | slot << 9 | generation << 8 | chunk (3.1). A chunk is as many phrases as fill one sector part (3,808 B), in
 *   phrase order, each as id (1 B), length (2 B), bytes. A phrase larger than a part has a chunk of its own with as many parts as it needs. The ids have bit 15
 *   set so the Release 2 firmware, which only asks for ids 0..15 and 0..3, never reads them and never erases them (its store keeps them as current copies).
 *   The record names, in its PHRD section, each phrase's chunk, its CRC-32 and a bit a chunk: the generation the record reads.
 *
 * The save protocol (3.1, 13 items 1 and 2)
 *   Chunks first, the record last. A chunk whose members (phrase ids and CRCs) differ from the chunk of the same number in the record now in charge is written under
 *   the OTHER generation of its id; an unchanged chunk is left where it is. Then the record, which names the new generation bits, is written: that is the commit.
 *   Until it commits the old record reads the old chunks, untouched. After it, the sweep erases every chunk record no current project record names. A cut before the
 *   record leaves chunks of the generation the record does not name: the sweep (pj4_boot, and straight after any failed save) erases them.
 *
 * The conversion (9)
 *   pj4_convert makes a slot's new project from its old one: each phrase the old slot names (a 64 bit mask, oc_project.c's ocj.names), read from the old pool, repacked
 *   (macro motion events of the old versions dropped, the version byte set, 8.1), written as the slot's own chunks under generation 0, then the new record, which
 *   carries the old record's bytes as one section (OJ3B) until the loader reads the sections itself (step 2), then the old record is erased. pj4_boot finishes a cut
 *   conversion (the new record wins), converts every slot that still has only an old record, and erases the old pool chunks no remaining old record names.
 *
 * RAM: one directory at a time and a few small tables in .pool (about 5 KB); the work area the conversion needs (PJ4_WORK, 26 KB) is the caller's: the boot's own
 * 27,904 B buffer. Everything is little-endian. Without MELODEE_FLASH it runs on the store's RAM sectors, as the store does.
 *
 * Revisions:
 *   Release 3 step 7 (r3-format, step 1): first version.
 */
#define PJ4_MAGIC 0x344A434Fu                  /* "OCJ4" */
#define PJ4_VER 1u
#define PJ4_SLOTS 4u
#define PJ4_NPH 255u                           /* phrases a project: ids 1..255, 0 means none */
#define PJ4_NONE 0xFFu                         /* a directory entry with no chunk */
#define PJ4_PHMAX 11223u                       /* the largest phrase: oc_pool.c's OCP_MAX */
#define PJ4_PHVER 3u                           /* the phrase version the revision writes: a flagged control note (8.2b) is allowed in it, motion may name the macro ids (8.1) */
#define PJ4_RESERVE 3u                         /* sectors a save leaves free (3.1: a record of three parts) */
#define PJ4_RECMAX (4u * OCS_PAYLOAD_MAX)      /* a record: four parts at most (the budget of 3.1) */
#define OCP_CHUNKS_PJ4 16u                     /* the old pool: sixteen chunks of four (oc_pool.c) */
#define PJ4_NSEC 24u                           /* sections a record holds, the core's two included */
#define PJ4_HEAD_LEN 12u                       /* magic u32, version u16, section count u16, whole length u32 */
#define PJ4_HSEC_LEN 52u
#define PJ4_DIR_LEN (PJ4_NPH * 7u + 32u)       /* 1,785 B of entries and the 32 B of generation bits */
#define PJ4_WORK (PJ4_PHMAX + 3u + PJ4_RECMAX + PJ4_PHMAX + 3u)   /* the conversion's: a pattern as read, as repacked, and the old record */
#define PJ4_TAG(a, b, c, d) ((uint32_t)(a) | (uint32_t)(b) << 8 | (uint32_t)(c) << 16 | (uint32_t)(d) << 24)
#define PJ4_T_HEAD PJ4_TAG('H', 'E', 'A', 'D')
#define PJ4_T_PHRD PJ4_TAG('P', 'H', 'R', 'D')
#define PJ4_T_OJ3B PJ4_TAG('O', 'J', '3', 'B')  /* the old (OCJ3) record's bytes, carried whole by the conversion until the loader splits them (step 2) */
#define PJ4_FULL (-20)                         /* PROJECT FULL: nothing written */
#define PJ4_BAD (-21)                          /* a request the core refuses (order, sizes, tags) */
#define PJ4_SRC (-11)                          /* the phrase source changed between the list and the write */

typedef struct { uint8_t id; uint16_t len; uint32_t crc; } pj4_phr_t;           /* one phrase: its id, bytes, CRC-32 (ocs_crc32) */
typedef int (*pj4_get_t)(void *ctx, uint32_t id, uint8_t *dst, uint32_t off, uint32_t n);   /* n bytes of phrase id from off; 0 done */
typedef struct { uint32_t tag, len; const uint8_t *p; } pj4_sec_t;
typedef struct {
    char name[12];
    uint8_t parts, sel;
    uint32_t seed, uid, rev;                   /* rev: bumped by every save (set by pj4_save) */
    char prof[16];
    uint32_t prof_id, next_id;
} pj4_head_t;
typedef struct { uint8_t chunk[256]; uint32_t crc[256]; uint16_t rsv[256]; uint8_t gen[32]; } pj4_dir_t;   /* by phrase id (entry 0 unused) */

static uint32_t pj4_g16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t pj4_g32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void pj4_p16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void pj4_p32(uint8_t *p, uint32_t v) { pj4_p16(p, v); pj4_p16(p + 2, v >> 16); }
static uint32_t pj4_rid(uint32_t slot) { return 0x8000u | slot; }
static uint32_t pj4_cid(uint32_t slot, uint32_t gen, uint32_t n) { return 0x8000u | slot << 9 | gen << 8 | n; }
static int pj4_mine(uint32_t id) { return (id & 0xF800u) == 0x8000u; }          /* bit 15 and bits 11..14 clear: ours (a later release may use the others) */

static pj4_dir_t pj4_d0 __attribute__((section(".pool")));                      /* the directory in charge */
static pj4_dir_t pj4_d1 __attribute__((section(".pool")));                      /* the one being made */
static uint8_t pj4_scr[PJ4_HSEC_LEN + PJ4_DIR_LEN] __attribute__((section(".pool")));   /* HEAD and PHRD as written */

static void pj4_dir_clear(pj4_dir_t *d) { memset(d, 0, sizeof *d); memset(d->chunk, PJ4_NONE, sizeof d->chunk); }
static int pj4_gen(const pj4_dir_t *d, uint32_t n) { return (d->gen[n >> 3] >> (n & 7u)) & 1u; }
static void pj4_gen_set(pj4_dir_t *d, uint32_t n, int g) { if (g) d->gen[n >> 3] |= (uint8_t)(1u << (n & 7u)); else d->gen[n >> 3] &= (uint8_t)~(1u << (n & 7u)); }
static void pj4_dir_enc(const pj4_dir_t *d, uint8_t *o)
{
    for (uint32_t i = 1; i <= PJ4_NPH; i++, o += 7) { o[0] = d->chunk[i]; pj4_p32(o + 1, d->crc[i]); pj4_p16(o + 5, d->rsv[i]); }
    memcpy(o, d->gen, 32);
}
static void pj4_dir_dec(pj4_dir_t *d, const uint8_t *in)
{
    pj4_dir_clear(d);
    for (uint32_t i = 1; i <= PJ4_NPH; i++, in += 7) { d->chunk[i] = in[0]; d->crc[i] = pj4_g32(in + 1); d->rsv[i] = (uint16_t)pj4_g16(in + 5); }
    memcpy(d->gen, in, 32);
}

/* ---------------------------------------------------------- the record --- */
/* section `tag` of the opened record r: its offset and length. 0 found; -1 not (or the record is not an OCJ4 one) */
static int pj4_sec_at(const ocs_rec_t *r, uint32_t tag, uint32_t *off, uint32_t *len)
{
    uint8_t h[PJ4_HEAD_LEN], s[8];
    uint32_t pos = PJ4_HEAD_LEN, n, i;
    if (r->total < PJ4_HEAD_LEN || ocs_at(r, 0, h, sizeof h) || pj4_g32(h) != PJ4_MAGIC || pj4_g16(h + 4) != PJ4_VER || pj4_g32(h + 8) != r->total) return -1;
    n = pj4_g16(h + 6);
    for (i = 0; i < n; i++) {
        uint32_t t, l;
        if (r->total - pos < 8u || ocs_at(r, pos, s, 8)) return -1;
        t = pj4_g32(s); l = pj4_g32(s + 4);
        if (l > r->total - pos - 8u) return -1;
        if (t == tag) { *off = pos + 8u; *len = l; return 0; }
        pos += 8u + l;
    }
    return -1;
}
static int pj4_open(uint32_t slot, ocs_rec_t *r) { return ocs_open(OCS_PROJECT, pj4_rid(slot), r); }

/* the slot's directory in charge into d (cleared when there is none). 0 a record is there and reads; -1 none */
static int pj4_dir_load(uint32_t slot, pj4_dir_t *d)
{
    ocs_rec_t r;
    uint32_t off, len;
    pj4_dir_clear(d);
    if (pj4_open(slot, &r) || pj4_sec_at(&r, PJ4_T_PHRD, &off, &len) || len != PJ4_DIR_LEN || ocs_at(&r, off, pj4_scr, PJ4_DIR_LEN)) return -1;
    pj4_dir_dec(d, pj4_scr);
    return 0;
}
static void pj4_head_enc(const pj4_head_t *h, uint8_t *o)
{
    memcpy(o, h->name, 12); o[12] = h->parts; o[13] = h->sel; pj4_p16(o + 14, 0);
    pj4_p32(o + 16, h->seed); pj4_p32(o + 20, h->uid); pj4_p32(o + 24, h->rev);
    memcpy(o + 28, h->prof, 16); pj4_p32(o + 44, h->prof_id); pj4_p32(o + 48, h->next_id);
}
static void pj4_head_dec(pj4_head_t *h, const uint8_t *o)
{
    memcpy(h->name, o, 12); h->parts = o[12]; h->sel = o[13];
    h->seed = pj4_g32(o + 16); h->uid = pj4_g32(o + 20); h->rev = pj4_g32(o + 24);
    memcpy(h->prof, o + 28, 16); h->prof_id = pj4_g32(o + 44); h->next_id = pj4_g32(o + 48);
}
/* the slot's HEAD. 0; -1 no record */
static int pj4_head_load(uint32_t slot, pj4_head_t *h)
{
    ocs_rec_t r;
    uint32_t off, len;
    uint8_t b[PJ4_HSEC_LEN];
    if (pj4_open(slot, &r) || pj4_sec_at(&r, PJ4_T_HEAD, &off, &len) || len != PJ4_HSEC_LEN || ocs_at(&r, off, b, sizeof b)) return -1;
    pj4_head_dec(h, b);
    return 0;
}
/* section `tag` of the slot's record: its length, bytes into dst (max); -1 none or too long */
static int pj4_sec_read(uint32_t slot, uint32_t tag, uint8_t *dst, uint32_t max)
{
    ocs_rec_t r;
    uint32_t off, len;
    if (pj4_open(slot, &r) || pj4_sec_at(&r, tag, &off, &len) || len > max || ocs_at(&r, off, dst, len)) return -1;
    return (int)len;
}
/* the sections of the slot's record that the caller does not know (its tags are not in known[]), copied into buf (max) and appended to secs[*n] (cap): a save
 * writes them back byte for byte (4: unknown tags are kept). The core's own two are never copied. 0; -1 buf or secs full or the record unreadable */
static int pj4_carry(uint32_t slot, const uint32_t *known, uint32_t nk, uint8_t *buf, uint32_t max, pj4_sec_t *secs, uint32_t *n, uint32_t cap)
{
    ocs_rec_t r;
    uint8_t h[PJ4_HEAD_LEN], s[8];
    uint32_t pos = PJ4_HEAD_LEN, i, k, used = 0, cnt;
    if (pj4_open(slot, &r) || r.total < PJ4_HEAD_LEN || ocs_at(&r, 0, h, sizeof h) || pj4_g32(h) != PJ4_MAGIC) return -1;
    cnt = pj4_g16(h + 6);
    for (i = 0; i < cnt; i++) {
        uint32_t t, l, skip;
        if (r.total - pos < 8u || ocs_at(&r, pos, s, 8)) return -1;
        t = pj4_g32(s); l = pj4_g32(s + 4);
        if (l > r.total - pos - 8u) return -1;
        skip = t == PJ4_T_HEAD || t == PJ4_T_PHRD;
        for (k = 0; k < nk; k++) skip |= known[k] == t;
        if (!skip) {
            if (*n >= cap || l > max - used || ocs_at(&r, pos + 8u, buf + used, l)) return -1;
            secs[*n].tag = t; secs[*n].len = l; secs[*n].p = buf + used; (*n)++;
            used += l;
        }
        pos += 8u + l;
    }
    return 0;
}

/* ---------------------------------------------------------- the chunks --- */
typedef struct {
    uint8_t of[PJ4_NPH + 1u];                  /* the chunk of phrase index i */
    uint16_t first[PJ4_NPH + 1u];              /* chunk n's first phrase index */
    uint8_t cnt[PJ4_NPH + 1u];                 /* .. and how many */
    uint16_t bytes[PJ4_NPH + 1u];              /* .. its payload bytes */
    uint32_t n;                                /* chunks */
} pj4_plan_t;
static pj4_plan_t pj4_pl __attribute__((section(".pool")));

/* phrases in ascending id order into chunks, as many as fill a part, a larger one alone */
static void pj4_plan(const pj4_phr_t *ph, uint32_t nph)
{
    uint32_t i, cur = 0, open = 0;
    pj4_pl.n = 0;
    for (i = 0; i < nph; i++) {
        uint32_t e = 3u + ph[i].len;
        if (open && (cur + e > OCS_PAYLOAD_MAX || cur > OCS_PAYLOAD_MAX)) open = 0;   /* (a chunk that has grown past one part holds a single large phrase) */
        if (!open) {
            pj4_pl.first[pj4_pl.n] = (uint16_t)i; pj4_pl.cnt[pj4_pl.n] = 0; pj4_pl.bytes[pj4_pl.n] = 0;
            cur = 0; open = 1; pj4_pl.n++;
        }
        pj4_pl.of[i] = (uint8_t)(pj4_pl.n - 1u);
        pj4_pl.cnt[pj4_pl.n - 1u]++;
        cur += e;
        pj4_pl.bytes[pj4_pl.n - 1u] = (uint16_t)cur;
        if (e > OCS_PAYLOAD_MAX) open = 0;
    }
}
static uint32_t pj4_parts(uint32_t bytes) { return (bytes + OCS_PAYLOAD_MAX - 1u) / OCS_PAYLOAD_MAX; }

/* the chunk n of directory d holds exactly ph[first .. first + cnt) with their CRCs, and its record is on the flash whole at the length expected */
static int pj4_chunk_same(uint32_t slot, const pj4_dir_t *d, uint32_t n, const pj4_phr_t *ph)
{
    uint32_t i, k = 0, id, want = 0;
    ocs_rec_t r;
    for (id = 1; id <= PJ4_NPH; id++) {
        if (d->chunk[id] != n) continue;
        if (k >= pj4_pl.cnt[n]) return 0;
        const pj4_phr_t *p = &ph[pj4_pl.first[n] + k];
        if (p->id != id || p->crc != d->crc[id]) return 0;
        want += 3u + p->len; k++;
    }
    if (k != pj4_pl.cnt[n]) return 0;
    (void)i;
    return !ocs_open(OCS_POOL, pj4_cid(slot, pj4_gen(d, n), n), &r) && r.total == want;
}

typedef struct {
    ocs_src_t src;
    const pj4_phr_t *ph;
    uint32_t i, end, pos, crc;                 /* the entry, the end of the chunk's, how far into the entry, the phrase's CRC so far */
    pj4_get_t get;
    void *ctx;
} pj4_csrc_t;
static int pj4_cget(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    pj4_csrc_t *c = (pj4_csrc_t *)s;
    while (n) {
        const pj4_phr_t *p;
        uint32_t k;
        if (c->i >= c->end) return PJ4_SRC;
        p = &c->ph[c->i];
        if (c->pos < 3u) {
            uint8_t hd[3];
            hd[0] = p->id; pj4_p16(hd + 1, p->len);
            k = 3u - c->pos; if (k > n) k = n;
            memcpy(dst, hd + c->pos, k);
            if (c->pos == 0) c->crc = 0xFFFFFFFFu;
        } else {
            uint32_t o = c->pos - 3u;
            k = p->len - o; if (k > n) k = n;
            if (c->get(c->ctx, p->id, dst, o, k)) return PJ4_SRC;
            c->crc = ocs_crc_add(c->crc, dst, k);
        }
        dst += k; n -= k; c->pos += k;
        if (c->pos == 3u + p->len) {
            if (~c->crc != p->crc) return PJ4_SRC;
            c->i++; c->pos = 0;
        }
    }
    return 0;
}

/* a gathered record: header, HEAD, PHRD and the caller's sections, as segments */
typedef struct { const uint8_t *p; uint32_t n; } pj4_seg_t;
typedef struct { ocs_src_t src; pj4_seg_t seg[2u * PJ4_NSEC + 2u]; uint32_t nseg, i, o; } pj4_rsrc_t;
static int pj4_rget(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    pj4_rsrc_t *g = (pj4_rsrc_t *)s;
    while (n) {
        uint32_t k;
        while (g->i < g->nseg && g->o >= g->seg[g->i].n) { g->i++; g->o = 0; }
        if (g->i >= g->nseg) return -3;
        k = g->seg[g->i].n - g->o; if (k > n) k = n;
        memcpy(dst, g->seg[g->i].p + g->o, k);
        dst += k; n -= k; g->o += k;
    }
    return 0;
}
static pj4_rsrc_t pj4_rs __attribute__((section(".pool")));
static uint8_t pj4_sh[PJ4_NSEC][8] __attribute__((section(".pool")));
static uint8_t pj4_hd[PJ4_HEAD_LEN] __attribute__((section(".pool")));

/* ---------------------------------------------------------- the sweep ---- */
/* erase every chunk record no current project record names (3.1): the generation a record does not name, a chunk number it does not use, every chunk of a slot with
 * no record. Records whose id is not ours (bit 15 set, bits 11..14 clear) are left alone. Returns the sectors erased */
static uint32_t pj4_sweep(void)
{
    uint32_t slot, s, n = 0;
    for (slot = 0; slot < PJ4_SLOTS; slot++) {
        uint8_t used[PJ4_NPH + 1u];
        uint32_t i;
        int have = pj4_dir_load(slot, &pj4_d0) == 0;
        memset(used, 0, sizeof used);
        for (i = 1; i <= PJ4_NPH; i++) if (pj4_d0.chunk[i] != PJ4_NONE) used[pj4_d0.chunk[i]] = 1;
        for (s = 0; s < OCS_NSECT; s++) {
            uint32_t id = ocs_hdr[s].id, c = id & 0xFFu;
            if (ocs_hdr[s].magic != OCS_MAGIC || ocs_hdr[s].type != OCS_POOL || !pj4_mine(id) || ((id >> 9) & 3u) != slot) continue;
            if (have && used[c] && pj4_gen(&pj4_d0, c) == (int)((id >> 8) & 1u)) continue;
            ocs_delete(OCS_POOL, id);
            n++;
        }
    }
    return n;
}

/* ------------------------------------------------------------- the save --- */
/* save slot's project: head h (its rev is set: the record in charge's + 1, else 1), the caller's sections (tags other than HEAD and PHRD, each at most once), the
 * phrases ph[0..nph) (ascending ids 1..255, length 1..PJ4_PHMAX, CRC-32 of their bytes) read through get. 0 saved. PJ4_FULL: nothing written, *over the bytes
 * short (3.2); PJ4_BAD a request that is not valid; PJ4_SRC the phrases changed under the save; another negative a flash error. A failure leaves the record in charge
 * as it was and sweeps the chunks it wrote */
static int pj4_save(uint32_t slot, pj4_head_t *h, const pj4_sec_t *secs, uint32_t nsec, const pj4_phr_t *ph, uint32_t nph, pj4_get_t get, void *ctx, uint32_t *over)
{
    pj4_head_t old;
    uint32_t i, n, need = PJ4_RESERVE, reclen, free_n;
    uint8_t chg[PJ4_NPH + 1u];
    int rc, had;
    if (slot >= PJ4_SLOTS || nph > PJ4_NPH || nsec + 2u > PJ4_NSEC) return PJ4_BAD;
    for (i = 0; i < nph; i++)
        if (!ph[i].id || (i && ph[i].id <= ph[i - 1u].id) || !ph[i].len || ph[i].len > PJ4_PHMAX) return PJ4_BAD;
    for (i = 0; i < nsec; i++) {
        if (secs[i].tag == PJ4_T_HEAD || secs[i].tag == PJ4_T_PHRD || (secs[i].len && !secs[i].p)) return PJ4_BAD;
        for (n = 0; n < i; n++) if (secs[n].tag == secs[i].tag) return PJ4_BAD;
    }
    had = pj4_dir_load(slot, &pj4_d0) == 0;
    h->rev = had && pj4_head_load(slot, &old) == 0 ? old.rev + 1u : 1u;
    pj4_plan(ph, nph);
    /* which chunks are written, under which generation; the directory the new record carries */
    pj4_dir_clear(&pj4_d1);
    for (n = 0; n < pj4_pl.n; n++) {
        chg[n] = had ? !pj4_chunk_same(slot, &pj4_d0, n, ph) : 1;
        pj4_gen_set(&pj4_d1, n, had ? pj4_gen(&pj4_d0, n) ^ chg[n] : 0);
        if (chg[n]) need += pj4_parts(pj4_pl.bytes[n]);
    }
    for (i = 0; i < nph; i++) { pj4_d1.chunk[ph[i].id] = pj4_pl.of[i]; pj4_d1.crc[ph[i].id] = ph[i].crc; if (had) pj4_d1.rsv[ph[i].id] = pj4_d0.rsv[ph[i].id]; }
    reclen = PJ4_HEAD_LEN + 8u + PJ4_HSEC_LEN + 8u + PJ4_DIR_LEN;
    for (i = 0; i < nsec; i++) reclen += 8u + secs[i].len;
    if (reclen > PJ4_RECMAX) { if (over) *over = reclen - PJ4_RECMAX; return PJ4_FULL; }
    need += pj4_parts(reclen);
    free_n = ocs_free_count();
    if (need > free_n) { if (over) *over = (need - free_n) * OCS_PAYLOAD_MAX; return PJ4_FULL; }
    /* the chunks, under the generation the record does not read */
    for (n = 0; n < pj4_pl.n; n++) {
        pj4_csrc_t c;
        uint32_t k;
        if (!chg[n]) continue;
        c.src.get = pj4_cget; c.ph = ph; c.i = pj4_pl.first[n]; c.end = c.i + pj4_pl.cnt[n]; c.pos = 0; c.crc = 0; c.get = get; c.ctx = ctx;
        k = pj4_pl.bytes[n];
        if ((rc = ocs_write_src(OCS_POOL, pj4_cid(slot, pj4_gen(&pj4_d1, n), n), &c.src, k)) != 0) { pj4_sweep(); return rc; }
    }
    /* the record, last: the commit */
    pj4_head_enc(h, pj4_scr);
    pj4_dir_enc(&pj4_d1, pj4_scr + PJ4_HSEC_LEN);
    pj4_p32(pj4_hd, PJ4_MAGIC); pj4_p16(pj4_hd + 4, PJ4_VER); pj4_p16(pj4_hd + 6, nsec + 2u); pj4_p32(pj4_hd + 8, reclen);
    pj4_rs.src.get = pj4_rget; pj4_rs.nseg = 0; pj4_rs.i = 0; pj4_rs.o = 0;
    pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_hd, PJ4_HEAD_LEN};
    pj4_p32(pj4_sh[0], PJ4_T_HEAD); pj4_p32(pj4_sh[0] + 4, PJ4_HSEC_LEN);
    pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_sh[0], 8};
    pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_scr, PJ4_HSEC_LEN};
    pj4_p32(pj4_sh[1], PJ4_T_PHRD); pj4_p32(pj4_sh[1] + 4, PJ4_DIR_LEN);
    pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_sh[1], 8};
    pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_scr + PJ4_HSEC_LEN, PJ4_DIR_LEN};
    for (i = 0; i < nsec; i++) {
        pj4_p32(pj4_sh[2u + i], secs[i].tag); pj4_p32(pj4_sh[2u + i] + 4, secs[i].len);
        pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){pj4_sh[2u + i], 8};
        if (secs[i].len) pj4_rs.seg[pj4_rs.nseg++] = (pj4_seg_t){secs[i].p, secs[i].len};
    }
    if ((rc = ocs_write_src(OCS_PROJECT, pj4_rid(slot), &pj4_rs.src, reclen)) != 0) { pj4_sweep(); return rc; }
    pj4_sweep();                               /* (the old generation of every chunk rewritten, and the chunks the record no longer uses) */
    return 0;
}

/* phrase id of the slot, whole into dst (max): its length; -1 none, too long, or its bytes do not match the directory's CRC */
static int pj4_phrase_read(uint32_t slot, uint32_t id, uint8_t *dst, uint32_t max)
{
    ocs_rec_t r;
    uint32_t n, pos = 0;
    if (!id || id > PJ4_NPH || pj4_dir_load(slot, &pj4_d0)) return -1;
    n = pj4_d0.chunk[id];
    if (n == PJ4_NONE || ocs_open(OCS_POOL, pj4_cid(slot, pj4_gen(&pj4_d0, n), n), &r)) return -1;
    while (pos + 3u <= r.total) {
        uint8_t e[3];
        uint32_t len;
        if (ocs_at(&r, pos, e, 3)) return -1;
        len = pj4_g16(e + 1);
        if (len > r.total - pos - 3u) return -1;
        if (e[0] == id) {
            if (len > max || ocs_at(&r, pos + 3u, dst, len) || ocs_crc32(dst, len) != pj4_d0.crc[id]) return -1;
            return (int)len;
        }
        pos += 3u + len;
    }
    return -1;
}

/* erase the slot's project and every chunk of it (a run time restore replaces a slot whole: 9). The record goes first; a cut after leaves chunks the sweep erases */
static int pj4_slot_clear(uint32_t slot)
{
    int rc = ocs_delete(OCS_PROJECT, pj4_rid(slot));
    pj4_sweep();
    return rc;
}

/* ------------------------------------------------------- the conversion --- */
/* a pattern of the old pool (OCP_VER 1 or 2) repacked as the revision writes it: the motion events on the macro ids 33..36 dropped from the old versions (8.1; they
 * were inert there: motion_param excluded the ids), the version byte PJ4_PHVER. Every other byte equal. The new length; -1 a pattern that does not parse */
static int pj4_repack(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t i, ns = 0, p, mc, q, nn, kept = 0, w;
    if (n < 20u || (in[0] != 1u && in[0] != 2u && in[0] != PJ4_PHVER)) return -1;
    for (i = 0; i < 8; i++) { uint32_t b = in[12u + i]; while (b) { ns += b & 1u; b >>= 1; } }
    p = 20u + 11u * ns;
    if (p + 1u > n) return -1;
    mc = in[p];
    q = p + 1u + 4u * mc;
    if (mc > 64u || q + 2u > n) return -1;
    nn = pj4_g16(in + q);
    if (q + 2u + 10u * nn != n) return -1;
    for (i = 0; i < nn; i++) {                 /* (8.2b) the control flag, bit 1 of the note's length byte, only in a phrase of the revision and only on a key's number */
        const uint8_t *r = in + q + 2u + 10u * i;
        if (r[9] > 1u && (in[0] < PJ4_PHVER || r[9] != 2u || r[8] || r[4] < 124u || r[4] > 127u || !r[5])) return -1;
    }
    memcpy(out, in, p);
    out[0] = (uint8_t)PJ4_PHVER;
    w = p + 1u;
    for (i = 0; i < mc; i++) {
        const uint8_t *ev = in + p + 1u + 4u * i;
        if (in[0] < PJ4_PHVER && ev[1] >= 33u && ev[1] <= 36u) continue;
        memcpy(out + w, ev, 4); w += 4u; kept++;
    }
    out[p] = (uint8_t)kept;
    memcpy(out + w, in + q, 2u + 10u * nn);
    return (int)(w + 2u + 10u * nn);
}

/* old pattern idx (0..63) of the old pool as its bytes (the old chunk: four 16 bit lengths, then the patterns). Length; 0 an empty pattern; -1 not readable */
static int pj4_old_phrase(uint32_t idx, uint8_t *dst, uint32_t max)
{
    ocs_rec_t r;
    uint8_t t[8];
    uint32_t off = 8u, k, len;
    if (ocs_open(OCS_POOL, idx / 4u, &r) || r.total < 8u || ocs_at(&r, 0, t, 8)) return -1;
    for (k = 0; k < idx % 4u; k++) off += pj4_g16(t + 2u * k);
    len = pj4_g16(t + 2u * (idx % 4u));
    if (!len) return 0;
    if (len > max || off + len > r.total || ocs_at(&r, off, dst, len)) return -1;
    return (int)len;
}

typedef struct { uint64_t names; uint8_t *raw, *out; uint32_t cached; } pj4_cv_t;
/* the converted phrase id (idx + 1) repacked into cv->out; its length */
static int pj4_cv_load(pj4_cv_t *cv, uint32_t id)
{
    int n = pj4_old_phrase(id - 1u, cv->raw, PJ4_PHMAX);
    if (n <= 0) return -1;
    n = pj4_repack(cv->raw, (uint32_t)n, cv->out);
    cv->cached = n > 0 ? id : 0;
    return n;
}
static int pj4_cv_get(void *ctx, uint32_t id, uint8_t *dst, uint32_t off, uint32_t n)
{
    pj4_cv_t *cv = ctx;
    int len;
    if (cv->cached != id && (len = pj4_cv_load(cv, id)) <= 0) return -1;
    memcpy(dst, cv->out + off, n);
    return 0;
}

/* convert slot's old project (the old record of id slot in the store, the patterns names says it uses) to the revision's format. work: PJ4_WORK bytes. h: the head
 * the new record starts with (name, seed, uid ..: the caller's; the project layer makes them). crcmap, when not null, is filled with the new CRC of phrase id
 * (index id) so that the freeze table's hashes can be recomputed (9 step 4). 0 converted (the old record erased); PJ4_FULL with *over; -1 no old record; else
 * the save's. Idempotent: a slot that already has a new record only has its old one erased */
static int pj4_convert(uint32_t slot, uint64_t names, uint8_t *work, pj4_head_t *h, uint32_t *over, uint32_t *crcmap)
{
    pj4_phr_t ph[PJ4_NPH];
    pj4_cv_t cv;
    pj4_sec_t sec;
    uint32_t nph = 0, id;
    int rc, rl;
    if (pj4_dir_load(slot, &pj4_d0) == 0) return ocs_delete(OCS_PROJECT, slot);   /* (a cut after the commit: finish step 5) */
    cv.names = names; cv.raw = work; cv.out = work + PJ4_PHMAX + 3u; cv.cached = 0;
    rl = ocs_read(OCS_PROJECT, slot, work + 2u * (PJ4_PHMAX + 3u), PJ4_RECMAX);
    if (rl < 0) return -1;
    for (id = 1; id <= 64u; id++) {
        int len;
        if (!((names >> (id - 1u)) & 1u)) continue;
        if ((len = pj4_cv_load(&cv, id)) <= 0) continue;                         /* (empty, or unreadable: the new project has no such phrase) */
        ph[nph].id = (uint8_t)id; ph[nph].len = (uint16_t)len; ph[nph].crc = ocs_crc32(cv.out, (uint32_t)len);
        if (crcmap) crcmap[id] = ph[nph].crc;
        nph++;
    }
    sec.tag = PJ4_T_OJ3B; sec.len = (uint32_t)rl; sec.p = work + 2u * (PJ4_PHMAX + 3u);
    if ((rc = pj4_save(slot, h, &sec, 1, ph, nph, pj4_cv_get, &cv, over)) != 0) return rc;
    return ocs_delete(OCS_PROJECT, slot);                                        /* (step 5) */
}

/* the old pool chunks that no remaining old project record names are erased (9 step 5). names[slot]: the patterns each old record uses */
static void pj4_old_pool_trim(const uint64_t *names)
{
    for (uint32_t k = 0; k < OCP_CHUNKS_PJ4; k++) {
        uint32_t slot, keep = 0;
        for (slot = 0; slot < PJ4_SLOTS; slot++)
            if (ocs_exists(OCS_PROJECT, slot) && ((names[slot] >> (4u * k)) & 0xFu)) keep = 1;
        if (!keep && ocs_exists(OCS_POOL, k)) ocs_delete(OCS_POOL, k);
    }
}

/* the boot's step (9): the sweep, then each slot: a new record and an old one (a cut conversion) -> the old one erased; an old one only -> converted; nothing -> as is.
 * status[slot]: 0 nothing, 1 converted (or already), 2 OLD FORMAT (does not fit; *over[slot] the bytes short), 3 failed otherwise. mk(slot, head) makes the head of a
 * slot's new project (name, seed, uid ..) */
static void pj4_boot(const uint64_t *names, uint8_t *work, void (*mk)(uint32_t slot, pj4_head_t *h), uint8_t *status, uint32_t *over)
{
    uint32_t slot;
    pj4_sweep();
    for (slot = 0; slot < PJ4_SLOTS; slot++) {
        pj4_head_t h;
        int rc;
        status[slot] = 0; over[slot] = 0;
        if (!ocs_exists(OCS_PROJECT, slot)) { status[slot] = pj4_dir_load(slot, &pj4_d0) == 0; continue; }
        memset(&h, 0, sizeof h);
        mk(slot, &h);
        rc = pj4_convert(slot, names[slot], work, &h, &over[slot], 0);
        status[slot] = rc == 0 ? 1 : rc == PJ4_FULL ? 2 : 3;
    }
    pj4_old_pool_trim(names);
}

