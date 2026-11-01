/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The new interface, step 1c (docs/orangecat/UI_BUILD.md): HOME's picture page (firmware/src/ui_home_pic.c) through the firmware's UI
 * on tests/ui_test.c's stubs.
 *   cc -O1 -w -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o home_pic_test tests/home_pic_test.c -lm
 *   ./home_pic_test            the checks below
 *   ./home_pic_test hash       (built with OC_HOME_PIC=0 or 1) a hash of HOME's screens with HOME PAGE at TRACKS, for run_tests.sh to
 *                              compare: the switch built in and TRACKS chosen draws release-2's pixels
 * Checked (OC_HOME_PIC=1):
 *   MENU      MENU > HOME PAGE: TRACKS by default, PICTURE in favorites' bit, back to TRACKS
 *   PAGES     PICTURE chosen: HOME lands on the picture; HOME tapped steps PICTURE, TRACKS, CHORD; HOME from another page lands on
 *             the picture; TRACKS chosen: never the picture
 *   KNOBS     on the picture, KNOB 1 .. 4 write the table's ids (a plain knob its parameter, a macro P_MACk within -64 .. 63, far
 *             past both ends); an engine without a table keeps its four EDIT knobs (and the plain picture)
 *   COMMIT    back to TRACKS: every track's macros committed into their targets as a save does, the macros 0
 *   REMOVE    a track removed: the sound kept (the macros committed), the macros 0
 *   DRAWS     the picture drawn for MACRO, SHAPES, DRUM-X; the sound line names a knob while it turns
 *   HIT       a note-on moves the picture; a chord's newer note freed first is not taken for one
 *   COZY      the four knob colors at 3:1 or more against the ground in every palette (and gray in GRAY)
 *   BUDGET    2 s of play at the UI's frame rate (a note every 500 ms, KNOB 2 turned every 100 ms in the second second, the scope
 *             moving), MACRO, SHAPES, DRUM-X and CZ-1 (the plain picture): the pixels
 *             sent a 100 ms frame, mean under 11,000 and worst under 22,000 (UI_DESIGN.md section 3: 15 ms and 30 ms of SPI)
 * Revisions:
 *   (the new interface 0.1, branch ui-build) first version.
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("home_pic: %-100s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static uint32_t screen_hash(void)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < 240u * 240u; i++) h = (h ^ host_screen[i]) * 16777619u;
    return h;
}
/* frames with the audio running (the voices' envelopes for the picture) */
#define SW(c) ((((uint32_t)(c) >> 8) | ((uint32_t)(c) << 8)) & 0xFFFFu)   /* (the stub display keeps the panel's byte order) */
/* a pixel (RGB565) within a few levels of colour ref in every channel: the lines are drawn anti-aliased */
static int like(uint32_t c, uint32_t ref)
{
    int dr = (int)(c >> 11) - (int)(ref >> 11), dg = (int)((c >> 5) & 63u) - (int)((ref >> 5) & 63u), db = (int)(c & 31u) - (int)(ref & 31u);
    return dr >= -4 && dr <= 4 && dg >= -8 && dg <= 8 && db >= -4 && db <= 4;
}
static void play_frames(uint32_t ms)
{
    int32_t out[2 * CTL];
    for (uint32_t i = 0; i < ms / 16u; i++) {
        for (uint32_t b = 0; b < 16u * 44100u / 1000u / CTL; b++) mix_block(out, CTL);
        frame();
    }
}
static void settle(void)
{
    int32_t out[2 * CTL];
    for (uint32_t b = 0; b < 400u; b++) mix_block(out, CTL);
}

/* HOME's screens with HOME PAGE at TRACKS: Stage, a knob turned on it, the chord view, back; on the power-on sound and on MACRO,
 * SHAPES and DRUM-X (the engines with a table: the knobs must stay today's); with the switch built in, after PICTURE was chosen and
 * TRACKS chosen again too */
