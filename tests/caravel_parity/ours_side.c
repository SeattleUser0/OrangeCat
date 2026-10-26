/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat, the engine candidates gate: tests/caravel_parity (run.sh): SEM and HADES as Caravel builds them against the modules' units. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
extern const fw_api_t *oc_host_fw;
extern const engine_t *oc_host_module_engine(void);
static track_t trk_h[NPART];
static const void *base[2];
static void *mcp(void *d, const void *s, uint32_t n) { return memcpy(d, s, n); }
static void *mst(void *d, int c, uint32_t n) { return memset(d, c, n); }
static fw_api_t api;
#define SET_TYPE(t) ((void)0)
#define PITCH_INC(p) tuned_pitch_inc((uint32_t)(p))
#include "drive.h"
int main(int argc, char **argv)
{
    const engine_t *e;
    FILE *f = fopen(argv[2], "wb");
    base[0] = SINE; base[1] = TANH_Q15;
    api.trk = trk_h; api.base_hot = base; api.mem_copy = mcp; api.mem_set = mst;
    oc_host_fw = &api;
    e = oc_host_module_engine();
    for (uint32_t k = 0; k < e->npresets; k++) drive(e, &trk_h[0], k, f);
    fclose(f);
    printf("%s: %u presets\n", e->name, e->npresets);
    return 0;
}
