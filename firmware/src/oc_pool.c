/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the pattern pool in flash (build doc phase 4a, part 3; Skiff's bank.c is the model).
 *
 * Sixty-four patterns, ids 0..63, shared by every project (as Skiff). They are kept in the record store (oc_store.c)
 * as sixteen chunks of four (record OCS_POOL, id = the chunk), so a small pattern does not take a sector of its own.
 * A track plays a pool pattern from one of its eight Melodee banks: loading (ocp_load) unpacks the pattern into a
 * bank, saving (ocp_save) packs a bank into the pool. The banks are the cache; the pool is what is kept.
 *
 * A pattern is everything Melodee keeps with a bank:
 *   0          version (OCP_VER)
 *   1          np: the parameter count its motion's ids are of (P_COUNT when written; project.c proj_motion_ids)
 *   2          flags: bit 0, the track's automation was on
 *   3          0
 *   4..11      LEN DIV SWING GATE (P_SLEN..P_SGATE), int16 little-endian
 *   12..19     the steps in use, a 64-bit mask, little-endian (a step is in use when it is not all empty)
 *   20..       each step in use, 11 bytes (step_snapshot.h: every field, Skiff's RATCH, FLAM, condition, NUDGE too)
 *   then       the motion: a count byte (0..64), then each event: step, param, value low, value high
 *   then       the timed notes (recording.c): a count, 16-bit, then each note's 10 bytes, its owner's track and bank
 *              bits left 0 (they are the bank's it is loaded into), its duration exponent kept
 * An empty pattern (no step in use, no motion, no timed notes) is kept as nothing: 0 bytes.
 *
 * A chunk's payload: four 16-bit lengths (the patterns' bytes, 0 empty), then the four patterns one after another.
 *
 * RAM: no image of the pool. A load reads its pattern straight from flash in 256-byte pieces (twice: the first pass
 * checks it and counts what it needs, the second applies it), and a save streams the new chunk from the bank being
 * saved and the chunk's copy before. ocp.len[] is each pattern's LEN (0: empty) for the PATTERNS page.
 * Saves erase flash, which mutes the audio: callers save only while stopped (Skiff's quiet stop).
 *
 * Revisions:
 *   0.3.1: first version.
 *   0.3.2: a pattern packs from any source (the sequencer's banks, or a project record's: oc_project.c); a chunk's
 *          patterns are replaced together (ocp_chunk_write); each pattern's CRC kept (ocp.hash).
 *   0.13.5: a slot loaded into the event store ends any repeat pending in it (recording_rep_drop).
 */
#define OCP_N 64u
#define OCP_PER 4u
#define OCP_CHUNKS (OCP_N / OCP_PER)
#define OCP_VER 1u
#define OCP_HEAD 20u
#define OCP_TABLE (2u * OCP_PER)
#define OCP_STEP ((uint32_t)sizeof(step_snapshot_t))
#define OCP_NOTE ((uint32_t)sizeof(recorded_note_t))
#define OCP_MAX (OCP_HEAD + NSTEP * OCP_STEP + 1u + MOTION_MAX * 4u + 2u + RECORD_MAX * OCP_NOTE)
_Static_assert(OCP_N == OCL_NPOOL, "the lists name the pool's patterns");
_Static_assert(NSTEP == 64u && OCP_TABLE + OCP_PER * OCP_MAX <= OCS_PARTS_MAX * OCS_PAYLOAD_MAX,
               "a chunk of four fullest patterns fits one record");

static struct {
    uint8_t len[OCP_N];                        /* each pattern's LEN, 0: empty */
    uint32_t hash[OCP_N];                      /* each pattern's bytes' CRC-32 (0: empty): a save writes only what
                                                * differs */
    uint32_t gen;                              /* any change (the page redraws) */
} ocp __attribute__((section(".pool")));

static uint32_t ocp_get16(const uint8_t *b) { return (uint32_t)b[0] | (uint32_t)b[1] << 8; }
static void ocp_put16(uint8_t *b, uint32_t v) { b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8); }

/* ------------------------------------------------------------- packing --- */
/* a pattern packed as it goes (an ocs_src_t): one item (the head, a step, a count, an event, a note) at a time.
 * Where its parts come from: the steps and timing in pat; the motion events of a list whose tags (mtag) name each
 * one's bank; the timed notes of a list, their owner the track and bank. The sequencer's own (ocp_gen_bank), or a
 * project record's (oc_project.c) */
typedef struct {
    ocs_src_t src;
    uint32_t k, bank, owner, len, nsteps, nmot, nnotes, mon;
    uint64_t mask;
    pattern_t pat;
    const motion_event_t *mev;
    const uint8_t *mtag;
    uint32_t mcount;
    const recorded_note_t *notes;
    int live;                                  /* notes: the sequencer's (recording_snapshot: a held one's length) */
    uint32_t phase, i;                         /* the next item: phase 0 head, 1 steps, 2 motion count, 3 events,
                                                * 4 notes count, 5 notes, 6 done */
    uint8_t item[OCP_HEAD], ipos, ilen;
} ocp_gen_t;

static int ocp_step_used(const step_t *s)
{
    step_t empty;
    memset(&empty, 0, sizeof empty);
    empty.time = ST_REST;
    return memcmp(s, &empty, sizeof empty) != 0;
}

/* back to the start (the same bytes again) */
static void ocp_gen_rewind(ocp_gen_t *g)
{
    g->phase = g->len ? 0u : 6u;
    g->i = 0; g->ipos = g->ilen = 0;
}

/* pat, the lists and k, bank set: what it holds counted. Its length (0: empty) */
static uint32_t ocp_gen_count(ocp_gen_t *g)
{
    uint32_t i;
    g->owner = g->k * NPAT + g->bank;
    g->mask = 0; g->nsteps = g->nmot = g->nnotes = 0;
    for (i = 0; i < NSTEP; i++)
        if (ocp_step_used(&g->pat.step[i])) { g->mask |= (uint64_t)1 << i; g->nsteps++; }
    for (i = 0; i < g->mcount; i++)
        g->nmot += (g->mev[i].place >> 6) == g->k && g->mtag[i] == g->bank;
    for (i = 0; g->notes && i < RECORD_MAX; i++)
        g->nnotes += g->notes[i].vel && (g->notes[i].owner & 31u) == g->owner;
    g->len = g->nsteps || g->nmot || g->nnotes ? OCP_HEAD + g->nsteps * OCP_STEP + 1u + g->nmot * 4u + 2u +
                                                     g->nnotes * OCP_NOTE : 0u;
    ocp_gen_rewind(g);
    return g->len;
}

/* track k's bank as the sequencer has it */
static uint32_t ocp_gen_bank(ocp_gen_t *g, uint32_t k, uint32_t bank)
{
    memset(g, 0, sizeof *g);
    g->k = k; g->bank = bank;
    pattern_read(k, bank, &g->pat);
    g->mev = motion.event; g->mtag = motion_pattern; g->mcount = motion.count;
    g->notes = recording; g->live = 1;
    g->mon = (motion.on >> k) & 1u;
    return ocp_gen_count(g);
}

/* the next item into g->item */
static void ocp_gen_item(ocp_gen_t *g)
{
    uint8_t *b = g->item;
    g->ipos = 0; g->ilen = 0;
    while (!g->ilen && g->phase < 6u) {
        switch (g->phase) {
        case 0:
            b[0] = OCP_VER; b[1] = (uint8_t)P_COUNT; b[2] = (uint8_t)g->mon; b[3] = 0;
            for (uint32_t j = 0; j < 4u; j++) ocp_put16(b + 4 + 2u * j, (uint16_t)g->pat.timing[j]);
            for (uint32_t j = 0; j < 8u; j++) b[12 + j] = (uint8_t)(g->mask >> (8u * j));
            g->ilen = OCP_HEAD; g->phase = 1; g->i = 0;
            break;
        case 1:
            for (; g->i < NSTEP && !((g->mask >> g->i) & 1u); g->i++)
                ;
            if (g->i == NSTEP) { g->phase = 2; break; }
            step_snapshot_put((step_snapshot_t *)b, &g->pat.step[g->i++]);
            g->ilen = (uint8_t)OCP_STEP;
            break;
        case 2:
            b[0] = (uint8_t)g->nmot; g->ilen = 1; g->phase = 3; g->i = 0;
            break;
        case 3:
            for (; g->i < g->mcount; g->i++)
                if ((g->mev[g->i].place >> 6) == g->k && g->mtag[g->i] == g->bank) break;
            if (g->i == g->mcount) { g->phase = 4; break; }
            b[0] = g->mev[g->i].place & 63u; b[1] = g->mev[g->i].param;
            ocp_put16(b + 2, (uint16_t)g->mev[g->i].value);
            g->ilen = 4; g->i++;
            break;
        case 4:
            ocp_put16(b, g->nnotes); g->ilen = 2; g->phase = 5; g->i = 0;
            break;
        case 5:
            for (; g->notes && g->i < RECORD_MAX; g->i++)
                if (g->notes[g->i].vel && (g->notes[g->i].owner & 31u) == g->owner) break;
            if (!g->notes || g->i == RECORD_MAX) { g->phase = 6; break; }
            {
                recorded_note_t r = g->live ? recording_snapshot(g->i) : g->notes[g->i];
                g->i++;
                r.owner &= 224u;
                memcpy(b, &r, OCP_NOTE);
            }
            g->ilen = (uint8_t)OCP_NOTE;
            break;
        }
    }
}

static int ocp_gen_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    ocp_gen_t *g = (ocp_gen_t *)s;
    while (n) {
        if (g->ipos == g->ilen) {
            ocp_gen_item(g);
            if (!g->ilen) return -1;           /* (asked past the length it gave: a bug, the save stops) */
        }
        uint32_t k = (uint32_t)(g->ilen - g->ipos);
        if (k > n) k = n;
        memcpy(dst, g->item + g->ipos, k);
        g->ipos = (uint8_t)(g->ipos + k); dst += k; n -= k;
    }
    return 0;
}

/* the CRC-32 of the bytes it gives (0: empty), rewound after */
static uint32_t ocp_gen_hash(ocp_gen_t *g)
{
    uint8_t b[64];
    uint32_t crc = 0xFFFFFFFFu;
    if (!g->len) return 0;
    ocp_gen_rewind(g);
    for (uint32_t at = 0; at < g->len;) {
        uint32_t n = g->len - at < sizeof b ? g->len - at : sizeof b;
        if (ocp_gen_get(&g->src, b, n)) break;
        crc = ocs_crc_add(crc, b, n);
        at += n;
    }
    ocp_gen_rewind(g);
    return ~crc | 1u;                          /* (never 0: that is empty) */
}

/* a chunk streamed: its table, then each pattern, a new one from its generator or the old one from the copy before */
typedef struct {
    ocs_src_t src;
    ocs_rec_t old;
    int have_old;
    uint32_t old_len[OCP_PER], old_off[OCP_PER], new_len[OCP_PER];
    ocp_gen_t *gen[OCP_PER];
    ocs_rec_t from;                            /* (ocp_copy) pattern j = cp_j: bytes of another chunk's record */
    uint32_t cp_j, cp_off;
    uint32_t pos;                              /* where the stream is */
} ocp_chunk_src_t;

static int ocp_chunk_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    ocp_chunk_src_t *c = (ocp_chunk_src_t *)s;
    while (n) {
        uint32_t at = c->pos, base = OCP_TABLE, j, k;
        if (at < OCP_TABLE) {                  /* the table */
            uint8_t t[OCP_TABLE];
            for (j = 0; j < OCP_PER; j++) ocp_put16(t + 2u * j, c->new_len[j]);
            k = OCP_TABLE - at < n ? OCP_TABLE - at : n;
            memcpy(dst, t + at, k);
        } else {
            for (j = 0; j < OCP_PER && at >= base + c->new_len[j]; j++) base += c->new_len[j];
            if (j == OCP_PER) return -1;
            k = base + c->new_len[j] - at < n ? base + c->new_len[j] - at : n;
            if (j == c->cp_j) {
                if (ocs_at(&c->from, c->cp_off + (at - base), dst, k)) return -11;
            } else if (c->gen[j]) {
                if (ocp_gen_get(&c->gen[j]->src, dst, k)) return -1;
            } else if (!c->have_old || ocs_at(&c->old, c->old_off[j] + (at - base), dst, k)) {
                return -11;                    /* (the copy before could not be read: the save stops) */
            }
        }
        c->pos += k; dst += k; n -= k;
    }
    return 0;
}

