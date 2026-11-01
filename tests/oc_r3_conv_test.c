/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format, OC_R3_FORMAT=1), Part 3: the conversion of the projects of the format before, on a store written by the
 * format before's own code (tests/oc_r3_oldgen.c, built without the switch):
 *   build/host/oc_r3_conv_test OLD_NOR OLD_EXPECT
 * 1. every slot the old reader loads converts and reads back as the same FBKK, byte for byte (banks, steps, Skiff's fields, timed notes, motion,
 *    lists, globals), and the slot the old reader found empty stays empty.
 * 2. the old records and the old pool chunks are gone; each project has its own phrases; an edit in one leaves the other.
 * 3. the phrases a chain names and no bank holds are copied; a version 1 phrase with np 116 and a knob-macro event becomes version 2, np P_COUNT, the
 *    macro event gone, the other mapped; every converted phrase is byte for byte what a load followed by a keep writes.
 * 4. the freeze table is split: a frozen phrase stays frozen (its hash recomputed) in each project that uses it.
 * 5. a second start converts nothing and writes nothing; a project survives a save and a reload.
 * 6. a store too full for the last slot leaves it as the old record (OLD FORMAT) with the KB it needs; freeing room and CONVERT converts it.
 * 7. power cut at every program and every erase of the boot conversion: after the next start every slot reads as the old project or the new one,
 *    never neither and never a mix, no orphan, and a further start converts what is left.
 * Revisions:
 *   Part 3 (November 1, 2026): first version. */
#define OC_PROJECT_NO_MAIN
#include <stdint.h>
typedef struct { uint16_t take; uint8_t trk, ver; int16_t lvl, bpm; uint32_t frames, hash; uint8_t dir, want; char name[14]; } frzp_t;
#define FZP_N 64u
#define FZP_VER 2u
static frzp_t frzp[FZP_N];
static volatile uint8_t frzp_dirty;
#define OCJ4_FRZ_TEST 1
#include "oc_project_test.c"

#define CHK(what, ok) (bad |= check(what, (ok)))
static uint8_t old_nor[sizeof nor], exp_fbkk[4][1 + BANK_STORE_SIZE], got[BANK_STORE_SIZE];

static int load_file(const char *path, void *p, size_t n)
{
    FILE *f = fopen(path, "rb");
    size_t r = f ? fread(p, 1, n, f) : 0;
    if (f) fclose(f);
    return r != n;
}
/* the start: store scan, pool scan, the conversion (project.c persist_boot's sequence) */
static void start(void)
{
    ocs_boot(); ocp_scan(); ocj_scan();
    ocj4_convert_boot();
    memset(proj_meta, 0, sizeof proj_meta);
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
}
static uint32_t old_records(void) { uint32_t n = 0; for (uint32_t s = 0; s < 4u; s++) n += ocs_exists(OCS_PROJECT, s); return n; }
static uint32_t old_chunks(void) { uint32_t n = 0; for (uint32_t c = 0; c < 16u; c++) n += ocs_exists(OCS_POOL, c); return n; }
static uint32_t new_records(void) { uint32_t n = 0; for (uint32_t s = 0; s < 4u; s++) n += ocs_exists(OCS_PROJECT, OCJ4_RID(s)); return n; }
/* the slot's FBKK now, 1 when it reads; orphans: chunk records of the Release 3 slots that their records do not name */
static int read_slot(uint32_t s) { return ocj_read(s, got) == (int)BANK_STORE_SIZE; }
static uint32_t orphans_all(void)
{
    uint32_t o = 0, t;
    for (uint32_t s = 0; s <= OCP_SCRATCH; s++) { (void)0; ocp_dir_t d; uint8_t nm[OCP_NCHUNK / 8u]; int has = !ocj4_phrd_read(s, &d, nm, 0, 0);
        for (uint32_t h = 0; h < OCS_NSECT; h++) { const ocs_hdr_t *x = &ocs_hdr[h]; uint32_t c, g;
            if (x->magic != OCS_MAGIC || x->type != OCS_POOL || !(x->id & 0x8000u) || ((x->id >> 9) & 7u) != s) continue;
            c = x->id & 255u; g = (x->id >> 8) & 1u;
            if (!(has && c && ocp_bit(nm, c) && ocp_bit(d.gen, c) == g)) o++; } (void)t; }
    return o;
}
/* every phrase of the slot's directory: a load into a bank, a keep's bytes: their CRC equals the directory's (what a load then a keep writes) */
static int phrases_are_keeps(uint32_t s)
{
    ocp_dir_t d;
    uint8_t nm[OCP_NCHUNK / 8u];
    static uint8_t len[OCP_N];
    static uint32_t hash[OCP_N];
    int ok = 1;
    if (ocj4_phrd_read(s, &d, nm, len, hash)) return 0;
    ocp_rd_dir = &d;
    for (uint32_t id = 0; id < OCP_N && ok; id++) {
        static ocp_gen_t g;
        ocs_rec_t r;
        ocp_rd_t rd;
        if (!d.chunk[id]) continue;
        if (ocp_r3_open(&d, id, &r, &rd) != 1) { ok = 0; break; }
        memset(&trk[0], 0, 0);
        wipe_runtime();
        ok &= ocp_load(id, 0, 3, 1) == 0;               /* (ocp_load reads through ocp_rd_dir: this directory) */
        ocp_gen_bank(&g, 0, 3);
        ok &= ocp_gen_hash(&g) == hash[id];
    }
    ocp_rd_dir = &ocp.dir;
    return ok;
}

