/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 3 (docs/orangecat/MODULE_FORMAT_R3.md 4, 5, 7): fw_pages version 3 on the device's wire bytes, built with OC_HOME_PIC=1.
 *   cc -O1 -w -DOC_HOME_PIC=1 -Ibuild/gen -Ifirmware/src -Itests -o mod_ui_test tests/mod_ui_test.c -lm
 * Checked:
 *   PAGES    fw_pages_check_ex: a good v3 descriptor passes; ext, a version 2 descriptor, a bad pid table (duplicate id, id 0xFFFF, a duplicate legacy slot, a slot at or above
 *            P_COUNT, one in P_RESERVED33..36, a slot on a removed entry, a table outside the image) and the effect scope's rules (SC_FX rows, no hook, no pid table) are named
 *   UI       fw_ui_check: a good picture; every bad field: magic, size, version, flags, nk, scene length, an address outside the image, a scene scene_ok refuses, a
 *            lower case label, a kind or shape out of range, depth -128, a name that is too long, a knob with no name
 *   REGISTRY modui_publish: the knobs resolved through the pid table to track slots, the ranges copied; a pid the table lacks dropped (a macro keeps its other targets);
 *            a refused picture is published as a record that says why and leaves the old HOME; the registry's room; modui_retire clears it and moves modui_gen
 *   SCENE    scene_of answers a module's validated scene before the pack's
 *   MACRO    the interrupt's mac_on / mac_off over a module's table use the range in the record (not param_desc_of), put on and come off exactly; after modui_retire
 *            nothing is put on
 *   RETIRE   the order the loader keeps: oc_mod_unreg retires first (read in oc_module.c's text)
 * Revisions:
 *   (Release 3, step 3, branch r3-pages) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "oc_modui.c"                              /* (oc_module.c includes it on the unit; this host build has no module loader) */
#include "fw_pages_wire.h"

static int nbad, nchk;
static void mchk(const char *what, int ok)
{
    nchk++;
    if (!ok || getenv("VERBOSE")) printf("mod_ui: %-96s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
#define BASE 0x10200000u
static uint8_t img[4096];
static int img_rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    (void)ctx;
    if (addr < BASE || addr - BASE + n > sizeof img) return 1;
    memcpy(buf, img + (addr - BASE), n);
    return 0;
}
#define LO (BASE + 0x100u)
#define HI (BASE + sizeof img)

static const char *const NAMES[] = {"A", "B"};
static const page_t ROWS[2] = {{"X ONE", FAM_EDIT, SC_MOD, GR_NONE, {0, 1, 2, 0xFF}}, {"X TWO", FAM_EDIT, SC_MOD, GR_NONE, {3, 0xFF, 0xFF, 0xFF}}};
static const page_t ROWS_FX[2] = {{"X ONE", FAM_EDIT, SC_FX, GR_NONE, {0, 1, 2, 0xFF}}, {"X TWO", FAM_EDIT, SC_FX, GR_NONE, {3, 0xFF, 0xFF, 0xFF}}};
static const param_desc_t PANEL[4] = {
    {"CUT", F_INT, 0, 127, 64, 0, 0}, {"RES", F_INT, 0, 100, 10, 0, 0}, {"MODE", F_ENUM, 0, 1, 0, NAMES, 0}, {"DRY", F_INT, -20, 20, 0, 0, 0},
};
/* ids 10, 11, 12, 13; legacy slots P_E1, P_E2, none, P_E3 */
static const fw_pid_t PID[4] = {{10, P_E1}, {11, P_E2}, {12, FW_PID_NONE}, {13, P_E3}};
static const uint8_t SCENE[] = {SC_VERSION, 100, 50, 0, 0};       /* an empty picture: the header, END */
static const fw_kdef_t KDEF[4] = {
    {"CUT", 1, 0, 10, {{FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
    {"TONE", 2, 0, 0, {{11, KS_UP, 90}, {13, KS_DOWN, -50}, {99, KS_LIN, 40}, {12, KS_LIN, 30}}, 0},      /* 99: no such pid; 12: no legacy slot */
    {"PLUCK", 2, 0, 0, {{10, KS_LIN, 127}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
    {"NONE", 0, 0, 0, {{FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
};
static fw_ui_t UI = {FW_UI_MAGIC, FW_UI_WIRE, 1, sizeof SCENE, 4, 0, SCENE, KDEF};
static fw_pages_t D;
static uint32_t desc;
static void make(const page_t *rows, const fw_pid_t *pid, const fw_ui_t *ui)
{
    memset(&D, 0, sizeof D);
    D.magic = FW_PAGES_MAGIC; D.size = FW_PAGES_WIRE; D.version = FW_PAGES_VERSION; D.npages = 2; D.npanel = 4;
    D.rows = rows; D.panel = PANEL; D.pid = pid; D.ui = ui;
    memset(D.knob, 0xFF, 4); memset(D.cc, 0xFF, 4);
    if (!fwp_wire(&D, img, sizeof img, BASE, &desc)) { printf("mod_ui: wire image full\n"); exit(2); }
}
static int pages(uint32_t scope) { return fw_pages_check_ex(img_rd, 0, desc, LO, HI, scope, P_COUNT, 0); }
static void put32(uint32_t at, uint32_t v) { img[at] = (uint8_t)v; img[at + 1] = (uint8_t)(v >> 8); img[at + 2] = (uint8_t)(v >> 16); img[at + 3] = (uint8_t)(v >> 24); }
static uint32_t d_off(void) { return desc - BASE; }
static uint32_t pid_off(void) { return (img[d_off() + 72] | img[d_off() + 73] << 8 | img[d_off() + 74] << 16 | (uint32_t)img[d_off() + 75] << 24) - BASE; }
static uint32_t ui_off(void) { return (img[d_off() + 68] | img[d_off() + 69] << 8 | img[d_off() + 70] << 16 | (uint32_t)img[d_off() + 71] << 24) - BASE; }
static uint32_t kd_off(void) { return (img[ui_off() + 16] | img[ui_off() + 17] << 8 | img[ui_off() + 18] << 16 | (uint32_t)img[ui_off() + 19] << 24) - BASE; }
static int uicheck(fw_ui_info_t *o) { return fw_ui_check(img_rd, 0, BASE + ui_off(), LO, HI, scene_ok, o); }

static void t_pages(void)
{
    make(ROWS, PID, 0);
    mchk("PAGES  a good version 3 descriptor with a pid table passes as an engine's", pages(SC_MOD) == FWP_OK);
    make(ROWS, 0, 0);
    mchk("PAGES  .. and with no pid table (the id is the panel index)", pages(SC_MOD) == FWP_OK);
    mchk("PAGES  the descriptor is 80 bytes, the picture 20, a knob 28, a pid 4", FW_PAGES_WIRE == 80u && FW_UI_WIRE == 20u && FW_KDEF_WIRE == 28u && FW_PID_WIRE == 4u && P_COUNT <= 256);
    make(ROWS, PID, 0); put32(d_off() + 76, 1);
    mchk("PAGES  ext nonzero: FWP_EXT", pages(SC_MOD) == FWP_EXT);
    make(ROWS, PID, 0); img[d_off() + 6] = 2;
    mchk("PAGES  a version 2 descriptor is refused (every module is rebuilt)", pages(SC_MOD) == FWP_SIZE);
    make(ROWS, PID, 0); img[d_off() + 4] = 68;
    mchk("PAGES  the old 68 byte size is refused", pages(SC_MOD) == FWP_SIZE);
    {   fw_pid_t b[4]; memcpy(b, PID, sizeof b);
        b[1].id = 10; make(ROWS, b, 0);
        mchk("PAGES  two entries of one id: FWP_PID", pages(SC_MOD) == FWP_PID);
        memcpy(b, PID, sizeof b); b[2].id = FW_PID_NONE; make(ROWS, b, 0);
        mchk("PAGES  an id of 0xFFFF (the unused-target mark): FWP_PID", pages(SC_MOD) == FWP_PID);
        memcpy(b, PID, sizeof b); b[3].legacy = P_E1; make(ROWS, b, 0);
        mchk("PAGES  two entries on one legacy slot: FWP_PID", pages(SC_MOD) == FWP_PID);
        memcpy(b, PID, sizeof b); b[3].legacy = P_COUNT; make(ROWS, b, 0);
        mchk("PAGES  a legacy slot at P_COUNT: FWP_PID", pages(SC_MOD) == FWP_PID);
        memcpy(b, PID, sizeof b); b[3].legacy = P_RESERVED34; make(ROWS, b, 0);
        mchk("PAGES  a legacy slot in P_RESERVED33..36 (the macros'): FWP_PID", pages(SC_MOD) == FWP_PID);
        memcpy(b, PID, sizeof b); b[3].legacy = P_COUNT - 1u; make(ROWS, b, 0);
        mchk("PAGES  the last slot below P_COUNT is fine", pages(SC_MOD) == FWP_OK);
    }
    {   param_desc_t pn[4]; fw_pages_t d2; fw_pid_t ok[4] = {{10, P_E1}, {11, FW_PID_NONE}, {12, FW_PID_NONE}, {13, FW_PID_NONE}};
        memcpy(pn, PANEL, sizeof pn); pn[1].label = 0;           /* entry 1 removed: its row stays, no slot */
        make(ROWS, ok, 0); d2 = D; d2.panel = pn;
        if (!fwp_wire(&d2, img, sizeof img, BASE, &desc)) exit(2);
        mchk("PAGES  a removed entry (label 0) with no slot passes", pages(SC_MOD) == FWP_OK);
        ok[1].legacy = P_E2;
        d2.pid = ok; if (!fwp_wire(&d2, img, sizeof img, BASE, &desc)) exit(2);
        mchk("PAGES  a removed entry that keeps a legacy slot: FWP_PID", pages(SC_MOD) == FWP_PID);
    }
    make(ROWS, PID, 0); put32(d_off() + 72, BASE + 0xFF0u);
    mchk("PAGES  a pid table that leaves the image: FWP_PID", pages(SC_MOD) == FWP_PID);
    make(ROWS, PID, 0); put32(d_off() + 72, pid_off() + BASE + 1u);
    mchk("PAGES  a pid table not word aligned: FWP_PID", pages(SC_MOD) == FWP_PID);
    /* the effect scope */
    make(ROWS_FX, PID, 0);
    mchk("PAGES  an effect's descriptor (SC_FX rows, a pid table, no hooks) passes as SC_FX", pages(SC_FX) == FWP_OK);
    mchk("PAGES  .. and is refused as an engine's (its rows are another scope)", pages(SC_MOD) == FWP_ROW);
    make(ROWS, PID, 0);
    mchk("PAGES  an engine's rows are refused as an effect's", pages(SC_FX) == FWP_ROW);
    make(ROWS_FX, 0, 0);
    mchk("PAGES  an effect with no pid table: FWP_PID", pages(SC_FX) == FWP_PID);
    make(ROWS_FX, PID, 0); put32(d_off() + 20, BASE + 0x104u);
    mchk("PAGES  an effect with a native hook: FWP_SCOPE", pages(SC_FX) == FWP_SCOPE);
    make(ROWS_FX, PID, 0); img[d_off() + 48] = 0;
    mchk("PAGES  an effect with a knob map: FWP_SCOPE", pages(SC_FX) == FWP_SCOPE);
    make(ROWS_FX, PID, 0); img[d_off() + 64] = 1;
    mchk("PAGES  an effect with a CC map: FWP_SCOPE", pages(SC_FX) == FWP_SCOPE);
    mchk("PAGES  a scope that is neither: FWP_SCOPE", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_TRACK, P_COUNT, 0) == FWP_SCOPE);
    mchk("PAGES  a P_COUNT above 256: FWP_SCOPE", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_FX, 300, 0) == FWP_SCOPE);
    {   uint32_t ui = 7;
        make(ROWS, PID, &UI);
        mchk("PAGES  the picture's address is handed back and a bad picture is not this check's", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_MOD, P_COUNT, &ui) == FWP_OK && ui == ui_off() + BASE);
    }
}

static void t_ui(void)
{
    fw_ui_info_t o;
    uint32_t u, k;
    make(ROWS, PID, &UI);
    u = ui_off(); k = kd_off();
    mchk("UI     a good picture and four knobs", uicheck(&o) == FWU_OK && o.nk == 4 && o.scene_len == sizeof SCENE && o.scene && o.kdef);
    make(ROWS, PID, 0);
    mchk("UI     no picture (0) is fine: the old HOME", fw_ui_check(img_rd, 0, 0, LO, HI, scene_ok, &o) == FWU_OK && !o.nk && !o.scene_len);
#define MUT(what, code, ...) do { make(ROWS, PID, &UI); u = ui_off(); k = kd_off(); __VA_ARGS__; mchk("UI     " what, uicheck(&o) == (code)); } while (0)
    MUT("the magic wrong: FWU_MAGIC", FWU_MAGIC, img[u] ^= 1);
    MUT("a size of 24: FWU_SIZE", FWU_SIZE, img[u + 4] = 24);
    MUT("version 2: FWU_SIZE", FWU_SIZE, img[u + 6] = 2);
    MUT("flags nonzero: FWU_RES", FWU_RES, img[u + 11] = 1);
    MUT("nk of 3: FWU_RES", FWU_RES, img[u + 10] = 3);
    MUT("a scene of 1025 bytes: FWU_RES", FWU_RES, img[u + 8] = 0x01, img[u + 9] = 0x04);
    MUT("a scene length with no address: FWU_SCENE", FWU_SCENE, put32(u + 12, 0));
    MUT("a scene that leaves the image: FWU_SCENE", FWU_SCENE, put32(u + 12, BASE + 0xFFEu));
    MUT("a scene scene_ok refuses (an unknown op instead of END): FWU_SCENE", FWU_SCENE, img[(img[u + 12] | img[u + 13] << 8 | img[u + 14] << 16) - (BASE & 0xFFFFFF) + 4] = 31);
    MUT("nk with no knob address: FWU_KDEF", FWU_KDEF, put32(u + 16, 0));
    MUT("knobs that leave the image: FWU_KDEF", FWU_KDEF, put32(u + 16, BASE + 0xFF8u));
    MUT("a lower case label: FWU_KDEF", FWU_KDEF, img[k] = 'c');
    MUT("a label of six letters: FWU_KDEF", FWU_KDEF, memcpy(img + k + 28, "ABCDEF", 6));
    MUT("a label with a letter after the NUL: FWU_KDEF", FWU_KDEF, img[k + 4] = 'X');
    MUT("a kind of 3: FWU_KDEF", FWU_KDEF, img[k + 6] = 3);
    MUT("a pad byte: FWU_KDEF", FWU_KDEF, img[k + 7] = 1);
    MUT("the reserved word: FWU_KDEF", FWU_KDEF, img[k + 26] = 1);
    MUT("a plain knob with the unused pid: FWU_KDEF", FWU_KDEF, img[k + 8] = 0xFF, img[k + 9] = 0xFF);
    MUT("a knob that turns with no name: FWU_KDEF", FWU_KDEF, memset(img + k, 0, 6));
    MUT("a target shape of 3: FWU_KDEF", FWU_KDEF, img[k + 28 + 12] = 3);
    MUT("a target depth of -128: FWU_KDEF", FWU_KDEF, img[k + 28 + 13] = 0x80);
    MUT("a target depth of -127 is fine", FWU_OK, img[k + 28 + 13] = 0x81);
    MUT("a target depth of 127 is fine", FWU_OK, img[k + 28 + 13] = 0x7F);
    {   fw_ui_t u2 = UI; u2.scene_len = 0; u2.scene = 0; u2.nk = 0; u2.kdef = 0;
        make(ROWS, PID, &u2);
        mchk("UI     a picture with neither scene nor knobs is valid", uicheck(&o) == FWU_OK && !o.nk && !o.scene_len);
        u2 = UI; u2.nk = 0; u2.kdef = 0; make(ROWS, PID, &u2);
        mchk("UI     a scene with no knobs is valid (the table says nothing; HOME keeps its EDIT knobs)", uicheck(&o) == FWU_OK && !o.nk && o.scene_len);
    }
    make(ROWS, PID, &UI);
    mchk("UI     no scene check function: a scene is refused (a build without pictures)", fw_ui_check(img_rd, 0, BASE + ui_off(), LO, HI, 0, &o) == FWU_SCENE);
}

/* ---- the registry and the interrupt ---- */
static int32_t mac_run(uint32_t e, int8_t m, uint32_t id, int16_t stored, int16_t *during)
{
    trk[0].engine = trk[0].eng_req = (uint8_t)e;
    trk[0].p[id] = stored;
    trk[0].p[P_MAC1] = m;
    mac_on();
    *during = trk[0].p[id];
    mac_off();
    return trk[0].p[id];
}
static void t_registry(void)
{
    const uint32_t E = 19u;                              /* an engine number with no table of its own */
    const knob_def_t *kd;
    int16_t during;
    uint32_t u, g0, i;
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song); host_tracks_init(); memset(mac, 0, sizeof mac);
    make(ROWS, PID, &UI);
    mchk("REGISTRY  before: the engine has no table", kdef_of(E) == 0 && !modui_of(E));
    g0 = modui_gen;
    mchk("REGISTRY  publish: the picture is in use (why 0)", modui_publish(E, img_rd, 0, desc, LO, HI, img, BASE) == 0 && modui_gen != g0);
    kd = kdef_of(E);
    mchk("REGISTRY  kdef_of answers the module's table, labels copied", kd && !strcmp(kd[0].label, "CUT") && !strcmp(kd[1].label, "TONE") && !strcmp(kd[2].label, "PLUCK"));
    mchk("REGISTRY  a plain knob resolved to its pid's legacy slot (pid 10 -> P_E1)", kd[0].kind == KD_PLAIN && kd[0].id == P_E1);
    mchk("REGISTRY  a macro: pid 11 -> P_E2 (UP 90), pid 13 -> P_E3 (DOWN -50), pid 99 (not in the table) and pid 12 (no slot) dropped",
          kd[1].kind == KD_MACRO && kd[1].t[0].id == P_E2 && kd[1].t[0].shape == KS_UP && kd[1].t[0].depth == 90 && kd[1].t[1].id == P_E3 &&
          kd[1].t[1].shape == KS_DOWN && kd[1].t[1].depth == -50 && kd[1].t[2].id == KD_NOT && kd[1].t[3].id == KD_NOT);
    mchk("REGISTRY  the range of a target is copied from its panel entry (RES 0..100, DRY -20..20)",
          modui_rng(kd, 4)[0] == 0 && modui_rng(kd, 4)[1] == 100 && modui_rng(kd, 5)[0] == -20 && modui_rng(kd, 5)[1] == 20);
    mchk("REGISTRY  a knob of kind none stays none", kd[3].kind == KD_NONE);
    mchk("REGISTRY  the scene is the module's, found by scene_of before the pack's",
          ({ uint32_t l; const uint8_t *sc = scene_of(E, &l); sc && l == sizeof SCENE && !memcmp(sc, SCENE, l); }));
    /* the interrupt */
    for (i = 0; i < 4u; i++) trk[0].p[P_MAC0 + i] = 0;
    mchk("MACRO  a macro at +40 on RES (stored 50) puts 50 + 90 x 40 / 64 = 106 clamped to the copied range 100, and takes it off exactly",
          mac_run(E, 40, P_E2, 50, &during) == 50 && during == 100);
    mchk("MACRO  at -64 on DRY (DOWN -50, stored 0): 0 + (-50 x -64 / 64) = 50 clamped to +20", mac_run(E, -64, P_E3, 0, &during) == 0 && during == 20);
    mchk("MACRO  the UP target does not move at -64", mac_run(E, -64, P_E2, 50, &during) == 50 && during == 50);
    /* retire */
    g0 = modui_gen;
    modui_retire(E);
    mchk("RETIRE  modui_retire clears the record, the table and the scene, and moves the generation", !kdef_of(E) && !modui_of(E) && modui_gen != g0 && ({ uint32_t l; !modui_scene(E, &l) && !l; }));
    trk[0].p[P_E2] = 50; trk[0].p[P_MAC1] = 40; trk[0].engine = trk[0].eng_req = (uint8_t)E;
    mac_on();
    mchk("RETIRE  after it the interrupt puts nothing on", trk[0].p[P_E2] == 50 && !mac[0].on);
    mac_off();
    /* a refused picture */
    {   uint8_t keep[8];
        make(ROWS, PID, &UI); u = ui_off(); memcpy(keep, img + u, 8); img[u] ^= 1;
        mchk("REGISTRY  a refused picture: publish says FWU_MAGIC, the engine keeps the old HOME", modui_publish(E, img_rd, 0, desc, LO, HI, img, BASE) == FWU_MAGIC && !kdef_of(E));
        mchk("REGISTRY  .. and the record says why", modui_of(E) && modui_of(E)->why == FWU_MAGIC && !modui_of(E)->nk);
        modui_retire(E);
        memcpy(img + u, keep, 8);
    }
    /* room */
    make(ROWS, PID, &UI);
    {   uint32_t n = 0;
        for (i = 0; i < OC_MODUI_N; i++) n += modui_publish(30u + i, img_rd, 0, desc, LO, HI, img, BASE) == 0;
        mchk("REGISTRY  OC_MODUI_N modules fit", n == OC_MODUI_N);
        mchk("REGISTRY  one more loads without picture or knobs (FWU_FULL), the others stay", modui_publish(40u, img_rd, 0, desc, LO, HI, img, BASE) == FWU_FULL && !kdef_of(40u) && kdef_of(30u));
        for (i = 0; i < OC_MODUI_N; i++) modui_retire(30u + i);
        mchk("REGISTRY  all retired: room again", modui_publish(41u, img_rd, 0, desc, LO, HI, img, BASE) == 0);
        modui_retire(41u);
    }
    /* a descriptor with no picture publishes nothing */
    make(ROWS, PID, 0);
    g0 = modui_gen;
    mchk("REGISTRY  a descriptor with no picture publishes nothing and moves nothing", modui_publish(E, img_rd, 0, desc, LO, HI, img, BASE) == 0 && modui_gen == g0 && !modui_of(E));
}
/* the loader's order, read in the text: a record is retired before the engine is unregistered, published after it is registered */
static void t_text(void)
{
    FILE *f = fopen("firmware/src/oc_module.c", "r");
    static char buf[200000];
    size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
    const char *un, *rt, *pub, *reg;
    if (f) fclose(f);
    buf[n] = 0;
    un = strstr(buf, "static void oc_mod_unreg(uint32_t e)");
    rt = un ? strstr(un, "modui_retire(e);") : 0;
    reg = un ? strstr(un, "eng_unregister(e);") : 0;
    pub = strstr(buf, "oc_mod_ui_publish(i, oc_mod_hdr_ptr(i));");
    mchk("RETIRE  oc_mod_unreg retires the record before it unregisters the engine", rt && reg && rt < reg);
    mchk("RETIRE  the boot publishes after the engine is registered (touch 4 follows oc_mod_reg)", pub && strstr(buf, "oc_mod_reg(i, oc_mod_hdr_ptr(i)") && strstr(buf, "oc_mod_reg(i, oc_mod_hdr_ptr(i)") < pub);
}
/* FUZZ: a mutated image of a descriptor with a pid table, a picture, a scene and four knobs: whatever the checks accept, publish turns into a record the UI and the interrupt may
 * trust (every id below P_COUNT or unused, every range ordered, the scene inside the image, labels printable, the table fits the registry); nothing reads outside the image (ASan) */
static uint32_t fz_state = 20260615u;
static uint32_t fz_rand(void) { fz_state = fz_state * 1664525u + 1013904223u; return fz_state >> 8; }
static void t_fuzz(void)
{
    static uint8_t base[sizeof img];
    uint32_t round, accepted = 0, published = 0, bad_inv = 0;
    make(ROWS, PID, &UI);
    memcpy(base, img, sizeof img);
    for (round = 0; round < 60000u; round++) {
        uint32_t nm = 1u + fz_rand() % 6u, k, code, ui;
        fw_ui_info_t o;
        memcpy(img, base, sizeof img);
        for (k = 0; k < nm; k++) {
            uint32_t at = (d_off() + fz_rand() % 0x280u) % sizeof img;   /* around the descriptor, the pid table, the picture, the scene and the knobs */
            if (fz_rand() & 1u) img[at] ^= (uint8_t)(1u << (fz_rand() % 8u)); else img[at] = (uint8_t)fz_rand();
        }
        if (fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_MOD, P_COUNT, &ui) != FWP_OK) continue;
        accepted++;
        code = (uint32_t)fw_ui_check(img_rd, 0, ui, LO, HI, scene_ok, &o);
        if (modui_publish(61u, img_rd, 0, desc, LO, HI, img, BASE) != code && code != FWU_OK) bad_inv++;
        {   const modui_t *m = modui_of(61u);
            if (m && m->nk) {
                uint32_t i, j;
                published++;
                for (i = 0; i < 4u; i++) {
                    if (m->kd[i].kind == KD_PLAIN && m->kd[i].id >= P_COUNT) bad_inv++;
                    if (m->kd[i].kind > KD_MACRO) bad_inv++;
                    for (j = 0; j < 5u; j++) if (m->lbl[i][j] && !((m->lbl[i][j] >= 'A' && m->lbl[i][j] <= 'Z') || (m->lbl[i][j] >= '0' && m->lbl[i][j] <= '9'))) bad_inv++;
                    if (m->lbl[i][5]) bad_inv++;
                    for (j = 0; j < KD_TGT; j++) {
                        const kd_tgt_t *g = &m->kd[i].t[j];
                        if (g->id != KD_NOT && (g->id >= P_COUNT || (g->id >= P_MAC0 && g->id <= P_MAC3) || g->shape > KS_DOWN || m->rng[i * KD_TGT + j][0] > m->rng[i * KD_TGT + j][1])) bad_inv++;
                    }
                }
                if (m->scene_len && (m->scene < img || m->scene + m->scene_len > img + sizeof img)) bad_inv++;
            }
        }
        modui_retire(61u);
    }
    mchk("FUZZ    60000 mutated images: whatever is accepted and published holds the registry's invariants", bad_inv == 0 && accepted > 1000u && published > 100u);
    printf("mod_ui: fuzz: %u accepted, %u published, %u invariant breaks\n", accepted, published, bad_inv);
}
int main(void)
{
    t_pages();
    t_ui();
    t_registry();
    t_text();
    t_fuzz();
    printf("mod_ui: %d checks, %d failed\n", nchk, nbad);
    return nbad != 0;
}
