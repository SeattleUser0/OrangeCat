/* SPDX-License-Identifier: MIT
 * Copyright 2016-2021 Emilie Gillet (Mutable Instruments Plaits). MIT License: LICENSES/MIT-Mutable.txt
 * C port for Orange Cat: Copyright 2026 Jeremiah Conner. Same license.
 *
 * Orange Cat MACRO2 candidate: the six-operator FM voice of Plaits 1.2's six-op engine (pl_sixop.h), in C.
 *   plaits/dsp/fm/operator.h      pl_dx_operator_t (fm::Operator), pl_dx_render_operators (RenderOperators<n,
 *                                 modulation_source, additive>: the three template arguments are plain arguments)
 *   plaits/dsp/fm/algorithms.h/.cc pl_dx_algorithms_t (Algorithms<6>): the 32 DX7 algorithms as opcodes
 *                                 (pl_dx_opcodes), compiled at init into render calls, as the C++. A render call
 *                                 names its renderer by (n, modulation_source, additive) instead of a function
 *                                 pointer (five bytes where the C++ has sixteen: 960 B for the table, not 3 KB).
 *   plaits/dsp/fm/voice.h         pl_dx_voice_t (fm::Voice<6>), pl_dx_voice_render
 * Every float operation in the order of the C++.
 *
 * Deliberate differences from the C++ (Orange Cat): the voice holds its patch unpacked (pl_dx_patch_t, 156 bytes)
 * where the C++ keeps a pointer into a bank of 32 unpacked patches; pl_dx_voice_set_patch takes the 128 packed
 * bytes and unpacks them there (Patch::Unpack is deterministic, so the voice sees the same patch). The algorithms
 * are passed to the render instead of being a pointer in the voice.
 */
#pragma once
#include "pl_dx_units.h"

/* ------------------------------------------------------- operator.h --- */
enum { PL_DX_MODULATION_SOURCE_EXTERNAL = -2, PL_DX_MODULATION_SOURCE_NONE = -1, PL_DX_MODULATION_SOURCE_FEEDBACK = 0 };
typedef struct { uint32_t phase; float amplitude; } pl_dx_operator_t;
static inline void pl_dx_operator_reset(pl_dx_operator_t *o) { o->phase = 0; o->amplitude = 0.0f; }

#define PL_DX_MAX_CHAIN 3                       /* the longest chain of operators one renderer renders (n) */
/* RenderOperators<n, modulation_source, additive>(ops, f, a, fb_state, fb_amount, modulation, out, size): n operators in
 * a chain, each phase-modulating the next; the first one by its feedback (modulation_source >= 0: the index in the
 * chain of the operator fed back), an external buffer (-2) or nothing (-1). The last one's output is written or
 * added (additive) to out. */
static void pl_dx_render_operators(int n, int modulation_source, int additive, pl_dx_operator_t *ops, const float *f,
                                   const float *a, float *fb_state, int fb_amount, const float *modulation, float *out, size_t size)
{
    float previous_0 = 0.0f, previous_1 = 0.0f;
    if (modulation_source >= PL_DX_MODULATION_SOURCE_FEEDBACK) {
        previous_0 = fb_state[0];
        previous_1 = fb_state[1];
    }
    uint32_t frequency[PL_DX_MAX_CHAIN];
    uint32_t phase[PL_DX_MAX_CHAIN];
    float amplitude[PL_DX_MAX_CHAIN];
    float amplitude_increment[PL_DX_MAX_CHAIN];
    const float scale = 1.0f / (float)size;
    for (int i = 0; i < n; ++i) {
        frequency[i] = (uint32_t)(pl_minf(f[i], 0.5f) * 4294967296.0f);
        phase[i] = ops[i].phase;
        amplitude[i] = ops[i].amplitude;
        amplitude_increment[i] = (pl_minf(a[i], 4.0f) - amplitude[i]) * scale;
    }
    const float fb_scale = fb_amount ? (float)(1 << fb_amount) / 512.0f : 0.0f;
    while (size--) {
        float pm = 0.0f;
        if (modulation_source >= PL_DX_MODULATION_SOURCE_FEEDBACK) pm = (previous_0 + previous_1) * fb_scale;
        else if (modulation_source == PL_DX_MODULATION_SOURCE_EXTERNAL) pm = *modulation++;
        for (int i = 0; i < n; ++i) {
            phase[i] += frequency[i];
            pm = pl_sine_pm(phase[i], pm) * amplitude[i];
            amplitude[i] += amplitude_increment[i];
            if (i == modulation_source) {
                previous_1 = previous_0;
                previous_0 = pm;
            }
        }
        if (additive) *out++ += pm;
        else *out++ = pm;
    }
    for (int i = 0; i < n; ++i) {
        ops[i].phase = phase[i];
        ops[i].amplitude = amplitude[i];
    }
    if (modulation_source >= PL_DX_MODULATION_SOURCE_FEEDBACK) {
        fb_state[0] = previous_0;
        fb_state[1] = previous_1;
    }
}

