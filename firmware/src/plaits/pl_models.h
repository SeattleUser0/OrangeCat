/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (Release 3, the model picker) The 25 models of MACRO in one list, their stable ids, and each model's unit: its descriptor (pl_mdl.h) with the
 * model's levels, flags and voice budget units, defined once here for the firmware (eng_macro.c: the built-in models) and for the MODELS module
 * (tools/module/plaits/mdl_*.c, tbl_*.c: one unit a model, a region each). The includer says which units it wants, one macro a unit
 * (PL_WANT_<key> 1; an unset one is 0), and gets pl_mdl_<key>_ptr for every model: its descriptor's address, or 0 for a model it did not want.
 *
 * The ids (stable: a project stores the id in P_E0, a preset's e[0] is the id; MACRO's 0..14 are MACRO's own numbers, so a stored MACRO sound
 * keeps its model): 0 VA 1 WAVSHP 2 FM 3 GRAIN 4 ADDTV 5 CHORD 6 SWARM 7 NOISE 8 PARTCL 9 STRING 10 MODAL 11 KICK 12 SNARE 13 HI-HAT 14 HARP
 * 15 VA-VCF 16 PD 17 6OP A 18 6OP B 19 6OP C 20 TERRAN 21 STRMCH 22 CHIP 23 WAVTBL 24 SPEECH. The pick items past the models: 25, 26, 27 the
 * wave banks A, B, C (WAVTBL needs one at least; TERRAN's wave terrains use the ones present).
 *
 * THE UNITS (engine_t.units: what a voice of the model takes of the voice budget, voice.c): the rule, from the unit's measured costs (BUILD.md
 * 7.29: each model alone through the packed module, one preset a model, the four-note chord on every step, one and four tracks, 20 s at 120 BPM;
 * hw/LOG.md "MACRO by model", Flash 12 and 14, for the fifteen of MACRO). A half is 2,901 us (128 frames at 44.1 kHz), the budget 18 units,
 * so a unit stands for 161 us. The half also carries the idle load (460 us) and 120 us a track; with four tracks that leaves 1,961 us for
 * voices. Per voice, a model costs p us: the first track's worst half less the 580 us of idle and the one track, over the voices that
 * preset sounds (one for a mono preset or a large model, else the four of the chord as the measured units admit them), and the cost a fourth
 * track adds less its 120 us; the larger of the two, and for MACRO's fifteen the larger again of hw/LOG.md's own per-voice figures (Flash 12's
 * with Flash 14's hot-set gain). The most voices of the model a half can render beside a steal's fade (an eighth of a voice, the cost caps'
 * eighth) is n = floor((1,961 - p / 8) / p), and the units are the smallest U with floor(18 / U) <= n: four tracks of four-note chords then
 * never render a late half, the budget steals down to n voices instead. ADDTV, MODAL and the three 6OP banks hold four tracks that way
 * (n = 2, 1 and 2). The per-voice costs and the units, old -> new (the olds: eng_macro.c MC_UNITS for 0..14, macro2.md's proposals for the rest):
 *   VA 313 us 4 -> 3 | WAVSHP 352 5 -> 4 | FM 462 8 -> 4 | GRAIN 258 4 -> 3 | ADDTV 765 9 -> 7 | CHORD 250 3 -> 3 | SWARM 385 5 -> 4 | NOISE 184 2 -> 2
 *   PARTCL 327 4 -> 4 | STRING 408 4 -> 4 | MODAL 950 9 -> 10 | KICK 262 3 -> 3 | SNARE 340 4 -> 4 | HI-HAT 320 3 -> 3 | HARP 234 4 -> 3
 *   VA-VCF 164 5 -> 2 | PD 395 5 -> 4 | 6OP A/B/C 705 6 -> 7 | TERRAN 209 7 -> 2 | STRMCH 241 5 -> 3 | CHIP 103 2 -> 1 | WAVTBL 350 8 -> 4 | SPEECH 237 5 -> 3
 * (docs/candidates/model_picker.md "The units" has the table with every estimate; Session A measures four tracks of each heavy model at these.) */
#ifndef PL_MODELS_LIST_H                               /* the list, once; the unit blocks below run at every inclusion (the host's one translation unit includes
#define PL_MODELS_LIST_H                               * this header once per model unit), each consuming the includer's PL_WANT_<key> */
#define PL_MODEL_LIST(X) X(0, va, "VA") X(1, wavshp, "WAVSHP") X(2, fm, "FM") X(3, grain, "GRAIN") X(4, addtv, "ADDTV") X(5, chord, "CHORD") \
    X(6, swarm, "SWARM") X(7, noise, "NOISE") X(8, partcl, "PARTCL") X(9, string, "STRING") X(10, modal, "MODAL") X(11, kick, "KICK") \
    X(12, snare, "SNARE") X(13, hihat, "HI-HAT") X(14, harp, "HARP") X(15, vavcf, "VA-VCF") X(16, pd, "PD") X(17, 6opa, "6OP A") \
    X(18, 6opb, "6OP B") X(19, 6opc, "6OP C") X(20, terran, "TERRAN") X(21, strmch, "STRMCH") X(22, chip, "CHIP") X(23, wavtbl, "WAVTBL") \
    X(24, speech, "SPEECH")
#define PL_N 25u
#define PL_VA 0u
#define PL_6OPA 17u
#define PL_TERRAN 20u
#define PL_WAVTBL 23u
#define PL_ITEM_WAV0 25u                              /* the pick items: the wave banks (the manifest's rules) */
#define PL_MODEL_PTR(id, key, name) pl_mdl_##key##_ptr,
#define PL_MODEL_NAME(id, key, name) name,

#if PL_UNITS_OLD                                      /* the units before this release (eng_macro.c MC_UNITS of 0.14.1 for 0..14, macro2.md's proposals for the rest): a build
 * for the proof that the restructured glue renders bit for bit as 0.16.0's (the budget's share of a chord changes with the units) */
#define PL_UNITS_VA 4
#define PL_UNITS_WAVSHP 5
#define PL_UNITS_FM 8
#define PL_UNITS_GRAIN 4
#define PL_UNITS_ADDTV 9
#define PL_UNITS_CHORD 3
#define PL_UNITS_SWARM 5
#define PL_UNITS_NOISE 2
#define PL_UNITS_PARTCL 4
#define PL_UNITS_STRING 4
#define PL_UNITS_MODAL 9
#define PL_UNITS_KICK 3
#define PL_UNITS_SNARE 4
#define PL_UNITS_HIHAT 3
#define PL_UNITS_HARP 4
#define PL_UNITS_VAVCF 5
#define PL_UNITS_PD 5
#define PL_UNITS_6OP 6
#define PL_UNITS_TERRAN 7
#define PL_UNITS_STRMCH 5
#define PL_UNITS_CHIP 2
#define PL_UNITS_WAVTBL 8
#define PL_UNITS_SPEECH 5
#else
#define PL_UNITS_VA 3
#define PL_UNITS_WAVSHP 4
#define PL_UNITS_FM 4
#define PL_UNITS_GRAIN 3
#define PL_UNITS_ADDTV 7
#define PL_UNITS_CHORD 3
#define PL_UNITS_SWARM 4
#define PL_UNITS_NOISE 2
#define PL_UNITS_PARTCL 4
#define PL_UNITS_STRING 4
#define PL_UNITS_MODAL 10
#define PL_UNITS_KICK 3
#define PL_UNITS_SNARE 4
#define PL_UNITS_HIHAT 3
#define PL_UNITS_HARP 3
#define PL_UNITS_VAVCF 2
#define PL_UNITS_PD 4
#define PL_UNITS_6OP 7
#define PL_UNITS_TERRAN 2
#define PL_UNITS_STRMCH 3
#define PL_UNITS_CHIP 1
#define PL_UNITS_WAVTBL 4
#define PL_UNITS_SPEECH 3
#endif
#endif /* PL_MODELS_LIST_H */

/* the units: the model's header, then PL_MDL_UNIT(key, type, prefix, big, draws, enveloped, units, out, aux) (the levels: eng_macro.c MC_POST
 * of 0.5.0 for 0..14, macro2.md for the rest; draws: the model reads plaits' shared generator in its render; enveloped: it makes its own envelope) */
#if PL_WANT_va
#include "pl_va.h"
PL_MDL_UNIT(va, pl_va_t, pl_va, 0, 0, 0, PL_UNITS_VA, 0.8f, 0.8f)
#undef pl_mdl_va_ptr
#define pl_mdl_va_ptr (&pl_mdl_va)
#else
#undef pl_mdl_va_ptr
#define pl_mdl_va_ptr 0
#endif
#if PL_WANT_wavshp
#include "pl_waveshaping.h"
PL_MDL_UNIT(wavshp, pl_ws_t, pl_ws, 0, 0, 0, PL_UNITS_WAVSHP, 0.7f, 0.6f)
#undef pl_mdl_wavshp_ptr
#define pl_mdl_wavshp_ptr (&pl_mdl_wavshp)
#else
#undef pl_mdl_wavshp_ptr
#define pl_mdl_wavshp_ptr 0
#endif
#if PL_WANT_fm
#include "pl_fm.h"
PL_MDL_UNIT(fm, pl_fm_t, pl_fm, 0, 0, 0, PL_UNITS_FM, 0.6f, 0.6f)
#undef pl_mdl_fm_ptr
#define pl_mdl_fm_ptr (&pl_mdl_fm)
#else
#undef pl_mdl_fm_ptr
#define pl_mdl_fm_ptr 0
#endif
#if PL_WANT_grain
#include "pl_grain.h"
PL_MDL_UNIT(grain, pl_grain_t, pl_grain, 0, 0, 0, PL_UNITS_GRAIN, 0.7f, 0.6f)
#undef pl_mdl_grain_ptr
#define pl_mdl_grain_ptr (&pl_mdl_grain)
#else
#undef pl_mdl_grain_ptr
#define pl_mdl_grain_ptr 0
#endif
#if PL_WANT_addtv
#include "pl_additive.h"
PL_MDL_UNIT(addtv, pl_add_t, pl_add, 0, 0, 0, PL_UNITS_ADDTV, 0.8f, 0.8f)
#undef pl_mdl_addtv_ptr
#define pl_mdl_addtv_ptr (&pl_mdl_addtv)
#else
#undef pl_mdl_addtv_ptr
#define pl_mdl_addtv_ptr 0
#endif
#if PL_WANT_chord
#include "pl_chord.h"
PL_MDL_UNIT(chord, pl_chord_t, pl_chord, 0, 0, 0, PL_UNITS_CHORD, 0.8f, 0.8f)
#undef pl_mdl_chord_ptr
#define pl_mdl_chord_ptr (&pl_mdl_chord)
#else
#undef pl_mdl_chord_ptr
#define pl_mdl_chord_ptr 0
#endif
#if PL_WANT_swarm
#include "pl_swarm.h"
PL_MDL_UNIT(swarm, pl_swarm_t, pl_swarm, 0, 1, 0, PL_UNITS_SWARM, -3.0f, 1.0f)
#undef pl_mdl_swarm_ptr
#define pl_mdl_swarm_ptr (&pl_mdl_swarm)
#else
#undef pl_mdl_swarm_ptr
#define pl_mdl_swarm_ptr 0
#endif
#if PL_WANT_noise
#include "pl_noise.h"
PL_MDL_UNIT(noise, pl_noise_t, pl_noise, 0, 1, 0, PL_UNITS_NOISE, -1.0f, -1.0f)
#undef pl_mdl_noise_ptr
#define pl_mdl_noise_ptr (&pl_mdl_noise)
#else
#undef pl_mdl_noise_ptr
#define pl_mdl_noise_ptr 0
#endif
#if PL_WANT_partcl
#include "pl_particle.h"
PL_MDL_UNIT(partcl, pl_particle_t, pl_particle, 1, 1, 0, PL_UNITS_PARTCL, -2.0f, 1.0f)
#undef pl_mdl_partcl_ptr
#define pl_mdl_partcl_ptr (&pl_mdl_partcl)
#else
#undef pl_mdl_partcl_ptr
#define pl_mdl_partcl_ptr 0
#endif
#if PL_WANT_string || PL_WANT_harp
#include "pl_string.h"
#endif
#if PL_WANT_string
PL_MDL_UNIT(string, pl_string_t, pl_string, 1, 1, 1, PL_UNITS_STRING, -1.0f, 0.8f)
#undef pl_mdl_string_ptr
#define pl_mdl_string_ptr (&pl_mdl_string)
#else
#undef pl_mdl_string_ptr
#define pl_mdl_string_ptr 0
#endif
#if PL_WANT_modal
#include "pl_modal.h"
PL_MDL_UNIT(modal, pl_modal_t, pl_modal, 0, 0, 1, PL_UNITS_MODAL, -1.0f, 0.8f)
#undef pl_mdl_modal_ptr
#define pl_mdl_modal_ptr (&pl_mdl_modal)
#else
#undef pl_mdl_modal_ptr
#define pl_mdl_modal_ptr 0
#endif
#if PL_WANT_kick
#include "pl_bassdrum.h"
PL_MDL_UNIT(kick, pl_bd_t, pl_bd, 0, 1, 1, PL_UNITS_KICK, 0.8f, 0.8f)
#undef pl_mdl_kick_ptr
#define pl_mdl_kick_ptr (&pl_mdl_kick)
#else
#undef pl_mdl_kick_ptr
#define pl_mdl_kick_ptr 0
#endif
#if PL_WANT_snare
#include "pl_snare.h"
PL_MDL_UNIT(snare, pl_sd_t, pl_sd, 0, 1, 1, PL_UNITS_SNARE, 0.8f, 0.8f)
#undef pl_mdl_snare_ptr
#define pl_mdl_snare_ptr (&pl_mdl_snare)
#else
#undef pl_mdl_snare_ptr
#define pl_mdl_snare_ptr 0
#endif
#if PL_WANT_hihat
#include "pl_hihat.h"
PL_MDL_UNIT(hihat, pl_hh_t, pl_hh, 0, 1, 1, PL_UNITS_HIHAT, 0.8f, 0.8f)
#undef pl_mdl_hihat_ptr
#define pl_mdl_hihat_ptr (&pl_mdl_hihat)
#else
#undef pl_mdl_hihat_ptr
#define pl_mdl_hihat_ptr 0
#endif
#if PL_WANT_harp
PL_MDL_UNIT(harp, pl_harp_t, pl_harp, 0, 1, 1, PL_UNITS_HARP, -1.0f, 0.8f)   /* (0.5.2, Orange Cat's own: STRING's string, one a voice) */
#undef pl_mdl_harp_ptr
#define pl_mdl_harp_ptr (&pl_mdl_harp)
#else
#undef pl_mdl_harp_ptr
#define pl_mdl_harp_ptr 0
#endif
#if PL_WANT_vavcf
#include "pl_vavcf.h"
PL_MDL_UNIT(vavcf, pl_vavcf_t, pl_vavcf, 0, 0, 0, PL_UNITS_VAVCF, 1.0f, 1.0f)
#undef pl_mdl_vavcf_ptr
#define pl_mdl_vavcf_ptr (&pl_mdl_vavcf)
#else
#undef pl_mdl_vavcf_ptr
#define pl_mdl_vavcf_ptr 0
#endif
#if PL_WANT_pd
#include "pl_pd.h"
PL_MDL_UNIT(pd, pl_pd_t, pl_pd, 0, 0, 0, PL_UNITS_PD, 0.7f, 0.7f)
#undef pl_mdl_pd_ptr
#define pl_mdl_pd_ptr (&pl_mdl_pd)
#else
#undef pl_mdl_pd_ptr
#define pl_mdl_pd_ptr 0
#endif
/* the six-op FM: one code unit (sixop) shared by the three models, whose descriptors sit with their DX7 banks (syx0, syx1, syx2: a model is
 * installed when its bank is). In the module the bank units name the code unit's functions as weak externs (a region reaches another through
 * absolute pointers only); in the firmware they are the same unit's statics */
#if PL_WANT_sixop || PL_WANT_syx0 || PL_WANT_syx1 || PL_WANT_syx2
#include "pl_sixop.h"
#endif
#if PL_WANT_sixop
PL_MDL_LINKAGE PL_MDL_ATTR void pl_mdl_6op_start0(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 0); pl_sixop_reset((pl_sixop_t *)m); }
PL_MDL_LINKAGE PL_MDL_ATTR void pl_mdl_6op_start1(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 1); pl_sixop_reset((pl_sixop_t *)m); }
PL_MDL_LINKAGE PL_MDL_ATTR void pl_mdl_6op_start2(void *m) { pl_sixop_init((pl_sixop_t *)m); pl_sixop_load_bank((pl_sixop_t *)m, 2); pl_sixop_reset((pl_sixop_t *)m); }
PL_MDL_LINKAGE PL_MDL_ATTR int pl_mdl_6op_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n)
{
    int env = 0;
    pl_sixop_render((pl_sixop_t *)m, q, o, a, n, &env);
    pl_sixop_flush((pl_sixop_t *)m);
    return 1;                                         /* (the voice's own envelopes) */
}
PL_KEEP(pl_mdl_6op_start0, pl_mdl_6op_start1, pl_mdl_6op_start2, pl_mdl_6op_render);
#elif PL_WANT_syx0 || PL_WANT_syx1 || PL_WANT_syx2
extern void pl_mdl_6op_start0(void *m) __attribute__((weak));
extern void pl_mdl_6op_start1(void *m) __attribute__((weak));
extern void pl_mdl_6op_start2(void *m) __attribute__((weak));
extern int pl_mdl_6op_render(void *m, const pl_params_t *q, float *o, float *a, uint32_t n) __attribute__((weak));
#endif
#define PL_MDL_6OP(key, start) PL_MDL_LINKAGE const pl_mdl_t pl_mdl_##key = { sizeof(pl_sixop_t), 1, 1, 1, PL_UNITS_6OP, 1.0f, 1.0f, start, pl_mdl_6op_render };
#if PL_WANT_syx0
PL_MDL_6OP(6opa, pl_mdl_6op_start0)
PL_KEEP(&pl_mdl_6opa);
#undef pl_mdl_6opa_ptr
#define pl_mdl_6opa_ptr (&pl_mdl_6opa)
#else
#undef pl_mdl_6opa_ptr
#define pl_mdl_6opa_ptr 0
#endif
#if PL_WANT_syx1
PL_MDL_6OP(6opb, pl_mdl_6op_start1)
PL_KEEP(&pl_mdl_6opb);
#undef pl_mdl_6opb_ptr
#define pl_mdl_6opb_ptr (&pl_mdl_6opb)
#else
#undef pl_mdl_6opb_ptr
#define pl_mdl_6opb_ptr 0
#endif
#if PL_WANT_syx2
PL_MDL_6OP(6opc, pl_mdl_6op_start2)
PL_KEEP(&pl_mdl_6opc);
#undef pl_mdl_6opc_ptr
#define pl_mdl_6opc_ptr (&pl_mdl_6opc)
#else
#undef pl_mdl_6opc_ptr
#define pl_mdl_6opc_ptr 0
#endif
#if PL_WANT_terran
#include "pl_terrain.h"
PL_MDL_UNIT(terran, pl_terrain_t, pl_terrain, 0, 0, 0, PL_UNITS_TERRAN, 0.7f, 0.7f)
#undef pl_mdl_terran_ptr
#define pl_mdl_terran_ptr (&pl_mdl_terran)
#else
#undef pl_mdl_terran_ptr
#define pl_mdl_terran_ptr 0
#endif
#if PL_WANT_strmch
#include "pl_strmach.h"
PL_MDL_UNIT(strmch, pl_strmach_t, pl_strmach, 1, 0, 0, PL_UNITS_STRMCH, 0.8f, 0.8f)
#undef pl_mdl_strmch_ptr
#define pl_mdl_strmch_ptr (&pl_mdl_strmch)
#else
#undef pl_mdl_strmch_ptr
#define pl_mdl_strmch_ptr 0
#endif
#if PL_WANT_chip
#include "pl_chiptune.h"
PL_MDL_UNIT(chip, pl_chiptune_t, pl_chiptune, 0, 1, 0, PL_UNITS_CHIP, 0.5f, 0.5f)
#undef pl_mdl_chip_ptr
#define pl_mdl_chip_ptr (&pl_mdl_chip)
#else
#undef pl_mdl_chip_ptr
#define pl_mdl_chip_ptr 0
#endif
#if PL_WANT_wavtbl
#include "pl_wavetable.h"
PL_MDL_UNIT(wavtbl, pl_wt_t, pl_wt, 0, 0, 0, PL_UNITS_WAVTBL, 0.6f, 0.6f)
#undef pl_mdl_wavtbl_ptr
#define pl_mdl_wavtbl_ptr (&pl_mdl_wavtbl)
#else
#undef pl_mdl_wavtbl_ptr
#define pl_mdl_wavtbl_ptr 0
#endif
#if PL_WANT_speech
#include "pl_speech.h"
PL_MDL_UNIT(speech, pl_speech_t, pl_speech, 1, 1, 0, PL_UNITS_SPEECH, -0.7f, 0.8f)   /* (enveloped as each block says: a replayed word is, a held vowel is not) */
#undef pl_mdl_speech_ptr
#define pl_mdl_speech_ptr (&pl_mdl_speech)
#else
#undef pl_mdl_speech_ptr
#define pl_mdl_speech_ptr 0
#endif
/* the wants consumed: the next inclusion (the host's one unit) names its own */
#undef PL_WANT_va
#undef PL_WANT_wavshp
#undef PL_WANT_fm
#undef PL_WANT_grain
#undef PL_WANT_addtv
#undef PL_WANT_chord
#undef PL_WANT_swarm
#undef PL_WANT_noise
#undef PL_WANT_partcl
#undef PL_WANT_string
#undef PL_WANT_modal
#undef PL_WANT_kick
#undef PL_WANT_snare
#undef PL_WANT_hihat
#undef PL_WANT_harp
#undef PL_WANT_vavcf
#undef PL_WANT_pd
#undef PL_WANT_sixop
#undef PL_WANT_syx0
#undef PL_WANT_syx1
#undef PL_WANT_syx2
#undef PL_WANT_terran
#undef PL_WANT_strmch
#undef PL_WANT_chip
#undef PL_WANT_wavtbl
#undef PL_WANT_speech
