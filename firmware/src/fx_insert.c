/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio) */
/* (Release 2 as jfx_dub.c; Release 3 step 5: the generic insert host, FX_MODULES_PLAN.md 5 and 14) The per-track insert (docs/orangecat/BUILD.md, "DUB TAPE as the per-track insert", design 3). Included by mix.c after jfx.c.
 *
 * Each of the four tracks can hold an instance of a kernel (fx_kernel_t, fx_insert.h; today DUB TAPE's, jfx_dub.c). The track's mono x, after DIST, mute, solo and LEVEL and before PAN, is captured
 * where the sends are taken (mix.c mix_send, into fx_x[fx_cur][track]); the instance's job, the first descriptor of the voice batch, runs on the PREVIOUS
 * block's capture (fx_x[fx_cur ^ 1]) and its wet is added into this block's dry mix and, in mono, through the track's own sends (mix.c fx_ins_add_wet).
 * The memory (jfx.c jfx_ins_mem) is first come, first served: an instance whose memory is not all there plays dry and the page says DUB FULL.
 * A change of the line's size or the rate stage never cuts: the wet is ramped out (the kernel's cut, 3 ms), the memory changes (or the line is cleared a block a
 * block), and the instance starts again from silence with its input ramped in.
 *
 * This file knows no kernel: it asks fx_kernel_of(k) for the capability words and calls the kernel only through that struct. What is DUB's is in jfx_dub.c. */
#if MELODEE_DUB
#ifndef OC_DUB_SRAM
#define OC_DUB_SRAM 0
#endif
#if OC_DUB_SRAM                                         /* (round 3, a measurement build only: the code in SRAM instead of flash, for the cost of one against the other) */
#pragma push_macro("JFX_XIP")
#undef JFX_XIP
#define JFX_XIP __attribute__((noinline))
#endif
#ifndef OC_DUB_KSRAM
#define OC_DUB_KSRAM 0
#endif
#if OC_DUB_KSRAM                                        /* (round 4) only the per-block job in SRAM; the control path stays in flash */
#define FX_HOT __attribute__((noinline))
#else
#define FX_HOT JFX_XIP
#endif
#define FX_SCALE 524288.0f                              /* the mix domain: 1.0 = 524,288 (x is clamped to +-524,287; the master shifts by 4 to Q15) */
#define FX_THR 132                                      /* -72 dB of the mix domain */
#define FX_RAMP 6u                                      /* blocks of cut before a change (the kernel's ramp is 3 ms = 4.1 blocks) */
#ifndef DUB_IDLE
#define DUB_IDLE 11000u                                 /* blocks (8 s) an instance stays after its effect went off: muted, memory kept, so ECHO back up has the repeats still there (the name the tests shorten) */
#endif
#define FX_IDLE DUB_IDLE
_Static_assert(JFX_SEG * sizeof(int16_t) == 4096u, "a line block is 4 KB");
_Static_assert(CTL == 32, "a job a block of 32 samples");
static __attribute__((section(".pool"))) fx_ins_t fx_ins[FX_INS_N];
#define fx_p(k) ((void *)fx_cm->p[k])                   /* the job's parameter copies, the capture pair, the wet and the batch's sink: the instances' common arena block (jfx.c fx_common_t) */
#define fx_x (fx_cm->x)
#define fx_iwet (fx_cm->wet)
#define fx_sink (fx_cm->sink)
static __attribute__((section(".pool"))) uint8_t fx_cur, fx_job_buf, fx_mask, fx_done;
static __attribute__((section(".pool"))) uint8_t fx_ins_gov_req;                          /* audio.c: late halves while an instance ran: the governor asks for one to give way */
static __attribute__((section(".pool"))) uint8_t fx_ins_force_degrade;                    /* a host test's CPU1 retirement */
static __attribute__((section(".pool"))) uint16_t fx_cpu_wait;                            /* blocks still to wait before an insert is admitted (CPU1 retired or forced off sets it to FX_CPU_OK_BLOCKS; it counts down while CPU1 is online; 0 at boot: .pool is zero) */
#define FX_CPU_OK_BLOCKS 2750u                          /* 2 s */

