/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 * Portions (the names, types and helpers of op1r's plugin API: op1_engine_t, the OP1_ constants, OP1_ENG_PAGE_WORD, the press values,
 * op1_voice_vel_gain's arithmetic, op1_sat16, op1_mul16) from op1r's synth_sdk/op1_plugin.h (https://github.com/Hundekraft/op1r-src, commit
 * bf808cfa, a fork of pcppcp/op1r-src), Copyright (C) 2026 pcppcp, GPL-3.0-or-later with the OP-1 firmware permission (op1r's NOTICE), taken
 * here under GPL-3.0.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "op1r"): the glue that runs one of op1r's synth plugins (an
 * op1_engine_t: note_on, render, note_off) as an Orange Cat engine_t, written once for every op1r module (tools/module/<name>_mod.c). The module
 * unit includes this after core.h, dsp.c and fw_api.h, then the plugin's file (tools/module/op1r/plugins/<name>/), whose own #include
 * "op1_plugin.h" finds tools/module/op1r/op1_plugin.h (build.py's "inc" for the op1r modules): op1r's header is never read. Then
 * OP1G_ENGINE_FUNCS(the plugin's op1_engine_t) makes the two engine_t callbacks.
 *
 * What op1r's firmware gives a plugin, and what the glue puts there:
 *   - the voice object `self`: op1r's is 0x24 bytes, word 0 the firmware's (never written by a plugin), bytes 4..0x23 the plugin's state. Here
 *     self is &v->ph[2]: word 0 is ph[2] and bytes 4..0x23 are v->s[0..7], exactly 32 bytes (checked below). The glue writes the voice's index
 *     into word 0 (op1_voice_index reads it) and the part's pointer into the word(s) before it (ph[1] on the target, ph[0..1] on a 64-bit host),
 *     at every call before the plugin runs: voice.c restores ph[] after a retrigger and adds a random phase to ph[0] of a UNISON voice, and
 *     neither matters then. The engines set keep = 0: voice.c restores no s[] word after note_on, so the plugin's note_on owns its 32 bytes
 *     (op1r's firmware does not touch them either).
 *   - the block: op1r calls render for 128 frames (OP1_BLOCK) and the plugins do block-rate work per call (index decays, limiters, the voices'
 *     note order); Orange Cat renders CTL (32) frames a call. The glue keeps each voice's 128 samples in the part's state (buf, left) and calls
 *     the plugin's render when the voice has used them, so the plugin runs at its own block size. Pitch, envelope and parameters are taken at
 *     that call (every 2.9 ms; op1r's are per block too).
 *   - OP1_PITCH_INC (a per-sample int32 array of the voice's phase increment, 2^32 a cycle a sample): m->inc, the same convention, for all 128.
 *   - OP1_VOICE_ENV (the stock envelope, 128 int16, velocity and level included, valid during render): m->amp1, the voice's amplitude at that
 *     control tick (the ADSR with the velocity), for all 128. The plugins read it as a timbre source; the glue still applies the amplitude.
 *   - op1_voice_vel_gain: v->vel x 258 (op1r's (vel << 8) >> 8 x 258); 0 or past 127: full.
 *   - params[0..3] (the four knobs, 0..32767): EDIT 1's P_E0..P_E3 (0..127) x 258, 0..32766; params[4] (op1r's engine selector, no plugin reads
 *     it): 0; params[5..7] (the second page's words, saved with the patch: OP1_ENG_PAGE_WORD): EDIT 2's P_E4..P_E6 x 258. P_E7 is not used.
 *   - note_off: Orange Cat's engine_t has no note-off; the glue calls the plugin's (when it has one) at the first render after v->gate fell,
 *     the release's first block (within the 128 frames the OP-1 itself is late by).
 *   - the output: the plugin's int16 (Q15) is scaled by the voice's amplitude ramp and level, voice_amp (the ADSR and gain op1r's firmware applies
 *     after render), and added to out.
 *   - OP1_SINE_TABLE (8192 int16, op1r's firmware table, read per sample): not given. 16 KB read per sample from flash breaks the cache rule
 *     (docs/orangecat/ADDING_AN_ENGINE.md, section 4) and is too large for a hot set. The plugins' lines that read it are changed to
 *     OP1_SINE(k): the same index k (0..8191, op1r's arithmetic kept) onto dsp.c's sine_i at the same phase (k << 19), the linear interpolation of
 *     SINE[1025] (1,024 intervals) through the firmware's base hot set. At k a multiple of 8 the two tables hold the same point; between, ours is
 *     interpolated: the renders differ from op1r's in the low bits (ENGINE_CANDIDATES.md gives the tolerance).
 *   - op1_dbg: nothing (op1r's is empty on the OP-1 too).
 *   - voices: OP1_VOICES (6). The engines' poly is 6; a voice past 5 (none, with that cap) renders silence and never reaches the plugin.
 * Not given (the plugins' screens, which the vendored copies leave out): OP1_VOICE(i), op1_eng_page, the screen and raster calls, the sample slot
 * and the FX API.
 */
#ifndef OP1_GLUE_H
#define OP1_GLUE_H 1
#define OP1_PLUGIN_H 1                       /* op1r's own header: never read (tools/module/op1r/op1_plugin.h) */
#define OP1_PLUGIN_API 1

#define OP1_BLOCK 128
#define OP1_VOICES 6
#define OP1_SAMPLE_RATE 44100
#define OP1_ENGINE_MAGIC 0x5300
#define OP1_VOICE_SIZE 0x24u

typedef struct {
    void (*note_on)(void *self, const int16_t *params);
    void (*render)(void *self, int16_t *out, const int16_t *params);   /* out[128] int16 */
    void (*note_off)(void *self);                                       /* may be 0 */
    uint8_t press[4];
} op1_engine_t;
#define OP1_ENG_PAGE_WORD(k) ((k) < 3 ? 5 + (k) : 7)
enum { OP1_PRESS_NONE = 0, OP1_PRESS_PAGE = 1, OP1_PRESS_LATCH = 2, OP1_PRESS_BROWSE = 3 };

static inline int16_t op1_sat16(int32_t x) { return x > 32767 ? 32767 : (x < -32768 ? -32768 : (int16_t)x); }
static inline int32_t op1_mul16(int32_t a, int32_t b) { return (a * b) >> 15; }
#define op1_dbg(text, a, b) ((void)0)

/* A part's state (one arena block a track; the header's state_size): the voices' blocks, the arrays the plugin reads through OP1_PITCH_INC
 * and OP1_VOICE_ENV, its params, and (OP1G_PART_STATE, defined by the module unit before this header) the plugin's own file-scope state */
typedef struct {
    int16_t buf[OP1_VOICES][OP1_BLOCK];      /* each voice's last block from the plugin */
    int32_t inc[OP1_BLOCK];                  /* OP1_PITCH_INC */
    int16_t env[OP1_BLOCK];                  /* OP1_VOICE_ENV */
    int16_t params[8];                       /* the plugin's params */
    uint8_t left[OP1_VOICES];                /* samples of buf not yet played (0: render the next block) */
    uint8_t gate[OP1_VOICES];                /* the key down as the glue last saw it (note_off) */
#ifdef OP1G_PART_STATE
    OP1G_PART_STATE own;
#endif
} op1g_part_t;

_Static_assert(__builtin_offsetof(voice_t, s) == __builtin_offsetof(voice_t, ph) + 3u * sizeof(uint32_t), "op1r's self: ph[2] then s[0..7]");
_Static_assert(sizeof(((voice_t *)0)->s) == OP1_VOICE_SIZE - 4u, "op1r's 32 bytes of voice state are voice_t's s[8]");
_Static_assert(sizeof(void *) <= 2u * sizeof(uint32_t), "the part's pointer fits ph[0..1]");

/* self (op1r's voice object) and what the glue keeps around it */
static inline op1g_part_t *op1g_part(const void *self)
{
    op1g_part_t *p;
    __builtin_memcpy(&p, (const char *)self - sizeof p, sizeof p);
    return p;
}
static inline voice_t *op1g_voice(const void *self) { return (voice_t *)((char *)self + 4 - __builtin_offsetof(voice_t, s)); }
static inline int op1_voice_index(const void *self) { return (int)*(const uint32_t *)self; }
static inline int32_t op1_voice_vel_gain(const void *self)
{
    int32_t v = op1g_voice(self)->vel;
    return v > 0 && v <= 127 ? v * 258 : 32767;
}
#define OP1_PITCH_INC ((const int32_t *)op1g_part(self)->inc)      /* (every plugin names its voice object self) */
#define OP1_VOICE_ENV ((const int16_t *)op1g_part(self)->env)
#define OP1G_OWN(self) (&op1g_part(self)->own)                       /* the plugin's file-scope state (OP1G_PART_STATE) */
#ifdef OP1G_PARITY_SINE                      /* tests/op1r_parity only, never a module: op1r's own 8192-point table (filled by the test as op1r's harness fills
                                             * it), so that the rest of the glue is proven sample for sample against op1r's render */
extern int16_t op1g_parity_sine[8192];
#define OP1_SINE(k) ((int32_t)op1g_parity_sine[(k)])
#elif defined(OP1G_EXACT_SINE)                /* (0.3) the module's hot set table number OP1G_EXACT_SINE is op1r's own sine, a quarter wave (op1_sine_q.h,
                                             * gen_op1_sine.py: OP1G_SINE_Q, 4,098 B), and the module unit defines OP1G_ENGINE (its engine number): op1r's table at
                                             * all 8192 points, for a plugin whose sine drives a pitch (brassophone's vibrato), where SINE's 1 LSB grows into drift */
#include "op1_sine_q.h"
static inline int32_t op1g_sine(int32_t k)
{
    const int16_t *q = (const int16_t *)ENG_HOT(0, OP1G_ENGINE, OP1G_EXACT_SINE);
    uint32_t u = (uint32_t)k & 8191u, i = u & 2047u;
    int32_t v = (u >> 11) & 1u ? q[2048u - i] : q[i];
    return u >> 12 ? -v : v;
}
#define OP1_SINE(k) op1g_sine((int32_t)(k))
#else
static inline int32_t op1g_sine(int32_t k) { return sine_i((uint32_t)k << 19); }
#define OP1_SINE(k) op1g_sine((int32_t)(k))
#endif

/* 64-bit division: the JieLi clang calls libgcc's __udivdi3 for some (dd_core.c's dd_set_notes: 2^40 / a period). tools/build.py refuses a module
 * object with an undefined symbol (a module imports nothing but the services table), so the glue carries its own, a shift and subtract: the target
 * only (a host build divides natively). The link keeps it only where a kept function calls it: today none does (dddd uses dd_set_incs, whose
 * divisions the compiler makes without the call, and the link drops dd_set_notes and this with it). */
#ifndef OC_MODULE_HOST
uint64_t __udivdi3(uint64_t n, uint64_t d);
uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    uint64_t q = 0, r = 0;
    int i;
    if (!d)
        return ~(uint64_t)0;                 /* (no plugin divides by 0: dd_set_incs and the limiter test first) */
    for (i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1u);
        if (r >= d) {
            r -= d;
            q |= (uint64_t)1 << i;
        }
    }
    return q;
}
#endif

/* the engine_t callbacks */
static inline op1g_part_t *op1g_part_of(const track_t *t)
{
    return OC_FW()->resource_get(OC_FW()->res_engine0 + (uint32_t)(t - trk) % NPART, sizeof(op1g_part_t));
}
static inline void *op1g_bind(op1g_part_t *P, voice_t *v, uint32_t vi)
{
    void *self = &v->ph[2];
    __builtin_memcpy((char *)self - sizeof P, &P, sizeof P);
    v->ph[2] = vi;
    return self;
}
static inline void op1g_params(op1g_part_t *P, const track_t *t)
{
    uint32_t k;
    for (k = 0; k < 4u; k++)
        P->params[k] = (int16_t)(clamp(t->p[P_E0 + k], 0, 127) * 258);
    P->params[4] = 0;
    for (k = 0; k < 3u; k++)
        P->params[5 + k] = (int16_t)(clamp(t->p[P_E4 + k], 0, 127) * 258);
#ifdef OP1G_PARAMS_HOOK                      /* (0.3) the module unit's: more of the part's state from the track, at each note-on and each block (aether's
                                             * accelerometer stand-in) */
    OP1G_PARAMS_HOOK(P, t);
#endif
}
static inline __attribute__((always_inline)) void op1g_note_on(track_t *t, voice_t *v, const op1_engine_t *E)
{
    uint32_t vi = (uint32_t)(v - t->v);
    op1g_part_t *P;
    void *self;
    if (vi >= OP1_VOICES || !(P = op1g_part_of(t)))
        return;
    self = op1g_bind(P, v, vi);
    op1g_params(P, t);
    P->left[vi] = 0;                         /* the next render asks the plugin for a block */
    P->gate[vi] = 1;
    E->note_on(self, P->params);
}
static inline __attribute__((always_inline)) void op1g_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m,
                                                              const op1_engine_t *E)
{
    uint32_t vi = (uint32_t)(v - t->v), i = 0, k;
    op1g_part_t *P;
    void *self;
    if (vi >= OP1_VOICES || !(P = op1g_part_of(t)))
        return;
    self = op1g_bind(P, v, vi);
    if (P->gate[vi] && !v->gate) {           /* the key went up: op1r's note_off */
        P->gate[vi] = 0;
        if (E->note_off)
            E->note_off(self);
    }
    while (i < n) {
        uint32_t left = P->left[vi], take;
        const int16_t *b;
        if (!left) {
            int32_t e = clamp(m->amp1, 0, 32767);
            for (k = 0; k < OP1_BLOCK; k++) {
                P->inc[k] = (int32_t)m->inc;
                P->env[k] = (int16_t)e;
            }
            op1g_params(P, t);
            E->render(self, P->buf[vi], P->params);
            left = OP1_BLOCK;
        }
        take = left < n - i ? left : n - i;
        b = P->buf[vi] + (OP1_BLOCK - left);
        for (k = 0; k < take; k++, i++)
            out[i] += voice_amp(b[k], m, i);
        P->left[vi] = (uint8_t)(left - take);
    }
}
#define OP1G_ENGINE_FUNCS(E)                                                                                              \
    static void op1g_eng_note_on(track_t *t, voice_t *v) { op1g_note_on(t, v, &(E)); }                                   \
    static void op1g_eng_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m) { op1g_render(t, v, out, n, m, &(E)); }
#endif
