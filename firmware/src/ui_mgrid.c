/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The melodic step grid (branch melodic-grid; docs/orangecat/MELODIC_GRID_PLAN.md; behind OC_MGRID, core.h; it needs OC_ONE_AUTH): SEQ > GRID.
 *
 * The page: the 16 steps of the page shown as tiles (4 x 4, the step's number and its first note), over them a strip with the four pages (1 .. 4; a page past
 * LEN is dim) and the stage in a pill: STEP (outline) or NOTE (filled in the accent, with a frame in the accent round the panel). The cursor's tile is lifted and
 * framed, the step playing has a bar over it. A step past LEN is a faint empty tile. The four dials (ui_draw.c): STEP, NOTE (the picked note of the chord), PITCH, LEN.
 *
 * This file is the state and the drawing, included by ui_graph.c. The keys, the stages and the knobs are ui_mgrid_edit.c (included by ui_input.c).
 * State: mg, 4 B of UI RAM (the stage is not saved: nothing of it is in a project).
 * Revisions:
 *   (melodic-grid) first version.
 */
#if OC_MGRID
static struct { uint8_t stage, arm, played, trk; } mg;  /* stage 0 STEP, 1 NOTE; arm: a step was picked and its key is not yet let go; played: a note went down in NOTE stage; trk: the track it is for */
static __attribute__((noinline)) int mgrid_page(void) { return !ui.home && cur_page()->graph == GR_MGRID; }
/* the page shown (ui.bank, as the DRUM grid's), kept inside LEN */
static __attribute__((noinline)) uint32_t mgrid_bank(const track_t *t)
{
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    return (uint32_t)ui.bank * 16u >= len ? (len - 1u) / 16u : ui.bank;
}
/* the picture changes: the signature of what the tiles and the strip show */
static uint32_t mgrid_sig(const track_t *t)
{
    uint32_t h = 2166136261u, len = (uint32_t)t->p[P_SLEN];
    h = (h ^ steps_hash(t)) * 16777619u;
    h ^= ui.cursor * 613u + mgrid_bank(t) * 7919u + mg.stage * 104729u + len * 31u + ui.note_slot * 937u;
    if (song.playing) h ^= (t->seq_idx + 1u) * 104723u;
    return h;
}
/* a step's tile text: the first note's name ("" for none; a chord shows pips under it, a tie a line: the tile has room for a number and one name, not more) */
static void mgrid_tile_text(const step_t *st, char *b)
{
    b[0] = 0;
    if (step_on(st) && st->n) note_name(b, st->note[0]);
}
#define MG_TX 7
#define MG_TY 24
#define MG_TW 55
#define MG_TH 22
static void graph_mgrid(const track_t *t, uint16_t c)
{
    uint32_t i, base = mgrid_bank(t) * 16u, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), pages = (len + 15u) / 16u, note = mg.stage == 1u;
    int32_t playing = song.playing && t->seq_idx < len && t->seq_idx / 16u == base / 16u ? (int32_t)(t->seq_idx % 16u) : -1;
    (void)c;
    for (i = 0; i < 16u; i++) {                         /* the tiles */
        int32_t x = MG_TX + (int32_t)(i % 4u) * (MG_TW + 3), y = MG_TY + (int32_t)(i / 4u) * (MG_TH + 2);
        const step_t *st = &t->step[(base + i) % NSTEP];
        int cur, on;
        uint16_t fill;
        char n[4], v[12];
        if (base + i >= len) {                          /* past LEN: faint and inert */
            cv_rrect(x, y, MG_TW, MG_TH, 5, T_PANEL, T_SURF);
            continue;
        }
        cur = base + i == ui.cursor;
        on = step_on(st) || st->time == ST_TIE;
        fill = cur ? T_LIFT : T_PANEL;
        if (cur) cv_rrect(x - 2, y - 2, MG_TW + 4, MG_TH + 4, 6, T_ACCENT, T_SURF);
        cv_rrect(x, y, MG_TW, MG_TH, 5, fill, cur ? T_ACCENT : T_SURF);
        if ((int32_t)i == playing)
            cv_rect(x + 8, y, MG_TW - 16, 3, T_TEXT);
        fmt_int(n, (int32_t)(base + i) + 1);
        cv_text_on(x + 3, ty_y(y + MG_TH / 2, TY_LB), TY_LB, n, cur ? T_TEXT : on ? T_MID : T_DIM, fill);
        mgrid_tile_text(st, v);
        if (v[0])
            cv_text_r(x + MG_TW - 2, ty_y(y + MG_TH / 2, TY_LB), TY_LB, v, T_THEME, fill);
        if (step_on(st) && st->n > 1u) {                /* a chord: one pip a note after the first */
            uint32_t q;
            for (q = 1; q < st->n && q < 4u; q++) cv_rect(x + MG_TW - 6 - (int32_t)q * 6, y + MG_TH - 4, 4, 2, T_THEME);
        } else if (st->time == ST_TIE) {                /* a tie: the line of the note holding */
            cv_rect(x + 6, y + MG_TH - 4, MG_TW - 12, 2, T_DIM);
        }
    }
    cv_text(10, ty_y(11, TY_LB), TY_LB, "PAGE", T_MID);  /* the strip: the pages and the stage */
    for (i = 0; i < 4u; i++) {
        int32_t x = 60 + (int32_t)i * 26;
        char n[2];
        n[0] = (char)('1' + i); n[1] = 0;
        if (i >= pages) {
            cv_rrect(x, 3, 23, 17, 5, T_PANEL, T_SURF);
            continue;
        }
        cv_rrect(x, 3, 23, 17, 5, i == base / 16u ? T_ACCENT : T_LIFT, T_SURF);
        cv_text_c(x + 11, ty_y(11, TY_LB), TY_LB, n, i == base / 16u ? T_BG : T_MID, i == base / 16u ? T_ACCENT : T_LIFT);
    }
    if (note) {                                         /* NOTE: filled pill, a frame round the whole panel */
        cv_rrect(168, 3, 62, 17, 5, T_ACCENT, T_SURF);
        cv_text_c(199, ty_y(11, TY_LB), TY_LB, "NOTE", T_BG, T_ACCENT);
        cv_rect(3, 0, 234, 2, T_ACCENT); cv_rect(3, graph_h - 2, 234, 2, T_ACCENT);
        cv_rect(3, 0, 2, graph_h, T_ACCENT); cv_rect(235, 0, 2, graph_h, T_ACCENT);
    } else {                                            /* STEP: an outline */
        cv_rrect(168, 3, 62, 17, 5, T_MID, T_SURF);
        cv_rrect(169, 4, 60, 15, 4, T_SURF, T_MID);
        cv_text_c(199, ty_y(11, TY_LB), TY_LB, "STEP", T_TEXT, T_SURF);
    }
}
#endif
