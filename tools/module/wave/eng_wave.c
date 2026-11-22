/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * WAVE (engine 32): Orange Cat's own wavetable engine (docs/candidates/wave.md: the design, the owner's answers of November 21: a filter per voice, 256-sample
 * frames, user tables in the sample slot's room, a sound kept as a user preset, four matrix slots, stereo behind OC_STEREO). Included by tools/module/wave_mod.c.
 *
 * A voice: two wavetable oscillators (a table each, its position, a warp: BEND, SYNC or MIRROR, 1 to 8 unison copies, octave and semitone), a sub (a sine an
 * octave under A) and noise, a filter (TPT state-variable LP, BP, HP, NOTCH, or a four-pole ladder), ENV 2 (an ADSR for the matrix; the amplitude is
 * voice.c's ADSR, the track's ENV page), two LFOs (triangle, saw, square, sample and hold; free or retriggered), four matrix slots (ENV 2, LFO 1, LFO 2,
 * velocity, key into the positions, warps, cutoff, resonance, pitch, levels), unison detune.
 *
 *   the eight edit values (the playing layer; HOME's four macros move them): POS A, POS B, WARP A, WARP B, CUT, RES, DTUN, SUB
 *   the patch (the rest: tables, warp modes, unison counts, tuning, levels, filter mode and key tracking, ENV 2, the LFOs, the matrix) is the preset's
 *   (WV_PATCH below, a row a factory preset); the native patch a user preset carries is the next milestone (docs/candidates/wave_engine.md)
 *
 * The split (ADDING_AN_ENGINE.md 5a): wave_render on CPU0 at the voice's turn does the control (the matrix, ENV 2, the LFOs, the increments, the filter's
 * coefficients and the frame cache: two stored frames blended and halved to the note's level into the voice's own RAM, refilled only when the position
 * moves by 1/1024 or more, one oscillator a block when both moved) and pushes the descriptor; wave_voice, the kernel, renders the block from the descriptor
 * and its voice's state alone (its own noise generator), on CPU1 or CPU0. Nothing is read per sample from flash: the frames a voice plays are in its state.
 * No hot set. Float for the filter (the FPU), fixed point for the oscillators.
 */
#include "wave_tables.h"
#define WV_LEN WV_FLEN                       /* 256 */
#define WV_UNI 8u                            /* unison copies an oscillator */
#define WV_NV 8u                             /* voices a track */
#define WV_MAGIC 0x45564157u                 /* "WAVE" */
enum { WARP_BEND, WARP_SYNC, WARP_MIRROR };
enum { WF_LP, WF_BP, WF_HP, WF_NOTCH, WF_LADDER };
enum { WS_ENV2, WS_LFO1, WS_LFO2, WS_VEL, WS_KEY, WS_N };
enum { WD_POSA, WD_POSB, WD_WARPA, WD_WARPB, WD_CUT, WD_RES, WD_PITCH, WD_LVLA, WD_LVLB, WD_N };
typedef struct { uint8_t src, dst; int8_t amt, pad; } wv_slot_t;          /* amt -127..127: a full source moves the destination by amt of its 0..127 range */
typedef struct {
    uint8_t tab[2], warp[2], uni[2];
    int8_t oct[2], semi[2];
    uint8_t lvl[2], noise, mode, kbd;        /* levels 0..127; noise 0..127; filter mode; key tracking 0..127 (127: the cutoff follows the key) */
    uint8_t e2[4];                           /* ENV 2: A D S R, 0..127 */
    uint8_t lrate[2], lshape[2];             /* the LFOs: rate 0..127 (0.05 .. 20 Hz), shape 0 triangle 1 saw 2 square 3 S&H, + 4: retriggered at the note */
    wv_slot_t slot[4];
    uint8_t width;                           /* (stereo, OC_STEREO firmware) the unison copies' spread, left to right, 0..127 (0: centred) */
    uint8_t level;                           /* the preset's level: (v / 64)^3, 64 unity (the render analysis sets it a preset: docs/candidates/wave_engine.md) */
} wv_patch_t;
typedef struct {
    uint32_t ph[2][WV_UNI], inc[2][WV_UNI];
    int16_t frame[2][WV_LEN];
    uint32_t cached[2];                      /* pos16 | level << 24 of the frame in the cache; ~0: none */
    uint8_t len_sh[2];                       /* 32 - log2 of the cached frame's length */
    uint8_t gate, est, uni[2];
    int32_t warp[2], amp[2], sub, noise;     /* this block's: warp amount (Q15), the levels (Q15) */
    float gain;                              /* the patch's level */
    int32_t ev;                              /* ENV 2 (Q15 << 8) */
    uint32_t lph[2];
    int32_t lsh[2];                          /* S&H values */
    float g, k, a1, ic1, ic2, l[4];          /* the filter: coefficients (this block's) and state */
    uint8_t fmode, wmode[2];
    uint32_t rng, blk;
    float ic1r, ic2r, lr[4];                 /* (stereo) the right lane's filter state */
    int32_t rbuf[CTL];                       /* (stereo) the voice's right lane this block (the kernel writes it; wave_post sums it on CPU0) */
    uint8_t rn;                              /* (stereo) frames in rbuf this block, 0 none */
} wv_voice_t;
typedef struct {
    fw_stereo_lane_t lane;                   /* FIRST (fw_api.h FW_MOD_COST_STEREO): the track's right lane, for an OC_STEREO firmware's mixer */
    uint32_t magic;
    wv_voice_t v[WV_NV];
} wv_part_t;

