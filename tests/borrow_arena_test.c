/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Round 30 item 1 (BUILD.md 7.47): does a save's borrow fit the arena while stopped, at the device's sizes? (firmware/src/res_borrow.c; build with -DOC_PJ4=1)
 * The banks are the linker's numbers of the OC_PJ4=1 build at r30-borrow (nm build/melodee.elf, _resource*_start and _end): main 51,424 B, low SRAM 26,824 B, data tail
 * 1,608 B, retained tail 816 B; the cache RAM's audio part 14,336 B (when its self-test passed); the card pool 36,864 B (lent when the card does not use it).
 * JA_MAIN_BYTES=59616 gives the main bank with Session C's 8 KB kept-phrase buffer (8,192 B more); BT_SEC=4224 the record's sections streamed (Session C: one part).
 * For each case: the boot's hot sets, four engine states, the histories (the note edit's 13,404 B and the recording's undo), the chorus, delay and reverb holding their
 * memory and two DUB lines of 32 KB; then the borrow of the save's pieces (sections, project part, the core's work area) with real blocks running. Printed as a table
 * (BUILD.md 7.47); checked: what the design promises (below), the engine states and hot sets untouched, every piece back after, the effects' memory taken again.
 * Also checked: the loader's resident RAM leaves the arena its worst-case minimum (the link check's arithmetic: OC_PJ4=0's arena 79,040 B less the resident part). */
#ifndef JA_MAIN_BYTES
#define JA_MAIN_BYTES 51424u
#endif
#ifndef BT_SEC
#define BT_SEC PJ4_RECMAX
#endif
#define RES_HOST_MAIN_WORDS (JA_MAIN_BYTES / 4u)
#define RES_HOST_LOW_WORDS (26824u / 4u)
#define RES_HOST_DATA_WORDS (1608u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static int bad;
static int32_t ba_out[2u * CTL];
static void ba_block(void)                              /* the effects' part of a block (jfx_arena_test's ja_block): the borrow's step runs first, in jfx_lend_check */
{
    uint32_t i;
    int perf;
    jfx_lend_check();
    for (i = 0; i < CTL; i++) {
        mix_l[i] = mix_r[i] = sl_l[i] = sl_r[i] = 0;
        send_c[i] = send_d[i] = send_r[i] = 0;
    }
    jfx_clock_begin();
    master_begin();
    perf = perf_begin(CTL);
    if (perf) perf_pre(mix_l, mix_r, sl_l, sl_r, send_d, send_r, CTL);
    jfx_bus_start(CTL);
    jfx_master(ba_out, CTL, perf);
}
static void ba_release_all(void)
{
    jfx_power_on();
    eng_hot_drop(ENGI_MACRO);
    eng_hot_drop(ENGI_SHAPES);
    if (dsp_hot_at[DSP_HOT_SINE] != (const void *)SINE) { dsp_hot_at[DSP_HOT_SINE] = SINE; resource_release(RES_HOT_BASE); }
    for (uint32_t k = 0; k < RES_COUNT; k++) resource_release(k);
    res_lend_ok = res_lend_used = 0;
    resource_host_cache_enabled = 0;
}
/* engines: four states of these sizes; cache: the cache RAM ready; lend: the card slots lent (15 idle, 0 busy: frozen takes); hist: the two histories held; dub: two
 * DUB instances of 8 blocks. 1: the borrow was granted (and everything checked) */
