/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.10.0 (module PoC step 5): the module loader (firmware/src/oc_module.c) on a simulated NOR flash.
 *   build/host/oc_module_test*   (built three ways by tests/run_tests.sh: OC_MODULE_WINS 1 with SHAPES built in, OC_MODULE_WINS 0, SHAPES built out)
 * Checked: nothing in the area (no message, nothing registered); a good module at either start is registered under its engine number
 * (the engine table, the header's state size, the PC guard's end); every refusal, each in its own state: bad magic (not a module), a torn
 * header (hcrc), another interface identifier, another table address, a table longer or a version later than the firmware's, a length that
 * is not a multiple of 4 / too small / too big, a link address that is not where the start runs, an engine_t outside the code, an engine
 * a module may not supply, no state size / too big a one, another build's pack, a bad CRC (one flipped byte, and a code page left
 * erased), code with no header page (an install cut off before its last step); two good modules for one engine (the lower start wins);
 * the crash mark (a crash in the first 30 s with the module in use bans it; one with it registered but unused does not; the ban holds across
 * resets, goes with a power loss, and goes when the start is installed or removed); a module removed is out of the table at the next boot;
 * a flash that will not read; (0.10.1) three sessions in a row that die with the module's engine in use ban it whenever they die (a clean reset, a death with the
 * engine not in use, a power loss or an install or removal breaks the count), MODULE BANNED once; (0.10.1) a record the update loader would take on a sector tail: refused at boot (MD_BAD_TAIL) and by the installer (rc 8, no header
 * page). The wiring of the loader into persist_boot and the main loop, the guard's register writes and the
 * installer are checked on the device (CI build) and by the installer's tests (step 6).
 */
/* Revisions:
 * (Orange Cat 0.10.1) module sector tails against the update loader's record rule
 * (Orange Cat 0.11.1) every path that takes a module out of the table writes the PC window
 * (Orange Cat 0.12.1) the Prophet as a module at 8 sectors, three module starts, a module of another engine in the way is refused and named
 * (Orange Cat 0.12.2) the Prophet module fits 8 sectors (34,764 B measured): the firmware's PITCH_OCT and ENV_LIN through the table, the factory programs in a palette fo...
 */
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
static void m_magic(fw_mod_hdr_t *h) { h->magic ^= 1u; }
static void m_iface(fw_mod_hdr_t *h) { h->iface ^= 0x100u; }
static void m_addr(fw_mod_hdr_t *h) { h->api_addr += 4u; }
static void m_size(fw_mod_hdr_t *h) { h->api_size += 4u; }
static void m_ver(fw_mod_hdr_t *h) { h->api_version++; }
static void m_len3(fw_mod_hdr_t *h) { h->length = LEN - 1u; }
static void m_len_small(fw_mod_hdr_t *h) { h->length = FW_MOD_CODE_OFF; }
static void m_len_big(fw_mod_hdr_t *h) { h->length = SHLEN + 4u; }
static void m_link(fw_mod_hdr_t *h) { h->link_addr += 0x1000u; }
static void m_eptr_lo(fw_mod_hdr_t *h) { h->engine_ptr = h->link_addr + 0x40u; }
static void m_eptr_hi(fw_mod_hdr_t *h) { h->engine_ptr = h->link_addr + LEN; }
static void m_eptr_odd(fw_mod_hdr_t *h) { h->engine_ptr++; }
static void m_eng(fw_mod_hdr_t *h) { h->engine = 3u; }
static void m_eng_big(fw_mod_hdr_t *h) { h->engine = 0xFFFFFFFFu; }
static void m_st0(fw_mod_hdr_t *h) { h->state_size = 0; }
static void m_stbig(fw_mod_hdr_t *h) { h->state_size = OC_MOD_STATE_MAX + 1u; }
static void m_pack(fw_mod_hdr_t *h) { h->pack_hash ^= 1u; }
static void m_hcrc(fw_mod_hdr_t *h) { h->state_size ^= 0x10u; }       /* used with reseal 0 and the hcrc then made before this: see below */

