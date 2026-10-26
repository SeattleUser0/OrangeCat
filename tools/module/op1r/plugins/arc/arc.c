/* (Orange Cat, this copy) op1r bf808cfa's file up to its screen code (left out). Changed, each marked Orange Cat: the screen's shared state (the
 * trace, its voice and position, the levels) is the part's (tools/module/arc_mod.c arc_part_t); the G table is decoded at build time
 * (tools/module/op1r/arc_gp.h, gen_arc_gp.py) instead of into a RAM array, so gp_decode has nothing left to do. */
/* arc: Duddell's singing arc (William Duddell, London 1899-1900). A carbon arc lamp shunted by an LC circuit
 * oscillates through the arc's negative resistance; Duddell played it from a keyboard. Each voice is a
 * Van der Pol oscillator  x'' - mu (1 - x^2) x' + w^2 x = 0  (a "lamp") plus a second, coupled lamp.
 *   page 1  blue  = current mu: clean sine -> reedy buzz -> screaming relaxation oscillation
 *           green = carbon: asymmetric electrode (even harmonics, a DC bias that the DC blocker removes)
 *           white = hiss: plasma noise injected into the oscillator (phase noise: pitch wanders at high values)
 *           red   = strike: pitch sag on attack (depth and 50 ms..1 s recovery) + an ignition noise burst; while a
 *                   key is held the lamp also sputters: random re-strikes (dip, crackle, short dropout), 0..6 / s
 *   page 2  blue  = 2nd lamp tuning ratio 1/2..2 with detents at 1/2 2/3 3/4 1 5/4 4/3 3/2 5/3 2 (word 5)
 *   (press) green = coupling: 0 = 2nd lamp off; weak = beats / locking, strong = chaos (word 6)
 *           white = brightness: key-tracked output tilt, dark .. bright (word 7; unset = neutral)
 *
 * DSP (integers only). The oscillator runs in its own time tau = w t, so the waveform does not change across the
 * keyboard: v' = eps (1 - x^2) v - x, x' = v, semi-implicit (symplectic) Euler with step h = G * theta per sample
 * (theta = 2 pi f / fs). The limit cycle period grows with eps (2 pi at eps = 0, ~12 at eps = 7) and the
 * discretisation shifts it again at high notes, so G comes from a 2-D table (eps knob x pitch, 1/8 octaves)
 * that gen_tables.py measured by running this very code (arc_calib.c): notes stay in tune at every setting.
 * The table is stored compressed (lossless, saves 3 KB of code space) and decoded into RAM once: by the screen's init
 * (GUI thread, normally before any note), else row by row on first use in render (bounded, once per boot).
 * Stability: x is clamped to +-3, v saturates at +-16; h*eps <= 0.3 (at high notes the lamp gets cleaner instead
 * of blowing up, which also keeps aliasing down) and the per-step damping factor is clamped at -0.75.
 * Cost: all products are 16 x 16 bit (one MAC on the Blackfin): x, v enter them as Q12 / Q11 views, h as a Q15
 * mantissa with one per-block shift per lamp. Pass 1 runs the lamps and writes the raw arc current into out[],
 * pass 2 shapes it (carbon, DC blocker, tilt, gain), so neither loop runs out of registers.
 * Voice state: 2 x (x, v) + rng + DC + tilt + strike env + gate = 32 bytes.
 * Copyright (C) 2026 ultralazr. GPL-3.0-or-later with the OP-1 firmware permission (see LICENSE and NOTICE).
 */
#include "op1_plugin.h"
#include "arc_tables.h"

