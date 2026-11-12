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
#include "fx_master.h"                                 /* (Release 3 step 8b) the master host's vocabulary: the FX layer's ring effects' state, context and calls (shared with the FX layer module) */
#include "fx_send.h"                                   /* (Release 3 step 8a) the send host's vocabulary: the tank, the modes' state, the context and the call a mode's kernel gets (shared with the reverb module) */
#include "fx_insert.h"                                 /* (Release 3 step 5) the insert host's vocabulary (fx_kernel_t, fx_ins_t; it includes dubdelay.h: dd_params_t for the size of the snapshot) */
#define JFX_XIP __attribute__((section(".text.xip_jfx"), noinline))
#define REVK_FN JFX_XIP                                 /* rev_kern.h: the resident copy of a mode's function sits where the reverb's did */
#ifndef OC_REV_RESIDENT
#define OC_REV_RESIDENT 1                               /* (Release 3 step 8a) the resident copy of SPRING, SHIMMER and RESO stays in the image this Nightly and plays whenever the module is not registered. 0 (the Nightly after: FX_MODULES_PLAN.md 16): the text is not built in, and those modes without the module are not run (the return silent) */
#endif
#if !OC_REV_RESIDENT
#define REVK_NO_MODES 1
#endif

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
typedef rev_tank_t jrev_t;                              /* (Release 3 step 8a) fx_send.h: ROOM's combs, SPRING's loop, RESO's strings (+1: even); ROOM's allpasses / SPRING's chain. The module's kernels get the same block */
_Static_assert(sizeof(((jrev_t *)0)->comb) == (REV_NCOMB + 1u) * sizeof(int16_t) && sizeof(((jrev_t *)0)->u.ap) == (REV_NAP + 1u) * sizeof(int16_t) &&
               sizeof(((jrev_t *)0)->u.sp) == (REV_NAP + 1u) / 2u * sizeof(int32_t), "fx_send.h's tank is the reverb's");