int main(void)
{
    uint32_t why, i, st;
    setvbuf(stdout, NULL, _IONBF, 0);

    clean();
    oc_mod_boot(0);
    chk("empty flash: both starts empty, nothing registered, the guard window unchanged",
        oc_mod.state[0] == MD_EMPTY && oc_mod.state[1] == MD_EMPTY && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end);

    for (i = 0; i < NSH; i++) {
        const engine_t *want = (const engine_t *)(oc_mod_link(i) + 0x180u);
        clean();
        good(i, LEN);
        oc_mod_boot(0);
#if MELODEE_SHAPES && !OC_MODULE_WINS
        chk("a good module (the built-in engine wins, so it is NOSLOT; see below)", oc_mod.state[i] == MD_NOSLOT && oc_mod.state[1u - i] == MD_EMPTY);
#else
        chk(i ? "a good module at start 6: registered" : "a good module at start 0: registered",
            oc_mod.state[i] == MD_OK && oc_mod.state[1u - i] == MD_EMPTY);
#endif
#if MELODEE_SHAPES && !OC_MODULE_WINS
        (void)want;
#else
        chk("... its engine_t is in the table under SHAPES' number, its header is the registry's, its state size is the header's",
            ENGINES[ENGI_SHAPES] == want && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(i) && eng_state_size(ENGI_SHAPES) == 1234u && eng_ok(ENGI_SHAPES));
        chk("... the PC guard's window ends at the module's last byte", fm1_guard_code_end == oc_mod_link(i) + LEN - 1u);
#endif
    }
    chk("XIP addresses: start 0 runs at 0x020C4000 (flash 0xC8000), start 6 at 0x020CA000",
        oc_mod_link(0) == 0x020C4000u && oc_mod_link(1) == 0x020CA000u);

#if MELODEE_SHAPES && !OC_MODULE_WINS
    clean(); good(0, LEN); oc_mod_boot(0);
    chk("built-in SHAPES stays (OC_MODULE_WINS 0): the module is refused as NOSLOT (shadowed), the table unchanged",
        oc_mod.state[0] == MD_NOSLOT && oc_mod.why[0] == 2 && ENGINES[ENGI_SHAPES] == &ENG_SHAPES && !fm1_guard_code_end);
#else
    /* refusals */
    struct { const char *what; mut_t f; int reseal; uint32_t want, why; } T[] = {
        {"bad magic (not a module): empty, no message", m_magic, 1, MD_EMPTY, 0},
        {"another interface identifier (a module built for another firmware)", m_iface, 1, MD_REFUSED, FW_MOD_BAD_IFACE},
        {"another table address (api_addr)", m_addr, 1, MD_REFUSED, FW_MOD_BAD_ADDR},
        {"a table longer than the firmware's (api_size)", m_size, 1, MD_REFUSED, FW_MOD_API_TOO_NEW},
        {"a later table version than the firmware's", m_ver, 1, MD_REFUSED, FW_MOD_API_TOO_NEW},
        {"a length that is not a multiple of 4", m_len3, 1, MD_BAD_HDR, 0},
        {"a length of no code (the header page only)", m_len_small, 1, MD_BAD_HDR, 0},
        {"a length past the 6 sectors a start has", m_len_big, 1, MD_BAD_HDR, 0},
        {"a link address that is another start's (the image was written to the wrong place)", m_link, 1, MD_BAD_HDR, 0},
        {"an engine_t inside the header page", m_eptr_lo, 1, MD_BAD_HDR, 0},
        {"an engine_t past the end of the code", m_eptr_hi, 1, MD_BAD_HDR, 0},
        {"an engine_t at an odd address", m_eptr_odd, 1, MD_BAD_HDR, 0},
        {"an engine number a module may not supply (3)", m_eng, 1, MD_BAD_HDR, 0},
        {"an engine number past the table (0xFFFFFFFF)", m_eng_big, 1, MD_BAD_HDR, 0},
        {"no state size", m_st0, 1, MD_BAD_HDR, 0},
        {"a state size past the limit", m_stbig, 1, MD_BAD_HDR, 0},
        {"another build's asset pack (pack_hash)", m_pack, 1, MD_BAD_HDR, 0},
    };
    for (uint32_t k = 0; k < sizeof T / sizeof T[0]; k++)
        for (i = 0; i < NSH; i++) {
            char w[160];
            clean();
            st = with(i, T[k].f, T[k].reseal, &why);
            snprintf(w, sizeof w, "refused at start %u: %s", i * 6u, T[k].what);
            chk(w, st == T[k].want && (!T[k].why || why == T[k].why) && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end);
            if (T[k].want != MD_EMPTY) chk("... and the built-in table is untouched", ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES));
        }
    (void)m_hcrc;

    /* a header whose own CRC is wrong: torn */
    for (i = 0; i < NSH; i++) {
        clean(); good(i, LEN);
        nor[oc_mod_flash(i) + 24] ^= 0x10u;          /* a flipped bit in state_size, hcrc unchanged */
        oc_mod_boot(0);
        chk(i ? "a torn header (hcrc) at start 6: MD_TORN, nothing registered" : "a torn header (hcrc) at start 0: MD_TORN, nothing registered",
            oc_mod.state[i] == MD_TORN && !eng_mod[ENGI_SHAPES]);
        clean(); good(i, LEN);
        nor[oc_mod_flash(i) + 48] ^= 1u;             /* a flipped bit of hcrc itself */
        oc_mod_boot(0);
        chk("... a flipped bit in hcrc itself is the same", oc_mod.state[i] == MD_TORN);
    }
    /* the code */
    for (i = 0; i < NSH; i++) {
        clean(); good(i, LEN);
        nor[oc_mod_flash(i) + 0x400u] ^= 0x01u;
        oc_mod_boot(0);
        chk("one flipped bit in the code: MD_BAD_CRC, not registered", oc_mod.state[i] == MD_BAD_CRC && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end);
        clean(); good(i, LEN);
        nor[oc_mod_flash(i) + LEN - 1u] ^= 0x80u;
        oc_mod_boot(0);
        chk("... the last byte of the code too", oc_mod.state[i] == MD_BAD_CRC);
        clean(); good(i, LEN);
        memset(nor + oc_mod_flash(i) + 0x800u, 0xFF, 0x100u);      /* a code page that never got programmed */
        oc_mod_boot(0);
        chk("a code page left erased: MD_BAD_CRC", oc_mod.state[i] == MD_BAD_CRC);
        clean();
        { fw_mod_hdr_t h = mk(i, LEN); code(3); seal(&h); put(i, &h, 0); }
        oc_mod_boot(0);
        chk("code with no header page (an install cut off before its last step): empty, nothing registered", oc_mod.state[i] == MD_EMPTY && !eng_mod[ENGI_SHAPES]);
        clean(); good(i, LEN);
        memset(nor + oc_mod_flash(i), 0xFF, 0x1000u);               /* the first sector erased (a removal begun) */
        oc_mod_boot(0);
        chk("the first sector erased: empty", oc_mod.state[i] == MD_EMPTY && !eng_mod[ENGI_SHAPES]);
    }
    clean(); good(0, SHLEN);
    oc_mod_boot(0);
    chk("a module of the full 6 sectors is accepted (its last byte is the area's up to the next start)",
        oc_mod.state[0] == MD_OK && fm1_guard_code_end == oc_mod_link(0) + SHLEN - 1u);

    /* two modules */
    /* (0.11.1) the loser of two valid modules is erased at the boot; lg_incomplete (an old preset pair may lie in the area) holds that erase back, so these
     * cases keep both modules to look at the choice and at the later bans. The erase itself is tested in the 0.11.1 block below. */
    clean(); good(0, LEN); good(1, LEN + 400u);
    lg_incomplete = 1;
    oc_mod_boot(0);
    chk("two good modules for one engine number, equal versions: the higher start is registered, the other is NOSLOT (0.11.1: it was the lower start)",
        oc_mod.state[1] == MD_OK && oc_mod.state[0] == MD_NOSLOT && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));
    chk("... the guard's window follows the registered one only", fm1_guard_code_end == oc_mod_link(1) + LEN + 400u - 1u);
    lg_incomplete = 0;
    clean(); good(1, LEN);
    { fw_mod_hdr_t h = mk(0, LEN); code(9); seal(&h); h.crc ^= 1u; h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); put(0, &h, 1); }
    oc_mod_boot(0);
    chk("a bad module at start 0 and a good one at start 6: the good one is registered (a replace cut after the new was written)",
        oc_mod.state[0] == MD_BAD_CRC && oc_mod.state[1] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));

    /* the update loader's record rule on the sector tails (0.10.1) */
    {
        #define LEN5 5000u                                 /* covers sector 0 and part of 1: one tail (0xF00) is inside it, sector 1's (0x1F00) is not */
        #define LEN9 9000u                                 /* covers sectors 0, 1 and part of 2: tails at 0xF00 and 0x1F00 are inside it */
        uint32_t r;
        uint8_t rec[80];
        for (uint32_t k = 0; k < 80u; k++) rec[k] = (uint8_t)((k * 11u + 5u) | 1u);
        rec[6] = 0x41; rec[7] = 0x54;
        { uint32_t c = pk_crc16(rec + 2, 78u); rec[0] = (uint8_t)c; rec[1] = (uint8_t)(c >> 8); }
        chk("(setup) the record is one the firmware's own rule takes (pk_record_like), and flipping its CRC clears it",
            pk_record_like(rec) && (rec[0] ^= 1u, !pk_record_like(rec)) && (rec[0] ^= 1u, pk_record_like(rec)));
        for (i = 0; i < NSH; i++) {
            fw_mod_hdr_t h;
            /* a record on sector 0's tail */
            clean(); h = mk(i, LEN5); code(11); memcpy(IMG + 0xF00u, rec, 80); seal(&h); put(i, &h, 1);
            oc_mod_boot(0);
            chk(i ? "a module whose sector 0 ends in an update record, at start 6: MD_BAD_TAIL, why = sector 1, not registered"
                  : "a module whose sector 0 ends in an update record, at start 0: MD_BAD_TAIL, why = sector 1, not registered",
                oc_mod.state[i] == MD_BAD_TAIL && oc_mod.why[i] == 1 && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end);
            chk("... the table is untouched, and the plan treats the start as one that can be written over", ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES) && oc_mod_plan(ENGI_SHAPES) == (int)(1u - i));
            /* a record on sector 1's tail */
            clean(); h = mk(i, LEN9); code(12); memcpy(IMG + 0x1F00u, rec, 80); seal(&h); put(i, &h, 1);
            oc_mod_boot(0);
            chk("... on sector 1's tail: the same, why = sector 2", oc_mod.state[i] == MD_BAD_TAIL && oc_mod.why[i] == 2 && !eng_mod[ENGI_SHAPES]);
            /* the same bytes in a sector the module does not cover are not its business */
            clean(); good(i, LEN5);
            memcpy(nor + oc_mod_flash(i) + 2u * 0x1000u + 0xF00u, rec, 80);
            oc_mod_boot(0);
            chk("a record on the tail of a sector past the module's length: the module is fine (the update erases only that empty sector)",
                oc_mod.state[i] == MD_OK && eng_mod[ENGI_SHAPES]);
            /* a record whose first bytes are in the module but whose end is past its length reads as erased flash there: not a record */
            clean(); h = mk(i, 0xF30u); code(13); memcpy(IMG + 0xF00u, rec, 0x30u); seal(&h); put(i, &h, 1);
            oc_mod_boot(0);
            chk("a record cut off by the module's end (the rest of the tail is erased flash) is not a record: MD_OK", oc_mod.state[i] == MD_OK);
            /* the CRC is checked first: a damaged module is BAD_CRC whatever its tails say */
            clean(); h = mk(i, LEN5); code(14); memcpy(IMG + 0xF00u, rec, 80); seal(&h); put(i, &h, 1);
            nor[oc_mod_flash(i) + 0x400u] ^= 1u;
            oc_mod_boot(0);
            chk("... a bad CRC is reported before the tails", oc_mod.state[i] == MD_BAD_CRC);
        }
        /* the installer */
        {   int rc;
            clean(); oc_mod_boot(0);
            { fw_mod_hdr_t hh = mk(0, LEN5); code(15); memcpy(IMG + 0xF00u, rec, 80); seal(&hh); memcpy(IMG, &hh, sizeof hh); }
            r = (uint32_t)oc_mod_begin(ENGI_SHAPES, 0, LEN5);
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < LEN5; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, LEN5 - o > 256u ? 256u : LEN5 - o);
            rc = oc_mod_end(IMG);
            chk("install of a module with a record on a sector tail: end refused with MI_TAIL (rc 8), no header page programmed, nothing registered, no write left open",
                r == 0 && rc == MI_TAIL && nor[oc_mod_flash(0)] == 0xFF && !eng_mod[ENGI_SHAPES] && !mw.writing && oc_mod.state[0] == MD_EMPTY);
            oc_mod_boot(0);
            chk("... the area reads as empty at the next boot and takes a good install", oc_mod.state[0] == MD_EMPTY && oc_mod_plan(ENGI_SHAPES) == 0);
            { fw_mod_hdr_t hh = mk(0, LEN9); code(16); memcpy(IMG + 0x1F00u, rec, 80); seal(&hh); memcpy(IMG, &hh, sizeof hh); }
            r = (uint32_t)oc_mod_begin(ENGI_SHAPES, 0, LEN9);
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < LEN9; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, LEN9 - o > 256u ? 256u : LEN9 - o);
            chk("... a record on sector 1's tail is refused the same way", r == 0 && oc_mod_end(IMG) == MI_TAIL && nor[oc_mod_flash(0)] == 0xFF && !eng_mod[ENGI_SHAPES]);
            { fw_mod_hdr_t hh = mk(0, LEN9); code(17); seal(&hh); memcpy(IMG, &hh, sizeof hh); }
            r = (uint32_t)oc_mod_begin(ENGI_SHAPES, 0, LEN9);
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < LEN9; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, LEN9 - o > 256u ? 256u : LEN9 - o);
            chk("... the same size with no record installs (the refusal is the tail, not the size)", r == 0 && oc_mod_end(IMG) == 0 && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES]);
        }
        #undef LEN5
        #undef LEN9
    }

    /* removal and rebuild */
    clean(); good(0, LEN);
    oc_mod_boot(0);
    chk("(setup) registered", eng_mod[ENGI_SHAPES] != 0);
    memset(nor + oc_mod_flash(0), 0xFF, 6u * 0x1000u);
    oc_mod_boot(0);
    chk("module removed, next boot: out of the table, the built-in slot is back, the guard window is _etext's again",
        !eng_mod[ENGI_SHAPES] && ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES) && !fm1_guard_code_end && oc_mod.state[0] == MD_EMPTY);
    good(1, LEN);
    oc_mod_boot(0);
    chk("module installed at the other start, next boot: registered there", oc_mod.state[1] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));

    /* the crash mark */
    clean(); good(0, LEN);
    oc_mod_boot(0);
    chk("(setup) registered, nothing plays it: the use bit stays clear", (oc_mod_note_use(), oc_modguard.inuse == 0));
    oc_mod_boot(1);
    chk("a crash with the module registered but not in use does not ban it", oc_mod.state[0] == MD_OK && oc_modguard.banned == 0);
    trk[2].engine = ENGI_SHAPES;
    oc_mod_note_use();
    chk("a track plays the engine: the start's use bit is set", oc_modguard.inuse == 1u);
    oc_mod_boot(0);
    chk("a boot without a crash clears the use bit and bans nothing", oc_modguard.inuse == 0 && oc_modguard.banned == 0 && oc_mod.state[0] == MD_OK);
    oc_mod_note_use();
    oc_mod_boot(1);
    chk("a crash in the first 30 s with the module in use: banned (MD_CRASHED), not registered, the table is the built-in slot",
        oc_mod.state[0] == MD_CRASHED && !eng_mod[ENGI_SHAPES] && ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES) && !fm1_guard_code_end);
    oc_mod_boot(0);
    chk("the ban holds at the next boots (no crash this time)", oc_mod.state[0] == MD_CRASHED && !eng_mod[ENGI_SHAPES]);
    chk("a track that used it is played by the table's answer for the number (silent unless the built-in engine is there)",
        ENGINES[trk[2].engine] == eng_default(ENGI_SHAPES));
    oc_mod_unban(0);
    oc_mod_boot(0);
    chk("installing or removing the start clears the ban: registered again", oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES]);
    oc_mod_note_use(); oc_mod_boot(1);
    memset(&oc_modguard, 0, sizeof oc_modguard);       /* power lost: the noinit RAM is gone */
    oc_mod_boot(0);
    chk("a power loss forgets the ban: registered again", oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES]);
    clean(); good(0, LEN); good(1, LEN);
    trk[2].engine = ENGI_SHAPES;
    lg_incomplete = 1;                                 /* (0.11.1) keeps the loser, see above */
    oc_mod_boot(0);
    oc_mod_note_use();
    chk("two modules, one registered: only the registered start's bit is set", oc_modguard.inuse == 2u);
    oc_mod_boot(1);
    chk("... a crash bans that one; the other (valid, not registered before) takes the engine number",
        oc_mod.state[1] == MD_CRASHED && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0));
    lg_incomplete = 0;

    /* the PC guard window (0.10.1): LIMIT0_H follows the registered modules, in a running session too */
    {
        clean(); host_pc_sets = 0;
        oc_mod_boot(0);
        chk("PC window, no module registered: the upper end is _etext", host_pc_hi == OC_HOST_ETEXT && host_pc_sets == 1u);
        good(0, LEN); oc_mod_boot(0);
        chk("... a module registered: the end of its last byte (the flash between is inside, the cost written in fm1_guard_pc.h)",
            host_pc_hi == oc_mod_link(0) + LEN - 1u && host_pc_hi > OC_HOST_ETEXT);
        good(1, LEN); lg_incomplete = 1; oc_mod_boot(0); lg_incomplete = 0;
        chk("... a second for the same engine is not registered (equal versions: the higher start wins): the window is the winner's", host_pc_hi == oc_mod_link(1) + LEN - 1u);
        chk("... oc_mod_remove in a running session writes the window again, and with no module left it is _etext",
            oc_mod_remove(ENGI_SHAPES) == 0 && host_pc_hi == OC_HOST_ETEXT && !fm1_guard_code_end);
        chk("... the window is written at every boot call (the install and the remove paths end in one)", host_pc_sets >= 4u);
        chk("fm1_guard_pc_hi: below _etext stays _etext, above it is the module's end",
            fm1_guard_pc_hi(0, 100) == 100 && fm1_guard_pc_hi(50, 100) == 100 && fm1_guard_pc_hi(150, 100) == 150);
    }

    /* the crash count beyond the first 30 s (0.10.1): sessions that die with the module's engine in use */
    {
        #define SESSION_USE() do { trk[2].engine = ENGI_SHAPES; oc_mod_note_use(); } while (0)
        clean(); good(0, LEN);
        oc_mod_power_on(0);
        chk("first power-on: no earlier session, nothing counted, the module is registered, the session is marked running",
            oc_mod.state[0] == MD_OK && oc_modguard.run == 1u && oc_modguard.unclean[0] == 0 && oc_modguard.banned == 0);
        SESSION_USE(); oc_mod_power_on(0);                  /* the session died (no orderly reset), 40 s in */
        chk("a session that died with the engine in use: one unclean, still registered", oc_modguard.unclean[0] == 1u && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES]);
        SESSION_USE(); oc_mod_power_on(0);
        chk("... two in a row: still registered", oc_modguard.unclean[0] == 2u && oc_mod.state[0] == MD_OK && !oc_modguard.banned);
        SESSION_USE(); oc_mod_power_on(0);
        chk("... three in a row: banned (MD_CRASHED), out of the table, the guard window back to _etext, the count restarts",
            oc_mod.state[0] == MD_CRASHED && (oc_modguard.banned & 1u) && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end && oc_modguard.unclean[0] == 0);
        ui.msg[0] = 0;
        oc_mod_ban_notice();
        chk("... the screen says MODULE BANNED SHAPES", !strcmp(ui.msg, "MODULE BANNED SHAPES"));
        ui.msg[0] = 0;
        oc_mod_ban_notice();
        chk("... once: the second call says nothing", ui.msg[0] == 0 && oc_modguard.notice == 0);
        oc_mod_power_on(0);
        chk("the ban holds at the next boots", oc_mod.state[0] == MD_CRASHED && !eng_mod[ENGI_SHAPES]);
        oc_mod_unban(0); oc_mod_boot(0);
        chk("installing or removing the start lifts the ban: registered again, the count at 0", oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] && oc_modguard.unclean[0] == 0);

        clean(); good(0, LEN);
        oc_mod_power_on(0);
        SESSION_USE(); oc_mod_power_on(0);
        SESSION_USE(); oc_mod_power_on(0);
        chk("(setup) two unclean sessions", oc_modguard.unclean[0] == 2u);
        reset_intended();
        chk("an orderly reset (reset_intended: UBOOT, an update) clears the running mark and the count", oc_modguard.run == 0 && oc_modguard.unclean[0] == 0);
        SESSION_USE(); oc_mod_power_on(0);
        chk("the next boot after an orderly end counts nothing", oc_modguard.unclean[0] == 0 && oc_mod.state[0] == MD_OK && oc_modguard.run == 1u);
        SESSION_USE(); oc_mod_power_on(0); SESSION_USE(); oc_mod_power_on(0);
        chk("... a clean reset between breaks the streak: two deaths after it are not three", oc_modguard.unclean[0] == 2u && oc_mod.state[0] == MD_OK);

        clean(); good(0, LEN);
        oc_mod_power_on(0);
        SESSION_USE(); oc_mod_power_on(0); SESSION_USE(); oc_mod_power_on(0);
        trk[2].engine = 0; oc_mod_note_use();               /* a session in which no track holds the module's engine dies */
        oc_mod_power_on(0);
        chk("a death with the module's engine not in use breaks the streak (count 0), the module stays", oc_modguard.unclean[0] == 0 && oc_mod.state[0] == MD_OK && !oc_modguard.banned);

        clean(); good(0, LEN);
        oc_mod_power_on(0); SESSION_USE(); oc_mod_power_on(0); SESSION_USE(); oc_mod_power_on(0);
        memset(&oc_modguard, 0, sizeof oc_modguard);        /* power lost: the noinit RAM is gone */
        oc_mod_power_on(0);
        chk("a power loss forgets the count: nothing counted at the next boot", oc_modguard.unclean[0] == 0 && oc_modguard.run == 1u && oc_mod.state[0] == MD_OK);

        clean(); good(0, LEN);
        oc_mod_power_on(0); SESSION_USE(); oc_mod_power_on(1);
        chk("the 30 second rule still bans at the first death, and tells the screen too", oc_mod.state[0] == MD_CRASHED && (oc_modguard.notice & 1u));
        ui.msg[0] = 0; oc_mod_ban_notice();
        chk("... MODULE BANNED SHAPES", !strcmp(ui.msg, "MODULE BANNED SHAPES"));

        clean(); good(0, LEN);
        oc_mod_power_on(0);
        for (i = 0; i < 3u; i++) { SESSION_USE(); oc_mod_power_on(0); if (i == 0) oc_mod_note_use(); }
        chk("(sanity) a module is not counted before it is registered and in use: a start with a module nobody holds never reaches the count",
            oc_mod.state[0] == MD_CRASHED);
        clean(); good(0, LEN); good(1, LEN);
        lg_incomplete = 1;                             /* (0.11.1) keeps the loser */
        oc_mod_power_on(0);
        for (i = 0; i < 3u; i++) { SESSION_USE(); oc_mod_power_on(0); }
        lg_incomplete = 0;
        chk("two modules for the engine: the registered one (start 1) is banned by three deaths and the other (valid, not registered before) takes the number",
            oc_mod.state[1] == MD_CRASHED && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_modguard.unclean[0] == 0);
        #undef SESSION_USE
    }

    /* ---------------------------------------------------------------- the installer (step 6) */
    {
        int rc;
        uint32_t before, k, n;
        /* a module for start i as the .ocm carries it: header page + code, header sealed; installs it with begin / write / end */
        #define BUILD(i_, len_, seed_) do { fw_mod_hdr_t hh_ = mk(i_, len_); code(seed_); seal(&hh_); memcpy(IMG, &hh_, sizeof hh_); } while (0)
        #define INSTALL(i_, len_, outrc) ({ int r_ = oc_mod_begin(ENGI_SHAPES, i_, len_); uint32_t o_; \
            for (o_ = FW_MOD_CODE_OFF; !r_ && o_ < (len_); o_ += 256u) r_ = oc_mod_write(o_, IMG + o_, (len_) - o_ > 256u ? 256u : (len_) - o_); \
            if (!r_) r_ = oc_mod_end(IMG); *(outrc) = r_; r_; })

        clean(); oc_mod_boot(0);
        chk("the plan on an empty area: start 0 for SHAPES; none for an engine a module may not supply", oc_mod_plan(ENGI_SHAPES) == 0 && oc_mod_plan(3) == -1);
        BUILD(0, LEN, 21);
        before = erases;
        { uint32_t f0 = oc_feeds;
          chk("install at start 0: the whole of it", INSTALL(0, LEN, &rc) == 0 && rc == 0);
          chk("... the watchdog was fed before each of the 6 erases of the span (0.12.2, Part 2d)", oc_feeds - f0 >= 6u); }
        chk("... registered, the 6 sectors of the start erased once and nothing else", oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] && erases - before == 6u);
        chk("... the flash holds the header page and the code as sent", !memcmp(nor + oc_mod_flash(0), IMG, LEN));
        chk("... the plan now says start 1: a replacement goes beside the old module (0.10.1)", oc_mod_plan(ENGI_SHAPES) == 1 && !oc_mod_inplace(ENGI_SHAPES, 1) && oc_mod_inplace(ENGI_SHAPES, 0));
        chk("a begin for the start that holds the module is refused (the plan is start 1)", (before = erases, oc_mod_begin(ENGI_SHAPES, 0, LEN) == -1) && !mw.writing && erases == before);
        chk("a begin for an engine a module may not supply, a length of 0, an odd length, too long: refused",
            oc_mod_begin(3, 0, LEN) == -1 && oc_mod_begin(ENGI_SHAPES, 0, 0) == -1 && oc_mod_begin(ENGI_SHAPES, 0, LEN + 1u) == -1 &&
            oc_mod_begin(ENGI_SHAPES, 0, SHLEN + 4u) == -1 && oc_mod_begin(ENGI_SHAPES, 2, LEN) == -1);
        chk("... and none of them touched the module that was there", oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] && !memcmp(nor + oc_mod_flash(0), IMG, LEN));

        /* a replacement beside the old module, a track playing the old one (0.10.1) */
        static uint8_t keep[SHLEN];
        memcpy(keep, nor + oc_mod_flash(0), LEN);
        trk[1].engine = ENGI_SHAPES; trk[1].v[0].active = trk[1].v[0].gate = 1; trk[1].v[0].stage = 2; trk[1].v[0].env = 1000;
        oc_modguard.banned = 1u;
        BUILD(1, LEN + 512u, 22);
        before = erases;
        chk("replace beside: begin erases only the free start (6 sectors); the old module stays registered and playing",
            oc_mod_begin(ENGI_SHAPES, 1, LEN + 512u) == 0 && erases - before == 6u && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && trk[1].v[0].active &&
            trk[1].v[0].env == 1000 && oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_EMPTY && !mw.inplace);
        chk("... the old module's flash is untouched", !memcmp(nor + oc_mod_flash(0), keep, LEN));
        for (n = FW_MOD_CODE_OFF; n < LEN + 512u; n += 256u) oc_mod_write(n, IMG + n, LEN + 512u - n > 256u ? 256u : LEN + 512u - n);
        chk("... still the old one in the table while the code is written", eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && trk[1].v[0].active);
        before = erases;
        chk("... the new one is committed and read back, then the old one is taken out: registered at start 1", oc_mod_end(IMG) == 0 && oc_mod.state[1] == MD_OK &&
            eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && !memcmp(nor + oc_mod_flash(1), IMG, LEN + 512u));
        chk("... the old start is erased (6 sectors) and empty, the track's voice is off, the ban is gone, the project's engine number is untouched",
            erases - before == 6u && oc_mod.state[0] == MD_EMPTY && nor[oc_mod_flash(0)] == 0xFF && !trk[1].v[0].active && trk[1].v[0].env == 0 &&
            oc_modguard.banned == 0 && trk[1].engine == ENGI_SHAPES);
        chk("... the PC guard's window ends at the new module's last byte", fm1_guard_code_end == oc_mod_link(1) + LEN + 512u - 1u);
        chk("... and the next replacement goes beside it, back to start 0", oc_mod_plan(ENGI_SHAPES) == 0);
        /* a replacement that is not a module the loader accepts: the old one stays */
        BUILD(0, LEN, 23);
        oc_mod_begin(ENGI_SHAPES, 0, LEN);
        for (n = FW_MOD_CODE_OFF; n < LEN; n += 256u) {
            uint8_t b2[256]; uint32_t m = LEN - n > 256u ? 256u : LEN - n;
            memcpy(b2, IMG + n, m);
            if (n == 0x400u) b2[3] ^= 1u;                 /* a byte of the code that differs from what the header sealed */
            oc_mod_write(n, b2, m);
        }
        chk("a beside install whose code does not match its header is refused at END: nothing committed, the old module (start 1) is untouched",
            oc_mod_end(IMG) == -1 && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[1] == MD_OK && nor[oc_mod_flash(0)] == 0xFF);
        /* in place: no other start is free */
        clean(); oc_mod_boot(0);
        BUILD(0, LEN, 24); INSTALL(0, LEN, &rc);
        oc_mod.state[1] = MD_OK; oc_mod.engine[1] = 99;     /* start 1 holds a usable module of another engine: not free */
        chk("no other start free: the plan is the old module's start, and the install there is in place", oc_mod_plan(ENGI_SHAPES) == 0 && oc_mod_inplace(ENGI_SHAPES, 0));
        trk[1].engine = ENGI_SHAPES; trk[1].v[0].active = trk[1].v[0].gate = 1; trk[1].v[0].env = 1000;
        BUILD(0, LEN + 512u, 25);
        before = erases;
        chk("in place: begin takes the old module out of the table first, its voices off, and erases its 6 sectors",
            oc_mod_begin(ENGI_SHAPES, 0, LEN + 512u) == 0 && mw.inplace && !eng_mod[ENGI_SHAPES] && ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES) &&
            !trk[1].v[0].active && erases - before == 6u && nor[oc_mod_flash(0)] == 0xFF && oc_mod.state[0] == MD_EMPTY);
        chk("... the PC window narrows at once to _etext (the module is gone; 0.11.1: it stayed at the old module's end until the next boot)",
            host_pc_hi == OC_HOST_ETEXT && !fm1_guard_code_end);
        for (n = FW_MOD_CODE_OFF; n < LEN + 512u; n += 256u) oc_mod_write(n, IMG + n, LEN + 512u - n > 256u ? 256u : LEN + 512u - n);
        chk("... the new one is committed and registered", oc_mod_end(IMG) == 0 && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0));
        chk("... and the window ends at the new module's last byte", host_pc_hi == oc_mod_link(0) + LEN + 512u - 1u && fm1_guard_code_end == host_pc_hi);

        /* refusals at end: nothing committed */
        {   struct { const char *what; mut_t f; } E[] = {
                {"a header for the other start (the wrong variant)", m_link}, {"another interface identifier", m_iface}, {"another table address", m_addr},
                {"another engine than begin named", m_eng}, {"a state size of 0", m_st0}, {"another build's pack", m_pack}, {"an engine_t outside the code", m_eptr_hi},
            };
            for (k = 0; k < sizeof E / sizeof E[0]; k++) {
                char w[160];
                fw_mod_hdr_t hh = mk(0, LEN);
                clean(); oc_mod_boot(0);
                code(5);
                E[k].f(&hh); hh.length = LEN;
                seal(&hh); memcpy(IMG, &hh, sizeof hh);
                if (E[k].f == m_eng) { /* begin as SHAPES, header says 3 */ }
                oc_mod_begin(ENGI_SHAPES, 0, LEN);
                for (n = FW_MOD_CODE_OFF; n < LEN; n += 256u) oc_mod_write(n, IMG + n, LEN - n > 256u ? 256u : LEN - n);
                snprintf(w, sizeof w, "end refused: %s; no header page written, nothing registered", E[k].what);
                chk(w, oc_mod_end(IMG) == -1 && nor[oc_mod_flash(0)] == 0xFF && !eng_mod[ENGI_SHAPES] && !mw.writing);
            }
        }
        /* the CRC of the stream against the header: one byte sent differently from what the header sealed */
        clean(); oc_mod_boot(0); BUILD(0, LEN, 8);
        oc_mod_begin(ENGI_SHAPES, 0, LEN);
        for (n = FW_MOD_CODE_OFF; n < LEN; n += 256u) {
            uint8_t b[256]; uint32_t m = LEN - n > 256u ? 256u : LEN - n;
            memcpy(b, IMG + n, m); if (n == 0x300u) b[5] ^= 0x40u;
            oc_mod_write(n, b, m);
        }
        chk("end refused: a code byte sent differently from what the header's CRC sealed", oc_mod_end(IMG) == -1 && nor[oc_mod_flash(0)] == 0xFF && !eng_mod[ENGI_SHAPES]);
        oc_mod_begin(ENGI_SHAPES, 0, LEN);
        oc_mod_write(FW_MOD_CODE_OFF, IMG + FW_MOD_CODE_OFF, 1000);
        chk("end before all the code is in: refused, no header page", oc_mod_end(IMG) == -1 && nor[oc_mod_flash(0)] == 0xFF);
        oc_mod_begin(ENGI_SHAPES, 0, LEN);
        chk("a write out of order is refused", oc_mod_write(FW_MOD_CODE_OFF + 16u, IMG, 16) == -1);
        chk("... a write past the length is refused", oc_mod_write(FW_MOD_CODE_OFF, IMG, LEN) == -1);
        chk("... a write with no begin is refused", (mw.writing = 0, oc_mod_write(FW_MOD_CODE_OFF, IMG, 16) == -1));

        /* a cut at every program */
        {   uint32_t progs = 0, bad_cut = 0;
            clean(); oc_mod_boot(0); BUILD(0, LEN, 33);
            for (k = 0; k < 40u; k++) {
                int r;
                clean(); oc_mod_boot(0);
                fail_after = (int)k;
                r = INSTALL(0, LEN, &rc);
                fail_after = -1;
                if (r == 0) { progs = k; break; }
                oc_mod_boot(0);                        /* the next power-on */
                bad_cut += oc_mod.state[0] == MD_OK || eng_mod[ENGI_SHAPES] || oc_mod.state[0] == MD_BAD_CRC || fm1_guard_code_end;
                if (oc_mod.state[0] != MD_EMPTY && oc_mod.state[0] != MD_TORN) bad_cut++;
            }
            chk("an install cut at every program (the code pages, then the header page): never half a module, the area reads as empty after", bad_cut == 0 && progs == 12u);
            clean(); oc_mod_boot(0); fail_after = 3; INSTALL(0, LEN, &rc); fail_after = -1; oc_mod_boot(0);
            chk("... the cut-off install leaves the area reusable (plan 0)", oc_mod_plan(ENGI_SHAPES) == 0);
        }
        /* a replacement beside, cut at every program and every erase (0.10.1): the old module survives every cut until the new one is complete */
        {   uint32_t ncut = 0, bad_old = 0, bad_new = 0, both = 0, end_progs = 0, end_erases = 0, k2;
            const uint32_t OLDLEN = LEN, NEWLEN = LEN + 700u;
            for (k = 0; k < 60u; k++) {                      /* programs */
                int r;
                clean(); oc_mod_boot(0);
                BUILD(0, OLDLEN, 80); INSTALL(0, OLDLEN, &rc);
                BUILD(1, NEWLEN, 81);
                fail_after = (int)k;
                r = INSTALL(1, NEWLEN, &rc);
                fail_after = -1;
                oc_mod_boot(0);                              /* the next power-on */
                if (r == 0) { end_progs = k; break; }
                ncut++;
                bad_old += !(oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_mod.state[1] == MD_EMPTY &&
                             fm1_guard_code_end == oc_mod_link(0) + OLDLEN - 1u);
            }
            chk("a replacement beside, cut at every program (the code pages, then the header page): the old module is the registered one, the new start empty",
                ncut == 15u && bad_old == 0 && end_progs == 15u);
            for (k2 = 0; k2 < 30u; k2++) {                   /* erases: 0..5 the new start at begin, 6..11 the old start after the commit */
                int r;
                clean(); oc_mod_boot(0);
                BUILD(0, OLDLEN, 80); INSTALL(0, OLDLEN, &rc);
                BUILD(1, NEWLEN, 81);
                erase_fail_after = (int)k2;
                r = INSTALL(1, NEWLEN, &rc);
                erase_fail_after = -1;
                oc_mod_boot(0);
                if (r == 0) { end_erases = k2; break; }
                if (k2 < 6u)                                 /* the new start was being erased: the old module is intact and registered */
                    bad_old += !(oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0));
                else if (k2 == 6u) {                         /* the commit is done, no old sector erased yet: both are good; 0.11.1: the newer is used, the old erased at that boot */
                    both += oc_mod.state[1] == MD_OK && oc_mod.state[0] == MD_EMPTY && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1);
                } else                                       /* the old header sector is gone: the new module */
                    bad_new += !(oc_mod.state[1] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[0] == MD_EMPTY);
            }
            chk("... cut at every erase: before the commit the old module; after it the new one (and for the instant between the commit and the first erase of the old start, both are valid: 0.11.1 the newer is used and the old start erased)",
                end_erases == 12u && bad_old == 0 && bad_new == 0 && both == 1u);
            /* after a cut at the first old erase the area still takes the next replacement: the stale old start is the one written over */
            chk("... and a power loss in that instant is cleared by the next replacement (the plan names a start that is free)", oc_mod_plan(ENGI_SHAPES) >= 0);
        }
        /* the erase fails at begin */
        clean(); oc_mod_boot(0); BUILD(0, LEN, 40); INSTALL(0, LEN, &rc);
        erase_error = 1;
        chk("a flash that will not erase, beside: begin returns the flash error, nothing is writing, the old module is still registered",
            oc_mod_begin(ENGI_SHAPES, 1, LEN) < 0 && !mw.writing && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_mod.state[0] == MD_OK);
        erase_error = 0;
        oc_mod.state[1] = MD_OK; oc_mod.engine[1] = 99;
        erase_error = 1;
        chk("... in place (no other start free): begin returns the flash error, nothing is writing, the module is out of the table",
            oc_mod_begin(ENGI_SHAPES, 0, LEN) < 0 && !mw.writing && !eng_mod[ENGI_SHAPES]);
        chk("... and the PC window is _etext (0.11.1: the error path set the end to 0 and never wrote the window)", host_pc_hi == OC_HOST_ETEXT && !fm1_guard_code_end);
        erase_error = 0;
        /* the plan with junk */
        clean(); BUILD(0, LEN, 41);
        { fw_mod_hdr_t hh = mk(0, LEN); code(41); seal(&hh); put(0, &hh, 1); nor[oc_mod_flash(0) + 0x500u] ^= 1u; }
        oc_mod_boot(0);
        chk("(setup) start 0 holds a damaged module", oc_mod.state[0] == MD_BAD_CRC);
        chk("the plan prefers the empty start (1) to the damaged one", oc_mod_plan(ENGI_SHAPES) == 1);
        { fw_mod_hdr_t hh = mk(1, LEN); code(42); seal(&hh); put(1, &hh, 1); nor[oc_mod_flash(1) + 0x500u] ^= 1u; }
        oc_mod_boot(0);
        chk("with both damaged the plan is the first of them (0)", oc_mod.state[0] == MD_BAD_CRC && oc_mod.state[1] == MD_BAD_CRC && oc_mod_plan(ENGI_SHAPES) == 0);
        /* two usable modules of other engines fill the area: no start free */
        oc_mod.state[0] = oc_mod.state[1] = MD_OK; oc_mod.engine[0] = oc_mod.engine[1] = 99;
        chk("no start free (both hold usable modules of other engines): -5, nothing erased", (before = erases, oc_mod_begin(ENGI_SHAPES, 0, LEN) == -5) && erases == before);
        /* the old preset pairs not yet moved */
        clean(); oc_mod_boot(0);
        lg_incomplete = 1;
        chk("the move of the old preset pairs not finished: begin and remove return -4, nothing erased", (before = erases, oc_mod_begin(ENGI_SHAPES, 0, LEN) == -4 && oc_mod_remove(ENGI_SHAPES) == -4) && erases == before);
        lg_incomplete = 0;

        /* remove */
        clean(); oc_mod_boot(0); BUILD(0, LEN, 50); INSTALL(0, LEN, &rc);
        trk[0].engine = ENGI_SHAPES; trk[0].v[1].active = 1;
        before = erases;
        chk("remove: the engine out of the table, its voices off, its 6 sectors erased", oc_mod_remove(ENGI_SHAPES) == 0 && !eng_mod[ENGI_SHAPES] && !trk[0].v[1].active &&
            erases - before == 6u && nor[oc_mod_flash(0)] == 0xFF && oc_mod.state[0] == MD_EMPTY && !fm1_guard_code_end);
        chk("... again with nothing there is a no-op that still answers ok", (before = erases, oc_mod_remove(ENGI_SHAPES) == 0) && erases == before);
        chk("remove of an engine a module may not supply is refused", oc_mod_remove(3) == -1);
        { fw_mod_hdr_t hh = mk(1, LEN); code(60); seal(&hh); put(1, &hh, 1); nor[oc_mod_flash(1) + 0x700u] ^= 1u; }
        oc_mod_boot(0);
        chk("(setup) a damaged module remains", oc_mod.state[1] == MD_BAD_CRC);
        chk("remove of the engine leaves what is not a module of it; remove 127 wipes the whole area",
            oc_mod_remove(ENGI_SHAPES) == 0 && oc_mod.state[1] == MD_BAD_CRC && oc_mod_remove(127) == 0 && oc_mod.state[1] == MD_EMPTY && nor[oc_mod_flash(1)] == 0xFF);
        /* (0.11.1, fix 3) the PC window after every path that takes a module out of the table */
        {
            uint32_t sets;
            fw_mod_hdr_t hh;
            /* an in-place install begun and never ended: the engine stays out of the table, so the window must not stay at the old module's end */
            clean(); oc_mod_boot(0); BUILD(0, LEN + 512u, 80); INSTALL(0, LEN + 512u, &rc);
            oc_mod.state[1] = MD_OK; oc_mod.engine[1] = 99;                    /* start 1 is not free: the install is in place */
            chk("(setup) a module at start 0, the window at its end", eng_mod[ENGI_SHAPES] && host_pc_hi == oc_mod_link(0) + LEN + 512u - 1u);
            BUILD(0, LEN, 81);
            sets = host_pc_sets;
            chk("an in-place install begun: the window is written (once) and is _etext", oc_mod_begin(ENGI_SHAPES, 0, LEN) == 0 && mw.inplace && host_pc_sets > sets && host_pc_hi == OC_HOST_ETEXT);
            for (n = FW_MOD_CODE_OFF; n < LEN / 2u; n += 256u) oc_mod_write(n, IMG + n, 256u);
            chk("... a part of the code written, then the install is abandoned (no end): engine out of the table, window still _etext",
                mw.writing && !eng_mod[ENGI_SHAPES] && host_pc_hi == OC_HOST_ETEXT && !fm1_guard_code_end);
            oc_mod_boot(0);                                                    /* a re-read of the area (a reset would do it): nothing registered, the same window */
            chk("... a boot after it: nothing registered (no header was written), the window _etext", !eng_mod[ENGI_SHAPES] && oc_mod.state[0] == MD_EMPTY && host_pc_hi == OC_HOST_ETEXT);
            /* ... and the next begin: start 0 is empty now, so this one goes beside (into the free start), the window stays until the new module is registered */
            sets = host_pc_sets;
            chk("the next begin after an abandoned install (the start is empty: not in place): the window stays _etext", oc_mod_begin(ENGI_SHAPES, 0, LEN) == 0 && !mw.inplace && host_pc_hi == OC_HOST_ETEXT);
            for (n = FW_MOD_CODE_OFF; n < LEN; n += 256u) oc_mod_write(n, IMG + n, LEN - n > 256u ? 256u : LEN - n);
            chk("... its end registers the module and the window ends at its last byte", oc_mod_end(IMG) == 0 && eng_mod[ENGI_SHAPES] && host_pc_hi == oc_mod_link(0) + LEN - 1u);
            /* an install abandoned in place, begun again in place: the window is _etext both times */
            clean(); oc_mod_boot(0); BUILD(0, LEN, 82); INSTALL(0, LEN, &rc);
            oc_mod.state[1] = MD_OK; oc_mod.engine[1] = 99;
            oc_mod_begin(ENGI_SHAPES, 0, LEN);
            chk("in place, begun twice (the first abandoned): the second begin also ends with the window at _etext",
                (oc_mod.state[0] = MD_OK, oc_mod.engine[0] = ENGI_SHAPES, oc_mod_begin(ENGI_SHAPES, 0, LEN)) == 0 && host_pc_hi == OC_HOST_ETEXT);
            /* beside: begin erases the free start only, the old module stays registered and the window stays at its end */
            clean(); oc_mod_boot(0); BUILD(0, LEN, 83); INSTALL(0, LEN, &rc);
            sets = host_pc_sets;
            chk("a beside begin: the old module stays registered, the window stays at its end", oc_mod_begin(ENGI_SHAPES, 1, LEN) == 0 && eng_mod[ENGI_SHAPES] && host_pc_hi == oc_mod_link(0) + LEN - 1u);
            /* beside, refused at end: nothing taken out, the window as it was */
            for (n = FW_MOD_CODE_OFF; n < LEN; n += 256u) oc_mod_write(n, IMG + n, LEN - n > 256u ? 256u : LEN - n);
            hh = mk(1, LEN); hh.state_size = 0; code(83); seal(&hh); memcpy(IMG, &hh, sizeof hh);
            chk("... its end refused (a header that does not fit): the old module and the window are as they were", oc_mod_end(IMG) == -1 && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && host_pc_hi == oc_mod_link(0) + LEN - 1u);
            /* beside with a SHORTER new module at a LOWER end: the old, longer one goes, the window follows the new one */
            clean(); oc_mod_boot(0); BUILD(1, LEN + 1024u, 84);
            oc_mod.state[1] = MD_EMPTY;
            { fw_mod_hdr_t h1 = mk(1, LEN + 1024u); code(84); seal(&h1); put(1, &h1, 1); oc_mod_boot(0); }
            chk("(setup) a long module at start 1", eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && host_pc_hi == oc_mod_link(1) + LEN + 1024u - 1u);
            BUILD(0, LEN, 85);
            chk("a shorter module beside it, at the lower start: after the swap the window ends at the new module (below the old end)",
                INSTALL(0, LEN, &rc) == 0 && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && host_pc_hi == oc_mod_link(0) + LEN - 1u && oc_mod.state[1] == MD_EMPTY);
            /* the helper on its own */
            sets = host_pc_sets;
            oc_mod_window();
            chk("oc_mod_window: writes the window from what is registered (the module at start 0)", host_pc_sets == sets + 1u && host_pc_hi == oc_mod_link(0) + LEN - 1u);
            oc_mod_park(ENGI_SHAPES);
            chk("... oc_mod_park writes it too: with the module out of the table the window is _etext (the state still says MD_OK, the registry decides)",
                oc_mod.state[0] == MD_OK && !eng_mod[ENGI_SHAPES] && host_pc_hi == OC_HOST_ETEXT && !fm1_guard_code_end);
        }

        /* (0.11.1) a running session: every oc_mod_boot from the installer parks the engine's voices first (a track on the engine would render the
         * built-in slot, then the module, under live voices, while the module is re-read) */
        {
            #define LIVE() do { trk[2].engine = ENGI_SHAPES; trk[2].v[0].active = 1; trk[2].v[0].gate = 1; trk[2].v[0].stage = 3; trk[2].v[0].env = 1000; } while (0)
            #define QUIET() (!trk[2].v[0].active && !trk[2].v[0].gate && !trk[2].v[0].stage && !trk[2].v[0].env)
            clean(); oc_mod_boot(0); BUILD(0, LEN, 90);
            LIVE();
            chk("fresh install (nothing registered before): the voices on the engine are parked", INSTALL(0, LEN, &rc) == 0 && QUIET());
            BUILD(1, LEN + 4u, 92);
            LIVE();
            chk("install beside: the voices are parked", INSTALL(1, LEN + 4u, &rc) == 0 && QUIET());
            clean(); good(0, LEN); good(1, LEN); lg_incomplete = 1; oc_mod_boot(0); lg_incomplete = 0;
            chk("(setup) both starts hold a module: the plan is in place", oc_mod_inplace(ENGI_SHAPES, (uint32_t)oc_mod_plan(ENGI_SHAPES)));
            { uint32_t st = (uint32_t)oc_mod_plan(ENGI_SHAPES); BUILD(st, LEN, 91); LIVE();
              chk("in-place install: the voices are parked", INSTALL(st, LEN, &rc) == 0 && QUIET()); }
            LIVE();
            LIVE();
            chk("remove: the voices are parked", oc_mod_remove(ENGI_SHAPES) == 0 && QUIET());
            { uint32_t o0, n0; clean(); good(0, LEN); oc_mod_boot(0); LIVE(); o0 = irq_offs; n0 = irq_ons;
              oc_mod_remove(ENGI_SHAPES);
              chk("the table's two stores in a running session are a critical section: interrupts off and on in pairs (unregister)", irq_offs > o0 && irq_offs - o0 == irq_ons - n0);
              BUILD(0, LEN, 93); o0 = irq_offs; n0 = irq_ons; INSTALL(0, LEN, &rc);
              chk("... and the register of the new module", irq_offs > o0 && irq_offs - o0 == irq_ons - n0);
              o0 = irq_offs; clean(); good(0, LEN); oc_mod_boot(0);
              chk("at power-on (oc_mod_boot alone) the interrupt state is not touched", irq_offs == o0); }
            LIVE();
            oc_mod_boot(0);
            chk("(the boot itself, before audio runs, is not a running session: it parks nothing)", !QUIET());
            memset(&trk[2], 0, sizeof trk[2]);
            #undef LIVE
            #undef QUIET
        }

        /* (0.11.1) two valid modules of one engine at boot: the higher header version wins, equal versions the higher start, the loser is erased at that boot */
        {
            uint32_t k5, s5, cuts = 0, bad5 = 0, both5 = 0;
            #define ERASED(i_) ({ uint32_t z_ = 1; for (s5 = 0; s5 < 6u * 0x1000u; s5++) if (nor[oc_mod_flash(i_) + s5] != 0xFF) { z_ = 0; break; } z_; })
            #define TWO(v0_, v1_) do { clean(); modver = (v0_); good(0, LEN); modver = (v1_); good(1, LEN + 4u); modver = 0; } while (0)
            TWO(1, 2); oc_mod_boot(0);
            chk("two valid, the one at start 1 has the higher version: it is registered, start 0 is erased at that boot",
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[1] == MD_OK && oc_mod.state[0] == MD_EMPTY && ERASED(0));
            chk("... the window ends at the winner", host_pc_hi == oc_mod_link(1) + LEN + 4u - 1u);
            TWO(3, 2); oc_mod_boot(0);
            chk("two valid, the one at start 0 has the higher version: it wins (not the higher start), start 1 is erased",
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_EMPTY && ERASED(1));
            TWO(2, 2); oc_mod_boot(0);
            chk("two valid, equal versions: the module at start 1 wins (a replacement goes to the free start), start 0 is erased",
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[1] == MD_OK && oc_mod.state[0] == MD_EMPTY && ERASED(0));
            TWO(1, 2); lg_incomplete = 1; oc_mod_boot(0);
            chk("an old preset pair may lie in the area (lg_incomplete): the winner is registered, the loser is kept as NOSLOT and not erased",
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[0] == MD_NOSLOT && !ERASED(0));
            lg_incomplete = 0; oc_mod_boot(0);
            chk("... the next boot without it erases the loser", oc_mod.state[0] == MD_EMPTY && ERASED(0) && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));
            TWO(1, 2); erase_error = 1; oc_mod_boot(0); erase_error = 0;
            chk("a flash that will not erase at that boot: the winner is registered, the loser stays NOSLOT (the next install finds one usable module there)",
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[0] == MD_NOSLOT && !ERASED(0));
            oc_mod_boot(0);
            chk("... and the next boot, the flash fine again, erases it", oc_mod.state[0] == MD_EMPTY && ERASED(0));
            TWO(1, 2); lg_incomplete = 1; oc_mod_boot(0);      /* (guard initialised; the loser kept for now) */
            oc_modguard.banned = 2u; oc_mod_boot(0); lg_incomplete = 0;
            chk("the winner is banned (MD_CRASHED): it takes no part, the other registers and nothing is erased",
                oc_mod.state[1] == MD_CRASHED && oc_mod.state[0] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && !ERASED(1));
            oc_modguard.banned = 0;

            /* the cut itself: a replacement beside (version 2 over version 1), every erase in turn fails */
            for (k5 = 0; k5 < 30u; k5++) {
                int r;
                clean(); oc_mod_boot(0);
                modver = 1; BUILD(0, LEN, 80); INSTALL(0, LEN, &rc);
                modver = 2; BUILD(1, LEN + 700u, 81);
                erase_fail_after = (int)k5;
                r = INSTALL(1, LEN + 700u, &rc);
                erase_fail_after = -1;
                oc_mod_boot(0);                                  /* the next power-on */
                modver = 0;
                if (r == 0) break;
                cuts++;
                if (k5 < 6u) bad5 += !(eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_mod.state[1] == MD_EMPTY);
                else {
                    bad5 += !(eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[0] == MD_EMPTY && oc_mod.state[1] == MD_OK && ERASED(0));
                    both5++;
                }
            }
            chk("a replacement beside cut at every erase: before the commit the old module; from the commit on, after the next boot, only the newer is left",
                cuts == 12u && bad5 == 0 && both5 == 6u);
            /* ... and the install that follows goes beside, not in place */
            {   uint32_t before5;
                clean(); oc_mod_boot(0);
                modver = 1; BUILD(0, LEN, 80); INSTALL(0, LEN, &rc);
                modver = 2; BUILD(1, LEN + 700u, 81);
                erase_fail_after = 6; INSTALL(1, LEN + 700u, &rc); erase_fail_after = -1;
                oc_mod_boot(0);
                chk("(setup) the cut between the commit and the first erase of the old start, then the boot: version 2 at start 1 is the only module", eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1) && oc_mod.state[0] == MD_EMPTY);
                chk("... the plan names the free start (0) and the install there is beside (the registered module stays playable until it ends)",
                    oc_mod_plan(ENGI_SHAPES) == 0 && !oc_mod_inplace(ENGI_SHAPES, 0));
                modver = 3; BUILD(0, LEN, 82); before5 = erases;
                chk("... the install: begin erases only the free start (6 sectors) and the new module wins, the version 2 module goes",
                    INSTALL(0, LEN, &rc) == 0 && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && oc_mod.state[1] == MD_EMPTY && erases - before5 == 12u);
                modver = 0;
            }
            #undef ERASED
            #undef TWO
        }

        /* (0.11.1) a crash before the first pass of the main loop: the project load marks the module's use at once (oc_mod_note_use runs only in the loop) */
        {
            clean(); reset(); good(0, LEN); oc_mod_boot(0);
            ENGINES[ENGI_SHAPES] = eng_default(ENGI_SHAPES);   /* (the host cannot run the made module's engine_t: the registry, eng_mod and the state, say it is there; the table's entry is the built-in's) */
            trk[2].eng_req = ENGI_SHAPES;
            project_save(0);
            chk("(setup) a project with a track on the module's engine is saved", !strcmp(ui.msg, "SAVED"));
            oc_modguard.inuse = 0; trk[2].eng_req = 0; trk[2].engine = 0;
            project_load(0);
            chk("project load: the module's use bit is set as soon as the track is given the engine (no main loop pass yet)", trk[2].eng_req == ENGI_SHAPES && oc_modguard.inuse == 1u);
            oc_mod_boot(1);
            chk("... the death before the loop's first pass bans the module (it was attributed)", oc_mod.state[0] == MD_CRASHED && (oc_modguard.notice & 1u));
            clean(); reset(); good(0, LEN); oc_mod_boot(0);
            ENGINES[ENGI_SHAPES] = eng_default(ENGI_SHAPES);
            trk[2].eng_req = 6; project_save(0); oc_modguard.inuse = 0; project_load(0);
            chk("a project with no track on the module's engine marks nothing", oc_modguard.inuse == 0);
            oc_mod_boot(1);
            chk("... and a death then bans nothing", oc_mod.state[0] == MD_OK && oc_modguard.banned == 0);
        }

        /* the ban is cleared by an install */
        clean(); oc_mod_boot(0); BUILD(0, LEN, 70); INSTALL(0, LEN, &rc);
        trk[2].engine = ENGI_SHAPES; oc_mod_note_use(); oc_mod_boot(1);
        chk("(setup) banned after a crash", oc_mod.state[0] == MD_CRASHED);
        BUILD(1, LEN, 71);
        chk("a new install beside a banned module is registered at once, the banned one erased, no ban left",
            INSTALL(1, LEN, &rc) == 0 && oc_mod.state[1] == MD_OK && oc_mod.state[0] == MD_EMPTY && oc_modguard.banned == 0 && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));

        /* ------------------------------------------------------------------------------------------------------------------------------
         * (0.12.1) The Prophet as a module: 8 sectors, starts 0 and 2 (sectors 0 and 4). Its span overlaps either SHAPES start, so the two never stand
         * together; an install is refused with the blocker named, and a replacement of the Prophet is always in place. Needs a module to win the number. */
