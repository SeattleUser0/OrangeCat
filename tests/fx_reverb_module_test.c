/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8a (docs/orangecat/FX_MODULES_PLAN.md 16): the reverb modes SPRING, SHIMMER and RESO as ONE effect module (kind 2; tools/module/reverb_mod.c) BESIDE the resident copy. THE GATE.
 *
 * Built with the module's translation unit (tests/module_host_reverb_tu.c: the module source with OC_MODULE_HOST, its own object, its own copy of the kernel text, reaching the firmware only
 * through FW_API and the context of each call). It runs the probe's scenarios (tests/fx_reverb_probe.c: every knob of each mode, the mode changes, the arena short, the rest, a project load, the
 * full mixes) through the real mixer:
 *   A  resident: no module registered. Every output block is kept; the printout equals tests/fx_reverb_golden.txt, pinned from the resident code before the cut
 *   B  the module registered: every output block compared with A's, sample for sample, and the behaviour hash; the module's three calls are behind counters (they ran, each mode)
 *   C  the module appearing and going away every 61 blocks in the middle of the render (the host owns the state, the tank and the line: the swap must not change one sample)
 *   D  the module gone: identical again, and not one call reaches it
 * then the host around it: each refusal of fx2_modk_set (nothing registered), the crash mark (taken once, when a module mode first runs; not for ROOM or CLOUD), the Effects table's `uses`,
 * the loader's commit and its erase guard, and the descriptor: the caps words, and each mode's pages against what the firmware shows today for that TYPE (the REVERB and REVERB 2 rows of
 * PAGES, global_desc for every knob) and against Session C's data (tests/fx_reverb_data.h), through the loader's checker as SC_FX.
 * With -DOC_REV_RESIDENT=0 (the image of the Nightly AFTER this one: no resident copy of the three modes) the same file checks what happens then: without the module a module mode is not run
 * (the return silent, the dry mix as it is); with it the render is the pinned resident printout.
 * Arguments: none. RVM_STOPFIRST=1: stop at the first differing block (the mutation check). Built and run by tests/fx_reverb_module.sh (tests/run_tests.sh). */
#define RVP_NO_MAIN 1
#include "fx_reverb_probe.c"
#include "fw_pages_wire.h"
#include "fw_fx.h"
#include "rev_module_host.h"

