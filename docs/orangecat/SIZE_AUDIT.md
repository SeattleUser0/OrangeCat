# Orange Cat firmware size audit

**Version:** 0.1
**Date:** October 23, 2026
**Branch:** `size-audit` (not merged, not released). Combined with the GUI work on `size-audit-gui`.
**Baseline:** `main` at 19271b0 (0.6.2), the shipping CI configuration (tools/orangecat_engines.sh: MACRO and SHAPES
on; SID, CZ-1, FM6, PROPHET and DRUM off; MELODEE_RAM_AUDIO=1, MELODEE_CACHE_RAM=1, dual core, no core 1 fallback).

The goal was flash for more synth engines, with nothing the instrument does taken away. RAM and the stacks were
looked at separately: freeing RAM frees no flash. The GUI's own size work (branch `gui-slim`, docs/orangecat/GUI_SLIM.md)
ran at the same time and is counted separately; nothing here touches the fonts, the icons, the canvas or the drawing code.

## What the numbers mean

- **Image**: build/melodee.bin, the boot stub (3,808 B) plus the application body. The body may be 573,440 B
  (fm1pkg_make.py BODY_MAX, app.ld XIP LENGTH 0x8C000): it ends below the commit marker's sector, 0x91000. The
  headroom is the body against 573,440, so the baseline's 570,944 B image had 6,304 B free (the build doc's "2.6 KB"
  compared the whole image, stub included, with the body's limit).
- **Package**: the .fwsc (610,529 B) adds the loader, the SPL and the flash header; its size does not change with the
  body (the app area is padded) and sets no limit of its own.
- **Flash the image takes**: `.text` (code and constants run in place) plus the flash copies of what boot copies to
  RAM: `.ram_text`, `.dsp_text`, `.dsp_tables`, `.data`, `.audio_text`.
- **RAM**: `.data` + `.bss` (96 KiB bank), `.pool` and `.audio_text` (the 336 KiB POOL bank; what POOL has left is the
  audio working arena), `.ram_text` / `.dsp_text` / `.dsp_tables` (the low 32 KiB bank; its tail joins the arena), `.noinit`
  (retained across resets). Persistent storage (projects, presets, the record store, the sample slot) is flash above
  0x93000 and is not counted anywhere here.

## How it was measured

- Every number is from a CI build of the real firmware (zvenson/fm1-build, the shipping options). The audit's workspace
  cannot reach JieLi's toolchain server, and GitHub's artifact store and job logs are off limits from it too, so the
  build's symbol table, section table and a few probes travel back as workflow notices (tools/ci_blob.py, tools/ci_syms.py,
  tools/ci_audit.sh: xz, base64, 3,800 characters a notice, ten notices a step). These are temporary and leave with the
  branch.
- Symbols are put under the source file that defines them by scanning the sources (one translation unit: the compiler
  leaves no file names). A function's size includes what was inlined into it.