static wv_part_t *wave_part(const track_t *t);
static const wv_patch_t *wave_patch(const track_t *t);

static inline int32_t wv_lerp(int32_t a, int32_t b, uint32_t f15) { return a + (((b - a) * (int32_t)f15) >> 15); }   /* (an arithmetic shift: no divide in the sample loop) */
/* 2^x for x in about -12 .. 12 (a five-term polynomial on the fraction: 1e-5 relative) */
static inline float wv_exp2(float x)
{
    int32_t i = (int32_t)(x < 0.0f ? x - 1.0f : x);
    float f = x - (float)i, p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013333f))));
    while (i > 0) { p *= 2.0f; i--; }
    while (i < 0) { p *= 0.5f; i++; }
    return p;
}

/* the frame cache (CPU0, block rate): frames f0 and f0 + 1 of the table at pos16, blended, halved `level` times (a 7-tap half-band each, circular: one
 * cycle), into the voice's own RAM; the stored frames are read in order, whole cache lines (the friendly case for the D-cache) */
static void wv_cache(wv_voice_t *V, uint32_t o, const int16_t *table, uint32_t nframes, uint32_t pos16, uint32_t level)
{
    int16_t tmp[WV_LEN / 2];
    uint32_t span = pos16 * (nframes - 1u), f0 = span >> 16, frac = (span & 0xFFFFu) >> 1, i, n = WV_LEN, k;
    const int16_t *a = table + f0 * WV_LEN, *b = table + (f0 + 1u < nframes ? f0 + 1u : f0) * WV_LEN;
    int16_t *d = V->frame[o];
    for (i = 0; i < WV_LEN; i++)
        d[i] = (int16_t)wv_lerp(a[i], b[i], frac);
    for (k = 0; k < level && n > 16u; k++) {
        uint32_t m = n / 2u;
        for (i = 0; i < m; i++) {
            int32_t c = d[2 * i], l1 = d[(2 * i + n - 1u) & (n - 1u)], r1 = d[(2 * i + 1u) & (n - 1u)], l3 = d[(2 * i + n - 3u) & (n - 1u)],
                    r3 = d[(2 * i + 3u) & (n - 1u)];
            tmp[i] = (int16_t)((16 * c + 9 * (l1 + r1) - (l3 + r3)) / 32);
        }
        memcpy(d, tmp, m * sizeof(int16_t));
        n = m;
    }
    V->len_sh[o] = (uint8_t)(32u - (uint32_t)__builtin_ctz(n));
    V->cached[o] = pos16 | level << 24;
}

