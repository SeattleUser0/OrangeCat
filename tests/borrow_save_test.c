/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat Round 30 item 1 (BUILD.md 7.47): the stopped-transport borrow of the audio arena (firmware/src/res_borrow.c) for the loader's save (oc_pj4_pool.c).
 * Build with -DOC_PJ4=1 (the loader, and OC_BORROW with it), over tests/oc_project_test.c's simulated NOR and the firmware's own mix_block:
 *   1. refused while playing: res_borrow says PLAYING, nothing of the arena moves, nothing is written.
 *   2. the UNSAVED rule: a SAVE while playing writes nothing and says UNSAVED; it waits while the transport runs; at the stop it runs with the music as it is then.
 *   3. a save while stopped with the chorus, the delay and the reverb holding their memory: the returns fade (a ramp, no step), the effects give their blocks back,
 *      the save runs in the borrowed pieces, the pieces go back, the effects take their memory again; the engine states are not touched.
 *   4. PLAY pressed during the save waits: the transport does not start until the borrow ends, then it does.
 *   5. a power cut at every program and at every erase of a save made in borrowed memory: the slot reads as before or as after, and every piece is back.
 * (Round 30 review, each failing on d066a82d)
 *   6. the MIDI clock's resync (a clock back after a loss) does not start the transport while the borrow is held; it does at the next beat after.
 *   7. a load that fails (an empty slot) while a save waits for the stop: the save still waits.
 *   8. a borrow that times out in its fade: the effects' gain comes back up without a step, the save waits (UNSAVED), it is not dropped.
 *   9. a 3 s save under incoming notes and clock with a MIDI Start in it: the queue never overflows, every note-off arrives (no note left on), the Start runs after.
 *  10. the chain's start (chain.armed and a start request) pressed during the save waits too.
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"
#include <assert.h>

static int32_t bt_out[2u * CTL];
static uint32_t bt_blocks, bt_play_seen, bt_gain_bad, bt_gain_last, bt_min_gain;
static int bt_play_at = -1;
static void bt_block(void)                               /* one audio block as the interrupt runs it, while the main loop waits in res_borrow */
{
    if (bt_play_at >= 0 && bt_blocks == (uint32_t)bt_play_at) transport_req = 1;   /* PLAY pressed (or MIDI, the chain) while the save is on */
    mix_block(bt_out, CTL);
    bt_blocks++;
    if (song.playing) bt_play_seen++;
    if (jfx_bor_g > bt_gain_last || bt_gain_last - jfx_bor_g > 32768u / RES_BOR_FADE + 1u) bt_gain_bad++;   /* (down a little a block, never up, during the borrow) */
    bt_gain_last = jfx_bor_g;
    if (jfx_bor_g < bt_min_gain) bt_min_gain = jfx_bor_g;
}
static void bt_sends_on(void)
{
    song.g[G_RTYPE] = 0;
    song.g[G_DTIME] = 49;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 100;
    for (uint32_t b = 0; b < 64u; b++) mix_block(bt_out, CTL);
}
static int bt_fx_held(void) { return cho_buf && jrev && dly_n && dly_have >= dly_n; }
static int bt_pieces_back(void)
{
    for (uint32_t k = 0; k < RES_BOR_N; k++) if (resource[RES_BOR0 + k].size) return 0;
    return !res_bor_hold && res_bor_st == BOR_IDLE;
}

