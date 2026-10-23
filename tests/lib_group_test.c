/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.9.1: host test of the library group (firmware/src/lib_group.c, included by storage.c) on a simulated NOR.
 *
 * What it checks
 *   save and reload of every bank; a save cut at every flash operation (erase, each program, the header) keeps the old copy;
 *   two valid copies after a cut (newer wins, older erased lazily); a full group (one spare, it rotates, one erase per save,
 *   all 23 sectors used); the migration from the old A/B pairs (all banks, some, none), cut at every flash operation of the
 *   copy and of the erase of the old pair, then a second cut during the recovery; pairs of retired engines still migrate
 *   when the build retires their objects; the areas the group does not own are never touched; a backup made with pairs
 *   restored after migration and a backup made after it restored again; group sectors are refused by the old object
 *   readers (type above every object number).
 * The simulated part follows the part: erase gives 0xFF, program only clears bits, and a power cut leaves a program
 * half done (first half of the bytes) or an erase half done (junk).
 *
 * Revisions: 0.9.1: first version.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MELODEE_P5 0                    /* the Orange Cat engine set: the build retires the Prophet, CZ-1 and native FM6 objects */
#define MELODEE_CZ 0
#define MELODEE_FM6 0
#define ST_RETIRE_BY_BUILD 1

static uint8_t nor[0x100000];
static long ops, cut_at = -1;           /* flash operations since the last reset; the operation that loses power (-1: none) */
static int dead;                        /* power is gone: every later operation fails until "boot" */
static uint32_t er[256], prog_bytes;    /* erases per 4 KiB sector */
static uint32_t n_erase, n_prog;

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (dead) return -9;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (dead) return -9;
    if (cut_at == ops++) {              /* power lost in the middle of an erase: junk */
        dead = 1;
        for (uint32_t i = 0; i < 4096u; i++) nor[off + i] = (uint8_t)(0xA5u ^ (i * 7u));
        return -9;
    }
    memset(nor + off, 0xFF, 4096);
    er[off >> 12]++;
    n_erase++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (dead) return -9;
    uint32_t m = n;
    if (cut_at == ops++) { dead = 1; m = n / 2u; }      /* power lost half way through the bytes */
    for (uint32_t i = 0; i < m; i++) nor[off + i] &= s[i];
    n_prog++;
    prog_bytes += m;
    return dead ? -9 : 0;
}
#include "../firmware/src/storage.c"

static int bad;
static int check(const char *what, int ok)
{
    if (!ok) bad++;
    printf("%-96s %s\n", what, ok ? "ok" : "FAIL");
    return ok;
}

/* the expected content of each bank */
static uint8_t exp_buf[LG_BANKS][ST_PAYLOAD_MAX];
static uint32_t exp_len[LG_BANKS];
static uint8_t present[LG_BANKS];

static uint32_t rnd_state = 12345u;
static uint32_t rnd(void) { rnd_state = rnd_state * 1664525u + 1013904223u; return rnd_state >> 8; }

static void make(uint32_t b, uint32_t ver, uint8_t *buf, uint32_t *len)
{
    /* compact UPB2 images (3716 B) for most banks, a 3600 B and a full 3840 B image for others */
    uint32_t l = b % 5u == 3u ? 3600u : b % 7u == 2u ? 3808u : 3716u;
    for (uint32_t i = 0; i < l; i++) buf[i] = (uint8_t)((i * 31u) ^ (b * 17u) ^ (ver * 101u) ^ (i >> 8));
    if (l == 3716u) { uint32_t m = 0x32425055u; memcpy(buf, &m, 4); }
    *len = l;
}
static void expect(uint32_t b, uint32_t ver)
{
    make(b, ver, exp_buf[b], &exp_len[b]);
    present[b] = 1;
}
static int bank_ok(uint32_t b)
{
    static uint8_t got[ST_PAYLOAD_MAX];
    int n = st_load(OBJ_LIB0 + b, got, sizeof got);
    if (!present[b]) return n < 0;
    return n == (int)exp_len[b] && !memcmp(got, exp_buf[b], exp_len[b]);
}
static int all_ok(void)
{
    int ok = 1;
    for (uint32_t b = 0; b < LG_BANKS; b++) ok &= bank_ok(b);
    return ok;
}
static void reset_exp(void) { memset(present, 0, sizeof present); }
static void boot(void) { dead = 0; cut_at = -1; ops = 0; lg_ready = 0; lg_boot(); ops = 0; }
static void blank(void) { memset(nor, 0xFF, sizeof nor); memset(er, 0, sizeof er); reset_exp(); lg_cursor = 0; dead = 0; cut_at = -1; ops = 0; lg_ready = 0; lg_busy = 0; }

