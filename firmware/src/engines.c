/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Revisions:
 * (Orange Cat 0.11.1) oc_mod_boot from a running session parks the engine's voices; the table's two stores are one critical section
 * (Orange Cat 0.12.0) a module's pages as data (fw_pages.h), the Prophet's number is a module slot
 * (Orange Cat 0.12.2) the Prophet module fits 8 sectors (34,764 B measured): the firmware's PITCH_OCT and ENV_LIN through the table, the factory programs in a palette fo...
 */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol) and the parts'
 * sounds at power-on. */
#include "dsp.c"
#include "audio_batch.h"
#if MELODEE_PROPHET
#include "eng_prophet_test.c"
#endif
#include "eng_phase.c"
#include "eng_cz.c"
#include "eng_sid.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: synthesized 808 circuits (drum_808.c), the 909, a C64's SID (drum_sid.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM rendered as Dexed renders it (fm6_core.c) */
#if MELODEE_MACRO
#include "eng_macro.c"          /* (Orange Cat 0.5.0) MACRO: fourteen sound models (plaits/, MIT) */
#endif
#include "eng_drumx.c"          /* (Orange Cat 0.9.0) DRUM-X: Jiant FM's drum engine (drumx_voice.c, GPL-3.0-only); in slot 9 when MELODEE_DRUMX */
#if MELODEE_SHAPES
#include "eng_shapes.c"         /* (Orange Cat 0.5.1) SHAPES: a Braids-style macro oscillator (braids/, MIT) */
#endif
#if MELODEE_SLICE
#include "eng_slice.c"
#else
static void slice_gone_note_on(struct track *t, voice_t *v) { (void)t; (void)v; }
static void slice_gone_render(struct track *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
}
static const engine_t ENG_RETIRED = {         /* 13 without MELODEE_SLICE: reserved, never offered (eng_ok) */
    .name = "-",
    .page_title = {"-", "-"},
    .edit = {{"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
             {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0}},
    .note_on = slice_gone_note_on,
    .render = slice_gone_render,
    .knob = {P_E4, P_E5, P_E6, P_REL},
};
#endif

/* Each track owns only its selected engine's working state. Simple engines
 * use voice_t alone. State is released after the old engine's switch fade. */
#include "resources.c"
/* (Orange Cat 0.8.0, step 6; 0.10.0, step 1) the services a loadable engine is given (fw_api.h). On the device the table is
 * in the image at a fixed address: the section .fwapi, which firmware/app.ld places 0x100 bytes into the body (FW_API_ADDR), and
 * which the build checks. tests/fw_api_test.c reads it on the host. */
#include "oc_pack.h"
#include "fw_api.h"
#define FW_UNUSED __attribute__((unused))
static FW_UNUSED void *fw_mem_copy(void *d, const void *s, uint32_t n) { return memcpy(d, s, n); }
static FW_UNUSED void *fw_mem_set(void *d, int c, uint32_t n) { return memset(d, c, n); }
static FW_UNUSED void *fw_resource_get(uint32_t id, uint32_t bytes) { return resource_get(id, bytes); }
static FW_UNUSED int32_t fw_tuning_log(void) { return tuning_log(); }
uint32_t fw_rng_cell = 0x21u;                /* (0.10.0) the module SHAPES' pb_rng_state (braids/pb_dsp.h): the firmware's word, same start value */
#if defined(__APPLE__)
#define FW_API_SECTION                      /* (the host on macOS: Mach-O section names differ; nothing reads the address there) */
#else
#define FW_API_SECTION __attribute__((section(".fwapi")))
#endif
/* (0.12.1) what the Prophet module calls outside itself: the patches (project.c, the presets and motion share them), the one CPU1 job, the bend target, the audio
 * worker and the batch queue. midi_bend_target is voice.c's, declared here so the table can point at it (a second tentative definition of the same static). */
static int32_t midi_bend_target[NTRK];
#if MELODEE_DUAL_CORE
#define FW_P5_JOB (&p5_job)
#else
#define FW_P5_JOB ((void *)0)
#endif
#if MELODEE_DUAL_CORE
#define FW_WORKER_ONLINE (&audio_worker_online)
static FW_UNUSED int fw_worker_submit(void (*fn)(void *), void *context) { return audio_worker_submit(fn, context); }
static FW_UNUSED int fw_worker_join(void) { return audio_worker_join(); }
#else                                           /* (a single core build: no worker, the module never submits) */
static int fw_no_worker;
#define FW_WORKER_ONLINE (&fw_no_worker)
static FW_UNUSED int fw_worker_submit(void (*fn)(void *), void *context) { (void)fn; (void)context; return 0; }
static FW_UNUSED int fw_worker_join(void) { return 0; }
#endif
static FW_UNUSED void fw_batch_push(void (*run)(const void *, int32_t *), const void *data, uint32_t bytes, int32_t *out, uint32_t n, uint32_t id)
{
    audio_batch_push(run, data, bytes, out, n, id);
}
/* (0.13.7) the hot sets (below, "hot sets"): a module's pointers, and the built-in engines whose set did not fit */
static const void *eng_hot_set[ENG_HOT_SLOTS][ENG_HOT_MAX];  /* a module's pointers, one array a slot: the copies */
static const void **eng_hot[NENGINES];                     /* engine number e's (fw_api_t hot): its slot's array while a module holds one, else 0 */
static uint32_t eng_hot_refused;                           /* bit e: built-in engine e's set did not fit: its slot holds ENG_RETIRED (silent) */
FW_API_SECTION __attribute__((used)) const fw_api_t FW_API = {
    FW_API_MAGIC, sizeof(fw_api_t), FW_API_VERSION, FW_API_IFACE,
    fw_mem_copy, fw_mem_set, fw_resource_get, fw_tuning_log, trk, &oc_pack_good,
    RES_ENGINE0, NENGINES, &fw_rng_cell,
    p5_patch, p5_ready, FW_P5_JOB, midi_bend_target, FW_WORKER_ONLINE, &audio_batch_active,
    fw_worker_submit, fw_worker_join, fw_batch_push, AP_PROPHET, &voice_was, PITCH_OCT, ENV_LIN,
    (const void *const *const *)eng_hot,
};
/* (0.10.0) the module registered under an engine number, or 0 (the loader, eng_register): its header says the state size */
static const fw_mod_hdr_t *eng_mod[NENGINES];
static uint32_t eng_state_size(uint32_t e)
{
    if (e < NENGINES && eng_mod[e])
        return eng_mod[e]->state_size;
    switch (e) {
#if MELODEE_PROPHET && MELODEE_P5                 /* (Orange Cat: MELODEE_P5) */
    case ENGI_PROPHET: return sizeof(p5_part_t);
#if MELODEE_PROPHET_PROTOTYPE
    case 0: return sizeof(p5_part_t);
#endif
#endif
#if MELODEE_LEGACY_EXTRAS
    case ENGI_PHYS: return sizeof(phys_slot_t) * PHYS_POLY;
    case 7: return sizeof(drw_part_t);
#endif
#if MELODEE_LEGACY_EXTRAS
    case 2:                                         /* (PHASE: CZ-1's core) */
#endif
#if MELODEE_CZ
    case ENGI_CZ: return sizeof(cz_part_t);
#endif
#if MELODEE_SID
    case 3: return sizeof(sid_part_t);              /* SID (eng_sid.c) */
#endif
#if MELODEE_DRUM
    case ENGI_DRUM: return sizeof(drum909_part_t);
#endif
#if MELODEE_FM6
    case ENGI_FM6: return sizeof(fm6_part_t);
#endif
#if MELODEE_DRUMX
    case ENGI_DRUMX: return sizeof(dxe_part_t);
#endif
#if MELODEE_MACRO
    case ENGI_MACRO: return sizeof(mc_part_t);      /* (Orange Cat 0.5.0) */
#endif
#if MELODEE_SHAPES
    case ENGI_SHAPES: return sizeof(sh_part_t);     /* (Orange Cat 0.5.1) */
#endif
#if MELODEE_SLICE
    case ENGI_SLICE: return sizeof(slc_rb_t);
#endif
    default: return 0;
    }
}
static void eng_state_clear(uint32_t part)
{
    if (part < NPART) resource_release(RES_ENGINE0 + part);
}
static void eng_state_reset(void) { for (uint32_t k = 0; k < NPART; k++) eng_state_clear(k); }
static int eng_state_prepare(const track_t *t)
{
    uint32_t size = (MELODEE_DRUM && t->engine == ENGI_DRUM)?drum_bytes(t):eng_state_size(t->engine);
    return !size || resource_get(RES_ENGINE0 + (uint32_t)(t - trk), size) != 0;
}
static phys_slot_t *phys_slots(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(phys_slot_t) * PHYS_POLY); }
static drum909_part_t *drum909_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum909_part_t)); }
static drum_lane_t *drum_kit_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(drum_lane_t) * DV_NLANE); }
static dsk_part_t *dsk_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(dsk_part_t)); }
static drw_part_t *drw_of(const track_t *t) { return resource_get(RES_ENGINE0 + (uint32_t)(t - trk), sizeof(drw_part_t)); }
static fm6_part_t *fm6_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(fm6_part_t)); }
static dxe_part_t *dxe_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(dxe_part_t)); }   /* (0.9.0) DRUM-X */
#if MELODEE_MACRO
static mc_part_t *mc_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(mc_part_t)); }
#endif
#if MELODEE_SHAPES
static sh_part_t *sh_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(sh_part_t)); }
#endif
static cz_part_t *cz_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(cz_part_t)); }
static sid_part_t *sid_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(sid_part_t)); }
#if MELODEE_PROPHET
static p5_part_t *p5_part(uint32_t part) { return resource_get(RES_ENGINE0 + part % NPART, sizeof(p5_part_t)); }
#endif
#if MELODEE_SLICE
static int16_t (*slc_rbuf(uint32_t part))[SLC_RB] { return resource_get(RES_ENGINE0 + part % NPART, sizeof(slc_rb_t)); }
#endif

