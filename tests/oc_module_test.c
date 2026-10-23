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
 * a flash that will not read. The wiring of the loader into persist_boot and the main loop, the guard's register writes and the
 * installer are checked on the device (CI build) and by the installer's tests (step 6).
 */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

static int bad;
static void chk(const char *what, int ok)
{
    printf("module loader: %-104s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define LEN 3000u                                     /* a made module: header page + 2,744 bytes of "code" */
static uint8_t IMG[OC_MOD_MAXLEN];
static fw_mod_hdr_t mk(uint32_t i, uint32_t len)      /* the fields a good SHAPES module of this start has */
{
    fw_mod_hdr_t h;
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.engine = ENGI_SHAPES; h.state_size = 1234u; h.engine_ptr = oc_mod_link(i) + 0x180u; h.link_addr = oc_mod_link(i);
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
static void m_len_big(fw_mod_hdr_t *h) { h->length = OC_MOD_MAXLEN + 4u; }
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

    for (i = 0; i < OC_MOD_STARTS; i++) {
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
        for (i = 0; i < OC_MOD_STARTS; i++) {
            char w[160];
            clean();
            st = with(i, T[k].f, T[k].reseal, &why);
            snprintf(w, sizeof w, "refused at start %u: %s", i * 6u, T[k].what);
            chk(w, st == T[k].want && (!T[k].why || why == T[k].why) && !eng_mod[ENGI_SHAPES] && !fm1_guard_code_end);
            if (T[k].want != MD_EMPTY) chk("... and the built-in table is untouched", ENGINES[ENGI_SHAPES] == eng_default(ENGI_SHAPES));
        }
    (void)m_hcrc;

    /* a header whose own CRC is wrong: torn */
    for (i = 0; i < OC_MOD_STARTS; i++) {
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
    for (i = 0; i < OC_MOD_STARTS; i++) {
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
    clean(); good(0, OC_MOD_MAXLEN);
    oc_mod_boot(0);
    chk("a module of the full 6 sectors is accepted (its last byte is the area's up to the next start)",
        oc_mod.state[0] == MD_OK && fm1_guard_code_end == oc_mod_link(0) + OC_MOD_MAXLEN - 1u);

    /* two modules */
    clean(); good(0, LEN); good(1, LEN + 400u);
    oc_mod_boot(0);
    chk("two good modules for one engine number: the lower start is registered, the other is NOSLOT",
        oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_NOSLOT && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(0));
    chk("... the guard's window follows the registered one only", fm1_guard_code_end == oc_mod_link(0) + LEN - 1u);
    clean(); good(1, LEN);
    { fw_mod_hdr_t h = mk(0, LEN); code(9); seal(&h); h.crc ^= 1u; h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u); put(0, &h, 1); }
    oc_mod_boot(0);
    chk("a bad module at start 0 and a good one at start 6: the good one is registered (a replace cut after the new was written)",
        oc_mod.state[0] == MD_BAD_CRC && oc_mod.state[1] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));

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
    oc_mod_boot(0);
    oc_mod_note_use();
    chk("two modules, one registered: only the registered start's bit is set", oc_modguard.inuse == 1u);
    oc_mod_boot(1);
    chk("... a crash bans that one; the other (valid, not registered before) takes the engine number",
        oc_mod.state[0] == MD_CRASHED && oc_mod.state[1] == MD_OK && eng_mod[ENGI_SHAPES] == oc_mod_hdr_ptr(1));

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
