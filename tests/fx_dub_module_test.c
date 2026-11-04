/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 6, round 20 (docs/orangecat/FX_MODULES_PLAN.md 15, 15.9): DUB TAPE's kernel is a MODULE (tools/module/dub_mod.c) and the image has no copy of it. This test runs the module through the
 * generic insert host (fx_insert.c, fx_kernel_t) and checks the host around it.
 *
 * It is built with the module's translation unit (tests/module_host_dub_tu.c, the module source with OC_MODULE_HOST: its own object, its own statics, the firmware only through FW_API) and runs the
 * scenarios of tests/fx_insert_test.c (the probe: LONG, MEM, R2, HEAVY1, HEAVY2) and its TRIG and PERF through the real mixer:
 *   ABSENT  no module registered: a track with DUB on plays DRY (its output equals the same render with ECHO 0, bit for bit), no instance, no capture, no memory taken, nothing for the governor,
 *           the DUB FULL bit of every such track set (the DUB page says MODULE MISSING), and a project with DUB values round-trips unchanged;
 *   MODULE  the module registered: EVERY track's instance runs it (no per-track choice), and the render of the seven scenarios equals the pinned printout tests/fx_insert_golden.txt, which is the
 *           resident kernel's of d620160d (tests/fx_insert_diff.sh proves the same against that tree, sample for sample: the hash of every output sample and of the behaviour);
 *   then the module appearing after boot (DRY -> sounding), going away (-> DRY, nothing called afterwards, memory given back), swapped for one with another state size under instances in WAIT and CLEAR,
 *   the descriptor (the capability words, call table, legacy-slot rows and pages against the firmware's own parameter table and PAGES; the pages pass the loader's checker as SC_FX), and each
 *   refusal of fx_modk_set.
 * Arguments: none. Built and run by tests/fx_dub_module.sh (tests/run_tests.sh); the mutation check is the same script with --mutate. */
#define FXI_NO_MAIN 1
#define OC_FX_TEST_HOOK 1                                /* fx_insert.c: fx_modk_clear_step, one block between the unbind and the rebind */
#include "fx_insert_test.c"
#include "fw_pages_wire.h"
#include "fw_fx.h"
#include "dub_module_host.h"                         /* the externs of the module's host unit, DUB_STATE_BYTES */
#define DUBK_VERSION (1u << 16)                          /* the module's version word, 1.0.0 (tools/module/dub_mod.c) */
#define DUBK_MODULE_ID 1u

extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const fw_pages_t *oc_host_fx_pages(void);

