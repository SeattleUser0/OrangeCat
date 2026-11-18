/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat Round 29 item 1 (BUILD.md 7.44): the cost of a rising block's take, a HOST reference (not in tests/run_tests.sh; not a unit number).
 * The arena of the unit's size laid out as the proof's three-OFF rows: track 4 MACRO CHIP ARP POLY, tracks 1 to 3 OFF (DRUM-X loaded, states given
 * back), the effects' memory held as the rows' load takes it (the reverb's tank 11,872 B and its model line's first block, DUB TAPE's
 * common block, state, spring tank (every running instance holds it: fx_insert.c) and four line blocks: placed directly with resource_allocate, as rising_cap_test's fx_take does, so the take's search
 * crosses the same number of live blocks). Timed on the host's clock (CLOCK_MONOTONIC):
 *   ALLOC   resource_allocate(RES_RISE0 + j, MC_STATE_BYTES) alone (the search and the 11,264 B zeroing), released and taken again, REPS times
 *   CHORD   the sixteen-note chord struck in one block (trk_note_on x 16, then that block's mix_block), TAKE: the blocks went back (a second of
 *           silence and more: three takes) against HELD: the same chord half a second after the last one ended (the blocks still kept: no take);
 *           alternated, REPS2 pairs; the medians and their difference
 * The numbers are the host's (x86, and under the suite's sanitizers when built with hc.sh: their instrumentation weighs on the zeroing). They say
 * how the take's cost splits (search, zeroing) and that it is paid once in the chord's block; the unit's figure is tools/hw/rise_proof.py --take-cost. */
#define RES_HOST_MAIN_WORDS (85760u / 4u)              /* the device's banks (tests/jfx_arena_test.c, as tests/rising_cap_test.c) */
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
#ifndef DUB_STATE_BYTES                                  /* (DUB TAPE is its module in this build: dd_t's target size, BUILD.md "DUB TAPE as the per-track insert") */
#define DUB_STATE_BYTES 2864u
#endif

#define REPS 2000u
#define REPS2 200u
static int32_t blk[CTL * 2];
static void run(uint32_t n) { while (n--) mix_block(blk, CTL); }
static uint64_t ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec; }
static int cmp64(const void *a, const void *b) { uint64_t x = *(const uint64_t *)a, y = *(const uint64_t *)b; return x < y ? -1 : x > y; }
static uint64_t median(uint64_t *v, uint32_t n) { qsort(v, n, sizeof *v, cmp64); return v[n / 2u]; }
static uint32_t mc_preset(const char *name)
{
    for (uint32_t i = 0; i < NELEM(MC_PRESETS); i++) if (!strcmp(MC_PRESETS[i].name, name)) return i;
    printf("rise_take_bench: no MACRO preset %s\n", name); exit(1);
}
static uint32_t rise_held(void) { uint32_t n = 0; for (uint32_t j = 0; j < OC_RISE_N; j++) n += resource[RES_RISE0 + j].size; return n; }
static uint64_t t_on[REPS2], t_mix[REPS2], h_on[REPS2], h_mix[REPS2], t_al[REPS];

static void strike(uint64_t *on, uint64_t *mx)
{
    uint64_t a = ns(), b, c;
    for (uint32_t i = 0; i < 16u; i++) trk_note_on(&trk[3], 48u + i, 100);
    b = ns();
    mix_block(blk, CTL);
    c = ns();
    *on = b - a; *mx = c - b;
}
static void release(void) { for (uint32_t i = 0; i < 16u; i++) trk_note_off(&trk[3], 48u + i); }