/* ---- (Release 3 step 6) a module's kernel beside the resident one (FX_MODULES_PLAN.md 15) ----
 * The Effects table (oc_module.c) registers an effect module of id FX_MODK_ID (DUB TAPE's) and hands its checked caps block, call table and rows to fx_modk_set, which makes a fx_kernel_t in RAM
 * (fx_modk) from them; fx_modk_clear drops it when the module goes (a removal, a reinstall, a ban). WHICH kernel a track's instance runs is the track's own setting:
 * fx_mod_want, a bit a track, 0 for all (the default: the resident kernel, so a build with no module, or with the module and no choice made, plays what it always played).
 * fx_mod_bound is the kernel the instance was made with: it changes only while the instance is OFF (a track whose setting changed ramps out and lets go first, as a governor release
 * does, then starts again on the other kernel from silence). fx_kernel_of(k) (jfx_dub.c) answers from fx_mod_bound. */
#if OC_FX_MODK
static __attribute__((section(".pool"))) fx_kernel_t fx_modk;
static __attribute__((section(".pool"))) uint8_t fx_modk_ok, fx_mod_want, fx_mod_bound;
static __attribute__((section(".pool"))) void (*fx_modk_note)(uint32_t);               /* oc_module.c: the crash mark of the module in use (oc_fxmod_note_use) */
#define FX_MODK_BOUND(k) ((fx_mod_bound >> (k)) & 1u)
static void fx_modk_clear(void);
/* the tracks that run the module's kernel (a bit each), or the ones that may: 0 with no module */
static inline uint32_t fx_mod_effective(void) { return fx_modk_ok ? fx_mod_want : 0u; }
/* the kernel of the module, from its checked descriptor: 0 ok, FXM_VER (a caps version this host does not know), FXM_RANGE (a capability outside what the host can give: instances, blocks, a seed, a class,
 * the snapshot's room). On a refusal nothing is registered: the resident kernel plays and the Effects row says so (oc_mod.fx .why) */
