/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 27, docs/orangecat/FLASH_CHIPS.md) the RAM flash driver itself (firmware/hal/fm1_flash.h) against a simulated part at the register
 * level: SPI0's buffer and pending bit, the chip select on PD0, the SFC's control register (XIP on / off), TIMER4, the P33 watchdog port.
 * The part answers 0x9F with its id, 0x05 with WIP / WEL, takes 0x06, 0x20 and 0x02 and stays busy for the time the test sets.
 *   - the write gate: an unknown id is never erased or programmed (no 0x20 / 0x02 reaches the part); the two known ids are
 *   - XIP is never switched back on while WIP is set: a part slower than the driver's limit (500 ms erase, 50 ms program) is waited out,
 *     the watchdog fed, and the call reports -4 (or -5 past 2 s); a normal part returns 0
 *   - (review) the gate closes when the id read cannot run (the SFC in a mode the driver refuses), whatever an earlier read found
 *   - (review) storage_hw.c's st_erase / st_prog refuse with flash_ok 0 even when the driver's gate is open, and the driver refuses
 *     with its gate closed even when flash_ok is 1: each gate alone keeps an unknown part unwritten
 *   - (review) the FLASH_ID report (fl_report) sends 0x05, 0x9F, 0x35, 0x5A and nothing else: no 0x15
 * Each register access goes through fl_reg(), which first applies the access before it (a write is seen at the next access). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static volatile uint32_t *fl_reg(uint32_t a);
#define REG32(a) (*fl_reg(a))
#define FL_HOST_SIM 1
#include "../firmware/hal/fm1_flash.h"
static int16_t abuf[64];                               /* (storage_hw.c's audio_silence clears it) */
#include "../firmware/src/storage_hw.c"                /* the real store side: its flash_ok gate above the driver's gate */
#ifndef ST_FLASH_SLOW
#define ST_FLASH_SLOW (-20)                            /* (a tree before the re-review has none: the checks below then fail on behaviour) */
#endif

#define SENT 0xA5A5A500u                               /* a register slot the driver has not written since: high bits set */
static volatile uint32_t r_spi_con, r_spi_buf = SENT | 0xFFu, r_portd = 1, r_portd_dir, r_iomap = 0x20, r_sfc = 0x200, r_cache = 0x4000, r_t4, r_p33_con, r_p33_dat = SENT, r_other;
static uint32_t cs_shadow = 1, sfc_shadow = 0x200, now;   /* now: simulated TIMER4 ticks (24 MHz) */
static struct { uint32_t id; uint8_t wel; uint64_t busy_until; uint32_t cmd, k, erases, progs, ops_dur; uint8_t seen[256]; } chip;
static uint32_t xip_on_busy, feeds, feed_seq, feed_gap_max, last_feed;
static int busy(void) { return (uint64_t)now < chip.busy_until; }

static uint32_t spi_byte(uint32_t b)                   /* one byte on the bus while CS is low: the part's answer */
{
    uint32_t k = chip.k++;
    if (k == 0) { chip.cmd = b; chip.seen[b & 255u] = 1; if (b == 0x06u && !busy()) chip.wel = 1; return 0xFFu; }
    switch (chip.cmd) {
    case 0x9Fu: return k == 1 ? chip.id >> 16 & 255u : k == 2 ? chip.id >> 8 & 255u : chip.id & 255u;
    case 0x05u: return (busy() ? 1u : 0u) | (chip.wel ? 2u : 0u);
    case 0x35u: case 0x15u: return 0u;
    default: return 0xFFu;
    }
}
static void cs_edge(uint32_t high)
{
    if (!high) { chip.k = 0; chip.cmd = 0; return; }
    if (chip.k && (chip.cmd == 0x20u || chip.cmd == 0x02u) && chip.wel && !busy()) {   /* the command ends: an erase or a program starts */
        chip.wel = 0;
        if (chip.cmd == 0x20u) chip.erases++; else chip.progs++;
        chip.busy_until = (uint64_t)now + (uint64_t)chip.ops_dur * 24u;
    }
}
static void commit(void)                               /* the access before this one, applied */
{
    if ((r_portd & 1u) != cs_shadow) { cs_shadow = r_portd & 1u; cs_edge(cs_shadow); }
    if (!(r_spi_buf & 0xFFFFFF00u)) { uint32_t v = spi_byte(r_spi_buf & 255u); r_spi_buf = SENT | v; r_spi_con |= 0x8000u; }
    if (r_spi_con & 0x4000u) r_spi_con &= ~0xC000u;    /* PCLR: the pending bit cleared */
    if (r_sfc != sfc_shadow) { if (r_sfc && !sfc_shadow && busy()) xip_on_busy++; sfc_shadow = r_sfc; }
    if (!(r_p33_dat & 0xFFFFFF00u)) {                 /* a P33 byte: the feed is 0x20 0x80 0x40 (or into WDT_CON) */
        uint32_t b = r_p33_dat & 255u;
        feed_seq = b == 0x20u ? 1u : (feed_seq == 1u && b == 0x80u) ? 2u : (feed_seq == 2u && b == 0x40u) ? 3u : 0u;
        if (feed_seq == 3u) { feeds++; if (last_feed && now - last_feed > feed_gap_max) feed_gap_max = now - last_feed; last_feed = now; feed_seq = 0; }
        r_p33_dat = SENT;
    }
    r_cache = 0x4000u;
}
static volatile uint32_t *fl_reg(uint32_t a)
{
    commit();
    switch (a) {
    case 0x11C00u: return &r_spi_con;
    case 0x11C08u: return &r_spi_buf;
    case 0x500C0u: return &r_portd;
    case 0x500C8u: return &r_portd_dir;
    case 0x5101Cu: return &r_iomap;
    case 0x40200u: return &r_sfc;
    case 0x1EEE008u: return &r_cache;
    case 0x10804u: now += 240u; r_t4 = now; return &r_t4;   /* 10 us a read */
    case 0x13E08u: return &r_p33_con;
    case 0x13E0Cu: return &r_p33_dat;
    default: return &r_other;
    }
}

