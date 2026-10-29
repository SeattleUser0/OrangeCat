/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat 0.6.0: the effects, ported from JIANT FM (github.com/juanjiant-bit/JIANT, main dc8d025, v0.9.18: fx.c,
 * slicer.c, perform.c). They replace Melodee's fx.c (the three-slot inserts and shared slots: PHASER .. CHORUS).
 *
 * What there is (the names are Jiant's; its comments are kept where the DSP is its):
 *   per track   DIST (SOFT HARD FOLD CRUSH RECT AMP; DRIVE, TYPE, TONE), AUTO PAN (DEPTH RATE WAVE SYNC), three sends
 *               (CHORUS, DELAY, REVERB), the SLICER route;
 *   buses       CHORUS (an ensemble: RATE DEPTH STEREO), DELAY (ANALOG / DIGITAL, synced / free TIME, FEEDBK past 100 %,
 *               resonant TONE, MOD (wow and flutter), WIDE, PING, GRAIN: PITCH SPRAY; half and quarter rate for the long
 *               times), REVERB (ROOM SPRING SHIMMER RESO CLOUD: SIZE DAMP PRE/AIR/PITCH, MOD RATE FILT WIDE; CLOUD FREEZE
 *               at SIZE 127), their gate and rest;
 *   the SLICER  (jfx_slicer.c) GATE / STUT on its own bus, 16 patterns, the rate, the pitch sequence, the routing;
 *   the master  DJ FILTER, CLIP, the LEVELER (Orange Cat: a setting), the speaker EQ (LOWCUT, BASS+), the limiter and
 *               the soft clip; the FX layer's punch-in effects (jfx_perform.c: REPEAT, GLITCH, TAPE STOP, SHUFFLE,
 *               FREEZE, the CRUSH and THROW sweeps, the track mutes).
 *
 * Signal path (one block; mix.c mix_block):
 *   track: engine -> DIST -> mute / solo ramp (the FX layer's mutes too) -> LEVEL -> the sends (mono, pre-pan) and,
 *          after PAN (+ AUTO PAN) and SPREAD, the dry mix or the SLICER's bus (the route on, the SLICER running);
 *          a frozen phrase's take (card_freeze.c) joins after its engine at the same place, its DIST and pan in it;
 *   buses: the three sends -> CHORUS, DELAY, REVERB (CPU1 when it is there, one job; CPU0 meanwhile the SLICER when its
 *          FX is OFF) -> a mono return and a side (left +, right -);
 *   then:  the sides into the mix; the return into the SLICER's bus (SL FX ON) or kept; the SLICER; the DJ FILTER; CLIP;
 *          the LEVELER's reading; MASTER (the knob); the FX layer's buffer effects and CRUSH; master_out: DC block, the
 *          speaker EQ, the LEVELER's gain, the limiter, the soft clip. USB audio's track stems are taken in mix.c as
 *          before (each track after its level, before the buses).
 * The freeze recorder takes a track's stem as it reaches the dry mix or the SLICER's bus (after DIST, LEVEL, PAN and
 * AUTO PAN; before the buses and the master), and a frozen take is mixed there: it gets the sends, never DIST again.
 *
 * Memory (the decision in docs/orangecat/FX_MIGRATION.md): Jiant keeps its lines static (152 KB). Here each bus takes
 * its buffers from the audio arena (resources.c) on its first send and gives them back when the buses have rested
 * (jfx_mem): the delay's 64 KB line in two 32 KB halves (no arena bank holds 64 KB in one piece), the reverb's combs and
 * allpasses (11.9 KB) and its model line (CLOUD 32 KB; SHIMMER and the pre-delay 4 KB), the chorus's 4 KB; the SLICER's
 * STUT and the FX layer share 32 KB. The sizes are Jiant's. A bus that cannot have its memory plays nothing and
 * jfx_full says which (the pages: FX FULL). CPU0 allocates and frees (the audio interrupt), before the job goes to CPU1.
 *
 * Code: in flash (.text.xip_jfx; tools/audio_ram.py leaves it there): the audio interrupt never runs while the flash is
 * written, and CPU1 runs a bus job only when the interrupt gave it one.
 *
 * Not here (they need parts of Jiant Orange Cat does not have): DUCK and PUNCH (DRUM-X's kick), the load governor's
 * lighter models (Orange Cat always runs the full ones), the modulation matrix's bus destinations, USB stems.
 *
 * Revisions:
 * (Orange Cat 0.12.2) sanitizer fixes, project-record fuzzer, editor-protocol check
 *   0.6.0: first version (Jiant 0.9.18's effects).
 */
#include "dubdelay.h"                                  /* (Release 2) dd_t, for the size of an instance's state */
#define JFX_XIP __attribute__((section(".text.xip_jfx"), noinline))

/* ------------------------------------------------------------------ memory --- */
#ifndef DLY_LEN
#define DLY_LEN 32768u                                  /* the delay's line, int16 samples (Jiant params.c): 64 KB */
#endif
#define JFX_SEG 2048u                                   /* (0.6.0 rev 2) the long lines in 4 KB arena blocks: the
                                                         * arena's banks are 4.6 .. 54 KB (resources.c), and a line in
                                                         * one or two pieces fitted only an empty main bank */
#define JFX_SEG_SH 11u
#define DLY_NSEG (DLY_LEN / JFX_SEG)                    /* the delay's line: 16 blocks */
_Static_assert(JFX_SEG == 1u << JFX_SEG_SH && DLY_LEN % JFX_SEG == 0u && (DLY_NSEG & (DLY_NSEG - 1u)) == 0u,
               "the delay's line: a power of two of 4 KB blocks");
#define CHO_LEN 2048u
#define REV_NCOMB (1116u + 1188u + 1277u + 1356u)
#define REV_NAP (556u + 441u)
typedef struct {
    int16_t comb[REV_NCOMB + 1u];                       /* ROOM's combs; SPRING's loop; RESO's strings (+1: even) */
    union { int16_t ap[REV_NAP + 1u]; int32_t sp[(REV_NAP + 1u) / 2u]; } u;   /* ROOM's allpasses; SPRING's chain */
} jrev_t;
#define RPRE_LEN 2048u                                  /* the reverb's pre-delay line, at half rate (4 KB): up to 92 ms */
#define SH_LEN 2048u                                    /* SHIMMER's line (4 KB) */
#define CL_LEN 16384u                                   /* .. CLOUD's, half rate: ~0.74 s (32 KB) */
#define RX_NSEG (CL_LEN / JFX_SEG)                      /* the model line: 8 blocks for CLOUD, the first one for the
                                                         * others (the pre-delay, SHIMMER's line) */
_Static_assert(RPRE_LEN == JFX_SEG && SH_LEN == JFX_SEG && (RX_NSEG & (RX_NSEG - 1u)) == 0u,
               "the model line: the pre-delay and SHIMMER in its first block");
#define SL_FR 8192u                                     /* the SLICER's recording: stereo frames (32 KB) */
/* (Release 2) DUB TAPE's lines, in the same 4 KB blocks (design 3, BUILD.md): a state handle and a spring handle for each of the four tracks, and a pool of
 * blocks the four share, an instance's line being the blocks it holds (any number from 1 to 16, in a ring of that many; nothing says they are side by side),
 * so a line can sit in the main bank, the cache bank and the card's lent slots as the delay's does. */
#define DUB_N 4u                                        /* instances: one a track */
#define DUB_NSEG 24u                                    /* the pool: 96 KB of blocks, shared (four lines of 24K, or one of 64K and two of 16K) */
#define DUB_MAXSEG 16u                                  /* one line: 64 KB */
#define DUB_STATE_BYTES ((uint32_t)((sizeof(dd_t) + 3u) & ~3u))   /* dd_t on the target (the review's 32-bit size, not the host's), rounded */
#define DUB_SPRING_BYTES 5632u                          /* the spring's rings (DD_SP_BUF int16) */
/* what only a running instance needs (the job's parameter copies, the capture pair, the wet, the batch's sink) is one arena block, taken with the first instance and given back with the
 * last: DUB unused costs the build its code and a few hundred bytes, not these 3 KB */
typedef struct { dd_params_t p[DUB_N]; int32_t x[2][DUB_N][32]; float wet[DUB_N][2][32]; int32_t sink[32];
                 uint8_t pool[DUB_N][DUB_MAXSEG]; int16_t *line[DUB_N][DUB_MAXSEG]; } dub_common_t;   /* .. and each line block's pool slot and pointer */
static dub_common_t *dub_cm;                            /* 0: no instance holds it */
#define dub_pool (dub_cm->pool)
#define dub_line (dub_cm->line)
enum { JB_REV, JB_CHO, JB_SL, JB_DLY0, JB_RX0 = JB_DLY0 + DLY_NSEG, JB_DUBS0 = JB_RX0 + RX_NSEG, JB_DUBP0 = JB_DUBS0 + DUB_N,
       JB_DUBG0 = JB_DUBP0 + DUB_N, JB_DUBC = JB_DUBG0 + DUB_NSEG, JB_N = JB_DUBC + 1 };
_Static_assert(JB_N == RES_JFX_N, "resources.c: an arena handle a block");
/* the arena's blocks now (0: not held); CPU0 sets them before a job, the job reads them */
static int16_t *dly_seg[DLY_NSEG], *rx_seg[RX_NSEG], *cho_buf;
static uint8_t dly_have, rx_have;                       /* (0.6.0 rev 2) the blocks held, from the first */
static __attribute__((section(".pool"))) uint8_t dub_want[DUB_N], dub_spring[DUB_N];      /* (Release 2) the blocks each instance wants (0: off) and whether it wants the spring: CPU0 sets
                                                         * them before jfx_mem, from the tracks */
static __attribute__((section(".pool"))) uint8_t dub_held_sp[DUB_N];                      /* the spring the held memory was taken for */
static __attribute__((section(".pool"))) uint16_t dub_sig[DUB_N];
static __attribute__((section(".pool"))) uint32_t dub_gen[DUB_N];         /* .. and the want (blocks, spring) it was short for: a new want asks again */
static __attribute__((section(".pool"))) uint8_t dub_have[DUB_N], dub_held[DUB_N], dub_idle[DUB_N], dub_fresh[DUB_N], dub_ready[DUB_N];   /* blocks held; the want they were taken for; blocks idle;
                                                         * a line to set up; the memory complete */
static __attribute__((section(".pool"))) uint8_t dub_nomem, dub_full_now;                 /* the arena was short for an instance this block; the DUB FULL bits this block */
static __attribute__((section(".pool"))) int16_t *dub_spbuf[DUB_N];
static void *dub_state[DUB_N] __attribute__((section(".pool")));
#define jrev_x rx_seg[0]                                /* the model line's first block (the pre-delay, SHIMMER) */
static uint32_t jfx_grow, jfx_wait;                     /* (0.6.0 rev 2) new blocks taken this block; blocks to wait
                                                         * before more (jfx_mem) */
static jrev_t *jrev;
static int16_t (*sl_buf)[2];
#define jrev_x_len ((uint32_t)rx_have * JFX_SEG)        /* the model line's samples held (CL_LEN, or SH_LEN = RPRE_LEN) */
static volatile uint8_t jfx_full;                       /* a bit a bus (1 chorus, 2 delay, 4 reverb, 8 SLICER / FX layer)
                                                         * that wanted memory and had none (the pages: FX FULL) */
#define JFX_FULL_CHO 1u
#define JFX_FULL_DLY 2u
#define JFX_FULL_REV 4u
#define JFX_FULL_SL 8u
#define JFX_FULL_DUB0 0x10u                             /* (Release 2) 0x10 .. 0x80: DUB TAPE on track 1 .. 4 wanted memory and had none (DUB FULL) */

/* (Melodee's fx.c, kept: mix.c and card_freeze.c scale with them) a * k / 65536 (k Q16) without 64 bits; x / 2^shift
 * as C's division rounds */
static inline __attribute__((always_inline)) int32_t fx_scale(int32_t a, uint32_t k)
{ return (a >> 16) * (int32_t)k + (int32_t)(((uint32_t)(a & 65535) * k) >> 16); }
static inline __attribute__((always_inline)) int32_t fx_div_pow2(int32_t x, uint32_t shift)
{ return (x + (int32_t)(((uint32_t)x >> 31) * ((1u << shift) - 1u))) >> shift; }

/* the delay's line, sample i (the two halves as one ring) */
static inline __attribute__((always_inline)) int32_t dly_get(uint32_t i)
{
    return dly_seg[(i >> JFX_SEG_SH) & (DLY_NSEG - 1u)][i & (JFX_SEG - 1u)];
}
static inline __attribute__((always_inline)) void dly_put(uint32_t i, int32_t v)
{
    dly_seg[(i >> JFX_SEG_SH) & (DLY_NSEG - 1u)][i & (JFX_SEG - 1u)] = (int16_t)v;
}
/* CLOUD's line, sample i (its blocks as one ring) */
static inline __attribute__((always_inline)) int32_t cl_get(uint32_t i)
{
    return rx_seg[(i >> JFX_SEG_SH) & (RX_NSEG - 1u)][i & (JFX_SEG - 1u)];
}
static inline __attribute__((always_inline)) void cl_put(uint32_t i, int32_t v)
{
    rx_seg[(i >> JFX_SEG_SH) & (RX_NSEG - 1u)][i & (JFX_SEG - 1u)] = (int16_t)v;
}

/* ------------------------------------------------------- tempo, the clock --- */
/* Jiant's transport grid (mod.c clk_n, clk_pos, ms_clock): the 1/16s since PLAY, the samples into the one now (CLK_START:
 * stopped), the samples since PLAY on the grid. Orange Cat's sequencer has its own per track; this one is kept here,
 * from the block the sequencer starts in, at the block's start while the block is processed (jfx_clock_end moves it on).
 * With an external clock it is taken each block from the sequencer's own place (seq.c seq_beats, beat_pos, in the
 * clock's units), so it never drifts from the steps (0.6.0 rev 2: it ran on the measured tempo before, and could). */
#define CLK_START 0xFFFFFFFFu
static uint32_t clk_n, clk_pos = CLK_START, ms_clock, jfx_starts;
static volatile uint32_t beat_pos, seq_beats;           /* (seq.c's: declared here as the unity build reads it first) */
static void jfx_clock_begin(void)
{
    if (!song.playing) {
        clk_pos = CLK_START;
        return;
    }
    if (clk_pos == CLK_START || jfx_starts != seq_start_count) {   /* started (or RESTART): the grid from 0 */
        jfx_starts = seq_start_count;
        clk_n = 0;
        clk_pos = 0;
        jfx_clock_started();                            /* (the SLICER, the FX layer) */
    }
    if (midi_sync_active) {                             /* external clock: the sequencer's place, in samples */
        uint32_t six = MIDI_BEAT_UNITS / 4u, u = beat_pos, p = div_samples(2);
        clk_n = seq_beats * 4u + u / six;
        clk_pos = p ? (uint32_t)((uint64_t)(u % six) * p / six) : 0u;
    }
    ms_clock = clk_n * div_samples(2) + clk_pos;
}
static void jfx_clock_end(uint32_t n)
{
    uint32_t p = div_samples(2);
    if (clk_pos == CLK_START || !p) return;
    clk_pos += n;
    while (clk_pos >= p) {
        clk_pos -= p;
        clk_n++;
    }
}

/* ------------------------------------------------------------------- DIST --- */
/* DIST: low cut -> drive (1x..8x, exponential) -> asymmetric soft clip
 * (a little bias = even harmonics) -> tone low-pass that closes with drive ->
 * make-up gain (straight into tanh over the full band, it would sound like a
 * broken digital fuzz).
 * (JIANT) DIST TYPE (P_DTYPE): SOFT the biased tanh as before; HARD a hard clip; FOLD a triangle wavefolder (the drive
 * folds the wave back on itself: more partials as it rises); CRUSH fewer bits and a held sample (the drive takes both
 * down); RECT full-wave rectified (an octave up, fuzz), its DC taken out. Then the two-pole low-pass, TONE (P_DTONE)
 * moving it darker / brighter, and the make-up. DRIVE 0: out of the chain.
 * (0.7.8) AMP, a guitar amp: two triode stages, each a biased tanh (the first leaning one way, the second the other:
 * even and odd harmonics), a coupling high-pass between them (~110 Hz: the bias moves with the level, the amp's sag
 * and bloom), the second stage's gain rising with the drive (clean .. crunch .. lead); then a cabinet: the two-pole
 * low-pass at a speaker's corner (TONE moves it ~2.5 .. 6 kHz) and a resonance at ~1.8 kHz (a state-variable band
 * added: the presence a 12" cone has).
 * (Orange Cat) The state is jfx's, per track and side: a track with SPREAD has two signals (its voices' two sides,
 * mix.c), each through its own DIST; one blend for both. */
enum { DT_SOFT, DT_HARD, DT_FOLD, DT_CRUSH, DT_RECT, DT_AMP };
#define AMP_B1 7000                                     /* the stages' biases (Q15 into tanh) */
#define AMP_B2 (-3500)
typedef struct { int32_t hp, lp1, lp2, dc, hold, cab_l, cab_b; uint8_t hc; } dist_t;
static dist_t dist_st[NTRK][2];
static int32_t dist_w[NTRK];                            /* the blend the last block ended at, Q12 */
static JFX_XIP int32_t dist_shape(dist_t *t, uint32_t type, int32_t x, int32_t g, int32_t d)
{
    int32_t v = ((x >> 2) * g) >> 10, a;
    switch (type) {
    case DT_HARD:
        return clamp(v, -22000, 22000);
    case DT_FOLD:                                        /* folded back at +-T (T 22000), up to 5T: three folds */
        v = clamp(v, -110000, 110000);
        for (a = 0; a < 3; a++)
            v = v > 22000 ? 44000 - v : v < -22000 ? -44000 - v : v;
        return v;
    case DT_CRUSH: {
        uint32_t hold = 1u + (uint32_t)d / 12u, sh = 4u + (uint32_t)d / 12u;   /* (up to 11x held, 4 .. 14 bits off) */
        if (!t->hc) {
            t->hold = (int32_t)((uint32_t)(clamp(x, -131072, 131068) >> sh) << sh);
            t->hc = (uint8_t)hold;
        }
        t->hc--;
        return clamp(((t->hold >> 2) * g) >> 10, -26000, 26000);   /* (the drive's gain as the others: level kept) */
    }
    case DT_RECT:
        a = x < 0 ? -x : x;
        t->dc += (a - t->dc) >> 9;                       /* (its mean: the DC the rectifier adds) */
        return softclip((((a - t->dc) >> 2) * g) >> 10);
    case DT_AMP:
        a = softclip(clamp(v + (v >> 1), -400000, 400000) + AMP_B1) - softclip(AMP_B1);   /* stage 1: x1.5 the drive */
        t->dc += (a - t->dc) >> 6;                       /* the coupling capacitor: ~110 Hz high-pass */
        a -= t->dc;
        return softclip(((a * (24 + d)) >> 4) + AMP_B2) - softclip(AMP_B2);   /* stage 2: x1.5 .. x8 */
    default:
        return softclip(v + 2400) - softclip(2400);
    }
}
/* (0.7.9) DIST AMP's block: the chain with the cabinet's resonance after the low-pass: the cone's, f 2 sin(pi 1800 /
 * 44100) (Q15 8400), damping 0.64, its band added at 0.6 */
static JFX_XIP void dist_amp(dist_t *t, int32_t *b, uint32_t n, int32_t g, int32_t d, int32_t k, int32_t mk,
                             int32_t full, int32_t w0, int32_t dw)
{
    int32_t hp = t->hp, l1 = t->lp1, l2 = t->lp2, cl = t->cab_l, cb = t->cab_b, i;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y, h;
        hp += (x - hp + 64) >> 7;
        x = clamp(x - hp, -230000, 230000);
        y = dist_shape(t, DT_AMP, x, g, d);
        l1 += mulq15(y - l1, k);
        l2 += mulq15(l1 - l2, k);
        h = l2 - cl - mulq15(cb, 21000);
        cb = clamp(cb + mulq15(h, 8400), -90000, 90000);   /* (x 21000 fits 32 bits) */
        cl = clamp(cl + mulq15(cb, 8400), -90000, 90000);
        y = mulq15(l2 + mulq15(cb, 19660), mk);
        b[i] = full ? y : b[i] + ((((y - b[i]) >> 2) * (w0 + dw * (i + 1))) >> 10);
    }
    t->hp = hp; t->lp1 = l1; t->lp2 = l2; t->cab_l = cl; t->cab_b = cb;
}
/* one side's block (the chain as Jiant's track_dist) */
static JFX_XIP void dist_run(dist_t *t, int32_t *b, uint32_t n, uint32_t type, int32_t d, int32_t tone, int32_t w0,
                             int32_t dw, int32_t full)
{
    int32_t i, g, k, mk, b0 = softclip(2400);
    if (!w0)                                            /* in: the filters from silence, the low cut from here */
        t->lp1 = t->lp2 = 0, t->hp = 0;
    g = 4096 + d * d * 2;                                /* Q12: 1x .. ~9x, gentle at first */
    k = clamp(32000 - d * 95 + tone * 300, 2500, 32767);   /* tone: transparent at low drive .. ~3 kHz, Q15 */
    mk = type == DT_HARD || type == DT_FOLD || type == DT_CRUSH ? 30000 - d * 60 : 30000 - d * 120;   /* make-up */
    if (type == DT_AMP) {                               /* the cabinet's corner: ~4 kHz, TONE 2.5 .. 6 kHz; its make-up */
        k = clamp(14500 + tone * 90, 9000, 20000);
        mk = 24000 - d * 70;
        if (!w0)
            t->dc = t->cab_l = t->cab_b = 0;
        dist_amp(t, b, n, g, d, k, mk, full, w0, dw);
        return;
    }
    {
        int32_t hp = t->hp, l1 = t->lp1, l2 = t->lp2;
        for (i = 0; i < (int32_t)n; i++) {
            int32_t x = b[i], y;
            hp += (x - hp + 64) >> 7;                   /* ~55 Hz low cut: keep the bass out of the clipper */
            x = clamp(x - hp, -230000, 230000);         /* (x >> 2) * g fits 32 bits; the clip is flat out there */
            y = type == DT_SOFT ? softclip((((x >> 2) * g) >> 10) + 2400) - b0 : dist_shape(t, type, x, g, d);
            l1 += mulq15(y - l1, k);                    /* two poles: tames the fizz */
            l2 += mulq15(l1 - l2, k);
            y = mulq15(l2, mk);
            b[i] = full ? y : b[i] + ((((y - b[i]) >> 2) * (w0 + dw * (i + 1))) >> 10);   /* (>> 2: fits 32 bits) */
        }
        t->hp = hp;
        t->lp1 = l1;
        t->lp2 = l2;
    }
}
/* (JIANT 0.6.8, the crackles turning DIST up from 0) DST 1..8 blends the dry part into the drive (it was all drive
 * at 1: the low cut and the clip at once), the blend eased a block at a time and ramped across it; at 0 out of the
 * chain once the blend is down. b: the track's signal; b2: its second side (SPREAD), or 0. 1 = it ran */
static int jfx_dist(const track_t *t, int32_t *b, int32_t *b2, uint32_t n)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, type = (uint32_t)t->p[P_DTYPE];
    int32_t d = clamp(t->p[P_DIST], 0, 127), w0 = dist_w[ti], w1 = d >= 8 ? 4096 : d * 512, dw, full;
    if (w1 != w0)                                       /* (half the way a block, then there) */
        w1 = w1 - w0 > 64 || w0 - w1 > 64 ? w0 + (w1 - w0) / 2 : w1;
    dist_w[ti] = w1;
    if (!w0 && !w1)
        return 0;                                       /* out of the chain */
    if (type > DT_AMP)
        type = DT_SOFT;
    dw = (w1 - w0) >> CTL_LOG2;
    full = w0 >= 4096 && w1 >= 4096;
    dist_run(&dist_st[ti][0], b, n, type, d, t->p[P_DTONE], w0, dw, full);
    if (b2)
        dist_run(&dist_st[ti][1], b2, n, type, d, t->p[P_DTONE], w0, dw, full);
    return 1;
}
static int jfx_dist_on(const track_t *t) { return t->p[P_DIST] > 0 || dist_w[(uint32_t)(t - trk) % NTRK]; }

/* --------------------------------------------------------------- AUTO PAN --- */
/* (JIANT 0.6.9) AUTO PAN: the part's PAN swung by an LFO of its own, DPTH up to the whole field either way, its RATE
 * FREE (the LFO's rates) or on the tempo (BEAT: 4 bars .. 1/32, in time with the bar while playing), SIN TRI SAW SQR or
 * S&H; through the pan's glide (mix.c: no step even on SQR / S&H). Per part: the phase and the S&H value */
static uint32_t ap_ph[NTRK], ap_free[NTRK];
static int16_t ap_sh[NTRK];
static uint32_t ap_rng = 0x2545F491u;
static const uint8_t AP_SIXT[8] = {128, 64, 32, 16, 8, 4, 2, 1};   /* a cycle in 1/32 notes: 4 BARS .. 1/32 (N_APDIV) */
static JFX_XIP int32_t autopan(const track_t *t, uint32_t ti, uint32_t n)   /* the offset, pan << 8 */
{
    const int16_t *p = t->p;
    uint32_t ph, old = ap_ph[ti], w = (uint32_t)clamp(p[P_APWAVE], 0, 4);
    int32_t v;
    if (p[P_APSYNC]) {                                    /* BEAT: from the transport's grid (free while stopped) */
        uint32_t len = beat_samples() * AP_SIXT[clamp(p[P_APRATE], 0, 7)] / 8u, pos;
        if (len < 64u)
            len = 64u;
        ap_free[ti] += n;
        pos = song.playing && clk_pos != CLK_START ? ms_clock + n : ap_free[ti];   /* (the block's end, as the free
                                                          * count: 0.6.0 rev 2, a reviewer's catch) */
        ph = (pos % len) * (0xFFFFFFFFu / len);
    } else {
        ph = old + LFO_INC[p[P_APRATE] & 127] * (n / CTL);   /* (the LFO's rates: once a block) */
    }
    ap_ph[ti] = ph;
    switch (w) {
    case 1: v = ph < 0x80000000u ? (int32_t)(ph >> 15) - 32768 : 98303 - (int32_t)(ph >> 15); break;   /* TRI */
    case 2: v = (int32_t)(ph >> 16) - 32768; break;                                                   /* SAW */
    case 3: v = ph < 0x80000000u ? 32767 : -32767; break;                                             /* SQR */
    case 4:                                                                                             /* S&H */
        if (ph < old) {                                   /* (a new cycle: a new place) */
            ap_rng = ap_rng * 1664525u + 1013904223u;
            ap_sh[ti] = (int16_t)((int32_t)(ap_rng >> 16) - 32768);
        }
        v = ap_sh[ti];
        break;
    default: v = osc_sine(ph); break;                                                                  /* SIN */
    }
    return ((v >> 7) * clamp(p[P_APDEP], 0, 127)) >> 1;   /* (+-256 x 127 / 2: +-64 << 8 at full depth) */
}

/* ------------------------------------------------------------ the buses --- */
/* the sends a block (mix.c: each track's mono signal after its level, by its send), the buses' return and side, the
 * reverb's input after FILT / the pre-delay, scratch. CPU0 writes the sends and reads the rest after the job; the job
 * reads the sends and writes the rest (never both at once: audio_worker.h) */
/* (Release 2, stage 2) MELODEE_FX_DEFER 1: the buses of block b are joined only when CPU0 next needs the worker, and the master of block b mixes the
 * returns of block b - 1 (one block, 32 samples, later; BUILD.md, "Release 2"). The sends and the returns are two sets each: the tracks write
 * fx_snd[fx_sc], the bus job reads fx_bs and writes fx_bw, the master reads fx_ret[fx_rr]. 0: one set, the master reads the returns of its own block
 * (the host's default, and every golden render before Release 2); the unit's builds have 1. */
#ifndef MELODEE_FX_DEFER
#ifdef FM1_IRQ_TARGET
#define MELODEE_FX_DEFER 1
#else
#define MELODEE_FX_DEFER 0
#endif
#endif
typedef struct { int32_t c[CTL], d[CTL], r[CTL]; } fx_sends_t;
typedef struct { int32_t wet[CTL], sl[CTL], sr[CTL]; } fx_rets_t;
static fx_sends_t fx_snd[2] __attribute__((section(".pool")));
static fx_rets_t fx_ret[2] __attribute__((section(".pool")));
static uint8_t fx_sc, fx_rw = MELODEE_FX_DEFER, fx_rr, fx_fresh;          /* the sends the tracks fill; the returns the job writes / the master reads; a result not yet adopted */
static fx_sends_t *fx_bs = &fx_snd[0];                 /* the job's sets (CPU0 sets them before the submit) */
static fx_rets_t *fx_bw = &fx_ret[0];
#define send_c (fx_snd[fx_sc].c)
#define send_d (fx_snd[fx_sc].d)
#define send_r (fx_snd[fx_sc].r)
#define fx_wet (fx_ret[fx_rr].wet)                       /* (the master's: the bus code names fx_bw) */
#define fx_sl (fx_bw->sl)
#define fx_sr (fx_bw->sr)
static int32_t fx_tmp[CTL], rpre_out[CTL];
/* what the bus reads of the rest of the machine, copied on CPU0 at bus_start: the job reads the copy, the events of the next block may change the
 * originals meanwhile. All of song.g (the bus code names fxp.g), the tempo, RESO's inputs. In .pool: .bss shares the top bank with the arena's
 * first blocks */
static struct {
    int16_t g[G_COUNT];
    uint32_t beat;                                       /* beat_samples() */
    uint32_t reso_mask, reso_lo;                         /* the scale (12 bits) and the followed track's lowest held note (0xFF: none) */
    int32_t reso_root;
} fxp __attribute__((section(".pool")));
#define fxp_div(d) division_samples(fxp.beat, (d))
static void jfx_bus_drain(void);
static struct {
    uint32_t dly_w, cho_w, cho_ph;
    int32_t dly_lp, dly_hp;              /* the delay's feedback: high cut (COLR), low cut (JIANT: HPF) */
    uint32_t rpre_w, rm_ph;              /* (JIANT) the pre-delay's write count (samples); the reverb modulation's phase */
    int32_t rm_m[4], rf_lp;              /* .. the combs' offsets (Q8 samples); the reverb's input filter (FILT) */
    uint32_t gr_ph, gr_sp[2];            /* .. the GRAIN delay: grain 0's phase (Q16), each grain's spray (samples) */
    int32_t gr_s[2];                     /* .. (0.6.81) each grain's PITCH (its ratio - 1, Q16), taken as it starts */
    uint32_t dl_now, dl_old, dr_now, dr_old;   /* (0.6.81) the delay's read points (left / mid, the right echo) and the
                                         * ones faded from: TIME, WIDE, PITCH turned crossfade, never a jump */
    uint16_t dl_xf, dr_xf;               /* .. the fades left (DXF_LEN .. 0 samples) */
    uint8_t gr_on, gr_old;               /* .. the GRAIN read now, and in the read faded from */
    uint8_t primed;                      /* .. set on the first block (no fade nor glide from 0 then) */
    uint8_t dfresh;                      /* (Orange Cat 0.6.0 rev 2) the delay's line just complete (jfx_mem): its rate
                                          * and read points set as on the first block (the other buses may have run
                                          * before it had its memory) */
    uint8_t dr_sh, dsw, dsw_to;          /* .. (0.8.4) the line's rate, FS >> dr_sh (0 full, 1 half, 2 a quarter); a change of
                                         * it: 1 the delay fading out, 2 the line being cleared; the rate it goes to */
    uint16_t dsw_n, din_g;               /* .. the fade's samples left / the chunk being cleared; the input's fade-in left */
    uint32_t dw_ph, df_ph;               /* .. (0.6.86) the tape's WOW and FLUTTER phases */
    int32_t dw_m;                        /* .. the read points' offset by them now (Q8 samples) */
    int32_t dt1, dt2, rt1, rt2;          /* .. (0.6.86) the resonant TONE (delay loop) and FILT (reverb input) filters */
    int32_t cd_cur, cw_cur, dm_cur;      /* .. the chorus's DEPTH, its side's width, the delay's MIX as they are (glide) */
    int32_t rpre_s;                      /* .. the even sample, averaged with the odd one into the line */
    uint16_t comb_i[4], ap_i[2];
    int32_t comb_lp[4];
    uint8_t rtype;                       /* the reverb model running (G_RTYPE: 0 ROOM, 1 SPRING, 2 SHIMMER 3 RESO 4 CLOUD) */
    uint32_t sh_w, sh_ph;                /* (JIANT 0.5) SHIMMER: its line's write count, the grains' phase (Q16, 0.6.12) */
    int32_t rs_lp[4];                    /* .. RESO: the combs' loop low-passes */
    uint32_t rs_per[4];                  /* .. (0.6.5) their periods now (Q8), gliding to the chord's */
    uint8_t rs_root;                     /* .. the note FOLW last heard + 1 (0: none yet) */
    uint16_t rs_w;                       /* .. their write index (each its own line, 1024) */
    uint32_t cl_w, cl_rng;               /* .. CLOUD: its line's write count (half rate), its random numbers */
    int32_t cl_dc;                       /* .. (0.9.14) its loop's DC (Q8 samples) */
    uint16_t cl_fill;                    /* .. (0.9.16) sound written into its line since it was cleared (to 32768) */
    int32_t cl_next, cl_lp, cl_half, cl_gl, cl_gr;     /* .. the samples to the next grain, the output's low-pass, the odd sample */
    uint16_t sp_w;                       /* SPRING: the loop's write index (SP_MASK) */
    uint8_t sp_ns;                       /* (0.6.70) SPRING's allpass stages last block */
    int32_t sp_lp, sp_hp, sp_he, sp_size;   /* .. its loop low-pass, low cut (and its remainder), the loop length (Q8) */
    uint32_t sp_ph;                      /* .. the output tap's wobble */
    uint32_t gr_rng;                     /* (Orange Cat) the GRAIN's random numbers: its own (Jiant's beta found the shared
                                          * generator drawn from both cores) */
} fx;
#define rev_comb (jrev->comb)
#define rev_ap (jrev->u.ap)
#define rpre_buf jrev_x
#define shim_buf jrev_x

/* SPRING (G_RTYPE 1): one spring of a spring tank, mono like the other buses, in the ROOM's own buffers (no
 * RAM of its own): the input and the loop's return -> a low cut (~110 Hz: a spring carries little bass) ->
 * SP_N stretched first-order allpasses, (a + z^-4) / (1 + a z^-4) (after Valimaki, Parker and Abel: below
 * fs / 8 = 5.5 kHz the group delay rises with frequency, the chirp; each pass round the loop adds more of
 * it: the "boing", the drips) -> the loop's delay line (rev_comb, SP_LEN) -> back through a one-pole
 * low-pass (DAMP) and the decay gain (SIZE). The output: the spring's far end, half way along the loop
 * (the first sound 15 .. 30 ms after the send: the tank's own pre-delay; a slow wobble of a sample or two
 * on it), plus a second, quieter pickup at three quarters (a shorter spring beside it: denser). SIZE sets
 * the loop's length (30 .. 60 ms) and its decay; DAMP the loop's low-pass. The allpasses' states: 4
 * samples each (int32), in rev_ap's memory. Changing the model fades the old one's block out and clears both buffers. */
#define SP_LEN 4096u                     /* the loop's line in rev_comb (4937 samples) */
#define SP_MASK (SP_LEN - 1u)
#define SP_N 10u                         /* allpass stages */
#define SP_A 2867                        /* their coefficient, Q12 */
_Static_assert(REV_NCOMB >= SP_LEN && (REV_NAP + 1u) / 2u >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");
static const uint16_t REV_COMB[4] = {1116, 1188, 1277, 1356};
static const uint16_t REV_AP[2] = {556, 441};

/* (JIANT) GRAIN delay (G_DPIT / G_DSPRY not 0): the line is read by two grains of GR_LEN samples, half a grain
 * apart, each faded in and out (triangles: their sum is flat), each sweeping its read point so it plays PITCH
 * semitones up / down (as a tape pitch shifter); a grain starting again jumps back, SPRY further by a random
 * amount (up to ~190 ms). In the feedback: each repeat a further PITCH away (shimmer, falling echoes), scattered.
 * The same line, no RAM of its own */
#define GR_LEN 2048u
#define DXF_LOG2 10u
#define DXF_LEN (1u << DXF_LOG2)                         /* (0.6.81) a read point's crossfade: ~21 ms */
static const uint32_t GR_RATIO[25] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858, 65536, 69433, 73562, 77936, 82570, 87480, 92682, 98193, 104032, 110218, 116772, 123715, 131072};   /* 2^(st/12), Q16 */
static uint32_t gr_rand(void) { fx.gr_rng = fx.gr_rng * 1664525u + 1013904223u; return fx.gr_rng >> 8; }
/* (0.6.81) the grains' clock, a sample: a grain starting again takes its spray and the PITCH now */
static void dly_grain_adv(void)
{
    uint32_t j;
    fx.gr_ph = (fx.gr_ph + 65536u / GR_LEN) & 0xFFFFu;
    for (j = 0; j < 2u; j++)
        if (((fx.gr_ph + j * 32768u) & 0xFFFFu) < 65536u / GR_LEN) {
            fx.gr_sp[j] = fxp.g[G_DSPRY] ? gr_rand() % ((uint32_t)fxp.g[G_DSPRY] * 64u + 1u) : 0u;
            fx.gr_s[j] = (int32_t)GR_RATIO[clamp(fxp.g[G_DPIT], -12, 12) + 12] - 65536;
        }
}
static JFX_XIP int32_t dly_grain(uint32_t dl, uint32_t sh)   /* (0.8.4) sh: the line at FS >> sh */
{
    int32_t x = 0;
    uint32_t j, top = (DLY_LEN << sh) - (2u << sh) - GR_LEN * 2u;   /* (the farthest a grain starts, FS samples) */
    for (j = 0; j < 2u; j++) {
        int32_t s = fx.gr_s[j];
        uint32_t e = (GR_LEN * (uint32_t)(s < 0 ? -s : s)) >> 8;   /* the read point's sweep over a grain, Q8 */
        uint32_t ph = (fx.gr_ph + j * 32768u) & 0xFFFFu, d, di, gl = dl;
        int32_t a, b, w;
        if (gl + fx.gr_sp[j] > top)                     /* (JIANT 0.5) a long TIME (the whole line): */
            gl = top - fx.gr_sp[j];                     /* room for the sweep */
        d = ((gl + fx.gr_sp[j]) << 8) + ((e * ((s > 0 ? 65535u - ph : ph) >> 4)) >> 12);   /* Q8: up, the delay
                                                         * shrinks over the grain; down, it grows */
        d >>= sh;                                       /* (FS samples -> the line's) */
        di = d >> 8;
        if (di > DLY_LEN - 2u)
            di = DLY_LEN - 2u;
        a = dly_get(fx.dly_w - di);
        b = dly_get(fx.dly_w - di - 1u);
        w = (int32_t)(ph < 32768u ? ph : 65535u - ph);  /* the triangle, 0 .. 32767 */
        x += ((a + (((b - a) * (int32_t)(d & 255u)) >> 8)) * w) >> 15;
    }
    return x;
}

/* (0.6.86) the delay as an analog one (a tape, a BBD, a PT2399) feels: FEEDBK past 100 % grows into itself,
 * held by a saturation in the loop; each pass a little darker and thinner; the read points drift by WOW (0.6 Hz)
 * and FLUTTER (5.3 Hz), so each repeat bends a little further from the last. FEEDBK 0..80 as before */
static int32_t dly_fb(int32_t v)                    /* FEEDBK (0..120) -> the loop's gain, Q15: 100 = 1 */
{
    return v <= 80 ? v * 230 : v <= 100 ? 18400 + (v - 80) * 718 : 32760 + (v - 100) * 245;
}
static inline int32_t dly_sat(int32_t u)            /* Q15 in, out within +-32767 */
{   /* (0.6.89) exact up to half scale, then a quadratic knee to full scale at 1.5, flat there */
    int32_t a = u < 0 ? -u : u, d;
    if (a <= 16384)
        return u;
    d = a - 16384;
    if (d > 32768) d = 32768;
    a = 16384 + d - ((d * d) >> 16);
    if (a > 32767) a = 32767;
    return u < 0 ? -a : a;
}
/* (0.6.86) TONE / FILT resonant: one knob, the middle open; one way a low-pass sweeping down, the other a low cut
 * sweeping up, both resonant (Q 0.7 near the middle, up to qmax from a third of the way). The level evened out (x 1/Q).
 * v -64..63; lo the cutoff index (CUTOFF_HZ) at the far low-pass end, hi at the far low cut end, top the low-pass's
 * start; kmin the damping at the most resonant (Q12) */
typedef struct { tsvf_t c; int32_t k, g, hp; } tone_t;
static int tone_set(tone_t *t, int32_t v, int32_t top, int32_t lo, int32_t base, int32_t hi, int32_t kmin, int half)
{
    int32_t a = v < 0 ? -v : v;
    if (!v) return 0;
    t->k = a >= 21 ? kmin : 5793 - (5793 - kmin) * a / 21;
    t->hp = v > 0;
    tsvf_coef_k(&t->c, (v < 0 ? top - (top - lo) * a / 64 : base + (hi - base) * a / 63) << 8, t->k);
    t->g = t->k >= 4096 ? 32767 : half ? (32767 + t->k * 8) >> 1 : t->k * 8;   /* (1/Q, Q15; half: half way to it) */
    return 1;
}
static inline int32_t tone_run(const tone_t *t, int32_t in, int32_t *ic1, int32_t *ic2)
{
    int32_t v3 = in - *ic2;
    int32_t v1 = (t->c.a1 * *ic1 + t->c.a2 * v3) >> 13;
    int32_t v2 = *ic2 + ((t->c.a2 * *ic1 + t->c.a3 * v3) >> 13);
    *ic1 = clamp(2 * v1 - *ic1, -150000, 150000);
    *ic2 = clamp(2 * v2 - *ic2, -150000, 150000);
    return mulq15(t->hp ? in - v2 - ((t->k * v1) >> 12) : v2, t->g);
}
static void dly_wow_adv(uint32_t n, int32_t *m0, int32_t *dm)   /* the block's drift: from *m0, *dm a sample */
{
    int32_t m, d = fxp.g[G_DMOD];                      /* (0.6.86) MOD: 0 none, 20 a tape's, 127 seasick */
    fx.dw_ph += (uint32_t)(0.6 * 4294967296.0 / FS) * n;
    fx.df_ph += (uint32_t)(5.3 * 4294967296.0 / FS) * n;
    m = ((osc_sine(fx.dw_ph) * d) >> 12) + ((osc_sine(fx.df_ph) * d) >> 15);   /* (Q8: WOW +-d / 3.9, FLUTTER +-d / 16) */
    *m0 = fx.dw_m;
    *dm = (m - fx.dw_m) / (int32_t)n;
    fx.dw_m = *m0 + *dm * (int32_t)n;
}
/* the line (written at w) q (Q8) samples back: on a sample exact; between two (MOD's drift) a 4-point Hermite (as
 * DaisySP's DelayLine ReadHermite, MIT) */
static inline int32_t dly_rd_q(uint32_t w, int32_t q)
{
    int32_t f = q & 255, xm1, x0, x1, x2, c1, c2, c3;
    uint32_t i = w - (uint32_t)(q >> 8);
    x0 = dly_get(i);
    if (!f)
        return x0;
    xm1 = dly_get(i + 1u);                              /* (newer) */
    x1 = dly_get(i - 1u);                               /* (older: further back by f) */
    x2 = dly_get(i - 2u);
    c1 = (x1 - xm1) >> 1;
    c2 = xm1 - ((5 * x0) >> 1) + 2 * x1 - (x2 >> 1);
    c3 = ((x2 - xm1) >> 1) + ((3 * (x0 - x1)) >> 1);
    return x0 + ((((((c3 * f) >> 8) + c2) * f >> 8) + c1) * f >> 8);
}
static inline int32_t dly_rd(uint32_t d, int32_t m) { return dly_rd_q(fx.dly_w, (int32_t)(d << 8) + m); }
/* (Release 2) MELODEE_FX_DEFER: the returns reach the master a block (CTL samples) late, so the output taps read CTL samples nearer, where the dry-path
 * echo is today; the feedback taps stay where they are (the loop is inside the bus). Not for GRAIN (its own phase and spray). Else DLY_C is 0 and
 * the output read is the feedback read */
#if MELODEE_FX_DEFER
#define DLY_C CTL
#else
#define DLY_C 0u
#endif
static inline uint32_t dly_near(uint32_t d) { return d > DLY_C + 1u ? d - DLY_C : DLY_C ? 1u : d; }
static inline int32_t dly_rdc(uint32_t d, int32_t m) { return dly_rd(dly_near(d), m); }
/* (0.8.4) the farthest back a read may sit, in the line's own samples short of its length (MOD's drift, the Hermite's
 * two older taps, one to spare) */
#define DLY_EDGE 8u
#define DLY_SPAN(sh) ((DLY_LEN - DLY_EDGE) << (sh))      /* the longest read at FS >> sh, FS samples */
#define DLY_MAX DLY_SPAN(2u)                             /* .. of all (a quarter rate) */
#define DT_FREE 10                                       /* G_DTIME: the divisions, then free times (DTIME_MS) */
static const uint16_t DTIME_MS[54] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 17, 19, 21, 24, 27, 31, 35, 39, 44, 50, 56, 64,
    72, 81, 91, 103, 116, 131, 148, 167, 189, 213, 241, 272, 307, 346, 391, 441, 498, 562, 634, 716, 808, 912, 1029, 1162,
    1311, 1480, 1670, 1885, 2128, 2402, 2711, 2950};
static uint32_t delay_samples(void)
{
    uint32_t g = (uint32_t)clamp(fxp.g[G_DTIME], 0, 63);
    uint32_t s = g < DT_FREE ? fxp_div(g) : (uint32_t)DTIME_MS[(g - DT_FREE) % NELEM(DTIME_MS)] * 441u / 10u;
    return s < 16u ? 16u : s > DLY_MAX ? DLY_MAX : s;
}

/* (0.9.14) the reverbs' int16 lines written through a soft knee: linear to -2.5 dBFS, then tanh up to ~-0.1 dB */
static inline int32_t rv_knee(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a <= 24576)
        return x;
    a = 24576 + (softclip(clamp(a - 24576, 0, 16384) * 4) >> 2);
    return x < 0 ? -a : a;
}
/* (JIANT 0.5) the combs' feedback from SIZE: 0.76 .. 0.99 */
static int32_t rev_size(void)
{
    int32_t g = clamp(fxp.g[G_RSIZE], 0, 127);
    return 25000 + g * 40 + g * g / 7;
}
/* ROOM (G_RTYPE 0): 4 damped combs + 2 allpasses (Freeverb-like, mono), added to out (WIDE: rev_room_mod) */
static JFX_XIP void rev_room(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i;
    int32_t size = rev_size(), dk = clamp(fxp.g[G_RDAMP], 0, 127) * 200;   /* (32767 - damp) */
    uint32_t i0 = fx.comb_i[0], i1 = fx.comb_i[1], i2 = fx.comb_i[2], i3 = fx.comb_i[3], a0 = fx.ap_i[0], a1 = fx.ap_i[1];
    int32_t l0 = fx.comb_lp[0], l1 = fx.comb_lp[1], l2 = fx.comb_lp[2], l3 = fx.comb_lp[3];
    int16_t *c0 = rev_comb, *c1 = c0 + REV_COMB[0], *c2 = c1 + REV_COMB[1], *c3 = c2 + REV_COMB[2];
    int16_t *p0 = rev_ap, *p1 = p0 + REV_AP[0];
#define RV_COMB(C, I, L, K)                                                                                        \
    {                                                                                                              \
        int32_t o = C[I];                                                                                          \
        L = o + mulq15(L - o, dk);                                                                                 \
        C[I] = (int16_t)clamp(in + mulq15(L, size), -32768, 32767);                                                \
        if (++I >= REV_COMB[K])                                                                                    \
            I = 0;                                                                                                 \
        a += o;                                                                                                    \
    }
#define RV_AP(P, I, K)                                                                                             \
    {                                                                                                              \
        int32_t o = P[I], v = a + (o >> 1);                                                                        \
        P[I] = (int16_t)clamp(v, -32768, 32767);                                                                   \
        a = o - a;                                      /* Freeverb: out = buf - in (o - v is a notch comb) */     \
        if (++I >= REV_AP[K])                                                                                      \
            I = 0;                                                                                                 \
    }
    for (i = 0; i < n; i++) {
        int32_t a = 0, in = mulq15(rev_in[i], 2580);    /* 1/8 at -4 dB: level as before the allpass fix */
        RV_COMB(c0, i0, l0, 0)
        RV_COMB(c1, i1, l1, 1)
        RV_COMB(c2, i2, l2, 2)
        RV_COMB(c3, i3, l3, 3)
        RV_AP(p0, a0, 0)
        RV_AP(p1, a1, 1)
        out[i] += a;
    }
#undef RV_COMB
#undef RV_AP
    fx.comb_i[0] = (uint16_t)i0; fx.comb_i[1] = (uint16_t)i1; fx.comb_i[2] = (uint16_t)i2; fx.comb_i[3] = (uint16_t)i3;
    fx.ap_i[0] = (uint16_t)a0; fx.ap_i[1] = (uint16_t)a1;
    fx.comb_lp[0] = l0; fx.comb_lp[1] = l1; fx.comb_lp[2] = l2; fx.comb_lp[3] = l3;
}

