/* round 12: the DUB renders of BEFORE (the kernel as it stands on linux-hw 09e233a: four-head patterns, TONE, SPACE) */
#include "oldk/dubdelay.c"
#include "dubin.h"
static int16_t *blk[16];
static void render(const char *name, const float *in, const float *mask, uint32_t n, int ptrn_heads, const float *hd, const float *lv, const float *pn, const float *fbw, float time, float fb, float tone, float thr)
{
    dd_t s; dd_params_t p; static int16_t pool[16 * 2048];
    for (int k = 0; k < 16; k++) blk[k] = pool + k * 2048;
    memset(pool, 0, sizeof pool);
    dk_init(&s, 2, blk, 16);
    float *ol = calloc(n, 4), *or_ = calloc(n, 4);
    for (uint32_t b = 0; b + 32 <= n; b += 32) {
        float x[32], l[32], r[32];
        for (int i = 0; i < 32; i++) x[i] = in[b + i];
        memset(&p, 0, sizeof p);
        p.time = time; for (int j = 0; j < ptrn_heads; j++) { p.head[j] = hd[j]; p.level[j] = lv[j]; p.pan[j] = pn[j]; p.fbw[j] = fbw[j]; }
        p.feedback = fb * 0.01f; p.send = mask ? (mask[b] > 0 ? thr : 0.0f) : 1.0f;
        if (tone < 64) { p.lp = 30.0f + 1.4f * tone; p.hp = 15.0f; } else { p.lp = 119.0f; p.hp = 15.0f + 1.6f * (tone - 64); }
        p.res = 0.3f; p.drive = 0.4f; p.wet = 1.0f; p.dry = 0.0f; p.glide = 0.12f; p.route = 1; p.lim_ceil = 0.891f;
        dk_process(&s, &p, x, l, r);
        for (int i = 0; i < 32; i++) { ol[b + i] = in[b + i] + l[i]; or_[b + i] = in[b + i] + r[i]; }
    }
    char path[256]; snprintf(path, sizeof path, "dub-renders/%s", name); wavw(path, ol, or_, n);
    free(ol); free(or_);
}
int main(void)
{
    uint32_t n = (uint32_t)(14.0 * RFS) / 32 * 32; float *x = calloc(n, 4), *m = calloc(n, 4);
    double beat = 60.0 / BPM;
    gen_skank(x, n);
    /* SKANK pattern at 1/8 (0.5 beat), FDBK 55, TONE 60, spring off */
    { float h[2] = {1, 1.5f}, lv[2] = {.8f, .5f}, pn[2] = {-.3f, .3f}, fw[2] = {.6f, .4f};
      render("before-skank.wav", x, 0, n, 2, h, lv, pn, fw, (float)(0.5 * beat), 55, 60, 1.0f); }
    gen_throw(x, m, n);
    /* the throw: SLAP (the default pattern), SEND at rest 0, THRW 127 for the snare, FDBK 75, TONE 30 (closing low-pass) */
    { float h[2] = {1, 3}, lv[2] = {.75f, .5f}, pn[2] = {.3f, -.4f}, fw[2] = {.2f, .8f};
      render("before-throw.wav", x, m, n, 2, h, lv, pn, fw, (float)(0.5 * beat), 75, 30, 1.0f); }
    puts("rendered");
    return 0;
}
