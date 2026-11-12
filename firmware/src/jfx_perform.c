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
enum { BM_NONE, BM_LOOP, BM_GLT, BM_TAPE, BM_SHUF, BM_FRZ };
/* (Release 3 step 8b, FX_MODULES_PLAN.md 17) the four ring effects' text is perf_kern.h, ONE text the image's resident copy (here) and the effect module (tools/module/fxlayer_mod.c: kind 3)
 * both include; this file is their host: the keys, what plays, the 1/16, the 32 KB, REPEAT, the sweeps, the mutes, and the dispatch to the registered module's kernel or the resident copy */
#ifndef OC_PERF_RESIDENT
#define OC_PERF_RESIDENT 1                               /* the resident copy of GLITCH, TAPE STOP, SHUFFLE and FREEZE stays in the image this Nightly and plays whenever the module is not registered. 0 (the Nightly after: FX_MODULES_PLAN.md 17): the text is not built in, and without the module those four keys do nothing (perf_avail) */
#endif
#define PFK_FN JFX_XIP                                   /* perf_kern.h: the resident copy of a kernel function sits where the layer's did */
#if !OC_PERF_RESIDENT
#define PFK_NO_FX 1
#endif
#include "perf_kern.h"
_Static_assert(PFK_RAMP_LOG2 == SL_RAMP_LOG2 && PFK_SLOPE == SL_SLOPE && FX3_FRAMES == SL_FR, "the kernel's fade and loop are the SLICER's");
_Static_assert(BM_GLT + GX_GLT == BM_GLT && BM_GLT + GX_TAPE == BM_TAPE && BM_GLT + GX_SHUF == BM_SHUF && BM_GLT + GX_FRZ == BM_FRZ, "a ring effect's mode is BM_GLT + its place");

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

static struct {
    uint32_t ph, split;                /* samples since play; this block: where a 1/16 starts (the 1/16 itself: k.P) */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute;          /* sync: a 1/16 starts with the next block; mute: tracks to ramp */
    uint8_t mode, src, next;           /* BM_*, the effect it plays (PF_N none), one waiting for the fade */
    uint32_t len, rp;                  /* LOOP: loop length (samples), read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    fx3_st_t k;                        /* (Release 3 step 8b) the buffer effect's state as a kernel gets it (fx_master.h; it was P, state, odd, wr, w, hl, hr, yl, yr, gs, gph and u here; tw is a local now): the 1/16,
                                        * LOOP: state 0 recording, 1 playing; frames written; the buffer's share and its target; the ring effects' own. Handed to the effect's kernel, resident or the module's */
    uint32_t sw[2];                    /* the sweeps (Q24 progress: 0 dry), CRUSH, THROW */
    int32_t ss[2];                     /* .. on the S curve, Q15 */
    int32_t cw, chl, chr;              /* CRUSH share, held sample */
    uint32_t cn;
    int32_t td;                        /* THROW: the share of the dry mix sent, Q15 */
    int32_t mg[NTRK];                  /* mute gains, Q15 (32768 = open) */
} pf = {.src = PF_N, .next = PF_N, .mg = {[0 ... NTRK - 1] = 32768}};

/* ---- (Release 3 step 8b) the master host: a ring effect's kernel is the registered module's (fx3_modk[x], set by the loader through fx3_modk_set: pointers into the module's flash) or, with
 * none, the resident copy of the same text (perf_kern.h). Both get the same context: the state and the 32 KB are the host's, so a module that appears or goes away between two blocks changes
 * nothing that sounds. fx3_modk is written on CPU0 with the audio interrupt masked; the audio interrupt (CPU0: perf_block) is its only reader */
static __attribute__((section(".pool"))) fx3_kcalls_t fx3_modk[FX3_FX];       /* [GX_*]: the module's calls, 0: none registered */
static __attribute__((section(".pool"))) uint8_t fx3_mod_n, fx3_marked, fx3_mod_start;   /* effects the module serves; the crash mark taken for this registration; the loader's note: the start
                                                                                * of the module the calls point into (oc_module.c; here, beside the other two bytes, it costs no RAM) */