static int t_refused_playing(void)
{
    int ok = 1;
    uint32_t sz[3] = {PJ4_RECMAX, PROJ_STORE_SIZE, (uint32_t)sizeof(pj4_ws_t)}, k, before[RES_COUNT];
    uint8_t *p[3];
    reset();
    bt_sends_on();
    ok &= bt_fx_held();
    for (k = 0; k < RES_COUNT; k++) before[k] = resource[k].size;
    song.playing = 1;
    res_bor_host_block = bt_block;
    ok &= res_borrow(sz, 3u, p) == RES_BOR_PLAYING;
    for (k = 0; k < RES_COUNT; k++) ok &= resource[k].size == before[k];
    ok &= bt_pieces_back() && bt_fx_held();
    song.playing = 0;
    transport_req = 1;                                   /* (a start pending counts as playing) */
    ok &= res_borrow(sz, 3u, p) == RES_BOR_PLAYING && bt_pieces_back();
    transport_req = 0;
    return check("borrow refused while playing (or a start pending): the arena untouched, the effects keep their memory", ok);
}
static int t_unsaved(void)
{
    int ok = 1;
    uint32_t e0;
    reset();
    res_bor_host_block = bt_block;
    fill_all(3, 4);
    ok &= project_save_as(0, "UNSV") == 0;
    snap(&A);
    fill_all(9, 4);
    song.playing = 1;
    e0 = erases;
    ok &= project_save(0) == 1 && !strcmp(ui.msg, "UNSAVED") && project_pending();
    ok &= erases == e0;
    fm1_ms += 2500u;
    project_pend_poll();                                 /* still playing: waits, says so again */
    ok &= erases == e0 && project_pending() && !strcmp(ui.msg, "UNSAVED");
    fill_all(11, 4);                                     /* (the music changes on while it plays) */
    snap(&B);
    song.playing = 0;
    project_pend_poll();                                 /* the stop: the save runs, with the music as it is now */
    ok &= !project_pending() && erases > e0;
    reboot(); wipe_runtime(); project_load(0); snap(&C);
    ok &= same(&B, &C) && !same(&A, &C);
    return check("a SAVE while playing: nothing written, UNSAVED until the stop, then saved with the music at the stop", ok);
}
static int t_save_stopped(void)
{
    int ok = 1;
    uint8_t *eng = 0, keep[64];
    uint32_t g0 = res_bor_grants, blocks;
    reset();
    res_bor_host_block = bt_block;
    fill_all(4, 4);
    trk[1].engine = trk[1].eng_req = ENGI_MACRO;
    eng = resource_get(RES_ENGINE0 + 1u, eng_state_size(ENGI_MACRO));
    ok &= eng != 0;
    if (eng) { memset(eng, 0x5A, 64); memcpy(keep, eng, sizeof keep); }
    bt_sends_on();
    ok &= bt_fx_held();
    snap(&A);
    bt_blocks = 0; bt_gain_bad = 0; bt_gain_last = 32768u; bt_min_gain = 32768u;
    ok &= project_save_as(0, "FXSAVE") == 0;
    blocks = bt_blocks;
    ok &= res_bor_grants == g0 + 1u && pjp_bor_rc == RES_BOR_OK;
    ok &= blocks >= RES_BOR_FADE && blocks < RES_BOR_FADE + 40u;          /* the fade, then a piece a block */
    ok &= bt_gain_bad == 0 && bt_min_gain == 0;                           /* a ramp down to 0, no step */
    ok &= bt_pieces_back() && !cho_buf && !jrev && !dly_have;            /* the pieces back; the effects gave theirs */
    ok &= resource[RES_ENGINE0 + 1u].ptr == eng && !memcmp(eng, keep, sizeof keep);
    for (uint32_t b = 0; b < 64u; b++) mix_block(bt_out, CTL);
    ok &= bt_fx_held() && jfx_bor_g == 32768u;                            /* taken again, as after a stop */
    reboot(); wipe_runtime(); project_load(0); snap(&B);
    ok &= same(&A, &B);
    {
        char what[200];
        snprintf(what, sizeof what, "a save while stopped, the effects holding memory: faded over %u blocks, given back, saved in %u blocks, taken again; engine state kept", (unsigned)RES_BOR_FADE, blocks);
        return check(what, ok);
    }
}
static int t_play_waits(void)
{
    int ok = 1;
    reset();
    res_bor_host_block = bt_block;
    fill_all(5, 4);
    bt_sends_on();
    bt_blocks = 0; bt_play_seen = 0; bt_play_at = 5; bt_gain_last = 32768u;
    ok &= project_save_as(0, "PLAYW") == 0;
    bt_play_at = -1;
    ok &= bt_play_seen == 0 && transport_req == 1u && !song.playing;     /* pressed during the save: waited */
    mix_block(bt_out, CTL);
    ok &= song.playing == 1 && transport_req == 0u;                       /* the borrow over: it starts */
    transport_req = 2; mix_block(bt_out, CTL);
    ok &= !song.playing;
    return check("PLAY pressed during the save waits until the borrow ends, then the transport starts", ok);
}
static int t_cuts(void)
{
    static uint8_t keep[sizeof nor];
    int ok = 1, done;
    uint32_t n, cuts = 0, torn = 0, leak = 0, g0 = res_bor_grants;
    reset();
    res_bor_host_block = bt_block;
    fill_all(7, 4);
    ok &= project_save_as(0, "CUTS") == 0;
    snap(&A);
    memcpy(keep, nor, sizeof nor);
    for (int mode = 0; mode < 2; mode++) {               /* 0: a cut before each program; 1: a cut right after each erase */
        done = 0;
        for (n = 0; n < 400u && !done; n++) {
            memcpy(nor, keep, sizeof nor);
            reboot(); wipe_runtime(); project_load(0);
            fill_all(8, 4);
            if (mode == 0) snap(&B);
            bt_sends_on();
            if (mode == 0) fail_after = (int)n; else erase_cut = (int)n;
            done = project_save(0) == 0;
            fail_after = -1; erase_cut = -1;
            leak += !bt_pieces_back();
            cuts++;
            reboot(); wipe_runtime(); project_load(0); snap(&C);
            if (!(same(&A, &C) || same(&B, &C))) torn++;
            if (done) ok &= same(&B, &C);
        }
        ok &= done;
    }
    ok &= res_bor_grants > g0 + cuts - 1u;
    {
        char what[220];
        snprintf(what, sizeof what, "a power cut at each of %u programs and erases of saves in borrowed memory: the slot as before or after (%u mixed), every piece back (%u not)", cuts, torn, leak);
        return check(what, ok && !torn && !leak);
    }
}

