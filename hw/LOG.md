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

### Flash 8: the read-path probe (fa41841 = linux-hw db62998 + `export OC_SFC_PROBE=1`, local commit)

- 2026-10-25T06:25:06Z, package `orangecat-0.6.2-fa41841.fwsc`, SHA-256 7fbdf7c7...e9f81f50. The image is c606d0d's (the same firmware before a
  documentation merge) but for the version string; c606d0d's host tests: 183 of 184, the one failure the protocol document missing command 109,
  fixed in db62998 (tools/check_editor_protocol.py passes). Unit on `ORANGE CAT v0.6.2-fa4184`; pack and module as before.
- `tools/hw/sfc_probe.py` (editor command 109, read only), 2 KB at each offset, interrupts off:

| offset | path | xor = 0x980F keystream | cold ns/read | warm ns/read |
|---|---|---|---|---|
| 0x93000 (the pack) | decrypting (mode 0) | yes | 45.4 | 27.8 |
| 0x94000 (the pack) | decrypting | yes | 45.4 | 27.8 |
| 0xC8000 (the module area) | decrypting | yes | 45.4 | 27.8 |
| 0xD0000 (store, erased) | decrypting | yes | 45.4 | 27.8 |
| every offset | plaintext window | (the reference) | 45.4-45.5 | 27.8 |
| 0x93000, 0xC8000 | LENC set (modes 1, 2) | reads stay plaintext | 45.4 | 27.8 |

