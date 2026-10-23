/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Popups (docs/design: mock/r6_popups, README "Popups"), over the page dimmed (ui_draw.c: drawn under cv_dim).
 * An action sheet: what can be done with the page (OCT+ held; a "..." in the header says there is one) or with the
 * thing its cursor is on: a card from the bottom, its rows; KNOB 2 the row, KNOB 1 a row's value, OCT+ does it (a
 * destructive one asks its question first), OCT- closes.
 * A picker: a knob whose value is a name from a list (a wave, a source, a division; not a list of numbers) turning
 * on a page: the names
 * around its value in a card under its ring; it goes ST_KNOB_MS after the last turn, on OCT- or when another knob
 * turns. The knob edits as ever; the page under it waits. Included by ui_draw.c */
typedef struct {
    const char *label;
    void (*value)(char *b);                             /* its value at the right (0: none) */
    void (*turn)(int32_t s);                            /* KNOB 1 changes it (0: none) */
    void (*act)(void);                                  /* OCT+ does it (0: OCT+ steps the value) */
    uint8_t cf;                                         /* the question it asks first (CF_*), 0 none; with act: act
                                                         * asks it itself (after its checks), the row drawn red;
                                                         * SR_RED: no question, red (a destructive act) */
} sheet_row_t;
#define SR_RED 0xFFu
enum { POP_NONE, POP_SHEET, POP_PICK, POP_LIST, POP_FILES };   /* (Orange Cat 0.6.1: FILES, the card's browser) */
#define POP_X BOX_X                                     /* a sheet (large type): 236 wide, its title row 34, its rows
                                                         * 32 px, 34 apart (ui_list.c row_draw), five at most shown */
#define POP_W BOX_W
#define POP_ROW 34
#define POP_VIS 5
#define POP_PICK_W 116                                  /* a picker: up to five names, 28 px each; two whole columns */
static struct {
    uint8_t on, n, sel, col;
    char title[24], sub[20];
    const sheet_row_t *rows;
    uint32_t t, sig;
} pop;

static void confirm_open(uint32_t kind, uint32_t trk);  /* ui_input.c */
static void edit_param(uint32_t slot, int32_t steps);
static void jam_take(void);
static int name_on(void);
static int new_on(void);
static void menu_words(char *d, const char *s, uint32_t n);   /* ui_menu.c */
static uint32_t slot_kind(void);                        /* ui_slots.c */
static void step_delete_edit(void);                     /* ui_input.c */
static int live_rec_sel(void);
static void slot_save_sheet(void);
static int track_remove(uint32_t k);                    /* tracks.c */
static void track_add(void);

/* (Orange Cat 0.6.1) the card's browser (card_browse.c, included later by usb_files.c); stand-ins where it is not in
 * (host tests without the card side) */
#ifdef FRZ_AUDIO
static void fb_open(uint32_t k);
static void fb_close(void);
static int fb_on(void);
static int fb_back(void);
static int fb_ok(void);
static int fb_pack_done(void);                          /* (Orange Cat 0.8.0) */
static void fb_move(int32_t d);
static const char *fb_where(void);
static int fb_row(uint32_t i, char *name, uint32_t nlen, char *val, uint32_t vlen);
static uint32_t fb_sig(void);
static uint32_t fb_sel_now(void);
#else
static void fb_open(uint32_t k) { (void)k; }
static void fb_close(void) {}
static int fb_on(void) { return 0; }
static int fb_back(void) { return 1; }
static int fb_ok(void) { return 1; }
static int fb_pack_done(void) { return 0; }
static void fb_move(int32_t d) { (void)d; }
static const char *fb_where(void) { return "Card"; }
static int fb_row(uint32_t i, char *name, uint32_t nlen, char *val, uint32_t vlen)
{ (void)nlen; (void)vlen; name[0] = val[0] = 0; if (i) return 0; str_cpy(name, "No card side", 16); return 1; }
static uint32_t fb_sig(void) { return 0; }
static uint32_t fb_sel_now(void) { return 0; }
#endif
static void pop_close(void)
{
    if (pop.on == POP_FILES && fb_on()) fb_close();
    if (pop.on) {
        pop.on = POP_NONE;
        ui.force = 1;
    }
}
static void sheet_open(const char *title, const char *sub, const sheet_row_t *rows, uint32_t n)
{
    str_cpy(pop.title, title, sizeof pop.title);
    str_cpy(pop.sub, sub, sizeof pop.sub);
    pop.rows = rows;
    pop.n = (uint8_t)n;
    pop.sel = 0;
    pop.on = POP_SHEET;
    ui.force = 1;
}

/* ----------------------------------------------------- the sheets --- */
/* the sound's (Stage, the sound pages) */
static void ps_save(void)
{
    uint32_t i;
    for (i = 0; i < NPAGES && PAGES[i].graph != GR_USER; i++)
        ;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
    slot_save_sheet();                                  /* (OCT+ then: its slot's sheet on Save here) */
}
static void ps_fav_v(char *b) { str_cpy(b, preset_favorite() ? "On" : "Off", 8); }
static void ps_fav(void) { preset_mark(!preset_favorite()); }
static void ps_undo(void) { undo_step(0); }
static const sheet_row_t SHEET_SOUND[] = {
    {"Init sound", 0, 0, 0, CF_INIT_SOUND}, {"Save as user\x85", 0, 0, ps_save, 0}, {"Favourite", ps_fav_v, 0, ps_fav, 0},
    {"Undo last load", 0, 0, ps_undo, 0}, {"Sound designer", 0, 0, designer_open, 0}};
