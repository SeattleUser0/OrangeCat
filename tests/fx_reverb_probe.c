/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8a (docs/orangecat/FX_MODULES_PLAN.md 16): the reverb modes' PROBE. It renders the reverb bus through the real mixer (mix_block: four tracks, their sends, the three buses, the
 * master) over the scenarios below and prints, for each, the FNV-1a hash of every output sample (au) and of the bus's behaviour a block (st: the mode running, the model line's blocks held, the
 * tank held, the FX FULL bits). The printout of the RESIDENT modes, made BEFORE the modes' text was cut out of jfx.c into rev_kern.h, is pinned in tests/fx_reverb_golden.txt (both settings of
 * MELODEE_FX_DEFER): the cut, and every later change, must reproduce it line for line. tests/fx_reverb_module_test.c includes this file and runs the same scenarios with the module registered.
 *
 * Scenarios (four tracks: MACRO, DRUM-X, MACRO, the Prophet; notes on every track throughout; REVERB sends 100, 64, 127, 80):
 *   KNOBSm  mode m (0 ROOM, 1 SPRING, 2 SHIMMER, 3 RESO, 4 CLOUD): every knob through its stored range, one at a time, the others at their defaults: SIZE 0 .. 127 (CLOUD: FREEZE at 127),
 *           DAMP 0 .. 127 (RESO: FOLW OFF, T1 .. T4 and the values past it), PRE -24 .. 100 (PRE in ms, AIR in semitones both ways, PITCH 0 .. 14 and the values past it), MOD, RATE, FILT, WIDE;
 *           then MOD with WIDE and RATE together, the first melodic track's ROOT and SCALE (RESO's chord), the tempo
 *   CHANGE  the mode changed while it sounds: every ordered pair of the five modes, with the knobs at their defaults and away from them (the old mode's block fades out, the tank and the model
 *           line are cleared, the new mode starts from silence); a change every block; the arena short of the model line (SHIMMER and CLOUD without it: FX FULL) and given back; the buses at
 *           rest (the sends silent until the memory is given back) and woken; a project load (jfx_clear_all) in the middle of a tail
 *   HOT     the sends as loud as the mixer can make them (every track's REVERB send and level full, chords at full velocity, SIZE at its top, DAMP 0): the lines' soft knee (rv_put / rv_knee:
 *           a line write above -2.5 dBFS) on SPRING, then SHIMMER and RESO at their clamps, then ROOM and CLOUD
 *   MIXm    a full mix on mode m: four engines, chords, the three sends at 64 on every track, the DELAY long enough for its lower rates, MOD and WIDE up
 * Arguments: none. Built and run by tests/fx_reverb_module.sh (tests/run_tests.sh). */
/* the device's banks (tests/jfx_arena_test.c): the arena is as small as the unit's */
#define RES_HOST_MAIN_WORDS (85760u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint64_t h_au, h_st;
static uint32_t gb;                                     /* the block count of the scenario */
static int32_t rec[CTL * 2];
static void (*rvp_tap)(const char *scenario, uint32_t block, const int32_t *pcm, size_t bytes);   /* tests/fx_reverb_module_test.c: every output block, as the mixer made it */
static const char *rvp_name;
typedef struct { char name[16]; uint32_t blocks; uint64_t au, st; } rvp_res_t;
static rvp_res_t rvp_res[16];
static uint32_t rvp_nres;
static char rvp_only[16];                               /* RVP_ONLY in the environment: one scenario */
static int bad;
static void rvp_hash(uint64_t *h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) { *h ^= *b++; *h *= 0x100000001B3ull; }
}
static void blk(void)
{
    uint32_t v[4];
    mix_block(rec, CTL);
    if (rvp_tap) rvp_tap(rvp_name, gb, rec, sizeof rec);
    rvp_hash(&h_au, rec, sizeof rec);
    v[0] = fx.rtype; v[1] = rx_have; v[2] = jrev != 0; v[3] = jfx_full;
    rvp_hash(&h_st, v, sizeof v);
    gb++;
}
static uint8_t notes_off;                               /* the tracks silent (the buses come to rest) */
/* the notes: track k plays a note every 96 + 16 k blocks and holds it 60; track 2 a second one */
static void notes(void)
{
    uint32_t k;
    if (notes_off) return;
    for (k = 0; k < NPART; k++) {
        uint32_t per = 96u + 16u * k, ph = gb % per;
        if (ph == 4u) { trk_note_on(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 3u) * 2, 100); if (k == 2u) trk_note_on(&trk[k], 55 + (int)((gb / per) % 2u) * 4, 90); }
        if (ph == 64u) { trk_note_off(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 3u) * 2); if (k == 2u) trk_note_off(&trk[k], 55 + (int)((gb / per) % 2u) * 4); }
    }
}
static void run(uint32_t n) { while (n--) { notes(); blk(); } }
static int begin(const char *name)
{
    if (rvp_only[0] && strcmp(rvp_only, name)) return 0;
    rvp_name = name;
    h_au = h_st = 0xCBF29CE484222325ull;
    gb = 0; notes_off = 0;
    return 1;
}
static void end(const char *name)
{
    printf("%s blocks %u au %016llx st %016llx\n", name, gb, (unsigned long long)h_au, (unsigned long long)h_st);
    fflush(stdout);
    if (rvp_nres < 16u) {
        rvp_res_t *r = &rvp_res[rvp_nres++];
        snprintf(r->name, sizeof r->name, "%s", name); r->blocks = gb; r->au = h_au; r->st = h_st;
    }
}
#define T(k, id, v) (trk[k].p[id] = (int16_t)(v))
#define G(id, v) (song.g[id] = (int16_t)(v))
static const uint8_t RVP_G[7] = {G_RSIZE, G_RDAMP, G_RPRE, G_RMOD, G_RRATE, G_RFILT, G_RWIDE};
static void fresh(void)
{
    uint32_t k;
    static const uint8_t eng[4] = { ENGI_MACRO, ENGI_DRUMX, ENGI_MACRO, ENGI_PROPHET }, pre[4] = { 8, 0, 3, 0 }, snd[4] = { 100, 64, 127, 80 };
    ui_power_on();
    resource_host_cache_enabled = 1;
    res_lend_ok = 15;
    for (k = 0; k < G_COUNT; k++) song.g[k] = GP[k].def;
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    for (k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], eng[k]);
        apply_preset_to(&trk[k], pre[k]);
        trk[k].p[P_DUB_ECHO] = 0;
        trk[k].p[P_REV] = snd[k]; trk[k].p[P_DLY] = 0; trk[k].p[P_CHOR] = 0;
    }
}
static void scenario_knobs(uint32_t m)
{
    static const int16_t V_SIZE[] = {0, 1, 20, 50, 100, 101, 110, 126, 127, 90}, V_DAMP[] = {0, 1, 2, 3, 4, 5, 30, 100, 127, 60}, V_PRE[] = {-24, -12, -7, -1, 1, 5, 14, 15, 30, 60, 100, 0},
                         V_MOD[] = {1, 32, 64, 65, 100, 127, 0}, V_RATE[] = {0, 64, 100, 127, 30}, V_FILT[] = {-64, -30, 20, 63, 0}, V_WIDE[] = {1, 40, 90, 127, 0};
    static const int16_t *const V[7] = {V_SIZE, V_DAMP, V_PRE, V_MOD, V_RATE, V_FILT, V_WIDE};
    static const uint8_t NV[7] = {10, 10, 12, 7, 5, 5, 5};
    char name[16];
    uint32_t i, j;
    snprintf(name, sizeof name, "KNOBS%u", (unsigned)m);
    if (!begin(name)) return;
    fresh();
    G(G_RTYPE, m);
    run(300);
    for (i = 0; i < 7u; i++)
        for (j = 0; j < NV[i]; j++) { G(RVP_G[i], V[i][j]); run(i == 0u && V[i][j] == 127 ? 400u : 80u); }   /* (the last value of each list is the default; SIZE 127 long enough for CLOUD's line to fill and freeze) */
    for (j = 0; j < 5u; j++) { G(G_RMOD, 30 + 20 * (int)j); G(G_RWIDE, 127 - 25 * (int)j); G(G_RRATE, 127 - 30 * (int)j); G(G_RSIZE, 60 + 15 * (int)j); run(120); }
    G(G_RDAMP, m == 3u ? 2 : 80); G(G_RPRE, m == 3u ? 3 : m == 2u || m == 4u ? 7 : 25);                 /* FOLW T2 / PITCH 3; AIR up; PRE 25 ms */
    for (j = 0; j < 12u; j++) { T(0, P_ROOT, j); T(0, P_SCALE, (int)(j % 6u)); run(90); }                /* RESO's chord: the first melodic track's ROOT and SCALE */
    for (j = 0; j < 4u; j++) { G(G_BPM, 70 + 50 * (int)j); run(100); }
    notes_off = 1;
    run(500);                                           /* the tail */
    end(name);
}
static void scenario_change(void)
{
    uint32_t i, j, h, pass;
    if (!begin("CHANGE")) return;
    fresh();
    for (pass = 0; pass < 2u; pass++) {
        if (pass) { G(G_RSIZE, 110); G(G_RDAMP, 3); G(G_RPRE, 9); G(G_RMOD, 70); G(G_RRATE, 90); G(G_RFILT, -20); G(G_RWIDE, 100); }
        for (i = 0; i < 5u; i++)
            for (j = 0; j < 5u; j++) { G(G_RTYPE, i); run(45); G(G_RTYPE, j); run(45); }
    }
    for (i = 0; i < 60u; i++) { G(G_RTYPE, (i * 3u + i / 7u) % 5u); run(1); }      /* a change every block */
    for (i = 0; i < 40u; i++) { G(G_RTYPE, i % 5u); run(2 + i % 3u); }
    G(G_RTYPE, 0); run(200);
    /* the arena short: what is left taken by something else, so the model line cannot be had (SHIMMER and CLOUD: FX FULL, not run); then given back */
    for (h = 0; h < 8; h++) { uint32_t size = 80000u; while (size >= 1000u && !resource_allocate(RES_JFX0 + JB_DLY0 + h, size)) size -= 500u; }
    for (i = 1; i < 5u; i++) { G(G_RTYPE, i); run(150); }
    G(G_RTYPE, 2); run(100);
    for (h = 0; h < 8; h++) resource_release(RES_JFX0 + JB_DLY0 + h);
    run(400);
    for (i = 4; i >= 1u; i--) { G(G_RTYPE, i); run(150); }
    /* the buses at rest: nothing sent until the memory is given back; then woken on each mode */
    for (i = 1; i < 5u; i++) {
        notes_off = 1;
        run(2600);
        G(G_RTYPE, i);
        notes_off = 0;
        run(500);
    }
    /* a project load in the middle of a tail */
    for (i = 1; i < 4u; i++) { G(G_RTYPE, i); run(300); jfx_clear_all(); run(300); }
    end("CHANGE");
}
static void scenario_hot(void)
{
    static const uint8_t order[5] = { 1, 2, 3, 0, 4 };
    uint32_t k, i, j, b;
    if (!begin("HOT")) return;
    fresh();
    notes_off = 1;
    for (k = 0; k < NPART; k++) { T(k, P_REV, 127); T(k, P_LEVEL, 127); }
    G(G_RSIZE, 127); G(G_RDAMP, 0); G(G_RMOD, 20); G(G_RWIDE, 127);
    for (i = 0; i < 5u; i++) {
        G(G_RTYPE, order[i]);
        G(G_RDAMP, order[i] == 3u ? 1 : 0);
        for (j = 0; j < 6u; j++) {
            for (k = 0; k < NPART; k++) { trk_note_on(&trk[k], 36 + 12 * (int)(k % 3u) + (int)j, 127); trk_note_on(&trk[k], 43 + 12 * (int)(k % 3u) + (int)j, 127); trk_note_on(&trk[k], 48 + 12 * (int)(k % 3u) + (int)j, 127); }
            for (b = 0; b < 110u; b++) blk();
            for (k = 0; k < NPART; k++) { trk_note_off(&trk[k], 36 + 12 * (int)(k % 3u) + (int)j); trk_note_off(&trk[k], 43 + 12 * (int)(k % 3u) + (int)j); trk_note_off(&trk[k], 48 + 12 * (int)(k % 3u) + (int)j); }
            for (b = 0; b < 40u; b++) blk();
        }
    }
    end("HOT");
}
static void scenario_mix(uint32_t m)
{
    uint32_t k, ps, st, i;
    char name[16];
    static const int notes_a[4][4] = { { 36, 0, 0, 0 }, { 36, 0, 0, 0 }, { 48, 52, 55, 59 }, { 60, 64, 67, 71 } }, notes_b[4][4] = { { 36, 0, 0, 0 }, { 38, 0, 0, 0 }, { 41, 45, 48, 53 }, { 65, 69, 72, 77 } };
    snprintf(name, sizeof name, "MIX%u", (unsigned)m);
    if (!begin(name)) return;
    fresh();
    notes_off = 1;                                      /* (the chords below are the notes) */
    G(G_RTYPE, m); G(G_RMOD, 40); G(G_RWIDE, 80); G(G_RSIZE, 100);
    G(G_RDAMP, m == 3u ? 4 : 50); G(G_RPRE, m == 3u ? 2 : m == 2u || m == 4u ? -5 : 30);
    for (k = 0; k < NPART; k++) { T(k, P_REV, 64); T(k, P_DLY, 64); T(k, P_CHOR, 64); }
    for (ps = 0; ps < 6u; ps++) {
        G(G_DTIME, ps < 2u ? 20 : ps < 4u ? 50 : 63);   /* the DELAY's time: its line at the full, the half and the quarter rate beside the reverb */
        for (st = 0; st < 16u; st++) {
            uint32_t tk = st % 4u;
            const int *nt = (ps & 1u) ? notes_b[tk] : notes_a[tk];
            for (i = 0; i < 4u && nt[i]; i++) trk_note_on(&trk[tk], nt[i], 100);
            for (i = 0; i < 172u; i++) {
                if (i == 120u) for (uint32_t j = 0; j < 4u && nt[j]; j++) trk_note_off(&trk[tk], nt[j]);
                blk();
            }
        }
    }
    end(name);
}
static void rvp_scenarios(void)
{
    uint32_t m;
    rvp_nres = 0;
    for (m = 0; m < 5u; m++) scenario_knobs(m);
    scenario_change();
    scenario_hot();
    for (m = 0; m < 5u; m++) scenario_mix(m);
}
#ifndef RVP_NO_MAIN
int main(void)
{
    if (getenv("RVP_ONLY")) snprintf(rvp_only, sizeof rvp_only, "%s", getenv("RVP_ONLY"));
    printf("fx_reverb probe: MELODEE_FX_DEFER %d\n", (int)MELODEE_FX_DEFER);
    rvp_scenarios();
    if (bad) { printf("fx_reverb: %d FAILED\n", bad); return 1; }
    printf("fx_reverb: done\n");
    return 0;
}
#endif
