/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: host test of Skiff's step fields on Melodee's sequencer (build doc phase 4a, parts 1 and 2):
 *   build/host/oc_steps_test
 * 1. the compact step (step_snapshot.h): every field, RATCH, FLAM, the trig condition and NUDGE included, survives
 *    the 11-byte form the undo history and the inactive banks keep, exactly; and a bank written and read back.
 * 2. RATCH x2..x4: a NOTE step plays its notes that many times, in equal parts of the step.
 * 3. FLAM: a grace at 9/16 of the velocity, then the full hit FLAM_SAMPLES later.
 * 4. the trig conditions: 1:2 on passes 1 and 3, 2:2 on 2 and 4, 3:4 on pass 3; !FILL every pass while FILL is not
 *    held, FILL never.
 * 5. NUDGE: +16 plays a quarter of the step late, -16 a quarter early (in the step before); a TIE keeps to the grid.
 * 6. a step whose chance is 0 % plays none of its RATCH parts.
 *
 * Revisions:
 *   0.2.0: first version (Skiff 1.3.3's seq.c ported to Melodee 1.4.5's).
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* every note-on a step plays (seq.c SEQ_TEST_HOOK): the block's start, the note, the velocity */
#define EV_MAX 256
static struct { uint32_t at; uint8_t note, vel; } ev[EV_MAX];
static uint32_t ev_n, ev_clock;
#define SEQ_TEST_HOOK(t, nn, vv, m, sk) do {                                              \
        for (uint32_t _i = 0; _i < (m); _i++)                                            \
            if (!(((sk) >> _i) & 1u) && (t) == &trk[0] && ev_n < EV_MAX) {               \
                ev[ev_n].at = ev_clock; ev[ev_n].note = (nn)[_i]; ev[ev_n].vel = (vv)[_i]; ev_n++; \
            }                                                                            \
    } while (0)

#define main hostsim_main
#include "hostsim.c"
#undef main

