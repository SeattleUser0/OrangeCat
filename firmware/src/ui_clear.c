/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* ui-fix2 item 2 (docs/orangecat/UI_BUILD.md "ui-fix2 2"), behind OC_REC_CLEAR (core.h; it needs OC_STEP_PAGES): REC held with the
 * sequencer stopped is the owner's CLEAR. What it clears is what the page is about:
 *     HOME (any of its pages)   the selected track's steps and notes (the pattern it is on, as "Clear track" did)
 *     a SEQ page                the values on that page back to their defaults (FEEL, NOTES, TRACKER, DRUM HIT: the chance,
 *                               condition, ratchet, nudge, accent and slide of the 16 steps of the cursor's page, and on NOTES, TRACKER
 *                               and DRUM HIT (the drum grid) a typed step's own velocity (the lanes' accents on a drum step); PHRASE: LEN,
 *                               DIV, SWING, GATE). The project's own SEQ pages (TEMPO, CHAIN, SONG, PHRASES, GROOVE) are not reset:
 *                               they hold the project's, not a track's, and the message says so
 *     a track's FX page         the selected track's effects (SENDS, DIST, AUTO PAN, DUB, DUB 2, DUB SETUP) back to their defaults.
 *                               A SHARED effects page does nothing (the heading says SHARED; the OCT+ sheet's "Reset effects" is its
 *                               own): the message says where
 *     a GLO page                every track's steps, notes and own effects: the music of the whole project (the shared effects, the
 *                               mix, the tempo and the sounds stay)
 *     MOTION                    the selected track's automation lanes of the pattern it is on
 * While the sequencer plays, REC held is Capture as before (this file is never asked). Held 350 ms the screen starts to count
 * (three counts of 300 ms: the clear comes about 1.25 s after the press, only if REC is still down and nothing else was touched);
 * let go earlier the count stops ("KEPT"), and a press shorter than 350 ms is the tap it always was (it arms REC).
 * Every clear is undoable by one EDIT held (the levels of ui.c: undo_seal first, so a clear is a level of its own, load_begin /
 * load_end around the change); GLO's four tracks are linked levels, so one undo, one redo takes all four; a GLO that clears
 * recorded notes keeps the one note history (note_tools.c, the whole pool) and clr_follow() brings the other levels along.
 * The count is the header's message (ui_message), a toast when it is long: no new drawing. State: clr, 8 B in RAM.
 * Included by ui_input.c before ui_input().
 * Revisions:
 *   (ui-fix2 2) first version.
 */
#if OC_REC_CLEAR
enum { CLR_NONE, CLR_TRACK, CLR_PAGE, CLR_FX, CLR_PROJECT, CLR_AUTO, CLR_SHARED, CLR_NOPE };   /* SHARED and NOPE clear nothing: REC's hold says why */
#define CLR_SHOW_MS 350u                                /* held this long the count starts (a slower tap than this is swallowed) */
#define CLR_STEP_MS 300u                                /* one count */
#define CLR_COUNTS 3u                                   /* 3, 2, 1: the clear at CLR_SHOW_MS + 3 * CLR_STEP_MS */
static struct { uint32_t t0; uint8_t arm, shown, ctx, last, sel, pg; } clr;   /* sel, pg: the track and the 16-step page the count began on */

/* what a REC held would clear here (CLR_NONE: nothing modal may be up, and the page must be one of the owner's) */
static uint32_t clr_ctx(void)
{
    const page_t *pg;
    if (ui.menu || ui.confirm || name_on() || new_on() || pop.on || ui.ly || ui.layer || designer_on() || smap.on)
        return CLR_NONE;
    if (ui.home)
        return CLR_TRACK;
    pg = cur_page();
    if (pg->graph == GR_MOTION)
        return CLR_AUTO;
    if (pg->fam == FAM_GLO || pg->fam == FAM_TRK)       /* (the GLO button's first page is the MIXER, FAM_TRK: the owner's GLO) */
        return CLR_PROJECT;
    if (pg->fam == FAM_FX)
        return pg->scope == SC_GLOBAL ? CLR_SHARED : CLR_FX;
    if (pg->fam == FAM_SEQ)                             /* the track's own pages; the project's (TEMPO, CHAIN, SONG, PHRASES, GROOVE) say so at once */
        return (pg->scope == SC_STEP || pg->scope == SC_DRUMHIT || (pg->scope == SC_TRACK && pg->graph == GR_STEPS)) ? CLR_PAGE : CLR_NOPE;
    return CLR_NONE;                                    /* (EDIT, ENV, LFO, SCL, ARP ..: REC held is what it was) */
}

