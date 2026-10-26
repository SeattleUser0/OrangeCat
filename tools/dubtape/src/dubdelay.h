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
#define DD_BLOCK 128
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
} dd_params_t;

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

#endif