typedef struct {
    uint32_t vtable;          /* +0x00 firmware */
    int32_t  x1, v1;          /* +0x04 main lamp: x Q29, v Q27 (saturating: |x| < 4, |v| < 16) */
    int32_t  x2, v2;          /* +0x0c second lamp */
    uint32_t rng;             /* +0x14 xorshift32 plasma noise */
    int32_t  dc;              /* +0x18 DC blocker state, Q20 */
    int32_t  lp;              /* +0x1c brightness tilt lowpass, Q20 */
    int16_t  env;             /* +0x20 strike envelope, Q15 */
    uint8_t  gate;            /* +0x22 key held (screen) */
    uint8_t  live;            /* +0x23 bit 0: voice has run (screen); bit 1: env is a sputter re-strike */
} arc_voice_t;
typedef char arc_voice_fits[sizeof(arc_voice_t) == OP1_VOICE_SIZE ? 1 : -1];

#define HE_MAX 9830           /* h * eps <= 0.3 (Q15): above that the period stops being monotonic in h */
#define A_MIN  (-24576)       /* damping per step >= -0.75: no sign flip of v, ever */
#define HMAX   29491          /* h <= 0.9 (symplectic Euler is stable below 2) */
#ifdef __bfin__
/* 16 x 16 -> 32 multiply as one MAC instruction (left to itself, gcc often sign-extends and uses the multi-cycle
 * 32-bit multiply), saturating add as one instruction */
static inline int32_t m16(int32_t a, int32_t b)
{ int32_t r; __asm__("%0 = %1.L * %2.L (IS);" : "=d"(r) : "d"(a), "d"(b)); return r; }
static inline int32_t m16h(int32_t a, int32_t b)
{ int32_t r; __asm__("%0 = %1.L * %2.H (IS);" : "=d"(r) : "d"(a), "d"(b)); return r; }
#define M16(a, b) m16((int32_t)(a), (int32_t)(b))
#define MH(a, b)  m16h((int32_t)(a), (int32_t)(b))         /* a.L * b.H: two constants share one register */
#define SADD(a, b) __builtin_bfin_add_fr1x32((a), (b))
#define SHL3(a)    __builtin_bfin_shl_fr1x32((a), 3)        /* saturating << 3 */
#else
#define M16(a, b) ((int32_t)(int16_t)(a) * (int16_t)(b))    /* 16 x 16 -> 32 (one MAC on the Blackfin) */
#define MH(a, b)  ((int32_t)(int16_t)(a) * (int16_t)((uint32_t)(b) >> 16))
static inline int32_t SADD(int32_t a, int32_t b)
{
    int32_t s = (int32_t)((uint32_t)a + (uint32_t)b);
    return ((a ^ s) & (b ^ s)) < 0 ? (a < 0 ? (int32_t)0x80000000 : 0x7fffffff) : s;
}
static inline int32_t SHL3(int32_t a) { return a > 0x0FFFFFFF ? 0x7fffffff : (a < -0x10000000 ? (int32_t)0x80000000 : a * 8); }
#endif

/* ---- shared with the screen (written by render, read by the GUI thread) ---- */
#define ARC_TN 256                                  /* phase portrait ring buffer (decimated) */
/* (Orange Cat) arc_trace[ARC_TN][4], arc_tpos, arc_tvoice (0xff at boot) and arc_level[OP1_VOICES]: the part's (tools/module/arc_mod.c) */

#ifdef ARC_CALIB                                    /* gen_tables.py: fixed G, record x1 per sample */
uint16_t arc_cal_gp; int32_t *arc_cal_rec;
#endif

#ifdef __bfin__
static int msb32(uint32_t v)                        /* index of the top set bit (v > 0): SIGNBITS */
{ return (int32_t)v < 0 ? 31 : 30 - __builtin_bfin_norm_fr1x32((int32_t)v); }
#else
static int msb32(uint32_t v) { int e = 0; while (v > 1) { v >>= 1; e++; } return e; }   /* <= 31 steps */
#endif
static uint32_t isqrt32(uint32_t v)                 /* floor(sqrt(v)), 16 steps (per block only) */
{
    uint32_t r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) { if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1; b >>= 2; }
    return r;
}

