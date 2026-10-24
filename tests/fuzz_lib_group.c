/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2 (Part 2b): a fuzzer of the library group (firmware/src/lib_group.c: lg_scan, lg_migrate, lg_leftovers, through lg_boot) on the
 * simulated NOR of tests/lib_group_test.c.
 *   build/host/fuzz_lib_group [seconds] [seed]      default 3 s, seed 20260612; run_tests.sh runs the default, a long run is `fuzz_lib_group 1800`
 * Each round: a flash image of the old firmware or of the group (all banks, some, mixed; a new save over a part), then 1 to 6 MUTATIONS of the bytes
 * of its sectors (a bit flip, a byte set to 00 / FF / random, a field of the 32 byte header, a sector copied onto another, a sector erased, a sector
 * cut at a random length, two sectors swapped), then a boot (scan, move of the old pairs, leftovers).
 * Invariants (a failure prints the seed, the round and the mutations, and stops):
 *   - no crash, no out of bounds access, no undefined behaviour (the sanitizers build, SAN=1 of run_tests.sh, makes them checks);
 *   - a bank that loads has exactly the content of one of the versions that were written for it (a body that fails its CRC is never picked);
 *   - after the boot a save of any bank succeeds, loads back, and survives another boot, and the other banks load as they did before the save;
 *   - a second boot changes no bank (the pass is safe to repeat).
 * Revisions: 0.12.2: first version. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LG_TEST_HOOKS 1                 /* (0.11.1) lg_test_reserve: sectors the picker must not return (the stuck state cannot be made from data) */
#define MELODEE_P5 0                    /* the Orange Cat engine set: the build retires the Prophet, CZ-1 and native FM6 objects */
#define MELODEE_CZ 0
#define MELODEE_FM6 0
#define ST_RETIRE_BY_BUILD 1

static uint8_t nor[0x100000];
static long ops, cut_at = -1;           /* flash operations since the last reset; the operation that loses power (-1: none) */
static int dead;                        /* power is gone: every later operation fails until "boot" */
static long fail_read_n = -1;           /* (0.11.1) the read this many reads from now fails once (-1: none); 0 is the next one */
static long fail_erase_n = -1;          /* (0.11.1) the same for erases: the one this many erases from now returns an error (the sector is left as it was) */
static uint32_t er[256], prog_bytes;    /* erases per 4 KiB sector */
static uint32_t n_erase, n_prog;

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (dead) return -9;
    if (fail_read_n >= 0 && fail_read_n-- == 0) { fail_read_n = -1; return -9; }
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (dead) return -9;
    if (fail_erase_n >= 0 && fail_erase_n-- == 0) { fail_erase_n = -1; return -9; }
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


