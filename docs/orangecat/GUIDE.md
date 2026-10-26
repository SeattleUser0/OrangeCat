# Orange Cat Quick Guide

**Firmware:** 0.13.7 (branch linux-hw, not released; measured on the unit); the drum kits: 0.14.2 (branch drum-kits, not on the unit yet)
**Date:** October 26, 2026

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

**Recorded notes** (0.7.1). A step you played in live, or fine-tuned on NOTES, shows its notes in the columns too, in the order NOTES goes through them (earliest first, then lowest). Each has its own velocity and length: KNOB 3 in its column changes that note's pitch, in VEL its velocity (of the note you last picked in a note column); KNOB 4 its length, a step a click (NOTES has quarter and sixteenth steps). EDIT in its column deletes that note only. The bottom line gives the note's length, "1.75" and the like. Every effect in FX works on a recorded step (0.13.0), and applies to all of the step's notes together: CHANCE and CONDITION are decided once for the step each time round, so a played-in chord plays whole or not at all; ACCENT plays each note at full velocity; NUDGE moves every note by the same amount (up to a step either way; a note pushed past the end of the loop stops at its end, it does not wrap round to the start); RATCHET plays each note again at equal parts of the step; FLAM plays each as a quiet grace note just ahead of the full one. EDIT in FX turns the picked effect off, EDIT held takes back the last change. A key on a recorded step replaces the whole step with what you play. A step marked REC holds notes that were played just before it and rounded into it: go to NOTES to reach those.

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

## The DRUM-X engine (0.9.0)

DRUM-X is a drum engine from Jiant FM, in engine slot 9. It is a kit of eight synthesized sounds: KICK, SNARE, CLAP,
HAT CL (closed), HAT OP (open), TOM, RIM and BELL. Each sound has two patches, A and B, and MRPH blends between them.
It is made in the spirit of Sonic Charge's Microtonic.

To play it, turn PRESETS to one of its nine kits (DRUM-X, 909 CLUB, 808 BOOM, LOFI DUST, INDUSTRIAL, ACOUSTIC, DUB ROOTS,
TECHNO RAW, BREAKS), or turn the browser's KNOB 4 (ENGINE) to DRUM-X. The kit is the preset: a saved sound, a saved
project and the PRESETS list keep it (since 0.14.2 a saved sound keeps the whole kit you made of it: below). The keys play the General MIDI drum map with the kick on the first C. A closed hat
cuts the open hat.

| Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
|---|---|---|---|---|
| KIT | MRPH | TUNE | TONE | DECY |
| KIT 2 | NOIS | FM | FOLD | DRV |

- **MRPH** blends patch A to patch B for every sound. **TUNE** moves every sound up or down 12 semitones. **TONE** moves
  the colour. **DECY** lengthens or shortens every sound. At 64 each sets the kit as it was made.
- **NOIS** is the noise in each sound. **FM** adds a short FM strike to each hit (a click, a beater, a metal edge). **FOLD**
  runs each sound through a wavefolder. 0 is off for these.
- **DRV** is a soft clip on the whole kit. Its level is kept.
- The LANES pages set each sound of the kit (0.14.2, below).
- SEQ > STEP on a DRUM-X track is the eight lane grid, as for the DRUM engine. Accents, ratchets and flams of the sequencer
  work on it.

### Tuning and panning the lanes (0.14.2)

Each of the eight sounds has its own level, tuning, length and place left to right, and a few sound settings of Jiant FM's.

1. Go to EDIT and step to **LANES**. Pick the sound: strike its key (the kick is the first C), or pick its lane with the black
   keys on the step grid. The first card names it (BD LV is the kick's level, SD the snare, CP the clap, CH and OH the hats,
   TM the tom, RS the rim, CB the bell).
