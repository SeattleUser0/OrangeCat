/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.0 rev 2: the effects' memory in an arena of the device's sizes (the linker's numbers from the CI build
 * of 0.6.0: main 54,272 B, low SRAM 23,848 B, data tail 7,416 B, retained tail 4,688 B, cache RAM 28,672 B of which the
 * UI keeps 14,336 B; the card pool 36,864 B). The real jfx.c and resources.c through the UI test's harness.
 *
 * For each case: some engines' states taken first (as the tracks take them), the cache RAM ready or not, the card pool
 * lent or not (idle card, or frozen phrases playing), then the sends set and 16 blocks run. The table says which buses
 * got their memory. It is printed for the build document (docs/orangecat/BUILD.md, the 0.6.0 measurements), and the
 * cases the documents promise are checked:
 *   - no engine, cache ready: the delay, the reverb (ROOM) and the chorus all hold;
 *   - four of the largest engines, cache ready, the card idle: the delay, the reverb (ROOM) and the chorus all hold;
 *   - four MACRO tracks, cache ready, the card idle: the same, and the SLICER's 32 KB too;
 *   - whatever does not fit says FX FULL, and nothing an engine took is touched.
 * Run by tests/run_tests.sh. */
#define RES_HOST_MAIN_WORDS (54272u / 4u)
#define RES_HOST_LOW_WORDS (23848u / 4u)
#define RES_HOST_DATA_WORDS (7416u / 4u)
#define RES_HOST_RETAINED_WORDS (4688u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

static int bad;
static int32_t ja_out[2u * CTL];

static void ja_block(void)
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
    jfx_master(ja_out, CTL, perf);
}

typedef struct { int cho, dly, rev, sl; uint32_t full; } ja_res_t;
static uint32_t ja_sizes[NPART];                        /* (bytes 0: engine k takes ja_sizes[k]) */
static uint32_t ja_size_of(const char *name)            /* an engine's state, by its name */
{
    for (uint32_t k = 0; k < NENG_SHOWN; k++)
        if (!strcmp(ENGINES[eng_vis(k)]->name, name)) return eng_state_size(eng_vis(k));
    assert(0);
    return 0;
}

/* engines: n engines of `bytes` each; cache: the cache RAM ready; lend: the card slots lent (bit s: slot s not in use
 * by the card); cloud: the reverb on CLOUD; stut: the SLICER on STUT (its 32 KB) */
static ja_res_t ja_case(uint32_t n, uint32_t bytes, int cache, int lend, int cloud, int stut)
{
    ja_res_t r;
    uint32_t k;
    uint8_t keep[NPART][16];
    ui_power_on();
    song.playing = 0;
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    resource_host_cache_enabled = (uint32_t)cache;
    res_lend_ok = 0;
    res_lend_used = 0;
    for (k = 0; k < n; k++) {
        uint32_t b = bytes ? bytes : ja_sizes[k];
        uint8_t *p = resource_get(RES_ENGINE0 + k, b);
        assert(p);
        memset(p, 0xA5, b);
        memcpy(keep[k], p, sizeof keep[k]);
    }
    res_lend_ok = (uint8_t)lend;                        /* (after the engines: the card's say comes from the main loop) */
    song.g[G_RTYPE] = cloud ? 4 : 0;
    song.g[G_SLMODE] = stut ? SL_STUT : 0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 100;
    for (k = 0; k < 16u; k++) ja_block();
    r.cho = cho_buf != 0;
    r.dly = dly_have == DLY_NSEG;
    r.rev = jrev != 0 && rev_can_run((uint32_t)song.g[G_RTYPE]);
    r.sl = sl_buf != 0;
    r.full = jfx_full;
    for (k = 0; k < n; k++) {                           /* (the engines' memory untouched) */
        const uint8_t *p = resource[RES_ENGINE0 + k].ptr;
        uint32_t b = bytes ? bytes : ja_sizes[k];
        if (resource[RES_ENGINE0 + k].size < b || memcmp(p, keep[k], sizeof keep[k]) || p[b - 1u] != 0xA5u)
            bad++, printf("  an engine's memory was touched (case %u x %u B)\n", n, bytes);
    }
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    song.g[G_SLMODE] = 0;
    jfx_power_on();
    res_lend_ok = 0;
    res_lend_used = 0;
    resource_host_cache_enabled = 0;
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    return r;
}
static const char *ja_yn(int v) { return v ? "yes" : "FULL"; }
static const char *ja_card(int lend)
{
    return lend == 15 ? "idle" : lend == 14 ? "1 take" : lend == 3 ? "freeze" : lend == 0 ? "4 takes" : "?";
}
static void ja_row(const char *what, uint32_t n, uint32_t bytes, int cache, int lend, int cloud, int stut, ja_res_t *out)
{
    ja_res_t r = ja_case(n, bytes, cache, lend, cloud, stut);
    printf("| %-34s | %-5s | %-7s | %-5s | %-5s | %-6s | %-5s |\n", what, cache ? "ready" : "off", ja_card(lend),
           ja_yn(r.dly), ja_yn(r.rev), ja_yn(r.cho), stut ? ja_yn(r.sl) : "-");
    if (out) *out = r;
}

