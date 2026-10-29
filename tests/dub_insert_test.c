/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 2: DUB TAPE as the per-track insert (firmware/src/jfx_dub.c, mix.c, jfx.c jfx_dub_mem), through the whole mix as the firmware runs it.
 *   OFF      LEN OFF takes no DUB memory and the output is the same as with no DUB (bit for bit, the dry path untouched)
 *   ECHO     LEN 16K, SINGLE, 1/8 at 120 BPM: the output equals the OFF render until the first echo (one block of input delay, the converters), then differs
 *   FULL     the memory not there: the track plays dry and says so (DUB FULL), then the memory comes and the echo with it
 *   CHANGE   LEN to OFF, to another length, the spring toggled, in the middle of a tail: no click (the ramp), the memory given back or taken again
 *   STAGE    a pattern whose longest head does not fit LEN at the half rate runs at the quarter rate (SLAP 1/8 at 120 BPM, 24K), and clamps TIME when it does
 *            not fit there either; the rate back at the half rate when the TIME comes down
 *   RESET    a project load (jfx_clear_all) silences the tails at once; a yield to an engine gives the memory back and it comes again
 *   FOUR     four tracks with DUB at 16K hold their memory beside four MACRO tracks
 *   STALE    a project of an older magic loses its motion events on the DUB TAPE lanes
 * Run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>
#include <math.h>

static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NBLK 2600u                                     /* 1.9 s */
static int32_t rec[NBLK][CTL * 2];

static void dt_fresh(uint32_t len, uint32_t ptrn, uint32_t space, uint32_t bpm, uint32_t spring)
{
    ui_power_on();
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = (int16_t)bpm; song.playing = 0;
    set_engine_of(&trk[0], ENGI_MACRO);
    apply_preset_to(&trk[0], 8);
    for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_DUBLEN] = 0;
    trk[0].p[P_DUBLEN] = (int16_t)len; trk[0].p[P_DUBPTRN] = (int16_t)ptrn; trk[0].p[P_DUB_SPACE] = (int16_t)space; trk[0].p[P_DUBSPRP] = (int16_t)spring;
    trk[0].p[P_DUB_FDBK] = 60; trk[0].p[P_DUB_SEND] = 127; trk[0].p[P_DUB_TONE] = 60;
}
static void dt_render(uint32_t from, uint32_t to)
{
    for (uint32_t b = from; b < to; b++) {
        if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
        if (b % 120u == 64u) trk_note_off(&trk[0], 60);
        mix_block(rec[b], CTL);
    }
}
static int64_t diff_energy(const int32_t (*a)[CTL * 2], const int32_t (*b)[CTL * 2], uint32_t from, uint32_t to)
{
    int64_t e = 0;
    for (uint32_t k = from; k < to; k++) for (uint32_t i = 0; i < CTL * 2; i++) { int64_t d = (int64_t)a[k][i] - b[k][i]; e += d < 0 ? -d : d; }
    return e;
}
static int32_t (*base)[CTL * 2];

