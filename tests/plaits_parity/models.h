/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.0: the Plaits models the parity test knows (the numbers are Plaits' own engine order, 0..15 of
 * its first bank). Shared by ref.cc (the original C++) and port.c (the C port). */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { int trigger; float note, timbre, morph, harmonics, accent; } parity_params_t;   /* = pl_params_t */
enum {
    M_VA = 0, M_WAVESHAPING = 1, M_FM = 2, M_GRAIN = 3, M_ADDITIVE = 4, M_WAVETABLE = 5, M_CHORD = 6, M_SPEECH = 7,
    M_SWARM = 8, M_NOISE = 9, M_PARTICLE = 10, M_STRING = 11, M_MODAL = 12, M_BASS_DRUM = 13, M_SNARE = 14, M_HI_HAT = 15,
    M_COUNT = 16
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
