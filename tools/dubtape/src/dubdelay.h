/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay: research prototype of a multitap dub feedback delay ("DUB-TAPE") for the M-VAVE FM-1 / Orange Cat.
 * Independent host-side code; not part of the Orange Cat firmware.
 *
 * One circulating tape-style line per channel, several virtual playback heads (taps) reading it, one feedback path
 * (weighted sum of heads -> high-pass -> resonant low-pass -> drive / saturation -> written back with the new input).
 *
 *   in ->(send)-> [decimate /R] -> (+) -> sat -> line[int16] --heads h1..h4--> sum(level, pan) -> [interp xR] -> wet
 *                                   ^                               |
 *                                   +-- x FEEDBACK <- LP <- HP <-- sum(fb weight)
 *
 * Contract: 44.1 kHz host rate, blocks of DD_BLOCK frames, parameters read once per DD_CTL frames, no allocation, no
 * transcendental functions and no coefficient design in dd_process (filter warps come from tables built in dd_init).
 */
#ifndef DUBDELAY_H
#define DUBDELAY_H
#include <stdint.h>
#include <stddef.h>

#define DD_FS 44100
#ifndef DD_BLOCK                        /* (measure/core-split: the firmware bench runs it a 32-sample block at a time) */
#define DD_BLOCK 128
#endif
#define DD_CTL 32                       /* control interval (host samples), as Orange Cat's 32-sample controls */
#define DD_TAPS 4
#define DD_KNOB 128                     /* table size for 0..127 cutoff knobs */

enum { DD_ALGO_DIGITAL = 0,             /* reference: one tap, flat feedback, linear read, hard clamp */
       DD_ALGO_LITE = 1,                /* A: linear-interp heads, 1-pole HP + resonant SVF LP, cubic soft clip */
       DD_ALGO_TAPE = 2 };              /* B: Hermite heads, tape-consistent wow/flutter, emphasis + ADAA saturation,
                                         *    SVF HP with a head bump, optional second line (stereo cross-feedback) */
enum { DD_AA_HB = 0,                    /* half-band FIR decimate / interpolate (reduced-rate lines) */
       DD_AA_CHEAP = 1 };               /* box-car decimate, linear interpolate (cheaper, aliases) */

typedef struct {
    float time;                         /* base step, in seconds (heads sit at multiples of it) */
    float head[DD_TAPS];                /* head position in steps (0 = head off) */
    float level[DD_TAPS];               /* head output level 0..1 */
    float pan[DD_TAPS];                 /* -1 (L) .. +1 (R) */
    float fbw[DD_TAPS];                 /* head's share of the feedback (sum <= 1 recommended) */
    float feedback;                     /* loop gain at the filters' pass band: 0 .. 1.2 (above 1: runaway into the clip) */
    float hp, lp;                       /* cutoff knobs 0..127 (DD_KNOB table, 20 Hz .. 20 kHz, log) */
    float res;                          /* LP resonance 0..1 */
    float drive;                        /* 0..1: the loop's saturation ceiling, 0 dBFS (0) .. -12 dBFS (1) */
    float wow, flutter;                 /* 0..1 (TAPE only) */
    float xfeed;                        /* 0..1 stereo cross-feedback (TAPE with two lines only) */
    float send;                         /* 0..1 input into the loop (a "throw": open it briefly) */
    float wet, dry;                     /* output gains */
    float glide;                        /* seconds: head-move time constant (tape speed inertia); 0 = jump */
    int route;                          /* 0: HP/LP in the feedback only (first echo full-band);
                                         * 1: record path, input + feedback -> HP/LP -> saturation (tape-like: the
                                         *    first echo filtered too; less aliasing at reduced rates) */
    /* (revision 0.4, the spring: a Fisher K-10 Space Expander as King Tubby used it, a small model at the line's rate; every field 0:
     * off, so a block of parameters from before it plays as it did. Needs dd_spring_attach) */
    int spring;                         /* 0 off; 1 in the feedback loop (every regeneration through the tank); 2 on the wet after the taps */
    float sp_mix;                       /* 0..1: in the loop, the share of the regeneration through the tank; on the wet, the tank's level */
    float sp_decay;                     /* the tank's T60, seconds (0: 2 s) */
    float sp_drive;                     /* 0..1: the tube stage before the tank (soft clip) */
    float sp_tone;                      /* 0..1: darker .. less dark (the loop's and the output's low-passes) */
    float sp_in;                        /* 0..1 (on the wet): the input straight into the tank, a reverb send beside the taps */
    float tap_cut;                      /* 0..1: the taps taken out of the wet (1 with the spring on the wet: the tank alone) */
    uint32_t crash;                     /* a count: each new value kicks the tank once (a burst into it, the drive clipping it) */
    /* (revision 0.5) riding a build: the cut and the wet's limiter. Every field 0: off */
    int cut;                            /* 1: the loop's input (SEND x the input) taken to 0 over 3 ms, back over 3 ms at 0 (a fader grabbed) */
    int cut_wet;                        /* 1: the cut takes the wet (the return) as well */
    float lim_ceil;                     /* the wet's limiter after the loop: its ceiling, linear (0.5 is -6 dBFS); 0 off */
} dd_params_t;