**Findings.** (1) The hardware decrypts any address with the package builder's cipher and key (tools/fm1pkg_make.py sfc, 0x980F, the
offset from 0x4000): a blob can be encrypted for any flash address. The KEY register reads 0 (write only). (2) The plaintext window is cached
exactly as the decrypting path: the same 27.8 ns a warm 16-bit read, 45.4 ns cold. (3) LENC_L/H do not carve an encrypted range out of the
window. (4) So the slowness is not the window: it is the data cache. With MELODEE_CACHE_RAM=1 (CI's build) one 4 KiB data way is left, a
direct-mapped cache. In the pack pb_wav_sine (0x2094756) and pb_ws_moderate_overdrive (0x2096776) are 0x2020 apart: their lines take cache
indices 0x3A-0x4A and 0x3B-0x4B, the same lines, and TRIPLE RING MOD reads them alternately every sample: every read a miss (about 280 ns
a line fill, the extra 250 ns a read measured in the render). In the image (a+e, 0.6.2) they sit at 0x2066E74 and 0x20699E4: indices 0x73-0x83
and 0x4F-0x5F, no overlap. A layout accident, both ways.


### Flash 9: 0.13.7, the hot sets (linux-hw 48f63de)

- 2026-10-25T07:09:58Z, package `orangecat-0.6.2-48f63de.fwsc` 610,529 B, SHA-256 6453d571...e056c7df, image 510,256 B, RAM .data+.bss 93,560 B
  (+288 over the head), arena 91,936 B. Host tests on the PC: all 185 jobs passed (golden hashes unchanged: the host output is byte-identical),
  among them tests/hot_set_test.c (11 checks).
- Unit on `ORANGE CAT v0.6.2-48f63d`, services table version 4. The SHAPES module of the head (start 0, interface 12f6be4f) is refused, as it must be
  (state 2, why 3: another interface): engine 14 is the built-in SHAPES, its hot set (5 tables, 2,580 B) copied into the arena at boot.

**(A) built-in SHAPES with the hot set** (worst us / late in 20 s / load %):

| N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,966 / 0 / 62 | 1,187 / 0 / 36 | 1,090 / 0 / 30 | 1,955 / 0 / 62 |
| 2 | 3,044 / 162 / 87 | 1,884 / 0 / 60 | 1,683 / 0 / 46 | 2,422 / 0 / 65 |
| 3 | 4,110 / 3,588 / 113 | 2,438 / 0 / 67 | 2,085 / 0 / 52 | 3,144 / 165 / 81 |
| 4 | 5,050 / 3,591 / 138 | 2,730 / 164 / 68 | 2,377 / 0 / 53 | 4,693 / 3,607 / 119 |

MALLET alone: 2,877 / 3,588 / 93.

### Module install from the computer: the SHAPES module of 48f63de

- 2026-10-25T07:19:54Z, `orangecat-shapes-0.6.2-48f63de.ocm` (2 variants of 23,944 B) with `tools/hw/oc_install.mjs module` (the installer page's
  web/fm1oc.js installModule, through tools/hw/midi_bridge.py). It went beside the old one: start 1, state 7, version 1.0.0, 23,944 B, registered
  without a restart (the hot set copied at registration). The old module stays at start 0, refused.

**(B) the SHAPES module with the hot set**:

| N | MACRO | SHAPES | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 2,022 / 0 / 63 | 1,209 / 0 / 37 | 1,128 / 0 / 30 | 2,018 / 0 / 63 |
| 2 | 3,071 / 163 / 88 | 1,883 / 0 / 60 | 1,694 / 0 / 46 | 2,433 / 0 / 66 |
| 3 | 4,133 / 3,592 / 113 | 2,431 / 0 / 67 | 2,083 / 0 / 52 | 3,142 / 164 / 81 |
| 4 | 5,054 / 3,618 / 138 | 2,714 / 164 / 68 | 2,362 / 0 / 53 | 4,719 / 3,592 / 119 |

MALLET alone: 2,880 / 3,588 / 93.

- Restart through the boot (the same package installed again, 2026-10-25T07:29:08Z): the module registers at boot through the loader's check of its
  hot set in flash (start 1, state 7); SHAPES RING BELL one track 1,182 us. (Checklist step 12, the update with the module installed: it stays OK.)

### Unit checklist items without the card (on 48f63de)

- **PACK MISSING** (2026-10-25T07:30:36Z): `oc_install.mjs erase-pack`: PACK_INFO state 0 (none). MACRO and SHAPES render nothing (worst 460 and
  482 us: the idle cost), DRUM-X plays (990 us). The PACK MISSING screen shows once at power-on: not seen (no power-on in between); the user's.
- **Pack install from the computer** (07:31:30Z, checklist step 4): `oc_install.mjs pack orangecat-0.6.2-48f63de.pak` (the page's installPack): state 1,
  40,232 B, hash 657ED950, this build's. MACRO ADDITIVE PAD 1,947 us, SHAPES RING BELL 1,178 us; their USB audio: peak 18,991 / 16,852, RMS 4,790 / 3,073,
  no clipped sample, no jump (the hot set's copies made again by the pack check: oc_pack_changed).
- **DRUM-X** (checklist step 5): each of the eight lanes struck alone from MIDI (36 38 39 42 46 45 37 56, kit DRUM-X): every lane sounds (peak 3,974
  to 9,134), no clipped sample, no jump. Lane mutes (0.13.1, ACC held + a lane key) are panel-only: the user's.
- **The presets after the round trip through 0.6.2 and 0.7.1**: BACKUP_LIST (read only) against the backup file of 2026-10-24: settings (object 1),
  the saved project slot (object 2) and the shared preset bank (object 6) are byte-identical (same size and CRC). Object 0, the running music,
  differs: the driver's unsaved track edits. No library damage from the downgrades.

### Flash 10: the module alone (2168f4e = linux-hw 48f63de + `export MELODEE_SHAPES=0`, local commit; checklist step 13)

- 2026-10-25T07:55:38Z, package `orangecat-0.6.2-2168f4e.fwsc`, SHA-256 f0e2ebad...6e0675df, image 487,084 B (SHAPES out of the image). Host tests: all
  passed (1,222 s). MOD_INFO flags 2 (SHAPES not built in); engine 14 from the module (start 1, state 7). SHAPES RING BELL: one track 1,203 / 0, four
  tracks 2,719 / 164: the module alone, as (B).
- **Power cycle** (checklist step 8; the user unplugged USB, switched off and on): the module registers at the cold boot (start 1, state 7), SHAPES RING
  BELL one track 1,195 us, 0 late; its USB audio peak 17,147, RMS 3,256, no clipped sample, no jump.

### Flash 11: back to the sonnet-work head (4e5da16), as asked for the end of the session

- 2026-10-25T15:04:56Z, CI's own package `orangecat-0.6.2-4e5da16.fwsc` (run 40139536138; SHA-256 cd854bd4...eb13c139, byte-identical to the PC's build,
  whose host tests passed: 184 jobs). Unit on `ORANGE CAT v0.6.2-4e5da1`, Audio readout schema 8. Pack: this build's (hash 657ED950, unchanged by
  0.13.7). Module area: start 0, the sonnet-work SHAPES module (23,784 B, interface 12f6be4f): state 7, registered, engine 14 from it; start 1, the
  0.13.7 module (23,944 B): refused (state 2, why 3, another interface), harmless; the next install of a SHAPES module of either build goes where the
  planner says. This firmware has the SHAPES regression (1.9 times 0.6.2 for RING BELL); 0.13.7 (linux-hw 48f63de) is the fix.
- The unit's music: the project as saved (the driver's track edits are not saved by the unit); `~/fm1-hw/music-before-2026-10-24.json` on the PC holds
  the tracks as found, `tools/hw/fm1hw.py putback` writes them back.

