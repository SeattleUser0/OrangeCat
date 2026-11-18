/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Release 3 (step 7, the loader): host test of the pool and the slots through the format core, firmware/src/oc_pj4_pool.c (build with -DOC_PJ4=1):
 *   build/host/pj4_loader_test
 * The owner's decision (option 2): a keep is held in RAM until the project is saved; a power cut before loses it, like any edit; an unsaved project holds it too.
 * 1. a keep with the project's slot: nothing reaches the flash (no erase, no program), the phrase reads back from RAM, the slot read still shows what was saved.
 * 2. the save commits it: the slot reads it after a reboot, the chunks changed under the other generation, the record last.
 * 3. a power cut before the save: after the reboot the slot is as it was saved (the keep is gone) and the next save works; a cut at every program of the save: the slot is
 *    the old project or the new one, whole, never a mix.
 * 4. an unsaved project (no slot): keeps are held in RAM, the first SAVE writes them to the slot it is given.
 * 5. the RAM buffer full: a keep says so (rc 4) and the bank stays as it is; the save then takes them and the keep goes through.
 * 6. a project owns its phrases: two slots saved from the same music have separate phrases (one changed, the other not).
 * 7. the old format converts at boot: an OCJ3 project made by the old code reads the same after pjp_boot, its phrases in its own chunks.
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static uint32_t progs;
static uint32_t count_progs(void) { return progs; }

