/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Release 3 step 8b (docs/orangecat/FX_MODULES_PLAN.md 17): the FX layer's buffer effects GLITCH, TAPE STOP, SHUFFLE and FREEZE as ONE effect module (kind 3; tools/module/fxlayer_mod.c) BESIDE
 * the resident copy. THE GATE.
 *
 * Built with the module's translation unit (tests/module_host_fxlayer_tu.c: the module source with OC_MODULE_HOST, its own object, its own copy of the kernel text, reaching the firmware only
 * through the context of each call). It runs the probe's scenarios (tests/fx_layer_probe.c: every card knob, SCRATCH, the tempo, the keys against each other, FX LATCH, perf_kill, the
 * transport, the 32 KB short and lost, a full mix, random keys) through the real mixer, one scenario at a time:
 *   A  resident: no module registered. Every output block is kept; the printout equals tests/fx_layer_golden.txt, pinned from the resident code before the cut
 *   B  the module registered: every output block compared with A's, sample for sample, and the behaviour hash; the module's calls are behind counters (they ran, each effect)
 *   C  the module appearing and going away every 61 blocks in the middle of the render (the host owns the state and the 32 KB: the swap must not change one sample)
 *   D  the module gone: identical again, and not one call reaches it
 *   E  the two copies of the kernel text side by side, without the mixer, on the same state, buffer and input: every value of every card knob (what a render of sane length cannot reach:
 *      a threshold one step off in the slices' hash)
 * then the host around it: the calls a block (the granularity), a key's way to the first changed sample (the same sample, resident and module), each refusal of fx3_modk_set (nothing
 * registered), the crash mark (taken once, before the module's first call; not for a REPEAT or a sweep), the Effects table's `uses`, a removal while an effect plays, and the descriptor: the
 * caps words, and each effect's page against what the cards show today (ui_layer.c FXK_LABEL, FXK_MIN, FXK_MAX, the enumerations, perf_gp's initial values) and against Session C's data
 * (tests/mod_ui_test.c t_step8_layer), through the loader's checker as SC_FX.
 * With -DOC_PERF_RESIDENT=0 (the image of the Nightly AFTER this one: no resident copy of the four effects) the same file checks what happens then: without the module the four keys do
 * nothing (no sound changed, no memory taken, no card), the REPEATs, the sweeps and the mutes work; with it the render is the pinned resident printout; a module removed under a playing
 * effect cuts it and gives the buffer back.
 * Arguments: none. FLM_STOPFIRST=1: the descriptor first, then A and B only, stopping at the first differing block (the mutation check); FLM_FIRST=<scenario>: that scenario first. Built
 * and run by tests/fx_layer_module.sh. */
#define FLP_NO_MAIN 1
#include "fx_layer_probe.c"
#include "fw_pages_wire.h"
#include "fw_fx.h"
#include "fxl_module_host.h"