static void ck(const char *what, int ok)
{
    printf("fx reverb module: %-170s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* ---- the module's calls behind counters ---- */
static uint64_t mcalls[FX2_MODES];
static void w_spring(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n) { mcalls[1]++; oc_host_rev_calls(0)->process(c, in, out, n); }
static void w_shimmer(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n) { mcalls[2]++; oc_host_rev_calls(1)->process(c, in, out, n); }
static void w_reso(const fx2_ctx_t *c, const int32_t *in, int32_t *out, uint32_t n) { mcalls[3]++; oc_host_rev_calls(2)->process(c, in, out, n); }
static const fx2_kcalls_t WRAP[3] = { {w_spring}, {w_shimmer}, {w_reso} };
static int module_set(void)
{
    fx2_reg_t r[3];
    oc_host_rev_fw = &FW_API;
    for (uint32_t k = 0; k < 3u; k++) { r[k].effect_id = oc_host_rev_entry(k)->effect_id; r[k].caps = oc_host_rev_caps(k); r[k].calls = &WRAP[k]; }
    return fx2_modk_set(oc_host_rev_desc()->module_id, oc_host_rev_desc()->version, 3, r);
}
/* ---- every block of run A kept; the later runs compared with it at the tap ---- */
#define RVM_BLOCKS 170000u
static int32_t (*keep)[CTL * 2];
static uint32_t keep_n, cmp_at, cmp_bad, toggle;
static int stopfirst;
static char first_bad[120];
static void tap_keep(const char *sc, uint32_t b, const int32_t *pcm, size_t bytes)
{
    (void)sc; (void)b;
    if (keep_n < RVM_BLOCKS) memcpy(keep[keep_n], pcm, bytes);
    keep_n++;
}
static void tap_cmp(const char *sc, uint32_t b, const int32_t *pcm, size_t bytes)
{
    if (cmp_at >= keep_n || memcmp(keep[cmp_at], pcm, bytes)) {
        if (!cmp_bad) {
            snprintf(first_bad, sizeof first_bad, "%s block %u differs from the resident render", sc, (unsigned)b);
            if (stopfirst) { printf("fx reverb module: FAIL: %s\n", first_bad); exit(1); }
        }
        cmp_bad++;
    }
    cmp_at++;
    if (toggle && !(cmp_at % toggle)) { if (fx2_modk_ok()) fx2_modk_clear(); else (void)module_set(); }   /* run C: between two blocks, as the unit's loader does it (the audio interrupt masked) */
}
static rvp_res_t resA[16];
static uint32_t nA;
static int same_as_A(void)
{
    uint32_t i, ok = rvp_nres == nA;
    for (i = 0; ok && i < nA; i++) ok = !strcmp(rvp_res[i].name, resA[i].name) && rvp_res[i].blocks == resA[i].blocks && rvp_res[i].au == resA[i].au && rvp_res[i].st == resA[i].st;
    return (int)ok;
}
/* the printout of this run against the pinned one (the section of this build's MELODEE_FX_DEFER) */
static int golden_ok(uint32_t mask)                    /* mask: the scenarios compared, a bit each in the printout's order */
{
    FILE *f = fopen(getenv("RVP_GOLDEN") ? getenv("RVP_GOLDEN") : "tests/fx_reverb_golden.txt", "r");
    char line[256], mine[256], head[64];
    uint32_t i = 0, in = 0, ok = 1;
    if (!f) return 0;
    snprintf(head, sizeof head, "fx_reverb probe: MELODEE_FX_DEFER %d", (int)MELODEE_FX_DEFER);
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "fx_reverb probe:", 16)) { in = !strcmp(line, head); continue; }
        if (!in || !strncmp(line, "fx_reverb:", 10)) continue;
        if (i >= rvp_nres) { ok = 0; break; }
        if (!((mask >> i) & 1u)) { i++; continue; }
        snprintf(mine, sizeof mine, "%s blocks %u au %016llx st %016llx", rvp_res[i].name, rvp_res[i].blocks, (unsigned long long)rvp_res[i].au, (unsigned long long)rvp_res[i].st);
        if (strcmp(mine, line)) { ok = 0; printf("  golden: %s\n  this:   %s\n", line, mine); }
        i++;
    }
    fclose(f);
    return ok && i == rvp_nres && i == 12u;
}
static uint32_t note_n;
static void note_hook(uint32_t id) { if (id == FX2_MODULE_ID) note_n++; }
#define WBASE 0x10200000u
static uint8_t wimg[8192];
static int wrd(void *ctx, uint32_t addr, void *buf, uint32_t n) { (void)ctx; if (addr < WBASE || addr - WBASE + n > sizeof wimg) return 1; memcpy(buf, wimg + (addr - WBASE), n); return 0; }
#include "fx_reverb_data.h"                            /* Session C's descriptors (the checker's test data): make_reverb(d, k), k 0 SPRING, 1 SHIMMER, 2 RESO */

