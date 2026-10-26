/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: SID (engine 3), firmware/src/eng_sid.c (Copyright (C) 2026 Kerem Kilic (Ellic Studio), GPL-3.0-only): the MOS 6581 and 8580 at the
 * register level as Dag Lem's reSID models them (reSID 0.16 and 1.0, GPL-2.0-or-later: its measured curves, ladders and combined waveforms are
 * tables of tools/gen_tables.py sid_tables, from assets/resid). Retired from the image (MELODEE_SID=0, tools/orangecat_engines.sh) and kept in the
 * tree; included here as it is.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "SID"): SID as a loadable module translation unit, in its own slot, 3,
 * registrable in an OC_CANDIDATES build (engines.c eng_module_slot). The engine is a whole chip per note: three voices (oscillators, the noise
 * register, sync and ring), the R-2R DACs, the envelope generator's counters, the filter on the measured cutoff curve, the C64's output stage; STACK
 * sets the three voices up (unison stacks, SYNC, RING, SUB); MODEL 6581 or 8580, or C64 6581 / 8580 (one voice a track, registers written once a
 * PAL frame, the player's hard restart and, in POLY, the chord as a frame arpeggio).
 *
 * What is the module's own and what is the firmware's:
 *   - its state: sid_part_t (four chips' state and the part's unpacked combined waveform, 4,096 B), one arena block a track (sid_part, here);
 *   - its tables: per sample the waveform DAC (SID_WDAC, 512 B) and the envelope DAC (SID_EDAC, 128 B): its hot set (640 B of arena); the combined
 *     waveforms are unpacked into the part at a waveform or model change (SID_WAVE_LZ, 4,277 B packed, from flash once); the filter's cutoff curve,
 *     resonance, the ADSR rates at a parameter change or once a block;
 *   - tuning_a4 through tools/module/fw_tuning.h; voice_was through the table;
 *   - ENV3: the built-in engine's envelope counter is voice.c's ENV for ENV DEST (pitch, cutoff, shape) and the matrix (sid_env_q15, called for
 *     &ENG_SID); the module brings it as a descriptor's env_source (SID_M_PAGES: one hidden row), which voice.c reads for engine 3 at the same
 *     point in an OC_CANDIDATES build (the one line voice.c gained);
 *   - voice_scale_pitch (voice.c), which the C64 chord's arpeggio calls for each next note: the firmware's applies the track's microtonal scale
 *     (micro_active, micro_pitch), which the module cannot reach; the module's sets the plain note (pitch16 = note x 16), so the C64 POLY arpeggio is
 *     12-TET under a microtonal scale (every other note's pitch is voice.c's, as before).
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a measurement, not a release */
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
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
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }
#define trk (OC_FW()->trk)
#define voice_was (*OC_FW()->voice_was)
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }
#include "fw_tuning.h"
#define tuning_a4 (oc_mod_a4())
#define ENGI_SID_MOD 3u                    /* (core.h names no ENGI_ for SID: engines.c's 3) */
static const eng_hot_t SID_HOT[2] = { { SID_WDAC, sizeof SID_WDAC }, { SID_EDAC, sizeof SID_EDAC } };
#define SID_WDAC ((const int16_t (*)[2][64])ENG_HOT(0, ENGI_SID_MOD, 0))
#define SID_EDAC ((const int16_t (*)[2][16])ENG_HOT(0, ENGI_SID_MOD, 1))

#include "eng_sid.c"
static sid_part_t *sid_part(uint32_t part) { return resource_get(OC_FW()->res_engine0 + part % NPART, sizeof(sid_part_t)); }
static void voice_scale_pitch(const track_t *t, voice_t *v, uint32_t note)   /* voice.c's, without the microtonal scale (above) */
{
    (void)t;
    v->pitch16 = (int32_t)note * 16;
    v->scale_fine = 0;
}
#include "fw_pages.h"
/* a descriptor for one hook, env_source (voice.c: the matrix's ENV source and, for SID, ENV DEST), as the built-in engine has it: the loader needs a row and a
 * panel entry (fw_pages_check); engine SID is not one of params.c's MODPAGE_ENG, so they are never shown */
static const page_t SID_M_ROWS[1] = { {"SID", FAM_EDIT, SC_MOD, GR_NONE, {0xFF, 0xFF, 0xFF, 0xFF}} };
static const param_desc_t SID_M_PANEL[1] = { {"-", F_INT, 0, 0, 0, 0, 0} };
static int32_t sid_m_env_source(track_t *t, const voice_t *v) { return sid_env_q15(t, (voice_t *)v); }
static const fw_pages_t SID_M_PAGES = {
    .magic = FW_PAGES_MAGIC, .size = sizeof(fw_pages_t), .version = FW_PAGES_VERSION, .npages = 1, .npanel = 1, .rows = SID_M_ROWS, .panel = SID_M_PANEL,
    .knob = {0xFF, 0xFF, 0xFF, 0xFF}, .env_source = sid_m_env_source, .cc = {0xFF, 0xFF, 0xFF, 0xFF},
};
static const engine_t ENG_SID_MOD = {      /* eng_sid.c's ENG_SID, with its hot set and the descriptor of its ENV3 */
    .name = "SID",
    .page_title = {"CHIP", "FILTER"},
    .edit = {
        {"MODEL", F_ENUM, 0, 3, 0, N_SID_MODEL, 0},
        {"WAVE", F_ENUM, 0, 7, 1, N_SID_WAVE, 0},
        {"STACK", F_ENUM, 0, 7, 0, N_SID_STACK, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0},
        {"CUT", F_PCT, 0, 127, 127, 0, 0},
        {"RES", F_INT, 0, 15, 0, 0, 0},
        {"MODE", F_ENUM, 0, 7, 0, N_SID_MODE, 0},
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = SID_PRESETS,
    .npresets = NELEM(SID_PRESETS),
    .note_on = sid_note_on,
    .render = sid_render, .block = sid_block,
    .knob = {P_E1, P_E3, P_E4, P_E5},
    .poly = SID_POLY,
    .cap = sid_cap,
    .ownenv = 1,
    .done = sid_done,
    .hot = SID_HOT, .nhot = 2,
    .pages = &SID_M_PAGES,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_SID_MOD, sizeof(sid_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_SID_MOD; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_SID_MOD, sizeof(sid_part_t), (uint32_t)(uintptr_t)&ENG_SID_MOD, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
