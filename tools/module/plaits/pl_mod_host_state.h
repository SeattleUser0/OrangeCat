/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * PLAITS (the model picker's module): the host unit's state_size, after the glue (its part layout) and every model unit are in (plaits_mod.c). */
#pragma once
/* the largest state a part needs with every unit in (the host header's state_size): a chain of constants, one a model (a nested max
 * macro doubles its text at each level: 23 levels broke the compiler) */
#define PL_HOST_NEED(T, big) (PL_PART_HEAD + ((big) ? 1u : PL_POLY) * ((sizeof(T) + 7u) & ~7u))
#define PL_HOST_MAX2(a, b) ((a) > (b) ? (a) : (b))
enum {
    PL_HOST_S_0 = 0u,
#ifndef PL_HOST_NO_va
    PL_HOST_S0 = PL_HOST_MAX2(PL_HOST_S_0, PL_HOST_NEED(pl_va_t, 0)),
#else
    PL_HOST_S0 = PL_HOST_S_0,
#endif
#ifndef PL_HOST_NO_wavshp
    PL_HOST_S1 = PL_HOST_MAX2(PL_HOST_S0, PL_HOST_NEED(pl_ws_t, 0)),
#else
    PL_HOST_S1 = PL_HOST_S0,
#endif
#ifndef PL_HOST_NO_fm
    PL_HOST_S2 = PL_HOST_MAX2(PL_HOST_S1, PL_HOST_NEED(pl_fm_t, 0)),
#else
    PL_HOST_S2 = PL_HOST_S1,
#endif
#ifndef PL_HOST_NO_grain
    PL_HOST_S3 = PL_HOST_MAX2(PL_HOST_S2, PL_HOST_NEED(pl_grain_t, 0)),
#else
    PL_HOST_S3 = PL_HOST_S2,
#endif
#ifndef PL_HOST_NO_addtv
    PL_HOST_S4 = PL_HOST_MAX2(PL_HOST_S3, PL_HOST_NEED(pl_add_t, 0)),
#else
    PL_HOST_S4 = PL_HOST_S3,
#endif
#ifndef PL_HOST_NO_chord
    PL_HOST_S5 = PL_HOST_MAX2(PL_HOST_S4, PL_HOST_NEED(pl_chord_t, 0)),
#else
    PL_HOST_S5 = PL_HOST_S4,
#endif
#ifndef PL_HOST_NO_swarm
    PL_HOST_S6 = PL_HOST_MAX2(PL_HOST_S5, PL_HOST_NEED(pl_swarm_t, 0)),
#else
    PL_HOST_S6 = PL_HOST_S5,
#endif
#ifndef PL_HOST_NO_noise
    PL_HOST_S7 = PL_HOST_MAX2(PL_HOST_S6, PL_HOST_NEED(pl_noise_t, 0)),
#else
    PL_HOST_S7 = PL_HOST_S6,
#endif
#ifndef PL_HOST_NO_partcl
    PL_HOST_S8 = PL_HOST_MAX2(PL_HOST_S7, PL_HOST_NEED(pl_particle_t, 1)),
#else
    PL_HOST_S8 = PL_HOST_S7,
#endif
#ifndef PL_HOST_NO_string
    PL_HOST_S9 = PL_HOST_MAX2(PL_HOST_S8, PL_HOST_NEED(pl_string_t, 1)),
#else
    PL_HOST_S9 = PL_HOST_S8,
#endif
#ifndef PL_HOST_NO_modal
    PL_HOST_S10 = PL_HOST_MAX2(PL_HOST_S9, PL_HOST_NEED(pl_modal_t, 0)),
#else
    PL_HOST_S10 = PL_HOST_S9,
#endif
#ifndef PL_HOST_NO_kick
    PL_HOST_S11 = PL_HOST_MAX2(PL_HOST_S10, PL_HOST_NEED(pl_bd_t, 0)),
#else
    PL_HOST_S11 = PL_HOST_S10,
#endif
#ifndef PL_HOST_NO_snare
    PL_HOST_S12 = PL_HOST_MAX2(PL_HOST_S11, PL_HOST_NEED(pl_sd_t, 0)),
#else
    PL_HOST_S12 = PL_HOST_S11,
#endif
#ifndef PL_HOST_NO_hihat
    PL_HOST_S13 = PL_HOST_MAX2(PL_HOST_S12, PL_HOST_NEED(pl_hh_t, 0)),
#else
    PL_HOST_S13 = PL_HOST_S12,
#endif
#ifndef PL_HOST_NO_harp
    PL_HOST_S14 = PL_HOST_MAX2(PL_HOST_S13, PL_HOST_NEED(pl_harp_t, 0)),
#else
    PL_HOST_S14 = PL_HOST_S13,
#endif
#ifndef PL_HOST_NO_vavcf
    PL_HOST_S15 = PL_HOST_MAX2(PL_HOST_S14, PL_HOST_NEED(pl_vavcf_t, 0)),
#else
    PL_HOST_S15 = PL_HOST_S14,
#endif
#ifndef PL_HOST_NO_pd
    PL_HOST_S16 = PL_HOST_MAX2(PL_HOST_S15, PL_HOST_NEED(pl_pd_t, 0)),
#else
    PL_HOST_S16 = PL_HOST_S15,
#endif
#ifndef PL_HOST_NO_sixop
    PL_HOST_S17 = PL_HOST_MAX2(PL_HOST_S16, PL_HOST_NEED(pl_sixop_t, 1)),
#else
    PL_HOST_S17 = PL_HOST_S16,
#endif
#ifndef PL_HOST_NO_terran
    PL_HOST_S18 = PL_HOST_MAX2(PL_HOST_S17, PL_HOST_NEED(pl_terrain_t, 0)),
#else
    PL_HOST_S18 = PL_HOST_S17,
#endif
#ifndef PL_HOST_NO_strmch
    PL_HOST_S19 = PL_HOST_MAX2(PL_HOST_S18, PL_HOST_NEED(pl_strmach_t, 1)),
#else
    PL_HOST_S19 = PL_HOST_S18,
#endif
#ifndef PL_HOST_NO_chip
    PL_HOST_S20 = PL_HOST_MAX2(PL_HOST_S19, PL_HOST_NEED(pl_chiptune_t, 0)),
#else
    PL_HOST_S20 = PL_HOST_S19,
#endif
#ifndef PL_HOST_NO_wavtbl
    PL_HOST_S21 = PL_HOST_MAX2(PL_HOST_S20, PL_HOST_NEED(pl_wt_t, 0)),
#else
    PL_HOST_S21 = PL_HOST_S20,
#endif
#ifndef PL_HOST_NO_speech
    PL_HOST_S22 = PL_HOST_MAX2(PL_HOST_S21, PL_HOST_NEED(pl_speech_t, 1)),
#else
    PL_HOST_S22 = PL_HOST_S21,
#endif
};
#define PL_STATE_HOST ((uint32_t)PL_HOST_S22)
