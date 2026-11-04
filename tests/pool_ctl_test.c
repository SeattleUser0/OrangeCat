/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* The pattern pool and the control-note flag (Release 3 step 7, 8.2b): a bank holding a flagged control note is saved as a version 3 pattern and loads back with the
 * flag; a bank without one is still version 2 (a Release 2 reader keeps reading it); a version 2 pattern that holds a flagged note is refused at load. */
#define OC_POOL_NO_MAIN
#include "oc_pool_test.c"

static int cbad;
static int cc(const char *what, int ok) { printf("pool ctl: %-110s %s\n", what, ok ? "ok" : "FAIL"); cbad += !ok; return !ok; }
int main(void)
{
    static uint8_t rec[OCS_PARTS_MAX * OCS_PAYLOAD_MAX];
    uint32_t i, owner = 0 * NPAT + 2, flagged = RECORD_MAX;
    int n;
    fresh();
    fill(0, 2, 24, 40, 6, 30, 1);
    cc("an ordinary bank saves as version 2", ocp_save(6, 0, 2) == 0 && (n = ocs_read(OCS_POOL, 1, rec, sizeof rec)) > 0 && rec[8 + (rec[0] | rec[1] << 8) + (rec[2] | rec[3] << 8)] != 3u);
    for (i = 0; i < RECORD_MAX && flagged == RECORD_MAX; i++) if (recording[i].vel && (recording[i].owner & 31u) == owner) flagged = i;
    cc("the fixture has timed notes to flag", flagged < RECORD_MAX);
    recording[flagged].note = 126; recording[flagged].pitch = 0; recording[flagged].length = RN_CTL; recording[flagged].vel = 100;
    cc("the flagged control note is a valid note", recording_valid(&recording[flagged]));
    cc("a bank with a flagged note saves", ocp_save(7, 0, 2) == 0);
    n = ocs_read(OCS_POOL, 1, rec, sizeof rec);
    cc("... as a version 3 pattern", n > 0 && ({ uint32_t off = 8 + (rec[0] | rec[1] << 8) + (rec[2] | rec[3] << 8) + (rec[4] | rec[5] << 8); rec[off] == 3u; }));
    cc("... and it loads into another bank with the flag, the number and the length intact", ocp_load(7, 1, 3, 1) == 0 && ({ int f = 0; for (i = 0; i < RECORD_MAX; i++) f |= recording[i].vel && (recording[i].owner & 31u) == 1u * NPAT + 3u && recording[i].length == RN_CTL && recording[i].note == 126; f; }));
    {   /* the same bytes as a version 2 pattern: refused */
        uint32_t off = 8 + (rec[0] | rec[1] << 8) + (rec[2] | rec[3] << 8) + (rec[4] | rec[5] << 8);
        rec[off] = 2u;
        cc("a version 2 pattern that holds a flagged note is refused at load", ocs_write(OCS_POOL, 1, rec, (uint32_t)n) == 0 && ocp_load(7, 1, 0, 1) != 0);
    }
    printf("pool ctl: %s\n", cbad ? "FAILED" : "all checks passed");
    return cbad != 0;
}
