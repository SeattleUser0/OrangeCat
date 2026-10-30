/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 5 (docs/orangecat/UI_DESIGN.md Part 1 section 5 "Drums", Part 2 drum_lanes.png; docs/orangecat/UI_BUILD.md
 * "Step 5: EDIT > LANES"): DRUM-X's first lane page as the table of its eight sounds, behind OC_LANES (core.h; it needs
 * OC_STEP_PAGES).
 *
 * The page keeps release-2's four knobs (params.c: LEVEL, TUNE, DECAY, PAN of the lane ui.lane, edited through page_desc and
 * edit_param as before: nothing of the editing is new). What is new: the four cards are drawn in the knobs' cozy colors with
 * their plain names, the eight lanes are a table under them (name, LEVEL, TUNE, DECAY, PAN), the picked lane's row shows its four
 * numbers in the knobs' colors and the other rows their bars (LEVEL from the left, TUNE, DECAY and PAN from the middle), and
 * PRESETS picks the lane (a key struck on the track does too, as release-2). The kit's values are read with dxl_value (params.c).
 * Included by ui_graph.c. State: none (the lane is ui.lane).
 * Revisions:
 *   (the new interface 0.5, branch ui-build) first version.
 */
#if OC_LANES
static int lanes_page(void) { return !ui.home && cur_page()->graph == GR_LANES && MELODEE_DRUMX && TSEL->eng_req == ENGI_DRUMX; }
static const uint8_t LANES_ID[4] = {DXL_LEVEL, DXL_TUNE, DXL_DECAY, DXL_PAN};

/* what the page shows changes: the eight lanes' four values and the picked lane */
static uint32_t lanes_sig(void)
{
    uint32_t h = 2166136261u, l, c;
    for (l = 0; l < NLANE; l++)
        for (c = 0; c < 4u; c++)
            h = (h ^ (uint32_t)(dxl_value(song.sel, l, LANES_ID[c]) + 128)) * 16777619u;
    return h ^ ui.lane * 7919u;
}
/* a bar in a cell 44 px wide: LEVEL (0 .. 127) from the left, the others (-64 .. 63) from the middle */
static void lanes_bar(int32_t x, int32_t y, uint32_t c, int32_t v, uint16_t col, uint16_t bg)
{
    cv_rect(x, y + 5, 44, 3, T_LINE);
    if (c == 0u) {
        cv_rect(x, y + 5, v * 44 / 127, 3, col);
        return;
    }
    if (v >= 0)
        cv_rect(x + 22, y + 5, v * 22 / 63, 3, col);
    else
        cv_rect(x + 22 + v * 22 / 64, y + 5, -v * 22 / 64, 3, col);
    cv_rect(x + 21, y + 3, 2, 7, T_MID);
    (void)bg;
}
/* the table, on the panel draw_graph made (cv_oy 0, from x 3, graph_h 124 high): eight rows of 15 px */
static void graph_lanes(const track_t *t, uint16_t c)
{
    uint32_t l, k;
    (void)c;
    for (l = 0; l < NLANE; l++) {
        int32_t y = 2 + (int32_t)l * 15;
        int sel = l == (ui.lane & 7u);
        uint16_t row = sel ? T_LIFT : T_SURF;
        if (sel)
            cv_rrect(5, y, 230, 14, 4, T_LIFT, T_SURF);
        cv_text_on(10, ty_y(y + 7, TY_LB), TY_LB, drum_lane_abbr(t, l), sel ? T_TEXT : T_MID, row);
        for (k = 0; k < 4u; k++) {
            int32_t x = 44 + (int32_t)k * 48, v = dxl_value(song.sel, l, LANES_ID[k]);
            if (sel) {
                char b[12];
                const char *unit;
                int16_t *vp;
                const param_desc_t *d = dxl_desc(song.sel, l, LANES_ID[k], k, &vp);
                param_format(d, v, b, &unit);
                cv_text_on(x, ty_y(y + 7, TY_LB), TY_LB, b, T_COZY(k), row);
            } else {
                lanes_bar(x, y, k, v, ux_mix(T_SURF, T_COZY(k), 70), row);
            }
        }
    }
}
#endif
