/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Portions (the voice: rh_coef, rh_hermite, rh_hermite_u, rh_init, rh_load_pulse, rh_set_damper, rh_voice_sub, the pickup load of rh_render) from Frodhes
 * firmware/src/rhodes.c by Leonardo Mirabal (https://github.com/leonardoMirabal/frodhes, commit a8320e860c2307a6faa7266b9f59763dc3acb786), GPL-3.0-only.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Frodhes"): Frodhes' physically modelled Rhodes voice as a loadable module
 * translation unit, built only to measure it (stage 0: feasibility; no version, not offered by any firmware). The proposed engine number is 23
 * (core.h ENGI_RHODES, under OC_CANDIDATES).
 *
 * What is ported: rhodes.c's voice, unchanged in its arithmetic: the hammer pulse (two velocity layers, the contact rate), the five normal modes per key
 * (exact-pole coupled-form resonators with error feedback, the damper crossfade between the sustain and release poles), the pickup (the flux table through
 * a cubic Hermite, the flux difference), the steal fade, the felt noise and the damper thump at the key's release, the end of a voice (its peak under the
 * key's kill level, or its life), and the part's pickup load (the RLC low-pass and the TYPE's body biquads on the sum of the voices). The tables are
 * Frodhes' generated firmware/gen/rhodes_tables.h (tools/module/frodhes/, unchanged) and six knob tables of its fx_tables.h (rhodes_knobs.h).
 * What is left out: Frodhes' own voice allocation, event ring, pedal and overload shedding (Orange Cat's voice.c allocates, steals, holds the sustain pedal
 * and sheds), its effects (frodhes.c, fx.c: drive, tremolo, wah, chorus, phaser, delay, reverb, compressor), its UI and presets store, the TUNE setting
 * (the track has its own), and the bar engine (bars.c: VIBES and MARIMBA, the TYPEs 4 and 5 of Frodhes' app, which rhodes.c does not have).
 *
 * The changes the port needs, and only these: Frodhes' file-scope state (rv[], rh_x, rh_F, rh_mono, the RLC and body filters, the voicing volatiles, the
 * noise LFSR, the voice ages) is a module's arena block a track here (rh_part_t; a module has no .data or .bss); a voice is Orange Cat's voice lane
 * (v - t->v) % NPOLY; the voicing is read from the track's eight controls at each note-on as Frodhes applies its volatiles at each note-on; the key's
 * release is the voice's gate falling (voice.c holds the gate while the sustain pedal is down, so the damper weight is PEDAL_W[0], the pedal up). The voice's
 * pickup output goes into the part's mix (rh_part_t mix) at the voice's amplitude (voice.c's fades); post() runs the pickup load and adds the result to
 * the track at VOICE_FS. The voice's life counts blocks of 256 frames, as Frodhes' audio callback does (here eight renders of 32).
 *
 * The edit page: TYPE (ST77 SUIT FELT DYNO: rhodes.c's four RH_TYPE rows), TONE, BARK, BELL, HAMR, DCAY, REL, NOIS: Frodhes' own knobs and their maps
 * (its params.c param_apply). Presets: the four types at Frodhes' defaults and four of its factory presets' voicings (their effects are not ported).
 * The engine's own envelopes end the voice (ownenv, done). Correctness: tests/rhodes_parity_test.c renders Frodhes' rhodes.c itself (the vendored copy,
 * built as its host build builds it) and this port side by side, sample for sample.
 */
#define OC_MODULE 1
#define OC_MODULE_VERSION 0x00000100u    /* 0.1.0: a measurement, not a release */
#include "build_options.h"
#include <stdint.h>
#ifdef OC_MODULE_HOST
#define memcpy oc_mod_memcpy
#define memset oc_mod_memset
#endif
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n);
void *memset(void *d, int c, __SIZE_TYPE__ n);
#include "melodee_tables.h"
#include "core.h"
#include "dsp.c"
#include "fw_api.h"
#ifdef OC_MODULE_HOST
extern const fw_api_t *oc_host_fw;
#define OC_FW() (oc_host_fw)
#else
#define OC_FW() ((const fw_api_t *)FW_API_ADDR)
#endif
#include "oc_pack.h"
static inline const void *const *oc_base_hot(void) { return OC_FW()->base_hot; }
#define trk (OC_FW()->trk)
#define resource_get(id, n) (OC_FW()->resource_get((id), (n)))
void *memcpy(void *d, const void *s, __SIZE_TYPE__ n) { return OC_FW()->mem_copy(d, s, (uint32_t)n); }
void *memset(void *d, int c, __SIZE_TYPE__ n) { return OC_FW()->mem_set(d, c, (uint32_t)n); }

/* ---- Frodhes' types and tables (rhodes.c, unchanged) */
#include "frodhes/fxp.h"
typedef struct { int32_t sh, ch, th, d, drel, cr, b; } rh_mode_t;
typedef struct {
    rh_mode_t m[5];
    int32_t off, gain, kill, hc, pk;
    uint16_t prate, life;
    uint8_t band, fam, res, pad;
} rh_key_t;
typedef struct {
    int32_t off_delta;
    int32_t bark, bark_g;
    int32_t bell, ham;
    uint8_t nbody, drive_row;
} rh_type_t;
#define DECAY_K RH_DECAY_K               /* (Frodhes' DECAY_K: Orange Cat's melodee_tables.h has a table of that name) */
#include "frodhes/rhodes_tables.h"
#include "frodhes/rhodes_knobs.h"
#define RH_O1 2u
#define RH_O2 3u
#define SUB 32u
#define RAMP_SUB 7u
#define FADE_SAMPLES 64u
#define RH_BLOCK 256u                          /* Frodhes' audio block: a voice's life counts these */
#define Q30 (1 << 30)
#define RH_S_MAX_HI 2048u
static inline int32_t sat28i(int32_t v) { return v > ((1 << 28) - 1) ? ((1 << 28) - 1) : v < -((1 << 28) - 1) ? -((1 << 28) - 1) : v; }

typedef struct {
    int32_t x, y, ex, ey;
    int32_t e, eps;
    int32_t e0, eps0, e1, eps1;
    int32_t es, epss;
    int32_t b;
    int32_t peak;
    uint8_t live;
} rh_ms_t;
typedef struct {                               /* rhodes.c rh_voice_t without the steal's pending note (voice.c steals here) */
    rh_ms_t m[RH_NM];
    const rh_key_t *k;
    uint8_t note, vel, alive, held;
    uint8_t ramp;
    int32_t w_tgt;
    int32_t fade;
    uint32_t age, life;
    int32_t Hprev, off, gain;
    uint32_t ppos, prate;
    int32_t pamp;
    const int16_t *ptab, *ptab2;
    int32_t pmix;
    uint16_t felt_n;
    int32_t felt_lp;
    uint16_t frames;                           /* (the port) frames rendered since the last 256-frame block: the life counter's clock */
} rh_voice_t;
typedef struct {                               /* (the port) rhodes.c's file-scope state, a track's */
    rh_voice_t v[NPOLY];
    int32_t mix[CTL];                          /* rh_mono: the voices' sum for this block (block() clears it) */
    int32_t x[SUB], F[SUB];                    /* rh_x, rh_F */
    uint32_t ages, lfsr;
    int32_t rlc_m1, rlc_m2, rlc_o1, rlc_o2, rlc_e;
    int32_t bd_m[2][2], bd_o[2][2], bd_e[2];
} rh_part_t;
typedef struct {                               /* the voicing at a note-on (rhodes.c's volatiles, from the track's controls) */
    uint32_t type;
    int32_t tone_off, bark_s, bark_g, bell, ham, k_decay, k_rel, tune_t, thump, felt;
} rh_voicing_t;
static rh_part_t *rh_part(const track_t *t) { return resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(rh_part_t)); }
static rh_voice_t *rh_vc(rh_part_t *P, const track_t *t, const voice_t *v) { return &P->v[(uint32_t)(v - t->v) % NPOLY]; }

/* Frodhes params.c param_apply: the knobs (0..127) to the engine's voicing */
static void rh_voicing(const track_t *t, rh_voicing_t *q)
{
    const int16_t *p = t->p;
    q->type = (uint32_t)clamp(p[P_E0], 0, 3);
    q->tone_off = (clamp(p[P_E1], 0, 127) - 64) * (1 << 18);
    q->bark_s = (int32_t)KN_P64[clamp(p[P_E2], 0, 127)];
    q->bark_g = (int32_t)KN_P64I[clamp(p[P_E2], 0, 127)];
    q->bell = (int32_t)KN_P32[clamp(p[P_E3], 0, 127)];
    q->ham = (int32_t)KN_HAM[clamp(p[P_E4], 0, 127)];
    q->k_decay = (int32_t)DECAY_K[clamp(p[P_E5], 0, 127)];
    q->k_rel = (int32_t)DECAY_K[clamp(p[P_E6], 0, 127)];
    q->tune_t = 0;
    q->thump = KN_THUMP[clamp(p[P_E7], 0, 127)];
    q->felt = KN_FELT[clamp(p[P_E7], 0, 127)];
}

/* ---- rhodes.c, the voice (its arithmetic unchanged; the globals are the part's and the voicing's) */
static void rh_coef(const rh_mode_t *m, int32_t d30, int32_t t30, int32_t k16, int32_t *E, int32_t *eps)
{
    int32_t a = (int32_t)(((int64_t)k16 * (k16 - 65536)) >> 16);
    int32_t c = (int32_t)(((int64_t)a * (k16 - 131072)) >> 16);
    int32_t d2 = mul30(d30, d30), d3 = mul30(d2, d30);
    int64_t dp = (((int64_t)k16 * d30) >> 16) - ((((int64_t)a * d2) >> 16) >> 1)
               + (int32_t)(((int64_t)c * d3) >> 16) / 6;
    int32_t dpi = dp < 1 ? 1 : (int32_t)dp;
    int32_t dp2 = mul30(dpi, dpi), dp3 = mul30(dp2, dpi);
    int32_t q = Q30 - (dpi >> 2) - 3 * (dp2 >> 5);
    int32_t om = (dpi >> 1) + (dp2 >> 3) + (dp3 >> 4);
    int32_t dl = mul30(m->th, t30);
    int32_t dl2 = mul30(dl, dl), dl3 = mul30(dl2, dl), dl4 = mul30(dl2, dl2), dl5 = mul30(dl4, dl);
    int32_t cdl = Q30 - (dl2 >> 1) + dl4 / 24, sdl = dl - dl3 / 6 + dl5 / 120;
    int32_t sp = mul30(m->sh, cdl) + mul30(m->ch, sdl);
    int32_t cr = m->cr - 2 * mul30(m->cr, t30) + 3 * mul30(m->cr, mul30(t30, t30));
    int32_t corr = Q30 + mul16(mul30(om, om), cr);
    int32_t t1 = (int32_t)(((int64_t)q * sp) >> 29);
    *E = mul30(t1, corr);
    *eps = dpi;
}

static void rh_load_pulse(rh_voice_t *v, uint32_t vel, const rh_voicing_t *q)
{
    uint32_t key = (uint32_t)(v->k - RH_KEYS), i = RH_VPOS_I[vel], f = RH_VPOS_F[vel];
    const uint16_t *pa = RH_PAMP[key];
    const uint32_t *pr = RH_PRATE[key];
    v->ptab = RH_PULSE[key][RH_VLAY_I[vel]];
    v->ptab2 = RH_PULSE[key][RH_VLAY_I[vel] + 1u < RH_NLAYER ? RH_VLAY_I[vel] + 1u : RH_NLAYER - 1u];
    v->pmix = RH_VLAY_W[vel];
    v->pamp = (int32_t)pa[i] + ((((int32_t)pa[i + 1] - (int32_t)pa[i]) * (int32_t)f) >> 8);
    v->prate = (uint32_t)((int32_t)pr[i] + ((((int32_t)pr[i + 1] - (int32_t)pr[i]) * (int32_t)f) >> 8)) << 4;
    {
        uint32_t h = (uint32_t)(((uint64_t)(uint32_t)q->ham * (uint32_t)RH_TYPE[q->type & 3u].ham) >> 16);
        v->prate = (uint32_t)(((uint64_t)v->prate * h) >> 16);
        v->pamp = (int32_t)(((int64_t)v->pamp * h) >> 16);
        if (v->pamp > 32767) v->pamp = 32767;
    }
    v->ppos = 0;
}

static int32_t rh_hermite(const int32_t *P, const int32_t *M, int64_t s)
{
    uint32_t i, t;
    int32_t p0, p1, m0, m1, c2, c3, a;
    if (s < 0) s = 0;
    if ((uint64_t)s >= ((uint64_t)RH_S_MAX_HI << 44)) s = ((int64_t)RH_S_MAX_HI << 44) - 1;
    i = (uint32_t)(s >> 44);
    t = (uint32_t)(s >> 22) & 0x3FFFFFu;
    p0 = P[i]; p1 = P[i + 1]; m0 = M[i]; m1 = M[i + 1];
    c3 = 2 * (p0 - p1) + m0 + m1;
    c2 = 3 * (p1 - p0) - 2 * m0 - m1;
    a = c2 + (int32_t)(((int64_t)t * c3) >> 22);
    a = m0 + (int32_t)(((int64_t)t * a) >> 22);
    return p0 + (int32_t)(((int64_t)t * a) >> 22);
}

static inline __attribute__((always_inline)) int32_t rh_hermite_u(const int32_t *P, const int32_t *M, int32_t u)
{
    int64_t s = (int64_t)u * u;
    uint32_t i, t;
    int32_t p0, p1, m0, m1, c2, c3, a;
    if ((uint32_t)(s >> 32) >= (RH_S_MAX_HI << 12)) s = ((int64_t)RH_S_MAX_HI << 44) - 1;
    i = (uint32_t)(s >> 44);
    t = (uint32_t)(s >> 22) & 0x3FFFFFu;
    p0 = P[i]; p1 = P[i + 1]; m0 = M[i]; m1 = M[i + 1];
    c3 = 2 * (p0 - p1) + m0 + m1;
    c2 = 3 * (p1 - p0) - 2 * m0 - m1;
    a = c2 + (int32_t)(((int64_t)t * c3) >> 22);
    a = m0 + (int32_t)(((int64_t)t * a) >> 22);
    return p0 + (int32_t)(((int64_t)t * a) >> 22);
}

static void rh_init(rh_part_t *P, rh_voice_t *v, uint32_t note, uint32_t vel, const rh_voicing_t *q)
{
    const rh_type_t *ty = &RH_TYPE[q->type & 3u];
    uint32_t m, n = note < RH_KEY_LO ? RH_KEY_LO : note > RH_KEY_LO + RH_NKEY - 1u ? RH_KEY_LO + RH_NKEY - 1u : note;
    const rh_key_t *k = &RH_KEYS[n - RH_KEY_LO];
    int32_t u;
    v->k = k;
    for (m = 0; m < RH_NM; m++) {
        rh_ms_t *s = &v->m[m];
        const rh_mode_t *km = &k->m[m];
        s->x = s->y = s->ex = s->ey = 0;
        s->peak = 0;
        s->live = km->th != 0;
        if (!s->live) { s->e = s->eps = 0; continue; }
        rh_coef(km, km->d, q->tune_t, q->k_decay, &s->e0, &s->eps0);
        rh_coef(km, km->drel, q->tune_t, q->k_rel, &s->e1, &s->eps1);
        s->e = s->es = s->e0; s->eps = s->epss = s->eps0;
        {
            int64_t b = ((((int64_t)km->b * q->bark_s) >> 16) * ty->bark) >> 16;
            if (m == RH_O1 || m == RH_O2) b = (((b * ty->bell) >> 16) * q->bell) >> 16;
            s->b = b > 0x7FFFFFFF ? 0x7FFFFFFF : b < -0x7FFFFFFF ? -0x7FFFFFFF : (int32_t)b;
        }
    }
    v->note = (uint8_t)note; v->vel = (uint8_t)vel;
    v->off = k->off + ty->off_delta + q->tone_off;
    v->gain = mul16(mul16(k->gain, q->bark_g), ty->bark_g);
    u = v->off;
    v->Hprev = rh_hermite(RH_PHI[k->fam], RH_SLOPE[k->fam], (int64_t)u * u);
    v->alive = 1; v->held = 1; v->ramp = 0; v->w_tgt = 0;
    v->fade = 32768; v->felt_n = 0; v->felt_lp = 0;
    v->age = ++P->ages;
    v->life = k->life;
    v->frames = 0;
    rh_load_pulse(v, vel, q);
}

static void rh_set_damper(rh_voice_t *v, int32_t w)
{
    uint32_t m;
    if (w == v->w_tgt) return;
    for (m = 0; m < RH_NM; m++) { v->m[m].es = v->m[m].e; v->m[m].epss = v->m[m].eps; }
    v->w_tgt = w;
    v->ramp = RAMP_SUB;
}

/* one voice, one sub-block of n <= 32 frames, added into mix[0..n) at amplitude amp (Q15 of voice.c's ramp; 32768 = Frodhes' own level) */
static void rh_voice_sub(rh_part_t *P, rh_voice_t *v, int32_t *mix, uint32_t n, const vmod_t *amp, int32_t felt)
{
    const rh_key_t *k = v->k;
    const int32_t *Pt = RH_PHI[k->fam], *M = RH_SLOPE[k->fam];
    int32_t *rh_x = P->x, *rh_F = P->F;
    uint32_t j, m;
    int32_t sum = 0, Hp = v->Hprev, off = v->off, gain = v->gain, fnz = 0;
    if (v->ramp) {
        int32_t f;
        v->ramp--;
        f = (int32_t)(RAMP_SUB - v->ramp) * 4681;
        if (f > 32768) f = 32768;
        if (!v->ramp) f = 32768;
        for (m = 0; m < RH_NM; m++) {
            rh_ms_t *s = &v->m[m];
            int32_t et, pt;
            if (!s->live) continue;
            et = s->e0 + (int32_t)(((int64_t)(s->e1 - s->e0) * v->w_tgt) >> 15);
            pt = s->eps0 + (int32_t)(((int64_t)(s->eps1 - s->eps0) * v->w_tgt) >> 15);
            s->e = s->es + (int32_t)(((int64_t)(et - s->es) * f) >> 15);
            s->eps = s->epss + (int32_t)(((int64_t)(pt - s->epss) * f) >> 15);
        }
    }
    if (v->ptab) {
        const int16_t *pt = v->ptab, *pt2 = v->ptab2;
        int32_t pmix = v->pmix, pamp = v->pamp;
        uint32_t ppos = v->ppos, prate = v->prate;
        fnz = 1;
        for (j = 0; j < n; j++) {
            if (pt) {
                uint32_t i = ppos >> 16;
                int32_t f = (int32_t)((ppos >> 1) & 0x7FFFu);
                int32_t a = pt[i], b = pt[i + 1];
                if (pmix) {
                    a += ((pt2[i] - a) * pmix) >> 15;
                    b += ((pt2[i + 1] - b) * pmix) >> 15;
                }
                rh_F[j] = mul15s(a + (((b - a) * f) >> 15), pamp);
                ppos += prate;
                if ((ppos >> 16) >= RH_PLEN) pt = 0;
            } else
                rh_F[j] = 0;
            rh_x[j] = 0;
        }
        v->ppos = ppos; v->ptab = pt;
    } else
        for (j = 0; j < n; j++) rh_x[j] = 0;
    for (m = 0; m < RH_NM; m++) {
        rh_ms_t *s = &v->m[m];
        int32_t x, y, ex, ey, e, eps, b, pk = 0;
        if (!s->live) continue;
        x = s->x; y = s->y; ex = s->ex; ey = s->ey; e = s->e; eps = s->eps; b = s->b;
        if (fnz)
            for (j = 0; j < n; j++) {
                int64_t t;
                if (rh_F[j]) y += (int32_t)(((int64_t)rh_F[j] * b) >> 20);
                t = (int64_t)e * y + ex;
                x += (int32_t)(t >> 30); ex = (int32_t)(t & 0x3FFFFFFF);
                t = ey - (int64_t)eps * y - (int64_t)e * x;
                y += (int32_t)(t >> 30); ey = (int32_t)(t & 0x3FFFFFFF);
                rh_x[j] += x;
                if (iabs(x) > pk) pk = iabs(x);
                if (iabs(y) > pk) pk = iabs(y);
            }
        else
            for (j = 0; j < n; j++) {
                int64_t t;
                t = (int64_t)e * y + ex;
                x += (int32_t)(t >> 30); ex = (int32_t)(t & 0x3FFFFFFF);
                t = ey - (int64_t)eps * y - (int64_t)e * x;
                y += (int32_t)(t >> 30); ey = (int32_t)(t & 0x3FFFFFFF);
                rh_x[j] += x;
                if (iabs(x) > pk) pk = iabs(x);
                if (iabs(y) > pk) pk = iabs(y);
            }
        s->x = sat28i(x); s->y = sat28i(y); s->ex = ex; s->ey = ey; s->peak = pk;
        sum += pk >> 3;
    }
    if (v->fade == 32768 && !v->felt_n)
        for (j = 0; j < n; j++) {
            int32_t H = rh_hermite_u(Pt, M, rh_x[j] + off);
            mix[j] += (int32_t)(((int64_t)((int32_t)(((int64_t)(H - Hp) * gain) >> 24)) * amp_at(amp, j)) >> 15);
            Hp = H;
        }
    else
    for (j = 0; j < n; j++) {
        int32_t H = rh_hermite_u(Pt, M, rh_x[j] + off);
        int32_t o = (int32_t)(((int64_t)(H - Hp) * gain) >> 24);
        Hp = H;
        if (v->fade != 32768) {
            o = (int32_t)(((int64_t)o * v->fade) >> 15);
            v->fade -= (int32_t)(32768 / FADE_SAMPLES);
            if (v->fade < 0) v->fade = 0;
        }
        if (v->felt_n) {
            int32_t nz;
            P->lfsr = (P->lfsr >> 1) ^ (-(P->lfsr & 1u) & 0xB400u);
            nz = (int32_t)(P->lfsr & 0xFFFFu) - 32768;
            v->felt_lp += (nz - v->felt_lp) >> 3;
            o += (int32_t)(((int64_t)v->felt_lp * felt) >> 10);
            v->felt_n--;
        }
        mix[j] += (int32_t)(((int64_t)o * amp_at(amp, j)) >> 15);
    }
    v->Hprev = Hp;
    if (v->fade == 0) { v->alive = 0; return; }
    if (!v->ptab && !v->felt_n && sum < (k->kill >> 3)) { v->alive = 0; return; }
    for (m = 1; m < RH_NM; m++)
        if (v->m[m].live && !v->ptab && v->m[m].peak < (k->kill >> 3)) {
            v->m[m].live = 0; v->m[m].x = v->m[m].y = v->m[m].ex = v->m[m].ey = 0;
        }
}

/* ---- the engine's callbacks */
static void rhm_note_on(track_t *t, voice_t *v)
{
    rh_part_t *P = rh_part(t);
    rh_voice_t *V;
    rh_voicing_t q;
    uint32_t vel = v->vel ? v->vel : 1u;
    if (!P) return;
    if (!P->lfsr) P->lfsr = 0xACE1u;
    V = rh_vc(P, t, v);
    rh_voicing(t, &q);
    if (V->alive && V->note == v->note && V->fade == 32768) {   /* rhodes.c rh_strike: the same note still sounding: re-strike, superpose */
        uint32_t m;
        for (m = 0; m < RH_NM; m++) if (V->k->m[m].th) V->m[m].live = 1;
        V->vel = (uint8_t)vel; V->held = 1; V->age = ++P->ages; V->life = V->k->life;
        rh_set_damper(V, 0);
        rh_load_pulse(V, vel, &q);
        return;
    }
    rh_init(P, V, v->note, vel, &q);
}
static void rhm_block(track_t *t)
{
    rh_part_t *P = rh_part(t);
    if (P) memset(P->mix, 0, sizeof P->mix);
}
static void rhm_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    rh_part_t *P = rh_part(t);
    rh_voice_t *V;
    (void)out;                                       /* the voices sum into the part's mix; post() adds it to the track */
    if (!P || !n) return;
    V = rh_vc(P, t, v);
    if (n > CTL) n = CTL;
    if (!V->alive) return;
    if (V->held && !v->gate) {                       /* rhodes.c rh_events, EV_NOTE_OFF (the pedal is voice.c's: the gate stays up under it, so the
                                                      * weight is rh_pedal_w's own start value, the full felt, as before any pedal event) */
        int32_t pw = 32768;
        V->held = 0;
        rh_set_damper(V, pw);
        {
            rh_voicing_t q;
            rh_voicing(t, &q);
            if (pw && q.thump && !V->ptab) {
                uint32_t key = (uint32_t)(V->k - RH_KEYS);
                V->ptab = RH_PULSE[key][0];
                V->pmix = 0;
                V->pamp = q.thump > 32767 ? 32767 : q.thump;
                V->prate = (uint32_t)RH_PRATE[key][1] << 4;
                V->ppos = 0;
            }
            if (pw && q.felt) V->felt_n = 353;
        }
    }
    rh_voice_sub(P, V, P->mix, n, m, KN_FELT[clamp(t->p[P_E7], 0, 127)]);
    V->frames = (uint16_t)(V->frames + n);
    if (V->frames >= RH_BLOCK) {                     /* rhodes.c rh_render, once a block of 256: the voice's life */
        V->frames = (uint16_t)(V->frames - RH_BLOCK);
        if (V->alive && V->life && --V->life == 0 && V->fade == 32768) V->fade = 32767;
    }
}
static int rhm_done(track_t *t, voice_t *v)
{
    rh_part_t *P = rh_part(t);
    return !P || !rh_vc(P, t, v)->alive;
}
/* rhodes.c rh_render's pickup load on the part's mix: the RLC low-pass and the TYPE's body biquads (Q28, error feedback), Q20 -> Q15, into the track */
static void rhm_post(track_t *t, int32_t *out, uint32_t n, uint32_t nr)
{
    rh_part_t *P = rh_part(t);
    uint32_t ty = (uint32_t)clamp(t->p[P_E0], 0, 3), nb = RH_TYPE[ty].nbody, j, sb, act = 0;
    const int32_t *c = RH_RLC[ty];
    int32_t mx = 0;
    if (!P) return;
    for (j = 0; j < NVOICE; j++)                     /* rhodes.c's act: a voice still alive after this block's renders */
        act |= t->v[j].active && P->v[j % NPOLY].alive;
    if (n > CTL) n = CTL;
    for (j = 0; j < n; j++) {
        int32_t m = nr ? P->mix[j] : 0, o;
        int64_t acc = (int64_t)c[0] * m + (int64_t)c[1] * P->rlc_m1 + (int64_t)c[2] * P->rlc_m2
                    - (int64_t)c[3] * P->rlc_o1 - (int64_t)c[4] * P->rlc_o2 + P->rlc_e;
        o = (int32_t)(acc >> 28); P->rlc_e = (int32_t)(acc & 0xFFFFFFF);
        P->rlc_m2 = P->rlc_m1; P->rlc_m1 = m; P->rlc_o2 = P->rlc_o1; P->rlc_o1 = o;
        for (sb = 0; sb < nb; sb++) {
            const int32_t *b = RH_BODY[ty][sb];
            int64_t a2 = (int64_t)b[0] * o + (int64_t)b[1] * P->bd_m[sb][0] + (int64_t)b[2] * P->bd_m[sb][1]
                       - (int64_t)b[3] * P->bd_o[sb][0] - (int64_t)b[4] * P->bd_o[sb][1] + P->bd_e[sb];
            int32_t y = (int32_t)(a2 >> 28);
            P->bd_e[sb] = (int32_t)(a2 & 0xFFFFFFF);
            P->bd_m[sb][1] = P->bd_m[sb][0]; P->bd_m[sb][0] = o; P->bd_o[sb][1] = P->bd_o[sb][0]; P->bd_o[sb][0] = y;
            o = y;
        }
        o = (o + 16) >> 5;
        if (iabs(o) > mx) mx = iabs(o);
        P->mix[j] = o;                                /* (the port: the Q15 mono, for the parity test) */
        out[j] += mulq15(clamp(o, -32767, 32767), VOICE_FS);
    }
    if (!act && mx < 2) {                             /* exact silence once everything has ended */
        P->rlc_m1 = P->rlc_m2 = P->rlc_o1 = P->rlc_o2 = P->rlc_e = 0;
        for (sb = 0; sb < 2u; sb++) P->bd_m[sb][0] = P->bd_m[sb][1] = P->bd_o[sb][0] = P->bd_o[sb][1] = P->bd_e[sb] = 0;
    }
}

