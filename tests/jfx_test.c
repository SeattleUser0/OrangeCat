/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.6.0: the effects ported from JIANT FM (firmware/src/jfx.c, jfx_slicer.c, jfx_perform.c), on the host.
 * The real sources through the UI test's harness (tests/ui_test.c). The master path is driven as mix.c mix_block runs
 * it, with the dry mix and the three sends given directly (no engine in the way), so each check sees one effect.
 *
 *   silence     nothing sent, every effect at rest: the output is exactly 0 and no effect memory is taken
 *   DELAY       the first echo where its TIME says (synced divisions at 120 BPM, a free time, the half and quarter
 *               rates of the long ones), ANALOG and DIGITAL; FEEDBK's repeats falling; PING and WIDE off: L == R
 *               exactly, PING on: the sides differ; the memory taken on the first send, given back 2 s after rest
 *   REVERB      ROOM SPRING SHIMMER RESO CLOUD: a tail that rises and then dies away; CLOUD at SIZE 127 (FREEZE) holds;
 *               each model driven hard for 10 s stays bounded and does not grow; a model change does not jump
 *   CHORUS      stereo: the sides differ
 *   DIST        SOFT HARD FOLD CRUSH RECT AMP each its own sound, bounded; DRIVE 0 fades out of the chain
 *   AUTO PAN    BEAT 1/4: 7..9 swings through the centre in 4 beats; DEPTH 0: no movement
 *   SLICER      GATE cuts the SLICER's bus to its pattern while playing; OFF: the bus is not in the mix
 *   FX layer    REPEAT 1/16 records from the next 1/16 and plays its loop exactly a 1/16 later; let go: back to the dry
 *               signal; a track's mute ramps to 0; THROW feeds the delay and the reverb
 *   FX FULL     a bus that gets no memory plays dry, says so (jfx_full), and comes in once there is memory
 *   storage     FUN23 round trip of every effect setting; out-of-range bytes clamp to the range; the template (TPLM)
 *               round trip; FUN22 written as 0.5.6 wrote it, and the frozen FUN16 .. FUN21, TPLF .. TPLK fixtures,
 *               converted: each old send on the bus of its old slot's kind, the rest at the new defaults, the leveler
 *               off for a converted project
 *   audition    dry and wet WAVs of every model (build/audition/), for listening
 *   cost        host time a block for each effect (x86, not the device: printed, not a gate)
 * Run by tests/run_tests.sh. */
#define UI_TEST_NO_MAIN 1
#define JFX_DLY_CHECK 1                                 /* (round 4) jfx.c counts a delay read or write outside the line held: jfx_dly_bad */
#include "ui_test.c"
#include <sys/stat.h>
#include <assert.h>

#define JT_SECS(s) ((uint32_t)((s) * FS))
#define JT_MAX JT_SECS(12)
static int32_t jt_l[JT_MAX], jt_r[JT_MAX];       /* a render's output */
static int32_t jo[2u * CTL];
static int bad;
static uint32_t jt_t0;                            /* the sine's time across renders (kind 3: no jump between them) */

static void jt_fresh(void)
{
    ui_power_on();
    song.master_q12 = 4096;                       /* MASTER at unity: the mix as it is */
    song.g[G_LEVEL] = 0;                          /* the leveler off: levels as they are */
    song.g[G_BPM] = 120;
    song.playing = 0;
    jt_t0 = 0;
}
/* one block of the master path as mix.c mix_block runs it, without tracks: the dry mix, the SLICER's bus, the sends */
static void jt_block(const int32_t *l, const int32_t *r, const int32_t *sl, const int32_t *c, const int32_t *d,
                     const int32_t *rv)
{
    uint32_t n = CTL, i;
    int perf;
    if (jfx_reset_req) jfx_reset_now();
    jfx_lend_check();
    for (i = 0; i < n; i++) {
        mix_l[i] = l ? l[i] : 0;
        mix_r[i] = r ? r[i] : 0;
        sl_l[i] = sl_r[i] = sl ? sl[i] : 0;
        send_c[i] = c ? c[i] : 0;
        send_d[i] = d ? d[i] : 0;
        send_r[i] = rv ? rv[i] : 0;
    }
    jfx_clock_begin();
    master_begin();
    perf = perf_begin(n);
    if (perf) perf_pre(mix_l, mix_r, sl_l, sl_r, send_d, send_r, n);
    jfx_bus_start(n);
    jfx_master(jo, n, perf);
}
/* the input of a render: kind 0 an impulse at 0, 1 a noise burst of `len` samples, 2 noise all along, 3 a sine
 * (440 Hz) all along, 4 plucked notes (an arpeggio) for listening; amplitude a */
static uint32_t jt_rng = 1u;
static int32_t jt_noise(void) { jt_rng = jt_rng * 1664525u + 1013904223u; return (int32_t)(jt_rng >> 16) - 32768; }
static int32_t jt_in(uint32_t kind, uint32_t t, uint32_t len, int32_t a)
{
    static const uint16_t NOTE_HZ[4] = {262, 330, 392, 523};
    switch (kind) {
    case 0: return t == 0 ? a : 0;
    case 1: return t < len ? (int32_t)((int64_t)jt_noise() * a >> 15) : 0;
    case 2: return (int32_t)((int64_t)jt_noise() * a >> 15);
    case 3: return (int32_t)((int64_t)sine_i((uint32_t)((uint64_t)(t + jt_t0) * 440u * 4294967296ull / FS)) * a >> 15);
    default: {
        uint32_t k = t / (FS / 4u), u = t % (FS / 4u);
        if (t >= len) return 0;
        int32_t env = 32767 - (int32_t)(u * 32767u / (FS / 4u));
        int64_t s = sine_i((uint32_t)((uint64_t)u * NOTE_HZ[k % 4u] * 4294967296ull / FS));
        return (int32_t)(s * env / 32768 * a >> 15);
    }
    }
}
/* where: 0 the dry mix, 1 the chorus send, 2 the delay send, 3 the reverb send, 4 the SLICER's bus; dry: the input
 * also in the dry mix. Renders `secs` into jt_l / jt_r */
/* (0.6.0 rev 2) the bus's memory in before a render, as on the device a track's send set brings it in before its first
 * note (jfx.c jfx_bus_start): the send set on track 1, silent blocks until the bus holds its memory (8 at most: an
 * arena that has none stays so, FX FULL) */
static int jt_bus_ready(uint32_t where)
{
    return where == 1 ? cho_buf != 0 : where == 2 ? (dly_n && dly_have >= dly_n) : where == 3 ? jrev != 0 && rx_have : 1;
}
static void jt_warm(uint32_t where)
{
    static int32_t z[CTL];
    if (where < 1u || where > 3u) return;
    trk[0].p[where == 1 ? P_CHOR : where == 2 ? P_DLY : P_REV] = 100;
    for (uint32_t b = 0; b < 8u && !jt_bus_ready(where); b++)
        jt_block(z, z, 0, 0, 0, 0);
}
static void jt_render(uint32_t where, int dry, uint32_t kind, uint32_t len, int32_t a, uint32_t samples)
{
    static int32_t x[CTL], z[CTL];
    uint32_t t = 0, i;
    jt_rng = 1u;
    memset(z, 0, sizeof z);
    jt_warm(where);
    while (t + CTL <= samples) {
        for (i = 0; i < CTL; i++) x[i] = jt_in(kind, t + i, len, a);
        jt_block(where == 0 || dry ? x : z, where == 0 || dry ? x : z, where == 4 ? x : 0, where == 1 ? x : 0,
                 where == 2 ? x : 0, where == 3 ? x : 0);
        for (i = 0; i < CTL; i++) {
            jt_l[t + i] = jo[2u * i];
            jt_r[t + i] = jo[2u * i + 1u];
        }
        t += CTL;
    }
    jt_t0 += t;
}
static double jt_energy(uint32_t from, uint32_t to)
{
    double e = 0;
    for (uint32_t i = from; i < to; i++) e += (double)jt_l[i] * jt_l[i] + (double)jt_r[i] * jt_r[i];
    return e / (double)(to > from ? to - from : 1u);
}
static uint32_t jt_peak_at(uint32_t from, uint32_t to, int32_t *peak)
{
    uint32_t at = from;
    int32_t m = -1;
    for (uint32_t i = from; i < to; i++) {
        int32_t a = abs(jt_l[i]) > abs(jt_r[i]) ? abs(jt_l[i]) : abs(jt_r[i]);
        if (a > m) { m = a; at = i; }
    }
    if (peak) *peak = m;
    return at;
}
static uint32_t jt_held(void)
{
    uint32_t n = 0;
    for (uint32_t k = 0; k < RES_JFX_N; k++) n += resource[RES_JFX0 + k].size;
    return n;
}
static uint64_t jt_hash(uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (uint32_t i = 0; i < n; i++) {
        h = (h ^ (uint32_t)jt_l[i]) * 1099511628211ull;
        h = (h ^ (uint32_t)jt_r[i]) * 1099511628211ull;
    }
    return h;
}

