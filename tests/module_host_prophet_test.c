/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.1 (Part 1b): the Prophet as a module, on the host, against the built-in engine and the golden file.
 *
 * The module's source (tools/module/prophet_mod.c) is built as its own translation unit (tests/module_host_prophet_tu.c, OC_MODULE_HOST): the same engine code
 * with the firmware's services replaced by the table's members, as on the device (the patches and their ready flags, the CPU1 job, the bend target, the audio
 * worker and the batch queue are the firmware's, reached through FW_API). This test gives it the firmware's own table, checks its header with fw_module_check,
 * puts its engine_t in the Prophet's slot of the (here writable) engine table and renders the Prophet's 201 presets through the whole voice path with
 * tests/regress.c's machinery (the phrase, the voices, the master, the FNV-1a 64 of the samples). Each hash must be the built-in engine's and the line in
 * tests/golden.txt (the default engine set has the built-in Prophet: its lines are the golden renders). Then the descriptor: its rows are the Prophet's PAGES
 * rows (all but the STORE page), its panel is P5_PANEL, and its hooks give the values the built-in page code gives (shown, put, active, motion skip, knobs,
 * titles, the bend range, the envelope source).
 *
 *   build/host/module_host_prophet_test [golden file]
 *
 * Revisions:
 *   0.12.1: first version.
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern const fw_pages_t *oc_host_module_pages(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("module host (Prophet): %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define NPRESET 201u
static void render(uint64_t *hash, uint32_t *crashed, uint32_t np)
{
    uint32_t i;
    nj = 0;
    for (i = 0; i < np; i++) {
        char name[64], s[64];
        job_t *j;
        slug(s, eng_preset(ENGINES[ENGI_PROPHET], i)->name, sizeof s);
        snprintf(name, sizeof name, "preset/%s/%02u_%s", ENGINES[ENGI_PROPHET]->name, i, s);
        j = add(J_PRESET, name);
        j->e = ENGI_PROPHET;
        j->pi = (uint8_t)i;
    }
    run_jobs(J, nj, 8);
    *crashed = 0;
    for (i = 0; i < np; i++) {
        hash[i] = J[i].r.hash;
        *crashed += J[i].crashed != 0;
    }
}
int main(int argc, char **argv)
{
    const char *gpath = argc > 1 ? argv[1] : "tests/golden.txt";
    static kv_t gold[MAXJ];
    static uint64_t builtin[NPRESET], module[NPRESET];
    static char names[NPRESET][64];
    uint32_t ng, np, i, crashed, same = 0, golden_same = 0;
    const engine_t *eb = ENGINES[ENGI_PROPHET], *em;
    const fw_pages_t *mp;
    fw_mod_hdr_t h;

    oc_host_fw = &FW_API;                              /* the firmware's own table: what the device gives its module */
    em = oc_host_module_engine();
    mp = oc_host_module_pages();
    h = OC_MODULE_HDR;
    check("the module's header is accepted by the registration check (identifier, table address, versions)", fw_module_check(&h) == FW_MOD_OK);
    check("the header names the Prophet, the state size of p5_part_t and the pack the tables came from",
          h.engine == ENGI_PROPHET && h.state_size == sizeof(p5_part_t) && h.pack_hash == OC_PACK_HASH);
    check("the module's engine_t is its own (not the built-in's), the same name, presets, voices and controls, and carries the descriptor",
          em != eb && !strcmp(em->name, eb->name) && em->npresets == eb->npresets && em->poly == eb->poly && em->ownenv == eb->ownenv &&
          em->note_on != eb->note_on && em->render != eb->render && em->pages == mp && !eb->pages);
    np = eb->npresets;
    check("the Prophet has 201 presets (INIT and the 200 factory programs)", np == NPRESET);
    {   uint32_t k, e = 1;
        for (k = 0; k < 8u; k++) e &= !strcmp(em->edit[k].label, eb->edit[k].label) && em->edit[k].min == eb->edit[k].min && em->edit[k].max == eb->edit[k].max && em->edit[k].def == eb->edit[k].def;
        check("the eight controls: names, ranges, defaults", e); }
    check("the module's patches are the firmware's (the same arrays, by the table)", oc_host_fw->p5_patch == (void *)p5_patch && oc_host_fw->p5_ready == p5_ready);
    ng = load_kv(gpath, gold, MAXJ);

    render(builtin, &crashed, np);
    check("the built-in engine renders all presets", crashed == 0);
    for (i = 0; i < np; i++) snprintf(names[i], sizeof names[i], "%s", J[i].name);

    ENGINES[ENGI_PROPHET] = em;                        /* the loader's registration, by hand (the built-in engine is in the slot here, so eng_register would call it shadowed) */
    render(module, &crashed, np);
    check("the module renders all presets", crashed == 0);
    for (i = 0; i < np; i++) {
        char hs[24];
        const char *want = kv_get(gold, ng, names[i]);
        snprintf(hs, sizeof hs, "%016llx", (unsigned long long)module[i]);
        same += module[i] == builtin[i];
        golden_same += want && !strcmp(want, hs);
        if (module[i] != builtin[i] || !want || strcmp(want, hs))
            printf("module host (Prophet):   %s module %s built-in %016llx golden %s\n", names[i], hs, (unsigned long long)builtin[i], want ? want : "-");
    }
    check("every one of the 201 hashes equals the built-in engine's", same == np);
    check("... and the line in tests/golden.txt", golden_same == np);

    /* ------------------------------------------------------------------ the descriptor against the built-in page code */
    {   uint32_t k, r, same_rows = 1, nrows = 0, panel_same = 1, id_ok = 1;
        for (k = 0; k < NPAGES; k++) {            /* the built-in Prophet rows, in order, minus the STORE page */
            const page_t *pg = &PAGES[k];
            if (pg->scope != SC_P5) continue;
            same_rows &= nrows < mp->npages && !strcmp(pg->title, mp->rows[nrows].title) && !memcmp(pg->id, mp->rows[nrows].id, 4) && mp->rows[nrows].scope == SC_MOD && mp->rows[nrows].fam == pg->fam;
            nrows++;
        }
        check("the descriptor's rows are the 15 built-in Prophet rows (titles, parameter ids) without the STORE page", same_rows && nrows == 15u && mp->npages == 15u);
        for (k = 0; k < NELEM(P5_PANEL); k++) {
            const param_desc_t *a = &P5_PANEL[k], *b = &mp->panel[k];
            panel_same &= (!a->label == !b->label) && (!a->label || (!strcmp(a->label, b->label) && a->fmt == b->fmt && a->min == b->min && a->max == b->max && a->def == b->def));
        }
        check("the panel is P5_PANEL (88 entries: labels, formats, ranges, defaults)", mp->npanel == NELEM(P5_PANEL) && panel_same);
        for (r = 0; r < mp->npages; r++)
            for (k = 0; k < 4u; k++) id_ok &= mp->rows[r].id[k] == 0xFFu || (mp->rows[r].id[k] < mp->npanel && mp->panel[mp->rows[r].id[k]].label);
        check("every parameter a row names is in the panel", id_ok);
        check("the knobs are the stage page's, the ENV and LFO lists the Prophet's",
              mp->knob[0] == P5_CUTOFF && mp->knob[1] == P5_RESONANCE && mp->knob[2] == P5_ENV_FILTER && mp->knob[3] == P5_RELEASE_AMP &&
              !strcmp(mp->env_titles[0], "P5 FLT ENV") && !strcmp(mp->env_titles[1], "P5 AMP ENV") && !strcmp(mp->env_titles[2], "P5 ENV MOD") && !mp->env_titles[3] &&
              !strcmp(mp->lfo_titles[0], "P5 LFO") && !strcmp(mp->lfo_titles[1], "P5 WHEEL") && !mp->lfo_titles[2]);
        {   uint32_t skip_ok = 1;
            for (k = 0; k < NELEM(P5_PANEL); k++) {
                uint32_t want = k == P5_UNISON || k == P5_UNISON_COUNT || k == P5_UNISON_DETUNE || k == P5_RETRIGGER;
                skip_ok &= (uint32_t)((mp->motion_skip[k >> 3] >> (k & 7u)) & 1u) == want;
            }
            check("motion leaves exactly the voice assignment alone (UNISON, VOICES, DETUNE, PRIORITY)", skip_ok); }
        check("CC 72, 73, 75 edit RELEASE, ATTACK, DECAY of the amplitude envelope", mp->cc[0] == P5_RELEASE_AMP && mp->cc[1] == P5_ATTACK_AMP && mp->cc[2] == P5_DECAY_AMP && mp->cc[3] == 0xFFu);
    }
    /* the hooks give what the built-in page code gives, over every factory patch and every parameter */
    {   uint32_t tr = 0, p, id, shown_same = 1, active_same = 1, put_same = 1, bend_same = 1, calls = 0;
        track_t *t = &trk[tr];
        for (p = 0; p < NPRESET; p++) {
            p5_patch_t *pt = p5_patch_of(t);
            if (p) p5_factory(p - 1u, pt); else p5_patch_init(pt);
            t->bend_raw = 0;
            for (id = 0; id < mp->npanel; id++) {
                int16_t v1, v2;
                const param_desc_t *d1, *d2;
                if (!P5_PANEL[id].label) continue;
                /* the built-in page code's value and descriptor (params.c page_desc, SC_P5) */
                v1 = (int16_t)(pt->raw[id] + (id == P5_BEND ? 1 : 0));
                if (id == P5_UNISON_COUNT && !v1) v1 = 5;
                d1 = &P5_PANEL[id];
                if (p5_keyed_tune(t, id)) { v1 = (int16_t)(clamp(pt->raw[id], 0, 96) / 2 - 12); d1 = &P5_TUNE_DISPLAY[id]; }
                else if (id == P5_BEND) d1 = &P5_BEND_DISPLAY;
                v2 = pt->raw[id];
                d2 = mp->shown(t, id, &v2);
                if (!d2) d2 = &mp->panel[id];
                shown_same &= v1 == v2 && !strcmp(d1->label, d2->label) && d1->min == d2->min && d1->max == d2->max;
                active_same &= mp->active(t, id) == p5_ed_active(t, id);
                calls++;
            }
            for (id = 0; id < mp->npanel; id += 7u) {      /* a put of the shown value back: the byte does not change (the round trip the page does on a turn) */
                int16_t v = mp->native(t)[id];
                uint8_t before;
                if (!P5_PANEL[id].label) continue;
                mp->shown(t, id, &v);
                if (!p5_keyed_tune(t, id) && id != P5_BEND && id != P5_UNISON_COUNT) { before = pt->raw[id]; mp->put(t, id, v); put_same &= pt->raw[id] == before; }
            }
            bend_same &= mp->bend(t) == (clamp(pt->raw[P5_BEND], 0, 11) + 1) * 256;
        }
        check("shown: the value and the descriptor are the built-in page's for every parameter of every factory patch", shown_same && calls > 10000u);
        check("active: the dim hints are the built-in rule's", active_same);
        check("put: a shown value written back leaves the byte as it was", put_same);
        check("bend: the wheel's range is the patch's, 1 to 12 semitones", bend_same);
        /* put of the bend and the keyed tune converts the shown value back to the raw one, as p5_panel_raw does */
        p5_patch_init(p5_patch_of(t));
        mp->put(t, P5_BEND, 7);
        mp->put(t, P5_FREQ_A, 5);
        check("put: BEND shown 7 stores 6; FREQ A shown +5 semitones stores 34 (half steps from -12)", p5_patch_of(t)->raw[P5_BEND] == 6 && p5_patch_of(t)->raw[P5_FREQ_A] == 34);
        mp->put(t, P5_ATTACK_AMP, 300);
        check("put: a value above the panel's range is clamped (ATTACK 300 stores 127)", p5_patch_of(t)->raw[P5_ATTACK_AMP] == 127);
    }
    ENGINES[ENGI_PROPHET] = eb;

    printf(bad ? "module host (Prophet): FAILED\n" : "module host (Prophet): all passed\n");
    return bad != 0;
}