static int fx_modk_set(uint32_t module_id, uint32_t version, uint32_t effect_id, const fx_caps_t *c, const fx_kcalls_t *k, const fx_prow_t *rows)
{
    fx_kernel_t m;
    if (c->magic != FX_CAPS_MAGIC || c->size != FX_CAPS_WIRE || c->version != FX_CAPS_VERSION) return FXM_VER;
    if (!c->ram_work_min || c->ram_work_min > c->ram_work_max || c->ram_work_max > FX_INS_MAXSEG || !c->max_inst || c->max_inst > FX_INS_N || c->prof >= AP_COUNT ||
        (c->flags & FXK_SEED) || !c->param_bytes || c->param_bytes > FX_PARAM_BYTES || (!c->ram_extra != !c->ram_extra_bytes) || !c->gov_us || !c->nrows != !rows) return FXM_RANGE;
    memset(&m, 0, sizeof m);
    m.module_id = module_id; m.version = version; m.legacy_module = c->legacy_module; m.effect_id = (uint8_t)effect_id; m.nrows = c->nrows; m.rows = rows;
    m.flags = c->flags; m.rates = c->rates; m.max_inst = c->max_inst; m.prof = c->prof; m.ram_work_min = c->ram_work_min; m.ram_work_max = c->ram_work_max; m.ram_extra = c->ram_extra;
    m.ram_state = c->ram_state; m.latency = c->latency; m.tail_base = c->tail_base; m.cost = c->cost; m.ram_extra_bytes = c->ram_extra_bytes; m.gov_us = c->gov_us;
    m.on = k->on; m.size_sig = k->size_sig; m.size_need = k->size_need; m.blocks_at = k->blocks_at; m.hold_len = k->hold_len; m.tick = k->tick; m.param_sig = k->param_sig;
    m.snap = k->snap; m.cost_extra = k->cost_extra; m.start = k->start; m.process = k->process; m.reset = k->reset;
    if (!m.on || !m.size_sig || !m.size_need || !m.blocks_at || !m.hold_len || !m.tick || !m.param_sig || !m.snap || !m.cost_extra || !m.start || !m.process || !m.reset) return FXM_RANGE;
    fx_modk_clear();
    fx_modk = m;
    fx_modk_ok = 1;
    return FXM_OK;
}
/* the module's kernel gone: the instances that were made with it let go at once (their memory is asked back by jfx_ins_mem as `want` falls to 0) and the tracks start again on the resident kernel */
static void fx_modk_clear(void)
{
    uint32_t k;
    for (k = 0; k < FX_INS_N; k++)
        if (FX_MODK_BOUND(k)) {
            fx_ins[k].state = FXS_OFF; fx_ins[k].evict = 0; fx_ins[k].gov = fx_ins[k].held_off = 0; fx_ins[k].kw[0] = fx_ins[k].kw[1] = 0;
            fx_ins_want[k] = 0; fx_ins_spring[k] = 0;
        }
    fx_mod_bound = 0;
    fx_modk_ok = 0;
    memset(&fx_modk, 0, sizeof fx_modk);
}
/* the tracks choose the module's kernel (a bit each) when one is registered; the change takes effect at the instance's next start */
static void fx_ins_use_module(uint32_t mask) { fx_mod_want = (uint8_t)(mask & ((1u << FX_INS_N) - 1u)); }
static uint32_t fx_modk_uses(void)         /* instances of the module's kernel running now (the Effects table's `uses`) */
{
    uint32_t k, n = 0;
    for (k = 0; k < FX_INS_N; k++) n += FX_MODK_BOUND(k) && (fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP);
    return n;
}
#else
#define FX_MODK_BOUND(k) 0u
static inline uint32_t fx_mod_effective(void) { return 0u; }
static inline int fx_modk_set(uint32_t module_id, uint32_t version, uint32_t effect_id, const fx_caps_t *c, const fx_kcalls_t *k, const fx_prow_t *rows)
{ (void)module_id; (void)version; (void)effect_id; (void)c; (void)k; (void)rows; return FXM_RANGE; }
static inline void fx_modk_clear(void) {}
static inline void fx_ins_use_module(uint32_t mask) { (void)mask; }
static inline uint32_t fx_modk_uses(void) { return 0u; }
#endif

/* the capture buffer track k writes this block, or 0 (no instance running) */
static inline int32_t *fx_ins_capture(uint32_t k)
{
    return k < FX_INS_N && (fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP) ? fx_x[fx_cur][k] : 0;
}
static inline int fx_ins_alive(uint32_t k) { return k < FX_INS_N && fx_ins[k].hold && (fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP); }

/* what a governor eviction was made for: the user's settings (the times, the mode, the speed, the tempo), not the sizes, which fall back when the instance ends */
static inline uint8_t fx_gov_key(const fx_ins_t *d) { return (uint8_t)(d->rsig ^ d->rsig >> 8 ^ d->rsig >> 16 ^ d->rsig >> 24); }
/* the job: every instance flagged in fx_mask, on the previous block's capture */
FX_HOT static void fx_ins_run(void)
{
    uint32_t k, i;
    for (k = 0; k < FX_INS_N; k++) {
        float in[CTL];
        const fx_kernel_t *kn;
        if (!((fx_mask >> k) & 1u)) continue;
        kn = fx_kernel_of(k);
        for (i = 0; i < CTL; i++) in[i] = (float)fx_x[fx_job_buf][k][i] * (1.0f / FX_SCALE);
        kn->process(fx_ins_state[k], fx_p(k), in, fx_iwet[k][0], fx_iwet[k][1]);
    }
}
static void fx_ins_job(const void *data, int32_t *pcm) { (void)data; (void)pcm; fx_ins_run(); }
static void fx_ins_merge(const void *data, const int32_t *pcm, uint32_t n) { (void)data; (void)pcm; (void)n; fx_done = 1; }

