/* SPDX-License-Identifier: GPL-3.0-only
 * Orange Cat 0.4.20: frozen tracks, the audio side (spec 0.9, 10.6). (0.5.6) Frozen phrases.
 *
 * A frozen phrase plays a recording of itself (its take, a WAV on the card) in place of its notes. The take is one
 * pass of the phrase, recorded from the track it was frozen on: that track's sound, level, pan and own effects, as
 * it went into the mix. The freeze belongs to the phrase (frzp[] below, kept in the record store with the phrase
 * pool it belongs to): whenever a track plays that phrase, in any mode (phrase, chain, song) and on any track, the
 * take plays and the phrase's notes stay quiet; when the track goes on to another phrase, its engine plays that one.
 * A frozen phrase's notes, settings and motion cannot be edited until it is unfrozen (the UI's frz_locked).
 *
 * Each block, after the sequencer's events (seq.c marks the frame a track begins a pass on: seq_pass_at; and asks
 * frz_pass at that moment whether the take plays the pass, so the notes stay quiet from its first step):
 *   - a pass of a frozen phrase whose take is ready in the track's player slot: the take starts there, from its first
 *     frame, and plays the pass (passes of the same phrase in a row: it plays on, looping, the take being one pass);
 *   - a pass of anything else while a take plays: the take plays up to that frame and stops (a short fade), the main
 *     loop rewinds it (usb_files.c uf_rearm) for the next time;
 *   - the sequencer stopping (or a RESTART) stops it the same way.
 * The track's engine renders as ever (no new notes reach it while the take plays: its tails ring out underneath, and
 * the next phrase's notes start on time). A frozen phrase whose take is not ready (no card, no free player slot, a
 * changed tempo, a phrase that follows another frozen one with no time to load) plays its notes instead.
 *
 * The take's mix (mix.c mix_frz_take): its LEVEL as against the one it was frozen at, its MUTE and SOLO (the same
 * ramp as the engine's), its effect sends; its pan, spread and own effects are in the take.
 *
 * Recording a freeze (frz[k], usb_host.c): the track's stereo stem (mix.c mix_stem_l / _r) into the recorder's ring
 * (card_play.c cr: slots 2 and 3 of the players' memory, 104 ms), frames [from, to) of each block (frz_after, from
 * audio.c after the mix); the main loop writes it out (usb_files.c). It records a pass of the phrase being frozen
 * that begins at least one pass after the start (so the tails of the pass before are in its start), and FZ_X frames
 * past its end (blended into its start: its end runs on into its start with no click).
 *
 * Included by melodee.c after seq.c and usb_host.c (frz[]); mix.c and audio.c reach it through mix.c's hooks
 * (mix_frz_block, mix_frz_take, mix_frz_after) and seq.c through seq_frz_hook, all set by frz_init (main.c): host
 * tests without it run without.
 *
 * Revisions:
 *   0.4.20: first version: a frozen track.
 *   0.5.5: the take recorded FZ_HEAD bits down, played back FZ_HEAD bits up. A track's stem is the mix's own scale, where
 *          the master's limiter brings the sum down after: one track alone runs past 16 bits (measured on the host: a
 *          VA BASS note 1.3 times full scale, ORGAN DRAWBAR's two notes 2.6, a four note chord on FM KEYS 4.4). 0.4.20
 *          held it to 16 bits, so frozen tracks came back clipped hard (heard on the unit: badly distorted). 3 bits
 *          (18 dB) holds 8 times full scale; the take keeps 13 of its bits for a stem at full scale, the noise of the
 *          lowest still under what the DAC plays. On a computer a take plays 18 dB quieter than the track did.
 *   0.5.6: frozen phrases (Jeremiah, October 22, 2026): the freeze and its take belong to the phrase, not the track;
 *          the take is one pass of the phrase (0.4.20: a cycle of the track: its chain or its song rows); a track
 *          plays the take for each pass of a frozen phrase and its engine for the rest; the phrase's notes stay quiet
 *          while the take plays (seq.c seq_frz_quiet; 0.4.20 skipped the engine instead, so its notes were never
 *          heard and its tails stopped); the engine renders all the time (the swap fade into the take is gone: a
 *          take starts at its pass, its first frames the blend of its own end).
 */
#define FRZ_AUDIO 1                                     /* (0.5.6) this file is in: ui.c, ui_draw.c use it, not their stand-ins */
#define FZ_X 2048u                                      /* frames recorded past the pass's end (46 ms): the blend */
#define FZ_HEAD 3                                       /* (0.5.5) bits of headroom in a take (see above) */
#define FZ_FADE 32u                                     /* (0.5.6) frames a take fades out over when it stops mid-block */

