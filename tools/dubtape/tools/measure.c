/* SPDX-License-Identifier: GPL-3.0-only
 * dubdelay research: objective measurements (timing, decay, spectra per repeat, stability, saturation, aliasing,
 * noise, parameter-change artifacts, dry / wet and bypass). Host analysis in double; writes Markdown to stdout. */
#include "../src/dubdelay.h"
#include "../src/dd_scenes.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define NMAX (DD_FS * 70)
static int16_t L0[1 << 17], L1[1 << 17];
static float xin[NMAX], ol[NMAX], orr[NMAX];

typedef struct { const char *name; int algo, R, aa; } cfg_t;
static const cfg_t CF[] = {
    {"DIGITAL 44k", DD_ALGO_DIGITAL, 1, DD_AA_HB}, {"LITE 44k", DD_ALGO_LITE, 1, DD_AA_HB},
    {"LITE 22k", DD_ALGO_LITE, 2, DD_AA_HB}, {"LITE 11k", DD_ALGO_LITE, 4, DD_AA_HB},
    {"LITE 22k cheapAA", DD_ALGO_LITE, 2, DD_AA_CHEAP},
    {"TAPE 44k", DD_ALGO_TAPE, 1, DD_AA_HB}, {"TAPE 22k", DD_ALGO_TAPE, 2, DD_AA_HB}, {"TAPE 11k", DD_ALGO_TAPE, 4, DD_AA_HB},
};
static const cfg_t CONV[] = {
    {"plain 44k (no converter)", DD_ALGO_DIGITAL, 1, DD_AA_HB}, {"half-band 31 22k", DD_ALGO_DIGITAL, 2, DD_AA_HB},
    {"half-band 31 x2 11k", DD_ALGO_DIGITAL, 4, DD_AA_HB}, {"box/linear 22k", DD_ALGO_DIGITAL, 2, DD_AA_CHEAP},
    {"box/linear 11k", DD_ALGO_DIGITAL, 4, DD_AA_CHEAP},
};
#define NCF (int)(sizeof CF / sizeof CF[0])

typedef void (*autom_t)(float t, dd_params_t *p);
static dd_params_t base;
static dd_t S;

static void run(const cfg_t *c, int n, autom_t a, float maxhead)
{
    int b;
    dd_init(&S, c->algo, c->R, c->aa, 1, L0, L1, dd_line_len(maxhead, c->R));
    for (b = 0; b + DD_BLOCK <= n; b += DD_BLOCK) {
        dd_params_t p = base;
        if (a) a((float)b / DD_FS, &p);
        if (c->algo == DD_ALGO_DIGITAL) { dd_to_digital(&p); p.lp = 127.0f; p.hp = 0.0f; }
        dd_process(&S, &p, xin + b, ol + b, orr + b);
    }
}
static void neutral(dd_params_t *p)
{
    memset(p, 0, sizeof *p);
    p->dry = 0.0f; p->wet = 1.0f; p->send = 1.0f; p->glide = 0.0f; p->hp = 0.0f; p->lp = 127.0f; p->drive = 0.0f;
}

/* ---------------------------------------------------------------- FFT (radix 2, double) --- */
static void fft(double *re, double *im, int n)
{
    int i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2.0 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j), xr = re[k + j + m / 2], xi = im[k + j + m / 2];
                double tr = wr * xr - wi * xi, ti = wr * xi + wi * xr;
                re[k + j + m / 2] = re[k + j] - tr; im[k + j + m / 2] = im[k + j] - ti;
                re[k + j] += tr; im[k + j] += ti;
            }
    }
}
/* power spectrum of x[0..n) with a Blackman-Harris window into ps[n/2] */
static void power(const float *x, int n, double *ps)
{
    double *re = calloc((size_t)n, sizeof(double)), *im = calloc((size_t)n, sizeof(double));
    int i;
    for (i = 0; i < n; i++) {
        double t = 2.0 * M_PI * i / n;
        re[i] = x[i] * (0.35875 - 0.48829 * cos(t) + 0.14128 * cos(2 * t) - 0.01168 * cos(3 * t));
    }
    fft(re, im, n);
    for (i = 0; i < n / 2; i++) ps[i] = re[i] * re[i] + im[i] * im[i];
    free(re); free(im);
}
static float tone(int i, int k, int N) { return (float)sin(2.0 * M_PI * (double)(((long)i * k) % N) / N); }
static double db(double p) { return 10.0 * log10(p + 1e-30); }

