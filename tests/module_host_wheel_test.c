/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.14.0: WHEEL as a module (engine 7), on the host, against the built-in engine of the reference build and the module golden file.
 *
 * Built twice by tests/run_tests.sh, with tests/module_host_wheel_tu.c (the module's own unit, the default engine set) linked in:
 *   -DMELODEE_LEGACY_EXTRAS=1  the reference build: WHEEL is built in (eng_wheel.c, engine 7). The module's engine_t is put in the slot by hand and its five
 *                              presets render through the whole voice path (tests/regress.c) with the same hashes as the built-in engine's.
 *   (default)                  the firmware's engine set: slot 7 is retired. A track stored with engine 7 keeps it and is silent; the loader's registration
 *                              (eng_register) puts the module in the slot, the engine is offered (eng_ok, ENGINE_ORDER) and the same track plays it again;
 *                              unregistering retires the slot again.
 * Both compare the hashes with tests/golden_modules.txt (module/WHEEL/..., new lines of 0.14.0). The archived lines of tests/golden_retired_extras.txt
 * (preset/WHEEL/..., October 8) are not these renders: they were taken with the presets' effect sends and the voice path of that day, which the effects'
 * removal (1e47387) and later voice work changed; today's built-in WHEEL does not render them either (docs/orangecat/ENGINE_CANDIDATES.md, "Wheel").
 *
 *   build/host/module_host_wheel_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("module host (WHEEL%s): %-100s %s\n", MELODEE_LEGACY_EXTRAS ? ", reference build" : "", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define NPRESET 5u
static void render(const engine_t *named, uint64_t *hash, uint32_t *crashed, char names[][64])   /* named: whose preset names (the retired slot has none) */
{
    uint32_t i;
    nj = 0;
    for (i = 0; i < NPRESET; i++) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(named, i)->name, sizeof s);
        snprintf(names[i], 64, "module/WHEEL/%02u_%s", i, s);
        j = add(J_PRESET, names[i]);
        j->e = ENGI_WHEEL;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    *crashed = 0;
    for (i = 0; i < NPRESET; i++) {
        hash[i] = J[i].r.hash;
        *crashed += J[i].crashed != 0;
    }
}
static uint32_t golden(const kv_t *gold, uint32_t ng, char names[][64], const uint64_t *h)
{
    uint32_t i, same = 0;
    for (i = 0; i < NPRESET; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)h[i]);
        same += want && !strcmp(want, hs);
        if (!want || strcmp(want, hs))
            printf("module host (WHEEL):   %s %s golden %s\n", names[i], hs, want ? want : "-");
    }
    return same;
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden_modules.txt";
    static kv_t gold[MAXJ];
    static uint64_t module[NPRESET], other[NPRESET];
    static char names[NPRESET][64];
    uint32_t ng, i, crashed, same = 0;
    const engine_t *em;
    fw_mod_hdr_t h;

    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    ng = load_kv(gpath, gold, MAXJ);
    check("the module's header is accepted by the registration check (identifier, table address, versions)", fw_module_check(&h) == FW_MOD_OK);
    check("the header names engine 7 (ENGI_WHEEL, Melodee's number), the state size of drw_part_t and the pack hash",
          h.engine == ENGI_WHEEL && h.engine == 7u && h.state_size == sizeof(drw_part_t) && h.pack_hash == OC_PACK_HASH);
    check("the module declares no hot set of its own (it reads SINE and TANH_Q15 through the firmware's base_hot)", em->nhot == 0u && !em->hot && !em->hot_at);
    check("five presets, the eight controls BARS and TONE, the block hook", em->npresets == NPRESET && !strcmp(em->name, "WHEEL") && em->block &&
          !strcmp(em->page_title[0], "BARS") && !strcmp(em->page_title[1], "TONE") && !strcmp(em->edit[0].label, "REG") && !strcmp(em->edit[7].label, "ROTR"));
    {   /* (0.14.0) base_hot: the module's sine is the firmware's; boot's copy into the arena changes no sample */
        int32_t s0 = 0, s1 = 0;
        for (i = 0; i < 1024u; i++) s0 += osc_sine(i * 0x00400000u + 0x1234u);
        memset(resource, 0, sizeof resource);
        eng_hot_base();
        for (i = 0; i < 1024u; i++) s1 += osc_sine(i * 0x00400000u + 0x1234u);
        check("boot copies SINE into the arena (base_hot[0] is the copy) and the sine the engines read is the same", dsp_hot_at[DSP_HOT_SINE] == resource[RES_HOT_BASE].ptr &&
              FW_API.base_hot[DSP_HOT_SINE] == resource[RES_HOT_BASE].ptr && s0 == s1);
        memset(resource, 0, sizeof resource);
        dsp_hot_at[DSP_HOT_SINE] = SINE;
    }
