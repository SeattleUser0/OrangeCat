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
#include "fw_fxstore.h"

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
static int pages(uint32_t scope) { return fw_pages_check_ex(img_rd, 0, desc, LO, HI, scope, P_COUNT, G_COUNT, 0); }
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
    mchk("PAGES  a scope that is neither: FWP_SCOPE", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_TRACK, P_COUNT, G_COUNT, 0) == FWP_SCOPE);
    mchk("PAGES  a P_COUNT above 256: FWP_SCOPE", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_FX, 300, G_COUNT, 0) == FWP_SCOPE);
    {   uint32_t ui = 7;
        make(ROWS, PID, &UI);
        mchk("PAGES  the picture's address is handed back and a bad picture is not this check's", fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_MOD, P_COUNT, G_COUNT, &ui) == FWP_OK && ui == ui_off() + BASE);
    }
}

/* ---- the effect side's hooks, the format limit and DUB TAPE's pages as MODULE_FORMAT_R3.md section 15 writes them ---- */
#include "fx_dub_data.h"
static void t_fx(void)
{
    fw_pages_t d;
    int16_t v = 0;
    const param_desc_t *pd;
    _Static_assert(sizeof(((fw_pages_t *)0)->shown) == sizeof(((fw_pages_t *)0)->fx_shown), "the union's members are one pointer");
    make_dub(&d);
    if (!fwp_wire(&d, img, sizeof img, BASE, &desc)) { printf("mod_ui: wire image full\n"); exit(2); }
    mchk("FX      DUB TAPE's pages as MODULE_FORMAT_R3.md section 15 writes them (3 pages, 14 panel entries, 4 views, shown and put, F_DUBTS / F_DUBSPD) pass the checker as SC_FX", pages(SC_FX) == FWP_OK);
    mchk("FX      .. and are refused as an engine's (their rows are SC_FX)", pages(SC_MOD) == FWP_ROW);
    mchk("FX      the example's legacy slots are today's DUB ids: 104 105 106 107 111 112 113 45 46 47", DUB_PID[0].legacy == 104 && DUB_PID[1].legacy == 105 && DUB_PID[2].legacy == 106 && DUB_PID[3].legacy == 107 &&
         DUB_PID[4].legacy == 111 && DUB_PID[5].legacy == 112 && DUB_PID[6].legacy == 113 && DUB_PID[7].legacy == 45 && DUB_PID[8].legacy == 46 && DUB_PID[9].legacy == 47);
    mchk("FX      the hooks sit at the offsets an engine's do (24, 28) in the wire bytes", fwp_u32(img + d_off() + 24) != 0 && fwp_u32(img + d_off() + 28) != 0);
    {   uint8_t keep[4]; memcpy(keep, img + d_off() + 24, 4);
        put32(d_off() + 24, 0);
        mchk("FX      views without a shown hook: FWP_PID", pages(SC_FX) == FWP_PID);
        memcpy(img + d_off() + 24, keep, 4); memcpy(&desc, &desc, 0);
        put32(d_off() + 28, 0);
        mchk("FX      views without a put hook: FWP_PID", pages(SC_FX) == FWP_PID);
        put32(d_off() + 28, BASE + 0x104u);
        mchk("FX      the hooks restored pass again", pages(SC_FX) == FWP_OK);
        put32(d_off() + 24, BASE + 0xFFFFFFF0u);
        mchk("FX      a shown hook outside the image: FWP_HOOK", pages(SC_FX) == FWP_HOOK);
        put32(d_off() + 24, BASE + 0x104u);
    }
    {   fw_pid_t bad[14]; fw_pages_t d2;
        memcpy(bad, DUB_PID, sizeof bad); bad[10].legacy = P_E1;
        d2 = d; d2.pid = bad; fwp_wire(&d2, img, sizeof img, BASE, &desc);
        mchk("FX      a view that has a legacy slot: FWP_PID", pages(SC_FX) == FWP_PID);
        make_dub(&d2); d2.pid = DUB_PID; fwp_wire(&d2, img, sizeof img, BASE, &desc);
    }
    {   fw_pid_t nov[14]; fw_pages_t d2;
        memcpy(nov, DUB_PID, sizeof nov); for (uint32_t i = 10; i < 14; i++) nov[i].id = (uint16_t)(20 + i);
        d2 = d; d2.pid = nov; d2.fx_shown = 0; d2.fx_put = 0;
        fwp_wire(&d2, img, sizeof img, BASE, &desc);
        mchk("FX      the same pages with the views made stored ids (option B) and no hooks pass: hooks are for views and the TIME format only", pages(SC_FX) == FWP_OK);
    }
    {   fw_pages_t d2; static param_desc_t pn[14];
        make_dub(&d2); memcpy(pn, DUB_PANEL, sizeof pn); d2.panel = pn;
        pn[7].fmt = F_DUBSPD; fwp_wire(&d2, img, sizeof img, BASE, &desc);
        mchk("FORMAT  F_DUBSPD, the new limit, is accepted", pages(SC_FX) == FWP_OK);
        pn[7].fmt = F_TONE + 1; fwp_wire(&d2, img, sizeof img, BASE, &desc);
        mchk("FORMAT  F_TONE + 1 (F_PANLR) is accepted: the limit was F_TONE", pages(SC_FX) == FWP_OK);
        { uint8_t *e = img + (fwp_u32(img + d_off() + 16) - BASE) + 7 * FW_PAGES_DESC_WIRE; e[4] = F_DUBSPD + 1;
          mchk("FORMAT  F_DUBSPD + 1 is refused: FWP_PANEL", pages(SC_FX) == FWP_PANEL); }
    }
    /* the hooks themselves: the cells over MODE and TAPE, the TIME format by SYNC, a write back */
    memset(dubp, 0, sizeof dubp);
    dubp[P_DUB_MODE] = 65; dubp[P_DUB_TAPE] = 8; dubp[P_DUB_TIML] = 53; dubp[P_DUB_TIMR] = 67;   /* Release 2 defaults: SYNC on, PING off, DRIVE 6, WOW 0 */
    pd = dub_shown(0, 0, 0x8001, &v);
    mchk("HOOK    SYNC reads 1 from MODE 65, PING 0, DRIVE 6, WOW 0", pd && v == 1 && (dub_shown(0, 0, 0x8002, &v), v == 0) && (dub_shown(0, 0, 0x8003, &v), v == 6) && (dub_shown(0, 0, 0x8004, &v), v == 0));
    pd = dub_shown(0, 0, 5, &v);
    mchk("HOOK    TIM L shows on the beat (F_DUBTS) with SYNC on, value 53", pd && pd->fmt == F_DUBTS && v == 53);
    dub_put(0, 0, 0x8001, 0);
    pd = dub_shown(0, 0, 6, &v);
    mchk("HOOK    SYNC put to 0 writes MODE 64 and TIM R shows free (F_DUBTF), value 67", dubp[P_DUB_MODE] == 64 && pd && pd->fmt == F_DUBTF && v == 67);
    dub_put(0, 0, 0x8003, 9); dub_put(0, 0, 0x8004, 5); dub_put(0, 0, 0x8002, 1);
    mchk("HOOK    DRIVE 9, WOW 5 write TAPE 9 x 12 + 5 - 64 = 49 in one store each; PING 1 writes MODE bit 1 and keeps SYNC off", dubp[P_DUB_TAPE] == 49 && dubp[P_DUB_MODE] == 66);
    mchk("HOOK    a stored id has no view: the hook answers 0 (the panel's own descriptor)", dub_shown(0, 0, 1, &v) == 0);
}

