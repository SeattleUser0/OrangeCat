/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.9.0: DRUM-X (eng_drumx.c, drumx_voice.c; Jiant FM's drum engine) through the real voices, sequencer and mixer.
 *   build/host/drumx_test [reference file]            (default tests/drumx_ref.txt)
 * 1. the voice against Jiant FM's: 288 renders (nine kits, eight lanes, four scenarios of every argument of dx_run, one
 *    of them moving them while the hit rings, striking again and choking) equal, sample for sample, the renders Jiant's own
 *    drumx_voice.c wrote into tests/drumx_ref.txt (tests/drumx_ref.h, tools/drumx_ref_gen.sh).
 * 2. the engine is registered in slot 9, can be picked, is in the browsing order, has a preset for each kit; the
 *    drum track test (seq.c drum_track) is true for it and for no other engine of the Orange Cat set.
 * 3. the GM map: each of 0..127 strikes the lane the grid shows (eng_drum.c drum_lane), and a lane's grid note strikes it.
 * 4. every kit, every lane, struck through trk_note_on and the mixer: sounds (above -30 dBFS), never at full scale, ends
 *    (its voice frees itself) within 3 s.
 * 5. a closed hat chokes the open one; a lane's one voice is reused at another pitch; eight lanes ring together.
 * 6. LANES: a lane's level 0 is silent, 64 is a quarter of the amplitude, 127 as the kit has it; the other lanes unmoved.
 * 7. DRV changes the sum and keeps it under full scale; the knobs (MRPH, TUNE, DECY, NOIS, FM, FOLD) move the sound.
 * 8. two tracks play two kits at once, each as it plays alone (no global kit).
 * 9. the step grid: a step's hit lanes play their GM notes, an accented lane at 127, a note on a lane; a RATCH repeats them.
 * 10. the keys: the first C is the kick (36); other engines' drum_track false.
 * 11. (0.13.1) lane mutes, performance state: a muted lane does not sound or flash and holds no voice, a ringing hit of it is cut, the other lanes are unmoved,
 *     the mute is per part, a clear puts every lane back, and the mask is not part of the stored track.
 * (0.14.2) drum kits (eng_drumx.c dxk_t, dxk_rec.h):
 * 12. a kit expanded with every value at its default (dxk_take) renders every lane of the nine factory kits as the factory kit does,
 *     sample for sample: the defaults are the fixed values of before.
 * 13. each lane value (TUNE, DECAY, LAYER, its FDBK, PUNCH, ATK+, BEND) moves its own lane and no other; TONE's BP the metal lanes, not
 *     the kick; HANDS the clap only; what each does: TUNE up raises the pitch, DECAY down ends the hit sooner, ATK+ softens the start,
 *     PUNCH makes the body louder, BEND moves the pitch over the hit.
 * 14. PAN: a lane hard left is silent on the right and on the left as it is at the centre; half right halves the left; the other lanes stay
 *     where they were; the track's PAN applies after the lane's (the law multiplied); DIST, SPREAD and the FX mute keep the two sides.
 * 15. DUCK: a kick of a kit with DUCK takes another part down and lets it back; no DUCK, a muted kick, or a drum track: no dip.
 * 16. the kit's record (dxk_rec.h): a kit round-trips byte for byte; a bad mark, version, CRC, length or value is refused; a longer body
 *     (a later firmware's) reads as far as this one knows.
 * 17. the cost, printed: ns per sample of a busy pattern through the mixer, every value at its default and with pan, tune and the
 *     sound values in use.
 * Printed (not checked): the sizes of the part state.
 *
 * Revisions:
 * (Orange Cat 0.13.1) DRUM-X lane mutes as performance state (ACC + a lane key; not stored; cleared at power-on and a project load); the other Jiant features left fixed,...
 *   0.9.0: first version.
 *   0.13.1: lane mutes (11).
 *   0.14.2: drum kits (12 to 17).
 */
#define SEQ_TEST_HOOK(t, nn, vv, m, sk) do {                                              \
        for (uint32_t _i = 0; _i < (m); _i++)                                            \
            if (!(((sk) >> _i) & 1u) && (t) == &trk[0] && ev_n < EV_MAX) {               \
                ev[ev_n].at = ev_clock; ev[ev_n].note = (nn)[_i]; ev[ev_n].vel = (vv)[_i]; ev_n++; \
            }                                                                            \
    } while (0)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EV_MAX 256
static struct { uint32_t at; uint8_t note, vel; } ev[EV_MAX];
static uint32_t ev_n, ev_clock;

#include <time.h>
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "drumx_ref.h"

static int bad;
static void check(const char *what, int ok)
{
    printf("drumx: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static void fresh(void)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    memset(resource, 0, sizeof resource);
    song.master_q12 = 4096;
    song.g[G_BPM] = 120;
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;   /* the master's leveler and limiter from rest each time */
    memset(dist_w, 0, sizeof dist_w);
}

/* the part's block as track_render gives it (twice the 16-bit scale: the master takes the half), so no master state carries
 * from one hit to the next (the leveler, the effects' tails) */
#define MAXB (44100u * 10u / CTL + 8u)
static int16_t take[MAXB * CTL];
static uint32_t take_n;

/* note on track 0 at vel, rendered until its voices are free or `max` blocks; take[] the output (16-bit scale). Returns the
 * block it freed at (0: still sounding at max) */
static uint32_t strike(uint32_t note, uint32_t vel, uint32_t max)
{
    int32_t out[CTL];
    uint32_t b, i, freed = 0;
    take_n = 0;
    trk_note_on(&trk[0], note, vel);
    for (b = 0; b < max && b < MAXB; b++) {
        int any = 0;
        track_render(&trk[0], out, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t l = out[i] >> 1;
            take[take_n++] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        }
        for (i = 0; i < NVOICE; i++) any |= trk[0].v[i].active;
        if (!any) {
            freed = b + 1u;
            break;
        }
    }
    return freed;
}
static int32_t peak_of(uint32_t from, uint32_t to)
{
    int32_t pk = 0;
    for (uint32_t i = from; i < to && i < take_n; i++) {
        int32_t a = take[i] < 0 ? -take[i] : take[i];
        if (a > pk) pk = a;
    }
    return pk;
}
static double rms_of(uint32_t from, uint32_t to)
{
    double s = 0;
    uint32_t n = 0;
    for (uint32_t i = from; i < to && i < take_n; i++, n++) s += (double)take[i] * take[i];
    return n ? __builtin_sqrt(s / n) : 0;
}
static uint64_t fnv64(const int16_t *p, uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++) {
        h ^= (uint16_t)p[i] & 255u; h *= 1099511628211ull;
        h ^= ((uint16_t)p[i] >> 8) & 255u; h *= 1099511628211ull;
    }
    return h;
}
static void drumx_track(uint32_t kit)
{
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, kit);
}

