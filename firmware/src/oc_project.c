/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the project store (build doc phase 4a, part 3). Included by project.c, after pattern_store.c.
 *
 * Melodee keeps each of its four project slots as one FBKK record of 27,904 bytes: the sounds, and all 32 pattern
 * banks inside it. Orange Cat keeps a slot as two things in the record store (oc_store.c):
 *   - each bank that holds anything is a pattern of the pool (oc_pool.c), with its timed notes, motion and Skiff's
 *     step fields (which FBKK has no room for: pattern_store.c proj_x);
 *   - the rest is one small record (OCS_PROJECT, id = the slot): which pool pattern each bank is, the active banks,
 *     the song's bank columns, the FM6 function settings, and Melodee's FUN22 project part with its steps, timed
 *     notes and motion left out, run-length coded (it is mostly zeros then: about 3 KB).
 * Everything above this file still works in FBKK: proj_read_slot gets an FBKK built from the store (ocj_read),
 * proj_write_slot hands one over (ocj_write). So SAVE, load, autosave, rename, the web editor and its backup are as
 * Melodee had them, and a Melodee backup restores into the store (its banks become pool patterns).
 *
 * The pool is shared: a pattern belongs to whichever slots name it. A slot's banks keep the pool patterns they had
 * (a save rewrites only those that changed); a bank new to a slot takes a free pattern (one no slot names). No free
 * pattern: the save fails (PATTERNS FULL, 64 at most across the four slots). Patterns no slot names any more are
 * emptied after the slot's record is written.
 *
 * Power cuts: a pool chunk and the project record are each written whole or not at all (oc_store.c). A save writes
 * the changed patterns first, then the record, then empties patterns let go. Cut before the record: the slot reads
 * its record before, with any of its own patterns already rewritten new (the save was part way). Cut after: patterns
 * let go are left in the pool, unnamed, until a later save takes them as free.
 *
 * The record (all little-endian):
 *   ocj_head_t, then the FUN22 project part run-length coded: a byte other than 0 is itself, 0 then n (1..255) is n
 *   zero bytes.
 *
 * Revisions:
 *   0.3.2: first version.
 */
#define OCJ_MAGIC 0x314A434Fu                  /* "OCJ1" */
#define OCJ_FN (4u + NTRK * FM6_NFN)           /* FBKK's FM6 function settings (BANK_FN_OFF: their magic, then them) */
typedef struct {
    uint32_t magic, len, zlen;                 /* len: the whole record; zlen: the coded project part after this */
    uint8_t pid[NTRK][NPAT];                   /* each bank's pool pattern + 1; 0: empty */
    uint8_t active[NTRK];                      /* the bank each track is on */
    uint8_t chain[CHAIN_ROWS * NTRK];          /* FBKK's song columns (BANK_CHAIN_OFF) */
    char name[PROJ_NAME_LEN];                  /* the project's name (the slot list) */
    uint8_t fn[OCJ_FN];
} ocj_head_t;

static struct {
    uint8_t pid[4][NTRK][NPAT];                /* each slot's map, as its record has it (all 0: no record) */
} ocj __attribute__((section(".pool")));
static ocj_head_t ocj_h __attribute__((section(".pool")));
static ocp_gen_t ocj_gen[OCP_PER] __attribute__((section(".pool")));
static int ocj_read(uint32_t slot, uint8_t *raw);

/* how many banks of the slots (but slot `but`, 4: none) name pool pattern id */
static uint32_t ocj_refs(uint32_t id, uint32_t but)
{
    uint32_t n = 0;
    for (uint32_t s = 0; s < 4u; s++)
        for (uint32_t k = 0; s != but && k < NTRK; k++)
            for (uint32_t b = 0; b < NPAT; b++) n += ocj.pid[s][k][b] == id + 1u;
    return n;
}

