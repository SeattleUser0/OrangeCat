/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.20: frozen tracks, the audio side (spec 0.9, 10.6). A frozen track plays a recording of itself (its
 * take, a WAV on the card) in place of its engine; the engine stops costing processor time.
 *
 * Its take is one cycle of the track: the passes of its phrase until what it plays comes round again (usb_files.c
 * frz_cycle counts them: 1 in the phrase mode, the chain's steps times their REPS in the chain mode, its block of
 * song rows in the song mode). The sequencer marks, each block, the frame a track begins a pass on (seq.c
 * seq_pass_at); this file counts them from the sequencer's start, so everything here lands on the sample:
 *   - the take is recorded from a cycle's first pass (after one whole cycle has played, so the tails of the one
 *     before are in its start) to the next cycle's, and FZ_X frames past it (usb_files.c blends those into its
 *     start, so its end runs on into its start with no click, and cuts the file to the cycle);
 *   - it loops at exactly the sequencer's own cycle (its frames are the sequencer's), so it does not drift;
 *   - it starts on a cycle's first pass: at the sequencer's start, or, frozen while playing, the next one. That block
 *     the track's engine fades out and the take fades in from the frame (mix.c mix_part_swap).
 * While the take plays, mix.c skips the engine (mix_frozen) and mixes the take as the track (mix_frz_take): its LEVEL
 * as against the one it was frozen at, its MUTE and SOLO (the same ramp as the engine's), its effect sends. Its pan,
 * spread and own effects are in the take. The sequencer stopping (or a RESTART) stops it; the main loop rewinds it,
 * and the engine plays until it comes in again.
 *
 * Recorded: the track's stereo stem (mix.c mix_stem_l / _r, as 0.4.19's track takes) into the recorder's ring (card_play.c
 * cr: slots 2 and 3 of the players' memory, 116 ms), frames [from, to) of each block (frz_after, from audio.c after
 * the mix); the main loop writes it out (usb_files.c).
 *
 * Included by melodee.c after seq.c (its marks) and usb_host.c (frz[]); mix.c and audio.c reach it through mix.c's
 * hooks (mix_frz_block, mix_frz_take, mix_frz_after), set by frz_init (main.c): host tests without it run without.
 *
 * Revisions:
 *   0.4.20: first version.
 */
#define FZ_X 2048u                                      /* frames recorded past the cycle's end (46 ms): the blend */
static struct {
    uint32_t pass;                                      /* passes since the sequencer started; FZ_NONE before the first */
    uint32_t rec_end;                                   /* the pass the take ends on */
    uint32_t got;                                       /* frames recorded */
    volatile uint32_t frames;                           /* the cycle's frames (known at its end) */
    uint32_t from, to;                                  /* this block: frames [from, to) recorded (equal: none) */
    uint32_t play_from;                                 /* this block: the take comes in at this frame */
    volatile uint32_t lost;                             /* frames the ring had no room for (the card behind) */
} fzi[4];
#define FZ_NONE 0xFFFFFFFFu
static uint32_t fz_was_starts, fz_was_playing;

/* after the sequencer's events, each block: the passes counted; a take armed, recorded, ended; a frozen track's take
 * started at its cycle, stopped with the sequencer */
static void frz_block(uint32_t n)
{
    uint32_t k, start = seq_start_count != fz_was_starts && song.playing, stop = fz_was_playing && !song.playing;
    fz_was_starts = seq_start_count;
    fz_was_playing = song.playing;
    mix_frz_swap = 0;
    for (k = 0; k < 4u && k < NTRK; k++) {
        uint32_t at = seq_pass_at[k], mark, cyc, bit = 1u << k;
        struct cp_stream *c = &cp[frz[k].slot % CP_N];
        if (start || !song.playing) fzi[k].pass = FZ_NONE;
        mark = at != SEQ_NOPASS && song.playing;
        if (at > n) at = n;
        if (mark) fzi[k].pass++;                        /* (FZ_NONE + 1: the first pass is 0) */
        fzi[k].from = fzi[k].to = 0;
        fzi[k].play_from = 0;
        cyc = frz[k].passes ? frz[k].passes : 1u;
        switch (frz[k].st) {
        case FZ_ARMED:                                  /* a cycle's first pass, a whole cycle after the start */
            if (mark && fzi[k].pass >= cyc && fzi[k].pass % cyc == 0u) {
                fzi[k].rec_end = fzi[k].pass + cyc;
                fzi[k].got = 0;
                fzi[k].from = at;
                fzi[k].to = n;
                frz[k].st = FZ_REC;
            }
            break;
        case FZ_REC:
            if (stop || start) { frz[k].st = FZ_FAIL; break; }   /* (stopped mid-take: the main loop drops it) */
            fzi[k].to = n;
            if (mark && fzi[k].pass == fzi[k].rec_end) {   /* the cycle's end: its frames; then FZ_X more */
                fzi[k].frames = fzi[k].got + at;
                frz[k].st = FZ_POST;
                if (FZ_X < n - at) fzi[k].to = at + FZ_X;
            }
            break;
        case FZ_POST:
            if (stop || start) { frz[k].st = FZ_SAVE; break; }   /* (what it has of the blend will do) */
            fzi[k].to = fzi[k].frames + FZ_X - fzi[k].got < n ? fzi[k].frames + FZ_X - fzi[k].got : n;
            break;
        case FZ_ON:
            if ((stop || start) && (c->on || (mix_frozen & bit))) {   /* the engine back; the main loop rewinds it */
                if (c->on) c->ended = 1;
                c->on = 0;
                mix_frozen &= (uint8_t)~bit;
            }
            if (c->armed && mark && fzi[k].pass % cyc == 0u) {   /* its cycle: the take comes in, the engine goes */
                c->armed = 0;
                c->on = 1;
                fzi[k].play_from = at;
                if (!(mix_frozen & bit)) {
                    mix_frz_swap |= (uint8_t)bit;
                    mix_frz_at[k] = (uint8_t)at;
                }
                mix_frozen |= (uint8_t)bit;
            }
            break;
        default:
            break;
        }
    }
}

