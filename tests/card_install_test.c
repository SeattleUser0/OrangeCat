/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Orange Cat 0.13.4 (Bug 2): the card's installers (firmware/src/card_install.c) against the REAL loaders (oc_pack.c, oc_module.c) on the simulated NOR flash,
 * with the card's files in RAM behind a small FatFs stand-in. The point: a pack or module file that the loader would refuse at the end is refused BEFORE the
 * first sector is erased, so the working pack or module is never lost to a damaged file. In every case below the simulated NOR counts zero erases, the
 * area's bytes are unchanged, the previous pack and module are still registered and usable (a reboot of the loader finds them), and fb.msg says why.
 *   - a pack file with one payload byte flipped and a good header
 *   - a pack file whose sector tail is an update record
 *   - a module file with one code byte flipped (the question of an in-place install is not asked either)
 *   - a module whose inner header has a wrong link address (the package header is fine)
 *   - a module whose sector tail matches the update loader's record rule
 *   - a set with one bad file (the pack bad; a module bad): no stage starts, the pack stays as it was
 * and the good files install (erases happen, the new module registered), so the refusals are the checks and not a broken harness.
 * Built like oc_module_test (OC_MODULE_WINS 1).
 */
/* Revisions:
 * (Orange Cat 0.13.4) first version
 * (Orange Cat 0.13.5) a set of two modules that both ask the in-place question: the press that answers goes on from the module that asked (set_resume() below)
 * (Release 3 step 4) fx_cases(): an effect module (kind 1, its module id in the image's fx_desc_t) on the simulated card against the real loader: planned and installed by
 *   module id, a newer version beside, each refusal before the first erase (a flipped code byte, a bad descriptor, a tail record, a bad engine word, nine sectors, no image for
 *   the planned start, no free span naming the module in the way), the relocatable package's flash = the variants package's, and sets: the effect module planned on the
 *   area an engine's install leaves, its sectors counted with the planner (two modules that fit alone but not together), the same id twice
 */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
/* (0.13.5) the geometry table of oc_module.c can be changed by a test: set_resume() gives SHAPES start 0 only and the Prophet start 1 only, 2 sectors each. They fit together,
 * and each can only be replaced in place: a set of the two asks twice */
#define OC_MOD_GEO_QUAL
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

/* ---- the card: files in RAM, the FatFs calls the installers use */
typedef unsigned int UINT;
typedef enum { FR_OK = 0, FR_NO_FILE = 4 } FRESULT;
#define FA_READ 1
#define AM_DIR 0x10
typedef struct { const uint8_t *d; uint32_t size, pos; } FIL;
typedef struct { uint32_t fsize; uint8_t fattrib; } FILINFO;
typedef struct { char path[64]; uint8_t *d; uint32_t size; } cf_t;
static cf_t cf[16];
static unsigned ncf;
static FIL uf_pf;
static cf_t *cf_find(const char *p) { for (unsigned i = 0; i < ncf; i++) if (!strcmp(cf[i].path, p)) return &cf[i]; return 0; }
static void cf_put(const char *p, const uint8_t *d, uint32_t n)
{
    cf_t *c = cf_find(p);
    if (!c) c = &cf[ncf++];
    snprintf(c->path, sizeof c->path, "%s", p);
    free(c->d);
    c->d = malloc(n); memcpy(c->d, d, n); c->size = n;
}
static FRESULT f_open(FIL *f, const char *p, int mode) { cf_t *c = cf_find(p); (void)mode; if (!c) return FR_NO_FILE; f->d = c->d; f->size = c->size; f->pos = 0; return FR_OK; }
static FRESULT f_lseek(FIL *f, uint32_t o) { f->pos = o; return FR_OK; }
static FRESULT f_read(FIL *f, void *b, UINT n, UINT *got)
{
    uint32_t k = f->pos >= f->size ? 0u : f->size - f->pos < n ? f->size - f->pos : n;
    memcpy(b, f->d + f->pos, k); f->pos += k; *got = k;
    return FR_OK;
}
static FRESULT f_close(FIL *f) { (void)f; return FR_OK; }
static FRESULT f_stat(const char *p, FILINFO *fi) { cf_t *c = cf_find(p); if (!c) return FR_NO_FILE; fi->fsize = c->size; fi->fattrib = 0; return FR_OK; }

/* ---- the browser's state, as card_browse.c has it */
#define FB_MAX 64u
#define FB_NAME 48u
#define FB_PATH 96u
typedef struct { char name[FB_NAME]; uint8_t dir, longname; uint32_t size; } fb_ent_t;
static struct { uint8_t st, trk, gen, up, relist, ask, packed, setq; uint16_t n, sel; uint32_t t0; char path[FB_PATH]; char msg[24]; } fb;
static char t_fb_text[2048 + 512];
#define FB_TEXT() t_fb_text
#define FB_SETS() ((oc_set_t *)(t_fb_text + 2048))
static char *uh_cat(char *b, const char *s) { while (*s) *b++ = *s++; *b = 0; return b; }   /* (usb_host.c's) */
#ifdef CIT_NOCHECK                                       /* (a mutation, to show the test fails without the fix: the installers behave as before 0.13.4, the whole-file checks skipped) */
#define pk_image_check(a, b, c) (0)
#define oc_mod_image_check(a, b, c, d, e) (0)
#endif
static uint32_t t_fb_rel[2048];                         /* (0.14.1) the relocation list's RAM (card_install.c FB_RELOCS) */
#define FB_RELOCS() t_fb_rel
#define FB_PAK 1
#define FB_OCM 1
#define FB_SET 1
#include "../firmware/src/card_install.c"

