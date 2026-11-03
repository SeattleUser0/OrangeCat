/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* The owner's rule, for a melodic chord as for a drum cell (tests/drum_editor_test.c): an edit never changes a note you did not touch. Each note of a recorded step keeps its own accent
 * from its own velocity (a note above 110 plays at 127); a velocity edit or a deletion of one note changes that note only.
 *   For each case a recorded chord is played by the sequencer into mix_block and rendered; the same chord is played after the edit; then THE EXPECTED chord is built from what the
 *   untouched notes were played at before (the hook of recording.c logs every hit's velocity), with the touched note as edited (or gone), and rendered. Checked:
 *     VOICE   every untouched note was played at the same velocity before and after the edit
 *     RENDER  the render after the edit is the expected chord's render, sample for sample (the notes of a chord are not independent in the mix: a limiter sits behind them, so a note's
 *             render cannot be taken out of a chord's by subtraction; the expected chord is the check instead)
 * Revisions:
 *   (the owner's rule, ui-fix2) first version.
 */
#include <stdint.h>
static uint32_t played[128];                            /* the velocity each note's last hit was played at */
#define RECORDING_HIT_HOOK(t, i, vel, gate, late) (played[recording[i].note & 127u] = (vel))
#define RECORDING_NO_MAIN 1
#include "recording_test.c"

#define RN 1400u                                        /* blocks of CTL rendered: about a second */
static int32_t R1[RN * CTL], R2[RN * CTL];
static int bad;
static void chk(const char *what, int ok) { printf("accent_own: %-124s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
/* a recorded chord on step 0 of the first track: notes/vels; flag = a deliberate step accent (or a project's from before) */
static void make_cell(const uint8_t *notes, const uint8_t *vels, uint32_t n, int flag)
{
    track_t *t = &trk[0];
    raw_begin(); song.rec = 0; seq_stop(); song.master_q12 = 4096; panic_req = 0;
    recording_clear(t);
    t->p[P_SLEN] = 16;
    for (uint32_t i = 0; i < n; i++) {
        recorded_note_t r = {.step = 0, .on = 0, .note = notes[i], .vel = vels[i], .duration = 32768, .owner = (uint8_t)recording_owner(t), .length = 0};
        recording_restore_note(t, i, r);
    }
    note_rebuild_all();
    if (flag) t->step[0].flags |= SF_ACCENT;
    go_page(GR_ROLL); cursor_set(0); frame();
}
static void render(int32_t *out)
{
    int32_t blk[2 * CTL];
    memset(played, 0, sizeof played);
    seq_start(); seq_advance(0);
    for (uint32_t b = 0; b < RN; b++) {
        seq_advance(CTL); mix_block(blk, CTL);
        for (uint32_t i = 0; i < CTL; i++) out[b * CTL + i] = blk[2 * i];
    }
    seq_stop();
}
static int32_t peak(const int32_t *a) { int32_t p = 0; for (uint32_t i = 0; i < RN * CTL; i++) { int32_t v = a[i] < 0 ? -a[i] : a[i]; if (v > p) p = v; } return p; }
static uint32_t event_of(uint8_t note)
{
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel && recording[i].note == note) return i;
    return RECORD_MAX;
}
enum { E_VEL, E_DELETE };
/* one case: the chord, the edit (a velocity delta, or the note's deletion) of `touched`; then the checks */
static void scenario(const char *name, const uint8_t *notes, const uint8_t *vels, uint32_t n, int flag, uint32_t touched, int kind, int32_t delta)
{
    uint32_t before[128], untouched_same = 1, k, c, m = 0;
    uint8_t en[4], ev[4];
    int32_t edited = 0;
    char buf[200];
    make_cell(notes, vels, n, flag);
    render(R1);
    memcpy(before, played, sizeof before);
    snprintf(buf, sizeof buf, "%s: the chord sounds, every note is played", name);
    for (k = 0, c = 1; k < n; k++) c &= before[notes[k]] != 0;
    chk(buf, peak(R1) > 500 && c);
    make_cell(notes, vels, n, flag);
    c = event_of((uint8_t)touched);
    ui.note_pick = (uint16_t)(c + 1u); note_assign(&ui.note_identity, &recording[c]); ui.note_generation = recording_generation;   /* (the pick, as pick_event makes it) */
    if (kind == E_VEL) { notes_event_edit(3, delta); edited = recording[c].vel; }
    else notes_delete_edit();
    render(R1);
    for (k = 0; k < n; k++) if (notes[k] != touched && played[notes[k]] != before[notes[k]]) untouched_same = 0;
    snprintf(buf, sizeof buf, "%s: VOICE  every untouched note is played at the velocity it was played at before the edit", name);
    chk(buf, untouched_same && (kind == E_DELETE ? !played[touched] : played[touched] != 0));
    for (k = 0; k < n; k++) {                           /* the expected chord: the untouched notes as they were played, the touched one as edited */
        if (notes[k] == touched) { if (kind == E_VEL) { en[m] = notes[k]; ev[m++] = (uint8_t)edited; } continue; }
        en[m] = notes[k]; ev[m++] = (uint8_t)before[notes[k]];
    }
    make_cell(en, ev, m, 0);
    render(R2);
    snprintf(buf, sizeof buf, "%s: RENDER the render after the edit is the expected chord's, sample for sample", name);
    chk(buf, !memcmp(R1, R2, sizeof R1));
}
int main(void)
{
    static const uint8_t N3[3] = {60, 64, 67}, V3[3] = {100, 90, 120}, V4[3] = {100, 90, 100};
    scenario("edit the accented note (120 -> 80)", N3, V3, 3, 0, 67, E_VEL, -40);
    scenario("delete the accented note", N3, V3, 3, 0, 67, E_DELETE, 0);
    scenario("edit an unaccented note (90 -> 60) beside an accented one", N3, V3, 3, 0, 64, E_VEL, -30);
    scenario("a step accent on (a project's from before): edit one note (100 -> 60)", N3, V4, 3, 1, 60, E_VEL, -40);
    scenario("a step accent on: delete one note", N3, V4, 3, 1, 64, E_DELETE, 0);
    {   static const uint8_t V5[3] = {100, 115, 100}, V6[3] = {100, 127, 100};
        make_cell(N3, V5, 3, 0); render(R1);
        make_cell(N3, V6, 3, 0); render(R2);
        chk("OWN     a note at velocity 115 plays as one at 127 (its own accent), the others at their own velocities", !memcmp(R1, R2, sizeof R1));
    }
    printf("accent_own: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