/* the chunk's copy before: its patterns' lengths and where they are. 0 none (all empty), 1 read, -1 unreadable */
static int ocp_chunk_open(uint32_t chunk, ocs_rec_t *r, uint32_t *len, uint32_t *off)
{
    uint8_t t[OCP_TABLE];
    uint32_t j, at = OCP_TABLE;
    memset(len, 0, OCP_PER * sizeof *len);
    memset(off, 0, OCP_PER * sizeof *off);
    if (ocs_open(OCS_POOL, chunk, r)) return 0;
    if (r->total < OCP_TABLE || ocs_at(r, 0, t, OCP_TABLE)) return -1;
    for (j = 0; j < OCP_PER; j++) {
        len[j] = ocp_get16(t + 2u * j);
        off[j] = at;
        at += len[j];
    }
    return at == r->total ? 1 : -1;
}

/* the LEN a pattern's bytes give (0: empty or unreadable) */
static uint32_t ocp_len_of(const ocs_rec_t *r, uint32_t off, uint32_t len)
{
    uint8_t h[6];
    if (len < OCP_HEAD || ocs_at(r, off, h, sizeof h) || h[0] != OCP_VER) return 0;
    return (uint32_t)clamp((int16_t)ocp_get16(h + 4), 1, NSTEP);
}

