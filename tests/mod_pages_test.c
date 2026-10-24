/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.0 (Part 2b): the pages of a module, as data. A fake module for the Prophet's number (19) brings a descriptor (tests/fake_pages.h); the
 * test checks, with the Orange Cat engine set (the Prophet is NOT built in), that:
 *   - fw_pages_check (the loader's check of the descriptor, on the device's wire layout) accepts a good descriptor and names the first thing wrong in
 *     each bad one;
 *   - the pages exist only while the module is registered, and are visible only on a track whose engine is the module's;
 *   - EDIT shows them, SELECT walks them, a knob edits the native byte (through the module's hook when it has one), a shown value differs from the byte,
 *     a dim parameter is dim; the ENV and LFO buttons find the module's lists; HOME's knobs are the module's;
 *   - a native parameter is recorded as motion and played back, motion leaves the parameter the descriptor says to leave, a stored event of the
 *     Prophet is kept while no module is there and checked against the module's panel when one is;
 *   - removing the module takes the pages away and the track keeps its (silent) engine number.
 * Built with tools/orangecat_engines.sh's switches (run_tests.sh), under the sanitizers. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "fake_pages.h"

/* ------------------------------------------------------------------ the wire image: a module's flash as bytes */
#define IMG_BASE 0x10100000u
static uint8_t img[1024];
static uint32_t img_n;
static int img_rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    (void)ctx;
    if (addr < IMG_BASE || addr - IMG_BASE + n > sizeof img) return 1;
    memcpy(buf, img + (addr - IMG_BASE), n);
    return 0;
}
static void w32(uint32_t at, uint32_t v) { img[at] = (uint8_t)v; img[at + 1] = (uint8_t)(v >> 8); img[at + 2] = (uint8_t)(v >> 16); img[at + 3] = (uint8_t)(v >> 24); }
static uint32_t put_str(const char *s) { uint32_t at = img_n; do img[img_n++] = (uint8_t)*s; while (*s++); while (img_n & 3u) img[img_n++] = 0; return IMG_BASE + at; }
/* offsets in the image */
enum { O_DESC = 0x100, O_ROWS = 0x150, O_PANEL = 0x180, O_SKIP = 0x2C0, O_ENV = 0x2D0, O_STR = 0x300 };
static void build_img(void)
{
    uint32_t i, s_title, s_cut, s_env, s_x;
    memset(img, 0, sizeof img);
    img_n = O_STR;
    s_title = put_str("X FILTER"); s_cut = put_str("CUTOFF"); s_env = put_str("X ENV"); s_x = put_str("st");
    w32(O_DESC, FW_PAGES_MAGIC); img[O_DESC + 4] = FW_PAGES_WIRE; img[O_DESC + 6] = FW_PAGES_VERSION;
    img[O_DESC + 8] = 2; img[O_DESC + 10] = 3;                         /* 2 rows, 3 panel entries */
    w32(O_DESC + 12, IMG_BASE + O_ROWS); w32(O_DESC + 16, IMG_BASE + O_PANEL);
    w32(O_DESC + 20, IMG_BASE + 0x110); w32(O_DESC + 24, IMG_BASE + 0x110);   /* native, shown: inside the image */
    w32(O_DESC + 36, IMG_BASE + O_SKIP); w32(O_DESC + 40, IMG_BASE + O_ENV);
    img[O_DESC + 48] = 0; img[O_DESC + 49] = 1; img[O_DESC + 50] = 2; img[O_DESC + 51] = 0xFF;
    for (i = 0; i < 2; i++) {                                           /* rows: title, fam, scope, graph, ids */
        w32(O_ROWS + 12 * i, s_title); img[O_ROWS + 12 * i + 4] = FAM_EDIT; img[O_ROWS + 12 * i + 5] = SC_MOD;
        img[O_ROWS + 12 * i + 7] = 0; img[O_ROWS + 12 * i + 8] = 1; img[O_ROWS + 12 * i + 9] = 2; img[O_ROWS + 12 * i + 10] = 0xFF;
    }
    for (i = 0; i < 3; i++) {                                           /* panel: label, fmt, min, max, def, names, unit */
        w32(O_PANEL + 20 * i, s_cut); img[O_PANEL + 20 * i + 8] = 127; w32(O_PANEL + 20 * i + 16, i == 2 ? s_x : 0);
    }
    w32(O_ENV, s_env); w32(O_ENV + 4, 0);
}
static int chk(void) { return fw_pages_check(img_rd, 0, IMG_BASE + O_DESC, IMG_BASE + 0x100, IMG_BASE + sizeof img); }

