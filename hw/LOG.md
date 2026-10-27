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

### Flash 14: MACRO's hot set (wt-mhot e53455f, linux-hw df3d4f5's firmware), Part 2

- 2026-10-25T17:41:52Z, package `orangecat-0.6.2-e53455f.fwsc` SHA-256 6d17182c...; image 507,472 B (+500), RAM .data+.bss 93,672 B (+144), arena 92,096 B, of which
  15,316 B is MACRO's hot set (12 tables: pl_lut_pitch_ratio_high and _low, pl_lut_sine, pl_lut_fold and _fold_2, pl_lut_4x_downsampler_fir, pl_chord_waves, the
  five waveshaper curves) beside SHAPES' 2,580. Host tests: 184 of 185, the one failure hot_set_test's assumption that SHAPES takes the first hot slot (MACRO,
  engine 8, takes it here): the test fixed (any slot), run again alone: passed. Golden hashes unchanged. Unit on `ORANGE CAT v0.6.2-e53455`, the 0.13.8 SHAPES
  module registered (the interface is unchanged).

MACRO by model, Flash 12 (no MACRO hot set) -> Flash 14 (the hot set): worst us / late in 20 s:

| Model | N=1 | N=2 | N=3 | N=4 |
|---|---|---|---|---|
| VA | 912 / 0 -> 893 / 0 (-2 %) | 1,323 / 0 -> 1,296 / 0 (-2 %) | 1,736 / 0 -> 1,696 / 0 (-2 %) | 2,143 / 0 -> 2,104 / 0 (-2 %) |
| WAVSHP | 1,903 / 0 -> 1,464 / 0 (-23 %) | 2,759 / 32 -> 2,107 / 0 (-24 %) | 3,201 / 81 -> 2,455 / 0 (-23 %) | 3,570 / 81 -> 2,844 / 39 (-20 %) |
| FM | 1,583 / 0 -> 1,241 / 0 (-22 %) | 2,476 / 0 -> 1,862 / 0 (-25 %) | 3,303 / 214 -> 2,417 / 0 (-27 %) | 4,171 / 3,202 -> 2,972 / 164 (-29 %) |
| GRAIN | 950 / 0 -> 838 / 0 (-12 %) | 1,348 / 0 -> 1,186 / 0 (-12 %) | 1,719 / 0 -> 1,517 / 0 (-12 %) | 2,137 / 0 -> 1,857 / 0 (-13 %) |
| ADDTV | 1,989 / 0 -> 1,775 / 0 (-11 %) | 3,129 / 1,083 -> 2,796 / 164 (-11 %) | 4,177 / 3,596 -> 3,725 / 3,591 (-11 %) | 5,218 / 3,594 -> 4,639 / 3,595 (-11 %) |
| CHORD | 827 / 0 -> 830 / 0 (+0 %) | 1,154 / 0 -> 1,150 / 0 (-0 %) | 1,491 / 0 -> 1,495 / 0 (+0 %) | 1,833 / 0 -> 1,832 / 0 (-0 %) |
| SWARM | 1,601 / 0 -> 1,469 / 0 (-8 %) | 1,951 / 0 -> 1,817 / 0 (-7 %) | 2,512 / 0 -> 2,342 / 0 (-7 %) | 3,006 / 164 -> 2,833 / 165 (-6 %) |
| NOISE | 794 / 0 -> 764 / 0 (-4 %) | 1,069 / 0 -> 1,053 / 0 (-1 %) | 1,367 / 0 -> 1,351 / 0 (-1 %) | 1,655 / 0 -> 1,636 / 0 (-1 %) |
| PARTCL | 931 / 0 -> 907 / 0 (-3 %) | 1,353 / 0 -> 1,308 / 0 (-3 %) | 1,776 / 0 -> 1,715 / 0 (-3 %) | 2,198 / 0 -> 2,116 / 0 (-4 %) |
| STRING | 976 / 0 -> 988 / 0 (+1 %) | 1,467 / 0 -> 1,459 / 0 (-1 %) | 1,949 / 0 -> 1,939 / 0 (-1 %) | 2,406 / 0 -> 2,398 / 0 (-0 %) |
| MODAL | 2,833 / 3,596 -> 2,837 / 3,584 (+0 %) | 3,513 / 3,595 -> 3,522 / 3,592 (+0 %) | 4,604 / 3,592 -> 4,595 / 3,587 (-0 %) | 5,670 / 3,352 -> 5,643 / 3,342 (-0 %) |
| KICK | 876 / 0 -> 822 / 0 (-6 %) | 1,253 / 0 -> 1,153 / 0 (-8 %) | 1,616 / 0 -> 1,498 / 0 (-7 %) | 1,974 / 0 -> 1,835 / 0 (-7 %) |
| SNARE | 932 / 0 -> 918 / 0 (-2 %) | 1,374 / 0 -> 1,333 / 0 (-3 %) | 1,806 / 0 -> 1,739 / 0 (-4 %) | 2,213 / 0 -> 2,151 / 0 (-3 %) |
| HI-HAT | 860 / 0 -> 841 / 0 (-2 %) | 1,251 / 0 -> 1,213 / 0 (-3 %) | 1,615 / 0 -> 1,569 / 0 (-3 %) | 1,989 / 0 -> 1,929 / 0 (-3 %) |
| HARP | 1,344 / 0 -> 1,338 / 0 (-0 %) | 2,001 / 0 -> 1,991 / 0 (-0 %) | 2,349 / 0 -> 2,324 / 0 (-1 %) | 2,689 / 164 -> 2,680 / 164 (-0 %) |

**The numbers move, so the declaration stays** (linux-hw df3d4f5): FM 22 to 29 % faster (four FM BELL tracks: 3,202 late -> 164), WAVSHP 20 to 24 % (two and
three tracks now clean), GRAIN 12 %, ADDTV 11 % (two ADDITIVE PAD tracks: 1,083 late -> 164), SWARM and KICK 7 %. MODAL, HARP, CHORD and STRING do not move: their
cost is arithmetic, not table reads. MODAL is still late on one track, and ADDTV from three: the voice budget (MC_UNITS) is the remaining lever (BUILD.md,
Thirteenth run, Part 2). Raw: `~/fm1-hw/table-e53455f-macro.json`.

### What the overload sounds like (Part 2; capture.py on the FM-1's USB audio, Flash 14)