/* ENV 2 once a block (Q15 << 8): A, D and R from 0..127 as times of 2 ms .. 10 s (exponential), S 0..127; each stage a one-pole step a block (the
 * attack linear), so the times are the stages' time constants */
static float wv_tau_blocks(uint32_t x) { return (0.002f * wv_exp2((float)x * (12.3f / 127.0f))) * (44100.0f / (float)CTL); }   /* 2 ms * 5000^(x/127), in blocks */
static int32_t wv_env2(wv_voice_t *V, const wv_patch_t *P)
{
    const float top = (float)(32767 << 8);
    float s = (float)(P->e2[2] * 258 << 8), e = (float)V->ev;
    if (!V->gate) V->est = 3;
    switch (V->est) {
    case 0:
        e += top / wv_tau_blocks(P->e2[0]);
        if (e >= top) { e = top; V->est = 1; }
        break;
    case 1:
        e += (s - e) / wv_tau_blocks(P->e2[1]);
        if (e - s < 256.0f) { e = s; V->est = 2; }
        break;
    case 2: e = s; break;
    default: e -= e / wv_tau_blocks(P->e2[3]); if (e < 256.0f) e = 0.0f; break;
    }
    V->ev = (int32_t)e;
    return V->ev >> 8;
}
static int32_t wv_lfo(wv_voice_t *V, const wv_patch_t *P, uint32_t i)
{
    uint32_t inc = (uint32_t)(wv_exp2(-4.3f + (float)P->lrate[i] * (12.6f / 127.0f)) * (4294967296.0f * (float)CTL / 44100.0f)), was = V->lph[i];
    int32_t tri;
    V->lph[i] += inc;
    switch (P->lshape[i] & 3u) {
    case 1: return (int32_t)(V->lph[i] >> 16) - 32768;
    case 2: return V->lph[i] >> 31 ? 32767 : -32767;
    case 3:
        if (V->lph[i] < was) { V->rng = V->rng * 1664525u + 1013904223u; V->lsh[i] = (int32_t)(V->rng >> 16) - 32768; }
        return V->lsh[i];
    default:
        tri = (int32_t)(V->lph[i] >> 16) - 32768;
        return (tri < 0 ? -tri : tri) * 2 - 32767;
    }
}

/* the descriptor a voice's block needs (the kernel reads it and its voice's state only) */
typedef struct {
    wv_voice_t *V;
    vmod_t m;
    uint16_t n;
    uint8_t uni[2], wmode[2], fmode, stereo;
    int16_t pan[2][WV_UNI];                  /* (stereo) each copy's place, -32767 left .. 32767 right */
} wave_job_t;
_Static_assert(sizeof(wave_job_t) <= AUDIO_BATCH_BYTES, "WAVE's batch descriptor");
static const int16_t WV_UNI_GAIN[WV_UNI + 1] = {0, 32767, 23170, 18919, 16384, 14654, 13377, 12385, 11585};   /* 1 / sqrt(n), Q15 */

