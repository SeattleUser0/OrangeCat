/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* Item 11 of tests/pj4_test.c, the Release 2 half: the OLD record store code (OLD_DIR/oc_store.c and storage.c from the release-2 branch, tests/pj4_old_store.sh)
 * boots on a converted store's NOR image, looks for the records it knows (old project ids 0..3, old pool chunks 0..15), finds none, saves an old-format project
 * and pool chunk, and writes the NOR image back. It must neither erase nor read the new records.  pj4_old_store IN OUT */
#define MELODEE_FLASH 1
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t nor[0x100000];
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n) { for (uint32_t i = 0; i < n; i++) nor[off + i] &= ((const uint8_t *)src)[i]; return 0; }
#define STR2(x) #x
#define STR(x) STR2(x)
#include STR(OLD_DIR/storage.c)
#include STR(OLD_DIR/oc_store.c)
int main(int argc, char **argv)
{
    FILE *fp;
    uint8_t buf[4000];
    uint32_t i, seen = 0;
    if (argc != 3 || !(fp = fopen(argv[1], "rb")) || fread(nor, 1, sizeof nor, fp) != sizeof nor) return 2;
    fclose(fp);
    ocs_boot();
    for (i = 0; i < 4; i++) seen += ocs_exists(OCS_PROJECT, i);
    for (i = 0; i < 16; i++) seen += ocs_exists(OCS_POOL, i);
    printf("old store: %u old records found on the converted store (want 0)\n", seen);
    if (seen) return 1;
    for (i = 0; i < sizeof buf; i++) buf[i] = (uint8_t)(i * 7u);
    if (ocs_write(OCS_PROJECT, 0, buf, sizeof buf) || ocs_write(OCS_POOL, 0, buf, sizeof buf)) { printf("old store: its own save failed\n"); return 1; }
    for (i = 0; i < 6; i++) if (ocs_write(OCS_PROJECT, 0, buf, sizeof buf)) { printf("old store: a further save failed\n"); return 1; }
    if (!(fp = fopen(argv[2], "wb")) || fwrite(nor, 1, sizeof nor, fp) != sizeof nor) return 2;
    fclose(fp);
    printf("old store: saved beside the converted records\n");
    return 0;
}