#if MELODEE_LEGACY_EXTRAS
    {
        const engine_t *eb = ENGINES[ENGI_WHEEL];
        check("the reference build has WHEEL built in, in slot 7", eb && !strcmp(eb->name, "WHEEL") && eb != em && eng_ok(ENGI_WHEEL));
        render(em, other, &crashed, names);
        check("the built-in WHEEL renders its five presets", crashed == 0);
        ENGINES[ENGI_WHEEL] = em;                       /* the registration, by hand (the built-in engine holds the slot: eng_register would call it shadowed) */
        render(em, module, &crashed, names);
        check("the module renders its five presets", crashed == 0);
        for (i = 0; i < NPRESET; i++) {
            same += module[i] == other[i];
            if (module[i] != other[i])
                printf("module host (WHEEL):   %s module %016llx built-in %016llx\n", names[i], (unsigned long long)module[i], (unsigned long long)other[i]);
        }
        check("every hash equals the built-in engine's", same == NPRESET);
        check("... and the line in tests/golden_modules.txt", golden(gold, ng, names, module) == NPRESET);
        ENGINES[ENGI_WHEEL] = eb;
    }
#else
    {
        track_t *t = &trk[0];
        uint32_t n, offered = 0;
        check("slot 7 is retired without the module: not offered, a stored engine 7 keeps its number (silent)",
              ENGINES[ENGI_WHEEL] == &ENG_RETIRED && !eng_ok(ENGI_WHEEL) && eng_sound_idx(ENGI_WHEEL) == ENGI_WHEEL);
        render(em, other, &crashed, names);
        for (i = 0; i < NPRESET; i++) same += other[i] == other[0];
        check("a track of engine 7 without the module: every render is the same silence", crashed == 0 && same == NPRESET);
        check("the loader's registration takes slot 7 (eng_register: a module slot, nothing built in)", eng_register(&h, em) == EM_OK && ENGINES[ENGI_WHEEL] == em &&
              eng_mod[ENGI_WHEEL] == &h);
        for (n = 0; n < NENG_SHOWN; n++) offered += eng_vis(n) == ENGI_WHEEL;
        check("registered, WHEEL is offered (eng_ok, once in ENGINE_ORDER) and a stored engine 7 is its own", eng_ok(ENGI_WHEEL) && offered == 1u &&
              eng_sound_idx(ENGI_WHEEL) == ENGI_WHEEL && eng_state_size(ENGI_WHEEL) == sizeof(drw_part_t));
        host_tracks_init();
        t->engine = t->eng_req = (uint8_t)ENGI_WHEEL;   /* a project's track stored with WHEEL before the module came */
        check("the same stored track now has WHEEL's state and plays (eng_state_prepare)", eng_state_prepare(t) && ENGINES[t->engine] == em);
        render(em, module, &crashed, names);
        check("the module renders its five presets", crashed == 0);
        for (i = 0, same = 0; i < NPRESET; i++) same += module[i] != other[i];
        check("... and they are sound, not the silence of the retired slot", same == NPRESET);
        check("... the lines in tests/golden_modules.txt (the reference build's built-in WHEEL renders the same)", golden(gold, ng, names, module) == NPRESET);
        eng_unregister(ENGI_WHEEL);
        check("unregistered, the slot is retired again (not offered, the number kept)", ENGINES[ENGI_WHEEL] == &ENG_RETIRED && !eng_ok(ENGI_WHEEL) &&
              eng_sound_idx(ENGI_WHEEL) == ENGI_WHEEL);
    }
#endif
    if (getenv("GOLDEN_PRINT"))
        for (i = 0; i < NPRESET; i++) printf("%s %016llx\n", names[i], (unsigned long long)module[i]);
    printf(bad ? "module host (WHEEL): FAILED\n" : "module host (WHEEL): all passed\n");
    return bad != 0;
}
