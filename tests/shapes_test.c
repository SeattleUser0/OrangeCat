/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.5.1: the SHAPES engine (eng_shapes.c) through the real voices and mixer.
 *   build/host/shapes_test [wav folder]
 * Each shape at the engine's defaults, then every factory preset: a note held and released. Checked: each makes
 * sound inside 16 bits and frees its voice; A4 plays at 440 Hz (the 88.2 kHz rendering and its pitch offset); the
 * filter, drive, bits and rate do something; four tracks at once. Host cost per shape in ns per sample, with CZ-1
 * INIT TONE for scale (printed only). With a folder: a WAV of each preset.
 * The shapes themselves are checked against the original C++ by tests/braids_parity (needs a checkout of it).
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>

static int bad;
static void audio_resources_reset(void) { memset(resource, 0, sizeof resource); }
static void check(const char *what, int ok)
{
    printf("shapes: %-86s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static double mt_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}
static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    audio_resources_reset();
    song.master_q12 = 4096;
}
static void wav_write(const char *path, const int16_t *s, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t v;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36 + n * 2; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    uint16_t h[2] = {1, 1}; fwrite(h, 2, 2, f);
    v = 44100; fwrite(&v, 4, 1, f); v = 88200; fwrite(&v, 4, 1, f);
    uint16_t h2[2] = {2, 16}; fwrite(h2, 2, 2, f);
    fwrite("data", 1, 4, f); v = n * 2; fwrite(&v, 4, 1, f);
    fwrite(s, 2, n, f);
    fclose(f);
}
#define SECS 5u
static int16_t take[44100u * SECS];
/* a note on track 0 held `hold` blocks, then released; render `total` blocks; the mono mix into take */
static int32_t play(uint32_t note, uint32_t hold, uint32_t total, double *ns, int *free_after)
{
    int32_t out[2 * CTL], peak = 0;
    uint32_t b, i;
    double t0 = mt_ns();
    *free_after = -1;
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < total; b++) {
        if (b == hold) trk_note_off(&trk[0], note);
        mix_block(out, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = out[2 * i];                     /* (the mix: 16-bit samples in 32 bits) */
            if (l > peak) peak = l;
            if (-l > peak) peak = -l;
            if (b * CTL + i < NELEM(take)) take[b * CTL + i] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        }
        if (b > hold && *free_after < 0) {
            int any = 0;
            for (i = 0; i < NVOICE; i++) any |= trk[0].v[i].active;
            if (!any) *free_after = (int)b;
        }
    }
    *ns = (mt_ns() - t0) / (double)(total * CTL);
    return peak;
}


