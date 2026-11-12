#include <stdint.h>
#define OC_MOD_HOST_XIP 1
#define OC_MOD_GEO_QUAL
struct fw_mod_hdr;
static const struct fw_mod_hdr *oc_mod_hdr_ptr(uint32_t i);
#define PERSISTENCE_TEST_NO_MAIN
static const void *oc_mod_xip_ptr(uint32_t a);          /* (the model picker) a module's XIP address into the simulated NOR: MACRO reads a MODELS module's table through it */
#define OC_MOD_XIP_PTR(a) oc_mod_xip_ptr(a)
#include "persistence_test.c"
/* the loader hands MACRO the module's table as a pointer table: on the device the 32 bit words are the pointers; here each word is read from the NOR and made a host pointer
 * into it (the fake descriptors of make_models: their state, big and units are what the glue reads; nothing is rendered) */
static const void *oc_mod_xip_ptr(uint32_t a)
{
    static const void *tabs[2][FW_MODELS_MAX];
    static uint32_t k;
    const void **tab = tabs[k++ & 1u];
    uint32_t i;
    for (i = 0; i < FW_MODELS_MAX; i++) {
        uint32_t w, off = a - oc_mod_link(0) + oc_mod_flash(0) + 4u * i;
        memcpy(&w, nor + off, 4);
        tab[i] = w && w >= oc_mod_link(0) && w < oc_mod_link(0) + 0x12000u ? (const void *)(nor + (w - oc_mod_link(0)) + oc_mod_flash(0)) : 0;
    }
    return tab;
}
static const fw_mod_hdr_t *oc_mod_hdr_ptr(uint32_t i) { return (const fw_mod_hdr_t *)(nor + oc_mod_flash(i)); }

/* SPDX-License-Identifier: GPL-3.0-only
 * (Session A, the model picker review) The C planner as an oracle for the installer page's room meter (web/fm1place.js, web/test_place.mjs).
 * stdin, one case a line:  <low 0|1> <nfxmod> then 18 x "<state> <kind> <mid> <engine> <length>" (the oc_mod arrays of a unit), then the queries:
 *   e <engine>            oc_mod_plan(engine)            -> the start or -1
 *   f <mid> <sectors>     oc_fxmod_plan(mid, sectors)    -> the start or -1
 * stdout: one line a case, the answers in order. Built by tests/run_tests.sh like fx_module_test (build/host/plan_oracle). */
int main(void)
{
    char line[8192];
    while (fgets(line, sizeof line, stdin)) {
        char *p = line, *e;
        unsigned long v;
        uint32_t i, lowok, nfx;
        v = strtoul(p, &e, 10); lowok = (uint32_t)v; p = e;
        v = strtoul(p, &e, 10); nfx = (uint32_t)v; p = e;
        memset(&oc_mod, 0, sizeof oc_mod);
        ocs_resv = lowok ? SMP_LEGACY_MASK : 0;
        for (i = 0; i < OC_MOD_STARTS; i++) {
            oc_mod.state[i] = (uint8_t)strtoul(p, &e, 10); p = e;
            oc_mod.kind[i] = (uint8_t)strtoul(p, &e, 10); p = e;
            oc_mod.mid[i] = (uint32_t)strtoul(p, &e, 10); p = e;
            oc_mod.engine[i] = (uint32_t)strtoul(p, &e, 10); p = e;
            oc_mod.length[i] = (uint32_t)strtoul(p, &e, 10); p = e;
        }
        oc_mod.nfxmod = (uint8_t)nfx;
        for (;;) {
            while (*p == ' ') p++;
            if (*p == 'e') { p++; uint32_t eng = (uint32_t)strtoul(p, &e, 10); p = e; printf("%d ", oc_mod_plan(eng)); }
            else if (*p == 'f') { p++; uint32_t mid = (uint32_t)strtoul(p, &e, 10); p = e; uint32_t sec = (uint32_t)strtoul(p, &e, 10); p = e; printf("%d ", oc_fxmod_plan(mid, sec)); }
            else break;
        }
        printf("\n");
    }
    return 0;
}
