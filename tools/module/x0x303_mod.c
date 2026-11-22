/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: X0X's TB-303 bass engine (https://github.com/charlesvestal/fm1-x0x, commit 54194c5: firmware/src/dsp/bass303.c and bass303.h,
 * GPL-3.0-only; Charles Vestal's port of Open303 (Robin Schmidt, MIT) with schwung-303's Devilfish ranges and Soft / RAT drive (GPL-3.0; the RAT
 * from davemollen/dm-Rat, GPL-3.0)), unchanged copies in tools/module/x0x/; X0X's fastmath.h and x0x_param.h are Orange Cat's already
 * (firmware/src/x0x/, from X0X through Melodee).
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"): X0X's 303 (Open303's signal path: the
 * saw or the transistor-shaped square, the TeeBeeFilter with its feedback high-pass, the decay envelope's RC followers, the accent and its decay,
 * the slide, the de-clicker and Open303's output filters; 2x oversampled; float) as a loadable module translation unit (stage 0). The proposed
 * engine number is 47 (core.h ENGI_X0X303; registrable only in an OC_CANDIDATES build). bass303.c is self-contained (no mutable static): one
 * bass303_t a part, in the part's arena block.
 *   - EDIT 1 (VCF: CUT RES ENV DEC), EDIT 2 (303: ACC WAVE DRIV SLID): eight of X0X's twelve pots. Fixed at X0X's defaults: TUNE 64 (A4 440: Orange
 *     Cat's tuning is not applied: the 303 figures its own pitch from the key), VOLUME 96, DRVTYP SOFT, ACCDEC 7;
 *   - notes: one voice (poly 1); a note is bass303_note_on with its key, the accent at velocity >= 120 (as X0X's sequencer's ACCENT steps), and a
 *     legato note a slide (bass303_note_on(.., slide 1) from the engine's legato hook: the 303 slides without a retrigger); the key up is
 *     bass303_note_off (at the first render after the gate fell);
 *   - its own envelopes (ownenv): the voice ends when the 303 is idle (done); voice.c's amplitude still brings the LFO on AMP and the fades;
 *   - the 303's own pitch: voice.c's glide, bend, LFO on the pitch and microtonal scale do not reach it (the gate's "modulation" row).
 */
#include "clipper/clipper_unit.h"
#define blep bass303_blep                /* (bass303.c's own float polyBLEP; Orange Cat's dsp.c has a fixed-point one by that name) */
#include "x0x/bass303.c"
#undef blep
typedef struct {
    bass303_t b;
    uint8_t gate;                          /* the key as the module last saw it (note_off) */
    uint32_t magic;
} x0x303_part_t;
#define X0X303_MAGIC 0x33303358u           /* "X303" */
static x0x303_part_t *x0x303_part(const track_t *t)
{
    x0x303_part_t *P = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(x0x303_part_t));
    if (P && P->magic != X0X303_MAGIC) {
        bass303_init(&P->b);
        P->gate = 0;
        P->magic = X0X303_MAGIC;
    }
    return P;
}
/* the eight controls into X0X's pots (bass303_set recomputes what a pot sets: only on a change) */
static void x0x303_pots(const track_t *t, bass303_t *b)
{
    static const uint8_t POT[8] = {BASS303_CUTOFF, BASS303_RESO, BASS303_ENVMOD, BASS303_DECAY, BASS303_ACCENT, BASS303_WAVE, BASS303_DRIVE, BASS303_SLIDE};
    uint32_t k;
    for (k = 0; k < 8u; k++) {
        int v = clamp(t->p[P_E0 + k], 0, bass303_param(POT[k])->max);
        if (bass303_get(b, POT[k]) != v)
            bass303_set(b, POT[k], v);
    }
}
static void x0x303_note_on(track_t *t, voice_t *v)
{
    x0x303_part_t *P = x0x303_part(t);
    if (!P) return;
    x0x303_pots(t, &P->b);
    bass303_note_on(&P->b, v->note, v->vel >= 120u, 0);
    P->gate = 1;
}
static void x0x303_legato(track_t *t, voice_t *v)
{
    x0x303_part_t *P = x0x303_part(t);
    if (!P) return;
    bass303_note_on(&P->b, v->note, v->vel >= 120u, 1);
    P->gate = 1;
}
static void x0x303_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    x0x303_part_t *P = x0x303_part(t);
    float buf[CTL];
    uint32_t i;
    if (!P || n > CTL) return;
    if (P->gate && !v->gate) {
        bass303_note_off(&P->b);
        P->gate = 0;
    }
    x0x303_pots(t, &P->b);
    bass303_render(&P->b, buf, (int)n);
    for (i = 0; i < n; i++)
        out[i] += voice_amp((int32_t)(fm_clampf(buf[i], -1.0f, 1.0f) * 32767.0f), m, i);
}
static int x0x303_done(track_t *t, voice_t *v)
{
    x0x303_part_t *P = x0x303_part(t);
    return !v->gate && (!P || P->b.idle);
}
static const preset_t X0X303_PRESETS[] = {
    /* {CUT RES ENV DEC, ACC WAVE DRIV SLID}, {A D S R} (unused: its own envelopes), ENV -> FILTER, mono */
    {"303", {64, 64, 64, 64, 64, 0, 0, 21}, {0, 64, 127, 10}, 0, 1},                 /* X0X's pot defaults */
    {"SQUELCH", {40, 115, 100, 50, 110, 0, 40, 21}, {0, 64, 127, 10}, 0, 1},
    {"SQR ACID", {50, 90, 70, 70, 80, 1, 20, 30}, {0, 64, 127, 10}, 0, 1},
    {"RAT 303", {30, 110, 90, 40, 120, 0, 90, 21}, {0, 64, 127, 10}, 0, 1},
};
static const engine_t ENG_X0X303 = {
    .name = "X0X303", .page_title = {"VCF", "303"},
    .edit = {
        {"CUT", F_PCT, 0, 127, 64, 0, 0},
        {"RES", F_PCT, 0, 127, 64, 0, 0},
        {"ENV", F_PCT, 0, 127, 64, 0, 0},
        {"DEC", F_PCT, 0, 127, 64, 0, 0},
        {"ACC", F_PCT, 0, 127, 64, 0, 0},
        {"WAVE", F_INT, 0, 1, 0, 0, 0},
        {"DRIV", F_PCT, 0, 127, 0, 0, 0},
        {"SLID", F_PCT, 0, 127, 21, 0, 0},
    },
    .presets = X0X303_PRESETS, .npresets = NELEM(X0X303_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E3}, .poly = 1, .ownenv = 1,
    .note_on = x0x303_note_on, .render = x0x303_render, .legato = x0x303_legato, .done = x0x303_done,
};
CL_MODULE_HEADER(ENGI_X0X303, ENG_X0X303, sizeof(x0x303_part_t))