/* Stage's: the sound's, then the tracks: the one added (the next), the one removed (the selected: its question) */
static void ps_trk_v(char *b, uint32_t k, int none)
{
    if (none) {
        str_cpy(b, "-", 4);
        return;
    }
    b[0] = 'T';
    b[1] = (char)('1' + k % NTRK);
    b[2] = 0;
}
static void ps_add_v(char *b) { ps_trk_v(b, trk_n(), trk_n() >= NTRK); }
static void ps_remove_v(char *b) { ps_trk_v(b, song.sel, trk_n() < 2u); }
static const sheet_row_t SHEET_STAGE[] = {
    {"Init sound", 0, 0, 0, CF_INIT_SOUND}, {"Save as user\x85", 0, 0, ps_save, 0}, {"Favourite", ps_fav_v, 0, ps_fav, 0},
    {"Undo last load", 0, 0, ps_undo, 0}, {"Sound designer", 0, 0, designer_open, 0},
    {"Add track", ps_add_v, 0, track_add, 0}, {"Remove track", ps_remove_v, 0, 0, CF_DEL_TRACK}};
/* (Orange Cat 0.4, build doc 4c) the song's (SONG, OCT+ held): rows in and out, the cell's chain copied, the song
 * cleared, the pool's patterns nothing names emptied (oc_lists.c) */
static void ocl_insert(uint32_t k, uint32_t e);
static void ocl_dup(uint32_t k, uint32_t e);
static int ocl_copy(uint32_t k, uint32_t e);
static int ocl_song_any(void);
static int ocl_chain_any(uint32_t c);
static void ocl_row_insert(uint32_t r);
static void ocl_row_delete(uint32_t r);
static int ocl_chain_clone(uint32_t r, uint32_t k);
static void ps_row_insert(void) { ocl_row_insert(ui.song_row); ui_message("ROW INSERTED"); }
static void ps_row_delete(void) { ocl_row_delete(ui.song_row); ui_message("ROW DELETED"); }
static void ps_clone(void)                             /* the cell's chain: a copy of its own, a new number */
{
    int rc = ocl_chain_clone(ui.song_row, song.sel);
    ui_message(!rc ? "CHAIN COPIED" : rc == 1 ? "NO CHAIN THERE" : "CHAINS FULL");
}
static int ocl_clear_unused(void);
static void ps_unused(void)                            /* the pool's patterns nothing names emptied */
{
    int n = ocl_clear_unused();
    char m[24];
    if (n < 0) { ui_message(n == -2 ? "STOP FIRST" : "SAVE ERROR"); return; }
    fmt_int(m, n);
    str_cpy(m + str_len(m), n == 1 ? " PHRASE FREED" : " PHRASES FREED", 16);
    ui_message(m);
}
static const sheet_row_t SHEET_SONG[] = {
    {"Insert row", 0, 0, ps_row_insert, 0}, {"Delete row", 0, 0, ps_row_delete, 0}, {"Copy chain", 0, 0, ps_clone, 0},
    {"Clear song", 0, 0, 0, CF_CLEAR_SONG}, {"Free unused phrases", 0, 0, ps_unused, 0}};

/* (Orange Cat 0.4.4) PHRASES' (OCT+ held): the phrase picked copied to a new number, or emptied */
static void pp_copy(void)
{
    uint32_t n = ocl_pool_copy(ui.pool_sel % 64u);
    char m[24];
    if (n == OCL_NPOOL + 1u) { ui_message("EMPTY PHRASE"); return; }
    if (n == OCL_NPOOL + 2u) { ui_message("PHRASES FULL"); return; }
    if (n >= OCL_NPOOL) { ui_message(song.playing ? "STOP FIRST" : "SAVE ERROR"); return; }
    str_cpy(m, "COPIED TO 01", sizeof m);
    m[10] = (char)('0' + (n + 1u) / 10u); m[11] = (char)('0' + (n + 1u) % 10u);
    ui.pool_sel = (uint8_t)n;
    ui_message(m);
}
static void pp_clear(void)
{
    int rc;
    if (frz_phrase_frozen(ui.pool_sel % 64u + 1u)) { ui_message("UNFREEZE FIRST"); return; }   /* (Orange Cat 0.5.6) */
    rc = ocl_pool_clear(ui.pool_sel % 64u);
    ui_message(!rc ? "PHRASE CLEARED" : rc == 1 ? "NOTHING TO CLEAR" : rc == 2 ? "IN USE: NOT CLEARED" :
               song.playing ? "STOP FIRST" : "SAVE ERROR");
}
static const sheet_row_t SHEET_POOL[] = {{"Copy to new", 0, 0, pp_copy, 0}, {"Clear phrase", 0, 0, pp_clear, 0}};

/* the pattern's (NOTES, the drum grid, PATTERN: OCT+ held; TOOLS' Clear pattern lives here) */
static void nt_quantize_v(char *b){str_cpy(b,ui.note_grid==2u?"0.25 step":ui.note_grid==1u?"0.5 step":"1 step",16);}
static void nt_quantize_grid(int32_t s){ui.note_grid=(uint8_t)clamp((int32_t)ui.note_grid+s,0,2);ui.force=1;}
static void nt_quantize(void){ui.note_copy=ui.note_move=0;note_quantize(RECORD_UNIT>>ui.note_grid);ui.force=1;}
static const sheet_row_t SHEET_PATTERN[] = {{"Quantize",nt_quantize_v,nt_quantize_grid,nt_quantize,0}, {"Shift right", 0, 0, st_shift_r, 0},
    {"Shift left", 0, 0, st_shift_l, 0}, {"Reverse", 0, 0, st_reverse, 0},   /* (Orange Cat 0.4.6: seq_tools.c) */
    {"Clear pattern", 0, 0, 0, CF_CLEAR_SEQ}, {"Clear motion", 0, 0, 0, CF_CLEAR_MOTION}};
