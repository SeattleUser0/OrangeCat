/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The UI design mockups (docs/orangecat/UI_DESIGN.md, Part 2): the new interface's screens drawn at 240 x 240 through
 * the firmware's own drawing code (firmware/src/gfx.c: the canvas, the band drawing, the screen diff; the Melodee Pixel
 * faces, the NIGHT palette, the 1-bit icons and keycaps of build/gen), so what is shown is what the unit can draw.
 * The engine pictures are scene data (tests/ui_scenes.h) drawn by the scene interpreter (tests/ui_scene.h).
 *
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o ui_mockup tests/ui_mockup.c -lm
 *   ./ui_mockup OUTDIR          (PPM screens and OUTDIR/report.txt; tests/ui_mockup.py makes the PNGs)
 *
 * Checked on every screen, as tests/ui_render.c's lint does: no text off the screen, no two texts overlapping, none
 * cut by its canvas. Measured: each scene's size in bytes, the host draw time of a scene region (only ratios to the
 * existing screens mean anything: tests/ui_render.c reports HOME's), and the pixels each animation frame sends to the
 * panel through the firmware's screen diff (the SPI cost: 57,600 px, a full screen, take about 77 ms at 12 MHz).
 * Nothing here is in the firmware build: the screens are a design to choose from.
 *
 * Revisions:
 *   (Orange Cat 0.15 design, branch ui-design) first version.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* ---- the LCD (host): what is sent lands in host_screen; host_sent counts the pixels sent ---- */
static uint16_t host_screen[240 * 240];
static uint64_t host_sent;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    for (uint32_t j = 0; j < h && y + j < 240u; j++)
        for (uint32_t i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = (uint16_t)((c >> 8) | (c << 8));
    host_sent += (uint64_t)w * h;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    host_sent += (uint64_t)w * h;
    for (uint32_t j = 0; j < h && y + j < 240u; j++)
        for (uint32_t i = 0; i < w && x + i < 240u; i++) host_screen[(y + j) * 240u + x + i] = p[j * w + i];
}

/* ---- the lint (as tests/ui_render.c's, smaller): every text's ink box as it lands on the screen ---- */
typedef struct { int16_t x0, y0, x1, y1; uint8_t fl; char s[28]; } box_t;
static box_t pend[128], scr[512];
static uint32_t npend, nscr, nfind;
static FILE *rep;
static const char *cur = "";
static int32_t cv_region_h;                             /* (gfx.c's, declared below: a text cut by a band is drawn whole
                                                         * by the bands together; the lint sees its parts) */