static uint32_t tracks_hash(void)
{
    static const uint32_t E[4] = {0xFFu, ENGI_MACRO, ENGI_SHAPES, ENGI_DRUMX};
    uint32_t h = 0;
    for (uint32_t pass = 0; pass < 2u; pass++)
        for (uint32_t e = 0; e < 4u; e++) {
            ui_power_on();
            if (E[e] != 0xFFu) set_engine_of(TSEL, E[e]);
            settle();
#if OC_HOME_PIC
            if (pass) { home_pic_set(1); frames(100); home_pic_set(0); }
#endif
            frames(200); h = h * 31u + screen_hash();
            turn(EN_K2, 3); frames(100); h = h * 31u + screen_hash();
            turn(EN_K4, -2); frames(100); h = h * 31u + screen_hash();
            frames(2000); h = h * 31u + screen_hash();
            press(B_HOME); frames(100); h = h * 31u + screen_hash();
            press(B_HOME); frames(100); h = h * 31u + screen_hash();
        }
    return h;
}

#if OC_HOME_PIC
static double contrast(uint16_t a, uint16_t b)
{
    double l[2];
    uint16_t c[2] = {a, b};
    for (int i = 0; i < 2; i++) {
        double r = ((c[i] >> 11) & 31) / 31.0, g = ((c[i] >> 5) & 63) / 63.0, bl = (c[i] & 31) / 31.0, v[3] = {r, g, bl};
        for (int k = 0; k < 3; k++) v[k] = v[k] <= 0.03928 ? v[k] / 12.92 : pow((v[k] + 0.055) / 1.055, 2.4);
        l[i] = 0.2126 * v[0] + 0.7152 * v[1] + 0.0722 * v[2];
    }
    return (fmax(l[0], l[1]) + 0.05) / (fmin(l[0], l[1]) + 0.05);
}
static void pic_on(void)
{
    hold(B_HOME);                                       /* MENU */
    ui.menu_sel = MI_HOME_PAGE;
    if (!home_pic_on()) press(B_OCTUP);
    hold(B_HOME);
    go_home();
    frames(100);
}
#endif

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1 && !strcmp(argv[1], "hash")) {
        printf("%08X\n", tracks_hash());
        return 0;
    }
#if !OC_HOME_PIC
    printf("home_pic: built without OC_HOME_PIC: only the hash mode\n");
    return 0;
