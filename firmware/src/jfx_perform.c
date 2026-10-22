/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Orange Cat 0.6.0: the FX layer's punch-in effects, ported from JIANT FM's perform.c (main dc8d025, v0.9.18). Included
 * by jfx.c. The keys (ui_layer.c: FX held, then a key; Orange Cat's own map, the build document):
 *   REPEAT 1/8 1/16 1/32, GLITCH, TAPE STOP, SHUFFLE, FREEZE (the buffer effects: the last pressed plays, letting it go
 *   returns to the one held before); the sweeps CRUSH and THROW; tracks 1..4 muted while held (not P_MUTE, never saved).
 * The buffer effects (REPEAT, the GLITCHER: GLITCH, TAPE STOP, SHUFFLE, FREEZE) start on the next 1/16 (at once while
 * stopped) and end when let go, with a 2.9 ms ramp. While a glitcher's key is down the four knobs are its own (perf_gp,
 * never saved).
 *   GLITCH (DENS PITCH SIZE VARI): the master cut on the transport's 1/16s, each one (a hash of its place in a 2-bar
 *     cycle and VARI: the same every time, so it grooves; the beats glitched by DENS squared) played live, or: the last
 *     L repeated (L from SIZE: LONG 1/16 .. TINY 1/256), each repeat PITCH semitones above the one before; the last
 *     1/16 reversed, an octave up or down; gated at 1/32 .. 1/128; SKIP (a CD skipping); CRUSH (live, 6 .. 9 bits off
 *     and held 1 .. 6 samples). Every slice windowed. Below ~82 BPM only its gates, crushes and live slices.
 *   TAPE STOP (SCRATCH TIME CURVE): the master read from the loop at a speed falling from 1 to 0 over TIME (1/64 .. a
 *     beat), the curve (1 - x)(1 + CURVE x); the pitch with it, faded over its last quarter. Stopped, the loop is kept
 *     and KNOB 1 is the hand on the record: each detent moves it 1/48 beat, the platter follows.
 *   SHUFFLE (AMOUNT SIZE SPEED VARI): the last bar's 1/16s (1/8s, 1/4s: SIZE) played back in another order; SPEED of the
 *     moved ones at half speed, at double or their first half twice. The bar is kept in IMA ADPCM, 4 bits, mono at
 *     22.05 kHz in the same 32 KB (2.95 s), the coder's state noted at every 1/16.
 *   FREEZE (DENS PITCH SPRAY SIZE): the eighth after the press is kept (live meanwhile), then played as grains: SIZE
 *     1/128 .. 1/8 long, 1 .. 4 overlapping (DENS), PITCH semitones, scanning the eighth at 1/8 of its speed and back;
 *     SPRAY scatters where they read and moves some an octave, a fifth or a fourth.
 * The SWEEPS: held, their mix rises from dry to wet over 2 bars at the tempo, on an S curve; let go, back to dry over a
 * beat. CRUSH: the bits and the rate reduced (4 .. 11 bits off, a sample held 1 .. 8); THROW: the dry mix (and the
 * SLICER's bus) into the delay and reverb sends, up to all of it.
 * The FILTER macro (the layer's KNOB 4, perf_m[3]): the DJ FILTER's low-pass closing. (Jiant's other macros, STEPS,
 * TRANSPOSE and MUTATE, are sequencer features: not ported, the build document says.)
 * Chain: the buffer effect -> CRUSH, after the master level and before master_out; THROW and the mutes act before the
 * buses (mix.c), the FILTER macro in the DJ FILTER.
 * The buffer: the SLICER's 32 KB (jfx.c JB_SL, from the arena while needed) as one stereo loop of 8192 frames at
 * 22.05 kHz (371 ms). While borrowed, STUT plays live; its recordings are dropped afterwards. A REPEAT 1/8 longer than
 * the loop at the tempo (below 81 BPM) does nothing. Idle (no key, no ramp left) every stage is skipped.
 * The punch-in MIDI effects (Jiant's pfx.c: OCT-, OCT+, 1/2 TEMPO, DEC-, DEC+, STUTTER, ATK+, ARP, RANDOM) and the
 * punch-in lane are not here (they hang on Jiant's note handling and song sections): the keys' places are kept. */
enum { PF_R8, PF_R16, PF_R32, PF_GLT, PF_TAPE,
       PF_OCTD, PF_OCTU, PF_HALF, PF_DSHT, PF_DLNG, PF_S8, PF_S16, PF_ATK, PF_ARP, PF_RND,   /* (Jiant's MIDI ones: none) */
       PF_SHUF,
       PF_SCRU, PF_STHR,
       PF_FRZ,
       PF_M1, PF_N = PF_M1 + NTRK };
#define PF_BIT(e) (1u << (e))
#define PF_BUF (0x1Fu | PF_BIT(PF_SHUF) | PF_BIT(PF_FRZ))      /* the buffer effects: REPEAT, GLITCH, TAPE, SHUFFLE, FREEZE */
#define PF_GX (PF_BIT(PF_GLT) | PF_BIT(PF_TAPE) | PF_BIT(PF_SHUF) | PF_BIT(PF_FRZ))   /* the glitcher: its knobs */
#define PF_Q PF_BUF                                            /* start on the 1/16 */
#define PF_SWEEP (3u << PF_SCRU)
#define PF_MIDI (((1u << (PF_RND + 1)) - 1u) & ~((1u << PF_OCTD) - 1u))
#define PB_FRAMES SL_FR
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
#define GL_SPAN 7680u                         /* GLITCH: the longest slice it reads back */
#define TP_LAG 15000u                         /* TAPE STOP: how far it may fall behind */
#define AD_BYTES (SL_FR * 4u - 256u)          /* SHUFFLE's ADPCM ring: 32512 B, 65024 nibbles, 2.95 s */
#define AD_N (2u * AD_BYTES)
#define AD_TBL (&sl_buf[SL_FR - 64u][0])      /* .. the coder at each 1/16's start: 32 x (place, value, step index) */
_Static_assert(AD_N <= 65536u, "SHUFFLE's ring places in 16 bits");
enum { BM_NONE, BM_LOOP, BM_GLT, BM_TAPE, BM_SHUF, BM_FRZ };
enum { GA_PASS, GA_RPT, GA_REV, GA_UP, GA_DN, GA_GATE, GA_SKIP, GA_CRU };
enum { SK_NORM, SK_HALF, SK_DBL, SK_STUT };
enum { GX_GLT, GX_TAPE, GX_SHUF, GX_FRZ };

static volatile uint8_t perf_kill;    /* main: every effect off (a dialog, a project load) */
static volatile uint8_t perf_m[4];    /* main: the macros (Orange Cat: only FILTER, perf_m[3], 0..100) */
/* main: the glitcher's knobs (ui_layer.c), read by the ISR. GLITCH DENS % PITCH st SIZE (LONG MID SHORT TINY) VARI;
 * TAPE (SCRATCH) TIME (1/64 .. 1/4) CURVE; SHUFFLE AMOUNT % SIZE (1/4 1/8 1/16) SPEED % VARI; FREEZE DENS % PITCH st
 * SPRAY % SIZE (1/128 .. 1/8) */
static volatile int8_t perf_gp[4][4] = {{50, 0, 0, 1}, {0, 4, 0, 0}, {60, 2, 20, 1}, {50, 0, 30, 2}};
static volatile uint32_t perf_scr;    /* main: TAPE's KNOB 1 detents, a running count */
static volatile uint32_t perf_held;   /* the effects held (PF_*) */
static volatile uint32_t perf_down;   /* the effects whose key is down (FX LATCH too): a glitcher's knobs */
static volatile uint8_t perf_latch_on;   /* main: FX LATCH: a key with FX toggles its effect */
static volatile uint32_t perf_latched;   /* the effects latched (main clears it) */
static volatile uint32_t perf_act;    /* .. running (a 1/16 one from its start on) */
static volatile uint8_t perf_hush;    /* tracks the layer mutes: no new notes (voice.c) */
static uint16_t perf_ord[PF_N], perf_seq;   /* press order: the last pressed buffer effect plays */

typedef struct {                       /* SHUFFLE: a reader of the ADPCM ring, or live */
    uint32_t fr, rate;
    uint16_t pos;
    int16_t a, b;
    uint8_t idx, live;
} shr_t;
typedef struct { uint32_t pos, rate, x, dx; } grain_t;   /* FREEZE: read place (Q16), rate, window 0..65536 */
static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute;          /* sync: a 1/16 starts with the next block; mute: tracks to ramp */
    uint8_t mode, src, next;           /* BM_*, the effect it plays (PF_N none), one waiting for the fade */
    uint8_t state, odd;                /* LOOP: 0 recording, 1 playing; TAPE: 1 stopped; FREEZE: 1 frozen; a pair's half */
    uint32_t len, wr, rp;              /* loop length (samples), frames written, read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    int32_t w, tw, hl, hr;             /* the buffer's share and its target, Q15; the first of a pair */
    int32_t yl, yr;                    /* what the glitcher plays this sample */
    uint32_t gs, gph;                  /* GLITCH, SHUFFLE: the transport's 1/16 (a count), samples into it */
    union {
        struct { uint8_t a, n, h, sh; uint32_t l, u, k, g0, sp, src; int32_t rate, cl, cr; } g;   /* GLITCH */
        struct { uint32_t x, dx, p, p1, q, sc; int32_t c, gn; } t;                                 /* TAPE */
        struct { uint32_t c, fs, iv, scan, rnd, ph; int32_t sd, gain, nl, nr; grain_t v[4]; } f;   /* FREEZE */
        struct { uint32_t ew, nrec, cu, mid; int32_t ep; uint8_t ei, kind, j0, xn; shr_t r[2]; } s;   /* SHUFFLE */
    } u;
    uint32_t sw[2];                    /* the sweeps (Q24 progress: 0 dry), CRUSH, THROW */
    int32_t ss[2];                     /* .. on the S curve, Q15 */
    int32_t cw, chl, chr;              /* CRUSH share, held sample */
    uint32_t cn;
    int32_t td;                        /* THROW: the share of the dry mix sent, Q15 */
    int32_t mg[NTRK];                  /* mute gains, Q15 (32768 = open) */
} pf = {.src = PF_N, .next = PF_N, .mg = {[0 ... NTRK - 1] = 32768}};

static void perf_reset(void)
{
    memset(&pf, 0, sizeof pf);
    pf.src = pf.next = PF_N;
    for (uint32_t k = 0; k < NTRK; k++) pf.mg[k] = 32768;
    perf_act = 0;
    perf_hush = 0;
    perf_djf = 0;
}

/* the loop length of a REPEAT at the tempo, 44.1 kHz samples; 0 = not one */
static const uint8_t PF_DEN[PF_R32 + 1] = {2, 4, 8};
static uint32_t perf_len(uint32_t e) { return e <= PF_R32 ? beat_samples() / PF_DEN[e] : 0u; }
/* the effects that can run at the tempo: a REPEAT longer than the loop cannot */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e <= PF_R32; e++)
        if (b > PB_MAX * PF_DEN[e])
            m &= ~PF_BIT(e);
    return m;
}

/* a layer key down / up (e: PF_*; ui_layer.c, the main loop) */
static void perf_press(uint32_t e, int down)
{
    if (e >= PF_N || ((PF_MIDI >> e) & 1u))
        return;
    if (down) {
        perf_down |= PF_BIT(e);
        perf_ord[e] = ++perf_seq;
    } else {
        perf_down &= ~PF_BIT(e);
    }
    if (perf_latch_on) {                  /* FX LATCH: a press turns it on or off, letting go does nothing */
        if (down)
            perf_latched ^= PF_BIT(e);
        return;
    }
    if (down)
        perf_held |= PF_BIT(e);
    else
        perf_held &= ~PF_BIT(e);
}
static void perf_all_off(void) { perf_held = perf_latched = perf_down = 0; }
static volatile uint8_t perf_off_req;   /* main: every effect off (FX + OCT-), done by the next block */

static void perf_start(void) { pf.ph = 0; pf.sync = 1; }   /* the sequencer started: a 1/16 starts with it */
static void perf_drop_slicer(void) { slicer_drop(); }

/* the buffer effect to play: the last pressed of those running */
static uint32_t perf_pick(uint32_t a)
{
    uint32_t e, best = PF_N;
    a &= PF_BUF;
    for (e = 0; e < PF_M1; e++)
        if (((a >> e) & 1u) && (best == PF_N || (uint16_t)(perf_ord[e] - perf_ord[best]) < 0x8000u))
            best = e;
    return best;
}
/* the glitcher whose knobs the FX layer's are: the last pressed of those whose key is down (PF_N: none) */
static uint32_t perf_gfocus(void) { return perf_kill ? PF_N : perf_pick(perf_down & PF_GX); }
static uint32_t perf_gx(uint32_t e) { return e == PF_GLT ? GX_GLT : e == PF_TAPE ? GX_TAPE : e == PF_SHUF ? GX_SHUF : GX_FRZ; }

/* a slice's place in its 2-bar cycle (and its variation) to 32 random bits */
static uint32_t pf_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h = (h ^ (h >> 15)) * 0x2C1B3C6Du;
    return h ^ (h >> 13);
}