- The history of image sizes is read from the CI records of every push to main since the fork (the "build sizes and
  checks" notice of each run), not rebuilt.

## Baseline (main 19271b0)

| | B |
|---|---|
| Image | 570,944 (body 567,136 of 573,440; 6,304 free) |
| `.text` | 494,140 (476,110 in 1,650 sized symbols; 16,946 anonymous: string literals and jump tables) |
| Flash copies: `.audio_text` / `.dsp_tables` / `.ram_text` / `.data` / `.dsp_text` | 63,480 / 4,616 / 3,384 / 848 / 668 |
| RAM `.data` + `.bss` | 92,936 of 98,304 |
| POOL: `.pool` + `.audio_text` + CPU1 stacks | 218,400 + 63,480 + 8,192; arena 53,984 (minimum 49,152) |
| Audio resources, all banks (with cache RAM) | 87,884 (102,220) |
| `.noinit` | 10,960 (pattern_chunks_retained 10,752) |
| Stacks | user 24,320 (0x01C74100..0x01C7A000), supervisor 7,936, CPU1 4,096 + 4,096 |

Flash by area (symbols; the anonymous strings are extra):

| Area | B | Notes |
|---|---|---|
| Screen code and data (ui_*.c, gfx.c, fonts, icons, keycaps, palettes) | about 221,000 | the GUI work's field |
| Engines: SHAPES (braids/) and MACRO (plaits/) code and tables | 101,400 | tables 22,050 + 17,702; code: pb_dig_render 14,064 alone |
| Sequencer, recording, motion, chords, notes (seq.c, recording.c, motion.c, note_tools.c, ...) | about 37,000 | seq.c 14,277 is in RAM too |
| Main loop and boot (main.c) | 23,908 | fm1_main 17,744: the loop with its once-called helpers inlined |
| Web editor link (editor.c, editor_backup.c, sysex_library.c, settings_persist.c, editor_preferences.c) | 21,100 | ed_handle 14,838 |
| Effects (jfx.c, jfx_perform.c, jfx_slicer.c) | 23,300 | |
| Projects and storage (project.c, pattern_store.c, oc_*.c, storage*.c, upreset.c, smp_slot.c) | 29,100 | |
| Card: FatFs, USB host, files, streams, freezing (fatfs/, usb_host.c, usb_files.c, card_*.c) | 23,500 | FatFs 13,717 |
| USB device, audio, MIDI (usb.c, usb_audio*.c, midi_*.c) | 7,000 | |
| Voices, mixing, parameters, tables (voice.c, mix.c, mod.c, params.c, dsp.c, melodee_tables.h, scales.h) | 34,200 | |
| HAL (flash driver in RAM, cache, faults, power) | 5,500 | |

## Experiments

Each row is one CI build against the baseline, same options. "Arena" is the main audio working arena.

| Id | What | Image B | Flash | Arena | Decision |
|---|---|---|---|---|---|
| T1 | Each RAM table in a section of its own name (`.dsp_tables.NAME`). All of them shared one `.dsp_tables`, so P5_TANH (4,100 B, the Prophet's and the 909's) was kept by TANH_Q15's use with both engines off | 566,844 | -4,100 | +4,100 (low bank) | Adopted |
| M1 | The card's, the record store's, the USB host's and the screen driver's main-loop files built for size: fatfs/ff.c, ffunicode.c, usb_host.c, usb_files.c, oc_pool.c, oc_lists.c, oc_store.c, smp_slot.c, ota_hw.c, storage_hw.c, lcd.c, and the main-loop parts of motion.c, pattern.c, jfx_perform.c, usb.c, jfx_slicer.c, song_chain.c, oc_throw.c. Their functions in the audio graph keep -Os (size_fns.py RESIDENT_NAMES; audio_ram.py reports any resident function built for size) | 568,920 | -2,024 | -64 | Adopted, then M2 |
| M2 | M1 with motion's native-parameter helpers (inlined into the audio graph) kept at -Os, so nothing moves into RAM | 568,984 | -1,960 | +128 | Adopted |
| R1 | Retired-engine remnants: the Prophet's panel table (P5_PANEL, 1,760 B) only with the Prophet in; the CZ-1's editing descriptors, actions and `cz_ed_active`, and PHASE's borrowed descriptors (CZ_ED, 400 B) only with those engines in | 567,752 | -3,192 | 0 | Adopted (see the note below) |
| R2 | The unhooked engines' rows of PAGES (79 of 136 rows, with their titles) compiled out; page_visible never showed them | 566,312 | -1,440 more | 0 | Adopted |
| C1 | `-fmerge-all-constants` | 570,944 | 0 | 0 | Rejected (no effect: the unit is compiled through LLVM IR, and the linker already merges strings) |
| D1 | Byte-identical constant objects in flash (every sized data symbol of 16 B or more hashed in the built image) | probe | 0 found | | Nothing to share: no table is duplicated, Plaits' and Braids' tables differ in format and content |
| S1 | ED_AUDIO_STATS (the editor's audio diagnostics command, what tools/usb_audio_stats.py reads) compiled out | 559,464 | -788 | | Rejected: a diagnostic the spec and the performance work rely on. Listed under optional cuts |
| X1 | Strings: 16,946 B of anonymous flash is string literals (page titles, messages, preset and category names) and jump tables; about 1,000 B of it were retired-engine titles (gone with R1/R2) | probe | | | No change: the rest is live text |

Combined (`size-audit` at 0052550): image **560,252 B** (-10,692 B; body 556,444, 16,996 B free), RAM .data + .bss 92,904
(-32), arena 54,144 (+160), audio resources 92,176 (+4,292), all host tests green, the render loops' target costs
within budget (fm1_alnk0_irq 3,268 instructions, was 3,310; midi_parameter 250, was 251; fx_buses 771, was 769).

A note on R1: Claude's GUI pass had looked at the same tables and left them (GUI_SLIM.md, row G) because P5_PANEL is read
when a stored project's Prophet automation events are checked. R1 keeps those events instead of dropping any out of
range: with the Prophet out no track can be a Prophet, so an event never applies; a firmware with the Prophet in
checks them against P5_PANEL again. So nothing a project stores is lost, and the lenient check is the one behavior
difference in this branch. The editor's Prophet commands (editor_prophet.c) were already behind MELODEE_P5.

## The growth from 378,992 B to 454,740 B (phase 3 done to 0.4.20)

From the CI record of every push, in order. Each line is a commit's change against the one before it.

| Step | Commits | Image B | Change |
|---|---|---|---|
| Phase 3 done (all of Melodee's engines out, TRIO in) | 44ce874 | 378,992 | |
| 4a parts 1-2: Skiff's step fields and their playback | d840123 | 379,544 | +552 |
| 4a part 3: record store, pool format, project slots, no memmove | 1f34cf3..0bda60a | 388,604 | +9,060 |
| 4c: tracks' lists, bank cache, SONG, PATTERNS pages, rests, device-test fixes | 6936547..d5a1d2e | 393,760 | +5,156 |
| 0.4.0: phrases, chains and the song as a tracker | 8fd6dfc, 0585a67 | 398,252 | +4,492 |
| 0.4.1: PERFORM (scenes, mutes, launch points), CHAIN knob, FILL | bb6a04f..e70fdd2 | 403,992 | +5,740 |
| 0.4.2: TRACKER view and lanes | 12d5def, 7a197f2 | 408,472 | +4,480 |
| 0.4.3: throws | 3ed34a4 | 409,536 | +1,064 |
| 0.4.4: PHRASES page, review fixes | 05650b2, 38c5f5f | 412,904 | +3,368 |
| 0.4.5: groove templates, GROOVE page | 6658ee3..8f7e710 | 414,264 | +1,360 |
| 0.4.6: SEQ tools, review fixes | 4325659, a5fb63f | 416,000 | +1,736 |
| **Sequencer, arranger and their views** | | | **+37,008** |
| 0.4.7: USB host mode, one hub, mass storage (SCSI), the drive test page | 5a25719 | 422,912 | +6,912 |
| 0.4.8, 0.4.9: hubs inside hubs, card reader slots, a retry | 728bdc8, 23b55c8 | 423,732 | +820 |
| **USB host, hubs and mass storage** (with the test page, since removed in 0.5.3) | | | **+7,732** |
| 0.4.10: FatFs (FAT12/16/32, exFAT, long names) and the files page | 1aa6264 | 435,028 | +11,296 |
| 0.4.11: a WAV plays from the card (player buffers) | eead474 | 436,872 | +1,844 |
| 0.4.12: the read from the timer interrupt | 78b4348 | 438,364 | +1,492 |
| 0.4.13: four WAVs at once | 38c3ffc | 439,456 | +1,092 |
| 0.4.14, 0.4.15: the main loop helps, the page, the speed limit | b3089cf..9ac384e | 439,788 | +332 |
| **WAV playback and buffering** | | | **+4,760** |
| 0.4.16: the recorder (REC to a WAV on the card) | 9420587 | 446,172 | +6,384 |
| 0.4.17, 0.4.18, 0.4.19: takes in time, freeze one track, stereo takes | d0d0fb0..2d51562 | 447,412 | +1,240 |
| 0.4.20: freeze a track from MIXER, player slots 9 KB | ed7c048 | 454,740 | +7,328 |
| **Recording and freezing** | | | **+14,952** |
| Total | | | **+75,748** |

So USB alone (host, hubs, mass storage) was about 7.7 KB, and with FatFs about 19 KB. The sequencer and arranger work
was the biggest part, 37 KB, spread over twelve versions; its controls, screens and the three new pages (TRACKER,
PHRASES, GROOVE) are inside those figures and were not built separately. The steps share code (the card's reading is
used by playback, recording and freezing; the pages share the UI), so the parts do not add up independently; the sums
are what each stage cost on top of the one before.

Later: 0.5.0 MACRO +52,156 (with TRIO out), 0.5.1 SHAPES +43,992, 0.5.4 -13,264 (the unhooked engines' pages and a
fix), 0.6.0 effects +20,904 (542,664 to 563,568 first build), 0.6.1 folders and the browser +6,092.

## What enabling an engine costs today

Comparison builds on the baseline with one engine switched on (tools/orangecat_engines.sh), the linker regions enlarged
so the link completes (never installable). Flash is the image change; the RAM numbers are from the same build.

| Engine | Flash | RAM .data + .bss | Low SRAM (tables) | POOL (state, resident code) | Fits? |
|---|---|---|---|---|---|
| PROPHET | +30,236 (+34,336 against T1's baseline: P5_TANH comes back) | +336 | -7,204 | +0 (arena 49,376, just above the 49,152 minimum) | flash: no at the baseline, yes on the combined branch (see below); RAM: barely |
| FM6 | +34,752 | +13,568 (106,504 of 98,304: 8,200 over) | -3,980 | +4,104 | flash yes on the combined branch; RAM no without moving FM6's static data (patches, state) into POOL or the arena |
| DRUM (808, 909, SID kits) | +51,392 | +64 | -5,540 | about +33,500 (resident kernels and buffers; the arena would fall to about 20 KB) | flash no; RAM no without keeping its kernels in flash |

Phase 3's removal figures (FM6 39,528; Prophet 32,984; DRUM 53,600) were measured with the engine's UI, banks and
glue of that time; the restore costs above are the current ones.

## Combined with the GUI work

`size-audit-gui` is `gui-slim` (0.7.0 stages 1 to 5, image 520,976 B, POOL 176,768 B) with this branch's five product
commits on top. See "Checkpoint" at the end for its build.

## RAM

Nothing was changed. The findings, for the record:

- `.data` + `.bss` 92,904 of 98,304: trk 10,896 (the four tracks), recording_run 8,192, up_meta 5,376 (384 user slots'
  names and engines), pattern_stage 3,368, abuf 2,048, frzp 2,048 (the frozen phrases' table), momentary 1,668, dlo
  1,628 (delay state), cs 1,036 (card streams), scope_buf 1,024.
- `.pool` 218,394: the screen's canvases 71,040 (cv_primary_u 59,520, cv_alt 11,520; the GUI work takes most of this
  back), the card players' rings cp_pool 36,864 (four slots of 9 KB, lent to the effects when idle), the project staging
  proj_wire_u 27,904 (also the update entry's scratch), USB audio ua 12,360, the recorder 21,000 (recording 10,240,
  recording_pitch 4,096, recording_due 4,096, heap 2,304 + 2,048 + 2,048 ...), step_history 7,460, bank_buf 4,292 (a
  user preset bank; a union with the CZ-1's and Prophet's banks, of which up_bank_t is the largest member, so nothing
  to shrink), proj_x 4,096, ocj_gen 3,744, set_rec 3,680, three list buffers of 2,396 (lists, the live one; proj_lists
  and ocj_l, two staging copies of the same shape in pattern_store.c and oc_project.c; their lifetimes overlap during a
  load, so sharing them needs a trace of both pipelines).
- `.audio_text` 63,244: the whole sequencer is resident (seq.c 14,194, mix.c 9,794, voice.c 5,314, fm1_timer5_irq 5,036,
  recording.c 4,626, midi_control.c 2,380, card_freeze.c 2,274, mod.c 2,022, motion.c 1,904, pattern.c 1,786, audio.c
  1,542 ...) because tools/audio_ram.py places everything the audio interrupt reaches. That is a design choice for
  timing; moving parts of it to flash (`.text.xip*`, as the MACRO engine's models) would free POOL for the arena, not
  flash, and costs audio time when the code is not in the cache.
- Retired-engine state still in RAM, small: cz_patch 576 and p5_patch 552 (`.pool`, the patch each track stores, part of
  the project format), fm6_patch 624 (`.bss`), fm6_fn 64.
- The T1 change gave the arena 4,100 B in the low bank (audio resources 87,884 to 92,176 B with M2's 128 B).

## Stacks

A static bound from the built image: each function's frame from its prologue (the `sp += -N` and `[--sp] = {..}` of
its first ten instructions, build/melodee.dis) and the call graph of the final IR (direct calls, the flash/RAM far
calls, and the engine callbacks as edges from their dispatch sites). It is a bound, not a measurement: nothing records
the high-water mark on the unit; the hardware stack guards (fm1_guard.h) catch an overflow as a crash.

| Root | Stack | Static bound | Deepest chain |
|---|---|---|---|
| fm1_main (the main loop) | user, 24,320 B | 23,832 B | fm1_main 3,160 > ed_service 316 > ed_handle 3,216 > up_boot 12,248 > shared_wipe > up_save_bank 3,760 > up_cache_bank 660 > ... |
| fm1_main through the UI | user | 12,216 B | ui_input 1,952 > ui_confirm_accept > sysex_commit 5,084 > up_put > up_save_bank 3,760 > ... |
| fm1_alnk0_irq (the audio interrupt) | supervisor, 7,936 B | 1,716 B | mix_block > midi_event > seq_advance 144 > arp_step 320 > rec_note > ... (the effects' chain through fx_buses 260 and rev_cloud 384 is shorter) |
| fm1_timer5_irq (10 kHz: input, the card) | supervisor | 196 B | cs_step > cs_start |
| CPU1 worker | 4,096 + 4,096 | small (the worker's loop is inlined; the paired kernels are FM6's and the Prophet's, off) | |

The one finding: the preset banks' repair and migration at boot (shared_boot, inlined into up_boot) keeps whole
banks on the stack (`up_bank_t bank = *up_cache_bank(b)`, 4,292 B, and more in the migration part), 12,248 B in one
frame, and the same function runs from the editor's backup restore (ed_handle, itself 3,216 B) on top of the main
loop's 3,160 B: a static worst case of 23,832 B on a 24,320 B stack, 488 B short of the guard. Melodee has shipped
with it, so the real depth is less than the bound (the chain's deepest callee frames are not all live at once), but
it leaves no margin to grow. Staging those bank copies in POOL (bank_buf is the obvious home; its owner at that
moment must be traced first) would take about 8 KB off the worst case. sysex_commit's 5,084 B frame is the same
pattern. Not changed in this branch: it is a lifetime question, not a size one.

## The largest remaining costs

In the combined branch, flash:

1. The screen: ui_draw.c 51,437, ui_input.c 39,482, ui_pages.c 12,739, ui.c 11,718, gfx.c 9,104, ui_graph.c 7,888, ...
   (the GUI work).
2. SHAPES and MACRO: code 61,600 and tables 39,750. The tables are the biggest block of constant data in the image after
   the fonts. They are used at audio rate and are not duplicated (D1).
3. main.c 23,436; the web editor link 21,100; the sequencer 37,000; the effects 23,300; FatFs 11,941.
4. String literals: about 16,000 B.

## Where more flash would come from

Measured or counted; none applied here.

1. **The GUI work** (gui-slim): -49,968 B at stage 5, independent of this branch.
2. **Constant data in the data flash, read in place** (see the USB section below): the engines' tables 39,750 B,
   and with the GUI's agreement the fonts and icons that remain, the scales 4,832 B, the preset and category names.
3. **Moving the app area's end from 0x93000 to 0x9D000**: +40,960 B of body (the retired Prophet user banks' ten
   sectors, directly above the app area). It is a flash layout change: the stub (STUB_BODY_MAX, STUB_MARK_SEC 0x91000,
   its plain window at 0x93000: stub_core.h, stub.c), the update loader (ldr_core.c LDR_APP_HI, LDR_MARK_SEC, loader.c
   FL_RANGE_OK), fm1pkg_make.py (APP_SLOT, FLASH_SIZE, BODY_MAX, the partition entries), fm1_flash.h (FL_STORE_OK, the
   plain window), storage.c (the Prophet banks' objects move or go), tools/fm1_recover.py (APP_HI), the installer, and
   every test that knows the boundary. A stub change is the one update step the boot protection does not cover
   (BUILDING.md), so it needs the risky-tests unit, a full backup first, and a new stub version. Worth it only when the
   two items above are done and still short.
4. **Optional feature cuts**, not applied: ED_AUDIO_STATS 788 B (measured); reading projects older than FUN23 (Melodee
   1.x and Orange Cat 0.3 to 0.5 formats: proj_import 2,218 B and its helpers, about 2.5 to 3 KB by symbol); the sound
   designer's Prophet, FM6 and CZ-1 feature sets inside ui_designer.c (its generic part serves every engine; about 1 to
   2 KB); the SysEx preset library's native-dump paths; MENU_CREDITS 464 B (the credits are attribution and should stay).

## Using the USB drive to free internal flash

Two different things live in internal flash: the application body (what this audit is about) and persistent data above
0x93000 (projects, presets, the record store, the sample slot, settings). The card already takes the biggest persistent
data, the frozen takes and recordings. Moving more persistent data to the card frees data flash, not the app body; it
helps only together with item 3 above (the boundary move), which does not need the card at all.

What the card can do for the app body is carry a **data pack** that the firmware installs once into data flash and then
reads in place, so the image no longer holds it:

- The mechanism exists. The internal sample slot (smp_slot.c, 0.6.0) is 88 KB of data flash (sectors 0xB2000..0xC7FFF)
  written through the editor's ED_SLOT_* SysEx and played in place through the plaintext XIP window
  (fm1_flash.h fl_plain_window_init: flash above 0x93000 reads as plaintext). The same window reads any constant data
  put there, at the same speed as the application's own flash (the MACRO engine already runs from flash through the
  cache). The write path from a file on the card is the file browser (usb_files.c) plus smp_slot's sector writes.
- Candidates: the SHAPES and MACRO tables (39,750 B), the scales (4,832 B), the preset lists and category words, and,
  if the GUI work agrees, the remaining font and icon data. The engines' tables alone would about double the headroom
  of the combined branch.
- Room: the retired engines' bank areas are free data flash today: the Prophet user banks 0x93000..0x9CFFF (40 KB),
  the CZ-1 banks 0xC8000..0xD7FFF (64 KB), the native FM6 banks 0xD8000..0xDBFFF (16 KB). (The sample slot keeps its
  88 KB.) A pack area of 40 KB at 0xC8000 would hold the tables with room over; FM6's and the Prophet's own data
  would need the same treatment if they come back with user banks.
- Cost: the firmware must run without the pack (the engines silent with a message until it is installed, as a missing
  sample slot is handled), the pack carries the firmware's version and a CRC, each table becomes a pointer or a fixed
  address instead of an array (no runtime cost at a fixed address), and the installer or the web editor gets one more
  step. The timing of table reads through the data window under full audio load is a device measurement.
- What should stay inside: anything the instrument needs to boot, load a project and play a sound without the card.

So the order of worth is: the GUI work (done, 50 KB), the data pack on the card for the engines' tables (about 40 KB,
no layout change, a device test), and only then the boundary move (41 KB, a stub change).

## Tests, blockers, hardware

- Host suite: green on `size-audit` (run 39900421706, every test, the sound tests included; golden renders unchanged).
- Render-loop costs (tests/target_budget.py): every loop within its budget; three loops changed by one or two
  instructions outside their inner loops.
- Not measured on the unit: the card's write speed with FatFs built for size (M1; the stream that plays takes reads
  sectors itself and is unchanged), the UI frame time with the screen driver built for size (lcd.c, 448 B of code).
  Both are main-loop paths; the host tests cover their behavior, not their speed on the FM-1.
- Blocked: no local toolchain (JieLi's server and Gitee are not reachable from the audit's workspace); every build went
  through CI, about two minutes each without the host tests.
- The P5_TANH finding means every explicitly sectioned object shares a section with its neighbours (`.pool`, `.noinit`,
  `.dsp_text`, `.ram_text`, `.audio_text`): the linker cannot drop one of them alone. T1 fixed the tables; the other
  sections hold only live objects today, but a future unused `.pool` buffer would stay in RAM the same way.

## Checkpoint

- `size-audit`: the five product commits (T1, M1, M2, R1, R2) on main 19271b0, plus the temporary CI probes
  (.github/workflows/build.yml, tools/ci_blob.py, tools/ci_syms.py, tools/ci_audit.sh, tools/ci_probe.conf, .ci_quick).
  Drop the probes before merging, or keep .ci_quick off the branch so the host tests run.
- Experiment branches, never to be installed: exp/sa-tables, exp/sa-merge, exp/sa-minsize, exp/sa-retired,
  exp/sa-nostats, exp/sa-eng-base, exp/sa-eng-fm6, exp/sa-eng-p5, exp/sa-eng-drum (the engine probes enlarge the
  linker regions).
- `size-audit-gui`: gui-slim at 38ce0b6 with the five commits (one conflict, size_fns.py's file list, merged).
