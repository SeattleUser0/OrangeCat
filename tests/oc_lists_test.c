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
 * 7. CHAIN: a track down its chain, each step its REPS passes, from flash through the bank cache; a stop drops the
 *    chain's next.
 * 8. PHRASE: a step loops; a launch comes in at the loop end.
 * 9. SONG: each track down its own column of the song, a row's chain then the next row's, the end of its block of rows
 *    back to the block's top; from a row picked.
 * 10. an 0.3 project (OCJ2) loads as chains 01..04 and row 01, and saves as OCJ3.
 *
 * Revisions:
 *   0.3.3: first version.
 *   0.3.4: SONG and launches while playing.
 *   0.4.0: chains and the song (9, 10).
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
    lists.mode = OCL_CHAIN;
    ocl_go(1, 4);
    project_save(0);
    wipe_runtime();
    project_load(0);
    bad |= lcheck("a project keeps its chains, REPS, PLAY's mode and the step each track is on",
                  ocl_ent_pat(1, 4) == 0u && OCL_ENT(1, 4)[1] == 2u && lists.mode == OCL_CHAIN && lists.cur[1] == 4u &&
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
        OCL_ENT(3, id % OCL_NENT)[0] = 0;
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
    OCL_ENT(0, 0)[1] = 1;                    /* A: REPS 2 */
    lists.mode = OCL_CHAIN;
    per = seq_div_samples((uint32_t)trk[0].p[P_SDIV]) * 4u / CTL + 1u;
    n = passes(got, 9, per * 9u, -1, 0);
    {
        static const uint32_t want[9] = {60, 60, 62, 64, 60, 60, 62, 64, 60};
        int ok = n >= 8u;
        for (uint32_t i = 0; ok && i < 8u; i++) ok = got[i] == want[i];
        bad |= lcheck("CHAIN: each pass A A B C A A B C (A's REPS 2), from flash through the bank cache", ok);
    }
    bad |= lcheck("... a stop drops the song's next: the track stays on the entry it played", !song.playing &&
                  trk[0].pattern_next == 0xff && ocl_ent_pat(0, lists.cur[0]) == (note_of(0) - 60u) / 2u);
    three_patterns();
    lists.mode = OCL_PHRASE;
    n = passes(got, 6, per * 5u, (int)per, 2);
    bad |= lcheck("PHRASE: A loops; a launch of C comes in at the loop end, the track then on C",
                  n >= 4u && got[0] == 60u && got[1] == 60u && got[n - 1u] == 64u && lists.cur[0] == 2u);
    {   /* a rest: A (REPS 1), an empty B, C: A, silence, C, A .. */
        uint32_t g3[12];
        three_patterns();
        OCL_ENT(0, 1)[0] = 0;                    /* (B emptied: a rest between A and C) */
        lists.mode = OCL_CHAIN;
        n = passes(g3, 12, per * 7u, -1, 0);
        int ok = n >= 6u && g3[0] == 60u && g3[1] == 0u && g3[2] == 64u && g3[3] == 60u && g3[4] == 0u && g3[5] == 64u;
        bad |= lcheck("CHAIN: an empty step between phrases is a rest (silence for its REPS), A - C A - C", ok);
    }
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
        lists.mode = OCL_CHAIN;
        n = passes(g2, 24, per * 22u, -1, 0);
        int ok = n >= 21u;
        for (uint32_t i = 0; ok && i < 21u; i++) ok = g2[i] == 40u + i % 10u;
        bad |= lcheck("CHAIN over ten patterns with 8 banks: each pass the right one, loaded while playing", ok);
    }
    return bad;
}

static int test_unused(void)
{
    reset();
    put_note(0, 50); ocl_keep_all();            /* 01, named by track 1's entry A */
    ocl_go_empty(0, 1); put_note(0, 51); ocl_keep_all();   /* 02 on B */
    ocl_go(0, 0);
    OCL_ENT(0, 1)[0] = 0;                     /* (B let go: 02 is still in a bank of track 1 .. */
    bank_pid[0][ocl_bank_of(0, 1)] = 0;         /* .. and that bank given up too: nothing names 02) */
    int n = ocl_clear_unused();
    return lcheck("Free unused phrases: the one nothing names emptied, the others kept",
                  n == 1 && !ocp.len[1] && ocp.len[0] && note_of(0) == 50u);
}

