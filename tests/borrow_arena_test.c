/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Round 30 item 1 (BUILD.md 7.47): does a save's borrow fit the arena while stopped, at the device's sizes? (firmware/src/res_borrow.c; build with -DOC_PJ4=1)
 * The banks are the linker's numbers of the OC_PJ4=1 build at r30-borrow after the review (nm build/melodee.elf, _resource*_start and _end): main 50,656 B, low SRAM 26,824 B, data tail
 * 1,560 B, retained tail 816 B; the cache RAM's audio part 14,336 B (when its self-test passed); the card pool 36,864 B (lent when the card does not use it).
 * JA_MAIN_BYTES=58848 gives the main bank with Session C's 8 KB kept-phrase buffer (8,192 B more); BT_SEC=4224 the record's sections streamed (Session C: one part).
 * For each case: the boot's hot sets, four engine states, the histories (the note edit's 13,404 B and the recording's undo), the chorus, delay and reverb holding their
 * memory and two DUB lines of 32 KB; then the borrow of the save's pieces (sections, project part, the core's work area) with real blocks running. Printed as a table
 * (BUILD.md 7.47); checked: what the design promises (below), the engine states and hot sets untouched, every piece back after, the effects' memory taken again.
 * Also checked: the loader's resident RAM leaves the arena its worst-case minimum (the link check's arithmetic: OC_PJ4=0's arena 76,704 B at 73f348f8 less the resident part). */
#ifndef JA_MAIN_BYTES
#define JA_MAIN_BYTES 50656u
#endif
#ifndef BT_SEC
#define BT_SEC PJ4_RECMAX
#endif
#define RES_HOST_MAIN_WORDS (JA_MAIN_BYTES / 4u)
#define RES_HOST_LOW_WORDS (26824u / 4u)
#define RES_HOST_DATA_WORDS (1560u / 4u)
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
static int ba_dropped, ba_hist_kept;                    /* (the last case: the histories were dropped for the borrow; they are all still held) */
/* engines: four states of these sizes; cache: the cache RAM ready; lend: the card slots lent (15 idle, 0 busy: frozen takes); hist: the two histories held; dub: two
 * DUB instances of 8 blocks. 1: the borrow was granted (and everything checked) */
