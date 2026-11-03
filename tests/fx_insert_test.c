/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 5 (docs/orangecat/FX_MODULES_PLAN.md 14): the generic insert host with DUB TAPE's kernel across the cut, still in the image. THE GATE: the output and the
 * behaviour are the ones the tree before the cut (09d4d4b3, jfx_dub.c whole) gave, sample for sample.
 *
 * This file is the PROBE. It runs the scenarios below through the real mixer (mix_block: the tracks, the capture, the job, the wet, the sends, the master) and prints, for each, the
 * FNV-1a hash of every output sample (au), the hash of the behaviour (st: every block, each instance's state, rate divisor, hold, run, memory held and ready, the governor's
 * wait and idle counters, the DUB FULL bits, and the bytes of the parameter snapshots the job reads) and the governor's firing count. tests/fx_insert_diff.sh builds it twice,
 * against the extracted tree of 09d4d4b3 (OLD) and against this tree (NEW), and compares the two printouts line for line, and the NEW one against tests/fx_insert_golden.txt (the
 * OLD printout, pinned, for a checkout without the history). Only names that both trees have are used; NEW adds the checks of the descriptor under FXI_NEW.
 *
 * Scenarios (four tracks, notes on every track throughout, a normal half fed to the governor every fourth block):
 *   LONG   one timeline, about 60 s of audio, in phases:
 *          1 four instances up at once (TIME L/R synced, a different pair a track; FEEDBACK, SEND, FILTER a track)
 *          2 the DUB page: FEEDBACK sweep, SEND sweep, FILTER sweep (low-pass half and high-pass half), ECHO 100 -> 60 -> 127 -> 100
 *          3 the DUB SETUP page: TIME L and TIME R through all 18 synced divisions, the tempo 120 -> 90 -> 160 -> 120, SYNC off with free times 0 .. 127, SYNC on again, PING on/off
 *          4 the DUB 2 page: SPRING 0 -> 127 -> 0, SPEED 0 .. 127 (slow side: the farthest tap grows), DRIVE 0 .. 15, WOW 0 .. 11
 *          5 a rate-stage change: a track from a short synced time to 6 beats (the quarter rate after the debounce, the line cleared a block a block), back, then ECHO 0 past the idle time so the stage resets
 *          6 size changes: TIME 1/8 -> 1/2 -> 1/1 -> 1/8 (grow, sticky), a free TIME of 1.4 s at the slow SPEED (the 16 blocks)
 *          7 the triggers: THRW pulses (replaces SEND), CUT held (muted, line kept), KICK edges (the spring's tank kicked)
 *          8 ECHO to 0 and back inside the idle time (muted, the memory kept) and after it (memory given back, taken again)
 *          9 the governor: late halves evict the highest running track (ramped, DUB FULL), the 8 s wait with quiet, the return; a second eviction inside 47 s (the wait doubles);
 *            a setting change that asks again; dub_force_degrade (CPU1 retired) and its 2 s; a heavy half in the window (no headroom)
 *   MEM    the arena as the unit has it: hogged until DUB FULL, released; a bus (REVERB) asking while four instances hold their memory (they give way); a project load (jfx_clear_all)
 *   R2     Release 2 projects: the old layout (LEN, PTRN, SPRP, SPACE, TONE; 126 combinations in turn, four at a time) packed with the project writer and loaded with the reader
 *          (dub_convert_old), the ten values then played through the legacy slots
 *   HEAVY  the sequencer test's heavy cell as the host can play it: four engines, chords, three sends at 64, DUB setting 1 (the old test's) and 2 (PING, WOW 3, DRIVE 8, SPRING 28),
 *          a step every 172 blocks, 16 passes of 16 steps, with the governor fed a normal half and, every 2,500th block once the instances run, a half at 91 %
 * Built and run by tests/fx_insert_diff.sh (tests/run_tests.sh). */
