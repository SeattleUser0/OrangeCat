/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 6 (docs/orangecat/FX_MODULES_PLAN.md 15): DUB TAPE's kernel as a MODULE (tools/module/dub_mod.c) runs through the SAME generic insert host (fx_insert.c, fx_kernel_t) beside the
 * resident copy. THE GATE: the module's render is the resident's, sample for sample and behaviour for behaviour.
 *
 * It is built with the module's translation unit (tests/module_host_dub_tu.c, the module source with OC_MODULE_HOST: its own object, its own statics, the firmware only through FW_API) and runs the
 * scenarios of tests/fx_insert_test.c (the probe: LONG, MEM, R2, HEAVY1, HEAVY2) four times through the real mixer:
 *   A  the resident kernel, no module registered (a build with no module): every output block is written to a file, the printout is kept;
 *   B  the module registered and chosen for all four tracks (fx_ins_use_module(15)): every output block is compared with A's, the hashes, the governor and the behaviour hash too, and the
 *      blocks in which an instance of the module's kernel ran are counted (the module really ran);
 *   C  the module registered and chosen for tracks 0 and 2 only: the instances are independent, so the render is still A's;
 *   D  the module registered and nothing chosen (the default): A's again (installing a module changes nothing until a track is set to it).
 * Then A is compared with the pinned printout of the tree before step 5's cut (tests/fx_insert_golden.txt), line for line. The descriptor: the module's capability words, call table, legacy-slot
 * rows and pages against the resident kernel's and the firmware's own parameter table and PAGES; the pages pass the loader's checker as SC_FX; a kernel the host cannot give is refused; a kernel
 * switch under way and the module going away under a running instance end on the resident kernel with the instance restarted from silence.
 * Arguments: none. Built and run by tests/fx_dub_module.sh (tests/run_tests.sh); the mutation check is the same script with --mutate. */
#define FXI_NO_MAIN 1
#include "fx_insert_test.c"
#include "fw_pages_wire.h"
#include "fw_fx.h"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const fx_caps_t *oc_host_fx_caps(void);
extern const fx_kcalls_t *oc_host_fx_calls(void);
extern const fx_prow_t *oc_host_fx_rows(void);
extern const fw_pages_t *oc_host_fx_pages(void);
extern const fx_desc_wire_t *oc_host_fx_desc(void);
extern const fx_entry_wire_t *oc_host_fx_entry(void);

