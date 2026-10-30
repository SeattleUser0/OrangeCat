/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface (OC_HOME_PIC; docs/orangecat/UI_DESIGN.md section 1 and Part 2's HOME B, docs/orangecat/UI_BUILD.md step 1c):
 * HOME's picture page, the landing page of HOME when MENU > HOME PAGE is PICTURE. HOME then has three pages, PICTURE (ui.home_pg
 * 2), TRACKS (Stage, 0) and CHORD (the chord view, 1); HOME tapped on HOME steps PICTURE, TRACKS, CHORD. With TRACKS chosen (the
 * default) nothing here runs and HOME is release-2's.
 *
 * The page (HOME B, the owner's decision 1), under the shared header:
 *   rows  30 ..  54  the sound line: the track's numeral on its color, the sound's name, a factory preset's place "n/N"; while a
 *                    knob turns (and ST_KNOB_MS after) the knob's name in its color and its value; PACK MISSING in REC's color
 *                    when the engine's picture cannot be read for want of the pack
 *   rows  54 ..  86  the four dials, in the knobs' fixed colors (gfx.c T_COZY): a ring, the value's arc (a plain knob's from its
 *                    left end, a macro's from the top, the sound as stored), a pointer
 *   rows  86 .. 108  the value row: each value under its dial in the knob's color, the knob turning in text color
 *   rows 108 .. 240  the picture: the engine's scene (oc_scene.c scene_of: the asset pack's trailer), drawn in bands of the one
 *                    canvas, its sources filled from the selected track every frame; without a scene, the plain picture (the
 *                    output's waveform and the engine's name)
 * The four knobs are the engine's table (oc_knobs.c) through Stage's knob page (ui.c stage_page), so they edit as every page's
 * knobs do (ui_input.c page_knob, edit_param). An engine without a table keeps its four EDIT knobs and the plain picture.
 *
 * Rate (UI_DESIGN.md section 3's budget): the picture draws at most every HP_FRAME_MS (10 frames a second, about 9 at the UI's
 * 16 ms frames) and only when its inputs changed (a scene binds the clock, so it redraws at that rate; the screen diff sends what
 * moved); the sound line, the dials and the value row draw when they change. Each part keeps the signature of what it
 * drew; the screen diff (gfx.c) sends only the pixels that changed.
 *
 * Included from ui_draw.c under OC_HOME_PIC, after ui_stage.c and oc_scene.c.
 *
 * Revisions:
 *   (the new interface 0.1, branch ui-build, UI_BUILD.md 1c) first version.
 */
#define HP_SOUND_Y 30
#define HP_SOUND_H 24
#define HP_DIALS_Y 54
#define HP_DIALS_H 32
#define HP_VALUE_Y 86
#define HP_VALUE_H 22
#define HP_PIC_Y 108
#define HP_PIC_H 132                                    /* (the scenes' design box: 240 x 132) */
#define HP_FRAME_MS 100u                                /* the picture's frame: 10 frames a second at most */
#define HP_HIT_MS 160u                                  /* SS_HIT falls from 256 to 0 over this long after a note-on */
#define HP_CELL_X(k) (2 + 60 * (int32_t)(k))            /* (a knob's cell: 57 px at x 2 + 60 k, as the pages' cards: ui.c CARD_X) */

static struct {
    uint32_t sig_sound, sig_dials, sig_values, sig_pic;  /* what each part last drew */
    uint32_t pic_ms, ph_ms;                              /* when the picture last drew; when the phases last moved */
    uint32_t age, hit_ms;                               /* the newest voice's age seen; the last note-on's time */
    const uint8_t *scene;                               /* the scene found (scene_of), its length; for the engine and the pack's check: */
    uint32_t scene_len;
    uint8_t scene_eng, scene_gen, scene_ok;             /* .. scene_ok 1: looked up for scene_eng at scene_gen */
    uint16_t ph[4];                                     /* the knob-rate phases (SS_PH1 .. SS_PH4), the selected track's */
    uint8_t note, trk, landed, ready;                   /* the last note; the track the state is of; HOME landed on the picture; the
                                                         * state is of trk (0: the first frame) */
} hp;

static uint32_t hp_hash(uint32_t h, const void *p, uint32_t n)   /* FNV-1a */
{
    const uint8_t *b = p;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}
/* 1: knob k is a macro of the selected track's table, in force (PICTURE chosen); an id 33 .. 36 elsewhere is not one (the Prophet test
 * engine's HOME knobs, a module's native offsets: the review) */
static int hp_is_macro(uint32_t k)
{
    const knob_def_t *kd = kdef_of(TSEL->eng_req);
    return home_pic_on() && kd && kd[k & 3u].kind == KD_MACRO;
}
/* knob k's place on its travel, 0 .. 256 (a macro: 128 + 2 x its offset, so 128 is the sound as stored, oc_scene.h); *macro 1 for a
 * macro; -1 no knob there */
static int32_t hp_knob_q8(uint32_t k, int *macro)
{
    const page_t *sp = stage_page();
    int16_t *vp;
    const param_desc_t *d = page_desc(sp, k, &vp);
    *macro = hp_is_macro(k);
    if (!d || !vp || !d->label || !d->label[0] || d->max <= d->min)
        return -1;
    if (*macro)
        return clamp(128 + 2 * *vp, 0, 256);
    return (int32_t)((clamp(*vp, d->min, d->max) - d->min) * 256 / (d->max - d->min));
}
/* knob k's name and its value as text ("+12" for a macro); 0 when no knob is there */
static const char *hp_knob_text(uint32_t k, char *val)
{
    const page_t *sp = stage_page();
    int16_t *vp;
    const char *unit = "";
    const param_desc_t *d = page_desc(sp, k, &vp);
    val[0] = 0;
    if (!d || !vp || !d->label || !d->label[0])
        return 0;
    if (hp_is_macro(k)) {                               /* a macro: its offset from the sound as stored */
        if (*vp > 0) { val[0] = '+'; fmt_int(val + 1, *vp); }
        else fmt_int(val, *vp);
        return d->label;
    }
    stage_value(sp, d, *vp, val, &unit);
    if (unit && unit[0] && str_len(val) + str_len(unit) < 11u) str_cpy(val + str_len(val), unit, 12u - str_len(val));
    return stage_label(k, d);
}

/* ---- the sound line ---- */
static void hp_sound_line(int pack_missing)
{
    const track_t *t = TSEL;
    const engine_t *e = ENGINES[t->eng_req % NENGINES];
    int hot = stage_knobs_out() ? (int)ui.hot_col : -1;
    char name[24], val[12], pos[12];
    const char *label = hot >= 0 ? hp_knob_text((uint32_t)hot & 3u, val) : 0;
    uint32_t sig;
    sound_name(t, name);
    pos[0] = 0;
    if (!label && user_of(t) == USER_NONE && !t->user_native && e->npresets) {   /* a factory preset: its place in the list */
        fmt_int(pos, (int32_t)(t->preset % e->npresets) + 1);
        str_cpy(pos + str_len(pos), "/", 2);
        fmt_int(pos + str_len(pos), (int32_t)e->npresets);
    }
    sig = hp_hash(2166136261u, name, str_len(name));
    sig = hp_hash(sig, pos, str_len(pos));
    sig = hp_hash(sig, val, str_len(val)) + (uint32_t)(hot + 2) * 7919u + song.sel * 131u + ux.gen * 977u +
          (uint32_t)pack_missing * 31u + (label ? 1u : 0u);
    if (!ui.force && sig == hp.sig_sound)
        return;
    hp.sig_sound = sig;
    cv_begin(240, HP_SOUND_H, T_BG);
    cv_rrect(4, 2, 20, 20, 5, T_TRK(song.sel), T_BG);
    {
        char d[2] = {(char)('1' + song.sel % NTRK), 0};
        cv_text_c(14, ty_y(12, TY_TX), TY_TX, d, T_INK, T_TRK(song.sel));
    }
    if (label) {                                        /* a knob turning: its name in its color, its value */
        int32_t x = cv_text(32, ty_y(12, TY_TX), TY_TX, label, T_COZY((uint32_t)hot));
        cv_text(x + 12, ty_y(12, TY_TX), TY_TX, val, T_TEXT);
    } else if (pack_missing) {
        cv_text(32, ty_y(12, TY_TX), TY_TX, "PACK MISSING", T_REC);
    } else {
        int32_t room = 236 - (pos[0] ? text_w(TY_LB, pos) + 8 : 0) - 32;
        cv_free_text(32, ty_y(12, TY_TX), TY_TX, name, T_TEXT, T_BG, room);
        if (pos[0]) cv_text_r(236, ty_y(12, TY_LB), TY_LB, pos, T_MID, T_BG);
    }
    cv_blit(0, HP_SOUND_Y);
}

/* ---- the dials and the value row ---- */
/* a dial: a ring in the knob's color, the value's arc on it (a macro's from the top), a pointer (scene geometry: oc_scene.c) */
static void hp_dial(int32_t cx, int32_t cy, int32_t d, uint32_t k, int32_t q, int macro, int hot)
{
    uint16_t c = T_COZY(k), quiet = ux_mix(T_BG, c, 30);
    int32_t r = d / 2, a;
    if (q < 0) {                                        /* (no knob: an empty place, as today's empty card) */
        cv_disc(cx, cy, 3, T_LINE, 0);
        return;
    }
    sc_depth = 0;
    sc_xf[0] = (sc_xf_t){0, 0, 0, 0, 4096, 0, 0};
    sc_weight = 1;
    sc_arc16(cx, cy, r, 96, 32, quiet);                 /* the travel: 135 degrees either side of the top */
    a = 96 + q * 192 / 256;                             /* the value's angle */
    sc_weight = 2;
    if (macro) {
        if (a > 192) sc_arc16(cx, cy, r, 192, a, c);
        else if (a < 192) sc_arc16(cx, cy, r, a, 192, c);
    } else if (a > 96) {
        sc_arc16(cx, cy, r, 96, a, c);
    }
    sc_weight = hot ? 2 : 1;
    sc_line16(cx * 16, cy * 16, cx * 16 + (r - 2) * 16 * sc_cos(a) / 4096, cy * 16 + (r - 2) * 16 * sc_sin(a) / 4096, c);
    cv_disc(cx, cy, hot ? 5 : 4, c, 0);
    sc_weight = 1;
}
static void hp_dials(void)
{
    int32_t q[4];
    int mac[4], hot = stage_knobs_out() ? (int)ui.hot_col : -1;
    uint32_t k, sig;
    for (k = 0; k < 4u; k++) q[k] = hp_knob_q8(k, &mac[k]);
    sig = hp_hash(2166136261u, q, sizeof q) + (uint32_t)(hot + 2) * 7919u + ux.gen * 977u + (uint32_t)mac[0] + 2u * (uint32_t)mac[1] +
          4u * (uint32_t)mac[2] + 8u * (uint32_t)mac[3];
    if (!ui.force && sig == hp.sig_dials)
        return;
    hp.sig_dials = sig;
    cv_begin(240, HP_DIALS_H, T_BG);
    for (k = 0; k < 4u; k++)
        hp_dial(HP_CELL_X(k) + 28, 16, 26, k, q[k], mac[k], hot == (int)k);
    cv_blit(0, HP_DIALS_Y);
}
static void hp_values(void)
{
    char v[4][12];
    int hot = stage_knobs_out() ? (int)ui.hot_col : -1;
    uint32_t k, sig = 2166136261u;
    const char *lab[4];
    for (k = 0; k < 4u; k++) {
        lab[k] = hp_knob_text(k, v[k]);
        sig = hp_hash(sig, v[k], str_len(v[k])) + (lab[k] ? k + 1u : 0u);
    }
    sig += (uint32_t)(hot + 2) * 7919u + ux.gen * 977u;
    if (!ui.force && sig == hp.sig_values)
        return;
    hp.sig_values = sig;
    cv_begin(240, HP_VALUE_H, T_BG);
    for (k = 0; k < 4u; k++)
        if (lab[k])
            cv_text_c(HP_CELL_X(k) + 28, ty_y(10, TY_LB), TY_LB, v[k], hot == (int)k ? T_TEXT : T_COZY(k), T_BG);
    cv_blit(0, HP_VALUE_Y);
}

/* ---- the picture ---- */
/* the scene's sources from the selected track, this frame (oc_scene.h: Q8) */
static void hp_inputs(scene_in_t *in)
{
    const track_t *t = TSEL;
    uint32_t k, i, now = fm1_ms, newest = 0, dt;
    int32_t env = 0, gate = 0, peak = 1500;
    const voice_t *nv = 0;
    memset(in, 0, sizeof *in);
    if (hp.trk != song.sel || !hp.ready) {              /* (another track, the first frame: its phases from 0; what sounds now is
                                                         * not a note-on: the newest voice's age is the mark) */
        memset(hp.ph, 0, sizeof hp.ph);
        hp.trk = song.sel;
        hp.age = 0;
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].age > hp.age) hp.age = t->v[i].age;
        hp.hit_ms = now - HP_HIT_MS;
        hp.ph_ms = now;
        hp.ready = 1;
    }
    for (i = 0; i < NVOICE; i++) {                      /* the loudest voice's envelope, a key held, the newest note-on */
        const voice_t *v = &t->v[i];
        if (!v->active) continue;
        if (v->env_out > env) env = v->env_out;
        gate |= v->gate;
        if (v->age > newest) { newest = v->age; nv = v; }
    }
    if (nv && newest > hp.age) {                        /* a note-on since the last frame (the voice clock only grows: a newer
                                                         * voice freed first is not one) */
        hp.hit_ms = now;
        hp.age = newest;
        hp.note = nv->note;
    }
    dt = now - hp.ph_ms;
    hp.ph_ms = now;
    if (dt > 1000u) dt = 1000u;
    for (k = 0; k < 4u; k++) {
        int mac;
        int32_t q = hp_knob_q8(k, &mac);
        in->s[SS_K1 + k] = (int16_t)(q < 0 ? 128 : q);
        /* the knob-rate phase: 0.1 Hz at 0 to 3 Hz at full, in 1/65,536 of a turn */
        hp.ph[k] = (uint16_t)(hp.ph[k] + (uint64_t)dt * (uint32_t)(100 + 2900 * (q < 0 ? 0 : q) / 256) * 65536u / 1000000u);   /* (64
                                                         * bits: dt x milli-hertz x 65,536 passes 32: the review) */
        in->s[SS_PH1 + k] = (int16_t)(hp.ph[k] >> 8);
    }
    in->s[SS_ENV] = (int16_t)(env >> 7);                /* (Q15 -> 0 .. 256) */
    in->s[SS_HIT] = (int16_t)(now - hp.hit_ms < HP_HIT_MS ? (int32_t)(HP_HIT_MS - (now - hp.hit_ms)) * 256 / (int32_t)HP_HIT_MS : 0);
    in->s[SS_GATE] = (int16_t)(gate ? 256 : 0);
    in->s[SS_LFO] = (int16_t)(t->lfo_val >> 7);
    in->s[SS_NOTE] = (int16_t)(hp.note % 12u * 256u / 12u);
    in->s[SS_TIME] = (int16_t)(now % 1000u * 256u / 1000u);
    in->s[SS_WOB] = (int16_t)(sc_sin((int32_t)(now % 2000u * 256u / 2000u)) / 16);
    in->col[SCC_INK] = T_SEC;
    for (k = 0; k < 4u; k++) in->col[SCC_K1 + k] = T_COZY(k);
    in->col[SCC_TRK] = T_TRK(song.sel);
    in->col[SCC_DIM] = T_LINE;
    in->col[SCC_BG] = T_BG;
    {   /* SCOPE: the output's last 256 samples in 64, scaled to its peak (scope_buf: the master output, as HOME's waveform today) */
        uint32_t w = scope_w;
        for (i = 0; i < 256u; i++) {
            int32_t s = scope_buf[(w - 256u + i) & (SCOPE_N - 1u)];
            if (s > peak) peak = s;
            else if (-s > peak) peak = -s;
        }
        for (i = 0; i < 64u; i++)
            in->scope[i] = (int16_t)(scope_buf[(w - 256u + 4u * i) & (SCOPE_N - 1u)] * 256 / peak);
    }
}
/* the plain picture (no scene): the output's waveform in a band of HP_PLAIN_H rows and the engine's name over it, quiet (the band
 * kept low: a waveform redraws its whole width every frame; at the region's full height it cost 18 ms of SPI a frame on average,
 * over the budget: home_pic_test BUDGET) */
