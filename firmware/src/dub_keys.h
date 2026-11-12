/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* (dub-perf; docs/orangecat/UI_DESIGN.md section 7, "Options for DUB as it is today", layout C) DUB TAPE's performance keys: while HOME's DUB page is open
 * (dub_view_on, set by ui_draw.c) the octave C4 .. B4 (keys 7 .. 18; key 0 is F3) is the view's, the other keys play the track:
 *   white C D E F G A   the throws THRW at 10 30 50 70 85 100 percent (CC 16 = 13 38 64 89 108 127): a throw replaces SEND while held; another throw key held moves to its
 *                       level; releasing one with others held returns to the highest of them; releasing the last returns SEND to the knob
 *   white B             CUT (CC 17), black C# KICK (CC 18), D# HOLD (CC 19), G# DIVE (CC 20), A# RISE (CC 21); F# is empty
 * The keys write dub_trig (dub_fields.h), the triggers MIDI CC 16 .. 21 write too, so a controller and the keys are one path.
 * Recording (the motion lanes DUB has): a throw is the SEND lane (its level while down, the knob's value after), CUT the ECHO lane at 0 (ECHO 0 mutes as CUT does: jfx_dub.c
 * dub_map; tests/dub_perf_test.c proves the renders equal), both only while the track is recording. KICK, HOLD, DIVE and RISE have no lane: (Release 3 step 7) they record as flagged control notes in the
 * timed-note store (PROJECT_FORMAT_R3.md 8.2b) and play back through dub_rec. Included by seq.c after motion.c, before keyboard_block. */
#define DUBK_FIRST 7u
#define DUBK_LAST 18u
static volatile uint8_t dub_view_on;                   /* 1: HOME's DUB page is showing (ui_draw.c), the keys are the view's */
static uint8_t dubk_held[4];                            /* [track] the throw keys held, bit n of the six (C D E F G A) */
static const uint8_t DUBK_LEVEL[6] = {13, 38, 64, 89, 108, 127};
static const uint8_t DUBK_ACT[12] = {0, 7, 1, 8, 2, 3, 0xFF, 4, 9, 5, 10, 6};   /* by the semitone from C: 0 .. 5 a throw, 6 CUT, 7 KICK, 8 HOLD, 9 DIVE, 10 RISE, 0xFF none */
static void dubk_rec(track_t *t, uint32_t id, int32_t v)
{
    if (rec_on(t) && t == TSEL) (void)motion_capture(t, id, (int16_t)v);
}
/* (Round 24) a DUB track: DUB TAPE is on for it (ECHO above 0, dub_on's rule and the DUB page's), by the patch's own value where motion moves ECHO (a recorded CUT is ECHO 0
 * for its steps). On any other track the keys and CC 18 to 21 do nothing the kernel hears, and nothing is recorded (Session A's finding 2) */
static int dubk_track(const track_t *t)
{
    return t->p[P_DUB_ECHO] > 0 || motion_base_value(t, P_DUB_ECHO) > 0;
}
/* (8.2b) a control key of a recording track is a timed note on its reserved number (124 + which), flagged: opened at the press, closed at the release.
 * (Round 24, Session A's findings 1, 2 and 4) It is recorded by recording_note_x alone, rounded to its step as a note is (rec_round): NO STEP IS WRITTEN (rec_note wrote the
 * grid as for a played note), no hold is tied (rec_hold never sees it), and only on a DUB track that is selected and recording, not frozen, not being erased. The release
 * closes the note whatever is selected or armed by then (it was skipped when the selection had changed, and the note stayed open until recording stopped); it closes only the
 * control note, never an ordinary note of the same number. In the audio interrupt (keyboard_block, midi_event), as rec_note is */
static void dubk_ctl_rec(track_t *t, uint32_t which, int down)
{
    uint32_t note = RN_CTL_FIRST + (which & 3u), next;
    if (!OC_CTL_REC) return;
    if (!down) { recording_release_x(t, note, 1u); return; }
    if (!rec_on(t) || t != TSEL || !dubk_track(t)) return;
    if (seq_erase_active(t) || ((seq_frz_lock >> trk_index(t)) & 1u)) return;
    (void)recording_note_x(t, note, 127u, rec_round(t, &next), 1u);
}
/* (Round 24) recording begins with a gate key down (the punch-in at the pattern's start, the end of the count-in): HOLD, DIVE and RISE are recorded from here, as a held note
 * is (rec_begin_held); KICK is a strike, its edge is past. The live trigger says what is down (the keys and CC 16 to 21 write it) */
static void dubk_ctl_held(track_t *t)
{
    uint32_t w;
    for (w = 1; w < 4u; w++)
        if (dub_trig[trk_index(t) % 4u][2u + w] >= 64u) dubk_ctl_rec(t, w, 1);
}
static void dub_key(uint32_t k, uint32_t tr, int down)
{
    track_t *t = &trk[tr % NTRK];
    uint32_t j = tr % 4u, a = k >= DUBK_FIRST && k <= DUBK_LAST ? DUBK_ACT[k - DUBK_FIRST] : 0xFFu, b;
    if (a == 0xFFu)
        return;
    if (a < 6u) {                                       /* a throw */
        uint32_t m;
        dubk_held[j] = (uint8_t)(down ? dubk_held[j] | (1u << a) : dubk_held[j] & ~(1u << a));
        m = dubk_held[j];
        if (down) b = a;
        else for (b = 5; b && !((m >> b) & 1u); b--) {}
        dub_trig[j][0] = m ? DUBK_LEVEL[b] : 0;
        dubk_rec(t, P_DUB_SEND, m ? DUBK_LEVEL[b] : t->p[P_DUB_SEND]);
    } else if (a == 6u) {                               /* CUT: the loop's input and the wet shut, as ECHO 0 */
        dub_trig[j][1] = down ? 127u : 0u;
        dubk_rec(t, P_DUB_ECHO, down ? 0 : t->p[P_DUB_ECHO]);
    } else {                                            /* KICK HOLD DIVE RISE: live, and (8.2b) recorded as control notes while the track records */
        dub_trig[j][a - 5u] = down ? 127u : 0u;
        dubk_ctl_rec(t, a - 7u, down);
    }
}
