/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.14.2, linux-hw) What the two cores do, a half at a time (OC_CORE_STATS=1 builds only; every other build has empty stubs).
 *
 * The audio interrupt renders a half (128 frames, the 2,902 us deadline) as four 32-sample blocks (audio.c). Each block: the events, every
 * track (its engine on CPU0, or its voices partly on CPU1 through the batch, audio_batch.h; its DIST, mute, level, pan and sends on CPU0),
 * then the tail: the send buses (CHORUS, DELAY, REVERB) as one CPU1 job started only after the last track, CPU0 waiting for it at the
 * join, then the master chain on CPU0 (SLICER, DJ FILTER, CLIP, the FX layer, the output stage, USB capture). Per half, summed over its
 * four blocks, timed with TIMER4 (24 ticks a microsecond, the same counter on both cores):
 *   wall       the interrupt's whole render (audio.c t0 to the end of its last block): what Worst reports
 *   cpu0       CPU0's work from each block's start to its last track done, the waits inside it taken out
 *   cpu1       the CPU1 jobs that finished in the half, as CPU1 timed each one (audio_worker.h), and by kind: voices (the batch, FM6,
 *              the Prophet), bus (the effects' buses), dub (the DUB TAPE bench, OC_DUB_BENCH builds)
 *   wait       CPU0 spinning in audio_worker_join, in all; tail_wait the part after the tracks (the buses)
 *   tail       CPU0's work from the last track done to the block's end, its waits taken out: the serial tail
 *   idle1      wall - cpu1: CPU1 not working during the render
 * Each: min, mean, max and a histogram of 32 us bins to 4,096 us (p99). Counters: halves, jobs by kind, submits refused while CPU1 was
 * busy, the worker retired (fault, stall or misread), single-core forced. A few TIMER4 reads a block and one histogram update a metric a
 * half; CPU1 stores one word a job. Read with AUDIO_STATS (command 72) [flags, 1]: schema 9 (web/EDITOR_PROTOCOL.md).
 * (0.14.3) The batch's voice kernels (audio_batch.h), each timed by the core that ran it, by profile id and core: count, sum, max (the
 * per-voice costs audio_batch_cost is set from); CPU1's stack high-water mark (its 8 KB, from the top, where its jobs run: painted when
 * CPU1 starts and at each reset while it is idle, the top 512 B, the polling loop's, not painted; 0 until painted). AUDIO_STATS
 * [flags, 3]: schema 10. */
#pragma once
#include <stdint.h>
#include "audio_profile.h"
#ifndef OC_CORE_STATS
#define OC_CORE_STATS 0
#endif
enum { CS_WALL, CS_CPU0, CS_CPU1, CS_VOICE, CS_BUS, CS_DUB, CS_WAIT, CS_TAILWAIT, CS_TAIL, CS_IDLE1, CS_N };
enum { CS_K_VOICE, CS_K_BUS, CS_K_DUB };
#if OC_CORE_STATS
#include "../hal/fm1_time.h"
#define CS_BINS 128u
#define CS_BIN_TICKS (32u * 24u)
typedef struct { uint32_t min, max, n, hist[CS_BINS]; uint64_t sum; } cs_metric_t;
static cs_metric_t cs_m[CS_N] __attribute__((section(".pool")));
static uint32_t cs_half[CS_N];                         /* this half's sums, ticks */
static uint32_t cs_t_blk, cs_t_trk, cs_w_blk, cs_in_tail;
static uint32_t cs_halves, cs_jobs[3], cs_refused, cs_retired;
static uint8_t cs_kind, cs_single;                     /* the kind of the job about to be submitted; 1: CPU1 never asked (single-core forced) */
typedef struct { uint32_t n, max; uint64_t sum; } cs_kn_t;
/* (round 6) the per-block trace, armed by AUDIO_STATS [flags, 10, 100]: the next CS_TR_JOBS worker jobs and CS_TR_BLKS blocks with their TIMER4 ticks (the same counter on both cores): a job's kind
 * (0 voices and DUB, 1 bus, 2 dub), the submit on CPU0, the start and end on CPU1, CPU0's join (start, end); a block's begin, tracks done, end. One shot: it stops when the jobs are full. */
