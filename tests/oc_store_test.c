/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: host test of the record store, firmware/src/oc_store.c (build doc phase 4a, part 3):
 *   build/host/oc_store_test
 * Runs the store on a simulated NOR flash (erase sets 0xFF, programming only clears bits, a program may not cross a
 * 256-byte page), with power cut after any program call and after any programmed byte.
 * 1. an empty store holds nothing; a record of one part and one of three parts save and read back, after a reboot too.
 * 2. a newer save wins; the copy before is freed.
 * 3. power cut at every program call and every 61st byte of a three-part save: after the reboot the record reads
 *    back as the old copy or the new one, never anything else; the next save works, with or without the reboot.
 * 4. an erase that fails, and a payload rotted in the newest copy (the copy before is read, and kept by the next save).
 * 5. delete forgets every copy; a full store refuses a save and keeps everything it had.
 * 6. only the two areas are touched, sector tails stay erased, saves spread over the free sectors (wear).
 * 7. no other object of storage.c lies in the store's sectors.
 *
 * Revisions:
 *   0.3.0: first version.
 */
#define MELODEE_FLASH 1                         /* (the simulated NOR below, not the RAM sectors) */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nor[0x100000];
static int fail_after = -1;            /* power cut: stop after N program calls */
static int fail_bytes = -1;            /* power cut: stop after N programmed bytes */
static uint32_t fail_erase_off = 0xFFFFFFFFu;
static uint32_t erases[256];

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (off == fail_erase_off) return -8;
    if (off & 4095u) { printf("unaligned erase at %#x\n", off); exit(1); }
    memset(nor + off, 0xFF, 4096);
    erases[off >> 12]++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    if ((off & 0xFFu) + n > 256u) { printf("page wrap at %#x\n", off); exit(1); }
    for (uint32_t i = 0; i < n; i++) {
        if (fail_bytes == 0) return -9;
        if (fail_bytes > 0) fail_bytes--;
        nor[off + i] &= s[i];
    }
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/oc_store.c"

