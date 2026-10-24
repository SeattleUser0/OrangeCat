/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.1: the Prophet (ENGI_PROPHET 19: eng_prophet_test.c, prophet_patch.c, prophet_edit.h) as a loadable engine module, built the way shapes_mod.c builds
 * SHAPES. The engine's source is included as it is, with a few lines under OC_MODULE: the patches and their ready flags (the project, the presets and motion
 * share them: the firmware's, by the table) and the one outstanding CPU1 job (the firmware's block, by the table). What the engine calls outside itself (the audio
 * batch queue, the CPU1 worker, the bend target) are the table's members (fw_api.h), as are memcpy, memset and the arena.
 *
 * The module is 8 sectors of the module area (32,768 B; measured 31,148 B for the code and its 11 KB of factory tables), linked for two starts (tools/build.py
 * MODULES): sector 0 and sector 4. It does not fit beside the SHAPES module (6 sectors) in the 12 sector area, and the installer refuses the pair (oc_module.c).
 *
 * The pages come with it (fw_pages.h): P5_PAGES is the descriptor engine_t.pages points at. The rows are the Prophet's PAGES rows of params.c (15 of them: the
 * STORE page is the firmware's UI state, p5_store_slot and the user slots, and is not part of the module), the panel is prophet_edit.h's P5_PANEL, and the
 * hooks are the Prophet's own rules: the keyed tuning shown in semitones, the bend shown from 1, the unison voice count shown as 5 until it is set, a put that
 * re-reads the voices, the "does nothing now" hints, the voice assignment that motion leaves alone, HOME's four knobs, and the ENV and LFO page lists.
 * New hooks (fw_pages.h version 2): loaded (a preset sets the patch), env_source (the matrix's ENV is the amplitude envelope), bend (the wheel's range is the
 * patch's) and cc (MIDI CC 72, 73, 75 edit the amplitude envelope).
 *
 * The cost line of the header (fw_mod_hdr_t cost) is tools/module/prophet.cost: the budget lines of the loops that module adds, summed by tools/build.py.
 *
 * Revisions:
 * (Orange Cat 0.12.2) the Prophet module fits 8 sectors (34,764 B measured): the firmware's PITCH_OCT and ENV_LIN through the table, the factory programs in a palette fo...
 *   0.12.0 (exp/sonnet-p5mod): first version, a measurement unit (no descriptor).
 *   0.12.1: the descriptor (P5_PAGES), the hooks, and the 8 sector placement; voice_was through the table (a note on read the module's own zero).
 *   0.12.2: PITCH_OCT and ENV_LIN through the table, the factory programs in the palette form (P5_FZ): 34,764 B measured on the device with the descriptor, 32,768 B hold.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00010000u
#include "build_options.h"
#include <stdint.h>
/* (0.12.2) PITCH_OCT and ENV_LIN are the firmware's, through the table (fw_api.h pitch_oct, env_lin): 1,280 B the 8 sectors cannot spare. melodee_tables.h defines both as
 * statics and defines pitch_inc over PITCH_OCT, so the header's names are moved aside here (the unused statics are dropped by the compiler) and both are written again below. */
#define PITCH_OCT PITCH_OCT_unused
#define ENV_LIN ENV_LIN_unused
#define pitch_inc pitch_inc_unused
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#define memcmp oc_mod_memcmp
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#undef PITCH_OCT
#undef ENV_LIN
#undef pitch_inc
static inline uint32_t pitch_inc(uint32_t p);       /* (defined below, over the table) */
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#include "fw_pages.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_fw;
#define OC_FW() (oc_host_fw)
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#endif
#include "oc_pack.h"

#define PITCH_OCT (OC_FW()->pitch_oct)
#define ENV_LIN (OC_FW()->env_lin)
static inline uint32_t pitch_inc(uint32_t p)         /* melodee_tables.h's, over the firmware's table */
{
    uint32_t i = p + 64u, o = (i * 21846u) >> 22;
    return PITCH_OCT[i - o * 192u] >> (10u - o);
}
#define voice_was (*OC_FW()->voice_was)       /* core.h's is a static of the firmware's unit; the module's own copy is never written (0.12.1: found by the host render test) */
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

/* ---------------------------------------------------------------------------------------------------------------- the pages (params.c's P5 rows, as data) */
#define PD(l, f, mn, mx, df) {l, f, mn, mx, df, 0, 0}          /* params.c's, for prophet_edit.h */
#define PE(l, n, df) {l, F_ENUM, 0, (int16_t)(sizeof(n) / sizeof(n[0]) - 1), df, n, 0}
static const char *const N_ONOFF[] = {"OFF", "ON"};
#include "prophet_edit.h"
#define PROW(title, a, b, c, d) {title, FAM_EDIT, SC_MOD, GR_NONE, {a, b, c, d}}
static const page_t P5_ROWS[] = {
    PROW("P5 OSC A", 0, 3, 4, 8),        PROW("P5 OSC B", 1, 2, 9, 12),       PROW("P5 B WAVES", 5, 6, 7, 11),     PROW("P5 MIXER", 14, 15, 16, 10),
    PROW("P5 FILTER", 17, 18, 19, 20),   PROW("P5 FLT ENV", 43, 45, 47, 49),  PROW("P5 AMP ENV", 44, 46, 48, 50),  PROW("P5 ENV MOD", 40, 41, 42, 51),
    PROW("P5 POLYMOD", 32, 33, 34, 35),  PROW("P5 MOD FLT", 36, 37, 38, 39),  PROW("P5 LFO", 21, 23, 24, 25),       PROW("P5 WHEEL", 22, 26, 27, 28),
    PROW("P5 WHEEL 2", 29, 30, 31, 13),  PROW("P5 UNISON", 52, 53, 54, 87),   PROW("P5 BEND", 86, 0xFF, 0xFF, 0xFF),
};
static uint8_t *p5m_native(track_t *t) { return p5_patch_of(t)->raw; }
static const param_desc_t *p5m_shown(const track_t *t, uint32_t id, int16_t *value)
{
    if (p5_keyed_tune(t, id)) {                          /* FREQ A, and FREQ B when it is keyed: audible semitones, not the raw 0..96 half steps */
        *value = (int16_t)(clamp(p5_patch_of(t)->raw[id], 0, 96) / 2 - 12);
        return &P5_TUNE_DISPLAY[id];
    }
    if (id == P5_BEND) { *value = (int16_t)(*value + 1); return &P5_BEND_DISPLAY; }
    if (id == P5_UNISON_COUNT && !*value) *value = 5;
    return 0;
}
static void p5m_put(track_t *t, uint32_t id, int32_t value) { p5_edit_value(t, id, p5_panel_raw(t, id, value)); }
static int p5m_active(const track_t *t, uint32_t id) { return p5_ed_active(t, id); }
static void p5m_loaded(track_t *t, uint32_t preset) { p5_preset_loaded(t, preset); }
static int32_t p5m_env_source(track_t *t, const voice_t *v) { return p5_env_source(t, (voice_t *)v); }
static int32_t p5m_bend(const track_t *t) { return (clamp(p5_patch_of(t)->raw[P5_BEND], 0, 11) + 1) * 256; }
/* the voice assignment is the track's; motion leaves it alone (motion.c's rule for the built-in Prophet) */
#define SKB(b) ((uint8_t)((((P5_UNISON >> 3) == (b)) ? 1u << (P5_UNISON & 7u) : 0u) | (((P5_UNISON_COUNT >> 3) == (b)) ? 1u << (P5_UNISON_COUNT & 7u) : 0u) | \
                          (((P5_UNISON_DETUNE >> 3) == (b)) ? 1u << (P5_UNISON_DETUNE & 7u) : 0u) | (((P5_RETRIGGER >> 3) == (b)) ? 1u << (P5_RETRIGGER & 7u) : 0u)))
