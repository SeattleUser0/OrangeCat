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
#include <stdint.h>
/* a stand-in for the card code's freeze table (card_freeze.c), so that FRZR's live branches run on the host */
#define OCJ4_FRZ_TEST 1
#define FZP_N 64u
#define FZP_VER 2u
typedef struct { uint16_t take; uint8_t trk, ver; int16_t lvl, bpm; uint32_t frames, hash; uint8_t dir, want; char name[14]; } frzp_t;
static frzp_t frzp[FZP_N];
static volatile uint8_t frzp_dirty;
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

/* a bank on a phrase above 64 reads back (the review of Part 3, finding 1: ocj_read_banks is shared with the conversion's old read, which is limited to 64) */
static int test_high_pid(void)
{
    int bad = 0;
    uint32_t hi = 70;
    reset();
    fill_all(2, 2);
    R3CHK("a project saves", project_save_as(1, "HIGH") == 0);
    for (uint32_t id = 0; id < hi; id++) {      /* phrases 1..70 exist, the 70th with the bank's music */
        poke(0, 3, id * 5u + 1u);
        if (ocp_save(id, 0, 3)) break;
    }
    R3CHK("70 phrases keep", ocp.hash[hi - 1u] != 0u);
    proj_pid[0][3] = (uint8_t)hi;
    R3CHK("... the project saves with a bank on a high phrase", project_save(1) == 0);
    hi = proj_pid[0][3];                        /* (the save names the bank's own phrase: the next free number, above 64 here) */
    R3CHK("... the bank is on a phrase above 64", hi > 64u);
    reboot(); wipe_runtime();
    memset(proj_pid, 0, sizeof proj_pid);        /* (nothing of the music is left for the load to be mistaken with) */
    project_load(1);
    R3CHK("... it loads again with that bank on the same phrase", proj_pid[0][3] == hi);
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
    /* the case the review found: the largest cut that still reads the old project is just before the record's commit, and by then
     * the changed chunks of the other generation are on the flash (the old record reads old data while they sit there) */
    {
        int kc = -1, ok2 = 1;
        uint32_t pre = 0;
        for (int k = 0; k <= (int)progs; k++) {
            memcpy(nor, nor_saved, sizeof nor);
            reboot(); wipe_runtime(); project_load(0);
            poke(0, 0, 11); poke(1, 1, 22); poke(2, 2, 33);
            fail_after = k; rc = project_save(0); fail_after = -1;
            if (kc < 0 && rc == 0) { kc = k; break; }
            { uint32_t o = 0; (void)pool_headers(0, &o); pre = o; }
            memcpy(nor + 0, nor + 0, 1);
            reboot(); wipe_runtime(); project_load(0); snap(&C);
            ok2 &= same(&C, &A);
        }
        snprintf(what, sizeof what, "... the record commits at program call %d of %u: every cut before it reads the old project, and just before it %u chunk records of the other generation sit unnamed", kc, progs, pre);
        R3CHK(what, kc > 0 && ok2 && pre >= 3u);
    }
    /* a save-as */
    memcpy(nor, nor_saved, sizeof nor);
    reboot(); wipe_runtime(); project_load(0);
    poke(0, 0, 11); poke(1, 1, 22); poke(2, 2, 33);
    snap(&B);
    { uint32_t e0 = erases; fail_after = 100000; project_save_as(1, "CUT"); progs = (uint32_t)(100000 - fail_after); fail_after = -1; ers = erases - e0; }
    ok = 1; n = m = 0;
    for (int k = 0; k <= (int)progs && ok; k++) { ok &= cut_round(0, 1, k, -1, &A, &B, &rc); n++; }
    for (int k = 0; k < (int)ers && ok; k++) { ok &= cut_round(0, 1, -1, k, &A, &B, &rc); m++; }
    snprintf(what, sizeof what, "power cut at each of %d program points (of %u) and %d erase points of a save into another slot: B whole or absent, A untouched, no orphan", n, progs, m);
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


/* ------------------------------------------------------------ helpers for the review's cases --- */
/* every chunk's used[] equals what its table says on the flash, and every directory entry points at a chunk that holds it */
static int used_consistent(void)
{
    for (uint32_t c = 1; c < OCP_NCHUNK; c++) {
        ocs_rec_t r;
        uint32_t u = 1, any = 0;
        if (!ocp.used[c]) continue;
        if (ocs_open(OCS_POOL, ocp_rid(ocp.dir.slot, ocp_bit(ocp.dir.gen, c), c), &r) || ocp_tab_read(&r, &ocp_otab)) return 0;
        for (uint32_t e = 0; e < ocp_otab.n; e++) if (ocp.dir.chunk[ocp_otab.id[e] - 1u] == c) { u += 3u + ocp_otab.len[e]; any = 1; }
        if (!any || u != ocp.used[c]) return 0;
    }
    for (uint32_t id = 0; id < OCP_N; id++) {
        ocs_rec_t r;
        uint32_t c = ocp.dir.chunk[id];
        if (!c) continue;
        if (!ocp.used[c] || ocs_open(OCS_POOL, ocp_rid(ocp.dir.slot, ocp_bit(ocp.dir.gen, c), c), &r) || ocp_tab_read(&r, &ocp_otab) ||
            ocp_tab_find(&ocp_otab, id) == ocp_otab.n) return 0;
    }
    return 1;
}
static uint8_t fbkk_img[BANK_STORE_SIZE];
/* the music in RAM captured as a foreign FBKK (a backup's, Melodee's: without Skiff's fields) */
static void fbkk_make(void)
{
    project_capture(&proj_scratch);
    bank_pack(fbkk_img, &proj_scratch, 1);
}
static int fbkk_write(uint32_t slot)
{
    proj_x_tag = 0;
    memcpy(proj_wire_u.raw, fbkk_img, sizeof fbkk_img);
    return proj_write_slot(slot, proj_wire_u.raw, BANK_STORE_SIZE);
}

/* ------------------------------------------------------------ a record that does not load is OLD FORMAT --- */
static int test_old_format_new(void)
{
    int bad = 0, n;
    uint32_t off, sn, len, victim = 0;
    reset();
    fill_all(30, 3);
    R3CHK("a project saved into slot 1", project_save_as(1, "HOLD") == 0 && (n = rec_get(1)) > 0);
    {   /* a chunk of its record erased */
        for (uint32_t s = 0; s < OCS_NSECT; s++)
            if (ocs_hdr[s].magic == OCS_MAGIC && ocs_hdr[s].type == OCS_POOL && ((ocs_hdr[s].id >> 9) & 7u) == 1u) { victim = ocs_hdr[s].id; break; }
        R3CHK("... one of its chunks erased", victim && ocs_delete(OCS_POOL, victim) == 0);
        reboot();
        R3CHK("... the slot does not read, and is held as OLD FORMAT, not free", !project_used(1) && project_old(1) && project_free_slot_or_none() != 1);
    }
    reset();
    fill_all(31, 2);
    R3CHK("a project saved", project_save_as(2, "MUT") == 0 && (n = rec_get(2)) > 0);
    {   /* its TRKS section made unreadable (np 0 in its first entry) */
        R3CHK("... its TRKS mutated", !rec_find(rec, (uint32_t)n, T_TRKS, &off, &sn));
        wr16(rec + off + 4u + 29u, 0);
        R3CHK("... and written back", rec_put(2, (uint32_t)n) == 0);
        reboot();
        R3CHK("... the slot is held as OLD FORMAT, not free", !project_used(2) && project_old(2) && project_free_slot_or_none() != 2);
    }
    /* a save that would write a record it cannot read is refused whole: counts follow the entries written */
    reset();
    fill_all(32, 2);
    R3CHK("a project saved", project_save_as(0, "COUNTS") == 0 && (n = rec_get(0)) > 0);
    {
        static uint8_t sec[256];
        /* an SMPR claiming 9 entries and holding 1; a TRKS claiming 9 and holding 4 */
        memcpy(sec, SMPR_X, sizeof SMPR_X); wr16(sec, 9);
        len = r3rec_set(rec, (uint32_t)n, T_SMPR, sec, sizeof SMPR_X);
        if (!rec_find(rec, len, T_TRKS, &off, &sn)) wr16(rec + off, 9);
        R3CHK("... its record given counts larger than the entries it holds", rec_put(0, len) == 0);
        reboot(); wipe_runtime();
        R3CHK("... which does not load (held as OLD FORMAT)", !project_used(0) && project_old(0));
    }
    reset();
    fill_all(33, 2);
    R3CHK("a project saved", project_save_as(0, "COUNTS") == 0 && (n = rec_get(0)) > 0);
    {
        static uint8_t sec[256];
        memcpy(sec, SMPR_X, sizeof SMPR_X); wr16(sec, 1);    /* an honest one, then a save: counts written are what is written */
        len = r3rec_set(rec, (uint32_t)n, T_SMPR, sec, sizeof SMPR_X);
        R3CHK("... an honest SMPR written", rec_put(0, len) == 0);
        reboot(); wipe_runtime(); project_load(0);
        R3CHK("... a save writes SMPR with its count equal to its entries", project_save(0) == 0 && (n = rec_get(0)) > 0 &&
              !rec_find(rec, (uint32_t)n, T_SMPR, &off, &sn) && rd16(rec + off) == 1u && sn == 2u + 2u + 24u);
    }
    return bad;
}

/* ------------------------------------------------------------ rename --- */
static int test_rename(void)
{
    int bad = 0, n;
    uint32_t len, uid, off, sn, off2, sn2, ok;
    static uint8_t before[OCJ4_MAX];
    recset_t a0, a1;
    reset();
    fill_all(40, 3);
    R3CHK("a project saved", project_save_as(0, "BEFORE") == 0 && (n = rec_get(0)) > 0);
    len = rec_make_foreign((uint32_t)n);
    R3CHK("... given foreign pieces", rec_put(0, len) == 0);
    reboot(); wipe_runtime(); project_load(0);
    snap(&A);
    uid = ocj4_meta.uid;
    memcpy(before, rec, len);
    recset(&a0);
    R3CHK("renamed (the live slot)", project_rename(0, "AFTER") == 0 && (n = rec_get(0)) > 0);
    {
        char nm[16];
        R3CHK("... the slot list has the new name", project_name(0, nm) && !strcmp(nm, "AFTER"));
        ok = 1;
        for (uint32_t pos = OCJ4_HLEN; pos + 8u <= len; pos += 8u + rd32(before + pos + 4)) {
            uint32_t tag = rd32(before + pos);
            if (tag == T_HEAD) continue;
            ok &= !rec_find(rec, (uint32_t)n, tag, &off, &sn) && !rec_find(before, len, tag, &off2, &sn2) && sn == sn2 && !memcmp(rec + off, before + off2, sn);
        }
        R3CHK("... every section but HEAD is byte for byte as before (the foreign pieces, the directory, the uid)", ok);
        R3CHK("... and HEAD differs in the name only", !rec_find(rec, (uint32_t)n, T_HEAD, &off, &sn) && !rec_find(before, len, T_HEAD, &off2, &sn2) && sn == sn2 &&
              !memcmp(rec + off + PROJ_NAME_LEN, before + off2 + PROJ_NAME_LEN, sn - PROJ_NAME_LEN) && !memcmp(rec + off, "AFTER\0", 6));
    }
    recset(&a1);
    {
        uint32_t same_chunks = 1;
        for (uint32_t i = 0; i < a0.n; i++) if ((a0.key[i] >> 24) == OCS_POOL) { uint32_t f = 0; for (uint32_t j = 0; j < a1.n; j++) f |= a1.key[j] == a0.key[i]; same_chunks &= f; }
        R3CHK("... the chunks were not touched", same_chunks);
    }
    reboot(); wipe_runtime(); project_load(0); snap(&B);
    R3CHK("... it loads the same, the uid is the same", same(&A, &B) && ocj4_meta.uid == uid);
    /* a slot that is not the live one */
    reset();
    fill_all(41, 2);
    R3CHK("slot 1 saved", project_save_as(1, "ONE") == 0);
    snap(&A);
    project_new_blank(); fill_all(42, 2);
    R3CHK("... other music saved into slot 2 (live)", project_save_as(2, "TWO") == 0);
    R3CHK("... slot 1 renamed while slot 2 is live", project_rename(1, "UNO") == 0);
    wipe_runtime(); project_load(1); snap(&B);
    R3CHK("... it loads as it was, with the new name", same(&A, &B) && !strcmp(proj_name, "UNO"));
    return bad;
}

/* ------------------------------------------------------------ the foreign FBKK (a backup restore) --- */
static uint8_t nor_f[sizeof nor];
static int orphans_now(uint32_t s0, uint32_t s1, uint32_t s2)
{
    uint32_t o = 0, t;
    pool_headers(s0, &t); o += t;
    pool_headers(s1, &t); o += t;
    pool_headers(s2, &t); o += t;
    return (int)o;
}
static int test_foreign_fbkk(void)
{
    int bad = 0, rc, ok;
    snap_t M, F;
    uint32_t progs, ers, n_p, n_e;
    char what[220];
    /* the foreign project */
    reset();
    fill_all(50, 2);
    fbkk_make();
    /* the music: slot 0 saved, live */
    reset();
    fill_all(51, 3);
    R3CHK("music saved into slot 0", project_save_as(0, "MUSIC") == 0);
    snap(&M);
    R3CHK("a foreign FBKK written into slot 3 (reference)", fbkk_write(3) == 0);
    wipe_runtime(); project_load(3); snap(&F);
    wipe_runtime(); project_load(0);
    R3CHK("... slot 3 loads as a project of its own banks, the music's slot untouched", !same(&F, &M) && ocp.dir.slot == 0u);
    memcpy(nor_f, nor, sizeof nor);
    /* 1. into a slot that is not the live one */
    R3CHK("restore into slot 1 (not live)", fbkk_write(1) == 0);
    reboot(); wipe_runtime(); project_load(1); snap(&C);
    R3CHK("... slot 1 is the foreign project", same(&C, &F));
    wipe_runtime(); project_load(0); snap(&C);
    R3CHK("... slot 0 is the music, no slot has an orphan", same(&C, &M) && !orphans_now(0, 1, 3) );
    /* 2. into the live slot: the music's phrases go to the scratch, the music in RAM keeps them */
    memcpy(nor, nor_f, sizeof nor);
    reboot(); wipe_runtime(); project_load(0);
    poke(0, 0, 77);                            /* (a bank edited, kept: its chunk is the live generation, uncommitted) */
    R3CHK("restore into the live slot 0", ocl_keep_all() == 0 && fbkk_write(0) == 0);
    R3CHK("... the live pool is now the scratch, holding the music's phrases", ocp.dir.slot == OCP_SCRATCH && used_patterns() >= 12u && used_consistent());
    R3CHK("... the music in RAM still keeps (its banks read their phrases)", ocl_keep_all() == 0);
    reboot(); wipe_runtime(); project_load(0); snap(&C);
    R3CHK("... after a reboot slot 0 is the foreign project", same(&C, &F) && !orphans_now(0, 1, 2));
    /* 3. into a slot holding orphans of both generations */
    memcpy(nor, nor_f, sizeof nor);
    reboot(); wipe_runtime(); project_load(0);
    fail_after = 1 << 20; (void)fbkk_write(2); progs = (uint32_t)((1 << 20) - fail_after); fail_after = -1;
    memcpy(nor, nor_f, sizeof nor); reboot(); wipe_runtime(); project_load(0);
    fail_after = (int)progs / 2; rc = fbkk_write(2); fail_after = -1;       /* a cut half way: chunks under gen 0, no record */
    R3CHK("a restore into slot 2 cut half way fails and leaves slot 2 empty", rc != 0 && !project_used(2));
    reboot(); wipe_runtime(); project_load(0);
    R3CHK("... a restore again over what the cut left: the chunks of both generations", fbkk_write(2) == 0 && !orphans_now(0, 2, 3));
    wipe_runtime(); project_load(2); snap(&C);
    R3CHK("... slot 2 is the foreign project", same(&C, &F));
    /* a restore over a slot that has a record, twice (the generation flips each time) */
    R3CHK("and over slot 2 again (its record is there: the other generation)", fbkk_write(2) == 0 && !orphans_now(0, 2, 3));
    wipe_runtime(); project_load(2); snap(&C);
    R3CHK("... still the foreign project", same(&C, &F));
    /* cuts: every program call and every erase of a restore into a non-live slot, and into the live slot */
    for (int live = 0; live < 2; live++) {
        uint32_t dst = live ? 0u : 1u, pe[2];
        memcpy(nor, nor_f, sizeof nor);
        reboot(); wipe_runtime(); project_load(0);
        { uint32_t e0 = erases; fail_after = 1 << 20; (void)fbkk_write(dst); progs = (uint32_t)((1 << 20) - fail_after); fail_after = -1; ers = erases - e0; }
        ok = 1; n_p = n_e = 0;
        for (int phase = 0; phase < 2; phase++)
            for (int k = 0; k < (phase ? (int)ers : (int)progs + 1) && ok; k++) {
                int cut_orph;
                memcpy(nor, nor_f, sizeof nor);
                reboot(); wipe_runtime(); project_load(0);
                fail_after = phase ? -1 : k; erase_cut = phase ? k : -1;
                rc = fbkk_write(dst);
                fail_after = -1; erase_cut = -1;
                (void)rc; (void)pe;
                reboot(); wipe_runtime();
                project_load(dst); snap(&C);
                ok &= same(&C, &F) || (live ? same(&C, &M) : !project_used(dst));
                wipe_runtime(); project_load(live ? 3 : 0); snap(&C);
                ok &= live ? same(&C, &F) : same(&C, &M);
                cut_orph = orphans_now(0, 1, 3) + (int)pool_headers(OCP_SCRATCH, 0);
                ok &= cut_orph == 0;
                if (phase) n_e++; else n_p++;
            }
        snprintf(what, sizeof what, "power cut at each of %u program points and %u erase points of a restore into %s: the slot reads old or new (the foreign project), the others untouched, no orphan after the boot",
                 n_p, n_e, live ? "the live slot 0" : "slot 1");
        R3CHK(what, ok);
    }
    return bad;
}

/* ------------------------------------------------------------ FRZR (a stand-in freeze table) --- */
static char fnames[16][32];
static uint32_t fh_get(const char *nm)
{
    uint32_t i;
    for (i = 0; i < 16u; i++) if (fnames[i][0] && !strcmp(fnames[i], nm)) return i;
    for (i = 0; i < 16u; i++) if (!fnames[i][0]) { strcpy(fnames[i], nm); return i; }
    return 16u;
}
static const char *fh_name(uint32_t idx) { return idx < 16u ? fnames[idx] : ""; }
static const ocj4_frzd_t fhooks = {fh_get, fh_name};
/* a FRZR section image: n folders, then entries (id, dir, want, tail bytes) */
static uint32_t frzr_build(uint8_t *b, uint32_t nf, const char *const *fn, uint32_t ne, const uint8_t (*e)[3], uint32_t tail)
{
    uint32_t at = 0;
    b[at++] = (uint8_t)nf;
    for (uint32_t i = 0; i < nf; i++) { memset(b + at, 0, 32); strcpy((char *)b + at, fn[i]); at += 32; }
    wr16(b + at, ne); at += 2;
    for (uint32_t i = 0; i < ne; i++) {
        wr16(b + at, 33u + tail); at += 2;
        memset(b + at, 0, 33u + tail);
        b[at] = e[i][0]; b[at + 1] = 1; b[at + 3] = 2;   /* take 1, version 2 */
        b[at + 17] = e[i][1]; b[at + 18] = e[i][2];
        at += 33u + tail;
    }
    return at;
}
static int test_frzr(void)
{
    int bad = 0, n;
    uint32_t off, sn, len, nf, ne;
    static uint8_t sec[2048];
    ocj4_frzd = &fhooks;
    memset(fnames, 0, sizeof fnames);
    strcpy(fnames[0], "ORANGECAT"); strcpy(fnames[1], "ORANGECAT/A"); strcpy(fnames[2], "ORANGECAT/B");
    reset();
    memset(frzp, 0, sizeof frzp);
    fill_all(60, 2);
    frzp[2].take = 1; frzp[2].ver = 2; frzp[2].dir = 1; frzp[2].want = 2; frzp[2].frames = 1000;
    frzp[9].take = 3; frzp[9].ver = 2; frzp[9].dir = 2; frzp[9].want = 2; frzp[9].frames = 2000;
    R3CHK("a project with two frozen phrases saved", project_save_as(0, "FROZEN") == 0 && (n = rec_get(0)) > 0);
    R3CHK("... its FRZR names two folders and two entries",
          !rec_find(rec, (uint32_t)n, T_FRZR, &off, &sn) && rec[off] == 2u && rd16(rec + off + 1u + 64u) == 2u);
    /* a unit whose folder table is in another order */
    memset(fnames, 0, sizeof fnames);
    strcpy(fnames[0], "ORANGECAT"); strcpy(fnames[1], "ORANGECAT/B"); strcpy(fnames[3], "ORANGECAT/A");
    memset(frzp, 0, sizeof frzp);
    reboot(); wipe_runtime(); project_load(0);
    R3CHK("... loaded on a unit with another folder order: the entries name the same folders by name",
          frzp[2].take == 1u && !strcmp(fnames[frzp[2].dir], "ORANGECAT/A") && !strcmp(fnames[frzp[2].want], "ORANGECAT/B") &&
          frzp[9].take == 3u && !strcmp(fnames[frzp[9].dir], "ORANGECAT/B") && frzp[9].frames == 2000u);
    R3CHK("... a new project has no frozen phrases", (project_new_blank(), frzp[2].take == 0u && frzp[9].take == 0u));
    /* a carried entry above 64, dead folder names dropped, indices remapped */
    {
        const char *fn[3] = {"ORANGECAT/DEAD", "ORANGECAT/LIVE", "ORANGECAT/DEAD2"};
        const uint8_t ent[1][3] = {{70, 1, 1}};
        reset();
        memset(frzp, 0, sizeof frzp);
        fill_all(61, 2);
        R3CHK("a project saved", project_save_as(0, "CARRY") == 0 && (n = rec_get(0)) > 0);
        len = frzr_build(sec, 3, fn, 1, ent, 5);
        len = r3rec_set(rec, (uint32_t)n, T_FRZR, sec, len);
        R3CHK("... its FRZR holding 3 folders and one entry (id 70, 5 extra bytes) naming the second", rec_put(0, len) == 0);
        reboot(); wipe_runtime(); project_load(0);
        R3CHK("a save", project_save(0) == 0 && (n = rec_get(0)) > 0 && !rec_find(rec, (uint32_t)n, T_FRZR, &off, &sn));
        nf = rec[off]; ne = rd16(rec + off + 1u + nf * 32u);
        R3CHK("... keeps only the folder the entry names (dead names dropped), the entry's folder bytes mapped, its tail kept",
              nf == 1u && !strcmp((char *)rec + off + 1u, "ORANGECAT/LIVE") && ne == 1u && rd16(rec + off + 1u + 32u + 2u) == 38u &&
              rec[off + 1u + 32u + 4u] == 70u && rec[off + 1u + 32u + 4u + 17u] == 0u && rec[off + 1u + 32u + 4u + 18u] == 0u);
    }
    {   /* a FRZR claiming 17 folders is not read (nothing carried, no over-read) */
        const char *fn[17];
        const uint8_t ent[1][3] = {{70, 0, 0}};
        static char nm[17][16];
        static uint8_t big[4096];
        for (int i = 0; i < 17; i++) { snprintf(nm[i], sizeof nm[i], "ORANGECAT/F%d", i); fn[i] = nm[i]; }
        reset();
        memset(frzp, 0, sizeof frzp);
        fill_all(62, 2);
        R3CHK("a project saved", project_save_as(0, "BIGFRZ") == 0 && (n = rec_get(0)) > 0);
        len = frzr_build(big, 17, fn, 1, ent, 0);
        len = r3rec_set(rec, (uint32_t)n, T_FRZR, big, len);
        R3CHK("... given a FRZR of 17 folders", rec_put(0, len) == 0);
        reboot(); wipe_runtime(); project_load(0);
        R3CHK("... a save works and writes an empty table", project_save(0) == 0 && (n = rec_get(0)) > 0 && !rec_find(rec, (uint32_t)n, T_FRZR, &off, &sn) && rec[off] == 0u);
    }
    {   /* the folders only a stored record names are not freed (the unit asks each record) */
        memset(fnames, 0, sizeof fnames);
        strcpy(fnames[0], "ORANGECAT");
        R3CHK("a folder named by a slot's record is found", ({ const char *fn1[1] = {"ORANGECAT/KEPT"}; const uint8_t e1[1][3] = {{5, 0, 0}};
              reset(); fill_all(63, 2); project_save_as(0, "K"); n = rec_get(0); len = frzr_build(sec, 1, fn1, 1, e1, 0);
              len = r3rec_set(rec, (uint32_t)n, T_FRZR, sec, len); rec_put(0, len); reboot();
              ocj4_folder_named("ORANGECAT/KEPT") && !ocj4_folder_named("ORANGECAT/OTHER"); }));
    }
    ocj4_frzd = 0;
    memset(frzp, 0, sizeof frzp);
    return bad;
}

/* ------------------------------------------------------------ rooms --- */
/* big phrases (notes timed notes each) kept until refused: how many, the least free sectors seen, the last rc */
static void big_keeps(uint32_t notes, uint32_t *kept, uint32_t *free_min, int *rc_last, uint32_t first)
{
    pattern_t p;
    pattern_read(0, 1, &p);
    for (uint32_t i = 0; i < NSTEP; i++) { p.step[i].n = 4; p.step[i].time = ST_NOTE; p.step[i].note[0] = (uint8_t)(30u + i); p.step[i].vel = 100; }
    pattern_write(0, 1, &p);
    for (uint32_t i = 0; i < notes; i++) {
        recorded_note_t *r = &recording[i];
        r->on = (uint16_t)(i * 50u); r->duration = 300; r->note = (uint8_t)(40u + i % 40u); r->vel = 90; r->owner = 1; r->step = (uint8_t)(i % 64u); r->pitch = 0; r->length = 0;
    }
    recording_reindex();
    *kept = 0; *free_min = 999; *rc_last = 0;
    for (uint32_t id = first; id < OCP_N; id++) {
        poke(0, 1, id);
        if ((*rc_last = ocp_save(id, 0, 1)) != 0) break;
        (*kept)++;
        if (ocs_free_count() < *free_min) *free_min = ocs_free_count();
    }
}
static int test_rooms(void)
{
    int bad = 0, rc;
    uint32_t kept, fmin, o;
    char what[200];
    /* a 3-part phrase against the reserve */
    reset();
    fill_all(70, 2);
    R3CHK("a project saved", project_save_as(0, "ROOM3") == 0);
    big_keeps(900, &kept, &fmin, &rc, 8);
    snprintf(what, sizeof what, "%u phrases of 3 parts kept until refused (rc %d): never fewer than %u free sectors (the reserve is %u)", kept, rc, fmin, OCJ4_RESERVE);
    R3CHK(what, rc == 2 && fmin >= OCJ4_RESERVE && kept >= 5u && used_consistent());
    {
        pattern_t p; pattern_read(0, 1, &p);
        for (uint32_t i = 0; i < NSTEP; i++) { memset(&p.step[i], 0, sizeof p.step[i]); p.step[i].time = ST_REST; }
        pattern_write(0, 1, &p); recording_reset();
    }
    R3CHK("... the project still saves", project_save(0) == 0 && used_consistent() && pool_headers(0, &o) && !o);
    /* the scratch: a new project's keeps leave room for its first save */
    reset();
    project_new_blank();
    fill_all(71, 1);
    R3CHK("its banks kept first", ocl_keep_all() == 0);
    big_keeps(300, &kept, &fmin, &rc, 8);
    snprintf(what, sizeof what, "a new project's %u big phrases kept into the scratch until refused (rc %d); the free sectors then %u", kept, rc, ocs_free_count());
    R3CHK(what, rc == 2 && kept >= 5u);
    {
        pattern_t p; pattern_read(0, 1, &p);
        for (uint32_t i = 0; i < NSTEP; i++) { memset(&p.step[i], 0, sizeof p.step[i]); p.step[i].time = ST_REST; }
        pattern_write(0, 1, &p); recording_reset();
    }
        R3CHK("... its first save still fits (the copy into a slot)", project_save_as(1, "FIRST") == 0 && ocp.dir.slot == 1u && !pool_headers(OCP_SCRATCH, 0) && used_consistent());
    R3CHK("... and it reads back after a reboot", (reboot(), wipe_runtime(), project_load(1), ocp.dir.slot == 1u && used_consistent()));
    /* used[] through a phrase emptied and a phrase moved */
    reset();
    fill_all(72, 3);
    R3CHK("a project saved", project_save_as(0, "USED") == 0 && used_consistent());
    R3CHK("a phrase cleared through the directory", ocp_clear(bank_pid[0][0] - 1u) == 0 && used_consistent());
    R3CHK("a phrase copied to a new number", ocp_copy(bank_pid[1][1] - 1u, 100) == 0 && ocp.dir.chunk[100] != 0u && used_consistent());
    R3CHK("... and the project saves and reloads with it", project_save(0) == 0 && (reboot(), wipe_runtime(), project_load(0), used_consistent() && ocp.dir.chunk[100] != 0u));
    return bad;
}



#if OC_LFO2
/* ------------------------------------------------------------ LFO 2: records by count --- */
/* a FUN23 image (np 124) of a FUN24 one (np 131): the same project as the firmware before LFO 2 wrote it. The tracks' values lose LFO 2's seven;
 * the sequential run after them (chain, motion, the user ids) moves up by 28 bytes; the motion events on the engine ids move down by 7 */
static void fun23_of(const project_store_t *n24, project_store_t *o23)
{
    const uint8_t *n = n24->raw;
    uint8_t *o = o23->raw;
    uint32_t pn = 68u, po = 68u, k, i, mot_at_n, seq_end_n;
    memcpy(o, n, 68);
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < P_E0 - 7u; i++) o[po++] = n[pn + i];            /* the common ids below LFO 2 */
        for (i = 0; i < 8u; i++) o[po++] = n[pn + P_E0 + i];             /* P_E0..P_E7 */
        po += 0; pn += P_COUNT;
        memcpy(o + po, n + pn, 2u + NSTEP * 8u); po += 2u + NSTEP * 8u; pn += 2u + NSTEP * 8u;
    }
    mot_at_n = pn + (uint32_t)sizeof(chain_config_t);
    seq_end_n = mot_at_n + (uint32_t)sizeof(motion_store_t) + 2u + 2u * NTRK;
    memcpy(o + po, n + pn, seq_end_n - pn);
    for (i = 0; i < n[mot_at_n]; i++) {                                  /* events: place, param, value (4 bytes) after count, on, rsv[2] */
        uint8_t *ev = o + po + (mot_at_n - pn) + 4u + 4u * i;
        if (ev[1] >= P_E0 && ev[1] < P_COUNT && !((ev[2] | ev[3] << 8) >= 256)) ev[1] = (uint8_t)(ev[1] - 7u);   /* (not a native event: its value is below 256) */
    }
    po += seq_end_n - pn;
    memset(o + po, 0, PROJ_STORE_SIZE - po);                              /* the fixed-offset parts after: copied from the new record */
    for (i = 0; i < PROJ_STORE_SIZE; i++) if (i >= PROJ_JFX_WIRE - 0u) o[i] = n[i];
    { uint32_t m = PROJ_FUN23_MAGIC, sum; memcpy(o, &m, 4); o[66] = 124; sum = proj_hash(o, PROJ_STORE_SIZE - 4u); memcpy(o + PROJ_STORE_SIZE - 4u, &sum, 4); }
}
static int test_lfo2_records(void)
{
    int bad = 0;
    static project_store_t n24, o23;
    static project_t q, r;
    uint32_t k;
    reset();
    fill_all(90, 2);
    for (k = 0; k < NTRK; k++) {
        trk[k].p[P_L2RATE] = (int16_t)(30 + k); trk[k].p[P_L2WAVE] = 2; trk[k].p[P_L2SYNC] = 3; trk[k].p[P_L2TRIG] = 1; trk[k].p[P_L2POL] = 1; trk[k].p[P_L2DST] = (int16_t)(6 + k); trk[k].p[P_L2AMT] = -17;
    }
    motion.count = 0;
    motion.event[motion.count].place = 3; motion.event[motion.count].param = P_E1; motion.event[motion.count].value = 9; motion_pattern[motion.count++] = 0;
    motion.event[motion.count].place = 4; motion.event[motion.count].param = P_DUB_FDBK; motion.event[motion.count].value = 70; motion_pattern[motion.count++] = 0;
    motion.event[motion.count].place = 5; motion.event[motion.count].param = P_L2RATE; motion.event[motion.count].value = 44; motion_pattern[motion.count++] = 0;
    motion.rsv[0] = 1;
    project_capture(&q);
    for (k = 0; k < NTRK; k++) q.t[k].mpc_octave = (int8_t)((int)k - 1);   /* -1 0 1 2 */
    R3CHK("a FUN24 record (np 131) packs and unpacks with LFO 2's seven kept", proj_pack(&n24, &q) && n24.raw[66] == P_COUNT && proj_unpack(&r, n24.raw) &&
          r.t[2].p[P_L2RATE] == 32 && r.t[2].p[P_L2DST] == 8 && r.t[2].p[P_L2AMT] == -17 && r.t[0].p[P_L2POL] == 1 && r.t[1].p[P_L2SYNC] == 3 && r.t[3].p[P_L2WAVE] == 2);
    fun23_of(&n24, &o23);
    R3CHK("a FUN23 record (np 124) loads by count: LFO 2 off at its defaults, every id below it and P_E0..P_E7 as they were",
          proj_unpack(&r, o23.raw) && ({ uint32_t ok = 1; for (k = 0; k < NTRK; k++) {
              for (uint32_t i = 0; i < P_L2RATE; i++) ok &= r.t[k].p[i] == q.t[k].p[i];
              for (uint32_t i = 0; i < 8u; i++) ok &= r.t[k].p[P_E0 + i] == q.t[k].p[P_E0 + i];
              ok &= r.t[k].p[P_L2RATE] == TP[P_L2RATE].def && r.t[k].p[P_L2DST] == 0 && r.t[k].p[P_L2AMT] == 0; } ok; }));
    R3CHK("... and its MPC octaves (-1 0 1 2) are kept (FUN23 is read with np 124, the octave word is where FUN22 and FUN23 put it)", ({ uint32_t ok = 1; for (k = 0; k < NTRK; k++) ok &= r.t[k].mpc_octave == (int)k - 1; ok; }));
    R3CHK("... its motion on the old P_E1 lands on the new P_E1, and its DUB TAPE lane is not dropped (FUN23 keeps FUN23's behaviour)",
          r.motion.count >= 2u && ({ uint32_t e1 = 0, dub = 0; for (uint32_t i = 0; i < r.motion.count; i++) { e1 |= r.motion.event[i].param == P_E1; dub |= r.motion.event[i].param == P_DUB_FDBK; } e1 && dub; }));
    R3CHK("... a FUN23 record is still known, FUN22's np 122 too, and a FUN23 with np 131 is refused", proj_magic_known(PROJ_FUN23_MAGIC) && ({ project_store_t bad131 = o23; bad131.raw[66] = 131; uint32_t sum = proj_hash(bad131.raw, PROJ_STORE_SIZE - 4u); memcpy(bad131.raw + PROJ_STORE_SIZE - 4u, &sum, 4); !proj_unpack(&r, bad131.raw); }));
    /* a phrase with a lane on LFO 2's RATE through the project and the pool */
    reset();
    fill_all(91, 2);
    motion.count = 0;
    motion.event[0].place = 2; motion.event[0].param = P_L2RATE; motion.event[0].value = 51; motion_pattern[0] = 0; motion.count = 1; motion.rsv[0] = 1;
    R3CHK("a project with a motion lane on P_L2RATE saves", project_save_as(0, "L2LANE") == 0);
    reboot(); wipe_runtime(); project_load(0);
    R3CHK("... and loads it back (a phrase with np 131 and a 5-byte event)", ({ uint32_t f = 0; for (uint32_t i = 0; i < motion.count; i++) f |= motion.event[i].param == P_L2RATE && motion.event[i].value == 51; f; }));
    /* user presets */
    {
        up_rec_t u;
        int16_t v[P_COUNT];
        memset(&u, 0, sizeof u);
        u.used = UP_USED; u.ver = UP_VER; u.engine = HOST_SYNTH; u.np = 124;
        memcpy(u.name, "OLD124", 6);
        for (uint32_t i = 0; i < 124u; i++) u.packed[i] = (uint8_t)(64 + (int)(i % 20) - 5);
        R3CHK("a user preset of np 124 is valid and loads by count (LFO 2 at its defaults, P_E0..P_E7 from its last eight)", up_valid(&u) && (up_values(&u, v), 1) &&
              v[P_L2RATE] == TP[P_L2RATE].def && v[P_L2AMT] == 0 &&
              v[P_E0] == (int16_t)clamp((int)u.packed[116] - 64, param_desc_of(HOST_SYNTH, P_E0)->min, param_desc_of(HOST_SYNTH, P_E0)->max));
        u.np = P_COUNT;
        for (uint32_t i = 0; i < P_COUNT; i++) u.packed[i] = (uint8_t)(64 + (int)(i % 7));
        up_values(&u, v);
        R3CHK("a user preset of np 131 is valid and keeps LFO 2's seven", up_valid(&u) && v[P_L2RATE] == (int16_t)clamp((int)u.packed[P_L2RATE] - 64, TP[P_L2RATE].min, TP[P_L2RATE].max));
    }
    /* a template of np 124 (TPLM) and one of np 131 (TPLN) */
    {
        static tmpl_t t24;
        static uint8_t old[sizeof(tmpl_t)];
        uint8_t *nb;
        uint32_t po, pn, tail;
        uint32_t oldlen = (uint32_t)sizeof(tmpl_t) - NTRK * 2u * (P_COUNT - 124u), m = TMPL_MAGIC_M;
        reset();
        template_save();                                     /* (a real template: valid native patches) */
        for (k = 0; k < NTRK; k++) for (uint32_t i = 0; i < P_COUNT; i++) tmpl.t[k].p[i] = (int16_t)clamp((int)((i * 3 + k) % 40), param_desc_of(tmpl.t[k].engine % NENGINES, i)->min, param_desc_of(tmpl.t[k].engine % NENGINES, i)->max);
        t24 = tmpl;
        R3CHK("a TPLN template (np 131) is the current layout", tmpl_param_count(sizeof(tmpl_t)) == P_COUNT && tmpl_blob_valid((const uint8_t *)&t24, sizeof t24) == 1);
        nb = (uint8_t *)&t24;
        memcpy(old, nb, 2u * PROJ_GLOBALS + 2u);
        po = pn = 2u * PROJ_GLOBALS + 2u;
        for (k = 0; k < NTRK; k++) {
            old[po++] = nb[pn++]; old[po++] = nb[pn++];
            for (uint32_t i = 0; i < P_E0 - 7u; i++) { memcpy(old + po, nb + pn + 2u * i, 2); po += 2; }
            for (uint32_t i = 0; i < 8u; i++) { memcpy(old + po, nb + pn + 2u * (P_E0 + i), 2); po += 2; }
            pn += 2u * P_COUNT;
        }
        tail = (uint32_t)sizeof(tmpl_t) - 8u - pn;
        memcpy(old + po, nb + pn, tail); po += tail;
        memcpy(old + po, &oldlen, 4); memcpy(old + po + 4, &m, 4);
        R3CHK("a TPLM template (np 124) is recognised, valid, and loads by count", tmpl_param_count(oldlen) == 124u && tmpl_blob_valid(old, oldlen) == 1 && tmpl_take(old, oldlen) &&
              ({ uint32_t ok = 1; for (k = 0; k < NTRK; k++) {
                     for (uint32_t i = 0; i < P_L2RATE; i++) ok &= tmpl.t[k].p[i] == t24.t[k].p[i];
                     for (uint32_t i = 0; i < 8u; i++) ok &= tmpl.t[k].p[P_E0 + i] == t24.t[k].p[P_E0 + i];
                     ok &= tmpl.t[k].p[P_L2RATE] == TP[P_L2RATE].def && tmpl.t[k].p[P_L2AMT] == 0; } ok; }));
    }
    return bad;
}
#endif

int main(void)
{
    int bad = 0;
#if OC_LFO2
    bad |= test_lfo2_records();
#endif
    bad |= test_round_trip_r3();
    bad |= test_255();
    bad |= test_high_pid();
    bad |= test_copy_and_scratch();
    bad |= test_foreign();
    bad |= test_phrase_bytes();
    bad |= test_cuts_r3();
    bad |= test_full_old_copy();
    bad |= test_scns_foreign();
    bad |= test_old_format_new();
    bad |= test_rename();
    bad |= test_foreign_fbkk();
    bad |= test_frzr();
    bad |= test_rooms();
    printf(bad ? "oc r3: FAILED\n" : "oc r3: all passed\n");
    return bad ? 1 : 0;
}
