# Orange Cat: the new interface (build log)

**Version:** 0.5
**Date:** October 29, 2026
**Branch:** ui-build (Session C), cut from release-2 e1c8ac3 with ui-design 8dc42a4 merged in. Not on release-2, not on
main, not on the unit.
**Tracks:** docs/orangecat/UI_DESIGN.md 0.4 (the owner's decisions of October 29, 2026).

This file is the build log of the new interface: what is done, what is left to build, what is left to test. It merges
into BUILD.md when the interface merges (Session A owns BUILD.md; this branch does not edit it). Status keys as in
BUILD.md: **Done**, **In progress**, **Not started**, **Blocked**.

Revisions:
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
