/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (Release 3, the model picker: docs/candidates/model_picker.md) A MACRO model's descriptor, the one thing the glue (eng_macro.c) knows
 * about a model, and the layout of the part the models render into. Shared by the firmware (its built-in models) and the MODELS module the
 * installer page assembles (tools/module/plaits/: one unit a model, each a region of one master image; the page keeps the picked regions).
 * The glue holds a table of descriptor pointers for the built-in models and reads the installed module's table (fw_models.h) for the rest,
 * so a model whose unit is not there reads as a null pointer: not installed. No name in here (the glue's own table has every name, so the
 * knob can show an uninstalled model as MISSG), and no string at all in a model unit, so the linker merges nothing across regions.
 *
 * A change to pl_mdl_t, pl_vhead_t, pl_part_t or fw_models_wire_t moves PL_MDL_ABI_ID (tools/gen_fw_api_id.py); a module carries it and the
 * loader refuses another one, so the interface identifier of every other module (FW_API_ID) does not move for the models.
 *
 * The includer says, before this header: PL_MDL_LINKAGE (static in the firmware, where every built-in model is in one unit; nothing in a
 * module unit, whose descriptor another region names), PL_MDL_ATTR (the section attribute of a model's start and render: MC_XIP in the
 * firmware, nothing in a module), and it has pl_dsp.h (pl_params_t, pl_svf_t, pl_limiter_t). */
#pragma once
#include "../fw_models.h"                                /* PL_MDL_WIRE and the offsets, PL_POLY, PL_PART_HEAD, pl_need */
#ifndef PL_MDL_LINKAGE
#define PL_MDL_LINKAGE
#endif
#ifndef PL_MDL_ATTR
#define PL_MDL_ATTR
#endif
typedef struct {
    uint32_t state;                                   /* bytes of state a voice (the model's struct) */
    uint8_t big;                                      /* 1: one voice a track (its state is large); else PL_POLY voices, a state each */
    uint8_t draws;                                    /* 1: reads plaits' shared random generator in its render: it renders on CPU0, in voice order */
    uint8_t enveloped;                                /* 1: makes its own envelope (the low-pass gate is skipped) */
    uint8_t units;                                    /* voice budget units a voice takes (engine_t.units; the rule: pl_models.h) */
    float out, aux;                                   /* the module's output levels (voice.cc: OUT, AUX; below 0: through a limiter at that drive) */
    void (*start)(void *m);                           /* the model's state from scratch (init and reset) */
    int (*render)(void *m, const pl_params_t *q, float *o, float *a, uint32_t n);   /* a block, then its flush; -> already_enveloped as the model says this block */
} pl_mdl_t;
_Static_assert(sizeof(void *) != 4u || sizeof(pl_mdl_t) == PL_MDL_WIRE, "pl_mdl_t: the loader and tools/pick_module.py read it at PL_MDL_WIRE (a device build)");
/* a voice's own (what the module keeps beside its model), and the part: the voice heads, then the model states (PL_POLY of them, or one for a
 * large model), each at m + i * pl_stride(state) */
typedef struct {
    uint8_t rise;                                     /* a note started: the next render is a rising edge */
    float note;                                       /* last note (the module averages two in a row) */
    float accent;
    float decay_env;                                  /* DECAY's envelope (1 at a note, falls) */
    pl_svf_t lpg;
    pl_limiter_t limit_out, limit_aux;
} pl_vhead_t;
typedef struct {                                      /* (no attributes: the host build strips them, and the layout must be the device's, PL_PART_HEAD) */
    uint8_t model;                                    /* the model whose states are in m */
    uint8_t started;                                  /* 0 on a fresh part (the arena is zeroed, so `model` reads 0 = VA there and no model's states
                                                       * have been started): the first render starts the model's, whichever it is (round 18's VA init fix) */
    uint8_t pad_[2];
    pl_vhead_t h[PL_POLY];                            /* ((0.14.3) the 16-frame out and aux buffers are the voice kernel's own, on its stack:
                                                       * one shared by the track's voices would be written by both cores) */
    uint32_t pad2_;                                   /* (m at a multiple of 8: the stride of a model's state is one) */
    uint8_t m[8];                                     /* the model states follow */
} pl_part_t;
_Static_assert(__builtin_offsetof(pl_part_t, m) == PL_PART_HEAD, "PL_PART_HEAD (fw_models.h) is offsetof(pl_part_t, m): the packer and the loader size a pick's state from it");
static inline void *pl_state(pl_part_t *s, const pl_mdl_t *d, uint32_t i) { return s->m + (d->big ? 0u : (i % PL_POLY) * pl_stride(d->state)); }

/* A unit's exports, kept: the symbols another region may name (its descriptor, its bank, the six-op's start and render) go into a pointer array
 * in section .plaits_keep, which the module's link script keeps (KEEP), so the linker's garbage collection keeps the same functions in a unit
 * whether or not the regions that name them are linked (the master link has every region; a pick's link may lack the one that would have kept
 * a symbol alive). Without this the two links differ (tools/build.py --pick 6opb: the bank-0 start would be gone from the six-op region). In the
 * firmware it is nothing. */
#ifdef OC_MODULE
#define PL_KEEP(...) __attribute__((section(".plaits_keep"), used)) static const void *const PL_CAT3(pl_keep_, PL_UNIT, __COUNTER__) [] = { __VA_ARGS__ }
#else
#define PL_KEEP(...)
#endif
/* a model's start and render for `key` of type T through its prefix_init / _reset / _render / _flush, and its descriptor */
#define PL_MDL_FUNCS(key, T, prefix) \
    static PL_MDL_ATTR void pl_mdl_##key##_start(void *m) { prefix##_init((T *)m); prefix##_reset((T *)m); } \
    static PL_MDL_ATTR int pl_mdl_##key##_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n) \
    { int env = 0; prefix##_render((T *)m, q, o, a, n, &env); prefix##_flush((T *)m); return env; }
#define PL_MDL_UNIT(key, T, prefix, big, draws, env, units, out, aux) \
    PL_MDL_FUNCS(key, T, prefix) \
    PL_MDL_LINKAGE const pl_mdl_t pl_mdl_##key = { sizeof(T), big, draws, env, units, out, aux, pl_mdl_##key##_start, pl_mdl_##key##_render }; \
    PL_KEEP(&pl_mdl_##key);