/* the shared effects' pages (0.6.0: DELAY .. MASTER): their settings back to a new project's */
static int jfx_globals_default(void)
{
    for (uint32_t i = G_DTIME; i < G_COUNT; i++)
        if (param_master_jfx(i) && song.g[i] != GP[i].def) return 0;
    return 1;
}
static const sheet_row_t SHEET_SHARED[] = {{"Reset effects", 0, 0, 0, CF_CLEAR_SHARED}};
/* (Orange Cat 0.6.1) MIXER's: a WAV from the card for the phrase the selected track is on (card_browse.c) */
static void mx_assign(void)
{
    pop_close();
    fb_open(song.sel);
    pop.on = POP_FILES;
    pop.sig = 0;
    ui.force = 1;
}
static const sheet_row_t SHEET_MIXER[] = {{"Assign clip", 0, 0, mx_assign, 0}};
/* (Orange Cat 0.4) CHAIN's (OCT+ held): the selected track's chain at the step picked */
static void ps_insert(void) { ocl_insert(song.sel, ui.chain_step); ui_message("SLOT INSERTED"); }
static void ps_dup(void)
{
    if (!OCL_ENT(song.sel % NTRK, ui.chain_step)[0]) { ui_message("EMPTY SLOT"); return; }
    ocl_dup(song.sel, ui.chain_step);
    ui_message("SLOT DUPLICATED");
}
static void ps_own(void)                               /* the step's phrase: a copy of its own, a new number */
{
    int rc = ocl_copy(song.sel, ui.chain_step);
    ui_message(!rc ? "OWN COPY MADE" : rc == 1 ? "EMPTY SLOT" : rc == 4 ? "PHRASES FULL" : song.playing ? "STOP FIRST" :
               "SAVE ERROR");
}
static const sheet_row_t SHEET_PATTERNS[] = {{"Insert slot", 0, 0, ps_insert, 0}, {"Duplicate slot", 0, 0, ps_dup, 0},
    {"Own copy", 0, 0, ps_own, 0}, {"Delete slot", 0, 0, 0, CF_DEL_ROW}, {"Clear chain", 0, 0, 0, CF_CLEAR_CHAIN}};

/* the step's: NOTES' note at the cursor (KNOB 1 its length, velocity, chance), the drum grid's hit of the lane there */
static void st_cap(uint32_t c, char *b)                 /* NOTES' column c as its chip shows it (draw_columns) */
{
    lrow_t rw;
    char lb[12];
    uint16_t vc;
    rw.page = cur_page()->title;
    rw.slot = (uint8_t)c;
    list_cap(&rw, lb, b, &vc);
}
static step_t *st_at(void)                              /* the note's step (else the cursor's) */
{
    uint32_t s = grid_on() ? NSTEP : notes_manual_start(TSEL);
    return &TSEL->step[s < NSTEP ? s : ui.cursor];
}
static int st_free(void)                                /* (a step edit now: not while a song plays, not recording) */
{
    if (chain_busy()) { ui_message("STOP TO EDIT"); return 0; }
    if (live_rec_sel()) { ui_message("STOP RECORDING"); return 0; }
    return 1;
}
static void nt_len_v(char *b)                          /* "3 steps" */
{
    uint32_t n;
    st_cap(2u, b);
    n = str_len(b);
    if (n && n < 9u && ((b[0] >= '0' && b[0] <= '9') || b[0] == '<') && b[n - 1] >= '0' && b[n - 1] <= '9')
        str_cpy(b + n, str_eq(b, "1") ? " step" : " steps", 8);
}
static void nt_len(int32_t s) { edit_param(2u, s > 0 ? 1 : -1); }
static void nt_vel_v(char *b) { st_cap(3u, b); }
static void nt_vel(int32_t s) { edit_param(3u, s); }
static void nt_chance_v(char *b)
{
    fmt_int(b, (int32_t)step_chance(st_at()));
    str_cpy(b + str_len(b), " %", 3);
}
static void nt_chance(int32_t s)
{
    step_t *st = st_at();
    if (st_free())
        step_set_chance(st, (uint32_t)clamp((int32_t)step_chance(st) + s, 0, 100));
}
static void nt_slide_v(char *b) { str_cpy(b, st_at()->flags & SF_SLIDE ? "On" : "Off", 8); }
static void nt_slide(void)
{
    if (!st_free())
        return;
    fm1_irq_off();
    st_at()->flags ^= SF_SLIDE;
    fm1_irq_on();
}
/* (Orange Cat 0.2, build doc phase 4a) Skiff's step fields on the note's and the hit's sheet: Ratchet x1..x4 or Flam,
 * the trig Condition, Nudge in 64ths of the step. Changed with the audio held off, as Slide is: one step, one write */
