/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Revisions:
 * (Orange Cat 0.9.1) Library in groups with a shared spare sector (0.9.1)
 */
/* FM-1 SPI NOR at runtime: erase / program / read. Used only when
 * MELODEE_FLASH is 1.
 *
 * Everything that executes while the SFC (XIP) is switched off lives in
 * .ram_text and touches only SFRs, the stack and RAM buffers: no .rodata, no
 * libcalls, no XIP-resident helpers. Call the fl_*_ram() entry points with
 * interrupts disabled. Same sequence as the AC79 SDK flash driver
 * (cpu.a/vm_sfc.c.o: enter_spi_code, norflash_erase, norflash_wait_ok, exit_spi_code).
 */
#pragma once
#include <stdint.h>
#include "fm1_xip.h"

#define RAMFN  __attribute__((section(".ram_text"), noinline, used))
#define RAMINL static inline __attribute__((always_inline))

/* XIP (0x02xxxxxx) -> RAM (0x01C0xxxx) is beyond the 23-bit call range:
 * call RAM code through a pointer the compiler cannot fold back. */
static inline void *fl_far(void *p) { void *volatile q = p; return q; }
#define FL_FAR(fn) ((__typeof__(&fn))fl_far((void *)&fn))

#define REG32(a) (*(volatile uint32_t *)(a))
#define SFC_CON         REG32(0x40200u)            /* bit31 busy; [11:8] read mode */
#define SFCENC_CON      (*(volatile uint8_t *)0x40300u)  /* b0 decrypt, b1 plain window */
#define SFCENC_UNENC_H  REG32(0x40308u)            /* WO, XIP (CPU) address */
#define SFCENC_UNENC_L  REG32(0x4030Cu)            /* WO, XIP (CPU) address */
#define SPI0_CON        REG32(0x11C00u)            /* b15 PND, b14 PCLR, b12 DIR=rx */
#define SPI0_BUF        REG32(0x11C08u)
#define IOMAP_CON0      REG32(0x5101Cu)            /* b5: 1 = SFC owns the flash pins */
#define PORTD_OUT       REG32(0x500C0u)            /* PD0 = flash CS# */
#define PORTD_DIR       REG32(0x500C8u)
#define CACHE_CON       REG32(0x1EEE008u)          /* b14 = cache idle */
#define T4_CNT          REG32(0x10804u)            /* TIMER4, 24 MHz free-running */

#define FL_XIP(off)     FM1_XIP(off)              /* fm1_xip.h */
#define FL_DATA_LO      0x00097000u                /* Melodee main store */
#define FL_DATA_HI      0x000E0000u
#define FL_GLOB_LO      0x000FC000u                /* Melodee superblock / globals */
#define FL_GLOB_HI      0x000FF000u
#define FL_BANK_EXT_LO  0x000E5000u                /* expanded presets and project extension copies; after OTA staging */
#define FL_BANK_EXT_HI  0x000FC000u                /* (0.9.1: one sector longer, the library group's initial spare at 0xFB000) */
#define FL_OTA_LO       0x000E0000u                /* M-UPGRADE loader staging, ota.c */
#define FL_OTA_HI       0x000E5000u
/* [off, off + n) inside [lo, hi), without wrapping: off + n can overflow, and
 * the 1 MiB part ignores the high address bits, so a wrapped range lands low. */
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
/* Melodee's own store (projects, user samples; settings) */
/* (Orange Cat 0.7.2) the flash map is in firmware/src/storage.c's top comment. The two engine areas, named: the PACK area
 * (asset pack, or the Prophet banks) and the MODULE area (CZ-1 and native FM6 banks, or the code module later). Both are
 * writable by the store; the same ranges are inside FL_DATA already (0x97000..), the pack area's first four sectors were the
 * only part that needed its own clause */
