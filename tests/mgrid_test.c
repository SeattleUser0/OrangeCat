/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The melodic step grid (branch melodic-grid, docs/orangecat/MELODIC_GRID_PLAN.md; firmware/src/ui_mgrid.c, ui_mgrid_edit.c), with OC_MGRID=1, through tests/ui_test.c's host UI:
 *   cc -O1 -w -DOC_HOME_PIC=1 -DOC_EDIT_SHEETS=1 -DOC_SHIFT_MAP=1 -DOC_STEP_PAGES=1 -DOC_ONE_AUTH=1 -DOC_MGRID=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o mgrid_test tests/mgrid_test.c -lm
 * The ten "Done when" items of the request, each by name in the lines it prints (DW1 .. DW10), plus the page, its tracks, the keys and the lights (PAGE, KEYS, LIGHTS):
 *   DW1   steps 1, 5, 9 and 13 take four notes, the steps between untouched and the cursor only where a pick put it
 *   DW2   the first four black keys reach the four pages; all 64 steps of a 64-step phrase are filled from the GRID alone; a page past LEN is inert
 *   DW3   the cursor does not move by itself: a key, a chord, a MIDI note, a MIDI chord, the cancel
 *   DW4   picking a step or a page never changes a step or a recorded note (64 steps and the pool byte for byte)
 *   DW5   replacing a step's notes is one undo, a deleted note and a deleted step come back, a recorded step gets its timed notes back
 *   DW6   a step's length (ties) and effects (velocity, accent, slide, ratchet, chance, condition, nudge) survive a replace and a pitch turn
 *   DW7   a project saved from the grid loads the same; nothing of the stage is in the record (its size, FW_API_ID)
 *   DW8   DRUM-X's grid is not touched: its keys, its LEDs, GRID is not offered on a drum track (the pictures are the render test's)
 *   DW9   the transport runs through picks, entries, a cancel, deletes and page changes
 *   DW10  recorded timing, a frozen phrase and live recording are not affected
 * Revisions:
 *   (melodic-grid) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad, nchk;
