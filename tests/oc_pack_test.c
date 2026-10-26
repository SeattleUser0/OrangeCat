/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0: the asset pack (firmware/src/oc_pack.h, oc_pack.c, tools/gen_pack.py) on a simulated NOR flash.
 *   build/host/oc_pack_test
 * Checked: the boot check says missing / this build's / another build's / damaged and SHAPES and MACRO play only with the pack
 * (the gate is on in this test, OC_PACK_GATE); an install (begin, write, end) puts the pack in and the engines back; a write torn at
 * EVERY program leaves no pack (never half of one); a bad payload CRC, another build's hash, a wrong length, writes out of order and
 * a sector tail that would read as an update record are refused; a damaged byte is found at the next boot; erase empties the area;
 * (0.9.1) the preset library is the group of lib_group.c and not in the pack's area: an install, an erase and a wipe of bank 16 touch no
 * preset and no pack sector, an old Prophet pair in the area moves into the group at boot BEFORE the pack check, and an install is
 * refused while that move has not finished; the loader's tail rule agrees with tools/gen_pack.py's (the same C test on the real pack is in
 * tests/run_tests.sh: python3 tools/gen_pack.py --check).
 * The in-memory tables of the host build stand for the device's (OC_PACK_XIP 0): this test is about the pack's flash life. */
/* Revisions:
 * (Orange Cat 0.13.4) pk_image_check over a buffer and over the flash: one verdict
 * (Orange Cat 0.9.1) Library in groups with a shared spare sector (0.9.1)
 * (Orange Cat 0.12.2) the Prophet module fits 8 sectors (34,764 B measured): the firmware's PITCH_OCT and ENV_LIN through the table, the factory programs in a palette fo...
 */
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
static uint8_t PAK[OC_PACK_AREA];
static uint32_t probe_n, probe_open;
static void gate_probe(void) { probe_n++; probe_open |= oc_pack_good; }   /* oc_pack_changed: the gate as the copies are made */
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
/* the engine's own render for a few blocks, called directly (the mixer's DC and effects tails would speak for it otherwise): the peak of
 * what it added to a cleared buffer, 0 for silence */
static int32_t peak_of_note(uint32_t engine)
{
    int32_t out[CTL], peak = 0;
    vmod_t m;
    const engine_t *en;
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song);
    host_tracks_init(); memset(resource, 0, sizeof resource);
    song.master_q12 = 4096;
    host_preset(&trk[0], engine, 0);
    trk[0].v[0].active = trk[0].v[0].gate = 1; trk[0].v[0].vel = 100;
    eng_state_prepare(&trk[0]);
    en = ENGINES[trk[0].engine];
    en->note_on(&trk[0], &trk[0].v[0]);
    memset(&m, 0, sizeof m);
    m.pitch16 = 60 * 16; m.shape = 64 << 8; m.amp0 = m.amp1 = 32767; m.envq15 = 32767;
    for (uint32_t b = 0; b < 100u; b++) {
        memset(out, 0, sizeof out);
        en->render(&trk[0], &trk[0].v[0], out, CTL, &m);
        for (uint32_t i = 0; i < CTL; i++) { int32_t l = out[i] < 0 ? -out[i] : out[i]; if (l > peak) peak = l; }
    }
    return peak;
}

