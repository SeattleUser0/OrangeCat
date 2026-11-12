/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO with the MODELS module (Release 3, the model picker), the UI's and the project's side, in the device's build (MELODEE_MACRO_PICK=1: VA alone
 * built in) with the module's unit as its own object (tests/module_host_plaits_tu.c): a track whose model is not installed keeps its settings, the
 * screen says MODULE MISSING (ui.c pack_notice, pack_boot_notice: the message a missing engine module gets), and a saved project loads the model id
 * back unchanged whether or not the module is there (stable ids: P_E0 is the id); a preset recall keeps the id too. On the persistence harness
 * (tests/persistence_test.c: the simulated NOR, the UI, the project store).
 *   build/host/plaits_ui_test
 */
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"
#if !MELODEE_MACRO_PICK
#error "build with -DMELODEE_MACRO_PICK=1: VA alone built in, the rest from the module"
#endif
extern const fw_api_t *oc_host_fw;
extern const void *const *oc_host_models(uint32_t *n);
extern const void *const *oc_host_banks(void);

static int bad;
static void chk(const char *what, int ok)
{
    printf("plaits ui: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define check chk
static const char *notice(uint32_t force)         /* what pack_notice puts on the screen for the selected track, "" for nothing */
{
    ui.msg[0] = 0; ui.msg_t = 0;
    pack_notice(force);
    return ui.msg;
}
int main(void)
{
    uint32_t n = 0;
    const void *const *models;
    setvbuf(stdout, NULL, _IONBF, 0);
    reset();
    settings_init();
    oc_host_fw = &FW_API;
    models = oc_host_models(&n);
    song.sel = 0;
    /* 1. the screen: a MACRO track on a model that is not installed */
    host_preset(&trk[0], ENGI_MACRO, 18);              /* MALLET: model 10, in the module */
    check("without the module: a MACRO track on MALLET (model 10) says MODULE MISSING when the engine is chosen (force) and on a key (not forced)",
          !strcmp(notice(1), "MODULE MISSING") && !strcmp(notice(0), "MODULE MISSING"));
    host_preset(&trk[0], ENGI_MACRO, 0);
    check("... a track on VA BASS (model 0, built in) does not", strcmp(notice(1), "MODULE MISSING") != 0);
    host_preset(&trk[1], ENGI_MACRO, 36);              /* TERRAIN DRONE: model 20 */
    song.ntrk = 2;
    ui.msg[0] = 0; ui.msg_t = 0; pack_boot_notice(0);
    check("at boot, a project with a track on a missing model: MODULE MISSING once (pack_boot_notice)", !strcmp(ui.msg, "MODULE MISSING"));
    check("the registration takes the module's table", eng_models_set(ENGI_MACRO, models, n, oc_host_banks()) == EM_OK);
    host_preset(&trk[0], ENGI_MACRO, 18);
    check("with the module: the same track says nothing of a module", strcmp(notice(1), "MODULE MISSING") != 0);
    ui.msg[0] = 0; ui.msg_t = 0; pack_boot_notice(0);
    check("... nor the boot notice", strcmp(ui.msg, "MODULE MISSING") != 0);
    /* 2. the project: the model id is stored and comes back, module or not */
    eng_models_clear(ENGI_MACRO);
    host_preset(&trk[0], ENGI_MACRO, 18);
    host_preset(&trk[1], ENGI_MACRO, 36);
    trk[1].p[P_E1] = 99; trk[1].p[P_E6] = 33;          /* settings beside the model: kept too */
    song.ntrk = 4;
    check("the tracks name models 10 and 20 (MISSG on the knob, VA on the voice) with their settings", trk[0].p[P_E0] == 10 && trk[1].p[P_E0] == 20 && mc_track_missing(&trk[0]) && mc_track_missing(&trk[1]));
    check("the project saves without the module", project_save(0) == 0);
    trk[0].p[P_E0] = 0; trk[1].p[P_E0] = 0; trk[1].p[P_E1] = 0; trk[1].p[P_E6] = 0;
    project_load(0);
    check("loaded again without the module: the ids 10 and 20 and the settings are back, unchanged (the model is MISSG, the track plays VA)",
          trk[0].eng_req == ENGI_MACRO && trk[0].p[P_E0] == 10 && trk[1].p[P_E0] == 20 && trk[1].p[P_E1] == 99 && trk[1].p[P_E6] == 33 && mc_track_missing(&trk[0]));
    check("the registration again", eng_models_set(ENGI_MACRO, models, n, oc_host_banks()) == EM_OK);
    trk[0].p[P_E0] = 0; trk[1].p[P_E0] = 0;
    project_load(0);
    check("loaded with the module: the same ids, now installed (the tracks play their models)", trk[0].p[P_E0] == 10 && trk[1].p[P_E0] == 20 && !mc_track_missing(&trk[0]) && !mc_track_missing(&trk[1]));
    check("the project saved with the module loads unchanged without it (the id is the id)", project_save(1) == 0 && (eng_models_clear(ENGI_MACRO), trk[1].p[P_E0] = 0, project_load(1), trk[1].p[P_E0] == 20 && mc_track_missing(&trk[1])));
    host_preset(&trk[2], ENGI_MACRO, 44);               /* VOWEL: model 24, SPEECH */
    check("a preset recall keeps its model id without the module (VOWEL: 24)", trk[2].p[P_E0] == 24 && mc_track_missing(&trk[2]));
    printf(bad ? "plaits ui: %d FAILED\n" : "plaits ui: all passed\n", bad);
    return bad != 0;
}
