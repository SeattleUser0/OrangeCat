/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat, Release 3 (branch r3-format), Part 3: writes a store in the FORMAT BEFORE, with the format before's own code (this file is built
 * WITHOUT OC_R3_FORMAT: the same sources as the shipping firmware), for tests/oc_r3_conv_test.c to convert:
 *   build/host/oc_r3_oldgen OUT_NOR OUT_EXPECT
 * OUT_NOR: the simulated NOR (0x100000 bytes). OUT_EXPECT: for each of the four slots, one byte (1: the old reader loads it) and the FBKK the old
 * reader gives (BANK_STORE_SIZE bytes). The scenarios: the owner-like case (two projects sharing phrases, a chain naming a phrase no bank
 * holds, scenes, grooves), a phrase handwritten as version 1 with np 116 and a knob-macro motion event that only a chain names (the old firmware
 * never loads a chain-only phrase, so it was valid there), a third project, and the freeze table (two frozen phrases) in OCS_FREEZE.
 * Revisions:
 *   Part 3 (November 1, 2026): first version. */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static int write_file(const char *path, const void *p, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 1;
    int ok = fwrite(p, 1, n, f) == n;
    fclose(f);
    return !ok;
}
/* a version 1 phrase: np 116, one step, a motion event on a knob-macro id (34), one on a real id, one timed note */
static uint32_t v1_phrase(uint8_t *b)
{
    uint32_t n = 0, i;
    step_snapshot_t v;
    step_t st;
    memset(b, 0, 128);
    b[0] = 1; b[1] = 116; b[2] = 0; b[3] = 0;
    ocp_put16(b + 4, 16); ocp_put16(b + 6, 1); ocp_put16(b + 8, 0); ocp_put16(b + 10, 50);
    b[12] = 0x05;                                  /* steps 0 and 2 */
    n = OCP_HEAD;
    memset(&st, 0, sizeof st); st.n = 1; st.time = ST_NOTE; st.note[0] = 60; st.vel = 100;
    for (i = 0; i < 2u; i++) { st.note[0] = (uint8_t)(60u + 4u * i); step_snapshot_put(&v, &st); memcpy(b + n, &v, OCP_STEP); n += OCP_STEP; }
    b[n++] = 2;                                    /* two motion events (4 bytes: step, param, value lo, value hi) */
    b[n++] = 0; b[n++] = 34; ocp_put16(b + n, 5); n += 2;        /* a knob macro's id: inert in the format before */
    b[n++] = 2; b[n++] = 112; ocp_put16(b + n, 9); n += 2;       /* np 116: 112 was P_E4 then (now P_E4 + 7 with LFO 2) */
    ocp_put16(b + n, 1); n += 2;                   /* one timed note */
    { recorded_note_t r; memset(&r, 0, sizeof r); r.on = 4000; r.duration = 900; r.note = 64; r.vel = 90; r.step = 2; memcpy(b + n, &r, OCP_NOTE); n += OCP_NOTE; }
    return n;
}

int main(int argc, char **argv)
{
    static uint8_t exp[4][1 + BANK_STORE_SIZE];
    uint32_t s;
    if (argc != 3) { fprintf(stderr, "usage: oc_r3_oldgen OUT_NOR OUT_EXPECT\n"); return 2; }
    reset();
    /* project A (slot 0): 4 tracks, banks 0..2, motion and timed notes, scenes and grooves in the lists */
    fill_all(1, 3);
    lists.scene[0][0][0] = 1; lists.scene[0][0][1] = 0x82; lists.scene[3][2][0] = 2; lists.scene_used = 0x09;
    lists.groove[1] = 3; lists.gamt[1] = 60; lists.launch = OCL_BAR;
    if (project_save_as(0, "GATE ONE")) { fprintf(stderr, "save A failed: %s\n", ui.msg); return 1; }
    /* project B (slot 2): the same music with one bank changed (the other banks' phrases are shared with A), a chain that names a phrase of A no bank of B holds */
    { pattern_t p; pattern_read(0, 1, &p); p.step[7].n = 1; p.step[7].time = ST_NOTE; p.step[7].note[0] = 70; p.step[7].vel = 80; pattern_write(0, 1, &p); }
    { uint32_t other = 0; for (uint32_t b = 0; b < 3u; b++) if (bank_pid[3][b]) other = bank_pid[3][b]; lists.chain[9][3][0] = (uint8_t)other; lists.chain[9][3][1] = 2; }
    if (project_save_as(2, "GATE TWO")) { fprintf(stderr, "save B failed: %s\n", ui.msg); return 1; }
    /* a phrase handwritten as version 1 (chunk 15 = phrases 61..64), named only by a chain of project C */
    {
        uint8_t ph[128], chunk[8 + 128];
        uint32_t len = v1_phrase(ph);
        memset(chunk, 0, sizeof chunk);
        ocp_put16(chunk, len);                     /* phrase 61 is the chunk's first; the other three empty */
        memcpy(chunk + OCP_TABLE, ph, len);
        if (ocs_write(OCS_POOL, 15, chunk, OCP_TABLE + len)) { fprintf(stderr, "chunk 15 failed\n"); return 1; }
        ocp_scan(); ocj_scan();
    }
    wipe_runtime();
    fill_all(2, 2);
    lists.chain[20][0][0] = 61; lists.chain[20][0][1] = 0;
    if (project_save_as(3, "GATE C")) { fprintf(stderr, "save C failed: %s\n", ui.msg); return 1; }
    /* the freeze table: phrase 1 (A's first) and phrase 61 frozen, as the card code keeps it (OCS_FREEZE: frzp[64] of 32 bytes, then frzd) */
    {
        static uint8_t fz[64 * 32 + 16 * 32 + 8];
        uint8_t *e;
        memset(fz, 0, sizeof fz);
        for (uint32_t k = 0; k < 2u; k++) {
            uint32_t id = k ? 61u : 1u, hash = ocp.hash[id - 1u];
            e = fz + (id - 1u) * 32u;
            e[0] = 1; e[1] = 0; e[2] = (uint8_t)k; e[3] = 2;               /* take 1, track, version 2 */
            e[4] = 100; e[6] = 120;                                        /* lvl, bpm */
            e[8] = 0x00; e[9] = 0x40; e[10] = 0; e[11] = 0;                /* frames 16384 */
            memcpy(e + 12, &hash, 4);
            e[16] = 1; e[17] = 1;                                          /* folder 1 now and wanted */
            memcpy(e + 18, k ? "T1_2.WAV" : "T1_1.WAV", 9);
        }
        memcpy(fz + 64 * 32, "ORANGECAT", 10);
        memcpy(fz + 64 * 32 + 32, "ORANGECAT/GATE ONE", 19);
        if (ocs_write(OCS_FREEZE, 0, fz, sizeof fz)) { fprintf(stderr, "freeze failed\n"); return 1; }
    }
    /* what the format before's own reader gives for each slot */
    memset(exp, 0, sizeof exp);
    reboot();
    for (s = 0; s < 4u; s++) {
        int n = ocj_read(s, exp[s] + 1);
        exp[s][0] = n == (int)BANK_STORE_SIZE;
        printf("oldgen: slot %u: %s\n", s, exp[s][0] ? "loads" : "empty");
    }
    if (write_file(argv[1], nor, sizeof nor) || write_file(argv[2], exp, sizeof exp)) { fprintf(stderr, "write failed\n"); return 1; }
    printf("oldgen: the store and the expected FBKKs written\n");
    return 0;
}
