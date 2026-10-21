/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: host test of the project store, firmware/src/oc_project.c (build doc phase 4a, part 3):
 *   build/host/oc_project_test
 * SAVE and load through the record store on a simulated NOR flash, the banks as pool patterns:
 * 1. a project with all 32 banks in use (Skiff's RATCH, FLAM, conditions and NUDGE, timed notes, motion) saves and
 *    loads back the same, after a reboot too; its own record is small.
 * 2. a save that changes nothing rewrites no pattern; changing one bank rewrites its chunk only.
 * 3. a change of RATCH alone is a change: the autosave sees it.
 * 4. two slots keep their own patterns; erasing one empties only its patterns; a save past 64 patterns in all fails
 *    (PATTERNS FULL) and keeps the slot as it was.
 * 5. a Melodee FBKK record handed to a slot (a backup restored) reads back byte for byte.
 * 6. power cut at every program call of a save: after the reboot the slot loads, each bank as before or as saved.
 *
 * Revisions:
 *   0.3.2: first version.
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
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { erases++; memset(nor + off, 0xFF, 4096); return 0; }
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
#include "../firmware/src/project.c"            /* (with MELODEE_FLASH: oc_store.c, oc_pool.c, oc_project.c) */

static int check(const char *what, int ok)
{
    printf("oc project: %-92s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

/* ------------------------------------------------------------ fixtures --- */
static void reboot(void)
{
    ocs_boot(); ocp_scan(); ocj_scan();
    memset(proj_meta, 0, sizeof proj_meta);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(proj_bank_slot, 0, sizeof proj_bank_slot);
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
}

static void reset(void)
{
    memset(nor, 0xFF, sizeof nor);
    ocs_boot(); ocp_scan(); ocj_scan();
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    pattern_init();
    memset(&motion, 0, sizeof motion);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(proj_meta, 0, sizeof proj_meta);
    up_cache_reset(); native_cache_reset();
    memset(&persist_saved, 0, sizeof persist_saved);
    memset(&settings, 0, sizeof settings);
    memset(&ui, 0, sizeof ui);
    panel = PANEL_DEFAULT;
    settings_init();
    host_tracks_init();
    fm1_ms = 0;
    transport_req = 0;
    erases = 0;
    proj_cur = PROJ_NO_SLOT;
}

/* every bank of every track filled (seed: which notes), with Skiff's fields, motion and timed notes */
static void fill_all(uint32_t seed, uint32_t banks)
{
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t b = 0; b < banks; b++) {
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
    /* motion: two events on each of the first banks of tracks 1 and 2 */
    motion.count = 0;
    for (uint32_t k = 0; k < 2u; k++)
        for (uint32_t b = 0; b < 4u && b < banks; b++)
            for (uint32_t e = 0; e < 2u; e++) {
                motion_event_t *m = &motion.event[motion.count];
                m->place = (uint8_t)(k << 6 | (e * 5u + b));
                m->param = e ? P_PAN : P_DETUNE;
                m->value = (int16_t)(b * 9u + e + seed % 7u);
                motion_pattern[motion.count++] = (uint8_t)b;
            }
    motion.rsv[0] = 1;
    /* timed notes: ten on each of tracks 1..4's bank 1 */
    recording_reset();
    for (uint32_t k = 0, n = 0; k < NTRK && banks > 1u; k++)
        for (uint32_t i = 0; i < 10u; i++, n++) {
            recorded_note_t *r = &recording[n];
            r->on = (uint16_t)(i * 1000u + seed); r->duration = (uint16_t)(500u + i);
            r->note = (uint8_t)(50u + i); r->vel = (uint8_t)(70u + i);
            r->owner = (uint8_t)(k * NPAT + 1u); r->step = (uint8_t)(i * 3u); r->pitch = 0; r->length = 0;
        }
    recording_reindex();
}

/* the sequencer's every bank, motion and timed notes, to compare */
typedef struct { pattern_t bank[NTRK][NPAT]; uint8_t pat[NTRK]; motion_store_t mot; uint8_t tag[MOTION_MAX];
                 recorded_note_t rec[RECORD_MAX]; } snap_t;
static int rec_cmp(const void *a, const void *b) { return memcmp(a, b, sizeof(recorded_note_t)); }
static void snap(snap_t *s)
{
    memset(s, 0, sizeof *s);
    for (uint32_t k = 0; k < NTRK; k++) {
        s->pat[k] = trk[k].pattern;
        for (uint32_t b = 0; b < NPAT; b++) pattern_read(k, b, &s->bank[k][b]);
    }
    s->mot.count = motion.count;
    for (uint32_t i = 0; i < motion.count; i++) {   /* (as a set: event + tag, in order of both) */
        s->mot.event[i] = motion.event[i];
        s->tag[i] = motion_pattern[i];
    }
    for (uint32_t i = 0, n = 0; i < RECORD_MAX; i++) if (recording[i].vel) s->rec[n++] = recording_snapshot(i);
    qsort(s->rec, RECORD_MAX, sizeof s->rec[0], rec_cmp);
}
static int mot_same(const snap_t *a, const snap_t *b)
{
    if (a->mot.count != b->mot.count) return 0;
    for (uint32_t i = 0; i < a->mot.count; i++) {
        uint32_t found = 0;
        for (uint32_t j = 0; j < b->mot.count && !found; j++)
            found = !memcmp(&a->mot.event[i], &b->mot.event[j], sizeof a->mot.event[i]) && a->tag[i] == b->tag[j];
        if (!found) return 0;
    }
    return 1;
}
static int same(const snap_t *a, const snap_t *b)
{
    return !memcmp(a->bank, b->bank, sizeof a->bank) && !memcmp(a->pat, b->pat, sizeof a->pat) && mot_same(a, b) &&
           !memcmp(a->rec, b->rec, sizeof a->rec);
}
static void wipe_runtime(void)
{
    pattern_init();
    memset(&motion, 0, sizeof motion);
    for (uint32_t k = 0; k < NTRK; k++)
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&trk[k].step[i]);
}

static snap_t A, B, C;
static uint32_t used_patterns(void)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < OCP_N; i++) n += ocp.len[i] != 0;
    return n;
}