/* boot (after ocs_boot, ocp_scan): each slot's map */
static void ocj_scan(void)
{
    memset(&ocj, 0, sizeof ocj);
    for (uint32_t s = 0; s < 4u; s++) {
        ocs_rec_t r;
        if (ocs_open(OCS_PROJECT, s, &r) || r.total < sizeof ocj_h || ocs_at(&r, 0, &ocj_h, sizeof ocj_h) ||
            ocj_h.magic != OCJ_MAGIC || ocj_h.len != r.total)
            continue;
        memcpy(ocj.pid[s], ocj_h.pid, sizeof ocj.pid[s]);
        for (uint32_t k = 0; k < NTRK; k++)
            for (uint32_t b = 0; b < NPAT; b++)
                if (ocj.pid[s][k][b] > OCP_N) ocj.pid[s][k][b] = 0;
    }
}

/* ------------------------------------------------------------- writing --- */
/* FBKK raw's bank b of track k as a pattern source (q: its project part, read by bank_valid; xs: proj_x goes with
 * raw). Its length (0: empty) */
static uint32_t ocj_gen_fbkk(ocp_gen_t *g, const project_t *q, const uint8_t *raw, uint32_t k, uint32_t b, int xs)
{
    uint32_t act = raw[BANK_ACTIVE_OFF + k], i;
    memset(g, 0, sizeof *g);
    g->k = k; g->bank = b;
    if (b == act) {
        memcpy(g->pat.step, q->t[k].step, sizeof g->pat.step);
    } else {
        const uint8_t *pos = raw + BANK_EXTRA_OFF + (k * (NPAT - 1u) + (b < act ? b : b - 1u)) * NSTEP * BANK_STEP_SIZE;
        for (i = 0; i < NSTEP; i++) step_wire_unpack(&g->pat.step[i], pos + i * BANK_STEP_SIZE);
    }
    if (xs) proj_x_get(k, b, g->pat.step);
    for (i = 0; i < 4u; i++) g->pat.timing[i] = raw[BANK_TIMING_OFF + (k * NPAT + b) * 4u + i];
    g->mev = q->motion.event; g->mtag = bank_import_tags; g->mcount = q->motion.count;
    g->notes = q->recording; g->live = 0;
    g->mon = (q->motion.on >> k) & 1u;
    return ocp_gen_count(g);
}

/* the record streamed: the head, then the project part (at raw + 8) run-length coded */
typedef struct {
    ocs_src_t src;
    const uint8_t *in;
    uint32_t pos, hpos;
    int pending;                               /* a zero run's count still to give, -1 none */
} ocj_src_t;
static int ocj_rle_byte(ocj_src_t *z, uint8_t *out)
{
    if (z->pending >= 0) { *out = (uint8_t)z->pending; z->pending = -1; return 0; }
    if (z->pos >= PROJ_STORE_SIZE) return -1;
    if (z->in[z->pos]) { *out = z->in[z->pos++]; return 0; }
    uint32_t n = 0;
    while (n < 255u && z->pos < PROJ_STORE_SIZE && !z->in[z->pos]) { n++; z->pos++; }
    *out = 0; z->pending = (int)n;
    return 0;
}
static int ocj_get(ocs_src_t *s, uint8_t *dst, uint32_t n)
{
    ocj_src_t *z = (ocj_src_t *)s;
    for (uint32_t i = 0; i < n; i++) {
        if (z->hpos < sizeof ocj_h) { dst[i] = ((const uint8_t *)&ocj_h)[z->hpos++]; continue; }
        if (ocj_rle_byte(z, dst + i)) return -1;
    }
    return 0;
}
static uint32_t ocj_rle_len(const uint8_t *in)
{
    ocj_src_t z = {{0}, in, 0, 0, -1};
    uint32_t n = 0;
    uint8_t b;
    while (!ocj_rle_byte(&z, &b)) n++;
    return n;
}

/* chunks with any of `want` (pool id + 1 per id, 0 none: for each id what to write: 1..32 the bank (k * NPAT + b) + 1,
 * 255 emptied) written, a chunk at a time. 0 done, else ocp_chunk_write's */