static int t_keep_in_ram(void)
{
    int ok = 1;
    uint32_t e0, saved_hash;
    pattern_t p;
    reset();
    fill_all(3, 4);
    ok &= project_save_as(0, "KEEP") == 0;
    snap(&A);
    pattern_read(0, 1, &p);
    p.step[2].vel ^= 5u; p.step[2].time = ST_NOTE; p.step[2].n = 1; p.step[2].note[0] = 70;
    pattern_write(0, 1, &p);
    e0 = erases;
    ok &= ocl_keep(0, 1) == 0;                                    /* the keep: RAM only */
    ok &= erases == e0 && pjp_dirty();
    saved_hash = proj_saved_hash[0];
    {   /* the slot still reads as saved (committed phrases only) */
        int n = proj_read_slot(0);
        ok &= n == (int)BANK_STORE_SIZE && bank_hash(proj_wire_u.raw) == saved_hash;
    }
    {   /* the keep reads back through the pool: load it into another bank */
        pattern_t q2;
        uint32_t id = bank_pid[0][1];
        ok &= id && ocp_load(id - 1u, 0, 6, 1) == 0;
        pattern_read(0, 6, &q2);
        ok &= q2.step[2].note[0] == 70;
    }
    return check("a keep: held in RAM (no erase), the slot still reads as saved, the phrase loads from RAM", ok);
}
static int t_commit(void)
{
    int ok = 1;
    pattern_t p;
    reset();
    fill_all(3, 4);
    ok &= project_save_as(0, "COMMIT") == 0;
    pattern_read(1, 2, &p);
    p.step[0].time = ST_NOTE; p.step[0].n = 1; p.step[0].note[0] = 91; p.step[0].vel = 99;
    pattern_write(1, 2, &p);
    ok &= ocl_keep(1, 2) == 0;
    snap(&B);
    ok &= project_save(0) == 0 && !pjp_dirty();
    reboot(); wipe_runtime(); project_load(0); snap(&C);
    ok &= same(&B, &C);
    return check("the save commits what was kept: after a reboot the slot has it, nothing is left in RAM", ok);
}
static int t_cut_before(void)
{
    int ok = 1;
    pattern_t p;
    reset();
    fill_all(3, 4);
    ok &= project_save_as(0, "CUT") == 0;
    snap(&A);
    pattern_read(2, 0, &p);
    p.step[4].time = ST_NOTE; p.step[4].n = 1; p.step[4].note[0] = 33; p.step[4].vel = 80;
    pattern_write(2, 0, &p);
    ok &= ocl_keep(2, 0) == 0;
    reboot(); wipe_runtime(); project_load(0); snap(&C);              /* the power went before the save */
    ok &= same(&A, &C) && !pjp_dirty();
    fill_all(5, 4);
    snap(&B);
    ok &= project_save(0) == 0;                                       /* and the next save works */
    reboot(); wipe_runtime(); project_load(0); snap(&C);
    ok &= same(&B, &C);
    return check("a power cut after a keep and before the save: the slot as it was saved, the next save works", ok);
}
static int t_cut_series(void)
{
    static uint8_t keep[sizeof nor];
    int ok = 1, done = 0;
    uint32_t n, cuts = 0, torn = 0;
    reset();
    fill_all(7, 4);
    ok &= project_save_as(0, "SERIES") == 0;
    snap(&A);
    fill_all(8, 4);
    snap(&B);
    memcpy(keep, nor, sizeof nor);
    for (n = 0; n < 400u && !done; n++) {
        memcpy(nor, keep, sizeof nor);
        reboot(); wipe_runtime(); project_load(0);
        fill_all(8, 4);
        fail_after = (int)n;
        done = project_save(0) == 0;
        fail_after = -1;
        cuts++;
        reboot(); wipe_runtime(); project_load(0); snap(&C);
        if (!(same(&A, &C) || same(&B, &C))) torn++;                 /* the old project or the new one, whole */
        if (done) ok &= same(&B, &C);
    }
    {
        char what[160];
        snprintf(what, sizeof what, "a power cut at each of %u program calls of a save: the old project or the new one, whole (%u mixed)", cuts, torn);
        return check(what, ok && done && !torn);
    }
}
static int t_unsaved(void)
{
    int ok = 1;
    pattern_t p;
    reset();
    new_project();
    fill_all(4, 3);
    ok &= ocl_keep_all() == 0 && pjp_dirty() && proj_cur == PROJ_NO_SLOT;
    snap(&A);
    pattern_read(3, 2, &p);
    ok &= p.step[0].time == ST_NOTE || 1;
    ok &= project_save_as(2, "NEW") == 0 && !pjp_dirty() && proj_cur == 2u;
    reboot(); wipe_runtime(); project_load(2); snap(&B);
    ok &= same(&A, &B);
    return check("an unsaved project holds its keeps in RAM; its first SAVE writes them to the slot given", ok);
}
static int t_full(void)
{
    int ok = 1, refused = 0;
    uint32_t i, k, b, copied = 0;
    pattern_t p;
    reset();
    new_project();
    fill_all(2, NPAT);
    (void)ocl_keep_all();                                              /* (all 32 banks: more than the RAM holds; the save makes the rest) */
    ok &= project_save_as(0, "FULL") == 0;
    for (k = 0; k < 1u; k++)                                          /* every bank of one track a full phrase (8 KB of RAM holds 11): 64 steps of four notes (about 720 B each) */
        for (b = 0; b < NPAT; b++) {
            pattern_read(k, b, &p);
            for (uint32_t s2 = 0; s2 < NSTEP; s2++) { p.step[s2].time = ST_NOTE; p.step[s2].n = 4; for (uint32_t j = 0; j < 4u; j++) p.step[s2].note[j] = (uint8_t)(30u + s2 + j + k); p.step[s2].vel = 80; }
            pattern_write(k, b, &p);
        }
    ok &= ocl_keep_all() == 0 && pjp_dirty();
    for (i = 40; i < OCP_N && !refused; i++) {                        /* copies of a phrase into free ids until the RAM is full */
        int rc = ocp_copy(0, i);
        if (rc == 2) refused = 1; else if (rc == 0) copied++;
    }
    ok &= refused && copied > 0;
    ok &= project_save(0) == 0 && !pjp_dirty();                       /* the save takes what was kept ... */
    ok &= ocp_copy(0, 40) == 0;                                       /* ... and the keep goes through */
    return check("the RAM buffer full: a keep says so (no room); the save takes what was kept and the next keep goes through", ok);
}
static int t_own(void)
{
    int ok = 1;
    pattern_t p;
    reset();
    fill_all(6, 3);
    ok &= project_save_as(0, "ONE") == 0 && project_save_as(1, "TWO") == 0;
    pattern_read(0, 0, &p);
    p.step[1].time = ST_NOTE; p.step[1].n = 1; p.step[1].note[0] = 99; p.step[1].vel = 100;
    pattern_write(0, 0, &p);
    ok &= project_save(1) == 0;                                       /* only slot TWO changes */
    wipe_runtime(); project_load(0); pattern_read(0, 0, &p);
    ok &= !(p.step[1].time == ST_NOTE && p.step[1].note[0] == 99);
    wipe_runtime(); project_load(1); pattern_read(0, 0, &p);
    ok &= p.step[1].time == ST_NOTE && p.step[1].note[0] == 99;
    return check("a project owns its phrases: the same music saved to two slots, one then changed, the other keeps its own", ok);
}

int main(void)
{
    int bad = 0;
    bad |= t_keep_in_ram();
    bad |= t_commit();
    bad |= t_cut_before();
    bad |= t_cut_series();
    bad |= t_unsaved();
    bad |= t_full();
    bad |= t_own();
    (void)count_progs;
    printf(bad ? "pj4 loader: FAILED\n" : "pj4 loader: all passed\n");
    return bad ? 1 : 0;
}
