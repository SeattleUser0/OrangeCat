/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md): a candidate module (SEM or HADES, Caravel's; 0.14.1: op1r's synths, through
 * tools/module/op1r/op1_glue.h) on the host, in a measurement build (OC_CANDIDATES=1: the engine table of 32, the proposed numbers registrable). Built once per candidate with its unit (tests/module_host_<name>_tu.c)
 * and -DCAND_ENGINE=ENGI_<NAME>: the header passes the registration check, the loader's registration takes the proposed slot (and only in this build),
 * the engine is offered while registered, its presets render through the whole voice path (tests/regress.c) with the lines of tests/golden_modules.txt
 * (module/<NAME>/..., new lines), and unregistering retires the slot. Sample for sample against Caravel's own build of the engine: tests/caravel_parity
 * (a development check: CI has no Caravel checkout).
 *
 *   build/host/module_host_<name>_test [tests/golden_modules.txt]
 */
#define REGRESS_MODULE_HOST 1
#define JOB_PRESET_HOOK(t, j) do { if ((j)->arg) (t)->p[P_E6] = (int16_t)(j)->arg; } while (0)   /* (0.14.0) arg: EDIT 2's third control, SEM's STAT */
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
#if !OC_CANDIDATES && !OC_ENGINE_MODS
#error "build with -DOC_CANDIDATES=1 (a measurement build), or OC_ENGINE_MODS=1 for the four a shipping build takes (engines-b)"
#endif