static int check(const char *what, int ok)
{
    printf("oc steps: %-80s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* run the transport for `blocks` blocks of CTL samples from the top, logging step 0..n note-ons of track 1 */
static void run(uint32_t blocks)
{
    int32_t o[2 * CTL];
    ev_n = 0;
    ev_clock = 0;
    transport_req = 1;
    for (uint32_t b = 0; b < blocks; b++, ev_clock += CTL)
        mix_block(o, CTL);
    transport_req = 2;
    mix_block(o, CTL);
}

static void one_note(track_t *t, uint32_t i, uint32_t note, uint32_t vel)
{
    step_t *s = &t->step[i];
    memset(s, 0, sizeof *s);
    s->n = 1;
    s->note[0] = (uint8_t)note;
    s->time = ST_NOTE;
    s->vel = (uint8_t)vel;
}

static void clear_steps(track_t *t)
{
    for (uint32_t i = 0; i < NSTEP; i++) {
        memset(&t->step[i], 0, sizeof t->step[i]);
        t->step[i].time = ST_REST;
    }
}

static uint32_t base_period(void) { return seq_div_samples((uint32_t)trk[0].p[P_SDIV]); }

static void setup(uint32_t len)
{
    host_tracks_init();
    song.g[G_BPM] = 120;
    trk[0].p[P_SLEN] = (int16_t)len;
    trk[0].p[P_SSWING] = 0;
    song.g[G_SWING] = 0;
    trk[0].p[P_SGATE] = 64;
    clear_steps(&trk[0]);
}

/* ---------------------------------------------------- 1. compact step --- */
static int test_snapshot(void)
{
    uint32_t bad = 0, k;
    srand(7);
    for (k = 0; k < 20000u; k++) {
        step_t a, b;
        step_snapshot_t c;
        memset(&a, 0, sizeof a);
        for (uint32_t i = 0; i < 4u; i++) a.note[i] = (uint8_t)(rand() & 127);
        a.n = (uint8_t)(rand() % 5);
        a.time = (uint8_t)(rand() % 3);
        a.flags = (uint8_t)(rand() & SF_ALL);
        a.vel = (uint8_t)(rand() & 127);
        a.hit = (uint8_t)rand();
        a.acc = (uint8_t)rand();
        a.probability = (uint8_t)(rand() % 102);
        a.cond = (uint8_t)(rand() % TC_COUNT);   /* (and, below, every flag a step may hold) */
        a.nudge = (int8_t)(NUDGE_MIN + rand() % (NUDGE_MAX - NUDGE_MIN + 1));
        step_snapshot_put(&c, &a);
        b = step_snapshot_get(&c);
        bad += memcmp(&a, &b, sizeof a) != 0;
    }
    int r = check("20000 random steps survive the 11-byte compact form exactly (RATCH FLAM cond NUDGE too)", !bad);
    /* through a bank: write bank 2 of track 1, read it back */
    pattern_init();
    {
        pattern_t p, q;
        memset(&p, 0, sizeof p);
        for (uint32_t i = 0; i < NSTEP; i++) {
            p.step[i].time = ST_REST;
            if (i % 3u == 0u) {
                one_note(&trk[0], 0, 60 + i % 12u, 100);
                p.step[i] = trk[0].step[0];
                step_set_rflam(&p.step[i], 1u + i % 5u);
                step_set_cond(&p.step[i], i % TC_COUNT);
                step_set_nudge(&p.step[i], (int32_t)(i % 64u) - 32);
            }
        }
        for (uint32_t i = 0; i < 4u; i++) p.timing[i] = TP[P_SLEN + i].def;
        pattern_write(0, 2, &p);
        pattern_read(0, 2, &q);
        r |= check("a bank written with RATCH / FLAM / conditions / NUDGE reads back the same", !memcmp(&p, &q, sizeof p));
    }
    return r;
}

/* ------------------------------------------------------------ 2. RATCH --- */
static int test_ratchet(void)
{
    int bad = 0;
    for (uint32_t hits = 2; hits <= 4; hits++) {
        uint32_t per, ok = 1;
        char what[120];
        setup(16);
        per = base_period();
        one_note(&trk[0], 0, 60, 100);
        step_set_ratchet(&trk[0].step[0], hits);
        run((per * 2u) / CTL);
        ok = ev_n == hits;
        for (uint32_t i = 0; ok && i < hits; i++) {
            int32_t want = (int32_t)(i * (per / hits));
            ok = abs((int32_t)ev[i].at - want) <= (int32_t)CTL && ev[i].note == 60;
        }
        snprintf(what, sizeof what, "RATCH x%u: %u hits in equal parts of the step (got %u)", hits, hits, ev_n);
        bad |= check(what, ok);
    }
    setup(16);
    one_note(&trk[0], 0, 60, 100);
    run((base_period() * 2u) / CTL);
    bad |= check("RATCH x1 (every older step): one hit, as before", ev_n == 1);
    return bad;
}

/* ------------------------------------------------------------- 3. FLAM --- */
static int test_flam(void)
{
    setup(16);
    one_note(&trk[0], 0, 60, 112);
    step_set_rflam(&trk[0].step[0], 5);
    run((base_period() * 2u) / CTL);
    return check("FLAM: a grace at 9/16 of the velocity, the full hit FLAM_SAMPLES (15 ms) later",
                 ev_n == 2 && ev[0].vel == 112u * 9u / 16u + 1u && ev[1].vel == 112 &&
                 abs((int32_t)(ev[1].at - ev[0].at) - (int32_t)FLAM_SAMPLES) <= (int32_t)CTL);
}

/* ------------------------------------------------- 4. trig conditions --- */
static uint32_t passes_played(uint32_t cond, uint32_t passes)   /* bit p: the step played on pass p + 1 */
{
    uint32_t per, mask = 0;
    setup(4);
    per = base_period();
    one_note(&trk[0], 0, 60, 100);
    step_set_cond(&trk[0].step[0], cond);
    run((per * 4u * passes - per / 2u) / CTL);     /* (the passes asked for, not the first hit of the next) */
    for (uint32_t i = 0; i < ev_n; i++)
        mask |= 1u << (ev[i].at / (per * 4u));
    return mask;
}
static int test_cond(void)
{
    int bad = 0;
    bad |= check("1:2 plays on passes 1 and 3", passes_played(TC_1_2, 4) == 0x5u);
    bad |= check("2:2 plays on passes 2 and 4", passes_played(TC_2_2, 4) == 0xAu);
    bad |= check("3:4 plays on pass 3 only", passes_played(TC_3_4, 4) == 0x4u);
    bad |= check("no condition plays every pass", passes_played(TC_NONE, 4) == 0xFu);
    seq_fill = 0;
    bad |= check("!FILL plays every pass while FILL is not held", passes_played(TC_NFILL, 4) == 0xFu);
    bad |= check("FILL plays on no pass while FILL is not held", passes_played(TC_FILL, 4) == 0u);
    return bad;
}

/* ------------------------------------------------------------ 5. NUDGE --- */
static int test_nudge(void)
{
    int bad = 0;
    uint32_t per;
    setup(8);
    per = base_period();
    one_note(&trk[0], 2, 62, 100);
    step_set_nudge(&trk[0].step[2], 16);
    run((per * 4u) / CTL);
    bad |= check("NUDGE +16: the step plays a quarter of a step late",
                 ev_n == 1 && abs((int32_t)ev[0].at - (int32_t)(2u * per + 16u * (per / 64u))) <= (int32_t)CTL);
    setup(8);
    one_note(&trk[0], 2, 62, 100);
    step_set_nudge(&trk[0].step[2], -16);
    run((per * 4u) / CTL);
    bad |= check("NUDGE -16: the step plays a quarter of a step early, in the step before",
                 ev_n == 1 && abs((int32_t)ev[0].at - (int32_t)(2u * per - 16u * (per / 64u))) <= (int32_t)CTL);
    setup(8);
    one_note(&trk[0], 1, 60, 100);
    trk[0].step[2].time = ST_TIE;
    step_set_nudge(&trk[0].step[2], 16);
    run((per * 4u) / CTL);
    bad |= check("a TIE's NUDGE is ignored: the note before is not played again", ev_n == 1);
    return bad;
}

/* ---------------------------------------------------- 6. chance, RATCH --- */
static int test_chance(void)
{
    setup(16);
    one_note(&trk[0], 0, 60, 100);
    step_set_ratchet(&trk[0].step[0], 4);
    step_set_chance(&trk[0].step[0], 0);
    run((base_period() * 2u) / CTL);
    return check("a step at 0 % chance plays none of its RATCH parts", ev_n == 0);
}

int main(void)
{
    int bad = 0;
    bad |= test_snapshot();
    bad |= test_ratchet();
    bad |= test_flam();
    bad |= test_cond();
    bad |= test_nudge();
    bad |= test_chance();
    printf(bad ? "oc steps: FAILED\n" : "oc steps: all passed\n");
    return bad ? 1 : 0;
}
