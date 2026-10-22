# Orange Cat Quick Guide

**Firmware:** 0.5.4
**Date:** October 22, 2026

This guide covers what Orange Cat adds on top of Melodee 1.4.5. Build.md has the full detail and the test lists.

## The three levels

Orange Cat arranges music the way the M8 and LSDJ trackers do.

- **Phrase:** a pattern of up to 64 steps. There are 64 phrases, numbered 01 to 64. All projects share them.
- **Chain:** 16 slots, A to P. Each slot names a phrase and how many times it repeats (REPS). Each project has 64 chains. Each track is on one chain.
- **Song:** 64 rows. Each row names a chain for each track.

A new project puts track 1 on chain 01, track 2 on chain 02, and so on. Row 01 of the song names chains 01 to 04.

## What PLAY plays

It depends on the page you press it on.

| Page | PLAY plays |
|---|---|
| SONG | The song, from the row picked |
| CHAIN | Each track's chain, then loops it |
| NOTES, TRACKER, PHRASE | Each track loops its phrase |
| Any other page | The same as last time |

In the song, each track goes down its own column at its own pace. When a track reaches an empty cell or the last row, it goes back to the top of its block of filled rows. A track whose cell is empty in the starting row stays silent.

## SEQ pages

Tap SEQ to step through them. Hold SEQ to jump to CHAIN.

