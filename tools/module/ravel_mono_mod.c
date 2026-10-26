/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Ravel's MONO (https://github.com/De-Pattern/Ravel_FM1, commit e9b6398, GPL-3.0-only; the files carry only their SPDX line, the author is
 * De-Pattern, the repository's owner): its oscillator (firmware/app/rv_synth.c's MONO section, mono_t from rv_int.h: tools/module/ravel/rv_mono.c and rv_mono.h,
 * extracts) and its SH-101 filter (firmware/app/rv_dsp.h: sh101_tick, ladder_set; with rv_math.h, Ravel's float maths: unchanged copies in
 * tools/module/ravel/). Ravel has not been run on an FM-1 yet (its README says so): its sound is the emulator's and the simulator's.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Ravel"): MONO (after the SH-101 played like a wind instrument: one
 * oscillator, saw, pulse and a square sub from one phase, noise; a detached note scoops up into pitch, a legato one keeps going; the vibrato fades in
 * after its delay; a slow drift; the IR3109-style four-pole filter, its resonance through a soft curve) as a loadable module translation unit
 * (stage 0). The proposed engine number is 42 (core.h ENGI_RAVEL_MONO; registrable only in an OC_CANDIDATES build).
 *
 * Ravel's MONO is not an engine of its own in Ravel: it is the oscillator section of a synth track whose filter, envelopes and glide are the
 * track's (rv_synth.c synth_voice_render, voice_post). The module is that voice put together on Orange Cat's: Ravel's mono_render and sh101_tick
 * as they are, and around them Orange Cat's ADSR (the amplitude; Ravel's articulation stages are not ported), its ENV -> FILTER and LFO on the
 * cutoff (m->cutoff, Ravel's FILTER ENV), its glide and legato (voice.c: a legato note calls the engine's legato hook, Ravel's mono_note(.., 1)).
 * The cutoff as Ravel figures it for MONO: CUT + the envelope + the key (KTRK 127: the note's distance from C4, 127 / 120 a semitone) + VEL>CUT
 * x (velocity - 0.5); after the filter Ravel's 2 sat(y / 2) and MONO's level trim (0.646, rv_synth.c eng_trim).
 *   - EDIT 1 (OSC: SAW PULS SUB PW), EDIT 2 (VCF: CUT RES VIB VDLY): eight of Ravel's sixteen. Fixed at Ravel's MONO defaults (rv_params.c SRC_MONO,
 *     its track defaults): SUB OCT -1 octave, NOISE 0, PWM 0, SCOOP 20, SCOOP T 40, VIB RATE 70, VEL>CUT 40, DRIFT 20, RETRIG (legato keeps going),
 *     DRIVE 30 (it scales the mix, as Ravel's mono_render does);
 *   - state: the part's arena block (ravel_mono_part_t: Ravel's voice fields MONO reads, mono_t's 32 bytes, the filter's state); poly 1;
 *   - float throughout (the FPU), Ravel's own maths (rv_math.h: no libm); per sample no table (fm_sin_turns once a block): no hot set.
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
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* Ravel's pieces, their names apart from Orange Cat's while they are included */
#define blep rv_blep
#define voice_t rv_voice_t
#define ladder_t rv_ladder_t
#include "ravel/rv_dsp.h"
enum { TP_S0, TP_S1, TP_S2, TP_S3, TP_S4, TP_S5, TP_S6, TP_S7, TP_S8, TP_S9, TP_S10, TP_S11, TP_S12, TP_S13, TP_S14, TP_S15, TP_SN };   /* Ravel's
                                                                                   * engine parameters (rv.h), the ones mono_render reads */
#include "ravel/rv_mono.h"
typedef struct { uint32_t rng; union { mono_t mo; } e; } rv_voice_t;   /* what of Ravel's voice_t MONO reads */
#include "ravel/rv_mono.c"
#undef blep
#undef voice_t
_Static_assert(sizeof(mono_t) == 32u, "Ravel's mono_t");

typedef struct {
    rv_voice_t rv;                         /* Ravel's voice: its rng and MONO's state */
    rv_ladder_t lad;                       /* the SH-101 filter */
} ravel_mono_part_t;
static ravel_mono_part_t *rmono_part(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(ravel_mono_part_t)); }
#undef ladder_t
/* Ravel's parameter array for MONO: the eight controls and the fixed rest (rv_params.c SRC_MONO's defaults) */
static void rmono_eff(const track_t *t, float *eff)
{
    const int16_t *p = t->p;
    static const int8_t FIX[TP_SN] = {0, 0, 0, 0, 0, 64, 0, 20, 40, 10, 70, 50, 40, 20, 0, 30};
    uint32_t k;
    for (k = 0; k < TP_SN; k++) eff[k] = FIX[k];
    eff[TP_S0] = clamp(p[P_E0], 0, 127);   /* SAW */
    eff[TP_S1] = clamp(p[P_E1], 0, 127);   /* PULSE */
    eff[TP_S2] = clamp(p[P_E2], 0, 127);   /* SUB */
    eff[TP_S5] = clamp(p[P_E3], 0, 127);   /* PW */
    eff[TP_S9] = clamp(p[P_E6], 0, 127);   /* VIBRATO */
    eff[TP_S11] = clamp(p[P_E7], 0, 127);  /* VIB DLY */
}
static void rmono_note_on(track_t *t, voice_t *v)
{
    ravel_mono_part_t *P = rmono_part(t);
    float eff[TP_SN];
    if (!P) return;
    if (!v->env && !v->env_out) {          /* from silence: Ravel's voice_engine_reset and its filter cleared */
        memset(P, 0, sizeof *P);
        P->rv.rng = 0x9E3779B9u ^ (uint32_t)v->note * 2654435761u;
    }
    rmono_eff(t, eff);
    mono_note(&P->rv, eff, 0);
}
static void rmono_legato(track_t *t, voice_t *v)
{
    ravel_mono_part_t *P = rmono_part(t);
    float eff[TP_SN];
    (void)v;
    if (!P) return;
    rmono_eff(t, eff);
    mono_note(&P->rv, eff, 1);
}
static void rmono_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    ravel_mono_part_t *P = rmono_part(t);
    float eff[TP_SN], buf[CTL], f, cut;
    uint32_t i;
    if (!P || n > CTL) return;
    rmono_eff(t, eff);
    f = (float)m->inc * (44100.0f / 4294967296.0f);
    mono_render(&P->rv, eff, f, buf, (int)n);
    cut = (float)clamp(t->p[P_E4], 0, 127) + (float)m->cutoff * (1.0f / 256.0f) + ((float)v->note - 60.0f) * (127.0f / 120.0f)
          + eff[TP_S12] * (70.0f / 127.0f) * ((float)v->vel * (1.0f / 127.0f) - 0.5f);
    ladder_set(&P->lad, rv_cut_hz_i(clampf(cut, 0.0f, 135.0f)), (float)clamp(t->p[P_E5], 0, 127) * (1.0f / 127.0f));
    for (i = 0; i < n; i++) {
        float y = sh101_tick(&P->lad, buf[i]);
        y = 2.0f * sat(y * 0.5f) * 0.646f;
        out[i] += voice_amp((int32_t)(clampf(y, -1.0f, 1.0f) * 32767.0f), m, i);
    }
}

