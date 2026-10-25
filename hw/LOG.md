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

### Flash 3: A/B candidate (a), OC_PACK_BUILTIN=1 (9de2370 = linux-hw 5e9aad8 + `export OC_PACK_BUILTIN=1` in tools/orangecat_engines.sh, local commit)

- 2026-10-25T04:15:24Z, package `orangecat-0.6.2-9de2370.fwsc`, SHA-256 656ff8e8...537e5daa, image 543,260 B (+37,464 B: the
  tables in the image), RAM unchanged. Host tests on the PC: 183 of 184 jobs passed in a run that shared the 4 cores with another
  suite; the one failure, "CPU live takes" (tests/audio_cpu_take_test.py, wall-clock timing: "Latched punch-out missed its pass"),
  passed 3 of 3 times alone. (0.7.1 and 0.6.2 showed the same in the same conditions, and passed it 3 of 3 alone.)
- Unit on `ORANGE CAT v0.6.2-9de237`; PACK_INFO says none (this build skips the check), the pack stays installed in flash; the SHAPES
  module still registered (OC_MODULE_WINS=1, so SHAPES rows still read the pack through the module: MACRO, MALLET and MIX are the test).

| N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,984 / 0 / 62 | 2,266 / 0 / 54 | 1,113 / 0 / 31 | 1,983 / 0 / 62 |
| 2 | 3,168 / 1,291 / 88 | 3,891 / 2,817 / 95 | 1,712 / 0 / 47 | 2,383 / 0 / 63 |
| 3 | 4,193 / 3,593 / 115 | 5,038 / 3,388 / 107 | 2,166 / 0 / 53 | 3,158 / 164 / 80 |
| 4 | 5,190 / 3,372 / 140 | 5,209 / 3,371 / 109 | 2,435 / 0 / 54 | 4,802 / 3,592 / 121 |

MALLET alone: 2,884 / 3,588 / 94. **Not the cause**: no better than the head (MACRO a few percent slower). The pack's plaintext
window is read through the cache as the image is.

### Flash 4: 0.7.1 (size-audit-gui 4063a29; the branch's head f6394df adds documents only)

- 2026-10-25T04:25:43Z (a downgrade below 0.9.1: approved by the user, who restores the library from the backup if it is damaged;
  no restore was run). Package `orangecat-0.6.2-4063a29.fwsc`, SHA-256 82d2054b...e622c933c, image 512,164 B, byte-identical to CI
  run 39909046086 (built with --date "2026-10-23 06:02:43"). Host tests: 183 of 184 in the shared run, "CPU live takes" 3 of 3 alone.
- Unit on `ORANGE CAT v0.6.2-4063a2` (schema 2; no pack and no module commands; no DRUM-X: its rows are skipped).

| N | MACRO | SHAPES |
|---|---|---|
| 1 | 1,926 / 0 / 62 | 1,237 / 0 / 39 |
| 2 | 3,027 / 175 / 86 | 1,980 / 0 / 64 |
| 3 | 4,090 / 3,588 / 113 | 2,507 / 0 / 71 |
| 4 | 5,058 / 3,618 / 138 | 2,784 / 164 / 71 |

MALLET alone: 2,834 / 3,588 / 92.

### Flash 5: 0.6.2 (main 19271b0)

- 2026-10-25T04:32:31Z (downgrade, as above). Package `orangecat-0.6.2-19271b0.fwsc`, SHA-256 33a4b822...cdb6cef0ee, image 570,944 B,
  byte-identical to CI run 39890561098 (--date "2026-10-23 02:38:10"). Host tests: as 0.7.1.
- Unit on `ORANGE CAT v0.6.2-19271b`.

| N | MACRO | SHAPES |
|---|---|---|
| 1 | 1,958 / 0 / 61 | 1,254 / 0 / 38 |
| 2 | 3,041 / 179 / 86 | 1,992 / 0 / 63 |
| 3 | 4,114 / 3,592 / 113 | 2,533 / 0 / 71 |
| 4 | 5,125 / 3,609 / 139 | 2,849 / 164 / 73 |

MALLET alone: 2,847 / 3,588 / 93.

**What the known-good builds say.** MACRO costs the same on 0.6.2, 0.7.1 and the head (within 4 %): two ADDITIVE PAD tracks are
already late on 0.6.2, and one MALLET track saturates on all three. That is not a regression; 0.6.2 "plays four tracks" with lighter
sounds. SHAPES (RING BELL) costs about 1.8 times as much on the head (N=1 2,286 us against 1,254; N=4 5,701 against 2,849).

### Flash 6: A/B candidate (e), OC_MODULE_WINS=0 (5b6a6c9 = linux-hw 5e9aad8 + `export OC_MODULE_WINS=0`, local commit)