#define CS_TR_JOBS 64u
#define CS_TR_BLKS 40u
static uint32_t cs_tr_job[CS_TR_JOBS][6] __attribute__((section(".pool")));
static uint32_t cs_tr_blk[CS_TR_BLKS][3] __attribute__((section(".pool")));
static volatile uint32_t cs_tr_arm, cs_tr_n, cs_tr_nb, cs_tr_cur = 0xFFFFu;
static cs_kn_t cs_k[AP_COUNT][2] __attribute__((section(".pool")));   /* (0.14.3) the batch's kernels: [profile id][CPU1] */
static inline void cs_kernel(uint32_t id, uint32_t cpu1, uint32_t ticks)
{
    if (id >= AP_COUNT) return;
    cs_k[id][cpu1 & 1u].n++; cs_k[id][cpu1 & 1u].sum += ticks;
    if (ticks > cs_k[id][cpu1 & 1u].max) cs_k[id][cpu1 & 1u].max = ticks;
}
/* CPU1's stack high-water mark: cs_stack_paint, cs_stack_used (hal/fm1_multicore.h, after the mailbox) */
#define CS_TICKS() fm1_ticks()
static inline void cs_blk_begin(void)
{
    cs_t_blk = CS_TICKS(); cs_w_blk = 0; cs_in_tail = 0;
    if (cs_tr_arm && cs_tr_nb < CS_TR_BLKS) cs_tr_blk[cs_tr_nb][0] = cs_t_blk;
}
static inline void cs_tracks_done(void)
{
    uint32_t t = CS_TICKS();
    cs_half[CS_CPU0] += t - cs_t_blk - cs_w_blk;
    cs_t_trk = t; cs_w_blk = 0; cs_in_tail = 1;
    if (cs_tr_arm && cs_tr_nb < CS_TR_BLKS) cs_tr_blk[cs_tr_nb][1] = t;
}
static inline void cs_blk_end(void)
{
    uint32_t t = CS_TICKS();
    cs_half[CS_TAIL] += t - cs_t_trk - cs_w_blk; cs_in_tail = 0;
    if (cs_tr_arm && cs_tr_nb < CS_TR_BLKS) { cs_tr_blk[cs_tr_nb][2] = t; cs_tr_nb++; }
}
/* audio_worker_join: CPU0 waited w ticks; the job (CPU1's ticks: job, 0 when it failed) was of kind cs_kind */
static inline void cs_join(uint32_t w, uint32_t job)
{
    cs_w_blk += w;
    cs_half[CS_WAIT] += w;
    if (cs_in_tail) cs_half[CS_TAILWAIT] += w;
    cs_half[CS_CPU1] += job;
    cs_half[cs_kind == CS_K_BUS ? CS_BUS : cs_kind == CS_K_DUB ? CS_DUB : CS_VOICE] += job;
    cs_jobs[cs_kind % 3u]++;
    cs_kind = CS_K_VOICE;
}
static void cs_metric(cs_metric_t *m, uint32_t v)
{
    uint32_t b = v / CS_BIN_TICKS;
    if (!m->n || v < m->min) m->min = v;
    if (v > m->max) m->max = v;
    m->n++; m->sum += v;
    m->hist[b < CS_BINS ? b : CS_BINS - 1u]++;
}
/* audio.c, at the end of a half: wall ticks */
static void cs_half_end(uint32_t wall)
{
    cs_half[CS_WALL] = wall;
    cs_half[CS_IDLE1] = wall > cs_half[CS_CPU1] ? wall - cs_half[CS_CPU1] : 0u;
    for (uint32_t k = 0; k < CS_N; k++) { cs_metric(&cs_m[k], cs_half[k]); cs_half[k] = 0; }
    cs_halves++;
}
static void cs_reset(void)
{
    for (uint32_t k = 0; k < CS_N; k++) { cs_m[k].min = cs_m[k].max = cs_m[k].n = 0; cs_m[k].sum = 0; for (uint32_t b = 0; b < CS_BINS; b++) cs_m[k].hist[b] = 0; }
    for (uint32_t k = 0; k < AP_COUNT; k++) cs_k[k][0].n = cs_k[k][0].max = cs_k[k][1].n = cs_k[k][1].max = 0, cs_k[k][0].sum = cs_k[k][1].sum = 0;
    cs_halves = cs_refused = cs_retired = 0;
    cs_jobs[0] = cs_jobs[1] = cs_jobs[2] = 0;
}
#else
static inline void cs_kernel(uint32_t id, uint32_t cpu1, uint32_t ticks) { (void)id; (void)cpu1; (void)ticks; }
static inline void cs_blk_begin(void) {}
static inline void cs_tracks_done(void) {}
static inline void cs_blk_end(void) {}
static inline void cs_join(uint32_t w, uint32_t job) { (void)w; (void)job; }
#endif
