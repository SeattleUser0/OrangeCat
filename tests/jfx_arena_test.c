/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.0 rev 2: the effects' memory in an arena of the device's sizes. The banks are the linker's numbers (build/melodee.elf, _resource*_start
 * and _end) of the build at 0.14.0 (linux-hw 7998769, ci_step.py build): main 92,064 B, low SRAM 27,740 B, data tail 4,584 B, retained tail 816 B;
 * cache RAM 28,672 B of which the UI keeps 14,336 B; the card pool 36,864 B (0.6.1's were main 53,984, low 23,844, data 5,368, retained 4,688).
 * The real jfx.c and resources.c through the UI test's harness.
 *
 * For each case: the hot sets the boot takes first (0.14.0: engines.c eng_hot_base, the base SINE; eng_hot_take_own for MACRO and SHAPES, as
 * eng_hot_boot), then some engines' states (as the tracks take them), the cache RAM ready or not, the card pool lent or not (idle card, or frozen
 * phrases playing), then the sends set and 16 blocks run. The table says which buses got their memory. It is printed for the build document
 * (docs/orangecat/BUILD.md, the 0.6.0 measurements), and the cases the documents promise are checked:
 *   - no engine, cache ready: the delay, the reverb (ROOM) and the chorus all hold;
 *   - four of the largest engines, cache ready, the card idle: the delay, the reverb (ROOM) and the chorus all hold;
 *   - four MACRO tracks, cache ready, the card idle: the same (0.14.0: no longer with the SLICER's 32 KB too: STUT then takes the delay's
 *     memory. Measured: the base set alone or SHAPES' alone still lets it fit; MACRO's alone does not, cut to one table as well, nor the base
 *     and SHAPES' together, so trimming MACRO's list cannot bring it back (first fit at 4 KB blocks). GUIDE.md says so; the row is in the
 *     table, not checked);
 *   - DRUM FM6 MACRO SHAPES, DRUM-X MACRO SHAPES MACRO, cache ready, the card idle: the same;
 *   - whatever does not fit says FX FULL, and nothing an engine or a hot set took is touched.
 * Run by tests/run_tests.sh. The four numbers change with the build: nm build/melodee.elf, _resource{,_low,_data,_retained}_{start,end}. */
#define RES_HOST_MAIN_WORDS (92064u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4584u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
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

/* (0.14.0) the hot sets as the boot takes them (eng_hot_boot: the base set, then the built-in engines' in number order), and back */
static uint32_t ja_hot_bytes;
static void ja_hot_take(void)
{
    eng_hot_base();
    assert(dsp_hot_at[DSP_HOT_SINE] != (const void *)SINE);
    assert(eng_hot_take_own(ENGI_MACRO, ENGINES[ENGI_MACRO]) == EM_OK && eng_hs.slot[ENGI_MACRO]);
    assert(eng_hot_take_own(ENGI_SHAPES, ENGINES[ENGI_SHAPES]) == EM_OK && eng_hs.slot[ENGI_SHAPES]);
    ja_hot_bytes = resource[RES_HOT_BASE].size + resource[RES_HOT0 + eng_hs.slot[ENGI_MACRO] - 1u].size +
                   resource[RES_HOT0 + eng_hs.slot[ENGI_SHAPES] - 1u].size;
}
static void ja_hot_drop(void)
{
    eng_hot_drop(ENGI_MACRO);
    eng_hot_drop(ENGI_SHAPES);
    if (dsp_hot_at[DSP_HOT_SINE] != (const void *)SINE) { dsp_hot_at[DSP_HOT_SINE] = SINE; resource_release(RES_HOT_BASE); }
}
static int ja_hot_intact(void)                          /* every copy still equals its source */
{
    int ok = !memcmp(dsp_hot_at[DSP_HOT_SINE], SINE, sizeof SINE);
    for (uint32_t q = 0; q < 2u; q++) {
        uint32_t e = q ? ENGI_SHAPES : ENGI_MACRO, s = eng_hs.slot[e] - 1u;
        for (uint32_t i = 0; i < eng_hs.n[s]; i++)
            ok &= eng_hs.at[s][i] != eng_hs.list[s][i].src && !memcmp(eng_hs.at[s][i], eng_hs.list[s][i].src, eng_hs.list[s][i].bytes);
    }
    return ok;
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
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    ja_hot_take();                                      /* (0.14.0) the boot's hot sets, before any track's state */
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
    if (!ja_hot_intact()) bad++, printf("  a hot set was touched (case %u x %u B)\n", n, bytes);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    song.g[G_SLMODE] = 0;
    jfx_power_on();
    res_lend_ok = 0;
    res_lend_used = 0;
    resource_host_cache_enabled = 0;
    ja_hot_drop();
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
    char label[80];
    snprintf(label, sizeof label, "%s%s", what, cloud ? ", CLOUD" : "");
    what = label;
    printf("| %-34s | %-5s | %-7s | %-5s | %-5s | %-6s | %-5s |\n", what, cache ? "ready" : "off", ja_card(lend),
           ja_yn(r.dly), ja_yn(r.rev), ja_yn(r.cho), stut ? ja_yn(r.sl) : "-");
    if (out) *out = r;
}