/* pitch: inc (2^32 = 1 cycle/sample) = m * 2^(e - 15), m in [2^15, 2^16); times ratio (Q14, 0.5..2) */
typedef struct { uint32_t m; int e; } pitch_t;
static pitch_t pitch_of(uint32_t inc, int32_t r14)
{
    pitch_t p;
    if (inc < (1u << 18)) inc = 1u << 18;           /* < 2.6 Hz */
    p.e = msb32(inc); p.m = inc >> (p.e - 15);
    p.m = (p.m * (uint32_t)r14) >> 14;
    if (p.m >= 1u << 16) { p.m >>= 1; p.e++; } else if (p.m < 1u << 15) { p.m <<= 1; p.e--; }
    return p;
}

/* ARC_GPZ (arc_tables.h, gen_tables.py: encode) -> arc_gp rows < n. Row k needs row k - 1 only, rows are decoded in
 * order and the decode is idempotent (same values written), so the GUI (screen init) and render (interrupt) may both
 * run it, even interleaved. Worst case in render: all 33 rows once (~3200 short steps), normally none. */
#include "arc_gp.h"                                /* (Orange Cat) arc_gp[ARC_KN][ARC_PN] decoded at build time (gen_arc_gp.py: gp_decode's arithmetic) */
#define arc_gp_n ARC_KN                             /* (Orange Cat) every row decoded */
#define gp_decode(n) ((void)(n))                    /* (Orange Cat) nothing to decode */

/* G * 2 pi (Q12) for knob row (ki, kf Q15) at pitch p: table nodes every 1/8 octave from inc 2^19 (5 Hz) */
static uint32_t gp_of(int ki, int32_t kf, pitch_t p)
{
#ifdef ARC_CALIB
    if (arc_cal_gp) return arc_cal_gp;
#endif
    int o = p.e - 19, pi; int32_t pf;
    if (o < 0) { pi = 0; pf = 0; }
    else if (o >= ARC_OCT) { pi = ARC_PN - 2; pf = 32767; }
    else { pi = o * 8 + (int)((p.m >> 12) & 7); pf = (int32_t)(p.m & 4095) << 3; }
    if (ki + 2 > arc_gp_n) gp_decode(ki + 2);       /* table rows ki, ki + 1 (normally decoded by the screen) */
    const uint16_t *a = arc_gp[ki], *b = arc_gp[ki + 1];
    int32_t g0 = a[pi] + ((((int32_t)a[pi + 1] - a[pi]) * pf) >> 15);
    int32_t g1 = b[pi] + ((((int32_t)b[pi + 1] - b[pi]) * pf) >> 15);
    return (uint32_t)(g0 + (((g1 - g0) * kf) >> 15));
}

/* per-block lamp coefficients. h = (m * Gp) * 2^(e - 59) is kept as a Q15 mantissa and one shift: h = hm >> sh.
 * Every lamp product (h v -> x, h x -> v, coupling -> v) uses that same shift. x is Q29 and v Q27 and both
 * saturate (|x| < 4: the limit cycle is 2, a wall for noise-driven excursions; |v| < 16). */