/* (0.6.1) an empty phrase given a clip (card_browse.c): ocl_keep_any numbers it, the pool's CRC its own; kept, and a
 * phrase with steps keeps as ocl_keep does */
static int test_keep_any(void)
{
    reset();
    uint32_t b = trk[0].pattern % NPAT, id;
    int ok = !bank_pid[0][b] && !ocl_bank_hash(0, b) && !ocl_keep(0, b) && !bank_pid[0][b];   /* (keep: nothing to keep) */
    ok &= !ocl_keep_any(0, b);
    id = bank_pid[0][b];
    ok &= id >= 1u && id <= OCP_N && ocp.hash[id - 1u] == ocl_bank_hash(0, b) && ocl_free_pat() != id - 1u;
    ok &= !ocl_keep_any(0, b) && bank_pid[0][b] == id;     /* (again: the same number) */
    put_note(0, 60);
    ok &= !ocl_keep_any(0, b) && bank_pid[0][b] == id && ocp.hash[id - 1u] == ocl_bank_hash(0, b);
    return lcheck("an empty phrase kept for a clip gets its own number; kept again, the same", ok);
}

static int test_melodee_lists(void)
{
    static uint8_t fbkk[BANK_STORE_SIZE];
    reset();
    for (uint32_t b = 0; b < 3u; b++) {          /* banks 1..3 of track 1 used, as a Melodee project has them */
        pattern_request(&trk[0], b);
        put_note(0, 70u + b);
    }
    pattern_request(&trk[0], 1);
    project_capture(&proj_scratch);
    bank_pack(fbkk, &proj_scratch, 1);
    proj_x_tag = 0;                              /* (from a backup: no lists beside it) */
    memcpy(proj_wire_u.raw, fbkk, sizeof fbkk);
    proj_write_slot(2, proj_wire_u.raw, BANK_STORE_SIZE);
    wipe_runtime();
    project_load(2);
    return lcheck("a Melodee project (no lists): its banks in use become chain 01's steps A B C, on B as its bank was",
                  OCL_ENT(0, 0)[0] && OCL_ENT(0, 1)[0] && OCL_ENT(0, 2)[0] && !OCL_ENT(0, 3)[0] &&
                  lists.cur[0] == 1u && note_of(0) == 71u && lists.tchain[0] == 0u && lists.song[0][0] == 1u);
}

static int test_named_empty(void)
{
    uint32_t got[12], n, per;
    int bad = 0;
    three_patterns();                           /* 01 02 03 on A B C */
    OCL_ENT(0, 3)[0] = 10;                    /* D names 10: nothing in it yet */
    lists.mode = OCL_CHAIN;
    per = seq_div_samples((uint32_t)trk[0].p[P_SDIV]) * 4u / CTL + 1u;
    n = passes(got, 12, per * 9u, -1, 0);
    bad |= lcheck("CHAIN: a step naming an empty pattern plays silent and the song goes on: 01 02 03 - 01 02 03 -",
                  n >= 8u && got[0] == 60u && got[1] == 62u && got[2] == 64u && got[3] == 0u && got[4] == 60u &&
                      got[7] == 0u);
    ocl_go(0, 3);
    put_note(0, 77);
    ocl_keep_all();
    bad |= lcheck("... going there and recording keeps the steps as pattern 10", ocp.len[9] && pool_note(9) == 77u &&
                  OCL_ENT(0, 3)[0] == 10u);
    return bad;
}

