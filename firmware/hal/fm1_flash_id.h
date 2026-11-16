/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 27) The SPI NOR parts the FM-1 ships with: the one list of accepted JEDEC ids (command 0x9F: manufacturer, memory
 * type, density). Used by the firmware (project.c persist_boot, hal/fm1_flash.h's write gate), the update loader
 * (firmware/loader/ldr_core.c), the editor's FLASH_ID report (editor.c, command 134); tools/fm1_recover.py and the page carry
 * no list of their own (fm1_recover.py mirrors this one, tests/recovery_test.py checks they agree; the page goes by the
 * unit's verdict). docs/orangecat/FLASH_CHIPS.md: what is verified and what is reported.
 *
 *   0x856014  Puya P25Q80 / P25Q80H   the original part (datasheet read: RDID 85h 60h 14h)
 *   0xB36014  UCUN UC25HQ80           production from about autumn 2026 (reported by the owner's research and the sibling
 *                                     firmware Clipper 1.3.1's fm1_flash_id.h; no datasheet text found)
 *
 * Both: 1 MiB, 4 KiB sector erase (0x20), 256-byte page program (0x02), read 0x03 / fast read 0x0B, status 0x05 / 0x35,
 * write enable 0x06. Those are the only commands the drivers send to write (FLASH_CHIPS.md, the audit). Any other id: the
 * flash is left alone (no erase, no program), and the unit says FLASH NOT SUPPORTED.
 *
 * Plain integer compares only: the RAM driver uses these while XIP is off (no .rodata). */
#pragma once
#include <stdint.h>

#define FL_JEDEC_PUYA_P25Q80   0x856014u
#define FL_JEDEC_UCUN_UC25HQ80 0xB36014u
#define FL_JEDEC_OK(id) ((id) == FL_JEDEC_PUYA_P25Q80 || (id) == FL_JEDEC_UCUN_UC25HQ80)
/* the part as a number for the reports: 1 Puya, 2 UCUN, 0 not supported */
#define FL_JEDEC_PART(id) ((id) == FL_JEDEC_PUYA_P25Q80 ? 1u : (id) == FL_JEDEC_UCUN_UC25HQ80 ? 2u : 0u)
#define FL_NOT_SUPPORTED_TEXT "FLASH NOT SUPPORTED"

/* the read-only report (hal/fm1_flash.h fl_report fills it on the device, from fl_cmd_rd_ram; editor_flash.c sends it as editor 134 FLASH_ID) */
typedef struct {
    uint32_t id;                  /* 0x9F: manufacturer << 16 | type << 8 | density */
    uint8_t busy;                 /* SR1's WIP bit was set: nothing past SR1 was read */
    uint8_t sr1, sr2, sr3;        /* 0x05, 0x35, 0x15 */
    uint8_t sfdp_ok;              /* the 0x5A header's signature read "SFDP" */
    uint8_t bfpt_n;               /* bytes of the basic parameter table read (parameter header 0's length, at most 64) */
    uint8_t sfdp[16];             /* 0x5A at 0: the SFDP header and parameter header 0 */
    uint8_t bfpt[64];             /* 0x5A at parameter header 0's pointer */
    uint32_t sfc_con;             /* the SFC's control register as found (bits 11:8 the XIP read mode the SPL set: 1-bit, dual or quad) */
} fl_report_t;
