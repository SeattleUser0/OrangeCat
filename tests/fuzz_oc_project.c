/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright 2026 Jeremiah Conner.
 *
 * Orange Cat 0.12.2 (Part 2b): a fuzzer of the project record loader (firmware/src/oc_store.c, oc_pool.c, oc_project.c through project_save_as / project_load) on the
 * simulated NOR of tests/persistence_test.c: the record store's 56 sectors (0xA0000.. and 0xEA000..) hold the project records and the pool chunks.
 *   build/host/fuzz_oc_project [seconds] [seed]      default 3 s, seed 20260616; run_tests.sh runs the default, a long run is `fuzz_oc_project 1800`
 * Each round: two projects (slots 0 and 1, a few banks of recognisable steps, motion and timed notes), then 1 to 6 MUTATIONS of the bytes of the store's sectors
 * (a bit flip, a byte set to 00 / FF / random, a field of the 32 byte header or the 256 byte tail, a sector copied onto another, erased, cut at a random length, two
 * sectors swapped), then a power-on (store scan, pool scan, record scan) and a load of each slot.
 * Invariants (a failure prints the seed, the round and the mutations, and stops):
 *   - no crash, no out of bounds access, no undefined behaviour (the sanitizers build, SAN=1 of run_tests.sh, makes them checks);
 *   - a bank a load puts in the sequencer is either exactly as it was saved, or empty (a chunk that fails its CRC is never used, never half used);
 *   - after the load a new project saves into a free slot (or reports PHRASES FULL when the surviving data fills the pool), and it loads back exactly, also after
 *     another power-on;
 *   - a second power-on changes no slot's loaded content (the scan is safe to repeat).
 * Revisions: 0.12.2: first version. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define PERSISTENCE_TEST_NO_MAIN
#include "persistence_test.c"

#define FB 4u                                           /* banks filled in each track */
typedef struct { pattern_t bank[NTRK][FB]; } snap_t;
static snap_t SNAP[3];

static void wipe_runtime(void)
{
    pattern_init();
    memset(&motion, 0, sizeof motion);
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&trk[k].step[i]);
}
static void fill(uint32_t seed)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < FB; b++) {
            pattern_t p;
            pattern_read(k, b, &p);
            for (uint32_t i = 0; i < NSTEP; i++) {
                step_t *s = &p.step[i];
                memset(s, 0, sizeof *s);
                s->time = ST_REST;
                if ((i + b + seed) % 3u == 0u) {
                    s->n = (uint8_t)(1u + (i + seed) % 4u);
                    for (uint32_t j = 0; j < 4u; j++) s->note[j] = (uint8_t)((40u + k * 8u + b + j * 3u + seed) & 127u);
                    s->time = ST_NOTE;
                    s->vel = (uint8_t)(60u + (i * 7u + seed) % 60u);
                    s->flags = (uint8_t)((i + seed) % 2u ? SF_ACCENT : 0u);
                    s->probability = (uint8_t)((i * 13u) % 102u);
                    step_set_rflam(s, 1u + (i + b + seed) % 5u);
                    step_set_cond(s, (i + k + seed) % TC_COUNT);
                    step_set_nudge(s, (int32_t)((i * 5u + b + seed) % 64u) - 32);
                }
            }
            p.timing[0] = (int16_t)(8u + (b * 7u + k + seed) % 57u);
            p.timing[1] = (int16_t)(1u + (b + seed) % 3u);
            p.timing[2] = (int16_t)((b * 11u + seed) % 60u);
            p.timing[3] = (int16_t)(30u + (b + k) % 90u);
            pattern_write(k, b, &p);
            if (b == trk[k].pattern) memcpy(&trk[k].p[P_SLEN], p.timing, sizeof p.timing);
        }
    motion.count = 0;
    for (uint32_t k = 0; k < 2u; k++)
        for (uint32_t b = 0; b < FB; b++) {
            motion_event_t *m = &motion.event[motion.count];
            m->place = (uint8_t)(k << 6 | b);
            m->param = P_DETUNE;
            m->value = (int16_t)(b * 9u + seed % 7u);
            motion_pattern[motion.count++] = (uint8_t)b;
        }
    motion.rsv[0] = 1;
    recording_reset();
    for (uint32_t k = 0, n = 0; k < NTRK; k++)
        for (uint32_t i = 0; i < 6u; i++, n++) {
            recorded_note_t *r = &recording[n];
            r->on = (uint16_t)(i * 1000u + seed); r->duration = (uint16_t)(500u + i);
            r->note = (uint8_t)(50u + i); r->vel = (uint8_t)(70u + i);
            r->owner = (uint8_t)(k * NPAT + 1u); r->step = (uint8_t)(i * 3u); r->pitch = 0; r->length = 0;
        }
    recording_reindex();
}
static void take(snap_t *s)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < FB; b++) pattern_read(k, b, &s->bank[k][b]);
}
static int same_bank(const pattern_t *a, const pattern_t *b) { return !memcmp(a, b, sizeof *a); }
static int empty_bank(const pattern_t *a)
{
    for (uint32_t i = 0; i < NSTEP; i++) if (a->step[i].time == ST_NOTE || a->step[i].n || a->step[i].hit) return 0;
    return 1;
}
static void power_on(void)
{
    ocs_boot(); ocp_scan(); ocj_scan();
    memset(proj_meta, 0, sizeof proj_meta);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(proj_bank_slot, 0, sizeof proj_bank_slot);
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
}
static uint32_t fz;
static uint32_t fr(void) { fz ^= fz << 13; fz ^= fz >> 17; fz ^= fz << 5; return fz; }
static char mlog[8][96];
static int nm;
static void note(const char *fmt, uint32_t a, uint32_t b) { if (nm < 8) snprintf(mlog[nm], sizeof mlog[0], fmt, a, b); nm++; }
static uint32_t sector_at(uint32_t s) { return s < 40u ? 0xA0000u + s * 4096u : 0xEA000u + (s - 40u) * 4096u; }

