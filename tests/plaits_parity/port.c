/* SPDX-License-Identifier: MIT
 * Orange Cat 0.5.0: the C port of the Plaits models (firmware/src/plaits/), for tests/plaits_parity. Built with
 * PL_PARITY (48 kHz, the reference's buffer sizes). A model whose header is not there yet reports "not ported".
 * PARITY_HEADERS="pl_fm pl_grain" (run.sh) compiles only those headers, so one model's work in progress does not
 * stop another's test. PARITY_FLUSH=1: each model's pl_X_flush after each render, as the firmware does. */
#include "models.h"
#include <stdlib.h>
#include <string.h>
#include "../../firmware/src/plaits/pl_dsp.h"
_Static_assert(sizeof(parity_params_t) == sizeof(pl_params_t), "parameters");

#define PORT(M, name, prefix)                                                       \
    case M: {                                                                       \
        prefix##_t *e = calloc(1, sizeof *e);                                       \
        prefix##_init(e);                                                           \
        prefix##_reset(e);                                                          \
        b->e = e;                                                                   \
        b->render = (render_fn)prefix##_render;                                     \
        b->flush = (flush_fn)prefix##_flush;                                        \
        return b;                                                                   \
    }
typedef void (*render_fn)(void *, const pl_params_t *, float *, float *, size_t, int *);
typedef void (*flush_fn)(void *);
typedef struct { void *e; render_fn render; flush_fn flush; } box_t;

#if __has_include("../../firmware/src/plaits/pl_va.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_va))
#include "../../firmware/src/plaits/pl_va.h"
#define HAVE_VA 1
#endif
#if __has_include("../../firmware/src/plaits/pl_waveshaping.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_waveshaping))
#include "../../firmware/src/plaits/pl_waveshaping.h"
#define HAVE_WS 1
#endif
#if __has_include("../../firmware/src/plaits/pl_fm.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_fm))
#include "../../firmware/src/plaits/pl_fm.h"
#define HAVE_FM 1
#endif
#if __has_include("../../firmware/src/plaits/pl_grain.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_grain))
#include "../../firmware/src/plaits/pl_grain.h"
#define HAVE_GRAIN 1
#endif
#if __has_include("../../firmware/src/plaits/pl_additive.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_additive))
#include "../../firmware/src/plaits/pl_additive.h"
#define HAVE_ADD 1
#endif
#if __has_include("../../firmware/src/plaits/pl_chord.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_chord))
#include "../../firmware/src/plaits/pl_chord.h"
#define HAVE_CHORD 1
#endif
#if __has_include("../../firmware/src/plaits/pl_swarm.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_swarm))
#include "../../firmware/src/plaits/pl_swarm.h"
#define HAVE_SWARM 1
#endif
#if __has_include("../../firmware/src/plaits/pl_noise.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_noise))
#include "../../firmware/src/plaits/pl_noise.h"
#define HAVE_NOISE 1
#endif
#if __has_include("../../firmware/src/plaits/pl_particle.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_particle))
#include "../../firmware/src/plaits/pl_particle.h"
#define HAVE_PARTICLE 1
#endif
#if __has_include("../../firmware/src/plaits/pl_string.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_string))
#include "../../firmware/src/plaits/pl_string.h"
#define HAVE_STRING 1
#endif
#if __has_include("../../firmware/src/plaits/pl_modal.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_modal))
#include "../../firmware/src/plaits/pl_modal.h"
#define HAVE_MODAL 1
#endif
#if __has_include("../../firmware/src/plaits/pl_bassdrum.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_bassdrum))
#include "../../firmware/src/plaits/pl_bassdrum.h"
#define HAVE_BD 1
#endif
#if __has_include("../../firmware/src/plaits/pl_snare.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_snare))
#include "../../firmware/src/plaits/pl_snare.h"
#define HAVE_SD 1
#endif
#if __has_include("../../firmware/src/plaits/pl_hihat.h") && (!defined(PARITY_SELECT) || defined(PARITY_WANT_pl_hihat))
#include "../../firmware/src/plaits/pl_hihat.h"
#define HAVE_HH 1
#endif

void *port_new(int model)
{
    box_t *b = calloc(1, sizeof *b);
    switch (model) {
#ifdef HAVE_VA
    PORT(M_VA, va, pl_va)
#endif
#ifdef HAVE_WS
    PORT(M_WAVESHAPING, waveshaping, pl_ws)
#endif
#ifdef HAVE_FM
    PORT(M_FM, fm, pl_fm)
#endif
#ifdef HAVE_GRAIN
    PORT(M_GRAIN, grain, pl_grain)
#endif
#ifdef HAVE_ADD
    PORT(M_ADDITIVE, additive, pl_add)
#endif
#ifdef HAVE_CHORD
    PORT(M_CHORD, chord, pl_chord)
#endif
#ifdef HAVE_SWARM
    PORT(M_SWARM, swarm, pl_swarm)
#endif
#ifdef HAVE_NOISE
    PORT(M_NOISE, noise, pl_noise)
#endif
#ifdef HAVE_PARTICLE
    PORT(M_PARTICLE, particle, pl_particle)
#endif
#ifdef HAVE_STRING
    PORT(M_STRING, string, pl_string)
#endif
#ifdef HAVE_MODAL
    PORT(M_MODAL, modal, pl_modal)
#endif
#ifdef HAVE_BD
    PORT(M_BASS_DRUM, bassdrum, pl_bd)
#endif
#ifdef HAVE_SD
    PORT(M_SNARE, snare, pl_sd)
#endif
#ifdef HAVE_HH
    PORT(M_HI_HAT, hihat, pl_hh)
#endif
    default: free(b); return 0;
    }
}
void port_render(void *box, const parity_params_t *p, float *out, float *aux, size_t size, int *enveloped)
{
    box_t *b = box;
    b->render(b->e, (const pl_params_t *)p, out, aux, size, enveloped);
#ifdef PARITY_FLUSH
    b->flush(b->e);                             /* (Orange Cat's tiny-state flush, as eng_macro.c calls it) */
#endif
}
void port_seed(uint32_t s) { pl_rng_state = s; }