static const preset_t RMONO_PRESETS[] = {
    /* {SAW PULS SUB PW, CUT RES VIB VDLY}, {A D S R}, ENV -> FILTER, mono (Ravel's MONO track defaults: ATK 50 .. ; FILTER ENV 35) */
    {"MONO", {127, 0, 8, 64, 73, 15, 10, 50}, {50, 82, 61, 72}, 35, 1},
    {"101 BASS", {80, 60, 90, 40, 58, 64, 0, 50}, {0, 45, 40, 20}, 40, 1},       /* Ravel's p_101bass: CUT 58, RES 64 */
    {"BREATH LEAD", {127, 20, 30, 64, 96, 40, 40, 70}, {40, 80, 90, 60}, 20, 1},  /* a wind player's: the delayed vibrato */
    {"SQUARE HORN", {0, 127, 40, 64, 80, 30, 30, 60}, {30, 70, 100, 50}, 30, 1},
};
static const engine_t ENG_RAVEL_MONO = {
    .name = "MONO", .page_title = {"OSC", "VCF"},
    .edit = {
        {"SAW", F_PCT, 0, 127, 127, 0, 0},
        {"PULS", F_PCT, 0, 127, 0, 0, 0},
        {"SUB", F_PCT, 0, 127, 8, 0, 0},
        {"PW", F_PCT, 0, 127, 64, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 73, 0, 0},
        {"RES", F_PCT, 0, 127, 15, 0, 0},
        {"VIB", F_PCT, 0, 127, 10, 0, 0},
        {"VDLY", F_PCT, 0, 127, 50, 0, 0},
    },
    .presets = RMONO_PRESETS, .npresets = NELEM(RMONO_PRESETS),
    .knob = {P_E4, P_E5, P_E6, P_E7}, .poly = 1,
    .note_on = rmono_note_on, .render = rmono_render, .legato = rmono_legato,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_RAVEL_MONO, sizeof(ravel_mono_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_RAVEL_MONO; }
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR,
    ENGI_RAVEL_MONO, sizeof(ravel_mono_part_t), (uint32_t)(uintptr_t)&ENG_RAVEL_MONO, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