static void perf_glt_step(void);
static void perf_shuf_step(void);
static void perf_frz_grain(void);
static void perf_buf_start(uint32_t e)
{
    uint32_t b = beat_samples();
    if (!sl_buf)                          /* (Orange Cat: no memory: nothing plays; jfx_full says so) */
        return;
    if (!sl_lent) {
        sl_lent = 1;
        perf_drop_slicer();
    }
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.wr = pf.rp = 0;
    pf.odd = 0;
    pf.xn = 0;
    pf.mode = e == PF_GLT ? BM_GLT : e == PF_TAPE ? BM_TAPE : e == PF_SHUF ? BM_SHUF : e == PF_FRZ ? BM_FRZ : BM_LOOP;
    pf.state = 0;
    pf.len = perf_len(e);
    memset(&pf.u, 0, sizeof pf.u);
    memset(sl_buf[PB_FRAMES - 2u], 0, 2u * sizeof sl_buf[0]);   /* (what GLITCH / TAPE read before the start: quiet) */
    if (e == PF_GLT || e == PF_SHUF) {    /* its first 1/16: the transport's (this block's split starts the next one) */
        pf.gs = song.playing && clk_pos != CLK_START ? clk_n + (pf.split ? 1u : 0u) : 0u;
        if (e == PF_GLT) {
            perf_glt_step();
        } else {
            pf.u.s.r[0].live = 1;         /* (live until a chunk moves) */
            perf_shuf_step();
        }
    }
    if (e == PF_TAPE) {                   /* two frames behind (quiet), at full speed falling to 0 over TIME */
        int32_t c = perf_gp[GX_TAPE][2];
        uint32_t t = b >> (4u - (uint32_t)clamp(perf_gp[GX_TAPE][1], 0, 4)), m = TP_LAG * 600u / (uint32_t)(300 - c);
        pf.u.t.p = (PB_MAX - 4u) << 16;
        pf.u.t.c = c * 327;
        pf.u.t.dx = (1u << 24) / (t < m ? (t > 64u ? t : 64u) : m);
        pf.u.t.sc = perf_scr;
    }
    if (e == PF_FRZ) {                    /* the eighth after the press, then the grains */
        pf.u.f.c = b / 2u < PB_MAX - 1024u ? (b / 2u > 4096u ? b / 2u : 4096u) : PB_MAX - 1024u;
        pf.u.f.rnd = 0x2545F491u;
        pf.u.f.sd = 1;
        pf.u.f.v[0].x = pf.u.f.v[1].x = pf.u.f.v[2].x = pf.u.f.v[3].x = 65536u;
    }
}

