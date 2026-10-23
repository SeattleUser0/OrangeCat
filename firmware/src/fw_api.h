/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0 (step 6): fw_api_t, the table of firmware services a loadable engine would be given
 * (docs/orangecat/ENGINE_LOADING_PLAN.md "The module image"). PREPARATION ONLY: nothing loads from it, nothing in the
 * image refers to it (the instance is a static const that no code uses, so the compiler drops it: the image is unchanged),
 * no flash layout and no PC guard changes. tests/fw_api_test.c checks it against the real services.
 *
 * What a module needs from the firmware is what SHAPES needs (an audit of the final AST of eng_shapes.c and braids/, host
 * build, October 23, 2026): the track array `trk` (the engine reads its track's parameters), `tuning_log` (the A4
 * offset; it reads a firmware global), `resource_get` (its part's state from the audio arena), the asset pack's ready
 * flag `oc_pack_good`, and memcpy / memset. Everything else it calls is header-only and goes into the module: clamp,
 * voice_amp (with amp_at), and the plaits/pl_dsp.h helpers pl_soft_clip, pl_svf_*, pl_semitones_to_ratio, pl_flush (SHAPES
 * shares them with MACRO). The tables are the pack's, at fixed addresses (oc_pack_tables.h), so they are not imports.
 *
 * A module receives a pointer to FW_API at alloc time and keeps it in its state. The table starts with a magic, its own
 * size and its version, so a module built for a longer table than the firmware's is refused (a shorter one works).
 * The interface identifier (0.9.0, step 5a): FW_API_ID in build/gen/fw_api_id.h is a 32-bit hash that
 * tools/gen_fw_api_id.py computes from the text of fw_api_t, param_desc_t, voice_t, vmod_t, preset_t, engine_t and
 * track_t (core.h), comments and white space aside. FW_API_IFACE mixes in the three sizes the text cannot show (the
 * array lengths behind P_COUNT, NVOICE and the like change sizeof). A module carries FW_API_IFACE in its header
 * (fw_mod_hdr_t); fw_module_check refuses it unless the numbers equal the firmware's. Nothing calls the check in the
 * image yet: it is the registration test of the loader that is not built (the compiler drops it).
 *
 * Revisions:
 *   0.8.0: first version.
 *   0.9.0: the `reserved` word is `iface`, the identifier (the table keeps its size); fw_mod_hdr_t and fw_module_check;
 *          FW_API_VERSION 2.
 */
#pragma once
#include "fw_api_id.h"                                   /* generated: FW_API_ID (tools/gen_fw_api_id.py) */
#define FW_API_MAGIC 0x50414657u                         /* "WFAP" little endian: FW API */
#define FW_API_VERSION 2u
/* the identifier a module is built with and the firmware compares: the hash of the text, and the sizes of what it hashes */
#define FW_API_IFACE (FW_API_ID ^ ((uint32_t)sizeof(track_t) * 0x9E3779B1u) ^ ((uint32_t)sizeof(voice_t) * 0x85EBCA6Bu) ^ \
                      ((uint32_t)sizeof(engine_t) * 0xC2B2AE35u) ^ ((uint32_t)sizeof(vmod_t) * 0x27D4EB2Fu))

typedef struct fw_api {
    uint32_t magic;                                      /* FW_API_MAGIC */
    uint32_t size;                                       /* sizeof(fw_api_t) of the firmware that made it */
    uint32_t version;                                    /* FW_API_VERSION */
    uint32_t iface;                                      /* FW_API_IFACE */
    void *(*mem_copy)(void *d, const void *s, uint32_t n);
    void *(*mem_set)(void *d, int c, uint32_t n);
    void *(*resource_get)(uint32_t id, uint32_t bytes);  /* the arena, as the engines' accessors call it */
    int32_t (*tuning_log)(void);                         /* the A4 offset, Q-whatever dsp.c returns */
    track_t *trk;                                        /* the track array (NTRK) */
    const uint8_t *pack_ready;                           /* 1 when the asset pack is there (oc_pack_good) */
} fw_api_t;

/* The header a module starts with (its first bytes, before any code). The firmware reads it before it runs anything. */
#define FW_MOD_MAGIC 0x444F4D4Fu                         /* "OMOD" little endian */
typedef struct fw_mod_hdr {
    uint32_t magic;                                      /* FW_MOD_MAGIC */
    uint32_t iface;                                      /* the FW_API_IFACE the module was built with */
    uint32_t api_size;                                   /* sizeof(fw_api_t) it was built with: the table it needs */
    uint32_t api_version;                                /* FW_API_VERSION it was built with */
} fw_mod_hdr_t;
enum { FW_MOD_OK = 0, FW_MOD_NONE, FW_MOD_BAD_MAGIC, FW_MOD_BAD_IFACE, FW_MOD_API_TOO_NEW };

/* The registration check: 0 when the module may be given FW_API, else why not. A module that needs a longer table or a
 * later version than the firmware has is refused (a shorter or earlier one works); the identifier must be equal. */
static inline __attribute__((unused)) int fw_module_check(const fw_mod_hdr_t *h)
{
    if (!h)
        return FW_MOD_NONE;
    if (h->magic != FW_MOD_MAGIC)
        return FW_MOD_BAD_MAGIC;
    if (h->iface != FW_API_IFACE)
        return FW_MOD_BAD_IFACE;
    if (h->api_size > sizeof(fw_api_t) || h->api_version > FW_API_VERSION)
        return FW_MOD_API_TOO_NEW;
    return FW_MOD_OK;
}
