/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * (wave-bank) A WAVE bank in the sample slot (firmware/src/smp_slot.c, firmware/src/wv_bank.h) on tests/smp_slot_test.c's simulated NOR: the bank the page packs
 * (web/test_wave_bank.mjs writes build/host/wave_bank.{data,hdr}) written with the slot's own calls (as ED_SLOT_BEGIN / WRITE / END do), committed, read as a bank
 * (state 3, its name) and again after a reboot; the header's checks refuse every broken field (a commit refused leaves the slot empty); a sample header still
 * works; the sector tails stay erased; the flash as left is written to build/host/wave_bank.nor for tests/wave_bank_render_test.c.
 *   build/host/wave_bank_slot_test [build/host]
 */
#define main smp_slot_test_main
#include "smp_slot_test.c"
#undef main

static uint8_t bdata[SMP_SLOT_MAX], bhdr[512];
static uint32_t blen;
static int load(const char *dir)
{
    char p[512];
    FILE *f;
    snprintf(p, sizeof p, "%s/wave_bank.data", dir);
    if (!(f = fopen(p, "rb"))) return 0;
    blen = (uint32_t)fread(bdata, 1, sizeof bdata, f); fclose(f);
    snprintf(p, sizeof p, "%s/wave_bank.hdr", dir);
    if (!(f = fopen(p, "rb"))) return 0;
    int ok = fread(bhdr, 1, 512, f) == 512; fclose(f);
    return ok && blen > 0;
}
static int write_bank(const uint8_t *h)
{
    if (smp_slot_begin(blen)) return -100;
    for (uint32_t off = 0; off < blen; off += 256u) {
        uint32_t n = blen - off < 256u ? blen - off : 256u;
        int rc = smp_slot_write(off, bdata + off, n);
        if (rc) return rc;
    }
    return smp_slot_end(h);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/host";
    char nm[9];
    uint32_t len, nt;
    if (!load(dir)) { printf("wave bank slot: no build/host/wave_bank.{data,hdr} (run web/test_wave_bank.mjs first)\n"); return 1; }
    wipe(); ocs_resv = 0; ocs_boot(); reboot();
    check("a fresh store: the slot claimed and empty", smp_slot_info(0, 0, 0) == SMP_EMPTY);
    check("the page's bank passes wv_bank.h's checks", wvb_hdr_check(bhdr) == 0);
    check("written with the slot's calls, committed", write_bank(bhdr) == 0);
    check("read as a WAVE bank: state 3, its name, its length, its tables", smp_slot_info(nm, &len, &nt) == 3u && !strcmp(nm, "WAVEBANK") && len == blen && nt == bhdr[6]);
    reboot();
    check("... and after a reboot", smp_slot_info(0, 0, 0) == 3u);
    {   /* the frames where the module reads them: data byte d at flash wvb_flash(WVB_DATA_OFF + d), a frame across a tail included */
        uint32_t d, ok = 1, crossed = 0;
        for (d = 0; d < blen; d++) ok &= nor[wvb_flash(WVB_DATA_OFF + d)] == bdata[d];
        for (uint32_t f = 0; f < blen / WVB_FBYTES; f++) { uint32_t r = WVB_DATA_OFF + f * WVB_FBYTES; crossed += r % WVB_SLOT_SPAN + WVB_FBYTES > WVB_SLOT_SPAN; }
        printf("wave bank slot:   %u frames, %u of them across a sector's tail\n", blen / WVB_FBYTES, crossed);
        check("every data byte where wv_bank.h says it is (the module reads them there)", ok && crossed > 0u);
        check("the header where wv_bank.h says it is", !memcmp(nor + wvb_flash(WVB_HDR_OFF), bhdr, 480));
    }
    check("the sector tails stay erased", tails_erased());
    {   /* the dump for the render test: the flash below the module area, as the module reads it */
        char p[512];
        FILE *f;
        snprintf(p, sizeof p, "%s/wave_bank.nor", dir);
        check("the flash written for tests/wave_bank_render_test.c", (f = fopen(p, "wb")) && fwrite(nor, 1, 0xC2000u, f) == 0xC2000u && !fclose(f));
    }
    {   /* every broken field refused at END; the slot is then empty */
        static const struct { uint32_t at, v, bytes; const char *what; } B[] = {
            {0, 0x4B425658u, 4, "a wrong magic"}, {4, 2, 2, "version 2"}, {6, 0, 1, "no table"}, {6, 17, 1, "17 tables"}, {7, 1, 1, "byte 7 set"},
            {32 + 10, 1, 1, "a table of one frame"}, {32 + 10, 65, 1, "a table of 65 frames"}, {32 + 8, 20, 2, "a table past the data"},
            {32 + 11, 1, 1, "a table's reserved byte"}, {32, 0x01, 1, "a name with a control character"}, {20, 1234, 4, "a wrong CRC"},
        };
        for (uint32_t k = 0; k < sizeof B / sizeof B[0]; k++) {
            uint8_t h[512];
            char what[96];
            memcpy(h, bhdr, 512);
            for (uint32_t i = 0; i < B[k].bytes; i++) h[B[k].at + i] = (uint8_t)(B[k].v >> (8 * i));
            snprintf(what, sizeof what, "refused at END: %s (the slot empty after)", B[k].what);
            check(what, write_bank(h) == -1 && smp_slot_info(0, 0, 0) == SMP_EMPTY);
        }
    }
    {   /* a sample still goes in as before, and the bank goes back in after it */
        make_sample(5000, 3); make_hdr(5000);
        check("a sample still goes in (state 2), then the bank again (state 3)", write_sample(5000, 256) == 0 && smp_slot_info(0, 0, 0) == SMP_FULL &&
              write_bank(bhdr) == 0 && smp_slot_info(0, 0, 0) == 3u);
    }
    printf(bad ? "wave bank slot: FAILED\n" : "wave bank slot: all passed\n");
    return bad != 0;
}