/* the buffer effect to play becomes e (at a 1/16): a shorter REPEAT of what is recorded switches with a cross-fade,
 * anything else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.state && e <= PF_R32 && pf.src != PF_N &&
        perf_len(e) <= 2u * pf.wr) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.w) {
        if (e == PF_N) {
            pf.src = PF_N;
            pf.next = PF_N;
        } else {
            perf_buf_start(e);
        }
        return;
    }
    pf.src = PF_N;                                  /* fade out, then the next one (perf_buf) */
    pf.next = (uint8_t)e;
}

/* a track let go from its mute, silent (its voices ended at 0): nothing to ramp back, open at once */
static JFX_XIP void perf_unhush(void)
{
    uint32_t k, i, on;
    for (k = 0; k < NTRK; k++)
        if (!pf.mg[k] && !((pf.act >> (PF_M1 + k)) & 1u)) {
            for (i = on = 0; i < NVOICE; i++)
                on |= trk[k].v[i].active;
            if (!on)
                pf.mg[k] = 32768;
        }
}
/* the sweeps (CRUSH, THROW): held, 2 bars from dry to wet; let go, a beat back; their S curve (3 p^2 - 2 p^3) */
static JFX_XIP void perf_sweeps(uint32_t n)
{
    uint32_t k, b = beat_samples(), up = (n << 24) / (8u * (b ? b : 1u)), dn = (n << 24) / (b ? b : 1u);
    for (k = 0; k < 2u; k++) {
        uint32_t x = pf.sw[k], p;
        if ((pf.act >> (PF_SCRU + k)) & 1u)
            x = x + up < (1u << 24) ? x + up : 1u << 24;
        else
            x = x > dn ? x - dn : 0u;
        pf.sw[k] = x;
        p = x >> 9;                                 /* (Q15) */
        pf.ss[k] = (int32_t)(((p * p) >> 15) * (3u * 32768u - 2u * p) >> 15);
    }
}
/* (Orange Cat) the 32 KB the SLICER's STUT and the buffer effects share, from the arena while either wants it */
static void jfx_sl_mem(uint32_t held)
{
    int want = song.g[G_SLMODE] == SL_STUT || sl.w || pf.mode != BM_NONE || (held & PF_BUF);
    if (want) {
        sl_buf = sl_buf || !jfx_wait ? resource_get(RES_JFX0 + JB_SL, SL_FR * 2u * sizeof(int16_t)) : 0;   /* (0.6.0
                                                     * rev 2: none taken while the effects wait, jfx.c jfx_mem) */
        if (sl_buf) jfx_full &= (uint8_t)~JFX_FULL_SL;
        else jfx_full |= JFX_FULL_SL;
    } else {
        if (sl_buf) {
            resource_release(RES_JFX0 + JB_SL);
            sl_buf = 0;
            slicer_drop();
        }
        jfx_full &= (uint8_t)~JFX_FULL_SL;
    }
}
/* each block, after the events and before the parts: what runs, the 1/16, the sweeps, the mutes. Returns 1 when a stage
 * has something to do */
