/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat: WAVs playing from the card (build doc phase 2, the plan): the audio side of frozen tracks' takes. Up to
 * four streams, each a ring of 16-bit frames (mono kept mono, stereo stereo) that the card reading fills
 * (card_stream.c, from the 10 kHz timer) and the audio ISR plays (card_freeze.c frz_take, as the track).
 *
 * Each stream follows its file's rate by stepping through its ring at rate / 44100 (Q16) with a straight line between
 * two frames: a 48 kHz file plays at its own pitch. When the ISR finds fewer frames than it needs it plays silence
 * for the rest of the block and counts a gap (cp[].gaps). (0.4.14) A gap is one dropout, however many blocks it lasts.
 *
 * Memory: 36 KB in the .pool section, four fixed slots of 9 KB (a stream takes a free slot): 2,304 stereo or 4,608
 * mono frames each, 52 ms or 104 ms at 44.1 kHz. .pool is shared with the audio working arena, which must keep 48 KB.
 * The rings are not powers of two, so each side keeps its own index and wraps it (no division per sample).
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
 *   0.4.16: the recorder's ring (cr): what the FM-1 plays, as 16-bit stereo, for usb_files.c to write to a WAV.
 *   0.4.17: in time with the sequencer: armed WAVs and takes started with it.
 *   0.4.19: a track take is stereo: the track as it goes into the mix (mix.c mix_stem_l / _r, picked by mix_stem_trk).
 *   0.4.20: the slots 9 KB (36 KB in all): the freeze's code in the audio RAM took the arena's last 928 bytes. A freeze
 *           records into slots 2 and 3 (4,608 frames, 104 ms; the slowest write measured, 25 ms). cp_pull split out
 *           of card_render; cp[].trk marks a frozen track's take (card_freeze.c).
 *   0.5.3: the drive test is gone, and with it what played and recorded for its page: card_render (WAVs added at the
 *          DAC), card_capture (the mix or a track to the recorder: a freeze fills the ring itself, card_freeze.c
 *          frz_after), card_sync (WAVs and takes started and stopped with the sequencer: frozen takes have their own,
 *          card_freeze.c frz_block), playing a file once (eof), cr's level meter and losses, and the 256 bytes of
 *          card_render's buffers (cp_tl, cp_tr).
 */
#ifndef CR_STEM_TRK
#define CR_STEM_TRK(t) (mix_stem_trk = (uint8_t)(t))    /* (0.4.19) mix.c keeps track t's stereo stem (0xFF: none) */
#endif
#ifndef CP_POOL_DEFINED                                 /* (0.6.0 rev 2: the firmware defines it in resources.c, which
                                                         * lends it to the effects while the card is idle) */
#define CP_N 4u                                         /* streams at once */
#define CP_SLOT 4608u                                   /* 16-bit words a slot: 9 KB (0.4.20: was 10 KB; the freeze's audio
                                                         * code took the arena's margin) */
static int16_t cp_pool[CP_N * CP_SLOT] __attribute__((section(".pool"), aligned(4)));
#endif
static volatile uint8_t res_lend_ok, res_lend_used;     /* (resources.c's: bit s, slot s lent to the effects) */
/* (0.6.0 rev 2) the slots in mask (bit s: slot s) back from the effects before the card uses them: their lending
 * stopped, then (the device) a wait for the audio interrupt to give back every effect with a block there (jfx.c
 * jfx_lend_check, the next block, ~1 ms). 1: the slots are the card's (until usb_files.c finds them unused and lends
 * them again); 0: the audio did not answer in 100 ms */
static int card_pool_take(uint32_t mask)
{
    res_lend_ok &= (uint8_t)~mask;
    RING_PUBLISH();
#ifdef FM1_IRQ_TARGET
    {
        uint32_t t0 = fm1_ms;
        while ((res_lend_used & mask) && fm1_ms - t0 < 100u)
            ;
    }
#endif
    return !(res_lend_used & mask);
}
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
    volatile uint8_t armed;                             /* (0.4.17) waits for its cycle to come round (on: 0) */
    volatile uint8_t ended;                             /* (0.4.17) the sequencer stopped it: the main loop rewinds it */
    uint8_t trk;                                        /* (0.4.20) k + 1: track k's frozen take (card_freeze.c plays it) */
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
    c->armed = c->ended = 0;
    c->trk = 0;
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

/* (0.4.20) stream c's frames [from, n) into l and r at the file's level (no MASTER), 0 where it runs short, a gap
 * counted (card_freeze.c mixes them as the track) */
static void cp_pull(struct cp_stream *c, int32_t *l, int32_t *rt, uint32_t from, uint32_t n)
{
    uint32_t i, r, w, ri, ph, st, cap, ch, next, sh = 0;
    const int16_t *b;
    r = c->r; w = c->w; ri = c->ri; ph = c->phase; st = c->step; cap = c->cap; ch = c->ch; b = c->buf;
    for (i = 0; i < from && i < n; i++) l[i] = rt[i] = 0;
    for (i = from; i < n; i++) {
        int32_t f, a0, a1, x, y;
        if (w - r < 2u) {                               /* short: silence for the rest of the block */
            sh = 1;
            break;
        }
        next = ri + 1u == cap ? 0u : ri + 1u;
        f = (int32_t)(ph >> 1);                         /* Q15 */
        a0 = b[ri * ch];
        a1 = b[next * ch];
        x = a0 + (((a1 - a0) * f) >> 15);
        if (ch == 2u) {
            a0 = b[ri * 2u + 1u];
            a1 = b[next * 2u + 1u];
            y = a0 + (((a1 - a0) * f) >> 15);
        } else {
            y = x;
        }
        l[i] = x;
        rt[i] = y;
        ph += st;
        while (ph >= 65536u) {                          /* (a step above 1: a file faster than 44.1 kHz) */
            ph -= 65536u;
            r++;
            if (++ri == cap) ri = 0;
        }
    }
    for (; i < n; i++) l[i] = rt[i] = 0;
    c->phase = ph;
    c->ri = ri;
    c->r = r;
    if (sh && !c->shrt) c->gaps++;                      /* (0.4.14) a dropout starts */
    c->shrt = (uint8_t)sh;
}
/* ---- (0.4.16) the recorder's ring: a freeze's take on its way to the card (card_freeze.c frz_after fills it, the
 * main loop writes it out: usb_files.c ur_write) ---- */
static struct {
    volatile uint32_t w, r;                             /* frames in (the ISR), frames written to the card (main loop) */
    uint32_t wi;                                        /* where the ISR writes */
    uint32_t cap;                                       /* frames the ring holds: a freeze's 4,608 (104 ms) */
    int16_t *buf;                                       /* (0.4.20) the ring: slots 2 and 3 of the players' memory */
} cr;
