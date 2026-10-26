/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "CZ-1"): the CZ-1 as a module (tools/module/cz_mod.c, engine 15), on the
 * host, against the built-in engine of the reference build and tests/golden_modules.txt, as tests/module_host_wheel_test.c is for WHEEL.
 *
 * Built twice by tests/run_tests.sh, with tests/module_host_cz_tu.c (the module's own unit) linked in, both measurement builds (OC_CANDIDATES=1):
 *   -DMELODEE_CZ=1  the reference build: the CZ-1 is built in (eng_cz.c, cz_native.c, its tone the firmware's cz_patch[]). Its 65 presets (INIT and
 *                   Casio's 64) render through the whole voice path (tests/regress.c), then the module's engine_t is put in the slot by hand (with its
 *                   hot set: FM6_SIN) and renders them again: every hash must be the built-in engine's, byte for byte;
 *   (default)       the firmware's engine set (MELODEE_CZ=0): slot 15 is retired; the loader's registration puts the module there (a candidate's slot
 *                   in this build), it is offered, its presets render the lines of tests/golden_modules.txt (module/CZ-1/..: the reference build's
 *                   built-in renders), and unregistering retires the slot again.
 * Both: the header passes the registration check; the module's FM6_SIN constants (its hot set) are the firmware's RAM table, value for value.
 *
 *   build/host/module_host_cz_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern const float *oc_host_cz_fm6_sin(void);
#if !OC_CANDIDATES
#error "build with -DOC_CANDIDATES=1"
#endif

static int bad;
static void check(const char *what, int ok)
{
    printf("module host (CZ-1%s): %-100s %s\n", MELODEE_CZ ? ", reference build" : "", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define NPRESET 65u
static char names[NPRESET][64];
static void render(const engine_t *named, uint64_t *hash, uint32_t *crashed)
{
    uint32_t i;
    nj = 0;
    for (i = 0; i < NPRESET; i++) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(named, i)->name, sizeof s);
        snprintf(names[i], 64, "module/CZ-1/%02u_%s", i, s);
        j = add(J_PRESET, names[i]);
        j->e = ENGI_CZ;
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
        else if (!want || strcmp(want, hs)) printf("module host (CZ-1):   %s %s golden %s\n", names[i], hs, want ? want : "-");
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
    check("the module's header is accepted by the registration check and names engine 15 (ENGI_CZ)", fw_module_check(&h) == FW_MOD_OK &&
          h.engine == ENGI_CZ && h.engine == 15u && h.pack_hash == OC_PACK_HASH);
    check("65 presets (INIT and Casio's 64), its own envelopes, the block hook, one hot set table", em->npresets == NPRESET && !strcmp(em->name, "CZ-1") &&
          em->ownenv && em->block && em->done && em->nhot == 1u && em->hot[0].bytes == sizeof(float) * 1025u);
    check("its hot set's FM6_SIN constants are the firmware's RAM table, value for value", !memcmp(oc_host_cz_fm6_sin(), FM6_SIN, sizeof(float) * 1025u));
#if MELODEE_CZ
    {
        const engine_t *eb = ENGINES[ENGI_CZ];
        check("the reference build has the CZ-1 built in, in slot 15", eb && !strcmp(eb->name, "CZ-1") && eb != em && eng_ok(ENGI_CZ));
        render(eb, other, &crashed);
        check("the built-in CZ-1 renders its 65 presets", crashed == 0);
        ENGINES[ENGI_CZ] = em;                          /* the registration, by hand (the built-in engine holds the slot), with its header (its state's size: */
        eng_mod[ENGI_CZ] = &h;                          /* cz_mod_part_t, the tone with it) and its hot set */
        check("the module's hot set is taken (by hand, as the loader does)", eng_hot_take(ENGI_CZ, 0, em->hot, em->nhot) == EM_OK);
        render(em, module, &crashed);
        check("the module renders its 65 presets", crashed == 0);
        for (i = 0; i < NPRESET; i++) {
            same += module[i] == other[i];
            if (module[i] != other[i])
                printf("module host (CZ-1):   %s module %016llx built-in %016llx\n", names[i], (unsigned long long)module[i], (unsigned long long)other[i]);
        }
        check("every hash equals the built-in engine's", same == NPRESET);
        check("... and the line in tests/golden_modules.txt", golden(gold, ng, module) == NPRESET);
        eng_hot_drop(ENGI_CZ);
        eng_mod[ENGI_CZ] = 0;
        ENGINES[ENGI_CZ] = eb;
    }
#else
    {
        uint32_t n, offered = 0;
        check("slot 15 is retired without the module: not offered, a stored engine 15 keeps its number (silent)",
              ENGINES[ENGI_CZ] == &ENG_RETIRED && !eng_ok(ENGI_CZ) && eng_sound_idx(ENGI_CZ) == ENGI_CZ);
        check("the loader's registration takes slot 15 (a candidate's module slot in this build)", eng_register(&h, em) == EM_OK && ENGINES[ENGI_CZ] == em);
        for (n = 0; n < NENG_SHOWN; n++) offered += eng_vis(n) == ENGI_CZ;
        check("registered, the CZ-1 is offered (eng_ok, once in ENGINE_ORDER)", eng_ok(ENGI_CZ) && offered == 1u);
        render(em, module, &crashed);
        check("the module renders its 65 presets", crashed == 0);
        check("... the lines in tests/golden_modules.txt (the reference build's built-in CZ-1 renders the same)", golden(gold, ng, module) == NPRESET);
        eng_unregister(ENGI_CZ);
        check("unregistered, the slot is retired again (not offered, the number kept)", ENGINES[ENGI_CZ] == &ENG_RETIRED && !eng_ok(ENGI_CZ));
    }
#endif
    printf(bad ? "module host (CZ-1): FAILED\n" : "module host (CZ-1): all passed\n");
    return bad != 0;
}