/* ------------------------------------------------------------- the tests --- */
static void t_silence(void)
{
    int ok = 1;
    jt_fresh();
    jt_render(0, 0, 0, 0, 0, JT_SECS(1));
    for (uint32_t i = 0; i < JT_SECS(1) / CTL * CTL; i++) ok &= !jt_l[i] && !jt_r[i];
    bad += check("jfx: silence in, every effect at rest: exactly 0 out, no effect memory taken", ok && !jt_held());
}

static void t_delay(void)
{
    static const struct { uint8_t time; uint32_t want; } T[] = {
        {0, 22050u}, {1, 11025u}, {2, 5512u}, {6, 44100u}, {7, 88200u}, {DT_FREE + 35, 346u * 441u / 10u},
    };
    int ok = 1;
    for (uint32_t ty = 0; ty < 2u; ty++)
        for (uint32_t k = 0; k < NELEM(T); k++) {
            int32_t pk;
            uint32_t at, want = T[k].want, span = want + JT_SECS(0.3);
            jt_fresh();
            song.g[G_DTYPE] = (int16_t)ty;
            song.g[G_DTIME] = T[k].time;
            song.g[G_DFDBK] = 0;
            song.g[G_DMIX] = 127;
            jt_render(2, 0, 0, 0, 16000, span);
            at = jt_peak_at(want / 2u, span / CTL * CTL, &pk);
            /* (the half and quarter rates of the long times: their filters' few ms late, as Jiant's; the repeats then
             * exactly TIME apart: below) */
            if ((int32_t)at < (int32_t)want - 16 || (int32_t)at > (int32_t)want + (want > DLY_SPAN(0u) ? 240 : 48) ||
                pk < 1500) {
                printf("  delay %s TIME %s: echo at %u (want %u), peak %d\n", ty ? "DIGITAL" : "ANALOG",
                       N_DTIME[T[k].time], at, want, pk);
                ok = 0;
            }
        }
    bad += check("jfx: DELAY's first echo where TIME says (1/4 1/8 1/16 1/2 1/1 at 120 BPM, 346 ms; half and quarter "
                 "rate a few ms late), ANALOG and DIGITAL", ok);
    {   /* 1/1 at 120 BPM (a quarter rate): the repeats exactly TIME apart */
        int32_t p1, p2;
        uint32_t a1, a2, d = 88200u;
        jt_fresh();
        song.g[G_DTIME] = 7;
        song.g[G_DFDBK] = 80;
        song.g[G_DMIX] = 127;
        jt_render(2, 0, 0, 0, 16000, 2u * d + 4096u);
        a1 = jt_peak_at(d - 400u, d + 400u, &p1);
        a2 = jt_peak_at(2u * d - 400u, 2u * d + 400u, &p2);
        bad += check("jfx: DELAY 1/1 (a quarter rate): the second repeat exactly TIME after the first (+-4)",
                     abs((int32_t)(a2 - a1) - (int32_t)d) <= 4 && p2 > 0);
    }
    {   /* FEEDBK 60 %: each repeat quieter than the one before */
        int32_t p1, p2, p3;
        uint32_t d = 11025u;
        jt_fresh();
        song.g[G_DTIME] = 1;
        song.g[G_DFDBK] = 60;
        song.g[G_DMIX] = 127;
        jt_render(2, 0, 0, 0, 16000, 4u * d + 2048u);
        jt_peak_at(d - 200u, d + 200u, &p1);
        jt_peak_at(2u * d - 200u, 2u * d + 200u, &p2);
        jt_peak_at(3u * d - 200u, 3u * d + 200u, &p3);
        bad += check("jfx: DELAY FEEDBK 60 %: the repeats fall (first > second > third)", p1 > p2 && p2 > p3 && p3 > 0);
    }
    {   /* PING and WIDE off: one signal on both sides; PING on: the sides differ */
        uint32_t n = JT_SECS(1.5), i, same = 1, diff = 0;
        jt_fresh();
        song.g[G_DTIME] = 2;
        jt_render(2, 0, 1, 2000, 12000, n);
        for (i = 0; i < n / CTL * CTL; i++) same &= jt_l[i] == jt_r[i];
        jt_fresh();
        song.g[G_DTIME] = 2;
        song.g[G_DPING] = 1;
        jt_render(2, 0, 1, 2000, 12000, n);
        for (i = 0; i < n / CTL * CTL; i++) diff += jt_l[i] != jt_r[i];
        bad += check("jfx: DELAY PING and WIDE off: L == R exactly; PING on: the echoes go to the sides", same && diff > 1000u);
    }
    {   /* memory: taken while a send is set (16 blocks of 4 KB, 4 a block), all of it given back 2 s after the sends
         * are 0 and the buses rest */
        uint32_t taken = 0, first, back, nb;
        static int32_t z[CTL];
        jt_fresh();
        song.g[G_DTIME] = 2;
        song.g[G_DFDBK] = 0;
        trk[0].p[P_DLY] = 100;
        jt_block(z, z, 0, 0, 0, 0);
        first = dly_have;
        jt_render(2, 0, 0, 0, 16000, CTL * 4u);
        for (uint32_t k = 0; k < DLY_NSEG; k++) taken += resource[RES_JFX0 + JB_DLY0 + k].size;
        nb = dly_n;
        trk[0].p[P_DLY] = 0;
        jt_render(0, 0, 0, 0, 0, JT_SECS(5));            /* (the echo, the rest, 2 s idle) */
        back = jt_held();
                bad += check("jfx: DELAY's line (the blocks its TIME needs, 4 to 16 of 4 KB, 4 a block) taken while its send is set, given back 2 s after rest",
                     first == JFX_GROW && nb >= 4 && nb <= DLY_NSEG && taken == nb * JFX_SEG * sizeof(int16_t) && back == 0u);
    }
    {   /* (round 4) TIME swept up through the line's block boundaries (4, 8, 12, 16 blocks) with the delay running on a 440 Hz tone: every grow is the rate change's fade, clear and restart,
         * so the output steps no more than the tone's own slew (12,000 x 2 pi x 440 / 44,100 = about 750 a sample) plus the echo's, and the line ends at the blocks the last TIME needs, with an echo */
        static int32_t x[CTL];
        uint32_t t = 0, i, maxstep = 0, grown[5] = {0}, ng = 0, last_n = 0;
        int32_t last = 0, pk = 0;
        jt_fresh();
        song.g[G_DFDBK] = 30;
        trk[0].p[P_DLY] = 100;
        for (uint32_t step = 0; step < 36u; step++) {
            song.g[G_DTIME] = (int16_t)(26 + step);             /* DTIME_MS[16 + step]: 56 ms up to 1.7 s: 4 blocks to 16, and the half-rate stage past 742 ms */
            for (uint32_t b = 0; b < JT_SECS(0.3) / CTL; b++) {
                for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000);
                jt_block(0, 0, 0, 0, x, 0);
                for (i = 0; i < CTL; i++) {
                    int32_t v = jo[2u * i], d = v > last ? v - last : last - v;
                    if (t > 16u * CTL && (uint32_t)d > maxstep) maxstep = (uint32_t)d;
                    if (t > 16u * CTL && abs(v) > pk) pk = abs(v);
                    last = v;
                }
                t += CTL;
            }
            if (dly_n != last_n && ng < 5u) { grown[ng++] = dly_n; last_n = dly_n; }
        }
        printf("  TIME swept: the line's blocks over the sweep %u %u %u %u %u, the largest sample step %u, peak %d\n", grown[0], grown[1], grown[2], grown[3], grown[4], maxstep, pk);
        bad += check("jfx: DELAY: TIME swept up through the line's block boundaries: the line grows (fade, clear, restart), no step beyond the tone's slew, the echo is there",
                     ng >= 2u && dly_n == DLY_NSEG && maxstep < 2400u && pk > 1000 && !jfx_dly_bad);
        {   uint32_t at3 = 0, blocks_after;                   /* and TIME back to 35 ms: under the line for 3 s, then it shrinks to 4 blocks and the 12 are given back */
            song.g[G_DTIME] = 26;
            for (uint32_t b = 0; b < JT_SECS(2.0) / CTL; b++) { for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000); jt_block(0, 0, 0, 0, x, 0); t += CTL; }
            at3 = dly_n;
            for (uint32_t b = 0; b < JT_SECS(3.0) / CTL; b++) { for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000); jt_block(0, 0, 0, 0, x, 0); t += CTL; }
            blocks_after = dly_have;
            bad += check("jfx: DELAY: TIME back down: the line shrinks to 4 blocks (at once with the rate change, else after 3 s) and gives the rest back", at3 <= DLY_NSEG && dly_n == 4u && blocks_after == 4u);
        }
        trk[0].p[P_DLY] = 0;
    }
    {   /* (round 4) PING-PONG turned on at a TIME above half the line's reach, a TIME change under way, PING-PONG off, and the GRAIN delay on with a small line: no read or write outside the line held */
        static int32_t x[CTL];
        uint32_t t = 0, i, step;
        int32_t pk = 0;
        jt_fresh();
        jfx_dly_bad = 0;
        song.g[G_DFDBK] = 40;
        trk[0].p[P_DLY] = 100;
        song.g[G_DTIME] = 44;                           /* 307 ms: 8 blocks, a reach of 16,376 samples, half of it 8,188 */
        for (step = 0; step < 6u; step++) {
            if (step == 1u) {
                (void)dly_rd_q(fx.dly_w, (int32_t)(40000u << 8));              /* a read farther back than the line holds (PING-PONG's twice TIME, a crossfade from a longer TIME): the oldest sample, not outside the ring */
                (void)dly_rd_q(fx.dly_w, (int32_t)(40000u << 8) + 77);
                (void)dly_rd_q(fx.dly_w, -(int32_t)(3u << 8));
            }
            if (step == 1u) song.g[G_DPING] = 1;        /* PING-PONG on: twice TIME is past the reach */
            if (step == 2u) song.g[G_DTIME] = 46;       /* TIME moves while the crossfades run */
            if (step == 3u) song.g[G_DPING] = 0;
            if (step == 4u) { song.g[G_DTIME] = 36; song.g[G_DPIT] = 5; }   /* GRAIN with whatever line is held */
            if (step == 5u) song.g[G_DPING] = 1;
            for (uint32_t b = 0; b < JT_SECS(0.7) / CTL; b++) {
                for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000);
                jt_block(0, 0, 0, 0, x, 0);
                for (i = 0; i < CTL; i++) if (abs(jo[2u * i]) > pk) pk = abs(jo[2u * i]);
                t += CTL;
            }
        }
        printf("  PING-PONG / TIME / GRAIN toggles: reads outside the line %u, peak %d, line %u blocks\n", jfx_dly_bad, pk, dly_n);
        bad += check("jfx: DELAY: PING-PONG at a TIME above half the reach, TIME moving, GRAIN on a small line: nothing read outside the line held", jfx_dly_bad == 0u && pk > 1000);
        song.g[G_DPING] = 0; song.g[G_DPIT] = 0;
        trk[0].p[P_DLY] = 0;
    }
    {   /* (round 4) a grow asked for and no longer needed (TIME back before the blocks were used): the blocks taken for it are given back, not held */
        static int32_t x[CTL];
        uint32_t t = 0, i;
        jt_fresh();
        song.g[G_DFDBK] = 20;
        trk[0].p[P_DLY] = 100;
        song.g[G_DTIME] = 26;                           /* 35 ms: 4 blocks */
        for (uint32_t b = 0; b < JT_SECS(0.5) / CTL; b++) { for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000); jt_block(0, 0, 0, 0, x, 0); t += CTL; }
        song.g[G_DTIME] = 63;                           /* the whole line asked for ... */
        for (uint32_t b = 0; b < 2u; b++) { for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000); jt_block(0, 0, 0, 0, x, 0); t += CTL; }
        song.g[G_DTIME] = 26;                           /* ... and TIME back at once */
        for (uint32_t b = 0; b < JT_SECS(0.5) / CTL; b++) { for (i = 0; i < CTL; i++) x[i] = jt_in(3, t + i, 0, 12000); jt_block(0, 0, 0, 0, x, 0); t += CTL; }
        printf("  abandoned grow: line %u blocks, held %u, wanted %u\n", dly_n, dly_have, dly_want);
        bad += check("jfx: DELAY: a grow no longer needed gives its blocks back (nothing held past the line)", dly_n == 4u && dly_have == 4u && dly_want == 4u);
        trk[0].p[P_DLY] = 0;
    }
    {   /* FEEDBK past 100 % (120): driven, it saturates; the output stays within the master's range */
        int32_t pk;
        jt_fresh();
        song.g[G_DTIME] = 2;
        song.g[G_DFDBK] = 120;
        jt_render(2, 1, 2, 0, 20000, JT_SECS(10));
        jt_peak_at(0, JT_SECS(10) / CTL * CTL, &pk);
        bad += check("jfx: DELAY FEEDBK 120 % driven by noise for 10 s: bounded (the master's soft clip)", pk <= 32767);
    }
}

