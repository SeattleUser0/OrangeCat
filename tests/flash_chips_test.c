/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 27, docs/orangecat/FLASH_CHIPS.md) the two flash parts and an unknown one, on the simulated NOR of persistence_test.c: persist_boot reads
 * the JEDEC id (fl_jedec_ram, here host_jedec); the Puya P25Q80(H) (0x856014) and the UCUN UC25HQ80 (0xB36014) save and read back after a power
 * cycle; an unknown id writes nothing (no erase, no byte of the NOR changed) and every save says FLASH NOT SUPPORTED. Then editor 134 FLASH_ID and
 * the verdict byte PACK_INFO / MOD_INFO append, against a simulated chip's answers (ED_FLASH_REPORT).
 *
 * Built so that it also compiles on a tree without fm1_flash_id.h (06fd4f40): there each check runs and the new ones FAIL. */
#define PERSISTENCE_TEST_NO_MAIN
#define OC_SET_OK 1
#include "persistence_test.c"

#ifndef FL_JEDEC_OK                                  /* (06fd4f40: no fm1_flash_id.h, no flag, no report) */
static uint8_t ui_flash_unsupported;
#define FC_HAVE_REPORT 0
#else
#define FC_HAVE_REPORT 1
#endif

static uint8_t fc_rep[512];
static uint32_t fc_rep_n;
#if FC_HAVE_REPORT
static fl_report_t fc_chip;                          /* the simulated chip's answers */
static int fc_report_rc;
static int fc_report(fl_report_t *r) { if (fc_report_rc) return fc_report_rc; *r = fc_chip; if (r->busy) { uint8_t s1 = r->sr1; memset(r, 0, sizeof *r); r->sr1 = s1; r->busy = 1; } return 0; }
static void fc_ed_b(uint32_t v) { if (fc_rep_n < sizeof fc_rep) fc_rep[fc_rep_n++] = (uint8_t)(v & 127u); }
#define ED_FLASH_REPORT(r) fc_report(r)
#define ed_b fc_ed_b
#include "../firmware/src/editor_flash.c"
#undef ed_b
#endif

static uint8_t snap[0x100000];

static int fc_check(const char *what, int ok) { printf("flash chips: %-96s %s\n", what, ok ? "ok" : "FAIL"); return ok ? 0 : 1; }

static void boot_as(uint32_t id)                     /* power on with this part: RAM gone, the NOR as it is */
{
    memcpy(snap, nor, sizeof snap);
    reset();                                         /* (clears the NOR too: put back) */
    memcpy(nor, snap, sizeof nor);
    ocs_boot(); ocp_scan(); ocj_scan();
    host_jedec = id;
    ui_flash_unsupported = 0;
    flash_ok = 0;
    erases = 0;
    persist_boot();
}

static int part_saves(uint32_t id, const char *name)
{
    static uint8_t got[BANK_STORE_SIZE];
    char what[128];
    int bad = 0;
    reset();
    boot_as(id);
    snprintf(what, sizeof what, "%s: persist_boot accepts the part (writes on, no notice)", name);
    bad += fc_check(what, flash_ok == 1 && !ui_flash_unsupported);
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    project_save(0);
    snprintf(what, sizeof what, "%s: a project save is written to the flash (SAVED, erases)", name);
    bad += fc_check(what, !strcmp(ui.msg, "SAVED") && erases > 0u);
    settings.palette = 2;
    settings_save();
    snprintf(what, sizeof what, "%s: the settings are written (nothing left pending)", name);
    bad += fc_check(what, !persist_pending);
    boot_as(id);                                     /* power off and on */
    snprintf(what, sizeof what, "%s: after a power cycle the project reads back from the flash", name);
    bad += fc_check(what, flash_ok == 1 && ocj_read(0, got) == (int)sizeof got &&
                    proj_import(&proj_scratch, (const project_store_t *)(got + 8u), sizeof proj_slot[0]) && proj_scratch.t[0].step[0].note[0] == 60u);
    {
        persist_t p;
        snprintf(what, sizeof what, "%s: after a power cycle the settings read back", name);
        bad += fc_check(what, st_load(OBJ_SETTINGS, &p, sizeof p) == (int)sizeof p && p.palette == palette_to_stored(2));
    }
    return bad;
}

