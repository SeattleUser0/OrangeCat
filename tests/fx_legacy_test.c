/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 8, L1 (docs/orangecat/MODULE_FORMAT_R3.md 16): a Release 2 project's reverb values load into the reverb module exactly as they play today.
 * For each mode (SPRING, SHIMMER, RESO, CLOUD) a project is made with that TYPE and every knob away from its default, saved with the firmware's own writer (proj_pack: the Release 2 record),
 * loaded again with its reader (proj_import, project_restore_runtime: song.g is what the resident reverb plays), and the module descriptor of that mode (tests/fx_reverb_data.h, built into wire bytes)
 * imports its values from the loaded project (fx_legacy_import, fw_fxstore.h). Checked:
 *   VALUES  each of the module's seven knob values is the resident reverb's value of that knob (song.g after the load), and the modules of the other modes, whose TYPE is not selected, hold their defaults
 *   RENDER  a track playing through the reverb is rendered (in its own forked process, as tests/accent_own_test.c) with the project loaded by the resident reader, and again with a project whose reverb
 *           knobs are DEFAULT apart from TYPE, the module's values written into the legacy slots (fx_legacy_export): the two renders are equal sample for sample. (No module kernel exists yet: the
 *           resident reverb is the stand-in that the module's values drive; what is proved is that nothing is lost, clamped or moved on the way in.) A control: the render with the other modes' modules'
 *           (default) values differs from the resident's, so the renders can tell.
 *   CLAMP   a stored value outside a mode's range (G_RPRE = 30 under RESO, where it is PITCH 0 .. 14) is clamped by the module and reported: the resident reverb's render of it is compared too
 * Revisions:
 *   (Release 3, step 3 decisions L1 to L4) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "fw_pages_wire.h"