static void t_reverb(void)
{
    static const char *const NAME[5] = {"ROOM", "SPRING", "SHIMMER", "RESO", "CLOUD"};
    int ok_tail = 1, ok_stable = 1, ok_jump = 1;
    for (uint32_t m = 0; m < 5u; m++) {
        double e1, e_end, e2, e9;
        jt_fresh();
        song.g[G_RTYPE] = (int16_t)m;
        song.g[G_RSIZE] = 90;
        jt_render(3, 0, 1, 2000, 16000, JT_SECS(8));
        e1 = jt_energy(JT_SECS(0.05), JT_SECS(1));
        e_end = jt_energy(JT_SECS(7), JT_SECS(8) / CTL * CTL);
        if (!(e1 > (m == 4u ? 1e3 : 1e4)) || !(e_end < e1 / 100.0)) {   /* (CLOUD: sparse grains, quieter) */
            printf("  reverb %s: first second %.0f, last %.0f\n", NAME[m], e1, e_end);
            ok_tail = 0;
        }
        jt_fresh();                                      /* driven hard: bounded, not growing */
        song.g[G_RTYPE] = (int16_t)m;
        song.g[G_RSIZE] = m == 4u ? 120 : 127;
        song.g[G_RDAMP] = 0;
        jt_render(3, 0, 2, 0, 24000, JT_SECS(10));
        e2 = jt_energy(JT_SECS(1), JT_SECS(2));
        e9 = jt_energy(JT_SECS(9), JT_SECS(10) / CTL * CTL);
        if (!(e9 <= 4.0 * e2 + 1.0)) {
            printf("  reverb %s driven: second 1 %.0f, second 9 %.0f\n", NAME[m], e2, e9);
            ok_stable = 0;
        }
    }
    bad += check("jfx: REVERB ROOM SPRING SHIMMER RESO CLOUD: a tail, dying to under 1 % in 7 s", ok_tail);
    bad += check("jfx: REVERB each model at full SIZE, no DAMP, driven 10 s: bounded and not growing", ok_stable);
    {   /* CLOUD at SIZE 127: FREEZE holds what it has */
        double e1, e4;
        jt_fresh();
        song.g[G_RTYPE] = 4;
        song.g[G_RSIZE] = 127;
        jt_render(3, 0, 1, JT_SECS(1), 16000, JT_SECS(5));
        e1 = jt_energy(JT_SECS(1), JT_SECS(2));
        e4 = jt_energy(JT_SECS(4), JT_SECS(5) / CTL * CTL);
        bad += check("jfx: CLOUD at SIZE 127 (FREEZE): its sound held seconds after the input stops", e1 > 1e4 && e4 > e1 / 50.0);
    }
    {   /* a model change in a tail (a low sine in): no click, a sample far off both its neighbours' line */
        int32_t worst = 0;
        jt_fresh();
        song.g[G_RSIZE] = 110;
        jt_render(3, 0, 3, 0, 12000, JT_SECS(1));
        for (uint32_t m = 1; m < 5u; m++) {
            song.g[G_RTYPE] = (int16_t)m;
            jt_render(3, 0, 3, 0, 12000, JT_SECS(0.5));
            for (uint32_t i = 2; i < JT_SECS(0.5) / CTL * CTL - 1u; i++) {
                int32_t pred = 2 * jt_l[i - 1] - jt_l[i - 2], e = abs(jt_l[i] - pred);
                if (e > worst) worst = e;
            }
        }
        bad += check("jfx: REVERB model changes in a tail: no click (no sample 1/4 of the range off its neighbours' line)",
                     worst < 8192);
        if (worst >= 8192) printf("  worst %d\n", worst);
    }
    {   /* the reverb's memory: 11.9 KB and its model line; CLOUD's 32 KB */
        uint32_t room, cloud;
        jt_fresh();
        jt_render(3, 0, 0, 0, 16000, CTL * 2u);
        room = resource[RES_JFX0 + JB_REV].size + resource[RES_JFX0 + JB_RX0].size;
        song.g[G_RTYPE] = 4;
        jt_render(3, 0, 0, 0, 16000, CTL * 4u);
        cloud = 0;
        for (uint32_t k = 0; k < RX_NSEG; k++) cloud += resource[RES_JFX0 + JB_RX0 + k].size;
        bad += check("jfx: REVERB's memory: its tank and pre-delay; CLOUD's 32 KB line when CLOUD is picked",
                     room == ((sizeof(jrev_t) + 3u) & ~3u) + SH_LEN * 2u && cloud == CL_LEN * 2u);
    }
}

