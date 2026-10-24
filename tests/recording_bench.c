/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* Build with MELODEE_RECORD_CURSOR=0 and =1. Measures the real sequencer,
 * excluding synthesis; host instruction counts are not FM-1 CPU percentages.
 *
 * (Orange Cat 0.13.4, Hardening 2) `recording_bench props` is the scheduler under load, for the unit test of audio timing: every track (NTRK, four here) has every
 * one of its 64 steps recorded with four notes (1,024 events, RECORD_MAX), the loop played block by block through seq_advance(CTL), once PLAIN (the recorded
 * notes alone: no step property) and once LOADED (every property on, on every step: accent, slide, chance 90, trig condition NOT FILL, a nudge of -24..+24, and
 * RATCH x2/x3/x4 or FLAM in turn, so 4 notes play 8 to 16 hits a step). It reports the hits a loop plays and the cost of one block: the mean and the worst of
 * a loop, the best of several repeats (the least disturbed), in ns and, where the host can count them (macOS rusage, Linux perf_event_open), instructions.
 * The two lines end in the ratio. Host numbers; the FM-1 core is not this machine, so the ratio is what carries over.
 *
 * Revisions:
 *   0.13.4 `props` mode (idle, plain, loaded), the hit counter, Linux perf_event_open and the callgrind switch.
 *   0.13.5 reports recording_rep_over (repeats finished early because the slots were full: 0 in both cases). */
#define MIDI_TIMING_NO_MAIN 1
#include <stdint.h>
static volatile uint32_t bench_hits;                    /* (0.13.4) the hits the scheduler played (recording.c's hook) */
#define RECORDING_HIT_HOOK(t, i, vel, gate, late) (bench_hits++)
#include "midi_timing_test.c"
#ifdef BENCH_CALLGRIND
#include <valgrind/callgrind.h>              /* (0.13.4) instruction counts where the kernel will not give them: valgrind --tool=callgrind --collect-atstart=no ./recording_bench props plain */
#endif
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
static int bench_perf_fd = -2;
#endif
static uint64_t bench_instructions(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 r;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&r)) return r.ri_instructions;
#endif
#ifdef __linux__
    if (bench_perf_fd == -2) {                         /* (0.13.4) user-space instructions of this process, when the kernel lets it count (perf_event_paranoid) */
        struct perf_event_attr a;
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE; a.size = sizeof a; a.config = PERF_COUNT_HW_INSTRUCTIONS; a.exclude_kernel = 1; a.exclude_hv = 1;
        bench_perf_fd = (int)syscall(SYS_perf_event_open, &a, 0, -1, -1, 0);
    }
    if (bench_perf_fd >= 0) {
        uint64_t v = 0;
        if (read(bench_perf_fd, &v, sizeof v) == (ssize_t)sizeof v) return v;
    }
#endif
    return 0;
}
#ifndef MELODEE_RECORD_QUEUE
#define MELODEE_RECORD_QUEUE 0
#endif
static volatile uint32_t bench_checksum;