/* the CRC-32 of a pattern's bytes as kept (0: empty or unreadable) */
static uint32_t ocp_hash_of(const ocs_rec_t *r, uint32_t off, uint32_t len)
{
    uint8_t b[256];
    uint32_t crc = 0xFFFFFFFFu;
    if (!len) return 0;
    for (uint32_t at = 0; at < len;) {
        uint32_t n = len - at < sizeof b ? len - at : sizeof b;
        if (ocs_at(r, off + at, b, n)) return 0;
        crc = ocs_crc_add(crc, b, n);
        at += n;
    }
    return ~crc | 1u;
}

/* chunk's patterns j with gen[j] set replaced by what gen[j] gives (length 0: emptied), the others as they were: one
 * record write (one erase per sector). 0 done; 2 no room in flash; another: a flash error (the chunk as it was is
 * kept). Stopped only: the erase mutes the audio */
static ocp_chunk_src_t ocp_cs __attribute__((section(".pool")));
static int ocp_chunk_write_cp(uint32_t chunk, ocp_gen_t *const *gen, uint32_t cp_j, uint32_t cp_len);
static int ocp_chunk_write(uint32_t chunk, ocp_gen_t *const *gen)
{
    memset(&ocp_cs, 0, sizeof ocp_cs);
    return ocp_chunk_write_cp(chunk, gen, OCP_PER, 0);
}
/* .. and with pattern cp_j (OCP_PER: none) cp_len bytes of ocp_cs.from at ocp_cs.cp_off (ocp_copy) */
static int ocp_chunk_write_cp(uint32_t chunk, ocp_gen_t *const *gen, uint32_t cp_j, uint32_t cp_len)
{
    ocp_chunk_src_t c_ = ocp_cs, *cp = &c_;
    uint32_t total = OCP_TABLE, empty = 1, j;
    int rc;
#define c (*cp)
    c.src.get = ocp_chunk_get;
    c.cp_j = cp_j;
    rc = ocp_chunk_open(chunk, &c.old, c.old_len, c.old_off);
    if (rc < 0) {                              /* (the copy before is unreadable: its other patterns start empty) */
        rc = 0;
        memset(c.old_len, 0, sizeof c.old_len);
    }
    c.have_old = rc;
    for (j = 0; j < OCP_PER; j++) {
        c.gen[j] = j == cp_j ? 0 : gen[j];
        if (c.gen[j]) ocp_gen_rewind(c.gen[j]);
        c.new_len[j] = j == cp_j ? cp_len : c.gen[j] ? c.gen[j]->len : c.old_len[j];
        total += c.new_len[j];
        empty &= !c.new_len[j];
    }
    if (empty) {                               /* nothing left in the chunk: no record at all */
        rc = ocs_delete(OCS_POOL, chunk);
    } else {
        rc = ocs_write_src(OCS_POOL, chunk, &c.src, total);
        if (rc == -1) rc = 2;
    }
    if (!rc) {
        for (j = 0; j < OCP_PER; j++) {
            uint32_t id = chunk * OCP_PER + j;
            if (!c.gen[j]) continue;
            ocp.len[id] = (uint8_t)(c.gen[j]->len ? clamp(c.gen[j]->pat.timing[0], 1, NSTEP) : 0);
            ocp.hash[id] = ocp_gen_hash(c.gen[j]);
        }
        ocp.gen++;
    }
    return rc;
#undef c
}