static void t_chorus(void)
{
    uint32_t diff = 0, n = JT_SECS(1);
    jt_fresh();
    jt_render(1, 0, 3, 0, 12000, n);
    for (uint32_t i = 0; i < n / CTL * CTL; i++) diff += jt_l[i] != jt_r[i];
    bad += check("jfx: CHORUS: an ensemble, the two sides differ", diff > n / 2u &&
                 resource[RES_JFX0 + JB_CHO].size == CHO_LEN * sizeof(int16_t));
}

static void t_dist(void)
{
    static int32_t b[CTL];
    uint64_t h[6];
    int distinct = 1, bounded = 1, out_ok;
    for (uint32_t ty = 0; ty < 6u; ty++) {
        uint64_t hh = 1469598103934665603ull;
        jt_fresh();
        trk[0].p[P_DIST] = 100;
        trk[0].p[P_DTYPE] = (int16_t)ty;
        for (uint32_t blk = 0; blk < 200u; blk++) {
            for (uint32_t i = 0; i < CTL; i++) b[i] = jt_in(3, blk * CTL + i, 0, 16000);
            jfx_dist(&trk[0], b, 0, CTL);
            for (uint32_t i = 0; i < CTL; i++) {
                hh = (hh ^ (uint32_t)b[i]) * 1099511628211ull;
                bounded &= abs(b[i]) < 4 * 32768;
            }
        }
        h[ty] = hh;
        for (uint32_t j = 0; j < ty; j++) distinct &= h[j] != hh;
    }
    bad += check("jfx: DIST SOFT HARD FOLD CRUSH RECT AMP: six different sounds, all bounded", distinct && bounded);
    jt_fresh();                                          /* DRIVE 0: fades out, then out of the chain */
    trk[0].p[P_DIST] = 80;
    for (uint32_t blk = 0; blk < 20u; blk++) jfx_dist(&trk[0], b, 0, CTL);
    out_ok = jfx_dist_on(&trk[0]);
    trk[0].p[P_DIST] = 0;
    for (uint32_t blk = 0; blk < 200u && jfx_dist_on(&trk[0]); blk++) jfx_dist(&trk[0], b, 0, CTL);
    bad += check("jfx: DIST DRIVE 0: fades out, then out of the chain", out_ok && !jfx_dist_on(&trk[0]));
}

static void t_autopan(void)
{
    uint32_t beats = 4u, n = beat_samples() * beats / CTL, cross = 0, still = 1;
    int32_t prev = 0;
    jt_fresh();
    trk[0].p[P_APDEP] = 127;
    trk[0].p[P_APSYNC] = 1;
    trk[0].p[P_APRATE] = 4;                             /* (a cycle a beat: 1/4) */
    trk[0].p[P_APWAVE] = 0;                             /* (SIN) */
    for (uint32_t b = 0; b < n; b++) {
        int32_t v = autopan(&trk[0], 0, CTL);
        if (b && ((v >= 0) != (prev >= 0))) cross++;
        prev = v;
    }
    trk[0].p[P_APDEP] = 0;
    for (uint32_t b = 0; b < 100u; b++) still &= autopan(&trk[0], 0, CTL) == 0;
    bad += check("jfx: AUTO PAN BEAT 1/4: 7..9 swings through the centre in 4 beats; DEPTH 0: still",
                 cross >= 7u && cross <= 9u && still);
}

static void t_slicer(void)
{
    uint32_t n = JT_SECS(2), lo = 0, hi = 0, zero = 1;
    jt_fresh();
    song.g[G_SLMODE] = 0;
    jt_render(4, 0, 2, 0, 12000, CTL * 50u);
    for (uint32_t i = 0; i < CTL * 50u; i++) zero &= !jt_l[i] && !jt_r[i];
    jt_fresh();
    song.g[G_SLMODE] = 1;                               /* GATE, pattern 1, 1/16, full depth: playing */
    song.playing = 1;
    seq_start_count++;
    jt_render(4, 0, 2, 0, 12000, n);
    for (uint32_t b = 0; b < n / CTL; b++) {
        double e = 0;
        for (uint32_t i = 0; i < CTL; i++) e += fabs((double)jt_l[b * CTL + i]);
        if (e < 500.0) lo++;
        else if (e > 50000.0) hi++;
    }
    song.playing = 0;
    if (!(zero && lo > 20u && hi > 20u)) printf("  SLICER: off silent %u, quiet blocks %u, loud %u\n", zero, lo, hi);
    bad += check("jfx: SLICER GATE cuts its bus to the pattern while playing (quiet and loud blocks); OFF: its bus is out",
                 zero && lo > 20u && hi > 20u);
}