#define FL_PACK_LO      0x00093000u                /* 10 sectors */
#define FL_PACK_HI      0x0009D000u
#define FL_MODULE_LO    0x000C8000u                /* 12 sectors (0.9.1: was 20, the library group took the top 8) */
#define FL_MODULE_HI    0x000D4000u
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_PACK_LO, FL_PACK_HI) || FL_IN(off, n, FL_MODULE_LO, FL_MODULE_HI) || FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_GLOB_LO, FL_GLOB_HI) || FL_IN(off, n, FL_BANK_EXT_LO, FL_BANK_EXT_HI))
/* Where the RAM driver may erase / program. The app build allows only its own
 * data regions; the update loader (firmware/loader) defines its own window. */
#ifndef FL_RANGE_OK
#define FL_RANGE_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI))
#endif

typedef struct { uint32_t sfc_con, spi_con; } fl_saved_t;

RAMINL void fl_csync(void) { __asm__ volatile("csync" ::: "memory"); }
RAMINL void fl_cache_idle(void) { while (!(CACHE_CON & 0x4000u)) ; }
RAMINL void fl_delay(uint32_t n) { volatile uint32_t i = n; while (i--) ; }

RAMINL void fl_cs(int high)
{
    PORTD_DIR &= ~1u;
    if (high) PORTD_OUT |= 1u; else PORTD_OUT &= ~1u;
    fl_delay(10);
}
RAMINL int fl_spi_wait(void)
{
    uint32_t n = 50000000u;
    fl_csync();
    while (!(SPI0_CON & 0x8000u))
        if (--n == 0) break;
    SPI0_CON |= 0x4000u;
    return n ? 0 : -1;
}
RAMINL void fl_tx(uint8_t b)
{
    SPI0_CON |= 8u; SPI0_CON &= ~0x1000u;
    SPI0_BUF = b; fl_spi_wait();
    SPI0_CON &= ~8u;
}
RAMINL uint8_t fl_rx(void)
{
    uint8_t v;
    SPI0_CON |= 8u; SPI0_CON |= 0x1000u;
    SPI0_BUF = 0xFFu; fl_spi_wait();
    v = (uint8_t)SPI0_BUF;
    SPI0_CON &= ~8u;
    return v;
}
RAMINL void fl_addr(uint32_t a) { fl_tx((uint8_t)(a >> 16)); fl_tx((uint8_t)(a >> 8)); fl_tx((uint8_t)a); }
RAMINL uint8_t fl_rdsr(uint8_t cmd) { uint8_t v; fl_cs(0); fl_tx(cmd); v = fl_rx(); fl_cs(1); return v; }
RAMINL void fl_wren(void) { fl_cs(0); fl_tx(0x06); fl_cs(1); }

RAMINL int fl_enter(fl_saved_t *s)
{
    uint32_t mode;
    fl_cache_idle();
    s->sfc_con = SFC_CON;
    s->spi_con = SPI0_CON;
    mode = (s->sfc_con >> 8) & 0xFu;
    if (mode == 6u || mode == 7u)                 /* continuous-read XIP: not handled here */
        return -1;
    fl_csync();
    while ((int32_t)SFC_CON < 0) ;                /* SFC idle */
    SFC_CON = 0;                                  /* XIP OFF from here on */
    IOMAP_CON0 &= ~0x20u;                         /* pins -> SPI0 / GPIO */
    SPI0_CON = 0;
    SPI0_CON = 0x1029u;                           /* SDK default */
    fl_csync();
    return 0;
}
RAMINL void fl_exit(const fl_saved_t *s)
{
    SPI0_CON = s->spi_con;
    IOMAP_CON0 |= 0x20u;
    fl_csync();
    while ((int32_t)SFC_CON < 0) ;
    SFC_CON = s->sfc_con;                         /* XIP back on */
    fl_csync();
}
RAMINL void fl_inval(uint32_t off, uint32_t len)
{
    uint32_t a = FL_XIP(off) & ~31u, e = FL_XIP(off) + len;
    fl_cache_idle();
    for (; a < e; a += 32u) {
        fl_csync();
        __asm__ volatile("flushinv [%0]" :: "r"(a) : "memory");
        fl_csync();
        fl_cache_idle();
    }
}
RAMINL int fl_wait_ready(uint32_t max_us, uint32_t *took_us)
{
    uint32_t t0 = T4_CNT, dt;
    for (;;) {
        dt = (T4_CNT - t0) / 24u;
        if (!(fl_rdsr(0x05) & 1u)) { *took_us = dt; return 0; }
        if (dt > max_us) { *took_us = dt; return -4; }
    }
}

