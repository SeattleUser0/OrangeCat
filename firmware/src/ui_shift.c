/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 3 (docs/orangecat/UI_DESIGN.md Part 1 section 1 "HOME held: the SHIFT map", the owner's
 * decision 4; docs/orangecat/UI_BUILD.md "Step 3: the SHIFT map"): behind OC_SHIFT_MAP (core.h; it needs
 * OC_EDIT_SHEETS).
 *
 * HOME is SHIFT from its press, when the press begins with nothing modal up (no MENU, confirm, NAME, NEW SONG, popup,
 * layer or the designer: the design review's 6 and 11). Let go before 700 ms with nothing else touched: a tap, as
 * today. Held to 700 ms alone: the map over the page; let go: the MENU opens (on the release, never racing a combo).
 * Anything pressed or turned while HOME is down is a combo, and the release then does nothing:
 *   - KNOB 1..4 on HOME's pages or a sheet: a fine turn (that detent with no acceleration);
 *   - PRESETS on HOME's pages: the engine, one step a detent;
 *   - EDIT on HOME's pages or a sheet: the init confirm (as the OCT+ sheet's Init sound);
 *   - OCT- / OCT+: copy and paste a sound, not built yet (UI_BUILD.md "Left for later from step 3"): eaten, a message;
 *   - anything else does its own job as today (only the MENU is cancelled).
 * Elsewhere SHIFT's knobs, PRESETS and EDIT are eaten (NOTES and FEEL's combos are step 4's: the design review's 7).
 * State: sh_st, 1 B (not in ui.home_t0, whose bits are its press time; not in ui.pg_down, which dialogs clear).
 * Included by ui_input.c before ui_input() (it needs page_knob and the OCT statics); ui_draw.c draws the map.
 * Revisions:
 *   (the new interface 0.3, branch ui-build) first version.
 */
enum { SHF_ON = 1u, SHF_COMBO = 2u, SHF_MAP = 4u, SHF_FINE = 8u };
static uint8_t sh_st;                                   /* SHF_*: HOME is SHIFT; a combo was made; the map is shown; a
                                                         * fine turn in progress (accel_by reads it; ui_input.c declares
                                                         * it first, this is the same object) */

static int shift_on(void) { return (sh_st & SHF_ON) != 0u; }
static int shift_map_on(void) { return (sh_st & (SHF_ON | SHF_MAP)) == (SHF_ON | SHF_MAP); }
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
static void shift_combo(void)
{
    if (sh_st & SHF_MAP)
        ui.force = 1;                                   /* (the map goes: the page under it drawn whole) */
    sh_st = (uint8_t)((sh_st | SHF_COMBO) & ~SHF_MAP);
}
static int shift_clean(void)                            /* nothing modal up: a HOME press may be SHIFT */
{
    return !ui.menu && !ui.confirm && !name_on() && !new_on() && !pop.on && !ui.ly && !ui.layer && !designer_on() &&
           !smap.on;
}
static void shift_menu_open(void)                       /* (as release-2's HOME held, ui_input.c) */
{
    ui.menu = 1;
    ui.menu_sel = 0;
    ui.confirm = 0;
    name_close();
    new_close();
    ui.force = 1;
    song.seq_mode = 0;
}
/* HOME's gesture, after btn_hold (home: its result) and before anything reads it. *pressed: this frame's button edges;
 * EDIT, OCT- and OCT+ taken as combos are removed from it (their own paths never see them). The result is what the
 * caller does with HOME: BT_TAP, BT_HOLD (release-2's MENU toggle: only for a press that is not SHIFT), BT_NONE */
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
        if (st & SHF_MAP)
            ui.force = 1;
        if ((st & SHF_COMBO) || (*pressed & ~hb) || notes)
            return BT_NONE;                             /* (a control pressed on the release's own pass: a combo too) */
        if ((st & SHF_MAP) && shift_clean())            /* held alone to the map: the MENU, on the release. Never for a
                                                         * press btn_hold did not see held (a tap between two passes: lost,
                                                         * as on release-2; the diff review's 1) */
            shift_menu_open();
        return (st & SHF_MAP) ? BT_NONE : home;
    }
    if (!shift_clean()) {                               /* something modal came up under it (SHIFT+EDIT's confirm): no
                                                         * more combos, no MENU, no tap */
        if (sh_st & SHF_MAP)
            ui.force = 1;
        sh_st = SHF_ON | SHF_COMBO;
        ui.home_t0 |= 2u;
        return BT_NONE;
    }
    if (home == BT_HOLD && !(sh_st & SHF_COMBO)) {     /* 700 ms alone: the map (the MENU waits for the release) */
        sh_st |= SHF_MAP;
        ui.force = 1;
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
            else step_delete_edit();
        } else
