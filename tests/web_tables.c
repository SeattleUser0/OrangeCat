/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the web app's and the controls guide's view of the firmware's tables, generated from the firmware's own C
 * (Session D, branch web-r3).
 *
 *   build/host/web_tables > build/host/web/fw_tables.json
 *
 * What the web app and docs/orangecat/CONTROLS.md must follow, in one JSON: the parameter tables (TP, GP: id, label, format, range,
 * default, enum names), the page table (params.c PAGES, in order, with the build's #if resolved), the families, HOME's four knobs of
 * every built-in engine (oc_knobs.c), the engines and their EDIT descriptors, the system menu's items (ui_menu.c MI_NAME), the
 * shortcut layers' headers and key hints (ui_layer.c LAYERS, FX_CELL, LAUNCH_NAME), DUB's performance keys (dub_keys.h).
 * It is compiled with the Nightly's switches (the OC_ list of tools/orangecat_engines.sh, as every UI job of tests/run_tests.sh is),
 * so a switch the Nightly turns on shows its pages and values here.
 *
 * web/app/tools/gen_fw_tables.mjs turns the JSON into web/app/lib/fw_tables.js (checked in); web/app/test/test_tables.mjs fails when
 * the checked-in file is not what the firmware writes now, and when the app or the guide does not cover a page, a parameter or a
 * menu item of it. Add a family, a page, a parameter, a menu item or a layer to the firmware and the suite fails until the app and
 * the guide follow.
 *
 * Revisions:
 *   web-r3: first version (Session D).
 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "web_wire.h"

/* the firmware's enums by name: a new family or scope fails the build here until the names follow */
static const char *const FAM_NAME[] = {"HOME", "ENV", "LFO", "SCL", "EDIT", "GLO", "SAVE", "ARP", "SEQ", "TRK", "FX"};
_Static_assert(sizeof FAM_NAME / sizeof FAM_NAME[0] == FAM_COUNT, "web_tables.c: a page family was added: name it here (and in the web app's guide)");
static const char *const SCOPE_NAME[] = {"TRACK", "GLOBAL", "ENGINE", "STEP", "TRK", "FM6", "FMOP", "CZ", "CZ1", "P5", "P5STORE", "DRUM", "DRUMHIT",
                                         "MOD", "DXLANE", "FX"};
_Static_assert(sizeof SCOPE_NAME / sizeof SCOPE_NAME[0] == SC_FX + 1, "web_tables.c: a page scope was added: name it here");
static const char *const GRAPH_NAME[] = {"NONE", "ADSR", "LFO", "STEPS", "ARP", "SCALE", "ROLL", "BROWSE", "SLOTS", "USER", "TRK", "MOD", "PATS", "SONG",
                                         "MOTION", "CHORD", "SLICES", "FMEG", "FMPEG", "FMSTORE", "CZTOOLS", "SCALE_PICKER", "DRUMHIT", "PATGRID",
                                         "FXRACK", "TRACKER", "POOL", "GROOVE"};
/* (a page graph added after GR_GROOVE by a switch is named by number: GR_FEEL, GR_MGRID, GR_LANES) */

static void js(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') putchar('\\');
        if ((unsigned char)*s < 32u) { printf("\\u%04x", (unsigned)*s); continue; }
        putchar(*s);
    }
    putchar('"');
}
static void desc(const param_desc_t *d)
{
    printf("{\"label\":"); js(d->label);
    printf(",\"fmt\":%d,\"min\":%d,\"max\":%d,\"def\":%d,\"unit\":", d->fmt, d->min, d->max, d->def);
    js(d->unit ? d->unit : "");
    printf(",\"names\":");
    if (d->fmt == F_ENUM && d->names) {
        putchar('[');
        for (int32_t i = 0; i <= d->max - d->min; i++) { if (i) putchar(','); js(d->names[i + d->min]); }
        putchar(']');
    } else printf("null");
    putchar('}');
}

/* (web-r3, the display) every distinct format the pages use, with the firmware's own text for each value of its range
 * (param_format: what the unit's column shows, "value unit"), and each TRACK / GLOBAL page's four cells as page_desc gives them:
 * the class of the cell's format, its label, and the id the editor link reads and writes it by. The web app shows these strings
 * and never formats a value itself, so a changed format, range or name reaches the app through the generated file. */