/* ------------------------------------------------------------ 1. Jiant */
static int test_reference(const char *path)
{
    FILE *f = fopen(path, "r");
    char line[128];
    uint32_t n = 0, equal = 0, k, l, s;
    unsigned long long h;
    if (!f) {
        printf("drumx: cannot open %s\n", path);
        return 1;
    }
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '#') continue;
        if (sscanf(line, "%u %u %u %llx", &k, &l, &s, &h) != 4) continue;
        n++;
        if (k < DXREF_NKIT && l < 8u && s < DXREF_NSCEN && dxref_hit(k, l, s) == h) equal++;
        else printf("drumx:   kit %u lane %u scenario %u differs from Jiant's\n", k, l, s);
    }
    fclose(f);
    printf("drumx:   %u of %u renders equal\n", equal, n);
    check("the voice equals Jiant FM's drumx_voice.c, sample for sample (288 renders, tests/drumx_ref.txt)",
          n == DXREF_NKIT * 8u * DXREF_NSCEN && equal == n);
    return 0;
}

/* ------------------------------------------------------- 2. registration */
static void test_registration(void)
{
    uint32_t k, inorder = 0;
    check("ENGINES[9] is DRUM-X, named DRUM-X", ENGINES[ENGI_DRUMX] == &ENG_DRUMX && !strcmp(ENG_DRUMX.name, "DRUM-X"));
    check("it can be picked (eng_ok) and is not retired", eng_ok(ENGI_DRUMX) && !eng_extra_retired(ENGI_DRUMX));
    for (k = 0; k < NENG_SHOWN; k++) inorder += eng_vis(k) == ENGI_DRUMX;
    check("it is once in the browsing order, reached by eng_step from its neighbour", inorder == 1u && eng_rank(ENGI_DRUMX) < NENG_SHOWN &&
          eng_vis(eng_rank(ENGI_DRUMX)) == ENGI_DRUMX);
    check("nine presets, a kit each, named as Jiant's", ENG_DRUMX.npresets == 9u && ENG_DRUMX.npresets == DX_NKIT &&
          !strcmp(ENG_DRUMX.presets[0].name, "DRUM-X") && !strcmp(ENG_DRUMX.presets[8].name, "BREAKS"));
    check("the part's state is its eight lanes", eng_state_size(ENGI_DRUMX) == sizeof(dxe_part_t));
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    host_preset(&trk[1], ENGI_MACRO, 0);
    check("drum_track is true for a DRUM-X track and false for a MACRO track", drum_track(&trk[0]) && !drum_track(&trk[1]));
    check("its units are two, its poly eight, it is a one-shot engine", ENG_DRUMX.units(&trk[0]) == 2u && ENG_DRUMX.poly == 8u && ENG_DRUMX.oneshot);
}

/* ------------------------------------------------------------ 3. GM map */
static void test_gm_map(void)
{
    uint32_t n, l, differ = 0, lanes_ok = 0;
    for (n = 0; n < 128u; n++) differ += dxe_lane(n) != drum_lane(n);
    for (l = 0; l < 8u; l++) lanes_ok += dxe_lane(DRUM_LANE_NOTE[l]) == l && drum_lane(DRUM_LANE_NOTE[l]) == l;
    if (differ) {
        printf("drumx:   notes whose lane differs between DRUM-X and the grid:");
        for (n = 0; n < 128u; n++) if (dxe_lane(n) != drum_lane(n)) printf(" %u(%u/%u)", n, dxe_lane(n), drum_lane(n));
        printf("\n");
    }
    check("every note 0..127 strikes the lane the grid shows (drum_lane)", differ == 0);
    check("each lane's grid note strikes that lane", lanes_ok == 8u);
}

/* ------------------------------------------------------- 4. every lane */
static void test_lanes(void)
{
    uint32_t kit, l, silent = 0, loud = 0, endless = 0, total = 0, longest = 0;
    int32_t worst = 0;
    for (kit = 0; kit < DX_NKIT; kit++)
        for (l = 0; l < 8u; l++) {
            uint32_t freed;
            int32_t pk;
            drumx_track(kit);
            freed = strike(DRUM_LANE_NOTE[l], 100, MAXB);
            pk = peak_of(0, take_n);
            total++;
            silent += pk < 1036;                         /* -30 dBFS */
            loud += pk >= 32767;
            endless += freed == 0;
            if (pk > worst) worst = pk;
            if (freed > longest) longest = freed;
            if (pk < 1036 || pk >= 32767 || !freed)
                printf("drumx:   kit %u lane %u: peak %d, freed at block %u\n", kit, l, pk, freed);
        }
    printf("drumx:   %u hits, loudest peak %d, the longest freed after %.2f s\n", total, worst, (double)longest * CTL / 44100.0);
    check("every lane of every kit sounds (peak above -30 dBFS)", silent == 0);
    check("none reaches full scale", loud == 0);
    check("each ends: its voice frees itself within 10 s (808 BOOM and DUB ROOTS have kicks over 3 s, by design)", endless == 0);
}

