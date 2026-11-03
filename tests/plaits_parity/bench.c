/* SPDX-License-Identifier: MIT
 * MACRO2 candidate: the host render cost of every Plaits model port (the 14 of MACRO and the 10 of MACRO2), model against model, so a new
 * model's cost can be placed beside one the unit has measured (ENGINE_LOADING_PLAN.md, "MACRO's own cost"). Each model renders one voice in
 * 16-frame blocks (the engine's MC_SUB) at 44.1 kHz (PL_PARITY off) for one second, the least CPU time of REPEATS runs (CLOCK_PROCESS_CPUTIME_ID:
 * other processes' load does not count, only this one's).
 *   tests/plaits_parity/bench.sh         (builds port.c without PL_PARITY and this file; prints the table)
 */
#include "models.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#define BLOCK 16
#define REPEATS 5
static double cpu_ns(void) { struct timespec ts; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts); return ts.tv_sec * 1e9 + ts.tv_nsec; }
typedef struct { int model; const char *label; float harm, timbre, morph; int unpatched; } case_t;
static const case_t CASES[] = {
    {M_VA, "VA (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_WAVESHAPING, "WAVSHP (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_FM, "FM (MACRO)", 0.5f, 0.5f, 0.5f, 0},
    {M_GRAIN, "GRAIN (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_ADDITIVE, "ADDTV (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_CHORD, "CHORD (MACRO)", 0.5f, 0.5f, 0.5f, 0},
    {M_SWARM, "SWARM (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_NOISE, "NOISE (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_PARTICLE, "PARTCL (MACRO)", 0.5f, 0.5f, 0.5f, 0},
    {M_STRING, "STRING (MACRO)", 0.25f, 0.5f, 0.5f, 0}, {M_MODAL, "MODAL (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_BASS_DRUM, "KICK (MACRO)", 0.5f, 0.5f, 0.5f, 0},
    {M_SNARE, "SNARE (MACRO)", 0.5f, 0.5f, 0.5f, 0}, {M_HI_HAT, "HI-HAT (MACRO)", 0.5f, 0.5f, 0.5f, 0},
    {M_VA_VCF, "VA-VCF", 0.5f, 0.5f, 0.5f, 0}, {M_PHASE_DISTORTION, "PD", 0.5f, 0.5f, 0.5f, 0},
    {M_SIX_OP_A, "6OP A (patch 0, SOLID BASS)", 0.0f, 0.5f, 0.5f, 0}, {M_SIX_OP_A, "6OP A (patch 16, CS 80)", 0.5f, 0.5f, 0.5f, 0},
    {M_SIX_OP_B, "6OP B (patch 0, E.PIANO 1)", 0.0f, 0.5f, 0.5f, 0}, {M_SIX_OP_C, "6OP C (patch 16, STRINGS 3)", 0.5f, 0.5f, 0.5f, 0},
    {M_WAVE_TERRAIN, "TERRAN (terrain 3)", 0.5f, 0.5f, 0.5f, 0}, {M_WAVE_TERRAIN, "TERRAN (terrain 6: a wave bank)", 0.9f, 0.5f, 0.5f, 0},
    {M_STRING_MACHINE, "STRMCH", 0.5f, 0.5f, 0.5f, 0}, {M_CHIPTUNE, "CHIP", 0.5f, 0.5f, 0.5f, 0},
    {M_WAVETABLE, "WAVTBL (interpolated)", 0.3f, 0.5f, 0.5f, 0}, {M_WAVETABLE, "WAVTBL (quantized, z > 3)", 0.6f, 0.5f, 0.5f, 0},
    {M_SPEECH, "SPEECH (formant, naive)", 0.1f, 0.5f, 0.5f, 0}, {M_SPEECH, "SPEECH (SAM)", 0.3f, 0.5f, 0.5f, 0},
    {M_SPEECH, "SPEECH (LPC phonemes)", 0.4f, 0.5f, 0.5f, 0}, {M_SPEECH, "SPEECH (LPC words, bank 4)", 0.95f, 0.5f, 0.5f, 0},
};
int main(void)
{
    double va = 0;
    printf("plaits bench: one voice, 16-frame blocks, 44.1 kHz, one second, the least CPU time of %d runs\n", REPEATS);
    printf("%-34s %9s %8s\n", "model", "ns/sample", "x VA");
    for (size_t c = 0; c < sizeof CASES / sizeof CASES[0]; c++) {
        const case_t *k = &CASES[c];
        double best = 1e30;
        for (int r = 0; r < REPEATS; r++) {
            void *e = port_new(k->model);
            float out[BLOCK], aux[BLOCK];
            parity_params_t q = { 0, 48.0f, k->timbre, k->morph, k->harm, 0.8f };
            int env = 0;
            double t0;
            if (!e) { printf("%-34s not ported\n", k->label); best = 0; break; }
            port_seed(0x21u);
            q.trigger = 1 | 4;
            port_render(e, &q, out, aux, BLOCK, &env);
            q.trigger = k->unpatched ? 2 : 4;
            for (int b = 0; b < 20; b++) port_render(e, &q, out, aux, BLOCK, &env);
            t0 = cpu_ns();
            for (int b = 0; b < 44100 / BLOCK; b++) port_render(e, &q, out, aux, BLOCK, &env);
            double ns = (cpu_ns() - t0) / (44100.0 / BLOCK * BLOCK);
            if (ns < best) best = ns;
        }
        if (c == 0) va = best;
        printf("%-34s %9.0f %8.2f\n", k->label, best, best / va);
    }
    return 0;
}