/* the editor protocol, user presets and projects store these indices: append, never reorder */
/* (Orange Cat 0.10.0, step 4) The table is RAM (80 B of .data, copied from flash by the startup code like every initialized
 * variable): a loaded module takes its slot at boot (eng_register, below). The entries are still these, until one does. */
static const engine_t *ENGINES[NENGINES] = {
#if MELODEE_PROPHET_PROTOTYPE
    &ENG_P5_TEST,                /* test firmware only: RAM patches, NEVER persist */
#else
    &ENG_RETIRED,                /* 0: retired ANALOG; stored ID reserved */
#endif
    &ENG_RETIRED,                /* 1: retired DIGITAL; stored ID reserved */
#if MELODEE_LEGACY_EXTRAS
    &ENG_PHASE,                  /* 2 */
#else
    &ENG_RETIRED,                /* 2: retired PHASE; stored ID reserved (its core: CZ-1's) */
#endif
#if MELODEE_SID
    &ENG_SID,                    /* 3: SID replaces LOFI without reusing a stored ID */
#else
    &ENG_RETIRED,                /* 3: (Orange Cat) SID unhooked (MELODEE_SID=0); stored ID reserved */
#endif
    &ENG_RETIRED,                /* 4: retired SAMPLE; reserved to preserve stored indices */
#if MELODEE_LEGACY_EXTRAS
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
#else
    &ENG_RETIRED,                /* 5: retired VOICE; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_TRIO,
#else
    &ENG_RETIRED,                /* 6: retired TRIO; stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_WHEEL,
#else
    &ENG_RETIRED,                /* 7: retired WHEEL; stored ID reserved */
#endif
#if MELODEE_MACRO
    &ENG_MACRO,                  /* 8: (Orange Cat 0.5.0) MACRO (ENGI_MACRO), in the slot of Melodee's retired GRAIN */
#else
    &ENG_RETIRED,                /* 8: retired GRAIN; stored index reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_PHYS,
#elif MELODEE_DRUMX
    &ENG_DRUMX,                  /* 9: (Orange Cat 0.9.0) DRUM-X (ENGI_DRUMX), in the slot of the retired PHYS */
#else
    &ENG_RETIRED,                /* 9: retired PHYS; stored ID reserved */
#endif
#if MELODEE_DRUM
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
#else
    &ENG_RETIRED,                /* 10: (Orange Cat) DRUM unhooked (MELODEE_DRUM=0); stored ID reserved */
#endif
#if MELODEE_LEGACY_EXTRAS
    &ENG_NOISE,                  /* 11 */
#else
    &ENG_RETIRED,                /* 11: retired NOISE; stored ID reserved */
#endif
#if MELODEE_FM6
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#else
    &ENG_RETIRED,                /* 12: (Orange Cat) FM6 unhooked (MELODEE_FM6=0); stored ID reserved */
#endif
#if MELODEE_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_RETIRED,             /* 13: reserved (MELODEE_SLICE=0 builds without SLICE) */
#endif
#if MELODEE_SHAPES
    &ENG_SHAPES,                 /* 14: (Orange Cat 0.5.1) SHAPES (ENGI_SHAPES), in the slot of Melodee's retired OBXF */
#else
    &ENG_RETIRED,                /* 14: retired OBXF; stored index reserved */
#endif
#if MELODEE_CZ
    &ENG_CZ,                     /* 15: native Casio CZ-1 */
#else
    &ENG_RETIRED,                /* 15: (Orange Cat) CZ-1 unhooked (MELODEE_CZ=0); stored ID reserved */
#endif
    &ENG_RETIRED, &ENG_RETIRED, &ENG_RETIRED, /* 16..18 reserved */
#if MELODEE_P5
    &ENG_P5_TEST,                /* 19: Prophet (ANALOG's ID 0 stays reserved) */
#else
    &ENG_RETIRED,                /* 19: (Orange Cat) PROPHET unhooked (MELODEE_P5=0); stored ID reserved */
#endif
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* preset k (< npresets) of e: its row, or one made on demand (Prophet, CZ-1: their names live in the native data).
 * Made presets take turns in two slots: two can be held at once (main loop only) */
static const preset_t *eng_preset(const engine_t *e, uint32_t k)
{
    static preset_t made[2];
    static char made_name[2][21];
    static uint8_t turn;
    if (e->presets)
        return &e->presets[k];
    turn ^= 1u;
    memset(&made[turn], 0, sizeof made[turn]);
    made_name[turn][0] = 0;
    made[turn].name = made_name[turn];
    e->preset_make(k, &made[turn], made_name[turn]);
    return &made[turn];
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the editor's list):
 * engine indices, never a retired one. The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
#if MELODEE_P5
    ENGI_PROPHET,                /* Prophet replaces ANALOG in new-sound browsing */
#endif
#if MELODEE_FM6
    12,                          /* FM6 */
#endif
#if MELODEE_LEGACY_EXTRAS
    2, ENGI_CZ, 3, 5,             /* PHASE CZ-1 SID VOICE */
    6, 7, 9,
    11,                          /* NOISE */
#else
#if MELODEE_CZ
    ENGI_CZ,                      /* CZ-1 */
#endif
#if MELODEE_SID
    3,                            /* SID */
#endif
#endif
#if MELODEE_SLICE
    13,                          /* SLICE */
#endif
#if MELODEE_DRUM
    10,                          /* DRUM */
#endif
#if MELODEE_DRUMX
    ENGI_DRUMX,                  /* (Orange Cat 0.9.0) DRUM-X: next to DRUM (first of the drums in Orange Cat's build, where DRUM is out) */
#endif
#if MELODEE_MACRO
    ENGI_MACRO,                  /* (Orange Cat 0.5.0) MACRO: after Melodee's own (first in Orange Cat's build, where
                                  * they are unhooked) */
#endif
#if MELODEE_SHAPES || OC_MODULES
    ENGI_SHAPES,                 /* (Orange Cat 0.5.1) SHAPES; (0.10.0) kept in the order while a module may supply it (eng_ok says whether it is there) */
#endif
#if !MELODEE_P5 && OC_MODULES
    ENGI_PROPHET,                /* (Orange Cat 0.12.1) the Prophet as a module: offered only while one is registered (eng_ok) */
#endif
};

/* Retired synth records (ANALOG and DIGITAL since 2.0 too) retain their stored identity and render silence.
 * Never relabel retired records FM6. */
static int eng_extra_retired(uint32_t e)
{
#if !MELODEE_PROPHET_PROTOTYPE
    if (e == ENGI_ANALOG) return 1;
#endif
    return e == ENGI_DIGITAL || (!MELODEE_LEGACY_EXTRAS && (e == 2u || e == 5u || e == 6u || e == 7u || (e == 9u && !MELODEE_DRUMX) || e == 11u))
        || (!MELODEE_SID && e == 3u) || (!MELODEE_CZ && e == ENGI_CZ) || (!MELODEE_FM6 && e == ENGI_FM6) || (!MELODEE_P5 && e == ENGI_PROPHET && !eng_mod[ENGI_PROPHET]) || (!MELODEE_DRUM && e == ENGI_DRUM);              /* (Orange Cat) an unhooked engine is retired: silent, identity kept */
}
/* the engines one can pick, in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e)
{
    return e < NENGINES && !(e >= 16u && e <= 18u) && !eng_extra_retired(e) && e != 4u && (MELODEE_MACRO || e != 8u) && (MELODEE_SHAPES || e != 14u || eng_mod[14]) && (MELODEE_SLICE || e != ENGI_SLICE);
}
static uint32_t eng_sound_idx(uint32_t e)
{
    return eng_extra_retired(e) || eng_ok(e) ? e : ENGI_FALLBACK;   /* (Orange Cat: was ENGI_FM6) */
}
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGI_FALLBACK;                       /* (a retired or reserved engine: the fallback's place; Orange Cat: was FM6) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    uint32_t n = eng_rank(e % NENGINES), tries;
    for (tries = 0; tries < NENG_SHOWN; tries++) {
        n = (n + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN;
        if (eng_ok(eng_vis(n)))
            break;                                   /* (0.10.0) an engine only a module can supply, with none there, is not stopped on */
    }
    return eng_vis(n);
}

