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
/* (the clip paste is only built with OC_CLIP and OC_CLIP_SEL, which the build switches leave off; the BULK checks need it, so this test turns the two on where the interface steps they
 * rest on are on: the same combination tests/clip_sel_test.c runs. The legacy job, everything off, leaves them off and skips the paste pass) */
#if defined(OC_MIXER) && OC_MIXER && !defined(OC_CLIP)
#define OC_CLIP 1
#define OC_CLIP_SEL 1
#endif
static uint32_t played[128];                            /* the velocity each note's last hit was played at */
#define RECORDING_HIT_HOOK(t, i, vel, gate, late) (played[recording[i].note & 127u] = (vel))
#define RECORDING_NO_MAIN 1
#include "recording_test.c"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define RN 1400u                                        /* blocks of CTL rendered: about a second */
static int32_t R1[RN * CTL], R2[RN * CTL];
static int bad;
static void chk(const char *what, int ok) { printf("accent_own: %-124s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
/* (the notes start a few samples apart, by pitch: events of equal start time come out of the chain in the order they were put in, and an edit puts its note back at the front, which
 * changes which voice plays which note: the same sound, other phases. With distinct starts the order is the notes', whatever was edited) */
/* a recorded chord on step 0 of the first track: notes/vels; flag = a deliberate step accent (or a project's from before) */
static void make_cell(const uint8_t *notes, const uint8_t *vels, uint32_t n, int flag)
{
    track_t *t = &trk[0];
    raw_begin(); song.rec = 0; seq_stop(); song.master_q12 = 4096; panic_req = 0;
    set_engine_of(t, ENGI_MACRO); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; panic_req = 0;   /* POLY (MONO plays the last note of a chord only); an engine whose level follows the velocity (the default one does not: a render could not tell a note's velocity) */
    recording_clear(t);
    t->p[P_SLEN] = 16;
    for (uint32_t i = 0; i < n; i++) {
        recorded_note_t r = {.step = 0, .on = (uint16_t)((notes[i] - 60) * 16), .note = notes[i], .vel = vels[i], .duration = 32768, .owner = (uint8_t)recording_owner(t), .length = 0};
        recording_restore_note(t, i, r);
    }
    note_rebuild_all();
    if (flag) t->step[0].flags |= SF_ACCENT;
    go_page(GR_ROLL); cursor_set(0); frame();
}
/* Each render runs in its own fork()ed child of a process that never rendered (as tests/oc_macro_test.c), so every take starts from the same DSP state (the generators, the buses, the
 * voice clock): a render is then a function of the chord only. The child's hit log comes back in the same shared page; the parent's UI and history state is not touched by it */
static struct { int32_t pcm[RN * CTL]; uint32_t played[128]; } *shm;
static void render(int32_t *out)
{
    pid_t pid;
    int st = 0;
    if (!shm) shm = mmap(0, sizeof *shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(shm, 0, sizeof *shm);
    fflush(stdout); fflush(stderr);
    pid = fork();
    if (!pid) {
        int32_t blk[2 * CTL];
        memset(played, 0, sizeof played);
        seq_start(); seq_advance(0);
        for (uint32_t b = 0; b < RN; b++) {
            seq_advance(CTL); mix_block(blk, CTL);
            for (uint32_t i = 0; i < CTL; i++) shm->pcm[b * CTL + i] = blk[2 * i];
        }
        memcpy(shm->played, played, sizeof played);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    memcpy(out, shm->pcm, sizeof shm->pcm);
    memcpy(played, shm->played, sizeof played);
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
    /* UNDO: the edit that bakes a step accent into the other notes and clears the flag undoes COMPLETELY (the flag, every note's bytes, the step, the render) and redoes the same */
    {   static const uint8_t VU[3] = {100, 90, 100};
        step_t s_before, s_edit, s_undo, s_redo;
        recorded_note_t e_before[3], e_edit[3], e_undo[3], e_redo[3];
        static int32_t RB[RN * CTL], RE[RN * CTL];
        uint32_t k, a, ok;
        make_cell(N3, VU, 3, 1);
        s_before = TSEL->step[0];
        for (k = 0; k < 3; k++) e_before[k] = recording[event_of(N3[k])];
        render(RB);
        make_cell(N3, VU, 3, 1);
        a = event_of(60); ui.note_pick = (uint16_t)(a + 1u); note_assign(&ui.note_identity, &recording[a]); ui.note_generation = recording_generation;
        notes_event_edit(3, -40);
        step_history_end(); step_history_finish();
        s_edit = TSEL->step[0];
        for (k = 0; k < 3; k++) e_edit[k] = recording[event_of(N3[k])];
        chk("UNDO    the edit: the first note 60, the others baked to 127, the step accent gone", e_edit[0].vel == 60 && e_edit[1].vel == 127 && e_edit[2].vel == 127 && !(s_edit.flags & SF_ACCENT) && (s_before.flags & SF_ACCENT));
        render(RE);
        chk("UNDO    the edit changed the step and the sound (the test can tell)", memcmp(&s_before, &s_edit, sizeof s_before) && memcmp(RB, RE, sizeof RB));
        make_cell(N3, VU, 3, 1);                          /* the same state again, now edited and undone in one piece */
        a = event_of(60); ui.note_pick = (uint16_t)(a + 1u); note_assign(&ui.note_identity, &recording[a]); ui.note_generation = recording_generation;
        notes_event_edit(3, -40);
        step_history_end(); step_history_finish();
        ok = step_history_apply(0);
        s_undo = TSEL->step[0];
        for (k = 0; k < 3; k++) e_undo[k] = recording[event_of(N3[k])];
        chk("UNDO    EDIT held undoes it: the step's bytes (the flag among them) are as before the edit", ok && !memcmp(&s_before, &s_undo, sizeof s_before));
        chk("UNDO    .. and every note's bytes are as before the edit (the other two back from 127 to 90 and 100)", !memcmp(e_before, e_undo, sizeof e_before));
        render(R1);
        chk("UNDO    .. and the render is the render before the edit, sample for sample", !memcmp(R1, RB, sizeof R1));
        ok = step_history_apply(1);
        s_redo = TSEL->step[0];
        for (k = 0; k < 3; k++) e_redo[k] = recording[event_of(N3[k])];
        chk("UNDO    redo bakes again: the step and every note's bytes are as after the edit", ok && !memcmp(&s_edit, &s_redo, sizeof s_edit) && !memcmp(e_edit, e_redo, sizeof e_edit));
        render(R1);
        chk("UNDO    .. and the render is the render after the edit, sample for sample", !memcmp(R1, RE, sizeof R1));
        ok = step_history_apply(0);
        for (k = 0; k < 3; k++) e_undo[k] = recording[event_of(N3[k])];
        chk("UNDO    undo again after the redo: back as before the edit", ok && !memcmp(e_before, e_undo, sizeof e_before) && !memcmp(&s_before, &TSEL->step[0], sizeof s_before));
    }
    {   static const uint8_t V5[3] = {100, 115, 100}, V6[3] = {100, 127, 100};
        make_cell(N3, V5, 3, 0); render(R1);
        make_cell(N3, V6, 3, 0); render(R2);
        chk("OWN     a note at velocity 115 plays as one at 127 (its own accent), the others at their own velocities", !memcmp(R1, R2, sizeof R1));
    }
    /* BULK    the real callers that rebuild the whole overview (a bulk quantize / move through note_transform, a clip paste through notes_paste) keep the deliberate step accent of a step
     * they did not touch, and its chord byte for byte (a rebuild no longer derives the accent from the velocities, so the flag must ride through in the metadata) */
#if OC_CLIP_SEL
#define NPASS 3
#else
#define NPASS 2
#endif
    {   static const uint8_t VB[3] = {100, 90, 100};
        recorded_note_t e0[3], e1[3];
        step_t s0;
        uint32_t k, a, i;
        for (int pass = 0; pass < NPASS; pass++) {
            const char *nm = pass == 0 ? "bulk quantize (note_quantize)" : pass == 1 ? "move (note_move_snapped)" : "clip paste (notes_copy + notes_paste)";
            char buf[200];
            make_cell(N3, VB, 3, 1);
            recorded_note_t o = {.step = 5, .on = RECORD_UNIT / 8u, .note = 72, .vel = 100, .duration = 32768, .owner = (uint8_t)recording_owner(&trk[0]), .length = 0};
            for (i = 0; i < RECORD_MAX && recording[i].vel; i++);
            recording_restore_note(&trk[0], i, o);
            note_rebuild_all();
            TSEL->step[0].flags |= SF_ACCENT;
            s0 = TSEL->step[0];
            for (k = 0; k < 3; k++) e0[k] = recording[event_of(N3[k])];
            chk("BULK    setup: step 0 flagged and recorded, step 5 recorded", (TSEL->step[0].flags & SF_ACCENT) && (TSEL->step[0].flags & SF_RECORDED) && (TSEL->step[5].flags & SF_RECORDED));
            a = event_of(72); ui.note_pick = (uint16_t)(a + 1u); note_assign(&ui.note_identity, &recording[a]); ui.note_generation = recording_generation;
            cursor_set(5);
            if (pass == 0) { bulk.on = 1; bulk.anchor = 5; bulk.end = 5; note_quantize(RECORD_UNIT); bulk.on = 0; }   /* (a range of step 5 alone: the whole pattern would quantize the chord too) */
            else if (pass == 1) note_move_snapped(1, RECORD_UNIT / 4u, 0);
            else {
#if OC_CLIP_SEL
                notes_copy(); cursor_set(9); notes_paste();
#endif
            }
            snprintf(buf, sizeof buf, "BULK    %s: the other step was edited (the test reached the real caller)", nm);
            chk(buf, pass == 2 ? (TSEL->step[9].flags & SF_RECORDED) != 0 : recording[event_of(72)].on != o.on || (recording[event_of(72)].step & 63u) != 5u);
            snprintf(buf, sizeof buf, "BULK    %s: step 0 keeps its deliberate SF_ACCENT", nm);
            chk(buf, (TSEL->step[0].flags & SF_ACCENT) != 0);
            for (k = 0; k < 3; k++) e1[k] = recording[event_of(N3[k])];
            snprintf(buf, sizeof buf, "BULK    %s: the chord's notes and velocities (100 90 100) are byte for byte, step 0 as before", nm);
            chk(buf, !memcmp(e0, e1, sizeof e0) && e1[0].vel == 100 && e1[1].vel == 90 && e1[2].vel == 100 && !memcmp(&s0, &TSEL->step[0], sizeof s0));
        }
    }
    printf("accent_own: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