/* ---- (0.5.6) the frozen phrases: one entry per phrase of the pool (oc_pool.c, 64), kept with it (usb_files.c
 * frz_table_save: the record store, OCS_FREEZE) ---- */
#define FZP_N 64u
typedef struct {
    uint16_t take;                                      /* its take number (T<trk + 1>_<take>.WAV when it was recorded
                                                         * here; 1 for a clip assigned from the card); 0: not frozen */
    uint8_t trk;                                        /* the track it was recorded on (or assigned from) */
    uint8_t ver;                                        /* FZP_VER */
    int16_t lvl;                                        /* that track's LEVEL then (the take has it in) */
    int16_t bpm;                                        /* the tempo then (song.g[G_BPM]): another tempo, its notes play */
    uint32_t frames;                                    /* its length: one pass */
    uint32_t hash;                                      /* the phrase's CRC then (oc_pool.c ocp.hash): the phrase changed
                                                         * since (emptied, a copy over it, an edit kept another way), the
                                                         * freeze no longer counts */
    uint8_t dir;                                        /* (0.6.1) the folder its file is in now (usb_files.c frzd: 0 is
                                                         * /ORANGECAT itself, where 0.4.20 .. 0.6.0 put every take) */
    uint8_t want;                                       /* (0.6.1) the folder it belongs in (its project's): the file is
                                                         * moved there once the card is in and the sequencer stopped */
    char name[14];                                      /* (0.6.1) its file's name in that folder, 13 characters at most */
} frzp_t;
_Static_assert(sizeof(frzp_t) == 32u, "a frozen phrase's entry: 32 bytes");
#define FZP_VER 2u                                      /* (0.6.1: 2, the folder and the name; 1 was 16 bytes, the file
                                                         * always /ORANGECAT/T<trk + 1>_<take>.WAV) */
static frzp_t frzp[FZP_N];
static volatile uint8_t frzp_dirty;                     /* changed since kept (the main loop keeps it, stopped) */
static const uint32_t *frz_pool_hash;                   /* oc_pool.c's ocp.hash (set by usb_files.c frz_table_load;
                                                         * host tests without the pool: not checked) */

/* the phrase track k is on (1..64; 0 none: steps not kept as a phrase yet) */
static uint32_t frz_phrase_of(uint32_t k)
{
    uint32_t id = bank_pid[k % NTRK][trk[k % NTRK].pattern % NPAT];
    return id <= FZP_N ? id : 0u;
}
static int frz_phrase_frozen(uint32_t id)
{
    return id && id <= FZP_N && frzp[id - 1u].take && (!frz_pool_hash || frz_pool_hash[id - 1u] == frzp[id - 1u].hash);
}
/* track k is on a frozen phrase: what it plays there cannot be edited (the UI), Stage shows it tinted */
static int frz_track_frozen(uint32_t k) { return k < NTRK && frz_phrase_frozen(frz_phrase_of(k)); }

/* ---- (0.5.6) each track's player: the slot its takes play in, the phrase loaded there ---- */
enum { FZW_NONE, FZW_CARD, FZW_FULL, FZW_TEMPO, FZW_SLOW, FZW_ERROR };
static struct {
    uint8_t slot;                                       /* its player slot (card_play.c cp[]), CP_N none */
    uint8_t phr;                                        /* the phrase whose take is in it (1..64), 0 none */
    uint8_t why;                                        /* FZW_*: why its frozen phrase plays its notes instead */
    uint8_t fail;                                       /* the phrase whose take failed to load (not tried again until
                                                         * the track moves on or the card comes back) */
    volatile uint8_t kill;                              /* (main loop) stop the take now: its phrase unfrozen */
    volatile uint8_t stop_at_to;                        /* (audio) this block the take stops at fzi.play_to */
} fzt[4] = {{CP_N, 0, 0, 0, 0, 0}, {CP_N, 0, 0, 0, 0, 0}, {CP_N, 0, 0, 0, 0, 0}, {CP_N, 0, 0, 0, 0, 0}};

/* the MIXER page's word for a track (ui_pages.c pv_strip, where its pan is; ui_draw.c's FREEZE column): a freeze
 * being made (Card: waking it; Ready: stopped, PLAY; Wait: the pass before; Rec; Save), the phrase it is on frozen
 * (Frozen; or why it plays its notes: Card?, Full, Tempo, Slow, Error), a failed freeze's word ("Stop", "Short",
 * "Lvl 0", "Save?" ..), "" none */
