/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat, the engine candidates gate (docs/orangecat/ENGINE_CANDIDATES.md, "Frodhes"): the Rhodes port (tools/module/rhodes_mod.c, the module's unit
 * built for the host: tests/module_host_rhodes_tu.c) against Frodhes' own engine (tests/rhodes_ref_tu.c: its rhodes.c, unchanged, as its host build builds
 * it), sample for sample. Each of the four instrument types (rhodes.c's RH_TYPE rows: STAGE 77, SUITCASE, EARLY FELT, BELL DYNO) at three notes (A1 33,
 * C4 60, C7 96: the low, middle and high key tables), a note held 1.5 s and let go (the damper, the thump and the felt noise at the release), and a
 * voicing with every knob away from its default (a three-note chord and a re-strike), all rendered 3 s in Frodhes' 256-frame blocks (the port in Orange
 * Cat's 32-frame renders: eight a block). What is compared: Frodhes' mono output (rh_render) and the port's (post(), the same Q15 values before the
 * track's VOICE_FS scaling); the voices' ends (the port's done() against Frodhes' active bits).
 *
 *   build/host/rhodes_parity_test
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"

extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern const int32_t *oc_host_rhodes_mono(const track_t *t);
void ref_voicing(const int *k);
void ref_note_on(uint32_t note, uint32_t vel);
void ref_note_off(uint32_t note);
void ref_render(int32_t *mono, uint32_t n);
uint32_t ref_active(void);
void ref_reset(void);

