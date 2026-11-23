/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (wave-engine M3) WAVE's patch, the owner's (a) of November 22 (docs/candidates/wave_engine.md 3): the patch lives in the module's state (the arena), its pages
 * edit it byte for byte, a user preset keeps it (mod_rec.h, upreset.c up_mxr_put), and the firmware gives the stored copy back to a new state (voice.c
 * mod_ext_apply). The firmware's units (hostsim: engines, params, voice, mix, seq; ui.c, upreset.c, project.c on a simulated NOR) with the WAVE module's host unit
 * (tests/module_host_wave_tu.c) registered by hand:
 *   - the pages: WAVE's rows after SHAPES' and the Prophet's, shown only on a WAVE track, a value the byte of the state; from the UI side the module's state is
 *     reached only while it is held (engines.c fw_resource_get: never a take outside the audio interrupt);
 *   - a factory preset: its patch (a new state's, and the loaded hook's on a state there);
 *   - a user preset: saved with the extension (the block as it plays), loaded again (the same bytes; on another engine first: after the engine change);
 *   - a release of the state: an edit not saved is gone, the user preset's patch comes back; a factory preset's comes back as it was;
 *   - a damaged extension, another version, a record without one (a preset of before): the factory preset 0's patch; a byte out of range: clamped;
 *   - a project: a track on the user preset loads its patch, one on a factory preset that one's.
 *   build/host/wave_patch_test
 */
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define MELODEE_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}
static int list_gone(uint32_t i) { (void)i; return 0; }    /* (browse.c's: no LIST here) */

static uint8_t nor[0x100000], flash_ok = 1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"