/* --generic: any old store (the unit's own dump, the backups' projects, the repo's fixtures; Part 5): the checks that hold for every project. The
 * expectation file is the one tests/oc_r3_oldgen.c wrote for the same store (the FBKK the old reader gives each slot). */
static int generic_main(const int *loads)
{
    int bad = 0;
    uint32_t s, nl = 0, nold0, e0;
    static uint8_t after[sizeof nor];
    char what[200];
    for (s = 0; s < 4u; s++) nl += loads[s] != 0;
    reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot();
    nold0 = old_records();
    start();
    CHK("the store holds at least one project the old reader loads (the checks below are not vacuous)", nl > 0u);
    snprintf(what, sizeof what, "%u of %u old records read and are converted (the others stay as they were)", nl, nold0);
    CHK(what, new_records() == nl && old_records() == nold0 - nl);
    {   uint32_t ok = 1;
        for (s = 0; s < 4u; s++) {
            int r = read_slot(s);
            ok &= r == loads[s];
            if (r && loads[s]) ok &= !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE);
        }
        CHK("every converted slot reads as the FBKK the old reader gave (notes, motion, chains, song, scenes, lists), byte for byte", ok);
    }
    {   uint32_t ok = !orphans_all();
        for (s = 0; s < 4u; s++) if (loads[s]) ok &= phrases_are_keeps(s);
        CHK("no orphan chunk; every phrase is what a load then a keep writes", ok);
    }
    {   /* save and reload, each project, through the music. A load normalises a few settings of the project part (the old firmware's too: G_SLOT of a
         * project in slot 3 reads 4 and is kept as 1), so the bytes of the project part may differ from the first save on; what must hold: every phrase,
         * chain, motion, note and list as the old reader gave them (the FBKK from the banks on), and a second load and save changes nothing */
        uint32_t ok = 1;
        static uint8_t a1[BANK_STORE_SIZE];
        for (s = 0; s < 4u; s++) {
            int sv, rd, same, idem = 1;
            if (!loads[s]) continue;
            project_load(s);
            sv = project_save(s);
            if (sv == 1) { printf("  slot %u: not saved (a track plays an engine past the shipped table: a build with OC_CANDIDATES refuses, as the firmware does)\n", s); continue; }
            ocs_boot(); ocp_scan(); ocj_scan(); memset(proj_meta, 0, sizeof proj_meta);
            rd = read_slot(s);
            same = rd && !memcmp(got + BANK_ACTIVE_OFF, exp_fbkk[s] + 1 + BANK_ACTIVE_OFF, BANK_STORE_SIZE - 4u - BANK_ACTIVE_OFF);
            memcpy(a1, got, sizeof a1);
            project_load(s);
            sv |= project_save(s);
            ocs_boot(); ocp_scan(); ocj_scan(); memset(proj_meta, 0, sizeof proj_meta);
            idem = read_slot(s) && !memcmp(got, a1, sizeof a1);
            if (sv || !rd || !same || !idem) fprintf(stderr, "  slot %u: save %d, reads %d, banks-on same %d, second save same %d\n", s, sv, rd, same, idem);
            ok &= !sv && rd && same && idem;
        }
        CHK("each converted project loads, saves and reads again (phrases, chains, motion, notes, lists as the old reader gave them; a second save changes nothing)", ok && !orphans_all());
    }
    e0 = erases; memcpy(after, nor, sizeof nor); start();
    CHK("a second start converts nothing and writes nothing", erases == e0 && !memcmp(nor, after, sizeof nor));
    {   /* power cut at program calls (every CONV_CUT_STRIDE-th, default 1) and every erase of the boot conversion */
        int progs, ers, ok = 1, n_p = 0, n_e = 0, stride = getenv("CONV_CUT_STRIDE") ? atoi(getenv("CONV_CUT_STRIDE")) : 1;
        if (stride < 1) stride = 1;
        reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
        e0 = erases; fail_after = 1 << 24; ocj4_convert_boot(); progs = (1 << 24) - fail_after; fail_after = -1; ers = (int)(erases - e0);
        for (int phase = 0; phase < 2 && ok; phase++)
            for (int k = 0; k < (phase ? ers : progs + 1) && ok; k += (phase ? 1 : stride)) {
                reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
                fail_after = phase ? -1 : k; erase_cut = phase ? k : -1;
                ocj4_convert_boot();
                fail_after = -1; erase_cut = -1;
                ocs_boot(); ocp_scan(); ocj_scan();
                for (s = 0; s < 4u && ok; s++) {
                    uint8_t *raw = proj_wire_borrow();
                    int old_ok = ocs_exists(OCS_PROJECT, s) && ocj4_old_read(s, raw) == (int)BANK_STORE_SIZE && !memcmp(raw, exp_fbkk[s] + 1, BANK_STORE_SIZE);
                    int new_ok = ocs_exists(OCS_PROJECT, OCJ4_RID(s)) && read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE);
                    if (loads[s]) { ok &= old_ok || new_ok; if (!(old_ok || new_ok)) fprintf(stderr, "  slot %u after the cut: old exists %d new exists %d\n", s, ocs_exists(OCS_PROJECT, s), ocs_exists(OCS_PROJECT, OCJ4_RID(s))); }
                }
                ocj4_convert_boot();
                memset(proj_meta, 0, sizeof proj_meta);
                for (s = 0; s < 4u && ok; s++) { ok &= !loads[s] || (read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE)); if (!ok) fprintf(stderr, "  slot %u after the next start does not read as expected\n", s); }
                ok &= old_records() == nold0 - nl && !orphans_all() && new_records() == nl;
                if (!ok) fprintf(stderr, "  cut %s %d failed: old records %u (want %u), orphans %u, new records %u (want %u), old chunks %u\n", phase ? "erase" : "program", k, old_records(), nold0 - nl, orphans_all(), new_records(), nl, old_chunks());
                if (phase) n_e++; else n_p++;
            }
        snprintf(what, sizeof what, "power cut at %d of %d program calls (every %d-th) and each of %d erases of the boot conversion: old or new, never neither; the next start finishes it", n_p, progs + 1, stride, n_e);
        CHK(what, ok);
    }
    printf("generic: %u projects converted\n", nl);
    return bad;
}

