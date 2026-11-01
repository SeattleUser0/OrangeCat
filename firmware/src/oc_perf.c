/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat, Release 3 (branch r3-format), Part 7: PERFORMANCE MODE (docs/orangecat/PROJECT_FORMAT_R3.md section 7; UI_BUILD.md "Part 7"). Behind OC_R3_SCENES. Included by
 * melodee.c before ui_input.c (the input and the layer code use the mode); the scenes' data (oc_scenes2.c) and their store and recall (oc_scenes2_run.c) come with
 * project.c. Revisions:
 *   Part 7 (November 1, 2026): first version. */
#if OC_R3_SCENES
#define SCN2_N 16u
#define SCN2_MAXSND 16u
static uint8_t scn2_missing_track, scn2_skipped_sounds;   /* the last scene recall's notes for its message (oc_lists.c sets them) */
static int ocl_go_at(uint32_t k, uint32_t c, uint32_t e, uint32_t row, uint32_t reps, int is_song);
/* ------------------------------------------------------------------------------------------------------- PERFORMANCE MODE --- */
/* PERF (a record section): version u8 (1), then 64 entries of 4 bytes by control id (a key 0..26, a pad 32..47 later): kind, target, flags, boundary.
 * kind 0 none, 1 phrase (target a phrase id 1..255, flags bits 1..2 the track), 2 chain (target 0..63, the track in flags), 3 scene (target 0..15); flags bit 0 latch
 * (else one-shot); boundary 0 the project's, 1 LOOP, 2 BAR, 3 BEAT, 4 NOW. A version this firmware does not know is carried whole and the mode is empty. */
#define PERF_N 64u
#define PERF_BYTES (1u + PERF_N * 4u)
enum { PK_NONE, PK_PHRASE, PK_CHAIN, PK_SCENE };
typedef struct { uint8_t kind, target, flags, boundary; } perf_ent_t;
static perf_ent_t perf_tab[PERF_N] __attribute__((section(".pool")));
static uint8_t perf_foreign;                   /* the record's PERF is of a version not read: carried, the mode is empty */
static volatile uint8_t kb_perf;               /* Performance Mode is on (double tap on the ARP layer); the page buttons, SAVE, EDIT, the menus and REC are inert */
/* the default of a new or converted project: the 8 white keys F3 to F4 launch scenes 1..8 (OCT+ held: 9..16), as the PERFORM page's do */
static void perf_defaults(void)
{
    memset(perf_tab, 0, sizeof perf_tab);
    static const uint8_t WHITE8[8] = {0, 2, 4, 6, 7, 9, 11, 12};      /* the key ids of F3 G3 A3 B3 C4 D4 E4 F4 */
    for (uint32_t i = 0; i < 8u; i++) { perf_tab[WHITE8[i]].kind = PK_SCENE; perf_tab[WHITE8[i]].target = (uint8_t)i; }
    perf_foreign = 0;
}
static int perf_ent_ok(const uint8_t *e)
{
    if (e[3] > 4u) return 0;
    switch (e[0]) {
    case PK_NONE: return 1;
    case PK_PHRASE: return e[1] >= 1u && e[1] <= OCL_NPOOL && (e[2] & ~7u) == 0;
    case PK_CHAIN: return e[1] < OCL_NCHAIN && ((e[2] >> 1) & 3u) < NTRK && (e[2] & ~7u) == 0;
    case PK_SCENE: return e[1] < SCN2_N;
    default: return 0;
    }
}
static void perf_encode(uint8_t *b) { b[0] = 1; for (uint32_t i = 0; i < PERF_N; i++) { b[1 + 4 * i] = perf_tab[i].kind; b[2 + 4 * i] = perf_tab[i].target; b[3 + 4 * i] = perf_tab[i].flags; b[4 + 4 * i] = perf_tab[i].boundary; } }
/* 0 read (the table is the record's); -1 foreign or hostile: carried, the table is empty */
static int perf_decode(const uint8_t *b, uint32_t n)
{
    if (n < 1u || b[0] != 1u || n < PERF_BYTES) { memset(perf_tab, 0, sizeof perf_tab); perf_foreign = 1; return -1; }
    for (uint32_t i = 0; i < PERF_N; i++) if (!perf_ent_ok(b + 1 + 4 * i)) { memset(perf_tab, 0, sizeof perf_tab); perf_foreign = 1; return -1; }
    for (uint32_t i = 0; i < PERF_N; i++) { perf_tab[i].kind = b[1 + 4 * i]; perf_tab[i].target = b[2 + 4 * i]; perf_tab[i].flags = b[3 + 4 * i]; perf_tab[i].boundary = b[4 + 4 * i]; }
    perf_foreign = 0;
    return 0;
}
/* the buttons Performance Mode keeps live: PLAY (stops as it does), OCT- and OCT+ (scenes 9..16), the layer's own button (leaves the mode); the rest are inert.
 * A bit mask by button label. */
