/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Orange Cat, the engine candidates gate: tests/op1r_parity (run.sh): an op1r plugin through its module unit (tools/module/<name>_mod.c and
 * tools/module/op1r/op1_glue.h, OC_MODULE_HOST), driven as op1r's synth_sdk/harness/audio_harness.c drives the plugin: voice 0, six notes of 138
 * blocks of 128 frames (0.4 s), velocity 127 and 64 alternating, the key up at block 110, the harness's stand-in envelope (a 10-block attack,
 * 24575 held, a fall to 0 from the key up) as OP1_VOICE_ENV, op1r's phase increment (lrint(2^32 f / 44100)), the four knobs on EDIT 1 (each a
 * multiple of 258: the glue's scale) and the second page's words at 0, as the harness has them. What it writes is what the plugin rendered through
 * the glue: voice 0's 128-sample block (op1g_part_t buf[0], the first bytes of the part's state) after the four 32-frame renders of each block.
 * The ADSR is not applied there (op1r's harness writes the plugin's raw output too). Built twice: with the glue's sine (OP1_SINE over SINE[1025]),
 * and with -DOP1G_PARITY_SINE, op1r's own 8192-point table, which must give op1r's render in every sample (the rest of the glue, proven); that build
 * with OP1G_TRUNC=1 in the environment fills the table truncated instead of rounded: op1r's own render with a sine at most 1 LSB off, as ours is
 * (compare.py: how far op1r moves by itself).
 *   ours_<name> n0,n1,n2,n3,n4,n5 k0 k1 k2 k3 > raw int16
 */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
static track_t trk_h[NPART];
static const void *base[2];
static uint32_t arena[16384 / 4];
static void *mcp(void *d, const void *s, uint32_t n) { return memcpy(d, s, n); }
static void *mst(void *d, int c, uint32_t n) { return memset(d, c, n); }
static void *rget(uint32_t id, uint32_t bytes) { return id == 0u && bytes <= sizeof arena ? (void *)arena : 0; }   /* track 0's block only */
static fw_api_t api;
#ifdef OP1G_PARITY_SINE
int16_t op1g_parity_sine[8192];             /* (the build with op1r's sine: op1_glue.h) */
#endif
#define OP1_BLOCK_FRAMES 128                       /* op1r's OP1_BLOCK */
static int32_t inc_of(int midi) { return (int32_t)lrint(4294967296.0 * 440.0 * pow(2, (midi - 69) / 12.0) / 44100); }

int main(int argc, char **argv)
{
    int notes[6], k[4];
    const engine_t *e;
    track_t *t = &trk_h[0];
    voice_t *v = &t->v[0];
    if (argc < 6 || sscanf(argv[1], "%d,%d,%d,%d,%d,%d", &notes[0], &notes[1], &notes[2], &notes[3], &notes[4], &notes[5]) != 6) {
        fprintf(stderr, "usage: ours n0,..,n5 k0 k1 k2 k3\n");
        return 2;
    }
    for (int i = 0; i < 4; i++) {
        k[i] = atoi(argv[2 + i]);
        if (k[i] % 258 || k[i] < 0 || k[i] > 127 * 258) { fprintf(stderr, "knob %d: %d is not a multiple of 258 in 0..32766\n", i, k[i]); return 2; }
    }
#ifdef OP1G_PARITY_SINE
    for (int i = 0; i < 8192; i++)            /* as op1r's audio_harness.c; OP1G_TRUNC=1: truncated, not rounded (at most 1 LSB off: op1r's sensitivity) */
        op1g_parity_sine[i] = (int16_t)(getenv("OP1G_TRUNC") ? 32767.0 * sin(2 * M_PI * i / 8192.0) : lrint(32767.0 * sin(2 * M_PI * i / 8192.0)));
#endif
    base[0] = SINE; base[1] = TANH_Q15;
    api.trk = trk_h; api.base_hot = base; api.mem_copy = mcp; api.mem_set = mst; api.resource_get = rget; api.res_engine0 = 0;
    oc_host_fw = &api;
    e = oc_host_module_engine();
    if (OC_MODULE_HDR.state_size > sizeof arena) { fprintf(stderr, "state %u B\n", (unsigned)OC_MODULE_HDR.state_size); return 2; }
    for (int i = 0; i < 4; i++) t->p[P_E0 + i] = (int16_t)(k[i] / 258);
    if (getenv("OP1G_SEED"))                  /* "w=value": op1r's voice word w (1..8: voice_t's s[w - 1]) before the first note-on: a plugin that seeds
                                               * its noise from the voice object's address (arc) gets the seed op1r's harness gave it (run.sh) */
        v->s[atoi(getenv("OP1G_SEED")) - 1] = (int32_t)strtoul(strchr(getenv("OP1G_SEED"), '=') + 1, 0, 0);
    for (int n = 0; n < 6; n++) {
        v->vel = (uint8_t)(n & 1 ? 64 : 127);
        v->note = (uint8_t)notes[n];
        v->gate = v->active = 1;
        e->note_on(t, v);
        for (int b = 0; b < 138; b++) {
            vmod_t m;
            int32_t out[CTL];
            if (b == 110) v->gate = 0;
            memset(&m, 0, sizeof m);
            m.inc = (uint32_t)inc_of(notes[n]);
            m.amp0 = m.amp1 = b < 10 ? 32767 * b / 10 : b < 110 ? 24575 : 24575 - 24575 * (b - 110) / 28;
            for (int q = 0; q < OP1_BLOCK_FRAMES / CTL; q++) {
                memset(out, 0, sizeof out);
                e->render(t, v, out, CTL, &m);
            }
            fwrite(arena, 2, OP1_BLOCK_FRAMES, stdout);   /* op1g_part_t buf[0]: the part's first bytes */
        }
    }
    return 0;
}
