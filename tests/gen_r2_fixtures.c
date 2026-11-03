/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Writes tests/fixtures/r2_projects/*.bin with the Release 2 project writer (project_capture, proj_pack) as it is in the tree this is built from.
 * Not part of the suite: run it by hand ONLY to make fixtures (they are committed once, so that the project format revision, which changes the writer, cannot change what fx_legacy_test loads):
 *   cc -O1 -DOC_HOME_PIC=1 -DMELODEE_USB_AUDIO=1 <FW flags as run_tests.sh> -Itests tests/gen_r2_fixtures.c -o gen -lm && ./gen tests/fixtures/r2_projects
 * Each file is the project_store_t bytes (13,212 B, the Release 2 record). */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include "fx_r2_values.h"
static void put_fixture(const char *dir, const char *name)
{
    project_store_t wire;
    char path[300];
    FILE *f;
    project_capture(&proj_scratch);
    if (!proj_pack(&wire, &proj_scratch)) { printf("gen: %s does not pack\n", name); exit(2); }
    snprintf(path, sizeof path, "%s/%s", dir, name);
    f = fopen(path, "wb");
    if (!f || fwrite(&wire, sizeof wire, 1, f) != 1) { printf("gen: cannot write %s\n", path); exit(2); }
    fclose(f);
    printf("gen: %s (%u B)\n", path, (unsigned)sizeof wire);
}
static void fresh(void)
{
    ui_power_on();
    for (uint32_t i = 0; i < G_COUNT; i++) song.g[i] = GP[i].def;
}
int main(int argc, char **argv)
{
    static const uint32_t G[7] = {G_RSIZE, G_RDAMP, G_RPRE, G_RMOD, G_RRATE, G_RFILT, G_RWIDE};
    const char *dir = argc > 1 ? argv[1] : "tests/fixtures/r2_projects";
    for (uint32_t k = 0; k < 4; k++) {
        fresh();
        song.g[G_RTYPE] = (int16_t)(k + 1);
        for (uint32_t i = 0; i < 7; i++) song.g[G[i]] = FXR2_KN[k][i];
        put_fixture(dir, FXR2_REV_FILE[k]);
    }
    fresh();
    song.g[G_RTYPE] = 3;
    for (uint32_t i = 0; i < 7; i++) song.g[G[i]] = FXR2_CLAMP[i];
    put_fixture(dir, FXR2_CLAMP_FILE);
    fresh();
    {   int16_t *p = trk[0].p;
        p[P_DUB_FDBK] = FXR2_DUB[0]; p[P_DUB_SEND] = FXR2_DUB[1]; p[P_DUB_FILT] = FXR2_DUB[2]; p[P_DUB_ECHO] = FXR2_DUB[3];
        p[P_DUB_TIML] = FXR2_DUB[4]; p[P_DUB_TIMR] = FXR2_DUB[5]; p[P_DUB_SPRG] = FXR2_DUB[6]; p[P_DUB_SPD] = FXR2_DUB[7];
        dub_put_field(p, DUBF_SYNC, FXR2_DUB_SYNC); dub_put_field(p, DUBF_PING, FXR2_DUB_PING);
        dub_put_field(p, DUBF_DRIVE, FXR2_DUB_DRIVE); dub_put_field(p, DUBF_WOW, FXR2_DUB_WOW);
    }
    put_fixture(dir, FXR2_DUB_FILE);
    return 0;
}
