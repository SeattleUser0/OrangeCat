/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2 (Part 2b): a fuzzer of the asset pack's boot check and install (firmware/src/oc_pack.c: oc_pack_boot, oc_pack_begin / write / end) on the
 * simulated NOR of tests/oc_pack_test.c.
 *   build/host/fuzz_oc_pack [seconds] [seed]      default 3 s, seed 20260613; a long run is `fuzz_oc_pack 1800`
 * Each round: a good pack installed, then 1 to 6 mutations of the pack area (a bit flip, a byte set, a header field, a sector erased or copied onto
 * another, a cut at a random length, a stale tail), then a boot. Invariants (a failure prints the seed, the round and the mutations):
 *   - no crash, no out of bounds, no undefined behaviour;
 *   - the pack is said to be good (PK_OK, the gate open) only when it is: the magic, this build's hash and length, and BOTH CRC-32 (header, payload)
 *     equal what an independent check computes; a pack that fails a CRC is never registered as good;
 *   - after the boot an install of a good pack works (any state of the area), the area holds it byte for byte, and a reboot finds it good;
 *   - the library group and the module area are untouched by the pack's install and boot.
 * Revisions: 0.12.2: first version. */
#define OC_PACK_GATE 1
#define OC_PACK_LEN 9000u              /* (three sectors of a made pack; the real one is 40,232 bytes of ten) */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"

