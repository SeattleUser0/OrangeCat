/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Control-note RECORDING switched on (Round 24, Session A; UI_BUILD.md "Session A, Round 24"): Session A's findings of Round 23, each with a
 * check that fails on the Round 23 source built with -DOC_CTL_REC=1 (marked F1 .. F5), and the side effects of the switch (S..). The rule under test: a control note
 * (KICK, HOLD, DIVE, RISE: a flagged timed note 124 to 127) OWNS NO STEP CELL. Recording one writes no step; no editor shows, picks, auditions or changes one.
 *   F1   the step grid is byte for byte what it was after a pass of KICK and HOLD over manual steps: a POLY track (a chord of four, a tie, an accent, a chance), a MONO track,
 *        a drum track (no accent set); no hold is tied
 *   F2   a track that is not a DUB track (ECHO 0) records nothing from the keys or CC 18 to 21; with the DUB page closed the key is a note and CC still records on a DUB track
 *   F3   NOTES never picks a control note, never auditions one, and its knobs (STEP, PITCH, LEN, VEL, the delete) leave it byte for byte, on a synth and on a drum track; the
 *        tracker's and GRID's note order does not hold it; the pictures of STEP, NOTES, TRACKER and GRID are the same with it and without it
 *   F4   the release closes the note after the selection changed while the key was held (the key and the CC)
 *   F5a  a release the store refuses on a retrigger leaves no count behind (the trigger is up after the stop); needs the test hook, see the note at q_fail
 *   F5b  a strike whose gate ends inside its own block (a step boundary splits the block) is handed to the kernel: it renders as the same strike with a long gate
 *   F5c  an ordinary note 125 at the same step and time as HOLD is a second event; each key closes its own
 *   F6   a step note 124 to 127 in a control note's cell still reaches the engine (the pitch index took the control note for its duplicate)
 *   F7   the bound: 3,000 CC edges record at most RN_CTL_MAX control notes in the bank and leave the store to the notes
 *   S1   no control note ever reaches an engine in the replay of F1's pass; dub_rec follows each gate
 *   S2   undo and redo of the take; EDIT held erases it with its step; a pattern copy, double and clear; the pool (save, load into another bank); a project save and load
 *   S3   the note tools on a pattern that holds control notes (a range moved, quantized, deleted): the control notes are byte for byte, the notes moved
 *   S4   record quantize moves a control note to the grid and writes no step
 *   S5   HOLD held across the punch-in and through the count-in is recorded from the start, KICK is not; a latched second pass adds to the first
 *   S6   the switch off (-DOC_CTL_REC=0): the keys and the CCs record nothing, a stored control note plays and reaches no engine
 * The independent review of the first cut (R..; each of R1 to R4 fails on the commit before it, c4c6d09a):
 *   R1   a key with the playhead past a LEN just cut (step 64, LEN 64 -> 48) stores a valid event inside LEN, an ordinary note and HOLD; the project saves and loads with it,
 *        the packed record too; and an event the check refuses, however it came to be, is left out of a saved project instead of making it refuse to load
 *   R2   a bank whose only events are control notes: SHIFT, REVERSE and the lane paste run (the control notes stay where they are in time), Capture does not reset LEN; a bank
 *        with an ordinary recorded note is still refused
 *   R3   record quantize refused for the take's notes leaves the take's control notes where they are too
 *   R4   a cell as a Round 23 development build with OC_CTL_REC=1 left it (the step note 125, flagged, its only event the control note): the reconcile takes the number out
 *        of the step, nothing reaches the engine, the control note stays
 *   R5   the gaps: tracks 2 to 4 (dub_rec, dub_struck); the stop clears dub_struck; a pattern switch, the take's undo and a reindex under a sounding HOLD; a deep mixed queue;
 *        F6 on a MONO and a drum track; the bound through a key and a same-time replacement at the bound; a cell's events dropped (the web editor's step write) under a control note
 * Run by tests/run_tests.sh (dub_t: the DUB module's host unit is linked) with the build set, twice: the switch on (the default) and off (-DOC_CTL_REC=0). */
#define RES_HOST_MAIN_WORDS (85760u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#define DUB_IDLE 700u
/* F5a's hook (recording.c recording_queue_add): the store refuses a release while q_fail is set. The Round 23 source has no such hook; the check is shown to fail on it with
 * this one line added to it after "if (was) recording_queue_remove(i);": the same line the new source has. */
static int q_fail;
#define RECORDING_QUEUE_FULL_HOOK(tr, i) (q_fail)
#include "ui_test.c"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dub_module_host.h"

#ifdef RN_CTL_MAX
#define NEW_SRC 1
#else
#define NEW_SRC 0                                        /* the Round 23 source (the proof that a check fails on it) */
#define RN_CTL_MAX 128u
#endif
enum { K_KICK = 8, K_HOLD = 10, K_DIVE = 15, K_RISE = 17 };
static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("ctl_rec: %-150s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void key(uint32_t k, int down)
{
    if (down) fm1_in.notes |= 1u << k; else fm1_in.notes &= ~(1u << k);
    keyboard_block();
}
static uint32_t period_of(const track_t *t) { return seq_div_samples((uint32_t)t->p[P_SDIV]); }
static uint32_t at_step(const track_t *t, uint32_t step) { return recording_prefix(t, period_of(t), step); }
static uint32_t now;                                     /* samples since the start */
static void adv(uint32_t n) { seq_advance(n); now += n; }
static void adv_to(uint32_t at) { if (at > now) adv(at - now); }
static uint32_t n_ctl(uint32_t owner) { uint32_t n = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) n += recording[i].vel && (recording[i].owner & 31u) == owner && (recording[i].length & RN_CTL); return n; }
static uint32_t n_ord(uint32_t owner) { uint32_t n = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) n += recording[i].vel && (recording[i].owner & 31u) == owner && !(recording[i].length & RN_CTL); return n; }
static uint32_t find_ctl(uint32_t owner, uint32_t note)
{
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel && (recording[i].owner & 31u) == owner && recording[i].length == RN_CTL && recording[i].note == note) return i;
    return RECORD_MAX;
}
static int ctl_all_valid(void)
{
    for (uint32_t i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel && (!recording_valid(&recording[i]) || ((recording[i].length & RN_CTL) && (recording[i].length != RN_CTL || recording[i].note < 124u || recording[i].pitch)))) return 0;
    return 1;
}
static int eng_note(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return 1;
    return 0;
}
static int eng_ctl(const track_t *t) { return eng_note(t, 124) || eng_note(t, 125) || eng_note(t, 126) || eng_note(t, 127); }
/* a fresh unit, track 1 a DUB track, 16 steps, the DUB keys the page's; rec: armed and recording from step 0, continuous (no pattern-pass end) */
static track_t *begin(int rec)
{
    track_t *t;
    ui_power_on();
    memset(dub_trig, 0, sizeof dub_trig); memset(dubk_held, 0, sizeof dubk_held);
    fm1_in.notes = 0; kb_prev = 0; kb_dubk = 0; dub_view_on = 1;
    keyboard_block();
    song.sel = 0; t = &trk[0];
    t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100; t->p[P_QUANT] = Q_OFF; t->p[P_RECQ] = 0;
    q_fail = 0; now = 0;
    if (rec >= 0) {
        song.rec = rec ? 1u : 0u;
        seq_start(); song.rec_pass = 0; seq_advance(0);
    }
    return t;
}
static void manual(step_t *s, uint32_t n, uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t vel)
{
    memset(s, 0, sizeof *s);
    s->time = ST_NOTE; s->n = (uint8_t)n; s->note[0] = a; s->note[1] = b; s->note[2] = c; s->note[3] = d; s->vel = vel;
}
static step_t SB[NSTEP];
/* the pass every grid check plays: HOLD from early in step 2 to step 9, KICK tapped in step 5, DIVE in step 12 and RISE late in step 13 (it rounds to 14) */
static void pass(track_t *t)
{
    uint32_t p = period_of(t);
    adv_to(at_step(t, 2) + 10u); key(K_HOLD, 1);
    adv_to(at_step(t, 5) + 20u); key(K_KICK, 1); adv(64); key(K_KICK, 0);
    adv_to(at_step(t, 9) + 30u); key(K_HOLD, 0);
    adv_to(at_step(t, 12) + p / 4u); key(K_DIVE, 1); adv(200); key(K_DIVE, 0);
    adv_to(at_step(t, 13) + p * 3u / 4u); key(K_RISE, 1); adv(100); key(K_RISE, 0);
    adv_to(at_step(t, 15) + 5u);
}
#if OC_CTL_REC
static void f1_grid(void)
{
    static const char *const NAME[3] = {"a POLY track", "a MONO track", "a drum track"};
    for (uint32_t v = 0; v < 3u; v++) {
        char what[200];
        track_t *t = begin(-1);
        step_t *s = t->step;
        if (v == 1u) t->p[P_VOICE] = V_MONO;
        if (v == 2u) { set_engine_of(t, ENGI_DRUMX); t->engine = t->eng_req; t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100; }
        if (v < 2u) {
            manual(&s[2], 1, 60, 0, 0, 0, 90); s[2].probability = 50; s[2].flags = SF_ACCENT;
            s[3].time = ST_TIE;
            manual(&s[5], 4, 60, 64, 67, 71, 80); s[5].cond = 2; s[5].nudge = 6;
            manual(&s[9], 1, 62, 0, 0, 0, 70);
            manual(&s[12], 2, 55, 59, 0, 0, 100);
            manual(&s[14], 1, 48, 0, 0, 0, 64); s[14].probability = 30;
        } else {
            for (uint32_t i = 2; i < 16u; i += 3u) { memset(&s[i], 0, sizeof s[i]); s[i].time = ST_NOTE; s[i].hit = (uint8_t)(1u << (i % 3u)); s[i].vel = 90; }
            memset(&s[5], 0, sizeof s[5]); s[5].time = ST_NOTE; s[5].hit = 1; s[5].vel = 90;
            memset(&s[12], 0, sizeof s[12]); s[12].time = ST_NOTE; s[12].hit = 2; s[12].vel = 60;
        }
        memcpy(SB, s, sizeof SB);
        chk("F1   (the fixture) the track is the kind the check names", (v == 2u) == (drum_track(t) != 0) && (v != 1u || trk_vmode(t) == V_MONO));
        song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
        pass(t);
        snprintf(what, sizeof what, "F1   %s: no hold is tied while HOLD is down and after the pass (rh_n 0)", NAME[v]);
        chk(what, t->rh_n == 0 && t->rh_ties == 0);
        seq_stop(); song.rec = 0;
        snprintf(what, sizeof what, "F1   %s: after KICK, HOLD, DIVE and RISE over its manual steps every step is byte for byte what it was", NAME[v]);
        chk(what, !memcmp(SB, s, sizeof SB));
        if (memcmp(SB, s, sizeof SB))
            for (uint32_t i = 0; i < 16u; i++) if (memcmp(&SB[i], &s[i], sizeof s[i]))
                printf("ctl_rec:        step %2u was time %u n %u notes %u %u %u %u vel %u flags %02x chance %u acc %u, is time %u n %u notes %u %u %u %u vel %u flags %02x chance %u acc %u\n", i,
                       SB[i].time, SB[i].n, SB[i].note[0], SB[i].note[1], SB[i].note[2], SB[i].note[3], SB[i].vel, SB[i].flags, SB[i].probability, SB[i].acc,
                       s[i].time, s[i].n, s[i].note[0], s[i].note[1], s[i].note[2], s[i].note[3], s[i].vel, s[i].flags, s[i].probability, s[i].acc);
        snprintf(what, sizeof what, "F1   %s: four control notes (124 to 127), flagged, valid, and no other event", NAME[v]);
        chk(what, n_ctl(0) == 4u && n_ord(0) == 0u && ctl_all_valid() && find_ctl(0, 124) < RECORD_MAX && find_ctl(0, 125) < RECORD_MAX && find_ctl(0, 126) < RECORD_MAX && find_ctl(0, 127) < RECORD_MAX);
        if (find_ctl(0, 125) < RECORD_MAX && find_ctl(0, 127) < RECORD_MAX) {
            const recorded_note_t *h = &recording[find_ctl(0, 125)], *r = &recording[find_ctl(0, 127)];
            uint32_t dur = (uint32_t)(((uint64_t)h->duration << (h->owner >> 5)) * period_of(t) / RECORD_UNIT), want = at_step(t, 9) + 30u - at_step(t, 2) - 10u;
            snprintf(what, sizeof what, "F1   %s: HOLD is at step 2 for the time the key was down, RISE (late in step 13) rounds to step 14", NAME[v]);
            chk(what, (h->step & 63u) == 2u && dur + 80u > want && dur < want + 80u && (r->step & 63u) == 13u && (r->step & 64u));
        }
        if (v == 0u) {                                   /* S1: the replay */
            int eng = 0, ok = 1, seen[4] = {0, 0, 0, 0}, manual_played = 0;
            seq_start(); seq_advance(0); now = 0;
            for (uint32_t k = 0; k < 16u * period_of(t) / 40u; k++) {
                adv(40);
                eng |= eng_ctl(t);
                manual_played |= eng_note(t, 60);
                for (uint32_t w = 0; w < 4u; w++) seen[w] |= dub_rec[0][w];
                if (now > at_step(t, 3) && now < at_step(t, 9)) ok &= dub_rec[0][1] == 1;
                if (now > at_step(t, 10)) ok &= dub_rec[0][1] == 0;
            }
            chk("S1   the replay: no control note reaches the engine, the manual steps play", !eng && manual_played);
            chk("S1   the replay: dub_rec follows HOLD's gate (on from step 2 to step 9, off after) and each of the four was on at some time", ok && seen[0] && seen[1] && seen[2] && seen[3]);
            seq_stop();
            chk("S1   the stop releases every recorded half", !dub_rec[0][0] && !dub_rec[0][1] && !dub_rec[0][2] && !dub_rec[0][3]);
        }
    }
}
static void f2_track(void)
{
    track_t *t = begin(1);
    t->p[P_DUB_ECHO] = 0;
    adv(700); key(K_KICK, 1); adv(100); key(K_KICK, 0);
    midi_parameter(t, 19, 127); adv(300); midi_parameter(t, 19, 0);
    midi_parameter(t, 18, 127); midi_parameter(t, 18, 0); midi_parameter(t, 20, 127); midi_parameter(t, 20, 0); midi_parameter(t, 21, 127); midi_parameter(t, 21, 0);
    chk("F2   ECHO 0 (not a DUB track): the keys and CC 18 to 21 on the selected, recording track record nothing", n_ctl(0) == 0u && n_ord(0) == 0u);
    midi_parameter(t, 19, 127);
    chk("F2   .. and the live trigger is still written (it sounds as before, when DUB is turned on)", dub_trig[0][3] == 127);
    midi_parameter(t, 19, 0);
    seq_stop();
    t = begin(1);
    dub_view_on = 0;
    adv(700); key(K_KICK, 1); adv(100); key(K_KICK, 0);
    chk("F2   a DUB track with the DUB page closed: the key C#4 is a note of the track, recorded as a note, never a control note", n_ctl(0) == 0u && n_ord(0) == 1u && dub_trig[0][2] == 0);
    adv(900); midi_parameter(t, 20, 127); adv(300); midi_parameter(t, 20, 0);
    chk("F2   .. and CC 20 (a controller, a foot switch) records DIVE on the DUB track whatever page shows", n_ctl(0) == 1u && find_ctl(0, 126) < RECORD_MAX);
    seq_stop();
    t = begin(1);
    song.sel = 1;
    adv(700); midi_parameter(t, 19, 127); adv(300); midi_parameter(t, 19, 0);
    chk("F2   CC 19 for a track that records and is not the selected one records nothing (as the lanes)", n_ctl(0) == 0u);
    seq_stop();
    t = begin(1);
    adv(at_step(t, 1) + 10u); key(18, 1);               /* CUT down: the ECHO lane at 0 */
    adv_to(at_step(t, 4) + 10u); key(18, 0);            /* .. and up: the knob's value */
    adv_to(16u * period_of(t) + at_step(t, 2) + 10u);   /* the next pass (still recording): the lane plays, ECHO is 0 in steps 1 to 3 */
    {   int e = t->p[P_DUB_ECHO], own = motion_base_value(t, P_DUB_ECHO);
        printf("ctl_rec:        (in the recorded CUT, the next pass: ECHO is %d, the track's own ECHO %d)\n", e, own);
        key(K_KICK, 1); adv(64); key(K_KICK, 0);
        chk("F2   KICK inside a recorded CUT (ECHO is 0 for those steps, the track's own ECHO is not: still a DUB track) is recorded", e == 0 && own == 100 && n_ctl(0) == 1u); }
    seq_stop();
}
static uint32_t screen_hash(void)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < 240u * 240u; i++) h = (h ^ host_screen[i]) * 16777619u;
    return h;
}
static void f3_editors(void)
{
#ifndef GR_MGRID
#define GR_MGRID 0xFFFFu                                 /* (a build without GRID: the page is not there, the loop below passes it by) */
#endif
    static const uint32_t GRAPH[4] = {GR_STEPS, GR_ROLL, GR_TRACKER, GR_MGRID};
    static const char *const GNAME[4] = {"PHRASE (the step row)", "NOTES", "TRACKER", "GRID"};
    for (uint32_t v = 0; v < 2u; v++) {
        char what[220];
        const char *kind = v ? "a drum track" : "a synth track";
        track_t *t = begin(-1);
        recorded_note_t was;
        uint32_t i, sel, nth;
        if (v) { set_engine_of(t, ENGI_DRUMX); t->engine = t->eng_req; t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100; }
        song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
        adv_to(at_step(t, 4) + 12u); key(K_HOLD, 1); adv(2u * period_of(t)); key(K_HOLD, 0);
        adv(period_of(t)); seq_stop(); song.rec = 0;
        i = find_ctl(0, 125);
        snprintf(what, sizeof what, "F3   (the fixture) %s holds one control note, HOLD at step 4, and its steps are empty", kind);
        chk(what, i < RECORD_MAX && n_ctl(0) == 1u && n_ord(0) == 0u && t->step[4].time == ST_REST && !t->step[4].n);
        if (i >= RECORD_MAX) continue;
        was = recording[i];
        go_page(GR_ROLL); cursor_set(4); frame(); frame();
        sel = notes_selected(TSEL); nth = notes_nth(TSEL, 4, 0);
        snprintf(what, sizeof what, "F3   %s: NOTES on the control note's step picks nothing, and the tracker's and GRID's note order holds nothing", kind);
        chk(what, sel == RECORD_MAX && nth == RECORD_MAX && ui.cursor == 4u);
        settings_preview = 1; audition_request = 0; audition_count = 0; memset(audition_notes, 0, sizeof audition_notes);
        notes_preview();
        snprintf(what, sizeof what, "F3   %s: NOTES' audition asks the engine for no note 124 to 127", kind);
        chk(what, !(audition_request && audition_count && audition_notes[0] >= 124u));
        audition_request = 0;
        notes_event_edit(1, 2); notes_event_edit(1, -5); notes_event_edit(2, 3); notes_event_edit(2, -1); notes_event_edit(3, -20); notes_event_edit(0, 1);
        turn(EN_K2, 2); turn(EN_K3, 2); turn(EN_K4, -3); turn(EN_K1, 1); turn(EN_K1, -1);
        snprintf(what, sizeof what, "F3   %s: NOTES' PITCH, LEN, VEL and STEP knobs leave the control note byte for byte, and valid (the pool takes the phrase)", kind);
        chk(what, !memcmp(&was, &recording[i], sizeof was) && recording_valid(&recording[i]) && n_ctl(0) == 1u);
        cursor_set(4); frame();
        notes_delete_edit();
        snprintf(what, sizeof what, "F3   %s: NOTES' delete on that step does not take it", kind);
        chk(what, n_ctl(0) == 1u && recording[i].vel);
        if (!recording[i].vel || memcmp(&was, &recording[i], sizeof was)) recording_restore_note(t, i, was);
        for (uint32_t g = 0; g < 4u; g++) {
            uint32_t h1, h0, at;
            step_t st[NSTEP];
            if (v && GRAPH[g] == GR_MGRID) continue;      /* (GRID is not offered on a drum track) */
            for (at = 0; at < NPAGES && PAGES[at].graph != GRAPH[g]; at++) {}
            if (at >= NPAGES) continue;
            memcpy(st, t->step, sizeof st);
            go_page(GRAPH[g]); cursor_set(4); ui.force = 1; frame(); ui.force = 1; frame(); frame();
            h1 = screen_hash();
            recording_remove(t, i);
            go_page(GRAPH[g]); cursor_set(4); ui.force = 1; frame(); ui.force = 1; frame(); frame();
            h0 = screen_hash();
            recording_restore_note(t, i, was);
            snprintf(what, sizeof what, "F3   %s, %s on the control note's step: the picture is the one of the pattern without it (no pitch, no bar, no mark), the steps untouched", kind, GNAME[g]);
            chk(what, h1 == h0 && !memcmp(st, t->step, sizeof st));
        }
        chk("F3   .. and the control note is still there, byte for byte, after the four pages drew", !memcmp(&was, &recording[i], sizeof was) && n_ctl(0) == 1u);
    }
}
static void f4_release(void)
{
    for (uint32_t v = 0; v < 2u; v++) {
        track_t *t = begin(1);
        uint32_t i, dur;
        adv(at_step(t, 1) + 10u);
        if (v) midi_parameter(t, 19, 127); else key(K_HOLD, 1);
        adv(1000);
        song.sel = 1;                                    /* another track selected while the key is down */
        adv(1000);
        if (v) midi_parameter(t, 19, 0); else key(K_HOLD, 0);
        i = find_ctl(0, 125);
        chk(v ? "F4   CC 19 let go after another track was selected: the note is closed at the release" : "F4   HOLD let go after another track was selected: the note is closed at the release",
            i < RECORD_MAX && !(recording_flags[i] & 1u));
        adv(6000); seq_stop();
        dur = i < RECORD_MAX ? (uint32_t)(((uint64_t)recording[i].duration << (recording[i].owner >> 5)) * period_of(t) / RECORD_UNIT) : 0u;
        chk("F4   .. and its length is the time the key was down (2,000 samples), not the time to the stop (8,000)", dur > 1900u && dur < 2100u);
    }
}
static void f4_reset(void)
{
    track_t *t = begin(1);
    uint32_t i;
    adv(at_step(t, 1) + 10u); midi_parameter(t, 19, 127); adv(1000);
    midi_event(0xB0u, 0, 121, 0);                        /* Reset All Controllers: the triggers are cleared, no CC edge follows */
    i = find_ctl(0, 125);
    chk("F4   CC 121 (reset all controllers) with CC 19 down: the trigger is cleared and the note being recorded is closed with it", dub_trig[0][3] == 0 && i < RECORD_MAX && !(recording_flags[i] & 1u));
    seq_stop();
}
static void f5a_queue(void)
{
    track_t *t = begin(-1);
    recorded_note_t r = {0};
    uint32_t loop;
    t->step[1].time = ST_NOTE; t->step[1].flags = SF_RECORDED;        /* (the Round 23 source plays an event only from a recorded NOTE cell) */
    loop = 16u * period_of(t);
    r.on = 100; r.note = 125; r.vel = 127; r.owner = 0; r.step = 1; r.length = RN_CTL;
    r.duration = 40960u; r.owner |= 5u << 5;             /* 20 steps (40960 << 5 units): it sounds through the loop's end and is fired again while it does */
    recording_restore_note(t, 0, r);
    seq_start(); seq_advance(0); now = 0;
    adv_to(at_step(t, 3));
    chk("F5a  (the fixture) a stored HOLD of 20 steps in a 16-step loop sounds", dub_rec[0][1] == 1 && recording_sounding(0));
    adv_to(loop + at_step(t, 1) - 40u);
    chk("F5a  .. through the loop's end", dub_rec[0][1] == 1 && recording_sounding(0));
    q_fail = 1;                                          /* its second onset: the store refuses the release */
    adv(200);
    q_fail = 0;
    adv(8u * period_of(t));
    seq_stop();
    chk("F5a  a release the store refuses on the retrigger leaves no count: after the stop the trigger is up", dub_rec[0][1] == 0 && recording_cref[0][1] == 0);
}
/* ---- F5b: through the module's kernel ---- */
#define RN 900u
static struct { int32_t pcm[RN * CTL]; uint32_t split; } *shm;
static int32_t RA[RN * CTL], RB[RN * CTL], RC[RN * CTL];
/* one render in its own process: a DUB track with a note, the phrase playing, a stored KICK at the very start of step 3. kind 0: no KICK; 1: its gate one unit (a sample);
 * 2: its gate half a step. KICK is an edge: 1 and 2 are the same strike */