static int bad;
static void chk(const char *what, int ok)
{
    printf("card install: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* ---- images */
static void wr32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static uint8_t PAKF[OC_PACK_LEN];                    /* a good pack file, as tools/gen_pack.py lays one out */
static void make_pack(uint8_t *pak, uint32_t salt)
{
    memset(pak, 0xFF, OC_PACK_HDR);
    for (uint32_t i = OC_PACK_HDR; i < OC_PACK_LEN; i++) pak[i] = (uint8_t)(i * 31u + (i >> 8) * 7u + salt);
    wr32(pak, OC_PACK_MAGIC); pak[4] = OC_PACK_VERSION; pak[5] = 0; pak[6] = pak[7] = 0;
    wr32(pak + 8, OC_PACK_HASH); wr32(pak + 12, OC_PACK_LEN);
    wr32(pak + 16, ocs_crc32(pak + OC_PACK_HDR, OC_PACK_LEN - OC_PACK_HDR));
    memset(pak + 20, 0, 4);
    wr32(pak + 508, ocs_crc32(pak, 508));
}
#define LEN 5000u                                       /* sector 0's tail (0xF00) is inside it */
static uint8_t IMG[OC_MOD_MAXLEN];
static uint32_t IMG_ENGINE = ENGI_SHAPES;                /* the engine the images are made for */
static fw_mod_hdr_t mk(uint32_t i, uint32_t len)
{
    fw_mod_hdr_t h;
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.engine = IMG_ENGINE; h.state_size = 1234u; h.engine_ptr = oc_mod_link(i) + 0x180u; h.link_addr = oc_mod_link(i);
    h.pack_hash = OC_PACK_HASH; h.length = len;
    return h;
}
static void seal(fw_mod_hdr_t *h)
{
    h->crc = ocs_crc32(IMG + FW_MOD_CODE_OFF, h->length - FW_MOD_CODE_OFF);
    h->hcrc = ocs_crc32((const uint8_t *)h, FW_MOD_HDR_BYTES - 4u);
}
static void code(uint32_t seed) { for (uint32_t k = 0; k < sizeof IMG; k++) IMG[k] = (uint8_t)(k * 31u + seed); memset(IMG, 0, FW_MOD_CODE_OFF); }
/* one start's image (header page, code) into out[LEN] */
typedef void (*mut_t)(fw_mod_hdr_t *);
static void m_link(fw_mod_hdr_t *h) { h->link_addr += 0x1000u; }
static uint8_t REC[80];
static void variant(uint8_t *out, uint32_t i, uint32_t seed, mut_t f, int flip_code, int rec)
{
    fw_mod_hdr_t h = mk(i, LEN);
    code(seed);
    if (rec) memcpy(IMG + 0xF00u, REC, 80);
    if (f) f(&h);
    seal(&h);                                          /* sealed over the good code: a flipped byte below is the file's damage */
    memcpy(IMG, &h, sizeof h);
    if (flip_code) IMG[0x400] ^= 1u;
    memcpy(out, IMG, LEN);
}
/* a .OCM package: two variants (sectors 0 and 6) of one SHAPES module */
static uint8_t OCM[512 + 2 * LEN];
static uint32_t ocm_size;
static void package(uint32_t seed, mut_t f, int flip_code, int rec)
{
    uint32_t base = 512u;
    memset(OCM, 0, sizeof OCM);
    wr32(OCM, 0x464D434Fu); OCM[4] = 1; OCM[6] = 2;
    wr32(OCM + 8, FW_API_IFACE); wr32(OCM + 12, FW_API_ADDR); wr32(OCM + 16, ENGI_SHAPES);
    wr32(OCM + 20, 11136u); wr32(OCM + 24, OC_PACK_HASH); wr32(OCM + 44, FW_API_VERSION); wr32(OCM + 48, sizeof(fw_api_t)); wr32(OCM + 52, 6u);
    for (uint32_t k = 0; k < 2u; k++) {
        wr32(OCM + 56 + 16 * k, 6u * k); wr32(OCM + 56 + 16 * k + 4, LEN); wr32(OCM + 56 + 16 * k + 12, base + LEN * k);
        variant(OCM + base + LEN * k, k, seed, f, flip_code, rec);
    }
    wr32(OCM + 88, ocs_crc32(OCM, 88));
    ocm_size = base + 2u * LEN;
}
/* (0.14.1) a relocatable package (format 2): one image linked at 0 (its link address and engine_t pointer 0-based), five sites (the header's two
 * words, two ABS32 words and a MOV_ABS32 pair in the code), each start's image made from it by a reference here (rel_ref: the rule of
 * tools/ocflash.py apply_relocs) and the package naming their CRC-32. mode: 0 good; 1 a list byte flipped (its CRC); 2 the last site past the
 * image's end (CRCs made good); 3 a list of FB_REL_MAX + 1 words (CRCs good); 4 both starts' CRC-32 wrong (the header's CRC made good); 5 the list
 * out of order (CRCs good); 6 (review) the image's offset 0xFFFFFF00 (off + len wraps in 32 bits; CRCs good). FIN[k]: the image start k gets;
 * package_v1() puts the two into a variants package (format 1). RW: the sites (RW_STD, or RW_STRADDLE). */
static const uint32_t RW_STD[] = { (1u << 24) | 28u, (1u << 24) | 32u, (1u << 24) | 0x200u, (2u << 24) | 0x302u, (1u << 24) | 0x404u };
/* (0.14.1, review) sites across the reader's 256-byte chunks (the install reads the image from 0x100 in 256-byte pieces): a MOV_ABS32 pair at 0x3FC
 * (its constant at 0x3FE..0x401, across 0x400), an ABS32 word at 0x4FE (across 0x500), a MOV_ABS32 pair at 0x5FA (the pair across 0x600) */
static const uint32_t RW_STRADDLE[] = { (1u << 24) | 28u, (1u << 24) | 32u, (2u << 24) | 0x3FCu, (1u << 24) | 0x4FEu, (2u << 24) | 0x5FAu };
static const uint32_t *RW = RW_STD;
#define RW_N 5u
static uint8_t FIN[2][LEN], OCM2[256 + LEN + 4u * 2049u];
static uint32_t ocm2_size, RWX[2049];
static uint32_t rd32(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void rel_ref(uint8_t *img, uint32_t base)
{
    for (uint32_t k = 0; k < RW_N; k++) {
        uint32_t a = (RW[k] & 0xFFFFFFu) + (RW[k] >> 24 == 2u ? 2u : 0u);
        wr32(img + a, rd32(img + a) + base);
    }
    wr32(img + 52, ocs_crc32(img + FW_MOD_CODE_OFF, LEN - FW_MOD_CODE_OFF));
    wr32(img + 56, ocs_crc32(img, 56));
}
static void package2(uint32_t seed, int mode)
{
    fw_mod_hdr_t h = mk(0, LEN);
    uint32_t nw = RW_N, at = 256u, k;
    h.link_addr = 0; h.engine_ptr = 0x180u;
    code(seed);
    seal(&h); memcpy(IMG, &h, sizeof h);
    for (k = 0; k < 2u; k++) { memcpy(FIN[k], IMG, LEN); rel_ref(FIN[k], oc_mod_link(k)); }
    memcpy(RWX, RW, RW_N * 4u);
    if (mode == 2) RWX[nw - 1u] = (1u << 24) | (LEN - 2u);
    if (mode == 3) { for (k = 0; k < 2049u; k++) RWX[k] = (1u << 24) | (0x200u + 4u * k); nw = 2049u; }
    if (mode == 5) { RWX[2] = RW[4]; RWX[4] = RW[2]; }
    memset(OCM2, 0, sizeof OCM2);
    wr32(OCM2, 0x464D434Fu); OCM2[4] = 2; OCM2[6] = 1;
    wr32(OCM2 + 8, FW_API_IFACE); wr32(OCM2 + 12, FW_API_ADDR); wr32(OCM2 + 16, ENGI_SHAPES);
    wr32(OCM2 + 20, 11136u); wr32(OCM2 + 24, OC_PACK_HASH); wr32(OCM2 + 44, FW_API_VERSION); wr32(OCM2 + 48, sizeof(fw_api_t)); wr32(OCM2 + 52, 6u);
    wr32(OCM2 + 56, 0xFFFFFFFFu); wr32(OCM2 + 60, LEN); wr32(OCM2 + 64, ocs_crc32(IMG, LEN)); wr32(OCM2 + 68, at);
    memcpy(OCM2 + at, IMG, LEN);
    for (k = 0; k < nw; k++) wr32(OCM2 + at + LEN + 4u * k, RWX[k]);
    wr32(OCM2 + 72, nw); wr32(OCM2 + 76, at + LEN); wr32(OCM2 + 80, ocs_crc32(OCM2 + at + LEN, 4u * nw)); wr32(OCM2 + 84, 2u);
    for (k = 0; k < 2u; k++) { wr32(OCM2 + 88 + 8u * k, 6u * k); wr32(OCM2 + 92 + 8u * k, ocs_crc32(FIN[k], LEN)); }
    if (mode == 4) { OCM2[92] ^= 1u; OCM2[92 + 8] ^= 1u; }
    if (mode == 6) wr32(OCM2 + 68, 0xFFFFFF00u);
    wr32(OCM2 + 104, ocs_crc32(OCM2, 104));
    if (mode == 1) OCM2[at + LEN + 9] ^= 0x40u;
    ocm2_size = at + LEN + 4u * nw;
}
static void package_v1(void)                             /* FIN[0], FIN[1] as the variants package of format 1 (sectors 0 and 6) */
{
    uint32_t base = 512u;
    memset(OCM, 0, sizeof OCM);
    wr32(OCM, 0x464D434Fu); OCM[4] = 1; OCM[6] = 2;
    wr32(OCM + 8, FW_API_IFACE); wr32(OCM + 12, FW_API_ADDR); wr32(OCM + 16, ENGI_SHAPES);
    wr32(OCM + 20, 11136u); wr32(OCM + 24, OC_PACK_HASH); wr32(OCM + 44, FW_API_VERSION); wr32(OCM + 48, sizeof(fw_api_t)); wr32(OCM + 52, 6u);
    for (uint32_t k = 0; k < 2u; k++) {
        wr32(OCM + 56 + 16 * k, 6u * k); wr32(OCM + 56 + 16 * k + 4, LEN); wr32(OCM + 56 + 16 * k + 8, ocs_crc32(FIN[k], LEN)); wr32(OCM + 56 + 16 * k + 12, base + LEN * k);
        memcpy(OCM + base + LEN * k, FIN[k], LEN);
    }
    wr32(OCM + 88, ocs_crc32(OCM, 88));
    ocm_size = base + 2u * LEN;
}
/* ---- the state of the unit */
static uint8_t snap[0x100000];
static uint32_t PRE0 = ENGI_SHAPES, PRE1 = ENGI_SHAPES;   /* the engines of the modules unit() puts at start 0 and 1 */
static void unit(void)                                   /* a unit with a working pack and a working module at start 0 and at start 1 (so a new install is in place) */
{
    memset(nor, 0xFF, sizeof nor);
    make_pack(PAKF, 0);
    memcpy(nor + OC_PACK_LO, PAKF, OC_PACK_LEN);
    for (uint32_t i = 0; i < 2u; i++) {
        uint8_t v[LEN];
        IMG_ENGINE = i ? PRE1 : PRE0;
        variant(v, i, 40u + i, 0, 0, 0);
        memcpy(nor + oc_mod_flash(i), v, LEN);
    }
    IMG_ENGINE = ENGI_SHAPES;
    memset(&pk, 0, sizeof pk); oc_pack_good = 0;
    memset(&oc_modguard, 0, sizeof oc_modguard);
    memset(trk, 0, sizeof trk);
    for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fm1_guard_code_end = 0;
    st_retire_engines(7);
    oc_pack_boot();
    oc_mod_boot(0);
    memset(&fb, 0, sizeof fb);
    strcpy(fb.path, "/C");
    erases = 0;
}
static int working(void)                                 /* the pack sound and the module registered, as the loader finds them after a restart */
{
    memset(&pk, 0, sizeof pk); oc_pack_good = 0;
    st_retire_engines(7);
    oc_pack_boot();
    for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    oc_mod_boot(0);
    return pk.state == PK_OK && oc_pack_good && (oc_mod.state[0] == MD_OK || oc_mod.state[1] == MD_OK) && eng_mod[ENGI_SHAPES] != 0;
}
static void keep(void) { memcpy(snap, nor, sizeof nor); }
static int same(void) { return !memcmp(snap, nor, sizeof nor); }
/* run a single-file install of /C/<name> as the browser does */
static int do_pak(void) { cf_t *c = cf_find("/C/X.PAK"); return fb_pak_file("/C/X.PAK", c->size, 0); }
static int do_mod(void) { cf_t *c = cf_find("/C/X.OCM"); fb.sel = 0; return fb_mod_file("/C/X.OCM", c->size, 0, 0u, 0, 0); }


/* (0.13.5) a set of two modules that both need the in-place question: three presses (ask for the first, install it and ask for the second, install the second). Before the
 * fix the third press began again at the first module, which asked again, and the two asked by turns for ever. The images are written once each (4 sector erases). */
static void package1(uint8_t *out, uint32_t *size, uint32_t engine, uint32_t s, uint32_t seed)
{
    IMG_ENGINE = engine;
    memset(out, 0, 512 + LEN);
    wr32(out, 0x464D434Fu); out[4] = 1; out[6] = 1;
    wr32(out + 8, FW_API_IFACE); wr32(out + 12, FW_API_ADDR); wr32(out + 16, engine);
    wr32(out + 20, 11136u); wr32(out + 24, OC_PACK_HASH); wr32(out + 44, FW_API_VERSION); wr32(out + 48, sizeof(fw_api_t)); wr32(out + 52, 2u);
    wr32(out + 56, (uint32_t)OC_MOD_START_REL(s)); wr32(out + 60, LEN); wr32(out + 68, 512u);
    variant(out + 512, s, seed, 0, 0, 0);
    wr32(out + 72, ocs_crc32(out, 72));
    *size = 512u + LEN;
    IMG_ENGINE = ENGI_SHAPES;
}
static int set_resume(void)
{
    static uint8_t A[512 + LEN], B[512 + LEN], pk2[OC_PACK_LEN];
    uint32_t sa, sb, presses = 0, e0;
    char m[512];
    fb_ent_t e;
    int rc = 1;
    oc_mod_geo[0].sectors = 2u; oc_mod_geo[0].starts = 0x01u; oc_mod_geo[1].sectors = 2u; oc_mod_geo[1].starts = 0x02u;
    PRE0 = ENGI_SHAPES; PRE1 = ENGI_PROPHET;
    unit();
    PRE0 = PRE1 = ENGI_SHAPES;
    chk("(setup) the unit has working modules of SHAPES (start 0) and the Prophet (start 1), 2 sectors each", oc_mod.engine[0] == ENGI_SHAPES && oc_mod.engine[1] == ENGI_PROPHET);
    package1(A, &sa, ENGI_SHAPES, 0u, 90u); package1(B, &sb, ENGI_PROPHET, 1u, 91u);
    cf_put("/C/A.OCM", A, sa); cf_put("/C/B.OCM", B, sb);
    make_pack(pk2, 0); cf_put("/C/S.PAK", pk2, sizeof pk2);
    snprintf(m, sizeof m, "{\n  \"version\": \"%u.%u.%u\",\n  \"pack_hash\": %u,\n  \"pack_len\": %u,\n  \"iface\": %u,\n  \"api_addr\": %u,\n  \"files\": {\"pak\": \"S.PAK\", \"ocm\": [\"A.OCM\", \"B.OCM\"]}\n}\n",
             (OC_FW_VERSION >> 16) & 255u, (OC_FW_VERSION >> 8) & 255u, OC_FW_VERSION & 255u, (unsigned)OC_PACK_HASH, (unsigned)OC_PACK_LEN, (unsigned)FW_API_IFACE, (unsigned)FW_API_ADDR);
    memset(&e, 0, sizeof e); strcpy(e.name, "U.SET.JSON"); e.size = (uint32_t)strlen(m);
    cf_put("/C/U.SET.JSON", (const uint8_t *)m, e.size);
    fb.sel = 0; erases = 0;
    while (rc && presses < 6u) {
        rc = fb_install_set(&e); presses++;
        if (presses == 1u) e0 = (uint32_t)erases;
        if (rc) printf("press %u: %d msg \"%s\" ask %u setq %u\n", presses, rc, fb.msg, fb.ask, fb.setq);
    }
    (void)e0;
    chk("a set of two in-place modules is installed by three presses, not for ever", !rc && presses == 3u && fb.packed == 3);
    chk("... each module written once (2 sectors each: 4 erases)", erases == 4);
    {
        uint8_t va[LEN], vb[LEN];
        IMG_ENGINE = ENGI_SHAPES; variant(va, 0, 90u, 0, 0, 0); IMG_ENGINE = ENGI_PROPHET; variant(vb, 1, 91u, 0, 0, 0); IMG_ENGINE = ENGI_SHAPES;
        chk("... both new images are in flash at their starts", !memcmp(nor + oc_mod_flash(0), va, LEN) && !memcmp(nor + oc_mod_flash(1), vb, LEN));
    }
    oc_mod_geo[0].sectors = 6u; oc_mod_geo[0].starts = 0x03u; oc_mod_geo[1].sectors = 8u; oc_mod_geo[1].starts = 0x05u;     /* the real ones back */
    return 0;
}

/* ---- (Release 3 step 4) effect modules on the card: a package whose engine word is kind 1 (insert) | 0, the module id in the image's fx_desc_t (firmware/src/fw_fx.h):
 * the descriptor at 0x180 (one bank of two effects), the entries at 0x200, two parameter rows at 0x240, a label at 0x300. FXLEN 5000 B: two sectors. fx_image(): the image
 * for `link` (0 for the relocatable one), its pointers link-based; mutation 1 a code byte flipped after the seal, 2 the module id 0, 3 an entry of another kind, 4 a sector tail
 * that is an update record; the header sealed over the code it has (as tools/build.py seals it) */
#define FXLEN 5000u
#define FXID 0x80001234u
static uint32_t FX_VER = 0x00010203u, FX_ID = FXID, FX_WORD = 1u << 8;
static uint8_t FIMG[9u * 4096u];                          /* (its own buffer: IMG holds 8 sectors, the nine-sector case needs more) */
static void fx_image(uint8_t *out, uint32_t link, uint32_t seed, int mutation, uint32_t len)
{
    fw_mod_hdr_t h;
    uint8_t *I = FIMG;
    for (uint32_t k = 0; k < sizeof FIMG; k++) I[k] = (uint8_t)(k * 31u + seed);
    memset(I, 0, FW_MOD_CODE_OFF);
    memset(I + 0x180, 0, 0x200);
    wr32(I + 0x180, 2u); wr32(I + 0x184, mutation == 2 ? 0u : FX_ID); wr32(I + 0x188, FX_VER); wr32(I + 0x18C, link + 0x200u); wr32(I + 0x190, 0);
    I[0x200] = 1; I[0x201] = mutation == 3 ? 2u : (uint8_t)(FX_WORD >> 8); I[0x204] = 2; wr32(I + 0x208, link + 0x240u);
    I[0x210] = 2; I[0x211] = (uint8_t)(FX_WORD >> 8);
    I[0x240] = 1; I[0x242] = I[0x243] = 0xFF; I[0x246] = 100; I[0x248] = 50; wr32(I + 0x24C, link + 0x300u);
    I[0x250] = 3; I[0x252] = I[0x253] = 0xFF; I[0x256] = 10;
    memcpy(I + 0x300, "MIX", 4);
    if (mutation == 4) memcpy(I + 0xF00u, REC, 80);
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.engine = FX_WORD; h.state_size = 0; h.engine_ptr = link + 0x180u; h.link_addr = link; h.pack_hash = OC_PACK_HASH; h.length = len; h.version = FX_VER;
    h.crc = ocs_crc32(I + FW_MOD_CODE_OFF, len - FW_MOD_CODE_OFF);
    h.hcrc = ocs_crc32((const uint8_t *)&h, FW_MOD_HDR_BYTES - 4u);
    memcpy(I, &h, sizeof h);
    if (mutation == 1) I[0x700] ^= 1u;
    memcpy(out, I, len);
}
/* a variants package (format 1) of the effect module for the starts st[0..ns) (start numbers), each image linked for its start */
static uint8_t FXP[512 + 3u * 8u * 4096u];
static uint32_t fxp_size;
static void fx_package(const uint32_t *st, uint32_t ns, uint32_t seed, int mutation, uint32_t len)
{
    uint32_t base = 512u, k;
    memset(FXP, 0, sizeof FXP);
    wr32(FXP, 0x464D434Fu); FXP[4] = 1; FXP[6] = (uint8_t)ns;
    wr32(FXP + 8, FW_API_IFACE); wr32(FXP + 12, FW_API_ADDR); wr32(FXP + 16, FX_WORD);
    wr32(FXP + 20, 0u); wr32(FXP + 24, OC_PACK_HASH); wr32(FXP + 44, FW_API_VERSION); wr32(FXP + 48, sizeof(fw_api_t)); wr32(FXP + 52, oc_mod_nsec(len));
    for (k = 0; k < ns; k++) {
        wr32(FXP + 56 + 16 * k, (uint32_t)OC_MOD_START_REL(st[k])); wr32(FXP + 56 + 16 * k + 4, len); wr32(FXP + 56 + 16 * k + 12, base + len * k);
        fx_image(FXP + base + len * k, oc_mod_link(st[k]), seed, mutation, len);
        wr32(FXP + 56 + 16 * k + 8, ocs_crc32(FXP + base + len * k, len));
    }
    wr32(FXP + 56 + 16 * ns, ocs_crc32(FXP, 56 + 16 * ns));
    fxp_size = base + len * ns;
}
/* the relocatable package (format 2) of the same image, for the starts st[]: five sites (the header's engine_ptr and link address, the entries, the parameter rows, a label) */
static const uint32_t RW_FX[] = { (1u << 24) | 28u, (1u << 24) | 32u, (1u << 24) | 0x18Cu, (1u << 24) | 0x208u, (1u << 24) | 0x24Cu };
static uint8_t FXP2[256 + FXLEN + 64];
static uint32_t fxp2_size;
static void fx_package2(const uint32_t *st, uint32_t ns, uint32_t seed)
{
    uint8_t one[FXLEN];
    uint32_t at = 256u, k;
    fx_image(one, 0u, seed, 0, FXLEN);
    memset(FXP2, 0, sizeof FXP2);
    wr32(FXP2, 0x464D434Fu); FXP2[4] = 2; FXP2[6] = 1;
    wr32(FXP2 + 8, FW_API_IFACE); wr32(FXP2 + 12, FW_API_ADDR); wr32(FXP2 + 16, FX_WORD);
    wr32(FXP2 + 20, 0u); wr32(FXP2 + 24, OC_PACK_HASH); wr32(FXP2 + 44, FW_API_VERSION); wr32(FXP2 + 48, sizeof(fw_api_t)); wr32(FXP2 + 52, 2u);
    wr32(FXP2 + 56, 0xFFFFFFFFu); wr32(FXP2 + 60, FXLEN); wr32(FXP2 + 64, ocs_crc32(one, FXLEN)); wr32(FXP2 + 68, at);
    memcpy(FXP2 + at, one, FXLEN);
    for (k = 0; k < 5u; k++) wr32(FXP2 + at + FXLEN + 4u * k, RW_FX[k]);
    wr32(FXP2 + 72, 5u); wr32(FXP2 + 76, at + FXLEN); wr32(FXP2 + 80, ocs_crc32(FXP2 + at + FXLEN, 20u)); wr32(FXP2 + 84, ns);
    for (k = 0; k < ns; k++) {
        uint8_t fin[FXLEN];
        uint32_t j, base = oc_mod_link(st[k]);
        memcpy(fin, one, FXLEN);
        for (j = 0; j < 5u; j++) wr32(fin + (RW_FX[j] & 0xFFFFFFu), rd32(fin + (RW_FX[j] & 0xFFFFFFu)) + base);
        wr32(fin + 52, ocs_crc32(fin + FW_MOD_CODE_OFF, FXLEN - FW_MOD_CODE_OFF)); wr32(fin + 56, ocs_crc32(fin, 56));
        wr32(FXP2 + 88 + 8u * k, (uint32_t)OC_MOD_START_REL(st[k])); wr32(FXP2 + 92 + 8u * k, ocs_crc32(fin, FXLEN));
    }
    wr32(FXP2 + 88 + 8u * ns, ocs_crc32(FXP2, 88 + 8u * ns));
    fxp2_size = at + FXLEN + 20u;
}
/* a unit with its pack and ONE SHAPES module, at start 0 (sectors 0..5 from 0xC8000): sectors 6..11 free (the six low ones are not the area's on this unit) */
static void unit_n(int shapes)                          /* shapes 0: the pack and no module at all */
{
    uint8_t v[LEN];
    memset(nor, 0xFF, sizeof nor);
    make_pack(PAKF, 0);
    memcpy(nor + OC_PACK_LO, PAKF, OC_PACK_LEN);
    IMG_ENGINE = ENGI_SHAPES;
    variant(v, 0, 40u, 0, 0, 0);
    if (shapes) memcpy(nor + oc_mod_flash(0), v, LEN);
    memset(&pk, 0, sizeof pk); oc_pack_good = 0;
    memset(&oc_modguard, 0, sizeof oc_modguard);
    memset(trk, 0, sizeof trk);
    for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e);
    memset(&oc_mod, 0, sizeof oc_mod);
    fm1_guard_code_end = 0;
    st_retire_engines(7);
    oc_pack_boot();
    oc_mod_boot(0);
    memset(&fb, 0, sizeof fb);
    strcpy(fb.path, "/C");
    erases = 0;
}
static void unit_one(void) { unit_n(1); }
static int do_fx(const uint8_t *d, uint32_t n) { cf_put("/C/F.OCM", d, n); fb.sel = 0; return fb_mod_file("/C/F.OCM", n, 0, 0u, 0, 0); }
static int fx_at(uint32_t mid, uint32_t start) { int k = oc_fx_find(mid); return k >= 0 && oc_mod.fx[k].start == start && oc_mod.state[start] == MD_OK; }
static int fx_set(const char *names)                    /* a set manifest of S.PAK and the modules `names` (a JSON list's inside), installed; its return */
{
    char m[512];
    fb_ent_t e;
    snprintf(m, sizeof m, "{\n  \"version\": \"%u.%u.%u\",\n  \"pack_hash\": %u,\n  \"pack_len\": %u,\n  \"iface\": %u,\n  \"api_addr\": %u,\n  \"files\": {\"pak\": \"S.PAK\", \"ocm\": [%s]}\n}\n",
             (OC_FW_VERSION >> 16) & 255u, (OC_FW_VERSION >> 8) & 255u, OC_FW_VERSION & 255u, (unsigned)OC_PACK_HASH, (unsigned)OC_PACK_LEN, (unsigned)FW_API_IFACE, (unsigned)FW_API_ADDR, names);
    memset(&e, 0, sizeof e); strcpy(e.name, "FX.SET.JSON"); e.size = (uint32_t)strlen(m);
    cf_put("/C/FX.SET.JSON", (const uint8_t *)m, e.size);
    fb.sel = 0;
    return fb_install_set(&e);
}
static void fx_cases(void)
{
    static const uint32_t S68[] = { 1u, 8u }, S0168[] = { 0u, 1u, 8u }, S1[] = { 1u };
    static uint8_t keepf[sizeof FXP], after1[0x100000];
    uint32_t keepn;
    uint8_t pk2[OC_PACK_LEN];
    char t[200];
    unit_one();
    chk("(fx setup) one SHAPES module at start 0, the low sectors not the area's, no effect module", oc_mod.state[0] == MD_OK && oc_mod.state[1] == MD_EMPTY && !OC_MOD_LOW_OK() && !oc_mod.nfxmod);
    /* the good one: planned at start 1 (sector 6, the first free span of two sectors), written there: two erases */
    fx_package(S68, 2u, 80u, 0, FXLEN);
    chk("an effect module's package (kind 1, sectors 6 and 8) installs at the start the loader plans: start 1, two sector erases",
        do_fx(FXP, fxp_size) == 0 && erases == 2 && fx_at(FXID, 1u) && oc_mod.nfxmod == 1 && oc_mod.fx[0].version == FX_VER && oc_mod.fx[0].kind == 1 && oc_mod.fx[0].nfx == 2);
    { uint8_t want[FXLEN]; fx_image(want, oc_mod_link(1), 80u, 0, FXLEN); chk("... the flash at start 1 holds the image for start 1, byte for byte", !memcmp(nor + oc_mod_flash(1), want, FXLEN)); }
    chk("... after a restart the Effects table has it and SHAPES still works", working() && fx_at(FXID, 1u));
    memcpy(after1, nor, sizeof nor);
    /* a newer version: BESIDE at start 8 (sector 8; 6..7 hold the old one), the old one erased after the new one reads back */
    FX_VER = 0x00010204u; fx_package(S68, 2u, 81u, 0, FXLEN); erases = 0;
    chk("a newer version of the id goes beside, at start 8, and the old one at start 1 is erased", do_fx(FXP, fxp_size) == 0 && fx_at(FXID, 8u) && oc_mod.state[1] == MD_EMPTY &&
        oc_mod.fx[oc_fx_find(FXID)].version == 0x00010204u && oc_mod.nfxmod == 1 && working());
    FX_VER = 0x00010203u;
    /* refusals, each before the first erase: the flash unchanged, the module and SHAPES working */
    {
        static const char *const what[] = { "", "a code byte flipped", "a descriptor with module id 0", "an entry of another kind than the header's", "a sector tail that is an update record" };
        static const char *const msg[] = { "", "Module file damaged", "Module header bad", "Module header bad", "Update would erase it" };
        for (int mu = 1; mu <= 4; mu++) {
            memcpy(nor, after1, sizeof nor); for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e); memset(&oc_mod, 0, sizeof oc_mod); oc_mod_boot(0);
            fx_package(S68, 2u, 82u, mu, FXLEN); keep(); erases = 0;
            snprintf(t, sizeof t, "an effect package with %s: refused '%s', zero erases, the flash unchanged, the old effect module working", what[mu], msg[mu]);
            chk(t, do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, msg[mu]) && erases == 0 && same() && working() && fx_at(FXID, 1u) && !fb.ask);
        }
        memcpy(nor, after1, sizeof nor); for (uint32_t e = 0; e < NENGINES; e++) eng_unregister(e); memset(&oc_mod, 0, sizeof oc_mod); oc_mod_boot(0);
        FX_WORD = (1u << 8) | ENGI_SHAPES; fx_package(S68, 2u, 83u, 0, FXLEN); keep(); erases = 0;
        chk("an engine word of kind 1 with an engine number: refused 'Not a module', zero erases", do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, "Not a module") && erases == 0 && same());
        FX_WORD = 4u << 8; fx_package(S68, 2u, 83u, 0, FXLEN); erases = 0;
        chk("an engine word of kind 4: refused 'Not a module', zero erases", do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, "Not a module") && erases == 0 && same());
        FX_WORD = 1u << 8;
        fx_package(S1, 1u, 84u, 0, 9u * 4096u); erases = 0;
        chk("an effect module of nine sectors: refused 'Module header bad', zero erases", do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, "Module header bad") && erases == 0 && same());
        { static const uint32_t S0[] = { 0u }; fx_package(S0, 1u, 85u, 0, FXLEN); }
        erases = 0;
        chk("a package with no image for the start the loader plans (only sector 0, where SHAPES stands): 'No image for it', zero erases",
            do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, "No image for it") && erases == 0 && same());
    }
    /* no room: no engine module, another effect module of eight sectors at start 0 (sectors 0..7): a six-sector module of this id has no free span (8..11 are four); the
     * module in the way is named ("EFFECT at 0"), nothing erased. (An engine module as the blocker is named by its engine_t's name, which a host build cannot read:
     * the engine blocker's text is usb_host_test's.) */
    { static const uint32_t S0[] = { 0u };
      unit_n(0); FX_ID = FXID + 7u; fx_package(S0, 1u, 86u, 0, 8u * 4096u); FX_ID = FXID; }
    chk("(fx setup) no engine module; another effect module of eight sectors at start 0", do_fx(FXP, fxp_size) == 0 && fx_at(FXID + 7u, 0u));
    fx_package(S68, 2u, 86u, 0, 6u * 4096u); keep(); erases = 0;
    chk("no free span of six sectors (an effect module at 0..7): refused naming the module in the way ('EFFECT at 0: remove it'), zero erases",
        do_fx(FXP, fxp_size) == 1 && !strcmp(fb.msg, "EFFECT at 0: remove it") && erases == 0 && same() && fx_at(FXID + 7u, 0u));
    /* the relocatable package (format 2) installs the same flash as the variants package of the same image */
    unit_one(); fx_package2(S68, 2u, 87u);
    chk("a relocatable effect package (format 2): installs at start 1", do_fx(FXP2, fxp2_size) == 0 && fx_at(FXID, 1u) && working());
    memcpy(after1, nor, sizeof nor);
    unit_one(); fx_package(S68, 2u, 87u, 0, FXLEN); do_fx(FXP, fxp_size);
    { uint8_t v[FXLEN]; fx_image(v, oc_mod_link(1), 87u, 0, FXLEN); chk("... and its flash is byte for byte the variants package's (the image linked for start 1)", !memcmp(after1, nor, sizeof nor) && !memcmp(nor + oc_mod_flash(1), v, FXLEN)); }
    /* a set: the pack, SHAPES (beside at start 1: 6..11) and the effect module, which the planner then puts at start 0 (sectors 0..1, freed by SHAPES' move) */
    unit_one(); make_pack(pk2, 0); cf_put("/C/S.PAK", pk2, sizeof pk2);
    package(90, 0, 0, 0); cf_put("/C/M.OCM", OCM, ocm_size);
    fx_package(S0168, 3u, 91u, 0, FXLEN); cf_put("/C/F.OCM", FXP, fxp_size); memcpy(keepf, FXP, fxp_size); keepn = fxp_size;
    erases = 0;
    chk("a set of SHAPES and an effect module: the effect module is planned on the area SHAPES' install leaves (start 0) and installed there",
        fx_set("\"M.OCM\", \"F.OCM\"") == 0 && fb.packed == 3 && fx_at(FXID, 0u) && oc_mod.state[1] == MD_OK && oc_mod.engine[1] == ENGI_SHAPES && working());
    unit_one(); fx_package(S68, 2u, 92u, 0, FXLEN); cf_put("/C/F.OCM", FXP, fxp_size); keep(); erases = 0;
    chk("the same set whose effect package has no image for start 0: refused 'No image for it' before the first stage, zero erases",
        fx_set("\"M.OCM\", \"F.OCM\"") == 1 && !strcmp(fb.msg, "No image for it") && erases == 0 && same() && working());
    cf_put("/C/F.OCM", keepf, keepn); cf_put("/C/G.OCM", keepf, keepn); erases = 0;
    chk("a set naming one effect module id twice: 'Modules don't fit', zero erases", fx_set("\"F.OCM\", \"G.OCM\"") == 1 && !strcmp(fb.msg, "Modules don't fit") && erases == 0 && same());
    /* two effect modules of four sectors, each of which fits the six free sectors alone: the planner, counting both, refuses the set */
    fx_package(S68, 2u, 93u, 0, 4u * 4096u - 512u); cf_put("/C/F.OCM", FXP, fxp_size);
    FX_ID = FXID + 1u; fx_package(S68, 2u, 94u, 0, 4u * 4096u - 512u); cf_put("/C/G.OCM", FXP, fxp_size); FX_ID = FXID;
    chk("(fx setup) each four-sector module alone plans a start", oc_fxmod_plan(FXID, 4u) >= 0 && oc_fxmod_plan(FXID + 1u, 4u) >= 0);
    erases = 0;
    chk("a set of two four-sector effect modules for six free sectors: 'Modules don't fit' (the sectors counted with the planner), zero erases",
        fx_set("\"F.OCM\", \"G.OCM\"") == 1 && !strcmp(fb.msg, "Modules don't fit") && erases == 0 && same() && working());
    fx_package(S68, 2u, 95u, 0, 2u * 4096u); cf_put("/C/F.OCM", FXP, fxp_size);
    FX_ID = FXID + 1u; fx_package(S68, 2u, 96u, 0, 2u * 4096u); cf_put("/C/G.OCM", FXP, fxp_size); FX_ID = FXID;
    erases = 0;
    chk("a set of two two-sector effect modules (they fit: starts 1 and 8): both installed", fx_set("\"F.OCM\", \"G.OCM\"") == 0 && fx_at(FXID, 1u) && oc_fx_find(FXID + 1u) >= 0 &&
        oc_mod.nfxmod == 2 && working());
    /* an engine-only set is judged as before (the planner's count is not asked): SHAPES twice is refused by the pair rule */
    unit_one(); erases = 0;
    chk("an engine-only set is judged as before: the same engine twice, 'Modules don't fit'", fx_set("\"M.OCM\", \"M.OCM\"") == 1 && !strcmp(fb.msg, "Modules don't fit") && erases == 0);
}

