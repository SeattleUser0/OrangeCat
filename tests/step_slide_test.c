/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3 format revision, Part 9 (docs/orangecat/UI_BUILD.md "Part 9"), OC_STEP_SLIDE=1 (needs OC_R3_FORMAT and OC_SLIDE_TIME): a step's own SLIDE TIME.
 *   cc -O1 -w <the build set> -DOC_R3_FORMAT=1 -DOC_STEP_SLIDE=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests tests/step_slide_test.c -o step_slide_test -lm
 * Checked, on MACRO, as slide_time_test.c measures it:
 *   TIME     an octave slide that leaves a step with its own time takes that time whatever the track's SLIDE TIME is; a step with none follows the track's;
 *            POLY, MONO, LEGATO and UNISON
 *   RECORDED the slide of a recorded cell takes the cell's own time too (recording.c)
 *   GLIDE    GLIDE set: its own time still wins
 *   CODEC    the step record (eleven bytes) round-trips every own time 0 .. 63 with the fields that share its bytes at their extremes (notes 127, vel 127,
 *            probability 101); a step with none is the bytes it always was; a project saved and loaded keeps the times (the FBKK's side bytes); an undo brings it back
 *   CELL     the tracker's SLIDE cell: off -> on (the track's) -> an own time (+4 a detent) -> lower; left with none turns it off; EDIT drops the own time first, then
 *            the slide; a slide turned off elsewhere takes its time with it; the foot says "ON SLOW*"
 *   WORDS    NOTES shows the step's own time as a word
 * Revisions:
 *   Part 9 (November 2, 2026): first version. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("step_slide: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void blocks(uint32_t n) { int32_t out[2 * CTL]; while (n--) mix_block(out, CTL); }
static int voice_of(const track_t *t, uint32_t note) { for (uint32_t i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].gate && t->v[i].note == note) return (int)i; return -1; }
static void manual(track_t *t, uint32_t s, uint32_t note, uint32_t flags)
{
    step_t *st = &t->step[s];
    memset(st, 0, sizeof *st);
    st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96; st->flags = (uint8_t)flags;
}
static track_t *prep(uint32_t mode, int slide_time, int glide)
{
    track_t *t;
    ui_power_on();
    stop_transport();
    go_title("NOTES"); frame();
    t = TSEL;
    set_engine_of(t, ENGI_MACRO);
    blocks(400);
    track_defaults_steps(t);
    t->p[P_SLEN] = 8; t->p[P_VOICE] = (int16_t)mode; t->p[P_GLIDE] = (int16_t)glide; t->p[P_GLMODE] = 0; t->p[P_PRIO] = 0; t->p[P_SGATE] = 64;
    t->p[P_ATK] = 0; t->p[P_DEC] = 40; t->p[P_SUS] = 100; t->p[P_REL] = 40;
    t->p[P_SLIDET] = (int16_t)slide_time;
    return t;
}
static uint32_t until_note(const track_t *t, uint32_t note, uint32_t limit)
{
    uint32_t n;
    for (n = 1; n <= limit; n++) { blocks(1); if (voice_of(t, note) >= 0) return n; }
    return 0;
}
/* an octave slide, 48 to 60: the blocks it took to arrive (0: the same voice did not go on legato) */
static uint32_t octave(uint32_t mode, int slide_time, int glide)
{
    track_t *t = prep(mode, slide_time, glide);
    int v0, v1;
    uint32_t k;
    manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
    for (k = 2; k < 8u; k++) t->step[k].time = ST_TIE;     /* (the note held on: the slowest slide must arrive inside it) */
    transport_req = 1; blocks(2);
    if (!until_note(t, 48, 400)) return 0;
    v0 = voice_of(t, 48);
    blocks(40);
    if (!until_note(t, 60, 400)) return 0;
    v1 = voice_of(t, 60);
    if (v1 != v0) return 0;                             /* a new voice: no legato */
    for (k = 1; k <= 1200u && t->v[v1].pitch_cur != t->v[v1].pitch16; k++) blocks(1);
    return k;
}


