/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO with the MODELS module (Release 3, the model picker; docs/candidates/model_picker.md): the firmware built the device's way (MELODEE_MACRO_PICK=1:
 * VA alone built in) and the module's unit (tools/module/plaits_mod.c with every model, built as its own object: tests/module_host_plaits_tu.c)
 * registered as the engine's extension (engines.c eng_models_set, what the loader does), through the real voices and mixer (tests/regress.c's
 * harness and phrase).
 *   build/host/plaits_test            without the module: only VA is offered, the other models read MISSG, a track that names one keeps the setting
 *                                     and plays VA (the render equals the same settings on VA); with the module: every model offered, every preset
 *                                     sounds and frees its voices, MACRO's 27 presets and the 20 new ones render bit for bit as tests/golden.txt's
 *                                     preset/MACRO lines (the plain host build has every model built in: the two paths are the same sound), the
 *                                     MODL knob, the state the header needs; the module out again: VA only
 *   build/host/plaits_pick_test       the unit built without WAVTBL, SPEECH, 6OP B and wave bank C (-DPL_HOST_NO_...): those models are not offered (the
 *                                     knob steps over them, the value reads MISSG), a track that names one keeps its settings and plays VA, the others
 *                                     play, TERRAN plays with two banks, 6OP A and C play beside the missing B
 *   build/host/plaits_test tests/golden.txt   (the golden file; GOLDEN_PRINT=1 prints the hashes)
 */