int main(void)
{
    uint32_t k, r, taken_bad = 0, held_bad = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    resource_host_cache_enabled = 0; res_lend_ok = 0;
    song.master_q12 = 4096; song.g[G_BPM] = 120; song.playing = 0; song.sel = 3;
    settings_budget = 18;
    trk_off_bits = 0;
    run(2);
    for (k = 0; k < 3u; k++) { set_engine_of(&trk[k], ENGI_DRUMX); apply_preset_to(&trk[k], 0); }
    set_engine_of(&trk[3], ENGI_MACRO); apply_preset_to(&trk[3], mc_preset("CHIP ARP")); trk[3].p[P_VOICE] = V_POLY;
    run(16);
    for (k = 0; k < 3u; k++) track_off_req(k, 1);
    run(16);
    /* the effects' memory of the rows' load (BUILD.md 7.44): placed first, as the rows let the effects take theirs before the chord */
    {
        static const struct { uint32_t id, bytes; } FX[] = {
            {JB_REV, sizeof(jrev_t)}, {JB_RX0, JFX_SEG * 2u}, {JB_INSC, 2912u}, {JB_INSS0 + 3u, 1176u}, {JB_INSP0 + 3u, 5632u},   /* (the target's sizes: BUILD.md 7.44; no chorus) */
            {JB_INSG0 + 0u, JFX_SEG * 2u}, {JB_INSG0 + 1u, JFX_SEG * 2u}, {JB_INSG0 + 2u, JFX_SEG * 2u}, {JB_INSG0 + 3u, JFX_SEG * 2u}};
        uint32_t tot = 0;
        for (k = 0; k < NELEM(FX); k++) {
            if (!resource_allocate(RES_JFX0 + FX[k].id, FX[k].bytes)) { printf("rise_take_bench: the effects' block %u (%u B) did not fit\n", FX[k].id, FX[k].bytes); return 1; }
            tot += FX[k].bytes;
        }
        printf("rise_take_bench (HOST): arena %u B, used %u B with the effects' %u B (jrev_t %u, fx_common_t %u, dd_t %u), free %u B\n",
               (unsigned)RESOURCE_CAPACITY, resource_used(), tot, (unsigned)sizeof(jrev_t), (unsigned)sizeof(fx_common_t), (unsigned)DUB_STATE_BYTES,
               (unsigned)(RESOURCE_CAPACITY - resource_used()));
    }
    /* ALLOC: one take alone, the free slot RES_RISE0 (no block held) */
    for (r = 0; r < REPS; r++) {
        uint64_t a = ns();
        void *p = resource_allocate(RES_RISE0, MC_STATE_BYTES);
        t_al[r] = ns() - a;
        if (!p) { printf("rise_take_bench: the take failed\n"); return 1; }
        resource_release(RES_RISE0);
    }
    printf("rise_take_bench (HOST): ALLOC one take (search + %u B zeroed): median %llu ns over %u\n", (unsigned)MC_STATE_BYTES, (unsigned long long)median(t_al, REPS), REPS);
    /* CHORD: TAKE (blocks gone) then HELD (blocks kept), alternated */
    for (r = 0; r < REPS2; r++) {
        run(2u * (FS / CTL));                           /* two seconds of silence: every block back */
        if (rise_held()) taken_bad++;
        strike(&t_on[r], &t_mix[r]);
        if (rise_held() != 3u * ((MC_STATE_BYTES + 3u) & ~3u)) taken_bad++;
        run(8); release(); run((FS / CTL) / 2u);       /* the voices end (CHIP ARP's release), half a second: the blocks kept */
        if (rise_held() != 3u * ((MC_STATE_BYTES + 3u) & ~3u)) held_bad++;
        strike(&h_on[r], &h_mix[r]);
        run(8); release();
    }
    {
        uint64_t a = median(t_on, REPS2), b = median(t_mix, REPS2), c = median(h_on, REPS2), d = median(h_mix, REPS2);
        printf("rise_take_bench (HOST): CHORD TAKE (3 takes): note-ons %llu ns + block %llu ns; HELD (no take): note-ons %llu ns + block %llu ns; "
               "the take's share %lld ns (medians of %u)\n", (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, (unsigned long long)d,
               (long long)((a + b) - (c + d)), REPS2);
    }
    printf("rise_take_bench (HOST): rows where the blocks were not as meant: TAKE %u, HELD %u (0 and 0: the comparison is what it says)\n", taken_bad, held_bad);
    return taken_bad || held_bad;
}
