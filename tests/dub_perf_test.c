/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* DUB TAPE's performance view (branch dub-perf; docs/orangecat/UI_DESIGN.md section 7, layout C; firmware/src/dub_keys.h, ui_home_pic.c, jfx_dub.c, midi_control.c):
 *   KEYS     with HOME's DUB page open the octave C4 .. B4 (keys 7 .. 18) is the page's: white C D E F G A the throws (13 38 64 89 108 127), B CUT, black C# KICK, D# HOLD, G# DIVE, A# RISE,
 *            F# nothing; a throw walked 100, 70, 10 and let go; two held, the first let go; the keys outside the octave and every key with the page closed play notes; a key let go after the
 *            page closed still releases its trigger; CC 19 .. 21 are the same triggers
 *   RECORD   (the track recording) a throw is the SEND lane (its level, then the knob's), CUT the ECHO lane (0, then the knob's); KICK, HOLD, DIVE and RISE write no event; not recording: no event
 *            and the motion base untouched
 *   RENDER   through the DUB module (tests/dub_module_host.h), sample for sample: CUT held = ECHO 0 for the same blocks (so a CUT recorded as ECHO 0 plays as the live trigger); a throw = SEND at that level;
 *            HOLD = FDBK 100; DIVE = SPEED 0; RISE = SPEED 127; DIVE and RISE together = SPEED 64; a control (nothing) differs from each
 *   PAGES    the HOME turn is PICTURE, TRACKS, CHORD while the track's ECHO is 0 and PICTURE, DUB, TRACKS, CHORD above it; the four knobs on DUB are FDBK SEND FILTER ECHO; the page keeps its place
 *            when ECHO is turned to 0; DUB needs PICTURE chosen
 *   DRAW     the page draws (the keys, the picture, the help line), a key held changes the pixels, the budget: 2 s of play with a throw key every 300 ms and KICK every 500 ms, mean under 11,000 px
 *            a 100 ms frame and worst under 22,000 (UI_DESIGN.md section 3)
 * Run by tests/run_tests.sh under OC_HOME_PIC=1. */
#define RES_HOST_MAIN_WORDS (85760u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#define DUB_IDLE 700u
#include "ui_test.c"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include "dub_module_host.h"                         /* (round 20) DUB's kernel is the module: registered in main */