/* ------------------------------------------------------------------------------ modules (Orange Cat 0.10.0, step 4) ---
 * The loader (oc_module.c, step 5) finds a valid module in the module area at boot and registers it here: the engine number
 * in its header takes the slot of the table, eng_state_size and eng_ok then answer from its header. Only the engines a module
 * may supply are accepted (eng_module_slot: SHAPES). A built-in engine of the same number stays unless OC_MODULE_WINS (core.h)
 * says the module takes over; eng_unregister puts the original back. Nothing here reads flash. */
enum { EM_OK = 0, EM_SLOT = -1, EM_SHADOWED = -2, EM_BAD = -3, EM_NOROOM = -4 };   /* (0.13.7) EM_NOROOM: its hot set does not fit the arena */

/* ------------------------------------------------------------------------------ hot sets (Orange Cat 0.13.7) ---
 * An engine's per-sample tables (engine_t hot, nhot) are copied into the audio arena when it is registered, and it reads the copies (ENG_HOT):
 * a read through the flash's plaintext window (the asset pack, a module's image) costs about 250 ns more than a read from the image or RAM
 * (hw/LOG.md, docs/orangecat/ENGINE_LOADING_PLAN.md "What runs from where, measured"). A built-in engine's set is taken at boot (eng_hot_boot);
 * a module's when the loader registers it (eng_register), and dropped when it leaves (eng_unregister, which takes the built-in engine's again).
 * One arena block a set (RES_HOT0 + slot, ENG_HOT_SLOTS at once). A set that does not fit refuses the registration: EM_NOROOM for a module;
 * a built-in engine's slot then holds ENG_RETIRED (silent, as an unhooked engine) and eng_hot_refused says so. (At boot the arena is still
 * empty, so a built-in set of a few KB always fits; eng_ok is left alone: it is inlined in the UI, and a test there cost 1.7 KB of image.) When the asset pack is checked again (an install, an erase), the
 * copies are made again (oc_pack_changed). A dropped set's pointers go back to its sources, so nothing ever reads a released block. */