/* (JIANT) ROOM with its networks modulated (G_RMOD > 0): each comb read up to ~5.7 ms nearer, by a slow sine of
 * G_RRATE a quarter apart per comb: the resonances drift and detune, the ringing smears into a chorused, wide tail.
 * WIDE wd: combs 1, 3 against 2, 4 as a side (left +, right -). With MOD and WIDE 0 the plain one runs */
static JFX_XIP void rev_room_mod(const int32_t *rev_in, int32_t *out, uint32_t n, int32_t wd)
{
    uint32_t i, k;
    int32_t size = rev_size(), damp = 32767 - clamp(fxp.g[G_RDAMP], 0, 127) * 200, m1[4], dm[4];
    int32_t rm = clamp(fxp.g[G_RMOD], 0, 127), depth = rm ? 64 + rm * rm * 4 : 0;   /* Q8 samples: 0.25 .. 252 */
    fx.rm_ph += LFO_INC[fxp.g[G_RRATE] & 127];
    for (k = 0; k < 4u; k++) {
        m1[k] = (int32_t)(((uint32_t)(osc_sine(fx.rm_ph + k * 0x40000000u) + 32768) * (uint32_t)depth) >> 16);
        if ((uint32_t)fx.rm_m[k] > 0xFFFFu)             /* (0.6.76) past any depth: a wrong word would read far outside */
            fx.rm_m[k] = m1[k];
        dm[k] = (m1[k] - fx.rm_m[k]) / (int32_t)CTL;    /* (Q8, per sample) */
    }
    for (i = 0; i < n; i++) {
        int32_t a = 0, sd = 0;
        int16_t *c = rev_comb;
        int32_t in = mulq15(rev_in[i], 2580);
        for (k = 0; k < 4u; k++) {
            int32_t m = fx.rm_m[k] += dm[k];
            uint32_t L = REV_COMB[k], j0 = fx.comb_i[k] + ((uint32_t)m >> 8), j1;
            int32_t o0, o1, o;
            if (j0 >= L)
                j0 -= L;
            j1 = j0 + 1u >= L ? 0u : j0 + 1u;
            o0 = c[j0];
            o1 = c[j1];
            o = o0 + (((o1 - o0) * (m & 255)) >> 8);    /* (a shorter delay: nearer the write) */
            fx.comb_lp[k] = o + mulq15(fx.comb_lp[k] - o, 32767 - damp);
            c[fx.comb_i[k]] = (int16_t)clamp(in + mulq15(fx.comb_lp[k], size), -32768, 32767);
            if (++fx.comb_i[k] >= L)
                fx.comb_i[k] = 0;
            a += o;
            sd += (k & 1u) ? -o : o;
            c += L;
        }
        if (wd) {
            sd = (sd * wd) >> 7;
            fx_sl[i] += sd;
            fx_sr[i] -= sd;
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = c[fx.ap_i[k]];
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)clamp(v, -32768, 32767);
            a = o - a;
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a;
    }
    for (k = 0; k < 4u; k++)
        fx.rm_m[k] = m1[k];                             /* (exact at the block's end) */
}

