/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware side of storage.c (MELODEE_FLASH): flash reads in short IRQ-off windows so the
 * audio keeps up, erases and writes only inside the storage areas, the audio buffer silenced
 * first. The host tests (storage_test.c) provide their own st_* on a simulated NOR. */
static uint8_t flash_ok;                 /* JEDEC id accepted at boot (persist_boot: hal/fm1_flash_id.h FL_JEDEC_OK) */
/* (Round 27 re-review) the driver's -4 / -5 (the part finished only after the wait's limit, fm1_flash.h fl_wait_ready) are this file's own
 * flash error, ST_FLASH_SLOW: the callers above give -4 and -5 other meanings (oc_module.c MI_PAIRS, MI_NOFREE; oc_pack.c -4) */
#define ST_FLASH_SLOW (-20)
static int st_hw_rc(int rc) { return rc == -4 || rc == -5 ? ST_FLASH_SLOW : rc; }
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
static int st_erase(uint32_t off)
{
#if MELODEE_PROPHET_PROTOTYPE
    (void)off;
    return -8; /* measurement firmware must not persist the temporary index-0 renderer */
#else
    uint32_t took, f;
    int rc;
    if (!flash_ok)                              /* (Round 27, FLASH_CHIPS.md) a part persist_boot did not accept: no writer erases it (the pool and the record store */
        return -6;                              /* did before this round, whatever flash_ok said); the driver refuses too (fm1_flash.h fl_id_gate) */
    if (!FL_STORE_OK(off, 0x1000u))
        return -8;
    f = irq_save();
    audio_silence();
    rc = FL_FAR(fl_erase4k_ram)(off, &took);
    irq_restore(f);
    return st_hw_rc(rc);
#endif
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
#if MELODEE_PROPHET_PROTOTYPE
    (void)off; (void)src; (void)n;
    return -8;
#else
    if (!flash_ok)                              /* (Round 27) as st_erase */
        return -6;
    if (!FL_STORE_OK(off, n))                   /* only inside the storage areas */
        return -8;
    {   /* (Round 27 re-review) the audio buffer cleared first, as st_erase does: a part slow to program would otherwise loop stale audio */
        uint32_t f = irq_save();
        audio_silence();
        irq_restore(f);
    }
    return st_hw_rc(fl_write(off, src, n));
#endif
}
