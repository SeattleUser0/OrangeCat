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
 *   0.4.16: the recorder's ring (cr, card_capture): what the FM-1 plays, as 16-bit stereo, for usb_files.c to write
 *           to a WAV on the card. It takes the players' 40 KB (nothing plays from the card while it records):
 *           10,240 frames, 232 ms. The audio ISR fills it (audio.c, before the click and the card's own WAVs, so
 *           neither is recorded; after MASTER, as USB audio takes it); the main loop empties it 8 KB at a time. A
 *           block that finds no room is dropped and counted (cr.lost, frames).
 *   0.4.17: in time with the sequencer (card_sync, from audio.c each block, after the sequencer's events): a WAV
 *           started while the sequencer is stopped waits, its ring full (armed), and plays from its first frame in
 *           the block the sequencer starts; it plays once (eof: the reading reached the end; the ring running dry
 *           then ends it with no gap counted), and stops when the sequencer stops or starts again (RESTART); the
 *           main loop then rewinds it to wait again (ended). A take armed the same way starts and stops with the
 *           sequencer. The recorder can take one track instead of the mix (cr.ch 1: mono, the track's own signal
 *           before its pan, the effects and MASTER: Melodee's USB stems, mix.c track_capture). WAVs play at their
 *           own level times MASTER (was -6 dB): a frozen track comes back as loud as it went in.
 *   0.4.20: the slots 9 KB (36 KB in all): the freeze's code in the audio RAM took the arena's last 928 bytes. A slot
 *           holds 2,304 stereo frames (52 ms); the recorder 9,216 (209 ms), a freeze 4,608 (104 ms; the slowest write
 *           measured, 25 ms). cp_pull split out of card_render; cp[].trk marks a frozen track's take (card_freeze.c).
 *   0.4.19: a track take is stereo (spec 0.9, 10.6): the track as it goes into the mix, after its level, pan,
 *           spread and its own effects, before the shared effects and MASTER (mix.c mix_stem_l / _r, picked by
 *           mix_stem_trk). The mono take (0.4.17, the USB stems before pan) is gone: every take is stereo.
 */
#ifndef CP_GAIN
#define CP_GAIN() ((int32_t)song.master_q12)            /* (host tests: a fixed level) */
#endif
#ifndef CR_STEM_TRK
#define CR_STEM_TRK(t) (mix_stem_trk = (uint8_t)(t))    /* (0.4.19) mix.c keeps track t's stereo stem (0xFF: none) */
#endif
#define CP_N 4u                                         /* streams at once */
#define CP_SLOT 4608u                                   /* 16-bit words a slot: 9 KB (0.4.20: was 10 KB; the freeze's audio
                                                         * code took the arena's margin) */
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
    volatile uint8_t armed;                             /* (0.4.17) waits for the sequencer to start (on: 0) */
    volatile uint8_t synced;                            /* (0.4.17) plays with the sequencer: stops with it, plays once */
    volatile uint8_t eof;                               /* (0.4.17) the reading has fed the file's last frame */
    volatile uint8_t ended;                             /* (0.4.17) the ISR stopped it (the end, a stop): the main loop
                                                         * rewinds it */
    uint8_t trk;                                        /* (0.4.20) 0: the page's; k + 1: track k's frozen take
                                                         * (card_freeze.c plays it, card_render and card_sync leave it) */
} cp[CP_N];
#ifndef CTL_MAX_CP
#ifdef CTL
#define CTL_MAX_CP CTL                                  /* (0.4.20) frames a block at most (audio.c's blocks) */
#else
#define CTL_MAX_CP 256u
#endif
#endif

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
    c->armed = c->synced = c->eof = c->ended = 0;
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

/* add the streams to one block of DAC samples (stereo Q15, after the master and the click) */
/* (0.4.20) stream c's frames [from, n) into l and r at the file's level (no MASTER), 0 where it runs short; the
 * gaps and a once-played file's end counted as card_render did (split out of it: a frozen track's take is mixed as
 * the track, card_freeze.c, not added at the DAC) */
static void cp_pull(struct cp_stream *c, int32_t *l, int32_t *rt, uint32_t from, uint32_t n)
{
    uint32_t i, r, w, ri, ph, st, cap, ch, next, sh = 0;
    const int16_t *b;
    r = c->r; w = c->w; ri = c->ri; ph = c->phase; st = c->step; cap = c->cap; ch = c->ch; b = c->buf;
    for (i = 0; i < from && i < n; i++) l[i] = rt[i] = 0;
    for (i = from; i < n; i++) {
        int32_t f, a0, a1, x, y;
        if (w - r < 2u && !(c->eof && w - r == 1u)) {   /* short: silence for the rest of the block */
            sh = 1;
            break;
        }
        next = w - r < 2u ? ri : ri + 1u == cap ? 0u : ri + 1u;   /* (0.4.17: a file's last frame, nothing after) */
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
    if (sh && c->eof) {                                 /* (0.4.17) played once to its end: over, not a gap */
        c->on = 0;
        c->ended = 1;
        return;
    }
    if (sh && !c->shrt) c->gaps++;                      /* (0.4.14) a dropout starts */
    c->shrt = (uint8_t)sh;
}
/* add the streams the page plays (not a frozen track's: card_freeze.c) to one block of DAC samples (stereo Q15, after
 * the master and the click) */
static int32_t cp_tl[CTL_MAX_CP], cp_tr[CTL_MAX_CP];
static __attribute__((noinline)) void card_render(int32_t *out, uint32_t n)
{
    uint32_t s, i, any = 0;
    int32_t g = CP_GAIN();
    if (n > CTL_MAX_CP) n = CTL_MAX_CP;
    for (s = 0; s < CP_N; s++) {
        struct cp_stream *c = &cp[s];
        if (!c->on || c->trk)
            continue;
        any = 1;
        cp_pull(c, cp_tl, cp_tr, 0, n);
        for (i = 0; i < n; i++) {
            out[2u * i] += (cp_tl[i] * g) >> 12;        /* MASTER (Q12) (0.4.17: at the file's level; was -6 dB) */
            out[2u * i + 1u] += (cp_tr[i] * g) >> 12;
        }
    }
    if (any)                                            /* (0.4.13) several at once: held to the DAC's full scale (Q15 x 2:
                                                         * audio.c's OUT_SHIFT puts Q15 at -6 dBFS), never wrapped */
        for (i = 0; i < 2u * n; i++)
            out[i] = out[i] > 65535 ? 65535 : out[i] < -65536 ? -65536 : out[i];
}

/* ---- (0.4.16) the recorder's ring ---- */
#define CR_CAP (CP_N * CP_SLOT / 2u)                    /* stereo frames: 9,216 (36 KB, 209 ms; 0.4.16 to 0.4.19: 232 ms) */
static struct {
    volatile uint32_t w, r;                             /* frames in (the ISR), frames written to the card (main loop) */
    uint32_t wi;                                        /* where the ISR writes */
    volatile uint8_t on;                                /* the ISR records */
    volatile uint32_t lost;                             /* frames dropped: no room */
    volatile int32_t peak;                              /* the loudest sample since the page last looked */
    uint8_t ch, trk;                                    /* 2 (0.4.19: always; 0.4.17 had 1 for a track); track trk (0..3)
                                                         * alone, or the mix (trk 0xFF) */
    uint32_t cap;                                       /* frames the ring holds: 9,216 (209 ms); a freeze's 4,608 (104 ms) */
    volatile uint8_t armed;                             /* (0.4.17) starts when the sequencer starts */
    volatile uint8_t synced;                            /* (0.4.17) stops when the sequencer stops */
    volatile uint8_t ended;                             /* (0.4.17) the sequencer stopped it: the main loop saves it */
    int16_t *buf;                                       /* (0.4.20) the ring: the whole pool (the page's REC), or slots 2
                                                         * and 3 (a freeze: frozen takes play in 0 and 1) */
} cr;
/* ready to record: src 0 the mix, 1..4 that track; armed: from the sequencer's next start */
static void cr_start(uint32_t src, int armed)
{
    cr.on = cr.armed = cr.synced = cr.ended = 0;
    cr.ch = 2u;
    cr.trk = (uint8_t)(src ? src - 1u : 0xFFu);
    cr.buf = cp_pool;
    cr.cap = CP_N * CP_SLOT / cr.ch;
    cr.w = cr.r = cr.wi = 0;
    cr.lost = 0;
    cr.peak = 0;
    CR_STEM_TRK(cr.trk);
    RING_PUBLISH();
    if (armed) cr.armed = cr.synced = 1;
    else cr.on = 1;
}
/* one block into the ring, held to 16 bits: the mix (stereo Q15, MASTER in), or (0.4.19) the track's stereo stem
 * (sl, sr: mix.c's mix_stem_l / _r; 0 when the track is not in use: silence) */
static __attribute__((noinline)) void card_capture(const int32_t *out, const int32_t *sl, const int32_t *sr, uint32_t n)
{
    uint32_t i, wi;
    int32_t pk;
    int16_t *b;
    if (!cr.on)
        return;
    if (cr.cap - (cr.w - cr.r) < n) {                   /* the card is behind: this block is lost */
        cr.lost = cr.lost + n;
        return;
    }
    wi = cr.wi;
    pk = cr.peak;
    b = cr.buf;
    for (i = 0; i < n; i++) {
        int32_t l, r;
        if (cr.trk == 0xFFu) { l = out[2u * i]; r = out[2u * i + 1u]; }   /* the mix */
        else if (sl) { l = sl[i]; r = sr[i]; }          /* (0.4.19) the track */
        else l = r = 0;
        l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
        r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
        b[2u * wi] = (int16_t)l;
        b[2u * wi + 1u] = (int16_t)r;
        if (l < 0) l = -l;
        if (r < 0) r = -r;
        if (l > pk) pk = l;
        if (r > pk) pk = r;
        if (++wi == cr.cap) wi = 0;
    }
    cr.wi = wi;
    cr.peak = pk;
    RING_PUBLISH();
    cr.w = cr.w + n;
}

/* (0.4.17) the sequencer, once a block (audio.c, after mix_block: its events start and stop the transport): playing
 * now, and how many times it has started (seq.c seq_start counts them in seq_start_count: a RESTART starts it again without a stop).
 * Armed WAVs and takes start in the block it starts; synced ones stop in the block it stops or starts again. */
static void card_sync(uint32_t playing, uint32_t starts)
{
    static uint32_t was_playing, was_starts;
    uint32_t s, start = starts != was_starts && playing, stop = (was_playing && !playing) || start;
    was_playing = playing;
    was_starts = starts;
    if (!start && !stop)
        return;
    for (s = 0; s < CP_N; s++) {
        struct cp_stream *c = &cp[s];
        if (c->trk) continue;                           /* (0.4.20) a frozen track's: card_freeze.c */
        if (c->on && c->synced) {                       /* (a RESTART too: it rewinds and waits for the next start) */
            c->on = 0;
            c->ended = 1;
        } else if (c->armed && start) {
            c->armed = 0;
            c->on = 1;
        }
    }
    if (cr.on && cr.synced && stop) {
        cr.on = 0;
        cr.ended = 1;
    } else if (cr.armed && start) {
        cr.armed = 0;
        cr.on = 1;
    }
}