| Page | What it shows | Knobs | OCT+ | OCT+ held |
|---|---|---|---|---|
| NOTES | The piano roll (Melodee's) | | | |
| TRACKER | The phrase as rows: # N1 N2 N3 N4 VEL FX | 1 row, 2 column, 3 value, 4 length (in FX: amount) | The note's sheet | |
| PHRASE | LEN, DIV, SWING, GATE | | | |
| GROOVE | The track's groove | 1 template, 2 amount | | |
| TEMPO | BPM, swing, quantize | | | |
| CHAIN | Each track's chain, 8 slots at a time | 1 slot, 2 track, 3 phrase, 4 REPS | Go to the slot | Insert, Duplicate, Own copy, Delete slot, Clear chain |
| SONG | The song rows, a column per track | 1 row, 2 track, 3 chain | Edit that chain | Insert row, Delete row, Copy chain, Clear song, Free unused phrases |
| PHRASES | All 64 phrases | 1 phrase, 2 a row of 8 | Put it in the CHAIN slot picked | Copy to new, Clear phrase |

On CHAIN, if you change the phrase in the slot a track is on, the track switches to it when the knob rests.

On NOTES, TRACKER and PHRASE, OCT+ held opens the pattern's sheet: Quantize, Shift right, Shift left, Reverse, Clear pattern, Clear motion.

### TRACKER

Each row is a step. A step holds up to four notes, one in each column N1 to N4, plus its velocity (VEL) and its effects (FX). The cursor's cell has a frame. The column name at the top is lit, and the bottom line says what KNOB 3 and KNOB 4 do there.

- **KNOB 1** moves up and down the rows.
- **KNOB 2** picks the column: N1, N2, N3, N4, VEL, FX.
- **In a note column (N1 to N4):** KNOB 3 changes that note's pitch, one semitone per click. On an empty column, KNOB 3 adds a note (it starts at the note before it). KNOB 4 sets how many steps the step's notes last. A held note shows bars on the rows after it.
- **In VEL:** KNOB 3 sets the velocity. KNOB 4 sets the length.
- **In FX:** KNOB 3 picks the effect: RATCHET, CHANCE, CONDITION, NUDGE, SLIDE, ACCENT. KNOB 4 sets it. The bottom line shows both, for example "K3 CHANCE · K4 50%".
- **Keys** write notes starting in the picked note column, then move down. Hold several keys to write a chord across the columns. A key in N2 replaces only N2's note; the step's other notes stay. In VEL or FX, keys start in N1.
- **EDIT** in a note column deletes that note only. In VEL it deletes the whole step. In FX it turns off the picked effect. EDIT held undoes.

To write a four note chord: KNOB 2 to N1, hold four keys together, let go. Or write one note, then KNOB 2 to N2 and turn KNOB 3 to add the next.

What FX shows: x2 to x4 ratchet, FL flam, a chance like 50%, a condition like 1:2, FIL (fill) or !FL (not fill), a nudge like +3, SL slide, AC accent. A row with several effects shows the first; put the cursor on it in FX and turn KNOB 3 to see each.

A track set to MONO, LEG or UNI still plays one note at a time. The page says so when you add a second note. To hear the chord, press EDIT until the VOICE page shows, then turn KNOB 1 (VCE) to POLY.

Turn PRESET right to see every track as a lane; ALGORITHM picks the track you edit. Turn PRESET left to go back.

## PERFORM (hold ARP)

| Control | Does |
|---|---|
| White keys F3 to F4 | Scenes 1 to 8: tap to recall |
| SAVE held + a white key | Store that scene |
| Black keys 1 to 4 | Mute or unmute tracks 1 to 4, at the launch point |
| D#4 held | FILL |
| KNOB 1 (AT) | The launch point: LOOP, BAR, BEAT, NOW |
| KNOB 2 to 4 | Throw the selected track's SEND 1 to 3; it glides back over a bar |
| OCT- | Put back the mutes and AT as they were when you opened it |

A scene stores each track's chain, slot and mute. AT decides when a scene, a staged mute, or a slot you go to by hand comes in while playing. Double-tap ARP to lock the map open.

## Groove

SEQ > GROOVE gives each track a template: LAID BACK, PUSH, DILLA, 2-STEP, DUB or HUMAN. KNOB 2 sets how much. Swing still works on top. Recording with quantize on the track's own step grid lands in the groove.

## The MACRO engine

MACRO is the new synth engine. It has 15 sound models, each shaped by the same three knobs. Fourteen are ported from the open source code of a well known Eurorack macro oscillator; HARP is Orange Cat's own, built from its string model. The four tracks start on MACRO sounds. SHAPES (below) is the other engine.

To play it:

1. Turn PRESETS to browse its 27 sounds (VA BASS, CHORD MINOR, PLUCKED STRING, HARP, KICK DEEP and so on). KNOB 1 on the browser picks a list (BASS, LEAD, PAD, DRUM and so on).
2. On the HOME page, the four knobs are MODL (the model), HARM, TIMB (timbre) and MRPH (morph).
3. Press EDIT for its pages. Press EDIT again (or turn SELECT) for the second page.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| MODEL | MODL | HARM | TIMB | MRPH |
| SHAPE | AUX | LPG | DECY | TMOD |

- **MODL** picks the sound model (below).
- **HARM, TIMB, MRPH** (harmonics, timbre, morph) shape it. What each does depends on the model.
- **AUX** blends to the model's second output, which is a different sound from the same model (0 is the main one, 127 the other).
- **LPG** adds a low-pass gate: the envelope closes a filter as well as the level, for a plucked, woody sound. 0 is off.
- **DECY** sets the length of a second, simple envelope that each note starts. **TMOD** sets how much it moves TIMB, up or down.

The ADSR, LFO and modulation pages work as for any engine. ENV and LFO to SHAPE move TIMB; ENV and LFO to FILTER move MRPH. Velocity is the model's accent.

| Model | Sound | HARM | TIMB | MRPH |
|---|---|---|---|---|
| VA | Two analog style oscillators | Detune | Square width, then sync | Saw shape |
| WAVSHP | Waveshaper and wavefolder | Waveshaper curve | Fold amount | Asymmetry |
| FM | Two operator FM | Frequency ratio | Mod index | Feedback |
| GRAIN | Formant grains | Formant ratio | Formant frequency | Formant width |
| ADDTV | Additive (sine harmonics) | Number of peaks | Loudest harmonic | Peak shape |
| CHORD | Four note chords | Chord type | Inversion | Waveform |
| SWARM | A cloud of eight sawtooth grains | Pitch spread | Grain density | Grain length |
| NOISE | Filtered noise | Filter type | Clock frequency | Resonance |
| PARTCL | Random clicks through filters | Pitch spread | Density | Filter type and echo |
| STRING | Plucked strings | Material | Brightness | Decay |
| MODAL | Struck bells, bars and plates | Material | Brightness | Decay |
| KICK | Bass drum | Punch and drive | Brightness | Decay |
| SNARE | Snare drum | Tone against noise | Body tone | Decay |
| HI-HAT | Hi-hat | Metal against noise | High-pass cutoff | Decay |
| HARP | Plucked strings, one per note, for chords | Material | Brightness | Decay |

Each track plays up to four notes at once on MACRO when its voice mode is POLY (press EDIT until the VOICE page shows; KNOB 1 is VCE). The pad, keys and bell presets start in POLY; the bass, lead and CHORD presets in LEG (one note; the CHORD model already plays a chord from one key).

Some models cost the processor much more than others, so the FM-1 limits how many of their notes sound at once, across all tracks. If you hold more, the oldest notes stop. Roughly, on one track with nothing else playing:

| Notes at once | Models |
|---|---|
| 4 | VA, WAVSHP, GRAIN, NOISE, KICK, SNARE, HARP |
| 3 | CHORD, SWARM, HI-HAT |
| 2 | ADDTV (ORGAN DRAWBAR, ADDITIVE PAD), MODAL, FM |
| 1 | PARTCL, STRING (always one at a time, see above) |

Other tracks playing at the same time take their share, so fewer notes are left. (Before 0.5.4 an ORGAN DRAWBAR chord of four notes ran the processor past its limit, and the sound broke up.) PARTCL and STRING play one note at a time. STRING has three strings that ring on over each other as you play notes one after another, but notes started together (a chord) give only one. For chords on strings use HARP: each of the four notes gets its own string. Its presets are HARP and STRING CHORDS. CHORD also plays chords from one key. STRING, HARP, MODAL and the drums decay on their own: their presets hold the envelope open, so MRPH sets how long they ring. Raise REL if a sound is cut short.

The models run at the FM-1's 44.1 kHz, so their own decay times are about 9% longer than on the module.

## The SHAPES engine

SHAPES is a second synth engine: a macro oscillator with 43 shapes, from analog style waves to vowels, FM, plucked
and blown models and drums, each shaped by TIMB (timbre) and COLR (color). It is ported from the open source code of
another well known Eurorack macro oscillator, and set up the way the M8's Macrosynth is: a filter, drive, and bit
and sample-rate reduction after the oscillator.

To play it, turn PRESETS to its 24 sounds (CSAW BASS, SYNC LEAD, VOWEL, PLUCK, KICK and so on), or turn the
browser's KNOB 4 (ENGINE) to SHAPES. On HOME the knobs are WAVE, TIMB, COLR and CUT. Press EDIT for its pages.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| SHAPE | WAVE | TIMB | COLR | CUT |
| TONE | RES | DRV | BITS | RATE |

- **WAVE** picks the shape. **TIMB** and **COLR** shape it; what they do depends on the shape.
- **CUT** and **RES**: a low-pass filter (CUT at full is open). ENV and LFO to FILTER move it.
- **DRV**: soft saturation.
- **BITS**: 16 bits down to 2, for grit. **RATE**: sample-rate reduction, OFF then 1/2 down to 1/24.
- ENV and LFO to SHAPE move TIMB. A note strikes the shape, so the plucked, struck and drum shapes start a new hit.

The shapes: CSAW, MORPH, SAWSQR, FOLD, BUZZ, SQRSUB, SAWSUB, SQRSYN, SAWSYN, SAWx3, SQRx3, TRIx3, SINx3, RING,
SWARM, COMB, TOY, ZLPF, ZPKF, ZBPF, ZHPF, VOSIM, VOWEL, VFOF, HARM, FM, FBFM, WTFM, PLUCK, BOWED, BLOWN, FLUTE, BELL,
DRUM, KICK, CYMBAL, SNARE, NOISE, TWINQ, CLKN, CLOUD, PRTCL, QPSK. The four wavetable shapes are left out for now:
their waves take 33 KB of flash.

Up to four notes at once per track in POLY, as with MACRO. The pad, keys, bell and choir presets start in POLY. COMB, PLUCK, BOWED, BLOWN and FLUTE play one note at a time. Like MACRO, its times (decays) run about 9% longer than on the module.

## Freezing a track

Freezing records a track to the card and plays the recording in its place, so its synth stops using the processor. The FM-1 does not power its USB port, so put the card in a hub with power on its PD port (or a card reader plugged into that hub), and plug the hub into the FM-1.

1. Tap GLO until the title at the top of the screen says MIXER. GLO steps through MIXER, then GLOBAL (where KNOB 3 is CLK) and the other setting pages, then back to MIXER. If you see CLK you are one page past MIXER. Turn ALGORITHM to the track.
2. Turn KNOB 3 right. The track's row says Card while the FM-1 finds the card, then Ready.
3. Press PLAY (from the phrase, chain or song page, as you want it to play). The row says Wait while one whole round of the track plays, then Rec while the next round records, then Save, then Frozen.
4. At the start of the following round the recording takes over from the synth. It loops in time with the sequencer for as long as it runs.

What one round is depends on what PLAY plays: the track's phrase (phrase page), its whole chain with each step's repeats (chain page), or its block of song rows (song page). Start it from the same place each time.

While frozen, LEVEL, MUTE, SOLO and the effect sends still work. Pan and the track's own effects are in the recording. Stopping the sequencer stops it; PLAY starts it again from its start.

Turn KNOB 3 left to unfreeze: the synth comes back. Turned left while it is freezing, it cancels and deletes the half-made file.

The recordings go in /ORANGECAT on the card: T1_001.WAV for track 1's first, and so on. They stay on the card after you unfreeze.

If the row shows a word in red, the freeze stopped: Card? (no card found), Stop (the sequencer stopped mid-recording), Full (the card or the FM-1's player memory is full: two frozen tracks can play while a third freezes), Slow (the card fell behind), Error (the card would not write), Lvl 0 (the track's level is off).

While a track is frozen, the USB port talks to the card, so USB MIDI, USB audio and the web editor to a computer are off. Unfreeze every track to get them back.

Frozen tracks are not saved with the project yet: loading a project unfreezes them.

## Saving and loading

- Projects save as before (SAVE, the autosave). Phrases keep themselves in the pool.
- At power-on, the project you last loaded or saved comes back.
- Projects saved with 0.3.x load as chains 01 to 04 in row 01. Saving again writes the new format.

## Known gaps

- Web editor backups do not yet hold chains, the song, scenes, grooves, or phrases that are not loaded in a bank. Do not rely on a web backup to bring back an Orange Cat project whole.
- The web editor does not show chains or the song yet.