static const char *frz_word(uint32_t k, int playing)
{
    static const char *const W[] = {"", "Card", "Ready", "Rec", "Rec", "Save", "", "Stop"};
    static const char *const Y[] = {"Frozen", "Card?", "Full", "Tempo", "Slow", "Error"};
    if (k >= 4u) return "";
    if (frz[k].st != FZ_OFF) return frz[k].st == FZ_ARMED && playing ? "Wait" : W[frz[k].st & 7u];
    if (frz_track_frozen(k)) return Y[fzt[k].why < 6u ? fzt[k].why : 0u];
    return frz[k].why;
}
/* the MIXER's colour for a track's word: 2 frozen, 1 a freeze being made, 0 neither (a failure's word) */
static uint32_t frz_state(uint32_t k)
{
    if (k >= 4u) return 0;
    return frz[k].st != FZ_OFF ? 1u : frz_track_frozen(k) ? 2u : 0u;
}

/* ---- the audio side ---- */
static struct {
    uint32_t pass;                                      /* passes since the sequencer started; FZ_NONE before the first */
    uint32_t got;                                       /* frames recorded */
    volatile uint32_t frames;                           /* the pass's frames (known at its end) */
    uint32_t from, to;                                  /* this block: frames [from, to) recorded (equal: none) */
    uint32_t play_from, play_to;                        /* this block: the take plays frames [play_from, play_to) */
    volatile uint32_t lost;                             /* frames the ring had no room for (the card behind) */
} fzi[4];
#define FZ_NONE 0xFFFFFFFFu
static uint32_t fz_was_starts, fz_was_playing;

/* (seq.c, a pass of track k begins: audio) 1 = the take plays it: the phrase is frozen and its take is in the
 * track's slot, rewound and ready (or already playing: the same phrase again) */
static int frz_pass(uint32_t k)
{
    uint32_t id;
    const struct cp_stream *c;
    if (k >= 4u || fzt[k].slot >= CP_N) return 0;
    id = frz_phrase_of(k);
    if (!frz_phrase_frozen(id) || fzt[k].phr != id || fzt[k].why) return 0;
    c = &cp[fzt[k].slot];
    return c->trk == k + 1u && (c->on || c->armed);
}

/* after the sequencer's events, each block: the passes counted; a take started at a frozen pass, stopped at another
 * (or with the sequencer); a freeze armed, recorded, ended */
static void frz_block(uint32_t n)
{
    uint32_t k, start = seq_start_count != fz_was_starts && song.playing, stop = fz_was_playing && !song.playing;
    fz_was_starts = seq_start_count;
    fz_was_playing = song.playing;
    if (!song.playing) seq_frz_quiet = 0;               /* (0.5.6: stopped, nothing to keep quiet) */
    for (k = 0; k < 4u && k < NTRK; k++) {
        uint32_t at = seq_pass_at[k], mark, bit = 1u << k, quiet = (seq_frz_quiet >> k) & 1u;
        struct cp_stream *c = fzt[k].slot < CP_N ? &cp[fzt[k].slot] : 0;
        if (start || !song.playing) fzi[k].pass = FZ_NONE;
        mark = at != SEQ_NOPASS && song.playing;
        if (at > n) at = n;
        if (mark) fzi[k].pass++;                        /* (FZ_NONE + 1: the first pass is 0) */
        fzi[k].from = fzi[k].to = 0;
        fzi[k].play_from = 0;
        fzi[k].play_to = n;
        fzt[k].stop_at_to = 0;
        if (fzt[k].kill) {                              /* (its phrase unfrozen while it plays: the notes from here) */
            fzt[k].kill = 0;
            seq_frz_quiet &= (uint8_t)~bit;
            quiet = 0;
            if (c && c->trk == k + 1u && c->on) {
                fzi[k].play_to = n < FZ_FADE ? n : FZ_FADE;
                fzt[k].stop_at_to = 1;
            }
        } else if (c && c->trk == k + 1u) {             /* the track's take */
            if (c->on && (stop || start || (!mark && !quiet))) {
                fzi[k].play_to = n < FZ_FADE ? n : FZ_FADE;   /* the sequencer stopped or started again (a RESTART:
                                                         * that pass plays neither, the take comes back at the next), or
                                                         * the phrase changed mid-pass (a launch NOW): over, faded; the
                                                         * main loop rewinds it */
                fzt[k].stop_at_to = 1;
            } else if (mark && quiet && !c->on && c->armed) {   /* a frozen pass: the take from its first frame */
                c->armed = 0;
                c->on = 1;
                fzi[k].play_from = at;
                mix_frozen |= (uint8_t)bit;
            } else if (mark && !quiet && c->on) {       /* another phrase's pass: the take up to here */
                fzi[k].play_to = at;
                fzt[k].stop_at_to = 1;
            }
        }
        switch (frz[k].st) {
        case FZ_ARMED:                                  /* a pass of the phrase, a whole pass after the start */
            if (mark && fzi[k].pass >= 1u && frz_phrase_of(k) == frz[k].phrase) {
                fzi[k].got = 0;
                fzi[k].from = at;
                fzi[k].to = n;
                frz[k].st = FZ_REC;
            }
            break;
        case FZ_REC:
            if (stop || start) { frz[k].st = FZ_FAIL; break; }   /* (stopped mid-take: the main loop drops it) */
            fzi[k].to = n;
            if (mark) {                                 /* the pass's end: its frames; then FZ_X more */
                fzi[k].frames = fzi[k].got + at;
                frz[k].st = FZ_POST;
                if (FZ_X < n - at) fzi[k].to = at + FZ_X;
            }
            break;
        case FZ_POST:
            if (stop || start) { frz[k].st = FZ_SAVE; break; }   /* (what it has of the blend will do) */
            fzi[k].to = fzi[k].frames + FZ_X - fzi[k].got < n ? fzi[k].frames + FZ_X - fzi[k].got : n;
            break;
        default:
            break;
        }
    }
}

