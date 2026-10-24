/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.1 (Part 1b): the Prophet module's REAL page descriptor (tools/module/prophet_mod.c, built for the host as tests/module_host_prophet_tu.c) in the UI,
 * with the Orange Cat engine set (the Prophet is NOT built in), under the sanitizers. tests/mod_pages_test.c does this with a fake descriptor; this is the same walk
 * with the module's own: its 15 pages (the STORE page is the firmware's and not a module page), the page lists, the shown and put hooks, HOME's four knobs, ENV and LFO,
 * a native parameter recorded as motion and played back (and the voice assignment, which motion leaves alone), and the module going away again.
 *   build/host/mod_pages_prophet_test
 * Revisions: 0.12.1: first version. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "fw_pages_wire.h"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern const fw_pages_t *oc_host_module_pages(void);

static fw_mod_hdr_t mod_hdr;
static void register_module(void)
{
    oc_host_fw = &FW_API;                               /* what the device gives its module */
    mod_hdr = OC_MODULE_HDR;
    mod_hdr.engine = ENGI_PROPHET;
    (void)eng_register(&mod_hdr, oc_host_module_engine());
}
static uint32_t shown_mod_pages(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < page_n(); i++) n += page_at(i)->scope == SC_MOD && page_visible(i);
    return n;
}
static void goto_page(const char *title)
{
    uint32_t i;
    go_home(); open_family(FAM_EDIT); frame();
    for (i = 0; i < 60u && !(cur_page()->scope == SC_MOD && str_eq(cur_page()->title, title)); i++) turn(EN_SELECT, 1);
}
static uint8_t *raw(void) { return p5_patch[trk_index(TSEL)].raw; }

