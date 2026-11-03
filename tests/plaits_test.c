/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * PLAITS (the model picker's module; docs/candidates/model_picker.md): the module's engine (tools/module/plaits_mod.c, built as its own unit:
 * tests/module_host_plaits_tu.c) through the real voices and mixer, registered in its proposed slot (an OC_CANDIDATES build), as
 * tests/macro2_test.c does for MACRO2.
 *   build/host/plaits_test            every unit in: each of the 25 models at the engine's defaults, every preset, a model change under a held
 *                                     note, a chord, four tracks; the header's state_size is what the glue computes for the models in
 *   build/host/plaits_pick_test       the unit built without WAVTBL, SPEECH, 6OP B and wave bank C (-DPL_HOST_NO_...): those models are not
 *                                     offered (the knob steps over them, the value reads MISSG), a track that names one keeps its settings and
 *                                     renders silence, the others play, TERRAN plays with two banks, 6OP A and C play beside the missing B
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#if !OC_CANDIDATES
#error "build with -DOC_CANDIDATES=1"
#endif
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern uint32_t oc_host_module_state(void);

static int bad;
static void audio_resources_reset(void) { memset(resource, 0, sizeof resource); }
static void check(const char *what, int ok)
{
    printf("plaits: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
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
    if (g_em) {
        eng_unregister(ENGI_MACRO2);
        eng_register(&g_h, g_em);
    }
}
static int32_t play(uint32_t note, uint32_t hold, uint32_t total, int *free_after)
{
    int32_t out[2 * CTL], peak = 0;
    uint32_t b, i;
    *free_after = -1;
    trk_note_on(&trk[0], note, 100);
    for (b = 0; b < total; b++) {
        if (b == hold) trk_note_off(&trk[0], note);
        mix_block(out, CTL);
        for (i = 0; i < 2 * CTL; i++) {
            int32_t v = out[i] < 0 ? -out[i] : out[i];
            if (v > peak) peak = v;
        }
        if (b > hold && *free_after < 0) {
            uint32_t n = 0;
            for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active;
            if (!n) *free_after = (int)(b - hold);
        }
    }
    return peak;
}
static void defaults(uint32_t model)
{
    uint32_t k;
    host_preset(&trk[0], ENGI_MACRO2, 0);
    for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = g_em->edit[k].def;
    trk[0].p[P_E0] = (int16_t)model;
    trk[0].p[P_VOICE] = V_LEGATO;
    trk[0].p[P_ATK] = 0; trk[0].p[P_DEC] = 64; trk[0].p[P_SUS] = 127; trk[0].p[P_REL] = 40;
}

int main(void)
{
    const engine_t *em = oc_host_module_engine();
    fw_mod_hdr_t h = OC_MODULE_HDR;
    uint32_t m, k, nm, blocks = 44100u * 2u / CTL, pblocks = 44100u * 5u / CTL, missing = 0;   /* (the presets: a release of up to 104 takes seconds) */
    int fr;
    char line[200], val[16];
    const char *unit;
    setvbuf(stdout, NULL, _IONBF, 0);
    oc_host_fw = &FW_API;
    nm = (uint32_t)em->edit[0].max + 1u;
    printf("plaits: part state %u B (the header's state_size), the glue's own count %u B; %u models, %u presets\n", (unsigned)h.state_size,
           (unsigned)oc_host_module_state(), nm, em->npresets);
    check("the header passes the registration check and names the candidate slot", fw_module_check(&h) == FW_MOD_OK && h.engine == ENGI_MACRO2);
    check("the header's state_size is what the glue computes for the units in, and within the loader's 16 KB",
          h.state_size == oc_host_module_state() && h.state_size <= 16384u);
    check("the registration takes the proposed slot", eng_register(&h, em) == EM_OK && ENGINES[ENGI_MACRO2] == em && eng_ok(ENGI_MACRO2));
    g_em = em; g_h = h;
    for (m = 0; m < nm; m++) {                        /* each model at the engine's defaults, one voice */
        int offered = em->offered(&trk[0], 0, (int32_t)m);
        int32_t peak;
        fresh();
        defaults(m);
        param_format(&em->edit[0], (int32_t)m, val, &unit);
        peak = play(48, blocks / 2, blocks, &fr);
        if (offered) {
            snprintf(line, sizeof line, "model %-6s sounds (peak %5d), stays in range, frees its voice", em->edit[0].names[m], peak);
            check(line, peak > 300 && peak < 32768 && fr > 0 && !strncmp(val, em->edit[0].names[m], 5));   /* (a value string is 5 characters) */
        } else {
            missing++;
            snprintf(line, sizeof line, "model %-6s is not installed: reads %s, renders silence (peak %d), the setting kept (%d)", em->edit[0].names[m], val, peak, trk[0].p[P_E0]);
            check(line, !strcmp(val, "MISSG") && peak == 0 && trk[0].p[P_E0] == (int16_t)m);
        }
    }
    for (k = 0; k < em->npresets; k++) {              /* every preset */
        const preset_t *p = eng_preset(em, k);
        int offered = em->offered(&trk[0], 0, p->e[0]);
        int32_t peak;
        fresh();
        host_preset(&trk[0], ENGI_MACRO2, k);
        peak = play(48, pblocks / 5, pblocks, &fr);
        snprintf(line, sizeof line, "preset %-16s %s (peak %5d)%s", p->name, offered ? "sounds" : "is silent: its model is not installed", peak,
                 offered && fr <= 0 ? " (NOT FREED)" : "");
        check(line, offered ? peak > 300 && peak < 32768 && fr > 0 : peak == 0);
    }
    {   /* the MODL knob: a step lands on an offered model only (params.c enum_step with engine_t.offered) */
        int32_t v = 0, steps = 0, hit_missing = 0;
        uint32_t i;
        fresh();
        defaults(0);
        for (i = 0; i < 2u * nm; i++) {
            int32_t nv = enum_step(&em->edit[0], v, v + 1 > em->edit[0].max ? v : v + 1);
            if (nv == v) break;
            hit_missing += !em->offered(&trk[0], 0, nv);
            v = nv; steps++;
        }
        snprintf(line, sizeof line, "the MODL knob steps through %d models and lands on no uninstalled one (%u not installed)", steps, missing);
        check(line, hit_missing == 0 && steps == (int32_t)(nm - missing - 1u));
    }
    {   /* a model change while a note sounds, and a second note on the same track */
        int32_t out[2 * CTL];
        uint32_t i, n = 0, b;
        int finite = 1;
        fresh();
        defaults(0);
        trk_note_on(&trk[0], 48, 100);
        for (b = 0; b < 20 * nm + 20; b++) {
            if (b % 20 == 10) trk[0].p[P_E0] = (int16_t)((b / 20) % nm);
            if (b == 100) trk_note_on(&trk[0], 55, 100);
            mix_block(out, CTL);
            for (i = 0; i < 2 * CTL; i++) finite &= out[i] == out[i];
        }
        for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active;
        check("model changed every 20 blocks under a held note (every model, installed or not); a second note: one voice, no fault", n == 1 && finite);
    }
    {   /* a chord: four voices on a small model (VA, POLY), one on a large one (STRING, 6OP A, STRMCH) */
        static const uint8_t chord[4] = {48, 52, 55, 59};
        static const uint8_t big[3] = {9, 17, 21};
        int32_t out[2 * CTL];
        uint32_t i, b, n4 = 0, n1 = 0, j;
        int finite = 1;
        fresh();
        defaults(0);
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
            defaults(big[j]);
            trk[0].p[P_VOICE] = V_POLY;
            for (i = 0; i < 4u; i++) trk_note_on(&trk[0], chord[i], 100);
            for (b = 0; b < 50; b++) mix_block(out, CTL);
            for (i = 0; i < NVOICE; i++) n += trk[0].v[i].active && trk[0].v[i].gate;
            n1 += n == 1u;
        }
        snprintf(line, sizeof line, "POLY: a four note chord sounds %u VA voices (the budget's share), the large models one each (%u of 3), no fault", n4, n1);
        check(line, n4 >= 2u && n4 <= 4u && n1 == 3u && finite);
    }
    {   /* four tracks at once: large models on each */
        static const uint8_t heavy[4] = {17, 21, 20, 9};
        int32_t out[2 * CTL];
        uint32_t b;
        fresh();
        for (k = 0; k < 4u; k++) {
            host_preset(&trk[k], ENGI_MACRO2, 0);
            trk[k].p[P_E0] = heavy[k];
            trk[k].p[P_SUS] = 127;
            trk_note_on(&trk[k], 40 + 7 * k, 100);
        }
        for (b = 0; b < blocks; b++) mix_block(out, CTL);
        check("four tracks (6OP A, STRMCH, TERRAN, STRING) at once: every track's state allocated", resource[RES_ENGINE0].size && resource[RES_ENGINE0 + 1].size &&
              resource[RES_ENGINE0 + 2].size && resource[RES_ENGINE0 + 3].size);
    }
    printf(bad ? "plaits: %d FAILED\n" : "plaits: all passed\n", bad);
    return bad != 0;
}
