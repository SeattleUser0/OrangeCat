/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 27 review, docs/orangecat/FLASH_CHIPS.md) the 32-byte "LOADER.BIN" head ota.c writes in front of the staged loader is read back like
 * the body: a head the flash did not take (a bit that stays 0, a program that silently fails) fails the stage (-12 in ota_stage), so no
 * update record is written for a loader the boot stub and the SPL could not find. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define MELODEE_ID "ota-FM-1_900"
#define MELODEE_OTA_DRYRUN 0

static uint8_t nor[0x100000];
static int head_fault;                                  /* 1: the head's program flips a bit; 2: it reports success and writes nothing */
static int ota_wire_send(const uint8_t *p, uint32_t n) { (void)p; (void)n; return 0; }
static int ota_frame_get(const uint8_t **p, uint32_t *n) { (void)p; (void)n; return 0; }
static void ota_frame_done(void) {}
static uint32_t ota_now_ms(void) { return 0; }
static void ota_idle(void) {}
static int ota_erase(uint32_t off) { memset(nor + off, 0xFF, 0x1000); return 0; }
static int ota_prog(uint32_t off, const void *p, uint32_t n)
{
    const uint8_t *s = p;
    if (off == 0xE0000u && head_fault == 2) return 0;
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    if (off == 0xE0000u && head_fault == 1) nor[off + 20] &= 0xFEu;
    return 0;
}
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/src/ota.c"

static int check(const char *what, int ok) { printf("ota head: %-80s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }
int main(void)
{
    int bad = 0;
    for (int f = 0; f < 3; f++) {
        memset(nor, 0xFF, sizeof nor);
        head_fault = f;
        int rc = ota_stage_head(0x1000u);
        bad += check(f == 0 ? "a good head: 0, and it reads back as LOADER.BIN" : f == 1 ? "a head with a bit the flash did not take: refused" : "a head program that wrote nothing: refused",
                     f == 0 ? rc == 0 && ota_memeq(nor + 0xE0000u + 16, (const uint8_t *)"LOADER.BIN", 11) : rc != 0);
    }
    printf("%s\n", bad ? "OTA HEAD TEST FAILED" : "ota head test passed");
    return bad != 0;
}