/* (0.6.70) SPRING's chain shortened / lengthened: the stages coming back start silent. (Orange Cat: no load governor,
 * always SP_N stages: only the first block sets it) */
static void sp_chain(int32_t *ap, uint32_t ns)
{
    uint32_t j, k;
    for (j = 0; j < 4u; j++)
        for (k = ns + 1u; k <= SP_N; k++)
            ap[j * (SP_N + 1u) + k] = 0;
    fx.sp_ns = (uint8_t)ns;
}
/* SPRING (see above), added to out */
static JFX_XIP void rev_spring(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)clamp(fxp.g[G_RSIZE], 0, 127);
    int32_t g = 19661 + (int32_t)s * 85 + (int32_t)(s * s) / 16;   /* the loop's gain: 0.6 .. 0.96 */
    int32_t kl = 26000 - clamp(fxp.g[G_RDAMP], 0, 127) * 160;     /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w, rm = clamp(fxp.g[G_RMOD], 0, 127);
    int16_t *ln = rev_comb;
    int32_t *ap = jrev->u.sp;
    uint32_t ns = SP_N;
    if (!fx.sp_size)
        fx.sp_size = len;
    fx.sp_size += clamp(len - fx.sp_size, -256, 256);   /* SIZE glides (a sample a block at most) */
    L = fx.sp_size >> 8;
    fx.sp_ph += 2u * LFO_INC[rm ? fxp.g[G_RRATE] & 127 : 24];   /* the wobble: a slow sine, 1.5 samples */
    w = (fx.sp_size >> 1) + ((osc_sine(fx.sp_ph) * (3 + rm / 2)) >> 8);   /* deep (MOD: up to 66); the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    if (ns != fx.sp_ns)
        sp_chain(ap, ns);
    for (i = 0; i < n; i++) {
        uint32_t wp = fx.sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(rev_in[i], 2580), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        fx.sp_lp += mulq15(r - fx.sp_lp, kl);
        o = fx.sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - fx.sp_hp + fx.sp_he;                    /* the low cut, its step's remainder kept */
        fx.sp_he = o & 63;
        fx.sp_hp += o >> 6;
        x -= fx.sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= ns; k++) {                     /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;
            o = ap[j + k];
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)clamp(x, -32768, 32767);
        fx.sp_w = (uint16_t)(wp + 1u);
        out[i] += (t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2;
    }
}
/* (JIANT 0.3) SPRING's WIDE, after its block: a tap a quarter along the loop against 3/4 as the side */
static JFX_XIP void rev_spring_side(uint32_t n, int32_t wd)
{
    const int16_t *ln = rev_comb;
    uint32_t i, L = (uint32_t)fx.sp_size >> 8, L3 = (L * 3u) >> 2, wp = (uint32_t)fx.sp_w - n;
    for (i = 0; i < n; i++, wp++) {
        int32_t sd = ((ln[(wp - L3) & SP_MASK] - ln[(wp - (L >> 2)) & SP_MASK]) * 3 * wd) >> 7;
        fx_sl[i] += sd;
        fx_sr[i] -= sd;
    }
}

/* (JIANT 0.5) three more models on the reverb send, no parameter of their own (the REVERB pages' knobs, read per model):
 *   SHIMMER (2)  ROOM with its output an octave up fed back into it (two crossfaded grains reading its own line twice
 *                as fast: a pitch shifter): each pass round the loop an octave higher. MOD the shimmer's amount; SIZE
 *                DAMP as ROOM; PRE: AIR
 *   RESO (3)     four combs tuned to the song's scale (ROOT and SCALE: its I III V VII from C3), a sympathetic string
 *                each: whatever is sent rings in the key. SIZE their sustain, PRE: PITCH, DAMP: FOLW, MOD the voicing
 *   CLOUD (4)    a granular cloud: the send recorded (half rate, ~0.74 s) and read by up to twelve grains; RATE their
 *                density, DAMP the tone, WIDE them left and right, MOD pitched grains, PRE: AIR. SIZE full: FREEZE */
static int32_t shim_fb[CTL];
/* (0.6.12) AIR: a semitone count as a speed, Q16: 2^(st / 12) (-24 .. 36) */
static uint32_t semi_q16(int32_t st)
{
    static const uint16_t R[12] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858};
    int32_t o = (st + 48) / 12 - 4, k = (st + 48) % 12;   /* (an octave down, Q15: then shifted) */
    uint32_t r = (uint32_t)R[k] << 1;                   /* 2^(k / 12), Q16 */
    return o >= 0 ? r << o : r >> -o;
}
static int32_t rev_air(void)
{
    return fxp.g[G_RTYPE] == 2 || fxp.g[G_RTYPE] == 4 ? clamp(fxp.g[G_RPRE], -24, 24) : 0;
}
static int32_t sh_tlp;                                  /* (0.6.12) SHIMMER's tone: AIR's low-pass / its shelf */
static JFX_XIP void rev_shimmer(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t in[CTL], t[CTL], amt = clamp(fxp.g[G_RMOD], 0, 127) * 190;   /* Q15: up to ~0.74 */
    int32_t air = rev_air(), st = clamp(12 + air, -12, 24), tk = air < 0 ? 32767 + air * 1150 : 9000;
    int32_t step = (int32_t)semi_q16(st) - 65536;       /* (the delay changes by 1 - speed a sample, Q16) */
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        in[i] = rev_in[i] + mulq15(shim_fb[i], amt);
        t[i] = 0;
    }
    rev_room(in, t, n);
    for (i = 0; i < n; i++) {
        int32_t x = 0, y = t[i];
        if (air) {                                      /* AIR: down a low-pass (dark), up a high shelf (air) */
            sh_tlp += mulq15(y - sh_tlp, tk);
            y = air < 0 ? sh_tlp : y + (((y - sh_tlp) >> 3) * air) / 3;
        }
        out[i] += y;
        shim_buf[fx.sh_w & (SH_LEN - 1u)] = (int16_t)clamp(y >> 1, -32768, 32767);
        for (j = 0; j < 2u; j++) {                      /* two grains, the delay sliding: read at octave + AIR's speed */
            uint32_t q = fx.sh_ph + j * ((SH_LEN / 2u) << 16), ph = (q >> 16) & (SH_LEN - 1u), d = SH_LEN - 1u - ph;
            int32_t w = (int32_t)(ph < SH_LEN / 2u ? ph : SH_LEN - 1u - ph);   /* the triangles: their sum flat */
            int32_t f = (int32_t)((q >> 8) & 255u), x0 = shim_buf[(fx.sh_w - d) & (SH_LEN - 1u)];
            int32_t x1 = shim_buf[(fx.sh_w - d + 1u) & (SH_LEN - 1u)];
            x += ((x0 + (((x1 - x0) * f) >> 8)) * w) >> 9;
        }
        fx.sh_ph += (uint32_t)step;
        fx.sh_w++;
        shim_fb[i] = x;                                 /* (into the next block's input) */
    }
}
#define RS_LEN 1024u                                     /* each comb's line, in rev_comb: down to ~43 Hz */
_Static_assert(REV_NCOMB >= 4u * RS_LEN, "RESO in ROOM's buffers");
/* (JIANT 0.6.5) RESO's two knobs of its own, in the slots of PRE and DAMP: PITCH (G_RPRE 0..14) the chord that many
 * steps of the scale up; FOLW (G_RDAMP 0..4, Orange Cat's four tracks) OFF / T1..T4: the chord built on the lowest
 * note that track sounds (held after it stops), so it moves with the sequence. The strings glide there */