#endif
        if (shift_here() && !cf_refused(CF_INIT_SOUND))
            confirm_open(CF_INIT_SOUND, song.sel);      /* (as the OCT+ sheet's row, ui_popup.c) */
    }
    if (*pressed & (dn | up)) {                         /* SHIFT + OCT: copy, paste (not yet): no octave, no sheet */
        oct_eat |= (uint8_t)(((*pressed & dn) ? 1u : 0u) | ((*pressed & up) ? 2u : 0u));
        ui.oct_t0 |= 2u;
        oct_deferred = 0;
        *pressed &= ~(dn | up);
        ui_message("COPY, PASTE: LATER");
    }
    return BT_NONE;
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

/* ------------------------------------------------------------------------------------------- the map --- */
static const struct { const char *ctl, *job; uint8_t combo; } SHIFT_TILE[12] = {
    {"PRESETS", "SOUND", 0}, {"ALGO", "TRACK", 0}, {"SELECT", "PAGES", 0},
    {"K1-4", "KNOBS", 0},    {"EDIT", "SHEETS", 0}, {"SEQ", "STEPS", 0},
    {"K1-4", "FINE", 1},     {"PRESETS", "ENGINE", 2}, {"EDIT", "INIT", 1},
    {"OCT-", "COPY", 3},     {"OCT+", "PASTE", 3},  {"LET GO", "MENU", 0}};
/* the map over the page (the header as the page's, its chip "SHIFT"): twelve tiles, 76 x 46, from row 34 (the
 * mockup's); a combo's tinted with a plus, dim where it does nothing (off HOME's pages and the sheets; copy and paste
 * not built yet). SELECT says ROWS on a sheet, PAGES elsewhere */
static void shift_draw(void)
{
    uint32_t i, r;
    int here = shift_here(), row = !ui.home && here;
#if OC_STEP_PAGES
    if (shift_steppg())
        row = 0;                                        /* (SELECT turns the pages there, not a sheet's rows) */
#endif
    if (ui.force)
        lcd_fill(0, H_HEAD, 240, 240 - H_HEAD, T_BG);
    draw_head();
    if (!ui.force)
        return;                                         /* (nothing on it moves: drawn once, on its force) */
    for (r = 0; r < 4u; r++) {
        cv_begin(240, 50, T_BG);
        for (i = 0; i < 3u; i++) {
            uint32_t t = r * 3u + i;
            int32_t x = 4 + 78 * (int32_t)i;
            uint32_t c = SHIFT_TILE[t].combo;
#if OC_STEP_PAGES
            int sp = shift_steppg();
#else
            int sp = 0;
#endif
            int dim = (c == 1u && !here) || (c == 2u && !ui.home && !sp) || c == 3u;
            uint16_t bg = c ? ux_mix(T_BG, T_THEME, dim ? 8 : 20) : T_QUIET;
            const char *job = t == 2u && row ? "ROWS" : SHIFT_TILE[t].job;
            if (sp && t == 7u) job = "STRETCH";         /* (NOTES and FEEL: PRESETS stretches the range, EDIT deletes it) */
            if (sp && t == 8u) job = "DELETE";
            cv_rrect(x, 2, 76, 46, 6, bg, T_BG);
            if (c) {                                    /* the plus, in the tile's corner above the words */
                cv_rect(x + 5, 6, 5, 1, dim ? T_DIM : T_THEME);
                cv_rect(x + 7, 4, 1, 5, dim ? T_DIM : T_THEME);
            }
            cv_text_c(x + 38, ty_y(18, TY_LB), TY_LB, SHIFT_TILE[t].ctl, dim ? T_DIM : c ? T_THEME : T_SEC, bg);
            cv_text_c(x + 38, ty_y(37, TY_LB), TY_LB, c == 3u ? "LATER" : job, dim && c != 3u ? T_DIM : T_TEXT, bg);
        }
        cv_blit(0, (uint32_t)(32 + 50 * (int32_t)r));
    }
}
