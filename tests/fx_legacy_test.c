/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3, step 8, L1 (docs/orangecat/MODULE_FORMAT_R3.md 16): a Release 2 project's reverb values load into the reverb module exactly as they play today.
 * For each mode (SPRING, SHIMMER, RESO, CLOUD) a project with that TYPE and every knob away from its default, as COMMITTED in tests/fixtures/r2_projects/ (written once by tests/gen_r2_fixtures.c with the
 * Release 2 writer, proj_pack, so that the project format revision, which changes the writer, cannot change what is loaded here), is loaded with the reader (proj_import, project_restore_runtime: song.g is what the resident reverb plays), and the module descriptor of that mode (tests/fx_reverb_data.h, built into wire bytes)
 * imports its values from the loaded project (fx_legacy_import, fw_fxstore.h). Checked:
 *   VALUES  each of the module's seven knob values is the resident reverb's value of that knob (song.g after the load), and the modules of the other modes, whose TYPE is not selected, hold their defaults
 *   RENDER  a track playing through the reverb is rendered (in its own forked process, as tests/accent_own_test.c) with the project loaded by the resident reader, and again with a project whose reverb
 *           knobs are DEFAULT apart from TYPE, the module's values written into the legacy slots (fx_legacy_export): the two renders are equal sample for sample. (No module kernel exists yet: the
 *           resident reverb is the stand-in that the module's values drive; what is proved is that nothing is lost, clamped or moved on the way in.) A control: the render with the other modes' modules'
 *           (default) values differs from the resident's, so the renders can tell.
 *   CLAMP   a stored value outside a mode's range (G_RPRE = 30 under RESO, where it is PITCH 0 .. 14) is clamped by the module and reported: the resident reverb's render of it is compared too
 *   DUB     (RENDER too) the fixture with DUB values on all three DUB pages loads into DUB's module (fx_dub_data.h) with every knob, SYNC, PING, DRIVE and WOW as written
 * Revisions:
 *   (Release 3, round 9) the projects are the committed fixtures, not made by this tree's writer; the DUB fixture.
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
#include "fx_dub_data.h"
#include "fx_r2_values.h"
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
/* DUB through the resident kernel (jfx_dub.c, tests/dub_insert_test.c's way): track 1 plays a note into the insert whose ids are whatever the caller set */
static void render_dub(int32_t *out)
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
        song.master_q12 = 4096; song.playing = 0;
        set_engine_of(t, ENGI_MACRO); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; panic_req = 0;
        for (uint32_t b = 0; b < RN; b++) {
            if (b % 120u == 4u) trk_note_on(t, 60, 100);
            if (b % 120u == 64u) trk_note_off(t, 60);
            mix_block(blk, CTL);
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
/* a committed Release 2 project: the file's bytes are the project_store_t record; loaded with the reader */
static int fixture_project(const char *name, project_t *out)
{
    static project_store_t wire;
    char path[300];
    const char *dir = getenv("FX_R2_DIR") ? getenv("FX_R2_DIR") : "tests/fixtures/r2_projects";
    FILE *f;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "rb");
    if (!f) { printf("fx_legacy: cannot open fixture %s\n", path); exit(2); }
    if (fread(&wire, 1, sizeof wire, f) != sizeof wire || fgetc(f) != EOF) { printf("fx_legacy: fixture %s is not %u bytes\n", path, (unsigned)sizeof wire); exit(2); }
    fclose(f);
    fresh_song();
    if (!proj_import(out, &wire, sizeof wire)) { printf("fx_legacy: the reader refuses fixture %s\n", path); exit(2); }
    return 1;
}
int main(void)
{
    /* a value of every knob in the range of each mode: SIZE, DAMP (FOLW 3 under RESO), PRE (AIR -7, PITCH 5), MOD, RATE, FILT, WIDE */
    const int16_t (*KN)[7] = FXR2_KN;
    int16_t vals[4][8];                                  /* the modules' imported values by mode */
    for (uint32_t k = 0; k < 4; k++) {
        project_t loaded;
        fw_pages_t d;
        char buf[200];
        int16_t mod[8], other[8];
        uint32_t ok = 1;
        fixture_project(FXR2_REV_FILE[k], &loaded);
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
        int16_t mod[8];
        int32_t R_clamp[RN * CTL];
        fixture_project(FXR2_CLAMP_FILE, &loaded);
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
    /* DUB: the fixture's track 1 holds DUB with a value on each of its three pages (DUB SETUP: SYNC TIM L TIM R PING; DUB: FDBK SEND FILTER ECHO; DUB 2: SPRNG DRIVE WOW SPEED) */
    {   project_t loaded;
        fw_pages_t d;
        int16_t v[14], sv = 0;
        uint32_t ok = 1;
        fixture_project(FXR2_DUB_FILE, &loaded);
        project_restore_runtime(&loaded);
        make_dub(&d);
        fwp_wire(&d, img, sizeof img, BASE, &desc);
        chk("DUB     the descriptor passes the checker", fw_pages_check_ex(img_rd, 0, desc, BASE + 0x100u, BASE + sizeof img, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK);
        memset(v, 0x55, sizeof v);
        chk("DUB     import from the loaded Release 2 project reads fourteen entries", fx_legacy_import(img_rd, 0, desc, trk[0].p, song.g, v) == 14);
        memcpy(dubp, trk[0].p, sizeof dubp);               /* the instance's stored ids, as the hooks read them */
        for (uint32_t i = 0; i < 8; i++) ok &= v[i] == FXR2_DUB[i];
        chk("DUB     DUB SETUP, DUB and DUB 2: FDBK 91 SEND 100 FILTER 33 ECHO 77, TIM L 40, TIM R 90, SPRNG 55, SPEED 100 are the module's values", ok);
        chk("DUB     the packed MODE and TAPE carry the new-layout mark and are in range", dub_new_layout(trk[0].p) && v[8] == trk[0].p[P_DUB_MODE] && v[9] == trk[0].p[P_DUB_TAPE]);
        ok = dub_shown(0, 0, 0x8001, &sv) && sv == FXR2_DUB_SYNC && dub_shown(0, 0, 0x8002, &sv) && sv == FXR2_DUB_PING &&
             dub_shown(0, 0, 0x8003, &sv) && sv == FXR2_DUB_DRIVE && dub_shown(0, 0, 0x8004, &sv) && sv == FXR2_DUB_WOW;
        chk("DUB     the views read SYNC FREE, PING on, DRIVE 11, WOW 7 from the packed MODE and TAPE", ok);
        chk("DUB     with SYNC FREE the times are the free ones (F_DUBTF)", dub_shown(0, 0, 5, &sv) == &DUBD_TF && dub_shown(0, 0, 6, &sv) == &DUBD_RF);
        chk("DUB     every knob of the three pages is away from its default (the fixture can tell a lost value)", v[0] != 55 && v[1] != 127 && v[2] != 60 && v[3] != 0 && v[4] != 53 && v[5] != 67 && v[6] != 0 && v[7] != 64);
        /* RENDER: the loaded project's DUB through the resident kernel against the same values set directly (the ids and the packed fields), sample for sample */
        {   static int32_t R_dres[RN * CTL], R_ddir[RN * CTL], R_ddef[RN * CTL];
            int16_t *p;
            render_dub(R_dres);
            chk("DUB     RENDER  the loaded project through the resident DUB kernel sounds (a track with an echo)", peak(R_dres) > 500);
            fresh_song();
            p = trk[0].p;
            p[P_DUB_FDBK] = FXR2_DUB[0]; p[P_DUB_SEND] = FXR2_DUB[1]; p[P_DUB_FILT] = FXR2_DUB[2]; p[P_DUB_ECHO] = FXR2_DUB[3];
            p[P_DUB_TIML] = FXR2_DUB[4]; p[P_DUB_TIMR] = FXR2_DUB[5]; p[P_DUB_SPRG] = FXR2_DUB[6]; p[P_DUB_SPD] = FXR2_DUB[7];
            dub_put_field(p, DUBF_SYNC, FXR2_DUB_SYNC); dub_put_field(p, DUBF_PING, FXR2_DUB_PING);
            dub_put_field(p, DUBF_DRIVE, FXR2_DUB_DRIVE); dub_put_field(p, DUBF_WOW, FXR2_DUB_WOW);
            render_dub(R_ddir);
            chk("DUB     RENDER  the same values set directly render to the loaded project's, sample for sample", !memcmp(R_dres, R_ddir, sizeof R_dres));
            fresh_song();
            render_dub(R_ddef);
            chk("DUB     RENDER  control: DUB at its defaults (ECHO 0) renders differently (the render can tell)", memcmp(R_dres, R_ddef, sizeof R_dres) != 0);
        }
    }
    /* (Session A's review of fw_fxstore.h, round 12) EXPORT RANGE, EXPORT CONDITION and ENGINE IDS: each fails on the code before the review */
    {   fw_pages_t d;
        int16_t big[8] = {500, -9, -5, 300, 900, -200, 400, 0}, back[8];
        uint32_t wrote;
        make_reverb(&d, 0);                              /* SPRING: SIZE, DAMP 0..127, PRE 0..100 ms, MOD 0..127, RATE 0..127, FILT -64..63, WIDE 0..127 */
        fwp_wire(&d, img, sizeof img, BASE, &desc);
        fresh_song();
        song.g[G_RTYPE] = (int16_t)REV_TYPE[0];
        wrote = fx_legacy_export(img_rd, 0, desc, big, trk[0].p, song.g);
        chk("EXPORT RANGE  seven values outside their ranges are written clamped (SIZE 127, DAMP 0, PRE 0, MOD 127, RATE 127, FILT -64, WIDE 127)",
            wrote == 7 && song.g[G_RSIZE] == 127 && song.g[G_RDAMP] == 0 && song.g[G_RPRE] == 0 && song.g[G_RMOD] == 127 && song.g[G_RRATE] == 127 && song.g[G_RFILT] == -64 && song.g[G_RWIDE] == 127);
        fx_legacy_import(img_rd, 0, desc, trk[0].p, song.g, back);
        chk("EXPORT RANGE  .. so the import reads back values inside the module's ranges and a second export changes nothing", back[0] == 127 && back[5] == -64 && fx_legacy_export(img_rd, 0, desc, back, trk[0].p, song.g) == 7 && song.g[G_RSIZE] == 127);
        fresh_song();
        song.g[G_RTYPE] = (int16_t)REV_TYPE[1]; song.g[G_RSIZE] = 77; song.g[G_RDAMP] = 33; song.g[G_RPRE] = -7;      /* SHIMMER is selected, with its own SIZE, DAMP, PRE */
        wrote = fx_legacy_export(img_rd, 0, desc, big, trk[0].p, song.g);   /* the SPRING module's export */
        chk("EXPORT CONDITION  the module of a mode that is not selected writes nothing: SHIMMER's SIZE 77, DAMP 33, PRE -7 stand", wrote == 0 && song.g[G_RSIZE] == 77 && song.g[G_RDAMP] == 33 && song.g[G_RPRE] == -7);
        song.g[G_RTYPE] = (int16_t)REV_TYPE[0];
        wrote = fx_legacy_export(img_rd, 0, desc, big, trk[0].p, song.g);
        chk("EXPORT CONDITION  .. and writes all seven once its mode is the selected one (the condition is not a blanket refusal)", wrote == 7 && song.g[G_RSIZE] == 127);
    }
    {   fw_pages_t d;
        static const exp_t E[4] = {{"A", F_PCT, 0, 127, 0, 0, 0}, {"B", F_PCT, 0, 127, 0, 0, 0}, {"C", F_PCT, 0, 127, 0, 0, 0}, {"D", F_PCT, 0, 127, 0, 0, 0}};
        static const page_t ER[1] = {{"ENGINE", FAM_EDIT, SC_MOD, GR_NONE, {0, 1, 2, 3}}};
        uint32_t flags[4] = {FW_PID_VOLATILE, FW_PID_RELATIVE, FW_PID_VIEW, 0};
        const char *nm[4] = {"a volatile", "a relative", "a view", "a plain"};
        for (uint32_t k = 0; k < 4; k++) {
            char buf[200];
            int rc;
            x_fill(&d, E, 4, ER, 1);
            for (uint32_t i = 0; i < 4; i++) { xpid[i].id = (uint16_t)(1 + i); xpid[i].legacy = FW_PID_NONE; }
            xpid[2].id = (uint16_t)(3 | flags[k]);
            fwp_wire(&d, img, sizeof img, BASE, &desc);
            img[desc - BASE + 24] = 0x00; img[desc - BASE + 25] = 0x02; img[desc - BASE + 26] = 0x20; img[desc - BASE + 27] = 0x10;   /* shown and put hooks inside the image: the relative and view rules for hooks are met */
            img[desc - BASE + 28] = 0x00; img[desc - BASE + 29] = 0x02; img[desc - BASE + 30] = 0x20; img[desc - BASE + 31] = 0x10;
            rc = fw_pages_check_ex(img_rd, 0, desc, BASE + 0x100u, BASE + sizeof img, SC_MOD, P_COUNT, G_COUNT, 0);
            snprintf(buf, sizeof buf, "ENGINE IDS  an engine whose pid table carries %s id is %s", nm[k], k >= 2 ? "accepted (a view is an engine's to have, with both hooks)" : "refused (FWP_PID)");
            chk(buf, k >= 2 ? rc == FWP_OK : rc == FWP_PID);
        }
    }
    printf("fx_legacy: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
