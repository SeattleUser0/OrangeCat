/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *
 * Orange Cat 0.13.7: engines' hot sets (core.h engine_t hot / nhot / hot_at, engines.c eng_hot_*). An engine's per-sample tables are copied
 * into the audio arena when it is registered and read from there (ENG_HOT); a module's set that does not fit is refused (EM_NOROOM).
 * Built with OC_MODULE_WINS=1 (a module takes SHAPES' slot) by tests/run_tests.sh.
 *   build/host/hot_set_test
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("hot set: %-118s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int in_arena(const void *p)
{
    for (uint32_t k = 0; k < ENG_HOT_SLOTS; k++)
        if (resource[RES_HOT0 + k].size && (const uint8_t *)p >= (const uint8_t *)resource[RES_HOT0 + k].ptr &&
            (const uint8_t *)p < (const uint8_t *)resource[RES_HOT0 + k].ptr + resource[RES_HOT0 + k].size)
            return 1;
    return 0;
}
static int copies_ok(const engine_t *g, const void *const *at)
{
    for (uint32_t i = 0; i < g->nhot; i++)
        if (!in_arena(at[i]) || memcmp(at[i], g->hot[i].src, g->hot[i].bytes)) return 0;
    return 1;
}
static int at_sources(const engine_t *g, const void *const *at)
{
    for (uint32_t i = 0; i < g->nhot; i++)
        if (at[i] != g->hot[i].src) return 0;
    return 1;
}
static const int16_t mod_a[300] = {1, 2, 3}, mod_b[40] = {7};
static const uint8_t mod_huge[1];                    /* (a set that claims more than the arena holds: never read) */
static const eng_hot_t MOD_HOT[2] = {{mod_a, sizeof mod_a}, {mod_b, sizeof mod_b}};
static const eng_hot_t HUGE_HOT[1] = {{mod_huge, 4u * 1024u * 1024u}};
static engine_t mod_engine, huge_engine;

int main(void)
{
    static fw_mod_hdr_t h = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR, ENGI_SHAPES, 1234u, 0, 0, 0, 0, 0, 0, 0, 0};
    const engine_t *shapes = ENGINES[ENGI_SHAPES];
    uint32_t bytes = 0;
    for (uint32_t i = 0; i < SH_HOT_N; i++) bytes += SH_HOT[i].bytes;

    check("SHAPES declares its sine and four wave shapers: 5 tables, 2,570 B (each 514 B: 257 int16), arena 2,580 B with each rounded to 4",
          shapes == &ENG_SHAPES && shapes->nhot == 5u && bytes == 2570u && eng_hot_bytes(shapes->hot, shapes->nhot) == 2580u && shapes->hot_at == sh_hot);
    check("before any registration SHAPES reads the sources (a host render or a build that never boots reads what it always read)",
          at_sources(shapes, sh_hot));
    check("the Prophet declares no hot set: its sample loop reads no table (arena 0)", ENG_P5_TEST.nhot == 0u && eng_hot_bytes(ENG_P5_TEST.hot, ENG_P5_TEST.nhot) == 0u);

    eng_hot_boot();
    check("boot: SHAPES' set is in the arena, byte for byte its sources, and the names read the copies", copies_ok(shapes, sh_hot) &&
          (const void *)pb_wav_sine == sh_hot[0] && (const void *)pb_ws_tri_fold == sh_hot[4] && eng_hs.slot[ENGI_SHAPES] == 1u);
    check("boot: the pack hook is set (an install or erase of the pack copies the sets again)", oc_pack_changed == eng_hot_refresh);
    {   const void *before = sh_hot[0];
        int16_t *p = (int16_t *)(uintptr_t)sh_hot[0];
        p[3] = (int16_t)~p[3];                       /* a stale copy */
        oc_pack_changed();
        check("refresh: the copies are made again from the sources, in the same block", sh_hot[0] == before && copies_ok(shapes, sh_hot)); }

    mod_engine = ENG_DRUMX; mod_engine.name = "SHAPES"; mod_engine.hot = MOD_HOT; mod_engine.nhot = 2; mod_engine.hot_at = 0;
    check("a module's set: registered, the firmware's pointers (fw_api_t hot, what ENG_HOT reads in a module) are its copies",
          eng_register(&h, &mod_engine) == EM_OK && ENGINES[ENGI_SHAPES] == &mod_engine && eng_hs.ptr[ENGI_SHAPES] &&
          copies_ok(&mod_engine, eng_hs.ptr[ENGI_SHAPES]) && FW_API.hot[ENGI_SHAPES] == eng_hs.ptr[ENGI_SHAPES] &&
          FW_API.hot[ENGI_SHAPES][1] == eng_hs.ptr[ENGI_SHAPES][1]);
    check("the built-in engine's set left with it: its pointers are back at the sources (nothing reads a released block)", at_sources(shapes, sh_hot));

    huge_engine = mod_engine; huge_engine.hot = HUGE_HOT; huge_engine.nhot = 1;
    eng_unregister(ENGI_SHAPES);
    check("unregistered: the module's pointers are gone and the built-in engine's set is in the arena again",
          eng_hs.ptr[ENGI_SHAPES] == 0 && ENGINES[ENGI_SHAPES] == shapes && copies_ok(shapes, sh_hot));
    check("NO ROOM: a module whose set does not fit the arena is refused, and the built-in engine keeps its slot and its set",
          eng_register(&h, &huge_engine) == EM_NOROOM && ENGINES[ENGI_SHAPES] == shapes && !eng_mod[ENGI_SHAPES] &&
          copies_ok(shapes, sh_hot) && !(eng_hs.refused >> ENGI_SHAPES & 1u));
    {   engine_t many = mod_engine; eng_hot_t list[ENG_HOT_MAX + 1u];
        for (uint32_t i = 0; i <= ENG_HOT_MAX; i++) list[i] = MOD_HOT[0];
        many.hot = list; many.nhot = ENG_HOT_MAX + 1u;
        check("a set of more than ENG_HOT_MAX tables is refused as bad, the built-in engine stays", eng_register(&h, &many) == EM_BAD &&
              ENGINES[ENGI_SHAPES] == shapes && copies_ok(shapes, sh_hot)); }
    printf("hot set: %s\n", bad ? "FAILED" : "all passed");
    return bad != 0;
}