static void run_cmp(const char *what, uint32_t tog)
{
    char t[260];
    cmp_at = 0; cmp_bad = 0; toggle = tog; first_bad[0] = 0;
    memset(mcalls, 0, sizeof mcalls);
    rvp_tap = tap_cmp;
    rvp_scenarios();
    snprintf(t, sizeof t, "%s: every output block equals the resident render's (%u blocks, %u differ%s%s) and the hashes of the twelve scenarios (the samples and the behaviour) are equal",
             what, (unsigned)cmp_at, (unsigned)cmp_bad, cmp_bad ? "; the first: " : "", first_bad);
    ck(t, !cmp_bad && cmp_at == keep_n && same_as_A());
}
static void descriptor_checks(void)
{
    static const uint8_t LINE[3] = { 0, 1, 0 };
    static const char *const NM[3] = { "SPRING", "SHIMMER", "RESO" };
    const fx_desc_wire_t *d = oc_host_rev_desc();
    uint32_t k, i, ok;
    ck("the bank: module id 2 (FX2_MODULE_ID, docs/orangecat/MODULE_IDS.md), version 1.0.0, three effects, no scene; the header's word is kind 2 with no engine number, state size 0",
       d->n == 3u && d->module_id == 2u && d->module_id == FX2_MODULE_ID && d->version == 0x00010000u && d->scene == 0 && OC_REVERB_MODULE_HDR.engine == (FW_MOD_KIND_SEND << 8) &&
       OC_REVERB_MODULE_HDR.state_size == 0 && OC_REVERB_MODULE_HDR.iface == FW_API_IFACE && fw_module_check(&OC_REVERB_MODULE_HDR) == FW_MOD_OK);
    for (k = 0; k < 3u; k++) {
        const fx_caps_t *c = oc_host_rev_caps(k);
        const fx_entry_wire_t *e = oc_host_rev_entry(k);
        const fw_pages_t *pg = oc_host_rev_pages(k);
        fw_pages_t sc;
        uint32_t desc = 0, found = 0;
        char t[300];
        snprintf(t, sizeof t, "%s: the entry is effect %u (the mode), kind 2, no flags, no parameter table; its caps block: GFXK, 56 bytes, version 1, abi FX2_ABI_ID (not FX_ABI_ID), one instance, no seed, "
                 "the bus's rate, no state / option / hot set of its own, the model line %u block(s), the snapshot 2 * G_COUNT bytes, the AP_FX class", NM[k], (unsigned)k + 1u, LINE[k]);
        ck(t, e->effect_id == k + 1u && e->kind == FW_MOD_KIND_SEND && !e->flags && !e->nparams && !e->params && c->magic == FX_CAPS_MAGIC && c->size == FX_CAPS_WIRE && c->version == FX_CAPS_VERSION &&
           c->abi == FX2_ABI_ID && FX2_ABI_ID != FX_ABI_ID && c->max_inst == 1 && c->flags == FXK_TAIL_SILENT && c->rates == 1 && !c->ram_state && !c->ram_extra && !c->ram_extra_bytes && !c->hot && !c->nhot &&
           !c->rows && !c->nrows && c->ram_work_min == LINE[k] && c->ram_work_max == LINE[k] && c->param_bytes == 2u * G_COUNT && c->prof == AP_FX && c->legacy_module == 0x52455632u);
        /* the pages through the loader's checker, out of the device's wire bytes */
        ok = fwp_wire(pg, wimg, sizeof wimg, WBASE, &desc) != 0;
        snprintf(t, sizeof t, "%s: its pages pass the loader's checker as an effect's (SC_FX: song.g legacy slots, the TYPE condition, the host's K1) and are refused as an engine's", NM[k]);
        ck(t, ok && fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK &&
           fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_MOD, P_COUNT, G_COUNT, 0) != FWP_OK);
        /* against what the firmware shows today for this TYPE: the REVERB and REVERB 2 rows of PAGES, global_desc for every knob */
        ok = pg->npages == 2u && pg->npanel == 7u && !pg->fx_shown && !pg->fx_put && pg->ext == ((G_RTYPE + 1u) | ((k + 1u) << 16));
        memset(&song, 0, sizeof song);
        song.g[G_RTYPE] = (int16_t)(k + 1u);
        for (i = 0; i < NPAGES; i++) {
            uint32_t r = !strcmp(PAGES[i].title, "REVERB") ? 0u : !strcmp(PAGES[i].title, "REVERB 2") ? 1u : 2u, j;
            if (r > 1u) continue;
            found++;
            ok &= !strcmp(pg->rows[r].title, PAGES[i].title) && pg->rows[r].fam == PAGES[i].fam && PAGES[i].scope == SC_GLOBAL && pg->rows[r].scope == SC_FX && pg->rows[r].graph == PAGES[i].graph;
            for (j = 0; j < 4u; j++) {
                uint32_t gid = PAGES[i].id[j], pe = pg->rows[r].id[j];
                if (gid == G_RTYPE) { ok &= pe == 0xFFu; continue; }                          /* K1 of REVERB: the host's TYPE chooser */
                ok &= pe < 7u && pg->pid[pe].legacy == (FW_LEG_GLOBAL | gid) && pg->pid[pe].id == pe + 1u && desc_same(&pg->panel[pe], global_desc(gid));
            }
        }
        snprintf(t, sizeof t, "%s: its two pages are the firmware's REVERB and REVERB 2 rows for TYPE %s: titles, family, K1 the host's TYPE (0xFF), and every other knob's song.g slot, label, "
                 "format, range, default, unit and names as global_desc shows it today", NM[k], NM[k]);
        ck(t, ok && found == 2u);
        /* against Session C's data (tests/fx_reverb_data.h, tests/mod_ui_test.c t_step8_reverb) */
        make_reverb(&sc, k);
        ok = sc.npages == pg->npages && sc.npanel == pg->npanel && sc.ext == pg->ext;
        for (i = 0; i < 2u; i++) ok &= !strcmp(sc.rows[i].title, pg->rows[i].title) && sc.rows[i].fam == pg->rows[i].fam && sc.rows[i].scope == pg->rows[i].scope && !memcmp(sc.rows[i].id, pg->rows[i].id, 4);
        for (i = 0; i < 7u; i++) ok &= desc_same(&sc.panel[i], &pg->panel[i]) && sc.pid[i].id == pg->pid[i].id && sc.pid[i].legacy == pg->pid[i].legacy;
        snprintf(t, sizeof t, "%s: the descriptor is MODULE_FORMAT_R3.md 16.2's as tests/fx_reverb_data.h has it: rows, panel, stable ids 1 .. 7, legacy slots, the condition", NM[k]);
        ck(t, ok);
    }
}
static void refusal_checks(void)
{
    static const char *const WHAT[] = {
        "another abi (FX_ABI_ID, the insert host's)", "another caps version", "a bad magic", "effect id 0 (ROOM is resident for good)", "effect id 4 (CLOUD is not this host's to take yet)",
        "the same effect id twice", "two instances", "a seed", "no rate of the bus", "a state of its own", "an option's blocks", "SHIMMER without its line block", "SPRING with a line block",
        "a snapshot that is not song.g's size", "no call", "another module id", "an empty bank", "a hot set" };
    uint32_t m, ok = 1;
    for (m = 0; m < 18u; m++) {
        fx_caps_t c[3];
        fx2_reg_t r[3];
        fx2_kcalls_t none = { 0 };
        uint32_t k, id = FX2_MODULE_ID, n = 3;
        int rc, want = FX2M_RANGE;
        char t[200];
        for (k = 0; k < 3u; k++) { c[k] = *oc_host_rev_caps(k); r[k].effect_id = (uint8_t)(k + 1u); r[k].caps = &c[k]; r[k].calls = &WRAP[k]; }
        switch (m) {
        case 0: c[1].abi = FX_ABI_ID; want = FX2M_VER; break;
        case 1: c[2].version = 2; want = FX2M_VER; break;
        case 2: c[0].magic ^= 1u; want = FX2M_VER; break;
        case 3: r[0].effect_id = 0; break;
        case 4: r[2].effect_id = 4; break;
        case 5: r[2].effect_id = 1; c[2].ram_work_min = c[2].ram_work_max = 0; break;
        case 6: c[0].max_inst = 2; break;
        case 7: c[1].flags |= FXK_SEED; break;
        case 8: c[2].rates = 2; break;
        case 9: c[0].ram_state = 4; break;
        case 10: c[1].ram_extra = 1; break;
        case 11: c[1].ram_work_min = c[1].ram_work_max = 0; break;
        case 12: c[0].ram_work_min = c[0].ram_work_max = 1; break;
        case 13: c[2].param_bytes += 2; break;
        case 14: r[1].calls = &none; break;
        case 15: id = 1; break;
        case 16: n = 0; break;
        default: c[0].nhot = 1; break;
        }
        fx2_modk_clear();
        rc = fx2_modk_set(id, 0x00010000u, n, r);
        snprintf(t, sizeof t, "fx2_modk_set refuses %s (%s) and registers nothing: every mode stays the resident copy's", WHAT[m], want == FX2M_VER ? "FX2M_VER" : "FX2M_RANGE");
        ok = rc == want && !fx2_modk_ok();
        for (k = 0; k < FX2_MODES; k++) ok &= !fx2_modk[k].process;
        ck(t, ok);
    }
    ck("fx2_modk_set takes the module's bank as it is: the three modes are the module's calls, ROOM and CLOUD have none", module_set() == FX2M_OK && fx2_modk_ok() && !fx2_modk[0].process &&
       fx2_modk[1].process == w_spring && fx2_modk[2].process == w_shimmer && fx2_modk[3].process == w_reso && !fx2_modk[4].process && fx2_mod_version == 0x00010000u);
    {   fx2_reg_t r[1] = { { 2, oc_host_rev_caps(1), &WRAP[1] } };                             /* a bank of one: the others stay resident */
        fx2_modk_clear();
        ck("a bank that serves SHIMMER alone registers: SPRING and RESO stay the resident copy's", fx2_modk_set(FX2_MODULE_ID, 0x00010000u, 1, r) == FX2M_OK && !fx2_modk[1].process && fx2_modk[2].process == w_shimmer && !fx2_modk[3].process);
    }
    fx2_modk_clear();
}
/* the crash mark and the Effects table's `uses` */
static void host_checks(void)
{
    uint32_t n0;
    uint64_t c0;
    rvp_tap = 0;                                         /* (these blocks are not a scenario's: nothing to compare them with) */
    fresh();
    fx2_modk_clear();
    (void)module_set();
    fx2_modk_note = note_hook; note_n = 0;
    G(G_RTYPE, 0);
    run(300);
    ck("ROOM with the module registered: no call into the module, no crash mark, `uses` 0", !mcalls[1] && !mcalls[2] && !mcalls[3] && note_n == 0 && fx2_modk_uses() == 0 && jrev);
    G(G_RTYPE, 4); run(200);
    ck("CLOUD (resident): the same", note_n == 0 && fx2_modk_uses() == 0 && !mcalls[1] && !mcalls[2] && !mcalls[3]);
    memset(mcalls, 0, sizeof mcalls);
    G(G_RTYPE, 1); run(1);
    n0 = note_n; c0 = mcalls[1];
    ck("the block in which TYPE turns to SPRING: the crash mark is taken (once) on CPU0 before the job runs; that block fades the old mode out, the module's SPRING has not run yet", n0 == 1u && c0 == 0);
    run(200);
    ck("SPRING running: the module's call runs every block, the mark is not taken again, `uses` 1", mcalls[1] == 200u && note_n == 1u && fx2_modk_uses() == 1u);
    fx2_marked = 0;                                      /* (oc_mod_boot rebuilt the registry: the mark was zeroed) */
    run(2);
    ck("after a rebuild of the registry (the mark zeroed) it is taken again at the next bus start", note_n == 2u);
    fx2_modk_clear();
    c0 = mcalls[1];
    run(300);
    ck("the module removed while SPRING sounds: not one call goes to it afterwards, `uses` 0, the reverb goes on (the resident copy on the same state)", mcalls[1] == c0 && fx2_modk_uses() == 0 && jrev && fx.rtype == 1u);
    fx2_modk_note = 0;
}
int main(void)
{
    stopfirst = getenv("RVM_STOPFIRST") != 0;
    keep = malloc(sizeof *keep * RVM_BLOCKS);
    if (!keep) return 2;
    printf("fx reverb module: MELODEE_FX_DEFER %d, OC_REV_RESIDENT %d\n", (int)MELODEE_FX_DEFER, (int)OC_REV_RESIDENT);
#if OC_REV_RESIDENT
    /* A: resident */
    fx2_modk_clear();
    rvp_tap = tap_keep; keep_n = 0;
    rvp_scenarios();
    memcpy(resA, rvp_res, sizeof resA); nA = rvp_nres;
    ck("A resident (no module registered): the printout is tests/fx_reverb_golden.txt's, pinned from the resident code before the cut", golden_ok(0xFFFu) && keep_n < RVM_BLOCKS);
    ck("... and not one call reached the module", !mcalls[1] && !mcalls[2] && !mcalls[3]);
    /* B: the module */
    ck("the module's bank registers with the send host (fx2_modk_set)", module_set() == FX2M_OK);
    run_cmp("B the module registered", 0);
    {   char t[200];
        snprintf(t, sizeof t, "... and the module ran: SPRING %llu, SHIMMER %llu, RESO %llu blocks through its calls", (unsigned long long)mcalls[1], (unsigned long long)mcalls[2], (unsigned long long)mcalls[3]);
        ck(t, mcalls[1] > 20000u && mcalls[2] > 20000u && mcalls[3] > 20000u);
    }
    {   uint64_t b1 = mcalls[1], b2 = mcalls[2], b3 = mcalls[3];
        char t[200];
        /* C: appearing and going away in the middle of everything */
        run_cmp("C the module appearing and going away every 61 blocks, in the middle of tails, fades and mode changes", 61);
        snprintf(t, sizeof t, "... with both copies playing parts of it (the module's calls: SPRING %llu, SHIMMER %llu, RESO %llu blocks, fewer than in B)", (unsigned long long)mcalls[1], (unsigned long long)mcalls[2], (unsigned long long)mcalls[3]);
        ck(t, mcalls[1] > 5000u && mcalls[1] < b1 && mcalls[2] > 5000u && mcalls[2] < b2 && mcalls[3] > 5000u && mcalls[3] < b3);
    }
    /* D: gone */
    fx2_modk_clear();
    run_cmp("D the module removed", 0);
    ck("... and not one call reached it", !mcalls[1] && !mcalls[2] && !mcalls[3]);
    host_checks();
#else
    /* the Nightly after: no resident copy of the three modes in the image */
    {   uint32_t i;
        fx2_modk_clear();
        rvp_tap = 0;
        rvp_scenarios();
        ck("no resident copy, no module: the modes SPRING, SHIMMER and RESO are not run: the three KNOBS renders are the same samples (the dry mix: the notes are the same, the reverb adds nothing)",
           rvp_nres == 12u && rvp_res[1].au == rvp_res[2].au && rvp_res[2].au == rvp_res[3].au && rvp_res[1].au != rvp_res[0].au && rvp_res[1].au != rvp_res[4].au);
        ck("... and ROOM and CLOUD (resident for good / still resident) are the pinned renders: KNOBS0, KNOBS4, MIX0, MIX4", golden_ok(1u | 1u << 4 | 1u << 7 | 1u << 11));
        (void)i;
        ck("the module's bank registers", module_set() == FX2M_OK);
        rvp_scenarios();
        ck("no resident copy, the module registered: the printout is tests/fx_reverb_golden.txt's (the resident render pinned before the cut): the module alone plays the three modes as the image did", golden_ok(0xFFFu));
        ck("... through its calls", mcalls[1] > 20000u && mcalls[2] > 20000u && mcalls[3] > 20000u);
        fx2_modk_clear();
    }
#endif
    rvp_tap = 0;
    refusal_checks();
    descriptor_checks();
    if (bad) { printf("fx reverb module: %d FAILED\n", bad); return 1; }
    printf("fx reverb module: all passed\n");
    return 0;
}