static int perf_begin(uint32_t n)
{
    uint32_t held;
    if (perf_off_req) {
        perf_off_req = 0;
        perf_all_off();
    }
    held = perf_kill ? 0u : (perf_held | perf_latched) & perf_avail() & ~PF_MIDI;
    uint32_t q, k, ph0, bnd;
    int32_t lp = -(int32_t)(perf_m[3] * 64u / 100u);
    jfx_sl_mem(held);
    if (!held && !pf.busy) {                        /* idle: the clock only */
        pf.ph += n;
        pf.sync = 0;
        perf_act = 0;
        perf_hush = 0;
        perf_djf = (int8_t)lp;
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    if (song.playing && clk_pos != CLK_START)       /* (JIANT) playing: the transport's 1/16, so a REPEAT starts on the grid */
        pf.P = div_samples(2);
    if (!pf.P) pf.P = 1u;
    ph0 = (song.playing && clk_pos != CLK_START ? clk_pos : pf.ph) % pf.P;   /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act |= held & ~PF_Q;
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    perf_sweeps(n);
    perf_djf = (int8_t)lp;
    pf.mute = 0;
    perf_unhush();
    for (k = 0; k < NTRK; k++)
        if (((held >> (PF_M1 + k)) & 1u) || pf.mg[k] != 32768)
            pf.mute |= (uint8_t)(1u << k);
    perf_hush = (uint8_t)((pf.act >> PF_M1) & ((1u << NTRK) - 1u));   /* muted: no new notes (voice.c) */
    pf.busy = 1;
    return 1;
}

/* a track muted by its key: its signal ramps to 0 and back (mix.c, after DIST) */
static JFX_XIP void perf_hush_end(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[k].v[i];
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = v->env_out = 0;
    }
}
static JFX_XIP void perf_mute(uint32_t k, int32_t *b, int32_t *b2, uint32_t n)
{
    uint32_t i;
    int32_t g = pf.mg[k], t = (pf.act >> (PF_M1 + k)) & 1u ? 0 : 32768;
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
        if (b2)
            b2[i] = mulq16(b2[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
    if (!g && !t)                                       /* silent: its voices end, the track costs nothing while muted */
        perf_hush_end(k);
}

/* before the buses: THROW (its sweep) adds the dry mix (and the SLICER's bus) to the delay and reverb sends */
static JFX_XIP void perf_pre(const int32_t *ml, const int32_t *mr, const int32_t *bl, const int32_t *br,
                             int32_t *sd, int32_t *sr, uint32_t n)
{
    uint32_t i;
    int32_t m = pf.ss[1];
    for (i = 0; i < n; i++) {
        pf.td += clamp(m - pf.td, -SL_SLOPE, SL_SLOPE);
        if (pf.td) {
            int32_t x = mulq16((ml[i] + mr[i] + bl[i] + br[i]) >> 1, (uint32_t)pf.td << 1);
            sd[i] += x;
            sr[i] += x;
        }
    }
}

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}
/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;
    const int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;
    if (s & 1u)
        q = f + 1u < n ? p + 2 : p;
    else
        q = f ? p - 2 : p;
    *l = (3 * p[0] + q[0]) >> 1;
    *r = (3 * p[1] + q[1]) >> 1;
}
/* the loop as a ring (GLITCH, TAPE, FREEZE) at the place s (Q16 of a 44.1 kHz sample), between its two frames */
static JFX_XIP void pb_ring_f(uint32_t s, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 17;
    int32_t fr = (int32_t)((s >> 2) & 0x7FFFu);
    const int16_t *p = &sl_buf[f & (PB_FRAMES - 1u)][0], *q = &sl_buf[(f + 1u) & (PB_FRAMES - 1u)][0];
    *l = 2 * (p[0] + (((q[0] - p[0]) * fr) >> 15));
    *r = 2 * (p[1] + (((q[1] - p[1]) * fr) >> 15));
}
/* REPEAT at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}
/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(uint32_t f, int32_t l, int32_t r)
{
    if (!pf.odd) {
        pf.hl = l;
        pf.hr = r;
        pf.odd = 1;
        return f;
    }
    pf.odd = 0;
    pb_put(f, (pf.hl + l) >> 2, (pf.hr + r) >> 2);
    return f + 1u;
}

/* ------------------------------------------------------------------ GLITCH --- */
static const uint8_t GA_OF[16] = {GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_REV, GA_REV, GA_UP, GA_DN,
                                  GA_GATE, GA_GATE, GA_SKIP, GA_SKIP, GA_CRU, GA_CRU};
