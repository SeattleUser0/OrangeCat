/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 * Adapted from MIDI clock contributions by ChanceTheMaker and keremimo (2026).
 * Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Audio ISR state. Musical phase is exactly 1024 units per MIDI pulse; changing
 * the measured tempo cannot move steps, gates, ties or the arpeggiator. */
static struct {
    uint32_t pos, rendered, last_ms, start_ms, last_time, interval_time, tempo_time, tempo_dt;
    uint32_t lost_pos, lost_time, lost_n;      /* (0.14.2) the resync after a lost clock: the last pulse's place and time, pulses since */
    uint8_t mode, selected, have_pulse, tempo_valid, tempo_n, tempo_count;
    uint8_t lost;                              /* 0 none; 1 stopped by a lost clock, no pulse back yet; 2 counting the pulses back */
} midi_clock;

/* Preserve fractional sequencer/ARP phase when external transport takes over. */
/* (0.14.2) the clock's counters (editor command 103 CLOCK_STATS; the tests): pulses taken from the selected source, the lost-clock stops,
 * the resyncs, the longest gap between two pulses (ms) and the pulses the last resync found missed */
static uint32_t midi_clock_pulses, midi_clock_losses, midi_clock_resyncs, midi_clock_gap_ms, midi_clock_missed;
static void midi_clock_mode(uint32_t mode)
{
    midi_clock.lost = 0;
    uint32_t old = seq_beat_samples();
    midi_sync_active = (uint8_t)mode;
    uint32_t next = seq_beat_samples();
    for (uint32_t i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        if (t->seq_pos < 0x7FFFFFFFu) t->seq_pos = (uint32_t)((uint64_t)t->seq_pos * next / old);
        if (t->arp_pos < 0xFFFFFFFu) t->arp_pos = (uint32_t)((uint64_t)t->arp_pos * next / old);
        t->arp_off = (uint32_t)((uint64_t)t->arp_off * next / old);
    }
    beat_pos = (uint32_t)((uint64_t)beat_pos * next / old);
    midi_clock.mode = (uint8_t)mode;
}

/* Local PLAY / a REC note: follow the master only while its clock is running. A stopped master
 * (Stop received, pulses gone) hands timing back to the internal clock until its next Start,
 * Continue or Stop, so PLAY never dies on the lost-clock timeout. 1 = sync kept. */
static uint32_t midi_clock_local_start(uint32_t ms)
{
    midi_clock.lost = 0;                       /* (0.14.2) a local PLAY: no resync */
    if (!midi_clock.mode)
        return 0;
    if (midi_clock.tempo_valid && ms - midi_clock.last_ms <= 500u)
        return 1;
    midi_clock_mode(0);
    return 0;
}

/* (Orange Cat 0.14.2) The resync. 500 ms without a pulse stops the sequencer (seq.c: a lost cable must not leave a held note), and before
 * this nothing but a Start, Continue or local PLAY ran it again: a master whose clock came back played on alone (the soak's stop, hw/LOG.md,
 * October 26, 2026). Now the clock coming back resumes it in time with the beat. At the stop the last pulse's place (pos) and time are
 * kept; the first pulse back gives the pulses missed from the time since and the tempo known (interval_time, rounded), the following
 * ones count on exactly. The sequencer had rendered on to where the next pulse would have been (midi_clock_advance stops a unit short
 * of it), so it resumes, as a Continue does, on the pulse 1 + 24 k after the last one: there the master is where it stopped within the
 * beat, a whole number of beats later, and nothing missed is played in a burst. Called for a pulse while stopped. A Start, Continue or
 * Stop, a local PLAY or STOP, a change of CLK, or an outage over 60 s (or with no tempo known) ends it: the unit waits for Start, as
 * before. (Skiff 2.0.1, ~/ref/skiff b03d98a, stops the same way and has no resync: this one is Orange Cat's.) */
static void midi_clock_lost(void)
{
    midi_clock_losses++;
    midi_clock.lost = midi_clock.have_pulse && midi_clock.interval_time ? 1u : 0u;
    midi_clock.lost_pos = midi_clock.pos;
    midi_clock.lost_time = midi_clock.last_time;
    midi_clock.lost_n = 0;
}
static void midi_clock_resync(uint32_t ms, uint32_t time)
{
    uint32_t iv = midi_clock.interval_time;
    if (midi_clock.lost == 1u) {
        uint32_t el = time - midi_clock.lost_time;
        if (!iv || el > 60000u * MIDI_TICKS_PER_MS) { midi_clock.lost = 0; return; }
        /* (six pulses' smoothed length at an eighth of a tick, tempo_dt, when known: millisecond host clocks round iv) */
        midi_clock.lost_n = midi_clock.tempo_dt ? (uint32_t)(((uint64_t)el * 48u + midi_clock.tempo_dt / 2u) / midi_clock.tempo_dt)
                                                : (el + iv / 2u) / iv;
        midi_clock_missed = midi_clock.lost_n ? midi_clock.lost_n - 1u : 0u;   /* (the pulses that never came) */
        midi_clock.lost = 2u;
    } else
        midi_clock.lost_n++;
    if (midi_clock.lost_n % 24u != 1u)
        return;
    midi_clock.lost = 0;
    cin_left = 0;
    midi_clock.pos = midi_clock.lost_pos + MIDI_BEAT_UNITS / 24u;
    if ((int32_t)(midi_clock.rendered - midi_clock.pos) > 0) midi_clock.pos = midi_clock.rendered;
    midi_clock.have_pulse = 0;                 /* (this pulse marks the place, as after a Continue: the next one moves on) */
    midi_clock.start_ms = ms;
    motion_begin();
    song.rec_wait = song.rec;
    song.rec_pass = 0;
    song.playing = 1;
    midi_clock_resyncs++;
}

