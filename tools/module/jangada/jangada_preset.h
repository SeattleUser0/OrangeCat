/* SPDX-License-Identifier: GPL-3.0-only
 * (Orange Cat, this file) Jangada's preset_t (https://github.com/zednaked/jangada, commit 249eb43, firmware/src/core.h: Felucca's core.h by Leo
 * Kuroshita, Hügelton Instruments, with Jangada's fields), renamed jg_preset_t, with its CAT FX ARP PAT SET initializer macros, as they are (NEDIT,
 * its engine-parameter count, is 16 there). A Jangada engine's presets are written in it (tools/module/robo_mod.c includes the engine with preset_t
 * defined as jg_preset_t). Orange Cat's preset_t has the first six fields only: jg_preset_make copies them (the category, the sends, the arp, the
 * pattern, the parameters past P_E7 and the SET extras are the gate's "lost" row).
 */
#ifndef JANGADA_PRESET_H
#define JANGADA_PRESET_H 1
#define JG_NEDIT 16
enum { PC_NONE, PC_BASS, PC_LEAD, PC_PAD, PC_KEYS, PC_PLUCK, PC_PERC, PC_DRONE, PC_FX, PC_COUNT };
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
    int16_t x[JG_NEDIT - 8];     /* Jangada: P_E8.. stored + 1, 0 = the engine's default; last, so the
                                  * positional initializers above stay as they are (.x = {..}) */
    const int16_t (*set)[2];     /* Jangada: any other parameters, {P_*, value} .. {-1}: SET(..) */
    uint8_t cat;                 /* Jangada 0.8.1 (issue #2): the category, PC_* (CAT(..)); the PRESETS page filters by it */
} jg_preset_t;
#define CAT(c) .cat = PC_##c
#define FX(d, c, dl, r) .fx = {(d) + 1, (c) + 1, (dl) + 1, (r) + 1}
#define ARP(m, rt, o, g) .arp = {(m) + 1, (rt) + 1, (o) + 1, (g) + 1}
#define PAT(n) .pat = (n)
#define SET(...) .set = 0             /* (Orange Cat) the extras name Jangada's track parameters; dropped (jg_preset_make) */
static void jg_preset_make(const jg_preset_t *list, uint32_t n, uint32_t k, preset_t *p, char *name)
{
    const jg_preset_t *c = &list[k < n ? k : 0];
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
