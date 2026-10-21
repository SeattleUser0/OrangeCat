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
 *   ocj_head_t, then the lists (ocl_lists_t) and the FUN22 project part run-length coded as one: a byte other than 0
 *   is itself, 0 then n (1..255) is n zero bytes. (OCJ2: the lists, 0.3's, at the head's end; the project part
 *   alone coded.)
 *
 * Revisions:
 *   0.3.2: first version.
 *   0.3.3: (4c) OCJ2: the lists in the record; the banks' patterns as the sequencer has them (proj_pid: shared, as
 *          Skiff's projects share patterns); a pattern anything loaded names is not emptied.
 *   0.4.0: OCJ3: the chains, the song, where each track is in them (pattern.c ocl_lists_t), coded with the project
 *          part (ahead of it: mostly zeros). An OCJ2 record reads as one: each track's list its chain (track 1 on
 *          chain 01 ..), row 01 of the song naming them. The head says how long the lists are (lsize): a later
 *          version may add to them.
 *   0.4.4: (a review) a phrase only a saved project's chains name is that project's: never emptied, never taken
 *          (ocj.names); a save's last resort takes only its own let go, or an empty phrase.
 */
#define OCJ_MAGIC 0x334A434Fu                  /* "OCJ3" */
#define OCJ_MAGIC2 0x324A434Fu                 /* "OCJ2" (0.3: the lists, one a track; OCJ1, before them: never released) */
#define OCJ_FN (4u + NTRK * FM6_NFN)           /* FBKK's FM6 function settings (BANK_FN_OFF: their magic, then them) */
typedef struct {
    uint32_t magic, len, zlen;                 /* len: the whole record; zlen: the coded project part after this */
    uint8_t pid[NTRK][NPAT];                   /* each bank's pool pattern + 1; 0: empty */
    uint8_t active[NTRK];                      /* the bank each track is on */
    uint8_t chain[CHAIN_ROWS * NTRK];          /* FBKK's song columns (BANK_CHAIN_OFF) */
    char name[PROJ_NAME_LEN];                  /* the project's name (the slot list) */
    uint8_t fn[OCJ_FN];
    uint32_t lsize;                            /* (OCJ3) the lists' bytes coded ahead of the project part: a later
                                                * version's longer lists read as far as this one's go, a shorter one's
                                                * rest as a new project's */
} ocj_head_t;
static ocl_lists_t ocj_l __attribute__((section(".pool")));   /* (4c) the chains, the song, where each track is */
static uint32_t ocj_lead;                      /* (0.4) the lists' bytes ahead of the project part, coded (OCJ2: 0) */
typedef struct {                               /* (0.3) OCJ2: as ocj_head_t to the lists */
    uint32_t magic, len, zlen;
    uint8_t pid[NTRK][NPAT];
    uint8_t active[NTRK];
    uint8_t chain[CHAIN_ROWS * NTRK];
    char name[PROJ_NAME_LEN];
    uint8_t fn[OCJ_FN];
    ocl_lists_v2_t lists;
} ocj_head2_t;

static struct {
    uint8_t pid[4][NTRK][NPAT];                /* each slot's map, as its record has it (all 0: no record) */
    uint32_t names[4][2];                      /* (0.4.4) each slot's phrases, a bit each: its banks' and its chains'
                                                * slots' (a phrase only a saved project's chain names is its too) */
} ocj __attribute__((section(".pool")));
static ocj_head_t ocj_h __attribute__((section(".pool")));
static ocp_gen_t ocj_gen[OCP_PER] __attribute__((section(".pool")));
static int ocj_read(uint32_t slot, uint8_t *raw);

/* how many saved slots (but slot `but`, 4: none) name pool pattern id: a bank of theirs, or (0.4.4) a slot of one of
 * their chains */
static uint32_t ocj_refs(uint32_t id, uint32_t but)
{
    uint32_t n = 0;
    if (id >= OCP_N) return 0;
    for (uint32_t s = 0; s < 4u; s++)
        if (s != but) n += (ocj.names[s][id >> 5] >> (id & 31u)) & 1u;
    return n;
}
/* (0.4.4) slot s's names: its map's phrases and, l given, its chains' */
static void ocj_names_set(uint32_t s, const uint8_t (*pid)[NPAT], const ocl_lists_t *l)
{
    s &= 3u;
    ocj.names[s][0] = ocj.names[s][1] = 0;
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++)
            if (pid[k][b] && pid[k][b] <= OCP_N) ocj.names[s][(pid[k][b] - 1u) >> 5] |= 1u << ((pid[k][b] - 1u) & 31u);
    for (uint32_t c = 0; l && c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++) {
            uint32_t v = l->chain[c][e][0];
            if (v && v <= OCP_N) ocj.names[s][(v - 1u) >> 5] |= 1u << ((v - 1u) & 31u);
        }
}