## 2026-10-25 (daytime)

### Flash 12: the fix as built on the PC (linux-hw ecf2bed = 97184ee's firmware, 0.13.8)

- 2026-10-25T16:57:09Z, package `orangecat-0.6.2-ecf2bed.fwsc` 610,529 B, SHA-256 2d53317a...; image 506,972 B (0.13.7's 510,256 less the 3,284 B of UI code the
  merged globals had cost; BUILD.md, Thirteenth run, Part 1), RAM .data+.bss 93,528 B. Host tests: all 185 jobs (a first run failed fw_api_test, which still named
  the old static: fixed in the test, and "CPU live takes", the wall-clock test, under four jobs; the second run of the whole suite passed).
- Unit on `ORANGE CAT v0.6.2-ecf2be`. The SHAPES module of 0.13.7 at start 1 registered (state 7; the interface is unchanged by 0.13.8), the head's at start 0
  refused (state 2). Pack: this build's. Tracks as found saved in `~/fm1-hw/music-before-2026-10-25.json`.

### The audio capture (Part 4 of the daytime list)

- **The AUDIO4c's inputs are silent to the PC**: all eight capture channels of `hw:AUDIO4c,0` (48 kHz, S32_LE) are exact digital zero, with a note playing (the FM-1's
  own USB audio had it at peak 14,260 at the same time). Exact zero, not noise, means the analog inputs are not routed to the USB host channels: a routing
  setting of the AUDIO4c (iConfig, from the Mac), not a gain. **For the user**: route analog inputs 1 and 2 to the USB host audio channels 1 and 2 (USB host port 1),
  then gain so a SHAPES note at the FM-1's full volume peaks around -12 dBFS; `tools/hw/capture.py record 3 x.wav --source audio4c` then shows it.
