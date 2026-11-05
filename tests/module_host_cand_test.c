/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md): a candidate module (SEM or HADES, Caravel's; 0.14.1: op1r's synths, through
 * tools/module/op1r/op1_glue.h) on the host, in a measurement build (OC_CANDIDATES=1: the engine table of 32, the proposed numbers registrable). Built once per candidate with its unit (tests/module_host_<name>_tu.c)
 * and -DCAND_ENGINE=ENGI_<NAME>: the header passes the registration check, the loader's registration takes the proposed slot (and only in this build),
 * the engine is offered while registered, its presets render through the whole voice path (tests/regress.c) with the lines of tests/golden_modules.txt
 * (module/<NAME>/..., new lines), and unregistering retires the slot. Sample for sample against Caravel's own build of the engine: tests/caravel_parity
 * (a development check: CI has no Caravel checkout).
 *
 *   build/host/module_host_<name>_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
#define JOB_PRESET_HOOK(t, j) do { if ((j)->arg) (t)->p[P_E6] = (int16_t)(j)->arg; } while (0)   /* (0.14.0) arg: EDIT 2's third control, SEM's STAT */
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
#if !OC_CANDIDATES
#error "build with -DOC_CANDIDATES=1"
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("module host (candidate %u): %-100s %s\n", (unsigned)CAND_ENGINE, what, ok ? "ok" : "FAIL");
    bad += !ok;
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden_modules.txt";
    static kv_t gold[MAXJ];
    static char names[64][64];
    const engine_t *em;
    fw_mod_hdr_t h;
    uint32_t i, n, np, offered = 0, same = 0, crashed = 0, ng;
    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    np = em->npresets;
    ng = load_kv(gpath, gold, MAXJ);
    check("the header passes the registration check and names the proposed number", fw_module_check(&h) == FW_MOD_OK && h.engine == CAND_ENGINE &&
          h.engine >= 21u && h.engine < NENGINES && NENGINES == 32);
    if (CAND_ENGINE < ENGI_FMBELL)
        check("no hot set of its own, the state in voice_t (a 4-byte token state: the loader refuses 0)", em->nhot == 0u && h.state_size == 4u && np >= 3u && np <= 64u);
    else                                       /* (0.14.1) op1r's: the glue's part state (the voices' 128-sample blocks, the plugin's own), six voices */
        check("no hot set of its own, the glue's part state (op1g_part_t) under 16 KB, OP1_VOICES voices", em->nhot == 0u && h.state_size > 2048u &&
              h.state_size <= 16384u && em->poly == 6u && em->keep == 0u && np >= 3u && np <= 64u);
    check("the slot is retired and not offered without the module", ENGINES[CAND_ENGINE] == &ENG_RETIRED && !eng_ok(CAND_ENGINE) &&
          eng_sound_idx(CAND_ENGINE) == CAND_ENGINE && !eng_ok(20u));
    check("the registration takes it (a candidate's slot in this build)", eng_register(&h, em) == EM_OK && ENGINES[CAND_ENGINE] == em);
    for (n = 0; n < NENG_SHOWN; n++) offered += eng_vis(n) == CAND_ENGINE;
    check("registered, it is offered (eng_ok, once in ENGINE_ORDER)", eng_ok(CAND_ENGINE) && offered == 1u);
    nj = 0;
    for (i = 0; i < np; i++) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(em, i)->name, sizeof s);
        snprintf(names[i], 64, "module/%s/%02u_%s", em->name, i, s);
        j = add(J_PRESET, names[i]);
        j->e = (uint8_t)CAND_ENGINE;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    for (i = 0; i < np; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        crashed += J[i].crashed != 0 || J[i].r.over || J[i].r.peak < LIM_PEAK_MIN;   /* (and regress's health limits: sound, no sample past 16 bits) */
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)J[i].r.hash);
        same += want && !strcmp(want, hs);
        if (getenv("GOLDEN_PRINT")) printf("%s %s\n", names[i], hs);
        else if (!want || strcmp(want, hs)) printf("module host:   %s %s golden %s\n", names[i], hs, want ? want : "-");
    }
    check("every preset renders through the voices with the line of tests/golden_modules.txt", crashed == 0 && same == np);
    if (CAND_ENGINE == ENGI_SEM) {             /* (0.14.0) TYPE on SEM's STAT control: 64 the notch blend, 127 exact HP (Caravel's TYPE HP), 128 TYPE BP */
        uint32_t k0 = nj;
        static const uint8_t A[3] = {64, 127, 128};
        for (uint32_t q = 0; q < 3u; q++) {
            uint32_t a = A[q];
            char nm[64]; snprintf(nm, sizeof nm, "sem-type-%u", a);
            job_t *j = add(J_PRESET, nm); j->e = (uint8_t)CAND_ENGINE; j->pi = 0; j->arg = (uint8_t)a;
        }
        run_jobs(J + k0, 3, 3);
        int ok = 1;
        for (uint32_t k = 0; k < 3u; k++) ok &= !J[k0 + k].crashed && !J[k0 + k].r.over && J[k0 + k].r.peak >= LIM_PEAK_MIN;
        ok &= J[k0].r.hash != J[k0 + 1].r.hash && J[k0 + 1].r.hash != J[k0 + 2].r.hash && J[k0].r.hash != J[k0 + 2].r.hash;
        if (!ok) for (uint32_t k = 0; k < 3u; k++) printf("  sem STAT %u: hash %016llx peak %d over %u crashed %d\n", A[k], (unsigned long long)J[k0 + k].r.hash, (int)J[k0 + k].r.peak, (unsigned)J[k0 + k].r.over, (int)J[k0 + k].crashed);
        check("SEM's STAT control: 64 (the notch blend), 127 (exact HP: TYPE HP) and 128 (TYPE BP) are three different sounds, each healthy", ok);
    }
    if (CAND_ENGINE == ENGI_DDDD) {            /* (0.14.1) the part's cost (fw_api.h FW_MOD_COST_PART): 4 units a track, its voices none */
        uint32_t snd[2][5], kill[2][5], busy[2][5];
        for (uint32_t mode = 0; mode < 2u; mode++)
            for (uint32_t nt = 1; nt <= 4u; nt++) {
                int32_t out[2 * CTL];
                uint32_t k0;
                h.cost = mode ? FW_MOD_COST_PART | 4u << 24 : 0u;
                memset(trk, 0, sizeof trk);
                memset(&song, 0, sizeof song);
                host_tracks_init();
                for (uint32_t p = 0; p < NPART; p++) {
                    host_preset(&trk[p], p < nt ? CAND_ENGINE : 0u, p < nt ? 2u : 0u);          /* SCREAM */
                    trk[p].p[P_VOICE] = V_POLY;
                }
                k0 = voice_kills;
                for (uint32_t p = 0; p < nt; p++)
                    for (uint32_t k = 0; k < 4u; k++)
                        input_on(&trk[p], 48u + 4u * k, 100);
                for (uint32_t b = 0; b < 50u; b++) mix_block(out, CTL);
                snd[mode][nt] = 0;
                for (uint32_t p = 0; p < NPART; p++)
                    for (uint32_t i = 0; i < NVOICE; i++) snd[mode][nt] += trk[p].v[i].active && trk[p].v[i].gate;
                kill[mode][nt] = voice_kills - k0;
                busy[mode][nt] = voices_busy();
                printf("module host:   dddd SCREAM x%u, four keys a track, %s: %u voices sound, %u given up, %u units busy of %u\n", nt,
                       mode ? "the part's cost" : "voices counted", snd[mode][nt], kill[mode][nt], busy[mode][nt], voice_budget());
            }
        check("voices counted (2 units each): from three tracks dddd gives up voices it does not pay for", kill[0][3] > 0u && kill[0][4] > 0u);
        check("the part's cost (4 units a track): every key of 1 to 4 tracks sounds, none given up, 4 units a sounding track",
              snd[1][1] == 4u && snd[1][2] == 8u && snd[1][3] == 12u && snd[1][4] == 16u && kill[1][4] == 0u &&
              busy[1][1] == 4u && busy[1][2] == 8u && busy[1][3] == 12u && busy[1][4] == 16u);
        h.cost = 0;
    }
    eng_unregister(CAND_ENGINE);
    check("unregistered, the slot is retired again", ENGINES[CAND_ENGINE] == &ENG_RETIRED && !eng_ok(CAND_ENGINE));
    printf(bad ? "module host (candidate): FAILED\n" : "module host (candidate): all passed\n");
    return bad != 0;
}