/* a segment's start: where it reads (Q16) and how fast */
static JFX_XIP void perf_glt_seg(void)
{
    uint32_t a = pf.u.g.a, L = pf.u.g.l, g0 = pf.u.g.g0, s = g0 - L;
    pf.u.g.rate = 65536;
    if (a == GA_RPT) {
        pf.u.g.rate = (int32_t)semi_q16(clamp((int32_t)(pf.u.g.k < 24u ? pf.u.g.k : 24u) * perf_gp[GX_GLT][1], -24, 24));
    } else if (a == GA_REV) {
        s = g0 - 1u;
        pf.u.g.rate = -65536;
    } else if (a == GA_UP) {
        s = g0 - 2u * L;
        pf.u.g.rate = 131072;
    } else if (a == GA_DN) {
        pf.u.g.rate = 32768;
    } else if (a == GA_SKIP) {
        s = g0 - pf.u.g.sp + pf_hash(pf.gs * 64u + (pf.u.g.k >> 1)) % (pf.u.g.sp - L + 1u);
    }
    pf.u.g.src = s << 16;
}
/* GLITCH: the next 1/16's slice, by the hash of its place in two bars and VARI */
static JFX_XIP void perf_glt_step(void)
{
    uint32_t h = pf_hash((pf.gs & 31u) + 0x51u + ((uint32_t)(perf_gp[GX_GLT][3] - 1) << 5)), s16 = pf.P ? pf.P : 1u,
             sp = s16 < GL_SPAN ? s16 : GL_SPAN, d = (uint32_t)clamp(perf_gp[GX_GLT][0], 0, 100),
             z = (uint32_t)clamp(perf_gp[GX_GLT][2], 0, 3), j = (h >> 12) % 3u, a, L;
    a = ((h >> 24) * 100u >> 8) < (pf.gs & 3u ? d : d * d / 100u) ? GA_OF[(h >> 8) & 15u] : GA_PASS;
    pf.u.g.g0 = 2u * pf.wr + pf.odd;                /* (the loop's sample now) */
    if ((s16 + sp > PB_MAX - 512u || pf.u.g.g0 < sp) && a != GA_GATE && a != GA_CRU)
        a = GA_PASS;                                /* (too slow: what it would read is gone; or not recorded yet) */
    L = a == GA_RPT ? sp >> (z + j) : a == GA_GATE ? (s16 / 2u) >> (z / 2u + j % 2u) : a == GA_SKIP ? sp >> (3u + z / 2u + j % 2u)
      : a == GA_UP ? sp / 2u : a == GA_CRU ? s16 : sp;
    pf.u.g.a = (uint8_t)a;
    pf.u.g.l = L < 2u * SL_RAMP ? 2u * SL_RAMP : L;
    pf.u.g.sp = sp;
    pf.u.g.h = (uint8_t)(1u + (h >> 14) % 6u);
    pf.u.g.sh = (uint8_t)(6u + j + (h >> 20) % 2u);
    pf.u.g.n = 255u;
    pf.u.g.u = pf.u.g.k = pf.gph = 0;
    perf_glt_seg();
}
/* GLITCH on one sample: 1 = it plays (pf.yl yr), 0 = live */
static JFX_XIP int perf_glt(int32_t l, int32_t r)
{
    uint32_t u = pf.u.g.u, L = pf.u.g.l, a = pf.u.g.a, e = L - 1u - u < u ? L - 1u - u : u, g = 32768u;
    pf.wr = pb_rec(pf.wr, l, r);
    if (a != GA_PASS) {
        if (a == GA_SKIP) {                         /* (each segment windowed; SKIP's edges 0.7 ms: a tick, shaped) */
            if (e < SL_RAMP / 4u)
                g = e << (17 - SL_RAMP_LOG2);
        } else if (e < (uint32_t)SL_RAMP) {
            g = e << (15 - SL_RAMP_LOG2);
        }
        if (a == GA_GATE) {
            g = pf.u.g.k & 1u ? 0u : g;
            pf.yl = mulq16(l, g << 1);
            pf.yr = mulq16(r, g << 1);
        } else if (a == GA_CRU) {                   /* (live, crushed; windowed as the others) */
            if (++pf.u.g.n >= pf.u.g.h) {
                int32_t b = 1 << (pf.u.g.sh - 1u);
                pf.u.g.n = 0;
                pf.u.g.cl = ((l + b) >> pf.u.g.sh) << pf.u.g.sh;
                pf.u.g.cr = ((r + b) >> pf.u.g.sh) << pf.u.g.sh;
            }
            pf.yl = mulq16(pf.u.g.cl, g << 1);
            pf.yr = mulq16(pf.u.g.cr, g << 1);
        } else {
            pb_ring_f(pf.u.g.src + (uint32_t)((int32_t)u * (pf.u.g.rate >> 2)) * 4u, &pf.yl, &pf.yr);
            pf.yl = mulq16(pf.yl, g << 1);
            pf.yr = mulq16(pf.yr, g << 1);
        }
    }
    if (++pf.u.g.u >= L) {
        pf.u.g.u = 0;
        pf.u.g.k++;
        perf_glt_seg();
    }
    if (++pf.gph >= pf.P) {
        pf.gs++;
        perf_glt_step();
    }
    return a != GA_PASS;
}