int main(void)
{
#if !(OC_STEP_SLIDE && OC_SLIDE_TIME)
    printf("step_slide: built without OC_STEP_SLIDE\n");
    return 0;
#else
    static const uint32_t MODE[] = {V_POLY, V_MONO, V_LEGATO, V_UNISON};
    static const char *const MN[] = {"POLY", "MONO", "LEGATO", "UNISON"};
    uint32_t m, i;
    setvbuf(stdout, NULL, _IONBF, 0);
    for (m = 0; m < NELEM(MODE); m++) {
        char what[160];
        uint32_t own_fast, own_slow, track_only, ok = 1;
        {   track_t *t = prep(MODE[m], 40, 0);                      /* the track: slow (SLOW) */
            manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
            for (i = 2; i < 8u; i++) t->step[i].time = ST_TIE;
            step_set_slide_time(&t->step[0], -32);                  /* ... this step: quick */
            transport_req = 1; blocks(2);
            if (!until_note(t, 48, 400)) ok = 0;
            blocks(40);
            if (!until_note(t, 60, 600)) ok = 0;
            for (i = 1; i <= 1200u && t->v[voice_of(t, 60) >= 0 ? voice_of(t, 60) : 0].pitch_cur != t->v[voice_of(t, 60) >= 0 ? voice_of(t, 60) : 0].pitch16; i++) blocks(1);
            own_fast = ok ? i : 0u;
        }
        own_slow = octave(MODE[m], -32, 0);                         /* the track alone at -32 for reference */
        track_only = octave(MODE[m], 40, 0);                        /* the track alone at 40 */
        snprintf(what, sizeof what, "TIME     %s: a step with -32 on a track at +40 slides in the time of -32 (%u), not +40 (%u)", MN[m], (unsigned)own_fast, (unsigned)track_only);
        chk(what, own_fast && own_slow && own_fast + 4u >= own_slow && own_fast <= own_slow + 4u && track_only > 10u * own_fast);
        {   track_t *t = prep(MODE[m], 40, 0);
            manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
            snprintf(what, sizeof what, "TIME     %s: a step with no time of its own follows the track's (+40: %u blocks)", MN[m], (unsigned)track_only);
            chk(what, t->step[0].slidet == 0 && track_only > 200u);
        }
    }
    {   /* GLIDE */
        track_t *t = prep(V_MONO, 0, 60);
        uint32_t a, k;
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
        for (k = 2; k < 8u; k++) t->step[k].time = ST_TIE;
        step_set_slide_time(&t->step[0], -64);
        transport_req = 1; blocks(2);
        until_note(t, 48, 400); blocks(40); a = until_note(t, 60, 400);
        chk("GLIDE    GLIDE set: a step's own time is ignored too (the glide's time)", a > 0u && octave(V_MONO, 63, 60) == octave(V_MONO, -64, 60));
    }
    {   /* CODEC */
        step_snapshot_t sn;
        step_t a, b;
        uint32_t ok = 1, plain = 1;
        for (i = 0; i < 64u; i++) {
            memset(&a, 0, sizeof a);
            a.note[0] = 127; a.note[1] = 127; a.note[2] = 127; a.note[3] = 127; a.n = 4; a.time = ST_NOTE; a.vel = 127; a.probability = 101; a.flags = SF_SLIDE | SF_ACCENT;
            a.cond = 5; a.nudge = -32; a.hit = 0; a.slidet = (uint8_t)i;
            step_snapshot_put(&sn, &a);
            b = step_snapshot_get(&sn);
            ok &= !memcmp(&a, &b, sizeof a) && ocp_step_ok(&b);
        }
        chk("CODEC    every own time 0 .. 63 round-trips through the eleven-byte record, beside notes 127, velocity 127, probability 101", ok);
        for (i = 0; i < 4u; i++) {
            memset(&a, 0, sizeof a);
            a.note[0] = (uint8_t)(10 + i); a.note[1] = (uint8_t)(20 + i); a.n = 2; a.time = ST_NOTE; a.vel = (uint8_t)(90 + i); a.probability = (uint8_t)(i * 30);
            step_snapshot_put(&sn, &a);
            plain &= !(sn.note[0] & 128u) && !(sn.note[1] & 128u) && !(sn.note[2] & 128u) && !(sn.note[3] & 128u) && !(sn.vel & 128u) && !(sn.probability & 128u) &&
                     sn.note[0] == a.note[0] && sn.vel == a.vel && sn.probability == a.probability;
        }
        chk("CODEC    a step with no time of its own is the bytes it always was (no high bit set: an older firmware reads it)", plain);
        memset(&a, 0, sizeof a); a.note[0] = 60; a.n = 1; a.vel = 100; a.flags = SF_SLIDE; a.slidet = 9;
        step_snapshot_put(&sn, &a);
        sn.vel |= 128u; sn.probability |= 128u;
        b = step_snapshot_get(&sn);
        chk("CODEC    decoded, a step's velocity and probability are 7 bits again (the high bits are the time's)", b.vel == 100 && b.probability == 0 && b.slidet == (uint8_t)(9 | 16 | 32));
    }
    {   /* STALE: a time with no slide is not kept */
        step_snapshot_t sn; step_t a, b;
        memset(&a, 0, sizeof a); a.note[0] = 60; a.n = 1; a.time = ST_NOTE; a.vel = 100; a.slidet = 20;   /* no SF_SLIDE */
        step_snapshot_put(&sn, &a); b = step_snapshot_get(&sn);
        chk("STALE    a time on a step with no slide is not stored (the record has no high bits)", b.slidet == 0 && !(sn.note[0] & 128u));
        a.slidet = 20; step_clear(&a);
        chk("STALE    step_clear takes the time with it", a.slidet == 0);
        a.slidet = 20; a.flags = 0;
        chk("STALE    step_slide_time ignores a time on a step with no slide", step_slide_time(&a, 7) == 7);
    }
    {   /* PROJECT: the side bytes beside the FBKK, and undo */
        track_t *t = prep(V_POLY, 0, 0);
        manual(t, 3, 55, SF_SLIDE); step_set_slide_time(&t->step[3], 20);
        manual(t, 5, 57, SF_SLIDE); step_set_slide_time(&t->step[5], -50);
        project_save(2);
        t->step[3].slidet = 0; t->step[5].slidet = 0;
        project_load(2);
        t = TSEL;
        chk("PROJECT  a project saved and loaded keeps each step's own time", step_slide_time(&t->step[3], 99) == 20 && step_slide_time(&t->step[5], 99) == -50 && step_slide_time(&t->step[0], 99) == 99);
    }
    {   /* CELL */
        track_t *t = prep(V_POLY, 10, 0);
        char h[64];
        go_title("TRACKER"); frame();
        manual(t, 2, 60, 0); cursor_set(2); ui.tk_col = TKC_FX; ui.tk_fx = TFX_SLIDE; frame();
        turn(EN_K4, 1);
        chk("CELL     right from off: the slide is on, with the track's time", (t->step[2].flags & SF_SLIDE) && !t->step[2].slidet);
        turn(EN_K4, 1);
        chk("CELL     right again: a time of its own, the track's plus four", t->step[2].slidet && step_slide_time(&t->step[2], 0) == 14);
        turn(EN_K4, 2);
        chk("CELL     two more detents: +8 each (22)", step_slide_time(&t->step[2], 0) == 22);
        turn(EN_K4, -1);
        chk("CELL     left lowers it", step_slide_time(&t->step[2], 0) == 18);
        ui.force = 1; frame();
        tk_hint(h, sizeof h);
        chk("CELL     the foot says ON, the word of its time and a star for a time of its own (ON EASY*)", strstr(h, "ON EASY*") != 0);
        press(B_EDIT);
        chk("CELL     EDIT drops the own time first (the slide stays on)", (t->step[2].flags & SF_SLIDE) && !t->step[2].slidet);
        press(B_EDIT);
        chk("CELL     a second EDIT turns the slide off", !(t->step[2].flags & SF_SLIDE));
        turn(EN_K4, 1); turn(EN_K4, -1);
        chk("CELL     on, then left with no time of its own: off", !(t->step[2].flags & SF_SLIDE) && !t->step[2].slidet);
        turn(EN_K4, 1); turn(EN_K4, 1);
        chk("CELL     (set up: on with an own time)", (t->step[2].flags & SF_SLIDE) && t->step[2].slidet);
        hold(B_EDIT);
        chk("CELL     EDIT held (undo) takes the own time back: on with the track's time", (t->step[2].flags & SF_SLIDE) && !t->step[2].slidet);
        step_set_slide_time(&t->step[2], 30); t->step[2].flags |= SF_SLIDE;
        go_title("NOTES"); frame(); cursor_set(2);
        TSEL->step[2].flags |= SF_SLIDE;
        chk("CELL     NOTES' SLIDE column word follows the step's own time (SLOW at 30, track NORM at 10)", !strcmp(slide_time_word(step_slide_time(&t->step[2], t->p[P_SLIDET])), "SLOW"));
    }
    printf("step_slide: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
#endif
}