static inline __attribute__((always_inline)) void wave_body(const void *data, int32_t *out, const int stereo)
{
    const wave_job_t *j = data;
    wv_voice_t *V = j->V;
    const vmod_t *m = &j->m;
    uint32_t i, o, k, n = j->n, rng = V->rng;
    float ic1 = V->ic1, ic2 = V->ic2, l0 = V->l[0], l1 = V->l[1], l2 = V->l[2], l3 = V->l[3];
    float jc1 = V->ic1r, jc2 = V->ic2r, m0 = V->lr[0], m1 = V->lr[1], m2 = V->lr[2], m3 = V->lr[3];
    const float g = V->g, kk = V->k, a1 = V->a1;
    const float mlp = j->fmode == WF_LP || j->fmode == WF_NOTCH ? 1.0f : 0.0f, mbp = j->fmode == WF_BP ? 1.0f : 0.0f,
                mhp = j->fmode == WF_HP || j->fmode == WF_NOTCH ? 1.0f : 0.0f;
    int32_t og[2];
    for (o = 0; o < 2u; o++)
        og[o] = (V->amp[o] * WV_UNI_GAIN[j->uni[o]]) >> 15;
    for (i = 0; i < n; i++) {
        int32_t acc = 0, accr = 0;
        float y, yr = 0.0f;
        for (o = 0; o < 2u; o++) {
            const int16_t *fr = V->frame[o];
            const uint32_t sh = V->len_sh[o], mask = (1u << (32u - sh)) - 1u;
            const int32_t w = V->warp[o];
            int32_t s = 0, sr = 0;
            for (k = 0; k < j->uni[o]; k++) {
                uint32_t ph = V->ph[o][k] += V->inc[o][k], q, idx, f;
                if (j->wmode[o] == WARP_SYNC)            /* the frame read up to 1.37 x faster than the cycle: the overflow restarts it (the sync) */
                    q = ph + (ph >> 16) * (uint32_t)(w * 3);
                else {                                   /* BEND: p + a p (1 - p), a up to about 1 (a quarter cycle at the middle); MIRROR: the second half read backwards first */
                    uint32_t pp = j->wmode[o] == WARP_MIRROR && ph >= 0x80000000u ? ~ph << 1 : j->wmode[o] == WARP_MIRROR ? ph << 1 : ph, x = pp >> 16;
                    q = pp + ((x * (65535u - x)) >> 13) * (uint32_t)w;
                }
                idx = q >> sh;
                f = (q << (32u - sh)) >> 17;
                if (stereo) {                            /* the copy at its place: (L + R) / 2 is the mono copy */
                    int32_t x = wv_lerp(fr[idx], fr[(idx + 1u) & mask], f);
                    s += (x * (32768 - j->pan[o][k])) >> 15;
                    sr += (x * (32768 + j->pan[o][k])) >> 15;
                } else
                    s += wv_lerp(fr[idx], fr[(idx + 1u) & mask], f);
            }
            acc += (s * og[o]) >> 15;
            if (stereo) accr += (sr * og[o]) >> 15;
        }
        {   /* the sub (a sine an octave under A's first copy: parabolic) and noise (the voice's own generator) */
            int32_t x = (int32_t)((V->ph[0][0] >> 1) >> 16) - 32768, sine = (x * (32767 - (x < 0 ? -x : x))) >> 13;
            rng = rng * 1664525u + 1013904223u;
            int32_t c = (sine * V->sub + ((int32_t)(rng >> 17) - 16384) * V->noise) >> 15;
            acc += c;
            if (stereo) accr += c;
        }
        {
            float in = (float)acc * (1.0f / 32768.0f);
            if (j->fmode == WF_LADDER) {                 /* four one-pole stages, a cubic soft clip on the feedback */
                float fb = in - kk * l3, c = fb - fb * fb * fb * 0.1481f;
                l0 += g * (c - l0); l1 += g * (l0 - l1); l2 += g * (l1 - l2); l3 += g * (l2 - l3);
                y = l3;
            } else {                                     /* the TPT state-variable filter (Zavalishin) */
                float hp = (in - (kk + g) * ic1 - ic2) * a1, bp = g * hp + ic1, lp;
                ic1 = g * hp + bp;
                lp = g * bp + ic2;
                ic2 = g * bp + lp;
                y = mlp * lp + mbp * bp + mhp * hp;
            }
            if (stereo) {                                /* the right lane through its own filter */
                float inr = (float)accr * (1.0f / 32768.0f);
                if (j->fmode == WF_LADDER) {
                    float fb = inr - kk * m3, c = fb - fb * fb * fb * 0.1481f;
                    m0 += g * (c - m0); m1 += g * (m0 - m1); m2 += g * (m1 - m2); m3 += g * (m2 - m3);
                    yr = m3;
                } else {
                    float hp = (inr - (kk + g) * jc1 - jc2) * a1, bp = g * hp + jc1, lp;
                    jc1 = g * hp + bp;
                    lp = g * bp + jc2;
                    jc2 = g * bp + lp;
                    yr = mlp * lp + mbp * bp + mhp * hp;
                }
            }
        }
        {
            int32_t smp = soft_knee((int32_t)(y * V->gain * 16383.0f), 32767);   /* Q15 with headroom to 2.0 (as WHEEL's); the level's peaks above 1.0 round off */
            smp = smp > 65535 ? 65535 : smp < -65535 ? -65535 : smp;
            out[i] += (int32_t)((uint32_t)voice_amp(smp, m, i) << 1);
            if (stereo) {
                int32_t sr = soft_knee((int32_t)(yr * V->gain * 16383.0f), 32767);
                sr = sr > 65535 ? 65535 : sr < -65535 ? -65535 : sr;
                V->rbuf[i] = (int32_t)((uint32_t)voice_amp(sr, m, i) << 1);
            }
        }
    }
    V->ic1 = ic1; V->ic2 = ic2; V->l[0] = l0; V->l[1] = l1; V->l[2] = l2; V->l[3] = l3;
    V->rng = rng;
    if (stereo) {
        V->ic1r = jc1; V->ic2r = jc2; V->lr[0] = m0; V->lr[1] = m1; V->lr[2] = m2; V->lr[3] = m3;
        V->rn = (uint8_t)n;
    }
}
/* the kernels: mono (as before: the same instructions), and stereo (the right lane into the voice's rbuf) */
static void wave_voice(const void *data, int32_t *out) { wave_body(data, out, 0); }
static void wave_voice_st(const void *data, int32_t *out) { wave_body(data, out, 1); }

