# Orange Cat: the new interface (build log)

**Version:** 0.8
**Date:** October 29, 2026
**Branch:** ui-build (Session C), cut from release-2 e1c8ac3 with ui-design 8dc42a4 merged in. Not on release-2, not on
main, not on the unit.
**Tracks:** docs/orangecat/UI_DESIGN.md 0.4 (the owner's decisions of October 29, 2026).

This file is the build log of the new interface: what is done, what is left to build, what is left to test. It merges
into BUILD.md when the interface merges (Session A owns BUILD.md; this branch does not edit it). Status keys as in
BUILD.md: **Done**, **In progress**, **Not started**, **Blocked**.

Revisions:
- 0.8: steps 4 (the step pages, OC_STEP_PAGES), 5 (DRUM-X's LANES table, OC_LANES) and 6 (the mixer's colors, OC_MIXER) built, reviewed
  and merged: the designs as built, the reviews' findings, flash and RAM, the budget table with the measured costs; step 7a, 8 and 9
  not built (see "Where the plan stops"); the hardware checks 13 to 18; the decisions.
- 0.7: release-2 2b46f80 merged again; flash and RAM re-measured with CI's build options (the 0.6 figures used the
  defaults and were wrong for the target: see "Budget revision 0.7"); the effects' and DUB TAPE's picture pages taken out
  of the budget (Release 3: each effect a module with its own picture); step 4's design review and its fixes.
- 0.6: release-2 876955a merged into ui-build (DUB rounds 4 to 6, the key and LED scan change, converter B, the sized
  shared DELAY); flash and RAM re-measured on today's release-2 and with every switch; the flash budget for steps 4 to 9.
- 0.5: step 3 (the new interface 0.3), HOME as SHIFT and its map behind OC_SHIFT_MAP: the design note, its independent
  review and the design as built; the code (firmware/src/ui_shift.c and its hooks), its diff review and the fixes (the
  edge-only tap, a button held before HOME, the toasts); flash and RAM measured after the fixes; the full host suite;
  the mockups; what is left for later.