static uint8_t eng_hot_slot[NENGINES];                     /* 1 + the slot of engine number e's set, 0 none */
static const eng_hot_t *eng_hot_list[ENG_HOT_SLOTS];       /* a slot's set: its list, its length, where its pointers are (a built-in engine's own */
static uint8_t eng_hot_n[ENG_HOT_SLOTS];                   /* hot_at, else the slot's eng_hot_set: a module's). Kept here, so a copy, a refresh or */
static const void **eng_hot_at[ENG_HOT_SLOTS];             /* a drop never reads an engine_t again (a module's is in flash, checked once by the loader) */
static uint32_t eng_hot_bytes(const eng_hot_t *hot, uint32_t n)   /* the arena a set takes (each table rounded to 4 bytes) */
{
    uint32_t b = 0;
    for (uint32_t i = 0; i < n; i++) b += (hot[i].bytes + 3u) & ~3u;
    return b;
}
static void eng_hot_copy(uint32_t e)                       /* the slot's block <- the sources; the pointers to the copies */
{
    uint32_t s = eng_hot_slot[e] - 1u, off = 0;
    uint8_t *block = resource[RES_HOT0 + s].ptr;
    for (uint32_t i = 0; i < eng_hot_n[s]; i++) {
        memcpy(block + off, eng_hot_list[s][i].src, eng_hot_list[s][i].bytes);
        eng_hot_at[s][i] = block + off;
        off += (eng_hot_list[s][i].bytes + 3u) & ~3u;
    }
}
static void eng_hot_drop(uint32_t e)
{
    if (e >= NENGINES || !eng_hot_slot[e]) return;
    uint32_t s = eng_hot_slot[e] - 1u;
    for (uint32_t i = 0; i < eng_hot_n[s]; i++) eng_hot_at[s][i] = eng_hot_list[s][i].src;
    if (eng_hot_at[s] == eng_hot_set[s]) eng_hot[e] = 0;
    resource_release(RES_HOT0 + s);
    eng_hot_list[s] = 0; eng_hot_n[s] = 0; eng_hot_at[s] = 0;
    eng_hot_slot[e] = 0;
}
/* the set hot[0 .. n) for engine number e, its pointers at `at` (a built-in engine's hot_at), or 0 for a module's (the firmware's, fw_api_t hot); the
 * set there before is dropped first: EM_OK, EM_BAD (more than ENG_HOT_MAX tables), EM_NOROOM */