/* pool pattern src copied to dst (stopped: an erase mutes the audio). 0 done; 1 src empty or unreadable; else as
 * ocp_chunk_write */
static int ocp_copy(uint32_t src, uint32_t dst)
{
    ocp_gen_t *gen[OCP_PER] = {0};
    uint32_t len[OCP_PER], off[OCP_PER];
    int rc;
    if (src >= OCP_N || dst >= OCP_N || src == dst) return 1;
    memset(&ocp_cs, 0, sizeof ocp_cs);
    if (ocp_chunk_open(src / OCP_PER, &ocp_cs.from, len, off) != 1 || !len[src % OCP_PER]) return 1;
    ocp_cs.cp_off = off[src % OCP_PER];
    rc = ocp_chunk_write_cp(dst / OCP_PER, gen, dst % OCP_PER, len[src % OCP_PER]);
    if (!rc) {
        ocp.len[dst] = ocp.len[src];
        ocp.hash[dst] = ocp.hash[src];
    }
    return rc;
}

/* save track k's bank as pool pattern id (an empty bank empties it): as ocp_chunk_write */
static int ocp_save(uint32_t id, uint32_t k, uint32_t bank)
{
    static ocp_gen_t g __attribute__((section(".pool")));
    ocp_gen_t *gen[OCP_PER] = {0};
    if (id >= OCP_N || k >= NTRK || bank >= NPAT) return 1;
    ocp_gen_bank(&g, k, bank);
    gen[id % OCP_PER] = &g;
    return ocp_chunk_write(id / OCP_PER, gen);
}