8 s each, the four-note chord on every step, SAFETY off (the unit's setting, the default):

| Case | Late | Worst | Budget steals | Peak / RMS dBFS | Clipped runs | Dropouts |
|---|---|---|---|---|---|---|
| overload: MACRO MALLET, one track (3 voices admitted) | 1,455 | 2,844 | 0 | -4.0 / -17.7 | 0 | **200 holes** of 880 to 1,057 samples (20 to 24 ms), about 25 a second |
| the budget steals: FOLD PLUCK, two tracks | 0 | 2,125 | 130 | -3.1 / -21.9 | 0 | 0 |
| clean: VA BASS, one track | 0 | 899 | 0 | -7.0 / -17.0 | 0 | 0 |

A steal is clean (the stolen voice fades; nothing in the stream). An overload is not a clip: on the USB stream it is a hole of about a USB service period each time
the render misses, and at the DAC (not recordable until the AUDIO4c is routed) the half buffer plays again, which is the screech the user heard. With SAFETY on
the shedder would cut a voice after two overloaded halves; it is off by default (upstream's choice, docs/AUDIO_PERFORMANCE.md), so admission (MC_UNITS) has to
keep the render inside the budget. Files: `~/fm1-hw/overload-*.wav`.

### Side task: the Nightly republished, Flash 15

- Workflow rev 18 (the live check reads the live Nightly; BUILD.md, Thirteenth run, "Side task"). **Run 40222235763**, sonnet-work 89b4e79, publish_nightly=true:
  build, tests-gate, pages all green, the check after the deploy too. Nightly label `sonnet-work · 89b4e79 · v0.6.2-89b4e7 · 2026-10-25T18:56:47Z`. Its pack and
  SHAPES module are byte-identical to ceccc1c's (the firmware differs by the version string only).
- 2026-10-25T18:57:50Z, the site's `melodee-nightly.fwsc` (SHA-256 ea558a69...): unit on `ORANGE CAT v0.6.2-89b4e7`; pack this build's, the SHAPES module at start 0
  registered.

### Part 5, the pack (on Flash 15; `~/fm1-hw/p5/st.py`: PACK_INFO, MOD_INFO, command 125, and one SHAPES and one MACRO note through capture.py)

| Step | Pack after (PACK_INFO) | SHAPES note | MACRO note | Result |
|---|---|---|---|---|
| before | 1, this build's, 657ED950 | -5.4 / -19.1 dBFS | -6.3 / -18.9 | |
| `oc_install.mjs erase-pack` | 0, none | silent (digital 0) | silent | pass |
| `oc_install.mjs pack orangecat-nightly.pak` | 1, 657ED950 | -5.5 / -19.1 | -6.3 / -18.9 | pass: sound back |
| a pack of another build (`orangecat-OTHER-build.pak`: a throwaway build with one pl_chord_waves sample + 1; its own hash, good CRCs) | 1, 657ED950, untouched | sounds | sounds | pass: the page refused it, **packWrongBuild**, before any command; nothing erased |
| the Nightly's pack with one payload byte flipped (offset 20,000) | 1, untouched | | | pass: the page refused it, **packDamaged**, before any command |
| the same file with the page's check bypassed (`oc_install.mjs pack-unchecked`: installPack's steps without packProblem) | **0, none** | silent | silent | the firmware refused it at PACK_END (rc 1, after 158 requests: the data CRC), never committed the header: no damaged pack, nothing plays wrong. **But the old pack is gone**: PACK_BEGIN erases the 10 sectors before the data comes (the protocol has no other order on USB), so the firmware's own check cannot keep it. The page's check is what keeps a pack intact; then reinstalled (state 1, sound back) |

### Part 5, the modules (Flashes 15 to 17; `oc_install.mjs`, state from MOD_INFO and command 125)

| Step | State after (start: state, engine) | Result |
|---|---|---|
| SHAPES module installed again (it was at start 0) | start 1: 7, engine 14 registered; start 0 erased after | pass: **replace beside** |
| software reset (Flash 16: the same Nightly installed again, the flash tool's restart; 19:00:25Z) | start 1: 7, engine 14 from the module (125: source 2) | pass: registered at boot; its SHAPES CSAW BASS note against the host render of the built-in engine: 0.34 dB (tolerance 1.2), 263.8 Hz on both |
| install cut at 60 % (`kill -9` of oc_install.mjs and its bridge) | start 0 (old) still 7 and playing; MOD_INFO flags 7 (an install under way) | pass: the old module in charge |
| software reset (19:01:49Z) | start 0: 7, registered, plays; start 1: 0 (empty) | pass, as designed: the header page is written last, so a cut install leaves no magic and reads EMPTY (oc_module.c: MD_TORN is a half-written header); the cut sectors are erased by the next install there |
| `remove 14` | every start 0; engine 14 the built-in SHAPES (source 1) | pass |
| the Prophet module installed | start 0: 7, engine 19, 31,884 B | pass; INIT PROPHET, It's a Prophet 5 and Formant Seraphim play (a four-note chord: peaks -5.3 to -9.1 dBFS, no dropout, no clipped run); 75 presets offered |
| SHAPES module with the Prophet there | refused before anything is erased: **modBlocked 19:0**, the page's text "ENGINE PROPHET INSTALLED AT START 0: REMOVE IT FIRST" | pass |
| a project with engine 19 on track 1 saved (PROJECT save, slot 4, empty before), track 1 changed to MACRO, slot 4 loaded | track 1 engine 19 preset 5 again | pass. **Slot 4 now holds this test project** (the user's to delete) |
| the Prophet installed again | in place, with the page's prompt ("no other start is free, the old module is erased first"; MOD_INFO flags 11) | pass: start 0: 7. A SHAPES module can never be in place (its two starts are never both its own); a Prophet replacement always is |
| `remove 19`, the SHAPES module installed | engine 19 not available (the track keeps the number, silent); SHAPES at start 0: 7 | pass |

**Module alone** (Flash 16b: linux-hw 06d67c6's firmware with `MELODEE_SHAPES=0`, wt-ns2 1622aed, image 483,788 B, host tests passed; 19:04:45Z, `ORANGE CAT
v0.6.2-1622ae`): MOD_INFO flags 2 (SHAPES not built in), engine 14 from the module (the Nightly's SHAPES module installed again: beside, start 1). Its note: 0.34 dB
from the host render. RING BELL, worst us / late in 20 s / load %: 1 track 1,184 / 0 / 37; 2: 1,891 / 0 / 61; 3: 2,425 / 0 / 68; 4: 2,724 / 164 / 69 (as (B) of
Flash 9). **Flash 17**: the Nightly back (19:07:59Z, `v0.6.2-89b4e7`).

**The crash ban** (last, everything before it passed). A throwaway module (branch hw-crash, never pushed: SHAPES whose render calls a null function pointer, a
fetch at address 0, outside the PC guard's window), `~/fm1-hw/orangecat-shapes-CRASH-test.ocm`, installed beside (start 0, registered, the good module erased
after). Three rounds (`~/fm1-hw/p5/crash.py`): 35 s after the boot (past the boot guard's 30 s, so the three-session rule is what is tested, not the first-death
rule), SHAPES on track 1, one note: the unit stopped answering at once, rebooted by itself (the firmware's fault path, a software reset) and was back on USB in
15 s, the module registered again. Before the fourth round command 125 said: start 0 **state 5, MD_CRASHED (MODULE BANNED)**, engine 14 the built-in SHAPES
(source 1). Pass: the unclean-session mark saw the software resets; no power switch needed. (The screen's "MODULE BANNED SHAPES" notice is not readable over
USB on this build.) Then `remove 14` (rc 0, the ban cleared with the start) and the Nightly's SHAPES module installed: start 0, state 7, registered; pack this
build's; SHAPES and MACRO sound (-5.5 and -6.3 dBFS peak). The unit is clean.

### Part 6, the library (on the Nightly, after Part 5; `fm1hw.py library`)

- **BACKUP_LIST (read only) against the backup file of 2026-10-24** (`Orange Cat-backup-2026-10-24.json`): **object 6, the shared preset bank (bank 0, slots 1 to
  16): same size and CRC (3,716 B, 41B5DCD7)**; object 2 (project slot 1): same (27,904 B, D954DF93). Object 0 (the running music) differs, as it must. Object 5
  (project slot 4) is today's test project (Part 5). **Object 1 (the settings) differed by one byte**: offset 562, `favorites.factory[15][30]`, 0 -> 0xC3. That byte is
  **two settings at once** (a bug, below): project.c's PROJ_LAST (0xC0 + the project last loaded or saved; Part 5 saved and loaded slot 4: 0xC3) and ui.c's
  PREF_BITS (bit 1 KNOB ACCEL OFF, bit 5 PICKER OFF). Then project slot 1 (the user's) loaded once, to clear bit 1 again (0xC0: knob acceleration on, picker on;
  power-on now loads slot 1; before today the byte was 0, "none", which only a restore could write back), and the tracks as found put back (`putback
  music-before-2026-10-24.json`: MACRO 10, 0, 1, 20).
- **The shared library** (command 80, list): 384 slots, 1 used before (slot 0 "TRIO 001", engine 6). **Store and read back**: the selected track's sound (MACRO preset
  10) stored to slot 383 as "HW TEST" (op 3: rc 0); read back (op 1): used, engine 8, "HW TEST". The list then has 2. Slot 383 is the user's to delete.
- **The bug** (not fixed today: the fix moves one of them to another byte, a settings layout change): saving or loading project slot 3 or 4 writes 0xC2 / 0xC3 and so
  turns KNOB ACCEL OFF on; slot 2 writes 0xC1 (bit 0, retired: harmless); and turning PICKER OFF (bit 5) makes the byte 0xE0 + the slot, which project_boot reads as
  "none", so the last project is no longer loaded at power-on. Both from Melodee's PREF_BITS (1.3.x) and Orange Cat 0.3.6's PROJ_LAST taking the same byte.

### Fix 1 of 0.11.1 (the library migration after a downgrade and an upgrade): its first real test was yesterday's round trip

Yesterday (Flashes 4, 5, 6) the unit went from the head (library in the 0.9.1 group, migrated long before) down to 0.7.1 and 0.6.2 (both below 0.9.1: the old pair
format, which cannot read the group) and back up to the head (Flash 6, 5b6a6c9 = 4e5da16 + a switch). What Fix 1 is for: an upgrade after such a round trip must
finish the migration (a stale group copy is released; a bank whose pair is newer wins), or say LIBRARY NEEDS WIPE, never stop half way. **What happened**: the boot
after the round trip did not stop at LIBRARY NEEDS WIPE; the library answered (UP_LIST slot 0 "TRIO 001"); BACKUP_LIST then (Flash 9) and today: the shared preset
bank (object 6), the settings and the project slot byte-identical to the backup of October 24 (the settings until today's Part 5, above). **Pass, with what it did
not cover**: no preset was saved on 0.6.2 or 0.7.1, so the case the warning is about (the old firmware's first save writing over group sectors, and the migration
then keeping the newer pair) did not happen; the round trip exercised the read side (the group survives a visit by old firmware that only reads, and the
migration at the next boot finds nothing to move). The write side needs a save on the old firmware, which destroys what it overwrites: a test for a unit
with a backup and the user's go.

### Part 3, MIDI note in (on the Nightly)

- The AUDIO4c's MIDI out (`AUDIO4c DIN`, ALSA client 24 port 0) is wired to the FM-1's TRS MIDI in. ROUT 0: channels 1 to 4 play tracks 1 to 4.
- A C major scale (C4 to C5, 150 ms held, 0.4 s apart, channel 1) into track 1 (MACRO CHORD MINOR), captured on the FM-1's USB audio, voices read over the editor
  (schema 3) while each note was held: **8 onsets, 0.403 to 0.406 s apart; one voice sounding for each note; the intervals exact** (the strongest partial is the
  octave of each note in this chord preset: 522.2, 586.1, 658.1, 697.1, 782.6, 878.2, 985.8, 1,044.4 Hz, 2.00 times C4 .. C5); peak -7.2 dBFS, no dropout, no
  clipped run. **Pass**: MIDI input on the new firmware.

### Part 7, the test surface on the unit (OC_HW_TEST builds: Flashes 18 to 20)

- Flash 18 (19:16:13Z): wt-hwt 47aa740's image built with `--switch OC_HW_TEST=1` (511,304 B; host tests with the switch: all passed), `v0.6.2-47aa74`. Flash 19
  (19:44:51Z): + op 6 (a step's recorded notes) and the page's title, the cursor, the tracker's column and effect in the state, `v0.6.2-4094c6` (host tests passed).
  Flash 20 (20:19:38Z): + the picked effect's value on the step a tracker edit writes and that row, `v0.6.2-ea238a` (511,848 B; host tests passed, eef870d).
- Command 108 answers over USB; **say state** decodes the same from a request on USB and one sent on the AUDIO4c's DIN port into the TRS input (page HOME,
  engine 8, notice code 2262: "SAVED A", a composed notice the decoder names by code only). The panel's map comes back with the state: REC is button 13,
  PLAY 12, KNOB 4 encoder 5.
- Every surface action in the checks below was followed by 200 ms and a state read, and the driver stopped on any state that did not follow
  (`~/fm1-hw/p3panel.py`; logs `~/fm1-hw/p3-panel*.log`, results `p3-panel-*.json`).

### Part 3, recording on the unit, with MIDI in and the test surface

- **REC armed by the REC key** (the surface's press of button 13): track 1 armed (TRACK 27), a second tap disarms. Passed.
- **The 0.7.1 acceptance case**: a chord (C3, D#4, D5) from the AUDIO4c's MIDI out during the loop, released after 1.0, 0.55 and 0.25 s (MACRO ORGAN DRAWBAR, CHRD
  off for the test: the project had CHRD MAJ, VOIC OPEN on track 1, which turns one key into a chord). Read back with op 6: three notes on the step, lengths
  **8.006, 4.396 and 1.979 steps (1.001, 0.550, 0.247 s)**, onsets within 2 ms of each other. **Pass**: three distinct durations, each as played.
- **Step properties on a recorded step**, set from the TRACKER page with the keys and knobs (KNOB 1 the row, KNOB 2 the FX column, KNOB 3 the effect, KNOB 4 one
  detent at a time until the tracker's own value reads the target), a FOLD PLUCK chord (three notes), captured on the USB audio; two placements:

| Property | Chord inside its own step (row 1, onset 0.16) | Chord rounded forward (played at 0.68 of step 0, shown and edited on row 1) |
|---|---|---|
| RATCH x4 | **pass**: 4 hits a pass in 3 passes, 31 to 33 ms apart | **pass**: 4, 4, 4, 30 to 34 ms apart |
| FLAM | **pass**: a grace and the full hit 15 to 16 ms apart, every pass | **pass**: 15 to 17 ms apart |
| NUDGE +16 (31.25 ms) | **pass**: 30.9 to 31.9 ms later | **pass**: 32.9 to 35.0 ms later |
| CHANCE 50 %, 20 passes | **pass**: 8 played, each whole | **pass**: 10 played |
| ACCENT | **pass**: the hits +1.8 dB RMS, centroid 2,545 -> 2,716 Hz (MACRO's velocity is its models' accent) | **pass**: +2.5 dB, 2,279 -> 2,394 Hz |
| two adjacent four-note chords, x4 on both rows (the reviewer's case) | **pass**: 8 onsets every pass in 4 passes (4 + 4), 29 to 34 ms apart; both steps hold their four notes (op 6) | |
| undo by EDIT held (1.2 s) | **pass**: one 10-detent CHANCE change (91 % -> 81 %) undone to 91 %, "UNDO"; ten single detents are ten history entries, one undone a hold | |

- **Retracted**: an earlier run (by hand) heard one hit for RATCH x4 on the rounded chord and was taken for a firmware fault. It was the driver: the value was
  set by a KNOB 4 turn right after an EDIT tap, never confirmed. With every action confirmed both placements pass, and the host agrees (tests/recording_props_test.c,
  rounded() and rounded_live(): a chord rounded forward and one wrapped from the loop's last step, every property set on the row they are shown on, and a chord
  recorded live late in its step: all pass). recording.c already reads every property of a recorded step from the row the overview shows, and the tracker writes
  that row: nothing to fix.
- **Found: an EDIT tap on TRACKER can also open the EDIT pages.** EDIT is a step modifier there: its release deletes the picked effect (ui_input.c, the step-mods
  release, step_delete_edit) and a tap with no step modifier held opens the EDIT family (page_tap). The two read the buttons at different moments of the UI
  frame; when the debounced release lands between them, both happen ("CHANCE OFF" and the page EDIT 1, the second of two taps 0.5 s apart). Not fixed today.

### The Prophet module's hot set (post-list item 1; on the Nightly firmware, Flash 21)

- 2026-10-25T20:33:19Z: the Nightly flashed again (Flash 21); `remove 14`; the Nightly's Prophet module (no hot set, 31,884 B) at start 0; `fm1hw.py table --set prophet`
  (INIT, It's a Prophet 5, Formant Seraphim; four-note chords; N = 1..4). Then the module of linux-hw 5273d8e (P5_TANH, P5_EXP2, P5_TPT_G declared as its hot set,
  6,668 B; 32,104 B; its host test: the set registered, all 201 presets equal the built-in's) installed in place, the same table. The interface is unchanged.

Worst us / late in 20 s, before (no hot set) -> after (the hot set):

| Preset | N=1 | N=2 | N=3 | N=4 |
|---|---|---|---|---|
| INIT | 1,258 / 0 -> 1,334 / 0 | 1,774 / 0 -> 1,884 / 0 | 2,364 / 0 -> 2,498 / 0 | 2,604 / 8 -> 2,679 / 111 (*) |
| P5 | 1,534 / 0 -> 1,658 / 0 | 2,176 / 0 -> 2,364 / 0 | 2,793 / 157 -> 3,093 / 3592 | 3,032 / 293 -> 3,318 / 3585 (*) |
| SERAPHIM | 1,514 / 0 -> 1,566 / 0 | 2,154 / 0 -> 2,222 / 0 | 2,804 / 157 -> 2,853 / -3594 (*) | 3,053 / 255 -> 3,120 / 937 (*) |

(*) the main loop starved under the overload: a stats request went unanswered for over a second, fm1hw fell back to the old readout
(schema 2: budget 2,902, late as a difference), so these late counts are not comparable; CPU1 stayed online with no fault (354,767 jobs).
**Slower with the hot set: +3 to +9 % at one and two tracks** (where every run is clean): the module's three tables do not collide in the D-cache on these
presets, and a module reaches a hot table through the firmware's pointers (OC_FW()->hot[e][i], loads in a chain) where the soft clip, inlined seven
times a sample, had one address. **Taken back** (linux-hw, after 5273d8e): the Prophet module declares no hot set; its engine_t says what it reads and why.
The module on the unit afterwards: the 5273d8e module (with the set) until the next install; see the end state.

### Post-list item 2: engine-candidates merged into linux-hw (bb0d281), the modules at FW_API_VERSION 5

- linux-hw bb0d281 (the merge; conflicts in the workflow notes, build.py's switches, fm1hw.py's sets, three documents: both sides kept). Its build: image
  507,676 B, RAM .data+.bss 93,720 B; every module rebuilt at the new interface: SHAPES 23,944 B, the Prophet 31,884 B, WHEEL 2,920 B, SEM 3,632 B, HADES
  3,308 B (each at two starts). Host tests: all passed (1,061 s).
- **Flash 22** (21:37:51Z): `orangecat-0.6.2-bb0d281.fwsc` (SHA-256 7eeb212c...), `ORANGE CAT v0.6.2-bb0d28`. Its pack installed (hash 657ED950); the old Prophet
  module (5273d8e, the old interface) refused, as it must (state 2), and removed; WHEEL's module: start 1, state 7, engine 7 from it.

WHEEL (module) FULL ORGAN, the four-note chord, worst us / late in 20 s / load %, voices sounding, budget steals:

| N | WHEEL module |
|---|---|
| 1 | 1,110 / 0 / 34, 4 voices |
| 2 | 1,787 / 0 / 54, 8 voices |
| 3 | 2,181 / 0 / 59, 9 voices, 606 steals |
| 4 | 2,488 / 0 / 61, 9 voices, 1,255 steals |

About 130 us a voice; four tracks with no late render; the admission keeps 9 voices sounding past two tracks (clean steals).

### Flash 23: the candidates' measurement build (linux-hw de216f3 + `OC_CANDIDATES=1`), SEM and HADES

- 2026-10-25T22:17:18Z, `orangecat-0.6.2-de216f3.fwsc` (wt-cand; image 507,864 B; host tests with the switch: all passed but two, re-run: the engine gate's
  test quoted SEM's old size from ENGINE_CANDIDATES.md (3,632 B; 3,684 B since TYPE on STAT: the document corrected, the test then passed) and "CPU live
  takes" under two suites at once (passed alone)). `v0.6.2-de216f`. WHEEL removed; SEM (engine 21) at start 1, HADES (22) at start 0, both state 7.

| N | SEM NOTCH PAD | HADES PULSE BAS (MONO) |
|---|---|---|
| 1 | 1,002 / 0 / 32, 4 voices | 739 / 0 / 21, 1 voice |
| 2 | 1,551 / 0 / 50, 8 voices | 1,034 / 0 / 27 |
| 3 | 2,044 / 0 / 56, 9 voices, 1,688 steals | 1,328 / 0 / 34 |
| 4 | 2,306 / 0 / 57, 9 voices, 2,373 steals | 1,618 / 0 / 41 |

### Flash 24: the built-in WHEEL (8c0f675 = linux-hw de216f3 + MELODEE_LEGACY_EXTRAS as a switch, throwaway branch hw-wheel-builtin; DRUM-X off in it)

- 22:22:38Z, `orangecat-0.6.2-8c0f675.fwsc` (image 522,068 B; host tests: as Flash 23's, the same two, the same outcome). Engine 7 built in (125: source 1).

| N | WHEEL built in | WHEEL module (Flash 22) |
|---|---|---|
| 1 | 1,209 / 0 / 34 | 1,110 / 0 / 34 |
| 2 | 2,053 / 0 / 59 | 1,787 / 0 / 54 |
| 3 | 2,525 / 0 / 66 | 2,181 / 0 / 59 |
| 4 | 2,859 / 40 / 68 | 2,488 / 0 / 61 |

**The module is at or under the built-in engine at every N: WHEEL ships as 0.14.0** (the user's rule). The module reads SINE through the firmware's base
hot set (RAM); the built-in engine reads it from the image.

### Flashes 25 and 26: the area cleared, the end state

- 22:25:47Z, Flash 25: de216f3's candidates build again, only to remove SEM (21) and HADES (22) by their numbers (a firmware without OC_CANDIDATES cannot
  name them, and a remove of the whole area is not done over USB): `remove 21`, `remove 22`: every start empty.
- 22:26:49Z, **Flash 26: the Nightly as published** (`melodee-nightly.fwsc` of sonnet-work 89b4e79, `v0.6.2-89b4e7`), its pack (`orangecat-nightly.pak`: state 1,
  657ED950) and its SHAPES module (`orangecat-shapes-nightly.ocm`: start 0, state 7, engine 14 from it); starts 1 and 2 empty: no test module. The tracks as found
  (`putback music-before-2026-10-24.json`: MACRO 10, 0, 1, 20) and track 1's CHRD MAJ and VOIC OPEN as found this morning; transport stopped; CLK put back to
  INT (a timed-out table had left it on USB). Project slot 4 holds the Part 5 test project and library slot 383 "HW TEST"; power-on loads project slot 1
  (PROJ_LAST 0xC0 since Part 6). **The unit stays here.**

## October 26 (UTC): the review's fixes on the Nightly, Flash 27

- **Flash 27: the Nightly as published** (sonnet-work 430cd4c, `v0.6.2-430cd4`, built 02:48:47Z by run 40258557380): `fm1hw.py flash`, the version read back.
  The SHAPES module of 89b4e79 at start 0 read state 2 (refused: interface 5), as CHANGES.md says. Then from the Nightly's own files: the pack
  (`orangecat-nightly.pak`, state 1, 657ED950, the same hash as before), SHAPES (`orangecat-shapes-nightly.ocm`: start 1, state 7) and WHEEL
  (`orangecat-wheel-nightly.ocm`: start 0, state 7). The Prophet's module (8 sectors) does not fit beside SHAPES (6) in the 12 sector area: not installed.
- **MACRO's favorites: one star not set by the user: preset 23**, read with ED_FAV_GET (37). It was bit 7 of favorites.factory[8][2]: the tag the
  unpublished 0.13.8 layout wrote while the day's test builds ran. The October 24 backup (object 1) has row 8 all zero (no MACRO star). Cleared with ED_FAV_SET
  (38: engine 8, preset 23, off); read back: no MACRO star. SHAPES, WHEEL and DRUM-X: none.
- **The effects' memory** (`~/fm1-hw/fxmem.py`, `fxmem2.py`, `fxmem_an.py`; recordings `fxmem*-*.wav`). The Nightly has no test surface and FX FULL is only
  drawn, so by ear on the unit's USB audio: per row, every track plays once with the sends at 0 (the engines' states first), then the card idle, REVERB ROOM:
  - delay: a hit on track 1, the tail 0.5 .. 1.6 s with sends 100/100/100 against DLY 0: MACRO x4 -43.0 / -51.5 dB, SHAPES x4 -43.0 / -50.7, DRUM-X MACRO
    SHAPES MACRO -51.4 / -59.5 (two runs with everything on differ by 0.8 .. 1.5 dB): **holds** in all three;
  - reverb (delay TIME 63, the window between the dry hit's end and the first echo: 17, 138 and 64 ms): everything on against REV 0: -40.9 / -84.6,
    -37.4 / -81.5, -45.4 / -86.3 dB (repeats within 1.4 dB): **holds** in all three (the MACRO row's window is short; the gap is 44 dB);
  - chorus: a held note from MIDI on track 2 (ORGAN DRAWBAR, SUB BASS, ORGAN DRAWBAR), the envelope's wobble (10 ms RMS, detrended, std): everything on
    0.99 and 0.92 dB against CHO 0 0.50 (MACRO), 0.82 and 0.82 against 0.23 (SHAPES), 0.95 and 0.93 against 0.51 (mixed): **holds** in all three.
  No bus played dry with four MACRO, four SHAPES (the largest state) or DRUM-X MACRO SHAPES MACRO: the guide's promise holds on the unit with the hot sets.
- Put back: the tracks (`putback`, steps and levels equal to the snapshot before Flash 27), every track parameter read back equal, REVERB TYPE ROOM, delay
  TIME 1; track 1's CHRD MAJ and VOIC OPEN set again (the power-on after the flash loaded project slot 1, which has them off, as after Flash 26's morning).
  Transport stopped, CLK 0, ROUT 0, BUDGET 18. **The unit stays here.**

## October 26 (UTC), later: the AUDIO4c's analog path calibrated (on the Nightly, Flash 27; no flash)

The user unmuted the AUDIO4c's inputs and set its DIN out to receive from every port. Scripts and recordings: `~/fm1-hw/a4c/` (`a4c_cal.py`,
`a4c_repeat.py`, `synccap.py`, `clicks.py`). `synccap.py` captures both cards on one clock (libasound through ctypes, each read stamped on its return,
sample 0 at the minimum of stamp - frames / rate); the AUDIO4c needs an explicit snd_pcm_start (a read in the prepared state fails with EIO). The tracks
were put back from a snapshot after each script (equal).

- **Silence, 5 s, inputs 1 and 2** (arecord, `hw:AUDIO4c,0`, 48 kHz, S32_LE x 8): input 1 -93.8 dBFS RMS (peak -80.3), input 2 **-77.7 dBFS RMS** (peak
  -63.6): **not digital zero** (1,313 and 7,188 distinct values). Input 2's floor is broadband, 14 dB over input 1's in every band; a 9,984 Hz line on both
  (-94 / -110 dB). The stream starts with about 0.5 s of digital zeros.
- **One SHAPES RING BELL note** (C4, velocity 127, 2 s) sent on `AUDIO4c DIN` (TRS MIDI in, ROUT 0, channel 1: track 1), the FM-1's USB audio and inputs
  1 and 2 captured at once, three notes:

| | USB (FM1) | input 1 | input 2 |
|---|---|---|---|
| peak dBFS | -6.3, -7.1, -7.3 | -29.2, -30.6, -28.3 (an arecord take before: -46.3) | -18.2, -18.6, -19.1 |
| RMS dBFS (the capture) | -31.4 | -56.0, -57.0, -54.2 | -43.2, -43.2, -43.3 |
| clipping, dropouts | 0, 0 | 0, 0 | 0, 0 |
| against USB (peak) | | -21.0 to -23.5 dB | **-11.5 to -11.9 dB** |
| spectrum against USB (RMS dB, 20 to 400 ms after the onset, ~200 bins) | | 4.89 to 6.03 | **1.14 to 1.21** |

  - **Inputs 1 and 2 do not match**: the same signal (correlation 0.992 to 0.993) but input 1 11 to 14 dB under input 2 in these three and 28 dB under in
    the first take, its spectrum 5 to 6 dB off (bins +13 to +17 dB at 486 and 632 Hz). The FM-1's output is mono, mirrored: input 1's cable or gain (the
    user's). capture.py uses **input 2 only**.
  - **No clipping** at the unit's level: input 2 is 11.8 dB under the digital capture, so the unit's digital full scale reaches about -12 dBFS there (at
    the FM-1's volume as it is set). No gain change needed.
  - **Latency**: the analog capture is **5.2 ms ahead** of the USB capture (cross-correlation -5.20, -5.20, -5.22 ms; the onsets alone -4.4 to -5.1),
    **polarity inverted**. From the DIN send: the analog onset 8.9 to 10.0 ms, the USB 14.0 to 14.7 ms.
  - The spectra are compared over the bell's decay: RING BELL falls about 60 dB a second, so capture.py's held-note window (0.4 s after the onset on)
    compares the two noise floors (15 dB apart; a first take scored that).
- **Two earlier results again, both paths at once** (8 s, the chord on every step, 120 BPM):

| Case | worst / late (the unit) | USB holes (capture.py) | analog (input 2) |
|---|---|---|---|
| four SHAPES RING BELL tracks | 2,669 us / 65 (before: 2,686 / 164 in 20 s) | **14** holes of 11 to 12 ms, about 0.62 s apart | 0 by capture.py; clicks at every step (the bell's attacks) |
| MACRO MALLET, one track | 2,850 / 1,530 | **254** holes of 880 to 1,057 samples (20 to 24 ms), about 25 a second, as before (200 in 8 s) | 0 by capture.py; a click in 97 to 142 of the 254 holes |

  - **The counts do not agree, and cannot on this path**: a late render is a hole in the USB stream (the USB service period) but at the DAC one half
    buffer (128 frames, 2.9 ms) played again, a step the converter's filter smooths. capture.py's dropout tests (identical-sample runs, the second
    difference) find none on the analog path. A click detector (the energy above 12 kHz in 1 ms frames, 15 dB over its 201 ms median, falling back within
    3 to 6 ms) marks 97 to 142 of MALLET's 254 holes (by its settings) and 40 to 57 clicks outside them, and on RING BELL it fires on every step (the bell's attack is as sharp
    as a replay). **The analog path is not the default**: capture.py keeps usb for dropouts and offers audio4c (input 2) for level and spectrum and
    for when the FM-1's USB port is busy as a host.
  - Four RING BELL tracks are **not clean on the Nightly**: 65 late renders in 8 s and 14 USB holes (0.13.7's table had 164 late in 20 s at N = 4 as
    well: never clean at four).

## October 26 (UTC): Flash 28, the measurement build (linux-hw 5c61037 + OC_CANDIDATES=1 OC_SFC_PROBE=1)

Built twice with tools/hw/ci_step.py (worktrees `~/fm1-hw/wt-f2`, and `wt-f1` with OC_OCM_VARIANTS=1 too): the firmware (fwsc SHA-256 a75fd86c...) and the
pack (a4e45b19...) byte-identical between the two, the SHAPES package 24,564 B (format 2) against 47,980 B (format 1); interface 8831E67C, the Nightly's. The
local suite passed on 5c61037 first (ci_step.py test, PARALLEL 8, 1,214 s, "CPU live takes" included). 2026-10-26T05:23:47Z: `fm1hw.py flash`: unit on
`ORANGE CAT v0.6.2-5c6103`; pack this build's, WHEEL (start 0) and SHAPES (start 1) registered as before. Scripts and files: `~/fm1-hw/f2/`.

### Format 2 on the unit (`f2_compare.py`, `flashread.py`, `shapes_table.py`)

- `remove 7` (WHEEL holds sector 0, in SHAPES' way), then SHAPES installed four times with oc_install.mjs, each beside the last (the old start erased after):
  format 2 to start 0, format 1 to start 1, format 1 to start 0, format 2 to start 1. After each, the start's 6 sectors read back with editor command 109
  (mode 1: the plaintext window's read, 32 bytes a request, 0.6 s a sector).
- **Identical**: start 0 format 2 = format 1 (SHA-256 39a44038...), start 1 format 2 = format 1 (7bac82b6...): 0 differing bytes of 24,576 at each. Each read
  is the build's variant image (`build/module/shapes.0.img`, `.6.img`, 23,944 B) followed by 632 B erased.
- The four-track table (SHAPES RING BELL, 1 to 4 tracks, 20 s, worst / late / load): the relocated module at start 1: 1,173 / 0 / 36.3, 1,855 / 0 / 59.8,
  2,400 / 0 / 66.8, 2,706 / 115 / 68.0; the variants package's at start 1 (installed twice to land there): 1,172 / 0 / 36.3, 1,854 / 0 / 59.8, 2,418 / 0 /
  66.4, 2,710 / 119 / 68.0. **The same.** So format 1's writer was removed (5f5a4bc). The card's install of a relocated module was not run (no card in the unit).

### op1r's synths (ENGINE_CANDIDATES.md, "op1r": the unit rows)

- `remove 14`; fmbell (start 0, 1,620 B) and dddd (start 1, 4,288 B) installed from their format-2 packages; `table --set op1r --label op1r-a`
  (`op1r-fmbell-dddd.json`); `remove 24`, supersaw (start 0, 1,672 B), `--label op1r-b` (`op1r-supersaw.json`); `measure` per preset (10 s, one track:
  `op1r-measure-a.txt`, `-b.txt`); fmbell back for its presets and a four-track FM BELL table (`op1r-fmbell-poly.json`). Idle (rests only): 424 us.

| Engine (preset) | 1 track | 2 | 3 | 4 | voices | per voice |
|---|---|---|---|---|---|---|
| FMBELL FM BASS (mono) | 539 / 0 | 640 / 0 | 749 / 0 | 840 / 0 | 1, 2, 3, 4 | |
| FMBELL FM BELL | 846 / 0 | 1,237 / 0 | 1,762 / 0 | 1,994 / 0 | 4, 8, 9, 9 | about 100 us (presets 841 to 843 with four, FM BASS 542 with one) |
| SUPERSAW SAW PAD | 946 / 0 | 1,441 / 0 | 2,004 / 0 | 2,323 / 0 | 4, 8, 9, 9 | about 125 us (948, 950 with four; 593, 555 with one) |
| DDDD SCREAM (beside FMBELL) | 1,047 / 0 | 1,348 / 0 | 1,713 / 0 | 2,063 / 0 | 4, 8, 9, 9 | the part: about 620 us over idle on one track, 300 to 365 a further track; every preset 1,012 to 1,065 |
| DDDD SCREAM (beside SUPERSAW) | 1,038 / 0 | 1,357 / 0 | 1,733 / 0 | 2,080 / 0 | 4, 8, 9, 9 | |

  Worst us / late in 20 s. **None late.** Admission: 2 units a voice of 18, so nine voices from three tracks on (voices given up: 1,680 to 2,384 in 20 s).

### MACRO's per-model units (approved; on this build: `macro_listen.py`, one track, the chord on every step, 8 s, both captures kept)

| Preset | old units (the Nightly): worst / late / USB holes | new units: worst / late / USB holes | voices | steals in 8 s |
|---|---|---|---|---|
| FOLD PLUCK (WAVSHP 3 -> 5) | 1,457 / 0 / 0 | 1,265 / 0 / 0 | 4 -> 3 | 309 |
| ADDITIVE PAD (ADDTV 7 -> 9) | 1,779 / 0 / 0 | 1,815 / 0 / 0 | 2 -> 2 | 464 |
| MALLET (MODAL 6 -> 9) | 2,847 / 1,535 / 256 | **2,062 / 0 / 0** | 3 -> 2 | 456 |
| GLASS BELL (MODAL 6 -> 9) | 2,848 / 1,561 / 257 | **2,096 / 0 / 0** | 3 -> 2 | 432 |

  **No stutter at one track** in any of the four (late 0, no hole). **The steal is clean**: every step's chord takes the oldest voices (capacity steals,
  AUDIO_STATS field 60), and nothing in the USB stream shows it: no clipped run, the largest second difference 0.06 to 0.32 (the dropout threshold 0.70;
  FOLD PLUCK's 0.32 against its 0.30 on the old units). Not listened to by a person: the recordings are `~/fm1-hw/f2/new-units-*-usb.wav` and `-a4c.wav`.

### Flash 29: back to the Nightly, the end state

- `remove 24`, `remove 26` on the measurement build (an engine the Nightly cannot register), then 2026-10-26T05:51:35Z the Nightly's `melodee-nightly.fwsc`
  (`~/fm1-hw/nightly-430cd4c`): unit on `ORANGE CAT v0.6.2-430cd4`. The pack reads this build's (the same pack bytes; not rewritten). WHEEL (start 0, state 7,
  2,920 B) and SHAPES (start 1, state 7, 23,944 B) installed again from the Nightly's .ocm files: the area as before Flash 28.
- The tracks equal the snapshot before Flash 28 (MACRO 10 / 0 / 1 / 20, steps, levels, track 1 selected). Track 1's CHRD MAJ and VOIC OPEN set again (command 31,
  ids 81 and 82: the restart loaded project slot 1, which has them off); CLK 0 (it read 1 after the restart), ROUT 0, BUDGET 18; transport stopped.
  **The unit stays here.** A Nightly with today's linux-hw work (MACRO's units, the EDIT fix, format 2 only) is not published.

### The AUDIO4c again, after the user's new Y cable and gain staging (on the Nightly, after Flash 29)

`~/fm1-hw/a4c/`: `silence2.wav`, `cal2.json` (`a4c_cal.py`), `repeat2.json` (`a4c_repeat.py`, both inputs kept), `gaps.py`, `repeats.py`. Tracks put back (equal);
track 1's CHRD MAJ / VOIC OPEN and CLK 0 read back unchanged.

- Silence, 5 s: input 1 **-73.7**, input 2 **-73.6 dBFS RMS** (peaks -60.3, -59.9), not digital zero, the two alike.
- RING BELL, three notes, both paths on one clock: USB peak -4.7 to -5.0 dBFS; inputs 1 and 2 -10.6 to -10.8, **equal** (0.0 to 0.1 dB, correlation 1.000,
  residual -62.1 to -62.7 dB); **5.7 to 6.0 dB under the USB capture**: digital full scale at about -5.8 dBFS, **no clipping**; spectra against USB
  **1.03 to 1.11 dB** (each input); **latency: the analog 4.96, 5.58, 5.49 ms ahead** (onsets: 4.75 to 5.46), inverted.
- The two results again: four RING BELL tracks 2,677 us / **65 late**, USB **14 holes**; MALLET 2,848 / **1,529 late**, USB **254 holes**: as before. Both inputs:
  0 by capture.py's hole tests. What the DAC does with a late half buffer, now readable: in MALLET's overload the analog falls 25 to 55 dB for about 2 ms
  every 5 to 6 ms: a half buffer (2.9 ms) played as silence, or as a dip when the render finished part-way. Not a replay of the old half (a 128-frame window
  equal to the one 256 frames before: 9 in the whole capture at -20 dB, none at -30).
  - `dac_silences` (capture.py now; runs at the noise floor, under -62 dBFS, 1.5 to 4 ms, sound on both sides): MALLET **371 / 375** (inputs 1 / 2), four
    RING BELL tracks **0**, the clean notes **0**. `dac_dips` (20 dB under the neighbours 2.5 ms away): MALLET 1,748, RING BELL 623 (its ring-modulated chords
    dip by themselves), a clean note 1.
  - **The counts do not agree with the USB's, and are not the same event**: 1,529 late renders are 254 USB holes (the USB service period) and 371 full
    silences plus partial dips at the DAC. RING BELL's 65 late renders leave no silence at the DAC at all. capture.py keeps **usb as the default** for the
    dropout counts the earlier results used; audio4c (both inputs again) for level, spectrum and the DAC's own gaps, and while USB is busy as a host.

## October 26 (UTC): the publish, Flash 30, the acceptance on the Nightly

- linux-hw 9c91266 + workflow rev 22 (227da22: the Nightly and the site leave op1r's .ocm out) merged into sonnet-work: **3ca2b56** (no force; the local suite
  passed on a9abaca first). **Run 40281512555** (publish_nightly=true): build, tests-gate, pages all green. The live
  https://seattleuser0.github.io/OrangeCat/firmware/nightly.json, read with curl: version `v0.6.2-3ca2b5`, commit 3ca2b565..., builtAt 2026-10-26T07:01:26Z,
  branchName sonnet-work; ocm prophet, shapes, wheel (no op1r). Files in `~/fm1-hw/nightly-3ca2b56/` (fwsc 88b2caee..., pak a4e45b19..., SHAPES 4c85f12e...: the
  format-2 package, byte-identical to Flash 28's).
- **The backup before the update**: `oc_install.mjs backup` (the page's captureBackup), `Orange Cat-backup-2026-10-26.json` beside the October 24 one (29 objects,
  63,844 B, read back). **Found: project slot 1 (object 2) is empty** (October 24 and Part 6: 27,904 B, D954DF93); object 5 (slot 4, the Part 5 test project)
  and the library bank 28 (slot 383 "HW TEST") as expected. Not restored (no restore without the user); the October 24 backup holds the project byte for byte.
  The window: after Part 6, two OC_CANDIDATES measurement builds ran (Flash 23 and Flash 28; Flash 28's first power-on showed every track as engine 22 preset 2,
  not slot 1's music). [Closed October 26: explained and fixed, see "Flash 31 and project slot 1, the cause": the autosave of measurement tracks into
  slot 1, the slot power-on loads since Part 6; the last write on op1r's engine 24, which normal builds refuse. Fixed for measurement builds in d49f558.
  Slot 1 not restored: the October 24 backup holds it, the restore waits for the user's word.]
- **Flash 30**, 2026-10-26T07:03:02Z: the site's `melodee-nightly.fwsc`: unit on `ORANGE CAT v0.6.2-3ca2b5`. The pack, WHEEL (start 0) and SHAPES (start 1)
  installed again from the published files (state 1, 7, 7).

The acceptance (fm1hw's standing columns: worst us / late / USB holes / DAC silences, 20 s; the captures running):

| Case | 1 track | 2 | 3 | 4 |
|---|---|---|---|---|
| SHAPES RING BELL | 1,193 / 0 / 0 / 0 | 1,885 / 0 / 0 / 0 | 2,436 / 0 / 0 / 0 | **2,753 / 165 / 33 / 0** |
| MACRO MALLET (MODAL 9 units) | 2,080 / 0 / 0 / 0 (722 steals) | 3,392 / 3,603 / 497 / 0 | 4,477 / 3,592 / 479 / 489 | 5,488 / 3,375 / 481 / 0 |

- **RING BELL x4 fails the line (2,710 us)** by 43 us: 2,724 and 2,739 without the captures, 2,750 and 2,754 with them; 8 s runs 2,747 to 2,764 against the
  previous Nightly's (430cd4c) 2,669 to 2,677 on the same 8 s: **+80 us (+3 %) with a byte-identical SHAPES module**. The late rate (68 in 8 s against 65) and
  the USB holes (14 in 8 s) are as before, **no DAC silence**. Nothing between the two firmwares touches SHAPES' render; the image grew (the card
  installer +1,388 B and more, linked before the DSP), so a layout or cache-aliasing shift is the suspect, not measured.
- **Four MALLET tracks fail**: from two tracks the unit is overloaded. Each part keeps one voice whatever the budget (voice.c: a part protects its newest), so
  four MODAL voices at about 950 us each are past the half buffer before anything else; no unit count can prevent it. One track is clean (2 voices, the
  steals clean). Under an overload this deep every half buffer is late and the converter plays continuous sound, so **DAC silences can read 0 (x2, x4)
  while the sound is broken**: read it with late and USB holes.
- **For the user to listen to** (AUDIO4c inputs 1 and 2, 48 kHz, the four-note chord held 3 s, CHRD off for the take): `~/fm1-hw/listen/FOLD_PLUCK-chord-nightly-3ca2b5.wav`
  (peak -10.4 dBFS) and `~/fm1-hw/listen/ADDITIVE_PAD-chord-nightly-3ca2b5.wav` (-10.1 dBFS); no clipping, no DAC silence. Track 1's CHRD MAJ / VOIC OPEN set
  again afterwards (the restart had cleared them), CLK 0.

## October 26 (UTC): the soak, the effects' cost

- **Soak** (`~/fm1-hw/soak/soak.py`, 2 h from 07:21Z, Nightly 3ca2b5; MACRO VA BASS, SHAPES RING BELL, DRUM-X 0, WHEEL 0, the chord on every step, REV / DLY /
  CHO 100 on every track, reverb ROOM; the PC's USB clock at 120 BPM; a 20 s sample every 10 min, both captures). Minutes 1 to 51: worst 3,428 to 3,487 us,
  late 1,895 to 1,935, USB holes 307 to 319, DAC silences 0, analog peak -8.4 to -9.1 dBFS, RMS -20.0 to -20.3 (within 0.3 dB): a steady overload (the
  effects ~730 us over the bare load), no growth. **From minute 61 to the end the unit was silent**: worst 403 to 412 us (idle), USB digital zero, the analog
  at the floor; the unit answered (`ORANGE CAT v0.6.2-3ca2b5`), the tracks and parameters went back equal, **objects 1.. of the backup byte-identical
  before and after**. The stop falls between 08:13Z and 08:22Z. The firmware stops the sequencer when no clock pulse is dispatched for 500 ms (seq.c,
  "cable loss") and clocks alone do not restart it; the PC's clock gaps were not recorded (fm1hw.play records the longest from now on:
  `clock_gap_max_ms`). No USB event in the kernel log. Open: the source of the gap (the PC or the unit), see the user's clock item.
- **Effects' cost** (`~/fm1-hw/fxcost/fxcost.py`; four MACRO VA BASS tracks, the chord on every step, 20 s a case; sends 100 / DIST 100 from every track):
  table in BUILD.md. All put back (tracks, parameters, the globals 35, 36, 37, 49, 72 equal).

## October 26 (UTC): Flash 31 and project slot 1, the cause

- **Flash 31**, 2026-10-26T16:14:15Z: sonnet-work 3ca2b56 (the Nightly's commit) + `OC_SFC_PROBE=1` only, no candidates (worktree `~/fm1-hw/wt-probe`; ci_step
  build and test, the test passed in 1,415 s; fwsc 107d37cf...). Unit on `ORANGE CAT v0.6.2-3ca2b5`, the pack, WHEEL (start 0), SHAPES (start 1) as before.
- **The record store read raw** (`~/fm1-hw/slot1/storedump.py`: command 109, mode 1, all 56 sectors; projects are records there, oc_store.c, not the old
  bank sectors 0xA0000 + slot x 40 KB the first note named). Kept beside the backups: `Orange Cat-store-dump-2026-10-26.bin` (SHA-256 4dada835...) and
  `.txt` (every sector's header). Sectors 22..39 hold the sample slot's data (no header, as designed), 18..21 erased; every record header and payload CRC
  checks: **nothing erased under a record, no header that fails, nothing written over one**.
- Read as the firmware reads it (`~/fm1-hw/slot1/inspect.c`: the dump on tests/cand_slot_test.c's harness; slot 4's CRC 7D3C03D7 equals the backup's
  object 5): slot 1's record (PROJECT id 0, name "GPITLA") has eleven copies, seq 40..61. Current, seq 61: **every track on engine 24** (FMBELL, op1r's
  proposed number); seq 60..57: engines 24 and 26 (DDDD); seq 56: SHAPES preset 5 on all four; seq 43..40: MACRO presets 2 / 0 / 1 / 20. A normal build
  (20 engines) refuses seq 61 (proj_ok: an engine past its table): the slot shows OLD FORMAT, a backup has it empty. **None of the copies is the October
  24 project** (D954DF93): every one is an autosave of what a measurement left on the tracks.
- **The cause**: the autosave (ui_input.c autosave_poll: stopped, 5 s with no key and no USB packet) writes the music back to the slot it came from.
  Between hardware scripts the unit rests, so a measurement's tracks went into slot 1, on normal builds too (seq 56: SHAPES); on the OC_CANDIDATES builds
  (Flash 23: HADES 22 preset 2, which Flash 28's first boot showed; Flash 28: op1r's 24 and 26) that made the record unreadable to every other firmware.
  Reproduced on the host, normal and OC_CANDIDATES=1 (32 and op1r-candidates' 52 entries) on one NOR: tests/cand_slot_test.c.
- **Fix** (linux-hw d49f558): an OC_CANDIDATES build never writes a project or the template with a track on an engine from 20 on (the autosave skips,
  SAVE says TEST ENGINE: NOT SAVED). Shipped builds unchanged. **Open**: the autosave still writes measurement tracks into the user's slot on a normal
  build; until the scripts can hold it off (the lab guard, editor.c, is in lab builds only), AUTOSAVE OFF in Settings for hardware sessions, or the music
  not loaded from a slot. Also seen on the host: loading a slot and resting rewrites it once (49 bytes: name padding 0 to space and one byte), wear only.
- Why slot 1: since Part 6 power-on loads project slot 1 (PROJ_LAST, set by loading it to clear the PREF_BITS byte), so the music on the unit came
  from slot 1 after every flash and restart, and every autosave went there.
- **Status: closed** (cause found, reproduced on the host, fixed for measurement builds: d49f558, tests/cand_slot_test.c). **Not restored**: the
  October 24 backup (`Orange Cat-backup-2026-10-24.json`, object 2, 27,904 B, D954DF93) holds the project; the restore (one object, PUT 2) waits
  for the user's word. Open beside it: a normal build still autosaves measurement tracks into the slot the music came from (AUTOSAVE OFF for
  hardware sessions, or the music not from a slot).

## October 26 (UTC): Flash 32 and the backup's arrangement gap

- **Flash 32**, 2026-10-26T16:27:34Z: web-app 10aebc9 (command 102; CI run 40343148928 green on it, built here with ci_step, the pack byte-identical
  to the Nightly's, a4e45b19...). Unit on `ORANGE CAT v0.6.2-10aebc`; WHEEL (start 0), SHAPES (start 1), the pack as before.
- **The gap, proven** (`~/fm1-hw/arrange/arrange_gap.py`, run2; only slot 3 written, emptied before and after): the music's lists read (102 GET);
  chain 06 set to phrase 01 x3 then phrase 01, song row 10 track 1 to chain 06 (102 COMMIT); PROJECT save to slot 3; loaded back: the chain and the
  row there. Backup object 4 read (27,904 B, 9DA160FB); the slot emptied (PUT, size 0); the music and its lists put back and a new LIST (a later
  restore's state); object 4 restored (rc 0); slot 3 loaded: **chain 06 empty, song row 10 empty** (the 4 bytes written, all gone). Run1, restored
  at once after the read, kept them: the unit's RAM copy of the lists is tagged with the record's hash (pattern_store.c proj_x_tag), so a restore of
  the bytes just read reattaches them; a real restore later does not. Cleanup: slot 3 empty, the music, its lists and tracks equal to before.
- **Fix** (linux-hw df2516a, host only so far): the arrangement objects, ids 64..84 (LIST 1): the slots' project records, the pool's chunks, the
  music's lists; restored straight into the record store as they arrive. tests/backup_test.c `arrangement_archive`: a wiped store restored from
  them gives all 21 objects and every slot's lists byte for byte. On the unit after the web-app merge (it needs 102 to set a chain).
- **Review of D's command 102 with this**: the firmware side holds (COMMIT keeps the banks first without touching the staged lists, ocl_keep_all
  does not use proj_wire_u; the checks and staleness are right). Two changes at the merge: 102 BEGIN must also drop an open backup stream
  (ocs_stream_drop: it already ends a staged restore); the web app's backup side channel (`file.arrangement`, the music's lists written back over
  102 when the pool's CRCs match) is redundant on a firmware with ids 64..84 (object 84 is exact) and never covered the saved slots or the pool:
  used only when the archive lacks the arrangement objects.

## October 26 (UTC): Flash 33, drum-kits merged, its DRUM-X table

- linux-hw ec49816 withdrew the parts' admission (e104e4d) on the owner's word; ffe34ab the CLOCK_STATS protocol row. drum-kits (c3ef697, CI run
  40297727741 green on ada31fd) merged: **46aa3bc** (BUILD.md's two appended sections kept, nothing else in conflict; drum-kits' commands are 0, 128,
  129, no clash with 103 or the backup ids 64..84). The local suite passed on 46aa3bc (ci_step test, 1,448 s); linux-hw fast-forwarded to it.
- **Flash 33**, 2026-10-26T18:01:32Z: unit on `ORANGE CAT v0.6.2-46aa3b`, the pack this build's (a4e45b19..., unchanged), WHEEL and SHAPES as before.
- PipeWire had taken the FM-1's new card after the flash (and the AUDIO4c's inputs): the FM-1 card profile set to off and the AUDIO4c's to
  output:analog-surround-71 (its outputs kept for playback); the captures then open again.
- **DRUM-X TECHNO RAW, 20 s** (`~/fm1-hw/drumkits/kit_table.py`, worst us / late / DAC silences; KIT_PUT for the kits):

| Tracks | factory kit | PAN off the centre and TUNE moved on every lane | linux-hw's last (2026-10-25) |
|---|---|---|---|
| 1 | 1,037 / 0 / 0 | 1,222 / 0 / 0 | 1,096 / 0 |
| 2 | 1,569 / 0 / 0 | 1,765 / 0 / 0 | 1,648 / 0 |
| 3 | 2,040 / 0 / 0 | 2,317 / 0 / 0 | 2,083 / 0 |
| 4 | 2,271 / 0 / 0 | **2,662 / 36 / 0** | 2,369 / 0 |

  One track with every value in use (PUNCH, ATK+, LAYER, BEND, TONE, HANDS, DUCK, DECAY too): 1,216 / 0 / 0. The factory row is within the noise of
  linux-hw's. **The pan and tune cost +12 to +18 %** (drum-kits' estimate: about +9 %), and four such tracks run late (36 in 20 s): over the acceptance
  line (0.6.2: none late). **USB holes are not usable in this session** (129 with no late render at one factory track, 46,109 once: the USB capture,
  since the profile change), so late and DAC silences are the columns here. Tracks and parameters put back.
- Left for the user from drum-kits' list: a kit saved, power off and on, reloaded; an old backup restored on this build (a restore); by ear: L/C/R
  on the speaker and headphones, the centre detent on KNOB 4, DUCK on a bass track.

## October 26 (UTC): Flash 34 (DUB TAPE's CPU1 bench), Flash 35 (back on the Nightly)

- Before: a full backup through the page's code, `Orange Cat-backup-2026-10-26-before-dubbench.json` (50 objects with the arrangement objects, 84,063
  B, read back; object 84 is 2,428 B: EDITOR_PROTOCOL.md corrected in 399de14).
- **Flash 34**, 2026-10-26T19:24:31Z: dub-tape eaa1dd7 with OC_DUB_BENCH=1 (ci_step build; host suite passed on eaa1dd7; reviewed): unit on
  `ORANGE CAT v0.6.2-eaa1dd`. The rows (`~/fm1-hw/dubtape/dub_rows.py`, dub-tape BUILD.md "Step 2"): one LITE instance about 308 us a half on CPU1,
  four 1,190 us; tracks and parameters put back.
- **Flash 35**, 2026-10-26T19:31:12Z: the published Nightly (`~/fm1-hw/nightly-3ca2b56/melodee-nightly.fwsc`): unit on `ORANGE CAT v0.6.2-3ca2b5`,
  the pack this build's, WHEEL (start 0, state 7), SHAPES (start 1, state 7); tracks MACRO 10 / 0 / 1 / 20 as found, track 1's CHRD MAJ and VOIC
  OPEN set again (the restart cleared them), CLK 0, ROUT 0, BUDGET 18, track 1 selected. PipeWire: the FM-1 card profile off and the AUDIO4c's
  output only, for the captures (set again after each flash's re-enumeration).
- Not yet published or flashed on linux-hw: the web-app merge (ca258e9, local; its suite passed in 1,276 s; to be merged again over linux-hw's two
  documentation commits since).

## October 26 (UTC): project slot 1 restored (the user's word)

- 2026-10-26T19:40:15Z, on the Nightly (v0.6.2-3ca2b5): object 2 alone from `Orange Cat-backup-2026-10-24.json` (`~/fm1-hw/slot1/restore_obj2.py`:
  BACKUP_PUT begin, data, commit; then BACKUP_LIST): **27,904 B, D954DF93, read back equal**. The October 24 backup has no arrangement objects (it
  predates them), so the slot's chains, song and scenes are a new project's; its sounds, steps, timed notes and motion are the October 24 ones.
- **The rule from now on**: power-on loads slot 1 (PROJ_LAST), so after every flash or restart the music on the unit is slot 1's and any script
  that changes the tracks would be autosaved into it 5 s after it rests. Before any script after a flash: `restore_obj2.py` with the same file
  (object 2 written back with its own bytes: a restore of the slot the music came from leaves the music from no slot, editor_backup.c), then
  the scripts. At the end the unit is left so; the next power-on loads slot 1 as the user has it.

## October 26 (UTC): Flash 36 (linux-hw 46aa3bc), the two-hour stability run on the internal clock

- **Flash 36**, 2026-10-26T19:41:42Z: linux-hw 46aa3bc (`~/fm1-hw/wt-merge/build/orangecat-0.6.2-46aa3bc.fwsc`; its host suite passed): unit on
  `ORANGE CAT v0.6.2-46aa3b`. PipeWire profiles set again; slot 1 detached with `restore_obj2.py` (D954DF93) before any script.
- **The load** (`~/fm1-hw/soak/stab.py`): MACRO VA BASS, DRUM-X TECHNO RAW, VA BASS, TECHNO RAW, the chord on every step; the internal clock
  through a one-row chain (editor command 74) started again whenever it ends (about every 33 s). Trial: 2,356 us worst, 0 late, 53 % load.
- **Run 1 (aborted at 71 min, the script's fault)**: it reset the counters at each 10-minute sample and read them 20 s later, so late renders were
  counted only inside the samples; and it did not restart the chain during a sample. The chain's 33 s locked to the 10-minute period (18 to
  one), so every sample caught a chain's end: 2 to 4 s of music and 16 s of silence (the "3.6 dB RMS drop" between the first samples was that).
  Stopped with SIGINT so its put-back ran (true, CLK 0). Kept in `~/fm1-hw/soak/stab-run-1-aborted/`.
- **Run 2** (fixed: the counters read for the whole interval before each reset, the chain polled every 0.5 s during the samples too, the share
  of each sample playing logged), 2026-10-26T21:10Z to 23:08Z, 7,200 s: **0 late renders over the whole run; worst 2,480 us** (85 % of the
  2,902 us deadline; the 10-minute intervals 2,408 to 2,480); every sample playing throughout (RMS -17.4 dBFS, peaks -7.7 to -9.6 dBFS);
  **DAC silences 0**; USB holes 0 to 3 a sample with none at the analog out (the USB capture, as on Flash 33); 222 chain restarts (each a stop and
  start of the internal transport, under 0.5 s). **Two chain queries unanswered** (12.35 and 23.91 min; run 1 had one at 22.8 min): the
  next query answered, the transport ran on. Open: which side drops the reply (the editor's request while the chain restarts, or the PC's
  MIDI), not chased yet. Tracks and parameters put back (true), CLK 0; backup before and after: **objects 1 and up identical**.
- Next: the external-sync run (`~/fm1-hw/clock/extsync.py`, the same load on the PC's clock through the AUDIO4c's DIN, CLK 2; its counters fixed
  the same way), started 23:10Z.

## October 26 to 27 (UTC): the external-sync run, the clock outage, Flash 37 (the Nightly), install cycling

- **External sync** (`~/fm1-hw/clock/extsync.py`, on Flash 36, linux-hw 46aa3bc), 2026-10-26T23:10Z to 2026-10-27T01:09Z, 7,200 s: the stability
  run's load on the PC's clock through the AUDIO4c's DIN out into the TRS MIDI in (CLK 2), 120 BPM. **Clocks sent 345,602, taken 345,602:
  no missed tick**; the unit's lost-clock stops 0, resyncs 0, its longest gap 25 ms. The PC's gaps (nominal 20.83 ms): 99.85 % in the 20 ms
  bin, 517 from 17 to 19 ms or 21 to 24 ms, the longest 24.62 ms (at 60.1 min); none near the 500 ms stop. **Late renders 0 over the whole run,
  worst 2,400 us**; DAC silences 0; RMS -17.4 dBFS throughout. (The mid-run "missed" of +-1 in some samples is the PC's count and the unit's read
  a pulse apart; the totals at the end are equal.) Put back (true), CLK 0; objects 1 and up identical before and after.
- **The clock outage** (`resync.py trs 120 60 1.2`): the clock cut for 1.2 s mid-run and sent again on the master's grid. The unit stopped once
  (the 500 ms rule; no kick while cut), resynced once, its missed count 58 = the 58 pulses the PC held back, no sent pulse lost; **the first kick
  back on quarter 63, pulse 1512 = the expected 1 + 24 k pulse**; latency median 8.63 ms before and after (spread 1.0 and 2.0 ms). The first try's
  put-back check timed out on one editor reply (the put-back itself ran; the tracks read as found after); the check now retries.
- **Flash 37**, 2026-10-27T01:13:56Z: the published Nightly (`~/fm1-hw/nightly-3ca2b56/melodee-nightly.fwsc`), for the install cycling on the
  build its files belong to: unit on `ORANGE CAT v0.6.2-3ca2b5`, the pack this build's, WHEEL (start 0, state 7), SHAPES (start 1, state 7),
  tracks MACRO 10 / 0 / 1 / 20. PipeWire profiles set; slot 1 detached (D954DF93, read back equal).
- **Install cycling** (`~/fm1-hw/cycle/cycle.py`, the Nightly's own SHAPES .ocm and pack, through oc_install.mjs, the page's code; the state read
  back after every step). **Run 1 stopped at SHAPES cycle 4** (3 clean): the install's write got no reply in its 1 s (`ocLost`: fm1oc.js
  `ask` without a retry on module writes). The abandoned install left the start empty (state 0), **nothing torn**; the remove and the script's
  closing install ran clean. **Run 2: 50 SHAPES install / remove cycles and 20 pack erase / install cycles, every state as expected, never a
  torn start, 0 fails**; ended with SHAPES at start 1 (state 7) and the pack installed (state 1). Kept: `cycle-run1.*`, `cycle.out`.
- **Dropped editor replies, five today, two builds** (linux-hw 46aa3bc and the Nightly): three chain queries in the stability runs, one step read
  in resync.py's check, one module write in cycling. Each time the next request was answered. Not chased yet: whether the unit never sent the
  reply or the PC's MIDI input lost it (a USB MIDI capture beside the request would tell). The page's installer would show `ocLost` to a user
  when it hits one; a retry of a write (the same offset again) would need the firmware to accept a repeated offset.
- The unit after cycling: `ORANGE CAT v0.6.2-3ca2b5`, the pack installed, WHEEL start 0, SHAPES start 1, tracks MACRO 10 / 0 / 1 / 20. To set
  again at the very end (after the core runs' flash): the selected track, and track 1's CHRD MAJ and VOIC OPEN (cleared by a restart, as after
  Flash 35).

## October 27 (UTC): Flash 38 and 39, the two-core controlled runs

- **Flash 38**, 2026-10-27T01:18:52Z: measure/core-split 0b45e5f with OC_CORE_STATS=1 OC_DUB_BENCH=1 (host suite passed on 0b45e5f, 1,175 s):
  `ORANGE CAT v0.6.2-0b45e5`; the pack, WHEEL and SHAPES kept through the flash. PipeWire set; slot 1 detached (D954DF93). The controlled runs
  (`~/fm1-hw/cores/core_runs.py`, 44 runs, then the CLOUD and spring groups again after the stats fault below): BUILD.md "What the two cores do",
  Parts 3 and 4. Tracks, parameters, CLK and G_RTYPE put back (true) after each pass.
- **The stats fault**: one dropped reply to AUDIO_STATS made fm1hw.py fall back to schema 2 for the session, on which a reset no longer starts Late
  over; the late counts of the last rows accumulated. Fixed in fm1hw.py (6ed5675); the affected groups run again.
- **Flash 39**, 2026-10-27T01:53:22Z: the same commit with OC_DUB_BENCH=1 only (no core stats; AUDIO_STATS [0, 1] answers schema 2): the
  overhead rows, two passes; then the same rows had run twice on Flash 38. The stats cost 30 to 40 us on the worst.
- **Flash 40**, 2026-10-27T02:04:41Z: back on the published Nightly (`~/fm1-hw/nightly-3ca2b56/melodee-nightly.fwsc`): `ORANGE CAT v0.6.2-3ca2b5`,
  the pack this build's, WHEEL (start 0, state 7), SHAPES (start 1, state 7); tracks MACRO 10 / 0 / 1 / 20 as found, track 1 selected, CLK 0;
  track 1's CHRD MAJ and VOIC OPEN set again (the restart cleared them) and read back. PipeWire set; slot 1 detached (D954DF93, read back equal):
  the next power-on loads slot 1 as the user has it. **The unit's end state.**
- Not done: the candidates' unit measurements "in B1 order": the order is not in any document or transcript this session can read (fmbell,
  supersaw and dddd have their unit rows from Flash 28; aether, brassophone, mixtur and arc are not ported; granular waits on the user).


## October 27 (UTC): the scheduler's stage 1 (the ports on the voice batch): Flash 41 onward

- **Flash 41**, 2026-10-27T03:21:55Z: the Flash 38 image again (measure/core-split 0b45e5f, OC_CORE_STATS=1 OC_DUB_BENCH=1: the DUB kernel in
  flash) for M1, the placement A/B's first half: `ORANGE CAT v0.6.2-0b45e5`; PipeWire set; slot 1 detached (D954DF93). The `dub` and `mallet_dub`
  groups (`~/fm1-hw/m1/m1-flash.json`): DUB LITE on CPU1 330 us one, 1,281 four beside four MALLET tracks on CPU0 (329 / 1,271 beside VA BASS in
  Part 3): no cost of both cores fetching from flash at once. Put back true. (The SRAM half, measure/dub-ram 685b3b3, passed its suite but was not
  flashed: the track-level design it belonged to was dropped; the A/B is redone on the batch build, measure/batch-s1-ram.)
- **Flash 42**, 2026-10-27T05:37:52Z: measure/native-cores 7202333 (0b45e5f with FM6 built in: MELODEE_FM6=1, MELODEE_SHAPES=0,
  MELODEE_DRUMX=0, FM6's boot tables and recording_due in .pool; OC_CORE_STATS=1; host suite passed, 2,815 s): `ORANGE CAT v0.6.2-720233`.
  PipeWire set; slot 1 detached. Tracks read MACRO/10, FM6/10, FM6/10, MACRO/20: the stored project has engine 12 on tracks 2 and 3, which the
  Nightly shows as its fallback (MACRO/0, MACRO/1); the put-back restores them as they were.
- **The module area for the Prophet**: its 8 sectors fit no start while WHEEL (start 0) and SHAPES (start 1) are installed. `remove 7`, then
  `remove 14` (one engine each; never the area), then the Prophet module of this build (`orangecat-prophet-0.6.2-7202333.ocm`) at start 0: state 7,
  engine 19. oc_install.mjs did not return after the write (a reply lost: the same as the dropped editor replies); `info` read the module
  registered. **WHEEL and SHAPES are to be reinstalled at the end** (the Nightly's packages, `~/fm1-hw/nightly-3ca2b56/`).
- **FM6 x4 and the Prophet x4**, the MALLET ladder's shape (one to four tracks, single-core forced and dual, effects off and on; core_runs.py
  groups `fm6`, `prophet`; `~/fm1-hw/native/native-7202333.json`): CPU1 renders their voices (voice jobs 14,000 to 57,000 a run; cpu1 voice
  1,400 to 2,250 us a half for FM6, about 1,650 for the Prophet). FM6 x1 3,601 -> 2,302 us (late to clean), the Prophet x2 3,932 -> 2,349 (late
  to clean); four tracks of either still over the budget dual-core. BUILD.md, stage 1.
- On linux-hw: **0a7b1b9**, stage 1 (MACRO, DRUM-X, SHAPES and WHEEL on the batch; host suite passed, 1,377 s). Measurement branches:
  measure/batch-s1 cbfa9c9 (0a7b1b9 + the DUB bench riding in the batch) and measure/batch-s1-ram 3e2819b (the same with MACRO's kernel, its VA
  and MODAL renders and the DUB kernel in .dsp_text).
- **Flash 43**, 2026-10-27T07:38:17Z: measure/batch-s1 f0ee004 (linux-hw 0a7b1b9 + the DUB bench riding in the batch, OC_CORE_STATS=1
  OC_DUB_BENCH=1; host suite passed, 2,506 s; reviewed twice before it went on: the draft and the committed 0a7b1b9, no bug in the audio
  path): `ORANGE CAT v0.6.2-f0ee00`. PipeWire set; slot 1 detached. The Prophet module (7202333's) still at start 0. Rows
  (`~/fm1-hw/s1/A-prophet-kcost-dub.json`): the Prophet ladder, the `dub` group, `kcost` (the ports' kernel costs). Then `remove 19` and this
  build's WHEEL (start 0, 2,928 B) and SHAPES (start 1, 24,160 B), both FW_API 6, registered; `kcost` again through the modules
  (`A-kcost-modules.json`). VA BASS x4 dual effects off: wall 1,058 mean, 1,850 max (Part 3: 1,421 / 2,579); MALLET x4 dual: 2,708 / 3,750
  (5,035 / 6,428). Put back true each pass.
- **Flash 44**, 2026-10-27T07:59:37Z: measure/batch-s1-ram 14f28c2 (the same with MACRO's kernel, VA and MODAL renders and the DUB kernel in
  .dsp_text; host suite passed, 1,331 s, after its first suite failed tests/target_budget.py: the SRAM-placed VA render inlined into
  mc_model_render, made noinline): `ORANGE CAT v0.6.2-14f28c`; WHEEL and SHAPES (f0ee004's) loaded. `kcost` and `dub` (`B-kcost-dub.json`):
  VA in SRAM 161 us a voice's block against 89 in flash, MODAL the same, DUB 4.5 to 7 % faster. BUILD.md, "Stage 1: placement".
- linux-hw 6356588: audio_batch_cost from these numbers, the placement rule in ADDING_AN_ENGINE.md.
