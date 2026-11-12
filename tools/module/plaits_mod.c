/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MODELS (Release 3, the model picker; docs/candidates/model_picker.md): the module that adds models to the built-in MACRO (eng_macro.c): kind
 * FW_MOD_KIND_MODELS, "sound models for engine 8" (fw_models.h). This unit is the module's header and its table of descriptors: one weak pointer
 * a model, filled by the model's own unit (tools/module/plaits/mdl_*.c, tbl_*.c) when its region is in the image, null when the pick left it out
 * (the linker resolves an undefined weak reference to 0; the page's packer writes the same 0: tools/pick_module.py). The loader checks the
 * table against the image (fw_models_check) and hands it to the engine (engines.c eng_models_set); the engine reads a model's descriptor from
 * it and calls the model's start and render through the descriptor. Nothing here runs.
 *
 * The header's state_size is 0 here: the build (and the packer) write the largest state the pick's models need (pl_need over the descriptors),
 * which the loader checks and the engine's own block (PL_STATE_MAX) holds. The host unit (OC_MODULE_HOST, tests/module_host_plaits_tu.c) has every
 * model and hands the test harness its pointer table (oc_host_models).
 *
 * Revisions:
 *   macro2-candidate (model picker): first version, a study (the glue in the module, registered as engine 32).
 *   Release 3 (picker-r3): the glue is the firmware's; the module is the models and this table.
 */
#include "pl_mod_common.h"
#include "plaits/pl_models.h"                          /* the list (PL_N, the keys); no unit wanted here */
#ifdef OC_MODULE_HOST
#define PTR32(x) 0u                                    /* a host pointer is not a device address: the harness takes the pointer table from oc_host_models */
#else
#define PTR32(x) ((uint32_t)(uintptr_t)(x))
#endif
#define PL_MODEL_WEAK(id, key, name) extern const pl_mdl_t pl_mdl_##key __attribute__((weak));
#define PL_MODEL_ADDR(id, key, name) PTR32(&pl_mdl_##key),
#define PL_MODEL_HOSTPTR(id, key, name) &pl_mdl_##key,
PL_MODEL_LIST(PL_MODEL_WEAK)
#ifdef OC_MODULE_HOST
#include "pl_mod_host_units.h"                        /* (the host's one unit: the model units after the weak declarations, so each is a weak definition; the
                                                       * tables are pl_tables2.h's statics, every bank in: a bank a pick leaves out is a device matter) */
#else
extern const int16_t pl_wav_bank_0[] __attribute__((weak));
extern const int16_t pl_wav_bank_1[] __attribute__((weak));
extern const int16_t pl_wav_bank_2[] __attribute__((weak));
#endif
/* the table the loader reads (fw_models.h), kept whole in the link (.plaits_keep: the pick's link may drop every region that would name it) */
__attribute__((section(".plaits_keep"), used)) const fw_models_wire_t PL_MODELS = {
    FW_MODELS_MAGIC, FW_MODELS_WIRE, FW_MODELS_VERSION, PL_MDL_ABI_ID, ENGI_MACRO, PL_N,
    { PL_MODEL_LIST(PL_MODEL_ADDR) },
    { PTR32(pl_wav_bank_0), PTR32(pl_wav_bank_1), PTR32(pl_wav_bank_2) },
};
/* the part layout, for the build's manifest (tools/build.py reads the two words from the master image: the packer sizes a pick's state from them) */
__attribute__((section(".plaits_keep"), used)) const uint32_t pl_mod_meta[2] = {PL_PART_HEAD, PL_POLY};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO | FW_MOD_KIND_MODELS << 8, 0, 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
static const pl_mdl_t *const oc_host_models_tab[PL_N] = { PL_MODEL_LIST(PL_MODEL_HOSTPTR) };
static const void *const oc_host_banks_tab[3] = {   /* (a bank left out with -DPL_HOST_NO_wavN reads as absent to the engine's rule) */
#ifdef PL_HOST_NO_wav0
    0,
#else
    pl_wav_bank_0,
#endif
#ifdef PL_HOST_NO_wav1
    0,
#else
    pl_wav_bank_1,
#endif
#ifdef PL_HOST_NO_wav2
    0,
#else
    pl_wav_bank_2,
#endif
};
const void *const *oc_host_models(uint32_t *n) { if (n) *n = PL_N; return (const void *const *)oc_host_models_tab; }
const void *const *oc_host_banks(void) { return oc_host_banks_tab; }
uint32_t oc_host_models_state(void)                    /* what the device header gets for this set of units: the largest state a part needs */
{
    uint32_t i, mx = 0;
    for (i = 0; i < PL_N; i++)
        if (oc_host_models_tab[i] && pl_need(oc_host_models_tab[i]->state, oc_host_models_tab[i]->big) > mx) mx = pl_need(oc_host_models_tab[i]->state, oc_host_models_tab[i]->big);
    return mx;
}
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_MACRO | FW_MOD_KIND_MODELS << 8, 0, (uint32_t)(uintptr_t)&PL_MODELS, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
