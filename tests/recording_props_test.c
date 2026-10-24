/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Jeremiah Conner.
 *
 * Orange Cat 0.13.0: the properties of a recorded step (ratchet, flam, trig condition, chance, nudge, accent) apply to the notes recorded on it as a group
 * (firmware/src/recording.c, docs/orangecat/TRACKER_REVIEW.md). The fixture is a CHORD of three onsets inside one step (a quarter, half and three quarters of the
 * way), played by the real sequencer one sample at a time; recording.c's RECORDING_HIT_HOOK logs every hit (note, velocity, gate, and when).
 * Revisions: 0.13.0: first version. */
#include <stdint.h>
#define RECORDING_NO_MAIN 1
static void hit_log(uint32_t note, uint32_t vel, uint32_t gate, uint32_t late);
#define RECORDING_HIT_HOOK(t, i, vel, gate, late) hit_log(recording[i].note, vel, gate, late)
#include "recording_test.c"

static struct { uint32_t note, vel, gate, at; } hl[512];
static uint32_t hn, T;                                  /* hits logged; samples advanced */
static void hit_log(uint32_t note, uint32_t vel, uint32_t gate, uint32_t late)
{
    if (hn < 512u) { hl[hn].note = note; hl[hn].vel = vel; hl[hn].gate = gate; hl[hn].at = T + 1u - late; hn++; }
}
#define SLEN 4u
static uint32_t PERIOD, LOOP;
static const uint16_t ON[3] = {16384, 32768, 49152};
static const uint8_t NOTE[3] = {60, 64, 67};
static uint32_t VIEW = 2, EARLY;                        /* the step the chord is on; EARLY: onsets close to the step's start */
static void begin_to(void (*prepare)(void))
{
    raw_begin();
    track_t *t = &trk[0];
    seq_stop(); song.rec = 0;
    recording_reset();
    t->p[P_SLEN] = SLEN;
    for (uint32_t i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }
    t->step[VIEW].time = ST_NOTE; t->step[VIEW].flags = SF_RECORDED;
    if (prepare) prepare();
    for (uint32_t k = 0; k < 3; k++) {
        recorded_note_t r = {0};
        r.on = EARLY ? (uint16_t)(k * 2000u) : ON[k]; r.duration = 8192; r.note = NOTE[k]; r.vel = 100; r.step = (uint8_t)VIEW;
        recording_restore_note(t, k, r);
    }
    PERIOD = seq_div_samples((uint32_t)t->p[P_SDIV]);
    LOOP = recording_prefix(t, PERIOD, SLEN);
    hn = 0; T = 0;
    seq_start(); seq_advance(0);
}
static void begin(void) { VIEW = 2; EARLY = 0; begin_to(0); }
static void run(uint32_t samples) { for (uint32_t i = 0; i < samples; i++) { seq_advance(1); T++; } }
/* the n-th hit of a pass of the loop, by note */
static uint32_t base_at(uint32_t k) { return 2u * PERIOD + (uint32_t)((uint64_t)ON[k] * PERIOD / RECORD_UNIT); }
static int near(uint32_t a, uint32_t b) { return a + 1u >= b && a <= b + 1u; }
static void set_props(uint32_t flags, int nudge, uint32_t cond, uint32_t chance)
{
    step_t *s = &trk[0].step[VIEW];
    s->flags = (uint8_t)(SF_RECORDED | flags); s->nudge = (int8_t)nudge; s->cond = (uint8_t)cond; s->probability = (uint8_t)(chance == 100u ? 0u : chance);
    recording_resched(&trk[0]);
}
static int baseline(void)
{
    begin(); run(LOOP);
    int ok = hn == 3u;
    for (uint32_t k = 0; k < 3u && ok; k++) ok &= hl[k].note == NOTE[k] && hl[k].vel == 100u && near(hl[k].at, base_at(k));
    return check("a recorded chord with no effects plays three hits at its recorded times", ok);
}
static int accent(void)
{
    begin(); set_props(SF_ACCENT, 0, 0, 100); run(LOOP);
    int ok = hn == 3u;
    for (uint32_t k = 0; k < 3u && ok; k++) ok &= hl[k].vel == 127u && near(hl[k].at, base_at(k));
    return check("ACCENT plays every onset of the step at velocity 127", ok);
}
static int nudge(void)
{
    int ok = 1;
    static const int N[3] = {16, -8, 32};
    for (uint32_t n = 0; n < 3u; n++) {
        begin(); set_props(0, N[n], 0, 100); run(LOOP);
        ok &= hn == 3u;
        for (uint32_t k = 0; k < 3u && ok; k++) ok &= near(hl[k].at, (uint32_t)((int32_t)base_at(k) + N[n] * (int32_t)(PERIOD / 64u)));
    }
    return check("NUDGE moves every onset of the step by the same amount (a quarter step later, an eighth earlier, a half step: past the step's end, late)", ok);
}
static int edge_nudge;
static void edge_prepare(void) { trk[0].step[VIEW].nudge = (int8_t)edge_nudge; }
static int nudge_edges(void)
{
    int ok;
    VIEW = SLEN - 1u; EARLY = 0; edge_nudge = 63;           /* the loop's last step, its onsets pushed past the loop's end */
    begin_to(edge_prepare);
    run(2u * LOOP);
    ok = hn == 6u;                                         /* three a pass, none wrapped to the start of the next, none lost */
    for (uint32_t k = 0; k < 6u && ok; k++) ok &= hl[k].at < (k / 3u + 1u) * LOOP + 2u && hl[k].at + 2u >= (k / 3u) * LOOP + (SLEN - 1u) * PERIOD;
    VIEW = 0; EARLY = 1; edge_nudge = -63;                 /* the first step, its onsets pulled before the loop's start */
    begin_to(edge_prepare);
    run(2u * LOOP - 1u);
    ok &= hn == 6u;
    for (uint32_t k = 0; k < 6u && ok; k++) ok &= hl[k].at < (k / 3u) * LOOP + 3u && hl[k].at + 3u >= (k / 3u) * LOOP;
    VIEW = 2; EARLY = 0;
    return check("NUDGE never wraps: onsets past the loop's end play at its end, ones before its start at its start, none are lost or doubled", ok);
}
static int ratchet(void)
{
    int ok = 1;
    for (uint32_t x = 2; x <= 4; x++) {
        begin(); set_props(0, 0, 0, 100); step_set_rflam(&trk[0].step[2], x); run(LOOP);
        uint32_t part = PERIOD / x;
        ok &= hn == 3u * x;
        /* the hits come in time order: sort by time to compare */
        for (uint32_t a = 0; a < hn; a++) for (uint32_t b = a + 1; b < hn; b++) if (hl[b].at < hl[a].at) { typeof(hl[0]) tmp = hl[a]; hl[a] = hl[b]; hl[b] = tmp; }
        for (uint32_t k = 0; k < 3u && ok; k++)
            for (uint32_t j = 0; j < x && ok; j++) {
                uint32_t want = base_at(k) + j * part, found = 0;
                for (uint32_t h = 0; h < hn; h++) if (hl[h].note == NOTE[k] && near(hl[h].at, want)) { found = 1; ok &= hl[h].gate <= part && hl[h].vel == 100u; }
                ok &= found;
            }
    }
    return check("RATCH x2..x4 plays each onset of the step again in equal parts of the step, a part's gate at most", ok);
}
static int flam(void)
{
    begin(); set_props(0, 0, 0, 100); step_set_rflam(&trk[0].step[2], 5); run(LOOP);
    uint32_t fl = flam_at(&trk[0], PERIOD);
    int ok = hn == 6u;
    for (uint32_t k = 0; k < 3u && ok; k++) {
        uint32_t g = 0, f = 0;
        for (uint32_t h = 0; h < hn; h++) if (hl[h].note == NOTE[k]) {
            if (near(hl[h].at, base_at(k))) { g++; ok &= hl[h].vel == 100u * 9u / 16u + 1u && hl[h].gate == fl; }
            if (near(hl[h].at, base_at(k) + fl)) { f++; ok &= hl[h].vel == 100u; }
        }
        ok &= g == 1u && f == 1u;
    }
    return check("FLAM plays each onset as a grace at 9/16 of its velocity and the full hit FLAM_SAMPLES later", ok);
}
static int chance_and_condition(void)
{
    int ok = 1, zero = 0, three = 0;
    begin(); set_props(0, 0, 0, 50); run(24u * LOOP);
    for (uint32_t p = 0; p < 24u; p++) {                   /* each pass: all of the chord, or none of it */
        uint32_t c = 0;
        for (uint32_t h = 0; h < hn; h++) c += hl[h].at / LOOP == p;
        ok &= c == 0u || c == 3u; zero += c == 0u; three += c == 3u;
    }
    ok &= zero && three;
    int a = ok;
    begin(); set_props(0, 0, 3 /* TC 1:2 */, 100); run(8u * LOOP);
    uint32_t c[8] = {0};
    for (uint32_t h = 0; h < hn; h++) if (hl[h].at / LOOP < 8u) c[hl[h].at / LOOP]++;
    uint32_t odd = 0, even = 0;
    for (uint32_t p = 0; p < 8u; p++) { if (p & 1u) odd += c[p]; else even += c[p]; }
    int b = (odd == 0u) != (even == 0u) && (odd == 12u || even == 12u);
    begin(); set_props(0, 0, 3, 50); run(24u * LOOP);
    uint32_t pc[24] = {0};
    for (uint32_t h = 0; h < hn; h++) if (hl[h].at / LOOP < 24u) pc[hl[h].at / LOOP]++;
    int d = 1;
    for (uint32_t p = 0; p < 24u; p++) d &= pc[p] == 0u || pc[p] == 3u;
    return check("CHANCE is rolled once for the step on each pass: the chord plays whole or not at all (both seen over 24 passes)", a) +
           check("the trig condition 1:2 is rolled once for the step: the chord plays on every other pass, whole", b) +
           check("chance and a condition together: still whole or not at all", d);
}
static int groups_with_mute_free_nothing_else_changes(void)
{
    /* the stored bytes of the step are the same ones a manual step keeps: setting the properties changes no other field of the step or the event */
    begin();
    recorded_note_t before[3]; memcpy(before, recording, sizeof before);
    set_props(SF_ACCENT, 5, 3, 70); step_set_rflam(&trk[0].step[2], 3);
    int ok = !memcmp(before, recording, sizeof before) && trk[0].step[2].time == ST_NOTE && (trk[0].step[2].flags & SF_RECORDED);
    return check("the properties live in the step: the recorded events are untouched by setting them", ok);
}
int main(void)
{
    int bad = baseline() + accent() + nudge() + nudge_edges() + ratchet() + flam() + chance_and_condition() + groups_with_mute_free_nothing_else_changes();
    printf("%s\n", bad ? "RECORDED STEP PROPERTIES TEST FAILED" : "Recorded step properties: all passed");
    return !!bad;
}