static const uint8_t P5_SKIP[11] = {SKB(0), SKB(1), SKB(2), SKB(3), SKB(4), SKB(5), SKB(6), SKB(7), SKB(8), SKB(9), SKB(10)};
static const char *const P5_ENV_TITLES[] = {"P5 FLT ENV", "P5 AMP ENV", "P5 ENV MOD", 0};
static const char *const P5_LFO_TITLES[] = {"P5 LFO", "P5 WHEEL", 0};
static const fw_pages_t P5_PAGES = {
    FW_PAGES_MAGIC, sizeof(fw_pages_t), FW_PAGES_VERSION, (uint16_t)(sizeof P5_ROWS / sizeof P5_ROWS[0]), (uint16_t)NELEM(P5_PANEL), P5_ROWS, P5_PANEL,
    p5m_native, p5m_shown, p5m_put, p5m_active, P5_SKIP, P5_ENV_TITLES, P5_LFO_TITLES,
    {P5_CUTOFF, P5_RESONANCE, P5_ENV_FILTER, P5_RELEASE_AMP},                 /* HOME's four knobs (ui.c stage_page's P5_K) */
    p5m_loaded, p5m_env_source, p5m_bend,
    {P5_RELEASE_AMP, P5_ATTACK_AMP, P5_DECAY_AMP, 0xFF},                      /* CC 72, 73, 75 */
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_PROPHET, sizeof(p5_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_P5_TEST; }
const fw_pages_t *oc_host_module_pages(void) { return &P5_PAGES; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_PROPHET, sizeof(p5_part_t), (uint32_t)(uintptr_t)&ENG_P5_TEST, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