#if !OC_WAVE
#error "build with the build set (OC_WAVE=1)"
#endif
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern const uint8_t *wave_host_row(uint32_t i);
extern const fw_pages_t *wave_host_pages(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("wave patch: %-128s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void boot(void)
{
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    pattern_init();
    memset(&motion, 0, sizeof motion);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(proj_bank_slot, 0, sizeof proj_bank_slot);
    memset(proj_meta, 0, sizeof proj_meta);
    up_cache_reset(); native_cache_reset();
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    proj_cur = PROJ_NO_SLOT;
    smp_slot_boot(); ocp_scan(); ocj_scan();
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
    up_boot();
}
/* n blocks of the audio side (the interrupt's: fw_host_ui 0) */
static void blocks(uint32_t n)
{
    int32_t o[2 * CTL];
    fw_host_ui = 0;
    while (n--) mix_block(o, CTL);
    fw_host_ui = 1;
}
enum { WNB = 49 };
enum { WF_BP = 1, WS_LFO2 = 2, WD_WARPB = 3 };          /* (tools/module/wave/eng_wave.c's: the module's unit has them) */
static const fw_pages_t *pg;
/* the block as the UI reaches it (0: not held) */
static uint8_t *ui_block(void) { fw_host_ui = 1; return pg->native(&trk[0]); }
/* .. compared with a patch (41 bytes after the eight) */
static int block_is(const uint8_t *patch)
{
    const uint8_t *b = ui_block();
    return b && !memcmp(b + 8, patch, WNB - 8u);
}
/* the bytes the user preset holds (its body), n */
static uint32_t rec_body(uint32_t k, uint8_t *body)
{
    uint8_t b[MXR_ROOM];
    const up_rec_t *r = up_rec(k);
    uint32_t n = mxr_unpack(b, up_mxr_get(r, b), r->engine);
    memcpy(body, b + MXR_HEAD, n);
    return n;
}
/* a record of slot k with its body changed by fn, CRC remade (or not), into slot d */
static void rec_edit(uint32_t k, uint32_t d, uint32_t at, uint8_t v, int crc, uint8_t ver)
{
    up_rec_t r = *up_rec(k);
    uint8_t b[MXR_ROOM], body[MXR_BODY_MAX];
    uint32_t n = rec_body(k, body);
    body[at] = v;
    mxr_pack(b, r.engine, body, n);
    if (!crc) b[3] ^= 0x5Au;
    b[1] = ver;
    up_mxr_put(&r, b, MXR_HEAD + n);
    up_put(d, &r);
}

int main(void)
{
    fw_mod_hdr_t h;
    const engine_t *em;
    uint8_t edited[WNB], saved[WNB], *b;
    uint32_t i, k, rows = 0, shown = 0;
    memset(nor, 0xFF, sizeof nor);
    boot();
    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    check("WAVE registers (engine 32), its descriptor's native block 49 bytes (8 for the edit values, the 41 of the patch)",
          eng_register(&h, em) == EM_OK && (pg = pages_of_engine(ENGI_WAVE)) == wave_host_pages() && pg->npanel == WNB && pg->native && pg->loaded);
    /* the pages: after PAGES, SHAPES' and the Prophet's indices; on a WAVE track only */
    for (i = NPAGES + 2u * FW_PAGES_MAX; i < NPAGES + 3u * FW_PAGES_MAX; i++)
        if (modpage_engine(i, 0) == ENGI_WAVE) rows++;
    host_preset(&trk[0], ENGI_WAVE, 1);
    blocks(4);
    song.sel = 0;
    for (i = NPAGES + 2u * FW_PAGES_MAX; i < NPAGES + 2u * FW_PAGES_MAX + rows; i++) shown += page_visible(i);
    check("WAVE's eleven rows have indices of their own (MODPAGE_ENG: SHAPES, PROPHET, WAVE), page_n reaches them, all shown on a WAVE track",
          rows == 11u && page_n() == NPAGES + 2u * FW_PAGES_MAX + rows && shown == rows && !strcmp(page_at(NPAGES + 2u * FW_PAGES_MAX)->title, "WAVE OSC A"));
    check("a factory preset (WAVE LEAD): the new state's patch is the preset's", block_is(wave_host_row(1)));
    {   /* a page's value is the byte; OCT shown from the middle (F_OFS) */
        int16_t *vp = 0;
        const page_t *tune = page_at(NPAGES + 2u * FW_PAGES_MAX + 2u);
        const param_desc_t *d = page_desc(tune, 0, &vp);
        check("WAVE TUNE: OCT A is the state's byte, F_OFS over 0..4 (shown -2..2)", d && vp && *vp == ui_block()[tune->id[0]] && d->fmt == F_OFS && d->min == 0 && d->max == 4);
    }
    /* an edit, as the page's default put writes it (the byte, clamped to the entry): TAB A to VOWEL, the filter to BP, slot 3 from LFO 2 to WARP B */
    b = ui_block();
    b[8 + 0] = 2; b[8 + 13] = WF_BP; b[8 + 23 + 8] = WS_LFO2; b[8 + 23 + 9] = WD_WARPB; b[8 + 23 + 10] = 127 + 60;
    memcpy(edited, b, WNB);
    blocks(2);
    check("the edit stays in the state through the audio side", block_is(edited + 8));
    /* from the UI side nothing is taken: a track whose state is not held has no block there */
    trk[1].eng_req = trk[1].engine = ENGI_WAVE;
    eng_state_clear(1);
    fw_host_ui = 1;
    check("from the UI, a WAVE track whose state is not held: no block (fw_resource_get takes nothing outside the audio side)",
          pg->native(&trk[1]) == 0 && resource[RES_ENGINE0 + 1].size == 0);
    trk[1].eng_req = trk[1].engine = 0;
    /* a user preset keeps it */
    check("saved as user preset 6: the record holds the extension, the block as it plays (49 bytes, mark M, version 1)",
          up_store_to(5, 0, "MY WAVE") == 0 && rec_body(5, saved) == WNB && !memcmp(saved, edited, WNB));
    apply_preset_to(&trk[0], 3);
    check("a factory preset (WAVE KEYS) loaded on the held state: its patch (the loaded hook)", block_is(wave_host_row(3)));
    up_load_to(5, 0);
    blocks(1);
    check("the user preset loaded again: its patch, byte for byte", block_is(edited + 8) && trk[0].user == 6u && trk[0].preset == 0u);
    /* a release: an edit not saved is gone, the user preset's patch comes back */
    ui_block()[8 + 15] = 99;
    eng_state_clear(0);
    check("released: no block (the state given back)", ui_block() == 0);
    blocks(1);
    check("taken again: the user preset's patch (the unsaved ENV 2 attack gone)", block_is(edited + 8));
    apply_preset_to(&trk[0], 4);
    blocks(1);
    eng_state_clear(0);
    blocks(1);
    check("a factory preset's track released and taken again: that preset's patch", block_is(wave_host_row(4)));
    /* from another engine: the user preset after the engine change */
    host_preset(&trk[0], ENGI_CZ, 0);
    blocks(2);
    check("on another engine (CZ-1): no WAVE block", ui_block() == 0);
    up_load_to(5, 0);
    for (k = 0; k < 8u && !block_is(edited + 8); k++) blocks(1);
    check("the WAVE user preset loaded on a CZ-1 track: after the engine change, its patch", block_is(edited + 8) && trk[0].engine == ENGI_WAVE);
    /* records that do not read: the factory preset 0's patch; a byte out of range: clamped */
    rec_edit(5, 7, 20, 0, 0, MXR_VER);
    up_load_to(7, 0); blocks(1);
    check("a damaged extension (its CRC): the factory preset 0's patch", block_is(wave_host_row(0)));
    rec_edit(5, 8, 20, 0, 1, 2);
    up_load_to(5, 0); blocks(1);
    up_load_to(8, 0); blocks(1);
    check("an extension of another version: the factory preset 0's patch", block_is(wave_host_row(0)));
    {
        up_rec_t r = *up_rec(5);
        memset(r.cz_extra, 0, sizeof r.cz_extra);
        up_put(9, &r);
        up_load_to(5, 0); blocks(1);
        up_load_to(9, 0); blocks(1);
        check("a WAVE preset of before (no extension): the factory preset 0's patch, as it played before", block_is(wave_host_row(0)));
    }
    rec_edit(5, 10, 8, 200, 1, MXR_VER);
    up_load_to(10, 0); blocks(1);
    check("a byte out of its range (TAB A 200): clamped to the entry's (USER 16, 19)", (b = ui_block()) && b[8] == 19u && !memcmp(b + 9, edited + 9, WNB - 9u));
    /* a project: a track on the user preset, another on a factory preset */
    up_load_to(5, 0); blocks(1);
    host_preset(&trk[2], ENGI_WAVE, 6);
    blocks(2);
    check("project saved (track 1 on user preset 6, track 3 on WAVE PLUCK)", project_save(0) == 0);
    apply_preset_to(&trk[0], 2);
    blocks(1);
    fw_host_ui = 1;
    pg->native(&trk[2])[8 + 0] = 3;                     /* (an unsaved edit on track 3: the project load puts the preset's back) */
    project_load(0);
    for (k = 0; k < 8u; k++) blocks(1);
    fw_host_ui = 1;
    check("project loaded: track 1 plays the user preset's patch, track 3 WAVE PLUCK's", block_is(edited + 8) && trk[0].user == 6u &&
          pg->native(&trk[2]) && !memcmp(pg->native(&trk[2]) + 8, wave_host_row(6), WNB - 8u));
    printf(bad ? "wave patch: FAILED\n" : "wave patch: all passed\n");
    return bad != 0;
}