/* the device's banks (tests/jfx_arena_test.c): the arena is as small as the unit's */
#define RES_HOST_MAIN_WORDS (85760u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#define DUB_IDLE 700u                                  /* (blocks an ECHO 0 instance stays: 8 s on the unit, 0.5 s here) */
#include "ui_test.c"
#include <assert.h>
#include <math.h>

static uint64_t h_au, h_st;
static uint32_t st_n[5], full_n, snap_n;                /* coverage: instance-blocks in each state, blocks with a DUB FULL bit, blocks with a snapshot in the common block */
static uint32_t gb;                                    /* the block count of the scenario */
static int fxi_progress;                               /* FXI_PROGRESS in the environment: the block count on stderr every 10,000 blocks */
static int32_t rec[CTL * 2];
static void (*fxi_tap)(const int32_t *block, size_t bytes);   /* tests/fx_dub_module_test.c: every output block, as the mixer made it */
typedef struct { char name[16]; uint32_t blocks, gov, states[5], full, cm; uint64_t au, st; } fxi_res_t;   /* what end() printed, for a caller that compares runs */
static fxi_res_t fxi_res[8];
static uint32_t fxi_nres;
static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static void fxi_hash(uint64_t *h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) { *h ^= *b++; *h *= 0x100000001B3ull; }
}
static void observe(void)
{
    uint32_t k;
    for (k = 0; k < 4; k++) {
        uint32_t v[9] = { dub_i[k].state, dub_i[k].R, dub_i[k].hold, dub_i[k].run, dub_have[k], dub_ready[k], dub_i[k].gov_wait, dub_i[k].idle, dub_i[k].ramp };
        fxi_hash(&h_st, v, sizeof v);
        if (v[0] < 5u) st_n[v[0]]++;
    }
    if (jfx_full & 0xF0u) full_n++;
    if (dub_cm) snap_n++;
    {   uint32_t f = jfx_full; fxi_hash(&h_st, &f, sizeof f); }
    if (dub_cm) fxi_hash(&h_st, dub_cm->p, sizeof dub_cm->p);
}
static void blk(void)
{
    mix_block(rec, CTL);
    if (fxi_tap) fxi_tap(rec, sizeof rec);
    fxi_hash(&h_au, rec, sizeof rec);
    observe();
    gb++;
    if (fxi_progress && !(gb % 10000u)) { fprintf(stderr, "block %u\n", gb); }
}
/* the notes: track k plays a note every 96 + 16 k blocks and holds it 60 */
static void notes(void)
{
    uint32_t k;
    for (k = 0; k < NPART; k++) {
        uint32_t per = 96u + 16u * k, ph = gb % per;
        if (ph == 4u) { trk_note_on(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 3u) * 2, 100); if (k == 2u) trk_note_on(&trk[k], 55 + (int)((gb / per) % 2u) * 4, 90); }
        if (ph == 64u) { trk_note_off(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 3u) * 2); if (k == 2u) trk_note_off(&trk[k], 55 + (int)((gb / per) % 2u) * 4); }
    }
}
static void run(uint32_t n)                             /* n blocks, a normal half to the governor every fourth */
{
    while (n--) {
        notes();
        if (!(gb & 3u)) dub_late_half(0, 1200u);
        blk();
    }
}
static void quiet_halves(uint32_t n)                    /* n blocks, a normal half every block (the quiet the governor wants) */
{
    while (n--) { notes(); dub_late_half(0, 1200u); blk(); }
}
static void begin(const char *name)
{
    printf("scenario %s\n", name);
    h_au = h_st = 0xCBF29CE484222325ull;
    gb = 0;
    memset(st_n, 0, sizeof st_n); full_n = snap_n = 0;
}
static void end(const char *name)
{
    printf("%s blocks %u au %016llx st %016llx gov %u states %u %u %u %u %u full %u cm %u\n", name, gb, (unsigned long long)h_au, (unsigned long long)h_st, (unsigned)dub_gov_fires,
           st_n[0], st_n[1], st_n[2], st_n[3], st_n[4], full_n, snap_n);
    fflush(stdout);
    if (fxi_nres < 8u) {
        fxi_res_t *r = &fxi_res[fxi_nres++];
        snprintf(r->name, sizeof r->name, "%s", name); r->blocks = gb; r->gov = (uint32_t)dub_gov_fires; r->au = h_au; r->st = h_st; r->full = full_n; r->cm = snap_n;
        memcpy(r->states, st_n, sizeof r->states);
    }
}
static void fresh(void)
{
    uint32_t k;
    static const uint8_t eng[4] = { ENGI_MACRO, ENGI_DRUMX, ENGI_MACRO, ENGI_PROPHET }, pre[4] = { 8, 0, 3, 0 };
    ui_power_on();
    resource_host_cache_enabled = 1;
    res_lend_ok = 15;
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    for (k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], eng[k]);
        apply_preset_to(&trk[k], pre[k]);
        trk[k].p[P_DUB_ECHO] = 0;
    }
}
#define T(k, id, v) (trk[k].p[id] = (int16_t)(v))
static void dub_defaults(uint32_t k)
{
    T(k, P_DUB_ECHO, 100); T(k, P_DUB_MODE, DUB_MODE_MARK | 1); T(k, P_DUB_TAPE, 6 * DUB_WOW_N - 64); T(k, P_DUB_SPD, 64); T(k, P_DUB_SPRG, 0);
    T(k, P_DUB_FDBK, 55); T(k, P_DUB_SEND, 127); T(k, P_DUB_FILT, 60);
    T(k, P_DUB_TIML, dub_t_val(7)); T(k, P_DUB_TIMR, dub_t_val(9));
    memset(dub_trig, 0, sizeof dub_trig);
}