static uint64_t mod_run_blocks;
static void tap(const int32_t *b, size_t n)             /* (fxi_tap) every output block: the blocks in which an instance of the module's kernel ran are counted (the module really ran) */
{
    uint32_t k;
    (void)b; (void)n;
    for (k = 0; k < FX_INS_N; k++) if (fx_ins[k].state == FXS_RUN && FX_MODK_BOUND(k)) mod_run_blocks++;
}
static void clear_step_run(void) { run(1); }                    /* (fx_modk_clear_step) one audio block in the middle of a removal */
static void run_all(void)
{
    fxi_nres = 0; fx_ins_gov_fires = 0; mod_run_blocks = 0;
    scenario_long(); scenario_mem(); scenario_r2(); scenario_heavy(1); scenario_heavy(2); scenario_trig(); scenario_perf();
}
#define WBASE 0x10200000u
static uint8_t wimg[8192];
static int wrd(void *ctx, uint32_t addr, void *buf, uint32_t n) { (void)ctx; if (addr < WBASE || addr - WBASE + n > sizeof wimg) return 1; memcpy(buf, wimg + (addr - WBASE), n); return 0; }
static fxi_res_t resA[8];
static uint32_t nA;
static void keepA(void) { memcpy(resA, fxi_res, sizeof resA); nA = fxi_nres; }
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
    const fx_desc_wire_t *d = oc_host_fx_desc();
    const fx_entry_wire_t *e = oc_host_fx_entry();
    const fw_pages_t *pg = oc_host_fx_pages();
    uint32_t i, ok = 1;
    ck("the module's header is accepted by the registration check (identifier, table address, versions); kind 1, engine byte 0, state size 0",
       fw_module_check(&OC_MODULE_HDR) == FW_MOD_OK && FW_MOD_KIND(&OC_MODULE_HDR) == FW_MOD_KIND_INSERT && FW_MOD_ENGINE(&OC_MODULE_HDR) == 0u && fw_mod_word_ok(OC_MODULE_HDR.engine) &&
       OC_MODULE_HDR.state_size == 0u && OC_MODULE_HDR.pack_hash == OC_PACK_HASH && OC_MODULE_HDR.version == DUBK_VERSION);
    ck("the descriptor: one effect, module id 1, version 1.0.0, effect id 1, kind 1, no parameter table, no scene", d->n == 1u && d->module_id == DUBK_MODULE_ID && d->version == DUBK_VERSION &&
       e->effect_id == 1u && e->kind == FW_MOD_KIND_INSERT && e->flags == 0 && e->nparams == 0 && e->params == 0 && d->scene == 0);
    ck("the capability words are the plan's column for DUB TAPE: cost 190 us, governor 450 us, 4 .. 16 work blocks, the spring 2 blocks / 5,632 B, state = dd_t, latency 61, tail 24 blocks, 4 instances",
       c->flags == (FXK_US | FXK_TAIL_FAR) && c->rates == 0x06 && c->max_inst == FX_INS_N && c->prof == AP_DUB && c->ram_work_min == 4 && c->ram_work_max == 16 && c->ram_extra == 2 &&
       c->ram_state == DUB_STATE_BYTES && c->latency == 61 && c->tail_base == 24 && c->cost == 190 && c->ram_extra_bytes == 5632 && c->gov_us == 450 && c->legacy_module == 0x44554232u &&
       c->nrows == 10 && c->param_bytes == sizeof(dd_params_t));
    ck("the caps block: magic, size 56, version 1, the abi of this build (FX_ABI_ID), no hot set, the padding 0", c->magic == FX_CAPS_MAGIC && c->size == FX_CAPS_WIRE && c->version == FX_CAPS_VERSION && c->abi == FX_ABI_ID && c->hot == 0 && c->nhot == 0 && c->pad == 0);
    {   static const uint16_t slots[10] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47 };
        for (i = 0; i < 10u; i++) {
            const param_desc_t *fd = param_desc_of(ENGI_MACRO, slots[i]);
            ok &= rows[i].legacy == slots[i] && rows[i].id == i + 1u && rows[i].min == fd->min && rows[i].max == fd->max && rows[i].def == fd->def;
        }
    }
    ck("the module's legacy-slot rows (ids 1 .. 10) sit on the Release 2 slots with the firmware's own ranges and defaults", ok && rows[0].id == 1u && rows[9].id == 10u);
    ck("all twelve calls are there", k->on && k->size_sig && k->size_need && k->blocks_at && k->hold_len && k->tick && k->param_sig && k->snap && k->cost_extra && k->start && k->process && k->reset);
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
/* the module's call table behind counters: after the module is removed not one call may go to it (a stale pointer into the flash that is gone would show here) */
static fx_kcalls_t cnt_tab;
static uint32_t cnt_n[12];
static const fx_kcalls_t *cnt_real;
static uint32_t cw_on(const track_t *t) { cnt_n[0]++; return cnt_real->on(t); }
static uint32_t cw_size_sig(const track_t *t, const fx_ictx_t *c) { cnt_n[1]++; return cnt_real->size_sig(t, c); }
static float cw_size_need(const track_t *t, const fx_ictx_t *c) { cnt_n[2]++; return cnt_real->size_need(t, c); }
static uint32_t cw_blocks_at(float need, uint32_t R) { cnt_n[3]++; return cnt_real->blocks_at(need, R); }
static uint32_t cw_hold_len(float need, uint32_t blocks, uint32_t R) { cnt_n[4]++; return cnt_real->hold_len(need, blocks, R); }
static void cw_tick(uint32_t *kw, const fx_ictx_t *c) { cnt_n[5]++; cnt_real->tick(kw, c); }
static uint32_t cw_param_sig(const track_t *t, const fx_ictx_t *c) { cnt_n[6]++; return cnt_real->param_sig(t, c); }
static void cw_snap(void *p, const track_t *t, fx_ictx_t *c) { cnt_n[7]++; cnt_real->snap(p, t, c); }
static uint32_t cw_cost_extra(const track_t *t, uint32_t e) { cnt_n[8]++; return cnt_real->cost_extra(t, e); }
static void cw_start(void *st, uint32_t R, int16_t *const *blk, uint32_t nblk, int16_t *extra) { cnt_n[9]++; cnt_real->start(st, R, blk, nblk, extra); }
static void cw_process(void *st, const void *p, const float *in, float *l, float *r) { cnt_n[10]++; cnt_real->process(st, p, in, l, r); }
static void cw_reset(void) { cnt_n[11]++; cnt_real->reset(); }
static void counted_reset(void) { memset(cnt_n, 0, sizeof cnt_n); }
static const fx_kcalls_t *counted_calls(void)
{
    cnt_real = oc_host_fx_calls();
    cnt_tab.on = cw_on; cnt_tab.size_sig = cw_size_sig; cnt_tab.size_need = cw_size_need; cnt_tab.blocks_at = cw_blocks_at; cnt_tab.hold_len = cw_hold_len; cnt_tab.tick = cw_tick;
    cnt_tab.param_sig = cw_param_sig; cnt_tab.snap = cw_snap; cnt_tab.cost_extra = cw_cost_extra; cnt_tab.start = cw_start; cnt_tab.process = cw_process; cnt_tab.reset = cw_reset;
    return &cnt_tab;
}
static uint32_t cnt_total(void) { uint32_t i, n = 0; for (i = 0; i < 12u; i++) n += cnt_n[i]; return n; }
static uint32_t cnt_snap(void) { return cnt_n[7]; }
static uint32_t cnt_process(void) { return cnt_n[10]; }
#define ST_SZ(k) (resource[RES_JFX0 + JB_INSS0 + (k)].size)
/* (round 20) no DUB module registered: the tracks that have DUB on play dry, say so, and cost nothing; a project with DUB values keeps them */
static void absent_checks(void)
{
    uint32_t k, i, dry_ok = 1, mem = 0;
    uint64_t au_on = 0, au_off = 0;
    uint32_t gov0, full_on = 0, full_off = 0, states = 0, cap = 0, cm_on = 0;
    int ph;
    fx_modk_clear();
    ck("no module registered: nothing is bound, no kernel is called for, and the UI is told (fx_ins_module_ok)", !fx_modk_ok && fx_mod_effective() == 0 && fx_mod_bound == 0 && !fx_ins_module_ok());
    for (ph = 0; ph < 2; ph++) {                                  /* 0: ECHO 100 on every track, 1: ECHO 0 */
        fresh();
        for (k = 0; k < NPART; k++) { dub_defaults(k); T(k, P_DUB_SPRG, 60); if (ph) T(k, P_DUB_ECHO, 0); }
        gov0 = (uint32_t)fx_ins_gov_fires;
        begin(ph ? "DRY0" : "DRY1");
        run(900);
        for (k = 0; k < FX_INS_N; k++) {
            states |= fx_ins[k].state; cap |= fx_ins_capture(k) != 0; mem |= fx_ins_have[k] | (fx_ins_state[k] != 0) | (fx_ins_spbuf[k] != 0) | (uint32_t)(ST_SZ(k) != 0) | (uint32_t)(resource[RES_JFX0 + JB_INSS0 + k].size != 0);
            dry_ok &= fx_ins[k].run == 0 && !fx_ins[k].hold;
        }
        cm_on |= fx_cm != 0;
        if (ph) full_off = jfx_full & 0xF0u; else full_on = jfx_full & 0xF0u;
        if (ph) au_off = h_au; else au_on = h_au;
        ck(ph ? "ABSENT, ECHO 0: no governor entry, no firing" : "ABSENT, ECHO 100 on four tracks: no governor entry, no firing", (uint32_t)fx_ins_gov_fires == gov0 && !fx_ins_gov_req);
        end(ph ? "DRY0" : "DRY1");
    }
    (void)i;
    ck("ABSENT: no instance ever left OFF, no capture buffer, no state, tank or line, no common block: no memory taken at all", states == 0 && cap == 0 && mem == 0 && cm_on == 0 && dry_ok);
    ck("ABSENT: a track with DUB on plays DRY: its output is the render with ECHO 0, bit for bit", au_on == au_off);
    ck("ABSENT: ... and says so: the DUB FULL bit of each of the four tracks is set (the DUB page reads MODULE MISSING), none with ECHO 0", full_on == 0xF0u && full_off == 0);
    /* a project with DUB values and the module absent: the ten stored values load back as they were saved (nothing converts them, nothing resets them) */
    {   static project_t a, b;
        static project_store_t st;
        static const uint8_t ids[10] = { P_JRES0, P_JRES1, P_JRES2, P_JRES3, P_JRES4, P_JRES5, P_JRES6, P_RESERVED45, P_RESERVED46, P_RESERVED47 };
        uint32_t ok = 1;
        blank_project(&a);
        for (k = 0; k < NPART; k++) {
            a.t[k].p[P_DUB_FDBK] = (int16_t)(40 + 5 * (int)k); a.t[k].p[P_DUB_SEND] = (int16_t)(127 - 9 * (int)k); a.t[k].p[P_DUB_FILT] = (int16_t)(30 + 17 * (int)k); a.t[k].p[P_DUB_ECHO] = (int16_t)(100 - 7 * (int)k);
            a.t[k].p[P_DUB_TIML] = (int16_t)(53 + k); a.t[k].p[P_DUB_TIMR] = (int16_t)(67 + k); a.t[k].p[P_DUB_SPRG] = (int16_t)(11 * (k + 1)); a.t[k].p[P_DUB_SPD] = (int16_t)(64 + 3 * (int)k);
            a.t[k].p[P_DUB_MODE] = (int16_t)(DUB_MODE_MARK | (k & 3u)); a.t[k].p[P_DUB_TAPE] = (int16_t)(6 * DUB_WOW_N - 64 + (int)k);
        }
        a.sum = proj_sum(&a);
        ok = proj_pack(&st, &a) && proj_import(&b, &st, sizeof st);
        for (k = 0; ok && k < NPART; k++) for (i = 0; i < 10u; i++) ok &= b.t[k].p[ids[i]] == a.t[k].p[ids[i]];
        ck("ABSENT: a project with DUB on (ten stored values on four tracks) packs and loads back with every value unchanged; the layout is the new one", ok && dub_new_layout(b.t[0].p));
    }
}
int main(void)
{
    const fx_caps_t *c = oc_host_fx_caps();
    int rc;
    uint32_t k, n;
    printf("fx dub module: DUB TAPE is the module's kernel only: dry without it, the pinned render with it (tests/fx_dub_module_test.c)\n");
    fxi_progress = getenv("FXI_PROGRESS") != 0;
    oc_host_fw = &FW_API;
    descriptor();
    absent_checks();
    /* the module registers: every track's instance runs it (there is no per-track choice) */
    fxi_tap = tap;
    rc = fx_modk_set(oc_host_fx_desc()->module_id, oc_host_fx_desc()->version, oc_host_fx_entry()->effect_id, c, oc_host_fx_calls(), oc_host_fx_rows());
    ck("the host takes the module's kernel (fx_modk_set): it is for every track, with nothing to choose", rc == FXM_OK && fx_modk_ok && fx_ins_module_ok() && fx_modk.module_id == 1u &&
       fx_modk.process == oc_host_fx_calls()->process && fx_mod_effective() == 15u);
    run_all(); keepA();
    ck("MODULE: seven scenarios ran (the probe's five, TRIG and PERF) on the module's kernel on every track (more than 100,000 instance-blocks in RUN, counted)", nA == 7u && mod_run_blocks > 100000u);
    ck("MODULE: ... and match the pinned printout (tests/fx_insert_golden.txt, the resident kernel of d620160d): the hash of every output sample and of the behaviour of every scenario, line for line", golden_ok());
    if (getenv("FXDM_FAST")) { printf(bad ? "fx dub module: FAILED\n" : "fx dub module: the module's render passed (fast)\n"); return bad != 0; }
    fxi_tap = 0;
    /* the module appearing after boot (dry -> sounding), and going away (-> dry): the instances switch only from silence; once the module is gone nothing calls it */
    {   uint32_t before;
        fx_modk_clear();
        counted_reset();
        fresh();
        for (k = 0; k < NPART; k++) dub_defaults(k);
        run(600);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_OFF && fx_ins_have[k] == 0;
        ck("appear: four tracks with DUB on and no module: dry, no instance, no memory (DUB FULL on all four)", n == 4u && cnt_total() == 0 && (jfx_full & 0xF0u) == 0xF0u && fx_cm == 0);
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
        ck("appear: the module registers after boot: for every track, the UI is told", rc == FXM_OK && fx_mod_effective() == 15u && fx_ins_module_ok());
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN && fx_kernel_of(k) == &fx_modk;
        ck("appear: the instances start from silence on the module's kernel, all four, with their memory; the module's snapshot and process run; DUB FULL clears", n == 4u && fx_modk_uses() == 4u && cnt_snap() > 0 && cnt_process() > 0 &&
           (jfx_full & 0xF0u) == 0 && fx_cm != 0);
        before = cnt_total();
        fx_modk_clear();
        ck("remove: the module goes away: the bound instances let go at once (state OFF, no memory wanted), the host's copy of its calls is zero, the UI is told",
           !fx_modk_ok && !fx_ins_module_ok() && fx_mod_bound == 0 && fx_mod_effective() == 0 && fx_modk_uses() == 0 && fx_modk.process == 0 && fx_modk.snap == 0 && fx_modk.start == 0 &&
           fx_ins[0].state == FXS_OFF && fx_ins[3].state == FXS_OFF && fx_ins_want[0] == 0 && fx_ins_want[3] == 0);
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_OFF && fx_ins_have[k] == 0 && fx_ins_state[k] == 0 && fx_ins_spbuf[k] == 0;
        ck("remove: back to DRY: no instance, the memory given back (state, tank, lines, common block), DUB FULL on the four tracks, and not one call went into the removed module (no stale pointer: 1,500 blocks)",
           n == 4u && fx_cm == 0 && (jfx_full & 0xF0u) == 0xF0u && cnt_total() == before);
        /* removed while an instance is ramping or waiting: the same */
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
        run(40);                                                    /* the memory is being taken, the ramp-in has begun */
        before = cnt_total();
        fx_modk_clear();
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += fx_ins[k].state == FXS_OFF && fx_ins_have[k] == 0;
        ck("remove during the start: back to dry and the module is never called again", rc == FXM_OK && n == 4u && cnt_total() == before && fx_cm == 0);
        /* reinstall: remove and register in a row (an in-place reinstall) */
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
        run(1500);
        fx_modk_clear();
        rc |= fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN;
        ck("reinstall (a clear and a registration in a row): every instance ends running on the module's kernel", rc == FXM_OK && n == 4u && fx_modk_uses() == 4u);
        before = cnt_total();
        fx_modk_clear();
        run(300);
        ck("... and a last removal leaves the tracks dry with no further call", cnt_total() == before && fx_modk_uses() == 0 && (jfx_full & 0xF0u) == 0xF0u);
    }
    /* a module swapped for one with another state size while the instances are in WAIT or CLEAR (nothing plays yet): the switch happens in that pass, and an instance changing modules gives back
     * what it holds and takes it again sized by the NEW one (a module that declares a larger state than DUB's, here, both ways) */
    {   fx_caps_t big = *c;
        uint32_t want_big, want_res, m;
        int ph;
        big.ram_state = (uint16_t)(c->ram_state + 256u);
        want_big = ((uint32_t)big.ram_state + 3u) & ~3u; want_res = ((uint32_t)c->ram_state + 3u) & ~3u;
        for (ph = 0; ph < 2; ph++) {                                  /* 0: WAIT, 1: CLEAR */
            fx_modk_clear(); fresh(); for (k = 0; k < NPART; k++) dub_defaults(k);
            rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
            run(1);
            for (k = 0, n = 0; k < FX_INS_N; k++) n += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_WAIT;
            if (ph) for (k = 0; k < FX_INS_N; k++) { fx_ins[k].state = FXS_CLEAR; fx_ins[k].clr = 0; fx_ins[k].pend_R = fx_ins[k].R; }
            ck(ph ? "swap in CLEAR: four instances bound to the module, put in CLEAR (the memory is still being taken)" : "swap in WAIT: four instances bound to the module, in WAIT (the memory is still being taken)", rc == FXM_OK && n == 4u);
            rc = fx_modk_set(1u, DUBK_VERSION, 1u, &big, counted_calls(), oc_host_fx_rows());
            run(3000);
            for (k = 0, m = 0; k < FX_INS_N; k++) m += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN && ST_SZ(k) == want_big;
            ck(ph ? "... the module swapped under CLEAR: every instance ends bound to the new one, running, on a state taken at ITS size (no livelock)" : "... the module swapped under WAIT: every instance ends bound to the new one, running, on a state taken at ITS size (no livelock)",
               rc == FXM_OK && m == 4u && fx_modk_uses() == 4u && want_big != want_res);
            fx_modk_clear(); fresh(); for (k = 0; k < NPART; k++) dub_defaults(k);
            rc = fx_modk_set(1u, DUBK_VERSION, 1u, &big, counted_calls(), oc_host_fx_rows());
            run(1);
            if (ph) for (k = 0; k < FX_INS_N; k++) { fx_ins[k].state = FXS_CLEAR; fx_ins[k].clr = 0; fx_ins[k].pend_R = fx_ins[k].R; }
            rc |= fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
            run(3000);
            for (k = 0, m = 0; k < FX_INS_N; k++) m += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN && ST_SZ(k) == want_res;
            ck(ph ? "... and back, under CLEAR: the state is taken again at the smaller size" : "... and back, under WAIT: the state is taken again at the smaller size", rc == FXM_OK && m == 4u);
        }
        /* a block (an interrupt) between the unbind and the rebind of a removal: nothing walks (the host plays dry from the unbind), what the instances held is given back, and the gate word is forgotten */
        fx_modk_clear(); fresh(); for (k = 0; k < NPART; k++) dub_defaults(k);
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, &big, counted_calls(), oc_host_fx_rows());
        run(1500);
        for (k = 0, n = 0; k < FX_INS_N; k++) n += FX_MODK_BOUND(k) && fx_ins[k].state == FXS_RUN && ST_SZ(k) == want_big;
        fx_modk_clear_step = clear_step_run;
        fx_ins_gen[0] = resource_gen + 1u;                              /* "short, nothing released since" for instance 0: must be forgotten by the switch */
        fx_modk_clear();
        fx_modk_clear_step = 0;
        {   uint32_t g0 = fx_ins_gen[0];
            run(600);
            for (k = 0, m = 0; k < FX_INS_N; k++) m += fx_ins[k].state == FXS_OFF && ST_SZ(k) == 0 && fx_ins_have[k] == 0;
            ck("remove with a block between the unbind and the rebind: all four dry, the module-sized state and everything else given back", rc == FXM_OK && n == 4u && m == 4u && fx_cm == 0);
            ck("... and the switch forgot 'short, nothing released since' (fx_ins_gen), so an instance that was short of memory asks for the new kernel's size at once", g0 == 0u); }
    }
    /* (round 18, found on the unit) the spring's tank on a start from nothing: the common block, the state and six lines (a free TIME R of 409 ms) are the block's four takes twice over, so the
     * tank is the NINTH take of a start and jfx_take refuses it for the per-block budget, not for want of memory. That was remembered as "short, nothing released since" and the instance started
     * without its spring for good (SPRG and KICK did nothing). The tank is now waited for: one track from nothing: running, six lines, the tank held */
    {   fx_modk_clear(); fresh();
        rc = fx_modk_set(1u, DUBK_VERSION, 1u, c, counted_calls(), oc_host_fx_rows());
        for (k = 0; k < NPART; k++) { dub_defaults(k); T(k, P_DUB_ECHO, k ? 0 : 100); }
        dub_put_field(trk[0].p, DUBF_SYNC, 0); T(0, P_DUB_TIML, 70); T(0, P_DUB_TIMR, 90); T(0, P_DUB_SPRG, 60);
        for (k = 0; k < 400u; k++) { if (!(gb & 3u)) dub_late_half(0, 1200u); blk(); }   /* (no notes: no engine takes the arena's memory in this run, so the tank is refused for the budget alone) */
        ck("the module, one track from nothing (cm, state, 6 lines, then the tank): running, 6 lines, and the spring's tank held", rc == FXM_OK && fx_ins[0].state == FXS_RUN && fx_ins_have[0] == 6u && fx_ins_spbuf[0] != 0 && FX_MODK_BOUND(0));
        fx_modk_clear();
    }