static const char *const N_RH_TYPE[] = {"ST77", "SUIT", "FELT", "DYNO"};
static const preset_t RH_PRESETS[] = {
    /* name, {TYPE, TONE, BARK, BELL, HAMR, DCAY, REL, NOIS}, {A D S R} (unused: the engine's own envelopes), fenv, mono */
    {"STAGE 77", {0, 64, 64, 64, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},
    {"SUITCASE", {1, 64, 64, 64, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},
    {"EARLY FELT", {2, 64, 64, 64, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},
    {"BELL DYNO", {3, 64, 64, 64, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},
    {"BARK", {0, 64, 100, 64, 90, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},           /* Frodhes F08 */
    {"80S BALLAD", {2, 64, 64, 80, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},      /* F04 */
    {"DREAM", {2, 64, 64, 64, 64, 80, 64, 48}, {0, 64, 127, 40}, 0, 0},           /* F10 */
    {"RADIO", {3, 94, 64, 64, 64, 64, 64, 48}, {0, 64, 127, 40}, 0, 0},           /* F11 */
};
static const engine_t ENG_RHODES = {
    .name = "RHODES",
    .page_title = {"TINE", "DAMPER"},
    .edit = {
        {"TYPE", F_ENUM, 0, 3, 0, N_RH_TYPE, 0},
        {"TONE", F_INT, 0, 127, 64, 0, 0},
        {"BARK", F_INT, 0, 127, 64, 0, 0},
        {"BELL", F_INT, 0, 127, 64, 0, 0},
        {"HAMR", F_INT, 0, 127, 64, 0, 0},
        {"DCAY", F_INT, 0, 127, 64, 0, 0},
        {"REL", F_INT, 0, 127, 64, 0, 0},
        {"NOIS", F_INT, 0, 127, 48, 0, 0},
    },
    .presets = RH_PRESETS,
    .npresets = NELEM(RH_PRESETS),
    .knob = {P_E0, P_E1, P_E2, P_E5},
    .poly = NPOLY,
    .note_on = rhm_note_on,
    .render = rhm_render,
    .block = rhm_block,
    .post = rhm_post,
    .ownenv = 1,
    .done = rhm_done,
};

#ifdef OC_MODULE_HOST
const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_RHODES, sizeof(rh_part_t), 0, 0, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
const fw_api_t *oc_host_fw;
const engine_t *oc_host_module_engine(void) { return &ENG_RHODES; }
const int32_t *oc_host_rhodes_mono(const track_t *t) { return rh_part(t)->mix; }   /* the parity test: the Q15 mono of the last post() */
#else
extern char _modstart[];
__attribute__((section(".modhdr"), used)) const fw_mod_hdr_t OC_MODULE_HDR = {
    FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION_BASE, FW_API_ADDR,
    ENGI_RHODES, sizeof(rh_part_t), (uint32_t)(uintptr_t)&ENG_RHODES, (uint32_t)(uintptr_t)_modstart, OC_PACK_HASH,
    0, OC_MODULE_VERSION, 0, 0, 0,
};
#endif