static int check(const char *what, int ok)
{
    printf("oc store: %-84s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static uint8_t A[12000], B[12000], C[12000], got[OCS_PARTS_MAX * OCS_PAYLOAD_MAX];

static int reads(uint32_t type, uint32_t id, const uint8_t *want, uint32_t len)
{
    int n = ocs_read(type, id, got, sizeof got);
    return n == (int)len && !memcmp(got, want, len);
}

/* a source of B's bytes that fails after 5000 */
int fail_get(ocs_src_t *s, uint8_t *d, uint32_t n)
{
    struct { ocs_src_t src; uint32_t left; } *f = (void *)s;
    if (n > f->left) return -3;
    memcpy(d, B, n);
    f->left -= n;
    return 0;
}

static void wipe(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(erases, 0, sizeof erases);
    ocs_boot();
}

/* ------------------------------------------------------ 1, 2. basics --- */
static int test_basics(void)
{
    int bad = 0;
    wipe();
    bad |= check("an empty store holds nothing, all 56 sectors free",
                 !ocs_exists(OCS_PROJECT, 0) && ocs_read(OCS_POOL, 0, got, sizeof got) < 0 && ocs_free_count() == 56u);
    bad |= check("a 3000-byte project record saves and reads back",
                 ocs_write(OCS_PROJECT, 0, A, 3000) == 0 && reads(OCS_PROJECT, 0, A, 3000));
    bad |= check("a 10000-byte pool chunk (three parts) saves and reads back",
                 ocs_write(OCS_POOL, 3, B, 10000) == 0 && reads(OCS_POOL, 3, B, 10000) && ocs_free_count() == 52u);
    ocs_boot();
    bad |= check("... both read back after a reboot",
                 reads(OCS_PROJECT, 0, A, 3000) && reads(OCS_POOL, 3, B, 10000) && ocs_free_count() == 52u);
    bad |= check("other records and types are not there",
                 !ocs_exists(OCS_PROJECT, 1) && !ocs_exists(OCS_POOL, 0) && !ocs_exists(OCS_SAMPLE, 3));
    bad |= check("a newer save wins, and the copy before is freed",
                 ocs_write(OCS_POOL, 3, C, 7600) == 0 && reads(OCS_POOL, 3, C, 7600) && ocs_free_count() == 53u);
    ocs_boot();
    bad |= check("... after a reboot too", reads(OCS_POOL, 3, C, 7600) && ocs_free_count() == 53u);
    bad |= check("an empty record and a record of exactly one full part",
                 ocs_write(OCS_SAMPLE, 9, A, 0) == 0 && ocs_read(OCS_SAMPLE, 9, got, sizeof got) == 0 &&
                     ocs_write(OCS_SAMPLE, 10, A, OCS_PAYLOAD_MAX) == 0 && reads(OCS_SAMPLE, 10, A, OCS_PAYLOAD_MAX));
    bad |= check("a record too big for twelve parts, or a bad type, is refused",
                 ocs_write(OCS_POOL, 1, got, OCS_PARTS_MAX * OCS_PAYLOAD_MAX + 1u) == -1 &&
                     ocs_write(OCS_NONE, 1, A, 10) == -1 && ocs_write(OCS_TYPES, 1, A, 10) == -1);
    bad |= check("a read into too small a buffer is refused", ocs_read(OCS_POOL, 3, got, 100) < 0);
    {   /* pieces across a part's end; a source that fails part way */
        ocs_rec_t r;
        uint8_t piece[600];
        int ok = ocs_write(OCS_POOL, 4, A, 11000) == 0 && ocs_open(OCS_POOL, 4, &r) == 0 && r.total == 11000u;
        ok &= ocs_at(&r, OCS_PAYLOAD_MAX - 300u, piece, 600) == 0 && !memcmp(piece, A + OCS_PAYLOAD_MAX - 300u, 600);
        ok &= ocs_at(&r, 2u * OCS_PAYLOAD_MAX - 1u, piece, 2) == 0 && !memcmp(piece, A + 2u * OCS_PAYLOAD_MAX - 1u, 2);
        ok &= ocs_at(&r, 10999, piece, 2) < 0 && ocs_open(OCS_POOL, 99, &r) < 0;
        bad |= check("pieces read across a part's end; past the end refused", ok);
    }
    {
        struct { ocs_src_t src; uint32_t left; } f = {{0}, 5000};
        int fail_get(ocs_src_t *s, uint8_t *d, uint32_t n);
        f.src.get = fail_get;
        bad |= check("a source that fails part way: the save fails, the copy before stays",
                     ocs_write_src(OCS_POOL, 4, &f.src, 11000) < -1 && reads(OCS_POOL, 4, A, 11000) &&
                         (ocs_boot(), reads(OCS_POOL, 4, A, 11000)));
    }
    return bad;
}

/* ---------------------------------------------------- 3. power cuts --- */
static int cut_case(int by_bytes, int k, int reboot, int *done)
{
    int rc, ok;
    wipe();
    ocs_write(OCS_PROJECT, 1, C, 2000);             /* (a neighbour that must survive) */
    ocs_write(OCS_POOL, 5, A, 10000);
    if (by_bytes) fail_bytes = k; else fail_after = k;
    rc = ocs_write(OCS_POOL, 5, B, 11000);
    fail_bytes = fail_after = -1;
    *done = rc == 0;
    if (reboot) ocs_boot();
    ok = reads(OCS_POOL, 5, A, 10000) || (reads(OCS_POOL, 5, B, 11000));
    ok &= rc != 0 || reads(OCS_POOL, 5, B, 11000);  /* reported saved: it is */
    ok &= reads(OCS_PROJECT, 1, C, 2000);
    /* the next save works */
    ok &= ocs_write(OCS_POOL, 5, C, 9000) == 0 && reads(OCS_POOL, 5, C, 9000);
    ocs_boot();
    ok &= reads(OCS_POOL, 5, C, 9000) && reads(OCS_PROJECT, 1, C, 2000);
    return ok;
}
static int test_cuts(void)
{
    int bad = 0;
    for (int reboot = 0; reboot < 2; reboot++) {
        int ok = 1, done = 0, k, n = 0;
        for (k = 0; !done && k < 400; k++, n++) ok &= cut_case(0, k, reboot, &done);
        char what[120];
        snprintf(what, sizeof what, "power cut after each of %d program calls: old or new copy, next save works (%s)",
                 n, reboot ? "rebooted" : "no reboot");
        bad |= check(what, ok && done);
        ok = 1; done = 0; n = 0;
        for (k = 0; !done && k < 20000; k += 61, n++) ok &= cut_case(1, k, reboot, &done);
        snprintf(what, sizeof what, "power cut at %d points byte by byte: old or new copy, next save works (%s)",
                 n, reboot ? "rebooted" : "no reboot");
        bad |= check(what, ok && done);
    }
    return bad;
}

/* --------------------------------------------- 4. erase errors, rot --- */
static int test_faults(void)
{
    int bad = 0, rc;
    wipe();
    ocs_write(OCS_POOL, 2, A, 9000);
    fail_erase_off = ocs_addr((ocs_cursor + 1u) % OCS_NSECT);
    rc = ocs_write(OCS_POOL, 2, B, 9000);
    fail_erase_off = 0xFFFFFFFFu;
    bad |= check("an erase that fails reports the error and keeps the copy before",
                 rc < 0 && reads(OCS_POOL, 2, A, 9000) && (ocs_boot(), reads(OCS_POOL, 2, A, 9000)));
    {
        uint8_t sect[OCS_PARTS_MAX];
        uint32_t seq;
        ocs_write(OCS_POOL, 2, B, 9000);
        ocs_current(OCS_POOL, 2, sect, &seq, 1);
        nor[ocs_addr(sect[1]) + OCS_HDR + 77] ^= 0x04;     /* rot in the newest copy's second part */
        ocs_boot();
        bad |= check("a rotted newest copy: the copy before is read", reads(OCS_POOL, 2, A, 9000));
        /* fill the free sectors' worth of saves of another record, so a careless allocator would reuse A's */
        for (int i = 0; i < 30; i++) ocs_write(OCS_SAMPLE, 1, C, 3000 + (uint32_t)i);
        bad |= check("... and other saves leave it alone", reads(OCS_POOL, 2, A, 9000));
        bad |= check("a new save over it works and reads back after a reboot",
                     ocs_write(OCS_POOL, 2, C, 8000) == 0 && reads(OCS_POOL, 2, C, 8000) &&
                         (ocs_boot(), reads(OCS_POOL, 2, C, 8000)));
    }
    {   /* a broken header */
        uint8_t sect[OCS_PARTS_MAX];
        uint32_t seq;
        ocs_write(OCS_PROJECT, 4, A, 1000);
        ocs_write(OCS_PROJECT, 4, B, 1000);
        ocs_current(OCS_PROJECT, 4, sect, &seq, 1);
        nor[ocs_addr(sect[0]) + 9] ^= 0x01;
        ocs_boot();
        bad |= check("a broken header: the copy before is read", reads(OCS_PROJECT, 4, A, 1000));
    }
    return bad;
}

/* ------------------------------------------------- 5. delete, full --- */
static int test_delete_full(void)
{
    int bad = 0;
    uint32_t id;
    wipe();
    ocs_write(OCS_PROJECT, 7, A, 1000);
    ocs_write(OCS_PROJECT, 7, B, 1000);
    ocs_write(OCS_PROJECT, 7, C, 1000);
    bad |= check("delete forgets every copy (none comes back after a reboot)",
                 ocs_delete(OCS_PROJECT, 7) == 0 && !ocs_exists(OCS_PROJECT, 7) &&
                     (ocs_boot(), !ocs_exists(OCS_PROJECT, 7)) && ocs_free_count() == 56u);
    for (id = 0; ocs_write(OCS_POOL, id, A, 3 * OCS_PAYLOAD_MAX) == 0; id++)
        ;
    {
        int ok = id == 18u;                        /* 18 x 3 sectors; 2 left, too few for another */
        for (uint32_t i = 0; i < id; i++) ok &= reads(OCS_POOL, i, A, 3 * OCS_PAYLOAD_MAX);
        bad |= check("a full store refuses the next save (-1), every record still reads back", ok);
        ok = ocs_write(OCS_POOL, 0, B, 3 * OCS_PAYLOAD_MAX) == -1;   /* replacing needs room for the new copy */
        ok &= ocs_write(OCS_SAMPLE, 0, B, 2 * OCS_PAYLOAD_MAX) == 0;  /* two sectors left fit two parts */
        ocs_boot();
        for (uint32_t i = 0; i < id; i++) ok &= reads(OCS_POOL, i, A, 3 * OCS_PAYLOAD_MAX);
        ok &= reads(OCS_SAMPLE, 0, B, 2 * OCS_PAYLOAD_MAX) && ocs_free_count() == 0u;
        bad |= check("... a save that fits the last sectors works; after a reboot all is there", ok);
    }
    return bad;
}

/* -------------------------------------------- 6. areas, tails, wear --- */
static int test_areas(void)
{
    int bad = 0, ok = 1;
    uint32_t lo = 0xFFFFFFFFu, hi = 0, used = 0;
    wipe();
    ocs_write(OCS_PROJECT, 0, A, 3000);
    ocs_write(OCS_PROJECT, 1, B, 3000);
    memset(erases, 0, sizeof erases);              /* (the projects' two sectors are not rewritten: left out below) */
    for (int i = 0; i < 540; i++) ocs_write(OCS_POOL, (uint32_t)i % 4u, C, 6000 + (uint32_t)i);
    for (uint32_t a = 0; a < sizeof nor; a++) {
        int in = (a >= 0xA0000u && a < 0xC8000u) || (a >= 0xEA000u && a < 0xFA000u);
        if (!in && nor[a] != 0xFFu) { ok = 0; break; }
    }
    for (uint32_t a = 0; a < sizeof erases / sizeof erases[0]; a++) {
        uint32_t addr = a << 12;
        int in = (addr >= 0xA0000u && addr < 0xC8000u) || (addr >= 0xEA000u && addr < 0xFA000u);
        if (!erases[a]) continue;
        if (!in) ok = 0;
    }
    bad |= check("only 0xA0000..0xC7FFF and 0xEA000..0xF9FFF are written or erased", ok);
    ok = 1;
    for (uint32_t s = 0; s < OCS_NSECT; s++)
        for (uint32_t i = OCS_SECTOR - OCS_TAIL; i < OCS_SECTOR; i++) ok &= nor[ocs_addr(s) + i] == 0xFFu;
    bad |= check("the last 256 bytes of every sector stay erased (the boot loader's scan)", ok);
    for (uint32_t s = 0; s < OCS_NSECT; s++) {
        uint32_t e = erases[ocs_addr(s) >> 12];
        if (e) { used++; if (e < lo) lo = e; if (e > hi) hi = e; }
    }
    {
        char what[120];
        snprintf(what, sizeof what, "1080 sector writes spread over the free sectors (%u used, %u..%u erases each)",
                 used, lo, hi);
        bad |= check(what, used >= 54u && hi <= lo + 2u);
    }
    ok = reads(OCS_PROJECT, 0, A, 3000) && reads(OCS_PROJECT, 1, B, 3000);
    for (uint32_t i = 0; i < 4u; i++) ok &= reads(OCS_POOL, i, C, 6000 + 536 + i);
    bad |= check("... and every record reads back its last save", ok && (ocs_boot(), reads(OCS_POOL, 3, C, 6539)));
    return bad;
}

/* ------------------------------------------- 7. storage.c's objects --- */
static int in_store(uint32_t a) { return (a >= 0xA0000u && a < 0xC8000u) || (a >= 0xEA000u && a < 0xFA000u); }
static int test_neighbours(void)
{
    int ok = 1;
    for (uint32_t o = 0; o < OBJ_COUNT; o++) {
        if (o >= OBJ_BANK0 && o < OBJ_BANK0 + 4u) continue;   /* (Melodee's bank projects: the store's now) */
        for (uint32_t c = 0; c < 2u; c++) {
            uint32_t s = st_sector(o, c);
            ok &= !in_store(s) && !in_store(s + 4095u);
            if (st_banked(o)) ok &= !in_store(st_extension(o, c)) && !in_store(st_extension(o, c) + 2u * 4096u - 1u);
        }
    }
    return check("no other flash object (settings, presets, banks) lies in the store's sectors", ok);
}

int main(void)
{
    int bad = 0;
    for (uint32_t i = 0; i < sizeof A; i++) {
        A[i] = (uint8_t)(i * 7u + 1u);
        B[i] = (uint8_t)(i * 13u + 5u);
        C[i] = (uint8_t)(i ^ (i >> 8) ^ 0x5Au);
    }
    bad |= test_basics();
    bad |= test_cuts();
    bad |= test_faults();
    bad |= test_delete_full();
    bad |= test_areas();
    bad |= test_neighbours();
    printf(bad ? "oc store: FAILED\n" : "oc store: all passed\n");
    return bad ? 1 : 0;
}