static void ck(const char *what, int ok)
{
    printf("fx layer module: %-170s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* ---- the module's calls behind counters ---- */
static uint64_t mstart[4], mrun[4], msamp[4];
static uint32_t blk_runs, blk_runs_max, blk_with, blk_starts, blk_starts_max;   /* the calls in the block being rendered; the most in one block; blocks with a call */
static uint64_t runs_total;
#define WRAPK(k_) \
static void w_start##k_(const fx3_ctx_t *c) { mstart[k_]++; blk_starts++; oc_host_fxl_calls(k_)->start(c); } \
static uint32_t w_run##k_(const fx3_ctx_t *c, int32_t *l, int32_t *r, uint32_t n) { uint32_t d = oc_host_fxl_calls(k_)->run(c, l, r, n); mrun[k_]++; msamp[k_] += d; blk_runs++; return d; }
WRAPK(0) WRAPK(1) WRAPK(2) WRAPK(3)
static const fx3_kcalls_t WRAP[4] = { {w_start0, w_run0}, {w_start1, w_run1}, {w_start2, w_run2}, {w_start3, w_run3} };
static int module_set(void)
{
    fx3_reg_t r[4];
    oc_host_fxl_fw = &FW_API;
    for (uint32_t k = 0; k < 4u; k++) { r[k].effect_id = oc_host_fxl_entry(k)->effect_id; r[k].caps = oc_host_fxl_caps(k); r[k].calls = &WRAP[k]; }
    return fx3_modk_set(oc_host_fxl_desc()->module_id, oc_host_fxl_desc()->version, 4, r);
}
static int no_calls(void) { return !mrun[0] && !mrun[1] && !mrun[2] && !mrun[3] && !mstart[0] && !mstart[1] && !mstart[2] && !mstart[3]; }
/* ---- every block of run A kept; the later runs compared with it at the tap ---- */
#define FLM_BLOCKS 260000u                             /* (the longest scenario: TEMPO, 238,724 blocks) */
static int32_t (*keep)[CTL * 2];
static uint32_t keep_n, cmp_at, cmp_bad, toggle, tog_n;
static int stopfirst;
static char first_bad[120];
static void blk_count(void)
{
    if (blk_runs) { blk_with++; runs_total += blk_runs; }
    if (blk_runs > blk_runs_max) blk_runs_max = blk_runs;
    if (blk_starts > blk_starts_max) blk_starts_max = blk_starts;
    blk_runs = blk_starts = 0;
}
static void tap_keep(const char *sc, uint32_t b, const int32_t *pcm, size_t bytes)
{
    (void)sc; (void)b;
    if (keep_n < FLM_BLOCKS) memcpy(keep[keep_n], pcm, bytes);
    keep_n++;
}
static void tap_cmp(const char *sc, uint32_t b, const int32_t *pcm, size_t bytes)
{
    blk_count();
    if (cmp_at >= keep_n || memcmp(keep[cmp_at], pcm, bytes)) {
        if (!cmp_bad) {
            snprintf(first_bad, sizeof first_bad, "%s block %u differs from the resident render", sc, (unsigned)b);
            if (stopfirst) { printf("fx layer module: FAIL: %s\nfx layer module: VERDICT render: %s\n", first_bad, first_bad); exit(1); }
        }
        cmp_bad++;
    }
    cmp_at++;
    if (toggle && !(++tog_n % toggle)) { if (fx3_modk_ok()) fx3_modk_clear(); else (void)module_set(); }   /* run C: between two blocks, as the unit's loader does it (the audio interrupt masked) */
}
#define NSC 12u
static const char *const SCN[NSC] = { "GLITCH", "TAPE", "SHUFFLE", "FREEZE", "TEMPO", "SWITCH", "START", "MEM", "MIX", "RANDOM", "STEREO", "SCRATCH" };
static flp_res_t resA[NSC], resX[4][NSC];                /* [run][scenario]: 0 A, 1 B, 2 C, 3 D */
static uint32_t difX[4], blkX[4];
static uint64_t runB[4], startB[4], runC[4];
/* the printout of run A against the pinned one (the section of this build's MELODEE_FX_DEFER) */
static int golden_ok(const flp_res_t *res)
{
    FILE *f = fopen(getenv("FLP_GOLDEN") ? getenv("FLP_GOLDEN") : "tests/fx_layer_golden.txt", "r");
    char line[256], mine[256], head[64];
    uint32_t i = 0, in = 0, ok = 1;
    if (!f) return 0;
    snprintf(head, sizeof head, "fx_layer probe: MELODEE_FX_DEFER %d", (int)MELODEE_FX_DEFER);
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "fx_layer probe:", 15)) { in = !strcmp(line, head); continue; }
        if (!in || !strncmp(line, "fx_layer:", 9)) continue;
        if (i >= NSC) { ok = 0; break; }
        snprintf(mine, sizeof mine, "%s blocks %u au %016llx st %016llx", res[i].name, res[i].blocks, (unsigned long long)res[i].au, (unsigned long long)res[i].st);
        if (strcmp(mine, line)) { ok = 0; printf("  golden: %s\n  this:   %s\n", line, mine); }
        i++;
    }
    fclose(f);
    return ok && i == NSC;
}
/* one scenario, one run: 0 A (kept), 1 B (the module), 2 C (the module toggled), 3 D (removed) */
static void one(uint32_t s, uint32_t run_)
{
    snprintf(flp_only, sizeof flp_only, "%s", SCN[s]);
    fx3_modk_clear();
    if (run_ == 1u || run_ == 2u) (void)module_set();
    toggle = run_ == 2u ? 61u : 0u;
    cmp_at = 0;
    if (run_ == 0u) { keep_n = 0; flp_tap = tap_keep; } else flp_tap = tap_cmp;
    {   uint32_t bad0 = cmp_bad;
        flp_scenarios();
        if (run_) { difX[run_] += cmp_bad - bad0; blkX[run_] += cmp_at; if (cmp_at != keep_n) difX[run_]++; }
    }
    flp_tap = 0;
    if (run_ == 0u) resA[s] = flp_res[0]; else resX[run_][s] = flp_res[0];
    fx3_modk_clear();
    flp_only[0] = 0;
}
static int same_res(uint32_t run_)
{
    uint32_t s, ok = 1;
    for (s = 0; s < NSC; s++) ok &= !strcmp(resX[run_][s].name, resA[s].name) && resX[run_][s].blocks == resA[s].blocks && resX[run_][s].au == resA[s].au && resX[run_][s].st == resA[s].st;
    return (int)ok;
}
static uint32_t note_n;
static void note_hook(uint32_t id) { if (id == FX3_MODULE_ID) note_n++; }
#define WBASE 0x10200000u
static uint8_t wimg[8192];
static int wrd(void *ctx, uint32_t addr, void *buf, uint32_t n) { (void)ctx; if (addr < WBASE || addr - WBASE + n > sizeof wimg) return 1; memcpy(buf, wimg + (addr - WBASE), n); return 0; }
static int8_t gp_init[4][4];                            /* perf_gp as the image starts it (read before anything writes it) */
/* Session C's descriptors of the four cards (tests/mod_ui_test.c t_step8_layer's table E, copied: that table is local to its test) */
typedef struct { const char *label; uint8_t fmt; int16_t mn, mx, def; const char *unit; const char *const *names; } sc_t;
static const char *const SC_GSIZE[] = {"LONG", "MID", "SHORT", "TINY"}, *const SC_TTIME[] = {"1/64", "1/32", "1/16", "1/8", "1/4"};
static const char *const SC_SSIZE[] = {"1/4", "1/8", "1/16"}, *const SC_FSIZE[] = {"1/128", "1/64", "1/32", "1/16", "1/8"};
static const sc_t SC_E[4][4] = {
    {{"DENS", F_INT, 0, 100, 50, "%", 0}, {"PITCH", F_INT, -12, 12, 0, "st", 0}, {"SIZE", F_ENUM, 0, 3, 0, 0, SC_GSIZE}, {"VARI", F_INT, 1, 16, 1, 0, 0}},
    {{"SCRATCH", F_INT, 0, 0, 0, 0, 0}, {"TIME", F_ENUM, 0, 4, 4, 0, SC_TTIME}, {"CURVE", F_INT, -100, 100, 0, 0, 0}, {0}},
    {{"AMOUNT", F_INT, 0, 100, 60, "%", 0}, {"SIZE", F_ENUM, 0, 2, 2, 0, SC_SSIZE}, {"SPEED", F_INT, 0, 100, 20, "%", 0}, {"VARI", F_INT, 1, 16, 1, 0, 0}},
    {{"DENS", F_INT, 0, 100, 50, "%", 0}, {"PITCH", F_INT, -12, 12, 0, "st", 0}, {"SPRAY", F_INT, 0, 100, 30, "%", 0}, {"SIZE", F_ENUM, 0, 4, 2, 0, SC_FSIZE}}};
static const char *const SC_NM[4] = {"GLITCH", "TAPE STOP", "SHUFFLE", "FREEZE"};
static const uint8_t SC_NK[4] = {4, 3, 4, 4};

