/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Bounded audio working-memory arena. Only the audio owner (or boot/host
 * setup before audio starts) mutates it. Fixed handles, no heap,
 * relocation, or searches proportional to the number of voices/samples.
 * Linker-derived SRAM tails are separate banks: an allocation never spans a
 * code/data/retained-state boundary. Small banks first preserve the large
 * contiguous POOL bank for engine state and note history. */
#define RES_JFX_N 67u                    /* (Orange Cat 0.6.0) the effects' blocks (jfx.c JB_*: the reverb's, the chorus's,
                                          * the SLICER's, then (rev 2) the delay's line in 16 blocks of 4 KB and the
                                          * reverb's model line in 8); (Release 2) DUB TAPE's: a state and a spring a track and a pool of 32 line blocks, 40 more */
enum { RES_ENGINE0, RES_NOTE_HISTORY = RES_ENGINE0 + NPART, RES_JFX0,
       RES_RECORD_UNDO = RES_JFX0 + RES_JFX_N,
       RES_HOT0, RES_HOT_BASE = RES_HOT0 + ENG_HOT_SLOTS,   /* (Orange Cat 0.13.7) the engines' hot sets (engines.c eng_hot_take); (0.14.0) the firmware's own (eng_hot_base) */
       RES_COUNT };
static uint8_t recording_undo_lost;
static uint8_t recording_undo_active, recording_undo_skip;
static struct { void *ptr; uint32_t size; } resource[RES_COUNT];
static uint32_t resource_peak, resource_failures;
/* (Orange Cat 0.6.0 rev 2) the card's 36 KB pool (card_play.c's cp_pool, defined here so the arena can lend it), four
 * slots of 9 KB: each slot the card does not use is lent to the effects (res_lend_ok bit s: the main loop's say,
 * usb_files.c). Each run of lent slots side by side is one more bank, for the effects' blocks only, tried before the
 * others (no engine can use it). Before the card uses a slot the main loop clears its bit and waits for its
 * res_lend_used bit to clear: the audio interrupt gives back every effect with a block there first (jfx.c
 * jfx_lend_check) */
#define CP_N 4u                                         /* streams at once */
#define CP_SLOT 4608u                                   /* 16-bit words a slot: 9 KB */
#define CP_POOL_DEFINED 1
static int16_t cp_pool[CP_N * CP_SLOT] __attribute__((section(".pool"), aligned(4)));
static volatile uint8_t res_lend_ok, res_lend_used;     /* bit s, slot s: the main loop's leave; a block there (audio's) */
#define CP_SLOT_BYTES (CP_SLOT * (uint32_t)sizeof(int16_t))
static volatile uint8_t res_jfx_yield;                  /* an engine found no memory while the effects held some */
#define RES_LEND_PUBLISH() __asm__ volatile("" ::: "memory")   /* (the main loop and the audio interrupt: one core) */
#define RES_IS_JFX(id) ((id) >= RES_JFX0 && (id) < RES_JFX0 + RES_JFX_N)
#ifdef FM1_IRQ_TARGET
extern uint8_t _resource_start[], _resource_end[];
extern uint8_t _resource_low_start[], _resource_low_end[];
extern uint8_t _resource_data_start[], _resource_data_end[];
extern uint8_t _resource_retained_start[], _resource_retained_end[];
extern uint8_t _resource_cache_start[], _resource_cache_end[];
#if MELODEE_CACHE_RAM
#define RESOURCE_CACHE_AVAILABLE (fm1_cache.status == FM1_CACHE_READY)
#else
#define RESOURCE_CACHE_AVAILABLE 0
#endif
#define RESOURCE_MAIN_CAPACITY ((uint32_t)((uintptr_t)_resource_end - (uintptr_t)_resource_start))
#else
/* the host's banks (words). (Orange Cat 0.6.0 rev 2) a test can set the device's sizes (tests/jfx_arena_test.c) */
#ifndef RES_HOST_MAIN_WORDS
#define RES_HOST_MAIN_WORDS 40960u
#define RES_HOST_LOW_WORDS 5120u
#define RES_HOST_DATA_WORDS 512u
#define RES_HOST_RETAINED_WORDS 1024u
#endif
#define RES_HOST_CACHE_WORDS 7168u
static uint32_t resource_host[RES_HOST_MAIN_WORDS];
static uint32_t resource_host_low[RES_HOST_LOW_WORDS], resource_host_data[RES_HOST_DATA_WORDS];
static uint32_t resource_host_retained[RES_HOST_RETAINED_WORDS];
static uint32_t resource_host_cache[RES_HOST_CACHE_WORDS], resource_host_cache_enabled;
#define RESOURCE_CACHE_AVAILABLE resource_host_cache_enabled
#define _resource_cache_start ((uint8_t *)resource_host_cache)
#define _resource_cache_end ((uint8_t *)(resource_host_cache + RES_HOST_CACHE_WORDS))
#define _resource_start ((uint8_t *)resource_host)
#define _resource_end ((uint8_t *)(resource_host + RES_HOST_MAIN_WORDS))
#define _resource_low_start ((uint8_t *)resource_host_low)
#define _resource_low_end ((uint8_t *)(resource_host_low + RES_HOST_LOW_WORDS))
#define _resource_data_start ((uint8_t *)resource_host_data)
#define _resource_data_end ((uint8_t *)(resource_host_data + RES_HOST_DATA_WORDS))
#define _resource_retained_start ((uint8_t *)resource_host_retained)
#define _resource_retained_end ((uint8_t *)(resource_host_retained + RES_HOST_RETAINED_WORDS))
#define RESOURCE_MAIN_CAPACITY ((uint32_t)sizeof resource_host)
#endif
/* The cache RAM's first UI_CACHE_BYTES are the UI's (ui.c: the undo levels), the rest the audio's: the audio's worst
 * case fits the SRAM banks alone (the linker's budget), so the cache RAM only ever adds to it */
