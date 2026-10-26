/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.13.4: the card's installers (pack, module, set), split out of card_browse.c so a host test can build
 * them against the real oc_pack.c and oc_module.c on a simulated NOR (tests/card_install_test.c). Included by
 * card_browse.c, after the fb struct and the fb_* name tests. Code moved unchanged except for the 0.13.4 checks: the
 * dry pass reads the whole image and runs the loader's own checks (pk_image_check, oc_mod_image_check) before any
 * erase. Authorship of the moved code is as in card_browse.c.
 *
 * Revisions: 0.13.4 split from card_browse.c.
 *   0.13.5 fb_install_set: the press that answers an in-place question goes on from the module that asked (a set of two modules asking by turns for ever).
 *   0.14.1 fb_mod_file: the relocatable package (format 2: one image linked at 0 and its relocation list), relocated here for the start (fb_rel_rd).
 */
#if FB_PAK
/* (0.8.0) the pack file at (fb.path, e) installed: 0 done, else fb.msg says why. Reads it in 512-byte pieces: the header, the
 * tables to oc_pack_write, the header again to oc_pack_end (it is programmed last). */
static void fb_file(char *p, const char *name)          /* (0.13.3) the path of a file of the folder shown */
{
    uh_cat(uh_cat(uh_cat(p, fb.path), str_eq(fb.path, "/") ? "" : "/"), name);
}
/* (0.13.4) the card's file as a reader for the shared checks (oc_rd_t): ctx is {the open file, the offset of the image in it} */
typedef struct { FIL *f; uint32_t base; } fb_rd_t;
static int fb_rd(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    const fb_rd_t *r = ctx;
    UINT got;
    return f_lseek(r->f, r->base + off) != FR_OK || f_read(r->f, dst, n, &got) != FR_OK || got != n;
}
/* (0.13.3) the pack file at p (size bytes) installed, or with dry only checked: 0 done, else fb.msg says why.
 * (0.13.4) The whole file is read and checked first, with the checks oc_pack_end and the boot check apply (pk_image_check: this build's header, the payload CRC,
 * the sector tails): nothing is erased for a file that would be refused at the end. Dry stops there. */