static void nt_ratch_v(char *b)
{
    uint32_t v = step_rflam(st_at());
    if (v >= 5u) { str_cpy(b, "Flam", 8); return; }
    b[0] = 'x'; b[1] = (char)('0' + v); b[2] = 0;
}
static void nt_ratch(int32_t s)
{
    if (!st_free())
        return;
    fm1_irq_off();
    step_set_rflam(st_at(), (uint32_t)clamp((int32_t)step_rflam(st_at()) + (s > 0 ? 1 : -1), 1, 5));
    fm1_irq_on();
}
static void nt_ratch_step(void) { nt_ratch(step_rflam(st_at()) >= 5u ? -4 : 1); }
static const char *const NT_COND[TC_COUNT] = {"Always", "Fill", "Not fill", "1 of 2", "2 of 2", "1 of 3", "2 of 3",
                                              "3 of 3", "1 of 4", "2 of 4", "3 of 4", "4 of 4"};
static void nt_cond_v(char *b) { str_cpy(b, NT_COND[st_at()->cond < TC_COUNT ? st_at()->cond : 0], 12); }
static void nt_cond(int32_t s)
{
    if (!st_free())
        return;
    fm1_irq_off();
    step_set_cond(st_at(), (uint32_t)clamp((int32_t)st_at()->cond + (s > 0 ? 1 : -1), 0, TC_COUNT - 1));
    fm1_irq_on();
}
static void nt_nudge_v(char *b)
{
    int32_t v = st_at()->nudge;
    if (!v) { str_cpy(b, "On grid", 10); return; }
    b[0] = v > 0 ? '+' : '-';
    fmt_int(b + 1, v > 0 ? v : -v);
    str_cpy(b + str_len(b), "/64", 4);
}
static void nt_nudge(int32_t s)
{
    if (!st_free())
        return;
    fm1_irq_off();
    step_set_nudge(st_at(), st_at()->nudge + s);
    fm1_irq_on();
}
static void nt_delete(void)
{
    if (st_free())
        step_delete_edit();
}
static void nt_copy(void) { if(st_free())note_copy_arm(); }
static const sheet_row_t SHEET_NOTE[] = {{"Length", nt_len_v, nt_len, 0, 0}, {"Velocity", nt_vel_v, nt_vel, 0, 0},
    {"Chance", nt_chance_v, nt_chance, 0, 0}, {"Slide", nt_slide_v, 0, nt_slide, 0},
    {"Ratchet", nt_ratch_v, nt_ratch, nt_ratch_step, 0}, {"Condition", nt_cond_v, nt_cond, 0, 0},   /* (Orange Cat 0.2) */
    {"Nudge", nt_nudge_v, nt_nudge, 0, 0}, {"Move note",0,0,note_move_arm,0}, {"Copy note",0,0,nt_copy,0}, {"Delete note", 0, 0, nt_delete, SR_RED}};
static const sheet_row_t SHEET_REC_NOTE[] = {{"Length", nt_len_v, nt_len, 0, 0}, {"Velocity", nt_vel_v, nt_vel, 0, 0},
    {"Move note",0,0,note_move_arm,0}, {"Copy note",0,0,nt_copy,0}, {"Delete note", 0, 0, nt_delete, SR_RED}};
static int ht_acc_on(void) { return (step_accents(&seq_steps(TSEL)[ui.cursor]) >> ui.lane) & 1u; }
static void ht_acc_v(char *b) { str_cpy(b, ht_acc_on() ? "On" : "Off", 8); }
static void ht_acc(void) { edit_param(3u, ht_acc_on() ? -1 : 1); }
static void ht_clear(void) { edit_param(2u, -1); }
static const sheet_row_t SHEET_HIT[] = {{"Accent", ht_acc_v, 0, ht_acc, 0}, {"Chance", nt_chance_v, nt_chance, 0, 0},
    {"Ratchet", nt_ratch_v, nt_ratch, nt_ratch_step, 0}, {"Condition", nt_cond_v, nt_cond, 0, 0},   /* (Orange Cat 0.2) */
    {"Nudge", nt_nudge_v, nt_nudge, 0, 0},
    {"Clear hit", 0, 0, ht_clear, SR_RED}};

/* OCT+ on NOTES / the drum grid: an empty place gets a note (the last one played) / the lane's hit; on a note or a
 * hit: its sheet. 0: not such a page */
static int step_enter(void)
{
    char t[24], sub[12];
    uint32_t chosen, start;
    if (ui.home || ui.layer || ui.menu || (cur_page()->graph != GR_ROLL && cur_page()->graph != GR_TRACKER))
        return 0;                                       /* ((Orange Cat 0.4.2) TRACKER: the row's note too) */
    if (frz_locked()) return 1;                         /* (Orange Cat 0.5.6) a frozen phrase: no note sheet */
    str_cpy(sub, "step ", sizeof sub);
    fmt_int(sub + 5, (int32_t)ui.cursor + 1);
    if (grid_on()) {
        if (!((step_lanes(&seq_steps(TSEL)[ui.cursor]) >> ui.lane) & 1u)) {
            edit_param(2u, 1);                          /* (the grid's HIT: its checks) */
            return 1;
        }
        str_cpy(t, drum_lane_abbr(TSEL, ui.lane), sizeof t);   /* "SD \xB7 step 5" */
        str_cpy(t + str_len(t), " \xB7 ", 4);
        str_cpy(t + str_len(t), sub, sizeof t - str_len(t));
        sheet_open(t, "", SHEET_HIT, NELEM(SHEET_HIT));
        return 1;
    }
    chosen = notes_selected(TSEL);
    start = notes_manual_start(TSEL);
    if (chosen >= RECORD_MAX && !(start < NSTEP && TSEL->step[start].n)) {
        edit_param(1u, 0);                              /* (NOTES' PITCH on an empty step: the last note there) */
        return 1;
    }
    str_cpy(t, "Note ", sizeof t);
    st_cap(1u, t + 5);
    if (chosen < RECORD_MAX)
        sheet_open(t, sub, SHEET_REC_NOTE, NELEM(SHEET_REC_NOTE));
    else
        sheet_open(t, sub, SHEET_NOTE, NELEM(SHEET_NOTE));
    return 1;
}

