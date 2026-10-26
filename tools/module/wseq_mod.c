/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * The engine: Clipper's WSEQ (https://github.com/isod89/clipper-fm1, commit e690672: firmware/src/eng_wseq.c, Copyright (C) 2026 3dSam (Clipper),
 * GPL-3.0-only), the copy in tools/module/clipper/ up to its engine_t; the pieces of Clipper's dsp.c it reads from
 * tools/module/clipper/clipper_dsp.h.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Clipper"): WSEQ (wave sequencing after the Wavestation: each note
 * plays a sequence of steps, each its own wave, pitch and level, the next fading in over the end of the one before; FWD BACK PING ONCE; a 12 dB
 * low-pass) as a loadable module translation unit (stage 0).
 * The proposed engine number is 38 (core.h ENGI_WSEQ; registrable only in an OC_CANDIDATES build).
 *   - EDIT 1 (SEQUENCE) and EDIT 2 (TONE): Clipper's eight; presets: Clipper's, through cl_preset_make (their sends, arp and extras dropped);
 *   - the waves: Clipper's WSEQ reads FMX's (1024-point tables, 26,624 B, per sample); here wt_shared.h's bank of the same eight waves
 *     (tools/module/clipper/wseq_bank.h from gen_wseq_bank.py: FMX's waves re-band-limited into the three copies, 10,752 B), the module's hot set,
 *     through the module's fmx_wave (SINE and NOISE as FMX computes them; the others the bank's copy for the step's increment: two eng_wseq.c
 *     lines pass it, marked). Not Clipper's samples: a cleaner top octave (FMX's TRI HALF ABS QUART SIN8 have no high-note copy);
 *   - the tempo: a step is a note value at the song's tempo, which a module cannot read (the services table has no tempo): 120 BPM (WSEQ's own
 *     fallback, song.g[G_BPM] read as 0 here);
 *   - voice state in voice_t (ph[0..2], s[0..5]): no arena block (the 4-byte token).
 */
#include "clipper/clipper_unit.h"
#include "clipper/wt_shared.h"
#include "clipper/wseq_bank.h"
static const eng_hot_t WSEQ_HOT[1] = { { WSEQ_BANK, sizeof WSEQ_BANK } };
static const struct { int16_t g[G_BPM + 1]; } cl_song = { { 0 } };   /* the song's tempo is the firmware's: WSEQ's 120 BPM fallback */
#define song cl_song
/* FMX's wave w at phase ph for increment inc: SINE and NOISE as FMX computes them, the others wt_shared.h's bank (the hot set's copy) */
static inline int32_t fmx_wave(uint32_t w, uint32_t ph, uint32_t inc, uint32_t *rnd)
{
    uint32_t lv;
    if (w == FXW_SINE)
        return sine_i(ph);
    if (w == FXW_NOISE) {
        *rnd = *rnd * 1664525u + 1013904223u;
        return (int32_t)(*rnd >> 16) - 32768;
    }
    lv = wts_level(inc);
    return wts_lerp((const int16_t *)ENG_HOT(0, ENGI_WSEQ, 0) + (w - 1u) * WTS_PTS + WTS_OFF[lv], ph, wts_shift(lv), wts_mask(lv));
}
#define preset_t cl_preset_t
#include "clipper/eng_wseq.c"
#undef preset_t
#undef song
_Static_assert(FXW_TRI == 1 && FXW_SIN8 == 8 && WSEQ_BANK_N == 8u, "WSEQ_BANK holds FMX's waves TRI .. SIN8");
static void wseq_preset_make(uint32_t k, preset_t *p, char *name) { cl_preset_make(WSEQ_PRESETS, NELEM(WSEQ_PRESETS), k, p, name); }
static const engine_t ENG_WSEQ = {           /* Clipper's ENG_WSEQ in Orange Cat's order (its fil_page 1 and colour 0x07FF have no field here) */
    .name = "WSEQ", .page_title = {"SEQUENCE", "TONE"},
    .edit = {
        {"SEQ", F_ENUM, 0, WSEQ_NSEQ - 1, 0, N_WSEQ, 0},
        {"RATE", F_INT, 0, 127, 40, N_WSEQ_RATE, 0},
        {"XFADE", F_PCT, 0, 127, 20, 0, 0},
        {"LOOP", F_ENUM, 0, 3, 0, N_WSEQ_LOOP, 0},
        {"START", F_INT, 0, 127, 0, 0, 0},
        {"PITCH", F_PCT, 0, 127, 64, 0, 0},
        {"CUT", F_CUTOFF, 0, 127, 110, 0, 0},
        {"RES", F_PCT, 0, 127, 10, 0, 0},
    },
    .preset_make = wseq_preset_make, .npresets = NELEM(WSEQ_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E6},
    .note_on = wseq_note_on, .render = wseq_render,
    .hot = WSEQ_HOT, .nhot = 1,
};
CL_MODULE_HEADER(ENGI_WSEQ, ENG_WSEQ, OC_STATE_TOKEN)