/* --------------------------------------------- 9. SONG: rows of chains --- */
static int test_song_rows(void)
{
    uint32_t got[16], n, per;
    int bad = 0;
    three_patterns();                           /* 01 02 03 on chain 01's steps A B C */
    OCL_ENT(0, 2)[0] = 0;                       /* chain 01: 01 02 */
    lists.chain[4][0][0] = 3;                   /* chain 05: 03 */
    memset(lists.song, 0, sizeof lists.song);
    lists.song[0][0] = 1; lists.song[1][0] = 5; /* track 1: row 01 chain 01, row 02 chain 05, row 03 empty */
    lists.song[0][1] = 2;                       /* track 2: row 01 chain 02 (nothing in it: silent), then nothing */
    per = seq_div_samples((uint32_t)trk[0].p[P_SDIV]) * 4u / CTL + 1u;
    bad |= lcheck("SONG from row 01: track 1 on chain 01 step A, the mode the song's",
                  ocl_song_start(0) == 0 && lists.mode == OCL_SONG && lists.tchain[0] == 0u && lists.cur[0] == 0u &&
                      lists.row[0] == 0u && note_of(0) == 60u);
    n = passes(got, 9, per * 8u, -1, 0);
    bad |= lcheck("... 01 02 (row 01's chain) 03 (row 02's), then the block's top again: 01 02 03 01 02",
                  n >= 7u && got[0] == 60u && got[1] == 62u && got[2] == 64u && got[3] == 60u && got[4] == 62u &&
                      got[5] == 64u && got[6] == 60u);
    ocl_go_chain(0, 0);
    bad |= lcheck("SONG from row 02: track 1 on chain 05 (03), track 2 out of the song (its cell empty)",
                  ocl_song_start(1) == 0 && lists.tchain[0] == 4u && note_of(0) == 64u && lists.row[1] == OCL_NOROW);
    n = passes(got, 9, per * 6u, -1, 0);
    bad |= lcheck("... 03, then row 03 empty: the top of its block, row 01: 03 01 02 03 01",
                  n >= 5u && got[0] == 64u && got[1] == 60u && got[2] == 62u && got[3] == 64u && got[4] == 60u);
    lists.song[3][0] = 5;                       /* a second block: rows 04 .. 04, chain 05 alone */
    ocl_go_chain(0, 0);
    n = (ocl_song_start(3), passes(got, 9, per * 4u, -1, 0));
    bad |= lcheck("a block of one row, one step: it loops, 03 03 03", n >= 3u && got[0] == 64u && got[1] == 64u &&
                  got[2] == 64u && lists.row[0] == 3u);
    {
        uint32_t c = ocl_free_chain();
        bad |= lcheck("a free chain: none in the song, on no track, naming nothing (chain 03 is track 3's)",
                      c != OCL_NCHAIN && c != 0u && c != 1u && c != 2u && c != 3u && c != 4u && !ocl_chain_any(c));
        bad |= lcheck("SONG's Copy chain: the cell's chain copied to a new number",
                      ocl_chain_clone(1, 0) == 0 && lists.song[1][0] == c + 1u && lists.chain[c][0][0] == 3u);
    }
    ocl_row_insert(0);
    bad |= lcheck("a row inserted: the rows after it a place down, the track's row with them",
                  !lists.song[0][0] && lists.song[1][0] == 1u && lists.song[4][0] == 5u);
    ocl_row_delete(0);
    bad |= lcheck("... and deleted again", lists.song[0][0] == 1u && lists.song[3][0] == 5u);
    return bad;
}

/* --------------------------------------------- 11. launch points, staged mutes --- */
/* 01 02 03 on track 1 (LEN 3: its loop ends off the beat), on A, playing; at block `at` a launch of C by hand (or a
 * MUTE staged); the time (samples from PLAY) of the step C came in on (or the MUTE turned), and its step there */