/* ------------------------------------------------------------- 5. voices */
static void test_voices(void)
{
    int32_t out[2 * CTL];
    uint32_t b, voices_a, voices_b;
    double rms_open, rms_after;
    dxe_part_t *s;
    /* the closed hat chokes the open one */
    drumx_track(0);
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_HATO], 110);
    for (b = 0; b < 40u; b++) mix_block(out, CTL);
    s = dxe_part(0);
    check("an open hat rings after 40 blocks", s && s->l[DV_HATO].x.live);
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_HATC], 110);
    mix_block(out, CTL);
    check("the closed hat's strike sets the open hat's choke", s && (s->l[DV_HATO].x.choke || !s->l[DV_HATO].x.live));
    for (b = 0; b < 12u; b++) mix_block(out, CTL);
    printf("drumx:   open hat's amplitude envelope 13 blocks after the closed hat: %d of 2^30\n", s ? (int)s->l[DV_HATO].x.ea : -1);
    check("the open hat is 55 dB down 13 blocks (~9 ms) after the closed hat", s && s->l[DV_HATO].x.ea < (1 << 30) / 500);
    (void)rms_open; (void)rms_after;
    /* a lane's one voice is reused at another pitch (voice.c: dxe_reuse) */
    drumx_track(0);
    trk_note_on(&trk[0], 45, 100);                        /* a tom */
    mix_block(out, CTL);
    voices_a = 0;
    for (b = 0; b < NVOICE; b++) voices_a += trk[0].v[b].active;
    trk_note_on(&trk[0], 47, 100);                        /* the same lane, another pitch */
    mix_block(out, CTL);
    voices_b = 0;
    for (b = 0; b < NVOICE; b++) voices_b += trk[0].v[b].active;
    check("two toms at two pitches use one voice (the lane's)", voices_a == 1u && voices_b == 1u);
    /* eight lanes together: eight voices */
    drumx_track(1);
    for (b = 0; b < 8u; b++) trk_note_on(&trk[0], DRUM_LANE_NOTE[b], 100);
    mix_block(out, CTL);
    voices_a = 0;
    for (b = 0; b < NVOICE; b++) voices_a += trk[0].v[b].active;
    check("the eight lanes ring together (eight voices)", voices_a == 8u);
}

/* -------------------------------------------------------------- 6. LANES */
static double kick_rms(int lv0, int lv1)
{
    drumx_track(0);
    trk[0].p[P_LN0] = (int16_t)lv0;
    trk[0].p[P_LN1] = (int16_t)lv1;
    strike(DRUM_LANE_NOTE[DV_KICK], 100, 40u);
    return rms_of(0, take_n);
}
static double snare_rms(int lv0, int lv1)
{
    drumx_track(0);
    trk[0].p[P_LN0] = (int16_t)lv0;
    trk[0].p[P_LN1] = (int16_t)lv1;
    strike(DRUM_LANE_NOTE[DV_SNARE], 100, 40u);
    return rms_of(0, take_n);
}
static void test_levels(void)
{
    double full = kick_rms(127, 127), half = kick_rms(64, 127), none = kick_rms(0, 127), sn_full = snare_rms(127, 127);
    double sn_other = snare_rms(0, 127);
    printf("drumx:   kick RMS at LEVEL 127 / 64 / 0: %.0f %.0f %.0f; snare with the kick's level 0: %.0f (127: %.0f)\n", full, half, none, sn_other, sn_full);
    check("a lane's LEVEL 0 is silent", none < 1.0 && full > 500.0);
    check("LEVEL 64 is a quarter of the amplitude (square law, +-15 %)", half > full * 0.21 && half < full * 0.29);
    check("another lane's level does not move the snare", sn_other == sn_full);
}

/* ------------------------------------------------------------ 7. the knobs */
static uint64_t knob_hash(uint32_t note, uint32_t id, int v)
{
    drumx_track(0);
    if (id != 255u) trk[0].p[id] = (int16_t)v;
    strike(note, 100, 60u);
    return fnv64(take, take_n);
}
static void test_knobs(void)
{
    uint64_t base = knob_hash(DRUM_LANE_NOTE[DV_SNARE], 255, 0);
    static const struct { uint8_t id; int16_t v; const char *n; } K[] = {
        {P_E0, 0, "MRPH"}, {P_E1, 90, "TUNE"}, {P_E2, 30, "TONE"}, {P_E3, 100, "DECY"}, {P_E4, 110, "NOIS"}, {P_E5, 90, "FM"},
        {P_E6, 100, "FOLD"}, {P_E7, 100, "DRV"},
    };
    uint32_t i, moved = 0;
    for (i = 0; i < NELEM(K); i++) {
        uint64_t h = knob_hash(DRUM_LANE_NOTE[DV_SNARE], K[i].id, K[i].v);
        moved += h != base;
        if (h == base) printf("drumx:   %s did not move the snare\n", K[i].n);
    }
    check("each of MRPH TUNE TONE DECY NOIS FM FOLD DRV moves the snare", moved == NELEM(K));
    drumx_track(0);
    trk[0].p[P_E7] = 127;
    trk[0].p[P_E0] = 127;
    strike(DRUM_LANE_NOTE[DV_KICK], 127, 80u);
    check("DRV 127 on a loud kick stays under full scale", peak_of(0, take_n) < 32767);
    {
        int32_t pk0, pk1;
        double r0, r1;
        drumx_track(0);
        strike(DRUM_LANE_NOTE[DV_KICK], 127, 80u);
        pk0 = peak_of(0, take_n);
        r0 = rms_of(0, take_n);
        drumx_track(0);
        trk[0].p[P_E7] = 127;
        strike(DRUM_LANE_NOTE[DV_KICK], 127, 80u);
        pk1 = peak_of(0, take_n);
        r1 = rms_of(0, take_n);
        printf("drumx:   kick at velocity 127, DRV 0 / 127: peak %d / %d, RMS %.0f / %.0f\n", pk0, pk1, r0, r1);
        check("DRV 127 makes the kick denser: its RMS up by 5 % or more, its peak under full scale", r1 > r0 * 1.05 && pk1 < 32767);
    }
}

