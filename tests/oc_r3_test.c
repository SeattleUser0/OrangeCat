/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format): host test of the project format revision's core, Part 1 (OC_R3_FORMAT=1):
 * firmware/src/oc_pool_r3.c (the project's chunks, the directory, the generation bit, the sweep) and oc_ocj4.c (the
 * OCJ4 record, its sections, the bytes carried, the metadata).
 *   build/host/oc_r3_test
 * On tests/oc_project_test.c's harness (a simulated NOR, power cuts after any program call and after any erase):
 *  1. a project with all 32 banks in use saves and loads back the same, after a reboot too; its record is at most
 *     three parts and holds the sections in the design's order.
 *  2. a save that changes nothing rewrites the record only; one bank changed rewrites its chunk under the other
 *     generation and the record, and the old generation is gone after; the autosave after a save erases nothing.
 *  3. 255 phrases keep, save and load back; the 256th is refused (no free number).
 *  4. a save into another slot copies: an edit saved in the copy leaves the source's bytes equal, and both read.
 *  5. a new project's keeps go to the scratch; a save moves them to a slot and the scratch is empty; a reboot sweeps a
 *     scratch left behind.
 *  6. a record carrying REND, TSTA, ROUT, FXRC and PERF content, an unknown section, bytes after HEAD's fields, after a
 *     TRKS entry's fields and a fifth TRKS entry loads, and a save and a save-as write every one of them back byte for
 *     byte; another project's record (another uid) carries nothing.
 *  7. the uid survives saves and the revision counts them; a profile written changes HEAD's bytes and no other section.
 *  8. SMPR: a reference the unit lacks says SAMPLE MISSING at the load and is written back unchanged.
 *  9. a version 1 phrase reads, its macro motion event dropped; the next keep writes it as version 2.
 * 10. a phrase that outgrows its chunk moves; the chunk it left drops its stale bytes when next written.
 * 11. a power cut after every program call and after every erase of a save of three changed phrases and of a save
 *     into another slot: after the reboot the slot reads entirely old or entirely new, and no chunk of either slot is
 *     an orphan.
 * 12. a full store refuses a save whole and leaves the slot readable; a keep refused leaves the reserve.
 * 13. an OLD FORMAT slot (the format before's record at its id) is never a free slot.
 * 14. a slot's records copied to another slot's ids read the same (no field names a slot).
 * 15. a SCNS of a version this firmware does not read is kept byte for byte, plays no scene and says so.
 *
 * Revisions:
 *   Part 1 (October 31, 2026): first version.
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

#define R3CHK(what, ok) (bad |= check(what, (ok)))

/* ------------------------------------------------------------ helpers --- */
static uint8_t rec[OCJ4_MAX + 4096u];
/* slot's record whole into rec: its length, -1 none */
static int rec_get(uint32_t slot) { return ocs_read(OCS_PROJECT, OCJ4_RID(slot), rec, sizeof rec); }
/* the sections of a record image: tags in order into tags[] (max n); their count */
static uint32_t rec_tags(const uint8_t *b, uint32_t len, uint32_t *tags, uint32_t n)
{
    uint32_t pos, c = 0;
    for (pos = OCJ4_HLEN; pos + 8u <= len && c < n; pos += 8u + rd32(b + pos + 4)) tags[c++] = rd32(b + pos);
    return c;
}
/* a section of a record image: its bytes' offset and length, or -1 */
static int rec_find(const uint8_t *b, uint32_t len, uint32_t tag, uint32_t *off, uint32_t *n)
{
    for (uint32_t pos = OCJ4_HLEN; pos + 8u <= len; pos += 8u + rd32(b + pos + 4))
        if (rd32(b + pos) == tag) { *off = pos + 8u; *n = rd32(b + pos + 4); return 0; }
    return -1;
}
/* bytes p (n) occur in the record image */
static int rec_has(const uint8_t *b, uint32_t len, const void *p, uint32_t n)
{
    for (uint32_t i = 0; i + n <= len; i++) if (!memcmp(b + i, p, n)) return 1;
    return 0;
}
/* a record image with section tag replaced by (p, n) (appended when absent), the head's length fixed. The new length */
static uint32_t r3rec_set(uint8_t *b, uint32_t len, uint32_t tag, const void *p, uint32_t n)
{
    uint32_t off, old;
    if (!rec_find(b, len, tag, &off, &old)) {
        memmove(b + off + n, b + off + old, len - off - old);
        memcpy(b + off, p, n);
        wr32(b + off - 4u, n);
        len = len - old + n;
    } else {
        wr32(b + len, tag); wr32(b + len + 4u, n); memcpy(b + len + 8u, p, n);
        len += 8u + n;
    }
    wr32(b + 8, len - OCJ4_HLEN);
    return len;
}
/* the record image written as slot's record */
static int rec_put(uint32_t slot, uint32_t len) { return ocs_write(OCS_PROJECT, OCJ4_RID(slot), rec, len); }
/* the chunk records of slot `slot` on the flash (by headers), and how many of them its record does not name */
static uint32_t pool_headers(uint32_t slot, uint32_t *orphans)
{
    ocp_dir_t d;
    uint8_t named[OCP_NCHUNK / 8u];
    uint32_t n = 0, o = 0;
    if (ocj4_phrd_read(slot, &d, named, 0, 0)) memset(named, 0, sizeof named);
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *h = &ocs_hdr[s];
        uint32_t c, g;
        if (h->magic != OCS_MAGIC || h->type != OCS_POOL || !(h->id & 0x8000u) || ((h->id >> 9) & 7u) != slot) continue;
        c = h->id & 255u; g = (h->id >> 8) & 1u;
        n++;
        if (!(c && ocp_bit(named, c) && ocp_bit(d.gen, c) == g)) o++;
    }
    if (orphans) *orphans = o;
    return n;
}
/* every current record's (type, id, seq) as a sorted list, to see what a save touched */
typedef struct { uint32_t n; uint32_t key[OCS_NSECT]; } recset_t;
static void recset(recset_t *r)
{
    r->n = 0;
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        const ocs_hdr_t *h = &ocs_hdr[s];
        if (h->magic != OCS_MAGIC) continue;
        r->key[r->n++] = (uint32_t)h->type << 24 | h->id << 8 | (h->seq & 255u);
    }
    for (uint32_t i = 1; i < r->n; i++)
        for (uint32_t j = i; j && r->key[j - 1u] > r->key[j]; j--) { uint32_t t = r->key[j]; r->key[j] = r->key[j - 1u]; r->key[j - 1u] = t; }
}
static int recset_same(const recset_t *a, const recset_t *b) { return a->n == b->n && !memcmp(a->key, b->key, a->n * sizeof a->key[0]); }
/* one step of track k's bank b changed (a note), so its phrase differs */
static void poke(uint32_t k, uint32_t b, uint32_t seed)
{
    pattern_t p;
    pattern_read(k, b, &p);
    p.step[seed % NSTEP].n = 1; p.step[seed % NSTEP].time = ST_NOTE;
    p.step[seed % NSTEP].note[0] = (uint8_t)(36u + seed % 60u); p.step[seed % NSTEP].vel = (uint8_t)(50u + seed % 70u);
    pattern_write(k, b, &p);
}
static uint32_t rec_parts(uint32_t slot)
{
    ocs_rec_t r;
    return ocs_open(OCS_PROJECT, OCJ4_RID(slot), &r) ? 0u : r.parts;
}

