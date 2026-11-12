/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8a (docs/orangecat/FX_MODULES_PLAN.md 16): the REAL reverb module (build/module/reverb.<start>.img, the images tools/build.py --module reverb linked with the device toolchain
 * and proved) through the loader and the install path, on the simulated NOR of tests/persistence_test.c, as tests/fx_dub_image_test.c does for DUB TAPE. The host cannot run the module's code
 * (tests/fx_reverb_module_test.c runs the same source as a host object); what it does is everything the firmware does with the image's BYTES:
 *   1  boot-scan at every start of this build's grid: registered in the Effects table (module 2, version 1.0.0, kind 2, a bank of three), no engine slot, one sector, the PC guard to its end
 *   2  the bank, the three caps blocks, their call tables (one word each: fx2_kcalls_t) and pages out of the flash through the loader's reader; the pages pass the checker as SC_FX; and the
 *      Release 2 import (fw_fxstore.h fx_legacy_import) through the REAL bytes: with TYPE the effect's mode its seven values are song.g's, with another TYPE its defaults
 *   3  the install by module id (plan, begin, write, end), a re-install beside
 *   4  each mutation refused at the boot scan, by the page's and the card's check (oc_fxmod_image_check) and at the install's end, nothing programmed: the caps block (magic, version, the abi of
 *      the INSERT host, a hot set, the call table out of the image, a call that is 0, the pages out of the image, a snapshot size of 0), an entry of another kind, an effect id twice
 *   5  the unmutated image accepted by the same check
 *   6  the send host's registration through the loader (oc_fx2mod_commit): the window first, a refusal is the Effects row's why; an unrelated rebuild keeps the kernels; the erase guard lets
 *      go before any erase of the module's sectors (a sector under it, a removal, a newer version beside) and not for an erase elsewhere; a banned module is not registered
 *   build/host/fx_reverb_image_test build/module
 * Built with OC_MODULE_WINS 1 as fx_module_test (tests/run_tests.sh). Without the images: SKIPPED (FXRI_REQUIRE=1: a failure). */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
#define OC_MOD_GEO_QUAL
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
#include "fw_fxstore.h"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