/* --------------------------------------------------------------- TAPE STOP --- */
static JFX_XIP int perf_tape(int32_t l, int32_t r)
{
    uint32_t g;
    if (!pf.state) {
        uint32_t xq = pf.u.t.x >> 9, v = (32768u - xq) * (uint32_t)(32768 + ((pf.u.t.c * (int32_t)xq) >> 15)) >> 15;
        pf.wr = pb_rec(pf.wr, l, r);
        g = v < 8192u ? v << 2 : 32768u;
        pb_ring_f(pf.u.t.p, &pf.yl, &pf.yr);
        pf.u.t.p += v << 1;
        if ((pf.u.t.x += pf.u.t.dx) >= 1u << 24) { /* stopped: the loop kept, the hand on it */
            pf.state = 1;
            pf.u.t.q = pf.u.t.p1 = pf.u.t.p;
        }
    } else {
        uint32_t p0 = pf.u.t.p;
        int32_t d;
        pf.u.t.p1 += (uint32_t)((int32_t)(pf.u.t.q - pf.u.t.p1) >> 8);
        pf.u.t.p += (uint32_t)((int32_t)(pf.u.t.p1 - pf.u.t.p) >> 8);
        d = (int32_t)(pf.u.t.p - p0);
        d = d < 0 ? -d : d;                         /* (its speed, Q16: the level up to a quarter of it) */
        pf.u.t.gn += ((d >= 16384 ? 32768 : d > 1024 ? (d - 1024) << 1 : 0) - pf.u.t.gn) >> 5;
        g = (uint32_t)pf.u.t.gn;
        pb_ring_f(pf.u.t.p, &pf.yl, &pf.yr);
    }
    pf.yl = mulq16(pf.yl, g << 1);
    pf.yr = mulq16(pf.yr, g << 1);
    return 1;
}
/* each block, stopped: KNOB 1's detents since the last move the hand 1/48 beat each, within what the loop holds */
static JFX_XIP void perf_scratch(void)
{
    uint32_t b = beat_samples() / 48u;
    int32_t d = clamp((int32_t)(perf_scr - pf.u.t.sc), -64, 64), r0, r1;
    r0 = (int32_t)(((pf.u.t.q >> 16) - 2u * pf.wr) & (PB_MAX - 1u)) - (int32_t)PB_MAX;
    pf.u.t.sc = perf_scr;
    r1 = clamp(r0 + d * (int32_t)(b < 512u ? b : 512u), -(int32_t)(PB_MAX - 1024u), -4);
    pf.u.t.q += (uint32_t)(r1 - r0) << 16;
}

/* ----------------------------------------------------------------- FREEZE --- */
static const int8_t FZ_JIT[4] = {12, -12, 7, -5};
static uint32_t frz_ov(void) { return 100u + 2u * (uint32_t)clamp(perf_gp[GX_FRZ][0], 0, 100); }
static JFX_XIP void perf_frz_grain(void)
{
    grain_t *v = &pf.u.f.v[0];
    uint32_t i, r = pf.u.f.rnd = pf.u.f.rnd * 1664525u + 1013904223u, ov = frz_ov(),
             sp = (uint32_t)clamp(perf_gp[GX_FRZ][2], 0, 100), len = beat_samples() >> (5u - (uint32_t)clamp(perf_gp[GX_FRZ][3], 0, 4)),
             span, room, j;
    int32_t n = perf_gp[GX_FRZ][1], rate;
    for (i = 1; i < 4u; i++)
        if (pf.u.f.v[i].x > v->x)
            v = &pf.u.f.v[i];
    if (((r >> 24) * 100u >> 8) < sp / 2u)
        n += FZ_JIT[(r >> 22) & 3u];
    rate = (int32_t)semi_q16(n);
    if (len < 256u)
        len = 256u;
    pf.u.f.iv = len * 100u / ov;
    pf.u.f.gain = ov > 150u ? (int32_t)(4915200u / ov) : 32768;
    if ((len >> 4) * ((uint32_t)rate >> 12) + 64u > pf.u.f.c)
        len = ((pf.u.f.c - 64u) << 4) / ((uint32_t)rate >> 12);
    v->dx = 65536u / len;
    len = 65535u / v->dx + 1u;
    span = (len >> 4) * ((uint32_t)rate >> 12) + 2u;
    room = span + 48u < pf.u.f.c ? pf.u.f.c - span - 48u : 0u;
    j = room * sp / 100u;
    if (pf.u.f.scan > room)
        pf.u.f.scan = room;
    i = pf.u.f.scan + (((r >> 6) & 0xFFFFu) * j >> 16);
    i = i > j / 2u ? i - j / 2u : 0u;
    v->pos = (16u + (i < room ? i : room)) << 16;
    v->rate = (uint32_t)rate;
    v->x = 0;
    j = pf.u.f.iv / 8u + 1u;
    if (pf.u.f.sd > 0)
        pf.u.f.scan += j;
    else
        pf.u.f.scan = pf.u.f.scan > j ? pf.u.f.scan - j : 0u;
    if (pf.u.f.scan >= room || !pf.u.f.scan)
        pf.u.f.sd = pf.u.f.scan ? -1 : 1;
}
static JFX_XIP void perf_frz_eval(void)
{
    uint32_t i;
    int32_t al = 0, ar = 0;
    if ((pf.u.f.fs += 2u) >= pf.u.f.iv) {
        pf.u.f.fs = 0;
        perf_frz_grain();
    }
    for (i = 0; i < 4u; i++) {
        grain_t *v = &pf.u.f.v[i];
        if (v->x < 65536u) {
            int32_t gl, gr, w = (int32_t)((v->x * (65536u - v->x)) >> 15);
            pb_ring_f(v->pos, &gl, &gr);
            al += (gl * w) >> 15;
            ar += (gr * w) >> 15;
            v->pos += 2u * v->rate;
            v->x += 2u * v->dx;
        }
    }
    pf.u.f.nl = mulq16(al, (uint32_t)pf.u.f.gain << 1);
    pf.u.f.nr = mulq16(ar, (uint32_t)pf.u.f.gain << 1);
}
static JFX_XIP int perf_frz(int32_t l, int32_t r)
{
    if (!pf.state) {
        uint32_t i, ov;
        pf.wr = pb_rec(pf.wr, l, r);
        if (2u * pf.wr < pf.u.f.c)
            return 0;
        ov = frz_ov() / 100u;
        pf.state = 1;                               /* frozen: DENS grains at once, staggered */
        for (i = 0; i < ov; i++)
            perf_frz_grain();
        for (i = 0; i < ov; i++)
            pf.u.f.v[i].x = i * (6553600u / frz_ov());
        pf.u.f.fs = 0;
        pf.u.f.ph = 1;
        perf_frz_eval();
    }
    if ((pf.u.f.ph ^= 1u) != 0u) {
        int32_t pl = pf.u.f.nl, pr = pf.u.f.nr;
        perf_frz_eval();
        pf.yl = (pl + pf.u.f.nl) >> 1;
        pf.yr = (pr + pf.u.f.nr) >> 1;
    } else {
        pf.yl = pf.u.f.nl;
        pf.yr = pf.u.f.nr;
    }
    return 1;
}