static uint32_t reso_pitch(void) { return fxp.g[G_RTYPE] == 3 ? (uint32_t)clamp(fxp.g[G_RPRE], 0, 14) : 0u; }
static uint32_t reso_folw(void) { return fxp.g[G_RDAMP] >= 1 && fxp.g[G_RDAMP] <= (int16_t)NTRK ? (uint32_t)fxp.g[G_RDAMP] : 0u; }
/* the first melodic track (its scale, Jiant's "song scale"): Orange Cat's DRUM tracks are drum_track's */
static const track_t *jfx_scale_track(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (!drum_track(&trk[k])) return &trk[k];
    return &trk[0];
}
static JFX_XIP void rev_reso(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, per[4], mask, s, fol = reso_folw(), up = reso_pitch();
    int32_t root = fxp.reso_root;
    int32_t sz = clamp(fxp.g[G_RSIZE], 0, 127), dk = 32767 - 60 * 200, note[4], deg = 0, nn;
    int32_t fb = sz <= 100 ? 26000 + sz * 52 : clamp(31200 + (sz - 100) * 58, 0, 32740);   /* (0.5.1: the top of SIZE
                                                         * rings on for tens of seconds) */
    mask = fxp.reso_mask;                                 /* (the copy: scale_mask of the first melodic track, 0 -> all) */
    nn = 48 + root;
    if (fol) {                                           /* FOLW: the lowest note the track sounds */
        uint32_t lo = fxp.reso_lo;
        if (lo != 0xFFu)
            fx.rs_root = (uint8_t)(lo + 1u);
        if (fx.rs_root) {
            nn = 48 + (int32_t)(fx.rs_root - 1u) % 12;          /* (its pitch class from C3, down onto the scale) */
            for (k = 0; k < 12u && !((mask >> (uint32_t)((nn - root + 120) % 12)) & 1u); k++)
                nn--;
        }
    }
    for (k = 0; k < up; nn++)                            /* PITCH: that many steps of the scale up */
        if ((mask >> (uint32_t)((nn + 1 - root + 120) % 12)) & 1u)
            k++;
    for (k = 0, s = 0; k < 4u && s < 48u; s++, nn++)     /* the I, III, V, VII from there up */
        if ((mask >> (uint32_t)((nn - root + 120) % 12)) & 1u) {
            if (deg % 2 == 0)
                note[k++] = nn;
            deg++;
        }
    for (; k < 4u; k++) note[k] = note[k - 1] + 12;
    if (fxp.g[G_RMOD] > 64) { note[2] += 12; note[3] += 12; }   /* MOD: the voicing open */
    for (k = 0; k < 4u; k++) {
        uint32_t inc = pitch_inc((uint32_t)clamp(note[k] * 16, 0, 2047));
        per[k] = 0xFFFFFFFFu / ((inc >> 8) | 1u);          /* the period, Q8 */
        if (per[k] > (RS_LEN - 2u) << 8) per[k] = (RS_LEN - 2u) << 8;
        if (!fx.rs_per[k])
            fx.rs_per[k] = per[k];
        fx.rs_per[k] += (uint32_t)(((int32_t)per[k] - (int32_t)fx.rs_per[k]) / 6);   /* (a glide, ~6 blocks) */
        per[k] = fx.rs_per[k];
    }
    for (i = 0; i < n; i++) {
        int32_t in = mulq15(rev_in[i], 3600), a = 0;
        for (k = 0; k < 4u; k++) {
            int16_t *c = rev_comb + k * RS_LEN;
            uint32_t d = per[k] >> 8, f = per[k] & 255u;
            int32_t x0 = c[(fx.rs_w - d) & (RS_LEN - 1u)], x1 = c[(fx.rs_w - d - 1u) & (RS_LEN - 1u)];
            int32_t o = x0 + (((x1 - x0) * (int32_t)f) >> 8);
            fx.rs_lp[k] = o + mulq15(fx.rs_lp[k] - o, 32767 - dk);
            c[fx.rs_w & (RS_LEN - 1u)] = (int16_t)clamp(in + mulq15(fx.rs_lp[k], fb), -32768, 32767);
            a += o;
        }
        fx.rs_w++;
        out[i] += a;
    }
}
#define CL_N 12u
#define CL_WMAX 320                                      /* (0.9.14) the grains' windows summed: 1.25 grains' worth at most */
#define CL_BACK (CL_LEN - 6000u - 1024u)                 /* .. the farthest back a grain may start */
static struct { uint32_t pos, inc; uint16_t len, age; int16_t pl, pr; } cl_g[CL_N];
static uint32_t cl_rand(void) { fx.cl_rng = fx.cl_rng * 1664525u + 1013904223u; return fx.cl_rng >> 8; }
static int jfx_cloud_frozen(void) { return fxp.g[G_RTYPE] == 4 && fxp.g[G_RSIZE] >= 127; }
/* CLOUD (G_RTYPE 4, JIANT 0.5, made wet in 0.5.1): a granular wash. The input and the cloud's own tail fed back are
 * written at half rate into a 0.74 s line; up to 12 overlapping grains (RATE: their density; 45 .. 140 ms long, triangle
 * windows) read it from anywhere behind the write, MOD of them an octave up, down or a fifth up; each grain panned at
 * random (WIDE). The grains go through ROOM's network (SIZE its decay, DAMP its tone): they bloom into a tail around
 * the sound. SIZE full freezes the line (the cloud holds) */
static JFX_XIP void rev_cloud(const int32_t *rev_in, int32_t *out, uint32_t n)
{
    static const uint32_t PITCH[4] = {65536u, 131072u, 32768u, 98184u};   /* x1, an octave up, down, a fifth up (Q16) */
    uint32_t i, k, freeze = fxp.g[G_RSIZE] >= 127 && fx.cl_fill >= 32768u;   /* (0.9.16: a line's worth first) */
    int32_t gap = 1700 - clamp(fxp.g[G_RRATE], 0, 127) * 11, dk = 32767 - clamp(fxp.g[G_RDAMP], 0, 127) * 220;
    int32_t wd = clamp(fxp.g[G_RWIDE], 0, 127), air = rev_air();
    uint32_t ar = semi_q16(air);                         /* (0.6.12) AIR: the grains up / down, the tone with them */
    if (air < 0) dk = clamp(dk + air * 1100, 1500, 32767);
    int32_t fb = 0, fg = clamp(fxp.g[G_RSIZE], 0, 127) * 110, g[CTL], t[CTL];   /* (the grains, ~0.43, back into the line) */
    for (i = 0; i < n; i++) {
        int32_t l = 0, r = 0, m;
        if (!freeze) {                                   /* the line: the input and the tail, two samples into one */
            int32_t x = (rev_in[i] >> 1) + mulq15(shim_fb[i], fb) + mulq15(fx.cl_lp, fg);
            fx.cl_dc += ((x << 8) - fx.cl_dc) >> 9;     /* (0.9.14) a low cut (~14 Hz, Q8) in the loop */
            x -= (fx.cl_dc + 128) >> 8;
            if (fx.cl_w & 1u)
                cl_put(fx.cl_w >> 1, rv_knee((fx.cl_half + x) >> 1));
            else
                fx.cl_half = x;
            fx.cl_w++;
            if (fx.cl_fill < 32768u && (fx.cl_fill || x)) fx.cl_fill++;
        }
        if (--fx.cl_next <= 0) {                         /* a grain is born, in the first one free */
            fx.cl_next = gap / 2 + (int32_t)(cl_rand() % (uint32_t)gap);
            for (k = 0; k < CL_N && cl_g[k].len; k++) {}
            if (k < CL_N) {
                uint32_t pk = cl_rand() % 100u < (uint32_t)clamp(fxp.g[G_RMOD], 0, 127) * 3u / 4u ? 1u + cl_rand() % 3u : 0u;
                int32_t pan = (int32_t)(cl_rand() % 257u) - 128;   /* -128 .. 128, times WIDE */
                pan = pan * wd >> 7;
                uint32_t inc = (uint32_t)(((uint64_t)(PITCH[pk] >> 1) * ar) >> 16), back;
                cl_g[k].len = (uint16_t)(2000u + cl_rand() % 4200u);
                if (inc > 32768u && (((uint64_t)cl_g[k].len * (inc - 32768u)) >> 16) > CL_BACK)   /* (0.9.14) AIR up: */
                    cl_g[k].len = (uint16_t)(((uint64_t)CL_BACK << 16) / (inc - 32768u));   /* shorter, to fit */
                back = inc > 32768u ? (uint32_t)(((uint64_t)cl_g[k].len * (inc - 32768u)) >> 16) : 0u;
                cl_g[k].pos = ((fx.cl_w >> 1) - 200u - back - cl_rand() % (CL_LEN - 6000u - back)) << 16;
                cl_g[k].inc = inc + (cl_rand() % 129u) - 64u;   /* (each grain a few cents off) */
                cl_g[k].age = 0;
                cl_g[k].pl = (int16_t)(128 - pan);       /* (0 .. 256: 128 the centre) */
                cl_g[k].pr = (int16_t)(128 + pan);
            }
        }
        if (!(i & 1u)) {                                 /* (0.6.12) the grains every other sample, as the line is written */
            int32_t gl = 0, gr = 0, ws = 0;
            for (k = 0; k < CL_N; k++) {
                if (!cl_g[k].len) continue;
                {
                    uint32_t p = cl_g[k].pos >> 16, f = (cl_g[k].pos >> 8) & 255u, a = cl_g[k].age, L = cl_g[k].len;
                    int32_t x0 = cl_get(p), x1 = cl_get(p + 1u);
                    int32_t x = x0 + (((x1 - x0) * (int32_t)f) >> 8), w = (int32_t)((a < L - a ? a : L - a) * 2u * 256u / L);
                    x = (x * w) >> 8;
                    ws += w;
                    gl += (x * cl_g[k].pl) >> 7;
                    gr += (x * cl_g[k].pr) >> 7;
                    cl_g[k].pos += cl_g[k].inc << 1;
                    cl_g[k].age = (uint16_t)(a + 2u);
                    if (cl_g[k].age >= cl_g[k].len) cl_g[k].len = 0;
                }
            }
            if (ws > CL_WMAX) {                          /* (0.9.14) the overlap at most CL_WMAX / 256 grains' worth */
                int32_t q = (CL_WMAX << 11) / ws;
                gl = (gl * q) >> 11;
                gr = (gr * q) >> 11;
            }
            l = (gl + fx.cl_gl) >> 1;
            r = (gr + fx.cl_gr) >> 1;
            fx.cl_gl = gl;
            fx.cl_gr = gr;
        } else {
            l = fx.cl_gl;
            r = fx.cl_gr;
        }
        m = (l + r) >> 1;
        fx.cl_lp = m + mulq15(fx.cl_lp - m, 32767 - dk);
        g[i] = fx.cl_lp;                                 /* (into the network, at half the level; x2 at its output) */
        if (air > 0)                                     /* (AIR up: the grains' highs lifted) */
            g[i] += ((m - fx.cl_lp) * air) / 24;
        t[i] = 0;
        if (wd) {                                        /* the grains' own place, left and right (a side) */
            int32_t sd = ((l - r) * wd) >> 9;
            fx_sl[i] += sd;
            fx_sr[i] -= sd;
        }
    }
    if (fxp.g[G_RMOD] || wd)
        rev_room_mod(g, t, n, wd * 2);                   /* (its side x2 as well: the network at half the level) */
    else
        rev_room(g, t, n);
    for (i = 0; i < n; i++) {
        out[i] += t[i] * 3 + (g[i] >> 1);                /* the bloom (+3.5 dB, 0.5.1), the grains themselves under it */
        shim_fb[i] = t[i];                               /* (the tail into the line, next block) */
    }
}
/* the model ty's block into out */
/* (0.6.0 rev 2) the model ty can run: CLOUD needs its 32 KB line, SHIMMER its 4 KB one (jfx_mem may not have them:
 * FX FULL). Checked before every rev_run, the fade of a model being left too (a reviewer found SHIMMER faded out with
 * no line read a null pointer) */
static int rev_can_run(uint32_t ty)
{
    return !(ty == 4u && rx_have < RX_NSEG) && !(ty == 2u && !jrev_x);
}
static void rev_run(uint32_t ty, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t wd = clamp(fxp.g[G_RWIDE], 0, 127);
    switch (ty) {
    case 1: rev_spring(rev_in, out, n); if (wd) rev_spring_side(n, wd); break;
    case 2: rev_shimmer(rev_in, out, n); break;
    case 3: rev_reso(rev_in, out, n); break;
    case 4: rev_cloud(rev_in, out, n); break;
    default:
        if (fxp.g[G_RMOD] || wd) rev_room_mod(rev_in, out, n, wd);
        else rev_room(rev_in, out, n);
        break;
    }
}
/* the reverb's buffers and states to silence (the model changed), but for the models' own line */
static void rev_state_clear(void)
{
    uint32_t i;
    if (jrev)
        memset(jrev, 0, sizeof *jrev);
    for (i = 0; i < 4u; i++)
        fx.comb_lp[i] = fx.rs_lp[i] = 0;
    fx.sp_lp = fx.sp_hp = fx.sp_he = 0;
    fx.sp_ns = 0;
    for (i = 0; i < CTL; i++)                           /* (JIANT 0.5) SHIMMER's feedback, CLOUD's grains */
        shim_fb[i] = 0;
    sh_tlp = 0;
    for (i = 0; i < CL_N; i++)
        cl_g[i].len = 0;
    fx.cl_gl = fx.cl_gr = 0;
    fx.cl_dc = 0;
    fx.cl_fill = 0;
    fx.cl_lp = fx.cl_half = 0;
}
/* the first n samples of the model line silenced (as far as it is held) */
static void rev_line_clear(uint32_t n)
{
    for (uint32_t k = 0; k < rx_have && k * JFX_SEG < n; k++)
        memset(rx_seg[k], 0, (n - k * JFX_SEG < JFX_SEG ? n - k * JFX_SEG : JFX_SEG) * sizeof(int16_t));
}
/* (0.8.4) how much of the model line a change from the model `from` to `to` silences */
static uint32_t rev_line_dirty(uint32_t from, uint32_t to)
{
    return to == 4u ? CL_LEN : to == 2u ? SH_LEN : from == 2u || from == 4u ? RPRE_LEN : 0u;
}

/* (0.5.1) the chorus ensemble: three taps of buf (written at w), the LFO phase ph a third apart each, base delays
 * 300 / 420 / 540 samples plus the sweep (depth Q8); returns the middle (Q15 x2 at unity), *sd the left - right */
static inline int32_t cho_tap(const int16_t *buf, uint32_t w, int32_t r)
{
    uint32_t ri = (uint32_t)r >> 8;
    int32_t f = r & 255, c0 = buf[(w - ri) & (CHO_LEN - 1u)], c1 = buf[(w - ri - 1u) & (CHO_LEN - 1u)];
    return c0 + (((c1 - c0) * f) >> 8);
}
#if MELODEE_FX_DEFER
#define CHO_C CTL                                       /* (Release 2) the returns come a block late: the taps read a block nearer (the sweep only adds) */
#else
#define CHO_C 0
#endif
static inline int32_t cho_ens(const int16_t *buf, uint32_t w, uint32_t ph, int32_t depth, int32_t *sd)
{
    int32_t a = cho_tap(buf, w, ((300 - CHO_C) << 8) + ((osc_sine(ph) + 32768) * depth >> 8));
    int32_t b = cho_tap(buf, w, ((420 - CHO_C) << 8) + ((osc_sine(ph + 0x55555555u) + 32768) * depth >> 8));
    int32_t c = cho_tap(buf, w, ((540 - CHO_C) << 8) + ((osc_sine(ph + 0xAAAAAAAAu) + 32768) * depth >> 8));
    *sd = a - b;
    return ((a + b + c) * 21845) >> 15;                 /* (x2 / 3 each: the sum at x2, as the one tap was) */
}

/* (0.6.14) the buses' gate: their feedback rounds to a small signal that never reaches 0 (a limit cycle). Nothing sent
 * to them for 1.5 s and their output that small: faded to 0 in ~25 ms; anything sent opens it at once. CLOUD's FREEZE
 * (SIZE 127) is meant to hold: never gated */
#define FXG_QUIET ((uint32_t)FS * 3u / 2u / CTL)
#define FXG_FLOOR 64
#define FXG_QUIET2 ((uint32_t)FS * 4u / CTL)
#define FXG_FLOOR2 512
#define FXG_QUIET3 ((uint32_t)FS * 8u / CTL)
#define FXG_FLOOR3 1024
static uint32_t fxg_quiet;
static int32_t fxg_dpk;                              /* (0.6.89) the delay line's peak this block */
static int32_t fxg_g = 32767;
static uint32_t fxg_hold, fxg_dact;
static uint8_t fxg_fb, fxg_dly, fx_rest_ph;
static void fx_gate(int32_t *wet, uint32_t n)
{
    int32_t pk = 0, g0 = fxg_g, d;
    uint32_t i, q;
    for (i = 0; i < n; i++) {
        int32_t a = wet[i] < 0 ? -wet[i] : wet[i];
        if (a > pk) pk = a;
    }
    int dly = fxg_dpk >= FXG_FLOOR && !fx.gr_on;       /* (0.6.89) a plain delay's tail still there */
    fxg_dly = (uint8_t)dly;
    fxg_dact = !fxg_quiet || (dly && fxg_fb) ? 0u : fxg_dact < 0xFFFFFFu ? fxg_dact + 1u : fxg_dact;
    q = fxg_hold && fxg_dact <= fxg_hold ? 0u : fxg_quiet;
    fxg_dpk = 0;
    if (((q > FXG_QUIET && pk < FXG_FLOOR) || (!dly && q > FXG_QUIET2 && pk < FXG_FLOOR2) ||
         (!dly && q > FXG_QUIET3 && pk < FXG_FLOOR3)) && !jfx_cloud_frozen())
        fxg_g = g0 > 1024 ? g0 - 1024 : 0;
    else if (g0 < 32767 && (g0 > 0 || fxg_quiet == 0u || dly))   /* (0.6.89: or the delay growing by itself) */
        fxg_g = g0 < 32767 - (fxg_quiet ? 1024 : 4096) ? g0 + (fxg_quiet ? 1024 : 4096) : 32767;
    else if (g0 == 0 && !jfx_cloud_frozen())           /* .. closed: it stays closed until something is sent */
        fxg_g = 0;
    else
        fxg_g = 32767;
    if (g0 == 32767 && fxg_g == 32767)
        return;
    d = (fxg_g - g0) / (int32_t)n;
    for (i = 0; i < n; i++, g0 += d)
        wet[i] = mulq15(wet[i], g0);
}
/* (0.8.4) the long delay, lighter: past what the line holds at the full rate, it runs at half the rate (up to twice
 * as long), past that at a quarter (four times). The input goes down through a half-band decimator (one stage a
 * halving), the whole loop (the reads, MOD, the GRAIN, the feedback, TONE) runs at the line's rate, and its output comes
 * back up through the same half-band as an interpolator: once each way, not every repeat. The half-band: 55 taps, 14
 * distinct (Parks-McClellan, pass band to 9.2 kHz): flat within 0.1 dB to 9.5 kHz at half rate, every alias under
 * -70 dB; at a quarter rate the same again, half as high: the long ones darker, as a tape's or a BBD's. A change of the
 * rate fades the delay out over DSW_FADE samples, clears the line (a 32 KB chunk a block) and starts it again with the
 * input faded in: a gap, never a click */