- 0.4: step 2 (the new interface 0.2), EDIT's sheets behind OC_EDIT_SHEETS: the design note, its independent review and
  the design as built; the code (firmware/src/ui_sheets.c and its hooks), its diff review and the fixes (the frame cost,
  the FX link from a shared effect, the tests' oracle); flash and RAM measured; the mockups; the decisions and what is
  left for later.
- 0.3: step 1 complete (the new interface 0.1): 1a, 1b and 1c built behind OC_HOME_PIC, every diff reviewed and the
  findings fixed, the full host suite run (release-2's own target-cost failure only, see "Step 1's full host suite"),
  flash and RAM measured. The mockups moved to docs/orangecat/ui/new/ (the repo's .gitignore drops any `build/`).
- 0.2: step 1's design note after its independent review (the findings and what was done, below each part); 1a built
  (code and host tests, before its diff review); 1b redesigned: the scenes in a trailer after the pack's tables, outside
  the table hash, so a switch-on firmware keeps release-2's pack and modules.
- 0.1: first version. The baseline; the owner's decisions; step 1's design note (1a the knob macros, 1b the scene
  interpreter and the scenes in the asset pack, 1c HOME's picture page).

## Ground rules of this branch

- Everything new is behind the build switch **OC_HOME_PIC** (default 0; `OC_HOME_PIC=1 ./build.sh` builds it in). With
  the switch off the code is not compiled and release-2's screens, sound and image stay as they are (checked: section
  "Baseline").
- No change to Session A's files: firmware/src/audio*, mix.c, jfx*, voice.c, audio_batch*, audio_worker*, core_stats*,
  the DUB files, tools/hw/, BUILD.md. Where the interface needs one, it is written down here and that item stops.
- Builds and tests run under `nice -n 19 ionice -c3`, one at a time (the host suite with PARALLEL=1), on the host only:
  the unit on USB is Session A's.
- Every step: a design note here, reviewed by an independent agent; host tests; the full host suite; mockups of every
  screen it changes, linted; its flash and RAM against release-2; an independent review of the diff before it is merged
  into ui-build.

## Baseline (release-2 e1c8ac3, built here with tools/orangecat_engines.sh)

| Measure | release-2 |
|---|---|
| Image | 537,248 B of the 577,248 B app area: **40,000 B free** (the brief said about 65 KB; that was the figure of an earlier image, BUILD.md "Free flash is the audit's limit") |
| RAM .data + .bss | 93,624 B of 98,304 |
| Data bank (the SRAM tail after .bss) | 4,680 B: **584 B above one 4 KB arena block**. That is the budget of this branch for new .data and .bss in main RAM |
| Audio working arena (main bank) | 142,976 B available, minimum 142,336: 640 B of margin. New `.pool` or `.audio_text` takes from it |
| Pool | 192,836 B of 344,064 |
| Asset pack | 57 tables, 40,232 of 40,960 B, hash 657ED950, 728 B spare |
| Audio-path placement | `throw_on`, `throw_off` and `mix_block` are in XIP flash in this build (0x0201xxxx); `.audio_text` is 60 B. New code beside the throws costs flash, not SRAM |

## The owner's decisions (October 29, 2026)

Written into UI_DESIGN.md 0.4 as decided:
1. HOME B: the picture, four colored dials and one row of small values under them.
2. The four knobs per engine: section 2's table, approved for now. The owner judges them by playing; each engine's knob
   table is in one place (firmware/src/oc_knobs.c) and easy to change.
3. Per-note ties: no. Notes keep their own lengths through the recorded-note pool.
4. The SHIFT map under HOME held: yes. HOME pressed alone opens the MENU (step 3).
5. Pictures of engines built into the firmware (MACRO, DRUM-X, SHAPES' built-in copy) and of the effects ride in the
   asset pack until they are modules; the firmware image holds none.
6. Parameter ids: the knob macros take P_RESERVED33..36. DUB TAPE (release-2) uses P_JRES0..6 and P_RESERVED45..47.
   None of those is reused.

## Step 1: HOME's picture and the four knobs

UI_DESIGN.md Part 3, step 1, built in three parts, each reviewed and merged on its own, all behind OC_HOME_PIC.

### 1a. The knob macros (design note)

**What.** `P_MAC0..P_MAC3` are `P_RESERVED33..36` (core.h, `#define`s; the enum is not touched). A macro is a stored
offset m, -64 .. 63, 0 the sound as stored. Each built-in engine with a picture has a knob table, `knob_def_t
kdef[4]`, in **one file, firmware/src/oc_knobs.c** (the owner's "easy to change in one place"; no engine file changes):
a label, a kind (plain: a parameter id; macro: up to four targets `{id, shape, depth}`), the table of UI_DESIGN.md
section 2 for MACRO (8), SHAPES (14) and DRUM-X (9). Shapes: LIN `stored + depth x m / 64`, UP `stored + depth x
max(m, 0) / 64`, DOWN `stored + depth x min(m, 0) / 64`, each clamped to the target's range (`param_desc_of`: the same
range the editor and the stores use). Integer arithmetic, C division (toward 0).

**Applied in the audio interrupt, once a block, as the throws.** New file firmware/src/oc_macro.c, included by
oc_throw.c (which mix.c includes). `mac_on()` runs at the end of `throw_on()` (after the block's events, so motion's
base never takes a macro in; before `mix_prepare_batch`, so the batch's parameter copies for CPU1 carry it);
`mac_off()` runs at the start of `throw_off()` (after the block is mixed). mac_on, for each track in use whose sounding
engine is the one asked for (`t->engine == t->eng_req`: no engine fade under way) and has a table: for every macro knob
with m != 0 and every target, keeps the stored value, puts `clamp(keep + delta)` into `t->p[id]` and marks the target.
mac_off walks the marks in reverse and puts the kept value back only where `t->p[id]` still holds what was put (a value
written meanwhile stays, as with the throws). Reverse order makes two macros on one target exact. The main loop never
runs inside the interrupt, so what it shows, saves and edits is the stored value.

**The call site without touching mix.c.** mix.c calls `throw_on` only `if (thr_out)` and `throw_off` only `if
(thr_on)`. The macros arm through **bit 31 of `thr_out`** (`MAC_ARM`); the throws use bits 0 .. 11. throw_on's loop
stops at bit 11 (unchanged); `throw_off` handles bit 31 and masks it before its own loop (it would otherwise index
track 10); `throw_stop` (a stop, a project load) keeps the bit. The bit is set and cleared by `mac_poll()`, called once
a main-loop pass from `ui_input()` under `motion_guard`: set while any track in use has a table and a nonzero macro.
This is a workaround for one line in mix.c (`if (thr_out || mac_armed) throw_on(n)`), which is Session A's to choose
at the merge into release-2.

**RAM (data bank).** Per track: 16 kept values (int16), a 16-bit mask of the targets put on, the four offsets as put
and the engine the table came from: 40 B a track, **160 B** of .bss (measured: tests/oc_macro_test.c prints it); the
four macro descriptors of HOME's knobs, 80 B (20 B each on the target). The macros themselves are track parameters (no RAM).

**Cost in the interrupt.** Nothing when no macro is off center (the bit clear: no call). Armed: per off-center macro
up to four targets of a descriptor lookup (`param_desc_of`), a multiply, a divide, a clamp and a store; back, the
value recomputed, a compare and a store; and `throw_on`'s own loop over the twelve throws runs on every armed block.
**Measured on the host: 642 ns a block** for four SHAPES tracks with three macros each turned (36 targets; about 2,000
host cycles), so on the device expect several microseconds a block, more with XIP misses (the first estimate, under
200 cycles, was several times too low: the review). **For Session A's review and timing before any merge into
release-2** (listed below). If it is too dear, the put value and the target's range can be cached per target (about
64 B more a track).

**Persistence before the Release 3 format revision.** project.c and upreset.c zero P_RESERVED33..36 on every load
(`param_reserved`), because old projects of the formats before Orange Cat's effects hold Melodee's inert effect values
there. Without a project format change a stored macro cannot be told from such a value. So until the revision:
- **Loads:** unchanged. Old projects, user sounds and templates load with every macro at 0, as they sounded. A sound
  load (factory preset, user preset, INIT, engine change) also sets the four to 0 (they are not `param_kept`).
- **Saves commit the macros ("bake").** The project, the template and a user sound save each target as it sounds,
  `clamp(stored + delta)`, and the macro itself as 0 (`mac_saved_value()`, a wrapper of `motion_base_value()` at the
  three save sites). A saved sound reloads sounding exactly as it was saved, with its macros back at the center. The
  live track keeps its offsets until a load.
- **Motion:** a macro is not recorded (`motion_param` refuses the ids, unchanged) and old inert events on those ids stay
  skipped (motion.c, unchanged). **A target that a motion lane plays is left alone by the macros, live and in a save**
  (the review: motion writes it absolutely every step and its saved events carry no offset, so a macro over it would
  sound one way now and another after a reload). The macro still moves its other targets.
- **Track removal** (tracks.c) captures the project and reloads every track: the sound is kept (the macros are
  committed) and every macro returns to the center. Noted, tested with 1c's UI tests.
- **Editor:** SET of a reserved id stays refused (editor.c, unchanged), so the web editor cannot turn a macro yet.
  ED_DUMP and ED_TRACK_DUMP report the stored values and the raw offsets on 33 .. 36 (-64 .. 63): **for Session D**, a
  sound saved from a dump in the web app loses the offsets (the unit zeroes them on the put); the web side must accept
  -64 .. 63 on those ids. Listed under the format revision.

**What the Release 3 format revision must add** (with project-owned phrases, Release Plan section 5): a new project
magic and user sound version; at conversion of the older formats, drop every motion event on P_RESERVED33..36 and zero
the four values; in the new formats keep P_MAC0..3 on load (project.c, upreset.c, templates: `param_reserved` no
longer covers them for the new formats); save the macros as they are instead of baking; let motion record and play
P_MACk (`motion_param`, the playback skip and the stored event check in motion.c); let the editor SET and GET them
with their descriptors (editor.c, the web tables, Session D).

**Rules for a future knob table** (from the review; to go into ADDING_AN_ENGINE.md with step 1): a macro's targets must
be read by the engine at render time, not latched at note-on (the events run before the macros go on), and must not
decide the engine's cap, units or voice count. Step 1's targets pass: MACRO's and SHAPES' note-on hooks read no
parameter, the envelope is read every tick (voice.c env_tick).

**Tests (host).** tests/oc_macro_test.c, over tests/hostsim.c (the firmware's engines and mix_block) built with
OC_HOME_PIC=1:
- every macro at 0: the renders of MACRO, SHAPES and DRUM-X presets bit-identical to the same build with no table
  (and the goldens of the full suite unchanged with the switch off);
- on and off over a block leaves every stored value exact, on every preset, for m at -64, -1, 1, 63;
- a macro at m renders bit-identically to the same preset with each target set by hand to the table's value
  (`clamp(stored + delta)`);
- a value written into a target during the block stays (the throws' rule);
- two macros on one target (constructed table) come off exactly;
- throws and macros together: each comes off exactly; a stop keeps the macros armed;
- an engine fade (eng_req != engine): nothing applied;
- the bake: a project and a user sound saved with macros turned reload to the same parameter values the interrupt put
  on (and the macros at 0); an old project with nonzero values on P_RESERVED33..36 loads with the macros at 0.

**The diff review of 1a** (a second independent agent, October 29, 2026): no bugs. With the switch off the build is
release-2's (every addition under `#if OC_HOME_PIC` but two defines); the tentative definition of `motion_active` matches
motion.c's exactly; the reverse removal, the index arithmetic, the races and the fade guard were found correct. Fixed after
it: a static assert that every id fits below KD_NOT and a host check (IDS) that every target is a parameter and never a
macro's own id (a typo would have written past `t->p` in the interrupt); one table lookup everywhere (`kdef_of(eng_req)`);
`mac_desc` called once in track_desc; P_MAC1, P_MAC2 defined; INIT tested through `sound_init_of`; the crossfade case
(xf_on) tested alone; SAVED run on a third track too; THROWS asserts its target can move; comments corrected (block counts,
the switch's scope, a stale line, the descriptors' 80 B).

### 1b. The scene interpreter and the scenes in the asset pack (design note)

**The interpreter** moves from tests/ui_scene.h into firmware/src/oc_scene.c (included from ui_draw.c under
OC_HOME_PIC), unchanged in what it draws, plus a checker. tests/ui_mockup.c includes the firmware's copy from then on,
so the mockups and the unit draw with one interpreter. Its working state (three group transforms, the weight) is
static, about 90 B of .bss (data bank).

**The checker, `scene_ok(s, max)`,** walks a scene with the interpreter's own length rule before every draw (about 200
bytes, a few microseconds): version 1, the design box 240 x up to 132, every op known and whole inside `max` bytes and
inside 1 KB, at most four BINDs before an op, PUSH and POP balanced and at most two deep, polylines of 2 .. 16 points,
sources under SS_N (with SS_CENTERED) and fields known. A scene that fails is not drawn: the plain picture shows.
The interpreter trusts what scene_ok passed (on its own it could read past END: a polyline's count, an unknown op). Its
cost is bounded too (0.2, the review): REP draws at most 32 copies, a disc's and an arc's size at most 255, every source
clamped to -256 .. 256 (so every product stays in 32 bits); a pathological scene that passes can still cost tens of
milliseconds a frame (the fuzz's slowest under the sanitizers, about 50 ms on the host), never an endless one.

**The scenes in the asset pack: a trailer after the tables (0.2, after the review).** The first plan (the scenes as
ordinary pack tables) changes the pack's table hash and length, and the firmware refuses a pack whose hash is not its
own (oc_pack.c) and every module built against another pack hash (oc_module.c): a switch-on firmware would refuse
release-2's pack and modules, and MACRO and SHAPES would go silent. So the scenes go in a **scene trailer** after the
tables, which the table hash, the length and the payload CRC do not cover:
- tools/gen_pack.py `--scenes firmware/src/oc_scenes.h` (build.py passes it when OC_HOME_PIC=1) lays the tables out
  exactly as without it, then at the length rounded up to 4 writes the trailer: magic "OCSC", version 1, the count, its
  length and its CRC-32, then (engine number, offset, length) per scene and the scene bytes. The pack's header is not
  changed, so `OC_PACK_HASH` and `OC_PACK_LEN` are release-2's: **release-2's firmware accepts the new pack** (it reads
  nothing past the length) and **the switch-on firmware accepts release-2's pack** (no trailer: the plain picture).
  Modules built against release-2's pack are accepted by both.
- The scene bytes are written by a host program that includes oc_scenes.h (the compiler's bytes, as the tables are).
  The loader's sector-tail rule is tested over the whole image, trailer included; the scenes move on by 4 bytes inside the
  trailer (0xFF padding after the entries) until no sector tail matches.
- The firmware (oc_scene.c) reads the trailer in place at `OC_PACK_LO + OC_PACK_LEN` (rounded), only when
  `OC_PACK_READY()`; it checks the magic, the version, that the trailer lies inside the pack area, and its CRC-32 at
  every lookup (about 600 bytes, a few microseconds a frame, no state to go stale), then finds a scene by engine number with its real length (the review: a pack table's
  length cannot come from sizeof on the device). On the host the scenes are oc_scenes.h's arrays.
- Size: the three scenes are 579 B and the trailer's head and table 34 B, 613 of the 728 B spare: the pack keeps its 10
  sectors.

**Installing a pack with its trailer (0.2).** The unit's install path takes exactly OC_PACK_LEN bytes today
(oc_pack_begin, pk_image_check, the card's fb_pak_file). With the switch on it takes OC_PACK_LEN up to the area
(`OC_PACK_MAXLEN`, oc_pack.h): the header's length must still be OC_PACK_LEN, the header's payload CRC is accumulated over
the tables alone, the sector tails are tested over everything written. With the switch off the code is release-2's. The
web installer (ED_PACK_INFO reports OC_PACK_LEN) can keep sending the first OC_PACK_LEN bytes, which are release-2's
pack: the unit then has no pictures, nothing else changes.

**The diff review of 1b** (a third independent agent): no bug that reads or writes out of bounds; the trailer's writer and
reader agree byte for byte; the install path with the switch off is the old code. Fixed after it: the sources and sizes
clamped (above); the pack's address through FL_XIP (oc_pack.c `pk_xip`) instead of a generated define, so the tables'
header is the same with or without scenes (checked by run_tests.sh); a set's install puts its pack in again when the file
carries a trailer and the area has none (`pk_trailer_ok`); a test of the switch-on install path (tests/
oc_pack_trailer_test.c: whole, a table byte refused, a trailer byte installed but refused where read, the tail rule,
lengths, the image check); oc_scene_test.c's buffers of exact size (so the sanitizer sees a read past a scene's end),
structural trailer faults with the CRC made good again, a real 17-point polyline and an open group, scene_of and the pack's
gate (a second build with `-DOC_PACK_GATE=1`), a looser time bound; run_tests.sh compares the pack the build's switch asks
for; comments (the layout's padding, the counts).

**Built (0.2):** firmware/src/oc_scene.h (the format), oc_scene.c (the interpreter, `scene_ok`, `scene_in_trailer`,
`scene_of`), oc_scenes.h (the three scenes and their table); tests/ui_scene.h now includes the firmware's interpreter (the
67 mockup screens redrawn with it are pixel-identical to the committed ones); tools/gen_pack.py `--scenes` (without it
every output is byte-identical to release-2's: checked against build/melodee.pak); the install path above; tests/
oc_scene_test.c. **Found by the fuzz and fixed:** the interpreter ended only on a byte of exactly 0 while the checker
ended on any op 0 (0x20 is op END with a color): the walks parted and a source index ran out of its array; both now
read op 0 as the end, and the checker refuses an END with color bits. Also changed: the coordinates' `<< 4` on negative
values (undefined in C, the sanitizer) became `* 16`.

**Flash.** The interpreter about 3.3 KB (the mockup's estimate, to be measured), the checker about 0.3 KB, a table of
three scene pointers.

**Tests (host).** gen_pack.py: with `--scenes` the tables' bytes, hash and length are those without it; the trailer
parses back and its CRC holds. tests/oc_scene_test.c: the firmware's trailer reader on made packs (none, a bad magic, a
bad CRC, a trailer running past the area, an engine not there); `scene_ok` accepts the three scenes and every scene of tests/ui_scenes.h;
refuses truncations at every length, an unknown op, five BINDs, unbalanced or three-deep groups, a polyline of 0, 1 or
17 points, a source or field out of range, a missing END, a box over 240 x 132; a fuzz of random byte changes never
reads out of the buffer (built with the address sanitizer in the run) and never draws outside the region;
a pack-missing render needs `-DOC_PACK_GATE=1` (the host builds have the gate off).

### 1c. HOME's picture page (design note)

**The switch at run time.** MENU > **HOME PAGE**: TRACKS (default) or PICTURE, beside HOME SELECT, kept in
`favorites.factory[15][24]` bit 6 (documented free in favorites.c), persisted with the other menu settings. TRACKS: HOME
is today's (Stage and the chord view), exactly. PICTURE: HOME has three pages, PICTURE (the landing page), TRACKS
(Stage), CHORD (the chord view); HOME tapped on HOME steps through them (today it toggles two); the header's page dots
show three.

**Encoding (after the review):** `ui.home_pg` 0 Stage, 1 the chord view, as today, and 2 the picture: with the switch
off or TRACKS chosen nothing reads 2. Every reader is changed together (ui_input.c's HOME tap, ui_draw.c's header
signature, its dots and their room check, widened to three, the page dispatch, ui_stage.c). HOME from another page keeps
landing where it was (go_home); with PICTURE chosen HOME from another page lands on the picture, as UI_DESIGN.md
section 1 has it. Choosing TRACKS again commits the selected tracks' macros into their targets (with the interrupt
off, as a save does) and sets home_pg to 0, so no offset is left where no knob shows it. It commits every track's (all
four have knobs on HOME), puts a momentary take back first, and tells the editor (a reload). The menu opened from HOME's
TRACKS or CHORD page closes onto the picture (HOME lands there with PICTURE chosen).

**Decision (safer reading):** SELECT keeps today's HOME SELECT setting (BPM by default) in step 1. The design's
"SELECT steps HOME's pages" comes with step 3, when HOME alone opens the MENU (the owner's decision 4) and the pages
need another way in. (Superseded by step 3's design review: HOME's tap still steps the pages, so they need no other way in yet;
SELECT on HOME is re-deferred to step 9.)

**The four knobs.** With PICTURE chosen and an engine with a table, Stage's knob page (`stage_page`) gives the table's
ids: a plain knob its parameter, a macro knob P_MACk. The whole edit path stays as it is (`edit_param`, the hot card,
undo of a sound edit). `track_desc` gives P_MACk the knob's label and the range -64 .. 63 (default 0) under the switch;
`TP[]` (the stores' and the editor's view) is unchanged. An engine with no table (a module, until step 7) keeps today's
four knobs and shows the plain picture.

**The page (HOME B, UI_DESIGN.md Part 2, `home_B_*.png`).** The shared header (draw_head, unchanged but for the three
dots); the sound line (rows 30 .. 54: the track's numeral on its color, the sound's name `sound_name`, a factory preset's
place "n/N"; while a knob turns and ST_KNOB_MS after, the knob's name in its color and its value; PACK MISSING in REC's
color when the pack is not there); the dials (54 .. 86: a ring in the knob's color, the value's arc, from the left end
for a plain knob and from the top for a macro, a pointer); the value row (86 .. 108: each value under its dial in the
knob's color, the knob turning in text color); the picture (108 .. 240, 240 x 132, drawn in bands of the one canvas).

**The knob colors** (`T_COZY(k)`, gfx.c, under the switch): blue, green, white, orange, the cozies', on the dark
palettes (NIGHT's values from the mockup); on the light palettes (DAY, CANDY) deeper shades of the same hues and the
"white" knob in the palette's text color, since white cannot be read on paper; the test palette GRAY in grays. Used on
the picture page and in the scene's K1..K4 slots. **Decision (safer reading):** today's track-shaded knob colors
(`T_KNOB`) stay on the other pages in step 1: T_KNOB also colors things that are not knobs (list rows, octaves, lanes),
and the pages change in step 2 (EDIT's sheets). A host test checks every cozy color at 3:1 or more against its
palette's ground (the palettes' own rule for a graphic).

**The scene's sources** (filled each frame from the selected track): K1..K4 the knobs' positions (a plain knob across
its range, a macro 128 at the center); ENV the loudest voice's `env_out`; HIT 256 at a note-on falling to 0 over 160 ms
(a note-on seen as a voice `age` newer than the last frame's); GATE any voice held; LFO `lfo_val`; NOTE the last note's
pitch class; TIME, WOB the UI's clock; PH1..PH4 four 16-bit phases at 0.1 .. 3 Hz by their knob, of the selected track
only (a track select starts them from 0, invisible), and a macro's K source is 128 + 2 x its offset. SCOPE: the
output's scope buffer (`scope_buf`, what HOME's waveform shows today: the master output, not the track alone; noted).
RAM: one set for the selected track (the phases, the last age, the hit time and note, the found scene): about 40 B (data
bank). A note-on is a voice age newer than the newest seen (the voice clock only grows, so a newer voice freed first is
not one); the first frame on a track takes what sounds as seen.

**Rate.** The picture draws at **10 frames a second** at most (the budget UI_DESIGN.md section 3 measured): it redraws
only when its signature changed and 100 ms have passed since it last drew (a full redraw on `ui.force`). The dials,
the value row and the sound line are redrawn when they change, at once (a knob's answer is not held back by the
picture's rate: the review); they are small. A still page sends nothing (the screen diff).

**Tests (host).** In tests/ui_render.c under OC_HOME_PIC (a second build of the renderer, run by run_tests.sh): HOME
PICTURE on MACRO, SHAPES and DRUM-X, each knob at both ends and at the center, a knob just turned, a note on, the pack
missing, a module with no table, in every palette: the lint (nothing off the screen, cut or overlapping) and MONO gray.
In tests/ui_test.c under the switch: the menu switch persists; with TRACKS nothing on HOME differs from release-2 (the
same pixels); with PICTURE HOME's tap steps PICTURE, TRACKS, CHORD; the knobs write the declared ids (a plain knob its
parameter, a macro P_MACk within -64 .. 63); a sound load sets the macros to 0; an engine without a table keeps
today's knobs. The pixels sent per frame over two seconds of play, against the design's budget (mean under 15 ms of
SPI a frame, worst under 30 ms). Mockups: the PNGs of those screens in docs/orangecat/ui/new/.

**Deviations from UI_DESIGN.md, said (the review):** MENU > HOME PAGE, not MENU > DISPLAY > HOME (the menu has no
DISPLAY section); the scenes in a trailer after the pack's tables, found by engine number, checked by CRC once and walked
before every draw, instead of a scene table checked at boot; the SHAPES module gets the built-in SHAPES' knob table
(the same engine: kdef_of goes by engine number, 14); an OC_PACK_BUILTIN build (an experiment's switch) has no pack and
so no scenes: the plain picture.

**RAM of all of step 1, planned:** the macros 160 B and their descriptors 64 B, the picture's state about 20 B a track
(80 B), the interpreter's working state about 90 B: **about 400 B of the 584 B** the data bank has above its block.
Measured per part as each is built; anything that would take the bank under 4,096 B stops and is written down.

**Built (0.2):** firmware/src/ui_home_pic.c (the page, `home_pic_set`, `mac_commit`), MENU > HOME PAGE (ui_menu.c),
`home_pic_on` (ui.c), the knobs through `stage_page` (ui.c), the three dots, the dispatch and the landing (ui_draw.c,
ui_input.c, go_home), `T_COZY` (gfx.c). Tests: tests/home_pic_test.c (MENU, PAGES, KNOBS, COMMIT, REMOVE, DRAWS, COZY,
BUDGET; and the TRACKS hash: the same 5 HOME screens with the switch off and on, at TRACKS, hash DADB7420 both), tests/
ui_render.c under the switch (38 picture screens in every palette: 0 lint findings, 0 MONO pixels off gray, with every
other screen of the renderer again: 270 screens x 8 palettes). Not rendered: PACK MISSING in the sound line (the renderer
has no gated build; the lookup's gate is tested in oc_scene_test's gated build). **Measured pixel budget** (home_pic_test,
2 s of play, a note every 500 ms, KNOB 2 turned in the second second, the scope moving; about 9 frames a second (the 100 ms
gate on 16 ms UI frames); SPI at 12 MHz):

| Engine | Mean px a 100 ms frame | SPI mean | Worst px | SPI worst |
|---|---|---|---|---|
| MACRO | 8,210 | 11.0 ms | 16,384 | 21.9 ms |
| SHAPES | 3,940 | 5.3 ms | 8,048 | 10.8 ms |
| DRUM-X | 8,030 | 10.7 ms | 18,336 | 24.5 ms |
| CZ-1 (the plain picture) | 7,496 | 10.0 ms | 11,200 | 15.0 ms |

All inside the design's budget (a mean under 15 ms, a worst frame under 30 ms).

**The mockups (docs/orangecat/ui/new/, drawn by the firmware's own HOME code through tests/ui_render.c, NIGHT unless
named):** `home_pic_sheet.png` (the sheet: MACRO as loaded, turned, a note; SHAPES as loaded, COLR turning, a note; DRUM-X
as loaded, TUNE turning, a note; CZ-1's plain picture; the MENU row; SHAPES on DAY), `home_pic_<engine>_load.png`,
`_turn.png`, `_turned.png`, `_note.png`, `_k<1..4>_min.png`, `_k<1..4>_max.png` for macro, shapes, drumx;
`home_pic_cz1_plain.png`; `home_pic_menu.png`; `day_home_pic_<engine>_load.png`.

**Flash and RAM, planned** (UI_DESIGN.md Part 3's estimate for all of step 1: about +6.4 KB flash, +64 B RAM; here
the project magic bump is not part of it, see 1a): measured per part in this file as each is built.

### The design review of step 1 (an independent agent, October 29, 2026)

| # | Finding | Severity | Done |
|---|---|---|---|
| 1 | Scenes as pack tables change the pack hash: the firmware would refuse release-2's pack (MACRO, SHAPES silent) and every module | blocker | 1b redesigned: a trailer outside the hash |
| 2 | A pack table's length cannot come from sizeof on the device | should fix | the trailer carries each scene's length |
| 3 | OC_HOME_PIC never reached the compiler or gen_pack (build.py's switch list) | should fix | added to build.py's switches; gen_pack's --scenes with 1b |
| 4 | Motion on a macro target: the bake would not reload as it sounded | should fix | a target motion plays is left alone, live and saved; tested |
| 5 | Editor dumps report raw offsets; track removal recenters the macros | should fix | written down (Session D, the format revision); removal tested in 1c |
| 6 | Back to TRACKS strands live offsets; mac_desc answered for every engine (the Prophet test engine uses 34, 36 as HOME knobs) | should fix | mac_desc only for a macro knob (tested); TRACKS commits the macros (1c) |
| 7 | `ui.home_pg` is a 0/1 flag in five places | should fix | the encoding fixed above (1c) |
| 8 | The cost estimate was several times too low | should fix | measured on the host, restated |
| 9 | RAM totals understated | minor | restated: 160 B measured, about 400 B in all |
| 10 | Guard against an engine crossfade (xf_on) as well | minor | added |
| 11 | Write down which targets a table may use | minor | written (1a) |
| 12 | Host builds have the pack gate off; hostsim tests must call mac_poll | minor | noted; the tests call it |
| 13 | Deviations from UI_DESIGN.md not said | minor | said (1c) |
| 14 | The 100 ms gate would delay the dials; one shared static descriptor | minor | dials not gated; four descriptors |

The reviewer found sound: the bit-31 arming (no stale bit, no ordering hole: mix_block has no early return between the
two calls, throw_turn touches only its own bit), the macros' place in the block (after the events, before the CPU1
copy; mod.c's save and restore around it), negative values on 33 .. 36 everywhere they can go, the free menu bit, and
every sound load zeroing the macros.

### The diff review of 1c (a fourth independent agent, October 29, 2026)

Found and fixed: **three bugs**: the knob-rate phases overflowed 32 bits (a phase jumped once a knob passed about a fifth of
its travel: now 64-bit); a note-on was seen when the newest voice freed while an older one sounded (now a newer age only);
an engine without a table drew a knob whose id is 33 .. 36 (the Prophet test engine's, a module's native offset) as a macro
(now only a macro of the table in force). Should fix, done: the header's dots fell back to two when there was no room
(now three or none); the first note after power-on or a track select showed no hit (seeded); a macro's center read 129
(now 128); Stage's picture read TP[255] for a table's empty knob (skipped); the scene was looked up (on the unit: the
trailer's CRC and the walk) every UI frame (now when the engine or the pack's check changes: `oc_pack_gen`); the commit
did not tell the editor, put a momentary take back first, or restore the interrupt state as it found it (all done);
gfx.c's default for the switch (it comes before core.h). Tests that could not fail, done: CZ-1's knobs compared with the
TRACKS page's; the TRACKS hash on the three table engines and after PICTURE and back (D60856FA with the switch off and on);
the job fails when a hash is empty; the sound line's color checked in pixels; the menu bit through the settings record;
DRAWS checks the scene path; BUDGET with a moving scope and the plain picture. **The plain picture was over the budget**
(18 ms of SPI a frame on average with a moving scope): its waveform band is now 48 rows (10.0 ms mean, 15.0 ms worst).
Found after the review while checking the note mockups: the page took a clock reading of exactly 0 for "not started" (a
note-on at that instant lost: now a flag of its own), and the renderer's note screens had no voice (the engine forced
without its state); a new check (HIT: a note-on moves the picture, 313 px on SHAPES; a chord's newer note freed first is
not a note-on). Not changed: the plain picture's waveform is snapped per band (as release-2's Stage), a frame's 13 descriptor lookups
for the signatures (cheap).

### Step 1's flash and RAM against release-2 (measured, October 29, 2026)

Both built here with tools/orangecat_engines.sh, the shipping set (`./build.sh`, and `OC_HOME_PIC=1 ./build.sh`):

| Measure | release-2 e1c8ac3 | OC_HOME_PIC=0 | OC_HOME_PIC=1 | Difference |
|---|---|---|---|---|
| Image | 537,248 B | **byte-identical** | 546,784 B | **+9,536 B** (30,464 B left of the 577,248 B area) |
| RAM .data + .bss | 93,624 B | the same | 94,008 B | **+384 B** |
| Data bank (above .bss) | 4,680 B | the same | 4,296 B | -384 B: **200 B above its 4 KB block** |
| Audio working arena | 142,976 B | the same | 142,976 B | 0 (no `.pool`, no `.audio_text` added) |
| Asset pack | 40,232 B, hash 657ED950 | **byte-identical** | 40,845 B, the same hash and tables | +613 B (the scene trailer; 115 B of the area left) |

Where the flash went (symbol sizes, nm): the page and the interpreter mostly inlined into `draw_page` (+5,804 B), `ui_input`
(+704, the menu row and the HOME tap), `mac_on` 394, `mac_saved_value` 232, `mac_off` 172, `mac_value` 116, the interpreter's
helpers (`sc_arc16` 214, `sc_aaline` 206, `sc_map` 146, `sc_mod` 136, the sine table 130, `sc_plot` 94, `sc_line16` 88,
`sc_len` 82), the page's (`hp_knob_text` 206, `hp_knob_q8` 122, `hp_is_macro` 100), `draw_head` +106, `track_desc` +104,
`palette_set` +104, the tables (SHAPES' 80 B each engine's). **`fm1_timer5_irq` grew by 300 B**: the audio interrupt's
handler, where the throws' calls are inlined (the bit-31 test and mask). The plan said about 6.4 KB; the measure is
9.5 KB, about 3 KB more, mostly the interpreter as the target's compiler builds it (the plan's 3.3 KB was clang for
Thumb-2 as a stand-in).

**The data bank is now 200 B above its block.** Step 2 onward cannot add more than that to .data and .bss without
taking a 4 KB block from the arena: anything that would is to be written down and stopped (the brief). Candidates to
win it back if needed: the macros' kept values (160 B) in the arena with the first armed macro, the four macro
descriptors (80 B) as constants in the tables.

### Step 1's full host suite (October 29, 2026)

The full host suite (tests/run_tests.sh, PARALLEL=1, niced, with tools/orangecat_engines.sh sourced as CI's test step
does) ran on the working tree of step 1 in 104 minutes. Every job passed, the seven new ones included, except one:
"regression: target cost of the render loops". Its two lines over budget are `dxe_post_sides` (9 against 8) and
`pl_osc_render` (280 against 133). **That failure is release-2's, not step 1's:** the suite analyzes the switch-off
image, which is byte-identical to release-2 e1c8ac3's, and the baseline run on release-2 failed the same two lines with
the same numbers. Most likely the local toolchain's code differs from CI's, where the budgets were taken. Step 1 does
not touch those engines (the brief's no-touch files), so the item goes to Session A below.

## Step 2: EDIT's sheets

**Status: Done (host).** Behind a second switch, `OC_EDIT_SHEETS` (core.h, default 0), which needs `OC_HOME_PIC=1` (the
plan: each step ships behind the step before; the sheets use step 1's knob colors). With it off, release-2's screens and
step 1's are unchanged.

### 2. The sheets (design note)

**What the code has today (read for this note).** The EDIT pages already fall into *sections* (ui_sections.c, `sec`:
up to 16, by title through `SEC_RULE`; FM6's operators a section each; Store and Tools last). PRESETS on an EDIT page
turns the section (`sec_turn`, to the page last used in it); EDIT on a page of an engine with two or more sections opens
the section map (`smap_open`); SELECT turns the pages of the whole family (`page_scroll`); the header's dots count the
section's pages. The section tabs (`tabs_draw`, rows 30 .. 55) exist but only the sound designer draws them. ENV and
LFO are families of their own (FAM_ENV: ENV, ENV DEST; FAM_LFO: LFO, LFO 2, LFO DEST, MOD); on the Prophet, FM6 and
CZ-1 the buttons visit the engine's own pages first (`native_titles`). VOICE (with VOICE 2 and 3) and FM LFO are list
pages (ui_list.c), already several rows on one screen.

**The step in one line:** a sheet is a section, and the sections grow to cover ENV, MOD, FX and VOICE; EDIT turns the
sheets, SELECT walks the rows into the next sheet, the tabs and the knobs' colors are drawn.

1. **Which sheets, in which order** (`sec_build` under the switch, ranks instead of its two passes):
   - rank 0: the engine's own sections as today (SOUND: EDIT 1 and EDIT 2, "Sound"; the Prophet's Osc .. Wheel; FM6's
     Algo, OP1 .. OP6, Pitch, LFO, Ctrl; CZ-1's lines; DRUM-X's KIT and LANES; a module's pages);
   - rank 1: **Env**: the FAM_ENV pages that are visible (ENV, ENV DEST; none on the engines with their own, as today);
   - rank 2: **Mod**: the FAM_LFO pages (LFO, LFO 2, LFO DEST, MOD);
     sections merge only within a rank, so the Prophet's own Mod section (ENV MOD, POLYMOD, MOD FLT) does not swallow
     the track's LFO pages; on an engine with LFO pages of its own (`native_titles(FAM_LFO)`) this sheet is named
     "Trk mod" so the tabs never show Mod twice;
   - rank 3: **FX**: a link, not a sheet of rows (`sec.first` = the FX family's first page, the track's sends);
   - rank 4: **Voice** (the "Voice" sections: VOICE's list; the Prophet's UNISON and BEND);
   - rank 5: Store and Tools, as today last.
   This is UI_DESIGN's SOUND, ENV, MOD, FX, VOICE, and for the native engines their sections, then ENV, MOD, FX, VOICE
   where they apply. DRUM-X's first sheet is named Kit (its EDIT 1 and 2 are KIT and KIT 2). Up to 16 still holds: FM6
   has the most, 13 (Store, Algo, OP1 .. OP6, Pitch, LFO, Ctrl, then Mod, FX, Voice).
2. **EDIT** (a tap; held is undo, unchanged):
   - on a sheet: the next sheet, after the last the first; reaching the FX link opens the FX family's first page;
   - on the track's FX pages (FAM_FX, the track's scope: FX, DIST, AUTO PAN, DUB ..): the sheet after the FX link
     (Voice), so EDIT keeps walking the sheets when it went through FX;
   - anywhere else (HOME, the shared effects, SEQ ..): the first sheet (SOUND), as UI_DESIGN's table says;
   - the section map is no longer opened by EDIT (its code stays until step 9; with the switch off it is as today).
   A sheet is entered at the row last used in it (`sec.last`, as PRESETS does today).
3. **SELECT** on a sheet: the next row of the sheet; past its last row, the first row of the next sheet (the FX link
   skipped: SELECT stays in EDIT); turned back, the same backwards (into the previous sheet's last row). On FM6's
   operator sheets the rows are the operator pages for the operator of the sheet (as the map does today).
4. **PRESETS** on a sheet: the next or previous sheet (today's `sec_turn`, now over every sheet, the FX link skipped).
   The design's "PRESETS: the sound" on a sheet is left for step 3 (the SHIFT map moves the engine change, and the
   browser's place, together); the safer reading keeps one meaning per control within this step.
5. **ENV and LFO** are unchanged: they visit the same pages, which are now rows of the Env and Mod sheets (on the
   Prophet, FM6 and CZ-1, rows of their own sheets), so they are the shortcuts the design asks for with no code.
6. **Drawing** (pv_draw, the RINGS and CURVE pages):
   - the sheets' tabs at rows 30 .. 55 (`tabs_draw`, as the designer's: mixed case, as drawn there today; the mockup's
     capitals were drawn by the mockup, not by the firmware), and the strip moved down under them (`pv_tab_h`, as the
     designer and FM6's EG pages already do);
   - the framed row's labels in the knobs' colors (`T_COZY`, step 1), its values in the text color;
   - on a RINGS page (no curve) whose sheet has another row: that row drawn under it at rows 108 .. 159, dimmed, as
     the mockup's second row (the next row; on the last row, the previous one), from the page's own descriptors through
     the list's capture (`list_cap`'s way: `col_style = CS_CAPTURE`, `ui.page` swapped while drawing nothing);
     under it (rows 160 .. 207) on the SOUND sheet of an engine with a knob table (MACRO, SHAPES, DRUM-X): what HOME's
     four knobs are (their names in the knobs' colors, a macro's targets: `mac_desc`), as the mockup; elsewhere the
     picture as today in what is left;
   - a CURVE page (ENV's ADSR, the LFO's wave, FM6's EGs) keeps its curve (shortened under the tabs, as the designer's
     pages are today) and shows no second row;
   - the header's dots: the rows of the sheet (`head_pages` already counts the section's pages; it now also does on the
     Env and Mod sheets).
   The list pages (VOICE, FM LFO) and MOD's route page (`pv_mod_rows`, rows 32 .. 232) keep today's screens: they have
   no room for the tabs. They are sheets for EDIT, SELECT and PRESETS. Their tabs wait for step 9 (written down below).
7. **Nothing on the audio side, nothing saved.** Only navigation and drawing; no parameter, format or pack change.

**RAM:** the `sec` table is today's (16 sections). New: the second row's and the box's signature (8 B). Within the
plan's +16 B and the 200 B left in the data bank. **Flash:** the plan says +1 to 2 KB.

**Tests:**
- `tests/sheet_test.c`, with the switch on, every engine in the build (the host's): the coverage check (every page that
  SELECT reached today in FAM_EDIT, FAM_ENV and FAM_LFO is a row of some sheet, and SELECT from SOUND's first row walks
  every one of them and comes back); EDIT walks every sheet and the FX link; EDIT from the FX pages goes to Voice; EDIT
  held still undoes; PRESETS turns the sheets; ENV and LFO land on rows of their sheets; the knobs edit the framed row's
  ids; with the switch off, today's EDIT map and cycle (ui_test.c's own tests, unchanged, run as they are).
- `ui_render.c` under both switches: every sheet of every engine (each row framed), linted on every palette.
- The full host suite; the switch-off image byte-identical to release-2's; with both switches the HOME and TRACKS
  hashes of home_pic_test unchanged.

### The design review of step 2 (an independent agent, October 29, 2026)

| # | Finding | Rating | What was done |
|---|---|---|---|
| b1 | `sec_of` finds a page's section by title: ENV, LFO and MOD rows fall to "Sound"; "Trk mod" never matches | blocker | Sheets no longer ride on `sec`: `ui_sheets.c` derives a page's sheet from the page itself (family, title rule, rank), key = (rank, name) |
| b2 | `sec_on`, `sec_go`, `smap_pages`, `head_pages` hard-code FAM_EDIT; `sec_go` writes `fam_last[FAM_EDIT]` with other families' pages | blocker | Under the switch `sec_on` is 0 (the old map, section turn and section dots off); the sheet code handles EDIT, SELECT, PRESETS and the dots; `fam_last` is written for the landed page's own family only |
| b6 | Sections past 16 are dropped silently; with SELECT walking sheets their pages would be lost | should-fix | No table: the sheets are computed on demand, so there is no cap; only the tabs' names are bounded (a stack array, the ones that fit are drawn anyway) |
| b7 | The section cache misses a module coming or going | should-fix | No cache |
| b3 | The FX link through `sec_go` lands by accident and pollutes `fam_last[FAM_EDIT]` | should-fix | The link calls `open_family(FAM_FX)` and is never a row |
| b4 | EDIT from the FX pages: Voice even when the FX button opened them | should-fix | One byte, `sh_fx`: set only when EDIT went through the link, cleared on any other page change; EDIT on an FX page with it set goes to the sheet after FX, otherwise to SOUND |
| b5 | EDIT's other jobs (DRUM HIT, the DRUM grid, rename, the NOTES step modifier) must keep priority | should-fix | The sheet code replaces only the `smap_open` branch and the final `open_family(FAM_EDIT)`; the order in `page_tap` is kept; tests below |
| a1 | `mac_desc` gives no macro targets | should-fix | The box lists the knobs' names in their colors and, for a macro knob, its targets' labels walked from `kdef_of(e)[k].t[]`; measured in flash |
| c1 | FM6's EG pages draw operator tabs at the same rows; the designer's tabs too | should-fix | The designer's tabs win while it is on; otherwise the sheet tabs replace FM6's operator tabs (the OP1 .. OP6 sheets are the same choice) |
| c2 | A picture squeezed into 48 rows | should-fix | A RINGS page with a second row draws no picture; a one-row RINGS sheet keeps its picture under the tabs (`pv_tab_h`, as the designer's pages) |
| c3 | The second row through `draw_column` thrashes the per-column caches; the cache key misses the label color | should-fix | The second row is captured (`CS_CAPTURE`, as `list_cap`) and drawn by its own small card code with its own signature; entering or leaving a sheet is a page change (`ui.force`) |
| d1, d2 | RAM and flash understated | should-fix | RAM: `sh_fx` 1 B and the second row's and box's signatures 8 B, no per-sheet memory (a sheet opens on its first row); flash measured from the map |
| a2 .. a4 | FM6's count (14, Store last), FM BEND also a list, the Prophet has Sound | nit | Corrected below |
| b8 | DRUM-X gets Mod and Voice beyond the design's list | nit | Kept (every page stays reachable); written in the decisions |
| b9 | The tab strip comes and goes (list, MOD route, Store/Tools pages) | nit | Accepted for step 2; written down for step 9 |
| c4 | The capture reads `page_over` | nit | No second row while `page_over` is set (a layer) |
| c5 | Row placement 58 vs the mockup's 56; the mockup's foot keys | nit | The strip's own `card_y` (58) is kept; the foot keys stay today's sound bar (step 3 owns the keys' hints) |
| c6 | The header dots compute a sheet per page per frame | nit | One scan a frame (the current sheet's rows); measured in the frame budget |
| d3 | Which image is compared | nit | Both: switch off against release-2, and OC_HOME_PIC=1 with OC_EDIT_SHEETS=0 against step 1 |
| e | Test gaps (EDIT's other jobs, modules coming and going, the Prophet hiding ENV, `fam_last`, layers over a sheet, the lint on light and MONO) | should-fix | All in sheet_test.c and ui_render.c (below) |

### 2. The sheets (the design as built, after the review)

Replaces points 1 .. 6 of the note above where they differ.

- **The sheets are computed, not stored** (`firmware/src/ui_sheets.c`, included by ui_draw.c under the switch). A page is
  a row when it is visible and in FAM_EDIT, FAM_ENV or FAM_LFO. Its sheet's key is (rank, name): FAM_ENV rank 1 "Env";
  FAM_LFO rank 2 "Mod" ("Trk mod" where the engine has LFO pages of its own); FAM_EDIT by today's title rule
  (`sec_rule`): "Voice" rank 4, Store and Tools rank 5, the rest rank 0 (DRUM-X's "Sound" is shown as "Kit"); FM6's
  operator pages a sheet per operator, rank 0, in the place of the first one. The FX link is rank 3. The list of sheets
  is the distinct keys by rank, then by first page. It is made again on every use (one walk over the pages: once a
  frame while drawing, once a button; the diff review's 1), so a module coming or going, an engine change or the Prophet hiding ENV can
  never leave it stale, and there is no cap.
- **FM6** (its EDIT 1 and 2 rows as the engine shows them today, a Sound sheet if visible): Algo, OP1 .. OP6, Pitch, LFO, Ctrl, Trk mod,
  FX, Voice, Store: 14. **The Prophet:** Sound, Osc, Mixer, Filter, Amp, Mod, LFO, Wheel, Trk mod, FX, Voice, Store.
- **A sheet opens on its first row** (no per-sheet memory: no RAM). PRESETS and EDIT both land there.
- **EDIT**, in `page_tap` where `smap_open` and the final `open_family(FAM_EDIT)` were (every earlier job of EDIT kept
  first): on a row, the next sheet (wrapping); the FX link opens the FX family and sets `sh_fx`; on a FAM_FX page with
  `sh_fx` set, the sheet after the link; anywhere else, the first sheet.
- **SELECT** on a row: the next row, past the last the next sheet's first row (the link skipped), backwards the previous
  sheet's last row. **PRESETS** on a row: the next or previous sheet (the link skipped). FM6's operator sheets set
  `fm6_opsel`.
- **`fam_last`** is written for the landed page's own family; never FAM_EDIT with another family's page.
- **Drawing:** tabs on RINGS and CURVE rows (the designer's own while it is on; FM6's operator tabs replaced); the
  framed row's labels `T_COZY`; a RINGS row whose sheet has another row: the next row (the previous on the last)
  captured and drawn dimmed at 108 .. 159 and the box at 160 .. 207 (on the first sheet of an engine with a knob table:
  HOME's four knobs and a macro's targets; otherwise empty); a one-row RINGS sheet keeps its picture; CURVE rows keep
  their curve. List pages, MOD's route page and the action pages keep today's screens.

### Step 2's flash and RAM (measured, October 29, 2026)

Firmware builds one at a time, niced, with tools/orangecat_engines.sh:

| Build | Image | .data+.bss | Data SRAM tail | Arena | Note |
|---|---|---|---|---|---|
| Both switches off | 537,248 B | 93,624 B | 4,680 B | 142,976 B | byte-identical to release-2 e1c8ac3 (`cmp`) |
| OC_HOME_PIC=1 only | 546,784 B | 94,008 B | 4,296 B | 142,976 B | step 1's image: the same disassembly as step 1's ELF |
| OC_HOME_PIC=1, OC_EDIT_SHEETS=1 | 549,328 B | 94,008 B | 4,296 B | 142,976 B | **+2,544 B of flash, +0 B of RAM against step 1** (after the diff review's fixes) |

Free flash with both switches: 577,248 - 549,328 = **27,920 B**. The sheets' code: `sh_make` 650 B, `sh_rows` 114, `sh_land`
100, `sh_row` 50, `sh_get` 44, `sh_add` 38, `sh_show` 34, the rest inlined into `page_tap`, `ui_input` and `pv_draw`
(pv_draw 8,598 B, now a function of its own: the compiler stopped inlining it into draw_page). Before the review's fix
of its frame cost the image was 546,816 B (+32 B): that build worked the sheets out three times a frame with string
compares and happened to inline differently; the numbers above are the shipped code's. The 13 B of state (`sh_fx`,
`sh_now`, two signatures) land in existing padding: .data+.bss and the data SRAM tail are unchanged, so the 200 B margin
under the data bank's block still holds. Stack: one `sh_ctx_t` (about 380 B) in `ui_draw` a frame, and one in a button's
handler (the user stack is about 24 KB, app.ld).

**Frame cost (host, i5-6260U, -O1, nothing changing on screen):** MACRO's SOUND row 36 us a frame with the sheets, 32
without; FM6 60 (31); the Prophet 38 (32); CZ-1 52. Making the sheets: 32 to 44 us. Before the fix: 353 us on MACRO.

**Host tests of step 2:** `tests/sheet_test.c` (sheet_job, under ASan and UBSan), 79 checks: COVER (against release-2's
own walk; FM6's six operators), EDIT, FAMLAST, FXLINK, PRESETS (both ends; FM6's row kept), SHORT, DRAW on SID, MACRO, DRUM-X, DRUM, FM6, SHAPES, CZ-1 and the Prophet (the host's engines), KNOBS (and EDIT held undoes),
OTHERS (SAVE > USER's EDIT, SEQ's EDIT), ENGINE (an engine with its own ENV under an Env row): 0 failed. The HOME and
TRACKS hash with both switches equals release-2's. `ui_render.c` with both switches (ui_renders_sheets): 270 screens
and 1,152 sheet screens over 8 palettes, 0 lint findings, 0 MONO pixels off gray, the FM6 charts check passing.

**The full host suite with step 2** (56 minutes, niced, PARALLEL=1, the engines file sourced): every job passed but the
render loops' target cost, release-2's own failure (the same two lines, the same numbers); sheet_job and
ui_renders_sheets passed.

**The mockups (docs/orangecat/ui/new/, NIGHT unless named, drawn by the firmware through tests/ui_render.c):**
`sheets_sheet.png` (the contact sheet: MACRO's SOUND row 1 and 2, ENV, MOD; DRUM-X's KIT and LANES; SHAPES; CZ-1's
Line 1; the Prophet's Osc and Filter; FM6's OP1 and Algo), `sheet_<engine>_<sheet>_<name>_<row>.png` (every row on
MACRO, SHAPES and DRUM-X; the first row of every sheet on CZ-1, FM6 and the Prophet), `day_sheet_macro_00_sound_1.png`,
`day_sheet_drumx_02_lanes_1.png`, `day_sheet_prophet_03_filter_1.png`.

### The diff review of step 2 (an independent agent, October 29, 2026)

| # | Finding | Rating | What was done |
|---|---|---|---|
| 1 | The sheets worked out three times a frame, each with every page's visibility and title rule: about 80,000 string compares, ui_draw 353 us against 32 on the host (an estimated 6 to 15 ms a frame on the unit) | should-fix | One walk over the pages makes a context (`sh_make`: each page's sheet index, no string work after it), made once a frame in `ui_draw` (`sh_now`) and once a button: 36 us a frame on the host |
| 2 | EDIT on a shared effect (DELAY) after the link went to Voice | should-fix, confirmed | The link's rule is for the track's FX pages only (`scope != SC_GLOBAL`); FXLINK tests it, and a mutation without the fix fails it on every engine |
| 3 | Tests that could pass broken: COVER checked the sheets against themselves; FM6's operators not checked; ENGINE passed on falling to HOME; PRESETS +1 only; the link's lifetime untested | should-fix | COVER's oracle is release-2's own walk (`page_scroll` round each family, the ENV and LFO buttons' lists); the six operators checked; ENGINE needs `!ui.home`; PRESETS back and both ends; FXLINK (after the link, from a shared effect, after another button) |
| 4 | PRESETS on an FM6 operator row lost the row | nit, confirmed | From one operator sheet to the next only the operator changes (release-2's operator turn); tested |
| 5 | The two drawing signatures reset only by `ui.force` | nit | Zeroed whenever the lower area is not the sheets' |
| 6 | `pv_pic` called twice a frame | nit | Kept (no side effects, a handful of compares) |
| 7, 8 | `tabs_draw`'s shared static; the stack | nit, OK | No change (different salts, a force between; a few hundred bytes of a 24 KB stack) |
| Checked OK | Array bounds (most rows 6, most sheets 15, 137 pages); every edge case of the walk; the capture (no cache touched); list pages, MOD's route page and the designer; `page_tap`'s order | | |

### Left for later from step 2

- The tabs on the list pages, MOD's route page and the action pages (they have no room today): step 9, when EDIT 1 and
  EDIT 2 retire and those screens are redrawn as sheets.
- PRESETS on a sheet as "the sound" (the design's table): step 3, with the SHIFT map.
- The foot of a sheet as the mockup's keys ("SELECT ROW 1/2", "EDIT NEXT"): step 3 (the keys' hints belong to the map).
- The capitals of the mockup's tabs: the owner's call (today's tabs are mixed case).

## Step 3: the SHIFT map

**Status: Done (host).** Behind a third switch, `OC_SHIFT_MAP` (core.h, default 0), which needs
`OC_EDIT_SHEETS` (and so `OC_HOME_PIC`). With it off, HOME held opens the MENU at 0.7 s as on release-2.

### 3. The SHIFT map (design note)

**What the code has today (read for this note).** HOME goes through `btn_hold` (ui_input.c, a fixed 700 ms): a tap acts
on release (HOME's pages, `go_home`), a hold fires once at 700 ms with HOME still down and opens (or closes) the MENU;
`menu_input` only runs after HOME is let go. HOME is combined with nothing today (a layer button held swallows HOME).
The layers (GLO, SCL, FX punch-in, ARP's PERFORM) are one table (ui_layer.c `LAYERS`): held alone past the menu's HOLD
time a map is drawn over the page (`ui.layer`, `draw_layer`), a combo opens it at once, a double tap locks it. The slot
`LAYER_RESERVED` (1) is unused. Fine turning exists only for NOTES' lengths (ENV held). Engine change is
`select_engine(eng_step(..))` (the browser's KNOB 4); init is `sound_init_of` (behind a confirm in the OCT+ sheet);
there is no sound clipboard (the user presets pack a track into a 238 B `up_rec_t` and write it to flash).

**The owner's decision 4 read safely.** "HOME pressed alone opens the MENU" is taken as UI_DESIGN's own text (section 1
and its controls table: "let go alone, the MENU opens"): HOME held and let go with no combo opens the MENU, on the
release; a HOME tap keeps stepping HOME's pages. Written in the decisions for the owner to confirm.

1. **HOME held** (`OC_SHIFT_MAP`): from the press, HOME is SHIFT. Held alone past the menu's HOLD time (as the layers'
   maps), the map is drawn over the page (`ui.layer` = the reserved slot, renamed `LAYER_SHIFT`, so the header, the toast
   and the redraw are the layers'). Let go:
   - after a combo (any knob, PRESETS, EDIT, OCT-, OCT+, SELECT, a key): nothing more (no MENU, no tap);
   - alone, before the HOLD time: a tap, as today;
   - alone, after it: the map closes and the MENU opens.
   With the MENU open, HOME held closes it as today (the MENU's own keys are unchanged). No double-tap lock for SHIFT.
2. **The combos** (step 3's set; each on the map before it is in the code):
   - SHIFT + KNOB 1..4: a fine turn, one raw step a detent with no acceleration (a quarter step needs a residue per knob
     in RAM; the parameters' own step is the finest without one: the safer reading, written down);
   - SHIFT + PRESETS: the track's engine, the next or previous (`select_engine(eng_step(..))`, admission as the browser);
   - SHIFT + EDIT: init the sound, through today's confirm (CF_INIT_SOUND); undoable as every load;
   - SHIFT + OCT-: copy the selected track's sound into a clipboard; SHIFT + OCT+: paste it onto the selected track
     (one undo step, `load_begin`/`load_end`, as a preset load). The clipboard is one `up_rec_t` (238 B) in `.pool`, not
     .bss (the data bank's 200 B margin), packed and unpacked by the user presets' own code split from its flash write
     (`up_pack`, `up_unpack` in upreset.c). Empty clipboard: "NOTHING COPIED".
   - NOTES and FEEL's combos (stretch, copy, paste, delete a selection) wait for step 4, where FEEL is built.
3. **The map** (`ui_shift.c`): four rows of three tiles under the header (the mockup's 76 x 46 tiles from y 34), each a
   control's name and its job on this page, a combo's tile tinted with a plus. Pages: HOME (its page's own: PICTURE,
   TRACKS, CHORD), a sheet's row, and every other page with the controls table's generic jobs (the page family's
   button, SELECT, PRESETS, OCT). The texts are a small const table (no RAM); the lint runs over the map on every page
   kind in ui_render.c.
4. **Nothing on the audio side, nothing saved** (the clipboard is RAM only, lost at power off).

**RAM:** .data and .bss: 0 B planned (the map's state rides in `ui.layer` and spare bits of `ui.home_t0`). `.pool`: 240
B for the clipboard; the pool shares SRAM with the audio arena, so the build must show the arena's blocks unchanged
(142,976 B, minimum 142,336) or this item stops. **Flash:** the plan's +1 KB, likely more with the clipboard.

**Tests:** `tests/shift_test.c`: a tap, a hold alone (map, MENU on the release), each combo (no MENU after), the MENU
closed by a hold as today, fine turns, the engine step, init behind its confirm, copy and paste (bit-exact sound, one
undo), an empty clipboard; with the switch off, ui_test's HOME tests as they are. `ui_render.c`: the map on each page
kind, every palette, linted. The full suite; the switch-off image byte-identical to release-2.

### The design review of step 3 (an independent agent, October 29, 2026)

| # | Finding | Rating | What was done |
|---|---|---|---|
| 6 | SHIFT+EDIT opens the init confirm while HOME is down; the next OCT+ could be "accept" or "paste" | blocker | SHIFT never arms while a confirm, NAME, NEW SONG, the designer, a popup, the MENU or a layer is up, and a combo is never read under one; paste is not in step 3 |
| 7 | On NOTES and TRACKER EDIT is a step modifier (a released EDIT deletes) and PRESETS zooms | blocker | The combos act on HOME's pages and on the sheets only; elsewhere SHIFT + a control is eaten (no job, no MENU) |
| 1 | `ui.home_t0`'s bits are its press time and it is zeroed on the release frame | should-fix | SHIFT's two flags live in `ui.pg_down` bits 14 and 15 (14 button ids use 0 .. 13), read on the release frame before they are cleared |
| 2 | `ui.layer` drags the layers' LEDs, autosave pause, OCT block and `layer_show`'s per-frame rewrite with it | should-fix | The map is not a layer: its own draw branch (as the section map's), redrawn on `ui.force` |
| 8 | OCT's taps and hold sheet, EDIT's hold undo, the momentary "KEPT" path still act under SHIFT | should-fix | A control used as SHIFT's combo is swallowed the way those paths already swallow (EDIT `edit_t0 |= 2`, OCT `oct_eat` and `oct_t0 |= 2`); combos act on the control's press |
| 9 | Other controls unclassified; HOME then a layer button; HOME + REC's capture | should-fix | Any other button, a key or ALGO while HOME is down counts as a combo: it does its own job as today and the release opens nothing (no MENU) |
| 10 | An accidental detent silently cancels the MENU; the 400 to 699 ms window | should-fix | The detent shows its value as every knob does (the message says it was used); the map and the MENU keep btn_hold's 700 ms, so a 400 to 699 ms press stays a tap |
| 11, 12 | A press begun under the designer or the MENU; two times | should-fix | SHIFT arms only on a press begun with none of those up; one time (700 ms) for the map, the MENU's opening and its closing |
| 13 | SHIFT+PRESETS: no `browse_commit`, a refusal is a dead end, a load a detent | should-fix | `browse_commit` first; one engine step per detent with the refusal's message (`select_engine`'s own); on HOME's pages only |
| 14 | The user preset record is not a faithful clipboard (admission, the user name, 21 values of a native engine, motion, the macros, the steps) | should-fix | **Copy and paste deferred** (below, with what they need): step 3 does not ship a lossy copy |
| 15, 22 | Step 1's promise of SELECT on HOME; PRESETS on a sheet; the sheet's foot; decision 4's reasoning | should-fix | Decision 4: UI_DESIGN's own section 1 and its table say "let go alone"; that settles it. SELECT on HOME's pages, PRESETS on a sheet and the foot hints are re-deferred (below) |
| 16 | A raw step a detent equals a slow turn on short ranges | nit | Written down: the fine turn is "no acceleration" (it differs on long ranges, where a fast turn jumps) |
| 17, 20 | 12 tiles, what is left off; the flash of the table | nit | The map is the mockup's twelve tiles; what it leaves off is the other buttons' everyday jobs (the controls table); measured |
| 18 | The switch-off image | should-fix | Every change under `#if OC_SHIFT_MAP`; nothing shared is refactored (no `up_pack` split in step 3) |
| 21 | Tests | should-fix | shift_test.c covers each case listed (below); ui_test, home_pic_test, sheet_test and layer_screen_test also built with the switch on |
| 3, 4, 5, 19 | Fine turning exists elsewhere too; the HOME tap's cycle; the pool before the audio text | nit | Noted; the HOME tap keeps step 1's PICTURE, TRACKS, CHORD cycle |

### 3. The SHIFT map (the design as built, after the review)

- **Arming:** HOME pressed with no MENU, confirm, NAME, NEW SONG, popup, layer or designer up, and no other button or key
  already held, arms SHIFT (`sh_st`, one byte of its own). Otherwise HOME is release-2's (a hold still closes the MENU
  at 700 ms; that press never opens it again).
- **Alone:** let go before 700 ms: a tap, as today. Held to 700 ms: the map is drawn; let go: the map closes and the MENU
  opens. Only a press that reached the map opens the MENU (a tap seen only as an edge between two passes is lost, as on
  release-2).
- **A combo** (bit 14 cleared, so the release does nothing): KNOB 1..4 on HOME's pages or a sheet: a fine turn (that
  detent with no acceleration); PRESETS on HOME's pages: the engine, one step a detent (`browse_commit` first, the
  refusal's message as `select_engine` gives it); EDIT on HOME's pages or a sheet: the init confirm (CF_INIT_SOUND).
  Anything else pressed or turned while HOME is down does its own job as today and only cancels the MENU. On any other
  page, SHIFT's knob, PRESETS and EDIT are eaten.
- **The map** (`ui_shift.c`): the mockup's twelve tiles (PRESETS SOUND, ALGO TRACK, SELECT PAGES / ROWS, K1-4 KNOBS,
  EDIT SHEETS, SEQ STEPS, then the combos tinted: K1-4 FINE, PRESETS ENGINE, EDIT INIT, OCT- COPY and OCT+ PASTE shown
  dim with "LATER", LET GO MENU), the header "SHIFT". On a page where the combos are eaten they are drawn dim.

### The diff review of step 3 (an independent agent, October 29, 2026)

| # | Finding | Rating | What was done |
|---|---|---|---|
| 1 | A HOME tap pressed and let go between two input passes (likely on the unit: a redraw blocks input) opened the MENU | bug, confirmed | The MENU opens only for a press that reached the map; an edge-only tap is lost as on release-2. Tested; a mutation without the fix fails two checks |
| 2 | A button or key already held when HOME goes down was not a combo (EDIT's hold undo ran under the map, then the MENU) | should-fix, confirmed | SHIFT does not arm then: HOME is release-2's. Tested |
| 3 | Tests: the tap's page step unchecked; the fine turn vacuous (detents 200 ms apart never accelerate); SEQ's "no step deleted" unchecked | should-fix | TAP checks the page steps; FINE turns five detents 5 ms apart (13 steps without SHIFT, 5 with); the SEQ check's text says what it checks |
| nit | A control pressed on HOME's release pass still opened the MENU | nit, confirmed | It counts as a combo |
| nit | `PREF_BITS` toggled around the knob: a future sheet knob that saves settings would keep ACCEL OFF | nit | A RAM bit of `sh_st` (`SHF_FINE`) read in `accel_by`; the setting is never touched |
| nit | SHIFT + PRESETS with a preview pending: two loads, two undo steps; one step a read, not a detent | nit | Accepted (as the design review's 13); the comment says a read |
| nit | Toasts freeze under the map; the doc's `pg_down` and SELECT labels | nit | As the section map does; the doc corrected |
| OK | OCT's hold and tap, EDIT removed from the edges, `sh_st` never stuck, the map's redraw, the switch-off image | | |

### Step 3's flash, RAM and tests (measured, October 29, 2026)

| Build | Image | .data+.bss | Data SRAM tail | Arena | Note |
|---|---|---|---|---|---|
| Every switch off | 537,248 B | 93,624 B | 4,680 B | 142,976 B | byte-identical to release-2 e1c8ac3 (`cmp`) |
| OC_HOME_PIC=1, OC_EDIT_SHEETS=1 | 549,328 B | 94,008 B | 4,296 B | 142,976 B | step 2's image (the same disassembly as step 2's ELF) |
| All three switches | 551,376 B | 94,024 B | 4,280 B | 142,976 B | **+2,048 B of flash, +16 B of RAM against step 2** (after the diff review's fixes; 551,520 B before them) |

Free flash with every step: **25,872 B**. The one byte of state (`sh_st`) costs 16 B of .bss (the alignment of what follows
it); the data bank's margin above its 4 KB block is now **184 B**. The map's table (`SHIFT_TILE`) is 144 B; the rest is
inlined into `ui_input` and `draw_page`. No `.pool` change (the clipboard is not in step 3).

**Host tests:** `tests/shift_test.c` (shift_job, ASan and UBSan): TAP (and a 480 ms press), HOLD (the map at 700 ms, the
MENU on the release), CLOSE (the MENU closed by a hold is not reopened), KNOB (HOME's picture and a sheet), ENGINE (on
HOME; eaten on a sheet), INIT (the confirm, held past 700 ms with no undo and no MENU; eaten on SEQ's page), OCT (no
octave, no OCT+ sheet), OTHERS (HOME then GLO; ALGO), DRAW (the map on HOME, a sheet and SEQ; a combo takes it away): 0
failed. With every switch: home_pic_test, sheet_test and layer_screen_test pass (layer_screen_test's EDIT-map race check
is skipped with the sheets built in: that map is not opened there). `ui_test` itself is release-2's behavior and runs
with the switches off; built with the sheets it reports 22 differences, every one a deliberate change of step 2 (the
EDIT cycle, the sections' map, the DRUM map, the strip under the sheets' tabs) or a knock-on of one (the SONG sheet check
after the DRUM map block passes from a clean start); step 3 adds none. `ui_render.c` with every switch: 0 lint findings,
0 MONO pixels off gray, the map on HOME, a sheet and SEQ in 8 palettes.

**Full host suite** (every switch as the suite builds them, after the fixes; 59 min, PARALLEL=1, niced): shift_job and
ui_renders_shift pass; the one failure is release-2's own "target cost of the render loops" (`dxe_post_sides`,
`pl_osc_render` over their budgets on this toolchain, failing identically on release-2 e1c8ac3: Session A's item).

**Mockups:** `shift_sheet.png` (the map on HOME, a sheet, SEQ's page, and HOME on DAY), `shift_map_home.png`,
`shift_map_sheet.png`, `shift_map_seq.png`, `day_shift_map_home.png`.

### Left for later from step 3

- **Copy and paste a sound** (SHIFT + OCT- / OCT+). The user preset record is not a faithful clipboard. A copy needs:
  the whole sound as `undo_t` holds it (engine, preset, user slot and name, every parameter, the native FM6, Prophet and
  CZ-1 patches, the DRUM and DRUM-X kits) in `.pool` (about 1 KB with the native patches: measure against the arena's
  blocks); a paste through `eng_admit` (a module engine that does not fit refused with its message), one undo step, the
  macros' dials and motion as a preset load leaves them. Its own step after step 4.
- **SELECT on HOME's pages** (step 1 promised it with step 3): HOME's tap keeps cycling them; SELECT on HOME keeps its
  menu-chosen job. Step 9, with the HOME switch's default.
- **PRESETS on a sheet as the sound, the sheet's foot hints**: step 9 (the map now says what the controls do).
- **NOTES and FEEL's combos**: step 4.

## Step 4: the step pages

**Status: Done (host).** Behind a fourth switch, `OC_STEP_PAGES` (core.h, default 0), which needs `OC_SHIFT_MAP=1` (each step
ships behind the one before). With it off, release-2's NOTES, TRACKER and the rest are unchanged (byte-identical image).
UI_DESIGN.md Part 1 sections 1 and 4, Part 2 `notes.png` and `feel.png`.

### 4. The step pages (design note, before its review)

**What the code has today (read for this note).** NOTES (`PAGES`: SC_STEP, GR_ROLL) already edits one recorded note at
a time through `notes_event_edit` (ui_input.c; one undo each through `event_replace`, seq_undo.c): KNOB 1 the cursor
(`notes_step_jog`: onto each note of a chord, then the steps), KNOB 2 the pitch, KNOB 3 the length (a quarter step a
detent, ENV held a sixteenth), KNOB 4 the velocity. PRESETS on NOTES is the roll's zoom (`ui.note_zoom`). OCT+ held with
KNOB 1 or 2 or SELECT selects a range (`bulk`, `bulk_jog`); with a range or a move armed KNOB 1 moves it. A manual
step's chord has one length; `note_convert` turns a manual step into events (all its notes at once, one undo through
`note_transaction`). TRACKER's KNOB 4 sets a recorded note's own length (a whole step a detent, `tracker_event_edit`) but
on a manual step it resizes the whole step (`step_note_resize`). The step properties (chance, condition, ratchet or flam,
nudge, accent, slide) live in the step's `step_t` for manual and recorded steps alike and have shared setters
(`step_set_chance`, `step_set_cond`, `step_set_rflam`, `step_set_nudge`, the flag bits); TRACKER's FX column is the one
place they are edited today.

**4a. NOTES (melodic tracks; the drum grid keeps its knobs).**
- With the switch: KNOB 1 PITCH, KNOB 2 START, KNOB 3 LEN, KNOB 4 VEL (the design's `notes.png`). PITCH, LEN and VEL are
  today's slots 1, 2, 3 of `notes_event_edit`, so every edit, its undo and its refusals are today's. **START** is a new
  slot (6): the note's onset moves a sixteenth of a step a detent (`RECORD_UNIT / 16`, through `note_onset`, wrapping at
  the phrase's length); if the move takes the note into another step cell the cell must not hold a separate manual note
  (the same "STEP OCCUPIED" refusal as today's whole-step move) and the cursor follows.
- **PRESETS** moves the cursor: `notes_step_jog`, today's KNOB 1 (each note of a chord in turn, then the steps). With a
  range selected or a move armed, KNOB 1 keeps moving the selection (today's "Move: KNOB 1" in the selection's sheet): the
  PITCH meaning applies only with no range and no move armed.
- **The zoom moves to ENV held + PRESETS** (the safer reading: ENV is already NOTES' fine modifier; nothing else is lost).
- The strip under the page names PITCH, START, LEN, VEL with the cursor note's values (START as steps and hundredths,
  `5.00`; LEN `3.50`), in the knobs' cozy colors (`T_COZY`, step 1's palette rule: blue, green, white, orange).
- The foot's hints: the tag "PRESETS STEP" and "SEQ NEXT" as `notes.png` has them (the foot is drawn by the page).

**4b. FEEL (new).** A page after NOTES in the SEQ family (SC_STEP, a new graph kind GR_FEEL; melodic and drum tracks both,
the properties are the step's). The page: the strip of four dials, then the 16 steps of the cursor's page (`cursor / 16 *
16`) as tiles, 4 x 4, as `feel.png`. Each tile: the step's number small, its value of **the knob turned last** under it;
a step that is off is drawn dim; the cursor tile is filled; the selection (a range) is framed in the accent.
- KNOB 1 **CHANCE**, then the conditions: one scale of 112 positions, 0 .. 100 % and 101 .. 111 = FILL, NOT FILL, 1:2 ..
  4:4 (`TC_FILL .. TC_4_4`). A step with a condition shows the condition (and keeps its chance); a step without shows its
  chance. Turning to a condition sets the condition and leaves the chance alone; turning back below it sets the condition
  to none and the chance to 100 (the position says 100). KNOB 2 **RATCH** x1 .. x4, FLAM (`step_rflam`, 1 .. 5). KNOB 3
  **NUDGE** (`step_nudge`, its own range, shown as +6). KNOB 4 **TYPE**: -, ACC, SLD, A+S (the accent and slide flags).
- **Every selected step**: the new value is worked out from the cursor's step (the dial shows it) and **set** on every
  step of the range, not added (so the range ends equal and the turn is predictable). With no range, the cursor's step.
  Steps that are off are skipped (a property of nothing); a recorded step's properties are written to its view cell as the
  tracker's FX column does (`recording_view`; the notes play them as a group, 0.13.0), then `recording_resched` for the nudge.
- The same acceleration as the tracker's FX (`list_accel` for the chance). Undo: through the step history as the tracker's
  FX edits are (a manual step) and the same rule for a recorded step; one detent, one undo step.
- **Selecting.** The same gesture as NOTES (OCT+ held with KNOB 1, KNOB 2 or SELECT: `bulk_jog`), its context widened to
  FEEL; the range is steps (`bulk.on == 1`). OCT- clears it. PRESETS moves the cursor a step. With the switch off nothing
  of this exists.
- **The state**: `ui.feel_k` (1 B, the knob turned last), in `ui` padding if it fits (checked by the data bank's margin).
- A drum track: FEEL edits the same step properties (the drum grid steps are `step_t`s too).

**4c. TRACKER's KNOB 4, a note's own length.** In a note column on a **manual** step whose cell holds two or more notes,
KNOB 4 no longer resizes the whole step: the step is converted to events (`note_convert`: the whole chord, equal lengths, one
undo through `note_transaction`; refused with RECORDING FULL if the pool cannot hold them, nothing changed) and the column's
note's own length is set (slot 5, a whole step a detent) so the notes now differ. A one-note step and the VEL column keep
today's whole-step resize (the note's own length is the step's). On a step that is already recorded nothing changes
(0.7.1). EDIT held undoes the conversion and the length together (one undo).

**4d. The SHIFT combos on NOTES and FEEL** (the design's list; step 3 left them here). SHIFT is HOME held; each is on the
map before it is in the code.
- **SHIFT + PRESETS**: stretches the selection (the range's end a step a detent; the anchor is where the selection began;
  with no selection one is begun at the cursor), as OCT+ held with KNOB 2 does.
- **SHIFT + EDIT**: deletes the selection (NOTES: `notes_bulk_delete`, as the sheet's "Delete selected"; FEEL: resets the
  chance, condition, ratchet, nudge and type of the selected steps to their defaults, one undo). With no selection NOTES'
  EDIT's own delete (the picked note); on FEEL the cursor step.
- **SHIFT + a knob**: the fine turn as on HOME, where the knob has one (CHANCE one percent, NUDGE one detent, no
  acceleration).
- **SHIFT + OCT- / OCT+: copy and paste of a selection are not built** (the sound's copy and paste of step 3 is the same
  kind of clipboard: it needs the whole selection's events in `.pool` and a paste through the pool's checks, an undo, and
  the same for FEEL's properties). Left for their own step after 4; the map shows them as LATER, as it does on HOME.

**What is left to a later step.** The **one-authority cleanup** (TRACKER_REVIEW.md "Left to do", point 3: `recording_prune`
goes) is not done here: it waits on the pads' event editor being brought under the shared one (point 1), which is a change
to the pads' path with no user-visible gain in this step. The pads' editor and keys on a recorded step likewise. Written
down for a step of their own; `recording_prune` stays.

**Flash and RAM.** Estimated 5,000 B of flash (the budget table above: NOTES' remap 0.5 KB, FEEL's page and edit 2.5 KB,
the tracker's conversion 0.3 KB, the combos and the map 0.7 KB, the strip and tabs 1 KB); RAM 1 B (`ui.feel_k`), no new
.data and .bss otherwise.

**Tests (host).** tests/step_pages_test.c (job `step_pages_job`, ASan and UBSan, over the host UI as ui_test does): NOTES'
knobs write pitch, onset, length, velocity of the cursor note each through one undo; START wraps at the phrase's end and
refuses an occupied cell; PRESETS walks the notes of a chord then the steps; the zoom on ENV + PRESETS; a range keeps KNOB
1 as the move; FEEL: each knob writes its field on the cursor step and on every step of a range (set, not added), off
steps skipped, a recorded step, the chance and condition scale at both ends and across 100; undo restores; the page's
order in the SEQ cycle; the tracker's conversion (a chord, a one-note step, VEL, the pool full, undo); the combos. With the
switch off the screens and the hash of ui_test equal release-2's. ui_render.c: FEEL and NOTES with the new strip on every
palette, linted.

### The design review of step 4 (an independent agent, October 29, 2026)

| # | Finding | Severity | Done in the design |
|---|---|---|---|
| B1 | 4c's one undo cannot work: `note_convert`'s transaction is invalid once the next event edit bumps `recording_generation` | blocker | conversion and the length in **one** `note_transaction`, looping the chord (as `note_manual_length`); it refuses while the song plays, a chain runs or a recording is armed ("STOP TO EDIT"); in those cases KNOB 4 keeps today's whole-step resize (no regression) |
| B2 | FEEL has no editor route: `step_edit` would send its knobs to the cursor and to `notes_event_edit` | blocker | a GR_FEEL branch at the top of `step_edit`, before the recorded-note routing |
| B3 | `edit_param` exempts slot 0 of SC_STEP pages from the frozen-phrase guard | blocker | FEEL's edit calls `frz_locked()`, `chain_busy()` and `live_rec_sel()` itself, on every knob |
| S1 | EDIT held (undo) and EDIT tap do not exist on FEEL (`step_modifier_mask`, `step_delete_edit`) | should fix | GR_FEEL added to both; EDIT tap on FEEL resets the cursor step's feel (the range's with SHIFT); EDIT held undoes |
| S2 | The selection does not survive NOTES to FEEL; OCT+ release would open "Selected notes" (and delete notes) on FEEL; `bulk.on == 2` has no step meaning | should fix | `bulk_context`, the clearing rule at 2451 and the OCT+ paths widened to FEEL; FEEL opens no sheet; entering FEEL with `bulk.on == 2` clears it |
| S3 | ENV + PRESETS (zoom) must set `step_used`, or ENV's release opens the ENV family | should fix | done in the design: the zoom sets `ui.step_used` |
| S4 | Recorded steps' properties are lost by existing event code (`notes_rebuild` recomputes ACC from velocity; a cross-cell move empties the old cell; `note_rebuild_all` and `note_transform` keep only chance and slide) | should fix | START carries the step's properties to the new cell when it empties the old one; the rest is **release-2's existing behavior, documented and tested, not changed** (the tracker's FX column has it today): an accent set by FEEL on a recorded step is lost at the next pitch, length or velocity edit of that cell |
| S5 | Manual and empty steps unspecified after the remap | should fix | below: the knob to slot map with the manual paths; START on a manual step is refused ("START: RECORDED NOTES") |
| S6 | LEN's grain (design: a sixteenth); the map's fixed tiles; SHIFT + EDIT is removed from `pressed` | should fix | LEN keeps the quarter step a detent as today, ENV held the sixteenth (a deviation, written); the map gets per-page tiles (STRETCH, DELETE, LATER for copy and paste); delete called explicitly |
| S7 | FEEL's tile is the view cell (rounded forward); the NOTES cursor is the actual step | should fix | FEEL works on view cells, as the tracker's FX column; the strip names the cursor's cell |
| S8 | Drums: PRESETS on the grid; NUDGE needs `ST_NOTE` and a non-recorded cell | should fix | the grid keeps today's controls (PRESETS falls through as today); FEEL on a drum track edits its cells, a knob that does nothing there is drawn dim (checked in the code when built) |
| m | The foot is `ui_draw.c`'s; the OCT+ menu hint goes; the SEQ family reaches 11 pages (the dots); `page_scroll` is in ui.c; scripts that use a page index (tools/hw) | minor | decided: the foot as `notes.png` shows (PRESETS STEP, SEQ NEXT); the dots as the header's rule; no script is touched; page indices checked |

Verified correct by the reviewer: the slot table of `notes_event_edit`, one undo an edit, `notes_step_jog`, the range and
move gestures, the step setters, the step history holding cond, nudge, ratchet and flam, nothing persisting a page index,
no test counting pages, and none of the owner's decisions violated.

**The knob map after the review (4a).** On a melodic NOTES page, no range and no move armed: KNOB 1 slot 1 (PITCH: a recorded
note's pitch; a manual note's pitch or, on an empty step, the last note, as KNOB 2's paths are today), KNOB 2 slot 6
(START, recorded notes only), KNOB 3 slot 2 (LEN, quarter step, ENV fine), KNOB 4 slot 3 (VEL, as today). A range or a move armed:
as today (KNOB 1 moves it, KNOB 2 the range's end).

### 4. The step pages (the design as built)

**Status: Done (host), merged into ui-build after the diff review.** Files: firmware/src/ui_feel.c (FEEL's reading, text,
signature and tiles; included by ui_graph.c), ui_feel_edit.c (FEEL's edit, NOTES' knob map, `bulk_stretch`,
`tracker_chord_length`; included by ui_input.c), hooks in ui_input.c, ui_shift.c, ui_draw.c, ui_graph.c, params.c (the FEEL page
after NOTES), core.h (`OC_STEP_PAGES`, `GR_FEEL` 0x60 outside fw_pages.h's enum so the module interface's id does not move),
tools/build.py (the switch). With the switch off the code is not compiled and the image is byte-identical to release-2 (`cmp`).

**As the design said, built:** NOTES (melodic): KNOB 1 PITCH, 2 START (slot 6, a sixteenth of a step a detent, wraps at the
phrase's end, refuses an occupied cell), 3 LEN, 4 VEL; PRESETS the cursor; ENV + PRESETS the zoom (and `step_used` set); a range
or a move armed keeps KNOB 1 as the move. FEEL: CHNC (chance, then FILL .. 4:4 on one scale of 112), RATCH, NUDGE, TYPE on every
selected step (set from the cursor's value); the guards (frozen, chain, live recording) of its own; EDIT tapped resets the
step, held undoes; no sheet on FEEL. The tracker's KNOB 4 on a manual chord: one transaction, one undo. SHIFT on NOTES and FEEL:
+ PRESETS stretches the range, + EDIT deletes it (FEEL: resets it), + a knob a fine turn; the map names STRETCH and DELETE there;
copy and paste still say LATER.

**Deviations and decisions (the safer reading):**
- The strip's label is **CHNC**, not CHANCE (the card holds four or five capitals: the lint). A tile's TYPE is AC, SL, AS (two
  letters: a tile has 55 px); the dial says ACC, SLD, A+S.
- LEN keeps the quarter step a detent (ENV held the sixteenth), as today; the design said a sixteenth.
- The foot of NOTES keeps release-2's hints (ADD, NOTE MENU, DELETE, DONE), not the mockup's; the foot belongs to step 9 with the
  sheets' (step 2's list). FEEL's foot is the generic one (the sound's numeral and name).
- START does not carry a step's feel to the new cell, and **release-2's loss stands** (`notes_rebuild` recomputes ACC from the
  velocity and drops a cell's feel when its last note leaves; a range move keeps only chance and slide): an accent set on FEEL
  on a recorded step is lost at the next pitch, length or velocity edit of that cell. Not changed: the same flaw is in the
  tracker's FX column today; the fix belongs with the one-authority cleanup.
- **Not built, for their own step:** copy and paste of a selection (and of a sound, step 3's item); the one-authority cleanup
  (`recording_prune`; it waits on the pads' editor being shared, TRACKER_REVIEW.md point 1).
- While the song plays, a chain runs or a recording is armed, KNOB 4 in a chord's note column keeps release-2's whole-step resize
  and says "STOP FOR NOTE LENGTH" (the conversion needs a stopped song, as NOTES' own length edit does).
- On a drum track's roll PRESETS keeps release-2's zoom; the new knobs apply to melodic tracks.
- FEEL's range follows the cursor: if PRESETS moves the cursor off the range the edit is the cursor's step alone.

**The diff review (an independent agent, October 30, 2026).** Found and fixed: (1) the tracker's column did not follow the note
after the conversion (a chord entered 67, 60, 64: two detents lengthened two different notes; now the column follows, tested);
(2) SHIFT + EDIT on NOTES deleted a step while a chain ran (now "STOP TO EDIT" as EDIT tapped, tested); (3) PRESETS changed on a
drum track's roll (now release-2's there); (4) KNOB 4's two meanings now say why; (5) the length clamp reached 128 steps, which
does not fit `note_duration` (now 127); (6) the START message says RECORDED ONLY; (7) the range and the cursor (above); (9) tests
that could not fail made exact (the zoom, START's wrap and its way down) and the chain and non-ascending chord tests added.
Not changed: a fine turn of LEN, VEL and START is the same as a plain one (they do not accelerate: cosmetic). The reviewer found
correct: the interrupt on and off on every path, the one transaction and its undo, every range, the history holding every field
FEEL edits, nothing in the firmware or tools keyed to SEQ's page numbers, the signature covering every field the tiles read.

**Flash and RAM (CI's build options, `build.sh`, niced):** every switch but this one: 550,448 B; with it **554,688 B: +4,240 B**
(estimate 4,000), .data+.bss unchanged (94,504 B), arena 84,288 B (unchanged). Free flash 22,560 B, 14,368 B above the reserve.
Switch off: 539,212 B, byte-identical to release-2.

**Host tests:** tests/step_pages_test.c (step_pages_job, ASan and UBSan): 46 checks, 0 failed (the order of SEQ's pages; NOTES'
four knobs and one undo each; START's wrap, refusal, the manual note's refusal; PRESETS and the zoom; the range keeping KNOB 1;
FEEL's every knob and its ends, the 100 % crossing, a range set and not added, the guards, undo; the tracker's chord, its undo, the
one-note and playing cases, the non-ascending chord; SHIFT + PRESETS and + EDIT on both pages; the chain). sheet_test and
shift_test pass with the switch on. tests/ui_render.c (ui_renders_steps): FEEL (as set, each knob last turned, a range, playing,
empty), NOTES' strip and both maps in 8 palettes: 0 lint findings, 0 MONO pixels off gray.

**The mockups (docs/orangecat/ui/new/, NIGHT unless named):** `step4_sheet.png` (the contact sheet), `step4_feel_set.png`,
`step4_feel_k1.png` .. `step4_feel_k4.png`, `step4_feel_range.png`, `step4_feel_play.png`, `step4_feel_empty.png`,
`step4_notes_rec.png`, `step4_notes_manual.png`, `step4_notes_none.png`, `step4_shift_map_feel.png`, `step4_shift_map_notes.png`,
`day_step4_feel_set.png`, `day_step4_notes_rec.png`.

## Step 5: EDIT > LANES

**Status: Done (host).** Behind `OC_LANES` (core.h, default 0; needs `OC_STEP_PAGES`). UI_DESIGN.md Part 1
section 5, Part 2 `drum_lanes.png`.

### 5. EDIT > LANES (design note)

**What the code has today.** DRUM-X's lane pages (release-2, Session E, params.c: SC_DXLANE) are three pages of four values:
LANES (LEVEL TUNE DECAY PAN of the lane `ui.lane`), LANES 2 (LAYER PUNCH ATK+ BEND), LANES 3 (the kit's TONE FDBK HANDS DUCK).
`dxl_desc` and `dxl_value` give any lane's value; `dxl_put` writes the kit; a key struck on the track picks its lane
(ui_input.c); EDIT's sheets (step 2) already list the three pages. The parameters are Session E's and exist on release-2.

**What changes (the design's section 5):** LANES becomes the table. The four cards are the picked lane's LEVEL, TUNE, DECAY, PAN in
the knobs' cozy colors with their plain names (release-2's first card says "SD LV"); under them the eight sounds (BD SD CP CH OH TM
RS CB) as rows; the picked lane's row shows its four numbers in the knobs' colors, the other rows their bars (LEVEL from the left,
TUNE, DECAY and PAN from the middle); **PRESETS picks the lane** (a key struck does too). Nothing of the editing changes: the
knobs go through `page_desc` and `edit_param` as before; LANES 2 and 3 and the kit's save and load are untouched. Behind the
switch the page's graph kind is `GR_LANES` (0x61, outside fw_pages.h's enum); off, `GR_NONE` as release-2's (the image is
byte-identical).
**Files:** ui_lanes.c (the table, its signature; included by ui_graph.c), hooks in ui_draw.c (the cards), ui_input.c (PRESETS),
ui_graph.c, params.c, core.h, tools/build.py.
**Safer readings:** the kit's values are not in `p[]`, so the page's signature hashes the 32 values (`lanes_sig`); DRUM (the
Melodee drum engine's) LANES pages keep release-2's screens (only DRUM-X's page is the table).
**Review:** for steps 5 and 6, which are small and read the existing pages unchanged, one independent agent reviews the note and the
diff together (decision of this branch, to keep the cost in proportion).

### 5. The design as built

Built as the note says, with the review's fixes. **As built the page is the mockup's:** the sheets' tabs, the four headings in the
knobs' cozy colors (they stand where release-2's cards were), then the eight rows, the picked lane's with its four numbers, and the
foot (the sound's bar). The page is drawn by `pv_draw`'s RINGS path (`pv_kind`), which keeps the sheets' tab strip.
**The diff review (an independent agent, October 30, 2026), found and fixed:** (1) the first build drew the table through
`draw_graph` and the sheets' tab strip vanished on LANES (a step 2 regression: the screens in `step5_*` showed no tabs); the table now
lives in the RINGS path under the tabs, and the cards give way to the headings (lanes_test checks the tab strip is drawn); (3) the
tests that could not fail: the table's pixels change with each lane picked and the tab strip is checked. **Accepted and written
down:** (2) PRESETS picks the lane on LANES (the design's text) so PRESETS no longer turns the sheets there: EDIT and SELECT
still do; (4) the table shows the numbers without units (the mockup's: the headings say what they are; TUNE is in semitones,
LEVEL in percent, DECAY and PAN as the cards formatted them); (5) the picked row reads `dxl_desc` and so rewrites the shared
descriptor of card k with the same lane and id: harmless today (no cards are drawn on this page), noted. The reviewer found correct:
the value formats, every index masked, the signature covering the kit's 32 values and `p[]`, GR_LANES leaking nowhere, the key pick and
the edit path untouched, the Melodee DRUM engine's lane pages untouched, switch off identical.

**Flash and RAM (CI's options):** every switch through step 4: 554,688 B; with OC_LANES **557,968 B: +3,280 B** (estimate 1,500; the
table's two layouts and the headings cost twice the guess); .data+.bss unchanged (94,504 B); the audio arena 84,064 B (was 84,288 B;
the minimum is 49,152 B). Switch off: 539,212 B, byte-identical to release-2.
**Tests:** tests/lanes_test.c (ASan and UBSan, 10 checks, 0 failed): the page, PRESETS both ways and at the ends, the four knobs edit lane
3's values and no other lane's, the signature follows a kit value, the table under the tabs changes with each lane picked, the tab strip is
drawn, a key struck still picks its lane. ui_render.c (ui_renders_steps now with OC_LANES=1): the table with lane 1, 3 and 8 picked and the
kit moved, in 8 palettes: 0 lint findings, 0 MONO pixels off gray. **Mockups:** `step5_sheet.png`, `step5_lanes_1.png`, `step5_lanes_3.png`,
`step5_lanes_8.png`, `day_step5_lanes_3.png`.

## Step 6: the mixer in the knobs' colors

**Status: Done (host).** Behind `OC_MIXER` (core.h, default 0; needs `OC_LANES`). UI_DESIGN.md Part 1 "The mixer", Part 2
`mixer.png`, Part 3 step 6 ("+0.3 KB, ui_render.c's mixer screens").

### 6. The mixer (design note)

**What the code has today.** GLO's first page, MIXER (SC_TRK, GR_TRK), is one row a track (ui_pages.c `pv_strip`): the numeral on the
track's color, the sound's name, the level in dB, the level's bar with the meter under it, the pan or Mute or the freeze word at the
right. KNOB 1 LEVEL, 2 PAN, 3 FREEZE, 4 MUTE of the **selected** track (`tracks_edit`); the knob just turned is shown by the row's
`hot` column in the text color.

**What the design shows and what is built.** `mixer.png` is four vertical faders with "K1-4 LEVELS": knob k is track k's level. That
layout would take PAN, FREEZE and MUTE off the knobs, and the design says nowhere where they go; Part 3 step 6 itself costs
only 0.3 KB and tests "the mixer screens", so it is the **colors**. **Decision (the safer reading):** the layout and the knobs stay
release-2's (no control lost); the colors become the knobs': the level's bar in KNOB 1's blue (`T_COZY(0)`) and its dB value in blue while KNOB 1 turns (at rest the text's), the
pan in KNOB 2's green, the freeze word in KNOB 3's white, a turning knob's value in its own cozy color instead of the text's; Mute
stays REC's red (it is a state, not a knob's value) and takes KNOB 4's orange only while KNOB 4 turns; the numeral stays the
track's color (the track's identity). Muted rows stay dim. The vertical faders and knob k = track k are for the owner to decide
(written under "Waiting on the owner").
**Cost:** a handful of color changes in `pv_strip`: estimate 300 B (the table says 1,200 after step 5's miss). RAM 0.

### 6. The design as built

Built as the note says: `MXC(i, old)` and `MXBAR` in ui_pages.c `pv_strip` (the level's bar in KNOB 1's blue, a turning knob's value in
its own cozy color; the numeral, the meter and Mute's red are release-2's); `OC_MIXER` in core.h and tools/build.py. **Flash and RAM (CI's
options):** every switch through step 5: 557,968 B; with OC_MIXER **557,872 B: -96 B** (the one color call replaces two conditionals;
estimate 300), RAM 0, arena unchanged (84,064 B). Switch off: 539,212 B, byte-identical to release-2.
**Tests:** tests/mixer_test.c (ASan and UBSan, by the screen's pixels, 5 checks, 0 failed): the level bars are KNOB 1's blue; KNOB 2
turned shows the pan in green and still changes the pan; KNOB 4 mutes and its word turns orange; a muted track's bar is dim; KNOB 1
is still the level. ui_render.c (every mixer screen of the renderer, now with OC_MIXER=1 in ui_renders_steps): 0 lint findings, 0 MONO
pixels off gray. **Mockups:** `step6_sheet.png`, `step6_mixer.png`, `step6_mixer_pan.png`, `step6_mixer_frozen.png`, `day_step6_mixer_pan.png`.
**The review (an independent agent, October 30, 2026):** no bugs; the switch-off expressions are release-2's, the hot-column mapping and the signature are right. Minor, kept: the BAR check counts the blue over the lower screen (loose: a track color equal to the cozy blue would pass), and nothing checks that the freeze word turns white; the note's sentence on the dB value is corrected above.
**For the owner (decision left open):** `mixer.png`'s layout (vertical faders, knob k = track k's level) would take PAN, FREEZE and MUTE
off the knobs; it is not built (see the note).

## Step 10: copy and paste of a sound (OC_CLIP; October 30, 2026)

**Switch** `OC_CLIP` (core.h, default 0, listed in tools/build.py; needs OC_MIXER). Off, the build is byte-identical to the one without it
(cmp of melodee.bin against release-2 2b46f80 with every switch off: identical).

**Design.** SHIFT + OCT- copies the selected track's sound into one clipboard; SHIFT + OCT+ pastes it on the selected track. Both act on
HOME's pages and on the sheets (where the SHIFT combos act); on NOTES and FEEL they still say LATER (step 11 is the selection's).
- **A sound is what EDIT held swaps for a sound load** (ui.c `undo_step`): engine, preset, user slot, every parameter `param_kept` does not
  keep, the native FM6, CZ-1 and Prophet patches, the DRUM patch or DRUM-X's kit. About 0.75 KB of RAM (`snd_clip_t`).
  The mix (LEVEL, PAN, MUTE), ARP, the pattern parameters, the effects and DUB's setup stay the track's own; the steps stay; **motion is not
  copied** (a sound load resets a track's motion, and so does a paste); the macros' dials come across at 0, as a user preset load leaves them.
- **A paste is a sound load** (`load_begin` / `load_end`): one undo level, EDIT held gives the old sound back whole.
- **Refusals, whole, the track keeps its sound:** nothing copied ("NOTHING COPIED"); the engine is not there (`eng_ok`: a module not
  installed, a retired slot: "ENGINE NOT HERE"); the unit cannot carry it (`eng_admit`: "ENGINE TOO HEAVY" / "NO ROOM").
- **The clipboard survives** a paste, a change of track and a project load (it is data; a project that lacks the engine refuses by the
  rule above). A power cycle clears it (RAM only; nothing is written to flash).
- **The map:** the COPY and PASTE tiles are tinted and live where the combos act, dim elsewhere; on NOTES and FEEL they stay LATER.
- **After the review (below):** a paste is refused during a chain or a live recording ("STOP TO EDIT", as INIT) and on a frozen
  phrase ("FROZEN"); it seals the undo level first, so EDIT held returns to before the paste, not before an earlier browse; values
  are clamped to the engine's ranges (a module replaced in its slot since the copy); a user slot erased since the copy is not
  pasted as a stale name; copy refuses a retired engine; both OCT keys in one scan copy only. FM6's operator switches (`fm6_on`,
  `fm6_fn`) are not copied, as undo and a user preset load do not copy them.
- Decisions: the clipboard is one sound, not a list; copy keeps the user slot and name (`user`), so a pasted user sound still shows its
  name and a later save-as works as for a loaded one (the safer reading: the record the sound came from is what the user sees).

**Tests** `tests/clip_test.c` (16 checks, ASan and UBSan): copy, paste on another track (engine, parameters, the mix and steps kept, the
macro dial 0), one undo, paste twice, the clipboard across a change of track, the two refusals that need no module, DRUM-X's kit, the map.
Run by `run_tests.sh` (job in step_pages_job); the renders gained `-DOC_CLIP=1`.

**Review (an agent, note and diff together, as for steps 5 and 6; no blocker):**

| # | Finding | Severity | Fix |
|---|---|---|---|
| 1 | Pasted values unclamped (a module replaced in its slot) | should-fix | clamped to `param_desc_of(eng, id)` |
| 2 | No chain or live-recording guard, though a load resets motion | should-fix | "STOP TO EDIT" as INIT |
| 3 | Both OCT in one scan: copy then a no-op paste | should-fix | `else if` |
| 4 | Frozen track allowed | note | refused ("FROZEN"), the safer reading |
| 5 | A paste after a browse merges into the browse's undo level | note | `undo_seal()` first; tested |
| 6, 7 | `fm6_fn`/`fm6_on` and `p5_track_accept` not handled | note | as undo; written above |
| 8 | A stale user slot name | note | cleared if the slot is not used |
| 9 | Copy on a retired engine says copied | note | refused at copy |
| 12 | Test gaps (native patches, undo contents, merge case, guards) | should-fix | 5 checks added; not covered: the `eng_admit` refusal (needs a module in the test) and a clip of a removed module |

**Mockups:** `step10_shift_map_home.png`, `step10_shift_map_sheet.png`, `day_step10_shift_map_home.png` (0 lint findings, 0 off-gray MONO).

**Flash and RAM (CI build options).** Image 558,784 B (+912 B over steps 1 to 6, estimate 1,000); .data+.bss 95,256 B (+752); data SRAM tail
3,048 B; the arena line reads 84,064 B. Free of 577,248 B: **18,464 B, 10,272 B above the 8,192 B reserve.**

## Step 11: copy and paste of notes and of a step's feel (OC_CLIP_SEL; October 30, 2026)

**Switch** `OC_CLIP_SEL` (core.h, default 0, listed in tools/build.py; needs OC_CLIP). Off, NOTES and FEEL say LATER as before.

**Design.** The same two keys, SHIFT + OCT- (copy) and SHIFT + OCT+ (paste), on the two step pages. Melodic NOTES only: on a drum track's NOTES (roll
and grid) the combos say LATER and the tiles are dim; FEEL works on any track's steps.
- **NOTES, copy:** the notes of the selection as `note_transform` reads one (`note_transform_has`: a range, an event selection or the picked
  note), and, with no event selection, the manual notes of the range's steps or of the step under the cursor. At most 64 notes
  (`CLIPN_MAX`, about 0.9 KB of RAM), else "TOO MANY NOTES" and nothing kept. Each note keeps its distance from the **start of the earliest
  note's step** (so its onset inside a step comes across), its length, velocity and pitch, and its step's chance and slide. A manual step is
  copied as the events `note_convert` makes of it (each note at the step's length, an accent as velocity 127); the source stays manual.
- **NOTES, paste:** the notes are written at the cursor step, on this track and pattern or any other, as one note transaction (the
  history `EDIT held` uses): one undo. A position past the loop's end wraps to its start, as a move does. Refused whole, nothing written:
  nothing copied; the song playing, a chain running or a live recording ("STOP TO EDIT"); a frozen phrase; a landing step that holds a
  manual note ("STEP OCCUPIED"); not enough free recording memory ("RECORDING FULL"). The cursor goes to the first pasted note; the
  paste is not left selected (the safer reading: a following KNOB 1 is PITCH of the picked note, not a range move).
- **FEEL, copy:** the cursor step's chance, condition, ratchet or flam, nudge, accent and slide ("NO NOTE THERE" on an empty step).
- **FEEL, paste:** sets those values on every step of the range (or the cursor's) that is on, as `feel_reset` walks it, behind its guards
  (frozen phrase, chain, live recording); off steps are untouched; a recorded step's notes take the nudge (`recording_resched`).
- **The map:** the COPY and PASTE tiles are live on NOTES and FEEL too.
- Decisions: manual notes of a range copy as events (the same result a move gives them), not as manual steps; a pasted chord's
  step is therefore a recorded one; the clipboards are separate (a sound, notes, a feel), each kept until power off.

**Tests** `tests/clip_sel_test.c` (19 checks, ASan and UBSan): a range of recorded notes (distances, velocities, lengths, onsets inside the
step, one undo, twice, the wrap at the loop's end), a manual chord, the five refusals, FEEL's copy and paste with off steps and the empty
and nothing-copied cases. Run by `run_tests.sh`; the renders gained `-DOC_CLIP_SEL=1`.

**Review (an agent, note and diff together; one blocker):**

| # | Finding | Severity | Fix |
|---|---|---|---|
| 1 | A paste's overview rebuild cleared the chance and slide of every other recorded step in the pattern | **blocker** | saved before the rebuild and restored after it, as `note_transform` does; tested |
| 2 | Pasted notes landing on a recorded step overwrote its chance and slide | should-fix | a recorded landing step keeps its own; one that was not takes the copied notes' |
| 3 | A failed copy emptied the old clipboard | should-fix | a counting pass first; a failed copy leaves the clipboard as it was; tested |
| 4 | Drum track: the doc said the combos do not act; the tile looked live | should-fix | the doc says LATER on a drum NOTES; the tile is dim there (checked) |
| 6 | The free-slot search could in theory run to RECORD_MAX | note | `if (i >= RECORD_MAX) break;` |
| 12 | A stale comment about "longer than it fits" | note | removed |
| 13 | Gaps: cross-track or cross-pattern paste, `bulk.on == 2` copy, FEEL on a recorded step, redo, drum track | note | **not covered**; written here |

What the rebuild still clears (nudge, ratchet, condition of a rebuilt recorded step) is release-2's behavior for a move as well.

**Mockups:** `step11_shift_map_notes.png`, `step11_shift_map_feel.png` (0 lint findings, 0 off-gray MONO).

## Flash budget (October 29, 2026, after the merge of release-2 876955a)

**The merge.** release-2's local head (Session A's ~/OrangeCat, 876955a; origin/release-2 still holds e1c8ac3) merged into
ui-build in one commit. One conflict, tools/build.py's switch list (both lists kept). Three files changed on both sides
(core.h, params.c, build.py); the rest merged clean. No file of Session A's was edited here.

**Measured today** (tools/orangecat_engines.sh sourced, `build.sh`, niced, one build at a time; the area is 577,248 B):

| Build | Image | Free | .data+.bss | Data SRAM tail | Arena |
|---|---|---|---|---|---|
| release-2 876955a | 538,528 B | **38,720 B** | 93,640 B | 4,664 B | 142,976 B |
| ui-build, every switch off | 538,528 B | 38,720 B | 93,640 B | 4,664 B | 142,976 B |
| OC_HOME_PIC=1 | 550,368 B | 26,880 B | 94,024 B | 4,280 B | 142,976 B |
| + OC_EDIT_SHEETS=1 | 550,800 B | 26,448 B | 94,024 B | 4,280 B | 142,976 B |
| + OC_SHIFT_MAP=1 (steps 1 to 3) | 552,832 B | **24,416 B** | 94,024 B | 4,280 B | 142,976 B |

- **Switch off is byte-identical to release-2**: `cmp` of melodee.bin, melodee.pak and melodee.elf, all three equal.
- release-2 itself grew 1,280 B of flash and 16 B of RAM since e1c8ac3 (the DUB rounds). The data bank above its 4 KB block is
  now 568 B on release-2 and **184 B** with the switches (the same as before the merge).
- Steps 1 to 3 cost **14,304 B** (11,840 + 432 + 2,032 by switch; 9,536 + 2,544 + 2,048 = 14,128 at e1c8ac3). The split
  moved because the compiler inlines differently (draw_page and pv_draw trade 6 to 8 KB between them); the sum barely did.
- The asset pack is 40,232 B of 40,960 B (728 B spare), 115 B spare with HOME's trailer. **The pack's area (10 sectors,
  0x93000..0x9CFFF) cannot grow without a change of the flash map** (storage.c; the sectors above it are the library's and
  the project copies'): that is Session A's file.

**Estimates for steps 4 to 9.** The design's Part 3 figures for steps 1 to 3 added up to 8.4 to 9.4 KB and the builds came
to 14.3 KB: 1.5 to 1.7 times. Each figure below is **twice the design's upper figure** (the safer reading), adjusted by
reading the step. Unit: bytes of flash in the app image.

| Step | What | Design said | Estimate | Running total of the steps | Free after | Above the 8,192 B reserve |
|---|---|---|---|---|---|---|
| 1 to 3 | measured | 8.4 to 9.4 KB | 14,304 | 14,304 | 24,416 | 16,224 |
| 4 | the step pages: NOTES' bars and cursor, FEEL (new), TRACKER's per-note length, the one-authority cleanup | 2 to 3 KB | 5,000 | 19,304 | 19,416 | 11,224 |
| 5 | EDIT > LANES (8 rows by 4 columns; the three existing LANES pages stay until step 9) | 1 KB | 2,000 | 21,304 | 17,416 | 9,224 |
| 6 | the mixer in the knobs' colors | 0.3 KB | 700 | 22,004 | 16,716 | 8,524 |
| 7a | fw_pages version 3 (`scene`, `kdef`), the loader's check, the audit | 0.5 KB | 1,000 | 23,004 | 15,716 | 7,524 |
| 7b | the effects' picture pages (section 8): 8 pages, their sources, the FX stepping, the rows | not in the table | 3,500 | 26,504 | 12,216 | 4,024 |
| 7c | DUB TAPE's performance view (section 7): the page, its keys drawn in text, the sources | not in the table | 1,500 | 28,004 | 10,716 | 2,524 |
| 8 | the web editor (Session D) | web only | 0 | 28,004 | 10,716 | 2,524 |
| 9 | retire the old (HOME's default, EDIT 1 and 2, the shades) | -2 KB | 0 (a credit of about 2,000 only once measured) | 28,004 | 10,716 | 2,524 |

**Result: the plan fits on paper**, with 10,716 B free at the end and 2,524 B above the 8 KB kept for Release 2 and 3
fixes. It is not a comfortable fit: 7b and 7c, the least known (no mockup has been built as code), take most of the slack.
If the measures of steps 4 to 6 come in more than 2,500 B over the estimates in all, 7b and 7c will not fit and the
options below apply.

**Two things the table does not carry:**
1. **The pack has no room for the effects' and DUB TAPE's scenes** (7b, 7c). The design's sizes are 96 to 169 B each for
   eight effects and 229 B for DUB TAPE: about 1,300 B against 115 B spare. The options, in plain words:
   - the flash map gives the pack an eleventh sector (+4,096 B of pack, taken from a neighbor; Session A's call; the app
     image does not change);
   - the pack's tables are trimmed by 1.2 KB (Session A's tables: gen_pack's content);
   - the effects' scenes ride in a **module** of their own, read in place (step 7 already makes modules carry scenes), at no
     cost to the pack and about +0.3 KB of lookup code;
   - the track effects share fewer scenes (3 pictures instead of 8): about 700 B less scene, a weaker design.
   Step 7 is last, so the call can wait; it is listed under "Waiting on Session A".
2. **RAM.** The data bank is 184 B above its block. Steps 4 to 6 are planned with no new .data or .bss (the design says 0).
   Anything that would put the bank under 4,096 B stops and is written down (the rule of step 1).

**If the budget fails at a later step**, the options and what each saves, in bytes of the app image:
- the pictures' code moves into a module (the interpreter and the page code are about 9 KB with the pages; the module ABI
  would export the plot calls; 2 to 3 KB saved, a large change in Session A's module loader);
- a step trimmed: FEEL's conditions (about 700 B) or the effects' pages cut to the three track effects (about 1,800 B);
- release-2 made smaller by Session A (the size audit, BUILD.md "Free flash is the audit's limit"): every 1 KB there is 1 KB here.

**Gate for each step:** the step's flash and RAM are measured against this table when it is built; the next step goes on
only if the free flash left is at least 8,192 B plus the estimates of the steps after it. If not, stop and report.

## Budget revision 0.7 (October 30, 2026): CI's build options, and the effects' pictures out

**Correction.** The 0.6 figures were built with the defaults. CI builds with `MELODEE_DUAL_CORE=1 MELODEE_CORE1_FALLBACK=0
MELODEE_RAM_AUDIO=1 MELODEE_CACHE_RAM=1 MELODEE_ADMISSION=0 MELODEE_AUDIO_PROFILE=0 MELODEE_AUDIO_BENCH=0` and
tools/orangecat_engines.sh (build.yml, both steps). Every figure below is built that way (`build.sh`, niced, one at a time).
The release-2 merged is 2b46f80 (local head of ~/OrangeCat, fetched read-only). The 0.6 table is **superseded**.

| Build | Image | Free of 577,248 B | .data+.bss | Data SRAM tail | Audio arena (minimum 49,152) |
|---|---|---|---|---|---|
| release-2 2b46f80 | 539,212 B | **38,036 B** | 94,088 B | 4,216 B | 84,832 B |
| ui-build, every switch off | 539,212 B | 38,036 B | 94,088 B | 4,216 B | 84,832 B |
| ui-build, OC_HOME_PIC + OC_EDIT_SHEETS + OC_SHIFT_MAP | 550,448 B | **26,800 B** | 94,504 B | 3,800 B | 84,288 B |

- Switch off is byte-identical to release-2 (`cmp` of melodee.bin).
- Steps 1 to 3 cost **11,236 B** of flash and **416 B** of RAM with CI's options (14,304 B with the defaults).
- **RAM.** With CI's options the audio arena is 84,832 B against a minimum of 49,152 B: 35 KB of margin, not the 640 B of
  the defaults' build. The data bank (SRAM tail) on release-2 is 4,216 B, **120 B** above one 4 KB block; the switches take it
  to 3,800 B, under the block, and the arena gives up 544 B (84,288 B): the build's own check passes. The rule of step 1 ("stop
  if the bank goes under 4,096 B") is replaced by: **stop if the arena's available bytes fall below 49,152 + 16,384 B** (the
  minimum and a 16 KB margin). Each step reports both numbers.
- **The effects' pictures and DUB TAPE's view leave the budget** (the owner's decision, October 30): no flash-map change; the
  effects keep their current pages under the new interface; in Release 3 each effect becomes a module and carries its own
  picture (the owner's rule: a picture lives in its module; Session A's docs/orangecat/FX_MODULES_PLAN.md on release-2 is the
  plan). Where they go: UI_DESIGN.md section 8's scenes and DUB TAPE's `SCENE_DUB` (section 7) are **not** built in
  step 7; step 7 keeps only the module-picture format (7a) for the engines. The asset pack's 115 B spare is no longer a
  constraint on this branch.

| Step | What | Estimate | Running total of steps 4 on | Free after | Above the 8,192 B reserve |
|---|---|---|---|---|---|
| 1 to 3 | measured | 11,236 | | 26,800 | 18,608 |
| 4 | the step pages | **4,240 measured** (est. 4,000) | 4,240 | 22,560 | 14,368 |
| 5 | EDIT > LANES | **3,280 measured** (est. 1,500) | 7,520 | 19,280 | 11,088 |
| 6 | the mixer in the knobs' colors | **-96 measured** (est. 1,200) | 7,424 | 19,376 | 11,184 |
| 7a | fw_pages version 3: the scene and kdef of a module, the loader's check | 1,500 (raised after step 5) | 8,924 | 17,876 | 9,684 |
| 7b, 7c | the effects' pictures, DUB TAPE's view | removed (Release 3, modules) | | | |
| 8 | the web editor | 0 | 8,924 | 17,876 | 9,684 |
| 9 | retire the old | 0 (credit only once measured) | 8,924 | 17,876 | 9,684 |

(The estimates are the design's upper figure times the 1.5 that steps 1 to 3 measured with CI's options: 11.2 against 8.4 to
9.4 KB.) **The plan fits with 9.7 KB above the reserve (after step 6).** The measured cost of each step is entered here as it is built.

## Budget revision 0.8 (October 30, 2026): steps 10 and 11 (CI's build options)

| Build | Image (B) | Free of 577,248 B | Above the 8,192 B reserve | .data+.bss | Data SRAM tail | Arena (min 49,152) |
|---|---|---|---|---|---|---|
| release-2 2b46f80 | 539,212 | 38,036 | 29,844 | 94,088 | 4,216 | 84,832 |
| six switches (steps 1 to 6) | 557,872 | 19,376 | 11,184 | 94,504 | 3,800 | 84,064 |
| + step 10 (OC_CLIP), estimate 1,000 | 558,784 (+912) | 18,464 | 10,272 | 95,256 (+752) | 3,048 | 84,064 |
| + step 11 (OC_CLIP_SEL), estimate 2,000 | 562,308 (+3,524) | 14,940 | **6,748** | 96,312 (+1,056) | 1,992 | 84,096 |

Step 11 cost 3,524 B against an estimate of 2,000 (the two-pass copy, the paste and FEEL's setters). **6,748 B remain above the reserve**;
the data SRAM tail is 1,992 B (the arena rule, minimum plus 16,384 B, holds with 35 KB). **No further step is built.** Left in the budget:
7a (about 1,500 B) fits; a step of 5,000 B or more would not. Steps 10 and 11 are built but **off** in the build set.

## Where the plan stops (October 30, 2026)

Steps 1 to 6 are built, reviewed and merged. The budget holds (see the table: 17,876 B free with every switch through step 6, 9.7 KB
above the reserve). **Not built, and why:**
- **7a, fw_pages version 3 (`scene`, `kdef` in a module's page descriptor).** It changes `fw_pages_t`, so the fw_api id moves and every
  module built so far (WHEEL, op1r's three, SEM, HADES, the SHAPES proof of concept) is refused until rebuilt; it rewrites the same
  struct and `oc_mod_pages_check` as Session A's FX_MODULES_PLAN.md section 9 item 3 (a scene pointer in `fx_desc_t`), which says
  "to be agreed before either merges". It is Session A's module system and Release 3's format. **Waits for Session A** (below). Its
  estimate is 1,500 B of flash; the budget has room.
- **7b and 7c** (the effects' and DUB TAPE's pictures): Release 3, modules (the owner, October 30).
- **8, the web editor:** Session D's; the firmware side it needs (an editor command returning an engine's `kdef` and scene bytes, the
  PACK trailer's version) waits on 7a's format.
- **9, retire the old:** it flips the HOME switch's default from TRACKS to PICTURE and removes the dead paths (EDIT 1 and 2, the old
  knob shades, `smap`, the section map); the defaults of five switches become 1. That is the owner's call after trying the
  interface on the unit, and the removal wants the hardware checks below first. Not done: no default was changed.
- Also left (their own steps): copy and paste of a sound and of a selection; the one-authority cleanup; `mixer.png`'s layout.

## Waiting on Session A

| Item | What Session A is asked | Status |
|---|---|---|
| The macros in the audio interrupt (1a: oc_macro.c, the hooks in oc_throw.c) | Review and time it on the unit before any merge into release-2 (host: 613 to 642 ns a block for 36 targets; `fm1_timer5_irq` +300 B); choose whether mix.c calls it directly instead of the `thr_out` bit 31 | **Waiting** (code on ui-build) |
| The render loops' target cost on this machine's toolchain | release-2 e1c8ac3 itself fails it here: `dxe_post_sides` 9 (budget 8), `pl_osc_render` 280 (budget 133). Confirm on CI, or re-take the budgets | **For Session A** (not a ui-build change) |
| fw_pages version 3 (7a) against FX_MODULES_PLAN.md section 9 item 3: which design carries `scene` and `kdef`; the api id moves, so every module is rebuilt | Agree the one field set, then Session C builds 7a (about 1,500 B) | **Waiting** |
| The pack's room for the effects' and DUB TAPE's scenes | None: they move to Release 3's modules (the owner, October 30) | **Closed** |
| The pack with the scene trailer (1b) | Nothing unless a build sets OC_HOME_PIC=1; see "The asset pack change" | **Waiting** (on ui-build only) |

## The asset pack change (for the installer and Session A's builds, when it merges)

- **What changes:** with OC_HOME_PIC=1 the build's `melodee.pak` (and the release's `orangecat-<version>.pak`) is the
  same pack with HOME's scene trailer after it: 40,845 B instead of 40,232 (613 B more; 115 B of the area left). The first
  40,232 bytes, the header, `OC_PACK_HASH` (657ED950 at release-2 e1c8ac3) and `OC_PACK_LEN` are unchanged, so every
  module's pack hash, the stub's and the set's checks (card_install.c `oc_set_vs`) are unchanged. With the switch off
  nothing changes.
- **The installer (web, Session D):** to give a switch-on firmware its pictures, send the whole file (ED_PACK_BEGIN with
  the file's length, up to 40,960). To a firmware without the switch (release-2) send the first OC_PACK_LEN bytes: it
  refuses a longer install (`len != OC_PACK_LEN`). A firmware that reports nothing new cannot be told apart by
  ED_PACK_INFO today; a field or a firmware version check is needed (Session D's call).
- **The card:** a switch-on firmware installs either file; release-2's installs only the base file.
- **Session A's builds:** nothing unless OC_HOME_PIC=1 is set; build.py passes `--scenes` to gen_pack only then.
- **The release workflow:** `orangecat-<version>.pak` is whatever gen_pack wrote; its checksum in SHA256SUMS changes
  with the switch on.

## Hardware checks for later (not run: the unit is Session A's)

Every one on a switch-on build (`OC_HOME_PIC=1`), with the pack carrying its scene trailer installed from the card:
1. **The macros' cost in the interrupt** (Session A): the block's time with four SHAPES tracks, every macro turned, against
   the same with the macros at 0 (host: 613 to 642 ns a block, about 2,000 host cycles).
2. **The macros sound as the host says**: PLUCK on MACRO and SHAPES at both ends, TONE and TIMB on SHAPES, over a held note
   and a sequence; no click when a macro is turned while a note sounds (the targets are put on whole a block, as the throws).
3. **A save and a reload** with macros turned (project, user sound): the sound the same after it, the dials at the center.
4. **HOME's picture page at 10 frames a second**: no audio late halves or underruns added while it animates (the design's
   SPI budget, measured here on the host's screen diff), on each engine with a chord held and KNOB 2 turned; the LCD's
   frame time.
5. **The pack with its trailer**: installed from the card (the pack file and a set), the pictures shown; the base pack (no
   trailer) installed: the plain picture, MACRO and SHAPES still sound; release-2's firmware with the trailer pack (from
   the web installer's base part only).
6. **The cozy colors** on the panel, every palette (the host checks 3:1 on the ground; the panel's gamma is not the host's).
7. **MENU > HOME PAGE back to TRACKS** with macros turned: the sound does not change.

On a build with both switches (`OC_HOME_PIC=1 OC_EDIT_SHEETS=1`), step 2:
8. **The sheets by hand on every shipping engine**: EDIT from HOME, EDIT through every sheet (the FX link and back to
   Voice), SELECT down every row and across, PRESETS, ENV and LFO; nothing unreachable that release-2 reached.
9. **The frame time on a sheet** with the second row and the box (the sheets are worked out on every frame for the dots
   and the tabs): no late audio halves while SELECT is turned fast and a sequence plays.
10. **EDIT held (undo) and EDIT with OCT** on a sheet, and the layers (GLO, SCL held) over a sheet: as release-2.

On a build with every switch (`OC_SHIFT_MAP=1` too), step 3:
11. **HOME by hand**: taps step HOME's pages; held alone the map at 0.7 s and the MENU on the release; a hold closes the
    MENU and the release leaves it closed; SHIFT + each knob, PRESETS, EDIT, OCT on HOME, a sheet and SEQ.
12. **The panel's own scan** under SHIFT: HOME and EDIT (or OCT) pressed in the same scan, released in the same scan; a
    knob nudged while HOME is held (the MENU is cancelled and the value shows).

On a build with every switch through step 4 (`OC_STEP_PAGES=1`), step 4:
13. **NOTES by hand** on a recorded phrase and a manual one: KNOB 1 PITCH, 2 START (a sixteenth a detent, across a half step and across
    the loop's end), 3 LEN, 4 VEL; PRESETS through a chord's notes and the steps; ENV + PRESETS the zoom; a range selected (KNOB 1 moves
    it); EDIT held after each: one undo. Nothing unreachable that release-2 reached (KNOB 1 was the cursor).
14. **FEEL by hand**: CHNC across 100 into the conditions and back, RATCH to FLAM, NUDGE, TYPE; a range of steps; the sound of a chance,
    a condition, a ratchet and a nudge on a recorded chord (0.13.0's group rule); the tiles' redraw while a sequence plays (frame time
    with 16 tiles, no late audio halves).
15. **The tracker's chord length**: KNOB 4 in a note column of a manual chord stopped (one undo), and while playing (the whole chord, with
    its message).
16. **SHIFT on NOTES and FEEL**: + PRESETS stretches, + EDIT deletes or resets (and refuses while a chain runs), + a knob; the map's tiles.
On `OC_LANES=1`, step 5:
17. **DRUM-X's LANES table**: PRESETS and a struck key pick a lane; the four knobs edit it; the sheets' tabs; the screen on the panel in
    every palette (the bars' contrast); the frame time while a pattern plays.
On `OC_MIXER=1`, step 6:
18. **The mixer's colors** on the panel in every palette (the blue bar on the track-tinted rows; the white and orange words).

## For Session A (October 30, 2026)

**The commit to merge.** The head of `ui-build` once this section is pushed (its hash is in the final report and in `git log -1 origin/ui-build`;
the host suite on it: ALL HOST TESTS PASSED with CI's settings, PARALLEL=1). It contains release-2 up to eb55c1d. What it changes outside
the interface's own files, so the merge is read knowing it:
- `tools/orangecat_engines.sh` sets `OC_HOME_PIC=1 OC_EDIT_SHEETS=1 OC_SHIFT_MAP=1 OC_STEP_PAGES=1 OC_LANES=1 OC_MIXER=1`, so a CI build is the
  interface build (the build and test steps both source it). Steps 10 and 11 (`OC_CLIP`, `OC_CLIP_SEL`) are **off** in it. The old code
  paths are all still in the image: with a switch unset the image is byte-identical to release-2 (cmp, checked for 10 and 11 too).
- **Not for release-2 until Session A has done them:** the knob macros' interrupt code (`oc_macro.c`, the `oc_throw.c` hooks) is in this
  build when `OC_HOME_PIC=1`. It is the code waiting for Session A's review and timing, so **a Nightly from this branch carries it**.
  Session A decides whether to merge before the review (Nightly only) or after.
- `web/fm1oc.js`, `web/editor.html`, `web/app/lib/editor_proto.js` (`packProblem`) and `web/make_site.py` (`check_set`) changed, with tests
  (see the pack below). These are the installer page's, the editor's and the site builder's files; the change is the minimum.
- Flash: 557,888 B with the six on (19,360 B free of 577,248; 11,168 B above the 8 KB reserve); .data+.bss 94,504 B; arena 84,064 B.

**The pack with the scene trailer.** With `OC_HOME_PIC=1` the build's `.pak` is 40,845 B: the 40,232 B of tables, then a 613 B scene
trailer ("OCSC", the pictures of MACRO, SHAPES and DRUM-X; 115 B of the 40,960 B area remain). The header's length, table hash and payload
CRC cover the **tables only**, so every module's `pack_hash` and the manifest are unchanged. What each side has to do:
- **The unit** (oc_pack.c, card_install.c) accepts any length from the tables' length up to the 40,960 B area; it checks the tables' CRC
  and the update loader's sector-tail rule over everything written. A firmware built **without** the switch refuses anything longer than
  the tables.
- **The installer page and the web app must send the whole file** (ED_PACK_BEGIN with the file's length, then every chunk), not the first
  40,232 B. Before this change their checks refused it (`packProblem` wanted the file's length to equal the tables'); now they check the
  header, hash and payload CRC over the first `buildLen` bytes and the tails over the whole file, and refuse a file longer than the area.
  `web/make_site.py check_set` does the same, or the Nightly's publish step would stop at "damaged (length or CRC)". Tests: `web/test_web.mjs`
  (one case), `tests/check_set_test.py` (four).
- **Open for Session A:** `ED_PACK_INFO` reports only the tables' length, so the page cannot tell a firmware that takes the trailer from
  one that does not. A trailer pack sent to a firmware without the switch is refused by the unit ("packFail"); the page's channel
  pairs the pack with its own firmware, so this only shows when a pack is installed alone on an older unit. A pack without the trailer on
  this firmware is safe: the pictures are absent, HOME draws the plain picture, and the sounds do not change.

**Smoke test, from the hardware checks 1 to 18** (the unit is Session A's; none run). "Harness" is what Session A's unit scripts can drive
and read (editor protocol, MIDI in, USB audio capture, the trace's per-job profile, late-half counters, a screen capture if the bench has
one); "person" goes to the owner's play test.

| # | Checks | What | Harness | Person |
|---|---|---|---|---|
| S1 | all | Boots; the version line; core 1 alive, no crash mark | identity and crash counters over the editor | the menu line fits |
| S2 | 5 | The pack with its trailer installed (the installer page, the Nightly channel): pack hash, trailer accepted; the base pack (no trailer): plain picture, MACRO and SHAPES still sound | pack hash and module list over the editor | the pictures |
| S3 | 1 | The macros in the interrupt: block time with four SHAPES tracks, every macro turned, against macros at 0; `fm1_timer5_irq` cost; late halves over a 10 minute sequence | yes (the trace) | no |
| S4 | 2 | PLUCK on MACRO and SHAPES at both ends, TONE and TIMB on SHAPES, over a held note and a sequence; no click on a turn | macro values, a capture's level and a click detector | how it sounds |
| S5 | 3, 7 | Save and reload with macros turned: the same sound, dials at the center; MENU > HOME PAGE back to TRACKS with macros turned: the sound does not change | render before and after and compare the capture | no |
| S6 | 4, 9, 14, 17 | Late audio halves and underruns with the screens animating: HOME's picture at 10 frames a second, a sheet with SELECT turned fast, FEEL's 16 tiles, the LANES table, each with a sequence and a chord held | late-half counters and LCD frame time from the trace, key and knob events injected | no |
| S7 | 8, 10, 13, 15 | Reachability: every release-2 page and gesture still reached (the sheets and the FX link, EDIT held undo, EDIT with OCT, NOTES knobs, the tracker's chord length) | partly: scripted key sequences against the host mockups in `docs/orangecat/ui/new/` | the feel of knobs and cursor |
| S8 | 11, 12 | HOME as SHIFT: the 0.7 s map, the MENU on a release, SHIFT with each knob, PRESETS, EDIT, OCT; two keys in one scan | key timing can be injected; the 0.7 s timer shows in the trace | the feel of the hold and tap |
| S9 | 14, 16 | FEEL and the SHIFT combos on NOTES: chance, condition, ratchet and nudge on a recorded chord sound as the host says; + PRESETS stretches, + EDIT deletes or resets and refuses while a chain runs | the sound by capture against the host render; the refusal by the screen text | whether it plays as intended |
| S10 | 6, 18 | The cozy colors and the mixer's bars in every palette on the panel | no (a photo can be kept) | **only a person** |
| S11 | all | Back to Standard: reinstall it, the unit boots, an old project loads | yes | no |

Pass: S1, S2, S3, S5, S6, S11 clean (numbers, not looks); S4 and S7 to S9 with no regression from release-2 the harness can see; S10 is
the owner's. A late half or underrun in S6 that release-2 does not show stops the Nightly from going to the owner.

## Step 12: the one-authority cleanup (not built; October 30, 2026)

TRACKER_REVIEW.md "Left to do" ties the cleanup (a manual gesture on a recorded step deletes its events through the history, and
`recording_prune` goes) to point 1, **the pads' event editor brought under the shared one**, which is a change to the pads' path with
no user-visible gain and which sits in `recording.c`, `note_tools.c` and the pads' input, the code every recording, NOTES edit and undo
runs through. The brief's test for this item was that it does not depend on 7a or on the owner's play test; it does depend on point 1,
which nobody has done. **The safer reading: not built.** Written so it is not lost: the order is point 1 (share the pads' editor), then
this, each behind its own switch with `tests/recording_props_test.c` as its gate and the unit's tracker checks (TRACKER_REVIEW.md) before
`recording_prune` is removed. It needs a step of its own with a design note by someone who has the pads' path open.

**Hardware checks added by steps 10 and 11** (the unit is Session A's; none run):
19. **Copy and paste of a sound** (SHIFT + OCT- / OCT+ on HOME, a sheet): between tracks of the same engine and of different engines
    (a built-in, a module, DRUM-X with an edited kit, FM6, CZ-1, the Prophet), the sound identical by ear and by the editor's dump;
    EDIT held returns the old sound; a module engine that was removed refuses.
20. **Copy and paste of notes on NOTES** (a range of recorded notes, a manual chord, between patterns and tracks): the phrase plays as
    the host says; the other steps' chance and slide unchanged; EDIT held undoes it whole; no late audio halves while pasting 64 notes.
21. **FEEL's copy and paste** on a recorded chord: the nudge and ratchet play as set.

## Decisions made in this branch (the safer reading, where the call was Session C's)

- BUILD.md kept as release-2's in the merge of ui-design (Session A owns it); ui-design's BUILD notes (0.79 .. 0.81) are
  summarized in UI_DESIGN.md and here.
- Free flash reported against the measured 40,000 B, not the brief's 65 KB.
- Macros bake into the saved values until the format revision (1a).
- SELECT on HOME unchanged in step 1 (1c).
- Cozy knob colors on the new HOME page only in step 1; light palettes get deeper shades (1c).
- Macros armed through thr_out bit 31 so mix.c is not touched (1a).
- HOME's page choice under MENU > HOME PAGE (TRACKS or PICTURE), not a DISPLAY > HOME row (1c).
- The scenes ride in a trailer after the pack's tables, outside the table hash, so release-2's pack, hash and modules stay
  valid (1b).
- The SHAPES module's knobs come from the built-in table (oc_knobs.c) like MACRO's and DRUM-X's (1a).
- The plain picture of an engine without a scene is its waveform in a 48-row band (1c).
- The picture's state (the scene cache, the note age) is kept for the selected track only (1c).
- The mockups live in docs/orangecat/ui/new/, since the repo ignores every `build/` folder.
- The render loops' target-cost failure is left to Session A: release-2 fails it the same way here (see the suite).
- Step 2 behind its own switch, `OC_EDIT_SHEETS`, which needs `OC_HOME_PIC` (each step behind the one before).
- The sheets are the sections made wider (no new table): computed from the pages on each use, so no cache and no cap.
- A sheet opens on its first row (no per-sheet memory: no RAM).
- PRESETS on a sheet turns the sheets (as it turned the sections); the design's "PRESETS: the sound" waits for step 3.
- EDIT on the FX pages goes on to Voice only when EDIT went through the FX link; from FX's own button, SOUND.
- SELECT and PRESETS skip the FX link; only EDIT goes through it.
- The tabs as the firmware already draws them (mixed case, as the sound designer's), not the mockup's capitals.
- On engines with LFO pages of their own the track LFO's sheet is "Trk mod".
- DRUM-X keeps the Mod and Voice sheets (everything stays reachable), beyond the design's KIT, LANES, ENV, FX.
- The second row and the box replace the scope or the wave only; the filter's curve and FM6's chart stay.
- List pages (VOICE, FM LFO, FM BEND), MOD's route page and the action pages (Store, Tools) keep release-2's screens with
  no tabs: written down for step 9.
- Step 3 behind its own switch, `OC_SHIFT_MAP`, which needs `OC_EDIT_SHEETS`.
- Decision 4 read as UI_DESIGN's text: HOME held and let go alone opens the MENU (on the release); a HOME tap keeps
  stepping HOME's pages. For the owner to confirm.
- SHIFT arms only on a HOME press begun with nothing modal up; one time, 700 ms, for the map, the MENU and its closing.
- The combos act on HOME's pages and the sheets only; elsewhere SHIFT's knobs, PRESETS and EDIT are eaten.
- Any other control used while HOME is down does its own job and cancels the MENU (no silent MENU after a touch).
- The fine turn is "no acceleration for that detent" (no per-knob residue in RAM for quarter steps).
- SHIFT + EDIT opens today's init confirm rather than initing at once.
- Copy and paste are not built in step 3 (no lossy clipboard); the map shows them as LATER.
- Step 4: FEEL's value is set on every selected step from the cursor's step (not added); a step without notes is skipped; the strip's
  label is CHNC and a tile's types AC, SL, AS (the lint); LEN keeps the quarter step; NOTES' foot hints unchanged; START does not carry a
  step's feel; the zoom is ENV + PRESETS; copy and paste of a selection left for their own step; the one-authority cleanup left.
- Step 5: the table is drawn in the RINGS path under the sheets' tabs, the headings standing for the cards; PRESETS picks the lane and
  so no longer turns the sheets there; no units in the table.
- Step 6: colors only (knob k = track k and the vertical faders are the owner's call: they would take PAN, FREEZE and MUTE off the knobs).
- Steps 5 and 6: one independent review of the note and the diff together (they are small and read the existing pages unchanged).
- Budget: estimates raised after each measured miss; the arena rule restated with CI's options (UI_BUILD "Budget revision 0.7").
- Step 7a, 8 and 9 not built (see "Where the plan stops"); no default of any switch changed.
- `git push` of a03cea8 (the release-2 merge, after its suite passed) to origin/ui-build before steps 4 to 6 were suite-tested.
