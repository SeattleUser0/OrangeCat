/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix 8 (docs/orangecat/UI_BUILD.md "ui-fix 8"): slide and legato, with OC_SLIDE=1, through tests/ui_test.c's host harness: the
 * sequencer and the voices driven by mix_block (the audio interrupt's path: events_block, seq_advance, the render's glide).
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DMELODEE_USB_AUDIO=1
 *      -Ibuild/gen -Ifirmware/src -Itests -o slide_test tests/slide_test.c -lm
 * Checked, on MACRO (it reads the voice's pitch every block):
 *   MODES     POLY, MONO, LEGATO, UNISON: a step with SLIDE into the next: the same voice goes on (its envelope's stage and level
 *             not restarted), its pitch target the new note and its pitch gliding there over about 64 blocks for an octave (GLIDE 0:
 *             the slide's own rate); no second voice starts; the note-off of the new note ends it
 *   CHORD     two notes into two: voice by voice
 *   REST      a slide into a rest releases
 *   SAME      a slide into the same pitch: no retrigger
 *   GLIDE     GLIDE set with GLMODE TIME: GLIDE's own time, not the slide's rate
 *   HELD      an old note that is a live key's is not taken: a new voice starts
 *   RATCHET   a slide into a ratcheted step glides into its first part; the repeat retriggers
 *   DRUMX     an oneshot engine never slides (a new voice)
 *   RECORDED  two recorded notes that overlap, the first's step with SLIDE: the second takes over the first's voice; two that do
 *             not overlap: a new voice
 * Revisions:
 *   (ui-fix 8, branch ui-fix) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("slide: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void blocks(uint32_t n) { int32_t out[2 * CTL]; while (n--) mix_block(out, CTL); }
static uint32_t voices_on(const track_t *t) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += t->v[i].active && t->v[i].gate; return n; }
static int voice_of(const track_t *t, uint32_t note) { for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return (int)i; return -1; }
static void manual(track_t *t, uint32_t s, uint32_t note, uint32_t flags)
{
    step_t *st = &t->step[s];
    memset(st, 0, sizeof *st);
    st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96; st->flags = (uint8_t)flags;
}
static track_t *prep(uint32_t engine, uint32_t mode)
{
    track_t *t;
    ui_power_on();
    stop_transport();
    go_title("NOTES"); frame();
    t = TSEL;
    set_engine_of(t, engine);
    blocks(400);                                        /* the engine switched on the audio side */
    track_defaults_steps(t);
    t->p[P_SLEN] = 4; t->p[P_VOICE] = (int16_t)mode; t->p[P_GLIDE] = 0; t->p[P_GLMODE] = 0; t->p[P_PRIO] = 0; t->p[P_SGATE] = 64;
    t->p[P_ATK] = 0; t->p[P_DEC] = 40; t->p[P_SUS] = 100; t->p[P_REL] = 40;
    return t;
}
static void start(void) { transport_req = 1; blocks(2); }
/* run until a gated voice of note sounds (or the limit): the blocks it took, or 0 */
static uint32_t until_note(const track_t *t, uint32_t note, uint32_t limit)
{
    uint32_t n;
    for (n = 1; n <= limit; n++) { blocks(1); if (voice_of(t, note) >= 0) return n; }
    return 0;
}
static uint32_t until_step(const track_t *t, uint32_t step, uint32_t limit)
{
    uint32_t n;
    for (n = 1; n <= limit; n++) { blocks(1); if (t->seq_idx == step && song.playing) return n; }
    return 0;
}
/* one slide, 48 to 60, in the given mode: 1 when the same voice went on legato and glided */
static int slide_mode(uint32_t mode, const char *name)
{
    track_t *t = prep(ENGI_MACRO, mode);
    int v0, v1, ok;
    uint32_t stage0, glided = 0, k;
    int32_t env0, cur0;
    manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
    start();
    ok = until_note(t, 48, 400) > 0;
    v0 = voice_of(t, 48);
    blocks(40);                                         /* into the sustain */
    stage0 = t->v[v0].stage; env0 = t->v[v0].env; cur0 = t->v[v0].pitch_cur;
    ok &= until_note(t, 60, 400) > 0;
    v1 = voice_of(t, 60);
    ok &= v1 == v0 && t->v[v1].stage == stage0 && t->v[v1].env >= env0 - env0 / 8 && voices_on(t) == (mode == V_UNISON ? trk_nvoice(t) : 1u);
    ok &= t->v[v1].pitch16 == 60 * 16 && t->v[v1].pitch_cur < 60 * 16 && t->v[v1].pitch_cur >= cur0;   /* gliding up from where it was */
    for (k = 1; k <= 200u && t->v[v1].pitch_cur != t->v[v1].pitch16; k++) blocks(1);
    glided = k;
    ok &= glided >= 56u && glided <= 72u;               /* an octave at three sixteenths a block: 64 */
    if (mode == V_UNISON) {                             /* (the review's 1) the voices keep their scaled level: no jump to the step's 96 */
        for (k = 0; k < trk_nvoice(t); k++) if (t->v[k].active && t->v[k].gate) ok &= t->v[k].vel < 96u;
    }
    ok &= until_step(t, 2, 400) > 0;                    /* the rest after: released */
    blocks(2);
    ok &= voice_of(t, 60) < 0;
    printf("slide:   %s: voice %d then %d, stage %u, the octave in %u blocks, %u voices\n", name, v0, v1, (unsigned)stage0, (unsigned)glided, (unsigned)voices_on(t));
    return ok;
}

int main(void)
{
#if !OC_SLIDE
    printf("slide: built without OC_SLIDE\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* MODES */
        chk("MODES   POLY: a slide goes on in the same voice, no retrigger, an octave glide in about 64 blocks, released at the rest after", slide_mode(V_POLY, "POLY"));
        chk("MODES   MONO: the same (today MONO retriggered)", slide_mode(V_MONO, "MONO"));
        chk("MODES   LEGATO: the same", slide_mode(V_LEGATO, "LEGATO"));
        chk("MODES   UNISON: its voices together", slide_mode(V_UNISON, "UNISON"));
    }
    {   /* CHORD */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int a0, b0, ok;
        t->step[0].note[0] = 48; t->step[0].note[1] = 52; t->step[0].n = 2; t->step[0].time = ST_NOTE; t->step[0].vel = 96; t->step[0].flags = SF_SLIDE;
        t->step[1].note[0] = 55; t->step[1].note[1] = 60; t->step[1].n = 2; t->step[1].time = ST_NOTE; t->step[1].vel = 96;
        start();
        ok = until_note(t, 52, 400) > 0;
        a0 = voice_of(t, 48); b0 = voice_of(t, 52);
        ok &= until_note(t, 60, 400) > 0;
        ok &= voice_of(t, 55) == a0 && voice_of(t, 60) == b0 && voices_on(t) == 2u;
        chk("CHORD   two notes into two: voice by voice (the first to the first, the second to the second)", ok);
    }
    {   /* REST, SAME */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok, v0;
        manual(t, 0, 48, SF_SLIDE); t->step[1].time = ST_REST;
        start();
        ok = until_note(t, 48, 400) > 0 && until_step(t, 1, 400) > 0;
        blocks(2);
        ok &= voice_of(t, 48) < 0;
        chk("REST    a slide into a rest releases", ok);
        t = prep(ENGI_MACRO, V_POLY);
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 48, 0);
        start();
        ok = until_note(t, 48, 400) > 0;
        v0 = voice_of(t, 48);
        blocks(40);
        {
            uint32_t stage0 = t->v[v0].stage;
            ok &= until_step(t, 1, 400) > 0;
            blocks(2);
            ok &= voice_of(t, 48) == v0 && t->v[v0].stage == stage0 && voices_on(t) == 1u;
        }
        chk("SAME    a slide into the same pitch: the voice goes on, no retrigger (today a restart)", ok);
    }
    {   /* GLIDE: GLMODE TIME with GLIDE set: GLIDE's own time */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok, v1;
        uint32_t k;
        t->p[P_GLIDE] = 60; t->p[P_GLMODE] = 1;
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
        start();
        ok = until_note(t, 48, 400) > 0 && until_note(t, 60, 400) > 0;
        v1 = voice_of(t, 60);
        ok &= v1 >= 0 && t->v[v1].gstep > 0 && t->v[v1].gstep != 3;
        for (k = 1; k <= 1000u && t->v[v1].pitch_cur != t->v[v1].pitch16; k++) blocks(1);
        ok &= k > 72u;                                  /* slower than the slide's own rate: GLIDE 60's time */
        chk("GLIDE   GLIDE set with GLMODE TIME: GLIDE's own time, not the slide's rate", ok);
    }
    {   /* HELD: the old note is a live key's: not taken */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok;
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
        start();
        ok = until_note(t, 48, 400) > 0;
        {
            int v0 = voice_of(t, 48);
            live_held[trk_index(t)][48 >> 5] |= 1u << (48 & 31u);
            ok &= until_note(t, 60, 400) > 0;
            ok &= voice_of(t, 60) != v0 && t->v[voice_of(t, 60)].stage == 1u;   /* a fresh attack in another voice */
            live_held[trk_index(t)][48 >> 5] = 0;
        }
        chk("HELD    an old note that is a live key's is not taken over: the new note starts its own voice", ok);
    }
    {   /* RATCHET */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok, v0, v1;
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
        step_set_rflam(&t->step[1], 2);
        start();
        ok = until_note(t, 48, 400) > 0;
        v0 = voice_of(t, 48);
        ok &= until_note(t, 60, 400) > 0;
        v1 = voice_of(t, 60);
        ok &= v1 == v0;                                 /* the first part slid in */
        {
            uint32_t stage0;
            blocks(5);
            stage0 = t->v[v1].stage;
            blocks(1);
            for (uint32_t k = 0; k < 200u && !(t->v[v1].stage == 1u && stage0 != 1u); k++) { blocks(1); if (t->v[v1].stage == 1u) break; }
            ok &= t->v[v1].stage == 1u || voice_of(t, 60) != v1;   /* the repeat retriggered (a new attack, or another voice) */
        }
        chk("RATCHET a slide into a ratcheted step glides into its first part; the repeat retriggers", ok);
    }
    {   /* DRUMX */
        track_t *t = prep(ENGI_DRUMX, V_POLY);
        int ok;
        manual(t, 0, 36, SF_SLIDE); manual(t, 1, 38, 0);
        start();
        ok = until_note(t, 36, 400) > 0;
        {
            int v0 = voice_of(t, 36);
            ok &= until_note(t, 38, 400) > 0 && voice_of(t, 38) != v0 && t->v[v0].active;   /* another voice; the kick rings on */
        }
        chk("DRUMX   an oneshot engine never slides: the next hit starts its own voice", ok);
    }
    {   /* RECORDED: the overlap rule */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok, v0, v1;
        uint32_t stage0;
        recorded_note_t a = {.step = 0, .on = 0, .note = 48, .vel = 100, .duration = 49152, .owner = (uint8_t)(recording_owner(t) | (1u << 5)), .length = 1};   /* 49152 x 2: 1.5 steps, over the next */
        recorded_note_t b = {.step = 1, .on = 0, .note = 60, .vel = 100, .duration = 32768, .owner = (uint8_t)recording_owner(t), .length = 1};
        recording_restore_note(t, 0, a); recording_restore_note(t, 1, b); note_rebuild_all();
        t->step[0].flags |= SF_SLIDE;
        start();
        ok = until_note(t, 48, 400) > 0;
        v0 = voice_of(t, 48);
        blocks(40); stage0 = t->v[v0].stage;
        ok &= until_note(t, 60, 400) > 0;
        v1 = voice_of(t, 60);
        ok &= v1 == v0 && t->v[v1].stage == stage0 && voices_on(t) == 1u && recording_refs[trk_index(t)][48] == 0u && recording_refs[trk_index(t)][60] == 1u;
        ok &= until_step(t, 3, 600) > 0;
        blocks(4);
        ok &= voices_on(t) == 0u;                       /* the second's gate ended it, no stuck note */
        chk("RECORDED two recorded notes that overlap, the first's step SLIDE: the second takes over the first's voice; it ends at its own gate", ok);
        t = prep(ENGI_MACRO, V_POLY);
        a.duration = 16384; a.owner = (uint8_t)recording_owner(t);   /* a quarter step: no overlap */
        recording_restore_note(t, 0, a); recording_restore_note(t, 1, b); note_rebuild_all();
        t->step[0].flags |= SF_SLIDE;
        start();
        ok = until_note(t, 48, 400) > 0 && until_note(t, 60, 400) > 0 && t->v[voice_of(t, 60)].stage == 1u;
        chk("RECORDED two that do not overlap: no slide, the second starts its own voice", ok);
        t = prep(ENGI_MACRO, V_POLY);                   /* a recorded chord into a chord: voice by voice (the review's 7) */
        {
            recorded_note_t c1 = a, c2 = a, d1 = b, d2 = b;
            int va, vb;
            c1.duration = 49152; c1.owner = (uint8_t)(recording_owner(t) | (1u << 5)); c2 = c1; c2.note = 52; d2.note = 64;
            recording_restore_note(t, 0, c1); recording_restore_note(t, 1, c2); recording_restore_note(t, 2, d1); recording_restore_note(t, 3, d2);
            note_rebuild_all(); t->step[0].flags |= SF_SLIDE;
            start();
            ok = until_note(t, 52, 400) > 0;
            va = voice_of(t, 48); vb = voice_of(t, 52);
            ok &= until_note(t, 64, 400) > 0 && voices_on(t) == 2u && va >= 0 && vb >= 0;
            ok &= (voice_of(t, 60) == va || voice_of(t, 60) == vb) && (voice_of(t, 64) == va || voice_of(t, 64) == vb);
            chk("RECORDED a recorded chord sliding into a chord: both voices taken over, none added", ok);
        }
    }
    {   /* HITS: a melodic step that also carries a lane hit: no takeover (the pairing would reach the hit's voice: the review's 10) */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok, v0;
        manual(t, 0, 48, SF_SLIDE); t->step[0].hit = 1u; manual(t, 1, 60, 0);
        start();
        ok = until_note(t, 48, 400) > 0;
        v0 = voice_of(t, 48);
        ok &= until_note(t, 60, 400) > 0 && voice_of(t, 60) != v0;
        chk("HITS    the step before carries a lane hit: the notes start afresh (no pairing with a hit's voice)", ok);
    }
    {   /* SET: NOTES, ENV held + KNOB 4 sets and clears SLIDE on a recorded step (today a manual step's only); EDIT held undoes it; the
         * strip's VEL column says SL */
        track_t *t = prep(ENGI_MACRO, V_POLY);
        int ok;
        recorded_note_t a = {.step = 2, .on = 0, .note = 48, .vel = 100, .duration = 32768, .owner = (uint8_t)recording_owner(t), .length = 1};
        t->p[P_SLEN] = 16;
        recording_restore_note(t, 0, a); note_rebuild_all();
        cursor_set(2); frame();
        ok = notes_selected(t) == 0u && !(t->step[2].flags & SF_SLIDE);
        btn_down(B_ENV); frame();
        turn(EN_K4, 1);
        ok &= (t->step[2].flags & SF_SLIDE) != 0u && recording[0].vel == 100u;   /* (the flag, not the velocity) */
        ui.force = 1; frame();
        btn_up(B_ENV); frame();
        ok &= (t->step[2].flags & SF_SLIDE) != 0u && !ui.home && cur_page()->graph == GR_ROLL;   /* (ENV's release opens no family) */
        hold(B_EDIT); frame();
        ok &= !(t->step[2].flags & SF_SLIDE);
        btn_down(B_ENV); frame(); turn(EN_K4, -1); btn_up(B_ENV); frame();
        ok &= !(t->step[2].flags & SF_SLIDE);
        chk("SET     NOTES: ENV + KNOB 4 sets SLIDE on a recorded step, EDIT held undoes it, turned down it clears", ok);
    }
    printf("slide: %d failed\n", nbad);
    return nbad != 0;
#endif
}