#else
    {   /* MENU */
        int ok;
        ui_power_on();
        ok = !home_pic_on() && ui.home_pg == 0u;
        pic_on();
        ok &= home_pic_on() && (favorites.factory[15][24] & 64u) && ui.home && ui.home_pg == 2u;
        {   /* the settings record and back (settings_persist.c), as a power cycle reads it */
            static persist_t rec;
            settings_export(&rec);
            favorites.factory[15][24] &= (uint8_t)~64u;
            settings_import(&rec, (int)sizeof rec);
            ok &= home_pic_on();
        }
        home_pic_set(0);
        ok &= !home_pic_on() && !(favorites.factory[15][24] & 64u) && ui.home_pg == 0u;
        chk("MENU    HOME PAGE: TRACKS by default; PICTURE in favorites' row 15 byte 24 bit 6, kept by the settings record; TRACKS", ok);
    }
    {   /* PAGES */
        int ok = 1;
        ui_power_on();
        pic_on();
        ok &= ui.home_pg == 2u;
        press(B_HOME); ok &= ui.home && ui.home_pg == 0u;
        press(B_HOME); ok &= ui.home_pg == 1u;
        press(B_HOME); ok &= ui.home_pg == 2u;
        press(B_SEQ); ok &= !ui.home;
        press(B_HOME); ok &= ui.home && ui.home_pg == 2u;
        home_pic_set(0);
        for (int i = 0; i < 4; i++) { press(B_HOME); ok &= ui.home_pg < 2u; }
        chk("PAGES   PICTURE lands on the picture; HOME steps PICTURE TRACKS CHORD; from SEQ back to the picture; TRACKS: never it", ok);
    }
    {   /* KNOBS */
        int ok = 1;
        static const uint32_t ENG[3] = {ENGI_MACRO, ENGI_SHAPES, ENGI_DRUMX};
        static const uint32_t K[4] = {EN_K1, EN_K2, EN_K3, EN_K4};
        for (uint32_t e = 0; e < 3u; e++) {
            const knob_def_t *kd = kdef_of(ENG[e]);
            ui_power_on();
            set_engine_of(TSEL, ENG[e]);
            settle();
            pic_on();
            for (uint32_t k = 0; k < 4u; k++) {
                uint32_t id = kd[k].kind == KD_PLAIN ? kd[k].id : P_MAC0 + k;
                int16_t before[P_COUNT];
                memcpy(before, TSEL->p, sizeof before);
                turn(K[k], 1);
                int moved = TSEL->p[id] != before[id];
                for (uint32_t i = 0; i < P_COUNT; i++) if (i != id) moved &= TSEL->p[i] == before[i];
                for (int i = 0; i < 40; i++) turn(K[k], 8);
                int top = TSEL->p[id];
                for (int i = 0; i < 80; i++) turn(K[k], -8);
                int bottom = TSEL->p[id];
                if (kd[k].kind == KD_MACRO) moved &= top == 63 && bottom == -64;
                if (!moved) printf("home_pic:   %s knob %u: id %u (%d .. %d)\n", ENGINES[ENG[e]]->name, (unsigned)k + 1u, (unsigned)id,
                                   bottom, top);
                ok &= moved;
            }
        }
        chk("KNOBS   MACRO, SHAPES, DRUM-X: each knob writes its table's id alone; a macro P_MACk, held to -64 .. 63", ok);
        ui_power_on();
        set_engine_of(TSEL, ENGI_CZ);
        settle();
        pic_on();
        {
            uint8_t pic[4], tracks[4];
            int same;
            memcpy(pic, stage_page()->id, 4);
            GLO_SELECT &= (uint8_t)~HOME_PIC_BIT;
            memcpy(tracks, stage_page()->id, 4);
            GLO_SELECT |= HOME_PIC_BIT;
            same = !memcmp(pic, tracks, 4) && !kdef_of(ENGI_CZ);
            chk("KNOBS   an engine without a table (CZ-1) keeps today's four knobs on the picture page", same);
        }
    }
    {   /* COMMIT */
        int16_t want[P_COUNT];
        ui_power_on();
        set_engine_of(&trk[0], ENGI_SHAPES);
        settle();
        pic_on();
        trk[0].p[P_MAC0] = 30; trk[0].p[P_MAC0 + 3] = -44;
        for (uint32_t i = 0; i < P_COUNT; i++) want[i] = mac_saved_value(&trk[0], i);
        home_pic_set(0);
        chk("COMMIT  back to TRACKS: the macros committed as a save would, set to 0",
            !memcmp(want, trk[0].p, sizeof want) && !trk[0].p[P_MAC0] && !trk[0].p[P_MAC0 + 3]);
    }
    {   /* REMOVE */
        int16_t want[P_COUNT];
        int ok;
        ui_power_on();
        song.ntrk = 3;
        set_engine_of(&trk[1], ENGI_MACRO);
        settle();
        trk[1].p[P_MAC0 + 3] = 40;
        for (uint32_t i = 0; i < P_COUNT; i++) want[i] = mac_saved_value(&trk[1], i);
        track_remove(0);                                /* (track 2 becomes track 1) */
        settle();
        ok = trk[0].eng_req == ENGI_MACRO && !trk[0].p[P_MAC0 + 3];
        for (uint32_t i = 0; i < P_E0 + 8u; i++)
            if (i >= P_E0 || (i >= P_ATK && i <= P_REL)) ok &= trk[0].p[i] == want[i];
        chk("REMOVE  a track removed: the sound kept with its macros committed, the macros at 0", ok);
    }
    {   /* DRAWS, the sound line */
        int ok = 1;
        static const uint32_t ENG[3] = {ENGI_MACRO, ENGI_SHAPES, ENGI_DRUMX};
        for (uint32_t e = 0; e < 3u; e++) {
            uint32_t len, blank;
            ui_power_on();
            set_engine_of(TSEL, ENG[e]);
            settle();
            pic_on();
            ok &= scene_of(ENG[e], &len) != 0 && hp.scene && hp.scene == scene_of(ENG[e], &len);   /* (the scene path drew) */
            blank = 0;
            for (uint32_t y = HP_PIC_Y; y < 240u; y++)
                for (uint32_t x = 0; x < 240u; x++) blank += host_screen[y * 240u + x] == host_screen[HP_PIC_Y * 240u];
            ok &= blank < 240u * HP_PIC_H - 500u;      /* (the picture has lines in it) */
        }
        {   /* the sound line: the knob's name in its color, drawn */
            uint32_t n = 0, x, y;
            uint16_t c = T_COZY(2u), sw = (uint16_t)((c >> 8) | (c << 8));
            turn(EN_K3, 2);
            frames(50);
            for (y = HP_SOUND_Y; y < HP_SOUND_Y + HP_SOUND_H; y++)
                for (x = 30; x < 120u; x++) n += host_screen[y * 240u + x] == sw || host_screen[y * 240u + x] == c;
            ok &= stage_knobs_out() && ui.hot_col == 2u && n > 20u;
        }
        chk("DRAWS   MACRO, SHAPES and DRUM-X draw their scenes; a knob turned shows in the sound line in its color", ok);
    }
    {   /* HIT: a note-on moves the picture; a chord's newer note let go first is not a note-on */
        int ok = 1;
        static uint16_t before[240 * 240];
        uint32_t d = 0, hit0;
        ui_power_on();
        set_engine_of(TSEL, ENGI_SHAPES);
        settle();
        pic_on();
        play_frames(300);
        memcpy(before, host_screen, sizeof before);
        trk_note_on(TSEL, 48, 110);
        play_frames(128);                               /* (past the picture's 100 ms gate) */
        for (uint32_t i = HP_PIC_Y * 240u; i < 240u * 240u; i++) d += host_screen[i] != before[i];
        ok &= d > 50u && fm1_ms - hp.hit_ms < 200u;
        trk_note_on(TSEL, 55, 110);                     /* a second note, then the newer one let go and freed */
        play_frames(48);
        hit0 = hp.hit_ms;
        trk_note_off(TSEL, 55);
        play_frames(1500);
        ok &= hp.hit_ms == hit0;
        trk_note_off(TSEL, 48);
        printf("home_pic: HIT     %u picture pixels moved by a note-on\n", (unsigned)d);
        chk("HIT     a note-on moves the picture; the newer of two notes freed first is not a note-on", ok);
    }
    {   /* MOVES (ui-fix 3, the owner's finding "the cat's green hand barely moves"; ui-fix2 6, the owner's finding on f5057c): on SHAPES each knob's two
         * ends draw clearly different pictures, and the motions are the knobs' RATES: COLR the tail's wag, PLUCK the paw's bat at the yarn (the
         * paw striking it, visibly), TIMB the yarn's size alone */
        static uint16_t a[240 * 240], b[240 * 240];
        static const uint32_t BAR[4] = {300u, 0u, 600u, 0u};   /* the yarn's size (K1) and the moon (K3): by position; K2 and K4 are rates */
        int ok = 1;
        uint32_t k, moved;
        ui_power_on();
        set_engine_of(TSEL, ENGI_SHAPES);
        settle();
        pic_on();
        play_frames(300);
        for (k = 0; k < 4u; k++) {                      /* the positions: K1's size, K3's moon differ between ends */
            int16_t *vp;
            const param_desc_t *d = page_desc(stage_page(), k, &vp);
            uint32_t i;
            if (!d || !vp) { ok = 0; continue; }
            *vp = d->min;
            play_frames(240);
            memcpy(a, host_screen, sizeof a);
            *vp = d->max;
            play_frames(240);
            for (moved = 0, i = HP_PIC_Y * 240u; i < 240u * 240u; i++) moved += host_screen[i] != a[i];
            printf("home_pic: MOVES   SHAPES knob %u: %u picture pixels differ between its ends\n", (unsigned)k + 1u, (unsigned)moved);
            ok &= moved >= BAR[k];
            *vp = d->def;
        }
        chk("MOVES   SHAPES: TIMB's ends draw clearly different yarn (size), TONE's the moon", ok);
        ok = 1;
        for (k = 0; k < 4u; k += 1u) {                  /* RATES: the turns a phase makes in 2 s at each end of its knob (0.5 Hz, 3 Hz) */
            static const uint32_t WHO[2] = {1u, 3u};    /* COLR (the tail), PLUCK (the paw) */
            uint32_t w, e, turns[2];
            if (k > 1u) break;
            w = WHO[k];
            for (e = 0; e < 2u; e++) {
                int16_t *vp;
                const param_desc_t *d = page_desc(stage_page(), w, &vp);
                uint32_t i, sum = 0;
                uint16_t last;
                if (!d || !vp) { ok = 0; turns[e] = 0; continue; }
                *vp = e ? d->max : d->min;
                play_frames(100);
                last = hp.ph[w];
                for (i = 0; i < 125u; i++) { play_frames(16); sum += (uint16_t)(hp.ph[w] - last); last = hp.ph[w]; }   /* 2 s */
                turns[e] = (uint32_t)((uint64_t)sum * 10u / 65536u);   /* tenths of a turn */
                *vp = d->def;
            }
            printf("home_pic: MOVES   %s in 2 s: %u.%u turns at its knob's left end, %u.%u at its right\n", w == 1u ? "the tail's wag (COLR)" : "the paw's bat (PLUCK)",
                   (unsigned)turns[0] / 10u, (unsigned)turns[0] % 10u, (unsigned)turns[1] / 10u, (unsigned)turns[1] % 10u);
            ok &= turns[0] >= 9u && turns[0] <= 11u && turns[1] >= 55u && turns[1] <= 62u;   /* 1 turn and 6: 0.5 Hz and 3 Hz */
        }
        chk("MOVES   COLR sets the tail's wag rate and PLUCK the paw's bat rate: 0.5 Hz at the left end, 3 Hz at the right", ok);
        {   /* the pictures at exact phases (the clock still: nothing advances between the sets and the draw): the paw strikes the yarn at the
             * wrap (its ball on the yarn) and rises away from it; TIMB's phase draws nothing (the yarn no longer rolls at its rate); COLR's phase
             * sweeps the tail */
            int ok2 = 1;
            uint32_t pose, touch[2] = {0, 0}, i;
            palette_set(0);                             /* (a palette with hues: the paw's and the yarn's colours apart) */
            go_home(); frames(100);
            ui.force = 1; ui_draw();
            for (pose = 0; pose < 2u; pose++) {
                uint32_t n = 0;
                hp.ph[0] = 0; hp.ph[1] = 0; hp.ph[2] = 0; hp.ph[3] = pose ? 0x8000u : 0x0100u;
                hp.ph_ms = fm1_ms; ui.force = 1; ui_draw();
                for (i = HP_PIC_Y * 240u; i < 240u * 240u; i++) {
                    uint32_t x = i % 240u, y = i / 240u, c = SW(host_screen[i]), dx, dy;
                    int near = 0;
                    if (!like(c, T_COZY(3u))) continue;                             /* the paw's colour */
                    for (dy = 0; dy < 5u && !near; dy++)
                        for (dx = 0; dx < 5u && !near; dx++) {
                            uint32_t xx = x + dx - 2u, yy = y + dy - 2u, o;
                            if (xx >= 240u || yy >= 240u) continue;
                            o = SW(host_screen[yy * 240u + xx]);
                            near = like(o, T_COZY(0u));                                     /* the yarn's */
                        }
                    n += (uint32_t)near;
                }
                touch[pose] = n;
            }
            printf("home_pic: MOVES   paw pixels on the yarn's edge: %u at the strike, %u risen\n", (unsigned)touch[0], (unsigned)touch[1]);
            chk("MOVES   the paw strikes the yarn (its ball on the yarn at the strike) and rises away from it", touch[0] >= 3u && touch[1] == 0u);
            hp.ph[0] = 0x1000u; hp.ph[1] = 0; hp.ph[2] = 0; hp.ph[3] = 0x4000u;
            hp.ph_ms = fm1_ms; ui.force = 1; ui_draw(); memcpy(a, host_screen, sizeof a);
            hp.ph[0] = 0xB000u; hp.ph[1] = 0; hp.ph[2] = 0; hp.ph[3] = 0x4000u;
            hp.ph_ms = fm1_ms; ui.force = 1; ui_draw();
            for (moved = 0, i = HP_PIC_Y * 240u; i < 240u * 240u; i++) moved += host_screen[i] != a[i];
            printf("home_pic: MOVES   TIMB's phase moves %u pixels (the yarn no longer rolls at its rate)\n", (unsigned)moved);
            ok2 &= moved == 0u;
            hp.ph[0] = 0; hp.ph[1] = 0x2000u; hp.ph[2] = 0; hp.ph[3] = 0x4000u;
            hp.ph_ms = fm1_ms; ui.force = 1; ui_draw(); memcpy(a, host_screen, sizeof a);
            hp.ph[0] = 0; hp.ph[1] = 0xA000u; hp.ph[2] = 0; hp.ph[3] = 0x4000u;
            hp.ph_ms = fm1_ms; ui.force = 1; ui_draw(); memcpy(b, host_screen, sizeof b);
            for (moved = 0, i = HP_PIC_Y * 240u; i < 240u * 240u; i++) moved += a[i] != b[i];
            printf("home_pic: MOVES   the tail between two phases of COLR's wag: %u pixels\n", (unsigned)moved);
            ok2 &= moved >= 60u;
            chk("MOVES   TIMB's phase draws nothing (no yarn roll at its rate); the tail sweeps with COLR's phase", ok2);
        }
    }
    {   /* COZY */
        int ok = 1;
        for (uint32_t p = 0; p < UI_PAL_ENTRIES; p++) {
            palette_set(p);
            for (uint32_t k = 0; k < 4u; k++) {
                double c = contrast(T_COZY(k), T_BG);
                if (ux.mono) ok &= (T_COZY(k) >> 11) * 2u == ((T_COZY(k) >> 5) & 63u) && (T_COZY(k) >> 11) == (T_COZY(k) & 31u);
                if (c < 3.0) { printf("home_pic:   palette %u knob %u: %.2f\n", (unsigned)p, (unsigned)k + 1u, c); ok = 0; }
            }
        }
        palette_set(UI_GRAY_INDEX);
        chk("COZY    the knob colors at 3:1 or more on the ground in every palette; gray in GRAY", ok);
    }
    {   /* BUDGET */
        static const uint32_t ENG[4] = {ENGI_MACRO, ENGI_SHAPES, ENGI_DRUMX, ENGI_CZ};   /* (CZ-1: the plain picture) */
        int ok = 1;
        for (uint32_t e = 0; e < 4u; e++) {
            uint64_t sum = 0, worst = 0, last;
            uint32_t n = 0;
            ui_power_on();
            palette_set(0);
            set_engine_of(TSEL, ENG[e]);
            settle();
            pic_on();
            frames(500);
            last = host_sent;
            for (uint32_t t = 0; t < 2000u; t += 100u) {
                if (t % 500u == 0u) trk_note_on(TSEL, ENG[e] == ENGI_DRUMX ? 36 : 48 + t / 500u, 100);
                if (t % 500u == 300u) trk_note_off(TSEL, ENG[e] == ENGI_DRUMX ? 36 : 48 + t / 500u);
                if (t >= 1000u) host_enc[panel.enc[EN_K2]] += 2 * panel.dir[EN_K2];
                for (uint32_t i = 0; i < SCOPE_N; i++)      /* (the output's scope moving, as the unit's: a saw drifting) */
                    scope_buf[i] = (int16_t)((int32_t)((i * 37u + t * 11u) % 128u) * 200 - 12800);
                scope_w = (scope_w + 97u) & (SCOPE_N - 1u);
                play_frames(100);
                uint64_t px = host_sent - last;
                last = host_sent;
                sum += px; n++;
                if (px > worst) worst = px;
            }
            printf("home_pic: BUDGET  %-7s (about 9 frames a second: the gate at 100 ms on 16 ms UI frames) mean %6.0f px a 100 ms frame (%4.1f ms SPI), worst %6u px (%4.1f ms)\n", ENGINES[ENG[e]]->name,
                   (double)sum / n, (double)sum / n * 77.0 / 57600.0, (unsigned)worst, worst * 77.0 / 57600.0);
            ok &= sum / n < 11000u && worst < 22000u;
        }
        palette_set(UI_GRAY_INDEX);
        chk("BUDGET  2 s of play: the pixels sent a 100 ms frame, mean under 11,000 and worst under 22,000, each engine", ok);
    }
    printf("home_pic: %d failed\n", nbad);
    return nbad != 0;
#endif
}
