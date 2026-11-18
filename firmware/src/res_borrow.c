/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Round 30 item 1, BUILD.md 7.47) The stopped-transport borrow of the audio arena. Included by mix.c after fx_insert.c (OC_BORROW only).
 *
 * The arena (resources.c) is the audio interrupt's. A main-loop operation that needs a large scratch only while it runs (the loader's save: the record's sections, the
 * project part, the format core's work area; oc_pj4_pool.c) borrows it here instead of keeping it resident in .pool, so the linker's worst-case budget for the tracks
 * (_resource_minimum: four of the largest engine states) is kept while playing: the borrow is never granted while the transport runs, and the transport does not start
 * while it is held.
 *
 * The protocol (one borrow at a time; the main loop's only):
 *   1. res_borrow(sizes, n, out): the main loop sets res_bor_hold, then res_bor_st = ASK, and waits (bounded) for the interrupt's answer.
 *   2. The interrupt, at the start of the next block (jfx_lend_check, CPU0, no bus job out): the transport running (or a start pending) -> NO_PLAY. Else FADE: the
 *      buses' returns (chorus, delay, reverb) fade to 0 over RES_BOR_FADE blocks (about 31 ms, a ramp a sample: no click), every DUB instance ramps its wet out and lets
 *      go (fx_insert.c, as for CPU1's loss: FX_RAMP blocks), the rising cap's blocks go back (voice.c rise_want 0: their voices faded by voice_kill), and nothing of
 *      the effects is taken (jfx_wait). Then the buses' memory and the SLICER's / FX layer's buffer are given back, as at rest (jfx_free_buses: the tails are faded,
 *      then cut at silence), and TAKE: one piece a block (bounded zeroing in the interrupt), largest first as the caller orders them; short, the disposable histories
 *      (the recording's undo, the last note edit's undo: both are dropped whenever PLAY starts anyway) go and the piece is tried again; still short -> NO_MEM, every
 *      piece back. All taken -> HELD.
 *   3. The tracks' engine states and the hot sets are never touched: a key (or a MIDI note) while the save runs plays on its held state; only an engine change
 *      that needs new memory can find none until the end (its track is silent until then, as when the arena is short).
 *   4. res_borrow_end(): the main loop gives every piece back (IRQs off) and clears the hold; the effects take their memory again as after a stop (from silence), DUB's
 *      instances ask again, the rising cap takes blocks when notes need them. PLAY, MIDI Start / Continue, the chain or a scene pressed meanwhile start then (seq.c).
 */
#define RES_BOR_FADE ((uint32_t)FS / CTL / 32u)          /* blocks the buses' returns fade over (43: 31 ms) */
#define RES_BOR_WAIT_MS 500u                            /* the main loop's wait for the answer (the fade, DUB's ramp, the pieces: about 50 blocks, 37 ms) */
enum { RES_BOR_OK = 0, RES_BOR_PLAYING = 1, RES_BOR_FULL = 2, RES_BOR_LATE = 3, RES_BOR_BUSY = 4 };
static uint16_t res_bor_fade;
static uint8_t res_bor_next;
static uint32_t res_bor_grants, res_bor_no_play, res_bor_no_mem, res_bor_late, res_bor_dropped;   /* (AUDIO_STATS / the tests) */
#ifndef FM1_IRQ_TARGET
static void (*res_bor_host_block)(void);                /* a host test: one audio block while the main loop waits (default: jfx_lend_check, where the step runs) */
#endif

static int res_bor_transport_busy(void);               /* seq.c: playing, counting in, the chain armed or running, a start pending */
static uint32_t jfx_bor_gain_to(void)
{
    if (res_bor_st == BOR_FADE) return (uint32_t)res_bor_fade * 32768u / RES_BOR_FADE;
    return RES_BOR_ON() ? 0u : 32768u;
}
/* nothing of DUB's sounding or ramping, no rising block held: the effects' memory can go */
static int res_bor_quiet(void)
{
    uint32_t k;
    for (k = 0; k < FX_INS_N; k++)
        if (fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP || fx_ins[k].state == FXS_CLEAR) return 0;
#if MELODEE_MACRO
    for (k = 0; k < NPART; k++) if (rise_have[k]) return 0;
#endif
    return 1;
}
static void jfx_bor_block(void)
{
    uint32_t st = res_bor_st;
    if (st == BOR_ASK) {
        if (res_bor_transport_busy()) {
            res_bor_no_play++;
            res_bor_st = BOR_NO_PLAY;
            return;
        }
        res_bor_fade = (uint16_t)RES_BOR_FADE;
        res_bor_st = BOR_FADE;
        st = BOR_FADE;
    }
    if (!RES_BOR_ON()) {
        jfx_bor_g = 32768u;
        return;
    }
    if (jfx_wait < 2u) jfx_wait = 2u;                   /* (no effect takes a block meanwhile: jfx_take, the SLICER's buffer) */
    if (st == BOR_FADE) {
        if (res_bor_fade) { res_bor_fade--; return; }
        if (!res_bor_quiet()) return;
        jfx_bus_drain();                                /* (no job out: the buses' memory may go) */
        jfx_free_buses();
        jfx_free_ins_all();
        if (sl_buf) jfx_sl_lost();
        res_bor_next = 0;
        res_bor_st = BOR_TAKE;
        return;
    }
    if (st == BOR_TAKE) {
        uint32_t i = res_bor_next;
        while (i < RES_BOR_N && !res_bor_size[i]) i++;
        if (i < RES_BOR_N) {
            void *p = resource_allocate(RES_BOR0 + i, res_bor_size[i]);
            if (!p && (resource[RES_RECORD_UNDO].size || resource[RES_NOTE_HISTORY].size)) {   /* the disposable histories first (resource_get's rule) */
                if (resource[RES_RECORD_UNDO].size) {
                    resource_release(RES_RECORD_UNDO); recording_undo_lost = 1;
                    recording_undo_skip |= recording_undo_active; recording_undo_active = 0;
                }
                resource_release(RES_NOTE_HISTORY);
                res_bor_dropped++;
                p = resource_allocate(RES_BOR0 + i, res_bor_size[i]);
            }
            if (!p) {
                for (uint32_t k = 0; k < RES_BOR_N; k++) resource_release(RES_BOR0 + k);
                res_bor_no_mem++;
                res_bor_st = BOR_NO_MEM;
                return;
            }
            res_bor_next = (uint8_t)(i + 1u);
            return;
        }
        res_bor_grants++;
        res_bor_st = BOR_HELD;
    }
}

/* ------------------------------------------------------------ the main loop's --- */
/* every piece back, the hold cleared: the effects take their memory again as after a stop */
static void res_borrow_end(void)
{
#ifdef FM1_IRQ_TARGET
    fm1_irq_off();                                      /* (one core: the interrupt sees all of it or none) */
#endif
    for (uint32_t k = 0; k < RES_BOR_N; k++) resource_release(RES_BOR0 + k);
    res_bor_st = BOR_IDLE;
    res_bor_hold = 0;
#ifdef FM1_IRQ_TARGET
    fm1_irq_on();
#endif
}
/* The main loop only, never from the audio interrupt, one at a time. sizes[0..n): the pieces' bytes (0: none; n <= RES_BOR_N), largest first. Returns RES_BOR_OK with
 * out[i] each piece (zeroed, 4-byte aligned, 0 for a size 0), held until res_borrow_end(); or not granted, nothing held: RES_BOR_PLAYING (the transport runs or a start
 * is pending: the caller waits for the stop), RES_BOR_FULL (the arena cannot give it even with every effect's memory back), RES_BOR_LATE (the audio did not answer),
 * RES_BOR_BUSY (a borrow is held) */
static int res_borrow(const uint32_t *sizes, uint32_t n, uint8_t **out)
{
    uint32_t k, st, t0, spins = 0;
    if (res_bor_hold || res_bor_st != BOR_IDLE || n > RES_BOR_N) return RES_BOR_BUSY;
    for (k = 0; k < RES_BOR_N; k++) { res_bor_size[k] = k < n ? sizes[k] : 0u; if (k < n) out[k] = 0; }
    res_bor_hold = 1;
    RES_LEND_PUBLISH();
    res_bor_st = BOR_ASK;
    t0 = fm1_ms;
    for (;;) {
        st = res_bor_st;
        if (st != BOR_ASK && st != BOR_FADE && st != BOR_TAKE) break;
#ifdef FM1_IRQ_TARGET
        if (fm1_ms - t0 >= RES_BOR_WAIT_MS) break;
        fm1_wdt_feed();
#else
        if (res_bor_host_block) res_bor_host_block(); else jfx_lend_check();
        if (++spins > 20000u) break;                    /* (the host's bound: blocks, not ms) */
        (void)t0;
#endif
    }
    st = res_bor_st;
    if (st == BOR_HELD) {
        for (k = 0; k < n; k++) out[k] = res_bor_size[k] ? (uint8_t *)resource[RES_BOR0 + k].ptr : 0;
        return RES_BOR_OK;
    }
    if (st != BOR_NO_PLAY && st != BOR_NO_MEM) res_bor_late++;
    res_borrow_end();
    return st == BOR_NO_PLAY ? RES_BOR_PLAYING : st == BOR_NO_MEM ? RES_BOR_FULL : RES_BOR_LATE;
}
/* At boot, before the audio starts (persist_boot): the pieces taken at once (nobody else allocates yet). RES_BOR_OK or RES_BOR_FULL; given back by res_borrow_end */
static int res_borrow_boot(const uint32_t *sizes, uint32_t n, uint8_t **out)
{
    if (res_bor_hold || res_bor_st != BOR_IDLE || n > RES_BOR_N) return RES_BOR_BUSY;
    res_bor_hold = 1;
    res_bor_st = BOR_HELD;
    for (uint32_t k = 0; k < n; k++) {
        out[k] = sizes[k] ? resource_allocate(RES_BOR0 + k, sizes[k]) : 0;
        if (sizes[k] && !out[k]) { res_borrow_end(); return RES_BOR_FULL; }
    }
    return RES_BOR_OK;
}