/* before the tracks, every block (mix.c): the pair swapped, each instance's state advanced, the job pushed first */
#if MELODEE_DUAL_CORE && defined(FM1_IRQ_TARGET)
#if OC_CORE_STATS
#define FX_CPU1_OK() (audio_worker_online != 0 && !cs_single)
#else
#define FX_CPU1_OK() (audio_worker_online != 0)
#endif
#else
#define FX_CPU1_OK() 1                                  /* (a host build: no second core to lose; tests use fx_ins_force_degrade) */
#endif
/* audio.c, the end of every half (round 14): the governor acts on the time the DMA sees, with headroom, and before a half is late. A running instance whose half passes 85 % of the deadline
 * (FX_BACK_US, the overload shed's line) is the one to give way, at the first such half: ramped out over FX_RAMP blocks, DUB FULL, and it comes back only after the wait (8, 16, 32 s), 2 s
 * without a half over the line, and headroom: the heaviest half of the last 32 s, seen while no instance ran, plus what an instance costs (the kernel's gov_us) must still be under the line
 * (a mix whose note-on halves are at 91 % alone never admits one: the insert stays out and says DUB FULL). The same headroom is asked of a new instance. */
#define FX_BACK_US ((HALF_FRAMES * 1000000u / FS) * 85u / 100u)
#define FX_PK_HALVES 1376u                              /* a bucket of the peak window: 4 s of halves; 8 of them, 32 s */
static __attribute__((section(".pool"))) uint16_t fx_gov_cool, fx_late_recent, fx_pk[8], fx_pk_n;     /* fx_late_recent: blocks left of the quiet a governor release waits for, set by every half over the line */
static __attribute__((section(".pool"))) uint8_t fx_pk_i;
#define FX_GOV_WAIT 11000u                              /* 8 s */
#define FX_QUIET 2750u                                  /* 2 s */
static __attribute__((section(".pool"))) uint32_t fx_ins_gov_fires;                       /* governor evictions since power-on (the readout) */
#ifndef OC_DUB_NOGOV
#define OC_DUB_NOGOV 0
#endif
static void fx_pk_note(uint32_t us)
{
    if (us > 65535u) us = 65535u;
    if (us > fx_pk[fx_pk_i]) fx_pk[fx_pk_i] = (uint16_t)us;
}
/* room for an instance: the heaviest half of the window with the instances' cost on it stays under the line */
static int fx_ins_headroom(void)
{
    uint32_t i, m = 0;
    uint32_t n = 0;
    for (i = 0; i < 8; i++) if (fx_pk[i] > m) m = fx_pk[i];
    for (i = 0; i < FX_INS_N; i++) if (fx_ins[i].state == FXS_RUN || fx_ins[i].state == FXS_RAMP) n += fx_kernel_of(i)->gov_us;
    return m + n + fx_kernel_of(0)->gov_us < FX_BACK_US;     /* (each instance already running costs as much again: the sum with the new one's, the kernel bound to the slot, under the line) */
}
JFX_XIP static void fx_ins_late_half(int late, uint32_t us)
{
    uint32_t k, running = 0;
#if OC_DUB_NOGOV                                        /* (round 5, a measurement build only: the governor never releases an instance, so a load over the budget can be timed with an insert running) */
    (void)late; (void)us; (void)k; (void)running;
    return;
#endif
    for (k = 0; k < FX_INS_N; k++) running |= fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP;
    if (!running) {                                     /* no instance: this half is the mix alone, and the window remembers its heaviest */
        fx_pk_note(us);
        fx_gov_cool = 0;
    } else if (late || us > FX_BACK_US) {
        fx_late_recent = FX_QUIET;
        if (!fx_gov_cool) {
            fx_ins_gov_req = 1;
            fx_gov_cool = 12u;                          /* the ramp (6 blocks) is under way: the next judges after about 35 ms */
            fx_ins_gov_fires++;
        }
    }
    if (fx_gov_cool && running) fx_gov_cool--;
    if (running) return;                                /* the window is the mix alone: it stands still while an instance runs (it must not empty under a long run) */
    if (++fx_pk_n >= FX_PK_HALVES) { fx_pk_n = 0; fx_pk_i = (uint8_t)((fx_pk_i + 1u) & 7u); fx_pk[fx_pk_i] = 0; }
}
/* an instance running: the voice caps' third column (voice.c vc_limit) */
static int fx_ins_running(void) { return fx_mask != 0; }

