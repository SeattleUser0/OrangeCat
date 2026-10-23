/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0 (step 6): fw_api_t, the services table a loadable engine would be given (firmware/src/fw_api.h), against
 * the real services it points at. Nothing loads from it yet; this is the preparation's check.
 *   build/host/fw_api_test
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("fw_api: %-86s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
int main(void)
{
    const fw_api_t *a = &FW_API;
    uint8_t x[16], y[16];
    uint32_t i;
    void *p1, *p2;
    check("the table starts with the magic, its own size and version 1", a->magic == FW_API_MAGIC && a->size == sizeof(fw_api_t) && a->version == 1u);
    check("every service is there", a->mem_copy && a->mem_set && a->resource_get && a->tuning_log && a->trk && a->pack_ready);
    for (i = 0; i < 16; i++) x[i] = (uint8_t)(i * 13u + 1u);
    a->mem_copy(y, x, 16);
    check("mem_copy copies", !memcmp(x, y, 16));
    a->mem_set(y, 0x5A, 16);
    check("mem_set fills", y[0] == 0x5A && y[15] == 0x5A);
    for (tuning_a4 = A4_MIN; tuning_a4 <= A4_MAX; tuning_a4++)
        if (a->tuning_log() != tuning_log()) break;
    check("tuning_log answers as dsp.c does over the whole A4 range", tuning_a4 > A4_MAX);
    tuning_a4 = 440;
    memset(resource, 0, sizeof resource);
    p1 = a->resource_get(RES_ENGINE0, 256);
    p2 = resource_get(RES_ENGINE0, 256);
    check("resource_get gives the arena's block, the same one the engines get", p1 && p1 == p2);
    check("trk is the track array", a->trk == trk);
    oc_pack_good = 0;
    check("pack_ready follows the pack's flag (0)", *a->pack_ready == 0);
    oc_pack_good = 1;
    check("pack_ready follows the pack's flag (1)", *a->pack_ready == 1);
    oc_pack_good = OC_PACK_GATE ? 0 : 1;
    printf(bad ? "fw_api: FAILED\n" : "fw_api: all passed\n");
    return bad != 0;
}
