# Orange Cat Quick Guide

**Firmware:** 0.6.0 (branch fx-jiant, not released)
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
| KNOB 2 to 4 | Throw the selected track's REV, DLY or CHO send; it glides back over a bar |
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
| STRING | Plucked strings | Material (32 is a plain string; higher is wood, then metal) | Brightness | Decay |
| MODAL | Struck bells, bars and plates | Material | Brightness | Decay |
| KICK | Bass drum | Punch and drive | Brightness | Decay |
| SNARE | Snare drum | Tone against noise | Body tone | Decay |
| HI-HAT | Hi-hat | Metal against noise | High-pass cutoff | Decay |
| HARP | Plucked strings, one per note, for chords | Material (32 is a plain string; higher is wood, then metal) | Brightness | Decay |

Each track plays up to four notes at once on MACRO when its voice mode is POLY (press EDIT until the VOICE page shows; KNOB 1 is VCE). The pad, keys and bell presets start in POLY; the bass, lead and CHORD presets in LEG (one note; the CHORD model already plays a chord from one key).

Some models cost the processor much more than others, so the FM-1 limits how many of their notes sound at once, across all tracks. If you hold more, the oldest notes stop. Roughly, on one track with nothing else playing:

| Notes at once | Models |
|---|---|
| 4 | VA, WAVSHP, GRAIN, NOISE, KICK, SNARE, HARP |
| 3 | CHORD, SWARM, HI-HAT |
| 2 | ADDTV (ORGAN DRAWBAR, ADDITIVE PAD), MODAL, FM |
| 1 | PARTCL, STRING (always one at a time, see above) |

Other tracks playing at the same time take their share, so fewer notes are left. When notes have to stop, the lowest note held and the newest ones keep playing. (Before 0.5.4 an ORGAN DRAWBAR chord of four notes ran the processor past its limit, and the sound broke up.) PARTCL and STRING play one note at a time. STRING has three strings that ring on over each other as you play notes one after another, but notes started together (a chord) give only one. For chords on strings use HARP: each of the four notes gets its own string. Its presets are HARP and STRING CHORDS. CHORD also plays chords from one key. STRING, HARP, MODAL and the drums decay on their own: their presets hold the envelope open, so MRPH sets how long they ring. Raise REL if a sound is cut short.

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

## Freezing a phrase

Freezing records one pass of a phrase to the card and plays the recording in place of its notes, so the synth has no notes to play while it runs. The freeze belongs to the phrase: whenever any track plays that phrase, the recording plays. When the track moves on to another phrase, its synth plays that one as usual. You can freeze one phrase and keep writing new phrases for the same track.

The FM-1 does not power its USB port, so put the card in a hub with power on its PD port (or a card reader plugged into that hub), and plug the hub into the FM-1.

1. Stop the sequencer. Go to the phrase you want to freeze on the track (the phrase page, or the chain).
2. Tap GLO until the title at the top of the screen says MIXER. GLO steps through MIXER, then GLOBAL (where KNOB 3 is CLK) and the other setting pages, then back to MIXER. If you see CLK you are one page past MIXER. Turn ALGORITHM to the track.
3. Turn KNOB 3 right. The phrase is saved first if it has changes. The track's row says Card while the FM-1 finds the card, then Ready.
4. Press PLAY. The row says Wait while the phrase plays once, then Rec while the next pass of it records, then Frozen.
5. From the next pass of that phrase, the recording plays and the phrase's notes stay quiet. It loops in time with the sequencer for as long as the phrase plays.

If the track is in a chain, the phrase records the next time the chain comes to it.

**A frozen phrase is locked.** Its notes, steps, PHRASE settings (LEN and the others) and motion cannot be changed, and REC will not record into it. Trying shows UNFREEZE TO EDIT. You can still move around in it and look. On HOME, a track that is on a frozen phrase is greyed out with a blue tint and says FROZEN.

The track's sound settings are not locked, but the recording already has the sound in it, so changing them does nothing to the frozen phrase. While it plays, LEVEL, MUTE, SOLO and the effect sends still work. Pan and the track's own effects are in the recording.