/* before the tracks, every block (mix.c): the pair swapped, each instance's state advanced, the job pushed first */
JFX_XIP static void fx_ins_start(uint32_t nt)
{
    uint32_t k, i, cost = 0, beat;
    int degrade;
    fx_job_buf = fx_cur;
    fx_cur ^= 1u;
    if (fx_cm) memset(fx_x[fx_cur], 0, sizeof fx_x[fx_cur]);
    fx_mask = 0;
    fx_done = 0;
    fx_ins_ev_full = 0;
    if (fx_late_recent) fx_late_recent--;
    if (fx_ins_force_degrade || !FX_CPU1_OK()) fx_cpu_wait = FX_CPU_OK_BLOCKS;      /* CPU1 retired or single-core forced: the instances are released, DUB FULL shows */
    else if (fx_cpu_wait) fx_cpu_wait--;
    degrade = fx_cpu_wait != 0;
    beat = beat_samples();                              /* (the kernels' tempo, once a block: fx_ictx_t) */
    if (fx_ins_gov_req) {                               /* the governor: the highest running instance gives way */
        fx_ins_gov_req = 0;
        for (k = FX_INS_N; k-- > 0;)
            if ((fx_ins[k].state == FXS_RUN || fx_ins[k].state == FXS_RAMP) && !fx_ins[k].evict) {
                fx_ins[k].evict = 1;
                fx_ins[k].gov = 1;
                break;
            }
    }
    for (k = 0; k < FX_INS_N; k++) {
        fx_ins_t *d = &fx_ins[k];
        const track_t *t = &trk[k];
        const fx_kernel_t *kn;
#if OC_FX_MODK
        if (((fx_mod_effective() ^ fx_mod_bound) >> k) & 1u) {        /* the track's kernel changed: only an OFF instance changes kernels; a running one ramps out and lets go first */
            if (d->state == FXS_OFF) { fx_mod_bound ^= (uint8_t)(1u << k); d->kw[0] = d->kw[1] = 0; if (FX_MODK_BOUND(k) && fx_modk_note) fx_modk_note(FX_MODK_ID); }
            else if (d->state == FXS_RUN || d->state == FXS_RAMP) { d->evict = 1; d->gov = 0; }
            else { fx_ins_want[k] = 0; fx_ins_spring[k] = 0; d->state = FXS_OFF; }
        }
#endif
        kn = fx_kernel_of(k);
        uint32_t blocks = 0u, sp, R = 2u;
        float need = 0.0f;
        fx_ictx_t c;
        int on = k < nt && kn->on(t), deb = 0, live = d->state == FXS_RUN || d->state == FXS_RAMP;
        d->run = 0;
        c.beat = beat; c.k = (uint8_t)k; c.trig[0] = fx_trig[k & 3u][0]; c.trig[1] = fx_trig[k & 3u][1]; c.trig[2] = fx_trig[k & 3u][2]; c.pad = 0;
        if (on) d->idle = 0;
        else if (k < nt && (d->state == FXS_RUN || d->state == FXS_RAMP) && d->idle < FX_IDLE) { on = 1; d->idle++; }   /* the effect off: muted, the instance and its memory stay a while */
        if (!live) { d->st_R = 0; d->st_blk = 0; d->idle = 0; }           /* (the size is sticky only while the line runs: one that is waiting for memory asks for what the times need now) */
        if (on) {                                           /* the size from the kernel: the rate stage (half rate while the farthest tap fits the most blocks, quarter rate when it does not) and the blocks, sticky */
            uint32_t rs = kn->size_sig(t, &c);
            if (rs != d->rsig) { d->rneed = kn->size_need(t, &c); d->rsig = rs; }       /* (made again only when what the sizing is made from changed) */
            need = d->rneed;
            if (!d->st_R) d->st_R = 2u;
            if (kn->blocks_at(need, 2u) > kn->ram_work_max) R = 4u;
            if (d->st_R == 4u) R = 4u;
            if (d->state == FXS_RUN && R != d->R) { if (d->rdeb < 344u) { d->rdeb++; R = d->R; deb = 1; } } else d->rdeb = 0;
            if (R > d->st_R) { d->st_R = (uint8_t)R; d->st_blk = 0; }
            blocks = kn->blocks_at(need, d->st_R);
            if (blocks > kn->ram_work_max) blocks = kn->ram_work_max;
            if (deb || blocks < d->st_blk) blocks = d->st_blk;    /* (a rate change still being waited for: the line as it is; the sticky size) */
            if ((d->state == FXS_RUN || d->state == FXS_RAMP) && blocks > fx_ins_have[k] && fx_ins_pool_room(k) < blocks) blocks = fx_ins_have[k] ? fx_ins_have[k] : blocks;   /* the pool cannot grow it: the echo that is running stays as it is (its taps clamp to the line) */
            d->st_blk = (uint8_t)blocks;
            R = d->st_R;
            d->hold_len = (uint16_t)kn->hold_len(need, blocks, R);
        }
        sp = blocks ? 1u : 0u;                              /* the option's memory (the spring's) is on every running instance (it never cuts the echo to attach it) */
        if (fx_ins_evict & (1u << k)) {                 /* a bus needs the memory (jfx.c jfx_ins_mem): ramp out and let go */
            fx_ins_evict &= (uint8_t)~(1u << k);
            if (d->state == FXS_RUN || d->state == FXS_RAMP) d->evict = 1;
            else if (d->state == FXS_WAIT || d->state == FXS_CLEAR) { fx_ins_want[k] = 0; fx_ins_spring[k] = 0; d->state = FXS_OFF; }
        }
        if (degrade && (d->state == FXS_RUN || d->state == FXS_RAMP)) { d->evict = 1; d->gov = 0; }
        else if (degrade && (d->state == FXS_WAIT || d->state == FXS_CLEAR)) { fx_ins_want[k] = 0; fx_ins_spring[k] = 0; d->state = FXS_OFF; d->held_off = 1; d->gov = 0; }
        kn->tick(d->kw, &c);
        switch (d->state) {
        case FXS_OFF:
            if (blocks) {
                if (d->held_off && d->gov && fx_gov_key(d) != d->gov_sig) { d->gov = 0; d->gov_n = 0; }      /* the setting changed: it asks again, from the first wait */
                if (d->held_off && d->gov) {                                                              /* a governor release is not for good: after 8 s, and 2 s without a late half, it asks again (a transient is forgiven, a lasting overload is not) */
                    if (d->gov_wait) d->gov_wait--;
                    if (!d->gov_wait && !fx_late_recent && fx_ins_headroom()) d->gov = 0;
                }
                if (d->held_off && !d->gov && !degrade) d->held_off = 0;
                else if (!d->held_off && degrade) d->held_off = 1;
                if (!d->held_off && !fx_ins_headroom()) { fx_ins_ev_full |= (uint8_t)(JFX_FULL_INS0 << k); break; }      /* the mix leaves no room for it (round 14): dry, DUB FULL, asked again every block */
                if (d->held_off) { fx_ins_ev_full |= (uint8_t)(JFX_FULL_INS0 << k); break; }               /* CPU short: dry, DUB FULL */
                fx_ins_want[k] = (uint8_t)blocks; fx_ins_spring[k] = (uint8_t)sp; d->R = (uint8_t)R; d->state = FXS_WAIT;
            } else d->held_off = d->gov = d->gov_n = 0;
            break;
        case FXS_WAIT:                                        /* the memory not all there: dry */
            if (!blocks) { fx_ins_want[k] = 0; fx_ins_spring[k] = 0; d->state = FXS_OFF; break; }
            fx_ins_want[k] = (uint8_t)blocks; fx_ins_spring[k] = (uint8_t)sp; d->R = (uint8_t)R;
            if (fx_ins_ready[k] && !fx_ins_fresh[k]) { d->pend_R = d->R; d->clr = 0; d->state = FXS_CLEAR; break; }   /* the memory is there but was in use (it is not fresh): the line cleared, then started */
            if (fx_ins_ready[k] && fx_ins_fresh[k]) {
                kn->start(fx_ins_state[k], d->R, fx_line[k], fx_ins_have[k], fx_ins_spbuf[k]);   /* the input ramped in */
                memset(fx_iwet[k], 0, sizeof fx_iwet[k]);
                memset(fx_x[0][k], 0, sizeof fx_x[0][k]); memset(fx_x[1][k], 0, sizeof fx_x[1][k]);
                fx_ins_fresh[k] = 0;
                d->hold = 0;
                d->pvalid = 0;
                d->state = FXS_RUN;
            }
            break;
        case FXS_RUN:
            if (!fx_ins_ready[k]) { d->state = FXS_WAIT; break; }          /* the memory was taken back (the card, an engine, a project load) */
            if (d->evict) { d->state = FXS_RAMP; d->ramp = FX_RAMP; d->pend_R = 0; }
            else if (blocks != fx_ins_want[k] || sp != fx_ins_spring[k]) { d->state = FXS_RAMP; d->ramp = FX_RAMP; d->pend_R = 0; }
            else if (R != d->R) { d->state = FXS_RAMP; d->ramp = FX_RAMP; d->pend_R = (uint8_t)R; }
            break;
        case FXS_RAMP:
            if (!fx_ins_ready[k]) { d->state = FXS_WAIT; d->evict = 0; d->gov = 0; break; }
            if (d->ramp) d->ramp--;
            if (!d->ramp) {
                if (d->evict) {                              /* let go: the memory is freed (jfx_ins_mem), the instance waits as asked */
                    d->evict = 0;
                    fx_ins_want[k] = 0; fx_ins_spring[k] = 0; d->state = FXS_OFF;
                    d->held_off = (uint8_t)(degrade || d->gov);
                    d->gov_sig = fx_gov_key(d);
                    if (d->gov) {
                        d->gov_wait = (uint16_t)(FX_GOV_WAIT << (d->gov_n > 2 ? 2 : d->gov_n));
                        if (d->gov_n < 3) d->gov_n++;
                        d->run_ok = 0;
                    }
                }
                else if (d->pend_R && (uint8_t)R == d->pend_R && blocks == fx_ins_want[k] && sp == fx_ins_spring[k]) { d->state = FXS_CLEAR; d->clr = 0; }   /* a new rate: the line cleared a block a block, then started again */
                else if (blocks == fx_ins_want[k] && sp == fx_ins_spring[k] && (uint8_t)R == d->R) d->state = FXS_RUN;   /* the setting came back during the ramp: nothing changed, the wet ramps in again */
                else if (blocks == fx_ins_want[k] && sp == fx_ins_spring[k]) { d->pend_R = (uint8_t)R; d->state = FXS_CLEAR; d->clr = 0; }
                else { fx_ins_want[k] = (uint8_t)blocks; fx_ins_spring[k] = (uint8_t)sp; d->state = blocks ? FXS_WAIT : FXS_OFF; d->R = (uint8_t)R; }
            }
            break;
        case FXS_CLEAR:
            if (!fx_ins_ready[k]) { d->state = FXS_WAIT; break; }
            if (d->clr < fx_ins_have[k]) memset(fx_line[k][d->clr++], 0, JFX_SEG * sizeof(int16_t));   /* a block a block (4 KB), bounded */
            else {
                d->R = d->pend_R;
                kn->start(fx_ins_state[k], d->R, fx_line[k], fx_ins_have[k], fx_ins_spbuf[k]);
                memset(fx_iwet[k], 0, sizeof fx_iwet[k]);
                d->pvalid = 0;
                d->state = FXS_RUN;
            }
            break;
        }
        if (d->state == FXS_RUN && d->gov_n && ++d->run_ok > 60000u) d->gov_n = 0;     /* 47 s running without a governor release: the next one starts from the first wait */
        if (d->state == FXS_RUN || d->state == FXS_RAMP) {
            int32_t pk = 0;
            const int32_t *x = fx_x[fx_job_buf][k];
            for (i = 0; i < CTL; i++) { int32_t a = x[i] < 0 ? -x[i] : x[i]; if (a > pk) pk = a; }
            if (pk > FX_THR) d->hold = d->hold_len;
            if (d->hold) {
                uint32_t ps;
                c.kw0 = d->kw[0]; c.k = (uint8_t)k; c.R = d->R; c.nblk = fx_ins_have[k]; c.ramp = d->state == FXS_RAMP;
                ps = kn->param_sig(t, &c);
                if (!d->pvalid || ps != d->psig) {          /* (the parameter copy is made again only when something it is made from changed) */
                    kn->snap(fx_p(k), t, &c);
                    d->psig = ps; d->pvalid = 1;
                }
                d->run = 1;
                fx_mask |= (uint8_t)(1u << k);
                cost += audio_batch_cost[kn->prof] + kn->cost_extra(t, fx_ins_spbuf[k] != 0);
            }
        }
    }
    if (!fx_mask) return;
#if MELODEE_BATCH_AUDIO && !MELODEE_LEGACY_EXTRAS
    if (audio_batch_active) {
        uint32_t unused = 0;
        audio_batch_push_cost(fx_ins_job, &unused, sizeof unused, fx_sink, CTL, fx_kernel_of(0)->prof, cost, fx_ins_merge);
        return;
    }
#endif
    fx_ins_run();
    fx_done = 1;
}
/* the instances from silence, their memory given back (a new project, power-on); the next block asks again for each track's setting */
JFX_XIP static void fx_ins_reset(void)
{
    if (fx_cm) { memset(fx_x, 0, sizeof fx_x); memset(fx_iwet, 0, sizeof fx_iwet); }
    memset(fx_ins, 0, sizeof fx_ins);
    memset(fx_ins_want, 0, sizeof fx_ins_want);
    memset(fx_ins_spring, 0, sizeof fx_ins_spring);
    fx_ins_evict = fx_ins_ev_full = fx_ins_gov_req = 0;
#if OC_FX_MODK
    fx_mod_bound = 0;                                   /* the instances are gone; each track binds again to the kernel it is set to (fx_mod_want stays) */
#endif
    fx_gov_cool = 0;
    memset(fx_pk, 0, sizeof fx_pk); fx_pk_n = 0; fx_pk_i = 0;
    memset(fx_trig, 0, sizeof fx_trig);                 /* the performance triggers (the host's, MIDI CC 16 to 18) start released */
    fx_kernel_of(0)->reset();
}
#if OC_DUB_SRAM
#pragma pop_macro("JFX_XIP")
#endif
#else
static inline void fx_ins_late_half(int late, uint32_t us) { (void)late; (void)us; }
static inline int fx_ins_running(void) { return 0; }
static inline int32_t *fx_ins_capture(uint32_t k) { (void)k; return 0; }
static inline int fx_ins_alive(uint32_t k) { (void)k; return 0; }
static inline void fx_ins_start(uint32_t nt) { (void)nt; }
static void fx_ins_reset(void) {}
static inline void fx_modk_clear(void) {}
static inline uint32_t fx_modk_uses(void) { return 0u; }
#endif