#include "fw_fxstore.h"
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define BASE 0x10200000u
static uint8_t img[4096];
static uint32_t desc;
static int img_rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    (void)ctx;
    if (addr < BASE || addr - BASE + n > sizeof img) return 1;
    memcpy(buf, img + (addr - BASE), n);
    return 0;
}
#include "fx_reverb_data.h"
static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("fx_legacy: %-130s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
#define RN 1400u
static struct { int32_t pcm[RN * CTL]; } *shm;
/* the track plays a long chord note into the reverb; song.g is whatever the caller set */
static void render(int32_t *out)
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
        set_engine_of(t, ENGI_MACRO); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; t->p[P_REV] = 127; panic_req = 0; song.master_q12 = 4096;
        trk_note_on(t, 60, 110);
        for (uint32_t b = 0; b < RN; b++) {
            mix_block(blk, CTL);
            if (b == 40) trk_note_off(t, 60);
            for (uint32_t i = 0; i < CTL; i++) shm->pcm[b * CTL + i] = blk[2 * i];
        }
        _exit(0);
    }
    waitpid(pid, &st, 0);
    memcpy(out, shm->pcm, sizeof shm->pcm);
}
static int32_t peak(const int32_t *a) { int32_t p = 0; for (uint32_t i = 0; i < RN * CTL; i++) { int32_t v = a[i] < 0 ? -a[i] : a[i]; if (v > p) p = v; } return p; }
static void fresh_song(void)
{
    ui_power_on();
    for (uint32_t i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
}
static int32_t R_res[RN * CTL], R_mod[RN * CTL], R_other[RN * CTL];
/* mode k's Release 2 project: TYPE and every knob away from its default and inside the mode's range */
static void project_of_mode(uint32_t k, const int16_t *knobs, project_t *out)
{
    project_store_t wire;
    fresh_song();
    song.g[G_RTYPE] = (int16_t)REV_TYPE[k];
    for (uint32_t i = 0; i < 7; i++) song.g[REV_G[i]] = knobs[i];
    project_capture(&proj_scratch);
    if (!proj_pack(&wire, &proj_scratch) || !proj_import(out, &wire, sizeof wire)) { printf("fx_legacy: the project does not pack\n"); exit(2); }
}
int main(void)
{
    /* a value of every knob in the range of each mode: SIZE, DAMP (FOLW 3 under RESO), PRE (AIR -7, PITCH 5), MOD, RATE, FILT, WIDE */
    static const int16_t KN[4][7] = {{77, 41, 33, 55, 88, -20, 66}, {100, 25, -7, 44, 12, 30, 90}, {64, 3, 5, 99, 50, -45, 10}, {120, 80, 11, 7, 101, 12, 127}};
    int16_t vals[4][8];                                  /* the modules' imported values by mode */
    for (uint32_t k = 0; k < 4; k++) {
        project_t loaded;
        fw_pages_t d;
        char buf[200];
        int16_t mod[8], other[8];
        uint32_t ok = 1;
        project_of_mode(k, KN[k], &loaded);
        project_restore_runtime(&loaded);                /* Release 2's own load: song.g is the project's */
        make_reverb(&d, k);
        if (!fwp_wire(&d, img, sizeof img, BASE, &desc)) { printf("fx_legacy: image full\n"); return 2; }
        chk("the descriptor passes the checker", fw_pages_check_ex(img_rd, 0, desc, BASE + 0x100u, BASE + sizeof img, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK);
        memset(mod, 0, sizeof mod);
        snprintf(buf, sizeof buf, "%s: import from the loaded Release 2 project reads seven values", REV_NM[k]);
        chk(buf, fx_legacy_import(img_rd, 0, desc, trk[0].p, song.g, mod) == 7);
        for (uint32_t i = 0; i < 7; i++) ok &= mod[i] == song.g[REV_G[i]] && mod[i] == KN[k][i];
        snprintf(buf, sizeof buf, "%s VALUES  every knob of the module is the resident reverb's value of that knob (song.g after the project load)", REV_NM[k]);
        chk(buf, ok);
        memcpy(vals[k], mod, sizeof mod);
        for (uint32_t j = 0; j < 4; j++) if (j != k) {   /* the other modes' modules: TYPE is not theirs */
            fw_pages_t e;
            uint32_t same = 1;
            make_reverb(&e, j);
            if (!fwp_wire(&e, img, sizeof img, BASE, &desc)) return 2;
            memset(other, 0, sizeof other);
            fx_legacy_import(img_rd, 0, desc, trk[0].p, song.g, other);
            for (uint32_t i = 0; i < 7; i++) same &= other[i] == rev_m[j][i].def;
            snprintf(buf, sizeof buf, "%s VALUES  the %s module, whose mode is not selected, holds its defaults", REV_NM[k], REV_NM[j]);
            chk(buf, same);
        }
        /* RENDER: the resident reader's project against the module's values driving the same reverb */
        render(R_res);
        snprintf(buf, sizeof buf, "%s RENDER  the project played through the resident reverb sounds (the test can hear a reverb)", REV_NM[k]);
        chk(buf, peak(R_res) > 500);
        make_reverb(&d, k);
        fwp_wire(&d, img, sizeof img, BASE, &desc);
        fresh_song();
        song.g[G_RTYPE] = (int16_t)REV_TYPE[k];            /* a project with the mode and the knobs at their defaults ... */
        fx_legacy_export(img_rd, 0, desc, mod, trk[0].p, song.g);   /* ... and the module's values written into the legacy slots */
        render(R_mod);
        snprintf(buf, sizeof buf, "%s RENDER  the module's values drive the reverb to the resident project's render, sample for sample", REV_NM[k]);
        chk(buf, !memcmp(R_res, R_mod, sizeof R_res));
        fresh_song();
        song.g[G_RTYPE] = (int16_t)REV_TYPE[k];            /* control: the project's TYPE with every knob at its default (what a module not imported would give) */
        render(R_other);
        snprintf(buf, sizeof buf, "%s RENDER  control: the same TYPE with the knobs at their defaults renders differently (the render can tell)", REV_NM[k]);
        chk(buf, memcmp(R_res, R_other, sizeof R_res) != 0);
    }
    /* CLAMP: RESO's PITCH is 0 .. 14, a stored 30 (it was a PRE of 30 ms under ROOM before the player chose RESO) */
    {   project_t loaded;
        fw_pages_t d;
        int16_t kn[7] = {64, 3, 30, 99, 50, -45, 10}, mod[8];
        int32_t R_clamp[RN * CTL];
        project_of_mode(2, kn, &loaded);
        project_restore_runtime(&loaded);
        render(R_res);
        make_reverb(&d, 2);
        fwp_wire(&d, img, sizeof img, BASE, &desc);
        fx_legacy_import(img_rd, 0, desc, trk[0].p, song.g, mod);
        chk("CLAMP   RESO: a stored G_RPRE of 30 is the module's PITCH 14 (its range)", song.g[G_RPRE] == 30 && mod[2] == 14);
        fresh_song();
        song.g[G_RTYPE] = 3;
        fx_legacy_export(img_rd, 0, desc, mod, trk[0].p, song.g);
        render(R_clamp);
        chk("CLAMP   .. and the resident reverb plays the 30 as it plays the 14 (it clamps the same way): renders equal", !memcmp(R_res, R_clamp, sizeof R_res));
    }
    printf("fx_legacy: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
