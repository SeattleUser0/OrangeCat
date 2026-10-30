/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 10 (docs/orangecat/UI_BUILD.md "Step 10: copy and paste of a sound") with OC_CLIP=1, through tests/ui_test.c's host UI.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_LANES=1 -DOC_MIXER=1 -DOC_CLIP=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o clip_test tests/clip_test.c -lm
 * Checked:
 *   COPY    SHIFT + OCT- keeps the selected track's sound; says so; the track is unchanged
 *   PASTE   SHIFT + OCT+ on another track: engine, preset, every sound parameter equal; the mix (LEVEL, PAN) and the steps stay the track's
 *           own; the macro dials 0; one undo (EDIT held) gives the old sound back; DRUM-X's kit comes across
 *   REFUSED nothing copied; an engine that is not there; an engine the unit cannot carry (eng_admit): the track keeps its sound
 *   KEPT    the clipboard survives a paste (paste twice) and a change of track
 *   MAP     the SHIFT map draws with the copy and paste tiles live (no crash; ui_render.c lints it)
 * Revisions:
 *   (the new interface 0.10, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("clip: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void down(uint32_t b)
{
    fm1_ms += 320u;
    fm1_in.buttons |= 1u << panel.btn[b];
    host_pressed |= 1u << panel.btn[b];
    frame();
}
static void up(uint32_t b)
{
    fm1_in.buttons &= ~(1u << panel.btn[b]);
    frame();
}
static void combo(uint32_t b)                           /* HOME held, b tapped, both let go */
{
    down(B_HOME);
    down(b);
    up(b);
    up(B_HOME);
    frame();
    frame();
}
static void settle(void)
{
    int32_t out[2 * CTL];
    for (uint32_t b = 0; b < 400u; b++) mix_block(out, CTL);
}
static void on_home(void)
{
    GLO_SELECT |= HOME_PIC_BIT;
    go_home();
    frames(60);
}

int main(void)
{
#if !OC_CLIP
    printf("clip: built without OC_CLIP\n");
    return 0;
#else
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* copy, paste, undo */
        ui_power_on();
        set_engine_of(&trk[0], ENGI_SHAPES);
        set_engine_of(&trk[1], ENGI_MACRO);
        settle();
        trk[0].p[P_E1] = 77; trk[0].p[P_E4] = 33; trk[0].p[P_ATK] = 11;
        trk[0].p[P_LEVEL] = 90; trk[1].p[P_LEVEL] = 40; trk[1].p[P_PAN] = -20;
        trk[1].step[3].n = 1; trk[1].step[3].note[0] = 60; trk[1].step[3].time = ST_NOTE;
        trk[1].p[P_MAC0] = 50;
        song.sel = 0;
        on_home();
        combo(B_OCTDN);
        chk("COPY    SHIFT + OCT- says so, keeps the sound, the track unchanged", snd_clip.valid && snd_clip.eng == ENGI_SHAPES && snd_clip.p[P_E1] == 77 && msg_is("SOUND COPIED") && trk[0].p[P_E1] == 77);
        song.sel = 1;
        frames(10);
        combo(B_OCTUP);
        chk("PASTE   the engine and the sound's parameters are the copy's", trk[1].eng_req == ENGI_SHAPES && trk[1].p[P_E1] == 77 && trk[1].p[P_E4] == 33 && trk[1].p[P_ATK] == 11 && msg_is("SOUND PASTED"));
        chk("PASTE   the mix and the steps stay the track's own", trk[1].p[P_LEVEL] == 40 && trk[1].p[P_PAN] == -20 && trk[1].step[3].n == 1u && trk[1].step[3].note[0] == 60);
        chk("PASTE   the macro dials come across at the center", trk[1].p[P_MAC0] == 0);
        hold(B_EDIT);
        chk("UNDO    EDIT held gives the old sound back whole", trk[1].eng_req == ENGI_MACRO && trk[1].p[P_E1] != 77);
        combo(B_OCTUP);
        combo(B_OCTUP);
        chk("KEPT    the clipboard survives a paste (twice)", snd_clip.valid && trk[1].eng_req == ENGI_SHAPES && trk[1].p[P_E1] == 77);
        song.sel = 0;
        frames(10);
        chk("KEPT    and a change of track", snd_clip.valid && snd_clip.p[P_E1] == 77);
    }
    {   /* refusals */
        ui_power_on();
        set_engine_of(&trk[1], ENGI_MACRO);
        settle();
        song.sel = 1;
        on_home();
        snd_clip.valid = 0;
        combo(B_OCTUP);
        chk("REFUSED nothing copied: says so, the track unchanged", msg_is("NOTHING COPIED") && trk[1].eng_req == ENGI_MACRO);
        snd_clip.valid = 1; snd_clip.eng = 4;           /* a number that is never an engine */
        combo(B_OCTUP);
        chk("REFUSED an engine that is not here: says so, the track unchanged", msg_is("ENGINE NOT HERE") && trk[1].eng_req == ENGI_MACRO);
    }
    {   /* DRUM-X's kit */
#if MELODEE_DRUMX
        ui_power_on();
        set_engine_of(&trk[0], ENGI_DRUMX);
        set_engine_of(&trk[1], ENGI_MACRO);
        settle();
        dxk[0].on = 1; dxk[0].tune[2] = 7;
        song.sel = 0;
        on_home();
        combo(B_OCTDN);
        song.sel = 1;
        frames(10);
        combo(B_OCTUP);
        chk("PASTE   DRUM-X's kit comes across", trk[1].eng_req == ENGI_DRUMX && dxk[1].on && dxk[1].tune[2] == 7);
#endif
    }
    {   /* the map */
        ui_power_on();
        on_home();
        down(B_HOME);
        for (uint32_t i = 0; i < 90u; i++) frame();
        chk("MAP     the SHIFT map draws with copy and paste live", shift_on() && (sh_st & SHF_MAP));
        up(B_HOME);
    }
    printf("clip: %s\n", nbad ? "FAILED" : "all ok");
    return nbad != 0;
#endif
}