static void wave_note_on(track_t *t, voice_t *v)
{
    wv_part_t *S = wave_part(t);
    const wv_patch_t *P = wave_patch(t);
    wv_voice_t *V;
    uint32_t o, k, vi = (uint32_t)(v - t->v);
    if (!S || vi >= WV_NV) return;
    V = &S->v[vi];
    V->rng = 0x9E3779B9u * (vi + 1u) ^ (uint32_t)v->note * 0x85EBCA6Bu;
    for (o = 0; o < 2u; o++)
        for (k = 0; k < WV_UNI; k++) {                   /* the copies start at spread phases (a fixed pattern a voice: renders repeat) */
            V->rng = V->rng * 1664525u + 1013904223u;
            V->ph[o][k] = k ? V->rng : 0u;
        }
    V->cached[0] = V->cached[1] = ~0u;
    V->gate = 1;
    V->est = 0;
    V->ev = 0;
    for (o = 0; o < 2u; o++)
        if (P->lshape[o] & 4u) V->lph[o] = 0;
    V->ic1 = V->ic2 = 0.0f;
    V->l[0] = V->l[1] = V->l[2] = V->l[3] = 0.0f;
    V->ic1r = V->ic2r = 0.0f;
    V->lr[0] = V->lr[1] = V->lr[2] = V->lr[3] = 0.0f;
    V->rn = 0;
}

