/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: throws (build doc 4d part 2; Skiff 1.3.3 throw.c). PERFORM's KNOB 2..4 (ARP held: ui_layer.c) push the
 * selected track's SEND, SEND 2 and SEND 3 (to the shared effects) away from their values for a moment: an offset over
 * the value, never saved, that glides back to nothing over one bar once the knob rests (THROW_REST ms), gone at a
 * stop. The audio ISR puts the offsets on at the start of each block (mix_block, before its events) and takes them off
 * once the block is mixed, so the main loop never sees them: what it shows, saves and edits is the value itself. A
 * value something else wrote during the block (motion) stays. Included by mix.c (after fx.c).
 *
 * Revisions:
 *   0.4.3: first version.
 */
#define THROW_REST 120u                                 /* ms the knob rests before its throw glides back */
#define THR_ONE 65536                                   /* an offset of 1 (fine enough that a bar's glide is a bar) */
#define THR_N 3u                                        /* SEND, SEND 2, SEND 3 */
static const uint8_t THR_ID[THR_N] = {P_FXSEND, P_FXSEND2, P_FXSEND3};
static struct {
    int32_t off, dec;                                   /* the offset, x THR_ONE; how much of it a block takes (0: still) */
    uint32_t t;                                         /* fm1_ms of its knob's last turn */
    int16_t keep, put;                                  /* this block: the value before, the one put on */
} thr[NTRK * THR_N];
static uint32_t motion_guard(void);                     /* (motion.c) */
static uint32_t beat_samples(void);                     /* (mix.c) */
static void motion_unguard(uint32_t f);
static volatile uint32_t thr_out;                       /* bit 3k + c: track k's throw c is out */
static uint32_t thr_on;                                 /* .. put on this block (throw_off takes these off) */
_Static_assert(NTRK * THR_N <= 32u, "a throw a bit");

/* the start of a block (mix_block, before the events): each throw on over its value, gliding once its knob rests */
static __attribute__((noinline)) void throw_on(uint32_t n)
{
    uint32_t i, m = thr_out, on = 0, now = fm1_ms, bar = 4u * beat_samples();
    for (i = 0; i < NTRK * THR_N; i++) {
        track_t *t = &trk[i / THR_N];
        uint32_t id = THR_ID[i % THR_N];
        if (!((m >> i) & 1u))
            continue;
        if (now - thr[i].t > THROW_REST) {              /* back over a bar, in a line */
            int32_t a = thr[i].off < 0 ? -thr[i].off : thr[i].off;
            if (!thr[i].dec)
                thr[i].dec = (int32_t)((uint32_t)a / (bar ? bar : 1u) * n + (uint32_t)a % (bar ? bar : 1u) * n /
                                       (bar ? bar : 1u)) + 1;
            a = a > thr[i].dec ? a - thr[i].dec : 0;
            thr[i].off = thr[i].off < 0 ? -a : a;
            if (!a) {
                thr_out &= ~(1u << i);
                continue;
            }
        }
        thr[i].keep = t->p[id];
        thr[i].put = (int16_t)clamp(thr[i].keep + thr[i].off / THR_ONE, 0, 127);
        t->p[id] = thr[i].put;
        on |= 1u << i;
    }
    thr_on = on;
}
/* the block mixed: the throws off again (a value written meanwhile kept) */
static __attribute__((noinline)) void throw_off(void)
{
    uint32_t i, m = thr_on;
    for (i = 0; m; i++, m >>= 1)
        if ((m & 1u) && trk[i / THR_N].p[THR_ID[i % THR_N]] == thr[i].put)
            trk[i / THR_N].p[THR_ID[i % THR_N]] = thr[i].keep;
    thr_on = 0;
}
/* (the main loop) PERFORM's KNOB c + 2 turned s: track k's send c thrown further (from its own value; 0..127 the
 * limit); its glide from when the knob rests */
static void throw_turn(uint32_t k, uint32_t c, int32_t s)
{
    track_t *t = &trk[k % NTRK];
    uint32_t i = (k % NTRK) * THR_N + c % THR_N, id = THR_ID[c % THR_N];
    int32_t o;
    uint32_t f = motion_guard();                        /* (the ISR not in between) */
    if (!((thr_out >> i) & 1u))
        thr[i].off = 0;
    o = clamp(thr[i].off / THR_ONE + s, -t->p[id], 127 - t->p[id]);
    thr[i].off = o * THR_ONE;
    thr[i].t = fm1_ms;
    thr[i].dec = 0;
    if (o) thr_out |= 1u << i;
    else thr_out &= ~(1u << i);
    motion_unguard(f);
}
/* track k's send c as it sounds now (its value and its throw) */
static int32_t throw_value(uint32_t k, uint32_t c)
{
    uint32_t i = (k % NTRK) * THR_N + c % THR_N;
    int32_t v = trk[k % NTRK].p[THR_ID[c % THR_N]];
    return (thr_out >> i) & 1u ? clamp(v + thr[i].off / THR_ONE, 0, 127) : v;
}
static void throw_stop(void) { thr_out = 0; }           /* (a stop, a project load: none out) */