/* ---- Release 3 step 8: the reverb modes (SPRING, SHIMMER, RESO, CLOUD) and the FX layer's buffer effects (GLITCH, TAPE STOP, SHUFFLE, FREEZE) as MODULE_FORMAT_R3.md section 16 writes them,
 * with the four decisions on L1 to L4: a song.g legacy slot (bit 15) and the import condition in `ext`; 0xFF in a row is the host's knob; a volatile pid (bit 14) and a relative pid (bit 13).
 * Every example is data here, built into the device's wire bytes and checked by fw_pages_check_ex as SC_FX; each panel entry is compared with what the firmware shows today (params.c global_desc
 * for the reverb with song.g[G_RTYPE] set to the mode; ui_layer.c FXK_* and jfx_perform.c perf_gp for the layer); the size of each descriptor's data is what fwp_wire wrote. */
#include "fx_reverb_data.h"
static uint32_t x_wire(const fw_pages_t *d)
{
    uint32_t len = fwp_wire(d, img, sizeof img, BASE, &desc);
    xbytes = len ? len - 0x100u : 0;
    return len;
}
static void t_step8_reverb(void)
{
    uint32_t sizes[4] = {0};
    for (uint32_t k = 0; k < 4; k++) {
        fw_pages_t d;
        char buf[200];
        make_reverb(&d, k);
        x_wire(&d);
        snprintf(buf, sizeof buf, "STEP8   %s (%u bytes of descriptor data) passes the checker as SC_FX: song.g legacy slots, the TYPE condition, the host's K1", REV_NM[k], xbytes);
        mchk(buf, xbytes && pages(SC_FX) == FWP_OK);
        sizes[k] = xbytes;
        {   int same = 1;                                      /* each knob is what the firmware shows today with TYPE = this mode */
            memset(&song, 0, sizeof song);
            song.g[G_RTYPE] = (int16_t)REV_TYPE[k];
            for (uint32_t i = 0; i < 7; i++) same &= desc_same(&xpanel[i], global_desc(REV_G[i]));
            snprintf(buf, sizeof buf, "STEP8   %s: its seven knobs are what the REVERB and REVERB 2 pages show for TYPE %s today (label, format, range, default, unit, names)", REV_NM[k], REV_NM[k]);
            mchk(buf, same);
        }
    }
    printf("mod_ui: step 8 reverb descriptor data: SPRING %u B, SHIMMER %u B, RESO %u B, CLOUD %u B\n", sizes[0], sizes[1], sizes[2], sizes[3]);
    {   fw_pages_t d;                                           /* L1: the checker's rules for a song.g slot and the condition */
        make_reverb(&d, 0);
        xpid[0].legacy = (uint16_t)(FW_LEG_GLOBAL | G_RSIZE);  x_wire(&d);
        mchk("L1      a song.g legacy slot (bit 15) passes in an effect", pages(SC_FX) == FWP_OK);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_RSIZE);  x_wire(&d);
        mchk("L1      .. two entries on one song.g slot: FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_RDAMP);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_BPM);  x_wire(&d);
        mchk("L1      .. a song.g slot that is not an effect's global (G_BPM): FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_BUDGET);  x_wire(&d);
        mchk("L1      .. G_BUDGET (a device setting, not song.g's): FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_COUNT);  x_wire(&d);
        mchk("L1      .. a slot at G_COUNT: FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[1].legacy = (uint16_t)(FW_LEG_GLOBAL | G_RDAMP);
        xpid[3].legacy = P_E1;  x_wire(&d);
        mchk("L1      .. a track slot and song.g slots in one table pass (two spaces)", pages(SC_FX) == FWP_OK);
        make_reverb(&d, 0);  x_wire(&d);
        mchk("L1      the track slot G_RSIZE would have been (a bare number) is still a track slot, and a descriptor of an engine may not use bit 15", pages(SC_FX) == FWP_OK);
        d.ext = 0;  x_wire(&d);
        mchk("L1      no condition (ext 0) passes", pages(SC_FX) == FWP_OK);
        d.ext = (G_RTYPE + 1u) | (1u << 16);  x_wire(&d);
        mchk("L1      ext: TYPE == 1 passes in an effect", pages(SC_FX) == FWP_OK && pages(SC_MOD) != FWP_OK);
        mchk("L1      .. and an engine's descriptor may not carry a condition (FWP_EXT)", ({ make_dub(&d); d.ext = 1; x_wire(&d); pages(SC_FX) == FWP_EXT; }));
        make_reverb(&d, 0);  d.ext = 0x00010000u;  x_wire(&d);
        mchk("L1      ext with no index (value only): FWP_EXT", pages(SC_FX) == FWP_EXT);
        make_reverb(&d, 0);  d.ext = (G_BPM + 1u) | (1u << 16);  x_wire(&d);
        mchk("L1      ext naming a global that is not an effect's (G_BPM): FWP_EXT", pages(SC_FX) == FWP_EXT);
        make_reverb(&d, 0);  d.ext = (G_COUNT + 1u) | (1u << 16);  x_wire(&d);
        mchk("L1      ext naming a global past G_COUNT: FWP_EXT", pages(SC_FX) == FWP_EXT);
    }
    {   fw_pages_t d;                                           /* L2: 0xFF in a row is the host's knob */
        page_t only_host[1] = {{"REVERB", FAM_FX, SC_FX, GR_NONE, {0xFF, 0xFF, 0xFF, 0xFF}}};
        make_reverb(&d, 0);  x_wire(&d);
        mchk("L2      a page whose K1 is the host's (0xFF) and K2 .. K4 the module's passes: the reverb examples above", pages(SC_FX) == FWP_OK && xrows[0].id[0] == 0xFF);
        x_fill(&d, rev_m[0], 7, only_host, 1);
        for (uint32_t i = 0; i < 7; i++) { xpid[i].id = (uint16_t)(1 + i); xpid[i].legacy = FW_PID_NONE; }
        x_wire(&d);
        mchk("L2      a page of an effect with nothing of its own (all four 0xFF): FWP_ROW", pages(SC_FX) == FWP_ROW);
        x_fill(&d, rev_m[0], 7, REV_ROWS, 2);
        for (uint32_t i = 0; i < 7; i++) { xpid[i].id = (uint16_t)(1 + i); xpid[i].legacy = FW_PID_NONE; }
        xrows[0].id[1] = 0xFF; xrows[0].id[2] = 0xFF; xrows[0].id[3] = 0xFF; xrows[0].id[0] = 0;   /* one knob of its own, three of the host's */
        x_wire(&d);
        mchk("L2      .. with one of its own it passes (the host may own the rest)", pages(SC_FX) == FWP_OK);
        make_reverb(&d, 0);  x_wire(&d);
    }
}
/* the layer's knobs: RAM only (L3: a volatile pid, 0x4001 up, no hooks); TAPE STOP's SCRATCH a relative knob (L4: 0x2001, put receives the detents) */
static int32_t scratch_turns;
static void lay_put(uint32_t slot, uint32_t instance, uint32_t id, int32_t v) { (void)slot; (void)instance; if (id == 0x2001u) scratch_turns += v; }
static void t_step8_layer(void)
{
    static const char *const NM[4] = {"GLITCH", "TAPE STOP", "SHUFFLE", "FREEZE"};
    static const exp_t E[4][4] = {
        {{"DENS", F_INT, 0, 100, 50, "%", 0}, {"PITCH", F_INT, -12, 12, 0, "st", 0}, {"SIZE", F_ENUM, 0, 3, 0, 0, X_GSIZE}, {"VARI", F_INT, 1, 16, 1, 0, 0}},
        {{"SCRATCH", F_INT, 0, 0, 0, 0, 0}, {"TIME", F_ENUM, 0, 4, 4, 0, X_TTIME}, {"CURVE", F_INT, -100, 100, 0, 0, 0}, {0}},
        {{"AMOUNT", F_INT, 0, 100, 60, "%", 0}, {"SIZE", F_ENUM, 0, 2, 2, 0, X_SSIZE}, {"SPEED", F_INT, 0, 100, 20, "%", 0}, {"VARI", F_INT, 1, 16, 1, 0, 0}},
        {{"DENS", F_INT, 0, 100, 50, "%", 0}, {"PITCH", F_INT, -12, 12, 0, "st", 0}, {"SPRAY", F_INT, 0, 100, 30, "%", 0}, {"SIZE", F_ENUM, 0, 4, 2, 0, X_FSIZE}}};
    static const uint8_t NK[4] = {4, 3, 4, 4};
    uint32_t sizes[4];
    for (uint32_t x = 0; x < 4; x++) {
        fw_pages_t d;
        page_t row = {0, FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}};
        char buf[200];
        int same = 1;
        row.title = NM[x];
        for (uint32_t k = NK[x]; k < 4; k++) row.id[k] = 0xFF;      /* (L2) the host's place: the layer draws nothing there */
        x_fill(&d, E[x], NK[x], &row, 1);
        for (uint32_t k = 0; k < NK[x]; k++) { xpid[k].id = (uint16_t)(FW_PID_VOLATILE + 1u + k); xpid[k].legacy = FW_PID_NONE; }
        if (x == 1) { xpid[0].id = (uint16_t)(FW_PID_RELATIVE + 1u); d.fx_put = lay_put; }   /* (L4) SCRATCH: the detents; the only hook */
        x_wire(&d);
        sizes[x] = xbytes;
        snprintf(buf, sizeof buf, "STEP8   FX layer %s (%u bytes of descriptor data, %s) passes the checker as SC_FX", NM[x], xbytes, x == 1 ? "volatile pids, SCRATCH relative, one put hook" : "volatile pids, no hooks");
        mchk(buf, xbytes && pages(SC_FX) == FWP_OK);
        for (uint32_t k = 0; k < NK[x]; k++)
            same &= !strcmp(xpanel[k].label, FXK_LABEL[x][k]) && xpanel[k].min == FXK_MIN[x][k] && xpanel[k].max == FXK_MAX[x][k];
        for (uint32_t k = NK[x]; k < 4; k++) same &= !FXK_LABEL[x][k][0];
        snprintf(buf, sizeof buf, "STEP8   FX layer %s: labels and ranges are ui_layer.c's FXK_LABEL, FXK_MIN, FXK_MAX", NM[x]);
        mchk(buf, same);
        {   static const int8_t DEF[4][4] = {{50, 0, 0, 1}, {0, 4, 0, 0}, {60, 2, 20, 1}, {50, 0, 30, 2}};   /* jfx_perform.c perf_gp's initial values */
            int dd = 1;
            for (uint32_t k = 0; k < NK[x]; k++) dd &= xpanel[k].def == DEF[x][k];
            snprintf(buf, sizeof buf, "STEP8   FX layer %s: defaults are perf_gp's initial values (the host keeps the bytes)", NM[x]);
            mchk(buf, dd);
        }
        {   int16_t vals[8] = {1, 2, 3, 4, 5, 6, 7, 8};
            uint8_t out[64];
            snprintf(buf, sizeof buf, "L3      FX layer %s: the project stores none of its values (a volatile or relative pid is skipped by the writer)", NM[x]);
            mchk(buf, fx_store_pack(img_rd, 0, desc, vals, out, sizeof out) == 0 && ({ static int16_t back[8]; memset(back, 0, sizeof back); fx_store_unpack(img_rd, 0, desc, out, 0, back) == 0; }));
        }
    }
    printf("mod_ui: step 8 FX layer descriptor data: GLITCH %u B, TAPE STOP %u B, SHUFFLE %u B, FREEZE %u B\n", sizes[0], sizes[1], sizes[2], sizes[3]);
    {   fw_pages_t d;                                           /* L3 and L4: the checker's rules */
        page_t row = {"GLITCH", FAM_FX, SC_FX, GR_NONE, {0, 1, 2, 3}};
        x_fill(&d, E[0], 4, &row, 1);
        for (uint32_t k = 0; k < 4; k++) { xpid[k].id = (uint16_t)(FW_PID_VOLATILE + 1u + k); xpid[k].legacy = FW_PID_NONE; }
        x_wire(&d);
        mchk("L3      volatile pids need no hooks: the layer's four knobs pass with none", pages(SC_FX) == FWP_OK && !fwp_u32(img + d_off() + 24) && !fwp_u32(img + d_off() + 28));
        xpid[0].legacy = P_E1;  x_wire(&d);
        mchk("L3      a volatile pid with a legacy slot: FWP_PID (it is not stored, so it has no slot)", pages(SC_FX) == FWP_PID);
        xpid[0].legacy = FW_PID_NONE;  xpid[0].id = (uint16_t)(FW_PID_VOLATILE | FW_PID_VIEW | 1u);  x_wire(&d);
        mchk("L3      a pid with two of the flag bits (volatile and view): FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[0].id = (uint16_t)(FW_PID_VOLATILE | FW_PID_RELATIVE | 1u);  x_wire(&d);
        mchk("L3      .. volatile and relative: FWP_PID", pages(SC_FX) == FWP_PID);
        xpid[0].id = (uint16_t)(FW_PID_RELATIVE + 1u);  x_wire(&d);
        mchk("L4      a relative pid with no put hook: FWP_PID (no hook, no knob)", pages(SC_FX) == FWP_PID);
        d.fx_put = lay_put;  x_wire(&d);
        mchk("L4      .. with the put hook it passes", pages(SC_FX) == FWP_OK);
        for (uint32_t k = 0; k < 4; k++) xpid[k].id = (uint16_t)(1 + k);
        d.fx_put = 0;  x_wire(&d);
        mchk("L3      the same four entries as plain pids (1 .. 4) pass too: they are stored, which is why the layer's are volatile", pages(SC_FX) == FWP_OK);
        scratch_turns = 0;
        lay_put(0, 0, 0x2001u, 3); lay_put(0, 0, 0x2001u, -1);
        mchk("L4      the relative knob's put receives detents: +3 then -1 sums to 2", scratch_turns == 2);
    }
    {   fw_pages_t d;                                           /* L3: the writer and reader of the stored values skip view, volatile, relative and removed entries; the others round-trip */
        int16_t vals[7] = {10, 20, 30, 40, 50, 60, 70}, back[7];
        uint8_t out[64];
        uint32_t n;
        make_reverb(&d, 0);  x_wire(&d);
        n = fx_store_pack(img_rd, 0, desc, vals, out, sizeof out);
        for (uint32_t i = 0; i < 7; i++) back[i] = (int16_t)(rev_m[0][i].def);
        mchk("L3      a plain descriptor stores all seven values (28 bytes) and the reader returns them", n == 28 && fx_store_unpack(img_rd, 0, desc, out, n, back) == 7 && back[0] == 10 && back[6] == 70);
        xpid[2].id = (uint16_t)(FW_PID_VOLATILE + 3u);  xpid[5].id = (uint16_t)(FW_PID_VOLATILE + 6u);  x_wire(&d);
        n = fx_store_pack(img_rd, 0, desc, vals, out, sizeof out);
        for (uint32_t i = 0; i < 7; i++) back[i] = (int16_t)(rev_m[0][i].def);
        mchk("L3      with two entries volatile, 20 bytes are stored and the reader leaves those two at their defaults", n == 20 && fx_store_unpack(img_rd, 0, desc, out, n, back) == 5 && back[2] == rev_m[0][2].def && back[5] == rev_m[0][5].def && back[1] == 20);
        xpid[2].legacy = FW_PID_NONE;
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
    {   const param_desc_t *d2 = param_desc_of(E, P_E2), *d3 = param_desc_of(E, P_E3);
        int32_t lo3 = -20 > d3->min ? -20 : d3->min, hi3 = 20 < d3->max ? 20 : d3->max, hi2 = 100 < d2->max ? 100 : d2->max;
        mchk("REGISTRY  the range of a target is the panel entry's cut to the slot's own descriptor (RES 0..100 on E2, DRY -20..20 on E3)",
             modui_rng(kd, 4)[0] == (d2->min > 0 ? d2->min : 0) && modui_rng(kd, 4)[1] == hi2 && modui_rng(kd, 5)[0] == lo3 && modui_rng(kd, 5)[1] == hi3);
    }
    mchk("REGISTRY  a knob of kind none stays none", kd[3].kind == KD_NONE);
    mchk("REGISTRY  the scene is the module's, found by scene_of before the pack's",
          ({ uint32_t l; const uint8_t *sc = scene_of(E, &l); sc && l == sizeof SCENE && !memcmp(sc, SCENE, l); }));
    /* the interrupt */
    for (i = 0; i < 4u; i++) trk[0].p[P_MAC0 + i] = 0;
    mchk("MACRO  a macro at +40 on E2 (stored 10) puts 10 + 90 x 40 / 64 = 66 clamped to the copied range (the panel's 0..100 cut to the slot's -24..24) = 24, and takes it off exactly",
          mac_run(E, 40, P_E2, 10, &during) == 10 && during == modui_rng(kd, 4)[1]);
    mchk("MACRO  at -64 on E3 (DOWN -50, stored 0): 0 + (-50 x -64 / 64) = 50 clamped to the copied top (the panel's +20)", mac_run(E, -64, P_E3, 0, &during) == 0 && during == modui_rng(kd, 5)[1] && during == 20);
    mchk("MACRO  the UP target does not move at -64", mac_run(E, -64, P_E2, 10, &during) == 10 && during == 10);
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
        for (i = 0; i < OC_MODUI_N; i++) n += modui_publish(2u + i, img_rd, 0, desc, LO, HI, img, BASE) == 0;
        mchk("REGISTRY  OC_MODUI_N modules fit", n == OC_MODUI_N);
        mchk("REGISTRY  one more loads without picture or knobs (FWU_FULL), the others stay", modui_publish(7u, img_rd, 0, desc, LO, HI, img, BASE) == FWU_FULL && !kdef_of(7u) && kdef_of(2u));
        for (i = 0; i < OC_MODUI_N; i++) modui_retire(2u + i);
        mchk("REGISTRY  all retired: room again", modui_publish(8u, img_rd, 0, desc, LO, HI, img, BASE) == 0);
        modui_retire(8u);
    }
    /* (review) a legacy slot outside E0..E7 is no knob target, and a module cannot widen a slot's range */
    {   static const fw_pid_t PID2[4] = {{10, P_VOICE}, {11, P_E1}, {12, P_MUTE}, {13, P_E4}};
        static param_desc_t wide[4];
        static const fw_kdef_t K2[4] = {
            {"VOICE", 1, 0, 10, {{FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
            {"WIDE", 2, 0, 0, {{11, KS_LIN, 127}, {12, KS_LIN, 127}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
            {"E4", 2, 0, 0, {{13, KS_LIN, 100}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
            {"X", 0, 0, 0, {{FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}, {FW_PID_NONE, 0, 0}}, 0},
        };
        static fw_ui_t U2; fw_pages_t d3;
        memcpy(wide, PANEL, sizeof wide);
        for (i = 0; i < 4u; i++) if (wide[i].fmt == F_INT) { wide[i].min = -32768; wide[i].max = 32767; }
        U2 = UI; U2.kdef = K2;
        make(ROWS, PID2, &U2); d3 = D; d3.panel = wide;
        if (!fwp_wire(&d3, img, sizeof img, BASE, &desc)) exit(2);
        mchk("REGISTRY  (review) the wide panel, with legacy slots P_VOICE, P_E1, P_MUTE, P_E4, publishes", modui_publish(E, img_rd, 0, desc, LO, HI, img, BASE) == 0);
        kd = kdef_of(E);
        mchk("REGISTRY  a plain knob on P_VOICE is dropped (a knob never switches what the sound is)", kd && kd[0].kind == KD_NONE);
        mchk("REGISTRY  a macro keeps its P_E1 target and drops the P_MUTE one", kd[1].kind == KD_MACRO && kd[1].t[0].id == P_E1 && kd[1].t[1].id == KD_NOT);
        mchk("REGISTRY  a module cannot widen a slot's range: P_E1's copied range is its own descriptor's",
             modui_rng(kd, 4)[0] == param_desc_of(E, P_E1)->min && modui_rng(kd, 4)[1] == param_desc_of(E, P_E1)->max);
        modui_retire(E);
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
        if (fw_pages_check_ex(img_rd, 0, desc, LO, HI, SC_MOD, P_COUNT, G_COUNT, &ui) != FWP_OK) continue;
        accepted++;
        code = (uint32_t)fw_ui_check(img_rd, 0, ui, LO, HI, scene_ok, &o);
        if (modui_publish(6u, img_rd, 0, desc, LO, HI, img, BASE) != code && code != FWU_OK) bad_inv++;
        {   const modui_t *m = modui_of(6u);
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
        modui_retire(6u);
    }
    mchk("FUZZ    60000 mutated images: whatever is accepted and published holds the registry's invariants", bad_inv == 0 && accepted > 1000u && published > 100u);
    printf("mod_ui: fuzz: %u accepted, %u published, %u invariant breaks\n", accepted, published, bad_inv);
}
int main(void)
{
    t_pages();
    t_fx();
    t_step8_reverb();
    t_step8_layer();
    t_ui();
    t_registry();
    t_text();
    t_fuzz();
    printf("mod_ui: %d checks, %d failed\n", nchk, nbad);
    return nbad != 0;
}