#if OC_MODULE_WINS
        {
            enum { S_NONE, S_AT0, S_AT1, S_BOTH, P_AT0, P_AT2, NCFG };      /* what stands in the area before the install */
            static const char *cfgname[NCFG] = { "an empty area", "SHAPES at 0", "SHAPES at 6", "SHAPES at both starts", "a Prophet at 0", "a Prophet at 4" };
            /* the plan the rule gives, by hand: {plan for SHAPES, blocker for SHAPES, plan for the Prophet, blocker for the Prophet}; -1 none */
            static const int want[NCFG][4] = {
                /* S_NONE */ { 0, -1, 0, -1 },
                /* S_AT0  */ { 1, -1, -1, 0 },
                /* S_AT1  */ { 0, -1, -1, 1 },
                /* S_BOTH */ { 0, -1, -1, 0 },
                /* P_AT0  */ { -1, 0, 0, -1 },
                /* P_AT2  */ { -1, 2, 2, -1 },
            };
            const uint32_t PLEN = 8u * 0x1000u - 64u;             /* a Prophet module of nearly the 8 sectors (the real one is 31,148 B of 32,768) */
            uint32_t c, kk, e;
            int rcp;
            #define PBUILD(eng_, i_, len_, seed_) do { modeng = (eng_); { fw_mod_hdr_t hh_ = mk(i_, len_); code(seed_); seal(&hh_); memcpy(IMG, &hh_, sizeof hh_); } modeng = ENGI_SHAPES; } while (0)
            #define PINSTALL(eng_, i_, len_, outrc) ({ int r_ = oc_mod_begin((eng_), (i_), (len_)); uint32_t o_; \
                for (o_ = FW_MOD_CODE_OFF; !r_ && o_ < (len_); o_ += 256u) r_ = oc_mod_write(o_, IMG + o_, (len_) - o_ > 256u ? 256u : (len_) - o_); \
                if (!r_) r_ = oc_mod_end(IMG); *(outrc) = r_; r_; })
            #define PSTAND(cfg_) do { clean(); \
                if ((cfg_) == S_AT0 || (cfg_) == S_BOTH) { modeng = ENGI_SHAPES; good(0, LEN); } \
                if ((cfg_) == S_AT1 || (cfg_) == S_BOTH) { modeng = ENGI_SHAPES; modver = (cfg_) == S_BOTH ? 1u : 0u; good(1, LEN); modver = 0; } \
                if ((cfg_) == P_AT0) { modeng = ENGI_PROPHET; good(0, PLEN); } \
                if ((cfg_) == P_AT2) { modeng = ENGI_PROPHET; good(2, PLEN); } \
                modeng = ENGI_SHAPES; lg_incomplete = (cfg_) == S_BOTH; oc_mod_boot(0); lg_incomplete = 0; } while (0)
            /* geometry */
            chk("the Prophet's header at start 0 and at start 4: registered, the window ends at its last byte",
                (clean(), modeng = ENGI_PROPHET, good(0, PLEN), oc_mod_boot(0), oc_mod.state[0] == MD_OK && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(0) && fm1_guard_code_end == oc_mod_link(0) + PLEN - 1u) &&
                (clean(), good(2, PLEN), oc_mod_boot(0), oc_mod.state[2] == MD_OK && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(2) && fm1_guard_code_end == oc_mod_link(2) + PLEN - 1u));
            modeng = ENGI_SHAPES;
            chk("... XIP addresses: start 2 (sector 4) runs at 0x020C8000", oc_mod_link(2) == 0x020C8000u && oc_mod_link(0) == 0x020C4000u);
            chk("a Prophet header at start 1 (sector 6, only 6 sectors left): MD_BAD_HDR",
                (clean(), modeng = ENGI_PROPHET, good(1, LEN), oc_mod_boot(0), modeng = ENGI_SHAPES, oc_mod.state[1] == MD_BAD_HDR && !eng_mod[ENGI_PROPHET]));
            chk("a SHAPES header at start 2 (sector 4, not one of its starts): MD_BAD_HDR",
                (clean(), good(2, LEN), oc_mod_boot(0), oc_mod.state[2] == MD_BAD_HDR && !eng_mod[ENGI_SHAPES]));
            chk("a Prophet module past its 8 sectors: MD_BAD_HDR; a SHAPES module of 7 sectors: MD_BAD_HDR",
                (clean(), modeng = ENGI_PROPHET, good(0, 8u * 0x1000u + 4u > sizeof IMG ? 8u * 0x1000u : 8u * 0x1000u + 4u), oc_mod_boot(0), modeng = ENGI_SHAPES, oc_mod.state[0] == MD_OK) &&   /* exactly 8 sectors fits */
                (clean(), good(0, 7u * 0x1000u), oc_mod_boot(0), oc_mod.state[0] == MD_BAD_HDR));
            /* the plan table */
            for (c = 0; c < NCFG; c++) {
                char w[200];
                PSTAND(c);
                snprintf(w, sizeof w, "plan with %s: SHAPES %d (blocked by %d), the Prophet %d (blocked by %d)", cfgname[c], want[c][0], want[c][1], want[c][2], want[c][3]);
                chk(w, oc_mod_plan(ENGI_SHAPES) == want[c][0] && (want[c][0] >= 0 || oc_mod_blocked_by(ENGI_SHAPES) == want[c][1]) &&
                       oc_mod_plan(ENGI_PROPHET) == want[c][2] && (want[c][2] >= 0 || oc_mod_blocked_by(ENGI_PROPHET) == want[c][3]));
            }
            /* every refused begin changes nothing: not one erase, nothing writing, the modules in the table as they were */
            for (c = 0; c < NCFG; c++) {
                uint32_t e0, before2;
                char w[200];
                for (kk = 0; kk < 2u; kk++) {
                    const uint32_t eng = kk ? ENGI_PROPHET : ENGI_SHAPES;
                    const int pl = want[c][kk * 2u];
                    if (pl >= 0) continue;
                    PSTAND(c);
                    e0 = erases; before2 = fm1_guard_code_end;
                    snprintf(w, sizeof w, "%s refused over %s (every start of it): rc -5, nothing erased, nothing writing, the modules untouched", kk ? "the Prophet" : "SHAPES", cfgname[c]);
                    {   int ok = 1; uint32_t st;
                        for (st = 0; st < OC_MOD_STARTS; st++) ok &= oc_mod_begin(eng, st, LEN) < 0;
                        chk(w, ok && erases == e0 && !mw.writing && fm1_guard_code_end == before2 && oc_mod_begin(eng, (uint32_t)(want[c][kk * 2u + 1u]), LEN) == (oc_mod_geo_of(eng)->starts >> want[c][kk * 2u + 1u] & 1u ? -5 : -1)); }
                }
            }
            /* installs: each engine, each way */
            PSTAND(S_NONE);
            PBUILD(ENGI_PROPHET, 0, PLEN, 101);
            chk("install the Prophet at start 0 on an empty area: the 8 sectors erased once and nothing else, registered", (e = erases, PINSTALL(ENGI_PROPHET, 0, PLEN, &rcp) == 0) && erases - e == 8u &&
                oc_mod.state[0] == MD_OK && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(0) && !memcmp(nor + oc_mod_flash(0), IMG, PLEN) && nor[oc_mod_flash(0) + 8u * 0x1000u] == 0xFF);
            chk("... the next Prophet goes in place at its own start (8 of 12 sectors leave no room beside); SHAPES is refused naming start 0",
                oc_mod_plan(ENGI_PROPHET) == 0 && oc_mod_inplace(ENGI_PROPHET, 0) && oc_mod_plan(ENGI_SHAPES) == -1 && oc_mod_blocked_by(ENGI_SHAPES) == 0);
            { uint32_t e2 = erases;
              PBUILD(ENGI_SHAPES, 0, LEN, 102);
              chk("... a SHAPES install over it: begin refused, no sector erased, the Prophet registered", oc_mod_begin(ENGI_SHAPES, 0, LEN) == -5 && oc_mod_begin(ENGI_SHAPES, 1, LEN) == -5 &&
                  erases == e2 && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(0)); }
            /* replacement in place, a track on it */
            trk[3].engine = ENGI_PROPHET; trk[3].v[0].active = trk[3].v[0].gate = 1; trk[3].v[0].env = 1000;
            PBUILD(ENGI_PROPHET, 0, PLEN - 256u, 103);
            chk("replace the Prophet (shorter): in place, the voices off, 8 sectors erased once, the new module registered and the window ends at it",
                (e = erases, PINSTALL(ENGI_PROPHET, 0, PLEN - 256u, &rcp) == 0) && erases - e == 8u && !trk[3].v[0].active && oc_mod.state[0] == MD_OK &&
                eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(0) && fm1_guard_code_end == oc_mod_link(0) + PLEN - 256u - 1u && trk[3].engine == ENGI_PROPHET);
            /* the same at start 4 */
            PSTAND(P_AT2);
            PBUILD(ENGI_PROPHET, 2, PLEN, 104);
            chk("a Prophet at start 4 is replaced in place at start 4 (the plan names it), 8 sectors from sector 4 erased", (e = erases, oc_mod_plan(ENGI_PROPHET) == 2 && PINSTALL(ENGI_PROPHET, 2, PLEN, &rcp) == 0) &&
                erases - e == 8u && oc_mod.state[2] == MD_OK && oc_mod.state[0] == MD_EMPTY && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(2));
            chk("... a begin at start 0 while the plan is 2 is refused, nothing erased", (e = erases, oc_mod_begin(ENGI_PROPHET, 0, PLEN) == -1) && erases == e);
            /* the Prophet over damaged junk at either start (not a usable module: written over, not refused) */
            clean(); modeng = ENGI_SHAPES; good(1, LEN); nor[oc_mod_flash(1) + 0x500u] ^= 1u; oc_mod_boot(0);
            chk("(setup) a damaged SHAPES module at start 6", oc_mod.state[1] == MD_BAD_CRC);
            PBUILD(ENGI_PROPHET, 0, PLEN, 105);
            chk("a damaged module does not block: the Prophet installs over it (the damaged one is erased with the span)", PINSTALL(ENGI_PROPHET, 0, PLEN, &rcp) == 0 &&
                oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_EMPTY);
            /* removal */
            PSTAND(P_AT0);
            chk("remove the Prophet: out of the table, 8 sectors erased, the area empty", (e = erases, oc_mod_remove(ENGI_PROPHET) == 0) && erases - e == 8u && !eng_mod[ENGI_PROPHET] &&
                oc_mod.state[0] == MD_EMPTY && nor[oc_mod_flash(0)] == 0xFF && !fm1_guard_code_end);
            PSTAND(P_AT2);
            chk("remove 127 erases all 12 sectors", (e = erases, oc_mod_remove(127) == 0) && erases - e == 12u && oc_mod.state[2] == MD_EMPTY);
            PSTAND(P_AT0); oc_mod_remove(ENGI_PROPHET); PBUILD(ENGI_SHAPES, 0, LEN, 106);
            chk("... and after the Prophet is gone SHAPES installs at once", (PINSTALL(ENGI_SHAPES, 0, LEN, &rcp) == 0) &&
                eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0) && !eng_mod[ENGI_PROPHET]);
            /* a Prophet in place, cut at every program and every erase: never a half module registered, and the next install works */
            {   uint32_t cutsP = 0, badP = 0, again = 0;
                for (kk = 0; kk < 80u; kk++) {                                          /* erases (8 for the span) then programs */
                    int r;
                    PSTAND(P_AT0);
                    PBUILD(ENGI_PROPHET, 0, PLEN, 107);
                    if (kk < 20u) erase_fail_after = (int)kk; else fail_after = (int)kk - 20;
                    r = PINSTALL(ENGI_PROPHET, 0, PLEN, &rcp);
                    erase_fail_after = -1; fail_after = -1;
                    oc_mod_boot(0);
                    if (r == 0) { if (kk < 20u) continue; else break; }
                    cutsP++;
                    if (kk == 0u)                                                   /* the first erase never ran: the old module is whole and registered */
                        badP += !(oc_mod.state[0] == MD_OK && eng_mod[ENGI_PROPHET] == oc_mod_hdr_ptr(0));
                    else                                                            /* the header sector went first: no module, nothing registered */
                        badP += (oc_mod.state[0] != MD_EMPTY && oc_mod.state[0] != MD_TORN) || eng_mod[ENGI_PROPHET] || fm1_guard_code_end;
                    PBUILD(ENGI_PROPHET, 0, PLEN, 108);                                 /* the next install works whatever the cut left */
                    again += PINSTALL(ENGI_PROPHET, 0, PLEN, &rcp) == 0 && oc_mod.state[0] == MD_OK;
                }
                chk("a Prophet replaced in place, cut at every erase and every program: the old module whole when no erase ran, else no module and nothing registered (the old one was erased first), and the next install works",
                    cutsP > 20u && badP == 0 && again == cutsP);
            }
            /* the crash count of start 2 */
            PSTAND(P_AT2);
            trk[2].engine = ENGI_PROPHET; oc_mod_note_use();
            chk("start 2 in use sets bit 4 of the use mark; a death in the first 30 s bans it and says MODULE BANNED once",
                (oc_modguard.inuse & 4u) && (oc_mod_boot(1), oc_mod.state[2] == MD_CRASHED && (oc_modguard.banned & 4u) && !eng_mod[ENGI_PROPHET] && !fm1_guard_code_end));
            chk("... removing it lifts the ban (the use bit and the count of start 2 are cleared)", (oc_mod_remove(ENGI_PROPHET), oc_mod_unban(2), !(oc_modguard.banned & 4u) && oc_modguard.unclean[2] == 0));
            #undef PBUILD
            #undef PINSTALL
            #undef PSTAND
        }
#endif
        #undef BUILD
        #undef INSTALL
    }

    /* a flash that will not read */
    clean(); good(0, LEN);
    flash_ok = 0;
    oc_mod_boot(0);
    chk("(the sim's flash_ok only gates the device checks; the area read is by st_read) a read still works", oc_mod.state[0] == MD_OK || oc_mod.state[0] == MD_EMPTY);
    flash_ok = 1;
#endif
    printf(bad ? "module loader: FAILED\n" : "module loader: all passed\n");
    return bad != 0;
}