/* ------------------------------------------------------- 8. two kits at once */
static uint64_t part_hash(track_t *t, uint32_t blocks, int play_other)
{
    int32_t out[CTL];
    int16_t pcm[CTL * 64];
    uint32_t b, i, n = 0;
    trk_note_on(t, DRUM_LANE_NOTE[DV_KICK], 100);
    trk_note_on(t, DRUM_LANE_NOTE[DV_SNARE], 90);
    trk_note_on(t, DRUM_LANE_NOTE[DV_HATC], 80);
    if (play_other) {
        trk_note_on(&trk[1], DRUM_LANE_NOTE[DV_KICK], 100);
        trk_note_on(&trk[1], DRUM_LANE_NOTE[DV_CLAP], 100);
        trk_note_on(&trk[1], DRUM_LANE_NOTE[DV_TOM], 100);
    }
    for (b = 0; b < blocks && b < 64u; b++) {
        track_render(t, out, CTL);
        if (play_other) {
            int32_t o2[CTL];
            track_render(&trk[1], o2, CTL);
        }
        for (i = 0; i < CTL; i++) pcm[n++] = (int16_t)(out[i] > 32767 ? 32767 : out[i] < -32768 ? -32768 : out[i]);
    }
    return fnv64(pcm, n);
}
static void test_two_kits(void)
{
    uint64_t alone, together, other_alone;
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    host_preset(&trk[1], ENGI_DRUMX, 4);
    alone = part_hash(&trk[0], 40u, 0);
    other_alone = part_hash(&trk[1], 40u, 0);
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    host_preset(&trk[1], ENGI_DRUMX, 4);
    together = part_hash(&trk[0], 40u, 1);
    check("a DRUM-X track renders the same with another DRUM-X track (another kit) playing", alone == together);
    check("the two kits sound different", alone != other_alone);
}

/* ------------------------------------------------------------ 9. the grid */
static void clear_steps(track_t *t)
{
    for (uint32_t i = 0; i < NSTEP; i++) {
        memset(&t->step[i], 0, sizeof t->step[i]);
        t->step[i].time = ST_REST;
    }
}
static void run(uint32_t blocks)
{
    int32_t o[2 * CTL];
    ev_n = 0;
    ev_clock = 0;
    transport_req = 1;
    for (uint32_t b = 0; b < blocks; b++, ev_clock += CTL)
        mix_block(o, CTL);
    transport_req = 2;
    mix_block(o, CTL);
}
static void test_grid(void)
{
    uint32_t period, i, kick = 0, snare = 0, snare_acc = 0, tom = 0;
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    trk[0].p[P_SLEN] = 4;
    trk[0].p[P_SGATE] = 64;
    clear_steps(&trk[0]);
    period = seq_div_samples((uint32_t)trk[0].p[P_SDIV]);
    trk[0].step[0].time = ST_NOTE;
    trk[0].step[0].hit = (uint8_t)((1u << DV_KICK) | (1u << DV_SNARE));
    trk[0].step[0].acc = (uint8_t)(1u << DV_SNARE);
    trk[0].step[0].vel = 90;
    trk[0].step[1].time = ST_NOTE;
    trk[0].step[1].n = 1;
    trk[0].step[1].note[0] = 41;                          /* a note on the tom lane, not the grid's note 45 */
    trk[0].step[1].vel = 100;
    run(period / CTL + 8u);
    for (i = 0; i < ev_n; i++) {
        kick += ev[i].note == DRUM_LANE_NOTE[DV_KICK];
        snare += ev[i].note == DRUM_LANE_NOTE[DV_SNARE];
        snare_acc += ev[i].note == DRUM_LANE_NOTE[DV_SNARE] && ev[i].vel == 127;
        tom += ev[i].note == 41;
    }
    check("a step's hit lanes play their GM notes (kick 36, snare 38) on the first step", kick >= 1u && snare >= 1u);
    check("the accented lane plays at 127, the other at the step's velocity", snare_acc >= 1u && ev_n >= 3u);
    check("step_lanes names the lanes a step strikes, a note on a lane included", step_lanes(&trk[0].step[0]) == ((1u << DV_KICK) | (1u << DV_SNARE)) &&
          step_lanes(&trk[0].step[1]) == (1u << DV_TOM));
    check("the note on the tom lane (41) plays on the second step", tom >= 1u);
    check("step_accents agrees with the accent bit", step_accents(&trk[0].step[0]) == (1u << DV_SNARE));
}

/* ------------------------------------------------------------- 10. keys */
static void test_keys(void)
{
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    check("the first C (key 7) is the kick, 36", ENG_DRUMX.keys(&trk[0], 7) == 36);
    check("the keys go up in semitones: key 0 is 29, key 26 is 55", ENG_DRUMX.keys(&trk[0], 0) == 29 && ENG_DRUMX.keys(&trk[0], 26) == 55);
}

/* ----------------------------------------------------- 11. lane mutes */
static void test_mutes(void)
{
    int32_t out[2 * CTL];
    uint32_t b, freed, voices;
    int32_t kick, snare;
    drumx_track(0);
    check("no lane is muted at the start", dxe_muted(0, DV_KICK) == 0 && dxe_muted(0, DV_SNARE) == 0);
    check("the toggle reports the lane's new state, and back", dxe_mute_toggle(0, DV_KICK) == 1u && dxe_muted(0, DV_KICK) == 1u && dxe_mute_toggle(0, DV_KICK) == 0u);
    dxe_mute_toggle(0, DV_KICK);
    drum_flash[0] = 0;
    freed = strike(DRUM_LANE_NOTE[DV_KICK], 110, 40u);
    kick = peak_of(0, take_n);
    check("a muted kick makes no sound, flashes nothing and holds no voice", kick == 0 && freed == 1u && drum_flash[0] == 0);
    strike(DRUM_LANE_NOTE[DV_SNARE], 110, 40u);
    snare = peak_of(0, take_n);
    check("the snare beside it sounds as it did", snare > 1036 && (drum_flash[0] & (1u << DV_SNARE)));
    dxe_mute_toggle(0, DV_KICK);
    strike(DRUM_LANE_NOTE[DV_KICK], 110, 40u);
    check("the kick sounds again when the mute is off", peak_of(0, take_n) > 1036);
    /* a ringing open hat is cut by muting it and striking it again */
    drumx_track(0);
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_HATO], 110);
    for (b = 0; b < 10u; b++) mix_block(out, CTL);
    dxe_mute_toggle(0, DV_HATO);
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_HATO], 110);
    for (b = 0, voices = 0; b < 2u; b++) { mix_block(out, CTL); }
    for (b = 0; b < NVOICE; b++) voices += trk[0].v[b].active;
    check("a muted lane struck again cuts its ringing voice", voices == 0u);
    /* per part: the mask of part 1 is its own */
    drumx_track(0);
    dxe_mute_toggle(1, DV_KICK);
    check("the mute belongs to the part: part 0's kick is not muted by part 1's", dxe_muted(0, DV_KICK) == 0u && dxe_muted(1, DV_KICK) == 1u);
    dxe_mute_toggle(0, DV_TOM);
    dxe_mute_clear();
    check("the clear (a project load) puts every lane of every part back", dxe_muted(0, DV_TOM) == 0u && dxe_muted(1, DV_KICK) == 0u);
    check("the mask is runtime state: four bytes, outside the track record", sizeof dxe_mute == NTRK);
}

