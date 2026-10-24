/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2 (Part 2b): a fuzzer of the module loader (firmware/src/oc_module.c: oc_mod_boot at every start, oc_mod_plan, oc_mod_begin / write / end) on the
 * simulated NOR of tests/oc_module_test.c (built with OC_MODULE_WINS 1, as that test).
 *   build/host/fuzz_oc_module [seconds] [seed]      default 3 s, seed 20260614; a long run is `fuzz_oc_module 1800`
 * Each round: 0 to 2 good modules (SHAPES at start 0 or 1, the Prophet at start 0 or 2, any mix, overlapping or not), then 1 to 6 mutations of the 12 sectors
 * of the module area (a bit flip, a byte set, a header field, a sector erased, copied onto another, cut at a length, two sectors swapped), then a boot.
 * Invariants (a failure prints the seed, the round and the mutations):
 *   - no crash, no out of bounds, no undefined behaviour;
 *   - a module the loader registers (MD_OK) passes an independent check in the flash: the header CRC, the CRC of its code, this firmware's interface, a link
 *     address that is its start's, an engine_t inside the code; a module that fails any of them is never registered, never reaches the table;
 *   - after the boot an install of a good SHAPES or Prophet works when the planner names a start (the flash holds the image, a reboot registers it), and
 *     when it names none (-1) the refusal is MI_NOFREE with oc_mod_blocked_by naming a start that holds a usable module of another engine; nothing of another
 *     engine is erased by an install that succeeds;
 *   - the library group (a bank saved before) still loads after every module operation.
 * Revisions: 0.12.2: first version. */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

