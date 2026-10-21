# Orange Cat Quick Guide

**Firmware:** 0.4.10
**Date:** October 21, 2026

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
| TRACKER | The phrase as rows: # NOTE LEN VEL FX | 1 row, 2 note, 3 length, 4 velocity | The note's sheet | |
| PHRASE | LEN, DIV, SWING, GATE | | | |
| GROOVE | The track's groove | 1 template, 2 amount | | |
| TEMPO | BPM, swing, quantize | | | |
| CHAIN | Each track's chain, 8 slots at a time | 1 slot, 2 track, 3 phrase, 4 REPS | Go to the slot | Insert, Duplicate, Own copy, Delete slot, Clear chain |
| SONG | The song rows, a column per track | 1 row, 2 track, 3 chain | Edit that chain | Insert row, Delete row, Copy chain, Clear song, Free unused phrases |
| PHRASES | All 64 phrases | 1 phrase, 2 a row of 8 | Put it in the CHAIN slot picked | Copy to new, Clear phrase |

On TRACKER, the keys write notes at the cursor and move it on. EDIT deletes the note on the row. EDIT held undoes. Turn PRESET right to see every track as a lane; ALGORITHM picks the track you edit. Turn PRESET left to go back.

On CHAIN, if you change the phrase in the slot a track is on, the track switches to it when the knob rests.

On NOTES, TRACKER and PHRASE, OCT+ held opens the pattern's sheet: Quantize, Shift right, Shift left, Reverse, Clear pattern, Clear motion.

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

## USB drive test

Menu (hold HOME) > USB DRIVE TEST. OCT+ starts it. The USB port becomes a host, like a computer's port. Plug in a flash drive. The page shows what it finds: the maker and model, the size, how the drive is formatted, and how fast it reads. OCT- ends the test and the port goes back to normal.

While the test runs, USB MIDI, USB audio and the web editor are off.

After a drive reads, OCT+ opens its files. Turn KNOB 1 (or KNOB 2, or PRESETS) to move. OCT+ opens a folder, or picks a file: a WAV shows its rate, bits, channels and length, and every file shows how fast it read. OCT- goes up a folder, and from the top back to the test. Files can be looked at, not loaded yet.

The FM-1 does not power its USB port. Use a hub with power on its PD port.

If nothing shows up, the drive may have no power. Try an OTG adapter that takes a charger, or a powered hub. Take a photo of the page each time. The small line at the bottom tells where the test got to.

## Saving and loading

- Projects save as before (SAVE, the autosave). Phrases keep themselves in the pool.
- At power-on, the project you last loaded or saved comes back.
- Projects saved with 0.3.x load as chains 01 to 04 in row 01. Saving again writes the new format.

## Known gaps

- Web editor backups do not yet hold chains, the song, scenes, grooves, or phrases that are not loaded in a bank. Do not rely on a web backup to bring back an Orange Cat project whole.
- The web editor does not show chains or the song yet.
