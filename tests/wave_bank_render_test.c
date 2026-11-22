/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (wave-bank) WAVE plays the bank in the sample slot's room: the flash tests/wave_bank_slot_test.c left (build/host/wave_bank.nor: the page's bank written with the
 * slot's own calls) mapped where the module reads it (tests/module_host_wave_tu.c wvb_host_flash). USER TABLE A and B render the bank's tables (another sound
 * than without a bank), healthy; without a bank, and with a bank whose header is broken, they render the built-in BASIC (the same as each other); the other
 * presets do not change with a bank there.
 *   build/host/wave_bank_render_test [build/host]
 */
#define REGRESS_MODULE_HOST 1
#include "regress.c"
#include "wv_bank.h"
extern const fw_api_t *oc_host_fw;
extern const fw_mod_hdr_t OC_MODULE_HDR;
extern const engine_t *oc_host_module_engine(void);
extern uint8_t wvb_host_flash[0xC2000u];

static int bad;
static void check(const char *what, int ok)
{
    printf("wave bank render: %-110s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static uint64_t renders[3][16];
static void render_all(uint32_t set, uint32_t np, uint32_t *crashed)
{
    nj = 0;
    for (uint32_t i = 0; i < np; i++) { job_t *j = add(J_PRESET, "wave"); j->e = (uint8_t)ENGI_WAVE; j->pi = (uint8_t)i; }
    run_jobs(J, nj, 8);
    for (uint32_t i = 0; i < np; i++) {
        renders[set][i] = J[i].r.hash;
        *crashed += J[i].crashed != 0 || J[i].r.over || J[i].r.peak < LIM_PEAK_MIN;
    }
}
int main(int argc, char **argv)
{
    static uint8_t nor[0xC2000u];
    const char *dir = argc > 1 ? argv[1] : "build/host";
    char p[512];
    FILE *f;
    fw_mod_hdr_t h;
    const engine_t *em;
    uint32_t np, crashed = 0, i, same = 1;
    snprintf(p, sizeof p, "%s/wave_bank.nor", dir);
    if (!(f = fopen(p, "rb")) || fread(nor, 1, sizeof nor, f) != sizeof nor) { printf("wave bank render: no %s (run tests/wave_bank_slot_test.c first)\n", p); return 1; }
    fclose(f);
    oc_host_fw = &FW_API;
    em = oc_host_module_engine();
    h = OC_MODULE_HDR;
    check("WAVE registers", eng_register(&h, em) == EM_OK);
    np = em->npresets;
    check("its presets include USER TABLE A and B (8, 9)", np >= 10u && !strcmp(eng_preset(em, 8)->name, "USER TABLE A") && !strcmp(eng_preset(em, 9)->name, "USER TABLE B"));
    memset(wvb_host_flash, 0xFF, sizeof wvb_host_flash);     /* an erased slot: no bank */
    render_all(0, np, &crashed);
    memcpy(wvb_host_flash, nor, sizeof wvb_host_flash);       /* the bank */
    render_all(1, np, &crashed);
    wvb_host_flash[wvb_flash(WVB_HDR_OFF)] ^= 1u;             /* a broken magic */
    render_all(2, np, &crashed);
    check("every render healthy (sound, no sample past 16 bits)", crashed == 0);
    check("with the bank, USER TABLE A and B play its tables: other sounds than without", renders[1][8] != renders[0][8] && renders[1][9] != renders[0][9]);
    check("a broken bank plays the built-in table, as no bank does", renders[2][8] == renders[0][8] && renders[2][9] == renders[0][9]);
    for (i = 0; i < 8u; i++) same &= renders[1][i] == renders[0][i];
    check("the factory presets (0..7, built-in tables) are the same with a bank there", same);
    printf(bad ? "wave bank render: FAILED\n" : "wave bank render: all passed\n");
    return bad != 0;
}
