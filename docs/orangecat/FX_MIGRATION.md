# Orange Cat: the Jiant FM effects migration (checkpoint)

This file lets a new session pick the work up without doing the investigation again. It is kept up to date at each
milestone. Plain words, short sentences, as the other Orange Cat documents.

## Pinned revisions

| What | Revision | Notes |
|---|---|---|
| Orange Cat start | `10615fc` (main, 0.5.6, October 22, 2026) | The branch starts here. CI run 39845631211 built it and passed the host tests. |
| This work | branch `fx-jiant` (worktree `/home/claude/oc-fx`) | Never merged to main by this work. |
| Jiant FM donor | `dc8d025526eecdd89d0bdf0cc3669c22cbde1b71` (main, v0.9.18, October 21, 2026) | Clone at `/home/claude/ref/jiant`. |
| Jiant installer | `6968b20ed195c284a2523138d16f59ee06477490` (v0.9.9) | The public installer is pinned to 0.9.9 ("later builds crash on the device"). The beta installer is 0.9.9-cpu5usb1 (`6cb8561`), CPU and USB fixes on 0.9.9, not 0.9.18. |

Why the donor is main (0.9.18) and not the installer's 0.9.9: only main has the full punch-in set the assignment names
(GLITCH, TAPE STOP, SHUFFLE, FREEZE, the CRUSH and THROW sweeps) and the CLOUD fixes. The crash that made Jiant pin its
installer is not in the effects code as far as its history shows (the beta's fixes are CPU and USB). The difference
between the two is listed in `/home/claude/ref/jiant_fx_inventory.md` section 12.

Inventories (read only, written by two agents at the start):
- `/home/claude/ref/jiant_fx_inventory.md`: every Jiant effect, its parameters, state, DSP (verbatim), tests, persistence.
- `/home/claude/ref/oc_fx_storage_inventory.md`: Orange Cat's old effects, their ids, buffers, the flash map, the old
  sample slots and what holds their flash now.

## Baseline (0.5.6, CI build of 10615fc)

| Measure | Value |
|---|---|
| Firmware image | 542,664 B of 573,440 B (XIP 0x02001000 + 0x8C000): 30,776 B free |
| RAM .data + .bss | 93,912 B of 98,304 B |
| POOL (.pool) | 218,656 B of 344,064 B (the rest: .audio_text 61,812 B, .cpu1_stacks 8,192 B, the arena) |
| Audio working arena (main bank) | 55,328 B (the linker's minimum 49,152 B: four engine states) |
| Other arena banks | low SRAM 17,392 B, data tail 4,392 B, retained tail 4,688 B: 81,800 B in all |
| Cache RAM | 28,672 B after the boot self-test: 14,336 B to the UI, 14,336 B to audio (96,136 B audio at most) |
| RAM code | .audio_text 61,812 B, .dsp_text 7,106 B, .ram_text 3,394 B, .dsp_tables 4,614 B |
| Stacks | 24 KiB user, 8 KiB system, CPU1 8 KiB (in .cpu1_stacks) |
| Old effects (size report group) | flash 11,928 B, static RAM 19,967 B; on demand from the arena: ECHO 8 KiB, REVERB 4 KiB, ROOM 2 KiB, CHORUS 2 KiB a slot |
| Host tests | all pass in CI. Locally 7 fail for the environment only (web pages, boot packages, power cuts, M-UPGRADE, update loader, installer CLI, ROM USB recovery: need the firmware package or network) |

Flash ownership (from the storage inventory, B1): app 0x05000-0x90FFF; marker 0x91000; cfg 0x92000; preset banks
0x93000-0x9FFFF; record store 0xA0000-0xC7FFF (40 sectors, was USR1 and USR2); preset banks 0xC8000-0xDFFFF (USR3 and
banks 0-1); update loader 0xE0000-0xE4FFF; preset banks 0xE5000-0xE9FFF; record store 0xEA000-0xF9FFF (16 sectors);
bank 7 B 0xFA000; free 0xFB000; settings 0xFC000-0xFDFFF; bank 6 B 0xFE000; key 0xFF000.

## The count ("fifteen effects")

Items 1 to 6 counted as models make exactly fifteen: DIST 6 (SOFT HARD FOLD CRUSH RECT AMP) + DELAY 1 + REVERB 5 (ROOM
SPRING SHIMMER RESO CLOUD) + CHORUS 1 + AUTO PAN 1 + SLICER 1. That leaves out item 7. The full set is about 16 effect
families, or about 41 selectable models and modes (the Jiant inventory, section 18). The work follows the seven items.

## Decisions (made without asking, as the assignment allows)

1. Donor: Jiant main 0.9.18 (above).
2. The new code is `firmware/src/jfx.c` (Jiant's DSP, its names kept where they are Jiant's), included by mix.c in
   place of fx.c. Jiant's GPL-3.0-only headers and copyright lines are kept.
3. Memory: Jiant keeps 152 KB of effect buffers static. Orange Cat cannot: the POOL holds the screen canvas (59.5 KB),
   the card players (36.9 KB), the project staging (27.9 KB), USB audio (12.4 KB) and 61.8 KB of audio code in RAM;
   the arena left is 81.8 KB (96.1 KB with cache RAM), and four MACRO tracks take 45 KB of it. So each bus takes its
   buffers from the arena when it is used and gives them back when it has rested: the delay's 64 KB line, the
   reverb's 11.9 KB and its model line (32 KB for CLOUD, 4 KB for SHIMMER and the pre-delay), the chorus 4 KB, the
   SLICER and the FX layer's 32 KB. The sizes are Jiant's. A bus that cannot get its memory plays dry and the page says
   FX FULL. Worst case and typical use are in the measurements. (Revised in decision 15: the first build split the
   delay in two 32 KB halves, which the device could never hold.)
4. Code placement: the effects' code stays in flash (section .text.xip_jfx, as MACRO's models): the RAM code would
   otherwise push the arena under its 48 KB floor. The audio interrupt never runs while the flash is written, and
   CPU1 only runs a bus job the audio interrupt gave it, so no effect code runs from flash during a write.
5. Parameters: per-track effect settings reuse the 18 retired FX track ids (96..113); the project format gets a new
   magic (FUN23) so those ids are never read with the old meaning. Global effect settings reuse the old global FX ids
   27..44 (41, BUDGET, stays) and add 45..75. (Changed from the first plan, the OCJ3 lists:) FUN23 keeps them in the
   project record itself, at PROJ_JFX_WIRE in its reserved tail (170 bytes were free there): "JFX", a version, one
   byte a global (value + 64). So FBKK banks and web backups carry them too. The template is TPLM, with the same 49
   bytes (jfx[52]) where TPLL had the old FX's 20.
6. Old projects (FUN22 and before): converted on load. The old track sends go to the new bus of the old slot's kind
   (ECHO to the delay, ROOM and REVERB to the reverb, CHORUS to the chorus); the old inserts (PHASER, WIDTH, FORMANT,
   TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS on a track) have no Jiant counterpart and are dropped. They cannot sound the
   same; the build document says so.
7. The leveler (Jiant's, always on there) is a MASTER setting, LEVEL. (Changed:) OFF by default for every project:
   on by default it changed the level of every existing sound (the golden renders, the spread test's pan law), which
   Orange Cat's sound should not do unasked.
8. Not ported, because they need parts of Jiant that Orange Cat does not have: DUCK and PUNCH (they hang on the DRUM-X
   engine's kick), the PERFORMANCE MACROS STEPS, TRANSPOSE and MUTATE (sequencer features, not audio), 1/2 TEMPO and
   RANDOM's steps (Jiant's live sequencer), the punch-in lane (Jiant's song sections). Listed again in the final report.

9. The internal sample slot: 22 sectors of the record store, 0xB2000..0xC7FFF (its sectors 18..39), not a fixed old
   Felucca address: no free 80 KiB was left (the old USR1/USR2 are the record store, USR3 the preset banks 4-5 and
   8-15). Each sector keeps its last 256 bytes erased, so 22 are needed for one Felucca slot's 81,408 bytes plus its
   header. The store drops from 56 to 34 sectors (worst case for patterns was 44 of 56; typical use is about 20).
   Claimed at every boot: records whose current copy is there are moved first (only a store 0.5.x filled that far has
   any), then old copies there are erased. A boot with nothing there writes nothing (the first-boot tests demand it:
   the first version erased the 22 sectors and wrote a mark, and failed them). No room for the moves: no slot until
   there is.
10. Editor commands for the slot: new ids 110..114. The old 11..15 stay retired (they wrote at 0xA0000).
11. Punch-in MIDI effects (Jiant pfx.c: OCT-, OCT+, 1/2 TEMPO, DEC-, DEC+, STUTTER, ATK+, ARP, RANDOM), its lane
   and its target key: not ported. They act on notes in Jiant's voice and sequencer code, which Orange Cat does not
   share. Their keys (D4 .. F5 white, A#4) do nothing.
12. FX layer: hold FX (a quick layer as GLO, SCL, ARP); tap FX: the FX pages, as before. The keys go to the effects
   from the audio interrupt (seq.c keyboard_block), as in Jiant, so they start on the 1/16 to the sample. No FX LATCH
   setting (double-tap FX locks the map open instead).
13. MIDI CC as Jiant: 91 REV, 93 CHO, 94 DLY, 104 DIST. Motion (locks, automation): DIST, TONE, AUTO PAN DEPTH and the
   three sends, as Jiant's list.
14. Labels shortened to fit Orange Cat's cards (CHO, FDBK, WIDE, DJF, LEVEL, SPK, SLC).
15. Effect memory, second version (the first CI build showed the delay could never be held on the device: the main
   bank is 54 KB, every other bank under 24 KB, and the delay wanted two 32 KB blocks):
   - the delay's line is 16 blocks of 4 KB and CLOUD's line 8 (the pre-delay and SHIMMER use the first of those 8).
     They fit wherever the arena has room. The reverb's 11.9 KB is the only effect block that must be in one piece,
     so it is taken first;
   - at most 4 new blocks a block (16 KB zeroed in one audio block, as the first version's 32 KB halves). A delay comes
     in over 4 blocks, 3 ms;
   - the card's 36 KB pool (the frozen tracks' players and recorder) is lent to the effects while the card plays and
     records nothing. Before the card uses it, the main loop takes it back and waits (about 1 ms) for the audio
     interrupt to let go of every effect with a block there. Those effects stop and come back elsewhere if there is
     room, else FX FULL. The effects use the lent pool first, so the SRAM stays free for engines;
   - an engine that finds no memory while the effects hold some makes them give all of it back at the next block, and
     they wait half a second before taking any again. The tracks' sound comes first. The note that found no memory is
     lost (the next one plays);
   - after the arena could not give a block, no new block is tried for a quarter second (no search and zeroing every
     block). FX FULL stays on the page for a second after the last block that wanted memory and got none (it
     flickered between notes before).
16. A bus is wanted while any track's send to it is set (not only while the send carries sound). Its memory comes in
   before the first note, so the first echo of a phrase is never lost to the 4-block growth, and it is not taken and
   given back on every phrase. It goes back 2 s after the sends are all 0 and the bus has rested.
17. Reviewer findings fixed (an independent review of the first checkpoint): SHIMMER faded out with no line read a
   null pointer (all models now go through rev_can_run); a reverb model change with no reverb held kept the buses
   from ever resting; the FX layer's track mutes now stop new notes too (voice.c, perf_hush); with an external MIDI
   clock the effects' grid (REPEAT, the SLICER, AUTO PAN BEAT) is taken each block from the sequencer's own place, so
   it cannot drift; AUTO PAN BEAT reads the block's end as its free count does; the FX FULL flicker (decision 15).
18. When the delay's line becomes complete after other buses already ran, the delay starts as on a first block (its
   rate and read points set, no rate-change fade). Without this the first echo was faded out.

## How it works (the notes the assignment asks for)

### Signal path

Each track, in the audio interrupt (mix.c mix_part):
engine -> DIST (its type, DRIVE, TONE; SPREAD: each side its own) -> the FX layer's mute for that track (a ramp) ->
LEVEL and PAN (AUTO PAN moves PAN: free or on the beat) -> the dry mix, or the SLICER's bus when SLC routes the track
there. The three sends (CHO, DLY, REV) are taken after LEVEL, before PAN, into the chorus, delay and reverb buses. A
frozen phrase's take plays in place of the engine and keeps the track's sends.

Then, once a block (mix.c mix_block, jfx.c jfx_master):
the FX layer's THROW adds the dry mix to the delay and reverb sends -> the buses run (CPU1 when it is free, else
CPU0): CHORUS, DELAY (ANALOG or DIGITAL, PING, GRAIN, its own TONE), REVERB (one of ROOM SPRING SHIMMER RESO CLOUD,
pre-delay for ROOM and SPRING) -> their mono return and stereo side -> the SLICER (on its bus; FX ON: the returns go
through it too) -> DJ FILTER -> CLIP -> the leveler (LEVEL, off by default) -> MASTER (the volume knob) -> the FX
layer's buffer effects (REPEAT, STUTTER, TAPE, GLITCH, SHUFFLE, FREEZE) and CRUSH -> the master out (SPK EQ, the soft
limit) -> the codec.

### Two cores

- Only CPU0's audio interrupt changes effect state, memory or settings it reads. CPU1 runs one job a block: the buses
  (jfx_bus_kernel). The interrupt hands it the sends, the block length and the buses' memory pointers, then joins it in
  the same interrupt (jfx_bus_join) before anything reads the result. No job is ever outstanding between interrupts.
- The memory for a job is fixed before the job starts (jfx_mem runs before jfx_bus_start). Memory is given back or
  lent only at the start of a block (jfx_lend_check), when no job runs.
- A job that does not finish in time is retired (the dual-core test's fault path): its return is dropped for that
  block, the buses keep their memory, their states are reset, and the buses run on CPU0 from then on.
- The FX layer's keys come from the key scan inside the audio interrupt (seq.c keyboard_block), so they never race
  the audio. The main loop only writes settings (song.g, track params) and flags the interrupt reads once a block.

### Flash writes

The effects' code lives in flash (section .text.xip_jfx, chosen to keep the RAM arena large). Code in flash cannot run
while the flash is being written (the flash's read mode, XIP, is off then). Every erase and program in Orange Cat
(records, presets, settings, the sample slot) runs with CPU0's interrupts off (storage_hw.c st_erase, fl_write: the
audio buffer silenced first), so the audio interrupt cannot run then. CPU1 runs effect code only inside a job the
audio interrupt gave it and joined in the same interrupt, so it is idle in RAM code whenever a write can start. So no
effect code runs while flash is written. The web editor's writes (the sample slot among them) also stop the
sequencer first (editor.c ed_flash_stop).

### Memory, as built (second version)

| Block | Size | Where from |
|---|---|---|
| DELAY line | 64 KB, 16 x 4 KB | any arena bank or the lent card pool; while any track's DLY is set |
| REVERB tank | 11.9 KB, one piece | taken first; while any track's REV is set |
| REVERB model line | 4 KB (pre-delay, SHIMMER), 32 KB for CLOUD (8 x 4 KB) | with the tank |
| CHORUS line | 4 KB | while any track's CHO is set |
| SLICER / FX layer buffer | 32 KB, one piece | while STUT runs or a buffer effect plays |

Totals: all three buses 84 KB (112 KB with CLOUD), plus 32 KB for the SLICER's STUT or the FX layer's buffer effects.
The arena is 90 KB on the device (104.5 KB with the cache RAM), plus the 36 KB card pool when the card is idle. The
engines come first. Four of the largest engines take 45.7 KB; then the delay, the reverb (not CLOUD) and the chorus
still fit when the card pool is lent, and do not all fit when the card is playing frozen phrases. Typical engines
(most take a few KB) leave room for everything. The page says FX FULL whenever a bus is short.

## Progress

- [x] Baseline recorded (above).
- [x] Inventories.
- [x] jfx.c: the DSP (DIST, AUTO PAN, CHORUS, DELAY, REVERB x5, gate and rest, SLICER, master chain, FX layer).
- [x] mix.c integration, CPU1 job, card freeze and USB taps.
- [x] Parameters, pages, MIDI CC, motion, web editor tables.
- [x] Persistence: FUN23, conversions, template (TPLM).
- [x] Old fx.c and its tests removed; build.py's removed-symbol list updated.
- [x] Internal sample slot (flash), its interface and tests.
- [x] Tests, audition WAVs, CI builds, measurements (host; the device's CPU is not measured: no hardware here).
- [x] Memory second version (decision 15), reviewer findings (decision 17).
- [x] Documents: BUILD, SPEC, GUIDE; final report.

## Commands

- Host tests: `tests/run_tests.sh` (about 14 minutes; PARALLEL=n). One test: see `tests/run_tests.sh` for its flags.
- Firmware builds: GitHub Actions only (no JieLi toolchain here; its download is blocked). Run the build workflow on
  this branch: `gh workflow run build.yml --ref fx-jiant -R SeattleUser0/OrangeCat` (no pages, no release: those run
  only for main and tags). Sizes come back as run annotations: `gh api repos/SeattleUser0/OrangeCat/check-runs/<job>/annotations`.

## Blockers

- No hardware here: every on-device check is left to the unit (a procedure is in BUILD.md when done).
- No local toolchain: firmware size and RAM numbers only from CI.