/* a frozen track's block (mix.c mix_part): its take, from where it came in, at the track's level against the one it
 * was frozen at, its mute and solo ramp (as mix_fx_samples), into the mix and its effect sends; the block it came in,
 * faded in over MIX_FRZ_FADE frames (its engine fades out there: mix_part_swap) */
static int32_t fz_l[CTL], fz_r[CTL];
static int frz_take(track_t *t, uint32_t n)
{
    uint32_t ti = (uint32_t)(t - trk), i, from, send = fx_send_on ? fx_send_gain(t) : 0u, fade;
    struct cp_stream *c;
    int32_t g, lvl0, gain, to;
    if (ti >= 4u || n > CTL) return 0;
    c = &cp[frz[ti].slot % CP_N];
    if (!c->on) {                                       /* (stopped this block: silence, the engine next) */
        mix_frozen &= (uint8_t)~(1u << ti);
        return 1;
    }
    from = fzi[ti].play_from;
    fade = (mix_frz_swap >> ti) & 1u;
    cp_pull(c, fz_l, fz_r, from, n);
    lvl0 = LEVEL_Q12[frz[ti].lvl & 127];
    g = lvl0 ? (int32_t)((uint32_t)LEVEL_Q12[t->p[P_LEVEL] & 127] * 4096u / (uint32_t)lvl0) : 4096;   /* Q12 */
    gain = solo_gain[ti];
    to = !t->p[P_MUTE] && (!track_solo || ((track_solo >> ti) & 1u)) ? 32768 : 0;
    for (i = 0; i < n; i++) {
        int32_t l, r;
        gain += clamp(to - gain, -256, 256);
        if (i < from) continue;
        l = (fz_l[i] * g) >> 12;
        r = (fz_r[i] * g) >> 12;
        l = fx_scale(l, (uint32_t)gain * 2u);           /* (as mix_fx_samples: no 64 bits) */
        r = fx_scale(r, (uint32_t)gain * 2u);
        if (fade && i - from < MIX_FRZ_FADE) {          /* (the take's share: 0..MIX_FRZ_FADE) */
            l = (l * (int32_t)(i - from)) >> 5;
            r = (r * (int32_t)(i - from)) >> 5;
        }
        mix_add(l, r, i, send);
    }
    solo_gain[ti] = gain;
    return 1;
}

/* after the mix, each block (audio.c): the frames [from, to) of a take being recorded, from the track's stem, into
 * the recorder's ring (the main loop writes it out); the post-roll's end ends it */
static void frz_after(uint32_t n)
{
    uint32_t k, i;
    for (k = 0; k < 4u; k++) {
        uint32_t from = fzi[k].from, to = fzi[k].to, wi;
        if (to <= from || (frz[k].st != FZ_REC && frz[k].st != FZ_POST && frz[k].st != FZ_SAVE)) continue;
        if (to > n) to = n;
        if (!mix_stem_n || mix_stem_trk != k) {         /* (the stem not kept: silence) */
            for (i = from; i < to; i++) mix_stem_l[i] = mix_stem_r[i] = 0;
        }
        if (cr.cap - (cr.w - cr.r) < to - from) {       /* the card behind: frames lost (the take is spoiled) */
            fzi[k].lost += to - from;
        } else {
            wi = cr.wi;
            for (i = from; i < to; i++) {
                int32_t l = mix_stem_l[i], r = mix_stem_r[i];
                l = l > 32767 ? 32767 : l < -32768 ? -32768 : l;
                r = r > 32767 ? 32767 : r < -32768 ? -32768 : r;
                cr.buf[2u * wi] = (int16_t)l;
                cr.buf[2u * wi + 1u] = (int16_t)r;
                if (++wi == cr.cap) wi = 0;
            }
            cr.wi = wi;
            RING_PUBLISH();
            cr.w = cr.w + (to - from);
        }
        fzi[k].got += to - from;
        if (frz[k].st == FZ_POST && fzi[k].got >= fzi[k].frames + FZ_X) frz[k].st = FZ_SAVE;
    }
}

static void frz_init(void)
{
    mix_frz_block = frz_block;
    mix_frz_take = frz_take;
    mix_frz_after = frz_after;
}
