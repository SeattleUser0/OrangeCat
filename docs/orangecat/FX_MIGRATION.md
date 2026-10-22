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
   buffers from the arena when it is used and gives them back when it has rested: the delay's 64 KB line in two
   32 KB halves (no bank holds 64 KB in one piece), the reverb's 11.9 KB and its model line (32 KB for CLOUD, 4 KB
   for SHIMMER and the pre-delay), the chorus 4 KB, the SLICER and the FX layer's 32 KB. The sizes are Jiant's. A bus
   that cannot get its memory plays dry and the page says FX FULL. Worst case and typical use are in the measurements.
4. Code placement: the effects' code stays in flash (section .text.xip_jfx, as MACRO's models): the RAM code would
   otherwise push the arena under its 48 KB floor. The audio interrupt never runs while the flash is written, and
   CPU1 only runs a bus job the audio interrupt gave it, so no effect code runs from flash during a write.
5. Parameters: per-track effect settings reuse the 18 retired FX track ids (96..113); the project format gets a new
   magic so those ids are never read with the old meaning. Global effect settings reuse the old global FX ids where
   free and add new ids after them; they are kept with the project in the record store (the OCJ3 lists, versioned by
   their length), as the chains, the song and the grooves are.
6. Old projects (FUN22 and before): converted on load. The old track sends go to the new bus of the old slot's kind
   (ECHO to the delay, ROOM and REVERB to the reverb, CHORUS to the chorus); the old inserts (PHASER, WIDTH, FORMANT,
   TAPE, SPIT, ECHO, ROOM, REVERB, CHORUS on a track) have no Jiant counterpart and are dropped. They cannot sound the
   same; the build document says so.
7. The leveler (Jiant's, always on there) is a MASTER setting: on for new projects, off for converted ones (their
   loudness stays as it was).
8. Not ported, because they need parts of Jiant that Orange Cat does not have: DUCK and PUNCH (they hang on the DRUM-X
   engine's kick), the PERFORMANCE MACROS STEPS, TRANSPOSE and MUTATE (sequencer features, not audio), 1/2 TEMPO and
   RANDOM's steps (Jiant's live sequencer), the punch-in lane (Jiant's song sections). Listed again in the final report.

## Progress

- [x] Baseline recorded (above).
- [x] Inventories.
- [ ] jfx.c: the DSP (DIST, AUTO PAN, CHORUS, DELAY, REVERB x5, gate and rest, SLICER, master chain, FX layer).
- [ ] mix.c integration, CPU1 job, card freeze and USB taps.
- [ ] Parameters, pages, MIDI CC, motion, web editor tables.
- [ ] Persistence: FUN23, the lists block, conversions, template.
- [ ] Old fx.c and its tests removed; build.py's removed-symbol list updated.
- [ ] Internal sample slot (flash), its interface and tests.
- [ ] Tests, audition WAVs, CI builds, measurements.
- [ ] Documents: BUILD, SPEC, GUIDE; final report.

## Commands

- Host tests: `tests/run_tests.sh` (about 14 minutes; PARALLEL=n). One test: see `tests/run_tests.sh` for its flags.
- Firmware builds: GitHub Actions only (no JieLi toolchain here; its download is blocked). Run the build workflow on
  this branch: `gh workflow run build.yml --ref fx-jiant -R SeattleUser0/OrangeCat` (no pages, no release: those run
  only for main and tags). Sizes come back as run annotations: `gh api repos/SeattleUser0/OrangeCat/check-runs/<job>/annotations`.

## Blockers

- No hardware here: every on-device check is left to the unit (a procedure is in BUILD.md when done).
- No local toolchain: firmware size and RAM numbers only from CI.