static int launch_run(uint32_t mode, uint32_t at, int mute, uint32_t *when, uint32_t *idx)
{
    int32_t o[2 * CTL];
    uint32_t b, from = trk[0].pattern;
    three_patterns();
    for (uint32_t k = 0; k < 3u; k++) { ocl_go(0, k); trk[0].p[P_SLEN] = 3; ocl_keep_all(); }
    ocl_go(0, 0);
    from = trk[0].pattern;
    lists.mode = OCL_PHRASE;
    lists.launch = (uint8_t)mode;
    trk[0].p[P_MUTE] = 0;
    transport_req = 1;
    *when = 0xFFFFFFFFu;
    for (b = 0; b < 4000u; b++) {
        mix_block(o, CTL);
        pattern_poll();
        ocl_poll();
        if (b == at) {
            from = trk[0].pattern;
            if (mute) trk[0].mute_arm = 2;
            else ocl_launch(0, 2);
        }
        if (b > at && *when == 0xFFFFFFFFu && (mute ? trk[0].p[P_MUTE] != 0 : trk[0].pattern != from)) {
            *when = b * CTL - trk[0].seq_pos;   /* (from step 0's start: the end of PLAY's first block) */
            *idx = trk[0].seq_idx;
            break;
        }
    }
    transport_req = 2;
    mix_block(o, CTL);
    pattern_poll(); ocl_poll();
    return *when != 0xFFFFFFFFu;
}
static int test_launch(void)
{
    int bad = 0;
    uint32_t when, idx, beat, step, at = 211u, t0;
    three_patterns();
    song.g[G_BPM] = 120;
    beat = seq_beat_samples();
    step = seq_div_samples((uint32_t)trk[0].p[P_SDIV]);
    t0 = at * CTL;
    bad |= lcheck("LOOP: a launch comes in at the track's loop end (a step that is a multiple of LEN 3)",
                  launch_run(OCL_LOOP, at, 0, &when, &idx) && idx == 0u && when >= t0 &&
                      (when + step / 2u) / step % 3u == 0u && when - t0 < 3u * step + step);
    bad |= lcheck("BAR: on the next bar line, from the pattern's first step",
                  launch_run(OCL_BAR, at, 0, &when, &idx) && idx == 0u && when >= t0 &&
                      (when + step / 2u) % (4u * beat) < step && when - t0 <= 4u * beat + step &&
                      lists.cur[0] == 2u);
    bad |= lcheck("BEAT: on the next beat line, from the pattern's first step",
                  launch_run(OCL_BEAT, at, 0, &when, &idx) && idx == 0u && when >= t0 &&
                      (when + step / 2u) % beat < step && when - t0 <= beat + step);
    bad |= lcheck("NOW: the next step, going on from the step it is at",
                  launch_run(OCL_NOW, at, 0, &when, &idx) && when >= t0 && when - t0 <= step + CTL);
    bad |= lcheck("a staged MUTE with BAR: it turns on the next bar line",
                  launch_run(OCL_BAR, at, 1, &when, &idx) && (when + step / 2u) % (4u * beat) < step &&
                      when - t0 <= 4u * beat + step && !trk[0].mute_arm);
    trk[0].p[P_MUTE] = 0;
    lists.launch = OCL_LOOP;
    return bad;
}

