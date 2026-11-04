/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 2, redone in round 13: DUB TAPE as the per-track insert (firmware/src/jfx_dub.c, mix.c, jfx.c jfx_dub_mem), through the whole mix as the firmware runs it.
 *   OFF      ECHO 0 (never run) takes no DUB memory and the output is the same as with no DUB (bit for bit, the dry path untouched)
 *   ECHO     ECHO 100, both taps at 1/8 at 120 BPM: the output equals the OFF render until the first echo (one block of input delay, the converters), then differs
 *   FULL     the memory not there: the track plays dry and says so (DUB FULL), then the memory comes and the echo with it
 *   CHANGE   ECHO to 0 (muted, the memory kept, given back after an idle time), the spring turned up in a tail (no memory change), TIME up (the line grows) and down (it stays)
 *   STAGE    the line is sized from the times: 1/2 at 120 BPM at the half rate, 1/1 at the quarter rate, 1/1 dotted clamped; the rate and the size are sticky
 *   RESET    a project load (jfx_clear_all) silences the tails at once; a yield to an engine gives the memory back and it comes again
 *   FOUR     four tracks with DUB at 16K hold their memory beside four MACRO tracks
 *   STALE    a project of an older magic loses its motion events on the DUB TAPE lanes
 * Run by tests/run_tests.sh. */
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
#include "dub_module_host.h"                         /* (round 20) the echo is the module's: registered in main */

static int bad;
#define GMIX(blk) do { static uint32_t gq; if (!(gq++ & 3u)) dub_late_half(0, 1200u); mix_block((blk), CTL); } while (0)
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define NBLK 2600u                                     /* 1.9 s */
static int32_t rec[NBLK][CTL * 2];

