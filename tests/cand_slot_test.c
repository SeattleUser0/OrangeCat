/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: a saved project across a measurement build (OC_CANDIDATES=1) and back, on a simulated NOR kept in a file:
 *   cand_slot_test save NOR      (a normal build) a project saved in slot 1, its tracks on built-in engines
 *   cand_slot_test cand NOR      (an OC_CANDIDATES=1 build) the boot's store scan, slot 1 loaded (proj_cur 0); the autosave with
 *                                nothing changed (expected: no erase), then with the tracks on candidate engines (a measurement
 *                                left on the unit: written to slot 1)
 *   cand_slot_test read NOR      (a normal build) slot 1 read back
 * The unit's finding (October 26, 2026): slot 1 read empty in the backup after the measurement builds. tests/run_tests.sh builds the
 * file both ways and runs the three in order; each phase prints what it found, and `read` exits 1 when slot 1 does not read.
 *
 * Revisions:
 *   (Orange Cat 0.14.2, linux-hw) first version.
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
static uint32_t erases;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off) { erases++; memset(nor + off, 0xFF, 4096); return 0; }
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
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

/* the music's state cleared, the flash as the file has it, the store read as a boot reads it */
static void boot(void)
{
    memset(&song, 0, sizeof song);
    memset(trk, 0, sizeof trk);
    memset(&chain, 0, sizeof chain);
    chain_defaults(&chain_config);
    pattern_init();
    memset(&motion, 0, sizeof motion);
    memset(proj_slot, 0, sizeof proj_slot);
    memset(proj_bank_slot, 0, sizeof proj_bank_slot);
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
    proj_cur = PROJ_NO_SLOT;
    smp_slot_boot(); ocp_scan(); ocj_scan();
    for (uint32_t i = 0; i < 4u; i++) proj_fetch(i);
}

static void load(const char *f)
{
    FILE *fp = fopen(f, "rb");
    if (!fp || fread(nor, 1, sizeof nor, fp) != sizeof nor) { printf("cand slot: cannot read %s\n", f); exit(2); }
    fclose(fp);
}
static void store(const char *f)
{
    FILE *fp = fopen(f, "wb");
    if (!fp || fwrite(nor, 1, sizeof nor, fp) != sizeof nor) { printf("cand slot: cannot write %s\n", f); exit(2); }
    fclose(fp);
}

static void show(const char *build)
{
    int n = proj_read_slot(0);
    printf("cand slot: %-10s NENGINES %2u: slot 1 %s", build, (unsigned)NENGINES, n ? "reads" : proj_meta[0].old ? "reads as OLD FORMAT (a record it refuses)" : "empty");
    if (n) {
        printf(", CRC %08X, engines", (unsigned)st_crc32(proj_wire_u.raw, (uint32_t)n));
        for (uint32_t k = 0; k < NTRK; k++) printf(" %u", (unsigned)proj_scratch.t[k].engine);
    }
    printf("\n");
}

int main(int argc, char **argv)
{
    if (argc != 3) { printf("cand_slot_test save|cand|read NOR\n"); return 2; }
    if (!strcmp(argv[1], "save")) {
        memset(nor, 0xFF, sizeof nor);
        boot();
        for (uint32_t k = 0; k < NTRK; k++) trk[k].eng_req = (uint8_t)(k == 0 ? 8u : k == 1 ? 10u : k == 2 ? 2u : 7u);
        for (uint32_t i = 0; i < NSTEP; i += 4u) { trk[0].step[i].time = ST_NOTE; trk[0].step[i].n = 1; trk[0].step[i].note[0] = 48; trk[0].step[i].vel = 100; }
        if (project_save(0)) { printf("cand slot: save failed\n"); return 1; }
        show("normal");
        store(argv[2]);
        return 0;
    }
    load(argv[2]);
    boot();
    if (!strcmp(argv[1], "cand")) {
        uint32_t e0, crc;
        int bad = 0;
        show("candidates");
        project_load(0);
        e0 = erases;
        project_autosave();
        printf("cand slot: candidates autosave, nothing changed: %u erases\n", (unsigned)(erases - e0));
        crc = proj_read_slot(0) ? st_crc32(proj_wire_u.raw, BANK_STORE_SIZE) : 0u;
        for (uint32_t k = 0; k < NTRK; k++) trk[k].eng_req = (uint8_t)(NENGINES - 1u);   /* a candidate's number (past 19) */
        e0 = erases;
        project_autosave();
        printf("cand slot: candidates autosave, tracks on engine %u: %u erases\n", (unsigned)(NENGINES - 1u), (unsigned)(erases - e0));
#if OC_CANDIDATES
        bad |= erases != e0;                               /* (0.14.2) never written: proj_candidate_tracks */
        bad |= project_save(0) != 1;                       /* SAVE refused too: TEST ENGINE: NOT SAVED */
        bad |= erases != e0;
#endif
        show("candidates");
        bad |= !proj_read_slot(0) || st_crc32(proj_wire_u.raw, BANK_STORE_SIZE) != crc;
        printf("cand slot: slot 1 as it was after the measurement: %s\n", bad ? "FAIL" : "ok");
        store(argv[2]);
        return bad;
    }
    show("normal");
    return proj_read_slot(0) ? 0 : 1;
}