int main(void)
{
    uint32_t big = 0, big_e = 0, macro = eng_state_size(ENGI_MACRO), k;
    ja_res_t r0, r1, r2, r3;
    char what[64];
    for (k = 0; k < NENG_SHOWN; k++) {
        uint32_t e = eng_vis(k), b = eng_state_size(e);
        if (b > big) { big = b; big_e = e; }
    }
    printf("jfx arena: the device's banks; engines: largest %s %u B, MACRO %u B\n", ENGINES[big_e]->name, big, macro);
    printf("| %-34s | %-5s | %-7s | %-5s | %-5s | %-6s | %-5s |\n", "engines", "cache", "card", "DELAY", "REV",
           "CHORUS", "STUT");
    printf("|------------------------------------|-------|---------|-------|-------|--------|-------|\n");
    ja_row("none", 0, 0, 1, 0, 0, 0, &r0);
    ja_row("none", 0, 0, 0, 0, 0, 0, 0);
    ja_row("none", 0, 0, 1, 15, 0, 1, 0);
    ja_row("none, CLOUD", 0, 0, 1, 0, 1, 0, 0);
    ja_row("none, CLOUD", 0, 0, 1, 15, 1, 0, 0);
    ja_row("none, CLOUD", 0, 0, 1, 15, 1, 1, 0);
    snprintf(what, sizeof what, "4 x largest (%u B)", big);
    ja_row(what, 4, big, 1, 15, 0, 0, &r1);
    ja_row(what, 4, big, 1, 14, 0, 0, 0);
    ja_row(what, 4, big, 1, 3, 0, 0, 0);
    ja_row(what, 4, big, 1, 0, 0, 0, 0);
    ja_row(what, 4, big, 0, 15, 0, 0, 0);
    ja_row(what, 4, big, 1, 15, 0, 1, 0);
    snprintf(what, sizeof what, "4 x MACRO (%u B)", macro);
    ja_row(what, 4, macro, 1, 15, 0, 0, &r2);
    ja_row(what, 4, macro, 1, 0, 0, 0, &r3);
    snprintf(what, sizeof what, "2 x largest (%u B)", big);
    ja_row(what, 2, big, 1, 15, 0, 0, 0);
    ja_row(what, 2, big, 1, 14, 0, 0, 0);
    ja_row(what, 2, big, 1, 0, 0, 0, 0);
    ja_row(what, 2, big, 1, 15, 1, 0, 0);
    ja_sizes[0] = ja_size_of("DRUM");
    ja_sizes[1] = ja_size_of("FM6");
    ja_sizes[2] = ja_size_of("SID");
    ja_sizes[3] = ja_size_of("CZ-1");
    ja_row("DRUM FM6 SID CZ-1", 4, 0, 1, 15, 0, 0, 0);
    ja_row("DRUM FM6 SID CZ-1", 4, 0, 1, 14, 0, 0, 0);
    ja_row("DRUM FM6 SID CZ-1", 4, 0, 1, 0, 0, 0, 0);
    ja_row("DRUM FM6 SID CZ-1", 4, 0, 1, 15, 1, 0, 0);
    ja_sizes[2] = ja_size_of("MACRO");
    ja_sizes[3] = ja_size_of("SHAPES");
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 15, 0, 0, 0);
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 14, 0, 0, 0);
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 0, 0, 0, 0);
    bad += !(r0.dly && r0.rev && r0.cho);
    bad += !(r1.dly && r1.rev && r1.cho);
    bad += !(r2.dly && r2.rev && r2.cho);
    printf("%-60s %s\n", "jfx arena: no engine (cache ready): DELAY, REVERB, CHORUS hold", r0.dly && r0.rev && r0.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: 4 x largest, card idle: DELAY, REVERB, CHORUS hold", r1.dly && r1.rev && r1.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: 4 x MACRO, card idle: DELAY, REVERB, CHORUS hold", r2.dly && r2.rev && r2.cho ? "ok" : "FAIL");
    printf(bad ? "jfx arena: %d FAILED\n" : "jfx arena: all passed\n", bad);
    return bad ? 1 : 0;
}