int main(void)
{
    uint32_t big = 0, big_e = 0, macro = eng_state_size(ENGI_MACRO), k;
    ja_res_t r0, r1, r2, r3, r4, r5, r6;
    char what[64];
    for (k = 0; k < NENG_SHOWN; k++) {
        uint32_t e = eng_vis(k), b = eng_state_size(e);
        if (b > big) { big = b; big_e = e; }
    }
    ja_hot_take();
    printf("jfx arena: the device's banks; hot sets taken first: %u B (base SINE %u, MACRO %u, SHAPES %u); engines: largest %s %u B, MACRO %u B\n",
           ja_hot_bytes, resource[RES_HOT_BASE].size, resource[RES_HOT0 + eng_hs.slot[ENGI_MACRO] - 1u].size,
           resource[RES_HOT0 + eng_hs.slot[ENGI_SHAPES] - 1u].size, ENGINES[big_e]->name, big, macro);
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    printf("| %-34s | %-5s | %-7s | %-5s | %-5s | %-6s | %-5s |\n", "engines", "cache", "card", "DELAY", "REV",
           "CHORUS", "STUT");
    printf("|------------------------------------|-------|---------|-------|-------|--------|-------|\n");
    ja_row("none", 0, 0, 1, 0, 0, 0, &r0);
    ja_row("none", 0, 0, 0, 0, 0, 0, 0);
    ja_row("none", 0, 0, 1, 15, 0, 1, 0);
    ja_row("none", 0, 0, 1, 0, 1, 0, 0);
    ja_row("none", 0, 0, 1, 15, 1, 0, 0);
    ja_row("none", 0, 0, 1, 15, 1, 1, 0);
    snprintf(what, sizeof what, "4 x largest (%u B)", big);
    ja_row(what, 4, big, 1, 15, 0, 0, &r1);
    ja_row(what, 4, big, 1, 14, 0, 0, 0);
    ja_row(what, 4, big, 1, 3, 0, 0, 0);
    ja_row(what, 4, big, 1, 0, 0, 0, 0);
    ja_row(what, 4, big, 0, 15, 0, 0, 0);
    ja_row(what, 4, big, 1, 15, 0, 1, 0);
    snprintf(what, sizeof what, "4 x MACRO (%u B)", macro);
    ja_row(what, 4, macro, 1, 15, 0, 0, &r2);
    ja_row(what, 4, macro, 1, 15, 0, 1, &r3);
    ja_row(what, 4, macro, 1, 0, 0, 0, 0);
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
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 15, 0, 0, &r4);
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 14, 0, 0, 0);
    ja_row("DRUM FM6 MACRO SHAPES", 4, 0, 1, 0, 0, 0, 0);
    ja_sizes[0] = ja_size_of("DRUM-X");
    ja_sizes[1] = ja_size_of("MACRO");
    ja_sizes[2] = ja_size_of("SHAPES");
    ja_sizes[3] = ja_size_of("MACRO");
    ja_row("DRUM-X MACRO SHAPES MACRO", 4, 0, 1, 15, 0, 0, &r5);
    ja_row("DRUM-X MACRO SHAPES MACRO", 4, 0, 1, 15, 0, 1, &r6);
    ja_row("DRUM-X MACRO SHAPES MACRO", 4, 0, 1, 0, 0, 0, 0);
    ja_row("DRUM-X MACRO SHAPES MACRO", 4, 0, 1, 15, 1, 0, 0);
    bad += !(r0.dly && r0.rev && r0.cho);
    bad += !(r1.dly && r1.rev && r1.cho);
    bad += !(r2.dly && r2.rev && r2.cho);
    bad += !(r4.dly && r4.rev && r4.cho);
    bad += !(r5.dly && r5.rev && r5.cho);
    printf("%-60s %s\n", "jfx arena: no engine (cache ready): DELAY, REVERB, CHORUS hold", r0.dly && r0.rev && r0.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: 4 x largest, card idle: DELAY, REVERB, CHORUS hold", r1.dly && r1.rev && r1.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: 4 x MACRO, card idle: DELAY, REVERB, CHORUS hold", r2.dly && r2.rev && r2.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: 4 x MACRO, card idle, STUT (table only): DELAY", r3.dly ? "yes" : "FULL");
    printf("%-60s %s\n", "jfx arena: DRUM FM6 MACRO SHAPES, card idle: DELAY, REVERB, CHORUS hold", r4.dly && r4.rev && r4.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: DRUM-X MACRO SHAPES MACRO, card idle: DELAY, REVERB, CHORUS hold", r5.dly && r5.rev && r5.cho ? "ok" : "FAIL");
    printf("%-60s %s\n", "jfx arena: DRUM-X MACRO SHAPES MACRO, card idle, STUT (table only)", r6.sl ? "yes" : "FULL");
    printf(bad ? "jfx arena: %d FAILED\n" : "jfx arena: all passed\n", bad);
    return bad ? 1 : 0;
}