static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("dub_perf: %-140s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void key(uint32_t k, int down)
{
    if (down) fm1_in.notes |= 1u << k; else fm1_in.notes &= ~(1u << k);
    keyboard_block();
}
static uint32_t lane_events(uint32_t id)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < motion.count; i++) n += (motion.event[i].place >> 6) == 0 && motion.event[i].param == id;
    return n;
}
static int lane_at(uint32_t id, uint32_t step, int *value)
{
    for (uint32_t i = 0; i < motion.count; i++)
        if (motion.event[i].place == (step & 63u) && motion.event[i].param == id) { *value = motion.event[i].value; return 1; }
    return 0;
}
static void fresh(void)
{
    ui_power_on();
    memset(dub_trig, 0, sizeof dub_trig); memset(dubk_held, 0, sizeof dubk_held);
    fm1_in.notes = 0; kb_prev = 0; kb_dubk = 0; dub_view_on = 0;
    keyboard_block();
}
/* ---- RENDER ---- */
#define RN 1500u
static struct { int32_t pcm[RN * CTL]; } *shm;
static int g_tl = 6, g_tr = 7;                          /* the synced divisions of TIME L and R (dub_sync_beats: 6 = 1/8, 7 = 1/8 dotted) */
typedef struct { int8_t trig; int8_t tv; int16_t pid; int16_t pv; } act_t;     /* trig >= 0: dub_trig[0][trig] = tv; pid >= 0: p[pid] = pv */
static void apply(const act_t *a)
{
    if (a->trig >= 0) dub_trig[0][a->trig] = (uint8_t)a->tv;
    if (a->pid >= 0) trk[0].p[a->pid] = a->pv;
}
/* one render in its own process: a DUB track with a note, `on` applied for blocks 400 .. 800 (and back with `off`); fdbk the knob's FDBK */
static void rend(const act_t *on, const act_t *off, int fdbk, int32_t *out)
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
        song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.playing = 0;
        set_engine_of(t, ENGI_MACRO); apply_preset_to(t, 8);
        for (uint32_t k = 0; k < NPART; k++) trk[k].p[P_DUB_ECHO] = 0;
        t->p[P_DUB_ECHO] = 100; t->p[P_DUB_TIML] = (int16_t)dub_t_val(g_tl); t->p[P_DUB_TIMR] = (int16_t)dub_t_val(g_tr);
        t->p[P_DUB_MODE] = DUB_MODE_MARK | 1; t->p[P_DUB_FDBK] = (int16_t)fdbk; t->p[P_DUB_SEND] = 127; t->p[P_DUB_FILT] = 60; t->p[P_DUB_SPRG] = 20;
        for (uint32_t b = 0; b < RN; b++) {
            if (b % 120u == 4u) trk_note_on(t, 60, 100);
            if (b % 120u == 64u) trk_note_off(t, 60);
            if (b == 400u && on) apply(on);
            if (b == 800u && off) apply(off);
            mix_block(blk, CTL);
            for (uint32_t i = 0; i < CTL; i++) shm->pcm[b * CTL + i] = blk[2 * i] + 3 * blk[2 * i + 1];
        }
        _exit(0);
    }
    waitpid(pid, &st, 0);
    memcpy(out, shm->pcm, sizeof shm->pcm);
}
static int32_t peak(const int32_t *a) { int32_t p = 0; for (uint32_t i = 0; i < RN * CTL; i++) { int32_t v = a[i] < 0 ? -a[i] : a[i]; if (v > p) p = v; } return p; }
static int32_t RA[RN * CTL], RB[RN * CTL], RC[RN * CTL];
static void same(const char *what, const act_t *a_on, const act_t *a_off, int fa, const act_t *b_on, const act_t *b_off, int fb)
{
    char buf[200];
    rend(a_on, a_off, fa, RA);
    rend(b_on, b_off, fb, RB);
    rend(0, 0, fa, RC);
    snprintf(buf, sizeof buf, "RENDER  %s: sample for sample", what);
    chk(buf, !memcmp(RA, RB, sizeof RA) && peak(RA) > 500);
    if (memcmp(RA, RB, sizeof RA)) {
        uint32_t first = 0, n = 0; int64_t mx = 0;
        for (uint32_t i = 0; i < RN * CTL; i++) if (RA[i] != RB[i]) { if (!n) first = i; n++; int64_t d = (int64_t)RA[i] - RB[i]; if (d < 0) d = -d; if (d > mx) mx = d; }
        printf("dub_perf:         (differs: first at block %u, %u samples, largest step %lld, peak %d)\n", first / CTL, n, (long long)mx, peak(RA));
    }
    snprintf(buf, sizeof buf, "RENDER  %s: the control (nothing applied) differs", what);
    chk(buf, memcmp(RA, RC, sizeof RA) != 0);
}
/* ---- PAGES and DRAW (home_pic_test.c's way) ---- */
static void settle(void) { int32_t o[2 * CTL]; for (uint32_t b = 0; b < 400u; b++) mix_block(o, CTL); }
static void play_frames(uint32_t ms)
{
    int32_t out[2 * CTL];
    for (uint32_t i = 0; i < ms / 16u; i++) {
        for (uint32_t b = 0; b < 16u * 44100u / 1000u / CTL; b++) mix_block(out, CTL);
        frame();
    }
}
static void pic_on(void)
{
    hold(B_HOME);
    ui.menu_sel = MI_HOME_PAGE;
    if (!home_pic_on()) press(B_OCTUP);
    hold(B_HOME);
    go_home();
    frames(100);
}
static uint32_t screen_hash(void)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < 240u * 240u; i++) h = (h ^ host_screen[i]) * 16777619u;
    return h;
}
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (dub_host_module_set() != FXM_OK) { printf("dub_perf: the DUB module did not register\n"); return 2; }
#if !OC_HOME_PIC
    printf("dub_perf: built without OC_HOME_PIC: nothing to check\n");
    return 0;
