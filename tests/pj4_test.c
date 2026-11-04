/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Host test of the project format core, firmware/src/oc_pj4.c (Release 3 step 7, step 1; PROJECT_FORMAT_R3.md 3, 9, 10, 13):
 *   build/host/pj4_test            (with  --old-store FILE  and a NOR image FILE it is the second half of item 11, below)
 * The record store on a simulated NOR (erase sets 0xFF, programming only clears bits, a program stays inside a 256-byte page) that loses power
 * at ANY erase or program (a torn erase leaves garbage; a torn program keeps a prefix), and, in a second pass, in the middle of a program every 61st byte.
 *  1. save, read, the revision, a second save that changes one chunk rewrites that chunk only, unknown sections come back byte for byte
 *  2. a large phrase takes a chunk of its own with three parts; 255 phrases; a phrase source that changes under the save is refused, the old record stays
 *  3. a full store refuses a save whole (no erase, no program), names the bytes short; the old project is intact
 *  4. POWER CUTS, SAVE: after each cut and the boot, the slot reads entirely old or entirely new, no chunk is left that no record names, the next save works
 *     (this includes the case 13.2: a cut after the first of three changed chunks and before the record)
 *  5. the repack: macro motion events of the old versions dropped, the version byte set, every other byte equal; the control flag only where 8.2b allows it
 *  6. CONVERSION: two old projects sharing phrases; a cut at every erase and program of the whole boot, then the boot again: converted once, each project's
 *     phrases its own, the old records and the unneeded old pool chunks gone, nothing orphaned; a third boot does nothing (zero erases and programs)
 *  7. a restore over a converted slot (old records written after pj4_slot_clear) converts the slot once, it is not erased; a cut in the middle of it too
 *  8. the worst case: four projects that each use many phrases: the conversion stops at the slot that does not fit, that slot stays OLD FORMAT with its old
 *     record readable, and converts once room is made
 *  9. the project uid and the revision counter: the uid survives a save, a conversion carries the head the caller made, the revision rises with each save
 * 10. the Gate 1 round trip: a record with unknown sections (and an unknown tag) loads and saves byte for byte
 * 11. the old store code (the Release 2 oc_store.c, run by tools in build/host/old_store.sh) boots a converted store, finds no old record, erases nothing of it,
 *     and an old-format save beside it leaves every converted phrase readable; tested here with the current store and with the file given by --old-store
 */
