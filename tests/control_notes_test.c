/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The control notes 124 .. 127 (KICK, HOLD, DIVE, RISE; docs/orangecat/PROJECT_FORMAT_R3.md 8.2b) against every note-range path
 * (the owner's request, round 11). They are ordinary timed notes with a reserved number, so the question is whether the number
 * comes through each path unchanged, and where an ordinary note can land on the same number.
 *   STORE    the recorder takes 124 .. 127 (recording_valid), keeps the number, replays it at its captured time and gate, and a project
 *            save and load returns it byte for byte; 128 is refused by recording_valid
 *   MIDI IN  midi_map with QNT off passes 124 .. 127 as they are; with SNAP, WHITE and ALL every result is silent or 0 .. 127
 *   KEYS     kb_map for every key at every octave -3 .. +3 and transpose -24, 0, +24, in every layout: silent or 0 .. 127
 *   SHIFT    the octave and the transpose are applied on the way IN only: a stored note is not moved by them (replay with either set
 *            plays the stored number)
 *   CLASH    what the test pins and the report states: an ordinary note CAN reach 124 .. 127 (MIDI 124 .. 127 as they are; the top key at
 *            OCT +3 and TRN +24 clamps to 127), so the number alone does not say "control" unless the recorder tags it
 * Run by tests/run_tests.sh. */
#define RECORDING_NO_MAIN 1
#include "recording_test.c"

static int ctl_store(void)
{
    int bad = 0;
    for (uint32_t n = 124; n <= 127; n++) {
        raw_begin(); track_t *t = &trk[0];
        t->p[P_QUANT] = Q_OFF;
        seq_advance(606); input_on(t, n, 101); seq_advance(1200); input_off(t, n); seq_stop();
        recorded_note_t original = recording[0];
        char what[120];
        snprintf(what, sizeof what, "note %u: recorded as that number, valid, one event", n);
        bad += check(what, original.note == n && recording_valid(&original) && !recording[1].vel);
        raw_replay(); int exact = !gate_note(t, n);
        seq_advance(605); exact &= !gate_note(t, n);
        seq_advance(1); exact &= gate_note(t, n);
        seq_advance(1200); exact &= !gate_note(t, n);
        snprintf(what, sizeof what, "note %u: replayed at its captured time and gate", n);
        bad += check(what, exact);
        project_capture(&proj_scratch);
        int packed = bank_pack(proj_wire_u.raw, &proj_scratch, 1) && bank_valid(proj_wire_u.raw, BANK_STORE_SIZE);
        memset(recording, 0, sizeof recording[0]);
        if (packed) { project_restore_runtime(&proj_scratch); bank_restore(proj_wire_u.raw); }
        snprintf(what, sizeof what, "note %u: saved and loaded byte for byte", n);
        bad += check(what, packed && !memcmp(&original, &recording[0], sizeof original));
    }
    recorded_note_t r = {0};
    r.vel = 90; r.duration = 10; r.note = 128; r.step = 0;
    bad += check("note 128 is refused by recording_valid (the recorder's ceiling)", !recording_valid(&r));
    r.note = 127;
    bad += check("note 127 is accepted by recording_valid", recording_valid(&r));
    return bad;
}
static int ctl_shift(void)
{
    int bad = 0;
    for (int tr = -24; tr <= 24; tr += 24)
        for (int oc = -3; oc <= 3; oc += 3) {
            raw_begin(); track_t *t = &trk[0];
            t->p[P_QUANT] = Q_OFF; t->p[P_TRANS] = 0; song.octave = 0;
            seq_advance(606); input_on(t, 125, 90); seq_advance(600); input_off(t, 125); seq_stop();
            t->p[P_TRANS] = (int16_t)tr; song.octave = (int8_t)oc;
            raw_replay(); seq_advance(606);
            char what[120];
            snprintf(what, sizeof what, "stored 125 replays as 125 with TRN %+d and OCT %+d", tr, oc);
            bad += check(what, recording[0].note == 125 && gate_note(t, 125));
        }
    trk[0].p[P_TRANS] = 0; song.octave = 0;
    return bad;
}
static int ctl_in(void)
{
    int bad = 0, off_ok = 1, range_ok = 1, keys_ok = 1, clash_midi = 0, clash_keys = 0;
    track_t *t = &trk[0];
    for (int q = Q_OFF; q <= Q_ALL; q++)
        for (int tr = -24; tr <= 24; tr += 24)
            for (int oc = -3; oc <= 3; oc++) {
                t->p[P_QUANT] = (int16_t)q; t->p[P_TRANS] = (int16_t)tr; song.octave = (int8_t)oc;
                for (uint32_t n = 124; n <= 127; n++) {
                    uint32_t m = midi_map(t, n);
                    if (q == Q_OFF && m != n) off_ok = 0;
                    if (m != KB_SILENT && m > 127u) range_ok = 0;
                    if (m != KB_SILENT && m >= 124u) clash_midi = 1;
                }
                for (uint32_t k = 0; k < 27u; k++) {
                    uint32_t m = kb_map(t, k);
                    if (m != KB_SILENT && m > 127u) keys_ok = 0;
                    if (m != KB_SILENT && m >= 124u) clash_keys = 1;
                }
            }
    t->p[P_QUANT] = Q_OFF; t->p[P_TRANS] = 0; song.octave = 0;
    bad += check("MIDI in, QNT off: 124 .. 127 come through unchanged at every OCT and TRN", off_ok);
    bad += check("MIDI in, every layout: silent or 0 .. 127, never above the recorder's ceiling", range_ok);
    bad += check("keys, every layout, OCT -3 .. +3, TRN -24 .. +24: silent or 0 .. 127", keys_ok);
    bad += check("CLASH (pinned): an ordinary MIDI note maps onto 124 .. 127", clash_midi);
    bad += check("CLASH (pinned): an ordinary key reaches 124 .. 127 at the top of the shifts (the clamp lands on 127)", clash_keys);
    return bad;
}
int main(void)
{
    int bad = ctl_store() + ctl_shift() + ctl_in();
    printf("%s\n", bad ? "CONTROL NOTES TEST FAILED" : "Control notes tests passed");
    return !!bad;
}