int main(void)
{
    uint8_t pk2[OC_PACK_LEN];
    setvbuf(stdout, NULL, _IONBF, 0);
    for (uint32_t k = 0; k < 80u; k++) REC[k] = (uint8_t)((k * 11u + 5u) | 1u);
    REC[6] = 0x41; REC[7] = 0x54;
    { uint32_t c = pk_crc16(REC + 2, 78u); REC[0] = (uint8_t)c; REC[1] = (uint8_t)(c >> 8); }
    chk("(setup) the record is one the loader's own rule takes", pk_record_like(REC));

    /* ---- a pack */
    unit(); keep();
    chk("(setup) the unit starts with a working pack and module", working());
    erases = 0;
    make_pack(pk2, 1); pk2[(OC_PACK_HDR + OC_PACK_LEN) / 2u] ^= 0x10;          /* a good header, one payload byte flipped */
    cf_put("/C/X.PAK", pk2, sizeof pk2);
    chk("a pack file with one payload byte flipped: refused 'Pack file damaged'", do_pak() == 1 && !strcmp(fb.msg, "Pack file damaged"));
    chk("... zero sector erases, the flash unchanged, the old pack and module still there", erases == 0 && same() && working());
    make_pack(pk2, 2); memcpy(pk2 + 0x1000 - 0x100, REC, 80);   /* sector 0's tail an update record (header fixed up after) */
    wr32(pk2 + 16, ocs_crc32(pk2 + OC_PACK_HDR, OC_PACK_LEN - OC_PACK_HDR)); wr32(pk2 + 508, ocs_crc32(pk2, 508));
    cf_put("/C/X.PAK", pk2, sizeof pk2);
    erases = 0;
    chk("a pack whose sector tail is an update record: refused 'Update would erase it'", do_pak() == 1 && !strcmp(fb.msg, "Update would erase it"));
    chk("... zero sector erases, the flash unchanged, the old pack still there", erases == 0 && same() && working());
    make_pack(pk2, 3); pk2[0] ^= 1u;                              /* not the pack's magic */
    cf_put("/C/X.PAK", pk2, sizeof pk2); erases = 0;
    chk("a file that is not this build's pack: refused, nothing erased", do_pak() == 1 && !strcmp(fb.msg, "Not this build's pack") && erases == 0 && same());
    make_pack(pk2, 4);                                            /* the good one installs */
    cf_put("/C/X.PAK", pk2, sizeof pk2); erases = 0;
    chk("a good pack file installs (erases happen, the pack is the new one)", do_pak() == 0 && erases >= PK_SECTORS && !memcmp(nor + OC_PACK_LO, pk2, OC_PACK_LEN) && working());

    /* ---- a module (both starts hold the module) */
    unit();
    package(60, 0, 1, 0);                                         /* one code byte flipped in both variants */
    cf_put("/C/X.OCM", OCM, ocm_size); keep();
    chk("a module file with one code byte flipped: refused 'Module file damaged', the in-place question not asked",
        do_mod() == 1 && !strcmp(fb.msg, "Module file damaged") && !fb.ask);
    chk("... zero sector erases, the flash unchanged, the old pack and module still registered", erases == 0 && same() && working());
    unit();
    package(61, m_link, 0, 0); cf_put("/C/X.OCM", OCM, ocm_size); keep();
    chk("a module whose inner header has a wrong link address: refused 'Module header bad', no question",
        do_mod() == 1 && !strcmp(fb.msg, "Module header bad") && !fb.ask);
    chk("... zero sector erases, the flash unchanged, the old module still registered", erases == 0 && same() && working());
    unit();
    package(62, 0, 0, 1); cf_put("/C/X.OCM", OCM, ocm_size); keep();
    chk("a module whose sector tail matches the loader's record rule: refused 'Update would erase it', no question",
        do_mod() == 1 && !strcmp(fb.msg, "Update would erase it") && !fb.ask);
    chk("... zero sector erases, the flash unchanged, the old module still registered", erases == 0 && same() && working());
    unit();
    package(63, 0, 0, 0); cf_put("/C/X.OCM", OCM, ocm_size);
    chk("the good package installs (the in-place question is covered by usb_host_test): erases happen, the unit works",
        do_mod() == 0 && erases > 0 && working());

    /* ---- (0.14.1) a relocatable package (format 2): the flash after its install is the flash after the variants package's of the same images */
    {
        static uint8_t after_v1[0x100000];
        static const char *const what[] = { "", "a list byte flipped (its CRC)", "a site past the image's end", "a list longer than FB_REL_MAX",
                                            "each start's CRC-32 not its relocated image's", "the list out of order",
                                            "the image's offset 0xFFFFFF00 (off + len wraps in 32 bits)" };
        unit(); package2(70, 0); package_v1(); cf_put("/C/X.OCM", OCM, ocm_size);
        chk("(setup) the variants package of the reference images installs", do_mod() == 0 && working());
        memcpy(after_v1, nor, sizeof nor);
        unit(); package2(70, 0); cf_put("/C/X.OCM", OCM2, ocm2_size);
        chk("the relocatable package installs: the unit works", do_mod() == 0 && erases > 0 && working());
        chk("... and the flash is byte for byte the flash the variants package left (the installed image is the variant's)", !memcmp(after_v1, nor, sizeof nor));
        for (int mode = 1; mode <= 6; mode++) {
            char t[160];
            unit(); package2(71, mode); cf_put("/C/X.OCM", OCM2, ocm2_size); keep();
            snprintf(t, sizeof t, "a relocatable package with %s: refused 'Module file damaged', zero erases, the old module working", what[mode]);
            chk(t, do_mod() == 1 && !strcmp(fb.msg, "Module file damaged") && erases == 0 && same() && working() && !fb.ask);
        }
        /* (review) the sites across the 256-byte chunks: the same flash as the variants package of the same images */
        RW = RW_STRADDLE;
        unit(); package2(72, 0); package_v1(); cf_put("/C/X.OCM", OCM, ocm_size);
        chk("(setup) the variants package of images with sites across the 256-byte chunks installs", do_mod() == 0 && working());
        memcpy(after_v1, nor, sizeof nor);
        unit(); package2(72, 0); cf_put("/C/X.OCM", OCM2, ocm2_size);
        chk("sites across the reader's 256-byte chunks (MOV32 at 0x3FC, ABS32 at 0x4FE, MOV32 at 0x5FA): installs, the unit works", do_mod() == 0 && erases > 0 && working());
        chk("... and the flash is byte for byte the variants package's", !memcmp(after_v1, nor, sizeof nor));
        RW = RW_STD;
    }

    /* ---- a set with one bad file: refused whole, before its first stage (the pack in the set is not installed even though it is good) */
    {
        static const char man[] = "{\n  \"version\": \"0.0.0\",\n  \"pack_hash\": 0,\n  \"pack_len\": 0,\n  \"iface\": 0,\n  \"api_addr\": 0,\n  \"files\": {\"pak\": \"S.PAK\", \"ocm\": [\"M.OCM\"]}\n}\n";
        char m[512];
        fb_ent_t e;
        snprintf(m, sizeof m, "{\n  \"version\": \"%u.%u.%u\",\n  \"pack_hash\": %u,\n  \"pack_len\": %u,\n  \"iface\": %u,\n  \"api_addr\": %u,\n  \"files\": {\"pak\": \"S.PAK\", \"ocm\": [\"M.OCM\"]}\n}\n",
                 (OC_FW_VERSION >> 16) & 255u, (OC_FW_VERSION >> 8) & 255u, OC_FW_VERSION & 255u, (unsigned)OC_PACK_HASH, (unsigned)OC_PACK_LEN, (unsigned)FW_API_IFACE, (unsigned)FW_API_ADDR);
        (void)man;
        memset(&e, 0, sizeof e); strcpy(e.name, "T.SET.JSON"); e.size = (uint32_t)strlen(m);
        cf_put("/C/T.SET.JSON", (const uint8_t *)m, e.size);
        /* the pack bad */
        unit(); make_pack(pk2, 5); pk2[OC_PACK_HDR + 77u] ^= 1u; cf_put("/C/S.PAK", pk2, sizeof pk2);
        package(64, 0, 0, 0); cf_put("/C/M.OCM", OCM, ocm_size); keep();
        chk("a set whose pack is damaged: refused 'Pack file damaged'", fb_install_set(&e) == 1 && !strcmp(fb.msg, "Pack file damaged"));
        chk("... zero erases, nothing changed, the old pack and module working", erases == 0 && same() && working());
        /* a module bad, the pack good: the pack must not be written first */
        unit(); make_pack(pk2, 6); cf_put("/C/S.PAK", pk2, sizeof pk2);
        package(65, 0, 1, 0); cf_put("/C/M.OCM", OCM, ocm_size); keep();
        chk("a set whose module has a flipped code byte (its pack good): refused 'Module file damaged'", fb_install_set(&e) == 1 && !strcmp(fb.msg, "Module file damaged"));
        chk("... zero erases (the good pack was not written first), the old pack and module working", erases == 0 && same() && working() && !fb.ask);
        unit(); package(66, 0, 0, 1); cf_put("/C/M.OCM", OCM, ocm_size); keep();
        chk("a set whose module has a record on its sector tail: refused 'Update would erase it', zero erases", fb_install_set(&e) == 1 && !strcmp(fb.msg, "Update would erase it") && erases == 0 && same() && working());
        unit(); package(67, m_link, 0, 0); cf_put("/C/M.OCM", OCM, ocm_size); keep();
        chk("a set whose module has a wrong link address: refused 'Module header bad', zero erases", fb_install_set(&e) == 1 && !strcmp(fb.msg, "Module header bad") && erases == 0 && same() && working());
        /* the good set: the pack (not this build's yet) and the module install */
        unit();
        memset(nor + OC_PACK_LO, 0xFF, OC_PACK_AREA); memset(&pk, 0, sizeof pk); oc_pack_good = 0; oc_pack_boot();
        make_pack(pk2, 7); cf_put("/C/S.PAK", pk2, sizeof pk2); package(68, 0, 0, 0); cf_put("/C/M.OCM", OCM, ocm_size);
        erases = 0; fb.ask = 0;
        chk("the good set installs whole (the pack, then the module): erases happen, the unit works", fb_install_set(&e) == 0 && fb.packed == 3 && erases >= PK_SECTORS && working());
    }
    set_resume();
    fx_cases();                                          /* (Release 3 step 4) effect modules on the card */
    printf("card install: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