typedef struct { int32_t h, sh, he, hk, sg, g; } lamp_t;
static int32_t shl(uint32_t v, int k) { return (int32_t)(k >= 0 ? v << k : (k > -32 ? v >> -k : 0)); }
static void lamp_setup(lamp_t *L, uint32_t inc, int32_t r14, int oct, int32_t sag, int ki, int32_t kf, int32_t eps12,
                       int32_t k12, int32_t sigc)
{
    pitch_t p = pitch_of(inc, r14);
    p.e += oct;                                     /* 2nd lamp runs at half rate: twice the step */
    uint32_t P = ((p.m * gp_of(ki, kf, p)) >> 15) * (uint32_t)sag;
    if (!P) P = 1;
    int s = 58 - (msb32(P) + p.e), h;               /* hm in [2^14, 2^15) */
    if (s < 0) { s = 0; h = HMAX; }                 /* h >= 1: clamp (very high notes) */
    else {
        if (s > 24) s = 24;
        h = shl(P, p.e - 44 + s);
        if (s == 0 && h > HMAX) h = HMAX;
    }
    L->h = h; L->sh = s;
    int32_t he = ((h * eps12) >> 12) >> s;
    L->he = he > HE_MAX ? HE_MAX : he;
    L->hk = (h * k12) >> 10;                        /* 4 h k: k <= 0.25 (Q12 <= 1024), dv is Q10 */
    /* noise per step ~ sqrt(h): the phase wander per cycle is the same on every key */
    uint32_t rt = isqrt32((uint32_t)h << 15) >> (s >> 1);
    if (s & 1) rt = (rt * 23170u) >> 15;
    L->sg = (int32_t)(((uint32_t)sigc * rt) >> 18); /* Q12 */
    int32_t g = s >= 1 ? h >> (s - 1) : 2 * h;      /* tilt lowpass g = 2h: corner ~2x the fundamental */
    L->g = g > HMAX ? HMAX : g;
}

/* one semi-implicit Euler step of a lamp. xs = x as Q12, nz = noise (int16), cpl = coupling (Q27 before >> sh),
 * K = h | he << 16, N = sg | hk << 16 (packed: the dual loop would run out of registers otherwise) */
#define LAMP(x, v, xs, K, sh, N, nz, cpl) do {                                                 \
        int32_t f_ = 2048 - (M16(xs, xs) >> 13);          /* 1 - x^2 in Q11, > -15 as |x| < 4 */ \
        int32_t a_ = (MH(f_, K) + 1024) >> 11;            /* h eps (1 - x^2), Q15, rounded */  \
        if (a_ < A_MIN) a_ = A_MIN;                                                            \
        (v) = SADD((v), M16((v) >> 16, a_) * 2);                                               \
        (v) = SADD((v), (((cpl) - M16(xs, K)) >> (sh)) + M16(nz, N));                          \
        (x) = SADD((x), SHL3(M16((v) >> 16, K) >> (sh)));    /* Q26 step -> Q29, saturating */ \
    } while (0)

/* page-2 ratio (word 5): breakpoints (word, ratio Q14), flat around the simple ratios; 0 = unset = 1:1 */
static int32_t ratio_of(int32_t w)
{
    if (w <= 0) return 16384;
    for (int i = 1; i < ARC_RN; i++)
        if (w <= ARC_RW[i]) {
            int32_t w0 = ARC_RW[i - 1], w1 = ARC_RW[i], r0 = ARC_RR[i - 1], r1 = ARC_RR[i];
            return w1 == w0 ? r1 : r0 + (r1 - r0) * (w - w0) / (w1 - w0);   /* per block */
        }
    return ARC_RR[ARC_RN - 1];
}

static inline int32_t clampk(int32_t k) { return k < 0 ? 0 : (k > 32767 ? 32767 : k); }

/* portrait ordinate (trace points only): (v - eps (x - x^3/3)) / (1 + eps/3). Small eps: v, the circle of a sine.
 * Large eps: -3 times the Lienard variable, so the relaxation cycle shows as the squarish loop of two slow cubic
 * branches joined by fast jumps. Q12 in/out, x clipped to +-3 for the cubic. */
static int16_t lienard(int32_t xs, int32_t v, int32_t eps12, int32_t vmul)
{
    if (xs > 12288) xs = 12288; else if (xs < -12288) xs = -12288;
    int32_t x3 = (((xs * xs) >> 12) * xs) >> 12, F = xs - ((x3 * 10923) >> 15);
    int32_t y = (((v >> 15) - ((eps12 * F) >> 12)) * vmul) >> 12;
    return (int16_t)(y > 32767 ? 32767 : (y < -32767 ? -32767 : y));
}