/* --------------------------------------------- 10. an 0.3 project --- */
static uint8_t rec_buf[64000], rec_dec[sizeof(ocl_lists_t) + PROJ_STORE_SIZE], rec_out[64000];
static int test_ocj2(void)
{
    ocj_head_t h;
    ocj_head2_t h2;
    uint32_t n, i, o, z;
    int bad = 0;
    three_patterns();                           /* 01 02 03 on track 1's A B C, on A */
    ocl_set(1, 0, 0, 2);                        /* track 2's A: 01, REPS 2 */
    project_save_as(0, "OLD");
    n = (uint32_t)ocs_read(OCS_PROJECT, 0, rec_buf, sizeof rec_buf);
    memcpy(&h, rec_buf, sizeof h);
    for (i = sizeof h, o = 0; i < n;) {         /* (its coded part: the lists, then the project part) */
        uint8_t v = rec_buf[i++];
        if (v) { rec_dec[o++] = v; continue; }
        for (z = rec_buf[i++]; z; z--) rec_dec[o++] = 0;
    }
    bad |= lcheck("an OCJ3 record: its head, then the lists and the project part coded",
                  h.magic == OCJ_MAGIC && h.len == n && o == sizeof rec_dec);
    memset(&h2, 0, sizeof h2);                  /* as 0.3.7 wrote it: OCJ2, the lists one a track at the head's end */
    h2.magic = OCJ_MAGIC2;
    memcpy(h2.pid, h.pid, sizeof h2.pid); memcpy(h2.active, h.active, sizeof h2.active);
    memcpy(h2.chain, h.chain, sizeof h2.chain); memcpy(h2.name, h.name, sizeof h2.name); memcpy(h2.fn, h.fn, sizeof h2.fn);
    {
        const ocl_lists_t *l = (const ocl_lists_t *)rec_dec;
        for (uint32_t k = 0; k < NTRK; k++) memcpy(h2.lists.ent[k], l->chain[k], sizeof h2.lists.ent[k]);
        memcpy(h2.lists.cur, l->cur, sizeof h2.lists.cur);
        h2.lists.song = 1;
    }
    o = sizeof h2;
    for (i = sizeof(ocl_lists_t); i < sizeof rec_dec;) {   /* the project part alone, coded */
        if (rec_dec[i]) { rec_out[o++] = rec_dec[i++]; continue; }
        for (z = 0; z < 255u && i < sizeof rec_dec && !rec_dec[i]; z++, i++)
            ;
        rec_out[o++] = 0; rec_out[o++] = (uint8_t)z;
    }
    h2.len = o; h2.zlen = o - (uint32_t)sizeof h2;
    memcpy(rec_out, &h2, sizeof h2);
    ocs_write(OCS_PROJECT, 0, rec_out, o);
    reboot();
    wipe_runtime();
    project_load(0);
    bad |= lcheck("an 0.3 project loads: each track's list its chain, row 01 naming them, SONG on as the chains",
                  ocl_ent_pat(0, 0) == 0u && ocl_ent_pat(0, 1) == 1u && ocl_ent_pat(0, 2) == 2u &&
                      ocl_ent_pat(1, 0) == 0u && OCL_ENT(1, 0)[1] == 1u && lists.tchain[0] == 0u &&
                      lists.tchain[1] == 1u && lists.song[0][0] == 1u && lists.song[0][1] == 2u &&
                      lists.mode == OCL_CHAIN && lists.cur[0] == 0u && note_of(0) == 60u);
    project_save(0);
    n = (uint32_t)ocs_read(OCS_PROJECT, 0, rec_buf, sizeof rec_buf);
    memcpy(&h, rec_buf, sizeof h);
    wipe_runtime();
    project_load(0);
    bad |= lcheck("... saved again, as OCJ3, and loads the same", h.magic == OCJ_MAGIC && ocl_ent_pat(0, 2) == 2u &&
                  lists.tchain[1] == 1u && lists.song[0][1] == 2u && lists.mode == OCL_CHAIN && note_of(0) == 60u &&
                  h.lsize == sizeof(ocl_lists_t));
    {   /* a later version's record: its lists 40 bytes longer (what this one does not know: passed over) */
        uint32_t extra = 40u;
        for (i = sizeof h, o = 0; i < n;) {
            uint8_t v = rec_buf[i++];
            if (v) { rec_dec[o++] = v; continue; }
            for (z = rec_buf[i++]; z; z--) rec_dec[o++] = 0;
        }
        o = sizeof h;
        for (i = 0; i < sizeof rec_dec + extra;) {
            uint32_t at = i < sizeof(ocl_lists_t) ? i : i < sizeof(ocl_lists_t) + extra ? 0xFFFFFFFFu : i - extra;
            uint8_t v = at == 0xFFFFFFFFu ? (uint8_t)(i & 1u ? 0x5Au : 0) : rec_dec[at];
            if (v) { rec_out[o++] = v; i++; continue; }
            for (z = 0; z < 255u && i < sizeof rec_dec + extra; z++, i++) {
                at = i < sizeof(ocl_lists_t) ? i : i < sizeof(ocl_lists_t) + extra ? 0xFFFFFFFFu : i - extra;
                if (at == 0xFFFFFFFFu ? (i & 1u) : rec_dec[at]) break;
            }
            rec_out[o++] = 0; rec_out[o++] = (uint8_t)z;
        }
        h.lsize += extra; h.len = o; h.zlen = o - (uint32_t)sizeof h;
        memcpy(rec_out, &h, sizeof h);
        ocs_write(OCS_PROJECT, 0, rec_out, o);
        reboot(); wipe_runtime(); project_load(0);
        bad |= lcheck("a later version's longer lists: read as far as this one's go, the project the same",
                      ocl_ent_pat(0, 2) == 2u && lists.tchain[1] == 1u && lists.song[0][1] == 2u && note_of(0) == 60u);
    }
    return bad;
}