#include "fw_pages_wire.h"
#include "oc_modui.c"                          /* (engines-b; oc_module.c includes it on the unit: this host build registers modules by hand) */
#define CW_BASE 0x020C4000u                    /* (engines-b) a module's link address at start 0: where the wire image of its descriptor is laid */
static int cw_rd(void *ctx, uint32_t addr, void *buf, uint32_t n)
{
    if (addr < CW_BASE || addr - CW_BASE + n > 16384u) return 1;
    memcpy(buf, (const uint8_t *)ctx + (addr - CW_BASE), n);
    return 0;
}
static int bad;
static void check(const char *what, int ok)
{
    printf("module host (candidate %u): %-100s %s\n", (unsigned)CAND_ENGINE, what, ok ? "ok" : "FAIL");
    bad += !ok;
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden_modules.txt";
    static kv_t gold[MAXJ];
    static char names[64][64];
    const engine_t *em;
    fw_mod_hdr_t h;
    uint32_t i, n, np, offered = 0, same = 0, crashed = 0, ng;
    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    np = em->npresets;
    ng = load_kv(gpath, gold, MAXJ);
    check("the header passes the registration check and names the proposed number", fw_module_check(&h) == FW_MOD_OK && h.engine == CAND_ENGINE &&
          h.engine >= 21u && h.engine < NENGINES && NENGINES == (OC_CANDIDATES ? 48 : 30));   /* (engines-b) 48: a measurement build (the Rhodes proposal at 47); 30: a shipping build with OC_ENGINE_MODS */
    if (CAND_ENGINE == ENGI_X0X303)            /* (engines-b) X0X's 303 (23: below op1r's numbers, but not Caravel's) */
        check("no hot set of its own, one bass303_t a part under 16 KB, one voice", em->nhot == 0u && h.state_size >= 4u && h.state_size <= 16384u &&
              em->poly == 1u && np >= 3u && np <= 64u);
    else if (CAND_ENGINE < ENGI_FMBELL)
        check("no hot set of its own, the state in voice_t (a 4-byte token state: the loader refuses 0)", em->nhot == 0u && h.state_size == 4u && np >= 3u && np <= 64u);
    else                                       /* (0.14.1) op1r's: the glue's part state (the voices' 128-sample blocks, the plugin's own), six voices */
        check("its hot set (brassophone: its warp table and op1r's sine; else none), the glue's part state (op1g_part_t) under 16 KB, OP1_VOICES voices",
              em->nhot == (CAND_ENGINE == ENGI_BRASS ? 2u : 0u) && h.state_size > 2048u &&
              h.state_size <= 16384u && em->poly == 6u && em->keep == 0u && np >= 3u && np <= 64u);
    check("the slot is retired and not offered without the module", ENGINES[CAND_ENGINE] == &ENG_RETIRED && !eng_ok(CAND_ENGINE) &&
          eng_sound_idx(CAND_ENGINE) == CAND_ENGINE && !eng_ok(20u));
#if !OC_CANDIDATES
    check("a shipping build (OC_ENGINE_MODS): this is one of its four module engines; a measurement build's numbers (FMBELL 24, SEM 21) are no slot here",
          ENG_MODS_SHIP(CAND_ENGINE) && !eng_module_slot(ENGI_FMBELL) && !eng_module_slot(ENGI_SEM) && !eng_ok(ENGI_FMBELL));
#endif
    check("the registration takes it (a candidate's slot in this build)", eng_register(&h, em) == EM_OK && ENGINES[CAND_ENGINE] == em);
    if (em->pages) {                           /* (engines-b) the page descriptor: what the device's loader and its knob registry do with it, on the device's layout */
        static uint8_t wimg[16384];
        const fw_pages_t *pg = em->pages;
        fw_ui_info_t info;
        uint32_t da = 0, ui = 0, wn = fwp_wire(pg, wimg, sizeof wimg, CW_BASE, &da), k, t, want = 0, got = 0, inrange = 1;
        int rc = wn ? fw_pages_check_ex(cw_rd, wimg, da, CW_BASE + 0x100u, CW_BASE + wn, SC_MOD, P_COUNT, G_COUNT, &ui) : -1;
        check("its descriptor (fw_pages_t version 3), in the device's layout, passes the loader's check; the panel is its edit[], the pid table P_E0..",
              rc == FWP_OK && ui && pg->panel == em->edit && pg->npanel >= 1u && pg->npanel <= 8u && pg->pid && pg->pid[0].legacy == P_E0);
        int uc = wn ? fw_ui_check(cw_rd, wimg, ui, CW_BASE + 0x100u, CW_BASE + wn, modui_scene_ok, &info) : -1;
        if (uc) printf("module host:   fw_ui_check says %d\n", uc);
        check("its knob table passes fw_ui_check: four knobs, no picture", uc == FWU_OK && info.nk == 4u && info.scene_len == 0u);
        uint32_t pc = modui_publish(CAND_ENGINE, cw_rd, wimg, da, CW_BASE + 0x100u, CW_BASE + wn, wimg, CW_BASE);
        const knob_def_t *kd = kdef_of(CAND_ENGINE);
        for (k = 0; k < 4u; k++)
            for (t = 0; t < 4u; t++) {
                const fw_kdef_t *d = &pg->ui->kdef[k];
                want += d->t[t].pid != FW_PID_NONE;
                if (kd && kd[k].t[t].id != KD_NOT) {
                    got++;
                    inrange &= kd[k].t[t].id >= P_E0 && kd[k].t[t].id < P_E0 + pg->npanel && kd[k].t[t].id == P_E0 + d->t[t].pid - 1u;
                }
            }
        printf("module host:   knobs %s %s %s %s: %u targets declared, %u resolved\n", kd ? kd[0].label : "-", kd ? kd[1].label : "-", kd ? kd[2].label : "-",
               kd ? kd[3].label : "-", want, got);
        check("registered, its four knobs are HOME's (the registry): four macros, every target resolved to its own P_E slot", pc == 0u && kd &&
              kd[0].kind == KD_MACRO && kd[1].kind == KD_MACRO && kd[2].kind == KD_MACRO && kd[3].kind == KD_MACRO && want == got && want >= 4u && inrange);
    }
    for (n = 0; n < NENG_SHOWN; n++) offered += eng_vis(n) == CAND_ENGINE;
    check("registered, it is offered (eng_ok, once in ENGINE_ORDER)", eng_ok(CAND_ENGINE) && offered == 1u);
    nj = 0;
    for (i = 0; i < np; i++) {
        char s[64];
        job_t *j;
        slug(s, eng_preset(em, i)->name, sizeof s);
        snprintf(names[i], 64, "module/%s/%02u_%s", em->name, i, s);
        j = add(J_PRESET, names[i]);
        j->e = (uint8_t)CAND_ENGINE;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    for (i = 0; i < np; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        crashed += J[i].crashed != 0 || J[i].r.over || J[i].r.peak < LIM_PEAK_MIN;   /* (and regress's health limits: sound, no sample past 16 bits) */
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)J[i].r.hash);
        same += want && !strcmp(want, hs);
        if (getenv("GOLDEN_PRINT")) printf("%s %s\n", names[i], hs);
        else if (!want || strcmp(want, hs)) printf("module host:   %s %s golden %s\n", names[i], hs, want ? want : "-");
    }
    check("every preset renders through the voices with the line of tests/golden_modules.txt", crashed == 0 && same == np);
    if (CAND_ENGINE == ENGI_SEM) {             /* (0.14.0) TYPE on SEM's STAT control: 64 the notch blend, 127 exact HP (Caravel's TYPE HP), 128 TYPE BP */
        uint32_t k0 = nj;
        static const uint8_t A[3] = {64, 127, 128};
        for (uint32_t q = 0; q < 3u; q++) {
            uint32_t a = A[q];
            char nm[64]; snprintf(nm, sizeof nm, "sem-type-%u", a);
            job_t *j = add(J_PRESET, nm); j->e = (uint8_t)CAND_ENGINE; j->pi = 0; j->arg = (uint8_t)a;
        }
        run_jobs(J + k0, 3, 3);
        int ok = 1;
        for (uint32_t k = 0; k < 3u; k++) ok &= !J[k0 + k].crashed && !J[k0 + k].r.over && J[k0 + k].r.peak >= LIM_PEAK_MIN;
        ok &= J[k0].r.hash != J[k0 + 1].r.hash && J[k0 + 1].r.hash != J[k0 + 2].r.hash && J[k0].r.hash != J[k0 + 2].r.hash;
        if (!ok) for (uint32_t k = 0; k < 3u; k++) printf("  sem STAT %u: hash %016llx peak %d over %u crashed %d\n", A[k], (unsigned long long)J[k0 + k].r.hash, (int)J[k0 + k].r.peak, (unsigned)J[k0 + k].r.over, (int)J[k0 + k].crashed);
        check("SEM's STAT control: 64 (the notch blend), 127 (exact HP: TYPE HP) and 128 (TYPE BP) are three different sounds, each healthy", ok);
    }
    if (CAND_ENGINE == ENGI_DDDD) {            /* (0.14.1) the part's cost (fw_api.h FW_MOD_COST_PART): 4 units a track, its voices none */
        uint32_t snd[2][5], kill[2][5], busy[2][5];
        for (uint32_t mode = 0; mode < 2u; mode++)
            for (uint32_t nt = 1; nt <= 4u; nt++) {
                int32_t out[2 * CTL];
                uint32_t k0;
                h.cost = mode ? FW_MOD_COST_PART | 4u << 24 : 0u;
                memset(trk, 0, sizeof trk);
                memset(&song, 0, sizeof song);
                host_tracks_init();
                for (uint32_t p = 0; p < NPART; p++) {
                    host_preset(&trk[p], p < nt ? CAND_ENGINE : 0u, p < nt ? 2u : 0u);          /* SCREAM */
                    trk[p].p[P_VOICE] = V_POLY;
                }
                k0 = voice_kills;
                for (uint32_t p = 0; p < nt; p++)
                    for (uint32_t k = 0; k < 4u; k++)
                        input_on(&trk[p], 48u + 4u * k, 100);
                for (uint32_t b = 0; b < 50u; b++) mix_block(out, CTL);
                snd[mode][nt] = 0;
                for (uint32_t p = 0; p < NPART; p++)
                    for (uint32_t i = 0; i < NVOICE; i++) snd[mode][nt] += trk[p].v[i].active && trk[p].v[i].gate;
                kill[mode][nt] = voice_kills - k0;
                busy[mode][nt] = voices_busy();
                printf("module host:   dddd SCREAM x%u, four keys a track, %s: %u voices sound, %u given up, %u units busy of %u\n", nt,
                       mode ? "the part's cost" : "voices counted", snd[mode][nt], kill[mode][nt], busy[mode][nt], voice_budget());
            }
        check("voices counted (2 units each): from three tracks dddd gives up voices it does not pay for", kill[0][3] > 0u && kill[0][4] > 0u);
        check("the part's cost (4 units a track): every key of 1 to 4 tracks sounds, none given up, 4 units a sounding track",
              snd[1][1] == 4u && snd[1][2] == 8u && snd[1][3] == 12u && snd[1][4] == 16u && kill[1][4] == 0u &&
              busy[1][1] == 4u && busy[1][2] == 8u && busy[1][3] == 12u && busy[1][4] == 16u);
        h.cost = 0;
    }
    if (getenv("CAND_BENCH")) {                /* (engines-b) the host's render cost, for information (never a check): each preset on one track, a four-note chord
                                                * held (sustain 127) or what the preset's voice mode keeps of it; 2 s of the whole mix after 0.5 s, thread CPU time,
                                                * the least of 9 runs (the PC is shared: the minimum is the run least disturbed), the idle mix's least subtracted;
                                                * ns a voice-sample over the voices sounding */
        static const uint8_t CH[4] = {48, 52, 55, 59};
        int32_t out[2 * CTL];
        uint32_t nb = (uint32_t)(2 * FS / CTL), w = (uint32_t)(FS / 2 / CTL), b, k, r;
        double idle = 0;
        for (i = 0; i <= np; i++) {            /* i == 0: the idle mix first (preset 0 loaded, no key); then preset i - 1 */
            double best = 1e30, v = 0;
            uint32_t vmax = 0;
            for (r = 0; r < 9u; r++) {
                struct timespec t0, t1;
                uint32_t vs = 0, cnt = 0;
                memset(trk, 0, sizeof trk);
                memset(&song, 0, sizeof song);
                host_tracks_init();
                host_preset(&trk[0], CAND_ENGINE, i ? i - 1u : 0u);
                trk[0].p[P_SUS] = 127;
                trk[0].p[P_AMODE] = 0;
                if (i)
                    for (k = 0; k < 4u; k++) input_on(&trk[0], CH[k], 100);
                for (b = 0; b < w; b++) mix_block(out, CTL);
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t0);
                for (b = 0; b < nb; b++) mix_block(out, CTL);
                clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t1);
                for (k = 0; k < NVOICE; k++) vs += trk[0].v[k].active != 0;
                cnt = vs;
                double ns = ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / ((double)nb * CTL);
                if (ns < best) best = ns;
                v = cnt; vmax = cnt > vmax ? cnt : vmax;
            }
            if (!i) { idle = best; printf("bench %s idle %.1f ns a sample\n", em->name, best); continue; }
            printf("bench %s %02u %-12s voices %.0f: %.1f ns a sample over idle, %.1f ns a voice-sample\n", em->name, i - 1u,
                   eng_preset(em, i - 1u)->name, v, best - idle, v > 0 ? (best - idle) / v : 0.0);
        }
    }
    eng_unregister(CAND_ENGINE);
    check("unregistered, the slot is retired again", ENGINES[CAND_ENGINE] == &ENG_RETIRED && !eng_ok(CAND_ENGINE));
    printf(bad ? "module host (candidate): FAILED\n" : "module host (candidate): all passed\n");
    return bad != 0;
}
