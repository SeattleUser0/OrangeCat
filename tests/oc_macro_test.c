/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 1a (docs/orangecat/UI_BUILD.md): HOME's knob macros (firmware/src/oc_knobs.c, oc_macro.c) through
 * the firmware's own engines, voices and mix_block (tests/hostsim.c), built with OC_HOME_PIC=1.
 *   cc -O2 -w -DOC_HOME_PIC=1 -Ibuild/gen -Ifirmware/src -o oc_macro_test tests/oc_macro_test.c -lm
 * Checked:
 *   ZERO     every macro at 0 and the macros armed: every preset of MACRO, SHAPES and DRUM-X renders bit-identically
 *            to the same preset with nothing armed (the interrupt's path does nothing at the center)
 *   EXACT    on every preset, every macro knob at -64, -1, 1 and 63 for 48 blocks (a note held for 24): after each block every
 *            stored parameter is exactly what it was (the main loop never sees a macro)
 *   VALUE    a macro at m renders bit-identically to the same preset with each target set by hand to
 *            clamp(stored + depth x shape(m) / 64) (this file's own formula, not oc_macro.c's) and nothing armed
 *   KEEP     a value written into a target during the block stays after it (the throws' rule)
 *   CHAIN    two macros on one target (a made table) come off exactly and put on the sum, clamped in order
 *   THROWS   a throw and the macros together: each comes off exactly; throw_stop keeps the macros armed, mac_poll
 *            disarms once every macro is back at 0
 *   FADE     an engine switch under way (eng_req != engine) or a crossfade running (xf_on): nothing is put on
 *   SAVED    mac_saved_value gives what the interrupt puts on, the macro ids as 0; a target a motion lane plays is left
 *            alone by both (motion writes it absolutely; its saved events carry no offset)
 *   MOVES    every macro knob changes the render at -64 or 63 on every preset (how much: tests/knob_audit.py)
 *   IDS      every target of every table is a parameter (below P_COUNT), never a macro's own id
 *   TABLE    the firmware's table matches tests/knob_audit.py's KNOBS (the audit renders what the unit plays)
 * Revisions:
 *   (the new interface 0.1, branch ui-build) first version.
 */
static const struct knob_def_s *test_kdef(unsigned e);
#define OC_KDEF_TEST_HOOK(e) ((const knob_def_t *)test_kdef(e))
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <sys/mman.h>

static int bad, nchk;
static void check(const char *what, int ok)
{
    nchk++;
    if (!ok || getenv("VERBOSE")) printf("oc_macro: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
/* the made table of CHAIN: engine 8 (MACRO) with two macros on HARM (E1), one LIN, one UP */
static int test_chain;
static const knob_def_t TEST_CHAIN[4] = {
    {"A", KD_MACRO, 0, {{P_E1, KS_LIN, 100}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}}},
    {"B", KD_MACRO, 0, {{P_E1, KS_UP, -60}, {P_E2, KS_DOWN, 127}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}}},
    {"C", KD_PLAIN, P_E3, {{KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}}},
    {"D", KD_NONE, 0, {{KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}, {KD_NOT, 0, 0}}},
};
static const struct knob_def_s *test_kdef(unsigned e)
{
    return test_chain && e == ENGI_MACRO ? (const struct knob_def_s *)(const void *)TEST_CHAIN : 0;
}

static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    memset(resource, 0, sizeof resource);
    song.master_q12 = 4096;
    thr_out = thr_on = 0;
    memset(mac, 0, sizeof mac);
}
/* this file's own reading of UI_DESIGN.md's shapes (independent of oc_macro.c's) */
static int32_t want_value(uint32_t e, const kd_tgt_t *g, int32_t stored, int32_t m)
{
    const param_desc_t *d = param_desc_of(e, g->id);
    int32_t x = g->shape == KS_LIN ? m : g->shape == KS_UP ? (m > 0 ? m : 0) : (m < 0 ? m : 0);
    int32_t v = stored + (g->depth * x) / 64;                  /* (C division truncates toward 0, as the table says) */
    return v < d->min ? d->min : v > d->max ? d->max : v;
}
/* render NB blocks of a note from a fresh start of preset pi of engine e (the macros as `m`, armed through mac_poll; force:
 * MAC_ARM set whatever the macros); out: the left samples; *exact: every stored parameter the same after each block.
 * Each render runs in its own fork()ed child of a process that never rendered (as tests/regress.c), so every take starts
 * from the same DSP state (the generators, the buses, the voice clock) */
#define NB 48u
static void render_body(uint32_t e, uint32_t pi, const int8_t m[4], int hand, int force, int16_t *out, int *exact)
{
    int32_t buf[2 * CTL];
    int16_t keep[P_COUNT];
    uint32_t b, i;
    fresh();
    host_preset(&trk[0], e, pi);
    for (i = 0; i < 4u; i++)
        trk[0].p[P_MAC0 + i] = m[i];
    if (hand) {                                         /* VALUE: the targets by hand, the macros back to 0 */
        const knob_def_t *kd = kdef_of(e);
        for (i = 0; i < 16u; i++) {
            const kd_tgt_t *g = &kd[i / 4u].t[i % 4u];
            if (kd[i / 4u].kind == KD_MACRO && g->id != KD_NOT && m[i / 4u])
                trk[0].p[g->id] = (int16_t)want_value(e, g, trk[0].p[g->id], m[i / 4u]);
        }
        for (i = 0; i < 4u; i++)
            trk[0].p[P_MAC0 + i] = 0;
    }
    mac_poll();
    if (force) thr_out |= MAC_ARM;
    memcpy(keep, trk[0].p, sizeof keep);
    *exact = 1;
    trk_note_on(&trk[0], e == ENGI_DRUMX ? 36 : 48, 100);
    for (b = 0; b < NB; b++) {
        if (b == NB / 2u) trk_note_off(&trk[0], e == ENGI_DRUMX ? 36 : 48);
        mix_block(buf, CTL);
        *exact &= !memcmp(keep, trk[0].p, sizeof keep);
        for (i = 0; i < CTL; i++) {
            int32_t l = buf[2 * i];
            out[b * CTL + i] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        }
    }
}
static struct { int16_t take[NB * CTL]; int exact; } *shm;
static void render_take(uint32_t e, uint32_t pi, const int8_t m[4], int hand, int force, int16_t *out, int *exact)
{
    pid_t pid;
    int st = 0;
    if (!shm) shm = mmap(0, sizeof *shm, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (!(pid = fork())) {
        render_body(e, pi, m, hand, force, shm->take, &shm->exact);
        _exit(0);
    }
    waitpid(pid, &st, 0);
    if (!WIFEXITED(st) || WEXITSTATUS(st)) { printf("oc_macro: a render crashed\n"); exit(1); }
    memcpy(out, shm->take, sizeof shm->take);
    *exact = shm->exact;
}
static int16_t take_a[NB * CTL], take_b[NB * CTL];

static void engine_checks(uint32_t e)
{
    const engine_t *en = ENGINES[e];
    const knob_def_t *kd = kdef_of(e);
    static const int8_t MS[4] = {-64, -1, 1, 63};
    char line[200];
    uint32_t pi, k, j, zero_ok = 1, exact_ok = 1, value_ok = 1, nvalue = 0, moves_ok = 1;
    static int16_t take_0[NB * CTL];
    for (pi = 0; pi < en->npresets; pi++) {
        int8_t m0[4] = {0, 0, 0, 0};
        int ex;
        int ex0;
        render_take(e, pi, m0, 0, 0, take_a, &ex);      /* nothing armed (all 0) */
        render_take(e, pi, m0, 0, 1, take_b, &ex0);     /* the bit forced on: mac_on runs, puts nothing */
        zero_ok &= ex && ex0;
        zero_ok &= !memcmp(take_a, take_b, sizeof take_a);
        memcpy(take_0, take_a, sizeof take_0);
        for (k = 0; k < 4u; k++) {
            int moved = 0;
            if (kd[k].kind != KD_MACRO) continue;
            for (j = 0; j < 4u; j++) {
                int8_t m[4] = {0, 0, 0, 0};
                int ex2;
                m[k] = MS[j];
                render_take(e, pi, m, 0, 0, take_a, &ex);
                exact_ok &= ex;
                moved |= (j == 0u || j == 3u) && memcmp(take_a, take_0, sizeof take_0);
                render_take(e, pi, m, 1, 0, take_b, &ex2);
                if (memcmp(take_a, take_b, sizeof take_a)) {
                    if (value_ok) printf("oc_macro: %s %s %s at %d differs from the targets set by hand\n", en->name,
                                         eng_preset(en, pi)->name, kd[k].label, MS[j]);
                    value_ok = 0;
                }
                nvalue++;
            }
            if (!moved) {
                printf("oc_macro: %s %s: %s moves nothing at either end\n", en->name, eng_preset(en, pi)->name, kd[k].label);
                moves_ok = 0;
            }
        }
    }
    snprintf(line, sizeof line, "ZERO   %-7s every preset (%u): macros at 0 and armed render as nothing armed", en->name,
             (unsigned)en->npresets);
    check(line, zero_ok);
    snprintf(line, sizeof line, "EXACT  %-7s every preset, every macro at -64 -1 1 63: the stored values exact after each block",
             en->name);
    check(line, exact_ok);
    snprintf(line, sizeof line, "VALUE  %-7s %u renders: a macro at m = its targets set by hand", en->name, (unsigned)nvalue);
    check(line, value_ok);
    snprintf(line, sizeof line, "MOVES  %-7s every macro changes the render at one end or the other, every preset (tests/knob_audit.py: how much)",
             en->name);
    check(line, moves_ok);
}

/* one block by hand: the throws' and macros' on (as mix_block's start), then their off */
static void block_on(void) { if (thr_out) throw_on(CTL); }
static void block_off(void) { if (thr_on) throw_off(); }

/* TABLE: tests/knob_audit.py's KNOBS for engine e, as written there, against oc_knobs.c's */
static int table_matches(uint32_t e, const char *py)
{
    const knob_def_t *kd = kdef_of(e);
    char key[16], want[1024], *w = want;
    const char *a, *z;
    snprintf(key, sizeof key, "\n    %u: [", (unsigned)e);
    if (!(a = strstr(py, key))) return 0;
    a += strlen(key);
    {   /* its closing bracket */
        int depth = 1;
        for (z = a; *z && depth; z++) depth += *z == '[' ? 1 : *z == ']' ? -1 : 0;
        if (depth) return 0;
        z--;                                            /* (z: the list's ']') */
    }
    for (uint32_t k = 0; k < 4u; k++) {
        if (kd[k].kind == KD_PLAIN) {
            uint32_t id = kd[k].id;
            if (id >= P_E0) w += sprintf(w, "plain(\"%s\",E(%u))", kd[k].label, (unsigned)(id - P_E0));
            else w += sprintf(w, "plain(\"%s\",%u)", kd[k].label, (unsigned)id);
        } else {
            static const char *const SH[3] = {"LIN", "UP", "DOWN"};
            static const char *const CN[6] = {"", "P_ATK", "P_DEC", "P_SUS", "P_REL", "P_ED_FLT"};
            w += sprintf(w, "macro(\"%s\",[", kd[k].label);
            for (uint32_t j = 0; j < 4u && kd[k].t[j].id != KD_NOT; j++) {
                uint32_t id = kd[k].t[j].id;
                if (j) *w++ = ',';
                if (id >= P_E0) w += sprintf(w, "(E(%u),\"%s\",%d)", (unsigned)(id - P_E0), SH[kd[k].t[j].shape], kd[k].t[j].depth);
                else w += sprintf(w, "(%s,\"%s\",%d)", id <= 5u ? CN[id] : "?", SH[kd[k].t[j].shape], kd[k].t[j].depth);
            }
            w += sprintf(w, "])");
        }
        if (k < 3u) *w++ = ',';
    }
    *w = 0;
    {   /* the Python text with its spaces and line breaks out, to its closing bracket */
        char got[1024];
        uint32_t n = 0;
        for (const char *c = a; c < z && n + 1u < sizeof got; c++)
            if (*c != ' ' && *c != '\n') got[n++] = *c;
        got[n] = 0;
        if (strcmp(got, want)) {
            printf("oc_macro: TABLE engine %u\n  knob_audit.py: %s\n  oc_knobs.c:    %s\n", (unsigned)e, got, want);
            return 0;
        }
    }
    return 1;
}

int main(int argc, char **argv)
{
    char line[200];
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("oc_macro: the macros' RAM %zu B (mac[]), a table %zu B\n", sizeof mac, sizeof(knob_def_t[4]));
    engine_checks(ENGI_MACRO);
    engine_checks(ENGI_SHAPES);
    engine_checks(ENGI_DRUMX);

    {   /* KEEP: a value written into a target during the block stays */
        fresh();
        host_preset(&trk[0], ENGI_SHAPES, 0);
        trk[0].p[P_MAC0 + 2] = 40;                      /* TONE right: RES and DRV up */
        int16_t res = trk[0].p[P_E4], cut = trk[0].p[P_E3];
        mac_poll();
        block_on();
        int moved = trk[0].p[P_E4] != res;
        trk[0].p[P_E4] = 77;                            /* (as motion or a lock would) */
        block_off();
        snprintf(line, sizeof line, "KEEP   SHAPES TONE at 40: RES put on (%d -> moved), written 77 in the block: 77 stays, CUT untouched",
                 res);
        check(line, moved && trk[0].p[P_E4] == 77 && trk[0].p[P_E3] == cut);
    }
    {   /* CHAIN: two macros on one target */
        int ok = 1;
        test_chain = 1;
        for (int a = -64; a <= 63; a += 9)
            for (int b2 = -64; b2 <= 63; b2 += 11) {
                fresh();
                host_preset(&trk[0], ENGI_MACRO, 0);
                trk[0].p[P_E1] = 100;
                trk[0].p[P_E2] = 10;
                trk[0].p[P_MAC0] = (int16_t)a;
                trk[0].p[P_MAC0 + 1] = (int16_t)b2;
                trk[0].p[P_MAC0 + 2] = 33;              /* (a plain knob's P_MAC: never applied) */
                int16_t keep[P_COUNT];
                memcpy(keep, trk[0].p, sizeof keep);
                mac_poll();
                int32_t e1 = want_value(ENGI_MACRO, &TEST_CHAIN[1].t[0], want_value(ENGI_MACRO, &TEST_CHAIN[0].t[0], 100, a), b2);
                int32_t e2 = want_value(ENGI_MACRO, &TEST_CHAIN[1].t[1], 10, b2);
                ok &= mac_saved_value(&trk[0], P_E1) == e1 && mac_saved_value(&trk[0], P_E2) == e2;   /* (the main loop's view) */
                block_on();
                ok &= trk[0].p[P_E1] == e1 && trk[0].p[P_E2] == e2 && trk[0].p[P_E3] == keep[P_E3];
                block_off();
                ok &= !memcmp(keep, trk[0].p, sizeof keep);
            }
        test_chain = 0;
        check("CHAIN  two macros on one target: on in order, clamped each time; off exactly; saved as sounded", ok);
    }
    {   /* THROWS: a throw out and the macros together */
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 3);
        trk[0].p[P_REV] = 30;
        trk[0].p[P_MAC0 + 3] = 50;                      /* PLUCK right */
        int16_t keep[P_COUNT];
        memcpy(keep, trk[0].p, sizeof keep);
        mac_poll();
        throw_turn(0, 0, 20);                           /* PERFORM's KNOB 2: the reverb send thrown by 20 */
        block_on();
        int both = keep[P_E5] < 127 && trk[0].p[P_REV] == 50 && trk[0].p[P_E5] != keep[P_E5];   /* (LPG below its top: it moves) */
        block_off();
        int back = !memcmp(keep, trk[0].p, sizeof keep);
        throw_stop();
        int armed = (thr_out & MAC_ARM) && !(thr_out & ~MAC_ARM);
        trk[0].p[P_MAC0 + 3] = 0;
        mac_poll();
        check("THROWS a throw and a macro on together, both off exactly; throw_stop keeps the macros armed; 0 disarms",
              both && back && armed && thr_out == 0);
    }
    {   /* FADE: nothing while the engine asked for is not the one sounding */
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        trk[0].p[P_MAC0 + 3] = 63;
        trk[0].eng_req = ENGI_SHAPES;                   /* (a switch asked, the old engine still fading) */
        int16_t keep[P_COUNT];
        memcpy(keep, trk[0].p, sizeof keep);
        mac_poll();
        thr_out |= MAC_ARM;                             /* (armed whatever the table of the engine asked for) */
        block_on();
        int none = !memcmp(keep, trk[0].p, sizeof keep) && !(thr_on & MAC_ARM);
        block_off();
        trk[0].eng_req = ENGI_MACRO;                    /* a crossfade still running with the engines the same (A to B to A) */
        trk[0].xf_on = 1;
        thr_out |= MAC_ARM;
        block_on();
        none &= !memcmp(keep, trk[0].p, sizeof keep) && !(thr_on & MAC_ARM);
        block_off();
        trk[0].xf_on = 0;
        check("FADE   an engine switch under way, or a crossfade running: nothing put on", none);
    }
    {   /* SAVED: mac_saved_value is what the interrupt puts on; the macro ids 0; motion's base */
        int ok = 1;
        for (uint32_t e = 0; e < 3u; e++) {
            uint32_t eng = e == 0 ? ENGI_MACRO : e == 1 ? ENGI_SHAPES : ENGI_DRUMX;
            for (uint32_t pi = 0; pi < ENGINES[eng]->npresets; pi += 3) {
                fresh();
                host_preset(&trk[0], eng, pi);
                for (uint32_t k = 0; k < 4u; k++) trk[0].p[P_MAC0 + k] = (int16_t)((int)(k * 37u % 127u) - 64);
                mac_poll();
                int16_t saved[P_COUNT];
                for (uint32_t id = 0; id < P_COUNT; id++)
                    saved[id] = mac_saved_value(&trk[0], id);   /* (in the main loop: never inside a block) */
                block_on();
                for (uint32_t id = 0; id < P_COUNT; id++)
                    ok &= saved[id] == (id >= P_MAC0 && id <= P_MAC3 ? 0 : trk[0].p[id]);
                block_off();
            }
        }
        /* under motion: the base, not the sounding value, is what the macro is committed into */
        fresh();
        host_preset(&trk[0], ENGI_MACRO, 0);
        trk[0].p[P_MAC0 + 3] = 64 - 1;
        motion_active[0][P_E5 / 32u] |= 1u << (P_E5 % 32u);
        motion_base[0][P_E5] = 20;
        trk[0].p[P_E5] = 90;                            /* (a motion lane's value sounding) */
        ok &= mac_saved_value(&trk[0], P_E5) == 20;     /* (a target motion plays: left alone, its base saved) */
        ok &= mac_saved_value(&trk[0], P_E6) == want_value(ENGI_MACRO, &KD_MACRO_ENG[3].t[1], trk[0].p[P_E6], 63);
        mac_poll();
        block_on();
        ok &= trk[0].p[P_E5] == 90 && trk[0].p[P_E6] == want_value(ENGI_MACRO, &KD_MACRO_ENG[3].t[1], motion_base_value(&trk[0], P_E6), 63);
        block_off();
        memset(motion_active, 0, sizeof motion_active);
        {   /* (on track 3: the track index) */
            int16_t saved[P_COUNT];
            fresh();
            host_preset(&trk[2], ENGI_SHAPES, 4);
            trk[2].p[P_MAC0] = 31; trk[2].p[P_MAC0 + 3] = -50;
            mac_poll();
            for (uint32_t id = 0; id < P_COUNT; id++) saved[id] = mac_saved_value(&trk[2], id);
            block_on();
            for (uint32_t id = 0; id < P_COUNT; id++) ok &= saved[id] == (id >= P_MAC0 && id <= P_MAC3 ? 0 : trk[2].p[id]);
            block_off();
        }
        check("SAVED  the value saved is the value put on (every id); macros as 0; a target motion plays left alone, live and saved", ok);
    }
    {   /* COST (printed, not a gate): mac_on + mac_off with four SHAPES tracks and every macro turned (36 targets), on this
         * host; Session A times the device (UI_BUILD.md "Waiting on Session A") */
        struct timespec t0, t1;
        uint32_t r, N = 200000u;
        fresh();
        for (uint32_t k = 0; k < NTRK; k++) {
            host_preset(&trk[k], ENGI_SHAPES, k);
            trk[k].p[P_MAC0] = 50; trk[k].p[P_MAC0 + 2] = -30; trk[k].p[P_MAC0 + 3] = 40;
        }
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (r = 0; r < N; r++) { (void)mac_on(); mac_off(); }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        printf("oc_macro: COST mac_on + mac_off, 4 SHAPES tracks, 3 macros each (36 targets): %.0f ns a block on this host\n",
               ((t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec)) / N);
    }
    {   /* IDS */
        int ok = 1;
        for (uint32_t e = 0; e < NENGINES; e++) {
            const knob_def_t *kd = kdef_of(e);
            for (uint32_t i = 0; kd && i < 16u; i++) {
                uint32_t id = kd[i / 4u].t[i % 4u].id;
                ok &= id == KD_NOT || (id < P_COUNT && !(id >= P_MAC0 && id <= P_MAC3));
                ok &= kd[i / 4u].kind != KD_PLAIN || (kd[i / 4u].id < P_COUNT && !(kd[i / 4u].id >= P_MAC0 && kd[i / 4u].id <= P_MAC3));
            }
        }
        check("IDS    every table's targets and plain ids are parameters below P_COUNT, never a macro's own id", ok);
    }
    {   /* TABLE */
        FILE *f = fopen(argc > 1 ? argv[1] : "tests/knob_audit.py", "rb");
        static char py[65536];
        size_t n = f ? fread(py, 1, sizeof py - 1u, f) : 0;
        if (f) fclose(f);
        py[n] = 0;
        check("TABLE  oc_knobs.c's MACRO, SHAPES, DRUM-X are tests/knob_audit.py's (the audit renders what HOME plays)",
              n && table_matches(ENGI_MACRO, py) && table_matches(ENGI_SHAPES, py) && table_matches(ENGI_DRUMX, py));
    }
    printf("oc_macro: %d of %d checks failed\n", bad, nchk);
    return bad != 0;
}