/* ---- entry points: IRQs OFF, args/buffers in RAM ---- */

static RAMFN uint32_t fl_jedec_ram(void)                 /* expect 0x856014 */
{
    fl_saved_t s; uint32_t id;
    if (fl_enter(&s)) return 0;
    fl_cs(0); fl_tx(0x9F);
    id = (uint32_t)fl_rx() << 16; id |= (uint32_t)fl_rx() << 8; id |= fl_rx();
    fl_cs(1);
    fl_exit(&s);
    return id;
}

static RAMFN uint32_t fl_status_ram(void)                /* SR1 | SR2 << 8 */
{
    fl_saved_t s; uint32_t v;
    if (fl_enter(&s)) return 0xFFFFFFFFu;
    v = fl_rdsr(0x05) | ((uint32_t)fl_rdsr(0x35) << 8);
    fl_exit(&s);
    return v;
}

static RAMFN int fl_erase4k_ram(uint32_t off, uint32_t *took_us)
{
    fl_saved_t s; int rc;
    if (!FL_RANGE_OK(off, 0x1000u) || (off & 0xFFFu))
        return -1;
    if (fl_enter(&s)) return -2;
    fl_wren();
    if (!(fl_rdsr(0x05) & 2u)) rc = -3;           /* WEL did not set */
    else {
        fl_cs(0); fl_tx(0x20); fl_addr(off); fl_cs(1);
        fl_inval(off, 4096u);
        rc = fl_wait_ready(500000u, took_us);     /* generic NOR max 400 ms */
    }
    fl_exit(&s);
    return rc;
}

static RAMFN int fl_prog_ram(uint32_t off, const uint8_t *src, uint32_t n, uint32_t *took_us)
{
    fl_saved_t s; int rc; uint32_t i;
    if (!FL_RANGE_OK(off, n) ||
        n == 0 || n > 256u || ((off & 0xFFu) + n) > 256u) return -1;    /* one page, no wrap */
    if (fl_enter(&s)) return -2;
    fl_wren();
    if (!(fl_rdsr(0x05) & 2u)) rc = -3;
    else {
        fl_cs(0); fl_tx(0x02); fl_addr(off);
        for (i = 0; i < n; i++) fl_tx(src[i]);
        fl_cs(1);
        fl_inval(off, n);
        rc = fl_wait_ready(10000u, took_us);      /* generic NOR max 3 ms */
    }
    fl_exit(&s);
    return rc;
}

static RAMFN int fl_read_ram(uint32_t off, uint8_t *dst, uint32_t n)   /* 0x0B fast read */
{
    fl_saved_t s; uint32_t i;
    if (fl_enter(&s)) return -2;
    fl_cs(0); fl_tx(0x0B); fl_addr(off); fl_tx(0xFF);
    for (i = 0; i < n; i++) dst[i] = fl_rx();
    fl_cs(1);
    fl_exit(&s);
    return 0;
}

/* ---- XIP side (normal .text) ---- */

/* Once at boot, before any XIP read of the data region: map the data area
 * as an unencrypted XIP window (as the SDK's IOCTL_SET_VM_INFO does). */
static void fl_plain_window_init(void)
{
    SFCENC_CON &= (uint8_t)~2u;
    SFCENC_UNENC_L = FL_XIP(0x93000u);            /* (the app area ends there; the Prophet banks begin) */
    SFCENC_UNENC_H = 0x07FFFFFFu;
    SFCENC_CON |= 2u;
    __asm__ volatile("cli" ::: "memory");
    fl_inval(0x93000u, 0x100000u - 0x93000u);     /* drop any decrypted lines */
    __asm__ volatile("csync\n\tsti" ::: "memory");
}