/* ------------------------------------------------------------- loading --- */
/* a pattern's bytes read in order, 256 at a time */
typedef struct {
    const ocs_rec_t *r;
    uint32_t off, end;
    uint8_t buf[256];
    uint32_t pos, n;
} ocp_rd_t;

static int ocp_rd(ocp_rd_t *rd, void *dst, uint32_t n)
{
    uint8_t *d = dst;
    while (n) {
        if (rd->pos == rd->n) {
            uint32_t k = rd->end - rd->off;
            if (!k) return -1;
            if (k > sizeof rd->buf) k = sizeof rd->buf;
            if (ocs_at(rd->r, rd->off, rd->buf, k)) return -1;
            rd->off += k; rd->pos = 0; rd->n = k;
        }
        uint32_t k = rd->n - rd->pos < n ? rd->n - rd->pos : n;
        memcpy(d, rd->buf + rd->pos, k);
        rd->pos += k; d += k; n -= k;
    }
    return 0;
}

static int ocp_step_ok(const step_t *s)
{
    if (s->n > 4u || s->time > ST_REST || (s->flags & ~SF_ALL) || s->vel > 127u || s->probability > 101u ||
        !step_acc_valid(s) || s->cond >= TC_COUNT || s->nudge < NUDGE_MIN || s->nudge > NUDGE_MAX)
        return 0;
    for (uint32_t j = 0; j < 4u; j++)
        if (s->note[j] > 127u) return 0;
    return 1;
}

/* the head, the steps and the motion of a pattern (the notes are read after, as they are placed) */
typedef struct {
    pattern_t pat;
    motion_store_t mot;                        /* (its events' places carry the track's bits) */
    uint32_t nnotes, flags;
} ocp_parsed_t;