/* a track's own effects back to the engine's defaults: the sends, DIST, AUTO PAN, DUB and its setup (param_jfx, param_dub_setup) */
static void clr_fx_values(track_t *t)
{
    uint32_t i;
    for (i = 0; i < P_COUNT; i++)
        if (param_jfx(i) || param_dub_setup(i))
            t->p[i] = param_desc_of(eng_idx(t->eng_req), i)->def;
}
static void clr_fx(void)
{
    track_t *t = TSEL;
    char b[12] = "1 FX RESET";
    undo_seal();
    load_begin(t, UNDO_FX);
    motion_replace_track(t, &undo_last()->motion_backup);   /* (load_begin clears the track's motion: only a clear of the music does that) */
    fm1_irq_off();
    clr_fx_values(t);
    fm1_irq_on();
    load_end(t);
    undo_seal();
    b[0] = (char)('1' + song.sel % NTRK);
    ui_say("TRACK ", b);
    ui.force = 1;
}

/* a SEQ page's values: FEEL, NOTES, TRACKER, DRUM HIT: the 16 steps of the cursor's page (FEEL's values on all four, and each page's own velocity or accents:
 * see own_notes below); PHRASE: its four parameters */
static void clr_page(void)
{
    track_t *t = TSEL;
    const page_t *pg = cur_page();
    uint32_t i, base, len, own_notes;
    if (frz_locked()) return;
    if (pg->scope == SC_TRACK && pg->graph == GR_STEPS) {   /* PHRASE: LEN DIV SWING GATE */
        undo_seal();
        load_begin(t, UNDO_PAT);
        motion_replace_track(t, &undo_last()->motion_backup);
        fm1_irq_off();
        for (i = 0; i < 4u; i++)
            t->p[pg->id[i]] = param_desc_of(eng_idx(t->eng_req), pg->id[i])->def;
        fm1_irq_on();
        load_end(t);
        undo_seal();
        ui_message("PHRASE RESET");
        ui.force = 1;
        return;
    }
    if (pg->scope != SC_STEP && pg->scope != SC_DRUMHIT) return;
#if OC_MGRID
    if (pg->graph == GR_MGRID) { ui_message("NOTHING TO RESET"); return; }   /* (melodic-grid: the page shows notes and lengths, no value to put back; chance, velocity and the rest are other pages') */
#endif
    if (live_rec_sel()) { ui_message("STOP RECORDING"); return; }
    base = ui.cursor / 16u * 16u;
    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
    own_notes = pg->graph != GR_FEEL;                   /* NOTES (the roll or the drum grid), TRACKER, DRUM HIT: their own values too; FEEL's page is FEEL's alone */
    undo_seal();
    load_begin(t, UNDO_PAT);
    motion_replace_track(t, &undo_last()->motion_backup);
    fm1_irq_off();
    for (i = base; i < base + 16u && i < len; i++) {
        step_t *st = &t->step[i];
        if (!step_on(st)) continue;
        step_set_chance(st, 100u);
        step_set_cond(st, TC_NONE);
        step_set_rflam(st, 1u);
        step_set_nudge(st, 0);
        st->flags &= (uint8_t)~(SF_ACCENT | SF_SLIDE);
        /* (close-out 1c) the page's own values besides FEEL's. NOTES and TRACKER: a typed step's VELOCITY back to the default (0: 96, the
         * way a new step is made). The DRUM grid and DRUM HIT: a typed step's lane accents and velocity. FEEL: only FEEL's (above).
         * Never the notes themselves (pitch, a typed note's length in ties, the lanes' hits: HOME's clear takes those) and never a RECORDED
         * note's velocity, start or length: those are events of the pool, edited only through the edit memory (a played velocity is the
         * performance, not a setting), so a recorded step keeps them and only its FEEL values go */
        if (own_notes && !(st->flags & SF_RECORDED)) {
            if (st->hit) { st->acc = 0; st->vel = 0; }   /* (a drum step: the lanes' accents) */
            else st->vel = 0;
        }
    }
    fm1_irq_on();
    load_end(t);
    undo_seal();
    recording_resched(t);                               /* (a recorded step's notes play its nudge) */
    ui_message("PAGE RESET");
    ui.force = 1;
}

