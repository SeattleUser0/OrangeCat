/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS (the model picker's module): a model's descriptor, the one thing the glue knows about a model. Each model unit defines its
 * descriptor `pl_mdl_<key>` (24 bytes: tools/pick_module.py reads state and big from the master image at the symbol's address, to size a
 * pick's arena state); the glue holds a table of weak pointers to all of them (eng_plaits.c PL_MODELS), so a model whose unit is not in
 * the image reads as a null pointer: not installed. No name in here (the glue's own table has every name, so the knob can show an
 * uninstalled model as MISSING), and no string at all in a model unit, so the linker merges nothing across regions. */
#pragma once
typedef struct {
    uint32_t state;                                   /* bytes of state a voice (the model's struct) */
    uint8_t big;                                      /* 1: one voice a track (its state is large); else PL_POLY voices, a state each */
    uint8_t draws;                                    /* 1: reads plaits' shared random generator in its render: it renders on CPU0, in voice order */
    uint8_t enveloped;                                /* 1: makes its own envelope (the low-pass gate is skipped) */
    uint8_t units;                                    /* voice budget units a voice takes (engine_t.units) */
    float out, aux;                                   /* the module's output levels (voice.cc: OUT, AUX; below 0: through a limiter at that drive) */
    void (*start)(void *m);                           /* the model's state from scratch (init and reset) */
    int (*render)(void *m, const pl_params_t *q, float *o, float *a, uint32_t n);   /* a block, then its flush; -> already_enveloped as the model says this block */
} pl_mdl_t;
#ifndef OC_MODULE_HOST
_Static_assert(sizeof(pl_mdl_t) == 24u, "pl_mdl_t: tools/pick_module.py reads it");
#endif
#define PL_MDL_STATE_AT 0u
#define PL_MDL_BIG_AT 4u

/* A unit's exports, kept: the symbols another region may name (its descriptor, its bank, the six-op's start and render) go into a pointer array
 * in section .plaits_keep, which the link script keeps (KEEP), so the linker's garbage collection keeps the same functions in a unit whether or
 * not the regions that name them are linked (the master link has every region; a pick's link may lack the one that would have kept a symbol
 * alive). Without this the two links differ (tools/build.py --pick 6opb: the bank-0 start would be gone from the six-op region). */
#define PL_KEEP(...) __attribute__((section(".plaits_keep"), used)) static const void *const PL_CAT3(pl_keep_, PL_UNIT, __COUNTER__) [] = { __VA_ARGS__ }

/* a unit's start and render for model `key` of type T through its pl_<prefix>_init / _reset / _render / _flush */
#define PL_MDL_FUNCS(key, T, prefix) \
    static void pl_mdl_##key##_start(void *m) { prefix##_init((T *)m); prefix##_reset((T *)m); } \
    static int pl_mdl_##key##_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n) \
    { int env = 0; prefix##_render((T *)m, q, o, a, n, &env); prefix##_flush((T *)m); return env; }
#define PL_MDL_DESC(key, T, big, draws, env, units, out, aux) \
    { sizeof(T), big, draws, env, units, out, aux, pl_mdl_##key##_start, pl_mdl_##key##_render }