static void descriptor_checks(void)
{
    const fx_desc_wire_t *d = oc_host_fxl_desc();
    uint32_t x, k, ok;
    ck("the bank: module id 3 (FX3_MODULE_ID, docs/orangecat/MODULE_IDS.md), version 1.0.0, four effects, no scene; the header's word is kind 3 with no engine number, state size 0",
       d->n == 4u && d->module_id == 3u && d->module_id == FX3_MODULE_ID && d->version == 0x00010000u && d->scene == 0 && OC_FXLAYER_MODULE_HDR.engine == (FW_MOD_KIND_MASTER << 8) &&
       OC_FXLAYER_MODULE_HDR.state_size == 0 && OC_FXLAYER_MODULE_HDR.iface == FW_API_IFACE && fw_module_check(&OC_FXLAYER_MODULE_HDR) == FW_MOD_OK);
    ck("the identifiers: FX3_ABI_ID is its own (not FX_ABI_ID, not FX2_ABI_ID, not FW_API_ID)", FX3_ABI_ID != FX_ABI_ID && FX3_ABI_ID != FX2_ABI_ID && FX3_ABI_ID != FW_API_ID);
    for (x = 0; x < 4u; x++) {
        const fx_caps_t *c = oc_host_fxl_caps(x);
        const fx_entry_wire_t *e = oc_host_fxl_entry(x);
        const fw_pages_t *pg = oc_host_fxl_pages(x);
        static const char *const *const NAMES[4][4] = { {0, 0, FXK_GSIZE, 0}, {0, FXK_TTIME, 0, 0}, {0, FXK_SSIZE, 0, 0}, {0, 0, 0, FXK_FSIZE} };
        uint32_t desc = 0;
        char t[400];
        snprintf(t, sizeof t, "%s: the entry is effect %u (1 + its place in the layer), kind 3, no flags, no parameter table; its caps block: GFXK, 56 bytes, version 1, abi FX3_ABI_ID, one instance, no seed, "
                 "the master's rate, no state / option / hot set of its own, the 32 KB as 8 working blocks, the snapshot the card's 4 bytes, the AP_FX class, no legacy key, no rows",
                 SC_NM[x], (unsigned)x + 1u);
        ck(t, e->effect_id == x + 1u && e->kind == FW_MOD_KIND_MASTER && !e->flags && !e->nparams && !e->params && c->magic == FX_CAPS_MAGIC && c->size == FX_CAPS_WIRE && c->version == FX_CAPS_VERSION &&
           c->abi == FX3_ABI_ID && c->max_inst == 1 && c->flags == 0 && c->rates == 1 && !c->ram_state && !c->ram_extra && !c->ram_extra_bytes && !c->hot && !c->nhot &&
           !c->rows && !c->nrows && c->ram_work_min == 8 && c->ram_work_max == 8 && c->param_bytes == 4u && c->prof == AP_FX && c->legacy_module == 0 && !c->latency && !c->tail_base && !c->cost && !c->gov_us);
        /* the pages through the loader's checker, out of the device's wire bytes */
        ok = fwp_wire(pg, wimg, sizeof wimg, WBASE, &desc) != 0;
        snprintf(t, sizeof t, "%s: its page passes the loader's checker as an effect's (SC_FX: volatile ids%s) and is refused as an engine's", SC_NM[x], x == 1u ? ", SCRATCH relative with its put hook" : ", no hooks");
        ck(t, ok && fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_FX, P_COUNT, G_COUNT, 0) == FWP_OK &&
           fw_pages_check_ex(wrd, 0, desc, WBASE + 0x100u, WBASE + sizeof wimg, SC_MOD, P_COUNT, G_COUNT, 0) != FWP_OK);
        /* against what the cards show today: ui_layer.c's tables and perf_gp's initial values, field for field */
        ok = pg->npages == 1u && pg->npanel == SC_NK[x] && !pg->fx_shown && (pg->fx_put != 0) == (x == 1u) && !pg->ext && !pg->ui && pg->rows[0].fam == FAM_FX && pg->rows[0].scope == SC_FX && pg->rows[0].graph == GR_NONE;
        for (k = 0; k < 4u; k++) {
            const param_desc_t *pd;
            const char *unit = k == 1u && (x == GX_GLT || x == GX_FRZ) ? "st" : (k == 0u || k == 2u) && x != GX_TAPE && !NAMES[x][k] ? "%" : "";   /* (layer_fx_cards' own expression for the unit) */
            if (!FXK_LABEL[x][k][0]) { ok &= pg->rows[0].id[k] == 0xFFu && k >= pg->npanel; continue; }      /* no knob: the host's place (L2), nothing drawn */
            ok &= pg->rows[0].id[k] == k && k < pg->npanel;
            if (k >= pg->npanel) continue;
            pd = &pg->panel[k];
            ok &= !strcmp(pd->label, FXK_LABEL[x][k]) && pd->min == FXK_MIN[x][k] && pd->max == FXK_MAX[x][k] && pd->def == gp_init[x][k] && pd->def >= pd->min && pd->def <= pd->max;
            ok &= (pd->unit ? !strcmp(pd->unit, unit) : !unit[0]);
            ok &= (pd->fmt == F_ENUM) == (NAMES[x][k] != 0);
            if (pd->fmt == F_ENUM && NAMES[x][k]) for (int v = pd->min; v <= pd->max; v++) ok &= !strcmp(pd->names[v], NAMES[x][k][v]);
            ok &= pg->pid[k].legacy == FW_PID_NONE && pg->pid[k].id == (x == GX_TAPE && k == 0u ? FW_PID_RELATIVE + 1u : FW_PID_VOLATILE + 1u + k);
        }
        snprintf(t, sizeof t, "%s: its page is the card ui_layer.c draws while the key is held: every knob's label (FXK_LABEL), range (FXK_MIN, FXK_MAX), default (perf_gp's initial value), unit and "
                 "enumeration (FXK_GSIZE / TTIME / SSIZE / FSIZE) in its place, an empty place 0xFF; the ids volatile 0x4001 ..%s, no legacy slot", SC_NM[x], x == 1u ? ", SCRATCH 0x2001" : "");
        ck(t, ok);
        /* against Session C's data (tests/mod_ui_test.c t_step8_layer) */
        ok = !strcmp(pg->rows[0].title, SC_NM[x]) && pg->npanel == SC_NK[x];
        for (k = 0; k < SC_NK[x]; k++) {
            const sc_t *s = &SC_E[x][k];
            const param_desc_t *pd = &pg->panel[k];
            ok &= !strcmp(pd->label, s->label) && pd->fmt == s->fmt && pd->min == s->mn && pd->max == s->mx && pd->def == s->def && (pd->unit == 0) == (s->unit == 0) && (!pd->unit || !strcmp(pd->unit, s->unit));
            if (s->fmt == F_ENUM) for (int v = s->mn; v <= s->mx; v++) ok &= !strcmp(pd->names[v], s->names[v]);
        }
        for (k = SC_NK[x]; k < 4u; k++) ok &= pg->rows[0].id[k] == 0xFFu;
        snprintf(t, sizeof t, "%s: the descriptor is MODULE_FORMAT_R3.md 16.3's as tests/mod_ui_test.c t_step8_layer has it: the title, the panel, the formats, the units, the names", SC_NM[x]);
        ck(t, ok);
    }
    {   const fw_pages_t *pg = oc_host_fxl_pages(1);
        uint32_t scr0 = perf_scr;
        pg->fx_put(0, 0, FW_PID_RELATIVE + 1u, 5);
        ck("SCRATCH's put hook is there (the checker requires it for a relative id) and does nothing: the detents are the host's count (perf_scr), which the hook cannot reach and does not move", perf_scr == scr0);
    }
}
static void refusal_checks(void)
{
    static const char *const WHAT[] = {
        "another abi (FX_ABI_ID, the insert host's)", "another abi (FX2_ABI_ID, the send host's)", "another caps version", "a bad magic", "effect id 0", "effect id 5",
        "the same effect id twice", "two instances", "a seed", "no rate of the master", "a state of its own", "an option's blocks", "a working memory that is not the 32 KB",
        "a snapshot that is not the card's four bytes", "no run call", "no start call", "another module id", "an empty bank", "a hot set", "a bank of five" };
    uint32_t m, ok = 1;
    for (m = 0; m < 20u; m++) {
        fx_caps_t c[5];
        fx3_reg_t r[5];
        fx3_kcalls_t part;
        uint32_t k, id = FX3_MODULE_ID, n = 4;
        int rc, want = FX3M_RANGE;
        char t[200];
        for (k = 0; k < 4u; k++) { c[k] = *oc_host_fxl_caps(k); r[k].effect_id = (uint8_t)(k + 1u); r[k].caps = &c[k]; r[k].calls = &WRAP[k]; }
        c[4] = c[0]; r[4] = r[0];
        switch (m) {
        case 0: c[1].abi = FX_ABI_ID; want = FX3M_VER; break;
        case 1: c[3].abi = FX2_ABI_ID; want = FX3M_VER; break;
        case 2: c[2].version = 2; want = FX3M_VER; break;
        case 3: c[0].magic ^= 1u; want = FX3M_VER; break;
        case 4: r[0].effect_id = 0; break;
        case 5: r[3].effect_id = 5; break;
        case 6: r[2].effect_id = 1; break;
        case 7: c[0].max_inst = 2; break;
        case 8: c[1].flags |= FXK_SEED; break;
        case 9: c[2].rates = 2; break;
        case 10: c[0].ram_state = 4; break;
        case 11: c[1].ram_extra = 1; break;
        case 12: c[3].ram_work_min = 7; break;
        case 13: c[2].param_bytes = 8; break;
        case 14: part = WRAP[1]; part.run = 0; r[1].calls = &part; break;
        case 15: part = WRAP[2]; part.start = 0; r[2].calls = &part; break;
        case 16: id = 2; break;
        case 17: n = 0; break;
        case 18: c[0].nhot = 1; break;
        default: n = 5; break;
        }
        fx3_modk_clear();
        rc = fx3_modk_set(id, 0x00010000u, n, r);
        snprintf(t, sizeof t, "fx3_modk_set refuses %s (%s) and registers nothing: every effect stays the resident copy's", WHAT[m], want == FX3M_VER ? "FX3M_VER" : "FX3M_RANGE");
        ok = rc == want && !fx3_modk_ok();
        for (k = 0; k < FX3_FX; k++) ok &= !fx3_modk[k].run && !fx3_modk[k].start;
        ck(t, ok);
    }
    ck("fx3_modk_set takes the module's bank as it is: the four effects are the module's calls", module_set() == FX3M_OK && fx3_modk_ok() && fx3_modk[0].run == w_run0 && fx3_modk[1].run == w_run1 &&
       fx3_modk[2].run == w_run2 && fx3_modk[3].run == w_run3 && fx3_modk[0].start == w_start0 && fx3_modk[3].start == w_start3 && fx3_mod_n == 4u);
    {   fx3_reg_t r[1] = { { 3, oc_host_fxl_caps(2), &WRAP[2] } };                             /* a bank of one: the others stay resident */
        fx3_modk_clear();
        ck("a bank that serves SHUFFLE alone registers: GLITCH, TAPE STOP and FREEZE stay the resident copy's", fx3_modk_set(FX3_MODULE_ID, 0x00010000u, 1, r) == FX3M_OK && !fx3_modk[0].run && !fx3_modk[1].run &&
           fx3_modk[2].run == w_run2 && !fx3_modk[3].run);
    }
    fx3_modk_clear();
}
#if OC_PERF_RESIDENT
/* ---- E: the two copies of the kernel text side by side, without the mixer: the resident copy (PERF_RESIDENT) and the module's calls on the same state, 32 KB, knobs and input, every value
 * of every card knob. The scenarios hear what a player does; this hears what they cannot reach in a render of sane length: a threshold one step off in the slices' hash (GLITCH's DENS,
 * SHUFFLE's AMOUNT and SPEED are compared with a hash a 1/16: one value in a hundred a slice), every DENS with every VARI, every CURVE with every TIME. Each run: the host's part of a start
 * (the state cleared, P and gs set), the kernel's start, then blocks of 32 samples of stereo noise cut in two runs at a place that moves (the second run not the block's head), SCRATCH's
 * count moving, the last blocks fading out; after every run the outputs, the return and the whole state must be equal, and the 32 KB at the end ---- */