static int check(const char *what, int ok) { printf("flash wait: %-100s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }
static void part(uint32_t id) { memset(&chip, 0, sizeof chip); chip.id = id; xip_on_busy = feeds = feed_gap_max = last_feed = 0; FL_FAR(fl_jedec_ram)(); }

int main(void)
{
    int bad = 0, rc;
    uint32_t took;
    static uint8_t page[256];
    memset(page, 0x5A, sizeof page);
    /* the write gate */
    part(0x123456u);
    rc = FL_FAR(fl_erase4k_ram)(0x97000u, &took); commit();   /* (the driver\'s last register write applied) */
    bad += check("unknown id 0x123456: the erase is refused, no 0x20 reaches the part", rc != 0 && chip.erases == 0u);
    rc = FL_FAR(fl_prog_ram)(0x97000u, page, 256u, &took); commit();   /* (the driver\'s last register write applied) */
    bad += check("unknown id 0x123456: the program is refused, no 0x02 reaches the part", rc != 0 && chip.progs == 0u);
    for (uint32_t i = 0; i < 2u; i++) {
        uint32_t id = i ? 0xB36014u : 0x856014u;
        char w[96];
        part(id); chip.ops_dur = 8000u;                  /* 8 ms: the Puya part's typical tSE */
        rc = FL_FAR(fl_erase4k_ram)(0x97000u, &took); commit();   /* (the driver\'s last register write applied) */
        snprintf(w, sizeof w, "id %06X: a 4 KiB erase (8 ms): 0, XIP back on only after WIP cleared", id);
        bad += check(w, rc == 0 && chip.erases == 1u && !xip_on_busy);
        chip.ops_dur = 2000u;
        rc = FL_FAR(fl_prog_ram)(0x97000u, page, 256u, &took); commit();   /* (the driver\'s last register write applied) */
        snprintf(w, sizeof w, "id %06X: a page program (2 ms): 0", id);
        bad += check(w, rc == 0 && chip.progs == 1u && !xip_on_busy);
    }
    /* a part slower than the limits */
    part(0xB36014u); chip.ops_dur = 1200000u;             /* 1.2 s, past the 500 ms erase limit */
    rc = FL_FAR(fl_erase4k_ram)(0x97000u, &took); commit();   /* (the driver\'s last register write applied) */
    bad += check("an erase of 1.2 s: waited out (-4), XIP never on while WIP is set", rc == -4 && !xip_on_busy && took >= 1190000u);
    bad += check("... the watchdog fed while waiting past the limit (every 100 ms)", feeds >= 6u && feed_gap_max <= 24u * 110000u);
    part(0xB36014u); chip.ops_dur = 120000u;              /* 120 ms, past the 50 ms program limit */
    rc = FL_FAR(fl_prog_ram)(0x97000u, page, 256u, &took); commit();   /* (the driver\'s last register write applied) */
    bad += check("a program of 120 ms: waited out (-4), XIP never on while WIP is set", rc == -4 && !xip_on_busy);
    part(0x856014u); chip.ops_dur = 6000000u;             /* 6 s: past FL_WIP_LONG_US */
    rc = FL_FAR(fl_erase4k_ram)(0x97000u, &took); commit();   /* (the driver\'s last register write applied) */
    bad += check("an erase of 6 s: -5 (the long error), still never XIP while busy, the watchdog fed all along", rc == -5 && !xip_on_busy && feeds >= 50u && feed_gap_max <= 24u * 110000u);
    /* (review) the gate after an id read that could not run */
    part(0x856014u);
    r_sfc = sfc_shadow = 0x600u;                          /* SFC mode 6 (continuous read): fl_enter refuses, fl_jedec_ram returns 0 */
    took = FL_FAR(fl_jedec_ram)();
    r_sfc = sfc_shadow = 0x200u;
    chip.ops_dur = 8000u;
    rc = FL_FAR(fl_erase4k_ram)(0x97000u, &took); commit();
    bad += check("a known id, then an id read that cannot run (SFC mode 6): the gate is closed, no 0x20 reaches the part", rc != 0 && chip.erases == 0u);
    /* (review) the two gates, each on its own, through the real storage_hw.c */
    part(0x856014u); chip.ops_dur = 8000u; flash_ok = 0;  /* the driver's gate open, persist_boot's verdict off */
    rc = st_erase(0x97000u); commit();
    bad += check("storage_hw.c: flash_ok 0, the driver's gate open: st_erase refused, no 0x20", rc != 0 && chip.erases == 0u);
    rc = st_prog(0x97000u, page, 256u); commit();
    bad += check("storage_hw.c: flash_ok 0, the driver's gate open: st_prog refused, no 0x02", rc != 0 && chip.progs == 0u);
    part(0x123456u); chip.ops_dur = 8000u; flash_ok = 1;  /* flash_ok forced on, the driver read an unknown id */
    rc = st_erase(0x97000u); commit();
    bad += check("storage_hw.c: flash_ok 1, the driver's gate closed (unknown id): st_erase refused, no 0x20", rc != 0 && chip.erases == 0u);
    rc = st_prog(0x97000u, page, 256u); commit();
    bad += check("storage_hw.c: flash_ok 1, the driver's gate closed: st_prog refused, no 0x02", rc != 0 && chip.progs == 0u);
    part(0xB36014u); chip.ops_dur = 8000u; flash_ok = 1;
    rc = st_erase(0x97000u); commit();
    bad += check("storage_hw.c: both open (UCUN): st_erase writes", rc == 0 && chip.erases == 1u);
    /* (re-review) a slow part through storage_hw.c: its own flash error, never the driver's -4 / -5 (which oc_module.c and oc_pack.c's callers read
     * as "the old pairs are still moving" / "no start free"); and the audio buffer cleared before a program as before an erase */
    part(0xB36014u); chip.ops_dur = 1200000u; flash_ok = 1;
    rc = st_erase(0x97000u); commit();
    bad += check("storage_hw.c: an erase past the limit: ST_FLASH_SLOW (-20), not -4", rc == ST_FLASH_SLOW);
    part(0xB36014u); chip.ops_dur = 6000000u; flash_ok = 1;
    rc = st_erase(0x97000u); commit();
    bad += check("storage_hw.c: an erase past 2 s: ST_FLASH_SLOW, not -5", rc == ST_FLASH_SLOW);
    part(0xB36014u); chip.ops_dur = 120000u; flash_ok = 1;
    for (uint32_t i = 0; i < sizeof abuf / sizeof abuf[0]; i++) abuf[i] = 1234;
    rc = st_prog(0x97000u, page, 256u); commit();
    {
        int clear = 1;
        for (uint32_t i = 0; i < sizeof abuf / sizeof abuf[0]; i++) clear &= abuf[i] == 0;
        bad += check("storage_hw.c: a program past the limit: ST_FLASH_SLOW; the audio buffer cleared first", rc == ST_FLASH_SLOW && clear);
    }
    /* (review) the report's commands */
    {
        static fl_report_t r;
        part(0xB36014u);
        rc = fl_report(&r); commit();
        bad += check("FLASH_ID's read (fl_report): 0x05, 0x9F, 0x35, 0x5A sent, the id read", rc == 0 && chip.seen[0x05] && chip.seen[0x9F] && chip.seen[0x35] && chip.seen[0x5A] && r.id == 0xB36014u);
        int other = 0;
        for (uint32_t c = 0; c < 256u; c++) other |= chip.seen[c] && c != 0x05u && c != 0x9Fu && c != 0x35u && c != 0x5Au;
        bad += check("FLASH_ID's read: no other opcode (no 0x15: not in the P25Q80H's map, the UCUN map unknown), SR3 reported 0", !other && !chip.seen[0x15] && r.sr3 == 0u);
    }
    printf("%s\n", bad ? "FLASH WAIT TEST FAILED" : "flash wait test passed");
    return bad != 0;
}