static void chk(const char *what, int ok)
{
    nchk++;
    printf("mgrid: %-150s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static uint64_t seen;                                   /* every cursor value any frame showed */
static void fr(void) { frame(); seen |= 1ull << (ui.cursor & 63u); }
static void frs(uint32_t n) { while (n--) fr(); }
static void kdown(uint32_t k) { key_down(k); fr(); }
static void kup(uint32_t k) { key_up(k); fr(); }
static void ktap(uint32_t k) { kdown(k); kup(k); }
static uint32_t wk(uint32_t p) { return key_at(0, p); }
static uint32_t bk(uint32_t p) { return key_at(1, p); }
static void midi(uint32_t note, uint32_t on) { step_midi_edge(song.sel, note, on, 0, 1); fr(); }
static void oct_minus(void) { press(B_OCTDN); seen |= 1ull << (ui.cursor & 63u); }
static void edit_tap(void) { press(B_EDIT); frs(2); }
static void edit_hold(void) { hold(B_EDIT); frs(2); }   /* held 0.8 s: undo */
static void run(uint32_t blocks) { int32_t o[2 * CTL]; while (blocks--) mix_block(o, CTL); }

static void fresh(uint32_t len)
{
    ui_power_on();
    stop_transport();
    go_title("GRID");
    frame();
    track_defaults_steps(TSEL);
    TSEL->p[P_SLEN] = (int16_t)len;
    cursor_set(0);
    mgrid_reset();
    bulk.on = 0;
    frs(3);
    seen = 1;
}
static void put_note(uint32_t i, uint32_t step, uint32_t on, uint32_t note, uint32_t vel, uint32_t len)
{
    recorded_note_t r = {.step = (uint8_t)step, .on = (uint16_t)on, .note = (uint8_t)note, .vel = (uint8_t)vel,
                         .duration = (uint16_t)len, .owner = (uint8_t)recording_owner(TSEL), .length = 1};
    recording_restore_note(TSEL, i, r);
}
static void recorded(uint32_t s, uint32_t note) { put_note(s, s, 0, note, 100, RECORD_UNIT / 2u); note_rebuild_all(); }
static void manual(uint32_t s, uint32_t note) { step_t *st = &TSEL->step[s]; st->note[0] = (uint8_t)note; st->n = 1; st->time = ST_NOTE; st->vel = 96; }

/* pick a step the way the player does: its page key if it is not shown, then its white key (down and up); the keyboard is then in NOTE */
static void pick(uint32_t step)
{
    if (step / 16u != ui.bank) ktap(bk(step / 16u));
    ktap(wk(step % 16u));
}
/* play keys together (a chord) and let them go, and settle: the keyboard is back in STEP */
static void play(const uint32_t *keys, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) kdown(keys[i]);
    for (i = 0; i < n; i++) kup(keys[i]);
    frs(2);
}
static void play1(uint32_t k) { play(&k, 1); }
/* a step entered from the grid: pick it, play the keys */
static void enter(uint32_t step, const uint32_t *keys, uint32_t n) { pick(step); play(keys, n); }
static void enter_notes_ref(uint32_t step, uint32_t key)   /* the same note on NOTES: the cursor there, the key, and the cursor put back */
{
    cursor_set((int32_t)step); frs(2);
    ktap(key); frs(2);
}
static int stage_step(void) { return mgrid_on() && !mg.stage; }
static int stage_note(void) { return mgrid_on() && mg.stage == 1u; }
static int same_steps(const step_t *a) { return !memcmp(a, TSEL->step, sizeof(step_t) * NSTEP); }

int main(void)
{
#if !OC_MGRID
    printf("mgrid: built without OC_MGRID\n");
    return 0;
#else
    static step_t snap[NSTEP];
    static recorded_note_t rsnap[RECORD_MAX];
    uint32_t i, ok;
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* PAGE: where GRID is, and on which tracks */
        uint32_t g, tr, ph;
        ui_power_on();
        for (g = 0; g < NPAGES && !str_eq(PAGES[g].title, "GRID"); g++) ;
        for (tr = 0; tr < NPAGES && !str_eq(PAGES[tr].title, "TRACKER"); tr++) ;
        for (ph = 0; ph < NPAGES && !str_eq(PAGES[ph].title, "PHRASE"); ph++) ;
        chk("PAGE    GRID is a SEQ step page after TRACKER and before PHRASE (PHRASE keeps its own page)", g < NPAGES && g == tr + 1u && ph == g + 1u && PAGES[g].fam == FAM_SEQ && PAGES[g].scope == SC_STEP && PAGES[ph].graph == GR_STEPS);
        chk("PAGE    the pitched track has it", page_visible(g));
        set_engine_of(&trk[1], ENGI_DRUMX); song.sel = 1;
        chk("PAGE    a DRUM-X track does not", !page_visible(g));
        song.sel = 0;
        ok = 1;
        for (i = 0; i < NENGINES; i++) {
            const engine_t *e;
            set_engine_of(&trk[2], i); song.sel = 2;
            e = ENGINES[eng_idx(trk[2].eng_req)];
            if (page_visible(g) != (!drum_track(&trk[2]) && !e->oneshot && !(MELODEE_SLICE && e == ENGINES[ENGI_SLICE]))) ok = 0;
            if (e->oneshot && page_visible(g)) ok = 0;
        }
        song.sel = 0;
        chk("PAGE    no engine that is a drum, a kit (one-shot) or the slice player has it; the others do", ok);
    }
    {   /* DW1: steps 1, 5, 9, 13 take four notes without visiting the steps between */
        uint32_t keys[4] = {wk(1), wk(5), wk(8), wk(10)}, st[4] = {0, 4, 8, 12}, c;
        fresh(16);
        chk("PAGE    GRID opens in STEP, the keyboard silent (song.grid 3)", stage_step() && song.grid == 3u && mgrid_track(TSEL));
        for (i = 0; i < 4u; i++) {
            pick(st[i]);
            if (i == 0) chk("DW1     a white key down and up: the step is picked and the keyboard goes to NOTE", stage_note() && ui.cursor == st[0] && song.grid == 0u);
            play1(keys[i]);
        }
        ok = 1;
        for (i = 0; i < 4u; i++) {
            const step_t *s = &TSEL->step[st[i]];
            ok &= s->n == 1u && s->note[0] == kb_map(TSEL, keys[i]) && s->time == ST_NOTE;
        }
        chk("DW1     steps 1, 5, 9 and 13 hold the four notes played", ok);
        for (ok = 1, c = 0; c < NSTEP; c++)
            if (c != 0 && c != 4 && c != 8 && c != 12 && (TSEL->step[c].n || TSEL->step[c].time != ST_REST)) ok = 0;
        chk("DW1     every step between is untouched", ok);
        chk("DW1     the cursor was only ever on the four picked steps (never walked, never advanced)", seen == ((1ull << 0) | (1ull << 4) | (1ull << 8) | (1ull << 12)));
        chk("DW1     after the last note is up the keyboard is back in STEP on the same step", stage_step() && ui.cursor == 12u);
    }
    {   /* KEYS: STEP is silent, NOTE plays, black keys */
        uint32_t k;
        fresh(16);
        ok = 1;
        for (k = 0; k < 27u; k++) {
            kdown(k);
            ok &= kb_note[k] == KB_SILENT;
            kup(k);
            if (!stage_step()) oct_minus();                /* a white key picks: the keyboard went to NOTE; leave it (nothing played) */
        }
        chk("KEYS    in STEP every key of the 27 is silent (no voice is started)", ok);
        fresh(16);
        ktap(wk(0));
        kdown(wk(3));
        ok = stage_note() && kb_note[wk(3)] != KB_SILENT && kb_note[wk(3)] == kb_map(TSEL, wk(3));
        kup(wk(3)); frs(2);
        chk("KEYS    in NOTE the next key sounds as the keyboard does", ok);
        fresh(16);
        pick(2);
        play(&(uint32_t){bk(5)}, 1);
        chk("KEYS    NOTE: a black key is a sharp (the step holds the key's note)", TSEL->step[2].n == 1u && TSEL->step[2].note[0] == kb_map(TSEL, bk(5)) && key_black(bk(5)));
        fresh(16);
        memcpy(snap, TSEL->step, sizeof snap);
        for (ok = 1, i = 4; i <= 10u; i++) {
            ktap(bk(i));
            ok &= stage_step() && ui.cursor == 0u && ui.bank == 0u && same_steps(snap);
        }
        chk("KEYS    STEP: black keys 5 to 11 do nothing (no pick, no page, no stage, no change)", ok);
        fresh(16);
        manual(3, 60);
        memcpy(snap, TSEL->step, sizeof snap);
        press(B_OCTUP); frs(2);
        chk("KEYS    OCT+ on GRID opens nothing (no sheet) and changes nothing: the note's sheet is NOTES' and TRACKER's", pop.on == 0 && !ui.home && cur_page()->graph == GR_MGRID && same_steps(snap));
        oct_minus();
        chk("KEYS    OCT- in STEP is Esc as everywhere (HOME)", ui.home);
    }
    {   /* DW2: the four pages; 64 steps from the grid alone; a page past LEN */
        uint32_t keys, p, s;
        fresh(64);
        for (ok = 1, p = 0; p < 4u; p++) {
            ktap(bk(p));
            ok &= ui.bank == p && stage_step() && ui.cursor == 0u;
        }
        chk("DW2     black keys 1 to 4 show pages 1 to 4 (LEN 64), STEP stays, the cursor stays", ok);
        keys = wk(6);
        for (s = 0; s < 64u; s++) { pick(s); play1(keys); }
        for (ok = 1, s = 0; s < 64u; s++) ok &= TSEL->step[s].n == 1u && TSEL->step[s].note[0] == kb_map(TSEL, keys);
        chk("DW2     all 64 steps filled from the GRID alone (a page key, a white key, a note each)", ok);
        fresh(20);
        memcpy(snap, TSEL->step, sizeof snap);
        ktap(bk(1));
        ok = ui.bank == 1u;
        ktap(bk(2)); ktap(bk(3));
        ok &= ui.bank == 1u && stage_step();
        chk("DW2     LEN 20: pages 3 and 4 are inert (the page stays), page 2 shows", ok);
        cursor_set(17); frs(2);
        ktap(wk(5));                                     /* step 22 > LEN */
        chk("DW2     LEN 20: a white key past LEN picks nothing and the keyboard stays in STEP", stage_step() && ui.cursor == 17u && same_steps(snap));
        ktap(wk(2));
        chk("DW2     .. a white key inside LEN (step 19) picks it", stage_note() && ui.cursor == 18u);
        oct_minus();
    }
    {   /* DW3: the cursor does not move by itself */
        uint32_t chord[3] = {wk(2), wk(4), wk(7)};
        fresh(16);
        pick(5);
        seen = 1ull << 5;
        play1(wk(3));
        chk("DW3     a key: the cursor stays on its step", ui.cursor == 5u && seen == (1ull << 5) && stage_step());
        pick(5); play(chord, 3);
        chk("DW3     a chord of three: the cursor stays, the step holds three notes", ui.cursor == 5u && seen == (1ull << 5) && TSEL->step[5].n == 3u);
        midi(64, 1); midi(64, 0); frs(2);
        chk("DW3     a MIDI note goes into the picked step (STEP stage) and the cursor stays", ui.cursor == 5u && TSEL->step[5].n == 1u && TSEL->step[5].note[0] == 64u && seen == (1ull << 5));
        midi(60, 1); midi(67, 1); midi(60, 0); midi(67, 0); frs(2);
        chk("DW3     a MIDI chord: both notes in the step, the cursor stays", ui.cursor == 5u && TSEL->step[5].n == 2u && seen == (1ull << 5));
        pick(5); kdown(wk(2));
        oct_minus();
        kup(wk(2)); frs(2);
        chk("DW3     OCT- in NOTE cancels: the cursor stays, STEP stage", ui.cursor == 5u && stage_step() && !ui.home);
    }
    {   /* DW4: picking never changes a step; paging never either */
        uint32_t s;
        fresh(64);
        for (s = 0; s < 64u; s += 3) manual(s, 50 + s % 20);
        recorded(1, 62); recorded(17, 64);
        memcpy(snap, TSEL->step, sizeof snap);
        memcpy(rsnap, recording, sizeof rsnap);
        for (s = 0; s < 64u; s++) { pick(s); oct_minus(); }
        for (i = 0; i < 4u; i++) ktap(bk(i));
        chk("DW4     every step picked (each then cancelled with nothing played), every page shown: the 64 steps byte for byte", same_steps(snap));
        chk("DW4     .. and the recorded notes byte for byte", !memcmp(rsnap, recording, sizeof rsnap));
        pick(1); chk("DW4     a recorded step picked is still recorded (flag, event)", stage_note() && (TSEL->step[1].flags & SF_RECORDED) && recording[1].vel);
        oct_minus();
        pick(3); ktap(wk(0)); frs(2);   /* a pick, then a key down and up: that IS an entry; it is DW5's */
    }
    {   /* DW5: replace, delete, undo */
        uint32_t a[3] = {wk(1), wk(3), wk(5)}, b[2] = {wk(8), wk(10)};
        step_t before[NSTEP];
        fresh(16);
        enter(4, a, 3);
        memcpy(before, TSEL->step, sizeof before);
        enter(4, b, 2);
        chk("DW5     a chord replaced by another: two notes now", TSEL->step[4].n == 2u && TSEL->step[4].note[0] == kb_map(TSEL, b[0]));
        edit_hold();
        ok = !memcmp(before, TSEL->step, sizeof before);
        chk("DW5     EDIT held: the first chord is back, byte for byte, in ONE undo", ok);
        edit_hold();
        ok = TSEL->step[4].n == 0u;
        chk("DW5     .. a second undo takes the first entry back (the replacement was one step of history, not one per note)", ok);
        fresh(16);
        enter(4, a, 3);
        memcpy(before, TSEL->step, sizeof before);
        ktap(wk(0)); oct_minus();                     /* picked, nothing played: pick 4 again */
        pick(4); oct_minus();
        mgrid_knob(1, 1); frs(2);
        edit_tap();
        chk("DW5     EDIT on the picked note of a chord deletes that note (two remain)", TSEL->step[4].n == 2u && ui.cursor == 4u);
        edit_hold();
        chk("DW5     .. and EDIT held brings it back", !memcmp(before, TSEL->step, sizeof before));
        fresh(16);
        enter(6, b, 1);
        memcpy(before, TSEL->step, sizeof before);
        pick(6); oct_minus();
        edit_tap();
        chk("DW5     EDIT on a one-note step deletes the step, the cursor stays on it", TSEL->step[6].n == 0u && TSEL->step[6].time != ST_NOTE && ui.cursor == 6u);
        edit_hold();
        chk("DW5     .. undone", !memcmp(before, TSEL->step, sizeof before));
        fresh(16);
        recorded(3, 60);
        memcpy(rsnap, recording, sizeof rsnap);
        memcpy(snap, TSEL->step, sizeof snap);
        enter(3, b, 1);
        chk("DW5     a key on a recorded step replaces it (the event goes)", !(TSEL->step[3].flags & SF_RECORDED) && !recording[0].vel);
        edit_hold();
        chk("DW5     .. EDIT held: the step and its timed note are back, byte for byte", same_steps(snap) && !memcmp(rsnap, recording, sizeof rsnap));
    }
    {   /* cancel: the step as it was */
        uint32_t k1[2] = {wk(1), wk(3)}, k2 = wk(6);
        step_t before[NSTEP];
        fresh(16);
        enter(2, k1, 2);
        memcpy(before, TSEL->step, sizeof before);
        pick(2);
        kdown(k2);
        ok = TSEL->step[2].n == 1u && ui.entry_open;
        oct_minus();
        kup(k2); frs(2);
        chk("DW5     OCT- in NOTE with a key held: the entry is undone, the step as it was", ok && same_steps(before) && stage_step() && ui.cursor == 2u);
        fresh(16);
        enter(2, k1, 2);
        enter(7, &k2, 1);                               /* two edits in the history */
        memcpy(before, TSEL->step, sizeof before);
        pick(7); kdown(k2);                             /* the same note again: the step does not change */
        oct_minus(); kup(k2); frs(2);
        chk("DW5     a cancel that changed nothing undoes nothing (the earlier edit is not taken back)", same_steps(before));
        fresh(16);
        pick(3);
        oct_minus();
        chk("DW5     a cancel with nothing played: back to STEP, the step as it was, still on the page", stage_step() && !ui.home && ui.cursor == 3u);
    }
    {   /* DW6: length and effects survive */
        step_t x;
        uint32_t keys[2] = {wk(2), wk(4)};
        fresh(16);
        manual(4, 60);
        x = TSEL->step[4];
        TSEL->step[4].flags |= SF_ACCENT | SF_SLIDE; TSEL->step[4].probability = 70; TSEL->step[4].nudge = 5; TSEL->step[4].cond = TC_1_2; TSEL->step[4].vel = 77;
        step_set_rflam(&TSEL->step[4], 3);
        TSEL->step[5].time = ST_TIE; TSEL->step[6].time = ST_TIE;     /* a note three steps long */
        x = TSEL->step[4];
        enter(4, keys, 2);
        {   step_t y = TSEL->step[4];
            y.n = x.n; memcpy(y.note, x.note, sizeof y.note);
            ok = !memcmp(&x, &y, sizeof x) && TSEL->step[4].n == 2u && TSEL->step[5].time == ST_TIE && TSEL->step[6].time == ST_TIE && step_note_length(TSEL, 4) == 3u;
        }
        chk("DW6     replacing the notes of a step with a three-step length and VEL ACC SLIDE RATCH CHANCE COND NUDGE: only the notes change, the ties stay", ok);
        memcpy(snap, TSEL->step, sizeof snap);
        pick(4); oct_minus();
        turn(EN_K3, 2);
        ok = TSEL->step[4].note[0] == snap[4].note[0] + 2u;
        TSEL->step[4].note[0] = snap[4].note[0];
        chk("DW6     KNOB 3 (pitch) moves the picked note and nothing else of the step or its ties", ok && same_steps(snap));
        {   step_t a[NSTEP];                              /* ENV is not a modifier here (it stays the ENV page's button): KNOB 4 held with it is the same coarse edit */
            fresh(16);
            enter(3, keys, 1); pick(3); oct_minus();
            turn(EN_K4, 1);
            memcpy(a, TSEL->step, sizeof a); memcpy(rsnap, recording, sizeof rsnap);
            fresh(16);
            enter(3, keys, 1); pick(3); oct_minus();
            btn_down(B_ENV); fr();
            turn(EN_K4, 1);
            btn_up(B_ENV); frs(2);
            chk("DW6     ENV held: KNOB 4 edits the length exactly as without it (ENV is not a fine-edit modifier on GRID)", !memcmp(a, TSEL->step, sizeof a) && !memcmp(rsnap, recording, sizeof rsnap));
        }
    }
    {   /* knobs */
        uint32_t chord[3] = {wk(1), wk(4), wk(7)};
        fresh(16);
        enter(3, chord, 3);
        pick(3); oct_minus();
        chk("KNOBS   KNOB 1 turned: the cursor moves, and past the page end the page follows", (turn(EN_K1, 1), ui.cursor == 4u) && (cursor_set(15), turn(EN_K1, 1), ui.cursor == 0u));
        fresh(40);
        cursor_set(15); frs(2);
        turn(EN_K1, 1);
        chk("KNOBS   .. KNOB 1 across a page: the cursor 17, page 2", ui.cursor == 16u && ui.bank == 1u);
        fresh(16);
        enter(3, chord, 3);
        pick(3); oct_minus();
        turn(EN_K2, 1);
        ok = ui.note_slot == 1u;
        turn(EN_K3, 3);
        ok &= TSEL->step[3].note[0] == kb_map(TSEL, chord[0]) && TSEL->step[3].note[1] == kb_map(TSEL, chord[1]) + 3u && TSEL->step[3].note[2] == kb_map(TSEL, chord[2]);
        chk("KNOBS   KNOB 2 picks the second note of the chord, KNOB 3 moves only that note's pitch", ok);
        fresh(40);
        cursor_set(14); frs(2);
        turn(EN_PRESET, 3);
        chk("KNOBS   PRESETS moves the cursor as on FEEL, across the page end (steps 15 + 3 = 18), and writes nothing", ui.cursor == 17u && ui.bank == 1u && !TSEL->step[17].n);
        fresh(16);
        enter(3, chord, 1);
        pick(3); oct_minus();
        turn(EN_K4, 2);
        memcpy(snap, TSEL->step, sizeof snap); memcpy(rsnap, recording, sizeof rsnap);
        fresh(16); go_title("NOTES"); frs(3);
        enter_notes_ref(3, chord[0]);
        cursor_set(3); frs(2);
        turn(EN_K3, 2);                                   /* NOTES' LEN is its KNOB 3 */
        chk("KNOBS   KNOB 4 sets the picked note's length through the shared edit path: the step and the notes are those of NOTES' LEN", same_steps(snap) && !memcmp(rsnap, recording, sizeof rsnap) && recording[0].vel + recording[1].vel + recording[2].vel + recording[3].vel > 0);
    }
    {   /* DW7: save and load */
        uint32_t keys[2] = {wk(1), wk(5)};
        fresh(32);
        enter(3, keys, 2); enter(20, keys, 1);
        recorded(9, 58);
        pick(9); oct_minus(); pick(20);
        memcpy(snap, TSEL->step, sizeof snap);
        memcpy(rsnap, recording, sizeof rsnap);
        ok = project_save(0) == 0;
        track_defaults_steps(TSEL);
        project_load(0); frs(100);
        chk("DW7     a project saved from the grid (a stage open) loads with the 64 steps byte for byte", ok && same_steps(snap));
        {   uint32_t a, b, na = 0, nb = 0, found = 0;                      /* the pool is packed by owner on a load: the events are compared as a set */
            for (a = 0; a < RECORD_MAX; a++) if (rsnap[a].vel) {
                na++;
                for (b = 0; b < RECORD_MAX; b++)
                    if (recording[b].vel && !memcmp(&rsnap[a], &recording[b], sizeof rsnap[a])) { found++; break; }
            }
            for (b = 0; b < RECORD_MAX; b++) nb += recording[b].vel != 0;
            chk("DW7     .. and the recorded notes (the same events, field for field)", na == 1u && nb == na && found == na);
        }
        chk("DW7     the stage is not in the project: after the load the keyboard is not in NOTE", !mg.stage);
        chk("DW7     the record is the size it was (13,212 B) and the module interface's id has not moved (0x126F12CC)", sizeof(project_store_t) == 13212u && FW_API_ID == 0x126F12CCu);
    }
    {   /* DW8: DRUM-X's grid */
        uint32_t k;
        ui_power_on();
        set_engine_of(&trk[0], ENGI_DRUMX); trk[0].engine = trk[0].eng_req;
        go_title_visible("NOTES"); frs(3);
        ok = grid_on() && !mgrid_on() && song.grid == 1u && !mgrid_stage_step();
        chk("DW8     a DRUM-X track's NOTES is its grid (keys are lanes), the melodic grid is not on", ok);
        memcpy(snap, TSEL->step, sizeof snap);
        ktap(wk(2));
        chk("DW8     a white key still toggles a hit at its step on DRUM-X's grid and puts the cursor there", ui.cursor == 2u && !same_steps(snap) && !mg.stage);
        for (ok = 1, k = 0; k < NPAGES; k++) if (str_eq(PAGES[k].title, "GRID") && page_visible(k)) ok = 0;
        chk("DW8     GRID is not offered on it", ok);
    }
    {   /* DW9: playback keeps going */
        uint32_t keys[2] = {wk(1), wk(5)}, seen_idx = 0, b, s;
        fresh(16);
        manual(0, 60);
        song.master_q12 = 4096;
        transport_req = 1;
        run(30);
        ok = song.playing;
        for (b = 0; b < 4u; b++) {
            pick(b * 4u + 1u); play(keys, 2); run(40);
            ok &= song.playing != 0;
            seen_idx |= 1u << (TSEL->seq_idx & 31u);
            pick(b * 4u + 2u); oct_minus(); edit_tap(); run(40);
            ok &= song.playing != 0;
            ktap(bk(0)); run(40);
            ok &= song.playing != 0;
            seen_idx |= 1u << (TSEL->seq_idx & 31u);
        }
        for (s = 0, b = 0; s < 32u; s++) b += (seen_idx >> s) & 1u;
        chk("DW9     the transport keeps running through picks, entries, a cancel, deletes and page keys, and the playhead moves", ok && b >= 2u);
        {   int32_t o[2 * CTL];
            int32_t peak = 0;
            uint32_t n;
            fresh(16);
            manual(0, 60);
            transport_req = 1; song.master_q12 = 4096;
            run(10);
            pick(0); play(keys, 2);
            for (n = 0; n < 800u; n++) {
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++) { int32_t a = o[2 * i] < 0 ? -o[2 * i] : o[2 * i]; if (a > peak) peak = a; }
            }
            chk("DW9     the step edited on the grid while playing sounds on the next pass", peak > 100);
        }
        stop_transport();
    }
    {   /* DW10: recorded timing, frozen phrase, live recording */
        uint32_t keys[1] = {wk(2)};
        recorded_note_t pool_a[RECORD_MAX], pool_b[RECORD_MAX];
        fresh(16);
        recorded(5, 62);
        memcpy(rsnap, recording, sizeof rsnap);
        memcpy(snap, TSEL->step, sizeof snap);
        pick(5); oct_minus(); ktap(bk(0)); pick(7); oct_minus();
        chk("DW10    a recorded note's onset, length and velocity are untouched by picking and paging", !memcmp(rsnap, recording, sizeof rsnap) && same_steps(snap));
        fresh(16);
        manual(4, 60);
        memcpy(snap, TSEL->step, sizeof snap);
        bank_pid[0][trk[0].pattern % NPAT] = 5;
        frz_host_frozen = 1ull << 4;
        pick(4);
        ok = ui.cursor == 4u && stage_note();
        play(keys, 1);
        edit_tap();
        turn(EN_K3, 2); turn(EN_K4, 1);
        chk("DW10    a frozen phrase: picking and moving still work; a key, EDIT and the knobs change no step", ok && same_steps(snap));
        frz_host_frozen = 0;
        oct_minus();
        {   recorded_note_t pool_c[RECORD_MAX];
            int page, det, same, n_a = 0, n_b = 0;
            for (page = 0; page < 3; page++) {                  /* live recording: the same keys at the same times on NOTES (twice: is the harness repeatable?) and on GRID */
                recorded_note_t *dst = page == 1 ? pool_b : page == 2 ? pool_c : pool_a;
                fresh(16);
                go_title(page == 1 ? "GRID" : "NOTES"); frs(3);
                song.rec = 1; song.master_q12 = 4096; transport_req = 1;
                run(30); frs(2);                                    /* (the UI has seen the transport run: the keys are the keyboard's) */
                key_down(wk(3)); run(20); fr();
                key_up(wk(3)); run(25); fr();
                key_down(bk(6)); run(15); fr();
                key_up(bk(6)); run(30); fr();
                memcpy(dst, recording, sizeof pool_a);
                if (page == 1) chk("DW10    live recording armed and playing with GRID open: the page is inert (no stage), keys record", !mg.stage && recording[0].vel);
                stop_transport(); song.rec = 0;
            }
            for (i = 0; i < RECORD_MAX; i++) { n_a += pool_a[i].vel != 0; n_b += pool_b[i].vel != 0; }
            det = !memcmp(pool_a, pool_c, sizeof pool_a);
            same = !memcmp(pool_a, pool_b, sizeof pool_a);
            if (!same) {
                for (i = 0; i < RECORD_MAX; i++)
                    if (memcmp(&pool_a[i], &pool_b[i], sizeof pool_a[i])) {
                        printf("mgrid:   event %u: NOTES on %u dur %u note %u vel %u owner %u step %u | GRID on %u dur %u note %u vel %u owner %u step %u\n", (unsigned)i,
                               pool_a[i].on, pool_a[i].duration, pool_a[i].note, pool_a[i].vel, pool_a[i].owner, pool_a[i].step,
                               pool_b[i].on, pool_b[i].duration, pool_b[i].note, pool_b[i].vel, pool_b[i].owner, pool_b[i].step);
                        break;
                    }
            }
            if (!same && !det) {                                  /* the harness does not repeat the run to the byte (the sequencer's phase): compare what was recorded without its onset */
                same = n_a == n_b && n_a >= 2;
                for (i = 0; i < RECORD_MAX && same; i++) same &= pool_a[i].vel == pool_b[i].vel && pool_a[i].note == pool_b[i].note && pool_a[i].owner == pool_b[i].owner;
            }
            chk(det ? "DW10    .. and what GRID records is, byte for byte, what NOTES records for the same keys at the same times" :
                      "DW10    .. and what GRID records is what NOTES records (the same events; onsets not compared: the harness does not repeat a run to the byte)", same && n_a >= 2);
        }
    }
    {   /* REC held, stopped: the page-reset of the other SEQ pages has nothing to reset on GRID */
        uint32_t k;
        fresh(16);
        manual(2, 60); manual(5, 64);
        TSEL->step[2].flags |= SF_ACCENT; TSEL->step[2].probability = 50; TSEL->step[5].vel = 40;
        memcpy(snap, TSEL->step, sizeof snap);
        fm1_ms += 320u;
        btn_down(B_REC);
        for (k = 0; k < 90u; k++) frame();
        btn_up(B_REC); frs(2);
        chk("DW6     REC held (stopped) on GRID resets nothing: chance, accent and velocity stay (they are other pages')", same_steps(snap));
    }
    {   /* LIGHTS */
        uint32_t k;
        ui_power_on();
        ui_leds();                                      /* (led_pos_init first; then a key map of the test's own, as test_key_lights) */
        for (k = 0; k < 41u; k++) led_pos[k] = (uint8_t)((k % FM1_NCOL) << 3 | (1u + k / FM1_NCOL));
        fresh(20);
        manual(2, 60); manual(8, 62); manual(18, 64);
        pick(2); oct_minus();
        frame(); ui_leds();
#define BRIGHT(k) (key_light(14u + (k)) == 2u)
#define DIMPL(k) ((fm1_led_dim[1][led_pos[14u + (k)] >> 3] >> (led_pos[14u + (k)] & 7u)) & 1u)
        ok = stage_step() && BRIGHT(wk(2)) && !BRIGHT(wk(8)) && !BRIGHT(wk(3)) && !BRIGHT(wk(15));
        chk("LIGHTS  STEP: the picked step's key is bright, no other is", ok);
        ok = DIMPL(wk(8)) && !DIMPL(wk(3)) && !DIMPL(wk(2));
        chk("LIGHTS  STEP: a step that holds a note is dim, an empty one dark", ok);
        ok = BRIGHT(bk(0)) && !BRIGHT(bk(1)) && DIMPL(bk(1)) && !BRIGHT(bk(2)) && !DIMPL(bk(2)) && !BRIGHT(bk(3)) && !DIMPL(bk(3));
        chk("LIGHTS  STEP: the page shown is bright, the other page inside LEN dim, pages past LEN dark", ok);
        ok = 1;
        for (k = 4; k <= 10u; k++) ok &= !BRIGHT(bk(k)) && !DIMPL(bk(k));
        chk("LIGHTS  STEP: black keys 5 to 11 are dark", ok);
        ktap(bk(1)); frame(); ui_leds();
        ok = BRIGHT(wk(0)) == 0 && !BRIGHT(wk(2)) && DIMPL(wk(2)) && BRIGHT(bk(1)) && DIMPL(bk(0)) && ui.cursor == 2u;
        for (k = 4; k < 16u; k++) ok &= !BRIGHT(wk(k)) && !DIMPL(wk(k));
        chk("LIGHTS  page 2 (LEN 20): its steps 17 to 20 are the keys that can light, the picked step 3 is not on it, keys past LEN dark", ok);
        ktap(bk(0)); ktap(wk(2)); frame(); ui_leds();
        ok = stage_note() && !BRIGHT(wk(2)) && !DIMPL(wk(8)) && !DIMPL(bk(1)) && !BRIGHT(bk(0));
        chk("LIGHTS  NOTE: the step map is gone from the keys (they show the playing layout), so the stage can be told by the keys alone", ok);
#undef BRIGHT
#undef DIMPL
    }
    printf("mgrid: %d checks, %d failed\n", nchk, nbad);
    return nbad != 0;
#endif
}
