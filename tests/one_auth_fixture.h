/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner. */
/* (ui-fix2 close-out) With OC_ONE_AUTH (ui-fix 7, in the build set) a timed note is an event AND a cell of its step flagged SF_RECORDED, and every load mends what
 * is only one of the two (recording.c recording_reconcile: "present" is a NOTE cell with the flag). A test that fills the event store by hand (random owners, steps)
 * calls this after recording_reindex(): every event gets its cell, so what is saved is what a load gives back. Harmless without the switch. */
#ifndef ONE_AUTH_FIXTURE_H
#define ONE_AUTH_FIXTURE_H
static void fixture_cells_for_events(void)
{
    uint32_t i;
    for (i = 0; i < RECORD_MAX; i++) {
        uint32_t owner, k, b, view;
        pattern_t p;
        if (!recording[i].vel) continue;
        owner = recording[i].owner & 31u; k = owner / NPAT; b = owner % NPAT;
        if (k >= NTRK) continue;
        view = recording_view(&trk[k], &recording[i]);
        if (view >= NSTEP) continue;
        pattern_read(k, b, &p);
        p.step[view].time = ST_NOTE;
        p.step[view].flags |= SF_RECORDED;
        pattern_write(k, b, &p);
    }
}
#endif