/* ------------------------------------------------------------ 1, 2 --- */
static int test_round_trip_r3(void)
{
    int bad = 0;
    uint32_t tags[32], nt, before, orphans;
    recset_t s0, s1;
    static const uint32_t ORDER[] = {T_HEAD, T_GLOB, T_TRKS, T_NATV, T_MCHN, T_LIST, T_SCNS, T_PERF, T_PHRD, T_FRZR, T_SMPR,
                                     T_FXRC, T_REND, T_TSTA, T_ROUT};
    reset();
    fill_all(1, NPAT);
    trk[2].pattern = 0;
    snap(&A);
    R3CHK("a project with all 32 banks in use saves (project-owned phrases)", project_save_as(0, "ALL BANKS") == 0 && !strcmp(ui.msg, "SAVED"));
    R3CHK("... its banks are 32 phrases of the live pool, the pool is slot A's", used_patterns() == 32u && ocp.dir.slot == 0u);
    {
        int n = rec_get(0);
        nt = n > 0 ? rec_tags(rec, (uint32_t)n, tags, 32) : 0u;
        char what[120];
        snprintf(what, sizeof what, "... its record is %d bytes, %u parts (at most 3), the %u sections in the design's order", n, rec_parts(0), nt);
        R3CHK(what, n > 0 && rec_parts(0) <= 3u && nt == sizeof ORDER / sizeof ORDER[0] && !memcmp(tags, ORDER, sizeof ORDER));
    }
    R3CHK("... no chunk of the slot is an orphan", pool_headers(0, &orphans) > 0u && orphans == 0u);
    wipe_runtime();
    project_load(0);
    snap(&B);
    R3CHK("... loads back the same: every bank, Skiff's fields, motion, timed notes", same(&A, &B));
    reboot();
    wipe_runtime();
    {
        char n[16];
        R3CHK("after a reboot the slot list has its name", project_name(0, n) && !strcmp(n, "ALL BANKS"));
    }
    project_load(0);
    snap(&B);
    R3CHK("... and it loads the same", same(&A, &B));
    R3CHK("... the live pool is the slot's again, with its 32 phrases", ocp.dir.slot == 0u && used_patterns() == 32u);
    recset(&s0);
    before = erases;
    R3CHK("a save that changes nothing rewrites the record only", project_save(0) == 0 && erases - before == rec_parts(0));
    recset(&s1);
    {   /* the chunk records are the same copies: only the project record's seq moved */
        uint32_t same_chunks = 1;
        for (uint32_t i = 0; i < s0.n; i++)
            if ((s0.key[i] >> 24) == OCS_POOL) {
                uint32_t found = 0;
                for (uint32_t j = 0; j < s1.n; j++) found |= s1.key[j] == s0.key[i];
                same_chunks &= found;
            }
        R3CHK("... every chunk record is the copy it was", same_chunks);
    }
    before = erases;
    project_autosave();
    R3CHK("... and the autosave after it erases nothing", erases == before);
    {
        uint32_t id = bank_pid[3][5] - 1u, c, g0, g1;
        c = ocp.dir.chunk[id]; g0 = ocp_bit(ocp.dir.gen, c);
        poke(3, 5, 7);
        before = erases;
        R3CHK("changing one bank rewrites its chunk and the record", project_save(0) == 0 && erases - before >= 1u + rec_parts(0) && erases - before <= 3u + rec_parts(0));
        g1 = ocp_bit(ocp.dir.gen, c);
        R3CHK("... the chunk is under the other generation, the old generation's record is gone",
              ocp.dir.chunk[id] == c && g1 != g0 && !ocs_exists(OCS_POOL, ocp_rid(0, g0, c)) && ocs_exists(OCS_POOL, ocp_rid(0, g1, c)));
        R3CHK("... and the committed bits follow the live ones", !memcmp(ocp.cgen, ocp.dir.gen, sizeof ocp.cgen));
        snap(&A);
        reboot(); wipe_runtime(); project_load(0); snap(&B);
        R3CHK("... it loads back after a reboot", same(&A, &B) && pool_headers(0, &orphans) && !orphans);
    }
    return bad;
}