#ifdef DXK_MARK
/* ------------------------------------------- 12. the kit's defaults (0.14.2) */
static uint64_t lane_hash(uint32_t kit, uint32_t lane, void (*set)(dxk_t *, uint32_t), uint32_t blocks)
{
    drumx_track(kit);
    if (set) set(dxk_take(0), lane);
    strike(DRUM_LANE_NOTE[lane], 100, blocks);
    return fnv64(take, take_n);
}
static void test_kit_defaults(void)
{
    uint32_t k, l, same = 0, n = 0;
    for (k = 0; k < DX_NKIT; k++)
        for (l = 0; l < 8u; l++) {
            uint64_t a, b;
            drumx_track(k);
            strike(DRUM_LANE_NOTE[l], 100, 48u);
            a = fnv64(take, take_n);
            drumx_track(k);
            (void)dxk_take(0);
            strike(DRUM_LANE_NOTE[l], 100, 48u);
            b = fnv64(take, take_n);
            n++;
            same += a == b;
            if (a != b) printf("drumx:   kit %u lane %u: the expanded kit differs\n", k, l);
        }
    drumx_track(3);
    {
        dxk_t *K = dxk_take(0);
        check("the expanded kit is the factory kit's: its base, its LAYERs, every other value 0", K->on && K->base == 3u &&
              !memcmp(K->lay, DX_KITS[3].lay, 8) && !K->tone && !K->hands && !K->duck && !K->tune[0] && !K->pan[7] && !K->bend[4] && !K->punch[2] && !K->atk[1]);
    }
    check("every lane of the nine kits, expanded at the defaults, renders as the factory kit (72 hits, sample for sample)", same == n && n == 72u);
    drumx_track(2);
    (void)dxk_take(0);
    host_preset(&trk[0], ENGI_DRUMX, 5);
    check("a factory preset's load puts the factory kit back (on 0)", dxk[0].on == 0 && dxe_kit(&trk[0]) == &DX_KITS[5]);
}