static void scenario_long(void)
{
    uint32_t k, i;
    begin("LONG");
    fresh();
    for (k = 0; k < NPART; k++) dub_defaults(k);
    /* 1: four instances at once */
    static const int tl[4] = { 7, 6, 9, 2 }, tr[4] = { 9, 6, 12, 6 };
    for (k = 0; k < NPART; k++) {
        T(k, P_DUB_TIML, dub_t_val(tl[k])); T(k, P_DUB_TIMR, dub_t_val(tr[k]));
        T(k, P_DUB_FDBK, 40 + 12 * (int)k); T(k, P_DUB_SEND, 127 - 20 * (int)k); T(k, P_DUB_FILT, (k & 1u) ? 20 + 30 * (int)k : 64);
    }
    run(2400);
    /* 2: the DUB page on track 0 */
    for (i = 0; i <= 127; i += 9) { T(0, P_DUB_FDBK, i); run(24); }
    for (i = 0; i <= 127; i += 9) { T(0, P_DUB_SEND, 127 - (int)i); run(24); }
    T(0, P_DUB_SEND, 100);
    for (i = 0; i <= 127; i += 5) { T(0, P_DUB_FILT, i); run(16); }
    T(0, P_DUB_FILT, 64);
    T(0, P_DUB_ECHO, 60); run(300); T(0, P_DUB_ECHO, 127); run(300); T(0, P_DUB_ECHO, 100); run(300);
    /* 3: the DUB SETUP page on track 1 and the tempo */
    for (i = 0; i < 18; i++) { T(1, P_DUB_TIML, dub_t_val((int)i)); T(1, P_DUB_TIMR, dub_t_val((int)((i * 5 + 3) % 18))); run(150); }
    T(1, P_DUB_TIML, dub_t_val(6)); T(1, P_DUB_TIMR, dub_t_val(6));
    song.g[G_BPM] = 90; run(500); song.g[G_BPM] = 160; run(500); song.g[G_BPM] = 120; run(500);
    for (k = 0; k < NPART; k++) dub_put_field(trk[k].p, DUBF_SYNC, 0);             /* free times */
    for (i = 0; i <= 112; i += 16) { T(0, P_DUB_TIML, i); T(0, P_DUB_TIMR, 127 - (int)i); T(2, P_DUB_TIML, 30 + (int)i / 2); T(2, P_DUB_TIMR, (int)i); run(200); }
    for (k = 0; k < NPART; k++) { T(k, P_DUB_TIML, 50 + 10 * (int)k); T(k, P_DUB_TIMR, 40 + 14 * (int)k); }
    run(300);
    for (k = 0; k < NPART; k++) { dub_put_field(trk[k].p, DUBF_SYNC, 1); T(k, P_DUB_TIML, dub_t_val(7)); T(k, P_DUB_TIMR, dub_t_val(9)); }
    run(300);
    for (i = 0; i < 6; i++) { dub_put_field(trk[3].p, DUBF_PING, (int)(i & 1u)); run(200); }
    for (i = 0; i < 6; i++) { dub_put_field(trk[1].p, DUBF_SYNC, (int)(i & 1u)); run(200); }
    dub_put_field(trk[1].p, DUBF_SYNC, 1);
    /* 4: the DUB 2 page */
    for (i = 0; i <= 127; i += 8) { T(2, P_DUB_SPRG, i); run(30); }
    for (i = 127; i > 16; i -= 16) { T(2, P_DUB_SPRG, i); run(30); }
    T(2, P_DUB_SPRG, 90); run(400);
    for (i = 0; i <= 127; i += 8) { T(3, P_DUB_SPD, i); run(30); }
    for (i = 127; i > 60; i -= 12) { T(3, P_DUB_SPD, i); run(30); }
    T(3, P_DUB_SPD, 64); run(300);
    for (i = 0; i < 16; i++) { dub_put_field(trk[1].p, DUBF_DRIVE, (int)i); run(40); }
    for (i = 0; i < 12; i++) { dub_put_field(trk[1].p, DUBF_WOW, (int)i); run(60); }
    dub_put_field(trk[1].p, DUBF_WOW, 0); dub_put_field(trk[1].p, DUBF_DRIVE, 6);
    /* 5: the rate stage: a long synced time (6 beats = 3 s) on track 0 */
    T(0, P_DUB_TIML, dub_t_val(17)); T(0, P_DUB_TIMR, dub_t_val(15));
    run(1500);                                          /* the debounce (344 blocks), the ramp, the clear, the quarter rate */
    T(0, P_DUB_TIML, dub_t_val(7)); T(0, P_DUB_TIMR, dub_t_val(9)); run(900);       /* back: the stage is sticky */
    T(0, P_DUB_ECHO, 0); run(1100); T(0, P_DUB_ECHO, 100); run(800);                 /* past the idle time: the stage resets and the line starts half rate again */
    /* 6: size changes */
    T(1, P_DUB_TIML, dub_t_val(3)); T(1, P_DUB_TIMR, dub_t_val(3)); run(600);
    T(1, P_DUB_TIML, dub_t_val(12)); T(1, P_DUB_TIMR, dub_t_val(12)); run(900);
    T(1, P_DUB_TIML, dub_t_val(15)); T(1, P_DUB_TIMR, dub_t_val(15)); run(1500);
    T(1, P_DUB_TIML, dub_t_val(6)); T(1, P_DUB_TIMR, dub_t_val(6)); run(600);
    dub_put_field(trk[3].p, DUBF_SYNC, 0); T(3, P_DUB_TIML, 127); T(3, P_DUB_TIMR, 120); T(3, P_DUB_SPD, 0); run(2000);
    T(3, P_DUB_SPD, 64); dub_put_field(trk[3].p, DUBF_SYNC, 1); T(3, P_DUB_TIML, dub_t_val(2)); T(3, P_DUB_TIMR, dub_t_val(6)); run(700);
    /* 7: the triggers */
    for (k = 0; k < NPART; k++) {
        for (i = 0; i < 4; i++) { dub_trig[k][0] = 127; run(20); dub_trig[k][0] = 0; run(80); }                 /* THRW */
        dub_trig[k][1] = 127; run(120); dub_trig[k][1] = 0; run(160);                                              /* CUT */
        for (i = 0; i < 5; i++) { dub_trig[k][2] = 127; run(15); dub_trig[k][2] = 0; run(45); }                 /* KICK */
    }
    /* 8: ECHO 0 and back */
    for (k = 0; k < NPART; k++) T(k, P_DUB_ECHO, 0);
    run(300);
    for (k = 0; k < NPART; k++) T(k, P_DUB_ECHO, 100);
    run(500);
    for (k = 0; k < NPART; k++) T(k, P_DUB_ECHO, (k & 1u) ? 0 : 100);
    run(1600);
    for (k = 0; k < NPART; k++) T(k, P_DUB_ECHO, 100);
    run(900);
    /* 9: the governor */
    for (i = 0; i < 6; i++) { dub_late_half(1, 3000u); dub_late_half(1, 3000u); notes(); blk(); }          /* a late score: the highest running track gives way */
    quiet_halves(1800);
    for (i = 0; i < 3; i++) { dub_late_half(1, 3000u); notes(); blk(); }                                    /* late halves while it is out */
    quiet_halves(11500);                                                                                    /* the 8 s, 2 s quiet, headroom: it asks again */
    run(900);
    for (i = 0; i < 6; i++) { dub_late_half(1, 3000u); dub_late_half(1, 3000u); notes(); blk(); }          /* a second eviction inside 47 s: the wait doubles */
    quiet_halves(1500);
    T(3, P_DUB_TIML, dub_t_val(4)); quiet_halves(30);                                                      /* a setting change asks again at once */
    quiet_halves(1500);
    for (i = 0; i < 6; i++) { dub_late_half(1, 3000u); dub_late_half(1, 3000u); notes(); blk(); }          /* a third */
    quiet_halves(22500);                                                                                    /* 16 s */
    run(1200);
    dub_force_degrade = 1; run(400);                                                                         /* CPU1 retired: released, DUB FULL */
    dub_force_degrade = 0; run(3300);                                                                        /* 2 s of CPU1 online, then admitted */
    dub_late_half(0, 2400u); run(100);                                                                       /* 83 %: nothing */
    dub_late_half(0, 2500u); run(60);                                                                        /* over the line: one gives way */
    quiet_halves(6000);
    end("LONG");
}