#define DD_SP_AP 12                     /* the dispersion chain: first-order allpasses in the tank's loop (the chirp) */
#ifndef DD_SP_TANK                      /* (measure/core-split: smaller rings for the firmware bench at the half rate) */
#define DD_SP_TANK 2048u                /* the caller's buffer: the tank's ring (37 ms at any line rate to 44.1 kHz; in the loop, 0.5, 1,024 floats: 37 ms to 22.05 kHz) .. */
#define DD_SP_D1 256u                   /* .. then the two density allpasses' rings */
#define DD_SP_D2 512u
#endif
#define DD_SP_BUF (DD_SP_TANK + DD_SP_D1 + DD_SP_D2)   /* int16 samples dd_spring_attach wants */
typedef struct {
    int16_t *buf;                       /* caller-owned, DD_SP_BUF samples (0: no spring) */
    uint32_t w, w1, w2, dt, d1, d2;     /* write indices; the tank's and the density allpasses' delays (line samples) */
    float ap[DD_SP_AP];                 /* the dispersion chain's states */
    float lp, olp, dc;                  /* the loop's low-pass, the output's, a DC block on the input */
    float a, g, glp, gout, norm, gin, igin, m, in, tg;   /* set a control tick: dispersion coefficient, loop gain, low-passes, scale, drive */
    float env, crash_amp, kd;           /* the kick's burst: its envelope, its first impulse, its fall a sample */
    float gd, al;                       /* (0.5) in the loop the tank is an allpass: its diffusion and its dispersion's coefficient */
    uint32_t crash_seen, crash_n, rng;
    int place;
} dd_spring_t;

typedef struct { float st[32]; uint32_t p; uint8_t ph; } dd_hb_t;   /* half-band stage state (masked ring) */

typedef struct {
    /* configuration (dd_init only) */
    int algo, R, aa, lines;             /* R: loop-rate divisor 1/2/4; lines: 1, or 2 (TAPE stereo) */
    uint32_t len, mask;                 /* line length (power of two) */
    int16_t *line[2];                   /* caller-owned storage, len int16 each */
    float fsl;                          /* loop sample rate */
    float lat;                          /* in-loop latency, loop samples (subtracted from the heads) */
    float conv_lat;                     /* rate converters' constant latency, 44.1 kHz samples (reported) */
    float gtab[DD_KNOB];                /* tan(pi f / fsl) for the knob's frequencies, clamped below Nyquist */
    float ktab[DD_KNOB], mktab[DD_KNOB];   /* LP damping and 1 / its resonant peak, by RES (0..1 as 0..127) */
    float rot_w[2], rot_f[2];           /* wow / flutter oscillator rotation per control tick (cos, sin) */
    float wow_w, flut_w;                /* their angular rates, rad per loop sample (for the head integrals) */
    /* state */
    uint32_t w;                         /* write index */
    float d[DD_TAPS];                   /* current head delays (loop samples, smoothed) */
    float gl[DD_TAPS], gr[DD_TAPS], fw[DD_TAPS];   /* smoothed head gains */
    float hp_s[2], lp1[2], lp2[2], hp1[2], hp2[2];  /* filter states per line */
    float pre_x1[2], de_y1[2], sat_x1[2];          /* emphasis / ADAA states */
    float fb_cur, send_cur, wet_cur, dry_cur;      /* ramped gains */
    float wo[2], fo[2];                 /* wow / flutter quadrature oscillators (cos, sin) */
    float drift, drift_lp; uint32_t rng;           /* slow random speed drift */
    float m_prev[DD_TAPS], m_next[DD_TAPS];        /* head modulation (loop samples) at control ticks */
    uint32_t ctl_left;                  /* host samples to the next control tick */
    dd_hb_t dec[2], intl[2], intr[2];   /* rate converters: [0] 44.1<->22.05, [1] 22.05<->11.025 */
    float box_acc; int box_n; float lin_l[2], lin_r[2];   /* the cheap converter */
    uint32_t denorm;
    int primed;                         /* the ramped gains have been set once */
    dd_spring_t sp;                     /* (0.4) the spring */
    float cut_g, cutw_g, lim_env, lim_g; /* (0.5) the cut's gains (input, wet), the limiter's peak envelope and gain */
} dd_t;

/* bytes of line storage needed: seconds of longest head at loop rate R, per line */
uint32_t dd_line_len(float max_seconds, int R);
void dd_init(dd_t *s, int algo, int R, int aa, int lines, int16_t *line0, int16_t *line1, uint32_t len);
void dd_reset(dd_t *s);
/* in: mono send source (DD_BLOCK frames); outL/outR: dry*in + wet*echoes */
void dd_process(dd_t *s, const dd_params_t *p, const float *in, float *outL, float *outR);
/* the longest head (seconds) the line holds at this configuration */
float dd_max_seconds(const dd_t *s);
float dd_knob_hz(float knob);           /* (host side, for reports) */
/* (0.4) the spring's memory: buf, DD_SP_BUF int16 samples, owned by the caller (after dd_init; 0 takes it away) */
void dd_spring_attach(dd_t *s, int16_t *buf);
/* (0.4) the tank alone, n samples at the line's rate, its settings from p (p->spring not 0): for the bench and the measurements */
void dd_spring_block(dd_t *s, const dd_params_t *p, const float *in, float *out, int n);

#endif