static int bad;
static void check(const char *what, int ok)
{
    printf("rhodes parity: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define BLK 256u
typedef struct { uint32_t at, note, vel, off; } ev_t;   /* off: 1 a release of note at block `at` */
/* one case: knobs, events (by 256-frame block), length in blocks. Returns the first differing sample or -1; *peak the largest |sample| */
static long run(const engine_t *em, const int *k, const ev_t *ev, uint32_t nev, uint32_t blocks, int32_t *peak, uint32_t *ends_ok)
{
    track_t *t = &trk[0];
    static int32_t ref[BLK], out[CTL];
    uint32_t b, s, i, e;
    long first = -1;
    host_tracks_init();
    for (i = 0; i < 8u; i++) t->p[P_E0 + i] = (int16_t)k[i];
    memset(resource_get(RES_ENGINE0, OC_MODULE_HDR.state_size), 0, OC_MODULE_HDR.state_size);
    for (i = 0; i < NVOICE; i++) t->v[i].active = t->v[i].gate = 0;
    ref_reset();
    ref_voicing(k);
    *peak = 0;
    *ends_ok = 1;
    for (b = 0; b < blocks; b++) {
        for (e = 0; e < nev; e++)                       /* the block's events, in order, before its render (rhodes.c rh_events) */
            if (ev[e].at == b) {
                if (ev[e].off) {
                    ref_note_off(ev[e].note);
                    for (i = 0; i < NVOICE; i++) if (t->v[i].active && t->v[i].note == ev[e].note) t->v[i].gate = 0;
                } else {
                    ref_note_on(ev[e].note, ev[e].vel);
                    for (i = 0; i < NVOICE && t->v[i].active && t->v[i].note != ev[e].note; i++) ;   /* the same note's voice (a re-strike), else a free one */
                    t->v[i].note = (uint8_t)ev[e].note; t->v[i].vel = (uint8_t)ev[e].vel;
                    t->v[i].active = t->v[i].gate = 1;
                    em->note_on(t, &t->v[i]);
                }
            }
        ref_render(ref, BLK);
        for (s = 0; s < BLK / CTL; s++) {
            uint32_t nr = 0;
            vmod_t m;
            memset(&m, 0, sizeof m);
            m.amp0 = m.amp1 = 32768;                    /* Frodhes' own level: the port's amplitude product is exact */
            memset(out, 0, sizeof out);
            em->block(t);
            for (i = 0; i < NVOICE; i++) {
                voice_t *v = &t->v[i];
                if (!v->active) continue;
                if (em->done(t, v)) { v->active = v->gate = 0; continue; }
                em->render(t, v, out, CTL, &m);
                nr++;
            }
            em->post(t, out, CTL, nr);
            for (i = 0; i < CTL; i++) {
                int32_t a = ref[s * CTL + i], c = oc_host_rhodes_mono(t)[i];
                if (a != c && first < 0) first = (long)(b * BLK + s * CTL + i);
                if (a != c && getenv("RH_DEBUG") && (b * BLK + s * CTL + i) < (uint32_t)first + 8u) printf("  %u: ref %d port %d\n", b * BLK + s * CTL + i, a, c);
                if (a > *peak) *peak = a;
                if (-a > *peak) *peak = -a;
            }
        }
        {   uint32_t ra = ref_active(), oa = 0;
            for (i = 0; i < NVOICE; i++) if (t->v[i].active && !em->done(t, &t->v[i])) oa++;
            *ends_ok &= (uint32_t)__builtin_popcount(ra) == oa;
        }
    }
    return first;
}
int main(void)
{
    static const char *const TN[4] = {"STAGE 77", "SUITCASE", "EARLY FELT", "BELL DYNO"};
    static const uint32_t NOTES[3] = {33, 60, 96};
    const engine_t *em;
    fw_mod_hdr_t h;
    uint32_t ty, k, ends;
    int32_t peak;
    char what[160];
    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    check("the port's header: engine 23 (ENGI_RHODES, proposed), the state of a track, the registration check", h.engine == ENGI_RHODES && h.engine == 23u &&
          fw_module_check(&h) == FW_MOD_OK && h.state_size > 0 && em->ownenv && em->done && em->post && em->block);
    for (ty = 0; ty < 4u; ty++)
        for (k = 0; k < 3u; k++) {
            int kn[8] = {(int)ty, 64, 64, 64, 64, 64, 64, 48};
            ev_t ev[2] = {{0, NOTES[k], 100, 0}, {258, NOTES[k], 0, 1}};
            long d = run(em, kn, ev, 2, 517, &peak, &ends);
            snprintf(what, sizeof what, "%s, note %u, velocity 100, held 1.5 s, 3 s in all: every sample equal (peak %d), the voice's end the same",
                     TN[ty], NOTES[k], peak);
            check(what, d < 0 && peak > 100 && ends);
            if (d >= 0) printf("rhodes parity:   first difference at sample %ld\n", d);
        }
    /* every knob away from its default, a chord at three velocities, a re-strike of a sounding note, the release of all. The releases are three blocks
     * apart: the felt noise of a release (353 samples) draws from one generator a part, and two at once draw in another order here (Frodhes renders one
     * voice's whole 256-frame block before the next voice's; Orange Cat 32 frames of each voice in turn): the same noise, other draws
     * (docs/orangecat/ENGINE_CANDIDATES.md, "Frodhes", correctness) */
    {
        int kn[8] = {3, 90, 100, 30, 100, 40, 90, 127};
        ev_t ev[7] = {{0, 48, 40, 0}, {0, 55, 90, 0}, {0, 64, 127, 0}, {40, 55, 120, 0}, {120, 48, 0, 1}, {200, 55, 0, 1}, {203, 64, 0, 1}};
        long d = run(em, kn, ev, 7, 400, &peak, &ends);
        snprintf(what, sizeof what, "BELL DYNO, TONE 90 BARK 100 BELL 30 HAMR 100 DCAY 40 REL 90 NOIS 127: a chord, a re-strike, the releases (peak %d)", peak);
        check(what, d < 0 && peak > 100 && ends);
        if (d >= 0) printf("rhodes parity:   first difference at sample %ld\n", d);
    }
    printf(bad ? "rhodes parity: FAILED\n" : "rhodes parity: all passed\n");
    return bad != 0;
}
