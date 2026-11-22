/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat (engines-b, Session B, 2026-11-21): the candidates' page descriptors as the LINKER laid them. tests/module_host_cand_test.c checks each
 * descriptor written out by tests/fw_pages_wire.h from the host's copy; this reads the real image tools/build.py --module linked with the device's toolchain
 * (build/module/<name>.<start>.img: the header page at the link address, the code after it), follows the header's engine_ptr to the engine_t and the word in
 * it that points at a descriptor (the one word of the engine_t that holds the address of a word-aligned "GPAG" inside the image), and runs the loader's own
 * checks on those bytes: fw_pages_check_ex (SC_MOD, this build's P_COUNT and G_COUNT) and fw_ui_check (four knobs, no picture). The host cannot run the
 * module's code; it reads its bytes, as the loader does.
 *   build/host/cand_image_pages_test build/module supersaw mixtur brassophone x0x303
 * A name whose image is missing fails (CI's build step builds every module of tools/build.py MODULES before the suite). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "build_options.h"
#include "core.h"
#include "fw_pages.h"

static uint8_t img[8u * 0x1000u];
static uint32_t ilen, link;
static int bad;
static void chk(const char *name, const char *what, int ok)
{
    printf("cand image pages: %-12s %-110s %s\n", name, what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    (void)ctx;
    if (addr < link || addr - link > ilen || n > ilen - (addr - link)) return 1;
    memcpy(buf, img + (addr - link), n);
    return 0;
}
static int scene_never(const uint8_t *s, uint32_t n) { (void)s; (void)n; return 0; }   /* (no candidate carries a picture: a scene would fail here, on purpose) */
static uint32_t w32(uint32_t off) { return off + 4u <= ilen ? fwp_u32(img + off) : 0u; }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/module";
    for (int a = 2; a < argc; a++) {
        char p[512];
        FILE *f;
        uint32_t eng, len, desc = 0, nfound = 0, ui = 0, o;
        fw_ui_info_t info;
        snprintf(p, sizeof p, "%s/%s.0.img", dir, argv[a]);
        f = fopen(p, "rb");
        ilen = f ? (uint32_t)fread(img, 1, sizeof img, f) : 0u;
        if (f) fclose(f);
        chk(argv[a], "the image of start 0 is there (tools/build.py --module)", ilen > 0x100u && fwp_u32(img) == 0x444F4D4Fu);
        if (ilen <= 0x100u) continue;
        eng = w32(28); link = w32(32); len = w32(48);   /* fw_mod_hdr_t: engine_ptr (word 7), link_addr (8), length (12) */
        chk(argv[a], "the header's length is the image's, its engine_t inside the code", len == ilen && eng >= link + 0x100u && eng < link + ilen);
        for (o = eng - link; o + 4u <= eng - link + 256u && o + 4u <= ilen; o += 4u) {   /* the engine_t's word that holds a descriptor's address */
            uint32_t v = w32(o);
            if (v >= link + 0x100u && v + FW_PAGES_WIRE <= link + ilen && !(v & 3u) && w32(v - link) == FW_PAGES_MAGIC) { desc = v; nfound++; }
        }
        chk(argv[a], "its engine_t points at one page descriptor (GPAG)", nfound == 1u);
        if (nfound != 1u) continue;
        chk(argv[a], "fw_pages_check_ex on the linked bytes: FWP_OK (version 3, SC_MOD)",
            fw_pages_check_ex(rd, 0, desc, link + 0x100u, link + ilen, SC_MOD, P_COUNT, G_COUNT, &ui) == FWP_OK && ui);
        chk(argv[a], "fw_ui_check on the linked bytes: four knobs, no picture",
            fw_ui_check(rd, 0, ui, link + 0x100u, link + ilen, scene_never, &info) == FWU_OK && info.nk == 4u && info.scene_len == 0u);
    }
    printf(bad ? "cand image pages: FAILED\n" : "cand image pages: all passed\n");
    return bad != 0;
}