/* the page's sheet: 0 none, 1 the sound's, 2 the song's, 3 the pattern's, 4 PATTERNS', 5 the shared FX's */
static uint32_t page_sheet(void)
{
    uint32_t fam;
    if (designer_on() || ui.menu || ui.confirm || ui.layer || pop.on || name_on() || new_on() || slot_kind() || ptc_on())
        return 0;
    if (ui.home)
        return 1;
    if (cur_page()->graph == GR_SONG)
        return 2;
    if (cur_page()->graph == GR_PATGRID)
        return 4;
    if (cur_page()->graph == GR_POOL)                   /* (Orange Cat 0.4.4) */
        return 6;
    if (cur_page()->graph == GR_TRK)                    /* (Orange Cat 0.6.1) MIXER */
        return 7;
    if (cur_page()->graph == GR_ROLL || cur_page()->graph == GR_STEPS || cur_page()->graph == GR_TRACKER)
        return 3;                                       /* ((Orange Cat 0.4.6) TRACKER's too) */
    fam = cur_page()->fam;
    if (fam == FAM_FX && cur_page()->scope == SC_GLOBAL)   /* the shared effects' pages */
        return 5;
    return fam == FAM_EDIT || fam == FAM_ENV || fam == FAM_LFO || fam == FAM_FX ? 1u : 0u;
}
static void page_sheet_open(void)
{
    char nm[20];
    switch (page_sheet()) {
    case 1:
        sound_name(TSEL, nm);
        if (ui.home)
            sheet_open(nm[0] ? nm : "Sound", ENGINES[eng_idx(TSEL->eng_req)]->name, SHEET_STAGE, NELEM(SHEET_STAGE));
        else
            sheet_open(nm[0] ? nm : "Sound", ENGINES[eng_idx(TSEL->eng_req)]->name, SHEET_SOUND, NELEM(SHEET_SOUND));
        break;
    case 2:
        str_cpy(nm, "Row 01 \xb7 T1", sizeof nm);        /* (Orange Cat 0.4: the cell picked) */
        nm[4] = (char)('0' + (ui.song_row % OCL_NROW + 1u) / 10u);
        nm[5] = (char)('0' + (ui.song_row % OCL_NROW + 1u) % 10u);
        nm[str_len(nm) - 1u] = (char)('1' + song.sel % NTRK);
        sheet_open("Song", nm, SHEET_SONG, NELEM(SHEET_SONG));
        break;
    case 3:
        str_cpy(nm, "T1", sizeof nm);
        nm[1] = (char)('1' + song.sel % NTRK);
        sheet_open("Pattern", nm, SHEET_PATTERN, NELEM(SHEET_PATTERN));
        break;
    case 4: {                                           /* "Chain 05", "T2 \xb7 step C" (Orange Cat 0.4) */
        char t[12] = "Chain 01";
        uint32_t c = lists.tchain[song.sel % NTRK] % OCL_NCHAIN + 1u;
        t[6] = (char)('0' + c / 10u); t[7] = (char)('0' + c % 10u);
        str_cpy(nm, "T1 \xb7 slot A", sizeof nm);
        nm[1] = (char)('1' + song.sel % NTRK);
        nm[str_len(nm) - 1u] = (char)('A' + ui.chain_step % OCL_NENT);
        sheet_open(t, nm, SHEET_PATTERNS, NELEM(SHEET_PATTERNS));
        break;
    }
    case 5:
        sheet_open("Shared FX", "All tracks", SHEET_SHARED, NELEM(SHEET_SHARED));
        break;
    case 6: {                                           /* "Phrase 07" (Orange Cat 0.4.4) */
        char t[12] = "Phrase 01";
        t[7] = (char)('0' + (ui.pool_sel % 64u + 1u) / 10u); t[8] = (char)('0' + (ui.pool_sel % 64u + 1u) % 10u);
        sheet_open(t, "the pool", SHEET_POOL, NELEM(SHEET_POOL));
        break;
    }
    case 7: {                                           /* "Track 2", "card" (Orange Cat 0.6.1) */
        char t[8] = "Track 1";
        t[6] = (char)('1' + song.sel % NTRK);
        sheet_open(t, "card", SHEET_MIXER, NELEM(SHEET_MIXER));
        break;
    }
    default:
        break;
    }
}