/* --------------------------------------------- 13. (0.4.5) groove --- */
static int test_groove(void)
{
    int32_t o[2 * CTL];
    int bad = 0, ok = 1;
    uint32_t period, sum = 0, t4 = 0xFFFFFFFFu, last = 0xFFFF;
    reset();
    song.g[G_BPM] = 120;
    trk[0].p[P_SLEN] = 16;
    put_note(0, 60);
    trk[0].step[4] = trk[0].step[0];
    lists.groove[0] = 1; lists.gamt[0] = 100;   /* LAID BACK: step 4 a tenth of a step late */
    period = seq_div_samples((uint32_t)trk[0].p[P_SDIV]);
    for (uint32_t i = 0; i < 16u; i++) {
        ok &= step_samples(&trk[0], period, i) > period / 2u;
        sum += step_samples(&trk[0], period, i);
    }
    bad |= lcheck("groove: the steps add up to the straight loop (the loop's end on the grid)", ok && sum == 16u * period &&
                  recording_prefix(&trk[0], period, 16) == 16u * period);
    bad |= lcheck("... and a step starts where the template says (LAID BACK: step 5 a tenth late)",
                  recording_prefix(&trk[0], period, 4) == 4u * period + period / 100u * 10u);
    transport_req = 1;
    for (uint32_t b = 0; b < 2000u && t4 == 0xFFFFFFFFu; b++) {
        mix_block(o, CTL);
        if (trk[0].seq_idx == 4u && last != 4u) t4 = b * CTL - trk[0].seq_pos;
        last = trk[0].seq_idx;
    }
    transport_req = 2; mix_block(o, CTL);
    bad |= lcheck("... the sequencer plays it there", t4 != 0xFFFFFFFFu &&
                  (t4 > 4u * period + period / 10u ? t4 - (4u * period + period / 10u) : 4u * period + period / 10u - t4) <
                      CTL + period / 100u);
    bad |= lcheck("... its velocity: the template's accent on top (LAID BACK +8 on step 1)",
                  groove_vel(&trk[0], 0) == 8 && groove_vel(&trk[0], 1) == -6);
    {   /* a recorded note just before step 5 (as grooved), quantized on the steps' own grid: onto step 5 as grooved */
        recorded_note_t r;
        uint32_t q = (uint32_t)trk[0].p[P_SDIV] + 1u, want = recording_prefix(&trk[0], period, 4);
        memset(&r, 0, sizeof r);
        r.step = 3; r.on = (uint16_t)(RECORD_UNIT * 97u / 100u); r.vel = 100; r.note = 60;
        bad |= lcheck("... a recorded note quantized on the step grid lands where the grooved step plays",
                      recording_quant_on(&trk[0], &r, period, q) == want && want != 4u * period);
    }
    {   /* (a review) a QNT take committed: each note stored where the grooved step plays, on the right step */
        recorded_note_t r, out;
        uint32_t q = (uint32_t)trk[0].p[P_SDIV] + 1u, okq = 1;
        lists.groove[0] = 3;                    /* DILLA, 100 % */
        for (uint32_t st = 0; st < 16u; st++) {
            uint32_t want = recording_prefix(&trk[0], period, st), pos, span, at, got;
            uint32_t c = recording_step_of(&trk[0], period, want + 200u, &pos, &span);
            memset(&r, 0, sizeof r);
            r.step = (uint8_t)c; r.on = (uint16_t)((uint64_t)pos * RECORD_UNIT / span); r.vel = 100; r.note = 60;
            at = recording_quant_on(&trk[0], &r, period, q);
            recording_quant_position(&trk[0], period, &r, at, 16, &out);
            got = recording_raw_on(&trk[0], &out, period);
            okq &= c == st && at == want && (out.step & 63u) == st && (got > want ? got - want : want - got) <= 2u;
        }
        bad |= lcheck("... a QNT take committed: every note stored on its grooved step (DILLA, all 16)", okq);
        {   /* QNT 1/32 on a 1/16 track: the 1/32 grid swung only, not grooved */
            uint32_t g32 = seq_div_samples(3), at = 5u * g32;
            memset(&r, 0, sizeof r);
            r.step = (uint8_t)recording_step_of(&trk[0], period, at, &at, &q);
            r.on = (uint16_t)((uint64_t)at * RECORD_UNIT / q);
            bad |= lcheck("... QNT on another grid: a note on its straight point stays there",
                          recording_quant_on(&trk[0], &r, period, 4) == 5u * g32);
        }
        lists.groove[0] = 1;
    }
    lists.gamt[0] = 50;
    bad |= lcheck("... the amount scales both", recording_prefix(&trk[0], period, 4) == 4u * period + period / 100u * 5u &&
                  groove_vel(&trk[0], 0) == 4);
    lists.groove[0] = 0; lists.gamt[0] = 100;
    {   /* (a review) the timed notes' schedule key: any one field changed alone changes it, groove on or off */
        uint32_t k0 = recording_key(&trk[0]), okk;
        song.g[G_SWING] += 1; okk = recording_key(&trk[0]) != k0; song.g[G_SWING] -= 1;
        trk[0].p[P_SLEN] = 18; okk &= recording_key(&trk[0]) != k0; trk[0].p[P_SLEN] = 16;
        lists.groove[0] = 2; okk &= recording_key(&trk[0]) != k0;
        k0 = recording_key(&trk[0]); lists.gamt[0] = 95; okk &= recording_key(&trk[0]) != k0;
        k0 = recording_key(&trk[0]); song.g[G_SWING] += 1; okk &= recording_key(&trk[0]) != k0; song.g[G_SWING] -= 1;
        lists.groove[0] = 0; lists.gamt[0] = 100;
        bad |= lcheck("... the timed notes' schedule sees a swing, LEN or groove change", okk);
    }
    return bad;
}