#define UI_CACHE_BYTES 14336u
static uint8_t *ui_cache(void) { return RESOURCE_CACHE_AVAILABLE ? _resource_cache_start : 0; }
static const struct { uint8_t *start, *end; } resource_banks[] = {
    { _resource_data_start, _resource_data_end },
    { _resource_retained_start, _resource_retained_end },
    { _resource_low_start, _resource_low_end },
    { _resource_cache_start + UI_CACHE_BYTES, _resource_cache_end },
    { _resource_start, _resource_end }
};
#define RESOURCE_BANKS ((uint32_t)(sizeof resource_banks / sizeof resource_banks[0]))
enum { RESOURCE_CACHE_BANK = 3 };
/* the size of the bank block id lies in (0: none, or the lent card pool) */
static uint32_t resource_bank_size(uint32_t id)
{
    uintptr_t p = (uintptr_t)resource[id].ptr;
    for (uint32_t b = 0; b < RESOURCE_BANKS; b++) {
        if (b == RESOURCE_CACHE_BANK && !RESOURCE_CACHE_AVAILABLE) continue;
        if (p >= (uintptr_t)resource_banks[b].start && p < (uintptr_t)resource_banks[b].end)
            return (uint32_t)((uintptr_t)resource_banks[b].end - (uintptr_t)resource_banks[b].start);
    }
    return 0;
}
/* the card slots block id lies in (bit s: slot s), 0: not in the card pool */
static uint32_t resource_lent(uint32_t id)
{
    uintptr_t p = (uintptr_t)resource[id].ptr, b = (uintptr_t)cp_pool;
    uint32_t m = 0, s;
    if (!resource[id].size || p < b || p >= b + sizeof cp_pool) return 0;
    for (s = (uint32_t)(p - b) / CP_SLOT_BYTES; s < CP_N && b + s * CP_SLOT_BYTES < p + resource[id].size; s++)
        m |= 1u << s;
    return m;
}
#define RESOURCE_CAPACITY (RESOURCE_MAIN_CAPACITY + \
    (uint32_t)((uintptr_t)_resource_low_end - (uintptr_t)_resource_low_start) + \
    (uint32_t)((uintptr_t)_resource_data_end - (uintptr_t)_resource_data_start) + \
    (uint32_t)((uintptr_t)_resource_retained_end - (uintptr_t)_resource_retained_start) + \
    (RESOURCE_CACHE_AVAILABLE ? (uint32_t)((uintptr_t)_resource_cache_end - (uintptr_t)_resource_cache_start) - UI_CACHE_BYTES : 0u))