static int ocp_parse(ocp_rd_t *rd, uint32_t k, ocp_parsed_t *p)
{
    uint8_t h[OCP_HEAD], b[4];
    uint64_t mask = 0;
    uint32_t i, np;
    if (ocp_rd(rd, h, OCP_HEAD) || h[0] != OCP_VER || h[3]) return 1;
    np = h[1];
    if (np < 8u || np > P_COUNT || (h[2] & ~1u)) return 1;
    p->flags = h[2];
    for (i = 0; i < 4u; i++) {
        int32_t v = (int16_t)ocp_get16(h + 4 + 2u * i);
        if (v < TP[P_SLEN + i].min || v > TP[P_SLEN + i].max) return 1;
        p->pat.timing[i] = (int16_t)v;
    }
    for (i = 0; i < 8u; i++) mask |= (uint64_t)h[12 + i] << (8u * i);
    for (i = 0; i < NSTEP; i++) {
        memset(&p->pat.step[i], 0, sizeof p->pat.step[i]);
        p->pat.step[i].time = ST_REST;
        if ((mask >> i) & 1u) {
            step_snapshot_t v;
            if (ocp_rd(rd, &v, OCP_STEP)) return 1;
            p->pat.step[i] = step_snapshot_get(&v);
            if (!ocp_step_ok(&p->pat.step[i])) return 1;
        }
    }
    memset(&p->mot, 0, sizeof p->mot);
    if (ocp_rd(rd, b, 1) || b[0] > MOTION_MAX) return 1;
    p->mot.count = b[0];
    p->mot.rsv[0] = 1;                         /* (events of several banks: no duplicate check across them) */
    for (i = 0; i < p->mot.count; i++) {
        if (ocp_rd(rd, b, 4) || b[0] >= NSTEP) return 1;
        p->mot.event[i].place = (uint8_t)(k << 6 | b[0]);
        p->mot.event[i].param = b[1];
        p->mot.event[i].value = (int16_t)ocp_get16(b + 2);
    }
    proj_motion_ids(&p->mot, np);
    if (!motion_valid(&p->mot)) return 1;
    if (ocp_rd(rd, b, 2)) return 1;
    p->nnotes = ocp_get16(b);
    return p->nnotes > RECORD_MAX;
}

/* pool pattern id opened for reading at its start (rd). 1 there, 0 empty, -1 unreadable */
static int ocp_open_pattern(uint32_t id, ocs_rec_t *r, ocp_rd_t *rd)
{
    uint32_t len[OCP_PER], off[OCP_PER];
    int rc = ocp_chunk_open(id / OCP_PER, r, len, off);
    memset(rd, 0, sizeof *rd);
    if (rc != 1) return rc;
    if (!len[id % OCP_PER]) return 0;
    rd->r = r; rd->off = off[id % OCP_PER]; rd->end = rd->off + len[id % OCP_PER];
    return 1;
}

/* pool pattern id read whole and checked (rd left at its notes): 0 good (p filled; an empty one: no steps, default
 * timing), 1 not */
static int ocp_check(uint32_t id, uint32_t k, ocs_rec_t *r, ocp_rd_t *rd, ocp_parsed_t *p, uint32_t *notes_at)
{
    int rc = ocp_open_pattern(id, r, rd);
    memset(p, 0, sizeof *p);
    if (rc < 0) return 1;
    if (!rc) {
        for (uint32_t i = 0; i < NSTEP; i++) p->pat.step[i].time = ST_REST;
        for (uint32_t i = 0; i < 4u; i++) p->pat.timing[i] = TP[P_SLEN + i].def;
        return 0;
    }
    if (ocp_parse(rd, k, p)) return 1;
    *notes_at = rd->off - (rd->n - rd->pos);  /* (where the notes start, to read them again) */
    for (uint32_t n = 0; n < p->nnotes; n++) {    /* every note checks before anything changes */
        recorded_note_t v;
        if (ocp_rd(rd, &v, OCP_NOTE) || !v.vel || (v.owner & 31u) || !recording_valid(&v)) return 1;
    }
    return rd->off != rd->end || rd->pos != rd->n;
}

