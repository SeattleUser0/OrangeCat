/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: the web app's view of the project record, generated from the firmware's own C (Session D, web/app).
 *
 *   build/host/app_layout json              > web/app/lib/layout.json   (the table the app's codec reads; checked in)
 *   build/host/app_layout fixtures DIR                                   (FBKK records and lists the firmware wrote)
 *
 * The app reads and writes the project as the backup objects carry it: an FBKK record (pattern_store.c) around a FUN23
 * project part (project.c), plus, through command 102, Orange Cat's lists (pattern.c ocl_lists_t). Their layout is the
 * firmware's: every offset and size below comes from the macros, the structs and the tables the firmware is built
 * from, so the page cannot drift from it. The bit layouts of a packed step (proj_step_pack) and a recorded note
 * (proj_record_pack) are code, not structs: the JSON gives them as field lists and, as "probes", values the firmware
 * itself packed (proj_step_pack, proj_record_pack), which web/app/test checks its encoder against. The fixtures are
 * records the firmware made (bank_pack, from a sequencer filled by tests/oc_project_test.c's fill_all), which the app's
 * codec must read and write back byte for byte (web/app/test/test_codec.mjs).
 *
 * tests/run_tests.sh builds this, writes the JSON and the fixtures to build/host/app/, and node web/app/test/run.mjs
 * fails when the checked-in web/app/lib/layout.json or tests/fixtures/app/ differ from what the firmware writes now.
 *
 * Revisions:
 *   0.14.2: first version (branch web-app).
 */
#define OC_PROJECT_NO_MAIN
#include "oc_project_test.c"

static void js_str(const char *s)
{
    putchar('"');
    for (; s && *s; s++) {
        if (*s == '"' || *s == '\\') putchar('\\');
        putchar(*s);
    }
    putchar('"');
}

/* ------------------------------------------------------------------ the JSON --- */
#define KV(name, v) printf("%s\"%s\":%ld", first++ ? "," : "", name, (long)(v))
static void layout_json(void)
{
    int first;
    printf("{\"generatedBy\":\"tests/app_layout.c\",\"note\":\"generated: do not edit (tests/app_layout.c json)\",\n");
    /* the counts */
    first = 0;
    printf("\"consts\":{");
    KV("NTRK", NTRK); KV("NSTEP", NSTEP); KV("NPAT", NPAT); KV("P_COUNT", P_COUNT); KV("G_COUNT", G_COUNT); KV("P_E0", P_E0);
    KV("NENGINES", NENGINES); KV("CHAIN_ROWS", CHAIN_ROWS); KV("MOTION_MAX", MOTION_MAX); KV("RECORD_MAX", RECORD_MAX);
    KV("PROJ_NAME_LEN", PROJ_NAME_LEN); KV("FM6_PACKED", FM6_PACKED); KV("FM6_NFN", FM6_NFN); KV("CZ_BYTES", CZ_BYTES);
    KV("DRUM_WIRE_BYTES", DRUM_WIRE_BYTES); KV("P5_BYTES", sizeof(p5_patch_t)); KV("P_SLEN", P_SLEN); KV("P_LEVEL", P_LEVEL);
    KV("P_PAN", P_PAN); KV("P_MUTE", P_MUTE); KV("TC_COUNT", TC_COUNT); KV("NUDGE_MIN", NUDGE_MIN); KV("NUDGE_MAX", NUDGE_MAX);
    KV("ST_NOTE", ST_NOTE); KV("ST_TIE", ST_TIE); KV("ST_REST", ST_REST); KV("SF_ACCENT", SF_ACCENT); KV("SF_SLIDE", SF_SLIDE);
    KV("SF_RECORDED", SF_RECORDED); KV("SF_RATCH_SHIFT", 3); KV("SF_FLAM", SF_FLAM); KV("RECORD_UNIT", RECORD_UNIT);
    printf("},\n");
    /* the FBKK record around the project part (pattern_store.c) */
    first = 0;
    printf("\"fbkk\":{");
    KV("magic", BANK_MAGIC); KV("size", BANK_STORE_SIZE); KV("projOff", 8); KV("projSize", BANK_PROJ_SIZE);
    KV("activeOff", BANK_ACTIVE_OFF); KV("extraOff", BANK_EXTRA_OFF); KV("stepSize", BANK_STEP_SIZE);
    KV("timingOff", BANK_TIMING_OFF); KV("timeSize", BANK_TIME_SIZE); KV("chainOff", BANK_CHAIN_OFF);
    KV("motionOff", BANK_MOTION_OFF); KV("fnOff", BANK_FN_OFF); KV("fnMagic", BANK_FN_MAGIC); KV("sumOff", BANK_STORE_SIZE - 4u);
    printf("},\n");
    /* the project part (project.c proj_pack) */
    first = 0;
    printf("\"proj\":{");
    KV("magic", PROJ_MAGIC); KV("size", PROJ_STORE_SIZE); KV("globalsOff", 8); KV("globals", PROJ_GLOBALS);
    KV("selOff", 62); KV("partsOff", 63); KV("physOff", 64); KV("banksLoOff", 65); KV("npOff", 66); KV("banksHiOff", 67);
    KV("tracksOff", 68); KV("chainSize", sizeof(chain_config_t)); KV("motionSize", sizeof(motion_store_t));
    KV("fxModeOff", PROJ_FX_MODE_OFF); KV("jfxWire", PROJ_JFX_WIRE); KV("jfxN", PROJ_JFX_N); KV("mpcOctWire", PROJ_MPC_OCT_WIRE);
    KV("fm6Off", PROJ_FM6_OFF); KV("czOff", PROJ_CZ_OFF); KV("recOff", PROJ_REC_OFF); KV("p5Off", PROJ_P5_OFF);
    KV("drumOff", PROJ_DRUM_OFF); KV("recDirOff", PROJ_REC_DIR); KV("recDirBits", 11); KV("nameOff", PROJ_NAME_OFF);
    KV("sumOff", PROJ_STORE_SIZE - 4u);
    printf(",\"knownMagics\":[%u,%u,%u,%u,%u,%u,%u,%u]", PROJ_MAGIC, PROJ_FUN22_MAGIC, PROJ_CUT_MAGIC, PROJ_PRE_HP_MAGIC,
           0x46554E46u, 0x46554E45u, 0x46554E44u, 0x46554E43u);
    printf("},\n");
    /* a packed step and a recorded note, as fields in order (LSB first, wire_bits_put) */
    printf("\"stepBits\":[[\"note0\",7],[\"note1\",7],[\"note2\",7],[\"note3\",7],[\"meta\",4],[\"flags\",2],[\"vel\",7],[\"hit\",8],[\"acc\",8],[\"prob\",7]],\n");
    printf("\"noteBits\":[[\"on\",16],[\"duration\",16],[\"note\",7],[\"vel\",7],[\"step\",8],[\"exp\",3],[\"pitch\",6],[\"length\",1]],\n");
    /* probes: the firmware's own packing of a few values (proj_step_pack, proj_record_pack) */
    {
        static const step_t S[] = {
            {{60, 64, 67, 71}, 4, ST_NOTE, SF_ACCENT | SF_SLIDE, 100, 0, 0, 50, 0, 0},
            {{0}, 0, ST_TIE, 0, 0, 0, 0, 0, 0, 0},
            {{0}, 0, ST_REST, 0, 0, 0, 0, 101, 0, 0},
            {{36}, 1, ST_NOTE, SF_RECORDED, 127, 0x81, 0x01, 100, 0, 0},
            {{127, 1, 2, 3}, 2, ST_NOTE, 0, 1, 0, 200, 1, 0, 0},
        };
        printf("\"stepProbes\":[");
        for (uint32_t i = 0; i < sizeof S / sizeof S[0]; i++) {
            uint8_t b[8];
            if (!proj_step_pack(b, &S[i])) { fprintf(stderr, "probe %u does not pack\n", i); exit(1); }
            printf("%s{\"step\":{\"notes\":[%u,%u,%u,%u],\"n\":%u,\"time\":%u,\"flags\":%u,\"vel\":%u,\"hit\":%u,\"acc\":%u,\"prob\":%u},\"bytes\":[",
                   i ? "," : "", S[i].note[0], S[i].note[1], S[i].note[2], S[i].note[3], S[i].n, S[i].time, S[i].flags, S[i].vel, S[i].hit, S[i].acc, S[i].probability);
            for (uint32_t j = 0; j < 8u; j++) printf("%s%u", j ? "," : "", b[j]);
            printf("]}");
        }
        printf("],\n");
    }
    {
        static project_t q;
        memset(&q, 0, sizeof q);
        q.recording[0] = (recorded_note_t){1234, 40000, 60, 100, (uint8_t)(2u << 5 | 9u), 17, -3, 1};
        q.recording[1] = (recorded_note_t){0, 65535, 127, 1, 0, 63, 24, 0};
        static uint8_t b[PROJ_STORE_SIZE];
        memset(b, 0, sizeof b);
        if (!proj_record_pack(b, &q)) { fprintf(stderr, "note probe does not pack\n"); exit(1); }
        printf("\"noteProbes\":[");
        /* owner 9 (track 1, bank 1) comes after owner 0 in the record's order */
        printf("{\"owner\":0,\"note\":{\"on\":0,\"duration\":65535,\"note\":127,\"vel\":1,\"step\":63,\"exp\":0,\"pitch\":24,\"length\":0},\"bytes\":[");
        for (uint32_t j = 0; j < 8u; j++) printf("%s%u", j ? "," : "", b[PROJ_REC_OFF + j]);
        printf("]},{\"owner\":9,\"note\":{\"on\":1234,\"duration\":40000,\"note\":60,\"vel\":100,\"step\":17,\"exp\":2,\"pitch\":-3,\"length\":1},\"bytes\":[");
        for (uint32_t j = 0; j < 8u; j++) printf("%s%u", j ? "," : "", b[PROJ_REC_OFF + 8u + j]);
        printf("]}],\n");
    }
    /* the lists (pattern.c ocl_lists_t), field by field */
    {
        ocl_lists_t *l = 0;
        first = 0;
        printf("\"lists\":{");
        KV("size", sizeof(ocl_lists_t)); KV("NPOOL", OCL_NPOOL); KV("NCHAIN", OCL_NCHAIN); KV("NENT", OCL_NENT); KV("NROW", OCL_NROW);
        KV("NSCENE", OCL_NSCENE); KV("NOROW", OCL_NOROW); KV("NLAUNCH", OCL_NLAUNCH); KV("GROOVE_N", GROOVE_N);
        KV("chainOff", (uintptr_t)&l->chain); KV("songOff", (uintptr_t)&l->song); KV("tchainOff", (uintptr_t)&l->tchain);
        KV("curOff", (uintptr_t)&l->cur); KV("rowOff", (uintptr_t)&l->row); KV("modeOff", (uintptr_t)&l->mode);
        KV("launchOff", (uintptr_t)&l->launch); KV("sceneOff", (uintptr_t)&l->scene); KV("sceneUsedOff", (uintptr_t)&l->scene_used);
        KV("grooveOff", (uintptr_t)&l->groove); KV("gamtOff", (uintptr_t)&l->gamt);
        printf(",\"grooves\":[");
        for (uint32_t g = 0; g < GROOVE_N; g++) { if (g) putchar(','); js_str(GROOVE_NAME[g]); }
        printf("],\"modes\":[\"PHRASE\",\"CHAIN\",\"SONG\"],\"launches\":[\"LOOP\",\"BAR\",\"BEAT\",\"NOW\"]},\n");
    }
    /* the engines: name, the HOME (STAGE) four knobs as engine parameter numbers 0..7 (ENGINES[e]->knob, ui.c stage_page) */
    printf("\"engines\":[");
    for (uint32_t e = 0; e < NENGINES; e++) {
        const engine_t *g = ENGINES[e];
        printf("%s{\"num\":%u,\"name\":", e ? "," : "", e);
        js_str(g && g->name ? g->name : "-");
        printf(",\"knob\":[%u,%u,%u,%u],\"edit\":[", g ? g->knob[0] : 0, g ? g->knob[1] : 0, g ? g->knob[2] : 0, g ? g->knob[3] : 0);
        for (uint32_t k = 0; k < 8u; k++) { if (k) putchar(','); js_str(g ? g->edit[k].label : ""); }
        printf("]}");
    }
    printf("]\n}\n");
}

/* ---------------------------------------------------------------- fixtures --- */
static void put_file(const char *dir, const char *name, const void *p, uint32_t n)
{
    char path[512];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    if (!f || fwrite(p, 1, n, f) != n) { fprintf(stderr, "cannot write %s\n", path); exit(1); }
    fclose(f);
}
static uint8_t fx_raw[BANK_STORE_SIZE];
/* the music loaded now, as backup object 0 reads it (editor_backup.c ed_bk_capture: project_capture, bank_pack runtime) */
static void runtime_fbkk(const char *dir, const char *name)
{
    project_capture(&proj_scratch);
    if (!bank_pack(fx_raw, &proj_scratch, 1) || !bank_valid(fx_raw, BANK_STORE_SIZE)) { fprintf(stderr, "%s: bank_pack failed\n", name); exit(1); }
    put_file(dir, name, fx_raw, BANK_STORE_SIZE);
}
static void name_project(const char *s)
{
    memset(proj_name, 0, sizeof proj_name);
    for (uint32_t i = 0; s[i] && i < PROJ_NAME_LEN; i++) proj_name[i] = s[i];
}
static void fixtures(const char *dir)
{
    /* 1. a new project, as a unit just reset has it */
    reset();
    runtime_fbkk(dir, "fbkk-new.bin");
    put_file(dir, "lists-new.bin", &lists, sizeof lists);
    /* 2. one bank a track, notes, chords, Skiff's fields, timing, motion; a name; engines changed */
    reset();
    fill_all(3, 1);
    set_engine_of(&trk[1], 12); apply_preset_to(&trk[1], 2); trk[1].engine = trk[1].eng_req;
    set_engine_of(&trk[3], 10); apply_preset_to(&trk[3], 0); trk[3].engine = trk[3].eng_req;
    trk[2].p[P_LEVEL] = 77; trk[2].p[P_PAN] = -20;
    song.sel = 2;
    name_project("ONE BANK");
    runtime_fbkk(dir, "fbkk-one-bank.bin");
    /* 3. all 32 banks, motion on two tracks, timed notes on every track's bank 2, the active banks moved */
    reset();
    fill_all(7, NPAT);
    for (uint32_t k = 0; k < NTRK; k++) pattern_request(&trk[k], (k * 3u + 1u) % NPAT);
    name_project("FULL 32");
    runtime_fbkk(dir, "fbkk-full.bin");
    /* the lists of a project with chains, a song and scenes (the bytes command 102 reads) */
    ocl_lists_init(&lists);
    for (uint32_t c = 0; c < 6u; c++)
        for (uint32_t e = 0; e < 5u + c; e++) { lists.chain[c][e][0] = (uint8_t)(1u + (c * 7u + e) % OCL_NPOOL); lists.chain[c][e][1] = (uint8_t)(e % 4u); }
    for (uint32_t r = 0; r < 9u; r++)
        for (uint32_t k = 0; k < NTRK; k++) lists.song[r][k] = (uint8_t)((r + k) % 7u);
    lists.tchain[1] = 3; lists.cur[1] = 2; lists.row[1] = 4; lists.mode = OCL_SONG; lists.launch = OCL_BAR;
    lists.scene[0][0][0] = 2; lists.scene[0][0][1] = 0x80u | 1u; lists.scene[5][3][0] = 6; lists.scene_used = 0x21u;
    lists.groove[0] = 3; lists.gamt[0] = 55;
    put_file(dir, "lists-song.bin", &lists, sizeof lists);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "json")) { layout_json(); return 0; }
    if (argc >= 3 && !strcmp(argv[1], "fixtures")) { fixtures(argv[2]); return 0; }
    fprintf(stderr, "usage: app_layout json | fixtures DIR\n");
    return 2;
}