2. | Page | KNOB 1 | KNOB 2 | KNOB 3 | KNOB 4 |
   |---|---|---|---|---|
   | LANES | LEVEL | TUNE | DECAY | PAN |
   | LANES 2 | LAYER | PUNCH | ATK+ | BEND |
   | LANES 3 | TONE | FDBK | HANDS | DUCK |

   - **TUNE** moves the sound up to 12 semitones either way, in steps of about a fifth of a semitone (the card shows
     semitones: +1.5 st). **DECAY** lengthens or shortens it. **PAN** places it: C is the middle, L50 halfway left, R100 all the
     way right. Turning PAN across the middle stops at C first, so the centre is easy to find. The track's own PAN (MIXER)
     still moves the whole kit after that.
   - **LAYER** adds the sound's second oscillator (a metallic edge up to a clang). **PUNCH** drives the hit for a fatter body.
     **ATK+** fades the hit in (a softer start; 8 is about 35 ms). **BEND** makes each hit glide up or down by up to 12
     semitones over one step.
   - LANES 3 is the whole kit's: **TONE** a band-pass on the hats, the rim and the bell, **FDBK** a growl on every sound,
     **HANDS** more hands on the clap, **DUCK** the kick pushing the other (non-drum) tracks down for a pumping mix.
   - At 0 (LAYER: the kit's own) every setting leaves the kit as it was made.
3. **ACC held + OCT+** on the step grid turns every muted lane back on at once (ACC + a lane key mutes one, as before).
4. To copy a lane's pattern onto another lane: on the grid, pick the lane, hold OCT+ for the sheet and choose **Copy lane**;
   pick the other lane, hold OCT+ and choose **Paste lane**. EDIT held undoes the paste.

### Saving a kit (0.14.2)

SAVE > USER saves the DRUM-X sound with its whole kit: every lane's settings and the kit's. Load it back from SAVE > USER
or the PRESETS browser like any sound. A factory kit you change becomes your own copy when you save it; the factory kits
stay as they are. A project remembers which preset a DRUM-X track plays (a factory kit or your saved one), so **save the kit
to SAVE > USER before you save the project**: changes you did not save as a sound are gone when the project is loaded again.

DRUM-X sounds saved before 0.14.2 kept no kit: they play the DRUM-X kit (the first) with their knobs, as they always did.
Load one, make the kit you want, and save it again. The web editor's Sound tab shows the kit as a table (a row per sound)
and changes it from the computer.

Not in this version: editing a kit's own patches (each sound's A and B), the sends of each sound, Jiant FM's X-MOD and its
step components. Each is fixed at the kit's value or off. The engine takes the slot of the old
PHYS engine: an old PHYS sound that was not a drum now plays as a DRUM-X kit.

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
card needs its own memory back, and the delay may say FX FULL with heavy sounds. CLOUD and STUT need 32 KB more each: with
four MACRO or SHAPES tracks, the delay says FX FULL while STUT runs, and comes back when it stops.

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

## Installing everything from the installer page (0.11.2)

The installer page puts the firmware, the pack and the SHAPES module on the unit in one run. You need the three files of one release: the .fwsc,
the .pak and the .ocm. The page's site has all three; they belong together, so take them from one release.

1. Open the installer page and connect the unit as the page says.
2. Press Install Orange Cat. The boxes above it say what it will install. Firmware, Pack and Module are each a box; clear a box to skip that part.
   Without the pack, MACRO and SHAPES make no sound. Without the module nothing is lost today, because SHAPES is still built in.
3. Wait. One bar and one summary cover all three parts. The unit restarts after the firmware, and the page waits for it to come back.
   Then it asks the unit what it has. A part the unit already has at the right version is skipped, and the summary says so.
4. Do not unplug until the summary says it is done.

If the unit has the stock firmware, or an Orange Cat before 0.8.0, it cannot be asked. The page then installs the firmware only and says to run it again.
Run it again and it does the pack and the module. Your projects and sounds are not touched by any part.

Stop the sequencer first. The pack and the module are refused while it plays.

## Installing a set from the card (0.13.3)

Copy the files of one build into a folder on the card: the .pak, the .ocm and the set file (`*.set.json`) that goes with them. There is one set file for
each module choice: pack alone, with SHAPES, or with the Prophet. The unit holds one of the two modules at a time.

