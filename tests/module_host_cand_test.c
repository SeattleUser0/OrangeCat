/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md): a candidate module (SEM or HADES, Caravel's) on the host, in a measurement
 * build (OC_CANDIDATES=1: the engine table of 23, the proposed numbers registrable). Built once per candidate with its unit (tests/module_host_<name>_tu.c)
 * and -DCAND_ENGINE=ENGI_<NAME>: the header passes the registration check, the loader's registration takes the proposed slot (and only in this build),
 * the engine is offered while registered, its presets render through the whole voice path (tests/regress.c) with the lines of tests/golden_modules.txt
 * (module/<NAME>/..., new lines), and unregistering retires the slot. Sample for sample against Caravel's own build of the engine: tests/caravel_parity
 * (a development check: CI has no Caravel checkout).
 *
 *   build/host/module_host_<name>_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
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
          h.engine >= 21u && h.engine < NENGINES && NENGINES == 23);
    check("no hot set of its own, the state in voice_t (a 4-byte token state: the loader refuses 0)", em->nhot == 0u && h.state_size == 4u && np >= 3u && np <= 64u);
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
    eng_unregister(CAND_ENGINE);
    check("unregistered, the slot is retired again", ENGINES[CAND_ENGINE] == &ENG_RETIRED && !eng_ok(CAND_ENGINE));
    printf(bad ? "module host (candidate): FAILED\n" : "module host (candidate): all passed\n");
    return bad != 0;
}
