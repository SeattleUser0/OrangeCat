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
#ifndef JA_MAIN_BYTES
#define JA_MAIN_BYTES 86368u                            /* (Release 2) the stage 2 build's _resource_end - _resource_start (nm build/melodee.elf, 1f72487) */
#endif
#define RES_HOST_MAIN_WORDS (JA_MAIN_BYTES / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4520u / 4u)
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

/* (Release 2, design 3) DUB TAPE's lines: n_eng MACRO-sized engines (VA BASS is MACRO's state), ROOM on or off, the card idle (lend 15) or busy (0), the cache
 * ready; every instance wants `segs` blocks (4 KB each; 0 none) with or without the spring. After 600 blocks: how many instances hold all their memory. */
static uint32_t ja_dub_case(uint32_t n_eng, uint32_t bytes, int room, int lend, uint32_t segs, int spring, uint32_t *peak)
{
    uint32_t k, ready = 0, best = 0;
    ui_power_on();
    song.playing = 0;
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    ja_hot_take();
    resource_host_cache_enabled = 1;
    res_lend_ok = 0;
    res_lend_used = 0;
    for (k = 0; k < n_eng; k++) {
        uint8_t *p = resource_get(RES_ENGINE0 + k, bytes);
        assert(p);
        memset(p, 0xA5, bytes);
    }
    res_lend_ok = (uint8_t)lend;
    song.g[G_RTYPE] = 0;
    song.g[G_SLMODE] = 0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = 0;
    trk[0].p[P_REV] = room ? 100 : 0;
    for (k = 0; k < DUB_N; k++) { dub_want[k] = (uint8_t)segs; dub_spring[k] = (uint8_t)spring; }
    for (uint32_t b = 0; b < 600u; b++) {
        ja_block();
        ready = 0;
        for (k = 0; k < DUB_N; k++) ready += dub_ready[k];
        if (ready > best) best = ready;
    }
    if (peak) *peak = best;
    trk[0].p[P_REV] = 0;
    for (k = 0; k < DUB_N; k++) dub_want[k] = dub_spring[k] = 0;
    jfx_power_on();
    res_lend_ok = 0;
    res_lend_used = 0;
    resource_host_cache_enabled = 0;
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    return ready;
}

/* the line blocks (4 KB) that still fit once the four DUB states are in: the engines, ROOM on or off, the card idle or busy */
static uint32_t ja_dub_blocks(uint32_t n_eng, uint32_t bytes, int room, int lend)
{
    uint32_t k, n = 0;
    ui_power_on();
    song.playing = 0;
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    ja_hot_take();
    resource_host_cache_enabled = 1;
    res_lend_ok = 0;
    res_lend_used = 0;
    for (k = 0; k < n_eng; k++) assert(resource_get(RES_ENGINE0 + k, bytes));
    res_lend_ok = (uint8_t)lend;
    song.g[G_RTYPE] = 0;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = 0;
    trk[0].p[P_REV] = room ? 100 : 0;
    for (k = 0; k < 16u; k++) ja_block();
    for (k = 0; k < DUB_N; k++) assert(resource_allocate(RES_JFX0 + JB_DUBS0 + k, DUB_STATE_BYTES));
    for (k = 0; k < DUB_NSEG; k++) n += resource_allocate(RES_JFX0 + JB_DUBG0 + k, JFX_SEG * sizeof(int16_t)) != 0;
    trk[0].p[P_REV] = 0;
    jfx_power_on();
    res_lend_ok = 0;
    res_lend_used = 0;
    resource_host_cache_enabled = 0;
    ja_hot_drop();
    for (k = 0; k < RES_COUNT; k++) resource_release(k);
    return n;
}
static void ja_dub_table(const char *what, uint32_t n_eng, uint32_t bytes, int room)
{
    static const uint32_t segs[] = { 2, 4, 5, 6, 7, 8, 12, 16 };
    printf("\n%s%s: instances that hold their memory (of 4), with the best seen in brackets when they flap\n", what, room ? " + ROOM" : "");
    printf("blocks of 4 KB left after the four states (of the pool's %u): card idle %u, card busy %u\n", DUB_NSEG, ja_dub_blocks(n_eng, bytes, room, 15), ja_dub_blocks(n_eng, bytes, room, 0));
    printf("| LEN (blocks, bytes) | card idle | card busy | card idle + spring | card busy + spring |\n|---|---|---|---|---|\n");
    for (uint32_t q = 0; q < sizeof segs / sizeof segs[0]; q++) {
        uint32_t cell[4], pk[4], c = 0;
        for (int sp = 0; sp < 2; sp++)
            for (int busy = 0; busy < 2; busy++, c++)
                cell[c] = ja_dub_case(n_eng, bytes, room, busy ? 0 : 15, segs[q], sp, &pk[c]);
        printf("| %2u, %2uK |", segs[q], segs[q] * 4u);
        for (c = 0; c < 4; c++) {
            if (pk[c] != cell[c]) printf(" %u (%u) |", cell[c], pk[c]);
            else printf(" %u |", cell[c]);
        }
        printf("\n");
    }
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
    ja_dub_table("4 x MACRO (VA BASS)", 4, macro, 1);
    ja_dub_table("4 x MACRO (VA BASS)", 4, macro, 0);
    ja_dub_table("no engine", 0, 0, 1);
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
