/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: host test of the tracks' lists (firmware/src/oc_lists.c, build doc 4c step 1): build/host/oc_lists_test
 * On the simulated NOR flash, stopped:
 * 1. steps on a new project's track become pool pattern 01 when kept, named by entry A.
 * 2. any track can play any pattern: track 2 on an entry naming pattern 01 plays track 1's steps.
 * 3. an edit is kept back to the pattern it is; another track's unchanged copy does not undo it.
 * 4. ten patterns through one track's 8 banks: each plays as kept, edits kept before its bank is reused.
 * 5. a project keeps its lists, the entry each track is on and the patterns its banks are; two projects naming one
 *    pattern share it (Skiff).
 * 6. no free pattern: PATTERNS FULL, the track as it was.
 * Playing:
 * 7. SONG on: a track down its list, each entry its REPS passes, from flash through the bank cache; a stop drops the
 *    song's next.
 * 8. SONG off: an entry loops; a launch comes in at the loop end.
 *
 * Revisions:
 *   0.3.3: first version.
 *   0.3.4: SONG and launches while playing.
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static int lcheck(const char *what, int ok)
{
    printf("oc lists: %-92s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* track k's playing steps: one note `note` on step 0 (the rest empty) */
static void put_note(uint32_t k, uint32_t note)
{
    for (uint32_t i = 0; i < NSTEP; i++) step_clear(&trk[k].step[i]);
    trk[k].step[0] = (step_t){{(uint8_t)note}, 1, ST_NOTE, 0, 100, 0, 0};
}
static uint32_t note_of(uint32_t k) { return trk[k].step[0].time == ST_NOTE ? trk[k].step[0].note[0] : 0u; }
static uint32_t pool_note(uint32_t id)    /* pattern id's step 0 note, loaded into a scratch bank (track 4's bank 7) */
{
    pattern_t p;
    if (ocp_load(id, 3, 7, 1)) return 0;
    pattern_read(3, 7, &p);
    bank_pid[3][7] = 0;
    return p.step[0].time == ST_NOTE ? p.step[0].note[0] : 0u;
}

static int test_new_and_shared(void)
{
    int bad = 0;
    reset();
    put_note(0, 60);
    bad |= lcheck("a new project's steps, kept, are pattern 01 named by entry A",
                  ocl_keep_all() == 0 && bank_pid[0][trk[0].pattern] == 1u && ocl_ent_pat(0, 0) == 0u &&
                      lists.cur[0] == 0u && ocp.len[0] == 16u && pool_note(0) == 60u);
    ocl_set(1, 2, 0, 1);
    bad |= lcheck("track 2 on its entry C naming pattern 01 plays track 1's steps",
                  ocl_go(1, 2) == 0 && note_of(1) == 60u && lists.cur[1] == 2u);
    put_note(0, 62);
    bad |= lcheck("track 1's edit kept back to 01; track 2's unchanged copy does not undo it",
                  ocl_keep_all() == 0 && pool_note(0) == 62u && note_of(1) == 60u);
    put_note(1, 64);
    bad |= lcheck("then track 2's edit kept: the one kept last is the pattern", ocl_keep_all() == 0 && pool_note(0) == 64u);
    return bad;
}

static int test_cache(void)
{
    int bad = 0, ok = 1;
    reset();
    for (uint32_t i = 0; i < 10u; i++) {        /* ten patterns on track 1's entries A..J */
        if (i) {
            uint32_t b = ocl_victim(0);
            pattern_request(&trk[0], b);
            bank_pid[0][b] = 0;                    /* (a new pattern there, not an edit of the one it held) */
            for (uint32_t j = 0; j < NSTEP; j++) step_clear(&trk[0].step[j]);
            lists.cur[0] = (uint8_t)i;
        }
        put_note(0, 40u + i);
        ok &= ocl_keep_all() == 0 && ocl_ent_pat(0, i) == i;
    }
    bad |= lcheck("ten patterns made on track 1, entries A..J", ok);
    ok = 1;
    for (uint32_t r = 0; r < 3u; r++)
        for (uint32_t i = 0; i < 10u; i++) {
            ok &= ocl_go(0, i) == 0 && note_of(0) == 40u + i + r * 10u;
            put_note(0, 40u + i + (r + 1u) * 10u);   /* (an edit, kept when the bank is next reused or left) */
        }
    bad |= lcheck("three rounds through the ten with 8 banks: each as kept, every edit kept before its bank is reused", ok);
    ok = ocl_keep_all() == 0;
    for (uint32_t i = 0; i < 10u; i++) ok &= pool_note(i) == 70u + i;
    return bad | lcheck("... and in the pool after", ok);
}

static int test_project(void)
{
    int bad = 0;
    char n[16];
    reset();
    put_note(0, 50);
    ocl_set(0, 3, OCP_N, 1);
    project_save_as(0, "FIRST");               /* (pattern 01 on track 1's entry A) */
    ocl_set(1, 4, 0, 3);                       /* track 2's entry E names 01, REPS 3 */
    lists.song = 1;
    ocl_go(1, 4);
    project_save(0);
    wipe_runtime();
    project_load(0);
    bad |= lcheck("a project keeps its lists, REPS, SONG and the entry each track is on",
                  ocl_ent_pat(1, 4) == 0u && lists.ent[1][4][1] == 2u && lists.song == 1u && lists.cur[1] == 4u &&
                      note_of(1) == 50u && note_of(0) == 50u && bank_pid[1][trk[1].pattern] == 1u);
    reboot();
    wipe_runtime();
    project_load(0);
    bad |= lcheck("... after a reboot too", ocl_ent_pat(1, 4) == 0u && lists.cur[1] == 4u && note_of(1) == 50u &&
                                            project_name(0, n) && !strcmp(n, "FIRST"));
    project_save_as(1, "SECOND");              /* (the same project, saved as another: it names 01 too) */
    put_note(0, 52);
    project_save(1);
    wipe_runtime();
    project_load(0);
    bad |= lcheck("two projects naming one pattern share it: an edit saved in one is heard in the other",
                  note_of(0) == 52u && pool_note(0) == 52u);
    return bad;
}

static int test_full(void)
{
    reset();
    for (uint32_t id = 0; id < OCP_N; id++) {   /* every pattern used (and named: track 4's list can hold 16) */
        put_note(3, 30u + id % 60u);
        bank_pid[3][trk[3].pattern] = 0;
        lists.cur[3] = (uint8_t)(id % OCL_NENT);
        lists.ent[3][id % OCL_NENT][0] = 0;
        ocl_keep(3, trk[3].pattern);
    }
    put_note(0, 99);
    int rc = ocl_keep_all();
    return lcheck("no free pattern: PATTERNS FULL (4), the track's steps as they were",
                  rc == 4 && note_of(0) == 99u && !bank_pid[0][trk[0].pattern]);
}

/* --------------------------------------------- 7, 8. playing: SONG, launch --- */
/* patterns 01, 02, 03 (notes 60, 62, 64; LEN 4) on track 1's entries A, B, C; on A */
static void three_patterns(void)
{
    reset();
    song.g[G_BPM] = 120;
    for (uint32_t i = 0; i < 3u; i++) {
        if (i) {
            uint32_t b = ocl_victim(0);
            pattern_request(&trk[0], b);
            bank_pid[0][b] = 0;
            lists.cur[0] = (uint8_t)i;
        }
        put_note(0, 60u + 2u * i);
        trk[0].p[P_SLEN] = 4;
        ocl_keep_all();
    }
    ocl_go(0, 0);
}
/* play blocks, the main loop's polls between; the note of each pass of track 1 (its step 0) into got[] */
static uint32_t passes(uint32_t *got, uint32_t max, uint32_t blocks, int launch_at, uint32_t launch_e)
{
    int32_t o[2 * CTL];
    uint32_t n = 0, last = 0xFFFF;
    transport_req = 1;
    for (uint32_t b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        pattern_poll();
        ocl_poll();
        if ((int)b == launch_at) ocl_launch(0, launch_e);
        if (trk[0].seq_idx == 0 && last != 0 && n < max) got[n++] = note_of(0);
        last = trk[0].seq_idx;
    }
    transport_req = 2;
    mix_block(o, CTL);
    pattern_poll();
    ocl_poll();
    return n;
}
static int test_song(void)
{
    int bad = 0;
    uint32_t got[16], n, per;
    three_patterns();
    lists.ent[0][0][1] = 1;                    /* A: REPS 2 */
    lists.song = 1;
    per = seq_div_samples((uint32_t)trk[0].p[P_SDIV]) * 4u / CTL + 1u;
    n = passes(got, 9, per * 9u, -1, 0);
    {
        static const uint32_t want[9] = {60, 60, 62, 64, 60, 60, 62, 64, 60};
        int ok = n >= 8u;
        for (uint32_t i = 0; ok && i < 8u; i++) ok = got[i] == want[i];
        bad |= lcheck("SONG on: each pass A A B C A A B C (A's REPS 2), from flash through the bank cache", ok);
    }
    bad |= lcheck("... a stop drops the song's next: the track stays on the entry it played", !song.playing &&
                  trk[0].pattern_next == 0xff && ocl_ent_pat(0, lists.cur[0]) == (note_of(0) - 60u) / 2u);
    three_patterns();
    lists.song = 0;
    n = passes(got, 6, per * 5u, (int)per, 2);
    bad |= lcheck("SONG off: A loops; a launch of C comes in at the loop end, the track then on C",
                  n >= 4u && got[0] == 60u && got[1] == 60u && got[n - 1u] == 64u && lists.cur[0] == 2u);
    {   /* ten patterns through 8 banks while playing (clean banks reused) */
        uint32_t g2[24];
        reset();
        song.g[G_BPM] = 120;
        for (uint32_t i = 0; i < 10u; i++) {
            if (i) {
                uint32_t b = ocl_victim(0);
                pattern_request(&trk[0], b);
                bank_pid[0][b] = 0;
                lists.cur[0] = (uint8_t)i;
            }
            put_note(0, 40u + i);
            trk[0].p[P_SLEN] = 4;
            ocl_keep_all();
        }
        ocl_go(0, 0);
        lists.song = 1;
        n = passes(g2, 24, per * 22u, -1, 0);
        int ok = n >= 21u;
        for (uint32_t i = 0; ok && i < 21u; i++) ok = g2[i] == 40u + i % 10u;
        bad |= lcheck("SONG over ten patterns with 8 banks: each pass the right one, loaded while playing", ok);
    }
    return bad;
}

int main(void)
{
    int bad = 0;
    bad |= test_song();
    bad |= test_new_and_shared();
    bad |= test_cache();
    bad |= test_project();
    bad |= test_full();
    printf(bad ? "oc lists: FAILED\n" : "oc lists: all passed\n");
    return bad ? 1 : 0;
}