static FILE *tapef;
static int tap_mode;                                    /* 0 none, 1 write, 2 compare */
static uint64_t tap_blocks, tap_diff, tap_first = ~0ull, mod_run_blocks, res_run_blocks;
static void tap(const int32_t *b, size_t n)
{
    uint32_t k;
    if (tap_mode == 1) fwrite(b, 1, n, tapef);
    else if (tap_mode == 2) {
        int32_t ref[CTL * 2];
        if (fread(ref, 1, n, tapef) != n || memcmp(ref, b, n)) {
            if (!tap_diff) tap_first = tap_blocks;
            tap_diff++;
            if (getenv("FXDM_STOPFIRST")) { printf("fx dub module: the first output block that differs from the resident's is block %llu of the run: FAILED\n", (unsigned long long)tap_blocks); exit(1); }   /* (the mutation check: stop at the first sample) */
        }
    }
    for (k = 0; k < FX_INS_N; k++)
        if (fx_ins[k].state == FXS_RUN) { if (FX_MODK_BOUND(k)) mod_run_blocks++; else res_run_blocks++; }
    tap_blocks++;
}
static void run_all(void)
{
    fxi_nres = 0; fx_ins_gov_fires = 0; tap_blocks = 0; tap_diff = 0; tap_first = ~0ull; mod_run_blocks = res_run_blocks = 0;
    scenario_long(); scenario_mem(); scenario_r2(); scenario_heavy(1); scenario_heavy(2); scenario_trig(); scenario_perf();
}
#define WBASE 0x10200000u
static uint8_t wimg[8192];
static int wrd(void *ctx, uint32_t addr, void *buf, uint32_t n) { (void)ctx; if (addr < WBASE || addr - WBASE + n > sizeof wimg) return 1; memcpy(buf, wimg + (addr - WBASE), n); return 0; }
static fxi_res_t resA[8];
static uint32_t nA;
static void keepA(void) { memcpy(resA, fxi_res, sizeof resA); nA = fxi_nres; }
static int same_as_A(const char *what)
{
    uint32_t i;
    int ok = fxi_nres == nA;
    for (i = 0; ok && i < nA; i++) {
        const fxi_res_t *a = &resA[i], *b = &fxi_res[i];
        ok = !strcmp(a->name, b->name) && a->blocks == b->blocks && a->au == b->au && a->st == b->st && a->gov == b->gov && a->full == b->full && a->cm == b->cm && !memcmp(a->states, b->states, sizeof a->states);
        if (!ok) printf("  %s: %s differs: au %016llx/%016llx st %016llx/%016llx gov %u/%u\n", what, a->name, (unsigned long long)a->au, (unsigned long long)b->au, (unsigned long long)a->st, (unsigned long long)b->st, a->gov, b->gov);
    }
    return ok;
}
static void ck(const char *what, int ok)
{
    printf("fx dub module: %-150s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static int golden_ok(void)
{
    FILE *f = fopen(getenv("FXI_GOLDEN") ? getenv("FXI_GOLDEN") : "tests/fx_insert_golden.txt", "r");
    char line[256], mine[256];
    uint32_t i, ok = 0, want = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "R2 combinations", 15)) { want++; ok += !strcmp(line, "R2 combinations 128"); continue; }
        for (i = 0; i < nA; i++) {
            const fxi_res_t *r = &resA[i];
            snprintf(mine, sizeof mine, "%s blocks %u au %016llx st %016llx gov %u states %u %u %u %u %u full %u cm %u", r->name, r->blocks, (unsigned long long)r->au, (unsigned long long)r->st,
                     r->gov, r->states[0], r->states[1], r->states[2], r->states[3], r->states[4], r->full, r->cm);
            if (!strncmp(line, r->name, strlen(r->name)) && line[strlen(r->name)] == ' ') { want++; ok += !strcmp(line, mine); }
        }
    }
    fclose(f);
    return want == 8u && ok == 8u;               /* the five scenarios, R2's combination count, TRIG and PERF */
}
/* the descriptor: the module's words against the resident kernel's and the firmware's tables */
static void descriptor(void)
{
    const fx_caps_t *c = oc_host_fx_caps();
    const fx_kcalls_t *k = oc_host_fx_calls();
    const fx_prow_t *rows = oc_host_fx_rows();
    const fx_kernel_t *r = &dub_kernel;
    const fx_desc_wire_t *d = oc_host_fx_desc();
    const fx_entry_wire_t *e = oc_host_fx_entry();
    const fw_pages_t *pg = oc_host_fx_pages();
    uint32_t i, ok = 1;
    ck("the module's header is accepted by the registration check (identifier, table address, versions); kind 1, engine byte 0, state size 0",
       fw_module_check(&OC_MODULE_HDR) == FW_MOD_OK && FW_MOD_KIND(&OC_MODULE_HDR) == FW_MOD_KIND_INSERT && FW_MOD_ENGINE(&OC_MODULE_HDR) == 0u && fw_mod_word_ok(OC_MODULE_HDR.engine) &&
       OC_MODULE_HDR.state_size == 0u && OC_MODULE_HDR.pack_hash == OC_PACK_HASH && OC_MODULE_HDR.version == DUBK_VERSION);
    ck("the descriptor: one effect, module id 1, version 1.0.0, effect id 1, kind 1, no parameter table, no scene", d->n == 1u && d->module_id == DUBK_MODULE_ID && d->version == DUBK_VERSION &&
       e->effect_id == 1u && e->kind == FW_MOD_KIND_INSERT && e->flags == 0 && e->nparams == 0 && e->params == 0 && d->scene == 0);
    ck("the capability words are the resident kernel's, word for word", c->flags == r->flags && c->rates == r->rates && c->max_inst == r->max_inst && c->prof == r->prof && c->ram_work_min == r->ram_work_min &&
       c->ram_work_max == r->ram_work_max && c->ram_extra == r->ram_extra && c->ram_state == r->ram_state && c->latency == r->latency && c->tail_base == r->tail_base && c->cost == r->cost &&
       c->ram_extra_bytes == r->ram_extra_bytes && c->gov_us == r->gov_us && c->legacy_module == r->legacy_module && c->nrows == r->nrows && c->param_bytes == sizeof(dd_params_t));
    ck("the caps block: magic, size 52, version 1, no hot set, the padding 0", c->magic == FX_CAPS_MAGIC && c->size == FX_CAPS_WIRE && c->version == FX_CAPS_VERSION && c->hot == 0 && c->nhot == 0 && c->pad == 0);
    for (i = 0; i < DUBP_N; i++) ok &= rows[i].id == r->rows[i].id && rows[i].legacy == r->rows[i].legacy && rows[i].min == r->rows[i].min && rows[i].max == r->rows[i].max && rows[i].def == r->rows[i].def;
    ck("the module's legacy-slot rows (ids 1 .. 10) are the resident table's", ok && DUBP_N == 10u && rows[0].id == 1u && rows[9].id == 10u);
    ck("all twelve calls are there", k->on && k->size_sig && k->size_need && k->blocks_at && k->hold_len && k->tick && k->param_sig && k->snap && k->cost_extra && k->start && k->process && k->reset);
    ck("... and are the module's own code, not the resident's (distinct addresses)", k->snap != r->snap && k->process != r->process && k->param_sig != r->param_sig && k->on != r->on);
    /* the pages, in the device's wire layout through the loader's checker */
    {   uint32_t desc = 0;
        ok = fwp_wire(pg, wimg, sizeof wimg, WBASE, &desc) != 0;
        if (ok) {
            ck("DUB's pages (3 pages, 14 panel entries, 4 views) pass fw_pages_check_ex as SC_FX in this build", fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK);
            ck("... and are refused as an engine's", fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_MOD, P_COUNT, G_COUNT, 0) == FWP_ROW);
        } else ck("DUB's pages fit the wire image", 0);
    }
    /* the pid table against the firmware's own parameter table: legacy slots, ranges, defaults, labels */
    ok = 1;
    for (i = 0; i < 10u; i++) {
        const param_desc_t *fd = param_desc_of(ENGI_MACRO, pg->pid[i].legacy), *md = &pg->panel[i];
        ok &= pg->pid[i].id == i + 1u && pg->pid[i].legacy == rows[i].legacy && md->min == fd->min && md->max == fd->max && md->def == fd->def && (i >= 8u || !strcmp(md->label, fd->label));
    }
    ck("the panel's ten stored entries: ids 1 .. 10, the rows' legacy slots, and the firmware's own ranges, defaults and labels (MODE and TAPE have no label)", ok && !pg->panel[8].label[0] && !pg->panel[9].label[0]);
    /* the pages as the firmware shows them: DUB SETUP, DUB, DUB 2 */
    {   static const char *const titles[3] = {"DUB SETUP", "DUB", "DUB 2"};
        uint32_t p, found = 0;
        ok = 1;
        for (p = 0; p < 3u; p++) {
            uint32_t n, j;
            for (n = 0; n < page_n(); n++) {
                const page_t *fp = page_at(n);
                if (!fp || strcmp(fp->title, titles[p]) || fp->scope != SC_TRACK) continue;
                found++;
                ok &= !strcmp(pg->rows[p].title, fp->title) && pg->rows[p].fam == fp->fam && pg->rows[p].scope == SC_FX;
                for (j = 0; j < 4u; j++) {
                    uint32_t mi = pg->rows[p].id[j], want = fp->id[j], got;
                    got = pg->pid[mi].id >= 0x8000u ? 0xE0u + (pg->pid[mi].id & 0xFFu) - 1u : pg->pid[mi].legacy;      /* a view: the firmware's DUBF_ column; a stored id: its slot */
                    ok &= got == want;
                }
            }
        }
        ck("the three pages (titles, family, and the id of every knob) are the firmware's PAGES rows for DUB SETUP, DUB and DUB 2, through the pid table", ok && found == 3u);
    }
    /* the hooks against the firmware's own fields (dub_fields.h, params.c page_desc): the views and the TIME pair's format */
    {   int16_t v = 0;
        const param_desc_t *pd;
        uint32_t f, mode, tape;
        ok = 1;
        for (mode = 64; mode < 68; mode++)
            for (tape = 0; tape < 192; tape += 7) {
                trk[0].p[P_DUB_MODE] = (int16_t)mode; trk[0].p[P_DUB_TAPE] = (int16_t)tape - 64; trk[0].p[P_DUB_TIML] = 53; trk[0].p[P_DUB_TIMR] = 67;
                for (f = 0; f < 4u; f++) {
                    pd = pg->fx_shown(0, 0, 0x8001u + f, &v);
                    ok &= pd && v == dub_get_field(trk[0].p, DUBF_SYNC + f) && !strcmp(pd->label, f == 0 ? "SYNC" : f == 1 ? "PING" : f == 2 ? "DRIVE" : "WOW");
                }
                pd = pg->fx_shown(0, 0, 5u, &v); ok &= pd && v == 53 && pd->fmt == (dub_get_sync(trk[0].p) ? F_DUBTS : F_DUBTF);
                pd = pg->fx_shown(0, 0, 6u, &v); ok &= pd && v == 67 && pd->fmt == (dub_get_sync(trk[0].p) ? F_DUBTS : F_DUBTF);
                ok &= pg->fx_shown(0, 0, 1u, &v) == 0 && pg->fx_shown(0, 9u, 0x8001u, &v) == 0;     /* a stored id: the panel's entry and the byte; an instance past the tracks: nothing */
            }
        ck("shown: SYNC, PING, DRIVE, WOW read MODE and TAPE as the firmware's page does; TIME L and R change format with SYNC; a stored id and a bad instance answer 0", ok);
        ok = 1;
        for (f = 0; f < 4u; f++) {
            int16_t want[256];
            uint32_t val;
            for (val = 0; val < 16u; val++) {
                trk[0].p[P_DUB_MODE] = 65; trk[0].p[P_DUB_TAPE] = 8; trk[1].p[P_DUB_MODE] = 65; trk[1].p[P_DUB_TAPE] = 8;
                pg->fx_put(0, 0, 0x8001u + f, (int32_t)val);
                dub_put_field(trk[1].p, DUBF_SYNC + f, (int32_t)val);
                memcpy(want, trk[1].p, sizeof want);
                ok &= trk[0].p[P_DUB_MODE] == trk[1].p[P_DUB_MODE] && trk[0].p[P_DUB_TAPE] == trk[1].p[P_DUB_TAPE];
            }
        }
        pg->fx_put(0, 0, 5u, 77);
        ck("put: a view written back into MODE or TAPE is dub_put_field's (one 16 bit store); a stored id is the host's", ok && trk[0].p[P_DUB_TIML] == 53);
    }
}
int main(void)
{
    const fx_caps_t *c = oc_host_fx_caps();
    int rc;
    printf("fx dub module: the tracks' stored DUB values play the same through the module's kernel (tests/fx_dub_module_test.c)\n");
    fxi_progress = getenv("FXI_PROGRESS") != 0;
    oc_host_fw = &FW_API;
    descriptor();
    tapef = tmpfile();
    if (!tapef) { printf("fx dub module: no temporary file\n"); return 2; }
    /* A: the resident kernel, no module */
    ck("no module registered: every track is bound to the resident kernel", !fx_modk_ok && fx_mod_want == 0 && fx_mod_bound == 0 && fx_kernel_of(0) == &dub_kernel);
    tap_mode = 1; fxi_tap = tap;
    run_all(); keepA();
    ck("A  the resident kernel: seven scenarios ran (the probe's five, TRIG and PERF), no instance of a module kernel", nA == 7u && mod_run_blocks == 0 && res_run_blocks > 100000u);
    ck("A  .. and match the pinned printout of the tree before the cut (tests/fx_insert_golden.txt), line for line", golden_ok());
    printf("fx dub module: A wrote %llu blocks (%llu bytes)\n", (unsigned long long)tap_blocks, (unsigned long long)tap_blocks * sizeof rec);
    /* the module registers */
    rc = fx_modk_set(oc_host_fx_desc()->module_id, oc_host_fx_desc()->version, oc_host_fx_entry()->effect_id, c, oc_host_fx_calls(), oc_host_fx_rows());
    ck("the host takes the module's kernel (fx_modk_set)", rc == FXM_OK && fx_modk_ok && fx_modk.module_id == 1u && fx_modk.process == oc_host_fx_calls()->process && fx_kernel_of(0) == &dub_kernel);
    /* B: the module for all four tracks */
    fx_ins_use_module(15u);
    rewind(tapef); tap_mode = 2;
    run_all();
    ck("B  the module for all four tracks: instances of the module's kernel ran (blocks counted)", mod_run_blocks > 100000u && res_run_blocks == 0);
    ck("B  every output sample of all seven scenarios (the mixer's blocks) equals the resident run's", tap_diff == 0 && tap_blocks == (uint64_t)ftell(tapef) / sizeof rec);
    if (tap_diff) printf("  B: %llu blocks differ, the first is block %llu\n", (unsigned long long)tap_diff, (unsigned long long)tap_first);
    ck("B  .. and every scenario's hash of the samples, of the behaviour (states, rates, holds, memory, DUB FULL bits, the parameter snapshots), the governor's firings and the state counts", same_as_A("B"));
    if (getenv("FXDM_FAST")) { printf(bad ? "fx dub module: FAILED\n" : "fx dub module: A and B passed (fast)\n"); return bad != 0; }
    /* C: the module for tracks 0 and 2 */
    fx_ins_use_module(5u);
    rewind(tapef);
    run_all();
    ck("C  the module for tracks 0 and 2, the resident for 1 and 3: both kernels ran", mod_run_blocks > 20000u && res_run_blocks > 20000u);
    ck("C  .. every output sample equals the resident run's, every hash too", tap_diff == 0 && same_as_A("C"));
    /* D: registered, nothing chosen */
    fx_ins_use_module(0);
    rewind(tapef);
    run_all();
    ck("D  the module registered and no track set to it (the default): the resident kernel plays, every sample as A", mod_run_blocks == 0 && tap_diff == 0 && same_as_A("D"));
    fxi_tap = 0; fclose(tapef);
    /* a kernel switch under way, and the module going away under a running instance */
    {   uint32_t k, n = 0, ran = 0;
        fx_ins_use_module(0);
        fresh();
        for (k = 0; k < NPART; k++) dub_defaults(k);
        run(1500);
        for (k = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_RUN;
        ck("switch: four resident instances running", n == 4u && fx_mod_bound == 0);
        fx_ins_use_module(15u);
        run(3);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_RAMP || fx_ins[k].evict;
        ck("switch: choosing the module ramps the running instances out (they do not change kernels under way)", n == 4u && fx_mod_bound == 0);
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) { n += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN; ran += fx_kernel_of(k) == &fx_modk; }
        ck("switch: after the ramp the instances start again from silence on the module's kernel (bound, running)", n == 4u && ran == 4u && fx_modk_uses() == 4u);
        {   fx_ictx_t cx; (void)cx; }
        fx_modk_clear();
        ck("the module goes away: the bound instances let go at once, every track is the resident kernel's again", !fx_modk_ok && fx_mod_bound == 0 && fx_kernel_of(2) == &dub_kernel && fx_modk_uses() == 0);
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_RUN && !FX_MODK_BOUND(k);
        ck("... and the instances run again on the resident kernel (the track's setting still says module, with none registered)", n == 4u && fx_mod_want == 15u);
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, oc_host_fx_calls(), oc_host_fx_rows());
        ck("the module registers again: the tracks set to it move to it", rc == FXM_OK && (run(1500), 1) && fx_modk_uses() == 4u);
    }
    /* what the host refuses */
    {   fx_caps_t bad_c;
        int ok = 1;
        fx_modk_clear();
        bad_c = *c; bad_c.version = 2;                          ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_VER;
        bad_c = *c; bad_c.ram_work_max = FX_INS_MAXSEG + 1u;    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.max_inst = FX_INS_N + 1u;             ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.flags |= FXK_SEED;                    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.param_bytes = FX_PARAM_BYTES + 4u;    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.prof = AP_COUNT;                      ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.ram_work_min = 0;                     ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        { fx_kcalls_t k0 = *oc_host_fx_calls(); k0.snap = 0;    ok &= fx_modk_set(1u, 1u, 1u, c, &k0, oc_host_fx_rows()) == FXM_RANGE; }
        ck("a kernel the host cannot give is refused (caps version, blocks, instances, a seed, a snapshot too big, an unknown class, a missing call) and nothing is registered", ok && !fx_modk_ok);
    }
    if (bad) { printf("fx dub module: %d FAILED\n", bad); return 1; }
    printf("fx dub module: all passed\n");
    return 0;
}