/* pool pattern id named by what is loaded (a bank's, a chain step's) or by the project being written (proj_pid,
 * proj_lists, when xs) */
static int ocj_named_now(uint32_t id, int xs)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < NPAT; b++)
            if (bank_pid[k][b] == id + 1u || (xs && proj_pid[k][b] == id + 1u)) return 1;
    for (uint32_t c = 0; c < OCL_NCHAIN; c++)
        for (uint32_t e = 0; e < OCL_NENT; e++)
            if (lists.chain[c][e][0] == id + 1u || (xs && proj_lists.chain[c][e][0] == id + 1u)) return 1;
    return 0;
}

/* (0.4) record r's head into ocj_h (an OCJ2's converted), *hsz its size in the record. 0, or -1 none that reads */
static int ocj_head_load(ocs_rec_t *r, uint32_t *hsz)
{
    static ocj_head2_t h2 __attribute__((section(".pool")));
    uint32_t magic;
    if (r->total < 4u || ocs_at(r, 0, &magic, 4)) return -1;
    if (magic == OCJ_MAGIC) {
        if (r->total < sizeof ocj_h || ocs_at(r, 0, &ocj_h, sizeof ocj_h)) return -1;
        *hsz = sizeof ocj_h;
        if (ocj_h.lsize > 16384u) return -1;
        ocj_lead = ocj_h.lsize;
        ocl_lists_init(&ocj_l);
    } else if (magic == OCJ_MAGIC2) {
        if (r->total < sizeof h2 || ocs_at(r, 0, &h2, sizeof h2)) return -1;
        *hsz = sizeof h2;
        memset(&ocj_h, 0, sizeof ocj_h);
        ocj_h.magic = OCJ_MAGIC; ocj_h.len = h2.len; ocj_h.zlen = h2.zlen;
        memcpy(ocj_h.pid, h2.pid, sizeof ocj_h.pid);
        memcpy(ocj_h.active, h2.active, sizeof ocj_h.active);
        memcpy(ocj_h.chain, h2.chain, sizeof ocj_h.chain);
        memcpy(ocj_h.name, h2.name, sizeof ocj_h.name);
        memcpy(ocj_h.fn, h2.fn, sizeof ocj_h.fn);
        ocl_lists_init(&ocj_l);                /* track k's list chain k + 1, row 01 naming them */
        for (uint32_t k = 0; k < NTRK; k++) {
            memcpy(ocj_l.chain[k], h2.lists.ent[k], sizeof h2.lists.ent[k]);
            ocj_l.cur[k] = h2.lists.cur[k];
        }
        ocj_l.mode = h2.lists.song ? OCL_CHAIN : OCL_PHRASE;
        ocl_lists_fix(&ocj_l);
        ocj_lead = 0;
    } else {
        return -1;
    }
    return ocj_h.len == r->total && ocj_h.zlen == r->total - *hsz ? 0 : -1;
}
/* (0.4.4) an OCJ3 record's lists alone (the first ocj_lead bytes of its coded part) into ocj_l, after ocj_head_load
 * (OCJ2: there already). 0, or -1 they do not read */
static int ocj_lists_decode(ocs_rec_t *r, uint32_t hsz)
{
    static ocp_rd_t rd __attribute__((section(".pool")));
    uint32_t out = 0;
    if (!ocj_lead) return 0;
    memset(&rd, 0, sizeof rd);
    rd.r = r; rd.off = hsz; rd.end = r->total;
    while (out < ocj_lead) {
        uint8_t v, n;
        if (ocp_rd(&rd, &v, 1)) return -1;
        if (v) {
            if (out < sizeof ocj_l) ((uint8_t *)&ocj_l)[out] = v;
            out++;
            continue;
        }
        if (ocp_rd(&rd, &n, 1) || !n) return -1;
        for (; n && out < ocj_lead; n--, out++)
            if (out < sizeof ocj_l) ((uint8_t *)&ocj_l)[out] = 0;
    }
    ocl_lists_fix(&ocj_l);
    return 0;
}