static __attribute__((section(".pool"))) void (*fx3_modk_note)(uint32_t module_id);   /* the loader's crash mark (oc_fxmod_note_use), set with the registration */
#if OC_PERF_RESIDENT
static const fx3_kcalls_t PERF_RESIDENT[FX3_FX] = { {PFK_CALLS_GLT}, {PFK_CALLS_TAPE}, {PFK_CALLS_SHUF}, {PFK_CALLS_FRZ} };
#define PERF_RESIDENT_OF(x) (&PERF_RESIDENT[x])
#else
#define PERF_RESIDENT_OF(x) ((const fx3_kcalls_t *)0)
#endif
/* the kernel of ring effect x (GX_*): the module's when one is registered, else the resident copy; 0: none (only without the resident copy: OC_PERF_RESIDENT 0) */
static inline __attribute__((always_inline)) const fx3_kcalls_t *perf_kern(uint32_t x) { return fx3_modk[x].run ? &fx3_modk[x] : PERF_RESIDENT_OF(x); }
/* the keys whose effect has a kernel: all of them while the resident copy is in the image */
static inline __attribute__((always_inline)) uint32_t perf_have(void)
{
#if OC_PERF_RESIDENT
    return ~0u;
#else
    return ~((fx3_modk[GX_GLT].run ? 0u : 1u << 3) | (fx3_modk[GX_TAPE].run ? 0u : 1u << 4) | (fx3_modk[GX_SHUF].run ? 0u : 1u << 15) | (fx3_modk[GX_FRZ].run ? 0u : 1u << 18));   /* (PF_GLT, PF_TAPE, PF_SHUF, PF_FRZ: asserted below) */
#endif
}
_Static_assert(PF_GLT == 3 && PF_TAPE == 4 && PF_SHUF == 15 && PF_FRZ == 18, "perf_have's bits");
/* ---- the registration: what the loader (oc_module.c oc_fx3mod_reg) and the host tests call ----
 * fx3_modk_set: the bank of module FX3_MODULE_ID, n entries (effect id = 1 + GX_*, its caps block, its call table), ALL OR NONE. Checked here, after the loader's fx_caps_check (the pointers,
 * the magic and the abi at install and at boot), for a caller that did not come through the loader and for what only the host knows:
 *   FX3M_VER    the caps block's magic, size, version or abi (FX3_ABI_ID: the layout of the context, the state, the 32 KB and the calls) is not this build's
 *   FX3M_RANGE  an effect id that is not 1 GLITCH, 2 TAPE STOP, 3 SHUFFLE or 4 FREEZE, or twice in the bank; more than one instance; a seed; a rate other than the master's; a state, an option
 *               or a hot set of its own (the state is the host's fx3_st_t); a working memory other than the 32 KB the host lends (8 blocks); a snapshot that is not the card's four bytes;
 *               a call missing
 * 0 FX3M_OK: from the next block on the registered effects run the module's code. The caller masks the audio interrupt in a running session */
#define OC_FX3_HOST 1                                   /* oc_module.c, editor.c: this build has the master host */
typedef struct { uint8_t effect_id; const fx_caps_t *caps; const fx3_kcalls_t *calls; } fx3_reg_t;
static int fx3_modk_set(uint32_t module_id, uint32_t version, uint32_t n, const fx3_reg_t *e)
{
    uint32_t i, seen = 0;
    if (module_id != FX3_MODULE_ID || !n || n > FX3_FX) return FX3M_RANGE;
    for (i = 0; i < n; i++) {
        const fx_caps_t *c = e[i].caps;
        uint32_t m = e[i].effect_id;
        if (!c || c->magic != FX_CAPS_MAGIC || c->size != FX_CAPS_WIRE || c->version != FX_CAPS_VERSION || c->abi != FX3_ABI_ID) return FX3M_VER;
        if (m < 1u || m > FX3_FX || ((seen >> m) & 1u)) return FX3M_RANGE;
        seen |= 1u << m;
        if (c->max_inst != 1u || (c->flags & FXK_SEED) || !(c->rates & 1u) || c->ram_state || c->ram_extra || c->ram_extra_bytes || c->hot || c->nhot ||
            c->ram_work_min != 8u || c->ram_work_max != 8u || c->param_bytes != 4u || !e[i].calls || !e[i].calls->start || !e[i].calls->run)
            return FX3M_RANGE;
    }
    for (i = 0; i < n; i++) fx3_modk[e[i].effect_id - 1u] = *e[i].calls;
    (void)version;
    fx3_mod_n = (uint8_t)n; fx3_marked = 0;
    return FX3M_OK;
}
/* the module goes (its sectors are about to be erased, or it no longer stands): the host's copy of the calls zeroed; from the next block on the resident copy plays, on the same state and
 * buffer (nothing to hand over, nothing that sounds changes). Without the resident copy (OC_PERF_RESIDENT 0) a ring effect that is playing is cut at the next block (perf_buf_gone) and its key
 * does nothing. The caller masks the audio interrupt, as for fx3_modk_set */
static void fx3_modk_clear(void)
{
    memset(fx3_modk, 0, sizeof fx3_modk);
    fx3_mod_n = 0; fx3_marked = 0;
}
static inline int fx3_modk_ok(void) { return fx3_mod_n != 0; }
/* the crash mark, taken once a registration, in the audio interrupt just before the module's code runs for the first time. In flash and not inlined (as fx2_mark: inlined, the loader's
 * oc_fxmod_note_use would follow it into SRAM and come off the audio arena) */
