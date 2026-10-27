/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Orange Cat 0.14.3) the ports on the batch (audio_batch.h; docs/orangecat/BUILD.md, "The scheduler, stage 1: the ports on the batch").
 * Four tracks of MACRO (VA BASS, MALLET, and models that draw plaits' shared generator: rendered at once on CPU0), DRUM-X (a factory kit
 * and one with lanes panned and PUNCH / ATK+: the side's merge), SHAPES (analog shapes in the batch, a digital one at once) and WHEEL (in
 * slot 7 as its module is: a voice kernel each), with the LFO, SPREAD and chords that come and go, a MACRO model and a SHAPES shape
 * changed while voices sound, an engine switched. Each case renders three ways, the
 * complete mixer's output compared sample for sample: PB_REF builds (MELODEE_BATCH_AUDIO=0: every voice rendered at its turn, as before
 * 0.14.3) write the reference; then the batch with CPU1 refused (every kernel on CPU0, at the flush) and the batch on a concurrent host
 * worker (CPU1 renders its share) must equal it. Also: every port's kernels went to CPU1 at least once; MODAL and VA left plaits'
 * generator alone.
 *   port_batch_test REF.bin           (PB_REF: write it; otherwise: compare with it) */
#include <stdlib.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include <assert.h>
#include <sched.h>
#ifdef PB_REF
#define MELODEE_BATCH_AUDIO 0
#endif
static uint32_t worker_clock(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + ts.tv_nsec / 1000u);
}
#define MELODEE_DUAL_CORE 1
#define AW_WORD _Atomic uint32_t
#define AW_LOAD(p) atomic_load_explicit(p, memory_order_acquire)
#define AW_STORE(p,v) atomic_store_explicit(p,v,memory_order_release)
#define AW_TICKS() worker_clock()
#define AW_TIMEOUT_TICKS 2000000u
#define AW_RAM_LOOP static
#define AW_IDLE() pthread_testcancel()
static void hold_worker(void);
#define AW_HOLD() hold_worker()
#include "../firmware/src/audio_worker.h"
static void hold_worker(void)
{
    while (AW_LOAD(&audio_worker.complete) != AW_LOAD(&audio_worker.request)) sched_yield();
}
static int serial_render;
#define audio_worker_submit(fn,context) (serial_render ? 0 : audio_worker_submit(fn,context))
#define main hostsim_main
#include "hostsim.c"
#undef main