static void scenario_mem(void)
{
    uint32_t k, h, b;
    begin("MEM");
    fresh();
    for (k = 0; k < NPART; k++) { dub_defaults(k); T(k, P_DUB_TIML, dub_t_val(9)); T(k, P_DUB_TIMR, dub_t_val(9)); }
    run(400);
    for (h = 0; h < 8; h++) { uint32_t size = 80000u; while (size >= 1000u && !resource_allocate(RES_JFX0 + JB_DLY0 + h, size)) size -= 500u; }   /* what is left taken by something else */
    T(0, P_REV, 100);
    run(2500);                                          /* the reverb asks: the instances give way, DUB FULL shows */
    T(0, P_REV, 0);
    for (h = 0; h < 8; h++) resource_release(RES_JFX0 + JB_DLY0 + h);
    run(1200);                                          /* the memory comes back */
    for (k = 0; k < NPART; k++) T(k, P_DUB_ECHO, 0);
    run(1500);
    for (k = 0; k < NPART; k++) { T(k, P_DUB_TIML, 127); T(k, P_DUB_TIMR, 127); dub_put_field(trk[k].p, DUBF_SYNC, 0); T(k, P_DUB_ECHO, 100); }
    run(1500);                                          /* four lines of 16 blocks: more than the arena holds */
    jfx_clear_all();                                    /* a project load: the instances from silence */
    run(900);
    for (b = 0; b < 3; b++) { T(1, P_REV, 80 * (int)(b & 1u)); T(2, P_DLY, 90 * (int)(b & 1u)); T(3, P_CHOR, 70); run(700); }
    end("MEM");
}