static int unknown_refuses(uint32_t id)
{
    static uint8_t before[0x100000];
    char what[128];
    int bad = 0;
    uint32_t e0;
    reset();
    boot_as(0x856014u);                              /* a unit with music on it, saved on a known part */
    trk[0].step[0] = (step_t){{60}, 1, ST_NOTE, 0, 96, 0, 0};
    project_save(0);
    memcpy(before, nor, sizeof before);
    boot_as(id);                                     /* the same flash, an id the firmware does not know */
    e0 = erases;
    snprintf(what, sizeof what, "id %06X: persist_boot leaves the flash alone (writes off, the notice flag set)", id);
    bad += fc_check(what, flash_ok == 0 && ui_flash_unsupported == 1);
    trk[0].step[0].note[0] = 72;
#ifdef FC_DEBUG
#define FC_DIFF(w) do { uint32_t k_; for (k_ = 0; k_ < sizeof before && nor[k_] == before[k_]; k_++) ; printf("  (debug) %s: erases %u, first change %#x\n", w, erases - e0, k_); } while (0)
#else
#define FC_DIFF(w) ((void)0)
#endif
    FC_DIFF("booted");
    project_save(1);
    FC_DIFF("project_save");
    snprintf(what, sizeof what, "id %06X: a project save says FLASH NOT SUPPORTED", id);
    bad += fc_check(what, !strcmp(ui.msg, "FLASH NOT SUPPORTED"));
    template_save();
    FC_DIFF("template_save");
    snprintf(what, sizeof what, "id %06X: the template save says FLASH NOT SUPPORTED (not \"TEMPLATE SAVED (RAM)\")", id);
    bad += fc_check(what, !strcmp(ui.msg, "FLASH NOT SUPPORTED"));
    settings.palette = 3;
    settings_save();
    settings_poll();
    FC_DIFF("settings");
    snprintf(what, sizeof what, "id %06X: no erase and not one byte of the flash changed", id);
    bad += fc_check(what, erases == e0 && !memcmp(before, nor, sizeof before));
#ifdef FC_DEBUG
    printf("  (debug) writes refused by the gate: %u\n", st_gate_refused);
#endif
    return bad;
}