static const param_desc_t *DCLS[400];
static uint32_t ndcls;
static int dclass(const param_desc_t *d)
{
    uint32_t i;
    for (i = 0; i < ndcls; i++) {
        const param_desc_t *c = DCLS[i];
        if (c == d || (c->fmt == d->fmt && c->min == d->min && c->max == d->max && c->names == d->names && c->unit == d->unit &&
                       c != &GP[G_SLOT] && d != &GP[G_SLOT]))
            return (int)i;
    }
    if (ndcls == NELEM(DCLS)) { fprintf(stderr, "web_tables.c: more than %u display classes\n", (unsigned)NELEM(DCLS)); exit(1); }
    DCLS[ndcls] = d;
    return (int)ndcls++;
}
static void cell_row(const page_t *pg)
{
    putchar('[');
    for (uint32_t k = 0; k < 4u; k++) {
        int16_t *vp = 0;
        const param_desc_t *d = pg->id[k] == 0xFFu ? 0 : page_desc(pg, k, &vp);
        if (k) putchar(',');
        if (!d) { printf("null"); continue; }
        printf("{\"c\":%d,\"label\":", dclass(d)); js(d->label);
        /* how the editor link reaches the value the unit's page shows (tests/web_wire.h; editor_test.c holds it equal to ed_val's) */
        printf(",\"id\":%d,\"wire\":\"%s\"}", pg->id[k], web_wire(pg, k, vp));
    }
    putchar(']');
}
static void display(void)
{
    uint32_t i;
    int32_t v;
    /* the cells with DUB's SYNC off (a fresh track), then with it on (TIME L / R read on the beat) */
    printf("\"cells\":[");
    for (i = 0; i < NPAGES; i++) {
        printf("%s\n ", i ? "," : "");
        if (PAGES[i].scope == SC_TRACK || PAGES[i].scope == SC_GLOBAL) cell_row(&PAGES[i]); else printf("null");
    }
    printf("],\n\"cellsDubSync\":[");
    {   int16_t keep = TSEL->p[P_DUB_MODE];
        TSEL->p[P_DUB_MODE] = (int16_t)(DUB_MODE_MARK | 1);
        for (i = 0; i < NPAGES; i++) {
            printf("%s\n ", i ? "," : "");
            if (PAGES[i].scope == SC_TRACK) cell_row(&PAGES[i]); else printf("null");
        }
        TSEL->p[P_DUB_MODE] = keep;
    }
    /* every parameter's class too (the Sounds sheets, the mixer), then the classes' text */
    printf("],\n\"tpClass\":[");
    for (i = 0; i < (uint32_t)P_E0; i++) printf("%s%d", i ? "," : "", dclass(&TP[i]));
    printf("],\n\"gpClass\":[");
    for (i = 0; i < (uint32_t)G_COUNT; i++) printf("%s%d", i ? "," : "", dclass(&GP[i]));
    printf("],\n\"mixer\":{\"level\":%d,\"pan\":%d,\"mute\":%d},", P_LEVEL, P_PAN, P_MUTE);
    printf("\n\"dub\":{\"mode\":%d,\"tape\":%d,\"mark\":%d,\"driveN\":%d,\"wowN\":%d,\"sync\":%u,\"ping\":%u,\"drive\":%u,\"wow\":%u},",
           P_DUB_MODE, P_DUB_TAPE, DUB_MODE_MARK, DUB_DRIVE_N, DUB_WOW_N, DUBF_SYNC, DUBF_PING, DUBF_DRIVE, DUBF_WOW);
    printf("\n\"recq\":{\"global\":%u,\"track\":%d},", G_TRK_RECQ, P_RECQ);
    /* dub_put_field's own results, for the web app's test of its write of a view field: from two start states, every field, every
     * value of its cell: [start MODE, start TAPE, field, value, MODE after, TAPE after] */
    printf("\n\"dubPut\":[");
    {   static const int16_t start[2][2] = {{DUB_MODE_MARK, -64}, {DUB_MODE_MARK | 3, 127}};
        uint32_t s, f, n = 0;
        for (s = 0; s < 2u; s++)
            for (f = DUBF_SYNC; f <= DUBF_WOW; f++) {
                int32_t hi = f == DUBF_DRIVE ? DUB_DRIVE_N - 1 : f == DUBF_WOW ? DUB_WOW_N - 1 : 1;
                for (v = 0; v <= hi; v++) {
                    int16_t p[P_COUNT] = {0};
                    p[P_DUB_MODE] = start[s][0]; p[P_DUB_TAPE] = start[s][1];
                    dub_put_field(p, f, v);
                    printf("%s[%d,%d,%u,%d,%d,%d]", n++ ? "," : "", start[s][0], start[s][1], f, v, p[P_DUB_MODE], p[P_DUB_TAPE]);
                }
            }
    }
    printf("],");
    /* the ids the editor link's SET / TRACK_PARAM refuse (editor.c: param_reserved, param_slide_time, global_reserved): the app shows them, read only */
    printf("\n\"refused\":{\"tp\":[");
    for (i = 0, v = 0; i < (uint32_t)P_COUNT; i++) if (param_reserved(i) || param_slide_time(i)) printf("%s%u", v++ ? "," : "", i);
    printf("],\"gp\":[");
    for (i = 0, v = 0; i < (uint32_t)G_COUNT; i++) if (global_reserved(i)) printf("%s%u", v++ ? "," : "", i);
    printf("]},");
    printf("\n\"classes\":[");
    for (i = 0; i < ndcls; i++) {
        const param_desc_t *d = DCLS[i];
        printf("%s\n {\"fmt\":%d,\"min\":%d,\"max\":%d,\"text\":[", i ? "," : "", d->fmt, d->min, d->max);
        for (v = d->min; v <= d->max; v++) {
            char val[16] = {0};
            const char *unit = "";
            char t[32];
            param_format(d, v, val, &unit);
            snprintf(t, sizeof t, "%s%s%s", val, unit && unit[0] ? " " : "", unit ? unit : "");
            if (v > d->min) putchar(',');
            js(t);
        }
        printf("]}");
    }
    printf("]");
}