int main(int argc, char **argv)
{
    double secs = argc > 1 ? atof(argv[1]) : 3.0;
    uint32_t seed = argc > 2 ? (uint32_t)strtoul(argv[2], 0, 0) : 20260616u;
    uint64_t rounds = 0, muts = 0, loaded = 0, refused = 0, saved = 0, full = 0;
    time_t t0 = time(0);
    int fails = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    fz = seed ? seed : 1u;
    for (;;) {
        int errs = 0, step = 0;
        reset();
        wipe_runtime();
        fill(1u + fr() % 5u);
        take(&SNAP[0]);
        if (project_save_as(0, "ONE") != 0) { errs |= 1; goto fail; }
        wipe_runtime();
        fill(10u + fr() % 5u);
        take(&SNAP[1]);
        if (project_save_as(1, "TWO") != 0) { errs |= 2; goto fail; }
        nm = 0;
        {
            uint32_t nmut = 1u + fr() % 6u;
            for (uint32_t i = 0; i < nmut; i++) {
                uint32_t k = fr() % 8u, s = sector_at(fr() % 56u), t = sector_at(fr() % 56u);
                uint32_t off = (fr() & 3u) == 0u ? fr() % 32u : (fr() & 3u) == 0u ? 3840u + fr() % 256u : fr() % 4096u;
                uint8_t *q = nor + s + off;
                switch (k) {
                case 0: *q ^= (uint8_t)(1u << (fr() & 7u)); note("flip sector %x off=%u", s, off); break;
                case 1: *q = 0x00; note("zero sector %x off=%u", s, off); break;
                case 2: *q = 0xFF; note("ff sector %x off=%u", s, off); break;
                case 3: *q = (uint8_t)fr(); note("random sector %x off=%u", s, off); break;
                case 4: memset(nor + s, 0xFF, 4096); note("erase sector %x (%u)", s, 0); break;
                case 5: memcpy(nor + t, nor + s, 4096); note("copy sector %x onto %x", s, t); break;
                case 6: memset(nor + s + off, 0xFF, 4096u - off); note("cut sector %x at %u", s, off); break;
                default: { uint8_t tmp[4096]; memcpy(tmp, nor + s, 4096); memcpy(nor + s, nor + t, 4096); memcpy(nor + t, tmp, 4096); note("swap %x and %x", s, t); break; }
                }
            }
            muts += nmut;
        }
        rounds++;
        step = 1;
        power_on();
        for (uint32_t slot = 0; slot < 2u; slot++) {
            wipe_runtime();
            project_load(slot);
            snap_t got;
            take(&got);
            if (!project_used(slot)) { refused++; continue; }
            loaded++;
            for (uint32_t k = 0; k < NTRK; k++)
                for (uint32_t b = 0; b < FB; b++)
                    if (!same_bank(&got.bank[k][b], &SNAP[slot].bank[k][b]) && !empty_bank(&got.bank[k][b])) { errs |= 4; printf("  slot %u track %u bank %u is neither saved nor empty\n", slot, k, b); goto fail; }
        }
        step = 2;
        {
            wipe_runtime();
            fill(20u + fr() % 5u);
            take(&SNAP[2]);
            int rc = project_save_as(2, "THREE");
            if (rc != 0) {
                if (strcmp(ui.msg, "PHRASES FULL")) { errs |= 8; printf("  save refused with '%s'\n", ui.msg); goto fail; }
                full++;
            } else {
                saved++;
                for (uint32_t again = 0; again < 2u; again++) {
                    power_on();
                    wipe_runtime();
                    project_load(2);
                    snap_t got;
                    take(&got);
                    if (!project_used(2) || memcmp(&got, &SNAP[2], sizeof got)) { errs |= 16; goto fail; }
                }
            }
        }
        step = 3;
        {   /* a second power-on changes nothing a slot loads */
            snap_t a, b;
            power_on(); wipe_runtime(); project_load(0); take(&a);
            power_on(); wipe_runtime(); project_load(0); take(&b);
            if (memcmp(&a, &b, sizeof a)) { errs |= 32; goto fail; }
        }
        if (difftime(time(0), t0) >= secs) break;
        continue;
    fail:
        fails++;
        printf("fuzz_oc_project: FAIL seed %u round %llu step %d errs 0x%x after %d mutations:\n", seed, (unsigned long long)rounds, step, errs, nm);
        for (int i = 0; i < nm && i < 8; i++) printf("    %s\n", mlog[i]);
        if (fails >= 3 || difftime(time(0), t0) >= secs) break;
    }
    printf("fuzz_oc_project: seed %u, %llu rounds, %llu mutations, %llu slots loaded, %llu slots refused, %llu saves after, %llu saves with the pool full: %s\n", seed,
           (unsigned long long)rounds, (unsigned long long)muts, (unsigned long long)loaded, (unsigned long long)refused, (unsigned long long)saved, (unsigned long long)full,
           fails ? "FAIL" : "all passed");
    return fails != 0;
}