static int bad;
static void chk(const char *what, int ok)
{
    printf("fx reverb image: %-150s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint8_t IMG[0x1000u];
static uint32_t ILEN;
static const char *dir;
static void clean(void)
{
    uint32_t e;
    memset(nor, 0xFF, sizeof nor);
    memset(&oc_modguard, 0, sizeof oc_modguard);
    memset(trk, 0, sizeof trk);
    for (e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fx2_modk_clear();
    fm1_guard_code_end = 0;
    ocs_resv = SMP_LEGACY_MASK;
    fail_after = -1; erase_fail_after = -1;
}
static void wr32(uint32_t off, uint32_t v) { IMG[off] = (uint8_t)v; IMG[off + 1] = (uint8_t)(v >> 8); IMG[off + 2] = (uint8_t)(v >> 16); IMG[off + 3] = (uint8_t)(v >> 24); }
static int load(int32_t sector)                          /* the image linked for the signed sector `sector` of the grid (build.py names its files by it) */
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/reverb.%d.img", dir, (int)sector);
    f = fopen(p, "rb");
    if (!f) return 0;
    memset(IMG, 0, sizeof IMG);
    ILEN = (uint32_t)fread(IMG, 1, sizeof IMG, f);
    fclose(f);
    if (ILEN <= FW_MOD_CODE_OFF || ILEN > sizeof IMG) return 0;
    wr32(4, FW_API_IFACE);                             /* the device's identifier mixes in sizes that are not the host's, and a host test build has its own pack hash: this host's, so the checks that follow are the loader's own */
    wr32(36, OC_PACK_HASH);
    wr32(56, ocs_crc32(IMG, 56));
    return 1;
}
static int load_at(uint32_t start) { return load(OC_MOD_START_REL(start)); }
static uint32_t rd32(const uint8_t *b, uint32_t off) { return b[off] | (uint32_t)b[off + 1] << 8 | (uint32_t)b[off + 2] << 16 | (uint32_t)b[off + 3] << 24; }
static void reseal(void)                                 /* the code's CRC and the header's, as the build makes them */
{
    wr32(52, ocs_crc32(IMG + FW_MOD_CODE_OFF, ILEN - FW_MOD_CODE_OFF));
    wr32(56, ocs_crc32(IMG, 56));
}
static int rd_whole(void *ctx, uint32_t a, void *b, uint32_t n2) { (void)ctx; if (a + n2 > ILEN) return 1; memcpy(b, IMG + a, n2); return 0; }
static int install(uint32_t mid, uint32_t start)
{
    uint32_t o, r = (uint32_t)oc_fxmod_begin(mid, start, ILEN);
    for (o = FW_MOD_CODE_OFF; !r && o < ILEN; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, ILEN - o > 256u ? 256u : ILEN - o);
    return (int)(r ? r : (uint32_t)oc_mod_end(IMG));
}
static int rd_img(void *ctx, uint32_t addr, void *buf, uint32_t n)   /* the image's words at the flash, as the module sees its own address space */
{
    uint32_t link = *(uint32_t *)ctx;
    if (addr < link || addr - link + n > ILEN) return 1;
    memcpy(buf, IMG + (addr - link), n);
    return 0;
}
static void never(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n) { (void)c; (void)in; (void)out; (void)n; }   /* a host address for a call this test never makes */
static const fx2_kcalls_t NEVER = { never };
/* the bank as the device's oc_fx2mod_reg reads it, with the caps blocks copied out of the image's bytes and a host call in place of the module's ARM code */
static fx_caps_t reg_caps[3];
static fx2_reg_t reg[3];
static void reg_from_image(uint32_t link)
{
    uint32_t desc = rd32(IMG, 28) - link, ent = rd32(IMG, desc + 12) - link, k;
    for (k = 0; k < 3u; k++) {
        memcpy(&reg_caps[k], IMG + (rd32(IMG, ent + 16u * k + 12u) - link), sizeof reg_caps[k]);
        reg[k].effect_id = IMG[ent + 16u * k]; reg[k].caps = &reg_caps[k]; reg[k].calls = &NEVER;
    }
}
int main(int argc, char **argv)
{
    static const int16_t KN[3][7] = {{77, 41, 33, 55, 88, -20, 66}, {100, 25, -7, 44, 12, 30, 90}, {64, 3, 5, 99, 50, -45, 10}};   /* tests/fx_r2_values.h FXR2_KN: a value of every knob, in each mode's range */
    static const uint8_t G7[7] = {G_RSIZE, G_RDAMP, G_RPRE, G_RMOD, G_RRATE, G_RFILT, G_RWIDE};
    static const int16_t DEF[3][7] = {{90, 60, 0, 0, 30, 0, 0}, {90, 60, 0, 0, 30, 0, 0}, {90, 0, 0, 0, 30, 0, 0}};
    uint32_t i, ok, nstart = 0;
    dir = argc > 1 ? argv[1] : "build/module";
    if (!load(0)) {
        printf("fx reverb image: %s/reverb.0.img is not there (build it: tools/build.py --module reverb): SKIPPED%s\n", dir, getenv("FXRI_REQUIRE") ? " (FXRI_REQUIRE set: FAILED)" : "");
        return getenv("FXRI_REQUIRE") ? 2 : 0;
    }
    /* ---- 1. every start of the grid ---- */
    ok = 1;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        uint32_t e;
        clean();
        if (!load_at(i)) { ok = 0; printf("  no image for start %u (sector %d)\n", (unsigned)i, (int)OC_MOD_START_REL(i)); continue; }
        memcpy(nor + oc_mod_flash(i), IMG, ILEN);
        oc_mod_boot(0);
        e = oc_mod.state[i] == MD_OK && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == FX2_MODULE_ID && oc_mod.fx[0].version == 0x00010000u && oc_mod.fx[0].kind == FW_MOD_KIND_SEND && oc_mod.fx[0].nfx == 3 &&
            oc_mod.fx[0].start == i && oc_fx_find(FX2_MODULE_ID) == 0 && fm1_guard_code_end == oc_mod_link(i) + ILEN - 1u && oc_mod_nsec(ILEN) == 1u && ILEN == oc_mod_hdr_ptr(i)->length;
        for (uint32_t g = 0; g < NENGINES; g++) e &= !eng_mod[g];
        if (!e) printf("  start %u (sector %d): state %u why %u nfx %u\n", (unsigned)i, (int)OC_MOD_START_REL(i), oc_mod.state[i], oc_mod.why[i], oc_mod.nfxmod);
        ok &= e; nstart++;
    }
    {   char t[220];
        snprintf(t, sizeof t, "every start of this build's grid (%u): the image boot-scans good, is the Effects table's module 2 (version 1.0.0, kind 2, a bank of three), takes no engine slot, is ONE sector (%u B of 4,096), the PC guard reaches its end",
                 (unsigned)nstart, (unsigned)ILEN);
        chk(t, ok && nstart == OC_MOD_STARTS);
    }
    /* ---- 2. the bank out of the flash ---- */
    {
        uint32_t s = 6u < OC_MOD_STARTS ? 6u : 0u, link, desc, ent, k, calls[3];
        clean(); load_at(s);
        memcpy(nor + oc_mod_flash(s), IMG, ILEN);
        oc_mod_boot(0);
        link = oc_mod_link(s);
        desc = rd32(IMG, 28) - link; ent = rd32(IMG, desc + 12) - link;
        chk("the descriptor: n 3, module id 2, version 1.0.0, no scene; the header: kind 2 with no engine number (0x200), state size 0",
            rd32(IMG, desc) == 3u && rd32(IMG, desc + 4) == 2u && rd32(IMG, desc + 8) == 0x00010000u && rd32(IMG, desc + 16) == 0 && rd32(IMG, 20) == 0x200u && rd32(IMG, 24) == 0);
        for (k = 0; k < 3u; k++) {
            static const char *const NM[3] = { "SPRING", "SHIMMER", "RESO" };
            uint32_t e = ent + 16u * k, caps = rd32(IMG, e + 12) - link, pages, v;
            int16_t val[8];
            fx_caps_t c;
            char t[260];
            memcpy(&c, IMG + caps, sizeof c);
            pages = c.pages;
            calls[k] = rd32(IMG, c.kernel - link);
            snprintf(t, sizeof t, "%s: entry %u is effect %u, kind 2, flags 0, no parameter table; its caps block: GFXK, 56 bytes, version 1, abi = this build's FX2_ABI_ID, no hot set, no rows, one instance, no state of its own, "
                     "the model line %u block(s), the snapshot 2 * G_COUNT; its ONE call is an address inside the code", NM[k], (unsigned)k, (unsigned)k + 1u, k == 1u ? 1u : 0u);
            chk(t, IMG[e] == k + 1u && IMG[e + 1] == 2u && !IMG[e + 2] && !IMG[e + 3] && !IMG[e + 4] && !rd32(IMG, e + 8) && c.magic == FX_CAPS_MAGIC && c.size == 56 && c.version == 1 && c.abi == FX2_ABI_ID &&
               !c.hot && !c.nhot && !c.rows && !c.nrows && c.max_inst == 1 && !c.ram_state && !c.ram_extra && !c.ram_extra_bytes && c.ram_work_min == (k == 1u) && c.ram_work_max == (k == 1u) &&
               c.param_bytes == 2u * G_COUNT && c.flags == FXK_TAIL_SILENT && c.rates == 1 && calls[k] >= link + FW_MOD_CODE_OFF && calls[k] < link + ILEN);
            v = link;
            snprintf(t, sizeof t, "%s: its pages pass the checker as SC_FX out of the real bytes; the condition word is TYPE == %u", NM[k], (unsigned)k + 1u);
            chk(t, fw_pages_check_ex(rd_img, &v, pages, link + FW_MOD_CODE_OFF, link + ILEN, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK && rd32(IMG, pages - link + 76) == ((G_RTYPE + 1u) | ((k + 1u) << 16)));
            /* the Release 2 import through the real bytes */
            memset(&song, 0, sizeof song);
            song.g[G_RTYPE] = (int16_t)(k + 1u);
            for (i = 0; i < 7u; i++) song.g[G7[i]] = KN[k][i];
            memset(val, 0x55, sizeof val);
            ok = fx_legacy_import(rd_img, &v, pages, trk[0].p, song.g, val) == 7u;
            for (i = 0; i < 7u; i++) ok &= val[i] == KN[k][i];
            snprintf(t, sizeof t, "%s: the Release 2 import out of the real descriptor: with TYPE %s its seven values are song.g's SIZE, %s, %s, MOD, RATE, FILT, WIDE", NM[k], NM[k], k == 2u ? "FOLW" : "DAMP",
                     k == 0u ? "PRE" : k == 1u ? "AIR" : "PITCH");
            chk(t, ok);
            song.g[G_RTYPE] = (int16_t)((k + 1u) % 3u + 1u);                               /* another module mode selected */
            ok = fx_legacy_import(rd_img, &v, pages, trk[0].p, song.g, val) == 7u;
            for (i = 0; i < 7u; i++) ok &= val[i] == DEF[k][i];
            snprintf(t, sizeof t, "%s: .. and with another TYPE selected it holds its defaults (the stored byte was not this mode's)", NM[k]);
            chk(t, ok);
        }
        chk("the three calls are three different functions", calls[0] != calls[1] && calls[1] != calls[2] && calls[0] != calls[2]);
    }
    /* ---- 3. the install ---- */
    {
        int r;
        uint32_t st, st2;
        clean(); load(0);
        st = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u);
        chk("the planner names a start for a one sector module of id 2 on an empty area", (int)st >= 0 && st < OC_MOD_STARTS);
        load_at(st);
        r = install(FX2_MODULE_ID, st);
        chk("install by module id 2 at the planned start: begin, the writes of 256 B, end (the header page last): done; registered; the flash holds the image byte for byte",
            r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == FX2_MODULE_ID && oc_mod.state[st] == MD_OK && !memcmp(nor + oc_mod_flash(st), IMG, ILEN));
        st2 = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u);
        chk("the planner offers a start beside the module that stands (not in place)", (int)st2 >= 0 && st2 != st);
        if ((int)st2 >= 0) {
            load_at(st2);
            wr32(44, 0x00010100u); reseal();                 /* version 1.1.0 in the header */
            r = install(FX2_MODULE_ID, st2);
            chk("a re-install of id 2 (the header page's version 1.1.0): accepted, the old copy gone, one entry in the Effects table", r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
        }
        {   /* beside DUB TAPE's id: a second id takes its own entry */
            chk("the module's id is not DUB TAPE's: the Effects table would hold both (ids 1 and 2)", FX2_MODULE_ID != FX_MODK_ID && oc_fx_find(FX_MODK_ID) < 0);
        }
    }
    /* ---- 4. each mutation is refused ---- */
    {
        static const struct { const char *what; int where; uint32_t off; uint32_t v; int bytes; } M[] = {   /* where: 0 the caps block of entry 1 (SHIMMER), 1 its call table, 2 entry 2, 3 entry 1 */
            { "a caps block's magic", 0, 0, 0, 4 }, { "a caps block's version", 0, 6, 2, 2 }, { "the abi of the INSERT host (FX_ABI_ID: a kind 1 module's)", 0, 52, FX_ABI_ID, 4 },
            { "another abi (FX2_ABI_ID + 1)", 0, 52, FX2_ABI_ID + 1u, 4 }, { "a hot set", 0, 16, 0x20C4800u, 4 }, { "the call table past the image", 0, 8, 0x20F8000u, 4 },
            { "a call that is 0", 1, 0, 0, 4 }, { "a call outside the image", 1, 0, 0x20F8000u, 4 }, { "the pages pointer past the image", 0, 12, 0x20F8000u, 4 }, { "a snapshot size of 0", 0, 38, 0, 2 },
            { "an entry of kind 1 in a kind 2 module", 3, 1, 1, 1 }, { "an effect id twice in the bank", 2, 0, 1, 1 }, { "an entry's reserved flags", 2, 2, 1, 1 },
        };
        uint32_t k, s = 6u < OC_MOD_STARTS ? 6u : 0u, link0 = oc_mod_link(s), desc, ent, caps, kern;
        for (k = 0; k < sizeof M / sizeof M[0]; k++) {
            char t[260];
            int r_end;
            uint32_t at;
            clean(); load_at(s);
            desc = rd32(IMG, 28) - link0; ent = rd32(IMG, desc + 12) - link0; caps = rd32(IMG, ent + 16u + 12u) - link0; kern = rd32(IMG, caps + 8) - link0;
            at = M[k].where == 0 ? caps : M[k].where == 1 ? kern : M[k].where == 2 ? ent + 32u : ent + 16u;
            if (M[k].bytes == 4) wr32(at + M[k].off, M[k].v);
            else if (M[k].bytes == 2) { IMG[at + M[k].off] = (uint8_t)M[k].v; IMG[at + M[k].off + 1] = (uint8_t)(M[k].v >> 8); }
            else IMG[at + M[k].off] = (uint8_t)M[k].v;
            reseal();
            memcpy(nor + oc_mod_flash(s), IMG, ILEN);
            oc_mod_boot(0);
            snprintf(t, sizeof t, "a mutated image (%s): the boot scan refuses it (BAD_HDR), the Effects table is empty, the send host holds nothing; the card's and the page's check (oc_fxmod_image_check) and the install's end refuse it", M[k].what);
            ok = oc_mod.state[s] == MD_BAD_HDR && oc_mod.nfxmod == 0 && !fx2_modk_ok();
            clean();
            {   uint32_t mid = 0;
                ok &= oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) != 0;
            }
            r_end = install(FX2_MODULE_ID, s);
            ok &= r_end != 0 && oc_mod.nfxmod == 0 && nor[oc_mod_flash(s)] == 0xFF;
            chk(t, ok);
        }
    }
    /* ---- 5. the unmutated image is accepted by the same check ---- */
    {
        uint32_t s = 6u < OC_MOD_STARTS ? 6u : 0u, mid = 0;
        clean(); load_at(s);
        chk("oc_fxmod_image_check accepts the real image (as the card and the page ask before the first erase) and names its module id 2", oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) == 0 && mid == FX2_MODULE_ID);
    }
    /* ---- 6. the send host through the loader: the registration, the guard, the rebuild, the ban ---- */
    {
        uint32_t st, link, other;
        int r;
        clean(); load(0);
        st = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u); load_at(st);
        r = install(FX2_MODULE_ID, st);
        link = oc_mod_link(st);
        reg_from_image(link);
        chk("after the install nothing is registered with the send host on the host build (the device's oc_fx2mod_reg reads 4 byte pointers; the resident copy plays)", r == 0 && !fx2_modk_ok());
        fm1_guard_code_end = link;                           /* a window that ends where the module begins */
        r = oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
        chk("registration (oc_fx2mod_commit with the caps blocks out of the image's bytes): the window is widened to the module's last byte BEFORE the host takes the calls; the three modes are the module's; why 0",
            r == FX2M_OK && fm1_guard_code_end >= link + ILEN - 1u && fx2_modk_ok() && fx2_modk[1].process == never && fx2_modk[2].process == never && fx2_modk[3].process == never && !fx2_modk[0].process &&
            !fx2_modk[4].process && oc_fx2_start == st && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0);
        oc_mod_boot(0);
        chk("an unrelated rebuild of the registry (an engine module's install or removal ends in oc_mod_boot) keeps the kernels: the same module stands at the same start", fx2_modk_ok() && fx2_modk[2].process == never && oc_mod.nfxmod == 1);
        fx2_marked = 1; oc_mod_boot(0);
        chk("... and the crash mark is taken again after it (the mark flag is cleared: the next bus start that runs a module mode notes the use)", fx2_marked == 0 && fx2_modk_note == oc_fxmod_note_use);
        {   fx_caps_t keep = reg_caps[1];
            fx2_modk_clear();
            reg_caps[1].abi = FX_ABI_ID;
            r = oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
            chk("a refusal (a caps block with the insert host's abi): the module stays listed, state OK, why 0x41 (0x40 + FX2M_VER) in the Effects row and in what FX_TABLE sends; nothing registered: the resident copy plays",
                r == FX2M_VER && !fx2_modk_ok() && oc_mod.state[st] == MD_OK && oc_mod.why[st] == 0x41 && oc_mod.fx[0].why == 0x41);
            reg_caps[1] = keep; reg_caps[1].ram_work_min = 0; reg_caps[1].ram_work_max = 0;
            r = oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
            chk("a refusal (SHIMMER declaring no line block): FX2M_RANGE, why 0x42, ALL OR NONE: SPRING and RESO are not taken either", r == FX2M_RANGE && oc_mod.why[st] == 0x42 && !fx2_modk_ok() && !fx2_modk[1].process);
            reg_caps[1] = keep;
            r = oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
            chk("the next good registration clears the reason", r == 0 && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0 && fx2_modk_ok());
        }
        other = OC_MOD_START_SECTOR(st) + 1u < OC_MOD_AREA ? OC_MOD_START_SECTOR(st) + 1u : OC_MOD_START_SECTOR(st) - 1u;
        (void)oc_mod_erase_span((int32_t)other, 1u);
        chk("guard: an erase of a sector outside the module's span leaves its kernels registered", fx2_modk_ok());
        (void)oc_mod_erase_span(OC_MOD_START_SECTOR(st), 1u);
        chk("guard: an erase of the module's sector lets the kernels go FIRST (fx2_modk_clear: the resident copy plays from the next bus job)", !fx2_modk_ok() && !fx2_modk[1].process && !fx2_modk[2].process && !fx2_modk[3].process);
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u); load_at(st);
        r = install(FX2_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
        r |= oc_fxmod_remove(FX2_MODULE_ID);
        chk("guard: the removal (the editor's command 132, the page's) lets go of the kernels before the first erase; the Effects table is empty", r == 0 && !fx2_modk_ok() && oc_mod.nfxmod == 0);
        /* a newer version beside: the old copy is erased at the end of the install, under the registered kernels */
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u); load_at(st);
        r = install(FX2_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
        {   uint32_t st2 = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u), kb;
            load_at(st2);
            wr32(44, 0x00010100u); reseal();
            r |= (int)st2 < 0 || st2 == st;
            r |= oc_fxmod_begin(FX2_MODULE_ID, st2, ILEN);
            kb = (uint32_t)fx2_modk_ok();
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < ILEN; o += 256u) r = oc_mod_write(o, IMG + o, ILEN - o > 256u ? 256u : ILEN - o);
            chk("guard: a newer version being written beside leaves the old copy's kernels registered (its sector is not touched)", r == 0 && kb == 1u && fx2_modk_ok());
            r |= oc_mod_end(IMG);
            chk("guard: ... and at the end of the install, when the old copy is erased, the host has let go first; the new copy is the only module (on the device it registers at once: oc_mod_boot -> oc_fx2mod_reg)",
                r == 0 && !fx2_modk_ok() && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
        }
        /* the ban: a session that died with the module in use, three times */
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX2_MODULE_ID, 1u); load_at(st);
        r = install(FX2_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx2mod_commit(st, FX2_MODULE_ID, 0x00010000u, 3, reg);
        oc_fxmod_note_use(FX2_MODULE_ID);
        chk("the crash mark: a module mode in use sets the module's inuse bit (oc_fxmod_note_use, which the send host calls at the bus start)", r == 0 && (oc_modguard.inuse & (1u << st)) != 0);
        oc_mod_boot(1);                                      /* the session died with it in use */
        chk("a crash with the module in use: the module is banned (MD_CRASHED), not in the Effects table, the send host holds nothing: the resident copy plays",
            oc_mod.state[st] == MD_CRASHED && oc_mod.nfxmod == 0 && oc_fx_find(FX2_MODULE_ID) < 0 && !fx2_modk_ok());
    }
    printf(bad ? "fx reverb image: FAILED\n" : "fx reverb image: all passed\n");
    return bad != 0;
}
