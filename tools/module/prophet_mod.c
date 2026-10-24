/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat exp/sonnet-p5mod (0.12.0 Part 2a, a MEASUREMENT unit): the Prophet (ENGI_PROPHET 19, eng_prophet_test.c, prophet_patch.c) as a
 * loadable module, built the way shapes_mod.c builds SHAPES. The engine's source is included as it is, with three changes under OC_MODULE:
 * the patches and their ready flags (p5_patch, p5_ready, in the pool) and the one outstanding CPU1 job are the firmware's, reached through
 * the table; and the services the engine calls outside itself (the audio batch queue, the CPU1 worker, the bend target, str_cpy) are the
 * table's members (fw_api.h, "exp/sonnet-p5mod"). Everything else it uses is header-only and is compiled in.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#define memcmp oc_mod_memcmp
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_fw;
#define OC_FW() (oc_host_fw)
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#endif
#include "oc_pack.h"

#define trk (OC_FW()->trk)
#define tuning_log() (OC_FW()->tuning_log())
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n)
{
    const uint8_t *x = a, *y = b;
    for (__SIZE_TYPE__ i = 0; i < n; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}
static void str_cpy(char *d, const char *s, unsigned max)
{
    unsigned i = 0;
    while (s[i] && i + 1 < max) { d[i] = s[i]; i++; }
    d[i] = 0;
}
/* the CPU1 worker and the batch queue: the firmware's, by the table */
typedef void (*audio_batch_fn)(const void *data, int32_t *pcm);
#define AUDIO_BATCH_BYTES 168u
#define audio_worker_online (*OC_FW()->worker_online)
#define audio_batch_active (*OC_FW()->batch_active)
#define audio_worker_submit(fn, ctx) (OC_FW()->worker_submit((fn), (ctx)))
#define audio_worker_join() (OC_FW()->worker_join())
#define audio_batch_push(run, data, bytes, out, n, id) (OC_FW()->batch_push((run), (data), (bytes), (out), (n), (id)))
#define AP_PROPHET (OC_FW()->ap_prophet)

#include "eng_prophet_test.c"
static p5_part_t *p5_part(uint32_t part) { return resource_get(OC_FW()->res_engine0 + part % NPART, sizeof(p5_part_t)); }
static void p5_update_bend(track_t *t)       /* voice.c's, moved: it reads the patch and writes the firmware's bend target */
{
    if (!t->bend_raw) { OC_FW()->midi_bend_target[(uint32_t)(t - trk)] = 0; return; }
    uint32_t ti = (uint32_t)(t - trk);
    int32_t range = (clamp(p5_patch_of(t)->raw[P5_BEND], 0, 11) + 1) * 256;
    OC_FW()->midi_bend_target[ti] = t->bend_raw * range / (t->bend_raw < 0 ? 8192 : 8191);
}

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_PROPHET, sizeof(p5_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_P5_TEST; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_PROPHET, sizeof(p5_part_t), (uint32_t)(uintptr_t)&ENG_P5_TEST, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
