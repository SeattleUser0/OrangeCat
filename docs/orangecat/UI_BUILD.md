# Orange Cat: the new interface (build log)

**Version:** 0.3
**Date:** October 29, 2026
**Branch:** ui-build (Session C), cut from release-2 e1c8ac3 with ui-design 8dc42a4 merged in. Not on release-2, not on
main, not on the unit.
**Tracks:** docs/orangecat/UI_DESIGN.md 0.4 (the owner's decisions of October 29, 2026).

This file is the build log of the new interface: what is done, what is left to build, what is left to test. It merges
into BUILD.md when the interface merges (Session A owns BUILD.md; this branch does not edit it). Status keys as in
BUILD.md: **Done**, **In progress**, **Not started**, **Blocked**.

Revisions:
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
need another way in.

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

## Waiting on Session A

| Item | What Session A is asked | Status |
|---|---|---|
| The macros in the audio interrupt (1a: oc_macro.c, the hooks in oc_throw.c) | Review and time it on the unit before any merge into release-2 (host: 613 to 642 ns a block for 36 targets; `fm1_timer5_irq` +300 B); choose whether mix.c calls it directly instead of the `thr_out` bit 31 | **Waiting** (code on ui-build) |
| The render loops' target cost on this machine's toolchain | release-2 e1c8ac3 itself fails it here: `dxe_post_sides` 9 (budget 8), `pl_osc_render` 280 (budget 133). Confirm on CI, or re-take the budgets | **For Session A** (not a ui-build change) |
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