/* ---------------------------------------- 13. each lane value (0.14.2) */
static void v_tune(dxk_t *K, uint32_t l) { K->tune[l] = 40; }
static void v_tune_dn(dxk_t *K, uint32_t l) { K->tune[l] = -64; }
static void v_decay(dxk_t *K, uint32_t l) { K->decay[l] = -40; }
static void v_layer(dxk_t *K, uint32_t l) { K->lay[l] = (uint8_t)((K->lay[l] & 0xF0u) | 12u); }
static void v_fdbk(dxk_t *K, uint32_t l) { K->lay[l] = (uint8_t)((K->lay[l] & 15u) | 9u << 4); }
static void v_punch(dxk_t *K, uint32_t l) { K->punch[l] = 15; }
static void v_atk(dxk_t *K, uint32_t l) { K->atk[l] = 12; }
static void v_bend(dxk_t *K, uint32_t l) { K->bend[l] = -12; }
static void v_tone(dxk_t *K, uint32_t l) { (void)l; K->tone = 9; }
static void v_hands(dxk_t *K, uint32_t l) { (void)l; K->hands = 12; }
static uint32_t crossings(uint32_t from, uint32_t to)
{
    uint32_t i, c = 0;
    for (i = from + 1u; i < to && i < take_n; i++) c += (take[i - 1] < 0) != (take[i] < 0);
    return c;
}
static void test_lane_values(void)
{
    static const struct { void (*set)(dxk_t *, uint32_t); const char *n; } V[] = {
        {v_tune, "TUNE"}, {v_decay, "DECAY"}, {v_layer, "LAYER"}, {v_fdbk, "FDBK"}, {v_punch, "PUNCH"}, {v_atk, "ATK+"}, {v_bend, "BEND"},
    };
    uint32_t i, own = 0, other = 0;
    for (i = 0; i < NELEM(V); i++) {
        uint64_t b0 = lane_hash(0, DV_SNARE, 0, 60u), b1 = lane_hash(0, DV_SNARE, V[i].set, 60u);
        uint64_t k0 = lane_hash(0, DV_KICK, 0, 60u), k1;
        drumx_track(0);
        V[i].set(dxk_take(0), DV_SNARE);
        strike(DRUM_LANE_NOTE[DV_KICK], 100, 60u);
        k1 = fnv64(take, take_n);
        own += b0 != b1;
        other += k0 == k1;
        if (b0 == b1) printf("drumx:   %s did not move the snare\n", V[i].n);
        if (k0 != k1) printf("drumx:   %s on the snare moved the kick\n", V[i].n);
    }
    check("each of TUNE DECAY LAYER FDBK PUNCH ATK+ BEND moves its lane (the snare)", own == NELEM(V));
    check(".. and not another lane (the kick)", other == NELEM(V));
    check("TONE (BP) moves the closed hat, not the kick", lane_hash(0, DV_HATC, v_tone, 60u) != lane_hash(0, DV_HATC, 0, 60u) &&
          lane_hash(0, DV_KICK, v_tone, 60u) == lane_hash(0, DV_KICK, 0, 60u));
    check("HANDS moves the clap, not the snare", lane_hash(0, DV_CLAP, v_hands, 120u) != lane_hash(0, DV_CLAP, 0, 120u) &&
          lane_hash(0, DV_SNARE, v_hands, 120u) == lane_hash(0, DV_SNARE, 0, 120u));
    {
        uint32_t c0, c1, c2, f0, f1;
        double r0, r1;
        int32_t a0, a1;
        drumx_track(0); strike(DRUM_LANE_NOTE[DV_TOM], 100, 40u); c0 = crossings(0, take_n);
        drumx_track(0); v_tune(dxk_take(0), DV_TOM); strike(DRUM_LANE_NOTE[DV_TOM], 100, 40u); c1 = crossings(0, take_n);
        drumx_track(0); v_tune_dn(dxk_take(0), DV_TOM); strike(DRUM_LANE_NOTE[DV_TOM], 100, 40u); c2 = crossings(0, take_n);
        printf("drumx:   the tom's zero crossings in 40 blocks: TUNE 0 %u, +7.5 st %u, -12 st %u\n", c0, c1, c2);
        check("TUNE up raises the lane's pitch, down lowers it", c1 > c0 + c0 / 4u && c2 < c0 - c0 / 4u);
        drumx_track(0); f0 = strike(DRUM_LANE_NOTE[DV_SNARE], 100, MAXB);
        drumx_track(0); v_decay(dxk_take(0), DV_SNARE); f1 = strike(DRUM_LANE_NOTE[DV_SNARE], 100, MAXB);
        printf("drumx:   the snare ends after %u blocks, %u with DECAY -40\n", f0, f1);
        check("DECAY down ends the hit sooner", f1 && f0 && f1 < f0 * 3u / 4u);
        drumx_track(0); strike(DRUM_LANE_NOTE[DV_KICK], 100, 40u); a0 = peak_of(0, 4u * CTL); r0 = rms_of(0, take_n);
        drumx_track(0); v_atk(dxk_take(0), DV_KICK); strike(DRUM_LANE_NOTE[DV_KICK], 100, 40u); a1 = peak_of(0, 4u * CTL);
        printf("drumx:   the kick's peak over its first 4 blocks: %d, with ATK+ 12 %d\n", a0, a1);
        check("ATK+ fades the hit in (its first blocks softer)", a1 < a0 / 2);
        drumx_track(0); v_punch(dxk_take(0), DV_KICK); strike(DRUM_LANE_NOTE[DV_KICK], 100, 40u); r1 = rms_of(0, take_n);
        printf("drumx:   the kick's RMS over 40 blocks: %.0f, with PUNCH 15 %.0f (peak %d)\n", r0, r1, peak_of(0, take_n));
        check("PUNCH makes the kick's body louder, under full scale", r1 > r0 * 1.1 && peak_of(0, take_n) < 32767);
        /* (808 BOOM's kick rings for seconds; a step of the track at 120 BPM and 1/16 is ~172 blocks) */
        drumx_track(2); strike(DRUM_LANE_NOTE[DV_KICK], 100, 400u); c0 = crossings(200u * CTL, 400u * CTL); f0 = crossings(0, 20u * CTL);
        drumx_track(2); v_bend(dxk_take(0), DV_KICK); strike(DRUM_LANE_NOTE[DV_KICK], 100, 400u); c1 = crossings(200u * CTL, 400u * CTL);
        c2 = crossings(0, 20u * CTL);
        printf("drumx:   808 BOOM's kick, zero crossings in blocks 200..399: BEND 0 %u, BEND -12 %u (its first 20 blocks: %u, %u)\n", c0, c1, f0, c2);
        check("BEND -12 takes the kick an octave down over a step (half the crossings, +-15 %), its start where it was", c1 * 100u > c0 * 35u && c1 * 100u < c0 * 65u &&
              c2 * 10u > f0 * 8u);
    }
}

/* -------------------------------------------------- 14. PAN (0.14.2) */
static int32_t side_l[CTL * 64], side_r[CTL * 64];
static void sides_take(uint32_t blocks)            /* track 0 through the mixer (mix_part): its left and right */
{
    uint32_t b, i;
    for (b = 0; b < blocks && b < 64u; b++) {
        memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
        memset(send_c, 0, sizeof send_c); memset(send_d, 0, sizeof send_d); memset(send_r, 0, sizeof send_r);
        mix_part(&trk[0], CTL);
        for (i = 0; i < CTL; i++) { side_l[b * CTL + i] = mix_l[i]; side_r[b * CTL + i] = mix_r[i]; }
    }
}
static double sum_abs(const int32_t *x, uint32_t n) { double s = 0; for (uint32_t i = 0; i < n; i++) s += x[i] < 0 ? -x[i] : x[i]; return s; }
static void pan_kick(int32_t lane_pan, int32_t track_pan, uint32_t lane, double *l, double *r)
{
    drumx_track(0);
    trk[0].p[P_PAN] = (int16_t)track_pan;
    dxk_take(0)->pan[lane] = (int8_t)lane_pan;
    trk_note_on(&trk[0], DRUM_LANE_NOTE[lane], 100);
    sides_take(40u);
    *l = sum_abs(side_l, 40u * CTL);
    *r = sum_abs(side_r, 40u * CTL);
}
static void test_pan(void)
{
    double cl, cr, ll, lr, hl, hr, ol, orr, tl, tr2, sl, sr;
    pan_kick(0, 0, DV_KICK, &cl, &cr);
    pan_kick(-64, 0, DV_KICK, &ll, &lr);
    pan_kick(32, 0, DV_KICK, &hl, &hr);
    printf("drumx:   the kick, |L| / |R|: centre %.0f / %.0f, hard left %.0f / %.0f, half right %.0f / %.0f\n", cl, cr, ll, lr, hl, hr);
    check("a lane at the centre is the same on both sides", cl == cr && cl > 1e5);
    check("hard left: silent on the right, on the left within 1 % of the centre's", lr < cl * 1e-3 && ll > cl * 0.99 && ll < cl * 1.01);
    check("half right (32): the left halved (+-5 %), the right as at the centre", hl > cl * 0.45 && hl < cl * 0.55 && hr > cr * 0.99 && hr < cr * 1.01);
    /* the other lanes stay at the centre: the snare, with the kick panned */
    drumx_track(0);
    dxk_take(0)->pan[DV_KICK] = -64;
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_SNARE], 100);
    sides_take(40u);
    ol = sum_abs(side_l, 40u * CTL); orr = sum_abs(side_r, 40u * CTL);
    check("with the kick panned, the snare stays at the centre", ol == orr && ol > 1e4);
    /* the track's PAN after the lane's: a lane hard left on a track hard right is (nearly) silent */
    pan_kick(-64, 63, DV_KICK, &tl, &tr2);
    printf("drumx:   the kick hard left on a track hard right: |L| %.0f |R| %.0f\n", tl, tr2);
    check("the track's PAN applies after the lane's (hard left on a track hard right: under 2 % of the centre)", tl + tr2 < cl * 0.02);
    /* DIST on both sides: the right stays silent */
    drumx_track(0);
    trk[0].p[P_DIST] = 90;
    dxk_take(0)->pan[DV_KICK] = -64;
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 100);
    sides_take(40u);
    sl = sum_abs(side_l, 40u * CTL); sr = sum_abs(side_r, 40u * CTL);
    check("DIST takes each side: a lane hard left stays silent on the right", sr < sl * 1e-3 && sl > 1e5);
    /* SPREAD is off on a panned kit (its lanes are where their PAN puts them) */
    drumx_track(0);
    trk[0].p[P_SPRD] = 100;
    dxk_take(0)->pan[DV_KICK] = -64;
    for (uint32_t l = 0; l < 8u; l++) trk_note_on(&trk[0], DRUM_LANE_NOTE[l], 100);
    sides_take(1u);
    check("SPREAD is off on a kit with a lane panned (the sides are the lanes')", !spread_on && dxe_right(&trk[0]) != 0);
    /* no lane panned: the path is the mono one (the sides not used) */
    drumx_track(0);
    (void)dxk_take(0);
    trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 100);
    sides_take(2u);
    check("no lane panned: one side (the mixer's path of before)", dxe_right(&trk[0]) == 0 && !dxk_panned(&trk[0]));
}