JFX_XIP static void fx3_mark(void)
{
    fx3_marked = 1;
    if (fx3_modk_note) fx3_modk_note(FX3_MODULE_ID);
}

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
    return m & perf_have();                               /* (step 8b) .. and a ring effect with no kernel (never, while the resident copy is in the image) */
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
static uint32_t perf_gfocus(void) { return perf_kill ? PF_N : perf_pick(perf_down & PF_GX & perf_have()); }   /* (step 8b: an effect with no kernel shows no card) */
static uint32_t perf_gx(uint32_t e);
static uint32_t perf_gx(uint32_t e) { return e == PF_GLT ? GX_GLT : e == PF_TAPE ? GX_TAPE : e == PF_SHUF ? GX_SHUF : GX_FRZ; }


/* what a ring effect's kernel gets with a call (fx_master.h): the state, the 32 KB, the card's knobs, the tempo and SCRATCH's count as this interrupt reads them */
static inline __attribute__((always_inline)) void perf_ctx(fx3_ctx_t *c, uint32_t x)
{
    c->st = &pf.k; c->buf = (fx3_buf_t *)sl_buf; c->gp = perf_gp[x]; c->beat = beat_samples(); c->scr = perf_scr;
    c->go = pf.src != PF_N; c->head = 0; c->semi = semi_q16;
}
static void perf_buf_start(uint32_t e)
{
    const fx3_kcalls_t *kn = 0;
    if (!sl_buf)                          /* (Orange Cat: no memory: nothing plays; jfx_full says so) */
        return;
    if ((PF_GX >> e) & 1u) {              /* (step 8b) a ring effect: its kernel, the module's or the resident copy */
        if ((kn = perf_kern(perf_gx(e))) == 0)
            return;                       /* (none: only without the resident copy, the module gone since the key was read: nothing plays, as with no memory) */
        if (kn != PERF_RESIDENT_OF(perf_gx(e)) && !fx3_marked)
            fx3_mark();                   /* the crash mark, before the module's first call */
    }
    if (!sl_lent) {
        sl_lent = 1;
        perf_drop_slicer();
    }
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.k.wr = pf.rp = 0;
    pf.k.odd = 0;
    pf.xn = 0;
    pf.mode = e == PF_GLT ? BM_GLT : e == PF_TAPE ? BM_TAPE : e == PF_SHUF ? BM_SHUF : e == PF_FRZ ? BM_FRZ : BM_LOOP;
    pf.k.state = 0;
    pf.len = perf_len(e);
    memset(&pf.k.u, 0, sizeof pf.k.u);
    memset(sl_buf[PB_FRAMES - 2u], 0, 2u * sizeof sl_buf[0]);   /* (what GLITCH / TAPE read before the start: quiet) */
    if (e == PF_GLT || e == PF_SHUF)      /* its first 1/16: the transport's (this block's split starts the next one) */
        pf.k.gs = song.playing && clk_pos != CLK_START ? clk_n + (pf.split ? 1u : 0u) : 0u;
    if (kn) {                             /* the effect's own start (perf_kern.h: GLITCH's and SHUFFLE's first step, TAPE's fall, FREEZE's eighth) */
        fx3_ctx_t c;
        perf_ctx(&c, perf_gx(e));
        kn->start(&c);
    }
}

/* the buffer effect to play becomes e (at a 1/16): a shorter REPEAT of what is recorded switches with a cross-fade,
 * anything else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.k.state && e <= PF_R32 && pf.src != PF_N &&
        perf_len(e) <= 2u * pf.k.wr) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.k.w) {
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
    pf.k.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    if (song.playing && clk_pos != CLK_START)       /* (JIANT) playing: the transport's 1/16, so a REPEAT starts on the grid */
        pf.k.P = div_samples(2);
    if (!pf.k.P) pf.k.P = 1u;
    ph0 = (song.playing && clk_pos != CLK_START ? clk_pos : pf.ph) % pf.k.P;   /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.k.P - ph0 < n ? pf.k.P - ph0 : n) : 0u;
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


/* (Orange Cat 0.6.0 rev 2) the buffer taken away (jfx.c jfx_sl_lost): the buffer effect out at once, as perf_buf does
 * when it finds no buffer (a new one may come back zeroed in the same block: it must not play on it) */
