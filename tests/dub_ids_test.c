/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Release 3 format revision, Part 8 (docs/orangecat/UI_BUILD.md "Part 8"), OC_DUB_IDS=1 (needs OC_R3_FORMAT=1 and OC_LFO2=1): DUB TAPE's setup values, each on an id
 * of its own (docs/orangecat/DUB_R12.md sections 1 .. 7, whose packing into ids 45 .. 47 could not be saved).
 *   cc -O1 -w <the build set> -DOC_R3_FORMAT=1 -DOC_LFO2=1 -DOC_DUB_IDS=1 -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests tests/dub_ids_test.c -o dub_ids_test -lm
 * Checked:
 *   IDS      the eight ids, their order before P_E0, P_COUNT 139 and P_E0 131; ranges and defaults (DUB_R12.md 6); not a sound's (param_dub_setup), never a motion lane,
 *            not an effect id; the engine ids moved up by eight and nothing else
 *   DERIVE   a store without them (Release 2: np 124, LFO 2 alone: np 131) gets TIME L, TIME R, SPRING from LEN .. SPACE as DUB_R12.md 7 says: every PTRN and SPACE
 *            against an independent reference in floating point (ties to the shorter time), SYNC on, PING off, DRIVE 25, WOW 0, SPEED 32; the old ids untouched; a
 *            store that has them (np 139) keeps what it holds
 *   SOUNDS   a user preset stores np 131 (the eight not stored: the layout an LFO 2 build writes), reads back by count with the eight at their defaults whatever
 *            the track held, and the engine values survive; a DRUM-X kit still fits the record
 *   TRACK    a sound load leaves the eight alone (the track's own, as the effects are)
 *   PROJECT  a project saved and loaded keeps the eight on every track
 * Revisions:
 *   Part 8 (November 2, 2026): first version. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void chk(const char *what, int ok)
{
    printf("dub_ids: %-110s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}

/* the reference: floating point, written from DUB_R12.md 7 and not from core.h's integers */
static int ref_nearest(double beats)
{
    static const double row[6] = {0.125, 0.25, 0.5, 1, 2, 4};
    static const double kind[3] = {1.0, 1.5, 2.0 / 3.0};
    int best = 0, k;
    double bd = 1e9;
    for (k = 0; k < 18; k++) {
        double v = row[k / 3] * kind[k % 3], d = v > beats ? v - beats : beats - v;
        if (d < bd - 1e-9 || (d < bd + 1e-9 && v < row[best / 3] * kind[best % 3] - 1e-9)) { bd = d; best = k; }
    }
    return best;
}

int main(void)
{
#if !OC_DUB_IDS
    printf("dub_ids: built without OC_DUB_IDS\n");
    return 0;
#else
    static const double div[14] = {0.125, 1.0 / 6, 0.25, 1.0 / 3, 0.375, 0.5, 2.0 / 3, 0.75, 1.0, 4.0 / 3, 1.5, 2.0, 3.0, 4.0};
    static const double h1[6] = {1, 1, 1, 1, 0.75, 1}, h2[6] = {3, 1, 1.5, 2, 1, 2};   /* SLAP SINGLE SKANK TRIPLE GALLOP SPREAD: the first two heads */
    uint32_t ptrn, space, sprp, i;
    setvbuf(stdout, NULL, _IONBF, 0);
    {   /* IDS */
        const param_desc_t *d;
        chk("IDS     the eight follow LFO 2's seven and come before P_E0, in the order of DUB_R12.md's pages",
            P_DUBSYNC == P_L2AMT + 1 && P_DUBTL == P_DUBSYNC + 1 && P_DUBTR == P_DUBSYNC + 2 && P_DUBPING == P_DUBSYNC + 3 && P_DUBSPR == P_DUBSYNC + 4 &&
            P_DUBDRV == P_DUBSYNC + 5 && P_DUBWOW == P_DUBSYNC + 6 && P_DUBSPD == P_DUBSYNC + 7 && P_DUBSPD + 1 == P_E0);
        chk("IDS     P_COUNT 139, P_E0 131 (LFO 2 alone is 131 and 123)", P_COUNT == 139 && P_E0 == 131 && P_DUBSYNC == 123);
        d = param_desc_of(0, P_DUBSYNC);
        chk("IDS     SYNC FREE/SYNC default SYNC", d->max == 1 && d->def == 1 && !strcmp(d->label, "SYNC"));
        d = param_desc_of(0, P_DUBTL);
        chk("IDS     TIME L 0 .. 127 default 7 (1/8 dotted)", d->min == 0 && d->max == 127 && d->def == 7 && !strcmp(d->label, "TIME L"));
        d = param_desc_of(0, P_DUBTR);
        chk("IDS     TIME R 0 .. 127 default 9 (1/4)", d->min == 0 && d->max == 127 && d->def == 9 && !strcmp(d->label, "TIME R"));
        d = param_desc_of(0, P_DUBPING);
        chk("IDS     PING OFF/ON default OFF", d->max == 1 && d->def == 0);
        d = param_desc_of(0, P_DUBSPR);
        chk("IDS     SPRING 0 .. 63 default 0", d->min == 0 && d->max == 63 && d->def == 0);
        d = param_desc_of(0, P_DUBDRV);
        chk("IDS     DRIVE 0 .. 63 default 25", d->min == 0 && d->max == 63 && d->def == 25);
        d = param_desc_of(0, P_DUBWOW);
        chk("IDS     WOW 0 .. 63 default 0", d->min == 0 && d->max == 63 && d->def == 0);
        d = param_desc_of(0, P_DUBSPD);
        chk("IDS     SPEED 0 .. 63 default 32 (normal)", d->min == 0 && d->max == 63 && d->def == 32);
        for (i = P_DUBSYNC; i <= P_DUBSPD; i++)
            if (!param_dub_setup(i) || param_jfx(i) || motion_param(i) || param_reserved(i)) break;
        chk("IDS     each is a setup value: not a sound's, not an effect id, never a motion lane, not reserved", i > P_DUBSPD);
        chk("IDS     the old setup ids are setup values still", param_dub_setup(P_RESERVED45) && param_dub_setup(P_RESERVED47) && !param_dub_setup(P_RESERVED48) && !param_dub_setup(P_L2AMT));
    }
    {   /* DERIVE */
        int16_t in[139], out[139], def[139];
        int ok = 1, ok2 = 1, ok3 = 1, n = 0;
        for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(0, i)->def;
        for (ptrn = 0; ptrn < 6u; ptrn++)
            for (space = 0; space < 128u; space++)
                for (sprp = 0; sprp < 3u; sprp++) {
                    double b = div[(space < 64 ? space : 63) * 14 / 64];
                    int tl = ref_nearest(b * h1[ptrn]), tr = ref_nearest(b * h2[ptrn]);
                    int sp = sprp && space >= 64 ? (int)space - 64 : 0;
                    memset(in, 0, sizeof in);
                    for (i = 0; i < 116u; i++) in[i] = def[i];
                    in[P_JRES3] = (int16_t)space; in[P_RESERVED45] = 3; in[P_RESERVED46] = (int16_t)ptrn; in[P_RESERVED47] = (int16_t)sprp;
                    for (i = 0; i < 8u; i++) in[116 + i] = (int16_t)(40 + i);                /* np 124's engine block */
                    params_by_count(out, in, 124u, def);
                    n++;
                    if (out[P_DUBTL] != tl || out[P_DUBTR] != tr) ok = 0;
                    if (out[P_DUBSPR] != sp || out[P_DUBSYNC] != 1 || out[P_DUBPING] != 0 || out[P_DUBDRV] != 25 || out[P_DUBWOW] != 0 || out[P_DUBSPD] != 32) ok2 = 0;
                    if (out[P_JRES3] != (int16_t)space || out[P_RESERVED45] != 3 || out[P_RESERVED46] != (int16_t)ptrn || out[P_RESERVED47] != (int16_t)sprp) ok3 = 0;
                    for (i = 0; i < 8u; i++) if (out[P_E0 + i] != 40 + (int)i) ok3 = 0;
                }
        chk("DERIVE  every PTRN x SPACE x SPRP (2,304) of a Release 2 store: TIME L and TIME R are the nearest synced times to the first and second head", ok && n == 2304);
        chk("DERIVE  SPRING is the spring's mix (SPACE - 64 when the spring is on), SYNC on, PING off, DRIVE 25, WOW 0, SPEED 32", ok2);
        chk("DERIVE  the old ids and the engine block are where they were", ok3);
        /* spot values worked by hand: SPACE 24 is the 1/8 division (0.5 beat) */
        memset(in, 0, sizeof in);
        for (i = 0; i < 116u; i++) in[i] = def[i];
        in[P_JRES3] = 24; in[P_RESERVED46] = 0;                                              /* SLAP: 0.5 and 1.5 beats = 1/8 (6) and 1/4 dotted (10) */
        params_by_count(out, in, 124u, def);
        chk("DERIVE  SLAP at SPACE 24: TIME L 1/8 (6), TIME R 1/4 dotted (10)", out[P_DUBTL] == 6 && out[P_DUBTR] == 10);
        in[P_RESERVED46] = 1;                                                                /* SINGLE: one head, both times the same */
        params_by_count(out, in, 124u, def);
        chk("DERIVE  SINGLE: TIME R equals TIME L", out[P_DUBTL] == 6 && out[P_DUBTR] == 6);
        in[P_RESERVED46] = 4;                                                                /* GALLOP: 0.375 (1/16 dotted, 4) and 0.5 (6) */
        params_by_count(out, in, 124u, def);
        chk("DERIVE  GALLOP at SPACE 24: 1/16 dotted (4) and 1/8 (6)", out[P_DUBTL] == 4 && out[P_DUBTR] == 6);
        /* LFO 2 alone (np 131) is a store without them as well; np 139 keeps its own */
        memset(in, 0, sizeof in);
        for (i = 0; i < 123u; i++) in[i] = def[i];
        in[P_JRES3] = 24; in[P_RESERVED46] = 2; in[P_RESERVED47] = 1;
        for (i = 0; i < 8u; i++) in[123 + i] = 40 + i;
        params_by_count(out, in, 131u, def);
        chk("DERIVE  an LFO 2 store (np 131) derives too, its engine block moved up", out[P_DUBTL] == 6 && out[P_DUBTR] == 7 && out[P_E0] == 40 && out[P_E7] == 47);
        memset(in, 0, sizeof in);
        for (i = 0; i < 131u; i++) in[i] = def[i];
        in[P_DUBTL] = 11; in[P_DUBTR] = 2; in[P_DUBSPD] = 50; in[P_DUBSYNC] = 0; in[P_JRES3] = 24; in[P_RESERVED46] = 2;
        for (i = 0; i < 8u; i++) in[131 + i] = 40 + i;
        params_by_count(out, in, 139u, def);
        chk("DERIVE  a store of np 139 keeps its eight (nothing derived over them)", out[P_DUBTL] == 11 && out[P_DUBTR] == 2 && out[P_DUBSPD] == 50 && out[P_DUBSYNC] == 0 && out[P_E7] == 47);
    }
    {   /* SOUNDS */
        up_rec_t r;
        int16_t def[P_COUNT], v[P_COUNT], back[P_COUNT];
        ui_power_on();
        for (i = 0; i < P_COUNT; i++) def[i] = param_desc_of(3, i)->def;
        memset(&r, 0, sizeof r); r.used = UP_USED; r.ver = UP_VER; r.engine = 3; r.np = UP_NP;
        for (i = 0; i < P_COUNT; i++) v[i] = def[i];
        v[P_E1] = 21; v[P_E7] = 5; v[P_DUBTL] = 99; v[P_DUBSPD] = 3; v[P_L2AMT] = 17;
        for (i = 0; i < P_COUNT; i++) up_set_value(&r, i, v[i]);
        chk("SOUNDS  a record stores np 131: the layout an LFO 2 build writes", r.np == 131u && UP_NP == 131u);
        up_params(&r, back, def);
        chk("SOUNDS  it reads back by count: the engine values and LFO 2 as written", back[P_E1] == 21 && back[P_E7] == 5 && back[P_L2AMT] == 17 && up_value(&r, P_E1) == 21);
        chk("SOUNDS  the eight are not in the record: they read as the Release 2 derivation of its old ids (a sound never carries them), never the track's 99 and 3",
            back[P_DUBTL] != 99 && back[P_DUBSPD] == 32);
        chk("SOUNDS  a DRUM-X kit still fits the record beside np 131", DXK_ROOM - 46u <= UP_PMAX * 2u - 131u);
    }
    {   /* TRACK, PROJECT */
        int ok = 1;
        ui_power_on();
        stop_transport();
        for (i = 0; i < NTRK; i++) {
            trk[i].p[P_DUBSYNC] = (int16_t)(i & 1u); trk[i].p[P_DUBTL] = (int16_t)(10 + i); trk[i].p[P_DUBTR] = (int16_t)(20 + i); trk[i].p[P_DUBPING] = (int16_t)(1 - (i & 1u));
            trk[i].p[P_DUBSPR] = (int16_t)(5 + i); trk[i].p[P_DUBDRV] = (int16_t)(30 + i); trk[i].p[P_DUBWOW] = (int16_t)(40 + i); trk[i].p[P_DUBSPD] = (int16_t)(50 + i);
        }
        sound_init_of(&trk[0], trk[0].eng_req);
        ok = trk[0].p[P_DUBTL] == 10 && trk[0].p[P_DUBSPD] == 50 && trk[0].p[P_DUBPING] == 1;
        chk("TRACK   TOOLS INIT (a sound load) leaves the eight alone: the track's own", ok);
        project_save(1);
        for (i = 0; i < NTRK; i++) {
            trk[i].p[P_DUBTL] = 0; trk[i].p[P_DUBTR] = 0; trk[i].p[P_DUBSPD] = 0; trk[i].p[P_DUBSPR] = 63; trk[i].p[P_DUBSYNC] = 1;
        }
        project_load(1);
        ok = 1;
        for (i = 0; i < NTRK; i++)
            ok &= trk[i].p[P_DUBSYNC] == (int16_t)(i & 1u) && trk[i].p[P_DUBTL] == (int16_t)(10 + i) && trk[i].p[P_DUBTR] == (int16_t)(20 + i) &&
                  trk[i].p[P_DUBPING] == (int16_t)(1 - (i & 1u)) && trk[i].p[P_DUBSPR] == (int16_t)(5 + i) && trk[i].p[P_DUBDRV] == (int16_t)(30 + i) &&
                  trk[i].p[P_DUBWOW] == (int16_t)(40 + i) && trk[i].p[P_DUBSPD] == (int16_t)(50 + i);
        chk("PROJECT a project saved and loaded keeps the eight on all four tracks", ok);
    }
    printf("dub_ids: %s\n", nbad ? "FAILED" : "all passed");
    return nbad != 0;
#endif
}