To unfreeze, go to the phrase on the track, then turn KNOB 3 left on the MIXER page. The notes play again at once. Turned left while it is freezing, it cancels and deletes the half-made file.

Frozen phrases are kept when you turn the FM-1 off, as part of the phrases. The FM-1 saves the list the next time the sequencer is stopped.

When a frozen phrase plays its notes instead of the recording, the row says why: Card? (no card), Tempo (the tempo is not the one it was frozen at; set it back and the recording returns), Full (no free player for it), Slow (the card fell behind), Error (the file is missing or will not read). Two different frozen phrases back to back on one track: the second plays its notes for its first pass while its recording loads.

The recordings go in a folder named after the project: /ORANGECAT/JAM/T1_001.WAV for the first one made on track 1 in a project called JAM, and so on. A project with no name yet uses /ORANGECAT/UNTITLED. They stay on the card after you unfreeze. They are recorded 18 dB down so loud tracks are not clipped, so on a computer they sound quieter than the track did.

**Naming moves them.** Save an unnamed project with a name, and the recordings of its phrases move from UNTITLED into the new name's folder. Rename a project (or save it back to its own slot under a new name) and its folder is renamed with it. If a folder with the new name is already there, each recording is moved in, and one whose name is taken there gets the next free number. Saving a copy under a new name moves nothing (new recordings of the copy go to its folder). If the card is not plugged in when you name the project, the files move the next time it is, with the sequencer stopped. The phrases keep playing from wherever their files are in the meantime. Recordings made before 0.6.1 stay in /ORANGECAT until their project is named or renamed.

**Assign a clip.** You can give a phrase any WAV on the card instead of freezing it. Go to the phrase on the track, open the MIXER page, and hold OCT+ for its sheet. Pick Assign clip. The FM-1 wakes the card (USB MIDI is off while the browser is open) and shows the project's folder. SELECT (or KNOB 1 or 2) moves, OCT+ opens a folder (".." goes up) or picks the WAV, OCT- goes up a folder or closes the browser. If a clip is refused, a red word says why; turn a knob or press OCT+ or OCT- to go back to the list. An empty phrase works too: it gets a number of its own. The phrase is then frozen with that clip, at the current tempo and the track's level now. Unfreeze it with KNOB 3 left as usual. The clip stays where it is on the card. It must be 16-bit stereo 44.1 kHz, with a name of 13 characters or fewer (like LOOP.WAV), in a folder (not the top of the card). A clip longer than the phrase is cut at the end of each pass; a shorter one leaves silence until the next pass.

If the row shows a word in red, the freeze did not finish: Card? (no card found), Stop (the sequencer stopped mid-recording), Save? (the phrase has no number yet, or changes that are not saved: stop the sequencer and try again), Full (the card or the FM-1's player memory is full), Slow (the card fell behind), Error (the card would not write), Lvl 0 (the track's level is off).

While a track has a frozen phrase in its banks, the USB port talks to the card, so USB MIDI, USB audio and the web editor to a computer are off.

Freezes made before 0.5.6 belonged to the track and were not saved. Freeze those phrases again.

## Effects (0.6.0)

Orange Cat's effects now come from Jiant FM. Press FX for the track's FX page; press FX again to step through the
pages. The first three pages belong to the selected track. The rest are shared by all four tracks.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| FX | DIST (drive) | CHO send | DLY send | REV send |
| DIST | DIST (drive) | TYPE: SOFT, HARD, FOLD, CRUSH, RECT, AMP | TONE | SLC: the track goes through the SLICER |
| AUTO PAN | DEPTH | RATE | SYNC: FREE or BEAT | WAVE |
| DELAY | TIME (synced 1/4 to 4BAR, or 5 ms to 2.95 s) | FDBK (past 100% it saturates) | TONE | MIX |
| DELAY 2 | WIDE | PITCH (grain delay) | SPRAY | MOD (wow and flutter) |
| DELAY 3 | TYPE: ANALOG or DIGITAL | PING: ping-pong | | |
| REVERB | TYPE: ROOM, SPRING, SHIMR, RESO, CLOUD | SIZE | DAMP (RESO: FOLW, the track it tunes to) | PRE (SHIMMER and CLOUD: AIR; RESO: PITCH) |
| REVERB 2 | MOD | RATE | FILT | WIDE |
| CHORUS | RATE | DEPTH | WIDE | |
| SLICER | MODE: OFF, GATE, STUT | PAT (1 to 16) | RATE | DEPTH |
| SLICER 2 | FX: the effects' return goes through the SLICER | LEN (pitch steps) | QUANT (CHR or SCL) | |
| SL PITCH 1 to 4 | The pitch steps S1 to S16 (only while the SLICER is on) | | | |
| MASTER | DJF (DJ filter: left darker, right thinner) | CLIP | LEVEL (the leveler, OFF by default) | SPK (speaker EQ: LOWCUT, BASS+) |