static int eng_hot_take(uint32_t e, const void **at, const eng_hot_t *hot, uint32_t n)
{
    uint32_t s;
    eng_hot_drop(e);
    if (!hot || !n) return EM_OK;
    if (n > ENG_HOT_MAX || e >= NENGINES) return EM_BAD;
    for (s = 0; s < ENG_HOT_SLOTS && eng_hot_list[s]; s++) ;
    if (s == ENG_HOT_SLOTS || !resource_get(RES_HOT0 + s, eng_hot_bytes(hot, n))) return EM_NOROOM;
    eng_hot_list[s] = hot; eng_hot_n[s] = (uint8_t)n; eng_hot_at[s] = at ? at : eng_hot_set[s];
    eng_hot_slot[e] = (uint8_t)(s + 1u);
    if (!at) eng_hot[e] = eng_hot_set[s];
    eng_hot_copy(e);
    return EM_OK;
}
static int eng_hot_take_own(uint32_t e, const engine_t *g)   /* a built-in engine's own set (its engine_t is in the image) */
{
    return g ? eng_hot_take(e, g->hot_at, g->hot, g->nhot) : (eng_hot_drop(e), EM_OK);
}
static void eng_hot_refresh(void)                          /* the asset pack was checked again: copy every set again */
{
    for (uint32_t e = 0; e < NENGINES; e++)
        if (eng_hot_slot[e]) eng_hot_copy(e);
}
/* persist_boot, after the pack check and the module loader: the built-in engines' sets (a registered module took its own already) */
static void eng_hot_boot(void)
{
    for (uint32_t e = 0; e < NENGINES; e++) {
        const engine_t *g = ENGINES[e];
        if (eng_mod[e] || !g || !g->nhot) continue;
        if (eng_hot_take_own(e, g)) { eng_hot_refused |= 1u << e; ENGINES[e] = &ENG_RETIRED; }
    }
    oc_pack_changed = eng_hot_refresh;
}
static int eng_module_slot(uint32_t e) { return e == ENGI_SHAPES || e == ENGI_PROPHET; }
static const engine_t *eng_default(uint32_t e)     /* what the slot holds without a module */
{
#if MELODEE_SHAPES
    if (e == ENGI_SHAPES)
        return &ENG_SHAPES;
#endif
#if MELODEE_P5
    if (e == ENGI_PROPHET)
        return &ENG_P5_TEST;       /* (0.12.1) built in: the slot's own engine */
#endif
    (void)e;
    return &ENG_RETIRED;
}
/* the module with header h and engine_t engine takes its slot: 0, EM_SLOT (not a number a module may supply), EM_SHADOWED
 * (built in, and OC_MODULE_WINS is 0), EM_BAD (nothing given) */
