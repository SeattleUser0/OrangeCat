/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0 (step 6): fw_api_t, the table of firmware services a loadable engine is given
 * (docs/orangecat/ENGINE_LOADING_PLAN.md "The module image"). Since 0.10.0 the table is in the image at a fixed address
 * (below). tests/fw_api_test.c checks it against the real services.
 *
 * What a module needs from the firmware is what SHAPES needs (an audit of the final AST of eng_shapes.c and braids/, host
 * build, October 23, 2026): the track array `trk` (the engine reads its track's parameters), `tuning_log` (the A4
 * offset; it reads a firmware global), `resource_get` (its part's state from the audio arena), the asset pack's ready
 * flag `oc_pack_good`, and memcpy / memset. Everything else it calls is header-only and goes into the module: clamp,
 * voice_amp (with amp_at), and the plaits/pl_dsp.h helpers pl_soft_clip, pl_svf_*, pl_semitones_to_ratio, pl_flush (SHAPES
 * shares them with MACRO). The tables are the pack's, at fixed addresses (oc_pack_tables.h), so they are not imports.
 *
 * A module finds the table at FW_API_ADDR (it is not handed a pointer). The table starts with a magic, its own
 * size and its version, so a module built for a longer table than the firmware's is refused (a shorter one works).
 * The interface identifier (0.9.0, step 5a): FW_API_ID in build/gen/fw_api_id.h is a 32-bit hash that
 * tools/gen_fw_api_id.py computes from the text of fw_api_t, param_desc_t, voice_t, vmod_t, preset_t, engine_t and
 * track_t (core.h), comments and white space aside. FW_API_IFACE mixes in the three sizes the text cannot show (the
 * array lengths behind P_COUNT, NVOICE and the like change sizeof). A module carries FW_API_IFACE in its header
 * (fw_mod_hdr_t); fw_module_check refuses it unless the numbers equal the firmware's. The loader
 * (0.10.0, step 5) calls the check at boot.
 *
 * Where the table is (0.10.0, step 1): in the image, at FW_API_ADDR (build/gen/fw_api_id.h, from tools/gen_fw_api_id.py), 0x100
 * bytes into the app body. firmware/app.ld puts the section .fwapi there, after the entry code and before any other code, and
 * tools/build.py fails the build if the linked symbol FW_API is anywhere else. A module reads the table at that address
 * ((const fw_api_t *)FW_API_ADDR), so it keeps no pointer of its own: its .data and .bss are zero bytes (tools/build.py
 * --module checks it). The module cannot read the table to learn that the address moved, so the address is part of its header
 * (api_addr) and fw_module_check compares it with the firmware's. The table is data inside the PC guard's code window
 * (0x02000120.._etext, fm1_guard.h): nothing executes it, so the guard is unchanged. The table also carries what the module
 * would otherwise hold as constants (the arena id of the first engine state, res_engine0, and the number of engines) and the one
 * word of state SHAPES keeps outside its part (Braids' random generator, rng_cell: the same cell, shared by the parts, as the built-in engine has).
 *
 * Revisions:
 *   0.8.0: first version.
 *   0.11.0: fw_mod_hdr_t gains `cost` and `version` (before length): FW_MOD_HDR_BYTES 60.
 *   0.12.1: eleven members for the Prophet module (p5_patch .. voice_was): the patches and ready flags the project, presets and motion share, the one CPU1 job,
 *           the bend target, the audio worker and batch queue, and voice_was (a module's own copy of the firmware's static is never written). FW_API_VERSION stays 3 (the identifier changes with the struct).
 *   0.9.0: the `reserved` word is `iface`, the identifier (the table keeps its size); fw_mod_hdr_t and fw_module_check;
 *          FW_API_VERSION 2.
 */
#pragma once
#include "fw_api_id.h"                                   /* generated: FW_API_ID (tools/gen_fw_api_id.py) */
#define FW_API_MAGIC 0x50414657u                         /* "WFAP" little endian: FW API */
#define FW_API_VERSION 3u
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
    uint32_t res_engine0;                                /* the arena id of the first engine state (RES_ENGINE0): a module's part p is res_engine0 + p */
    uint32_t nengines;                                   /* NENGINES: the length of the engine table */
    uint32_t *rng_cell;                                  /* SHAPES' random generator word (pb_rng_state, 0x21 at power-on): a module has no RAM, so the firmware keeps it */
    /* (0.12.1, the Prophet as a module: what its source calls outside itself) */
    void *p5_patch;                                      /* p5_patch_t[NPART] (the patches the editor, the presets and motion also use; the pool) */
    uint8_t *p5_ready;                                   /* NPART flags for them */
    void *p5_job;                                        /* the firmware's p5_job_t block: the one outstanding CPU1 job (the module's p5_job_t is the same type) */
    int32_t *midi_bend_target;                           /* NTRK: the bend target p5_update_bend writes */
    const int *worker_online;                            /* audio_worker_online */
    const uint8_t *batch_active;                         /* audio_batch_active */
    int (*worker_submit)(void (*fn)(void *), void *context);
    int (*worker_join)(void);
    void (*batch_push)(void (*run)(const void *, int32_t *), const void *data, uint32_t bytes, int32_t *out, uint32_t n, uint32_t profile_id);
    uint32_t ap_prophet;                                 /* AP_PROPHET: the batch profile id */
    const uint8_t *voice_was;                            /* core.h voice_was: what the voice a note starts did just before (an engine's note_on reads it; found by the host render test) */
} fw_api_t;

/* The header a module starts with (its first bytes, before any code). The firmware reads it before it runs anything.
 * The image: the header in the first 256 byte page of the module's first sector (the rest of the page is zero), the code from
 * FW_MOD_CODE_OFF on. The installer programs the code first and this page last, so a torn install has no header (the pack's rule).
 * tools/build.py --module fills length, crc and hcrc after the link; the other fields are the module's own source.
 *   length   bytes of the image from the header page on, a multiple of 4
 *   crc      CRC-32 (zlib's) of bytes FW_MOD_CODE_OFF .. length
 *   hcrc     CRC-32 of bytes 0 .. offsetof(hcrc)
 *   engine   the engine number it is (ENGI_*): the table slot the loader puts it in
 *   engine_ptr  the address of its engine_t, inside the module (the code is linked for the address it sits at)
 *   link_addr  the XIP address it was linked for; the loader refuses it anywhere else
 *   pack_hash  OC_PACK_HASH the module's tables were taken from (the asset pack it reads); must be the firmware's
 *   version  (0.11.0) the module's own version 0x00MMmmpp, the module source's constant (SHAPES 1.0.0 = 0x00010000); the engine list shows it
 *   cost     (0.11.0) the engine's measured render cost, instructions a sample from tests/target_budget.txt (tools/build.py sums the lines the module
 *            names in tools/module/<name>.cost); 0 = not measured. Carried for a render-time admission later: the firmware has no such budget today
 *            (MELODEE_ADMISSION is 0), so the admission at assignment (ui.c eng_admit) uses the voice units and the arena only */
#define FW_MOD_MAGIC 0x444F4D4Fu                         /* "OMOD" little endian */
#define FW_MOD_CODE_OFF 0x100u
typedef struct fw_mod_hdr {
    uint32_t magic;                                      /* FW_MOD_MAGIC */
    uint32_t iface;                                      /* the FW_API_IFACE the module was built with */
    uint32_t api_size;                                   /* sizeof(fw_api_t) it was built with: the table it needs */
    uint32_t api_version;                                /* FW_API_VERSION it was built with */
    uint32_t api_addr;                                   /* FW_API_ADDR it reads the table at: checked data (0.10.0) */
    uint32_t engine;                                     /* the engine number (ENGI_*) it registers under */
    uint32_t state_size;                                 /* bytes of arena state a part of this engine needs (sizeof its part struct) */
    uint32_t engine_ptr;                                 /* address of its engine_t */
    uint32_t link_addr;                                  /* the address it was linked for */
    uint32_t pack_hash;                                  /* OC_PACK_HASH of the pack it reads */
    uint32_t cost;                                       /* (0.11.0) measured render cost, see above; 0 not measured */
    uint32_t version;                                    /* (0.11.0) the module's own version, 0x00MMmmpp (major, minor, patch); shown by the engine list (ED_ENGINES) */
    uint32_t length;                                     /* see above: filled after the link */
    uint32_t crc;
    uint32_t hcrc;
} fw_mod_hdr_t;
#define FW_MOD_HDR_BYTES 60u                             /* sizeof(fw_mod_hdr_t) (0.11.0: 52 before the cost and version words; no module of 0.10.x is on a unit) */
enum { FW_MOD_OK = 0, FW_MOD_NONE, FW_MOD_BAD_MAGIC, FW_MOD_BAD_IFACE, FW_MOD_API_TOO_NEW, FW_MOD_BAD_ADDR };

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
    if (h->api_addr != FW_API_ADDR)                      /* it would read some other place for the table */
        return FW_MOD_BAD_ADDR;
    if (h->api_size > sizeof(fw_api_t) || h->api_version > FW_API_VERSION)
        return FW_MOD_API_TOO_NEW;
    return FW_MOD_OK;
}
