/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Jangada's ROBO (https://github.com/zednaked/jangada, commit 249eb43: firmware/src/eng_robo.c, GPL-3.0-only; the file carries only its
 * SPDX line: Jangada's author, zednaked; the idea from its issue #4, gospodindilly-ops), the copy in tools/module/jangada/ up to its engine_t.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "The hub's other voices"): ROBO (four small machine voices written
 * after published algorithms: VOSIM's sin^2 pulse bursts (Kaegi and Tempelaars), GENDYN's stochastic breakpoints (Xenakis), Walsh functions in
 * sequency order, scanned synthesis on a ring of 16 masses (Verplank, Mathews, Shaw); then a drift, a crusher, a tone low-pass and a DC blocker) as a
 * loadable module translation unit (stage 0). The proposed engine number is 46 (core.h ENGI_ROBO; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (VOICE: MODE FORM PULS DCAY) and EDIT 2 (MOVE: TRK DRFT CRSH TONE): Jangada's eight, the first four named per MODE by its desc hook
 *     (robo_desc: Orange Cat's engine_t has the same hook);
 *   - presets: Jangada's, in its preset_t (tools/module/jangada/jangada_preset.h), made into Orange Cat's by jg_preset_make (their category, sends,
 *     arp and SET extras dropped);
 *   - GENDY's points, WALSH's wave and SCAN's ring live in the part's arena block (robo_part_t, a voice's each, as Jangada's eng_arena_of gives
 *     it); the module clears it the first time the block is the module's (a magic word: Jangada clears it when the part's engine changes);
 *   - Jangada's rng() (one xorshift for its firmware, seeding a note's walk) is the part's own word (robo_rng);
 *   - the rest of the voice in voice_t; per sample SINE (VOSIM's pulses: the base hot set) and the Walsh table (WALSH: 128 B, read once a block
 *     into the part's wave); no hot set of its own.
 */
#include "clipper/clipper_unit.h"
#undef FX
#undef ARP
#undef PAT
#include "jangada/jangada_preset.h"
#define ENGI_ROBO_J ENGI_ROBO
struct robo_arena;
static void *robo_arena(const track_t *t);
static uint32_t robo_rng(const track_t *t);
#define eng_arena_of(t, e) robo_arena(t)
#define rng() robo_rng(t)                  /* (robo_note_on names its track t) */
#define preset_t jg_preset_t
#include "jangada/eng_robo.c"
#undef preset_t
#undef eng_arena_of
#undef rng
#define ROBO_MAGIC 0x4F424F52u             /* "ROBO" */
typedef struct { robo_part_t p; uint32_t magic, rng; } robo_arena_t;
static void *robo_arena(const track_t *t)
{
    robo_arena_t *a = resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(robo_arena_t));
    if (a && a->magic != ROBO_MAGIC) {     /* (Jangada's eng_arena_of: cleared when the block was another engine's) */
        memset(a, 0, sizeof *a);
        a->magic = ROBO_MAGIC;
        a->rng = 0x1234567u;
    }
    return a ? &a->p : 0;
}
/* Jangada's rng() (its libc.c: one xorshift for the firmware, from 0x1234567): the part's own word here (a module has no RAM) */
static uint32_t robo_rng(const track_t *t)
{
    robo_arena_t *a = (robo_arena_t *)((char *)robo_arena(t) - __builtin_offsetof(robo_arena_t, p));
    a->rng ^= a->rng << 13;
    a->rng ^= a->rng >> 17;
    a->rng ^= a->rng << 5;
    return a->rng;
}
static void robo_preset_make(uint32_t k, preset_t *p, char *name) { jg_preset_make(ROBO_PRESETS, NELEM(ROBO_PRESETS), k, p, name); }
static const engine_t ENG_ROBO = {           /* Jangada's ENG_ROBO in Orange Cat's fields (its fil_page -1, colour 0x9EDD and macro: knob) */
    .name = "ROBO",
    .page_title = {"VOICE", "MOVE"},
    .edit = {
        {"MODE", F_ENUM, 0, RB_COUNT - 1, 0, N_ROBO_MODE, 0},
        {"FORM", F_PCT, 0, 127, 60, 0, 0},
        {"PULS", F_INT, 0, 127, 30, 0, 0},
        {"DCAY", F_PCT, 0, 127, 50, 0, 0},
        {"TRK", F_PCT, 0, 127, 64, 0, 0},
        {"DRFT", F_PCT, 0, 127, 0, 0, 0},
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_CUTOFF, 0, 127, 110, 0, 0},
    },
    .preset_make = robo_preset_make, .npresets = NELEM(ROBO_PRESETS),
    .knob = {P_E1, P_E3, P_E5, P_E7},
    .note_on = robo_note_on, .render = robo_render, .desc = robo_desc,
};
CL_MODULE_HEADER(ENGI_ROBO, ENG_ROBO, sizeof(robo_arena_t))