#define HB_N 14u
#define HB_H (2u * HB_N)
static const int16_t HB_Q[HB_N] = {5194, -1675, 940, -607, 412, -283, 193, -129, 83, -51, 30, -16, 8, -3};   /* (sum 4096) */
typedef struct { int32_t e[2u * HB_H], o[2u * HB_N]; uint8_t ep, op, ph; } hbd_t;   /* (each line twice: no wrap) */
typedef struct { int32_t u[2u * HB_H]; uint8_t p; } hbi_t;
static struct {
    hbd_t d[2];                                          /* the decimator's stages: FS -> FS/2, FS/2 -> FS/4 */
    hbi_t ix[2], ir[2];                                  /* the interpolator's (the left / mid echo, the right): [1] first */
    int32_t ox[4], orr[4];                               /* the last step's output, R samples at FS */
    uint8_t k;                                           /* .. the next of them */
    uint8_t st;                                          /* .. the right echo apart (else ir idle: the left's copied) */
} dlo;
static int32_t dlo_x[CTL], dlo_xr[CTL];
#define DSW_LOG2 7u
#define DSW_FADE (1u << DSW_LOG2)                        /* a rate change's fade out (and the input's in): 2.9 ms */
#define DLY_CLR 16384u                                   /* .. the line cleared 32 KB a block */
static inline int32_t hb_sum(const int32_t *h)           /* h[0] the newest: the pairs about the middle (lag 13.5) */
{
    int32_t a = 0;
    uint32_t k;
    for (k = 0; k < HB_N; k++)
        a += HB_Q[k] * (h[HB_N - 1u - k] + h[HB_N + k]);
    return a;
}
static inline int hb_dec(hbd_t *s, int32_t x, int32_t *y)   /* an FS sample in; every other one a sample out */
{
    if (s->ph) {                                         /* (odd: the middle tap's line) */
        s->op = (uint8_t)(s->op ? s->op - 1u : HB_N - 1u);
        s->o[s->op] = s->o[s->op + HB_N] = x;
        s->ph = 0;
        return 0;
    }
    s->ep = (uint8_t)(s->ep ? s->ep - 1u : HB_H - 1u);
    s->e[s->ep] = s->e[s->ep + HB_H] = x;
    s->ph = 1;
    *y = (hb_sum(s->e + s->ep) + s->o[s->op + HB_N - 1u] * 8192 + 8192) >> 14;
    return 1;
}
static inline void hb_int(hbi_t *s, int32_t u, int32_t *y0, int32_t *y1)   /* a sample in, two out (x2 the rate) */
{
    s->p = (uint8_t)(s->p ? s->p - 1u : HB_H - 1u);
    s->u[s->p] = s->u[s->p + HB_H] = u;
    *y0 = (hb_sum(s->u + s->p) + 4096) >> 13;
    *y1 = s->u[s->p + HB_N - 1u];
}
static inline int32_t dly_rd_sh(uint32_t d, int32_t m, uint32_t sh)   /* d (+ m, Q8) FS samples back, the line at FS >> sh */
{
    return dly_rd_q(fx.dly_w, ((int32_t)(d << 8) + m) >> sh);
}
static inline int32_t dly_rdc_sh(uint32_t d, int32_t m, uint32_t sh) { return dly_rd_sh(dly_near(d), m, sh); }
/* the rate a read need (FS samples) back takes: the lowest shift it fits; toward a higher rate only once it is 1/32 inside
 * that one's reach (a tempo from an external clock wavering at a boundary: not a restart each beat) */
