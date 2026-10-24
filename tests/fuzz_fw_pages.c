/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2 (Part 2b): a fuzzer of the loader's check of a module's page descriptor (firmware/src/fw_pages.h: fw_pages_check), on the wire image of the
 * REAL Prophet descriptor (tests/fw_pages_wire.h writes tools/module/prophet_mod.c's P5_PAGES the way the linker lays it out for the device).
 *   build/host/fuzz_fw_pages [seconds] [seed]      default 3 s, seed 20260615; a long run is `fuzz_fw_pages 1800`
 * Built with the Prophet module's own object (tests/module_host_prophet_tu.c, OC_MODULE_HOST), which only supplies the descriptor here.
 * Each round: 1 to 8 mutations of the image (a bit flip, a byte or a 32 bit word set to 0 / FFFFFFFF / a pointer into or out of the image / random, a copy of a
 * piece onto another, a cut to 0xFF) and the check. Invariants (a failure prints the seed, the round and the mutations):
 *   - no crash and no out of bounds read: the reader the check gets counts every read outside [image) and the check must not make one;
 *   - a descriptor the check accepts can be FOLLOWED the way the UI does without leaving the image or meeting a null pointer: every row title and parameter
 *     label and unit is a string that ends inside the image, a panel entry of F_ENUM has its names (min >= 0, names[0..max] strings), an F_INT name list ends in a
 *     0 inside the image, the format is one params.c's param_format knows, the knobs, the ids of every row and the CC targets are inside the panel, the title lists end;
 *   - the unmutated image passes, so the fuzzer is not rejecting everything: the count of accepted mutated images is printed.
 * Revisions: 0.12.2: first version. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "build_options.h"
#include "melodee_tables.h"
#include "core.h"
#include "fw_pages.h"
#include "fw_api.h"
#include "fw_pages_wire.h"

extern const fw_pages_t *oc_host_module_pages(void);

#define BASE 0x020C4000u
#define CAP 16384u
static uint8_t img[CAP], orig[CAP];
static uint32_t n_img, oob_reads;
static int rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    (void)ctx;
    if (addr < BASE || addr - BASE + n > n_img) { oob_reads++; return 1; }
    memcpy(buf, img + (addr - BASE), n);
    return 0;
}
static uint32_t fz;
static uint32_t fr(void) { fz ^= fz << 13; fz ^= fz >> 17; fz ^= fz << 5; return fz; }
static char mlog[10][96];
static int nm;
static void note(const char *fmt, uint32_t a, uint32_t b) { if (nm < 10) snprintf(mlog[nm], sizeof mlog[0], fmt, a, b); nm++; }
static uint32_t u32(uint32_t a) { return a >= BASE && a - BASE + 4u <= n_img ? fwp_u32(img + (a - BASE)) : 0xFFFFFFFFu; }
static int str_ok(uint32_t a)                                /* a string that ends inside the image */
{
    if (a < BASE + 0x100u) return 0;
    for (uint32_t i = 0; i < 25u; i++) { if (a - BASE + i >= n_img) return 0; if (!img[a - BASE + i]) return 1; }
    return 0;
}
static int follow(uint32_t da)                               /* the UI's use of an accepted descriptor, on the bytes: 1 when it never leaves the image */
{
    const uint8_t *d = img + (da - BASE);
    uint32_t n = d[8] | d[9] << 8, np = d[10] | d[11] << 8, rows = fwp_u32(d + 12), panel = fwp_u32(d + 16), i, k;
    if (!n || n > FW_PAGES_MAX || !np || np > FW_PANEL_MAX) return 0;
    for (i = 0; i < n; i++) {
        const uint8_t *r = img + (rows - BASE) + 12u * i;
        if (rows < BASE || rows - BASE + 12u * n > n_img || !str_ok(fwp_u32(r))) return 0;
        for (k = 0; k < 4u; k++) if (r[7 + k] != 0xFFu && r[7 + k] >= np) return 0;
    }
    for (i = 0; i < np; i++) {
        const uint8_t *p = img + (panel - BASE) + 20u * i;
        int mn = (int16_t)(p[6] | p[7] << 8), mx = (int16_t)(p[8] | p[9] << 8);
        uint32_t names = fwp_u32(p + 12);
        if (panel < BASE || panel - BASE + 20u * np > n_img) return 0;
        if (!fwp_u32(p)) continue;
        if (!str_ok(fwp_u32(p)) || (fwp_u32(p + 16) && !str_ok(fwp_u32(p + 16)))) return 0;
        if (p[4] > F_TONE || mn > mx) return 0;
        if (p[4] == F_ENUM) {
            if (!names || mn < 0 || mx >= 128) return 0;
            for (int v = mn; v <= mx; v++) if (!str_ok(u32(names + 4u * (uint32_t)v))) return 0;
        } else if (names) {
            if (p[4] != F_INT) return 0;
            for (k = 0;; k++) { uint32_t s = u32(names + 4u * k); if (k > 200u) return 0; if (!s) break; if (!str_ok(s)) return 0; }
            if (k == 0) return 0;
        }
    }
    for (int h = 0; h < 2; h++) {
        uint32_t l = fwp_u32(d + 40 + 4 * h);
        if (!l) continue;
        for (k = 0;; k++) { uint32_t s = u32(l + 4u * k); if (k > 9u) return 0; if (!s) break; if (!str_ok(s)) return 0; }
    }
    for (i = 0; i < 4u; i++) if ((d[48 + i] != 0xFFu && d[48 + i] >= np) || (d[64 + i] != 0xFFu && d[64 + i] >= np)) return 0;
    for (i = 0; i < 7u; i++) { uint32_t h = fwp_u32(d + (i < 4u ? 20u + 4u * i : 52u + 4u * (i - 4u))); if (h && (h < BASE + 0x100u || h >= BASE + n_img)) return 0; }
    { uint32_t sk = fwp_u32(d + 36); if (sk && (sk < BASE + 0x100u || sk - BASE + (np + 7u) / 8u > n_img)) return 0; }
    return 1;
}
int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 3.0;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 20260615u, da = 0, len;
    uint64_t rounds = 0, muts = 0, accepted = 0;
    time_t t0 = time(0);
    int fails = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    fz = seed ? seed : 1u;
    len = fwp_wire(oc_host_module_pages(), orig, CAP, BASE, &da);
    if (!len) { printf("fuzz_fw_pages: the descriptor does not fit the image\n"); return 1; }
    n_img = len;
    memcpy(img, orig, CAP);
    if (fw_pages_check(rd, 0, da, BASE + 0x100u, BASE + len) != FWP_OK || !follow(da)) { printf("fuzz_fw_pages: the unmutated Prophet descriptor does not pass\n"); return 1; }
    for (;;) {
        int rc, step = 0, errs = 0;
        memcpy(img, orig, CAP);
        nm = 0;
        uint32_t nmut = 1u + fr() % 8u;
        for (uint32_t i = 0; i < nmut; i++) {
            uint32_t k = fr() % 9u, off = 0x100u + fr() % (len - 0x100u), t = 0x100u + fr() % (len - 0x100u);
            uint32_t ptr[4] = { BASE + 0x100u + fr() % (len - 0x100u), BASE + len + fr() % 64u, BASE + fr() % 0x100u, 0x20000000u };
            switch (k) {
            case 0: img[off] ^= (uint8_t)(1u << (fr() & 7u)); note("flip off=%x (%u)", off, 0); break;
            case 1: img[off] = (uint8_t)fr(); note("random byte off=%x (%u)", off, 0); break;
            case 2: img[off] = 0; note("zero byte off=%x (%u)", off, 0); break;
            case 3: img[off] = 0xFF; note("ff byte off=%x (%u)", off, 0); break;
            case 4: off &= ~3u; if (off + 4u <= len) { uint32_t v = ptr[fr() & 3u]; img[off] = (uint8_t)v; img[off + 1] = (uint8_t)(v >> 8); img[off + 2] = (uint8_t)(v >> 16); img[off + 3] = (uint8_t)(v >> 24); } note("pointer word off=%x (%u)", off, 0); break;
            case 5: off &= ~3u; if (off + 4u <= len) { memset(img + off, 0, 4); } note("zero word off=%x (%u)", off, 0); break;
            case 6: { uint32_t n = 1u + fr() % 24u; if (off + n <= len && t + n <= len) memmove(img + t, img + off, n); note("copy piece %x onto %x", off, t); break; }
            case 7: { uint32_t n = 1u + fr() % 16u; if (off + n <= len) memset(img + off, 0xFF, n); note("ff run at %x (%u)", off, n); break; }
            default: { uint32_t da_off = da - BASE + (fr() % FW_PAGES_WIRE); img[da_off] = (uint8_t)fr(); note("descriptor byte %x (%u)", da_off, 0); break; }
            }
        }
        muts += nmut; rounds++;
        oob_reads = 0;
        rc = fw_pages_check(rd, 0, da, BASE + 0x100u, BASE + len);
        step = 1;
        if (oob_reads) { errs |= 1; goto fail; }
        if (rc == FWP_OK) {
            accepted++;
            step = 2;
            if (!follow(da)) { errs |= 2; goto fail; }
        }
        if (difftime(time(0), t0) >= secs) break;
        continue;
    fail:
        fails++;
        printf("fuzz_fw_pages: FAIL seed %u round %llu step %d errs 0x%x rc %d after %d mutations:\n", seed, (unsigned long long)rounds, step, errs, rc, nm);
        for (int i = 0; i < nm && i < 10; i++) printf("    %s\n", mlog[i]);
        if (fails >= 3 || difftime(time(0), t0) >= secs) break;
    }
    printf("fuzz_fw_pages: seed %u, %llu rounds, %llu mutations, %llu mutated images accepted: %s\n", seed, (unsigned long long)rounds, (unsigned long long)muts,
           (unsigned long long)accepted, fails ? "FAIL" : "all passed");
    return fails != 0;
}