static int16_t dkA[FX3_FRAMES][2], dkB[FX3_FRAMES][2];
static uint64_t dk_runs, dk_samples, dk_bad;
static char dk_first[160];
static uint32_t dk_seed;
static int32_t dk_noise(void) { dk_seed = dk_seed * 1664525u + 1013904223u; return (int32_t)(dk_seed >> 8) % 24001 - 12000; }
static void direct_one(uint32_t x, int g0, int g1, int g2, int g3, uint32_t beat, uint32_t P, uint32_t gs, uint32_t nblk, uint32_t tag)
{
    fx3_st_t sa, sb;
    fx3_ctx_t ca, cb;
    volatile int8_t ga[4], gb[4];
    const fx3_kcalls_t *res = PERF_RESIDENT_OF(x), *mod = oc_host_fxl_calls(x);
    uint32_t b, i, scr = tag * 7u;
    int32_t la[CTL], ra[CTL], lb[CTL], rb[CTL];
    ga[0] = gb[0] = (int8_t)g0; ga[1] = gb[1] = (int8_t)g1; ga[2] = gb[2] = (int8_t)g2; ga[3] = gb[3] = (int8_t)g3;
    memset(&sa, 0, sizeof sa); memset(&sb, 0, sizeof sb);
    memset(dkA[FX3_FRAMES - 2u], 0, 2u * sizeof dkA[0]); memset(dkB[FX3_FRAMES - 2u], 0, 2u * sizeof dkB[0]);   /* (the host's perf_buf_start: quiet before the start) */
    sa.P = sb.P = P; sa.gs = sb.gs = gs;
    ca.st = &sa; ca.buf = (fx3_buf_t *)dkA; ca.gp = ga; cb.st = &sb; cb.buf = (fx3_buf_t *)dkB; cb.gp = gb;
    ca.beat = cb.beat = beat; ca.scr = cb.scr = scr; ca.go = cb.go = 1; ca.head = cb.head = 0; ca.semi = cb.semi = semi_q16;
    res->start(&ca); mod->start(&cb);
    dk_seed = 0x9E3779B9u ^ (tag * 2654435761u) ^ x;
    for (b = 0; b < nblk; b++) {
        uint32_t cut = 1u + (b * 13u + tag) % (CTL - 1u), part, done = 0, ended = 0;
        for (i = 0; i < CTL; i++) { la[i] = lb[i] = dk_noise(); ra[i] = rb[i] = dk_noise(); }
        if (b % 5u == 3u) scr += (uint32_t)((int32_t)((b * 7u + tag) % 41u) - 20);          /* SCRATCH: the knob turns (TAPE STOP alone reads it) */
        if (b + 8u >= nblk) ca.go = cb.go = 0;                                              /* let go: the fade out, and the early return when the share is 0 */
        ca.scr = cb.scr = scr;
        for (part = 0; part < 2u && !ended; part++) {
            uint32_t n = part ? CTL - cut : cut, da, db;
            ca.head = cb.head = part == 0u;
            da = res->run(&ca, la + done, ra + done, n);
            db = mod->run(&cb, lb + done, rb + done, n);
            dk_runs++; dk_samples += da;
            if (da != db || memcmp(la, lb, sizeof la) || memcmp(ra, rb, sizeof ra) || memcmp(&sa, &sb, sizeof sa)) {
                if (!dk_bad) snprintf(dk_first, sizeof dk_first, "%s knobs %d %d %d %d beat %u P %u gs %u: block %u differs from the resident copy's", SC_NM[x], g0, g1, g2, g3, (unsigned)beat, (unsigned)P, (unsigned)gs, (unsigned)b);
                dk_bad++;
                if (stopfirst) { printf("fx layer module: FAIL: the kernels side by side: %s\nfx layer module: VERDICT kernel: %s\n", dk_first, dk_first); exit(1); }
                return;
            }
            done += da;
            if (da < n) ended = 1;                                                          /* faded out: the host would give the buffer back here */
        }
        if (ended) break;
    }
    if (memcmp(dkA, dkB, sizeof dkA)) {
        if (!dk_bad) snprintf(dk_first, sizeof dk_first, "%s knobs %d %d %d %d beat %u P %u gs %u: the 32 KB differs from the resident copy's", SC_NM[x], g0, g1, g2, g3, (unsigned)beat, (unsigned)P, (unsigned)gs);
        dk_bad++;
        if (stopfirst) { printf("fx layer module: FAIL: the kernels side by side: %s\nfx layer module: VERDICT kernel: %s\n", dk_first, dk_first); exit(1); }
    }
}
static void direct_checks(void)
{
    int a, v, c;
    uint32_t tag = 0;
    char t[400];
    memset(dkA, 0, sizeof dkA); memset(dkB, 0, sizeof dkB);
    /* GLITCH: every DENS with every VARI (SIZE and PITCH moving with them), 12 of the 1/16s from every place of the two bars; then every PITCH with every SIZE */
    for (a = 0; a <= 100; a++)
        for (v = 1; v <= 16; v++, tag++)
            direct_one(GX_GLT, a, (a * 7 + v) % 25 - 12, (a + v) % 4, v, 4400u + 16u * (uint32_t)v, 1100u + 4u * (uint32_t)v, tag % 32u, 12u * (1100u + 4u * (uint32_t)v) / CTL, tag);
    for (a = -12; a <= 12; a++)
        for (v = 0; v <= 3; v++, tag++)
            direct_one(GX_GLT, 100, a, v, 1 + (a + 12) % 16, 9000u, 2250u, tag % 32u, 20u * 2250u / CTL, tag);
    /* SHUFFLE: every AMOUNT with every VARI (SIZE and SPEED moving), 20 of the 1/16s; then every SPEED at AMOUNT 100 with each SIZE */
    for (a = 0; a <= 100; a++)
        for (v = 1; v <= 16; v++, tag++)
            direct_one(GX_SHUF, a, (a + v) % 3, (a * 13 + v * 7) % 101, v, 4000u, 1000u + 8u * (uint32_t)v, tag % 32u, 20u * (1000u + 8u * (uint32_t)v) / CTL, tag);
    for (a = 0; a <= 100; a++)
        for (v = 0; v <= 2; v++, tag++)
            direct_one(GX_SHUF, 100, v, a, 1 + a % 16, 4800u, 1200u, tag % 32u, 24u * 1200u / CTL, tag);
    /* FREEZE: every DENS with every SIZE (PITCH and SPRAY moving); every SPRAY with every PITCH; three tempos (the eighth it keeps: its floor, a middle, its ceiling) */
    for (a = 0; a <= 100; a++)
        for (v = 0; v <= 4; v++, tag++)
            direct_one(GX_FRZ, a, a % 25 - 12, (a * 3 + v * 17) % 101, v, v == 4 ? 40000u : 6000u + 900u * (uint32_t)v, 1500u, 0, (v == 4 ? 15360u + 9000u : 4096u + 7000u) / CTL, tag);
    for (a = 0; a <= 100; a++)
        for (v = -12; v <= 12; v++, tag++)
            direct_one(GX_FRZ, 30 + (a + v + 12) % 71, v, a, (a + v + 12) % 5, 11000u, 2750u, 0, (5500u + 5000u) / CTL, tag);
    /* TAPE STOP: every CURVE with every TIME at three tempos, the fall and then the hand on the stopped record */
    for (c = -100; c <= 100; c++)
        for (v = 0; v <= 4; v++, tag++) {
            uint32_t beat = (uint32_t[]){3000u, 9000u, 30000u}[(uint32_t)(c + 100 + v) % 3u], tt = beat >> (4u - (uint32_t)v), m = 15000u * 600u / (uint32_t)(300 - c), fall = tt < m ? (tt > 64u ? tt : 64u) : m;
            direct_one(GX_TAPE, 0, v, c, 0, beat, beat / 4u, 0, (fall + 2600u) / CTL, tag);
        }
    snprintf(t, sizeof t, "E the two copies of the kernel text side by side (no mixer): every DENS x VARI and PITCH x SIZE of GLITCH, every AMOUNT x VARI and SPEED x SIZE of SHUFFLE, every DENS x SIZE and SPRAY x PITCH of "
             "FREEZE, every CURVE x TIME of TAPE STOP with SCRATCH turning: %llu runs, %llu samples: the outputs, the returns, the state and the 32 KB are equal (%llu differ%s%s)",
             (unsigned long long)dk_runs, (unsigned long long)dk_samples, (unsigned long long)dk_bad, dk_bad ? "; the first: " : "", dk_first);
    ck(t, !dk_bad && dk_runs > 1000000u);
}
#endif
/* ---- a key's way to the sound: the first output sample that differs from the same render with no key, resident and module ---- */
#define LAT_BLOCKS 900u
static int32_t (*lat_ref)[CTL * 2], (*lat_out)[CTL * 2];
static uint8_t lat_mode[LAT_BLOCKS];
static void lat_render(int32_t (*out)[CTL * 2], int e, int playing, int with_module)
{
    uint32_t b;
    fresh();
    fx3_modk_clear();
    if (with_module) (void)module_set();
    G(G_BPM, 200);
    perf_gp[GX_GLT][0] = 100; perf_gp[GX_SHUF][0] = 100;   /* (DENS and AMOUNT full: every 1/16 is cut, so the first one after the start is heard) */
    if (playing) play();
    gb = 0; notes_off = 0;
    for (b = 0; b < LAT_BLOCKS; b++) {
        if (b == 207u && e >= 0) perf_press((uint32_t)e, 1);      /* the key scan's call, between two blocks */
        notes();
        blk_runs = blk_starts = 0;
        mix_block(out[b], CTL);
        lat_mode[b] = pf.mode;
        gb++;
    }
    if (e >= 0) perf_press((uint32_t)e, 0);
    stop();
    fx3_modk_clear();
}
static uint32_t lat_first(void)
{
    uint32_t i;
    for (i = 0; i < LAT_BLOCKS * CTL * 2u; i++) if (lat_ref[0][i] != lat_out[0][i]) return i / 2u;
    return 0xFFFFFFFFu;
}
static void latency_checks(void)
{
    uint32_t x, playing;
    lat_ref = malloc(sizeof *lat_ref * LAT_BLOCKS); lat_out = malloc(sizeof *lat_out * LAT_BLOCKS);
    if (!lat_ref || !lat_out) { ck("memory for the latency check", 0); return; }
    for (playing = 0; playing < 2u; playing++) {
        for (x = 0; x < 4u; x++) {
            uint32_t fr, fm, sr, sm, b;
            uint8_t mr[LAT_BLOCKS];
            char t[300];
            lat_render(lat_ref, -1, (int)playing, 0);
            lat_render(lat_out, FLP_E[x], (int)playing, 0);
            fr = lat_first();
            memcpy(mr, lat_mode, sizeof mr);
            for (b = 0; b < LAT_BLOCKS && mr[b] == BM_NONE; b++) {}
            sr = b;
            lat_render(lat_out, FLP_E[x], (int)playing, 1);
            fm = lat_first();
            for (b = 0; b < LAT_BLOCKS && lat_mode[b] == BM_NONE; b++) {}
            sm = b;
            snprintf(t, sizeof t, "%s pressed before block 207, %s: the effect starts in block %u (resident) and %u (module)%s; the first output sample that differs from the render with no key is "
                     "sample %u (resident) and %u (module): the same", SC_NM[x], playing ? "playing" : "stopped", (unsigned)sr, (unsigned)sm,
                     playing ? ", the block of the next 1/16" : ", the block of the press", (unsigned)fr, (unsigned)fm);
            ck(t, fr == fm && fr != 0xFFFFFFFFu && sr == sm && !memcmp(mr, lat_mode, sizeof mr) && (playing ? sr >= 207u && sr <= 207u + 3308u / CTL + 1u : sr == 207u) && fr >= sr * CTL);
        }
    }
    free(lat_ref); free(lat_out);
}
/* the crash mark, the Effects table's `uses`, a removal under a playing effect */
static void host_checks(void)
{
    uint64_t c0;
    uint32_t k;
    flp_tap = 0;                                         /* (these blocks are not a scenario's: nothing to compare them with) */
    fresh();
    fx3_modk_clear();
    (void)module_set();
    memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart); memset(msamp, 0, sizeof msamp);
    fx3_modk_note = note_hook; note_n = 0;
    run(100);
    ck("the module registered, no key: no call into the module, no crash mark, `uses` 0, the 32 KB not taken", no_calls() && note_n == 0 && fx3_modk_uses() == 0 && !sl_buf);
    perf_press(PF_R16, 1); run(300); perf_press(PF_R16, 0); run(40);
    perf_press(PF_SCRU, 1); perf_press(PF_STHR, 1); perf_press(PF_M1, 1); run(300); perf_press(PF_SCRU, 0); perf_press(PF_STHR, 0); perf_press(PF_M1, 0); run(900);
    ck("a REPEAT, CRUSH, THROW and a mute (resident for good): the same: no call, no mark, `uses` 0", no_calls() && note_n == 0 && fx3_modk_uses() == 0);
    perf_press(PF_GLT, 1); run(1);
    ck("the block GLITCH starts in (stopped: the block of the press): the crash mark is taken once, the module's start ran once and its run in that same block, `uses` 1",
       note_n == 1u && mstart[0] == 1u && mrun[0] == 1u && msamp[0] == CTL && fx3_modk_uses() == 1u && pf.mode == BM_GLT);
    run(200);
    ck("GLITCH held: the module's run is called once a block (one call for the block's samples), the mark is not taken again", mrun[0] == 201u && mstart[0] == 1u && note_n == 1u);
    fx3_marked = 0;                                      /* (oc_mod_boot rebuilt the registry: the mark was zeroed) */
    run(2);
    ck("after a rebuild of the registry (the mark zeroed) it is taken again before the module's next call", note_n == 2u);
    fx3_modk_clear();
    c0 = mrun[0];
    run(300);