/* the notes again from where ocp_check found them */
static void ocp_notes_rewind(ocp_rd_t *rd, uint32_t notes_at) { rd->off = notes_at; rd->pos = rd->n = 0; }

/* pool pattern id into track k's bank (the one playing too: it goes on from the same step). apply 0: only checks.
 * 0 done; 1 refused (a song running) or the pattern unreadable; 2 no room (motion or timed notes) */
static int ocp_load(uint32_t id, uint32_t k, uint32_t bank, int apply)
{
    static ocp_parsed_t p __attribute__((section(".pool")));
    static ocp_rd_t rd __attribute__((section(".pool")));
    ocs_rec_t r;
    uint32_t owner = k * NPAT + bank, i, j, f, n, free = 0, notes_at = 0;
    track_t *t = &trk[k];
    if (id >= OCP_N || k >= NTRK || bank >= NPAT) return 1;
    if (ocp_check(id, k, &r, &rd, &p, &notes_at)) return 1;
    f = motion_guard();
    if (chain.running || chain.armed) { motion_unguard(f); return 1; }
    for (i = 0, n = 0; i < motion.count; i++) n += (motion.event[i].place >> 6) == k && motion_pattern[i] == bank;
    for (i = 0; i < RECORD_MAX; i++) free += !recording[i].vel || (recording[i].owner & 31u) == owner;
    if (motion.count - n + p.mot.count > MOTION_MAX || p.nnotes > free) { motion_unguard(f); return 2; }
    if (!apply) { motion_unguard(f); return 0; }
    /* the bank's notes and motion out, the pattern's steps and motion in */
    recording_drop(t, owner);
    recording_reindex();
    pattern_write(k, bank, &p.pat);
    for (i = 0, n = 0; i < motion.count; i++) if ((motion.event[i].place >> 6) != k || motion_pattern[i] != bank) {
        motion_pattern[n] = motion_pattern[i]; motion.event[n++] = motion.event[i];
    }
    motion.count = (uint8_t)n;
    for (i = 0; i < p.mot.count; i++) {
        motion.event[motion.count] = p.mot.event[i]; motion_pattern[motion.count++] = (uint8_t)bank;
    }
    motion.rsv[0] = 1;
    if (p.flags & 1u) motion.on |= (uint8_t)(1u << k);
    motion_unguard(f);
    /* the notes, read again and placed a guarded piece at a time (placed, not indexed: the audio does not see them
     * until the reindex below, and live recording takes only free entries) */
    if (p.nnotes) {
        ocp_notes_rewind(&rd, notes_at);
        for (n = 0, j = 0; n < p.nnotes; n++) {
            recorded_note_t v;
            if (ocp_rd(&rd, &v, OCP_NOTE)) break;
            v.owner = (uint8_t)((v.owner & 224u) | owner);
            f = motion_guard();
            for (; j < RECORD_MAX && recording[j].vel; j++)
                ;
            if (j < RECORD_MAX) {
                recording[j] = v;
                memset(&recording_run[j], 0, sizeof recording_run[j]);
                recording_rep_drop(j); recording_flags[j] = 0;
            }
            motion_unguard(f);
        }
    }
    f = motion_guard();
    recording_reindex();
    if (bank == t->pattern) pattern_apply(t, bank);
    motion_unguard(f);
    return 0;
}

/* boot: every pattern's LEN for the PATTERNS page, and its bytes' CRC (after ocs_boot) */
static void ocp_scan(void)
{
    memset(&ocp, 0, sizeof ocp);
    for (uint32_t c = 0; c < OCP_CHUNKS; c++) {
        ocs_rec_t r;
        uint32_t len[OCP_PER], off[OCP_PER];
        if (ocp_chunk_open(c, &r, len, off) != 1) continue;
        for (uint32_t j = 0; j < OCP_PER; j++) {
            ocp.len[c * OCP_PER + j] = (uint8_t)(len[j] ? ocp_len_of(&r, off[j], len[j]) : 0u);
            ocp.hash[c * OCP_PER + j] = ocp_hash_of(&r, off[j], len[j]);
        }
    }
    ocp.gen++;
}