static void perf_buf_lost(void)
{
    pf.mode = BM_NONE;
    pf.next = PF_N;
    pf.k.w = 0;
    sl_lent = 0;
}
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
/* REPEAT on one sample: the share pf.k.w of the loop, the rest live */
static inline __attribute__((always_inline)) void perf_loop(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0, tw;
    if (!pf.k.state) {
        pf.k.wr = pb_rec(&pf.k, (fx3_buf_t *)sl_buf, pf.k.wr, l, r);
        if (++pf.rp >= pf.len) {
            pf.k.state = 1;
            pf.rp = 0;
        }
    } else {
        pb_loop(pf.rp, pf.len, pf.k.wr, &yl, &yr);
        if (pf.xn) {                            /* the loop it switched from, fading out */
            int32_t ol, orr, g = (int32_t)pf.xn;
            pb_loop(pf.xrp, pf.xlen, pf.k.wr, &ol, &orr);
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
    tw = on && pf.src != PF_N ? 32768 : 0;
    pf.k.w += clamp(tw - pf.k.w, -SL_SLOPE, SL_SLOPE);
    if (pf.k.w) {
        *pl = pf_mix(l, yl, pf.k.w);
        *pr = pf_mix(r, yr, pf.k.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or the buffer back */
        perf_buf_done();
    }
}
/* (step 8b) a ring effect is playing and its kernel is gone (only without the resident copy: the module removed under it): out at once, the buffer back */
static JFX_XIP void perf_buf_gone(void)
{
    pf.mode = BM_NONE;
    pf.next = PF_N;
    pf.k.w = 0;
    if (sl_lent) {
        sl_lent = 0;
        perf_drop_slicer();
    }
}
/* the buffer effect over the samples [i, end) of the block, in place; returns where it stopped (end, or earlier when the effect ended or changed: the caller goes on from there).
 * (step 8b) It was perf_buf, called a sample at a time; REPEAT still is (perf_loop), a ring effect is one call of its kernel's `run` for the whole run of samples */
static inline __attribute__((always_inline)) uint32_t perf_buf_run(int32_t *bl, int32_t *br, uint32_t i, uint32_t end)
{
    if (!sl_buf) {                                  /* (Orange Cat: the memory taken back: out at once) */
        pf.mode = BM_NONE;
        pf.k.w = 0;
        sl_lent = 0;
        return i;
    }
    if (pf.mode == BM_LOOP) {
        for (; i < end && pf.mode == BM_LOOP; i++)
            perf_loop(&bl[i], &br[i]);
        return i;
    }
    {
        uint32_t x = (pf.mode - BM_GLT) & 3u, d;
        const fx3_kcalls_t *kn = perf_kern(x);
        fx3_ctx_t c;
        if (!kn) {
            perf_buf_gone();
            return i;
        }
        if (kn != PERF_RESIDENT_OF(x) && !fx3_marked)
            fx3_mark();                             /* (a module registered while the resident copy played this effect: its first call is this one) */
        perf_ctx(&c, x);
        c.head = i == 0u;
        d = kn->run(&c, bl + i, br + i, end - i);
        if (!d || d > end - i)                      /* (a kernel answers 1 .. n; anything else would stall or overrun the block) */
            d = end - i;
        i += d;
        if (!pf.k.w && pf.src == PF_N)              /* faded out at its last sample: the next one, or the buffer back */
            perf_buf_done();
    }
    return i;
}
static void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* after the master level: the buffer effect, CRUSH on l / r (stereo, Q15).
 * (step 8b) The buffer effect runs over the block first (in runs: up to the 1/16's sample, then from it; a run ends early where an effect ends), CRUSH over the result after it: the old loop
 * did both on each sample in turn, and neither reads the other's state, so the samples are the same (tests/fx_layer_golden.txt, pinned from the old loop) */
static JFX_XIP void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0, sh, hold;
    int32_t ct = pf.ss[0];
    sh = 4u + (uint32_t)(ct * 7) / 32768u;
    hold = sh - 3u;
    if (pf.split)
        perf_switch(a);
    for (i = 0; i < n;) {
        uint32_t end = i < pf.split && pf.split < n ? pf.split : n;
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode == BM_NONE)
            i = end;
        else
            i = perf_buf_run(bl, br, i, end);
    }
    if (ct || pf.cw)                                /* (idle, the share stays 0 and nothing is written: not run) */
        for (i = 0; i < n; i++) {
            int32_t l = bl[i], r = br[i];
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
    pf.busy = pf.act || pf.mode != BM_NONE || pf.k.w || pf.cw || pf.td || pf.mute || pf.sw[0] || pf.sw[1];
}
/* the Effects table's `uses` (editor FX_TABLE): 1 while a ring effect plays and its kernel is the module's */
static uint32_t fx3_modk_uses(void) { uint32_t m = pf.mode; return m >= BM_GLT && fx3_modk[(m - BM_GLT) & 3u].run ? 1u : 0u; }
