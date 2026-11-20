/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 32) FX LATCH from the panel (firmware/src/ui_layer.c fx_latch, layer_key, layer_oct, layer_foot; jfx_perform.c perf_press, perf_latch_put). The owner's finding:
 * perf_latch_on was read (perf_press) and never set, so FX LATCH could not be turned on. FX held + G5 (key 26) toggles it now; the footer says LATCH ON / LATCH OFF.
 *   POWER    off at power-on: the footer is OCT- CLEAR, G5 LATCH OFF, each with its word (the old footer's row was too wide: ALL OFF was not drawn)
 *   KEY      FX held + G5: LATCH ON (the message, the footer, G5's light), no effect, no note; again: LATCH OFF
 *   TAP      LATCH on: a tap of REPEAT 1/16's key (G3) plays it with FX let go, a second tap ends it; two latched, LATCH OFF lets both go
 *   HELD     an effect held when LATCH comes on is latched: it plays on after its key is let go (not held by nobody), and a tap ends it
 *   OCT-     FX + OCT- with two effects latched: LATCH off, nothing latched, held or playing
 *   DUB      on HOME's DUB page FX + G5 is still LATCH (G5 is outside the DUB octave C4 .. B4)
 *   OTHERS   G5 with GLO, ARP, SEQ held does nothing of theirs and leaves LATCH alone; SCL + G5 is ROOT G (SCL's any key); G5 alone is a note
 * Run by tests/run_tests.sh. On a firmware without FX + G5 (e26960bc) it builds with the fallbacks below and fails. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#ifndef FX_LATCH_KEY                                    /* (the firmware before Round 32: nothing sets the latch; its footer KEYS HOLD: EFFECT, OCT- ALL OFF) */
#define FX_LATCH_KEY 26u
#define KC_G5 KC_COUNT
static uint32_t layer_foot(uint32_t l, khint_t *h) { memcpy(h, LAYERS[l].foot, sizeof LAYERS[l].foot); return h[2].act ? 3u : 2u; }
#endif

static int bad, nchk;
static void chk(const char *what, int ok) { nchk++; printf("fx_latch: %-150s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void blocks(uint32_t n) { int32_t o[2 * CTL]; while (n--) mix_block(o, CTL); }
static int foot_is(const char *latch)
{
    khint_t h[3];
    uint32_t n = layer_foot(LAYER_FX, h);
    return n == 2u && h[1].act && h[1].key == KC_G5 && str_eq(h[1].act, latch) && h[0].key == KC_OCTDN && str_eq(h[0].act, "CLEAR");
}
static int foot_fits(void)                              /* cv_key_row keeps every hint's word (the row from x 4 to 236, 6 px at least between hints) */
{
    khint_t h[3];
    uint32_t n = layer_foot(LAYER_FX, h), i;
    int32_t w = 6 * (int32_t)(n - 1u);
    for (i = 0; i < n; i++) w += kh_w(h[i].key, h[i].act);
    return w <= 232;
}
static void start(void)
{
    ui_power_on();
    stop_transport();
    fm1_in.notes = 0; fm1_in.buttons = 0; kb_prev = 0; kb_dubk = 0; dub_view_on = 0;
    keyboard_block();
    for (uint32_t k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = 0, trk[k].mute_arm = 0;
    go_home(); frames(100);
    song.g[G_BPM] = 120;
    ui.msg_t = 0;
}
static void tap(uint32_t k) { key_down(k); frames(48); key_up(k); frames(48); }
static void fx_g5(void) { btn_down(B_FX); frames(48); tap(FX_LATCH_KEY); btn_up(B_FX); frames(100); }
#define G3 white(1)                                     /* REPEAT 1/16 (white place 1) */
#define DS5 black(9)                                    /* CRUSH (black place 9) */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    int ok;
    chk("the keys: G5 is key 26, white, outside the DUB octave and no effect's key; G3 REPEAT 1/16, D#5 CRUSH",
        FX_LATCH_KEY == 26u && !key_black(26) && jfx_key(26) == PF_N && !((DUBK_MASK >> 26) & 1u) && jfx_key(G3) == PF_R16 && jfx_key(DS5) == PF_SCRU);

    /* ---- POWER ---- */
    chk("POWER    a fresh unit: LATCH off, nothing latched", perf_latch_on == 0 && perf_latched == 0);
    start();
    chk("POWER    power-on: LATCH off; the FX footer: OCT- CLEAR, G5 LATCH OFF (every word fits)", perf_latch_on == 0 && foot_is("LATCH OFF") && foot_fits());

    /* ---- KEY ---- */
    start();
    btn_down(B_FX); frames(48); key_down(FX_LATCH_KEY); frames(48);
    ok = perf_latch_on == 1 && msg_is("FX LATCH ON") && foot_is("LATCH ON") && foot_fits() && perf_down == 0u && perf_latched == 0u && !gates();
    ok &= ((layer_leds() >> FX_LATCH_KEY) & 1u) && ((layer_leds() >> FX_LATCH_KEY) & 1u);
    key_up(FX_LATCH_KEY); frames(48); btn_up(B_FX); frames(100);
    chk("KEY      FX + G5: LATCH ON (message, footer LATCH ON, G5 lit), no effect, no note", ok && perf_latch_on == 1);
    blocks(4);
    chk("KEY      .. and nothing plays", perf_act == 0u && perf_held == 0u);
    fx_g5();
    chk("KEY      FX + G5 again: LATCH OFF", perf_latch_on == 0 && msg_is("FX LATCH OFF") && foot_is("LATCH OFF"));

    /* ---- TAP ---- */
    start(); fx_g5();
    btn_down(B_FX); frames(48); tap(G3); btn_up(B_FX); frames(100); blocks(4);
    chk("TAP      LATCH on: a tap of G3 latches REPEAT 1/16; it plays with FX and the key let go", perf_latched == PF_BIT(PF_R16) && ((perf_act >> PF_R16) & 1u) && perf_held == 0u);
    btn_down(B_FX); frames(48); tap(G3); btn_up(B_FX); frames(100); blocks(4);
    chk("TAP      a second tap of G3: off", perf_latched == 0u && !((perf_act >> PF_R16) & 1u) && perf_latch_on == 1);
    btn_down(B_FX); frames(48); tap(G3); tap(DS5); frames(48); tap(FX_LATCH_KEY); btn_up(B_FX); frames(100); blocks(4);
    chk("TAP      two latched (G3, D#5), then G5: LATCH OFF lets both go", perf_latch_on == 0 && perf_latched == 0u && perf_act == 0u);

    /* ---- HELD ---- */
    start();
    btn_down(B_FX); frames(48); key_down(G3); frames(48); blocks(2);
    ok = ((perf_held >> PF_R16) & 1u) && ((perf_act >> PF_R16) & 1u);
    tap(FX_LATCH_KEY);
    key_up(G3); frames(48); btn_up(B_FX); frames(100); blocks(4);
    ok &= perf_latch_on == 1 && perf_held == 0u && perf_latched == PF_BIT(PF_R16) && ((perf_act >> PF_R16) & 1u);
    btn_down(B_FX); frames(48); tap(G3); btn_up(B_FX); frames(100); blocks(4);
    chk("HELD     G3 held, LATCH turned on meanwhile: REPEAT 1/16 is latched (plays on with G3 let go); a tap of G3 ends it", ok && perf_latched == 0u && perf_held == 0u && perf_act == 0u);

    /* ---- OCT- ---- */
    start(); fx_g5();
    btn_down(B_FX); frames(48); tap(G3); tap(DS5); blocks(4);
    ok = perf_latched == (PF_BIT(PF_R16) | PF_BIT(PF_SCRU)) && ((perf_act >> PF_R16) & 1u);
    press(B_OCTDN); frames(48); blocks(2);
    ok &= perf_latch_on == 0 && perf_latched == 0u && perf_held == 0u && perf_down == 0u && msg_is("PUNCH-IN: ALL OFF") && foot_is("LATCH OFF");
    btn_up(B_FX); frames(100); blocks(4);
    chk("OCT-     FX + OCT- with two latched: LATCH off, nothing latched, held or playing (ALL OFF)", ok && perf_act == 0u && perf_latch_on == 0);

    /* ---- DUB ---- */
    start();
    TSEL->p[P_DUB_ECHO] = 100;
    while (ui.home_pg != DUB_PG) press(B_HOME);
    frames(100);
    ok = dub_view_on == 1;
    fx_g5();
    chk("DUB      on HOME's DUB page FX + G5 is LATCH ON", ok && perf_latch_on == 1);

    /* ---- OTHERS ---- (each from power-on, with the layer's map open while G5 goes down) */
    {
        uint32_t sel, cur;
        start();
        btn_down(B_GLO); frames(48); key_down(FX_LATCH_KEY); frames(48);
        ok = ui.layer == LAYER_GLO;
        key_up(FX_LATCH_KEY); frames(48); btn_up(B_GLO); frames(100);
        ok &= perf_latch_on == 0 && track_solo == 0u && song.g[G_BPM] == 120 && !msg_is("FX LATCH ON");
        for (uint32_t k = 0; k < NTRK; k++) ok &= trk[k].p[P_MUTE] == 0;
        chk("OTHERS   GLO + G5: nothing (no mute, solo or tempo tap), LATCH stays off", ok);
        start();
        btn_down(B_ARP); frames(48); key_down(FX_LATCH_KEY); frames(48);
        ok = ui.layer == LAYER_PERF;
        key_up(FX_LATCH_KEY); frames(48); btn_up(B_ARP); frames(100);
        ok &= perf_latch_on == 0 && perf_scene == 0xFFu && !ui.msg_t;
        for (uint32_t k = 0; k < NTRK; k++) ok &= trk[k].mute_arm == 0 && trk[k].p[P_MUTE] == 0;
        chk("OTHERS   ARP + G5: no scene, no staged mute, LATCH stays off", ok);
        start();
        sel = song.sel; cur = lists.cur[sel % NTRK];
        btn_down(B_SEQ); frames(48); tap(FX_LATCH_KEY); btn_up(B_SEQ); frames(100);
        chk("OTHERS   SEQ + G5: the track and its slot unchanged, LATCH stays off", perf_latch_on == 0 && song.sel == sel && lists.cur[sel % NTRK] == cur);
        start();
        TSEL->p[P_ROOT] = 0;
        btn_down(B_SCL); frames(48); key_down(FX_LATCH_KEY); frames(48);
        ok = ui.layer == LAYER_SCL;
        key_up(FX_LATCH_KEY); frames(48); btn_up(B_SCL); frames(100);
        chk("OTHERS   SCL + G5: ROOT G (SCL's any key, as before), LATCH stays off", ok && perf_latch_on == 0 && TSEL->p[P_ROOT] == 7);
        start();
        key_down(FX_LATCH_KEY); frames(48);
        ok = gates() > 0u;
        key_up(FX_LATCH_KEY); frames(100);
        chk("OTHERS   G5 with no layer: a note, LATCH stays off", ok && perf_latch_on == 0);
    }
    fx_g5();
    btn_down(B_FX); frames(48); tap(G3); btn_up(B_FX); frames(100);
    ui_power_on();
    chk("POWER    LATCH on and an effect latched, then power-on again: off, nothing latched", perf_latch_on == 0 && perf_latched == 0u);

    printf("fx_latch: %d checks, %d failed\n", nchk, bad);
    return bad != 0;
}
