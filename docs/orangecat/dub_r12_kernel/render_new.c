/* round 12: the DUB renders of AFTER (the two-tap kernel): the old projects' nearest settings, then the new things (ping, wow, spring on the return, the speed bend) */
#include "newk/dubdelay.c"
#include "dubin.h"
typedef struct { float tl, tr; int ping; float fb, filt, send, drive, wow, spring, speed; } set_t;
static int16_t *blk[16];
typedef void (*ctl_t)(double t, set_t *s, const float *mask, uint32_t b);
static void render(const char *name, const float *in, const float *mask, uint32_t n, set_t base, ctl_t ctl)
{
    dd_t s; dd_params_t p; static int16_t pool[16 * 2048]; static int16_t spb[DD_SP_BUF + 8];
    for (int k = 0; k < 16; k++) blk[k] = pool + k * 2048;
    memset(pool, 0, sizeof pool);
    dk_init(&s, 2, blk, 16); dk_spring_attach(&s, spb);
    float *ol = calloc(n, 4), *or_ = calloc(n, 4);
    for (uint32_t b = 0; b + 32 <= n; b += 32) {
        float x[32], l[32], r[32]; set_t c = base;
        for (int i = 0; i < 32; i++) x[i] = in[b + i];
        if (ctl) ctl(b / (double)RFS, &c, mask, b);
        memset(&p, 0, sizeof p);
        p.time_l = c.tl; p.time_r = c.tr; p.ping = c.ping; p.speed = powf(2.0f, (c.speed - 32.0f) / 32.0f); p.wow = c.wow / 63.0f;
        p.feedback = c.fb * 0.01f; p.send = c.send / 127.0f;
        if (c.filt < 64) { p.lp = 30.0f + 1.4f * c.filt; p.hp = 15.0f; } else { p.lp = 119.0f; p.hp = 15.0f + 1.6f * (c.filt - 64); }
        p.res = 0.3f; p.drive = c.drive / 63.0f; p.wet = 1.0f; p.dry = 0.0f; p.glide = 0.12f; p.route = 1; p.lim_ceil = 0.891f;
        p.spring = 2; p.sp_mix = c.spring / 63.0f; p.sp_decay = 2.0f; p.sp_drive = 0.4f; p.sp_tone = 0.3f;
        dk_process(&s, &p, x, l, r);
        for (int i = 0; i < 32; i++) { ol[b + i] = in[b + i] + l[i]; or_[b + i] = in[b + i] + r[i]; }
    }
    char path[256]; snprintf(path, sizeof path, "dub-renders/%s", name); wavw(path, ol, or_, n);
    free(ol); free(or_);
}
static void throw_ctl(double t, set_t *s, const float *mask, uint32_t b) { s->send = mask[b] > 0 ? 127.0f : 0.0f; }
static void bend_ctl(double t, set_t *s, const float *mask, uint32_t b)
{
    s->send = mask[b] > 0 ? 127.0f : 0.0f;
    if (t >= 6.5 && t < 7.0) s->speed = 32.0f + 24.0f * (float)((t - 6.5) / 0.5);          /* the repeat-rate turn up: half a second */
    else if (t >= 7.0 && t < 8.5) s->speed = 56.0f;
    else if (t >= 8.5) s->speed = 32.0f;                                                   /* and back to the centre */
}
int main(void)
{
    uint32_t n = (uint32_t)(14.0 * RFS) / 32 * 32; float *x = calloc(n, 4), *m = calloc(n, 4);
    double beat = 60.0 / BPM;
    gen_skank(x, n);
    /* the old SKANK (heads 1 and 1.5 of 1/8) as the conversion makes it: TIME L 1/8, TIME R 1/8 dotted; FEEDBACK 55, FILTER 60, ECHO 100, drive 25, no spring */
    render("after-skank-converted.wav", x, 0, n, (set_t){(float)(0.5 * beat), (float)(0.75 * beat), 0, 55, 60, 127, 25, 0, 0, 32}, 0);
    /* the new: 1/8 dotted and 1/4, PING, a little wow, drive 40, spring 14 on the return */
    render("after-skank-ping.wav", x, 0, n, (set_t){(float)(0.75 * beat), (float)(1.5 * beat), 1, 62, 60, 127, 40, 14, 14, 32}, 0);
    gen_throw(x, m, n);
    /* the old SLAP (heads 1 and 3 of 1/8) converted: 1/8 and 3/8 (nearest 1/4 dotted = 1.5 beat); the send at rest 0 and thrown for the snare; FEEDBACK 75, FILTER 30 */
    render("after-throw-converted.wav", x, m, n, (set_t){(float)(0.5 * beat), (float)(1.5 * beat), 0, 75, 30, 0, 25, 0, 0, 32}, throw_ctl);
    /* the new: the same throw with PING, the filter closing on the repeats, and the speed turned up at 6.5 s and back to the centre at 8.5 s */
    render("after-throw-bend.wav", x, m, n, (set_t){(float)(0.5 * beat), (float)(1.0 * beat), 1, 82, 34, 0, 40, 10, 12, 32}, bend_ctl);
    puts("rendered");
    return 0;
}