#define HP_PLAIN_H 48
static void hp_plain(void)
{
    const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
    CV_BANDS(0, HP_PIC_Y, 240, HP_PIC_H, T_BG) {
        stage_wave(ux_mix(T_BG, T_TRK(song.sel), 70), 40, HP_PLAIN_H);
        cv_text_c(120, ty_y(12, TY_TX), TY_TX, e->name, T_MID, T_BG);
    }
}
static void hp_picture(void)
{
    scene_in_t in;
    uint32_t sig;
    const uint8_t *s;
    if (!ui.force && fm1_ms - hp.pic_ms < HP_FRAME_MS)
        return;
    if (!hp.scene_ok || hp.scene_eng != TSEL->eng_req || hp.scene_gen != oc_pack_gen || !OC_PACK_READY()) {   /* (found again only when
                                                         * the engine or the pack changed: on the unit a lookup reads the trailer's CRC
                                                         * and walks the scene) */
        hp.scene = kdef_of(TSEL->eng_req) ? scene_of(TSEL->eng_req, &hp.scene_len) : 0;
        hp.scene_eng = TSEL->eng_req;
        hp.scene_gen = oc_pack_gen;
        hp.scene_ok = (uint8_t)OC_PACK_READY();
    }
    s = hp.scene;
    hp_inputs(&in);
    sig = s ? hp_hash(2166136261u, &in, sizeof in) + (uint32_t)(uintptr_t)s : 0x51A1u + ux.gen * 977u + TSEL->eng_req * 131u +
          (uint32_t)scope_w;                            /* (the plain picture follows the waveform) */
    if (!ui.force && sig == hp.sig_pic)
        return;
    hp.sig_pic = sig;
    hp.pic_ms = fm1_ms;
    if (!s) {
        hp_plain();
        return;
    }
    CV_BANDS(0, HP_PIC_Y, 240, HP_PIC_H, T_BG) {
        scene_draw(s, 0, HP_PIC_H - s[2], &in);         /* (a scene's box anchored to the region's bottom) */
    }
}