/* on: ECHO 100 (else 0); idl, idr: the synced divisions of TIME L and TIME R (dub_sync_beats: 6 = 1/8, 9 = 1/4, 12 = 1/2, 15 = 1/1, 16 = 1/1 dotted) */
static void dt_fresh(uint32_t on, int idl, int idr, uint32_t bpm)
{
    ui_power_on();
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = (int16_t)bpm; song.playing = 0;
    set_engine_of(&trk[0], ENGI_MACRO);
    apply_preset_to(&trk[0], 8);
    for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_DUB_ECHO] = 0;
    trk[0].p[P_DUB_ECHO] = on ? 100 : 0; trk[0].p[P_DUB_TIML] = (int16_t)dub_t_val(idl); trk[0].p[P_DUB_TIMR] = (int16_t)dub_t_val(idr);
    trk[0].p[P_DUB_MODE] = DUB_MODE_MARK | 1; trk[0].p[P_DUB_FDBK] = 60; trk[0].p[P_DUB_SEND] = 127; trk[0].p[P_DUB_FILT] = 60;
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
    int32_t maxstep, dry_step = 0;
    base = malloc(sizeof rec);
    if (dub_host_module_set() != FXM_OK) { printf("dub insert: the DUB module did not register\n"); return 2; }

    /* OFF: no DUB memory; the reference render */
    dt_fresh(0, 6, 6, 120);
    dt_render(0, NBLK);
    memcpy(base, rec, sizeof rec);
    {   int32_t pk = 0; for (b = 0; b < NBLK; b++) for (uint32_t i = 0; i < CTL * 2; i++) { int32_t a = rec[b][i] < 0 ? -rec[b][i] : rec[b][i]; if (a > pk) pk = a; }
        printf("dub insert: the dry render's peak %d\n", pk); CHECK(pk > 5000, "the dry render is silent"); }
    CHECK(!dub_state[0] && !dub_have[0] && !dub_want[0], "ECHO 0 holds DUB memory");
    CHECK(!(jfx_full & 0xF0u), "ECHO 0 says DUB FULL");

    /* ECHO: both taps at 1/8, at 120 BPM 0.25 s = 11025 samples = 345 blocks */
    dt_fresh(1, 6, 6, 120);
    dt_render(0, NBLK);
    CHECK(dub_ready[0] && dub_have[0] == 4, "16K should hold 4 blocks: ready %u have %u", dub_ready[0], dub_have[0]);
    for (b = 0; b < NBLK && !memcmp(rec[b], base[b], sizeof rec[b]); b++) ;
    first = b;
    printf("dub insert: the output equals the OFF render until block %u (the note at block 4, TIME 345 blocks, the memory 2, the input a block late)\n", first);
    CHECK(first >= 4 + 340 && first <= 4 + 360, "the first echo at block %u", first);
    CHECK(diff_energy(rec, (const int32_t (*)[CTL * 2])base, 4 + 345, NBLK) > 100000, "no audible echo");
    printf("dub insert: the echo's energy %lld\n", (long long)diff_energy(rec, (const int32_t (*)[CTL * 2])base, 4 + 345, NBLK));

    /* FULL: the arena held by something else: the track plays dry and says DUB FULL, then the memory comes when it is released */
    dt_fresh(0, 6, 6, 120);
    {   static const uint32_t hog[] = { RES_JFX0 + JB_DLY0, RES_JFX0 + JB_DLY0 + 1, RES_JFX0 + JB_DLY0 + 2, RES_JFX0 + JB_DLY0 + 3, RES_JFX0 + JB_DLY0 + 4, RES_JFX0 + JB_DLY0 + 5, RES_JFX0 + JB_DLY0 + 6, RES_JFX0 + JB_DLY0 + 7 };   /* (the DELAY's line blocks: no delay send, so nothing else wants them) */
        uint32_t k;
        for (k = 0; k < sizeof hog / sizeof hog[0]; k++) {
            uint32_t size = 80000u;
            while (size >= 700u && !resource_allocate(hog[k], size)) size -= 700u;      /* (each takes the largest piece left in some bank) */
        }
        trk[0].p[P_DUB_ECHO] = 100;
        for (b = 0; b < 40; b++) mix_block(rec[b], CTL);
        CHECK(!dub_ready[0], "DUB took memory the arena did not have");
        CHECK(jfx_full & 0x10u, "DUB FULL should show on track 1: jfx_full %x", jfx_full);
        {   uint32_t f0 = resource_failures;                 /* a short instance asks again only when something is released: no allocator attempts in steady state (no periodic spike) */
            for (b = 40; b < 2040; b++) mix_block(rec[b % NBLK], CTL);
            CHECK(resource_failures == f0, "a short instance retries without a release: %u failed attempts in 2000 blocks", resource_failures - f0);
            CHECK(jfx_full & 0x10u, "DUB FULL should stay on while the arena is short: jfx_full %x", jfx_full); }
        for (k = 0; k < sizeof hog / sizeof hog[0]; k++) resource_release(hog[k]);
        for (b = 40; b < 400; b++) mix_block(rec[b], CTL);
        CHECK(dub_ready[0], "the memory should have come once released: state %u have %u", dub_i[0].state, dub_have[0]);
    }

    /* a short instance asks again when its own want comes down, with nothing released since (the review's second fault) */
    dt_fresh(0, 6, 6, 120);
    song.ntrk = 1;                                       /* (round 10: a track in use holds its engine's state from the switch; the probe below counts the blocks beside one state) */
    for (uint32_t k = 1; k < NPART; k++) eng_state_clear(k);
    {   static const uint32_t hog[] = { RES_JFX0 + JB_DLY0, RES_JFX0 + JB_DLY0 + 1, RES_JFX0 + JB_DLY0 + 2, RES_JFX0 + JB_DLY0 + 3, RES_JFX0 + JB_DLY0 + 4, RES_JFX0 + JB_DLY0 + 5, RES_JFX0 + JB_DLY0 + 6, RES_JFX0 + JB_DLY0 + 7 };
        uint32_t k;
        for (k = 0; k < sizeof hog / sizeof hog[0]; k++) {
            uint32_t size = 80000u;
            while (size >= 700u && !resource_allocate(hog[k], size)) size -= 700u;
        }
        resource_release(hog[1]); resource_release(hog[2]); resource_release(hog[3]);     /* a bank's pieces come back: room for a short line (4 blocks at least: the floor), not for 16 */
        {   uint32_t n = 0, q; int32_t ok = 0;                 /* how many 4 KB blocks fit beside the common block and one state, probed on spare handles */
            int32_t v;
            resource_allocate(RES_JFX0 + JB_DUBC, sizeof(dub_common_t));
            resource_allocate(RES_JFX0 + JB_DUBS0 + 3, DUB_STATE_BYTES);
            while (n < 16 && resource_allocate(RES_JFX0 + JB_DUBG0 + 16 + n, JFX_SEG * sizeof(int16_t))) n++;
            for (q = 0; q < n; q++) resource_release(RES_JFX0 + JB_DUBG0 + 16 + q);
            resource_release(RES_JFX0 + JB_DUBS0 + 3);
            resource_release(RES_JFX0 + JB_DUBC);
            for (v = 127; v >= 0 && !ok; v--) {            /* the longest free time whose line fits the blocks that do: the line is sized from the times now */
                float need = (float)dub_free_ms10(v) * 1e-4f * 22050.0f + 64.0f;
                if (fx_modk.blocks_at(need, 2u) <= n) ok = v;
            }
            printf("dub insert: %u blocks fit beside the common block and a state; the lowered free TIME %d\n", n, ok);
            trk[0].p[P_DUB_MODE] = DUB_MODE_MARK;            /* free times */
            trk[0].p[P_DUB_TIML] = trk[0].p[P_DUB_TIMR] = 127;     /* 1.4 s: 16 blocks, more than fit */
            trk[0].p[P_DUB_ECHO] = 100;
            for (b = 0; b < 60; b++) mix_block(rec[b], CTL);
            CHECK(!dub_ready[0] && (jfx_full & 0x10u), "16 blocks should not fit: ready %u full %x", dub_ready[0], jfx_full);
            trk[0].p[P_DUB_TIML] = trk[0].p[P_DUB_TIMR] = (int16_t)ok;
            for (b = 60; b < 200; b++) mix_block(rec[b], CTL);
            CHECK(ok && dub_ready[0], "the lowered want should be taken with nothing released: state %u have %u full %x", dub_i[0].state, dub_have[0], jfx_full);
        }
    }
    song.ntrk = NTRK;
    /* the setting back to what it was during the ramp (the review's first fault): the instance runs again */
    dt_fresh(1, 6, 6, 120);
    for (b = 0; b < 40; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].state == 2 && dub_have[0] == 4, "1/8 at 120 BPM: a 16K line: state %u have %u", dub_i[0].state, dub_have[0]);
    trk[0].p[P_DUB_TIML] = (int16_t)dub_t_val(12);       /* TIME L up to 1/2 (1 s): the line grows (a ramp, the memory changes, a restart) */
    for (b = 40; b < 400; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].state == 2 && dub_ready[0] && dub_have[0] == 12, "TIME up: the line grows to 12 blocks: state %u ready %u have %u", dub_i[0].state, dub_ready[0], dub_have[0]);
    trk[0].p[P_DUB_TIML] = (int16_t)dub_t_val(6);        /* and TIME down again: the line stays (sticky), nothing is cut */
    for (b = 400; b < 700; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].state == 2 && dub_ready[0] && dub_have[0] == 12, "TIME down: the line stays: state %u ready %u have %u", dub_i[0].state, dub_ready[0], dub_have[0]);

    /* CHANGE: ECHO to 0 in the middle of the tail: no step bigger than the signal's own (the kernel's cut); the instance and its memory stay for the idle time, then go back */
    dt_fresh(0, 6, 6, 120);                          /* the OFF render's own largest step over the same blocks, both channels */
    dt_render(0, 900);
    {   int32_t d1 = 0; for (b = 800; b < 900; b++) for (uint32_t i = 1; i < CTL * 2; i++) { int32_t d = rec[b][i] - rec[b][i - 1]; if (i == CTL) continue; if (d < 0) d = -d; if (d > d1) d1 = d; } dry_step = d1; }
    dt_fresh(1, 6, 6, 120);
    dt_render(0, 800);
    maxstep = 0;
    {   int32_t last = rec[799][CTL * 2 - 1];
        trk[0].p[P_DUB_ECHO] = 0;
        for (b = 800; b < 900; b++) {
            if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
            if (b % 120u == 64u) trk_note_off(&trk[0], 60);
            mix_block(rec[b], CTL);
            for (uint32_t i = 0; i < CTL * 2; i++) { int32_t d = rec[b][i] - last; if (i == CTL) { last = rec[b][i]; continue; } if (d < 0) d = -d; if (d > maxstep) maxstep = d; last = rec[b][i]; }
        }
    }
    printf("dub insert: ECHO to 0 in a tail, the largest step %d (full scale 524288)\n", maxstep);
    CHECK(maxstep < 40000 && maxstep < 3 * dry_step + 3000, "a click at ECHO 0: %d (the dry render's own step %d)", maxstep, dry_step);
    CHECK(dub_ready[0] && dub_have[0] == 4, "ECHO 0 keeps the instance for the idle time: ready %u have %u", dub_ready[0], dub_have[0]);
    trk[0].p[P_DUB_ECHO] = 100;                          /* back up inside the idle time: the same instance, no restart */
    for (b = 900; b < 1000; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].state == 2 && dub_ready[0] && dub_have[0] == 4 && dub_i[0].idle == 0, "ECHO back up: the same instance runs: state %u idle %u", dub_i[0].state, dub_i[0].idle);
    trk[0].p[P_DUB_ECHO] = 0;
    for (b = 1000; b < 1000 + DUB_IDLE + 40; b++) mix_block(rec[b % NBLK], CTL);
    CHECK(!dub_state[0] && !dub_have[0], "the memory was not given back after the idle time at ECHO 0");
    trk[0].p[P_DUB_ECHO] = 100;                          /* and on again */
    for (b = 910; b < 1010; b++) mix_block(rec[b], CTL);
    CHECK(dub_ready[0] && dub_have[0] == 4, "ECHO up again not taken: ready %u have %u", dub_ready[0], dub_have[0]);
    trk[0].p[P_DUB_SPRG] = 80;                           /* the spring turned up: nothing about the memory changes (the spring's was taken with the instance), no restart */
    {   uint32_t st0 = dub_i[0].state; int16_t *sp0 = dub_spbuf[0];
        for (b = 1000; b < 1100; b++) mix_block(rec[b % NBLK], CTL);
        CHECK(dub_ready[0] && dub_spbuf[0] && dub_spbuf[0] == sp0 && dub_i[0].state == st0 && st0 == 2, "SPRING restarted or lost the instance: state %u -> %u", st0, dub_i[0].state);
    }

    /* STAGE: the line is sized from the times. 1/2 at 120 BPM (1 s): 12 blocks at the half rate; 1/1 (2 s): the half rate would need 22, so the quarter rate and 12 blocks;
     * 1/1 dotted (3 s): more than 16 blocks even at the quarter rate: the tap clamps, no crash */
    dt_fresh(1, 12, 12, 120);
    dt_render(0, 300);
    CHECK(dub_i[0].R == 2 && dub_have[0] == 12, "1/2 at 120 BPM should be 12 blocks at the half rate: R %u have %u", dub_i[0].R, dub_have[0]);
    dt_fresh(1, 15, 15, 120);
    dt_render(0, 300);
    CHECK(dub_i[0].R == 4 && dub_have[0] == 12, "1/1 at 120 BPM should be 12 blocks at the quarter rate: R %u have %u", dub_i[0].R, dub_have[0]);
    dt_fresh(1, 16, 16, 120);
    dt_render(0, 400);
    CHECK(dub_i[0].R == 4 && dub_have[0] == 16, "1/1 dotted: 16 blocks at the quarter rate, the tap clamped: R %u have %u", dub_i[0].R, dub_have[0]);
    {   int32_t pk = 0;
        for (b = 0; b < 400; b++) for (uint32_t i = 0; i < CTL * 2; i++) { int32_t a = rec[b][i] < 0 ? -rec[b][i] : rec[b][i]; if (a > pk) pk = a; }
        CHECK(pk < 1500000, "the clamped TIME misbehaves: peak %d", pk);
    }
    /* sticky: the TIME comes down and the line and the rate stay (no cut for passing through a range) */
    trk[0].p[P_DUB_TIML] = trk[0].p[P_DUB_TIMR] = (int16_t)dub_t_val(0);
    for (b = 0; b < 600; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 4 && dub_have[0] == 16 && dub_i[0].state == 2, "the line should stay when TIME comes down: R %u have %u state %u", dub_i[0].R, dub_have[0], dub_i[0].state);

    /* a tempo still settling must not switch the rate stage (and clear the line): BPM 50 for 100 blocks and back; 1/2 at 90 BPM (1.33 s) needs 14.4 blocks at the half rate, at 50 BPM (2.4 s) 26 */
    dt_fresh(1, 12, 12, 90);
    for (b = 0; b < 400; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 2, "1/2 at 90 BPM should run at the half rate: R %u", dub_i[0].R);
    song.g[G_BPM] = 50;
    for (b = 0; b < 100; b++) mix_block(rec[b], CTL);
    song.g[G_BPM] = 90;
    for (b = 0; b < 500; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 2 && dub_i[0].state == 2, "a tempo glitch moved the rate stage: R %u state %u", dub_i[0].R, dub_i[0].state);
    song.g[G_BPM] = 50;
    for (b = 0; b < 700; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 4, "a lasting slow tempo should move the line to the quarter rate: R %u", dub_i[0].R);
    song.g[G_BPM] = 90;
    for (b = 0; b < 1200; b++) mix_block(rec[b], CTL);
    CHECK(dub_i[0].R == 4, "and the rate stays at the quarter rate when the tempo returns (sticky until the instance goes): R %u", dub_i[0].R);
    /* RESET and YIELD: the output after a project load equals the OFF render (no old echo plays; the instance restarts, its first new echo a TIME away) */
    dt_fresh(0, 6, 6, 120);
    dt_render(0, 400);
    jfx_clear_all();
    for (b = 400; b < 800; b++) {
        if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
        if (b % 120u == 64u) trk_note_off(&trk[0], 60);
        mix_block(rec[b], CTL);
    }
    memcpy(base, rec, sizeof rec);
    dt_fresh(1, 6, 6, 120);
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

    /* STATE (round 10, BUILD.md 7.21 item 3): a track in use holds its engine's state from the switch (engine_block), not from its first note. With the
     * arena full (DUB at 32K, the three buses, fillers), a MACRO track's engine change yields the effects at the switch; its first note, two blocks
     * later, sounds. Before round 10 the take was at the first note, failed there, and the note was dropped: the second and third checks fail on that code. */
    ui_power_on();
    resource_host_cache_enabled = 0;
    res_lend_ok = 0;
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0; song.g[G_DTIME] = 1;
    for (uint32_t k = 0; k < NPART; k++) { set_engine_of(&trk[k], ENGI_MACRO); apply_preset_to(&trk[k], 8); trk[k].p[P_DUB_ECHO] = 0; }
    for (b = 0; b < 4; b++) mix_block(rec[b % NBLK], CTL);
    for (uint32_t k = 0; k < NPART; k++) { eng_state_clear(k); trk[k].engine = 0; }   /* as a project load or power-on leaves the tracks: the engines named (eng_req), nothing held, no preset hook run */
    for (b = 4; b < 8; b++) mix_block(rec[b % NBLK], CTL);
    {   uint32_t held = 0;
        for (uint32_t k = 0; k < NPART; k++) held += resource[RES_ENGINE0 + k].size != 0;
        printf("dub insert: STATE: four MACRO tracks from a project load, %u of %u states held before any note\n", held, trk_n());
        CHECK(held == trk_n(), "a track in use should hold its engine's state from the switch, not from its first note: %u of %u", held, trk_n());
    }
    trk[0].p[P_DUB_ECHO] = 100; trk[0].p[P_DUB_TIML] = (int16_t)dub_t_val(6); trk[0].p[P_DUB_TIMR] = (int16_t)dub_t_val(9); trk[0].p[P_DUB_FDBK] = 60; trk[0].p[P_DUB_SEND] = 127;
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 64;
    for (uint32_t k = 2; k < NPART; k++) { trk[k].engine = trk[k].eng_req = 0; eng_state_clear(k); }   /* tracks 3 and 4: no engine, their handles the fillers' */
    for (b = 8; b < 300; b++) { if (b == 12) trk_note_on(&trk[0], 60, 100); mix_block(rec[b % NBLK], CTL); }
    {   uint32_t fails0 = resource_failures, want = (eng_state_size(ENGI_MACRO) + 3u) & ~3u, sz;
        int dub_ran = dub_ready[0] && dub_state[0] != 0;
        CHECK(dub_ran, "the instance did not run beside the buses before the engine change");
        eng_state_clear(1);                                             /* track 2's state given back: an engine change clears it (engine_block) */
        for (sz = 60000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_ENGINE0 + 2, sz)) break;   /* the arena filled: the two largest free runs */
        for (sz = 60000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_ENGINE0 + 3, sz)) break;
        int full = resource_allocate(RES_ENGINE0 + 1, want) == 0;        /* (a probe: a MACRO state must not fit now) */
        if (!full) resource_release(RES_ENGINE0 + 1);
        resource_failures = fails0;
        printf("dub insert: STATE: arena %u of %u used with DUB 32K, the buses and the fillers; a MACRO state (%u) %s\n", resource_used(), RESOURCE_CAPACITY, want, full ? "does not fit" : "fits (the cell is not the unit's)");
        CHECK(full, "the cell did not fill the arena");
        memset(eng_take_gen, 0, sizeof eng_take_gen);
        trk[1].engine = 0;                                              /* (an engine change as the switch sees it, no preset hook: eng_req MACRO, the switch at the next block) */
        mix_block(rec[300 % NBLK], CTL);                                /* block 300: the switch, its take refused (the yield runs at block 301) */
        int held_at_switch = resource[RES_ENGINE0 + 1].size != 0;
        trk_note_on(&trk[1], 36, 100);                                  /* a note between the switch and the yield: its state is not there; it waits in xp_note */
        int waiting = trk[1].xp_n == 1 && trk[1].xp_note[0] == 36, active_early = 0;
        for (uint32_t i = 0; i < NVOICE; i++) active_early |= trk[1].v[i].active;
        for (b = 301; b < 306; b++) {
            if (b == 302) trk_note_on(&trk[1], 38, 100);                /* two blocks after the switch: the yield has run, the state is there */
            mix_block(rec[b % NBLK], CTL);
        }
        int sounding = 0, sounding36 = 0;
        for (uint32_t i = 0; i < NVOICE; i++) { sounding |= trk[1].v[i].active && trk[1].v[i].gate && trk[1].v[i].note == 38; sounding36 |= trk[1].v[i].active && trk[1].v[i].note == 36; }
        printf("dub insert: STATE: after the switch: held at the switch %d, state held %u, arena take failures %u, the note in the wait block waiting %d (active %d) then sounding %d, the note two blocks later sounding %d, DUB state %d ready %d\n",
               held_at_switch, resource[RES_ENGINE0 + 1].size != 0, resource_failures - fails0, waiting, active_early, sounding36, sounding, dub_i[0].state, dub_ready[0]);
        CHECK(!held_at_switch, "the full arena gave the state at the switch: the cell did not fill it");
        CHECK(resource[RES_ENGINE0 + 1].size != 0, "track 2's state is not held after its engine switch");
        CHECK(resource_failures > fails0, "the full arena refused no take: the yield was not exercised");
        CHECK(waiting && !active_early, "a note between the switch and the yield should wait in xp_note: xp_n %u active %d", trk[1].xp_n, active_early);
        CHECK(sounding36, "the note that waited for the state was lost");
        CHECK(sounding, "track 2's first note after the switch (two blocks later) was lost");
        trk_note_off(&trk[1], 36); trk_note_off(&trk[1], 38);
        {   /* UNISON: every voice of the note comes to voice_start; the note waits once, and a second note in the same wait block waits too (the review's finding 1).
             * The voices gone first (no fade: the switch is at once), the arena filled again so the state does not fit, then the pair in the wait block */
            uint32_t sz, fails1;
            for (b = 306; b < 330; b++) mix_block(rec[b % NBLK], CTL);
            eng_state_clear(1); memset(eng_take_gen, 0, sizeof eng_take_gen);
            for (sz = 60000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_NOTE_HISTORY, sz)) break;   /* (the effects' yielded room, filled) */
            for (sz = 60000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_HOT0 + 2, sz)) break;
            int full2 = resource_allocate(RES_ENGINE0 + 1, want) == 0;
            if (!full2) resource_release(RES_ENGINE0 + 1);
            fails1 = resource_failures;
            trk[1].p[P_VOICE] = V_UNISON; trk[1].engine = 0;
            mix_block(rec[330 % NBLK], CTL);                             /* the switch, its take refused */
            int no_fade = !trk[1].xf_on && trk[1].engine == eng_idx(trk[1].eng_req), inactive = 1;
            trk_note_on(&trk[1], 40, 100); trk_note_on(&trk[1], 43, 100);
            for (uint32_t i = 0; i < NVOICE; i++) inactive &= !trk[1].v[i].active;
            int two = trk[1].xp_n == 2 && trk[1].xp_note[0] == 40 && trk[1].xp_note[1] == 43;
            resource_release(RES_NOTE_HISTORY); resource_release(RES_HOT0 + 2);
            for (b = 331; b < 340; b++) mix_block(rec[b % NBLK], CTL);
            int s40 = 0, s43 = 0;
            for (uint32_t i = 0; i < NVOICE; i++) { s40 |= trk[1].v[i].active && trk[1].v[i].note == 40; s43 |= trk[1].v[i].active && trk[1].v[i].note == 43; }
            printf("dub insert: STATE: UNISON: arena full %d, the switch at once %d, two notes in the wait block: xp_n %u (%u %u) with every voice inactive %d, take failures %u; after the room came back 40 sounding %d, 43 sounding %d (UNISON: one note, the last)\n",
                   full2, no_fade, trk[1].xp_n, trk[1].xp_note[0], trk[1].xp_note[1], inactive, resource_failures - fails1, s40, s43);
            CHECK(full2 && no_fade, "the UNISON cell did not reach voice_start's wait (arena full %d, switch at once %d)", full2, no_fade);
            CHECK(two && inactive, "UNISON pushed a note once a voice, or started one: xp_n %u", trk[1].xp_n);
            CHECK(s43 && !s40, "UNISON (one note at a time, LAST priority): the last note of the wait block should sound, the first stay held (40 %d, 43 %d)", s40, s43);
            trk_note_off(&trk[1], 40); trk_note_off(&trk[1], 43); trk[1].p[P_VOICE] = V_POLY;
        }
        for (b = 340; b < 1400; b++) mix_block(rec[b % NBLK], CTL);
        printf("dub insert: STATE: 0.8 s later: DUB state %d ready %d have %u, FX FULL / DUB FULL bits %x, arena %u used\n", dub_i[0].state, dub_ready[0], dub_have[0], jfx_full, resource_used());
        CHECK(dub_ready[0] || (jfx_full & JFX_FULL_DUB0), "after the yield DUB neither came back nor says DUB FULL");
        resource_release(RES_ENGINE0 + 2); resource_release(RES_ENGINE0 + 3);
    }
    trk[0].p[P_REV] = trk[0].p[P_DLY] = trk[0].p[P_CHOR] = 0; trk[0].p[P_DUB_ECHO] = 0;

    /* FOUR: four tracks, 16K each, beside four MACRO tracks (the cache RAM ready and the card's slots lent, as the unit has them idle) */
    ui_power_on();
    resource_host_cache_enabled = 1;
    res_lend_ok = 15;
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], ENGI_MACRO);
        apply_preset_to(&trk[k], 8);
        trk[k].p[P_DUB_ECHO] = 100;
        dub_put_field(trk[k].p, DUBF_SYNC, 0); trk[k].p[P_DUB_TIML] = 40; trk[k].p[P_DUB_TIMR] = 40;   /* free times of 76 ms: lines of 16K, as the test says (since round 13 the line is sized from the times, and the default pair needs 24K; four of those and the spring tanks do not fit beside four MACRO tracks, and this test passed only because tanks refused for the per-block budget were dropped, FX_MODULES_PLAN.md 15.8) */
    }
    for (b = 0; b < 200; b++) {
        if (b == 4) for (uint32_t k = 0; k < NPART; k++) trk_note_on(&trk[k], 60 + (int)k, 100);
        mix_block(rec[b], CTL);
    }
    CHECK(dub_ready[0] && dub_ready[1] && dub_ready[2] && dub_ready[3], "four instances at 16K: %u %u %u %u", dub_ready[0], dub_ready[1], dub_ready[2], dub_ready[3]);
    CHECK(!(jfx_full & 0xF0u), "DUB FULL with four 16K instances and no ROOM");
    CHECK(dub_i[0].state == DUBS_RUN && dub_i[1].state == DUBS_RUN && dub_i[2].state == DUBS_RUN && dub_i[3].state == DUBS_RUN && fx_ins_spbuf[0] && fx_ins_spbuf[1] && fx_ins_spbuf[2] && fx_ins_spbuf[3], "four 16K instances running, each with its spring tank");

    /* BUSES FIRST (the owner, October 29): DUB holds the memory, then the REVERB is asked for: it gets its memory, the instances give way (ramped), DUB FULL shows on them */
    ui_power_on();
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    resource_host_cache_enabled = 1;
    res_lend_ok = 15;
    for (uint32_t k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], ENGI_MACRO);
        apply_preset_to(&trk[k], 8);
        trk[k].p[P_DUB_ECHO] = 100; trk[k].p[P_DUB_TIML] = trk[k].p[P_DUB_TIMR] = (int16_t)dub_t_val(9);       /* 1/4 at 120 BPM (0.5 s): 6 blocks x 4 */
    }
    for (b = 0; b < 400; b++) mix_block(rec[b], CTL);
    {   uint32_t before = dub_ready[0] + dub_ready[1] + dub_ready[2] + dub_ready[3], after, h;
        for (h = 0; h < 8; h++) { uint32_t size = 80000u; while (size >= 1000u && !resource_allocate(RES_JFX0 + JB_DLY0 + h, size)) size -= 500u; }   /* (what is left taken by something else: the reverb cannot fit without DUB giving way) */
        trk[0].p[P_REV] = 100;
        for (b = 0; b < 2500; b++) mix_block(rec[b], CTL);
        after = dub_ready[0] + dub_ready[1] + dub_ready[2] + dub_ready[3];
        printf("dub insert: buses first: %u instances held before the reverb, %u after; the reverb %s\n", before, after, jrev ? "has its memory" : "has none");
        CHECK(jrev != 0 && !(jfx_full & 7u), "the reverb should keep its memory: jrev %p jfx_full %x", (void *)jrev, jfx_full);
        CHECK(after < before && (jfx_full & 0xF0u), "an instance should give way with DUB FULL showing: %u -> %u, jfx_full %x", before, after, jfx_full);
        trk[0].p[P_REV] = 0;
        for (h = 0; h < 8; h++) resource_release(RES_JFX0 + JB_DLY0 + h);
    }
    /* CPU PROTECTION: CPU1 retired: the instances are released, DUB FULL shows; back after 2 s */
    dt_fresh(1, 6, 6, 120);
    for (b = 0; b < 200; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
    CHECK(dub_ready[0], "the instance should be running before the degrade");
    dub_force_degrade = 1;
    for (b = 200; b < 400; b++) mix_block(rec[b], CTL);
    CHECK(!dub_state[0] && (jfx_full & 0x10u), "CPU1 retired: the instance should be released with DUB FULL: state %p jfx_full %x", dub_state[0], jfx_full);
    CHECK(!dub_cm, "the common block should go back with the last instance");
    dub_force_degrade = 0;
    for (b = 0; b < 2900; b++) mix_block(rec[b % NBLK], CTL);
    CHECK(dub_ready[0] && !(jfx_full & 0xF0u), "back after CPU1 returned for 2 s: ready %u jfx_full %x", dub_ready[0], jfx_full);
    /* the governor: late halves while an instance runs: it gives way, stays out until its setting changes */
    for (b = 0; b < 6; b++) { dub_late_half(1, 3000u); dub_late_half(1, 3000u); GMIX(rec[b]); }
    for (b = 0; b < 200; b++) GMIX(rec[b]);
    CHECK(!dub_state[0] && (jfx_full & 0x10u), "the governor should have released the instance with DUB FULL: jfx_full %x", jfx_full);
    for (b = 0; b < 3000; b++) GMIX(rec[b % NBLK]);
    CHECK(!dub_state[0], "a governed instance must stay out for 8 s");
    for (b = 0; b < 4000; b++) { if (b % 400u == 0) dub_late_half(1, 3000u); GMIX(rec[b % NBLK]); }                 /* late halves go on: it stays out, past the 8 s */
    CHECK(!dub_state[0], "a governed instance must stay out while late halves go on");
    for (b = 0; b < 3500; b++) GMIX(rec[b % NBLK]);                                                            /* (block ~10,700 after the release: just before the 8 s are out) */
    dub_late_half(1, 3000u);                                                                                                    /* a late half now: the 2 s of quiet start over */
    for (b = 0; b < 2000; b++) GMIX(rec[b % NBLK]);
    CHECK(!dub_state[0], "the 2 s of quiet must wait for a late half that came at the end of the 8 s");
    for (b = 0; b < 1500; b++) GMIX(rec[b % NBLK]);
    CHECK(!dub_state[0], "the window still holds the heavy half: no room yet");
    for (b = 0; b < 44000; b++) GMIX(rec[b % NBLK]);                                                           /* quiet: after the wait, 2 s of quiet and 32 s without a heavy half it asks again */
    CHECK(dub_ready[0], "a governed instance must come back after 8 s and 2 s without a late half: state %u", dub_i[0].state);
    for (b = 0; b < 6; b++) { dub_late_half(1, 3000u); dub_late_half(1, 3000u); GMIX(rec[b]); }                              /* and again, to test the setting-change release */
    for (b = 0; b < 200; b++) GMIX(rec[b]);
    CHECK(!dub_state[0], "the governor should have released it again");
    CHECK(dub_i[0].gov_wait > 21000u && dub_i[0].gov_wait <= 22000u, "the second governor release in a row should wait twice as long: %u", dub_i[0].gov_wait);
    trk[0].p[P_DUB_ECHO] = 100; trk[0].p[P_DUB_TIML] = trk[0].p[P_DUB_TIMR] = (int16_t)dub_t_val(9);
    for (b = 0; b < 300; b++) GMIX(rec[b]);
    CHECK(dub_ready[0], "a new setting asks again: ready %u", dub_ready[0]);
    /* round 14: late halves before the instance existed are the mix's heaviest, and the window keeps them: no room for the instance that comes next, DUB FULL; 32 s lighter and it starts */
    dt_fresh(0, 6, 6, 120);
    for (b = 0; b < 100; b++) { dub_late_half(1, 3000u); mix_block(rec[b], CTL); }
    for (b = 0; b < 4000; b++) GMIX(rec[b % NBLK]);
    trk[0].p[P_DUB_ECHO] = 100;
    for (b = 0; b < 300; b++) GMIX(rec[b]);
    CHECK(!dub_ready[0] && (jfx_full & 0x10u), "no room after heavy halves: it must wait with DUB FULL (state %u full %x)", dub_i[0].state, jfx_full);
    for (b = 0; b < 44000; b++) GMIX(rec[b % NBLK]);
    CHECK(dub_ready[0] && dub_i[0].state == 2, "32 s of lighter halves: it should start: state %u", dub_i[0].state);
    /* the window itself, counted in halves with no instance: one heavy half is remembered for 8 buckets of 1376 halves, then forgotten */
    dub_reset(); dub_late_half(0, 2640u);
    for (b = 0; b < 8u * 1376u - 1u - 1u; b++) dub_late_half(0, 1000u);
    CHECK(!dub_headroom(), "a heavy half is still in the window after 7 buckets and a bit");
    for (b = 0; b < 1376u + 2u; b++) dub_late_half(0, 1000u);
    CHECK(dub_headroom(), "a heavy half is forgotten after 8 buckets");
    /* round 14: a lone half over 85 % gives way at once (no score), with a quiet half in between nothing does */
    for (b = 0; b < 4000; b++) { dub_late_half(0, 2000u); GMIX(rec[b % NBLK]); }
    CHECK(dub_i[0].state == 2, "halves at 2000 us (69 %%) must not evict: state %u", dub_i[0].state);
    dub_late_half(0, 2400u);
    CHECK(dub_i[0].state == 2 && !dub_gov_req, "a half at 83 %% must not evict");
    dub_late_half(0, 2500u); GMIX(rec[0]);
    for (b = 0; b < 40; b++) GMIX(rec[b]);
    CHECK(dub_i[0].state != 2 && (jfx_full & 0x10u) , "a lone half at 86 %% should give way at once, DUB FULL: state %u full %x", dub_i[0].state, jfx_full);
    /* the headroom: a mix whose heaviest half (91 %) leaves no room for an instance keeps it out, however long the wait */
    for (b = 0; b < 16000; b++) { dub_late_half(0, (b % 700u) == 0 ? 2640u : 1500u); GMIX(rec[b % NBLK]); }
    CHECK(!dub_state[0] && dub_gov_fires > 0, "no headroom: the instance must stay out");
    for (b = 0; b < 40000; b++) { dub_late_half(0, 1500u); GMIX(rec[b % NBLK]); }
    CHECK(dub_ready[0], "headroom again after 32 s of lighter halves: it should be back (state %u)", dub_i[0].state);
    /* the parameter copy follows the track's values although it is made again only when they change (the cache): SPACE, TONE, FDBK, CUT, the tempo */
    dt_fresh(1, 6, 6, 120);
    for (b = 0; b < 300; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
    {   float t0 = dub_cm->p[0].time_l, lp0 = dub_cm->p[0].lp, fb0 = dub_cm->p[0].feedback;
        trk[0].p[P_DUB_TIML] = (int16_t)dub_t_val(3); trk[0].p[P_DUB_FILT] = 20; trk[0].p[P_DUB_FDBK] = 90;     /* 1/32 .. 1/16 */
        for (b = 0; b < 300; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
        CHECK(dub_cm->p[0].time_l < 0.6f * t0 && dub_cm->p[0].lp != lp0 && dub_cm->p[0].feedback > fb0, "the parameter copy did not follow: time %f -> %f, lp %f -> %f, fb %f -> %f", t0, dub_cm->p[0].time_l, lp0, dub_cm->p[0].lp, fb0, dub_cm->p[0].feedback);
        {   float t1 = dub_cm->p[0].time_l;
            song.g[G_BPM] = 60;
            for (b = 0; b < 300; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
            CHECK(dub_cm->p[0].time_l > 1.8f * t1 && dub_cm->p[0].time_l < 2.2f * t1, "the tempo change did not reach TIME: %f -> %f", t1, dub_cm->p[0].time_l); }
        {   float s0 = dub_cm->p[0].speed, w0 = dub_cm->p[0].wow, d0 = dub_cm->p[0].drive, sp0 = dub_cm->p[0].sp_mix;     /* (the new ones: SPEED, WOW, DRIVE and SPRING follow, and PING) */
            dub_put_field(trk[0].p, DUBF_WOW, 11); dub_put_field(trk[0].p, DUBF_DRIVE, 15); dub_put_field(trk[0].p, DUBF_PING, 1);
            trk[0].p[P_DUB_SPD] = 96; trk[0].p[P_DUB_SPRG] = 100;
            for (b = 0; b < 300; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
            CHECK(dub_cm->p[0].wow > w0 + 0.9f && dub_cm->p[0].drive > d0 + 0.5f && dub_cm->p[0].ping == 1 && dub_cm->p[0].speed > 1.3f * s0 && dub_cm->p[0].sp_mix > sp0 + 0.5f,
                  "the parameter copy did not follow SPEED, WOW, DRIVE, SPRING, PING: wow %f, drive %f, ping %d, speed %f, spring %f", dub_cm->p[0].wow, dub_cm->p[0].drive, dub_cm->p[0].ping, dub_cm->p[0].speed, dub_cm->p[0].sp_mix);
        }
        dub_trig[0][1] = 127;                           /* CC 17: CUT */
        for (b = 0; b < 150; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
        CHECK(dub_cm->p[0].cut == 1, "CUT (the trigger) did not reach the parameter copy");
        dub_trig[0][1] = 0;
        trk[0].p[P_DUB_ECHO] = 0;                       /* ECHO 0 mutes: the kernel's cut */
        for (b = 0; b < 150; b++) { if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100); mix_block(rec[b], CTL); }
        CHECK(dub_cm->p[0].cut == 1, "ECHO 0 should cut the instance");
        trk[0].p[P_DUB_ECHO] = 100;
    }
    /* UNUSED: the lanes are inert and the memory is back with ECHO 0 */
    {   uint32_t used0, used1;
        dt_fresh(0, 6, 6, 120);
        dt_render(0, 400);
        used0 = resource_used();
        CHECK(!dub_cm && !dub_state[0], "ECHO 0: no DUB memory at all");
        memcpy(base, rec, sizeof rec);
        dt_fresh(0, 6, 6, 120);
        for (b = 0; b < 400; b++) {                      /* the other values driven to extremes as a motion lane would (ECHO stays 0), and the triggers */
            trk[0].p[P_DUB_FDBK] = (int16_t)(b & 1 ? 127 : 0); trk[0].p[P_DUB_SEND] = (int16_t)(b & 2 ? 127 : 0); trk[0].p[P_DUB_FILT] = (int16_t)(b & 4 ? 127 : 0);
            trk[0].p[P_DUB_SPD] = (int16_t)(b & 8 ? 127 : 0); trk[0].p[P_DUB_TIML] = (int16_t)(b & 16 ? 127 : 0); trk[0].p[P_DUB_SPRG] = (int16_t)(b & 32 ? 127 : 0); trk[0].p[P_DUB_TIMR] = (int16_t)(b & 64 ? 127 : 0);
            dub_trig[0][0] = (uint8_t)(b & 1 ? 127 : 0); dub_trig[0][1] = (uint8_t)(b & 2 ? 127 : 0); dub_trig[0][2] = (uint8_t)(b & 4 ? 127 : 0);
            if (b % 120u == 4u) trk_note_on(&trk[0], 60, 100);
            if (b % 120u == 64u) trk_note_off(&trk[0], 60);
            mix_block(rec[b], CTL);
        }
        used1 = resource_used();
        CHECK(!diff_energy(rec, (const int32_t (*)[CTL * 2])base, 0, 400), "motion on the lanes changed the output with ECHO 0");
        CHECK(used0 == used1 && !dub_cm, "ECHO 0 with the lanes moving took memory: %u vs %u", used0, used1);
        printf("dub insert: with ECHO 0 the lanes and triggers at extremes change nothing (bit for bit) and take no memory\n");
        memset(dub_trig, 0, sizeof dub_trig);
    }
    /* STALE: an older magic drops the lanes */
    {   motion_store_t m;
        memset(&m, 0, sizeof m);
        m.count = 3;
        m.event[0].param = P_DUB_FILT; m.event[1].param = P_LEVEL; m.event[2].param = P_DUB_SPRG;
        proj_motion_drop_dub(&m);
        CHECK(m.count == 1 && m.event[0].param == P_LEVEL, "the old FX 3 and TONE lanes should go: count %u", m.count);
    }
    printf(bad ? "dub insert: %d FAILED\n" : "dub insert: all passed\n", bad);
    return bad ? 1 : 0;
}