static int fb_pak_file(const char *p, uint32_t size, int dry)
{
    uint8_t buf[512];
    FIL *f = &uf_pf;
    UINT got;
    uint32_t len = OC_PACK_LEN, off;
    fb_rd_t rd;
    int rc;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (size != len) { str_cpy(fb.msg, "Not this build's pack", sizeof fb.msg); return 1; }
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    rd.f = f; rd.base = 0;
    rc = pk_image_check(fb_rd, &rd, size);
    if (rc) {
        f_close(f);
        str_cpy(fb.msg, rc == 1 ? "Not this build's pack" : rc == 2 ? "Pack file damaged" : rc == 3 ? "Update would erase it" : "Unreadable", sizeof fb.msg);
        return 1;
    }
    if (dry) { f_close(f); return 0; }
    if (oc_pack_begin(len) != 0) { f_close(f); str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    rc = 0;
    if (f_lseek(f, OC_PACK_HDR) != FR_OK) rc = 1;
    for (off = OC_PACK_HDR; off < len && !rc; off += got) {
        uint32_t n = len - off > sizeof buf ? (uint32_t)sizeof buf : len - off;
        if (f_read(f, buf, n, &got) != FR_OK || got != n || oc_pack_write(off, buf, n) != 0) rc = 1;
        SMP_FEED();
    }
    if (!rc && (f_lseek(f, 0) != FR_OK || f_read(f, buf, sizeof buf, &got) != FR_OK || got != sizeof buf || oc_pack_end(buf) != 0))
        rc = 1;
    f_close(f);
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    return 0;
}
static int fb_install(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    fb_file(p, e->name);
    if (fb_pak_file(p, e->size, 0)) return 1;
    fb.packed = 1;
    return 0;
}
#endif
#if FB_OCM
static uint32_t fb_put(char *d, uint32_t at, const char *s, uint32_t max)        /* (0.12.1) up to max characters of s at d[at]; the new end (fb.msg holds 24 with its NUL: the callers' sum is 23 at most) */
{
    uint32_t k;
    for (k = 0; s[k] && k < max && at + 1u < sizeof fb.msg; k++) d[at++] = s[k];
    return at;
}
/* (0.14.1) A relocatable package (format 2, web/EDITOR_PROTOCOL.md): one image linked at 0, its relocation list and, for each start it was proved for, the
 * CRC-32 of the image that start gets. A list word is a site (tools/ocflash.py apply_relocs, web/fm1oc.js ocmRelocate: the same rules): bits 0..23 its
 * offset, bits 24..31 its kind, 1 a 32-bit word holding an address, 2 a MOV_ABS32 instruction pair (8 bytes) whose constant's two halves sit at offset + 2
 * and + 4, which is the little-endian 32-bit value at offset + 2: either way base is added to the 32-bit value at fb_rel_at. The list is read into the
 * borrowed RAM (FB_RELOCS) and refused when it is longer than FB_REL_MAX, out of order, has another kind, a site outside the image or one that straddles
 * the header page's end. The image a start gets is read through fb_rel_rd: the header page from RAM (its sites applied, the two CRCs filled as the build
 * fills them: fb_rel_seal), the rest the file's bytes with the sites in the range relocated. Its CRC-32 must be the package's for that start, so the flash
 * gets byte for byte the image the variants package would have held; then oc_mod_image_check and the write read the same reader. */
#define FB_REL_MAX 2048u
#ifndef FB_RELOCS
#define FB_RELOCS() ((uint32_t *)(void *)(proj_wire_u.raw + 8192u))   /* the list: after the rows (4 KB), a set's manifest (2 KB) and the parsed set (below) */
_Static_assert(sizeof proj_wire_u >= 8192u + 4u * FB_REL_MAX, "the relocation list fits the borrowed RAM");
#define FB_RELOCS_AT_8K 1
#endif
#define FB_RELOCATABLE 0xFFFFFFFFu
typedef struct { fb_rd_t f; const uint32_t *w; uint32_t n, base; uint8_t pg[256]; } fb_rel_t;
static uint32_t fb_rel_at(uint32_t w) { return (w & 0xFFFFFFu) + ((w >> 24) == 2u ? 2u : 0u); }
static void fb_wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static int fb_rel_rd(void *ctx, uint32_t off, void *dst, uint32_t n)
{
    fb_rel_t *r = ctx;
    uint8_t *d = dst;
    uint32_t k;
    for (; n && off < 256u; n--) *d++ = r->pg[off++];
    if (!n) return 0;
    if (fb_rd(&r->f, off, d, n)) return 1;
    for (k = 0; k < r->n; k++) {
        uint32_t a = fb_rel_at(r->w[k]), j, v;
        uint8_t o[4];
        if (a < 256u || a + 4u <= off || a >= off + n) continue;
        if (a >= off && a + 4u <= off + n) memcpy(o, d + (a - off), 4);
        else if (fb_rd(&r->f, a, o, 4)) return 1;
        v = pk_rd32(o) + r->base;
        for (j = 0; j < 4u; j++)
            if (a + j >= off && a + j < off + n) d[a + j - off] = (uint8_t)(v >> (8u * j));
    }
    return 0;
}
/* the header page of the image for r->base and that image's CRC-32 against want (the package's for the start): 0, MC_READ or MC_CRC. b: 256 bytes */
static int fb_rel_seal(fb_rel_t *r, uint32_t len, uint32_t want, uint8_t *b)
{
    uint32_t crc = 0xFFFFFFFFu, off, k, pass;
    if (fb_rd(&r->f, 0, r->pg, 256u)) return MC_READ;
    for (k = 0; k < r->n; k++) {
        uint32_t a = fb_rel_at(r->w[k]);
        if (a < 256u) fb_wr32(r->pg + a, pk_rd32(r->pg + a) + r->base);
    }
    for (pass = 0; pass < 2u; pass++) {               /* the code's CRC-32 into the header, then the whole image's */
        if (pass) {
            fb_wr32(r->pg + 52, ~crc);
            fb_wr32(r->pg + 56, ocs_crc32(r->pg, 56u));
            crc = ocs_crc_add(0xFFFFFFFFu, r->pg, 256u);
        }
        for (off = 256u; off < len; off += 256u) {
            uint32_t n = len - off > 256u ? 256u : len - off;
            if (fb_rel_rd(r, off, b, n)) return MC_READ;
            crc = ocs_crc_add(crc, b, n);
            SMP_FEED();
        }
    }
    return ~crc == want ? 0 : MC_CRC;
}
/* the list of a format 2 package (nrel words at roff, CRC-32 rcrc) into FB_RELOCS, checked against an image of len bytes: 0, or 1 refused */
static int fb_rel_load(FIL *f, uint32_t nrel, uint32_t roff, uint32_t rcrc, uint32_t len)
{
    uint32_t *w = FB_RELOCS(), k, prev = 0;
    UINT got;
    if (nrel > FB_REL_MAX || f_lseek(f, roff) != FR_OK || f_read(f, w, 4u * nrel, &got) != FR_OK || got != 4u * nrel ||
        ocs_crc32(w, 4u * nrel) != rcrc)
        return 1;
    for (k = 0; k < nrel; k++) {
        uint32_t x = pk_rd32((const uint8_t *)&w[k]), o = x & 0xFFFFFFu, t = x >> 24, a;
        w[k] = x;                                       /* (the file's little-endian word, whatever the CPU) */
        a = fb_rel_at(x);
        if ((k && o <= prev) || (t != 1u && t != 2u) || o + (t == 2u ? 8u : 4u) > len || (a < 256u && a + 4u > 256u))
            return 1;
        prev = o;
    }
    return 0;
}
/* the image start vs gets from the package's row o, ready for fb_rel_rd with its header page in r->pg: format 1, variant o's bytes (no sites); format 2,
 * the one image relocated for vs, its CRC-32 the package's (hdr: the package's first 256 bytes; b: 256 bytes of scratch). 0 or MC_READ, MC_CRC */
static int fb_mod_src(fb_rel_t *r, const uint8_t *hdr, uint32_t ver, uint32_t o, uint32_t vs, uint32_t voff, uint32_t len, uint8_t *b)
{
    r->f.base = voff;
    if (ver != 2u) {
        r->n = 0; r->base = 0;
        return fb_rd(&r->f, 0, r->pg, 256u) ? MC_READ : 0;
    }
    r->n = pk_rd32(hdr + 72u); r->base = oc_mod_link(vs);
    return fb_rel_seal(r, len, pk_rd32(hdr + 92u + 8u * o), b);
}
/* (0.10.0) the module package at (fb.path, e) installed: 0 done, else fb.msg says why. The package: a 56 byte header, a table of 16 byte variants, a CRC-32 of
 * both, then the images (web/EDITOR_PROTOCOL.md); (0.14.1) or format 2, above. The image for the place oc_mod_plan names goes to oc_mod_begin / write in 256
 * byte pieces; its first 256 bytes (the header page) go to oc_mod_end, which programs them last. */
static int fb_mod_file(const char *p, uint32_t size, int armed, uint32_t q, int dry, uint32_t *engine_out)
{
    uint8_t buf[256], sc[256];                        /* (0.14.1) sc: fb_rel_seal's scratch (buf keeps the package's header) */
    FIL *f = &uf_pf;
    UINT got;
    uint32_t n, tab = 56u, hc, engine, start, off = 0, len = 0, o, ver, m = 0, po = 0;
    fb_rel_t rel;
    int plan, rc = 0;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (f_read(f, buf, sizeof buf, &got) != FR_OK || got < 64u || pk_rd32(buf) != 0x464D434Fu || ((ver = pk_rd16(buf + 4)) != 1u && ver != 2u)) {
        f_close(f);
        str_cpy(fb.msg, "Not a module", sizeof fb.msg);
        return 1;
    }
    n = pk_rd16(buf + 6);
    if (ver == 2u && got >= tab + 32u) m = pk_rd32(buf + tab + 28u);  /* (0.14.1) format 2: one image, then the list's place and each start's CRC-32 */
    hc = ver == 2u ? tab + 32u + 8u * m : tab + 16u * n;
    if (n < 1u || n > 8u || (ver == 2u && (n != 1u || m < 1u || m > 8u || pk_rd32(buf + tab) != FB_RELOCATABLE)) || hc + 4u > got ||
        ocs_crc32(buf, hc) != pk_rd32(buf + hc)) { f_close(f); str_cpy(fb.msg, "Not a module", sizeof fb.msg); return 1; }
    engine = pk_rd32(buf + 16);
    if (engine_out) *engine_out = engine;
    if (pk_rd32(buf + 8) != FW_API_IFACE || pk_rd32(buf + 12) != FW_API_ADDR || pk_rd32(buf + 44) > FW_API_VERSION ||
        pk_rd32(buf + 48) > sizeof(fw_api_t) || pk_rd32(buf + 24) != OC_PACK_HASH) {
        f_close(f);
        str_cpy(fb.msg, "Not for this firmware", sizeof fb.msg);
        return 1;
    }
    plan = oc_mod_plan(engine);
    if (plan < 0) {
        int b = oc_mod_blocked_by(engine);             /* (0.12.1) a usable module of another engine stands in the way: never erased by an install; the user removes it first */
        f_close(f);
        if (b >= 0) {                                   /* "ENGINE X INSTALLED AT START N: REMOVE IT FIRST" in the 23 characters a row has: "PROPHET at 2: remove it" */
            const char *nm = ENGINES[oc_mod.engine[b] < NENGINES ? oc_mod.engine[b] : 0]->name;
            uint32_t at = 0;
            at = fb_put(fb.msg, at, nm, 9u);
            at = fb_put(fb.msg, at, " at ", 4u);
            fb.msg[at++] = (char)('0' + b);
            at = fb_put(fb.msg, at, ": remove it", 11u);
            fb.msg[at] = 0;
        } else str_cpy(fb.msg, "No place free", sizeof fb.msg);
        return 1;
    }
    start = (uint32_t)plan;
    rel.f.f = f; rel.w = FB_RELOCS();
    for (o = 0; o < (ver == 2u ? m : n); o++)          /* po: the planned start's row (a variant, or a start's CRC-32 in format 2) */
        if (pk_rd32(buf + tab + (ver == 2u ? 32u + 8u * o : 16u * o)) == OC_MOD_START_SECTOR(start)) { po = o; len = 1u; }
    if (ver == 2u) {                                   /* (0.14.1) the one image and the list (checked into FB_RELOCS) */
        off = pk_rd32(buf + tab + 12u);
        len = len ? pk_rd32(buf + tab + 4u) : 0u;
        if (len && (off + len > size || fb_rel_load(f, pk_rd32(buf + tab + 16u), pk_rd32(buf + tab + 20u), pk_rd32(buf + tab + 24u), len))) {
            f_close(f); str_cpy(fb.msg, "Module file damaged", sizeof fb.msg); return 1;
        }
    } else if (len) {
        len = pk_rd32(buf + tab + 16u * po + 4u); off = pk_rd32(buf + tab + 16u * po + 12u);
    }
    if (!len || off + len > size || len <= FW_MOD_CODE_OFF || (len & 3u)) { f_close(f); str_cpy(fb.msg, "No image for it", sizeof fb.msg); return 1; }
    /* (0.13.4) the whole image for the start the unit names is read and checked here, with the checks oc_mod_end and the boot check apply (oc_mod_image_check:
     * the inner header against the start and the firmware, the code's CRC-32, the page descriptor, the sector tails), before the in-place question and before
     * anything is erased. A dry pass (the set's preflight) checks the file's image for every start its engine may be put at, since an earlier module of the set
     * can change which start the real install uses. (0.14.1) Every image is read through fb_rel_rd: a format 1 variant is an image with no sites. */
    for (o = 0; o < (ver == 2u ? m : n); o++) {
        uint32_t sec = pk_rd32(buf + tab + (ver == 2u ? 32u + 8u * o : 16u * o)), vs, vlen = ver == 2u ? len : pk_rd32(buf + tab + 16u * o + 4u),
                 voff = ver == 2u ? off : pk_rd32(buf + tab + 16u * o + 12u);
        int ck;
        for (vs = 0; vs < OC_MOD_STARTS && OC_MOD_START_SECTOR(vs) != sec; vs++) ;
        if (vs >= OC_MOD_STARTS || (vs != start && (!dry || !oc_mod_allowed(engine, vs)))) continue;
        if (!vlen || voff + vlen > size) { f_close(f); str_cpy(fb.msg, "No image for it", sizeof fb.msg); return 1; }
        ck = fb_mod_src(&rel, buf, ver, o, vs, voff, vlen, sc);
        if (!ck) ck = oc_mod_image_check(fb_rel_rd, &rel, engine, vs, vlen);
        if (ck) {
            f_close(f);
            str_cpy(fb.msg, ck == MC_TAIL ? "Update would erase it" : ck == MC_READ ? "Unreadable" : ck == MC_PAGES ? "Module pages bad" : ck == MC_CRC ? "Module file damaged" : "Module header bad", sizeof fb.msg);
            return 1;
        }
    }
    if (fb_mod_src(&rel, buf, ver, po, start, off, len, sc)) {    /* the planned start's image again (a dry pass may have read another start's last) */
        f_close(f); str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1;
    }
    if (dry) { f_close(f); return 0; }
    if (oc_mod_inplace(engine, start) && !armed) {      /* (0.10.1) no other place is free: the old module is erased first and a power cut leaves none.
                                                                                  * Said once; OCT+ on the same row again goes on (fb.ask is the row, as for a pack's question).
                                                                                  * (0.13.4) asked only for a file that passed the check above */
        f_close(f);
        fb.ask = (uint8_t)(fb.sel + 1u);
        fb.setq = (uint8_t)q;                           /* (0.13.3) for a set: which of its modules asked */
        str_cpy(fb.msg, "Erases old first. Again", sizeof fb.msg);
        return 1;
    }
    if (oc_mod_begin(engine, start, len) != 0) { f_close(f); str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    for (o = FW_MOD_CODE_OFF; o < len && !rc; o += sizeof buf) {
        uint32_t k = len - o > sizeof buf ? (uint32_t)sizeof buf : len - o;
        if (fb_rel_rd(&rel, o, buf, k) || oc_mod_write(o, buf, k) != 0) rc = 1;
        SMP_FEED();
    }
    f_close(f);
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    rc = oc_mod_end(rel.pg);
    if (rc == -8) { str_cpy(fb.msg, "Update would erase it", sizeof fb.msg); return 1; }   /* (0.10.1) -8 is oc_module.c MI_TAIL: a sector tail like an update record, nothing committed */
    if (rc) { str_cpy(fb.msg, "Install failed", sizeof fb.msg); return 1; }
    return 0;
}
static int fb_install_mod(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    int armed = fb.ask == (uint8_t)(fb.sel + 1u);      /* (0.10.1) the in-place question was asked on this row; any other press drops it */
    fb.ask = 0;
    fb.setq = 0;
    fb_file(p, e->name);
    if (fb_mod_file(p, e->size, armed, 0u, 0, 0)) return 1;
    fb.packed = 2;
    return 0;
}
#if FB_SET
#include "oc_set.h"
#ifdef FB_RELOCS_AT_8K
_Static_assert(4096u + 2048u + sizeof(oc_set_t) <= 8192u, "(0.14.1) the parsed set ends before the relocation list (FB_RELOCS)");
#endif
#ifndef FB_TEXT
#define FB_TEXT() ((char *)proj_wire_u.raw + 4096u)      /* (0.13.3) a set's manifest, and the parsed set after it, in the borrowed RAM behind the rows */
#define FB_SETS() ((oc_set_t *)(proj_wire_u.raw + 4096u + 2048u))
#endif
#define FB_TEXT_MAX 2048u
#ifndef OC_FW_VERSION
#define OC_FW_VERSION 0u
#endif
/* (0.13.3) the file `name` of the set's folder: its path in p, its size in *size. 0, or 1 and fb.msg "Missing <name>" */
static int fb_set_file(char *p, const char *name, uint32_t *size)
{
    FILINFO fi;
    fb_file(p, name);
    if (f_stat(p, &fi) != FR_OK || (fi.fattrib & AM_DIR)) {
        uint32_t at = fb_put(fb.msg, 0u, "Missing ", 8u);
        at = fb_put(fb.msg, at, name, 15u);
        fb.msg[at] = 0;
        return 1;
    }
    *size = (uint32_t)fi.fsize;
    return 0;
}
/* (0.13.3) "Install set": the manifest at (fb.path, e) read, checked against this firmware (version, then interface identifier, table address, pack hash and
 * length), every file of it found and checked as far as nothing is written (the pack's header, each module's header and the place the unit names), the
 * modules checked in pairs (the 12 sector area holds SHAPES or the Prophet, not both); then the pack (unless this build's is there and sound), then the
 * modules in the order of the manifest, each through fb_mod_file as OCT+ on a .OCM does, with its own in-place question (OCT+ again on the row answers
 * the one that asked). Nothing is kept between the presses but fb.ask and fb.setq: a cut or a question leaves the stages done and the next press does the
 * rest (the pack is skipped, a module is installed again). Only the stages the card can do: the firmware package of the set is not started from here
 * (the update loader takes it from a computer over USB), so a set for another version of the firmware is refused with the words to update it first.
 * 0 done (fb.packed 3), else fb.msg says why. */
static int fb_install_set(const fb_ent_t *e)
{
    char p[FB_PATH + FB_NAME + 2u];
    FIL *f = &uf_pf;
    UINT got;
    oc_set_t *o = FB_SETS();
    uint32_t k, j, size, plen, phash, eng[OC_SET_OCM];
    uint8_t ask0 = fb.ask, q0 = fb.setq;
    uint32_t k0 = 0;
    fb.ask = 0;
    fb.setq = 0;
    if (song.playing || transport_busy()) { str_cpy(fb.msg, "Stop first", sizeof fb.msg); return 1; }
    if (e->size < 16u || e->size >= FB_TEXT_MAX) { str_cpy(fb.msg, "Not a set", sizeof fb.msg); return 1; }
    fb_file(p, e->name);
    if (f_open(f, p, FA_READ) != FR_OK) { str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    if (f_read(f, FB_TEXT(), e->size, &got) != FR_OK || got != e->size) { f_close(f); str_cpy(fb.msg, "Unreadable", sizeof fb.msg); return 1; }
    f_close(f);
    if (oc_set_parse(FB_TEXT(), e->size, o) != OC_SET_OK) { str_cpy(fb.msg, "Not a set", sizeof fb.msg); return 1; }
    switch (oc_set_vs(o, OC_FW_VERSION, FW_API_IFACE, FW_API_ADDR, OC_PACK_HASH, OC_PACK_LEN)) {
    case OC_SET_NEWER: str_cpy(fb.msg, "Update firmware first", sizeof fb.msg); return 1;
    case OC_SET_OLDER: str_cpy(fb.msg, "Set is for older build", sizeof fb.msg); return 1;
    case OC_SET_OTHER: str_cpy(fb.msg, "Set: another build", sizeof fb.msg); return 1;
    default: break;
    }
    if (fb_set_file(p, o->pak, &size) || fb_pak_file(p, size, 1)) return 1;
    for (k = 0; k < o->nocm; k++) {
        if (fb_set_file(p, o->ocm[k], &size) || fb_mod_file(p, size, 0, 0u, 1, &eng[k])) return 1;
        for (j = 0; j < k; j++)
            if (eng[j] == eng[k] || !oc_mod_pair_fits(eng[j], eng[k])) { str_cpy(fb.msg, "Modules don't fit", sizeof fb.msg); return 1; }
    }
    if (!(oc_pack_info(&plen, &phash) == PK_OK && plen == OC_PACK_LEN && phash == OC_PACK_HASH)) {
        if (fb_set_file(p, o->pak, &size) || fb_pak_file(p, size, 0)) return 1;
    }
    /* (0.13.5) the press that answers an in-place question goes on from the module that asked: the ones before it were installed by the presses before, and a
     * loop from the first would write the first again (beside: one more erase; in place: its question again, and the two modules asking by turns for ever) */
    if (ask0 == (uint8_t)(fb.sel + 1u) && q0 >= 1u && q0 <= o->nocm) k0 = (uint32_t)q0 - 1u;
    for (k = k0; k < o->nocm; k++) {
        if (fb_set_file(p, o->ocm[k], &size) ||
            fb_mod_file(p, size, ask0 == (uint8_t)(fb.sel + 1u) && q0 == (uint8_t)(k + 1u), k + 1u, 0, 0))
            return 1;
    }
    fb.packed = 3;
    return 0;
}
#endif
#endif