/* an empty project as the stores hold it (tests/project_test.c) */
static void blank_project(project_t *a)
{
    memset(a, 0, sizeof *a);
    for (uint32_t k = 0; k < NTRK; k++) {
        p5_patch_init(&a->p5[k]);
        cz_patch_init(a->cz[k].raw);
        memcpy(a->fm6[k], FM6_INIT, FM6_PACKED);
    }
    a->magic = PROJ_MAGIC; a->size = sizeof *a; a->parts = NPART; a->phys = PROJ_PHYS;
    chain_defaults(&a->chain);
    a->sum = proj_sum(a);
}
static void scenario_r2(void)
{
    static project_t a, c;
    static project_store_t st;
    uint32_t k, n, ok = 0, combos = 0;
    begin("R2");
    fresh();
    for (n = 0; n < 126u; n += 4u) {                     /* LEN 0..6 x PTRN 0..5 x SPRP 0..2 = 126 combinations, four at a time (one a track) */
        for (k = 0; k < NPART; k++) {
            uint32_t x = (n + k) % 126u, len = x % 7u, ptrn = (x / 7u) % 6u, sprp = x / 42u;
            blank_project(&a);
            a.t[k].p[P_DUBO_LEN] = (int16_t)len; a.t[k].p[P_DUBO_PTRN] = (int16_t)ptrn; a.t[k].p[P_DUBO_SPRP] = (int16_t)sprp;
            a.t[k].p[P_JRES0] = (int16_t)(40 + (x * 7u) % 60u); a.t[k].p[P_JRES1] = (int16_t)(60 + (x * 11u) % 67u); a.t[k].p[P_DUBO_TONE] = (int16_t)((x * 13u) % 128u);
            a.t[k].p[P_DUBO_SPACE] = (int16_t)(x % 2u ? 24 + (int)(x % 40u) : 64 + (int)(x % 60u));
            a.sum = proj_sum(&a);
            ok = proj_pack(&st, &a) && proj_import(&c, &st, sizeof st);
            CHECK(ok && dub_new_layout(c.t[k].p), "a Release 2 project does not load as the new layout (%u)", x);
            {   static const uint8_t ids[10] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47 };
                for (uint32_t i = 0; i < 10u; i++) trk[k].p[ids[i]] = c.t[k].p[ids[i]];     /* the ten slots, as the load puts them in the track */
            }
            combos++;
        }
        run(260);
    }
    printf("R2 combinations %u\n", combos);
    end("R2");
}