/* ----------------------------------------------------- algorithms.h --- */
#define PL_DX_NUM_ALGORITHMS 32                 /* NumAlgorithms<6>::N */
enum {                                          /* OpcodeFlags */
    PL_DX_DESTINATION_MASK = 0x03, PL_DX_SOURCE_MASK = 0x30, PL_DX_SOURCE_FEEDBACK = 0x30,
    PL_DX_ADDITIVE_FLAG = 0x04, PL_DX_FEEDBACK_SOURCE_FLAG = 0x40
};
/* RenderCall: the renderer as (n, modulation_source, additive) where the C++ has a pointer to the instantiation */
typedef struct { int8_t n, modulation_source, additive, input_index, output_index; } pl_dx_render_call_t;
typedef struct { pl_dx_render_call_t render_call[PL_DX_NUM_ALGORITHMS][PL_DX_NUM_OPERATORS]; } pl_dx_algorithms_t;

/* algorithms.cc: for each operator, from where it takes its phase modulation and to which buffer it writes */
#define PL_DX_MOD(n) ((n) << 4)
#define PL_DX_ADD(n) ((n) | PL_DX_ADDITIVE_FLAG)
#define PL_DX_OUT(n) (n)
#define PL_DX_FB_SRC PL_DX_FEEDBACK_SOURCE_FLAG
#define PL_DX_FB_DST PL_DX_MOD(3)
#define PL_DX_FB (PL_DX_FB_SRC | PL_DX_FB_DST)
#define PL_DX_NO_MOD PL_DX_MOD(0)
#define PL_DX_OUTPUT PL_DX_ADD(0)
static const uint8_t pl_dx_opcodes[PL_DX_NUM_ALGORITHMS][PL_DX_NUM_OPERATORS] = {   /* Algorithms<6>::opcodes_ (from DX7) */
    /* 1 */  { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 2 */  { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 3 */  { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 4 */  { PL_DX_FB_DST | PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_FB_SRC | PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 5 */  { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 6 */  { PL_DX_FB_DST | PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_FB_SRC | PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 7 */  { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 8 */  { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_FB | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 9 */  { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 10 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 11 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 12 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 13 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 14 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 15 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 16 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_OUT(2), PL_DX_MOD(2) | PL_DX_ADD(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT },
    /* 17 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_OUT(2), PL_DX_MOD(2) | PL_DX_ADD(1), PL_DX_FB | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT },
    /* 18 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_FB | PL_DX_ADD(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT },
    /* 19 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 20 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 21 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 22 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 23 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 24 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 25 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 26 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 27 */ { PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_NO_MOD | PL_DX_ADD(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 28 */ { PL_DX_NO_MOD | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0) },
    /* 29 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 30 */ { PL_DX_NO_MOD | PL_DX_OUTPUT, PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 31 */ { PL_DX_FB | PL_DX_OUT(1), PL_DX_MOD(1) | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) },
    /* 32 */ { PL_DX_FB | PL_DX_OUTPUT, PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0), PL_DX_NO_MOD | PL_DX_ADD(0) }
};
#undef PL_DX_MOD
#undef PL_DX_ADD
#undef PL_DX_OUT
#undef PL_DX_FB_SRC
#undef PL_DX_FB_DST
#undef PL_DX_FB
#undef PL_DX_NO_MOD
#undef PL_DX_OUTPUT

/* Algorithms<6>::renderers_: the RenderOperators<> instantiations the C++ has (the core six, and the two feedback
 * loops spanning several operators); GetRenderer becomes "is there one" (pl_dx_render_operators takes the triple) */
typedef struct { int8_t n, modulation_source, additive; } pl_dx_renderer_spec_t;
static const pl_dx_renderer_spec_t pl_dx_renderers[] = {
    { 1, -2, 0 }, { 1, -2, 1 }, { 1, -1, 0 }, { 1, -1, 1 }, { 1, 0, 0 }, { 1, 0, 1 },
    { 3, 2, 1 }, { 2, 1, 1 },
    { 0, 0, 0 }
};
static inline int pl_dx_has_renderer(int n, int modulation_source, int additive)
{
    for (const pl_dx_renderer_spec_t *r = pl_dx_renderers; r->n; ++r)
        if (r->n == n && r->modulation_source == modulation_source && r->additive == additive) return 1;
    return 0;
}
/* Algorithms::Compile(algorithm): the opcodes to a sequence of render calls */
static void pl_dx_algorithms_compile(pl_dx_algorithms_t *alg, int algorithm)
{
    const uint8_t *opcodes = pl_dx_opcodes[algorithm];
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ) {
        uint8_t opcode = opcodes[i];
        int n = 1;
        while (i + n < PL_DX_NUM_OPERATORS) {
            uint8_t from = opcodes[i + n - 1];
            uint8_t to = (opcodes[i + n] & PL_DX_SOURCE_MASK) >> 4;
            int has_additive = from & PL_DX_ADDITIVE_FLAG;
            int broken = (from & PL_DX_DESTINATION_MASK) != to;
            if (has_additive || broken) {
                /* if the same modulation happens to be reused by subsequent operators (algorithms 19 to 25), discard the chain */
                if (to == (opcode & PL_DX_DESTINATION_MASK)) n = 1;
                break;
            }
            ++n;
        }
        /* try to find if a pre-compiled renderer is available for this chain */
        for (int attempt = 0; attempt < 2; ++attempt) {
            uint8_t out_opcode = opcodes[i + n - 1];
            int additive = (out_opcode & PL_DX_ADDITIVE_FLAG) != 0;
            int modulation_source = -3;
            if (!(opcode & PL_DX_SOURCE_MASK)) {
                modulation_source = -1;
            } else if ((opcode & PL_DX_SOURCE_MASK) != PL_DX_SOURCE_FEEDBACK) {
                modulation_source = -2;
            } else {
                for (int j = 0; j < n; ++j)
                    if (opcodes[i + j] & PL_DX_FEEDBACK_SOURCE_FLAG) modulation_source = j;
            }
            if (pl_dx_has_renderer(n, modulation_source, additive)) {
                pl_dx_render_call_t *call = &alg->render_call[algorithm][i];
                call->n = (int8_t)n;
                call->modulation_source = (int8_t)modulation_source;
                call->additive = (int8_t)additive;
                call->input_index = (int8_t)((opcode & PL_DX_SOURCE_MASK) >> 4);
                call->output_index = (int8_t)(out_opcode & PL_DX_DESTINATION_MASK);
                break;
            } else {
                if (n == 1) { /* assert(false) */ } else n = 1;
            }
        }
        i += n;
    }
}
/* Algorithms::Init. (The entries inside a chain, which the C++ leaves unset and never reads, are zeroed first.) */
static void pl_dx_algorithms_init(pl_dx_algorithms_t *alg)
{
    for (int i = 0; i < PL_DX_NUM_ALGORITHMS; ++i)
        for (int j = 0; j < PL_DX_NUM_OPERATORS; ++j) {
            pl_dx_render_call_t *call = &alg->render_call[i][j];
            call->n = call->modulation_source = call->additive = call->input_index = call->output_index = 0;
        }
    for (int i = 0; i < PL_DX_NUM_ALGORITHMS; ++i) pl_dx_algorithms_compile(alg, i);
}
static inline int pl_dx_is_modulator(int algorithm, int op) { return (pl_dx_opcodes[algorithm][op] & PL_DX_DESTINATION_MASK) != 0; }

/* ---------------------------------------------------------- voice.h --- */
typedef struct {                                /* Voice<6>::Parameters */
    int sustain, gate;
    float note, velocity, brightness, envelope_control, pitch_mod, amp_mod;
} pl_dx_voice_params_t;
typedef struct {                                /* fm::Voice<6> */
    float sample_rate, one_hz, a0;
    int gate;
    pl_dx_operator_t op[PL_DX_NUM_OPERATORS];
    pl_dx_env_t operator_envelope[PL_DX_NUM_OPERATORS];
    pl_dx_env_t pitch_envelope;
    float normalized_velocity, note;
    float ratios[PL_DX_NUM_OPERATORS], level_headroom[PL_DX_NUM_OPERATORS], level[PL_DX_NUM_OPERATORS];
    float feedback_state[2];
    pl_dx_patch_t patch;                        /* (Orange Cat) the patch itself, unpacked, where the C++ has a pointer */
    int dirty;
} pl_dx_voice_t;

static void pl_dx_voice_init(pl_dx_voice_t *v, float sample_rate)
{
    v->sample_rate = sample_rate;
    v->one_hz = 1.0f / sample_rate;
    v->a0 = 55.0f / sample_rate;
    const float native_sr = 44100.0f;           /* legacy sample rate */
    const float envelope_scale = native_sr * v->one_hz;
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ++i) {
        pl_dx_operator_reset(&v->op[i]);
        pl_dx_env_init(&v->operator_envelope[i], envelope_scale);
    }
    pl_dx_env_init(&v->pitch_envelope, envelope_scale);
    v->feedback_state[0] = v->feedback_state[1] = 0.0f;
    /* (patch_ = NULL: the unpacked patch's bytes are zeroed; it is never read before pl_dx_voice_set_patch) */
    uint8_t *bytes = (uint8_t *)&v->patch;
    for (size_t i = 0; i < sizeof(v->patch); ++i) bytes[i] = 0;
    v->gate = 0;
    v->note = 48.0f;
    v->normalized_velocity = 10.0f;
    v->dirty = 1;
}
/* SetPatch, given the 128 packed bytes of the patch (Orange Cat: unpacked here, into the voice) */
static inline void pl_dx_voice_set_patch(pl_dx_voice_t *v, const uint8_t *packed)
{
    pl_dx_patch_unpack(&v->patch, packed);
    v->dirty = 1;
}
/* Setup: everything that can be pre-computed once a patch is loaded (envelope constants, frequency ratios) */
static int pl_dx_voice_setup(pl_dx_voice_t *v)
{
    if (!v->dirty) return 0;
    pl_dx_pitch_env_set(&v->pitch_envelope, v->patch.pitch_envelope.rate, v->patch.pitch_envelope.level);
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ++i) {
        const pl_dx_patch_operator_t *op = &v->patch.op[i];
        int level = pl_dx_operator_level(op->level);
        pl_dx_op_env_set(&v->operator_envelope[i], op->envelope.rate, op->envelope.level, (uint8_t)level);
        /* the level increase caused by keyboard scaling plus velocity scaling should not exceed this number */
        v->level_headroom[i] = (float)(127 - level);
        /* pre-compute frequency ratios; the base frequency (1 Hz or the root note) as the sign of the ratio */
        float sign = op->mode == 0 ? 1.0f : -1.0f;
        v->ratios[i] = sign * pl_dx_frequency_ratio(op);
    }
    v->dirty = 0;
    return 1;
}
static inline float pl_dx_voice_op_level(const pl_dx_voice_t *v, int i) { return v->level[i]; }

/* Render(parameters, buffers[4], size) */
static void pl_dx_voice_render(pl_dx_voice_t *v, const pl_dx_algorithms_t *algorithms, const pl_dx_voice_params_t *parameters,
                               float *buffers[4], size_t size)
{
    if (pl_dx_voice_setup(v)) {
        /* (the C++: not enough CPU for a patch setup and a full render in one block: a clean 0.5 ms blank instead) */
        return;
    }
    const float envelope_rate = (float)size;
    const float ad_scale = pl_dx_pow2_fast(1, (0.5f - parameters->envelope_control) * 8.0f);
    const float r_scale = pl_dx_pow2_fast(1, -pl_fabsf(parameters->envelope_control - 0.3f) * 8.0f);
    const float gate_duration = 1.5f * v->sample_rate;
    const float envelope_sample = gate_duration * parameters->envelope_control;

    /* apply LFO and pitch envelope modulations */
    const float pitch_envelope = parameters->sustain
        ? pl_dx_env_render_at_sample(&v->pitch_envelope, 0, envelope_sample, gate_duration)
        : pl_dx_env_render(&v->pitch_envelope, 0, parameters->gate, envelope_rate, ad_scale, r_scale);
    const float pitch_mod = pitch_envelope + parameters->pitch_mod;
    const float f0 = v->a0 * 0.25f * pl_semitones_to_ratio_safe(parameters->note - 9.0f + pitch_mod * 12.0f);

    /* sample the note and velocity (used for scaling) only on a trigger, or constantly when free-running */
    const int note_on = parameters->gate && !v->gate;
    v->gate = parameters->gate;
    if (note_on || parameters->sustain) {
        v->normalized_velocity = pl_dx_normalize_velocity(parameters->velocity);
        v->note = parameters->note;
    }
    /* reset operator phase if a note on is detected and the patch requires it */
    if (note_on && v->patch.reset_phase)
        for (int i = 0; i < PL_DX_NUM_OPERATORS; ++i) v->op[i].phase = 0;

    /* compute frequencies and amplitudes */
    float f[PL_DX_NUM_OPERATORS];
    float a[PL_DX_NUM_OPERATORS];
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ++i) {
        const pl_dx_patch_operator_t *op = &v->patch.op[i];
        f[i] = v->ratios[i] * (v->ratios[i] < 0.0f ? -v->one_hz : f0);
        const float rate_scaling = pl_dx_rate_scaling(v->note, op->rate_scaling);
        float level = parameters->sustain
            ? pl_dx_env_render_at_sample(&v->operator_envelope[i], 1, envelope_sample, gate_duration)
            : pl_dx_env_render(&v->operator_envelope[i], 1, parameters->gate, envelope_rate * rate_scaling, ad_scale, r_scale);
        const float kb_scaling = pl_dx_keyboard_scaling(v->note, &op->keyboard_scaling);
        const float velocity_scaling = v->normalized_velocity * (float)op->velocity_sensitivity;
        const float brightness = pl_dx_is_modulator(v->patch.algorithm, i) ? (parameters->brightness - 0.5f) * 32.0f : 0.0f;
        level += 0.125f * pl_minf(kb_scaling + velocity_scaling + brightness, v->level_headroom[i]);
        v->level[i] = level;
        const float sensitivity = pl_dx_amp_mod_sensitivity(op->amp_mod_sensitivity);
        /* (not FAST_OP_LEVEL_MODULATION: the exponential formula, to match Dexed's output) */
        const float log_level_mod = sensitivity * parameters->amp_mod - 1.0f;
        const float level_mod = 1.0f - pl_dx_pow2_fast(2, 6.4f * log_level_mod);
        a[i] = pl_dx_pow2_fast(2, -14.0f + level * level_mod);
    }
    for (int i = 0; i < PL_DX_NUM_OPERATORS; ) {
        const pl_dx_render_call_t *call = &algorithms->render_call[v->patch.algorithm][i];
        pl_dx_render_operators(call->n, call->modulation_source, call->additive, &v->op[i], &f[i], &a[i],
                               v->feedback_state, v->patch.feedback, buffers[call->input_index], buffers[call->output_index], size);
        i += call->n;
    }
}
/* Render(parameters, temp, size): the buffers are temp's out, aux and scratch, size floats each (3 x size in all) */
static inline void pl_dx_voice_render_temp(pl_dx_voice_t *v, const pl_dx_algorithms_t *algorithms, const pl_dx_voice_params_t *parameters,
                                           float *temp, size_t size)
{
    float *buffers[4] = { temp, temp + size, temp + 2 * size, temp + 2 * size };
    pl_dx_voice_render(v, algorithms, parameters, buffers, size);
}
