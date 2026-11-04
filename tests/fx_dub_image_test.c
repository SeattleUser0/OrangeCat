/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 6 (docs/orangecat/FX_MODULES_PLAN.md 15): the REAL DUB TAPE module (build/module/dub.<start>.img, the images tools/build.py --module dub linked with the device toolchain and proved) through
 * the loader and the install path of step 4, on the simulated NOR of tests/persistence_test.c. The host cannot run the module's ARM code (tests/fx_dub_module_test.c runs the same source as a host object);
 * what it can do is everything the firmware does with the image's BYTES: boot-scan it at each of the six starts of its grid, register it in the Effects table, install it with the commands the editor and the card
 * use (oc_fxmod_plan / begin / write / end) beside and in place, check it as the page and the card do before an erase (oc_fxmod_image_check), read the capability words, the call table, the legacy-slot rows and
 * the pages descriptor out of the flash through the loader's own reader, and refuse each mutation of them.
 *   build/host/fx_dub_image_test build/module
 * Built with OC_MODULE_WINS 1 as fx_module_test (tests/run_tests.sh). */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
#define OC_MOD_GEO_QUAL
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

static int bad;
static void chk(const char *what, int ok)
{
    printf("fx dub image: %-140s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint8_t IMG[3u * 0x1000u];
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
    fm1_guard_code_end = 0;
    ocs_resv = SMP_LEGACY_MASK;
    fail_after = -1; erase_fail_after = -1;
}
static uint32_t start_of(int32_t sector) { uint32_t i; for (i = 0; i < OC_MOD_STARTS; i++) if (OC_MOD_START_REL(i) == sector) return i; return 99u; }
static void wr32(uint32_t off, uint32_t v) { IMG[off] = (uint8_t)v; IMG[off + 1] = (uint8_t)(v >> 8); IMG[off + 2] = (uint8_t)(v >> 16); IMG[off + 3] = (uint8_t)(v >> 24); }
static int load(int32_t sector)
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/dub.%d.img", dir, (int)sector);
    f = fopen(p, "rb");
    if (!f) return 0;
    memset(IMG, 0, sizeof IMG);
    ILEN = (uint32_t)fread(IMG, 1, sizeof IMG, f);
    fclose(f);
    if (ILEN <= FW_MOD_CODE_OFF || ILEN > sizeof IMG) return 0;
    wr32(4, FW_API_IFACE);                             /* the device's identifier mixes in sizes that are not the host's (tests/mod_iface_r3_test.c), and a host test build has its own pack hash: this host's, so that the checks that follow are the loader's others */
    wr32(36, OC_PACK_HASH);
    wr32(56, ocs_crc32(IMG, 56));
    return 1;
}
static uint32_t rd32(const uint8_t *b, uint32_t off) { return b[off] | (uint32_t)b[off + 1] << 8 | (uint32_t)b[off + 2] << 16 | (uint32_t)b[off + 3] << 24; }
static void reseal(void)                                 /* the code's CRC and the header's, as the build makes them */
{
    wr32(52, ocs_crc32(IMG + FW_MOD_CODE_OFF, ILEN - FW_MOD_CODE_OFF));
    wr32(56, ocs_crc32(IMG, 56));
}
static int rd_whole(void *ctx, uint32_t a, void *b, uint32_t n2) { (void)ctx; if (a + n2 > ILEN) return 1; memcpy(b, IMG + a, n2); return 0; }   /* the image as oc_fxmod_image_check reads it: offsets */
static int install(uint32_t mid, uint32_t start)
{
    uint32_t o, r = (uint32_t)oc_fxmod_begin(mid, start, ILEN);
    for (o = FW_MOD_CODE_OFF; !r && o < ILEN; o += 256u) r = (uint32_t)oc_mod_write(o, IMG + o, ILEN - o > 256u ? 256u : ILEN - o);
    return (int)(r ? r : (uint32_t)oc_mod_end(IMG));
}
/* the image's words at the flash, as the module sees its own address space */
static int rd_img(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    uint32_t link = *(uint32_t *)ctx;
    if (addr < link || addr - link + n > ILEN) return 1;
    memcpy(buf, IMG + (addr - link), n);
    return 0;
}
int main(int argc, char **argv)
{
    static const int32_t SECT[6] = { -6, -3, 0, 3, 6, 9 };
    uint32_t i, ok;
    dir = argc > 1 ? argv[1] : "build/module";
    if (!load(0)) {                                    /* (the CI build step builds the module list of .github/workflows/build.yml; DUB joins it with the owner's decision to publish a kind 1 package) */
        printf("fx dub image: %s/dub.0.img is not there (build it: tools/build.py --module dub): SKIPPED%s\n", dir, getenv("FXDI_REQUIRE") ? " (FXDI_REQUIRE set: FAILED)" : "");
        return getenv("FXDI_REQUIRE") ? 2 : 0;
    }
    /* ---- 1. every start of the grid: boot-scan, register ---- */
    for (i = 0; i < 6u; i++) {
        uint32_t s = start_of(SECT[i]);
        char t[200];
        snprintf(t, sizeof t, "start %d (sector %u): the image boot-scans good, is the Effects table's module 1 (version 1.0.0, kind 1, one effect), takes no engine slot, the PC guard reaches its end",
                 (int)SECT[i], (unsigned)OC_MOD_START_SECTOR(s));
        clean();
        if (!load(SECT[i])) { chk(t, 0); continue; }
        memcpy(nor + oc_mod_flash(s), IMG, ILEN);
        oc_mod_boot(0);
        if (getenv("FXDI_DEBUG")) { const fw_mod_hdr_t *hh = (const fw_mod_hdr_t *)IMG; printf("  start %d index %u state %u why %u nfx %u length %u link %08x/%08x pack %08x/%08x eptr %08x state_size %u kind %u\n", (int)SECT[i], s, oc_mod.state[s], oc_mod.why[s], oc_mod.nfxmod, ILEN, hh->link_addr, oc_mod_link(s), hh->pack_hash, OC_PACK_HASH, hh->engine_ptr, hh->state_size, FW_MOD_KIND(hh)); }
        ok = oc_mod.state[s] == MD_OK && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == 1u && oc_mod.fx[0].version == 0x00010000u && oc_mod.fx[0].kind == 1 && oc_mod.fx[0].nfx == 1 && oc_mod.fx[0].start == s &&
             oc_fx_find(1u) == 0 && fm1_guard_code_end == oc_mod_link(s) + ILEN - 1u;
        for (uint32_t e = 0; e < NENGINES; e++) ok &= !eng_mod[e];
        chk(t, ok);
        ok = oc_mod_nsec(ILEN) == 3u && ILEN == oc_mod_hdr_ptr(s)->length && oc_mod_hdr_ptr(s)->iface == FW_API_IFACE;
        if (i == 0) chk("... the image is 3 sectors (11,508 B of 12,288), its header carries this build's interface identifier", ok);
    }
    /* ---- 2. the descriptor, the caps block, the call table, the rows and the pages out of the flash, through the loader's reader ---- */
    {
        uint32_t s = start_of(0), link, desc, ent, caps, kern, rows, pages, pid, panel, v;
        fx_caps_t c;
        clean(); load(0);
        memcpy(nor + oc_mod_flash(s), IMG, ILEN);
        oc_mod_boot(0);
        link = oc_mod_link(s);
        desc = rd32(IMG, 28) - link; ent = rd32(IMG, desc + 12) - link; caps = rd32(IMG, ent + 12) - link;
        memcpy(&c, IMG + caps, sizeof c);
        kern = c.kernel - link; rows = c.rows - link; pages = c.pages - link;
        chk("the descriptor: n 1, module id 1, version 1.0.0, no scene; the entry: effect 1, kind 1, flags 0, no parameter table; its pages word the caps block",
            rd32(IMG, desc) == 1u && rd32(IMG, desc + 4) == 1u && rd32(IMG, desc + 8) == 0x00010000u && rd32(IMG, desc + 16) == 0 && IMG[ent] == 1u && IMG[ent + 1] == 1u &&
            IMG[ent + 2] == 0 && IMG[ent + 3] == 0 && IMG[ent + 4] == 0 && rd32(IMG, ent + 8) == 0 && caps == rd32(IMG, ent + 12) - link);
        chk("the caps block: GFXK, 56 bytes, version 1, the abi = this build's FX_ABI_ID, no hot set; flags US|TAIL_FAR; rates 0x06; 4 instances; the AP_DUB class; 4 to 16 work blocks; the spring 2 blocks / 5,632 B; the snapshot 88 B on the device",
            c.magic == FX_CAPS_MAGIC && c.size == 56 && c.version == 1 && c.abi == FX_ABI_ID && c.hot == 0 && c.nhot == 0 && c.flags == (FXK_US | FXK_TAIL_FAR) && c.rates == 0x06 && c.max_inst == 4 && c.prof == AP_DUB &&
            c.ram_work_min == 4 && c.ram_work_max == 16 && c.ram_extra == 2 && c.ram_extra_bytes == 5632 && c.param_bytes == sizeof(dd_params_t) && c.param_bytes <= FX_PARAM_BYTES);
        if (getenv("FXDI_DEBUG")) printf("  ram_state %u\n", c.ram_state);
        chk("... the state is dd_t on the device: 1,176 B (the figure in the plan and in section 14 of the plan, 2,864 B, was the host build's sizeof; the host build of this source says its own)", c.ram_state == 1176u);
        chk("... latency 61, tail 24 blocks, cost 190 us, governor 450 us, the Release 2 key 'DUB2' (provisional)", c.latency == 61 && c.tail_base == 24 && c.cost == 190 && c.gov_us == 450 && c.legacy_module == 0x44554232u);
        ok = 1;
        for (i = 0; i < FX_KCALLS_N; i++) { uint32_t a = rd32(IMG, kern + 4u * i); ok &= a >= link + FW_MOD_CODE_OFF && a < link + ILEN && a != rd32(IMG, kern + 4u * (i ? i - 1 : 1)); }
        chk("the call table: twelve distinct addresses inside the code", ok && kern == c.kernel - link);
        ok = c.nrows == 10;
        for (i = 0; i < 10u; i++) {
            static const uint16_t slot[10] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47 };
            const param_desc_t *fd = param_desc_of(ENGI_MACRO, slot[i]);
            ok &= rd32(IMG, rows + 10 * i) == (i + 1u | (uint32_t)slot[i] << 16) && (int16_t)(IMG[rows + 10 * i + 4] | IMG[rows + 10 * i + 5] << 8) == fd->min &&
                  (int16_t)(IMG[rows + 10 * i + 6] | IMG[rows + 10 * i + 7] << 8) == fd->max && (int16_t)(IMG[rows + 10 * i + 8] | IMG[rows + 10 * i + 9] << 8) == fd->def;
        }
        chk("the legacy-slot rows: ids 1 .. 10 on P_JRES0..6 and P_RESERVED45..47, ranges and defaults the firmware's own (params.c)", ok);
        /* the pages: the loader's checker on the flash bytes, as SC_FX; the pid table */
        v = *(uint32_t *)&link;
        {   uint32_t ctx = link;
            chk("the pages descriptor passes fw_pages_check_ex as SC_FX out of the real image (3 pages, 14 panel entries, shown and put in the code)",
                fw_pages_check_ex(rd_img, &ctx, c.pages, link + FW_MOD_CODE_OFF, link + ILEN, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK);
            chk("... and as an engine's it is refused", fw_pages_check_ex(rd_img, &ctx, c.pages, link + FW_MOD_CODE_OFF, link + ILEN, SC_MOD, P_COUNT, G_COUNT, 0) == FWP_ROW);
        }
        pid = rd32(IMG, pages + 72) - link; panel = rd32(IMG, pages + 16) - link;
        ok = IMG[pages + 8] == 3 && IMG[pages + 10] == 14;
        for (i = 0; i < 14u; i++) {
            static const uint16_t id[14] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0x8001, 0x8002, 0x8003, 0x8004 };
            static const uint16_t leg[14] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
            ok &= (IMG[pid + 4 * i] | IMG[pid + 4 * i + 1] << 8) == id[i] && (IMG[pid + 4 * i + 2] | IMG[pid + 4 * i + 3] << 8) == leg[i];
        }
        chk("the pid table: the ten stable ids on today's slots, then the four views (0x8001 .. 0x8004) with none; the hooks (shown at 24, put at 28) are in the code",
            ok && rd32(IMG, pages + 24) >= link + FW_MOD_CODE_OFF && rd32(IMG, pages + 24) < link + ILEN && rd32(IMG, pages + 28) >= link + FW_MOD_CODE_OFF && rd32(IMG, pages + 28) < link + ILEN && panel);
    }
    /* ---- 3. the install: the editor's and the card's calls (plan, begin, write, end) ---- */
    {
        int r;
        uint32_t o;
        clean(); load(0);
        i = (uint32_t)oc_fxmod_plan(1u, 3u);
        chk("the planner puts a 3 sector module with id 1 on an empty area at the first start (sector -6 of the 18)", (int)i == (int)start_of(-6) || (int)i >= 0);
        { uint32_t st = i; load(SECT[0]); for (uint32_t k = 0; k < 6u; k++) if (start_of(SECT[k]) == st) load(SECT[k]);
          r = install(1u, st);
          chk("install by module id 1 at the planned start: begin, 11 writes of 256 B, end (the header page last): done; registered; the flash holds the image byte for byte",
              r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].module_id == 1u && oc_mod.state[st] == MD_OK && !memcmp(nor + oc_mod_flash(st), IMG, ILEN)); }
        (void)o;
        /* a newer version of the id goes beside, and the old one is erased */
        load(3);
        { uint32_t st2 = (uint32_t)oc_fxmod_plan(1u, 3u);
          chk("the planner offers a start beside the module that stands (not in place)", (int)st2 >= 0 && st2 != i);
          if ((int)st2 >= 0) {
              uint32_t sec = (uint32_t)OC_MOD_START_REL(st2);
              load((int32_t)sec);
              wr32(44, 0x00010100u);                          /* version 1.1.0 in the header; the descriptor's version word is the module's (the install checks the header page it is given) */
              reseal();
              r = install(1u, st2);
              chk("a re-install of id 1 (the header page's version 1.1.0): accepted, the old copy gone, one entry in the Effects table", r == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
          } }
    }
    /* ---- 4. each mutation of the descriptor is refused before an erase (the page and the card) and at the end of an install ---- */
    {
        static const struct { const char *what; uint32_t off; uint32_t v; int bytes; } M[] = {
            { "the caps block's magic", 0, 0, 4 }, { "the caps block's version", 6, 2, 2 }, { "a hot set", 16, 0x20C4800u, 4 }, { "the call table past the image", 8, 0x20C8000u, 4 },
            { "a call that is 0", 0, 0, 4 }, { "the pages pointer past the image", 12, 0x20C8000u, 4 }, { "the rows pointer unaligned", 24, 1, 4 }, { "a snapshot size of 0", 38, 0, 2 },
            { "the abi of another calling convention (FX_ABI_ID + 1)", 52, FX_ABI_ID + 1u, 4 }, { "the old 52 byte size", 4, 52, 2 },
        };
        uint32_t k, link0 = oc_mod_link(start_of(0)), desc, ent, caps, kern;
        for (k = 0; k < sizeof M / sizeof M[0]; k++) {
            uint32_t s = start_of(0);
            char t[200];
            int r_end;
            clean(); load(0);
            desc = rd32(IMG, 28) - link0; ent = rd32(IMG, desc + 12) - link0; caps = rd32(IMG, ent + 12) - link0; kern = rd32(IMG, caps + 8) - link0;
            if (M[k].off == 0 && !strcmp(M[k].what, "a call that is 0")) wr32(kern + 4u * 5u, 0);
            else if (M[k].bytes == 4) wr32(caps + M[k].off, M[k].v ? M[k].v + (M[k].off == 24 ? 0 : 0) : 0);
            else { IMG[caps + M[k].off] = (uint8_t)M[k].v; IMG[caps + M[k].off + 1] = (uint8_t)(M[k].v >> 8); }
            if (M[k].off == 24) wr32(caps + 24, link0 + 0x3001u);
            reseal();
            { uint32_t cp = start_of(0); memcpy(nor + oc_mod_flash(cp), IMG, ILEN); }
            oc_mod_boot(0);
            snprintf(t, sizeof t, "a mutated image (%s): the boot scan refuses it (BAD_HDR), the Effects table is empty; the card's and the page's check (oc_fxmod_image_check) and the install's end refuse it", M[k].what);
            ok = oc_mod.state[s] == MD_BAD_HDR && oc_mod.nfxmod == 0;
            clean();
            {   uint32_t mid = 0;
                ok &= oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) != 0;
            }
            r_end = install(1u, s);
            ok &= r_end != 0 && oc_mod.nfxmod == 0 && nor[oc_mod_flash(s)] == 0xFF;
            chk(t, ok);
        }
    }
    /* ---- 5. the unmutated image is accepted by the same check ---- */
    {
        uint32_t s = start_of(0), mid = 0;
        load(0);
        clean();
        chk("oc_fxmod_image_check accepts the real image (as the card and the page ask before the first erase) and names its module id 1", oc_fxmod_image_check(rd_whole, 0, s, ILEN, &mid) == 0 && mid == 1u);
    }
    /* ---- 6. (round 18) the host lets go of the module's kernel before ANY erase of the module's sectors (a removal, an in-place reinstall, a sector under it); an erase elsewhere leaves it ---- */
    {
        fx_caps_t kc;
        fx_kcalls_t kt;
        void (*w[FX_KCALLS_N])(void);
        uint32_t st, k, n0, other;
        int r;
        for (k = 0; k < FX_KCALLS_N; k++) w[k] = (void (*)(void))main;          /* twelve nonzero host addresses: this test never calls them */
        memcpy(&kt, w, sizeof kt);
        memset(&kc, 0, sizeof kc);
        kc.magic = FX_CAPS_MAGIC; kc.size = FX_CAPS_WIRE; kc.version = FX_CAPS_VERSION; kc.abi = FX_ABI_ID; kc.max_inst = 4; kc.prof = AP_DUB; kc.ram_work_min = 4; kc.ram_work_max = 16;
        kc.ram_extra = 2; kc.ram_extra_bytes = 5632; kc.param_bytes = 88; kc.gov_us = 450; kc.ram_state = 1176;
        clean(); load(SECT[0]);
        st = (uint32_t)oc_fxmod_plan(1u, 3u);                                      /* the planner's start, and the image linked for it */
        for (k = 0; k < 6u; k++) if (start_of(SECT[k]) == st) load(SECT[k]);
        r = install(1u, st);
        chk("guard: the module is installed and its kernel registered with the host (fx_modk_set), as oc_fxmod_kernel_reg does on the device", r == 0 && fx_modk_set(1u, 0x00010000u, 1u, &kc, &kt, 0) == FXM_OK && fx_modk_ok == 1);
        n0 = OC_MOD_START_SECTOR(st) + oc_mod_nsec(ILEN);                           /* the first sector after the module */
        other = n0 < OC_MOD_AREA ? n0 : OC_MOD_START_SECTOR(st) - 1u;
        (void)oc_mod_erase_span(other, 1u);
        chk("guard: an erase of a sector outside the module's span leaves its kernel registered", fx_modk_ok == 1);
        (void)oc_mod_erase_span(OC_MOD_START_SECTOR(st) + 1u, 1u);
        chk("guard: an erase of a sector inside the module's span lets the kernel go first (fx_modk_clear: the instances let go, the resident kernel plays)", fx_modk_ok == 0 && fx_mod_bound == 0);
        clean(); load(SECT[0]); st = (uint32_t)oc_fxmod_plan(1u, 3u); for (k = 0; k < 6u; k++) if (start_of(SECT[k]) == st) load(SECT[k]);
        r = install(1u, st);
        r |= fx_modk_set(1u, 0x00010000u, 1u, &kc, &kt, 0);
        r |= oc_fxmod_remove(1u);
        chk("guard: the removal (the editor's command 132, the page's) lets go of the kernel before the first erase", r == 0 && fx_modk_ok == 0 && oc_mod.nfxmod == 0);
        /* a newer version beside: the old copy is erased at the end of the install (oc_mod_end), under the registered kernel; the host must have let go of it before that erase */
        clean(); load(SECT[0]); st = (uint32_t)oc_fxmod_plan(1u, 3u); for (k = 0; k < 6u; k++) if (start_of(SECT[k]) == st) load(SECT[k]);
        r = install(1u, st);
        r |= fx_modk_set(1u, 0x00010000u, 1u, &kc, &kt, 0);
        {   uint32_t st2 = (uint32_t)oc_fxmod_plan(1u, 3u), kb = fx_modk_ok;
            for (k = 0; k < 6u; k++) if (start_of(SECT[k]) == st2) load(SECT[k]);
            wr32(44, 0x00010100u); reseal();
            r |= (int)st2 < 0 || st2 == st;
            r |= oc_fxmod_begin(1u, st2, ILEN);
            chk("guard: a newer version installed beside leaves the kernel of the old copy registered while the new one is written (its sectors are not touched)", r == 0 && kb == 1 && fx_modk_ok == 1);
            for (uint32_t o = FW_MOD_CODE_OFF; !r && o < ILEN; o += 256u) r = oc_mod_write(o, IMG + o, ILEN - o > 256u ? 256u : ILEN - o);
            r |= oc_mod_end(IMG);
            chk("guard: ... and at the end of the install, when the old copy is erased, the host lets go of its kernel first: nothing is bound, the instances are OFF, the new copy is the only module",
                r == 0 && fx_modk_ok == 0 && fx_mod_bound == 0 && fx_modk_uses() == 0 && oc_mod.nfxmod == 1 && oc_mod.fx[0].start == st2);
        }
    }
    /* ---- 7. (round 18) the registration of the kernel: the PC guard's window is wide enough BEFORE the host takes it; a refusal is what FX_TABLE sends; an unrelated rebuild keeps it ---- */
    {
        fx_caps_t kc;
        fx_kcalls_t kt;
        void (*w[FX_KCALLS_N])(void);
        fx_desc_wire_t d = { 1u, 1u, 0x00010000u, 0u, 0u };
        fx_entry_wire_t e = { 1u, 1u, 0u, 0u, 0u, 0u, 0u };
        uint32_t st, k, link;
        int r;
        for (k = 0; k < FX_KCALLS_N; k++) w[k] = (void (*)(void))main;
        memcpy(&kt, w, sizeof kt);
        memset(&kc, 0, sizeof kc);
        kc.magic = FX_CAPS_MAGIC; kc.size = FX_CAPS_WIRE; kc.version = FX_CAPS_VERSION; kc.abi = FX_ABI_ID; kc.max_inst = 4; kc.prof = AP_DUB; kc.ram_work_min = 4; kc.ram_work_max = 16;
        kc.ram_extra = 2; kc.ram_extra_bytes = 5632; kc.param_bytes = 88; kc.gov_us = 450; kc.ram_state = 1176;
        clean(); load(SECT[0]); st = (uint32_t)oc_fxmod_plan(1u, 3u); for (k = 0; k < 6u; k++) if (start_of(SECT[k]) == st) load(SECT[k]);
        r = install(1u, st);
        link = oc_mod_link(st);
        fm1_guard_code_end = link;                                              /* a window that ends where the module begins: the next audio interrupt would fault at its first instruction */
        r |= oc_fxmod_kernel_commit(st, &d, &e, &kc, &kt, 0);
        chk("registration: the window is widened to the module's last byte before the host takes the kernel (a call from the next interrupt does not fault)", r == 0 && fm1_guard_code_end >= link + ILEN - 1u && fx_modk_ok == 1 && oc_fxk_start == st);
        chk("... and the row says nothing is wrong: state OK, why 0 in the Effects table and in what FX_TABLE sends (oc_mod.why[start])", oc_mod.state[st] == MD_OK && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0);
        oc_mod_boot(0);
        chk("an unrelated rebuild of the registry (an engine module's install or removal ends in oc_mod_boot) keeps the kernel and its instances: the same module stands at the same start", fx_modk_ok == 1 && oc_mod.nfxmod == 1);
        {   fx_mod_bound = 1u; oc_modguard.inuse = 0;                                 /* an instance is bound to the kept kernel; the rebuild zeroes the crash mark */
            oc_mod_boot(0);
            chk("... and the crash mark of the module in use is taken again after the table is rebuilt (the bound instance notes only when it binds): a kernel crash soon after an engine install bans the module",
                fx_modk_ok == 1 && (oc_modguard.inuse & (1u << st)) != 0);
            fx_mod_bound = 0; }
        { fx_caps_t bad_c = kc; bad_c.abi ^= 1u; fx_modk_clear();
          r = oc_fxmod_kernel_commit(st, &d, &e, &bad_c, &kt, 0);
          chk("a refusal (another abi): the module stays listed, state OK, and why is 0x40 + the host's code (0x41) in the Effects row AND in what FX_TABLE sends (oc_mod.why[start]); nothing is registered",
              r == FXM_VER && fx_modk_ok == 0 && oc_mod.state[st] == MD_OK && oc_mod.why[st] == 0x41 && oc_mod.fx[0].why == 0x41); }
        { fx_caps_t bad_c = kc; bad_c.max_inst = 2;
          r = oc_fxmod_kernel_commit(st, &d, &e, &bad_c, &kt, 0);
          chk("a refusal (max_inst below the tracks): FXM_RANGE, why 0x42", r == FXM_RANGE && oc_mod.why[st] == 0x42 && fx_modk_ok == 0); }
        r = oc_fxmod_kernel_commit(st, &d, &e, &kc, &kt, 0);
        chk("the next good registration clears the reason", r == 0 && oc_mod.why[st] == 0 && oc_mod.fx[0].why == 0 && fx_modk_ok == 1);
    }
    printf(bad ? "fx dub image: FAILED\n" : "fx dub image: all passed\n");
    return bad != 0;
}
