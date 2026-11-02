/* round 13: the DUB renders through the firmware's own path. Each scene's settings are TRACK IDS (the ten of dub_fields.h; an old project's through dub_convert_old), mapped
 * by the firmware's dub_map into dd_params_t and run through the firmware's dubdelay.c, block by block, with the throw and the speed bend as the triggers and the SPEED id.
 * Writes the WAVs and a raw float file of each (OUTDIR/NAME.wav, NAME.f32). render_scratch.c renders the same scenes with the parameters written out by hand through the
 * scratch kernel (docs/orangecat/dub_r12_kernel/dubdelay.c, from round 12); the two sets of .f32 are compared with cmp (identical).
 * Build (repo root):  cc -O1 -w -ffp-contract=off -DUI_TEST_NO_MAIN=1 -Ibuild/gen -Ifirmware/src -Itests docs/orangecat/dub_r12_kernel/render_fw.c -o render_fw -lm && ./render_fw OUTDIR */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "dubin.h"
static int16_t *blk[16];
typedef void (*ctl_t)(double t, const float *mask, uint32_t b);
static void render(const char *dir, const char *name, const float *in, const float *mask, uint32_t n, ctl_t ctl)
{
    dd_t s; dd_params_t p; static int16_t pool[16 * 2048]; static int16_t spb[DD_SP_BUF + 8];
    for (int k = 0; k < 16; k++) blk[k] = pool + k * 2048;
    memset(pool, 0, sizeof pool);
    dk_init(&s, 2, blk, 16); dk_spring_attach(&s, spb);
    float *ol = calloc(n, 4), *or_ = calloc(n, 4), *st = calloc(2 * n, 4);
    for (uint32_t b = 0; b + 32 <= n; b += 32) {
        float x[32], l[32], r[32];
        for (int i = 0; i < 32; i++) x[i] = in[b + i];
        if (ctl) ctl(b / (double)RFS, mask, b);
        dub_map(0, &trk[0], &p);
        dk_process(&s, &p, x, l, r);
        for (int i = 0; i < 32; i++) { ol[b + i] = in[b + i] + l[i]; or_[b + i] = in[b + i] + r[i]; st[2 * (b + i)] = ol[b + i]; st[2 * (b + i) + 1] = or_[b + i]; }
    }
    char path[512]; snprintf(path, sizeof path, "%s/%s.wav", dir, name); wavw(path, ol, or_, n);
    snprintf(path, sizeof path, "%s/%s.f32", dir, name); FILE *f = fopen(path, "wb"); fwrite(st, 4, 2 * n, f); fclose(f);
    free(ol); free(or_); free(st);
}
static void ids(int16_t echo, int tl, int tr, int ping, int fb, int filt, int send, int drive, int wow, int spring, int speed)
{
    int16_t *p = trk[0].p;
    p[P_DUB_ECHO] = echo; p[P_DUB_TIML] = (int16_t)dub_t_val(tl); p[P_DUB_TIMR] = (int16_t)dub_t_val(tr);
    p[P_DUB_MODE] = DUB_MODE_MARK | 1; dub_put_field(p, DUBF_PING, ping);
    p[P_DUB_FDBK] = (int16_t)fb; p[P_DUB_FILT] = (int16_t)filt; p[P_DUB_SEND] = (int16_t)send;
    dub_put_field(p, DUBF_DRIVE, drive); dub_put_field(p, DUBF_WOW, wow); p[P_DUB_SPRG] = (int16_t)spring; p[P_DUB_SPD] = (int16_t)speed;
}
static void throw_ctl(double t, const float *mask, uint32_t b) { dub_trig[0][0] = mask[b] > 0 ? 127 : 0; }
static void bend_ctl(double t, const float *mask, uint32_t b)
{
    dub_trig[0][0] = mask[b] > 0 ? 127 : 0;
    if (t >= 6.5 && t < 7.0) trk[0].p[P_DUB_SPD] = (int16_t)(64 + 48.0 * ((t - 6.5) / 0.5));
    else if (t >= 7.0 && t < 8.5) trk[0].p[P_DUB_SPD] = 112;
    else if (t >= 8.5) trk[0].p[P_DUB_SPD] = 64;
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t n = (uint32_t)(14.0 * RFS) / 32 * 32; float *x = calloc(n, 4), *m = calloc(n, 4);
    ui_power_on();
    song.g[G_BPM] = 75;                                  /* the scenes' tempo (dubin.h BPM) */
    gen_skank(x, n);
    /* an old project's SKANK (LEN 3, PTRN SKANK, SPRP OFF, SPACE 24 = 1/8, FEEDBACK 55, SEND 127, TONE 60) loaded: the nearest new values */
    memset(trk[0].p, 0, sizeof trk[0].p);
    trk[0].p[P_DUBO_LEN] = 3; trk[0].p[P_DUBO_PTRN] = 2; trk[0].p[P_DUBO_SPRP] = 0; trk[0].p[P_DUBO_SPACE] = 24;
    trk[0].p[P_JRES0] = 55; trk[0].p[P_JRES1] = 127; trk[0].p[P_DUBO_TONE] = 60;
    dub_convert_old(trk[0].p);
    memset(dub_trig, 0, sizeof dub_trig);
    render(dir, "after-skank-converted", x, 0, n, 0);
    /* the new voices: 1/8 dotted and 1/4 dotted, PING, FEEDBACK 62, DRIVE 8, WOW 3, SPRING 28 */
    ids(100, 7, 10, 1, 62, 60, 127, 8, 3, 28, 64);
    render(dir, "after-skank-ping", x, 0, n, 0);
    gen_throw(x, m, n);
    /* an old SLAP (heads 1 and 3 of 1/8; FEEDBACK 75, SEND 0, TONE 30) loaded; the throw key held for the snare */
    memset(trk[0].p, 0, sizeof trk[0].p);
    trk[0].p[P_DUBO_LEN] = 3; trk[0].p[P_DUBO_PTRN] = 0; trk[0].p[P_DUBO_SPRP] = 0; trk[0].p[P_DUBO_SPACE] = 24;
    trk[0].p[P_JRES0] = 75; trk[0].p[P_JRES1] = 0; trk[0].p[P_DUBO_TONE] = 30;
    dub_convert_old(trk[0].p);
    render(dir, "after-throw-converted", x, m, n, throw_ctl);
    /* the new: PING, FILTER closing on the repeats, the speed turned up at 6.5 s and back to the centre at 8.5 s */
    ids(100, 6, 9, 1, 82, 34, 0, 10, 2, 24, 64);
    render(dir, "after-throw-bend", x, m, n, bend_ctl);
    puts("rendered through the firmware's dub_map and dubdelay.c");
    return 0;
}