int main(void)
{
    int bad = 0;
    uint32_t i;
    const fw_pages_t *d = oc_host_module_pages();
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    stop_transport();
    bad += check("no module: no Prophet pages, none shown", shown_mod_pages() == 0 && !pages_of_engine(ENGI_PROPHET) && !eng_ok(ENGI_PROPHET));
    register_module();
    bad += check("the module registered: the Prophet engine is there (201 presets) and its descriptor is the engine's pages", eng_mod[ENGI_PROPHET] && eng_ok(ENGI_PROPHET) &&
                 ENGINES[ENGI_PROPHET]->npresets == 201u && pages_of_engine(ENGI_PROPHET) == d);
    bad += check("..a track on another engine shows none of the pages", TSEL->eng_req != ENGI_PROPHET && shown_mod_pages() == 0);
    set_engine_of(TSEL, ENGI_PROPHET);
    bad += check("..a track on the Prophet shows all 15", TSEL->eng_req == ENGI_PROPHET && shown_mod_pages() == d->npages && d->npages == 15u);

    /* EDIT: the pages in order, the titles as the built-in Prophet's */
    {   static const char *const T[] = {"P5 OSC A", "P5 OSC B", "P5 B WAVES", "P5 MIXER", "P5 FILTER", "P5 FLT ENV", "P5 AMP ENV", "P5 ENV MOD", "P5 POLYMOD", "P5 MOD FLT",
                                        "P5 LFO", "P5 WHEEL", "P5 WHEEL 2", "P5 UNISON", "P5 BEND"};
        int ok = 1;
        for (i = 0; i < 15u; i++) ok &= str_eq(d->rows[i].title, T[i]);
        bad += check("the 15 pages are the Prophet's, in its order", ok); }
    goto_page("P5 FILTER");
    bad += check("EDIT: SELECT reaches the module's FILTER page", cur_page()->scope == SC_MOD && str_eq(cur_page()->title, "P5 FILTER"));
    {   int16_t *vp; const param_desc_t *pd = page_desc(cur_page(), 0, &vp);
        bad += check("a page column: CUTOFF, with the patch's byte", pd && str_eq(pd->label, "CUTOFF") && vp && *vp == raw()[cur_page()->id[0]]); }
    {   uint8_t before = raw()[cur_page()->id[0]];
        turn(EN_K1, -5); frame();
        bad += check("KNOB 1 edits the patch's CUTOFF byte (the module's put hook)", raw()[cur_page()->id[0]] != before); }
    goto_page("P5 OSC A");
    {   int16_t *vp; const param_desc_t *pd = page_desc(cur_page(), 0, &vp);
        uint8_t id0 = cur_page()->id[0];
        raw()[id0] = 48;                                  /* FREQ A, raw half steps: the shown value is semitones from -12 (the module's shown hook) */
        pd = page_desc(cur_page(), 0, &vp);
        bad += check("a shown value that is not the byte: the keyed tuning shows semitones (raw 48 -> 12)", pd && vp && *vp == 12 && raw()[id0] == 48); }

    /* ENV, LFO, HOME */
    go_home(); open_family(FAM_ENV); frame();
    bad += check("ENV: the module's list picks its first page (P5 FLT ENV)", str_eq(cur_page()->title, "P5 FLT ENV"));
    go_home(); open_family(FAM_LFO); frame();
    bad += check("LFO: the module's list picks P5 LFO", str_eq(cur_page()->title, "P5 LFO"));
    go_home(); frame();
    {   int16_t *vp; const param_desc_t *pd = home_param(0, &vp);
        bad += check("HOME: KNOB 1 is the Prophet's CUTOFF", stage_page()->scope == SC_MOD && pd && str_eq(pd->label, "CUTOFF") && vp); }

    /* motion */
    stop_transport();
    bad += check("motion: a track on the Prophet has the Prophet's native tag", motion_native_tag(TSEL) == MO_TAG_P5);
    {   uint32_t ids_ok = 1, k;
        for (k = 0; k < d->npanel; k++) {
            int skip = d->motion_skip && (d->motion_skip[k >> 3] >> (k & 7u) & 1u);
            if (d->panel[k].label) ids_ok &= (motion_native_at(TSEL, k) != 0) == !skip;
            else ids_ok &= motion_native_at(TSEL, k) == 0;
        }
        bad += check("motion: every parameter of the panel is a native lane except the voice assignment the descriptor skips (UNISON, count, detune, retrigger)", ids_ok); }
    goto_page("P5 FILTER");
    {   uint8_t id0 = cur_page()->id[0];
        song.rec = 1; seq_start(); frame();
        turn(EN_K1, -10); frame();
        bad += check("motion: REC and a module knob records the byte as motion", motion_count(TSEL) >= 1 && motion.event[0].param == MO_NATIVE + id0 &&
                     (uint32_t)motion.event[0].value >> 8 == MO_TAG_P5 && (motion.event[0].value & 255) == raw()[id0]);
        press(B_PLAY); events_block(32);
        song.rec = 0;
        raw()[id0] = 3;
        motion_set_event(TSEL, 0, MO_NATIVE + id0, (int16_t)((MO_TAG_P5 << 8) | 99));
        motion_native_set(TSEL, id0, 99);
        bad += check("motion: playback sets the patch's byte and keeps the patch's own to put back", raw()[id0] == 99 && motion_nbit(trk_index(TSEL), id0) && motion_nbase[trk_index(TSEL)][id0] == 3);
        motion_native_restore(TSEL);
        bad += check("motion: restore puts the patch's byte back", raw()[id0] == 3); }

    /* the module out */
    eng_unregister(ENGI_PROPHET);
    bad += check("the module out: its pages are gone, the track keeps the number, the engine is silent again", shown_mod_pages() == 0 && TSEL->eng_req == ENGI_PROPHET &&
                 !pages_of_engine(ENGI_PROPHET) && !eng_ok(ENGI_PROPHET));
    bad += check("..no module: a Prophet track has no native tag and motion moves nothing", motion_native_tag(TSEL) == MO_TAG_NONE && !motion_native_at(TSEL, 0));
    register_module();
    bad += check("..and back again: the engine plays again (registered, 201 presets, the pages return)", eng_ok(ENGI_PROPHET) && ENGINES[ENGI_PROPHET]->npresets == 201u && shown_mod_pages() == 15u);
    printf("mod_pages_prophet: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