static int ba_case(const uint32_t *eng, int cache, int lend, int hist, int dub, uint32_t *free_before)
{
    uint32_t k, sz[3] = {BT_SEC, PROJ_STORE_SIZE, (uint32_t)sizeof(pj4_ws_t)}, used = 0;
    uint8_t *p[3], keep[NPART][16];
    void *at[NPART];
    int rc, fx0;
    uint32_t d0, h0, h1;
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
    d0 = res_bor_dropped;
    h0 = resource[RES_NOTE_HISTORY].size, h1 = resource[RES_RECORD_UNDO].size;   /* (what the histories hold now: an effect's resource_get may have taken the undo already) */
    rc = res_borrow(sz, 3u, p);
    ba_dropped = res_bor_dropped != d0;
    ba_hist_kept = resource[RES_NOTE_HISTORY].size == h0 && resource[RES_RECORD_UNDO].size == h1;
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
    const uint32_t e3s[NPART] = {sh, sh, dx, dx};
    /* want: the result at the numbers this file is built with by default (r30-borrow: main 50,656 B, the save 38,836 B); -1 not checked (another build's numbers) */
    struct { const char *what; const uint32_t *e; int cache, lend, hist, dub; int want, want_drop; int r, drop; } c[] = {
        {"4 x SHAPES", e4s, 1, 0, 1, 1, 0, 0}, {"4 x SHAPES", e4s, 0, 0, 1, 1, 0, 0}, {"4 x SHAPES", e4s, 1, 15, 1, 1, 1, -1}, {"4 x SHAPES", e4s, 0, 15, 1, 1, 0, 0},
        {"4 x MACRO", e4m, 1, 0, 1, 1, 0, 0}, {"4 x MACRO", e4m, 0, 0, 1, 1, 0, 0}, {"4 x MACRO", e4m, 1, 15, 1, 1, 1, -1},
        {"DRUM-X MACRO SHAPES MACRO", emix, 1, 0, 1, 1, 0, 0}, {"DRUM-X MACRO SHAPES MACRO", emix, 0, 0, 1, 1, 0, 0},
        {"SHAPES SHAPES DRUM-X DRUM-X", e3s, 1, 0, 1, 1, 1, 1},       /* (fits only with the histories dropped: they go) */
        {"SHAPES SHAPES DRUM-X DRUM-X", e3s, 1, 0, 0, 1, 1, 0},       /* (no histories: fits as it is, nothing dropped) */
    };
    uint32_t n = (uint32_t)(sizeof c / sizeof c[0]), k;
    need = BT_SEC + PROJ_STORE_SIZE + (uint32_t)sizeof(pj4_ws_t);
    {   /* the link check, computed: an OC_PJ4=1 build/melodee.elf (when there is one: ci_step.py build --switch OC_PJ4=1): its arena against its _resource_minimum, from nm;
         * and the resident part of the loader that is file-scope here (the kept phrases, the directory, the reads' section buffer) */
        FILE *f = popen("nm build/melodee.elf 2>/dev/null", "r");
        char line[256];
        unsigned long long a = 0, z = 0, m = 0, w = 0, v;
        char ty, nm[200];
        while (f && fgets(line, sizeof line, f))
            if (sscanf(line, "%llx %c %199s", &v, &ty, nm) == 3) {
                if (!strcmp(nm, "_resource_start")) a = v;
                else if (!strcmp(nm, "_resource_end")) z = v;
                else if (!strcmp(nm, "_resource_minimum")) m = v;
                else if (!strcmp(nm, "pjp_rbuf")) w = v;
            }
        if (f) pclose(f);
        printf("borrow arena: the loader's resident RAM here %u B (kept phrases %u, directory %u, read buffer %u)\n",
               (unsigned)(sizeof pjp + sizeof pjp_dir + sizeof pjp_rbuf), (unsigned)sizeof pjp, (unsigned)sizeof pjp_dir, (unsigned)sizeof pjp_rbuf);
        if (a && z && m && w) {
            printf("borrow arena: build/melodee.elf (OC_PJ4=1, the borrow): arena %llu B, minimum %llu B\n", z - a, m);
            bad += z - a < m;
        } else printf("borrow arena: no OC_PJ4=1 build/melodee.elf with the borrow: the link check not repeated here (the build makes it)\n");
    }
    {   /* the UNSAVED rule through the UI's poll (autosave_poll, ui_input.c): playing, a SAVE waits; it runs at the stop */
        int ok = 1;
        ui_power_on();
        song.playing = 1;
        ok &= project_save_as(1, "POLL") == 1 && !strcmp(ui.msg, "UNSAVED") && project_pending() && !project_used(1);
        autosave_poll(0);
        ok &= project_pending() && !project_used(1);
        song.playing = 0;
        autosave_poll(0);
        ok &= !project_pending() && project_used(1);
        printf("borrow arena: %-100s %s\n", "a SAVE while playing says UNSAVED and waits; autosave_poll runs it at the stop", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    printf("borrow arena: main bank %u B; the save borrows %u B (sections %u, project part %u, work area %u); engine states SHAPES %u, MACRO %u, DRUM-X %u\n",
           JA_MAIN_BYTES, need, (unsigned)BT_SEC, (unsigned)PROJ_STORE_SIZE, (unsigned)sizeof(pj4_ws_t), sh, mc, dx);
    printf("| engines | cache | card | histories | DUB | arena held before (B) | borrow |\n|---|---|---|---|---|---|---|\n");
    for (k = 0; k < n; k++) {
        c[k].r = ba_case(c[k].e, c[k].cache, c[k].lend, c[k].hist, c[k].dub, &u);
        c[k].drop = ba_dropped;
        printf("| %s | %s | %s | %s | %s | %u | %s%s |\n", c[k].what, c[k].cache ? "ready" : "off", c[k].lend ? "idle" : "busy", c[k].hist ? "held" : "-",
               c[k].dub ? "2 x 32K" : "-", u, c[k].r ? "GRANTED" : "NO MEMORY", c[k].drop ? " (histories dropped)" : "");
        if (!c[k].r && !ba_hist_kept) bad++, printf("  refused, yet the histories were dropped\n");
#if JA_MAIN_BYTES == 50656u && BT_SEC == PJ4_RECMAX
        if (c[k].r != c[k].want) bad++, printf("  expected %s\n", c[k].want ? "GRANTED" : "NO MEMORY");
        if (c[k].want_drop >= 0 && c[k].drop != c[k].want_drop) bad++, printf("  expected the histories %s\n", c[k].want_drop ? "dropped" : "kept");
#endif
    }
#if JA_MAIN_BYTES >= 58848u && BT_SEC < PJ4_RECMAX
    bad += !c[0].r || !c[4].r || !c[7].r;                /* (with Session C's two reductions: the worst case fits with the cache RAM ready, the card busy) */
#endif
    printf(bad ? "borrow arena: %d FAILED\n" : "borrow arena: all passed\n", bad);
    return bad != 0;
}