static int bad;
static void chk(const char *what, int ok)
{
    printf("pack: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t crc32_of(const uint8_t *p, uint32_t n) { return ocs_crc32(p, n); }
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
/* a pack of OC_PACK_LEN bytes as tools/gen_pack.py lays one out: 512-byte header (0xFF between its fields), then tables */
static void make_pack(uint8_t *pak, uint32_t hash, uint32_t len)
{
    uint32_t i;
    memset(pak, 0xFF, OC_PACK_HDR);
    for (i = OC_PACK_HDR; i < len; i++) pak[i] = (uint8_t)(i * 31u + (i >> 8) * 7u);
    wr32(pak, OC_PACK_MAGIC);
    pak[4] = OC_PACK_VERSION; pak[5] = 0; pak[6] = pak[7] = 0;
    wr32(pak + 8, hash);
    wr32(pak + 12, len);
    wr32(pak + 16, crc32_of(pak + OC_PACK_HDR, len - OC_PACK_HDR));
    pak[20] = 0; pak[21] = 0; pak[22] = pak[23] = 0;
    wr32(pak + 508, crc32_of(pak, 508));
}
static void fix_header(uint8_t *pak, uint32_t len)      /* after the test changed a header field or the payload */
{
    wr32(pak + 16, crc32_of(pak + OC_PACK_HDR, len - OC_PACK_HDR));
    wr32(pak + 508, crc32_of(pak, 508));
}
static uint32_t rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint8_t PAK[OC_PACK_AREA];
/* the install of PAK: 0 the whole of it; else the call that failed (1 begin, 2 write, 3 end) */
static int install(int *rc_out)
{
    int rc = oc_pack_begin(OC_PACK_LEN);
    if (rc) { *rc_out = rc; return 1; }
    for (uint32_t off = OC_PACK_HDR; off < OC_PACK_LEN; off += 256u) {
        uint32_t n = OC_PACK_LEN - off > 256u ? 256u : OC_PACK_LEN - off;
        if ((rc = oc_pack_write(off, PAK + off, n)) != 0) { *rc_out = rc; return 2; }
    }
    if ((rc = oc_pack_end(PAK)) != 0) { *rc_out = rc; return 3; }
    *rc_out = 0;
    return 0;
}
static void fresh_flash(void)
{
    reset();
    memset(&pk, 0, sizeof pk);
    oc_pack_good = 0;
    st_retire_engines(7);                     /* (the Orange Cat build: the engines' bank objects retired by the build) */
}
static void reboot(void)                      /* the next power-on: the RAM state forgotten, the flash read */
{
    memset(&pk, 0, sizeof pk);
    oc_pack_good = 1;                         /* (it must not matter) */
    st_retire_engines(7);
    oc_pack_boot();
}

#include <time.h>
static uint32_t fz;
static uint32_t fr(void) { fz ^= fz << 13; fz ^= fz >> 17; fz ^= fz << 5; return fz; }
static char mlog[8][96];
static int nm;
static void note(const char *fmt, uint32_t a, uint32_t b) { if (nm < 8) snprintf(mlog[nm], sizeof mlog[0], fmt, a, b); nm++; }
static int indep_good(void)                                  /* what the boot check must agree with: the pack in the flash, checked here */
{
    const uint8_t *p = nor + OC_PACK_LO;
    uint32_t len;
    if (rd32(p) != OC_PACK_MAGIC || p[4] != OC_PACK_VERSION || rd32(p + 8) != OC_PACK_HASH) return 0;
    len = rd32(p + 12);
    if (len < OC_PACK_HDR || len > OC_PACK_AREA) return 0;
    if (ocs_crc32(p, 508) != rd32(p + 508)) return 0;
    return ocs_crc32(p + OC_PACK_HDR, len - OC_PACK_HDR) == rd32(p + 16);
}
int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 3.0;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 20260613u;
    uint64_t rounds = 0, muts = 0, good = 0, refused = 0, installs = 0;
    time_t t0 = time(0);
    int fails = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    fz = seed ? seed : 1u;
    make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
    for (;;) {
        int rc, step = 0, errs = 0;
        uint8_t guard_lo[4096];
        fresh_flash();
        if (install(&rc) != 0) { printf("fuzz_oc_pack: the first install failed (%d)\n", rc); return 1; }
        memset(nor + 0x20000, 0xC3, 0x1000);                 /* a marker sector outside the pack's area: must stay */
        memcpy(guard_lo, nor + 0x20000, sizeof guard_lo);
        nm = 0;
        uint32_t nmut = 1u + fr() % 6u;
        for (uint32_t i = 0; i < nmut; i++) {
            uint32_t k = fr() % 8u, sec = fr() % PK_SECTORS, t = fr() % PK_SECTORS;
            uint32_t s = OC_PACK_LO + sec * 4096u, off = (fr() & 1u) ? fr() % 512u : fr() % (OC_PACK_LEN + 4096u);
            if (off >= PK_SECTORS * 4096u) off = fr() % 4096u;
            uint8_t *q = nor + OC_PACK_LO + off;
            switch (k) {
            case 0: *q ^= (uint8_t)(1u << (fr() & 7u)); note("flip off=%u (%u)", off, 0); break;
            case 1: *q = 0x00; note("zero off=%u (%u)", off, 0); break;
            case 2: *q = 0xFF; note("ff off=%u (%u)", off, 0); break;
            case 3: *q = (uint8_t)fr(); note("random off=%u (%u)", off, 0); break;
            case 4: memset(nor + s, 0xFF, 4096); note("erase pack sector %u (%u)", sec, 0); break;
            case 5: memcpy(nor + OC_PACK_LO + t * 4096u, nor + s, 4096); note("copy pack sector %u onto %u", sec, t); break;
            case 6: memset(nor + s + (off & 4095u), 0xFF, 4096u - (off & 4095u)); note("cut sector %u at %u", sec, off & 4095u); break;
            default: wr32(nor + OC_PACK_LO + 4u * (fr() % 128u), fr()); note("a word of the header (%u) %u", 0, 0); break;
            }
        }
        muts += nmut; rounds++;
        memset(&pk, 0, sizeof pk); oc_pack_good = 1; st_retire_engines(7);
        oc_pack_boot();
        step = 1;
        int want = indep_good();
        if ((pk.state == PK_OK) != want || (oc_pack_good != 0) != want) { errs |= 1; goto fail; }
        want ? good++ : refused++;
        step = 2;
        if (memcmp(guard_lo, nor + 0x20000, sizeof guard_lo)) { errs |= 2; goto fail; }
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        if (install(&rc) != 0 || rc != 0) { errs |= 4; goto fail; }
        installs++;
        step = 3;
        if (memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN) || pk.state != PK_OK || !oc_pack_good) { errs |= 8; goto fail; }
        reboot();
        step = 4;
        if (pk.state != PK_OK || !oc_pack_good || memcmp(guard_lo, nor + 0x20000, sizeof guard_lo)) { errs |= 16; goto fail; }
        if (difftime(time(0), t0) >= secs) break;
        continue;
    fail:
        fails++;
        printf("fuzz_oc_pack: FAIL seed %u round %llu step %d errs 0x%x after %d mutations:\n", seed, (unsigned long long)rounds, step, errs, nm);
        for (int i = 0; i < nm && i < 8; i++) printf("    %s\n", mlog[i]);
        if (fails >= 3 || difftime(time(0), t0) >= secs) break;
    }
    printf("fuzz_oc_pack: seed %u, %llu rounds, %llu mutations (%llu packs still good, %llu refused), %llu installs after: %s\n", seed, (unsigned long long)rounds,
           (unsigned long long)muts, (unsigned long long)good, (unsigned long long)refused, (unsigned long long)installs, fails ? "FAIL" : "all passed");
    return fails != 0;
}
