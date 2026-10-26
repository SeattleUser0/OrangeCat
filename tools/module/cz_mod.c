/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: the CZ-1 (ENGI_CZ 15): firmware/src/eng_cz.c and cz_patch.h (Copyright (C) 2026 Kerem Kilic (Ellic Studio), GPL-3.0-only), its native
 * voice firmware/src/cz_native.c (BSD-3-Clause: Copyright (c) Devin Acker, the uPD933 model; modifications Copyright (C) 2026 Kerem Kilic (Ellic
 * Studio); LICENSES/BSD-3-Clause-uPD933.txt), the phase family's shared state in firmware/src/eng_phase.c (Kerem Kilic; portions Leo Kuroshita
 * (@kurogedelic), Hügelton Instruments), and Casio's 64 CZ-1 preset tones (build/gen/melodee_cz1.h, tools/gen_cz1_factory.py). Retired from the image
 * (MELODEE_CZ=0, tools/orangecat_engines.sh) and kept in the tree; all of it included here as it is.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "CZ-1"): the CZ-1 as a loadable module translation unit, as WHEEL came
 * back (wheel_mod.c), registrable in its own slot, 15, in an OC_CANDIDATES build (engines.c eng_module_slot). The voice is the whole native one: two
 * lines, each a phase-distortion oscillator (eight waves, the second wave every other cycle, a window) with its DCO, DCW and DCA envelopes of eight
 * rate and level points (the uPD933's laws), key follow, velocity, vibrato, line modes 1, 2, 1+2, 1+1', ring and noise modulation, detune.
 *
 * What is the module's own and what is the firmware's:
 *   - the tone. The built-in engine plays the firmware's cz_patch[track] (144 bytes in the pool, which projects save, the CZ pages edit, the CZ SysEx
 *     and the editor's CZ commands read and write). A module cannot reach it (no member of fw_api_t: adding one changes the interface of every
 *     module), so the module keeps each part's tone in its own state (cz_mod_part_t), read through eng_phase.c's CZ_TONE (the one line the firmware's
 *     sources gained for this: their CZ_TONE is cz_patch[track], so the built-in engine is unchanged). The tone follows BANK and PTCH (P_E0, P_E1), as
 *     the built-in engine's cz_factory_loaded and cz_bank_poll do: at each block, when they changed (or the part's state is not the module's), the
 *     tone is Casio's preset tone BANK x 16 + PTCH - 1 (banks A..D: the 64), or the INIT tone (PTCH 0, or banks E..H, which the built-in engine
 *     reads from user banks the module does not have);
 *   - its tables: Casio's tones, packed (CZ_DICT, CZ_LZ: 5,389 B), the amplitude law (CZ_AMP), A4_LOG (the tuning, below), in the module's flash, read
 *     at a key or tone change or once a block; FM6_SIN (1,025 floats), read every sample by the oscillator, is RAM in the firmware (dsp_tables_init
 *     fills it at boot): the module carries it as constants (tools/module/cz/cz_fm6_sin.h, gen_cz_fm6_sin.py, checked against the firmware's by
 *     tests/module_host_cz_test.c) and declares it its hot set (4,100 B of arena);
 *   - the tuning: cz_native_inc reads tuning_a4 (Hz), the firmware's, not reachable: tools/module/fw_tuning.h finds it from tuning_log();
 *   - voice_was through the table (as the Prophet module); memcpy, memset, memcmp, the arena the table's;
 *   - the matrix's ENV: voice.c reads the built-in CZ-1's DCA1 (phase_env_source) for &ENG_CZ; the module brings it as a descriptor's env_source
 *     (CZ_M_PAGES: one hidden row, the hook voice.c reads at the same point for any module).
 * Not in the module (the gate's "controls that survive" row): its pages. The CZ-1's pages are 39 rows over 137 parameters (cz_edit.h, params.c
 * SC_CZ1); a module's page descriptor (fw_pages.h) holds 24 rows and 128 parameters, so they do not fit it without a change of that format; and an
 * edit would last only while the track stays on the CZ-1 (the part's state is the arena's). Without pages: the 64 tones and INIT, BANK and PTCH on
 * EDIT 1, TONE (CZ_NATIVE) fixed. The user banks (cz_bank.c: the old CZ bank objects, now the module area's sectors), the tone in projects, the CZ
 * SysEx (cz_store.c) and the editor's CZ commands (editor_cz.c) are the firmware's and stay off (MELODEE_CZ=0).
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a measurement, not a release */
#include "build_options.h"
#include <stdint.h>
#define FM6_SIN FM6_SIN_ram_unused          /* melodee_tables.h's RAM table (filled at the firmware's boot): moved aside, the module's constants below */
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#define memcmp oc_mod_memcmp
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#undef FM6_SIN
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
int memcmp(const void *a, const void *b, __SIZE_TYPE__ n)
{
    const uint8_t *x = a, *y = b;
    for (__SIZE_TYPE__ i = 0; i < n; i++) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}
static void str_cpy(char *d, const char *s, unsigned max)   /* libc.c's (the preset names) */
{
    unsigned i = 0;
    while (s[i] && i + 1u < max) { d[i] = s[i]; i++; }
    d[i] = 0;
}
#include "fw_tuning.h"
#define tuning_a4 (oc_mod_a4())
#include "cz/cz_fm6_sin.h"
static const eng_hot_t CZ_HOT[1] = { { CZ_FM6_SIN, sizeof CZ_FM6_SIN } };
#define FM6_SIN ((const float *)ENG_HOT(0, ENGI_CZ, 0))

/* a part's state: the built-in engine's (eng_phase.c cz_part_t), the tone it plays, and which preset tone that is */
#include "cz_patch.h"
static cz_patch_t *cz_mod_tone(const track_t *t);
#define CZ_TONE(t) (*cz_mod_tone(t))
#include "eng_phase.c"
#include "eng_cz.c"
#define CZ_MOD_MAGIC 0x315A434Du           /* "MCZ1": the part's state is the module's (else another engine's, or new) */
typedef struct {
    cz_part_t cz;
    cz_patch_t tone;
    uint32_t magic;
    uint16_t pick;                         /* BANK x 17 + PTCH the tone was taken for */
} cz_mod_part_t;
static cz_mod_part_t *cz_mod_part_of(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(cz_mod_part_t)); }
static cz_patch_t *cz_mod_tone(const track_t *t) { return &cz_mod_part_of(t)->tone; }
static cz_part_t *cz_part(uint32_t part) { return &((cz_mod_part_t *)resource_get(OC_FW()->res_engine0 + part % NPART, sizeof(cz_mod_part_t)))->cz; }

/* the tone follows BANK and PTCH (eng_cz.c cz_factory_loaded, cz_bank_poll), then the built-in engine's block */
static void cz_mod_block(track_t *t)
{
    cz_mod_part_t *P = cz_mod_part_of(t);
    uint32_t pick = (uint32_t)t->p[P_E0] * 17u + (uint32_t)t->p[P_E1];
    if (!P)
        return;
    if (P->magic != CZ_MOD_MAGIC || P->pick != pick) {
        uint32_t k = (uint32_t)t->p[P_E0] * 16u + (uint32_t)t->p[P_E1] - 1u;
        if (t->p[P_E1] > 0 && k < CZ_FACTORY_N)
            cz_factory(k, P->tone.raw);
        else
            cz_patch_init(P->tone.raw);
        P->cz.native_valid = 0;            /* (the envelopes' definitions from this tone at the block below) */
        P->magic = CZ_MOD_MAGIC;
        P->pick = (uint16_t)pick;
    }
    cz_native_block(t);
}
#include "fw_pages.h"
/* a descriptor for one hook, env_source (voice.c: the matrix's ENV source), as the built-in engine has it: the loader needs a row and a
 * panel entry (fw_pages_check); engine CZ-1 is not one of params.c's MODPAGE_ENG, so they are never shown */
static const page_t CZ_M_ROWS[1] = { {"CZ-1", FAM_EDIT, SC_MOD, GR_NONE, {0xFF, 0xFF, 0xFF, 0xFF}} };
static const param_desc_t CZ_M_PANEL[1] = { {"-", F_INT, 0, 0, 0, 0, 0} };
static int32_t cz_m_env_source(track_t *t, const voice_t *v) { return phase_env_source(t, (voice_t *)v); }
static const fw_pages_t CZ_M_PAGES = {
    .magic = FW_PAGES_MAGIC, .size = sizeof(fw_pages_t), .version = FW_PAGES_VERSION, .npages = 1, .npanel = 1, .rows = CZ_M_ROWS, .panel = CZ_M_PANEL,
    .knob = {0xFF, 0xFF, 0xFF, 0xFF}, .env_source = cz_m_env_source, .cc = {0xFF, 0xFF, 0xFF, 0xFF},
};
static const engine_t ENG_CZ_MOD = {       /* eng_cz.c's ENG_CZ, with the module's block, hot set and the descriptor of its matrix ENV */
    .name = "CZ-1", .page_title = {"CZ-1", "TONE"},
    .edit = {
        {"BANK", F_INT, 0, 7, 0, 0, 0}, {"PTCH", F_INT, 0, 16, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"-", F_INT, 0, 0, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0}, {"TONE", F_INT, CZ_NATIVE, CZ_NATIVE, CZ_NATIVE, 0, 0},
    },
    .preset_make = cz_preset_make, .npresets = CZ_FACTORY_N + 1u,
    .ownenv = 1, .done = cz_native_done, .keep = 0x0fu,
    .note_on = cz_native_note_on, .render = cz_native_render, .block = cz_mod_block,
    .knob = {P_E0, P_E1, P_E7, P_E7},
    .hot = CZ_HOT, .nhot = 1,
    .pages = &CZ_M_PAGES,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_CZ, sizeof(cz_mod_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_CZ_MOD; }
const float *oc_host_cz_fm6_sin(void) { return CZ_FM6_SIN; }   /* (tests/module_host_cz_test.c: against the firmware's FM6_SIN) */
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_CZ, sizeof(cz_mod_part_t), (uint32_t)(uintptr_t)&ENG_CZ_MOD, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