#undef ST_SZ
    /* what the host refuses */
    {   fx_caps_t bad_c;
        int ok = 1;
        fx_modk_clear();
        bad_c = *c; bad_c.version = 2;                          ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_VER;
        bad_c = *c; bad_c.abi ^= 1u;                            ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_VER;
        bad_c = *c; bad_c.ram_work_max = FX_INS_MAXSEG + 1u;    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.max_inst = FX_INS_N + 1u;             ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.flags |= FXK_SEED;                    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.param_bytes = FX_PARAM_BYTES + 4u;    ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.prof = AP_COUNT;                      ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.ram_work_min = 0;                     ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.max_inst = FX_INS_N - 1u;             ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;   /* every track gets an instance */
        bad_c = *c; bad_c.ram_state = 0;                        ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.ram_state = (uint16_t)(FX_STATE_MAX + 1u); ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        bad_c = *c; bad_c.ram_extra_bytes = (uint16_t)(c->ram_extra * 4096u + 4u); ok &= fx_modk_set(1u, 1u, 1u, &bad_c, oc_host_fx_calls(), oc_host_fx_rows()) == FXM_RANGE;
        { fx_kcalls_t k0 = *oc_host_fx_calls(); k0.snap = 0;    ok &= fx_modk_set(1u, 1u, 1u, c, &k0, oc_host_fx_rows()) == FXM_RANGE; }
        ck("a kernel the host cannot give is refused (caps version, abi, blocks, instances (fewer than the tracks too), a state of 0 or past what the host lends, an option larger than its blocks, a seed, a snapshot too big, an unknown class, a missing call) and nothing is registered", ok && !fx_modk_ok);
    }
    if (bad) { printf("fx dub module: %d FAILED\n", bad); return 1; }
    printf("fx dub module: all passed\n");
    return 0;
}
