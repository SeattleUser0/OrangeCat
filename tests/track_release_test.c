/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat round 23: the automatic release of a track's engine state (firmware/src/seq.c trk_wanted and eng_idle_check, voice.c eng_held; BUILD.md 7.31,
 * "Track OFF: the design", item 1), through the whole mix as the firmware runs it, on an arena of the unit's size.
 *   BYTES    four MACRO tracks, nothing to play: the three not selected give their states back after the check (a second, stopped); the arena's bytes
 *   HOLD     a track is never released with a voice active or fading, a note or a hit in its playing pattern, a staged pattern with notes (or one
 *            not read yet), armed, selected, a key held, an arp note, the old SONG chain running, an engine with a block hook (DRUM-X)
 *   TAIL     a held note keeps the state; released, the state goes only after the voice has ended
 *   RETAKE   at the track's selection, at arming, when a pattern with notes is staged (before its launch: the launch's notes sound, none taken at
 *            the note), when a step is written; a staged empty pattern takes nothing
 *   NOTE     a note nothing announced (MIDI on the track's channel) sounds at once, and the track is kept from then on
 *   FAIL     the arena full: the retake fails, the note waits, the effects' blocks are given back and the note plays; a block that does not yield
 *            leaves the track silent until it is freed
 *   SAME     a project rendered with the release and without it: the same samples
 * Run by tests/run_tests.sh. */
#define RES_HOST_MAIN_WORDS (85760u / 4u)              /* the device's banks (tests/jfx_arena_test.c) */
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SEC (FS / CTL + 2u)                            /* blocks: a little over a second (the check stopped) */
static int32_t blk[CTL * 2];
static void run(uint32_t n) { while (n--) mix_block(blk, CTL); }
static uint32_t held(uint32_t k) { return resource[RES_ENGINE0 + k].size; }
static uint32_t sounding(uint32_t k) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[k].v[i].active; return n; }
static void put(track_t *t, uint32_t i, uint32_t note)
{
    t->step[i].note[0] = (uint8_t)note; t->step[i].n = 1; t->step[i].time = ST_NOTE; t->step[i].vel = 100;
}
/* four MACRO tracks (ORGAN DRAWBAR, POLY), empty patterns, track 1 selected, stopped; the counts from zero */
static void fresh(void)
{
    ui_power_on();
    resource_host_cache_enabled = 0; res_lend_ok = 0;
    song.master_q12 = 4096; song.g[G_BPM] = 120; song.playing = 0; song.sel = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        eng_state_clear(k);
        set_engine_of(&trk[k], ENGI_MACRO); apply_preset_to(&trk[k], 8); trk[k].engine = trk[k].eng_req;
        track_defaults_steps(&trk[k]);
        trk[k].p[P_SLEN] = 16;
    }
    memset(eng_rel, 0, sizeof eng_rel); memset(eng_keep, 0, sizeof eng_keep);
    eng_releases = eng_release_bytes = eng_retakes = eng_note_takes = 0; eng_idle_left = FS;
    run(4);
}
/* bank b of track k: one note on step 0, read into the staging buffer and staged (as the main loop does: motion.c pattern_request, oc_lists.c) */
static void stage_bank(uint32_t k, uint32_t b, int notes, int prepare)
{
    pattern_t p;
    pattern_read(k, b, &p);
    if (notes) { p.step[0].note[0] = 60; p.step[0].n = 1; p.step[0].time = ST_NOTE; p.step[0].vel = 100; }
    pattern_write(k, b, &p);
    if (prepare) pattern_prepare(k, b);
    trk[k].pattern_next = (uint8_t)b;
}

#define SAME_N 6000u                                    /* 4.4 s */
static int32_t same[2][SAME_N][CTL * 2];
static void same_render(uint32_t pass)
{
    fresh();
    if (pass) memset(eng_keep, 1, sizeof eng_keep);     /* (pass 1: no track is ever released) */
    for (uint32_t i = 0; i < 16u; i += 4u) put(&trk[0], i, 48u + i);
    for (uint32_t i = 2; i < 16u; i += 4u) put(&trk[1], i, 60u + i);
    transport_req = 1;
    for (uint32_t b = 0; b < SAME_N; b++) {
        if (b == 3000u) stage_bank(3, 1, 1, 1);              /* track 4: empty, then a pattern with a note comes in at its loop end */
        if (b == 4000u) song.sel = 2;                   /* track 3 selected: a key */
        if (b == 4100u) trk_note_on(&trk[2], 72, 100);
        if (b == 4500u) trk_note_off(&trk[2], 72);
        mix_block(same[pass][b], CTL);
    }
}