static int ocj_put_chunks(const uint8_t *want, const project_t *q, const uint8_t *raw, int xs)
{
    for (uint32_t c = 0; c < OCP_CHUNKS; c++) {
        ocp_gen_t *gen[OCP_PER] = {0};
        uint32_t any = 0;
        int rc;
        for (uint32_t j = 0; j < OCP_PER; j++) {
            uint32_t w = want[c * OCP_PER + j];
            if (!w) continue;
            gen[j] = &ocj_gen[j];
            if (w == 255u) { memset(gen[j], 0, sizeof *gen[j]); gen[j]->phase = 6; }
            else ocj_gen_fbkk(gen[j], q, raw, (w - 1u) / NPAT, (w - 1u) % NPAT, xs);
            any = 1;
        }
        if (any && (rc = ocp_chunk_write(c, gen)) != 0) return rc;
    }
    return 0;
}

/* FBKK raw (len bytes, BANK_STORE_SIZE; 0: the slot emptied) into slot. 0 done; 2 not (PATTERNS FULL, no room, a flash
 * error: the slot's record as it was). raw is used, and left as the slot reads back (ocj_read) when it is done */
static int ocj_write(uint32_t slot, uint8_t *raw, uint32_t len)
{
    static uint8_t want[OCP_N];
    uint8_t npid[NTRK][NPAT];
    uint32_t k, b, id, hash[NTRK][NPAT];
    project_t *q = &proj_scratch;
    int xs, rc;
    slot &= 3u;
    if (!len) {                                /* emptied: the record, then the patterns only it named */
        if (ocs_delete(OCS_PROJECT, slot)) return 2;
        memset(want, 0, sizeof want);
        for (k = 0; k < NTRK; k++)
            for (b = 0; b < NPAT; b++)
                if ((id = ocj.pid[slot][k][b]) != 0u && !ocj_refs(id - 1u, slot)) want[id - 1u] = 255u;
        memset(ocj.pid[slot], 0, sizeof ocj.pid[slot]);
        (void)ocj_put_chunks(want, q, raw, 0);   /* (failing: left unnamed, free to a later save) */
        return 0;
    }
    xs = proj_x_tag == proj_hash(raw, BANK_STORE_SIZE);
    if (len != BANK_STORE_SIZE || !bank_valid(raw, len)) return 2;
    /* 1. each bank: what it holds, and the pool pattern it keeps or takes */
    memset(npid, 0, sizeof npid);
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++) {
            hash[k][b] = 0;
            if (ocj_gen_fbkk(&ocj_gen[0], q, raw, k, b, xs)) hash[k][b] = ocp_gen_hash(&ocj_gen[0]);
        }
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++) {
            if (!hash[k][b]) continue;
            id = ocj.pid[slot][k][b];
            if (id && !ocj_refs(id - 1u, slot)) {   /* (its own, no other slot's: kept) */
                uint32_t dup = 0;
                for (uint32_t k2 = 0; k2 < NTRK; k2++)
                    for (uint32_t b2 = 0; b2 < NPAT; b2++) dup |= npid[k2][b2] == id;
                if (!dup) { npid[k][b] = (uint8_t)id; continue; }
            }
            for (id = 1; id <= OCP_N; id++) {       /* a free one: no slot names it, no bank here has it */
                uint32_t taken = ocj_refs(id - 1u, slot);
                for (uint32_t k2 = 0; k2 < NTRK && !taken; k2++)
                    for (uint32_t b2 = 0; b2 < NPAT; b2++) taken |= npid[k2][b2] == id || ocj.pid[slot][k2][b2] == id;
                if (!taken) break;
            }
            if (id > OCP_N) {                       /* (none: try this slot's own let go, before giving up) */
                for (id = 1; id <= OCP_N; id++) {
                    uint32_t taken = ocj_refs(id - 1u, slot);
                    for (uint32_t k2 = 0; k2 < NTRK && !taken; k2++)
                        for (uint32_t b2 = 0; b2 < NPAT; b2++) taken |= npid[k2][b2] == id;
                    if (!taken) break;
                }
                if (id > OCP_N) { ui_message("PATTERNS FULL"); return 2; }
            }
            npid[k][b] = (uint8_t)id;
        }
    /* 2. the patterns that changed */
    memset(want, 0, sizeof want);
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++)
            if ((id = npid[k][b]) != 0u && ocp.hash[id - 1u] != hash[k][b]) want[id - 1u] = (uint8_t)(k * NPAT + b + 1u);
    if ((rc = ocj_put_chunks(want, q, raw, xs)) != 0) return 2;
    /* 3. the record: the project part without its steps, timed notes and motion */
    memset(&ocj_h, 0, sizeof ocj_h);
    ocj_h.magic = OCJ_MAGIC;
    memcpy(ocj_h.pid, npid, sizeof npid);
    memcpy(ocj_h.active, raw + BANK_ACTIVE_OFF, NTRK);
    memcpy(ocj_h.chain, raw + BANK_CHAIN_OFF, sizeof ocj_h.chain);
    memcpy(ocj_h.name, q->name, PROJ_NAME_LEN);
    memcpy(ocj_h.fn, raw + BANK_FN_OFF, OCJ_FN);
    for (k = 0; k < NTRK; k++)
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&q->t[k].step[i]);
    memset(q->recording, 0, sizeof q->recording);
    q->motion.count = 0;
    memset(q->motion.event, 0, sizeof q->motion.event);
    if (!proj_pack((project_store_t *)(raw + 8u), q)) return 2;
    ocj_h.zlen = ocj_rle_len(raw + 8u);
    ocj_h.len = (uint32_t)sizeof ocj_h + ocj_h.zlen;
    {
        ocj_src_t z = {{ocj_get}, raw + 8u, 0, 0, -1};
        rc = ocs_write_src(OCS_PROJECT, slot, &z.src, ocj_h.len);
    }
    if (rc) return 2;
    /* 4. the patterns let go (no slot names them now) emptied */
    memset(want, 0, sizeof want);
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++) {
            uint32_t old = ocj.pid[slot][k][b], still = 0;
            if (!old) continue;
            for (uint32_t k2 = 0; k2 < NTRK; k2++)
                for (uint32_t b2 = 0; b2 < NPAT; b2++) still |= npid[k2][b2] == old;
            if (!still && !ocj_refs(old - 1u, slot)) want[old - 1u] = 255u;
        }
    memcpy(ocj.pid[slot], npid, sizeof npid);
    (void)ocj_put_chunks(want, q, raw, 0);     /* (failing: left unnamed, free to a later save) */
    /* 5. the staging as the slot reads back */
    return ocj_read(slot, raw) == (int)BANK_STORE_SIZE ? 0 : 2;
}