static uint32_t perf_inert_labels(void)
{
    return 1u << B_FX | 1u << B_SCL | 1u << B_ENV | 1u << B_LFO | 1u << B_EDIT | 1u << B_GLO | 1u << B_HOME | 1u << B_SAVE | 1u << B_SEQ | 1u << B_REC;
}
static uint32_t perf_inert_bits(void)
{
    uint32_t m = 0, l = perf_inert_labels();
    for (uint32_t b = 0; b < NB; b++) if ((l >> b) & 1u) m |= 1u << panel.btn[b];
    return m;
}
static int perf_inert(uint32_t label) { return kb_perf && ((perf_inert_labels() >> label) & 1u); }
/* a key pressed in the mode: the entry it launches, or 0 (an unassigned key is silent, never a note) */
static const perf_ent_t *perf_key(uint32_t key, uint32_t oct_held)
{
    static perf_ent_t alt;
    const perf_ent_t *e;
    if (key >= PERF_N) return 0;
    e = &perf_tab[key];
    if (e->kind == PK_SCENE && oct_held && e->target < 8u) { alt = *e; alt.target = (uint8_t)(e->target + 8u); e = &alt; }   /* (OCT+ held: scenes 9..16) */
    return e->kind == PK_NONE ? 0 : e;
}

/* ------------------------------------------------------------------------------------------------------- THE PRE-LOAD PLANNER --- */
/* On entering the mode, and when a scene or chain launch changes what is assigned: the phrases of the assigned keys are read into banks ahead of any press,
 * one a UI frame, in key order. The planner is the decision, with no I/O: given the table and what each track's banks hold, which key to read next.
 *   at most PRE_PER_TRACK phrases a track stay pre-loaded (its banks); the next is on demand
 *   it stops when fewer than PRE_FLOOR timed notes would remain free once the phrase is in (the shared store of RECORD_MAX notes)
 * `notes_free` is the free timed notes now, `phrase_notes` the notes the phrase would add (0 when unknown/none). A key whose phrase did not fit is dim and launches by
 * the on-demand read (the next boundary). Returns the key (0..PERF_N-1) to read now, or -1 when nothing is left to pre-load; *dim has a bit a key that will not be. */
#define PRE_FLOOR 256u
#define PRE_PER_TRACK 6u                       /* (not 8: eviction skips pinned banks, a track's current and next bank are never victims, so 8 pinned leave an on-demand launch no bank) */
typedef struct {
    uint64_t done;                              /* a bit a key already pre-loaded */
    uint64_t dim;                               /* a bit a key left on demand (the floor, or the track's 8) */
    uint8_t per_track[NTRK];
    uint8_t pinned[NTRK];                       /* a bit a bank (8): pre-loaded, clean and idle: eviction skips it */
} perf_pre_t;
static void perf_pre_begin(perf_pre_t *p) { memset(p, 0, sizeof *p); }
/* the track and the phrase a key's entry names: 1 when it has one (phrase and chain entries); a scene's phrases are read when it is pressed */
static int perf_pre_target(const perf_ent_t *e, uint32_t *track, uint32_t *phrase)
{
    if (e->kind == PK_PHRASE) { *track = (e->flags >> 1) & 3u; *phrase = e->target; return *track < NTRK; }
    if (e->kind == PK_CHAIN && e->target < OCL_NCHAIN) { *track = (e->flags >> 1) & 3u; *phrase = lists.chain[e->target][0][0]; return *track < NTRK && *phrase != 0u; }
    return 0;
}
static int perf_pre_next(perf_pre_t *p, uint32_t notes_free, uint32_t (*phrase_notes)(uint32_t phrase), uint32_t *track, uint32_t *phrase)
{
    for (uint32_t key = 0; key < PERF_N; key++) {
        uint32_t k, ph, add;
        if (((p->done | p->dim) >> key) & 1u) continue;
        if (!perf_pre_target(&perf_tab[key], &k, &ph)) { p->done |= 1ull << key; continue; }
        if (p->per_track[k] >= PRE_PER_TRACK) { p->dim |= 1ull << key; continue; }
        add = phrase_notes ? phrase_notes(ph) : 0u;
        if (notes_free < add || notes_free - add < PRE_FLOOR) { p->dim |= 1ull << key; return -1; }   /* (stops: the floor) */
        p->done |= 1ull << key; p->per_track[k]++; p->pinned[k] |= (uint8_t)(1u << (p->per_track[k] - 1u));
        *track = k; *phrase = ph;
        return (int)key;
    }
    return -1;
}
/* the mode left: nothing stays pinned */
static void perf_pre_end(perf_pre_t *p) { memset(p->pinned, 0, sizeof p->pinned); p->done = 0; }


/* a key whose entry is not a scene: a chain launches its first step on its track (ocl_go_at, as a scene's chain does: stopped at once, playing staged for the
 * project's launch point); a phrase launches by the chain step that holds it on that track (the slotless staged phrase is the audio part: R3-AUDIO-INTERRUPT), else it says so */
static void perf_launch_other(const perf_ent_t *e)
{
    uint32_t k = (e->flags >> 1) & 3u;
    if (k >= trk_n()) return;
    if (e->kind == PK_CHAIN) {
        int rc = ocl_go_at(k, e->target % OCL_NCHAIN, 0, OCL_NOROW, 0, 0);
        ui_message(rc ? "CHAIN: NOT LAUNCHED" : "CHAIN");
    } else if (e->kind == PK_PHRASE) {
        for (uint32_t c = 0; c < OCL_NCHAIN; c++)
            for (uint32_t en = 0; en < OCL_NENT; en++)
                if (lists.chain[c][en][0] == e->target) {
                    int rc = ocl_go_at(k, c, en, OCL_NOROW, 0, 0);
                    ui_message(rc ? "PHRASE: NOT LAUNCHED" : "PHRASE");
                    return;
                }
        ui_message("PHRASE: NOT IN A CHAIN");
    }
}
#endif