int main(void)
{
    uint32_t mc = (eng_state_size(ENGI_MACRO) + 3u) & ~3u, used0, k, b;

    /* BYTES */
    fresh();
    for (k = 0; k < NPART; k++) CHECK(held(k) == mc, "track %u should hold its state before the check: %u", k + 1u, held(k));
    used0 = resource_used();
    run(SEC - 40u);
    CHECK(!eng_releases && resource_used() == used0, "released before a second passed: %u", eng_releases);
    run(40);
    CHECK(held(0) == mc && !eng_rel[0], "the selected track was released");
    for (k = 1; k < NPART; k++) CHECK(!held(k) && eng_rel[k], "track %u, nothing to play, should be released: %u B held", k + 1u, held(k));
    CHECK(eng_releases == 3u && eng_release_bytes == 3u * mc && used0 - resource_used() == 3u * mc, "the arena's bytes: %u released, %u back", eng_release_bytes, used0 - resource_used());
    printf("track release: BYTES: a MACRO state %u B (SHAPES %u B, DRUM-X %u B); four idle MACRO tracks %u B, after the check %u B: %u B back, %u B a track\n",
           mc, (eng_state_size(ENGI_SHAPES) + 3u) & ~3u, (eng_state_size(ENGI_DRUMX) + 3u) & ~3u, used0, resource_used(), used0 - resource_used(), (used0 - resource_used()) / 3u);
    CHECK(mc >= 10000u, "a MACRO state should be about 11 KB: %u", mc);
    run(2u * SEC);
    CHECK(eng_releases == 3u && !eng_retakes && held(0) == mc && !held(1), "the released tracks should stay released, the selected one held");

    /* HOLD: each condition alone on track 2, two checks stopped (and two bars playing, where it applies) */
    {
        static const char *const what[] = {"a note in its playing pattern", "a hit in its playing pattern", "a recorded view in its playing pattern", "a staged pattern with a note",
            "a staged pattern not read yet", "armed", "armed and waiting", "the record latch", "a key held", "an arp note", "keys held for the arp", "a voice fading (stolen)",
            "a note waiting for the state", "an engine switch asked", "the old SONG chain running", "the old SONG chain armed", "an audition asked"};
        for (uint32_t c = 0; c < sizeof what / sizeof what[0]; c++) {
            fresh();
            track_t *t = &trk[1];
            switch (c) {
            case 0: put(t, 5, 60); break;
            case 1: t->step[9].hit = 1; t->step[9].time = ST_NOTE; break;
            case 2: t->step[3].flags |= SF_RECORDED; break;
            case 3: stage_bank(1, 2, 1, 1); break;
            case 4: stage_bank(1, 2, 0, 0); break;
            case 5: song.rec |= 2u; break;
            case 6: song.rec_wait |= 2u; break;
            case 7: song.rec_latch |= 2u; break;
            case 8: live_held[1][1] |= 1u << 28; break;
            case 9: t->arp_note = 60; break;
            case 10: t->arp_phys = 1; break;
            case 11: t->v[2].active = 1; t->v[2].stage = 4; break;
            case 12: t->xp_n = 1; t->xp_note[0] = 60; t->xp_vel[0] = 0; break;
            case 13: t->eng_req = ENGI_DRUMX; break;
            case 14: chain.running = 1; break;
            case 15: chain.armed = 1; break;
            default: audition_request = 1; audition_track = 1; break;
            }
            CHECK(trk_wanted(t), "HOLD: %s: the track should be wanted", what[c]);
            if (c == 9 || c == 11 || c == 12 || c == 13 || c >= 14) continue;   /* (states the next block consumes: the rule is checked, above) */
            run(2u * SEC);
            CHECK(held(1) == mc && !eng_rel[1], "HOLD: %s: track 2 was released", what[c]);
            CHECK(!held(2) && !held(3), "HOLD: %s: tracks 3 and 4 have nothing to play", what[c]);
        }
        fresh();
        CHECK(!trk_wanted(&trk[1]) && trk_wanted(&trk[0]), "an idle track not selected is not wanted, the selected one is");
        trk[1].eng_req = ENGI_DRUMX;
        CHECK(trk_wanted(&trk[1]), "HOLD: an engine switch asked: wanted");
        run(2u * SEC);
        CHECK(trk[1].engine == ENGI_DRUMX && held(1) && !eng_rel[1], "HOLD: an engine with a block hook (DRUM-X) keeps its state: %u B", held(1));
        fresh();
        song.sel = 1; run(2u * SEC);
        CHECK(held(1) == mc && !held(0), "HOLD: selected: track 2 held, track 1 (not selected now, nothing to play) released");
        printf("track release: HOLD: %u conditions, each alone, keep the state; DRUM-X keeps its own\n", (unsigned)(sizeof what / sizeof what[0]) + 2u);
    }

    /* TAIL: a note held on track 2 by a caller that is not a key (the state is held: no note take); released, the state stays until the voice has ended */
    fresh();
    trk_note_on(&trk[1], 60, 100);
    run(3u * SEC);
    CHECK(sounding(1) && held(1) == mc && !eng_note_takes, "TAIL: a held note should keep the state");
    trk_note_off(&trk[1], 60);
    for (b = 0; b < 8u * SEC && sounding(1); b++) { run(1); if (held(1) != mc) break; }
    CHECK(held(1) == mc, "TAIL: released under a sounding voice (block %u)", b);
    CHECK(!sounding(1), "TAIL: the voice did not end");
    run(SEC + 2u);
    CHECK(!held(1) && eng_rel[1], "TAIL: the voice ended a second ago: the state should be back");

    /* RETAKE */
    fresh(); run(SEC);
    CHECK(eng_rel[2] && eng_rel[3] && eng_rel[1], "RETAKE: three tracks released first");
    song.sel = 2; run(1);
    CHECK(held(2) == mc && !eng_rel[2] && eng_retakes == 1u, "RETAKE: at the track's selection, the next block");
    trk_note_on(&trk[2], 64, 100);
    CHECK(sounding(2) == 1u && !trk[2].xp_n && !eng_note_takes, "RETAKE: the selected track's key sounds at once, nothing taken at the note");
    trk_note_off(&trk[2], 64);
    song.rec |= 8u; run(1);
    CHECK(held(3) == mc && !eng_rel[3] && eng_retakes == 2u, "RETAKE: at arming, the next block");
    song.rec = 0;
    put(&trk[1], 0, 55); run(1);
    CHECK(held(1) == mc && eng_retakes == 3u, "RETAKE: a step written: the next block");
    run(8u * SEC);
    CHECK(!held(0) && !held(3) && held(1) == mc && held(2) == mc, "RETAKE: then track 1 (not selected, empty) and track 4 (not armed) go, 2 and 3 stay");
    printf("track release: RETAKE: selection, arming and a written step take the state a block later: %u retakes, %u at a note\n", eng_retakes, eng_note_takes);

    fresh(); run(SEC);
    transport_req = 1; run(8);
    CHECK(song.playing && eng_rel[1] && eng_rel[3], "RETAKE: playing, tracks 2 and 4 released");
    stage_bank(3, 1, 0, 1);                                  /* an empty pattern staged for track 4: nothing taken, at the launch neither */
    stage_bank(1, 1, 1, 1);                                  /* a pattern with a note for track 2 */
    run(1);
    CHECK(held(1) == mc && !eng_rel[1] && trk[1].pattern == 0, "RETAKE: a staged pattern with a note: the state taken the next block, before the launch");
    CHECK(!held(3) && eng_rel[3], "RETAKE: a staged empty pattern takes nothing");
    {
        uint32_t heard = 0, launched = 0;
        for (b = 0; b < 6000u; b++) {                   /* (the loop is 16 steps: 2 s) */
            run(1);
            heard |= sounding(1);
            launched |= trk[1].pattern == 1u;
        }
        CHECK(launched && heard, "RETAKE: the staged pattern came in and its note sounded (%u, %u)", launched, heard);
        CHECK(trk[3].pattern == 1u && !held(3), "RETAKE: track 4's empty pattern came in with no state taken");
        CHECK(!eng_note_takes && !eng_keep[1], "RETAKE: no state was taken at a note: %u", eng_note_takes);
    }
    transport_req = 2; run(4);
    printf("track release: RETAKE: a staged pattern with a note takes the state before its launch, an empty one takes none: %u retakes, %u at a note\n", eng_retakes, eng_note_takes);

    /* NOTE: a note nothing announced */
    fresh(); run(SEC);
    CHECK(eng_rel[3], "NOTE: track 4 released first");
    trk_note_on(&trk[3], 67, 100);
    CHECK(sounding(3) == 1u && !trk[3].xp_n && held(3) == mc, "NOTE: the note should sound at once on a state taken there");
    CHECK(eng_note_takes == 1u && eng_keep[3] && !eng_rel[3], "NOTE: counted, and the track kept");
    trk_note_off(&trk[3], 67);
    run(10u * SEC);
    CHECK(!sounding(3) && held(3) == mc, "NOTE: a track a stray note played keeps its state from then on");
    CHECK(!held(1) && !held(2), "NOTE: the others stay released");

    /* FAIL: the arena full of effects' blocks (they yield to an engine), then of a block that does not */
    fresh(); run(SEC);
    {
        uint32_t sz, fails;
        CHECK(eng_rel[1] && !held(1), "FAIL: track 2 released first");
        for (k = 0; k < 6u; k++)
            for (sz = 60000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_JFX0 + RES_JFX_N - 1u - k, sz)) break;
        CHECK(!resource_allocate(RES_ENGINE0 + 1, mc), "FAIL: the arena should have no room for a MACRO state");
        fails = resource_failures;
        song.sel = 1; run(1);                           /* the retake, at the selection: fails; the effects are asked to lend */
        CHECK(!eng_rel[1] && !held(1) && resource_failures > fails, "FAIL: the retake should have failed: %u B held", held(1));
        trk_note_on(&trk[1], 60, 100);
        CHECK(!sounding(1) && trk[1].xp_n == 1u, "FAIL: the note should wait for the state, not be dropped");
        CHECK(held(0) == mc || eng_rel[0], "FAIL: the other tracks as they were");
        for (b = 0; b < 8u && !sounding(1); b++) run(1);
        CHECK(sounding(1) == 1u && held(1) == mc && !trk[1].xp_n, "FAIL: the effects gave their blocks back and the waiting note plays (%u blocks)", b);
        printf("track release: FAIL: the retake failed with the arena full, the note waited %u blocks for the effects' memory and played\n", b);
        trk_note_off(&trk[1], 60);
    }
    fresh(); run(SEC);
    {
        uint32_t sz, fails;
        for (k = 0; k < 2u; k++)
            for (sz = 100000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_HOT0 + k, sz)) break;   /* (not the effects': nothing yields) */
        for (sz = 100000u; sz >= 400u; sz -= 400u) if (resource_allocate(RES_ENGINE0 + 3, sz)) break;
        CHECK(!resource_allocate(RES_ENGINE0 + 1, mc), "FAIL: the arena should have no room for a MACRO state");
        fails = resource_failures;
        song.sel = 1; run(1);
        trk_note_on(&trk[1], 60, 100);
        run(200);
        CHECK(!held(1) && !sounding(1) && trk[1].xp_n == 1u, "FAIL: no memory anywhere: the track is silent, its note waiting");
        CHECK(resource_failures - fails <= 4u, "FAIL: a failed take should be asked again only after a release, not every block: %u scans", resource_failures - fails);
        resource_release(RES_HOT0);
        run(2);
        CHECK(held(1) == mc && sounding(1) == 1u, "FAIL: memory freed: the state is taken and the note plays");
        resource_release(RES_HOT0 + 1u); resource_release(RES_ENGINE0 + 3);
    }

    /* SAME */
    same_render(0);
    {
        uint32_t rel = eng_releases, ret = eng_retakes, nt = eng_note_takes;
        int64_t e = 0, d = 0;
        same_render(1);
        CHECK(rel >= 2u && ret >= 2u && !nt, "SAME: the first render should release and retake: %u, %u, %u at a note", rel, ret, nt);
        CHECK(!eng_releases, "SAME: the second render should release nothing: %u", eng_releases);
        for (b = 0; b < SAME_N; b++)
            for (uint32_t i = 0; i < CTL * 2u; i++) { int64_t x = same[0][b][i], y = same[1][b][i]; e += x < 0 ? -x : x; d += x > y ? x - y : y - x; }
        CHECK(e > 0 && d == 0, "SAME: the two renders differ: %lld of %lld", (long long)d, (long long)e);
        printf("track release: SAME: 4.4 s of four tracks with %u releases and %u retakes, and with none: the same samples\n", rel, ret);
    }

    if (bad) { printf("track release: %d FAILED\n", bad); return 1; }
    printf("track release: OK\n");
    return 0;
}
