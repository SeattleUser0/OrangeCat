/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.0: host test of the internal user sample slot (firmware/src/smp_slot.c) on a simulated NOR flash
 * (erase sets 0xFF, programming only clears bits, no program across a 256-byte page, power cut after any program call):
 *   build/host/smp_slot_test
 * 1. the claim: a store with records in the slot's sectors moves them, every one reads back after a reboot; the slot's
 *    22 sectors are the slot's and the store never hands them out again; a boot with nothing to move writes nothing (a full store refuses, the slot is untouched);
 *    a store with no room for the moves keeps everything and has no slot (UNCLAIMED), until it has room.
 * 2. a power cut at every program call of the claim: after the reboot every record reads back, and the claim ends.
 * 3. a sample of the largest size (81,408 bytes) and of 1 byte: written, read back at every offset, after a reboot too;
 *    sizes 0 and 81,409 refused; writes out of order, past the end, an END that does not match (length, CRC, magic)
 *    refused and leaving the slot empty.
 * 4. a power cut at every program call of a write (and of an erase): after the reboot the slot is empty or holds the
 *    whole new sample, never a part of one; the record store reads as before.
 * 5. what must never touch it: saves, deletes and a full store's refusals (the store's sectors only); the sector tails
 *    stay erased (the update loader's scan); nothing written outside 0xB2000..0xC7FFF by the slot.
 */
#define MELODEE_FLASH 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t nor[0x100000], nor_before[0x100000];
static int fail_after = -1;
static uint32_t prog_lo = 0xFFFFFFFFu, prog_hi;

static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if (fail_after == 0) return -9;
    if (off & 4095u) { printf("unaligned erase at %#x\n", off); exit(1); }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    if ((off & 0xFFu) + n > 256u) { printf("page wrap at %#x\n", off); exit(1); }
    if (off < prog_lo) prog_lo = off;
    if (off + n > prog_hi) prog_hi = off + n;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"
#include "../firmware/src/oc_store.c"
#include "../firmware/src/smp_slot.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("sample slot: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint8_t rec[OCS_NSECT][20000], got[OCS_PARTS_MAX * OCS_PAYLOAD_MAX];
static uint8_t smp_data[SMP_SLOT_MAX + 1u], smp_got[SMP_SLOT_MAX];
static uint8_t hdr[512];

static void wipe(void) { memset(nor, 0xFF, sizeof nor); }
static void reboot(void) { smp_slot_boot(); }

/* records as 0.5.6 left them: n records of up to two parts, saved with the whole store (no slot yet) */
static void fill_records(uint32_t n, uint32_t len)
{
    ocs_resv = 0;
    ocs_boot();
    for (uint32_t k = 0; k < n; k++) {
        for (uint32_t i = 0; i < len; i++) rec[k][i] = (uint8_t)(k * 31u + i * 7u + (i >> 8));
        if (ocs_write(OCS_POOL, k, rec[k], len)) { printf("fill %u failed\n", k); exit(1); }
    }
}
static int records_ok(uint32_t n, uint32_t len)
{
    for (uint32_t k = 0; k < n; k++)
        if (ocs_read(OCS_POOL, k, got, sizeof got) != (int)len || memcmp(got, rec[k], len)) return 0;
    return 1;
}
static int in_slot(uint32_t s) { return s >= SMP_SLOT_S0 && s < SMP_SLOT_S0 + SMP_SLOT_NSECT; }
static int tails_erased(void)
{
    for (uint32_t s = 0; s < OCS_NSECT; s++)
        for (uint32_t i = 4096u - 256u; i < 4096u; i++)
            if (nor[ocs_addr(s) + i] != 0xFFu) return 0;
    return 1;
}

static void make_sample(uint32_t len, uint32_t seed)
{
    for (uint32_t i = 0; i < len; i++) smp_data[i] = (uint8_t)(seed * 13u + i * 17u + (i >> 9));
}
static void make_hdr(uint32_t len)
{
    smp_hdr_t h;
    memset(hdr, 0xFF, sizeof hdr);
    memset(&h, 0, sizeof h);
    h.magic = SMP_FSMP_MAGIC;
    h.version = 1;
    h.nz = 1;
    memcpy(h.name, "TESTSMP", 7);
    h.data_len = len;
    h.crc = ocs_crc32(smp_data, len);
    memcpy(hdr, &h, sizeof h);
}
static int write_sample(uint32_t len, uint32_t chunk)
{
    int rc = smp_slot_begin(len);
    if (rc) return rc;
    for (uint32_t off = 0; off < len; off += chunk)
        if ((rc = smp_slot_write(off, smp_data + off, len - off < chunk ? len - off : chunk)) != 0) return rc;
    make_hdr(len);
    return smp_slot_end(hdr);
}
static int sample_reads(uint32_t len)
{
    uint32_t l;
    if (smp_slot_info(0, &l, 0) != SMP_FULL || l != len) return 0;
    if (smp_slot_read(0, smp_got, len) || memcmp(smp_got, smp_data, len)) return 0;
    for (uint32_t off = 0; off < len; off += 997u) {          /* (pieces across the tails) */
        uint8_t b[300];
        uint32_t k = len - off < sizeof b ? len - off : sizeof b;
        if (smp_slot_read(off, b, k) || memcmp(b, smp_data + off, k)) return 0;
    }
    return smp_slot_read(len, smp_got, 1) != 0;              /* (past the end: refused) */
}

int main(void)
{
    /* ---------------------------------------------------------------- 1. the claim --- */
    {
        int ok;
        uint32_t in = 0;
        wipe();
        fill_records(24, 2000);                          /* 24 sectors from the cursor: 6 in 18..39 */
        for (uint32_t s = 0; s < OCS_NSECT; s++) in += in_slot(s) && ocs_hdr[s].magic == OCS_MAGIC;
        reboot();
        ok = in > 0 && smp_slot_info(0, 0, 0) == SMP_EMPTY && records_ok(24, 2000);
        reboot();
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY && records_ok(24, 2000) && ocs_resv == smp_resv_mask();
        for (uint32_t s = 0; s < OCS_NSECT; s++) ok &= !in_slot(s) || !ocs_hdr[s].magic;
        check("claim: the records in the slot's 22 sectors moved, all 24 read back after reboots", ok);
    }
    {   /* a full store refuses and leaves the slot alone */
        int ok = 1, n = 0;
        memcpy(nor_before + SMP_SLOT_BASE, nor + SMP_SLOT_BASE, SMP_SLOT_NSECT * 4096u);
        for (uint32_t k = 100; k < 140; k++) {
            if (ocs_write(OCS_POOL, k, rec[0], 3000)) break;
            n++;
        }
        ok &= ocs_write(OCS_POOL, 200, rec[0], 3000) == -1;
        ok &= !memcmp(nor_before + SMP_SLOT_BASE, nor + SMP_SLOT_BASE, SMP_SLOT_NSECT * 4096u);
        ok &= ocs_free_count() == 0 && n == (int)(OCS_NSECT - SMP_SLOT_NSECT) - 24;
        for (uint32_t k = 100; k < 100u + (uint32_t)n; k++) ok &= !ocs_delete(OCS_POOL, k);
        ok &= !memcmp(nor_before + SMP_SLOT_BASE, nor + SMP_SLOT_BASE, SMP_SLOT_NSECT * 4096u) && records_ok(24, 2000);
        check("the store keeps to its 34 sectors: filled, refused and emptied again without touching the slot", ok);
    }
    {   /* (0.6.0 rev 2) nothing in the slot's sectors: the boot writes nothing, the slot is there */
        int ok;
        wipe();
        fill_records(8, 3000);                           /* (one sector each: 0..7) */
        memcpy(nor_before, nor, sizeof nor);
        reboot();
        ok = smp_slot_info(0, 0, 0) == SMP_EMPTY && ocs_resv == smp_resv_mask() && !memcmp(nor_before, nor, sizeof nor);
        reboot();
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY && !memcmp(nor_before, nor, sizeof nor) && records_ok(8, 3000);
        check("claim with nothing to move: no flash written at boot, the slot there", ok);
    }
    {   /* a record moved out of the slot, then deleted: its old copy there does not come back */
        int ok;
        uint32_t back = 0;
        wipe();
        fill_records(24, 2000);
        reboot();
        for (uint32_t k = 0; k < 24u; k++) ocs_delete(OCS_POOL, k);
        reboot();
        reboot();
        for (uint32_t k = 0; k < 24u; k++) back += ocs_read(OCS_POOL, k, got, sizeof got) > 0;
        ok = !back && smp_slot_info(0, 0, 0) == SMP_EMPTY;
        check("records moved out of the slot and deleted stay deleted after reboots", ok);
    }
    {   /* no room for the moves: nothing moved, no slot; room made: claimed */
        int ok;
        wipe();
        fill_records(27, 5000);                          /* 54 of 56 sectors: no room for what sits in 18..39 */
        memcpy(nor_before, nor, sizeof nor);
        reboot();
        ok = smp_slot_info(0, 0, 0) == SMP_UNCLAIMED && !memcmp(nor_before, nor, sizeof nor) && records_ok(27, 5000) &&
             !ocs_resv && smp_slot_begin(100) == -1;
        for (uint32_t k = 0; k < 14u; k++) ocs_delete(OCS_POOL, k);
        reboot();
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY;
        for (uint32_t k = 14; k < 27u; k++)
            ok &= ocs_read(OCS_POOL, k, got, sizeof got) == 5000 && !memcmp(got, rec[k], 5000);
        check("claim with no room: nothing moved or erased, no slot; once records are deleted, claimed at boot", ok);
    }
    /* ------------------------------------------------- 2. power cuts during the claim --- */
    {
        int ok = 1, ended = 0;
        for (int cut = 0; cut < 400 && !ended; cut += 3) {
            wipe();
            fill_records(14, 5000);                      /* (28 sectors: 10 in the slot, two-part records across it) */
            fail_after = cut;
            reboot();
            fail_after = -1;
            reboot();
            ok &= records_ok(14, 5000) && smp_slot_info(0, 0, 0) == SMP_EMPTY;
            if (!ok) { printf("  cut after %d program calls: records %d state %u\n", cut, records_ok(14, 5000), smp_slot_info(0, 0, 0)); break; }
            {   /* (past the claim's last program call: the cut never lands) */
                wipe(); fill_records(14, 5000); fail_after = cut; reboot();
                ended = fail_after > 0; fail_after = -1;
            }
        }
        check("power cut at every 3rd program call of the claim: every record reads back, the claim ends", ok && ended);
    }
    /* ------------------------------------------------------- 3. a sample, its edges --- */
    {
        int ok;
        wipe();
        fill_records(10, 4000);
        reboot();
        make_sample(SMP_SLOT_MAX, 1);
        ok = write_sample(SMP_SLOT_MAX, 256) == 0 && sample_reads(SMP_SLOT_MAX);
        reboot();
        ok &= sample_reads(SMP_SLOT_MAX) && records_ok(10, 4000) && tails_erased();
        {
            char name[9];
            uint32_t nz;
            smp_slot_info(name, 0, &nz);
            ok &= !strcmp(name, "TESTSMP") && nz == 1u;
        }
        check("a sample of 81,408 bytes (a Felucca slot's): written, read back at every place, after a reboot; tails erased", ok);
        make_sample(1, 2);
        ok = write_sample(1, 256) == 0 && sample_reads(1);
        reboot();
        ok &= sample_reads(1);
        check("a sample of 1 byte: written and read back, after a reboot too", ok);
        ok = smp_slot_begin(0) == -1 && smp_slot_begin(SMP_SLOT_MAX + 1u) == -1;
        check("sizes 0 and 81,409 refused", ok);
        make_sample(5000, 3);
        ok = smp_slot_begin(5000) == 0 && smp_slot_write(0, smp_data, 100) == 0 && smp_slot_write(200, smp_data + 200, 100) == -1;
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY;
        ok &= smp_slot_begin(5000) == 0 && smp_slot_write(0, smp_data, 5000) == 0 && smp_slot_write(5000, smp_data, 1) == -1;
        make_hdr(5000);
        ((smp_hdr_t *)hdr)->crc ^= 1u;
        ok &= smp_slot_end(hdr) == -1 && smp_slot_info(0, 0, 0) == SMP_EMPTY;
        ok &= smp_slot_begin(5000) == 0 && smp_slot_write(0, smp_data, 4000) == 0;
        make_hdr(5000);
        ok &= smp_slot_end(hdr) == -1;                       /* (not all the data) */
        ok &= smp_slot_begin(5000) == 0 && smp_slot_write(0, smp_data, 5000) == 0;
        make_hdr(5000);
        ((smp_hdr_t *)hdr)->magic = 0;
        ok &= smp_slot_end(hdr) == -1 && smp_slot_info(0, 0, 0) == SMP_EMPTY;
        reboot();
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY;
        check("out of order, past the end, a bad CRC, a short write, a bad header: refused, the slot left empty", ok);
        make_sample(3000, 4);
        ok = write_sample(3000, 256) == 0 && smp_slot_erase() == 0 && smp_slot_info(0, 0, 0) == SMP_EMPTY;
        reboot();
        ok &= smp_slot_info(0, 0, 0) == SMP_EMPTY && smp_marked();
        check("erase: the slot empty and still the slot's, after a reboot too", ok);
    }
    /* ---------------------------------------- 4. power cuts during a write --- */
    {
        int ok = 1, ended = 0;
        uint32_t len = 20000u;
        for (int cut = 0; cut < 200 && !ended; cut++) {
            wipe();
            fill_records(10, 4000);
            reboot();
            make_sample(9000, 5);
            if (write_sample(9000, 256)) { ok = 0; break; }
            make_sample(len, 6);
            fail_after = cut;
            int rc = write_sample(len, 256);
            ended = rc == 0;
            fail_after = -1;
            reboot();
            uint32_t st = smp_slot_info(0, 0, 0);
            {   /* (empty, the whole new one, or, cut before the first erase, the one before) */
                uint32_t l = 0;
                smp_slot_info(0, &l, 0);
                if (st == SMP_FULL && l == 9000u) make_sample(9000, 5);
                ok &= records_ok(10, 4000) && (st == SMP_EMPTY || (st == SMP_FULL && sample_reads(l == 9000u ? 9000u : len)));
                ok &= l != 9000u || cut == 0;
            }
            ok &= !ended || st == SMP_FULL;
            if (!ok) { printf("  cut after %d program calls: state %u\n", cut, st); break; }
        }
        check("power cut at every program call of a 20,000-byte write: empty, the whole sample (cut before: the old), the store as it was", ok && ended);
    }
    /* ------------------------------------------------- 5. nothing else touches it --- */
    {
        int ok;
        wipe();
        fill_records(10, 4000);
        reboot();
        make_sample(SMP_SLOT_MAX, 7);
        write_sample(SMP_SLOT_MAX, 256);
        memcpy(nor_before + SMP_SLOT_BASE, nor + SMP_SLOT_BASE, SMP_SLOT_NSECT * 4096u);
        for (uint32_t r = 0; r < 300u; r++) {                 /* saves and deletes rotating over the store */
            ocs_write(OCS_PROJECT, r % 4u, rec[r % 10u], 3000u + r);
            if (r % 7u == 0u) ocs_delete(OCS_PROJECT, (r / 7u) % 4u);
        }
        ok = !memcmp(nor_before + SMP_SLOT_BASE, nor + SMP_SLOT_BASE, SMP_SLOT_NSECT * 4096u) && sample_reads(SMP_SLOT_MAX);
        reboot();
        ok &= sample_reads(SMP_SLOT_MAX) && records_ok(10, 4000) && tails_erased();
        check("300 saves and deletes over the store: the slot byte for byte as written; every tail erased", ok);
        prog_lo = 0xFFFFFFFFu; prog_hi = 0;
        make_sample(SMP_SLOT_MAX, 8);
        write_sample(SMP_SLOT_MAX, 256);
        smp_slot_erase();
        check("the slot's writes and erases stay inside 0xB2000..0xC7FFF",
              prog_lo >= SMP_SLOT_BASE && prog_hi <= SMP_SLOT_BASE + SMP_SLOT_NSECT * 4096u);
    }
    printf(bad ? "sample slot: %d FAILED\n" : "sample slot: all passed\n", bad);
    return bad ? 1 : 0;
}
