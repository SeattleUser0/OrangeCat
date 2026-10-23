/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.10.0 (module PoC step 3): SHAPES as a module, on the host, before any hardware is touched.
 *
 * The module's source (tools/module/shapes_mod.c) is built as its own translation unit (tests/module_host_tu.c, OC_MODULE_HOST):
 * the same engine code with the firmware's services replaced by the table's members, as on the device. This test gives it the
 * firmware's own table (FW_API, engines.c), checks its header with fw_module_check, puts its engine_t in SHAPES' slot of the
 * (here writable) engine table, and renders the 24 SHAPES presets through the whole voice path with tests/regress.c's own
 * machinery (the phrase, the voices, the master, the FNV-1a 64 of the samples). Each hash must be the built-in engine's, and the
 * line in tests/golden.txt. A control: with the pack's ready flag at 0 the module is silent, so the hashes change; the table
 * is what the module reads.
 *
 *   build/host/module_host_test [golden file]
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("module host: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void render(uint64_t *hash, uint32_t *crashed, uint32_t np)
{
    uint32_t i;
    nj = 0;
    for (i = 0; i < np; i++) {
        char name[64], s[64];
        job_t *j;
        slug(s, eng_preset(ENGINES[ENGI_SHAPES], i)->name, sizeof s);
        snprintf(name, sizeof name, "preset/%s/%02u_%s", ENGINES[ENGI_SHAPES]->name, i, s);
        j = add(J_PRESET, name);
        j->e = ENGI_SHAPES;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    *crashed = 0;
    for (i = 0; i < np; i++) {
        hash[i] = J[i].r.hash;
        *crashed += J[i].crashed != 0;
    }
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden.txt";
    static kv_t gold[MAXJ];
    static uint64_t builtin[64], module[64], silent[64];
    static char names[64][64];
    uint32_t ng, np, i, crashed, same = 0, golden_same = 0, differ = 0;
    const engine_t *eb = ENGINES[ENGI_SHAPES], *em;
    fw_mod_hdr_t h;

    oc_host_fw = &FW_API;                              /* the firmware's own table: what the device gives its module */
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    check("the module's header is accepted by the registration check (identifier, table address, versions)", fw_module_check(&h) == FW_MOD_OK);
    check("the header names SHAPES, the state size of sh_part_t and the pack the tables came from",
          h.engine == ENGI_SHAPES && h.state_size == sizeof(sh_part_t) && h.pack_hash == OC_PACK_HASH);
    check("the module's engine_t is its own (not the built-in's) and has the same name, presets and controls",
          em != eb && !strcmp(em->name, eb->name) && em->npresets == eb->npresets && em->note_on != eb->note_on && em->render != eb->render);
    np = eb->npresets;
    check("SHAPES has 24 presets", np == 24u);
    {   uint32_t k, e = 1;
        for (k = 0; k < 8u; k++) e &= !strcmp(em->edit[k].label, eb->edit[k].label) && em->edit[k].min == eb->edit[k].min && em->edit[k].max == eb->edit[k].max && em->edit[k].def == eb->edit[k].def;
        check("the eight controls: names, ranges, defaults", e); }
    if (np > 64u) return 1;
    ng = load_kv(gpath, gold, MAXJ);

    render(builtin, &crashed, np);
    check("the built-in engine renders all presets", crashed == 0);
    for (i = 0; i < np; i++) snprintf(names[i], sizeof names[i], "%s", J[i].name);

    ENGINES[ENGI_SHAPES] = em;                         /* the loader's registration, by hand (steps 4 and 5 do it at boot) */
    render(module, &crashed, np);
    check("the module renders all presets", crashed == 0);
    for (i = 0; i < np; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)module[i]);
        same += module[i] == builtin[i];
        golden_same += want && !strcmp(want, hs);
        if (module[i] != builtin[i] || !want || strcmp(want, hs))
            printf("module host:   %s module %s built-in %016llx golden %s\n", names[i], hs, (unsigned long long)builtin[i], want ? want : "-");
    }
    check("every one of the 24 hashes equals the built-in engine's", same == np);
    check("... and the line in tests/golden.txt", golden_same == np);

    oc_pack_good = 0;                                  /* control: the module reads the flag through the table and falls silent */
    render(silent, &crashed, np);
    for (i = 0; i < np; i++) differ += silent[i] != module[i];
    check("control: with the pack's flag at 0 the module is silent (all hashes change): it reads the firmware's table", crashed == 0 && differ == np);
    oc_pack_good = OC_PACK_GATE ? 0 : 1;

    printf(bad ? "module host: FAILED\n" : "module host: all passed\n");
    return bad != 0;
}