#else
    {   /* KEYS */
        static const uint8_t L[6] = {13, 38, 64, 89, 108, 127};
        static const uint8_t WK[6] = {7, 9, 11, 12, 14, 16};      /* C D E F G A */
        int ok = 1;
        fresh(); dub_view_on = 1;
        for (uint32_t i = 0; i < 6u; i++) { key(WK[i], 1); ok &= dub_trig[0][0] == L[i]; key(WK[i], 0); ok &= dub_trig[0][0] == 0; }
        chk("KEYS    the six white keys C .. A are the throws at 13 38 64 89 108 127 (10 .. 100 percent), let go: 0", ok);
        key(16, 1); key(12, 1); ok = dub_trig[0][0] == L[3]; key(9, 1); ok &= dub_trig[0][0] == L[1];
        key(9, 0); ok &= dub_trig[0][0] == L[5]; key(12, 0); ok &= dub_trig[0][0] == L[5]; key(16, 0); ok &= dub_trig[0][0] == 0;
        chk("KEYS    a throw walked 100, 70, 30: each its level; 30 let go with 100 and 70 held: the highest held, 100 (the 70 is let go first: still 100); the last let go: 0", ok);
        key(18, 1); ok = dub_trig[0][1] == 127; key(18, 0); ok &= dub_trig[0][1] == 0;
        key(8, 1); ok &= dub_trig[0][2] == 127; key(8, 0); ok &= dub_trig[0][2] == 0;
        key(10, 1); ok &= dub_trig[0][3] == 127; key(10, 0); ok &= dub_trig[0][3] == 0;
        key(15, 1); ok &= dub_trig[0][4] == 127; key(15, 0); ok &= dub_trig[0][4] == 0;
        key(17, 1); ok &= dub_trig[0][5] == 127; key(17, 0); ok &= dub_trig[0][5] == 0;
        chk("KEYS    B is CUT, C# KICK, D# HOLD, G# DIVE, A# RISE: each sets its own trigger while down and clears it", ok);
        {   uint8_t before[4][6];
            memcpy(before, dub_trig, sizeof before);
            key(13, 1); ok = !memcmp(before, dub_trig, sizeof before) && kb_note[13] == KB_SILENT; key(13, 0);
            chk("KEYS    F# is empty: no trigger and no note", ok); }
        key(6, 1); ok = kb_note[6] != KB_SILENT && !dub_trig[0][0]; key(6, 0);
        key(19, 1); ok &= kb_note[19] != KB_SILENT; key(19, 0);
        chk("KEYS    the keys outside the octave (B3 below, C5 above) play the track while the page is open", ok);
        dub_view_on = 0;
        key(16, 1); ok = kb_note[16] != KB_SILENT && dub_trig[0][0] == 0; key(16, 0);
        chk("KEYS    with the page closed the octave plays notes", ok);
        dub_view_on = 1; key(16, 1); key(18, 1); dub_view_on = 0; key(16, 0); key(18, 0);
        chk("KEYS    keys let go after the page closed still release their triggers", dub_trig[0][0] == 0 && dub_trig[0][1] == 0);
        dub_view_on = 1;
        midi_parameter(TSEL, 19, 127); midi_parameter(TSEL, 20, 100); midi_parameter(TSEL, 21, 64);
        ok = dub_trig[0][3] == 127 && dub_trig[0][4] == 100 && dub_trig[0][5] == 64 && !dub_trig[0][0];
        memset(dub_trig, 0, sizeof dub_trig);
        chk("KEYS    CC 19 HOLD, 20 DIVE and 21 RISE write the triggers in RAM, as the keys do", ok);
    }
    {   /* RECORD */
        int v;
        fresh(); dub_view_on = 1;
        song.sel = 0; song.playing = 1; song.rec = 1u; song.rec_wait = 0; chain.running = 0;
        TSEL->p[P_SLEN] = 16; TSEL->seq_idx = 3; TSEL->seq_pos = 0;
        TSEL->p[P_DUB_SEND] = 90; TSEL->p[P_DUB_ECHO] = 100;
        memset(&motion, 0, sizeof motion);
        key(16, 1);
        chk("RECORD  recording: the 100 throw writes the SEND lane at the step, 127", lane_events(P_DUB_SEND) == 1 && lane_at(P_DUB_SEND, 3, &v) && v == 127);
        TSEL->seq_idx = 5; key(16, 0);
        chk("RECORD  .. let go two steps later: the knob's value, 90, on the SEND lane", lane_events(P_DUB_SEND) == 2 && lane_at(P_DUB_SEND, 5, &v) && v == 90);
        TSEL->seq_idx = 8; key(18, 1);
        chk("RECORD  CUT writes the ECHO lane at 0", lane_events(P_DUB_ECHO) == 1 && lane_at(P_DUB_ECHO, 8, &v) && v == 0);
        TSEL->seq_idx = 10; key(18, 0);
        chk("RECORD  .. and the knob's value, 100, when let go", lane_events(P_DUB_ECHO) == 2 && lane_at(P_DUB_ECHO, 10, &v) && v == 100);
        {   uint32_t n = motion.count;
            TSEL->seq_idx = 12;
            key(8, 1); key(8, 0); key(10, 1); key(10, 0); key(15, 1); key(15, 0); key(17, 1); key(17, 0);
            chk("RECORD  KICK, HOLD, DIVE and RISE write no motion event (they record as flagged control notes: tests/ctl_play_test.c)", motion.count == n); }
        song.rec = 0; song.playing = 0;
        memset(&motion, 0, sizeof motion);
        TSEL->p[P_DUB_SEND] = 90;
        key(16, 1); key(16, 0); key(18, 1); key(18, 0);
        chk("RECORD  not recording: no event, and the knobs' values untouched (SEND 90, ECHO 100)", motion.count == 0 && TSEL->p[P_DUB_SEND] == 90 && TSEL->p[P_DUB_ECHO] == 100);
    }
    {   /* RENDER: the module's kernel */
        static const act_t CUT_ON = {1, 127, -1, 0}, CUT_OFF = {1, 0, -1, 0}, E0_ON = {-1, 0, P_DUB_ECHO, 0}, E0_OFF = {-1, 0, P_DUB_ECHO, 100};
        static const act_t TH_ON = {0, 64, -1, 0}, TH_OFF = {0, 0, -1, 0}, SD_ON = {-1, 0, P_DUB_SEND, 64}, SD_OFF = {-1, 0, P_DUB_SEND, 127};
        static const act_t HO_ON = {3, 127, -1, 0}, HO_OFF = {3, 0, -1, 0}, FB_ON = {-1, 0, P_DUB_FDBK, 100}, FB_OFF = {-1, 0, P_DUB_FDBK, 20};
        static const act_t DI_ON = {4, 127, -1, 0}, DI_OFF = {4, 0, -1, 0}, SP0_ON = {-1, 0, P_DUB_SPD, 0}, SP_OFF = {-1, 0, P_DUB_SPD, 64};
        static const act_t RI_ON = {5, 127, -1, 0}, RI_OFF = {5, 0, -1, 0}, SP127_ON = {-1, 0, P_DUB_SPD, 127};
        static const act_t DR_ON = {4, 127, 0, 0}, DR_ON2 = {5, 127, -1, 0};
        static const act_t BOTH_ON[2] = {{4, 127, -1, 0}, {5, 127, -1, 0}};
        same("CUT held (CC 17) = ECHO 0 for the same blocks, then back (a CUT recorded as ECHO 0 plays as the live trigger)", &CUT_ON, &CUT_OFF, 60, &E0_ON, &E0_OFF, 60);
        same("a throw at 64 = SEND 64 for the same blocks", &TH_ON, &TH_OFF, 60, &SD_ON, &SD_OFF, 60);
        same("HOLD = FDBK 100 for the same blocks (the knob at 20)", &HO_ON, &HO_OFF, 20, &FB_ON, &FB_OFF, 20);
        g_tl = 0; g_tr = 1;                               /* (short times: the knob's SPEED 0 would grow the line for the slower tape, DIVE does not: the taps stop at the line's end) */
        same("DIVE = SPEED 0 (an octave down), times short enough for the line to hold either", &DI_ON, &DI_OFF, 60, &SP0_ON, &SP_OFF, 60);
        g_tl = 6; g_tr = 7;
        same("RISE = SPEED 127", &RI_ON, &RI_OFF, 60, &SP127_ON, &SP_OFF, 60);
        (void)DR_ON; (void)DR_ON2; (void)BOTH_ON;
        {   /* DIVE and RISE together: normal speed (applied as one act of two: the second applies on top of the first) */
            static int32_t RD[RN * CTL];
            pid_t pid; (void)pid;
            rend(&DI_ON, &DI_OFF, 60, RA);
            rend(&RI_ON, &RI_OFF, 60, RB);
            chk("RENDER  DIVE and RISE render differently from each other and from nothing (a tape down and a tape up)", memcmp(RA, RB, sizeof RA) != 0);
            (void)RD;
        }
    }
    {   /* PAGES */
        int ok;
        ui_power_on();
        pic_on();
        TSEL->p[P_DUB_ECHO] = 0;
        ok = ui.home_pg == 2u; press(B_HOME); ok &= ui.home_pg == 0u; press(B_HOME); ok &= ui.home_pg == 1u; press(B_HOME); ok &= ui.home_pg == 2u;
        chk("PAGES   ECHO 0: HOME turns PICTURE, TRACKS, CHORD (the DUB page is not in the turn)", ok);
        TSEL->p[P_DUB_ECHO] = 100;
        ok = ui.home_pg == 2u; press(B_HOME); ok &= ui.home_pg == DUB_PG; press(B_HOME); ok &= ui.home_pg == 0u; press(B_HOME); ok &= ui.home_pg == 1u; press(B_HOME); ok &= ui.home_pg == 2u;
        chk("PAGES   ECHO above 0: PICTURE, DUB, TRACKS, CHORD", ok);
        press(B_HOME); frames(200);
        {   int16_t *p = TSEL->p;
            turn(EN_K1, 5); turn(EN_K2, -3); turn(EN_K3, 4); turn(EN_K4, -2);
            chk("PAGES   on DUB the four knobs write FDBK, SEND, FILTER and ECHO", ui.home_pg == DUB_PG && p[P_DUB_FDBK] != 55 && p[P_DUB_SEND] != 127 && p[P_DUB_FILT] != 60 && p[P_DUB_ECHO] != 100); }
        {   uint32_t n;
            memset(&motion, 0, sizeof motion);
            song.sel = 0; song.playing = 1; song.rec = 1u; song.rec_wait = 0; chain.running = 0; TSEL->p[P_SLEN] = 16; TSEL->seq_idx = 2; TSEL->seq_pos = 0;
            turn(EN_K1, 2); turn(EN_K2, 2); turn(EN_K3, 2); turn(EN_K4, -1);
            n = lane_events(P_DUB_FDBK) + lane_events(P_DUB_SEND) + lane_events(P_DUB_FILT) + lane_events(P_DUB_ECHO);
            chk("RECORD  the four knobs of the page, turned while the track records, write their own lanes (FDBK, SEND, FILTER, ECHO)", n == 4u);
            song.rec = 0; song.playing = 0; }
        frames(100);
        chk("PAGES   the page is on while it shows (the keys are the page's)", ui.home_pg == DUB_PG && dub_view_on == 1);
        TSEL->p[P_DUB_ECHO] = 0; frames(100);
        chk("PAGES   ECHO turned to 0 on the page: the page stays (the knob that turns it on is on it)", ui.home_pg == DUB_PG);
        press(B_HOME); frames(100);
        chk("PAGES   HOME leaves it, the keys are notes again", ui.home_pg == 0u && dub_view_on == 0);
        home_pic_set(0); frames(100);
        chk("PAGES   TRACKS chosen: never the DUB page", ui.home_pg != DUB_PG);
        {   /* (round 18 review) leaving HOME for any other screen clears the view: no page leaves the keys C4 .. B4 as DUB's */
            uint32_t i, stuck = 0, seen = 0;
            for (i = 0; i < NPAGES; i++) {
                ui_power_on(); pic_on();
                TSEL->p[P_DUB_ECHO] = 100;
                press(B_HOME); frames(200);
                if (ui.home_pg != DUB_PG || !dub_view_on) { stuck = 1000; break; }
                ui.home = 0; ui.page = (uint8_t)i; page_entered();
                if (!page_visible(i)) continue;
                seen++; frames(200);
                stuck += dub_view_on;
            }
            printf("dub_perf: %u visible pages tried, %u leave the DUB keys on\n", seen, stuck);
            chk("PAGES   no visible page, after the DUB page, leaves the DUB keys on", stuck == 0 && seen > 20u); }
    }
    {   /* DRAW and BUDGET */
        int ok;
        uint32_t h0, h1;
        ui_power_on(); palette_set(0); settle(); pic_on();
        TSEL->p[P_DUB_ECHO] = 100; TSEL->p[P_DUB_FDBK] = 115;
        press(B_HOME); frames(300);
        h0 = screen_hash();
        dub_trig[0][0] = 108; frames(100);
        h1 = screen_hash();
        chk("DRAW    the DUB page draws; a throw held (CC 16) lights its key and moves the picture's bar", ui.home_pg == DUB_PG && h0 != h1);
        dub_trig[0][0] = 0; dub_trig[0][1] = 127; frames(100);
        ok = screen_hash() != h1 && screen_hash() != h0;
        chk("DRAW    CUT held draws again (its key red, the arrow gone)", ok);
        memset(dub_trig, 0, sizeof dub_trig);
        {   uint64_t sum = 0, worst = 0, last;
            uint32_t n = 0;
            frames(500);
            last = host_sent;
            for (uint32_t t = 0; t < 2000u; t += 100u) {
                dub_trig[0][0] = (t / 100u) % 3u == 0 ? DUBK_LEVEL[(t / 300u) % 6u] : 0;
                dub_trig[0][2] = t % 500u < 100u ? 127 : 0;
                for (uint32_t i = 0; i < SCOPE_N; i++) scope_buf[i] = (int16_t)((int32_t)((i * 37u + t * 11u) % 128u) * 200 - 12800);
                play_frames(100);
                uint64_t px = host_sent - last;
                last = host_sent;
                sum += px; n++;
                if (px > worst) worst = px;
            }
            printf("dub_perf: BUDGET  a throw every 300 ms, KICK every 500 ms: mean %6.0f px a 100 ms frame (%4.1f ms SPI), worst %6u px (%4.1f ms)\n", (double)sum / n, (double)sum / n * 77.0 / 57600.0,
                   (unsigned)worst, worst * 77.0 / 57600.0);
            chk("BUDGET  the pixels sent a 100 ms frame: mean under 12,500 and worst under 22,000", sum / n < 12500u && worst < 22000u);
        }
        memset(dub_trig, 0, sizeof dub_trig);
    }
    printf("dub_perf: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
#endif
}
