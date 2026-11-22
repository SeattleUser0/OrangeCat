/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments (Felucca's core.h); Clipper's fields Copyright (C) 2026 3dSam (Clipper).
 * (Orange Cat, this file) Clipper's preset_t (firmware/src/core.h at e690672), renamed cl_preset_t, with its FX, ARP, PAT and XP initializer macros, as
 * they are: a Clipper engine's presets are written in it (the module unit includes the engine with preset_t defined as cl_preset_t). Orange Cat's
 * preset_t has the first six fields only (name, e, env, fenv, mono); the effect sends, the arp, the pattern and the extras (the LFO, the glide, a
 * voice mode the preset sets: track parameters) have no place in it, and Orange Cat's preset_apply sets none of them: cl_preset_make below copies the
 * six into the engine's preset_make (the extras are the gate's "lost" row).
 */
#ifndef CLIPPER_PRESET_H
#define CLIPPER_PRESET_H 1
typedef struct {
    const char *name;
    int8_t e[8];                 /* P_E0..P_E7 (signed: an interval below the note; every value fits) */
    uint8_t env[4];              /* ATK DEC SUS REL */
    int8_t fenv;                 /* ENV -> FILTER amount (-64..63) */
    uint8_t mono;                /* 1 = MONO (bass / lead), 0 = POLY */
    /* the rest of the patch; each value is stored + 1, 0 = the default */
    uint8_t fx[4];               /* DIST, CHORUS, DELAY, REVERB sends */
    uint8_t arp[4];              /* MODE, RATE, OCT, GATE */
    uint8_t pat;                 /* sequence pattern (PATTERNS[pat - 1]), loaded only into an empty sequencer */
    int8_t x[8];                 /* more of the patch: up to 4 (track parameter id + 1, value) pairs, 0 = none
                                  * (glide, pitch / LFO modulation, voice mode...: preset_extras) */
} cl_preset_t;
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define ARP(m, rt, o, g) .arp = {(m) + 1, (rt) + 1, (o) + 1, (g) + 1}
#define PAT(n) .pat = (n)
#define XP(...) .x = {__VA_ARGS__}  /* XP(P_GLIDE + 1, 30, P_ED_PIT + 1, 8): parameter id + 1, value */

/* Clipper's preset k (of the engine's n) as Orange Cat's: its first six fields */
static void cl_preset_make(const cl_preset_t *list, uint32_t n, uint32_t k, preset_t *p, char *name)
{
    const cl_preset_t *c = &list[k < n ? k : 0];
    uint32_t i;
    for (i = 0; i < 8u; i++) p->e[i] = c->e[i];
    for (i = 0; i < 4u; i++) p->env[i] = c->env[i];
    p->fenv = c->fenv;
    p->mono = c->mono;
    for (i = 0; c->name[i] && i < 20u; i++) name[i] = c->name[i];
    name[i] = 0;
    p->name = name;
}
#endif