#define BLOCKS 320u
typedef struct { uint8_t e[4]; const char *pre[4]; uint8_t pan, spread; } case_t;
static const case_t CASES[] = {
    {{ENGI_MACRO, ENGI_MACRO, ENGI_MACRO, ENGI_MACRO}, {"VA BASS", "VA BASS", "VA BASS", "VA BASS"}, 0, 0},
    {{ENGI_MACRO, ENGI_MACRO, ENGI_MACRO, ENGI_MACRO}, {"MALLET", "MALLET", "MALLET", "MALLET"}, 0, 1},
    {{ENGI_MACRO, ENGI_MACRO, ENGI_MACRO, ENGI_DRUMX}, {"MALLET", "NOISE WIND", "SNARE", "TECHNO RAW"}, 1, 0},
    {{ENGI_DRUMX, ENGI_DRUMX, ENGI_DRUMX, ENGI_DRUMX}, {"TECHNO RAW", "TECHNO RAW", "909 CLUB", "TECHNO RAW"}, 1, 0},
    {{ENGI_SHAPES, ENGI_SHAPES, ENGI_SHAPES, ENGI_MACRO}, {"CSAW BASS", "MORPH LEAD", "VOWEL", "VA BASS"}, 0, 1},
    {{ENGI_WHEEL, ENGI_WHEEL, ENGI_MACRO, ENGI_DRUMX}, {"FULL ORGAN", "JAZZ PERC", "MALLET", "TECHNO RAW"}, 1, 0},
    {{ENGI_WHEEL, ENGI_SHAPES, ENGI_MACRO, ENGI_DRUMX}, {"ROCK DRIVE", "PLUCK", "HI-HAT", "909 CLUB"}, 1, 1},
};
#define NCASE (sizeof CASES / sizeof CASES[0])
static int32_t out_all[NCASE][BLOCKS * CTL * 2];
static fw_mod_hdr_t wheel_hdr;
static uint32_t preset_of(uint32_t e, const char *name)
{
    for (uint32_t k = 0; k < ENGINES[e]->npresets; k++)
        if (!strcmp(eng_preset(ENGINES[e], k)->name, name)) return k;
    fprintf(stderr, "no preset %s\n", name);
    abort();
}
static void setup(const case_t *c)
{
    memset(trk, 0, sizeof trk);
    eng_state_reset();
    memset(midi_bend_q8, 0, sizeof midi_bend_q8);
    memset(midi_bend_target, 0, sizeof midi_bend_target);
    memset(&mod, 0, sizeof mod);
    memset(resource, 0, sizeof resource);
    memset(mix_cache, 0, sizeof mix_cache);
    track_solo = 0;
    for (uint32_t i = 0; i < NPART; i++) solo_gain[i] = 32768;
    lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    rng_state = 0x12345678u;
    mod_seed = 0x2545F491u;
    pl_rng_state = 0x21u;
    pb_rng_state = 0x21u;                               /* (Braids': SHAPES' digital shapes) */
    vage = 0; panic_req = 0;
    host_tracks_init();
    song.ntrk = 4;
    for (uint32_t i = 0; i < 4u; i++) {
        track_t *t = &trk[i];
        host_preset(t, c->e[i], preset_of(c->e[i], c->pre[i]));
        t->p[P_LEVEL] = 70;
        t->p[P_LWAVE] = 4; t->p[P_LRATE] = 90;                   /* (the LFO's S&H: rng() in track order) */
        t->p[P_M1SRC] = MS_SH; t->p[P_M1DST] = MD_PITCH; t->p[P_M1AMT] = 9;
        t->p[P_M2SRC] = MS_LFO; t->p[P_M2DST] = MD_CUT; t->p[P_M2AMT] = 20;
        if (c->spread && i == 0) t->p[P_SPRD] = 60;
        if (c->e[i] == ENGI_DRUMX && c->pan) {                   /* lanes panned, PUNCH and ATK+ on some */
            dxk_t k;
            dxk_view(i, &k);
            k.on = 1;
            for (uint32_t l = 0; l < DXK_NLANE; l++) { k.pan[l] = (int8_t)((int)l * 17 - 60); k.punch[l] = (uint8_t)(l & 3u) * 4u; k.atk[l] = (uint8_t)(l == 2u ? 3u : 0u); }
            dxk_put(i, &k);
        }
    }
    for (uint32_t i = 0; i < 4u; i++)
        for (uint32_t k = 0; k < 4u; k++) trk_note_on(&trk[i], 36u + i * 7u + k * 4u, 80u + k * 12u);
}
static void render(uint32_t ci)
{
    setup(&CASES[ci]);
    for (uint32_t b = 0; b < BLOCKS; b++) {
        int32_t out[2 * CTL];
        if (b == 90) for (uint32_t i = 0; i < 4u; i++) trk_note_off(&trk[i], 36u + i * 7u);
        if (b == 140) for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[i], 48u + i, 120u);
        if (b == 200) for (uint32_t i = 0; i < 4u; i++) for (uint32_t k = 0; k < 4u; k++) trk_note_off(&trk[i], 36u + i * 7u + k * 4u);
        if (b == 230) for (uint32_t i = 0; i < 4u; i++) trk_note_on(&trk[i], 40u + i * 3u, 100u);
        if (b == 170)                                   /* a MACRO model and a SHAPES shape changed while voices sound (analog <-> digital) */
            for (uint32_t i = 0; i < 4u; i++) {
                if (trk[i].engine == ENGI_MACRO) trk[i].p[P_E0] = (int16_t)(trk[i].p[P_E0] == MC_MODAL ? MC_VA : MC_MODAL);
                if (trk[i].engine == ENGI_SHAPES) trk[i].p[P_E0] = (int16_t)(trk[i].p[P_E0] < 13 ? 22 : 0);
            }
        if (b == 260) trk[3].eng_req = (uint8_t)(trk[3].engine == ENGI_MACRO ? ENGI_DRUMX : ENGI_MACRO);   /* an engine switch: its fade */
        mix_block(out, CTL);
        assert(AW_LOAD(&audio_worker.request) == AW_LOAD(&audio_worker.complete));
        memcpy(out_all[ci] + 2u * b * CTL, out, sizeof out);
    }
}
static void *worker_thread(void *unused)
{
    (void)unused;
    audio_worker_loop();
    return 0;
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    ENGINES[ENGI_WHEEL] = &ENG_WHEEL;                            /* (slot 7, as the module registers it) */
    wheel_hdr.state_size = sizeof(drw_part_t);
    eng_mod[ENGI_WHEEL] = &wheel_hdr;
#ifdef PB_REF
    for (uint32_t c = 0; c < NCASE; c++) render(c);
    FILE *f = fopen(argv[1], "wb");
    assert(f && fwrite(out_all, sizeof out_all, 1, f) == 1);
    fclose(f);
    printf("port batch: the reference (no batch), %u cases written\n", (unsigned)NCASE);
#else
    static int32_t ref[NCASE][BLOCKS * CTL * 2];
    FILE *f = fopen(argv[1], "rb");
    assert(f && fread(ref, sizeof ref, 1, f) == 1);
    fclose(f);
    pthread_t thread;
    assert(!pthread_create(&thread, 0, worker_thread, 0));
    while (!AW_LOAD(&audio_worker.ready)) sched_yield();
    audio_worker_online = 1;
    static const uint32_t IDS[4] = {AP_MACRO, AP_DRUMX, AP_SHAPES, AP_WHEEL};
    static const char *const NAMES[4] = {"MACRO", "DRUM-X", "SHAPES", "WHEEL"};
    uint32_t on1[4] = {0, 0, 0, 0};
    for (uint32_t mode = 0; mode < 2u; mode++) {
        serial_render = mode == 0;
        for (uint32_t c = 0; c < NCASE; c++) {
            for (uint32_t k = 0; k < AUDIO_BATCH_JOBS; k++) audio_batch_jobs[k].profile_id = 0;
            render(c);
            for (uint32_t b = 0; b < BLOCKS; b++)
                if (memcmp(ref[c] + 2u * b * CTL, out_all[c] + 2u * b * CTL, 2u * CTL * sizeof(int32_t))) {
                    fprintf(stderr, "difference: case %u (%s %s %s %s), %s, block %u\n", (unsigned)c, CASES[c].pre[0], CASES[c].pre[1],
                            CASES[c].pre[2], CASES[c].pre[3], mode ? "CPU1 concurrent" : "CPU1 refused", (unsigned)b);
                    abort();
                }
        }
    }
    /* which ports' kernels CPU1 ran (one more concurrent pass of each case, counting the worker's share at each flush) */
    for (uint32_t c = 0; c < NCASE; c++) {
        setup(&CASES[c]);
        for (uint32_t b = 0; b < 60u; b++) {
            int32_t out[2 * CTL];
            mix_block(out, CTL);
            uint32_t mask = AB_LOAD(&audio_batch_worker_mask), n = AB_LOAD(&audio_batch_published);
            for (uint32_t k = 0; k < n && k < AUDIO_BATCH_JOBS; k++)
                if ((mask >> k) & 1u)
                    for (uint32_t e = 0; e < 4u; e++) on1[e] += audio_batch_jobs[k].profile_id == IDS[e];
        }
    }
    for (uint32_t e = 0; e < 4u; e++) {
        printf("port batch: %s kernels on CPU1: %u\n", NAMES[e], on1[e]);
        assert(on1[e] > 0);
    }
    const case_t d = {{ENGI_MACRO, ENGI_MACRO, ENGI_MACRO, ENGI_MACRO}, {"MALLET", "VA BASS", "MALLET", "VA BASS"}, 0, 0};
    setup(&d);
    for (uint32_t b = 0; b < 200; b++) { int32_t out[2 * CTL]; mix_block(out, CTL); }
    assert(pl_rng_state == 0x21u);
    assert(!pthread_cancel(thread));
    assert(!pthread_join(thread, 0));
    printf("port batch: %u cases x 2 (CPU1 refused, concurrent) equal the reference without the batch; MODAL and VA leave plaits' generator alone\n",
           (unsigned)NCASE);
#endif
    return 0;
}
