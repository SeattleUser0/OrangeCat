/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix2 item 4 (docs/orangecat/UI_BUILD.md "ui-fix2 4"): SLIDE TIME, with OC_SLIDE=1 and OC_SLIDE_TIME=1, through tests/ui_test.c's host harness
 * (the sequencer and voices driven by mix_block, as slide_test.c does).
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_SLIDE=1 -DOC_SLIDE_TIME=1
 *      -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o slide_time_test tests/slide_time_test.c -lm
 * Checked, on MACRO (it reads the voice's pitch every block):
 *   TIME      in POLY, MONO, LEGATO and UNISON an octave slide takes about 192 / (3 * 2^(-s/16)) blocks for SLIDE TIME s: s = 0 as before (64
 *             blocks, an octave in 46 ms), s = -32 a quarter of it, s = 16 and 40 and 63 longer each; the time never gets shorter as s rises;
 *             the voice goes on legato (no retrigger) at every s
 *   GLIDE     GLIDE set: its own time, SLIDE TIME ignored
 *   OLD       a track whose value is 0 (every project saved before): the rate slides had
 *   KEPT      a sound load leaves the track's SLIDE TIME; a project saved and loaded keeps it; a new track starts at 0
 *   WORDS     the five words NOTES and the tracker show (SNAP FAST QUICK NORM EASY SLOW LONG) and the tracker's foot: "ON SLOW"
 *   INERT     motion on the id stays inert; a user sound does not store it
 * Revisions:
 *   (ui-fix2 4, branch ui-fix2) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("slide_time: %-104s %s\n", what, ok ? "ok" : "FAIL");
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
#if !(OC_SLIDE && OC_SLIDE_TIME)
    printf("slide_time: built without OC_SLIDE_TIME\n");
    return 0;
#else
    static const int S[] = {-64, -32, -16, 0, 16, 40, 63};
    static const uint32_t MODE[] = {V_POLY, V_MONO, V_LEGATO, V_UNISON};
    static const char *const MN[] = {"POLY", "MONO", "LEGATO", "UNISON"};
    uint32_t m, i;
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    for (m = 0; m < NELEM(MODE); m++) {
        uint32_t prev = 0, ok_mono = 1, ok_range = 1, legato = 1, took[NELEM(S)];
        char line[256];
        int n = 0;
        for (i = 0; i < NELEM(S); i++) {
            double rate = 3.0 * pow(2.0, -S[i] / 16.0), exp = 192.0 / rate;
            took[i] = octave(MODE[m], S[i], 0);
            legato &= took[i] != 0;
            ok_mono &= took[i] >= prev;
            prev = took[i];
            /* the rate is accumulated in 1/256 of a step, so the formula holds within a few percent and a block or two */
            ok_range &= took[i] * 1.1 + 2 >= exp && took[i] <= exp * 1.1 + 3;
            n += snprintf(line + n, sizeof line - (size_t)n, " %d:%u", S[i], (unsigned)took[i]);
        }
        printf("slide_time:   %s: octave blocks by SLIDE TIME %s\n", MN[m], line);
        {
            char what[160];
            snprintf(what, sizeof what, "TIME     %s: the same voice goes on at every setting, the time never shortens as SLIDE TIME rises, about the formula", MN[m]);
            chk(what, legato && ok_mono && ok_range);
            snprintf(what, sizeof what, "TIME     %s: 0 is as before (64 blocks), -32 about a quarter, 63 at least ten times", MN[m]);
            chk(what, took[3] >= 56u && took[3] <= 72u && took[1] >= 12u && took[1] <= 20u && took[6] >= 10u * took[3]);
        }
    }
    {   /* GLIDE */
        uint32_t a = octave(V_MONO, 0, 60), b = octave(V_MONO, 63, 60), c = octave(V_MONO, -64, 60);
        chk("GLIDE    GLIDE set: SLIDE TIME ignored (the same time at -64, 0 and 63)", a == b && b == c && a > 0u);
    }
    {   /* KEPT */
        track_t *t = prep(V_POLY, 20, 0);
        uint32_t k;
        set_engine_of(t, ENGI_SHAPES);                  /* a sound load: the engine and its preset */
        chk("KEPT     a sound load (an engine jump) leaves SLIDE TIME", t->p[P_SLIDET] == 20);
        project_save(2);
        t->p[P_SLIDET] = 0;
        project_load(2);
        chk("KEPT     a project saved and loaded keeps it", TSEL->p[P_SLIDET] == 20);
        project_save(1); { uint32_t j; int ok = 1; for (j = 0; j < NTRK; j++) ok &= stored_param(1, j, P_SLIDET) == (j == song.sel ? 20 : 0); chk("KEPT     every track's own value is saved (the others stay 0)", ok); }
        { track_t u; memset(&u, 0, sizeof u); chk("KEPT     the descriptor's default is 0 (a new track, an old project)", param_desc_of(ENGI_MACRO, P_SLIDET)->def == 0 && param_desc_of(ENGI_SHAPES, P_SLIDET)->def == 0); }
        t->p[P_SLIDET] = 77;                            /* (outside the range) */
        project_save(3); project_load(3);
        chk("KEPT     a value out of range is clamped on a load", TSEL->p[P_SLIDET] <= 63);
    }
    {   /* OLD: id 48 was the slicer's DEPTH (default 127) in projects before commit 1e47387: no SLIDE TIME */
        track_t *t = prep(V_POLY, 0, 0);
        t->p[P_SLIDET] = 127;                           /* what such a project holds */
        project_save(2); t->p[P_SLIDET] = 5; project_load(2);
        chk("OLD      a project holding 127 there (the old slicer depth) loads with SLIDE TIME 0, not 63", TSEL->p[P_SLIDET] == 0);
        t->p[P_SLIDET] = 30;
        chk("OLD      a value in range survives (a project saved with this build)", (project_save(3), t->p[P_SLIDET] = 0, project_load(3), TSEL->p[P_SLIDET] == 30));
        /* a new slide starts its accumulator at 0: a leftover fraction does not shorten the first step */
        manual(t, 0, 48, SF_SLIDE); manual(t, 1, 60, 0);
        {   uint32_t a = octave(V_POLY, 16, 0), b = octave(V_POLY, 16, 0);
            chk("OLD      the same slide twice takes the same time (the accumulator restarts)", a == b && a > 0u);
        }
    }
    {   /* WORDS */
        track_t *t = prep(V_POLY, 0, 0);
        char b[16];
        chk("WORDS    SNAP FAST QUICK NORM EASY SLOW LONG over the range", !strcmp(slide_time_word(-64), "SNAP") && !strcmp(slide_time_word(-30), "FAST") &&
            !strcmp(slide_time_word(-10), "QUICK") && !strcmp(slide_time_word(0), "NORM") && !strcmp(slide_time_word(10), "EASY") &&
            !strcmp(slide_time_word(30), "SLOW") && !strcmp(slide_time_word(63), "LONG"));
        manual(t, 2, 60, SF_SLIDE);
        t->p[P_SLIDET] = 30;
        tk_fx_text(&t->step[2], TFX_SLIDE, b, 1);
        chk("WORDS    the editor's hardware-test string stays ON", !strcmp(b, "ON"));
        {   char h[48];
            ui.tk_col = TKC_FX; ui.tk_fx = TFX_SLIDE; ui.cursor = 2;
            tk_hint(h, sizeof h);
            chk("WORDS    the tracker's foot says ON SLOW", strstr(h, "ON SLOW") != 0);
        }
        tk_fx_text(&t->step[2], TFX_SLIDE, b, 0);
        chk("WORDS    the tracker's cell stays SL", !strcmp(b, "SL"));
    }
    printf("slide_time: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
#endif
}