static uint32_t resource_used(void)
{
    uint32_t n = 0;
    for (uint32_t k = 0; k < RES_COUNT; k++) n += resource[k].size;
    return n;
}
static uint32_t resource_gen;                           /* (Release 2) counts the releases: DUB's instances ask again for memory only when something was freed */
static void resource_release(uint32_t id) { if (resource[id].size) resource_gen++; resource[id].size = 0; }
static __attribute__((noinline)) void *resource_allocate(uint32_t id, uint32_t bytes)
{
    if (bytes > RESOURCE_CAPACITY) { resource_failures++; return 0; }
    uint32_t size = (bytes + 3u) & ~3u;
    if (!size) { resource_release(id); return 0; }
    uint32_t lend = RES_IS_JFX(id) ? res_lend_ok & ((1u << CP_N) - 1u) : 0u;   /* (0.6.0 rev 2) the effects: the
                                                         * banks as everything else, then the lent card slots (last: so
                                                         * the card's memory is borrowed only when the arena is short,
                                                         * and a take loading cuts no effect it need not) */
    for (uint32_t step = 0; step < RESOURCE_BANKS + CP_N; step++) {
        uintptr_t at, limit;
        if (step >= RESOURCE_BANKS) {                   /* (a run of lent slots from slot c: not one of resource_banks) */
            uint32_t c = step - RESOURCE_BANKS, e = c;
            if (!((lend >> c) & 1u) || (c && ((lend >> (c - 1u)) & 1u))) continue;
            while (e < CP_N && ((lend >> e) & 1u)) e++;
            at = (uintptr_t)cp_pool + c * CP_SLOT_BYTES;
            limit = (uintptr_t)cp_pool + e * CP_SLOT_BYTES;
        } else {
            uint32_t bank = step;
            if (bank == RESOURCE_CACHE_BANK && !RESOURCE_CACHE_AVAILABLE) continue;
            at = (uintptr_t)resource_banks[bank].start;
            limit = (uintptr_t)resource_banks[bank].end;
        }
        /* Each collision advances past a live block; at most RES_COUNT passes.
         * Integer addresses permit distinct host arrays and disjoint SRAM banks. */
        for (uint32_t pass = 0; pass <= RES_COUNT; pass++) {
            if (at > limit || size > limit - at) break;
            uintptr_t next = at;
            for (uint32_t k = 0; k < RES_COUNT; k++) {
                if (k == id || !resource[k].size) continue;
                uintptr_t begin = (uintptr_t)resource[k].ptr;
                uintptr_t end = begin + resource[k].size;
                if (at < end && at + size > begin && end > next) next = end;
            }
            if (next == at) {
                void *p = (void *)at;
                resource[id].ptr = p; resource[id].size = size;
                if (step >= RESOURCE_BANKS) res_lend_used |= (uint8_t)resource_lent(id);
                uint32_t *words=p;
                for (uint32_t j=0;j<size/4u;j++)words[j]=0;
                uint32_t used = resource_used();
                if (used > resource_peak) resource_peak = used;
                return p;
            }
            at = next;
        }
    }
    resource_failures++;
    return 0;                          /* leave an existing allocation intact */
}

/* Keep the already allocated path cheap in every engine accessor. */
static inline void *resource_get(uint32_t id, uint32_t bytes)
{
    uint32_t size=(bytes+3u)&~3u;
    if(size && size==resource[id].size)return resource[id].ptr;
    void *p=resource_allocate(id,bytes);
    /* A history snapshot must never prevent a voice/effect from acquiring its
     * working memory. It is disposable, unlike sounding engine state. */
    if(!p && bytes && bytes<=RESOURCE_CAPACITY && id!=RES_RECORD_UNDO && resource[RES_RECORD_UNDO].size){
        resource_release(RES_RECORD_UNDO);recording_undo_lost=1;
        recording_undo_skip|=recording_undo_active;recording_undo_active=0;
        p=resource_allocate(id,bytes);
    }
    /* (Orange Cat 0.6.0 rev 2) an engine first: the effects give back theirs at the next block (jfx.c jfx_lend_check),
     * when one of their blocks is in a bank that could hold this engine's state (else giving it back cannot help) */
    if(!p && bytes && id<RES_NOTE_HISTORY){
        for(uint32_t k=RES_JFX0;k<RES_JFX0+RES_JFX_N;k++)if(resource[k].size&&!resource_lent(k)&&resource_bank_size(k)>=bytes){res_jfx_yield=1;break;}
    }
    return p;
}