/* (0.13.7) the loader's way in: the module's hot set as the loader read and checked it in flash (oc_module.c oc_mod_hot_check), never through the engine_t
 * here (on a host build a module's engine_ptr is a flash address) */
static int eng_register_hot(const fw_mod_hdr_t *h, const engine_t *engine, const eng_hot_t *hot, uint32_t nhot);
static int eng_register(const fw_mod_hdr_t *h, const engine_t *engine)   /* an engine_t in RAM or the image (the tests', a host module's) */
{
    return eng_register_hot(h, engine, engine ? engine->hot : 0, engine ? engine->nhot : 0u);
}
static int eng_register_hot(const fw_mod_hdr_t *h, const engine_t *engine, const eng_hot_t *hot, uint32_t nhot)
{
    uint32_t e;
    if (!h || !engine)
        return EM_BAD;
    e = h->engine;
    if (e >= NENGINES || !eng_module_slot(e))
        return EM_SLOT;
    if (eng_default(e) != &ENG_RETIRED && !OC_MODULE_WINS)
        return EM_SHADOWED;
    {   /* (0.13.7) its hot set first: one that does not fit is NO ROOM, and what holds the slot keeps its own */
        uint32_t s = eng_hot_slot[e] ? eng_hot_slot[e] - 1u : ENG_HOT_SLOTS;
        const eng_hot_t *was = s < ENG_HOT_SLOTS ? eng_hot_list[s] : 0;
        uint32_t wasn = s < ENG_HOT_SLOTS ? eng_hot_n[s] : 0u;
        const void **wasat = s < ENG_HOT_SLOTS && eng_hot_at[s] != eng_hot_set[s] ? eng_hot_at[s] : 0;
        int rc = eng_hot_take(e, 0, hot, nhot);
        if (rc) {
            if (eng_hot_take(e, wasat, was, wasn)) { eng_hot_refused |= 1u << e; ENGINES[e] = &ENG_RETIRED; }
            return rc;
        }
    }
    eng_hot_refused &= ~(1u << e);
    ENGINES[e] = engine;                            /* (0.11.1) the two stores are one step in a running session: oc_module.c's oc_mod_reg / oc_mod_unreg */
    eng_mod[e] = h;
    return EM_OK;
}
static void eng_unregister(uint32_t e)
{
    if (e >= NENGINES)
        return;
    eng_mod[e] = 0;                                 /* (0.11.1) one step in a running session, as in eng_register */
    ENGINES[e] = eng_default(e);
    if (eng_hot_take_own(e, ENGINES[e])) { eng_hot_refused |= 1u << e; ENGINES[e] = &ENG_RETIRED; }   /* (0.13.7) the module's set out, the built-in engine's in */
}