1. Stop the sequencer.
2. Open the card browser and go to the folder. The set file shows the value "Set".
3. Press OCT+ on it. The unit checks the set against its own firmware and every file before it writes anything.
4. If it asks about the old module ("Erases old first. Again"), press OCT+ on the same row to go on.
5. "SET INSTALLED" says it is done.

The card cannot update the firmware. If the unit says "Update firmware first", update it from the installer page, then install the set. "Set: another build"
means the files are from a different build than the firmware on the unit. Format: SET_FORMAT.md.

## The asset pack (0.8.0, changed in 0.9.1)

SHAPES and MACRO read their big tables from a small file kept in the unit's flash. That file is the asset pack. It
comes with each release as orangecat-VERSION.pak. It has to match the firmware.

Without the pack, SHAPES and MACRO make no sound. The screen says PACK MISSING when you pick one or play a key on it.
Everything else works. Your projects and sounds are not touched.

**Back up the unit before you install 0.9.1 or later.** The first power-on moves every user sound into a new place in the
flash ("MOVING PRESETS" with a bar) and then erases the old place. Firmware before 0.9.1 cannot read the new place and sees
empty banks. If power is lost during the bar, the move finishes at the next power-on. A web backup made before 0.9.1
restores with this firmware and the sounds are moved again at the next power-on.

