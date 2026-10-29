/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Orange Cat Release 2: DUB TAPE's kernel, from the research prototype (git branch dub-tape, tools/dubtape/src/dubdelay.c 0.5), LITE only:
 * one circulating tape-style line, up to four playback heads, one feedback path (heads -> high-pass -> resonant low-pass -> drive -> back
 * into the line with the new input), the spring (in the loop or on the wet), the cut, the wet's limiter. What changed from the prototype
 * (BUILD.md, "DUB TAPE as the per-track insert", design 3): no TAPE (wow) and no DIGITAL algorithm, no second line; the line is not one
 * buffer but a table of 4 KB blocks (the arena's, jfx.c) in a ring of any number of blocks; no tables (the filter coefficients are computed at the control tick); a block is 32 samples; the line rates are the half (R 2, 22.05 kHz) and the quarter (R 4).
 * Contract: 44.1 kHz host rate, blocks of DD_BLOCK frames, parameters read once a block, no allocation; the filter coefficients are computed once a block at the control tick (a few exp, tan and one sqrt a block, no per-sample transcendental).
 */
#ifndef DUBDELAY_H
#define DUBDELAY_H
#include <stdint.h>
#include <stddef.h>

#define DD_FS 44100
#define DD_BLOCK 32
#define DD_CTL 32                       /* control interval (host samples): the block */
#define DD_TAPS 4
#define DD_KNOB 128                     /* table size for 0..127 cutoff knobs */
#define DD_BLK_SH 11u                   /* a line block: 2,048 int16 samples (4 KB) */
#define DD_BLK_N (1u << DD_BLK_SH)
#define DD_MAXBLK 16u                   /* up to 64 KB a line */

typedef struct {
    float time;                         /* base step, in seconds (heads sit at multiples of it) */
    float head[DD_TAPS];                /* head position in steps (0 = head off) */
    float level[DD_TAPS];               /* head output level 0..1 */
    float pan[DD_TAPS];                 /* -1 (L) .. +1 (R) */
    float fbw[DD_TAPS];                 /* head's share of the feedback (sum <= 1 recommended) */
    float feedback;                     /* loop gain at the filters' pass band: 0 .. 1.27 (above 1: runaway into the clip) */
    float hp, lp;                       /* cutoff knobs 0..127 (20 Hz .. 20 kHz, log) */
    float res;                          /* LP resonance 0..1 */
    float drive;                        /* 0..1: the loop's saturation ceiling, 0 dBFS (0) .. -12 dBFS (1) */
    float send;                         /* 0..1 input into the loop (a "throw": open it briefly) */
    float wet, dry;                     /* output gains */
    float glide;                        /* seconds: head-move time constant; 0 = jump */
    int route;                          /* 1: record path, input + feedback -> HP/LP -> saturation (the first echo filtered too) */
    int spring;                         /* 0 off; 1 in the feedback loop; 2 on the wet after the taps */
    float sp_mix, sp_decay, sp_drive, sp_tone, sp_in, tap_cut;
    uint32_t crash;                     /* a count: each new value kicks the tank once */
    int cut;                            /* 1: the loop's input taken to 0 over 3 ms, back over 3 ms at 0 */
    int cut_wet;                        /* 1: the cut takes the wet as well */
    float lim_ceil;                     /* the wet's limiter after the loop: its ceiling, linear; 0 off */
} dd_params_t;

#define DD_SP_AP 12
#define DD_SP_TANK 2048u
#define DD_SP_D1 256u
#define DD_SP_D2 512u
#define DD_SP_BUF (DD_SP_TANK + DD_SP_D1 + DD_SP_D2)   /* int16 samples dk_spring_attach wants (4-byte aligned) */
typedef struct {
    int16_t *buf;
    uint32_t w, w1, w2, dt, d1, d2;
    float ap[DD_SP_AP];
    float lp, olp, dc;
    float a, g, glp, gout, norm, gin, igin, m, in, tg;
    float env, crash_amp, kd;
    float gd, al;
    float wm;                           /* (Release 2) the loop spring's crossfade gain, 0 (bypassed) .. 1, a one-pole toward sp_mix: no click when the mix comes near 0 */
    uint32_t idle;                      /* blocks the loop spring has been bypassed and silent: past SP_IDLE its tank is cleared once */
    uint32_t crash_seen, crash_n, rng;
    int place;
} dd_spring_t;

typedef struct { float st[32]; uint32_t p; uint8_t ph; } dd_hb_t;   /* half-band stage state (masked ring) */

typedef struct {
    int R;                              /* loop-rate divisor 2 or 4 */
    uint32_t nblk, len;                 /* blocks held, and the ring's length in samples (nblk x 2,048) */
    int16_t *blk[DD_MAXBLK];            /* the line's blocks, caller-owned */
    float fsl;                          /* loop sample rate */
    uint32_t w;                         /* write index */
    float d[DD_TAPS];                   /* current head delays (loop samples, smoothed) */
    float gl[DD_TAPS], gr[DD_TAPS], fw[DD_TAPS];
    float hp_s, lp1, lp2;
    float fb_cur, send_cur, wet_cur, dry_cur;
    uint32_t ctl_left;
    dd_hb_t dec[2], intl[2], intr[2];   /* rate converters: [0] 44.1<->22.05, [1] 22.05<->11.025 */
    uint32_t denorm;
    int primed;
    dd_spring_t sp;
    float cut_g, cutw_g, lim_env, lim_g;
} dd_t;

/* an instance on `nblk` blocks (zeroed by the caller), at rate divisor R (2 or 4) */
void dk_init(dd_t *s, int R, int16_t *const *blk, uint32_t nblk);
void dk_reset(dd_t *s);                 /* the state from silence, the line NOT cleared (dk_clear_block) */
void dk_clear_block(dd_t *s, uint32_t k);   /* block k of the line to zero */
void dk_process(dd_t *s, const dd_params_t *p, const float *in, float *outL, float *outR);
/* the longest head (seconds) the line holds at this configuration */
float dk_max_seconds(const dd_t *s);
void dk_spring_attach(dd_t *s, int16_t *buf);
#endif
