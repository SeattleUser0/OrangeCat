/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat DRUM-X measurement (exp/sonnet-drumx, throwaway): Jiant FM's DRUM-X voice (drumx_voice.c, copied unchanged
 * from juanjiant-bit/jiant firmware/src) behind MELODEE_DRUMX, in the retired PHYS slot (ENGI_DRUMX 9). The glue is the least
 * that drives every part of the voice: eight lanes a part (dx_voice_t each), a hit's lane from its note, MORPH from E0, the
 * nine factory kits as presets (dx_kit_load). What is NOT here (Jiant's eng_drum.c, 615 lines): the lane mutes and groups,
 * IMAGE and SEND buses (drum_side_*), the kit bus (DRV, knee), PUNCH and ATK, FLAM and RATCH, BEND, the LFO on the kit, the
 * motion lanes, the GM map, the pad grid's names and the EDIT pages. They are inventoried in ENGINE_LOADING_PLAN.md.
 *
 * Revisions:
 *   exp: first version.
 */
#pragma once
#include "drumx_voice.c"

#define DXE_XIP_ATTR
#ifdef DXE_XIP
#define DXE_XIP_ATTR __attribute__((section(".text.xip_drumx"), noinline))
#endif

typedef struct { dx_voice_t v[DV_NLANE]; } dxe_part_t;
static dxe_part_t *dxe_part(uint32_t part);             /* engines.c */

static uint32_t dxe_lane(uint32_t note) { return (note % 12u) < 8u ? (note % 12u) : 7u - (note % 12u) % 8u; }

static DXE_XIP_ATTR void dxe_note_on(track_t *t, voice_t *v)
{
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    uint32_t l = dxe_lane(v->note);
    if (!s) return;
    v->s[0] = (int32_t)l;
    dx_trigger(&s->v[l]);
    if (l == DV_HATC) dx_choke(&s->v[DV_HATO]);
}

static DXE_XIP_ATTR void dxe_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    dxe_part_t *s = dxe_part((uint32_t)(t - trk));
    uint32_t l = (uint32_t)v->s[0] & (DV_NLANE - 1u), i;
    int32_t y[CTL];
    if (!s) return;
    if (n > CTL) n = CTL;
    dx_run(&dx_kit[l], &s->v[l], p[P_E0], (p[P_E1] - 64) * 3, p[P_E3] - 64, p[P_E2] - 64, p[P_E4] - 64, p[P_E6], p[P_E5], y, n);
    for (i = 0; i < n; i++) out[i] += voice_amp(y[i] + (y[i] >> 2), m, i) << 1;
}

static const char *const DXE_KIT_NAME[] = {"DRUM-X", "909 CLUB", "808 BOOM", "LOFI DUST", "INDUSTRIAL", "ACOUSTIC", "DUB ROOTS",
                                           "TECHNO RAW", "BREAKS"};
static void dxe_kit_load(uint32_t k)                    /* (Jiant's dx_kit_load: the kit, its layers) */
{
    uint32_t l;
    if (k >= DX_NKIT) k = 0;
    for (l = 0; l < 8u; l++) {
        dx_kit[l] = DX_KITS[k].lane[l];
        dx_lay[l] = DX_KITS[k].lay[l];
    }
    dx_tone = 0;
    dx_hands = 0;
}
#define DXE_PRESET(n) {n, {64, 64, 64, 64, 64, 64, 0, 0}, {0, 60, 90, 20}, 0, 0}
static const preset_t DXE_PRESETS[] = {
    DXE_PRESET("DRUM-X"), DXE_PRESET("909 CLUB"), DXE_PRESET("808 BOOM"), DXE_PRESET("LOFI DUST"), DXE_PRESET("INDUSTRIAL"),
    DXE_PRESET("ACOUSTIC"), DXE_PRESET("DUB ROOTS"), DXE_PRESET("TECHNO RAW"), DXE_PRESET("BREAKS"),
};

static const engine_t ENG_DRUMX = {
    .name = "DRUM-X",
    .page_title = {"KIT", "TONE"},
    .edit = {
        {"MRPH", F_PCT, 0, 127, 0, 0, 0},
        {"TUNE", F_BIPCT, 0, 127, 64, 0, 0},
        {"TONE", F_BIPCT, 0, 127, 64, 0, 0},
        {"DECY", F_BIPCT, 0, 127, 64, 0, 0},
        {"NOIS", F_BIPCT, 0, 127, 64, 0, 0},
        {"FM", F_PCT, 0, 127, 0, 0, 0},
        {"WARP", F_PCT, 0, 127, 0, 0, 0},
        {"-", F_INT, 0, 0, 0, 0, 0},
    },
    .presets = DXE_PRESETS,
    .npresets = NELEM(DXE_PRESETS),
    .poly = 8,
    .note_on = dxe_note_on,
    .render = dxe_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
};