/* the parts at power-on and in a new song (engine, preset; the steps empty): poly, bass, lead, drums. IT'S A
 * PROPHET 5, FM6 SOLID BASS, CZ-1 SYNTH.LEAD 1, 808 KIT (until 2026-10-16: PROPHET FUNK BASS II, FM6 PAD, SID INIT
 * SID, 808 KIT) */
/* (Orange Cat) an unhooked engine's track takes the fallback: FM6 SOLID BASS, else TRIO FAT BASS (track 2); FM6
 * SOLID BASS, else TRIO ARP LEAD (track 3, when CZ-1 is out too). (0.5.0) MACRO before TRIO: CHORD MINOR, VA BASS,
 * VA SYNC LEAD, KICK DEEP (eng_macro.c MC_PRESETS 10, 0, 1, 20; TRIO's were 4, 0, 1, 3) */
#define TRK_ALT (MELODEE_MACRO ? ENGI_MACRO : 6u)
static const uint8_t TRK_DEF[NPART][2] = {
    {MELODEE_P5 ? ENGI_PROPHET : TRK_ALT, MELODEE_P5 ? 1 : (MELODEE_MACRO ? 10 : 4)},
    {MELODEE_FM6 ? ENGI_FM6 : TRK_ALT, MELODEE_FM6 ? 10 : 0},
    {MELODEE_CZ ? ENGI_CZ : MELODEE_FM6 ? ENGI_FM6 : TRK_ALT, MELODEE_CZ ? 52 : MELODEE_FM6 ? 10 : 1},
    {MELODEE_DRUM ? ENGI_DRUM : TRK_ALT, MELODEE_DRUM ? 0 : (MELODEE_MACRO ? 20 : 3)}};
