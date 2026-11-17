/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Round 27 item 3: a track turned OFF (firmware/src/voice.c trk_off_block, eng_state_take_quiet; seq.c seq_off_quiet; ui_layer.c glo_track_off;
 * BUILD.md 7.40), through the whole mix as the firmware runs it, on an arena of the unit's size, with the automatic release compiled out (OC_TRK_RELEASE 0).
 *   KINDS    MACRO, SHAPES (the module's stand-in: the built-in engine) and DRUM-X (a block hook: its state runs with no voice), a note sounding: OFF fades it
 *            out in XF_BLOCKS blocks (no click: falling), the state goes back to the arena (its bytes), the track is not rendered; ON takes it back
 *   SILENT   while OFF: a key, MIDI on its channel, the sequencer (playing two passes) and the arp start no voice and take no memory; nothing recorded;
 *            the transport and the other tracks play on
 *   KEPT     the steps, the sound (preset, params) and the mix as they were after OFF and ON; a preset and an engine change made while OFF take nothing,
 *            and come in at the ON (the new engine's state)
 *   LAUNCH   ON while playing: its sequencer notes start at the next launch point (LAUNCH LOOP: its loop end; BAR: the bar line), a key at once; stopped: at once
 *   PROJECT  a project saved with tracks 2 and 4 OFF, loaded: they are released (no state held), silent, and their steps are the project's
 *   THREE    four MACRO tracks, three OFF: their 3 x 11 KB back; the fourth's DUB / effects room grows by as much (an allocation that failed now fits);
 *            its voices are what they were (the budget is of sounding voices: an idle track holds none)
 *   WAIT     ON with the arena full of the effects' blocks: the track waits (WAIT on its row), the effects keep their memory (no yield asked), its keys
 *            are silent and nothing waits in xp_note; a block freed: the state is taken and it plays
 *   KEY      GLO + OCT+ (a tap) turns the selected track OFF and ON again, the octave unchanged; a quick GLO + OCT- is GLO's put back (no leak); a dead GLO turns nothing OFF
 *   FADE     ON inside the OFF's fade, and inside an engine change's: the fade ends, the state is held, not left waiting
 *   TAKES    a take recorded through the keys' path waits for the launch point too (recording.c)
 *   LATE     ON while waiting for memory across a loop end: it comes in at a launch point after the take
 *   LOAD     a load that turns a track ON takes its state as a load does
 * Run by tests/run_tests.sh. On a firmware without the engine side (06fd4f40) it builds with the fallbacks below and fails. */
#define RES_HOST_MAIN_WORDS (85760u / 4u)              /* the device's banks (tests/jfx_arena_test.c) */
#define RES_HOST_LOW_WORDS (27740u / 4u)
#define RES_HOST_DATA_WORDS (4216u / 4u)
#define RES_HOST_RETAINED_WORDS (816u / 4u)
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#ifndef OC_TRK_OFF_ENGINE                               /* (the firmware before Round 27: the bit and nothing that reads it) */
static uint8_t trk_on_wait, seq_off_quiet;
static uint32_t trk_off_n, trk_off_bytes, trk_on_n, trk_on_waits;
static void track_off_req(uint32_t k, int off) { trk_off_set(k, off); }
#endif

static int bad;
#define CHECK(c, ...) do { if (!(c)) { bad++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define SEC (FS / CTL + 2u)
static int32_t blk[CTL * 2];
static void run(uint32_t n) { while (n--) mix_block(blk, CTL); }
static uint32_t held(uint32_t k) { return resource[RES_ENGINE0 + k].size; }
static uint32_t sz(uint32_t e) { return (eng_state_size(e) + 3u) & ~3u; }
static uint32_t sounding(uint32_t k) { uint32_t n = 0; for (uint32_t i = 0; i < NVOICE; i++) n += trk[k].v[i].active; return n; }
static void put(track_t *t, uint32_t i, uint32_t note)
{
    t->step[i].note[0] = (uint8_t)note; t->step[i].n = 1; t->step[i].time = ST_NOTE; t->step[i].vel = 100;
}
/* four tracks of engine e, empty patterns, track 1 selected, stopped, nothing OFF */
static void fresh_e(uint32_t e)
{
    ui_power_on();
    resource_host_cache_enabled = 0; res_lend_ok = 0;
    song.master_q12 = 4096; song.g[G_BPM] = 120; song.playing = 0; song.sel = 0;
    trk_off_bits = 0;
    run(2);
    trk_off_n = trk_off_bytes = trk_on_n = trk_on_waits = 0;
    for (uint32_t k = 0; k < NPART; k++) {
        set_engine_of(&trk[k], e); apply_preset_to(&trk[k], e == ENGI_MACRO ? 8u : 0u);
        track_defaults_steps(&trk[k]);
        trk[k].p[P_SLEN] = 16;
    }
    run(8);
}
static void fresh(void) { fresh_e(ENGI_MACRO); }
/* the effects' room: 2 KB blocks taken in every effect slot until the arena has none, counted, given back */
static uint32_t fx_room(void)
{
    uint32_t n = 0, k;
    for (k = 0; k < RES_JFX_N; k++) {
        uint32_t s;
        for (s = 98304u; s >= 2048u; s -= 2048u) if (resource_allocate(RES_JFX0 + k, s)) { n += s; break; }
    }
    for (k = 0; k < RES_JFX_N; k++) resource_release(RES_JFX0 + k);
    return n;
}
static uint32_t off_settle(uint32_t k)                  /* blocks until track k has let go of its state (at most 64) */
{
    uint32_t b;
    for (b = 0; b < 64u && held(k); b++) run(1);
    return b;
}

int main(void)
{
    uint32_t mc = sz(ENGI_MACRO), k, b;
    setvbuf(stdout, NULL, _IONBF, 0);

    /* KINDS */
    {
        static const uint32_t E[3] = {ENGI_MACRO, ENGI_SHAPES, ENGI_DRUMX};
        static const char *const N[3] = {"MACRO", "SHAPES", "DRUM-X"};
        for (uint32_t c = 0; c < 3u; c++) {
            uint32_t s = sz(E[c]), used, fade, rendered = 0;
            int32_t pk0 = 0, pk_last = 0;
            fresh_e(E[c]);
            CHECK(trk[1].engine == E[c] && held(1) == s, "KINDS %s: track 2 holds its state at first: %u of %u B", N[c], held(1), s);
            trk_note_on(&trk[1], E[c] == ENGI_DRUMX ? 36u : 60u, 110);
            run(40);
            CHECK(sounding(1) || E[c] == ENGI_DRUMX, "KINDS %s: the note sounds", N[c]);
            for (b = 0; b < 4u; b++) { trk[1].peak = 0; run(1); if (trk[1].peak > pk0) pk0 = trk[1].peak; }
            used = resource_used();
            track_off_req(1, 1);
            for (fade = 0; fade < 64u && (sounding(1) || held(1)); fade++) {
                trk[1].peak = 0; run(1);
                if (trk[1].peak) { pk_last = trk[1].peak; rendered = fade + 1u; }
            }
            CHECK(!sounding(1) && !held(1) && eng_rel[1], "KINDS %s: OFF: no voice, no state held (%u B), released", N[c], held(1));
            CHECK(used - resource_used() == s, "KINDS %s: OFF: the arena's bytes back: %u of %u", N[c], used - resource_used(), s);
            CHECK(fade <= XF_BLOCKS + 2u, "KINDS %s: OFF took %u blocks (the fade is %u)", N[c], fade, XF_BLOCKS);
            CHECK(!pk0 || pk_last < pk0, "KINDS %s: the last block heard (%u) falls below the level (%d -> %d): a fade, not a cut", N[c], rendered, pk0, pk_last);
            CHECK(trk_off_n == 1u && trk_off_bytes == s, "KINDS %s: the counts: %u OFF, %u B", N[c], trk_off_n, trk_off_bytes);
            trk[1].peak = 0; run(2u * SEC);
            CHECK(!held(1) && !sounding(1) && !trk[1].peak, "KINDS %s: two seconds OFF: nothing taken (%u B), nothing rendered (peak %d)", N[c], held(1), trk[1].peak);
            CHECK(held(0) == s && held(2) == s && held(3) == s, "KINDS %s: the other tracks keep theirs", N[c]);
            track_off_req(1, 0);
            run(1);
            CHECK(held(1) == s && !eng_rel[1] && !((trk_on_wait >> 1) & 1u) && trk_on_n == 1u, "KINDS %s: ON: the state taken back the next block: %u B", N[c], held(1));
            trk_note_on(&trk[1], E[c] == ENGI_DRUMX ? 36u : 60u, 110);
            trk[1].peak = 0; run(20);
            CHECK(trk[1].peak > 0, "KINDS %s: ON: a key sounds at once", N[c]);
            trk_note_off(&trk[1], 60);
            printf("track off: KINDS %s: OFF in %u blocks (the fade %u), %u B back; ON: taken again, plays\n", N[c], fade, XF_BLOCKS, s);
        }
    }

    /* SILENT */
    fresh();
    {
        uint32_t used, fails, rec0 = 0;
        for (b = 0; b < 16u; b++) put(&trk[1], b, 60u + b % 5u);
        for (b = 0; b < 16u; b += 4u) put(&trk[0], b, 48u);
        midi_event(0x90u, 1, 64, 100); run(4);
        CHECK(sounding(1), "SILENT: (first) MIDI on channel 2 plays track 2");
        midi_event(0x80u, 1, 64, 0); run(SEC / 2u);
        track_off_req(1, 1); off_settle(1); run(2);
        used = resource_used(); fails = resource_failures;
        trk_note_on(&trk[1], 62, 100);
        CHECK(!sounding(1) && !held(1) && !trk[1].xp_n, "SILENT: a key on an OFF track: no voice, no state, nothing waiting");
        trk_note_off(&trk[1], 62);
        midi_event(0x90u, 1, 65, 100); run(4);
        CHECK(!sounding(1) && !held(1) && !trk[1].xp_n, "SILENT: MIDI on its channel: no voice, no state");
        midi_event(0x80u, 1, 65, 0);
        trk[1].p[P_AMODE] = 1; trk_note_on(&trk[1], 60, 100); run(SEC / 2u); trk_note_off(&trk[1], 60); trk[1].p[P_AMODE] = 0;
        CHECK(!sounding(1) && !held(1), "SILENT: the arp: no voice, no state");
        for (b = 0; b < RECORD_MAX; b++) rec0 += recording[b].vel != 0;
        song.rec = 2u;
        transport_req = 1;
        {
            uint32_t any1 = 0, any0 = 0, rec1 = 0;
            for (b = 0; b < 2u * 2u * SEC; b++) {       /* two passes of the 16 steps (2 s) */
                if (b == 100u) input_on(&trk[1], 67, 100);      /* (the keys' own path: sound, the take, the capture) */
                if (b == 300u) input_off(&trk[1], 67);
                run(1); any1 |= sounding(1) || held(1); any0 |= sounding(0);
            }
            for (b = 0; b < RECORD_MAX; b++) rec1 += recording[b].vel != 0;
            for (b = 0; b < NSTEP; b++) if (trk[1].step[b].flags & SF_RECORDED) rec1 += 100u;
            CHECK(song.playing && !any1, "SILENT: the sequencer playing two passes: track 2 started no voice and took no state");
            CHECK(any0, "SILENT: track 1 played on");
            CHECK(rec1 == rec0, "SILENT: armed, a key on the OFF track recorded nothing (%u -> %u)", rec0, rec1);
            CHECK(resource_used() == used && resource_failures == fails, "SILENT: the arena as it was: %u B -> %u B, %u failures", used, resource_used(), resource_failures - fails);
        }
        song.rec = 0; transport_req = 2; run(4);
        printf("track off: SILENT: keys, MIDI, arp, the sequencer: nothing on the OFF track; the others play\n");
    }

    /* KEPT */
    fresh();
    {
        step_t st[NSTEP]; int16_t p[P_COUNT];
        for (b = 0; b < 16u; b += 3u) put(&trk[2], b, 50u + b);
        trk[2].p[P_LEVEL] = 77; trk[2].p[P_PAN] = -20; trk[2].p[P_E0 + 1] = 33;
        memcpy(st, trk[2].step, sizeof st); memcpy(p, trk[2].p, sizeof p);
        track_off_req(2, 1); off_settle(2); run(SEC);
        CHECK(!memcmp(st, trk[2].step, sizeof st) && !memcmp(p, trk[2].p, sizeof p), "KEPT: OFF: the steps and the sound as they were");
        track_off_req(2, 0); run(2);
        CHECK(!memcmp(st, trk[2].step, sizeof st) && !memcmp(p, trk[2].p, sizeof p) && held(2) == mc, "KEPT: ON: the steps and the sound as they were, the state held");
        track_off_req(2, 1); off_settle(2);
        apply_preset_to(&trk[2], 3); run(SEC);
        CHECK(!held(2) && trk[2].preset == 3u, "KEPT: a preset loaded while OFF: stored (preset %u), nothing taken (%u B)", trk[2].preset, held(2));
        set_engine_of(&trk[2], ENGI_SHAPES); apply_preset_to(&trk[2], 0); run(SEC);
        CHECK(!held(2) && trk[2].eng_req == ENGI_SHAPES, "KEPT: an engine change while OFF: asked (eng_req), nothing taken (%u B)", held(2));
        track_off_req(2, 0); run(2);
        CHECK(trk[2].engine == ENGI_SHAPES && held(2) == sz(ENGI_SHAPES), "KEPT: ON: the new engine's state (%u B, SHAPES %u)", held(2), sz(ENGI_SHAPES));
        printf("track off: KEPT: steps, params, level, pan kept; a preset and an engine change while OFF come in at the ON\n");
    }

    /* LAUNCH */
    for (uint32_t mode = 0; mode < 2u; mode++) {
        uint32_t first = 0xFFFFu, idx_at = 0xFFFFu, beat_at = 0xFFFFu;
        fresh();
        lists.launch = (uint8_t)(mode ? OCL_BAR : OCL_LOOP);
        for (b = 0; b < 16u; b++) put(&trk[1], b, 60);
        trk[1].p[P_SLEN] = 12;                          /* (three beats: the loop end is not the bar line) */
        track_off_req(1, 1); off_settle(1);
        transport_req = 1; run(1);
        while (trk[1].seq_idx != 5u) run(1);           /* mid-pass */
        track_off_req(1, 0); run(1);
        CHECK(held(1) == mc, "LAUNCH: ON while playing: the state at once");
        for (b = 0; b < 4u * SEC && first == 0xFFFFu; b++) {
            run(1);
            if (sounding(1)) { first = b; idx_at = trk[1].seq_idx; beat_at = beat_n; }
        }
        if (!mode) CHECK(first != 0xFFFFu && idx_at == 0u, "LAUNCH LOOP: the first note at its loop end (step %u, block %u)", idx_at, first);
        else CHECK(first != 0xFFFFu && beat_at == 0u && idx_at != 0u, "LAUNCH BAR: the first note on the bar line (beat %u, step %u, block %u)", beat_at, idx_at, first);
        transport_req = 2; run(4);
        lists.launch = OCL_LOOP;
        printf("track off: LAUNCH %s: ON at step 5, the first note at step %u, beat %u\n", mode ? "BAR" : "LOOP", idx_at, beat_at);
    }
    fresh();
    track_off_req(1, 1); off_settle(1); track_off_req(1, 0); run(1);
    CHECK(!((seq_off_quiet >> 1) & 1u), "LAUNCH: ON while stopped: nothing waits for a launch point");
    put(&trk[1], 0, 60); transport_req = 1; run(4);
    CHECK(sounding(1), "LAUNCH: then PLAY: its first step plays");
    transport_req = 2; run(4);

    /* PROJECT */
    fresh();
    {
        static project_t pj;
        for (b = 0; b < 16u; b += 2u) { put(&trk[1], b, 61); put(&trk[3], b, 63); }
        trk_off_bits = 0x0Au;
        project_capture(&pj);
        CHECK(pj.t[1].off && pj.t[3].off && !pj.t[0].off, "PROJECT: the capture keeps the bits");
        trk_off_bits = 0; run(SEC);
        CHECK(held(1) == mc && held(3) == mc, "PROJECT: (before the load all four held)");
        CHECK(project_restore_runtime(&pj) == 0, "PROJECT: loaded");
        run(8);
        CHECK(trk_off_bits == 0x0Au && !held(1) && !held(3) && held(0) == mc && held(2) == mc, "PROJECT: tracks 2 and 4 OFF and released: %u, %u B held", held(1), held(3));
        transport_req = 1;
        { uint32_t any = 0; for (b = 0; b < 2u * SEC; b++) { run(1); any |= sounding(1) | sounding(3) | held(1) | held(3); }
          CHECK(!any, "PROJECT: playing, the OFF tracks stay silent and take nothing"); }
        CHECK(trk[1].step[0].n == 1u && trk[1].step[0].note[0] == 61u, "PROJECT: their steps are the project's");
        transport_req = 2; run(4);
        trk_off_bits = 0; run(2);
        printf("track off: PROJECT: loaded with tracks 2 and 4 OFF: released, silent, their steps there\n");
    }

    /* THREE */
    fresh();
    {
        uint32_t used4 = resource_used(), used1, got_before, got_after, v_before, v_after, i;
        song.sel = 3;
        run(2);
        got_before = fx_room();
        for (i = 0; i < 8u; i++) trk_note_on(&trk[3], 48u + 4u * i, 100);
        run(8); v_before = sounding(3);
        for (i = 0; i < 8u; i++) trk_note_off(&trk[3], 48u + 4u * i);
        run(3u * SEC);
        for (k = 0; k < 3u; k++) track_off_req(k, 1);
        for (k = 0; k < 3u; k++) off_settle(k);
        run(2);
        used1 = resource_used();
        CHECK(used4 - used1 == 3u * mc, "THREE: three MACRO tracks OFF: %u B back (3 x %u)", used4 - used1, mc);
        got_after = fx_room();
        CHECK(got_after >= got_before + 3u * mc - 3u * 2048u, "THREE: what the effects can take (2 KB blocks): %u B -> %u B (3 x %u B more, less the blocks' rounding)", got_before, got_after, mc);
        for (i = 0; i < 8u; i++) trk_note_on(&trk[3], 48u + 4u * i, 100);
        run(8); v_after = sounding(3);
        CHECK(v_after == v_before && v_after > 0u, "THREE: the fourth's voices: %u before, %u with three OFF (the budget is of sounding voices)", v_before, v_after);
        for (i = 0; i < 8u; i++) trk_note_off(&trk[3], 48u + 4u * i);
        printf("track off: THREE: arena used %u B -> %u B (%u back); the effects' room (2 KB blocks) %u B -> %u B; the fourth's voices %u -> %u\n",
               used4, used1, used4 - used1, got_before, got_after, v_before, v_after);
    }

    /* WAIT */
    fresh();
    {
        uint32_t s, fx_held = 0, ks;
        track_off_req(1, 1); off_settle(1); run(2);
        for (k = 0; k < 6u; k++)
            for (s = 60000u; s >= 400u; s -= 400u) if (resource_allocate(RES_JFX0 + RES_JFX_N - 1u - k, s)) break;
        for (k = 0; k < 6u; k++) fx_held += resource[RES_JFX0 + RES_JFX_N - 1u - k].size;
        CHECK(!resource_allocate(RES_ENGINE0 + 1, mc), "WAIT: (the arena has no room for a MACRO state)");
        track_off_req(1, 0); run(1);
        ks = 0; for (k = 0; k < 6u; k++) ks += resource[RES_JFX0 + RES_JFX_N - 1u - k].size;
        CHECK(!held(1) && ((trk_on_wait >> 1) & 1u), "WAIT: ON, no room: the track waits (%u B held)", held(1));
        CHECK(!res_jfx_yield && ks == fx_held, "WAIT: the effects are not made to yield: %u of %u B still theirs, yield %u", ks, fx_held, res_jfx_yield);
        trk_note_on(&trk[1], 60, 100); run(8);
        CHECK(!sounding(1) && !trk[1].xp_n && !held(1), "WAIT: a key while waiting: silent, nothing waits for the state");
        trk_note_off(&trk[1], 60);
        run(SEC);
        ks = 0; for (k = 0; k < 6u; k++) ks += resource[RES_JFX0 + RES_JFX_N - 1u - k].size;
        CHECK(ks == fx_held && ((trk_on_wait >> 1) & 1u), "WAIT: a second on: still waiting, the effects whole");
        for (k = 0; k < 6u; k++) resource_release(RES_JFX0 + RES_JFX_N - 1u - k);
        run(2);
        CHECK(held(1) == mc && !((trk_on_wait >> 1) & 1u), "WAIT: memory freed: the state is taken (%u B)", held(1));
        trk_note_on(&trk[1], 60, 100); run(4);
        CHECK(sounding(1), "WAIT: and the key plays");
        trk_note_off(&trk[1], 60);
        printf("track off: WAIT: ON with the effects holding the arena: the track waited %u blocks, the effects kept %u B; freed: taken\n", trk_on_waits, fx_held);
    }

    /* KEY: GLO + OCT+ on the selected track */
    fresh();
    {
        uint32_t sel;
        int8_t oct;
        go_home(); frames(100);
        song.sel = 2; sel = 2; frames(50);
        oct = song.octave;
        btn_down(B_GLO); frames(48); press(B_OCTUP); frames(48); btn_up(B_GLO); frames(100);
        run(8);
        CHECK(trk_off(sel) && !held(sel), "KEY: GLO + OCT+: the selected track (3) OFF, released (%u B)", held(sel));
        CHECK(!trk_off(0) && !trk_off(1) && !trk_off(3), "KEY: the others ON");
        CHECK(song.octave == oct, "KEY: the octave unchanged (%d, was %d)", song.octave, oct);
        btn_down(B_GLO); frames(48); press(B_OCTUP); frames(48); btn_up(B_GLO); frames(100);
        run(2);
        CHECK(!trk_off(sel) && held(sel) == mc, "KEY: again: ON, the state back");
        CHECK(song.octave == oct, "KEY: the octave still unchanged");
        {   /* a quick GLO + OCT-: GLO's put back, the tap not leaked to the page (the octave, a page's OCT-) */
            uint32_t home = ui.home, pg = ui.home_pg;
            ui.msg[0] = 0;
            btn_down(B_GLO); frames(48); press(B_OCTDN); frames(16);
            CHECK(!strcmp(ui.msg, "MIX PUT BACK"), "KEY: a quick GLO + OCT-: GLO's put back took the tap (said \"%s\")", ui.msg);
            frames(32); btn_up(B_GLO); frames(100);
            CHECK(song.octave == oct && ui.home == home && ui.home_pg == pg && !trk_off(sel), "KEY: a quick GLO + OCT-: no octave, the page as it was, no OFF");
        }
        {   /* a dead GLO (pressed with the menu up, LY_DEAD): its OCT+ is no OFF */
            ui.menu = 1; btn_down(B_GLO); frames(48); ui.menu = 0; frames(32);
            press(B_OCTUP); frames(48); btn_up(B_GLO); frames(100); run(8);
            CHECK(!trk_off(sel), "KEY: a dead GLO (pressed under the menu) + OCT+: no OFF");
            song.octave = oct;
        }
        printf("track off: KEY: GLO + OCT+ turns the selected track OFF and ON; a quick OCT- does not leak, a dead GLO turns nothing OFF\n");
    }

    /* FADE: ON inside the OFF's fade (2.9 ms), and inside an engine change's fade */
    fresh();
    {
        trk_note_on(&trk[1], 60, 110); run(20);
        track_off_req(1, 1); run(1);
        CHECK(trk[1].xf_on && held(1) == mc, "FADE: (OFF: the fade running)");
        track_off_req(1, 0);
        run(XF_BLOCKS + 4u);
        CHECK(!((trk_on_wait >> 1) & 1u) && !trk[1].xf_on && !sounding(1) && held(1) == mc, "FADE: ON inside the fade: not left waiting (wait %u, xf_on %u, xf %u, voices %u, %u B)",
              (trk_on_wait >> 1) & 1u, trk[1].xf_on, trk[1].xf, sounding(1), held(1));
        trk_note_on(&trk[1], 62, 100); run(4);
        CHECK(sounding(1), "FADE: and a key plays");
        trk_note_off(&trk[1], 62); trk_note_off(&trk[1], 60);
        trk_note_on(&trk[1], 64, 110); run(10);
        set_engine_of(&trk[1], ENGI_SHAPES); apply_preset_to(&trk[1], 0);
        run(1);
        track_off_req(1, 1); run(1); track_off_req(1, 0);
        run(XF_BLOCKS + 6u);
        CHECK(!((trk_on_wait >> 1) & 1u) && trk[1].engine == ENGI_SHAPES && held(1) == sz(ENGI_SHAPES) && !trk[1].xf_on, "FADE: OFF and ON inside an engine change's fade: SHAPES, held (%u B), not waiting", held(1));
        printf("track off: FADE: ON inside the OFF's fade and inside an engine change's: taken, not left waiting\n");
    }

    /* TAKES: a recorded take on track 2 (the keys' path, armed), then OFF and ON mid-pass: its recorded notes wait for the loop end too */
    fresh();
    {
        uint32_t first = 0xFFFFu, idx_at = 0xFFFFu, i;
        trk[1].p[P_SLEN] = 16;
        song.rec = 2u; transport_req = 1; run(1);
        for (i = 0; i < 16u; i++) {                     /* a note on every step, through input_on */
            uint32_t s0 = trk[1].seq_idx;
            input_on(&trk[1], 60u + (i & 3u), 100);
            while (trk[1].seq_idx == s0) run(1);
            input_off(&trk[1], 60u + (i & 3u));
        }
        song.rec = 0; run(8);
        { uint32_t n = 0; for (i = 0; i < RECORD_MAX; i++) n += recording[i].vel != 0; CHECK(n >= 8u, "TAKES: (the take recorded: %u notes)", n); }
        track_off_req(1, 1); off_settle(1);
        while (trk[1].seq_idx != 5u) run(1);
        track_off_req(1, 0); run(1);
        for (b = 0; b < 4u * SEC && first == 0xFFFFu; b++) { run(1); if (sounding(1)) { first = b; idx_at = trk[1].seq_idx; } }
        CHECK(first != 0xFFFFu && idx_at == 0u, "TAKES: ON at step 5: the recorded notes from the loop end (first at step %u)", idx_at);
        transport_req = 2; run(4);
        printf("track off: TAKES: a recorded take waits for the launch point too (first note at step %u)\n", idx_at);
    }

    /* LATE: ON while waiting for memory, across a launch point: it comes in at a launch point after its state is taken */
    fresh();
    {
        uint32_t s, first = 0xFFFFu, idx_at = 0xFFFFu, taken_at = 0xFFFFu;
        for (b = 0; b < 16u; b++) put(&trk[1], b, 60);
        track_off_req(1, 1); off_settle(1);
        transport_req = 1; run(1);
        for (k = 0; k < 6u; k++)
            for (s = 60000u; s >= 400u; s -= 400u) if (resource_allocate(RES_JFX0 + RES_JFX_N - 1u - k, s)) break;
        while (trk[1].seq_idx != 5u) run(1);
        track_off_req(1, 0); run(1);
        CHECK((trk_on_wait >> 1) & 1u, "LATE: (waiting for memory)");
        while (trk[1].seq_idx != 0u) run(1);           /* a launch point passes while it waits */
        while (trk[1].seq_idx != 6u) run(1);
        CHECK(((seq_off_quiet >> 1) & 1u) && !sounding(1), "LATE: the loop end passed while waiting: still quiet");
        for (k = 0; k < 6u; k++) resource_release(RES_JFX0 + RES_JFX_N - 1u - k);
        run(2); taken_at = trk[1].seq_idx;
        CHECK(held(1) == mc, "LATE: memory freed at step %u: taken", taken_at);
        for (b = 0; b < 4u * SEC && first == 0xFFFFu; b++) { run(1); if (sounding(1)) { first = b; idx_at = trk[1].seq_idx; } }
        CHECK(first != 0xFFFFu && idx_at == 0u, "LATE: the first note at the next loop end after the take (step %u)", idx_at);
        transport_req = 2; run(4);
        printf("track off: LATE: waiting across a loop end, taken at step %u, the first note at step %u\n", taken_at, idx_at);
    }

    /* LOAD: a project load that turns a track ON takes its state as a load does: the effects yield (not the quiet take's wait) */
    fresh();
    {
        static project_t pj;
        uint32_t s;
        project_capture(&pj);                           /* (all four ON) */
        track_off_req(1, 1); off_settle(1);
        for (k = 0; k < 6u; k++)
            for (s = 60000u; s >= 400u; s -= 400u) if (resource_allocate(RES_JFX0 + RES_JFX_N - 1u - k, s)) break;
        CHECK(project_restore_runtime(&pj) == 0, "LOAD: (loaded)");
        for (b = 0; b < 16u && held(1) != mc; b++) run(1);
        CHECK(held(1) == mc && !((trk_on_wait >> 1) & 1u), "LOAD: track 2 turned ON by the load: its state taken in %u blocks (the effects gave way)", b);
        for (k = 0; k < 6u; k++) resource_release(RES_JFX0 + RES_JFX_N - 1u - k);
        printf("track off: LOAD: a load turning a track ON takes the state as a load does (%u blocks)\n", b);
    }

    if (bad) { printf("track off: %d FAILED\n", bad); return 1; }
    printf("track off: OK\n");
    return 0;
}
