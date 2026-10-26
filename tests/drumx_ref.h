/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.9.0: the render that tests/drumx_ref.txt holds, one procedure for both sides.
 *
 * The same code is compiled twice. Once in Jiant FM's tree (juanjiant-bit/jiant, commit dc8d025) after its hostsim.c, with
 * -DDXREF_JIANT: dx_run takes a pointer into the global kit, which dx_kit_load fills. That run wrote tests/drumx_ref.txt
 * (tools/drumx_ref_gen.sh says how). Once in Orange Cat's tests/drumx_test.c: dx_run takes the kit row and the lane. The
 * hashes must be equal: the voice of this tree is Jiant's, sample for sample.
 *
 * Each line of the file: kit, lane, scenario, the 64-bit FNV-1a of every output sample of a hit rendered for 48 blocks.
 * The scenarios move every argument of dx_run (MORPH, tune, decay, colour, noise offsets, the fold, the FM strike) and
 * move them while the hit rings, strike the hit again in mid-render, and choke the open hat.
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define DXREF_NKIT 9u
#define DXREF_NSCEN 4u
#define DXREF_BLOCKS 48u

#ifdef DXREF_JIANT
#define DXREF_RUN(K, L, V, MORPH, TUNE, DOFS, COFS, NOFS, WARP, FM, Y) \
    dx_run(&dx_kit[L], V, MORPH, TUNE, DOFS, COFS, NOFS, WARP, FM, Y, CTL)
#define DXREF_LOAD(K) dx_kit_load(K)
#else
#define DXREF_RUN(K, L, V, MORPH, TUNE, DOFS, COFS, NOFS, WARP, FM, Y) \
    ((V)->layer = DX_KITS[K].lay[L], (V)->tone = (V)->hands = 0,   /* ((0.14.2) the hit's LAYER as the kit has it, TONE and HANDS 0:   \
                                                                   * Jiant's kit) */                                                 \
     dx_run(&DX_KITS[K], L, V, MORPH, TUNE, DOFS, COFS, NOFS, WARP, FM, Y, CTL))
#define DXREF_LOAD(K) ((void)(K))
#endif

static uint64_t dxref_hit(uint32_t kit, uint32_t lane, uint32_t scen)
{
    dx_voice_t v;
    int32_t y[CTL];
    uint64_t h = 1469598103934665603ull;
    uint32_t b, i;
    DXREF_LOAD(kit);
    memset(&v, 0, sizeof v);
    dx_trigger(&v);
    for (b = 0; b < DXREF_BLOCKS; b++) {
        int32_t morph = 64, tune = 0, dofs = 0, cofs = 0, nofs = 0, warp = 0, fm = 0;
        if (scen == 1u) {                                /* every offset set */
            morph = 0; tune = -96; dofs = 20; cofs = -30; nofs = 25; warp = 60; fm = 70;
        } else if (scen == 2u) {                         /* B, up, short, bright, noisy, folded */
            morph = 127; tune = 120; dofs = -25; cofs = 30; nofs = -20; warp = 127; fm = 127;
        } else if (scen == 3u) {                         /* everything moves while the hit rings; a second strike */
            morph = (int32_t)((b * 7u) & 127u); tune = (int32_t)b * 5 - 100; dofs = (int32_t)b - 24; cofs = 24 - (int32_t)b;
            nofs = (int32_t)(b & 15u); warp = (int32_t)(b * 2u); fm = (int32_t)((b * 3u) & 127u);
            if (b == 20u) dx_trigger(&v);
            if (lane == DV_HATO && b == 12u) dx_choke(&v);
        }
        DXREF_RUN(kit, lane, &v, morph, tune, dofs, cofs, nofs, warp, fm, y);
        for (i = 0; i < CTL; i++) {
            uint32_t s = (uint32_t)y[i];
            int k;
            for (k = 0; k < 4; k++) {
                h ^= (s >> (8 * k)) & 255u;
                h *= 1099511628211ull;
            }
        }
    }
    return h;
}

/* the lines the file holds */
static void dxref_print(FILE *f)
{
    uint32_t k, l, s;
    fprintf(f, "# DRUM-X reference renders: kit lane scenario FNV-1a64 (tests/drumx_ref.h). Written by Jiant FM's drumx_voice.c\n"
               "# (juanjiant-bit/jiant dc8d025, GPL-3.0-only) through tools/drumx_ref_gen.sh; Orange Cat's must equal it.\n");
    for (k = 0; k < DXREF_NKIT; k++)
        for (l = 0; l < 8u; l++)
            for (s = 0; s < DXREF_NSCEN; s++)
                fprintf(f, "%u %u %u %016llx\n", k, l, s, (unsigned long long)dxref_hit(k, l, s));
}