/* ---- the page ---- */
static void home_pic_draw(void)
{
    int pack_missing = kdef_of(TSEL->eng_req) && !OC_PACK_READY();
    if (ui.force)
        hp.sig_sound = hp.sig_dials = hp.sig_values = hp.sig_pic = 0;
    hp_sound_line(pack_missing);
    hp_dials();
    hp_values();
    hp_picture();
}
/* HOME tapped on HOME (ui_input.c): PICTURE, TRACKS, CHORD in turn */
static void home_pic_next(void)
{
    ui.home_pg = (uint8_t)(ui.home_pg == 2u ? 0u : ui.home_pg == 0u ? 1u : 2u);
    ui.force = 1;
}
/* HOME entered (go_home, the first frame after power-on): its landing page, the picture when PICTURE is chosen */
static void home_pic_land(void)
{
    if (home_pic_on()) ui.home_pg = 2;
    else if (ui.home_pg > 1u) ui.home_pg = 0;
}

/* (MENU > HOME PAGE back to TRACKS, ui_menu.c) track t's macros committed into its targets and set to 0, as a save commits them, so
 * no offset is left where no knob shows it (the design review, UI_BUILD.md 1c). A target a motion lane plays is left as it is (its
 * base is motion's). With the interrupt off: it never sees the targets moved and the macros still on */