static void scenario_heavy(uint32_t setting)
{
    uint32_t k, ps, st, i;
    char name[16];
    static const int notes_a[4][4] = { { 36, 0, 0, 0 }, { 36, 0, 0, 0 }, { 48, 52, 55, 59 }, { 60, 64, 67, 71 } }, notes_b[4][4] = { { 36, 0, 0, 0 }, { 38, 0, 0, 0 }, { 41, 45, 48, 53 }, { 65, 69, 72, 77 } };
    snprintf(name, sizeof name, "HEAVY%u", setting);
    begin(name);
    fresh();
    for (k = 0; k < NPART; k++) {
        T(k, P_REV, 64); T(k, P_DLY, 64); T(k, P_CHOR, 64);
        T(k, P_DUB_ECHO, 100); T(k, P_DUB_TIML, 53); T(k, P_DUB_TIMR, 74); T(k, P_DUB_FDBK, 60); T(k, P_DUB_SEND, 100); T(k, P_DUB_FILT, 64); T(k, P_DUB_SPD, 64);
        if (setting == 2u) { T(k, P_DUB_MODE, 64 | 2 | 1); T(k, P_DUB_TAPE, 8 * 12 + 3 - 64); T(k, P_DUB_SPRG, 28); }
        else { T(k, P_DUB_MODE, 64 | 1); T(k, P_DUB_TAPE, 6 * 12 + 0 - 64); T(k, P_DUB_SPRG, 0); }
    }
    for (ps = 0; ps < 16u; ps++)
        for (st = 0; st < 16u; st++) {
            uint32_t tk = st % 4u;
            const int *nt = (ps & 1u) ? notes_b[tk] : notes_a[tk];
            for (i = 0; i < 4u && nt[i]; i++) trk_note_on(&trk[tk], nt[i], 100);
            for (i = 0; i < 172u; i++) {
                if (i == 120u) for (uint32_t j = 0; j < 4u && nt[j]; j++) trk_note_off(&trk[tk], nt[j]);
                if (!(gb & 3u)) dub_late_half(0, (gb >= 1500u && (gb % 2500u) == 1248u) ? 2640u : (gb % 700u) == 348u ? 1500u : 1200u);   /* a half at 91 % now and then, once the instances run */
                blk();
            }
        }
    end(name);
}