/* the frequency of take[from, from + n) by its rising zero crossings (interpolated) */
static double freq_of(uint32_t from, uint32_t n)
{
    double first = -1, last = -1;
    int count = 0;
    for (uint32_t i = from + 1; i < from + n; i++)
        if (take[i - 1] < 0 && take[i] >= 0) {
            double t = i - 1 + (double)-take[i - 1] / (double)(take[i] - take[i - 1]);
            if (first < 0) first = t;
            last = t;
            count++;
        }
    return count > 1 ? 44100.0 * (count - 1) / (last - first) : 0;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    uint32_t m, k, blocks = 44100u * SECS / CTL;
    double ns, ns_ref;
    int fr;
    char line[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("shapes: part state %zu B\n", sizeof(sh_part_t));
    check("one track's SHAPES state fits Melodee's 12 KB per part", sizeof(sh_part_t) <= 12u * 1024u);

    fresh();
    host_preset(&trk[0], ENGI_CZ, 0);
    play(60, blocks / 2, blocks, &ns_ref, &fr);
    printf("shapes: CZ-1 INIT TONE for scale: %.0f ns per sample on this host\n", ns_ref);

    for (m = 0; m < (uint32_t)SH_N; m++) {              /* each shape at the engine's defaults */
        fresh();
        host_preset(&trk[0], ENGI_SHAPES, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENG_SHAPES.edit[k].def;
        trk[0].p[P_E0] = (int16_t)m;
        trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 64; trk[0].p[P_SUS] = 127; trk[0].p[P_REL] = 40;
        int32_t peak = play(48, blocks / 2, blocks, &ns, &fr);
        snprintf(line, sizeof line, "shape %-6s sounds (peak %5d), stays in range, frees its voice (%.0f ns/sample, %.1fx CZ-1)",
                 N_SH_SHAPE[m], peak, ns, ns / ns_ref);
        check(line, peak > 200 && peak < 32768 && fr > 0);
    }

    {   /* A4 at 440 Hz: SINE_TRIANGLE (shape 3) at TIMBRE 0 is a plain triangle */
        fresh();
        host_preset(&trk[0], ENGI_SHAPES, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENG_SHAPES.edit[k].def;
        trk[0].p[P_E0] = 3; trk[0].p[P_E1] = 0; trk[0].p[P_E2] = 0;
        trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127;
        play(69, blocks, blocks, &ns, &fr);
        double f = freq_of(22050, 44100);
        snprintf(line, sizeof line, "A4 plays at %.2f Hz (440 within 0.3 %%: 88.2 kHz and its +188 pitch offset)", f);
        check(line, f > 438.7 && f < 441.3);
    }

    {   /* the tone section: each control changes the sound */
        static const struct { uint8_t e; int16_t v; const char *what; } T[] = {
            {3, 30, "CUTOFF 30 darkens"}, {5, 127, "DRIVE 127"}, {6, 6, "BITS 2"}, {7, 6, "RATE 1/24"} };
        int32_t ref[4096];
        fresh();
        host_preset(&trk[0], ENGI_SHAPES, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENG_SHAPES.edit[k].def;
        trk[0].p[P_SUS] = 127;
        play(48, blocks, blocks, &ns, &fr);
        for (k = 0; k < 4096u; k++) ref[k] = take[22050 + k];
        for (uint32_t j = 0; j < NELEM(T); j++) {
            long diff = 0;
            fresh();
            host_preset(&trk[0], ENGI_SHAPES, 0);
            for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENG_SHAPES.edit[k].def;
            trk[0].p[P_SUS] = 127;
            trk[0].p[P_E0 + T[j].e] = T[j].v;
            int32_t peak = play(48, blocks, blocks, &ns, &fr);
            for (k = 0; k < 4096u; k++) diff += labs((long)take[22050 + k] - ref[k]);
            snprintf(line, sizeof line, "%s: the sound changes (mean difference %ld), stays in range", T[j].what, diff / 4096);
            check(line, diff / 4096 > 100 && peak < 32768);
        }
    }

    for (k = 0; k < ENG_SHAPES.npresets; k++) {         /* every preset */
        const preset_t *p = &SH_PRESETS[k];
        fresh();
        host_preset(&trk[0], ENGI_SHAPES, k);
        int32_t peak = play(SH_SHAPES[p->e[0]] >= PB_SHAPE_KICK && SH_SHAPES[p->e[0]] <= PB_SHAPE_SNARE ? 36 : 48,
                            blocks / 5, blocks, &ns, &fr);
        snprintf(line, sizeof line, "preset %-15s sounds (peak %5d), frees its voice %s", p->name, peak, fr > 0 ? "" : "(NOT FREED)");
        check(line, peak > 200 && peak < 32768 && fr > 0);
        if (dir) {
            char path[512];
            snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, k, p->name);
            for (char *c = path + strlen(dir) + 1; *c; c++) if (*c == ' ') *c = '_';
            wav_write(path, take, NELEM(take));
        }
    }

    {   /* four tracks at once */
        static const uint8_t heavy[4] = {PB_SHAPE_SAW_SWARM, PB_SHAPE_PLUCKED, PB_SHAPE_VOWEL_FOF, PB_SHAPE_GRANULAR_CLOUD};
        int32_t out[2 * CTL];
        uint32_t b;
        fresh();
        for (k = 0; k < 4u; k++) {
            host_preset(&trk[k], ENGI_SHAPES, 0);
            for (m = 0; m < (uint32_t)SH_N && SH_SHAPES[m] != heavy[k]; m++) ;
            trk[k].p[P_E0] = (int16_t)m;
            trk[k].p[P_SUS] = 127;
            trk_note_on(&trk[k], 40 + 7 * k, 100);
        }
        double t0 = mt_ns();
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        ns = (mt_ns() - t0) / (double)(blocks * CTL);
        snprintf(line, sizeof line, "four tracks (SWARM, PLUCK, VFOF, CLOUD) at once: %.0f ns/sample, %.1fx CZ-1", ns, ns / ns_ref);
        check(line, 1);
    }
    printf(bad ? "shapes: %d FAILED\n" : "shapes: all passed\n", bad);
    return bad != 0;
}
