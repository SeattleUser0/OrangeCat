/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix2 item 3 (docs/orangecat/UI_BUILD.md "ui-fix2 3"): the plain project page, with OC_PROJECT_PAGE=1.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_PROJECT_PAGE=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen
 *      -Ifirmware/src -Itests -o project_page_test tests/project_page_test.c -lm
 * Checked:
 *   SAVE      SAVE tapped (from HOME, from another page) opens the PROJECT page; SAVE again goes on to the sound's USER slots
 *   NOW       the slot the music came from or was saved to is the one in use (none on a new song); it follows a save and a load
 *   UNSAVED   the page's flag: saved right after a save or a load, unsaved once the music changes (worked out once a second while the page
 *             is up and the sequencer stopped), "not saved" on a new song
 *   LOAD      over unsaved changes asks first (OCT- keeps the music, OCT+ loads); over a saved project, or an empty slot, no question
 *   SAVE HERE a used slot asks OVERWRITE, then NAME (the project's own name as the prefill), the slot in use follows
 *   NEW       a new song over unsaved changes asks first; none in use afterwards ("not saved"); RENAME in the sheet renames the slot in place
 * Revisions:
 *   (ui-fix2 3, branch ui-fix2) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("project_page: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static void settle(void) { uint32_t i; for (i = 0; i < 80u; i++) { if (i == 40u) panel_moved |= 1u; frame(); } }   /* 1.3 s with a touch of the panel in it: the unsaved flag is worked out again (only a touch brings it) */

int main(void)
{
    uint32_t i;
    setvbuf(stdout, NULL, _IONBF, 0);
    ui_power_on();
    stop_transport();
    go_home();
    press(B_SAVE);
    chk("SAVE     tapped from HOME: the PROJECT page", !ui.home && cur_page()->graph == GR_SLOTS);
    press(B_SAVE);
    chk("SAVE     tapped again: the sound's USER slots", cur_page()->graph == GR_USER);
    go_title("DELAY");
    press(B_SAVE);
    chk("SAVE     tapped from another page: the PROJECT page", cur_page()->graph == GR_SLOTS);
    chk("NOW      a new song: no slot in use", project_current() == 4u);
    settle();
    chk("UNSAVED  a new song with no notes: the page says not saved (no slot)", project_current() == 4u);
    /* two projects */
    for (i = 0; i < NTRK; i++) track_defaults_steps(&trk[i]);
    trk[0].step[2].n = 1; trk[0].step[2].note[0] = 60; trk[0].step[2].time = ST_NOTE; trk[0].step[2].vel = 96;
    project_save(0);
    chk("NOW      after a save: the slot saved to", project_current() == 0u);
    settle();
    chk("UNSAVED  right after a save: saved", pj.d == 0u);
    trk[0].step[5].n = 1; trk[0].step[5].note[0] = 64; trk[0].step[5].time = ST_NOTE; trk[0].step[5].vel = 96;
    settle();
    chk("UNSAVED  the music changed: unsaved within a second or two", pj.d == 1u);
    project_save(1);
    chk("NOW      saved to B: B is in use", project_current() == 1u);
    settle();
    chk("UNSAVED  and saved again", pj.d == 0u);
    /* LOAD */
    trk[0].step[9].n = 1; trk[0].step[9].note[0] = 67; trk[0].step[9].time = ST_NOTE; trk[0].step[9].vel = 96;   /* unsaved changes in B */
    settle();
    song.g[G_SLOT] = 1;                                 /* slot A picked */
    sheet_do("Load");
    chk("LOAD     over unsaved changes: asks (LOAD PROJECT A?)", ui.confirm == CF_LOAD_PROJ && ui.confirm_trk == 0u);
    press(B_OCTDN);
    chk("LOAD     OCT- keeps the music (nothing loaded)", ui.confirm == CF_NONE && trk[0].step[9].n == 1 && project_current() == 1u);
    sheet_do("Load");
    press(B_OCTUP);
    chk("LOAD     OCT+ loads A: its music, A in use, the change gone", ui.confirm == CF_NONE && trk[0].step[9].n == 0 && trk[0].step[2].n == 1 &&
        trk[0].step[5].n == 0 && project_current() == 0u);
    settle();
    chk("UNSAVED  a project just loaded: saved", pj.d == 0u);
    song.g[G_SLOT] = 3;                                 /* slot C picked: empty */
    sheet_do("Load");
    chk("LOAD     an empty slot: no question, EMPTY SLOT", ui.confirm == CF_NONE && msg_has("EMPTY") && project_current() == 0u);
    song.g[G_SLOT] = 2;                                 /* slot B picked, music as saved */
    sheet_do("Load");
    chk("LOAD     a saved project over saved music: no question, loads B", ui.confirm == CF_NONE && project_current() == 1u && trk[0].step[5].n == 1);
    song.g[G_SLOT] = 4; settle();
    chk("UNSAVED  browsing the slots (KNOB 1) is no edit: still saved", pj.d == 0u);
    trk[0].p[P_LEVEL] = trk[0].p[P_LEVEL];               /* (nothing) */
    song.sel = 2; settle();
    chk("UNSAVED  selecting another track is no edit: still saved", pj.d == 0u);
    song.sel = 0;
    settle();
    {   uint32_t t0 = fm1_ms; (void)t0;
        trk[0].step[13].n = 1; trk[0].step[13].note[0] = 66; trk[0].step[13].time = ST_NOTE; trk[0].step[13].vel = 96;
        { uint32_t i; for (i = 0; i < 80u; i++) frame(); }     /* no touch at all: a transfer of the web editor leaves proj_wire_u alone */
        chk("UNSAVED  with no touch of the panel the flag is not worked out again (the staging RAM stays the editor's)", pj.d == 0u);
        trk[0].step[13].n = 0;
    }
    /* SAVE HERE over a used slot */
    song.g[G_SLOT] = 1;
    sheet_do("Save here");
    chk("SAVE HERE a used slot: OVERWRITE first", ui.confirm == CF_OVR_PROJ);
    press(B_OCTDN);
    /* NEW */
    trk[0].step[11].n = 1; trk[0].step[11].note[0] = 70; trk[0].step[11].time = ST_NOTE; trk[0].step[11].vel = 96;
    settle();
    ui.proj_new = 1;
    press(B_OCTUP);
    chk("NEW      over unsaved changes: asks (START A NEW SONG?)", ui.confirm == CF_NEW_SONG);
    press(B_OCTDN);
    ui.proj_new = 0;
    chk("NEW      OCT- keeps the music", trk[0].step[11].n == 1);
    /* RENAME */
    song.g[G_SLOT] = 1;
    sheet_do("Rename");
    chk("RENAME   the sheet's Rename opens NAME for the slot", name_on());
    printf("project_page: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
}