/* ------------------------------------------------------------- 1, 2, 3 --- */
static int test_round_trip(void)
{
    int bad = 0;
    ocs_rec_t r;
    uint32_t before;
    reset();
    fill_all(1, NPAT);
    trk[2].pattern = 0;
    snap(&A);
    bad |= check("a project with all 32 banks in use saves", project_save_as(0, "ALL BANKS") == 0 && !strcmp(ui.msg, "SAVED"));
    bad |= check("... its banks are 32 pool patterns", used_patterns() == 32u);
    bad |= check("... its own record is one sector", ocs_open(OCS_PROJECT, 0, &r) == 0 && r.total <= OCS_PAYLOAD_MAX);
    wipe_runtime();
    project_load(0);
    snap(&B);
    bad |= check("... loads back the same: every bank, Skiff's fields, motion, timed notes", same(&A, &B));
    reboot();
    wipe_runtime();
    {
        char n[16];
        bad |= check("after a reboot the slot list has its name", project_name(0, n) && !strcmp(n, "ALL BANKS"));
    }
    project_load(0);
    snap(&B);
    bad |= check("... and it loads the same", same(&A, &B));
    before = erases;
    bad |= check("a save that changes nothing rewrites no pattern (the record only)",
                 project_save(0) == 0 && erases - before == 1u);
    {
        pattern_t p;
        pattern_read(3, 5, &p);
        p.step[1].vel ^= 1u;
        pattern_write(3, 5, &p);
    }
    before = erases;
    bad |= check("changing one bank rewrites its chunk and the record", project_save(0) == 0 && erases - before <= 1u + 3u &&
                 erases - before >= 2u);
    snap(&A);
    {   /* RATCH alone */
        pattern_t p;
        pattern_read(1, 2, &p);
        uint32_t i = 0;
        while (p.step[i].time != ST_NOTE) i++;
        step_set_ratchet(&p.step[i], step_ratchet(&p.step[i]) % 4u + 1u);
        pattern_write(1, 2, &p);
        snap(&A);
        before = erases;
        project_autosave();
        wipe_runtime();
        project_load(0);
        snap(&B);
        bad |= check("a change of RATCH alone: the autosave writes it, it loads back", erases > before && same(&A, &B));
        before = erases;
        project_autosave();
        bad |= check("... and an autosave after that writes nothing", erases == before);
    }
    return bad;
}