#if OC_SFC_PROBE
/* (Orange Cat 0.13.7, branch linux-hw; OC_SFC_PROBE builds only: editor command 109) the two read paths of flash above the app, read only. For
 * k = 0 the plaintext window as fl_plain_window_init sets it; for k = 1 the other path: mode 0, the window's low end moved past [off, off + 4 KB)
 * (the decrypting XIP path the app is read through); mode 1, LENC_L / LENC_H set over the range with the window as it is; mode 2, as 1 with
 * SFCENC_CON bit 2. Each: the D-cache lines dropped, a cold and a warm pass of 1,024 16-bit reads timed in TIMER4 ticks (ticks[2k], [2k + 1]),
 * then 32 bytes at off (got[32k ..]). Afterwards the window is put back and LENC cleared. -> the KEY register. Interrupts off (the caller). */
static volatile uint32_t fl_probe_sink;
static uint32_t fl_probe_paths(uint32_t off, uint32_t mode, uint8_t *got, uint32_t *ticks)
{
    uint32_t key = REG32(0x40304u) & 0xFFFFu;
    for (uint32_t k = 0; k < 2u; k++) {
        if (k) {
            if (mode == 0u) {
                SFCENC_CON &= (uint8_t)~2u;
                SFCENC_UNENC_L = FL_XIP((off + 0x1FFFu) & ~0xFFFu);
                SFCENC_UNENC_H = 0x07FFFFFFu;
                SFCENC_CON |= 2u;
            } else {
                REG32(0x40314u) = FL_XIP(off);
                REG32(0x40310u) = FL_XIP(off + 0x1000u) - 1u;
                if (mode == 2u) SFCENC_CON |= 4u;
            }
            fl_csync();
        }
        fl_inval(off, 2048u);
        for (uint32_t pass = 0; pass < 2u; pass++) {
            const volatile uint16_t *p = (const volatile uint16_t *)FL_XIP(off);
            uint32_t sum = 0, t0 = T4_CNT;
            for (uint32_t i = 0; i < 1024u; i++) sum += p[i];
            ticks[k * 2u + pass] = T4_CNT - t0;
            fl_probe_sink = sum;
        }
        for (uint32_t i = 0; i < 32u; i++) got[k * 32u + i] = ((const volatile uint8_t *)FL_XIP(off))[i];
    }
    SFCENC_CON &= (uint8_t)~6u;
    REG32(0x40314u) = 0;
    REG32(0x40310u) = 0;
    SFCENC_UNENC_L = FL_XIP(0x93000u);
    SFCENC_UNENC_H = 0x07FFFFFFu;
    SFCENC_CON |= 2u;
    fl_csync();
    fl_inval(off, 2048u);
    return key;
}
#endif

static inline uint32_t irq_save(void) { __asm__ volatile("cli" ::: "memory"); return 0; }
static inline void irq_restore(uint32_t f) { (void)f; __asm__ volatile("csync\n\tsti" ::: "memory"); }

static int fl_erase4k(uint32_t off, uint32_t *took_us)
{
    uint32_t f = irq_save();
    int rc = FL_FAR(fl_erase4k_ram)(off, took_us);
    irq_restore(f);
    return rc;
}

static int fl_write(uint32_t off, const uint8_t *src_ram, uint32_t n)
{
    uint32_t took;
    while (n) {
        uint32_t chunk = 256u - (off & 0xFFu);
        int rc;
        uint32_t f;
        if (chunk > n) chunk = n;
        f = irq_save();
        rc = FL_FAR(fl_prog_ram)(off, src_ram, chunk, &took);
        irq_restore(f);
        if (rc) return rc;
        off += chunk; src_ram += chunk; n -= chunk;
    }
    return 0;
}

/* Read back through XIP (needs fl_plain_window_init) and compare. */
static int fl_verify_xip(uint32_t off, const uint8_t *ref, uint32_t n)
{
    const volatile uint8_t *p = (const volatile uint8_t *)FL_XIP(off);
    uint32_t i;
    for (i = 0; i < n; i++) if (p[i] != ref[i]) return -(int)(i + 1);
    return 0;
}
