/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Orange Cat, the new interface (OC_HOME_PIC; docs/orangecat/UI_DESIGN.md section 2, docs/orangecat/UI_BUILD.md step 1a):
 * the four knobs of HOME for every built-in engine that has a picture, in ONE place, so the owner can change a knob after
 * playing it (the owner's decision 2, October 29, 2026: the table is approved for now and judged by ear).
 *
 * A knob is either PLAIN (one of the engine's parameters, edited as its EDIT page edits it) or a MACRO: a stored offset m,
 * -64 .. 63, in P_MAC0 + k (core.h), 0 the sound as stored, that moves up to four targets. Each target has a shape and a
 * signed depth (oc_macro.c applies them once a block, as PERFORM's throws are applied):
 *     KS_LIN   target = stored + depth x m / 64
 *     KS_UP    target = stored + depth x max(m, 0) / 64      (only the right half of the knob moves it)
 *     KS_DOWN  target = stored + depth x min(m, 0) / 64      (only the left half)
 * each clamped to the target's own range (params.c param_desc_of). A knob never switches what the sound is (a model, a
 * shape, a wave): those are EDIT's and the presets'.
 *
 * To change a knob: edit its row below. The four-knob audit (tests/knob_audit.py) holds the same table for the host render;
 * keep the two in step (tests/oc_macro_test.c checks that they agree). A module (step 7) carries its own table in its pages
 * descriptor; an engine with no table keeps its four EDIT knobs on HOME and shows the plain picture.
 *
 * Included by oc_throw.c (the audio interrupt's side needs the table) before oc_macro.c.
 *
 * Revisions:
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1a) first version: MACRO, SHAPES, DRUM-X as UI_DESIGN.md
 *   section 2's table.
 */
enum { KD_NONE, KD_PLAIN, KD_MACRO };                   /* a knob's kind (KD_NONE: an empty place, nothing turns) */
enum { KS_LIN, KS_UP, KS_DOWN };                        /* a macro target's shape */
#define KD_TGT 4u                                       /* targets a macro moves, at most */
#define KD_NOT 0xFFu                                    /* (an unused target) */
typedef struct { uint8_t id, shape; int8_t depth; } kd_tgt_t;   /* a target: a P_ id, its shape, its depth (signed) */
typedef struct {
    const char *label;                                  /* the knob's name on HOME (the sound line, the value row) */
    uint8_t kind;                                       /* KD_PLAIN, KD_MACRO */
    uint8_t id;                                         /* KD_PLAIN: the parameter it edits (a P_ id) */
    kd_tgt_t t[KD_TGT];                                 /* KD_MACRO: its targets, KD_NOT the unused ones */
} knob_def_t;
#define KD_P(label, id) {label, KD_PLAIN, id, {{KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}}}
#define KD_T(id, shape, depth) {id, shape, depth}
#define KD_T0 {KD_NOT, 0, 0}

/* ---- the table (UI_DESIGN.md section 2; K1 blue, K2 green, K3 white, K4 orange) ---- */
#if MELODEE_MACRO
/* MACRO: HARM, TIMB, MRPH plain; PLUCK: right, the low-pass gate closes in and the internal decay shortens (a pluck, a
 * bonk); left, a slower attack and a longer release (a swell). Audit: weakest 3.1 dB (TIMB on SNARE) */
static const knob_def_t KD_MACRO_ENG[4] = {
    KD_P("HARM", P_E1),
    KD_P("TIMB", P_E2),
    KD_P("MRPH", P_E3),
    {"PLUCK", KD_MACRO, 0, {KD_T(P_E5, KS_UP, 127), KD_T(P_E6, KS_UP, -100), KD_T(P_ATK, KS_DOWN, -90),
                            KD_T(P_REL, KS_DOWN, -90)}},
};
#endif
#if MELODEE_SHAPES
/* SHAPES: TIMB (the timbre over its whole range from the preset, COLR a little with it: some shapes answer TIMB faintly
 * alone); COLR plain; TONE (left, the filter closes; right, resonance and drive come up); PLUCK (as MACRO's, on the ADSR).
 * Audit: weakest 1.7 dB (TIMB on CSAW BASS) */
static const knob_def_t KD_SHAPES_ENG[4] = {
    {"TIMB", KD_MACRO, 0, {KD_T(P_E1, KS_LIN, 127), KD_T(P_E2, KS_LIN, 40), KD_T0, KD_T0}},
    KD_P("COLR", P_E2),
    {"TONE", KD_MACRO, 0, {KD_T(P_E3, KS_DOWN, 100), KD_T(P_E4, KS_UP, 90), KD_T(P_E5, KS_UP, 90), KD_T0}},
    {"PLUCK", KD_MACRO, 0, {KD_T(P_SUS, KS_UP, -127), KD_T(P_DEC, KS_UP, -80), KD_T(P_ATK, KS_DOWN, -90),
                            KD_T(P_REL, KS_DOWN, -90)}},
};
#endif
#if MELODEE_DRUMX
/* DRUM-X: its four HOME knobs of today, plain (the kit as made is their middle, in the engine). Audit: weakest 7.7 dB */
static const knob_def_t KD_DRUMX_ENG[4] = {
    KD_P("MRPH", P_E0),
    KD_P("TUNE", P_E1),
    KD_P("TONE", P_E2),
    KD_P("DECY", P_E3),
};
#endif

/* engine e's four knobs, 0 when it has none (a module, an engine without a picture yet) */
static const knob_def_t *kdef_of(uint32_t e)
{
#ifdef OC_KDEF_TEST_HOOK
    if (OC_KDEF_TEST_HOOK(e)) return OC_KDEF_TEST_HOOK(e);   /* (host tests only: tests/oc_macro_test.c's made tables) */
#endif
#if MELODEE_MACRO
    if (e == ENGI_MACRO) return KD_MACRO_ENG;
#endif
#if MELODEE_SHAPES
    if (e == ENGI_SHAPES) return KD_SHAPES_ENG;         /* (the built-in SHAPES and its module alike: the same engine) */
#endif
#if MELODEE_DRUMX
    if (e == ENGI_DRUMX) return KD_DRUMX_ENG;
#endif
    (void)e;
    return 0;
}