#ifdef FXI_NEW
/* the descriptor DUB TAPE hands the host (fx_kernel_t): the plan's section 4 column, the legacy slots, the stable ids, against the firmware's own parameter table */
static void descriptor_checks(void)
{
    const fx_kernel_t *kn = fx_kernel_of(0);
    static const uint16_t slots[10] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47 };
    uint32_t i, ok = 1;
    printf("descriptor checks\n");
    CHECK(kn == fx_kernel_of(3), "one kernel for every track");
    CHECK(kn->module_id == 1u && kn->effect_id == 1u && kn->legacy_module != 0u, "identity: module %u effect %u", (unsigned)kn->module_id, kn->effect_id);
    CHECK(kn->nrows == 10u && kn->rows, "ten stored parameters");
    for (i = 0; i < 10u; i++) {
        const param_desc_t *d = param_desc_of(ENGI_MACRO, slots[i]);
        ok &= kn->rows[i].legacy == slots[i] && kn->rows[i].id == i + 1u && kn->rows[i].min == d->min && kn->rows[i].max == d->max && kn->rows[i].def == d->def;
        if (i) ok &= kn->rows[i].id > kn->rows[i - 1u].id;
    }
    CHECK(ok, "the rows' legacy slots, ranges and defaults are the firmware's parameter table's");
    CHECK(kn->max_inst == DUB_N && kn->ram_state == DUB_STATE_BYTES && kn->ram_extra_bytes == 5632u && kn->ram_work_max == 16u && kn->ram_work_min == 4u, "capacity words");
    CHECK((kn->flags & FXK_SEED) == 0 && (kn->flags & FXK_TAIL_FAR) && (kn->flags & FXK_US) && kn->tail_base == 24u && kn->prof == AP_DUB && kn->rates == 0x06u, "tail, seed 0, class, rates");
    CHECK(kn->on && kn->size_sig && kn->size_need && kn->blocks_at && kn->hold_len && kn->tick && kn->param_sig && kn->snap && kn->cost_extra && kn->start && kn->process && kn->reset, "every call is there");
    {   fx_ictx_t c; memset(&c, 0, sizeof c); c.beat = beat_samples();
        CHECK(fx_kernel_of(0)->blocks_at(dub_far(&trk[0], &c), 2u) == dub_blocks_at(dub_far(&trk[0], &c), 2u), "sizing through the struct"); }
    CHECK(sizeof(dub_common_t) == sizeof(fx_common_t), "one common block");
    CHECK(FW_API_ID == 0xCAF53177u, "the interface identifier did not move (0x%08X)", (unsigned)FW_API_ID);
}
#endif

#ifndef FXI_NO_MAIN                                    /* tests/fx_dub_module_test.c runs the scenarios itself */
int main(void)
{
    fxi_progress = getenv("FXI_PROGRESS") != 0;
    scenario_long();
    scenario_mem();
    scenario_r2();
    scenario_heavy(1);
    scenario_heavy(2);
#ifdef FXI_NEW
    descriptor_checks();
#endif
    if (bad) { printf("fx_insert: %d FAILED\n", bad); return 1; }
    printf("fx_insert: done\n");
    return 0;
}
#endif