/* ------------------------------------------------- 15. DUCK (0.14.2) */
#define DUCK_N 1600u                                /* (1.16 s) */
static double duck_e[DUCK_N];
static void duck_run(uint32_t duck, int mute_kick, uint32_t other_engine)   /* track 1's level a block (duck_e), a kick at block 40 */
{
    uint32_t b, i;
    fresh();
    host_preset(&trk[0], ENGI_DRUMX, 0);
    host_preset(&trk[1], other_engine, 0);
    trk[1].p[P_ATK] = 0; trk[1].p[P_SUS] = 127;         /* (a held note at its level) */
    dxk_take(0)->duck = (uint8_t)duck;
    trk_note_on(&trk[1], 60, 100);
    dxe_duck_env = 0; dxe_duck_att = dxe_duck_hold = 0; dxe_duck_g0 = dxe_duck_g1 = 32767; dxe_duck_req = 0;
    if (mute_kick) dxe_mute_toggle(0, DV_KICK);
    for (b = 0; b < DUCK_N; b++) {
        double e = 0;
        if (b == 40u) trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 110);
        dxe_duck_block();
        memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
        mix_part(&trk[1], CTL);
        for (i = 0; i < CTL; i++) e += mix_l[i] < 0 ? -mix_l[i] : mix_l[i];
        duck_e[b] = e;
    }
    if (mute_kick) dxe_mute_toggle(0, DV_KICK);
}
/* the lowest of track 1's level over blocks 41..99 against the same run with no DUCK, and its last 20 blocks' against the same's */
static void duck_vs(uint32_t duck, int mute_kick, uint32_t other_engine, double *dip, double *after)
{
    double ref[DUCK_N], lo = 1e30, a = 0, ar = 0;
    uint32_t b;
    duck_run(0, 0, other_engine);
    memcpy(ref, duck_e, sizeof ref);
    duck_run(duck, mute_kick, other_engine);
    for (b = 41u; b < 100u; b++)
        if (ref[b] > 0 && duck_e[b] / ref[b] < lo) lo = duck_e[b] / ref[b];
    for (b = DUCK_N - 20u; b < DUCK_N; b++) { a += duck_e[b]; ar += ref[b]; }
    *dip = lo;
    *after = ar > 0 ? a / ar : 0;
}
static void test_duck(void)
{
    double r100, a100, rm, am, rd, ad, r30, a30;
    duck_vs(100, 0, ENGI_MACRO, &r100, &a100);
    duck_vs(30, 0, ENGI_MACRO, &r30, &a30);
    duck_vs(100, 1, ENGI_MACRO, &rm, &am);
    duck_vs(100, 0, ENGI_DRUMX, &rd, &ad);
    printf("drumx:   another part after a kick, of the same with no DUCK: DUCK 100 down to %.3f, back to %.3f; DUCK 30 %.3f / %.3f; a muted kick %.3f; "
           "a DRUM-X part %.3f\n", r100, a100, r30, a30, rm, rd);
    check("DUCK 100: a kick takes another part 20 dB down or more", r100 < 0.1);
    check(".. and lets it back (within 1 %, 1.1 s on)", a100 > 0.99 && a100 < 1.01);
    check("DUCK 30 less deep than DUCK 100", r30 > r100 * 3 && r30 < 0.9);
    check("a muted kick does not duck", rm > 0.999);
    check("a drum track is not ducked", rd > 0.999);
}