**Do not go back below 0.9.1 with sounds you want to keep.** Going back destroys the library. The old firmware cannot see the
new place, and when it saves a sound it erases part of it. Make a full backup (the web editor's backup) before you go back.
Restore it after you come back to 0.9.1 or later. If you come back without a backup, the sounds the old firmware did not touch
may still be there and the sounds it saved are moved in, but the rest is gone. If the unit says LIBRARY NEEDS WIPE at
power-on, the sounds could not be moved. OCT- (WIPE) then OCT+ wipes the presets. KEEP carries on with the presets locked.
Projects and settings stay.

Since 0.9.1 the pack takes no sounds. No bank is asked about and none is erased.

Install it from the computer:

1. Stop the sequencer.
2. Open the web editor and go to the Settings tab. The Asset pack box is there when the firmware has it.
3. Press Install pack and pick the .pak file. The page checks that it matches this firmware before it sends anything.

Install it from the card:

1. Stop the sequencer.
2. Put the .pak file on the card. Any folder works.
3. Open the clip browser (MIXER, then OCT+ held, Assign clip). The .pak shows with the value Pack.
4. Pick it and press OCT+.

When the unit starts with a project that has a MACRO or SHAPES track and no pack, the screen says PACK MISSING once at power-on.

Erase pack in the web editor takes it out again. Do not unplug during an install. If power is lost, the pack reads as
missing and you install it again. Nothing is ever half installed. If the unit says to restart (the move of the old sounds has
not finished), restart it and try again.

## Engine modules (0.11.0, a proof of concept)

SHAPES can play from a module kept in the unit's flash instead of from the firmware. The module comes with each release as
orangecat-shapes-VERSION-COMMIT.ocm. It has to match the firmware. It takes none of your sounds. The firmware build in the repository
keeps its own SHAPES as well, so if the module is missing, damaged, or switched off after a crash, SHAPES still plays.

The Engines table (web editor, Settings tab) lists every engine number the unit knows: its name, whether it is built in or a module, the module's
version and size, where it sits, its state (OK, refused with the reason, torn, banned) and how many tracks use it. Each row has its buttons: Install
(a module number with no module), Replace, Remove.

Install it from the computer:

1. Stop the sequencer.
2. Open the web editor, Settings tab, Engines. Press Install on the SHAPES row and pick the .ocm file. The page checks that it is for this firmware before it sends anything.
   The row then says OK, Module.
3. Turn the unit off and on. SHAPES now plays from the module.

Install it from the card: put the .ocm on the card, open the clip browser, pick it (value Module) and press OCT+. The screen says MODULE INSTALLED.

Replace puts the new module in the other place and erases the old one only after the new one reads back good, so a power cut leaves the old module or the new one. If no
other place is free, the page (or the card, with a second OCT+) says first that the old module is erased first and a power cut leaves none.

Remove takes it out. SHAPES plays from the firmware if it has its own, otherwise it goes silent. Your projects keep their engine choice either way.

If the unit stops while a track is playing SHAPES from the module, three times in a row, the next power-on leaves the module out and the home screen says MODULE BANNED SHAPES
(a stop within 30 seconds of power-on bans it at once). It stays out until you install it again, remove it, or turn the unit off and on once more.
Do not unplug during an install.

A module whose engine is too heavy for the voice budget, or whose state does not fit the audio memory, is refused when you give it to a track: the screen says ENGINE TOO HEAVY or
NO ROOM and the track keeps its engine and its sound.

(0.13.7) A module of an earlier build is refused after this firmware (the module interface changed once): the Engines row says refused, and SHAPES
plays from the firmware's own until you install the module of this build. A module whose RAM tables do not fit the audio memory is not used at all
and its row says NO ROOM.

**How many tracks of what (measured on the unit, 0.13.7; the Nightly of October 25).** The unit renders a block of 128 samples in under 2.9 ms or the sound breaks up. With
four-note chords on every step: SHAPES fits four tracks with any preset (RING BELL, the heaviest, 2.7 ms on four tracks); DRUM-X fits four tracks
(2.4 ms); MACRO's light presets fit four tracks, but ADDITIVE PAD fits one (two are already over), and MALLET and GLASS BELL (the modal model)
take nearly the whole budget on one track alone. This is the same on 0.6.2. Mix heavy MACRO presets with lighter tracks, or use fewer notes.
Since 0.13.8 MACRO's FM and wave-folder sounds (FM BELL, FM BASS, FOLD PLUCK, FOLD LEAD) cost about a quarter less, so three of them fit where two did; ADDITIVE PAD
two tracks, MALLET and GLASS BELL still one. When a track runs out of voices the oldest note fades out (a clean steal); what breaks up is the render running late.

## The Engines table: Use and Replace (0.12.0)

Each engine the unit can play has a Use button in the Engines table, with a track list beside it. Use puts that engine on that track. The unit may say no: ENGINE TOO HEAVY or NO ROOM
shows in the page, and the track keeps its engine and its sound. The track you were on is selected again afterwards.

Replace takes a module file of a different engine, not only the same one. Pick the file; the page checks it, asks, takes the old module out and puts the new one in its place. The tracks that
used the old engine keep its number and are silent until you give them another engine. Your sounds are kept. Install on an empty row still takes only that engine's own file.

## The Prophet as a module (not yet)

The Prophet is not in this firmware, and it cannot be a module yet. It is 31,148 bytes and a module place holds 24,576. What it would need is in docs/orangecat/ENGINE_LOADING_PLAN.md.
The engine number 19 is ready for a module, so a Prophet in an old project stays silent until one exists. Its sound is kept in the project.

## Saving and loading

- Projects save as before (SAVE, the autosave). Phrases keep themselves in the pool.
- At power-on, the project you last loaded or saved comes back.
- Projects saved with 0.3.x load as chains 01 to 04 in row 01. Saving again writes the new format.

## Known gaps

- Web editor backups do not yet hold chains, the song, scenes, grooves, or phrases that are not loaded in a bank. Do not rely on a web backup to bring back an Orange Cat project whole.
- The web editor does not show chains or the song yet.


## The Prophet module (0.12.1, not yet tried on the unit)

The Prophet can also come as a module (orangecat-prophet-VERSION-COMMIT.ocm, kept in the unit's flash like SHAPES' one) for firmware that has the Prophet engine left out. It takes 8 of the 12 sectors of the module area and SHAPES' module 6, so the two do not fit together: with one installed, installing the other says ENGINE X INSTALLED AT START N: REMOVE IT FIRST. Remove the other from the Engines table (Remove), then install. Install it the same ways as SHAPES: the Engines table, the installer page (it offers SHAPES or the Prophet; the choice is kept until you close the page), or the card browser.

With the module installed, the Prophet appears in the engine list, its 15 edit pages work, your stored Prophet sounds and library presets play, and parameter motion can be recorded on its knobs. Remove it and tracks that use it stay on the Prophet but are silent; install it again and they play. The sequencer must be stopped for any install.
