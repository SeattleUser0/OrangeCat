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
    printf("]}\n}\n");
    return 0;
}