#if OC_PERF_RESIDENT
    ck("the module removed while GLITCH plays: not one call goes to it afterwards, `uses` 0, GLITCH goes on (the resident copy on the same state)", mrun[0] == c0 && fx3_modk_uses() == 0 && pf.mode == BM_GLT && sl_buf);
    (void)module_set();
    run(50);
    ck("the module registered again under it: its run is called from the next block on (no start: the state is the host's), the mark taken before that call", mrun[0] == c0 + 50u && mstart[0] == 1u && note_n == 3u && fx3_modk_uses() == 1u);
#else
    ck("no resident copy: the module removed while GLITCH plays: not one call goes to it afterwards, GLITCH is cut in the next block and the 32 KB is given back; the key still down does nothing",
       mrun[0] == c0 && fx3_modk_uses() == 0 && pf.mode == BM_NONE && !sl_buf && !sl_lent && !(perf_act & PF_GX) && !pf.k.w);
    (void)module_set();
    run(50);
    ck("the module registered again, the key still down: GLITCH starts again from the next block", pf.mode == BM_GLT && mstart[0] == 2u && mrun[0] == c0 + 50u && sl_buf);
#endif
    perf_press(PF_GLT, 0); run(60);
    for (k = 1; k < 4u; k++) { perf_press(FLP_E[k], 1); run(120); perf_press(FLP_E[k], 0); run(40); }
    ck("TAPE STOP, SHUFFLE and FREEZE in turn: each through its own start and run", mstart[1] == 1u && mstart[2] == 1u && mstart[3] == 1u && mrun[1] >= 120u && mrun[2] >= 120u && mrun[3] >= 120u);
    fx3_modk_note = 0;
    fx3_modk_clear();
}
#if !OC_PERF_RESIDENT
/* the Nightly after: what the layer does with no resident copy and no module */
static void absent_checks(void)
{
    static int32_t ref[400][CTL * 2], out[400][CTL * 2];
    uint32_t x, b, ok;
    fresh();
    fx3_modk_clear();
    ck("no resident copy, no module: perf_avail leaves out GLITCH, TAPE STOP, SHUFFLE and FREEZE and keeps every other key", (perf_avail() & PF_GX) == 0 && (perf_avail() | PF_GX) == ~0u);
    for (x = 0; x < 4u; x++) {
        char t[300];
        fresh(); G(G_BPM, 200); gb = 0;
        for (b = 0; b < 400u; b++) { notes(); mix_block(ref[b], CTL); gb++; }
        fresh(); G(G_BPM, 200); gb = 0;
        ok = 1;
        for (b = 0; b < 400u; b++) {
            if (b == 100u) perf_press(FLP_E[x], 1);
            if (b == 300u) perf_press(FLP_E[x], 0);
            notes(); mix_block(out[b], CTL); gb++;
            ok &= pf.mode == BM_NONE && !sl_buf && !sl_lent && !(jfx_full & JFX_FULL_SL) && !perf_hush && !(perf_act & PF_GX) && !pf.k.w;
            if (b == 200u) ok &= perf_gfocus() == PF_N && (perf_down >> FLP_E[x]) & 1u;
        }
        snprintf(t, sizeof t, "no resident copy, no module: %s held for 200 blocks: every output sample is the render's with no key, no memory is taken, no FX FULL, nothing runs, no track is hushed, "
                 "the layer shows no card for it (the key is down: perf_down), and it is idle again after", SC_NM[x]);
        ck(t, ok && !memcmp(ref, out, sizeof ref) && !pf.busy);
    }
    fresh(); G(G_BPM, 200); gb = 0;
    perf_press(PF_R16, 1); run(300);
    ok = pf.mode == BM_LOOP && sl_buf && (perf_act >> PF_R16) & 1u;
    perf_press(PF_GLT, 1); run(100);                    /* (a glitcher's key over a REPEAT: the REPEAT goes on) */
    ok &= pf.mode == BM_LOOP && pf.src == PF_R16;
    perf_press(PF_GLT, 0); perf_press(PF_R16, 0); run(60);
    ok &= pf.mode == BM_NONE && !sl_buf;
    perf_press(PF_SCRU, 1); perf_press(PF_STHR, 1); perf_press(PF_M1 + 1, 1); run(400);
    ok &= pf.cw > 0 && pf.td > 0 && pf.mg[1] == 0 && perf_hush == 2u;
    perf_press(PF_SCRU, 0); perf_press(PF_STHR, 0); perf_press(PF_M1 + 1, 0); run(1200);
    ck("no resident copy, no module: REPEAT 1/16 records and loops (a glitcher's key over it changes nothing), CRUSH, THROW and a mute work and end: the rest of the layer is as it was", ok && !pf.busy && !perf_hush);
    {   fx3_reg_t one_[1] = { { 3, oc_host_fxl_caps(2), &WRAP[2] } };
        fx3_modk_clear();
        ok = fx3_modk_set(FX3_MODULE_ID, 0x00010000u, 1, one_) == FX3M_OK && (perf_avail() & PF_GX) == PF_BIT(PF_SHUF);
        ok &= module_set() == FX3M_OK && (perf_avail() & PF_GX) == PF_GX;
        fx3_modk_clear();
        ok &= (perf_avail() & PF_GX) == 0;
        ck("no resident copy: perf_avail follows the registration: a bank that serves SHUFFLE alone gives SHUFFLE's key alone, the whole bank all four, a cleared table none", ok);
    }
}
#endif
int main(void)
{
    uint32_t s, r;
    memcpy(gp_init, (void *)perf_gp, sizeof gp_init);
    stopfirst = getenv("FLM_STOPFIRST") != 0;
    keep = malloc(sizeof *keep * FLM_BLOCKS);
    if (!keep) return 2;
    printf("fx layer module: MELODEE_FX_DEFER %d, OC_PERF_RESIDENT %d\n", (int)MELODEE_FX_DEFER, (int)OC_PERF_RESIDENT);
    if (stopfirst) {                                     /* the mutation check: the descriptor and the host's checks of it first (a mutated descriptor word ends here), then A against B */
        refusal_checks();
        descriptor_checks();
        if (bad) { printf("fx layer module: %d FAILED\nfx layer module: VERDICT descriptor: %d check(s) of the module's descriptor and its registration failed\n", bad, bad); return 1; }
    }
#if OC_PERF_RESIDENT
    if (getenv("FLM_E_ONLY")) {                          /* (for a quick look: run E alone) */
        direct_checks();
        printf(bad ? "fx layer module: %d FAILED\n" : "fx layer module: E passed\n", bad);
        return bad != 0;
    }
    for (uint32_t q = 0; q < NSC; q++) {
        /* the scenarios in their order; the mutation check may name the one to run first (FLM_FIRST: the scenario of the mutated effect), so a mutant is caught in seconds */
        const char *first = stopfirst ? getenv("FLM_FIRST") : 0;
        uint32_t f = NSC;
        for (s = 0; first && s < NSC; s++) if (!strcmp(first, SCN[s])) f = s;
        s = f == NSC ? q : q == 0u ? f : q <= f ? q - 1u : q;
        memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart);
        one(s, 0);
        if (keep_n >= FLM_BLOCKS) { ck("the scenario fits the kept blocks", 0); return 1; }
        ck("A resident (no module registered): not one call reached the module", no_calls());
        memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart);
        one(s, 1);
        for (r = 0; r < 4u; r++) { runB[r] += mrun[r]; startB[r] += mstart[r]; }
        if (stopfirst) continue;
        memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart);
        tog_n = s * 17u;
        one(s, 2);
        for (r = 0; r < 4u; r++) runC[r] += mrun[r];
        memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart);
        one(s, 3);
        ck("D the module removed: not one call reached it", no_calls());
    }
    ck("A resident: the printout is tests/fx_layer_golden.txt's, pinned from the resident code before the cut (twelve scenarios)", golden_ok(resA));
    {   char t[400];
        snprintf(t, sizeof t, "B the module registered: every output block equals the resident render's (%u blocks, %u differ%s%s) and the hashes of the twelve scenarios (the samples and the behaviour) are equal",
                 (unsigned)blkX[1], (unsigned)difX[1], difX[1] ? "; the first: " : "", first_bad);
        ck(t, !difX[1] && blkX[1] > 600000u && same_res(1));
        snprintf(t, sizeof t, "... and the module ran: starts GLITCH %llu, TAPE STOP %llu, SHUFFLE %llu, FREEZE %llu; runs %llu, %llu, %llu, %llu", (unsigned long long)startB[0], (unsigned long long)startB[1],
                 (unsigned long long)startB[2], (unsigned long long)startB[3], (unsigned long long)runB[0], (unsigned long long)runB[1], (unsigned long long)runB[2], (unsigned long long)runB[3]);
        ck(t, startB[0] > 100u && startB[1] > 100u && startB[2] > 100u && startB[3] > 100u && runB[0] > 20000u && runB[1] > 20000u && runB[2] > 20000u && runB[3] > 20000u);
        if (!stopfirst) {
            snprintf(t, sizeof t, "C the module appearing and going away every 61 blocks, in the middle of effects, fades and switches: every output block equals the resident render's (%u blocks, %u differ) "
                     "and the hashes are equal, both copies having played parts of it (the module's runs: %llu, %llu, %llu, %llu, fewer than in B)", (unsigned)blkX[2], (unsigned)difX[2],
                     (unsigned long long)runC[0], (unsigned long long)runC[1], (unsigned long long)runC[2], (unsigned long long)runC[3]);
            ck(t, !difX[2] && blkX[2] == blkX[1] && same_res(2) && runC[0] > 5000u && runC[0] < runB[0] && runC[1] > 5000u && runC[1] < runB[1] && runC[2] > 5000u && runC[2] < runB[2] && runC[3] > 5000u && runC[3] < runB[3]);
            snprintf(t, sizeof t, "D the module removed: every output block equals the resident render's (%u blocks, %u differ) and the hashes are equal", (unsigned)blkX[3], (unsigned)difX[3]);
            ck(t, !difX[3] && blkX[3] == blkX[1] && same_res(3));
            /* the granularity: the calls of a block, counted over runs B and C */
            snprintf(t, sizeof t, "the calls a block: %llu run calls in %u blocks that had one (%.3f a block), at most %u in one block (a run before the 1/16's sample, one after it, one more where an "
                     "effect ends and the next starts) and at most %u start(s); the resident text was called once a SAMPLE (32 a block) through PERF_RING",
                     (unsigned long long)runs_total, (unsigned)blk_with, blk_with ? (double)runs_total / blk_with : 0.0, (unsigned)blk_runs_max, (unsigned)blk_starts_max);
            ck(t, blk_with > 100000u && blk_runs_max <= 4u && blk_starts_max <= 2u && (double)runs_total / blk_with < 1.05);
        }
    }
    direct_checks();                                     /* (the mutation check too, after the renders: a mutant only this catches says so: VERDICT kernel) */
    if (!stopfirst) {
        latency_checks();
        host_checks();
    }
