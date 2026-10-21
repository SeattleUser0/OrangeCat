/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: host test of the pattern pool, firmware/src/oc_pool.c (build doc phase 4a, part 3):
 *   build/host/oc_pool_test
 * On a simulated NOR flash (oc_store_test.c's), with the sequencer's banks, motion and timed notes:
 * 1. a bank saved to the pool and loaded into another track's bank comes back the same: every step field (RATCH,
 *    FLAM, conditions, NUDGE too), LEN DIV SWING GATE, its motion and its timed notes (on the new track and bank).
 * 2. after a reboot the PATTERNS page's lengths are back and the pattern loads the same, into the bank playing too.
 * 3. four patterns share a chunk: saving one keeps the others. An emptied pattern loads as an empty bank (its notes
 *    and motion gone); a chunk with nothing left is no record.
 * 4. the fullest pattern (64 steps, 64 motion events, 1024 timed notes) round trips.
 * 5. no room for its timed notes or motion: the load refuses and nothing changes.
 * 6. power cut at every program call of a save: after the reboot the pattern is the old one or the new one, and its
 *    neighbours in the chunk are untouched.
 *
 * Revisions:
 *   0.3.1: first version.
 */
static unsigned char host_samples[3][0x14000];
#define SMP_USER_XIP(k) host_samples[k]
#define main hostsim_main
#include "hostsim.c"
#undef main

static int32_t fm1_enc_take(uint32_t e) { (void)e; return 0; }
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}
static void lcd_sync(void) {}
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c) { (void)x; (void)y; (void)w; (void)h; (void)c; }
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#define MELODEE_FLASH 1
#include "../firmware/src/gfx.c"
#include "../firmware/src/panel.c"
#include "../firmware/src/ui.c"
static void panel_setup(void) {}

static uint8_t nor[0x100000], flash_ok = 1;
static int fail_after = -1;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (fail_after == 0) return -9;
    if (fail_after > 0) fail_after--;
    if ((off & 0xFFu) + n > 256u) { printf("page wrap at %#x\n", off); exit(1); }
    for (uint32_t i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0; }
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../firmware/src/storage.c"
#include "../firmware/src/upreset.c"
#include "../firmware/src/project.c"
#include "../firmware/src/oc_store.c"
#include "../firmware/src/oc_pool.c"

