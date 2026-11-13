/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8b (docs/orangecat/FX_MODULES_PLAN.md 17): the REAL FX layer module (build/module/fxlayer.<start>.img, the images tools/build.py --module fxlayer linked with the device
 * toolchain and proved) through the loader and the install path, on the simulated NOR of tests/persistence_test.c, as tests/fx_reverb_image_test.c does for the reverb module. The host cannot
 * run the module's code (tests/fx_layer_module_test.c runs the same source as a host object); what it does is everything the firmware does with the image's BYTES:
 *   1  boot-scan at every start of this build's grid: registered in the Effects table (module 3, version 1.0.0, kind 3, a bank of four), no engine slot, its sectors, the PC guard to its end
 *   2  the bank, the four caps blocks, their call tables (two words each: fx3_kcalls_t) and pages out of the flash through the loader's reader; the pages pass the checker as SC_FX; the
 *      project stores nothing of them and a Release 2 project imports nothing (volatile and relative ids: fw_fxstore.h through the REAL bytes)
 *   3  the install by module id (plan, begin, write, end), a re-install beside
 *   4  each mutation refused at the boot scan, by the page's and the card's check (oc_fxmod_image_check) and at the install's end, nothing programmed: the caps block (magic, version, the abi of
 *      the INSERT host and of the SEND host, a hot set, the call table out of the image, a call that is 0, the pages out of the image, a snapshot size of 0), an entry of another kind, an
 *      effect id twice
 *   5  the unmutated image accepted by the same check
 *   6  the master host's registration through the loader (oc_fx3mod_commit): the window first, a refusal is the Effects row's why; an unrelated rebuild keeps the kernels; the erase guard lets
 *      go before any erase of the module's sectors (a sector under it, a removal, a newer version beside) and not for an erase elsewhere; a banned module is not registered
 *   build/host/fx_layer_image_test build/module
 * Built with OC_MODULE_WINS 1 as fx_module_test (tests/run_tests.sh). Without the images: SKIPPED (FXLI_REQUIRE=1: a failure). */
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
    printf("fx layer image: %-150s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint8_t IMG[0x2000u];
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
    fx3_modk_clear();
    fm1_guard_code_end = 0;
    ocs_resv = SMP_LEGACY_MASK;
    fail_after = -1; erase_fail_after = -1;
}
static void wr32(uint32_t off, uint32_t v) { IMG[off] = (uint8_t)v; IMG[off + 1] = (uint8_t)(v >> 8); IMG[off + 2] = (uint8_t)(v >> 16); IMG[off + 3] = (uint8_t)(v >> 24); }
static int load(int32_t sector)                          /* the image linked for the signed sector `sector` of the grid (build.py names its files by it) */
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/fxlayer.%d.img", dir, (int)sector);
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
static void never_start(const fx3_ctx_t *c) { (void)c; }                                                       /* host addresses for calls this test never makes */
static uint32_t never_run(const fx3_ctx_t *c, int32_t *l, int32_t *r, uint32_t n) { (void)c; (void)l; (void)r; return n; }
static const fx3_kcalls_t NEVER = { never_start, never_run };
/* the bank as the device's oc_fx3mod_reg reads it, with the caps blocks copied out of the image's bytes and host calls in place of the module's code */
static fx_caps_t reg_caps[4];
static fx3_reg_t reg[4];
static void reg_from_image(uint32_t link)
{
    uint32_t desc = rd32(IMG, 28) - link, ent = rd32(IMG, desc + 12) - link, k;
    for (k = 0; k < 4u; k++) {
        memcpy(&reg_caps[k], IMG + (rd32(IMG, ent + 16u * k + 12u) - link), sizeof reg_caps[k]);
        reg[k].effect_id = IMG[ent + 16u * k]; reg[k].caps = &reg_caps[k]; reg[k].calls = &NEVER;
    }
}
static int all_never(void) { uint32_t k, ok = 1; for (k = 0; k < FX3_FX; k++) ok &= fx3_modk[k].run == never_run && fx3_modk[k].start == never_start; return (int)ok; }
static int none_reg(void) { uint32_t k, ok = !fx3_modk_ok(); for (k = 0; k < FX3_FX; k++) ok &= !fx3_modk[k].run && !fx3_modk[k].start; return (int)ok; }
int main(int argc, char **argv)
{
    uint32_t i, ok, nstart = 0, nomiss = 0, NSEC;
    dir = argc > 1 ? argv[1] : "build/module";
    if (!load(0)) {
        printf("fx layer image: %s/fxlayer.0.img is not there (build it: tools/build.py --module fxlayer): SKIPPED%s\n", dir, getenv("FXLI_REQUIRE") ? " (FXLI_REQUIRE set: FAILED)" : "");
        return getenv("FXLI_REQUIRE") ? 2 : 0;
    }
    NSEC = (ILEN + 4095u) / 4096u;
    /* ---- 1. every start of the grid ---- */
    ok = 1;
    for (i = 0; i < OC_MOD_STARTS; i++) {
        uint32_t e;
        clean();
        if (!load_at(i)) {                               /* (a start whose span would leave its place has no variant and no image: the last sector of the area and of the tail for two sectors;
                                                          * a build without the tail region links the two sector grid only, every other start: tools/build.py module_starts) */
            if (oc_span_ok(OC_MOD_START_SECTOR(i), NSEC)) nomiss++;
            continue;
        }
        memcpy(nor + oc_mod_flash(i), IMG, ILEN);
        oc_mod_boot(0);
        e = oc_mod.state[i] == MD_OK && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == FX3_MODULE_ID && oc_mod.fx[0].version == 0x00010000u && oc_mod.fx[0].kind == FW_MOD_KIND_MASTER && oc_mod.fx[0].nfx == 4 &&
            oc_mod.fx[0].start == i && oc_fx_find(FX3_MODULE_ID) == 0 && fm1_guard_code_end == oc_mod_link(i) + ILEN - 1u && oc_mod_nsec(ILEN) == NSEC && ILEN == oc_mod_hdr_ptr(i)->length &&
            oc_span_ok(OC_MOD_START_SECTOR(i), NSEC);
        for (uint32_t g = 0; g < NENGINES; g++) e &= !eng_mod[g];
        if (!e) printf("  start %u (sector %d): state %u why %u nfx %u\n", (unsigned)i, (int)OC_MOD_START_REL(i), oc_mod.state[i], oc_mod.why[i], oc_mod.nfxmod);
        ok &= e; nstart++;
    }
    {   char t[260];
        uint32_t want = 0;
        for (i = 0; i < OC_MOD_STARTS; i++) want += oc_span_ok(OC_MOD_START_SECTOR(i), NSEC) != 0;
        snprintf(t, sizeof t, "every start the build made an image for (%u; %u of this geometry's %u starts hold the span, %u of them have no image: 0 in a tail build's packages, the odd sectors otherwise): "
                 "the image boot-scans good, is the Effects table's module 3 (version 1.0.0, kind 3, a bank of four), takes no engine slot, is %u sector(s) (%u B; the flash map counts on 2), the PC guard reaches its end",
                 (unsigned)nstart, (unsigned)want, (unsigned)OC_MOD_STARTS, (unsigned)nomiss, (unsigned)NSEC, (unsigned)ILEN);
        chk(t, ok && nstart + nomiss == want && nstart >= 9u && (nomiss == 0u || nstart == 9u) && NSEC <= 2u);
    }
    /* ---- 2. the bank out of the flash ---- */
    {
        uint32_t s = 6u < OC_MOD_STARTS ? 6u : 0u, link, desc, ent, k, calls[8];
        clean(); load_at(s);
        memcpy(nor + oc_mod_flash(s), IMG, ILEN);
        oc_mod_boot(0);
        link = oc_mod_link(s);
        desc = rd32(IMG, 28) - link; ent = rd32(IMG, desc + 12) - link;
        chk("the descriptor: n 4, module id 3, version 1.0.0, no scene; the header: kind 3 with no engine number (0x300), state size 0",
            rd32(IMG, desc) == 4u && rd32(IMG, desc + 4) == 3u && rd32(IMG, desc + 8) == 0x00010000u && rd32(IMG, desc + 16) == 0 && rd32(IMG, 20) == 0x300u && rd32(IMG, 24) == 0);
        for (k = 0; k < 4u; k++) {
            static const char *const NM[4] = { "GLITCH", "TAPE STOP", "SHUFFLE", "FREEZE" };
            static const uint8_t NK[4] = { 4, 3, 4, 4 };
            uint32_t e = ent + 16u * k, caps = rd32(IMG, e + 12) - link, pages, v, pg, put;
            int16_t val[8];
            uint8_t out[64];
            fx_caps_t c;
            char t[300];
            memcpy(&c, IMG + caps, sizeof c);
            pages = c.pages; pg = pages - link;
            calls[2u * k] = rd32(IMG, c.kernel - link); calls[2u * k + 1u] = rd32(IMG, c.kernel - link + 4u);
            snprintf(t, sizeof t, "%s: entry %u is effect %u, kind 3, flags 0, no parameter table; its caps block: GFXK, 56 bytes, version 1, abi = this build's FX3_ABI_ID, no hot set, no rows, one instance, no state of its own, "
                     "8 working blocks (the 32 KB), the snapshot 4 bytes; its TWO calls (start, run) are addresses inside the code", NM[k], (unsigned)k, (unsigned)k + 1u);
            chk(t, IMG[e] == k + 1u && IMG[e + 1] == 3u && !IMG[e + 2] && !IMG[e + 3] && !IMG[e + 4] && !rd32(IMG, e + 8) && c.magic == FX_CAPS_MAGIC && c.size == 56 && c.version == 1 && c.abi == FX3_ABI_ID &&
               !c.hot && !c.nhot && !c.rows && !c.nrows && c.max_inst == 1 && !c.ram_state && !c.ram_extra && !c.ram_extra_bytes && c.ram_work_min == 8 && c.ram_work_max == 8 &&
               c.param_bytes == 4u && c.flags == 0 && c.rates == 1 && calls[2u * k] >= link + FW_MOD_CODE_OFF && calls[2u * k] < link + ILEN && calls[2u * k + 1u] >= link + FW_MOD_CODE_OFF && calls[2u * k + 1u] < link + ILEN);
            v = link;
            put = rd32(IMG, pg + 28);
            snprintf(t, sizeof t, "%s: its page passes the checker as SC_FX out of the real bytes: one page, %u panel entries, no condition word, no shown hook, %s", NM[k], NK[k],
                     k == 1u ? "SCRATCH's put hook an address inside the code" : "no put hook");
            chk(t, fw_pages_check_ex(rd_img, &v, pages, link + FW_MOD_CODE_OFF, link + ILEN, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK && (rd32(IMG, pg + 8) & 0xFFFFu) == 1u && (rd32(IMG, pg + 8) >> 16) == NK[k] &&
               rd32(IMG, pg + 76) == 0 && rd32(IMG, pg + 24) == 0 && (k == 1u ? put >= link + FW_MOD_CODE_OFF && put < link + ILEN : put == 0));
            /* nothing of it is a project's: no Release 2 slot to import, nothing stored */
            memset(&song, 0, sizeof song);
            memset(val, 0x55, sizeof val);
            ok = fx_legacy_import(rd_img, &v, pages, trk[0].p, song.g, val) == NK[k];
            {   int16_t vals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
                ok &= fx_store_pack(rd_img, &v, pages, vals, out, sizeof out) == 0;
            }
            snprintf(t, sizeof t, "%s: out of the real descriptor: the project's writer stores none of its values (volatile and relative ids) and a Release 2 project imports only its defaults (no legacy slot)", NM[k]);
            {   static const int16_t DEF[4][4] = {{50, 0, 0, 1}, {0, 4, 0, 0}, {60, 2, 20, 1}, {50, 0, 30, 2}};
                for (i = 0; i < NK[k]; i++) ok &= val[i] == DEF[k][i];
            }
            chk(t, ok);
        }
        ok = 1;
        for (i = 0; i < 8u; i++) for (k = 0; k < i; k++) ok &= calls[i] != calls[k];
        chk("the eight calls are eight different functions", ok);
    }
    /* ---- 3. the install ---- */
    {
        int r;
        uint32_t st, st2;
        clean(); load(0);
        st = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC);
        chk("the planner names a start for the module of id 3 on an empty area", (int)st >= 0 && st < OC_MOD_STARTS);
        load_at(st);
        r = install(FX3_MODULE_ID, st);
        chk("install by module id 3 at the planned start: begin, the writes of 256 B, end (the header page last): done; registered; the flash holds the image byte for byte",
            r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == FX3_MODULE_ID && oc_mod.state[st] == MD_OK && !memcmp(nor + oc_mod_flash(st), IMG, ILEN));
        st2 = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC);
        chk("the planner offers a start beside the module that stands (not in place)", (int)st2 >= 0 && st2 != st);
        if ((int)st2 >= 0) {
            load_at(st2);
            wr32(44, 0x00010100u); reseal();                 /* version 1.1.0 in the header */
            r = install(FX3_MODULE_ID, st2);
            chk("a re-install of id 3 (the header page's version 1.1.0): accepted, the old copy gone, one entry in the Effects table", r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
        }
        chk("the module's id is not DUB TAPE's nor the reverb's: the Effects table would hold the three (ids 1, 2 and 3)", FX3_MODULE_ID != FX_MODK_ID && FX3_MODULE_ID != FX2_MODULE_ID && oc_fx_find(FX_MODK_ID) < 0 && oc_fx_find(FX2_MODULE_ID) < 0);
    }
    /* ---- 4. each mutation is refused ---- */
    {
        static const struct { const char *what; int where; uint32_t off; uint32_t v; int bytes; } M[] = {   /* where: 0 the caps block of entry 1 (TAPE STOP), 1 its call table, 2 entry 2, 3 entry 1 */
            { "a caps block's magic", 0, 0, 0, 4 }, { "a caps block's version", 0, 6, 2, 2 }, { "the abi of the INSERT host (FX_ABI_ID: a kind 1 module's)", 0, 52, FX_ABI_ID, 4 },
            { "the abi of the SEND host (FX2_ABI_ID: a kind 2 module's)", 0, 52, FX2_ABI_ID, 4 },
            { "another abi (FX3_ABI_ID + 1)", 0, 52, FX3_ABI_ID + 1u, 4 }, { "a hot set", 0, 16, 0x20C4800u, 4 }, { "the call table past the image", 0, 8, 0x20F8000u, 4 },
            { "a start call that is 0", 1, 0, 0, 4 }, { "a run call that is 0", 1, 4, 0, 4 }, { "a run call outside the image", 1, 4, 0x20F8000u, 4 }, { "the pages pointer past the image", 0, 12, 0x20F8000u, 4 },
            { "a snapshot size of 0", 0, 38, 0, 2 },
            { "an entry of kind 2 in a kind 3 module", 3, 1, 2, 1 }, { "an effect id twice in the bank", 2, 0, 1, 1 }, { "an entry's reserved flags", 2, 2, 1, 1 },
        };
        uint32_t k, s = 6u < OC_MOD_STARTS ? 6u : 0u, link0 = oc_mod_link(s), desc, ent, caps, kern;
        for (k = 0; k < sizeof M / sizeof M[0]; k++) {
            char t[280];
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
            snprintf(t, sizeof t, "a mutated image (%s): the boot scan refuses it (BAD_HDR), the Effects table is empty, the master host holds nothing; the card's and the page's check (oc_fxmod_image_check) and the install's end refuse it", M[k].what);
            ok = oc_mod.state[s] == MD_BAD_HDR && oc_mod.nfxmod == 0 && none_reg();
            clean();
            {   uint32_t mid = 0;
                ok &= oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) != 0;
            }
            r_end = install(FX3_MODULE_ID, s);
            ok &= r_end != 0 && oc_mod.nfxmod == 0 && nor[oc_mod_flash(s)] == 0xFF;
            chk(t, ok);
        }
    }
    /* ---- 5. the unmutated image is accepted by the same check ---- */
    {
        uint32_t s = 6u < OC_MOD_STARTS ? 6u : 0u, mid = 0;
        clean(); load_at(s);
        chk("oc_fxmod_image_check accepts the real image (as the card and the page ask before the first erase) and names its module id 3", oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) == 0 && mid == FX3_MODULE_ID);
    }
    /* ---- 6. the master host through the loader: the registration, the guard, the rebuild, the ban ---- */
    {
        uint32_t st, link, other;
        int r;
        clean(); load(0);
        st = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC); load_at(st);
        r = install(FX3_MODULE_ID, st);
        link = oc_mod_link(st);
        reg_from_image(link);
        chk("after the install nothing is registered with the master host on the host build (the device's oc_fx3mod_reg reads 4 byte pointers; the resident copy plays)", r == 0 && none_reg());
        fm1_guard_code_end = link;                           /* a window that ends where the module begins */
        r = oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
        chk("registration (oc_fx3mod_commit with the caps blocks out of the image's bytes): the window is widened to the module's last byte BEFORE the host takes the calls; the four effects are the module's; why 0",
            r == FX3M_OK && fm1_guard_code_end >= link + ILEN - 1u && fx3_modk_ok() && all_never() && fx3_mod_start == st && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0);
        oc_mod_boot(0);
        chk("an unrelated rebuild of the registry (an engine module's install or removal ends in oc_mod_boot) keeps the kernels: the same module stands at the same start", fx3_modk_ok() && all_never() && oc_mod.nfxmod == 1);
        fx3_marked = 1; oc_mod_boot(0);
        chk("... and the crash mark is taken again after it (the mark flag is cleared: the module's next call notes the use)", fx3_marked == 0 && fx3_modk_note == oc_fxmod_note_use);
        {   fx_caps_t keep = reg_caps[1];
            fx3_modk_clear();
            reg_caps[1].abi = FX2_ABI_ID;
            r = oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
            chk("a refusal (a caps block with the send host's abi): the module stays listed, state OK, why 0x41 (0x40 + FX3M_VER) in the Effects row and in what FX_TABLE sends; nothing registered: the resident copy plays",
                r == FX3M_VER && none_reg() && oc_mod.state[st] == MD_OK && oc_mod.why[st] == 0x41 && oc_mod.fx[0].why == 0x41);
            reg_caps[1] = keep; reg_caps[1].ram_work_min = 7;
            r = oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
            chk("a refusal (TAPE STOP declaring 7 working blocks): FX3M_RANGE, why 0x42, ALL OR NONE: GLITCH, SHUFFLE and FREEZE are not taken either", r == FX3M_RANGE && oc_mod.why[st] == 0x42 && none_reg());
            reg_caps[1] = keep;
            r = oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
            chk("the next good registration clears the reason", r == 0 && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0 && fx3_modk_ok());
        }
        other = OC_MOD_START_SECTOR(st) + NSEC < OC_MOD_AREA ? OC_MOD_START_SECTOR(st) + NSEC : OC_MOD_START_SECTOR(st) - 1u;
        (void)oc_mod_erase_span((int32_t)other, 1u);
        chk("guard: an erase of a sector outside the module's span leaves its kernels registered", fx3_modk_ok() && all_never());
        (void)oc_mod_erase_span(OC_MOD_START_SECTOR(st) + (int32_t)NSEC - 1, 1u);
        chk("guard: an erase of the module's LAST sector lets the kernels go FIRST (fx3_modk_clear: the resident copy plays from the next block)", none_reg());
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC); load_at(st);
        r = install(FX3_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
        r |= oc_fxmod_remove(FX3_MODULE_ID);
        chk("guard: the removal (the editor's command 132, the page's) lets go of the kernels before the first erase; the Effects table is empty", r == 0 && none_reg() && oc_mod.nfxmod == 0);
        /* a newer version beside: the old copy is erased at the end of the install, under the registered kernels */
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC); load_at(st);
        r = install(FX3_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
        {   uint32_t st2 = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC), kb;
            load_at(st2);
            wr32(44, 0x00010100u); reseal();
            r |= (int)st2 < 0 || st2 == st;
            r |= oc_fxmod_begin(FX3_MODULE_ID, st2, ILEN);
            kb = (uint32_t)fx3_modk_ok();
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < ILEN; o += 256u) r = oc_mod_write(o, IMG + o, ILEN - o > 256u ? 256u : ILEN - o);
            chk("guard: a newer version being written beside leaves the old copy's kernels registered (its sectors are not touched)", r == 0 && kb == 1u && fx3_modk_ok());
            r |= oc_mod_end(IMG);
            chk("guard: ... and at the end of the install, when the old copy is erased, the host has let go first; the new copy is the only module (on the device it registers at once: oc_mod_boot -> oc_fx3mod_reg)",
                r == 0 && none_reg() && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
        }
        /* the reverb module's guard and this one's do not touch each other's kernels */
        clean(); load(0); st = (uint32_t)oc_fxmod_plan(FX3_MODULE_ID, NSEC); load_at(st);
        r = install(FX3_MODULE_ID, st); reg_from_image(oc_mod_link(st));
        r |= oc_fx3mod_commit(st, FX3_MODULE_ID, 0x00010000u, 4, reg);
        chk("the send host (the reverb module's kernels) holds nothing when this module registers: the two hosts are separate", r == 0 && !fx2_modk_ok() && fx3_modk_ok());
        /* the ban: a session that died with the module in use */
        oc_fxmod_note_use(FX3_MODULE_ID);
        chk("the crash mark: a ring effect in use sets the module's inuse bit (oc_fxmod_note_use, which the master host calls before the module's first call)", (oc_modguard.inuse & (1u << st)) != 0);
        oc_mod_boot(1);                                      /* the session died with it in use */
        chk("a crash with the module in use: the module is banned (MD_CRASHED), not in the Effects table, the master host holds nothing: the resident copy plays",
            oc_mod.state[st] == MD_CRASHED && oc_mod.nfxmod == 0 && oc_fx_find(FX3_MODULE_ID) < 0 && none_reg());
    }
    printf(bad ? "fx layer image: FAILED\n" : "fx layer image: all passed\n");
    return bad != 0;
}