#define MELODEE_FLASH 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nor[0x100000];
static uint8_t snap[0x100000];
static long opn, cut_op = -1;              /* the erase and program calls so far; the one that finds the power gone */
static long bytes_n, cut_byte = -1;        /* the programmed bytes so far; the one after which the power goes */
static int dead;
static int erase_torn = 1;                   /* a cut at an erase: 1 garbage, 0 the power went before it started */
static long eo, err_op = -1;              /* a transient flash error (the power stays) at the err_op-th erase or program call */
static long n_erase, n_prog;
static uint32_t junk = 0x1234567u;

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (dead) return -8;
    if (err_op >= 0 && eo++ == err_op) return -8;
    if (off & 4095u) { printf("unaligned erase at %#x\n", off); exit(1); }
    if (cut_op >= 0 && opn++ == cut_op) {                          /* a torn erase: garbage */
        if (!erase_torn) { dead = 1; return -8; }
        for (uint32_t i = 0; i < 4096; i++) { junk = junk * 1664525u + 1013904223u; nor[off + i] = (uint8_t)(junk >> 24); }
        dead = 1;
        return -8;
    }
    memset(nor + off, 0xFF, 4096);
    n_erase++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (dead) return -9;
    if (err_op >= 0 && eo++ == err_op) return -9;
    if ((off & 0xFFu) + n > 256u) { printf("page wrap at %#x\n", off); exit(1); }
    if (cut_op >= 0 && opn++ == cut_op) { dead = 1; return -9; }
    n_prog++;
    for (uint32_t i = 0; i < n; i++) {
        if (cut_byte >= 0 && bytes_n++ == cut_byte) { dead = 1; return -9; }
        nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/oc_store.c"
#define OCP_PJ4_TEST 1
#include "../firmware/src/oc_pj4.c"

static int bad;
static int check(const char *what, int ok)
{
    printf("pj4: %-110s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) bad++;
    return !ok;
}

/* ---------------------------------------------------------- fixtures ----- */
static uint32_t rnd_s = 1;
static uint32_t rnd(void) { rnd_s ^= rnd_s << 13; rnd_s ^= rnd_s >> 17; rnd_s ^= rnd_s << 5; return rnd_s; }
static void bytes_of(uint32_t seed, uint8_t *out, uint32_t n)
{
    uint32_t x = seed * 2654435761u + 1u;
    for (uint32_t i = 0; i < n; i++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; out[i] = (uint8_t)(x >> 8); }
}
typedef struct { uint32_t n, id[255], len[255], seed[255]; } pset_t;
static void pset_add(pset_t *p, uint32_t id, uint32_t len, uint32_t seed)
{
    uint32_t i;
    for (i = 0; i < p->n && p->id[i] < id; i++) ;
    if (i < p->n && p->id[i] == id) { p->len[i] = len; p->seed[i] = seed; return; }
    memmove(p->id + i + 1, p->id + i, (p->n - i) * 4u); memmove(p->len + i + 1, p->len + i, (p->n - i) * 4u); memmove(p->seed + i + 1, p->seed + i, (p->n - i) * 4u);
    p->id[i] = id; p->len[i] = len; p->seed[i] = seed; p->n++;
}
static uint8_t tmp[PJ4_PHMAX + 8];
static int pget(void *ctx, uint32_t id, uint8_t *dst, uint32_t off, uint32_t n)
{
    const pset_t *p = ctx;
    for (uint32_t i = 0; i < p->n; i++)
        if (p->id[i] == id) { bytes_of(p->seed[i], tmp, p->len[i]); memcpy(dst, tmp + off, n); return 0; }
    return -1;
}
static uint32_t phr(const pset_t *p, pj4_phr_t *out)
{
    for (uint32_t i = 0; i < p->n; i++) {
        bytes_of(p->seed[i], tmp, p->len[i]);
        out[i].id = (uint8_t)p->id[i]; out[i].len = (uint16_t)p->len[i]; out[i].crc = ocs_crc32(tmp, p->len[i]);
    }
    return p->n;
}
static pj4_head_t mkhead(const char *name, uint32_t uid)
{
    pj4_head_t h;
    memset(&h, 0, sizeof h);
    strncpy(h.name, name, 11); h.parts = 4; h.sel = 1; h.seed = uid * 7u + 3u; h.uid = uid; h.next_id = 10;
    return h;
}
static int save(uint32_t slot, const pset_t *p, pj4_head_t *h, const pj4_sec_t *s, uint32_t ns, uint32_t *over)
{
    static pj4_phr_t ph[255];
    uint32_t n = phr(p, ph);
    return pj4_save(slot, h, s, ns, ph, n, pget, (void *)p, over);
}
/* the slot reads as exactly the set p: head revision rev (0: any), every phrase's bytes, no other phrase */
static int reads_set(uint32_t slot, const pset_t *p, uint32_t rev)
{
    static uint8_t b[PJ4_PHMAX + 8];
    static uint8_t want[PJ4_PHMAX + 8];
    pj4_head_t h;
    uint32_t i, cnt = 0, id;
    if (pj4_head_load(slot, &h) || (rev && h.rev != rev) || pj4_dir_load(slot, &pj4_d0)) return 0;
    for (id = 1; id <= 255; id++) cnt += pj4_d0.chunk[id] != PJ4_NONE;
    if (cnt != p->n) return 0;
    for (i = 0; i < p->n; i++) {
        int n = pj4_phrase_read(slot, p->id[i], b, sizeof b);
        bytes_of(p->seed[i], want, p->len[i]);
        if (n != (int)p->len[i] || memcmp(b, want, p->len[i])) return 0;
    }
    return 1;
}
/* every chunk record of ours is named by its slot's record in charge */
static int no_orphans(void)
{
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        uint32_t id = ocs_hdr[s].id, slot = (id >> 9) & 3u, c = id & 0xFFu, i, used = 0;
        if (ocs_hdr[s].magic != OCS_MAGIC || ocs_hdr[s].type != OCS_POOL || !pj4_mine(id)) continue;
        if (pj4_dir_load(slot, &pj4_d0)) return 0;
        for (i = 1; i <= PJ4_NPH; i++) used |= pj4_d0.chunk[i] == c;
        if (!used || pj4_gen(&pj4_d0, c) != (int)((id >> 8) & 1u)) return 0;
    }
    return 1;
}
static void wipe(void)
{
    memset(nor, 0xFF, sizeof nor);
    dead = 0; cut_op = -1; cut_byte = -1; opn = 0; bytes_n = 0;
    ocs_resv = 0; ocs_held = 0;
    ocs_boot();
}
static void reboot(void) { dead = 0; cut_op = -1; cut_byte = -1; ocs_held = 0; ocs_boot(); }
static void snapshot(void) { memcpy(snap, nor, sizeof nor); }
static void restore(void) { memcpy(nor, snap, sizeof nor); dead = 0; cut_op = -1; cut_byte = -1; opn = 0; bytes_n = 0; ocs_held = 0; ocs_boot(); }

/* the sample slot's claim: sectors 18..39 are not the store's (the store is 34 sectors, with the six at 0xC2000 not yet moved) */
static void claim(void) { for (uint32_t s = 18; s <= 39; s++) ocs_resv |= (uint64_t)1 << s; ocs_boot(); }

/* ------------------------------------------------------------ 1 --------- */
static int test_basic(void)
{
    pset_t a = {0}, b = {0};
    pj4_head_t h = mkhead("ONE", 0xA11CE), h2;
    uint8_t carry[64];
    pj4_sec_t secs[4], again[4];
    static const uint8_t unk[7] = {1, 2, 3, 4, 5, 6, 7};
    static const uint32_t known[1] = {PJ4_TAG('K', 'N', 'O', 'W')};
    uint32_t over = 0, na = 0, e0;
    wipe(); claim();
    pset_add(&a, 1, 1000, 11); pset_add(&a, 2, 2000, 12); pset_add(&a, 3, 2500, 13); pset_add(&a, 40, 100, 14);
    secs[0].tag = PJ4_TAG('K', 'N', 'O', 'W'); secs[0].len = 5; secs[0].p = (const uint8_t *)"hello";
    secs[1].tag = PJ4_TAG('Z', 'Z', 'Z', 'Z'); secs[1].len = 7; secs[1].p = unk;
    check("save: a project of four phrases (two chunks) is written", save(0, &a, &h, secs, 2, &over) == 0);
    check("read: every phrase back byte for byte, the head's revision is 1", reads_set(0, &a, 1) && h.rev == 1);
    check("no chunk is left that the record does not name", no_orphans());
    pj4_dir_load(0, &pj4_d0);
    check("the phrases 1 and 2 are in chunk 0 and 3 and 40 in chunk 1", pj4_d0.chunk[1] == 0 && pj4_d0.chunk[2] == 0 && pj4_d0.chunk[3] == 1 && pj4_d0.chunk[40] == 1);
    /* a second save that changes phrase 3 only */
    b = a; pset_add(&b, 3, 2600, 99);
    n_erase = n_prog = 0; e0 = 0;
    check("save: phrase 3 changed", save(0, &b, &h, secs, 2, &over) == 0);
    check("the revision rose to 2 and the phrases read as the new set", reads_set(0, &b, 2) && h.rev == 2);
    pj4_dir_load(0, &pj4_d0);
    check("chunk 0 was not rewritten (its generation is 0 still), chunk 1 flipped to generation 1", pj4_gen(&pj4_d0, 0) == 0 && pj4_gen(&pj4_d0, 1) == 1);
    check("the old generation of chunk 1 is gone (the sweep), nothing is orphaned", no_orphans() && !ocs_exists(OCS_POOL, pj4_cid(0, 0, 1)) && ocs_exists(OCS_POOL, pj4_cid(0, 0, 0)));
    /* unknown sections */
    na = 0;
    check("pj4_carry: the sections the caller does not know (ZZZZ) come back", pj4_carry(0, known, 1, carry, sizeof carry, again, &na, 4) == 0 && na == 1 && again[0].tag == PJ4_TAG('Z', 'Z', 'Z', 'Z') && again[0].len == 7 && !memcmp(again[0].p, unk, 7));
    h2 = h;
    secs[1].tag = PJ4_TAG('Q', 'Q', 'Q', 'Q');
    check("a save with the carried section: written again byte for byte", save(0, &b, &h2, again, na, &over) == 0 && pj4_sec_read(0, PJ4_TAG('Z', 'Z', 'Z', 'Z'), carry, sizeof carry) == 7 && !memcmp(carry, unk, 7));
    check("the head round trips (name, uid, seed, next id)", pj4_head_load(0, &h2) == 0 && !strcmp(h2.name, "ONE") && h2.uid == 0xA11CE && h2.next_id == 10 && h2.rev == 3);
    return 0;
}

/* ------------------------------------------------------------ 2 --------- */
static int bad_get(void *ctx, uint32_t id, uint8_t *dst, uint32_t off, uint32_t n) { memset(dst, 0x5A, n); return 0; }
static int test_sizes(void)
{
    pset_t a = {0}, b = {0}, big = {0};
    pj4_head_t h = mkhead("BIG", 7);
    pj4_phr_t ph[2];
    uint32_t i, over = 0;
    wipe(); claim();
    pset_add(&big, 5, PJ4_PHMAX, 5); pset_add(&big, 6, 40, 6); pset_add(&big, 9, PJ4_PHMAX, 9);
    check("a phrase of the largest size takes a chunk of its own: two of them and a small one save", save(1, &big, &h, 0, 0, &over) == 0 && reads_set(1, &big, 1));
    pj4_dir_load(1, &pj4_d0);
    check(".. the chunks are 0, 1 and 2, three parts each for the large ones", pj4_d0.chunk[5] == 0 && pj4_d0.chunk[6] == 1 && pj4_d0.chunk[9] == 2 && pj4_parts(PJ4_PHMAX + 3u) == 3);
    wipe(); claim();
    for (i = 1; i <= 255; i++) pset_add(&a, i, 4 + (i % 7), i * 3);
    check("255 phrases (the limit) save and read back", save(2, &a, &h, 0, 0, &over) == 0 && reads_set(2, &a, 1) && no_orphans());
    pset_add(&b, 1, 100, 1);
    check("a save that names phrase 0 or a length of 0 is refused (PJ4_BAD)", ({ pj4_phr_t q = {0, 5, 0}; pj4_save(2, &h, 0, 0, &q, 1, pget, &b, &over); }) == PJ4_BAD);
    /* a source that changes under the save */
    wipe(); claim();
    pset_add(&a, 1, 0, 0); a.n = 0; pset_add(&a, 1, 500, 1); pset_add(&a, 2, 500, 2);
    check("the first save", save(0, &a, &h, 0, 0, &over) == 0);
    ph[0].id = 1; ph[0].len = 500; ph[0].crc = 0x12345678u;
    check("a phrase whose bytes do not match the CRC given: refused (PJ4_SRC)", pj4_save(0, &h, 0, 0, ph, 1, bad_get, 0, &over) == PJ4_SRC);
    check(".. the old record is in charge, nothing is orphaned", reads_set(0, &a, 1) && no_orphans());
    return 0;
}

/* ------------------------------------------------------------ 3 --------- */
static int test_full(void)
{
    pset_t a = {0}, huge = {0};
    pj4_head_t h = mkhead("FULL", 1);
    uint32_t over = 0, i;
    long e, p;
    wipe(); claim();
    for (i = 1; i <= 8; i++) pset_add(&a, i, 3000, i);
    check("a project of eight 3,000 B phrases saves", save(0, &a, &h, 0, 0, &over) == 0);
    for (i = 1; i <= 200; i++) pset_add(&huge, i, 3000, 700 + i);
    e = n_erase; p = n_prog;
    check("a save that cannot fit is refused PROJECT FULL and names the bytes short", save(1, &huge, &h, 0, 0, &over) == PJ4_FULL && over > 0);
    check(".. with no erase and no program", n_erase == e && n_prog == p);
    check(".. the first project is intact", reads_set(0, &a, 1) && no_orphans());
    {   /* the reserve: grow a project until a save is refused; the last one that went through left the 3 sector reserve free */
        pset_t g = {0};
        uint32_t last_free = 0, k;
        wipe(); claim();
        for (k = 1; k <= 60; k++) {
            pj4_head_t hh = mkhead("GROW", 2);
            pset_add(&g, k, 3000, 50 + k);
            if (save(0, &g, &hh, 0, 0, &over) != 0) break;
            last_free = ocs_free_count();
        }
        check("the reserve: a project grown one 3,000 B phrase at a time is refused before the store is exhausted; the last save left PJ4_RESERVE sectors free", k < 60 && last_free >= 3u && no_orphans());
    }
    return 0;
}

/* ------------------------------------------------------------ 4 --------- */
/* Run `run` with the power going at erase/program call k (mode 0) or after programmed byte k (mode 1), for k = 0, 1, 2 .. until the run completes; after each,
 * a reboot (the store's scan and the sweep), then check(arg, finished). Returns the number of cut points tried (negative: a check failed) */
typedef struct { int (*run)(void *); int (*chk)(void *, int); void *arg; } cutter_t;
static int cut_all(const cutter_t *c, int mode, int step, int *failed)
{
    int k, done = 0, n = 0;
    erase_torn = mode != 2;
    if (mode == 2) mode = 0;
    for (k = 0; !done; k += step, n++) {
        restore();
        if (mode == 0) { cut_op = k; opn = 0; } else { cut_byte = k; bytes_n = 0; }
        c->run(c->arg);
        done = !dead;
        cut_op = -1; cut_byte = -1; dead = 0;
        ocs_boot(); pj4_sweep();
        if (!c->chk(c->arg, done)) { (*failed)++; if (*failed < 4) printf("  (failed at %s %d)\n", mode ? "byte" : "call", k); }
        if (n > 100000) { *failed += 1000; break; }
    }
    return n;
}
static pset_t sv_old, sv_new;
static pj4_head_t sv_h;
static int sv_run(void *a) { pj4_head_t h = sv_h; uint32_t over; (void)a; return save(0, &sv_new, &h, 0, 0, &over); }
static int sv_chk(void *a, int finished)
{
    (void)a;
    if (!(reads_set(0, &sv_old, 1) || reads_set(0, &sv_new, 2))) return 0;       /* entirely old or entirely new */
    if (finished && !reads_set(0, &sv_new, 2)) return 0;
    if (!no_orphans()) return 0;
    {   /* the next save works */
        pset_t nx = sv_new;
        pj4_head_t h = sv_h;
        uint32_t over;
        pset_add(&nx, 3, 777, 4242);
        return save(0, &nx, &h, 0, 0, &over) == 0 && reads_set(0, &nx, 0) && no_orphans();
    }
}
static int test_save_cuts(void)
{
    cutter_t c = {sv_run, sv_chk, 0};
    int failed = 0, n0, n1;
    uint32_t over;
    char what[200];
    sv_h = mkhead("CUT", 99);
    memset(&sv_old, 0, sizeof sv_old);
    pset_add(&sv_old, 1, 3000, 1); pset_add(&sv_old, 2, 3000, 2); pset_add(&sv_old, 3, 3000, 3); pset_add(&sv_old, 4, 900, 4);
    sv_new = sv_old;                                                              /* (every one of the four chunks changes: the 13.2 case, three or more at a time) */
    pset_add(&sv_new, 1, 3000, 101); pset_add(&sv_new, 2, 3000, 102); pset_add(&sv_new, 3, 3000, 103); pset_add(&sv_new, 5, 50, 105);
    wipe(); claim();
    { pj4_head_t h = sv_h; save(0, &sv_old, &h, 0, 0, &over); }
    snapshot();
    n0 = cut_all(&c, 0, 1, &failed);
    snprintf(what, sizeof what, "SAVE, power cut at each of %d erase and program calls: the slot reads entirely old or entirely new, no orphan, the next save works", n0);
    check(what, !failed && n0 > 20);
    failed = 0;
    n1 = cut_all(&c, 1, 61, &failed);
    snprintf(what, sizeof what, "SAVE, power cut after each 61st programmed byte (%d points): the same", n1);
    check(what, !failed && n1 > 20);
    failed = 0;
    n0 = cut_all(&c, 2, 1, &failed);
    snprintf(what, sizeof what, "SAVE, power gone just before each of %d erase and program calls (an erase that never started): the same", n0);
    check(what, !failed && n0 > 20);
    {   /* a flash error that is not a power cut: the save fails, the device goes on; no reboot: the old record is in charge, nothing orphaned at once */
        int k, ok = 1, runs = 0;
        for (k = 0; k < n0 && ok; k++) {
            pj4_head_t h = sv_h;
            uint32_t over2;
            int rc;
            restore();
            eo = 0; err_op = k;
            rc = save(0, &sv_new, &h, 0, 0, &over2);
            err_op = -1;
            runs++;
            if (rc == 0) ok = reads_set(0, &sv_new, 2) && no_orphans();
            else ok = reads_set(0, &sv_old, 1) && no_orphans();
            if (rc != 0) { restore(); }
        }
        snprintf(what, sizeof what, "SAVE, a flash error (not a power cut) at each of %d erase and program calls: the save fails whole, the old record reads, no chunk is left orphaned at once", runs);
        check(what, ok && runs > 20);
    }
    /* 13.2 pinned: a cut after the first changed chunk is written and before the record */
    restore();
    {
        pj4_head_t h = sv_h;
        pj4_phr_t ph[8];
        uint32_t n = phr(&sv_new, ph), over2;
        long chunk_ops;
        pj4_plan(ph, n);
        opn = 0; cut_op = 1000000;
        save(0, &sv_new, &h, 0, 0, &over2);
        chunk_ops = opn;
        restore();
        opn = 0; cut_op = chunk_ops - 7;                                          /* (somewhere inside the record's own writes: after every chunk) */
        h = sv_h; save(0, &sv_new, &h, 0, 0, &over2);
        cut_op = -1; dead = 0; ocs_boot();
        check("13.2: a cut after all the changed chunks and before the record commits: the old record reads the old data", reads_set(0, &sv_old, 1));
        pj4_sweep();
        check(".. and the sweep erases the chunks of the generation it does not name", reads_set(0, &sv_old, 1) && no_orphans());
    }
    return 0;
}

/* ------------------------------------------------------------ 5 --------- */
static uint32_t mk_pattern(uint32_t ver, uint32_t ns, uint32_t mc, uint32_t nn, uint32_t seed, uint8_t *o, int ctl)
{
    uint32_t i, p = 20, k = 0;
    uint64_t m = 0;
    bytes_of(seed, o, 20);
    o[0] = (uint8_t)ver; o[1] = 124; o[3] = 0;
    for (i = 0; i < ns; i++) m |= (uint64_t)1 << ((i * 5u) & 63u);
    ns = 0; for (i = 0; i < 64; i++) ns += (uint32_t)((m >> i) & 1u);
    for (i = 0; i < 8; i++) o[12 + i] = (uint8_t)(m >> (8u * i));
    bytes_of(seed + 1, o + p, 11u * ns); p += 11u * ns;
    o[p++] = (uint8_t)mc;
    for (i = 0; i < mc; i++, p += 4) { o[p] = (uint8_t)i; o[p + 1] = (uint8_t)(i % 2 ? 34 : 5 + i); o[p + 2] = (uint8_t)(i * 3); o[p + 3] = 0; if (i % 2 == 0 && o[p + 1] >= 33 && o[p + 1] <= 36) o[p + 1] = 9; }
    pj4_p16(o + p, nn); p += 2;
    for (i = 0; i < nn; i++, p += 10) {
        bytes_of(seed + 100 + i, o + p, 10);
        o[p + 4] = (uint8_t)(40 + i); o[p + 5] = 100; o[p + 8] = 0; o[p + 9] = (uint8_t)(i & 1u);
        if (ctl && i == 0) { o[p + 4] = 125; o[p + 9] = 2; }
    }
    (void)k;
    return p;
}
static int test_repack(void)
{
    static uint8_t in[600], out[600];
    uint32_t n, mc = 6, i, drops = 0, ns = 4, q;
    int r;
    n = mk_pattern(1, ns, mc, 3, 77, in, 0);
    for (i = 0; i < mc; i++) drops += in[20 + 11 * 4 + 1 + 4 * i + 1] >= 33 && in[20 + 11 * 4 + 1 + 4 * i + 1] <= 36;
    r = pj4_repack(in, n, out);
    check("repack: a version 1 pattern with macro events: they are dropped, the version byte is set", r == (int)(n - 4 * drops) && out[0] == PJ4_PHVER && drops == 3 && out[20 + 44] == mc - drops);
    q = 20 + 44;
    check(".. the head, the steps and the notes are equal byte for byte, the kept events in order", !memcmp(in + 1, out + 1, q - 1) && !memcmp(in + q + 1 + 4 * mc, out + q + 1 + 4 * (mc - drops), 2 + 30));
    n = mk_pattern(2, ns, mc, 3, 78, in, 0);
    r = pj4_repack(in, n, out);
    check("a version 2 pattern (round 13) likewise", r == (int)(n - 4 * drops) && out[0] == PJ4_PHVER);
    n = mk_pattern(PJ4_PHVER, ns, mc, 3, 79, in, 1);
    r = pj4_repack(in, n, out);
    check("a pattern of the revision keeps every motion event (macro ids are legal in it) and its control note (8.2b)", r == (int)n && !memcmp(in + 1, out + 1, n - 1) && out[20 + 44 + 1 + 4 * mc + 2 + 9] == 2);
    n = mk_pattern(2, ns, mc, 3, 80, in, 1);
    check("a flagged control note in an old version pattern is refused (the old readers' check refuses it)", pj4_repack(in, n, out) == -1);
    n = mk_pattern(PJ4_PHVER, ns, mc, 3, 81, in, 1);
    in[20 + 44 + 1 + 4 * mc + 2 + 8] = 3;                                         /* a pitch on the control note */
    check("a flag with a pitch, on a number below 124, or without a velocity is refused", pj4_repack(in, n, out) == -1 && ({ in[20 + 44 + 1 + 4 * mc + 2 + 8] = 0; in[20 + 44 + 1 + 4 * mc + 2 + 4] = 123; pj4_repack(in, n, out); }) == -1 &&
          ({ in[20 + 44 + 1 + 4 * mc + 2 + 4] = 125; in[20 + 44 + 1 + 4 * mc + 2 + 5] = 0; pj4_repack(in, n, out); }) == -1);
    n = mk_pattern(1, ns, mc, 3, 82, in, 0);
    check("a pattern that does not parse (cut short, extra bytes, a bad version) is refused", pj4_repack(in, n - 1, out) == -1 && pj4_repack(in, n + 1, out) == -1 && ({ in[0] = 9; pj4_repack(in, n, out); }) == -1);
    return 0;
}

/* ------------------------------------------------------------ 6 --------- */
static uint64_t cv_names[4];
static uint8_t cv_old_rec[4][3600];
static uint32_t cv_old_len[4];
static uint8_t cv_pat[64][1800];
static int cv_heavy;
static uint32_t cv_pat_len[64], cv_pat_ver[64];
static uint8_t cv_work[PJ4_WORK];
static void cv_fixture(uint32_t nslots, const uint32_t (*use)[16], uint32_t base_seed)
{
    uint32_t k, s, i;
    memset(cv_names, 0, sizeof cv_names);
    for (s = 0; s < nslots; s++)
        for (i = 0; use[s][i] != 0xFFFFu; i++) cv_names[s] |= (uint64_t)1 << use[s][i];
    for (k = 0; k < 64; k++) {
        uint32_t any = 0;
        for (s = 0; s < nslots; s++) any |= (uint32_t)((cv_names[s] >> k) & 1u);
        cv_pat_ver[k] = k % 3u == 0 ? 1u : 2u;
        cv_pat_len[k] = any ? mk_pattern(cv_pat_ver[k], 2 + k % 5, 4, cv_heavy ? 150 + k % 5 : 1 + k % 3, base_seed + k * 31u, cv_pat[k], 0) : 0;
    }
    for (k = 0; k < 16; k++) {
        static uint8_t pay[4 * 1800 + 8];
        uint32_t off = 8, j, any = 0;
        for (j = 0; j < 4; j++) { pj4_p16(pay + 2 * j, cv_pat_len[4 * k + j]); any |= cv_pat_len[4 * k + j]; }
        for (j = 0; j < 4; j++) { memcpy(pay + off, cv_pat[4 * k + j], cv_pat_len[4 * k + j]); off += cv_pat_len[4 * k + j]; }
        if (any) ocs_write(OCS_POOL, k, pay, off);
    }
    for (s = 0; s < nslots; s++) {
        cv_old_len[s] = 2400 + s * 300;
        bytes_of(base_seed + 5000 + s, cv_old_rec[s], cv_old_len[s]);
        ocs_write(OCS_PROJECT, s, cv_old_rec[s], cv_old_len[s]);
    }
}
static void cv_mk(uint32_t slot, pj4_head_t *h) { *h = mkhead(slot ? "TWO" : "ONE", 1000 + slot); }
static uint8_t cv_status[4];
static uint32_t cv_over[4];
static int cv_run(void *a) { (void)a; pj4_boot(cv_names, cv_work, cv_mk, cv_status, cv_over); return 0; }
/* slot s converted: every phrase it names is its own, the repack of the old one, and the old record's bytes ride in OJ3B */
static int cv_slot_ok(uint32_t s)
{
    static uint8_t b[PJ4_PHMAX + 8], rp[PJ4_PHMAX + 8], rec[3700];
    uint32_t k, cnt = 0, id;
    pj4_head_t h;
    if (pj4_head_load(s, &h) || pj4_dir_load(s, &pj4_d0)) return 0;
    for (id = 1; id <= 255; id++) cnt += pj4_d0.chunk[id] != PJ4_NONE;
    for (k = 0; k < 64; k++) {
        int n, r;
        if (!((cv_names[s] >> k) & 1u)) continue;
        r = pj4_repack(cv_pat[k], cv_pat_len[k], rp);
        n = pj4_phrase_read(s, k + 1u, b, sizeof b);
        if (r < 0 || n != r || memcmp(b, rp, (uint32_t)r)) return 0;
    }
    { uint32_t names = 0; for (k = 0; k < 64; k++) names += (uint32_t)((cv_names[s] >> k) & 1u); if (names != cnt) return 0; }
    if (pj4_sec_read(s, PJ4_T_OJ3B, rec, sizeof rec) != (int)cv_old_len[s] || memcmp(rec, cv_old_rec[s], cv_old_len[s])) return 0;
    return h.rev == 1 && h.uid == 1000 + s;
}
static int cv_state_ok(int finished)
{
    uint32_t s;
    int ok = 1;
    for (s = 0; s < 2; s++) {
        int has_new = pj4_dir_load(s, &pj4_d0) == 0, has_old = ocs_exists(OCS_PROJECT, s);
        static uint8_t rec[3700];
        if (!has_new && !has_old) return 0;
        if (has_old && (ocs_read(OCS_PROJECT, s, rec, sizeof rec) != (int)cv_old_len[s] || memcmp(rec, cv_old_rec[s], cv_old_len[s]))) ok = 0;
        if (has_new && !cv_slot_ok(s)) ok = 0;
        if (has_old && has_new && !finished && 0) ok = 0;
        for (uint32_t k = 0; k < 64 && !has_new && ok; k++) {                      /* the old pool must still hold what an unconverted slot names */
            static uint8_t b[PJ4_PHMAX];
            if (((cv_names[s] >> k) & 1u) && pj4_old_phrase(k, b, sizeof b) != (int)cv_pat_len[k]) ok = 0;
        }
    }
    return ok && no_orphans();
}
static int cv_chk(void *a, int finished)
{
    long e = 0, p = 0;
    (void)a;
    if (!cv_state_ok(finished)) return 0;
    /* the boot again, uncut: converted once; a third boot does nothing */
    cv_run(0);
    if (cv_status[0] != 1 || cv_status[1] != 1 || !cv_slot_ok(0) || !cv_slot_ok(1) || ocs_exists(OCS_PROJECT, 0) || ocs_exists(OCS_PROJECT, 1) || !no_orphans()) return 0;
    for (uint32_t k = 0; k < 16; k++) if (ocs_exists(OCS_POOL, k)) return 0;      /* (the old pool is erased: no old record names it) */
    e = n_erase; p = n_prog;
    cv_run(0);
    return n_erase == e && n_prog == p && cv_slot_ok(0) && cv_slot_ok(1);
}
static int test_convert(void)
{
    static const uint32_t use[2][16] = {{0, 1, 5, 9, 10, 0xFFFF}, {1, 5, 6, 40, 0xFFFF}};
    cutter_t c = {cv_run, cv_chk, 0};
    int failed = 0, n0, n1;
    char what[220];
    wipe(); claim();
    cv_fixture(2, use, 4000);
    snapshot();
    cv_run(0);
    check("CONVERSION: two old projects sharing phrases 2 and 6 convert; each has its own copy, the repacked bytes, the old record in OJ3B", cv_status[0] == 1 && cv_status[1] == 1 && cv_slot_ok(0) && cv_slot_ok(1));
    check(".. the old records and the old pool are gone, nothing is orphaned", !ocs_exists(OCS_PROJECT, 0) && !ocs_exists(OCS_PROJECT, 1) && !ocs_exists(OCS_POOL, 0) && !ocs_exists(OCS_POOL, 2) && no_orphans());
    {   /* a phrase two projects used is now two phrases: edit one, the other is unchanged */
        pset_t e = {0};
        static uint8_t b[PJ4_PHMAX], rp[PJ4_PHMAX];
        pj4_head_t h = mkhead("ONE", 1000);
        uint32_t over;
        pset_add(&e, 2, 300, 4242);
        check("the shared phrase 2 (slot 0 and 1 both used it) is two copies: a save to slot 0 leaves slot 1's alone",
              save(0, &e, &h, 0, 0, &over) == 0 && pj4_phrase_read(1, 2, b, sizeof b) == pj4_repack(cv_pat[1], cv_pat_len[1], rp) && ({ uint32_t l = (uint32_t)pj4_repack(cv_pat[1], cv_pat_len[1], rp); !memcmp(b, rp, l); }) && cv_slot_ok(1));
    }
    restore();
    n0 = cut_all(&c, 0, 1, &failed);
    snprintf(what, sizeof what, "CONVERSION, power cut at each of %d erase and program calls of the boot, then the boot again: converted once, nothing lost, nothing orphaned, a third boot does nothing", n0);
    check(what, !failed && n0 > 40);
    failed = 0;
    n1 = cut_all(&c, 2, 1, &failed);
    snprintf(what, sizeof what, "CONVERSION, power gone just before each of %d erase and program calls (the old record erased or not): the same", n1);
    check(what, !failed && n1 > 40);
    failed = 0;
    n1 = cut_all(&c, 1, 97, &failed);
    snprintf(what, sizeof what, "CONVERSION, power cut after each 97th programmed byte (%d points): the same", n1);
    check(what, !failed && n1 > 20);
    return 0;
}

/* ------------------------------------------------------------ 7 --------- */
static pset_t rs_old_new;
static int rs_run(void *a)
{
    (void)a;
    pj4_slot_clear(0);                         /* the restore replaces the slot whole: new records first */
    if (dead) return -1;
    cv_fixture(1, (const uint32_t (*)[16]) & (const uint32_t[16]){2, 3, 0xFFFF}, 9000);   /* the old backup's records */
    if (dead) return -1;
    pj4_boot(cv_names, cv_work, cv_mk, cv_status, cv_over);
    return 0;
}
static int rs_chk(void *a, int finished)
{
    (void)a;
    if (finished) {
        cv_names[0] = (uint64_t)1 << 2 | (uint64_t)1 << 3;
        return cv_status[0] == 1 && cv_slot_ok(0) && !ocs_exists(OCS_PROJECT, 0) && no_orphans();
    }
    return no_orphans();                       /* (a cut part way: the slot is whatever it was, whole; the restore is run again by the player) */
}
static int test_restore(void)
{
    static const uint32_t use0[2][16] = {{0, 1, 0xFFFF}, {0xFFFF}};
    cutter_t c = {rs_run, rs_chk, 0};
    int failed = 0, n0;
    char what[200];
    wipe(); claim();
    cv_fixture(1, use0, 2000);
    cv_names[1] = 0;
    cv_run(0);
    check("a restore over a converted slot: the slot is converted (before)", cv_status[0] == 1 && pj4_dir_load(0, &pj4_d0) == 0 && !ocs_exists(OCS_PROJECT, 0));
    snapshot();
    rs_run(0);
    cv_names[0] = (uint64_t)1 << 2 | (uint64_t)1 << 3;
    check("then a restore of an old backup (new records erased, the old ones written), then the boot: converted once, with the backup's phrases, not erased", cv_status[0] == 1 && cv_slot_ok(0) && !ocs_exists(OCS_PROJECT, 0) && no_orphans());
    cv_names[0] = 0;
    {   /* the cuts */
        restore();
        n0 = cut_all(&c, 0, 1, &failed);
        snprintf(what, sizeof what, "RESTORE, power cut at each of %d erase and program calls: never an orphan; uncut it ends converted once", n0);
        check(what, !failed && n0 > 40);
        failed = 0;
        n0 = cut_all(&c, 2, 1, &failed);
        snprintf(what, sizeof what, "RESTORE, power gone just before each of %d erase and program calls: the same", n0);
        check(what, !failed && n0 > 40);
    }
    return 0;
}

/* ------------------------------------------------------------ 8 --------- */
static int test_worst(void)
{
    static uint32_t use[4][16];
    static uint8_t rec[3700];
    uint32_t s, i, round;
    wipe(); claim();
    for (s = 0; s < 4; s++) { for (i = 0; i < 24; i++) use[s][i % 15] = i < 15 ? i : use[s][i % 15]; }
    /* every project uses the same 24 old patterns (the worst case: every phrase in all four): sixteen of them fit a row of the fixture table; the names are set directly */
    cv_heavy = 1;
    for (s = 0; s < 4; s++) { for (i = 0; i < 15; i++) use[s][i] = i; use[s][15] = 0xFFFF; }
    cv_fixture(4, (const uint32_t (*)[16])use, 31000);
    cv_heavy = 0;
    cv_run(0);
    check("the worst case (four projects, each using the same fifteen heavy phrases): slots 0 and 1 convert, 2 and 3 do not fit: OLD FORMAT, with the KB needed",
          cv_status[0] == 1 && cv_status[1] == 1 && cv_status[2] == 2 && cv_status[3] == 2 && cv_over[2] > 0 && cv_over[3] > 0);
    for (s = 2; s < 4; s++)
        check("an OLD FORMAT slot keeps its old record readable and its old phrases", ocs_read(OCS_PROJECT, s, rec, sizeof rec) == (int)cv_old_len[s] && !memcmp(rec, cv_old_rec[s], cv_old_len[s]) && cv_pat_len[3] == (uint32_t)pj4_old_phrase(3, rec, 1800));
    check(".. and the converted ones are whole, no orphan", cv_slot_ok(0) && cv_slot_ok(1) && no_orphans());
    for (round = 0; round < 2; round++) {                                       /* room is made (a converted project cleared), the boot converts the next */
        pj4_slot_clear(round);
        cv_run(0);
        check("after room is made (CLEAR a converted project) the next boot converts the next slot, once; no orphan; the old pool still serves the rest",
              cv_status[round + 2] == 1 && cv_slot_ok(round + 2) && no_orphans() && (round == 1 || (cv_status[3] == 2 && ocs_read(OCS_PROJECT, 3, rec, sizeof rec) == (int)cv_old_len[3] && pj4_old_phrase(3, rec, 1800) == (int)cv_pat_len[3])));
    }
    check("at the end every old record and the old pool are gone", !ocs_exists(OCS_PROJECT, 2) && !ocs_exists(OCS_PROJECT, 3) && !ocs_exists(OCS_POOL, 0) && cv_slot_ok(2) && cv_slot_ok(3));
    return 0;
}

/* ------------------------------------------------------------ 9, 10 ----- */
static int test_head_sections(void)
{
    pset_t a = {0};
    pj4_head_t h = mkhead("UID", 0xBEEF), g;
    pj4_sec_t s[3];
    static uint8_t buf[200], sec[3][40];
    uint32_t over, i, n = 0;
    static const uint32_t none[1] = {0};
    wipe(); claim();
    pset_add(&a, 1, 100, 1);
    for (i = 0; i < 3; i++) { bytes_of(60 + i, sec[i], 40); s[i].tag = PJ4_TAG('U', 'N', 'K', '0' + i); s[i].len = 17 + i * 7; s[i].p = sec[i]; }
    s[1].len = 0; s[1].p = 0;
    save(2, &a, &h, s, 3, &over);
    h.rev = 0;
    save(2, &a, &h, s, 3, &over);
    pj4_head_load(2, &g);
    check("the uid survives two saves and the revision counter rises with each (1, 2)", g.uid == 0xBEEF && g.rev == 2);
    check("a record with three unknown sections (one empty) carries them: read back byte for byte", pj4_sec_read(2, s[0].tag, buf, sizeof buf) == 17 && !memcmp(buf, sec[0], 17) && pj4_sec_read(2, s[1].tag, buf, sizeof buf) == 0 && pj4_sec_read(2, s[2].tag, buf, sizeof buf) == 31 && !memcmp(buf, sec[2], 31));
    pj4_sec_t again[6];
    check("pj4_carry copies all three, in order", pj4_carry(2, none, 0, buf, sizeof buf, again, &n, 6) == 0 && n == 3 && again[0].tag == s[0].tag && again[2].len == 31);
    h = mkhead("UID", 0xBEEF);
    save(2, &a, &h, again, n, &over);
    check("and a save of the carried sections writes them again unchanged", pj4_sec_read(2, s[2].tag, buf, sizeof buf) == 31 && !memcmp(buf, sec[2], 31) && pj4_sec_read(2, s[0].tag, buf, sizeof buf) == 17);
    check("a section with a core tag, or a tag twice, is refused", ({ pj4_sec_t d[2] = {{PJ4_T_HEAD, 0, 0}, {0, 0, 0}}; pj4_save(2, &h, d, 1, 0, 0, pget, &a, &over); }) == PJ4_BAD &&
          ({ pj4_sec_t d[2] = {{PJ4_TAG('A', 'A', 'A', 'A'), 0, 0}, {PJ4_TAG('A', 'A', 'A', 'A'), 0, 0}}; pj4_save(2, &h, d, 2, 0, 0, pget, &a, &over); }) == PJ4_BAD);
    return 0;
}

/* ------------------------------------------------------------ 11 -------- */
static int test_old_store(void)
{
    static const uint32_t use[2][16] = {{0, 1, 5, 9, 10, 0xFFFF}, {1, 5, 6, 40, 0xFFFF}};
    static uint8_t sent[4096];
    pset_t keep = {0};
    uint32_t s, id, i;
    wipe(); claim();
    cv_fixture(2, use, 4000);
    cv_run(0);
    /* "the old firmware": the store's boot scan, then it asks for what it knows: old ids only */
    ocs_boot();
    check("the old firmware's scan: it finds no old project record and no old pool chunk (it sees empty slots)", !ocs_exists(OCS_PROJECT, 0) && !ocs_exists(OCS_PROJECT, 1) && !ocs_exists(OCS_POOL, 0) && !ocs_exists(OCS_POOL, 1));
    for (s = 0; s < 4; s++) for (id = 0; id < 16; id++) { if (ocs_read(OCS_PROJECT, s, sent, sizeof sent) >= 0 || ocs_read(OCS_POOL, id, sent, sizeof sent) >= 0) { check("an old id reads something", 0); } }
    check("the converted projects read as they did", cv_slot_ok(0) && cv_slot_ok(1));
    /* the old firmware saves a project and a pool chunk of its own beside them */
    bytes_of(5, sent, 3000);
    check("an old-format save (project 0, pool chunk 0) goes to free sectors", ocs_write(OCS_PROJECT, 0, sent, 3000) == 0 && ocs_write(OCS_POOL, 0, sent, 3000) == 0);
    check(".. every converted phrase and head still reads, nothing erased", cv_slot_ok(0) && cv_slot_ok(1) && no_orphans());
    for (i = 0; i < 20; i++) ocs_write(OCS_PROJECT, 0, sent, 3000);                 /* (wear: many saves) */
    check(".. after twenty more saves too", cv_slot_ok(0) && cv_slot_ok(1));
    (void)keep;
    return 0;
}

/* item 11, the two halves (tests/pj4_old_store.sh): --dump FILE writes the NOR of a converted store; --verify FILE reads the NOR the old store code left and
 * checks that every converted phrase and head still reads and nothing of ours was erased */
static int image(int dump, const char *f)
{
    static const uint32_t use[2][16] = {{0, 1, 5, 9, 10, 0xFFFF}, {1, 5, 6, 40, 0xFFFF}};
    FILE *fp;
    wipe(); claim();
    cv_fixture(2, use, 4000);
    if (dump) {
        cv_run(0);
        if (!cv_slot_ok(0) || !cv_slot_ok(1)) { printf("pj4: the image to dump is not converted\n"); return 1; }
        fp = fopen(f, "wb"); if (!fp || fwrite(nor, 1, sizeof nor, fp) != sizeof nor) return 1; fclose(fp);
        return 0;
    }
    fp = fopen(f, "rb"); if (!fp || fread(nor, 1, sizeof nor, fp) != sizeof nor) { printf("pj4: no image\n"); return 1; } fclose(fp);
    ocs_boot();
    check("the Release 2 store code ran on the converted store and wrote its own records beside it: every converted phrase and head still reads", cv_slot_ok(0) && cv_slot_ok(1));
    check(".. no chunk of ours was erased or left orphaned, and its old-format records are there, and they are not read as ours", no_orphans() && ocs_exists(OCS_PROJECT, 0) && ocs_exists(OCS_POOL, 0));
    return bad;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "--dump")) return image(1, argv[2]);
    if (argc == 3 && !strcmp(argv[1], "--verify")) { image(0, argv[2]); printf("pj4: %s\n", bad ? "FAILED" : "old store: passed"); return bad != 0; }
    test_basic();
    test_sizes();
    test_full();
    test_save_cuts();
    test_repack();
    test_convert();
    test_restore();
    test_worst();
    test_head_sections();
    test_old_store();
    printf("pj4: %s\n", bad ? "FAILED" : "all checks passed");
    return bad ? 1 : 0;
}