#if FC_HAVE_REPORT
static uint32_t fc_u24(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 7 | (uint32_t)p[2] << 14 | (uint32_t)p[3] << 21; }
static uint32_t fc_u8(const uint8_t *p) { return p[0] | (uint32_t)p[1] << 7; }
static int report(void)
{
    int bad = 0;
    static const uint8_t SFDP[16] = {'S', 'F', 'D', 'P', 0x06, 0x01, 0x01, 0xFF, 0x00, 0x06, 0x01, 0x10, 0x30, 0x00, 0x00, 0xFF};
    reset();
    boot_as(0xB36014u);
    memset(&fc_chip, 0, sizeof fc_chip);
    fc_chip.id = 0xB36014u; fc_chip.sr1 = 0x00; fc_chip.sr2 = 0x02; fc_chip.sr3 = 0x60; fc_chip.sfdp_ok = 1; fc_chip.sfc_con = 0x80000521u;
    memcpy(fc_chip.sfdp, SFDP, 16); fc_chip.bfpt_n = 64;
    for (uint32_t i = 0; i < 64u; i++) fc_chip.bfpt[i] = (uint8_t)(0xE5u + i * 3u);
    fc_rep_n = 0;
    bad += fc_check("FLASH_ID: answered (no arguments)", ed_flash_handle(ED_FLASH_ID, 0, 0) == 1);
    bad += fc_check("FLASH_ID: version 1, verdict 2 (UCUN), writes on, the boot id", fc_rep[0] == 1u && fc_rep[1] == 2u && fc_rep[2] == 1u && fc_u24(fc_rep + 3) == 0xB36014u);
    bad += fc_check("FLASH_ID: rc 0, the id now, SR1 SR2 SR3 as 2 x 7 bits", fc_rep[7] == 0u && fc_u24(fc_rep + 8) == 0xB36014u && fc_u8(fc_rep + 12) == 0u && fc_u8(fc_rep + 14) == 2u && fc_u8(fc_rep + 16) == 0x60u);
    {
        int ok = fc_rep[18] == 1u && fc_rep[35 + 16] == 64u && fc_rep_n == 52u + 128u + 5u;
        uint32_t sc = 0;
        for (uint32_t i = 0; i < 5u; i++) sc |= (uint32_t)fc_rep[180 + i] << (7u * i);
        ok &= sc == 0x80000521u;
        for (uint32_t i = 0; i < 16u; i++) ok &= fc_u8(fc_rep + 19 + 2 * i) == SFDP[i];
        for (uint32_t i = 0; i < 64u; i++) ok &= fc_u8(fc_rep + 52 + 2 * i) == (uint8_t)(0xE5u + i * 3u);
        bad += fc_check("FLASH_ID: the SFDP header (16 bytes), the basic parameter table (64 bytes), top bits kept; the SFC register last", ok);
    }
    fc_chip.busy = 1; fc_chip.sr1 = 0x03; fc_rep_n = 0;
    ed_flash_handle(ED_FLASH_ID, 0, 0);
    bad += fc_check("FLASH_ID: a write in progress (WIP): rc 1, SR1 only, nothing else read", fc_rep[7] == 1u && fc_u24(fc_rep + 8) == 0u && fc_u8(fc_rep + 12) == 3u && fc_rep[18] == 0u && fc_rep_n == 57u);
    fc_chip.busy = 0; fc_report_rc = -2; fc_rep_n = 0;
    ed_flash_handle(ED_FLASH_ID, 0, 0);
    bad += fc_check("FLASH_ID: the driver could not read (SFC mode): rc 2", fc_rep[7] == 2u && fc_rep_n == 57u);
    fc_report_rc = 0;
    {
        uint8_t one = 1;
        fc_rep_n = 0;
        bad += fc_check("FLASH_ID: an argument: no reply", ed_flash_handle(ED_FLASH_ID, &one, 1) == 0 && fc_rep_n == 0u);
    }
    boot_as(0x123456u);
    fc_chip.id = 0x123456u; fc_rep_n = 0;
    ed_flash_handle(ED_FLASH_ID, 0, 0);
    bad += fc_check("FLASH_ID on an unknown part: verdict 0 (FLASH NOT SUPPORTED), writes off, its id", fc_rep[1] == 0u && fc_rep[2] == 0u && fc_u24(fc_rep + 3) == 0x123456u);
    bad += fc_check("the verdict PACK_INFO and MOD_INFO append: 0 unknown", ED_FLASH_VERDICT == 0u);
    boot_as(0x856014u);
    bad += fc_check("the verdict: 1 Puya", ED_FLASH_VERDICT == 1u);
    return bad;
}
#else
static int report(void) { return fc_check("FLASH_ID (editor 134) and the verdict byte exist", 0); }
#endif

int main(void)
{
    int bad = 0;
    bad += part_saves(0x856014u, "Puya P25Q80H 0x856014");
    bad += part_saves(0xB36014u, "UCUN UC25HQ80 0xB36014");
    bad += unknown_refuses(0x123456u);
    bad += unknown_refuses(0xC86014u);               /* (another vendor's 1 MiB id: not on the list) */
    bad += unknown_refuses(0x000000u);               /* (no answer, or the SFC in a mode the driver does not handle: fl_jedec_ram's 0) */
    bad += unknown_refuses(0xFFFFFFu);               /* (a floating bus) */
    bad += report();
    printf("%s\n", bad ? "FLASH CHIPS TEST FAILED" : "flash chips test passed");
    return bad != 0;
}
