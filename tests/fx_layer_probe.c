/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8b (docs/orangecat/FX_MODULES_PLAN.md 17): the FX layer's PROBE. It renders the layer through the real mixer (mix_block: four tracks, their sends, the buses, the master, the
 * layer's buffer effects and CRUSH after the master level) over the scenarios below and prints, for each, the FNV-1a hash of every output sample (au) and of the layer's behaviour a block (st:
 * the buffer effect's mode and source, what runs, the 32 KB held and lent, the FX FULL bits, the muted tracks). The printout of the RESIDENT layer, made BEFORE the four ring effects' text was
 * cut out of jfx_perform.c into perf_kern.h, is pinned in tests/fx_layer_golden.txt (both settings of MELODEE_FX_DEFER): the cut, and every later change, must reproduce it line for line.
 * tests/fx_layer_module_test.c includes this file and runs the same scenarios with the module registered.
 *
 * Scenarios (four tracks: MACRO, DRUM-X, MACRO, the Prophet; notes on every track throughout). The keys are perf_press (what seq.c's key scan calls), the cards' knobs perf_gp (what
 * ui_layer.c's fx_layer_knob writes), SCRATCH perf_scr:
 *   GLITCH, TAPE, SHUFFLE, FREEZE   the effect held and let go, stopped and playing, with every knob of its card through its range (FXK_MIN .. FXK_MAX), one at a time, the others at their
 *           defaults; then the knobs turned while it is held; TAPE: SCRATCH (the hand on the stopped record: single detents, fast turns both ways, past what the loop holds)
 *   TEMPO   each effect and the REPEATs at 40 .. 300 BPM (below ~82 BPM GLITCH plays only its gates, crushes and live slices; a REPEAT 1/8 longer than the loop does nothing), the tempo
 *           changed while an effect is held, an external clock's tempo
 *   SWITCH  the keys against each other: every ordered pair of the seven buffer effects (the second pressed over the first, then let go in both orders: the last pressed plays, letting it go
 *           returns to the one held before), presses in the middle of a fade, three held at once, a REPEAT shortened (the cross-fade), FX LATCH, every effect off (perf_off_req), a dialog
 *           (perf_kill) and back, a project load (jfx_clear_all) in the middle of an effect
 *   START   the transport started, stopped and restarted while an effect is held and before it is pressed (perf_start: a 1/16 starts with it)
 *   MEM     the 32 KB: the arena short (jfx_sl_mem refused: FX FULL, the key does nothing) and given back; the buffer taken away in the middle of each effect (jfx_sl_lost: perf_buf_lost) and
 *           pressed again; STUT (the SLICER's use of the same 32 KB) under a buffer effect and after it
 *   MIX     a full mix (chords, the three sends, the SLICER's GATE) with the buffer effects in turn over CRUSH, THROW, the mutes and the FILTER macro
 *   RANDOM  40,000 blocks of keys, knobs, tempo, transport, latch, dialogs and lost buffers from a fixed generator
 *   STEREO  the tracks panned apart, the CHORUS and a wide reverb on (the scenarios above are all but mono at the layer: a kernel that mixed its channels up would pass them): each effect
 *           and a REPEAT at two settings, stopped and playing
 *   SCRATCH the hand on the record where the scenarios above do not reach: detents turned while the tape still falls (they move the hand when it stops), and fast turns at 300 BPM, where
 *           64 detents stay inside what the loop holds (at 200 BPM they run into its ends)
 * (STEREO and SCRATCH were added after the mutation check let four mutants live: their lines of tests/fx_layer_golden.txt were made from the tree of 45eef53b, the resident code before the
 * cut, with this probe, whose other twenty lines that tree reproduces.)
 * Arguments: none. Built and run by tests/fx_layer_module.sh (tests/run_tests.sh). */
/* the device's banks (tests/jfx_arena_test.c): the arena is as small as the unit's */
#define RES_HOST_MAIN_WORDS (85760u / 4u)
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint64_t h_au, h_st;
static uint32_t gb;                                     /* the block count of the scenario */
static int32_t rec[CTL * 2];
static void (*flp_tap)(const char *scenario, uint32_t block, const int32_t *pcm, size_t bytes);   /* tests/fx_layer_module_test.c: every output block, as the mixer made it */
static void (*flp_pre)(void);                           /* .. and before every block (the module registered or not) */
static const char *flp_name;
typedef struct { char name[16]; uint32_t blocks; uint64_t au, st; } flp_res_t;
static flp_res_t flp_res[16];
static uint32_t flp_nres;
static char flp_only[16];                               /* FLP_ONLY in the environment: one scenario */
static int bad;
static void flp_hash(uint64_t *h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) { *h ^= *b++; *h *= 0x100000001B3ull; }
}
static void blk(void)
{
    uint32_t v[8];
    if (flp_pre) flp_pre();
    mix_block(rec, CTL);
    if (flp_tap) flp_tap(flp_name, gb, rec, sizeof rec);
    flp_hash(&h_au, rec, sizeof rec);
    v[0] = pf.mode; v[1] = pf.src; v[2] = perf_act; v[3] = sl_buf != 0; v[4] = sl_lent; v[5] = jfx_full; v[6] = perf_hush; v[7] = pf.busy;
    flp_hash(&h_st, v, sizeof v);
    gb++;
}
static uint8_t notes_off;
/* the notes: track k plays a note every 40 + 6 k blocks and holds it 26; track 2 a second one (short notes: the buffer effects have edges to cut) */
static void notes(void)
{
    uint32_t k;
    if (notes_off) return;
    for (k = 0; k < NPART; k++) {
        uint32_t per = 40u + 6u * k, ph = gb % per;
        if (ph == 2u) { trk_note_on(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 5u) * 2, 100); if (k == 2u) trk_note_on(&trk[k], 55 + (int)((gb / per) % 2u) * 4, 90); }
        if (ph == 28u) { trk_note_off(&trk[k], 48 + 7 * (int)k + (int)((gb / per) % 5u) * 2); if (k == 2u) trk_note_off(&trk[k], 55 + (int)((gb / per) % 2u) * 4); }
    }
}
static void run(uint32_t n) { while (n--) { notes(); blk(); } }
static int begin(const char *name)
{
    if (flp_only[0] && strcmp(flp_only, name)) return 0;
    flp_name = name;
    h_au = h_st = 0xCBF29CE484222325ull;
    gb = 0; notes_off = 0;
    return 1;
}
static void end(const char *name)
{
    printf("%s blocks %u au %016llx st %016llx\n", name, gb, (unsigned long long)h_au, (unsigned long long)h_st);
    fflush(stdout);
    if (flp_nres < 16u) {
        flp_res_t *r = &flp_res[flp_nres++];
        snprintf(r->name, sizeof r->name, "%s", name); r->blocks = gb; r->au = h_au; r->st = h_st;
    }
}
#define T(k, id, v) (trk[k].p[id] = (int16_t)(v))
#define G(id, v) (song.g[id] = (int16_t)(v))
static const int8_t FLP_DEF[4][4] = {{50, 0, 0, 1}, {0, 4, 0, 0}, {60, 2, 20, 1}, {50, 0, 30, 2}};   /* perf_gp's initial values */
static const uint8_t FLP_E[4] = {PF_GLT, PF_TAPE, PF_SHUF, PF_FRZ};
static const uint8_t FLP_BUF[7] = {PF_R8, PF_R16, PF_R32, PF_GLT, PF_TAPE, PF_SHUF, PF_FRZ};
static void play(void) { song.playing = 1; seq_start_count++; }
static void stop(void) { song.playing = 0; }
static void fresh(void)
{
    uint32_t k, x;
    static const uint8_t eng[4] = { ENGI_MACRO, ENGI_DRUMX, ENGI_MACRO, ENGI_PROPHET }, pre[4] = { 8, 0, 3, 0 };
    ui_power_on();
    resource_host_cache_enabled = 1;
    res_lend_ok = 15;
    for (k = 0; k < G_COUNT; k++) song.g[k] = GP[k].def;
    song.master_q12 = 4096; song.g[G_LEVEL] = 0; song.g[G_BPM] = 120; song.playing = 0;
    midi_sync_active = 0;
    for (k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], eng[k]);
        apply_preset_to(&trk[k], pre[k]);
        trk[k].p[P_DUB_ECHO] = 0;
        trk[k].p[P_REV] = 20; trk[k].p[P_DLY] = 0; trk[k].p[P_CHOR] = 0;
    }
    perf_all_off();
    perf_latch_on = 0; perf_kill = 0; perf_off_req = 0; perf_scr = 0;
    memset((void *)perf_m, 0, sizeof perf_m);
    memset(perf_ord, 0, sizeof perf_ord); perf_seq = 0;
    for (x = 0; x < 4u; x++) for (k = 0; k < 4u; k++) perf_gp[x][k] = FLP_DEF[x][k];
}
/* the values a card's knob is run through: all of a short range, the ends and steps of a long one */
static uint32_t knob_vals(uint32_t x, uint32_t k, int16_t *v)
{
    int32_t lo = FXK_MIN[x][k], hi = FXK_MAX[x][k], i;
    uint32_t n = 0;
    if (hi - lo <= 16) { for (i = lo; i <= hi; i++) v[n++] = (int16_t)i; return n; }
    if (hi - lo <= 24) { static const int8_t P[] = {-12, -11, -7, -5, -2, -1, 0, 1, 3, 5, 7, 11, 12}; for (i = 0; i < 13; i++) v[n++] = P[i]; return n; }
    if (lo < 0) { static const int8_t C[] = {-100, -99, -50, -1, 0, 1, 50, 99, 100}; for (i = 0; i < 9; i++) v[n++] = C[i]; return n; }
    { static const int8_t A[] = {0, 1, 10, 25, 49, 50, 75, 99, 100}; for (i = 0; i < 9; i++) v[n++] = A[i]; }
    return n;
}
static void scratch_part(void)
{
    static const int8_t D[] = {1, 1, -1, 5, -5, 20, -20, 64, -64, 100, -100, 127, 127, 127, 127, -127, -127, -127, -127, -127, -127, 3, -2};
    uint32_t i;
    perf_press(PF_TAPE, 1);
    run(900);                                           /* (TIME 1/4 at 120 BPM: stopped after half a second) */
    for (i = 0; i < sizeof D; i++) { perf_scr += (uint32_t)(int32_t)D[i]; run(30); }
    for (i = 0; i < 60u; i++) { perf_scr += (uint32_t)(int32_t)((i % 7u) - 3); run(3); }   /* a hand moving every few blocks */
    run(200);
    perf_press(PF_TAPE, 0);
    run(60);
}
static void scenario_fx(uint32_t x, const char *name)
{
    uint32_t e = FLP_E[x], pass, k, j, n;
    int16_t v[40];
    if (!begin(name)) return;
    fresh();
    G(G_BPM, 200);                                      /* (a 1/16 of 103 blocks: several slices in every hold) */
    run(150);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        perf_press(e, 1); run(1400); perf_press(e, 0); run(80);       /* the defaults, long enough for 13 of the 1/16s */
        for (k = 0; k < 4u; k++) {
            if (FXK_MIN[x][k] == FXK_MAX[x][k]) continue;
            n = knob_vals(x, k, v);
            for (j = 0; j < n; j++) {
                perf_gp[x][k] = (int8_t)v[j];
                perf_press(e, 1); run(x == GX_FRZ || x == GX_TAPE ? 330u : 520u); perf_press(e, 0); run(40 + j % 3u);
            }
            perf_gp[x][k] = FLP_DEF[x][k];
        }
        /* the knobs turned while it is held */
        perf_press(e, 1);
        run(200);
        for (j = 0; j < 60u; j++) {
            k = j % 4u;
            if (FXK_MIN[x][k] != FXK_MAX[x][k]) perf_gp[x][k] = (int8_t)(FXK_MIN[x][k] + (int32_t)((j * 37u + 11u) % (uint32_t)(FXK_MAX[x][k] - FXK_MIN[x][k] + 1)));
            run(45);
        }
        perf_press(e, 0);
        run(80);
        for (k = 0; k < 4u; k++) perf_gp[x][k] = FLP_DEF[x][k];
        if (x == GX_TAPE) {
            scratch_part();
            perf_gp[GX_TAPE][1] = 0; scratch_part();    /* TIME 1/64: stopped at once */
            perf_gp[GX_TAPE][1] = 2; perf_gp[GX_TAPE][2] = -100; G(G_BPM, 60); scratch_part(); G(G_BPM, 200);
            perf_gp[GX_TAPE][1] = 4; perf_gp[GX_TAPE][2] = 0;
        }
        if (x == GX_GLT || x == GX_SHUF) {              /* two bars and more of the 1/16s: every place of the cycle, at three settings */
            perf_gp[x][0] = 100; perf_gp[x][3] = 5; if (x == GX_SHUF) perf_gp[x][2] = 100;
            G(G_BPM, 300);
            perf_press(e, 1); run(2600); perf_press(e, 0); run(60);
            perf_gp[x][3] = 12; perf_gp[x][1] = x == GX_GLT ? 7 : 0; if (x == GX_GLT) perf_gp[x][2] = 3;
            perf_press(e, 1); run(2600); perf_press(e, 0); run(60);
            perf_gp[x][3] = 16; perf_gp[x][1] = x == GX_GLT ? -12 : 1; if (x == GX_GLT) perf_gp[x][2] = 1; else perf_gp[x][2] = 50;
            perf_press(e, 1); run(2600); perf_press(e, 0); run(60);
            G(G_BPM, 200);
            for (k = 0; k < 4u; k++) perf_gp[x][k] = FLP_DEF[x][k];
        }
        if (x == GX_FRZ) {                              /* every DENS with every SIZE, SPRAY up; a long freeze (the scan there and back) */
            for (j = 0; j < 5u; j++)
                for (k = 0; k < 4u; k++) {
                    perf_gp[x][3] = (int8_t)j; perf_gp[x][0] = (int8_t)(k * 33u); perf_gp[x][2] = (int8_t)(k * 30u + j * 2u); perf_gp[x][1] = (int8_t)((int)(j + k) * 3 - 9);
                    perf_press(e, 1); run(420); perf_press(e, 0); run(50);
                }
            for (k = 0; k < 4u; k++) perf_gp[x][k] = FLP_DEF[x][k];
            perf_gp[x][3] = 0; perf_press(e, 1); run(3000); perf_press(e, 0); run(60);
            perf_gp[x][3] = 4; perf_gp[x][2] = 100; perf_press(e, 1); run(3000); perf_press(e, 0); run(60);
            for (k = 0; k < 4u; k++) perf_gp[x][k] = FLP_DEF[x][k];
        }
        stop();
        run(60);
    }
    end(name);
}
static void scenario_tempo(void)
{
    static const int16_t BPM[] = {40, 60, 80, 81, 82, 83, 100, 120, 163, 200, 240, 300};
    uint32_t i, j, pass;
    if (!begin("TEMPO")) return;
    fresh();
    run(100);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        for (i = 0; i < sizeof BPM / sizeof BPM[0]; i++) {
            G(G_BPM, BPM[i]);
            for (j = 0; j < 7u; j++) {
                uint32_t h = (uint32_t)(44100u * 60u / (uint32_t)BPM[i]) * 3u / 2u / CTL;   /* a beat and a half */
                perf_press(FLP_BUF[j], 1); run(h < 300u ? 300u : h > 1500u ? 1500u : h); perf_press(FLP_BUF[j], 0); run(50);
            }
        }
        /* the tempo changed while it is held */
        for (j = 0; j < 7u; j++) {
            G(G_BPM, 120);
            perf_press(FLP_BUF[j], 1);
            for (i = 0; i < 10u; i++) { run(150); G(G_BPM, 70 + (int)((i * 53u) % 200u)); }
            perf_press(FLP_BUF[j], 0);
            run(60);
        }
        G(G_BPM, 120);
        stop();
        run(40);
    }
    /* an external clock's tempo (stopped: the measured beat is the tempo the effects read) */
    midi_sync_active = 1;
    for (i = 0; i < 4u; i++) {
        midi_beat_samples = 9000u + 7000u * i;
        for (j = 0; j < 7u; j++) { perf_press(FLP_BUF[j], 1); run(400); if (j == 3u) midi_beat_samples += 1234u; run(200); perf_press(FLP_BUF[j], 0); run(50); }
    }
    midi_sync_active = 0; midi_beat_samples = 0;
    run(40);
    end("TEMPO");
}
static void scenario_switch(void)
{
    uint32_t a, b, i, pass;
    if (!begin("SWITCH")) return;
    fresh();
    G(G_BPM, 220);
    run(100);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        for (a = 0; a < 7u; a++)
            for (b = 0; b < 7u; b++) {
                if (a == b) continue;
                perf_press(FLP_BUF[a], 1); run(170 + a);
                perf_press(FLP_BUF[b], 1); run(150 + b);              /* the last pressed plays */
                if ((a + b) & 1u) { perf_press(FLP_BUF[b], 0); run(140); perf_press(FLP_BUF[a], 0); }   /* letting it go returns to the one held before */
                else { perf_press(FLP_BUF[a], 0); run(140); perf_press(FLP_BUF[b], 0); }
                run(30 + (a * 7u + b) % 40u);
            }
        /* presses in the middle of a fade (2.9 ms: four blocks) */
        for (a = 0; a < 7u; a++)
            for (i = 0; i < 6u; i++) {
                perf_press(FLP_BUF[a], 1); run(120);
                perf_press(FLP_BUF[a], 0); run(i);
                perf_press(FLP_BUF[(a + 1u + i) % 7u], 1); run(1 + i);
                perf_press(FLP_BUF[(a + 3u) % 7u], 1); run(110);
                perf_press(FLP_BUF[(a + 1u + i) % 7u], 0); perf_press(FLP_BUF[(a + 3u) % 7u], 0); run(i + 2u);
            }
        /* three held at once, let go in every order */
        for (i = 0; i < 6u; i++) {
            static const uint8_t ORD[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
            static const uint8_t SET[3] = {PF_GLT, PF_FRZ, PF_SHUF};
            perf_press(PF_TAPE, 1); run(100);
            for (a = 0; a < 3u; a++) { perf_press(SET[a], 1); run(90); }
            for (a = 0; a < 3u; a++) { perf_press(SET[ORD[i][a]], 0); run(100); }
            perf_press(PF_TAPE, 0); run(40);
        }
        /* a REPEAT shortened and lengthened (the cross-fade), a glitcher over it */
        perf_press(PF_R8, 1); run(500); perf_press(PF_R16, 1); run(200); perf_press(PF_R32, 1); run(200); perf_press(PF_GLT, 1); run(200);
        perf_press(PF_R32, 0); run(100); perf_press(PF_GLT, 0); run(100); perf_press(PF_R16, 0); run(200); perf_press(PF_R8, 0); run(60);
        /* FX LATCH: a press turns it on or off, letting go does nothing */
        perf_latch_on = 1;
        for (a = 3; a < 7u; a++) { perf_press(FLP_BUF[a], 1); run(20); perf_press(FLP_BUF[a], 0); run(260); }
        for (a = 3; a < 7u; a++) { perf_press(FLP_BUF[a], 1); run(10); perf_press(FLP_BUF[a], 0); run(150); }
        perf_press(PF_FRZ, 1); perf_press(PF_FRZ, 0); run(200); perf_press(PF_SHUF, 1); perf_press(PF_SHUF, 0); run(200);
        perf_latch_on = 0; perf_latched = 0;            /* (ui_layer.c: the latch off clears them) */
        run(60);
        /* every effect off (FX + OCT-), a dialog (perf_kill) and back with the key still down */
        for (a = 3; a < 7u; a++) {
            perf_press(FLP_BUF[a], 1); run(250); perf_off_req = 1; run(80); perf_press(FLP_BUF[a], 0); run(10);
            perf_press(FLP_BUF[a], 1); run(250); perf_kill = 1; run(120); perf_kill = 0; run(250); perf_press(FLP_BUF[a], 0); run(40);
        }
        /* a project load in the middle of an effect, the key still down, then let go and pressed again */
        for (a = 3; a < 7u; a++) {
            perf_press(FLP_BUF[a], 1); run(300); jfx_clear_all(); run(200); perf_press(FLP_BUF[a], 0); run(30);
            perf_press(FLP_BUF[a], 1); run(200); perf_press(FLP_BUF[a], 0); run(30);
        }
        stop();
        run(50);
    }
    end("SWITCH");
}
static void scenario_start(void)
{
    uint32_t a, i;
    if (!begin("START")) return;
    fresh();
    G(G_BPM, 170);
    run(100);
    for (a = 0; a < 7u; a++) {
        perf_press(FLP_BUF[a], 1); run(200 + 13u * a);            /* held while stopped .. */
        play(); run(333);                                         /* .. the transport starts under it (perf_start) */
        seq_start_count++; run(301);                              /* RESTART */
        stop(); run(150);
        play(); run(97);
        perf_press(FLP_BUF[a], 0); run(40);
        for (i = 0; i < 5u; i++) {                                /* pressed at five places of the 1/16 (playing: it starts on the next one) */
            perf_press(FLP_BUF[a], 1); run(180 + 29u * i); perf_press(FLP_BUF[a], 0); run(17 + 31u * i);
        }
        stop(); run(31);
        perf_press(FLP_BUF[a], 1); play(); run(1); stop(); run(1); play(); run(240); perf_press(FLP_BUF[a], 0); stop(); run(50);
    }
    end("START");
}
static void scenario_mem(void)
{
    uint32_t a, h, pass;
    if (!begin("MEM")) return;
    fresh();
    G(G_BPM, 200);
    run(100);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        /* the arena short: what is left taken by something else, so the 32 KB cannot be had (FX FULL, the key does nothing); given back while the key is down */
        for (h = 0; h < 8; h++) { uint32_t size = 80000u; while (size >= 1000u && !resource_allocate(RES_JFX0 + JB_DLY0 + h, size)) size -= 500u; }
        for (a = 0; a < 7u; a++) { perf_press(FLP_BUF[a], 1); run(160); perf_press(FLP_BUF[a], 0); run(30); }
        perf_press(PF_GLT, 1); run(100);
        for (h = 0; h < 8; h++) resource_release(RES_JFX0 + JB_DLY0 + h);
        run(400); perf_press(PF_GLT, 0); run(60);
        /* the buffer taken away in the middle of each effect (the card's slot, an engine's memory: jfx_sl_lost), the key still down; then pressed again */
        for (a = 0; a < 7u; a++) {
            perf_press(FLP_BUF[a], 1); run(260 + 7u * a);
            if (sl_buf) jfx_sl_lost();
            run(300);
            perf_press(FLP_BUF[a], 0); run(40);
            perf_press(FLP_BUF[a], 1); run(200); perf_press(FLP_BUF[a], 0);
            run(2); if (sl_buf) jfx_sl_lost();                     /* .. and in its fade */
            run(60);
        }
        /* STUT (the SLICER's use of the same 32 KB) under a buffer effect and after it */
        G(G_SLMODE, SL_STUT);
        for (a = 0; a < NPART; a++) T(a, P_SLROUTE, 1);
        run(300);
        for (a = 3; a < 7u; a++) { perf_press(FLP_BUF[a], 1); run(300); perf_press(FLP_BUF[a], 0); run(200); }
        G(G_SLMODE, 0);
        for (a = 0; a < NPART; a++) T(a, P_SLROUTE, 0);
        run(100);
        stop();
        run(40);
    }
    end("MEM");
}
static void scenario_mix(void)
{
    uint32_t k, ps, st, i, tk;
    static const int notes_a[4][4] = { { 36, 0, 0, 0 }, { 36, 0, 0, 0 }, { 48, 52, 55, 59 }, { 60, 64, 67, 71 } }, notes_b[4][4] = { { 36, 0, 0, 0 }, { 38, 0, 0, 0 }, { 41, 45, 48, 53 }, { 65, 69, 72, 77 } };
    if (!begin("MIX")) return;
    fresh();
    notes_off = 1;                                      /* (the chords below are the notes) */
    G(G_BPM, 150);
    for (k = 0; k < NPART; k++) { T(k, P_REV, 64); T(k, P_DLY, 64); T(k, P_CHOR, 64); }
    G(G_SLMODE, 1); T(1, P_SLROUTE, 1);                   /* the SLICER's GATE on the drums */
    play();
    for (ps = 0; ps < 14u; ps++) {
        for (st = 0; st < 16u; st++) {
            const int *nt;
            tk = st % 4u;
            nt = (ps & 1u) ? notes_b[tk] : notes_a[tk];
            for (i = 0; i < 4u && nt[i]; i++) trk_note_on(&trk[tk], nt[i], 100);
            if (st == 2u && ps < 7u) perf_press(FLP_BUF[ps], 1);
            if (st == 2u && ps >= 7u) { perf_press(FLP_BUF[ps - 7u], 1); perf_press(ps & 1u ? PF_SCRU : PF_STHR, 1); perf_press(PF_M1 + ps % 4u, 1); perf_m[3] = (uint8_t)(ps * 7u); }
            if (st == 6u && ps >= 10u) perf_press(FLP_BUF[(ps - 4u) % 7u], 1);
            if (st == 12u) { for (i = 0; i < 7u; i++) perf_press(FLP_BUF[i], 0); }
            if (st == 14u) { perf_press(PF_SCRU, 0); perf_press(PF_STHR, 0); for (i = 0; i < NTRK; i++) perf_press(PF_M1 + i, 0); perf_m[3] = 0; }
            for (i = 0; i < 138u; i++) {
                if (i == 100u) for (uint32_t j = 0; j < 4u && nt[j]; j++) trk_note_off(&trk[tk], nt[j]);
                blk();
            }
        }
    }
    stop();
    for (i = 0; i < 200u; i++) blk();
    end("MIX");
}
static void scenario_random(void)
{
    uint32_t s = 0x1234567u, i, down = 0;
    if (!begin("RANDOM")) return;
    fresh();
    run(50);
#define RND() (s = s * 1664525u + 1013904223u, s >> 8)
    for (i = 0; i < 40000u; i++) {
        uint32_t r = RND() % 1000u;
        if (r < 28u) {                                  /* a key: down or up */
            static const uint8_t KEYS[13] = {PF_R8, PF_R16, PF_R32, PF_GLT, PF_TAPE, PF_SHUF, PF_FRZ, PF_SCRU, PF_STHR, PF_M1, PF_M1 + 1, PF_M1 + 2, PF_M1 + 3};
            uint32_t q = RND() % 16u, e = KEYS[q < 13u ? q : 3u + q % 4u];
            down ^= 1u << e;
            perf_press(e, (down >> e) & 1u);
        } else if (r < 60u) {                           /* a card's knob */
            uint32_t x = RND() % 4u, k = RND() % 4u;
            if (FXK_MIN[x][k] != FXK_MAX[x][k]) perf_gp[x][k] = (int8_t)(FXK_MIN[x][k] + (int32_t)(RND() % (uint32_t)(FXK_MAX[x][k] - FXK_MIN[x][k] + 1)));
        } else if (r < 75u) {
            perf_scr += (uint32_t)((int32_t)(RND() % 31u) - 15);
        } else if (r < 80u) {
            G(G_BPM, 45 + (int)(RND() % 250u));
        } else if (r < 84u) {
            if (song.playing) stop(); else play();
        } else if (r < 85u) {
            seq_start_count++;
        } else if (r < 87u) {
            perf_latch_on ^= 1u;
            if (!perf_latch_on) perf_latched = 0;
        } else if (r < 89u) {
            perf_kill ^= 1u;
        } else if (r < 90u) {
            perf_off_req = 1; down = 0;
        } else if (r < 92u) {
            if (sl_buf) jfx_sl_lost();
        } else if (r < 93u) {
            G(G_SLMODE, song.g[G_SLMODE] == SL_STUT ? 0 : SL_STUT);
        } else if (r < 95u) {
            perf_m[3] = (uint8_t)(RND() % 101u);
        } else if (r == 95u && !(RND() % 4u)) {
            jfx_clear_all();
        }
        notes();
        blk();
    }
#undef RND
    end("RANDOM");
}
static void scenario_stereo(void)
{
    static const int8_t SET[2][4][4] = {{{100, 5, 1, 3}, {0, 3, 40, 0}, {100, 2, 100, 7}, {70, -5, 60, 1}}, {{80, -7, 3, 9}, {0, 1, -60, 0}, {90, 1, 50, 2}, {100, 12, 100, 3}}};
    static const int8_t PAN[4] = {-50, 30, 60, -20};
    uint32_t x, k, pass, set;
    if (!begin("STEREO")) return;
    fresh();
    G(G_BPM, 220);
    G(G_RWIDE, 110); G(G_RMOD, 50);
    for (k = 0; k < NPART; k++) { T(k, P_PAN, PAN[k]); T(k, P_CHOR, 50); T(k, P_REV, 50); }
    run(200);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        for (set = 0; set < 2u; set++)
            for (x = 0; x < 4u; x++) {
                for (k = 0; k < 4u; k++) perf_gp[x][k] = SET[set][x][k];
                perf_press(FLP_E[x], 1); run(x == GX_TAPE ? 500u : 900u);
                if (x == GX_TAPE) { perf_scr += 9u; run(40); perf_scr -= 30u; run(120); }
                perf_press(FLP_E[x], 0); run(50);
            }
        perf_press(PF_R16, 1); run(400); perf_press(PF_R32, 1); run(200); perf_press(PF_R32, 0); perf_press(PF_R16, 0); run(50);
        stop();
        run(40);
    }
    end("STEREO");
}
static void scenario_scratch(void)
{
    static const int8_t FALL[] = {3, -2, 7, -11, 1, 20, -30, 5};
    static const int8_t FAST[] = {-64, 64, -100, 100, -127, 127, -63, 63, -65, 65, -1, 1, -40, 90, -127, -127, 127, 127, 127};
    uint32_t i, pass;
    if (!begin("SCRATCH")) return;
    fresh();
    run(100);
    for (pass = 0; pass < 2u; pass++) {
        if (pass) play();
        /* the knob turned while the tape still falls (TIME 1/4 at 120 BPM: half a second): the detents move the hand when it stops */
        G(G_BPM, 120);
        perf_press(PF_TAPE, 1);
        for (i = 0; i < sizeof FALL; i++) { run(60); perf_scr += (uint32_t)(int32_t)FALL[i]; }
        run(700);
        perf_scr += 4u; run(100);
        perf_press(PF_TAPE, 0); run(60);
        perf_gp[GX_TAPE][2] = 100;                      /* CURVE up: a longer fall */
        perf_press(PF_TAPE, 1); run(30); perf_scr -= 25u; run(30); perf_scr += 3u; run(900); perf_press(PF_TAPE, 0); run(60);
        perf_gp[GX_TAPE][2] = 0;
        /* fast turns at 300 BPM: a detent is 183 samples, 64 of them stay inside the loop */
        G(G_BPM, 300);
        perf_press(PF_TAPE, 1);
        run(500);
        for (i = 0; i < sizeof FAST; i++) { perf_scr += (uint32_t)(int32_t)FAST[i]; run(25); }
        run(100);
        perf_press(PF_TAPE, 0); run(60);
        /* .. and at 240 BPM with TIME 1/64 (stopped at once), the hand pulled back and let run forward */
        G(G_BPM, 240); perf_gp[GX_TAPE][1] = 0;
        perf_press(PF_TAPE, 1); run(100);
        for (i = 0; i < 12u; i++) { perf_scr -= 50u; run(8); }
        for (i = 0; i < 12u; i++) { perf_scr += 45u; run(8); }
        run(100);
        perf_press(PF_TAPE, 0); run(60);
        perf_gp[GX_TAPE][1] = 4;
        stop();
        run(40);
    }
    end("SCRATCH");
}
static void flp_scenarios(void)
{
    flp_nres = 0;
    scenario_fx(GX_GLT, "GLITCH");
    scenario_fx(GX_TAPE, "TAPE");
    scenario_fx(GX_SHUF, "SHUFFLE");
    scenario_fx(GX_FRZ, "FREEZE");
    scenario_tempo();
    scenario_switch();
    scenario_start();
    scenario_mem();
    scenario_mix();
    scenario_random();
    scenario_stereo();
    scenario_scratch();
}
#ifndef FLP_NO_MAIN
int main(void)
{
    if (getenv("FLP_ONLY")) snprintf(flp_only, sizeof flp_only, "%s", getenv("FLP_ONLY"));
    printf("fx_layer probe: MELODEE_FX_DEFER %d\n", (int)MELODEE_FX_DEFER);
    flp_scenarios();
    if (bad) { printf("fx_layer: %d FAILED\n", bad); return 1; }
    printf("fx_layer: done\n");
    return 0;
}
#endif
