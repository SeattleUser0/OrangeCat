# Orange Cat Quick Guide

**Firmware:** 0.4.19
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

After a drive reads, OCT+ opens its files. Turn KNOB 1 (or KNOB 2, or PRESETS) to move. OCT+ opens a folder, or picks a file: a WAV shows its rate, bits, channels and length, and every file shows how fast it read. OCT- goes up a folder, and from the top back to the test. OCT+ on a WAV plays it from the card, over and over, mixed in with whatever else is playing. Up to four play at once: OCT+ on another WAV adds it, OCT+ on one that is playing (its row says PLAY) stops it. They keep playing while you move through folders, and stop when you leave the file list. The bottom lines show how many play and the processor load; then the gaps (each time the sound dropped out) and how fast the card is read against how fast the playing files need it ("gap 0, 512 of 529 KB/s"). The card keeps up with about three CD-quality stereo files, or six mono. A file that would go past that does not start, and the page says "Too much for the card".

PLAY starts and stops the sequencer on this page too.

REC on the file list records to a new WAV in the folder you are in (OC001.WAV, OC002.WAV and so on). KNOB 3 picks what it records: the mix, or one track alone. A track is recorded in stereo with its level, pan and its own effects, but without its reverb and delay sends, and before MASTER, so it plays back as it went into the mix. That is how you freeze a track. Set the track's level before you record it, and record before you mute it.

- REC while the sequencer plays starts recording at once. REC again stops it.
- REC while the sequencer is stopped makes the take wait. PLAY then starts the song and the take together, and stopping the song saves it. REC again before PLAY cancels it.

The new file shows in the list when it is saved. Any WAVs playing stop when you start recording. The bottom lines show the time, any sound lost because the card fell behind, how loud it is, and how fast the card takes the writes. If you pull the card while recording, it keeps what was recorded up to a few seconds before.

OCT+ on a WAV plays it now, by itself, over and over. OCT+ again stops it.

SEQ on a WAV ties it to the song: its row says READY. PLAY starts it from the top with the song. It plays once, then waits for the next PLAY. Stopping the song stops it and takes it back to the start. SEQ or OCT+ on it again stops it.

The bottom lines say how many play and how many are ready.

To freeze a track: KNOB 3 to that track, stop the sequencer, REC, PLAY, and PLAY again at the end. Then mute the track, SEQ on the new file, and PLAY: the take plays in time with the rest. Other files are read once and timed.

The FM-1 does not power its USB port. Use a hub with power on its PD port.

If nothing shows up, the drive may have no power. Try an OTG adapter that takes a charger, or a powered hub. Take a photo of the page each time. The small line at the bottom tells where the test got to.

## Saving and loading

- Projects save as before (SAVE, the autosave). Phrases keep themselves in the pool.
- At power-on, the project you last loaded or saved comes back.
- Projects saved with 0.3.x load as chains 01 to 04 in row 01. Saving again writes the new format.

## Known gaps

- Web editor backups do not yet hold chains, the song, scenes, grooves, or phrases that are not loaded in a bank. Do not rely on a web backup to bring back an Orange Cat project whole.
- The web editor does not show chains or the song yet.
