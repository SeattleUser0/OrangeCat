/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 3 (docs/orangecat/MODULE_FORMAT_R3.md 7): the interface identifier moved. fw_module_check (the registration check, fw_api.h) of THIS firmware
 *   - refuses the headers of the modules the last Release 2 build made (tests/fixtures/r2_modules/*.hdr: SHAPES, WHEEL, the Prophet, linux-hw fd159be): FW_MOD_BAD_IFACE, and
 *     nothing else is wrong with them (every other number of the header would pass), so the identifier alone is what refuses them;
 *   - accepts the headers of the modules this build made (build/module/<name>.<start>.img, the images the build proved), FW_MOD_OK, for every start of each;
 *   - the identifier of the old and the new are different, and FW_API_IFACE is the generator's id for the text of the structures (tools/gen_fw_api_id.py).
 *   fw_module_check_old: the same check with the old identifier (a firmware of Release 2) refuses the new headers, both ways.
 * Arguments: DIR_OF_FIXTURES DIR_OF_MODULE_IMAGES (the build's build/module).
 * Revisions:
 *   (Release 3, step 3, branch r3-pages) first version.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <dirent.h>

static int bad, nchk;
static void check(const char *what, int ok)
{
    nchk++;
    printf("mod_iface: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int load(const char *path, fw_mod_hdr_t *h)
{
    FILE *f = fopen(path, "rb");
    size_t n = f ? fread(h, 1, sizeof *h, f) : 0;
    if (f) fclose(f);
    return n == sizeof *h;
}
/* The device's FW_API_IFACE is FW_API_ID xor three sizes the text cannot show (fw_api.h): the host's sizes are not the device's (8 byte pointers), so this host build cannot
 * make the device's identifier. What it can do: the sizes did not change between the two builds (nothing in track_t, voice_t or the like moved), so the two identifiers
 * differ by exactly FW_API_ID(old) xor FW_API_ID(new); and the registration check of a firmware whose identifier is `dev` is fw_module_check with the header's identifier
 * mapped to this host's (equal to dev -> FW_API_IFACE, else something else). */
#define OLD_FW_API_ID 0x126F12CCu                        /* linux-hw fd159be: build/gen/fw_api_id.h of the build the fixtures came from */
static int check_as(const fw_mod_hdr_t *h, uint32_t dev)
{
    fw_mod_hdr_t g = *h;
    g.iface = h->iface == dev ? FW_API_IFACE : ~FW_API_IFACE;
    return fw_module_check(&g);
}
int main(int argc, char **argv)
{
    static const char *const NAMES[] = {"shapes", "wheel", "prophet"};
    uint32_t i, starts, seen = 0, dev = 0;
    if (argc < 3) { printf("mod_iface: arguments: fixtures dir, module images dir\n"); return 2; }
    {   fw_mod_hdr_t w0; char p0[512];
        snprintf(p0, sizeof p0, "%s/wheel.0.img", argv[2]);
        if (!load(p0, &w0)) { check("this build's WHEEL image is there", 0); return 1; }
        dev = w0.iface;
    }
    for (i = 0; i < 3u; i++) {
        char p[512];
        fw_mod_hdr_t old, nw;
        snprintf(p, sizeof p, "%s/%s.hdr", argv[1], NAMES[i]);
        if (!load(p, &old)) { check("a Release 2 header is there", 0); continue; }
        check("the old header is a module's (magic, the table address and size of this firmware)", old.magic == FW_MOD_MAGIC && old.api_addr == FW_API_ADDR && old.api_size <= sizeof(fw_api_t));
        check("the identifier moved by exactly the generator's change (FW_API_ID old xor new): the sizes behind it did not", (old.iface ^ dev) == (OLD_FW_API_ID ^ FW_API_ID) && old.iface != dev);
        check("a Release 3 firmware refuses the Release 2 module: FW_MOD_BAD_IFACE", check_as(&old, dev) == FW_MOD_BAD_IFACE);
        check("..and it is the identifier alone: the same header with the new identifier passes", ({ fw_mod_hdr_t g = old; g.iface = dev; check_as(&g, dev) == FW_MOD_OK || old.api_version > FW_API_VERSION; }));
        for (starts = 0; starts < 8u; starts++) {
            snprintf(p, sizeof p, "%s/%s.%u.img", argv[2], NAMES[i], starts);
            if (!load(p, &nw)) continue;
            seen++;
            check("a Release 3 firmware takes the module built for it (every start)", check_as(&nw, dev) == FW_MOD_OK && nw.iface == dev);
            check("the Release 3 module's engine is the Release 2 module's", nw.engine == old.engine);
            check("a Release 2 firmware (the old identifier) refuses the new module: FW_MOD_BAD_IFACE", check_as(&nw, old.iface) == FW_MOD_BAD_IFACE);
            check("the Release 2 module passes in a Release 2 firmware (the check itself is not what changed)", check_as(&old, old.iface) == FW_MOD_OK || old.api_version > FW_API_VERSION);
        }
    }
    check("at least one module image of this build was read", seen > 0);
    printf("mod_iface: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
