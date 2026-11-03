/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS, the model picker's module (docs/candidates/model_picker.md): what every unit of the module shares. The module is built from
 * many units (tools/module/plaits/mdl_*.c, one a model; tbl_*.c, one a wave or DX7 bank; eng_plaits.c, the glue), each a region of the
 * one image the build links (tools/build.py MODULES "plaits"), so that the installer page can leave the models the user did not pick out
 * of the image it installs (tools/pick_module.py, web/fm1pick.js). The rule that makes that possible: a unit reaches another only through
 * absolute pointers (the glue's PL_MODELS table of weak descriptors, the bank tables of pl_tables2.h), never through a call the compiler
 * resolves itself; so every unit includes this header, which brings the firmware's services (fw_api.h, as tools/module/macro2_mod.c), the
 * asset pack's tables (pl_tables.h, at their pack addresses), the module's own tables in split mode (pl_tables2.h: PL_TABLES2_SPLIT, a weak
 * extern each unless the unit defines it) and MACRO's hot set (eng_macro.c MC_HOT_LIST, the same indices: a module registered in MACRO's
 * slot reads MACRO's arena copies).
 *
 * Host build (OC_MODULE_HOST, tests/module_host_plaits_tu.c): one translation unit includes every unit; the weak externs are then defined.
 */
#pragma once
#define OC_MODULE 1
/* core.h defines the boot guard record (bootguard, .noinit) in every unit that includes it: a name of the unit's own, so the link sees no clash
 * (PL_UNIT: tools/build.py passes -DPL_UNIT=<unit>; the host's one unit is "host") */
#ifndef PL_UNIT
#define PL_UNIT host
#endif
#define PL_CAT3_(a, b, c) a##b##c
#define PL_CAT3(a, b, c) PL_CAT3_(a, b, c)
#define bootguard PL_CAT3(pl_, PL_UNIT, _bootguard)
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a candidate */
#include "build_options.h"
#include <stdint.h>
/* memcpy and memset: the firmware's, through the table, defined once in the glue's unit (plaits_mod.c). A struct copy the compiler turns into
 * a memcpy call would be a PC-relative call into the glue's region from any other unit: the one thing a region may not do, so the build refuses
 * a model unit that imports them (tools/build.py: a model unit has no hard import at all), and such a copy is written out by hand there. (The
 * host's one unit has them under other names: the test harness has libc's.) */
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
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }   /* dsp.c: SINE and TANH_Q15, the firmware's (its hot set) */

#define trk (OC_FW()->trk)
#define tuning_log() (OC_FW()->tuning_log())
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))

/* plaits' shared random generator (pl_dsp.h pl_rng_state) is .data; a module has none: the word is the firmware's rng_cell (fw_api_t) */
#define PL_RNG_EXTERN 1
#define pl_rng_state (*OC_FW()->rng_cell)
#include "audio_profile.h"
typedef void (*audio_batch_fn)(const void *data, int32_t *pcm);
#define AUDIO_BATCH_BYTES 168u
#define audio_batch_active (*OC_FW()->batch_active)
#define audio_batch_push(run, data, bytes, out, n, id) (OC_FW()->batch_push((run), (data), (bytes), (out), (n), (id)))

#include "plaits/pl_tables.h"
#ifndef OC_MODULE_HOST
#define PL_TABLES2_SPLIT 1
#endif
#include "plaits/pl_tables2.h"

/* MACRO's hot set (eng_macro.c MC_HOT_LIST), the same tables in the same order: the module reads the arena copies through the services table.
 * Registered under its own number (the candidate build, ENGI_MACRO2) it gets its own copies; registered as MACRO's extension (the design,
 * model_picker.md section 4) it reads MACRO's. PL_HOT_ENGINE says which. */
#ifndef PL_HOT_ENGINE
#define PL_HOT_ENGINE ENGI_MACRO2
#endif
#define PL_HOT_LIST(X) X(pl_lut_pitch_ratio_high, PL_LUT_PITCH_RATIO_HIGH_SIZE * 4u, float) X(pl_lut_pitch_ratio_low, PL_LUT_PITCH_RATIO_LOW_SIZE * 4u, float) \
    X(pl_lut_sine, PL_LUT_SINE_SIZE * 4u, float) X(pl_lut_fold, PL_LUT_FOLD_SIZE * 4u, float) X(pl_lut_fold_2, PL_LUT_FOLD_2_SIZE * 4u, float) \
    X(pl_lut_4x_downsampler_fir, PL_LUT_4X_DOWNSAMPLER_FIR_SIZE * 4u, float) X(pl_chord_waves, 15u * 132u * 2u, int16_t) \
    X(pl_lut_ws_inverse_tan, PL_LUT_WS_INVERSE_TAN_SIZE * 2u, int16_t) X(pl_lut_ws_inverse_sin, PL_LUT_WS_INVERSE_SIN_SIZE * 2u, int16_t) \
    X(pl_lut_ws_linear, PL_LUT_WS_LINEAR_SIZE * 2u, int16_t) X(pl_lut_ws_bump, PL_LUT_WS_BUMP_SIZE * 2u, int16_t) \
    X(pl_lut_ws_double_bump, PL_LUT_WS_DOUBLE_BUMP_SIZE * 2u, int16_t)
#define PL_HOT_IDX(t, b, ty) PLH_##t,
#define PL_HOT_ENTRY(t, b, ty) {t, b},
enum { PL_HOT_LIST(PL_HOT_IDX) PL_HOT_N };
static const eng_hot_t PL_HOT[PL_HOT_N] __attribute__((unused)) = { PL_HOT_LIST(PL_HOT_ENTRY) };   /* (the sources: the pack's addresses; the glue's engine_t names it) */
#ifndef PL_NO_HOT                                     /* (tools/model_inventory.py: the tables read as sections, not through the services table) */
#undef pl_lut_pitch_ratio_high
#undef pl_lut_pitch_ratio_low
#undef pl_lut_sine
#undef pl_lut_fold
#undef pl_lut_fold_2
#undef pl_lut_4x_downsampler_fir
#undef pl_chord_waves
#define pl_lut_pitch_ratio_high ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_pitch_ratio_high))
#define pl_lut_pitch_ratio_low ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_pitch_ratio_low))
#define pl_lut_sine ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_sine))
#define pl_lut_fold ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_fold))
#define pl_lut_fold_2 ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_fold_2))
#define pl_lut_4x_downsampler_fir ((const float *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_4x_downsampler_fir))
#define pl_chord_waves ((const int16_t *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_chord_waves))
#define PL_WS_CURVE(k) ((const int16_t *)ENG_HOT(0, PL_HOT_ENGINE, PLH_pl_lut_ws_inverse_tan + ((k) > 4 ? 4 : (k))))
#endif
#include "plaits/pl_dsp.h"
#include "plaits/pl_dsp2.h"
#include "plaits/pl_osc.h"
#include "pl_mdl.h"