CLOUD at SIZE 127 freezes what it holds. The shared pages' OCT+ sheet has Reset effects (all shared settings back to a
new project's).

**FX FULL.** The delay, reverb and chorus take their memory as soon as a track's send to them is up (REV, DLY or CHO
above 0), and give it back two seconds after all those sends are at 0. The SLICER takes its own while STUT runs. The
synths always come first: if a sound needs memory an effect holds, the effect lets go and its page says FX FULL. An
effect that cannot get memory plays dry and says FX FULL. It comes in by itself once there is memory.

What fits: with the card idle (no frozen phrase playing, no freeze running), the delay, a reverb (not CLOUD) and the
chorus fit beside any four sounds, even four MACRO or SHAPES tracks. While frozen phrases play from the card, the
card needs its own memory back, and the delay may say FX FULL with heavy sounds. CLOUD and STUT need 32 KB more each.

**Punch-in effects (hold FX).** Hold FX for the map. Keys you press while FX is held play an effect on the whole mix
while you hold them. They start on the next 1/16 when playing. Double-tap FX to lock the map open.

| Key | Effect |
|---|---|
| F3, G3, A3 | REPEAT 1/8, 1/16, 1/32 (1/8 needs about 81 BPM or faster) |
| B3 | GLITCH |
| C4 | TAPE stop |
| F#3, G#3, A#3, C#4 | Mute track 1 to 4 |
| G#4 | FREEZE |
| C#5 | SHUFFLE |
| D#5 | CRUSH (sweeps in over two bars) |
| F#5 | THROW (the mix into the delay and reverb) |
| OCT- | Everything off |

While GLITCH, TAPE, SHUFFLE or FREEZE is held, KNOB 1 to 4 are its settings (not saved). Otherwise KNOB 4 is FILTER
(a low-pass on the whole mix).

**MIDI.** CC 91 REV send, CC 93 CHO send, CC 94 DLY send, CC 104 DIST, on the track the channel plays. DIST, its
TONE, AUTO PAN DEPTH and the three sends can be recorded as motion.

**Old projects.** Projects from 0.5.6 and before load with their sends moved to the new effect of the same kind (an
old ECHO send to DLY, ROOM or REVERB to REV, CHORUS to CHO). The old per-track effects (PHASER, TAPE and the others)
have no new match and are dropped, so old projects will sound different. The leveler stays off for them.

## The internal sample slot (0.6.0)

The FM-1 keeps one user sample slot in its own flash, as large as one of Felucca's three (81,408 bytes, about 7
seconds of mono). It took 22 sectors from the phrase store, which now has 34. Load it from a computer with
tools/fm1_sample_upload.py (load NAME file.wav ...). Nothing plays it yet: a sample engine comes later.

## Saving and loading

- Projects save as before (SAVE, the autosave). Phrases keep themselves in the pool.
- At power-on, the project you last loaded or saved comes back.
- Projects saved with 0.3.x load as chains 01 to 04 in row 01. Saving again writes the new format.

## Known gaps

- Web editor backups do not yet hold chains, the song, scenes, grooves, or phrases that are not loaded in a bank. Do not rely on a web backup to bring back an Orange Cat project whole.
- The web editor does not show chains or the song yet.
