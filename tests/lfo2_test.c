/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format, OC_LFO2): host test of LFO 2, firmware/src/mod.c (docs/orangecat/UI_BUILD.md "Part 2"):
 *   build/host/lfo2_test       (built with -DOC_R3_FORMAT=1 -DOC_LFO2=1, on mod_test's harness)
 * 1. off: a track with LFO 2 at its defaults, with a DEST and no AMT, or an AMT and no DEST renders bit-identically; one that is on
 *    changes the sound and leaves p[] as it was after the block.
 * 2. the waves at known phases, UNI's range, S&H from its own generator (the shared rng untouched).
 * 3. SYNC at 1/4 gives the increment LFO 1 gives with the same setting; RATE gives LFO_INC.
 * 4. TRIG NOTE restarts the phase on a fresh phrase only (no voice held); FREE never.
 * 5. the fifth slot alone on PITCH, CUT, PAN and E1; a stored slot with SRC LFO2; LFO2 -> DEPTH.
 * 6. unused: the phase holds, no work; a foreign track_t gets no LFO 2 and no crash.
 * 7. the cached path and the reference path leave the same state.
 * 8. the parameters: ids, ranges, defaults; the pages exist and LFO 1's page is LFO SYNC.
 * Revisions:
 *   Part 2 (October 31, 2026): first version. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("lfo2: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int32_t out_buf[2 * CTL];
static uint64_t hash;
static void blocks(uint32_t n)
{
    while (n--) {
        mix_block(out_buf, CTL);
        for (uint32_t i = 0; i < 2u * CTL; i++)
            for (uint32_t k = 0; k < 4u; k++) { hash ^= ((uint32_t)out_buf[i] >> (8u * k)) & 0xFFu; hash *= 0x100000001B3ull; }
    }
}
static void fresh(void)
{
    lim_env = LIM_T;
    memset(trk, 0, sizeof trk);
    memset(&mod, 0, sizeof mod);
    memset(lfo2, 0, sizeof lfo2);
    memset(lfo2_sync_cache, 0, sizeof lfo2_sync_cache);
    host_tracks_init();
    for (uint32_t k = 0; k < NPART; k++) host_preset(&trk[k], k ? 0u : HOST_SYNTH, 0u);
    song.sel = 0;
    hash = 0xCBF29CE484222325ull;
}
static void slot(track_t *t, uint32_t k, int32_t s, int32_t d, int32_t a)
{
    t->p[P_M1SRC + 3u * k] = (int16_t)s; t->p[P_M1DST + 3u * k] = (int16_t)d; t->p[P_M1AMT + 3u * k] = (int16_t)a;
}
static uint64_t phrase(void)
{
    track_t *t = &trk[0];
    trk_note_on(t, 60, 100); trk_note_on(t, 64, 70);
    blocks(FS / 4u / CTL);
    trk_note_on(t, 72, 90);
    blocks(FS / 8u / CTL);
    trk_note_off(t, 60); trk_note_off(t, 64); trk_note_off(t, 72);
    blocks(FS / 2u / CTL);
    return hash;
}
static uint64_t phrase_child(uint32_t k)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd)) return 0;
    fflush(stdout);
    if (!(pid = fork())) {
        int16_t keep[P_COUNT];
        fresh();
        if (k == 1u) { trk[0].p[P_L2DST] = 2; trk[0].p[P_L2AMT] = 0; }             /* a DEST and no AMT */
        else if (k == 2u) { trk[0].p[P_L2DST] = 0; trk[0].p[P_L2AMT] = 40; }        /* an AMT and no DEST */
        else if (k == 3u) { trk[0].p[P_L2DST] = 2; trk[0].p[P_L2AMT] = 50; trk[0].p[P_L2RATE] = 90; }   /* on: CUT */
        memcpy(keep, trk[0].p, sizeof keep);
        h = phrase();
        if (memcmp(keep, trk[0].p, sizeof keep) || mod.on) h = 0;
        if (write(fd[1], &h, sizeof h) != sizeof h) _exit(1);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h) h = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