static void t_layer(void)
{
    static int32_t ref_l[JT_SECS(2)];
    uint32_t n = JT_SECS(1), P = div_samples(2), press_at = 10007u, first = 0, i, loop_err = 0, loop_sum = 0;
    int ok_start, ok_back;
    /* the reference: the noise as it is, playing */
    jt_fresh();
    song.playing = 1;
    seq_start_count++;
    jt_render(0, 0, 2, 0, 8000, n);
    memcpy(ref_l, jt_l, n * sizeof *jt_l);
    /* REPEAT 1/16 pressed at sample press_at: the output follows the reference until the next 1/16, loops from there */
    jt_fresh();
    song.playing = 1;
    seq_start_count++;
    {
        static int32_t x[CTL];
        uint32_t t = 0;
        jt_rng = 1u;
        while (t + CTL <= n) {
            for (i = 0; i < CTL; i++) x[i] = jt_in(2, t + i, 0, 8000);
            if (t <= press_at && press_at < t + CTL) perf_press(PF_R16, 1);
            if (t <= 3u * n / 4u && 3u * n / 4u < t + CTL) perf_press(PF_R16, 0);
            jt_block(x, x, 0, 0, 0, 0);
            for (i = 0; i < CTL; i++) { jt_l[t + i] = jo[2u * i]; jt_r[t + i] = jo[2u * i + 1u]; }
            t += CTL;
        }
    }
    for (i = 0; i < n && jt_l[i] == ref_l[i]; i++) {}
    first = i;
    /* (the loop records from the next 1/16 on, the input heard as it is; it plays from the 1/16 after: the first
     * change there, to the sample) */
    ok_start = first == (press_at + P - 1u) / P * P + P;
    {   /* the loop: one 1/16 like the one before (its length: the tempo's 1/16, perf_len) */
        uint32_t L = perf_len(PF_R16);
        for (i = first + L; i < first + 3u * L && i < 3u * n / 4u; i++) {   /* (two loops before it is let go) */
            loop_err += (uint32_t)abs(jt_l[i] - jt_l[i - L]);
            loop_sum += (uint32_t)abs(jt_l[i]);
        }
    }
    {
        uint32_t back = 3u * n / 4u + P, e = 0, s = 0;      /* let go: the dry signal again (the DC blocker settles) */
        for (i = back; i < n / CTL * CTL; i++) { e += (uint32_t)abs(jt_l[i] - ref_l[i]); s += (uint32_t)abs(ref_l[i]); }
        ok_back = e < s / 20u;
    }
    bad += check("jfx: FX layer REPEAT 1/16: records from the next 1/16, plays its loop exactly a 1/16 later; let go: dry again",
                 ok_start && loop_sum > 0 && loop_err < loop_sum / 10u && ok_back);
    if (!ok_start) printf("  first change at %u, 1/16 = %u, pressed at %u\n", first, P, press_at);
    {   /* a track's mute: its gain ramps to 0 while held, back after */
        static int32_t b[CTL];
        int32_t g_held, g_back;
        jt_fresh();
        perf_press(PF_M1, 1);
        for (uint32_t k = 0; k < 40u; k++) {
            for (i = 0; i < CTL; i++) b[i] = 10000;
            perf_begin(CTL);
            perf_mute(0, b, 0, CTL);
            jfx_clock_end(CTL);
        }
        g_held = b[CTL - 1u];
        perf_press(PF_M1, 0);
        for (uint32_t k = 0; k < 40u; k++) {
            for (i = 0; i < CTL; i++) b[i] = 10000;
            if (perf_begin(CTL)) perf_mute(0, b, 0, CTL);
        }
        g_back = b[CTL - 1u];
        bad += check("jfx: FX layer mute T1: the track ramps to silence while held, back after", g_held == 0 && g_back == 10000);
    }
    {   /* THROW: the dry mix into the delay and the reverb */
        static int32_t x[CTL];
        uint32_t fed = 0;
        jt_fresh();
        perf_press(PF_STHR, 1);
        for (uint32_t k = 0; k < 400u; k++) {
            int perf;
            for (i = 0; i < CTL; i++) { x[i] = 8000; mix_l[i] = mix_r[i] = x[i]; sl_l[i] = sl_r[i] = 0; send_d[i] = send_r[i] = 0; }
            jfx_clock_begin();
            master_begin();
            perf = perf_begin(CTL);
            if (perf) perf_pre(mix_l, mix_r, sl_l, sl_r, send_d, send_r, CTL);
            fed |= send_d[0] != 0 && send_r[0] != 0;
            jfx_clock_end(CTL);
        }
        perf_press(PF_STHR, 0);
        bad += check("jfx: FX layer THROW: the dry mix into the delay and reverb sends", fed);
    }
}

static void t_full(void)
{
    /* the arena taken by four engines' worth of state (all of it): the delay gets none */
    uint32_t i, same = 1, cap_main = (uint32_t)(_resource_end - _resource_start);
    static int32_t dry_l[JT_SECS(1)];
    jt_fresh();
    jt_render(0, 0, 1, 2000, 12000, JT_SECS(0.5));
    memcpy(dry_l, jt_l, JT_SECS(0.5) * sizeof *jt_l);
    jt_fresh();
    for (uint32_t k = 0; k < RES_JFX0; k++) resource_release(k);   /* (no tracks play here) */
    assert(resource_get(RES_ENGINE0, cap_main));
    assert(resource_get(RES_ENGINE0 + 1, (uint32_t)(_resource_low_end - _resource_low_start)));
    assert(resource_get(RES_ENGINE0 + 2, (uint32_t)(_resource_data_end - _resource_data_start)));
    assert(resource_get(RES_ENGINE0 + 3, (uint32_t)(_resource_retained_end - _resource_retained_start)));
    jt_render(2, 1, 1, 2000, 12000, CTL);              /* (the send: FX FULL while it wants memory) */
    int full = (jfx_full & JFX_FULL_DLY) != 0;
    memcpy(jt_l + JT_SECS(1), jt_l, CTL * sizeof *jt_l);
    jt_t0 = 0;
    {   /* the rest of the half second, after the first block */
        static int32_t x[CTL];
        uint32_t t = CTL;
        jt_rng = 1u;
        for (i = 0; i < CTL; i++) (void)jt_in(1, i, 2000, 12000);
        while (t + CTL <= JT_SECS(0.5)) {
            for (i = 0; i < CTL; i++) x[i] = jt_in(1, t + i, 2000, 12000);
            jt_block(x, x, 0, 0, x, 0);
            for (i = 0; i < CTL; i++) jt_l[t + i] = jo[2u * i];
            t += CTL;
        }
        memcpy(jt_l, jt_l + JT_SECS(1), CTL * sizeof *jt_l);
    }
    for (i = 0; i < JT_SECS(0.5) / CTL * CTL; i++) same &= jt_l[i] == dry_l[i];
    for (uint32_t k = 0; k < 4u; k++) resource_release(RES_ENGINE0 + k);
    jt_render(2, 1, 1, 2000, 12000, JT_SECS(1.5));   /* (the retry after 0.25 s, FX FULL shown 1 s past the last) */
    if (!(same && full)) printf("  FX FULL: dry %d, full %d, after %d\n", same, full, jfx_full);
    bad += check("jfx: FX FULL: a bus with no memory plays dry and says so; it comes in once there is memory",
                 same && full && !(jfx_full & JFX_FULL_DLY) && (dly_n && dly_have >= dly_n));
}