#define RPRE_LEN 2048u                                  /* the reverb's pre-delay line, at half rate (4 KB): up to 92 ms */
#define CL_LEN 16384u                                   /* .. CLOUD's, half rate: ~0.74 s (32 KB) */
#define RX_NSEG (CL_LEN / JFX_SEG)                      /* the model line: 8 blocks for CLOUD, the first one for the
                                                         * others (the pre-delay, SHIMMER's line) */
_Static_assert(RPRE_LEN == JFX_SEG && SH_LEN == JFX_SEG && (RX_NSEG & (RX_NSEG - 1u)) == 0u,
               "the model line: the pre-delay and SHIMMER in its first block");
#define SL_FR 8192u                                     /* the SLICER's recording: stereo frames (32 KB) */
/* (Release 2, generic since Release 3 step 5) the insert instances' lines, in the same 4 KB blocks (design 3, BUILD.md): a state handle and an option handle (the spring) for each of the four tracks, and a pool of
 * blocks the four share, an instance's line being the blocks it holds (any number from 1 to 16, in a ring of that many; nothing says they are side by side),
 * so a line can sit in the main bank, the cache bank and the card's lent slots as the delay's does. The state's and the option's sizes are the kernel's capability words
 * (fx_kernel_t.ram_state, ram_extra_bytes: fx_insert.h); FX_INS_N, FX_INS_NSEG and FX_INS_MAXSEG are the host's. */
/* what only a running instance needs (the job's parameter copies, the capture pair, the wet, the batch's sink) is one arena block, taken with the first instance and given back with the
 * last: an insert unused costs the build its code and a few hundred bytes, not these 3 KB */
typedef struct { uint32_t p[FX_INS_N][FX_PARAM_BYTES / 4u]; int32_t x[2][FX_INS_N][32]; float wet[FX_INS_N][2][32]; int32_t sink[32];
                 uint8_t pool[FX_INS_N][FX_INS_MAXSEG]; int16_t *line[FX_INS_N][FX_INS_MAXSEG]; } fx_common_t;   /* .. and each line block's pool slot and pointer */
static fx_common_t *fx_cm;                            /* 0: no instance holds it */
#define fx_pool (fx_cm->pool)
#define fx_line (fx_cm->line)
enum { JB_REV, JB_CHO, JB_SL, JB_DLY0, JB_RX0 = JB_DLY0 + DLY_NSEG, JB_INSS0 = JB_RX0 + RX_NSEG, JB_INSP0 = JB_INSS0 + FX_INS_N,
       JB_INSG0 = JB_INSP0 + FX_INS_N, JB_INSC = JB_INSG0 + FX_INS_NSEG, JB_N = JB_INSC + 1 };
_Static_assert(JB_N == RES_JFX_N, "resources.c: an arena handle a block");
/* the arena's blocks now (0: not held); CPU0 sets them before a job, the job reads them */
static int16_t *dly_seg[DLY_NSEG], *rx_seg[RX_NSEG], *cho_buf;
static uint8_t dly_have, rx_have;                       /* (0.6.0 rev 2) the blocks held, from the first */
static uint8_t dly_n, dly_want;                        /* (round 4) the delay's line in use, in blocks (4, 8, 12 or 16; 0: none), and the blocks asked of jfx_mem; kept out of `fx` (fx_bus_clear clears that) */
static uint16_t dly_shrink;                             /* .. the blocks the need has been under the line for: a shrink waits for DLY_SHRINK */
static uint32_t dly_gen;                                /* .. resource_gen + 1 when a grow was refused: asked again only after something was released */
#define DLY_STEP 4u
#define DLY_SHRINK 4134u                                /* 3 s of blocks (44,100 / CTL) */
static __attribute__((section(".pool"))) uint8_t fx_ins_want[FX_INS_N], fx_ins_spring[FX_INS_N];      /* (Release 2) the blocks each instance wants (0: off) and whether it wants the spring: CPU0 sets
                                                         * them before jfx_mem, from the tracks */
static __attribute__((section(".pool"))) uint8_t fx_ins_held_sp[FX_INS_N];                      /* the spring the held memory was taken for */
static __attribute__((section(".pool"))) uint16_t fx_ins_sig[FX_INS_N];
static __attribute__((section(".pool"))) uint32_t fx_ins_gen[FX_INS_N];         /* .. and the want (blocks, spring) it was short for: a new want asks again */
static __attribute__((section(".pool"))) uint8_t fx_ins_have[FX_INS_N], fx_ins_held[FX_INS_N], fx_ins_idle[FX_INS_N], fx_ins_fresh[FX_INS_N], fx_ins_ready[FX_INS_N], fx_ins_refree;   /* blocks held; the want they were taken for; blocks idle;
                                                         * a line to set up; the memory complete */
static __attribute__((section(".pool"))) uint8_t fx_ins_nomem, fx_ins_full_now;                 /* the arena was short for an instance this block; the DUB FULL bits this block */
static __attribute__((section(".pool"))) int16_t *fx_ins_spbuf[FX_INS_N];
static __attribute__((section(".pool"))) uint32_t fx_ins_spgen[FX_INS_N];                      /* (round 13) the generation a spring tank was refused at: asked again only after something is released */
static void *fx_ins_state[FX_INS_N] __attribute__((section(".pool")));
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
#define JFX_FULL_INS0 0x10u                             /* (Release 2) 0x10 .. 0x80: DUB TAPE on track 1 .. 4 wanted memory and had none (DUB FULL) */

/* (Melodee's fx.c, kept: mix.c and card_freeze.c scale with them) a * k / 65536 (k Q16) without 64 bits; x / 2^shift
 * as C's division rounds */
static inline __attribute__((always_inline)) int32_t fx_scale(int32_t a, uint32_t k)
{ return (a >> 16) * (int32_t)k + (int32_t)(((uint32_t)(a & 65535) * k) >> 16); }
static inline __attribute__((always_inline)) int32_t fx_div_pow2(int32_t x, uint32_t shift)
{ return (x + (int32_t)(((uint32_t)x >> 31) * ((1u << shift) - 1u))) >> shift; }

/* the delay's line, sample i of a ring of dly_n blocks (round 4: the line is 4 to 16 blocks, not a power of two: an index within a line's length either
 * side of 0 is folded once; the write index is kept in [0, L)) */
#ifdef JFX_DLY_CHECK                                   /* host tests: a read or a write outside the line held is counted */
static uint32_t jfx_dly_bad;
#define DLY_CHK(i) do { if ((i) >= (uint32_t)dly_have * JFX_SEG) jfx_dly_bad++; } while (0)
#else
#define DLY_CHK(i) ((void)0)
#endif
static inline __attribute__((always_inline)) uint32_t dly_fold(uint32_t i)
{
    const uint32_t L = (uint32_t)dly_n * JFX_SEG;
    if ((int32_t)i < 0) i += L;
    else if (i >= L) i -= L;
    return i;
}
static inline __attribute__((always_inline)) int32_t dly_get(uint32_t i)
{
    i = dly_fold(i);
    DLY_CHK(i);
    return dly_seg[i >> JFX_SEG_SH][i & (JFX_SEG - 1u)];
}
static inline __attribute__((always_inline)) void dly_put(uint32_t i, int32_t v)
{
    i = dly_fold(i);
    DLY_CHK(i);
    dly_seg[i >> JFX_SEG_SH][i & (JFX_SEG - 1u)] = (int16_t)v;
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
    uint8_t dr_sh, dsw, dsw_to, dn_to;          /* .. (0.8.4) the line's rate, FS >> dr_sh (0 full, 1 half, 2 a quarter); a change of
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
    rev_kst_t k;                         /* (Release 3 step 8a) SPRING's, SHIMMER's and RESO's state (fx_send.h; it was sh_w .. sp_ph here and the static sh_tlp): handed to the mode's kernel, resident or the module's, in fx2_ctx_t */
    uint32_t cl_w, cl_rng;               /* .. CLOUD: its line's write count (half rate), its random numbers */
    int32_t cl_dc;                       /* .. (0.9.14) its loop's DC (Q8 samples) */
    uint16_t cl_fill;                    /* .. (0.9.16) sound written into its line since it was cleared (to 32768) */
    int32_t cl_next, cl_lp, cl_half, cl_gl, cl_gr;     /* .. the samples to the next grain, the output's low-pass, the odd sample */
    uint32_t gr_rng;                     /* (Orange Cat) the GRAIN's random numbers: its own (Jiant's beta found the shared
                                          * generator drawn from both cores) */
} fx;
static inline __attribute__((always_inline)) void dly_adv(void)           /* the write index on, wrapped at the line's end */
{
    if (++fx.dly_w >= (uint32_t)dly_n * JFX_SEG) fx.dly_w = 0;
}
#define rev_comb (jrev->comb)
#define rev_ap (jrev->u.ap)
#define rpre_buf jrev_x
#define shim_buf jrev_x

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
    uint32_t j, Ln = (uint32_t)dly_n * JFX_SEG, top = (Ln << sh) - (2u << sh) - GR_LEN * 2u;   /* (the farthest a grain starts, FS samples) */
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
        if (di > Ln - 2u)
            di = Ln - 2u;
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
    int32_t f = q & 255, xm1, x0, x1, x2, c1, c2, c3, dist = q >> 8;
    uint32_t i;
    if (dist > (int32_t)((uint32_t)dly_n * JFX_SEG) - 4) dist = (int32_t)((uint32_t)dly_n * JFX_SEG) - 4;   /* (round 4: a read farther than the line holds, PING-PONG's twice TIME while TIME is above half the reach, a crossfade from a longer TIME: the oldest sample, never outside the ring) */
    i = w - (uint32_t)dist;
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
#define DLY_SPAN_N(sh) ((((uint32_t)dly_n * JFX_SEG) - DLY_EDGE) << (sh))   /* (round 4) .. of the line held now */
#define DLY_MAX DLY_SPAN(2u)                             /* .. of all (a quarter rate) */
#define DT_FREE 10                                       /* G_DTIME: the divisions, then free times (DTIME_MS) */
static const uint16_t DTIME_MS[54] = {5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 17, 19, 21, 24, 27, 31, 35, 39, 44, 50, 56, 64,
    72, 81, 91, 103, 116, 131, 148, 167, 189, 213, 241, 272, 307, 346, 391, 441, 498, 562, 634, 716, 808, 912, 1029, 1162,
    1311, 1480, 1670, 1885, 2128, 2402, 2711, 2950};
/* (Release 2, round 8) On an external clock the tempo (beat_samples) is measured again every six pulses from stamps quantized to 0.5 ms and moves by a few
 * samples nearly every time; a synced TIME (and the right echo's offset) that followed it a sample at a time started a 21 ms read crossfade (fx_buses,
 * dl_xf / dr_xf) for about a sixth of the time and could step the line's size at a boundary (BUILD.md 7.19, ~200 us on the worst half). They now keep the
 * value in use until the tempo has moved it by more than 1/512 (the offset: by more than 2 samples, so a BPM or WIDE step that moves it by 2 or less is not followed: at most 2 samples, not accumulating). A division change (25 percent or more), a BPM step
 * (0.8 percent at 120) and every free TIME pass at once; the internal clock never moves, so nothing changes there. Held outside `fx`, cleared with it. */
static uint32_t dly_hold_dl, dly_hold_dro;
static inline uint32_t dly_hold(uint32_t x, uint32_t *held, uint32_t tol)
{
    if (*held && (x > *held ? x - *held : *held - x) <= tol)
        return *held;
    return *held = x;
}
static uint32_t delay_samples(void)
{
    uint32_t g = (uint32_t)clamp(fxp.g[G_DTIME], 0, 63);
    uint32_t s = g < DT_FREE ? fxp_div(g) : (uint32_t)DTIME_MS[(g - DT_FREE) % NELEM(DTIME_MS)] * 441u / 10u;
    s = s < 16u ? 16u : s > DLY_MAX ? DLY_MAX : s;
    if (g < DT_FREE)
        return dly_hold(s, &dly_hold_dl, s >> 9);
    dly_hold_dl = 0;                                    /* (a free TIME: exact, and the next synced one starts from its own value) */
    return s;
}
/* the right echo's offset (WIDE: up to 1/32 of a beat later; none with PING-PONG), held the same way; exact when it is 0 */
static uint32_t dly_dro(int32_t wd, int png)
{
    uint32_t raw = wd && !png ? fxp_div(3) * (uint32_t)wd / 127u : 0u;
    if (!raw)
        return dly_hold_dro = 0u;
    return dly_hold(raw, &dly_hold_dro, 2u);
}

#include "rev_kern.h"                                  /* (Release 3 step 8a) the lines' scale and knee, and the modes SPRING, SHIMMER and RESO: the text the module also includes */
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
        C[I] = (int16_t)rv_put(in + mulq15(L, size));                                                              \
        if (++I >= REV_COMB[K])                                                                                    \
            I = 0;                                                                                                 \
        a += o;                                                                                                    \
    }