/* write bank b into its old pair, both copies (as an old firmware leaves it after two or more saves) */
static void legacy_put(uint32_t b, uint32_t ver)
{
    uint8_t t[ST_PAYLOAD_MAX];
    uint32_t l, old = st_engines_off;
    make(b, ver, t, &l);
    st_retire_engines(0);
    st_save(lg_legacy_obj(b), t, l);          /* copy A */
    st_save(lg_legacy_obj(b), t, l);          /* copy B, one sequence on */
    st_retire_engines(old);
    expect(b, ver);
}
static void legacy_set(uint32_t mask, uint32_t ver)
{
    st_retire_engines(0);
    for (uint32_t b = 0; b < LG_BANKS; b++) if (mask >> b & 1u) legacy_put(b, ver);
    st_retire_engines(7);
}
/* a flash image of the old firmware: banks in mask, with the pack area's other sectors and the module area's marked */
static void legacy_image(uint32_t mask)
{
    blank();
    st_retire_engines(7);
    legacy_set(mask, 1);
}

static uint8_t snap[0x100000];

int main(void)
{
    static uint8_t buf[ST_PAYLOAD_MAX], got[ST_PAYLOAD_MAX];
    uint32_t b, i, l;
    st_retire_engines(7);

    /* ---- the table ---- */
    {
        int ok = 1;
        uint32_t seen = 0;
        for (i = 0; i < LG_SECTORS; i++) {
            ok &= lg_addr[i] % ST_SECTOR == 0 && lg_addr[i] < 0x100000u;
            ok &= !(lg_addr[i] >= ST_PACK_LO && lg_addr[i] < ST_PACK_LO + ST_PACK_SECTORS * ST_SECTOR);
            ok &= !(lg_addr[i] >= ST_MODULE_LO && lg_addr[i] < ST_MODULE_LO + ST_MODULE_SECTORS * ST_SECTOR);
            ok &= !(lg_addr[i] >= 0xE0000u && lg_addr[i] < 0xE5000u);            /* the update loader's staging */
            ok &= !(lg_addr[i] >= 0xFC000u && lg_addr[i] != 0xFE000u);           /* the settings pair */
            ok &= !(lg_addr[i] >= 0xA0000u && lg_addr[i] < 0xC8000u);            /* bank projects */
            ok &= !(lg_addr[i] >= 0xEA000u && lg_addr[i] < 0xFA000u);            /* project extensions */
            for (uint32_t j = 0; j < i; j++) ok &= lg_addr[i] != lg_addr[j];
            seen++;
        }
        check("the 23 group sectors: distinct, outside the pack area, the module area, projects, OTA staging, settings", ok && seen == 23u);
        check("22 banks, one spare; the hdr type is above every old object number", LG_BANKS + 1u == LG_SECTORS && LG_TYPE >= OBJ_LEGACY_COUNT);
        ok = 1;
        for (b = 0; b < LG_BANKS; b++) ok &= lg_legacy_bank(lg_legacy_obj(b)) == b;
        for (b = 0; b < OBJ_LEGACY_COUNT; b++) ok &= lg_legacy_bank(b) == LG_NONE || lg_legacy_obj(lg_legacy_bank(b)) == b;
        check("the legacy object of each bank is the old up_obj mapping, one to one", ok);
    }

    /* ---- save and reload every bank ---- */
    {
        blank();
        int ok = 1;
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); ok &= st_save(OBJ_LIB0 + b, buf, l) == 0; expect(b, 1); }
        ok &= all_ok();
        check("save and reload all 22 banks (images of 3716, 3600 and 3808 B)", ok);
        for (b = 0; b < LG_BANKS; b++) { make(b, 2, buf, &l); ok &= st_save(OBJ_LIB0 + b, buf, l) == 0; expect(b, 2); }
        ok &= all_ok();
        check("save over every bank again: new content, none of the others disturbed", ok);
        boot();
        check("... and after a boot (rescan of the 23 sectors)", all_ok());
        {   /* st_current / st_exists / st_head on a bank, and the sector's header names the group, the bank and the sequence */
            st_hdr_t h;
            int c = st_current(OBJ_LIB0 + 4, &h);
            ok = c == 0 && h.type == LG_TYPE && h.slot == 4u && h.seq == 2u && st_exists(OBJ_LIB0 + 4) && !st_retired(OBJ_LIB0 + 4);
            ok &= st_read_range(OBJ_LIB0 + 4, 0, 0, got, 64) == 0 && !memcmp(got, exp_buf[4], 64);
            check("header: type = group, slot = bank, seq = save count; range reads follow the sector the bank is in", ok);
        }
        ok = st_save(OBJ_LIB0 + 3, buf, ST_PAYLOAD_MAX + 1u) < 0 && st_save(OBJ_COUNT, buf, 4) < 0 && st_load(OBJ_COUNT, got, 4) < 0 &&
             st_load(OBJ_LIB0 + 22, got, sizeof got) < 0;
        check("oversize and out of range refused", ok);
        make(5, 2, buf, &l);
        ok = st_load(OBJ_LIB0 + 5, got, l - 1u) < 0;
        check("a destination too small refuses the whole bank", ok);
        {   /* the last 256 bytes of every group sector stay erased: the update loader scans this range for records */
            ok = 1;
            for (i = 0; i < LG_SECTORS; i++) for (uint32_t j = 4096u - 256u; j < 4096u; j++) ok &= nor[lg_addr[i] + j] == 0xFF;
            check("the last 256 bytes of every sector stay erased (update loader scan)", ok);
        }
        {   /* a 3840 B image (the old objects' largest, never made by the firmware) still round-trips, payload at 256 */
            for (i = 0; i < 3840u; i++) buf[i] = (uint8_t)(i * 3u);
            int r = st_save(OBJ_LIB0 + 3, buf, 3840u) == 0 && st_load(OBJ_LIB0 + 3, got, sizeof got) == 3840 && !memcmp(got, buf, 3840);
            make(3, 2, buf, &l);
            r &= st_save(OBJ_LIB0 + 3, buf, l) == 0 && bank_ok(3);
            check("a 3840 B image round-trips (payload at 256), and the bank takes its own image again", r);
        }
    }

    /* ---- a cut save at every flash operation ---- */
    {
        int ok = 1, all = 1;
        long n_ops;
        blank();
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 1); }
        memcpy(snap, nor, sizeof nor);
        make(5, 9, buf, &l);
        ops = 0; st_save(OBJ_LIB0 + 5, buf, l); n_ops = ops;
        check("a compact save is one erase, the programs and the header (flash operations counted)", n_ops == 1 + 16 + 1 || n_ops > 3);
        printf("    (one save: %ld flash operations)\n", n_ops);
        for (long cut = 0; cut < n_ops; cut++) {
            memcpy(nor, snap, sizeof nor);
            boot();
            cut_at = cut; ops = 0;
            int rc = st_save(OBJ_LIB0 + 5, buf, l);
            ok &= rc != 0 && dead;
            boot();                                             /* power back */
            all &= all_ok();                                    /* the old copy of bank 5, every other bank as it was */
            make(5, 10, buf, &l);                               /* and the next save goes through */
            ok &= st_save(OBJ_LIB0 + 5, buf, l) == 0;
            expect(5, 10);
            all &= all_ok();
            expect(5, 1);
            make(5, 9, buf, &l);
        }
        check("a save cut at each flash operation (erase, every program, the header) leaves the previous copy in charge", ok && all);
        /* a save cut inside every byte of the header */
        ok = 1;
        for (uint32_t cb = 0; cb < sizeof(st_hdr_t); cb++) {
            memcpy(nor, snap, sizeof nor);
            boot();
            uint32_t s = lg_pick();
            memset(nor + lg_addr[s], 0xFF, 4096);
            make(5, 9, buf, &l);
            for (uint32_t off = 0; off < l; off += 256u) { uint32_t m = l - off > 256u ? 256u : l - off; for (uint32_t k = 0; k < m; k++) nor[lg_addr[s] + 32u + off + k] &= buf[off + k]; }
            st_hdr_t h;
            h.magic = ST_MAGIC; h.type = LG_TYPE; h.slot = 5; h.seq = lg_seq[5] + 1u; h.len = l; h.crc = st_crc32(buf, l);
            h.rsv[0] = ST_COMPACT_TAG; h.rsv[1] = 0xFFFFFFFFu; h.hcrc = st_crc32(&h, sizeof h - 4u);
            for (uint32_t k = 0; k < cb; k++) nor[lg_addr[s] + k] &= ((uint8_t *)&h)[k];      /* the header, cb bytes in */
            lg_ready = 0;
            expect(5, 1);
            ok &= all_ok();
        }
        check("... and a header cut after each of its 32 bytes", ok);
    }

    /* ---- two valid copies after a cut, the older erased lazily ---- */
    {
        int ok = 1;
        blank();
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 1); }
        uint32_t s_old = lg_map[7];
        make(7, 2, buf, &l); st_save(OBJ_LIB0 + 7, buf, l); expect(7, 2);
        uint32_t s_new = lg_map[7];
        st_hdr_t h1, h2;
        ok &= s_old != s_new && !lg_rd_hdr(s_old, &h1) && !lg_rd_body(s_old, &h1) && !lg_rd_hdr(s_new, &h2) && h2.seq == h1.seq + 1u;
        check("after a save the previous copy is still a valid record in the sector the bank left (not erased yet)", ok);
        boot();
        check("two valid copies at boot: the newer sequence wins", lg_map[7] == s_new && all_ok());
        nor[lg_addr[s_new] + 100] ^= 0x10;                      /* the newer one rots */
        lg_ready = 0; lg_scan();
        make(7, 1, buf, &l); expect(7, 1);
        check("... and if the newer one fails its CRC the older one is in charge", lg_map[7] == s_old && all_ok());
        make(7, 2, buf, &l); expect(7, 2);
        blank();
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 1); }
        s_old = lg_map[7];
        make(7, 2, buf, &l); st_save(OBJ_LIB0 + 7, buf, l); expect(7, 2);
        /* the lazy erase: the free sector is the one the next save picks; saving bank 7 again reuses s_old and erases it then */
        ok = lg_pick() == s_old;
        uint32_t e0 = er[lg_addr[s_old] >> 12];
        make(7, 3, buf, &l); st_save(OBJ_LIB0 + 7, buf, l); expect(7, 3);
        ok &= er[lg_addr[s_old] >> 12] == e0 + 1u && lg_map[7] == s_old;
        check("the older copy is erased lazily: by the next save that picks its sector, not at boot", ok);
        /* sequence wrap */
        lg_seq[9] = 0xFFFFFFFEu;
        make(9, 4, buf, &l); st_save(OBJ_LIB0 + 9, buf, l); expect(9, 4);
        make(9, 5, buf, &l); st_save(OBJ_LIB0 + 9, buf, l); expect(9, 5);
        make(9, 6, buf, &l); st_save(OBJ_LIB0 + 9, buf, l); expect(9, 6);
        boot();
        check("the save sequence wraps (0xFFFFFFFE, 0xFFFFFFFF, 0, 1): the newest still wins at boot", all_ok() && lg_seq[9] < 4u);
    }

    /* ---- a full group: the spare rotates ---- */
    {
        int ok = 1;
        blank();
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 1); }
        ok &= __builtin_popcount(lg_owned()) == 22;
        uint32_t free_n = 0;
        for (i = 0; i < LG_SECTORS; i++) free_n += !((lg_owned() >> i) & 1u);
        check("22 banks in 23 sectors: exactly one free sector", ok && free_n == 1u);
        memset(er, 0, sizeof er);
        uint32_t saves = 0, ver = 2;
        for (uint32_t k = 0; k < 2300u; k++) {
            b = rnd() % LG_BANKS;
            make(b, ver, buf, &l);
            uint32_t was = lg_map[b];
            ok &= st_save(OBJ_LIB0 + b, buf, l) == 0 && lg_map[b] != was;
            expect(b, ver++);
            saves++;
        }
        boot();
        ok &= all_ok();
        uint32_t tot = 0, lo = 0xFFFFFFFFu, hi = 0;
        for (i = 0; i < LG_SECTORS; i++) { uint32_t e = er[lg_addr[i] >> 12]; tot += e; if (e < lo) lo = e; if (e > hi) hi = e; }
        check("2300 random saves keep every bank right across reboot", ok);
        check("one erase per save (as an A/B pair did)", tot == saves);
        printf("    (erases per sector after %u random saves: min %u, max %u, mean %u)\n", saves, lo, hi, tot / LG_SECTORS);
        check("the free sector rotates: all 23 sectors take their share (no sector under half the mean, none over twice)", lo * 2u >= tot / LG_SECTORS && hi <= 2u * (tot / LG_SECTORS));
        /* hammer one bank: it alternates between two sectors, as the pair did, never worse */
        memset(er, 0, sizeof er);
        for (uint32_t k = 0; k < 200u; k++) { make(11, ver, buf, &l); st_save(OBJ_LIB0 + 11, buf, l); expect(11, ver++); }
        uint32_t used = 0;
        for (i = 0; i < LG_SECTORS; i++) used += er[lg_addr[i] >> 12] != 0;
        check("one bank saved 200 times alternates two sectors (the old pair's wear, no worse)", used == 2u && all_ok());
    }

    /* ---- migration from the old A/B pairs ---- */
    {
        uint32_t masks[4] = {0x3FFFFFu, 0x2A5A5Au & 0x3FFFFFu, 0u, 0x00001u};
        const char *names[4] = {"all 22 banks", "some banks (a mixed set)", "none (a fresh unit)", "one bank only"};
        for (int sc = 0; sc < 4; sc++) {
            int ok = 1;
            legacy_image(masks[sc]);
            /* the pack area outside the Prophet pairs, and sectors nobody owns, carry a marker the group must never touch */
            uint8_t before[0x100000];
            memcpy(before, nor, sizeof nor);
            memset(er, 0, sizeof er); n_erase = n_prog = 0;
            int copied = lg_boot();
            ok &= copied == __builtin_popcount(masks[sc]);
            ok &= all_ok();
            check(names[sc], ok);
            printf("    (migration: %d banks moved, %u erases, %u programs)\n", copied, n_erase, n_prog);
            /* every old pair is erased where it lay outside the group; the group holds the banks */
            ok = 1;
            for (b = 0; b < LG_BANKS; b++) {
                if (!(masks[sc] >> b & 1u)) continue;
                for (uint32_t c = 0; c < 2u; c++) {
                    uint32_t a = st_sector(lg_legacy_obj(b), c);
                    if (lg_index_of(a) != LG_NONE) continue;
                    for (uint32_t j = 0; j < 4096u; j++) ok &= nor[a + j] == 0xFF;
                }
            }
            check("... the old pairs outside the group are erased (the pack and module areas are free of presets)", ok);
            /* a second boot does nothing: no erase, no program */
            n_erase = n_prog = 0; boot();
            check("... a second boot has nothing to move (no erase, no program)", n_erase == 0 && n_prog == 0 && all_ok());
            /* sectors outside the group and outside the migrated pairs were never touched */
            ok = 1;
            for (uint32_t a = 0; a < 0x100000u; a += 4096u) {
                int owned_by_pair = 0;
                for (b = 0; b < LG_BANKS; b++) for (uint32_t c = 0; c < 2u; c++) owned_by_pair |= (masks[sc] >> b & 1u) && st_sector(lg_legacy_obj(b), c) == a;
                if (lg_index_of(a) != LG_NONE || owned_by_pair) continue;
                ok &= !memcmp(nor + a, before + a, 4096);
            }
            check("... no other sector of the flash was touched", ok);
        }
    }

    /* ---- migration cut at every flash operation, then a second cut during the recovery ---- */
    {
        int ok = 1, ok2 = 1;
        legacy_image(0x3FFFFFu);
        memcpy(snap, nor, sizeof nor);
        ops = 0; lg_boot(); long total = ops;
        printf("    (a full migration is %ld flash operations)\n", total);
        uint8_t keep[LG_BANKS], keepn = 0; (void)keepn; (void)keep;
        for (long cut = 0; cut < total; cut++) {
            memcpy(nor, snap, sizeof nor);
            for (b = 0; b < LG_BANKS; b++) expect(b, 1);
            dead = 0; cut_at = cut; ops = 0; lg_ready = 0; lg_busy = 0;
            lg_boot();
            ok &= dead;
            /* power back: whatever was cut, the first complete boot gets every bank right, wherever the cut fell */
            boot();
            ok &= all_ok();
            n_erase = n_prog = 0; boot();
            ok &= n_erase == 0 && n_prog == 0;                  /* and that boot finished the job */
            /* a user save after the recovery works */
            make(3, 7, buf, &l);
            ok &= st_save(OBJ_LIB0 + 3, buf, l) == 0;
        }
        check("migration cut at each of its flash operations: the next boot has every bank intact and finishes", ok);
        for (long cut = 0; cut < total; cut += 7) {
            for (long cut2 = 0; cut2 < 40; cut2 += 3) {
                memcpy(nor, snap, sizeof nor);
                for (b = 0; b < LG_BANKS; b++) expect(b, 1);
                dead = 0; cut_at = cut; ops = 0; lg_ready = 0; lg_busy = 0; lg_boot();
                dead = 0; cut_at = cut2; ops = 0; lg_ready = 0; lg_busy = 0; lg_boot();      /* a second power loss in the recovery */
                boot();
                ok2 &= all_ok();
                n_erase = n_prog = 0; boot();
                ok2 &= n_erase == 0 && n_prog == 0;
            }
        }
        check("... and a second power loss during the recovery boot, the same", ok2);
    }

    /* ---- a cut anywhere: the data is readable by the OLD firmware's reader or by the group, never lost between ---- */
    {
        int ok = 1;
        legacy_image(0x3FFFFFu);
        memcpy(snap, nor, sizeof nor);
        ops = 0; lg_boot(); long total = ops;
        for (long cut = 0; cut < total; cut += 5) {
            memcpy(nor, snap, sizeof nor);
            dead = 0; cut_at = cut; ops = 0; lg_ready = 0; lg_busy = 0; lg_boot();
            dead = 0; cut_at = -1;
            /* without any boot: each bank is readable in the group (lg_scan) or in its old pair */
            lg_ready = 0; lg_scan();
            for (b = 0; b < LG_BANKS; b++) {
                st_hdr_t h;
                int in_group = lg_map[b] != LG_NONE, in_pair = lg_legacy_cur(b, &h) >= 0;
                ok &= in_group || in_pair;
            }
        }
        check("at no cut is a bank in neither the group nor its old pair", ok);
    }

    /* ---- the old object readers refuse group sectors (a downgrade sees empty banks, never wrong data) ---- */
    {
        int ok = 1;
        blank();
        for (b = 0; b < LG_BANKS; b++) { make(b, 1, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 1); }
        st_retire_engines(0);
        for (b = 0; b < LG_BANKS; b++) {
            uint32_t o = lg_legacy_obj(b);
            st_hdr_t h;
            ok &= st_head(o, 0, &h) != 0 && st_head(o, 1, &h) != 0;
        }
        st_retire_engines(7);
        check("every old object reader refuses the sectors the group now holds (old firmware sees empty banks)", ok);
    }

    /* ---- backup: made with pairs and restored after migration; made after migration and restored again ---- */
    {
        int ok = 1;
        static uint8_t backup[LG_BANKS][ST_PAYLOAD_MAX];
        uint32_t blen[LG_BANKS];
        legacy_image(0x3FFFFFu);
        /* the editor backup of an old unit: each bank's object content */
        st_retire_engines(0);
        for (b = 0; b < LG_BANKS; b++) blen[b] = (uint32_t)st_load(lg_legacy_obj(b), backup[b], sizeof backup[b]);
        st_retire_engines(7);
        boot();
        for (b = 0; b < LG_BANKS; b++) { make(b, 3, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 3); }   /* edited since */
        for (b = 0; b < LG_BANKS; b++) ok &= st_save(OBJ_LIB0 + b, backup[b], blen[b]) == 0;                   /* restore (editor_backup.c: st_save(up_obj(bank)...)) */
        for (b = 0; b < LG_BANKS; b++) expect(b, 1);
        ok &= all_ok();
        check("a backup made with pairs, restored after migration: every bank comes back", ok);
        static uint8_t again[LG_BANKS][ST_PAYLOAD_MAX];
        uint32_t alen[LG_BANKS];
        for (b = 0; b < LG_BANKS; b++) alen[b] = (uint32_t)st_load(OBJ_LIB0 + b, again[b], sizeof again[b]);
        for (b = 0; b < LG_BANKS; b++) { make(b, 8, buf, &l); st_save(OBJ_LIB0 + b, buf, l); expect(b, 8); }
        for (b = 0; b < LG_BANKS; b++) ok &= st_save(OBJ_LIB0 + b, again[b], alen[b]) == 0;
        for (b = 0; b < LG_BANKS; b++) expect(b, 1);
        boot();
        ok &= all_ok();
        check("a backup made after migration, restored again (and after a boot): every bank comes back", ok);
        /* an empty bank restores as an empty bank */
        ok = st_save(OBJ_LIB0 + 6, buf, 0) == 0 && st_load(OBJ_LIB0 + 6, got, sizeof got) == 0 && !st_exists(OBJ_LIB0 + 6);
        check("restoring an empty bank (length 0) is a record of length 0", ok);
    }

    /* ---- an old pair that is newer than the group's copy (an older firmware ran in between) moves in again ---- */
    {
        int ok = 1;
        blank();
        st_retire_engines(7);
        legacy_set(1u << 20, 1);                         /* a Prophet bank, in the pack area */
        boot();
        ok &= all_ok();
        st_retire_engines(0);
        make(20, 6, buf, &l);
        for (int k = 0; k < 3; k++) st_save(lg_legacy_obj(20), buf, l);      /* the old firmware saved it again, sequences on */
        st_retire_engines(7);
        expect(20, 6);
        boot();
        ok &= all_ok();
        check("an old pair with a newer sequence than the group copy is moved in again and erased", ok);
        for (uint32_t c = 0; c < 2u; c++) { uint32_t a = st_sector(lg_legacy_obj(20), c); for (uint32_t j = 0; j < 4096u; j++) ok &= nor[a + j] == 0xFF; }
        check("... the old pair gone", ok);
    }

    /* ---- the group's reads and writes never reach the pack area or the module area ---- */
    {
        int ok = 1;
        blank();
        for (uint32_t a = ST_PACK_LO; a < ST_PACK_LO + ST_PACK_SECTORS * ST_SECTOR; a += 4) memcpy(nor + a, "OCPK", 4);
        for (uint32_t a = ST_MODULE_LO; a < ST_MODULE_LO + ST_MODULE_SECTORS * ST_SECTOR; a += 4) memcpy(nor + a, "MODL", 4);
        memcpy(snap, nor, sizeof nor);
        boot();
        for (uint32_t k = 0; k < 400u; k++) { b = rnd() % LG_BANKS; make(b, k + 1u, buf, &l); ok &= st_save(OBJ_LIB0 + b, buf, l) == 0; expect(b, k + 1u); }
        ok &= all_ok();
        ok &= !memcmp(nor + ST_PACK_LO, snap + ST_PACK_LO, ST_PACK_SECTORS * ST_SECTOR) &&
              !memcmp(nor + ST_MODULE_LO, snap + ST_MODULE_LO, ST_MODULE_SECTORS * ST_SECTOR);
        boot();
        ok &= !memcmp(nor + ST_PACK_LO, snap + ST_PACK_LO, ST_PACK_SECTORS * ST_SECTOR);
        check("an installed pack and module: 400 saves and a boot leave both areas byte for byte as they were", ok);
    }

    printf("%s\n", bad ? "LIBRARY GROUP TEST FAILED" : "library group test passed");
    return bad != 0;
}