/* (0.6.0 rev 2) the arena as the device has it: banks of a few KB and the lent card pool */
static void jt_fill_to(uint32_t id, uint8_t *start, uint8_t *end, uint32_t leave)   /* a bank taken but `leave` bytes */
{
    uint32_t n = (uint32_t)(end - start);
    if (n > leave) assert(resource_get(id, n - leave));
}
static void t_arena(void)
{
    static int32_t z[CTL];
    {   /* no bank with 32 KB free, 70 KB in all: the delay's 16 blocks spread over them; it echoes */
        uint32_t banks = 0, k;
        int32_t pk = 0;
        jt_fresh();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
        resource_host_cache_enabled = 1;
        jt_fill_to(RES_ENGINE0, _resource_start, _resource_end, 30u * 1024u);
        jt_fill_to(RES_ENGINE0 + 1, _resource_data_start, _resource_data_end, 0);
        song.g[G_DTIME] = 49;     /* (562 ms: 24,784 samples, 13 blocks of the full-rate line, so 16 held, in pieces, and the first echo inside the second) */
        song.g[G_DFDBK] = 0;
        jt_render(2, 0, 0, 0, 16000, JT_SECS(1));
        for (k = 0; k < DLY_NSEG; k++) {
            uint8_t *p = resource[RES_JFX0 + JB_DLY0 + k].ptr;
            banks |= p >= _resource_start && p < _resource_end ? 1u : p >= _resource_low_start && p < _resource_low_end ? 2u
                   : p >= _resource_cache_start && p < _resource_cache_end ? 4u : 8u;
        }
        jt_peak_at(CTL, JT_SECS(1) / CTL * CTL, &pk);
        bad += check("jfx: arena in pieces (no bank with 32 KB free): the delay's blocks in three banks, it echoes",
                     (dly_n && dly_have >= dly_n) && (banks & 7u) == 7u && pk > 1000 && !(jfx_full & JFX_FULL_DLY));
        resource_host_cache_enabled = 0;
        jfx_power_on();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
    }
    {   /* the arena full: the card pool lent holds the reverb and the chorus; taken back, every block there let go */
        uint32_t k, lent = 0, after = 0;
        jt_fresh();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
        jt_fill_to(RES_ENGINE0, _resource_start, _resource_end, 0);
        jt_fill_to(RES_ENGINE0 + 1, _resource_low_start, _resource_low_end, 0);
        jt_fill_to(RES_ENGINE0 + 2, _resource_data_start, _resource_data_end, 0);
        jt_fill_to(RES_ENGINE0 + 3, _resource_retained_start, _resource_retained_end, 0);
        res_lend_ok = 15u;                              /* (all four card slots idle) */
        trk[0].p[P_REV] = 100;
        trk[0].p[P_CHOR] = 100;
        for (k = 0; k < 8u; k++) jt_block(z, z, 0, 0, 0, 0);
        for (k = 0; k < JB_N; k++) lent += resource_lent(RES_JFX0 + k) != 0u;
        int had = jrev && cho_buf && res_lend_used && lent >= 3u;
        res_lend_ok = 0;                                /* (card_play.c card_pool_take, as the host runs it: no
                                                         * interrupt to wait for, the pool back the block after) */
        int took = !res_lend_used;
        jt_block(z, z, 0, 0, 0, 0);
        for (k = 0; k < JB_N; k++) after += resource_lent(RES_JFX0 + k) != 0u;
        bad += check("jfx: card pool lent while idle: the reverb and chorus there; taken back, nothing of theirs left",
                     had && !took && !after && !res_lend_used && !jrev && !cho_buf);
        trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0;
        jfx_power_on();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
        res_lend_ok = 0;
    }
    {   /* an engine short of memory while the delay holds some: the effects give it back, the engine gets it */
        uint32_t k;
        void *eng;
        jt_fresh();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
        jt_fill_to(RES_ENGINE0, _resource_start, _resource_end, 64u * 1024u);
        jt_fill_to(RES_ENGINE0 + 1, _resource_low_start, _resource_low_end, 0);
        jt_fill_to(RES_ENGINE0 + 2, _resource_data_start, _resource_data_end, 0);
        jt_fill_to(RES_ENGINE0 + 3, _resource_retained_start, _resource_retained_end, 0);
        trk[0].p[P_DLY] = 100;
        song.g[G_DTIME] = 63;                           /* (the whole line: 16 blocks, so the engine must ask) */
        for (k = 0; k < 8u; k++) jt_block(z, z, 0, 0, 0, 0);
        int had = (dly_n && dly_have >= dly_n);
        resource_release(RES_ENGINE0 + 3);
        eng = resource_get(RES_ENGINE0 + 3, 20000u);    /* (more than the retained bank: the main bank's, the delay's) */
        int asked = !eng && res_jfx_yield;
        jt_block(z, z, 0, 0, 0, 0);
        eng = resource_get(RES_ENGINE0 + 3, 20000u);
        for (k = 0; k < JFX_YIELD - 4u; k++) jt_block(z, z, 0, 0, 0, 0);
        int waited = eng != 0 && dly_have == 0;
        for (k = 0; k < 16u; k++) jt_block(z, z, 0, 0, 0, 0);
        if (!(had && asked && waited && !res_jfx_yield && (jfx_full & JFX_FULL_DLY)))
            printf("  had %d asked %d waited %d yield %d full %d have %u\n", had, asked, waited, res_jfx_yield, jfx_full, dly_have);
        bad += check("jfx: an engine short of memory: the effects give theirs back first, wait, then take what is left",
                     had && asked && waited && !res_jfx_yield && (jfx_full & JFX_FULL_DLY));
        trk[0].p[P_DLY] = 0;
        jfx_power_on();
        for (k = 0; k < RES_JFX0; k++) resource_release(k);
    }
}

/* --------------------------------------------------------------- storage --- */
static int16_t jt_rand_in(const param_desc_t *d)
{
    jt_rng = jt_rng * 1664525u + 1013904223u;
    return (int16_t)(d->min + (int32_t)((jt_rng >> 8) % (uint32_t)(d->max - d->min + 1)));
}
static void t_storage(void)
{
    static project_t q;
    static project_store_t wire;
    static int16_t g[G_COUNT], p[NTRK][P_COUNT];
    int ok = 1;
    uint32_t i, k, h;
    jt_fresh();
    jt_rng = 77u;
    for (i = G_DTIME; i < G_COUNT; i++)
        if (param_master_jfx(i)) song.g[i] = jt_rand_in(&GP[i]);
    for (k = 0; k < NTRK; k++)
        for (i = P_DIST; i <= P_JRES6; i++)
            if (!param_jfx_free(i)) trk[k].p[i] = jt_rand_in(track_desc(&trk[k], i));
    memcpy(g, song.g, sizeof g);
    for (k = 0; k < NTRK; k++) memcpy(p[k], trk[k].p, sizeof p[k]);
    project_capture(&q);
    ok &= proj_pack(&wire, &q) && ((uint32_t *)wire.raw)[0] == PROJ_MAGIC && proj_import(&proj_scratch, &wire, sizeof wire);
    ok &= !project_restore_runtime(&proj_scratch);
    for (i = G_DTIME; i < G_COUNT; i++)
        if (param_master_jfx(i) && song.g[i] != g[i]) { printf("  global %u: %d, saved %d\n", i, song.g[i], g[i]); ok = 0; }
    for (k = 0; k < NTRK; k++)
        for (i = P_DIST; i <= P_JRES6; i++)
            if (!param_jfx_free(i) && trk[k].p[i] != p[k][i]) { printf("  T%u id %u: %d, saved %d\n", k + 1, i, trk[k].p[i], p[k][i]); ok = 0; }
    bad += check("jfx: FUN23: every effect setting (49 globals, 15 a track) saved and loaded as it was", ok);
    /* a byte out of its range (a later version's wider range, damage the hash missed): the nearest end */
    wire.raw[PROJ_JFX_WIRE + 4u + (G_DFDBK - G_DTIME)] = 250;
    wire.raw[PROJ_JFX_WIRE + 4u + (G_RPRE - G_DTIME)] = 0;
    h = proj_hash(wire.raw, sizeof wire - 4u);
    memcpy(wire.raw + sizeof wire - 4u, &h, 4);
    ok = proj_import(&proj_scratch, &wire, sizeof wire) && proj_scratch.g[G_DFDBK] == GP[G_DFDBK].max &&
         proj_scratch.g[G_RPRE] == GP[G_RPRE].min;
    wire.raw[PROJ_JFX_WIRE] = 'X';                      /* no "JFX": not a FUN23 record */
    h = proj_hash(wire.raw, sizeof wire - 4u);
    memcpy(wire.raw + sizeof wire - 4u, &h, 4);
    ok &= !proj_import(&proj_scratch, &wire, sizeof wire);
    bad += check("jfx: FUN23: an effect byte out of range loads at the range's end; a record without its JFX tag is refused", ok);
    /* the template: TPLM keeps them too */
    jt_fresh();
    jt_rng = 99u;
    for (i = G_DTIME; i < G_COUNT; i++)
        if (param_master_jfx(i)) song.g[i] = jt_rand_in(&GP[i]);
    trk[1].p[P_DLY] = 77;
    trk[2].p[P_DIST] = 33;
    memcpy(g, song.g, sizeof g);
    template_save();
    {
        static uint8_t blob[sizeof(tmpl_t)];
        memcpy(blob, &tmpl, sizeof tmpl);
        jt_fresh();
        ok = tmpl_take(blob, sizeof blob);
        template_load();
        for (i = G_DTIME; i < G_COUNT; i++)
            if (param_master_jfx(i)) ok &= song.g[i] == g[i];
        ok &= trk[1].p[P_DLY] == 77 && trk[2].p[P_DIST] == 33 && tmpl.magic == TMPL_MAGIC;
    }
    bad += check("jfx: the template (TPLM): the effects' settings saved and loaded", ok);
}

