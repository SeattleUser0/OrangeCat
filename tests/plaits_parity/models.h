/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.0: the Plaits models the parity test knows (the numbers are Plaits' own engine order, 0..15 of
 * its first bank). Shared by ref.cc (the original C++) and port.c (the C port).
 * MACRO2 candidate: Plaits 1.2's second bank appended as 16..23 (Plaits itself lists that bank first, 0..7, and the
 * original sixteen after it: the numbers here keep the first bank's as they were). The six-op FM engine is one
 * engine in Plaits registered three times, once a DX7 bank (fm_patches_table[0..2]): three models here too. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { int trigger; float note, timbre, morph, harmonics, accent; } parity_params_t;   /* = pl_params_t */
enum {
    M_VA = 0, M_WAVESHAPING = 1, M_FM = 2, M_GRAIN = 3, M_ADDITIVE = 4, M_WAVETABLE = 5, M_CHORD = 6, M_SPEECH = 7,
    M_SWARM = 8, M_NOISE = 9, M_PARTICLE = 10, M_STRING = 11, M_MODAL = 12, M_BASS_DRUM = 13, M_SNARE = 14, M_HI_HAT = 15,
    M_VA_VCF = 16, M_PHASE_DISTORTION = 17, M_SIX_OP_A = 18, M_SIX_OP_B = 19, M_SIX_OP_C = 20, M_WAVE_TERRAIN = 21,
    M_STRING_MACHINE = 22, M_CHIPTUNE = 23,
    M_COUNT = 24
};
#ifdef __cplusplus
extern "C" {
#endif
void *ref_new(int model);
void ref_render(void *e, const parity_params_t *p, float *out, float *aux, size_t size, int *enveloped);
void ref_seed(uint32_t s);
void *port_new(int model);              /* 0: not ported (yet) */
void port_render(void *e, const parity_params_t *p, float *out, float *aux, size_t size, int *enveloped);
void port_seed(uint32_t s);
#ifdef __cplusplus
}
#endif
