/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat, the engine candidates gate: tests/caravel_parity (run.sh): SEM and HADES as Caravel builds them against the modules' units. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define NVOICE 8
#define NPART 4
#include "fe_tables.h"
#include "felucca_engine.h"
enum { P_ATK, P_REL, P_FTYPE, P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_COUNT };
typedef struct track { int16_t p[P_COUNT]; voice_t v[NVOICE]; } track_t;
static track_t trk[NPART];
#include "dsp.c"
#include "va_blocks.c"
#include "eng_sem.c"
#include "eng_hades.c"
#define SET_TYPE(t) ((t)->p[P_FTYPE] = 0)
#define PITCH_INC(p) pitch_inc((uint32_t)(p))
#include "drive.h"
int main(int argc, char **argv)
{
    const engine_t *e = argv[1][0] == 's' ? &ENG_SEM : &ENG_HADES;
    FILE *f = fopen(argv[2], "wb");
    for (uint32_t k = 0; k < e->npresets; k++) drive(e, &trk[0], k, f);
    fclose(f);
    printf("%s: %u presets\n", e->name, e->npresets);
    return 0;
}