/* a FUN22 record as 0.5.6 wrote it, made from today's: the old FX wire filled, the new one cleared */
static void jt_fun22(project_store_t *w, const uint8_t type[3], uint32_t mode, const uint8_t send[NTRK][3])
{
    uint8_t *b = w->raw;
    uint32_t magic = PROJ_FUN22_MAGIC, h, t, i, pos;
    memcpy(b, &magic, 4);
    b[PROJ_FX_WIRE_OFF] = 'M'; b[PROJ_FX_WIRE_OFF + 1u] = 'F'; b[PROJ_FX_WIRE_OFF + 2u] = 'X'; b[PROJ_FX_WIRE_OFF + 3u] = 100;
    for (i = 0; i < 12u; i++) b[PROJ_MASTER_WIRE + i] = i % 4u ? 64 : type[i / 4u];
    b[PROJ_FX_MODE_OFF] = (uint8_t)mode;
    for (i = 0; i < 3u; i++) { b[PROJ_TONE_WIRE + i] = 64; b[PROJ_LP_WIRE + i] = 0; }
    memset(b + PROJ_JFX_WIRE, 0, 4u + PROJ_JFX_N);
    for (t = 0; t < NTRK; t++) {
        pos = 68u + t * (P_COUNT + 2u + NSTEP * 8u);
        for (i = 96u; i <= 107u; i++) b[pos + i] = (uint8_t)(64 + (i % 4u == 0 ? 6 : 50));   /* (inserts: ECHO, 50) */
        for (i = 0; i < 3u; i++) b[pos + 108u + i] = (uint8_t)(64 + send[t][i]);
        for (i = 111u; i <= 113u; i++) b[pos + i] = 64 + 10;                                  /* (TONEs) */
    }
    h = proj_hash(b, PROJ_STORE_SIZE - 4u);
    memcpy(b + PROJ_STORE_SIZE - 4u, &h, 4);
}
static int jt_new_defaults(const track_t *t)          /* the new ids but the sends at their defaults */
{
    int ok = 1;
    for (uint32_t i = P_DIST; i <= P_JRES6; i++)
        if (!param_jfx_send(i)) ok &= t->p[i] == track_desc(t, i)->def;
    return ok;
}
static void t_legacy(void)
{
    static project_t q;
    static project_store_t w;
    int ok;
    /* FUN22 serial (MODE 0): REVERB, ECHO, CHORUS; T1's send 90 goes to all three */
    {
        static const uint8_t type[3] = {8, 6, 9}, send[NTRK][3] = {{90, 20, 30}, {0, 0, 0}, {0, 40, 0}, {127, 0, 0}};
        jt_fresh();
        project_capture(&q);
        assert(proj_pack(&w, &q));
        jt_fun22(&w, type, 0, send);
        ok = proj_import(&proj_scratch, &w, sizeof w) && !project_restore_runtime(&proj_scratch);
        ok &= trk[0].p[P_REV] == 90 && trk[0].p[P_DLY] == 90 && trk[0].p[P_CHOR] == 90;
        ok &= !trk[1].p[P_REV] && !trk[2].p[P_DLY] && trk[3].p[P_REV] == 127 && trk[3].p[P_CHOR] == 127;
        for (uint32_t k = 0; k < NTRK; k++) ok &= jt_new_defaults(&trk[k]);
        ok &= song.g[G_LEVEL] == 0 && song.g[G_RTYPE] == GP[G_RTYPE].def && song.g[G_DTIME] == GP[G_DTIME].def;
        bad += check("jfx: FUN22 serial chain: the one send on every bus its slots had; inserts gone; leveler off", ok);
    }
    /* FUN22 separate sends (MODE 1): ROOM, PHASER, ECHO: send 1 to the reverb, send 2 nowhere, send 3 to the delay */
    {
        static const uint8_t type[3] = {7, 1, 6}, send[NTRK][3] = {{90, 20, 30}, {0, 55, 66}, {0, 0, 0}, {5, 0, 0}};
        jt_fresh();
        project_capture(&q);
        assert(proj_pack(&w, &q));
        jt_fun22(&w, type, 1, send);
        ok = proj_import(&proj_scratch, &w, sizeof w) && !project_restore_runtime(&proj_scratch);
        ok &= trk[0].p[P_REV] == 90 && trk[0].p[P_DLY] == 30 && !trk[0].p[P_CHOR];
        ok &= !trk[1].p[P_REV] && trk[1].p[P_DLY] == 66 && trk[3].p[P_REV] == 5;
        bad += check("jfx: FUN22 separate sends: each send on the bus of its slot's kind (PHASER's: none)", ok);
    }
    /* the frozen fixtures */
    {
        static uint8_t b[BANK_STORE_SIZE];
        static const struct { const char *path; uint8_t kind; int16_t rev, dly, chor; } F[] = {
            {"tests/fixtures/fx/fun16.bin", 0, 0, 0, 0}, {"tests/fixtures/fx/fbkk.bin", 1, 0, 0, 0},
            {"tests/fixtures/fx/tplf.bin", 2, 0, 0, 0}, {"tests/fixtures/fx/fun17.bin", 0, 0, 0, 0},
            {"tests/fixtures/fx/fbkk17.bin", 1, 0, 0, 0}, {"tests/fixtures/fx/tplg.bin", 2, 0, 0, 0},
            {"tests/fixtures/fx/fun18.bin", 0, 0, 0, 0}, {"tests/fixtures/fx/fbkk18.bin", 1, 0, 0, 0},
            {"tests/fixtures/fx/tplh.bin", 2, 0, 0, 0},
            {"tests/fixtures/fun19-send-chain.bin", 0, 81, 0, 0}, {"tests/fixtures/tpli-send-chain.bin", 2, 81, 0, 0},
            {"tests/fixtures/fun21-cuts.bin", 0, -1, 71, -1}, {"tests/fixtures/tplk-cuts.bin", 2, -1, 71, -1},
        };
        ok = 1;
        for (uint32_t f = 0; f < NELEM(F); f++) {
            FILE *fp = fopen(F[f].path, "rb");
            uint32_t n;
            int one = 1;
            if (!fp) { printf("  %s: missing\n", F[f].path); ok = 0; continue; }
            n = (uint32_t)fread(b, 1, sizeof b, fp);
            fclose(fp);
            jt_fresh();
            if (F[f].kind == 2) {
                one = tmpl_take(b, n);
                if (one) template_load();
            } else {
                one = F[f].kind ? bank_valid(b, n) : proj_import(&proj_scratch, b, (int)n);
                one = one && !project_restore_runtime(&proj_scratch);
                if (one && F[f].kind) bank_restore(b);
            }
            one = one && (F[f].rev < 0 || TSEL->p[P_REV] == F[f].rev) && (F[f].dly < 0 || TSEL->p[P_DLY] == F[f].dly) &&
                  (F[f].chor < 0 || TSEL->p[P_CHOR] == F[f].chor) && jt_new_defaults(TSEL);
            one = one && (F[f].kind == 2 || song.g[G_LEVEL] == 0);
            if (!one) {
                printf("  %s: REV %d DLY %d CHOR %d, DIST %d\n", F[f].path, TSEL->p[P_REV], TSEL->p[P_DLY], TSEL->p[P_CHOR],
                       TSEL->p[P_DIST]);
                ok = 0;
            }
        }
        bad += check("jfx: frozen FUN16 .. FUN21, FBKK, TPLF .. TPLK: the old sends on their buses, the new settings' defaults", ok);
    }
}

