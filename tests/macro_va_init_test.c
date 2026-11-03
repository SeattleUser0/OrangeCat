/* SPDX-License-Identifier: GPL-3.0-only */
/* MACRO's VA starts on a fresh part (macro_va_init.diff, round 18): a MACRO preset rendered on a fresh part equals the same preset rendered after the part ran
 * another model (so VA's states were started by a model change). Before the fix the VA presets differed (the part's model byte reads 0 = VA, so
 * nothing started its states). VA BASS and VA SYNC LEAD are checked, for a switch from three other models. Uses regress.c's host and phrase. */
#define REGRESS_MODULE_HOST 1
#include "regress.c"
static uint64_t render_in_child(uint32_t pre, uint32_t pi)    /* pre: a MACRO preset to run first (0xFFFF: none, a fresh part) */
{
    track_t *t = &trk[0];
    host_tracks_init();
    memset(&R, 0, sizeof R); free_cap_s = FREE_CAP_S; tail_s = TAIL_S;
    if (pre != 0xFFFFu) { host_preset(t, ENGI_MACRO, pre); t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0; phrase(t, 60); memset(&R, 0, sizeof R); fpos = 0; }
    host_preset(t, ENGI_MACRO, pi);
    t->p[P_REV] = t->p[P_DLY] = t->p[P_CHOR] = 0;           /* dry: an effect's tail from the first phrase would differ for reasons that are not the model's */
    phrase(t, 60);
    return R.hash;
}
#include <sys/wait.h>
#include <unistd.h>
static uint64_t render_after(uint32_t pre, uint32_t pi)   /* one process a render: the host's clock and state start from nothing, as regress.c's jobs do */
{
    int fd[2]; uint64_t h = 0;
    if (pipe(fd)) return 0;
    if (fork() == 0) { uint64_t r = render_in_child(pre, pi); if (write(fd[1], &r, sizeof r) != sizeof r) _exit(1); _exit(0); }
    if (read(fd[0], &h, sizeof h) != sizeof h) h = 0;
    close(fd[0]); close(fd[1]); { int st = 0; wait(&st); if (st) printf("macro_va_init: child status %d (pre %u preset %u)\n", st, pre, pi); }
    return h;
}
int main(void)
{
    uint32_t n = ENGINES[ENGI_MACRO]->npresets, pi, bad = 0, va = 0;
    static const uint8_t SW[3] = {10, 20, 24};
    if (getenv("MVI_DEBUG")) {
        uint32_t k;
        printf("fresh x3: %llx %llx %llx\n", (unsigned long long)render_after(0xFFFFu, 0), (unsigned long long)render_after(0xFFFFu, 0), (unsigned long long)render_after(0xFFFFu, 0));
        for (k = 0; k < 3; k++) printf("after %u x2: %llx %llx\n", SW[k], (unsigned long long)render_after(SW[k], 0), (unsigned long long)render_after(SW[k], 0));
        return 0;
    }
    for (pi = 0; pi < 2 && pi < n; pi++) {            /* VA's presets (VA BASS, VA SYNC LEAD): the fix is about model 0; the other models' noise generators carry over a model change by design */
        uint64_t h0 = render_after(0xFFFFu, pi);
        uint32_t k, same = 1;
        for (k = 0; k < 3; k++) {
            uint32_t pre = SW[k] < n ? SW[k] : n - 1;
            if (pre == pi) continue;
            if (render_after(pre, pi) != h0) same = 0;
        }
        if (!same) { bad++; printf("macro_va_init: preset %u (%s): fresh differs from after a switch\n", pi, eng_preset(ENGINES[ENGI_MACRO], pi)->name); }
        va += pi < 2;
    }
    printf("macro_va_init: %u presets, %u differ fresh vs after a model switch\n", n, bad);
    return bad ? 1 : 0;
}