static void arc_note_on(void *self, const int16_t *params)
{
    arc_voice_t *v = (arc_voice_t *)self;
    v->x1 = 0; v->v1 = 2 << 27;                     /* on the sine limit cycle, output starts at 0: no click */
    v->x2 = 1 << 30; v->v2 = 0;                     /* 2nd lamp a quarter cycle away: it pulls into phase */
    if (!v->rng) v->rng = 0x9E3779B9u ^ (uint32_t)(uintptr_t)self;
    v->dc = (M16(-8192, clampk(params[1])) >> 16) * 256;   /* carbon's DC at x = 0: output starts at 0 */
    v->lp = 0;
    v->env = 32767; v->gate = 1; v->live = 1;           /* ignition (not a sputter) */
    arc_tvoice = (uint8_t)op1_voice_index(self);
}

static void arc_note_off(void *self) { ((arc_voice_t *)self)->gate = 0; }

static void arc_render(void *self, int16_t *out, const int16_t *params)
{
    arc_voice_t *v = (arc_voice_t *)self;
    const int32_t *inc = OP1_PITCH_INC;
    int32_t mu = clampk(params[0]), carbon = clampk(params[1]), hiss = clampk(params[2]), strike = clampk(params[3]);
    int32_t wr = clampk(params[5]), wc = clampk(params[6]), wb = clampk(params[7]);
    int vi = op1_voice_index(self);

    /* blue: eps = 0.04 .. 7 (quadratic: most of the travel is in the musical sine..reed range) */
    int32_t u2 = (mu * mu) >> 15;
    int32_t eps12 = 164 + ((u2 * 28508) >> 15);
    int ki = (int)(mu >> 10); int32_t kf = (mu & 1023) << 5;
    /* red: strike sag depth up to 0.5 (-12 semitones; 1 - (1 - red)^2, so the lower half is already clearly
     * audible), recovery 50 ms .. 1 s (per-block decay; the pitch steps per block are phase-continuous).
     * Sputter: while the key is held the lamp re-strikes at random, 0 .. 6 per second (red^2): the envelope jumps
     * to 35..90 % and decays 4x faster than the ignition, with a level dip (a short dropout) on top. */
    int32_t sq = 32767 - strike, env0 = v->env, depth = (32767 - ((sq * sq) >> 15)) >> 1;
    int32_t dec = 1927 - ((strike * 1832) >> 15);
    int sput = v->live & 2;
    if (sput) dec <<= 2;
    int32_t env1 = env0 - ((env0 * dec) >> 15); if (env1 < 16) env1 = 0, sput = 0;
    if (v->gate && strike) {
        uint32_t q = v->rng; q ^= q << 13; q ^= q >> 17; q ^= q << 5; v->rng = q;
        if ((q & 0xffff) < (uint32_t)((((strike * strike) >> 15) * 1141) >> 15)) {   /* 6 / s / 344.5 blocks, Q16 */
            int32_t amp = 11469 + (int32_t)(((q >> 16) * 18022) >> 16);              /* 0.35 .. 0.9 */
            if (amp > env1) env1 = amp, sput = 2;
        }
    }
    v->env = (int16_t)env1; v->live = (uint8_t)((v->live & 1) | sput);
    int32_t sag = 32767 - ((depth * ((env0 + env1) >> 1)) >> 15);
    /* sputter dropout: level dip up to 85 % with the re-strike envelope, ramped across the block (no click) */
    int32_t dip0 = sput ? 32767 - ((env0 * 27853) >> 15) : 32767, dip1 = sput ? 32767 - ((env1 * 27853) >> 15) : 32767;
    /* white: plasma noise sigma (continuous-time, Q15) up to 0.45, plus the strike's ignition burst */
    int32_t h2k = (hiss * hiss) >> 15;
    int32_t sigc = ((h2k * 14745) >> 15) + ((((depth * env1) >> 15) * 3) >> 1);
    if (sigc > 32767) sigc = 32767;
    /* page 2 */
    int32_t r14 = ratio_of(wr);
    /* coupling: dissipative, k (v2 - v1) (the lamps share the arc current). Unlike a spring coupling k (x2 - x1) it
     * adds no stiffness, so the played note stays in tune at every ratio, and it pulls the pair into phase (a
     * spring coupling locks relaxation oscillators in anti-phase: the mix cancels and jumps an octave). It damps
     * an unlocked lamp by k, so k <= eps / 4 keeps both singing (amplitude >= 1.7). k = 0..0.25, quadratic. */
    int32_t k12 = ((wc * wc) >> 15) >> 5;
    if (k12 > eps12 >> 2) k12 = eps12 >> 2;
    int32_t mix = wc << 2; if (mix > 16384) mix = 16384;   /* 2nd lamp level: equal mix from 1/8 knob */
    int dual = wc > 0;
    int32_t wbb = wb ? wb : 8192;                   /* unset word = neutral brightness */
    int32_t bt = wbb <= 8192 ? 1024 + ((wbb * 3072) >> 13) : 4096 + (((wbb - 8192) * 6144) / 24575);

    uint32_t im = (uint32_t)inc[OP1_BLOCK / 2];     /* pitch per block (bends / LFO are smooth) */
    lamp_t A, B;
    lamp_setup(&A, im, 16384, 0, sag, ki, kf, eps12, k12, sigc);
    if (dual) lamp_setup(&B, im, r14, 1, sag, ki, kf, eps12, k12, sigc); else B = A;

    /* level: the table's loudness correction across mu, velocity, and headroom for 2 lamps / bright / carbon / hiss */
    int32_t gn = ARC_GAIN[ki] + (((ARC_GAIN[ki + 1] - ARC_GAIN[ki]) * kf) >> 15);              /* Q12 */
    int32_t gv = (op1_voice_vel_gain(self) * gn) >> 12;
    gv = (gv * (4096 + (mix >> 4))) / (4096 + ((carbon * 1229) >> 15) + ((h2k * 1638) >> 15) + (bt > 4096 ? bt - 4096 : 0));
    if (gv > 32767) gv = 32767;                     /* ^ per block (division) */

    /* trace: the most recent voice writes a point every `chunk` samples (64..128 points per cycle) */
    int traced = vi == arc_tvoice, chunk = OP1_BLOCK;
    if (traced) {
        int k = 32 - msb32(im | 1) - 6;
        chunk = k < 1 ? 2 : (k > 7 ? OP1_BLOCK : 1 << k);
    }
    /* portrait with the effective eps (h eps is capped at 0.3 at high notes); per-block divisions */
    int32_t epsd = A.he >= HE_MAX && (A.h >> A.sh) > 0 ? (HE_MAX * 4096) / (A.h >> A.sh) : eps12;
    if (epsd > eps12) epsd = eps12;
    int32_t vmul = (4096 * 4096) / (4096 + epsd / 3);                     /* portrait scale, see lienard() */

    /* pass 1: the lamps -> raw arc current y (Q12) + plasma hiss riding on |x| (no onset click) into out[] */
    int32_t x1 = v->x1, v1 = v->v1, x2 = v->x2, v2 = v->v2;
    uint32_t r = v->rng;
#define PACK(lo, hi) ((int32_t)(((uint32_t)(hi) << 16) | ((uint32_t)(lo) & 0xffff)))
    const int32_t K1 = PACK(A.h, A.he), N1 = PACK(A.sg, A.hk), K2 = PACK(B.h, B.he), N2 = PACK(B.sg, B.hk);
    const int32_t O = PACK(h2k >> 2, mix);         /* hiss level | 2nd lamp mix */
    const int sh1 = A.sh, sh2 = B.sh;
    for (int16_t *o = out; o < out + OP1_BLOCK; ) {
        int32_t xs = 0, xt = 0;
        int16_t *e = o + chunk;
        if (!dual)
            for (; o < e; o++) {
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                xs = x1 >> 17;
                LAMP(x1, v1, xs, K1, sh1, N1, r >> 16, 0);
#ifdef ARC_CALIB
                if (arc_cal_rec) *arc_cal_rec++ = x1;
#endif
                *o = (int16_t)(xs + (M16(M16(r, O) >> 15, xs < 0 ? -xs : xs) >> 13));
            }
        else
            for (; o < e; o += 2) {                    /* 2nd lamp at half rate (CPU), linearly interpolated */
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                int32_t xp = x2 >> 17, dv = (v2 >> 17) - (v1 >> 17);   /* dv: Q10 */
                xt = xp;
                LAMP(x2, v2, xt, K2, sh2, N2, r >> 8, -MH(dv, N2));
                xt = x2 >> 17; xp = (xp + xt) >> 1;
                xs = x1 >> 17; dv = MH((v2 >> 17) - (v1 >> 17), N1);   /* held for the pair */
                LAMP(x1, v1, xs, K1, sh1, N1, r >> 16, dv);
                o[0] = (int16_t)(xs + (MH((xp - xs) >> 1, O) >> 14) + (M16(M16(r, O) >> 15, xs < 0 ? -xs : xs) >> 13));
                xs = x1 >> 17;
                LAMP(x1, v1, xs, K1, sh1, N1, r, dv);
                o[1] = (int16_t)(xs + (MH((xt - xs) >> 1, O) >> 14) + (M16(M16(r >> 16, O) >> 15, xs < 0 ? -xs : xs) >> 13));
            }
        if (traced) {
            int t = arc_tpos;
            arc_trace[t][0] = (int16_t)xs; arc_trace[t][1] = lienard(xs, v1, epsd, vmul);
            arc_trace[t][2] = (int16_t)(dual ? xt : xs);
            arc_trace[t][3] = dual ? lienard(xt, v2, epsd, vmul) : arc_trace[t][1];
            arc_tpos = (uint16_t)((t + 1) & (ARC_TN - 1));
        }
    }
    v->x1 = x1; v->v1 = v1; v->x2 = x2; v->v2 = v2; v->rng = r;

    /* pass 2: carbon (y += carbon/2 (y^2 - 2): a 2nd harmonic for a sine), DC blocker ~7 Hz, key-tracked tilt
     * (one-pole at ~2x the fundamental: out = lp + bt (y - lp)), gain */
    int32_t dc = v->dc, lp = v->lp, pk = 0;
    const int16_t cb = (int16_t)carbon, g = (int16_t)A.g, btq = (int16_t)bt;
    int32_t ga = ((gv * dip0) >> 15) << 16, dg = ((((gv * dip1) >> 15) << 16) - ga) >> 7;   /* gain ramp, Q16 */
    for (int n = 0; n < OP1_BLOCK; n++) {
        const int16_t gq = (int16_t)(ga >> 16); ga += dg;
        int32_t y = out[n];
        int32_t q = (M16(y, y) >> 12) - 8192; if (q > 32767) q = 32767;
        y += M16(q, cb) >> 16;
        dc += (y * 256 - dc) >> 10; y -= dc >> 8;
        if (y > 16383) y = 16383; else if (y < -16383) y = -16383;
        lp += M16(y - (lp >> 8), g) >> 7;
        int32_t l = lp >> 8, o = l + (M16(y - l, btq) >> 12);
        if (o > 32767) o = 32767; else if (o < -32767) o = -32767;
        out[n] = (int16_t)(SHL3(M16(o, gq)) >> 16);   /* = op1_sat16(o * gq >> 13), branch-free */
    }
    v->dc = dc; v->lp = lp;
    for (int n = 0; n < OP1_BLOCK; n += 8) { int32_t a = out[n] < 0 ? -out[n] : out[n]; if (a > pk) pk = a; }
    arc_level[vi] = (int16_t)pk;
}

const op1_engine_t ENGINE_arc = { arc_note_on, arc_render, arc_note_off,
                                  { OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_PAGE, OP1_PRESS_NONE } };