static void mac_commit(track_t *t)
{
    int16_t v[MAC_N];
    const knob_def_t *kd = kdef_of(t->eng_req);
    uint32_t i, k = (uint32_t)(t - trk) % NTRK, f;
    if (!kd)
        return;
    f = motion_guard();                                 /* (the interrupt not in between; the state before restored after) */
    for (i = 0; i < MAC_N; i++)                         /* (each target's committed value, before anything is written) */
        v[i] = kd[i / KD_TGT].t[i % KD_TGT].id == KD_NOT ? 0 : mac_saved_value(t, kd[i / KD_TGT].t[i % KD_TGT].id);
    for (i = 0; i < MAC_N; i++) {
        uint32_t id = kd[i / KD_TGT].t[i % KD_TGT].id;
        if (id != KD_NOT && kd[i / KD_TGT].kind == KD_MACRO && !mac_motion(k, id))
            t->p[id] = v[i];
    }
    for (i = 0; i < 4u; i++)
        if (kd[i].kind == KD_MACRO)
            t->p[P_MAC0 + i] = 0;
    motion_unguard(f);
}
/* MENU > HOME PAGE set: PICTURE (on 1) or TRACKS */
static void home_pic_set(int on)
{
    if (on == home_pic_on())
        return;
    if (on) {
        GLO_SELECT = (uint8_t)(GLO_SELECT | HOME_PIC_BIT);
    } else {
        momentary_restore();                            /* (a momentary take on a macro back first: it would put an offset back) */
        for (uint32_t k = 0; k < NTRK; k++) mac_commit(&trk[k]);
        GLO_SELECT = (uint8_t)(GLO_SELECT & ~HOME_PIC_BIT);
        sync_reload = 1;                                /* (the editor: the committed values) */
    }
    home_pic_land();
    ui.force = 1;
}
