# Hardware log

Every flash of the FM-1 and every measurement on it, in order: the commit, the image, what was tested, the numbers.
Branch linux-hw. The unit: one FM-1 on USB to the Linux PC (port `Orange Cat FM-1 MIDI 1`, USB 1209:0001).
Tools: `tools/hw/ci_step.py` (the CI build on the PC), `tools/hw/fm1hw.py` (flash, set up tracks, play, read the Audio
numbers), `tools/hw/audio_check.py` (the USB audio: clipping, jumps). See BUILDING.md, "Building on a Linux PC".

**How the numbers are taken** (`fm1hw.py table`): for each case the driver sets the tracks (engine, preset, a four-note
chord on every one of the 64 steps: C3 E3 G3 B3, or for DRUM-X GM 36 38 42 46; the tracks past N hold rests), sets CLK to USB,
sends MIDI Start and 24 clocks a beat at 120 BPM, resets the Audio numbers after 2 s, plays 20 s, reads them, sends Stop and
puts CLK back. **Worst**: the slowest render of a half buffer (us) since the reset. **Budget**: the half buffer's time
(128 frames at 44.1 kHz, 2,902 us). **Late**: renders the DMA overtook (each an audible fault). **Load**: the highest 93 ms
average seen once a second. Firmware before 0.13.6 answers schema 2: Late is taken as the difference over the run.

Two sets. **light**: preset 0 of each engine (MACRO VA BASS, SHAPES CSAW BASS, DRUM-X DRUM-X; MIX = MACRO, SHAPES, DRUM-X,
MACRO). **heavy** (the A/B table): MACRO ADDITIVE PAD (9) x N, SHAPES RING BELL (5) x N, DRUM-X TECHNO RAW (7) x N, MIX = MACRO
ADDITIVE PAD, DRUM-X BREAKS (8), SHAPES BELL (19), MACRO GLASS BELL (19) (close to the session that showed the fault), then one
MACRO MALLET (18) track alone.

## 2026-10-24

### The unit as found (no flash)

- `ORANGE CAT v0.6.2-80b85f` (sonnet-work 80b85fb, the Nightly), identity FM-1_900. Pack: this build's (state 1). Module: SHAPES
  at start 0 (23,784 B, state 7, registered: engine 14 comes from the module). Tracks: MACRO presets 10, 0, 1, 20, no notes on the
  current steps (saved: `~/fm1-hw/music-before-2026-10-24.json` on the PC, `fm1hw.py putback` writes it back).
- Backup checked before any flash: `/home/userzero/OrangeCat/Orange Cat-backup-2026-10-24.json` (not in the repository):
  format felucca-backup, version 1, firmware `ORANGE CAT v0.6.2-80b85f`, objects 0 (27,904 B), 1 (604 B), 2 (27,904 B), 6 (3,716 B)
  non-empty, every CRC good (web/fm1backup.js `readBackup`, run with node).
- 80b85fb rebuilt on the PC: byte-identical to CI run 40132493524 (.fwsc, .pak, both .ocm, manifests).

### Light table, 80b85fb (no late render anywhere)

| N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 912 / 0 | 769 / 0 | 1,015 / 0 | 927 / 0 |
| 2 | 1,323 / 0 | 1,043 / 0 | 1,530 / 0 | 1,286 / 0 |
| 3 | 1,711 / 0 | 1,322 / 0 | 1,923 / 0 | 1,987 / 0 |
| 4 | 2,102 / 0 | 1,588 / 0 | 2,181 / 0 | 2,429 / 0 |

(worst us / late in 20 s; budget 2,902 us.) The USB audio of N = 1..4 (MACRO and DRUM-X) had no clipped sample and no jump; the
user listened to the same eight runs: clean. Live MIDI chords on each track alone and on three and four tracks: clean, no late.

### The user's own playing, watched (80b85fb)