/* every track's steps, notes and own effects, one undo (see the header) */
static void clr_project(void)
{
    uint32_t k, tt = NTRK, first = 1;
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    for (k = 0; k < NTRK; k++)
        if (tt == NTRK && !frz_locked_k(k) && recording_head[recording_owner(&trk[k])] < RECORD_MAX)
            tt = k;                                     /* the first track with recorded notes: the note history's track */
    undo_levels();                                      /* (the cache RAM's levels, if it is there) */
    {   uint32_t need = 0;
        for (k = 0; k < NTRK; k++) need += !frz_locked_k(k);
        if (need > undo_nlv) { ui_message("UNDO MEMORY FULL"); return; }   /* (one undo takes all or none: a stack too small for the group clears nothing) */
    }
    undo_seal();
    if (tt < NTRK) {                                    /* one snapshot of the whole recording pool for every track */
        fm1_irq_off();
        for (k = 0; k < NTRK; k++) if (!frz_locked_k(k)) recording_stop(&trk[k]);
        if (!note_transaction_begin_track(&trk[tt])) { fm1_irq_on(); return; }   /* (says NO EDIT MEMORY; nothing cleared) */
        motion_snapshot_track(&trk[tt], &note_transaction.motion);
        note_transaction.clear = 1;
        note_transaction.group = 1;                     /* (its undo takes the levels below along: clr_follow) */
        fm1_irq_on();
    }
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        if (frz_locked_k(k)) continue;                  /* (a frozen phrase: as it was frozen) */
        load_begin(t, UNDO_FX | (k == tt ? 0u : UNDO_PAT));   /* (the note history holds the first one's steps and motion) */
        if (undo_top)
            undo_lv[undo_top - 1u].link = (uint8_t)((first ? 0u : 1u) | (tt < NTRK ? 2u : 0u) | (k == tt ? 4u : 0u));   /* bit 0: the level below is of
                                                         * this gesture; bit 1: its note history is GLO's too; bit 2: that history owns this motion */
        first = 0;
        fm1_irq_off();
        track_defaults_steps(t);
        t->nheld = 0;
        t->arp_phys = 0;
        clr_fx_values(t);
        fm1_irq_on();
        load_end(t);
    }
    if (tt < NTRK) note_transaction_end();
    undo_seal();
    ui.note_pick = 0;
    ui_message("PROJECT CLEARED");
    ui.force = 1;
}
/* (the undo side of GLO's group: clr_group_levels, clr_group_drop, clr_history_ok, undo_group_ok and clr_follow live in ui.c, beside the levels, so a build that has ui.c and not this file links) */

static void clr_fire(uint32_t ctx)
{
    uint32_t k = song.sel % NTRK;
    switch (ctx) {
    case CLR_TRACK:
        if (chain_busy()) ui_message("STOP TO EDIT"); else seq_clear_now(CF_CLEAR_TRK, k);
        break;
    case CLR_PAGE: clr_page(); break;
    case CLR_FX: clr_fx(); break;
    case CLR_PROJECT: clr_project(); break;
    case CLR_AUTO:
        if (chain_busy()) ui_message("STOP TO EDIT"); else dq_do(CF_CLEAR_MOTION, k, trk[k].pattern, 0);
        break;
    default: break;
    }
    undo_seal();                                        /* (a sound browsed next is a level of its own, not part of the clear) */
}
static const char *clr_word(uint32_t ctx)
{
    return ctx == CLR_TRACK ? "CLEARING TRACK " : ctx == CLR_PAGE ? "RESETTING PAGE " : ctx == CLR_FX ? "RESETTING FX " :
           ctx == CLR_PROJECT ? "CLEARING PROJECT " : "CLEARING AUTOMATION ";
}
/* The press edge: armed when the sequencer is stopped and nothing else is down; btn_hold's own hold (Capture) is then not asked
 * for. Called before btn_hold with this frame's button edges; clr_hold_ok() is its `hold_ok`. */
