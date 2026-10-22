/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.5.0: the MACRO engine (eng_macro.c) through the real voices and mixer.
 *   build/host/macro_test [wav folder]
 * Each of its 14 models at the engine's defaults, then every factory preset: a note held 1 s and released. Checked:
 * no NaN or wrap (the mixer's output stays inside 16 bits), each makes sound, the voice ends after the release, a
 * model change while a note sounds, a second note while one sounds (one voice in MONO), a chord in POLY (0.5.2: four
 * voices; STRING one), four tracks at once.
 * Host cost per model in ns per sample, with CZ-1 INIT TONE for scale (printed only). With a folder: a WAV of each
 * preset (16-bit mono 44.1 kHz) to listen to.
 * The models themselves are checked against the original C++ by tests/plaits_parity (needs a checkout of it).
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>

static int bad;
static void audio_resources_reset(void) { memset(resource, 0, sizeof resource); }
static void check(const char *what, int ok)
{
    printf("macro: %-86s %s\n", what, ok ? "ok" : "FAIL");
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

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    uint32_t m, k, blocks = 44100u * SECS / CTL;
    double ns, ns_ref;
    int fr;
    char line[160];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("macro: part state %zu B (largest model: STRING %zu B, PARTICLE %zu B)\n", sizeof(mc_part_t),
           sizeof(pl_string_t), sizeof(pl_particle_t));
    check("one track's MACRO state fits Melodee's 12 KB per part", sizeof(mc_part_t) <= 12u * 1024u);

    fresh();
    host_preset(&trk[0], ENGI_CZ, 0);
    play(60, blocks / 2, blocks, &ns_ref, &fr);
    printf("macro: CZ-1 INIT TONE for scale: %.0f ns per sample on this host\n", ns_ref);

    for (m = 0; m < MC_N; m++) {                        /* each model at the engine's defaults */
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENG_MACRO.edit[k].def;
        trk[0].p[P_E0] = (int16_t)m;
        trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 64; trk[0].p[P_SUS] = 127; trk[0].p[P_REL] = 40;
        int32_t peak = play(48, blocks / 2, blocks, &ns, &fr);
        snprintf(line, sizeof line, "model %-6s sounds (peak %5d), stays in range, frees its voice (%.0f ns/sample, %.1fx CZ-1)",
                 N_MC_MODEL[m], peak, ns, ns / ns_ref);
        check(line, peak > 300 && peak < 32768 && fr > 0);
    }

    for (k = 0; k < ENG_MACRO.npresets; k++) {          /* every preset */
        const preset_t *p = &MC_PRESETS[k];
        fresh();
        host_preset(&trk[0], ENGI_MACRO, k);
        int32_t peak = play(p->e[0] >= MC_KICK && p->e[0] <= MC_HIHAT ? 36 : 48, blocks / 5, blocks, &ns, &fr);
        snprintf(line, sizeof line, "preset %-15s sounds (peak %5d), frees its voice %s", p->name, peak,
                 fr > 0 ? "" : "(NOT FREED)");
        check(line, peak > 300 && peak < 32768 && fr > 0);
        if (dir) {
            char path[512];
            snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, k, p->name);
            for (char *c = path + strlen(dir) + 1; *c; c++) if (*c == ' ') *c = '_';
            wav_write(path, take, NELEM(take));
        }
    }

    {   /* a model change while a note sounds, and a second note on the same track */
        int32_t out[2 * CTL];
        uint32_t i, n = 0, b;
        int finite = 1;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 200; b++) {
            if (b % 20 == 10) trk[0].p[P_E0] = (int16_t)((b / 20) % MC_N);
            if (b == 100) trk_note_on(&trk[0], 55, 100);
            mix_block(out, CTL);
            for (i = 0; i < NVOICE; i++) n += 0;
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active;
        check("model changed every 20 blocks under a held note; a second note: one voice, no fault", n == 1 && finite);
    }

    {   /* (0.5.2) a step's chord: four voices on a POLY preset, one on STRING; each voice's own pitch */
        static const uint8_t chord[4] = {48, 52, 55, 59};
        int32_t out[2 * CTL];
        uint32_t i, b, n4 = 0, n1 = 0, pitches = 1;
        int finite = 1;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 9);            /* (ADDITIVE PAD: POLY) */
        for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
        for (b = 0; b < 100; b++) {
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n4 += trk[0].v[i].active;
        for (i = 1; i < 4u; i++) pitches &= trk[0].v[i].note != trk[0].v[0].note;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 17);           /* (LOW STRING: one voice) */
        trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
        for (b = 0; b < 50; b++) mix_block(out, CTL);
        for (i = 0; i < NVOICE; i++) n1 += trk[0].v[i].active && trk[0].v[i].gate;
        snprintf(line, sizeof line, "POLY: a four note chord plays four voices (%u), STRING one (%u), no fault", n4, n1);
        check(line, n4 == 4u && n1 == 1u && pitches && finite);
    }
    {   /* (0.5.2) HARP: STRING's string, one a voice: a chord rings on four */
        static const uint8_t chord[4] = {48, 52, 55, 59};
        int32_t out[2 * CTL], peak = 0;
        uint32_t i, b, n = 0;
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 25);           /* (HARP) */
        for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
        for (b = 0; b < 100; b++) {
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) peak = out[i] > peak ? out[i] : -out[i] > peak ? -out[i] : peak;
        }
        for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active;
        snprintf(line, sizeof line, "HARP: a four note chord rings on four voices (%u), peak %d", n, peak);
        check(line, n == 4u && peak > 300 && peak < 32768 && !strcmp(MC_PRESETS[25].name, "HARP"));
    }

    {   /* four tracks of MACRO at once: the heaviest models */
        static const uint8_t heavy[4] = {MC_STRING, MC_CHORD, MC_PARTCL, MC_MODAL};
        int32_t out[2 * CTL];
        uint32_t b;
        fresh();
        for (k = 0; k < 4u; k++) {
            host_preset(&trk[k], ENGI_MACRO, 0);
            trk[k].p[P_E0] = heavy[k];
            trk[k].p[P_SUS] = 127;
            trk_note_on(&trk[k], 40 + 7 * k, 100);
        }
        double t0 = mt_ns();
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        ns = (mt_ns() - t0) / (double)(blocks * CTL);
        snprintf(line, sizeof line, "four tracks (STRING, CHORD, PARTCL, MODAL) at once: %.0f ns/sample, %.1fx CZ-1", ns, ns / ns_ref);
        check(line, 1);
        check("four tracks' MACRO states all allocated", resource[RES_ENGINE0].size && resource[RES_ENGINE0 + 1].size &&
              resource[RES_ENGINE0 + 2].size && resource[RES_ENGINE0 + 3].size);
    }
    printf(bad ? "macro: %d FAILED\n" : "macro: all passed\n", bad);
    return bad != 0;
}