/* ---------------------------------------------------------------- 1, 2: timing and decay --- */
/* SKANK heads 2 and 3 steps, feedback from head 3: echoes at 2D, 3D, 5D, 6D, 8D, 9D ... (repeats of the 3D head) */
static void a_timing(float t, dd_params_t *p) { (void)t; dd_apply_pattern(p, 1); p->time = 0.15f; p->feedback = 0.5f; }
static void a_timing_wf(float t, dd_params_t *p) { a_timing(t, p); p->wow = 0.5f; p->flutter = 0.5f; }
static double peak_near(const float *y, double at, int win, double *amp)
{
    int i0 = (int)at - win, i, bi = i0;
    double best = 0.0, a, b, c, d;
    for (i = i0; i <= (int)at + win; i++) if (fabs(y[i]) > best) { best = fabs(y[i]); bi = i; }
    a = fabs(y[bi - 1]); b = fabs(y[bi]); c = fabs(y[bi + 1]);
    d = (a - 2 * b + c) != 0.0 ? 0.5 * (a - c) / (a - 2 * b + c) : 0.0;     /* parabolic refinement */
    {   double e = 0; int j;                                          /* level: the echo's energy over +-win */
        for (j = (int)at - win; j <= (int)at + win; j++) e += (double)y[j] * y[j];
        *amp = sqrt(e); }
    (void)b;
    return bi + d;
}
static void m_timing(void)
{
    int c, k;
    const int mult[6] = {2, 3, 5, 6, 8, 9}, n = DD_FS * 3;
    printf("\n## 1. Echo timing and 2. amplitude decay\n\n");
    printf("Unit impulse (0.5) at t = 0.1 s; SKANK heads (2, 3 steps of D = 150 ms = 6615 samples), FEEDBACK 0.5 from the "
           "3-step head, filters open, DRIVE 0, no wow/flutter. Expected echoes at k*D (k = 2,3,5,6,8,9) plus the rate "
           "converters' constant latency (column *conv*). Error = measured peak (parabolic) - expected, in 44.1 kHz samples. "
           "Decay = energy (+-60 samples) of the 6D and 9D repeats re the previous repeat of that head (ideal: 20 log10 0.5 = -6.02 dB; the filters at their open extremes still take a little).\n\n");
    printf("| config | conv lat | err 2D | err 3D | err 5D | err 6D | err 8D | err 9D | 6D/3D dB | 9D/6D dB |\n|---|---|---|---|---|---|---|---|---|---|\n");
    for (c = 0; c < NCF; c++) {
        double amp[6], t0 = 0.1 * DD_FS;
        if (CF[c].algo == DD_ALGO_DIGITAL) continue;
        memset(xin, 0, sizeof(float) * (size_t)n); xin[(int)t0] = 0.5f;
        neutral(&base); run(&CF[c], n, a_timing, 1.5f);
        printf("| %s | %.1f |", CF[c].name, (double)S.conv_lat);
        for (k = 0; k < 6; k++) {
            double exp_ = t0 + mult[k] * 0.15 * DD_FS + S.conv_lat, pk = peak_near(ol, exp_, 60, &amp[k]);
            printf(" %+.2f |", pk - exp_);
        }
        printf(" %.2f | %.2f |\n", 20 * log10(amp[3] / amp[1]), 20 * log10(amp[5] / amp[3]));
    }
    /* wow / flutter: timing spread of the echoes (the modulation is intended) */
    printf("\nWith WOW 0.5, FLUTTER 0.5 (TAPE): echo-time deviation from nominal, 44.1 kHz samples (ms):\n\n| config | 2D | 3D | 5D | 6D | 8D | 9D |\n|---|---|---|---|---|---|---|\n");
    for (c = 5; c < NCF; c++) {
        double amp, t0 = 0.1 * DD_FS;
        memset(xin, 0, sizeof(float) * (size_t)n); xin[(int)t0] = 0.5f;
        neutral(&base); run(&CF[c], n, a_timing_wf, 1.5f);
        printf("| %s |", CF[c].name);
        for (k = 0; k < 6; k++) {
            double exp_ = t0 + mult[k] * 0.15 * DD_FS + S.conv_lat, pk = peak_near(ol, exp_, 200, &amp);
            printf(" %+.1f (%.2f) |", pk - exp_, (pk - exp_) / 44.1);
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------------- 3: spectra per repeat --- */
static float sp_lp = 92.0f, sp_hp = 42.0f, sp_res = 0.3f, sp_drive = 0.0f;
static void a_spec(float t, dd_params_t *p)
{
    (void)t; dd_apply_pattern(p, 0); p->time = 0.25f; p->feedback = 0.85f;
    p->lp = sp_lp; p->hp = sp_hp; p->res = sp_res; p->drive = sp_drive;
}
static void m_spectra(void)
{
    static const double band[8] = {63, 125, 250, 500, 1000, 2000, 4000, 8000};
    int c, k, j, n = DD_FS * 4;
    const int W = 4096;
    double ps[2048];
    printf("\n## 3. Spectral evolution on successive repeats\n\n");
    printf("White-noise burst (10 ms, -12 dBFS) through one head (D = 250 ms), FEEDBACK 0.85, HP knob %.0f (%.0f Hz), "
           "LP knob %.0f (%.0f Hz), RES %.1f, DRIVE %.1f. Octave-band levels (dB) of each repeat relative to the first "
           "repeat's band level; a 4096-sample Blackman-Harris window centred on each echo.\n\n",
           (double)sp_hp, (double)dd_knob_hz(sp_hp), (double)sp_lp, (double)dd_knob_hz(sp_lp), (double)sp_res, (double)sp_drive);
    for (c = 0; c < NCF; c++) {
        double ref[8];
        if (CF[c].aa == DD_AA_CHEAP) continue;
        memset(xin, 0, sizeof(float) * (size_t)n);
        sc_rng = 99u;
        for (k = 0; k < 441; k++) xin[2205 + k] = 0.25f * sc_noise();
        neutral(&base); run(&CF[c], n, a_spec, 0.5f);
        printf("**%s**\n\n| repeat | 63 | 125 | 250 | 500 | 1k | 2k | 4k | 8k | total dB |\n|---|---|---|---|---|---|---|---|---|---|\n", CF[c].name);
        for (k = 1; k <= 8; k++) {
            int ctr = 2205 + 220 + (int)(k * 0.25 * DD_FS + S.conv_lat), i;
            double e[8] = {0}, tot = 0;
            float *seg = malloc(sizeof(float) * (size_t)W);
            for (i = 0; i < W; i++) seg[i] = ol[ctr - W / 2 + i];
            power(seg, W, ps); free(seg);
            for (i = 1; i < W / 2; i++) {
                double f = (double)i * DD_FS / W;
                tot += ps[i];
                for (j = 0; j < 8; j++) if (f >= band[j] / M_SQRT2 && f < band[j] * M_SQRT2) e[j] += ps[i];
            }
            if (k == 1) { for (j = 0; j < 8; j++) ref[j] = e[j]; }
            if (k == 1 || k == 2 || k == 4 || k == 8) {
                printf("| %d |", k);
                for (j = 0; j < 8; j++) printf(" %.1f |", db(e[j]) - db(ref[j]));
                printf(" %.1f |\n", db(tot));
            }
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------------- 4: stability --- */
static float st_res, st_lp, st_drive, st_fb;
static void a_stab(float t, dd_params_t *p)
{
    (void)t; dd_apply_pattern(p, 3); p->time = 0.12f; p->feedback = st_fb; p->res = st_res; p->lp = st_lp; p->hp = 30.0f;
    p->drive = st_drive; p->wow = 0.3f; p->flutter = 0.3f;
}
static uint32_t fz;
static float frand(void) { fz = fz * 1664525u + 1013904223u; return (float)(fz >> 8) * (1.0f / 16777216.0f); }
static void a_fuzz(float t, dd_params_t *p)
{
    static int k;
    static dd_params_t q;
    (void)t;
    if (k++ % 7 == 0) {                         /* new random settings every 7 blocks (20 ms) */
        int i;
        dd_apply_pattern(&q, (int)(frand() * 5.99f));
        q.time = 0.02f + 0.3f * frand(); q.feedback = 1.2f * frand(); q.hp = 127.0f * frand(); q.lp = 127.0f * frand();
        q.res = frand(); q.drive = frand(); q.wow = frand(); q.flutter = frand(); q.send = frand() < 0.5f ? 1.0f : 0.0f;
        q.wet = frand(); q.dry = frand(); q.glide = frand() < 0.5f ? 0.0f : 0.3f * frand();
        for (i = 0; i < DD_TAPS; i++) q.pan[i] = 2.0f * frand() - 1.0f;
    }
    *p = q;
}
static void m_stability(void)
{
    static const float cases[][4] = {   /* fb, res, lp, drive */
        {1.2f, 0.0f, 127.0f, 0.0f}, {1.2f, 1.0f, 127.0f, 0.0f}, {1.2f, 1.0f, 60.0f, 1.0f}, {1.2f, 1.0f, 0.0f, 1.0f},
        {1.2f, 0.0f, 90.0f, 1.0f}, {0.98f, 0.5f, 90.0f, 0.3f}};
    int c, k, i, n = DD_FS * 60;
    printf("\n## 4. Feedback stability\n\n60 s per case. Input: a 5 ms noise burst, then continuous low-level noise (-60 dBFS) to "
           "seed regeneration. TRIPLE heads, D = 120 ms, WOW/FLUTTER 0.3. *peak* = output peak over the run; *last-10 s RMS*; "
           "*finite* = no NaN/Inf; *DC* = mean of the last 10 s.\n\n");
    printf("| config | FEEDBACK | RES | LP knob | DRIVE | peak dBFS | last-10 s RMS dBFS | DC | finite |\n|---|---|---|---|---|---|---|---|---|\n");
    sc_rng = 5u;
    for (i = 0; i < n; i++) xin[i] = 0.001f * sc_noise();
    add_click(xin, n, 0.1f, 0.7f);
    for (c = 0; c < NCF; c++) {
        if (CF[c].aa == DD_AA_CHEAP || CF[c].R == 4) continue;
        for (k = 0; k < (int)(sizeof cases / sizeof cases[0]); k++) {
            double pk = 0, rms = 0, dc = 0;
            int fin = 1;
            st_fb = cases[k][0]; st_res = cases[k][1]; st_lp = cases[k][2]; st_drive = cases[k][3];
            if (CF[c].algo == DD_ALGO_DIGITAL && k > 0) continue;
            neutral(&base); run(&CF[c], n, a_stab, 0.5f);
            for (i = 0; i < n; i++) {
                if (!isfinite(ol[i]) || !isfinite(orr[i])) fin = 0;
                if (fabs(ol[i]) > pk) pk = fabs(ol[i]);
                if (fabs(orr[i]) > pk) pk = fabs(orr[i]);
                if (i >= n - DD_FS * 10) { rms += (double)ol[i] * ol[i]; dc += ol[i]; }
            }
            printf("| %s | %.2f | %.1f | %.0f | %.1f | %.2f | %.1f | %.1e | %s |\n", CF[c].name, (double)st_fb, (double)st_res,
                   (double)st_lp, (double)st_drive, 20 * log10(pk + 1e-12), 10 * log10(rms / (DD_FS * 10.0) + 1e-20),
                   dc / (DD_FS * 10.0), fin ? "yes" : "NO");
        }
    }
    /* parameter fuzz */
    printf("\nParameter fuzz: 60 s of random settings every 20 ms (all parameters, FEEDBACK up to 1.2, random pattern, TIME "
           "20..320 ms), input pink-ish noise bursts.\n\n| config | peak dBFS | finite |\n|---|---|---|\n");
    for (i = 0; i < n; i++) xin[i] = (i / 4410) % 3 == 0 ? 0.3f * sc_noise() : 0.0f;
    for (c = 0; c < NCF; c++) {
        double pk = 0;
        int fin = 1;
        fz = 1u;
        neutral(&base); run(&CF[c], n, a_fuzz, 0.4f);
        for (i = 0; i < n; i++) {
            if (!isfinite(ol[i]) || !isfinite(orr[i])) fin = 0;
            if (fabs(ol[i]) > pk) pk = fabs(ol[i]);
            if (fabs(orr[i]) > pk) pk = fabs(orr[i]);
        }
        printf("| %s | %.2f | %s |\n", CF[c].name, 20 * log10(pk), fin ? "yes" : "NO");
    }
}

/* ---------------------------------------------------------------- 5: saturation and aliasing --- */
static float sa_drive;
static int sa_route;
static void a_sat(float t, dd_params_t *p) { (void)t; dd_apply_pattern(p, 0); p->time = 0.01f; p->feedback = 0.0f; p->drive = sa_drive; if (sa_route) { p->route = 1; p->lp = 100.0f; p->hp = 30.0f; } }
/* fundamental at bin kf of N: harmonics, aliases (inharmonic bins), noise */
static void tone_analysis(const float *y, int N, int kf, double *thd, double *alias, double *h, int nh)
{
    double *ps = malloc(sizeof(double) * (size_t)(N / 2)), pf = 0, ph = 0, pa = 0;
    int i, k;
    power(y, N, ps);
    for (i = 1; i < N / 2; i++) {
        int near_h = 0;
        for (k = 1; k * kf < N / 2 + 8; k++) if (abs(i - k * kf) <= 6) { near_h = k; break; }
        if (near_h == 1) pf += ps[i];
        else if (near_h > 1) { ph += ps[i]; if (near_h <= nh) h[near_h - 1] += ps[i]; }
        else if (i > 6) pa += ps[i];
    }
    *thd = db(ph / pf); *alias = db(pa / pf);
    for (k = 1; k < nh; k++) h[k] = db(h[k] / pf);
    free(ps);
}
static void m_saturation(void)
{
    const int N = 16384, n = N + 44160;
    static const float lev[4] = {-20, -12, -6, 0};
    static const float drv[3] = {0.0f, 0.5f, 1.0f};
    int c, d, l, i;
    printf("\n## 5. Saturation characteristics and aliasing\n\nOne head (10 ms), FEEDBACK 0 (one pass through the record "
           "saturator), wet only. Sine at bin 384/16384 (1033.6 Hz). THD = harmonics 2..n re fundamental; *alias+noise* = "
           "all inharmonic bins re fundamental (aliasing, quantisation, converter images).\n\n");
    printf("| config | DRIVE | level dBFS | THD dB | H2 | H3 | H5 | alias+noise dB |\n|---|---|---|---|---|---|---|---|\n");
    for (c = 1; c < NCF; c++) {
        if (CF[c].aa == DD_AA_CHEAP || CF[c].R != 1) continue;
        for (d = 0; d < 3; d++)
            for (l = 0; l < 4; l++) {
                double thd, al, h[9] = {0};
                float a = powf(10.0f, lev[l] / 20.0f);
                for (i = 0; i < n; i++) xin[i] = a * tone(i, 384, N);
                sa_drive = drv[d];
                neutral(&base); run(&CF[c], n, a_sat, 0.2f);
                tone_analysis(ol + DD_FS, N, 384, &thd, &al, h, 9);
                printf("| %s | %.1f | %.0f | %.1f | %.1f | %.1f | %.1f | %.1f |\n", CF[c].name, (double)drv[d], (double)lev[l],
                       thd, h[1], h[2], h[4], al);
            }
    }
    printf("\nAliasing: sine at bin 1486/16384 (4000 Hz; in the 44.1 kHz loop H7 folds to 16.1 kHz, H9 to 8.1 kHz; in the "
           "22.05 kHz loop H3 folds to 10.05 kHz). *alias+noise* re fundamental, dB. LITE: plain cubic; TAPE: pre-emphasis "
           "+ ADAA cubic + de-emphasis.\n\n| config | DRIVE | -12 dBFS | -6 dBFS | 0 dBFS |\n|---|---|---|---|---|\n");
    for (c = 1; c < 2 * NCF; c++) {
        sa_route = c >= NCF;
        if (c == NCF || CF[c % NCF].aa == DD_AA_CHEAP || CF[c % NCF].R == 4) continue;
        for (d = 1; d < 3; d++) {
            printf("| %s%s | %.1f |", CF[c % NCF].name, sa_route ? ", record route LP 4.7k" : "", (double)drv[d]);
            for (l = 1; l < 4; l++) {
                double thd, al, h[9] = {0};
                float a = powf(10.0f, lev[l] / 20.0f);
                for (i = 0; i < n; i++) xin[i] = a * tone(i, 1486, N);
                sa_drive = drv[d];
                neutral(&base); run(&CF[c % NCF], n, a_sat, 0.2f);
                tone_analysis(ol + DD_FS, N, 1486, &thd, &al, h, 9);
                printf(" %.1f |", al);
            }
            printf("\n");
        }
    }
}

/* ---------------------------------------------------------------- 5b: rate converters --- */
static void m_converters(void)
{
    const int N = 16384, n = N + 44160;
    static const int bins[] = {372, 1486, 2600, 3343, 3715, 4087, 4458, 5574};   /* 1, 4, 7, 9, 10, 11, 12, 15 kHz */
    int c, b, i;
    printf("\n## 5b. Reduced-rate lines: pass band, aliasing and images\n\nThe plain (DIGITAL) path, one head 10 ms, FEEDBACK 0, no "
           "loop filters, -6 dBFS sine, so only the converters act. *gain* = output fundamental re input (dB; the head level 0.8 and centre pan take -4.9 dB); *alias* = everything else re fundamental (dB): "
           "aliases from the decimator, images from the interpolator, quantisation.\n\n");
    printf("| config |");
    for (b = 0; b < 8; b++) printf(" %.1f kHz gain / alias |", bins[b] * 44.1 / 16384.0);
    printf("\n|---|---|---|---|---|---|---|---|---|\n");
    for (c = 0; c < 5; c++) {
        printf("| %s |", CONV[c].name);
        for (b = 0; b < 8; b++) {
            double *ps = malloc(sizeof(double) * (size_t)(N / 2)), *pi = malloc(sizeof(double) * (size_t)(N / 2)), pf = 0, pin = 0, pa = 0;
            for (i = 0; i < n; i++) xin[i] = 0.5f * tone(i, bins[b], N);
            sa_drive = 0.0f;
            neutral(&base); run(&CONV[c], n, a_sat, 0.2f);
            power(ol + DD_FS, N, ps); power(xin + DD_FS, N, pi);
            for (i = 1; i < N / 2; i++) {
                if (abs(i - bins[b]) <= 6) { pf += ps[i]; pin += pi[i]; }
                else if (i > 6) pa += ps[i];
            }
            printf(" %.1f / %.0f |", db(pf / pin), db(pa / (pf + 1e-30)));
            free(ps); free(pi);
        }
        printf("\n");
    }
}

/* ---------------------------------------------------------------- 6: noise --- */
static float no_fb;
static void a_noise(float t, dd_params_t *p) { (void)t; dd_apply_pattern(p, 0); p->time = 0.2f; p->feedback = no_fb; p->lp = 100.0f; p->hp = 30.0f; }
static void m_noise(void)
{
    const int N = 16384, n = 2756 * DD_BLOCK;
    int c, f, i;
    static const float fbs[2] = {0.0f, 0.9f};
    printf("\n## 6. Noise\n\nContinuous 1033.6 Hz sine at -20 dBFS, one head (200 ms), LP knob 100, HP knob 30; the last "
           "16384 samples. SINAD-style: everything but the fundamental and its harmonics, re fundamental. The line stores "
           "int16 (no dither). *idle* = output RMS 4 s after a burst stops, FEEDBACK 0.9.\n\n");
    printf("| config | FEEDBACK 0: noise dB | FEEDBACK 0.9: noise dB | idle (dBFS) |\n|---|---|---|---|\n");
    for (c = 1; c < NCF; c++) {
        if (CF[c].aa == DD_AA_CHEAP) continue;
        printf("| %s |", CF[c].name);
        for (f = 0; f < 2; f++) {
            double thd, al, h[9] = {0};
            for (i = 0; i < n; i++) xin[i] = 0.1f * tone(i, 384, N);
            no_fb = fbs[f];
            neutral(&base); run(&CF[c], n, a_noise, 0.3f);
            tone_analysis(ol + n - N, N, 384, &thd, &al, h, 9);
            printf(" %.1f |", al);
        }
        {   double r = 0;
            memset(xin, 0, sizeof(float) * (size_t)n); add_click(xin, n, 0.1f, 0.5f);
            no_fb = 0.9f; neutral(&base); run(&CF[c], n, a_noise, 0.3f);
            for (i = n - DD_FS; i < n; i++) r += (double)ol[i] * ol[i];
            printf(" %.1f |\n", 10 * log10(r / DD_FS + 1e-30)); }
    }
}

/* ---------------------------------------------------------------- 7: parameter-change artifacts --- */
static float pc_glide;
static int pc_mode;
static void a_pc(float t, dd_params_t *p)
{
    dd_apply_pattern(p, 0); p->feedback = 0.0f; p->lp = 110.0f; p->hp = 20.0f; p->glide = pc_glide;
    p->time = 0.2f;
    if (pc_mode == 0) p->time = t < 1.0f ? 0.2f : 0.2473f;                       /* TIME change */
    if (pc_mode == 1) { p->feedback = 0.5f; p->lp = t < 1.0f ? 60.0f : t < 2.0f ? 60.0f + 50.0f * (t - 1.0f) : 110.0f; }   /* LP sweep */
    if (pc_mode == 2) { dd_apply_pattern(p, t < 1.0f ? 0 : 3); p->time = 0.1f; }   /* pattern change */
    if (pc_mode == 3) p->feedback = t < 1.0f ? 0.0f : 0.9f;                      /* feedback jump */
}
/* click metric: the peak of the content above 5 kHz (two 2nd-order Butterworth high-passes) within +-win of the event,
 * dBFS. A 220 Hz sine has none: whatever is there is the transition's splatter */
static double click_db(const float *y, int at, int win, int ref_at)
{
    const double w0 = tan(M_PI * 5000.0 / DD_FS), k = M_SQRT2, a0 = 1.0 / (1.0 + k * w0 + w0 * w0);
    double z[2][2] = {{0}}, m = 0;
    int i, st;
    (void)ref_at;
    for (i = at - win - 4096; i < at + win; i++) {
        double x = y[i];
        for (st = 0; st < 2; st++) {   /* TPT SVF high-pass */
            double hp = (x - (k + w0) * z[st][0] - z[st][1]) * a0, bp = w0 * hp + z[st][0], lp = w0 * bp + z[st][1];
            z[st][0] = w0 * hp + bp; z[st][1] = w0 * bp + lp; x = hp;
        }
        if (i >= at - win && fabs(x) > m) m = fabs(x);
    }
    return 20 * log10(m + 1e-12);
}
static void m_params(void)
{
    const int n = 1034 * DD_BLOCK;
    int c, i;
    printf("\n## 7. Parameter-change artifacts\n\nInput: continuous 220 Hz sine at -12 dBFS. *Click* = peak of the output content above 5 kHz "
           "within +-2048 samples of the event (or of its echo), dBFS: the sine has none, so this is the transition's splatter "
           "(below about -70 dBFS is inaudible in practice). TIME 200 -> 247.3 ms at t = 1 s: with GLIDE 0 (a jump) "
           "and GLIDE 0.12 s (tape-style slew, a pitch bend instead). *glide max bend* is computed, not measured: 1200 log2(1 / (1 - step/tau)) cents, downward.\n\n");
    printf("| config | TIME jump | TIME glide 0.12 s | glide max bend (cents, computed) | LP sweep 60->110 in 1 s | pattern SINGLE->TRIPLE | FEEDBACK 0->0.9 |\n|---|---|---|---|---|---|---|\n");
    for (i = 0; i < n; i++) xin[i] = 0.25f * (float)sin(2.0 * M_PI * 220.0 * i / DD_FS);
    for (c = 1; c < NCF; c++) {
        double r[5], bend;
        if (CF[c].aa == DD_AA_CHEAP) continue;
        neutral(&base); pc_mode = 0; pc_glide = 0.0f; run(&CF[c], n, a_pc, 0.4f); r[0] = click_db(ol, (int)(1.0 * DD_FS) + 2 * 128, 2048, (int)(2.5 * DD_FS));
        neutral(&base); pc_glide = 0.12f; run(&CF[c], n, a_pc, 0.4f); r[1] = click_db(ol, (int)(1.0 * DD_FS) + 2 * 128, 2048, (int)(2.5 * DD_FS));
        bend = 1200.0 * log2(1.0 / (1.0 - 0.0473 / 0.12));  /* computed: one-pole slew, max d'(t) = step / tau; pitch falls */
        neutral(&base); pc_mode = 1; run(&CF[c], n, a_pc, 0.4f); r[2] = click_db(ol, (int)(1.5 * DD_FS), 4096, (int)(2.6 * DD_FS));
        neutral(&base); pc_mode = 2; run(&CF[c], n, a_pc, 0.4f); r[3] = click_db(ol, (int)(1.0 * DD_FS) + (int)(0.1 * DD_FS), 2048, (int)(2.5 * DD_FS));
        neutral(&base); pc_mode = 3; run(&CF[c], n, a_pc, 0.4f); r[4] = click_db(ol, (int)(1.2 * DD_FS) + 128, 2048, (int)(2.5 * DD_FS));
        printf("| %s | %.1f | %.1f | %.0f | %.1f | %.1f | %.1f |\n", CF[c].name, r[0], r[1], bend, r[2], r[3], r[4]);
    }
}

/* ---------------------------------------------------------------- 8: dry / wet, bypass --- */
static int dw_mode;
static void a_dw(float t, dd_params_t *p)
{
    dd_apply_pattern(p, 1); p->time = 0.15f; p->feedback = 0.7f; p->lp = 95.0f; p->hp = 40.0f; p->dry = 1.0f;
    p->wet = dw_mode == 0 ? 0.0f : (t < 1.0f ? 1.0f : 0.0f);
    if (dw_mode == 2) { p->wet = 1.0f; p->send = t < 1.0f ? 1.0f : 0.0f; }
}
static void m_drywet(void)
{
    const int n = 1034 * DD_BLOCK;
    int c, i;
    printf("\n## 8. Dry / wet and bypass\n\nInput: 220 Hz sine -12 dBFS. *WET 0 exact* = max |out - in| with "
           "WET 0, DRY 1 (bit-exact dry path, zero latency). *Wet kill* / *send cut* click: the same >5 kHz peak, dBFS. *Wet kill* = WET 1 -> 0 at t = 1 s (a one-pole a block, ~8 ms). "
           "*Send cut (tails) click* = SEND 1 -> 0 at t = 1 s, the echoes continue (\"bypass with tails\").\n\n");
    printf("| config | WET 0 exact | wet kill click dBFS | send cut click dBFS |\n|---|---|---|---|\n");
    for (i = 0; i < n; i++) xin[i] = 0.25f * (float)sin(2.0 * M_PI * 220.0 * i / DD_FS);
    for (c = 0; c < NCF; c++) {
        double mx = 0, k1, k2;
        if (CF[c].aa == DD_AA_CHEAP) continue;
        neutral(&base); dw_mode = 0; run(&CF[c], n, a_dw, 0.5f);
        for (i = 0; i < n; i++) { double d = fabs(ol[i] - xin[i]); if (d > mx) mx = d; }
        neutral(&base); dw_mode = 1; run(&CF[c], n, a_dw, 0.5f);
        for (i = 0; i < n; i++) ol[i] -= xin[i];          /* the wet part only */
        k1 = click_db(ol, (int)(1.0 * DD_FS) + 1024, 2048, (int)(0.8 * DD_FS));
        neutral(&base); dw_mode = 2; run(&CF[c], n, a_dw, 0.5f);
        for (i = 0; i < n; i++) ol[i] -= xin[i];
        k2 = click_db(ol, (int)(1.0 * DD_FS) + 128, 512, (int)(0.8 * DD_FS));
        printf("| %s | %.1e | %.1f | %.1f |\n", CF[c].name, mx, k1, k2);
    }
}

int main(int argc, char **argv)
{
    const char *only = argc > 1 ? argv[1] : "";
    printf("# DUB-TAPE prototype measurements\n\nGenerated by `build/measure` (tools/measure.c). Host analysis; 44.1 kHz, "
           "128-frame blocks, control tick 32 frames, int16 line storage.\n");
    if (!*only || strchr(only, '1')) m_timing();
    if (!*only || strchr(only, '3')) m_spectra();
    if (!*only || strchr(only, '4')) m_stability();
    if (!*only || strchr(only, '5')) { m_saturation(); m_converters(); }
    if (!*only || strchr(only, '6')) m_noise();
    if (!*only || strchr(only, '7')) m_params();
    if (!*only || strchr(only, '8')) m_drywet();
    return 0;
}