- 2026-10-25T04:58:52Z. Package `orangecat-0.6.2-5b6a6c9.fwsc`, SHA-256 28fb20de...7d79cad6, image 505,764 B. Host tests: 184 passed.
- Back from 0.6.2: the pack still "this build's"; the SHAPES module found (state 6) and not registered, as the switch says (engine
  14 built in); the library answers (UP_LIST slot 0 "TRIO 001"). The boot did not stop at LIBRARY NEEDS WIPE.

| N | MACRO | SHAPES (built in) | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,911 / 0 / 60 | 2,287 / 0 / 54 | 1,082 / 0 / 30 | 1,905 / 0 / 60 |
| 2 | 2,967 / 164 / 84 | 4,052 / 2,746 / 94 | 1,657 / 0 / 46 | 2,406 / 0 / 64 |
| 3 | 3,979 / 3,592 / 110 | 4,969 / 3,360 / 106 | 2,067 / 0 / 52 | 3,144 / 164 / 80 |
| 4 | 4,910 / 3,607 / 134 | 5,431 / 3,308 / 107 | 2,313 / 0 / 53 | 4,712 / 3,604 / 120 |

MALLET alone: 2,925 / 3,588 / 95. **The module is not the cause**: the built-in SHAPES is as slow on the head. (Four voices sound
for one RING BELL track: schema 3 voices_active 4.) The module's code is also not compiled differently in effect: built through
LLVM IR with the app's -O2 route it is byte-identical to the -Os build (the IR keeps optsize).

### Module code through the plaintext window (Flash 2 against Flash 6; the user's step 1)

Flash 2 is the head with OC_MODULE_WINS=1 and the SHAPES module registered (ENGINES: engine 14 source 2, the module at start 0,
flash 0xC8000, read through the plaintext window); Flash 6 is the same firmware with OC_MODULE_WINS=0, the built-in SHAPES (code in
the image, the normal XIP path). Both read Braids' tables from the pack. SHAPES RING BELL, worst us / late in 20 s / load %:

| N | module (code in the window) | built in (code in the image) | difference |
|---|---|---|---|
| 1 | 2,286 / 0 / 54 | 2,287 / 0 / 54 | 0 % |
| 2 | 4,277 / 2,936 / 96 | 4,052 / 2,746 / 94 | +6 % |
| 3 | 4,729 / 3,438 / 107 | 4,969 / 3,360 / 106 | -5 % |
| 4 | 5,701 / 3,392 / 109 | 5,431 / 3,308 / 107 | +5 % |

**Instruction fetch from the window costs nothing measurable** (one track: equal to the microsecond; two to four tracks: within
+-6 %, both ways, the spread of a run that is already late every other block). What is slow is data: with the tables moved into the
image as well (Flash 7, a+e, below) SHAPES drops to 1,188 us at N=1. So: code in the window, fine; tables read per sample from the
window, about 1.9 times the render time.

### Flash 7: A/B candidate (a+e), OC_PACK_BUILTIN=1 and OC_MODULE_WINS=0 (4f5d722 = linux-hw 5e9aad8 + both switches, local commit)

- 2026-10-25T05:31:08Z. Package `orangecat-0.6.2-4f5d722.fwsc`, SHA-256 31ce719c...9849fd14, image 543,212 B. Host tests: 184 passed.
  The configuration of the earlier exp/sonnet-ab-builtin: the built-in SHAPES with Braids' tables in the image. The pack stays in flash, unused.

| N | MACRO | SHAPES (built in, image tables) | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,954 / 0 / 62 | 1,188 / 0 / 36 | 1,125 / 0 / 30 | 1,963 / 0 / 62 |
| 2 | 3,154 / 282 / 88 | 1,887 / 0 / 60 | 1,710 / 0 / 48 | 2,322 / 0 / 64 |
| 3 | 4,160 / 3,608 / 114 | 2,446 / 0 / 67 | 2,173 / 0 / 54 | 3,176 / 211 / 83 |
| 4 | 5,196 / 3,396 / 140 | 2,721 / 164 / 68 | 2,460 / 0 / 55 | 4,857 / 3,610 / 124 |

MALLET alone: 2,905 / 3,580 / 94. **SHAPES is back at 0.6.2's cost** (N=1 1,188 us against 0.6.2's 1,254;
N=4 2,721 / 164 against 2,849 / 164). With (a) alone SHAPES still came from the module and read the pack, so (a) never tested Braids'
tables; with (e) alone the built-in SHAPES read the pack. **The cause of the SHAPES regression: Braids' tables read per sample from the
pack's plaintext window.** MACRO is not changed by it: Plaits' ADDITIVE and MODAL compute their oscillators and read tables per block.
