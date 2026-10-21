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
                  lists.tchain[1] == 1u && lists.song[0][1] == 2u && lists.mode == OCL_CHAIN && note_of(0) == 60u);
    return bad;
}

int main(void)
{
    int bad = 0;
    bad |= test_song_rows();
    bad |= test_ocj2();
    bad |= test_named_empty();
    bad |= test_melodee_lists();
    bad |= test_unused();
    bad |= test_song();
    bad |= test_new_and_shared();
    bad |= test_cache();
    bad |= test_project();
    bad |= test_full();
    printf(bad ? "oc lists: FAILED\n" : "oc lists: all passed\n");
    return bad ? 1 : 0;
}