int main(void)
{
    track_t *t;
    uint32_t i;
    /* 1 */
    {
        uint64_t h0 = phrase_child(0), h1 = phrase_child(1), h2 = phrase_child(2), h3 = phrase_child(3);
        check("LFO 2 at its defaults, with a DEST and no AMT, with an AMT and no DEST: bit-identical", h0 && h0 == h1 && h0 == h2);
        check("LFO 2 on (DEST CUT, AMT 50) changes the sound and leaves no modulated value in p[]", h3 && h3 != h0);
    }
    /* 8: the parameters */
    check("the seven parameters sit just before P_E0, P_COUNT 131, P_E0 123", P_L2RATE + 6 == P_L2AMT && P_L2AMT + 1 == P_E0 && P_COUNT == 131 && P_E0 == 123);
    check("defaults: RATE 60, WAVE SIN, SYNC OFF, TRIG NOTE, POL BI, DEST OFF, AMT 0",
          TP[P_L2RATE].def == 60 && TP[P_L2WAVE].def == 0 && TP[P_L2SYNC].def == 0 && TP[P_L2TRIG].def == 0 && TP[P_L2POL].def == 0 && TP[P_L2DST].def == 0 && TP[P_L2AMT].def == 0);
    check("ranges: AMT -64..63, DEST 0..15, WAVE 0..4, RATE 0..127", TP[P_L2AMT].min == -64 && TP[P_L2AMT].max == 63 && TP[P_L2DST].max == 15 && TP[P_L2WAVE].max == 4 && TP[P_L2RATE].max == 127);
    {
        uint32_t found2 = 0, foundsync = 0, foundl2sync = 0, ok = 1;
        for (i = 0; i < NPAGES; i++) {
            if (str_eq(PAGES[i].title, "LFO 2")) { found2 = 1; ok &= PAGES[i].id[0] == P_L2RATE && PAGES[i].id[3] == P_L2AMT; }
            if (str_eq(PAGES[i].title, "LFO SYNC")) { foundsync = 1; ok &= PAGES[i].id[0] == P_LSYNC; }
            if (str_eq(PAGES[i].title, "LFO 2 SYNC")) { foundl2sync = 1; ok &= PAGES[i].id[0] == P_L2SYNC; }
        }
        check("pages: LFO 2 (RATE WAVE DEST AMT), LFO 2 SYNC, and LFO 1's sync page renamed LFO SYNC", found2 && foundsync && foundl2sync && ok);
    }
    /* 2: the waves */
    check("SIN: zero at 0, +1 at a quarter, -1 at three quarters", lfo2_wave(0, 0, 0) / 64 == 0 && lfo2_wave(0, 0x40000000u, 0) > 32000 && lfo2_wave(0, 0xC0000000u, 0) < -32000);
    check("TRI: the bottom at 0, the middle at a quarter, the peak at half", lfo2_wave(1, 0, 0) == -32768 && lfo2_wave(1, 0x40000000u, 0) / 64 == 0 && lfo2_wave(1, 0x80000000u, 0) > 32000);
    check("SAW: -1 at 0, 0 at half, near +1 at the end", lfo2_wave(2, 0, 0) == -32768 && lfo2_wave(2, 0x80000000u, 0) == 0 && lfo2_wave(2, 0xFFFFFFFFu, 0) > 32000);
    check("SQR: +1 in the first half, -1 in the second", lfo2_wave(3, 0x10000000u, 0) == 32767 && lfo2_wave(3, 0xA0000000u, 0) == -32767);
    check("S&H: the held random value", lfo2_wave(4, 0, 0x80000000u) == 0 && lfo2_wave(4, 0x1234567u, 0xFFFF0000u) == 32767);
    /* the tick: RATE advances by LFO_INC, UNI, the generator's own */
    fresh(); t = &trk[0];
    t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40; t->p[P_L2RATE] = 80; t->p[P_L2WAVE] = 2;      /* SAW to PAN */
    mod_begin(t); mod_end(t);
    check("the tick: the phase advances by LFO_INC[RATE], the value is the wave of it, mod_l2v for this call", lfo2[0].ph == LFO_INC[80] && lfo2[0].val == lfo2_wave(2, LFO_INC[80], 0) && mod_l2v == lfo2[0].val);
    {
        int32_t lo = 0, hi = 0;
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40; t->p[P_L2RATE] = 127; t->p[P_L2WAVE] = 2; t->p[P_L2POL] = 1;
        for (i = 0; i < 60000u; i++) { mod_begin(t); mod_end(t); if (lfo2[0].val < lo) lo = lfo2[0].val; if (lfo2[0].val > hi) hi = lfo2[0].val; }
        check("POL UNI: 0..+1 across a SAW", lo >= 0 && hi > 30000);
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40; t->p[P_L2RATE] = 127; t->p[P_L2WAVE] = 2; lo = hi = 0;
        for (i = 0; i < 60000u; i++) { mod_begin(t); mod_end(t); if (lfo2[0].val < lo) lo = lfo2[0].val; if (lfo2[0].val > hi) hi = lfo2[0].val; }
        check("POL BI: a SAW is bipolar", lo < -30000 && hi > 30000);
    }
    {
        uint32_t rs = rng_state, r1, r2;
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40; t->p[P_L2RATE] = 127; t->p[P_L2WAVE] = 4;
        for (i = 0; i < 90000u; i++) { mod_begin(t); mod_end(t); }
        r1 = lfo2[0].rnd;
        for (i = 0; i < 90000u; i++) { mod_begin(t); mod_end(t); }
        r2 = lfo2[0].rnd;
        check("S&H draws from LFO 2's own generator (the shared rng untouched), a new value each cycle", rng_state == rs && r1 && r2 && r1 != r2);
    }
    /* 3: SYNC */
    fresh(); t = &trk[0];
    t->p[P_L2SYNC] = 5; t->p[P_LSYNC] = 5; song.g[G_BPM] = 120;
    check("SYNC 1/4 at 120 BPM: the increment LFO 1 gives with the same setting", lfo2_sync_inc(t, 0) == lfo_sync_inc(t) && lfo2_sync_inc(t, 0) != 0);
    t->p[P_L2SYNC] = 8; t->p[P_LSYNC] = 8;
    { uint32_t i16 = lfo2_sync_inc(t, 0), i4; t->p[P_L2SYNC] = 5; i4 = lfo2_sync_inc(t, 0); check("... 1/16: four times as fast, and the same as LFO 1's", i16 == lfo_sync_inc((t->p[P_LSYNC] = 8, t)) && i16 > 3u * i4 && i16 < 5u * i4); }
    /* 4: TRIG */
    fresh(); t = &trk[0];
    t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40;
    lfo2[0].ph = 0x30000000u;
    trk_note_on(t, 60, 100);
    check("TRIG NOTE: a first note (no voice held) restarts LFO 2's phase", lfo2[0].ph == 0);
    lfo2[0].ph = 0x30000000u;
    trk_note_on(t, 64, 100);
    check("... a second note while one is held does not", lfo2[0].ph == 0x30000000u);
    trk_note_off(t, 60); trk_note_off(t, 64);
    fresh(); t = &trk[0];
    t->p[P_L2DST] = 5; t->p[P_L2AMT] = 40; t->p[P_L2TRIG] = 1;
    lfo2[0].ph = 0x30000000u;
    trk_note_on(t, 60, 100);
    check("TRIG FREE: a note never restarts it", lfo2[0].ph == 0x30000000u);
    /* 5: the fifth slot */
    {
        vmod_t m;
        int16_t pan0;
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 5; t->p[P_L2AMT] = 63; t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;       /* SQR: +1 for the first half */
        pan0 = t->p[P_PAN] = 0;
        mod_begin(t);
        check("the fifth slot alone on PAN: the SQR's +1 at AMT 63 takes PAN to its top (63), and mod_end puts it back",
              t->p[P_PAN] == 63 && (mod_end(t), t->p[P_PAN] == pan0));
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 1; t->p[P_L2AMT] = 20; t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;
        trk_note_on(t, 60, 100);
        mod_begin(t);
        memset(&m, 0, sizeof m); m.pitch16 = 1000; m.inc = pitch_inc(1000);
        mod_voice(t, &t->v[0], &m, 0);
        check("... on PITCH: a voice's pitch rises by (32767 x 20 x 3) >> 15", m.pitch16 == 1000 + ((32767 * 20 * 3) >> 15));
        mod_end(t);
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 2; t->p[P_L2AMT] = 30; t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;
        trk_note_on(t, 60, 100);
        mod_begin(t);
        memset(&m, 0, sizeof m);
        mod_voice(t, &t->v[0], &m, 0);
        check("... on CUT: (32767 x 30) >> 7", m.cutoff == (32767 * 30) >> 7);
        mod_end(t);
        fresh(); t = &trk[0];
        t->p[P_L2DST] = 8; t->p[P_L2AMT] = 32; t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;       /* E1 */
        {
            const param_desc_t *pd = &ENGINES[t->engine]->edit[0];
            int16_t e0 = t->p[P_E0];
            mod_begin(t);
            check("... on E1: the engine's first edit parameter moves, within its range, and is put back",
                  (t->p[P_E0] != e0 || pd->max == pd->min || pd->def >= pd->max) && t->p[P_E0] >= pd->min && t->p[P_E0] <= pd->max && (mod_end(t), t->p[P_E0] == e0));
        }
        fresh(); t = &trk[0];
        t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;
        slot(t, 0, MS_LFO2, MD_CUT, 30);
        trk_note_on(t, 60, 100);
        mod_begin(t);
        memset(&m, 0, sizeof m);
        mod_voice(t, &t->v[0], &m, 0);
        check("a stored slot with SRC LFO2 (and the fifth slot off) works: -> CUT", m.cutoff == (32767 * 30) >> 7 && lfo2[0].ph != 0);
        mod_end(t);
        fresh(); t = &trk[0];
        t->p[P_L2WAVE] = 3; t->p[P_L2RATE] = 0;
        slot(t, 0, MS_LFO2, MD_DEPTH, -64);

        t->lfo_val = 32767; t->lfo_fade = 32767;
        mod_begin(t);
        check("LFO2 -> DEPTH: LFO 1's depth is scaled by it", mod.ldep < 32767);
        mod_end(t);
    }
    /* 6: unused, foreign */
    fresh(); t = &trk[0];
    lfo2[0].ph = 12345u;
    for (i = 0; i < 100u; i++) { mod_begin(t); mod_end(t); }
    check("unused: the phase holds, the value reads 0", lfo2[0].ph == 12345u && mod_l2v == 0 && !mod.on);
    {
        track_t foreign;
        memcpy(&foreign, &trk[0], sizeof foreign);
        foreign.p[P_L2DST] = 5; foreign.p[P_L2AMT] = 50;
        mod_begin(&foreign); mod_end(&foreign);
        check("a track_t that is not one of trk[]: no LFO 2, no crash, no state touched", mod_l2v == 0 && lfo2[0].ph == 12345u);
    }
    /* 7: cached = reference */
    {
        int16_t p_ref[P_COUNT];
        mod_route_t save_routes;
        int ok = 1;
        for (uint32_t trial = 0; trial < 8u && ok; trial++) {
            fresh(); t = &trk[0];
            t->p[P_L2DST] = (int16_t)(trial % 16u); t->p[P_L2AMT] = (int16_t)(20 + 5 * trial); t->p[P_L2WAVE] = (int16_t)(trial % 5u); t->p[P_L2RATE] = 100; t->p[P_L2POL] = (int16_t)(trial & 1u);
            slot(t, 1, MS_LFO2, MD_PAN, (int32_t)trial * 7 - 20);
            slot(t, 2, MS_LFO, MD_E1 + 2, 30);
            t->lfo_val = 20000; t->lfo_fade = 32767;
            memset(&mod, 0, sizeof mod);
            uint32_t ph0 = lfo2[0].ph;
            int r1 = mod_begin_reference(t);
            memcpy(p_ref, t->p, sizeof p_ref);
            typeof(mod) mod_ref = mod;
            int32_t v_ref = lfo2[0].val;
            mod_end(t);
            lfo2[0].ph = ph0;
            memset(&mod, 0, sizeof mod);
            memset(mod_routes, 0, sizeof mod_routes);
            int r2 = mod_begin(t);
            ok &= r1 == r2 && !memcmp(p_ref, t->p, sizeof p_ref) && lfo2[0].val == v_ref && mod_ref.nv == mod.nv && mod_ref.nk == mod.nk &&
                  mod_ref.pit == mod.pit && mod_ref.cut == mod.cut && mod_ref.shp == mod.shp && mod_ref.gain == mod.gain && mod_ref.ldep == mod.ldep;
            mod_end(t);
            (void)save_routes;
        }
        check("the cached path and the reference path leave the same p[], the same terms, the same LFO 2 value (8 setups)", ok);
    }
    printf(bad ? "LFO2 TEST FAILED\n" : "lfo2 test passed\n");
    return bad != 0;
}
