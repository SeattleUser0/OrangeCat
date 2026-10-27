/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.8.0 (step 6): fw_api_t, the services table a loadable engine would be given (firmware/src/fw_api.h), against
 * the real services it points at. Nothing loads from it yet; this is the preparation's check.
 *   build/host/fw_api_test
 */
/* Revisions:
 * (Orange Cat 0.9.0) DRUM-X, Jiant FM's drum engine, in engine slot 9; the loadable-engine interface identifier.
 * (Orange Cat 0.10.0) fw_api_t at a fixed address, no RAM in the module (0.10.0)
 * (Orange Cat 0.12.2) the Prophet module fits 8 sectors (34,764 B measured): the firmware's PITCH_OCT and ENV_LIN through the table, the factory programs in a palette fo...
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
    check("the table starts with the magic, its own size, version 6 (0.14.3: batch_push takes AP_SHAPES, AP_WHEEL) and the interface identifier", a->magic == FW_API_MAGIC && a->size == sizeof(fw_api_t) && a->version == 6u && a->iface == FW_API_IFACE);
    check("(0.14.3) a module that needs nothing 6 added declares FW_API_VERSION_BASE, 5: a 0.14.2 firmware still loads it", FW_API_VERSION_BASE == 5u && FW_API_VERSION_BASE < FW_API_VERSION);
    check("every service is there", a->mem_copy && a->mem_set && a->resource_get && a->tuning_log && a->trk && a->pack_ready);
    /* 0.10.0 (step 1) the arena id a module's part state starts at, and the engine count, come from the table */
    check("res_engine0 is the arena id of the first engine state and nengines the table's length", a->res_engine0 == RES_ENGINE0 && a->nengines == NENGINES && NENGINES == 20u);
    check("rng_cell is the generator word SHAPES starts with (0x21), shared by the parts", a->rng_cell && *a->rng_cell == 0x21u);
    check("a module's part p gets the block the built-in engine gets (res_engine0 + p)", (memset(resource, 0, sizeof resource), a->resource_get(a->res_engine0 + 1u, 64u) == resource_get(RES_ENGINE0 + 1u, 64u)));
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
    /* 0.12.1 / 0.12.2: what the Prophet module reads through the table instead of carrying: the track voices' last state, the firmware's pitch and envelope tables */
    check("voice_was, pitch_oct and env_lin are the firmware's own (the Prophet module reads them through the table)", a->voice_was == &voice_was && a->pitch_oct == PITCH_OCT && a->env_lin == ENV_LIN &&
          a->pitch_oct[0] == PITCH_OCT[0] && a->pitch_oct[191] == PITCH_OCT[191] && a->env_lin[127] == ENV_LIN[127]);
    check("(0.13.7) hot is the firmware's per-engine table of hot-set pointers (engines.c eng_hs.ptr)", a->hot == (const void *const *const *)eng_hs.ptr);
    /* (0.14.0) the firmware's own hot set (dsp.c dsp_hot_at): SINE (the image's flash until boot copies it into the arena) and TANH_Q15 (SRAM) */
    check("(0.14.0) base_hot is the firmware's own hot set: SINE and TANH_Q15, the tables sine_i and softclip read", a->base_hot == (const void *const *)dsp_hot_at &&
          a->base_hot[DSP_HOT_SINE] == (const void *)SINE && a->base_hot[DSP_HOT_TANH] == (const void *)TANH_Q15);
    {   int32_t before = sine_i(0x12345678u), clip = softclip(20000);
        memset(resource, 0, sizeof resource);
        eng_hot_base();
        check("(0.14.0) at boot SINE is copied into the arena (RES_HOT_BASE, 2,052 B) and sine_i reads the copy: the same value",
              resource[RES_HOT_BASE].ptr && resource[RES_HOT_BASE].size == 2052u && a->base_hot[DSP_HOT_SINE] == resource[RES_HOT_BASE].ptr &&
              !memcmp(a->base_hot[DSP_HOT_SINE], SINE, sizeof SINE) && sine_i(0x12345678u) == before && softclip(20000) == clip);
        eng_hot_base();
        check("(0.14.0) a second call copies nothing again (the pointer is the copy already)", a->base_hot[DSP_HOT_SINE] == resource[RES_HOT_BASE].ptr);
        resource_release(RES_HOT_BASE);
        dsp_hot_at[DSP_HOT_SINE] = SINE;
    }
    oc_pack_good = 0;
    check("pack_ready follows the pack's flag (0)", *a->pack_ready == 0);
    oc_pack_good = 1;
    check("pack_ready follows the pack's flag (1)", *a->pack_ready == 1);
    oc_pack_good = OC_PACK_GATE ? 0 : 1;
    /* 0.9.0 (step 5a): the identifier and the registration check */
    {
        fw_mod_hdr_t h = {FW_MOD_MAGIC, FW_API_IFACE, sizeof(fw_api_t), FW_API_VERSION, FW_API_ADDR}, g;
        check("the identifier is the generated hash mixed with the sizes it cannot see",
              FW_API_IFACE == (FW_API_ID ^ ((uint32_t)sizeof(track_t) * 0x9E3779B1u) ^ ((uint32_t)sizeof(voice_t) * 0x85EBCA6Bu) ^
                               ((uint32_t)sizeof(engine_t) * 0xC2B2AE35u) ^ ((uint32_t)sizeof(vmod_t) * 0x27D4EB2Fu)) && FW_API_ID != 0u);
        check("a module built for this firmware is accepted", fw_module_check(&h) == FW_MOD_OK);
        check("no header is refused", fw_module_check(0) == FW_MOD_NONE);
        g = h; g.magic ^= 1u;
        check("a wrong magic is refused", fw_module_check(&g) == FW_MOD_BAD_MAGIC);
        for (i = 0; i < 32; i++) {
            g = h; g.iface ^= 1u << i;
            if (fw_module_check(&g) != FW_MOD_BAD_IFACE) break;
        }
        check("an identifier differing in any one bit is refused", i == 32);
        for (i = 0; i < 32; i++) {
            g = h; g.api_addr ^= 1u << i;
            if (fw_module_check(&g) != FW_MOD_BAD_ADDR) break;
        }
        check("a table address differing in any one bit is refused (0.10.0: the module would read some other place)", i == 32);
        check("the table's address is a word-aligned XIP address 0x100 bytes into the body", (FW_API_ADDR & 3u) == 0 && FW_API_ADDR == 0x02001000u + 0x100u);
        g = h; g.api_size = sizeof(fw_api_t) + 4u;
        check("a module that needs a longer table is refused", fw_module_check(&g) == FW_MOD_API_TOO_NEW);
        g = h; g.api_version = FW_API_VERSION + 1u;
        check("a module built for a later version is refused", fw_module_check(&g) == FW_MOD_API_TOO_NEW);
        g = h; g.api_size = sizeof(fw_api_t) - 8u; g.api_version = 1u;
        check("a module that needs a shorter table and an earlier version is accepted", fw_module_check(&g) == FW_MOD_OK);
    }
    printf(bad ? "fw_api: FAILED\n" : "fw_api: all passed\n");
    return bad != 0;
}