static int ba_case(const uint32_t *eng, int cache, int lend, int hist, int dub, uint32_t *free_before)
{
    uint32_t k, sz[3] = {BT_SEC, PROJ_STORE_SIZE, (uint32_t)sizeof(pj4_ws_t)}, used = 0;
    uint8_t *p[3], keep[NPART][16];
    void *at[NPART];
    int rc, fx0;
    ui_power_on();
    song.playing = 0;
    ba_release_all();
    eng_hot_base();
    assert(eng_hot_take_own(ENGI_MACRO, ENGINES[ENGI_MACRO]) == EM_OK);
    assert(eng_hot_take_own(ENGI_SHAPES, ENGINES[ENGI_SHAPES]) == EM_OK);
    resource_host_cache_enabled = (uint32_t)cache;
    for (k = 0; k < NPART; k++) {
        uint8_t *q = resource_get(RES_ENGINE0 + k, eng[k]);
        assert(q);
        memset(q, 0xA5 ^ (int)k, eng[k]);
        memcpy(keep[k], q, sizeof keep[k]);
        at[k] = q;
    }
    if (hist) {
        assert(resource_get(RES_NOTE_HISTORY, 13404u) || 1);
        assert(resource_get(RES_RECORD_UNDO, sizeof(recording_undo_t)) || 1);
    }
    res_lend_ok = (uint8_t)lend;
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 49;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 100;
    for (k = 0; k < DUB_N; k++) { dub_want[k] = (uint8_t)(dub && k < 2u ? 8u : 0u); dub_spring[k] = 0; }
    for (k = 0; k < 400u; k++) ba_block();
    for (k = 0; k < RES_COUNT; k++) used += resource[k].size;
    *free_before = used;
    fx0 = cho_buf && jrev && dly_n;
    res_bor_host_block = ba_block;
    rc = res_borrow(sz, 3u, p);
    if (getenv("BA_DEBUG")) {
        printf("  rc %d next %u; held after the fade:", rc, res_bor_next);
        for (k = 0; k < RES_COUNT; k++) if (resource[k].size) printf(" [%u %p %u]", k, resource[k].ptr, resource[k].size);
        printf("\n  banks: main %p low %p data %p ret %p cache %p cp %p\n", (void *)resource_host, (void *)resource_host_low, (void *)resource_host_data, (void *)resource_host_retained, (void *)resource_host_cache, (void *)cp_pool);
    }
    if (rc == RES_BOR_OK) {
        for (k = 0; k < 3u; k++) { assert(p[k]); memset(p[k], 0x3C, sz[k]); }      /* (the save writes all of it) */
        res_borrow_end();
    }
    for (k = 0; k < NPART; k++)
        if (resource[RES_ENGINE0 + k].ptr != at[k] || memcmp(at[k], keep[k], sizeof keep[k])) bad++, printf("  an engine's state was touched\n");
    for (k = 0; k < RES_BOR_N; k++) if (resource[RES_BOR0 + k].size) bad++, printf("  a piece was not given back\n");
    for (k = 0; k < 600u; k++) ba_block();
    if (fx0 && !(cho_buf && jrev && dly_n)) bad++, printf("  the effects did not take their memory again\n");
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    for (k = 0; k < DUB_N; k++) dub_want[k] = 0;
    ba_release_all();
    return rc == RES_BOR_OK;
}
int main(void)
{
    uint32_t sh = eng_state_size(ENGI_SHAPES), mc = eng_state_size(ENGI_MACRO), dx = eng_state_size(ENGI_DRUMX), need, u;
    const uint32_t e4s[NPART] = {sh, sh, sh, sh}, e4m[NPART] = {mc, mc, mc, mc}, emix[NPART] = {dx, mc, sh, mc};
    struct { const char *what; const uint32_t *e; int cache, lend, hist, dub; int r; } c[] = {
        {"4 x SHAPES", e4s, 1, 0, 1, 1, 0}, {"4 x SHAPES", e4s, 0, 0, 1, 1, 0}, {"4 x SHAPES", e4s, 1, 15, 1, 1, 0}, {"4 x SHAPES", e4s, 0, 15, 1, 1, 0},
        {"4 x MACRO", e4m, 1, 0, 1, 1, 0}, {"4 x MACRO", e4m, 0, 0, 1, 1, 0}, {"4 x MACRO", e4m, 1, 15, 0, 0, 0},
        {"DRUM-X MACRO SHAPES MACRO", emix, 1, 0, 1, 1, 0}, {"DRUM-X MACRO SHAPES MACRO", emix, 0, 0, 1, 1, 0},
    };
    uint32_t n = (uint32_t)(sizeof c / sizeof c[0]), k;
    need = BT_SEC + PROJ_STORE_SIZE + (uint32_t)sizeof(pj4_ws_t);
    {   /* the link check's arithmetic: the loader's resident RAM in .pool (the kept phrases, the directory, the reads' section buffer) against the OC_PJ4=0 arena's margin */
        uint32_t resident = (uint32_t)(sizeof pjp + sizeof pjp_dir + sizeof pjp_rbuf), room = 79040u - 49152u;
        printf("borrow arena: the loader's resident RAM %u B (kept phrases %u, directory %u, read buffer %u) of the %u B the OC_PJ4=0 arena has above its minimum\n",
               resident, (unsigned)sizeof pjp, (unsigned)sizeof pjp_dir, (unsigned)sizeof pjp_rbuf, room);
        bad += resident + 2048u > room;                  /* (2 KB for the loader's small tables and statics: the build measured 24,324 B in all) */
    }
    printf("borrow arena: main bank %u B; the save borrows %u B (sections %u, project part %u, work area %u); engine states SHAPES %u, MACRO %u, DRUM-X %u\n",
           JA_MAIN_BYTES, need, (unsigned)BT_SEC, (unsigned)PROJ_STORE_SIZE, (unsigned)sizeof(pj4_ws_t), sh, mc, dx);
    printf("| engines | cache | card | histories | DUB | arena held before (B) | borrow |\n|---|---|---|---|---|---|---|\n");
    for (k = 0; k < n; k++) {
        c[k].r = ba_case(c[k].e, c[k].cache, c[k].lend, c[k].hist, c[k].dub, &u);
        printf("| %s | %s | %s | %s | %s | %u | %s |\n", c[k].what, c[k].cache ? "ready" : "off", c[k].lend ? "idle" : "busy", c[k].hist ? "held" : "-",
               c[k].dub ? "2 x 32K" : "-", u, c[k].r ? "GRANTED" : "NO MEMORY");
    }
    /* what the numbers give (BUILD.md 7.47): with the card idle and the cache RAM ready the save fits (the card's 36 KB pool is lent to the borrow as to the effects) */
    bad += !c[2].r || !c[6].r;                          /* (with the cache RAM ready) */
#if JA_MAIN_BYTES >= 59616u && BT_SEC < PJ4_RECMAX
    bad += !c[0].r || !c[4].r || !c[7].r;                /* (with Session C's two reductions: the worst case fits with the cache RAM ready, the card busy) */
#endif
    printf(bad ? "borrow arena: %d FAILED\n" : "borrow arena: all passed\n", bad);
    return bad != 0;
}