/* ----------------------------------------------------------------- 4 --- */
static int test_slots(void)
{
    int bad = 0;
    char n[16];
    reset();
    fill_all(2, 4);
    snap(&A);
    project_save_as(0, "ONE");
    fill_all(3, 4);
    snap(&B);
    project_save_as(1, "TWO");
    bad |= check("two slots of 16 banks: 32 patterns", used_patterns() == 32u);
    wipe_runtime(); project_load(0); snap(&C);
    bad |= check("... slot A loads its own", same(&A, &C));
    wipe_runtime(); project_load(1); snap(&C);
    bad |= check("... slot B its own", same(&B, &C));
    bad |= check("erasing slot A empties only its patterns", project_erase(0) == 0 && used_patterns() == 16u &&
                 !project_name(0, n) && (wipe_runtime(), project_load(1), snap(&C), same(&B, &C)));
    fill_all(4, NPAT);
    snap(&A);
    bad |= check("a 32-bank project into slot A: 48 patterns in all", project_save_as(0, "BIG") == 0 && used_patterns() == 48u);
    fill_all(5, NPAT);
    bad |= check("another into slot C: past 64, refused (PATTERNS FULL), nothing changes",
                 project_save_as(2, "TOO BIG") != 0 && used_patterns() == 48u && !project_name(2, n));
    reboot();
    wipe_runtime(); project_load(0); snap(&C);
    bad |= check("... after a reboot slot A as saved", same(&A, &C));
    wipe_runtime(); project_load(1); snap(&C);
    bad |= check("... and slot B", same(&B, &C));
    wipe_runtime(); project_load(0);
    bad |= check("saving slot A over itself reuses its own patterns (no new ones)",
                 project_save(0) == 0 && used_patterns() == 48u);
    return bad;
}

/* ----------------------------------------------------------------- 5 --- */
static int test_fbkk(void)
{
    static uint8_t fbkk[BANK_STORE_SIZE];
    reset();
    fill_all(6, NPAT);
    project_capture(&proj_scratch);
    bank_pack(fbkk, &proj_scratch, 1);
    proj_x_tag = 0;                                /* (from elsewhere: without Skiff's fields) */
    memcpy(proj_wire_u.raw, fbkk, sizeof fbkk);
    int ok = proj_write_slot(3, proj_wire_u.raw, BANK_STORE_SIZE) == 0;
    reboot();
    ok &= proj_read_slot(3) == (int)BANK_STORE_SIZE;
    return check("a Melodee FBKK record into slot D reads back byte for byte", ok && !memcmp(proj_wire_u.raw, fbkk, sizeof fbkk));
}

/* ----------------------------------------------------------------- 6 --- */
static int test_cuts(void)
{
    int ok = 1, done = 0, n = 0;
    for (int k = 0; !done && k < 4000; k++, n++) {
        reset();
        fill_all(7, NPAT);
        snap(&A);
        project_save_as(0, "OLD");
        fill_all(8, NPAT);
        snap(&B);
        fail_after = k;
        done = project_save(0) == 0;
        fail_after = -1;
        reboot();
        wipe_runtime();
        project_load(0);
        snap(&C);
        for (uint32_t t = 0; t < NTRK; t++)
            for (uint32_t b = 0; b < NPAT; b++)
                ok &= !memcmp(&C.bank[t][b], &A.bank[t][b], sizeof C.bank[t][b]) ||
                      !memcmp(&C.bank[t][b], &B.bank[t][b], sizeof C.bank[t][b]);
        if (done) ok &= same(&B, &C);
        /* and the next save works */
        fill_all(9, NPAT);
        snap(&A);
        ok &= project_save(0) == 0;
        reboot(); wipe_runtime(); project_load(0); snap(&C);
        ok &= same(&A, &C);
    }
    char what[120];
    snprintf(what, sizeof what, "power cut at %d points of a save: each bank as before or as saved; the next save works", n);
    return check(what, ok && done);
}

int main(void)
{
    int bad = 0;
    bad |= test_round_trip();
    bad |= test_slots();
    bad |= test_fbkk();
    bad |= test_cuts();
    printf(bad ? "oc project: FAILED\n" : "oc project: all passed\n");
    return bad ? 1 : 0;
}