- Until then `tools/hw/capture.py` records the FM-1's own USB audio (`--source usb`, 44.1 kHz, the digital mix before the DAC): every capture today is from it.
- **The AUDIO4c's MIDI out reaches the FM-1** (`AUDIO4c DIN` port, TRS MIDI in): ROUT 0, channels 1 to 4 play tracks 1 to 4 (notes on 5 to 16 do nothing).
- **Calibration** (capture.py calibrate, SHAPES CSAW BASS, C4 held 2 s, on Flash 12): peak -5.5 dBFS, RMS -17.6 dBFS, no clipped run, no gap; the largest second
  difference 0.230 (the saw's own edges): the dropout threshold JUMP2 = 0.70 (3 times, rounded up). Against the host render (build/host/hostsim 14 0, PRESET=1 NOTE=7):
  the fundamental 263.8 Hz on both, spectral score 0.34 dB over 142 bins (60 Hz .. 12 kHz, the sustain 0.4 s after the onset, 1.4 s): the tolerance TOL_DB =
  1.2 dB. The second difference cannot tell a dropout from a saw's edge in a heavier patch: on bright presets the late-render count is the dropout measure, and
  capture.py's dropouts are read on smooth ones.

### MACRO by model, Flash 12 (Part 2 of the daytime list; `fm1hw.py table --set macro`)

One preset a model (fm1hw.py MACRO_MODELS), the four-note chord C3 E3 G3 B3 on every step, N tracks of it, 20 s at 120 BPM. Worst us / late in 20 s; voices: the most
sounding at once (schema 3, sampled once a second); kills: voices the budget took (a clean steal, faded); sheds: the overload cut, **0 everywhere: SAFETY is off on this
unit, as it is by default** (GLO > WEIGHTS 2, docs/AUDIO_PERFORMANCE.md; an untagged settings record reads as OFF), so an overload is heard as late renders.
Units: MC_UNITS of eng_macro.c, of the budget of 18 (GLO BUDGET).

| Model (preset) | units | N=1 | N=2 | N=3 | N=4 |
|---|---|---|---|---|---|
| VA (VA BASS, mono) | 3 | 912 / 0 | 1,323 / 0 | 1,736 / 0 | 2,143 / 0 |
| WAVSHP (FOLD PLUCK) | 3 | 1,903 / 0, 4 v | 2,759 / 32, 6 v | 3,201 / 81 | 3,570 / 81 |
| FM (FM BELL) | 8 | 1,583 / 0, 2 v | 2,476 / 0 | 3,303 / 214 | 4,171 / 3,202 |
| GRAIN (FORMANT VOX, mono) | 4 | 950 / 0 | 1,348 / 0 | 1,719 / 0 | 2,137 / 0 |
| ADDTV (ADDITIVE PAD) | 7 | 1,989 / 0, 2 v | 3,129 / 1,083 | 4,177 / 3,596 | 5,218 / 3,594 |
| CHORD (CHORD MINOR, mono) | 5 | 827 / 0 | 1,154 / 0 | 1,491 / 0 | 1,833 / 0 |
| SWARM (SWARM PAD) | 5 | 1,601 / 0, 3 v | 1,951 / 0 | 2,512 / 0 | 3,006 / 164 |
| NOISE (NOISE WIND, mono) | 4 | 794 / 0 | 1,069 / 0 | 1,367 / 0 | 1,655 / 0 |
| PARTCL (DUST RAIN, cap 1) | 6 | 931 / 0 | 1,353 / 0 | 1,776 / 0 | 2,198 / 0 |
| STRING (PLUCKED STRING, cap 1) | 6 | 976 / 0 | 1,467 / 0 | 1,949 / 0 | 2,406 / 0 |
| MODAL (MALLET) | 6 | **2,833 / 3,596, 3 v** | 3,513 / 3,595 | 4,604 / 3,592 | 5,670 / 3,352 |
| KICK (KICK DEEP, mono) | 4 | 876 / 0 | 1,253 / 0 | 1,616 / 0 | 1,974 / 0 |
| SNARE (mono) | 4 | 932 / 0 | 1,374 / 0 | 1,806 / 0 | 2,213 / 0 |
| HI-HAT (mono) | 5 | 860 / 0 | 1,251 / 0 | 1,615 / 0 | 1,989 / 0 |
| HARP (HARP) | 3 | 1,344 / 0, 4 v | 2,001 / 0 | 2,349 / 0 | 2,689 / 164, 6 v |

Raw: `~/fm1-hw/table-ecf2bed-macro.json`. Ten models fit four tracks; SWARM and HARP touch the budget at four (164 late: one burst at a loop start); WAVSHP from two;
FM from three; ADDTV from two; **MODAL is late on one track** with the three voices its 6 units admit. Per voice (the N=4 runs: worst - 460 us idle - 120 us a track,
over the voices): MODAL about 950 us, ADDTV 860, FM 650, WAVSHP 440, SWARM 410, STRING 370, the others 180 to 320.

### Flash 13: the Nightly as published (sonnet-work ceccc1c), Part 1

- CI: run 40212043712 (sonnet-work ceccc1c, build and tests-gate green, 185 host-test jobs); the publish, run 40212959451 (publish_nightly=true: build, tests-gate
  green, deployed; its last step, the live check, timed out at 10 minutes on the Pages cache with the old site, as on the earlier Nightly runs: the live site had the
  new nightly.json minutes later). **Nightly label**: `sonnet-work · ceccc1c · v0.6.2-ceccc1 · 2026-10-25T16:58:44Z`. The site's `firmware/melodee-nightly.fwsc`,
  `orangecat-nightly.pak` and `orangecat-shapes-nightly.ocm` are byte-identical to the run's artifact (fwsc SHA-256 f9c93ad6..., pak a4e45b19..., ocm b3fb9c91...).
- 2026-10-25T17:30:50Z, the site's `melodee-nightly.fwsc` with `fm1hw.py flash`: unit on `ORANGE CAT v0.6.2-ceccc1`. Then `oc_install.mjs pack orangecat-nightly.pak`
  (state 1, 40,232 B, hash 657ED950) and `oc_install.mjs module orangecat-shapes-nightly.ocm`: the planner put it at start 0 (over the head's refused module) and
  erased the 0.13.7 module at start 1 after: one module in the area, start 0, state 7, version 1.0.0, 23,944 B, engine 14 from it.

Heavy table, the shipped build (worst us / late in 20 s / load max %):

| N | MACRO | SHAPES (module) | DRUM-X | MIX |
|---|---|---|---|---|
| 1 | 1,991 / 0 / 62 | 1,168 / 0 / 36 | 1,096 / 0 / 30 | 1,998 / 0 / 62 |
| 2 | 3,110 / 1,099 / 88 | 1,864 / 0 / 60 | 1,648 / 0 / 46 | 2,304 / 0 / 63 |
| 3 | 4,188 / 3,609 / 114 | 2,391 / 0 / 67 | 2,083 / 0 / 52 | 3,133 / 165 / 80 |
| 4 | 5,169 / 3,620 / 140 | 2,686 / 164 / 68 | 2,369 / 0 / 53 | 4,692 / 3,592 / 118 |

MALLET alone: 2,856 / 3,605 / 93. SHAPES as 0.13.7's and at or under 0.6.2 (1,254 / 0 at one track, 2,849 / 164 at four). The unit stays on this build.