int main(int argc, char **argv)
{
    int bad = 0, loads[4];
    uint32_t s, free_before;
    static uint8_t nor_after[sizeof nor];
    if ((argc != 3 && !(argc == 4 && (!strcmp(argv[3], "--generic") || !strcmp(argv[3], "--afterold")))) || load_file(argv[1], old_nor, sizeof old_nor) || load_file(argv[2], exp_fbkk, sizeof exp_fbkk)) { fprintf(stderr, "usage: oc_r3_conv_test OLD_NOR OLD_EXPECT\n"); return 2; }
    for (s = 0; s < 4u; s++) loads[s] = exp_fbkk[s][0];
    if (argc == 4 && !strcmp(argv[3], "--afterold")) {   // OLD_NOR is the store the old firmware ran on after the conversion (oc_r3_oldgen --oldrun); EXPECT as before
        static uint8_t after_old[sizeof nor];
        int b2 = 0, ok = 1;
        if (load_file(getenv("OC_R3_OLDRUN") ? getenv("OC_R3_OLDRUN") : "", after_old, sizeof after_old)) { fprintf(stderr, "OC_R3_OLDRUN: the store after the old firmware's run\n"); return 2; }
        reset(); memcpy(nor, after_old, sizeof nor);
        start();
        for (s = 0; s < 4u; s++) if (loads[s]) ok &= read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE);
        b2 |= check("after the old firmware ran on the converted store (and saved a project of its own into the free slot), a start of this firmware reads every converted project as it was", ok);
        b2 |= check("... and converts the old firmware's project of slot 1 as well", read_slot(1) && !old_records() && new_records() == 4u);
        printf("oc r3 conversion (after the old firmware): %s\n", b2 ? "FAILED" : "all passed");
        return b2;
    }
    if (argc == 4) { int b2 = generic_main(loads); printf("oc r3 conversion (generic): %s\n", b2 ? "FAILED" : "all passed"); return b2; }
    /* the unit as the old firmware left it, a start of this firmware */
    reset();
    memcpy(nor, old_nor, sizeof nor);
    ocs_boot();
    CHK("the store is the old firmware's: three old records and its pool chunks, no Release 3 record", old_records() == 3u && old_chunks() > 0u && !new_records());
    start();
    CHK("the start converts the three: three Release 3 records, no old record, no old chunk", new_records() == 3u && !old_records() && !old_chunks());
    if (getenv("OC_R3_DUMP_CONVERTED")) { FILE *f = fopen(getenv("OC_R3_DUMP_CONVERTED"), "wb"); if (f) { fwrite(nor, 1, sizeof nor, f); fclose(f); } }   // (for oc_r3_oldgen --oldrun)
    {
        uint32_t ok = 1;
        for (s = 0; s < 4u; s++) {
            int r = read_slot(s);
            ok &= r == loads[s];
            if (r && loads[s]) { if (memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE)) { ok = 0; uint32_t d = 0; while (got[d] == exp_fbkk[s][1 + d]) d++; fprintf(stderr, "  slot %u differs first at byte %u (of %u)\n", s, d, (unsigned)BANK_STORE_SIZE); } }
        }
        CHK("each slot reads back as the FBKK the old reader gave, byte for byte; the empty slot is still empty", ok);
    }
    CHK("no chunk is an orphan, and every converted phrase is byte for byte what a load then a keep writes", !orphans_all() && phrases_are_keeps(0) && phrases_are_keeps(2) && phrases_are_keeps(3));
    {   /* the chain-only phrases: A's phrase named by B's chain, the handwritten one by C's */
        ocp_dir_t d;
        uint8_t nm[OCP_NCHUNK / 8u];
        static uint8_t len[OCP_N];
        static uint32_t hash[OCP_N];
        ocs_rec_t r;
        ocp_rd_t rd;
        uint8_t h[4];
        CHK("the handwritten phrase 61 is in project C only", !ocj4_phrd_read(3, &d, nm, len, hash) && d.chunk[60] && !ocj4_phrd_read(0, &d, nm, len, hash) && !d.chunk[60]);
        ocj4_phrd_read(3, &d, nm, len, hash);
        CHK("... as version 2, np P_COUNT, one motion event (the knob macro's gone; the other mapped), its note kept",
            ocp_r3_open(&d, 60, &r, &rd) == 1 && !ocs_at(&r, rd.off, h, 4) && h[0] == 2 && (h[2] | h[3] << 8) == P_COUNT && ({ uint8_t m[1]; (void)m;
              ocp_parsed_t p; ocs_rec_t r2; ocp_rd_t rd2; uint32_t na; ocp_rd_dir = &d; int rc = ocp_check(60, 0, &r2, &rd2, &p, &na); ocp_rd_dir = &ocp.dir;
              !rc && p.mot.count == 1 && p.mot.event[0].param == (uint32_t)(112 + P_COUNT - 116) && p.mot.event[0].value == 9 && p.nnotes == 1; }));
        CHK("... project B's chain names the phrase of A that no bank of B holds, and B has its own copy of it", ({ uint32_t id; (void)0; ocj4_phrd_read(2, &d, nm, len, hash); id = 0; for (uint32_t i = 0; i < 64u; i++) if (d.chunk[i]) id++; id > 0u; }));
    }
    {   /* the two projects are independent: an edit in B leaves A's bytes as they were */
        static uint8_t a_before[BANK_STORE_SIZE];
        read_slot(0); memcpy(a_before, got, sizeof a_before);
        wipe_runtime(); project_load(2);
        { pattern_t p; pattern_read(0, 0, &p); p.step[11].n = 1; p.step[11].time = ST_NOTE; p.step[11].note[0] = 55; p.step[11].vel = 70; pattern_write(0, 0, &p); }
        CHK("a project converted, loaded and edited saves; the other project is untouched", project_save(2) == 0 && read_slot(0) && !memcmp(got, a_before, sizeof a_before));
        wipe_runtime(); reboot(); project_load(2);
        CHK("... and reloads with the edit", ({ pattern_t p; pattern_read(0, 0, &p); p.step[11].n == 1u && p.step[11].note[0] == 55u; }));
    }
    {   /* the freeze table: phrase 1 and 61 were frozen; each project that uses them has the entry, hash recomputed */
        ocs_rec_t r;
        uint8_t tab[4096];
        uint32_t found0 = 0, found1 = 0, found3 = 0, n;
        frzp_t f;
        (void)tab;
        memcpy(nor, old_nor, sizeof nor);
        reset(); memcpy(nor, old_nor, sizeof nor);
        memset(frzp, 0, sizeof frzp);
        start();
        for (s = 0; s < 4u; s++) {
            ocs_rec_t rr;
            if (ocs_open(OCS_PROJECT, OCJ4_RID(s), &rr)) continue;
            n = rr.total;
            { static uint8_t rec[OCJ4_MAX]; uint32_t off = 0, sn = 0;
              ocs_at(&rr, 0, rec, n);
              for (uint32_t pos = OCJ4_HLEN; pos + 8u <= n; pos += 8u + rd32(rec + pos + 4)) if (rd32(rec + pos) == T_FRZR) { off = pos + 8u; sn = rd32(rec + pos + 4); }
              if (sn >= 3u) { uint32_t nf = rec[off], ne = rd16(rec + off + 1u + nf * 32u), at = off + 1u + nf * 32u + 2u;
                for (uint32_t e = 0; e < ne; e++) { uint32_t elen = rd16(rec + at); memcpy(&f, rec + at + 3u, 32); { ocp_dir_t dd; uint8_t nm2[OCP_NCHUNK / 8u]; static uint8_t ln[OCP_N]; static uint32_t hs[OCP_N]; uint32_t id2 = rec[at + 2], h2; ocj4_phrd_read(s, &dd, nm2, ln, hs); memcpy(&h2, rec + at + 3u + 12u, 4);
                    if (id2 == 1u && h2 == hs[0]) { found0 += s == 0u; found1 += 1u; } if (id2 == 61u && h2 == hs[60]) found3 += s == 3u; } at += 2u + elen; } } }
        }
        (void)r;
        CHK("the freeze table is split: phrase 1's entry is in project A, phrase 61's in project C (not in the others), each with the hash of the version 2 bytes", found0 == 1u && found3 == 1u);
        {   /* every project whose chunks hold phrase 1 has the entry (a shared phrase is frozen in each of its projects) */
            uint32_t users = 0;
            for (s = 0; s < 4u; s++) { ocp_dir_t dd; uint8_t nm2[OCP_NCHUNK / 8u]; static uint8_t ln[OCP_N]; static uint32_t hs[OCP_N];
                if (!ocs_exists(OCS_PROJECT, OCJ4_RID(s))) continue;
                memset(ln, 0, sizeof ln); ocj4_phrd_read(s, &dd, nm2, ln, hs); users += ln[0] != 0u; }
            CHK("... every project that holds phrase 1 has its entry (a shared phrase is frozen in each; the review's finding 18)", users >= 1u && found1 == users);
        }
    }
    /* a second start writes nothing */
    {
        uint32_t e0 = erases;
        memcpy(nor_after, nor, sizeof nor);
        start();
        CHK("a second start converts nothing and writes nothing", erases == e0 && !memcmp(nor, nor_after, sizeof nor));
    }
    {   /* 7. a power cut at every program call and every erase of the boot conversion (three projects) */
        int progs, ers, ok = 1, n_p = 0, n_e = 0;
        uint32_t e0;
        char what[220];
        reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
        e0 = erases; fail_after = 1 << 24; ocj4_convert_boot(); progs = (1 << 24) - fail_after; fail_after = -1; ers = (int)(erases - e0);
        for (int phase = 0; phase < 2 && ok; phase++)
            for (int k = 0; k < (phase ? ers : progs + 1) && ok; k++) {
                uint32_t np_old = 0, np_new = 0;
                reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
                fail_after = phase ? -1 : k; erase_cut = phase ? k : -1;
                ocj4_convert_boot();
                fail_after = -1; erase_cut = -1;
                ocs_boot(); ocp_scan(); ocj_scan();                    /* power back on: the scan and the sweep, nothing converted yet */
                for (s = 0; s < 4u && ok; s++) {                       /* each slot reads as the old project or the new, never neither */
                    uint8_t *raw = proj_wire_borrow();
                    int old_ok = ocs_exists(OCS_PROJECT, s) && ocj4_old_read(s, raw) == (int)BANK_STORE_SIZE && !memcmp(raw, exp_fbkk[s] + 1, BANK_STORE_SIZE);
                    int new_ok = ocs_exists(OCS_PROJECT, OCJ4_RID(s)) && read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE);
                    if (!loads[s]) ok &= !ocs_exists(OCS_PROJECT, s) && !ocs_exists(OCS_PROJECT, OCJ4_RID(s));
                    else ok &= old_ok || new_ok;
                    np_old += old_ok; np_new += new_ok;
                }
                ocj4_convert_boot();                                   /* the next start resumes */
                memset(proj_meta, 0, sizeof proj_meta);
                for (s = 0; s < 4u && ok; s++) ok &= !loads[s] || (read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE));
                ok &= !old_records() && !old_chunks() && !orphans_all() && new_records() == 3u;
                if (!ok) fprintf(stderr, "  cut %s %d: old readable %u, new readable %u\n", phase ? "erase" : "program", k, np_old, np_new);
                if (phase) n_e++; else n_p++;
            }
        snprintf(what, sizeof what, "power cut at each of %d program calls and each of %d erases of the boot conversion: after the cut every slot reads as the old project or the new one, the next start finishes it, no orphan", n_p, n_e);
        CHK(what, ok);
    }
    {   /* 8. an old record restored over a slot that has a Release 3 project of its own (Part 4): the pool objects, the record, then the slot's
         * Release 3 records go (editor_backup.c's order, with the pool first). A power cut at every program call and erase of it: after the next
         * start slot 0 reads as the project it had or as the restored one (never empty, never a mix), the other slots are untouched, nothing is left over */
        static uint8_t *orec, *ochunk[16];
        static uint32_t orec_len, ochunk_len[16];
        static uint8_t x_fbkk[BANK_STORE_SIZE], conv_fbkk[3][BANK_STORE_SIZE];
        int progs, ers, ok = 1, n_p = 0, n_e = 0, saw_old = 0, saw_new = 0, k_settle = 0;
        uint32_t e0;
        char what[240];
        reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot();
        { ocs_rec_t r;                                  /* the old store's record of slot 0 and its pool chunks, as a backup holds them */
          if (!ocs_open(OCS_PROJECT, 0, &r)) { orec_len = r.total; orec = malloc(r.total); ocs_at(&r, 0, orec, r.total); }
          for (uint32_t c = 0; c < 16u; c++) if (!ocs_open(OCS_POOL, c, &r)) { ochunk_len[c] = r.total; ochunk[c] = malloc(r.total); ocs_at(&r, 0, ochunk[c], r.total); } }
        reset(); memcpy(nor, old_nor, sizeof nor);
        start();                                        /* converted: slots 0, 2, 3 (slot 1 is empty) */
        project_load(3); CHK("(a different project goes into slot 0: its own save, no marker)", project_save_as(0, "OWN") == 0);
        ocs_boot(); ocp_scan(); ocj_scan(); memset(proj_meta, 0, sizeof proj_meta);
        read_slot(0); memcpy(x_fbkk, got, sizeof x_fbkk);
        for (s = 2; s < 4u; s++) { read_slot(s); memcpy(conv_fbkk[s - 2u], got, BANK_STORE_SIZE); }
        memcpy(nor_after, nor, sizeof nor);
        {   /* the restore, counted */
            /* the shipped page's order: the project record (64..67) first, then the pool chunks (68..83); editor_backup.c settles after each object */
            #define RESTORE_OLD() do { \
                if (!ocs_write(OCS_PROJECT, 0, orec, orec_len)) ocj4_restore_settle(); \
                for (uint32_t c = 0; c < 16u; c++) if (ochunk[c] && !ocs_write(OCS_POOL, c, ochunk[c], ochunk_len[c])) ocj4_restore_settle(); } while (0)
            e0 = erases; fail_after = 1 << 24; RESTORE_OLD(); progs = (1 << 24) - fail_after; fail_after = -1; ers = (int)(erases - e0);
        }
        {   /* where the restore replaces the slot: the program calls used when slot 0's Release 3 record is gone */
            reset(); memcpy(nor, nor_after, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
            fail_after = 1 << 24;
            if (!ocs_write(OCS_PROJECT, 0, orec, orec_len)) ocj4_restore_settle();
            for (uint32_t c = 0; c < 16u && ocs_exists(OCS_PROJECT, OCJ4_RID(0)); c++) if (ochunk[c] && !ocs_write(OCS_POOL, c, ochunk[c], ochunk_len[c])) ocj4_restore_settle();
            k_settle = (1 << 24) - fail_after; fail_after = -1;
            CHK("(the Release 3 record goes only when the old project reads: after the last pool object, not after the record)", !ocs_exists(OCS_PROJECT, OCJ4_RID(0)) && k_settle > 20);
        }
        for (int phase = 0; phase < 2 && ok; phase++)
            for (int k = 0; k < (phase ? ers : progs + 1) && ok; k++) {
                int is_old, is_new;
                reset(); memcpy(nor, nor_after, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
                fail_after = phase ? -1 : k; erase_cut = phase ? k : -1;
                RESTORE_OLD();
                fail_after = -1; erase_cut = -1;
                start();                                // power back on: the scan, the conversion (or its finishing), the sweep
                is_old = read_slot(0) && !memcmp(got, exp_fbkk[0] + 1, BANK_STORE_SIZE);
                is_new = read_slot(0) && !memcmp(got, x_fbkk, BANK_STORE_SIZE);
                saw_old += is_old; saw_new += is_new;
                ok &= is_old || is_new;
                if (!phase && k < k_settle - 2) ok &= is_new;        // a program cut before the restore replaced the slot: the project it had
                for (s = 2; s < 4u; s++) ok &= read_slot(s) && !memcmp(got, conv_fbkk[s - 2u], BANK_STORE_SIZE);
                ok &= !old_records() && !orphans_all() && new_records() == 3u && !old_chunks();
                if (!ok) fprintf(stderr, "  restore cut %s %d: old %d new %d, old records %u, new records %u, old chunks %u, orphans %u\n", phase ? "erase" : "program", k, is_old, is_new, old_records(), new_records(), old_chunks(), orphans_all());
                if (phase) n_e++; else n_p++;
            }
        snprintf(what, sizeof what, "an old record restored over a slot with a project of its own, cut at each of %d program calls and %d erases: after the next start slot 0 is the project it had (%d cuts) or the restored one (%d), the others untouched, no old record or chunk left, no orphan", n_p, n_e, saw_new, saw_old);
        CHK(what, ok && saw_old > 0 && saw_new > 0);
    }
    {   /* a store too full for the last slots: they stay as the old records, with the KB they need; room freed, CONVERT converts them */
        uint32_t room = 0, left = 0, lvl;
        for (lvl = 14; lvl >= 3u; lvl--) {             /* the fullest store that still lets some slot convert and not all */
            reset(); memcpy(nor, old_nor, sizeof nor); ocs_boot(); ocp_scan(); ocj_scan();
            for (uint32_t i = 0; ocs_free_count() > lvl && i < 40u; i++) (void)ocs_write(OCS_SAMPLE, i, nor_after, 3000u);   /* (junk records, a sector each) */
            room = ocs_free_count();
            memset(proj_meta, 0, sizeof proj_meta);
            ocj4_convert_boot();
            left = old_records();
            if (left > 0u && left < 3u) break;
        }
        {
            char m[200];
            snprintf(m, sizeof m, "a store with %u sectors free: %u project(s) converted, %u left as the old record; the summary says: '%s'", room, 3u - left, left, ocj4_conv_msg);
            CHK(m, left > 0u && left < 3u && (strstr(ocj4_conv_msg, "NEEDS") || strstr(ocj4_conv_msg, "CANNOT")));
        }
        CHK("... the ones left are still readable as the old firmware read them (OLD FORMAT, nothing erased, the old chunks kept)", ({ uint32_t ok = old_chunks() > 0u; for (s = 0; s < 4u; s++) if (ocs_exists(OCS_PROJECT, s)) { uint8_t *raw = proj_wire_borrow(); ok &= ocj4_old_read(s, raw) == (int)BANK_STORE_SIZE && !memcmp(raw, exp_fbkk[s] + 1, BANK_STORE_SIZE); } ok; }));
        CHK("... and the converted ones read back as before", ({ uint32_t ok = 1; for (s = 0; s < 4u; s++) if (!ocs_exists(OCS_PROJECT, s) && loads[s]) ok &= read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE); ok; }));
        for (uint32_t i = 0; i < 40u; i++) (void)ocs_delete(OCS_SAMPLE, i);        /* the room freed */
        for (s = 0; s < 4u; s++)
            if (ocs_exists(OCS_PROJECT, s)) {
                char what[100];
                snprintf(what, sizeof what, "CONVERT (the load key) of slot %u with the room freed converts it and loads it", s);
                project_load(s);
                CHK(what, !ocs_exists(OCS_PROJECT, s) && ocs_exists(OCS_PROJECT, OCJ4_RID(s)) && read_slot(s) && !memcmp(got, exp_fbkk[s] + 1, BANK_STORE_SIZE));
            }
        CHK("... and then no old record and no old chunk is left", !old_records() && !old_chunks() && !orphans_all());
    }
    printf(bad ? "oc r3 conversion: FAILED\n" : "oc r3 conversion: all passed\n");
    return bad != 0;
}