/* ---------------------------------------------------------------- SHUFFLE --- */
/* ADPCM, 4 bits, IMA's way (its index moves, its codes) with its steps 2^(1/8) apart, 8 .. 29696 */
static const uint8_t AD_M[8] = {16, 17, 19, 21, 23, 25, 27, 29};
static const int8_t AD_IDX[8] = {-1, -1, -1, -1, 2, 4, 6, 8};
static int32_t ad_stp(uint32_t ix) { return (int32_t)((uint32_t)AD_M[ix & 7u] << (ix >> 3)) >> 1; }
/* the value after code c from p (the coder and the reader alike); codes 0 and 8 move nothing, so silence stays 0 */
static int32_t ad_next(uint32_t c, int32_t p, uint8_t *ix)
{
    int32_t s = ad_stp(*ix), d = c & 7u ? s >> 3 : 0;
    if (c & 4u) d += s;
    if (c & 2u) d += s >> 1;
    if (c & 1u) d += s >> 2;
    *ix = (uint8_t)clamp((int32_t)*ix + AD_IDX[c & 7u], 0, 95);
    return clamp(c & 8u ? p - d : p + d, -32768, 32767);
}
static void ad_put(int32_t x)
{
    uint32_t w = pf.u.s.ew, c = 0;
    int32_t d = x - pf.u.s.ep, s = ad_stp(pf.u.s.ei);
    uint8_t *b = (uint8_t *)sl_buf + (w >> 1);
    if (d < 0) {
        c = 8u;
        d = -d;
    }
    if (d >= s) { c |= 4u; d -= s; }
    if (d >= s >> 1) { c |= 2u; d -= s >> 1; }
    if (d >= s >> 2) c |= 1u;
    pf.u.s.ep = ad_next(c, pf.u.s.ep, &pf.u.s.ei);
    *b = (uint8_t)(w & 1u ? (*b & 0x0Fu) | c << 4 : (*b & 0xF0u) | c);
    pf.u.s.ew = w + 1u < AD_N ? w + 1u : 0u;
}
static int32_t shr_out(shr_t *q)
{
    int32_t y = q->a + (((q->b - q->a) * (int32_t)(q->fr >> 1)) >> 15);
    if ((q->fr += q->rate) >= 65536u) {
        uint32_t w = q->pos;
        q->fr -= 65536u;
        q->a = q->b;
        q->b = (int16_t)ad_next((uint32_t)((const uint8_t *)sl_buf)[w >> 1] >> (w & 1u ? 4 : 0) & 15u, q->b, &q->idx);
        q->pos = (uint16_t)(w + 1u < AD_N ? w + 1u : 0u);
    }
    return 2 * y;
}
static JFX_XIP void perf_shuf_go(uint32_t j, uint32_t rate)
{
    shr_t *c = &pf.u.s.r[0];
    const int16_t *t = AD_TBL + 3u * (j & 31u);
    if (j > 31u && c->live)
        return;
    pf.u.s.r[1] = *c;
    pf.u.s.xn = SL_RAMP;
    c->live = j > 31u;
    c->pos = (uint16_t)t[0];
    c->a = c->b = t[1];
    c->idx = (uint8_t)t[2];
    c->fr = 0;
    c->rate = rate;
}
static JFX_XIP void perf_shuf_step(void)
{
    uint32_t gs = pf.gs, ch = 4u >> clamp(perf_gp[GX_SHUF][1], 0, 2), s16 = pf.P / 2u ? pf.P / 2u : 1u,
             A = (uint32_t)clamp(perf_gp[GX_SHUF][0], 0, 100), h, thr, dm, d, k;
    int16_t *t = AD_TBL + 3u * (gs & 31u);
    t[0] = (int16_t)pf.u.s.ew;
    t[1] = (int16_t)pf.u.s.ep;
    t[2] = (int16_t)pf.u.s.ei;
    pf.u.s.nrec++;
    pf.gph = 0;
    if (gs % ch)
        return;
    pf.u.s.cu = 0;
    h = pf_hash((gs & 31u) + 0x3C7u + ((uint32_t)(perf_gp[GX_SHUF][3] - 1) << 5));
    thr = !(gs & 15u) ? A * A / 100u * A / 100u : !(gs & 3u) ? A * A / 100u : A;
    dm = (AD_N - 2048u) / s16;
    dm = dm > ch + 16u ? 16u : dm > ch ? dm - ch : 0u;
    dm = dm < pf.u.s.nrec - 1u ? dm : pf.u.s.nrec - 1u;
    pf.u.s.kind = SK_NORM;
    if (((h >> 24) * 100u >> 8) >= thr || dm < ch) {
        perf_shuf_go(32u, 0);
        return;
    }
    d = ch < 4u && dm >= 4u && (h & 0x100u) ? 4u * (1u + (h >> 9) % (dm / 4u)) : ch * (1u + (h >> 9) % (dm / ch));
    k = (((h >> 16) & 0xFFu) * 100u >> 8) < (uint32_t)clamp(perf_gp[GX_SHUF][2], 0, 100) ? 1u + (h >> 13) % 3u : SK_NORM;
    pf.u.s.kind = (uint8_t)k;
    pf.u.s.j0 = (uint8_t)((gs - d) & 31u);
    pf.u.s.mid = ch * pf.P / 2u;
    perf_shuf_go(pf.u.s.j0, k == SK_HALF ? 16384u : k == SK_DBL ? 65536u : 32768u);
}
static JFX_XIP int perf_shuf(int32_t l, int32_t r)
{
    shr_t *c = &pf.u.s.r[0];
    if (pf.odd) {
        ad_put(clamp((pf.hl + l + r) >> 3, -32768, 32767));
        pf.odd = 0;
    } else {
        pf.hl = l + r;
        pf.odd = 1;
    }
    if (c->live) {
        pf.yl = l;
        pf.yr = r;
    } else {
        pf.yl = pf.yr = shr_out(c);
    }
    if (pf.u.s.xn) {
        shr_t *o = &pf.u.s.r[1];
        int32_t g = (int32_t)pf.u.s.xn--, ol = l, orr = r;
        if (!o->live)
            ol = orr = shr_out(o);
        pf.yl += ((ol - pf.yl) * g) >> SL_RAMP_LOG2;
        pf.yr += ((orr - pf.yr) * g) >> SL_RAMP_LOG2;
    }
    if (++pf.u.s.cu == pf.u.s.mid && pf.u.s.kind >= SK_DBL)
        perf_shuf_go(pf.u.s.j0, c->rate);
    if (++pf.gph >= pf.P) {
        pf.gs++;
        perf_shuf_step();
    }
    return 1;
}