static int bad;
#define NSH 2u                                        /* (0.12.1) SHAPES' starts (sectors 0 and 6); the loader has a third (sector 4, the Prophet's) */
#define SHLEN (6u * 0x1000u)                          /* SHAPES' six sectors */
static void chk(const char *what, int ok)
{
    printf("module loader: %-104s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define LEN 3000u                                     /* a made module: header page + 2,744 bytes of "code" */
static uint8_t IMG[OC_MOD_MAXLEN];
static uint32_t modeng = ENGI_SHAPES;                /* (0.12.1) the engine mk gives a module (the Prophet block sets it) */
static uint32_t modver;                               /* (0.11.1) the header version word mk gives (0 unless a test sets it) */
static fw_mod_hdr_t mk(uint32_t i, uint32_t len)      /* the fields a good SHAPES module of this start has */
{
    fw_mod_hdr_t h;
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.version = modver;
    h.engine = modeng; h.state_size = 1234u; h.engine_ptr = oc_mod_link(i) + 0x180u; h.link_addr = oc_mod_link(i);
    h.pack_hash = OC_PACK_HASH; h.length = len;
    return h;
}
static void seal(fw_mod_hdr_t *h)                     /* crc over the code in IMG, then hcrc */
{
    h->crc = ocs_crc32(IMG + FW_MOD_CODE_OFF, h->length - FW_MOD_CODE_OFF);
    h->hcrc = ocs_crc32((const uint8_t *)h, FW_MOD_HDR_BYTES - 4u);
}
static void put(uint32_t i, const fw_mod_hdr_t *h, int header)   /* the module into the flash: code first, the header page last */
{
    memcpy(IMG, h, sizeof *h);
    memcpy(nor + oc_mod_flash(i) + FW_MOD_CODE_OFF, IMG + FW_MOD_CODE_OFF, h->length - FW_MOD_CODE_OFF);
    if (header) memcpy(nor + oc_mod_flash(i), IMG, FW_MOD_CODE_OFF);
}
static void code(uint32_t seed) { for (uint32_t k = 0; k < sizeof IMG; k++) IMG[k] = (uint8_t)(k * 31u + seed); memset(IMG, 0, FW_MOD_CODE_OFF); }
static void clean(void)                               /* a new flash, a power-on's RAM */
{
    memset(nor, 0xFF, sizeof nor);
    memset(&oc_modguard, 0, sizeof oc_modguard);
    memset(trk, 0, sizeof trk);
    for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fm1_guard_code_end = 0;
}
static void good(uint32_t i, uint32_t len) { fw_mod_hdr_t h = mk(i, len); code(i + 1u); seal(&h); put(i, &h, 1); }
/* one bad header: made by `f`, sealed, written */
typedef void (*mut_t)(fw_mod_hdr_t *);
static uint32_t with(uint32_t i, mut_t f, int reseal, uint32_t *why)
{
    fw_mod_hdr_t h = mk(i, LEN);
    code(7);
    if (f) f(&h);
    if (reseal) seal(&h); else { h.crc = ocs_crc32(IMG + FW_MOD_CODE_OFF, LEN - FW_MOD_CODE_OFF); h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); }
    put(i, &h, 1);
    oc_mod_boot(0);
    if (why) *why = oc_mod.why[i];
    return oc_mod.state[i];
}

#include <time.h>
static uint32_t fz;
static uint32_t fr(void) { fz ^= fz << 13; fz ^= fz >> 17; fz ^= fz << 5; return fz; }
static char mlog[8][96];
static int nm;
static void note(const char *fmt, uint32_t a, uint32_t b) { if (nm < 8) snprintf(mlog[nm], sizeof mlog[0], fmt, a, b); nm++; }
static int indep_ok(uint32_t i)                       /* the module at start i, checked from the flash by this file's own reading of the format */
{
    const fw_mod_hdr_t *h = (const fw_mod_hdr_t *)(nor + oc_mod_flash(i));
    if (h->magic != FW_MOD_MAGIC || h->hcrc != ocs_crc32((const uint8_t *)h, FW_MOD_HDR_BYTES - 4u)) return 0;
    if (h->iface != FW_API_IFACE || h->api_addr != FW_API_ADDR || h->link_addr != oc_mod_link(i)) return 0;
    if (h->length < FW_MOD_CODE_OFF || (h->length & 3u) || h->length > OC_MOD_MAXLEN) return 0;
    if (h->engine_ptr < h->link_addr + FW_MOD_CODE_OFF || h->engine_ptr >= h->link_addr + h->length) return 0;
    return h->crc == ocs_crc32(nor + oc_mod_flash(i) + FW_MOD_CODE_OFF, h->length - FW_MOD_CODE_OFF);
}
static uint32_t eng_geo_sectors(uint32_t e) { return e == ENGI_PROPHET ? 8u : 6u; }
/* a module of engine e for start `start` (the loader's numbering), as the .ocm carries it */
static uint32_t build_for(uint32_t e, uint32_t start, uint32_t len, uint32_t seed)
{
    fw_mod_hdr_t h = mk(start, len);
    h.engine = e;
    code(seed); h.engine_ptr = oc_mod_link(start) + 0x180u;
    seal(&h);
    memcpy(IMG, &h, sizeof h);
    return len;
}
static int install_img(uint32_t e, uint32_t start, uint32_t len, int *rc_out)
{
    int r = oc_mod_begin(e, start, len);
    for (uint32_t o = FW_MOD_CODE_OFF; !r && o < len; o += 256u) r = oc_mod_write(o, IMG + o, len - o > 256u ? 256u : len - o);
    if (!r) r = oc_mod_end(IMG);
    *rc_out = r;
    return r;
}
int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 3.0;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 20260614u;
    uint64_t rounds = 0, muts = 0, registered = 0, refusedmods = 0, installs = 0, nofree = 0;
    time_t t0 = time(0);
    int fails = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    fz = seed ? seed : 1u;
    for (;;) {
        int errs = 0, step = 0;
        clean();
        for (uint32_t k = fr() % 3u; k > 0; k--) {          /* good modules, any start of either engine (they may overlap: a damaged flash can show that) */
            uint32_t e = (fr() & 1u) ? ENGI_PROPHET : ENGI_SHAPES, st = e == ENGI_PROPHET ? (fr() & 1u ? 2u : 0u) : (fr() & 1u);
            uint32_t len = e == ENGI_PROPHET ? 5000u + 4u * (fr() % 700u) : 3000u + 4u * (fr() % 500u);
            build_for(e, st, len, fr());
            memcpy(nor + oc_mod_flash(st), IMG, len);
        }
        nm = 0;
        uint32_t nmut = 1u + fr() % 6u;
        for (uint32_t i = 0; i < nmut; i++) {
            uint32_t k = fr() % 8u, s = ST_MODULE_LO + (fr() % 12u) * 4096u, t = ST_MODULE_LO + (fr() % 12u) * 4096u;
            uint32_t off = (fr() & 1u) ? fr() % FW_MOD_HDR_BYTES : fr() % 4096u;
            uint8_t *q = nor + s + off;
            switch (k) {
            case 0: *q ^= (uint8_t)(1u << (fr() & 7u)); note("flip sector %x off=%u", s, off); break;
            case 1: *q = 0x00; note("zero sector %x off=%u", s, off); break;
            case 2: *q = 0xFF; note("ff sector %x off=%u", s, off); break;
            case 3: *q = (uint8_t)fr(); note("random sector %x off=%u", s, off); break;
            case 4: memset(nor + s, 0xFF, 4096); note("erase sector %x (%u)", s, 0); break;
            case 5: memcpy(nor + t, nor + s, 4096); note("copy sector %x onto %x", s, t); break;
            case 6: memset(nor + s + off, 0xFF, 4096u - off); note("cut sector %x at %u", s, off); break;
            default: { uint8_t tmp[4096]; memcpy(tmp, nor + s, 4096); memcpy(nor + s, nor + t, 4096); memcpy(nor + t, tmp, 4096); note("swap %x and %x", s, t); break; }
            }
        }
        muts += nmut; rounds++;
        memset(&oc_modguard, 0, sizeof oc_modguard);
        for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e);
        memset(&oc_mod, 0, sizeof oc_mod);
        oc_mod_boot(0); step = 1;
        for (uint32_t i = 0; i < 3u; i++) {
            if (oc_mod.state[i] == MD_OK) {
                registered++;
                if (!indep_ok(i)) { const fw_mod_hdr_t *h = (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); printf("  states %u %u %u engines %u %u %u eng_mod[14]=%d eng_mod[19]=%d\n", oc_mod.state[0], oc_mod.state[1], oc_mod.state[2], oc_mod.engine[0], oc_mod.engine[1], oc_mod.engine[2], eng_mod[14] != 0, eng_mod[19] != 0); printf("  start %u: len %u eng %u eptr %x link %x magic %d hcrc %d iface %d crc %d\n", i, h->length, h->engine, h->engine_ptr, h->link_addr, h->magic == FW_MOD_MAGIC, h->hcrc == ocs_crc32((const uint8_t *)h, FW_MOD_HDR_BYTES - 4u), h->iface == FW_API_IFACE, h->length >= FW_MOD_CODE_OFF && h->length <= OC_MOD_MAXLEN && h->crc == ocs_crc32(nor + oc_mod_flash(i) + FW_MOD_CODE_OFF, h->length - FW_MOD_CODE_OFF)); errs |= 1; goto fail; }
                if (!eng_mod[oc_mod.engine[i]]) { errs |= 2; goto fail; }
            } else if (oc_mod.state[i] != MD_EMPTY) refusedmods++;
        }
        for (uint32_t e = 0; e < NENGINES; e++)               /* nothing in the table that no start supplies */
            if (eng_mod[e]) { int found = 0; for (uint32_t i = 0; i < 3u; i++) found |= oc_mod.state[i] == MD_OK && oc_mod.engine[i] == e && eng_mod[e] == oc_mod_hdr_ptr(i); if (!found) { errs |= 4; goto fail; } }
        step = 2;
        {   /* an install of either engine */
            uint32_t e = (fr() & 1u) ? ENGI_PROPHET : ENGI_SHAPES, sect = eng_geo_sectors(e);
            int plan = oc_mod_plan(e), rc;
            uint8_t other[12][4096]; uint32_t ownsec = 0;     /* the sectors of the usable modules of this engine, before the install: it may erase them */
            for (uint32_t j = 0; j < 3u; j++) if ((oc_mod.state[j] == MD_OK || oc_mod.state[j] == MD_CRASHED || oc_mod.state[j] == MD_NOSLOT) && oc_mod.engine[j] == e)
                for (uint32_t q = 0; q < sect; q++) ownsec |= 1u << (OC_MOD_START_SECTOR(j) + q);
            for (uint32_t k = 0; k < 12u; k++) memcpy(other[k], nor + ST_MODULE_LO + k * 4096u, 4096);
            if (plan < 0) {
                nofree++;
                int b = oc_mod_blocked_by(e);
                if (oc_mod_begin(e, 0, 3000u) != MI_NOFREE && oc_mod_begin(e, 1, 3000u) != MI_NOFREE) { errs |= 8; goto fail; }
                if (b >= 0 && !(oc_mod.state[b] == MD_OK || oc_mod.state[b] == MD_CRASHED || oc_mod.state[b] == MD_NOSLOT)) { errs |= 16; goto fail; }
                if (b >= 0 && oc_mod.engine[b] == e) { errs |= 32; goto fail; }
            } else {
                uint32_t len = e == ENGI_PROPHET ? 6000u : 3000u;
                build_for(e, (uint32_t)plan, len, fr());
                if (install_img(e, (uint32_t)plan, len, &rc) != 0) { errs |= 64; goto fail; }
                installs++;
                if (memcmp(nor + oc_mod_flash((uint32_t)plan), IMG, len) || oc_mod.state[plan] != MD_OK || !eng_mod[e]) { errs |= 128; goto fail; }
                /* the sectors outside the new module's span, and outside the old module of this engine, are untouched */
                uint32_t lo = OC_MOD_START_SECTOR((uint32_t)plan), hi = lo + sect;
                for (uint32_t k = 0; k < 12u; k++) {
                    int own = (ownsec >> k) & 1;
                    if ((k < lo || k >= hi) && !own && memcmp(other[k], nor + ST_MODULE_LO + k * 4096u, 4096)) { errs |= 256; goto fail; }
                }
                for (uint32_t k = 0; k < NENGINES; k++) eng_unregister(k);
                memset(&oc_mod, 0, sizeof oc_mod);
                oc_mod_boot(0);
                if (!eng_mod[e]) { errs |= 512; goto fail; }
            }
        }
        if (difftime(time(0), t0) >= secs) break;
        continue;
    fail:
        fails++;
        printf("fuzz_oc_module: FAIL seed %u round %llu step %d errs 0x%x after %d mutations:\n", seed, (unsigned long long)rounds, step, errs, nm);
        for (int i = 0; i < nm && i < 8; i++) printf("    %s\n", mlog[i]);
        if (fails >= 3 || difftime(time(0), t0) >= secs) break;
    }
    printf("fuzz_oc_module: seed %u, %llu rounds, %llu mutations, %llu modules registered, %llu starts refused, %llu installs, %llu with no place: %s\n", seed,
           (unsigned long long)rounds, (unsigned long long)muts, (unsigned long long)registered, (unsigned long long)refusedmods, (unsigned long long)installs,
           (unsigned long long)nofree, fails ? "FAIL" : "all passed");
    return fails != 0;
}
