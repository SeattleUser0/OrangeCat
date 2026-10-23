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
 * The interface identifier of the plan (a hash of the layouts of track_t, voice_t, vmod_t, engine_t and of this
 * header) is NOT built yet: FW_API_VERSION is the number until it is.
 *
 * Revisions:
 *   0.8.0: first version.
 */
#pragma once
#define FW_API_MAGIC 0x50414657u                         /* "WFAP" little endian: FW API */
#define FW_API_VERSION 1u

typedef struct fw_api {
    uint32_t magic;                                      /* FW_API_MAGIC */
    uint32_t size;                                       /* sizeof(fw_api_t) of the firmware that made it */
    uint32_t version;                                    /* FW_API_VERSION */
    uint32_t reserved;
    void *(*mem_copy)(void *d, const void *s, uint32_t n);
    void *(*mem_set)(void *d, int c, uint32_t n);
    void *(*resource_get)(uint32_t id, uint32_t bytes);  /* the arena, as the engines' accessors call it */
    int32_t (*tuning_log)(void);                         /* the A4 offset, Q-whatever dsp.c returns */
    track_t *trk;                                        /* the track array (NTRK) */
    const uint8_t *pack_ready;                           /* 1 when the asset pack is there (oc_pack_good) */
} fw_api_t;