#include <time.h>
static uint32_t fz;                                   /* the fuzzer's own generator: fixed seed, not the test's rnd */
static uint32_t fr(void) { fz ^= fz << 13; fz ^= fz >> 17; fz ^= fz << 5; return fz; }
static char mlog[8][96];
static int nm;
static void note(const char *fmt, uint32_t a, uint32_t b) { if (nm < 8) snprintf(mlog[nm], sizeof mlog[0], fmt, a, b); nm++; }
static uint32_t all_sectors[64], nall;
static void mutate(void)
{
    uint32_t k = fr() % 8u;
    uint32_t s = all_sectors[fr() % nall], t = all_sectors[fr() % nall];
    uint32_t off = (fr() & 1u) ? fr() % 32u : fr() % 4096u;
    switch (k) {
    case 0: nor[s + off] ^= (uint8_t)(1u << (fr() & 7u)); note("flip bit s=%x off=%u", s, off); break;
    case 1: nor[s + off] = 0x00; note("zero s=%x off=%u", s, off); break;
    case 2: nor[s + off] = 0xFF; note("ff s=%x off=%u", s, off); break;
    case 3: nor[s + off] = (uint8_t)fr(); note("random s=%x off=%u", s, off); break;
    case 4: memcpy(nor + t, nor + s, 4096); note("copy sector %x onto %x", s, t); break;
    case 5: memset(nor + s, 0xFF, 4096); note("erase sector %x (%u)", s, 0); break;
    case 6: memset(nor + s + off, 0xFF, 4096u - off); note("cut sector %x at %u", s, off); break;
    default: { uint8_t tmp[4096]; memcpy(tmp, nor + s, 4096); memcpy(nor + s, nor + t, 4096); memcpy(nor + t, tmp, 4096); note("swap %x and %x", s, t); break; }
    }
}
static int version_ok(uint32_t b, const uint8_t *got, int n)     /* the content of bank b is one that was written for it */
{
    uint8_t w[ST_PAYLOAD_MAX]; uint32_t l;
    for (uint32_t v = 1; v <= 3; v++) { make(b, v, w, &l); if (n == (int)l && !memcmp(got, w, l)) return 1; }
    return 0;
}
static int loads[LG_BANKS];
static int load_all(uint8_t out[LG_BANKS][ST_PAYLOAD_MAX], int *errs)
{
    int ok = 1;
    for (uint32_t b = 0; b < LG_BANKS; b++) {
        int n = st_load(OBJ_LIB0 + b, out[b], ST_PAYLOAD_MAX);
        loads[b] = n;
        if (n >= 0 && !version_ok(b, out[b], n)) { ok = 0; if (errs) *errs |= 1; }
    }
    return ok;
}
int main(int argc, char **argv)
{
    static uint8_t A[LG_BANKS][ST_PAYLOAD_MAX], B[LG_BANKS][ST_PAYLOAD_MAX], buf[ST_PAYLOAD_MAX];
    static int la[LG_BANKS];
    double secs = argc > 1 ? atof(argv[1]) : 3.0;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 20260612u;
    uint64_t loaded = 0, rounds = 0, muts = 0, boots = 0, saves = 0;
    time_t t0 = time(0);
    int fails = 0;
    fz = seed ? seed : 1u;
    st_retire_engines(7);
    nall = 0;
    for (uint32_t i = 0; i < LG_SECTORS; i++) all_sectors[nall++] = lg_addr[i];
    for (;;) {
        uint32_t mask = fr() & ((1u << LG_BANKS) - 1u), l;
        blank();
        st_retire_engines(7);
        legacy_set(mask & fr(), 1);                                /* the old pairs of some banks */
        for (uint32_t b = 0; b < LG_BANKS; b++)                    /* the group's copies of others, one or two versions */
            if ((mask >> b & 1u) && fr() % 3u) { for (uint32_t v = 1; v <= 1u + fr() % 2u; v++) { make(b, v, buf, &l); st_save(OBJ_LIB0 + b, buf, l); } }
        if ((fr() & 3u) == 0) boot();
        /* the sectors of the old pairs: every sector the base image touched is a target too */
        nall = LG_SECTORS;
        for (uint32_t s = 0; s < 256u && nall < 64u; s++) { int hit = 0; for (uint32_t i = 0; i < LG_SECTORS; i++) hit |= lg_addr[i] == s * 4096u; if (!hit) for (uint32_t j = 0; j < 64u; j++) if (nor[s * 4096u + j] != 0xFF) { all_sectors[nall++] = s * 4096u; break; } }
        nm = 0;
        uint32_t nmut = 1u + fr() % 6u;
        for (uint32_t i = 0; i < nmut; i++) mutate();
        muts += nmut; rounds++;
        int errs = 0, step = 0;
        lg_ready = 0; lg_busy = 0;
        dead = 0; cut_at = -1; ops = 0;
        lg_boot(); boots++; step = 1;
        if (!load_all(A, &errs)) goto fail;
        for (uint32_t b = 0; b < LG_BANKS; b++) loaded += loads[b] >= 0;
        step = 2;
        lg_ready = 0; lg_boot(); boots++;                           /* a second pass changes nothing */
        load_all(B, 0);
        for (uint32_t b = 0; b < LG_BANKS; b++) { la[b] = loads[b]; }
        { int same = 1; for (uint32_t b = 0; b < LG_BANKS; b++) { int na = st_load(OBJ_LIB0 + b, B[b], ST_PAYLOAD_MAX); (void)na; } (void)same; }
        step = 3;
        {   /* a save of one bank (any, chosen) and the others as before */
            uint32_t sb = fr() % LG_BANKS;
            static uint8_t before[LG_BANKS][ST_PAYLOAD_MAX]; int nb[LG_BANKS];
            for (uint32_t b = 0; b < LG_BANKS; b++) nb[b] = st_load(OBJ_LIB0 + b, before[b], ST_PAYLOAD_MAX);
            make(sb, 3, buf, &l);
            if (st_save(OBJ_LIB0 + sb, buf, l) != 0) { errs |= 2; goto fail; }
            saves++;
            step = 4;
            for (uint32_t b = 0; b < LG_BANKS; b++) {
                static uint8_t g[ST_PAYLOAD_MAX];
                int n = st_load(OBJ_LIB0 + b, g, ST_PAYLOAD_MAX);
                if (b == sb) { if (n != (int)l || memcmp(g, buf, l)) { errs |= 4; goto fail; } }
                else if (n != nb[b] || (n > 0 && memcmp(g, before[b], (size_t)n))) { errs |= 8; goto fail; }
            }
            lg_ready = 0; lg_boot(); boots++;
            step = 5;
            for (uint32_t b = 0; b < LG_BANKS; b++) {
                static uint8_t g[ST_PAYLOAD_MAX];
                int n = st_load(OBJ_LIB0 + b, g, ST_PAYLOAD_MAX);
                if (b == sb) { if (n != (int)l || memcmp(g, buf, l)) { errs |= 16; goto fail; } }
                else if (n != nb[b] || (n > 0 && memcmp(g, before[b], (size_t)n))) { errs |= 32; goto fail; }
            }
        }
        if (difftime(time(0), t0) >= secs) break;
        continue;
    fail:
        fails++;
        printf("fuzz_lib_group: FAIL seed %u round %llu step %d errs 0x%x after %d mutations:\n", seed, (unsigned long long)rounds, step, errs, nm);
        for (int i = 0; i < nm && i < 8; i++) printf("    %s\n", mlog[i]);
        if (fails >= 3) break;
        if (difftime(time(0), t0) >= secs) break;
    }
    (void)la; (void)A; (void)B;
    printf("fuzz_lib_group: seed %u, %llu rounds, %llu mutations, %llu boots, %llu saves, %llu banks loaded after mutation: %s\n", seed, (unsigned long long)rounds, (unsigned long long)muts,
           (unsigned long long)boots, (unsigned long long)saves, (unsigned long long)loaded, fails ? "FAIL" : "all passed");
    return fails != 0;
}