/* --------------------------------------------- 12. (0.4.4) a review's findings --- */
static int test_review(void)
{
    int bad = 0, ok = 1;
    reset();
    for (uint32_t i = 0; i < 10u; i++) {        /* ten phrases on track 1's slots A..J: the banks hold 8 of them */
        if (i) {
            uint32_t b = ocl_victim(0);
            pattern_request(&trk[0], b);
            bank_pid[0][b] = 0;
            for (uint32_t j = 0; j < NSTEP; j++) step_clear(&trk[0].step[j]);
            lists.cur[0] = (uint8_t)i;
        }
        put_note(0, 40u + i);
        ocl_keep_all();
    }
    project_save_as(0, "TEN");
    wipe_runtime();
    pattern_init();                             /* (another project loaded: nothing of it here) */
    ocl_clear_unused();
    for (uint32_t i = 0; i < 10u; i++) ok &= ocp.len[i] != 0u;
    reboot();
    wipe_runtime();
    pattern_init();
    ocl_clear_unused();
    for (uint32_t i = 0; i < 10u; i++) ok &= ocp.len[i] != 0u && pool_note(i) == 40u + i;
    bad |= lcheck("a phrase only a saved project's chain names is never emptied (Free unused, after a reboot too)", ok);
    project_load(0);
    ok = 1;
    for (uint32_t i = 0; i < 10u; i++) ok &= ocl_ent_pat(0, i) == i && ocp.len[i];
    bad |= lcheck("... the project loads with all ten", ok);
    reset();
    three_patterns();                           /* 01 02 03 on chain 01's A B C */
    lists.tchain[1] = 0; lists.cur[1] = 2; lists.cur[0] = 1;
    ocl_insert(0, 0);
    ok = lists.cur[1] == 3u && lists.cur[0] == 2u && ocl_ent_pat(1, 3) == 2u;
    ocl_delete(0, 0);
    ok &= lists.cur[1] == 2u && lists.cur[0] == 1u && ocl_ent_pat(1, 2) == 2u;
    bad |= lcheck("a slot inserted or deleted moves every track on that chain with it", ok);
    reset();                                    /* (a second review) a step staged by hand, then a load: dropped */
    three_patterns();
    project_save_as(0, "STAGE");
    lists.tchain[0] = 7; lists.chain[7][1][0] = 2;
    song.playing = 1;
    ocl_launch(0, 1);
    ok = ocl_staged_ent(0) == 1u;
    song.playing = 0;
    project_load(0);
    ocl_poll();
    bad |= lcheck("a step staged by hand in a project let go never lands in the one loaded", ok && lists.tchain[0] == 0u &&
                  ocl_staged_ent(0) == OCL_NENT);
    return bad;
}

int main(void)
{
    int bad = 0;
    bad |= test_groove();
    bad |= test_review();
    bad |= test_launch();
    bad |= test_song_rows();
    bad |= test_ocj2();
    bad |= test_named_empty();
    bad |= test_melodee_lists();
    bad |= test_unused();
    bad |= test_keep_any();
    bad |= test_song();
    bad |= test_new_and_shared();
    bad |= test_cache();
    bad |= test_project();
    bad |= test_full();
    printf(bad ? "oc lists: FAILED\n" : "oc lists: all passed\n");
    return bad ? 1 : 0;
}