static int check(const char *what, int ok)
{
    printf("oc pool: %-92s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------ fixtures --- */
static void boot(void) { ocs_boot(); ocp_scan(); }

static void fresh(void)
{
    memset(nor, 0xFF, sizeof nor);
    host_tracks_init();
    memset(&motion, 0, sizeof motion);
    boot();
}

/* track k's bank filled: steps with every field, LEN len, motion events nmot, timed notes nnotes */
static void fill(uint32_t k, uint32_t bank, uint32_t len, uint32_t nsteps, uint32_t nmot, uint32_t nnotes, uint32_t seed)
{
    pattern_t p;
    uint32_t i, owner = k * NPAT + bank;
    srand(seed);
    pattern_read(k, bank, &p);
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &p.step[i];
        memset(s, 0, sizeof *s);
        s->time = ST_REST;
        if (i < nsteps && (nsteps == NSTEP || rand() % 3)) {
            s->n = (uint8_t)(1 + rand() % 4);
            for (uint32_t j = 0; j < 4u; j++) s->note[j] = (uint8_t)(rand() & 127);
            s->time = (uint8_t)(rand() % 2 ? ST_NOTE : ST_TIE);
            s->flags = (uint8_t)(rand() & (SF_ACCENT | SF_SLIDE | SF_RECORDED));
            s->vel = (uint8_t)(rand() & 127);
            s->probability = (uint8_t)(rand() % 102);
            step_set_rflam(s, 1u + (uint32_t)rand() % 5u);
            step_set_cond(s, (uint32_t)rand() % TC_COUNT);
            step_set_nudge(s, NUDGE_MIN + rand() % (NUDGE_MAX - NUDGE_MIN + 1));
        }
    }
    p.timing[0] = (int16_t)len;
    p.timing[1] = (int16_t)(1 + seed % 3u);
    p.timing[2] = (int16_t)(seed % 50u);
    p.timing[3] = (int16_t)(20 + seed % 90u);
    pattern_write(k, bank, &p);
    for (i = 0; i < motion.count;)              /* the bank's motion replaced */
        if ((motion.event[i].place >> 6) == k && motion_pattern[i] == bank) {
            memmove(&motion.event[i], &motion.event[i + 1], (motion.count - i - 1u) * sizeof motion.event[0]);
            memmove(&motion_pattern[i], &motion_pattern[i + 1], motion.count - i - 1u);
            motion.count--;
        } else i++;
    for (i = 0; i < nmot && motion.count < MOTION_MAX; i++) {
        motion_event_t *e = &motion.event[motion.count];
        e->place = (uint8_t)(k << 6 | (i % NSTEP));
        e->param = i & 1u ? P_PAN : P_DETUNE;
        e->value = (int16_t)(-64 + rand() % 192);
        motion_pattern[motion.count++] = (uint8_t)bank;
    }
    motion.rsv[0] = 1;
    recording_drop(&trk[k], owner);
    for (i = 0; i < nnotes; i++) {
        uint32_t j = 0;
        while (j < RECORD_MAX && recording[j].vel) j++;
        if (j == RECORD_MAX) break;
        recorded_note_t *r = &recording[j];
        r->on = (uint16_t)rand(); r->duration = (uint16_t)(1 + rand() % 60000);
        r->note = (uint8_t)(rand() & 127); r->vel = (uint8_t)(1 + rand() % 127);
        r->owner = (uint8_t)(owner | (uint32_t)(rand() % 8) << 5);
        r->step = (uint8_t)(rand() % NSTEP); r->pitch = (int8_t)(rand() % 49 - 24); r->length = (uint8_t)(rand() % 2);
    }
    recording_reindex();
}

/* a bank's motion and timed notes, put in order to compare (track and bank bits dropped) */
typedef struct { uint32_t nmot, nnotes; motion_event_t mot[MOTION_MAX]; recorded_note_t note[RECORD_MAX]; } extra_t;
static int note_cmp(const void *a, const void *b) { return memcmp(a, b, sizeof(recorded_note_t)); }
static int mot_cmp(const void *a, const void *b) { return memcmp(a, b, sizeof(motion_event_t)); }
static void extra_of(uint32_t k, uint32_t bank, extra_t *x)
{
    memset(x, 0, sizeof *x);
    for (uint32_t i = 0; i < motion.count; i++)
        if ((motion.event[i].place >> 6) == k && motion_pattern[i] == bank) {
            x->mot[x->nmot] = motion.event[i];
            x->mot[x->nmot++].place &= 63u;
        }
    for (uint32_t i = 0; i < RECORD_MAX; i++)
        if (recording[i].vel && (recording[i].owner & 31u) == k * NPAT + bank) {
            x->note[x->nnotes] = recording[i];
            x->note[x->nnotes++].owner &= 224u;
        }
    qsort(x->mot, x->nmot, sizeof x->mot[0], mot_cmp);
    qsort(x->note, x->nnotes, sizeof x->note[0], note_cmp);
}

static pattern_t want_p;
static extra_t want_x, got_x;
static void remember(uint32_t k, uint32_t bank) { pattern_read(k, bank, &want_p); extra_of(k, bank, &want_x); }
static int same_as_remembered(uint32_t k, uint32_t bank)
{
    pattern_t p;
    pattern_read(k, bank, &p);
    extra_of(k, bank, &got_x);
    return !memcmp(&p, &want_p, sizeof p) && got_x.nmot == want_x.nmot && got_x.nnotes == want_x.nnotes &&
           !memcmp(got_x.mot, want_x.mot, want_x.nmot * sizeof want_x.mot[0]) &&
           !memcmp(got_x.note, want_x.note, want_x.nnotes * sizeof want_x.note[0]);
}

/* ------------------------------------------------------------------ 1-3 --- */
static int test_round_trip(void)
{
    int bad = 0;
    pattern_t p;
    extra_t x;
    fresh();
    fill(0, 2, 24, 40, 6, 30, 1);
    remember(0, 2);
    bad |= check("an empty pool: every pattern empty", !ocp.len[0] && !ocp.len[5] && !ocp.len[63]);
    bad |= check("track 1's bank 3 saved as pool pattern 6: LEN 24 on the page",
                 ocp_save(6, 0, 2) == 0 && ocp.len[6] == 24u && !ocp.len[5] && !ocp.len[7]);
    bad |= check("... loads into track 2's bank 4 the same: steps, timing, motion, timed notes",
                 ocp_load(6, 1, 3, 1) == 0 && same_as_remembered(1, 3) && want_x.nmot == 6u && want_x.nnotes == 30u);
    bad |= check("... and track 1's bank is as it was", same_as_remembered(0, 2));
    boot();
    bad |= check("after a reboot the page has LEN 24 for pattern 7, nothing else", ocp.len[6] == 24u && !ocp.len[4]);
    bad |= check("... it loads into track 3's bank playing, which then plays it",
                 ocp_load(6, 2, 0, 1) == 0 && same_as_remembered(2, 0) && !memcmp(trk[2].step, want_p.step, sizeof want_p.step) &&
                     trk[2].p[P_SLEN] == 24);
    /* neighbours in the chunk */
    fill(3, 5, 12, 64, 2, 5, 2);
    bad |= check("pool pattern 5 (the same chunk) saved; pattern 7 still loads the same",
                 ocp_save(4, 3, 5) == 0 && ocp.len[4] == 12u && ocp_load(6, 1, 1, 1) == 0 && same_as_remembered(1, 1));
    remember(3, 5);
    bad |= check("... and pattern 5 loads as saved", ocp_load(4, 0, 7, 1) == 0 && same_as_remembered(0, 7));
    /* emptied */
    pattern_read(3, 6, &p);
    extra_of(3, 6, &x);
    bad |= check("an empty bank saved over pattern 7 empties it", x.nmot == 0 && x.nnotes == 0 && ocp_save(6, 3, 6) == 0 && !ocp.len[6]);
    bad |= check("... loading it empties the bank: steps, motion and timed notes gone, timing to defaults",
                 ocp_load(6, 1, 3, 1) == 0 && (extra_of(1, 3, &x), x.nmot == 0 && x.nnotes == 0) &&
                     (pattern_read(1, 3, &p), !ocp_step_used(&p.step[0]) && !ocp_step_used(&p.step[63]) &&
                                                   p.timing[0] == TP[P_SLEN].def));
    bad |= check("a chunk with nothing left is no record",
                 ocs_exists(OCS_POOL, 1) && ocp_save(4, 3, 6) == 0 && !ocs_exists(OCS_POOL, 1) && !ocp.len[4]);
    bad |= check("an id past 63, a track past 4, a bank past 8: refused",
                 ocp_save(64, 0, 0) && ocp_load(64, 0, 0, 1) && ocp_save(0, NTRK, 0) && ocp_load(0, 0, NPAT, 1));
    return bad;
}

/* --------------------------------------------------------------- 4, 5 --- */
static int test_full(void)
{
    int bad = 0;
    pattern_t p;
    fresh();
    fill(1, 6, 64, 64, 64, RECORD_MAX, 3);
    remember(1, 6);
    bad |= check("the fullest pattern: 64 steps, 64 motion events, 1024 timed notes",
                 want_x.nmot == 64u && want_x.nnotes == RECORD_MAX && ocp_save(63, 1, 6) == 0 && ocp.len[63] == 64u);
    pattern_read(1, 6, &p);
    for (uint32_t i = 0; i < NSTEP; i++) step_clear(&p.step[i]);
    pattern_write(1, 6, &p);
    recording_drop(&trk[1], 1u * NPAT + 6u);
    recording_reindex();
    motion.count = 0;
    boot();
    bad |= check("... loads back the same after the bank is cleared and a reboot", ocp_load(63, 1, 6, 1) == 0 && same_as_remembered(1, 6));
    /* no room: the store is full of track 2's notes, track 4 asks for 1024 more */
    bad |= check("no room for its timed notes: refused, nothing changes",
                 (pattern_read(3, 2, &p), ocp_load(63, 3, 2, 1) == 2) && (extra_of(3, 2, &got_x), got_x.nnotes == 0 && got_x.nmot == 0) &&
                     same_as_remembered(1, 6));
    fresh();
    fill(0, 1, 16, 4, 40, 0, 4);
    ocp_save(9, 0, 1);
    fill(2, 1, 16, 4, 24, 0, 5);
    bad |= check("no room for its motion (40 events, the store full at 64): refused", ocp_load(9, 1, 1, 1) == 2 && motion.count == 64u);
    bad |= check("... a check without apply answers the same, and room where its own bank's events go",
                 ocp_load(9, 1, 1, 0) == 2 && ocp_load(9, 0, 1, 0) == 0);
    return bad;
}

/* -------------------------------------------------------------- 6. cuts --- */
static int test_cuts(void)
{
    int ok = 1, done = 0, n = 0;
    pattern_t keep4, keep7;
    extra_t x4, x7;
    for (int k = 0; !done && k < 200; k++, n++) {
        int rc;
        fresh();
        fill(0, 1, 20, 50, 5, 20, 10);          /* pattern 4: a neighbour */
        ocp_save(4, 0, 1);
        remember(0, 1); keep4 = want_p; x4 = want_x;
        fill(0, 2, 30, 50, 5, 20, 11);          /* pattern 7: old */
        ocp_save(7, 0, 2);
        remember(0, 2); keep7 = want_p; x7 = want_x;
        fill(0, 3, 40, 60, 8, 40, 12);          /* pattern 7: new */
        fail_after = k;
        rc = ocp_save(7, 0, 3);
        fail_after = -1;
        done = rc == 0;
        boot();
        want_p = keep4; want_x = x4;
        ok &= ocp_load(4, 1, 1, 1) == 0 && same_as_remembered(1, 1);
        ocp_load(7, 1, 2, 1);
        want_p = keep7; want_x = x7;
        if (!same_as_remembered(1, 2)) {
            remember(0, 3);
            ok &= same_as_remembered(1, 2) && ocp.len[7] == 40u;
        } else {
            ok &= !done && ocp.len[7] == 30u;
        }
    }
    char what[120];
    snprintf(what, sizeof what, "power cut after each of %d program calls of a save: old or new, neighbour untouched", n);
    return check(what, ok && done);
}

int main(void)
{
    int bad = 0;
    bad |= test_round_trip();
    bad |= test_full();
    bad |= test_cuts();
    printf(bad ? "oc pool: FAILED\n" : "oc pool: all passed\n");
    return bad ? 1 : 0;
}