/* ----------------------------------------------------- listening, cost --- */
static void jt_wav(const char *name, uint32_t n)
{
    char path[160];
    FILE *f;
    snprintf(path, sizeof path, "build/audition/%s.wav", name);
    f = fopen(path, "wb");
    if (!f) return;
    wav_hdr(f, n);
    for (uint32_t i = 0; i < n; i++) wav_put(f, clamp(jt_l[i], -32767, 32767), clamp(jt_r[i], -32767, 32767));
    fclose(f);
}
static void t_audition(void)
{
    static const struct { const char *name; uint8_t where; int16_t id, v, id2, v2; } A[] = {
        {"delay_analog", 2, G_DTYPE, 0, G_DFDBK, 60}, {"delay_digital", 2, G_DTYPE, 1, G_DFDBK, 60},
        {"delay_ping", 2, G_DPING, 1, G_DFDBK, 70}, {"delay_grain", 2, G_DPIT, 12, G_DSPRY, 40},
        {"reverb_room", 3, G_RTYPE, 0, G_RSIZE, 100}, {"reverb_spring", 3, G_RTYPE, 1, G_RSIZE, 100},
        {"reverb_shimmer", 3, G_RTYPE, 2, G_RSIZE, 100}, {"reverb_reso", 3, G_RTYPE, 3, G_RSIZE, 100},
        {"reverb_cloud", 3, G_RTYPE, 4, G_RSIZE, 110}, {"chorus", 1, G_CDEPTH, 90, G_CSTER, 40},
    };
    uint32_t n = JT_SECS(4) / CTL * CTL;
    mkdir("build/audition", 0755);
    jt_fresh();
    jt_render(0, 0, 4, JT_SECS(2), 12000, n);
    jt_wav("dry", n);
    for (uint32_t k = 0; k < NELEM(A); k++) {
        jt_fresh();
        song.g[A[k].id] = A[k].v;
        song.g[A[k].id2] = A[k].v2;
        jt_render(A[k].where, 1, 4, JT_SECS(2), 12000, n);
        jt_wav(A[k].name, n);
    }
    bad += check("jfx: audition WAVs (build/audition: dry and each delay / reverb model, chorus)", 1);
}
/* (round 8) the delay time and the right echo's offset hold against the external clock's few samples of wobble (jfx.c dly_hold): a synced TIME
 * is the same value through 2,000 wobbles of 1 to 8 samples in the beat; a tempo step of 1 BPM, a division change and a free TIME pass at once;
 * the right echo's offset follows a 2 sample wobble not at all and is exactly 0 with WIDE at 0 or PING-PONG on; the free TIME and the return to a synced
 * one are exact (no old held value comes back) */
static void t_dly_hold(void)
{
    int ok = 1;
    jt_fresh();
    fx_bus_clear();
    fxp.g[G_DTIME] = 1;
    fxp.beat = 22050u;
    uint32_t dl0 = delay_samples(), dro0 = dly_dro(64, 0), x, wob = 0;
    ok &= dl0 == 11025u && dro0 == 22050u / 8u * 64u / 127u;
    for (uint32_t i = 0; i < 2000u; i++) {
        wob = (wob * 5u + 3u) % 17u;                              /* 0..16 -> -8..+8 samples */
        fxp.beat = 22050u + wob - 8u;
        ok &= delay_samples() == dl0;
        ok &= dly_dro(64, 0) == dro0;
    }
    fxp.beat = (uint32_t)((uint64_t)22050u * 120u / 121u);       /* 1 BPM faster: passes at once */
    x = delay_samples();
    ok &= x != dl0 && x == fxp_div(1);
    uint32_t dro1 = dly_dro(64, 0);
    ok &= dro1 != dro0 && dro1 == fxp_div(3) * 64u / 127u;
    ok &= dly_dro(0, 0) == 0u && dly_dro(64, 1) == 0u;           /* WIDE 0 and PING-PONG: exactly none */
    ok &= dly_dro(64, 0) == fxp_div(3) * 64u / 127u;             /* and back from none: the value itself */
    fxp.g[G_DTIME] = 2;                                           /* a division change: at once */
    ok &= delay_samples() == fxp_div(2);
    fxp.g[G_DTIME] = DT_FREE + 35;                                /* a free TIME: the table's, exactly */
    ok &= delay_samples() == 346u * 441u / 10u;
    fxp.g[G_DTIME] = 1;                                           /* back to a synced one: its own value, not an old held one */
    ok &= delay_samples() == fxp_div(1);
    printf("  DELAY hold: a synced TIME and the right echo's offset hold through the external clock's wobble, pass a 1 BPM step, a division, a free TIME: %s\n", ok ? "ok" : "FAIL");
    if (!ok) bad++;
}
static void t_cost(void)
{
    static const struct { const char *name; uint8_t where; int16_t id, v; } C[] = {
        {"dry (no effect)", 0, G_DTYPE, 0}, {"DELAY ANALOG", 2, G_DTYPE, 0}, {"DELAY DIGITAL", 2, G_DTYPE, 1},
        {"REVERB ROOM", 3, G_RTYPE, 0}, {"REVERB SPRING", 3, G_RTYPE, 1}, {"REVERB SHIMMER", 3, G_RTYPE, 2},
        {"REVERB RESO", 3, G_RTYPE, 3}, {"REVERB CLOUD", 3, G_RTYPE, 4}, {"CHORUS", 1, G_CDEPTH, 60},
    };
    printf("jfx: host time a %u-sample block (x86 -O1, this machine; not the device):\n", CTL);
    for (uint32_t k = 0; k < NELEM(C); k++) {
        struct timespec a, b;
        uint32_t n = JT_SECS(4) / CTL * CTL;
        jt_fresh();
        song.g[C[k].id] = C[k].v;
        clock_gettime(CLOCK_MONOTONIC, &a);
        jt_render(C[k].where, 1, 2, 0, 12000, n);
        clock_gettime(CLOCK_MONOTONIC, &b);
        printf("  %-16s %6.0f ns\n", C[k].name, ((b.tv_sec - a.tv_sec) * 1e9 + (b.tv_nsec - a.tv_nsec)) / (n / CTL));
    }
}

int main(void)
{
    t_silence();
    t_delay();
    t_reverb();
    t_chorus();
    t_dist();
    t_autopan();
    t_slicer();
    t_layer();
    t_full();
    t_arena();
    t_storage();
    t_legacy();
    t_audition();
    t_dly_hold();
    t_cost();
    printf(bad ? "jfx: %d FAILED\n" : "jfx: all passed\n", bad);
    return bad ? 1 : 0;
}