static int t_resync_held(void)
{
    int ok = 1;
    uint32_t k, h0 = midi_clock_resync_held;
    reset();
    memset(&midi_clock, 0, sizeof midi_clock);
    midi_clock.lost = 2u;                                /* (the clock was lost while playing and has come back: counting its pulses) */
    midi_clock.lost_n = 0;
    res_bor_hold = 1;
    midi_clock_resync(1000u, 1000u);                     /* (lost_n 1: a beat's first pulse, where a resync starts the transport) */
    ok &= !song.playing && midi_clock.lost == 2u && midi_clock_resync_held == h0 + 1u;
    res_bor_hold = 0;
    for (k = 0; k < 23u; k++) midi_clock_resync(1000u, 1000u);
    ok &= !song.playing;
    midi_clock_resync(1000u, 1000u);                     /* (the next beat: after the save) */
    ok &= song.playing == 1 && midi_clock.lost == 0u;
    song.playing = 0;
    return check("the MIDI clock's resync waits while the borrow is held and starts at the next beat after it", ok);
}
static int t_load_fail_keeps(void)
{
    int ok = 1;
    reset();
    res_bor_host_block = bt_block;
    fill_all(3, 4);
    ok &= project_save_as(0, "KEEP") == 0;
    song.playing = 1;
    ok &= project_save(0) == 1 && project_pending();
    project_load(3);                                     /* (an empty slot: EMPTY SLOT, nothing loaded) */
    ok &= project_pending();
    project_load(0);                                     /* (a load that happens: the music the save was for is gone) */
    ok &= !project_pending();
    song.playing = 0;
    return check("a load that fails (an empty slot) leaves the waiting save waiting; one that loads drops it", ok);
}
static uint32_t bt_dead_after;
static void bt_block_dies(void) { if (bt_blocks < bt_dead_after) bt_block(); }   /* (the audio stops answering in the fade) */
static int t_timeout(void)
{
    int ok = 1;
    uint32_t e0, b;
    reset();
    fill_all(3, 4);
    res_bor_host_block = bt_block;
    ok &= project_save_as(0, "LATE") == 0;
    fill_all(6, 4);
    bt_sends_on();
    bt_blocks = 0; bt_dead_after = 10; bt_gain_last = 32768u; bt_gain_bad = 0;
    res_bor_host_block = bt_block_dies;
    e0 = erases;
    ok &= project_save(0) == 1 && !strcmp(ui.msg, "UNSAVED") && project_pending() && erases == e0;
    ok &= jfx_bor_g < 32768u && bt_pieces_back();
    res_bor_host_block = bt_block;
    for (b = 0; b < 200u; b++) {                         /* (back up a little a block, never a step) */
        uint32_t g = jfx_bor_g;
        mix_block(bt_out, CTL);
        if (jfx_bor_g < g || jfx_bor_g - g > 32768u / RES_BOR_FADE + 1u) ok = 0;
    }
    ok &= jfx_bor_g == 32768u && bt_fx_held();
    fm1_ms += 2500u;
    project_pend_poll();                                 /* (tried again: the audio answers now) */
    ok &= !project_pending() && erases > e0;
    return check("a borrow that times out in its fade: the gain back up with no step, UNSAVED, the save kept and run later", ok);
}
static int t_midi_long_save(void)
{
    int ok = 1;
    uint32_t sz[3] = {PJ4_RECMAX, PROJ_STORE_SIZE, (uint32_t)sizeof(pj4_ws_t)}, b, on = 0, k, act = 0, heard = 0;
    uint8_t *p[3];
    reset();
    res_bor_host_block = bt_block;
    song.g[G_CLOCK] = 1;                                 /* (the clock from USB) */
    bt_sends_on();
    midi_in_overflow = 0;
    ok &= res_borrow(sz, 3u, p) == RES_BOR_OK;
    for (b = 0; b < 4134u; b++) {                        /* 3 s of blocks while the save's flash work runs */
        if (b % 29u == 0u) midi_enqueue(0x0Fu | 0xF8u << 8, 1u);          /* 120 bpm: 48 clocks a second */
        if (b == 100u) midi_enqueue(0x0Fu | 0xFAu << 8, 1u);              /* MIDI Start during the save */
        if (b % 60u == 0u) { midi_enqueue(on ? (0x08u | 0x80u << 8 | 60u << 16) : (0x09u | 0x90u << 8 | 60u << 16 | 100u << 24), 1u); on ^= 1u; }
        fm1_ms += (b & 1u);
        bt_block();
        ok &= !song.playing;
        for (uint32_t t = 0; t < NTRK; t++) for (k = 0; k < NVOICE; k++) heard += trk[t].v[k].active && trk[t].v[k].gate;
    }
    if (on) midi_enqueue(0x08u | 0x80u << 8 | 60u << 16, 1u);
    ok &= !midi_in_overflow;
    res_borrow_end();
    for (b = 0; b < 400u; b++) bt_block();
    ok &= song.playing == 1;                             /* (the Start ran after the save) */
    for (uint32_t t = 0; t < NTRK; t++) for (k = 0; k < NVOICE; k++) act += trk[t].v[k].active && trk[t].v[k].gate;
    ok &= act == 0 && heard > 0;                         /* (the notes sounded during the save, every note-off arrived: nothing held) */
    transport_req = 2; bt_block();
    song.g[G_CLOCK] = 0;
    {
        char what[200];
        snprintf(what, sizeof what, "a 3 s save under notes and clock with a MIDI Start in it: no overflow (%u), notes sounded (%u voice-blocks), none left on (%u), the Start after", midi_in_overflow, heard, act);
        return check(what, ok);
    }
}
static void bt_block_chain(void) { if (bt_blocks == 5u) { chain.armed = 1; transport_req = 1; } bt_block(); }
static int t_chain_waits(void)
{
    int ok = 1;
    reset();
    fill_all(5, 4);
    res_bor_host_block = bt_block_chain;
    bt_blocks = 0; bt_play_seen = 0;
    ok &= project_save_as(0, "CHAIN") == 0;
    ok &= bt_play_seen == 0 && !song.playing && transport_req == 1u;
    res_bor_host_block = bt_block;
    mix_block(bt_out, CTL);
    ok &= song.playing == 1;
    transport_req = 2; mix_block(bt_out, CTL);
    chain.armed = chain.running = 0;
    return check("the chain's start during the save waits until the borrow ends", ok);
}

int main(void)
{
    int bad = 0;
    bad |= t_refused_playing();
    bad |= t_unsaved();
    bad |= t_save_stopped();
    bad |= t_play_waits();
    bad |= t_cuts();
    bad |= t_resync_held();
    bad |= t_load_fail_keeps();
    bad |= t_timeout();
    bad |= t_midi_long_save();
    bad |= t_chain_waits();
    printf(bad ? "borrow save: FAILED\n" : "borrow save: all passed\n");
    return bad;
}