/* ------------------------------------------------------------- reading --- */
/* slot as an FBKK into raw (proj_x with it). BANK_STORE_SIZE, or -1 (no record, or one that does not read) */
static int ocj_read(uint32_t slot, uint8_t *raw)
{
    static ocp_parsed_t p __attribute__((section(".pool")));
    static ocp_rd_t rd __attribute__((section(".pool")));
    project_t *q = &proj_scratch;
    ocs_rec_t r, pr;
    uint32_t k, b, i, at, out = 0, nmot = 0, nnote = 0, notes_at = 0;
    slot &= 3u;
    if (ocs_open(OCS_PROJECT, slot, &r) || r.total < sizeof ocj_h || ocs_at(&r, 0, &ocj_h, sizeof ocj_h) ||
        ocj_h.magic != OCJ_MAGIC || ocj_h.len != r.total || ocj_h.zlen != r.total - sizeof ocj_h)
        return -1;
    /* the project part, decoded into raw + 8 */
    memset(&rd, 0, sizeof rd);
    rd.r = &r; rd.off = sizeof ocj_h; rd.end = r.total;
    for (at = 0; at < ocj_h.zlen;) {
        uint8_t v, n;
        if (ocp_rd(&rd, &v, 1)) return -1;
        at++;
        if (v) { if (out >= PROJ_STORE_SIZE) return -1; raw[8u + out++] = v; continue; }
        if (ocp_rd(&rd, &n, 1) || !n || out + n > PROJ_STORE_SIZE) return -1;
        at++;
        memset(raw + 8u + out, 0, n);
        out += n;
    }
    if (out != PROJ_STORE_SIZE || !proj_import(q, raw + 8u, PROJ_STORE_SIZE)) return -1;
    memset(q->recording, 0, sizeof q->recording);
    q->motion.count = 0;
    memset(raw + BANK_ACTIVE_OFF, 0, BANK_STORE_SIZE - BANK_ACTIVE_OFF);
    memset(proj_x, 0, sizeof proj_x);
    /* the banks, from the pool */
    for (k = 0; k < NTRK; k++) {
        uint32_t act = ocj_h.active[k];
        if (act >= NPAT || act != q->pattern[k]) return -1;
        raw[BANK_ACTIVE_OFF + k] = (uint8_t)act;
        for (b = 0; b < NPAT; b++) {
            uint32_t id = ocj_h.pid[k][b];
            const step_t *st;
            if (id > OCP_N) return -1;
            if (id) {
                if (ocp_check(id - 1u, k, &pr, &rd, &p, &notes_at)) return -1;
                if (nmot + p.mot.count > MOTION_MAX || nnote + p.nnotes > RECORD_MAX) return -1;
                for (i = 0; i < p.mot.count; i++) {
                    q->motion.event[nmot] = p.mot.event[i];
                    raw[BANK_MOTION_OFF + nmot++] = (uint8_t)b;
                }
                if (p.nnotes) {
                    ocp_notes_rewind(&rd, notes_at);
                    for (i = 0; i < p.nnotes; i++) {
                        recorded_note_t v;
                        if (ocp_rd(&rd, &v, OCP_NOTE)) return -1;
                        v.owner = (uint8_t)((v.owner & 224u) | (k * NPAT + b));
                        q->recording[nnote++] = v;
                    }
                }
            } else {
                memset(&p, 0, sizeof p);
                for (i = 0; i < NSTEP; i++) p.pat.step[i].time = ST_REST;
                for (i = 0; i < 4u; i++) p.pat.timing[i] = b == act ? q->t[k].p[P_SLEN + i] : TP[P_SLEN + i].def;
            }
            st = p.pat.step;
            proj_x_put(k, b, st);
            for (i = 0; i < 4u; i++) raw[BANK_TIMING_OFF + (k * NPAT + b) * 4u + i] = (uint8_t)p.pat.timing[i];
            if (b == act) {
                memcpy(q->t[k].step, st, sizeof q->t[k].step);
                for (i = 0; i < 4u; i++) q->t[k].p[P_SLEN + i] = p.pat.timing[i];
            } else {
                uint8_t *pos = raw + BANK_EXTRA_OFF + (k * (NPAT - 1u) + (b < act ? b : b - 1u)) * NSTEP * BANK_STEP_SIZE;
                for (i = 0; i < NSTEP; i++)
                    if (!bank_step_pack(pos + i * BANK_STEP_SIZE, &st[i])) return -1;
            }
        }
    }
    q->motion.count = (uint8_t)nmot;
    q->motion.rsv[0] = 1;
    memcpy(raw + BANK_CHAIN_OFF, ocj_h.chain, sizeof ocj_h.chain);
    memcpy(raw + BANK_FN_OFF, ocj_h.fn, OCJ_FN);
    {
        uint32_t magic = BANK_MAGIC, size = BANK_STORE_SIZE;
        memcpy(raw, &magic, 4); memcpy(raw + 4, &size, 4);
    }
    q->sum = proj_sum(q);
    if (!proj_pack((project_store_t *)(raw + 8u), q)) return -1;
    bank_checksum(raw);
    proj_x_tag = proj_hash(raw, BANK_STORE_SIZE);
    return (int)BANK_STORE_SIZE;
}
