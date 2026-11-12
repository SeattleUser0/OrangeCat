/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* (round 24) HOME's DUB page under a held layer (firmware/src/dub_keys.h dubk_owned, seq.c keyboard_block, ui_input.c). The owner's finding, "Track 4 graying out": C#4 is DUB's KICK
 * and also the layers' fourth black key, so a KICK played with ARP still down staged MUTE 4 (at the launch point, latched), with GLO down muted track 4 at once, with FX down muted it
 * while held. The rule: while the DUB page's keys are on, the octave C4 .. B4 (keys 7 .. 18) is DUB's whatever button is held (GLO, SCL, ARP, FX, SEQ); the other keys keep the layer's
 * meaning; a layer locked open by a double tap (its map is the screen, no button is held) keeps every key; off the DUB page nothing changes.
 *   HELD     GLO, ARP (stopped and playing), FX, SCL and SEQ held with a DUB key: the trigger fires, nothing of the layer's happens (no mute, no staged mute, no punch-in, no ROOT,
 *            no track or pattern switch), also for a key tapped and let go inside one frame
 *   OTHERS   the keys outside the octave keep the layer: GLO + F#3 mutes track 1, ARP + G#3 playing stages MUTE 2, GLO + F3 solos track 1; the key lights under GLO's map: the
 *            layer's outside the octave, in it the DUB keys held
 *   ELSEWHERE on TRACKS the same presses are the layer's: GLO + C#4 mutes track 4, ARP + C#4 playing stages it, FX + C#4 is its punch-in
 *   LOCK     GLO double tapped on the DUB page: C#4 is MUTE 4
 * Run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("dub_layer_keys: %-150s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void pic_on(void)
{
    hold(B_HOME);
    ui.menu_sel = MI_HOME_PAGE;
    if (!home_pic_on()) press(B_OCTUP);
    hold(B_HOME);
    go_home();
    frames(100);
}
/* power-on, four tracks, the selected track's ECHO up; dub: on HOME's DUB page, else on TRACKS */
static void start(int dub, int playing)
{
    ui_power_on();
    stop_transport();
    ui_leds();                                          /* (its led_pos_init first, then a map of the test's own: the host's FM1_KEYMAP is empty, as ui_test.c's key lights test) */
    for (uint32_t k = 0; k < 41u; k++) led_pos[k] = (uint8_t)((k % FM1_NCOL) << 3 | (1u + k / FM1_NCOL));
    memset(dub_trig, 0, sizeof dub_trig); memset(dubk_held, 0, sizeof dubk_held);
    fm1_in.notes = 0; kb_prev = 0; kb_dubk = 0; dub_view_on = 0;
    keyboard_block();
    perf_all_off();
    for (uint32_t k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0, trk[k].mute_arm = 0;
    pic_on();
    TSEL->p[P_DUB_ECHO] = 100;
    while (ui.home_pg != (dub ? DUB_PG : 0u)) press(B_HOME);
    frames(100);
    song.playing = (uint8_t)playing;
    ui.msg_t = 0;
}
static int quiet(void)                                  /* nothing of a layer's happened to the tracks */
{
    int ok = track_solo == 0u && perf_down == 0u && kb_layer == 0u;
    for (uint32_t k = 0; k < NTRK; k++) ok &= trk[k].p[P_MUTE] == 0 && trk[k].mute_arm == 0;
    return ok;
}
static uint32_t key_led(uint32_t k) { uint8_t q = led_pos[14u + k]; return q != 0xFF && ((fm1_led[q >> 3] >> (q & 7u)) & 1u); }   /* key k's light, the bright plane */
static int quiet2(void) { return trk[2].p[P_MUTE] == 0 && trk[3].p[P_MUTE] == 0 && trk[3].mute_arm == 0 && perf_down == 0u; }   /* (tracks 3 and 4 untouched) */
#define KICK 8u                                         /* C#4: DUB's KICK, the layers' fourth black key */
int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
#if !OC_HOME_PIC
    printf("dub_layer_keys: built without OC_HOME_PIC: nothing to check\n");
    return 0;
#else
    uint32_t j;
    int ok;
    start(1, 0); j = song.sel % 4u;
    chk("the DUB page is on with track 4 in use and unmuted", ui.home_pg == DUB_PG && dub_view_on == 1 && trk_n() == NTRK && key_black(KICK) && key_place(KICK) == 3u && quiet());

    /* ---- HELD ---- */
    btn_down(B_GLO); frame(); key_down(KICK); frames(64);
    ok = dub_trig[j][2] == 127 && quiet();
    key_up(KICK); frames(32); ok &= dub_trig[j][2] == 0 && quiet();
    btn_up(B_GLO); frames(32);
    chk("HELD     GLO held + C#4 on the DUB page: KICK fires and lets go, track 4 is not muted", ok && quiet());

    start(1, 0);
    btn_down(B_GLO); frame(); key_down(KICK); key_up(KICK); frames(64); btn_up(B_GLO); frames(32);   /* (down and up between two frames: the UI sees the edge with the key already up) */
    chk("HELD     GLO held + C#4 tapped and let go inside one frame: still no mute", quiet());

    for (int playing = 0; playing < 2; playing++) {
        start(1, playing);
        btn_down(B_ARP); frame(); key_down(KICK); frames(64);
        ok = dub_trig[j][2] == 127 && quiet() && !ui.msg_t;
        key_up(KICK); frames(32); btn_up(B_ARP); frames(32);
        chk(playing ? "HELD     ARP held + C#4 while playing: KICK, no MUTE staged for the launch point (mute_arm 0), no T4 MUTE message"
                    : "HELD     ARP held + C#4 stopped: KICK, track 4 is not muted", ok && quiet() && dub_trig[j][2] == 0);
        song.playing = 0;
    }

    start(1, 1);
    btn_down(B_FX); frame(); key_down(KICK); frames(64);
    ok = dub_trig[j][2] == 127 && perf_down == 0u && kb_fxk == 0u && quiet();
    key_up(KICK); frames(32); btn_up(B_FX); frames(32);
    chk("HELD     FX held + C#4: KICK, no punch-in (MUTE T4 while held) starts", ok && quiet());
    song.playing = 0;

    start(1, 0);
    {
        int16_t root = TSEL->p[P_ROOT];
        btn_down(B_SCL); frame(); key_down(7); frames(64);                       /* C4: the lowest throw */
        ok = dub_trig[j][0] == 13 && TSEL->p[P_ROOT] == root && quiet();
        key_up(7); frames(32); btn_up(B_SCL); frames(32);
        chk("HELD     SCL held + C4: the throw at 13, ROOT stays", ok && dub_trig[j][0] == 0 && TSEL->p[P_ROOT] == root);
    }

    start(1, 0);
    {
        uint32_t sel = song.sel, cur = lists.cur[sel % NTRK];
        ok = sel != 3u;
        btn_down(B_SEQ); frame(); key_down(KICK); frames(64); key_up(KICK); frames(64);
        ok &= song.sel == sel;                                                   /* (SEQ + the fourth black key: track 4) */
        key_down(7); frames(64);
        ok &= dub_trig[j][0] == 13 && !ui.pat_key;                               /* (SEQ + C4: pattern E) */
        key_up(7); frames(64); btn_up(B_SEQ); frames(32);
        chk("HELD     SEQ held + C#4, then C4: KICK and the throw, the selected track and its pattern stay", ok && song.sel == sel && lists.cur[sel % NTRK] == cur &&
            ocl_staged_ent(sel % NTRK) == OCL_NENT && quiet());
        chk("HELD     SEQ let go after a DUB key: the DUB page stays (no SEQ tap)", ui.home && ui.home_pg == DUB_PG);
    }

    /* ---- OTHERS ---- */
    start(1, 0);
    btn_down(B_GLO); frame(); key_down(1); frames(64); key_up(1); frames(64);     /* F#3: the first black key */
    ok = trk[0].p[P_MUTE] == 1 && dub_trig[j][2] == 0;
    key_down(0); frames(64); ok &= track_solo == 1u; key_up(0); frames(64);        /* F3: SOLO 1 while held */
    btn_up(B_GLO); frames(32);
    chk("OTHERS   GLO held on the DUB page: F#3 still mutes track 1, F3 still solos it while held", ok && track_solo == 0u);
    start(1, 1);
    btn_down(B_ARP); frame(); key_down(3); frames(64); key_up(3); frames(64); btn_up(B_ARP); frames(64);   /* G#3: the second black key */
    chk("OTHERS   ARP held on the DUB page while playing: G#3 still stages MUTE 2", trk[1].mute_arm == 2u);
    song.playing = 0;

    {   /* the key lights under the layer's map: the octave shows the DUB keys held, not the layer's lights */
        start(1, 0);
        btn_down(B_GLO); frame(); key_down(3); frames(64); key_up(3); frames(64);   /* (G#3: a combo, the map opens; track 2 muted) */
        fm1_ms = 1000u; ui_leds();                      /* (a blink's lit half) */
        ok = ui.layer == LAYER_GLO && trk[1].p[P_MUTE] == 1 && key_led(1) && !key_led(3) && !key_led(KICK);   /* (F#3: track 1 sounding, lit; G#3: muted, dark; C#4: DUB's, not held) */
        key_down(KICK); frames(64); fm1_ms = 1000u; ui_leds();
        ok &= key_led(KICK) == 1u && quiet2();
        key_up(KICK); frames(64); btn_up(B_GLO); frames(64);
        chk("OTHERS   GLO's map up over the DUB page: the lights of F#3 and G#3 are the layer's (sounding, muted), C#4's is dark until it is held (KICK), track 4 stays on", ok);
    }

    /* ---- ELSEWHERE ---- */
    start(0, 0);
    btn_down(B_GLO); frame(); key_down(KICK); frames(64); key_up(KICK); frames(64); btn_up(B_GLO); frames(32);
    chk("ELSEWHERE on TRACKS GLO held + C#4 mutes track 4 as before, no DUB trigger", ui.home_pg == 0u && !dub_view_on && trk[3].p[P_MUTE] == 1 && dub_trig[j][2] == 0);
    start(0, 1);
    btn_down(B_ARP); frame(); key_down(KICK); frames(64); key_up(KICK); frames(64); btn_up(B_ARP); frames(64);
    chk("ELSEWHERE on TRACKS ARP held + C#4 while playing stages MUTE 4 as before", trk[3].mute_arm == 2u);
    song.playing = 0;
    start(0, 1);
    btn_down(B_FX); frame(); key_down(KICK); frames(64);
    ok = perf_down != 0u && (kb_fxk >> KICK) & 1u;
    key_up(KICK); frames(64); btn_up(B_FX); frames(32);
    chk("ELSEWHERE on TRACKS FX held + C#4 is its punch-in as before", ok && perf_down == 0u);
    song.playing = 0;

    /* ---- LOCK ---- */
    start(1, 0);
    btn_down(B_GLO); frame(); btn_up(B_GLO); frame(); btn_down(B_GLO); frame(); btn_up(B_GLO); frame();
    ok = ui.lock == LAYER_GLO;
    key_down(KICK); frames(64); key_up(KICK); frames(64);
    chk("LOCK     GLO double tapped on the DUB page (its map is the screen): C#4 is MUTE 4, no DUB trigger", ok && trk[3].p[P_MUTE] == 1 && dub_trig[j][2] == 0);

    printf("dub_layer_keys: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
#endif
}