/* a clearing row with nothing there to clear (or a song playing): says so instead of asking; 0 = ask */
static int cf_refused(uint32_t cf)
{
    int edit = cf == CF_CLEAR_SEQ || cf == CF_CLEAR_MOTION || cf == CF_DEL_PAT;   /* (Orange Cat 0.3: the lists edit
                                                                                   * while playing too) */
    if (edit && chain_busy())
        ui_message("STOP TO EDIT");
    else if (cf == CF_DEL_TRACK && trk_n() < 2u)
        ui_message("ONLY ONE TRACK");
    else if (cf == CF_DEL_TRACK && transport_busy())
        ui_message("STOP FIRST");
    else if (cf == CF_DEL_PAT && pattern_empty(song.sel % NTRK, TSEL->pattern))
        ui_message("NOTHING TO DELETE");
    else if ((cf == CF_CLEAR_SONG && !ocl_song_any()) || (cf == CF_CLEAR_SEQ && seq_is_empty(TSEL)) ||
             (cf == CF_CLEAR_CHAIN && !ocl_chain_any(lists.tchain[song.sel % NTRK])) ||
             (cf == CF_CLEAR_MOTION && !motion_count(TSEL)) ||
             (cf == CF_CLEAR_SHARED && jfx_globals_default()))
        ui_message("NOTHING TO CLEAR");
    else
        return 0;
    return 1;
}

/* KNOB 2 the row, KNOB 1 its value; OCT+ does it (its question first), OCT- closes (oct: bit 0 OCT-, bit 1 OCT+) */
static void sheet_input(int32_t k1, int32_t k2, uint32_t oct)
{
    const sheet_row_t *r;
    if (k2)
        pop.sel = (uint8_t)clamp((int32_t)pop.sel + (k2 > 0 ? 1 : -1), 0, (int32_t)pop.n - 1);
    r = &pop.rows[pop.sel % pop.n];
    if (k1 && r->turn)
        r->turn(k1);
    if (oct & 1u) {
        pop_close();
    } else if (oct & 2u) {
        pop_close();
        if (r->act)
            r->act();
        else if (r->cf && !cf_refused(r->cf))
            confirm_open(r->cf, song.sel);
        else if (r->turn)
            r->turn(1);
    }
}

/* ---------------------------------------------------- the pickers --- */
/* Destination IDs stay stable; both picker styles show the current engine's parameter labels. */
static const char *enum_name(const param_desc_t *d, int32_t v)
{
    return d->names == N_MDST ? mod_dst_name(TSEL, v) : d->names[v - d->min];
}
/* knob c turned on a page: its picker when its value is a name from a long list; another knob: the picker goes.
 * Not on a screen of its own: SCALES is the scales' list itself (Kerem: the picker covered it) */
static int own_screen(void);                            /* ui_draw.c */
/* a list of numbers (a MIDI channel, an algorithm, an envelope step; one name such as OFF among them): its value steps
 * on the card as MIDI's and ROUT's do, no picker (Kerem, 2026-10-11) */
static int pick_numbers(const param_desc_t *d)
{
    uint32_t named = 0;
    int32_t v;
    for (v = d->min; v <= d->max; v++) {
        const char *s = d->names[v];
        while (*s >= '0' && *s <= '9')
            s++;
        named += *s || s == d->names[v];
    }
    return named <= 1u;
}
static void pick_touch(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d;
    if (pop.on == POP_PICK && pop.col != c)
        pop_close();
    if (ui.home || pop.on == POP_SHEET || own_screen() || (PREF_BITS & PREF_PICK_OFF))
        return;
    d = page_desc(cur_page(), c, &vp);
    if (!d || !vp || d->fmt != F_ENUM || !d->names || d->max - d->min < 4 || pick_numbers(d))
        return;
    if (pop.on != POP_PICK)
        ui.force = 1;                                   /* (the page dimmed under it) */
    pop.on = POP_PICK;
    pop.col = (uint8_t)c;
    pop.t = fm1_ms;
}
static void pick_poll(void)                             /* ST_KNOB_MS after the last turn: gone */
{
    if (pop.on == POP_PICK && fm1_ms - pop.t > ST_KNOB_MS)
        pop_close();
}

/* ------------------------------------------------ a value's list --- */
/* OCT+ on a list page's row whose value is a name from a list: the whole list (KNOB 2, OCT+ takes it, OCT- closes) */
/* Compact enum lists omit retired empty entries without changing stored IDs. */
static int32_t enum_rows(const param_desc_t *d)
{
    int32_t count=0;for(int32_t v=d->min;v<=d->max;v++)count+=d->names[v-d->min][0]!=0;return count;
}
static int32_t enum_rank(const param_desc_t *d,int32_t value)
{
    int32_t rank=0;for(int32_t v=d->min;v<value;v++)rank+=d->names[v-d->min][0]!=0;return rank;
}
static int32_t enum_value(const param_desc_t *d,int32_t rank)
{
    for(int32_t v=d->min;v<=d->max;v++)if(d->names[v-d->min][0] && !rank--)return v;return d->min;
}
static struct { uint8_t page, slot; int16_t sel; } vl;
static const param_desc_t *vlist_desc(int16_t **vp) { return page_desc(&PAGES[vl.page], vl.slot, vp); }
static void vlist_open(uint8_t page, uint8_t slot)
{
    int16_t *vp;
    const param_desc_t *d;
    vl.page = page;
    vl.slot = slot;
    d = vlist_desc(&vp);
    if (!d || !vp)
        return;
    vl.sel = *vp;
    pop.on = POP_LIST;
    ui.force = 1;
}
static void vlist_input(int32_t k2, uint32_t oct)
{
    int16_t *vp;
    const param_desc_t *d = vlist_desc(&vp);
    if (!d || !vp) {
        pop_close();
        return;
    }
    if (k2)
        vl.sel = (int16_t)enum_step(d,vl.sel,clamp(vl.sel + (k2 > 0 ? 1 : -1), d->min, d->max));
    if (oct & 1u) {
        pop_close();
    } else if (oct & 2u) {                              /* (its page edits it, as if it were shown) */
        uint8_t pg = ui.page;
        int32_t steps = vl.sel - *vp;
        ui.page = vl.page;
        if (steps)
            edit_param(vl.slot, steps);
        ui.page = pg;
        pop_close();
    }
}
/* a card's title rows: the title (the text face) and its sub (the label face) at the right, or (both do not fit) under it */
static int32_t pop_title_h(const char *title, const char *sub)
{
    int32_t sw = sub && sub[0] ? text_w(TY_LB, sub) + 12 : 0;
    return sw && text_w(TY_TX, title) > POP_W - 20 - sw ? POP_ROW + 20 : POP_ROW;
}
/* a card from the bottom (h rows tall under its title rows, slices of whole rows): its title, then rows drawn by
 * row(i, y) for first .. first + vis - 1 */