/* ------------------------------------------------------------ 3 --- */
static int test_255(void)
{
    int bad = 0;
    uint32_t id, ok = 1, hash[OCP_N], free_before, orphans;
    reset();
    fill_all(2, 2);
    R3CHK("a project saves", project_save_as(1, "MANY") == 0);
    for (id = 0; id < OCP_N && ok; id++) {     /* each phrase a kept bank, small (two notes), varied */
        pattern_t p;
        pattern_read(0, 3, &p);
        for (uint32_t i = 0; i < NSTEP; i++) { memset(&p.step[i], 0, sizeof p.step[i]); p.step[i].time = ST_REST; }   /* (a step not in use) */
        pattern_write(0, 3, &p);
        poke(0, 3, id * 7u + 3u);
        poke(0, 3, id * 11u + 40u);
        ok &= ocp_save(id, 0, 3) == 0;
        hash[id] = ocp.hash[id];
        ok &= hash[id] != 0u && ocp.dir.chunk[id] != 0u;
    }
    R3CHK("255 phrases keep into the live pool", ok && id == OCP_N);
    R3CHK("... a 256th has no number", ocl_free_pat() == OCP_N);
    {   /* the directory's bookkeeping agrees with the chunks on the flash */
        uint32_t sum = 0, chunks = 0;
        for (uint32_t c = 1; c < OCP_NCHUNK; c++) if (ocp.used[c]) { chunks++; sum += ocp.used[c]; }
        ok = chunks > 0;
        for (uint32_t c = 1; c < OCP_NCHUNK && ok; c++) {
            ocs_rec_t r;
            if (!ocp.used[c]) continue;
            ok &= ocs_open(OCS_POOL, ocp_rid(ocp.dir.slot, ocp_bit(ocp.dir.gen, c), c), &r) == 0 && ocp_tab_read(&r, &ocp_otab) == 0;
            if (ok) {                          /* used[c] is the table plus its live phrases (stale ones not counted) */
                uint32_t u = 1;
                for (uint32_t e = 0; e < ocp_otab.n; e++) if (ocp.dir.chunk[ocp_otab.id[e] - 1u] == c) u += 3u + ocp_otab.len[e];
                if (u != ocp.used[c]) fprintf(stderr, "  chunk %u: used %u, its table says %u (%u entries)\n", c, ocp.used[c], u, ocp_otab.n);
                ok &= u == ocp.used[c];
            } else fprintf(stderr, "  chunk %u does not open\n", c);
        }
        {   /* bank 3 emptied again: the save that follows has nothing to keep */
            pattern_t p;
            pattern_read(0, 3, &p);
            for (uint32_t i = 0; i < NSTEP; i++) { memset(&p.step[i], 0, sizeof p.step[i]); p.step[i].time = ST_REST; }
            pattern_write(0, 3, &p);
        }
        char what[120];
        snprintf(what, sizeof what, "... in %u chunks (%u B live), each chunk's used bytes as its table says", chunks, sum);
        R3CHK(what, ok);
    }
    free_before = ocs_free_count();
    R3CHK("... the project saves (its directory names them all)", project_save(1) == 0 && ocs_free_count() + 1u >= free_before);
    reboot(); wipe_runtime(); project_load(1);
    ok = ocp.dir.slot == 1u;
    for (id = 0; id < OCP_N && ok; id++) ok &= ocp.hash[id] == hash[id] && ocp.dir.chunk[id] != 0u && ocp_load(id, 0, 3, 0) == 0;
    R3CHK("... after a reboot all 255 read back with their CRCs, none an orphan", ok && pool_headers(1, &orphans) && !orphans);
    return bad;
}

/* ------------------------------------------------------------ 4, 5 --- */
static int test_copy_and_scratch(void)
{
    int bad = 0;
    uint32_t orphans, n4;
    snap_t SA, SB;
    recset_t a0, a1;
    reset();
    fill_all(3, NPAT);
    R3CHK("slot A saved", project_save_as(0, "SOURCE") == 0);
    snap(&SA);
    recset(&a0);
    R3CHK("save-as into slot B copies every phrase; the live pool is B's", project_save_as(1, "COPY") == 0 && ocp.dir.slot == 1u && used_patterns() == 32u);
    R3CHK("... A's records are untouched (every one the copy it was)", (recset(&a1), 1) && ({ uint32_t ok = 1; for (uint32_t i = 0; i < a0.n; i++) { uint32_t f = 0; for (uint32_t j = 0; j < a1.n; j++) f |= a1.key[j] == a0.key[i]; ok &= f; } ok; }));
    R3CHK("... neither slot has an orphan", pool_headers(0, &orphans) && !orphans && pool_headers(1, &orphans) && !orphans);
    poke(1, 2, 99);
    R3CHK("an edit saved in B", project_save(1) == 0);
    snap(&SB);
    wipe_runtime(); project_load(0); snap(&A);
    R3CHK("... leaves A as it was", same(&A, &SA) && !same(&A, &SB));
    wipe_runtime(); project_load(1); snap(&B);
    R3CHK("... and B holds the edit", same(&B, &SB));
    /* the scratch */
    project_new_blank();
    R3CHK("a new project's pool is the scratch, empty", ocp.dir.slot == OCP_SCRATCH && used_patterns() == 0u && !pool_headers(OCP_SCRATCH, 0));
    fill_all(4, 3);
    R3CHK("... its keeps go to the scratch's chunks", ocl_keep_all() == 0 && pool_headers(OCP_SCRATCH, 0) > 0u && used_patterns() == 12u);
    n4 = pool_headers(OCP_SCRATCH, 0);
    R3CHK("... a save moves them to the slot and the scratch is swept", project_save_as(2, "NEW") == 0 && ocp.dir.slot == 2u && !pool_headers(OCP_SCRATCH, 0) && pool_headers(2, &orphans) >= 1u && !orphans && n4);
    snap(&A); reboot(); wipe_runtime(); project_load(2); snap(&B);
    R3CHK("... and it loads back", same(&A, &B));
    project_new_blank();
    fill_all(5, 2);
    R3CHK("a new project's keeps left unsaved", ocl_keep_all() == 0 && pool_headers(OCP_SCRATCH, 0) > 0u);
    reboot();
    R3CHK("... are swept at the next boot (unsaved music dies with the power)", !pool_headers(OCP_SCRATCH, 0) && pool_headers(2, &orphans) && !orphans);
    return bad;
}