/* ---- (0.13.4) the scheduler under load: PLAIN against LOADED, all tracks, every step recorded with four notes */
static void props_fixture(int mode)                       /* 0 plain, 1 loaded, 2 idle (the same steps, no recorded events: the fixed cost of a block) */
{
    int loaded = mode == 1;
    rng_state = 0x1234567u;                                   /* the chance rolls are the same ones every run */
    uint32_t n = 0;
    timing_reset(2, 24000000u);
    timing_packet(timing_now, 0xFA, 0, 0, 2);
    timing_packet(timing_now, 0xF8, 0, 0, 2);
    events_block(0);
    song.rec_pass = 0;
    seq_stop(); song.rec = 0;
    recording_reset();
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        t->p[P_SLEN] = 64; t->p[P_VOICE] = V_POLY; t->p[P_SDIV] = 2; t->p[P_RECQ] = 0;
        for (uint32_t i = 0; i < NSTEP; i++) {
            step_t *st = &t->step[i];
            memset(st, 0, sizeof *st);
            st->time = ST_NOTE; st->flags = SF_RECORDED;
            if (loaded) {
                static const uint8_t RF[6] = {4, 2, 5, 3, 4, 4};                       /* x4, x2, FLAM, x3, x4, x4 */
                st->flags = (uint8_t)(SF_RECORDED | (i % 3u == 0 ? SF_ACCENT : 0u) | (i % 5u == 0 ? SF_SLIDE : 0u));
                st->probability = 90; st->cond = TC_NFILL; st->nudge = (int8_t)((int)(i % 7u) * 8 - 24);
                step_set_rflam(st, RF[(i + k) % 6u]);
            }
            for (uint32_t q = 0; q < 4; q++) {
                recorded_note_t r = {0};
                r.on = (uint16_t)(q * 16384u + (i % 4u) * 1000u); r.duration = 8192; r.note = (uint8_t)(36u + q * 4u + k); r.vel = 100; r.step = (uint8_t)i;
                r.owner = (uint8_t)(k * NPAT);
                if (mode != 2) recording_restore_note(t, n++, r);
            }
        }
    }
    for (uint32_t k = 0; k < NTRK; k++) (void)recording_prefix(&trk[k], seq_div_samples((uint32_t)trk[k].p[P_SDIV]), 64u);   /* the index the scheduler reads */
    seq_start(); seq_advance(0);
}
typedef struct { double mean_ns, worst_ns, mean_ins; uint32_t hits; } props_res_t;
static props_res_t props_run(int mode, int reps)
{
    props_res_t best = {1e30, 1e30, 0, 0};
    for (int rep = 0; rep < reps; rep++) {
        props_fixture(mode);
        uint32_t period = seq_div_samples(trk[0].p[P_SDIV]), blocks = 64u * period / CTL + 1u;
        for (uint32_t i = 0; i < 256u; i++) seq_advance(CTL);                     /* warm: the first pass reads the events in */
        bench_hits = 0;
#ifdef BENCH_CALLGRIND
        CALLGRIND_START_INSTRUMENTATION;
#endif
        uint64_t i0 = bench_instructions(), t0 = now_ns(), worst = 0;
        for (uint32_t i = 0; i < blocks; i++) {
            uint64_t a = now_ns();
            seq_advance(CTL);
            uint64_t d = now_ns() - a;
            if (d > worst) worst = d;
        }
        uint64_t ns = now_ns() - t0, ins = bench_instructions() - i0;
#ifdef BENCH_CALLGRIND
        CALLGRIND_STOP_INSTRUMENTATION;
        fprintf(stderr, "blocks measured: %u, hits: %u, repeat slots finished early: %u\n", blocks, (unsigned)bench_hits, (unsigned)recording_rep_over);
#endif
        if ((double)ns / blocks < best.mean_ns) { best.mean_ns = (double)ns / blocks; best.mean_ins = (double)ins / blocks; best.hits = bench_hits; }
        if (rep == 0 || (double)worst < best.worst_ns) best.worst_ns = (double)worst;   /* the least disturbed repeat's worst block (an interrupted one is the machine's, not the scheduler's) */
    }
    return best;
}
static int props_bench(const char *only)
{
    if (only) { (void)props_run(!strcmp(only, "loaded") ? 1 : !strcmp(only, "idle") ? 2 : 0, 1); return 0; }
    props_res_t i = props_run(2, 7), a = props_run(0, 7), b = props_run(1, 7);
    double blocks = 64.0 * seq_div_samples(trk[0].p[P_SDIV]) / CTL;
    printf("scheduler under load: %u tracks x 64 steps x 4 notes = %u recorded events (RECORD_MAX %u), one loop of 64 steps played in blocks of %u samples\n", (unsigned)NTRK, (unsigned)(NTRK * 64u * 4u), (unsigned)RECORD_MAX, (unsigned)CTL);
    printf("  case,hits_per_loop,mean_ns_per_block,worst_ns_per_block,mean_instructions_per_block\n");
    printf("  idle,%u,%.1f,%.1f,%.1f\n", i.hits, i.mean_ns, i.worst_ns, i.mean_ins);
    printf("  plain,%u,%.1f,%.1f,%.1f\n", a.hits, a.mean_ns, a.worst_ns, a.mean_ins);
    printf("  loaded,%u,%.1f,%.1f,%.1f\n", b.hits, b.mean_ns, b.worst_ns, b.mean_ins);
    printf("  above idle, ns per hit: plain %.1f, loaded %.1f (loaded / plain %.3f); ns per block: plain %.1f, loaded %.1f (loaded / plain %.3f); hits a block: %.2f and %.2f\n",
           (a.mean_ns - i.mean_ns) * blocks / a.hits, (b.mean_ns - i.mean_ns) * blocks / b.hits, ((b.mean_ns - i.mean_ns) / b.hits) / ((a.mean_ns - i.mean_ns) / a.hits),
           a.mean_ns - i.mean_ns, b.mean_ns - i.mean_ns, (b.mean_ns - i.mean_ns) / (a.mean_ns - i.mean_ns), a.hits / blocks, b.hits / blocks);
    return 0;
}
int main(int argc, char **argv)
{
    if(argc>1 && !strcmp(argv[1],"props"))return props_bench(argc>2?argv[2]:0);
    int sustained=argc>1 && !strcmp(argv[1],"sustained");
    puts("cursor,queue,sustained,quant,tracks,notes_per_step,stored_records,event_hash,capture_instructions_per_note,instructions_per_tick,ns_per_tick");
    for(uint32_t q=0;q<=3;q+=3) for (uint32_t tracks=1; tracks<=4; tracks*=4) for(uint32_t notes=1;notes<=4;notes*=4) {
        timing_reset(0,24000000u);song.rec=(uint8_t)((1u<<tracks)-1u);
        for(uint32_t k=0;k<NTRK;k++){
            track_t *t=&trk[k];t->p[P_RECQ]=(int16_t)q;t->p[P_SLEN]=64;t->p[P_VOICE]=V_POLY;
            memset(t->step,0,sizeof t->step);for(uint32_t i=0;i<NSTEP;i++)t->step[i].time=ST_REST;
        }
        seq_start();seq_advance(0);song.rec_pass=0;
        uint32_t period=seq_div_samples(TSEL->p[P_SDIV]);
        uint64_t capture_start=bench_instructions();
        for(uint32_t i=0;i<64;i++){
            seq_advance(period/4u+512u);
            for(uint32_t k=0;k<tracks;k++)for(uint32_t n=0;n<notes;n++)input_on(&trk[k],48+n*4,90+n*8);
            seq_advance(period/8u);
            for(uint32_t k=0;k<tracks;k++)for(uint32_t n=0;n<notes;n++)input_off(&trk[k],48+n*4);
            seq_advance(period-period/4u-512u-period/8u);
        }
        seq_stop();song.rec=0;
        double capture_cost=(double)(bench_instructions()-capture_start)/(64u*tracks*notes);
        /* Stress the active-list bound too: keep events sounding for nearly
         * two complete loops rather than the phrase's short captured gates. */
        if(sustained)for(uint32_t i=0;i<RECORD_MAX;i++)if(recording[i].vel){recording[i].duration=65535;recording[i].owner|=224u;}
        uint32_t stored=0;for(uint32_t i=0;i<RECORD_MAX;i++)stored+=recording[i].vel!=0;
        uint32_t event_hash=fnv(2166136261u,recording,sizeof recording);
        /* Optional canonical fixture dump for the host-only storage probe.
         * Explicit fields avoid dependence on C padding or host byte order. */
        if(argc>2){
            char path[512];snprintf(path,sizeof path,"%s/q%u-t%u-n%u-s%d.bin",argv[2],q,tracks,notes,sustained);
            FILE *f=fopen(path,"wb");if(!f)return 2;
            for(uint32_t i=0;i<RECORD_MAX;i++)if(recording[i].vel){
                const recorded_note_t *r=&recording[i];
                uint8_t b[10]={r->on,r->on>>8,r->duration,r->duration>>8,r->note,r->vel,r->owner,r->step,(uint8_t)r->pitch,r->length};
                if(fwrite(b,1,sizeof b,f)!=sizeof b){fclose(f);return 2;}
            }
            if(fclose(f))return 2;
        }
        seq_start();for(uint32_t i=0;i<1024;i++)seq_advance(CTL);
        uint32_t ticks=50000;
        uint64_t c0=bench_instructions(),n0=now_ns();
        for(uint32_t i=0;i<ticks;i++)seq_advance(CTL);
        uint64_t ns=now_ns()-n0,ins=bench_instructions()-c0;
        bench_checksum^=trk[0].seq_pos+trk[0].seq_n;
        printf("%u,%u,%d,%u,%u,%u,%u,%08x,%.1f,%.1f,%.1f\n",MELODEE_RECORD_CURSOR,MELODEE_RECORD_QUEUE,sustained,q,tracks,notes,stored,event_hash,capture_cost,(double)ins/ticks,(double)ns/ticks);
        seq_stop();
    }
    return 0;
}