#else
    /* the Nightly after: no resident copy of the four effects in the image */
    absent_checks();
    for (s = 0; s < NSC; s++) {
        memset(mrun, 0, sizeof mrun); memset(mstart, 0, sizeof mstart);
        snprintf(flp_only, sizeof flp_only, "%s", SCN[s]);
        fx3_modk_clear();
        (void)module_set();
        flp_tap = 0;
        flp_scenarios();
        resA[s] = flp_res[0];
        for (r = 0; r < 4u; r++) { runB[r] += mrun[r]; startB[r] += mstart[r]; }
        flp_only[0] = 0;
    }
    ck("no resident copy, the module registered: the printout is tests/fx_layer_golden.txt's (the resident render pinned before the cut): the module alone plays the four effects as the image did", golden_ok(resA));
    ck("... through its calls", runB[0] > 20000u && runB[1] > 20000u && runB[2] > 20000u && runB[3] > 20000u && startB[0] > 100u && startB[1] > 100u && startB[2] > 100u && startB[3] > 100u);
    host_checks();
#endif
    flp_tap = 0;
    if (!stopfirst) {
        refusal_checks();
        descriptor_checks();
    }
    if (bad && stopfirst) {                             /* (the mutation check: a failed check that is neither the descriptor's nor a differing block is no proof that the mutant was seen) */
        printf("fx layer module: %d FAILED\nfx layer module: VERDICT none: a check failed that is not the render's or the descriptor's\n", bad);
        return 3;
    }
    if (bad) { printf("fx layer module: %d FAILED\n", bad); return 1; }
    printf("fx layer module: all passed\n");
    return 0;
}