static int (*const PERF_RING[4])(int32_t, int32_t) = {perf_glt, perf_tape, perf_shuf, perf_frz};

/* the buffer effect faded out: the next one, or the buffer back */
static JFX_XIP void perf_buf_done(void)
{
    if (pf.next != PF_N) {
        perf_buf_start(pf.next);
        if (pf.src == PF_N) pf.mode = BM_NONE, pf.next = PF_N;   /* (no memory: none) */
    } else {
        pf.mode = BM_NONE;
        if (sl_lent) {
            sl_lent = 0;
            perf_drop_slicer();
        }
    }
}
/* the buffer effect on one sample: the share pf.w of what it plays, the rest live */
static JFX_XIP void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0;
    if (!sl_buf) {                                  /* (Orange Cat: the memory taken back: out at once) */
        pf.mode = BM_NONE;
        pf.w = 0;
        sl_lent = 0;
        return;
    }
    if (pf.mode == BM_LOOP) {
        if (!pf.state) {
            pf.wr = pb_rec(pf.wr, l, r);
            if (++pf.rp >= pf.len) {
                pf.state = 1;
                pf.rp = 0;
            }
        } else {
            pb_loop(pf.rp, pf.len, pf.wr, &yl, &yr);
            if (pf.xn) {                            /* the loop it switched from, fading out */
                int32_t ol, orr, g = (int32_t)pf.xn;
                pb_loop(pf.xrp, pf.xlen, pf.wr, &ol, &orr);
                yl += (ol * g) >> SL_RAMP_LOG2;
                yr += (orr * g) >> SL_RAMP_LOG2;
                pf.xn--;
                if (++pf.xrp >= pf.xlen)
                    pf.xrp = 0;
            }
            if (++pf.rp >= pf.len)
                pf.rp = 0;
            on = 1;
        }
    } else {
        if ((on = PERF_RING[(pf.mode - BM_GLT) & 3u](l, r)) != 0) {
            yl = pf.yl;
            yr = pf.yr;
        }
    }
    pf.tw = on && pf.src != PF_N ? 32768 : 0;
    pf.w += clamp(pf.tw - pf.w, -SL_SLOPE, SL_SLOPE);
    if (pf.w) {
        *pl = pf_mix(l, yl, pf.w);
        *pr = pf_mix(r, yr, pf.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or the buffer back */
        perf_buf_done();
    }
}
static void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* after the master level: the buffer effect, CRUSH on l / r (stereo, Q15) */
static JFX_XIP void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0, sh, hold;
    int32_t ct = pf.ss[0];
    sh = 4u + (uint32_t)(ct * 7) / 32768u;
    hold = sh - 3u;
    if (pf.split)
        perf_switch(a);
    if (pf.mode == BM_TAPE && pf.state)
        perf_scratch();
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode != BM_NONE)
            perf_buf(&l, &r);
        {   /* CRUSH: fewer bits, a held sample (its sweep: both, and its mix) */
            pf.cw += clamp(ct - pf.cw, -SL_SLOPE, SL_SLOPE);
            if (pf.cw) {
                if (++pf.cn >= hold) {
                    pf.cn = 0;
                    pf.chl = ((l >> sh) << sh) + (1 << (sh - 1u));
                    pf.chr = ((r >> sh) << sh) + (1 << (sh - 1u));
                }
                l = pf_mix(l, pf.chl, pf.cw);
                r = pf_mix(r, pf.chr, pf.cw);
            }
        }
        bl[i] = l;
        br[i] = r;
    }
    pf.busy = pf.act || pf.mode != BM_NONE || pf.w || pf.cw || pf.td || pf.mute || pf.sw[0] || pf.sw[1];
}