/* -------------------------------------------- 16. the kit's record (0.14.2) */
static void test_kit_record(void)
{
    dxk_t a, b;
    uint8_t r[DXK_ROOM], r2[DXK_ROOM];
    uint32_t i;
    memset(&a, 0, sizeof a);
    a.on = 1; a.base = 6; a.tone = 0x93; a.hands = 7; a.duck = 85;
    for (i = 0; i < 8u; i++) {
        a.tune[i] = (int8_t)(i * 17 - 64); a.decay[i] = (int8_t)(63 - (int)i * 9); a.pan[i] = (int8_t)((int)i * 18 - 64);
        a.bend[i] = (int8_t)((int)i * 3 - 12); a.lay[i] = (uint8_t)(i * 37); a.punch[i] = (uint8_t)(15 - i); a.atk[i] = (uint8_t)(i * 2);
    }
    memset(r, 0, sizeof r);
    dxk_pack(&a, r);
    memset(&b, 0x55, sizeof b);
    check("the record is 56 bytes: 'X', version 1, a body of 52 and its CRC-8", DXK_BYTES == 56u && r[0] == 'X' && r[1] == 1u && r[2] == 52u && r[3] == dxk_crc(r + 4, 52));
    check("a kit round-trips through its record, value for value", dxk_unpack(&b, r, sizeof r) && !memcmp(&a, &b, sizeof a));
    dxk_pack(&b, r2);
    check(".. and its record again byte for byte", !memcmp(r, r2, DXK_BYTES));
    memcpy(r2, r, sizeof r); r2[0] = 0;
    check("no mark (a preset of before 0.14.2): no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[1] = 2;
    check("another version: no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[4 + 20] ^= 1u;
    check("a byte changed (the CRC): no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[2] = 51;
    check("a body shorter than version 1's: no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[4 + 0] = 9; r2[3] = dxk_crc(r2 + 4, 52);
    check("a base past the nine kits: no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[4 + 44] = 13; r2[3] = dxk_crc(r2 + 4, 52);
    check("a BEND past 12: no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[4 + 3] = 101; r2[3] = dxk_crc(r2 + 4, 52);
    check("a DUCK past 100: no kit", !dxk_unpack(&b, r2, sizeof r2));
    memcpy(r2, r, sizeof r); r2[2] = 59; r2[56] = 1; r2[57] = 2; r2[58] = 3; r2[59] = 4; r2[60] = 5; r2[61] = 6; r2[62] = 7; r2[3] = dxk_crc(r2 + 4, 59);
    memset(&b, 0, sizeof b);
    check("a longer body (a later firmware's, 59 bytes): the fields this one knows, the rest left", dxk_unpack(&b, r2, sizeof r2) && !memcmp(&a, &b, sizeof a));
    check("a record cut short of its body (the room): no kit", !dxk_unpack(&b, r, DXK_BYTES - 1u));
}

/* -------------------------------------------------- 17. the cost (0.14.2) */
#endif
static double bench_now(void) { struct timespec ts; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts); return ts.tv_sec * 1e9 + ts.tv_nsec; }
/* ns per sample: a 16-step loop (all eight lanes on the first, kick and hat on every 4th, hats between) through mix_part, 4000 blocks; the
 * fastest of 7 runs. mode 0 the factory kit; (0.14.2) 1 every lane panned and tuned, 2 every value in use */
static double bench_run(int mode)
{
    double best = 1e30;
    for (int run = 0; run < 7; run++) {
        double t0, t1;
        drumx_track(0);
#ifdef DXK_MARK
        if (mode) {
            dxk_t *K = dxk_take(0);
            for (uint32_t l = 0; l < 8u; l++) {
                K->pan[l] = (int8_t)((int)l * 16 - 56);
                K->tune[l] = (int8_t)((int)l * 5 - 20);
                if (mode == 2) { K->decay[l] = -10; K->lay[l] = 0x37; K->punch[l] = 9; K->atk[l] = 3; K->bend[l] = (int8_t)((int)l - 4); }
            }
            if (mode == 2) { K->tone = 0x55; K->hands = 9; K->duck = 60; }
        }
#else
        if (mode) return 0;
#endif
        t0 = bench_now();
        for (uint32_t b = 0; b < 4000u; b++) {
            uint32_t step = b / 8u;
            if (b % 8u == 0u) {
                if (step % 16u == 0u) for (uint32_t l = 0; l < 8u; l++) trk_note_on(&trk[0], DRUM_LANE_NOTE[l], 100);
                else if (step % 4u == 0u) { trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_KICK], 100); trk_note_on(&trk[0], DRUM_LANE_NOTE[DV_HATC], 90); }
                else trk_note_on(&trk[0], DRUM_LANE_NOTE[step % 2u ? DV_HATC : DV_HATO], 70);
            }
            memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
            memset(send_c, 0, sizeof send_c); memset(send_d, 0, sizeof send_d); memset(send_r, 0, sizeof send_r);
#ifdef DXK_MARK
            dxe_duck_block();
#endif
            mix_part(&trk[0], CTL);
        }
        t1 = bench_now();
        if (t1 - t0 < best) best = t1 - t0;
    }
    return best / (4000.0 * CTL);
}
static void test_cost(void)
{
    double d = bench_run(0), p = bench_run(1), a = bench_run(2);
    printf("drumx:   cost, ns per sample of a busy kit through the mixer (fastest of 7): factory kit %.1f", d);
    if (p > 0) printf(", lanes panned and tuned %.1f (%+.0f %%), every value in use %.1f (%+.0f %%)", p, (p / d - 1) * 100, a, (a / d - 1) * 100);
    printf("\n");
}

int main(int argc, char **argv)
{
    test_reference(argc > 1 ? argv[1] : "tests/drumx_ref.txt");
    test_registration();
    test_gm_map();
    test_lanes();
    test_voices();
    test_levels();
    test_knobs();
    test_two_kits();
    test_grid();
    test_keys();
    test_mutes();
#ifdef DXK_MARK
    test_kit_defaults();
    test_lane_values();
    test_pan();
    test_duck();
    test_kit_record();
#endif
    test_cost();
    printf("drumx: sizes: dx_voice_t %u B, a lane %u B, the part (arena, per DRUM-X track) %u B, DX_KITS %u B (flash)\n",
           (unsigned)sizeof(dx_voice_t), (unsigned)sizeof(dxe_lane_t), (unsigned)sizeof(dxe_part_t), (unsigned)sizeof DX_KITS);
    printf(bad ? "drumx: %d FAILED\n" : "drumx: all checks ok\n", bad);
    return bad != 0;
}