#define RV_AP(P, I, K)                                                                                             \
    {                                                                                                              \
        int32_t o = (int32_t)((uint32_t)(P[I]) << JFX_ROOM_APSH), v = a + (o >> 1);                                                       \
        P[I] = (int16_t)rv_put(v >> JFX_ROOM_APSH);                                                                \
        a = o - a;                                      /* Freeverb: out = buf - in (o - v is a notch comb) */     \
        if (++I >= REV_AP[K])                                                                                      \
            I = 0;                                                                                                 \
    }
    for (i = 0; i < n; i++) {
        int32_t a = 0, in = mulq15(RV_SEND(rev_in[i]), RV_IN);   /* 1/8 at -4 dB (the lines JFX_ROOM_HEAD bits above it): level as before the allpass fix */
        RV_COMB(c0, i0, l0, 0)
        RV_COMB(c1, i1, l1, 1)
        RV_COMB(c2, i2, l2, 2)
        RV_COMB(c3, i3, l3, 3)
        RV_AP(p0, a0, 0)
        RV_AP(p1, a1, 1)
        out[i] += a >> JFX_ROOM_HEAD;
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
        int32_t in = mulq15(RV_SEND(rev_in[i]), RV_IN);
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
            c[fx.comb_i[k]] = (int16_t)rv_put(in + mulq15(fx.comb_lp[k], size));
            if (++fx.comb_i[k] >= L)
                fx.comb_i[k] = 0;
            a += o;
            sd += (k & 1u) ? -o : o;
            c += L;
        }
        if (wd) {
            sd = (sd * wd) >> (7 + JFX_ROOM_HEAD);
            fx_sl[i] += sd;
            fx_sr[i] -= sd;
        }
        c = rev_ap;
        for (k = 0; k < 2u; k++) {
            int32_t o = (int32_t)((uint32_t)(c[fx.ap_i[k]]) << JFX_ROOM_APSH);
            int32_t v = a + (o >> 1);
            c[fx.ap_i[k]] = (int16_t)rv_put(v >> JFX_ROOM_APSH);
            a = o - a;
            if (++fx.ap_i[k] >= REV_AP[k])
                fx.ap_i[k] = 0;
            c += REV_AP[k];
        }
        out[i] += a >> JFX_ROOM_HEAD;
    }
    for (k = 0; k < 4u; k++)
        fx.rm_m[k] = m1[k];                             /* (exact at the block's end) */
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
static int32_t rev_air(void) { return revk_air(fxp.g); }
static uint32_t reso_folw(void) { return revk_folw(fxp.g); }
/* the first melodic track (its scale, Jiant's "song scale"): Orange Cat's DRUM tracks are drum_track's */
static const track_t *jfx_scale_track(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (!drum_track(&trk[k])) return &trk[k];
    return &trk[0];
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
            fx.cl_dc += ((int32_t)((uint32_t)(x) << 8) - fx.cl_dc) >> 9;     /* (0.9.14) a low cut (~14 Hz, Q8) in the loop */
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
/* (Release 3 step 8a) the send host: a mode's kernel is the registered module's (fx2_modk[mode], set by the loader through fx2_modk_set: pointers into the module's flash) or, with none, the
 * resident copy of the same text (rev_kern.h). Both get the same context: the state, the tank and the line are the host's, so a module that appears or goes away between two blocks changes
 * nothing that sounds. fx2_modk is written on CPU0 with the audio interrupt masked and no bus job out (the interrupt drains the job before it ends: audio.c); the job reads it */
static __attribute__((section(".pool"))) fx2_kcalls_t fx2_modk[FX2_MODES];     /* [mode]: the module's call, 0: none registered */
static __attribute__((section(".pool"))) uint32_t fx2_mod_version;            /* the registered module's version (0: none) */
static __attribute__((section(".pool"))) uint8_t fx2_mod_n, fx2_marked;       /* modes the module serves; the crash mark taken for this registration */
static __attribute__((section(".pool"))) void (*fx2_modk_note)(uint32_t module_id);   /* the loader's crash mark (oc_fxmod_note_use), set with the registration */
#if OC_REV_RESIDENT
static const fx2_kcalls_t REV_RESIDENT[FX2_MODES] = { {0}, {REVK_CALLS_SPRING}, {REVK_CALLS_SHIMMER}, {REVK_CALLS_RESO}, {0} };
#define REV_RESIDENT_OF(ty) (REV_RESIDENT[ty].process)
#else
#define REV_RESIDENT_OF(ty) ((void (*)(const fx2_ctx_t *, const int32_t *, int32_t *, uint32_t))0)
#endif
static inline __attribute__((always_inline)) void rev_ctx(fx2_ctx_t *c)
{
    c->g = fxp.g; c->st = &fx.k; c->fb = shim_fb; c->tank = jrev; c->line = rx_seg; c->nline = rx_have; c->sl = fx_sl; c->sr = fx_sr;
    c->reso_mask = fxp.reso_mask; c->reso_lo = fxp.reso_lo; c->reso_root = fxp.reso_root;
    c->room = rev_room; c->room_mod = rev_room_mod; c->lfo_inc = LFO_INC;
}
/* a mode that is a kernel's (1 .. 3) has one: the module's or the resident copy (always, while the resident copy is in the image: OC_REV_RESIDENT) */
static inline int rev_has_kernel(uint32_t ty) { return ty < 1u || ty > 3u || fx2_modk[ty].process || REV_RESIDENT_OF(ty); }
/* (0.6.0 rev 2) the model ty can run: CLOUD needs its 32 KB line, SHIMMER its 4 KB one (jfx_mem may not have them:
 * FX FULL). Checked before every rev_run, the fade of a model being left too (a reviewer found SHIMMER faded out with
 * no line read a null pointer) */
static int rev_can_run(uint32_t ty)
{
    return !(ty == 4u && rx_have < RX_NSEG) && !(ty == 2u && !jrev_x) && rev_has_kernel(ty);   /* (step 8a) .. and a mode that is a kernel's has one (it always has while the resident copy is in the image) */
}
static void rev_run(uint32_t ty, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t wd = clamp(fxp.g[G_RWIDE], 0, 127);
    switch (ty) {
    case 1: case 2: case 3: {
        fx2_ctx_t c;
        void (*run)(const fx2_ctx_t *, const int32_t *, int32_t *, uint32_t) = fx2_modk[ty].process;
        if (!run) run = REV_RESIDENT_OF(ty);
        rev_ctx(&c);
        run(&c, rev_in, out, n);
        break;
    }
    case 4: rev_cloud(rev_in, out, n); break;
    default:
        if (fxp.g[G_RMOD] || wd) rev_room_mod(rev_in, out, n, wd);
        else rev_room(rev_in, out, n);
        break;
    }
}
/* ---- (Release 3 step 8a) the send host's registration: what the loader (oc_module.c oc_fx2mod_reg) and the host tests call ----
 * fx2_modk_set: the bank of module FX2_MODULE_ID, n entries (effect id = the mode, its caps block, its call table), ALL OR NONE. Checked here, after the loader's fx_caps_check (the pointers, the
 * magic and the abi at install and at boot), for a caller that did not come through the loader and for what only the host knows:
 *   FX2M_VER    the caps block's magic, size, version or abi (FX2_ABI_ID: the layout of the context, the state, the tank and the call) is not this build's
 *   FX2M_RANGE  an effect id that is not a mode a kernel may serve (1 SPRING, 2 SHIMMER, 3 RESO) or twice in the bank; more than one instance; a seed; a rate other than the bus's; a state, an
 *               option or a hot set of its own (the state is the host's rev_kst_t); a model line other than the one the host gives the mode (REV_LINE: SHIMMER 1 block, SPRING and RESO none);
 *               a snapshot that is not song.g (2 * G_COUNT bytes); no call
 * 0 FX2M_OK: from the next bus job on the registered modes run the module's code. The caller masks the audio interrupt in a running session (no bus job is out between interrupts) */
#define OC_FX2_HOST 1                                   /* oc_module.c, editor.c: this build has the send host */
typedef struct { uint8_t effect_id; const fx_caps_t *caps; const fx2_kcalls_t *calls; } fx2_reg_t;
static const uint8_t REV_LINE[FX2_MODES] = { 0, 0, 1, 0, RX_NSEG };   /* the model line's blocks a mode needs before it runs (rev_can_run); the pre-delay's block is the host's own business */
static int fx2_modk_set(uint32_t module_id, uint32_t version, uint32_t n, const fx2_reg_t *e)
{
    uint32_t i, seen = 0;
    if (module_id != FX2_MODULE_ID || !n || n > FX2_MODES) return FX2M_RANGE;
    for (i = 0; i < n; i++) {
        const fx_caps_t *c = e[i].caps;
        uint32_t m = e[i].effect_id;
        if (!c || c->magic != FX_CAPS_MAGIC || c->size != FX_CAPS_WIRE || c->version != FX_CAPS_VERSION || c->abi != FX2_ABI_ID) return FX2M_VER;
        if (m < 1u || m > 3u || ((seen >> m) & 1u)) return FX2M_RANGE;
        seen |= 1u << m;
        if (c->max_inst != 1u || (c->flags & FXK_SEED) || !(c->rates & 1u) || c->ram_state || c->ram_extra || c->ram_extra_bytes || c->hot || c->nhot ||
            c->ram_work_min != REV_LINE[m] || c->ram_work_max != REV_LINE[m] || c->param_bytes != 2u * G_COUNT || !e[i].calls || !e[i].calls->process)
            return FX2M_RANGE;
    }
    for (i = 0; i < n; i++) fx2_modk[e[i].effect_id].process = e[i].calls->process;
    fx2_mod_version = version; fx2_mod_n = (uint8_t)n; fx2_marked = 0;
    return FX2M_OK;
}
/* the module goes (its sectors are about to be erased, or it no longer stands): the host's copy of the calls zeroed; from the next bus job on the resident copy plays, on the same state, tank
 * and line (nothing to hand over, nothing that sounds changes). The caller masks the audio interrupt, as for fx2_modk_set */
static void fx2_modk_clear(void)
{
    memset(fx2_modk, 0, sizeof fx2_modk);
    fx2_mod_version = 0; fx2_mod_n = 0; fx2_marked = 0;
}
static inline int fx2_modk_ok(void) { return fx2_mod_n != 0; }
/* the Effects table's `uses` (editor FX_TABLE): 1 while the reverb is held and the mode it runs is the module's */
static uint32_t fx2_modk_uses(void) { return jrev && fx2_modk[fx.rtype < FX2_MODES ? fx.rtype : 0u].process ? 1u : 0u; }
/* CPU0, at bus start, before the job is submitted: the crash mark is taken once a registration, the first time a block is about to run the module's code (the mode running, or the one
 * being changed to: its first block is the next). In flash and not inlined (as jfx_mem beside it): inlined into the bus start it put 118 B of mix_block and the loader's
 * oc_fxmod_note_use into SRAM (the audio RAM pass follows the call), 192 B off the audio arena for a call that happens once a registration */
JFX_XIP static void fx2_mark(void)
{
    if (fx2_mod_n && !fx2_marked && jrev) {
        uint32_t rt = (uint32_t)clamp(fxp.g[G_RTYPE], 0, 4);
        if (fx2_modk[fx.rtype < FX2_MODES ? fx.rtype : 0u].process || fx2_modk[rt].process) {
            fx2_marked = 1;
            if (fx2_modk_note) fx2_modk_note(FX2_MODULE_ID);
        }
    }
}
/* the reverb's buffers and states to silence (the model changed), but for the models' own line */
static void rev_state_clear(void)
{
    uint32_t i;
    if (jrev)
        memset(jrev, 0, sizeof *jrev);
    for (i = 0; i < 4u; i++)
        fx.comb_lp[i] = fx.k.rs_lp[i] = 0;
    fx.k.sp_lp = fx.k.sp_hp = fx.k.sp_he = 0;
    fx.k.sp_ns = 0;
    for (i = 0; i < CTL; i++)                           /* (JIANT 0.5) SHIMMER's feedback, CLOUD's grains */
        shim_fb[i] = 0;
    fx.k.sh_tlp = 0;
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
/* (round 4) the blocks of the line a read need (FS samples) at the rate FS >> sh takes: the samples, the 8-sample edge, in blocks, up to a multiple of 4 */
static uint32_t dly_segs_for(uint32_t need, uint32_t sh)
{
    uint32_t n = (((need + (1u << sh) - 1u) >> sh) + DLY_EDGE + JFX_SEG - 1u) / JFX_SEG;
    n = (n + DLY_STEP - 1u) / DLY_STEP * DLY_STEP;
    return n < DLY_STEP ? DLY_STEP : n > DLY_NSEG ? DLY_NSEG : n;
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
            dly_adv();
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
    uint32_t need = png ? *dl * 2u : *dl + dro, s, span, nt;
    int fresh = 0;
    s = dly_stage(need, fx.primed ? fx.dr_sh : 0u);
    nt = gr ? DLY_NSEG : dly_segs_for(need, s);          /* (round 4) the blocks it needs at that rate (the GRAIN delay sweeps the whole line) */
    if (!fx.primed || fx.dfresh) {
        fx.dr_sh = (uint8_t)s;
        fx.dsw = 0; fx.din_g = 0;
        fx.dly_w = 0;
        dly_shrink = 0;
        memset(&dlo, 0, sizeof dlo);
    } else if (fx.dsw == 2u) {                          /* clearing: a chunk this block (the larger of the old and the new line), the last one: the new rate and size */
        uint32_t Lc = (uint32_t)(dly_n > fx.dn_to ? dly_n : fx.dn_to) * JFX_SEG, from = (uint32_t)fx.dsw_n * DLY_CLR;
        dly_line_clear(from, Lc - from < DLY_CLR ? Lc - from : DLY_CLR);
        if ((uint32_t)++fx.dsw_n * DLY_CLR >= Lc) {
            fx.dr_sh = fx.dsw_to;
            dly_n = dly_want = fx.dn_to;                /* (the blocks past it are given back by jfx_mem) */
            fx.dly_w = 0;
            fx.dsw = 0; fx.din_g = DSW_FADE;
            dly_lo_reset();
            fresh = 1;
        }
    } else if (!fx.dsw && (s != fx.dr_sh || nt > dly_n || (nt < dly_n && dly_shrink >= DLY_SHRINK))) {
        if (nt > dly_have) {                            /* more blocks than held: ask (jfx_mem), go on at this size with TIME held to its reach */
            dly_want = (uint8_t)nt;
        } else {
            fx.dsw = 1;
            fx.dsw_n = DSW_FADE;
            fx.dsw_to = (uint8_t)s;
            fx.dn_to = (uint8_t)nt;
            dly_shrink = 0;
        }
    }
    if (!fx.dsw && nt <= dly_n && dly_want > dly_n) dly_want = dly_n;        /* (a grow no longer needed: the blocks taken for it are given back by jfx_mem) */
    if (nt < dly_n) { if (dly_shrink < 0xFFFFu) dly_shrink++; }   /* (the need under the line: a shrink after 3 s of it) */
    else dly_shrink = 0;
    span = DLY_SPAN_N(fx.dr_sh);
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
        dly_line_clear((ph - 1u) * ((uint32_t)dly_n * JFX_SEG / 4u), (uint32_t)dly_n * JFX_SEG / 4u);
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
    int have_d = dly_n && dly_have >= dly_n, have_r = jrev != 0, have_c = cho_buf != 0;
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
    uint32_t dro = dly_dro(wd, png), dr;   /* the right echo: up to 1/32 later */
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
                fx.dly_hp += (((int32_t)((uint32_t)(xr) << 8) - fx.dly_hp) * hk + 16384) >> 15;
            } else
                fx.dly_hp += (((int32_t)((uint32_t)(x) << 8) - fx.dly_hp) * hk + 16384) >> 15;
            {
                int32_t lf = (wr == 2 ? xr : x) - ((fx.dly_hp + 128) >> 8), w;
                if (dton) lf = tone_run(&dtn, lf, &fx.dt1, &fx.dt2);
                w = (dly_in[i] >> 1) + mulq15(clamp(lf, -32767, 32767), wr == 2 ? fb2 : fb);
                dly_put(fx.dly_w, dig ? clamp(w, -32767, 32767) : dly_sat(w));   /* (DIGITAL: clean) */
            }
            if (wr == 2)
                oxr = mulq15(clamp(oxr, -65535, 65535), fb > 32767 ? 32767 : fb);   /* (the right: one repeat later) */
            dly_adv();
        }
        if (wr) {                                       /* mid and side */
            side += mulq15(ox - oxr, fx.dm_cur);
            ox = (ox + oxr) >> 1;
        }
        y += mulq15((int32_t)((uint32_t)(ox) << 1), fx.dm_cur);
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
    dly_line_clear(0, (uint32_t)dly_have * JFX_SEG);
    dly_lo_reset();
    memset(&fx, 0, sizeof fx);
    dly_hold_dl = dly_hold_dro = 0;
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
    dly_have = dly_n = dly_want = 0;
    dly_shrink = 0;
    dly_gen = 0;
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
JFX_XIP static int fx_ins_slot_used(uint32_t g)
{
    for (uint32_t i = 0; i < FX_INS_N; i++)
        for (uint32_t j = 0; j < fx_ins_have[i]; j++)
            if (fx_pool[i][j] == g) return 1;
    return 0;
}
/* (round 13) the pool segments instance k could hold: the ones it has and the free ones (jfx_dub.c asks before it grows a running line) */
JFX_XIP static uint32_t fx_ins_pool_room(uint32_t k)
{
    uint32_t g, n = fx_ins_have[k];
    for (g = 0; g < FX_INS_NSEG; g++) if (!fx_ins_slot_used(g)) n++;
    return n;
}
/* (Release 2) one DUB instance's memory back: its state, its spring and every block of its line (the pool slots free again) */
JFX_XIP static void jfx_free_ins(uint32_t i)
{
    for (uint32_t j = 0; j < fx_ins_have[i]; j++) {
        resource_release(RES_JFX0 + JB_INSG0 + fx_pool[i][j]);
        fx_line[i][j] = 0;
    }
    resource_release(RES_JFX0 + JB_INSS0 + i);
    resource_release(RES_JFX0 + JB_INSP0 + i);
    fx_ins_have[i] = fx_ins_held[i] = fx_ins_idle[i] = fx_ins_fresh[i] = fx_ins_ready[i] = 0;
    fx_ins_state[i] = 0;
    fx_ins_spbuf[i] = 0;
}
static void fx_ins_reset(void);                         /* fx_insert.c */
JFX_XIP static void jfx_free_ins_all(void)
{
    for (uint32_t i = 0; i < FX_INS_N; i++) jfx_free_ins(i);
    resource_release(RES_JFX0 + JB_INSC);
    fx_cm = 0;
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
        dly_hold_dl = dly_hold_dro = 0;
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
/* (Release 2) DUB's instances, each block after the buses: an instance holds its state, its spring when it wants one, and `fx_ins_want` blocks of its line, or nothing (all or none: one that
 * cannot finish gives back what it holds, so the others can; the same JFX_GROW, JFX_RETRY and JFX_YIELD rules as the buses'). The instances share one common block (fx_cm).
 * **The buses come first** (the owner, October 29): a bus that is on keeps its memory. When a bus wants memory and is short (bus_short) and an instance holds some, the highest
 * holder is asked to give way (fx_ins_evict: jfx_dub.c ramps it out, then frees it) and no instance takes memory (fx_ins_blocked) until the buses are satisfied; if the bus is short with
 * nothing of DUB's held, it would not fit anyway (CLOUD beside four MACRO tracks): fx_ins_bus_bad lets DUB take what is left and nothing more is evicted for it. A change of an
 * instance's want (LEN, the rate stage's blocks) gives the memory back and takes it again: the line starts from silence (fx_ins_fresh). Returns the DUB FULL bits. */
static uint32_t fx_ins_cm_gen;                              /* resource_gen + 1 when the common block could not be had */
static __attribute__((section(".pool"))) uint8_t fx_ins_blocked, fx_ins_bus_bad, fx_ins_evict;      /* fx_ins_evict: bit i, jfx_dub.c ramps instance i out and frees it; fx_ins_ev_full: the instances held off by jfx_dub.c (DUB FULL bits) */
static __attribute__((section(".pool"))) uint8_t fx_ins_ev_full;
JFX_XIP static uint8_t jfx_ins_mem(uint32_t bus_fail, uint32_t bus_short)
{
    uint8_t full = 0;
    uint32_t any_want = 0, any_state = 0, i, sp_later = 0;
    fx_ins_nomem = 0;
    if (bus_fail) {                                     /* a bus asked for memory this block and the arena had none (not the retry wait after it): one holder gives way */
        uint32_t holder = FX_INS_N;
        for (i = FX_INS_N; i-- > 0;) if (fx_ins_state[i] && !(fx_ins_evict & (1u << i))) { holder = i; break; }
        if (holder < FX_INS_N && !fx_ins_bus_bad) { fx_ins_evict |= (uint8_t)(1u << holder); fx_ins_blocked = 1; }
        else if (holder == FX_INS_N && !fx_ins_evict) { fx_ins_bus_bad = 1; fx_ins_blocked = 0; }       /* nothing of DUB's held (or being let go) and the bus is still short: it would not fit anyway */
    }
    if (!bus_short) fx_ins_blocked = fx_ins_bus_bad = 0;
    for (i = 0; i < FX_INS_N; i++) any_want |= fx_ins_want[i];
    if (any_want && !fx_cm && !fx_ins_blocked && fx_ins_cm_gen != resource_gen + 1u) {
        uint8_t nm_c = 0;
        uint32_t gen0 = resource_gen;
        fx_cm = jfx_take(JB_INSC, sizeof *fx_cm, &nm_c);
        if (!fx_cm && nm_c) fx_ins_cm_gen = gen0 + 1u;     /* short: asks again only when something is released */
    }
    if (any_want && !fx_cm) { for (i = 0; i < FX_INS_N; i++) if (fx_ins_want[i]) full |= (uint8_t)(JFX_FULL_INS0 << i); }
    for (i = 0; i < FX_INS_N; i++) {
        uint32_t w = fx_ins_want[i], g, gen0, sig = (uint32_t)fx_ins_want[i] | (uint32_t)(fx_ins_spring[i] != 0) << 8;
        uint8_t nm = 0;
        if ((fx_ins_refree >> i) & 1u) {                    /* (round 18) the instance changed kernels (fx_insert.c): what it holds was sized by the other one's ram_state / ram_extra_bytes, so it all goes back and is taken again for the new kernel */
            fx_ins_refree &= (uint8_t)~(1u << i);
            jfx_free_ins(i);
        }
        if (fx_ins_state[i] && (fx_ins_held[i] != w || fx_ins_held_sp[i] != (fx_ins_spring[i] != 0))) jfx_free_ins(i);   /* (jfx_dub.c changes a want only after its ramp) */
        if (!w) {
            if (fx_ins_state[i]) jfx_free_ins(i);          /* LEN OFF: jfx_dub.c has ramped the wet out before it set the want to 0 */
            continue;
        }
        fx_ins_idle[i] = 0;
        if (!fx_cm || (fx_ins_blocked && !fx_ins_state[i])) { full |= (uint8_t)(JFX_FULL_INS0 << i); continue; }   /* (a bus needs the memory, or the common block is not there) */
        if (!fx_ins_state[i] && fx_ins_gen[i] == resource_gen + 1u && fx_ins_sig[i] == sig) { full |= (uint8_t)(JFX_FULL_INS0 << i); continue; }   /* (short, and nothing released since: no retry, no spike) */
        if (!fx_ins_spring[i] && fx_ins_spbuf[i]) {
            resource_release(RES_JFX0 + JB_INSP0 + i);
            fx_ins_spbuf[i] = 0;
        }
        sp_later = 0;
        gen0 = resource_gen;                            /* (after this block's own releases: they are real ones) */
        if (!fx_ins_state[i]) fx_ins_state[i] = jfx_take(JB_INSS0 + i, fx_kernel_of(i)->ram_state, &nm);
        if (fx_ins_state[i]) { fx_ins_held[i] = (uint8_t)w; fx_ins_held_sp[i] = (uint8_t)(fx_ins_spring[i] != 0); }
        while (fx_ins_state[i] && !nm && fx_ins_have[i] < w) {
            for (g = 0; g < FX_INS_NSEG && fx_ins_slot_used(g); g++)
                ;
            if (g == FX_INS_NSEG) {                        /* (the pool's 32 blocks are all in use by the others) */
                nm = 1;
                break;
            }
            int16_t *p = jfx_take(JB_INSG0 + g, JFX_SEG * sizeof(int16_t), &nm);
            if (!p) break;
            fx_pool[i][fx_ins_have[i]] = (uint8_t)g;
            fx_line[i][fx_ins_have[i]++] = p;
        }
        if (!nm && fx_ins_state[i] && fx_ins_have[i] == w && fx_ins_spring[i] && !fx_ins_spbuf[i] && fx_ins_spgen[i] != resource_gen + 1u) {   /* (round 13) the spring's tank is optional: SPRING plays without it when the arena has no 6 KB left, and asks again only when something is released */
            uint8_t nm2 = 0;
            uint32_t gen2 = resource_gen;
            fx_ins_spbuf[i] = jfx_take(JB_INSP0 + i, fx_kernel_of(i)->ram_extra_bytes, &nm2);
            if (!fx_ins_spbuf[i]) {
                if (nm2 || jfx_wait) { resource_gen = gen2; fx_ins_spgen[i] = gen2 + 1u; }   /* really short, or a bus is (jfx_wait: the retry delay of a shortage): the tank is optional, the instance plays without it, as before */
                else sp_later = 1;                      /* (round 18) refused by jfx_take for the per-block budget (JFX_GROW) alone, not for want of memory: NOT "short" (that was remembered until something was released, and the instance started without its spring for good: the common block, the state and six lines had used the block's takes); ask again next block, and an instance that is not ready yet waits for it */
            }
        }
        if (nm) {
            jfx_free_ins(i);                            /* (all or none: a part given back) */
            resource_gen = gen0;                        /* (the attempt left the arena as it was: it is no release to the others) */
            fx_ins_gen[i] = gen0 + 1u;
            fx_ins_sig[i] = sig;
            fx_ins_nomem = 1;
        } else if (fx_ins_state[i] && fx_ins_have[i] == w && !(sp_later && !fx_ins_ready[i])) {
            if (!fx_ins_ready[i]) fx_ins_fresh[i] = 1;        /* complete now: set up as new */
            fx_ins_ready[i] = 1;
            continue;
        }
        fx_ins_ready[i] = 0;
        if (nm || jfx_wait) full |= (uint8_t)(JFX_FULL_INS0 << i);
    }
    for (i = 0; i < FX_INS_N; i++) any_state |= fx_ins_state[i] != 0;
    if (!any_want && !any_state && fx_cm) {            /* the last instance is off: the common block goes back */
        resource_release(RES_JFX0 + JB_INSC);
        fx_cm = 0;
    }
    return full;
}
/* (round 4) the blocks the delay needs at first take, from the settings by the same rule as fx_buses / dly_rate (the farthest read, its rate stage, the blocks that rate takes) */
static uint32_t dly_estimate(void)
{
    uint32_t dl = delay_samples(), need, dro;
    int32_t wd = clamp(fxp.g[G_WIDTH], 0, 127), gr = fxp.g[G_DPIT] || fxp.g[G_DSPRY];
    int png = fxp.g[G_DPING] == 1 && !gr;
    if (gr) return DLY_NSEG;                            /* (the GRAIN delay sweeps the whole line) */
    dro = dly_dro(wd, png);
    need = png ? dl * 2u : dl + dro;
    return dly_segs_for(need, dly_stage(need, 0u));
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
        if (!dly_n) {                                   /* (round 4) no line yet: taken whole, as many blocks as the settings need (partly over several blocks), all or none */
            uint32_t tgt;
            if (!dly_want) dly_want = (uint8_t)dly_estimate();
            tgt = dly_want;
            for (k = 0; k < tgt; k++)
                if ((dly_seg[k] = jfx_take(JB_DLY0 + k, JFX_SEG * sizeof(int16_t), &nm_d)) == 0)
                    break;
            if (k == tgt) {                             /* (complete now: set up as new, fx_buses) */
                dly_n = (uint8_t)tgt;
                fx.dfresh = 1;
            }
            dly_have = (uint8_t)k;
            if (nm_d) jfx_free_dly();                   /* (all or none: a part given back) */
            if ((want & 2u) && !dly_n && (nm_d || jfx_wait)) full |= JFX_FULL_DLY;
        } else if (dly_want > dly_have) {               /* a grow, asked by the delay (dly_rate): optional, so a refusal is no bus failure, no FX FULL, no wait; asked again only after a release */
            if (dly_gen != resource_gen + 1u) {
                uint8_t nm_g = 0;
                for (k = dly_have; k < dly_want; k++)
                    if ((dly_seg[k] = jfx_take(JB_DLY0 + k, JFX_SEG * sizeof(int16_t), &nm_g)) == 0)
                        break;
                if (nm_g) {                             /* refused: the blocks taken this time given back, the line goes on at its size */
                    for (uint32_t j = dly_have; j < k; j++) { resource_release(RES_JFX0 + JB_DLY0 + j); dly_seg[j] = 0; }
                    dly_want = dly_n;
                    dly_gen = resource_gen + 1u;
                } else {
                    dly_have = (uint8_t)k;
                }
            }
        } else if (dly_have > dly_want && dly_n <= dly_want && !fx.dsw) {   /* the line shrank (dly_rate): the blocks past it given back */
            for (k = dly_want; k < dly_have; k++) { resource_release(RES_JFX0 + JB_DLY0 + k); dly_seg[k] = 0; }
            dly_have = dly_want;
        }
    }
    fx_ins_full_now = (uint8_t)(jfx_ins_mem((uint32_t)(nm_r || nm_c || nm_d), (uint32_t)(full & 7u)) | fx_ins_ev_full);                       /* (the instances' own state each block: not held, not accumulated with the buses' FX FULL) */
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
    jfx_full = (uint8_t)((jfx_full & JFX_FULL_SL) | (jfx_full_acc & 0x0Fu) | fx_ins_full_now);
}
/* (Release 2) the lend check for DUB: the common block, or any block of an instance, in a slot the card takes back: that instance, from silence later (all of them for the common block) */
JFX_XIP static void jfx_ins_lend(uint32_t back)
{
    if (fx_cm && (resource_lent(RES_JFX0 + JB_INSC) & back)) {
        jfx_free_ins_all();
        return;
    }
    for (uint32_t i = 0; i < FX_INS_N; i++) {
        uint32_t m = resource_lent(RES_JFX0 + JB_INSS0 + i) | resource_lent(RES_JFX0 + JB_INSP0 + i);
        for (uint32_t j = 0; j < fx_ins_have[i]; j++) m |= resource_lent(RES_JFX0 + JB_INSG0 + fx_pool[i][j]);
        if (m & back) jfx_free_ins(i);
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
        jfx_free_ins_all();                                /* (Release 2) an engine first: DUB's lines too */
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
        jfx_ins_lend(back);                             /* (Release 2) DUB's instances with a block in a slot taken back */
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
    fx2_mark();                                         /* (Release 3 step 8a) the reverb module's crash mark, before its code can run */
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
    jfx_free_ins_all();                                    /* (Release 2) a new project: DUB's lines from silence (the instances start again from their tracks' LEN) */
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
    jfx_free_ins_all();
    fx_ins_reset();
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
