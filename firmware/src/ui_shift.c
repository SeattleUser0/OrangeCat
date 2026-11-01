/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 3 (docs/orangecat/UI_DESIGN.md Part 1 section 1 "HOME held: the SHIFT map", the owner's
 * decision 4; docs/orangecat/UI_BUILD.md "Step 3: the SHIFT map"): behind OC_SHIFT_MAP (core.h; it needs
 * OC_EDIT_SHEETS).
 *
 * HOME is SHIFT from its press, when the press begins with nothing modal up (no MENU, confirm, NAME, NEW SONG, popup,
 * layer or the designer: the design review's 6 and 11). Let go before 700 ms with nothing else touched: a tap, as
 * today. Held to 700 ms alone: the MENU opens, HOME still down (release-2's own path: btn_hold's BT_HOLD and page_tap's
 * toggle), and the release does nothing more. Anything pressed or turned while HOME is down before that is a combo, and
 * then no MENU opens for that press:
 *   - KNOB 1..4 on HOME's pages or a sheet: a fine turn (that detent with no acceleration);
 *   - PRESETS on HOME's pages: the engine, one step a detent; on NOTES and FEEL the selection stretched;
 *   - EDIT on HOME's pages or a sheet: the init confirm (as the OCT+ sheet's Init sound); on NOTES and FEEL a delete;
 *   - OCT- / OCT+: copy and paste (a sound with OC_CLIP, a selection with OC_CLIP_SEL), else eaten with a message;
 *   - anything else does its own job as today (only the MENU is cancelled).
 * Elsewhere SHIFT's knobs, PRESETS and EDIT are eaten. A combo begun after the MENU opened is the MENU's: its knobs are
 * read when HOME is let go (ui-fix 6, October 31, 2026: the owner withdrew the map of step 3, which had waited for a
 * combo at any time while HOME was held; now the combo window is 700 ms from HOME's press).
 * State: sh_st, 1 B (not in ui.home_t0, whose bits are its press time; not in ui.pg_down, which dialogs clear).
 * Included by ui_input.c before ui_input() (it needs page_knob and the OCT statics).
 * Revisions:
 *   (the new interface 0.3, branch ui-build) first version, with the map.
 *   (ui-fix 6, branch ui-fix) the map and its state gone; the MENU at 700 ms held.
 */
enum { SHF_ON = 1u, SHF_COMBO = 2u, SHF_FINE = 8u };
_Static_assert(SHF_FINE == 8u, "accel_by (ui_input.c) reads sh_st & 8u for the fine turn");
static uint8_t sh_st;                                   /* SHF_*: HOME is SHIFT; a combo was made; a fine turn in
                                                         * progress (accel_by reads it; ui_input.c declares it first,
                                                         * this is the same object) */

static int shift_on(void) { return (sh_st & SHF_ON) != 0u; }
/* where the combos act: HOME's pages and a sheet's row (elsewhere they are eaten) */
#if OC_STEP_PAGES
/* (the new interface 0.4, UI_BUILD.md step 4) NOTES (melodic) and FEEL: the combos act here too: + a knob a fine turn, + PRESETS
 * stretches the selected range, + EDIT deletes it (FEEL: back to the defaults); copy and paste are LATER */
static int shift_steppg(void) { return feel_page() || notes_page(); }
#endif
static int shift_here(void)
{
#if OC_STEP_PAGES
    if (shift_steppg())
        return 1;
#endif
    return ui.home || (!page_over && sh_row(ui.page));
}
static void shift_combo(void) { sh_st |= SHF_COMBO; }
static int shift_clean(void)                            /* nothing modal up: a HOME press may be SHIFT */
{
    return !ui.menu && !ui.confirm && !name_on() && !new_on() && !pop.on && !ui.ly && !ui.layer && !designer_on() &&
           !smap.on;
}
/* HOME's gesture, after btn_hold (home: its result) and before anything reads it. *pressed: this frame's button edges;
 * EDIT, OCT- and OCT+ taken as combos are removed from it (their own paths never see them). The result is what the
 * caller does with HOME: BT_TAP (let go alone before 700 ms), BT_HOLD (700 ms held with no combo made: release-2's
 * MENU toggle), BT_NONE. The pass's combos are handled first, so an edge arriving on the hold's own pass makes a combo
 * and no MENU (the design review's 1) */
static uint32_t shift_input(uint32_t home, uint32_t *pressed, uint32_t notes)
{
    uint32_t hb = 1u << panel.btn[B_HOME], eb = 1u << panel.btn[B_EDIT], dn = 1u << panel.btn[B_OCTDN],
             up = 1u << panel.btn[B_OCTUP], down = (fm1_in.buttons & hb) != 0u;
    if (*pressed & hb)                                  /* a press begun with nothing modal up and nothing else held
                                                         * (a button or key down before HOME: release-2's HOME, the diff
                                                         * review's 2) */
        sh_st = shift_clean() && !(fm1_in.buttons & ~hb) && !fm1_in.notes ? SHF_ON : 0u;
    if (!shift_on())
        return home;
    if (!down) {                                        /* HOME let go */
        uint32_t st = sh_st;
        sh_st = 0;
        if ((st & SHF_COMBO) || (*pressed & ~hb) || notes)
            return BT_NONE;                             /* (a control pressed on the release's own pass: a combo too) */
        return home;                                    /* a tap (BT_TAP; after the MENU opened btn_hold says BT_NONE) */
    }
    if (!shift_clean()) {                               /* something modal came up under it (the MENU at 700 ms,
                                                         * SHIFT+EDIT's confirm): no more combos, no tap */
        sh_st = SHF_ON | SHF_COMBO;
        ui.home_t0 |= 2u;
        return BT_NONE;
    }
    if ((*pressed & ~hb) || notes)
        shift_combo();
    if (*pressed & eb) {                                /* SHIFT + EDIT: init (the confirm), never EDIT's own jobs */
        *pressed &= ~eb;
        ui.edit_t0 |= 2u;
#if OC_STEP_PAGES
        if (shift_steppg()) {                           /* (the new interface 0.4) the selection, or the step, deleted */
            if (feel_page()) feel_reset(1);
            else if (bulk.on) notes_bulk_delete();
            else if (chain_busy()) ui_message("STOP TO EDIT");   /* (as EDIT tapped) */
            else step_delete_edit();
        } else
#endif
        if (shift_here() && !cf_refused(CF_INIT_SOUND))
            confirm_open(CF_INIT_SOUND, song.sel);      /* (as the OCT+ sheet's row, ui_popup.c) */
    }
    if (*pressed & (dn | up)) {                         /* SHIFT + OCT: copy, paste (a sound with OC_CLIP): no octave, no sheet */
        uint32_t oct_now = *pressed & (dn | up);
        oct_eat |= (uint8_t)(((*pressed & dn) ? 1u : 0u) | ((*pressed & up) ? 2u : 0u));
        ui.oct_t0 |= 2u;
        oct_deferred = 0;
        *pressed &= ~(dn | up);
#if OC_CLIP
        if (!shift_steppg() && shift_here()) {          /* (the new interface 0.10) a sound */
            if (oct_now & dn) sound_copy();
            else if (oct_now & up) sound_paste();
        } else
#if OC_CLIP_SEL
        if (shift_steppg()) {                           /* (the new interface 0.11) NOTES: the selected notes; FEEL: a step's feel */
            if (oct_now & dn) { if (feel_page()) feel_copy(); else notes_copy(); }
            else if (oct_now & up) { if (feel_page()) feel_paste(); else notes_paste(); }
        } else
#endif
#endif
        ui_message("COPY, PASTE: LATER");
    }
    return home == BT_HOLD && !(sh_st & SHF_COMBO) ? BT_HOLD : BT_NONE;   /* 700 ms held with no combo: the MENU (page_tap) */
}
/* SHIFT + KNOB k: on HOME's pages or a sheet a fine turn (no acceleration for this detent); elsewhere eaten. 1: taken */
static int shift_knob(uint32_t k, int32_t s)
{
    if (!shift_on())
        return 0;
    shift_combo();
    if (!shift_here())
        return 1;
    sh_st |= SHF_FINE;                                  /* (accel_by: no acceleration for this detent; RAM only) */
#if OC_STEP_PAGES
    if (!steppg_knob(k, s))                             /* (the new interface 0.4: NOTES' and FEEL's knobs) */
#endif
    page_knob(k, s);
    sh_st &= (uint8_t)~SHF_FINE;
    return 1;
}
/* SHIFT + PRESETS: on HOME's pages the engine, one step a read of the encoder (a preview committed first; a refusal says why, as
 * select_engine does); elsewhere eaten. 1: taken */
static int shift_presets(int32_t s)
{
    if (!shift_on() || !s)
        return 0;
    shift_combo();
    if (ui.home) {
        browse_commit();
        select_engine(eng_step(TSEL->eng_req, s > 0 ? 1 : -1));
    }
#if OC_STEP_PAGES
    else if (shift_steppg())
        bulk_stretch(s > 0 ? 1 : -1);                   /* (the new interface 0.4) the range's end a step */
#endif
    return 1;
}
/* ALGO, SELECT turned while HOME is SHIFT: their own jobs, but a combo (no MENU) */
static void shift_turned(int32_t s)
{
    if (s && shift_on())
        shift_combo();
}