180 s of the Audio numbers every second while the user played tracks 1 to 4 from the keys: worst up to 3,943 us, 4,141 late
renders, load up to 99.6 %. About 155 late a 2 s whenever track 4 held MACRO ADDITIVE PAD (9) or GLASS BELL (19) beside DRUM-X
BREAKS and SHAPES (presets 15 to 23 in turn: PLUCK, FLUTE, BELL, TOM, SNARE); 2 to 7 late a 2 s with lighter presets on track 4. The render is late: the light set
was too light to show it.

### One track, every preset (80b85fb; worst us, 5 s, four-note chord)

MACRO: MALLET 2,849 (92 % load, 924 late), GLASS BELL 2,844 (921 late), FOLD PLUCK 2,004, ORGAN DRAWBAR 1,963, ADDITIVE PAD 1,944,
FM BELL 1,655, SWARM PAD 1,509, HARP 1,357, STRING CHORDS 1,350, the rest 781 to 1,140. SHAPES: RING BELL 2,212, HARMONICS 1,947,
TRIPLE SAW PAD 1,818, BELL 1,707, RESO LPF 1,562, SAW SWARM 1,523, FOF CHOIR 1,512, the rest 699 to 1,161. DRUM-X: 1,004 to 1,111
(all nine kits). Raw: `~/fm1-hw/preset-costs-80b85fb.txt`.

### Heavy table, 80b85fb (the baseline of the A/B)

| N | MACRO (ADDITIVE PAD) | SHAPES (RING BELL) | DRUM-X (TECHNO RAW) | MIX |
|---|---|---|---|---|
| 1 | 1,991 / 0 | 2,350 / 0 | 1,128 / 0 | 1,997 / 0 |
| 2 | 3,089 / 165 | 4,487 / 2,986 | 1,705 / 0 | 2,352 / 0 |
| 3 | 4,187 / 3,586 | 5,322 / 3,439 | 2,102 / 0 | 3,187 / 171 |
| 4 | 5,047 / 3,576 | 5,791 / 3,391 | 2,403 / 0 | 4,751 / 3,591 |

MALLET, one track: 2,865 / 3,607. (worst us / late in 20 s.)

### Flash 1: 80b85fb over itself (the flash path)

- 2026-10-25T03:22:53Z, `tools/hw/fm1hw.py flash` (tools/fm1_install.py: the update entry, the loader, the write, the restart,
  the identity), package `orangecat-0.6.2-80b85fb.fwsc` built on the PC, 610,529 B, SHA-256 2eb86c69...dccc11, image 505,796 B
  (byte-identical to CI's); host tests on the PC: 184 jobs passed.
- 43 s to the unit back on `ORANGE CAT v0.6.2-80b85f`. Pack still "this build's", SHAPES module still at start 0, the project
  as saved (the driver's track edits are not saved by the unit).

### Flash 2: linux-hw 5e9aad8 (the head; firmware = sonnet-work 4e5da16), the A/B baseline

- 2026-10-25T03:23:59Z, package `orangecat-0.6.2-5e9aad8.fwsc` 610,529 B, SHA-256 c55c4fce...502d1ef8, image 505,796 B (15 bytes
  from CI's 4e5da16 package: the version string); host tests of 4e5da16 on the PC: 184 jobs passed. Unit back on
  `ORANGE CAT v0.6.2-5e9aad`, pack "this build's", SHAPES module at start 0. Audio readout schema 8 (budget 2,901 us).

Heavy table (worst us / late in 20 s / load max %):

| N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,898 / 0 / 59 | 2,286 / 0 / 54 | 1,075 / 0 / 30 | 1,891 / 0 / 59 |
| 2 | 2,952 / 164 / 84 | 4,277 / 2,936 / 96 | 1,651 / 0 / 45 | 2,354 / 0 / 63 |
| 3 | 3,941 / 3,602 / 109 | 4,729 / 3,438 / 107 | 2,109 / 0 / 51 | 3,116 / 164 / 81 |
| 4 | 4,930 / 3,595 / 133 | 5,701 / 3,392 / 109 | 2,325 / 0 / 52 | 4,711 / 3,588 / 119 |

MALLET alone: 2,894 / 3,584 / 95. Late stops growing near 3,600 in 20 s (every other half buffer); past that, Worst and Load
are what tell builds apart.