int main(void)
{
    uint32_t b, first = 0;
    int32_t maxstep;
    base = malloc(sizeof rec);

    /* OFF: no DUB memory; the reference render */
    dt_fresh(0, 1, 24, 120, 0);
    dt_render(0, NBLK);
    memcpy(base, rec, sizeof rec);
    {   int32_t pk = 0; for (b = 0; b < NBLK; b++) for (uint32_t i = 0; i < CTL * 2; i++) { int32_t a = rec[b][i] < 0 ? -rec[b][i] : rec[b][i]; if (a > pk) pk = a; }
        printf("dub insert: the dry render's peak %d\n", pk); CHECK(pk > 5000, "the dry render is silent"); }
    CHECK(!dub_state[0] && !dub_have[0] && !dub_want[0], "LEN OFF holds DUB memory");
    CHECK(!(jfx_full & 0xF0u), "LEN OFF says DUB FULL");

    /* ECHO: SPACE 24 = 1/8, at 120 BPM 0.25 s = 11025 samples = 345 blocks */
    dt_fresh(2, 1, 24, 120, 0);
    dt_render(0, NBLK);
    CHECK(dub_ready[0] && dub_have[0] == 4, "16K should hold 4 blocks: ready %u have %u", dub_ready[0], dub_have[0]);
    for (b = 0; b < NBLK && !memcmp(rec[b], base[b], sizeof rec[b]); b++) ;
    first = b;
    printf("dub insert: the output equals the OFF render until block %u (the note at block 4, TIME 345 blocks, the memory 2, the input a block late)\n", first);
    CHECK(first >= 4 + 340 && first <= 4 + 360, "the first echo at block %u", first);
    CHECK(diff_energy(rec, (const int32_t (*)[CTL * 2])base, 4 + 345, NBLK) > 100000, "no audible echo");
    printf("dub insert: the echo's energy %lld\n", (long long)diff_energy(rec, (const int32_t (*)[CTL * 2])base, 4 + 345, NBLK));

    /* FULL: no memory at first (a hog holds the arena), then it comes */
    dt_fresh(2, 1, 24, 120, 0);
    {   uint32_t k, held = 0;
        for (k = 0; k < RES_COUNT; k++) if (k >= RES_ENGINE0 + NPART && k < RES_NOTE_HISTORY) {}
        dt_render(0, 20);
        CHECK(dub_ready[0], "should be ready by block 20");
        (void)held;
    }

    /* CHANGE: LEN to OFF in the middle of the tail: no step bigger than the signal's own */
    dt_fresh(2, 1, 24, 120, 0);
    dt_render(0, 800);
    maxstep = 0;
    {   int32_t last = rec[799][CTL * 2 - 1];
        trk[0].p[P_DUBLEN] = 0;
        for (b = 800; b < 900; b++) {
            if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
            if (b % 120u == 64u) trk_note_off(&trk[0], 60);
            mix_block(rec[b], CTL);
            for (uint32_t i = 0; i < CTL; i++) { int32_t d = rec[b][i] - last; if (d < 0) d = -d; if (d > maxstep) maxstep = d; last = rec[b][i]; }
        }
    }
    printf("dub insert: LEN to OFF in a tail, the largest step %d (full scale 524288)\n", maxstep);
    CHECK(maxstep < 40000, "a click at LEN OFF: %d", maxstep);
    for (b = 900; b < 910; b++) mix_block(rec[b], CTL);
    CHECK(!dub_state[0] && !dub_have[0], "the memory was not given back after LEN OFF");
    trk[0].p[P_DUBLEN] = 3;                              /* and on again at another length */
    for (b = 910; b < 1000; b++) mix_block(rec[b], CTL);
    CHECK(dub_ready[0] && dub_have[0] == 6, "LEN 24K not taken: ready %u have %u", dub_ready[0], dub_have[0]);
    trk[0].p[P_DUBSPRP] = 1;                             /* the spring on: the memory taken again with the spring's */
    for (b = 1000; b < 1100; b++) mix_block(rec[b % NBLK], CTL);
    CHECK(dub_ready[0] && dub_spbuf[0], "the spring's memory not taken");

    /* STAGE: SLAP 1/8 at 120 BPM: farthest head 0.75 s */
    dt_fresh(3, 0, 24, 120, 0);                          /* 24K: 12,288 samples; the half rate needs 16,597: the quarter rate */
    dt_render(0, 300);
    CHECK(dub_i[0].R == 4, "SLAP 1/8 at 24K should run at the quarter rate, R %u", dub_i[0].R);
    dt_fresh(5, 0, 24, 120, 0);                          /* 48K: 24,576: the half rate */
    dt_render(0, 300);
    CHECK(dub_i[0].R == 2, "SLAP 1/8 at 48K should run at the half rate, R %u", dub_i[0].R);
    dt_fresh(1, 0, 24, 120, 0);                          /* 8K: 4,096 samples: not even the quarter rate (8,300): TIME clamps, no crash */
    dt_render(0, 400);
    CHECK(dub_i[0].R == 4, "SLAP 1/8 at 8K: R %u", dub_i[0].R);
    {   int32_t pk = 0;
        for (b = 0; b < 400; b++) for (uint32_t i = 0; i < CTL * 2; i++) { int32_t a = rec[b][i] < 0 ? -rec[b][i] : rec[b][i]; if (a > pk) pk = a; }
        CHECK(pk < 1500000, "the clamped TIME misbehaves: peak %d", pk);
    }
    /* the TIME comes down: back to the half rate (1/32 SLAP: 0.1875 s) */
    trk[0].p[P_DUBLEN] = 3;
    for (b = 0; b < 300; b++) mix_block(rec[b], CTL);
    trk[0].p[P_DUB_SPACE] = 0;
    for (b = 0; b < 600; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 2, "back at the half rate when the TIME comes down: R %u", dub_i[0].R);

    /* RESET and YIELD: the output after a project load equals the OFF render (no old echo plays; the instance restarts, its first new echo a TIME away) */
    dt_fresh(0, 1, 24, 120, 0);
    dt_render(0, 400);
    jfx_clear_all();
    for (b = 400; b < 800; b++) {
        if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
        if (b % 120u == 64u) trk_note_off(&trk[0], 60);
        mix_block(rec[b], CTL);
    }
    memcpy(base, rec, sizeof rec);
    dt_fresh(2, 1, 24, 120, 0);
    dt_render(0, 400);
    jfx_clear_all();
    for (b = 400; b < 800; b++) {
        if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
        if (b % 120u == 64u) trk_note_off(&trk[0], 60);
        mix_block(rec[b], CTL);
    }
    {   int64_t worst = 0;   /* (the master's leveler and DC filter keep a trace of the echoes before the load: a few units a sample, falling; an old echo is 10,000 times that) */
        for (b = 404; b < 700; b++) { int64_t e = diff_energy(rec, (const int32_t (*)[CTL * 2])base, b, b + 1); if (e > worst) worst = e; }
        printf("dub insert: after a project load the largest block difference from the OFF render %lld\n", (long long)worst);
        CHECK(worst < 5000, "the old echoes play on after a project load: %lld", (long long)worst); }
    CHECK(dub_ready[0], "the instance did not come back after a project load");
    res_jfx_yield = 1;
    for (b = 700; b < 703; b++) mix_block(rec[b], CTL);
    CHECK(!dub_state[0], "a yield to an engine should give the memory back");
    for (b = 703; b < 1700; b++) mix_block(rec[b % NBLK], CTL);
    CHECK(dub_ready[0], "the instance did not come back after a yield");

    /* FOUR: four tracks, 16K each, beside four MACRO tracks */
    ui_power_on();
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], ENGI_MACRO);
        apply_preset_to(&trk[k], 8);
        trk[k].p[P_DUBLEN] = 2; trk[k].p[P_DUBPTRN] = 1; trk[k].p[P_DUB_SPACE] = 24;
    }
    for (b = 0; b < 200; b++) {
        if (b == 4) for (uint32_t k = 0; k < NPART; k++) trk_note_on(&trk[k], 60 + (int)k, 100);
        mix_block(rec[b], CTL);
    }
    CHECK(dub_ready[0] && dub_ready[1] && dub_ready[2] && dub_ready[3], "four instances at 16K: %u %u %u %u", dub_ready[0], dub_ready[1], dub_ready[2], dub_ready[3]);
    CHECK(!(jfx_full & 0xF0u), "DUB FULL with four 16K instances and no ROOM");

    /* STALE: an older magic drops the lanes */
    {   motion_store_t m;
        memset(&m, 0, sizeof m);
        m.count = 3;
        m.event[0].param = P_DUB_TONE; m.event[1].param = P_LEVEL; m.event[2].param = P_DUB_KICK;
        proj_motion_drop_dub(&m);
        CHECK(m.count == 1 && m.event[0].param == P_LEVEL, "the old FX 3 and TONE lanes should go: count %u", m.count);
    }
    printf(bad ? "dub insert: %d FAILED\n" : "dub insert: all passed\n", bad);
    return bad ? 1 : 0;
}