int main(void)
{
    uint32_t i, k;
    printf("{\"generatedBy\":\"tests/web_tables.c\",\"note\":\"generated: do not edit (tests/web_tables.c)\",\n");
    printf("\"consts\":{\"P_COUNT\":%d,\"G_COUNT\":%d,\"P_E0\":%d,\"NTRK\":%d,\"NSTEP\":%d,\"NENGINES\":%d,\"NPAGES\":%u},\n",
           P_COUNT, G_COUNT, P_E0, NTRK, NSTEP, NENGINES, (unsigned)NPAGES);
    /* the parameters: P_E0.. are the engine's, shown by each engine's EDIT descriptors */
    printf("\"TP\":[");
    for (i = 0; i < (uint32_t)P_E0; i++) { printf("%s\n ", i ? "," : ""); desc(&TP[i]); }
    printf("],\n\"GP\":[");
    for (i = 0; i < (uint32_t)G_COUNT; i++) { printf("%s\n ", i ? "," : ""); desc(&GP[i]); }
    printf("],\n\"families\":[");
    for (i = 0; i < FAM_COUNT; i++) { if (i) putchar(','); js(FAM_NAME[i]); }
    /* the page table, in the firmware's order (the build's #if resolved) */
    printf("],\n\"pages\":[");
    for (i = 0; i < NPAGES; i++) {
        const page_t *p = &PAGES[i];
        printf("%s\n {\"i\":%u,\"title\":", i ? "," : "", i); js(p->title);
        printf(",\"fam\":"); js(p->fam < FAM_COUNT ? FAM_NAME[p->fam] : "?");
        printf(",\"scope\":"); js(p->scope <= SC_FX ? SCOPE_NAME[p->scope] : "?");
        if (p->graph < sizeof GRAPH_NAME / sizeof GRAPH_NAME[0]) { printf(",\"graph\":"); js(GRAPH_NAME[p->graph]); }
        else printf(",\"graph\":\"#%u\"", p->graph);
        printf(",\"id\":[%d,%d,%d,%d]}", p->id[0] == 0xFFu ? -1 : p->id[0], p->id[1] == 0xFFu ? -1 : p->id[1], p->id[2] == 0xFFu ? -1 : p->id[2],
               p->id[3] == 0xFFu ? -1 : p->id[3]);
    }
    /* the engines: name, EDIT descriptors, page titles, HOME's four knobs (oc_knobs.c; none: the engine keeps its EDIT knobs) */
    printf("],\n\"engines\":[");
    for (i = 0; i < NENGINES; i++) {
        const engine_t *g = ENGINES[i];
        const knob_def_t *kd = kdef_of(i);
        printf("%s\n {\"num\":%u,\"name\":", i ? "," : "", i); js(g && g->name ? g->name : "-");
        printf(",\"inBuild\":%d,\"edit\":[", g && g->name && g->name[0] != '-' ? 1 : 0);
        for (k = 0; k < 8u; k++) { if (k) putchar(','); desc(&g->edit[k]); }
        printf("],\"knobs\":");
        if (!kd) printf("null");
        else {
            putchar('[');
            for (k = 0; k < 4u; k++) {
                printf("%s{\"label\":", k ? "," : ""); js(kd[k].label);
                printf(",\"kind\":\"%s\",\"id\":%d,\"targets\":[", kd[k].kind == KD_MACRO ? "macro" : kd[k].kind == KD_PLAIN ? "plain" : "none", kd[k].id);
                for (uint32_t t = 0, n = 0; t < KD_TGT; t++)
                    if (kd[k].kind == KD_MACRO && kd[k].t[t].id != KD_NOT)
                        printf("%s{\"id\":%u,\"shape\":%u,\"depth\":%d}", n++ ? "," : "", kd[k].t[t].id, kd[k].t[t].shape, kd[k].t[t].depth);
                printf("]}");
            }
            putchar(']');
        }
        printf("}");
    }
    /* the order the unit shows its engines in (engines.c ENGINE_ORDER: engine numbers; a module's engine only while its module is there) */
    printf("],\n\"engineOrder\":[");
    for (i = 0; i < NENG_SHOWN; i++) printf("%s%u", i ? "," : "", ENGINE_ORDER[i]);
    /* the system menu's items, in order (ui_menu.c) */
    printf("],\n\"scenes\":[");
    for (i = 0; i < NELEM(SCENES); i++) {
        printf("%s{\"engine\":%u,\"bytes\":[", i ? "," : "", SCENES[i].engine);
        for (k = 0; k < SCENES[i].len; k++) printf("%s%u", k ? "," : "", SCENES[i].scene[k]);
        printf("]}");
    }
    printf("],\n\"menu\":[");
    for (i = 0; i < MI_COUNT; i++) { if (i) putchar(','); js(MI_NAME[i]); }
    printf("],\n\"menuUsb\":[");
    for (i = 0; i < 4; i++) { if (i) putchar(','); js(MI_USB_NAME[i]); }
    /* the shortcut layers (ui_layer.c LAYERS): the held button, its header, the footer's hints; FX layer's cells; LAUNCH names */
    printf("],\n\"layers\":[");
    for (i = 2, k = 0; i < LAYER_N; i++) {
        const layer_t *l = &LAYERS[i];
        printf("%s\n {\"head\":", k++ ? "," : ""); js(l->head);
        printf(",\"kind\":\"%s\",\"foot\":[", l->kind == LK_HOLD ? "hold" : "set");
        for (uint32_t f = 0, n = 0; f < 3u; f++)
            if (l->foot[f].act) { printf("%s", n++ ? "," : ""); js(l->foot[f].act); }
        printf("]}");
    }
    printf("],\n\"fxCells\":[");
    for (i = 0; i < NELEM(FX_CELL); i++) { printf("%s[", i ? "," : ""); js(FX_CELL[i].key); putchar(','); js(FX_CELL[i].name); putchar(']'); }
    printf("],\n\"launch\":[");
    for (i = 0; i < OCL_NLAUNCH; i++) { if (i) putchar(','); js(LAUNCH_NAME[i]); }
    /* DUB's performance keys (dub_keys.h): the semitone from C of the view's octave -> the act, and the throws' levels (CC 16..21) */
    printf("],\n\"dubKeys\":{\"first\":%u,\"last\":%u,\"act\":[", DUBK_FIRST, DUBK_LAST);
    for (i = 0; i < 12u; i++) printf("%s%d", i ? "," : "", DUBK_ACT[i] == 0xFFu ? -1 : DUBK_ACT[i]);
    printf("],\"level\":[");
    for (i = 0; i < 6u; i++) printf("%s%u", i ? "," : "", DUBK_LEVEL[i]);
    printf("]},\n");
    display();
    printf("\n}\n");
    return 0;
}