#define REGRESS_MODULE_HOST 1
#define JOB_PRESET_HOOK(t, j) do { if ((j)->arg == 1) (t)->p[P_E0] = 0; } while (0)   /* arg 1: the preset's settings on VA (what a missing model plays) */
#include "regress.c"
#if !MELODEE_MACRO_PICK
#error "build with -DMELODEE_MACRO_PICK=1: VA alone built in, the rest from the module"
#endif
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const void *const *oc_host_models(uint32_t *n);
extern const void *const *oc_host_banks(void);
extern uint32_t oc_host_models_state(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("plaits: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint32_t npre;
static void fresh(void)                            /* the tracks and the arena from nothing (a held chord of the block before would still count) */
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    for (uint32_t k = 0; k < NPART; k++) resource_release(RES_ENGINE0 + k);   /* (the parts only: wiping the whole table forgot the hot sets' blocks, which the next
                                                                              * part then took and a model's start zeroed: MACRO read tables of zeros, NaN out of NOISE) */
    song.master_q12 = 4096;
}
static int hot_intact(void)                        /* every table of every taken hot set equals its source (a model's tables are never written) */
{
    uint32_t s, i, ok = 1, sets = 0;
    for (s = 0; s < ENG_HOT_SLOTS; s++) {
        if (!eng_hs.list[s]) continue;
        sets++;
        for (i = 0; i < eng_hs.n[s]; i++) ok &= !memcmp(eng_hs.at[s][i], eng_hs.list[s][i].src, eng_hs.list[s][i].bytes);
    }
    return ok && sets;
}
/* every preset as a regress job (names preset/MACRO/NN_NAME, golden.txt's), arg 1: on VA; step: every step-th preset only */
static uint32_t jobs_presets(char names[][64], uint32_t arg, uint32_t step)
{
    uint32_t i, k0 = nj;
    for (i = 0; i < npre; i += step) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(&ENG_MACRO, i)->name, sizeof s);
        snprintf(names[i], 64, "preset/MACRO/%02u_%s", i, s);
        j = add(J_PRESET, names[i]);
        j->e = ENGI_MACRO; j->pi = (uint8_t)i; j->arg = (uint8_t)arg;
    }
    return k0;
}
#define VA_STEP 5u                                 /* the "on VA" comparison for every fifth preset (the renders are the suite's time) */
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden.txt";
    static kv_t gold[MAXJ];
    static char names[64][64], names2[64][64];
    const void *const *models;
    uint32_t n = 0, i, k0, k1, k2, ng, present = 0, missing = 0, crashed = 0, same = 0, silent_ok = 1;
    char line[200], val[16];
    const char *unit;
    setvbuf(stdout, NULL, _IONBF, 0);
    oc_host_fw = &FW_API;
    eng_hot_boot();                                   /* MACRO's hot set into the arena: the module reads the copies through the services table */
    models = oc_host_models(&n);
    npre = ENG_MACRO.npresets;
    ng = load_kv(gpath, gold, MAXJ);
    for (i = 0; i < n; i++) present += models[i] != 0;
    printf("plaits: the module carries %u of %u models (state %u B for the largest), MACRO has %u presets; %u golden lines\n", present, n, oc_host_models_state(), npre, ng);
    check("the module's header: the registration check passes, kind MODELS for engine 8 (MACRO), state_size 0 in the source (the packer fills it)",
          fw_module_check(&OC_MODULE_HDR) == FW_MOD_OK && FW_MOD_KIND(&OC_MODULE_HDR) == FW_MOD_KIND_MODELS && FW_MOD_ENGINE(&OC_MODULE_HDR) == ENGI_MACRO && OC_MODULE_HDR.state_size == 0u);
    check("the largest state a part needs for the module's models fits the engine's block (PL_STATE_MAX), and the block is what MACRO's part takes",
          oc_host_models_state() <= PL_STATE_MAX && eng_state_size(ENGI_MACRO) == PL_STATE_MAX && eng_models_max(ENGI_MACRO) == PL_STATE_MAX);
    /* 1. without the module: VA alone */
    {   uint32_t off = 0, miss = 0;
        for (i = 0; i < PL_N; i++) {
            param_format(&ENG_MACRO.edit[0], (int32_t)i, val, &unit);
            off += mc_offered((int32_t)i) != 0;
            miss += !strcmp(val, "MISSG");
        }
        check("without the module VA is the one model offered; the 24 others read MISSG", off == 1u && mc_offered(0) && miss == PL_N - 1u);
        k0 = jobs_presets(names, 0, 1);
        k1 = jobs_presets(names2, 1, VA_STEP);
        run_jobs(J + k0, nj - k0, 8);
        crashed = 0;
        for (i = 0; i < npre; i++) {
            const preset_t *p = eng_preset(&ENG_MACRO, i);
            crashed += J[k0 + i].crashed != 0 || J[k0 + i].r.over;
            silent_ok &= J[k0 + i].r.peak >= LIM_PEAK_MIN;                               /* a missing model plays VA: never silent */
            if (i % VA_STEP) continue;
            {   const job_t *v = &J[k1 + i / VA_STEP];                                   /* ... and renders as the same settings on VA */
                crashed += v->crashed != 0;
                same += J[k0 + i].r.hash == v->r.hash || p->e[0] == 0;
                if (p->e[0] && J[k0 + i].r.hash != v->r.hash) printf("plaits:   %s: missing %016llx, on VA %016llx\n", names[i], (unsigned long long)J[k0 + i].r.hash, (unsigned long long)v->r.hash);
            }
        }
        check("without the module every preset sounds (its model missing, it plays VA), and every fifth renders exactly as its settings on VA; none crashes", crashed == 0 && silent_ok && same == (npre + VA_STEP - 1u) / VA_STEP);
        check("the UI's question: a track on a missing model says so (mc_track_missing), one on VA does not",
              (host_preset(&trk[0], ENGI_MACRO, 18), mc_track_missing(&trk[0])) && (host_preset(&trk[0], ENGI_MACRO, 0), !mc_track_missing(&trk[0])));
    }
    /* 2. with the module */
    check("the registration takes the module's table (engines.c eng_models_set: what the loader does)", eng_models_set(ENGI_MACRO, models, n, oc_host_banks()) == EM_OK);
    {   uint32_t off = 0;
        for (i = 0; i < PL_N; i++) {
            int o = mc_offered((int32_t)i);
            param_format(&ENG_MACRO.edit[0], (int32_t)i, val, &unit);
            off += o != 0;
            if (!o) { missing++; snprintf(line, sizeof line, "model %-6s is not in this build of the module: reads %s", ENG_MACRO.edit[0].names[i], val); check(line, !strcmp(val, "MISSG")); }
            else if (strncmp(val, ENG_MACRO.edit[0].names[i], 5)) { snprintf(line, sizeof line, "model %s reads %s", ENG_MACRO.edit[0].names[i], val); check(line, 0); }
        }
        snprintf(line, sizeof line, "with the module %u of %u models are offered (the knob's names read as the models'), %u not in this build", off, PL_N, missing);
        check(line, off + missing == PL_N && off >= 1u);
    }
    nj = 0;
    k2 = jobs_presets(names, 0, 1);
    run_jobs(J + k2, nj - k2, 8);
    crashed = 0; same = 0; silent_ok = 1;
    {   uint32_t expect = 0;
        for (i = 0; i < npre; i++) {
            const preset_t *p = eng_preset(&ENG_MACRO, i);
            int o = mc_offered(p->e[0]);
            char hs[24];
            const char *want = kv_get(gold, ng, names[i]);
            crashed += J[k2 + i].crashed != 0 || J[k2 + i].r.over;
            snprintf(hs, sizeof hs, "%016llx", (unsigned long long)J[k2 + i].r.hash);
            if (getenv("GOLDEN_PRINT")) printf("%s %s\n", names[i], hs);
            if (!o) { silent_ok &= J[k2 + i].r.peak >= LIM_PEAK_MIN; continue; }   /* a model this build of the module lacks: VA, as above */
            expect++;
            if (want && !strcmp(want, hs)) same++;
            else printf("plaits:   %s %s golden %s\n", names[i], hs, want ? want : "-");
        }
        snprintf(line, sizeof line, "with the module every preset of an installed model renders bit for bit as tests/golden.txt's line (%u of %u; the plain host build has every model built in)", same, expect);
        check(line, crashed == 0 && silent_ok && same == expect);
    }
    {   /* the MODL knob: a step lands on an offered model only (params.c enum_step with param_offered) */
        int32_t v = 0, steps = 0, hit_missing = 0;
        for (i = 0; i < 2u * PL_N; i++) {
            int32_t nv = enum_step(&ENG_MACRO.edit[0], v, v + 1 > ENG_MACRO.edit[0].max ? v : v + 1);
            if (nv == v) break;
            hit_missing += !mc_offered(nv);
            v = nv; steps++;
        }
        snprintf(line, sizeof line, "the MODL knob steps through %d models and lands on no uninstalled one (%u missing)", steps, missing);
        check(line, hit_missing == 0 && steps == (int32_t)(PL_N - missing - 1u));
    }
    {   /* a model change while a note sounds, every model installed or not, and a second note: one voice, no fault */
        int32_t out[2 * CTL];
        uint32_t b, nv = 0;
        int finite = 1;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        trk[0].p[P_VOICE] = V_LEGATO;
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 20 * PL_N + 20; b++) {
            if (b % 20 == 10) trk[0].p[P_E0] = (int16_t)((b / 20) % PL_N);
            if (b == 100) trk_note_on(&trk[0], 55, 100);
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) nv += trk[0].v[i].active;
        check("the hot tables are whole after the switches (a model's start writes its own state only)", hot_intact());
        check("model changed every 20 blocks under a held note (every model, installed or not); a second note: one voice, no fault", nv == 1 && finite);
    }
    {   /* a chord: four voices on a small model (VA, POLY), one on a large one (STRING, 6OP A, STRMCH), the voice cap of the model that plays */
        static const uint8_t chord[4] = {48, 52, 55, 59};
        static const uint8_t big[3] = {9, 17, 21};
        int32_t out[2 * CTL];
        uint32_t b, n4 = 0, n1 = 0, j;
        int finite = 1;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
        for (b = 0; b < 100; b++) {
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n4 += trk[0].v[i].active;
        for (j = 0; j < 3u; j++) {
            uint32_t nn = 0;
            fresh();
            host_preset(&trk[0], ENGI_MACRO, 0);
            trk[0].p[P_E0] = big[j];
            trk[0].p[P_VOICE] = V_POLY;
            if (!mc_offered(big[j])) { n1++; continue; }   /* (the pick build: 6OP B is out, A is in) */
            for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
            for (b = 0; b < 50; b++) mix_block(out, CTL);
            for (i = 0; i < NVOICE; i++) nn += trk[0].v[i].active && trk[0].v[i].gate;
            if (nn != 1u) printf("plaits:   model %u: %u voices sound after the chord (want 1)\n", big[j], nn);
            n1 += nn == 1u;
        }
        snprintf(line, sizeof line, "POLY: a four note chord sounds %u VA voices (the budget's share), the large models one each (%u of 3), no fault", n4, n1);
        check(line, n4 >= 2u && n4 <= 4u && n1 == 3u && finite);
    }
    {   /* four tracks at once on large models: every track's state allocated at the engine's block */
        static const uint8_t heavy[4] = {17, 21, 20, 9};
        int32_t out[2 * CTL];
        uint32_t b, k, ok = 1;
        fresh();
        for (k = 0; k < 4u; k++) {
            host_preset(&trk[k], ENGI_MACRO, 0);
            trk[k].p[P_E0] = heavy[k];
            trk[k].p[P_SUS] = 127;
            trk_note_on(&trk[k], 40 + 7 * k, 100);
        }
        for (b = 0; b < 44100u / CTL; b++) mix_block(out, CTL);
        for (k = 0; k < 4u; k++) ok &= resource[RES_ENGINE0 + k].size == PL_STATE_MAX;
        check("four tracks (6OP A, STRMCH, TERRAN, STRING) at once: every track's state is the engine's block", ok);
    }
    /* 3. the module out again */
    eng_models_clear(ENGI_MACRO);
    {   uint32_t off = 0;
        for (i = 0; i < PL_N; i++) off += mc_offered((int32_t)i) != 0;
        check("the module out: VA alone is offered again, a track on another model plays VA (mc_track_missing)", off == 1u && (host_preset(&trk[0], ENGI_MACRO, 18), mc_track_missing(&trk[0])));
    }
    printf(bad ? "plaits: %d FAILED\n" : "plaits: all passed\n", bad);
    return bad != 0;
}
