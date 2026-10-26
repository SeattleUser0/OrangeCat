/* SPDX-License-Identifier: GPL-3.0-only */
/* (Orange Cat, this file) An extract of Ravel (https://github.com/De-Pattern/Ravel_FM1, commit e9b6398, GPL-3.0-only): its MONO engine's voice
 * state, mono_t (firmware/app/rv_int.h), the text unchanged. rv_mono.c holds the MONO section of rv_synth.c that reads it. */
/* MONO: one oscillator (saw, pulse, sub, noise), the scoop, delayed vibrato and drift */
typedef struct { float ph, sub, scoop, vib_t, vib_ph, drift, drift_v; uint32_t ns; } mono_t;