int main(void)
{
    int rc;
    setvbuf(stdout, NULL, _IONBF, 0);
    fresh_flash();
    make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);

    /* ---- boot: nothing there */
    oc_pack_boot();
    chk("empty flash: no pack, the gate closed", pk.state == PK_MISSING && !oc_pack_good && !OC_PACK_READY());
    chk("empty flash: the Prophet objects are retired by the build, the library is the group's", st_retired(OBJ_P5BANK0) && !st_retired(up_obj(16)));
    chk("without the pack MACRO and SHAPES are silent", peak_of_note(ENGI_MACRO) == 0 && peak_of_note(ENGI_SHAPES) == 0);

    /* ---- an install */
    {
        uint32_t before = erases, f0 = oc_feeds;
        chk("install: the whole of it", install(&rc) == 0 && rc == 0);
        chk("install: erased the ten sectors once", erases - before == PK_SECTORS);
        chk("install: the watchdog was fed before every erase and through the check of the payload (0.12.2)", oc_feeds - f0 >= PK_SECTORS);
    }
    chk("installed: state OK, the gate open", pk.state == PK_OK && oc_pack_good && OC_PACK_READY());
    chk("installed: the flash holds the pack byte for byte", !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN));
    chk("installed: the Prophet objects retired (read empty, refuse to save)", st_retired(OBJ_P5BANK0 + 4) && st_save(OBJ_P5BANK0, PAK, 64) < 0);
    chk("installed: MACRO and SHAPES sound again", peak_of_note(ENGI_MACRO) > 200 && peak_of_note(ENGI_SHAPES) > 200);
    {
        uint32_t len, hash, st = oc_pack_info(&len, &hash);
        chk("info: this build's pack, its length and hash", st == PK_OK && len == OC_PACK_LEN && hash == OC_PACK_HASH);
    }
    reboot();
    chk("reboot: the pack is checked and found, the Prophet objects retired from the first read", pk.state == PK_OK && oc_pack_good && st_retired(OBJ_P5BANK0));
    {   /* (0.14.0) the hot sets are copied before the gate opens, and a pack gone bad closes the gate before its copies change: a MACRO note
         * sequenced during an install never renders from a half-copied block */
        void (*was)(void) = oc_pack_changed;
        int ok;
        oc_pack_changed = gate_probe;
        probe_n = 0; probe_open = 0;
        ok = install(&rc) == 0 && probe_n >= 1u && !probe_open && oc_pack_good;
        nor[OC_PACK_LO + 5000] ^= 0x10;
        oc_pack_good = 1; probe_n = 0;
        oc_pack_scan();
        ok &= probe_n == 1u && !probe_open && !oc_pack_good;
        nor[OC_PACK_LO + 5000] ^= 0x10;
        oc_pack_scan();
        ok &= oc_pack_good;
        oc_pack_changed = was;
        chk("the hot sets are copied with the gate closed: an install opens it after the copy, a damaged pack closes it before (0.14.0)", ok);
    }

    /* ---- damaged bytes, another build's pack */
    nor[OC_PACK_LO + 5000] ^= 0x10;
    reboot();
    chk("a changed payload byte: DAMAGED at boot, the engines silent, the area still the pack's", pk.state == PK_DAMAGED && !oc_pack_good && st_retired(OBJ_P5BANK0));
    nor[OC_PACK_LO + 5000] ^= 0x10;
    reboot();
    chk("... put back: OK again", pk.state == PK_OK && oc_pack_good);
    nor[OC_PACK_LO + 100] &= 0x7F;
    reboot();
    chk("a changed header byte: DAMAGED", pk.state == PK_DAMAGED && !oc_pack_good);
    memcpy(nor + OC_PACK_LO, PAK, OC_PACK_HDR);
    {
        uint8_t other[OC_PACK_AREA];
        make_pack(other, OC_PACK_HASH ^ 1u, OC_PACK_LEN);
        memcpy(nor + OC_PACK_LO, other, OC_PACK_LEN);
        reboot();
        chk("another build's pack (its hash): OTHER, the engines silent, the area kept", pk.state == PK_OTHER && !oc_pack_good);
        make_pack(other, OC_PACK_HASH, OC_PACK_LEN + 4u);
        memcpy(nor + OC_PACK_LO, other, OC_PACK_LEN + 4u);
        reboot();
        chk("another build's pack (its length): OTHER", pk.state == PK_OTHER && !oc_pack_good);
    }

    /* ---- erase */
    {
        uint32_t i, blank = 1;
        reboot();
        chk("erase: done", oc_pack_erase() == 0);
        for (i = 0; i < OC_PACK_AREA; i++) blank &= nor[OC_PACK_LO + i] == 0xFF;
        chk("erase: the ten sectors blank, no pack, the engines silent", blank && pk.state == PK_MISSING && !oc_pack_good && peak_of_note(ENGI_MACRO) == 0);
        chk("erase: the Prophet objects stay retired (the build's), the library is not in the area", st_retired(OBJ_P5BANK0));
        reboot();
        chk("erase: and so after a reboot", pk.state == PK_MISSING && st_retired(OBJ_P5BANK0));
    }

    /* ---- refusals: nothing written */
    {
        uint32_t before;
        fresh_flash(); oc_pack_boot();
        before = erases;
        chk("begin refuses another length, erasing nothing", oc_pack_begin(OC_PACK_LEN + 1u) == -1 && oc_pack_begin(0) == -1 && erases == before);
        chk("write, end without begin are refused", oc_pack_write(OC_PACK_HDR, PAK, 16) == -1 && oc_pack_end(PAK) == -1);
        chk("begin", oc_pack_begin(OC_PACK_LEN) == 0);
        chk("write out of order is refused", oc_pack_write(OC_PACK_HDR + 16u, PAK, 16) == -1);
        chk("... and the install goes on: the right write is taken", oc_pack_write(OC_PACK_HDR, PAK + OC_PACK_HDR, 16) == 0);
        chk("no pack, no header on the flash", nor[OC_PACK_LO] == 0xFF);
    }
    {   /* a write past the length; the end before all the data */
        int r;
        fresh_flash(); oc_pack_boot();
        oc_pack_begin(OC_PACK_LEN);
        r = oc_pack_write(OC_PACK_HDR, PAK + OC_PACK_HDR, OC_PACK_LEN - OC_PACK_HDR + 1u);
        chk("a write past the length is refused", r == -1);
        oc_pack_begin(OC_PACK_LEN);
        oc_pack_write(OC_PACK_HDR, PAK + OC_PACK_HDR, 1000);
        chk("end before all the data is refused, no header written", oc_pack_end(PAK) == -1 && nor[OC_PACK_LO] == 0xFF);
    }
    {   /* a bad payload CRC; another build's hash; a header CRC that does not hold */
        uint8_t h[OC_PACK_AREA];
        int r;
        fresh_flash(); oc_pack_boot();
        memcpy(h, PAK, OC_PACK_LEN);
        wr32(h + 16, crc32_of(h + OC_PACK_HDR, OC_PACK_LEN - OC_PACK_HDR) ^ 1u); wr32(h + 508, crc32_of(h, 508));
        memcpy(PAK, h, OC_PACK_LEN);
        r = install(&rc);
        chk("a header with a wrong payload CRC is refused at end (3), nothing committed", r == 3 && rc == -1 && pk.state == PK_MISSING && nor[OC_PACK_LO] == 0xFF && !oc_pack_good);
        make_pack(PAK, OC_PACK_HASH ^ 0x100u, OC_PACK_LEN);
        r = install(&rc);
        chk("another build's hash is refused at end, nothing committed", r == 3 && rc == -1 && nor[OC_PACK_LO] == 0xFF && !oc_pack_good);
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        PAK[508] ^= 1;
        r = install(&rc);
        chk("a header whose own CRC fails is refused", r == 3 && rc == -1 && nor[OC_PACK_LO] == 0xFF);
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        PAK[OC_PACK_HDR + 500] ^= 0x40;                 /* the payload differs from the CRC in the header */
        r = install(&rc);
        chk("a payload that does not match its header's CRC is refused", r == 3 && rc == -1 && nor[OC_PACK_LO] == 0xFF);
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
    }
    {   /* a sector tail that the update loader would take for a record */
        int r;
        uint32_t crc;
        fresh_flash(); oc_pack_boot();
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        {   /* sector 0's last 256 bytes: an 80-byte record at 0xF00 */
            uint8_t *rec = PAK + 0xF00;
            memset(rec, 0x11, 80);
            rec[6] = 0x41; rec[7] = 0x54;
            crc = pk_crc16(rec + 2, 78);
            rec[0] = (uint8_t)crc; rec[1] = (uint8_t)(crc >> 8);
            fix_header(PAK, OC_PACK_LEN);
        }
        chk("the test's record reads as an update record (the rule)", pk_record_like(PAK + 0xF00));
        r = install(&rc);
        chk("a pack whose sector tail would read as an update record is refused, no header written", r == 3 && rc == -1 && nor[OC_PACK_LO] == 0xFF && pk.state == PK_MISSING);
        PAK[0xF00 + 7] = 0x55;                           /* one byte off: not a record */
        fix_header(PAK, OC_PACK_LEN);
        chk("... one byte different: installs", install(&rc) == 0 && pk.state == PK_OK);
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
    }

    /* ---- a write torn at every program */
    {
        uint32_t k, ok = 1, half = 0, done = 0;
        for (k = 0; k < 60u; k++) {
            int r;
            fresh_flash(); oc_pack_boot();
            fail_after = (int)k;
            r = install(&rc);
            fail_after = -1;
            reboot();
            if (r == 0) { done++; ok &= pk.state == PK_OK && oc_pack_good && !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN); }
            else { ok &= !oc_pack_good && pk.state != PK_OK; if (pk.state == PK_DAMAGED) half++; }
            ok &= (pk.state == PK_OK) == (r == 0);
        }
        chk("a write cut after any program (0..59) leaves no pack, or the whole of it", ok && done > 0);
        chk("... cut between the header's two pages: DAMAGED, the engines silent (at least that one case)", half == 1);
    }

    /* ---- the library and the pack (0.9.1): the group is outside the pack's area */
    {
        up_rec_t r2 = {0};
        uint32_t i, slot = 16u * UP_PER_BANK, before;
        static uint8_t image[ST_PAYLOAD_MAX];
        int n;
        r2.used = UP_USED; r2.ver = UP_VER; r2.engine = 9u; r2.np = P_COUNT; strcpy(r2.name, "KEPT");
        for (i = 0; i < P_COUNT; i++) r2.packed[i] = 64;
        fresh_flash(); oc_pack_boot(); up_cache_reset();
        chk("a preset in bank 16 saves (into the group, not the pack's area)", up_put(slot, &r2) == 0 && up_used(slot) && st_exists(up_obj(16)));
        {
            int outside = 1;
            for (i = 0; i < LG_SECTORS; i++) outside &= lg_addr[i] + ST_SECTOR <= OC_PACK_LO || lg_addr[i] >= OC_PACK_LO + OC_PACK_AREA;
            chk("every group sector is outside the pack's ten", outside && !st_exists(OBJ_P5BANK0));
        }
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        before = erases;
        chk("install: no question, the ten sectors erased once, nothing else", install(&rc) == 0 && erases - before == PK_SECTORS);
        chk("... the preset is still there, bank 16 saves again, the pack is whole",
            up_used(slot) && st_exists(up_obj(16)) && up_put(slot, &r2) == 0 && up_used(slot) && !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN) && pk.state == PK_OK);
        {
            int ok = 1;
            for (uint32_t b = 16u; b < 21u; b++) ok &= up_put(b * UP_PER_BANK + 3u, &r2) == 0;
            for (uint32_t k = 0; k < 60u; k++) ok &= up_put((16u + k % 5u) * UP_PER_BANK + k % UP_PER_BANK, &r2) == 0;
            chk("65 saves into banks 16..20 leave the pack byte for byte as it was", ok && !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN) && pk.state == PK_OK);
        }
        chk("a wipe of bank 16 is a save like any other (the pack stays whole)", shared_wipe_bank(16, 0) == 0 && !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN) && !up_used(slot));
        chk("another bank saves as before", up_put(0, &r2) == 0 && up_used(0));
        before = erases;
        chk("erase of the pack: no question, the ten sectors, no preset touched", oc_pack_erase() == 0 && erases - before == PK_SECTORS && up_used(0));
    }
    {   /* an old unit: the Prophet banks' pairs hold presets in the pack's area. The boot moves them first; the pack check sees a blank area */
        up_rec_t r2 = {0};
        uint32_t i, slot = 17u * UP_PER_BANK + 2u;
        static uint8_t image[ST_PAYLOAD_MAX];
        int n, ok;
        char nm[16];
        r2.used = UP_USED; r2.ver = UP_VER; r2.engine = 9u; r2.np = P_COUNT; strcpy(r2.name, "OLDPAIR");
        for (i = 0; i < P_COUNT; i++) r2.packed[i] = 64;
        fresh_flash(); oc_pack_boot(); up_cache_reset();
        up_put(slot, &r2);
        n = st_load(up_obj(17), image, sizeof image);                     /* bank 17 as the group has it ... */
        memset(nor, 0xFF, sizeof nor);                                   /* ... a unit as 0.9.0 left it: the same bank in its pair */
        st_retire_engines(0);
        ok = n > 0 && st_save(OBJ_P5BANK0 + 1, image, (uint32_t)n) == 0 && st_save(OBJ_P5BANK0 + 1, image, (uint32_t)n) == 0;
        st_retire_engines(7);
        up_cache_reset();
        chk("fixture: an old Prophet pair holds a preset in the pack's area", ok && nor[ST_PACK_LO + 2u * ST_SECTOR] == 0x46);
        ok = up_lg_boot() == 1;                                           /* persist_boot: the move comes before the pack check */
        oc_pack_boot();
        up_cache_reset();
        {
            int blank = 1;
            for (i = 0; i < OC_PACK_AREA; i++) blank &= nor[OC_PACK_LO + i] == 0xFF;
            chk("boot: the pair moved into the group, the pack area blank, no pack found", ok && blank && pk.state == PK_MISSING && up_used(slot) && (up_name(slot, nm), !strncmp(nm, "OLDPAIR", 7)));
        }
        make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
        chk("install over the area: the preset stays", install(&rc) == 0 && up_used(slot) && pk.state == PK_OK);
        /* the move not finished (a flash error at boot): the area may still hold an old pair, so the installer and the eraser refuse */
        lg_incomplete = 1;
        {
            uint32_t before = erases;
            chk("a move not finished: install and erase are refused (-4), nothing erased",
                oc_pack_begin(OC_PACK_LEN) == -4 && oc_pack_erase() == -4 && erases == before && !memcmp(nor + OC_PACK_LO, PAK, OC_PACK_LEN));
        }
        lg_incomplete = 0;
    }

    /* ---- (0.13.4) one copy of the checks: pk_image_check over a buffer (the card's file) and over the flash (the installed image) gives the same verdict for each case */
    {
        uint32_t base = OC_PACK_LO, k;
        struct { const char *what; int want; } C[4] = { { "good", 0 }, { "one payload byte flipped", 2 }, { "a record on sector 0's tail", 3 }, { "another build's hash", 1 } };
        for (k = 0; k < 4u; k++) {
            int vb, vf;
            uint8_t rec[80];
            char w[160];
            fresh_flash();
            make_pack(PAK, OC_PACK_HASH, OC_PACK_LEN);
            if (k == 1) PAK[(OC_PACK_HDR + OC_PACK_LEN) / 2u] ^= 0x10;
            if (k == 2) {
                uint32_t c;
                for (uint32_t j = 0; j < 80u; j++) rec[j] = (uint8_t)((j * 11u + 5u) | 1u);
                rec[6] = 0x41; rec[7] = 0x54;
                c = pk_crc16(rec + 2, 78u); rec[0] = (uint8_t)c; rec[1] = (uint8_t)(c >> 8);
                memcpy(PAK + 0xF00u, rec, 80);
                fix_header(PAK, OC_PACK_LEN);
            }
            if (k == 3) { make_pack(PAK, OC_PACK_HASH ^ 1u, OC_PACK_LEN); }
            memset(nor + OC_PACK_LO, 0xFF, OC_PACK_AREA);
            memcpy(nor + OC_PACK_LO, PAK, OC_PACK_LEN);              /* the image as an installed one would sit in the area */
            vb = pk_image_check(oc_rd_buf, PAK, OC_PACK_LEN);
            vf = pk_image_check(oc_rd_flash, &base, OC_PACK_LEN);
            snprintf(w, sizeof w, "pk_image_check, %s: the file's bytes and the flash's give the same verdict (%d)", C[k].what, C[k].want);
            chk(w, vb == C[k].want && vf == vb);
        }
        fresh_flash();
    }

    /* ---- the pack area and the engines' gate on a firmware build without the gate: the tables are the image's */
    chk("the area is the storage map's PACK area", OC_PACK_LO == ST_PACK_LO && OC_PACK_AREA == ST_PACK_SECTORS * ST_SECTOR);
    printf("%s\n", bad ? "OC PACK TEST FAILED" : "oc pack test passed");
    return bad != 0;
}
