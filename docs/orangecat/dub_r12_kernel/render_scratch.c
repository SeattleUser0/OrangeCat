/* round 13: the same four scenes as render_fw.c, with the parameters written out by hand (the musical values, not track ids) through the SCRATCH kernel of round 12 (dubdelay.c
 * in this folder). Writes OUTDIR/NAME.f32 for cmp against render_fw's. Build (this folder): cc -O1 -w -ffp-contract=off render_scratch.c -o render_scratch -lm */
#include "dubdelay.c"
#include "dubin.h"
static int16_t *blk[16];
typedef struct { float tl, tr; int ping; float fb, filt, send, drive, wow, spring, speed; } set_t;
typedef void (*ctl_t)(double t, set_t *s, const float *mask, uint32_t b);
static void render(const char *dir, const char *name, const float *in, const float *mask, uint32_t n, set_t base, ctl_t ctl)
{
    dd_t s; dd_params_t p; static int16_t pool[16 * 2048]; static int16_t spb[DD_SP_BUF + 8];
    for (int k = 0; k < 16; k++) blk[k] = pool + k * 2048;
    memset(pool, 0, sizeof pool);
    dk_init(&s, 2, blk, 16); dk_spring_attach(&s, spb);
    float *st = calloc(2 * n, 4);
    for (uint32_t b = 0; b + 32 <= n; b += 32) {
        float x[32], l[32], r[32]; set_t c = base;
        for (int i = 0; i < 32; i++) x[i] = in[b + i];
        if (ctl) ctl(b / (double)RFS, &c, mask, b);
        memset(&p, 0, sizeof p);
        p.time_l = c.tl; p.time_r = c.tr; p.ping = c.ping;
        p.speed = expf((c.speed - 64.0f) * (0.69314718f / 64.0f)); p.wow = c.wow / 11.0f;       /* SPEED 0 .. 127 (64 normal), WOW 0 .. 11 */
        p.feedback = c.fb * 0.01f; p.send = c.send / 127.0f;
        if (c.filt < 64) { p.lp = 30.0f + 1.4f * c.filt; p.hp = 15.0f; } else { p.lp = 119.0f; p.hp = 15.0f + 1.6f * (c.filt - 64); }
        p.res = 0.3f; p.drive = c.drive / 15.0f; p.wet = 1.0f; p.dry = 0.0f; p.glide = 0.12f; p.route = 1; p.lim_ceil = 0.891f;
        p.spring = 2; p.sp_mix = c.spring / 127.0f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f; p.sp_in = 0.3f;
        dk_process(&s, &p, x, l, r);
        for (int i = 0; i < 32; i++) { st[2 * (b + i)] = in[b + i] + l[i]; st[2 * (b + i) + 1] = in[b + i] + r[i]; }
    }
    char path[512]; snprintf(path, sizeof path, "%s/%s.f32", dir, name); FILE *f = fopen(path, "wb"); fwrite(st, 4, 2 * n, f); fclose(f);
    free(st);
}
static void throw_ctl(double t, set_t *s, const float *mask, uint32_t b) { s->send = mask[b] > 0 ? 127.0f : 0.0f; }
static void bend_ctl(double t, set_t *s, const float *mask, uint32_t b)
{
    s->send = mask[b] > 0 ? 127.0f : 0.0f;
    if (t >= 6.5 && t < 7.0) s->speed = (float)(int)(64 + 48.0 * ((t - 6.5) / 0.5));
    else if (t >= 7.0 && t < 8.5) s->speed = 112.0f;
    else if (t >= 8.5) s->speed = 64.0f;
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t n = (uint32_t)(14.0 * RFS) / 32 * 32; float *x = calloc(n, 4), *m = calloc(n, 4);
    double beat = 60.0 / BPM;
    /* beats as the firmware reads them: the sync table's floats times samples a beat / 44100 (35280 / 44100 s at 75 BPM) */
    float bs = 35280.0f * (1.0f / 44100.0f);
    gen_skank(x, n);
    render(dir, "after-skank-converted", x, 0, n, (set_t){0.5f * bs, 0.75f * bs, 0, 55, 60, 127, 6, 0, 0, 64}, 0);
    render(dir, "after-skank-ping", x, 0, n, (set_t){0.75f * bs, 1.5f * bs, 1, 62, 60, 127, 8, 3, 28, 64}, 0);
    gen_throw(x, m, n);
    render(dir, "after-throw-converted", x, m, n, (set_t){0.5f * bs, 1.5f * bs, 0, 75, 30, 0, 6, 0, 0, 64}, throw_ctl);
    render(dir, "after-throw-bend", x, m, n, (set_t){0.5f * bs, 1.0f * bs, 1, 82, 34, 0, 10, 2, 24, 64}, bend_ctl);
    (void)beat;
    puts("rendered through the scratch kernel with the parameters by hand");
    return 0;
}
