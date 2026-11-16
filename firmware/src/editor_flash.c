/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 27, docs/orangecat/FLASH_CHIPS.md) editor 134 FLASH_ID (through the extension: F0 7D 46 4C 00 06 F7): the flash part, read
 * only. The JEDEC id (0x9F), status registers 1/2/3 (0x05, 0x35, 0x15), the SFDP header and basic parameter table (0x5A), and the
 * firmware's verdict (hal/fm1_flash_id.h). Nothing is written and no write command is sent; when SR1 says a write is in progress
 * only SR1 is read (rc 1). The read runs as every fl_*_ram (from RAM, interrupts off: ED_FLASH_REPORT, hal/fm1_flash.h fl_report). Reply: web/EDITOR_PROTOCOL.md.
 *
 * Hooks: ED_FLASH_REPORT(r) fills an fl_report_t, -> 0 read, else not read (the device: hal/fm1_flash.h fl_report; a host test
 * its own). flash_ok and flash_jedec: persist_boot's (project.c). */
#if MELODEE_FLASH
/* PACK_INFO's and MOD_INFO's last byte, FLASH_ID's second: 0 FLASH NOT SUPPORTED (the firmware writes nothing), 1 Puya, 2 UCUN. On the device flash_ok is
 * FL_JEDEC_OK(flash_jedec); a host test that sets flash_ok without persist_boot gets 1 */
#define ED_FLASH_VERDICT (!flash_ok ? 0u : FL_JEDEC_PART(flash_jedec) ? FL_JEDEC_PART(flash_jedec) : 1u)
#else
#define ED_FLASH_VERDICT 1u                            /* (a host build of the pack's commands without the store: its simulated part) */
#endif
#if MELODEE_FLASH
enum { ED_FLASH_ID = 134 };
#ifndef ED_FLASH_REPORT
#ifdef FM1_IRQ_TARGET
static int ed_flash_report_dev(fl_report_t *r) { uint32_t f = irq_save(); int rc = fl_report(r); irq_restore(f); return rc; }
#define ED_FLASH_REPORT(r) ed_flash_report_dev(r)
#endif
#endif
#ifdef ED_FLASH_REPORT
static fl_report_t ed_fl_rep;                         /* in RAM: the RAM driver writes it with XIP off */
static void ed_b8(uint32_t v) { ed_b(v & 127u); ed_b((v >> 7) & 1u); }   /* a byte: the low 7 bits, then the top bit */
static void ed_b24(uint32_t v) { ed_b(v); ed_b(v >> 7); ed_b(v >> 14); ed_b(v >> 21); }   /* 24 bits as 4 x 7, low first */
static int ed_flash_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    fl_report_t *r = &ed_fl_rep;
    uint32_t i, rc;
    (void)a;
    if (!(cmd == ED_FLASH_ID) || na) return 0;            /* (written as cmd == ED_FLASH_ID: tools/check_editor_protocol.py reads the commands so) */
    memset(r, 0, sizeof *r);
    rc = ED_FLASH_REPORT(r) ? 2u : r->busy ? 1u : 0u;
    ed_b(1);                                          /* the reply's version */
    ed_b(ED_FLASH_VERDICT);                           /* the verdict on the id read at boot: 0 FLASH NOT SUPPORTED, 1 Puya P25Q80(H), 2 UCUN UC25HQ80 */
    ed_b(flash_ok ? 1u : 0u);                         /* 1 the firmware writes the flash; 0 it is left alone */
    ed_b24(flash_jedec);                              /* the id persist_boot read */
    ed_b(rc);                                         /* 0 read; 1 a write in progress (SR1 only); 2 not read */
    ed_b24(rc ? 0u : r->id);                          /* the id read now */
    ed_b8(rc == 2u ? 0u : r->sr1);
    ed_b8(rc ? 0u : r->sr2);
    ed_b8(rc ? 0u : r->sr3);
    ed_b(!rc && r->sfdp_ok ? 1u : 0u);
    for (i = 0; i < 16u; i++) ed_b8(rc ? 0u : r->sfdp[i]);
    ed_b(rc ? 0u : r->bfpt_n);
    for (i = 0; i < (rc ? 0u : r->bfpt_n); i++) ed_b8(r->bfpt[i]);
    for (i = 0; i < 5u; i++) ed_b(r->sfc_con >> (7u * i));   /* the SFC's control register as found (5 x 7): bits 11:8 the XIP read mode */
    return 1;
}
#else
static int ed_flash_handle(uint32_t cmd, const uint8_t *a, uint32_t na) { (void)cmd; (void)a; (void)na; return 0; }
#endif
#else
static int ed_flash_handle(uint32_t cmd, const uint8_t *a, uint32_t na) { (void)cmd; (void)a; (void)na; return 0; }
#endif
