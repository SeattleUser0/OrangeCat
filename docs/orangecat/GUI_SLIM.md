# GUI slimming: checkpoint

This file tracks the work to make the screen code smaller, so there is room for more synth engines and more
effects memory. It is the place to resume from. It is updated at each milestone.

## Revisions

- Start: `main` at 19271b0 ("Build doc 0.51: 0.6.2's fixes"). That is 0.6.2, with the Jiant effects and the clip
  browser.
- Work branch: `gui-slim` (not merged, not released).
- Experiment branches (pushed): `exp/a1`, `exp/a2`, `exp/b1`, `exp/b2`, `exp/c1`, `exp/d1`, `exp/e1`, `exp/e2`,
  `exp/f1`, `exp/f2`. E3 was tried on the host only. Each holds one idea on top of the start (b2 is on b1, e2 on e1, f2 on f1). They carry a `.ci_quick`
  file, so CI builds the firmware and skips the host tests.
- References: Jiant FM (https://github.com/juanjiant-bit/JIANT, GPL-3.0-only), Melodee
  (https://github.com/keremimo/melodee, GPL-3.0-only), the Felucca 0.8-beta history inside Jiant (5684e18).
- Build configuration: the shipping one (tools/orangecat_engines.sh): MACRO and SHAPES on; SID, CZ, FM6, P5 and
  DRUM off. Builds come only from the GitHub workflow (zvenson/fm1-build).

## How sizes were measured

- Every number marked "target" comes from the CI build of the real firmware (the shipping engine set).
- `tools/size_by_file.py` (new) reads the linked image's symbol table (`objdump -t`) and puts each symbol under the
  source file that defines it. This is exact for symbols. A function's size includes what the compiler inlined
  into it, so a file's total is where its code ended up.
- The old size report groups by symbol name and is only a guide.
- "Host" numbers come from the host test build (x86, -O1). Only their ratios mean anything for the device.
- "Asset" numbers are the bytes a generator writes.

## Baseline (target, start revision)

- Image 570,944 B of 573,440 B. Only 2,496 B were free.
- RAM .data + .bss 92,936 B of 98,304 B. Pool 218,400 B.
- Audio working arena (main bank) 53,984 B. All audio resources 87,884 B (102,220 B with the cache).
- Screen code and data by file (flash / RAM):

| File | Flash B | RAM B | Biggest parts |
|---|---|---|---|
| ui_draw.c | 51,327 | 39 | draw_page 29,904, ui_draw_frame 12,042 |
| ui_input.c | 39,146 | 85 | ui_input 22,802, edit_param 3,898 |
| ui_fonts.h | 31,705 | 0 | Rubik faces 27,680 with kerning 2,736; Melodee Pixel 2,287 |
| ui_pages.c | 12,401 | 8 | pv_draw 7,962 |
| ui_icons.h | 11,153 | 0 | 61 cells at 12 px, 34 at 16 px, 7 at 24 px |
| gfx.c | 9,104 | 74,382 | canvas 59,520, second canvas 11,520, screen diff 3,040 |
| ui_graph.c | 8,468 | 1,107 | |
| ui.c | 7,746 | 4,187 | undo and momentary state (not drawing) |
| ui_popup.c | 6,112 | 64 | |
| ui_designer*.c | 6,278 | 980 | |
| other ui_*.c | about 16,000 | about 300 | |

- The big canvas (240 x 124, 59,520 B) is also the scratch space where a saved project is decoded
  (`proj_scratch`, project.c). A decoded project is 17,028 B. Nothing else borrows it.
- Redraw: each screen part is drawn into a canvas and sent by DMA. A second small canvas (240 x 24) lets the next
  part be drawn while the last one is still being sent. A diff keeps hashes of what was sent, so only changed
  blocks go out again. The panel's SPI runs at 12 MHz: the whole screen takes about 77 ms.

## Research

Full notes from the research pass. Sources are the projects' own code and history.

- Jiant FM (GPL-3.0-only): fonts at 2 phases; glyph records packed (224fe41, -2.6 KB); kerning indexed (e7a30ff,
  -1.6 KB); keycaps Huffman-coded; icons removed and marks drawn with rectangles (cc939b4, -11.7 KB); one canvas
  only (240 x 150), no second canvas.
- Melodee: the large type (17412df) brought the four Rubik roles and Melodee Pixel; b1aa153 cut them from 53.5 KB
  to 31.4 KB; 12a0827 added the screen diff and the second canvas. Felucca 0.8 already had the 240 x 124 canvas.
- u8g2 (BSD-2 code): 1-bit run-length fonts, 5x7 in 804 B, helvB24 in 2,965 B.
- Dirtywave M8 / m8c (MIT): the whole tracker uses two 1-bit fonts of 566 B and 1,010 B.
- LVGL (MIT): its docs ask for a draw buffer of at least a tenth of the screen; compressed fonts draw about 30 %
  slower.
- Mutable Instruments avrlib (GPL-3.0-or-later): a character-level screen diff.
- Nothing was copied from these projects. The pixel face, the band drawing and the 1-bit icons are written here.

## Candidates and results

Target image size: smaller is better. Arena: the audio working arena (main bank); larger is better.
Each row is measured against the start (570,944 B image, 53,984 B arena) unless it says "on".

| Id | What | Image B | Flash change | Arena B | RAM change | Notes | Decision |
|---|---|---|---|---|---|---|---|
| A1 | Text in Melodee Pixel only (no Rubik faces, kerning, phases, Huffman; no FONT setting) | 536,040 | -34,904 | 53,696 | -288 (an audio ISR inlined more; not the screen) | Settings > FONT > PIXEL shows it on 0.6.2 today | Adopt |
| A2 | Rubik kept but small: LB, TX, VL at one phase, no kerning, no hero face, no pixel face | 546,688 | -24,256 | 53,984 | 0 | 10.6 KB bigger than A1; values shrink to 28 px without the hero | Reject (A1 smaller) |
| A2b | Rubik LB and TX only, one phase, no kerning | asset only: 6,997 B of fonts | about -29,000 | | | still 4.7 KB more font data than A1, and values at 20 px | Reject (A1 smaller) |
| B1 | Only the icons the screens draw (2 at 12 px, 14 at 16 px, none at 24 px) | 561,968 | -8,976 | 53,984 | 0 | no visible change | Adopt |
| B2 | Those icons at 1 bit, drawn like the pixel font (on B1) | 560,708 | -10,236 | 53,696 | -288 (same ISR effect) | blocky icons to match the pixel text | Adopt |
| C1 | One palette (NIGHT), no COLOR setting | 569,504 | -1,440 | 53,984 | 0 | the palette number is part of the web editor's protocol (UI_SET, 8 palettes advertised), backups and stored settings: 7 host tests broke | Reject (small gain, wide reach) |
| D1 | No rolling digits: values change at once | 570,068 | -876 | 53,984 | 0 | animation only | Adopt |
| E1 | Canvas 240 x 62 (was 124): taller parts drawn in bands | 569,948 | -996 | 84,416 | +30,432 | every screen pixel for pixel the same on the host; see below | Adopt |
| E2 | No second canvas (on E1) | 569,820 | -1,124 | 95,936 | +41,952 | drawing the next part waits for the last transfer; partial sends go a row at a time; host renders identical, fewer pixels sent in some cases | Adopt |
| E3 | Canvas 240 x 42 (host only) | | | | about +9,600 more | 3,552 canvases cut in the host run: 12 more places need bands, the graph drawn 3 times | Not now |
| F1 | Build the newer screen files for size (ui_boot, ui_view, ui_tracker, ui_pool, ui_groove, card_browse, seq_tools, shared_presets, oc_project, tracks, editor_library) | 568,848 | -2,096 | 53,984 | 0 | slower code only in the main loop | Adopt |
| F2 | Keep shared screen helpers out of line (on F1) | 568,784 | -64 more | 53,984 | 0 | | Reject (no gain) |
| G | Compile out retired-engine screen tables | not built | | | | P5_PANEL (1,760 B) is read by the stored-automation check (motion.c:65-83) and the editor protocol (editor_prophet.c:24); screen branches are already behind MELODEE_P5 / CZ / FM6 constants (params.c:734, ui_stage.c:95), so the compiler drops them | Reject (would change what stored projects keep) |
| H | Pixel glyph records packed (8 B to 4 B) | in the cumulative build | asset 2,287 B to 1,447 B | | | the condensed 1 also made tabular (a blank column) | Adopt |
| I | 8-bit indexed canvas | not built | | | | rounded corners, discs and the wave trace blend colours into the canvas (gfx.c mix565 over the canvas); DMA cannot expand indices, so every send needs a conversion pass and line buffers; E1 frees the same RAM with no change on screen | Reject |
| J | Faster SPI clock | not built | | | | cannot be checked without the unit; a wrong clock can garble the screen | Reject for now (a device test) |

### E1 details (band drawing)

- `CV_BANDS(x, y, w, h, bg) { ... }` (gfx.c) runs the drawing once per band of the canvas's rows. The
  primitives subtract the band's first row (`CV_OY`), so each band gets exactly its part.
- 10 places were changed to bands: the graph, the page panel, the menu and its document, the Mixer's scale panel,
  Stage's shape, NEW SONG's key, the chord wheel, the groove bars, the designer's engine name, the value picker.
  Six slice loops now follow the canvas size instead of a fixed 124.
- 62 rows was chosen so the bands of a 124-row graph meet at the same row the old slices did.
- A glyph wholly outside a band is not decoded. Text-heavy screens got faster than before (credits -11 %).
- Host draw cost of a full redraw (us, start to E1): home 310 to 355, presets 240 to 305, step 165 to 167,
  menu 140 to 149, about credits 327 to 294, confirm 1,408 to 1,411.
- Host renders: all 1,122 screen images are the same as the start, byte for byte. The lint finds nothing.
- The decoded project (17,028 B) still fits the canvas (29,760 B). The static check in project.c stays.
- The screen diff now keeps 13 places of 48 slots (was 9 of 76), 2,688 B instead of 3,040 B.
- The host lint now reports any canvas asked taller than the canvas holds (a part that would be cut).

## Selected design and why

- A1 + H: the biggest flash win (about 35 KB) and the safest: the pixel face was already a shipped choice and the
  host lint already drew every screen in it. A2 kept anti-aliasing but cost 10.6 KB more.
- B1 + B2: 10.2 KB, and the icons now match the pixel text.
- E1 + E2: 41.6 KB back to the audio arena. Every screen drew the same pixels with bands (checked on the host before
  the font change). The cost is CPU and overlap, not memory: see "Costs".
- D1: 0.9 KB and fewer screen sends while a knob turns.
- F1: 2.1 KB, no behaviour change.
- Rejected: A2 (A1 smaller), C1 (protocol reach), E3 (more band work for 9.6 KB, not now), F2 (no gain), G
  (compatibility readers), I (complexity, same RAM as E1), J (needs the unit).

## Cumulative results (target, CI)

| Build | Image B | Free flash B | .data+.bss B | Main bank B | All audio B (cache ready) |
|---|---|---|---|---|---|
| 0.6.2 (start) | 570,944 | 2,496 | 92,936 | 53,984 | 102,220 |
| F1 + E1 + E2 (stage 2, 6c39402) | 567,692 | 5,748 | 93,032 | 95,936 | 144,076 |
| All (stage 5) | 520,976 | 52,464 | 92,728 | 95,616 | 144,060 |

- Screen code and data (the size report's group): 121,046 B flash and 71,675 B RAM to 95,527 B and 30,451 B.
- Fonts 31,705 B to 1,527 B; icons 11,153 B to 746 B; gfx.c 9,104 B to 7,480 B (RAM 74,382 B to 32,528 B).
- RAM code (.audio_text) 63,420 B to 63,406 B. Stacks and the cache split are unchanged (CPU1 8 KB; cache 14,336 B UI
  and 14,336 B audio).
- Net memory for engines and effects: the main bank grew 41,632 B, the data tail 208 B: 41,840 B in all.
- draw_page grew (29,904 B to 35,118 B): the compiler inlined more into it once the files around it shrank. It is
  the same code in one place, not a copy.

## Effects' memory (host, tests/jfx_arena_test.c, the device's bank sizes)

Rows with FX FULL in 0.6.2 (main 53,984 B): 16 of 26 (the delay in all of them, more in the worst). In 0.7.0 (main
95,616 B): 1 of 26, the new worst case (four of the largest engines, no cache, four takes, CLOUD and the stutter: the
delay and the chorus). Every case with frozen phrases playing (1 take, a freeze, 4 takes) now holds the delay.

## Costs

- CPU: a region taller than 62 rows is drawn once per band. Host draw cost of a full redraw (us): home 310 to 355,
  presets 240 to 305, menu 140 to 149, about credits 327 to 294 (glyphs outside a band are skipped), confirm 1,408 to
  1,411. Only the ratios mean anything for the device.
- With one canvas, drawing the next part waits for the transfer before it. The SPI was already the bottleneck (a
  full screen about 77 ms); what is lost is the drawing that used to happen during a transfer. Not measured on the
  unit.
- Look: pixel text everywhere, blocky icons, numbers jump instead of rolling.

## On the unit (left to test)

1. Install 0.7.0 (the nightly from branch gui-slim). Turn it on: the boot screen and HOME draw cleanly.
2. Change pages fast (SEQ, NOTES, TRACKER, MIXER, FX, the menu). Look for torn or half-drawn panels, and for any
   page that now feels slow.
3. Turn a knob fast on GLOBAL (TUNE) and TEMPO (BPM): numbers follow at once, no smears.
4. Open the menu's document (ABOUT, scroll the credits): it scrolls smoothly.
5. Play a chord on the chord wheel (VIEW): the ring and the chord name draw whole.
6. Assign a clip (MIXER sheet, Assign clip), freeze a phrase, play frozen phrases: the screen keeps up while the card
   streams.
7. Save and load a project, make a backup and restore it (the canvas is still the decode scratch).
8. With four tracks of SHAPES or MACRO and frozen phrases playing, turn on DELAY, REVERB and CHORUS: none should say FX
   FULL.
9. Listen for clicks or dropouts while paging fast during playback.

## Build and test state

- gui-slim stage 5 (c.f. git log): CI build and all host tests pass (run 39900238037). Locally the host tests pass
  except the ones that need the firmware image (web, boot packages, power cuts, loader, installer, ROM recovery);
  those pass in CI.
- Not merged to main. Not released. No hardware tests.

## Resume

```
cd /home/claude/oc-gui            # branch gui-slim
gh workflow run build.yml --ref gui-slim -R SeattleUser0/OrangeCat
gh run list -R SeattleUser0/OrangeCat -L 5
# a run's notices (sizes by file, the build checks):
gh api repos/SeattleUser0/OrangeCat/check-runs/<job id>/annotations?per_page=100
# host renders and lint:
gcc -O1 -w -DMELODEE_USB_AUDIO=1 -Ibuild/gen -Ifirmware/src -Itests -o /tmp/ui_render tests/ui_render.c -lm
mkdir -p /tmp/r/ppm /tmp/rs && /tmp/ui_render /tmp/r /tmp/rs
```