static void wave_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    wv_part_t *S = wave_part(t);
    const wv_patch_t *P = wave_patch(t);
    const int16_t *p = t->p;
    wv_voice_t *V;
    int32_t src[WS_N], dst[WD_N], pos[2], cut, res;
    uint32_t vi = (uint32_t)(v - t->v), o, k, need[2] = {0, 0}, pos16[2], lev[2], ti[2];
    float r[2];
    wave_job_t j;
    float hz, pitch;
    if (!S || vi >= WV_NV || n > CTL) return;
    V = &S->v[vi];
    V->gate = v->gate != 0;
    V->blk++;
    src[WS_ENV2] = wv_env2(V, P);
    src[WS_LFO1] = wv_lfo(V, P, 0);
    src[WS_LFO2] = wv_lfo(V, P, 1);
    src[WS_VEL] = (int32_t)v->vel * 258;
    src[WS_KEY] = clamp(((int32_t)v->note - 60) * 546, -32767, 32767);
    for (k = 0; k < WD_N; k++) dst[k] = 0;
    for (k = 0; k < 4u; k++) {                           /* the matrix: a full source moves its destination by amt of 127 (the 0..127 range, << 8) */
        const wv_slot_t *s = &P->slot[k];
        if (s->amt && s->src < WS_N && s->dst < WD_N) dst[s->dst] += (src[s->src] * s->amt) / 128;
    }
    /* the frame each oscillator plays: the position (POS A, POS B, the track's SHAPE modulation, the matrix), the level the pitch allows */
    hz = (float)m->inc * (44100.0f / 4294967296.0f);
    for (o = 0; o < 2u; o++) {
        uint32_t c;
        float f;
        ti[o] = P->tab[o] < WV_NTAB ? P->tab[o] : 0u;
        r[o] = wv_exp2(((float)P->oct[o] * 12.0f + (float)P->semi[o]) * (1.0f / 12.0f) + (float)dst[WD_PITCH] * (1.0f / (12.0f * 256.0f)));
        f = hz * r[o] * 128.0f;                          /* the top harmonic of the stored frame */
        pos[o] = clamp(p[P_E0 + o] * 256 + (m->shape - (64 << 8)) + dst[WD_POSA + o], 0, 127 << 8);
        pos16[o] = (uint32_t)pos[o] * 2u;
        lev[o] = 0;
        while (f > 18000.0f && lev[o] < 4u) { f *= 0.5f; lev[o]++; }
        c = V->cached[o] & 0xFFFFFFu;
        need[o] = V->cached[o] == ~0u || (V->cached[o] >> 24) != lev[o] || (c > pos16[o] ? c - pos16[o] : pos16[o] - c) >= 64u;   /* 1/1024 of the table */
    }
    if (need[0] && need[1] && V->cached[0] != ~0u && V->cached[1] != ~0u)
        need[(V->blk & 1u) ^ 1u] = 0;                    /* both moved: one a block (A on even blocks, B on odd) */
    for (o = 0; o < 2u; o++)
        if (need[o]) wv_cache(V, o, WV_TABLES[ti[o]], WV_NFRAME, pos16[o], lev[o]);
    for (o = 0; o < 2u; o++) {
        V->uni[o] = P->uni[o] < 1u ? 1u : P->uni[o] > WV_UNI ? (uint8_t)WV_UNI : P->uni[o];
        {   /* the copies' increments: DTUN spreads them over up to +-50 cents, evenly */
            float base = (float)m->inc * r[o], spread = (float)p[P_E6] * (50.0f / 127.0f / 1200.0f);
            uint32_t u = V->uni[o];
            for (k = 0; k < u; k++) {
                float c = u > 1u ? spread * (2.0f * (float)k / (float)(u - 1u) - 1.0f) : 0.0f;
                V->inc[o][k] = (uint32_t)(base * wv_exp2(c));
            }
        }
        V->warp[o] = clamp(p[P_E2 + o] * 256 + dst[WD_WARPA + o], 0, 127 << 8) / 4;   /* 0 .. 8128: BEND up to 1/4 of the phase, SYNC up to 1.75 x */
        V->amp[o] = clamp(P->lvl[o] * 256 + dst[WD_LVLA + o], 0, 127 << 8) * 258 / 256;
        V->wmode[o] = P->warp[o];
    }
    V->sub = p[P_E7] * 258;
    V->noise = P->noise * 129;
    /* the filter: CUT, the track's cutoff modulation (ENV, LFO, the matrix's CUT), key tracking and the patch's matrix; 20 Hz .. 18 kHz */
    cut = clamp(p[P_E4] * 256 + m->cutoff + (((int32_t)v->note - 60) * 256 * 127 / 120) * P->kbd / 127 + dst[WD_CUT], 0, 127 << 8);
    res = clamp(p[P_E5] * 256 + dst[WD_RES], 0, 127 << 8);
    pitch = 20.0f * wv_exp2((float)cut * (9.8f / 32512.0f));
    if (pitch > 18000.0f) pitch = 18000.0f;
    {
        float x = 3.14159265f * pitch * (1.0f / 44100.0f), x2 = x * x;
        float g = x * (1.0f + x2 * (0.3333333f + x2 * 0.1333333f));
        float r = P->mode == WF_LADDER ? (float)res * (3.8f / 32512.0f) : 2.0f - 1.92f * (float)res * (1.0f / 32512.0f);
        if (P->mode == WF_LADDER) g = g / (1.0f + g);
        V->g = g; V->k = r; V->a1 = 1.0f / (1.0f + g * (g + r));
    }
    V->fmode = P->mode;
    {   float lv = (float)(P->level ? P->level : 64u) * (1.0f / 64.0f); V->gain = lv * lv * lv; }
    j.V = V;
    j.m = *m;
    j.n = (uint16_t)n;
    j.uni[0] = V->uni[0]; j.uni[1] = V->uni[1];
    j.wmode[0] = V->wmode[0]; j.wmode[1] = V->wmode[1];
    j.fmode = V->fmode;
    j.stereo = (uint8_t)(wave_stereo_fw() && P->width);
    if (j.stereo)                                        /* the copies spread evenly across the width, A's and B's interleaved (A from the left, B from the right) */
        for (o = 0; o < 2u; o++)
            for (k = 0; k < WV_UNI; k++) {
                int32_t u = V->uni[o], w = P->width * 258;
                int32_t x = u > 1 ? (int32_t)((2 * (int32_t)k - (u - 1)) * w / (u - 1)) : 0;
                j.pan[o][k] = (int16_t)clamp(o ? -x : x, -32767, 32767);
            }
    if (audio_batch_active) {
        audio_batch_push(j.stereo ? wave_voice_st : wave_voice, &j, sizeof j, out, n, AP_WAVE);
        return;
    }
    (j.stereo ? wave_voice_st : wave_voice)(&j, out);
}
/* (stereo) once a block before the voices: no right lane yet; after the voices (CPU0, after the batch's join): the voices' right lanes summed into the track's (an
 * integer sum: the order of the voices does not change it) */