/* boot (after ocs_boot, ocp_scan): each slot's map */
static void ocj_scan(void)
{
    memset(&ocj, 0, sizeof ocj);
    for (uint32_t s = 0; s < 4u; s++) {
        ocs_rec_t r;
        uint32_t hsz;
        if (ocs_open(OCS_PROJECT, s, &r) || ocj_head_load(&r, &hsz))
            continue;
        memcpy(ocj.pid[s], ocj_h.pid, sizeof ocj.pid[s]);
        for (uint32_t k = 0; k < NTRK; k++)
            for (uint32_t b = 0; b < NPAT; b++)
                if (ocj.pid[s][k][b] > OCP_N) ocj.pid[s][k][b] = 0;
        ocj_names_set(s, ocj.pid[s], ocj_lists_decode(&r, hsz) ? 0 : &ocj_l);   /* (0.4.4) its chains' too */
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

/* the record streamed: the head, then the lists (ocj_l) and the project part (at raw + 8) run-length coded */
#define OCJ_CODED ((uint32_t)sizeof ocj_l + PROJ_STORE_SIZE)
typedef struct {
    ocs_src_t src;
    const uint8_t *in;
    uint32_t pos, hpos;
    int pending;                               /* a zero run's count still to give, -1 none */
} ocj_src_t;
static uint8_t ocj_in(const ocj_src_t *z, uint32_t pos)
{
    return pos < sizeof ocj_l ? ((const uint8_t *)&ocj_l)[pos] : z->in[pos - sizeof ocj_l];
}
static int ocj_rle_byte(ocj_src_t *z, uint8_t *out)
{
    if (z->pending >= 0) { *out = (uint8_t)z->pending; z->pending = -1; return 0; }
    if (z->pos >= OCJ_CODED) return -1;
    if (ocj_in(z, z->pos)) { *out = ocj_in(z, z->pos++); return 0; }
    uint32_t n = 0;
    while (n < 255u && z->pos < OCJ_CODED && !ocj_in(z, z->pos)) { n++; z->pos++; }
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
                if ((id = ocj.pid[slot][k][b]) != 0u && !ocj_refs(id - 1u, slot) && !ocj_named_now(id - 1u, 0)) want[id - 1u] = 255u;
        memset(ocj.pid[slot], 0, sizeof ocj.pid[slot]);
        ocj.names[slot][0] = ocj.names[slot][1] = 0;
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
            if (xs && proj_pid[k][b] && proj_pid[k][b] <= OCP_N) {   /* (4c) the pattern the bank is: shared as named */
                npid[k][b] = proj_pid[k][b];
                continue;
            }
            id = ocj.pid[slot][k][b];
            if (id && !ocj_refs(id - 1u, slot)) {   /* (its own, no other slot's: kept) */
                uint32_t dup = 0;
                for (uint32_t k2 = 0; k2 < NTRK; k2++)
                    for (uint32_t b2 = 0; b2 < NPAT; b2++) dup |= npid[k2][b2] == id;
                if (!dup) { npid[k][b] = (uint8_t)id; continue; }
            }
            for (id = 1; id <= OCP_N; id++) {       /* a free one: no slot names it, no bank here has it */
                uint32_t taken = ocj_refs(id - 1u, slot) || ocj_named_now(id - 1u, xs) || ocp.len[id - 1u];
                for (uint32_t k2 = 0; k2 < NTRK && !taken; k2++)
                    for (uint32_t b2 = 0; b2 < NPAT; b2++) taken |= npid[k2][b2] == id || ocj.pid[slot][k2][b2] == id;
                if (!taken) break;
            }
            if (id > OCP_N) {                       /* (none: try this slot's own let go, before giving up) */
                for (id = 1; id <= OCP_N; id++) {
                    uint32_t taken = ocj_refs(id - 1u, slot) || ocj_named_now(id - 1u, xs), own = 0;
                    for (uint32_t k2 = 0; k2 < NTRK; k2++)   /* (0.4.4: its own, or empty: never a phrase with notes
                                                              * nothing names, a phrase kept in the pool) */
                        for (uint32_t b2 = 0; b2 < NPAT; b2++) own |= ocj.pid[slot][k2][b2] == id;
                    taken |= !own && (ocp.len[id - 1u] || ocp.hash[id - 1u]);
                    for (uint32_t k2 = 0; k2 < NTRK && !taken; k2++)
                        for (uint32_t b2 = 0; b2 < NPAT; b2++) taken |= npid[k2][b2] == id;
                    if (!taken) break;
                }
                if (id > OCP_N) { ui_message("PHRASES FULL"); return 2; }
            }
            npid[k][b] = (uint8_t)id;
        }
    /* 2. the patterns that changed */
    memset(want, 0, sizeof want);
    for (k = 0; k < NTRK; k++)
        for (b = 0; b < NPAT; b++)
            if ((id = npid[k][b]) != 0u && ocp.hash[id - 1u] != hash[k][b] && !(xs && proj_pid[k][b] == id))
                want[id - 1u] = (uint8_t)(k * NPAT + b + 1u);   /* (4c: a bank the sequencer named was kept already
                                                                  * (oc_lists.c); its copy is not written back over a
                                                                  * newer one another track kept) */
    if ((rc = ocj_put_chunks(want, q, raw, xs)) != 0) return 2;
    /* 3. the record: the project part without its steps, timed notes and motion */
    memset(&ocj_h, 0, sizeof ocj_h);
    ocj_h.magic = OCJ_MAGIC;
    ocj_h.lsize = (uint32_t)sizeof ocj_l;
    memcpy(ocj_h.pid, npid, sizeof npid);
    memcpy(ocj_h.active, raw + BANK_ACTIVE_OFF, NTRK);
    memcpy(ocj_h.chain, raw + BANK_CHAIN_OFF, sizeof ocj_h.chain);
    memcpy(ocj_h.name, q->name, PROJ_NAME_LEN);
    memcpy(ocj_h.fn, raw + BANK_FN_OFF, OCJ_FN);
    if (xs) ocj_l = proj_lists;                /* (4c; a project from elsewhere: a new project's) */
    else ocl_lists_init(&ocj_l);
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
            if (!still && !ocj_refs(old - 1u, slot) && !ocj_named_now(old - 1u, xs)) want[old - 1u] = 255u;
        }
    memcpy(ocj.pid[slot], npid, sizeof npid);
    ocj_names_set(slot, ocj.pid[slot], &ocj_l);   /* (0.4.4) its chains as written */
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
    uint32_t k, b, i, at, out = 0, nmot = 0, nnote = 0, notes_at = 0, hsz;
    slot &= 3u;
    if (ocs_open(OCS_PROJECT, slot, &r) || ocj_head_load(&r, &hsz))
        return -1;
    /* the project part, decoded into raw + 8 */
    memset(&rd, 0, sizeof rd);
    rd.r = &r; rd.off = hsz; rd.end = r.total;
    for (at = 0; at < ocj_h.zlen;) {
        uint8_t v, n;
        if (ocp_rd(&rd, &v, 1)) return -1;
        at++;
        if (v) {
            if (out >= ocj_lead + PROJ_STORE_SIZE) return -1;
            if (out < ocj_lead) { if (out < sizeof ocj_l) ((uint8_t *)&ocj_l)[out] = v; out++; }   /* (0.4: the
                                                                                                    * lists first) */
            else { raw[8u + out - ocj_lead] = v; out++; }
            continue;
        }
        if (ocp_rd(&rd, &n, 1) || !n || out + n > ocj_lead + PROJ_STORE_SIZE) return -1;
        at++;
        for (; n; n--, out++) {
            if (out < ocj_lead) { if (out < sizeof ocj_l) ((uint8_t *)&ocj_l)[out] = 0; }
            else raw[8u + out - ocj_lead] = 0;
        }
    }
    if (out != ocj_lead + PROJ_STORE_SIZE || !proj_import(q, raw + 8u, PROJ_STORE_SIZE)) return -1;
    ocl_lists_fix(&ocj_l);
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
    memcpy(proj_pid, ocj_h.pid, sizeof proj_pid);    /* (4c) the patterns the banks are, the lists */
    proj_lists = ocj_l;                        /* (in range: ocl_lists_fix above) */
    proj_x_tag = proj_hash(raw, BANK_STORE_SIZE);
    return (int)BANK_STORE_SIZE;
}
