/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (ui-fix2, close-out 2) recording_restore_note over a slot still in use must leave the track's event chain a chain.
 * It used to link the slot a second time: a cycle, and every walk of the chain (NOTES' jog, the roll, erase) never ended
 * (drum_editor_test hung for 98 minutes). The walks below are bounded, and an alarm ends the test, so the old code FAILS here. */
#define RECORDING_NO_MAIN 1
#include "recording_test.c"
#include <unistd.h>
static uint32_t walk_chain(uint32_t owner, uint32_t *seen_twice)
{
    uint32_t n = 0, mark[RECORD_MAX / 32 + 1] = {0};
    *seen_twice = 0;
    for (uint32_t i = recording_head[owner]; i < RECORD_MAX && n <= RECORD_MAX; i = recording_next[i], n++) {
        if (mark[i / 32] & (1u << (i % 32))) { *seen_twice = 1; break; }
        mark[i / 32] |= 1u << (i % 32);
    }
    return n;
}
int main(void)
{
    int bad = 0;
    alarm(30);                                  /* a hang is a failure, not a stall */
    midi_test_reset(); recording_clear(TSEL);
    uint32_t owner = recording_owner(TSEL), twice;
    recording_restore_note(TSEL, 0, (recorded_note_t){1000, 2000, 36, 90, (uint8_t)owner, 3, 0, 0});
    recording_restore_note(TSEL, 1, (recorded_note_t){3000, 2000, 38, 90, (uint8_t)owner, 3, 0, 0});
    bad += check("two events make a chain of two", walk_chain(owner, &twice) == 2 && !twice);
    recording_restore_note(TSEL, 1, (recorded_note_t){3000, 2000, 40, 80, (uint8_t)owner, 3, 5, 0});
    uint32_t n = walk_chain(owner, &twice);
    bad += check("restoring over a slot in use keeps the chain a chain (no slot twice)", n == 2 && !twice);
    bad += check("the slot holds the restored event", recording[1].note == 40 && recording[1].pitch == 5);
    recording_restore_note(TSEL, 0, (recorded_note_t){1000, 2000, 41, 70, (uint8_t)owner, 3, 0, 0});
    bad += check("restoring over the chain's head too", walk_chain(owner, &twice) == 2 && !twice);
    /* a slot restored into another track's owner leaves its old track's chain */
    uint32_t other = (owner + NPAT) & 31u;
    recording_restore_note(&trk[other / NPAT], 1, (recorded_note_t){3000, 2000, 40, 80, (uint8_t)other, 3, 0, 0});
    uint32_t a = walk_chain(owner, &twice), b = walk_chain(other, &twice);
    bad += check("an event moved to another track leaves the old chain", a == 1 && b == 1 && !twice);
    printf("RESTORE CHAIN: %s\n", bad ? "FAILED" : "all passed"); return bad != 0;
}