/* ------------------------------------------------------------ 6, 7, 8, 15 --- */
static const uint8_t FOREIGN_REND[] = {0x02, 0x00, 0x11, 0x00, 7, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0x05, 0x00, 'r', 'e', 'n', 'd', '!'};
static const uint8_t FOREIGN_TSTA[] = {0x01, 0x00, 0x10, 0x00, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint8_t FOREIGN_ROUT[] = {0x01, 0x00, 0x0A, 0x00, 'R', 'O', 'U', 'T', 'E', 'D', '!', '!', '!', '!'};
static const uint8_t FOREIGN_FXRC[] = {0x01, 0x00, 0x06, 0x00, 'f', 'x', 'r', 'c', 0, 1};
static const uint8_t FOREIGN_PERF[] = {4, 1, 2, 3, 4, 2, 5, 6, 7};
static const uint8_t FOREIGN_ZZZZ[] = {'u', 'n', 'k', 'n', 'o', 'w', 'n', ' ', 's', 'e', 'c', 't', 'i', 'o', 'n'};
static const uint8_t HEAD_TAIL[] = {0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7};
static const uint8_t TRK_TAIL[] = {0xB1, 0xB2, 0xB3, 0xB4, 0xB5};
static const uint8_t TRK_FIFTH[] = {0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8};
static const uint8_t FRZR_FAR[] = {1, 'O', 'R', 'A', 'N', 'G', 'E', 'C', 'A', 'T', '/', 'F', 'A', 'R', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                   0x01, 0x00, 33, 0, 70, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t SMPR_X[] = {0x01, 0x00, 24, 0, 1, 'X', 'S', 'M', 'P', 0, 0, 0, 0, 0, 0, 0, 0, 0x10, 0x27, 0, 0, 0xEF, 0xBE, 0xAD, 0xDE, 0x01, 0x03, 0x00};
/* the record in rec (len) given every foreign piece; the new length */
static uint32_t rec_make_foreign(uint32_t len)
{
    uint32_t off, n;
    static uint8_t sec[8192];
    /* HEAD: its fields then a tail */
    if (!rec_find(rec, len, T_HEAD, &off, &n)) { memcpy(sec, rec + off, n); memcpy(sec + n, HEAD_TAIL, sizeof HEAD_TAIL); len = r3rec_set(rec, len, T_HEAD, sec, n + sizeof HEAD_TAIL); }
    /* TRKS: entry 0 with a tail, a fifth entry */
    if (!rec_find(rec, len, T_TRKS, &off, &n)) {
        uint32_t e0 = rd16(rec + off + 2u), at = 0;
        wr16(sec, 5);
        at = 2;
        wr16(sec + at, e0 + sizeof TRK_TAIL); at += 2;
        memcpy(sec + at, rec + off + 4u, e0); at += e0;
        memcpy(sec + at, TRK_TAIL, sizeof TRK_TAIL); at += sizeof TRK_TAIL;
        memcpy(sec + at, rec + off + 4u + e0, n - 4u - e0); at += n - 4u - e0;   /* entries 1..3 */
        wr16(sec + at, sizeof TRK_FIFTH); at += 2;
        memcpy(sec + at, TRK_FIFTH, sizeof TRK_FIFTH); at += sizeof TRK_FIFTH;
        len = r3rec_set(rec, len, T_TRKS, sec, at);
    }
    len = r3rec_set(rec, len, T_REND, FOREIGN_REND, sizeof FOREIGN_REND);
    len = r3rec_set(rec, len, T_TSTA, FOREIGN_TSTA, sizeof FOREIGN_TSTA);
    len = r3rec_set(rec, len, T_ROUT, FOREIGN_ROUT, sizeof FOREIGN_ROUT);
    len = r3rec_set(rec, len, T_FXRC, FOREIGN_FXRC, sizeof FOREIGN_FXRC);
    len = r3rec_set(rec, len, T_PERF, FOREIGN_PERF, sizeof FOREIGN_PERF);
    len = r3rec_set(rec, len, T_FRZR, FRZR_FAR, sizeof FRZR_FAR);
    len = r3rec_set(rec, len, T_SMPR, SMPR_X, sizeof SMPR_X);
    len = r3rec_set(rec, len, OCJ4_TAG('Z', 'Z', 'Z', 'Z'), FOREIGN_ZZZZ, sizeof FOREIGN_ZZZZ);
    return len;
}
/* every foreign piece is in the record image, byte for byte, in its section */
#define FK(c) do { if (!(c)) { fprintf(stderr, "  foreign piece not kept: %s\n", #c); ok = 0; } } while (0)
static int rec_foreign_kept(uint32_t len)
{
    uint32_t off, n, e0;
    int ok = 1;
    FK(!rec_find(rec, len, T_REND, &off, &n) && n == sizeof FOREIGN_REND && !memcmp(rec + off, FOREIGN_REND, n));
    FK(!rec_find(rec, len, T_TSTA, &off, &n) && n == sizeof FOREIGN_TSTA && !memcmp(rec + off, FOREIGN_TSTA, n));
    FK(!rec_find(rec, len, T_ROUT, &off, &n) && n == sizeof FOREIGN_ROUT && !memcmp(rec + off, FOREIGN_ROUT, n));
    FK(!rec_find(rec, len, T_FXRC, &off, &n) && n == sizeof FOREIGN_FXRC && !memcmp(rec + off, FOREIGN_FXRC, n));
    FK(!rec_find(rec, len, T_PERF, &off, &n) && n == sizeof FOREIGN_PERF && !memcmp(rec + off, FOREIGN_PERF, n));
    FK(!rec_find(rec, len, OCJ4_TAG('Z', 'Z', 'Z', 'Z'), &off, &n) && n == sizeof FOREIGN_ZZZZ && !memcmp(rec + off, FOREIGN_ZZZZ, n));
    FK(!rec_find(rec, len, T_HEAD, &off, &n) && n == OCJ4_HEAD_KNOWN + sizeof HEAD_TAIL && !memcmp(rec + off + OCJ4_HEAD_KNOWN, HEAD_TAIL, sizeof HEAD_TAIL));
    if (!rec_find(rec, len, T_TRKS, &off, &n)) {
        FK(rd16(rec + off) == 5u);
        e0 = rd16(rec + off + 2u);
        FK(e0 == OCJ4_TRK_KNOWN + sizeof TRK_TAIL && !memcmp(rec + off + 4u + OCJ4_TRK_KNOWN, TRK_TAIL, sizeof TRK_TAIL));
        FK(rec_has(rec + off, n, TRK_FIFTH, sizeof TRK_FIFTH));
    } else FK(0);
    FK(!rec_find(rec, len, T_FRZR, &off, &n) && rec_has(rec + off, n, FRZR_FAR + 1, 32) && rec_has(rec + off, n, FRZR_FAR + 35, 35));   /* the folder, the id-70 entry */
    FK(!rec_find(rec, len, T_SMPR, &off, &n) && n >= sizeof SMPR_X && !memcmp(rec + off, SMPR_X, sizeof SMPR_X));
    return ok;
}
static int test_foreign(void)
{
    int bad = 0, n = 0;
    uint32_t len, uid, rev, off, sn;
    reset();
    fill_all(6, 3);
    R3CHK("a project saved", project_save_as(0, "GATE ONE") == 0 && (n = rec_get(0)) > 0);
    len = rec_make_foreign((uint32_t)n);
    R3CHK("... its record rewritten with REND, TSTA, ROUT, FXRC, PERF content, an unknown section, tails after HEAD and a TRKS entry, a fifth TRKS entry, an FRZR entry above 64, an SMPR reference",
          rec_put(0, len) == 0 && (n = rec_get(0)) == (int)len && rec_foreign_kept(len));
    reboot(); wipe_runtime();
    ui.msg[0] = 0;
    project_load(0);
    R3CHK("... loads (the sample reference the unit lacks: SAMPLE MISSING T1, the track's state missing)",
          ocp.dir.slot == 0u && ocj4_meta.nsmp == 1u && ocj4_meta.smp_state[0] == SMP_MISSING && !strcmp(ui.msg, "SAMPLE MISSING T1"));
    uid = ocj4_meta.uid; rev = ocj4_meta.rev;
    poke(0, 0, 5);
    R3CHK("a save after it", project_save(0) == 0 && (n = rec_get(0)) > 0);
    R3CHK("... writes every foreign byte back (sections, tails, entries, the id-70 freeze, the reference unchanged)", rec_foreign_kept((uint32_t)n));
    R3CHK("... the uid is kept and the revision is one more", ocj4_meta.uid == uid && ocj4_meta.rev == rev + 1u);
    R3CHK("a save into another slot carries them too", project_save_as(2, "GATE TWO") == 0 && (n = rec_get(2)) > 0 && rec_foreign_kept((uint32_t)n) && ocj4_meta.uid == uid);
    {   /* a profile: HEAD's bytes change and no other section's */
        static uint8_t before[OCJ4_MAX];
        uint32_t blen = (uint32_t)n, off2, n2, ok = 1;
        memcpy(before, rec, blen);
        memcpy(ocj4_meta.prof, "STAGE RIG", 9); ocj4_meta.prof_id = 0x5150u;
        R3CHK("a profile set and saved", project_save(2) == 0 && (n = rec_get(2)) > 0);
        R3CHK("... comes back", (reboot(), wipe_runtime(), project_load(2), ocj4_meta.prof_id == 0x5150u && !memcmp(ocj4_meta.prof, "STAGE RIG", 9)));
        for (uint32_t pos = OCJ4_HLEN; pos + 8u <= blen; pos += 8u + rd32(before + pos + 4)) {
            uint32_t tag = rd32(before + pos);
            if (tag == T_HEAD) continue;
            ok &= !rec_find(rec, (uint32_t)n, tag, &off2, &n2) && !rec_find(before, blen, tag, &off, &sn) && n2 == sn && !memcmp(rec + off2, before + off, sn);
        }
        R3CHK("... and every section but HEAD is byte for byte as before", ok);
    }
    /* another project's record in the slot the music came from: nothing of it is carried */
    reset();
    fill_all(7, 2);
    R3CHK("project P saved into slot 3", project_save_as(3, "P") == 0 && (n = rec_get(3)) > 0);
    len = rec_make_foreign((uint32_t)n);
    R3CHK("... its record given the foreign pieces", rec_put(3, len) == 0);
    fill_all(8, 2);
    R3CHK("other music Q (another uid) saved into slot 3 over it", (project_new_blank(), fill_all(8, 2), project_save_as(3, "Q") == 0) && (n = rec_get(3)) > 0);
    R3CHK("... carries none of P's foreign bytes", !rec_find(rec, (uint32_t)n, T_REND, &off, &sn) && sn == 0u && rec_find(rec, (uint32_t)n, OCJ4_TAG('Z', 'Z', 'Z', 'Z'), &off, &sn) < 0);
    return bad;
}

/* ------------------------------------------------------------ 9, 10 --- */
static int test_phrase_bytes(void)
{
    int bad = 0, n = 0;
    uint32_t len, off, sn, id = 99, c = 200;
    reset();
    fill_all(9, 2);
    R3CHK("a project saved", project_save_as(0, "V1") == 0 && (n = rec_get(0)) > 0);
    {   /* a version 1 phrase (one step, one motion event on a macro id, no notes) as a chunk of its own */
        uint8_t ph[OCP_HEAD + OCP_STEP + 1u + 4u + 2u], chunk[4u + sizeof ph], e[OCJ4_PHRD_ENT];
        step_snapshot_t v;
        step_t st;
        uint32_t plen = sizeof ph;
        memset(ph, 0, sizeof ph);
        ph[0] = 1; ph[1] = (uint8_t)P_COUNT; ph[2] = 1; ph[3] = 0;
        ocp_put16(ph + 4, 16); ocp_put16(ph + 6, 1); ocp_put16(ph + 8, 0); ocp_put16(ph + 10, 50);
        ph[12] = 1;                            /* step 0 in use */
        memset(&st, 0, sizeof st); st.n = 1; st.time = ST_NOTE; st.note[0] = 60; st.vel = 100;
        step_snapshot_put(&v, &st); memcpy(ph + OCP_HEAD, &v, OCP_STEP);
        ph[OCP_HEAD + OCP_STEP] = 1;           /* one motion event: step 0, P_MAC0, value 5 */
        ph[OCP_HEAD + OCP_STEP + 1u] = 0; ph[OCP_HEAD + OCP_STEP + 2u] = (uint8_t)P_MAC0; ocp_put16(ph + OCP_HEAD + OCP_STEP + 3u, 5);
        ocp_put16(ph + OCP_HEAD + OCP_STEP + 5u, 0);
        chunk[0] = 1; chunk[1] = (uint8_t)(id + 1u); ocp_put16(chunk + 2, plen); memcpy(chunk + 4, ph, plen);
        R3CHK("a version 1 phrase written as chunk 200 of the slot", ocs_write(OCS_POOL, ocp_rid(0, 0, c), chunk, sizeof chunk) == 0);
        /* the record's PHRD names it */
        R3CHK("... the directory names it", !rec_find(rec, (uint32_t)n, T_PHRD, &off, &sn));
        {
            static uint8_t sec[4096];
            uint32_t cnt = rec[off], at;
            memcpy(sec, rec + off, 1u + cnt * OCJ4_PHRD_ENT);
            at = 1u + cnt * OCJ4_PHRD_ENT;
            e[0] = (uint8_t)(id + 1u); e[1] = (uint8_t)c; e[2] = 16; wr32(e + 3, 0x12345678u); wr16(e + 7, 0);
            memcpy(sec + at, e, OCJ4_PHRD_ENT); at += OCJ4_PHRD_ENT;
            memcpy(sec + at, rec + off + 1u + cnt * OCJ4_PHRD_ENT, OCP_NCHUNK / 8u); at += OCP_NCHUNK / 8u;
            sec[0] = (uint8_t)(cnt + 1u);
            len = r3rec_set(rec, (uint32_t)n, T_PHRD, sec, at);
            R3CHK("... the record rewritten", rec_put(0, len) == 0);
        }
    }
    reboot(); wipe_runtime(); project_load(0);
    {
        uint32_t mc = motion.count, mac = 0;
        int rc = ocp_load(id, 1, 4, 1);
        for (uint32_t i = 0; i < motion.count; i++) mac += motion.event[i].param == P_MAC0;
        R3CHK("the version 1 phrase reads: its LEN, its one step; the macro motion event dropped (no event of it lands)",
              ocp.len[id] == 16u && ocp.dir.chunk[id] == c && rc == 0 && motion.count == mc && !mac);
    }
    {
        pattern_t p;
        pattern_read(1, 4, &p);
        R3CHK("... the step is there", p.step[0].time == ST_NOTE && p.step[0].note[0] == 60u && p.timing[0] == 16);
    }
    {
        uint8_t b[2];
        ocs_rec_t r;
        ocp_rd_t rd;
        R3CHK("the next keep writes it as version 2", ocp_save(id, 1, 4) == 0 && ocp_r3_open(&ocp.dir, id, &r, &rd) == 1 &&
              !ocs_at(&r, rd.off, b, 2) && b[0] == 2u && ocp_bit(ocp.dir.gen, ocp.dir.chunk[id]) == 1u);
    }
    /* 10: a phrase that outgrows its chunk moves; its stale bytes go at the next rewrite */
    {
        uint32_t a = bank_pid[0][0] - 1u, bb = bank_pid[0][1] - 1u, ca = ocp.dir.chunk[a], cb = ocp.dir.chunk[bb], before;
        pattern_t p;
        R3CHK("two small phrases share a chunk", ca && ca == cb);
        before = ocp.used[ca];
        pattern_read(0, 0, &p);
        for (uint32_t i = 0; i < NSTEP; i++) { p.step[i].n = 4; p.step[i].time = ST_NOTE; p.step[i].note[0] = (uint8_t)(30u + i); p.step[i].note[1] = 50; p.step[i].note[2] = 60; p.step[i].note[3] = 70; p.step[i].vel = 100; }
        pattern_write(0, 0, &p);
        for (uint32_t i = 0; i < 300u; i++) {  /* 300 timed notes: about 3,700 B of phrase */
            recorded_note_t *r = &recording[i];
            r->on = (uint16_t)(i * 100u); r->duration = 300; r->note = (uint8_t)(40u + i % 40u); r->vel = 90; r->owner = 0; r->step = (uint8_t)(i % 64u); r->pitch = 0; r->length = 0;
        }
        recording_reindex();
        R3CHK("the first grows past what the chunk can hold with the other: it is kept", ocp_save(a, 0, 0) == 0);
        R3CHK("... and moved to a chunk of its own; the other stays; the old chunk's used bytes count it out",
              ocp.dir.chunk[a] != ca && ocp.dir.chunk[bb] == cb && ocp.used[cb] < before);
        {
            ocs_rec_t r;
            uint32_t total_before = 0;
            R3CHK("... the old chunk still holds its stale bytes", ocs_open(OCS_POOL, ocp_rid(0, ocp_bit(ocp.dir.gen, cb), cb), &r) == 0 && (total_before = r.total) > ocp.used[cb]);
            poke(0, 1, 123);
            R3CHK("a keep of the other rewrites the old chunk without them", ocp_save(bb, 0, 1) == 0 &&
                  ocs_open(OCS_POOL, ocp_rid(0, ocp_bit(ocp.dir.gen, cb), cb), &r) == 0 && r.total == ocp.used[cb] && r.total < total_before);
        }
    }
    R3CHK("... and the project saves and reloads", project_save(0) == 0 && (snap(&A), reboot(), wipe_runtime(), project_load(0), snap(&B), same(&A, &B)));
    return bad;
}

/* ------------------------------------------------------------ 11 --- */
static uint8_t nor_saved[sizeof nor];
/* the store as it is kept aside; the music of slot `slot` loaded with three banks changed; the save cut at point
 * (programs, erases); after the reboot the slot is old (A) or new (B) and no slot has an orphan. 1 good */
static int cut_round(uint32_t slot, uint32_t dst, int programs, int erases_cut, const snap_t *old, const snap_t *nw, int *rc_out)
{
    uint32_t orphans0, orphans1;
    int rc, ok;
    memcpy(nor, nor_saved, sizeof nor);
    reboot(); wipe_runtime(); project_load(slot);
    poke(0, 0, 11); poke(1, 1, 22); poke(2, 2, 33);
    fail_after = programs; erase_cut = erases_cut;
    rc = dst == slot ? project_save(slot) : project_save_as(dst, "CUT");
    fail_after = -1; erase_cut = -1;
    *rc_out = rc;
    reboot(); wipe_runtime();
    project_load(dst);
    snap(&C);
    ok = same(&C, nw) || (dst == slot ? same(&C, old) : !project_used(dst));
    if (dst != slot) { wipe_runtime(); project_load(slot); snap(&C); ok &= same(&C, old); }
    pool_headers(slot, &orphans0); pool_headers(dst, &orphans1);
    return ok && !orphans0 && !orphans1 && !pool_headers(OCP_SCRATCH, 0);
}
static int test_cuts_r3(void)
{
    int bad = 0, rc, ok = 1, n = 0, m = 0;
    uint32_t progs, ers;
    char what[200];
    reset();
    fill_all(10, NPAT);
    R3CHK("a project saved into slot A", project_save_as(0, "CUTS") == 0);
    snap(&A);
    memcpy(nor_saved, nor, sizeof nor);
    /* how many program calls and erases a clean save of three changed phrases takes */
    poke(0, 0, 11); poke(1, 1, 22); poke(2, 2, 33);
    {
        uint32_t e0 = erases;
        int p0;
        fail_after = 100000; project_save(0); progs = (uint32_t)(100000 - fail_after); fail_after = -1;
        ers = erases - e0;
        (void)p0;
    }
    snap(&B);
    snprintf(what, sizeof what, "a save of three changed phrases: %u program calls, %u erases (the record and three chunks under the other generation)", progs, ers);
    R3CHK(what, progs > 20u && ers >= 1u + 3u);
    for (int k = 0; k <= (int)progs && ok; k++) { ok &= cut_round(0, 0, k, -1, &A, &B, &rc); n++; }
    for (int k = 0; k < (int)ers && ok; k++) { ok &= cut_round(0, 0, -1, k, &A, &B, &rc); m++; }
    snprintf(what, sizeof what, "power cut at each of %d program points and %d erase points of the save: the slot reads old or new, no orphan after the boot", n, m);
    R3CHK(what, ok);
    /* the case the review found: a cut before the record, after the first changed chunk: the old record reads old data */
    R3CHK("... a cut after the first chunk's commit and before the record: the old record reads the old phrases",
          cut_round(0, 0, 20, -1, &A, &B, &rc) && rc != 0 && (memcpy(nor, nor_saved, sizeof nor), reboot(), wipe_runtime(), project_load(0), snap(&C), same(&C, &A)));
    /* a save-as */
    memcpy(nor, nor_saved, sizeof nor);
    reboot(); wipe_runtime(); project_load(0);
    poke(0, 0, 11); poke(1, 1, 22); poke(2, 2, 33);
    snap(&B);
    { uint32_t e0 = erases; fail_after = 100000; project_save_as(1, "CUT"); progs = (uint32_t)(100000 - fail_after); fail_after = -1; ers = erases - e0; }
    ok = 1; n = m = 0;
    for (int k = 0; k <= (int)progs && ok; k += 3) { ok &= cut_round(0, 1, k, -1, &A, &B, &rc); n++; }
    for (int k = 0; k < (int)ers && ok; k++) { ok &= cut_round(0, 1, -1, k, &A, &B, &rc); m++; }
    snprintf(what, sizeof what, "power cut at %d program points (every third of %u) and %d erase points of a save into another slot: B whole or absent, A untouched, no orphan", n, progs, m);
    R3CHK(what, ok);
    return bad;
}

/* ------------------------------------------------------------ 12, 13, 14 --- */
static int test_full_old_copy(void)
{
    int bad = 0, n = 0;
    uint32_t id, free_min = 999, k = 0;
    int rc = 0;
    reset();
    fill_all(11, 2);
    R3CHK("a project saved", project_save_as(0, "FULL") == 0);
    snap(&A);
    {   /* big phrases until the store is full: each keep leaves the reserve. Bank 1 is the source and is put back after,
         * so that the save that follows has nothing to keep */
        static recorded_note_t rec_was[RECORD_MAX];
        pattern_t p, was;
        pattern_read(0, 1, &was);
        for (uint32_t i = 0; i < RECORD_MAX; i++) rec_was[i] = recording_snapshot(i);
        pattern_read(0, 1, &p);
        for (uint32_t i = 0; i < NSTEP; i++) { p.step[i].n = 4; p.step[i].time = ST_NOTE; p.step[i].note[0] = (uint8_t)(30u + i); p.step[i].vel = 100; }
        pattern_write(0, 1, &p);
        for (uint32_t i = 0; i < 300u; i++) {
            recorded_note_t *r = &recording[i];
            r->on = (uint16_t)(i * 100u); r->duration = 300; r->note = (uint8_t)(40u + i % 40u); r->vel = 90; r->owner = 1; r->step = (uint8_t)(i % 64u); r->pitch = 0; r->length = 0;
        }
        recording_reindex();
        for (id = 8; id < OCP_N; id++) {
            poke(0, 1, id);
            rc = ocp_save(id, 0, 1);
            if (rc) break;
            k++;
            if (ocs_free_count() < free_min) free_min = ocs_free_count();
        }
        pattern_write(0, 1, &was);
        memcpy(recording, rec_was, sizeof rec_was);
        recording_reindex();
    }
    {
        char what[160];
        snprintf(what, sizeof what, "%u big phrases kept until the store is full: the keep then refused (rc 2), never fewer than %u free sectors (the reserve is %u)", k, free_min, OCJ4_RESERVE);
        R3CHK(what, rc == 2 && free_min >= OCJ4_RESERVE && k > 10u);
    }
    R3CHK("... the project still saves (the record fits the reserve) and reads back with every kept phrase", project_save(0) == 0 && (n = rec_get(0)) > 0 && (reboot(), wipe_runtime(), project_load(0), used_patterns() == 8u + k));
    R3CHK("... a save into another slot is refused whole: PROJECT FULL, the other slot empty, this one as it was",
          project_save_as(1, "COPY") != 0 && !strcmp(ui.msg, "PROJECT FULL") && !project_used(1) && !pool_headers(1, 0) && (wipe_runtime(), project_load(0), used_patterns() == 8u + k));
    /* 13: OLD FORMAT */
    reset();
    {
        uint8_t old[64];
        memset(old, 0, sizeof old); memcpy(old, "OCJ3", 4);
        R3CHK("the format before's record at slot 3's id", ocs_write(OCS_PROJECT, 3, old, sizeof old) == 0);
        reboot();
        R3CHK("... reads as OLD FORMAT, not used, not free", !project_used(3) && project_old(3) && project_free_slot_or_none() != 3);
        fill_all(12, 1);
        R3CHK("... the autosave of new music takes another slot", (project_autosave(), proj_cur < 4u && proj_cur != 3u));
    }
    /* 14: no field names a slot: the records of slot 0 copied to slot 2's ids read the same */
    reset();
    fill_all(13, NPAT);
    R3CHK("a project saved", project_save_as(0, "MOVE ME") == 0 && (n = rec_get(0)) > 0);
    snap(&A);
    {
        uint32_t ok = 1;
        for (uint32_t s = 0; s < OCS_NSECT && ok; s++) {
            const ocs_hdr_t *h = &ocs_hdr[s];
            static uint8_t buf[OCS_PARTS_MAX * OCS_PAYLOAD_MAX];
            int len;
            if (h->magic != OCS_MAGIC || h->type != OCS_POOL || !(h->id & 0x8000u) || ((h->id >> 9) & 7u) != 0u || h->part) continue;
            len = ocs_read(OCS_POOL, h->id, buf, sizeof buf);
            ok &= len >= 0 && ocs_write(OCS_POOL, (h->id & ~(7u << 9)) | 2u << 9, buf, (uint32_t)len) == 0;
        }
        ok &= rec_put(2, (uint32_t)n) == 0;
        reboot(); wipe_runtime(); project_load(2); snap(&B);
        R3CHK("... its record and chunks written under slot 2's ids read as the same project", ok && same(&A, &B) && ocp.dir.slot == 2u);
    }
    return bad;
}

/* ------------------------------------------------------------ 15 --- */
static int test_scns_foreign(void)
{
    int bad = 0, n = 0;
    uint32_t len, off, sn;
    static const uint8_t SCNS_V9[] = {9, 16, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    reset();
    fill_all(14, 2);
    lists.scene[0][0][0] = 1; lists.scene_used = 1;
    R3CHK("a project with a scene saved", project_save_as(0, "SCENES") == 0 && (n = rec_get(0)) > 0);
    len = r3rec_set(rec, (uint32_t)n, T_SCNS, SCNS_V9, sizeof SCNS_V9);
    R3CHK("... its SCNS replaced by a version 9", rec_put(0, len) == 0);
    reboot(); wipe_runtime(); ui.msg[0] = 0; project_load(0);
    R3CHK("the load keeps it, plays no scene and says so", ocj4_meta.scns_foreign && lists.scene_used == 0u && !strcmp(ui.msg, "SCENES NEED NEWER FW"));
    R3CHK("a save writes it back byte for byte", project_save(0) == 0 && (n = rec_get(0)) > 0 && !rec_find(rec, (uint32_t)n, T_SCNS, &off, &sn) && sn == sizeof SCNS_V9 && !memcmp(rec + off, SCNS_V9, sn));
    return bad;
}

int main(void)
{
    int bad = 0;
    bad |= test_round_trip_r3();
    bad |= test_255();
    bad |= test_copy_and_scratch();
    bad |= test_foreign();
    bad |= test_phrase_bytes();
    bad |= test_cuts_r3();
    bad |= test_full_old_copy();
    bad |= test_scns_foreign();
    printf(bad ? "oc r3: FAILED\n" : "oc r3: all passed\n");
    return bad ? 1 : 0;
}
