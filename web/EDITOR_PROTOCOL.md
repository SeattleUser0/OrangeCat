# Melodee editor protocol (SysEx over USB-MIDI)

The firmware side is `firmware/src/editor.c`. Commands 1-15, track selection, dumps, step reads
and parameter writes (27, 29, 30, 31) were checked on hardware. `tests/editor_test.c` also exercises the real C
handler, malformed transfers and stop timeouts against simulated flash. User-preset
flash writes and live pushes (16-26, 32) have host coverage; that does not establish
their hardware behavior. The step chance byte, motion (64) and the full backup (65-67) have host
coverage only (`tests/editor_test.c`, `tests/backup_test.c`, `web/test_backup.mjs`).

**v3 (four tracks):** the device has four tracks, each a synth part (since 1.0; before it, track 4
was a GM drum track: see "Track 4 since 1.0" below). One
of them is *selected* (ALGORITHM on the device, or `TRACK`). Every v1 / v2 command acts on the
selected track (its parameters, engine, preset, steps, the user presets it stores or loads); `TRACK`,
`TRACK_MIX`, `TRACK_DUMP` and `TRACK_STEP` reach any track. Command numbers 1-26 are unchanged.

**v4 (any track's parameters):** `TRACK_PARAM` gets or sets a parameter of any track without changing
the selection, and the `TRACK_CHANGED` push follows level, pan and mute of the tracks that are not
selected. The v1-v3 commands are byte for byte as before; v4 is asked for with bit 1 of `WATCH`.

**v5 (the drum grid):** a step also holds lane hits and their accents (see "v5: the drum grid" below).
`STEP_GET` / `STEP_SET` / `TRACK_STEP` replies get 3 more bytes, `STEP_SET` / `TRACK_STEP` take them
optionally, and `UP_GET` / `UP_PUT` carry a user preset's 16-step grid after its pattern. Every
earlier byte is where it was; an editor that reads the first bytes of a reply keeps working.

**v7 (motion, chance, backup, 89 parameters):** a step gets a chance byte after its hits. `MOTION` (64) reads
and edits a track's recorded knob moves. `BACKUP_LIST` / `BACKUP_GET` / `BACKUP_PUT` (65-67) read and
restore the whole device. P_COUNT is 89 and P_E0 is 81: twenty FM operator parameters (ids 61..80) went in
before the engine parameters, which moved from 61..68 to 81..88. Always take P_E0 from `INFO`. INFO ends with
tagged capability blocks for these (see `INFO`).

**Chord keys (91 parameters, 1.0):** two track parameters, `CHRD` (81) and `VOIC` (82), went in before the
engine parameters, which moved from 81..88 to 83..90: P_COUNT 91, P_E0 83. No command changed; an editor that
takes P_COUNT and P_E0 from `INFO` keeps working (see "The chord keys" below).

## Framing

The device USER sheet also exports general Melodee presets as `F0 7D 4D 4C 01 <packed record>
<checksum> F7`. This is a preset file rather than an editor request. The record is the complete
238-byte `up_rec_t`, including its twelve-byte name and stored pattern. Pack groups of seven
raw bytes as one high-bit mask (bit j is byte j's bit 7), followed by seven low-seven-bit bytes.
The final shorter group uses only its corresponding mask bits. The checksum makes the sum of
all bytes between F0 and F7 zero modulo 128. Version, exact length, 7-bit encoding, checksum and
record validity are checked before staging. Imports open the general USER list for choosing a
slot and confirming replacement. FM6, CZ-1 and Prophet slot exports use their native single-voice
formats instead. Bulk export concatenates a frame for each used selected slot.

A request is `F0 7D 46 4C <cmd> <args...> F7`:

- `7D` is the non-commercial SysEx ID.
- `46 4C` is "FL".

Every valid request gets exactly one reply, with the same header and the same `<cmd>`.
Unknown commands and invalid fixed argument lengths get no reply; argument errors
follow the command-specific rules below.
Fixed-size commands require exactly the lengths in the tables; step writes accept the
8-byte legacy step, the 11-byte grid step or the 12-byte step with its chance (a chance above 100 gets
no reply). Every data byte is 7 bit.
MIDI realtime bytes may occur inside SysEx; another status aborts the partial frame. While the editor
watches (v2, `WATCH`), the device also sends push frames (cmds 23, 24, 26) at any time.

| Item | Encoding |
| --- | --- |
| value (v14) | 2 bytes, LSB first, holding value + 8192, so the range is -8192..8191. `[lo, hi]`: value = (lo \| hi << 7) − 8192 |
| u32 (v7) | 5 bytes, 7 bits each, LSB first; the last byte is 0..15. Used by the backup commands |
| string | ASCII bytes, ended by a 0 byte |
| scope | 0 = parameter of the selected track (`P_*`, 0..P_COUNT−1); 1 = global parameter (`G_*`, 0..G_COUNT−1) |
| track | 0..3: tracks 1..4 (synth parts) |
| engine byte | 0..NENGINES−1 (firmware before 1.0: NENGINES = its drum track, no engine). The numbers are fixed, new engines are appended: 0 reserved (ANALOG before 2.0), 1 reserved (DIGITAL before 1.0: see below), 2 PHASE, 3 SID (LOFI before this change), 4 reserved (SAMPLE retired), 5 VOICE, 6 reserved (TRIO retired), 7 reserved (WHEEL retired), 8 reserved (GRAIN retired), 9 DRUM-X (Orange Cat 0.9.0, with MELODEE_DRUMX; the slot of the retired PHYS, reserved where the switch is off), 10 DRUM, 11 NOISE, 12 FM6, 13 reserved (SLICE retired), 14 reserved (OBXF retired), 15 CZ-1, 16–18 reserved, 19 PROPHET (NENGINES 20). The new-sound browser order is PROPHET FM6 PHASE CZ-1 SID VOICE NOISE DRUM (`ENGINE_ORDER`), and in Orange Cat DRUM-X after DRUM (`ENGINE_ORDER`). 0 ANALOG is retired since 2.0 (its stored sounds keep the number and play silent); the numbers stay |

The engine parameters are `P_E0..P_E7`: P_COUNT−8 .. P_COUNT−1 (83..90), and `INFO` gives `P_E0`.
Their meaning, range and names depend on the current engine, so re-read `DESC` for them
after an engine change.

The modulation matrix (the MOD page, `firmware/src/mod.c`) is twelve track parameters at ids 49..60 (since
P_COUNT 69): slot k (1..4) is `SRCk`, `DSTk`, `AMTk` at ids 49 + 3 (k − 1) .. 51 + 3 (k − 1).

| id | label | values |
| --- | --- | --- |
| 49, 52, 55, 58 | SRC1..SRC4 | enum: 0 OFF, 1 LFO, 2 ENV, 3 VEL, 4 KEY, 5 RAND, 6 MODW (CC1), 7 AT (channel aftertouch), 8 EXPR (CC11) |
| 50, 53, 56, 59 | DST1..DST4 | enum (20 names): 0 OFF, 1 PITCH, 2 CUT, 3 SHP, 4 AMP (per voice); 5 PAN, 6..9 retired (empty names), 10 RATE (LFO rate), 11 VIB (LFO pitch depth), 12..19 E1..E8 = `P_E0..P_E7` (per block) |
| 51, 54, 57, 60 | AMT1..AMT4 | −64..63 (fmt BIPCT) |

`DESC` names E1..E8 as such; the device shows the engine's label of that parameter instead (`DESC` of
`P_E0 + n`), and so does the editor. A slot with SRC, DST or AMT at 0 does nothing; every default is 0
(a factory preset sets all twelve to 0). The modulation never changes the stored values: `GET`,
`DUMP` and the pushes report what was set.

**DIGITAL (engine 1) was replaced by FM6 (Dexed-based)** in 1.0. The number stays reserved: `INFO` names it
"-" (a build with `MELODEE_FM4=1` has DIGITAL back, named "DIGITAL"), `NAMES 1` lists no presets, its EDIT
descriptors are "-" with DIGITAL's ranges, and no track ever has it. Whatever brings a DIGITAL sound plays it as
FM6 with a patch converted from its values (`firmware/src/fm4_convert.c`; the patch is the track's own, `FM6_GET`
reads it, a project keeps it): `SET` of `G_ENGSEL` = 1 (DIGITAL's first preset), `PRESET` 1 k (its preset k),
`UP_LOAD` of a slot stored with engine 1 (`UP_PUT` still takes engine 1: the record keeps the DIGITAL values, every
load converts them), projects with DIGITAL tracks (their motion on E1..E8 and ids 61..80 is dropped). The track
reports engine 12 (FM6) and the FM6 preset that covers the sound. The editor converts DIGITAL patches of library
files the same way (it equals the firmware's, `test_web.mjs`).

The four FM operator envelopes (the DIGITAL engine's OP1..OP4 ENV and OP LEVEL pages, `eng_digital.c`) are
twenty track parameters at ids 61..80, five per operator: operator k (1..4) has `ATK`, `DEC`, `SUS`, `REL`,
`LVL` at ids 61 + 5 (k − 1) .. 65 + 5 (k − 1). Since DIGITAL was retired they are inert: no page and no editor
layout shows them, only the conversion of a DIGITAL sound reads them (its FM6 tracks hold the defaults). The ids
and labels stay (library files key values by label).

| id | label | fmt, range, default |
| --- | --- | --- |
| 61, 66, 71, 76 | ATK | TIME, 0..127, 0 |
| 62, 67, 72, 77 | DEC | TIME, 0..127, 0 |
| 63, 68, 73, 78 | SUS | PCT, 0..127, 127 |
| 64, 69, 74, 79 | REL | TIME, 0..127, 0 |
| 65, 70, 75, 80 | LVL | PCT, 0..127, 127 |

The defaults change nothing: the sound is the same as before the parameters existed. The master `ENV`
(ids 1..4) shapes the whole voice; these shape each operator on top of it. Other engines ignore them, and the
device shows the pages for DIGITAL only. Descriptors are the same for every engine.

The chord keys (SCL > CHORD on the device, `firmware/src/chord.c`) are two track parameters at ids 81, 82:

| id | label | values |
| --- | --- | --- |
| 81 | CHRD | enum: 0 OFF, 1 DIA3, 2 DIA7 (the triad / seventh of the track's ROOT and SCALE on the key: every tone in key), 3 MAJ, 4 MIN, 5 DOM7, 6 MAJ7, 7 MIN7, 8 SUS4, 9 POW (fixed shapes) |
| 82 | VOIC | enum: 0 CLOSE, 1 OPEN (1-5-3), 2 INV1, 3 INV2, 4 +OCT (the root an octave down; a seventh drops its fifth) |

With CHRD on, a key, a MIDI note of that track and so the arp's held notes play the chord (at most 4 notes),
and live recording writes it into one step; MONO / LEGATO / UNISON play its root; a kit (DRUM, SAMPLE PERC)
ignores it. Both defaults are 0: nothing changes until CHRD is set. They are the track's, like ARP and SCL: a
sound load (a factory preset, `UP_LOAD`, an audition) keeps them, and motion never records them.

Other enums that grew: `P_SDIV` (DIV, id 30) has 10 names (1/4, 1/8, 1/16, 1/32, 8T, 16T, 1/2, 1/1, 2BAR,
4BAR; the first six keep their numbers), `P_AMODE` (id 17) 7 (OFF, UP, DN, UPDN, RND, ORD, REPEAT) and the
global `G_CLOCK` (id 2, label "CLK") 3 (INT, USB, TRS). `G_MIDI` (id 12) is an enum of USB / TRS that nothing reads.

## Commands

| cmd | Request args | Reply args |
| --- | --- | --- |
| 1 INFO | — | version string, NENGINES, P_COUNT, G_COUNT, NSTEP, P_E0, then NENGINES engine-name strings, then (v3) NTRK (4), then (v6) CHAIN_ROWS (16), then the tagged blocks `55 01 uiCaps`, `4D 01 64 01` and `42 01 3` (below); older firmware ends earlier |
| 2 GET | scope, id | scope, id, v14 |
| 3 SET | scope, id, v14 | scope, id, v14 (the value after clamping). Setting global `G_ENGSEL` (id from DESC label "ENG") changes the engine: its defaults, then its first preset (as on the device) |
| 4 DUMP | — | engine, preset, then P_COUNT × v14 (the selected track), then G_COUNT × v14 (globals), then the preset's high bits (preset >> 7; see "Presets past 127") |
| 5 DESC | scope, id | scope, id, fmt, min v14, max v14, def v14, label string, unit string, then for an enum (fmt 8) one name string per value (at most 24; firmware before the matrix: at most 16) |
| 6 STEP_GET | index 0..NSTEP−1 | index, n (0..4 notes), note0..note3, time (0 NOTE, 1 TIE, 2 REST), flags (1 accent, 2 slide), vel, then (v5) hits (3 bytes, below), then (v7) chance 0..100 |
| 7 STEP_SET | index, n, note0..3, time, flags, vel [, hits (3 bytes, v5) [, chance 0..100 (v7)]] | same as STEP_GET (after the write). Without the hits the step keeps its own; without the chance it keeps its own. The chance can only follow the hits |
| 8 PRESET | engine, preset [, preset >> 7] | engine, preset, preset >> 7 (applies the preset's sound; the steps and the track's own parameters stay, see "Sound loads and undo"); (0.11.0) a fourth byte only when a module's engine was refused: 1 ENGINE TOO HEAVY, 2 NO ROOM, the track keeps its engine and its sound, the first three bytes are what it holds) |
| 9 PROJECT | op (0 load, 1 save, 2 query), slot 0..3 | op, slot, used (1/0). Save writes flash: allow ~2 s; it stops the transport first (see "Saves while playing") |
| 10 NAMES | engine [, first preset, 2 × 7 bit] | engine, count (≤ 127), count preset-name strings (≤ 20 characters) from the first, then the two edit-page titles, then all presets and the first (2 × 7 bit each). Ask again from first + count until all are read |
| 11 SMP_BEGIN | slot 0..2 | slot, rc (0 ok). Erases the slot's header sector: the slot is empty from now on |
| 12 SMP_WRITE | slot, offset (3 × 7 bit, LSB first), pack7 data (≤ 256 bytes) | slot, offset, rc: 0 ok, 1 arguments, 2 erase, 3 write, 4 slot in use (send SMP_BEGIN first). Offset ≥ 512 and a multiple of 256; writes go in increasing order (a write at a 4 KiB boundary erases that sector) |
| 13 SMP_END | slot, pack7 header (480 bytes) | slot, rc: 0 ok, 1 size, 2 header, 3 data CRC, 4 flash, 5 zones |
| 14 SMP_ERASE | slot | slot, rc (erases the whole slot, ~1 s) |
| 15 SMP_INFO | — | slots, slot KiB, then per slot: zone count (0 = empty), name string, data KiB |
| 16 UP_LIST | start, count (1..16) | start, count, total slots, then per slot: used (0/1), engine, name string ("" if unused) |
| 17 UP_GET | slot | slot, used, engine, name, P_COUNT × v14, 16 × (note, flags), then (v5) kind (0 a note pattern, 1 a drum grid) and for kind 1 16 × hi |
| 18 UP_PUT | slot, engine, name, P_COUNT × v14, 16 × (note, flags) [, kind 0 or 1, 16 × hi (v5)] | slot, rc (0 ok, 1 args, 2 flash). Writes flash: allow 1 s |
| 19 UP_STORE | slot, name | slot, rc. Stores the current sound: engine, parameters, the first 16 sequencer steps as the pattern (TIE steps → flag 4) |
| 20 UP_LOAD | slot | slot, rc (0 ok, 1 empty/invalid). Applies its sound (not its pattern; the steps stay) |
| 21 UP_ERASE | slot | slot, rc |
| 22 WATCH | on (0/1; v4: 3 = also `TRACK_CHANGED`) | on (0/1; v4 firmware: 3 when 3 was asked for). While on, the device pushes cmds 23, 24, 26 (and 32 with bit 1) |
| 23 CHANGED (push) | — | scope, id, v14 |
| 24 RELOAD (push) | — | engine, preset, then (v3) the selected track, then preset >> 7 |
| 25 PING | — | 0 |
| 26 STEP_CHANGED (push) | — | index, then (v3) the selected track |

**Presets past 127.** PROPHET has 201 factory presets (INIT, then Sequential's 200 v1.03 programs).
A preset number is 7 bits where the tables above say "preset"; its high bits follow at the end of the
reply (older editors ignore them, older firmware sends none: read them only if present). `PRESET` takes
them as a third byte. `NAMES` pages: firmware before this sends one reply with every name and no totals;
an older editor sees the first page (the names that fit 1024 bytes).

| cmd (v3) | Request args | Reply args |
| --- | --- | --- |
| 27 TRACK | — (query), or track (select it) | selected track, NTRK, then per track: engine byte, preset, level v14, mute (0/1), armed (0/1, live recording), then per track preset >> 7 |
| 28 TRACK_MIX | track (get), or track, level v14 (0..127), mute (set) | track, level v14, mute: the track's `P_LEVEL` and `P_MUTE` |
| 29 TRACK_DUMP | track | track, engine byte, preset, P_COUNT × v14 (that track's parameters; no globals), then preset >> 7 |
| 30 TRACK_STEP | track, index (get), or track, index, n, note0..3, time, flags, vel [, hits (v5) [, chance (v7)]] (set) | track, index, n, note0..3, time, flags, vel, then (v5) hits, then (v7) chance |

| cmd (v4) | Request args | Reply args |
| --- | --- | --- |
| 31 TRACK_PARAM | track, id (get), or track, id, v14 (set); id = `P_*` (0..P_COUNT−1) | track, id, v14 (the value after clamping, as `SET`). The selection does not change; no push about the editor's own write |
| 32 TRACK_CHANGED (push) | — | track, id, v14: `P_LEVEL`, `P_PAN` or `P_MUTE` of a track that is not selected changed on the device (only while `WATCH` was sent with bit 1) |

| cmd (v7) | Request args | Reply args |
| --- | --- | --- |
| 64 MOTION | track (query); track, 1, on 0/1 (play on / off); track, 2 (clear); track, 3, step, id, v14 (set an event); track, 4, step, id[, v14] (delete an event) | track, rc, on (0/1), count (this track's events), max (64), then count × (step, id, v14) |
| 65 BACKUP_LIST | — | 1, rc, count (11), then per object: id, size u32, crc u32 |
| 66 BACKUP_GET | id, offset u32, count lo, count hi (≤ 256) | id, rc, offset u32, count lo, count hi, pack7 data |
| 67 BACKUP_PUT | op 0 begin: 0, id, size u32, crc u32; op 1 data: 1, id, offset u32, pack7 data; op 2 commit: 2, id; op 3 abort: 3, id | op, id, rc |

Without flash (no flash part found at boot) `SMP_BEGIN`, `SMP_WRITE`, `SMP_END` and `SMP_ERASE` get no
reply.

**pack7:** groups of up to 7 bytes, each preceded by one byte holding their top bits
(bit j = bit 7 of byte j). A mask must have at least one following data byte;
unused mask bits in the last group must be zero. Oversized or incomplete transfers
are rejected; SMP_WRITE decodes at most 256 bytes, SMP_END exactly 480 bytes.

**User sample slot** (80 KiB each, SAMPLE engine sets USR1..USR3; reference uploader
`tools/fm1_sample_upload.py`, slot builder `sampleio.user_slot`; the editor's port of it is
checked byte for byte by `web/test_web.mjs`): header at 0, ADPCM data at 512.

| Offset | Field |
| --- | --- |
| 0 | magic `"FSMP"` (u32 0x504D5346), u16 version 1, u8 zone count 1..16, u8 0 |
| 8 | name, 8 ASCII bytes (0-padded) |
| 16 | u32 data length (bytes), u32 CRC-32 (zlib) of the data, 8 bytes 0 |
| 32 | 16 zones × 28 bytes: u32 off (in the data), n (samples), loop start, loop end, rate (Hz / 44100 × 65536); i16 root × 16 (MIDI note), ADPCM predictor at the loop start; u8 step index at the loop start, lo note, hi note, looped (0/1) |

Data is IMA ADPCM, 4 bit, low nibble first, starting from predictor 0 and step index 0.
All little endian.

The slot's last 4 KiB sector (offset 0x13000) holds SLICE's slices set by hand on the device (`src/slice_store.c`:
magic `"SLM1"`, the sample's length and data CRC, the slice starts) when the data leaves it free (at most 77,312
bytes). An upload writes over it like any other data; a record that is not the slot's sample's is ignored.

`fmt` values (`firmware/src/core.h`):

| Value | Name | Value | Name | Value | Name |
| --- | --- | --- | --- | --- | --- |
| 0 | INT | 5 | CUTOFF | 10 | NOTE |
| 1 | PCT | 6 | DB | 11 | ONOFF |
| 2 | BIPCT | 7 | SEMI | 12 | OCT |
| 3 | TIME | 8 | ENUM | 13 | STEPS |
| 4 | LFOHZ | 9 | BPM | | |

The editor should show the value with the unit; formatting it exactly like the device does
is not required.

## v2: user presets

A user preset = engine (0..NENGINES−1), name (1..12 chars, ASCII 32..126; the device shows it upper
case), all P_COUNT instrument parameters (v14 each, the same order as `DUMP`), and a 16-step pattern:
16 × (note 0..127 (0 = rest), flags: 1 accent, 2 slide, 4 tie). Loading one applies the engine and
the parameters of the sound, as a factory preset (the track's own parameters, the steps and LEN stay; see
"Sound loads and undo"). The stored pattern is kept and returned by `UP_GET`; the device no longer loads
it (its phrase list, SEQ > PHRASES, was removed in 2026-10). The slots are
numbered 0..63 (the device shows U01..U64).

- `UP_LIST`: count is cut at 16 and at the last slot (start ≥ 32: count 0, no entries).
- `UP_GET` of an empty slot has the same shape with used 0, engine 0, name "" and all values 0.
  Values come back in the current parameter order, inside their ranges.
- `UP_PUT`: rc 1 for a slot ≥ 32, an engine ≥ NENGINES, a name that is empty, longer than 12 or has
  bytes outside 32..126, or an incomplete or oversized frame. Values are clamped to their ranges for that
  engine. A note with flag 4 is stored as a tie (note 0); flags on a rest are dropped.
- **DRUM** (engine 10) was PHYS's MODEL 4 (DRUM) before 1.0. A record of PHYS with E1 (MODEL) = 4,
  stored then or sent by `UP_PUT` from an older editor, is the DRUM engine: the device rewrites it (engine
  10; E1..E8 {MODEL, TUNE, TONE, DECY, SNAP, ACC, KICK 0..127, PERC 0..127} become {KIT = PERC / 32, TUNE,
  TONE, DECY, SNAP, ACC, KICK 0 PUNCH / 1 ROUND (old ≥ 64), DRV 0}), and `UP_GET` / `UP_LIST` give it so.
  Projects do the same. PHYS's MODEL is 0..3 (MODAL STRNG MEMB SYMP) now.
- `UP_STORE`: name "" stores with the automatic name the device uses (engine name + slot number,
  "SID 07"). rc 1 for a bad slot or name.
- rc 2 = the flash write failed or there is no flash. A failed flash write keeps the previous
  record in RAM and flash, including its name. Without flash, a valid change is RAM-only until power-off.
  (`UP_PUT` / `UP_STORE` / `UP_ERASE` stop the transport first, see "Saves while playing"; a device
  that cannot stop it in 100 ms answers rc 2 and writes nothing.)
- Frames stay below 640 bytes (`UP_PUT` is 5 + 1 + 1 + 13 + 2 × P_COUNT + 32 + 1).

**On the device:** SAVE > USER page: KNOB 1 picks the slot, KNOB 2 LOAD, KNOB 3 ERASE, KNOB 4 SAVE;
OCT+ executes the selected action, OCT- goes back. SAVE over a used slot asks
"OVERWRITE U07?" with OCT+ / OCT-. SAVE uses the automatic name. The PRESETS knob and the SAVE > PRESETS browser continue past the factory presets into the used user
presets.

**Flash** (`firmware/src/upreset.c`): two storage objects (`OBJ_UPRESET0/1`, A/B sector pairs at
0xDC000..0xDFFFF), 16 records of 238 bytes each (192-byte legacy records migrate on load), behind a bank header (magic "UPB1", record size,
slot count; a mismatch reads as an empty bank). A record keeps its layout version (versions 1..5 are read; another: empty)
and the P_COUNT it was stored with; another count is mapped by count (last 8 values = P_E0..P_E7, the
first ones = P_LEVEL.. in order, missing ones = defaults). Versions 4 (a note pattern) and 5 (a drum grid)
store each value as one byte, value + 64, so P_COUNT can reach 127; versions 1..3 stored 16-bit values.
Version-1 records with 58 or 62 parameters belong to pre-1.0 Melodee's incompatible MPC/chord/engine
layout. They are treated as empty without changing their bytes; they are not mapped as upstream presets.
The wire format is unchanged: `UP_GET` / `UP_PUT` still carry v14 values, and `UP_PUT` now answers rc 1
for a value outside −64..127. P_COUNT was 53 (P_E0 45) until the SLICER
parameters (SLCR, PAT, RATE, DEPTH: ids 45..48) went in just before P_E0: P_COUNT 57, P_E0 49; then
the twelve matrix parameters (ids 49..60): P_COUNT 69, P_E0 61. An editor takes both from `INFO`;
records stored with 53 load with the SLICER off and every matrix slot off, with 57 with every slot off. The
twenty FM operator parameters (ids 61..80) went in before P_E0 after that: P_COUNT 89, P_E0 81. Records stored with 69 load with
those at their defaults, which change nothing. Then the chord keys (ids 81, 82): P_COUNT 91, P_E0 83; records
stored with 89 (or 69) load with CHRD OFF and VOIC CLOSE, their engine values at 83..90.

## v5: the drum grid

A DRUM track (engine 10) is edited on the device as a grid of 8 lanes × the steps (SEQ > STEP). The
grid lives in the steps themselves, so every engine has it:

- **Hits.** Each step has a lane mask `hit` and an accent mask `acc` (bit l = lane l, `acc` only on
  lanes that hit). The lanes and the General MIDI note each plays: 0 KICK 36, 1 SNARE 38, 2 CLAP 39,
  3 HAT CL 42, 4 HAT OP 46, 5 TOM 45, 6 RIM 37, 7 BELL 56 (KIT HAND plays CONGA / CLAVE on lanes 5 / 6, KIT
  CYM a cymbal on lane 8). A NOTE step plays its notes and then its hits, on any engine (DRUM strikes its
  lanes, SAMPLE PERC its GM kit, a synth plays the pitches); an accented hit at velocity 127, the others at
  the step's velocity (0 = 96). A TIE or REST step plays no hits.
- **On the wire** (after `vel`): `hit & 127`, `acc & 127`, then `(hit >> 7) | (acc >> 7) << 1`. A
  `STEP_SET` / `TRACK_STEP` of 8 step bytes (an editor of before v5) leaves the step's hits as they are;
  send `0, 0, 0` to clear them.
- **Notes on lanes.** A step's notes keep playing as before; the device shows each note on the lane
  it strikes (GM 35..81, the others folded into their octave of 36..47: a low tom 41 on TOM, a crash 49 on
  BELL). When the device edits a lane of a step, notes on that lane become the lane's hit. DRUM tracks of
  projects saved before the grid get their lanes' own notes as hits (the same notes and velocities play).
- **Sound loads never convert steps** (as before): switching a track to or from DRUM keeps its notes
  and hits.
- **User presets.** A user preset stored from a DRUM track whose first 16 steps strike a lane holds a
  16-step grid instead of a note pattern (record version 3; version 5 with byte values). `UP_GET` gives its low 7 bits in the 16
  (note, flags) pairs (hits, accents), then kind 1 and 16 bytes of bit 7s (bit 0 hits, bit 1 accents);
  a note pattern ends with kind 0. `UP_PUT` with kind 1 and the 16 bytes stores a grid (accents only on
  hits); without an extension, a note pattern as before. If an extension is sent, it must be
  exactly 17 bytes: kind 0 or 1 followed by 16 bytes (kind 0 ignores those bytes).
  Unknown kinds, truncated extensions and extra bytes are rejected without changing the slot.
  Firmware before v5 shows a grid record as empty.
- **Projects** in v5 used format 5 ("FUN5"): 10-byte steps and 40 reserved bytes per track.
  FUN6 retained those fields and added the song chain (v6 below); FUN7 is current (v7 below). Older formats
  are converted on load.

## v2: live sync

- `WATCH 1` starts the pushes. Watching ends by itself 3 s after the last request of any kind (send
  `PING` about every 1 s), on a USB reset, and when the host goes away; `WATCH 0` ends it at once.
- **CHANGED** (scope, id, v14): a parameter changed on the device (knob, menu, sequencer edit of a
  `P_*`), not by the editor's own `SET`. Coalesced: each (scope, id) at most every 20 ms, with the
  latest value.
- **RELOAD** (engine, preset): the engine, a preset, a user preset or a project was loaded; re-read
  `DESC` of the engine parameters, `DUMP` and the steps. It is also sent after loads the editor asked
  for (`SET` of G_ENGSEL, `PRESET`, `PROJECT` load, `UP_LOAD`).
- **STEP_CHANGED** (index): a sequencer step changed on the device (record, clear, step edit,
  pattern load); not after the editor's own `STEP_SET`.
- Push frames have the normal header. Accept them at any time, also while waiting for a reply:
  match replies by cmd (23, 24 and 26 are never replies). The device sends at most a few per
  ~5 ms pass, and only when its USB send queue has room, so a push never delays a reply.

## v3: tracks

- **Track 4 since 1.0** is a synth part like tracks 1..3: `DUMP` / `RELOAD` / `TRACK` / `TRACK_DUMP`
  give its real engine byte (power-on: SAMPLE, preset PERC, the General MIDI kit), and `PRESET`, `SET` of
  `G_ENGSEL`, `UP_LOAD` and `UP_STORE` work on it; its level is its `P_LEVEL`. The commands are byte for
  byte as before; only the meaning changed. Firmware before 1.0 had a GM drum track there: engine byte
  NENGINES, no presets (`UP_LOAD` / `UP_STORE` rc 1), its level the global `G_DRLVL`. Drums are now the
  SAMPLE engine's PERC set and the DRUM engine on any part: the first C key is the kick (C2 = 36), notes are GM numbers.
- Global ids 24..26 are retired format padding, as are 4..11. `G_COUNT` stays 27.
  Their descriptors have empty labels and no page; loading a song clears their runtime values.
  MIDI input follows the ROUT range filter described below, with the DRUM override only inside that range.
- Selecting a track with `TRACK` does not push `RELOAD` (the editor re-reads `DUMP`, the steps and the
  engine `DESC` itself); selecting one on the device does (`RELOAD` with the new track).
- Pushes are about the selected track only: `CHANGED` (scope 0) and `STEP_CHANGED` refer to it, and
  changes to other tracks (live recording from MIDI into another track, `TRACK_*` writes) push nothing.
- Level and mute are also `P_LEVEL` / `P_MUTE` of the selected track (`SET`); `TRACK_MIX` reaches the
  others. Presets and user presets change a part's sound but keep its `P_LEVEL`, `P_PAN`, `P_MUTE`.
- Projects (`PROJECT`) save and load all four tracks, the selection, the song chain and the motion (FUN7;
  FUN5 added the drum grid, FUN6 the chain). Formats 6..1 are converted; a format 1 project loads into track 1. A project saved before
  1.0 loads its drum track as track 4 with the power-on sound (SAMPLE PERC), its steps kept.
- Older firmware (no NTRK in `INFO`): one instrument; skip the track UI.

## v4: any track's parameters

- **Finding out:** send `WATCH 3`. v4 firmware answers 3; v3 (0.8) firmware answers 1, does not know
  cmds 31 / 32 (no reply) and never pushes `TRACK_CHANGED`. `WATCH 1` behaves exactly as in v2 / v3
  (reply 1, no `TRACK_CHANGED`). Match the `WATCH` reply by bit 0.
- `TRACK_PARAM` clamps like `SET` scope 0: to the range of that parameter; the engine parameters
  `P_E0..P_E7` to the ranges of that track's engine. A parameter
  with a fixed range (min = max) keeps its value. A track ≥ NTRK or an id ≥ P_COUNT gets no reply.
  For the selected track it is the same as `SET` scope 0.
- `TRACK_CHANGED` is never about the selected track (its changes stay `CHANGED` scope 0). Coalesced like
  `CHANGED` (each track and id at most every 20 ms, latest value), and not sent for the editor's own
  `TRACK_PARAM` / `TRACK_MIX` writes. After a selection change (`RELOAD`, or the editor's `TRACK`) the
  device takes the current values as known.

## Notes for the editor

- **One request at a time.** Wait for the reply, about 10–50 ms, before sending the next.
  The device holds only one incoming SysEx frame.
- **Following the device.** With v2 firmware, `WATCH` and `PING` (above). Older firmware pushes
  nothing (no reply to `PING`): poll `DUMP` about every 300–500 ms while the page is visible.
- **Port.** The device's MIDI port is named "Melodee" (USB 1209:0001). Updates use the same
  port with other SysEx (the `F0 22 24 35 …` keys, `00 59 …` frames); never send those
  from the editor. Since 1.0 the same USB device also has an audio input ("Melodee",
  44.1 kHz stereo; bcdDevice 3.11); the MIDI port and this protocol are unchanged, and both
  work while the computer records.
- **Global ids.** `G_ROUTE` (id 14, label "ROUT", GLO > SYSTEM) was a placeholder ("--", range 0..0);
  it is the MIDI IN routing: 0 "CH1-4", 1 "SEL", 2 "CH5-8", 3 "CH9-12", 4 "CH13-16".
  Each range maps only its four channels to tracks 1..4 in order; other channels and tracks not in use
  are ignored. SEL receives every channel on the selected track. DRUM redirects its channel to the
  first DRUM track only within the selected range (or in SEL); without a DRUM track, normal routing applies.
  Note-offs and sustain release still reach their original owners after changing ROUT. Earlier firmware
  sent out-of-range channels to the selected track; this fallback is removed. Stored values 0 and 1,
  global id 14, the global count and the project layout are unchanged. ROUT persists between starts and
  is saved with projects/templates. The editor shows it under GLOBAL > SYSTEM.
- **Sound loads and undo.** A load that changes a track's sound (`PRESET`, `SET` of `G_ENGSEL`, `UP_LOAD`,
  and the same on the device) changes the sound only: the engine and the parameters of the sound. It
  never changes the steps, nor the track's own parameters: `P_LEVEL`, `P_PAN`, `P_MUTE` (0, 39, 40), the
  ARP pages, SCL and LEN / DIV / SWING / GATE (17..32). It does drop the track's recorded
  motion (the device's SAVE held brings it back). (Before 1.0 a preset also
  set the arp and replaced the steps with its pattern; a factory preset turned the SLICER off.) The byte
  layout of every command is unchanged. Steps are recorded on the device or written by the editor with
  `STEP_SET` / `TRACK_STEP` (the device's factory phrases were removed in 2026-10).
  The device keeps one copy of the track from before the last load (any track); SAVE held 0.7 s on the
  device swaps back what the load changed (the sound). Loads in a row on one track with nothing changed between them keep the copy from before
  the first; `SET` (scope 0) within 1.5 s of a load on the selected track counts as part of that load, so
  an audition (`G_ENGSEL`, then the patch's values by `SET`, the track's own parameters skipped as
  `UP_LOAD` skips them) and the next one still undo to the state before the first. `PROJECT` load takes no
  copy.
- **Saves while playing.** A flash erase silences the audio and stalls the sequencer for a moment. The
  device refuses its own saves while the transport plays ("STOP TO SAVE"); `PROJECT` save, `UP_PUT`,
  `UP_STORE`, `UP_ERASE`, `BACKUP_LIST`, `BACKUP_PUT` and sample BEGIN / WRITE / END / ERASE from the editor stop the transport
  first (`BACKUP_GET` does not: it answers rc 3 while the transport runs). If it does not stop within 100 ms, no flash operation starts: user-preset commands
  return rc 2, sample commands a nonzero rc, backup commands rc 3. PROJECT retains its existing reply shape
  (op, slot, used); a failed PROJECT save gets no reply, allowing the editor to report
  a timeout instead of confirming the previous used slot. The previous slot is kept.
  A pending PLAY or SONG start also prevents a device save.
- **Published samples.** SMP_END repeated with an identical committed header succeeds without
  another write or zone scan. A different header is rejected (rc 2) until SMP_BEGIN,
  so sounding sample voices cannot see their zone table change.
- **Safety.** `PROJECT` save, sample-slot commands, `UP_PUT` / `UP_STORE` / `UP_ERASE`, and the tagged preference writes below write flash, and only in
  Melodee's own storage; never the app or the update area.


## Orange Cat: the internal sample slot (commands 110–114, firmware 0.6.0)

One user sample slot in internal flash (firmware/src/smp_slot.c): 22 sectors at 0xB2000..0xC7FFF, taken from the
record store at the first boot of 0.6.0 (its records there are moved first; with no room for them the slot is not
there until the store has room). Each sector keeps its last 256 bytes erased (the update loader's tail scan), so the
slot holds 3,840 bytes a sector. Its data limit is a Felucca slot's: 81,408 bytes of IMA ADPCM, with Felucca's
512-byte header (480 bytes of fields, "FSMP", version 1, 1..16 zones, name, data length, the data's CRC-32 (zlib)).
The old commands 11–14 wrote at 0xA0000, over what is now the record store: they stay retired (no reply), and
`SMP_INFO` (15) still reports no slots, so an older editor never offers an upload. Every write stops the transport
first (100 ms, else rc 2), as user-preset writes do.

| Command | Request | Reply |
|---|---|---|
| 110 SLOT_INFO | — | state (0 no slot yet, 1 empty, 2 a sample), data length (3 × 7 bits, low first), zones, capacity (3 × 7 bits: 81,408), name (string) |
| 111 SLOT_BEGIN | data length (3 × 7 bits), 1..81,408 | rc, bytes written (3 × 7 bits). The sample there is gone from now on |
| 112 SLOT_WRITE | offset (3 × 7 bits), pack7 data (≤ 256 bytes) | rc, bytes written. The offset must be where the last write ended; nothing past the length |
| 113 SLOT_END | pack7 header (512 bytes) | rc, bytes written. The header's length and CRC must be the data's; then it is programmed (the commit) |
| 114 SLOT_ERASE | — | rc, 0. The slot empty, still the slot's |

rc: 0 done, 1 refused (no slot, a bad length or offset, a header that does not match), 2 the flash failed or the
transport did not stop. A write torn anywhere (power, a refusal) leaves the slot empty, never part of a sample.
`tools/fm1_sample_upload.py` builds a slot from WAV files and uploads it with these commands. Nothing in 0.6.0 plays
the slot yet (a sample engine is phase 9's).

## Orange Cat: DRUM-X (engine 9, firmware 0.9.0)

No command is new. `INFO` names engine 9 "DRUM-X" instead of "-" (the number of engines stays 20), `ED_DESC` and `ED_NAMES` give its
eight edit values, and its nine kits are its presets. An editor that reads the descriptors needs no change; one that keeps its
own table (web/editor.html does, and web/test_web.mjs compares it with the firmware's dump) gets the entry below.

| Value | Label | Range | Default | Meaning |
|---|---|---|---|---|
| P_E0 | MRPH | 0..127 | 64 | MORPH, patch A to B, every lane |
| P_E1 | TUNE | 0..127 | 64 | +-12 semitones (64: the kit's) |
| P_E2 | TONE | 0..127 | 64 | colour |
| P_E3 | DECY | 0..127 | 64 | decay |
| P_E4 | NOIS | 0..127 | 64 | noise |
| P_E5 | FM | 0..127 | 0 | FM strike with each hit |
| P_E6 | FOLD | 0..127 | 0 | wavefolder after each sound |
| P_E7 | DRV | 0..127 | 0 | soft clip of the kit's sum |

Presets 0..8: DRUM-X, 909 CLUB, 808 BOOM, LOFI DUST, INDUSTRIAL, ACOUSTIC, DUB ROOTS, TECHNO RAW, BREAKS; the preset number is
the kit. The eight lane levels are `P_LN0..P_LN7` as for DRUM (v5), and a DRUM-X track is a drum track for the step grid: the hits
of a step (v5) play their General MIDI notes on it, and `UP_GET` / `UP_PUT` kind 1 (a drum grid) is allowed for it. The editor's
drum pages (the grid and the levels) show for engines 9 and 10. A firmware without DRUM-X reports engine 9 as "-" and plays a
track that holds it silent; an old sound of PHYS (engine 9, not a drum) now plays as a DRUM-X kit.

## Orange Cat: the asset pack (commands 115-119, firmware 0.8.0; no library banks taken since 0.9.1)

SHAPES and MACRO read 57 constant tables (40,232 bytes) from a pack kept in the PACK area of the flash map
(storage.c: 10 sectors at 0x93000..0x9CFFF; until 0.9.0 the Prophet banks' five A/B pairs, since 0.9.1 the pack's alone: the user
preset library lives in 23 other sectors, `firmware/src/lib_group.c`). `tools/gen_pack.py` makes the pack
with the firmware (`orangecat-VERSION.pak`) and the pointer header the firmware is built with. The pack's first 512
bytes are its header: magic "OCPK" (0x4B50434F), version 1, flags, the table hash (u32 at 8), the length (u32 at 12),
the payload CRC-32 (zlib, at 16) and a CRC-32 of bytes 0..507 (at 508). The firmware accepts only a pack with its own
hash and length. Without one, SHAPES and MACRO are silent and the screen says PACK MISSING; the unit boots and
everything else plays.

A command a firmware does not know gets no reply, so an editor offers the pack only after a reply to 115. Every
write is refused while the transport runs (rc 2): stop it first, as for the sample slot.

| Command | Request | Reply |
|---|---|---|
| 115 PACK_INFO | - | state (0 none, 1 this build's pack, 2 another build's, 3 damaged), 0 (until 0.9.0: 1 when library banks 16..20 held presets; a pack takes none now), stored length (3 x 7 bits), stored hash (5 x 7 bits), this build's length (3 x 7), its hash (5 x 7), the area (3 x 7) |
| 116 PACK_BEGIN | length (3 x 7 bits), optionally a byte 0 or 1 (ignored since 0.9.1; older pages send it) | rc, bytes written (3 x 7 bits). The pack there is gone and the 10 sectors are erased |
| 117 PACK_WRITE | offset (3 x 7 bits), pack7 data (up to 256 bytes) | rc, bytes written. The first offset is 512; each is where the last write ended |
| 118 PACK_END | pack7 header (512 bytes) | rc, bytes written. The header must agree with the data and the build; it is programmed last (the commit) |
| 119 PACK_ERASE | optionally a byte 0 or 1 (ignored since 0.9.1) | rc, bytes written. The area erased; no preset is touched |

rc: 0 done, 1 refused (not this build's length, out of order, a header that does not match), 2 the flash failed or
the transport is running, 3 never given since 0.9.1 (firmware before it: library banks 16..20 hold presets and the take byte was 0; a
page sending take 0 keeps them), 4 the move of the old preset pairs into the library group has not finished (restart the unit;
BEGIN and ERASE only; the area may still hold an old pair). A pack takes no preset since 0.9.1: the editor asks nothing, and a
firmware before 0.9.1 answering rc 3 means the unit must be updated first. The Prophet bank objects are retired by the build
(refused for writing, no record on reads), so nothing can write a pack sector through them. A write torn anywhere leaves no header, so the pack reads as missing, never
half installed. The update loader erases a sector whose last 256 bytes look like one of its update records;
`gen_pack.py` lays the tables out so none does, and PACK_END tests the same rule.

## Orange Cat: engine modules (commands 120-124, firmware 0.10.0)

SHAPES can run from a module kept in the MODULE area of the flash map (12 sectors at 0xC8000..0xD3FFF) instead of from the firmware
image. A module is a built piece of code (`tools/build.py --module`, source `tools/module/shapes_mod.c`) that reaches the firmware only through
the services table `fw_api_t` (`firmware/src/fw_api.h`), which sits in the image at a fixed address. It has no RAM of its own. Its package is
`orangecat-shapes-VERSION-COMMIT.ocm`, with one image for each place the module can be linked for: SHAPES' module is 6 sectors, so it has an image for
start 0 (flash 0xC8000, runs at XIP 0x020C4000) and one for start 6 (flash 0xCE000, XIP 0x020CA000). The unit names the start to use, so a replacement
goes into a free place beside the old module when there is one, and in place only when there is not (0.10.1; before that it was always in place), and the page sends
that image.

The package (little endian): magic "OCMF" (0x464D434F), version u16 = 1, variant count u16 (1..8), interface identifier u32 (at 8), table address u32
(at 12), engine u32 (at 16), state size u32 (at 20), pack hash u32 (at 24), name 16 bytes (at 28, zero filled), table version u32 (at 44), table size u32
(at 48), sectors u32 (at 52); then per variant 16 bytes {start sector index, image length, CRC-32 of the image, file offset}; then a CRC-32 of everything
before it; then the images. An image's first 256 bytes are the header page (`fw_mod_hdr_t`, 60 bytes since 0.11.0, the rest zero), its code starts at byte 256; its CRC
(zlib) covers bytes 256..length (at 52 of the header) and the header's own CRC (at 56) covers bytes 0..55.

(0.14.1) **Format 2, the relocatable package** (what `tools/build.py --module` writes; since a relocated SHAPES was installed on the unit and measured equal to
the variants package's, the only format it writes: format 1 is still read, by the page, the card and tools/ocflash.py). The same 56 bytes with version u16 = 2 and the variant count 1; then one 16-byte row {0xFFFFFFFF
(relocatable: the image is linked at address 0), image length, CRC-32 of the image, file offset}; then 16 bytes {relocation count n (at most 2048), file
offset of the list, CRC-32 of the list's 4 x n bytes, start count m (1..8)}; then m rows of 8 bytes {start sector index, CRC-32 of the image that start
gets}; then a CRC-32 of everything before it; then the image, then the list. A list word is a site: bits 0..23 its byte offset in the image (the header
page is byte 0), bits 24..31 its kind, 1 a 32-bit little-endian word holding an address (ABS32), 2 a MOV_ABS32 instruction pair of 8 bytes whose 32-bit
constant sits in two little-endian halfwords at offset + 2 (low) and + 4 (high) (MOV32); the sites in strictly increasing order, each inside the image.
The image a start gets: every site's 32-bit value plus the start's XIP address (0x020C4000 + sector x 4096, carries included), then the header's two CRCs
as the build fills them (the code's, bytes 256..length, at 52; the header's, bytes 0..55, at 56). Its CRC-32 must be the row's for that start: the bytes
are then exactly the image the variants package holds for that start, and the commands, the unit's checks and what it programs are unchanged. The page
(web/fm1oc.js ocmParse and ocmRelocate, the same text as web/editor.html) relocates the image for every start the package names when it reads the file;
the card's installer relocates it as it reads the file for the start the unit names (card_install.c fb_rel_rd: the list in RAM, refused when longer than
2048, out of order, of another kind, a site outside the image, or when the relocated image's CRC-32 is not the package's). An older firmware's card
installer and an older page refuse format 2 ("Not a module"): it is the file's format that changed, not the unit's. tools/ocflash.py apply_relocs is the
reference; web/test_ocm.mjs, tests/ocflash_test.py and tests/card_install_test.c hold the three to it.

(0.13.4) Nothing the unit would refuse at 123 or 118 is sent: before 116 or 121 the page runs the unit's own checks on the file's bytes (web/fm1oc.js packProblem and
ocmImage, the same text as web/editor.html): for a pack its header (length, hash, header CRC), the payload CRC-32 and the update loader's record rule on every sector tail
(oc_pack.c pk_image_check); for a module variant the header page against the package and the start (interface, table address, engine, state size 1..16384, pack hash,
length, link address 0x020C4000 + sector x 4096, an aligned engine_t inside the code, the header CRC), the CRC-32 of the code and the record rule (oc_module.c
oc_mod_image_check). The page descriptor check of a device build stays the unit's. The unit still checks everything at the end, as before; the commands and replies
are unchanged. The card's installer (OCT+ on a .PAK, .OCM or .set.json) reads the whole selected file through the same two functions before its first erase.

(0.13.5) The reply to 116 and to 121 comes after the unit has erased (a module in place up to 16 sectors, the pack 10; the driver allows 500 ms a sector), so the page waits 16 x 500 ms + 2 s = 10 s for it (`OC_BEGIN_MS` in web/fm1oc.js and web/editor.html) and never repeats the command: a repeated BEGIN would erase again. The other choice, answering before the erase and reporting its result on the first 117 or 122, would change the protocol and what a write's rc means; the longer wait changes neither.

A command a firmware does not know gets no reply, so an editor offers modules only after a reply to 120. Every write is refused while the transport
runs (rc 2). The pack's commands 115-119 are unchanged (their BEGIN already carries an optional byte older pages send, so an area selector there would be
ambiguous); the module has ids of its own.

| Command | Request | Reply |
|---|---|---|
| 120 MOD_INFO | optionally an engine | starts (3 since 0.12.1: sector 0, sector 6, sector 4 of the area; SHAPES uses starts 0 and 1, the Prophet 0 and 2), sectors of the area (12), flags (1 SHAPES is built in, 2 a module takes the slot of a built-in engine, 4 an install is under way, 8 (0.10.1) the install the plan names is in place), table version, table size, interface identifier (5 x 7 bits), table address (5 x 7), pack hash (5 x 7), the start an install of the engine goes to (0, 1, or 127 none; 127 too without an engine: a free start first, else the start that holds the engine's module), then per start: state, why, engine, length (3 x 7 bits), then (0.12.1) the blocker: with an engine and no start that can take it, the start whose usable module of another engine is in the way (the install is refused: ENGINE X INSTALLED AT START N: REMOVE IT FIRST), else 127 |
| 121 MOD_BEGIN | engine, start, length (3 x 7 bits) | rc, bytes written (3 x 7 bits). Beside (flag 8 clear): only the 6 sectors of the free start are erased; the old module stays registered and playing. In place (flag 8 set): the engine is taken out of the table, the voices and arena state of the tracks that play it are cleared, the 6 sectors of the start (and of any other module of that engine) are erased, and a power cut leaves no module |
| 122 MOD_WRITE | offset (3 x 7 bits), pack7 data (up to 256 bytes) | rc, bytes written. The first offset is 256; each is where the last write ended |
| 123 MOD_END | pack7 header page (256 bytes) | rc, bytes written. The header must agree with the code just sent (its CRC), the start, and this firmware; it is programmed last (the commit); the area is then read as at boot and the module registered. Beside, the new start is read back and checked as the loader checks it before the old module is taken out of the table and its start erased (header sector first); if it fails the new start is erased, the old module stays and the reply is 7 |
| 124 MOD_REMOVE | engine (127: the whole area) | rc, bytes written. The engine's module out of the table and its sectors erased |
| 125 ENGINES | none (any byte is refused: no reply) | (0.11.0) the engine list: the number of engine numbers N (20), then for each number 0..N-1 eleven bytes: source (0 not available: retired, or a number only a module can supply with none registered; 1 built in; 2 a module), module state (the MOD_INFO states; 0 when no module was found for the number), why, start (0, 1, or 127 none), version (major, minor, patch, 3 bytes of 7 bits: the module header's `version` word 0x00MMmmpp; 0 for a built-in engine), length of the module (3 x 7 bits), tracks that hold the number (a count); then the number of starts and for each: state, why, engine (a torn start names no engine, so the page shows it from this part). A module that was found but not registered (banned, refused, a wrong tail) gives its state and start on its number with source 0 or 1; where two modules claim one number the registered one is told. The page builds the Engines table from this and the names of INFO, and offers Install on a number a module may supply (14) with none found, Replace and Remove on one with a module; a firmware without 125 never answers and the page keeps the two buttons of 0.10.x |

State: 0 empty, 1 torn header, 2 refused (another interface, another table address, a table the firmware does not have; `why` is the check's code: 3 identifier,
4 table too new, 5 address), 3 a header field out of range (link address, length, engine_ptr, engine number, state size, pack hash), 4 bad code CRC, 5 stopped
after a crash (below), 6 valid but the built-in engine keeps the slot (flag 2 clear), 7 registered, 8 (0.10.1) the code is good but a sector of it ends in 80 bytes
the update loader would take for one of its update records (`why` is the sector, counted from 1); not registered.

rc: 0 done, 1 refused (not the start named by MOD_INFO, a bad length, an engine a module may not supply, out of order, a header that does not match), 2 the flash
failed or the transport is running, 4 the move of the old preset pairs into the library group has not finished (restart the unit), 5 no start free, 7 written but
not readable as a module (install it again), 8 (0.10.1) a sector of the module would be erased by a firmware update (below); nothing was committed.

The update loader's record rule (0.10.1): after an update the loader erases any data sector between 0x93000 and 0xFC000 whose last 256 bytes, read as 80, have
0x5441 at bytes 6..7 and a first word that is not zero and equals the CRC-16 (poly 0x1021, init 0) of bytes 2..79. A module must have no such sector tail
(sectors counted from the start of its image, tails at 0xF00, 0x1F00, ...; flash after its end is erased and reads 0xFF). `tools/build.py --module` moves the code
on 4 bytes at a time until none does, the page tests the image it is about to send, MOD_END tests the flash before it programs the header page (rc 8), and the
loader tests it at every boot (state 8).

Checks at boot, in this order: magic, header CRC, `fw_module_check` (interface identifier, table address, table size and version), the link address (it is
linked for one start and refused at any other), the length (a multiple of 4, at most 6 sectors), the engine_t inside the code, the engine number (SHAPES, and from 0.12.0 the Prophet's 19),
the state size, the pack hash, then the CRC of the code, then (0.12.0) the page descriptor if the engine_t has one (state 3 when a pointer leaves the image), then the sector tails. A write torn anywhere leaves no header, so the area reads as empty. Two valid modules for one number: the
higher header version is used (equal versions: the higher start) and the other is erased at that boot (0.11.1). Power cuts of a replacement beside (tested at every program and every erase): before the new header page is programmed the old module is the one that loads; once
the old start's first erase has run it is the new one; between the two (the commit done, no erase yet) both are valid and the newer is used (0.11.1). The engine
number a track holds is never changed by an install or a removal; the table answers for the number (the built-in engine if there is
one, else silence).

Crash mark: a module that is playing in the first 30 s of a boot that then dies is not loaded at the next boot (state 5) until the unit loses power or the
start is installed or removed. (0.10.1) A crash later than that is counted too. A flag in noinit RAM says a session is running; every orderly reset (UBOOT, an update, a test reboot) clears it and the count. A reset that finds the flag still set is a session that died (the chip's reset reason cannot tell: a fault ends in a soft reset too). If the module's engine was held by a track in that session, the start's count goes up; a death without it, a clean reset, a power loss, an install or a removal sets it to 0. Three in a row ban the start (state 5) until power loss, install or remove, and the home screen says MODULE BANNED with the engine name, once.

## Orange Cat: pages from a module (commands 126-127, firmware 0.12.0)

A module engine may bring its own pages: `engine_t.pages` points at a descriptor in the module's flash (`firmware/src/fw_pages.h`): up to 24 page rows, up to 128 native
parameters (one byte each of the track's native block; the engine's motion events are `MO_NATIVE + index` as for the built-in Prophet), hooks, and the lists the ENV and LFO
tabs use. The unit answers from the descriptor in place. A built-in engine, a number with no module and a firmware before 0.12.0 give no descriptor (no reply from an old
firmware: the command is unknown). The Prophet's number (19) is a module slot from 0.12.0; no Prophet module exists yet (docs/orangecat/ENGINE_LOADING_PLAN.md, "The Prophet as a module").

| Command | Request | Reply |
|---|---|---|
| 126 PAGES | engine (exactly one byte; a number past the table, or any other length, is refused: no reply) | engine, source (0 no descriptor, 2 a registered module's), page count (0..24), panel count (0..128); then per page its title (ASCII, up to 8, 0 ended) and four panel indices (127 = no parameter in that column) |
| 127 PANEL | engine, optionally the first entry (2 x 7 bits) | engine, first (2 x 7 bits), count (as many entries as fit the 1024 byte reply, at most 127), then per entry: present (0 a hole in the panel, nothing more; 1), format (the `F_` number), min, max, default (14 bit values, 2 bytes each, offset 8192 as everywhere), label (up to 8, 0 ended), unit (up to 8, 0 ended, empty if none), motion skip (1 = motion leaves this parameter), and for a format 8 (F_ENUM) the names of min..max (up to 8 each, 0 ended); then the panel count. The editor asks again from first + count until it has the panel count. |

A page's value is the byte of the track's native block at its panel index; the Prophet module's block is the Prophet patch. The editor shows the titles of a module's pages in
the Engines table (`fetchModPages` in web/editor.html: both commands, in parts). Setting a value on such a page is not a new command: the Prophet's native commands (95) and
TRACK_PARAM are the ways in once the module exists; the descriptor says what exists.

The Engines table (0.12.0) also has, per row for an engine the unit plays: **Use** on a track (the page selects the track, sends PRESET with the engine and preset 0, which is the
track engine command, and selects the track that was selected; the unit's admission may refuse it with the fourth reply byte, 1 engine too heavy, 2 no room, and the track keeps
its engine); and **Replace** with a file of another engine (a swap): the page checks the file against the unit, asks, sends MOD_REMOVE for the old engine, reads MOD_INFO for the new
engine (the freed start is the plan) and sends MOD_BEGIN, MOD_WRITE, MOD_END as for an install. The tracks that held the old engine keep its number and are silent.

## Orange Cat: set manifests (0.13.3)

No SysEx command. The card browser's "Install set" reads a `*.set.json` from the card. The format, the checks and the messages are in docs/orangecat/SET_FORMAT.md.

## Pattern banks (commands 73–74)

INFO sends 0 in the former untagged CHAIN_ROWS byte, then retains the tagged UI, motion and backup
capabilities. `50 01 08 10` announces eight patterns per track and 16 bank-song rows; FM6's tag follows.
Editors must use the tag, not infer support from a version string. Older firmware retains command 33's
project-based SONG behavior; its historical format is documented below.

| cmd | Request | Reply |
| --- | --- | --- |
| 73 PATTERN | track 0..3, op 0 query; op 1 select, bank 0..7; op 2 copy, source 0..7, destination 0..7 | track, op, rc, active bank, queued bank (127 none), bank count 8 |
| 74 BANK_SONG | op 0 query; op 1 set, count 0..16, count × (T1 bank, T2 bank, T3 bank, T4 bank, repeat 1..16); op 2 start; op 3 stop | op, rc, count, running, row, remaining, count × (four banks, repeat) |

rc 0 succeeds, 1 rejects invalid values or a blocked action, 2 means busy or motion capacity exceeded.
Bank-song rows validate completely before replacing the arrangement. Pattern selection queues at that
track's loop end while playing; selecting the active bank cancels its pending change. STOP applies queued
choices. Copy retains four-note chords, ties, drum data, chance, timing and motion. Copying into the active
bank requires STOP; SONG blocks bank changes/copies. All pattern IDs on the wire are zero-based.

BANK_SONG switches all four tracks at track 1's loop boundary, preserves the current sounds and mix,
pauses recording and sequence edits, and restores the original bank selections on STOP or the song's end.
Each bank has LEN/DIV/SWING/GATE. Automation has a project-wide limit of 64 events; events belong to a
track and bank, while the ON switch remains track-wide. WATCH sends RELOAD when a bank changes.

Projects and runtime backups are FBK9 (`46 42 4B 39` in byte order, size 20224). Header words are magic
0x394B4246 and size; a complete 3584-byte FUN8 record follows at byte 8. Byte 3592 holds four active bank
IDs. Seven other banks per track follow, with the same nine-byte packed steps as FUN8. Then come 32 sets
of four int16 timing values, 16 × four SONG bank assignments, and 64 motion bank tags. Then (byte 20108) the
tracks' FM6 function settings: `46 4E 36 31` ("FN61") and 4 × 16 bytes, track 1 first, in the order of the
device's FM BEND / PORTA / WH/FT / BR/AT pages (bend up, bend down, step, portamento mode, time, glissando, wheel
range, target, foot, breath and aftertouch the same, Dexed velocity, ENGINE 0 MODERN 1 MARK I 2 OPL); all zero in a
record saved before them: Dexed's defaults. Remaining bytes are reserved; the final uint32 is FNV-1a over every
preceding byte. The inner motion record's reserved byte 0
is 1 when bank tags accompany it. Duplicate (place,param) events are valid on different banks only.

Library (0.9.1): the 22 user preset banks are no longer 22 A/B pairs but one group of 23 sectors with a shared spare
(`firmware/src/lib_group.c`): a sector's commit record names the group (type 0x100), the bank (slot 0..21) and that bank's
save counter; a save goes to the free sector, header last, and the sector the bank left is the new spare. The ids of the
inventory (command 4's `lab` list, 31 objects) and of BACKUP_LIST stay the old object numbers; a library bank answers from the
group. A backup made by 0.9.0 or earlier restores into the group unchanged; firmware before 0.9.1 cannot find banks that
0.9.1 moved: back up before installing it.

Flash objects 8..11 have two five-sector copies each at 0xA0000..0xC7FFF; CRC-verified payload precedes
its header-last commit. The old single-sector project objects remain readable for migration. Old FUN8,
FUN7 and older supported single-pattern projects become bank 1, with other banks empty and the old
project-based SONG cleared. USR1–3 are removed; sample commands refuse uploads/erases and SMP_INFO
reports zero slots. Recorded material and SAMPLE/GRAIN/SLICE are removed. DRUM provides only synthesized 808.

Current BACKUP_LIST has nine objects: 0 runtime (FBK9), 1 settings/template, 2..5 saved projects (FBK9 or
an older format before first save), 6..7 original user presets, 8 retired FM6 bank, 17..18 expanded user records and 19..20 owned FM6 voices. PUT accepts 20224-byte FBK9, 3584-byte
FUN8 and 3388-byte FUN7/FUN6 project records; saved legacy restores become FBK9. Full validation precedes
publication. The web restore checks the target inventory before writes; nonempty samples cannot restore
onto this firmware. Empty sample objects in an older archive are skipped.

## Historical v6: project-based song chain

INFO appends CHAIN_ROWS (16) after NTRK. Earlier INFO bytes and command numbers
stay where they were. No trailing byte means no SONG command: hide its controls.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 33 SONG | op 0 query; op 1 set, count 0..16, count × (slot 0..3, repeat 1..16); op 2 start; op 3 stop | op, rc, count, running 0/1, row 0..15, remaining repeats, count × (slot, repeat) |

A set must have exactly `2 + count × 2` argument bytes. rc 0 = accepted,
1 = invalid rows (or an empty chain on start), 2 = busy, 3..6 = source slot 0..3
is empty. Rejected sets keep every previous row. Set edits RAM; PROJECT save
persists it, PROJECT load recalls it. A stop/start is acknowledged before the
next audio block; query reports the actual running state. `row` and `remaining`
are meaningful only while running. Rows always start from 0, with track 1's
loop as the repeat and transition boundary; the last row stops.

A row takes all four tracks' steps (with their chance) and LEN / DIV / SWING / GATE from its saved
project slot, retaining the current sounds, mix, ARP, scales. The
four sources are copied before starting, with no flash operation in playback.
Original editable patterns and timing are restored on stop. Sources are shared
project slots; overwriting a slot changes its uses on the next start.

STEP_GET / TRACK_STEP and WATCH report the playing source. During chain
playback, STEP_SET / TRACK_STEP writes, SET / TRACK_PARAM writes to
LEN / DIV / SWING / GATE and MOTION edits (rc 3) are ignored, returning the current value. Other
sound and mix parameters remain editable. Recording and panel pattern edits
also require STOP. Older editors keep working; older firmware gets no SONG
requests from the new editor.

Projects now write FUN7 (below), the same 3388 bytes in the same A/B sectors. FUN6 has the same size;
FUN5 (3352 bytes) and FUN1–FUN4 convert with an empty chain, without changing the sample-slot layout.
A chain row also plays the motion of its source project (events for an FM operator parameter are skipped
when the track's engine is not the saved one).

## v7: chance, motion, MIDI clock

- **Chance.** Each step has a chance, 0..100 %, as the last byte of the step reply (and the optional last byte
  of a step write). 100 is the default and what every older pattern holds; 0 means the step never plays. The
  device rolls once each time a step comes round, for all of it: its notes, its drum hits and a TIE. A failed
  roll plays nothing and releases what rang before. Chance is saved in projects (FUN7) and in song rows; a
  user preset does not store it. Changing it on the device pushes `STEP_CHANGED`.
- **Motion** is knob moves recorded per step: 64 events shared by the four tracks. An event is (track, step
  0..63, parameter id, value). While a track plays its motion, the step sets the value at the step and it
  holds until another step changes it; the loop restarts from the sound's own value, and stopping puts the
  sound's own values back. Parameters that can be recorded (`motion_param`): ids 0..16 (LEVEL, ENV, LFO),
  38 (GLIDE), 39 (PAN), 44 (DETUNE), 61..80 (the FM operator parameters) and
  83..90 (the engine parameters; not the chord keys 81, 82). The device records them while the track is armed, playing and selected, from its knobs
  and from `SET` / `TRACK_PARAM` alike.
  - `MOTION` with the track alone is the query. `on` 0 keeps the data and stops playing it; 1 plays it. Clear
    drops the track's events and turns it off; on the device it is undone by SAVE held.
  - Set: `value` must be inside the parameter's range for that track's engine (and −64..127); a
    new event when the 64 are used gets rc 2 ("MOTION FULL" on the device), an event that already exists is
    updated. A set turns the track's motion on. A delete of an event that does not exist succeeds.
  - rc: 0 ok, 1 invalid (step ≥ 64, an id that cannot be recorded, a value outside its range), 2 full, 3 a song is playing.
    A track ≥ NTRK, `on` above 1 or any other length gets no reply.
  - Events come back in the order they are stored. The device does not push motion changes: poll `MOTION`
    (the web editor does, while the sequencer tab shows).
  - `DUMP`, `TRACK_DUMP`, a user preset store and a project save give the sound's own (base) values;
    `GET`, `SET` and the pushes give what is sounding now, which differs while a motion event is in effect.
  - A sound or pattern load, a pattern clear and INIT SOUND on the device drop the track's motion; the
    device's UNDO brings it back. `MOTION` clear does the same through the same undo copy.
- **Projects (FUN7).** 3388 bytes, little endian, in the same A/B sectors as before: `46 55 4E 37` ("FUN7"),
  size u32 (3388), the 27 globals as i16 (bytes 8..61), sel, parts, phys (62..64), P_COUNT as stored (byte 66),
  then from byte 68 for each of the 4 tracks: P_COUNT bytes (value + 64), engine, preset, 64 steps of 9 bytes
  (4 notes; n | time << 3 | flags << 5; vel; hit; acc; chance byte where 0 = 100 %, 1..100, 101 = never);
  then the song chain, then the motion (260 bytes: count, on mask, 2 reserved, 64 × (track << 6 | step, id,
  i16 value)); the reserved tail is zero up to byte 3371; bytes 3372..3383 are the project's name (since
  1.0: ASCII 32..126, upper case, 0-padded; all zero = no name, as firmware before wrote them; a byte
  outside 32..126 reads as no name, it never refuses the project); the last 4 bytes are an FNV-1a hash of all
  before (the name included). The device names projects itself (SAVE > PROJECT, NAME); the editor's PROJECT
  save keeps the current name, and backups carry it as part of the 3388 bytes. FUN6..FUN1 load with no name
  and are bounded to today's ranges. A count smaller than P_COUNT is mapped as for user presets, and so are the
  motion events' ids: a FUN7 of 89 parameters has its engine parameters' events at 81..88, which load as 83..90
  (the ids below its P_E0 stay). 68 + 4 × (91 + 2 + 576) + chain + motion = 3040 bytes: 332 to spare.
- **Projects (FUN8, with FM6).** FUN7 laid out the same way, `46 55 4E 38` ("FUN8"), size 3584: the data up to
  byte 3040 as above (16 bytes to spare), the four tracks' FM6 patches as packed 128-byte records (see "FM6
  patches") at bytes 3056..3567, the name at 3568..3579, the hash last. FUN7 (3388) and older load with the
  init patch on every track. Backups, `PROJECT` and the editor's project files carry the 3584 bytes.
- **MIDI clock** has no SysEx. `G_CLOCK` selects the source: 0 INT, 1 USB, 2 TRS. With 1 or 2 the sequencer
  steps on that port's Clock pulses (the other port's are ignored), Start (0xFA) restarts from step 0,
  Continue (0xFB) resumes and Stop (0xFC) stops; BPM follows the incoming tempo (40..240), and 500 ms
  without a pulse stops the transport. Changing `G_CLOCK` stops it. (Orange Cat 0.14.2) The clock coming back after
  such a stop resumes the transport in time with the beat: on the pulse 1 + 24 k after the last one before the loss
  (the pulses missed reckoned from the time and the tempo), from the place that next pulse would have had, nothing
  missed played. A Start, Continue or Stop, a local PLAY or STOP, a change of `G_CLOCK` or an outage over 60 s ends
  the wait (`firmware/src/midi_clock.c` midi_clock_resync; `tests/midi_control_test.c` resync_test).
  `103 CLOCK_STATS` reads the clock's counters: request `[flags]` (1: reset after reading); reply pulses taken from the
  selected source (5 x 7 bits), stops on a lost clock, resyncs, the longest gap between two pulses in ms, and the
  pulses the last resync found missed (3 x 7 bits each). A firmware without it does not answer.
- Pitch bend, sustain (CC64), RPN 0 (bend range, ±0..24 semitones), CC120 / 121 / 123 are MIDI only and
  have no parameters, protocol or saved state.

## Historical v7: full backup (65-67)

INFO advertises `42 01 caps`: bit 0 = `BACKUP_LIST` / `BACKUP_GET` (read), bit 1 = `BACKUP_PUT` (restore);
this firmware sends 3. Requests name objects, never flash addresses.

| id | object | size |
| --- | --- | --- |
| 0 | runtime: the music being played now, as a FUN8 project (firmware before FM6: FUN7, 3388) | 3584 |
| 1 | settings (palette, retired speaker padding, HOLD time, favorites, panel calibration, USB audio devices, BOOT, CLK TUNE MIDI ROUT, ...), then the template (SAVE > PROJECT, SLOT TMPL) when one is saved; PER5 only (2.0: older records restore on 1.2), a tagged palette id | the settings record's size: 604, or 1924 with the template |
| 2..5 | PROJECT slots 1..4 (FUN8) | 3584, or 0 if empty |
| 6, 7 | user preset banks (slots 1..16, 17..32) | the bank's size, or 0 if empty |
| 8 | retired FM6 bank; a payload is refused at begin | 0 |
| 17, 18 | user records U33..U48, U49..U64 | 3816 or 0 |
| 19..22 | native FM6 voices F033..F064, F001..F032 | 2060 or 0 |
| 32..34 | user sample slots 1..3: header (512 bytes) then ADPCM data | 512 + data length, or 0 if empty |

Reading: `BACKUP_LIST` (no arguments) stops the transport, then takes a snapshot of the runtime object and
answers `1, rc, count` (12 with FM6, 11 before) and, per object in the order above, `id, size u32, crc u32` (CRC-32, zlib). The other objects are read as
they are in RAM or flash. Then `BACKUP_GET` reads an object in pieces: `id, offset u32, count lo, count hi`
(count 1..256, LSB first 7 bit pair) answers `id, rc, offset u32, count lo, count hi` and the data as pack7. Check each object's CRC
against the list; if it differs the device changed, so start again.

Restoring: `BACKUP_PUT` takes ids 0..8 (the samples are written with `SMP_BEGIN` / `SMP_WRITE` / `SMP_END`,
or `SMP_ERASE` for an empty slot). Begin: `0, id, size u32, crc u32`: size is 3584 (FUN8) or 3388 (FUN7, FUN6) for id 0,
the settings record's size for id 1, 3584, 3388 or 0 (empty the slot) for ids 2..5, the bank's size or 0 for 6 and 7,
0 for 8 (the retired FM6 bank). A size the firmware does not read is refused at begin (rc 1), so the web
editor begins and aborts every object first and refuses the whole archive before any write. Data: `1, id, offset u32, pack7` with the next offset (they must follow each other)
and at most 256 decoded bytes. Commit: `2, id`: the device checks the length and the CRC, validates the
content, and then writes. Abort: `3, id`. Id 0 replaces the music now playing (RAM only, no flash);
ids 1..7 are written to flash (settings and presets are applied too). Nothing is written before the commit.
The web editor sends ids 2..7 and the samples first, then 1, then 0 last. A failed restore can leave
earlier objects restored; the file is still the source.

| rc | Meaning |
| --- | --- |
| 0 | ok |
| 1 | invalid: arguments, id, size, offset, count, or an id `BACKUP_PUT` does not take |
| 2 | validation failed: CRC, length or content at commit (and a runtime object that cannot be packed) |
| 3 | stop playback first (`LIST` and `PUT` stop it themselves, and answer 3 if it does not stop in 100 ms; `GET` answers 3 while it plays) |
| 4 | flash write failed |
| 5 | stale: no `LIST` yet, the USB bus was reset, 15 s with no `PUT` request, or a project save / load reused the staging RAM (for `GET`, of id 0 only). Start again with `LIST` / `BACKUP_PUT` begin |

`BACKUP_LIST` answers `1, rc, 0` when rc is not 0. `GET` and `PUT` answer id 127 when they got no arguments.
A `LIST` replaces the snapshot, and a `PUT` begin ends it: a `GET` after a begin gets rc 5.

### Orange Cat: the arrangement objects (ids 64..84, firmware 0.14.2 on linux-hw)

A project as ids 0 and 2..5 carry it (Melodee's FBKK) has no room for Orange Cat's lists (the 64 chains, the 64-row song, the 8 scenes,
each track's groove: `pattern.c` `ocl_lists_t`) nor for which pool phrase each bank is, and the phrase pool was no object, so a restore
brought every project back with a new project's chains, song and scenes (shown on the unit, October 26, 2026: hw/LOG.md). These objects
are the record store's own records (`firmware/src/oc_store.c`), read and written as they are:

| id | object | size |
| --- | --- | --- |
| 64..67 | slot 1..4's project record (`OCS_PROJECT` 0..3, "OCJ3": its banks' phrases, its lists, its project part; `oc_project.c`) | its length, or 0: no record |
| 68..83 | the pool's chunk 0..15 (`OCS_POOL`: four phrases, a table of four 16-bit lengths then the phrases; `oc_pool.c`) | its length (up to 45,696), or 0 |
| 84 | the music loaded: its lists (`ocl_lists_t`, 2,396 B) then each bank's phrase + 1 (`bank_pid`, NTRK x 32) | 2,524 |

`BACKUP_LIST` with the argument `1` (after a plain `LIST`, in its session: else rc 5) answers `1, rc, 21`, then per object
`id, size u32, crc u32` as above. A plain `LIST` is unchanged (29 objects), so a page that knows ids 0..28 sees what it always did; a
firmware without these objects answers `LIST 1` with `1, 1, 0`. `GET` reads them as the others. `PUT` takes them: a record (64..83) is
written as its pieces arrive, straight into free sectors of the store (no RAM image: a chunk can be 45 KB); the commit checks the length,
the whole CRC and the record's form (a project record's head, a chunk's table) and only then writes the commit headers, so an abort, a
stale session (USB reset, 15 s, a project save between) or a bad CRC leaves the record as it was. Size 0 deletes the record (as an empty
slot, id 2..5, does). Id 84 is staged and taken whole (its values in range: a phrase past 64 refuses it, rc 2). After a record, the pool
and the slots are read again.

**Order.** Restore them after ids 2..5 (which write the slots through the pool) and id 84 after id 0 (the music restored takes a new
project's lists): the web editor sends 2..28, 64..83, the samples, 1, 0, then 84. The slots, the pool and the music's arrangement are
then the backup's byte for byte (`tests/backup_test.c` `arrangement_archive`; `web/test_backup.mjs`). An archive holding them (50
objects: ids 0..28, then 64..84) is refused by a firmware without them before any write. Phrase numbers are the unit's: the pool comes
with them, so a backup restored onto another unit brings its own phrases.

## FM6 patches (68-71)

The FM6 engine (12) plays one 6-operator voice per track. Its EDIT values are macros
(ALG, FB, MLVL, MRAT, MEG, VMOD, DTUN); the eighth value is unused (0..0).
The main knobs control MLVL, MRAT, MEG and DTUN. Factory voices load through the preset
browser; user presets retain the actual voice. INFO advertises `46 01 18 00` (24 factory, zero bank slots).

A patch is the 128-byte packed record of the generic 6-operator voice (the 32-voice bank's record; every byte is
7-bit, so it travels as it is, no pack7). Operators come sixth first: per operator 17 bytes (R1..R4, L1..L4,
break point, left / right depth, curves `LC | RC << 2`, `RS | DET << 3`, `AMS | KVS << 2`, output level,
`MODE | FC << 1`, fine), then pitch EG rates and levels (102..109), algorithm 0..31 (110), `FB | OKS << 3`,
LFO speed, delay, PMD, AMD, `SYNC | WAVE << 1 | PMS << 4`, transpose (24 = none), the name (10 ASCII bytes).
The device stores every value clamped into its range.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 68 FM6_GET | target, index | target, index, rc, then (rc 0) the 128 bytes |
| 69 FM6_PUT | target, index, the 128 bytes | target, index, rc (target 0, a track: that track's patch from now on) |
| 70 FM6_LIST | — | nfactory, nbank, then per slot (factory first): used (0/1), name string ("" if empty) |
| 71 FM6_ERASE | bank index | index, rc |

Targets: 0 track (index 0..3), 1 retired bank (GET/PUT always rc 3),
2 factory (index 0..23, GET only), 3 user preset voice (index 0..63).
Target 3 GET returns rc 2 for a preset without a stored FM6 voice. Target 3 PUT
requires an existing FM6 user record and writes its voice (every value into its range) to native slot k. Library
transfers send UP_PUT first, then FM6_PUT target 3. UP_STORE saves both automatically.
FM6_LIST lists only factory voices; FM6_ERASE always returns rc 3 for the retired bank.
Erase a user sound through UP_ERASE. Track PUT replaces the track voice.
Other rc values: 0 success, 1 bad arguments, 2 empty voice or flash/transport error.

The web editor (6-OP FM tab) reads and writes these, and imports / exports the generic SysEx files of the
format: a single voice `F0 43 0n 00 01 1B`, the 155-byte unpacked voice, checksum, `F7` (163 bytes), and 32
voices `F0 43 0n 09 20 00`, 32 x 128 packed, checksum, `F7` (4104 bytes); the checksum is the two's complement
of the data's sum, 7 bits. Raw 155 / 4096-byte files are read too.

The device itself also takes these as MIDI SysEx on its USB-MIDI port (any channel `n`), so Dexed or a DX7
librarian can edit a track live: a single voice replaces the FM6 track's patch (the selected track when it plays
FM6, else track n + 1, else the first FM6 track; the notes stop, as a DX7 program change). A 32-voice dump
replaces native slots F001–F032 when EDIT > STORE > SLOT is F001–F032, or F033–F064 when SLOT is
F033–F064. Stop playback before importing a bank. Its first voice is loaded onto the FM6 track, if one
exists; all 32 voices can then be browsed as regular FM6 user presets. No web editor is needed.
A voice parameter change `F0 43 1n gg pp dd F7` edits one byte of
the patch (pp + 128 gg; 155: the six operator switches, OP1 = bit 5), a function parameter change
`F0 43 1n 08 pp dd F7` sets the FM6 function settings (64 mono, 65 bend range, 66 step, 68 glissando, 69
portamento time, 70..77 wheel / foot / breath / aftertouch range and target: that track's own, saved with the
project and the template), and the dump requests `F0 43 2n 00 F7` / `F0 43 2n 09 F7` answer with the track's
voice / the 32-slot bank selected by STORE > SLOT. Both replies use the request's channel `n`. Empty bank
slots export as INIT voices. EDIT > STORE > SEND sends the current track's single voice on its MIDI
channel (track 1–4 = channel 1–4). SysEx output uses USB; the TRS jack remains input-only.

Incoming bank bytes are preserved exactly in the native preset pool. Checksums, lengths and 7-bit
framing are validated before import. Flash commits each 16-voice object separately: a failed object
retains its previous patches. If only the first object committed, the device reports `FM6 BANK SAVE ERROR`
and `16 PATCHES SAVED`; resend the bank after resolving the storage failure. Builds with flash disabled
report `FM6 BANK IN RAM`, so those imported presets do not survive power-off. A flash-enabled build
refuses the import if flash is unavailable.

## Tagged device preferences v1

INFO appends `0x55, 1, uiCaps` after the existing `NTRK, CHAIN_ROWS` bytes.
Only this complete tagged extension enables commands 34–38. An untagged byte
from an experimental firmware is not a SONG or preference capability.
Command 33 remains SONG. Older editors can ignore the additional INFO bytes.

`uiCaps`: bit 0 palette, bit 1 font weight (retired: firmware since the 1.0 UI has one
weight and no longer sets it), bit 2 reserved for a MIDI monitor, bit 3 favorites, bit 4 recording preferences, bit 5 layer/screen preferences. This build
advertises 57. Layer preference 9 is retired; screen preference 10 remains.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 34 UI_STATE | none | caps, palette, font, monitor, filter, favoriteSig u28, bankSig u28, [click, clickLevel, countin, preview, chordAdd, retired=127, screen] |
| 35 UI_SET | id, value | rc, id, value, UI_STATE payload |
| 36 UI_PALETTES | none | count, count × name string |
| 37 FAV_GET | engine, start v14, count 1..32 | rc; on success: engine, start v14, count, count × on/off |
| 38 FAV_SET | engine, preset v14, on/off | rc; when applied: engine, preset v14, on/off |

UI_SET ids: 0 palette (0..count-1, the order of UI_PALETTES: MONO GREEN AMBER ICE VIOLET ROSE PAPER
HI-CON), 1 font (retired: rc 2), 2 reserved, 3 preset filter (0 all, 1 favorites). Unsupported state
fields are 127. With capability bit 4, UI_STATE appends five bytes after its
13-byte prefix. UI_SET ids 4 click (0 OFF, 1 REC, 2 ON), 5 click level (0 LOW,
1 MID, 2 HIGH), 6 count-in (0 OFF, 1 BAR, 2 BARS), 7 note preview (0/1), and
8 chord entry (0 HOLD, 1 ADD) all share capability bit 4. Older clients may
ignore the suffix; newer clients require both the bit and the full suffix.
Both u28 signatures are four least-significant-first 7-bit bytes; compare them
to refresh changed favorites and user slots. Factory references use stable
engine/preset ids; engine NENGINES denotes a user slot. An empty user slot
cannot be marked. Factory DRUM sounds use the same favorite path.

rc 0 = applied and saved; 1 = invalid arguments; 2 = unsupported feature;
3 = applied in RAM but not saved (no flash or a failed write);
4 = applied and queued to save after STOP. A failed settings write is retried
by the same persistence path as the panel. These changes never stop playback.
Unchanged writes do not erase flash. Replies echo preference ids/values and
favorite ranges so the editor rejects replies to a different request.

## USB audio diagnostics

Firmware built with USB audio (`MELODEE_USB_AUDIO`, the default) answers command 72 with its USB audio counters;
without it there is no answer. `tools/usb_audio_stats.py` reads them.

**Schema 8, the Audio readout (Orange Cat 0.13.6).** Request flag 64 (alone: the other flag bits are ignored with it) asks for 23 counters, each 5 x 7 bits, LSB first,
reply schema 8. Fields 0 to 19 are the base counters above (16 late renders, 18 worst render in us since the reset, 19 load in 1/256). Field 20 is the block in
samples (HALF_FRAMES), field 21 the block budget in us (the time the block lasts, 128 samples at 44.1 kHz is 2,901 us), field 22 the blocks rendered since the
reset. Flag 65 (64 + 1) also starts the maxima, the late count and the block count over. No new flash: the numbers were already kept. The web editor's Settings
tab shows them as "Audio" and asks once a second while the tab is open; `tools/usb_audio_stats.py --budget` reads the same.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 72 AUDIO_STATS | — or 1 (start new maxima after this reply) | schema (2), then 20 counters, each 5 × 7 bits, LSB first: play alt, capture alt, play rate, capture rate (Hz), play fill, capture fill (frames), play underruns, play overruns, capture underruns, capture overruns, bad packets, packets received, packets sent, missed USB frames, longest gap between services (µs), longest service (µs), late renders, the feedback (10.14), longest render (µs), CPU (Q8) |


## Orange Cat: the flash read-path probe (command 109, OC_SFC_PROBE builds only, 0.13.7)

A hardware experiment's command, compiled only with `OC_SFC_PROBE=1` (core.h; default 0: no reply). Read only: nothing is written to flash.
It reads 2 KB at a flash offset first through the plaintext XIP window (as the firmware reads the pack and the modules) and then through
another path, timing a cold and a warm pass of each (TIMER4, 24 MHz), and returns the first 32 bytes of each. Interrupts are off for the
probe; the window is put back as `fl_plain_window_init` sets it and LENC is cleared (hal/fm1_flash.h `fl_probe_paths`). Stop the transport first.

| cmd | Request args | Reply args |
| --- | --- | --- |
| 109 SFC_PROBE | mode (0: the window's low end moved past the range, so the normal decrypting XIP path; 1: LENC_L / LENC_H set over the range, the window as it is; 2: as 1 with SFCENC_CON bit 2), flash offset (3 x 7 bits, 0x93000 .. 0xFF000, rounded down to 32) | mode, the SFCENC KEY register (3 x 7 bits), then for the plaintext path and for the other path: 32 bytes (each two bytes, the low 7 bits then the top bit) and the cold and warm ticks (5 x 7 bits each). 127 alone: a bad request; 126 alone: an offset out of range |

The PC compares the XOR of the two reads with the keystream of `tools/fm1pkg_make.py sfc()` (key 0x980F, the offset from flash 0x4000):
equal means the hardware decrypts that address the way the package build encrypts the app.

## Orange Cat: the hardware test surface (command 108, OC_HW_TEST builds only, 0.13.8)

**Test builds only.** `OC_HW_TEST` is 0 in every published build: there command 108 has no handler and gets no reply. A test build
(`tools/hw/ci_step.py build --switch OC_HW_TEST=1`) answers it on USB and also takes it from the TRS MIDI input (only this command;
every other SysEx there is ignored as always), so a test can drive the unit while its USB port is busy with something else (the card
tests). Source: `firmware/src/oc_hw_test.c`.

The keys go through the real input path: a pressed id counts as a closed contact in the panel scan (`hal/fm1_input.h`, `FM1_INPUT_FORCED`),
so the debounce, the hold times and the UI see what a finger makes. Knob steps are added where the encoder decoder leaves its detents.

| Request (after 108) | Action |
| --- | --- |
| `0` | query |
| `1, id` | press key id and keep it pressed (0..13 buttons, 14..40 note keys: `FM1_KEYMAP`) |
| `2, id` | release key id (127: every key) |
| `3, knob, s lo, s hi` | turn knob 0..6 by s detents (14 bits, offset 8192; + = clockwise) |
| `4, page` | select page (an index of `PAGES` the UI shows now; 127: HOME) |
| `5` | say state (below) |
| `6, track, step` | the state, then the step's recorded notes in NOTES' order: count, and for each: note, velocity, onset (3 x 7 bits, 65,536 a step), duration (3 x 7 bits), its exponent (length in steps = duration << exponent / 65,536) |

| 108 HW_TEST | (as above) | op, rc (0 ok, 1 a bad request or a page not shown), held keys (6 x 7 bits, ids 0..40), page (127: HOME), engine of the selected track, selected track, notice code (2 x 7 bits), the last notice's text (0 ended), then the panel's map: the button id of each printed label (14 bytes, FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY REC OCT- OCT+) and for each knob role (SELECT ALGORITHM PRESETS KNOB 1..4) its encoder and 1 if clockwise is + (14 bytes), the page's title (0 ended; HOME), the step cursor, the tracker's column (0 N1 .. 5 FX) and its picked effect (0 RATCHET, 1 CHANCE, 2 CONDITION, 3 NUDGE, 4 SLIDE, 5 ACCENT), the picked effect's value on the step a tracker edit at the cursor writes, as the tracker shows it (0 ended: "x4", "FLAM", "+16", "50%", "ON", "OFF"), and that step's row (a recorded step's notes: the row they are shown on) |

**Say state.** The output becomes a tone sequence (the music is replaced while it plays, about 0.6 s): a marker of 1,760 Hz for 120 ms,
then five digits, each 20 ms of silence and 60 ms of tone, digit d at 400 + 50 d Hz: page >> 6, page & 63, engine, code >> 6, code & 63.
The code is FNV-1a of the last notice's text folded to 12 bits (`hwt_code`). `tools/hw/capture.py say FILE.wav` decodes a recording and
names the notice by hashing the firmware's notice texts the same way. Host tests: tests/input_test.c (a held key through the debounce, every
id) and tests/hw_say_test.c (the plan and the interrupt's tone code, rendered).

## OBXF (engine 14)

OBXF is the hardware-float OB-Xf port. `NAMES` exposes 75 curated CC0 factory patches.
EDIT/HOME macros are CUT, RES, ENV, ATK, DEC, REL, DTN, PTCH; the first seven are neutral at zero.
On-device OBXF pages edit the full 95-value patch (HQ is retained but not rendered).
User presets retain PTCH and macros, like FM6. Projects, templates and runtime backups retain the edited patch
and its name, independently of PTCH. Full-patch SysEx transfer and dedicated web pages are deferred.

## Native CZ-1 tones (commands 75/76)

INFO advertises native tones with the tagged capability `43 01 10 01` (hex):
CZ support, version 1, 16-byte native names, bank support. Retired sample commands
no longer upload material; SMP_INFO reports zero factory and user slots.

- **75 CZ_GET**: arguments `target index`; target 0 = track (0..3), 1 = user preset
  (0..127). Reply `target index rc`, followed on success by 288 low-first nibbles
  encoding the complete 144-byte native tone.
- **76 CZ_PUT**: arguments `target index` followed by those 288 nibbles. For a bank
  slot only, an optional 32-byte ordinary pattern may follow. Reply `target index rc`.

Invalid lengths, nibble values and native synthesis parameters are rejected before
changing the track or flash. An unused/non-native preset GET returns an error.
Track PUT selects engine 15 (CZ-1), native tone marker 2, resets engine-specific controls to
neutral defaults, and preserves the track's musical/routing settings.
Preset record version 8 preserves all native bytes in a 238-byte record. Earlier 192-byte banks and next’s v6/v7 CZ records remain readable.
FUN13 projects (12352 bytes), FBKG pattern banks (27200 bytes) and TPLB templates preserve each track's native
tone; older formats remain readable. See [native tones](../docs/CZ1_SYSEX.md).

DRUM now has one factory preset, **808 KIT** (index 0); KIT's stored value stays 4.
Older custom KIT values render the 808. No 909 is included.

OBXF is removed from synthesis, presets, editing and patch storage. Engine 14 stays reserved.

### Legacy CZ bank compatibility

INFO advertises zero bank count and zero slots per bank. Command 77 and CZ_GET target 2 remain
readable for older clients, but the current editor uses command 78. Backup objects 9–16 retain their
2332-byte CZBK layout: LE magic 0x42435A43, u16 version 1 or 2, u16 slot count 16, u32 used mask,
16 bank-name bytes, then 16 complete 144-byte native tones. Version 2 marks completion of native-slot
migration, including empty collections. Factory tones are separate; unsaved user slots start empty.

Earlier next FUNA/FUNB, FBKB/FBKC and TPL8/TPL9 CZ sounds are migrated to raw native tones; common sound settings and patterns are retained.

Recorded synth gates: when a NOTE or TIE has no lane hits, its stored accent byte
holds the final gate as 1..255/255 of that swung step (0 uses the legacy track
GATE). FUN11/FBKE retain these gates without changing record sizes. FUN10/FBKD
and earlier formats remain readable with their original accent validation.
STEP_SET/TRACK_STEP keep existing recorded gates on hit-free notes; converting
to lane hits clears the gate. The 12-byte editor reply continues to report lane
accents only. Full backups retain recorded gates.


### Original performance timing (FUN13 / FBKG)

Track parameter **8**, previously the inert `P_ED_FX` field, is now `P_RECQ`: **OFF** (0),
then the ten existing DIV values plus one (1/4 = 1, 1/16 = 3, 16T = 6, 4BAR = 10).
P_COUNT stays 92 and P_E0 stays 84. This parameter is per track, survives sound loads, and
is shown on SEQ > TEMPO and the web sequencer. Scale QNT remains parameter 27. Old projects
and templates initialize timing QNT to OFF. New TPLB templates retain it without changing size.
Web library files use `TQNT` for parameter 8 and keep `QNT` for scale parameter 27, so older
library files retain their scale mapping. The timing page appears only when the device advertises it.

Live recording captures original note edges. When the take finishes, timing QNT commits the new
take's onsets to the nearest swung boundary of the selected division, with loop-end mapping to zero.
Each note retains its captured duration; its release shifts with its committed onset. OFF leaves
the take unchanged. Changing QNT afterward configures the next take without changing playback.
Repeated hits within one step and independent chord releases are retained. A destination occupied
by a manual step or its tie tail rejects the pass without modifying the take or the existing steps.
Playback fires each stored event at most once per loop.

FUN13 LE magic is `0x46554E3D`, size 12352. The original FM6/CZ payload offsets stay fixed.
Header bytes 65 and 67 hold a 12-bit active-bank selection (four 3-bit bank ids, low byte at 65).
At offset 4144 are 1024 eight-byte timed-note records: LE u16 onset fraction, LE u16 duration,
note byte, velocity byte, owner byte, step byte. Velocity zero means unused. Owner low five bits
are `track * 8 + bank`; high three bits are the duration exponent. Step low six bits select the
original onset step, bit 6 marks an overview rounded to its following step, and bit 7 marks overview
wrap to zero. Onset is 0..65535/65536 of that swung step; duration is `u16 * 2^exponent / 65536`
nominal pattern steps. The name remains the final 12 bytes before the FNV-1a checksum.

Step flag 4 marks the overview of recorded events. FUN13 encodes it in the high bit of the step's
metadata byte. FBKG LE magic is `0x474B4246`, size 27200. Each A/B copy retains its five base sectors and
uses two disjoint extension sectors in `0xEA000..0xF9FFF`. The last 256 bytes of each extension
sector stay erased for the SPL update-record scan; payload capacity is 27904 bytes.
The single 256-byte storage header commits last and its CRC covers the complete logical payload. Its inactive steps retain their eight-byte size: four 7-bit notes, then
4 metadata bits (`NOTE n` = 0..4, TIE = 5, REST = 6; bit 3 is the recorded flag), two ordinary flags,
7 velocity bits, 8 hits, 8 accent/gate bits, and 7 probability bits. Counts on REST/TIE normalize to
zero. FUN12/FBKF (152 notes), FUN11/FBKE, FUN10/FBKD and previous formats keep their original decoders and migrate on save.

The 12-byte STEP_SET/TRACK_STEP protocol remains a conventional step edit; it converts that overview
group to manual step playback. Replies mask the internal recorded flag. Full runtime/project backups
retain original timing and every bank. User-preset patterns carry the overview only. Capacity is 1024
notes across the project; overflow reports RECORDING FULL without overwriting existing entries.

All external effects have been removed. Track ids 33..36 and 45..48, global ids
4..11 and 24..26, and modulation destination ids 6..9 retain their positions as
inert format padding. Their labels are empty and editors must hide them. Old
project/preset bytes still parse; runtime loads clear the track/global values,
and old motion or modulation routes to these ids are ignored. New FX motion
cannot be captured. CC91 and CC93 have no effect. No FX pages, hold layer, key
assignment or latch setting remain; UI preference 9 reports 127 (unsupported)
and UI_SET for id 9 returns rc 2. The physical FX button has no action.

## Shared user library (command 80)

INFO appends `55 01 00 03`: shared-library tag U, version 1, capacity **384** as an unsigned
low-first 7-bit pair. Modern clients use command **80** for every engine and every user slot.
All indices below are absolute zero-based shared slots; they are not positions in an engine pool.

Request header: `operation slotLo slotHi`. Every reply begins
`operation slotLo slotHi rc` (0 success, 1 invalid/empty, 2 refused or save error).

| Operation | Request arguments after the header | Successful reply after `rc` |
|---|---|---|
| 0 List | count (1–16) | actualCount totalLo totalHi, then used engine oldFormat name-Z per slot |
| 1 Get | none | used engine name-Z, P_COUNT signed-v14 values, 32 empty legacy phrase bytes, nativeKind [sizeLo sizeHi pack7(raw)] |
| 2 Put | engine name-Z, P_COUNT signed-v14 values, 32 legacy phrase bytes, nativeKind [sizeLo sizeHi pack7(raw)] | none |
| 3 Store track | track, optional printable name bytes without a terminator | none |
| 4 Load track | track | none |
| 5 Erase | none | none |
| 6 Rename | printable name bytes without a terminator | none |

Native kinds: 0 parameters only, 1 DX7 VMEM (128 bytes), 2 CZ-1 (144 bytes), 3 Prophet
(138 bytes, including model/size/command/address metadata), 4 drum sound offsets (54 bytes).
PUT kind bit 5 preserves native name bytes while assigning the short browser name.
PUT kind bit 6 requests default Felucca controls for an imported native synth voice; otherwise
its common sound controls come from the supplied parameters. Names accept up to 20 bytes for
native voices (the browser's short label remains 12); ordinary sound names accept 12.
Phrase bytes remain present for framing compatibility and are not loaded or stored on flash.
GET preserves the complete drum patch, including offsets for all sixteen sounds.

All save paths share the same occupancy and overwrite rules. Device SysEx imports ask before
replacing any occupied slot, regardless of its engine. Native bank imports validate the full
transfer before writing and commit once per affected storage object. On failure, only the
unsaved suffix remains staged. Audio and receive callbacks never access the library cache.
Loading restores the sound and preserves track mix, effects, scale, ARP, and sequence data.
Favorites share each preset's atomic commit; changes made while playing are queued until STOP.
DUMP and native status replies continue to encode the loaded slot plus one with two 7-bit bytes.
Projects use an optional `U,3` origin trailer with four little-endian 16-bit shared references;
embedded sound bytes remain authoritative even when an origin slot is erased.

Flash format UPB2 (`0x32425055`): 8-byte header (record size 206, 18 records), followed by
18 records, total **3716 bytes**. The storage commit header uses reserved word 0 tag
`0x32504D43` (CMP2) to put the payload at byte 32 in every reused pair. Old copies retain
their original offset during conversion; the last 256 bytes of each new copy remain erased
for the stock loader's update-record scan. A compact record contains the first 160 bytes and final 46 bytes
of the existing 238-byte sound record; the unused 32 phrase bytes are omitted. Native versions
11 (FM6), 12 (Prophet), and 13 (CZ) put their voice in the 144-byte payload and 45 named sound
controls in the tail. Used-byte bit 1 is the persistent favorite flag. General parameters and
drum patches retain their existing versions. Unknown records stay inert and protected.

BACKUP_LIST contains **29 objects, IDs 0–28**. Existing object IDs and physical A/B pairs remain:

| Shared bank (18 slots per bank) | Backup IDs | Previous owner |
|---|---|---|
| 0–3 | 6, 7, 17, 18 | General presets |
| 4–7 | 21, 22, 19, 20 | FM6 voices |
| 8–15 | 9–16 | CZ-1 tones |
| 16–20 | 23–27 | Current Prophet programs |
| 21 (six visible slots) | 28 | Previously free 0x9D000/0x9E000 pair |

ID 8 remains empty and refuses payloads. First boot converts current preceding formats in place
using A/B commits. General FM6 sidecars are embedded before their source banks are converted.
The eight excess programs in each old 26-program Prophet bank move to the two spare records
in each preceding 16-record bank; the last six use bank 21. Destinations are deterministic, so
an interrupted conversion retries identical writes. Foreign occupied spill destinations and
unsupported bank shapes are preserved; an incomplete conversion blocks subsequent preset writes.
Boot maintenance initializes the display before any conversion writes and shows
`MIGRATING PRESETS`, completed-bank progress, and `KEEP POWER CONNECTED`.
Update staging cleanup uses the same screen (`FINISHING UPDATE`) with sector progress.
Failures remain visible with restart-to-retry guidance. On preset conversion errors, OCT+
keeps presets and continues with preset writes protected; OCT- opens a destructive wipe
confirmation (OCT- back, OCT+ wipe). Update cleanup errors never offer preset wiping.
Confirmed wiping clears both A/B copies of every shared/preceding preset object, retaining
projects and settings. Bank 21's final unused record (index 17, outside the 384 visible slots)
reserves the 12-byte name `WIPE-PENDING` in an otherwise empty record as durable reset intent.
This is committed before source banks are cleared and removed only after every preceding
copy is empty; interrupted resets resume wiping before attempting any conversion. Successful
reset unlocks preset writes, invalidates loaded slot references and stops future migration
retries. Wipe errors remain visible with writes protected. Clean boots skip maintenance.
The retired Prophet format is never migrated. Unknown committed bytes remain in full backups.
A complete preceding 28-object restore clears the derived extension before conversion; runtime
music restores last, after all source banks are present.

Legacy commands 16–21 address the first 128 shared positions. Command 78 retains the older
64-FM6/128-CZ/128-Prophet addressable prefixes and manufacturer wire framing; it refuses to
replace or erase a slot belonging to another engine. Its INFO capabilities remain compatible.
Clients that need the full bank or mixed-engine writes must use command 80. The historical
storage descriptions below apply to backups from older firmware.

## Previous user storage

UP_LIST reports 64 shared user slots, U01..U64. All slot indices remain 7-bit.
BACKUP_LIST lists ids 0..27. Id 8 is always empty (the retired FM6 bank) and refuses a payload.
Ids 17 and 18 hold user records U33..U48 and U49..U64 (3816-byte UPB1 banks). Ids 19..22 hold the
native FM6 voice banks F033..F048, F049..F064, F001..F016, F017..F032 (2060 bytes each); the owned-voice
objects (UPF6, 3728 or 4240 bytes) that ids 19 and 20 held before 1.0 are refused since 2.0
(Melodee 1.2 restores and rewrites them). An FM6 general preset's voice is native slot k.

On flash, extension user banks omit their eight-byte UPB1 header (3808 record
bytes), which is reconstructed at boot and included in backups. New storage objects
start payloads at byte 32 and leave bytes 3840..4095 erased for OTA record scanning.
Existing user record banks and storage object numbers remain in place.


## Previous native FM6 and CZ-1 pools (command 78)

INFO appends `4E 01 40 00 00 01`: tag N, version 1, FM6 capacity u14 = 64, CZ capacity u14 = 128.
These are unsigned low-first 7-bit pairs (unlike signed v14). Native slots appear after each engine's
factory presets in global and engine-specific scrolling; only used slots are listed for playing.
SAVE > USER and FM6 STORE write to these collections. They store no common Felucca parameters,
patterns or function settings. Loading preserves those track settings and same-engine macros;
changing engine initializes only its engine-specific controls. General UP_* commands retain 64 legacy/general
slots for compatibility. New native transfers use command 78 rather than UP_PUT + a sidecar voice.

Request: `engine op index [data]`, engine 12 FM6 or 15 CZ-1. Reply: `engine op index rc [data]`.
Indices are zero-based 7-bit bytes, including CZ index 127. rc 0 success, 1 invalid/empty load,
2 empty GET or flash error, 3 transport could not stop. Writes stop transport before touching flash.

| op | request data | successful reply data |
|---|---|---|
| 0 list | count 1..16, index is page start | count clamped to capacity, capacity u14, count × (used, name NUL) |
| 1 get | none | FM6: 128 raw 7-bit VMEM bytes; CZ: 144 low-first nibble pairs (288 bytes) |
| 2 put | native bytes as above | none |
| 3 store track tone | track 0..3 | none |
| 4 load slot to track | track 0..3 | none |
| 5 erase | none | none |
| 6 rename | ASCII name, 1..10 FM6 or 1..16 CZ bytes; no terminator | none |
| 7 current slot | none; index is track 0..3 | slot+1 u14, or zero if no native origin |

List names use at most 12 characters for display; GET preserves the full native name (10 FM6, 16 CZ).
The web editor imports DX7/Dexed .syx voices into free FM6 slots and Casio .syx frames into free CZ slots;
collection exports use those same native formats. Native favorites use frozen categories 17 (FM6)
and 18 (CZ), independent of NENGINES, through FAV_GET/SET with their existing signed v14 index encoding.

Before Prophet, complete backup inventories contained 23 objects, ids 0..22. Current inventories add 23..27, as documented below. General ids 6,7,17,18 are unchanged.
CZ ids 9..16 retain the layout above. FM6 ids 21,22,19,20 hold F001..F016, F017..F032, F033..F048,
F049..F064 respectively. Each is 2060 bytes: LE magic 0x314D464E (NFM1), u16 version 1, u16 slot count
16, u32 used mask (low 16 bits), then 16 × 128 native VMEM bytes. Used voices must contain only 7-bit
bytes. Empty restore writes a valid empty object so erased slots cannot resurrect on reboot.
Old 3728-byte UPF6 objects 19/20 remain accepted for owned-voice migration. Retired selector-based
bank object 8 accepts only an empty restore.

Native FM6 storage uses new objects 23/24 (A/B pairs 0xD8000..0xDBFFF) for slots 1..32, and reuses
old objects 7/22 for slots 33..64. Migration commits the new objects before reusing old voice sectors,
then clears successfully migrated shared records. Existing CZ bank tones keep their indices; embedded
CZ user tones move into free native slots, without overwriting existing tones. If the CZ collection is full,
unmatched general records remain accessible. All new objects use the existing CRC-checked A/B save path.

### Microtonal SCL selections

Track parameter 26 (SCL) now has 70 names (IDs 0–69); the original 0–15 retain their meanings.
DESC replies carry up to 96 enum names within the existing 1024-byte response buffer. Consumers
should read the names present in the reply, allowing older firmware's shorter descriptors.
With a new scale and QNT enabled, note addresses are scale degrees around address 60 = ROOT at
C4 + TRN. Recordings and STEP/NOTES keep these 7-bit addresses. MIDI OUT does not send tuning
messages. See [the catalogue](../assets/scales/README.md) for mapping and register limits.


### Prophet native programs (engine 19)

INFO adds the capability bytes `35 01 13 10 11 12 14 00 01` (hex): tag 0x35,
version 1, engine ID 19, frozen general/FM6/CZ/Prophet-user categories
16/17/18/20, flags 0, native protocol version 1. Native collection capacities
also include engine 19 with 128 slots. Never derive collection categories from
the engine count when this namespace block is present. Factory Prophet
favorites use category 19; general presets retain category 16. Preset command
8 recognizes factory engine IDs only: use native command 78 for user slots.
General UP_STORE/UP_PUT reject engine 19; full Prophet records use native storage.

**Command 95:** `operation track [data]`, track 0–3.

| operation | request data | reply |
| --- | --- | --- |
| 0 exact GET | none | `1 rc track` plus native frame without F0/F7 |
| 1 complete PUT | native frame without F0/F7 | `1 rc track` |
| 2 panel field | raw offset, value | `2 rc track` |
| 3 rename | 1–20 printable ASCII bytes, no terminator | `3 rc track` |
| 4 send native edit dump | none | `4 rc track`, plus a separate native dump |

rc 0 success, 1 invalid arguments/frame. Field/rename operations require engine
19 on the destination track. Field offsets are 0–54, 86 (bend) and 87
(priority/retrigger); only defined descriptors are accepted. Bool fields are
0/1; knobs usually 0–120, Fine B/Poly-Mod envelope/Vintage 0–127; keyboard
tracking 0–2, unison count 1–5, detune 0–7, bend raw 0–11, priority 0–3.
Bend displays 1–12 semitones. Imported native records can retain values above
panel limits: DSP clamps these without rewriting them.

A complete PUT validates before changing RAM, initializes native performance,
sets engine 19, resets its eight common macros and clears user-slot origin.
It retains matrix, scales, chords and sequencer and participates in
sound undo/redo. Whole patches replace sounding voices using the normal panic
fade. Field edits affect held notes; unknown/opaque fields cannot be addressed
by panel operation 2. Native names occupy raw 65–84.

Native frames use `F0 01 model command ... F7`, model 0x31 or 0x32, command
2 (program: group 0–9, program 0–39) or 3 (edit buffer). Payloads are exactly
128 raw / 147 packed bytes or 133 raw / 152 packed bytes. Eight-byte packed
groups contain a high-bit mask then up to seven low-seven-bit bytes. Final
unused mask bits must be zero. Length, embedded status bytes and addresses
are checked before mutation. Exact GET/export preserves original model,
length, command, address and all unused bytes. Native dump send uses command 3.

Direct USB native command 2/3 stages the validated named patch and opens the Prophet USER list for
destination selection. OCT+ > Import here commits it while stopped; occupied slots ask first.
Command 95 PUT still loads the selected RAM track without a flash
save. Native command 6 requests its edit dump. Command 5 plus group/program
requests a used user slot at `group * 40 + program`, limited to 0–127.
Unsupported/empty program requests have no native reply. Native NRPN/CC
parameter editing and TRS native SysEx are outside this implementation.

Command **78** adds engine 19 using the same operations as FM6/CZ. Its native
record is 138 bytes: raw[133], raw size, model, command, group, program. All
138 bytes use low-first nibble pairs (276 transfer bytes). Rename permits
1–20 characters, GET retains all twenty, LIST truncates only its display name
to twelve. LOAD sets native performance and keeps track matrix/patterns.
The dedicated collection stores the complete native record rather than common
Melodee parameters. Native slot indices remain unsigned bytes 0–127.

**Backups:** inventory IDs 0–22 remain fixed; 23–27 are Prophet banks. Each bank
is 3600 bytes: LE u32 magic `0x31553550` (P5U1), used mask, favorite mask,
then 26 complete 138-byte records. Banks 0–3 use mask bits 0–25; bank 4 uses
0–23. Bank 0's favorite bit 31 holds the legacy factory INIT star; current
factory Prophet stars, including all 200 programs, live in the settings record.
Other high bits are invalid. Used records require valid wire metadata. Fresh
user banks and an empty restore are empty; factory programs remain available
in the regular browser. Older archives that omit these objects leave them intact.
The editor validates nonempty Prophet objects and target capability before
starting restore writes.

**Projects:** FUN14 magic `0x46554E3E`, size 12904, preserves all previous
FM6/CZ/timed-note offsets. Four 138-byte Prophet records start at offset
12336; name/checksum move to the final sixteen bytes. FBKH magic `0x484B4246`,
size 27752, embeds the FUN14 record at offset 8; its extra-bank sections follow
the larger record. TPLC magic `0x434C5054` includes the same four full native
records. Older FUN13/FBKG/TPLB and earlier data migrate with INIT Prophet
records for tracks without native data. Projects, runtime backups, templates
and sound undo/redo retain opaque bytes independent of user collection slots.
Current projects also keep each Prophet track's user-slot origin in reserved
bytes so browsing resumes at that slot. Older projects locate an unchanged
embedded factory or user patch by its complete record.

The app boundary is 0x89000. Five native A/B pairs occupy 0x89000–0x92FFF.
Normal OTA/loader paths reject mismatched older app extents before writing.
See [prototype validation and release gates](../docs/PROPHET5_PROTOTYPE.md).

### Second-core diagnostic extension

AUDIO_STATS (72) flag bit 2 (`4`) requests schema 4: the 20 schema-2 counters,
the six schema-3 voice counters, then six further 32-bit values encoded as five
7-bit bytes each: `core1_online`, `core1_jobs`, `core1_max_job_us`,
`core1_max_wait_us`, `core1_timeouts`, `fm6_pairs`. A serial build returns zero
for these six values. Existing requests still receive schema 2 or 3. Flag bit 0
also clears worker job/wait maxima after the snapshot; counts remain cumulative.
The worker is idle whenever the main-loop handler takes this snapshot.
With flag 4, schema-3 fields 22 and 23 (formerly OBXF's voices, always 0) carry
`core1_rejected` (requests CPU1 misread and refused) and `core1_faults` (CPU1
exceptions caught); either retires the worker (`core1_online` 0). Both stay 0 on
a healthy unit. Test builds (`MELODEE_CORE1_TEST=1`) add command 79: argument 0 report, 1 inject
a misread request, 2 a job at address 0, 3 a job that never returns, 4 reboot (a
second argument 1 keeps CPU1 barred, otherwise the bar clears as at power-off),
10 crash CPU0 (no reply); the reply is mode, rc, then eleven 32-bit values as
five 7-bit bytes each. Arguments 5 (rails, read only, and clock), 7 count
misreads only (0/1) and 8 hold/release eight notes on track 0 reply mode, rc and
sixteen such values; 9 replies mode, rc and P33 0x00..0x3F, 0x72, 0x74, 0x90,
0x92, 0x9B as 7-bit pairs (see `tools/core1_fault_test.py`).
`core1_jobs` includes both FM6 and Prophet sample jobs; `fm6_pairs` continues to
count only FM6 pairs. For Prophet measurements, combine worker jobs with the
schema-3 voice counts and audio timing counters.

### Recording/performance data extension

P_COUNT is 100 and P_E0 is 92. Common IDs 84–91 are drum lane levels
(KICK, SNARE, CLAP, HATCL, HATOP, TOM, RIM, BELL), range 0–127, default 127.
FUN15 magic `0x46554E3F`, size 12936, moves the FUN14 FM6/CZ/note/Prophet
tail offsets by 32 bytes. FBKI magic `0x494B4246`, size 27784, embeds FUN15
at offset 8. TPLD magic `0x444C5054` adds 64 bytes of track parameters to TPLC.
FUN14/FBKH/TPLC and earlier data remain readable, with missing lane levels
initialized to 127. Settings retain PER5 and its template boundary; a
`0x5250` high-word tag in the unused zoom word identifies recording preferences.

### Native synth motion (1.0.2)

Native events use parameter `112 + patch_byte` (up to 255) and value
`engine_tag << 8 | byte_value`, with tags 1 = Prophet, 2 = CZ-1, 3 = FM6. Voice assignment,
CZ line selection, FM6 transpose and names are excluded. Fields and values are validated.
The stored four-byte event and project extents are unchanged; older firmware rejects native events.
The project snapshot saves the original patch bytes, independently of sounding motion.

In command 64 queries, writes and deletes, the high bit of the parameter is carried in bit 6
of the step byte: `wire_step = step | ((param & 128) >> 1)`, `wire_param = param & 127`.
Decode `step = wire_step & 63`, `param = wire_param | ((wire_step & 64) << 1)`.
Existing events keep their original wire representation. Values retain the signed 14-bit encoding.
The web editor displays and edits the raw native byte while preserving its engine tag.

With three track FX slots (`P_COUNT = 117`, `P_E0 = 109`), common engine IDs
112–115 overlap the first native IDs. Native values carry a tag and are at least
256; ordinary values remain -64..127. Both kinds may coexist at the same step
and raw ID. A six-byte delete request includes the event's v14 to select its
kind. The legacy four-byte delete removes both kinds at that step and ID.
Editors should send typed deletes when INFO reports `pe0 >= 108`.

FX slots occupy IDs 96–99, 100–103 and 104–107 (TYPE, MIX, RATE, CHAR). They
belong to the track; preset loads and sound undo preserve them. Projects and
templates store all three slots. REVERB is appended as type 8; previous type IDs
are unchanged.

INFO advertises motion capability version 2 (`4D 02 40 01`), so cached v1 editors
do not offer a motion interface that would truncate native parameter IDs. The current editor
accepts both versions 1 and 2 and retains all other tagged capabilities.

The send bus appends track SEND at ID 108; engine parameters are 109–116. Globals 27–38 are three shared FX slots (TYPE/MIX/RATE/CHAR); global 39 is RETURN. `G_COUNT=40`. SEND is track-owned during preset loads; shared controls are song globals. FUN19/TPLI store the routing; FUN18/TPLH and earlier records map engine IDs by count and default shared FX off. Native tagged motion IDs retain their meaning during conversion.

## Complete audio profiling (laboratory builds)

`72 AUDIO_STATS`, flag `32`, returns schema 7: all 55 schema-6 fields followed
by complete interrupt maximum bound (us), last complete C span (us), elapsed C
deadline violations, 80% target violations, cross-track batch pairs, occupied
slot capacity steals, admission rejections, admission-enabled flag, and fixed
benchmark status. Benchmark status bit 0 is its lease; higher bits count
suppressed emergency fade requests. Existing diagnostic flags remain compatible.
The complete bound includes an assembly entry/exit envelope, the largest observed
nested TIMER5 span as a footer-preemption allowance, and 4 us wrapper allowance.

With `MELODEE_AUDIO_PROFILE=1`, editor 98 (`[]` or `[0]` read, `[1]` read then
reset scope totals) replies `rc, schema=1, scope_count=21`, then three unsigned
32-bit values per scope (five little-endian 7-bit bytes each): total TIMER4 ticks,
calls, maximum ticks. Scope order is defined in `audio_profile.h`. Five trailing
values are worker ticks, CPU0 waiting ticks, lifetime worker jobs, waits since
scope reset, and current SYS PLL clock in MHz. Inclusive/nested scopes must not
be summed; worker engine spans are published privately and merged after joins.
The FX span includes track inserts, shared preparation and the CPU1 shared
kernel; output includes master processing, USB copy and output conversion.

Editor 100 is a read-only target query available in production too. It replies
`rc=0, schema=2, count=12`, then twelve u32 values in five 7-bit bytes each:
PLL clock MHz, maximum TIMER5 ticks, maximum assembly audio-envelope ticks,
profiling-enabled, benchmark-support-enabled, admission-enabled, half-buffer
frames, four native engine capacities, and the selected shared workload budget
in points. Schema 1 has the same eleven-field prefix but reports the earlier
fixed track reservations; readers should accept both schemas. TIMER4 runs at
24 MHz. The footer
allowance above is valid only while maximum TIMER5 service is below its 100-us
period (2400 ticks). This command writes no musical or persisted state.

With `MELODEE_AUDIO_BENCH=1`, editor 99 `[0|1]` disables/enables a 30-second fixed
voice lease. It bypasses normal admission and records emergency requests without
cutting voices, preventing overloaded runs from reducing their own workload.
`[2, track, mode]` sets the live FM6 render mode (0 MODERN, 1 MARK I, 2 OPL) for
laboratory coverage. Both reply `rc`. Neither writes saved projects or patches.
These controls are absent from production builds. A passing performance contract
must also be checked with normal admission and emergency protection enabled.

Editor 97 (device builds with `MELODEE_FX_PROFILE=1` only, laboratory): the effects' profiler. `[mask, fx1, fx2, fx3, ticks1, ticks2 [, mode 0|1]]` (mask 0..15,
the three effect numbers below `FX_COUNT`; mask 0 or no argument: read only) arms a measurement, and every call replies `rc` (1: a bad argument) then 14 values of five
7-bit groups (LSB first): the control word, three words (ticks, blocks, maximum) for each of four measured slots, and the effects hardware status. It writes no musical or
persisted state and is absent from production builds (0.12.2: documented; the code is unchanged).

### Laboratory persistence guard and transport/REC controls

With `MELODEE_AUDIO_PROFILE=1`, editor 98 also accepts:

- `[2]`: the bounded audio-envelope histogram. Reply `rc=0, 2, bins=64`,
  then u32 bin width in TIMER4 ticks, u32 maximum envelope ticks and 64 u32 bin
  counts (five 7-bit bytes each). The last bin counts every longer buffer.
- `[3]` reads and `[3, 0|1]` clears/sets the persistence guard. Reply
  `rc=0, 3, guard`. Setting it replies only `3` while the transport is busy
  (playing, counting in, song chain, pending Start), a momentary snapshot or an
  old template is active, or any voice sounds. The guard remembers the current
  project slot; clearing restores it. While set, autosave, kept-value and settings
  writes are skipped, storage commits fail, and editor 67 restores preferences in
  RAM only. A malformed request replies only `1`.
- `[4]`: committed storage headers. Reply `rc=0, 4, count`, then per object
  a present flag and u32 length, CRC and sequence.

Editor 101 exists only when **both** `MELODEE_AUDIO_PROFILE=1` and
`MELODEE_AUDIO_BENCH=1`; other builds give it no reply. It drives the same
queued transport and REC arming as the panel, for guarded CPU fixtures. It adds
no state, persistence or audio-tick work. Request `[op, mask]`, exactly two bytes:

| op | Action | mask |
| --- | --- | --- |
| 0 | Query | 0 |
| 1 | Queue internal-clock Start (count-in applies as on the panel) | 0 |
| 2 | Queue Stop; also cancels a pending Start or count-in | 0 |
| 3 | Arm latched REC | tracks, 1–15 |
| 4 | Arm one-pass REC | tracks, 1–15 |
| 5 | Disarm (seal, finish and quantize the take as a REC tap does) | tracks, 1–15 |

The reply is always ten bytes: `rc, schema=1, playing, counting, rec, rec_wait,
rec_pass, rec_latch, pending, clock`. The fields are one snapshot taken after the
operation with audio interrupts paused. `rec*` are four-track masks, `pending` is
the queued transport request (0 none, 1 Start, 2 Stop, 3 restart) and `clock` is
CLK (0 INT, 1 USB, 2 TRS).

`rc` 0 means accepted. `rc` 1 is a malformed request: wrong length, op above 5,
a mask of 16 or more, or a mask present for ops 0–2 or missing for ops 3–5.
`rc` 3 is refused: no persistence guard (every op, including query); Start or
arming while a song chain is armed or running, a momentary snapshot is active or
the template is old; Start with external clock, while playing or counting in, or
with any transport request pending; arming a track beyond the tracks in use.
Stop and disarm need only the guard, so cleanup stays possible.

Start acknowledges a queued request, not a running transport: the audio interrupt
applies it at its next block. Poll query until `pending=0` and `playing=1` (or
`counting=1`). Never resend Start after a lost reply; query, then Stop if needed.
Query, Stop and disarm are idempotent. Arming follows native REC: during playback
a newly armed track waits for loop zero (`rec_wait`), and a one-pass take disarms
after its pass. Arming an already armed track leaves it, including its latch mode,
unchanged, so a retried arm cannot alter the take. Recorded notes change only the
live project, exactly as panel recording would.

### Shared workload budget

Global ID 41 is BUDGET (`G_COUNT=42`), range 8–32 integer points. Existing
global IDs remain unchanged; FXMODE is 40. GLO > GLOBAL knob 2 edits this
device setting. Like A4, it is a virtual global value preserved across project
and template loads. A tagged high word (`0x5742`) in PER5 `ext.usb_off` stores
nondefault budgets in bits 8–15 while retaining the low two USB flags. The
604-byte settings record and project/template layouts are unchanged. Unknown
or absent tags use the build default (16 with admission enabled).

GLO > WEIGHTS shows device settings on the reserved global IDs 4–9 (P5, CZ-1,
FM6, SID and DRUM points per voice, 0 = AUTO; SAFETY). They are panel-only:
`global_reserved()` still covers these IDs, so the editor reads 0 and cannot set
them, and projects never store them. PER5 favorites row 8 bytes 8–15 hold them
('W', version 1, five points, bit 0 of the last byte SAFETY OFF).

The shared pool borrows idle capacity and retains native engine slot counts.
Its weights are Prophet 3 points with one active Prophet track, 4 with multiple
active Prophet tracks; FM6, SID and DRUM 2 points; CZ 3 points. Capacity fades
retain the legacy `voices_given_up` count and also increment `capacity_steals`;
normal pool fades can be read as `voices_given_up - voices_shed`. Emergency
CPU cuts remain `voices_shed`. Automatic settings writes wait until voices and
USB audio streams are idle.

### FX TONE

`P_COUNT=122`, `P_E0=114`, `G_COUNT=45`. Track IDs 111–113 and global IDs
42–44 are the three insert/shared slots' TONE, −63..63, default 0. Format 17
(`F_TONE`) displays 0 as FULL, a value below 0 as DARK (a high cut of
`120 + (64 − |v|)² × 2520 / 3969` Hz: 2640 Hz at −1 down to 120 Hz at −63) and
a value above 0 as THIN (a low cut of `150 + v² × 1130 / 3969` Hz: 150 Hz at 1
up to 1280 Hz at 63), using integer division. Echo, Room and Reverb filter their
wet inputs and always keep a low cut of their own (Echo 100 Hz, Room and Reverb
150 Hz) at FULL and under a high cut; THIN raises it. Other effects ignore TONE,
and the device hides its knob for them. The editor places each slot's TONE on a page
beside its main controls and hides it when that effect type does not support it.
These values belong to tracks/projects, so instrument loads and librarian capture
preserve/exclude them like the existing insert settings. FUN22 stores the three
shared TONEs + 64 in the first three of the six reserved bytes before
`PROJ_FM6_OFF` (the other three stay 0); track values map by count. TPLL stores
the shared TONEs + 64 in master bytes 14–16 of its 20-byte master block (17–19
zero).

The firmware before it (201cbbd, never released) had a low cut and a high cut
per slot (`P_COUNT=125`, `P_E0=117`, `G_COUNT=48`; FUN21 and TPLK, formats 17
`F_HIGHPASS` and 18 `F_LOWPASS`). Its records load: each slot's cut becomes the
nearest TONE, the low cut when both were set; a low cut at or under 150 Hz counts
as the effects' own and leaves the high cut (or FULL). FUN20/TPLJ and all previously
supported imports load TONE FULL and retain their routing and engine/motion values.

### Chord strum

Common track IDs 114 `STRUM` and 115 `STROKE` are appended before the engine
parameters: `P_COUNT=124`, `P_E0=116`, `G_COUNT=45`. Earlier common IDs retain
their values; engine IDs are read from INFO. STRUM is an enum: 0 OFF, 1 1/8,
2 1/16, 3 1/32, 4 1/64, 5 1/128, 6 16T, 7 32T. STROKE is 0 DOWN, 1 UP,
2 ALT. Both default to 0 and survive sound loads. The device abbreviates STROKE to DIR. Projects map older counts
with STRUM OFF; existing engine values retain their mapping.

The division is the complete first-to-last sweep, with an immediate first tone
and evenly spaced remaining tones. Input release and STOP cancel pending tones;
MIDI sustain follows existing ownership. ARP bypasses strumming. Live recording
captures the staggered onsets; STEP entry continues to write a complete chord.