static void hk_text(int32_t x0, int32_t y0, int32_t x1, int32_t y1, const char *s, uint32_t fl)
{
    if (cv_region_h) fl &= ~2u;
    if (npend < 128u) {
        pend[npend] = (box_t){(int16_t)x0, (int16_t)y0, (int16_t)x1, (int16_t)y1, (uint8_t)fl, {0}};
        strncpy(pend[npend].s, s, 27);
        npend++;
    }
}
static void hk_blit(uint32_t x, uint32_t y, uint32_t r0)
{
    for (uint32_t i = 0; i < npend && nscr < 512u; i++) {
        box_t b = pend[i];
        if (b.y1 <= (int32_t)r0) continue;
        b.x0 = (int16_t)(b.x0 + (int32_t)x); b.x1 = (int16_t)(b.x1 + (int32_t)x);
        b.y0 = (int16_t)(b.y0 + (int32_t)y); b.y1 = (int16_t)(b.y1 + (int32_t)y);
        scr[nscr++] = b;
    }
    npend = 0;
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hk_text(x0, y0, x1, y1, s, flags)
#define GFX_HOOK_BLIT(x, y, r0) hk_blit(x, y, r0)
#define GFX_HOOK_BEGIN() (npend = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)
#include "gfx.c"
/* icons.c wants a few of the firmware's types; the mockups use only its drawing */
typedef struct { int x; } track_t;
typedef struct { int x; } param_desc_t;
typedef struct { int x; } page_t;
#define NTRK 4u
#include "icons.c"
#include "ui_scenes.h"

static void lint_screen(void)
{
    for (uint32_t i = 0; i < nscr; i++) {
        const box_t *a = &scr[i];
        if (a->x0 < 0 || a->y0 < 0 || a->x1 > 240 || a->y1 > 240) {
            fprintf(rep, "LINT %-22s off the screen      '%s' (%d,%d)-(%d,%d)\n", cur, a->s, a->x0, a->y0, a->x1, a->y1);
            nfind++;
        }
        if ((a->fl & 2u) && !(a->fl & 32u)) {
            fprintf(rep, "LINT %-22s cut by its canvas   '%s'\n", cur, a->s);
            nfind++;
        }
        for (uint32_t j = i + 1u; j < nscr; j++) {
            const box_t *b = &scr[j];
            if (!strcmp(a->s, b->s) && a->x0 == b->x0) continue;     /* (one text across two bands) */
            if (a->x0 < b->x1 && b->x0 < a->x1 && a->y0 < b->y1 && b->y0 < a->y1 && !((a->fl | b->fl) & 16u)) {
                fprintf(rep, "LINT %-22s overlaps            '%s' / '%s'\n", cur, a->s, b->s);
                nfind++;
            }
        }
    }
}
static void shot(const char *dir, const char *name)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n240 240\n255\n");
    for (uint32_t i = 0; i < 240u * 240u; i++) {
        uint16_t c = (uint16_t)((host_screen[i] >> 8) | (host_screen[i] << 8));
        uint8_t rgb[3] = {(uint8_t)(((c >> 11) & 31) * 255 / 31), (uint8_t)(((c >> 5) & 63) * 255 / 63), (uint8_t)((c & 31) * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    cur = name;
    lint_screen();
    nscr = 0;
}
static void begin_screen(void)
{
    gfx_forget();                                       /* a screen of its own: everything sent whole */
    nscr = npend = 0;
    lcd_fill(0, 0, 240, 240, T_BG);
}

#define CARD_X0 2                                       /* the knob cells: 57 px at x 2 + 60 k (ui.c CARD_X) */
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
static void fmt_int(char *b, int32_t v)                 /* (ui.c's, as the pages print a number) */
{
    char t[12];
    int n = 0, neg = v < 0;
    uint32_t u = neg ? (uint32_t)-v : (uint32_t)v;
    do { t[n++] = (char)('0' + u % 10u); u /= 10u; } while (u);
    if (neg) *b++ = '-';
    while (n) *b++ = t[--n];
    *b = 0;
}

/* ---- the knobs ---- */
/* the knob cozies' colours (OP-1): blue, green, white, orange. Fixed for every track and every palette: they are the
 * colours of the plastic on the knobs */
static uint16_t KCOL[4];
typedef struct {
    const char *name;
    int16_t lo, hi;              /* its range; a macro: -64 .. 63, 0 the preset as stored */
    uint8_t macro;
    int16_t v;                   /* its value now */
    uint8_t fmt;                 /* how its value reads: 0 a number, 1 hundredths ("1.05"), 2 percent, 3 SPACE (a time
                                  * on the first half, the spring's share on the second) */
} knob_t;
typedef struct {
    const char *engine, *preset, *pos;
    uint32_t track;
    const uint8_t *scene;
    knob_t k[4];
    uint8_t dub;                 /* 1: DUB TAPE's performance view (an effect's picture, its own sources) */
} sound_t;
static int32_t knob_q8(const knob_t *k) { return (int32_t)((k->v - k->lo) * 256 / (k->hi - k->lo)); }
static void knob_text(const knob_t *k, char *b)
{
    static const char *const DT[6] = {"1/16", "1/8", "8D", "1/4", "4D", "1/2"};   /* SPACE's first half: the time */
    if (k->fmt == 1) { fmt_int(b, k->v / 100); b[1] = '.'; b[2] = (char)('0' + k->v / 10 % 10); b[3] = (char)('0' + k->v % 10); b[4] = 0; return; }
    if (k->fmt == 2) { fmt_int(b, k->v); strcat(b, "%"); return; }
    if (k->fmt == 3) {
        if (k->v < 64) { strcpy(b, DT[k->v * 6 / 64]); return; }
        strcpy(b, "SP"); fmt_int(b + 2, (k->v - 64) * 100 / 63); return;
    }
    if (k->macro || k->lo < 0) {
        if (k->v > 0) { b[0] = '+'; fmt_int(b + 1, k->v); }
        else fmt_int(b, k->v);
    } else fmt_int(b, k->v);
}

/* ---- the shared header (ui_draw.c draw_head, as it is): transport, BPM, REC; the chip at the right ---- */
static void head(const char *chip, uint16_t chip_c, int playing, int rec, int dots, int dot)
{
    cv_begin(240, 30, T_BG);
    cv_icon_mid(5, 15, 16, playing ? ICON_X_PLAY : ICON_X_STOP, playing ? T_TEXT : T_MID, T_BG);
    cv_text(26, ty_y(15, TY_TX), TY_TX, "124", T_TEXT);
    int32_t x = 68;
    if (rec) { cv_icon_mid(x, 15, 16, ICON_X_REC, T_REC, T_BG); x += 18; }
    for (int i = 0; i < dots; i++) {
        int32_t w = i == dot ? 16 : 6;
        cv_rrect(x + 6, 12, w, 6, 3, i == dot ? T_THEME : T_LINE, T_BG);
        x += w + 6;
    }
    cv_text_r(236, ty_y(15, TY_TX), TY_TX, chip, chip_c, T_BG);
    cv_blit(0, 0);
}

/* ---- HOME ---- */
#define Y_SOUND 30
#define H_SOUND 24
#define Y_DIALS 54
#define H_DIALS 32
/* the sound line: the track's numeral on its colour, the preset's name, its place in the list; a notice in its place
 * (the knob just turned: its name and value in its colour) */
static void sound_line(const sound_t *s, int hot, const char *notice, uint16_t notice_c)
{
    char b[24];
    cv_begin(240, H_SOUND, T_BG);
    cv_rrect(4, 2, 20, 20, 5, T_TRK(s->track), T_BG);
    b[0] = (char)('1' + s->track); b[1] = 0;
    cv_text_c(14, ty_y(12, TY_TX), TY_TX, b, T_INK, T_TRK(s->track));
    if (notice) {
        cv_text(32, ty_y(12, TY_TX), TY_TX, notice, notice_c);
    } else if (hot >= 0) {
        const knob_t *k = &s->k[hot];
        int32_t x = cv_text(32, ty_y(12, TY_TX), TY_TX, k->name, KCOL[hot]);
        knob_text(k, b);
        cv_text(x + 12, ty_y(12, TY_TX), TY_TX, b, T_TEXT);
    } else {
        cv_text(32, ty_y(12, TY_TX), TY_TX, s->preset, T_TEXT);
        cv_text_r(236, ty_y(12, TY_LB), TY_LB, s->pos, T_MID, T_BG);
    }
    cv_blit(0, Y_SOUND);
}
/* a dial: a ring in the knob's colour, the value's arc on it (a macro's from the top, the preset), a pointer */
static void dial(int32_t cx, int32_t cy, int32_t d, uint32_t k, const knob_t *kn, int hot)
{
    uint16_t c = KCOL[k], quiet = ux_mix(T_BG, c, 30);
    int32_t r = d / 2, a, q = knob_q8(kn);
    sc_depth = 0; sc_xf[0] = (sc_xf_t){0, 0, 0, 0, 4096, 0, 0};
    sc_weight = 1;
    sc_arc16(cx, cy, r, 96, 32, quiet);                 /* the track: 135 degrees either side of the top */
    a = 96 + q * 192 / 256;                             /* the value's angle */
    sc_weight = 2;
    if (kn->macro) {
        if (a > 192) sc_arc16(cx, cy, r, 192, a, c);
        else if (a < 192) sc_arc16(cx, cy, r, a, 192, c);
    } else if (a > 96) sc_arc16(cx, cy, r, 96, a, c);
    sc_weight = hot ? 2 : 1;
    sc_line16(cx * 16, cy * 16, (cx * 16) + (r - 2) * 16 * sc_cos(a) / 4096, (cy * 16) + (r - 2) * 16 * sc_sin(a) / 4096, c);
    cv_disc(cx, cy, hot ? 5 : 4, c, 0);
}
static void dial_strip(const sound_t *s, int hot)
{
    cv_begin(240, H_DIALS, T_BG);
    for (uint32_t k = 0; k < 4u; k++)
        dial(CARD_X0 + 60 * (int32_t)k + 28, 16, 26, k, &s->k[k], hot == (int)k);
    cv_blit(0, Y_DIALS);
}
/* HOME B's row of values: each under its dial, in the knob's colour */
static void value_row(const sound_t *s, int32_t y, int hot)
{
    char b[12];
    cv_begin(240, 22, T_BG);
    for (uint32_t k = 0; k < 4u; k++) {
        knob_text(&s->k[k], b);
        cv_text_c(CARD_X0 + 60 * (int32_t)k + 28, ty_y(10, TY_LB), TY_LB, b, hot == (int)k ? T_TEXT : KCOL[k], T_BG);
    }
    cv_blit(0, (uint32_t)y);
}
/* the scene's sources for a frame */
typedef struct { int32_t env, hit, gate, note, t_ms, keys, thr; } frame_t;   /* (DUB TAPE: keys a bit a key held, white
                                                                            * 0..6 then black 8..12; thr the throw held, 0 none) */
static void scene_inputs(const sound_t *s, const frame_t *f, scene_in_t *in)
{
    memset(in, 0, sizeof *in);
    for (uint32_t k = 0; k < 4u; k++) {
        in->s[SS_K1 + k] = (int16_t)knob_q8(&s->k[k]);
        /* the knob-rate phases: 0.1 Hz at 0 to 3 Hz at full (the UI keeps these four phases a track) */
        double hz = 0.1 + 2.9 * knob_q8(&s->k[k]) / 256.0;
        if (s->dub && k == 0u) {                         /* DUB TAPE: the loop's turn, slow to 1.0, fast past it (HOLD: 1.0) */
            double fb = (f->keys >> 9) & 1 ? 1.0 : s->k[0].v / 100.0;
            hz = 0.3 + 0.5 * (fb < 1.0 ? fb : 1.0) + 8.0 * (fb > 1.0 ? fb - 1.0 : 0.0);
        }
        in->s[SS_PH1 + k] = (int16_t)((int64_t)(fmod(f->t_ms / 1000.0 * hz, 1.0) * 256.0));
    }
    in->s[SS_ENV] = (int16_t)f->env;
    in->s[SS_HIT] = (int16_t)f->hit;
    in->s[SS_GATE] = (int16_t)f->gate;
    if (s->dub) {                                       /* SEND in force: the throw held, else the knob; CUT closes the input */
        if (f->thr) in->s[SS_K2] = (int16_t)(f->thr * 256 / 100);
        in->s[SS_GATE] = (int16_t)((f->keys >> 6) & 1 ? 0 : 256);
    }
    in->s[SS_NOTE] = (int16_t)(f->note * 256 / 12);
    in->s[SS_TIME] = (int16_t)((f->t_ms % 1000) * 256 / 1000);
    in->s[SS_WOB] = (int16_t)lround(256.0 * sin(f->t_ms * 2.0 * M_PI / 2000.0));
    in->col[SC_INK] = T_SEC;
    for (uint32_t k = 0; k < 4u; k++) in->col[SC_K1 + k] = KCOL[k];
    in->col[SC_TRK] = T_TRK(s->track);
    in->col[SC_DIM] = T_LINE;
    in->col[SC_BG] = T_BG;
    for (uint32_t i = 0; i < 64u; i++) {                /* a stand-in for the track's scope: a saw with some of knob 1 */
        double ph = fmod(i / 21.0 + f->t_ms / 700.0, 1.0);
        double v = (ph * 2.0 - 1.0) * 0.6 + 0.4 * sin(i * 0.9 + f->t_ms / 300.0) * in->s[SS_K1] / 256.0;
        in->scope[i] = (int16_t)(v * (40 + f->env * 216 / 256));
    }
}
static uint32_t scene_bytes;
static double scene_us;
/* the picture: the region drawn in bands (CV_BANDS), the scene anchored to its bottom */
static void picture(const sound_t *s, int32_t y, int32_t h, const frame_t *f)
{
    scene_in_t in;
    struct timespec t0, t1;
    scene_inputs(s, f, &in);
    clock_gettime(CLOCK_MONOTONIC, &t0);
    CV_BANDS(0, (uint32_t)y, 240, (uint32_t)h, T_BG) {
        scene_bytes = scene_draw(s->scene, 0, h - s->scene[2], &in);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    scene_us = (t1.tv_sec - t0.tv_sec) * 1e6 + (t1.tv_nsec - t0.tv_nsec) / 1e3;
}
enum { HOME_A, HOME_B };
static void home(const sound_t *s, int variant, int hot, const frame_t *f, const char *notice, uint16_t nc)
{
    palette_track(s->track);
    head(s->engine, T_THEME, 1, 0, 2, 0);
    sound_line(s, hot, notice, nc);
    dial_strip(s, hot);
    if (variant == HOME_A)
        picture(s, Y_DIALS + H_DIALS, 240 - Y_DIALS - H_DIALS, f);
    else {
        value_row(s, Y_DIALS + H_DIALS, hot);
        picture(s, Y_DIALS + H_DIALS + 22, 240 - Y_DIALS - H_DIALS - 22, f);
    }
}

/* ---- the sounds of the mockups (the presets' values; the knobs as UI_DESIGN.md's table) ---- */
static sound_t S_MACRO = {"MACRO", "VA SYNC LEAD", "2/27", 0, SCENE_MACRO,
    {{"HARM", 0, 127, 0, 76}, {"TIMB", 0, 127, 0, 90}, {"MRPH", 0, 127, 0, 40}, {"PLUCK", -64, 63, 1, 0}}};
static sound_t S_SHAPES = {"SHAPES", "MORPH LEAD", "2/24", 1, SCENE_SHAPES,
    {{"TIMB", -64, 63, 1, 0}, {"COLR", 0, 127, 0, 64}, {"TONE", -64, 63, 1, 0}, {"PLUCK", -64, 63, 1, 0}}};
static sound_t S_DRUMX = {"DRUM-X", "909 CLUB", "2/9", 3, SCENE_DRUMX,
    {{"MRPH", 0, 127, 0, 60}, {"TUNE", 0, 127, 0, 64}, {"TONE", 0, 127, 0, 76}, {"DECY", 0, 127, 0, 64}}};
static sound_t S_WHEEL = {"WHEEL", "JAZZ PERC", "2/5", 2, SCENE_WHEEL,
    {{"SUB", -8, 8, 0, 0}, {"BODY", -8, 8, 0, 0}, {"TOP", -8, 8, 0, 0}, {"SPIN", -64, 63, 1, 0}}};

static sound_t S_PROPHET = {"PROPHET", "INIT PROPHET", "", 0, SCENE_PROPHET,
    {{"CUTOFF", 0, 127, 0, 64}, {"RES", 0, 127, 0, 20}, {"ENV AMT", 0, 127, 0, 50}, {"SHAPE", -64, 63, 1, 0}}};
static sound_t S_SEM = {"SEM", "SEM BRASS", "1/8", 1, SCENE_SEM,
    {{"CUT", 0, 127, 0, 48}, {"RES", 0, 127, 0, 24}, {"STAT", 0, 128, 0, 0}, {"DRIFT", -64, 63, 1, 0}}};
static sound_t S_HADES = {"HADES", "DEEP SUB", "1/8", 2, SCENE_HADES,
    {{"CUT", 0, 127, 0, 44}, {"RES", 0, 127, 0, 12}, {"DRV", 0, 127, 0, 8}, {"SUBS", -64, 63, 1, 0}}};
static sound_t S_FMBELL = {"FMBELL", "FM BELL", "1/4", 3, SCENE_FMBELL,
    {{"RAT", 0, 127, 0, 32}, {"INDX", 0, 127, 0, 62}, {"DCAY", 0, 127, 0, 31}, {"FDBK", 0, 127, 0, 0}}};
static sound_t S_SUPERSAW = {"SUPERSAW", "SUPERSAW", "1/4", 0, SCENE_SUPERSAW,
    {{"DTUN", 0, 127, 0, 31}, {"MRPH", 0, 127, 0, 0}, {"SUB", 0, 127, 0, 47}, {"DRIV", 0, 127, 0, 23}}};
static sound_t S_DDDD = {"DDDD", "WALL", "1/4", 1, SCENE_DDDD,
    {{"SRC", 0, 127, 0, 0}, {"FDBK", 0, 127, 0, 93}, {"DSTR", 0, 127, 0, 78}, {"HAND", 0, 127, 0, 31}}};
static sound_t S_CZ1 = {"CZ-1", "SYN STRINGS", "", 2, SCENE_CZ1,
    {{"BRIGHT", -64, 63, 1, 0}, {"WARP", -64, 63, 1, 0}, {"DETUNE", -64, 63, 1, 0}, {"SHAPE", -64, 63, 1, 0}}};
static sound_t S_SID = {"SID", "BREADBOX", "", 3, SCENE_SID,
    {{"CUT", 0, 127, 0, 60}, {"RES", 0, 15, 0, 6}, {"COLOR", -64, 63, 1, 0}, {"DRIVE", 0, 127, 0, 0}}};
/* DUB TAPE on track 1: FEEDBACK 0 .. 1.30 (hundredths), SEND %, TONE (low-pass at 0 to high-pass at 127), SPACE */
static sound_t S_DUB = {"DUB TAPE", "VA SYNC LEAD", "SKANK", 0, SCENE_DUB,
    {{"FDBK", 0, 130, 0, 55, 1}, {"SEND", 0, 100, 0, 30, 2}, {"TONE", 0, 127, 0, 64}, {"SPACE", 0, 127, 0, 40, 3}}, 1};

/* ---- DUB TAPE's performance view (SPEC.md 9.3.2, branch dub-tape) ----
 * HOME's layout with the effect's picture (240 x 80) and, under it, the octave of performance keys: seven white keys
 * (throws at the six levels, CUT), five black (KICK, HOLD, CRASH, two spare). Each key in the colour of the knob it
 * acts on: the throws SEND's green, HOLD FEEDBACK's blue, KICK and CRASH SPACE's orange (the spring), CUT REC's red.
 * A key held is filled in its colour. The black keys' names sit in a row above the keys (a name is wider than a key). */
#define Y_DUBPIC (Y_DIALS + H_DIALS)
#define H_DUBPIC 80
#define Y_KEYS (Y_DUBPIC + H_DUBPIC)
static void dub_keys(uint32_t keys)
{
    static const char *const WL[7] = {"10", "30", "50", "70", "85", "100", "CUT"};
    static const int BW[5] = {0, 1, 2, 4, 5};          /* a black key after white key k: F# G# A# C# D# */
    static const char *const BL[5] = {"KICK", "HOLD", "CRASH", "", ""};
    uint16_t bc[5] = {KCOL[3], KCOL[0], KCOL[3], T_DIM, T_DIM};
    int32_t bx[5], lx[3];
    CV_BANDS(0, Y_KEYS, 240, 240 - Y_KEYS, T_BG) {
        for (int k = 0; k < 7; k++) {                   /* the white keys */
            int32_t x = 2 + k * 34, on = (keys >> k) & 1;
            uint16_t c = k == 6 ? T_REC : KCOL[1], f = on ? c : T_QUIET;
            cv_rrect(x, 20, 32, 52, 4, on ? c : T_LINE, T_BG);   /* (an edge, so the keys read as keys) */
            cv_rrect(x + 1, 21, 30, 50, 3, f, on ? c : T_LINE);
            cv_text_c(x + 16, ty_y(61, TY_LB), TY_LB, WL[k], on ? T_INK : c, f);
        }
        for (int b = 0; b < 5; b++) {                   /* the black keys */
            int32_t on = (keys >> (8 + b)) & 1;
            bx[b] = 2 + (BW[b] + 1) * 34 - 1;
            cv_rrect(bx[b] - 11, 18, 22, 30, 3, T_BG, T_QUIET);
            cv_rrect(bx[b] - 10, 18, 20, 29, 3, on ? bc[b] : ux_mix(T_BG, bc[b], 28), T_BG);
        }
        /* their names: KICK flush left, HOLD centred, CRASH from just past HOLD; a tick down to each key */
        lx[0] = 2; lx[1] = bx[1] - text_w(TY_LB, BL[1]) / 2; lx[2] = lx[1] + text_w(TY_LB, BL[1]) + 6;
        for (int b = 0; b < 3; b++) {
            int32_t on = (keys >> (8 + b)) & 1;
            cv_text(lx[b], ty_y(8, TY_LB), TY_LB, BL[b], on ? T_TEXT : bc[b]);
            cv_rect(bx[b], 15, 1, 4, bc[b]);
        }
    }
}
static void dub_view(const sound_t *s, int hot, const frame_t *f)
{
    palette_track(s->track);
    head("DUB TAPE", T_THEME, 1, 0, 3, 1);
    sound_line(s, hot, 0, 0);
    dial_strip(s, hot);
    picture(s, Y_DUBPIC, H_DUBPIC, f);
    dub_keys((uint32_t)f->keys);
}

/* ---- the other pages ---- */
/* a card as the pages draw it today (ui_draw.c draw_column): the label in its knob's colour, the value, the gauge */
static void card(int32_t x, int32_t y, const char *label, const char *val, uint16_t lc, int32_t ratio, int on, int hot)
{
    cv_begin(57, 48, T_BG);
    if (on) cv_rrect(0, 0, 57, 48, 6, hot ? ux_mix(T_BG, lc, 22) : T_QUIET, T_BG);
    cv_text_c(28, ty_y(11, TY_LB), TY_LB, label, on ? lc : T_DIM, on ? (hot ? ux_mix(T_BG, lc, 22) : T_QUIET) : T_BG);
    cv_text_c(28, ty_y(30, TY_TX), TY_TX, val, on ? T_TEXT : T_DIM, on ? (hot ? ux_mix(T_BG, lc, 22) : T_QUIET) : T_BG);
    if (ratio >= 0) {
        cv_rect(8, 42, 41, 2, on ? T_LINE : ux_mix(T_BG, T_LINE, 50));
        cv_rect(8, 42, 41 * ratio / 1000, 2, on ? lc : T_DIM);
    }
    cv_blit((uint32_t)x, (uint32_t)y);
}
/* the tabs of EDIT's sheets under the header */
static void tabs(const char *const *t, uint32_t n, uint32_t on, int32_t y)
{
    cv_begin(240, 22, T_BG);
    int32_t x = 4;
    for (uint32_t i = 0; i < n; i++) {
        int32_t w = text_w(TY_LB, t[i]) + 8;
        if (i == on) cv_rrect(x, 1, w, 20, 5, T_THEME, T_BG);
        cv_text(x + 4, ty_y(11, TY_LB), TY_LB, t[i], i == on ? T_INK : T_MID);
        x += w + 2;
    }
    cv_blit(0, (uint32_t)y);
}
static void foot_keys(const khint_t *h, uint32_t n)
{
    cv_begin(240, 28, T_BG);
    cv_key_row(4, 236, 3, h, n, 0xFFu, T_BG);
    cv_blit(0, 212);
}
/* EDIT's first sheet (SOUND) on a MACRO track: the eight controls in two rows of four; the knobs on the row framed */
static void edit_sheet(void)
{
    static const char *const T[] = {"SOUND", "ENV", "MOD", "FX", "VOICE"};
    static const char *const L[8] = {"MODL", "HARM", "TIMB", "MRPH", "AUX", "LPG", "DECY", "TMOD"};
    static const char *const V[8] = {"VA", "76", "90", "40", "0", "0", "70", "0"};
    static const int R[8] = {-1, 598, 709, 315, 0, 0, 551, 500};
    palette_track(0);
    head("MACRO", T_THEME, 1, 0, 5, 0);
    tabs(T, 5, 0, 30);
    for (uint32_t i = 0; i < 8u; i++)
        card(CARD_X0 + 60 * (int32_t)(i % 4u), 56 + 52 * (int32_t)(i / 4u), L[i], V[i], KCOL[i % 4u], R[i], i < 4u, i == 1u);
    /* what the row's knobs are, and what the four HOME knobs are made of */
    cv_begin(240, 48, T_BG);
    cv_rrect(4, 2, 232, 44, 6, T_QUIET, T_BG);
    int32_t x = 14;
    static const char *const HK[4] = {"HARM", "TIMB", "MRPH", "PLUCK"};
    for (uint32_t k = 0; k < 4u; k++) x = cv_text(x, ty_y(14, TY_LB), TY_LB, HK[k], KCOL[k]) + 8;
    cv_text(12, ty_y(34, TY_LB), TY_LB, "PLUCK: LPG DECY ATK REL", T_SEC);
    cv_blit(0, 160);
    static const khint_t F[] = {{KC_SELECT, "ROW 1/2"}, {KC_EDIT, "NEXT"}};
    foot_keys(F, 2);
}
/* NOTES: the piano roll, each note its own length (they overlap); the cursor's note picked */
static void notes_page(int feel)
{
    static const char *const T[] = {"NOTES", "FEEL", "TRACKER"};
    palette_track(0);
    head("STEP 5", T_THEME, 1, 0, 4, feel);
    tabs(T, 3, (uint32_t)feel, 30);
    if (!feel) {
        card(CARD_X0, 56, "PITCH", "E4", KCOL[0], -1, 1, 0);
        card(CARD_X0 + 60, 56, "START", "5.00", KCOL[1], -1, 1, 0);
        card(CARD_X0 + 120, 56, "LEN", "3.50", KCOL[2], -1, 1, 1);
        card(CARD_X0 + 180, 56, "VEL", "100", KCOL[3], 787, 1, 0);
        /* the roll: 16 steps across, 12 rows (C4 .. B4) */
        static const struct { int step, row, len16; } N[] = {{0, 0, 16}, {0, 4, 32}, {0, 7, 64}, {4, 4, 56}, {6, 9, 12},
                                                            {8, 0, 8}, {9, 2, 8}, {10, 4, 28}, {12, 7, 20}, {14, 11, 16}};
        CV_BANDS(0, 108, 240, 104, T_BG) {
            int32_t x0 = 22, cw = 13, rh = 8, y0 = 4;
            for (int r = 0; r < 12; r++) {
                static const int black[12] = {0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0};
                cv_rect(x0, y0 + (11 - r) * rh, 16 * cw, rh - 1, black[r] ? T_QUIET : T_LANE);
                cv_rect(4, y0 + (11 - r) * rh, 14, rh - 1, black[r] ? T_DIM : T_SEC);
            }
            for (int c = 0; c <= 16; c++) cv_rect(x0 + c * cw, y0, 1, 12 * rh - 1, c % 4 ? T_GRID : T_LINE);
            for (unsigned i = 0; i < NELEM(N); i++) {
                int32_t x = x0 + N[i].step * cw + 1, w = N[i].len16 * cw / 16 - 2, yy = y0 + (11 - N[i].row) * rh + 1;
                int picked = i == 3;
                if (x + w > x0 + 16 * cw) w = x0 + 16 * cw - x;
                cv_rrect(x, yy, w, rh - 3, 2, picked ? T_ACCENT : T_THEME, T_LANE);
                if (picked) cv_frame(x - 1, yy - 1, w + 2, rh - 1, T_TEXT);
            }
            cv_rect(x0 + 4 * cw, y0, 1, 12 * rh - 1, T_ACCENT);   /* the cursor's step */
        }
        static const khint_t F[] = {{KC_PRESETS, "STEP"}, {KC_KEYS, "PICK"}};
        foot_keys(F, 2);
    } else {
        static const char *const KL[4] = {"CHANCE", "RATCH", "NUDGE", "TYPE"};
        static const char *const KV[4] = {"75%", "x2", "+6", "ACC"};
        for (uint32_t k = 0; k < 4u; k++) card(CARD_X0 + 60 * (int32_t)k, 56, KL[k], KV[k], KCOL[k], -1, 1, k == 0u);
        /* 16 tiles, a step each: its CHANCE (the knob turned last) */
        static const char *const TV[16] = {"", "", "", "", "75", "", "x2", "", "", "50", "", "", "1:2", "", "FL", ""};
        static const int ON[16] = {1, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 1, 0, 1, 1};
        CV_BANDS(0, 108, 240, 104, T_BG) {
            for (int i = 0; i < 16; i++) {
                int32_t x = 6 + (i % 8) * 29, y = 6 + (i / 8) * 48;
                uint16_t bg = i == 4 ? ux_mix(T_BG, KCOL[0], 26) : ON[i] ? T_QUIET : T_BG;
                cv_rrect(x, y, 26, 42, 5, bg, T_BG);
                if (!ON[i]) cv_frame(x, y, 26, 42, T_LINE);
                char nb[4]; fmt_int(nb, i + 1);
                cv_text_c(x + 13, ty_y(y + 10, TY_LB), TY_LB, nb, ON[i] ? T_MID : T_DIM, bg);
                if (TV[i][0]) cv_text_c(x + 13, ty_y(y + 30, TY_LB), TY_LB, TV[i], i == 4 ? T_TEXT : T_SEC, bg);
            }
            cv_frame(5, 5, 4 * 29 + 1, 44, T_ACCENT);                /* the selection: steps 1 .. 4 */
        }
        static const khint_t F[] = {{KC_PRESETS, "STEP"}, {KC_SEQ, "NEXT"}};
        foot_keys(F, 2);
    }
}
/* EDIT > LANES on a DRUM-X track: the eight sounds, a row each: LEVEL TUNE DECAY PAN, the columns in the knobs' colours
 * (KNOB 1..4 edit the picked lane's four); the picked lane shows its numbers, the others their bars */
static void drum_lanes(void)
{
    static const char *const T[] = {"KIT", "LANES", "ENV", "FX"};
    static const char *const LN[8] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    static const char *const COL[4] = {"LEVEL", "TUNE", "DECAY", "PAN"};
    static const int VAL[8][4] = {{110, 0, 64, 0}, {96, 2, 50, -6}, {88, 0, 40, 8}, {70, 5, 20, -14},
                                  {66, 5, 70, -14}, {90, -3, 60, 12}, {60, 0, 18, 4}, {58, 7, 30, 18}};
    palette_track(3);
    head("DRUM-X", T_THEME, 1, 0, 4, 1);
    tabs(T, 4, 1, 30);
    cv_begin(240, 20, T_BG);                            /* the columns: the knobs' names in their colours */
    for (int k = 0; k < 4; k++) cv_text_c(58 + k * 50, ty_y(10, TY_LB), TY_LB, COL[k], KCOL[k], T_BG);
    cv_blit(0, 54);
    CV_BANDS(0, 76, 240, 136, T_BG) {
        for (int l = 0; l < 8; l++) {
            int32_t y = l * 17, sel = l == 1;
            uint16_t bg = sel ? ux_mix(T_BG, T_THEME, 22) : T_BG;
            if (sel) cv_rrect(2, y, 236, 16, 4, bg, T_BG);
            cv_text(8, ty_y(y + 8, TY_LB), TY_LB, LN[l], sel ? T_TEXT : T_SEC);
            for (int k = 0; k < 4; k++) {
                int32_t x = 38 + k * 50, w = 40, v = VAL[l][k];
                if (sel) {                              /* the picked lane: its numbers */
                    char b[8];
                    if (k == 3) { b[0] = v < 0 ? 'L' : v > 0 ? 'R' : 'C'; fmt_int(b + 1, v < 0 ? -v : v); if (!v) b[1] = 0; }
                    else if (k == 1 && v > 0) { b[0] = '+'; fmt_int(b + 1, v); }
                    else fmt_int(b, v);
                    cv_text_c(x + w / 2, ty_y(y + 8, TY_LB), TY_LB, b, KCOL[k], bg);
                    continue;
                }
                uint16_t c = ux_mix(T_BG, KCOL[k], 60);
                cv_rect(x, y + 8, w, 1, T_LINE);
                if (k == 0 || k == 2) cv_rect(x, y + 6, w * v / 127, 5, c);
                else {                                  /* tune, pan: from the middle */
                    int32_t m = x + w / 2, d = w / 2 * v / (k == 1 ? 12 : 64);
                    cv_rect(d < 0 ? m + d : m, y + 6, d < 0 ? -d : d, 5, c);
                    cv_rect(m, y + 4, 1, 9, T_MID);
                }
            }
        }
    }
    static const khint_t F[] = {{KC_PRESETS, "LANE"}, {KC_EDIT, "NEXT"}};
    foot_keys(F, 2);
}
/* MIXER (GLO): a strip a track: its numeral, the sound, LEVEL a fader, PAN, the sends; KNOB k: track k's LEVEL */
static void mixer(void)
{
    static const char *const NM[4] = {"VA SYNC", "MORPH", "JAZZ", "909 CLUB"};
    static const int LV[4] = {96, 80, 70, 104}, PN[4] = {0, -20, 16, 0}, RV[4] = {40, 70, 30, 10};
    palette_track(0);
    head("MIXER", T_TEXT, 1, 0, 0, 0);
    for (uint32_t k = 0; k < 4u; k++) {
        cv_begin(57, 182, T_BG);
        int32_t x = 0;
        uint16_t tc = T_TRK(k), bg = k == 0u ? ux_mix(T_BG, tc, 18) : T_QUIET;
        cv_rrect(x, 0, 57, 182, 6, bg, T_BG);
        char nb[2] = {(char)('1' + k), 0};
        cv_rrect(19, 4, 20, 20, 5, tc, bg);
        cv_text_c(29, ty_y(14, TY_TX), TY_TX, nb, T_INK, tc);
        cv_text_c(28, ty_y(36, TY_LB), TY_LB, NM[k][0] == '9' ? "909" : k == 2u ? "JAZZ" : k == 1u ? "MORPH" : "VA", T_SEC, bg);
        /* the fader, in the knob's colour (KNOB k is track k's LEVEL) */
        cv_rect(27, 52, 3, 92, T_LINE);
        int32_t fy = 52 + 92 - 92 * LV[k] / 127;
        cv_rect(27, fy, 3, 52 + 92 - fy, KCOL[k]);
        cv_rrect(16, fy - 4, 25, 8, 3, KCOL[k], bg);
        /* the pan, the reverb send */
        cv_rect(8, 154, 41, 2, T_LINE);
        cv_rect(28 + PN[k] * 20 / 64, 151, 2, 8, T_TEXT);
        cv_rect(8, 168, 41, 2, T_LINE);
        cv_rect(8, 167, 41 * RV[k] / 127, 4, T_MID);
        cv_blit(CARD_X0 + 60 * k, 30);
    }
    (void)NM;
    static const khint_t F[] = {{KC_K14, "LEVELS"}, {KC_ALGO, "TRACK"}};
    foot_keys(F, 2);
}
/* the SHIFT map (HOME held over HOME): what every control does on this page, 4 rows of 3 */
static void shift_map(void)
{
    static const char *const C[12][2] = {
        {"PRESETS", "SOUND"}, {"ALGO", "TRACK"}, {"SELECT", "TRACKS"},
        {"K1-4", "KNOBS"}, {"EDIT", "SHEETS"}, {"SEQ", "STEPS"},
        {"+K1-4", "FINE"}, {"+PRESETS", "ENGINE"}, {"+EDIT", "INIT"},
        {"+OCT-", "COPY"}, {"+OCT+", "PASTE"}, {"LET GO", "MENU"}};
    palette_track(0);
    head("SHIFT", T_TEXT, 1, 0, 0, 0);
    for (int i = 0; i < 12; i++) {
        int32_t x = 4 + (i % 3) * 78, y = 34 + (i / 3) * 50;
        int combo = C[i][0][0] == '+', home = i == 11;
        const char *ctl = combo ? C[i][0] + 1 : C[i][0];
        cv_begin(76, 46, T_BG);
        cv_rrect(0, 0, 76, 46, 6, combo ? ux_mix(T_BG, T_THEME, 20) : T_QUIET, T_BG);
        if (combo) { cv_rect(4, 7, 7, 1, T_THEME); cv_rect(7, 4, 1, 7, T_THEME); }   /* the plus: with HOME held */
        cv_text_c(combo ? 41 : 38, ty_y(13, TY_LB), TY_LB, ctl, combo ? T_THEME : home ? T_MID : T_SEC, combo ? ux_mix(T_BG, T_THEME, 20) : T_QUIET);
        cv_text_c(38, ty_y(33, TY_LB), TY_LB, C[i][1], T_TEXT, combo ? ux_mix(T_BG, T_THEME, 20) : T_QUIET);
        cv_blit((uint32_t)x, (uint32_t)y);
    }
}
/* a notice on HOME: a card over the foot of the picture for two seconds (the reason the knob did nothing, a refusal) */
static void notice_card(const char *l1, const char *l2)
{
    cv_begin(232, 52, T_BG);
    cv_rrect(0, 0, 232, 52, 8, T_SURF, T_BG);
    cv_rrect(0, 0, 6, 52, 3, T_REC, T_SURF);
    cv_text(16, ty_y(15, TY_TX), TY_TX, l1, T_TEXT);
    cv_text(16, ty_y(38, TY_LB), TY_LB, l2, T_SEC);
    cv_blit(4, 182);
}

/* ---- the run ---- */
static frame_t F_REST = {0, 0, 0, 0, 0}, F_NOTE = {220, 256, 256, 4, 600};
static void set_knobs(sound_t *s, int a, int b, int c, int d) { s->k[0].v = (int16_t)a; s->k[1].v = (int16_t)b; s->k[2].v = (int16_t)c; s->k[3].v = (int16_t)d; }

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/ui_mockup";
    char path[512];
    snprintf(path, sizeof path, "%s/report.txt", dir);
    rep = fopen(path, "w");
    if (!rep) { fprintf(stderr, "ui_mockup: cannot write %s\n", path); return 1; }
    palette_set(0);                                     /* NIGHT */
    KCOL[0] = RGB(77, 158, 255); KCOL[1] = RGB(0, 226, 140); KCOL[2] = RGB(236, 236, 236); KCOL[3] = RGB(255, 128, 32);
    sound_t *S[12] = {&S_MACRO, &S_SHAPES, &S_DRUMX, &S_WHEEL, &S_PROPHET, &S_SEM, &S_HADES, &S_FMBELL, &S_SUPERSAW,
                      &S_DDDD, &S_CZ1, &S_SID};
    static const char *const N[12] = {"macro", "shapes", "drumx", "wheel", "prophet", "sem", "hades", "fmbell", "supersaw",
                                      "dddd", "cz1", "sid"};
    /* two knob positions each: as the preset stands, then the knobs turned (the second, hot, named in the sound line) */
    static const int16_t TURN[12][4] = {{30, 20, 120, 50}, {50, 110, -50, 48}, {127, 30, 120, 127}, {8, -6, 7, 60},
                                        {120, 110, 127, 50}, {110, 120, 128, 60}, {127, 127, 127, 60}, {110, 127, 127, 60},
                                        {127, 127, 127, 127}, {90, 40, 127, 127}, {60, 60, 60, 60}, {127, 15, 60, 127}};
    fprintf(rep, "UI design mockups (tests/ui_mockup.c), palette NIGHT\n\n");
    fprintf(rep, "%-8s %6s %10s %12s %12s %12s %12s   %7s %7s %7s %7s\n", "scene", "bytes", "draw us", "still px", "anim px",
            "turn px", "note px", "2s mean", "2s max", "mean ms", "max ms");
    for (int e = 0; e < 12; e++) {
        sound_t *s = S[e];
        knob_t keep[4];
        memcpy(keep, s->k, sizeof keep);
        char name[64];
        begin_screen();
        home(s, HOME_A, -1, &F_REST, 0, 0);
        snprintf(name, sizeof name, "home_A_%s_1", N[e]); shot(dir, name);
        begin_screen();
        set_knobs(s, TURN[e][0], TURN[e][1], TURN[e][2], TURN[e][3]);
        home(s, HOME_A, 1, &F_NOTE, 0, 0);
        snprintf(name, sizeof name, "home_A_%s_2", N[e]); shot(dir, name);
        memcpy(s->k, keep, sizeof keep);
        if (e < 4) {                                    /* (HOME B: the first four only; the pick is the same for all) */
        begin_screen();
        home(s, HOME_B, -1, &F_REST, 0, 0);
        snprintf(name, sizeof name, "home_B_%s_1", N[e]); shot(dir, name);
        begin_screen();
        set_knobs(s, TURN[e][0], TURN[e][1], TURN[e][2], TURN[e][3]);
        home(s, HOME_B, 1, &F_NOTE, 0, 0);
        snprintf(name, sizeof name, "home_B_%s_2", N[e]); shot(dir, name);
        memcpy(s->k, keep, sizeof keep);
        }
        /* the costs of HOME A's picture: a frame with nothing new, an animation frame (the clock 50 ms on: 20 frames a
         * second), a knob turned one detent (and its dial), a note-on (the envelope and the hit) */
        uint64_t still, anim, turn, note;
        frame_t f = F_REST;
        f.t_ms = 1000;
        begin_screen();
        home(s, HOME_A, -1, &f, 0, 0);
        double us = 0;
        for (int i = 0; i < 20; i++) { picture(s, Y_DIALS + H_DIALS, 154, &f); us += scene_us; }
        us /= 20;
        host_sent = 0; picture(s, Y_DIALS + H_DIALS, 154, &f); still = host_sent;
        f.t_ms += 50; host_sent = 0; picture(s, Y_DIALS + H_DIALS, 154, &f); anim = host_sent;
        s->k[1].v = (int16_t)(s->k[1].v + (s->k[1].macro ? 2 : 1));
        host_sent = 0; dial_strip(s, 1); sound_line(s, 1, 0, 0); picture(s, Y_DIALS + H_DIALS, 154, &f); turn = host_sent;
        frame_t g = f; g.env = 200; g.hit = 256; g.gate = 256; g.note = 7;
        host_sent = 0; picture(s, Y_DIALS + H_DIALS, 154, &g); note = host_sent;
        memcpy(s->k, keep, sizeof keep);
        /* two seconds played at 20 frames a second: a note every 500 ms (held 300 ms, its envelope falling after),
         * KNOB 2 turned a detent every 100 ms in the second second: the mean and the worst frame */
        uint64_t sum = 0, worst = 0;
        double dsum = 0, dworst = 0;
        begin_screen();
        home(s, HOME_A, -1, &F_REST, 0, 0);
        for (int i = 0; i < 40; i++) {
            int32_t t = 3000 + i * 50, since = (i * 50) % 500;
            frame_t g = {since < 300 ? 220 : 220 - (since - 300) * 220 / 200, since < 160 ? 256 - since * 256 / 160 : 0,
                         since < 300 ? 256 : 0, (i / 10) * 3 % 12, t};
            host_sent = 0;
            if (i >= 20 && i % 2 == 0) {
                s->k[1].v = (int16_t)(s->k[1].v + (s->k[1].v + 2 < s->k[1].hi ? 2 : -20));
                dial_strip(s, 1);
                sound_line(s, 1, 0, 0);
            }
            picture(s, Y_DIALS + H_DIALS, 154, &g);
            sum += host_sent;
            if (host_sent > worst) worst = host_sent;
            dsum += scene_us;
            if (scene_us > dworst) dworst = scene_us;
        }
        memcpy(s->k, keep, sizeof keep);
        fprintf(rep, "%-8s %6u %10.1f %12llu %12llu %12llu %12llu   %7llu %7llu %7.1f %7.1f\n", N[e], scene_bytes, us,
                (unsigned long long)still, (unsigned long long)anim, (unsigned long long)turn, (unsigned long long)note,
                (unsigned long long)(sum / 40u), (unsigned long long)worst, sum / 40.0 * 77.0 / 57600.0, worst * 77.0 / 57600.0);
        (void)dsum; (void)dworst;
    }
    /* the filmstrip: four frames of WHEEL's picture, a chord held, SPIN turned up, 100 ms apart */
    for (int i = 0; i < 4; i++) {
        frame_t f = {200 - i * 30, i == 0 ? 256 : 0, 256, (i * 4) % 12, 2000 + i * 100};
        char name[32];
        begin_screen();
        S_WHEEL.k[3].v = 50;
        home(&S_WHEEL, HOME_A, -1, &f, 0, 0);
        snprintf(name, sizeof name, "film_wheel_%d", i); shot(dir, name);
        S_WHEEL.k[3].v = 0;
    }
    /* DUB TAPE: resting; building past 1.0 with the 100 throw held and the spring in; KICK struck with HOLD held, CUT
     * held; then a filmstrip: the throw walked down 100, 70, 10 and let go while the loop builds */
    {
        frame_t f1 = {120, 0, 256, 0, 1000, 0, 0};
        begin_screen(); dub_view(&S_DUB, -1, &f1); shot(dir, "dub_1");
        S_DUB.k[0].v = 115; S_DUB.k[3].v = 100;
        frame_t f2 = {220, 0, 256, 0, 1300, 1u << 5, 100};
        begin_screen(); dub_view(&S_DUB, 0, &f2); shot(dir, "dub_2");
        frame_t f3 = {200, 256, 256, 0, 1600, (1u << 8) | (1u << 9) | (1u << 6), 0};
        begin_screen(); dub_view(&S_DUB, -1, &f3); shot(dir, "dub_3");
        static const int WALK[4] = {5, 3, 0, -1};       /* the keys: 100, 70, 10, none */
        static const int LV[6] = {10, 30, 50, 70, 85, 100};
        for (int i = 0; i < 4; i++) {
            frame_t g = {200, 0, 256, 0, 2000 + i * 150, WALK[i] >= 0 ? 1 << WALK[i] : 0, WALK[i] >= 0 ? LV[WALK[i]] : 0};
            char name[32];
            begin_screen(); dub_view(&S_DUB, -1, &g);
            snprintf(name, sizeof name, "film_dub_%d", i); shot(dir, name);
        }
        /* its cost: two seconds at 20 frames a second, FEEDBACK 1.15 (spinning fast), a throw key every 250 ms, KICK every
         * 500 ms: the picture and the keys redrawn each frame (only what changed is sent) */
        uint64_t sum = 0, worst = 0, still;
        frame_t g = {200, 0, 256, 0, 3000, 0, 0};
        begin_screen(); dub_view(&S_DUB, -1, &g);
        host_sent = 0; picture(&S_DUB, Y_DUBPIC, H_DUBPIC, &g); dub_keys(0); still = host_sent;
        for (int i = 0; i < 40; i++) {
            int32_t since = (i * 50) % 500, k = (i / 5) % 2 ? 1 << ((i / 5) % 6) : 0;
            frame_t h = {200, since < 160 ? 256 - since * 256 / 160 : 0, 256, 0, 3000 + i * 50,
                         k | (since < 100 ? 1 << 8 : 0), k ? LV[(i / 5) % 6] : 0};
            host_sent = 0;
            picture(&S_DUB, Y_DUBPIC, H_DUBPIC, &h);
            dub_keys((uint32_t)h.keys);
            sum += host_sent;
            if (host_sent > worst) worst = host_sent;
        }
        fprintf(rep, "%-8s %6u %10.1f %12llu %12s %12s %12s   %7llu %7llu %7.1f %7.1f\n", "dubtape", scene_bytes, scene_us,
                (unsigned long long)still, "-", "-", "-", (unsigned long long)(sum / 40u), (unsigned long long)worst,
                sum / 40.0 * 77.0 / 57600.0, worst * 77.0 / 57600.0);
        S_DUB.k[0].v = 55; S_DUB.k[3].v = 40;
    }
    begin_screen(); edit_sheet(); shot(dir, "edit_sound");
    begin_screen(); notes_page(0); shot(dir, "notes");
    begin_screen(); notes_page(1); shot(dir, "feel");
    begin_screen(); drum_lanes(); shot(dir, "drum_lanes");
    begin_screen(); mixer(); shot(dir, "mixer");
    begin_screen(); home(&S_SHAPES, HOME_A, -1, &F_REST, 0, 0); notice_card("ENGINE TOO HEAVY", "T2 KEEPS SHAPES"); shot(dir, "notice");
    begin_screen(); home(&S_MACRO, HOME_A, -1, &F_REST, "PACK MISSING", T_REC); shot(dir, "notice_line");
    begin_screen(); shift_map(); shot(dir, "shift_map");
    fprintf(rep, "\nstill: px sent for a frame with nothing changed; anim: the clock 50 ms on; turn: KNOB 2 one detent (its "
                 "dial, the sound line and the picture); note: a note-on (envelope, hit, gate).\n"
                 "2s: two seconds at 20 frames a second, a note every 500 ms, KNOB 2 turned every 100 ms in the second "
                 "second: px sent per frame, mean and worst, and their SPI time (ms).\n"
                 "dubtape: FEEDBACK 1.15, a throw key every 250 ms and KICK every 500 ms, the picture and the keys; its draw "
                 "us is one frame's.\n"
                 "A full screen is 57,600 px, about 77 ms on the 12 MHz SPI (1,000 px about 1.3 ms).\n");
    fprintf(rep, "\nlint: %u findings\n", nfind);
    fclose(rep);
    printf("ui_mockup: screens in %s, %u lint findings (report.txt)\n", dir, nfind);
    return nfind ? 1 : 0;
}