static void clr_edge(uint32_t pressed)
{
    uint32_t rb = 1u << panel.btn[B_REC];
    if (!(pressed & rb)) return;
    clr.arm = clr.shown = 0;
    if (!transport_busy() && !(fm1_in.buttons & ~rb) && !fm1_in.notes) {
        clr.ctx = (uint8_t)clr_ctx();
        if ((clr.ctx == CLR_TRACK || clr.ctx == CLR_PAGE) && frz_locked()) clr.ctx = CLR_NOPE;   /* (a frozen phrase: says so, no count) */
        clr.arm = clr.ctx != CLR_NONE && clr.ctx != CLR_SHARED && clr.ctx != CLR_NOPE;
        clr.sel = song.sel;
        clr.pg = (uint8_t)(ui.cursor / 16u);
        clr.t0 = fm1_ms;
        clr.last = 0;
    }
}
static int clr_hold_ok(void) { return !clr.arm; }
/* REC's own hold (700 ms) on a SHARED effects page with the sequencer stopped: it says where the shared effects reset, instead of
 * Capture's "NOTHING TO CAPTURE". 1: said */
static int clr_shared_say(void)
{
    uint32_t c;
    if (transport_busy()) return 0;
    c = clr_ctx();
    if ((c == CLR_TRACK || c == CLR_PAGE) && frz_locked()) { ui_message("UNFREEZE TO EDIT"); return 1; }
    if (c == CLR_NOPE) { ui_message("NOT A TRACK'S VALUES"); return 1; }
    if (c != CLR_SHARED) return 0;
    ui_message("SHARED: OCT+ HELD, RESET EFFECTS");
    return 1;
}
/* every frame, after btn_hold: the count, the clear, the cancel. REC's own gesture is consumed once the count has shown */
static void clr_poll(uint32_t pressed, uint32_t notes)
{
    uint32_t rb = 1u << panel.btn[B_REC], el, n;
    if (!clr.arm) return;
    if (!(fm1_in.buttons & rb) || (pressed & ~rb) || notes || transport_busy() || clr_ctx() != clr.ctx || song.sel != clr.sel ||
        (clr.ctx == CLR_PAGE && ui.cursor / 16u != clr.pg)) {   /* (a knob moved the track or the page: not what was held for) */
        if (clr.shown && (fm1_in.buttons & rb) == 0u) ui_message("KEPT");
        clr.arm = clr.shown = 0;
        return;
    }
    el = fm1_ms - clr.t0;
    if (el < CLR_SHOW_MS) return;
    ui.rec_t0 |= 2u;                                    /* (its release is no tap from here on) */
    clr.shown = 1;
    if (el >= CLR_SHOW_MS + CLR_COUNTS * CLR_STEP_MS) {
        clr.arm = clr.shown = 0;
        clr_fire(clr.ctx);
        return;
    }
    n = CLR_COUNTS - (el - CLR_SHOW_MS) / CLR_STEP_MS;  /* 3, 2, 1 */
    if (n != clr.last) {
        char b[40];
        uint32_t m = 0;
        const char *w = clr_word(clr.ctx);
        while (w[m] && m < 30u) { b[m] = w[m]; m++; }
        if (clr.ctx == CLR_TRACK || clr.ctx == CLR_FX) {
            b[m++] = (char)('1' + song.sel % NTRK);
            b[m++] = ' ';
        }
        b[m++] = (char)('0' + n);
        b[m] = 0;
        clr.last = (uint8_t)n;
        ui_message(b);
    }
}
#endif