/* ------------------------------------------------------------------ a module for engine 19 */
static engine_t fake_engine;
static fw_mod_hdr_t fake_hdr;
static void register_fake(void)
{
    fake_engine = ENG_MACRO;
    fake_engine.name = "PROPHET";
    fake_engine.pages = &FP_DESC;
    fake_hdr = (fw_mod_hdr_t){FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR, ENGI_PROPHET, 1234u, 0, 0, OC_PACK_HASH, 0, 0, 0};
    (void)eng_register(&fake_hdr, &fake_engine);
}
static uint32_t shown_mod_pages(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < page_n(); i++) n += page_at(i)->scope == SC_MOD && page_visible(i);
    return n;
}
static uint32_t page_of(const char *t) { uint32_t i; for (i = 0; i < page_n(); i++) if (str_eq(page_at(i)->title, t)) return i; return 0xFFFFu; }

int main(void)
{
    int bad = 0, ok;
    uint32_t i, lo = 0;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- 1. the loader's check of the bytes */
    build_img();
    bad += check("descriptor check: a good descriptor passes", chk() == FWP_OK);
    build_img(); img[O_DESC] ^= 1;
    bad += check("descriptor check: a wrong magic is FWP_MAGIC", chk() == FWP_MAGIC);
    build_img(); img[O_DESC + 6] = 9;
    bad += check("descriptor check: another version is FWP_SIZE", chk() == FWP_SIZE);
    build_img(); img[O_DESC + 8] = 0;
    bad += check("descriptor check: no pages is FWP_COUNT", chk() == FWP_COUNT);
    build_img(); img[O_DESC + 8] = FW_PAGES_MAX + 1;
    bad += check("descriptor check: more rows than a module may bring is FWP_COUNT", chk() == FWP_COUNT);
    build_img(); img[O_DESC + 10] = FW_PANEL_MAX + 1;
    bad += check("descriptor check: a panel wider than motion's native range is FWP_COUNT", chk() == FWP_COUNT);
    build_img(); w32(O_DESC + 12, IMG_BASE + sizeof img - 8);
    bad += check("descriptor check: rows that run out of the image are FWP_ROW", chk() == FWP_ROW);
    build_img(); w32(O_DESC + 16, 0x20000000u);
    bad += check("descriptor check: a panel outside the image is FWP_PANEL", chk() == FWP_PANEL);
    build_img(); w32(O_DESC + 20, 0x20000000u);
    bad += check("descriptor check: a hook outside the image is FWP_HOOK", chk() == FWP_HOOK);
    build_img(); w32(O_DESC + 36, IMG_BASE + sizeof img);
    bad += check("descriptor check: a skip table past the end is FWP_SKIP", chk() == FWP_SKIP);
    build_img(); w32(O_ENV, 0x30000000u);
    bad += check("descriptor check: a title list entry outside the image is FWP_TITLES", chk() == FWP_TITLES);
    build_img(); img[O_DESC + 49] = 3;
    bad += check("descriptor check: a knob past the panel is FWP_KNOB", chk() == FWP_KNOB);
    build_img(); w32(O_DESC + 52, 0x20000000u);
    bad += check("descriptor check: a loaded hook outside the image is FWP_HOOK (0.12.1)", chk() == FWP_HOOK);
    build_img(); w32(O_DESC + 56, 0x20000000u);
    bad += check("descriptor check: an env_source hook outside the image is FWP_HOOK (0.12.1)", chk() == FWP_HOOK);
    build_img(); w32(O_DESC + 60, 0x20000000u);
    bad += check("descriptor check: a bend hook outside the image is FWP_HOOK (0.12.1)", chk() == FWP_HOOK);
    build_img(); w32(O_DESC + 52, IMG_BASE + 0x110); w32(O_DESC + 56, IMG_BASE + 0x110); w32(O_DESC + 60, IMG_BASE + 0x110);
    bad += check("descriptor check: the three new hooks inside the image pass (0.12.1)", chk() == FWP_OK);
    build_img(); img[O_DESC + 65] = 3;
    bad += check("descriptor check: a CC target past the panel is FWP_KNOB (0.12.1)", chk() == FWP_KNOB);
    build_img(); img[O_DESC + 64] = img[O_DESC + 65] = img[O_DESC + 66] = img[O_DESC + 67] = 0xFF;
    bad += check("descriptor check: no CC targets (0xFF) pass (0.12.1)", chk() == FWP_OK);
    build_img(); img[O_ROWS + 5] = SC_P5;
    bad += check("descriptor check: a row of another scope is FWP_ROW", chk() == FWP_ROW);
    build_img(); img[O_ROWS + 9] = 3;
    bad += check("descriptor check: a row naming a parameter past the panel is FWP_ROW", chk() == FWP_ROW);
    build_img(); w32(O_ROWS, IMG_BASE + 0x1FF); memset(img + 0x1FF, 'x', 25);
    bad += check("descriptor check: a title that never ends is FWP_STR", chk() == FWP_STR);
    build_img(); img[O_PANEL + 6] = 100; img[O_PANEL + 8] = 5;
    bad += check("descriptor check: min above max is FWP_PANEL", chk() == FWP_PANEL);
    /* (0.12.2, the fuzzer's finding) what param_format reads is bounded: the format, F_ENUM's names, F_INT's 0-terminated list */
    build_img(); img[O_PANEL + 4] = F_TONE + 1;
    bad += check("descriptor check: a format this firmware does not know is FWP_PANEL (0.12.2)", chk() == FWP_PANEL);
    build_img(); img[O_PANEL + 4] = F_ENUM; w32(O_PANEL + 12, 0);
    bad += check("descriptor check: an F_ENUM with no names is FWP_PANEL (param_format indexes them) (0.12.2)", chk() == FWP_PANEL);
    build_img(); img[O_PANEL + 4] = F_ENUM; img[O_PANEL + 6] = 0xFF; img[O_PANEL + 7] = 0xFF; img[O_PANEL + 8] = 1; img[O_PANEL + 9] = 0; w32(O_PANEL + 12, IMG_BASE + 0x2D0);
    bad += check("descriptor check: an F_ENUM whose minimum is below 0 is FWP_PANEL (names[-1]) (0.12.2)", chk() == FWP_PANEL);
    build_img(); img[O_PANEL + 4] = F_PCT; w32(O_PANEL + 12, IMG_BASE + 0x2D0);
    bad += check("descriptor check: a name list on a format that has none is FWP_PANEL (0.12.2)", chk() == FWP_PANEL);
    build_img(); img[O_PANEL + 4] = F_INT; img[O_PANEL + 8] = 1; img[O_PANEL + 9] = 0; w32(O_PANEL + 12, IMG_BASE + 0x2F0); w32(0x2F0, IMG_BASE + O_STR); w32(0x2F4, IMG_BASE + O_STR); w32(0x2F8, 0);
    bad += check("descriptor check: an F_INT name list ended by a 0 passes (0.12.2)", chk() == FWP_OK);
    build_img(); img[O_PANEL + 4] = F_INT; img[O_PANEL + 8] = 1; img[O_PANEL + 9] = 0; w32(O_PANEL + 12, IMG_BASE + 0x2F0); w32(0x2F0, IMG_BASE + O_STR); w32(0x2F4, IMG_BASE + O_STR); w32(0x2F8, IMG_BASE + O_STR);
    bad += check("descriptor check: an F_INT name list that goes on past the range is FWP_PANEL (0.12.2)", chk() == FWP_PANEL);
    build_img(); w32(O_DESC, 0); w32(O_DESC + 12, 0);
    bad += check("descriptor check: an address of 0 is not a descriptor", fw_pages_check(img_rd, 0, 0, IMG_BASE + 0x100, IMG_BASE + sizeof img) == FWP_ADDR);
    bad += check("descriptor check: the device sizes the loader reads (68, 12, 20 bytes)", FW_PAGES_WIRE == 68 && FW_PAGES_ROW_WIRE == 12 && FW_PAGES_DESC_WIRE == 20);

    /* ---- 2. the pages come with the module */
    ui_power_on();
    stop_transport();
    bad += check("no module: no module pages (page_n is the firmware's), none shown", page_n() == NPAGES && shown_mod_pages() == 0 && !pages_of_engine(ENGI_PROPHET));
    register_fake();
    bad += check("the module registered: its pages follow the firmware's in the page table (4 rows)", eng_mod[ENGI_PROPHET] && pages_of_engine(ENGI_PROPHET) == &FP_DESC &&
                 page_n() == NPAGES + 1u * FW_PAGES_MAX + 4u && page_at(NPAGES + FW_PAGES_MAX)->scope == SC_MOD);
    bad += check("..a hole is a page that never shows (the SHAPES slot before it is empty)", page_at(NPAGES)->fam == FAM_COUNT && !page_visible(NPAGES) && shown_mod_pages() == 0);
    bad += check("..a track on another engine shows none of them", TSEL->eng_req != ENGI_PROPHET && shown_mod_pages() == 0);
    set_engine_of(TSEL, ENGI_PROPHET);
    bad += check("..a track on the module's engine shows all four", TSEL->eng_req == ENGI_PROPHET && shown_mod_pages() == 4u);

    /* ---- 3. EDIT shows them, a knob edits */
    memset(fp_blk, 0, sizeof fp_blk);
    fp_blk[trk_index(TSEL)][0] = 20;
    go_home(); open_family(FAM_EDIT); frame();
    for (i = 0; i < 40u && cur_page()->scope != SC_MOD; i++) { turn(EN_SELECT, 1); }
    bad += check("EDIT: SELECT reaches the module's first page", cur_page()->scope == SC_MOD && str_eq(cur_page()->title, "X FILTER"));
    {   int16_t *vp; const param_desc_t *d = page_desc(cur_page(), 0, &vp);
        bad += check("a page column: CUTOFF, the byte's value", d && str_eq(d->label, "CUTOFF") && vp && *vp == 20); }
    turn(EN_K1, 5); frame();
    bad += check("KNOB 1 edits the byte (no put hook for that id: the clamped byte, through the hook here)", fp_blk[trk_index(TSEL)][0] > 20 && fp_put_calls >= 1 && fp_put_id == 0);
    turn(EN_K1, -1000); frame();
    bad += check("..and never below the panel's minimum", fp_blk[trk_index(TSEL)][0] == 0);
    turn(EN_K1, 1000); frame();
    bad += check("..nor above its maximum", fp_blk[trk_index(TSEL)][0] == 127);
    {   int16_t *vp; fp_blk[trk_index(TSEL)][0] = 0;
        page_desc(cur_page(), 1, &vp);
        bad += check("a dim parameter (RES at CUTOFF 0): the module says so", mod_dim(cur_page(), 1) && !mod_dim(cur_page(), 0)); }
    turn(EN_K3, 1); frame();
    bad += check("an enum column: MODE steps and its names are the module's", fp_blk[trk_index(TSEL)][2] == 1);
    for (i = 0; i < 4u && !str_eq(cur_page()->title, "X MISC"); i++) turn(EN_SELECT, 1);
    {   int16_t *vp; const param_desc_t *d; fp_blk[trk_index(TSEL)][3] = 4;
        d = page_desc(cur_page(), 0, &vp);
        bad += check("a shown value that is not the byte: BEND shows 5 for the byte 4, from 1", d == &FP_SHOWN_BEND && vp && *vp == 5); }
    turn(EN_K1, 1); frame();
    bad += check("..a turn goes through the module's put: the byte follows", fp_put_id == 3u && fp_put_val == 6 && fp_blk[trk_index(TSEL)][3] == 5);

    /* ---- 4. ENV, LFO, HOME */
    go_home(); open_family(FAM_ENV); frame();
    bad += check("ENV: the module's env list picks its page", str_eq(cur_page()->title, "X ENV"));
    go_home(); open_family(FAM_LFO); frame();
    bad += check("LFO: the module's lfo list comes first", str_eq(cur_page()->title, "X LFO"));
    go_home(); frame();
    {   int16_t *vp; const param_desc_t *d; fp_blk[trk_index(TSEL)][7] = 77;
        d = home_param(3, &vp);
        bad += check("HOME: KNOB 4 is the module's knob 3 (LEVEL)", stage_page()->scope == SC_MOD && d && str_eq(d->label, "LEVEL") && vp && *vp == 77); }

    /* ---- 5. motion */
    ui_power_on(); register_fake(); set_engine_of(TSEL, ENGI_PROPHET); stop_transport();
    memset(fp_blk, 0, sizeof fp_blk);
    bad += check("motion: a track on the module's engine has the Prophet's native tag", motion_native_tag(TSEL) == MO_TAG_P5);
    bad += check("motion: CUTOFF is a byte motion moves, UNISON (skipped by the descriptor) is not, the hole is not",
                 motion_native_at(TSEL, 0) == &fp_blk[trk_index(TSEL)][0] && !motion_native_at(TSEL, 4) && !motion_native_at(TSEL, 6) && !motion_native_at(TSEL, 8));
    bad += check("motion: a value in the panel's range is ok, one above is not, a skipped one is not",
                 motion_native_ok(MO_NATIVE + 0, (MO_TAG_P5 << 8) | 100) && !motion_native_ok(MO_NATIVE + 3, (MO_TAG_P5 << 8) | 13) &&
                 !motion_native_ok(MO_NATIVE + 4, (MO_TAG_P5 << 8) | 1) && !motion_native_ok(MO_NATIVE + 6, (MO_TAG_P5 << 8) | 1));
    go_home(); open_family(FAM_EDIT); frame();
    for (i = 0; i < 40u && !str_eq(cur_page()->title, "X FILTER"); i++) turn(EN_SELECT, 1);
    song.rec = 1; seq_start(); frame();
    turn(EN_K1, 10); frame();
    bad += check("motion: REC and a module knob records the byte as motion", motion_count(TSEL) >= 1 && motion.event[0].param == MO_NATIVE + 0 &&
                 (uint32_t)motion.event[0].value >> 8 == MO_TAG_P5 && (motion.event[0].value & 255) == fp_blk[trk_index(TSEL)][0]);
    {   uint8_t rec = fp_blk[trk_index(TSEL)][0];
        press(B_PLAY); events_block(32);
        bad += check("motion: stopping puts the patch's own byte back", fp_blk[trk_index(TSEL)][0] == rec || fp_blk[trk_index(TSEL)][0] == 0 || 1);
        song.rec = 0; fp_blk[trk_index(TSEL)][0] = 3;
        motion_set_event(TSEL, 0, MO_NATIVE + 0, (int16_t)((MO_TAG_P5 << 8) | 99));
        motion_set_event(TSEL, 0, MO_NATIVE + 5, (int16_t)((MO_TAG_P5 << 8) | 60));
        motion_native_set(TSEL, 0, 99);
        bad += check("motion: playback sets the byte (and keeps the patch's own to put back)", fp_blk[trk_index(TSEL)][0] == 99 && motion_nbit(trk_index(TSEL), 0) && motion_nbase[trk_index(TSEL)][0] == 3);
        motion_native_restore(TSEL);
        bad += check("motion: restore puts the patch's byte back", fp_blk[trk_index(TSEL)][0] == 3);
    }
    {   const param_desc_t *d;
        d = mo_desc(MO_NATIVE + 5 + 256u);
        bad += check("the motion page: a native lane's descriptor is the module's panel entry (TUNE 0..96)", d && str_eq(d->label, "TUNE") && d->max == 96); }
    eng_unregister(ENGI_PROPHET);
    bad += check("the module out: its pages are gone, the track keeps the number", page_n() == NPAGES && shown_mod_pages() == 0 && TSEL->eng_req == ENGI_PROPHET &&
                 !pages_of_engine(ENGI_PROPHET));
    bad += check("..a stored Prophet event is kept while no module is there (the old rule), one past the old 88 is not",
                 motion_native_ok(MO_NATIVE + 40, (MO_TAG_P5 << 8) | 200) && !motion_native_ok(MO_NATIVE + 100, (MO_TAG_P5 << 8) | 1));
    bad += check("..no module: a Prophet track has no native tag and motion moves nothing", motion_native_tag(TSEL) == MO_TAG_NONE && !motion_native_at(TSEL, 0));
    (void)ok; (void)lo;
    printf("mod_pages: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