/* a track's block while its take plays (mix.c mix_part, after its engine): frames [play_from, play_to), at the
 * track's level against the one the phrase was frozen at, its mute and solo ramp (as mix_fx_samples), into the mix
 * and its effect sends; stopping in the block: faded out over FZ_FADE frames up to play_to, then over */
static int32_t fz_l[CTL], fz_r[CTL];
static int frz_take(track_t *t, uint32_t n)
{
    uint32_t ti = (uint32_t)(t - trk), i, from, to, id;
    struct cp_stream *c;
    int32_t g, lvl0, gain, target;
    if (ti >= 4u || n > CTL || fzt[ti].slot >= CP_N) { mix_frozen &= (uint8_t)~(1u << (ti & 7u)); return 0; }
    c = &cp[fzt[ti].slot];
    if (!c->on) {                                       /* (stopped: nothing more of it) */
        mix_frozen &= (uint8_t)~(1u << ti);
        return 1;
    }
    from = fzi[ti].play_from;
    to = fzi[ti].play_to < n ? fzi[ti].play_to : n;
    if (to > from) cp_pull(c, fz_l, fz_r, from, to);   /* (a stop at frame 0: nothing pulled) */
    id = fzt[ti].phr;
    lvl0 = LEVEL_Q12[(id && id <= FZP_N ? frzp[id - 1u].lvl : t->p[P_LEVEL]) & 127];
    g = lvl0 ? (int32_t)((uint32_t)LEVEL_Q12[t->p[P_LEVEL] & 127] * 4096u / (uint32_t)lvl0) : 4096;   /* Q12 */
    if (g > 16384) g = 16384;                           /* (0.5.5) at most 4 times (+12 dB) the level it was frozen at */
    gain = solo_gain[ti];
    target = !t->p[P_MUTE] && (!track_solo || ((track_solo >> ti) & 1u)) ? 32768 : 0;
    for (i = 0; i < n; i++) {
        int32_t l, r;
        gain += clamp(target - gain, -256, 256);
        if (i < from || i >= to) continue;
        l = (fz_l[i] * g) >> (12 - FZ_HEAD);            /* (0.5.5: the take's headroom back; 32,767 x a Q12 gain of up
                                                         * to 16,384 fits 31 bits) */
        r = (fz_r[i] * g) >> (12 - FZ_HEAD);
        l = fx_scale(l, (uint32_t)gain * 2u);           /* (as mix_fx_samples: no 64 bits) */
        r = fx_scale(r, (uint32_t)gain * 2u);
        if (fzt[ti].stop_at_to && to - i <= FZ_FADE) {  /* (0.5.6) stopping: faded out to play_to */
            l = (l * (int32_t)(to - i)) >> 5;
            r = (r * (int32_t)(to - i)) >> 5;
        }
        mix_add(l, r, i);                               /* (0.6.0: where the track goes, the SLICER's bus too) */
        mix_send((l + r) >> 1, i);                      /* .. its sends, mono (mix.c: the track's gains) */
    }
    if (fzt[ti].stop_at_to) {                           /* over: the main loop rewinds it (uf_rearm) */
        c->on = 0;
        c->ended = 1;
        mix_frozen &= (uint8_t)~(1u << ti);
        fzt[ti].stop_at_to = 0;
    }
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
                int32_t l = mix_stem_l[i] >> FZ_HEAD, r = mix_stem_r[i] >> FZ_HEAD;   /* (0.5.5: headroom) */
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
    seq_frz_hook = frz_pass;
}
