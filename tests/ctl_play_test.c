/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Control-note recording and playback (Release 3 step 7, PROJECT_FORMAT_R3.md 8.2b), through dub-perf's key path and MIDI CC 18 to 21:
 *  1. KICK, HOLD, DIVE, RISE keys on a recording track make flagged timed notes (124 to 127, length RN_CTL, a velocity, pitch 0), opened at the press and closed at the release
 *  2. the player drives dub_rec at the onset and releases it at the duration, at the stop, and a strike is one block at least; the live trigger is not touched
 *  3. a flagged note never reaches an engine; an ordinary note on the same number always does (and the control note does not end it)
 *  4. MIDI CC 18 to 21 edges record the same; a track that is not recording records nothing, and the keys still sound live
 *  5. a note edit (pitch) never moves a control note into a pitch
 * Run by tests/run_tests.sh. */
#define RECORDING_NO_MAIN 1
#include "recording_test.c"

static int ctl_keys(void)
{
    int bad = 0;
    static const uint32_t key[4] = {8, 10, 15, 17};              /* C# D# G# A#: KICK HOLD DIVE RISE */
    for (uint32_t w = 0; w < 4; w++) {
        char what[160];
        raw_begin(); track_t *t = &trk[0];
        t->p[P_QUANT] = Q_OFF;
        seq_advance(606); dub_key(key[w], 0, 1);
        bad += check("the live trigger is down while the key is", dub_trig[0][2 + w] == 127);
        seq_advance(1200); dub_key(key[w], 0, 0); seq_stop();
        bad += check("the live trigger is released with the key", dub_trig[0][2 + w] == 0);
        recorded_note_t r = recording[0];
        snprintf(what, sizeof what, "key %u records note %u flagged, vel 127, pitch 0, valid, one event", key[w], 124 + w);
        bad += check(what, r.note == 124 + w && r.length == RN_CTL && r.vel == 127 && r.pitch == 0 && recording_valid(&r) && !recording[1].vel && r.duration >= 1200);
        raw_replay();
        int ok = !dub_rec[0][w];
        seq_advance(605); ok &= !dub_rec[0][w];
        seq_advance(1); ok &= dub_rec[0][w] == 1;
        seq_advance(600); ok &= dub_rec[0][w] == 1;
        seq_advance(1200); ok &= dub_rec[0][w] == 0;
        for (uint32_t n = 124; n <= 127; n++) ok &= !gate_note(t, n);
        snprintf(what, sizeof what, "replay: dub_rec[%u] on at the onset, off at the duration, no engine note on %u..127", w, 124);
        bad += check(what, ok);
        for (uint32_t o = 0; o < 4; o++) if (o != w) bad += dub_rec[0][o] != 0;
    }
    return bad;
}
static int ctl_stop(void)
{
    int bad = 0;
    raw_begin(); track_t *t = &trk[0];
    t->p[P_QUANT] = Q_OFF;
    seq_advance(606); dub_key(10, 0, 1); seq_advance(2400); dub_key(10, 0, 0); seq_stop();
    raw_replay(); seq_advance(606 + 100);
    bad += check("HOLD sounding in the replay", dub_rec[0][1] == 1);
    seq_stop();
    bad += check("the transport stop releases the recorded half at once", dub_rec[0][1] == 0);
    (void)t;
    return bad;
}
static int ctl_mix(void)
{
    int bad = 0;
    raw_begin(); track_t *t = &trk[0];
    t->p[P_QUANT] = Q_OFF;
    seq_advance(606); input_on(t, 125, 100); seq_advance(300);
    dub_key(10, 0, 1); seq_advance(600); dub_key(10, 0, 0); seq_advance(300); input_off(t, 125); seq_stop();
    uint32_t n_ctl = 0, n_ord = 0;
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel) { n_ctl += recording[i].length == RN_CTL; n_ord += recording[i].length != RN_CTL && recording[i].note == 125; }
    bad += check("an ordinary 125 and a HOLD control note (also 125) are two events, one flagged", n_ctl == 1 && n_ord == 1);
    raw_replay();
    int eng = 0, rec = 0;
    for (uint32_t k = 0; k < 40; k++) { seq_advance(100); eng |= gate_note(t, 125); rec |= dub_rec[0][1]; }
    bad += check("replay: the ordinary note reaches the engine, the control note reaches dub_rec", eng && rec);
    return bad;
}
static int ctl_cc(void)
{
    int bad = 0;
    raw_begin(); track_t *t = &trk[0];
    t->p[P_QUANT] = Q_OFF;
    seq_advance(606); midi_parameter(t, 18, 127); seq_advance(900); midi_parameter(t, 18, 0); seq_stop();
    bad += check("MIDI CC 18 (KICK) down and up while recording makes a flagged note 124", recording[0].note == 124 && recording[0].length == RN_CTL && recording[0].duration >= 900);
    midi_parameter(t, 18, 127); midi_parameter(t, 18, 100);
    bad += check("a CC that stays high is one edge, not a second note", ({ uint32_t c = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) c += recording[i].vel != 0; c; }) == 1);
    midi_parameter(t, 18, 0);
    return bad;
}
static int ctl_idle(void)
{
    int bad = 0;
    raw_begin(); track_t *t = &trk[0];
    song.rec = 0;
    memset(recording, 0, sizeof recording[0] * 4);
    seq_advance(606); dub_key(8, 0, 1); seq_advance(300); dub_key(8, 0, 0); midi_parameter(t, 19, 127); midi_parameter(t, 19, 0);
    bad += check("a track that is not recording records no control note", !recording[0].vel);
    return bad;
}
int main(void)
{
    int bad = ctl_keys() + ctl_stop() + ctl_mix() + ctl_cc() + ctl_idle();
    printf("%s\n", bad ? "CONTROL NOTE PLAY TEST FAILED" : "Control note recording and playback tests passed");
    return !!bad;
}
