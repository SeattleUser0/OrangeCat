/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Portions Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 3 step 8a, FX_MODULES_PLAN.md 16) The reverb modes SPRING, SHIMMER and RESO: ONE text that the image's resident copy (jfx.c) and the effect module (tools/module/reverb_mod.c,
 * orangecat-reverb.ocm: kind 2, one bank of three) both include. It was jfx.c's rev_spring, rev_spring_side, rev_shimmer and rev_reso (Jiant's DSP, its comments kept), cut out word for word
 * but for where a value comes from: every state, buffer, parameter and service is read through the context of the call (fx_send.h fx2_ctx_t) where the old text named jfx.c's statics.
 * tests/fx_reverb_module.sh proves the cut changed no sample of the resident render (the printout pinned before it) and that the module's render is the resident's.
 *
 * The includer defines, before this text: REVK_FN (the placement of a mode's function: .text.xip_jfx in the image, nothing in the module), the types and tables of core.h, clamp, mulq15,
 * softclip and osc_sine (dsp.c), pitch_inc (melodee_tables.h; the module's reads the firmware's PITCH_OCT through fw_api_t), fx_send.h. With REVK_NO_MODES defined only the first part is
 * taken (the lines' scale and knee, AIR's and FOLW's reading of the knobs: what the resident ROOM and CLOUD and the host use too). */

/* (0.9.14) the reverbs' int16 lines written through a soft knee: linear to -2.5 dBFS, then tanh up to ~-0.1 dB */
static inline int32_t rv_knee(int32_t x)
{
    int32_t a = x < 0 ? -x : x;
    if (a <= 24576)
        return x;
    a = 24576 + (softclip(clamp(a - 24576, 0, 16384) * 4) >> 2);
    return x < 0 ? -a : a;
}
/* (round 11) the reverbs' 16-bit lines held JFX_ROOM_HEAD / JFX_SPRING_HEAD bits higher than the input scale of before (ROOM and SPRING: their input 1/8 at -4 dB,
 * mulq15 2580, and the lines' quantization under it was the hiss the owner heard under a quiet note: a floor 6 dB (ROOM) and 10 dB (SPRING) over
 * the dry one). The lines are written through rv_knee (linear to -2.5 dBFS of the line, then soft) instead of a hard clamp, and the outputs are
 * scaled back, so the sound is the one of before wherever the line stays under the knee; a line that reaches it saturates softly where it
 * clamped hard (RESO, SHIMMER, CLOUD: their own code, unchanged). 0: the lines as before */
#ifndef JFX_ROOM_HEAD
#define JFX_ROOM_HEAD 1                                  /* ROOM's combs (and SHIMMER's, which runs ROOM) */
#endif
#ifndef JFX_ROOM_APSH
#define JFX_ROOM_APSH 1                                  /* ROOM's allpass lines held this many bits under the combs (they carry the four combs' sum and
                                                          * reach the knee first: at 1 the headroom of before is nearly whole and 3.4 of the 4.5 dB stay) */
#endif
#ifndef JFX_SPRING_HEAD
#define JFX_SPRING_HEAD 2                                /* SPRING's loop */
#endif
#define RV_IN (2580 << JFX_ROOM_HEAD)
#define SP_IN (2580 << JFX_SPRING_HEAD)
/* the bus input into the lines' scale: the send is bounded by four tracks' contributions of 65,536 each (mix.c) and THROW's dry mix, so it is
 * clamped at four full-scale tracks before the multiply (the product would wrap above 416k at RV_IN, 208k at SP_IN; the review of round 11) */
#define RV_SEND(x) clamp((x), -131072, 131072)
/* a line write: as it is under the knee (no call), rv_knee above (soft to 32,473: the int16 holds it); the review: the clamp that stood before
 * rv_knee made the knee a hard clip at 30,814, and rv_knee was a call on every write */
static inline int32_t rv_put(int32_t v) { return (uint32_t)(v + 24576) <= 49152u ? v : rv_knee(v); }
/* (0.6.12) AIR: a semitone count as a speed, Q16: 2^(st / 12) (-24 .. 36) */
static uint32_t semi_q16(int32_t st)
{
    static const uint16_t R[12] = {32768, 34716, 36781, 38968, 41285, 43740, 46341, 49097, 52016, 55109, 58386, 61858};
    int32_t o = (st + 48) / 12 - 4, k = (st + 48) % 12;   /* (an octave down, Q15: then shifted) */
    uint32_t r = (uint32_t)R[k] << 1;                   /* 2^(k / 12), Q16 */
    return o >= 0 ? r << o : r >> -o;
}

/* AIR (SHIMMER, CLOUD: the PRE knob, -24 .. 24 semitones) and RESO's two knobs of its own, read from the snapshot g (song.g as the bus job has it) */
static int32_t revk_air(const int16_t *g)
{
    return g[G_RTYPE] == 2 || g[G_RTYPE] == 4 ? clamp(g[G_RPRE], -24, 24) : 0;
}
/* (JIANT 0.6.5) RESO's two knobs of its own, in the slots of PRE and DAMP: PITCH (G_RPRE 0..14) the chord that many
 * steps of the scale up; FOLW (G_RDAMP 0..4, Orange Cat's four tracks) OFF / T1..T4: the chord built on the lowest
 * note that track sounds (held after it stops), so it moves with the sequence. The strings glide there */
static uint32_t revk_pitch(const int16_t *g) { return g[G_RTYPE] == 3 ? (uint32_t)clamp(g[G_RPRE], 0, 14) : 0u; }
static uint32_t revk_folw(const int16_t *g) { return g[G_RDAMP] >= 1 && g[G_RDAMP] <= (int16_t)NTRK ? (uint32_t)g[G_RDAMP] : 0u; }

#ifndef REVK_NO_MODES
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
_Static_assert(sizeof(((rev_tank_t *)0)->comb) / sizeof(int16_t) > SP_LEN && sizeof(((rev_tank_t *)0)->u.sp) / sizeof(int32_t) >= 4u * (SP_N + 1u), "SPRING in ROOM's buffers");
/* (0.6.70) SPRING's chain shortened / lengthened: the stages coming back start silent. (Orange Cat: no load governor,
 * always SP_N stages: only the first block sets it) */
static void sp_chain(const fx2_ctx_t *kc, int32_t *ap, uint32_t ns)
{
    uint32_t j, k;
    for (j = 0; j < 4u; j++)
        for (k = ns + 1u; k <= SP_N; k++)
            ap[j * (SP_N + 1u) + k] = 0;
    kc->st->sp_ns = (uint8_t)ns;
}
/* SPRING (see above), added to out */
REVK_FN static void revk_spring(const fx2_ctx_t *kc, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, s = (uint32_t)clamp(kc->g[G_RSIZE], 0, 127);
    int32_t g = 19661 + (int32_t)s * 85 + (int32_t)(s * s) / 16;   /* the loop's gain: 0.6 .. 0.96 */
    int32_t kl = 26000 - clamp(kc->g[G_RDAMP], 0, 127) * 160;     /* its low-pass: ~9 kHz .. ~1.3 kHz */
    int32_t len = (int32_t)(1323u + ((s * 1323u) >> 7)) << 8, L, L2, L3, f, w, rm = clamp(kc->g[G_RMOD], 0, 127);
    int16_t *ln = kc->tank->comb;
    int32_t *ap = kc->tank->u.sp;
    uint32_t ns = SP_N;
    if (!kc->st->sp_size)
        kc->st->sp_size = len;
    kc->st->sp_size += clamp(len - kc->st->sp_size, -256, 256);   /* SIZE glides (a sample a block at most) */
    L = kc->st->sp_size >> 8;
    kc->st->sp_ph += 2u * kc->lfo_inc[rm ? kc->g[G_RRATE] & 127 : 24];   /* the wobble: a slow sine, 1.5 samples */
    w = (kc->st->sp_size >> 1) + ((osc_sine(kc->st->sp_ph) * (3 + rm / 2)) >> 8);   /* deep (MOD: up to 66); the far end, Q8 */
    L2 = w >> 8;
    f = w & 255;
    L3 = (L * 3) >> 2;
    if (ns != kc->st->sp_ns)
        sp_chain(kc, ap, ns);
    for (i = 0; i < n; i++) {
        uint32_t wp = kc->st->sp_w, j = (wp & 3u) * (SP_N + 1u);
        int32_t x = mulq15(RV_SEND(rev_in[i]), SP_IN), r = ln[(wp - (uint32_t)L) & SP_MASK], p, o;
        int32_t t0 = ln[(wp - (uint32_t)L2) & SP_MASK], t1 = ln[(wp - (uint32_t)L2 - 1u) & SP_MASK];
        kc->st->sp_lp += mulq15(r - kc->st->sp_lp, kl);
        o = kc->st->sp_lp * g;
        x += (o + ((o >> 31) & 32767)) >> 15;           /* towards 0: a loop of floors would hold an offset */
        o = x - kc->st->sp_hp + kc->st->sp_he;                    /* the low cut, its step's remainder kept */
        kc->st->sp_he = o & 63;
        kc->st->sp_hp += o >> 6;
        x -= kc->st->sp_hp;
        p = ap[j];                                      /* the chain: ap[j + k], stage k's output 4 samples ago */
        ap[j] = x;
        for (k = 1; k <= ns; k++) {                     /* (lossless: bounded by the loop's input, no clamp) */
            int32_t v = (x - ap[j + k]) * SP_A;
            o = ap[j + k];
            x = ((v + ((v >> 31) & 4095)) >> 12) + p;
            p = o;
            ap[j + k] = x;
        }
        ln[wp & SP_MASK] = (int16_t)rv_put(x);
        kc->st->sp_w = (uint16_t)(wp + 1u);
        out[i] += ((t0 + (((t1 - t0) * f) >> 8)) * 4 + ln[(wp - (uint32_t)L3) & SP_MASK] * 2) >> JFX_SPRING_HEAD;
    }
}
/* (JIANT 0.3) SPRING's WIDE, after its block: a tap a quarter along the loop against 3/4 as the side */
REVK_FN static void revk_spring_side(const fx2_ctx_t *kc, uint32_t n, int32_t wd)
{
    const int16_t *ln = kc->tank->comb;
    uint32_t i, L = (uint32_t)kc->st->sp_size >> 8, L3 = (L * 3u) >> 2, wp = (uint32_t)kc->st->sp_w - n;
    for (i = 0; i < n; i++, wp++) {
        int32_t sd = ((ln[(wp - L3) & SP_MASK] - ln[(wp - (L >> 2)) & SP_MASK]) * 3 * wd) >> (7 + JFX_SPRING_HEAD);
        kc->sl[i] += sd;
        kc->sr[i] -= sd;
    }
}


REVK_FN static void revk_shimmer(const fx2_ctx_t *kc, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t in[CTL], t[CTL], amt = clamp(kc->g[G_RMOD], 0, 127) * 190;   /* Q15: up to ~0.74 */
    int32_t air = revk_air(kc->g), st = clamp(12 + air, -12, 24), tk = air < 0 ? 32767 + air * 1150 : 9000;
    int32_t step = (int32_t)semi_q16(st) - 65536;       /* (the delay changes by 1 - speed a sample, Q16) */
    uint32_t i, j;
    for (i = 0; i < n; i++) {
        in[i] = rev_in[i] + mulq15(kc->fb[i], amt);
        t[i] = 0;
    }
    kc->room(in, t, n);
    for (i = 0; i < n; i++) {
        int32_t x = 0, y = t[i];
        if (air) {                                      /* AIR: down a low-pass (dark), up a high shelf (air) */
            kc->st->sh_tlp += mulq15(y - kc->st->sh_tlp, tk);
            y = air < 0 ? kc->st->sh_tlp : y + (((y - kc->st->sh_tlp) >> 3) * air) / 3;
        }
        out[i] += y;
        kc->line[0][kc->st->sh_w & (SH_LEN - 1u)] = (int16_t)clamp(y >> 1, -32768, 32767);
        for (j = 0; j < 2u; j++) {                      /* two grains, the delay sliding: read at octave + AIR's speed */
            uint32_t q = kc->st->sh_ph + j * ((SH_LEN / 2u) << 16), ph = (q >> 16) & (SH_LEN - 1u), d = SH_LEN - 1u - ph;
            int32_t w = (int32_t)(ph < SH_LEN / 2u ? ph : SH_LEN - 1u - ph);   /* the triangles: their sum flat */
            int32_t f = (int32_t)((q >> 8) & 255u), x0 = kc->line[0][(kc->st->sh_w - d) & (SH_LEN - 1u)];
            int32_t x1 = kc->line[0][(kc->st->sh_w - d + 1u) & (SH_LEN - 1u)];
            x += ((x0 + (((x1 - x0) * f) >> 8)) * w) >> 9;
        }
        kc->st->sh_ph += (uint32_t)step;
        kc->st->sh_w++;
        kc->fb[i] = x;                                 /* (into the next block's input) */
    }
}
#define RS_LEN 1024u                                     /* each comb's line, in rev_comb: down to ~43 Hz */
_Static_assert(sizeof(((rev_tank_t *)0)->comb) / sizeof(int16_t) > 4u * RS_LEN, "RESO in ROOM's buffers");
REVK_FN static void revk_reso(const fx2_ctx_t *kc, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    uint32_t i, k, per[4], mask, s, fol = revk_folw(kc->g), up = revk_pitch(kc->g);
    int32_t root = kc->reso_root;
    int32_t sz = clamp(kc->g[G_RSIZE], 0, 127), dk = 32767 - 60 * 200, note[4], deg = 0, nn;
    int32_t fb = sz <= 100 ? 26000 + sz * 52 : clamp(31200 + (sz - 100) * 58, 0, 32740);   /* (0.5.1: the top of SIZE
                                                         * rings on for tens of seconds) */
    mask = kc->reso_mask;                                 /* (the copy: scale_mask of the first melodic track, 0 -> all) */
    nn = 48 + root;
    if (fol) {                                           /* FOLW: the lowest note the track sounds */
        uint32_t lo = kc->reso_lo;
        if (lo != 0xFFu)
            kc->st->rs_root = (uint8_t)(lo + 1u);
        if (kc->st->rs_root) {
            nn = 48 + (int32_t)(kc->st->rs_root - 1u) % 12;          /* (its pitch class from C3, down onto the scale) */
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
    if (kc->g[G_RMOD] > 64) { note[2] += 12; note[3] += 12; }   /* MOD: the voicing open */
    for (k = 0; k < 4u; k++) {
        uint32_t inc = pitch_inc((uint32_t)clamp(note[k] * 16, 0, 2047));
        per[k] = 0xFFFFFFFFu / ((inc >> 8) | 1u);          /* the period, Q8 */
        if (per[k] > (RS_LEN - 2u) << 8) per[k] = (RS_LEN - 2u) << 8;
        if (!kc->st->rs_per[k])
            kc->st->rs_per[k] = per[k];
        kc->st->rs_per[k] += (uint32_t)(((int32_t)per[k] - (int32_t)kc->st->rs_per[k]) / 6);   /* (a glide, ~6 blocks) */
        per[k] = kc->st->rs_per[k];
    }
    for (i = 0; i < n; i++) {
        int32_t in = mulq15(rev_in[i], 3600), a = 0;
        for (k = 0; k < 4u; k++) {
            int16_t *c = kc->tank->comb + k * RS_LEN;
            uint32_t d = per[k] >> 8, f = per[k] & 255u;
            int32_t x0 = c[(kc->st->rs_w - d) & (RS_LEN - 1u)], x1 = c[(kc->st->rs_w - d - 1u) & (RS_LEN - 1u)];
            int32_t o = x0 + (((x1 - x0) * (int32_t)f) >> 8);
            kc->st->rs_lp[k] = o + mulq15(kc->st->rs_lp[k] - o, 32767 - dk);
            c[kc->st->rs_w & (RS_LEN - 1u)] = (int16_t)clamp(in + mulq15(kc->st->rs_lp[k], fb), -32768, 32767);
            a += o;
        }
        kc->st->rs_w++;
        out[i] += a;
    }
}

/* ---- the calls (fx2_kcalls_t.process), one a mode: what rev_run did for it ---- */
REVK_FN static void revk_spring_run(const fx2_ctx_t *kc, const int32_t *rev_in, int32_t *out, uint32_t n)
{
    int32_t wd = clamp(kc->g[G_RWIDE], 0, 127);
    revk_spring(kc, rev_in, out, n);
    if (wd) revk_spring_side(kc, n, wd);
}
#define REVK_CALLS_SPRING revk_spring_run
#define REVK_CALLS_SHIMMER revk_shimmer
#define REVK_CALLS_RESO revk_reso
/* the capability words of the three (fx_caps_t; the module's descriptor and the test read them here) */
#define REVK_MODULE_ID FX2_MODULE_ID                    /* 2: docs/orangecat/MODULE_IDS.md */
#define REVK_VERSION 0x00010000u                        /* 1.0.0 */
#define REVK_LEGACY 0x52455632u                         /* "REV2": the Release 2 key (provisional, as DUB2): the values sit in song.g's legacy slots (the pages' pid table, FW_LEG_GLOBAL) */
#define REVK_FLAGS 8u                                   /* FXK_TAIL_SILENT: the bus runs until it is silent (the host's gate and rest); no seed; the cost in the engine unit (0: not measured) */
#define REVK_RATES 1u                                   /* 44.1 kHz: the bus runs at the full rate (the model line is written at the rate the mode chooses) */
#define REVK_LINE_SPRING 0u                             /* the model line's blocks the mode needs to run (ram_work_min = ram_work_max): SPRING and RESO none of their own, SHIMMER its 4 KB line */
#define REVK_LINE_SHIMMER 1u
#define REVK_LINE_RESO 0u
#endif