static __attribute__((noinline)) void midi_clock_transport(uint32_t status, uint32_t ms)
{
    midi_clock.lost = 0;                       /* (0.14.2) the master's own transport: no resync */
    if (status == 0xFAu) {
        midi_clock.pos = midi_clock.rendered = 0;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_time = 0;
        midi_clock.start_ms = ms;
        seq_start();
    } else if (status == 0xFBu) {
        cin_left = 0;
        midi_clock.pos = midi_clock.rendered;
        midi_clock.have_pulse = midi_clock.tempo_valid = 0;
        midi_clock.interval_time = 0;
        midi_clock.start_ms = ms;
        if (!song.playing) {
            motion_begin();
            song.rec_wait = song.rec;
            song.rec_pass = 0;
        }
        song.playing = 1;
    } else if (status == 0xFCu) {
        seq_stop();
    }
}

static __attribute__((noinline)) void midi_clock_pulse(uint32_t ms, uint32_t time)
{
    midi_clock_pulses++;
    if (midi_clock.last_ms && ms - midi_clock.last_ms > midi_clock_gap_ms) midi_clock_gap_ms = ms - midi_clock.last_ms;
    if (midi_clock.lost && !song.playing)
        midi_clock_resync(ms, time);           /* (0.14.2) the clock back after a loss */
    if (!midi_clock.tempo_valid || ms - midi_clock.last_ms > 500u) {
        midi_clock.tempo_valid = 1;
        midi_clock.tempo_time = time;
        midi_clock.tempo_n = 0;
        midi_clock.tempo_dt = 0;
        midi_clock.tempo_count = 0;
    } else if (++midi_clock.tempo_n == 6u) {
        uint32_t dt = time - midi_clock.tempo_time;
        midi_clock.tempo_time = time;
        midi_clock.tempo_n = 0;
        /* Allow endpoint jitter around the supported 40..240 BPM range. */
        if (dt >= 60u * MIDI_TICKS_PER_MS && dt <= 377u * MIDI_TICKS_PER_MS) {
            /* Six clocks acquire tempo quickly, but their endpoint jitter must
             * not modulate the LFO or the rate between pulses. Smooth duration
             * over roughly two beats; a change above 6.25% locks immediately.
             * Keep eighth-tick precision even with millisecond host clocks.
             * Musical phase still advances by exactly 1024 units per clock. */
            uint32_t first = !midi_clock.tempo_dt;
            int32_t delta = (int32_t)(dt * 8u) - (int32_t)midi_clock.tempo_dt;
            uint32_t change = (uint32_t)(delta < 0 ? -delta : delta);
            if (first || change > midi_clock.tempo_dt / 16u) {
                midi_clock.tempo_dt = dt * 8u;
                midi_clock.tempo_count = 1;
            } else {
                /* Average acquisition windows before using the steady filter,
                 * so a delayed first pulse cannot bias the initial lock. */
                if (midi_clock.tempo_count < 8u) midi_clock.tempo_count++;
                midi_clock.tempo_dt = (uint32_t)((int32_t)midi_clock.tempo_dt + delta / midi_clock.tempo_count);
            }
            dt = midi_clock.tempo_dt / 8u;
            midi_clock.interval_time = dt / 6u;
            midi_beat_samples = (uint32_t)(((uint64_t)FS * dt) / (250u * MIDI_TICKS_PER_MS));
            /* +/- 0.875 BPM hysteresis keeps fractional master tempos from
             * repeatedly rolling the header digits across a rounding edge. */
            int32_t bpm = (int32_t)(((uint64_t)15000u * MIDI_TICKS_PER_MS * 256u + dt / 2u) / dt);
            int32_t shown = (int32_t)song.g[G_BPM] * 256;
            if (first || bpm > shown + 224 || bpm < shown - 224)
                song.g[G_BPM] = (int16_t)clamp((bpm + 128) / 256, 40, 240);
        }
    }
    if (song.playing) {
        if (midi_clock.have_pulse) {
            uint32_t interval = time - midi_clock.last_time;
            if (!midi_clock.tempo_dt && interval >= 8u * MIDI_TICKS_PER_MS && interval <= 80u * MIDI_TICKS_PER_MS)
                midi_clock.interval_time = interval;
            midi_clock.pos += MIDI_BEAT_UNITS / 24u;
        }
        midi_clock.have_pulse = 1;
    }
    if (!midi_clock.interval_time)
        midi_clock.interval_time = 60000u * MIDI_TICKS_PER_MS / ((uint32_t)song.g[G_BPM] * 24u);
    midi_clock.last_ms = ms;
    midi_clock.last_time = time;
}

static uint32_t midi_clock_advance(uint32_t now)
{
    uint32_t target, elapsed, offset, n;
    if (!midi_clock.have_pulse)
        return 0;
    elapsed = now - midi_clock.last_time;
    /* The render timeline can precede a freshly received pulse. */
    if ((int32_t)elapsed < 0)
        elapsed = 0;
    if (elapsed > midi_clock.interval_time)
        elapsed = midi_clock.interval_time;
    offset = elapsed * (MIDI_BEAT_UNITS / 24u) / midi_clock.interval_time;
    if (offset >= MIDI_BEAT_UNITS / 24u)
        offset = MIDI_BEAT_UNITS / 24u - 1u;
    target = midi_clock.pos + offset;
    n = (int32_t)(target - midi_clock.rendered) > 0 ? target - midi_clock.rendered : 0u;
    midi_clock.rendered += n;
    return n;
}
