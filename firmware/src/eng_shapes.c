/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.5.1: SHAPES, engine 14 (the slot of Melodee's retired OBXF). Braids-style macro oscillator: 40-odd
 * sound shapes from analog-style waves to physical models and drums, each shaped by TIMBRE and COLOR, ported from the
 * open source code of a well known Eurorack macro oscillator (firmware/src/braids/, MIT: LICENSES/MIT-Mutable.txt;
 * tests/braids_parity checks it sample for sample against the original), with an M8 Macrosynth style voice after it:
 * a filter, drive, and bit and sample-rate reduction (and a DC blocker: some shapes, VOSIM most, sit off zero).
 * This file is the glue to Melodee's voices.
 *
 * The edit pages:
 *   EDIT 1 SHAPE: WAVE (the shape), TIMB (TIMBRE), COLR (COLOR), CUT (the filter's cutoff). HOME's four knobs are
 *                 these too.
 *   EDIT 2 TONE:  RES (the filter's resonance), DRV (drive: soft saturation), BITS (16 down to 2 bits, Braids' own
 *                 steps), RATE (sample-rate reduction, Braids' own steps: off, then 1/2 .. 1/24). (Labels are four
 *                 letters at most: the screen's cells.)
 * ENV -> SHAPE and LFO -> SHAPE move TIMBRE; ENV -> FILTER and LFO -> FILTER move the cutoff. A note strikes the
 * shape (the plucked, struck and drum shapes start a new hit).
 *
 * How it runs: the oscillator is Braids' own code with its own tables, made for the module's 96 kHz. It renders at
 * 88.2 kHz (two samples per output sample) with every pitch raised by 12 log2(96 / 88.2) = 1.47 semitones (188 of
 * Braids' 1/128 semitones), so every table indexed by pitch gives the right frequency; its times (decays) run 8.8 %
 * longer than on the module. A half-band filter takes it down to 44.1 kHz. Its code stays in flash (section
 * .text.xip_shapes: tools/audio_ram.py), as MACRO's.
 * Voices (0.5.2): four per track in POLY (and UNISON), each an oscillator and tone section of its own, for chords.
 * The shapes with delay lines (COMB, PLUCK, BOWED, BLOWN, FLUTE: 8 KB of them) play one at a time.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.5.1: first version.
 *   0.5.2: polyphonic: four voices (SH_POLY) but in the delay line shapes (sh_cap), which share one set of delay lines
 *          (sh_part_t.lines, voice 0's); the pad, keys, bell and choir presets play POLY.
 *   0.5.3: voice units by shape from measured costs (sh_units: 2 to 4; was 2 for all).
 *   0.8.0: sh_render is silent while the asset pack (Braids' tables, oc_pack.h) is not there (OC_PACK_READY).
 *   0.13.7: the hot set (SH_HOT): the sine and the wave shapers read from the arena, not the pack.
 */
#include "braids/pb_tables.h"
/* (Orange Cat 0.13.7) SHAPES' hot set (core.h engine_t hot): the tables its oscillators read every sample, copied into the arena at registration
 * (engines.c eng_hot_take) because a read from the asset pack's plaintext window costs about 250 ns more (RING BELL took 1.9 times 0.6.2's
 * render time; hw/LOG.md). The sine and the four wave shapers, 5 x 514 B = 2,570 B of arena. Captured here, before the render code: the names
 * are then the copies (ENG_HOT). The built-in engine's pointers start at the sources (a host test that registers nothing reads those); a module's
 * are the firmware's (fw_api_t hot). The other Braids tables (the comb wavetables, the resonator, the bowed and blown shapes' envelopes, ..)
 * stay in the pack: tools/pack_hot.py lists which of them are also read per sample, and their cost. */
#define SH_HOT_LIST(X) X(pb_wav_sine, 0) X(pb_ws_moderate_overdrive, 1) X(pb_ws_violent_overdrive, 2) X(pb_ws_sine_fold, 3) X(pb_ws_tri_fold, 4)
#define SH_HOT_N 5u
#define SH_HOT_ENTRY(t, i) {t, 2u * 257u},
static const eng_hot_t SH_HOT[SH_HOT_N] = { SH_HOT_LIST(SH_HOT_ENTRY) };
#ifndef OC_MODULE
#define SH_HOT_SRC(t, i) t,
static const void *sh_hot[SH_HOT_N] = { SH_HOT_LIST(SH_HOT_SRC) };
#endif
_Static_assert(PB_WAV_SINE_SIZE == 257u && PB_WS_MODERATE_OVERDRIVE_SIZE == 257u && PB_WS_VIOLENT_OVERDRIVE_SIZE == 257u &&
               PB_WS_SINE_FOLD_SIZE == 257u && PB_WS_TRI_FOLD_SIZE == 257u, "SHAPES' hot set: the tables' sizes");
#undef pb_wav_sine
#undef pb_ws_moderate_overdrive
#undef pb_ws_violent_overdrive
#undef pb_ws_sine_fold
#undef pb_ws_tri_fold
#define pb_wav_sine ((const int16_t *)ENG_HOT(sh_hot, ENGI_SHAPES, 0))
#define pb_ws_moderate_overdrive ((const int16_t *)ENG_HOT(sh_hot, ENGI_SHAPES, 1))
#define pb_ws_violent_overdrive ((const int16_t *)ENG_HOT(sh_hot, ENGI_SHAPES, 2))
#define pb_ws_sine_fold ((const int16_t *)ENG_HOT(sh_hot, ENGI_SHAPES, 3))
#define pb_ws_tri_fold ((const int16_t *)ENG_HOT(sh_hot, ENGI_SHAPES, 4))
#include "braids/pb_macro.h"
#include "plaits/pl_dsp.h"                                /* (its float filter and soft clip) */

#define SH_XIP __attribute__((section(".text.xip_shapes"), noinline))
#define SH_PITCH_UP 188                                   /* 96 kHz tables at 88.2 kHz: 12 log2(96 / 88.2) x 128 */
#define SH_CHUNK 24u                                      /* Braids' block */

/* the shapes offered, in Braids' order (its easter egg, QUESTION_MARK, is not; nor the four wavetable shapes when the
 * build leaves their 33 KB of waves out) */
static const uint8_t SH_SHAPES[] = {
    PB_SHAPE_CSAW, PB_SHAPE_MORPH, PB_SHAPE_SAW_SQUARE, PB_SHAPE_SINE_TRIANGLE, PB_SHAPE_BUZZ, PB_SHAPE_SQUARE_SUB,
    PB_SHAPE_SAW_SUB, PB_SHAPE_SQUARE_SYNC, PB_SHAPE_SAW_SYNC, PB_SHAPE_TRIPLE_SAW, PB_SHAPE_TRIPLE_SQUARE,
    PB_SHAPE_TRIPLE_TRIANGLE, PB_SHAPE_TRIPLE_SINE, PB_SHAPE_TRIPLE_RING_MOD, PB_SHAPE_SAW_SWARM, PB_SHAPE_SAW_COMB,
    PB_SHAPE_TOY, PB_SHAPE_DIGITAL_FILTER_LP, PB_SHAPE_DIGITAL_FILTER_PK, PB_SHAPE_DIGITAL_FILTER_BP,
    PB_SHAPE_DIGITAL_FILTER_HP, PB_SHAPE_VOSIM, PB_SHAPE_VOWEL, PB_SHAPE_VOWEL_FOF, PB_SHAPE_HARMONICS, PB_SHAPE_FM,
    PB_SHAPE_FEEDBACK_FM, PB_SHAPE_CHAOTIC_FEEDBACK_FM, PB_SHAPE_PLUCKED, PB_SHAPE_BOWED, PB_SHAPE_BLOWN,
    PB_SHAPE_FLUTED, PB_SHAPE_STRUCK_BELL, PB_SHAPE_STRUCK_DRUM, PB_SHAPE_KICK, PB_SHAPE_CYMBAL, PB_SHAPE_SNARE,
#if PB_WAVETABLES
    PB_SHAPE_WAVETABLES, PB_SHAPE_WAVE_MAP, PB_SHAPE_WAVE_LINE, PB_SHAPE_WAVE_PARAPHONIC,
#endif
    PB_SHAPE_FILTERED_NOISE, PB_SHAPE_TWIN_PEAKS_NOISE, PB_SHAPE_CLOCKED_NOISE, PB_SHAPE_GRANULAR_CLOUD,
    PB_SHAPE_PARTICLE_NOISE, PB_SHAPE_DIGITAL_MODULATION,
};
#define SH_N ((int16_t)NELEM(SH_SHAPES))
static const char *const N_SH_SHAPE[] = {
    "CSAW", "MORPH", "SAWSQR", "FOLD", "BUZZ", "SQRSUB", "SAWSUB", "SQRSYN", "SAWSYN", "SAWx3", "SQRx3", "TRIx3",
    "SINx3", "RING", "SWARM", "COMB", "TOY", "ZLPF", "ZPKF", "ZBPF", "ZHPF", "VOSIM", "VOWEL", "VFOF", "HARM", "FM",
    "FBFM", "WTFM", "PLUCK", "BOWED", "BLOWN", "FLUTE", "BELL", "DRUM", "KICK", "CYMBAL", "SNARE",
#if PB_WAVETABLES
    "WTBL", "WMAP", "WLIN", "WTx4",
#endif
    "NOISE", "TWINQ", "CLKN", "CLOUD", "PRTCL", "QPSK",
};
_Static_assert(NELEM(N_SH_SHAPE) == NELEM(SH_SHAPES), "a name per shape");
static const char *const N_SH_RATE[] = {"OFF", "1/2", "1/3", "1/4", "1/6", "1/12", "1/24"};
static const uint8_t SH_DECIMATE[7] = {1, 2, 3, 4, 6, 12, 24};   /* braids.cc decimation_factors, reversed */
static const uint16_t SH_BITS_MASK[7] = {0xffff, 0xfff0, 0xff00, 0xf800, 0xf000, 0xe000, 0xc000};   /* bit_reduction_masks, reversed */
static const char *const N_SH_BITS[] = {"16", "12", "8", "5", "4", "3", "2"};

#define SH_POLY 4u                                        /* (0.5.2) voices per track; the delay line shapes: 1 */
typedef struct {                                          /* (0.5.2) a voice's */
    pb_macro_t osc;
    uint8_t started, strike;
    uint8_t rate_count;                               /* the sample-rate reduction's counter */
    int16_t held;
    float hb[7];                                      /* the half-band filter's history */
    pl_svf_t filter;
    float dc_x, dc_y;                                 /* the DC blocker (VOSIM and others sit off zero) */
} sh_voice_t;
typedef struct {
    sh_voice_t v[SH_POLY];
                                                      /* ((0.14.3) its 88.2 kHz block buffer is the voice kernel's own: sh_voice) */
    uint8_t sync[SH_CHUNK];                           /* (no sync input: zeros) */
    pb_delay_lines_t lines;                           /* (0.5.2) voice 0's delay lines (braids/pb_digital.h) */
} sh_part_t;
static sh_part_t *sh_part(uint32_t part);           /* engines.c */
static inline int sh_lines(int shape)               /* (0.5.2) the shapes with delay lines */
{
    return shape == PB_SHAPE_SAW_COMB || shape == PB_SHAPE_PLUCKED || shape == PB_SHAPE_BOWED ||
           shape == PB_SHAPE_BLOWN || shape == PB_SHAPE_FLUTED;
}
static SH_XIP uint32_t sh_cap(const track_t *t)
{
    return sh_lines(SH_SHAPES[clamp(t->p[P_E0], 0, SH_N - 1)]) ? 1u : SH_POLY;
}

static SH_XIP void sh_note_on(track_t *t, voice_t *v)
{
    sh_part_t *s = sh_part((uint32_t)(t - trk));
    if (s) s->v[(uint32_t)(v - t->v) % SH_POLY].strike = 1;
}

/* (Orange Cat 0.14.3) a voice's block in two parts (audio_batch.h; in the module through FW_API's batch_push): sh_render, on CPU0 at the
 * voice's turn, the control (the oscillator's start, shape, parameters, pitch and strike, the tone section's settings) into a descriptor;
 * sh_voice, the oscillator at 88.2 kHz into its own buffer (on its stack: the track's one, shared by its voices, would be written by both
 * cores) and the tone section, from the descriptor alone: on CPU1 or CPU0 as the batch splits the block's voices, or at once (no batch;
 * Braids' digital shapes, which draw its shared generator pb_rng_state: their draws stay in voice order on CPU0, bit for bit). It
 * touches its voice's state (P->v[vi], voice 0's delay lines) and read-only tables only (P->sync: zeros) */
typedef struct {
    sh_voice_t *s;
    const uint8_t *sync;
    vmod_t m;
    uint32_t n, dec;
    uint16_t mask;
    uint8_t filtering;
    float drive, pre, post;
} sh_job_t;
_Static_assert(sizeof(sh_job_t) <= AUDIO_BATCH_BYTES, "SHAPES' batch descriptor");
static SH_XIP void sh_voice(const void *data, int32_t *out)
{
    const sh_job_t *j = data;
    sh_voice_t *s = j->s;
    const vmod_t *m = &j->m;
    int16_t buf[2 * CTL];
    uint32_t i, done, n = j->n, dec = j->dec;
    uint16_t mask = j->mask;
    float drive = j->drive, pre = j->pre, post = j->post;
    for (done = 0; done < 2u * n;) {                  /* 2 n samples at 88.2 kHz, in Braids' blocks of 24 */
        uint32_t k = 2u * n - done < SH_CHUNK ? 2u * n - done : SH_CHUNK;
        pb_macro_render(&s->osc, j->sync, buf + done, k);
        done += k;
    }
    for (i = 0; i < n; i++) {
        float *h = s->hb, y;
        h[0] = h[2]; h[1] = h[3]; h[2] = h[4]; h[3] = h[5]; h[4] = h[6];
        h[5] = (float)buf[2 * i];
        h[6] = (float)buf[2 * i + 1];
        /* the 7-tap half-band (-1 0 9 16 9 0 -1) / 32: flat to 10 kHz, -40 dB past 30 kHz; a new output every two
         * inputs */
        y = 0.5f * h[3] + 0.28125f * (h[2] + h[4]) - 0.03125f * (h[0] + h[6]);
        y = y > 32767.0f ? 32767.0f : y < -32768.0f ? -32768.0f : y;
        if (dec > 1u) {                               /* hold every dec-th sample */
            if (s->rate_count == 0) s->held = (int16_t)y;
            s->rate_count = (uint8_t)((s->rate_count + 1u) % dec);
            y = s->held;
        }
        y = (float)(int16_t)((int32_t)y & (int16_t)mask);
        y *= 0.7f / 32768.0f;                         /* (-3 dB: Braids plays near full scale; MACRO's level) */
        {                                             /* DC blocker, about 7 Hz */
            float x = y;
            y = x - s->dc_x + 0.999f * s->dc_y;
            s->dc_x = x;
            s->dc_y = y;
        }
        if (j->filtering) y = pl_svf_process(&s->filter, PL_LP, y);
        if (drive > 0.0f) y = pl_soft_clip(y * pre) * post * (1.0f + drive);
        y = y > 1.0f ? 1.0f : y < -1.0f ? -1.0f : y;
        out[i] += voice_amp((int32_t)(y * 32767.0f) * 2, m, i) * 2;
    }
    pl_flush_svf(&s->filter);
    pl_flush(&s->dc_y);
}

static SH_XIP void sh_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    sh_part_t *P = sh_part((uint32_t)(t - trk));
    const int16_t *p = t->p;
    uint32_t vi = (uint32_t)(v - t->v) % SH_POLY;
    sh_voice_t *s;
    sh_job_t j;
    if (!P || !OC_PACK_READY()) return;               /* (Orange Cat 0.8.0: the asset pack, with Braids' tables, is not there: silent) */
    s = &P->v[vi];
    int32_t shape_ix = clamp(p[P_E0], 0, SH_N - 1);
    if (sh_lines(SH_SHAPES[shape_ix]) && vi)          /* (voices left from a shape of four: silent to their end) */
        return;
    if (!s->started) {
        pb_macro_init(&s->osc);
        pl_svf_init(&s->filter);
        s->osc.shape = -1;                            /* (so the first shape set strikes) */
        s->started = 1;
    }
    s->osc.digital_oscillator.delay_lines = vi ? 0 : &P->lines;
    int32_t timbre = clamp(p[P_E1] * 258 + (m->shape - (64 << 8)) * 2, 0, 32767);
    int32_t color = clamp(p[P_E2] * 258, 0, 32767);
    /* the voice's pitch in Braids' 1/128 semitones (pitch16 is 1/16; the residual in 1/4096 of the frequency, 0.541
     * of a 1/128 semitone each; A4 in Q24 octaves), raised for 88.2 kHz */
    int32_t pitch = m->pitch16 * 8 + ((m->fine * 554) >> 10) + (tuning_log() * 3 >> 15) + SH_PITCH_UP;
    pitch = clamp(pitch, 0, 16383);
    pb_macro_set_shape(&s->osc, SH_SHAPES[shape_ix]);
    pb_macro_set_parameters(&s->osc, (int16_t)timbre, (int16_t)color);
    pb_macro_set_pitch(&s->osc, (int16_t)pitch);
    if (s->strike) {
        pb_macro_strike(&s->osc);
        s->strike = 0;
    }
    /* the tone section, at 44.1 kHz: half-band down (coefficients for a 7-tap half-band, 0.5 at the centre),
     * sample-rate and bit reduction, the filter, drive */
    j.dec = SH_DECIMATE[clamp(p[P_E7], 0, 6)];
    j.mask = SH_BITS_MASK[clamp(p[P_E6], 0, 6)];
    float cut = (float)clamp(p[P_E3] * 256 + m->cutoff, 0, 127 * 256) * (1.0f / 256.0f);
    float f = 20.0f * pl_semitones_to_ratio(cut * (120.0f / 127.0f)) / 44100.0f;   /* 20 Hz .. 20 kHz */
    float res = 0.5f + (float)clamp(p[P_E4], 0, 127) * (20.0f / 127.0f);
    j.drive = (float)clamp(p[P_E5], 0, 127) * (1.0f / 127.0f);
    j.pre = 1.0f + j.drive * j.drive * 15.0f;
    j.post = 1.0f / (1.0f + j.drive * 2.0f);
    j.filtering = (uint8_t)(p[P_E3] < 127 || m->cutoff < 0);
    if (f > 0.45f) f = 0.45f;
    pl_svf_set_f_q(&s->filter, PL_FREQ_ACCURATE, f, res);   /* ((0.14.3) before the oscillator's block: neither reads the other) */
    j.s = s; j.sync = P->sync; j.m = *m; j.n = n;
    if (audio_batch_active && SH_SHAPES[shape_ix] < PB_SHAPE_TRIPLE_RING_MOD) {
        audio_batch_push(sh_voice, &j, sizeof j, out, n, AP_SHAPES);
        return;
    }
    sh_voice(&j, out);
}

/* the voice budget's units a voice takes (voice.c; a CZ-1 voice 2, 16 in all). (0.5.3) Measured on the host against
 * CZ-1 INIT TONE (as eng_macro.c MC_UNITS): most shapes 1.2x to 1.8x (2 units), the three-oscillator, folding, additive
 * and FM ones about 2x (3), DRUM 3x (4) */
static SH_XIP uint32_t sh_units(const track_t *t)
{
    switch (SH_SHAPES[clamp(t->p[P_E0], 0, SH_N - 1)]) {
    case PB_SHAPE_SINE_TRIANGLE: case PB_SHAPE_TRIPLE_SQUARE: case PB_SHAPE_TRIPLE_TRIANGLE:
    case PB_SHAPE_DIGITAL_FILTER_LP: case PB_SHAPE_HARMONICS: case PB_SHAPE_FM: case PB_SHAPE_FEEDBACK_FM:
    case PB_SHAPE_STRUCK_BELL:
        return 3u;
    case PB_SHAPE_STRUCK_DRUM:
        return 4u;
    default:
        return 2u;
    }
}

/* name, {SHAPE, TIMBRE, COLOR, CUTOFF, RES, DRIVE, BITS, RATE}, {A D S R}, ENV -> FILTER, mono (0.5.2: 0, POLY, for
 * the pads, keys, bells and the choir: they play a step's four notes) */
static const preset_t SH_PRESETS[] = {
    {"CSAW BASS", {0, 40, 50, 70, 30, 20, 0, 0}, {0, 60, 80, 20}, 30, 1},
    {"MORPH LEAD", {1, 64, 64, 100, 20, 0, 0, 0}, {0, 70, 110, 40}, 10, 1},
    {"SYNC LEAD", {8, 70, 40, 110, 10, 30, 0, 0}, {0, 70, 110, 40}, 0, 1},
    {"SUB BASS", {5, 50, 90, 60, 20, 0, 0, 0}, {0, 60, 100, 20}, 20, 1},
    {"TRIPLE SAW PAD", {9, 40, 70, 90, 10, 0, 0, 0}, {70, 90, 110, 90}, 0, 0},
    {"RING BELL", {13, 70, 50, 127, 0, 0, 0, 0}, {0, 90, 0, 80}, 0, 0},
    {"SAW SWARM", {14, 80, 60, 100, 0, 0, 0, 0}, {40, 90, 110, 80}, 0, 0},
    {"TOY CHIP", {16, 64, 40, 127, 0, 0, 2, 2}, {0, 60, 90, 30}, 0, 1},
    {"RESO LPF", {17, 50, 90, 127, 0, 0, 0, 0}, {0, 70, 90, 40}, 0, 0},
    {"VOSIM VOX", {21, 50, 60, 127, 0, 0, 0, 0}, {20, 80, 110, 60}, 0, 1},
    {"VOWEL", {22, 30, 60, 127, 0, 0, 0, 0}, {10, 80, 110, 60}, 0, 1},
    {"FOF CHOIR", {23, 60, 40, 120, 0, 0, 0, 0}, {60, 90, 110, 90}, 0, 0},
    {"HARMONICS", {24, 60, 70, 127, 0, 0, 0, 0}, {10, 80, 110, 60}, 0, 0},
    {"FM KEYS", {25, 50, 40, 127, 0, 0, 0, 0}, {0, 90, 40, 70}, 0, 0},
    {"FEEDBACK FM", {26, 70, 50, 120, 0, 10, 0, 0}, {0, 70, 100, 40}, 0, 1},
    {"PLUCK", {28, 60, 64, 127, 0, 0, 0, 0}, {0, 127, 127, 96}, 0, 1},
    {"BOWED", {29, 60, 64, 127, 0, 0, 0, 0}, {40, 90, 120, 80}, 0, 1},
    {"BLOWN", {30, 60, 64, 127, 0, 0, 0, 0}, {30, 90, 120, 70}, 0, 1},
    {"FLUTE", {31, 60, 64, 127, 0, 0, 0, 0}, {30, 90, 120, 70}, 0, 1},
    {"BELL", {32, 60, 64, 127, 0, 0, 0, 0}, {0, 127, 127, 104}, 0, 0},
    {"TOM", {33, 60, 64, 127, 0, 0, 0, 0}, {0, 127, 127, 96}, 0, 1},
    {"KICK", {34, 64, 64, 127, 0, 20, 0, 0}, {0, 127, 127, 96}, 0, 1},
    {"CYMBAL", {35, 64, 64, 127, 0, 0, 0, 0}, {0, 127, 127, 100}, 0, 1},
    {"SNARE", {36, 64, 64, 127, 0, 0, 0, 0}, {0, 127, 127, 90}, 0, 1},
};

static const engine_t ENG_SHAPES = {
    .name = "SHAPES",
    .page_title = {"SHAPE", "TONE"},
    .edit = {
        {"WAVE", F_ENUM, 0, SH_N - 1, 0, N_SH_SHAPE, 0},
        {"TIMB", F_PCT, 0, 127, 64, 0, 0},
        {"COLR", F_PCT, 0, 127, 64, 0, 0},
        {"CUT", F_PCT, 0, 127, 127, 0, 0},
        {"RES", F_PCT, 0, 127, 0, 0, 0},
        {"DRV", F_PCT, 0, 127, 0, 0, 0},
        {"BITS", F_ENUM, 0, 6, 0, N_SH_BITS, 0},
        {"RATE", F_ENUM, 0, 6, 0, N_SH_RATE, 0},
    },
    .presets = SH_PRESETS,
    .npresets = NELEM(SH_PRESETS),
    .poly = SH_POLY,
    .cap = sh_cap,                                     /* (0.5.2) the delay line shapes: 1 */
    .note_on = sh_note_on,
    .render = sh_render,
    .units = sh_units,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .hot = SH_HOT, .nhot = SH_HOT_N,                  /* (0.13.7) the hot set, above */
#ifndef OC_MODULE
    .hot_at = sh_hot,
#endif
};
