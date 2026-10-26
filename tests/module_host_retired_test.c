/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Retired engines"): one of Orange Cat's retired engines as a module in
 * its own slot (the CZ-1, tools/module/cz_mod.c, engine 15; SID, tools/module/sid_mod.c, engine 3), on the host, against the built-in engine of the
 * reference build and tests/golden_modules.txt, as tests/module_host_wheel_test.c is for WHEEL. -DRET_SID=1 builds SID's, else the CZ-1's.
 *
 * Built twice for each by tests/run_tests.sh (module_host_retired), with the module's own unit (tests/module_host_<name>_tu.c) linked in, both
 * measurement builds (OC_CANDIDATES=1):
 *   -DMELODEE_CZ=1 / -DMELODEE_SID=1  the reference build: the engine is built in (the CZ-1's tone the firmware's cz_patch[]). Its presets render
 *                   through the whole voice path (tests/regress.c), then the module's engine_t is put in the slot by hand (with its header, for its
 *                   state's size, and its hot set) and renders them again: every hash must be the built-in engine's, byte for byte;
 *   =0              the firmware's engine set: the slot is retired; the loader's registration puts the module there (a candidate's slot in this
 *                   build), it is offered, its presets render the lines of tests/golden_modules.txt (module/<NAME>/..: the reference build's built-in
 *                   renders), and unregistering retires the slot again.
 * Both: the header passes the registration check; the CZ-1 module's FM6_SIN constants (its hot set) are the firmware's RAM table, value for value.
 *
 *   build/host/module_host_<name>_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
#if RET_SID
#define RET_ENGINE 3u
#define RET_NAME "SID"
#define RET_SWITCH MELODEE_SID
#define NPRESET 32u
#else
extern const float *oc_host_cz_fm6_sin(void);
#define RET_ENGINE ENGI_CZ
#define RET_NAME "CZ-1"
#define RET_SWITCH MELODEE_CZ
#define NPRESET 65u
#endif
#if !OC_CANDIDATES
#error "build with -DOC_CANDIDATES=1"
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("module host (%s%s): %-100s %s\n", RET_NAME, RET_SWITCH ? ", reference build" : "", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static char names[NPRESET][64];
static void render(const engine_t *named, uint64_t *hash, uint32_t *crashed)
{
    uint32_t i;
    nj = 0;
    for (i = 0; i < NPRESET; i++) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(named, i)->name, sizeof s);
        snprintf(names[i], 64, "module/%s/%02u_%s", RET_NAME, i, s);
        j = add(J_PRESET, names[i]);
        j->e = RET_ENGINE;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    *crashed = 0;
    for (i = 0; i < NPRESET; i++) {
        hash[i] = J[i].r.hash;
        *crashed += J[i].crashed != 0 || J[i].r.over;
    }
}
static uint32_t golden(const kv_t *gold, uint32_t ng, const uint64_t *h)
{
    uint32_t i, same = 0;
    for (i = 0; i < NPRESET; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)h[i]);
        same += want && !strcmp(want, hs);
        if (getenv("GOLDEN_PRINT")) printf("%s %s\n", names[i], hs);
        else if (!want || strcmp(want, hs)) printf("module host (%s):   %s %s golden %s\n", RET_NAME, names[i], hs, want ? want : "-");
    }
    return same;
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden_modules.txt";
    static kv_t gold[MAXJ];
    static uint64_t module[NPRESET], other[NPRESET];
    uint32_t ng, i, crashed, same = 0;
    const engine_t *em;
    fw_mod_hdr_t h;

    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    ng = load_kv(gpath, gold, MAXJ);
    check("the module's header is accepted by the registration check and names its engine's own number", fw_module_check(&h) == FW_MOD_OK &&
          h.engine == RET_ENGINE && h.pack_hash == OC_PACK_HASH);
#if RET_SID
    check("32 presets, the chip's own envelopes, the cap and block hooks, its hot set (the two DACs)", em->npresets == NPRESET && !strcmp(em->name, "SID") &&
          em->ownenv && em->block && em->done && em->cap && em->nhot == 2u && em->poly == 4u);
#else
    check("65 presets (INIT and Casio's 64), its own envelopes, the block hook, one hot set table", em->npresets == NPRESET && !strcmp(em->name, "CZ-1") &&
          em->ownenv && em->block && em->done && em->nhot == 1u && em->hot[0].bytes == sizeof(float) * 1025u);
    check("its hot set's FM6_SIN constants are the firmware's RAM table, value for value", !memcmp(oc_host_cz_fm6_sin(), FM6_SIN, sizeof(float) * 1025u));
#endif
#if RET_SWITCH
    {
        const engine_t *eb = ENGINES[RET_ENGINE];
        check("the reference build has it built in, in its slot", eb && !strcmp(eb->name, RET_NAME) && eb != em && eng_ok(RET_ENGINE));
        render(eb, other, &crashed);
        check("the built-in engine renders its presets", crashed == 0);
        ENGINES[RET_ENGINE] = em;                          /* the registration, by hand (the built-in engine holds the slot), with its header (its state's size: */
        eng_mod[RET_ENGINE] = &h;                          /* cz_mod_part_t, the tone with it) and its hot set */
        check("the module's hot set is taken (by hand, as the loader does)", eng_hot_take(RET_ENGINE, 0, em->hot, em->nhot) == EM_OK);
        render(em, module, &crashed);
        check("the module renders its presets", crashed == 0);
        for (i = 0; i < NPRESET; i++) {
            same += module[i] == other[i];
            if (module[i] != other[i])
                printf("module host (%s):   %s module %016llx built-in %016llx\n", RET_NAME, names[i], (unsigned long long)module[i], (unsigned long long)other[i]);
        }
        check("every hash equals the built-in engine's", same == NPRESET);
        check("... and the line in tests/golden_modules.txt", golden(gold, ng, module) == NPRESET);
        eng_hot_drop(RET_ENGINE);
        eng_mod[RET_ENGINE] = 0;
        ENGINES[RET_ENGINE] = eb;
    }
#else
    {
        uint32_t n, offered = 0;
        check("the slot is retired without the module: not offered, a stored engine keeps its number (silent)",
              ENGINES[RET_ENGINE] == &ENG_RETIRED && !eng_ok(RET_ENGINE) && eng_sound_idx(RET_ENGINE) == RET_ENGINE);
        check("the loader's registration takes the slot (a candidate's module slot in this build)", eng_register(&h, em) == EM_OK && ENGINES[RET_ENGINE] == em);
        for (n = 0; n < NENG_SHOWN; n++) offered += eng_vis(n) == RET_ENGINE;
        check("registered, it is offered (eng_ok, once in ENGINE_ORDER)", eng_ok(RET_ENGINE) && offered == 1u);
        render(em, module, &crashed);
        check("the module renders its presets", crashed == 0);
        check("... the lines in tests/golden_modules.txt (the reference build's built-in engine renders the same)", golden(gold, ng, module) == NPRESET);
        eng_unregister(RET_ENGINE);
        check("unregistered, the slot is retired again (not offered, the number kept)", ENGINES[RET_ENGINE] == &ENG_RETIRED && !eng_ok(RET_ENGINE));
    }
#endif
    printf(bad ? "module host (%s): FAILED\n" : "module host (%s): all passed\n", RET_NAME);
    return bad != 0;
}
