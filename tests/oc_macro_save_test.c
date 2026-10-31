/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 1a (docs/orangecat/UI_BUILD.md): HOME's knob macros through the firmware's own saves and loads
 * (project.c, upreset.c, ui.c on tests/ui_test.c's stubs), built with OC_HOME_PIC=1.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o oc_macro_save_test tests/oc_macro_save_test.c -lm
 * Until the Release 3 format revision a save commits the macros into their targets (and stores them as 0) and every load
 * sets them to 0 (UI_BUILD.md 1a). Checked:
 *   PROJECT   macros turned on SHAPES and MACRO, the project saved and loaded: every parameter is what the interrupt was
 *             putting on, the macros 0; it sounds the same
 *   TEMPLATE  the same through the template
 *   USER      the same through a user sound
 *   OLD       a project record with values on P_RESERVED33 .. 36 (an old format's inert effect values) loads with the
 *             macros at 0
 *   LOADS     a factory preset load, INIT and an engine change set the four to 0
 *   EDIT      HOME's knob on a macro: track_desc gives the knob's name and -64 .. 63 (the knob's edit path itself: 1c's UI tests)
 * Revisions:
 *   (the new interface 0.1, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("oc_macro_save: %-96s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
/* the audio side run until every engine asked for is the one sounding (an engine switch fades over some blocks) */
static void settle(void)
{
    int32_t out[2 * CTL];
    for (uint32_t b = 0; b < 400u; b++) {
        uint32_t k, done = 1;
        for (k = 0; k < NTRK; k++) done &= trk[k].engine == trk[k].eng_req;
        if (done) return;
        mix_block(out, CTL);
    }
}
/* what the interrupt puts on this block, for track k (a block's start by hand, read, the block's end) */
static void sounding(uint32_t k, int16_t *out)
{
    settle();
    mac_poll();
    if (thr_out) throw_on(CTL);
    memcpy(out, trk[k].p, sizeof trk[k].p);
    if (thr_on) throw_off();
}
static int same_but_macros(const int16_t *a, const int16_t *b)
{
    for (uint32_t i = 0; i < P_COUNT; i++)
        if (!(i >= P_MAC0 && i <= P_MAC3) && a[i] != b[i]) {
            printf("oc_macro_save:   id %u: %d, want %d\n", (unsigned)i, a[i], b[i]);
            return 0;
        }
    return 1;
}
static int macros_zero(const track_t *t)
{
    return !t->p[P_MAC0] && !t->p[P_MAC0 + 1] && !t->p[P_MAC0 + 2] && !t->p[P_MAC0 + 3];
}

int main(void)
{
    int16_t want0[P_COUNT], want1[P_COUNT], got[P_COUNT];
    setvbuf(stdout, NULL, _IONBF, 0);

    ui_power_on();
    set_engine_of(&trk[0], ENGI_SHAPES);
    apply_preset_to(&trk[0], 5);
    set_engine_of(&trk[1], ENGI_MACRO);
    apply_preset_to(&trk[1], 2);
    trk[0].p[P_MAC0] = -40; trk[0].p[P_MAC0 + 2] = 63; trk[0].p[P_MAC0 + 3] = 21;
    trk[1].p[P_MAC0 + 3] = -64;
    sounding(0, want0);
    sounding(1, want1);
    chk("PROJECT  (the macros move their targets before the save)", memcmp(want0, trk[0].p, sizeof want0) != 0 &&
        memcmp(want1, trk[1].p, sizeof want1) != 0);
    project_save(0);
    apply_preset_to(&trk[0], 0);
    apply_preset_to(&trk[1], 0);
    project_load(0);
    chk("PROJECT  SHAPES: every parameter loaded is what the interrupt put on; the macros 0",
        same_but_macros(trk[0].p, want0) && macros_zero(&trk[0]));
    chk("PROJECT  MACRO: the same", same_but_macros(trk[1].p, want1) && macros_zero(&trk[1]));
    sounding(0, got);
    chk("PROJECT  .. and loaded it sounds the same (nothing armed, nothing put on)", same_but_macros(got, want0) &&
        !(thr_out & MAC_ARM));

    trk[0].p[P_MAC0 + 3] = -30;
    sounding(0, want0);
    template_save();
    apply_preset_to(&trk[0], 1);
    template_load();
    chk("TEMPLATE SHAPES through the template: as it sounded, the macros 0", same_but_macros(trk[0].p, want0) &&
        macros_zero(&trk[0]));

    trk[1].p[P_MAC0 + 3] = 50;
    sounding(1, want1);
    track_select(1);
    up_store(6, "MACROS");
    apply_preset_to(&trk[1], 0);
    up_load(6);
    {   /* (a user sound keeps the sound, not the track's own: compare the sound's parameters) */
        int ok = macros_zero(&trk[1]);
        static const uint8_t SOUND[] = {P_ATK, P_DEC, P_SUS, P_REL, P_E0, P_E0 + 1, P_E0 + 2, P_E0 + 3, P_E0 + 4, P_E0 + 5,
                                        P_E0 + 6, P_E0 + 7};
        for (uint32_t i = 0; i < sizeof SOUND; i++) ok &= trk[1].p[SOUND[i]] == want1[SOUND[i]];
        chk("USER     MACRO through a user sound: the sound as it sounded, the macros 0", ok);
    }
    track_select(0);

    {   /* OLD: inert values on the four ids in a record */
        static project_t p;
        project_capture(&p);
        for (uint32_t k = 0; k < NTRK; k++)
            for (uint32_t i = 0; i < 4u; i++) p.t[k].p[P_RESERVED33 + i] = (int16_t)(17 * (int)(i + 1u) - 30);
        project_restore_runtime(&p);
        chk("OLD      values on P_RESERVED33 .. 36 in a record load as macros at 0", macros_zero(&trk[0]) && macros_zero(&trk[1]) &&
            macros_zero(&trk[2]) && macros_zero(&trk[3]));
    }
    {   /* LOADS */
        int ok = 1;
        trk[0].p[P_MAC0] = 9; apply_preset_to(&trk[0], 3); ok &= macros_zero(&trk[0]);
        trk[0].p[P_MAC0 + 1] = -9; set_engine_of(&trk[0], ENGI_MACRO); ok &= macros_zero(&trk[0]);
        trk[0].p[P_MAC0 + 3] = 12; sound_init_of(&trk[0], trk[0].eng_req); ok &= macros_zero(&trk[0]);   /* (INIT SOUND, ui_input.c) */
        chk("LOADS    a factory preset, an engine change, INIT: the macros 0", ok);
    }
    {   /* EDIT: the descriptor HOME's knob edits through */
        const param_desc_t *d;
        set_engine_of(&trk[0], ENGI_SHAPES);
        d = track_desc(&trk[0], P_MAC0 + 2);
        chk("EDIT     SHAPES' third knob: TONE, -64 .. 63, default 0", d && str_eq(d->label, "TONE") && d->min == -64 &&
            d->max == 63 && d->def == 0);
        d = track_desc(&trk[0], P_MAC0 + 1);
        chk("EDIT     a plain knob's P_MAC keeps TP[]'s descriptor (the id is not a macro there)", d == &TP[P_MAC0 + 1]);
        chk("EDIT     TP[] (the stores' and the editor's view) unchanged: 0 .. 127, no label",
            TP[P_MAC0].min == 0 && TP[P_MAC0].max == 127 && !TP[P_MAC0].label[0]);
    }
    printf("oc_macro_save: %d failed\n", nbad);
    return nbad != 0;
}