static void pop_card(int32_t h, const char *title, const char *sub, uint32_t first, uint32_t vis,
                     void (*row)(uint32_t i, int32_t y))
{
    int32_t th = pop_title_h(title, sub), y0, top;
    uint32_t i;
    h += th - POP_ROW;
    y0 = 238 - h;
    for (top = 0; top < h; ) {                          /* in slices between rows (a canvas holds CV_MAX / POP_W rows) */
        int32_t R = (int32_t)(CV_MAX / POP_W), sh = h - top <= R ? h - top : th + (top + R - th) / POP_ROW * POP_ROW - top;
        cv_begin(POP_W, (uint32_t)sh, T_BG);
        cv_oy = -top;
        cv_rrect(0, 0, POP_W, h, 10, T_LINE, T_BG);
        cv_rrect(1, 1, POP_W - 2, h - 2, 9, T_SURF, T_LINE);
        if (top == 0) {
            int32_t sw = sub && sub[0] ? text_w(TY_LB, sub) + 12 : 0;
            const aafont_t *f = ty_fit(TY_TX, title, POP_W - 20 - (th > POP_ROW ? 0 : sw));
            cv_free_text(10, ty_y(POP_ROW / 2, f), f, title, T_TEXT, T_SURF, POP_W - 20 - (th > POP_ROW ? 0 : sw));
            if (sw && th > POP_ROW)
                cv_text_on(10, ty_y(POP_ROW + 4, TY_LB), TY_LB, sub, T_MID, T_SURF);
            else if (sw)
                cv_text_r(POP_W - 10, ty_y(POP_ROW / 2, TY_LB), TY_LB, sub, T_MID, T_SURF);
        }
        for (i = first; i < first + vis; i++) {
            int32_t y = th + (int32_t)(i - first) * POP_ROW;
            if (y + POP_ROW <= top || y >= top + sh)
                continue;
            row(i, y);
        }
        cv_oy = 0;
        cv_blit(POP_X, (uint32_t)(y0 + top));
        top += sh;
    }
}
/* a sheet's or a list's row on the card (row_draw's look, on the card's surface): picked tinted in the track's colour;
 * red: it would destroy something (it asks first); dot: the value now */
static void pop_row(int32_t y, const char *lb, const char *v, int on, int red, int dot)
{
    const aafont_t *tf = TY_TX;
    uint16_t bg = on ? ux_mix(T_SURF, T_THEME, 22) : T_SURF;
    int32_t cy = y + (POP_ROW - 2) / 2, vx = POP_W - 10;
    if (on)
        cv_rrect(4, y, POP_W - 8, POP_ROW - 2, 7, bg, T_SURF);
    if (dot)
        cv_disc(POP_W - 16, cy, 8, T_THEME, 0), vx = POP_W - 28;
    if (v && v[0])
        vx = cv_text_r(vx, ty_y(cy, tf), tf, v, on ? T_THEME : T_MID, bg) - 8;
    tf = ty_fit(tf, lb, vx - 12);
    cv_free_text(12, ty_y(cy, tf), tf, lb, red ? T_REC : on ? T_TEXT : T_SEC, bg, vx - 12);
}
static const param_desc_t *vl_d;
static int32_t vl_first, vl_now;
static void vlist_row(uint32_t i, int32_t y)
{
    char b[20];
    int32_t v = enum_value(vl_d, (int32_t)i);
    menu_words(b, enum_name(vl_d, v), sizeof b);
    pop_row(y, b, 0, v == vl.sel, 0, v == vl_now);
}
static void vlist_draw(void)
{
    int16_t *vp;
    const param_desc_t *d = vlist_desc(&vp);
    int32_t n, first;
    uint32_t sig;
    char b[20];
    if (!d || !vp || !d->names) {
        pop_close();
        return;
    }
    n = enum_rows(d) < POP_VIS ? enum_rows(d) : POP_VIS;
    sig = (uint32_t)(vl.sel + 1) * 7919u + vl.page * 131u + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == pop.sig)
        return;
    pop.sig = sig;
    first = clamp(enum_rank(d,vl.sel) - POP_VIS / 2, 0, enum_rows(d) - n);
    menu_words(b, d->label, sizeof b);
    vl_d = d;
    vl_now = *vp;
    pop_card(POP_ROW + n * POP_ROW + 4, b, "", (uint32_t)first, (uint32_t)n, vlist_row);
}

