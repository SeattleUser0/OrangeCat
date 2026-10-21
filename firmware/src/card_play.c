/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: WAVs playing from the USB drive (build doc phase 2, the plan). The audio side only: up to four
 * streams, each a ring of 16-bit frames (mono kept mono, stereo stereo) that the card reading fills
 * (card_stream.c, from the 10 kHz timer) and the audio ISR plays (card_render, from audio.c audio_block, after the
 * click: at the DAC, with MASTER, each stream -6 dB, the sum held to the DAC's full scale).
 *
 * Each stream follows its file's rate by stepping through its ring at rate / 44100 (Q16) with a straight line between
 * two frames: a 48 kHz file plays at its own pitch. When the ISR finds fewer frames than it needs it plays silence
 * for the rest of the block and counts a gap (cp[].gaps): the page shows them, so a test says whether the card keeps
 * up. (0.4.14) A gap is one dropout, however many blocks it lasts (0.4.13 counted blocks: thousands a second).
 *
 * Memory: 40 KB in the .pool section, four fixed slots of 10 KB (a stream takes a free slot): 2,560 stereo or 5,120
 * mono frames each, 58 ms or 116 ms at 44.1 kHz. .pool is shared with the audio working arena, which must keep 48 KB;
 * 40 KB is about all the player can have. The rings are not powers of two, so each side keeps its own index and wraps
 * it (no division per sample).
 *
 * One producer per ring (the card reading: w, wi), one consumer (the audio ISR: r, ri). The producer writes the
 * frames, then w (RING_PUBLISH between); the ISR reads w once a block. w and r count frames for ever (their difference
 * is the fill); wi and ri are the places in the ring.
 *
 * Revisions:
 *   0.4.11: first version (one stream, 64 KB then 32 KB, filled by the main loop).
 *   0.4.12: the card read from the timer interrupt (card_stream.c), straight into the ring.
 *   0.4.13: up to four streams in four 10 KB slots; mono kept mono (twice the time a slot holds); the sum clamped.
 *   0.4.14: a gap counted once per dropout (short: the last block ran short), not once per block.
 */
#ifndef CP_GAIN
#define CP_GAIN() ((int32_t)song.master_q12)            /* (host tests: a fixed level) */
#endif
#define CP_N 4u                                         /* streams at once */
#define CP_SLOT 5120u                                   /* 16-bit words a slot: 10 KB */
static int16_t cp_pool[CP_N * CP_SLOT] __attribute__((section(".pool"), aligned(4)));
static struct cp_stream {
    volatile uint32_t w, r;                             /* frames written (producer), frames passed (ISR), for ever */
    uint32_t wi, ri;                                    /* where in the ring: the producer's, the ISR's */
    volatile uint8_t on;                                /* the ISR plays it */
    uint8_t ch;                                         /* 1 mono, 2 stereo, as kept in the ring */
    uint32_t cap;                                       /* frames the ring holds */
    int16_t *buf;
    uint32_t step, phase;                               /* file frames per output frame (Q16), the fraction now */
    volatile uint32_t gaps;                             /* dropouts: the ring ran short (0.4.14: once each) */
    uint8_t shrt;                                       /* (0.4.14) the last block ran short */
} cp[CP_N];

/* a slot's ring made ready for a file of ch channels (1 or 2 kept), rate Hz; the ISR does not play it yet */
static void cp_setup(uint32_t s, uint32_t ch, uint32_t rate)
{
    struct cp_stream *c = &cp[s];
    c->on = 0;
    c->buf = cp_pool + s * CP_SLOT;
    c->ch = (uint8_t)(ch > 1u ? 2u : 1u);
    c->cap = CP_SLOT / c->ch;
    c->w = c->r = 0;
    c->wi = c->ri = 0;
    c->phase = 0;
    c->gaps = 0;
    c->shrt = 0;
    c->step = rate * 16384u / 11025u;                   /* rate / 44100 in Q16 (192 kHz fits 32 bits) */
}
static uint32_t cp_room(uint32_t s) { return cp[s].cap - 2u - (cp[s].w - cp[s].r); }   /* (2: the line looks ahead) */
/* one frame in (the producer) */
static void cp_put(uint32_t s, int16_t l, int16_t r)
{
    struct cp_stream *c = &cp[s];
    int16_t *o = c->buf + c->wi * c->ch;
    o[0] = l;
    if (c->ch == 2u) o[1] = r;
    if (++c->wi == c->cap) c->wi = 0;
    RING_PUBLISH();
    c->w = c->w + 1u;
}

/* add the streams to one block of DAC samples (stereo Q15, after the master and the click) */
static __attribute__((noinline)) void card_render(int32_t *out, uint32_t n)
{
    uint32_t s, i, any = 0;
    int32_t g = CP_GAIN();
    for (s = 0; s < CP_N; s++) {
        struct cp_stream *c = &cp[s];
        uint32_t r, w, ri, ph, st, cap, ch, next, sh = 0;
        const int16_t *b;
        if (!c->on)
            continue;
        any = 1;
        r = c->r; w = c->w; ri = c->ri; ph = c->phase; st = c->step; cap = c->cap; ch = c->ch; b = c->buf;
        for (i = 0; i < n; i++) {
            int32_t f, a0, a1, l, rr;
            if (w - r < 2u) {                           /* short: silence for the rest of the block */
                sh = 1;
                break;
            }
            next = ri + 1u == cap ? 0u : ri + 1u;
            f = (int32_t)(ph >> 1);                     /* Q15 */
            a0 = b[ri * ch];
            a1 = b[next * ch];
            l = a0 + (((a1 - a0) * f) >> 15);
            if (ch == 2u) {
                a0 = b[ri * 2u + 1u];
                a1 = b[next * 2u + 1u];
                rr = a0 + (((a1 - a0) * f) >> 15);
            } else {
                rr = l;
            }
            out[2u * i] += (l * g) >> 13;               /* MASTER (Q12), -6 dB */
            out[2u * i + 1u] += (rr * g) >> 13;
            ph += st;
            while (ph >= 65536u) {                      /* (a step above 1: a file faster than 44.1 kHz) */
                ph -= 65536u;
                r++;
                if (++ri == cap) ri = 0;
            }
        }
        c->phase = ph;
        c->ri = ri;
        c->r = r;
        if (sh && !c->shrt) c->gaps++;                  /* (0.4.14) a dropout starts */
        c->shrt = (uint8_t)sh;
    }
    if (any)                                            /* (0.4.13) several at once: held to the DAC's full scale (Q15 x 2:
                                                         * audio.c's OUT_SHIFT puts Q15 at -6 dBFS), never wrapped */
        for (i = 0; i < 2u * n; i++)
            out[i] = out[i] > 65535 ? 65535 : out[i] < -65536 ? -65536 : out[i];
}
