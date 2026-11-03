/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * MACRO2 (candidate; docs/candidates/macro2.md): the module's engine (tools/module/macro2_mod.c, built as its own unit: tests/module_host_macro2_tu.c)
 * through the real voices and mixer, registered in its proposed slot (an OC_CANDIDATES build), as tests/macro_test.c does for MACRO.
 *   build/host/macro2_test [wav folder]
 * Each of its models at the engine's defaults, then every preset: a note held, then released. Checked: no NaN or wrap, each makes sound, the voice
 * ends after the release, a model change under a held note, a chord on a POLY model and one voice on a large one, four tracks at once.
 * Host cost per model in ns per sample, with CZ-1 INIT TONE and MACRO's VA for scale (printed: the numbers of the report's "Render cost" table).
 * The models themselves are checked against the original C++ by tests/plaits_parity.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <time.h>
#if !OC_CANDIDATES
#error "build with -DOC_CANDIDATES=1"
#endif
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);

static int bad;
static void audio_resources_reset(void) { memset(resource, 0, sizeof resource); }
static void check(const char *what, int ok)
{
    printf("macro2: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static double mt_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}
static const engine_t *g_em;
static fw_mod_hdr_t g_h;
static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    audio_resources_reset();
    song.master_q12 = 4096;
    if (g_em) {                                       /* the arena is empty again: the module's hot set is copied again (as the loader does after a pack check) */
        eng_unregister(ENGI_MACRO2);
        eng_register(&g_h, g_em);
    }
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
            int32_t l = out[2 * i];
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
/* the model's cost alone: the idle mix (no note) taken off, as eng_macro.c's units were made. This PC runs other sessions' suites: every cost is
 * the least of COST_REPEATS one-second renders (the quietest run), and the idle mix is measured the same way */
#define COST_REPEATS 7u
static double idle_ns(void)
{
    int32_t out[2 * CTL];
    uint32_t b, r, blocks = 44100u / CTL;
    double best = 1e30;
    fresh();
    for (r = 0; r < COST_REPEATS; r++) {
        double t0 = mt_ns();
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        double ns = (mt_ns() - t0) / (double)(blocks * CTL);
        if (ns < best) best = ns;
    }
    return best;
}
/* one voice of the track's current sound held: the least ns per sample of COST_REPEATS one-second renders */
static double held_ns(uint32_t note)
{
    int32_t out[2 * CTL];
    uint32_t b, r, blocks = 44100u / CTL;
    double best = 1e30;
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < 20; b++) mix_block(out, CTL);        /* (past the attack) */
    for (r = 0; r < COST_REPEATS; r++) {
        double t0 = mt_ns();
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        double ns = (mt_ns() - t0) / (double)(blocks * CTL);
        if (ns < best) best = ns;
    }
    trk_note_off(&trk[0], note);
    return best;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : 0;
    const engine_t *em = oc_host_module_engine();
    fw_mod_hdr_t h = OC_MODULE_HDR;
    uint32_t m, k, nm, blocks = 44100u * SECS / CTL;
    double ns, ns_ref, ns_va, ns_idle, ns_idle_unused;
    int fr;
    char line[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    oc_host_fw = &FW_API;
    nm = (uint32_t)em->edit[0].max + 1u;
    printf("macro2: part state %u B (the header's state_size); %u models, %u presets\n", (unsigned)h.state_size, nm, em->npresets);
    check("the header passes the registration check and names ENGI_MACRO2", fw_module_check(&h) == FW_MOD_OK && h.engine == ENGI_MACRO2);
    check("one track's state fits the loader's limit (16 KB) and MACRO's 12 KB a part", h.state_size <= 16384u && h.state_size <= 12u * 1024u);
    check("the registration takes the proposed slot", eng_register(&h, em) == EM_OK && ENGINES[ENGI_MACRO2] == em && eng_ok(ENGI_MACRO2));
    g_em = em; g_h = h;

    ns_idle = idle_ns();
    fresh();
    host_preset(&trk[0], ENGI_CZ, 0);
    trk[0].p[P_VOICE] = V_LEGATO;
    ns_ref = held_ns(60);
    fresh();
    host_preset(&trk[0], ENGI_MACRO, 0);
    for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = ENGINES[ENGI_MACRO]->edit[k].def;
    trk[0].p[P_E0] = 0;                              /* MACRO's VA, at its defaults, one voice */
    trk[0].p[P_VOICE] = V_LEGATO;
    trk[0].p[P_SUS] = 127;
    ns_va = held_ns(48);
    printf("macro2: for scale on this host: idle mix %.0f ns/sample, CZ-1 INIT TONE %.0f, MACRO VA (one voice) %.0f\n", ns_idle, ns_ref, ns_va);
    printf("macro2: cost table (ns/sample of one voice less the idle mix; x CZ-1 INIT TONE's; x MACRO VA's)\n");
    for (m = 0; m < nm; m++) {                        /* each model at the engine's defaults, one voice */
        double own, cz, va;
        fresh();
        host_preset(&trk[0], ENGI_MACRO2, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = em->edit[k].def;
        trk[0].p[P_E0] = (int16_t)m;
        trk[0].p[P_VOICE] = V_LEGATO;
        trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 64; trk[0].p[P_SUS] = 127; trk[0].p[P_REL] = 40;
        ns = held_ns(48);
        int32_t peak = play(48, blocks / 2, blocks, &ns_idle_unused, &fr);
        own = ns - ns_idle; cz = ns_ref - ns_idle; va = ns_va - ns_idle;
        printf("macro2: cost %-6s %6.0f ns  %5.2fx CZ-1  %5.2fx VA\n", em->edit[0].names[m], own, own / cz, own / va);
        snprintf(line, sizeof line, "model %-6s sounds (peak %5d), stays in range, frees its voice", em->edit[0].names[m], peak);
        check(line, peak > 300 && peak < 32768 && fr > 0);
    }
    for (k = 0; k < em->npresets; k++) {              /* every preset */
        const preset_t *p = eng_preset(em, k);
        fresh();
        host_preset(&trk[0], ENGI_MACRO2, k);
        int32_t peak = play(48, blocks / 5, blocks, &ns, &fr);
        snprintf(line, sizeof line, "preset %-16s sounds (peak %5d), frees its voice %s", p->name, peak, fr > 0 ? "" : "(NOT FREED)");
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
        host_preset(&trk[0], ENGI_MACRO2, 0);
        trk[0].p[P_VOICE] = V_LEGATO;
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 20 * nm + 20; b++) {
            if (b % 20 == 10) trk[0].p[P_E0] = (int16_t)((b / 20) % nm);
            if (b == 100) trk_note_on(&trk[0], 55, 100);
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active;
        check("model changed every 20 blocks under a held note (every model); a second note: one voice, no fault", n == 1 && finite);
    }
    {   /* a chord: four voices on a small model (VA-VCF at its defaults, POLY), one on a large one (6OP A, STRMCH, SPEECH) */
        static const uint8_t chord[4] = {48, 52, 55, 59};
        static const uint8_t big[3] = {2, 6, 9};
        int32_t out[2 * CTL];
        uint32_t i, b, n4 = 0, n1 = 0, j;
        int finite = 1;
        fresh();
        host_preset(&trk[0], ENGI_MACRO2, 0);
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = em->edit[k].def;
        trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
        for (b = 0; b < 100; b++) {
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n4 += trk[0].v[i].active;
        for (j = 0; j < 3u; j++) {
            uint32_t n = 0;
            fresh();
            host_preset(&trk[0], ENGI_MACRO2, 0);
            for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = em->edit[k].def;
            trk[0].p[P_E0] = big[j];
            trk[0].p[P_VOICE] = V_POLY;
            for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
            for (b = 0; b < 50; b++) mix_block(out, CTL);
            for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active && trk[0].v[i].gate;
            n1 += n == 1u;
        }
        snprintf(line, sizeof line, "POLY: a four note chord sounds %u VA-VCF voices (the budget's share), the large models one each (%u of 3), no fault", n4, n1);
        check(line, n4 >= 2u && n4 <= 4u && n1 == 3u && finite);
    }
    {   /* four tracks at once: the heaviest models */
        static const uint8_t heavy[4] = {2, 6, 8, 9};
        int32_t out[2 * CTL];
        uint32_t b;
        fresh();
        for (k = 0; k < 4u; k++) {
            host_preset(&trk[k], ENGI_MACRO2, 0);
            trk[k].p[P_E0] = heavy[k];
            trk[k].p[P_SUS] = 127;
            trk_note_on(&trk[k], 40 + 7 * k, 100);
        }
        double t0 = mt_ns();
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        ns = (mt_ns() - t0) / (double)(blocks * CTL);
        snprintf(line, sizeof line, "four tracks (6OP A, STRMCH, WAVTBL, SPEECH) at once: %.0f ns/sample, %.1fx CZ-1", ns, ns / ns_ref);
        check(line, 1);
        check("four tracks' MACRO2 states all allocated", resource[RES_ENGINE0].size && resource[RES_ENGINE0 + 1].size &&
              resource[RES_ENGINE0 + 2].size && resource[RES_ENGINE0 + 3].size);
    }
    printf(bad ? "macro2: %d FAILED\n" : "macro2: all passed\n", bad);
    return bad != 0;
}