static void wave_block(track_t *t)
{
    wv_part_t *S = wave_part(t);
    if (!S) return;
    S->lane.magic = FW_STEREO_MAGIC;
    S->lane.on = 0;
    for (uint32_t i = 0; i < WV_NV; i++) S->v[i].rn = 0;
}
static void wave_post(track_t *t, int32_t *out, uint32_t n, uint32_t rendered)
{
    wv_part_t *S = wave_part(t);
    uint32_t i, k, any = 0;
    (void)out; (void)rendered;
    if (!S || n > CTL) return;
    for (k = 0; k < WV_NV; k++) any |= S->v[k].rn;
    if (!any) return;
    for (i = 0; i < n; i++) S->lane.right[i] = 0;
    for (k = 0; k < WV_NV; k++)
        if (S->v[k].rn)
            for (i = 0; i < n; i++) S->lane.right[i] += S->v[k].rbuf[i];
    S->lane.on = 1;
}
/* voice units (the shared budget): wave.md 4a's estimate, about 3 units at unison 1 and 5 at unison 4 a side; the unit's numbers replace it */
static uint32_t wave_units(const track_t *t)
{
    const wv_patch_t *P = wave_patch(t);
    return 3u + ((uint32_t)P->uni[0] + P->uni[1]) / 4u;
}