static uint32_t dly_stage(uint32_t need, uint32_t cur)
{
    uint32_t s = need <= DLY_SPAN(0u) ? 0u : need <= DLY_SPAN(1u) ? 1u : 2u;
    if (s == 0u && cur > 0u && need > DLY_SPAN(0u) - (DLY_SPAN(0u) >> 5))
        s = 1u;
    if (s == 1u && cur > 1u && need > DLY_SPAN(1u) - (DLY_SPAN(1u) >> 5))
        s = 2u;
    return s;
}
static void dly_lo_reset(void)
{
    memset(&dlo, 0, sizeof dlo);
    fx.dly_hp = fx.dly_lp = fx.dt1 = fx.dt2 = 0;
    fx.dl_xf = fx.dr_xf = 0;
}
/* the line's samples [from, from + n) silenced (whole halves at a time or less) */
static void dly_line_clear(uint32_t from, uint32_t n)
{
    while (n) {
        uint32_t s = (from >> JFX_SEG_SH) & (DLY_NSEG - 1u), o = from & (JFX_SEG - 1u);
        uint32_t k = JFX_SEG - o < n ? JFX_SEG - o : n;
        if (s < dly_have) memset(dly_seg[s] + o, 0, k * sizeof(int16_t));
        from += k;
        n -= k;
    }
}
typedef struct { int32_t fb, fb2, hk, dig, dton, png, wd; tone_t tn; } dlyk_t;   /* the block's settings */
/* the delay's block, when not plain at the full rate: into dx (the left / mid echo) and dxr (the right), FS */
static JFX_XIP void dly_lo(const int32_t *dly_in, int32_t *dx, int32_t *dxr, uint32_t n, const dlyk_t *k, int32_t wm,
                           int32_t dwm)
{
    uint32_t i, j, sh = fx.dr_sh, r = 1u << sh;
    if (fx.dsw == 2u) {                                  /* (the line being cleared: silence) */
        for (i = 0; i < n; i++)
            dx[i] = dxr[i] = 0;
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t u = clamp(dly_in[i] >> 1, -65535, 65535), x, xr, ox, oxr;   /* (ox, oxr: the output taps, Release 2) */
        int ok = 1;
        if (fx.dsw == 2u) {                              /* (faded out within this block) */
            dx[i] = dxr[i] = 0;
            continue;
        }
        wm += dwm;
        int32_t wmo = wm + (int32_t)DLY_C * dwm;
        if (fx.din_g) {                                  /* (a new line: the input faded in) */
            u = (u * (int32_t)(DSW_FADE - fx.din_g)) >> DSW_LOG2;
            fx.din_g--;
        }
        if (sh) {
            ok = hb_dec(&dlo.d[0], u, &u);
            if (ok && sh > 1u)
                ok = hb_dec(&dlo.d[1], u, &u);
        }
        if (ok) {                                        /* a step of the loop, at the line's rate */
            int32_t lf, w;
            if (fx.gr_on || (fx.dl_xf && fx.gr_old))
                for (j = 0; j < r; j++)
                    dly_grain_adv();
            x = fx.gr_on ? dly_grain(fx.dl_now, sh) : dly_rd_sh(fx.dl_now, wm, sh);
            ox = fx.gr_on ? x : dly_rdc_sh(fx.dl_now, wmo, sh);
            if (fx.dl_xf) {
                int32_t xo = fx.gr_old ? dly_grain(fx.dl_old, sh) : dly_rd_sh(fx.dl_old, wm, sh);
                int32_t oo = fx.gr_old ? xo : dly_rdc_sh(fx.dl_old, wmo, sh);
                x += ((xo - x) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                ox += ((oo - ox) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                fx.dl_xf = (uint16_t)(fx.dl_xf - r);
            }
            xr = x;
            oxr = ox;
            if (k->png) {
                xr = dly_rd_sh(fx.dl_now * 2u, wm, sh);
                oxr = dly_rdc_sh(fx.dl_now * 2u, wmo, sh);
                if (fx.dl_xf) {
                    int32_t xo = dly_rd_sh(fx.dl_old * 2u, wm, sh), oo = dly_rdc_sh(fx.dl_old * 2u, wmo, sh);
                    xr += ((xo - xr) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                    oxr += ((oo - oxr) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                }
            } else if (k->wd || fx.dr_xf || fx.dr_now != fx.dl_now) {
                xr = dly_rd_sh(fx.dr_now, wm, sh);
                oxr = fx.gr_on ? xr : dly_rdc_sh(fx.dr_now, wmo, sh);
                if (fx.dr_xf) {
                    int32_t xo = dly_rd_sh(fx.dr_old, wm, sh), oo = fx.gr_on ? xo : dly_rdc_sh(fx.dr_old, wmo, sh);
                    xr += ((xo - xr) * (int32_t)fx.dr_xf) >> DXF_LOG2;
                    oxr += ((oo - oxr) * (int32_t)fx.dr_xf) >> DXF_LOG2;
                    fx.dr_xf = (uint16_t)(fx.dr_xf - r);
                }
            }
            fx.dly_lp = x;
            { int32_t ax = x < 0 ? -x : x; if (ax > fxg_dpk) fxg_dpk = ax; }
            if (k->png) {
                int32_t ax = xr < 0 ? -xr : xr; if (ax > fxg_dpk) fxg_dpk = ax;
            }
            lf = k->png ? xr : x;
            fx.dly_hp += (((lf * 256) - fx.dly_hp) * k->hk + 16384) >> 15;
            lf -= (fx.dly_hp + 128) >> 8;
            if (k->dton) lf = tone_run(&k->tn, lf, &fx.dt1, &fx.dt2);
            w = u + mulq15(clamp(lf, -32767, 32767), k->png ? k->fb2 : k->fb);
            dly_put(fx.dly_w, k->dig ? clamp(w, -32767, 32767) : dly_sat(w));
            fx.dly_w++;
            if (k->png)
                oxr = mulq15(clamp(oxr, -65535, 65535), k->fb > 32767 ? 32767 : k->fb);
            x = ox;                                      /* (from here the output's taps) */
            xr = oxr;
            if (!sh) {
                dlo.ox[0] = x;
                dlo.orr[0] = xr;
            } else {                                     /* back up to FS */
                x = clamp(x, -65535, 65535);
                xr = clamp(xr, -65535, 65535);
                int st = xr != x || k->png || k->wd || fx.dr_xf;   /* (the right its own: interpolated too) */
                if (st && !dlo.st)                       /* (the right apart again: from the left's state) */
                    memcpy(dlo.ir, dlo.ix, sizeof dlo.ir);
                dlo.st = (uint8_t)st;
                if (sh == 1u) {
                    hb_int(&dlo.ix[0], x, &dlo.ox[0], &dlo.ox[1]);
                    if (st) hb_int(&dlo.ir[0], xr, &dlo.orr[0], &dlo.orr[1]);
                } else {
                    int32_t a, b;
                    hb_int(&dlo.ix[1], x, &a, &b);
                    hb_int(&dlo.ix[0], a, &dlo.ox[0], &dlo.ox[1]);
                    hb_int(&dlo.ix[0], b, &dlo.ox[2], &dlo.ox[3]);
                    if (st) {
                        hb_int(&dlo.ir[1], xr, &a, &b);
                        hb_int(&dlo.ir[0], a, &dlo.orr[0], &dlo.orr[1]);
                        hb_int(&dlo.ir[0], b, &dlo.orr[2], &dlo.orr[3]);
                    }
                }
                if (!st)
                    for (j = 0; j < r; j++)
                        dlo.orr[j] = dlo.ox[j];
            }
            dlo.k = 0;
        }
        j = dlo.k < r ? dlo.k : r - 1u;
        dlo.k++;
        x = dlo.ox[j];
        xr = dlo.orr[j];
        if (fx.dsw == 1u) {                              /* (a rate change: the old line faded out) */
            x = (x * (int32_t)fx.dsw_n) >> DSW_LOG2;
            xr = (xr * (int32_t)fx.dsw_n) >> DSW_LOG2;
            if (!--fx.dsw_n)
                fx.dsw = 2;                              /* .. done: silence, the line cleared from the next block */
        }
        dx[i] = x;
        dxr[i] = xr;
    }
}
/* (0.8.4) once a block: the line's rate for the farthest read (PING-PONG: twice TIME; WIDE: the right echo's); a change
 * of it fades the delay out, clears the line a chunk a block, then starts it again at the new rate. Returns whether the
 * block is dly_lo's (not plain at the full rate) */
static int dly_rate(uint32_t *dl, uint32_t *dr, uint32_t dro, int png, int gr)
{
    uint32_t need = png ? *dl * 2u : *dl + dro, s, span;
    int fresh = 0;
    s = dly_stage(need, fx.primed ? fx.dr_sh : 0u);
    if (!fx.primed || fx.dfresh) {
        fx.dr_sh = (uint8_t)s;
        fx.dsw = 0; fx.din_g = 0;
        memset(&dlo, 0, sizeof dlo);
    } else if (fx.dsw == 2u) {                          /* clearing: a chunk this block, the last one: the new rate */
        dly_line_clear(fx.dsw_n * DLY_CLR, DLY_CLR);
        if (++fx.dsw_n >= DLY_LEN / DLY_CLR) {
            fx.dr_sh = fx.dsw_to;
            fx.dsw = 0; fx.din_g = DSW_FADE;
            dly_lo_reset();
            fresh = 1;
        }
    } else if (!fx.dsw && s != fx.dr_sh) {
        fx.dsw = 1;
        fx.dsw_n = DSW_FADE;
        fx.dsw_to = (uint8_t)s;
    }
    span = DLY_SPAN(fx.dr_sh);
    if (png && *dl > span / 2u) *dl = span / 2u;
    if (*dl > span) *dl = span;
    *dr = *dl + dro;
    if (*dr > span) *dr = span;
    if (fresh) {                                        /* (the new line: the read points where they are, no fade) */
        fx.dl_now = *dl; fx.dr_now = *dr; fx.gr_on = (uint8_t)gr;
    }
    fxg_hold = *dl > FXG_QUIET * CTL ? *dl / CTL + 2u : 0u;
    return fx.dr_sh || fx.dsw || fx.din_g;
}
/* (0.6.81) a value's step a sample from cur toward to: a quarter of the way over the block, one at least (never past) */
static int32_t fx_glide(int32_t cur, int32_t to, uint32_t n)
{
    int32_t d = to - cur, s = (d >> 2) / (int32_t)n;
    if (!s && d)
        s = d > 0 ? 1 : -1;
    if (s * (int32_t)n > d && d > 0) s = d / (int32_t)n;
    if (s * (int32_t)n < d && d < 0) s = d / (int32_t)n;
    return s;
}
/* (0.9.14) the buses at rest: their gate shut, nothing sent, the delay quiet and its loop under 100 %, its line not
 * being cleared, CLOUD not frozen: not run. Their lines and states silenced over the first blocks of it, so the next
 * send starts from silence. (Orange Cat: the memory given back after that: jfx_mem) */
static int fx_rest(int32_t any, int32_t *wet, uint32_t n)
{
    uint32_t ph = fx_rest_ph;
    if (any || fxg_g || fxg_dly || fxp.g[G_DFDBK] >= 100 || fx.dsw || jfx_cloud_frozen() ||
        (jrev && (uint32_t)clamp(fxp.g[G_RTYPE], 0, 4) != fx.rtype)) {   /* (0.6.0 rev 2: a model change waits to
                                                         * fade only while the reverb is held: with none, nothing to
                                                         * fade, and the buses could never rest) */
        fx_rest_ph = 0;
        return 0;
    }
    memset(wet, 0, n * sizeof *wet);
    if (ph == 0u) {
        rev_state_clear();
        if (cho_buf) memset(cho_buf, 0, CHO_LEN * sizeof *cho_buf);
        memset(&dlo, 0, sizeof dlo);
        fx.dt1 = fx.dt2 = fx.rt1 = fx.rt2 = 0;
        fx.dly_lp = fx.dly_hp = fx.rf_lp = fx.rpre_s = fx.cl_lp = fx.cl_half = 0;
    } else if (ph <= 4u) {
        dly_line_clear((ph - 1u) * (DLY_LEN / 4u), DLY_LEN / 4u);
    } else if (ph == 5u) {
        rev_line_clear(CL_LEN);
    }
    if (ph < 255u)
        fx_rest_ph = (uint8_t)(ph + 1u);
    return 1;
}
/* the three buses, a block: the sends in, wet (the mono return) and the side (fx_sl / fx_sr) out. A bus with no memory
 * this block (jfx_mem) does nothing */
static JFX_XIP void fx_buses(const int32_t *cho_in, const int32_t *dly_in, const int32_t *rev_in, int32_t *wet, uint32_t n)
{
    uint32_t i, dl = delay_samples();
    int32_t dcd, dcw, ddm;
    int have_d = dly_have == DLY_NSEG, have_r = jrev != 0, have_c = cho_buf != 0;
    for (i = 0; i < n; i++)
        fx_sl[i] = fx_sr[i] = 0;
    {   /* (0.6.14) anything sent to the buses this block: their gate open */
        int32_t any = 0;
        for (i = 0; i < n; i++)
            any |= cho_in[i] | dly_in[i] | rev_in[i];
        fxg_quiet = any ? 0u : fxg_quiet < 0xFFFFFFu ? fxg_quiet + 1u : fxg_quiet;
        if (fx_rest(any, wet, n))                       /* (0.9.14) at rest: not run */
            return;
    }
    int32_t fb = dly_fb(clamp(fxp.g[G_DFDBK], 0, 120)), tn = clamp(fxp.g[G_DCOLOR], 0, 127) - 64;
    int32_t wm, dwm, dig = fxp.g[G_DTYPE] == 1;
    tone_t dtn, rtn;
    int dton;
    int32_t dmix = clamp(fxp.g[G_DMIX], 0, 127) * 258;
    int32_t cdepth = clamp(fxp.g[G_CDEPTH], 0, 127) * 6, rt;
    uint32_t cinc = LFO_INC[fxp.g[G_CRATE] & 127] / CTL;
    int32_t hk = 5, wd = clamp(fxp.g[G_WIDTH], 0, 127);   /* (the loop's DC block: ~1 Hz, 0.6.89); WIDTH */
    int32_t gr = fxp.g[G_DPIT] || fxp.g[G_DSPRY];     /* (JIANT) the GRAIN delay */
    /* (0.6.90) PING-PONG (not with the GRAIN): the line read at TIME (left) and twice TIME (right, the loop's tap), the
     * loop's gain squared, the right x FEEDBK */
    int32_t png = fxp.g[G_DPING] == 1 && !gr, fb2 = (int32_t)(((int64_t)fb * fb) >> 15);
    if (dig && fb > 32767) fb = 32767;                  /* (0.6.90) DIGITAL: 100 % at most, a repeat held for ever */
    if (dig && fb2 > 32767) fb2 = 32767;
    int32_t cs = clamp(fxp.g[G_CSTER], -64, 63), cw = 80 + wd / 2;   /* (0.6.9) the chorus's side */
    cw = cs >= 0 ? cw + cs * 2 : (cw * (64 + cs)) >> 6;
    uint32_t dro = wd && !png ? fxp_div(3) * (uint32_t)wd / 127u : 0u, dr;   /* the right echo: up to 1/32 later */
    int dly_lo_on = have_d ? dly_rate(&dl, &dr, dro, png, gr) : 0;
    fxg_fb = fb > 0;
    if (!have_d) dr = dl;
    {   /* (0.8.4) TONE's filter at the line's rate: the same Hz an octave (14 of the table) up the table each halving */
        int32_t to = 14 * (int32_t)fx.dr_sh;
        dton = dig ? tone_set(&dtn, tn > 63 ? 63 : tn, 113 + to, 43 + to, 18 + to, 93 + to, 5793, 0)
                   : tone_set(&dtn, tn > 63 ? 63 : tn, 113 + to, 43 + to, 18 + to, 93 + to, 2900, 1);
    }
    if (!dton) fx.dt1 = fx.dt2 = 0;
    if (!fx.primed) {
        fx.primed = 1;
        fx.dl_now = dl; fx.dr_now = dr; fx.gr_on = (uint8_t)gr;
        fx.cd_cur = cdepth; fx.cw_cur = cw; fx.dm_cur = dmix;
    }
    if (fx.dfresh && have_d) {                          /* (0.6.0 rev 2) the delay's first block with its line */
        fx.dfresh = 0;
        fx.dl_now = dl; fx.dr_now = dr; fx.gr_on = (uint8_t)gr;
        fx.dl_xf = fx.dr_xf = 0;
    }
    if (!fx.dl_xf && (dl != fx.dl_now || (uint32_t)gr != fx.gr_on)) {
        fx.dl_old = fx.dl_now; fx.gr_old = fx.gr_on;
        fx.dl_now = dl; fx.gr_on = (uint8_t)gr;
        fx.dl_xf = DXF_LEN;
    }
    if (!fx.dr_xf && dr != fx.dr_now) {
        fx.dr_old = fx.dr_now;
        fx.dr_now = dr;
        fx.dr_xf = DXF_LEN;
    }
    dcd = fx_glide(fx.cd_cur, cdepth, n);
    dcw = fx_glide(fx.cw_cur, cw, n);
    ddm = fx_glide(fx.dm_cur, dmix, n);
    dly_wow_adv(n, &wm, &dwm);
    if (tone_set(&rtn, clamp(fxp.g[G_RFILT], -64, 63), 121, 50, 14, 79, 1600, 1)) {   /* (JIANT) the reverb's tone */
        for (i = 0; i < n; i++)
            rpre_out[i] = tone_run(&rtn, rev_in[i], &fx.rt1, &fx.rt2);
        rev_in = rpre_out;
    } else
        fx.rt1 = fx.rt2 = 0;
    if (have_r && jrev_x && fxp.g[G_RPRE] > 0 && fxp.g[G_RTYPE] <= 1 && fx.rtype <= 1u) {   /* (JIANT) the pre-delay */
        uint32_t pd = (uint32_t)clamp(fxp.g[G_RPRE], 0, 100) * 20u;   /* (ms -> half-rate samples: 2000 at 100) */
        for (i = 0; i < n; i++) {                       /* half rate: two samples averaged in, read back between */
            uint32_t h = fx.rpre_w >> 1;
            int32_t a = rpre_buf[(h - pd) & (RPRE_LEN - 1u)], b = rpre_buf[(h - pd + 1u) & (RPRE_LEN - 1u)];
            if (fx.rpre_w & 1u)
                rpre_buf[h & (RPRE_LEN - 1u)] = (int16_t)clamp((fx.rpre_s + (rev_in[i] >> 3)) >> 1, -32768, 32767);
            else
                fx.rpre_s = rev_in[i] >> 3;
            rpre_out[i] = ((fx.rpre_w & 1u) ? (a + b) >> 1 : a) * 8;
            fx.rpre_w++;
        }
        rev_in = rpre_out;
    }
    if (dly_lo_on) {                                    /* (0.8.4) the delay at half / a quarter rate, or changing */
        dlyk_t k;
        k.fb = fb; k.fb2 = fb2; k.hk = hk << fx.dr_sh; k.dig = dig; k.dton = dton; k.png = png; k.wd = wd;
        k.tn = dtn;
        dly_lo(dly_in, dlo_x, dlo_xr, n, &k, wm, dwm);
    }
    for (i = 0; i < n; i++) {
        int32_t y = 0, x = 0, side = 0, xr = 0, wr = 0, ox = 0, oxr = 0;   /* (ox, oxr: the output taps, x, xr the feedback's) */
        /* chorus (0.5.1: an ensemble): three taps of the line, ~7, 9.5 and 12 ms, each swept by the LFO a third of a
         * turn apart; the first left, the second right, the third in the middle: always stereo (WIDTH wider) */
        fx.cho_ph += cinc;
        fx.cd_cur += dcd; fx.cw_cur += dcw; fx.dm_cur += ddm;
        if (have_c) {
            int32_t sd;
            cho_buf[fx.cho_w & (CHO_LEN - 1u)] = (int16_t)clamp(cho_in[i] >> 1, -32768, 32767);
            y += cho_ens(cho_buf, fx.cho_w, fx.cho_ph + CHO_C * cinc, fx.cd_cur, &sd);   /* (the LFO at the sample that is heard: CHO_C on) */
            sd = (sd * fx.cw_cur) >> 7;
            side += sd;
            fx.cho_w++;
        }
        if (!have_d) {
        } else if (dly_lo_on) {                         /* (0.8.4: dly_lo's, the right echo always there) */
            ox = dlo_x[i];
            oxr = dlo_xr[i];
            wr = 1;
        } else {
            /* delay with a low-passed (COLR) and low-cut (HPF) feedback */
            if (fx.gr_on || (fx.dl_xf && fx.gr_old))
                dly_grain_adv();
            wm += dwm;
            int32_t wmo = wm + (int32_t)DLY_C * dwm;         /* (the wow at the sample that is heard: extrapolated) */
            x = fx.gr_on ? dly_grain(fx.dl_now, 0u) : dly_rd(fx.dl_now, wm);
            ox = fx.gr_on ? x : dly_rdc(fx.dl_now, wmo);
            if (fx.dl_xf) {                                 /* (0.6.81) the old read point fading out */
                int32_t xo = fx.gr_old ? dly_grain(fx.dl_old, 0u) : dly_rd(fx.dl_old, wm);
                int32_t oo = fx.gr_old ? xo : dly_rdc(fx.dl_old, wmo);
                x += ((xo - x) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                ox += ((oo - ox) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                fx.dl_xf--;
            }
            if (png) {                                      /* (0.6.90) PING-PONG: the right, twice as far back */
                xr = dly_rd(fx.dl_now * 2u, wm);
                oxr = dly_rdc(fx.dl_now * 2u, wmo);
                if (fx.dl_xf) {
                    int32_t xo = dly_rd(fx.dl_old * 2u, wm), oo = dly_rdc(fx.dl_old * 2u, wmo);
                    xr += ((xo - xr) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                    oxr += ((oo - oxr) * (int32_t)fx.dl_xf) >> DXF_LOG2;
                }
                wr = 2;
            } else if (wd || fx.dr_xf || fx.dr_now != fx.dl_now) {   /* (JIANT) the right echo later */
                xr = dly_rd(fx.dr_now, wm);
                oxr = fx.gr_on ? xr : dly_rdc(fx.dr_now, wmo);
                if (fx.dr_xf) {
                    int32_t xo = dly_rd(fx.dr_old, wm), oo = fx.gr_on ? xo : dly_rdc(fx.dr_old, wmo);
                    xr += ((xo - xr) * (int32_t)fx.dr_xf) >> DXF_LOG2;
                    oxr += ((oo - oxr) * (int32_t)fx.dr_xf) >> DXF_LOG2;
                    fx.dr_xf--;
                }
                wr = 1;
            }
            fx.dly_lp = x;
            { int32_t ax = x < 0 ? -x : x; if (ax > fxg_dpk) fxg_dpk = ax; }   /* (the delay's own level: fx_gate) */
            if (wr == 2) {                                  /* (PING-PONG: the loop from the right tap; its level, too) */
                int32_t ax = xr < 0 ? -xr : xr; if (ax > fxg_dpk) fxg_dpk = ax;
                fx.dly_hp += (((xr << 8) - fx.dly_hp) * hk + 16384) >> 15;
            } else
                fx.dly_hp += (((x << 8) - fx.dly_hp) * hk + 16384) >> 15;
            {
                int32_t lf = (wr == 2 ? xr : x) - ((fx.dly_hp + 128) >> 8), w;
                if (dton) lf = tone_run(&dtn, lf, &fx.dt1, &fx.dt2);
                w = (dly_in[i] >> 1) + mulq15(clamp(lf, -32767, 32767), wr == 2 ? fb2 : fb);
                dly_put(fx.dly_w, dig ? clamp(w, -32767, 32767) : dly_sat(w));   /* (DIGITAL: clean) */
            }
            if (wr == 2)
                oxr = mulq15(clamp(oxr, -65535, 65535), fb > 32767 ? 32767 : fb);   /* (the right: one repeat later) */
            fx.dly_w++;
        }
        if (wr) {                                       /* mid and side */
            side += mulq15(ox - oxr, fx.dm_cur);
            ox = (ox + oxr) >> 1;
        }
        y += mulq15(ox << 1, fx.dm_cur);
        wet[i] = y;
        if (side) {                                     /* (the side: left +, right -) */
            if (fxg_g < 32767)                          /* (0.6.14: gated as the rest) */
                side = mulq15(side, fxg_g);
            fx_sl[i] += side;
            fx_sr[i] -= side;
        }
    }
    rt = (uint32_t)clamp(fxp.g[G_RTYPE], 0, 4);
    if (!have_r) {                                      /* (0.6.0 rev 2) no reverb held: the model taken as it is
                                                         * (nothing playing to fade) */
        fx.rtype = (uint8_t)rt;
        fx_gate(wet, n);
        return;
    }
    if ((uint32_t)rt != fx.rtype) {                     /* the model changed: the old one's block fades out, */
        int32_t *t = fx_tmp, g = 65536, d = 65536 / (int32_t)n;   /* its buffers are cleared, the new one starts */
        for (i = 0; i < n; i++)                                   /* from silence (never two at once: their */
            t[i] = 0;                                             /* lines shared) */
        if (rev_can_run(fx.rtype))                      /* (Orange Cat: the old model's line still held) */
            rev_run(fx.rtype, rev_in, t, n);
        for (i = 0; i < n; i++, g -= d)
            wet[i] += mulq16(t[i], (uint32_t)g);
        rev_state_clear();
        rev_line_clear(rev_line_dirty(fx.rtype, (uint32_t)rt));
        fx.rtype = (uint8_t)rt;
        fx_gate(wet, n);
        return;
    }
    if (rev_can_run((uint32_t)rt))                      /* (CLOUD without its 32 KB line, SHIMMER without its 4 KB:
                                                         * not run, jfx_full says so) */
        rev_run((uint32_t)rt, rev_in, wet, n);
    fx_gate(wet, n);
}
/* every buffer and state of the buses silenced (a trip, a project load, the tests) */
static void fx_bus_clear(void)
{
    rev_state_clear();
    rev_line_clear(CL_LEN);
    if (cho_buf) memset(cho_buf, 0, CHO_LEN * sizeof *cho_buf);
    dly_line_clear(0, DLY_LEN);
    dly_lo_reset();
    memset(&fx, 0, sizeof fx);
    fxg_g = 32767; fxg_quiet = fxg_dact = fxg_hold = 0; fxg_dpk = 0; fxg_fb = fxg_dly = 0; fx_rest_ph = 0;
}

/* ------------------------------------------------------------- the master --- */
/* master: peak limiter in front of the soft clipper. Fast attack (~0.1 ms), ~150 ms release, threshold where tanh is
 * still nearly linear, so chords get quieter instead of crushed */
#define LIM_T 18000
static int32_t lim_env = LIM_T;
/* (JIANT) CLIP (G_CLIP 0..100): the mix driven x1 .. x4 into the soft clip ahead of the limiter; 0: out of the chain */
static int32_t clip_g, clip_mk;
static int32_t wet[CTL];                                /* the buses' return as the master has it (fx_wet, after the job) */
static JFX_XIP void clip_block(uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {                           /* (>> 4: the sums may be far above Q15; << 2 back; rounded) */
        int32_t l = (mix_l[i] + wet[i] + 8) >> 4, r = (mix_r[i] + wet[i] + 8) >> 4;
        mix_l[i] = ((softclip(l * clip_g >> 10) * clip_mk + 16384) >> 15) << 2;
        mix_r[i] = ((softclip(r * clip_g >> 10) * clip_mk + 16384) >> 15) << 2;
        wet[i] = 0;
    }
}
/* (0.9.1) the master DJ FILTER (G_DJF, the MASTER page): one bipolar knob on the whole mix, just before CLIP and the
 * leveler. 0: out of the chain, not run at all. Below 0 a low-pass sweeping down (darker), above 0 a high-pass sweeping
 * up (thinner): a trapezoidal SVF in 64-bit products, its coefficients Q28 and the mix 16 x finer in its states, stereo,
 * the cutoff on CUTOFF_HZ's log scale (LP ~15 kHz at -1 .. ~70 Hz at -64; HP ~32 Hz at +1 .. ~6 kHz at +63), a mild
 * resonance. The cutoff glides (~6 ms) and the coefficients ramp sample by sample; out of the chain and back, and from
 * one side to the other, a ~5.8 ms crossfade with the dry mix */
#define DJF_LO 17
#define DJF_HI 107
#define DJF_KMAX 5689
#define DJF_KMIN 3724
#define DJF_XB 8
#define DJF_SL 4
#define DJF_MAX (1 << 28)
static int32_t djf_cut(int32_t v)
{
    return v < 0 ? (127 << 8) - ((127 - DJF_LO) << 8) * -v / 64 : ((DJF_HI << 8) * v) / 63;
}
static int32_t djf_k(int32_t c, int hp)
{
    int32_t e = hp ? c * 256 / (DJF_HI << 8) : ((127 << 8) - c) * 256 / ((127 - DJF_LO) << 8);
    return DJF_KMAX - (DJF_KMAX - DJF_KMIN) * clamp(e, 0, 256) / 256;
}
static void djf_coef(int32_t *a, int32_t c, int32_t k)
{
    int32_t i = c >> 8;
    int64_t g = (int64_t)SVF_G[i] << 16, den;           /* tan(pi fc / FS), Q28, between the table's points */
    if (i < 127)
        g += (((int64_t)(SVF_G[i + 1] - SVF_G[i]) << 16) * (c & 255)) >> 8;
    den = (1 << 28) + ((g * (g + ((int64_t)k << 16))) >> 28);
    a[0] = (int32_t)(((uint64_t)1 << 56) / (uint32_t)den);
    a[1] = (int32_t)((a[0] * g) >> 28);
    a[2] = (int32_t)((a[1] * g) >> 28);
}
static struct {
    int32_t ic[4];                                      /* the states: L ic1 ic2, R ic1 ic2 (the mix << DJF_SL) */
    int32_t a[3], k;                                    /* the coefficients playing (Q28), the damping (Q12) */
    int32_t cut, w;                                     /* the cutoff gliding (Q8), the filter's share (Q12, 0: dry) */
    int8_t mode;                                        /* -1 LP, 1 HP in the chain; 0 out */
} djf;
static volatile int8_t perf_djf;                        /* (the FX layer's FILTER: jfx_perform.c; 0 none here) */
static int32_t djf_v(void) { return perf_djf ? perf_djf : clamp(song.g[G_DJF], -64, 63); }
static JFX_XIP void djf_block(uint32_t n)
{
    int32_t v = djf_v(), dm = v < 0 ? -1 : v > 0 ? 1 : 0, w0, w1, dw, k, c[3], d[3];
    uint32_t i;
    if (!djf.mode) {                                    /* in: at its cutoff, the states as the mix (no step) */
        djf.mode = (int8_t)dm;
        djf.cut = djf_cut(v);
        djf.k = djf_k(djf.cut, dm > 0);
        djf_coef(djf.a, djf.cut, djf.k);
        djf.ic[0] = djf.ic[2] = 0;
        djf.ic[1] = clamp(mix_l[0] + wet[0], -DJF_MAX >> DJF_SL, DJF_MAX >> DJF_SL) << DJF_SL;
        djf.ic[3] = clamp(mix_r[0] + wet[0], -DJF_MAX >> DJF_SL, DJF_MAX >> DJF_SL) << DJF_SL;
        djf.w = 0;
    }
    w0 = djf.w;
    if (dm == djf.mode) {                               /* its side: the filter in, the cutoff toward the knob's */
        int32_t e = djf_cut(v) - djf.cut;
        djf.cut += e / 8 ? e / 8 : e;
        w1 = w0 + 4096 / DJF_XB > 4096 ? 4096 : w0 + 4096 / DJF_XB;
    } else {                                            /* the middle, or the other side: out first */
        w1 = w0 - 4096 / DJF_XB < 0 ? 0 : w0 - 4096 / DJF_XB;
    }
    k = djf_k(djf.cut, djf.mode > 0);
    djf_coef(c, djf.cut, k);
    for (i = 0; i < 3u; i++)
        d[i] = (c[i] - djf.a[i]) / (int32_t)n;
    dw = (w1 - w0) / (int32_t)n;
    {
        int32_t a1 = djf.a[0], a2 = djf.a[1], a3 = djf.a[2], w = w0, l1 = djf.ic[0], l2 = djf.ic[1], r1 = djf.ic[2];
        int32_t r2 = djf.ic[3], kq = djf.k << 8, dk = ((k - djf.k) << 8) / (int32_t)n, hp = djf.mode > 0;
        for (i = 0; i < n; i++) {
            int32_t xl = mix_l[i] + wet[i], xr = mix_r[i] + wet[i], v1, v2, v3, yl, yr;
            int32_t sl = clamp(xl, -DJF_MAX >> DJF_SL, DJF_MAX >> DJF_SL) << DJF_SL;
            int32_t sr = clamp(xr, -DJF_MAX >> DJF_SL, DJF_MAX >> DJF_SL) << DJF_SL;
            a1 += d[0]; a2 += d[1]; a3 += d[2]; w += dw; kq += dk;
            v3 = sl - l2;
            v1 = (int32_t)(((int64_t)a1 * l1 + (int64_t)a2 * v3) >> 28);
            v2 = l2 + (int32_t)(((int64_t)a2 * l1 + (int64_t)a3 * v3) >> 28);
            l1 = clamp(2 * v1 - l1, -DJF_MAX, DJF_MAX);
            l2 = clamp(2 * v2 - l2, -DJF_MAX, DJF_MAX);
            yl = hp ? sl - v2 - (int32_t)(((int64_t)kq * v1) >> 20) : v2;
            v3 = sr - r2;
            v1 = (int32_t)(((int64_t)a1 * r1 + (int64_t)a2 * v3) >> 28);
            v2 = r2 + (int32_t)(((int64_t)a2 * r1 + (int64_t)a3 * v3) >> 28);
            r1 = clamp(2 * v1 - r1, -DJF_MAX, DJF_MAX);
            r2 = clamp(2 * v2 - r2, -DJF_MAX, DJF_MAX);
            yr = hp ? sr - v2 - (int32_t)(((int64_t)kq * v1) >> 20) : v2;
            mix_l[i] = xl + (int32_t)(((int64_t)(clamp(yl, -DJF_MAX, DJF_MAX) - sl) * w) >> (12 + DJF_SL));
            mix_r[i] = xr + (int32_t)(((int64_t)(clamp(yr, -DJF_MAX, DJF_MAX) - sr) * w) >> (12 + DJF_SL));
            wet[i] = 0;
        }
        djf.ic[0] = l1; djf.ic[1] = l2; djf.ic[2] = r1; djf.ic[3] = r2;
    }
    for (i = 0; i < 3u; i++)
        djf.a[i] = c[i];
    djf.k = k;
    djf.w = w1;
    if (!w1 && dm != djf.mode)
        djf.mode = 0;                                   /* out (the next block: in on the other side, or not at all) */
}
static void master_begin(void)
{
    int32_t a = clamp(song.g[G_CLIP], 0, 100);
    clip_g = a > 0 ? 4096 + a * 12288 / 100 : 0;
    if (clip_g)                                         /* (JIANT 0.5) made up at a mix's usual level (-15 dBFS) */
        clip_mk = (int32_t)((6000u << 15) / (uint32_t)softclip((6000 * clip_g) >> 12));
}
/* (JIANT 0.5) the LEVELER: the mix (dry + wet, before the MASTER knob) followed block by block (its peak, ~45 ms up,
 * ~1.5 s down: no pumping) and brought toward LEV_T at 2:1 (-9 .. +6 dB), ramped across the block; the gain smoothed
 * (~190 ms); below LEV_GATE (-40 dB) it eases back to unity. (Orange Cat: G_LEVEL, the MASTER page, ON or OFF; OFF: the
 * gain stays at unity, as Orange Cat's master was before) */
#define LEV_T 12000
#define LEV_GATE 400
static int32_t lev_env, lev_g = 32768, lev_cur = 32768, lev_dg;   /* the gain now and its step a sample (master_out) */
static JFX_XIP void lev_block(uint32_t n)
{
    uint32_t i;
    int32_t pk = 0, g1, on = song.g[G_LEVEL] != 0;
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i] + wet[i], r = mix_r[i] + wet[i];
        l = l < 0 ? -l : l;
        r = r < 0 ? -r : r;
        if (l > pk) pk = l;
        if (r > pk) pk = r;
    }
    lev_env += pk > lev_env ? (pk - lev_env) >> 6 : -((lev_env - pk) >> 11);
    g1 = 32768;                                         /* (below the gate, or OFF: back to unity, slowly) */
    if (on && lev_env > LEV_GATE && pk > LEV_GATE) {
        uint32_t q = ((uint32_t)LEV_T << 12) / (uint32_t)lev_env, rt = 0, b = 1u << 30;   /* LEV_T / level, Q12 */
        q <<= 8;                                        /* (Q20: its square root Q10) */
        while (b > q) b >>= 2;
        while (b) {
            if (q >= rt + b) { q -= rt + b; rt = (rt >> 1) + b; }
            else rt >>= 1;
            b >>= 2;
        }
        g1 = clamp((int32_t)rt << 5, 11500, 65536);     /* Q15: x0.35 .. x2 */
    }
    g1 = lev_g + ((g1 - lev_g) >> (on && lev_env > LEV_GATE && pk > LEV_GATE ? 8 : 5));
    lev_cur = lev_g;
    lev_dg = (g1 - lev_g) / (int32_t)n;
    lev_g = g1;
}
/* the speaker EQ (G_SPKEQ, MASTER: Jiant's MENU > LOWCUT): 1 LOWCUT 12 dB/oct ~110 Hz, 2 BASS+ (the small speaker):
 * 12 dB/oct ~220 Hz plus the harmonics of the bass (spk_bass) */
static int32_t lc_l1, lc_l2, lc_r1, lc_r2, dc_l, dc_r, dce_l, dce_r;
/* DC blocker (~2 Hz), always on: a leaky integrator of the input (Q6 state) subtracted from it, its remainder kept
 * (error feedback): the state follows the input exactly, down to 0 after the sound stops */
static inline int32_t dc_block(int32_t x, int32_t *dc, int32_t *err)
{
    int32_t e = x * 64 - *dc + *err, d = e >> 12;
    *err = e - d * 4096;
    *dc += d;
    return x - ((*dc + 32) >> 6);
}
static int32_t lce[4];
static inline int32_t lowcut1(int32_t x, int32_t *lc, int32_t *err, uint32_t sh)   /* x minus its one-pole low-pass */
{
    int32_t e = x - *lc + *err, d = e >> sh;
    *err = e - (d << sh);
    *lc += d;
    return x - *lc;
}
/* BASS+: what the speaker cannot play, heard through its harmonics. The bass below ~150 Hz is clipped at its own
 * envelope (odd harmonics), then band-passed ~220 Hz..1 kHz and added */
static int32_t sb_lp1, sb_lp2, sb_lp3, sb_lp4, sb_env, sb_h1, sb_h2, sb_hl;
static inline int32_t spk_bass(int32_t m)
{
    int32_t a, t, u;
    sb_lp1 += ((m - sb_lp1) * 692) >> 15;
    sb_lp2 += ((sb_lp1 - sb_lp2) * 692) >> 15;
    sb_lp3 += ((sb_lp2 - sb_lp3) * 692) >> 15;
    sb_lp4 += ((sb_lp3 - sb_lp4) * 692) >> 15;
    a = sb_lp4 < 0 ? -sb_lp4 : sb_lp4;
    if (a > sb_env)
        sb_env += (a - sb_env) >> 2;
    else if (sb_env > 0)
        sb_env -= (sb_env >> 11) + 1;
    t = clamp(sb_lp4 * 8, -sb_env, sb_env);
    sb_h1 += (t - sb_h1) >> 5;
    u = t - sb_h1;
    sb_h2 += (u - sb_h2) >> 5;
    u -= sb_h2;
    sb_hl += (u - sb_hl) >> 3;
    return sb_hl * 3;
}
static inline __attribute__((always_inline)) void master_out_x(int32_t *l, int32_t *r, uint32_t spk)
{
    int32_t al, ar, a;
    *l = dc_block(*l, &dc_l, &dce_l);
    *r = dc_block(*r, &dc_r, &dce_r);
    if (spk) {                                          /* two one-pole high-passes, error feedback as dc_block */
        uint32_t sh = spk == 2u ? 5u : 6u;
        int32_t b = spk == 2u ? spk_bass((*l + *r) >> 1) : 0;
        *l = lowcut1(*l, &lc_l1, &lce[0], sh);
        *l = lowcut1(*l, &lc_l2, &lce[1], sh) + b;
        *r = lowcut1(*r, &lc_r1, &lce[2], sh);
        *r = lowcut1(*r, &lc_r2, &lce[3], sh) + b;
    }
    if (lev_cur != 32768 || lev_dg) {                   /* (JIANT 0.5) the leveler's gain (unity: skipped, exact) */
        *l = (clamp(*l, -262143, 262143) * (lev_cur >> 4)) >> 11;
        *r = (clamp(*r, -262143, 262143) * (lev_cur >> 4)) >> 11;
        lev_cur += lev_dg;
    }
    al = *l < 0 ? -*l : *l;
    ar = *r < 0 ? -*r : *r;
    a = al > ar ? al : ar;
    if (a > lim_env)
        lim_env += (a - lim_env) >> 2;
    else if (lim_env > LIM_T)
        lim_env -= ((lim_env - LIM_T) >> 12) + 1;       /* (Orange Cat's release, as its master_out had it) */
    if (lim_env > LIM_T) {
        int32_t g = (int32_t)(((uint32_t)LIM_T << 15) / (uint32_t)lim_env);   /* < 32768 */
        *l = ((*l >> 4) * g) >> 11;                      /* >> 4 first: |l| may be far above Q15 */
        *r = ((*r >> 4) * g) >> 11;
    }
    *l = softclip(*l);
    *r = softclip(*r);
}
static void master_out(int32_t *l, int32_t *r) { master_out_x(l, r, (uint32_t)clamp(song.g[G_SPKEQ], 0, 2)); }

/* ------------------------------------------------------------- the rest --- */
#include "jfx_slicer.c"                                 /* the SLICER (Jiant slicer.c) */
#include "jfx_perform.c"                                /* the FX layer (Jiant perform.c) */

/* the sequencer started (jfx_clock_begin): the SLICER's step 0, the FX layer's 1/16 with it */
static void jfx_clock_started(void)
{
    slicer_start();
    perf_start();
}

/* (Orange Cat) each block, CPU0, before the buses' job: their memory. A bus takes its blocks on its first send (or, the
 * reverb, when its model wants a longer line: CLOUD) and keeps them until the buses rest (fx_rest's clearing done, then
 * JFX_IDLE blocks more): then all are given back and the next send starts from silence, as fx_rest would have left it.
 * The SLICER's and the FX layer's 32 KB: while STUT runs or a buffer effect plays (jfx_perform.c perf_mem).
 *
 * (0.6.0 rev 2) How the memory is found (the first build could never hold the delay: its two 32 KB halves fitted only
 * the arena's main bank, 54 KB, one at a time):
 *   - the delay's line is 16 blocks of 4 KB, CLOUD's 8: they fit wherever the arena has room (resources.c's banks);
 *   - the reverb's 11.9 KB, the one block that must be one piece, is taken first;
 *   - at most JFX_GROW new blocks a block (16 KB zeroed, a bounded cost in the audio interrupt): a delay comes in over
 *     4 blocks (3 ms), a bus waiting for its last blocks plays dry;
 *   - the card's 36 KB pool: each of its four 9 KB slots the card does not use is lent to the effects (resources.c
 *     res_lend_ok: the main loop's say), and taken back before the card uses it (card_play.c card_pool_take,
 *     jfx_lend_check);
 *   - an engine that finds no memory while the effects hold some has them give it all back (resources.c
 *     res_jfx_yield, jfx_lend_check): the tracks' sound first. The effects then wait JFX_YIELD blocks before taking
 *     any again, so the engine gets its memory first;
 *   - the arena short (a bus asked, none to be had): the bus's partial blocks given back, FX FULL, and no new block
 *     taken for JFX_RETRY blocks (no search and zeroing every block). */
#define JFX_IDLE ((uint32_t)FS * 2u / CTL)              /* 2 s at rest: the memory back */
#define JFX_FULL_HOLD ((uint32_t)FS / CTL)              /* (0.6.0 rev 2) FX FULL shown 1 s past its last block */
#define JFX_GROW 4u                                     /* (0.6.0 rev 2) new blocks a block, at most */
#define JFX_RETRY ((uint32_t)FS / CTL / 4u)             /* (0.6.0 rev 2) 0.25 s after the arena was short */
#define JFX_YIELD ((uint32_t)FS / CTL / 2u)             /* (0.6.0 rev 2) 0.5 s after the effects gave way to an engine */
static uint8_t jfx_full_acc;
static uint32_t jfx_full_hold;
static uint32_t jfx_idle;
static uint8_t jfx_sends;                               /* the buses wanted this block: 1 chorus 2 delay 4 reverb (a send
                                                         * not silent, or a track's send set) */
static void jfx_free_dly(void)
{
    for (uint32_t k = 0; k < DLY_NSEG; k++) {
        resource_release(RES_JFX0 + JB_DLY0 + k);
        dly_seg[k] = 0;
    }
    dly_have = 0;
}
static void jfx_free_rx(uint32_t from)                  /* the model line's blocks from `from` on */
{
    for (uint32_t k = from; k < RX_NSEG; k++) {
        resource_release(RES_JFX0 + JB_RX0 + k);
        rx_seg[k] = 0;
    }
    if (rx_have > from) rx_have = (uint8_t)from;
}
/* a pool slot held by an instance (the common block's tables say which) */
JFX_XIP static int dub_slot_used(uint32_t g)
{
    for (uint32_t i = 0; i < DUB_N; i++)
        for (uint32_t j = 0; j < dub_have[i]; j++)
            if (dub_pool[i][j] == g) return 1;
    return 0;
}
/* (Release 2) one DUB instance's memory back: its state, its spring and every block of its line (the pool slots free again) */
JFX_XIP static void jfx_free_dub(uint32_t i)
{
    for (uint32_t j = 0; j < dub_have[i]; j++) {
        resource_release(RES_JFX0 + JB_DUBG0 + dub_pool[i][j]);
        dub_line[i][j] = 0;
    }
    resource_release(RES_JFX0 + JB_DUBS0 + i);
    resource_release(RES_JFX0 + JB_DUBP0 + i);
    dub_have[i] = dub_held[i] = dub_idle[i] = dub_fresh[i] = dub_ready[i] = 0;
    dub_state[i] = 0;
    dub_spbuf[i] = 0;
}
static void dub_reset(void);                            /* (Release 2) jfx_dub.c */
static void dub_tables(void);
JFX_XIP static void jfx_free_dubs(void)
{
    for (uint32_t i = 0; i < DUB_N; i++) jfx_free_dub(i);
    resource_release(RES_JFX0 + JB_DUBC);
    dub_cm = 0;
}
static void jfx_free_buses(void)
{
    jfx_free_dly();
    jfx_free_rx(0);
    resource_release(RES_JFX0 + JB_REV);
    resource_release(RES_JFX0 + JB_CHO);
    jrev = 0;
    cho_buf = 0;
    {   /* as from a new project: the states from silence (the lines come back zeroed) */
        uint32_t keep_rt = fx.rtype;
        memset(&fx, 0, sizeof fx);
        fx.rtype = (uint8_t)keep_rt;
        memset(&dlo, 0, sizeof dlo);
        rev_state_clear();
    }
}
/* block jb of `bytes`: held (its pointer), else taken now when this block may take one more; 0 otherwise (*nomem set
 * when the arena had none) */
static void *jfx_take(uint32_t jb, uint32_t bytes, uint8_t *nomem)
{
    uint32_t size = (bytes + 3u) & ~3u;
    void *p;
    if (resource[RES_JFX0 + jb].size == size)
        return resource[RES_JFX0 + jb].ptr;
    if (jfx_wait || jfx_grow >= JFX_GROW)
        return 0;
    jfx_grow++;
    p = resource_allocate(RES_JFX0 + jb, bytes);        /* (0.6.0 rev 2, a reviewer's catch: not resource_get, which
                                                         * gives up the recording's undo for any block a bus wants and
                                                         * may then give back whole) */
    if (!p) *nomem = 1;
    return p;
}
/* (0.6.0 rev 2) the SLICER's and the FX layer's buffer taken away (the card's slot, an engine's memory): the SLICER
 * and the buffer effect stop at once, and nothing takes the buffer again this block */
static void jfx_sl_lost(void)
{
    resource_release(RES_JFX0 + JB_SL);
    sl_buf = 0;
    slicer_drop();
    perf_buf_lost();
    if (!jfx_wait) jfx_wait = 1;
}
/* (Release 2) DUB's instances, each block after the buses: an instance holds its state, its spring when it wants one, and `dub_want` blocks of its line, or nothing (all or none: one that
 * cannot finish gives back what it holds, so the others can; the same JFX_GROW, JFX_RETRY and JFX_YIELD rules as the buses'). The instances share one common block (dub_cm).
 * **The buses come first** (the owner, October 29): a bus that is on keeps its memory. When a bus wants memory and is short (bus_short) and an instance holds some, the highest
 * holder is asked to give way (dub_evict: jfx_dub.c ramps it out, then frees it) and no instance takes memory (dub_blocked) until the buses are satisfied; if the bus is short with
 * nothing of DUB's held, it would not fit anyway (CLOUD beside four MACRO tracks): dub_bus_bad lets DUB take what is left and nothing more is evicted for it. A change of an
 * instance's want (LEN, the rate stage's blocks) gives the memory back and takes it again: the line starts from silence (dub_fresh). Returns the DUB FULL bits. */
static uint32_t dub_cm_gen;                              /* resource_gen + 1 when the common block could not be had */
static __attribute__((section(".pool"))) uint8_t dub_blocked, dub_bus_bad, dub_evict;      /* dub_evict: bit i, jfx_dub.c ramps instance i out and frees it; dub_ev_full: the instances held off by jfx_dub.c (DUB FULL bits) */
static __attribute__((section(".pool"))) uint8_t dub_ev_full;
JFX_XIP static uint8_t jfx_dub_mem(uint32_t bus_fail, uint32_t bus_short)
{
    uint8_t full = 0;
    uint32_t any_want = 0, any_state = 0, i;
    dub_nomem = 0;
    if (bus_fail) {                                     /* a bus asked for memory this block and the arena had none (not the retry wait after it): one holder gives way */
        uint32_t holder = DUB_N;
        for (i = DUB_N; i-- > 0;) if (dub_state[i] && !(dub_evict & (1u << i))) { holder = i; break; }
        if (holder < DUB_N && !dub_bus_bad) { dub_evict |= (uint8_t)(1u << holder); dub_blocked = 1; }
        else if (holder == DUB_N && !dub_evict) { dub_bus_bad = 1; dub_blocked = 0; }       /* nothing of DUB's held (or being let go) and the bus is still short: it would not fit anyway */
    }
    if (!bus_short) dub_blocked = dub_bus_bad = 0;
    for (i = 0; i < DUB_N; i++) any_want |= dub_want[i];
    if (any_want && !dub_cm && !dub_blocked && dub_cm_gen != resource_gen + 1u) {
        uint8_t nm_c = 0;
        uint32_t gen0 = resource_gen;
        dub_cm = jfx_take(JB_DUBC, sizeof *dub_cm, &nm_c);
        if (!dub_cm && nm_c) dub_cm_gen = gen0 + 1u;     /* short: asks again only when something is released */
    }
    if (any_want && !dub_cm) { for (i = 0; i < DUB_N; i++) if (dub_want[i]) full |= (uint8_t)(JFX_FULL_DUB0 << i); }
    for (i = 0; i < DUB_N; i++) {
        uint32_t w = dub_want[i], g, gen0, sig = (uint32_t)dub_want[i] | (uint32_t)(dub_spring[i] != 0) << 8;
        uint8_t nm = 0;
        if (dub_state[i] && (dub_held[i] != w || dub_held_sp[i] != (dub_spring[i] != 0))) jfx_free_dub(i);   /* (jfx_dub.c changes a want only after its ramp) */
        if (!w) {
            if (dub_state[i]) jfx_free_dub(i);          /* LEN OFF: jfx_dub.c has ramped the wet out before it set the want to 0 */
            continue;
        }
        dub_idle[i] = 0;
        if (!dub_cm || (dub_blocked && !dub_state[i])) { full |= (uint8_t)(JFX_FULL_DUB0 << i); continue; }   /* (a bus needs the memory, or the common block is not there) */
        if (!dub_state[i] && dub_gen[i] == resource_gen + 1u && dub_sig[i] == sig) { full |= (uint8_t)(JFX_FULL_DUB0 << i); continue; }   /* (short, and nothing released since: no retry, no spike) */
        if (!dub_spring[i] && dub_spbuf[i]) {
            resource_release(RES_JFX0 + JB_DUBP0 + i);
            dub_spbuf[i] = 0;
        }
        gen0 = resource_gen;                            /* (after this block's own releases: they are real ones) */
        if (!dub_state[i]) dub_state[i] = jfx_take(JB_DUBS0 + i, DUB_STATE_BYTES, &nm);
        if (dub_state[i]) { dub_held[i] = (uint8_t)w; dub_held_sp[i] = (uint8_t)(dub_spring[i] != 0); }
        if (dub_state[i] && dub_spring[i] && !dub_spbuf[i]) dub_spbuf[i] = jfx_take(JB_DUBP0 + i, DUB_SPRING_BYTES, &nm);
        while (dub_state[i] && !nm && dub_have[i] < w && (!dub_spring[i] || dub_spbuf[i])) {
            for (g = 0; g < DUB_NSEG && dub_slot_used(g); g++)
                ;
            if (g == DUB_NSEG) {                        /* (the pool's 32 blocks are all in use by the others) */
                nm = 1;
                break;
            }
            int16_t *p = jfx_take(JB_DUBG0 + g, JFX_SEG * sizeof(int16_t), &nm);
            if (!p) break;
            dub_pool[i][dub_have[i]] = (uint8_t)g;
            dub_line[i][dub_have[i]++] = p;
        }
        if (nm) {
            jfx_free_dub(i);                            /* (all or none: a part given back) */
            resource_gen = gen0;                        /* (the attempt left the arena as it was: it is no release to the others) */
            dub_gen[i] = gen0 + 1u;
            dub_sig[i] = sig;
            dub_nomem = 1;
        } else if (dub_state[i] && dub_have[i] == w && (!dub_spring[i] || dub_spbuf[i])) {
            if (!dub_ready[i]) dub_fresh[i] = 1;        /* complete now: set up as new */
            dub_ready[i] = 1;
            continue;
        }
        dub_ready[i] = 0;
        if (nm || jfx_wait) full |= (uint8_t)(JFX_FULL_DUB0 << i);
    }
    for (i = 0; i < DUB_N; i++) any_state |= dub_state[i] != 0;
    if (!any_want && !any_state && dub_cm) {            /* the last instance is off: the common block goes back */
        resource_release(RES_JFX0 + JB_DUBC);
        dub_cm = 0;
    }
    return full;
}
static void jfx_mem(void)
{
    uint32_t want = jfx_sends, rt = (uint32_t)clamp(song.g[G_RTYPE], 0, 4), k, need;
    uint8_t full = 0, nm_r = 0, nm_c = 0, nm_d = 0;
    int held = dly_have || jrev || cho_buf || rx_have;
    jfx_grow = 0;
    if (!want && held && fx_rest_ph >= 6u) {            /* at rest, cleared: count to the release */
        if (++jfx_idle >= JFX_IDLE) {
            jfx_free_buses();
            jfx_idle = 0;
        }
    } else {
        jfx_idle = 0;
    }
    if ((want & 4u) || jrev) {                          /* the reverb first: its 11.9 KB in one piece */
        jrev = jfx_take(JB_REV, sizeof(jrev_t), &nm_r);
        need = rt == 4u ? RX_NSEG : 1u;                 /* (CLOUD its 0.74 s; the others the pre-delay or SHIMMER's) */
        if (need < rx_have) need = rx_have;             /* (never shrunk while held) */
        for (k = 0; jrev && k < need; k++)
            if ((rx_seg[k] = jfx_take(JB_RX0 + k, JFX_SEG * sizeof(int16_t), &nm_r)) == 0)
                break;
        if (jrev) rx_have = (uint8_t)k;
        if (nm_r) {                                     /* (short: the reverb keeps what its model can run with) */
            if (!jrev) jfx_free_rx(0);
            else if (rx_have < RX_NSEG) jfx_free_rx(rx_have ? 1u : 0u);
        }
        if ((want & 4u) && (!jrev || !rev_can_run(rt) || (rt <= 1u && !rx_have)) && (nm_r || jfx_wait))
            full |= JFX_FULL_REV;
    }
    if ((want & 1u) || cho_buf) {
        cho_buf = jfx_take(JB_CHO, CHO_LEN * sizeof(int16_t), &nm_c);
        if ((want & 1u) && !cho_buf && (nm_c || jfx_wait)) full |= JFX_FULL_CHO;
    }
    if ((want & 2u) || dly_have) {
        for (k = 0; k < DLY_NSEG; k++)
            if ((dly_seg[k] = jfx_take(JB_DLY0 + k, JFX_SEG * sizeof(int16_t), &nm_d)) == 0)
                break;
        if (k == DLY_NSEG && dly_have < DLY_NSEG) fx.dfresh = 1;   /* (complete now: set up as new, fx_buses) */
        dly_have = (uint8_t)k;
        if (nm_d) jfx_free_dly();                       /* (all or none: a part given back) */
        if ((want & 2u) && dly_have < DLY_NSEG && (nm_d || jfx_wait)) full |= JFX_FULL_DLY;
    }
    dub_full_now = (uint8_t)(jfx_dub_mem((uint32_t)(nm_r || nm_c || nm_d), (uint32_t)(full & 7u)) | dub_ev_full);                       /* (the instances' own state each block: not held, not accumulated with the buses' FX FULL) */
    if (jfx_wait) jfx_wait--;
    if (nm_r || nm_c || nm_d) jfx_wait = JFX_RETRY;           /* (DUB TAPE does not: a short instance asks again only when something was released) */
    if (full) {                                         /* (0.6.0 rev 2) FX FULL held on screen a second past the
                                                         * last block that wanted memory and got none: a send between
                                                         * notes is silent, and the words flickered */
        jfx_full_acc |= full;
        jfx_full_hold = JFX_FULL_HOLD;
    } else if (jfx_full_hold && !--jfx_full_hold) {
        jfx_full_acc = 0;
    }
    jfx_full = (uint8_t)((jfx_full & JFX_FULL_SL) | (jfx_full_acc & 0x0Fu) | dub_full_now);
}
/* (Release 2) the lend check for DUB: the common block, or any block of an instance, in a slot the card takes back: that instance, from silence later (all of them for the common block) */
JFX_XIP static void jfx_dub_lend(uint32_t back)
{
    if (dub_cm && (resource_lent(RES_JFX0 + JB_DUBC) & back)) {
        jfx_free_dubs();
        return;
    }
    for (uint32_t i = 0; i < DUB_N; i++) {
        uint32_t m = resource_lent(RES_JFX0 + JB_DUBS0 + i) | resource_lent(RES_JFX0 + JB_DUBP0 + i);
        for (uint32_t j = 0; j < dub_have[i]; j++) m |= resource_lent(RES_JFX0 + JB_DUBG0 + dub_pool[i][j]);
        if (m & back) jfx_free_dub(i);
    }
}
/* (0.6.0 rev 2) mix.c, each block first (CPU0, no bus job outstanding): an engine short of memory while the effects
 * held some (resources.c res_jfx_yield): all of theirs given back, none taken for JFX_YIELD blocks. The card's pool
 * slots taken back by the main loop (their res_lend_ok bits cleared): every bus with a block there given back, then
 * their res_lend_used bits cleared (the main loop waits for them before the card uses the slots) */
static void jfx_lend_check(void)
{
    if (res_jfx_yield) {
        jfx_bus_drain();                                /* (the buses' memory: only with no job out) */
        res_jfx_yield = 0;
        jfx_free_buses();
        jfx_free_dubs();                                /* (Release 2) an engine first: DUB's lines too */
        if (sl_buf) jfx_sl_lost();
        jfx_wait = JFX_YIELD;
        jfx_full_acc |= jfx_sends & 7u;                 /* (what was playing: FX FULL a while) */
        jfx_full_hold = JFX_FULL_HOLD;
    }
    if (!(res_lend_used & (uint8_t)~res_lend_ok))
        return;
    jfx_bus_drain();
    {
        uint32_t k, back = res_lend_used & (uint8_t)~res_lend_ok, d = 0, r = 0, used = 0;
        for (k = 0; k < DLY_NSEG; k++) d |= resource_lent(RES_JFX0 + JB_DLY0 + k) & back;
        for (k = 0; k < RX_NSEG; k++) r |= resource_lent(RES_JFX0 + JB_RX0 + k) & back;
        r |= resource_lent(RES_JFX0 + JB_REV) & back;
        if (d) {                                        /* (a bus with a block there: that whole bus, the others go on) */
            jfx_free_dly();
            dly_lo_reset();
            fx.dsw = 0;
            fx.din_g = 0;
        }
        if (r) {
            resource_release(RES_JFX0 + JB_REV);
            jrev = 0;
            jfx_free_rx(0);
            rev_state_clear();
        }
        jfx_dub_lend(back);                             /* (Release 2) DUB's instances with a block in a slot taken back */
        if (resource_lent(RES_JFX0 + JB_CHO) & back) {
            resource_release(RES_JFX0 + JB_CHO);
            cho_buf = 0;
        }
        if (sl_buf && (resource_lent(RES_JFX0 + JB_SL) & back))
            jfx_sl_lost();
        for (k = 0; k < JB_N; k++)                      /* (what is still lent: other slots) */
            used |= resource_lent(RES_JFX0 + k);
        RES_LEND_PUBLISH();
        res_lend_used = (uint8_t)used;
    }
}

/* the buses' job (CPU1 when there is one): the sends of this block in (fx_bs), the returns out (fx_bw) */
static uint32_t jfx_bus_n;
static uint8_t jfx_bus_pending, jfx_bus_ran;
static void jfx_bus_kernel(void *unused)
{
    (void)unused;
    fx_buses(fx_bs->c, fx_bs->d, fx_bs->r, fx_bw->wet, jfx_bus_n);
}
/* the copy the job reads (CPU0, at bus_start, with no job out) */
static void fxp_take(void)
{
    memcpy(fxp.g, song.g, sizeof fxp.g);
    fxp.beat = beat_samples();
    fxp.reso_mask = 0xFFFu;
    fxp.reso_root = 0;
    fxp.reso_lo = 0xFFu;
    if (song.g[G_RTYPE] == 3 || fx.rtype == 3u) {       /* RESO (running, or fading out): its inputs, read from the tracks here and not on CPU1 */
        const track_t *t = jfx_scale_track();
        uint32_t fol = reso_folw(), k, m = scale_mask(t) & 0xFFFu;
        fxp.reso_mask = m ? m : 0xFFFu;
        fxp.reso_root = t->p[P_ROOT];
        if (fol) {                                      /* FOLW: the lowest note the track sounds */
            const track_t *ft = &trk[fol - 1u];
            for (k = 0; k < NVOICE; k++)
                if (ft->v[k].active && ft->v[k].gate && ft->v[k].note < fxp.reso_lo)
                    fxp.reso_lo = ft->v[k].note;
        }
    }
}
static void jfx_bus_start(uint32_t n)
{
    uint32_t i, s = 0;
    jfx_bus_drain();                                    /* the last block's job, if still out; its returns the master's from here */
    fxp_take();
    for (i = 0; i < n; i++) {                           /* which sends are not silent (jfx_mem) */
        s |= send_c[i] ? 1u : 0u;
        s |= send_d[i] ? 2u : 0u;
        s |= send_r[i] ? 4u : 0u;
    }
    {   /* (0.6.0 rev 2) a track with a send set wants its bus too, silent or not: the memory comes in (over a few
         * blocks, JFX_GROW) before its first note, and stays while the send is set (an engine short of memory still
         * comes first: res_jfx_yield) */
        uint32_t k, nt = trk_n();
        for (k = 0; k < nt; k++) {
            const int16_t *p = trk[k].p;
            s |= p[P_CHOR] > 0 ? 1u : 0u;
            s |= p[P_DLY] > 0 ? 2u : 0u;
            s |= p[P_REV] > 0 ? 4u : 0u;
        }
    }
    jfx_sends = (uint8_t)s;
    jfx_mem();
    jfx_bus_n = n;
    jfx_bus_pending = 0;
    jfx_bus_ran = dly_have || jrev || cho_buf;
    fx_bs = &fx_snd[fx_sc];
    fx_bw = &fx_ret[fx_rw];
    fx_fresh = 1;                                       /* a result to adopt (jfx_bus_drain) */
    fx_sc ^= MELODEE_FX_DEFER;                          /* the next block's tracks fill the other set */
    if (!jfx_bus_ran) {                                 /* nothing held, nothing sent: no buses at all */
        memset(fx_bw->wet, 0, n * sizeof fx_bw->wet[0]);
        memset(fx_bw->sl, 0, n * sizeof fx_bw->sl[0]);
        memset(fx_bw->sr, 0, n * sizeof fx_bw->sr[0]);
        return;
    }
#if MELODEE_DUAL_CORE
    if (audio_worker_submit(jfx_bus_kernel, 0)) {
        jfx_bus_pending = 1;
#if MELODEE_FX_DEFER && MELODEE_DUAL_CORE
        audio_worker_deferred = jfx_bus_drain;          /* joined by whoever needs the worker next */
#endif
        return;
    }
#endif
    jfx_bus_kernel(0);
}
/* (a worker that failed: its block of the buses dropped, their states reset; CPU0 alone from then on) */
static void jfx_bus_join(uint32_t n)
{
#if MELODEE_DUAL_CORE
    if (jfx_bus_pending && !audio_worker_join()) {
        memset(fx_bw->wet, 0, n * sizeof fx_bw->wet[0]);
        memset(fx_bw->sl, 0, n * sizeof fx_bw->sl[0]);
        memset(fx_bw->sr, 0, n * sizeof fx_bw->sr[0]);
        fx_bus_clear();
    }
#endif
    jfx_bus_pending = 0;
    (void)n;
}
/* a trip (fx_trip in Jiant): the buses' output out of all bounds (a fault, never music): the block muted, every line
 * and state cleared */
static void jfx_trip_check(uint32_t n)
{
    uint32_t i;
    int32_t m = 0;
    for (i = 0; i < n; i++) {
        int32_t a = fx_bw->wet[i] < 0 ? -fx_bw->wet[i] : fx_bw->wet[i], b = fx_bw->sl[i] < 0 ? -fx_bw->sl[i] : fx_bw->sl[i];
        if (a > m) m = a;
        if (b > m) m = b;
    }
    if (m > (1 << 20)) {
        memset(fx_bw->wet, 0, n * sizeof fx_bw->wet[0]);
        memset(fx_bw->sl, 0, n * sizeof fx_bw->sl[0]);
        memset(fx_bw->sr, 0, n * sizeof fx_bw->sr[0]);
        fx_bus_clear();
    }
}
/* The buses' job joined and its returns adopted: the trip check on them, then (deferred) the master's set becomes the one just written. Safe to call
 * at any time and many times (one result is adopted once): at the first submit of the next block (audio_worker_submit), bus_start, a reset, a lend,
 * the end of a half (audio.c: nothing is out when the main loop runs, which may write the flash), and the master when the join is not deferred */
static void jfx_bus_drain(void)
{
    if (!fx_fresh) return;
    fx_fresh = 0;
#if MELODEE_FX_DEFER && MELODEE_DUAL_CORE
    audio_worker_deferred = 0;
#endif
#if OC_CORE_STATS
    {   /* (0.14.2) the join counts this job as the buses', whoever joins it (a submit about to set another kind keeps its own) */
        uint8_t kind = cs_kind;
        cs_kind = CS_K_BUS;
        jfx_bus_join(jfx_bus_n);
        cs_kind = kind;
    }
#else
    jfx_bus_join(jfx_bus_n);
#endif
    if (jfx_bus_ran)
        jfx_trip_check(jfx_bus_n);
#if MELODEE_FX_DEFER
    fx_rr = fx_rw;
    fx_rw ^= 1u;
#endif
}
/* a new project, a reset: every effect from silence (the FX layer let go), the next block */
static volatile uint8_t jfx_reset_req;
static void jfx_clear_all(void) { jfx_reset_req = 1; }
static void jfx_reset_now(void)
{
    jfx_reset_req = 0;
    jfx_bus_drain();                                    /* (no job out) */
    jfx_free_dubs();                                    /* (Release 2) a new project: DUB's lines from silence (the instances start again from their tracks' LEN) */
    memset(fx_ret, 0, sizeof fx_ret);                   /* (both sets of returns: the next master plays none of the old) */
    fx_bus_clear();
    memset(dist_st, 0, sizeof dist_st);
    memset(dist_w, 0, sizeof dist_w);
    memset(&djf, 0, sizeof djf);
    slicer_reset();
    perf_reset();
}
/* power-on, a host test's fresh start: every bus's memory back, every state from silence */
static void jfx_power_on(void)
{
    jfx_bus_drain();
    jfx_free_buses();
    jfx_free_dubs();
    dub_reset();
    dub_tables();
    resource_release(RES_JFX0 + JB_SL);
    sl_buf = 0;
    jfx_full = jfx_full_acc = 0;
    jfx_full_hold = 0;
    jfx_idle = 0;
    jfx_wait = jfx_grow = 0;
    res_jfx_yield = 0;
    jfx_reset_now();
    lim_env = LIM_T;                                    /* the master's own states (a project load keeps them: no step) */
    lev_env = lev_dg = 0;
    lev_g = lev_cur = 32768;
    lc_l1 = lc_l2 = lc_r1 = lc_r2 = dc_l = dc_r = dce_l = dce_r = 0;
    memset(lce, 0, sizeof lce);
    sb_lp1 = sb_lp2 = sb_lp3 = sb_lp4 = sb_env = sb_h1 = sb_h2 = sb_hl = 0;
}

/* the master, a block (mix.c mix_block, after the tracks): the buses joined, the SLICER, DJ FILTER, CLIP, the leveler,
 * MASTER, the FX layer, master_out. mix_l / mix_r hold the dry mix, sl_l / sl_r the SLICER's bus. out: stereo Q15 */
static void jfx_master(int32_t *out, uint32_t n, int perf)
{
    uint32_t i, spk = (uint32_t)clamp(song.g[G_SPKEQ], 0, 2);
    int32_t mg = (int32_t)song.master_q12;
    int slfx = slicer_fx();
    if (!slfx)
        slicer_bus(sl_l, sl_r, n);                      /* (CPU0, while the buses run: its clock always) */
#if !MELODEE_FX_DEFER
    jfx_bus_drain();                                    /* (the buses of this block) */
#endif
    {
        const fx_rets_t *rt = &fx_ret[fx_rr];           /* (deferred: the last block's) */
        for (i = 0; i < n; i++) {                       /* the buses' return and sides */
            wet[i] = rt->wet[i];
            mix_l[i] += rt->sl[i];
            mix_r[i] += rt->sr[i];
        }
    }
    if (slfx) {                                         /* (JIANT 0.6.3) FX ON: the returns through the SLICER */
        for (i = 0; i < n; i++) {
            sl_l[i] += wet[i];
            sl_r[i] += wet[i];
            wet[i] = 0;
        }
        slicer_bus(sl_l, sl_r, n);
    }
    if (slicer_on() || slfx)
        for (i = 0; i < n; i++) {
            mix_l[i] += sl_l[i];
            mix_r[i] += sl_r[i];
        }
    if (djf_v() || djf.mode)                            /* (0.9.1) the DJ FILTER; centred and out: not run */
        djf_block(n);
    if (clip_g)
        clip_block(n);
    lev_block(n);
    for (i = 0; i < n; i++) {
        mix_l[i] = (((mix_l[i] + wet[i]) >> 2) * mg) >> 10;
        mix_r[i] = (((mix_r[i] + wet[i]) >> 2) * mg) >> 10;
    }
    if (perf)
        perf_block(mix_l, mix_r, n);                    /* the FX layer's buffer effects and CRUSH */
    for (i = 0; i < n; i++) {
        int32_t l = mix_l[i], r = mix_r[i];
        master_out_x(&l, &r, spk);
        out[2u * i] = l;
        out[2u * i + 1u] = r;
    }
    jfx_clock_end(n);
}
