/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.10.0 (module PoC step 4): the engine table in RAM and the registration of a module's engine (engines.c).
 * Built three ways by tests/run_tests.sh:
 *   default                 SHAPES built in, OC_MODULE_WINS 0: a module for SHAPES is "shadowed", the table is unchanged
 *   -DOC_MODULE_WINS=1      SHAPES built in: a module takes the slot; unregistering puts the built-in engine back
 *   -DMELODEE_SHAPES=0      SHAPES built out (the second build of the plan): the slot is retired until a module supplies it
 * The loader that calls eng_register at boot is step 5 (tests/oc_module_test.c).
 *   build/host/engine_table_test*
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("engine table: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static engine_t fake_engine;                         /* a module's engine_t: a copy of DRUM-X's, renamed */
static fw_mod_hdr_t hdr(uint32_t engine)
{
    fw_mod_hdr_t h = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR, engine, 1234u, 0, 0, OC_PACK_HASH, 0, 0, 0};
    return h;
}
int main(void)
{
    static fw_mod_hdr_t h;
    uint32_t k, seen, n;
    const engine_t *orig[NENGINES];
    for (k = 0; k < NENGINES; k++) orig[k] = ENGINES[k];
    fake_engine = ENG_DRUMX;
    fake_engine.name = "SHAPES";
    h = hdr(ENGI_SHAPES);

    check("the engine table is 20 pointers in RAM (80 B on the target), and the module table beside it is the same size",
          sizeof ENGINES == NENGINES * sizeof(void *) && sizeof eng_mod == NENGINES * sizeof(void *) && NENGINES == 20u);
    ENGINES[0] = ENGINES[0];                         /* (writable: this compiles and runs) */
    check("nothing is registered at the start", ({ int z = 1; for (k = 0; k < NENGINES; k++) z &= eng_mod[k] == 0; z; }));
    check("NENG_SHOWN counts SHAPES' slot (a module may supply it) and ENGINE_ORDER ends with it",
          eng_vis(NENG_SHOWN - 1u) == ENGI_SHAPES);

    /* refusals that hold in every build */
    check("no header or no engine is refused", eng_register(0, &fake_engine) == EM_BAD && eng_register(&h, 0) == EM_BAD);
    {   uint32_t bads[] = {0, 1, 3, 4, ENGI_MACRO, 10, ENGI_FM6, 13, ENGI_CZ, 16, 17, 18, 20, 255, 0xFFFFFFFFu}, ok = 1, i;
        for (i = 0; i < sizeof bads / sizeof bads[0]; i++) {
            fw_mod_hdr_t g = hdr(bads[i]);
            ok &= eng_register(&g, &fake_engine) == EM_SLOT;
        }
        check("a number a module may not supply (anything but SHAPES' 14) is refused and the table is untouched",
              ok && ({ int z = 1; for (k = 0; k < NENGINES; k++) z &= ENGINES[k] == orig[k] && eng_mod[k] == 0; z; })); }

#if MELODEE_SHAPES
    check("SHAPES is built in: its slot holds the built-in engine, its state size is sh_part_t's, it is selectable",
          ENGINES[ENGI_SHAPES] == &ENG_SHAPES && eng_state_size(ENGI_SHAPES) == sizeof(sh_part_t) && eng_ok(ENGI_SHAPES));
#if OC_MODULE_WINS
    check("a module for SHAPES takes the slot (OC_MODULE_WINS 1)", eng_register(&h, &fake_engine) == EM_OK && ENGINES[ENGI_SHAPES] == &fake_engine && eng_mod[ENGI_SHAPES] == &h);
    check("the state size now comes from the module's header, the engine is still selectable",
          eng_state_size(ENGI_SHAPES) == 1234u && eng_ok(ENGI_SHAPES));
    check("nothing else in the table moved", ({ int z = 1; for (k = 0; k < NENGINES; k++) z &= k == ENGI_SHAPES || (ENGINES[k] == orig[k] && !eng_mod[k]); z; }));
    eng_unregister(ENGI_SHAPES);
    check("unregistering puts the built-in engine back, its state size, nothing registered",
          ENGINES[ENGI_SHAPES] == &ENG_SHAPES && eng_state_size(ENGI_SHAPES) == sizeof(sh_part_t) && !eng_mod[ENGI_SHAPES]);
#else
    check("a module for SHAPES is shadowed by the built-in engine (OC_MODULE_WINS 0): refused, the table unchanged",
          eng_register(&h, &fake_engine) == EM_SHADOWED && ENGINES[ENGI_SHAPES] == &ENG_SHAPES && !eng_mod[ENGI_SHAPES] && eng_state_size(ENGI_SHAPES) == sizeof(sh_part_t));
#endif
#else
    check("SHAPES is built out: its slot is the reserved one, unselectable, no state",
          ENGINES[ENGI_SHAPES] == &ENG_RETIRED && !eng_ok(ENGI_SHAPES) && eng_state_size(ENGI_SHAPES) == 0u);
    for (seen = 0, n = eng_vis(0), k = 0; k < NENG_SHOWN + 2u; k++) { n = eng_step(n, 1); seen += n == ENGI_SHAPES; }
    check("stepping through the engines (ENG knob) never stops on it while no module supplies it", seen == 0u);
    check("a module takes the empty slot", eng_register(&h, &fake_engine) == EM_OK && ENGINES[ENGI_SHAPES] == &fake_engine && eng_mod[ENGI_SHAPES] == &h);
    check("then it is selectable, with the module's state size", eng_ok(ENGI_SHAPES) && eng_state_size(ENGI_SHAPES) == 1234u);
    for (seen = 0, n = eng_vis(0), k = 0; k < NENG_SHOWN + 2u; k++) { n = eng_step(n, 1); seen += n == ENGI_SHAPES; }
    check("and the ENG knob reaches it, forwards and back", seen >= 1u && eng_step(eng_vis(0), -1) == ENGI_SHAPES);
    check("the browser lists only engines that are there (the list is the same length)", ({ int z = 1; for (k = 0; k < NENG_SHOWN; k++) z &= eng_ok(eng_vis(k)); z; }));
    eng_unregister(ENGI_SHAPES);
    check("unregistering retires the slot again", ENGINES[ENGI_SHAPES] == &ENG_RETIRED && !eng_ok(ENGI_SHAPES) && eng_state_size(ENGI_SHAPES) == 0u && !eng_mod[ENGI_SHAPES]);
    seen = 0;
    for (n = eng_vis(0), k = 0; k < NENG_SHOWN + 2u; k++) { n = eng_step(n, 1); seen += n == ENGI_SHAPES; }
    check("... and the knob skips it once more", seen == 0u);
#endif
    printf(bad ? "engine table: FAILED\n" : "engine table: all passed\n");
    return bad != 0;
}