/* ------------------------------------------------------- drawing --- */
static void sheet_row(uint32_t i, int32_t y)
{
    const sheet_row_t *r = &pop.rows[i];
    char v[20];
    v[0] = 0;
    if (r->value)
        r->value(v);
    pop_row(y, r->label, v, i == pop.sel, r->cf != 0, 0);
}
static void sheet_draw(void)
{
    uint32_t i, vis = pop.n < POP_VIS ? pop.n : POP_VIS, first, sig = str_hash(pop.sel * 7u + pop.n * 131u + ux.gen * 977u + ux.pal * 31u, pop.title);
    char v[20];
    for (i = 0; i < pop.n; i++)
        if (pop.rows[i].value) {
            pop.rows[i].value(v);
            sig = str_hash(sig, v);
        }
    if (!ui.force && sig == pop.sig)
        return;
    pop.sig = sig;
    first = (uint32_t)clamp((int32_t)pop.sel - (int32_t)vis / 2, 0, (int32_t)(pop.n - vis));
    pop_card(POP_ROW + (int32_t)vis * POP_ROW + 4, pop.title, pop.sub, first, vis, sheet_row);
}
static void pick_draw(void)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), pop.col, &vp);
    int32_t x, v, i;
    uint32_t sig;
    if (!d || !vp || !d->names) {
        pop_close();
        return;
    }
    v = *vp;
    sig = (uint32_t)(v + 1) * 7919u + pop.col * 131u + ux.gen * 977u + ux.pal * 31u;
    if (!ui.force && sig == pop.sig)
        return;
    pop.sig = sig;
    {
    int32_t rows = enum_rows(d) < 5 ? enum_rows(d) : 5, h = 12 + rows * 28;
    int32_t first = clamp(enum_rank(d,v) - 2, 0, enum_rows(d) - rows);   /* five names around it, the list's ends kept */
    x = CARD_X(pop.col < 3u ? pop.col : 2u);            /* its column and the next (the last: the two last), whole:
                                                         * a neighbour's value is under it entirely or not at all */
    CV_BANDS((uint32_t)x, Y_LABEL + CARD_H + 2, POP_PICK_W, (uint32_t)h, T_BG) {   /* (0.7.0: in bands of the canvas's rows) */
        cv_rrect(0, 0, POP_PICK_W, h, 10, T_LINE, T_BG);
        cv_rrect(1, 1, POP_PICK_W - 2, h - 2, 9, T_SURF, T_LINE);
        for (i = 0; i < rows; i++) {
            int32_t n = enum_value(d,first + i), y = 6 + i * 28, far = n - v > 1 || v - n > 1;
            uint16_t bg = n == v ? ux_mix(T_SURF, T_THEME, 22) : T_SURF;
            char b[16];
            list_words(b, enum_name(d, n), sizeof b);
            if (n == v)
                cv_rrect(4, y, POP_PICK_W - 8, 26, 6, bg, T_SURF);
            cv_free_text(10, ty_y(y + 13, ty_fit(TY_TX, b, POP_PICK_W - 18)), ty_fit(TY_TX, b, POP_PICK_W - 18), b,
                         n == v ? T_THEME : far ? T_MID : T_SEC, bg, POP_PICK_W - 18);
        }
    }
    }
}
/* (Orange Cat 0.6.1) the card's browser: the folder's rows, KNOB 2 the row (ui_input.c files_input) */
static uint32_t fb_first, fb_rows_drawn;
static void files_row(uint32_t i, int32_t y)
{
    char nm[44], v[16];
    if (!fb_row(i, nm, sizeof nm, v, sizeof v)) return;
    pop_row(y, nm, v, (int)(i == fb_sel_now()), 0, 0);
}
static void files_draw(void)
{
    uint32_t n = 0, sel = fb_sel_now(), sig = fb_sig() * 7919u + ux.gen * 977u + ux.pal * 31u + 1u;
    char nm[44], v[16];
    if (!ui.force && sig == pop.sig)
        return;
    pop.sig = sig;
    while (n < 64u && fb_row(n, nm, sizeof nm, v, sizeof v)) n++;
    if ((n < POP_VIS ? n : POP_VIS) != fb_rows_drawn && !ui.force) {   /* (the card's height changed: the page under it
                                                         * drawn again first, else the taller card stays behind it; 0.6.1
                                                         * on the unit: "Save first" drawn over the list) */
        fb_rows_drawn = n < POP_VIS ? n : POP_VIS;
        ui.force = 1;
        pop.sig = 0;
        return;
    }
    fb_rows_drawn = n < POP_VIS ? n : POP_VIS;
    if (sel < fb_first) fb_first = sel;
    if (sel >= fb_first + POP_VIS) fb_first = sel - POP_VIS + 1u;
    if (fb_first + POP_VIS > n) fb_first = n > POP_VIS ? n - POP_VIS : 0u;
    pop_card(POP_ROW + (int32_t)(n < POP_VIS ? n : POP_VIS) * POP_ROW + 4, "Assign clip", fb_where(), fb_first,
             n < POP_VIS ? n : POP_VIS, files_row);
}
static void files_input(int32_t k2, uint32_t oct)
{
    if (k2) fb_move(k2);
    if (oct & 1u) {
        if (fb_back()) pop_close();
    } else if (oct & 2u) {
        if (fb_ok()) {
            pop_close();
            ui_message(fb_pack_done() ? "PACK INSTALLED" : "CLIP ASSIGNED");
        }
    }
    ui.force |= fb_on() ? 0u : 1u;
}
static void pop_draw(void)
{
    if (pop.on == POP_SHEET)
        sheet_draw();
    else if (pop.on == POP_PICK)
        pick_draw();
    else if (pop.on == POP_LIST)
        vlist_draw();
    else if (pop.on == POP_FILES)
        files_draw();
}