static void rend(int kind, int32_t *out, uint32_t *split)
{
    pid_t pid;
    int st = 0;
    if (!shm) shm = mmap(0, sizeof *shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    memset(shm, 0, sizeof *shm);
    fflush(stdout);
    pid = fork();
    if (!pid) {
        int32_t blk[2 * CTL];
        track_t *t = &trk[0];
        ui_power_on();
        memset(dub_trig, 0, sizeof dub_trig);
        song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.playing = 0; song.sel = 0;
        set_engine_of(t, ENGI_MACRO); apply_preset_to(t, 8);
        for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_DUB_ECHO] = 0;
        t->p[P_DUB_ECHO] = 100; t->p[P_DUB_TIML] = (int16_t)dub_t_val(6); t->p[P_DUB_TIMR] = (int16_t)dub_t_val(7);
        t->p[P_DUB_MODE] = DUB_MODE_MARK | 1; t->p[P_DUB_FDBK] = 60; t->p[P_DUB_SEND] = 127; t->p[P_DUB_FILT] = 60; t->p[P_DUB_SPRG] = 20;
        t->p[P_SLEN] = 16;
        t->step[3].time = ST_NOTE; t->step[3].flags = SF_RECORDED;    /* (the Round 23 source plays an event only from a recorded NOTE cell; no note in it) */
        if (kind) {
            recorded_note_t r = {0};
            r.on = 0; r.duration = kind == 1 ? 1u : (uint16_t)(RECORD_UNIT / 2u); r.note = 124; r.vel = 127; r.owner = 0; r.step = 3; r.length = RN_CTL;
            recording_restore_note(t, 0, r);
        }
        shm->split = at_step(t, 3) % CTL;
        for (uint32_t b = 0; b < RN; b++) {
            if (b == 300u) seq_start();
            if (b % 120u == 4u) trk_note_on(t, 60, 100);
            if (b % 120u == 64u) trk_note_off(t, 60);
            mix_block(blk, CTL);
            for (uint32_t i = 0; i < CTL; i++) shm->pcm[b * CTL + i] = blk[2 * i] + 3 * blk[2 * i + 1];
        }
        _exit(0);
    }
    waitpid(pid, &st, 0);
    memcpy(out, shm->pcm, sizeof shm->pcm);
    if (split) *split = shm->split;
}
static void f5b_strike(void)
{
    uint32_t split = 0;
    int32_t pk = 0;
    rend(0, RC, &split); rend(1, RA, 0); rend(2, RB, 0);
    for (uint32_t i = 0; i < RN * CTL; i++) { int32_t a = RB[i] < 0 ? -RB[i] : RB[i]; if (a > pk) pk = a; }
    printf("ctl_rec:        (step 3 begins %u samples into its block; the render's peak %d)\n", split, pk);
    chk("F5b  (the fixture) step 3 begins inside a block, and a KICK with a long gate changes the render", split != 0u && pk > 500 && memcmp(RB, RC, sizeof RB) != 0);
    chk("F5b  a stored KICK whose gate is one sample, at a step boundary inside the block, is handed to the kernel: the render differs from no KICK", memcmp(RA, RC, sizeof RA) != 0);
    chk("F5b  .. and is sample for sample the same strike with a gate of half a step (KICK is an edge)", !memcmp(RA, RB, sizeof RA));
}
static void f5c_same(void)
{
    track_t *t = begin(1);
    uint32_t c, o = RECORD_MAX;
    adv(at_step(t, 2) + 100u);
    key(K_HOLD, 1); input_on(t, 125, 100);               /* the same step, the same time */
    c = find_ctl(0, 125);
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel && recording[i].length != RN_CTL && recording[i].note == 125) o = i;
    chk("F5c  HOLD and an ordinary note 125 at the same step and time are two events, one flagged", n_ctl(0) == 1u && n_ord(0) == 1u && c < RECORD_MAX && o < RECORD_MAX && c != o);
    adv(500); key(K_HOLD, 0);
    chk("F5c  HOLD let go closes the control note and leaves the note 125 held", c < RECORD_MAX && o < RECORD_MAX && !(recording_flags[c] & 1u) && (recording_flags[o] & 1u));
    adv(100); key(K_HOLD, 1); c = RECORD_MAX;
    for (uint32_t i = 0; i < RECORD_MAX; i++) if (recording[i].vel && recording[i].length == RN_CTL && (recording_flags[i] & 1u)) c = i;
    adv(300); input_off(t, 125);
    chk("F5c  the note 125 let go closes the note and leaves a HOLD pressed again held", c < RECORD_MAX && o < RECORD_MAX && (recording_flags[c] & 1u) && !(recording_flags[o] & 1u));
    key(K_HOLD, 0); seq_stop();
}
static void f6_skip(void)
{
    track_t *t = begin(-1);
    int eng = 0, rec = 0;
    manual(&t->step[6], 1, 125, 0, 0, 0, 100);           /* a step note on the top of the range: an ordinary note, whatever its number */
    song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
    adv_to(at_step(t, 6) - 200u); key(K_HOLD, 1); adv(period_of(t)); key(K_HOLD, 0);    /* rounds to step 6 */
    adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
    chk("F6   (the fixture) HOLD recorded on step 6, which holds the step note 125; the step is as it was", n_ctl(0) == 1u && t->step[6].n == 1u && t->step[6].note[0] == 125u && !(t->step[6].flags & SF_RECORDED));
    for (uint32_t i = 0; i < 4u; i++) {                  /* a dense bank takes the pitch index's path: four more recorded notes elsewhere */
        seq_start(); song.rec = 1u; song.rec_pass = 0; seq_advance(0); now = 0;
        adv_to(at_step(t, 9 + i) + 5u); input_on(t, 40 + i, 90); adv(200); input_off(t, 40 + i);
        seq_stop(); song.rec = 0;
    }
    seq_start(); seq_advance(0); now = 0;
    for (uint32_t k = 0; k < 9u * period_of(t) / 40u; k++) { adv(40); if (now > at_step(t, 6) && now < at_step(t, 7)) { eng |= eng_note(t, 125); rec |= dub_rec[0][1]; } }
    seq_stop();
    chk("F6   the replay: the step note 125 reaches the engine in step 6 (it is not the control note's duplicate) and HOLD reaches dub_rec", eng && rec);
}
static void f7_flood(void)
{
    track_t *t = begin(1);
    uint32_t n, m;
    adv(at_step(t, 1));
    for (uint32_t k = 0; k < 3000u; k++) { midi_parameter(t, 19, 127); adv(3); midi_parameter(t, 19, 0); adv(2); }
    n = n_ctl(0);
    printf("ctl_rec:        (3,000 CC 19 edges in %u samples: %u control notes recorded, recording_full %u)\n", 15000u, n, recording_full);
    chk("F7   3,000 HOLD edges record at most RN_CTL_MAX (128) control notes in the bank, and the unit says RECORDING FULL", n <= RN_CTL_MAX && n >= 64u && recording_full);
    recording_full = 0;
    for (uint32_t k = 0; k < 40u; k++) { input_on(t, 50 + k, 100); adv(20); input_off(t, 50 + k); adv(20); }
    m = n_ord(0);
    chk("F7   .. and the store still takes the track's notes (40 of 40)", m == 40u && !recording_full);
    key(K_KICK, 1); key(K_KICK, 0);
    chk("F7   .. a control key past the bound still sounds live and is not recorded", n_ctl(0) == n);
    seq_stop();
    chk("F7   .. and every event is valid", ctl_all_valid());
}
static void s2_take(void)
{
    track_t *t = begin(-1);
    step_t *s = t->step;
    static recorded_note_t EV[RECORD_MAX];
    static step_t TK[NSTEP];
    uint32_t nc, i;
    manual(&s[2], 1, 60, 0, 0, 0, 90); manual(&s[5], 3, 60, 64, 67, 0, 80); manual(&s[12], 1, 55, 0, 0, 0, 100);
    memcpy(SB, s, sizeof SB);
    song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
    pass(t);
    seq_stop(); song.rec = 0;
    memcpy(EV, recording, sizeof EV); memcpy(TK, s, sizeof TK);
    nc = n_ctl(0);
    chk("S2   (the fixture) a take of four control notes over three manual steps", nc == 4u && !memcmp(SB, s, sizeof SB));
    chk("S2   EDIT held (the undo): UNDO RECORDING takes the four control notes away, the steps as before", recording_history(0) == 1 && n_ctl(0) == 0u && !memcmp(SB, s, sizeof SB));
    chk("S2   .. the redo brings them back byte for byte, flagged", recording_history(1) == 1 && n_ctl(0) == 4u && ctl_all_valid() && !memcmp(SB, s, sizeof SB) &&
        ({ int eq = 1; for (i = 0; i < RECORD_MAX; i++) eq &= !EV[i].vel || find_ctl(0, EV[i].note) < RECORD_MAX; eq; }));
    /* a pattern copy, double, clear */
    chk("S2   the pattern page counts a pattern that holds only control notes as used (a copy onto it asks, a delete has something to delete)",
        ({ step_t keep[NSTEP]; int u; memcpy(keep, s, sizeof keep); for (i = 0; i < NSTEP; i++) { memset(&s[i], 0, sizeof s[i]); s[i].time = ST_REST; } pattern_switch(t, 1); pattern_switch(t, 0); u = pattern_used(0, 0); memcpy(s, keep, sizeof keep); u; }) || !NEW_SRC);
    chk("S2   a pattern copy (bank 1 to bank 3) carries the four control notes, flagged", pattern_copy(t, 0, 2) == 0 && n_ctl(2) == 4u && n_ctl(0) == 4u && ctl_all_valid());
    {   uint32_t before = n_ctl(0);
        int rc = pattern_double(t, 0, 1);
        chk("S2   a doubled pattern (16 to 32 steps) holds each control note twice, the second 16 steps later", rc == 0 && t->p[P_SLEN] == 32 && n_ctl(0) == 2u * before && ctl_all_valid() &&
            ({ int ok = 0; for (i = 0; i < RECORD_MAX; i++) ok |= recording[i].vel && !(recording[i].owner & 31u) && recording[i].length == RN_CTL && recording[i].note == 125 && (recording[i].step & 63u) == 18u; ok; })); }
    {   int eng = 0, rec = 0;
        seq_start(); seq_advance(0); now = 0;
        for (uint32_t k = 0; k < 32u * period_of(t) / 40u; k++) { adv(40); eng |= eng_ctl(t); if (now > at_step(t, 19) && now < at_step(t, 24)) rec |= dub_rec[0][1]; }
        seq_stop();
        chk("S2   .. it plays: the copy of HOLD sounds in steps 19 to 24, nothing reaches the engine", rec && !eng); }
    /* the pool */
    pool_empty();
    chk("S2   the pool saves the phrase", ocp_save(5, 0, 0) == 0);
    chk("S2   .. and loads it into track 2's bank 3: the control notes there, flagged, the steps the phrase's", ocp_load(5, 1, 2, 1) == 0 && n_ctl(1u * NPAT + 2u) == n_ctl(0) && ctl_all_valid());
    {   pattern_t p;
        pattern_read(1, 2, &p);
        chk("S2   .. the loaded bank's steps are the saved bank's (no cell made or flagged for a control note)", !memcmp(p.step, s, sizeof p.step)); }
    /* the erase: EDIT held while recording, over steps 4 to 6 */
    {   uint32_t ed = 1u << panel.btn[B_EDIT], rec = 1u << panel.btn[B_REC], before;
        t->p[P_SLEN] = 16;
        for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel && !(recording[i].owner & 31u) && (recording[i].step & 63u) >= 16u) recording_remove(t, i);
        before = n_ctl(0);
        song.rec = 1u; song.seq_mode = 1; seq_start(); song.rec_pass = 0; seq_advance(0); now = 0;
        adv_to(at_step(t, 4) - 30u);
        fm1_in.buttons |= ed;
        seq_erase_button = (uint16_t)ed; seq_erase_generation = t->pattern_gen; seq_erase_request = 0x80000000u | ((ed | rec) << 16) | recording_owner(t);
        adv_to(at_step(t, 4) + 10u);
        key(K_DIVE, 1); key(K_DIVE, 0);                  /* a control key while the erase is held: not recorded */
        adv_to(at_step(t, 6) + 40u);
        fm1_in.buttons &= ~ed; seq_erase_request = 0;
        adv_to(at_step(t, 8)); seq_stop(); song.rec = 0;
        chk("S2   (the fixture) four control notes before the erase", before == 4u);
        printf("ctl_rec:        (after the erase: %u control notes; KICK %s, HOLD %s, DIVE %s, RISE %s; step 5 time %u n %u, step 2 n %u, step 12 n %u)\n", n_ctl(0), find_ctl(0, 124) < RECORD_MAX ? "there" : "gone",
               find_ctl(0, 125) < RECORD_MAX ? "there" : "gone", find_ctl(0, 126) < RECORD_MAX ? "there" : "gone", find_ctl(0, 127) < RECORD_MAX ? "there" : "gone", s[5].time, s[5].n, s[2].n, s[12].n);
        chk("S2   EDIT held while recording over steps 4 to 6 erases KICK (step 5) with the manual chord of its step and HOLD (sounding when the erase began, as a sounding note is), records nothing new, keeps DIVE and RISE",
            n_ctl(0) == 2u && find_ctl(0, 124) == RECORD_MAX && find_ctl(0, 125) == RECORD_MAX && find_ctl(0, 126) < RECORD_MAX && find_ctl(0, 127) < RECORD_MAX && s[5].time == ST_REST && s[2].n == 1u && s[12].n == 1u);
        chk("S2   .. and that take's undo brings KICK, HOLD and the chord back", recording_history(0) == 1 && n_ctl(0) == 4u && s[5].n == 3u); }
    chk("S2   clearing bank 3 takes its control notes with it; bank 1 keeps its own", pattern_clear(t, 2) == 0 && n_ctl(2) == 0u && n_ctl(0) == 4u);
    /* a project */
    {   uint32_t a = n_ctl(0), b = n_ctl(1u * NPAT + 2u);
        project_save(1);
        recording_reset(); for (i = 0; i < NSTEP; i++) { memset(&s[i], 0, sizeof s[i]); s[i].time = ST_REST; }
        project_load(1);
        chk("S2   a project saved and loaded holds every control note, flagged and valid, and track 1's manual steps", a == 4u && n_ctl(0) == a && n_ctl(1u * NPAT + 2u) == b && ctl_all_valid() && trk[0].step[2].n == 1u && trk[0].step[5].n == 3u);
        printf("ctl_rec:        (after the load: %u control notes in track 1's bank 1, %u in track 2's bank 3)\n", n_ctl(0), n_ctl(1u * NPAT + 2u)); }
}
static void s3_tools(void)
{
    track_t *t = begin(1);
    static recorded_note_t C[4];
    uint32_t i, idx[4], ok;
    adv_to(at_step(t, 2) + 10u); key(K_HOLD, 1);
    adv_to(at_step(t, 3) + 300u); input_on(t, 60, 100); adv(400); input_off(t, 60);
    adv_to(at_step(t, 5) + 20u); key(K_KICK, 1); adv(64); key(K_KICK, 0);
    adv_to(at_step(t, 5) + 700u); input_on(t, 64, 90); adv(300); input_off(t, 64);
    adv_to(at_step(t, 9) + 30u); key(K_HOLD, 0);
    adv_to(at_step(t, 10) + 200u); input_on(t, 125, 80); adv(300); input_off(t, 125);    /* an ordinary note on a control's number */
    adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
    idx[0] = find_ctl(0, 124); idx[1] = find_ctl(0, 125);
    chk("S3   (the fixture) three recorded notes (60, 64 and an ordinary 125) and two control notes", n_ord(0) == 3u && n_ctl(0) == 2u && idx[0] < RECORD_MAX && idx[1] < RECORD_MAX);
    if (idx[0] >= RECORD_MAX || idx[1] >= RECORD_MAX) return;
    C[0] = recording[idx[0]]; C[1] = recording[idx[1]];
    go_page(GR_ROLL); cursor_set(3); frame();
    note_quantize(RECORD_UNIT);                          /* the whole pattern to the step grid */
    ok = 1; for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel && !(recording[i].length & RN_CTL)) ok &= recording[i].on == 0u;
    chk("S3   the note tools' quantize (the whole pattern): the three notes go to the grid, the control notes are byte for byte",
        ok && n_ord(0) == 3u && !memcmp(&C[0], &recording[idx[0]], sizeof C[0]) && !memcmp(&C[1], &recording[idx[1]], sizeof C[1]));
    bulk.on = 1; bulk.anchor = 0; bulk.end = 15;
    note_move_snapped(1, RECORD_UNIT, 0);                /* the range one step later */
    ok = 0; for (i = 0; i < RECORD_MAX; i++) ok |= recording[i].vel && recording[i].note == 60 && (recording[i].step & 63u) >= 4u;
    chk("S3   a range moved a step: the notes moved, the control notes are byte for byte and no cell was made for them",
        ok && n_ord(0) == 3u && n_ctl(0) == 2u && find_ctl(0, 124) < RECORD_MAX && !memcmp(&C[0], &recording[find_ctl(0, 124)], sizeof C[0]) && !memcmp(&C[1], &recording[find_ctl(0, 125)], sizeof C[1]) &&
        !(t->step[2].flags & SF_RECORDED));
    bulk.on = 1; bulk.anchor = 0; bulk.end = 15;
    notes_bulk_delete();
    chk("S3   the range deleted: the three notes (the ordinary 125 too) are gone, the control notes stay", n_ord(0) == 0u && n_ctl(0) == 2u && ctl_all_valid());
    {   int eng = 0, rec = 0;
        seq_start(); seq_advance(0); now = 0;
        for (uint32_t k = 0; k < 12u * period_of(t) / 40u; k++) { adv(40); eng |= eng_ctl(t); rec |= dub_rec[0][1]; }
        seq_stop();
        chk("S3   .. and they play: HOLD reaches dub_rec, nothing reaches the engine", rec && !eng); }
}
static void s4_quant(void)
{
    track_t *t = begin(-1);
    uint32_t i;
    manual(&t->step[4], 2, 60, 67, 0, 0, 90); t->step[5].time = ST_TIE; manual(&t->step[8], 1, 62, 0, 0, 0, 90);
    memcpy(SB, t->step, sizeof SB);
    t->p[P_RECQ] = (int16_t)(t->p[P_SDIV] + 1);          /* the record quantize: the steps' own grid */
    song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
    adv_to(at_step(t, 4) + period_of(t) / 3u); key(K_KICK, 1); adv(64); key(K_KICK, 0);
    adv_to(at_step(t, 7) + period_of(t) * 2u / 3u); key(K_HOLD, 1); adv(900); key(K_HOLD, 0);
    adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
    i = find_ctl(0, 124);
    chk("S4   record quantize: KICK a third into step 4 is on step 4's start, HOLD two thirds into step 7 on step 8's, both flagged and valid",
        i < RECORD_MAX && recording[i].on == 0u && (recording[i].step & 63u) == 4u && find_ctl(0, 125) < RECORD_MAX && recording[find_ctl(0, 125)].on == 0u &&
        (recording[find_ctl(0, 125)].step & 63u) == 8u && ctl_all_valid() && n_ctl(0) == 2u);
    chk("S4   .. over a manual chord, its tie and a manual note: every step byte for byte, and the quantize was not refused for them", !memcmp(SB, t->step, sizeof SB) && !recording_quant_blocked);
}
static void s5_arm(void)
{
    track_t *t = begin(0);
    uint32_t loop = 16u * period_of(t), i;
    adv_to(at_step(t, 13)); key(K_HOLD, 1); key(K_KICK, 1);
    adv(100); rec_set(t, 1);                              /* armed while it plays: recording starts at the pattern's start */
    chk("S5   (the fixture) armed while playing: waiting for the pattern's start, HOLD and KICK down, nothing recorded", (song.rec_wait & 1u) && !rec_on(t) && n_ctl(0) == 0u);
    adv_to(loop + 200u);
    i = find_ctl(0, 125);
    chk("S5   HOLD held across the punch-in is recorded from step 1 (open), KICK (a strike, its edge before the take) is not",
        rec_on(t) && n_ctl(0) == 1u && i < RECORD_MAX && (recording[i].step & 63u) == 0u && (recording_flags[i] & 1u));
    key(K_KICK, 0); adv_to(loop + at_step(t, 3)); key(K_HOLD, 0);
    song.rec_latch |= 1u;
    adv_to(loop + at_step(t, 6) + 10u); key(K_RISE, 1); adv(200); key(K_RISE, 0);
    adv_to(2u * loop + at_step(t, 1));
    chk("S5   REC latched: the take goes on into the next pass", rec_on(t) && n_ctl(0) == 2u);
    {   int rec = 0;                                     /* the first pass's HOLD plays while the second pass records over it */
        uint32_t k;
        for (k = 0; k < 40u; k++) { adv(40); rec |= dub_rec[0][1]; }
        adv_to(2u * loop + at_step(t, 9) + 10u); key(K_DIVE, 1); adv(300); key(K_DIVE, 0);
        chk("S5   .. the first pass's HOLD plays (dub_rec) while the second records DIVE over it: three control notes, the steps untouched",
            rec && n_ctl(0) == 3u && find_ctl(0, 126) < RECORD_MAX && ({ int e = 1; for (k = 0; k < 16u; k++) e &= t->step[k].time == ST_REST && !t->step[k].n; e; })); }
    song.rec_latch = 0;
    adv_to(3u * loop + 100u);
    chk("S5   the latch off: the take ends at the pattern's end", !rec_on(t) && !(song.rec & 1u));
    seq_stop();
    /* the count-in */
    t = begin(-1);
    settings_countin = 1; song.rec = 1u; transport_req = 1;
    events_block(CTL);
    chk("S5   (the fixture) PLAY with REC armed counts in", seq_counting() && !song.playing);
    fm1_in.notes |= 1u << K_HOLD;
    for (i = 0; i < 20000u && !song.playing; i++) events_block(CTL);
    chk("S5   HOLD pressed in the count-in and still down at the start is recorded from step 1", song.playing && n_ctl(0) == 1u && find_ctl(0, 125) < RECORD_MAX && (recording[find_ctl(0, 125)].step & 63u) == 0u);
    for (i = 0; i < 200u; i++) events_block(CTL);
    fm1_in.notes &= ~(1u << K_HOLD);
    for (i = 0; i < 50u; i++) events_block(CTL);
    chk("S5   .. and closed when the key goes up", find_ctl(0, 125) < RECORD_MAX && !(recording_flags[find_ctl(0, 125)] & 1u));
    seq_stop(); settings_countin = 0;
}
/* ---- the review (R1 .. R5) ---- */
static void stored(track_t *t, uint32_t slot, uint32_t note, uint32_t step, uint32_t on, uint32_t dur, uint32_t exp, int ctl)
{
    recorded_note_t r = {0};
    r.on = (uint16_t)on; r.note = (uint8_t)note; r.vel = ctl ? 127 : 100; r.owner = (uint8_t)(recording_owner(t) | (exp << 5)); r.step = (uint8_t)step;
    r.duration = (uint16_t)dur; r.length = ctl ? RN_CTL : 0;
    recording_restore_note(t, slot, r);
    if (!ctl) { t->step[step].time = ST_NOTE; t->step[step].flags |= SF_RECORDED; }
}
static uint32_t watch_sum(void)
{
    uint32_t s = 0;
    for (uint32_t k = 0; k < NTRK; k++) for (uint32_t v = 0; v < NSTEP; v++) s += recording_watch[k][v];
    return s;
}
static uint32_t cref_sum(void) { uint32_t s = 0; for (uint32_t k = 0; k < NTRK; k++) for (uint32_t w = 0; w < 4; w++) s += recording_cref[k][w]; return s; }
static uint32_t drec_sum(void) { uint32_t s = 0; for (uint32_t k = 0; k < 4; k++) for (uint32_t w = 0; w < 4; w++) s += dub_rec[k][w]; return s; }
static uint32_t sounding_n(int ctl) { uint32_t s = 0; for (uint32_t i = 0; i < RECORD_MAX; i++) s += recording_sounding(i) && ((recording[i].length & RN_CTL) != 0) == ctl; return s; }
static int quiet(void)
{
    uint32_t h = 0;
    for (uint32_t k = 0; k < NTRK; k++) h += recording_heap_count[k];
    return !h && !recording_heap_taken && !sounding_n(0) && !sounding_n(1) && !cref_sum() && !drec_sum() && !watch_sum();
}
static uint32_t struck_any(void)
{
#if NEW_SRC
    return (uint32_t)(dub_struck[0] | dub_struck[1] | dub_struck[2] | dub_struck[3]);
#else
    return 0;
#endif
}
static void r1_len(void)
{
    static project_t q;
    static project_store_t st;
    for (int v = 0; v < 2; v++) {
        char what[220];
        const char *kind = v ? "an ordinary note" : "HOLD";
        track_t *t = begin(-1);
        uint32_t i, e = RECORD_MAX, inv = 0, n = 0;
        t->p[P_SLEN] = 64;
        song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0); now = 0;
        adv_to(at_step(t, 63) + 20u);
        t->p[P_SLEN] = 48;                               /* LEN cut with the playhead on step 64 */
        if (v) { input_on(t, 60, 100); adv(30); input_off(t, 60); } else { key(K_HOLD, 1); adv(30); key(K_HOLD, 0); }
        for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel) { n++; e = i; inv += !recording_valid(&recording[i]); }
        snprintf(what, sizeof what, "R1   %s with the playhead on step 64 and LEN just cut to 48: one event, valid, on step 16 (63 mod 48), where the step row puts it", kind);
        chk(what, n == 1u && !inv && e < RECORD_MAX && (recording[e].step & 63u) == 15u && recording_view(t, &recording[e]) < 48u);
        if (e < RECORD_MAX) printf("ctl_rec:        (%s: step byte 0x%02x, on %u, view %u; the step row's cell 15 is time %u flags 0x%02x)\n", kind, recording[e].step, recording[e].on, recording_view(t, &recording[e]), t->step[15].time, t->step[15].flags);
        if (v) chk("R1   .. the ordinary note has its recorded cell (step 16) and is what NOTES edits there", e < RECORD_MAX && recording_active(t, e) && t->step[15].time == ST_NOTE && (t->step[15].flags & SF_RECORDED) && t->step[15].note[0] == 60);
        adv(2000); seq_stop(); song.rec = 0;
        project_capture(&q);
        snprintf(what, sizeof what, "R1   %s: the project captured after it passes the load's check and packs (the stored record)", kind);
        chk(what, ({ int ok = 1; for (i = 0; i < RECORD_MAX; i++) ok &= recording_valid(&q.recording[i]); ok; }) && proj_pack(&st, &q) != 0);
        project_save(2);
        recording_reset(); for (i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; } t->p[P_SLEN] = 16;
        project_load(2);
        snprintf(what, sizeof what, "R1   %s: saved and loaded: the project loads (LEN 48 back) and holds the event", kind);
        chk(what, trk[0].p[P_SLEN] == 48 && n_ctl(0) + n_ord(0) == 1u && ctl_all_valid());
    }
    {   /* the second guard: an event the check refuses, however it got there */
        track_t *t = begin(-1);
        uint32_t i;
        stored(t, 0, 60, 3, 100, 30000, 0, 0); stored(t, 1, 125, 5, 100, 30000, 0, 1); stored(t, 2, 62, 7, 100, 30000, 0, 0);
        recording[1].step = 0x7Fu;                       /* view 64 */
        recording[2].pitch = 40;
        chk("R1   (the fixture) two events the check refuses (a step past the row, a pitch out of range) and one good one", !recording_valid(&recording[1]) && !recording_valid(&recording[2]) && recording_valid(&recording[0]));
        project_capture(&q);
        chk("R1   the project capture leaves the refused events out and keeps the good one: the capture passes the load's check and packs",
            ({ int ok = 1; for (i = 0; i < RECORD_MAX; i++) ok &= recording_valid(&q.recording[i]); ok; }) && !q.recording[1].vel && !q.recording[2].vel && q.recording[0].vel && proj_pack(&st, &q) != 0);
        recording[1].step = 5; recording[2].pitch = 0;
        project_save(2);
        recording[1].step = 0x7Fu;
        project_save(3);
        recording_reset(); t->p[P_SLEN] = 7;
        project_load(3);
        chk("R1   .. a project saved with a refused event in the store loads: LEN back, the good notes there", trk[0].p[P_SLEN] == 16 && n_ord(0) == 2u && n_ctl(0) == 0u);
    }
}
static void r2_tools(void)
{
    track_t *t = begin(1);
    recorded_note_t was;
    uint32_t i;
    int rc;
    adv_to(at_step(t, 2) + 10u); key(K_KICK, 1); adv(50); key(K_KICK, 0);
    adv_to(at_step(t, 4)); seq_stop(); song.rec = 0;
    manual(&t->step[6], 1, 60, 0, 0, 0, 90);
    i = find_ctl(0, 124);
    chk("R2   (the fixture) a bank with a manual step 7 and one KICK on step 3, nothing NOTES can pick", i < RECORD_MAX && n_ctl(0) == 1u && n_ord(0) == 0u && !notes_have_recording(t));
    if (i >= RECORD_MAX) return;
    was = recording[i];
    rc = seq_tool(t, ST_SHIFT_R);
    chk("R2   SHIFT RIGHT runs on it (it said RECORDED NOTES: USE NOTES): the step is on 8, the KICK byte for byte where it was in time", rc == 0 && t->step[7].n == 1u && t->step[7].note[0] == 60 && !t->step[6].n && !memcmp(&was, &recording[i], sizeof was));
    rc = seq_tool(t, ST_SHIFT_L);
    chk("R2   SHIFT LEFT: the step back on 7, the KICK untouched", rc == 0 && t->step[6].n == 1u && !t->step[7].n && !memcmp(&was, &recording[i], sizeof was));
    rc = seq_tool(t, ST_REVERSE);
    chk("R2   REVERSE: the step on 10 (16 - 7 + 1), the KICK untouched", rc == 0 && t->step[9].n == 1u && !t->step[6].n && !memcmp(&was, &recording[i], sizeof was) && n_ctl(0) == 1u);
    stored(t, 40, 64, 12, 0, 30000, 0, 0);
    chk("R2   with an ordinary recorded note in the bank the tools still leave it to NOTES", seq_tool(t, ST_SHIFT_R) == 1);
    /* the lane paste on a drum track */
    t = begin(-1);
    set_engine_of(t, ENGI_DRUMX); t->engine = t->eng_req; t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100;
    stored(t, 0, 124, 3, 100, 30000, 0, 1);
    for (i = 0; i < 16u; i += 4u) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_NOTE; t->step[i].hit = 1; t->step[i].vel = 90; }
    ui.lane = 0; st_lane_copy(); ui.lane = 2; ui.msg_t = 0; st_lane_paste();
    chk("R2   the lane paste runs on a drum pattern that holds a KICK control note: lane 1's four hits are on lane 3, the control note untouched",
        !msg_is("RECORDED NOTES: USE NOTES") && (t->step[0].hit & 4u) && (t->step[4].hit & 4u) && (t->step[8].hit & 4u) && (t->step[12].hit & 4u) && n_ctl(0) == 1u);
    /* Capture */
    t = begin(-1);
    t->p[P_SLEN] = 64;
    stored(t, 0, 125, 40, 100, 30000, 2, 1); stored(t, 1, 124, 50, 0, 100, 0, 1);
    seq_start(); seq_advance(0); now = 0;
    adv_to(at_step(t, 1) + 10u); input_on(t, 60, 100); adv(300); input_off(t, 60);
    adv_to(at_step(t, 2) + 10u); input_on(t, 64, 100); adv(300); input_off(t, 64);
    adv_to(at_step(t, 4));
    ui.msg_t = 0; capture_take();
    printf("ctl_rec:        (Capture of two played notes into a 64-step bank that holds only two control notes: LEN %d, %u notes, message %s)\n", t->p[P_SLEN], n_ord(0), ui.msg_t ? ui.msg : "none");
    chk("R2   Capture into a bank that holds only control notes (steps 41 and 51 of 64) keeps LEN 64 (it took the bank for empty and set LEN 16: they went dormant) and takes the notes",
        t->p[P_SLEN] == 64 && n_ord(0) == 2u && n_ctl(0) == 2u);
    seq_stop();
}
static void r3_quant(void)
{
    track_t *t = begin(-1);
    uint32_t i, c, h, o = RECORD_MAX;
    uint32_t q;
    manual(&t->step[6], 1, 62, 0, 0, 0, 90);            /* a manual note on step 7 */
    for (q = 1; q < 16u && !(seq_div_samples(q - 1u) + 2u >= 2u * period_of(t) && seq_div_samples(q - 1u) <= 2u * period_of(t) + 2u); q++) {}
    chk("R3   (the fixture) a record quantize grid of two steps exists", q < 16u);
    t->p[P_RECQ] = (int16_t)q;
    recording_quant_blocked = 0;
    song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0); now = 0;
    adv_to(at_step(t, 5) + period_of(t) / 5u); input_on(t, 60, 100); key(K_KICK, 1); adv(64); key(K_KICK, 0); input_off(t, 60);   /* played on step 6; the grid's nearest line is step 7, a manual note's */
    adv_to(at_step(t, 9) + period_of(t) / 5u); key(K_HOLD, 1); adv(300); key(K_HOLD, 0);                                          /* nearest line: step 11, free */
    adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
    c = find_ctl(0, 124); h = find_ctl(0, 125);
    for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel && !(recording[i].length & RN_CTL)) o = i;
    printf("ctl_rec:        (quantize blocked %u; note 60 step byte 0x%02x on %u; KICK step byte 0x%02x on %u; HOLD step byte 0x%02x on %u)\n", recording_quant_blocked, o < RECORD_MAX ? recording[o].step : 0xff,
           o < RECORD_MAX ? recording[o].on : 0, c < RECORD_MAX ? recording[c].step : 0xff, c < RECORD_MAX ? recording[c].on : 0, h < RECORD_MAX ? recording[h].step : 0xff, h < RECORD_MAX ? recording[h].on : 0);
    chk("R3   (the fixture) the take's note would quantize onto a manual step: the quantize is refused, the note stays where it was played, the manual step as it was",
        o < RECORD_MAX && recording_quant_blocked && recording[o].on != 0u && (recording[o].step & 63u) == 5u && t->step[6].n == 1u && t->step[6].note[0] == 62 && !(t->step[6].flags & SF_RECORDED));
    chk("R3   .. and the take's control notes stay where they were played too (KICK in step 6, HOLD in step 10): the take is not quantized in part",
        c < RECORD_MAX && h < RECORD_MAX && recording[c].on != 0u && (recording[c].step & 63u) == 5u && recording[h].on != 0u && (recording[h].step & 63u) == 9u);
}
static void r4_old_cell(void)
{
#if OC_ONE_AUTH
    for (int v = 0; v < 2; v++) {
        track_t *t = begin(-1);
        int eng = 0, rec = 0;
        manual(&t->step[6], v ? 2 : 1, 125, v ? 60 : 0, 0, 0, 127); t->step[6].flags = SF_RECORDED;      /* as rec_note left it */
        stored(t, 0, 125, 6, 100, 30000, 0, 1);
        fm1_irq_off(); recording_reconcile(); fm1_irq_on();
        printf("ctl_rec:        (after the reconcile: step 7 time %u n %u note %u flags 0x%02x, %u control notes)\n", t->step[6].time, t->step[6].n, t->step[6].note[0], t->step[6].flags, n_ctl(0));
        seq_start(); seq_advance(0); now = 0;
        for (uint32_t k = 0; k < 10u * period_of(t) / 40u; k++) { adv(40); eng |= eng_ctl(t); rec |= dub_rec[0][1]; }
        seq_stop();
        chk(v ? "R4   a Round 23 cell (step notes 125 and 60, flagged, its only event HOLD) through the reconcile: 125 is out of the step, the 60 stays a manual note, HOLD plays, no engine gets 125"
              : "R4   a Round 23 cell (the step note 125, flagged, its only event HOLD) through the reconcile: the step is empty, HOLD stays and plays, no engine gets a note 124 to 127",
            !eng && rec && n_ctl(0) == 1u && !(t->step[6].flags & SF_RECORDED) && (v ? t->step[6].n == 1u && t->step[6].note[0] == 60 : t->step[6].n == 0u && t->step[6].time == ST_REST));
    }
#endif
}
static void r5_gaps(void)
{
    /* a track other than track 1 */
    for (uint32_t k = 1; k < NTRK; k++) {
        char what[200];
        track_t *t;
        int own = 0, other = 0, eng = 0;
        uint32_t ow, st = 0;
        begin(-1);
        song.sel = (uint8_t)k; t = &trk[k]; ow = k * NPAT + t->pattern;
        t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100; t->p[P_QUANT] = Q_OFF; t->p[P_RECQ] = 0;
        song.rec = (uint8_t)(1u << k); seq_start(); song.rec_pass = 0; seq_advance(0); now = 0;
        adv_to(at_step(t, 2) + 10u); key(K_HOLD, 1); adv_to(at_step(t, 5)); key(K_HOLD, 0);
        adv_to(at_step(t, 7) + 10u); midi_parameter(t, 21, 127); adv(200); midi_parameter(t, 21, 0);
        adv_to(at_step(t, 9)); seq_stop(); song.rec = 0;
        snprintf(what, sizeof what, "R5   track %u: HOLD (the key) and RISE (CC 21) record in its bank, the live triggers were its own", k + 1u);
        chk(what, n_ctl(ow) == 2u && n_ctl(0) == 0u && find_ctl(ow, 125) < RECORD_MAX && find_ctl(ow, 127) < RECORD_MAX);
        seq_start(); seq_advance(0); now = 0;
        for (uint32_t j = 0; j < 10u * period_of(t) / 40u; j++) {
            adv(40); eng |= eng_ctl(t);
            own |= dub_rec[k][1] | (dub_rec[k][3] << 1);
#if NEW_SRC
            st |= dub_struck[k];
            for (uint32_t x = 0; x < 4u; x++) if (x != k) other |= dub_struck[x];
#endif
            for (uint32_t x = 0; x < 4u; x++) if (x != k) other |= dub_rec[x][0] | dub_rec[x][1] | dub_rec[x][2] | dub_rec[x][3];
        }
        snprintf(what, sizeof what, "R5   track %u: the replay drives dub_rec[%u] and dub_struck[%u] (HOLD and RISE), no other track's, no engine", k + 1u, k, k);
        chk(what, own == 3 && !other && !eng && (!NEW_SRC || st == 0x0Au));
        seq_stop();
        snprintf(what, sizeof what, "R5   track %u: the stop leaves nothing down, queued or struck", k + 1u);
        chk(what, quiet() && !struck_any());
    }
    {   /* the stop clears dub_struck */
        track_t *t = begin(-1);
        stored(t, 0, 124, 2, 0, 10, 0, 1);
        seq_start(); seq_advance(0); now = 0;
        adv_to(at_step(t, 2) + 4u);
#if NEW_SRC
        chk("R5   (the fixture) a KICK just struck: its bit waits for the block's read", dub_struck[0] == 1u);
#endif
        seq_stop();
        chk("R5   the stop clears dub_struck: a strike of the stop's own block is not handed to the kernel after it", !struck_any() && quiet());
    }
    {   /* a pattern switch under a sounding HOLD, and back */
        track_t *t = begin(-1);
        uint32_t k, seen = 0, maxc = 0;
        stored(t, 0, 125, 2, 100, 49152, 3, 1);          /* 6 steps */
        seq_start(); seq_advance(0); now = 0;
        adv_to(at_step(t, 4));
        chk("R5   (the fixture) a stored HOLD of 6 steps sounds", dub_rec[0][1] == 1 && recording_cref[0][1] == 1);
        pattern_switch(t, 1); adv(200);
        chk("R5   a pattern switch under a sounding HOLD: the trigger is up, nothing queued or counted", quiet());
        pattern_switch(t, 0);
        for (k = 0; k < 40u * period_of(t) / 16u; k++) { adv(16); if (t->seq_idx >= 3 && t->seq_idx <= 6) seen |= dub_rec[0][1]; if (recording_cref[0][1] > maxc) maxc = recording_cref[0][1]; if (recording_heap_count[0] > 1) maxc = 99; }
        chk("R5   .. back on the bank: HOLD sounds in the passes that follow, counted once", seen && maxc == 1u);
        seq_stop();
        chk("R5   .. the stop: quiet", quiet());
    }
    {   /* a reindex, then the take's undo and redo, under its sounding HOLD */
        track_t *t = begin(1);
        adv_to(at_step(t, 2) + 10u); key(K_HOLD, 1); adv_to(at_step(t, 8)); key(K_HOLD, 0);
        adv_to(at_step(t, 10)); key(K_KICK, 1); adv(50); key(K_KICK, 0);
        adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
        seq_start(); seq_advance(0); now = 0;
        adv_to(at_step(t, 4));
        fm1_irq_off(); recording_reindex(); fm1_irq_on();
        chk("R5   a reindex under a sounding HOLD: counted once, still down, one release queued", n_ctl(0) == 2u && dub_rec[0][1] == 1 && recording_cref[0][1] == 1 && recording_heap_count[0] == 1 && !watch_sum());
        adv(100);
        chk("R5   the take's undo under its sounding HOLD: the trigger up, nothing queued, no control note left", recording_history(0) == 1 && quiet() && n_ctl(0) == 0u);
        adv_to(16u * period_of(t) + at_step(t, 1));
        chk("R5   .. the redo while it plays", recording_history(1) == 1 && n_ctl(0) == 2u);
        adv_to(16u * period_of(t) + at_step(t, 4));
        chk("R5   .. HOLD is back and sounds on the next pass, counted once", dub_rec[0][1] == 1 && recording_cref[0][1] == 1);
        adv_to(16u * period_of(t) + at_step(t, 9));
        chk("R5   .. and is released at its end", quiet());
        seq_stop();
    }
    {   /* a deep queue, notes and control notes mixed; a cell's events dropped under it (the web editor's step write: editor.c ED_STEP_SET calls events_drop_view) */
        track_t *t = begin(-1);
        uint32_t i, a, b;
        for (i = 0; i < 60u; i++) stored(t, i, 36 + i, 1 + (i % 3u), 5 + i * 7u, 49152, 3, 0);
        for (i = 0; i < 24u; i++) stored(t, 100 + i, 124 + (i & 3u), 1 + (i % 4u), 9 + i * 11u, 40000 + i * 37u, 3, 1);
        seq_start(); seq_advance(0); now = 0;
        adv_to(at_step(t, 5));
        a = sounding_n(0); b = sounding_n(1);
        printf("ctl_rec:        (the deep queue: %u notes and %u control notes sound, %u queued, %u chunks taken, %u cells' watch counts)\n", a, b, recording_heap_count[0], recording_heap_taken, watch_sum());
        chk("R5   a deep mixed queue (more than 32 releases): every sounding event queued, the control count is the control notes sounding, the watch counts the notes only",
            recording_heap_count[0] > 32u && recording_heap_count[0] == a + b && b >= 12u && cref_sum() == b && watch_sum() == a && recording_heap_taken >= 1u);
        events_drop_view(t, 2);
        fm1_irq_off(); step_clear(&t->step[2]); fm1_irq_on();
        adv(64);
        chk("R5   .. a cell's events dropped and the cell cleared under it (the web editor's step write): its 20 notes are gone and silent, every control note is still there and still counted",
            n_ord(0) == 40u && n_ctl(0) == 24u && sounding_n(1) == b && cref_sum() == b && sounding_n(0) == a - 20u && watch_sum() == a - 20u && recording_heap_count[0] == a + b - 20u);
        t->p[P_SLEN] = 3;                                /* LEN cut under it: the events of steps 4 and 5 are out of the loop */
        adv(64);
        chk("R5   .. LEN cut to 3 under it: the events past LEN are released, the counts agree", cref_sum() == sounding_n(1) && watch_sum() == sounding_n(0) && recording_heap_count[0] == sounding_n(0) + sounding_n(1));
        adv(20u * period_of(t));
        chk("R5   .. twenty steps on: the counts still agree", cref_sum() == sounding_n(1) && watch_sum() == sounding_n(0) && recording_heap_count[0] == sounding_n(0) + sounding_n(1));
        seq_stop();
        chk("R5   .. the stop: quiet", quiet() && !struck_any());
    }
    /* F6 on a MONO and a drum track */
    for (int v = 1; v < 3; v++) {
        track_t *t = begin(-1);
        int eng = 0, rec = 0;
        if (v == 1) t->p[P_VOICE] = V_MONO;
        else { set_engine_of(t, ENGI_DRUMX); t->engine = t->eng_req; t->p[P_SLEN] = 16; t->p[P_DUB_ECHO] = 100; }
        manual(&t->step[6], 1, 125, 0, 0, 0, 100);
        memcpy(SB, t->step, sizeof SB);
        song.rec = 1u; seq_start(); song.rec_pass = 0; seq_advance(0);
        adv_to(at_step(t, 6) - 200u); key(K_HOLD, 1); adv(period_of(t)); key(K_HOLD, 0);
        adv_to(at_step(t, 12)); seq_stop(); song.rec = 0;
        for (uint32_t i = 0; i < 4u; i++) stored(t, 200 + i, 40 + i, 9 + i, 5, 3000, 0, 0);     /* a dense bank: the pitch index's path */
        memcpy(SB, t->step, sizeof SB);
        seq_start(); seq_advance(0); now = 0;
        for (uint32_t k = 0; k < 9u * period_of(t) / 20u; k++) { adv(20); if (now > at_step(t, 6) && now < at_step(t, 7)) { eng |= v == 1 ? eng_note(t, 125) : (int)t->seq_n && t->seq_notes[0] == 125; rec |= dub_rec[0][1]; } }
        seq_stop();
        chk(v == 1 ? "R5   F6 on a MONO track: the step note 125 under HOLD reaches the engine in its step, HOLD reaches dub_rec, the steps as they were"
                   : "R5   F6 on a drum track: the step note 125 under HOLD is played by the step (not skipped as the control note's duplicate), HOLD reaches dub_rec, the steps as they were",
            eng && rec && n_ctl(0) == 1u && !memcmp(SB, t->step, sizeof SB));
    }
    {   /* the bound through a key, and a same-time replacement at the bound */
        track_t *t = begin(1);
        uint32_t i, last = RECORD_MAX;
        adv(at_step(t, 1));
        for (i = 0; i < 400u && n_ctl(0) < RN_CTL_MAX - 1u; i++) { key(K_KICK, 1); adv(3); key(K_KICK, 0); adv(2); }
        key(K_KICK, 1); key(K_KICK, 0);                  /* the 128th, at this very time */
        chk("R5   the bound through the key: KICK hammered records 128 control notes", n_ctl(0) == RN_CTL_MAX && !recording_full);
        key(K_KICK, 1);                                  /* the same step and time: the same slot, not a 129th */
        for (i = 0; i < RECORD_MAX; i++) if (recording[i].vel && (recording_flags[i] & 1u)) last = i;
        chk("R5   .. at the bound a press at the same step and time takes the slot of the one that is there (128, not refused, the note open)", n_ctl(0) == RN_CTL_MAX && !recording_full && last < RECORD_MAX && recording[last].length == RN_CTL);
        key(K_KICK, 0); adv(5);
        key(K_KICK, 1); key(K_KICK, 0); key(K_HOLD, 1); key(K_HOLD, 0);
        chk("R5   .. a press at a new time is refused for every control key: 128, RECORDING FULL, the live triggers still worked", n_ctl(0) == RN_CTL_MAX && recording_full && ctl_all_valid());
        recording_full = 0;
        seq_stop();
    }
}
#else
static void s6_off(void)
{
    track_t *t = begin(1);
    recorded_note_t r = {0};
    int eng = 0, rec = 0;
    pass(t);
    midi_parameter(t, 18, 127); midi_parameter(t, 18, 0); midi_parameter(t, 19, 127); adv(100); midi_parameter(t, 19, 0);
    seq_stop(); song.rec = 0;
    chk("S6   the switch off: the DUB keys and CC 18 to 21 on a recording DUB track record nothing", n_ctl(0) == 0u && n_ord(0) == 0u);
    manual(&t->step[2], 1, 60, 0, 0, 0, 90);
    memcpy(SB, t->step, sizeof SB);
    r.on = 100; r.duration = 32768u; r.owner = 2u << 5; r.note = 125; r.vel = 127; r.step = 4; r.length = RN_CTL;   /* a stored HOLD of 2 steps (32768 << 2 units) on a step with no cell */
    recording_restore_note(t, 0, r);
    r.step = 2; r.note = 124; r.duration = 10; r.owner = 0;                              /* and a KICK on a manual step */
    recording_restore_note(t, 1, r);
#if OC_ONE_AUTH
    recording_reconcile();
#endif
    chk("S6   a project's stored control notes survive the load's reconcile with no cell of their own, the steps untouched", n_ctl(0) == 2u && !memcmp(SB, t->step, sizeof SB));
    seq_start(); seq_advance(0); now = 0;
    for (uint32_t k = 0; k < 16u * period_of(t) / 40u; k++) { adv(40); eng |= eng_ctl(t); if (now > at_step(t, 5) && now < at_step(t, 6)) rec |= dub_rec[0][1]; }
    seq_stop();
    chk("S6   .. they play (HOLD reaches dub_rec in step 5), no engine gets a note 124 to 127, the steps are byte for byte", rec && !eng && !memcmp(SB, t->step, sizeof SB) && !dub_rec[0][1]);
    project_save(1);
    recording_reset(); for (uint32_t i = 0; i < NSTEP; i++) { memset(&t->step[i], 0, sizeof t->step[i]); t->step[i].time = ST_REST; }
    project_load(1);
    eng = rec = 0;
    seq_start(); seq_advance(0); now = 0;
    for (uint32_t k = 0; k < 16u * period_of(&trk[0]) / 40u; k++) { adv(40); eng |= eng_ctl(&trk[0]); if (now > at_step(&trk[0], 5) && now < at_step(&trk[0], 6)) rec |= dub_rec[0][1]; }
    seq_stop();
    chk("S6   a project that holds control notes, saved and loaded on the build with the switch off: both are there, flagged and valid, the steps as saved, and they play as before",
        n_ctl(0) == 2u && ctl_all_valid() && !memcmp(SB, trk[0].step, sizeof SB) && rec && !eng);
}
#endif
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (dub_host_module_set() != FXM_OK) { printf("ctl_rec: the DUB module did not register\n"); return 2; }
    printf("ctl_rec: OC_CTL_REC %d, %s\n", OC_CTL_REC, NEW_SRC ? "this source" : "the Round 23 source (the checks marked F must FAIL here)");
#if OC_CTL_REC
    f1_grid(); f2_track(); f3_editors(); f4_release(); f4_reset(); f5a_queue(); f5b_strike(); f5c_same(); f6_skip(); f7_flood();
    s2_take(); s3_tools(); s4_quant(); s5_arm();
    r1_len(); r2_tools(); r3_quant(); r4_old_cell(); r5_gaps();
#else
    s6_off();
#endif
    printf("ctl_rec: %d checks, %d failed\n", nchk, bad);
    printf("%s\n", bad ? "CONTROL NOTE RECORDING TEST FAILED" : "Control note recording tests passed");
    return bad != 0;
}
