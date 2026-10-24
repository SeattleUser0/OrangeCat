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
 */
#include <stdint.h>
#define OC_MOD_HOST_XIP 1
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
static fw_mod_hdr_t mk(uint32_t i, uint32_t len)
{
    fw_mod_hdr_t h;
    memset(&h, 0, sizeof h);
    h.magic = FW_MOD_MAGIC; h.iface = FW_API_IFACE; h.api_size = sizeof(fw_api_t); h.api_version = FW_API_VERSION; h.api_addr = FW_API_ADDR;
    h.engine = ENGI_SHAPES; h.state_size = 1234u; h.engine_ptr = oc_mod_link(i) + 0x180u; h.link_addr = oc_mod_link(i);
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
/* ---- the state of the unit */
static uint8_t snap[0x100000];
static void unit(void)                                   /* a unit with a working pack and a working module at start 0 and at start 1 (so a new install is in place) */
{
    memset(nor, 0xFF, sizeof nor);
    make_pack(PAKF, 0);
    memcpy(nor + OC_PACK_LO, PAKF, OC_PACK_LEN);
    for (uint32_t i = 0; i < 2u; i++) {
        uint8_t v[LEN];
        variant(v, i, 40u + i, 0, 0, 0);
        memcpy(nor + oc_mod_flash(i), v, LEN);
    }
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
    printf("card install: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
