# Orange Cat: the new interface (build log)

**Version:** 0.9
**Date:** October 31, 2026
**Branch:** ui-fix (Session C), from linux-hw c27dc5c, after ui-build (cut from release-2 e1c8ac3 with ui-design 8dc42a4
merged in) was merged into linux-hw and published as Nightly v0.16.0-cb5048. The ui-fix work is the section "ui-fix
(October 31, 2026)" at the end.
**Tracks:** docs/orangecat/UI_DESIGN.md 0.4 (the owner's decisions of October 29, 2026).

This file is the build log of the new interface: what is done, what is left to build, what is left to test. It merges
into BUILD.md when the interface merges (Session A owns BUILD.md; this branch does not edit it). Status keys as in
BUILD.md: **Done**, **In progress**, **Not started**, **Blocked**.

Revisions:
- 0.9 (October 31, 2026, branch ui-fix): the owner's findings after Nightly v0.16.0: the FX button round every effect page
  (1), PRESETS on CHAIN for every chain (2), SHAPES' picture moving with its knobs (3), the effect pages in the owner's
  order (5), built; LFO 2 (4), the SHIFT map's removal (6), one authority for every note (7) and slide and legato (8)
  designed for review; the baseline and the budget measured on linux-hw with CI's build step (budget revision 0.9).
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

**Mockups (removed by ui-fix 6 with the map):** `shift_sheet.png` (the map on HOME, a sheet, SEQ's page, and HOME on DAY), `shift_map_home.png`,
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
`step4_notes_rec.png`, `step4_notes_manual.png`, `step4_notes_none.png`, `step4_shift_map_feel.png` and `step4_shift_map_notes.png` (the two map screens removed by ui-fix 6),
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

**Mockups (removed by ui-fix 6 with the map):** `step10_shift_map_home.png`, `step10_shift_map_sheet.png`, `day_step10_shift_map_home.png` (0 lint findings, 0 off-gray MONO).

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

**Mockups (removed by ui-fix 6 with the map):** `step11_shift_map_notes.png`, `step11_shift_map_feel.png` (0 lint findings, 0 off-gray MONO).

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
11. **HOME by hand** (as ui-fix 6 has it: the map withdrawn): taps step HOME's pages; held alone the MENU at 0.7 s with HOME still down;
    a hold closes the MENU and the release leaves it closed; SHIFT + each knob, PRESETS, EDIT, OCT on HOME, a sheet and SEQ, each begun
    within 0.7 s of HOME's press.
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
16. **SHIFT on NOTES and FEEL**: + PRESETS stretches, + EDIT deletes or resets (and refuses while a chain runs), + a knob.
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

## ui-fix (October 31, 2026): the owner's findings after Nightly v0.16.0

**Branch:** ui-fix (Session C), from linux-hw c27dc5c, the line of Nightly v0.16.0-cb5048 (ui-build merged at b401b03).
The ground rules above hold: everything new behind a switch (the switch of the step it belongs to, or its own), none of
Session A's files touched, builds niced and one at a time, host tests, the full suite with CI's settings, linted mockups,
flash and RAM against the baseline, an independent review of every design note and of every diff. The owner's list, in
his order: bugs 1 to 4, the design calls 5 and 6, the sequencer 7 and 8. Each item: what was found, what was done or
designed, its review, its tests, its cost; the budget table (revision 0.9) after item 8; what waits on Session A's unit
test; the decisions.

**The baseline, measured** (tools/hw/ci_step.py build: CI's own build step with the six switches of
tools/orangecat_engines.sh on, OC_CLIP and OC_CLIP_SEL off; 1 min 29 s niced):

| Measure | linux-hw c27dc5c |
|---|---|
| Image | 558,596 B of 577,248: **18,652 B free, 10,460 B above the 8,192 B reserve** |
| RAM .data+.bss | 94,536 B of 98,304 |
| Data SRAM tail | 3,768 B |
| Audio working arena | 84,032 B (minimum 49,152; the rule: stop below 65,536) |
| Asset pack | 40,845 B (115 B of the 40,960 B area left) |

### ui-fix 1: the FX button reaches every effect page (done)

**Found.** ui.c open_family: FX from any page but the family's first jumped to the first page ("the track's rack",
Felucca 0.8's rack navigation, 4182267); only from the first page did it step on. So FX showed the sends, then DIST, then
the sends again: two pages of fifteen. Release-2 had the same code; the sheets did not change it.

**Done** (OC_EDIT_SHEETS): FX from outside the family lands on its first page; on an effect page it goes to the next,
round every visible page and back to the first. EDIT's link into the effect pages, SELECT down them and EDIT on to Voice
are as step 2 built them.

**Test:** tests/sheet_test.c FXWALK: from HOME, FX lands on the first page; fifteen taps visit the fifteen effect pages
in the family's order and come round; from a sheet, FX lands on the first page.

### ui-fix 2: every chain reachable from CHAIN (done)

**Found.** On CHAIN nothing picks which chain the selected track plays: KNOB 1 is the step, KNOB 2 the track, KNOB 3 the
step's phrase, KNOB 4 its REPS, and each reaches 0 (checked on the host). Only SONG's OCT+ moved a track onto another
chain. On a new project the four tracks sit on chains 01 to 04, so a knob turned to find a chain walks the chips 01, 02,
03, 04 and stops: "will not go below chain 4". That is the reading taken (the decisions).

**Done** (OC_STEP_PAGES): PRESETS on CHAIN takes the selected track to the next or previous chain, 01 to 64, held at the
ends, through ocl_go_chain as SONG's OCT+ does (stopped: at once; an empty chain gets a new empty phrase, ready to
record); with the chains or the song playing: "STOP TO EDIT CHAIN". The message "T1 CHAIN 05" says where it went and the
chain's steps show at once. PRESETS was free on CHAIN, and it is the step pages' pattern (PRESETS picks the page's own
thing: the step on NOTES and FEEL, the lane on LANES).

**Test:** tests/step_pages_test.c CHAIN: up to 64 and down to 01, held at the ends; refused while playing; per track;
SONG's cells untouched.

### ui-fix 3: SHAPES' picture moves with the knobs (done)

**Found.** The cat's tail (K2 COLR, green: "the green hand") moved 13 px between COLR's ends and swayed 4 px; the paw
(K4 PLUCK) turned 14 degrees over PLUCK's travel; the yarn's size never changed. The scene's bindings were timid.

**Done** (the scene bytes in oc_scenes.h: the pack's trailer, not the image): every part moves with its knob's position,
in size and in speed. The yarn (K1 TIMB) grows from 10 to 38 px across TIMB and rolls at TIMB's rate (PH1); the tail (K2
COLR) lies along the floor at COLR's left end and stands up, curled over, at its right (the tip rises 24 rows), and the
tip flicks at COLR's rate (PH2: slowly over, then back); the moon (K3 TONE) fills to 24 px; the paw (K4 PLUCK) rises
through 70 degrees each side of its middle (140 end to end) over PLUCK's travel and swats through 80 on a note; the eyes follow the yarn twice as far.

**Tests:** tests/home_pic_test.c MOVES: on SHAPES each knob's two ends differ by 347 to 715 picture pixels, each above a
bar set over the old scene's readings (400, 330, 600, 400); with the knobs at rest the picture differs by 239 px 300 ms apart (the flick, the roll). HIT (379 px on a
note-on) and BUDGET pass as before. ui_render.c: the SHAPES screens in every palette, 0 lint findings.

**Mockups:** docs/orangecat/ui/new/fix3_shapes_sheet.png (as loaded; each knob at its left and right end; a note; every
knob turned; DAY) and the home_pic_shapes_* screens made again.

### The diff review of ui-fix 1, 2, 3 and 5 (an independent agent, October 31, 2026)

Found correct: open_family's switch-off body byte for byte release-2's, the generic branch's wrap and skipping of
hidden pages, every caller of open_family(FAM_FX), the PRESETS dispatch order with every switch on or off, the forward
declaration and the include order, pt_chain_turn's clamp, guard and message buffer, the untouched song row, the PG_FX
macros (the same entries, release-2's order with the switch off), nothing keyed to an effect page's index, the scene's
bindings against scene_ok (four BINDs at most, the point indexes, PH2 filled per knob, nothing leaving the box, +8 B in
the trailer), the fxorder render block, and that FXWALK and CHAIN fail with the fix reverted (FXWALK would read "2 taps
round"). Found and fixed:

| # | Finding | Severity | Fix |
|---|---|---|---|
| 4 | pt_chain_turn said "STOP TO EDIT CHAIN" for ocl_go_chain's 2, which after the busy guard is a flash error (song_go says "SAVE ERROR") | should fix | the 2 falls to "SAVE ERROR" |
| 16 | MOVES' 200 px bar is cleared by the old scene on knobs 2, 3 and 4 (307, 475, 333 px) and at rest (261): it guarded the tail only through knob 1 (190) | should fix | a bar per knob above the old readings (knob 1 400, 2 330, 3 600, 4 400), the old readings in the test's comment |
| 14 | The yarn spans 17 to 31 px, not 10 to 38: SF_R is a diameter and a centered source gives half the depth | should fix | depth 28: 10 to 38 px, as the note says |
| 13 | The paw's 70 degrees is each side of its middle: 140 end to end | minor | the comment and the note say so |
| 20 | "greys" | minor | "grays" |

### ui-fix 4: LFO 2 (design note, before its review)

**Found: it is how the code was inherited, not the sheets.** Melodee (Felucca 1.4) has one LFO a track: RATE, WAVE, PHS
and FADE on the LFO page, and that LFO's SYNC, TRIG and POL on a page named "LFO 2" (params.c; voice.c track_lfo_tick;
the state lfo_ph, lfo_val, lfo_fade, lfo_rnd, lfo_prev in track_t). The Mod sheet lists the pages as they are (LFO, LFO
2, LFO DEST, MOD). No second LFO exists anywhere in the firmware; SPEC.md 9.1's "two LFOs per synth track" was a
proposed target that was never built. "LFO 2" was Felucca 1.2's name for LFO 1's second page.

**Design: a second LFO a track, behind OC_LFO2.**
1. **Seven parameters,** appended just before P_E0 as core.h says new common parameters go ("user presets and projects
   map by count"); P_COUNT 124 to 131: `P_L2RATE` (RATE, F_LFOHZ 0 .. 127, default 60, as LFO 1's), `P_L2WAVE` (WAVE: SIN
   TRI SAW SQR S&H), `P_L2SYNC` (SYNC: OFF, 4BAR .. 1/32), `P_L2TRIG` (TRIG: NOTE, FREE), `P_L2POL` (POL: BI, UNI),
   `P_L2DST` (DEST: OFF, PITCH, CUT, SHP, AMP, PAN, RATE, VIB, E1 .. E8: the matrix's destinations but DEPTH), `P_L2AMT`
   (AMT -64 .. 63). Defaults DEST OFF, AMT 0: a loaded sound sounds as it did.
2. **Pages:** today's "LFO 2" renamed "LFO SYNC" (it is LFO 1's); after LFO DEST, "LFO 2" (RATE WAVE DEST AMT) and "LFO 2
   SYNC" (SYNC TRIG POL); then MOD. The Mod sheet gains two rows; the LFO button visits them.
3. **The audio side, in mod.c** (the matrix's file; nothing in voice.c, mix.c or an engine): per-track state outside
   track_t (`lfo2[NTRK]`: phase, value, the S&H value and the cycle before's: 14 B each, 56 B of .bss), so the module
   interface identifier (gen_fw_api_id.py hashes track_t's text) does not move for the state. Ticked once a block at the
   top of mod_begin (both the reference and the cached path), only while used (DEST with AMT, or a matrix slot with SRC
   LFO2); unused, no work, the phase holds. The waves as LFO 1's (its own copy of lfo_wave's shapes: that one reads LFO 1's
   state), the synced increment as lfo_sync_inc's with P_L2SYNC. TRIG NOTE: in mod_note (voice.c calls it on every
   note-on) when no voice of the track holds its gate, the phase restarts (LFO 1's "fresh phrase" rule). POL UNI as LFO 1's.
4. **The matrix:** MS_LFO2 appended to the sources (N_MSRC "LFO2"), bipolar; mod_tsrc returns its value. **Its own
   destination** is a fifth, implicit slot: the loops of mod_begin and mod_route_prepare read slot k through one accessor
   (k < 4: P_M1SRC + 3k; k = 4: SRC LFO2, P_L2DST, P_L2AMT); the arrays of `mod` and `mod_route_t` get one entry more
   (about 20 B of .bss, and the batch copy mix.c makes of `mod` grows with it). DEPTH is refused on the fifth slot (a
   matrix slot can still route LFO 2 into LFO 1's depth).
5. **Cost:** off, one compare more a block (the active-slot scan sees AMT 0). On: a tick (a table read, an add, a
   compare) and one slot's terms. tests/mod_test.c's cost check bounds it; the macros' rule applies: Session A times it
   on the unit before a merge into release-2. Flash about 1,200 B (the tick 200, the fifth slot and the source 350, two
   pages and seven descriptors 250, the names 60, the by-count and np rules 100, the rest inlined); RAM about 80 B of
   .bss; 14 B a track in the stores.
6. **Motion** records the seven as it records LFO 1's (motion_param). **The editor:** ED_INFO reports P_COUNT 131 and
   P_E0 123; dumps carry 131 values; the web app (Session D) reads both from INFO (model.js), and web/app/lib/layout.js
   is generated from tests/app_layout.c (gen_layout.mjs): generated again, seven values more in its tables. **Modules**
   read their eight parameters at P_E0 as compiled (tools/module/*.c), so P_E0 moving means every module is rebuilt
   with the firmware: the Nightly's set carries them; the interface identifier is made to move with it (the enum's text
   added to gen_fw_api_id's hash), so an old module is refused instead of reading the wrong parameters.

**The format consequence, which decides when.** User presets and projects map parameters by count: a record written
before carries np 124 and loads with LFO 2 at its defaults, off: **old projects and sounds keep their music exactly.** A
record written by this firmware carries np 131, and **a firmware without LFO 2 refuses it:** project.c proj_unpack
refuses a PROJ_MAGIC record whose np is not its own P_COUNT ("PROJECT DOES NOT READ"), and oc_pool.c ocp_parse refuses a
phrase record whose np is above its P_COUNT (the phrase and its notes gone from the list). So a project saved on a
Nightly with LFO 2 is unreadable on Standard 0.16.0: the one-way door PROJECT_FORMAT_R3.md section 10 is written for
(with the installer page's part in it), and its section 8.2 is where new built-in parameters are appended. The proposal:
**write np 124 while LFO 2 is unused** (the phrase's np names its motion ids: 124 unless a lane is on a new id; the
project's np names its values: 124 unless a track's seven differ from the defaults, else 131). A project that never
touched LFO 2 goes back to Standard as before; one that uses it cannot, and the installer page should say so when it is
built (Session D). **The one question for the review and the owner: ship LFO 2 on the Nightly behind OC_LFO2 with that
rule, or hold it for the Release 3 format revision.** The safer reading until the owner answers: design only, and the
misnamed page renamed now ("LFO SYNC", under OC_EDIT_SHEETS: no bytes, no format).

**Tests when built:** tests/mod_test.c (off is bit-identical to no LFO 2; each wave; SYNC at a division; TRIG; UNI; the
fifth slot alone and with a matrix slot on LFO2; the cost), a store test (an old record loads with LFO 2 off; a new one
keeps the seven; np 124 while unused and 131 once used; an old firmware's parser refusing 131 is documented, not
tested), sheet_test (the rows), ui_render (the three pages), the web tests (layout.js generated again).

**The design review of ui-fix 4** (an independent agent, October 31, 2026). The finding confirmed (one LFO a track;
"LFO 2" was Felucca 1.2's name for the second page, so "named to mislead" is softened). The design as written **cannot
be built:**

| # | Finding | Severity | What is done |
|---|---|---|---|
| 2 | Parameter ids travel as one 7-bit byte: the editor protocol, motion's native-id scheme, the user preset record; four static asserts (motion.c, upreset.c, project.c's P_COUNT == 124 "a new magic and a conversion") and proj_pack's refusal above 127. Only three ids can ever be appended; seven cannot | blocker | LFO 2 waits for the Release 3 format revision, which must widen the ids (an R3 item added to the note for PROJECT_FORMAT_R3.md 8.2) |
| 4 | An alternative with no door: five parameters on the reserved ids 33 .. 36 and 48, zeroed on load by every firmware | should fix | **not open:** 33 .. 36 are the knob macros' P_MAC0..3 (the owner's decision 6; zeroed on load only until the format revision, the bake); P_RESERVED48 alone is free, and one id carries no LFO |
| 3, 6, 7, 8 | The new firmware's own gate would refuse np 124 projects unless relaxed; templates derive their count from a fixed list and would be lost at the upgrade; user presets accept a wider record (UP_PMAX 72) and drop the extras, so the blanket "refused" was wrong for presets; the real messages are "OLD FORMAT" and "PHRASE DOES NOT READ" | should fix | the format paragraph corrected; all part of the R3 design when it is taken up |
| 10, 11, 12, 13, 14, 15, 16, 18 | The audio side: the tick before the early return and folded into the slot scan; a foreign track_t indexed by `t - trk` out of range; a one-block skew against LFO 1; the fifth slot touches eight sites and needs a DEST to MD_* map; the UNI treatment at four sites; mod_note runs before the gate scan; the synced increment's cache is voice.c's; tests/target_budget.txt bounds mod_begin's size at 158 B | should fix | kept with the design for R3 (its audio side holds with these changes; "nothing in voice.c" does not: the sync cache) |
| 19, 20, 21, 22, 23 | RAM about 142 B not 80, flash 1,400 to 1,600 B; motion's whitelist is explicit (RATE and AMT would be named); the web editor mock hard-codes P_COUNT and P_E0 (editor.html, test_web.mjs) beyond layout.js; the interface identifier must hash the enum text (track_t's layout moves anyway); ui_designer.c skips the page by its title and dims a FAM_LFO knob by LFO 1's state | minor | the numbers corrected; the rename updates ui_designer.c's skip list |

**Decision (ui-fix 4):** design only; LFO 2 rides the Release 3 format revision. Done now (OC_EDIT_SHEETS): the page
"LFO 2" is renamed "LFO SYNC" (and ui_designer.c's skip list with it), so the Mod sheet reads LFO, LFO SYNC, LFO DEST,
MOD and no one looks for a second LFO that is not there.

### ui-fix 5: the effect pages' order (done)

**The owner's order** for the FX button and the sheets' FX link: the track's own effects first, DUB TAPE at the top (the
one that is played), then DIST, AUTO PAN, then the track's sends, then the shared DELAY, REVERB, CHORUS, then SLICER,
then MASTER last. **Done** (OC_EDIT_SHEETS; params.c's PAGES table, the same entries in the new order): DUB, DUB 2, DUB
SETUP, DIST, AUTO PAN, FX (the sends), DELAY, DELAY 2, DELAY 3, REVERB, REVERB 2, CHORUS, SLICER, SLICER 2, SL PITCH 1 to
4, MASTER. Without the sheets the table is release-2's. Nothing in the firmware, the tests or tools/hw is keyed to an
effect page's index (checked); page_first(FAM_FX), the sheets' link and the FX button's landing page are DUB now.

**Mockups:** docs/orangecat/ui/new/fix5_fx_order_sheet.png (the fifteen visible pages as fifteen FX taps reach them)
and fxorder_NN_<title>.png; ui_render.c draws them in every palette (0 lint findings) and fails if a tap lands out of
order.

### ui-fix 6: the SHIFT map goes (design note, before its review)

**What goes.** The map over the page (ui_shift.c shift_draw and SHIFT_TILE, the header's SHIFT chip, ui_draw.c's
branch) and the map state (SHF_MAP). **What stays:** HOME as SHIFT from its press and every combo exactly as built: a
knob a fine turn; PRESETS the engine on HOME's pages, STRETCH on NOTES and FEEL; EDIT the init confirm on HOME's pages
and the sheets, DELETE on NOTES and FEEL; OCT- and OCT+ copy and paste where steps 10 and 11 are built in, "LATER"
otherwise; any other control its own job (and no MENU for that press).

**HOME held opens the MENU at 700 ms, held** (release-2's own path through btn_hold's BT_HOLD and page_tap's menu
toggle), not on the release: step 3 opened it on the release only so that the map could show first; with no map there
is nothing to wait for, and "HOME held opens the MENU" is the owner's words. A combo made before 700 ms cancels the
MENU for that press (as now); once the MENU is open the knobs are the MENU's (as now); let go before 700 ms with nothing
touched: a tap, HOME's pages (as now). SHIFT + EDIT's confirm coming up while HOME is held: no more combos, no MENU, no
tap (as now).

**Saved:** the tile table (144 B) and the drawing: **880 B measured** (the review's 0.7 to 1 KB). **RAM:** 0. **Tests:**
shift_test.c HOLD becomes "the MENU at 700 ms held, nothing on its release" and DRAW goes; the rest as they are
(sheet_test, step_pages_test, clip tests unchanged). ui_render's three map screens go, and the map mockups (shift_map_*,
step4_shift_map_*, step10_*, step11_*) are removed from docs/orangecat/ui/new. The switch keeps its name (OC_SHIFT_MAP:
HOME as SHIFT and its combos).

**The design review of ui-fix 6** (an independent agent, October 31, 2026): the state machine holds (a tap, a hold alone,
a combo before 700 ms, a confirm under SHIFT, a second hold with the MENU open: each traced through shift_input and
btn_hold), the MENU's knobs are untouched (ui_input returns at the MENU gate before any SHIFT knob code), the step-3 tap
race cannot recur (the MENU opens only from a press btn_hold saw held). Found, and taken into the build:

| # | Finding | Severity | What is done |
|---|---|---|---|
| 1 | BT_HOLD must pass through after the pass's combos are handled, else an EDIT or OCT edge on the hold's own pass would both open the MENU and reach its own path (hardware check 12's case) | should fix | the pass-through is the last thing shift_input does: BT_HOLD only with SHF_COMBO clear |
| 5 | tests/clip_test.c's MAP check reads SHF_MAP: the clip job would not compile | blocker | the check becomes "HOME held alone opens the MENU" |
| 6 | ui_render.c draws five map screens (the step-4 block's shift_map_feel and _notes too), not three | should fix | all five removed, the report lines and run_tests.sh's texts with them |
| 7 | SHF_FINE's value 8 is read as a literal in accel_by: renumbering the enum would break the fine turn; more removals: shift_map_on, shift_menu_open, head_chip_text's SHIFT, draw_page's branch, the comments in core.h, build.py, ui_input.c | should fix | SHF_FINE keeps 8 with a static assert (sh_st is declared before ui_shift.c is included, so accel_by keeps its literal); every site listed removed or reworded |
| 8 | shift_test's TAP and CLOSE read shift_map_on; HOLD's expectation changes | should fix | rewritten: the MENU set at 700 ms with HOME still down, nothing on the release |
| 9 | 11 map mockups, not 7 (shift_sheet.png, day_shift_map_home.png, day_step10_shift_map_home.png too); the earlier mockup lines of this file then point at removed files | should fix | twelve removed (the review's eleven and step4_shift_map_notes.png); the lines of steps 3, 4, 10 and 11 marked "removed by ui-fix 6" |
| 10 | PLAY_LIST.md and UI_DESIGN.md (decision 4, the controls table, section 1, the page table, the plan) describe the map; hardware checks 11, 12, 16 say "the map"; tests/ui_mockup.c draws the design's map | should fix | UI_DESIGN.md 0.5: decision 4 reversed by the owner on October 31 (the map withdrawn, HOME as SHIFT kept); PLAY_LIST.md is Session A's (noted "For Session A"); checks 11 and 16 reworded, check 25 added; ui_mockup.c's map is the design history, left |
| 11 | A combo begun after 700 ms is gone: today the map waited for it, now the MENU is open and a late knob's detents move the MENU on the release | should fix (wording; the owner's call) | said plainly in the note and in the decisions: the combo window is 700 ms from HOME's press |
| 2, 3, 4 | The MENU's knobs; the race; 700 ms | OK | the note's text corrected ("as release-2", not "as now") |
| 12, 13 | 0.7 to 1 KB, not "about 1 KB"; the switch's comments say "its map" | minor | measured when built; the comments reworded |

**The plain change for the owner (review 11):** with the map, a combo could begin at any time while HOME was held;
now a combo must begin within 700 ms of HOME's press (the MENU is open after that, and a knob turned then moves the
MENU when HOME is let go). "HOME plus a knob or button" read as pressed together or within 0.7 s. For the owner to
feel (check 25).

**The design as built (ui-fix 6).** ui_shift.c: the map, its state (SHF_MAP), shift_map_on, shift_menu_open, SHIFT_TILE
and shift_draw are gone; shift_input handles the pass's combos first and returns btn_hold's BT_HOLD only when no combo
was made, so page_tap opens the MENU at 700 ms with HOME still down (release-2's path); the next pass sees the MENU
(shift_clean false), marks the press as used, and the release does nothing. SHF_FINE keeps the value 8 (a static assert:
accel_by reads it as a literal). ui_draw.c's two hooks and the SHIFT chip are gone; the comments in core.h, build.py and
ui_input.c say "HOME as SHIFT and its combos". Tests: shift_test's TAP and CLOSE no longer read the map, HOLD checks no
MENU at 480 ms and the MENU at 700 ms held, the release leaving it open, and LATE (new) that a knob turned after the MENU
opened moves nothing on the page; clip_test's MAP check became MENU; ui_render's five map screens are gone (the step-3
block and the two of step 4); run_tests.sh's texts; the twelve map mockups removed from docs/orangecat/ui/new (the
earlier mockup lines of this file say so); UI_DESIGN.md 0.5 records decision 4 reversed; hardware checks 11 and 16
reworded, 25 added. PLAY_LIST.md (Session A's, BUILD 7.20) still describes the map: listed under "For Session A".

**The diff review of ui-fix 6** (an independent agent, October 31, 2026): every case traced holds (a tap, a hold, each combo
inside and on the 700 ms pass, the confirm, a second hold with the MENU open, a press over a held control, the edge-only
tap); the removed symbols are gone from the code and the ELF; the docs no longer describe the map as live; the image matches
the budget line. Found: no test pinned the pass-through's order (a mutant with it placed first passed every check): the EDGE
check added to shift_test.c passes on the tree and fails on the mutant; two `frames()` calls that were no-ops (the
millisecond helper) made `wait_frames`; a dead `w == 11u` in ui_render.c; OC_CLIP's comment in core.h still said "the map";
two cells of the table above corrected (SHF_FINE's literal, twelve mockups). Noted, not changed: a knob detent on the very
pass of 700 ms is the MENU's (a 16 ms race inside the stated window).

### ui-fix 7: one authority for every note (design note, before its review)

**The owner's requirement** (SPEC.md 5 and 5.2, his first): tracker, step view, piano roll, pads and live recording
edit the same notes, and every note has its own pitch, velocity, start and length however it got in. **Today:** a
manual step's notes share one length and have no start of their own; NOTES' START says "RECORDED ONLY" and the strip
grays it (LEN says "ALL" on a manual chord); the pads' editor (DRUM HIT: drum_hit_edit, drum_hit_delete) is a third
copy of the event editor and patches a mixed cell (manual hits beside one recorded hit); `recording_prune` still exists
for events a manual gesture might leave orphaned (TRACKER_REVIEW.md "Left to do"); a key on a recorded step is refused
by the dispatcher. This is step 12, deferred on October 30.

**Design: the recorded note is the one form of a note that has its own start or length, and a step converts whole.** A
manual step stays the compact form of "notes that share their length and start on the grid": the sequencer, the
phrase record and the tracker read it as before; nothing migrates; old projects keep their music exactly.
1. **START on a manual note converts the step and moves the note,** in one note transaction (note_convert: every note
   of the step becomes an event at the step's length and velocity, as LEN's note_manual_length already does; then
   note_onset): one undo. From then on every note of every step has the four knobs; the strip never grays START or LEN;
   "START: RECORDED ONLY" goes. PITCH and VEL keep editing a manual note in place (nothing needs an event). The tracker's
   KNOB 4 (4c) already converts a chord.
2. **The pads' editor comes under the shared one.** drum_hit_edit's pitch and length become notes_event_edit's slots on
   the hit's event (1: a drum track's pitch offset; 4: a sixteenth of a step); a hit with no event (a manual or grid hit)
   converts its cell first (note_convert: every hit of the cell becomes an event, so no cell is ever half manual and
   half recorded, and the patching of a mixed cell goes); drum_hit_delete becomes notes_delete_edit on the hit's event
   (notes_rebuild already rebuilds a drum cell's lanes); a pad struck (drum_hit_key) adds or clears a hit as the grid does
   on a manual cell and as an event on a recorded one. DRUM is unhooked in the shipping build (MELODEE_DRUM=0), so this is
   measured on the host and costs the image nothing; it is the shape every pad page must have.
3. **Every manual gesture on a recorded cell deletes its events through the history first** (events_delete_view,
   0.7.2). The sites that still clear SF_RECORDED by hand: seq_edit.c step_note_move (a copy of the cells moved: the
   source cells are cleared, so the flag on the copy is the bug the 0.7.2 sites fixed elsewhere: it goes through
   events_delete_view of the source cell first), seq_edit.c step_note_resize (a recorded step resized by hand: the same),
   ui.c grid_hit (done in 0.7.2), ui_input.c drum_hit_delete (goes with 2). Then **`recording_prune` goes** (its callers:
   note_transaction_begin_track, the capture in ui_input.c, recording_note's non-FAST path, all no-ops once no orphan
   can exist) and a host check asserts the invariant after every edit path: every event of the phrase's owner has a
   NOTE cell flagged recorded at its view, and every cell flagged recorded has at least one event.
4. **A key on a recorded step** replaces the step as it does a manual one (the entry paths tracker_entry_notes and
   seq_entry_notes already delete the events through the history, 0.7.2): the dispatcher's refusal goes; EDIT held brings
   the recording back, an event at a time.

**Cost:** about 400 B (START's conversion 150, the key dispatcher 50, the resize and move sites 100, prune's removal
-100; the pads 0 in the image). RAM 0. **Tests:** step_pages_test (START on a manual note and on a chord: converts, moves,
one undo; the strip), recording_props_test and the scheduler tests unchanged, a new tests/one_auth_test.c (the invariant
after every path: keys on manual and recorded steps, tracker entry, grid hits, a step delete, START, LEN, the tracker's
KNOB 4, a move, a resize, a capture, copy and paste, undo and redo; the pads' editor on DRUM in the host build), ui_test
unchanged (its tracker and NOTES checks), ui_render (NOTES' strip on a manual note).

**The design review of ui-fix 7** (an independent agent, October 31, 2026): the direction is right and the code
supports it (note_convert does the conversion, events_delete_view is the one delete path, the flag is consistently on
the view cell, the sites named are real, the drum round trip through note_convert and notes_rebuild loses only a hit's
natural length, nothing in a format changes). Not ready as written; found, and taken into the design:

| # | Finding | Severity | What is done |
|---|---|---|---|
| 16 | Two orphan makers missed: EDIT tapped on the drum grid (page_tap's step_clear, no events_delete_view) and the web editor's ED_STEP_SET (ed_step_put rewrites a cell's flags without deleting its events) | should fix | both go through events_delete_view (the editor has no step history: its deletes are plain recording_remove per event, said) |
| 18 | The remedy for the tracker's SCL + SELECT move and ENV + SELECT resize of a recorded step (events_delete_view first) would flatten a recording silently: SPEC.md 6 forbids it and the note's own principle contradicts it | should fix | on a recorded step those gestures go to the events: a one-step range and note_move_snapped (one transaction, one undo, "STEP OCCUPIED"); the resize sets every event's length (slot 5); step_note_move and step_note_resize then never see a flagged cell (their two clearing lines become asserts) |
| 19 | Stored phrases and projects from older builds can carry orphans (every event with vel is saved, present or not); without recording_prune they would live forever and the invariant fails right after a load | should fix | one reconcile at the load boundaries (pool load, project load, boot): an event whose view cell is not a flagged NOTE is dropped, a flagged cell with no event unflagged; the one place the lazy prune survives, tested with a fixture that has an orphan |
| 3 | recording_prune's callers listed incompletely: the FAST capture path retires inline, and tests/capture_alloc_test.c keeps a verbatim copy of the non-FAST allocator calling it by name | should fix | the FAST path's inline retire stays (the live recording's safety net, said); capture_alloc_test gets a local copy of the reference prune |
| 5 | START on a manual note while the song plays, a chain runs or a recording is armed has no stated behavior (LEN refuses then: the transaction's undo would not survive) | should fix | "STOP TO EDIT" then, as LEN; on a recorded note today's behavior |
| 6 | After note_convert the pick is the chord's first event, not the manual note the user had | should fix | the pitch is read before the conversion, the event found by step and pitch after it, moved and picked, as tracker_chord_length does; tested with a chord entered out of order |
| 7 | Several detents in one call can move the note into a cell that holds a separate manual note: the conversion must roll back | should fix | notes_manual_occupied on the new cell inside the transaction; on a hit the snapshot is restored, nothing converted, "STEP OCCUPIED" |
| 8, 9 | The ties become the event's duration and the TIE cells go (a visible change on the tracker's rows); the first START detent clears the step page's earlier undo levels (as LEN) and a note moved out of its cell loses the step's feel (S4) | OK, minor | said in point 1 |
| 12, 13 | The pads: the first edit on a manual cell becomes a transaction that refuses while playing and clears the step history; drum_editor_test pins today's mixed cell; a hit's natural length (length 0) is not reachable through notes_event_edit | should fix, minor | point 2 built in its own later step with its own note (the natural length kept reachable: a duration of 0 on slot 4); drum_editor_test changes with it; host only, nobody waits on it |
| 15 | Live recording leaves a manual cell's notes beside the events (rec_note; the manual notes keep playing where no event has their pitch): no cell is "never half manual and half recorded" | should fix | the exception written into points 2 and 3; the invariant is stated on the flags, never as "overview equals rebuild" |
| 20 | The invariant must be stated with recording_view and recording_present, per owner bank, dormant events beyond LEN included | OK | written so |
| 4, 22 | The key refusal has two conditions (the cell flagged; an event picked by its actual step, which a neighbor's rounded event can be); after the refusal goes the entry must clear the pick | OK, minor | both conditions named; ui.note_pick cleared in the two entry paths' first-entry block; the rounded case tested |
| 23 | Undo of a key on a recorded cell is bounded by the eight history levels: a cell can hold more events than four | should fix | a key on a cell with more events than the free levels goes through a note transaction (one undo of the whole overwrite); a 9-event cell tested |
| 24 | ui_test's test_overwrite_recorded and step_pages_test's "START: RECORDED ONLY" pin today's | should fix | both listed as tests that change (the overwrite test becomes the key path's positive test) |
| 26 | 400 B is low: expect 500 to 700 B net | minor | the table carries the measured number |
| 27 | Tests to add: ED_STEP_SET on a recorded cell, the grid's EDIT, a load with an orphan and with a flagged empty cell, live recording over a manual and over a recorded MONO cell, LEN below a recorded cell, START while playing, pool full on START, a 9-event cell under a key, the rounded-forward pick, capture_alloc_test building, frozen phrases | should fix | in the plan |
| 2, 10, 11, 14, 17, 21, 25 | The pads are a user of the shared primitive that patches by hand, not a third copy; pool full and frozen handled; notes_rebuild rebuilds a drum cell; drum_editor_test exercises the pads on the host; the move and resize reach a recorded step from the TRACKER only; the other sites are orphan free; no format change | OK | the note's wording corrected |

**Build order (the review's), taken:** (1) the host invariant check with the load reconcile first, on the full suite,
which finds any site still missed and says whether recording_prune is a no-op today; (2) the sites of point 3, with the
tracker's two gestures on a recorded step routed to the events; (3) point 4 with its two conditions, the pick cleared
and the depth bound; (4) point 1 (START converts, with the pitch kept, the rollback and the guards); (5)
recording_prune's removal last; (6) the pads in a step of their own.

**The design as built (ui-fix 7, in the review's order; behind OC_ONE_AUTH, on in tools/orangecat_engines.sh).**
1. **The invariant and the reconcile** (recording.c `recording_reconcile_owner`, `recording_reconcile`): every event with a
   velocity has a NOTE cell flagged SF_RECORDED at its view (recording_present), and every flagged NOTE cell has an event
   viewing it, per owner bank. At the load boundaries (the pool's load of a phrase, a project load and so boot) an orphan
   event is dropped and a flagged cell with no event becomes the manual cell its notes make, or an empty one; the current
   bank through the track's steps, another bank read into a pattern_t, mended and written back only when it needs it;
   under the caller's guard (the guards save and restore the interrupt state, so they nest). tests/one_auth_test.c asserts
   the invariant after every edit path and that recording_prune then finds nothing: every path of today kept it already.
2. **The sites.** The tracker's SCL + SELECT on a recorded step moves every event of the cell exactly a step (its onset
   inside the step kept; note_transform would snap the first onset to the grid), in one transaction, "STEP OCCUPIED" when a
   destination holds a separate manual note, the cursor following (ui_one_auth.c `tracker_recorded_move`); ENV + SELECT
   there says "K4 SETS A NOTE'S LENGTH". step_note_move and step_note_resize never see a flagged cell now. The drum grid's
   EDIT (page_tap) and the web editor's ED_STEP_SET (editor.c, `events_drop_view`: no history, as the editor has none)
   delete the cell's events first.
3. **A key on a recorded step** replaces it (the dispatcher's two conditions gone; the entry paths delete the events
   through the history and clear the pick, so a neighbor's event rounded into the step is not the entry's). The bound
   stays as the review found it: the history holds eight levels, one event a level, so a cell with more than seven events
   is not brought back whole by EDIT held (written here, not routed through a transaction: an entry runs over frames and
   its own step writes would invalidate the transaction's undo).
4. **START on a manual note** (`start_convert_edit`): the step converts whole (note_convert: the ties become the note's
   duration and their cells go, as LEN; the step's feel stays on the cell), the pitch read before and the note found by it
   after, moved and picked, so the next detent goes on with it; "STOP TO EDIT" while the song plays, a chain runs or a
   recording is armed; "STEP OCCUPIED" rolls the conversion back; a full pool leaves the step manual ("RECORDING FULL");
   one undo returns the manual step (a later event edit takes the conversion's undo with it, as LEN's: step 4's B1). The
   strip shows a manual note's START as its step, lit; "START: RECORDED ONLY" is gone.
5. **recording_prune** has no caller left under the switch (note_transaction_begin_track's and the capture's go; the
   FAST capture path keeps its inline retire, the live recording's own safety net); the function stays for the host
   check and the non-FAST path.
6. **The pads** (DRUM HIT, host only: MELODEE_DRUM is 0 in the shipping image): a hit's pitch and length are
   notes_event_edit's slots 1 and 4 on the hit's event; a hit with no event converts its whole cell first (one
   transaction, refused while the song plays), so no cell is half manual and half recorded by an edit; a sound the cell
   lacks gets a new event; a hit's natural length (length 0) is reached by turning its length below one sixteenth and
   left by turning up; the delete is the shared one (the cell rebuilt from what is left); a pad makes a hit, the accent
   pad sets 127 through the velocity slot. EDIT held on DRUM HIT is the sound's undo, as it was; the pads' undo is the
   step history (drum_editor_test drives it so). drum_editor_test keeps today's behavior for the switch-off build.
   Live recording still leaves a manual cell's notes beside the events it adds (rec_note), as the review found: said.

**Tests:** tests/one_auth_test.c (43 checks, ASan and UBSan in the suite's job, the alignment check off as the suite's rule): RECONCILE (an orphan event, a flagged empty
cell, a flagged manual cell, another bank, a project saved with an orphan and loaded), PATHS (fourteen edit paths with the
invariant and prune's no-op after each), START (the conversion and its undo, the next detent, a chord entered 67 60 64,
STEP OCCUPIED, STOP TO EDIT, the pool full), TRACKER (the move, its undo, the resize refused), KEY (NOTES and the TRACKER,
the undo, the pick), EDITOR, PADS (the conversion, the length and the natural length, the delete and its undo, a pad and
the accent pad), REVIEW (the diff review's cases: the ordered list's move, the TIE cell's two gestures, a load with a flagged
empty cell in another bank, LEN below a recorded cell, a key beside a rounded-forward event, DRUM-X's LEN). step_pages_test's
"START: RECORDED ONLY" check is the switch-off build's (#if !OC_ONE_AUTH; with the switch it checks the conversion);
sheet_test, shift_test, step_pages_test, lanes_test, clip_test and clip_sel_test pass with the switch on; ui_test and
drum_editor_test are the switch-off build's. ui_render.c (ui_renders_steps, now with OC_ONE_AUTH=1): NOTES' strip with
a manual note's START lit, linted.

**The diff review of ui-fix 7** (an independent agent, October 31, 2026; it built the test with the sanitizers and wrote probes
for its findings). Found correct: the reconcile (the walk, the guards' nesting, the load sites, the dormant events kept),
START's conversion (every path compared with tracker_chord_length and note_transform), the key dispatcher, the grid's EDIT,
the pads, the editor's cell, the switch-off build, the CI switches. Found and fixed:

| # | Finding | Severity | Fix |
|---|---|---|---|
| 1 | The tracker's move walked the owner's list while it moved the event it stood on: once the scheduler had ordered the list (after any play) the moved event was re-linked past the cell's others and the cell split in two | bug | the cell's events are collected first, then moved; tested with the list ordered (recording_sync) |
| 2 | ENV + SELECT's refusal read the cursor cell's flag: on a recorded note's TIE cell (a live take leaves them) it fell through to step_note_resize and flattened the recording | bug | the flag is tested at step_note_start, as the move does; tested |
| 3 | SCL + SELECT on such a TIE cell began a transaction on the cursor cell, rebuilt it (step_delete of the recorded start), cleared the step history and said SELECT A NOTE | bug | the start cell is passed in; with nothing viewing it the gesture is refused before any transaction; tested |
| 4 | The review's depth bound (a key on a cell with more events than the free history levels through a transaction) is not built | should fix | deferred, said in point 3 above: an entry runs over frames, so a transaction around it would be invalidated by its own step writes; the bound (seven events) stands |
| 5 | The pads' natural-length rule was keyed on drum_track, so NOTES' and the tracker's LEN on a recorded DRUM-X hit changed in the shipping image; natural to natural recorded a no-op entry | should fix | the rule is the pads' page's only (MELODEE_DRUM and SC_DRUMHIT); natural to natural is nothing; DRUM-X's LEN tested unchanged |
| 7 | drum_hit_edit set the pick without the track and bank notes_selected keeps it by: after a track change the edit could land on another event | should fix | pick_event sets all of it (the pads and START) |
| 8 | events_drop_view used recording_active, which is LEN-bounded; the editor addresses any cell | minor | matched on the owner and the view, no LEN bound |
| 9 | The reconcile cleared a flagged empty cell but not its TIE tail; the track pointer was formed before the range check | minor | recording_reconcile_cell clears the tail in both bank paths; the check first; tested with a tie in another bank |
| 10 | The cursor followed the last event walked | minor | the earliest moved note's cell |
| 11 | A move into a recorded cell merges the events into it (as note_transform); unsaid; a redundant term | minor | said; the term dropped |
| 12 | recording_prune's non-FAST caller unguarded; the comment | minor | guarded; the comment |
| 13 | step_pages_test's guard wrapped more than START's check | minor | the one check, with its OC_ONE_AUTH counterpart |
| 14 | Test gaps | should fix | added: the ordered move, the two TIE-cell gestures, a load with a flagged empty cell and its tie in another bank, LEN below a recorded cell, a key beside a rounded-forward event, DRUM-X's LEN; still open: ED_STEP_SET through the editor command (the helper is tested), live recording over a manual and a recorded MONO cell, a 9-event cell under a key (the bound is stated), frozen phrases |
| 6 | 2,144 B against 500 to 700 | should fix | measured and carried in the budget table above, with where it went |

**Session A, round 11 (October 31, 2026; BUILD.md 7.22).** The merge's independent review found the reconcile that `project_restore_runtime`
ran right after `pattern_init()`: every other bank was empty at that moment, so a project load, a boot through the track's removal and the
editor's restore deleted the other banks' recorded notes and flattened their cells to manual notes. Fixed on r11 (b1997d1): no reconcile inside
`project_restore_runtime`; one after `bank_restore` on every path (project_load, tracks.c, editor_backup.c); one_auth_test's LOAD check plants a
consistent note in another bank and sees it survive a save and a load. Also from that review, fixed: the slide's louder-note rule in POLY (nv is
NVOICE there), a 1-step loop taking over its own recorded chord, the list walk in recording_slide_in (the queue's per-view count returns at
once), the tracker's move of more than 64 events (refused whole). Stated, not changed: a chord whose notes swap across a slide neither swaps nor
glides; the pairing reads the step before by index (a TIE chain or a bank boundary gives no takeover); m_vi is not moved on a POLY takeover.
TRACK_STEP on a recorded cell now drops the cell's events as STEP_SET does (the guard was on one of the two).

### ui-fix 8: slide and legato (design note, before its review)

**What SLIDE does today** (seq.c seq_step, voice.c): a step's SLIDE flag makes the next step's notes start before this
step's are released (`slide_in`) and sets `slide_glide` for the voice code. What follows depends on the track's VOICE
mode, not on the engine. POLY (every preset's default): a new voice with a new envelope starts and the old note is
released after it; no glide unless GLIDE is set, and then a fresh voice glides from a released voice's last pitch. MONO:
the voice glides to the new pitch but its envelope retriggers (mono_play always retriggers in MONO). LEGATO and UNISON:
the voice glides and the envelope goes on: the 303. Recorded notes (recording.c) never read SLIDE. So SLIDE is a true
slide only on a LEGATO track, only for manual steps, on MACRO, SHAPES and every module alike (DRUM-X: oneshot, no glide,
by design). The engines need nothing: MACRO and SHAPES read the voice's pitch every block (pitch16, glided in
pitch_cur), and the `legato` hook exists for an engine that caches its pitch.

**Design: a slide is a legato glide in every voice mode, for manual and recorded notes, behind OC_SLIDE.**
1. **Manual steps** (seq.c): when step s+1 fires with slide_in, each of its notes takes over a sounding voice of step s
   instead of starting one: the j-th new note takes the j-th old note's voice (a 303 line is one note; a chord slides
   voice by voice; new notes beyond the old ones start, old notes beyond the new ones release). `seq_slide_voice(t, old,
   new)`: the voice's note becomes the new one, its pitch target the new pitch (voice_scale_pitch), gliding from where
   it is (pitch_cur) at GLIDE's time, or the render's 45 ms default when GLIDE is 0 (GLMODE TIME with GLIDE 0 would jump,
   so a slide uses the RATE path then); the envelope is untouched; the engine's legato hook runs if it has one. In MONO,
   LEGATO and UNISON the same takes over v[0] (UNISON: its voices) and the mono note and stack follow, so a later
   note-off of the new note ends it and one of the old note finds nothing. The velocity stays the sounding note's (the
   303: the accent is the first note's). seq.c only: voice.c is not touched (voice_scale_pitch and the hook are the one
   translation unit's).
2. **Recorded notes** (recording.c): a recorded step with SLIDE holds its notes to the next step's first onset
   (recording_shape: the gate reaches that onset when the event's own gate would end before it), and the next cell's
   first hit takes over the sounding voice of the slid note (seq_slide_voice again; the old event's hold ends without a
   note-off: its reference count is given up and recording_end's note-off skipped), and the new event owns the voice
   from then on (its own gate, queue and reference count as any hit). A ratchet's or a flam's later hits never slide; the
   grace and the first part slide in, as a manual step's do.
3. **Setting it, obvious on every page:** NOTES (melodic): ENV held + KNOB 4 sets or clears SLIDE on the cursor's step,
   manual or recorded (today a manual step's only), the roll draws the slide line from a bar to the next step's bar for
   recorded notes as it does for manual ones, and the strip marks VEL's column "SL" while the step slides; FEEL's TYPE
   (SLD, A+S) as built; the tracker's FX column SLIDE as built, on recorded steps too; the note sheet's Slide row (OCT+)
   for both. Undo through the step history (the steps' flags are in every snapshot).
4. **Cost:** about 500 B (seq.c 200, recording.c 200, the pages 100). RAM 0. Old projects keep their notes; a project
   with SLIDE on a POLY track now glides where it overlapped before: the owner's intent.

**Tests:** a new tests/slide_test.c over the sequencer (as recording_props_test plays steps a sample at a time): POLY,
MONO, LEGATO and UNISON: a slide step glides the voice (pitch_cur moves over the glide time, the envelope's stage and
level go on, no new voice starts), a chord slides voice by voice, a slide into a rest releases, GLIDE 0 uses the 45 ms
default, GLMODE TIME uses GLIDE's time, a ratchet or a flam does not slide; the recorded case: the hold to the next
onset and the takeover, the reference counts after it (no stuck note, no early release); step_pages_test: ENV + KNOB 4
on a recorded step; ui_render: the roll with a recorded slide and the strip's SL.

**The design review of ui-fix 8** (an independent agent, October 31, 2026): the account of today's behavior confirmed in
every voice mode (POLY a fresh voice and attack, a same-pitch slide restarting the attack; MONO retriggering; LEGATO and
UNISON legato; recorded notes blind to SLIDE); seq_step runs inside the audio render already, and voice.c's helpers are
the one translation unit's, so the takeover from seq.c is sound. Found, and taken into the design:

| # | Finding | Severity | What is done |
|---|---|---|---|
| 2 | The "45 ms" default glide is a comment the table contradicts: ENV_LIN[40] >> 14 is 40 sixteenths of a semitone a control tick, an octave in about 3.5 ms. Today's LEGATO slide with GLIDE 0 is nearly a jump | should fix | a slide gets its own rate: 3 sixteenths a tick (an octave in about 46 ms at 44.1 kHz; gstep set by seq_slide_voice from the interval), GLIDE's own rate or time when GLIDE is set; the test counts ticks, not milliseconds; voice.c's comment is Session A's (noted "For Session A") |
| 6 | The state a takeover must keep: v->note for the later note-off; in the mono modes mono_remove(old), mono_push(new), mono_note = new, PRIO honored (mono_pick may say the old note wins: then no takeover); UNISON re-pitches each gated voice of v[0..nv) and keeps its detune; the matrix's note values (mvel, mrnd, mod_note for KEY) as mono_play's legato branch does | should fix | written into point 1; mod_note(t, new, vel) is called so KEY follows |
| 7 | An old note that is a live key's, the arp's or a recorded note's is not the sequencer's to take (seq_release's test) | should fix | only an old note that passes seq_release's test is taken over; otherwise today's path |
| 8 | The pairing must run over the step's notes only (nn[] carries the lane hits after them); oneshot engines never glide | should fix | pairs over min(s->n, the old notes); the takeover is skipped on an oneshot engine |
| 9 | MACRO, SHAPES and the Prophet need no engine code; FM6's legato expects its mono bookkeeping (mono_key first) and in POLY it allocates voices itself | should fix | the mono modes call mono_key then legato as voice.c does; an engine with its own alloc (FM6) keeps today's path in POLY; eng_sid and eng_noise's v->note uses checked when built |
| 11 | "The velocity stays": LEGATO raises v->vel when the new note is louder, and an accent on the slid-into step would be lost | minor | mono_play's rule kept: raised if louder, else the sounding note's |
| 12, 13, 14 | Point 2 as written is a blocker: the next onset is not known in recording_shape (the queue's deadlines, the scanner's unsorted list), the takeover cannot identify the event and voice at the next hit, and it touches the queue, the countdown, the repeats, the roll and the wrap. A simpler rule serves a played take: slide only when the next cell's first onset falls within the sliding note's own gate (the notes already overlap: no extension, no new deadline) | blocker, resolved | point 2 is the overlap rule: at a cell's first hit, if the cell before has SLIDE and a recorded note of it is still sounding (recording_refs), that voice is re-pitched and the reference moved (a recording_end variant without the note-off); no gate extension; the repeat path never takes over |
| 15 | Manual to recorded and recorded to manual steps | minor | no slide across the boundary, said |
| 16 | The UI routing holds; the recorded path of step_edit needs the ENV + KNOB 4 branch; the roll never draws the line from a recorded bar | OK | the branch and the line added as the note says |
| 10, 17, 18, 19 | Microtonal (a sub-sixteenth step at the slide's start, as today's legato), the flag's survival, the undo, the formats and factory content | OK | microtonal accepted and noted |
| 20 | 500 B is optimistic: 300 to 450 for point 1, 700 to 1,000 for both | minor | the table carries the measured number |
| 21 | Tests to add: the same-pitch slide, a live-held or arp old note, FM6 and the Prophet, a step with notes and hits, PRIO LOW and HIGH, the queue and countdown variants, the repeat path | should fix | in the test plan |

**Build order (the review's):** the manual-step slide alone first, then the UI, then the recorded overlap rule.

**The design as built (ui-fix 8, in the review's order; behind OC_SLIDE, on in tools/orangecat_engines.sh).**
1. **Manual steps** (seq.c `seq_slide_voice`, called from seq_step): when a step with SLIDE goes into the next, the j-th new note
   takes over the j-th old note's sounding voice (the step's notes only, never its hits): the voice's note and pitch target
   become the new note's, its pitch glides from where it is, its envelope goes on; the matrix's note values follow (mod_note);
   in MONO, LEGATO and UNISON the stack and the mono note follow too (mono_key then legato, as voice.c does), so the new
   note's later note-off ends the voice and the old note's finds nothing; a louder new note raises the velocity (mono_play's
   rule), a softer one leaves it. Not taken: an old note that is a live key's, the arp's or a recorded note's (seq_release's
   test), an oneshot engine (DRUM-X), an engine with its own allocator in POLY (FM6), a mono voice that is not the old
   note's, PRIO LOW or HIGH with more than one note held. **The slide's rate**: three sixteenths of a semitone a control
   tick, an octave in about 64 ticks (46 ms at 44.1 kHz); with GLIDE set, GLIDE's own rate or time. (The "45 ms" default of
   voice.c's comment is ENV_LIN[40] >> 14 = 40 sixteenths a tick, nearly a jump: Session A's comment to correct.) A slide into
   the same pitch goes on without a retrigger (today a restart); a slide into a rest releases; a slide into a ratcheted
   step glides into its first part and the repeats retrigger (as before).
2. **Recorded notes** (recording.c `recording_slide_in`, the overlap rule): at a cell's first hit, when the cell before has
   SLIDE and one of its recorded notes is still sounding on its own voice, that voice is re-pitched (seq_slide_voice) and the
   reference moves: the old event's hold ends without a note-off, this hit's begins as any hit's. No gate is extended: the
   notes must overlap (a played legato take does); a ratchet's or a flam's later hits never take over. No slide across a
   manual and a recorded step (either way).
3. **Setting it**: NOTES: ENV held + KNOB 4 sets or clears SLIDE on the cursor's step, a recorded step's too (step_edit's
   recorded path), undone by EDIT held through the step history; the strip shows SLIDE with ENV held on either kind of
   step and marks VEL's column "SL" while the step slides; the roll draws the slide line from a recorded step's lowest note
   to the next step's first note as it does for a manual step; FEEL's TYPE, the tracker's FX column and the note sheet's
   Slide as built.

**Tests:** tests/slide_test.c (16 checks, ASan and UBSan in the suite's job, the alignment check off; the sequencer and the voices driven by mix_block,
the interrupt's own path): MODES (POLY, MONO, LEGATO, UNISON: the same voice, the stage and level kept, the octave in 64
blocks, the release at the rest after), CHORD, REST, SAME, GLIDE (GLMODE TIME with GLIDE set), HELD (a live key's old note
not taken), RATCHET, DRUMX, RECORDED (the overlap, the no-overlap and a chord into a chord), HITS (a step with a lane hit:
no pairing), SET (ENV + KNOB 4 on a recorded step, its undo).
step_pages_test and one_auth_test pass with the switch on. ui_render.c (ui_renders_steps, now with OC_SLIDE=1): NOTES with a
recorded step sliding into the next, ENV held (notes_slide), linted.

**Mockups:** docs/orangecat/ui/new/fix8_notes_slide.png (the roll's slide line from a recorded bar, the strip's SLIDE).

**The diff review of ui-fix 8** (an independent agent, October 31, 2026; it ran the test plain, under ASan and under UBSan).
Found correct: the takeover in POLY, MONO and LEGATO on MACRO, SHAPES and the modules; the rate (CTL is 32 samples, gstep 3
is an octave in 64 blocks, 46.4 ms at 44.1 kHz; ENV_LIN[40] >> 14 is 40: "nearly a jump" confirmed); GLMODE TIME never
0 or overshooting; mod_note's order; the ratchet, flam, chance and TIE paths; the recorded hold's end under the queue; the
UI routing and the undo level (the SET check does prove one); the switch-off build; the arena (below). Found and fixed:

| # | Finding | Severity | Fix |
|---|---|---|---|
| 1 | UNISON's voices start at a scaled level; "louder raises" compared the step's velocity with it and raised every voice two to three times | bug | mono_play's rule in full: raised only with one voice; the UNISON check asserts it |
| 2 | The legato hook ran before mono_key, the reverse of voice.c's order: FM6's legato would take nothing | should fix | the stack and mono_key first, then the re-pitch, then the mono note |
| 3 | SID's C64 chord in POLY: the slid voice was released at once by the chord stack | should fix | no takeover with trk_chord, as with an engine's own allocator |
| 4 | The suite's sanitized job aborts on a pre-existing host arena alignment report (mc_part_t) before its first check; the one_auth job renders voices too | should fix | both jobs built with -fno-sanitize=alignment, as the suite's own SAN=1 rule; run here under the exact flags: both pass |
| 6 | The arena fell 1,024 B: not a static; seq_step, seq_slide_voice, recording_fire and recording_hit are audio-resident code (.audio_text), placed before the arena, so resident sequencer code grows out of it | OK | said in the budget note |
| 7 | recording_slide_in runs at each hit of a cell, not "the first": a recorded chord slides voice by voice | minor | the comment and the note say so; a chord into a chord tested |
| 8 | The old pitch under the queue is the message's (the pitch the hit was played with), as recording_end reads it | minor | so read |
| 9 | A manual step's note of the same pitch shares the voice: not ours to take | minor | seq_notes checked, as recording_off does |
| 10 | The pairing could reach a lane hit's voice on a step that carries hits | minor | bounded by the step before's note count, none when it has hits; tested |
| 12 | The slid POLY voice kept its old age (the first to be stolen) | minor | its age is the newest |
| 14 | The design note said a ratchet or a flam never slides; the grace and the first part do slide in (manual and recorded alike) | minor | the wording here |
| 17 | The strip's SLIDE showed on an empty step | minor | with a note selected only |
| 18, 19 | A comment; test gaps (PRIO, the Prophet and FM6 through their hooks, the countdown variant, the repeat path, the wrap from the last step) | minor | the comment; the gaps are listed |

### Budget revision 0.9 (ui-fix; CI's build options, one niced build each)

| Build | Image (B) | Free of 577,248 | Above the 8,192 B reserve | .data+.bss | Data SRAM tail | Arena (min 49,152) | Pack |
|---|---|---|---|---|---|---|---|
| linux-hw c27dc5c (the baseline) | 558,596 | 18,652 | 10,460 | 94,536 | 3,768 | 84,032 | 40,845 |
| + ui-fix 1, 2, 3, 5 | 558,980 (+384) | 18,268 | 10,076 | 94,536 | 3,768 | 84,032 | 40,853 (+8: the scene's bindings; 107 B of the area left) |
| + ui-fix 6 (the map gone) | 558,100 (-880) | 19,148 | 10,956 | 94,536 | 3,768 | 84,032 | 40,853 |
| + ui-fix 7 (one authority), at its commit 5a26150 | 560,020 (+1,920) | 17,228 | 9,036 | 94,536 | 3,768 | 84,032 | 40,853 |
| + ui-fix 8 (slide and legato), after its diff review | 561,608 (+1,588) | 15,640 | 7,448 | 94,536 | 3,768 | 82,848 | 40,853 |

Ui-fix 7 cost 1,920 B against the review's 500 to 700 (2,144 before its diff review's fixes): the reconcile is 472 B (its two
bank paths), and START's conversion and the tracker's move, inlined into edit_param and step_move_edit, about 1,000 B between
them (each carries a transaction, a rollback and a rebuild; a shared "the cell's events, shifted" helper could save about 300 B,
left for later); the pads are dead code in the shipping image. Ui-fix 8 cost 1,588 B against 700 to 1,000 (seq_slide_voice
and the seq_step block, the recorded overlap rule, the strip and the roll; 1,452 before its diff review's fixes). **The audio working arena fell by 1,184 B with
ui-fix 8** (84,032 to 82,848; .data+.bss unchanged): seq_step, seq_slide_voice, recording_fire and recording_hit are
audio-resident code (.audio_text, placed in the pool before the arena), so about 1 KB of the item is SRAM as well as flash;
33.7 KB above the minimum and 17.3 KB above the rule's 65,536. **The reserve holds: 15,640 B free, 7,448 B above 8,192.** Nothing further is built on
this branch without the owner's say: the next item of any size eats into what is left above the reserve.

**The full host suite** (tools/hw/ci_step.py test, CI's settings, PARALLEL=1, niced): at 3f28cb7 (ui-fix 1, 2, 3, 5)
PASSED 214 jobs in 83 min; at c246106 (ui-fix 6) one job failed (shift_test under the sanitizers: the sound line's
buffer, the decisions); at 3cb55f1 (the head: ui-fix 6, 7, 8 and the fix) PASSED 214 jobs in 67 min, ALL HOST TESTS.

### For Session A (ui-fix)

**Run the on-unit sequencer test on ui-fix before merge.** The sequencer items are ui-fix 7 (commit 5a26150, one
authority: OC_ONE_AUTH) and ui-fix 8 (commit 9d45d84, slide and legato: OC_SLIDE), both on in
tools/orangecat_engines.sh, so a CI build of ui-fix carries them. What the unit test should cover beyond the host
(hardware checks 26 and 27 below): a live take over a manual step and over a recorded MONO step, then every edit of it;
a 303 line on MACRO and SHAPES with SLIDE on every other step in POLY, MONO and LEGATO; a recorded legato take with SLIDE
set on its steps; the audio interrupt's time with four tracks sliding (seq_slide_voice runs inside seq_step; the
reconcile runs only at a load). Also for Session A: voice.c's comment "slide: ~45 ms" (track_render_audio's glide) is
the table's 40 sixteenths a tick, nearly a jump: the slide now uses its own rate in seq.c; the comment is Session A's. Also: docs/orangecat/PLAY_LIST.md (BUILD 7.20) still tells the owner to hold HOME for the
map of shortcuts; with ui-fix 6 it is "hold HOME 0.7 s for the MENU; HOME plus a knob or button within 0.7 s is its
combo" (Session A's file).

### Hardware checks (ui-fix; the unit is Session A's, none run)

22. **FX by hand:** from HOME, FX lands on DUB; FX again walks DUB 2, DUB SETUP, DIST, AUTO PAN, FX, DELAY .. MASTER and
    comes round; EDIT > FX still lands on DUB and EDIT goes on to Voice.
23. **CHAIN by hand:** PRESETS walks the selected track's chain 01 .. 64 stopped (the chip, the message, the steps shown);
    refused while the chains or the song play; the project saved and loaded keeps the chain.
24. **SHAPES on the panel:** the yarn, tail, moon and paw at each knob's ends; the tail's flick speeds up with COLR, the
    yarn's roll with TIMB; no late audio halves while it moves (as check 4).
25. **HOME held (ui-fix 6):** held alone 0.7 s opens the MENU with HOME still down, nothing on the release; a knob, PRESETS,
    EDIT or OCT within 0.7 s of the press is its combo and no MENU follows; a knob turned after the MENU opened moves the
    MENU when HOME is let go (the combo window is 0.7 s from the press: the owner to say whether that is right).
26. **One authority (ui-fix 7):** on NOTES, START on a typed note (the step converts, the note moves, EDIT held brings
    the manual step back); a key on a recorded step (replaced; EDIT held brings the events back one by one); the tracker's
    SCL + SELECT on a recorded step (the notes move a step, their timing kept); an old project with recordings loads as it
    did (nothing mended on a project saved by 0.16.0 unless it carried an orphan: the message log says nothing either way).
27. **Slide (ui-fix 8):** a bass line with SLIDE on every other step on MACRO, SHAPES, a module (SEM, HADES) in POLY, MONO
    and LEGATO: the pitch glides (about 46 ms an octave), the envelope does not retrigger, no stuck note at a stop; a
    recorded legato take with SLIDE set on its steps (FEEL's TYPE) slides where the notes overlap; the roll's line and the
    strip's SL on a recorded step; no late audio halves with four tracks sliding.

### Decisions (ui-fix, the safer reading)

- ui-fix 1 behind OC_EDIT_SHEETS, 2 behind OC_STEP_PAGES, 5 behind OC_EDIT_SHEETS: the switches of the steps they
  belong to, so a build with a switch off is release-2's there; the scene change (3) is the pack's, not the image's.
- ui-fix 2 read as "no control on CHAIN reaches another chain" (every knob reaches 0 on the host); PRESETS picks the
  chain (the step pages' pattern). For the owner to confirm on the unit (check 23).
- ui-fix 3: the tail's flick is a sawtooth at COLR's rate (the scene's phases are sawtooths; a sine would need a new
  source), read as a cat's flick; the paw drops below the body's line at PLUCK's left end (a resting paw).
- ui-fix 4: the format consequence is the owner's call; until then design only and the page renamed.
- Items 1 to 3 and 5 shared one full host suite run (three small fixes and a table order); the diff review's fixes of
  1 to 3 and item 6 share one (the fixes are a message, a scene depth and test bars); 7 and 8 each their own. From item 6
  on the suite runs in a worktree of the item's commit (~/oc-ui-suite), so the next item's code goes on meanwhile.
- The full suite at ui-fix 6's commit failed one job: shift_test under the address sanitizer, a stack read past the HOME
  picture page's value buffer in hp_sound_line (ui_home_pic.c, step 1c: `val` was hashed uninitialized when no knob was
  turning; the new LATE check's frames made the sanitizer see it). The bug is in 0.16.0's image (a wrong signature at
  worst, no write); fixed in commit 2d38f7b. The suites queued for ui-fix 7 and 8 at their own commits would have
  failed the same job, so they were stopped and one suite runs at the head with the fix: it covers 6, 7 and 8 (the
  first suite covered 1, 2, 3 and 5). Written here so the per-item rule is seen to be bent, once, for this reason.
- ui-fix 7: the depth bound of a key on a recorded cell (more events than the history's seven free levels) is stated, not
  routed through a transaction (an entry runs over frames); the pads' natural length is the pads' page's only; a move into
  a recorded cell joins it (as note_transform); recording_prune stays as a function for the host check and the non-FAST
  path.
- ui-fix 8: the slide's own rate is three sixteenths of a semitone a control tick (an octave in about 46 ms), not a
  parameter; GLIDE set takes GLIDE's rate or time; a recorded slide needs the notes to overlap (no gate extension); no
  slide across a manual and a recorded step; FM6 keeps today's path in POLY (its own allocator); the velocity follows
  mono_play's rule.

## ui-fix2 (branch ui-fix2, from linux-hw; the owner's note on v0.16.0-f5057c, November 1, 2026)

Session B's items 1 to 7 of the owner's note; Session A does the PRESETS skipping, the NOTES "No edit memory" error and the DUB pages' controls
and none of those is touched here. Every item is behind a switch of its own (core.h, tools/build.py's list, tools/orangecat_engines.sh): OC_REC_CLEAR
(2), OC_PROJECT_PAGE (3), OC_SLIDE_TIME (4), OC_START_NUDGE (5); items 1 (the FX menu) and 6 (the SHAPES cat, a pack scene) have none, as ui-fix 3 and 5
had none. With every switch off the screens and the hash of ui_test are linux-hw's (item 1 and the scene excepted, which are what they change).

**For Session A: run the on-unit sequencer test on ui-fix2 before merge.** It touches the audio path in two places: voice.c's glide loop (a slide's
rate, an accumulator in a new byte of `voice_t`, taken from padding: the struct does not grow) and seq.c's slide_glide_set; and the sequencer's undo
(ui.c: a level may be linked to the one below it; UNDO_FX swaps a track's effects). What the unit test should add to the earlier checks (hardware
checks 28 to 34 below): a slide at SLIDE TIME -64, 0 and +63 on a MACRO and a SHAPES line in POLY, MONO and LEGATO with four tracks sliding (the audio
interrupt's time: the accumulator is a few instructions per gliding voice per block); REC held to clear on every page of item 2 while a project plays
on the other tracks (it only clears when stopped: check that a PLAY pressed during the count cancels it and nothing is cleared).

### ui-fix2 1: the FX menu, order and headings (no switch)

**Order** (params.c, with OC_EDIT_SHEETS; without it release-2's order stays): SENDS (the track's page that was called FX: how much of this track goes
into the shared effects; the new name says so), then the shared effects DELAY, DELAY 2, DELAY 3, REVERB, REVERB 2, CHORUS, SLICER, SLICER 2, (SL PITCH 1 to
4 where SLICER's length reaches them: the owner's list left them out; they stay where they were, between SLICER 2 and MASTER, **a call of mine**), MASTER,
then the track's own: DIST, AUTO PAN, DUB SETUP, DUB, DUB 2. Session A reworks the DUB pages' knobs: their names and order are as given. The FX button from
HOME lands on SENDS now (it landed on DUB; hardware check 22 below is replaced by 28).

**Headings.** The line at the foot of every FX page (the sound bar, ui_pages.c pv_sound): a shared page says **SHARED  all tracks** beside the four tracks'
colours (it said "All tracks"); a track's page says **THIS TRACK** and the sound's name beside the track's number badge. The header chip is the page's
name as before, the dots its place ("2/15"). Mockups: build/ui_steps/NIGHT/fxorder_01_sends .. fxorder_15_dub_2 (every FX page, in the FX button's
order, in every palette, linted: 0 findings).

### ui-fix2 2: clearing with REC held (OC_REC_CLEAR)

**Design.** ui_clear.c (included by ui_input.c). A REC press with the sequencer stopped and nothing else down is armed with the page's context
(clr_ctx: HOME, a SEQ page, a track's FX page, a GLO page, MOTION; a SHARED FX page is its own context that clears nothing; every other page
is none: REC held is what it was there). While armed, btn_hold's own 700 ms hold (Capture) is not asked for. Held 350 ms the count starts: the header's
message says "CLEARING TRACK 1 3", then 2, then 1 (300 ms each), a toast when long: no new drawing; at 350 + 3 x 300 ms the clear is done if REC is still
down, the page is the same and nothing else was pressed, no key played, the transport not started. Let go during the count: "KEPT", nothing cleared, the
release no tap (REC does not arm). A press shorter than 350 ms is the tap it always was. While the sequencer plays or counts in, REC held is Capture.
**What each clears**: HOME the selected track's steps and notes of the pattern it is on (seq_clear_now: the existing "Clear track", recorded notes through
the note history, manual steps through an undo level); a SEQ page the values on that page: FEEL, NOTES, TRACKER and DRUM HIT the chance, condition,
ratchet, nudge and accent/slide of the **16 steps of the cursor's page** (the page's tiles; **a call of mine**: the whole pattern would be a bigger
surprise), PHRASE its LEN DIV SWING GATE; TEMPO, CHAIN, SONG, PHRASES and GROOVE are the project's own values: nothing is reset there and the screen says
"NOT A TRACK'S VALUES" (**a call of mine**, the safer reading of "any SEQ page"); a track's FX page the selected track's effects (every param_jfx id and DUB's
setup) to the engine's defaults; **a SHARED FX page clears nothing** and says where its reset is (the heading says SHARED; the OCT+ held sheet's Reset
effects is its own, **a call of mine**); GLO (any page of its family; not the GLO layer) every track's steps, notes and own effects, **not** the
shared effects, the mix, the tempo or the sounds (**a call of mine**: "the whole project's music"); MOTION the selected track's lanes of the pattern it is on.
**One undo.** Every clear is a level of its own (undo_seal before load_begin) and EDIT held brings it back, EDIT + OCT+ redoes. A new level kind, UNDO_FX,
swaps the effects' ids (param_kept kept them out of every other level on purpose). GLO's four tracks are four levels linked by a byte of the level
(`link`: bit 0 "the level below is of this gesture", bit 1 "its note history is GLO's too"): one undo, one redo take all four. Recorded notes live in one
pool for all tracks and the existing note history snapshots the whole pool once; GLO takes that one snapshot (note_transaction.group = 1, a byte of
the struct's padding) and ui.c's undo calls clr_follow() after a note-history undo or redo so the linked levels (the tracks' manual steps, motion and
effects) go with it; the first track's level then carries only its effects (the history holds its steps and motion). If the edit memory is not there
GLO says so (NO EDIT MEMORY, note_tools.c's own) and clears nothing: Session A's fix of that error is on another branch and not merged here.
**Not done / limits**: the count is the header's message, not a drawn bar (a mockup of the messages is below); a frozen track is skipped by GLO as it is by every clear.
**Tests**: tests/rec_clear_test.c (31 checks: TIMING a tap, KEPT, the count 3 2 1 and the clear, playing = Capture, another button cancels; HOME manual and
recorded; FEEL / PHRASE / TEMPO; FX track and shared, undo and redo; GLO with four tracks and with recorded notes of two, one undo, one redo; MOTION), mutation-checked
(the follow call and the link bit each removed: the GLO checks fail). **Mockups** (docs/orangecat/ui/fix2/night_clear_<home|feel|fx|glo|auto>_<3|2|1>.png, a selection of them, every
palette linted, 0 findings): the count on five pages.

### ui-fix2 3: saving and loading projects, made obvious (OC_PROJECT_PAGE)

**Audit of today's flow** (what the owner met): SAVE tapped opens the sound's **USER** slots (SAVE > USER), not the project: the PROJECT page is the third
page of the family, reached by SAVE again or SELECT; on it OCT+ opens a sheet (Load, Save here, Rename, Boot, Erase) and nothing on the screen says it;
the slot in use was shown nowhere (the cursor row is the slot picked, which KNOB 1 changes, not the slot the music came from); Load loaded over unsaved
changes without a word (New song asked, Load did not); the autosave writes the slot in use after 5 s stopped and untouched without saying which (MENU >
AUTOSAVE, off the page); the quick save is SAVE held (0.7 s) with the transport stopped, nowhere said. Naming: Save here opens NAME (prefilled with the
project's name) and OCT+ writes it.
**Design.** SAVE opens the PROJECT page (SAVE again: the USER slots, and back; PRESETS has its own button). On it: the slot in use has its letter on a
coloured badge (in the list, and on its card); the picked card's second line says what OCT+ does ("OCT+ LOAD OR SAVE"; "Empty - OCT+ SAVE"; the slot in
use: "NOW - saved" / "NOW - unsaved changes"); the foot, in place of the sound bar, always says the slot in use, its name and **saved** / **unsaved** (**not
saved** on a new song with no slot yet). Load over unsaved changes asks "LOAD PROJECT B? Unsaved changes are lost" (OCT+ yes, OCT- no); New song already asked.
**"Unsaved"** is worked out in the input pass (project_unsaved packs the project into proj_scratch, which the drawing borrows between frames) at most once
a second while the page is up and the sequencer stopped, at once on entering the page and after a load or save. It compares the music's pack with the pack
taken right after the last load or save of that slot (pj_base), **not** with the hash the slot stores: a re-pack after a load differs from the stored record
in name fields (spaces against zeros, seen on the host), which would warn of changes that are not there; and G_SLOT, G_LOAD, G_SAVE (the slot picked,
the actions) are zeroed before packing, since picking a slot is no edit (the existing project_dirty counts it as one: seen). With no base (a boot, a
restore) it is project_dirty's: a warning without a change is possible, a missing one is not (**the safer reading**).
**Not done**: a renamed slot's NAME flow is the existing one; the page has four slots + template + New song as before (no "slot" picker by letter keys).
**Tests**: tests/project_page_test.c (21 checks). **Mockups** (project_new / saved / unsaved / sheet, every palette, 0 lint findings).

### ui-fix2 4: SLIDE TIME (OC_SLIDE_TIME, needs OC_SLIDE)

**Design.** One value per track, **P_SLIDET on the retired id P_RESERVED48** (the last free common id; the macros took 33 to 36; no P_COUNT change, no project
format change). Range -64 .. 63, default 0, shown as a percent (F_BIPCT) "how far from the old speed": 0 is the rate slides had (3/16 semitone a control tick,
an octave in about 46 ms: every project and sound saved before has 0 there, zeroed as a reserved id, and slides as it did), each 16 down doubles the speed (-64:
an octave in about 3 ms), each 16 up halves it (+63: about 0.7 s). It is the track's own setting like the effects: param_kept (a sound load leaves it),
user sounds store the default (upreset.c), motion events on the id stay inert (motion.c). GLIDE set still wins (its own time). The page is "SLIDE" in EDIT after
VOICE 2 (an ordinary one-knob page). The voice's glide step is now an accumulator (voice.c): a negative `gstep` is the rate in 1/256 of 1/16 semitone a
tick, its fraction kept in `gtick` (a byte of voice_t's padding); seq.c's slide_gstep computes 768 x 2^(-s/16) with a 16-entry table and shifts.
**Shown**: NOTES with ENV held: the SLIDE column says the word instead of ON: SNAP FAST QUICK NORM EASY SLOW LONG (thresholds -40, -20, -6, 5, 20, 40);
the tracker's foot on the FX column SLIDE says "ON SLOW". The editor protocol: the editor accepts the id now (it was reserved); the web editor's tables
and its `trackOwn` list do not know it yet (**Session D**: web/app/lib/editor_proto.js and web/editor.html TP[48]; the host test compares tables built
with the switch off, so it passes). **A per-step longer or shorter slide is not done** (the owner: "if it fits simply"): a step has no spare bits (step_t is
full: note[4], n, time, flags, vel, hit, acc, probability, cond, nudge; the project's step layout, the step snapshot, the clipboards and the editor's step
commands would all change: the format revision's, not this).
**Tests**: tests/slide_time_test.c (POLY, MONO, LEGATO, UNISON x seven values: the octave takes 4, 16, 32, 64, 128, 365, 984 blocks for -64 .. 63, within 10% and
2 blocks of 192 / (3 x 2^(-s/16)), never shorter as s rises, the same voice goes on; GLIDE set ignores it; kept across a sound load and a save; the words). Mockups
(slide_notes / slide_tracker / slide_page x snap, norm, long: 0 lint findings).

### ui-fix2 5: NOTES, KNOB 2 moves the note in time as NUDGE does (OC_START_NUDGE, needs OC_ONE_AUTH)

The owner's f5057c finding: START on a typed note converted the step through the note transaction and met the edit memory error (Session A's fix, not on any
branch yet: **not merged here, so nothing of this note depends on it**). K2 on a typed note now sets the step's **nudge** (ui_one_auth.c start_nudge_edit): a 64th
of a step a click (acceleration makes it more), -32 .. +31, the note stays a typed step, nothing is converted and the edit memory is never asked for; the START
card shows the step and the nudge in hundredths ("4.04"); the manual step history undoes it (EDIT held); it moves while the song plays (a recorded step's nudge,
like the tracker's, is plays-live); a chord moves together (the nudge is the step's); refused only while a chain runs or the phrase is frozen. A recorded note's K2
is its onset as before. **A call of mine**: ui-fix 7 made START on a typed note convert it (one authority for every note); the owner's words now ("as NUDGE does
in the tracker") read as the step's own NUDGE; the conversion is still reachable through LEN. **Tests**: tests/start_nudge_test.c (13 checks, the edit memory shown
never asked for).

### ui-fix2 6: the SHAPES cat (a pack scene, no switch)

COLR sets how fast the tail wags: the tail, curled over, pivots about its root and sweeps 42 degrees each side with SS_PH2 (a sawtooth at COLR's rate: the sweep
and a flick back). PLUCK sets how fast the paw bats the yarn: the paw pivots at the shoulder with SS_PH4: it rises slowly through 67 degrees and comes down on the yarn
at the wrap (a note's SS_HIT swats it too), the yarn's loops are knocked round at each strike (SS_PH4 on the loops), the paw's ball overlapping the yarn's edge at
the strike (the strike pose is the table's rest pose). TIMB is the yarn's size and its loops only (it no longer rolls at TIMB's rate; the eyes no longer follow it
rolling: still, looking at the yarn). **The phases run 0.5 Hz at a knob's left end to 3 Hz at its right** (ui_home_pic.c; it was 0.1 Hz at 0: a turn in ten seconds,
which at the left end nobody sees move; it is the same rule for every scene: only SHAPES' uses the phases in the shipping pack). The scene is data in the pack's
scene trailer (outside the pack's hash); the interpreter is one constant in the image. **Tests** (tests/home_pic_test.c, MOVES): the turns a phase makes in 2 s
(1.0 at the left end, 6.0 to 6.7 at the right, for COLR and PLUCK), the paw's pixels on the yarn's edge at the strike (5) and none when risen, TIMB's phase draws 0 pixels,
the tail's two phases 257 pixels apart, TIMB's and TONE's ends differ by 930 and 1,040 pixels. **Frame strips**: docs/orangecat/ui/fix2/night_shapes_cycle_paw_strip.png and
night_shapes_cycle_tail_strip.png (one cycle each, an eighth of a turn apart, NIGHT; the rate is the knob's).

### ui-fix2 7: the HOME shortcuts (docs only)

Listed in plain words in PLAY_LIST.md (the new section on top) and GUIDE.md ("HOME held: the shortcuts"): the hold 0.7 s = the MENU; HOME + a knob, PRESETS,
EDIT, OCT-/OCT+ and what each does on HOME's pages, a sheet, NOTES and FEEL; copy and paste are "LATER" in this build (OC_CLIP is not in the shipping set). No screen.

### Hardware checks (ui-fix2; the unit is Session A's, none run)

28. **FX by hand:** from HOME, FX lands on SENDS; FX again walks DELAY .. MASTER (SL PITCH where the slicer reaches them), DIST, AUTO PAN, DUB SETUP, DUB, DUB 2 and
    comes round; the line at the bottom says SHARED all tracks on the shared pages and THIS TRACK, the track's number and its sound on the others.
29. **Clear by REC held (stopped):** on HOME with a few notes: hold REC: the top says CLEARING TRACK n 3, 2, 1 and clears; let go at "2": KEPT and the notes stay; a tap arms
    REC as ever; PLAY pressed during the count cancels it; EDIT held brings the notes back. The same on FEEL (the page's 16 steps), PHRASE, a track's DUB page, MOTION, GLO
    (all four tracks; one EDIT held brings all back); a shared FX page says where its reset is; while playing REC held is Capture.
30. **Project page:** SAVE opens PROJECT; the slot in use has its badge and the foot says saved/unsaved (change a note: "unsaved" within two seconds; save: "saved"); Load over an
    unsaved change asks; Load over a saved one does not; New song; Rename; hold SAVE saves at once; the 5 s autosave writes the slot in use.
31. **SLIDE TIME on the panel:** a bass line with SLIDE on every other step at -64, 0, +63 on MACRO, SHAPES and a module in POLY, MONO and LEGATO: the glide is audibly very quick,
    as before, slow; the envelope does not retrigger; NOTES (ENV held) and the tracker's foot say the word.
32. **K2 on NOTES:** a typed note: K2 nudges it later and earlier a little a click (the START card moves in hundredths), the sound follows while playing, EDIT held undoes; no error.
33. **The cat:** PLUCK from its left end to its right: the paw comes down on the yarn once every two seconds to three times a second and the yarn is knocked round; COLR the same for the
    tail; TIMB only the yarn's size; no late audio halves while it moves (as check 4).
34. **HOME held:** the list in the play list is true on the panel (each combo of item 7 once).

### Decisions (ui-fix2, the safer reading)

- Items 2 to 5 each have a switch and are on in tools/orangecat_engines.sh (as ui-fix 7 and 8): a CI build of ui-fix2 carries them; they wait for the on-unit sequencer test.
- 1: SL PITCH pages stay between SLICER 2 and MASTER. 2: the page's 16 steps, not the pattern; the project's SEQ pages are not reset; a shared FX page does not clear; GLO leaves the
  shared effects, the mix, the tempo and the sounds; the count is the header's message. 3: SAVE tapped lands on PROJECT (the owner's "obvious"); "unsaved" is a count against what the music
  packed to at the last load or save, not the slot's stored hash, with G_SLOT and the actions left out. 4: the id P_RESERVED48 (no format change), percent shown, no per-step slide time.
  5: K2 on a typed note is NUDGE (not the conversion). 6: the phases' floor 0.5 Hz for every scene.
- Not merged: Session A's "No edit memory" fix (not on any branch when this was written); GLO's clear with recorded notes needs the edit memory and says so when it is not there.
- The items shared one full suite run (below), not one a part: the switches are independent, the parts small, and the review of the diff came first.

### The independent reviews of ui-fix2 (two agents on the diff, November 1, 2026) and what was done

**Items 1 to 3 (18 findings; 3 blockers, all GLO's one undo):** (1, 2, 3) the single note history could be undone or redone out of the stack's order, left stale after
another note edit, or overwritten by a later clear, leaving recorded steps with no note (ghosts) or notes with no step: the history now goes first only when the level
it belongs to is the one the undo (redo) is on (clr_history_ok); a history that was overwritten or invalidated takes its levels out of the stack (clr_group_drop: GLO's undo is
then gone, as any single-slot note history is gone after the next note edit); tests for the sound-load-after-GLO order, the stale redo and the overwritten history, mutation-checked.
(4) the history's track no longer swaps its motion twice (link bit 2). (5) a group is refused whole when any member's phrase changed (undo_group_ok). (6) one undo level only
(no cache RAM): GLO says UNDO MEMORY FULL and clears nothing. (7) an FX or page reset no longer clears the track's automation (HOME and GLO still do: the existing Clear track
does; said here). (8) the PROJECT page's pack ran once a second and bumped proj_wire_gen, which a web editor transfer's staging RAM is checked against: it is worked out only
when the panel is touched (and on entry, after a load or a save). (9) the selected track and BOOT no longer count as an edit. (10) the MIXER, the GLO button's first page, is GLO's.
(11) the renderer's SENDS scene. (14) a knob that changes the track or FEEL's page cancels the count. (15) pages that cannot clear and a frozen track say so at once, no count.
(16) a clear is sealed so a sound browsed next is its own level. (13) the foot says THIS TRACK alone (the name was cut); item 1 has no switch: said above. **Not done:**
(12) ui_test and sheet_test under OC_PROJECT_PAGE assert the old SAVE > USER flows and fail 12 and 1 checks: they are not run with the switch (project_page_test is its suite),
**open**; (17) NOTES, TRACKER and DRUM HIT reset FEEL's values (chance, condition, ratchet, nudge, accent, slide), not their own (velocity, length): the owner's call; (18) LOAD of the template row
does not ask, and while playing the flag is not worked out.

**Items 4 to 6 (12 findings; 1 blocker):** (1) id 48 was the slicer's DEPTH (default 127) in projects before commit 1e47387: such a project would have loaded as SLIDE TIME 63 (slow
slides): a project value outside -64 .. 63 loads as 0 (a value 0 .. 63 of an old customised slicer depth cannot be told apart: **residual**, only slide steps are affected).
(2) the web editor wrote the id as a sound parameter: ED_SET and its sibling refuse the id (the unit's own panel is where it is set until Session D's table has it). (3) tools/hw/rectest.py
expects "ON": the word is added in the tracker's foot only (tk_hint), the hardware-test string stays "ON". (4) one_auth_test's START checks assert the conversion: under OC_START_NUDGE
they are out and a job runs it with both switches (the shipping pair). (5) the tail was a sawtooth with a snap: now two copies by phase, a triangle, 21 degrees out and back.
(6) the paw: HIT's swat smaller, the yarn's knock a quarter turn (its comment); the strike's ball overlaps the yarn's edge near TIMB's middle and minimum and its middle at the top
(**open**, minor). 3 Hz at 9 frames a second is about three poses a cycle: it reads as a bat. (7) the F_BIPCT percent is a scale, not a time (the comment says so; NOTES says it in words).
(8) the test's turns formula fixed (the printed COLR 5.7, PLUCK 6.0 in 2 s; the window tightened). (9) tests added: OLD (a 127 loads as 0), the accumulator restarting, every track's own value
saved; **still not covered**: user sounds not storing the value, motion on the id, the word thresholds. (10) K2's nudge is shown in the START number, **not drawn in the roll** (open).
(11) ui_mockup's phase rate. (12) the gtick comment.

### ui-fix2: suite and budget (November 1, 2026)

The full host suite with CI's settings (ci_step.py test --parallel 1, on afab159, the commit the reviews' fixes were made in) **passed in 4,766 s**, with the new jobs: ui-fix2 2, 3, 4, 5 (and one_auth with both
switches), the renders with every switch (0 lint findings) and home_pic_test. Budget (capacity 577,248 B, reserve 8,192 B; measured with ci_step build, CI's options):

| Build | Image B | .data+.bss B | pool B |
|---|---|---|---|
| linux-hw (09e233a) | 562,288 | 94,552 | 180,656 |
| every ui-fix2 switch off (items 1 and 6 only) | 562,656 (+368) | 94,552 | 180,656 |
| + OC_REC_CLEAR (2) | 563,084 (+428) | 94,584 (+32) | 180,656 |
| + OC_PROJECT_PAGE (3) | 564,364 (+1,280) | 94,600 (+16) | 180,656 |
| + OC_SLIDE_TIME (4) | 564,708 (+344) | 94,600 | 180,656 |
| + OC_START_NUDGE (5), all on, before the reviews' fixes | 564,180 (-528: layout) | 94,600 | 180,656 |
| all on, after the fixes (the pushed head) | 565,044 | 94,600 (+48) | 180,656 |

565,044 B leaves 12,204 B free: 4,012 B above the 8 KB reserve. The steps are not isolated costs (code layout moves between builds: item 5 measured smaller than item 4 before it). Item 6 (the scene) is in the pack, not the image; its image cost is a constant.

## ui-fix2 close-out (November 1 to 2, 2026): the open items closed, and the suite built with the build set

The owner's note after ui-fix2 asked for five open items closed and for the suite to build every test with the build set's switches on, as CI ships them, so that a switch that is on is
always tested. Each fix was pushed to origin/ui-fix2 as it was made.

**a. ui_test and sheet_test on the new flows.** With OC_PROJECT_PAGE, SAVE tapped lands on PROJECT and SAVE again on USER: the checks take the extra tap (and say PROJECT first); Load over
unsaved changes asks first (the test answers YES). With the whole set on, `ui_test` failed 42 checks, not 13: 22 of them assert the category map, the engine EDIT cycles and the SONG sheet
that OC_EDIT_SHEETS replaced (sheet_test has them: every engine, every row), four are NOTES' knob jobs of OC_STEP_PAGES (step_pages_test), three a key on a recorded step that OC_ONE_AUTH
overwrites (one_auth_test), one REC held (OC_REC_CLEAR: rec_clear_test), one the mixer's bar drawing (mixer_test). Those checks are guarded `#if !OC_x` with the name of the test that has the new
flow; the SAVE / Load / FRAME-flow ones are updated, not guarded. `sheet_test`'s DRUM-X PRESETS walk stops at the LANES table (PRESETS picks the lane there: lanes_test) and its SAVE > USER
check takes the extra tap. **Found by this work and fixed:** `pj_pack` (the unsaved flag's pack) left bank_pack's step fields (RATCH, FLAM, COND, NUDGE: `proj_x`) tagged with the last
pack, so the next LOAD of the slot applied the live music's fields instead of the record's (host only: a flash build refills them in ocj_read); and, found by the review, the first fix
(restoring the tag) still let a FEEL-only edit come back through a Load that discarded it. The unsaved flag now packs for a hash alone (`bank_live_hash`, `bank_pack_keep`): the stored
record's fields, lists and pids are not touched at all. Tests: REPACK (a second load keeps the fields), DISCARD (a FEEL-only edit is unsaved, and a Load that discards it gives the saved
fields back). Mutation-checked.

**The suite builds every test with the build set (tests/run_tests.sh).** `BS` is every `OC_x=1` of tools/orangecat_engines.sh, put in the compiler command (a job's own flags come later and
win); `BS_OFF` is the same list at 0. Baselines that mean "release 2" are built with BS_OFF (home_pic_off, home_pic_off2, the release-2 render, the format before's generator and the
conversion job), and `ui_legacy` runs ui_test with every switch off, so the old paths stay tested. The first full run with the set found what no run had shown, because no test had ever
been built with ONE_AUTH, STEP_PAGES, HOME_PIC or PROJECT_PAGE together. What was wrong, and what was done (all in the commits of this section):
- project.c called `pj_note()` of the UI: ten tests that build project.c without ui_slots.c did not compile. A generation counter (`proj_ui_gen`) the page reads instead.
- GLO's undo helpers (`clr_follow`, `clr_group_drop`, `clr_history_ok`, `undo_group_ok`) sat in ui_clear.c, which only ui_input.c includes: prophet_integration_test did not link. They are in ui.c now.
- Tests whose fixtures fill the event store by hand (oc_project_test, oc_r3_test, oc_pool_test, backup_test) made events with no flagged cell, or flagged cells with no event; OC_ONE_AUTH mends
  both at a load, so "saved = loaded" failed. Fixtures keep the invariant (`tests/one_auth_fixture.h`, `fixture_cells_for_events`); the web app's fixture records were regenerated (the
  fixture's cells are flagged now: web/app/tools/gen_layout.mjs; identical with and without the set).
- oc_pack_test and usb_host_test did not know OC_PACK_MAXLEN (HOME_PIC's pack with a scene trailer is longer than the tables); the breathing key check of ui_test depended on the frame count
  before it (now it looks at all eight phases).
- **Pinned (closed in "ui-fix2 close-out 2" below: converted, BS_NOSTEP gone):** `seq_notes_test`, `seq_edit_test` and `note_tools_test` assert the interface before the step pages (NOTES' KNOB 1 browses, KNOB 2 is START, ...). They are built with
  BS_NOSTEP (OC_STEP_PAGES and what needs it off) and say so in run_tests.sh. The new interface's side of the same code is step_pages_test, one_auth_test, rec_clear_test, start_nudge_test
  and ui_test on the full set. Converting the three is open (about 46 checks). `drum_editor_test` is pinned the same way: the DRUM engine is not in the build set (MELODEE_DRUM=0), and under OC_ONE_AUTH it **hung** (a restore of a timed note into slot 1, which the converted cell already holds, closed the event list into a cycle: the test's misuse, found by the first full run, killed after 98 minutes), then failed two checks of the old model.
- `step_pages_test`: a manual note's START is its NUDGE under OC_START_NUDGE (the check says so).
- tests/golden.txt: with the set on, `song/4track_mix` (a mix with slide steps) plays them as ui-fix 8's legato glides: 7b5647232840caa8 -> 9e96d61252be67d8, the only golden of 410 that moves (bisected: OC_SLIDE).
  golden.txt is the shipping build's now; the old paths keep a run of their own (`regress_legacy`: the set off, that one line as it was; 0 changed).

**b. K2's nudge is drawn in the roll.** A typed step's onset bar starts `nudge / 64` of the column later or earlier (`ui_graph.c`: the column is 12 px, so one detent is 0.19 px and eight
are one pixel and a half; never left of the roll's first column; the end stays the step's). `steps_hash` includes the nudge, so the roll redraws by itself. start_nudge_test ROLL: +16
changes only the first three pixels of the bar, -16 reaches into the column before, 0 draws what it drew.

**c. NOTES, TRACKER and the drum grid reset their own values.** A page reset (REC held on the page) now also puts a typed step's **velocity** back to the default (0: 96) on NOTES and
TRACKER, and a typed drum step's lane accents and velocity on the drum grid and DRUM HIT; FEEL resets FEEL's values only. **Decision (the safer reading):** never the notes themselves
(pitch, a typed note's length in ties, the lanes' hits: HOME's clear takes those) and never a recorded step's velocity, start or length: those are events of the pool, edited only through
the edit memory (Session A's), and a played velocity is the performance, not a setting. One EDIT held undoes the whole page reset, velocity included (rec_clear_test OWN, 7 checks).

**d. Loading the template row asks.** Load of the template over unsaved changes opens "LOAD THE TEMPLATE?" (CF_LOAD_PROJ with the template's slot); YES loads it, OCT- keeps the music.
With no template saved it asks nothing (the review: NO TEMPLATE says so at once). Saved music: no question (project_page_test TEMPLATE, 4 checks).

**e. Slot 48 in the web editor.** `web/editor.html` and `web/app/lib/editor_proto.js` (the same protocol text; the web app's test checks that) describe TP[48] as SLIDE (F_BIPCT, -64 .. 63,
default 0) and `trackOwn` is true for 48, so a sound load or a factory preset in the editor leaves it, as the unit does. The editor's SET of 48 stays refused (editor.c, unchanged) and the
mock's `paramReserved(48)` stays true: the id is set on the unit's panel. web/test_web.mjs: the mock's tables equal the firmware's (built with the set: SLIDE at 48); "removed effect
parameter is inert" now says 48 has a description and no write; DESC count 120. web/app/test: slot 48 is the track's own, described as SLIDE.

**The review (one independent agent on the close-out diff):** 8 points. Should-fix, done: (1) the tag restore was incomplete (above); (2) the baselines built with the set hid a regression
of the release-2 screens: BS_OFF for them. Nits done: the stale comment in ui_feel.c, the template question only with a template, build.log out of the repository. Left, written down: the SONG
sheet's "first row" check is under the sections guard though it is not about sections (the Insert row is covered by sheet_do); an overlapping negative nudge covers the previous column's bar
at the same pitch.

**Close-out: suite and budget.** The full suite with CI's settings (`ci_step.py test --parallel 2`) on 04958c9 (the close-out, the golden, the legacy runs): every job passed but one, `drum_editor_test`,
which hung under OC_ONE_AUTH (above; killed after 98 minutes, the run reported 8,077 s); the test is pinned and passes alone. The closing run is the r3-format suite (branch r3-format, bc8d4ad, which
holds ui-fix2 at 8b37931 merged): **240 jobs passed in 3,096 s** (the ui-fix2 jobs among them). Image with every switch on: **565,556 B** (+512 B on the 565,044 B before the close-out: the roll's nudge,
the page resets, the template question, the hash-only pack, GLO's helpers moved), 94,600 B .data+.bss, 180,656 B pool: 11,692 B free, 3,500 B above the 8 KB reserve.

## ui-fix2 close-out 2 (November 2, 2026): the hang, the time limit, the four pinned tests converted

**The hang (drum_editor_test, 98 minutes under OC_ONE_AUTH): the test did it, and the firmware let it.** The test restored a timed note into slot 1 with `recording_restore_note`;
under ONE_AUTH slot 1 already holds the hit's converted event. The primitive took the old event out of the pitch index and the live list but **not out of the track's event chain**
(`recording_head` / `recording_next`), then linked the slot in again: the chain became a cycle, and every walk of it (here NOTES' jog in `notes_selected`, the same loop is in the roll, erase and the
tracker) never ended. Found with a stack of the stuck process (gdb, SIGINT): `recording_active <- notes_in_step <- notes_selected <- notes_step_jog <- drum_hit_jog`.
- **Firmware (recording.c, `recording_restore_note`): the old event leaves its chain first** (`recording_unlink`, which also drops the pitch index entry, the live link, the repeats, the schedule).
  Equivalent to the old calls for an empty slot (unlink returns at once there, the repeats are dropped as before); for a slot in use it is the fix.
- **Is it in the shipped Nightly?** The primitive is, so the fix belongs in the next one. **A path from the panel to it is not shown:** the callers (undo of an edit, the clip paste, ONE_AUTH's
  conversions, the MIDI paste at ui_input.c) take a free slot or one they have just freed, and the history is invalidated when a take reuses a slot (seq_notes_test "slot reuse by recording undoes the
  take"). So this is a hardening of an invariant, not a field bug I can reproduce. **For Session A: take recording.c's three-line change into the next Nightly** (tests/restore_chain_test.c).
- **tests/restore_chain_test.c** (job "Restoring an event over a slot in use ..."): restores over a live slot, over the chain's head, and into another track's owner; walks the chain bounded and by
  visited slots; `alarm(30)`. It **fails on the old code** (three checks: a slot twice in the chain, the head, an event moved to another track) and passes on the new; no hang either way.
- The drum_editor_test no longer restores into a used slot (it takes a free one, as the firmware's callers do).

**A time limit on every host test.** `run_tests.sh`: `limited` (timeout -k 10, `TEST_TIMEOUT` seconds, default 900, times 4 under `SAN=1`) wraps the run of every test of the `t` jobs and of the `step`
runs of binaries in $OUT; an expired limit prints "TIMED OUT after N s: <command>" and fails that job (exit 124) and the suite goes on. The slowest jobs of the closing runs are minutes, not a quarter of an hour.
The multi-step jobs that run python or cc are not limited (their pieces are).

**The four pinned tests converted (about 46 checks).** `BS_NOSTEP` is gone. seq_notes_test (29 checks failed under the set), seq_edit_test (13), note_tools_test (1) and drum_editor_test (2) run on the interface as
it ships. `tests/notes_knobs.h` names NOTES' knobs once: with OC_STEP_PAGES, PRESETS browses the notes and steps, KNOB 1 is PITCH, 3 LEN, 4 VEL and ENV + PRESETS the zoom (`nk_zoom`); with it off, release-2's
(KNOB 1 browses, 2 PITCH, PRESETS the zoom). So **each test also runs with every switch off** (BS_OFF: jobs `seq_notes_legacy`, `seq_edit_legacy`, `note_tools_legacy`, `drum_editor_legacy`), the legacy job kept for
builds with the switches off. Almost all of it was the knob map. Three checks are different, **decisions made here, the shipped design stands**:
- seq_notes_test, "MIDI audition on NOTES ... without entering": with ONE_AUTH a MIDI note on a recorded step is that step's entry (as a key: one_auth_test KEY), so it enters and replaces the events. The check says
  so (sounds, enters, releases) and that **EDIT held twice** (the entry, then the events' deletion: one level each, as one_auth_test KEY) brings the take back byte for byte. The old check stays for the switch-off build.
- drum_editor_test, "hit undo restores pitch and natural length": the first edit converts the hit's cell to events, and a converted hit carries its step's whole length (flagged), so "natural" is one step; the check
  says that. The old assertion stays for the switch-off build.
- drum_editor_test, "SELECT can edit a repeated hit": the cell's snare is an event now, ahead of the repeated kick in the jog's order (onset, then pitch): two detents, not one.
- seq_edit_test, "PRESETS has no STEP editing role": PRESETS now browses; the check says it edits no step and moves the cursor.

**The review of close-out 2 (one independent agent):** 6 points. Must-fix, done: `limited` could not run a shell function (`powercut_tests`, `old_to_new`: timeout exits 127), so two jobs would have failed on every run; functions run unlimited now, and a kill (137) is reported as a timeout too.
Should-fix, done: the redo check of drum_editor_test now checks the duration (the converted hit already had `length` 1, so it could not fail). Nits done: check names that said KNOB 1 for the browse knob, restore_chain_test's per-walk flags.
Verified by the reviewer: `recording_unlink` in place of the two calls is equivalent for empty slots and fixes used ones (no double decrement of the pitch count; it was inflated before); every firmware caller restores into a free or just-unlinked slot.
**Open, for the owner (a design question, not a test bug):** the first pad edit of a drum cell converts the whole cell to events with `length` 1 (gated at one step), so an untouched snare beside an edited kick is now cut at one step instead of decaying naturally;
UI_BUILD.md ui-fix 7 point 6 says a hit's natural length is `length` 0. Host only today (MELODEE_DRUM=0 in the image). The converted test enshrines the built behaviour; if the pads' conversion should keep `length` 0, that test's ONE_AUTH branch goes with it.
Left: python, node and cc steps of the multi-step jobs have no limit of their own.

## ui-fix2 merged with linux-hw round 14 (November 2, 2026): ready to merge, for Session A

**For Session A: ui-fix2 (1109c2b plus this note) is ready to merge into linux-hw (fd159be).** The host suite (`ci_step.py test --parallel 2`, the build set on in every test, a time limit on every test) passed on 1109c2b: 4,612 s with a second suite running beside it.
The on-unit sequencer test for ui-fix2 is still yours (checks 28 to 34, and an own SLIDE TIME).

**The conflicts, and who won (Session A's behaviour kept wherever the two disagree):**
1. `ui_clear.c`: round 13 rewrote GLO's undo helpers where they stood; ui-fix2 had moved them into `ui.c` (a test that builds ui.c without ui_clear.c did not link). **They stay in `ui.c`, with round 13's bodies:**
   `clr_history_ok` asks `resource[RES_NOTE_HISTORY].size` as well, and `clr_next_is_group` is added (round 13's "a group clear whole or not at all"). ui_clear.c keeps only the pointer comment.
2. The web editor's descriptor table (`web/editor.html`, `web/app/lib/editor_proto.js`): round 13 made ids 45 to 47 DUB TAPE's SPEED, MODE and TAPE (id 48 empty); ui-fix2 had put SLIDE at 48. **Round 13's three stay; 48 is SLIDE.**
3. `tests/fixtures/app/fbkk-full.bin`, `web/app/lib/mock_data.js` (generated): taken from linux-hw, then regenerated from the merged firmware (`web/app/tools/gen_layout.mjs`); `--check` passes.
4. Not a text conflict but a disagreement: round 13 removed `!transport_busy()` from the project rows' Load question ("LOAD asks while playing too"). **The template row's Load question had the same condition (ui-fix2 close-out 1d): it is gone there too**, so both ask while playing.

**Files touched (against fd159be), the firmware's ten:** `pattern_store.c` (the hash-only pack), `project.c`, `recording.c` (`recording_restore_note` leaves its old chain first), `ui.c`, `ui_clear.c`, `ui_draw.c`, `ui_feel.c`, `ui_graph.c`, `ui_input.c`, `ui_slots.c`; the web: `web/editor.html`, `web/app/lib/editor_proto.js`, `web/app/lib/mock_data.js`, `web/app/lib/layout.js`
(regenerated), `web/test_web.mjs`, `web/app/test/test_sounds.mjs`; the rest are tests (`run_tests.sh` with the limit and the legacy jobs, `notes_knobs.h`, `restore_chain_test.c`, the four converted tests, fixtures) and this file. `build.log`, committed by accident in an earlier close-out, is removed.

**What changes for the player (on top of what fd159be already has of ui-fix2):**
- K2 (START) on a typed note moves it by its NUDGE, and the roll draws the nudge (the onset bar shifts in its column).
- A page reset (REC held on NOTES, TRACKER or the drum grid) also puts a typed step's own velocity back (the drum grid: its lane accents); never notes, never a recorded step's velocity, start or length.
- Loading the template row over unsaved changes asks first ("LOAD THE TEMPLATE?"), also while playing; with no template saved it says NO TEMPLATE.
- SAVE opens the project page (SAVE > PROJECT > SAVE > USER, the unsaved flag a hash-only pack: a discarded FEEL-only edit no longer comes back, and RATCH, FLAM, COND and NUDGE are no longer lost at the next Load).
- Slot 48 (SLIDE TIME) is described in the web editor (the unit still refuses its SET from there).
- A MIDI note or a key on a recorded step replaces the step's events as the other keys do (ONE_AUTH); two EDIT holds bring the take back (one level each: the entry, the events' deletion). Not new here; a test now says it.

**For the next Nightly (firmware, not ui-fix2's alone):** `recording_restore_note` took an event out of the pitch index and the live list but not out of the track's event chain; restoring over a slot in use linked it twice, a cycle, and every walk of the chain looped for ever
(the 98-minute drum_editor_test hang). No path from the panel to it is shown (the callers restore into a free or freshly unlinked slot), so it is a hardening: `recording.c`, three lines, `tests/restore_chain_test.c`.

**Open for the owner:** the first pad edit of a drum cell converts the whole cell to events with `length` 1 (gated at one step); an untouched snare beside the edited kick is cut at one step instead of decaying. The ui-fix 7 text says a hit's natural length is `length` 0 (host only today: MELODEE_DRUM=0).

## ui-fix2: a drum cell's edit never changes a sound you did not touch (November 2, 2026): for Session A

**For Session A: ui-fix2 (the commit after 3463f98) carries this fix; the host suite (`ci_step.py test --parallel 3`, the build set on, a time limit on every test) passed on 43f8453 in 3,080 s.** The owner's rule: an edit never changes a sound you did not touch. The first pad edit of a drum cell converts the cell to events
(ONE_AUTH, `note_convert`); the converted test had enshrined that an untouched snare was then cut at one step. **Two things changed the untouched hits, both fixed:**
1. **Length** (`note_tools.c` `note_convert`): every converted drum hit was flagged `length` 1, gated at its step; a manual hit plays its sound's natural length. Converted hits now keep `length` 0 (natural); the pad's length edit sets the flag on the one hit it touches
   (`notes_event_edit` slot 4 already did: from natural, up is a length; below a sixteenth is natural again).
2. **Accent** (`seq_notes.c` `notes_rebuild`): a cell holding one accented lane (velocity above 110) got the step flag `SF_ACCENT`, and a recorded step plays **every** event of an accented step at 127 (`recording_shape`): converting a cell with an accented snare made the unedited kick play at 127 instead of its 100.
   Found by the render check, not by looking: for a drum track the step flag is no longer set from the events' velocities (the lanes' `acc` carries the accent, per lane, as a manual cell does). Not changed, for the owner: a **melodic** recorded chord still accents as a group (one note over 110 accents the whole step, release 2's rule): an edit of one note's velocity can change the others.

**The check** (`tests/drum_editor_test.c`, `untouched_lanes`): a cell of a kick and an accented snare is played by the sequencer into `mix_block` for 1,400 blocks (about a second, several steps) and rendered; then the same cell after a one-pad edit (pitch +7, then a half-step length) of one lane, rendered
with **that** lane's level at its attenuation maximum, so what is left is the untouched lane; the two renders must be equal sample for sample, for the kick edited and for the snare edited. It fails on the old firmware for each of the two causes alone (mutation-checked: the old length alone fails two checks, the old accent alone one). The converted
test asserts the natural length again (`!length`; a half step from natural is eight sixteenths up, `HALF_STEP`), and `one_auth_test` PADS drives the same gesture. Both models (the set on, every switch off) pass.
**What changes for the player:** a cell of drum hits you edit with one pad keeps every other hit exactly as it sounded; the hit you edited changes. No other behaviour moves.

## Release 3, step 3 (branch r3-pages, from linux-hw fd159be; November 2, 2026): fw_pages version 3. For Session A

**Status: built and reviewed; host tests only, nothing flashed.** RELEASE3_PLAN.md section 5, step 3; MODULE_FORMAT_R3.md 4, 5, 7 and the new section 14 ("as built", with the corrections). **For Session A: r3-pages is ready to merge: the host suite (`ci_step.py test --parallel 3`, a time limit on every test) passed on 0d906fa in 3,061 s (the commits after it are this note),
and your review is wanted on two things: the checker and the interrupt-side macro records.**

**What it is.** `fw_pages_t` version 3 (80 bytes: `ui`, `pid`, `ext`), `fw_pid_t`, `fw_ui_t` (the picture and HOME's four knobs, 20 bytes), `fw_kdef_t` (28), `SC_FX` after `SC_DXLANE`; the checker `fw_pages_check_ex` (scope, P_COUNT; the pid table's rules; the effect scope's rules) and
`fw_ui_check` (a bad picture or knob table is not a refusal); the registry of validated pictures and resolved knob records in RAM (`oc_knobs.c`), filled at registration by `oc_modui.c`; the three structures hashed into the interface identifier. The identifier moved (the unit's
`iface` 0xCC9AED54 -> 0x588CCCF2; `FW_API_ID` 0x126F12CC -> 0x8679336A): **every module is refused until rebuilt and reinstalled.** SHAPES, WHEEL, the Prophet (and the build's other modules) are rebuilt by CI; the Prophet's descriptor is version 3 with `ui`, `pid`, `ext` 0;
WHEEL and SHAPES have no descriptor. The installer page needed no code: it compares the package's identifier with the unit's and says "wrong firmware" (`modWrongFw`); `web/test_ocm.mjs` section 4 proves it on this build's packages (taken) and the same packages with the Release 2 identifier (refused).

**Proof on the host (tests):** `tests/mod_iface_r3_test.c` (the real headers of Release 2's SHAPES, WHEEL and Prophet, `tests/fixtures/r2_modules`, are refused with FW_MOD_BAD_IFACE and the identifier alone is the reason; this build's images, every start, are accepted; the identifier moved by exactly the generator's change);
`web/test_ocm.mjs` 4 (the page); `tests/mod_ui_test.c` (80 checks: the checker on wire bytes, every bad field of the picture and knobs, the registry, the interrupt's records through `mac_on`/`mac_off`, retire, the loader's order read in oc_module.c's text, a 60,000-image fuzz under the sanitizers: whatever is accepted and published
holds the registry's invariants; mutation-checked); `gen_fw_api_id.py --selftest` moves the id for a field of each new structure; the existing `mod_pages_test`, `fuzz_fw_pages`, `module_host_prophet_test`, `fw_api_test`, `hot_set_test` and the module tests pass on version 3 (`mod_pages_test` expects 80).

**Size against RELEASE3_PLAN.md section 2 (CI's build, `ci_step.py build`, SHAPES not in the image; the same build of fd159be beside it):**

| | image B | .data+.bss B | pool B |
|---|---|---|---|
| linux-hw fd159be | 545,772 | 94,600 | 180,708 |
| r3-pages | **548,264 (+2,492)** | **95,144 (+544)** | 180,708 |

Against the body's room (`BODY_MAX` 573,440 B) the free flash is 25,176 B, and 16,984 B above the 8 KB reserve. **The cost is 1,000 B over the plan's 1,100 to 1,500 B estimate** (it was `oc_mod_boot` with the picture check and the publish inlined, +1,682 B; `oc_mod_pages_check_rd` +396 B; `mac_value`,
`kdef_of`, `modui_of`, `oc_mod_unreg` +300 B together); the RAM is the registry, 3 records of 180 B (the document said 330 B: it counted 112 B a record and no labels or ranges). The plan's central estimate for 7a stays counted in its column; the owner decides if the +1,000 B matters.
The same figure for the plan's own image (562,144 B at b1997d1) is not this build's: fd159be is already 545,772 B without SHAPES.

**Every place oc_module.c is touched (for the merge with your module area, flash map step 1): four touches in three places, all marked "r3-pages" and none inside a function that step 1 is likely to change:**
1. after `oc_mod_pages_check` (about line 271): `#include "oc_modui.c"` (the new file) and a new function `oc_mod_ui_publish(i, h)`;
2. `oc_mod_unreg` (about line 349): one line first, `modui_retire(e);`;
3. `oc_mod_boot` (about line 427): one line after the registration block, `oc_mod_ui_publish(i, oc_mod_hdr_ptr(i));`.
`oc_mod_pages_check_rd` is **not** touched (it calls `fw_pages_check`, now a wrapper). Other files: `fw_pages.h` (the format and the checkers), `oc_knobs.c` (the registry, `kdef_of`), `oc_macro.c` (`mac_value` reads the record's range: 2 lines), `oc_scene.c` (`scene_of`: the module's scene first),
`ui_home_pic.c` (the cache looks at `modui_gen`), `tools/gen_fw_api_id.py`, `tools/module/prophet_mod.c` (3 zero fields).

**What Session A must review:**
1. **The checker** (`fw_pages.h`): `fw_pages_check_ex` and `fw_ui_check` read bytes a module controls. The review (an independent agent) found the bounds sound (`fwp_in` cannot wrap; every count is bounded before it multiplies) and no way for a 68-byte version 2 descriptor through. Yours to confirm: **the stack**, which I could not measure on the unit:
   `fw_ui_check` holds a 1,024-byte scene buffer, `fw_pages_check_ex` about 300 B more (the id list and the slot bitmap), `modui_publish` about 330 B; all run on the main loop's stack from `persist_boot -> oc_mod_boot` and from the installer's `oc_mod_reload`; `-fstack-usage` would settle it.
2. **The interrupt-side macro records** (`oc_knobs.c` `modui_t`, `modui_rng`, `kdef_of`; `oc_macro.c`; `oc_modui.c`). The interrupt reads only the RAM record: ids already resolved to track slots, ranges copied. The review's finding, fixed: **a knob's target may only be P_E0 .. P_E7** and the copied range is the panel entry's cut to the slot's own descriptor (the `legacy` slot can be any track slot; without this a module
   could aim a macro at P_VOICE with any range). Publish and retire run in the main loop under `motion_guard`; `mac_on` and `mac_off` are one `mix_block`, so a retire cannot fall between them; `oc_mod_unreg` retires before `eng_unregister`. Yours: whether `motion_guard` is the right guard for the registry (the throw code's tables use it), and the extra three compares
   a target costs in `mac_value` for the built-in tables (`modui_rng` looks at three records; not measured on the unit).

**Decisions I made (the safer reading):** `fw_ui_t` is 20 bytes by dropping the document's reserved u16 (it summed to 22 and unaligned); plain knobs resolve to track slots like macro targets (a plain knob on a native byte is dropped); the picture needs a knob table (as before); a registry of 3 (a fourth module loads without picture); the checker allows any
`legacy` slot but the knob path only E0..E7; the real modules ship with `ui` 0 (no module has a picture yet: `fw_ui_t` is exercised by the tests, not by a shipped module).

**Not done (plan step 4, and the others'):** `fx_desc_t`/`fx_entry_t` and their place in the identifier's hash; the module-id install path and the Effects table; "PICTURE REFUSED" in the Engines table (the record holds the code); the install-time advisory result on the card and the page (the page's `ocmCheck` is unchanged);
the web editor and app tables for `kdef` and scene bytes (Session D, plan step 8).

### r3-pages: the main stack (November 2, 2026; for Session A, answering the stack question above)

**No cut needed: every path keeps well over 1 KB.** The firmware's clang is 4.0.1 and has no `-fstack-usage`, so `tools/stack_depth.py` reads the frames from the linked image's code (`build/melodee.dis`): a function's frame is its pushes (four bytes a register of
`[--sp] = {rets, r15-r4}`) plus its `sp += -N`, and the call graph is the direct calls plus the callbacks written down in the tool (the readers, the scene check: a pointer call cannot be followed from the code, so an edge missing there would make a depth too small; the
module path's are listed). The main loop runs on the user stack of `app.ld`: `_ustack_lo` 0x01C74100 to `_ustack_top` 0x01C7A000 = **24,320 B**; interrupts and the supervisor use `_sstack` (7,936 B). From `fm1_main`, linux-hw fd159be against r3-pages:

| path | before | after | | margin after |
|---|---|---|---|---|
| power-on boot: `fm1_main > oc_mod_boot > oc_mod_check > oc_mod_pages_check > oc_mod_pages_check_rd` | 3,844 | 5,680 | +1,836 | 18,640 |
| install over SysEx: `fm1_main > ed_service > ed_handle > oc_mod_end > oc_mod_reload > oc_mod_boot > ... pages_check_rd` | 10,580 | 12,416 | +1,836 | 11,904 |
| install from the card: `fm1_main > ui_input > fb_mod_file > oc_mod_end > oc_mod_reload > oc_mod_boot > ...` | 8,472 | 10,316 | +1,844 | 14,004 |
| the whole main loop's deepest path (`ed_service > ed_handle > up_boot > ... st_read_range`, storage, not the module path) | 18,800 | 18,800 | 0 | **5,520** |

Where the +1,836 is: `oc_mod_boot`'s own frame 180 -> 1,680 B (the picture check's 1,024-byte scene buffer and the publish's record, inlined into it) and `oc_mod_pages_check_rd` 212 -> 548 B (the checker's id list and slot bitmap). The audio side: `mac_on` 92 -> 112 B, `mix_block` 1,140 B
unchanged, on the supervisor stack. If a margin ever matters, the cheap cut is the scene buffer: a static in `.pool` (the check runs only at boot and install, one at a time) takes 1,024 B back; not done, since the least margin of any path is 5,520 B and none of the
module paths is the deepest. `run_tests.sh` has a step (`stack_depth.py --check`) that fails when the deepest path leaves under 1 KB. Caveat: a depth read from the code is an estimate (every push counted, none popped early), not a measurement on the unit.

### r3-pages, November 3, 2026: DUB's needs folded into step 3 (the identifier moves once). For Session A

**r3-pages had not been merged, so this is on r3-pages itself (not a new branch), after merging linux-hw round 15 (7c57605; one conflict, `web/test_ocm.mjs`, both sections kept).** The host suite on the last commit is stated at the end of this entry.

**What changed since 667ac44 (all in `fw_pages.h` unless named):**
1. **The format limit is `F_DUBSPD`** (was `F_TONE`): the checker accepts the DUB formats and `F_PANLR`, `F_ST16`. The fuzzer's model and `mod_pages_test` follow it.
2. **`shown` and `put` for effects, with the instance signature, in the hashed structure.** `fw_pages_t.shown` and `.put` are unions at the same offsets (24, 28): the engine's (`track_t *`) and the effect's `fx_shown(slot, instance, id, &value)` and `fx_put(slot, instance, id, value)`. Wire size 80 unchanged,
   `_Static_assert`ed on the device. Their signatures are inside the structure's text, so they are in the hash (`gen_fw_api_id.py --selftest` moves the id for a change to either). SC_FX may carry these two; it still refuses `native`, `active`, `motion_skip`, the title lists, `loaded`, `env_source`, `bend`, `knob[]` and `cc[]`.
3. **Views:** a pid with `FW_PID_VIEW` (0x8000) has no legacy slot and needs both hooks (`FWP_PID` otherwise); `modui_publish` never offers a view as a knob target.
4. **MODULE_FORMAT_R3.md section 15's DUB example is data in `tests/mod_ui_test.c`** (`make_dub`: three pages, 14 panel entries, the ten kept ids with today's legacy slots, four views, hooks over `dub_get_field`/`dub_put_field`) and **passes the checker as SC_FX**; the hooks are exercised (SYNC/PING/DRIVE/WOW read from MODE 65 and TAPE 8, TIME's format by SYNC, a write back in one store). 98 checks; mutation-checked (the limit, the views rule, the hook set, the view's slot).
5. **The identifier:** `FW_API_ID` 0xCAF53177, the unit's `iface` **0x1400CEEF** (was 0xCC9AED54 in Release 2 at fd159be). SHAPES, WHEEL, the Prophet (and the rest) are rebuilt by CI. `mod_iface_r3_test` (Release 2's real headers refused with FW_MOD_BAD_IFACE, this build's accepted, every start) and `web/test_ocm.mjs` section 5 (the page: old refused as `modWrongFw`, new taken) pass.

**Size and stack** (CI's build, SHAPES not in the image; linux-hw 7c57605 beside r3-pages d255be9): image **546,224 -> 548,332 B (+2,108)**, .data+.bss 94,904 -> 95,480 (+576). Against the 667ac44 figures (+2,492 / +544 over the older base) the step's own cost fell by 384 B of flash (the compiler's choices, and round 15's tables; not a change I made), the RAM
rose by 32. Stack, `tools/stack_depth.py` (24,320 B user stack): boot 3,852 -> 5,688 (+1,836), install over SysEx 10,588 -> 12,424, from the card 8,480 -> 10,332, the whole loop's deepest 18,800 unchanged, margin 5,520 B.

**What Session A must review (additions to the earlier list):** (a) the union: the host will call `fx_shown`/`fx_put` through the effect side only for SC_FX descriptors, and the engine's through the track side; nothing in the checker can tell which a hook is, only the scope (it checks the address, not the signature); (b) the view rule is a convention of the id space
(0x8000), checked by the loader but not by the project writer: the project format must not store a view (step 4/7); (c) `F_DUBSPD` as the limit freezes that enum order as the module interface (a format added later extends the limit, an older firmware refuses the module).
**What the owner will notice:** nothing on the unit yet (no shipped module uses the new fields): modules must be reinstalled once because the identifier moved, which the page says as before.

**Suite:** the full host suite (`ci_step.py test --parallel 3`, a time limit on every test, the stack check included) passed on 97f1e04 in 3,635 s; the commits after it are this note.

## ui-fix2: a recorded chord's notes keep their own accents (November 3, 2026): for Session A

**For Session A: ui-fix2 carries this too (the drum cell fix's sibling).** The owner's rule, for melodic recorded steps: an edit never changes a note you did not touch. Before, `notes_rebuild` (and `seq_erase`'s rebuild) set the step flag `SF_ACCENT` whenever any event of the step had velocity above 110,
and `recording_shape` played **every** event of an accented step at 127: one hard note made the whole chord loud, and editing or deleting that one note (flag recomputed) made the others suddenly quieter, or lowering one note's velocity changed its neighbours. Now:
1. **`recording.c` `recording_shape`:** a note plays at 127 when the step has a deliberate accent flag **or its own velocity is above 110**, else at its own velocity: each note keeps its own accent from its own velocity.
2. **`seq_notes.c` `notes_rebuild`, `seq_erase.c`:** the flag is **not derived from the velocities** any more (and no longer cleared by a rebuild): it is what the player (FEEL, the tracker) or a project from before set. The drum cell's guard of the last fix is subsumed.
3. **`ui_input.c` `notes_event_edit`, velocity (KNOB 4):** on a step that carries the flag (a deliberate accent, or an old project's derived one) the edit first bakes it into the **other** notes (velocity 127, what they were playing) and clears the flag, so the edit changes the touched note only and the others keep playing exactly as they did.
   (Superseded below, November 3: the undo now brings those bytes and the flag back.) A project from before is untouched until a note of such a step is edited: it plays as it did.
4. **`ui_graph.c`:** the PATTERN page's accent colour for a recorded step also shows when its loudest note is accented (it read the flag, which is no longer set by the notes).

**The check (`tests/accent_own_test.c`, in the set and with every switch off):** a recorded chord (C, E, G; velocities 100, 90, 120) is played by the sequencer into `mix_block` for about a second, the edit is made (the accented note 120 -> 80; the same note deleted; an unaccented note 90 -> 60 beside an accented one; the same with a step accent on),
and the chord is played again. **VOICE:** every note not touched is played at the velocity it had before (the hook of `recording.c` logs each hit), and **RENDER:** the render after the edit equals the render of the expected chord (the untouched notes as they were played, the touched one as edited) sample for sample. The chord's notes are not independent in the mix (a limiter sits
behind them: subtraction of one note's render from the chord's is off by up to 16,000 counts), so the isolation is by the expected chord, not by subtraction. On the old firmware four VOICE checks fail (the accented note edited, deleted, and the step-accent cases); the RENDER checks pass there because the expected chord rebuilds the old accent too: VOICE is what separates, RENDER is exactness of the final state.
**What the owner will notice:** playing a chord with one hard note no longer makes the other notes of the chord loud (they play at their own velocities: **a recording made before sounds as it did until a note of its step is edited**; a new recording of a chord with one hard note sounds softer on the other notes than the same take did); editing one note's velocity no longer moves the others.
**Open (for the owner):** the step-accent flag on a recorded step can only be cleared by FEEL (TYPE), the tracker or a velocity edit; there is no other place that shows a recorded step as accented by flag, only by note.

**Suite:** the full host suite (`ci_step.py test --parallel 2`, the build set on, a time limit on every test) passed on d7a2cdd in 5,131 s (run beside the r3-pages suite); the commit after it is this note.

### ui-fix2: the velocity edit on a flagged step undoes completely (November 3, 2026): for Session A
**For Session A: ui-fix2 (6d7a15f and after) carries this.** The edit that bakes a step accent into the other notes and clears the flag **now undoes and redoes completely**. Before, the undo restored the flag (the step's snapshot) and the edited note but left the other notes at 127 (the history held one event).
- **`seq_undo.c`:** each history state also holds the notes an edit baked (`bake_idx`, `bake_old`, at most `STEP_BAKE_MAX` = 8, 25 B a state, 225 B of the `.pool` history in all); undo puts each back to its old velocity, redo to 127. The state is filled by `notes_event_edit` and copied at `step_history_finish` with the edit.
- **`ui_input.c` `notes_event_edit`:** the bake now happens only once the edit is certain (after the "nothing changed" return), records what it changed, and **refuses with "TOO MANY NOTES"** when a flagged step holds more than eight other notes below 127 (rather than change them where the undo could not bring them back).
- **Check (`tests/accent_own_test.c`):** a flagged chord is edited, EDIT held undoes it, then redo, then undo: after each the step's bytes (`step_t`, the flag among them) and every note's bytes (`recorded_note_t`) are compared with the state before/after, and the **render** equals the render before/after, sample for sample. It fails on the previous firmware (two byte checks), not on the sound alone: the flag came back already, the notes did not.
- **The test itself was made stricter on the way:** its renders were vacuous before (the default engine ignores velocity, and a MONO track plays the last note of a chord only, so most "RENDER" checks compared the same one note; the VOICE checks were the real ones). It now plays a velocity-sensitive POLY engine (MACRO), each take in its own forked child of a process that never rendered (the DSP state is then the same for every take: the leftover state of an earlier render made two identical chords differ),
  and the chord's notes start a few samples apart by pitch (events of equal time come out of the chain in the order put in, and an edit puts its note back first: which voice plays which note changed with the edit, same sound, other phases). With that the RENDER checks fail on the old accent logic as well as the VOICE ones.
- **UI_DESIGN.md** now says what the step accent is (section 4, "Accent: a note's and a step's"): the deliberate accent (FEEL's TYPE, the tracker, a manual step's, a project's from before), never derived from the velocities; a note's own accent is its velocity above 110.
**What the owner will notice:** nothing new on the unit except that EDIT held after a velocity edit on an accented step now restores the other notes' velocities too (the display of a note's velocity shows its stored value), and a refusal message in the rare step with more than eight notes.

**Suite:** the full host suite (`ci_step.py test --parallel 3`, the build set on, a time limit on every test) passed on 6d7a15f in 3,396 s; the commits after it are this note.

### r3-pages, November 3, 2026: step 8's examples found what the frozen interface lacks. For Session A, before step 3 ships

**Step 8 (the reverb modes and the FX layer's buffer effects as modules) is written as worked examples in MODULE_FORMAT_R3.md section 16 and as data in `tests/mod_ui_test.c`** (126 checks, sanitizers, the fuzz): SPRING, SHIMMER, RESO and CLOUD (344, 340, 388, 340 B of descriptor data) and GLITCH, TAPE STOP, SHUFFLE and FREEZE
(284, 256, 264, 296 B) each pass `fw_pages_check_ex` as SC_FX **as written, within the frozen interface**, and each knob equals what the firmware shows today. **They cannot say everything the brief asked, because of these (section 16.4); the checker is not changed:**
1. **L1: a legacy slot can name only a track parameter, and the reverb's values are `song.g`.** A `song.g` id written as a legacy slot passes the checker (it is below `P_COUNT`) and would be read from the track: silently wrong (the test shows it). So **a Release 2 project's reverb values do not load into a module through the pid table**; the import must also be conditional on TYPE
   (one stored `G_RPRE` is PRE, AIR or PITCH by the mode). Proposal: bit 15 of `legacy` selects `song.g`, the checker takes a `gcount`; no layout change, no identifier move, but checker and loader code.
2. **L2: a module's page cannot hold TYPE** (the host's chooser). A convention for "the host's knob here" is wanted (0xFF in the row).
3. **L3: no mark for RAM-only parameters** (the layer's knobs are "never saved"): views with two do-nothing hooks work today; a "volatile" bit in the pid would remove the hooks. As the data stands, nothing stops a loader from storing them.
4. **L4: an action knob** (TAPE STOP's SCRATCH moves the tape by hand, by detents): `put` has a value, not a delta.
**What I ask:** decide L1 to L3 (and L4) before step 3 ships, since each is a rule in the checker or a convention in the pid bits that a shipped module would be built to; the examples then change in one place each (`legacy` of the reverb, K1 of `REVERB`, the layer's pid bits). **Not stopped:** nothing in what is built needs them; the module path of steps 4 and 5 does not touch these effects.
Also for Session A: the byte costs (reverb bank of three 1,072 B of descriptors, CLOUD 340 B, the layer 1,100 B plus about 160 B of hook code) are small beside the kernels' (plan section 6); no packed fields exist in either, so no views for packing.

### r3-pages, November 3, 2026: L1 to L4 decided and built. The identifier did not move. For Session A

**Decisions taken (the owner's) and built on r3-pages before step 3 ships; MODULE_FORMAT_R3.md 16.5 has the text.** **The interface identifier did NOT move** (`FW_API_ID` 0xCAF53177, the unit's `iface` 0x1400CEEF, unchanged; `gen_fw_api_id.py --print` and `--selftest`): every decision fits in a field that already exists (the descriptor's `ext` word, a row's 0xFF, bits of the pid and of `legacy`), no hashed structure's text
changed, so SHAPES, WHEEL and the Prophet were not rebuilt for it and `mod_iface_r3_test` and `web/test_ocm.mjs` ran again in the suite without change. **L4 was a pid bit, not the kdef flags**: the card's knobs are panel entries of the module's page, not HOME's `fw_kdef_t` knobs, so the kdef flags are not where it belongs; no layout change.
- **L1.** Bit 15 of `legacy` = `song.g` (effects' globals only, each once, `gcount` beside `pcount` in `fw_pages_check_ex`). **The import rule is declared in the descriptor**: `ext` = a `song.g` index + 1 (bits 0 .. 15) and a value (bits 16 .. 31); the legacy slots are imported only while `song.g[index]` equals it (the reverb: `G_RTYPE` == the mode); otherwise defaults.
  The loader code is `firmware/src/fw_fxstore.h` (`fx_legacy_import`, `fx_legacy_export`): header only, **not included by the firmware yet** (the image does not grow for it; the host of steps 4 and 5 will include it). **Proof: `tests/fx_legacy_test.c` (38 checks, in the suite):** a Release 2 project of each mode is packed by the firmware's writer and loaded by its reader; the mode's module holds the resident reverb's value of every knob, the other modes' modules their defaults,
  and a track played through the reverb renders sample for sample the same with the resident project loaded as with the module's values written into the legacy slots (the resident reverb stands in for the module's kernel, which does not exist yet; a control with default knobs renders differently); an out-of-range stored value (PITCH 30 under RESO) is clamped by the module and plays as the resident reverb plays it. Mutation-checked (the condition removed: 12 fail; the export dropped: 5).
- **L2.** 0xFF in a row = the host's knob (the reverb's TYPE). Checker rule: an effect's page has at least one knob of its own (FWP_ROW otherwise). Tested.
- **L3.** `FW_PID_VOLATILE` 0x4000: not stored, no hook, no legacy slot; at most one flag bit per pid (view 0x8000, volatile 0x4000, relative 0x2000). `fx_store_pack`/`fx_store_unpack` (the project writer's and reader's skip, in fw_fxstore.h): a view, a volatile or relative pid and an unlabeled entry are not written, and a pair for one is ignored on reading. The layer's examples have no hooks. Tested, including the round trip.
- **L4.** `FW_PID_RELATIVE` 0x2000: no value, never stored, `put` receives the signed detents; the checker requires the `put` hook. TAPE STOP's SCRATCH uses it (the one hook of the layer's effects).
- **The FX layer keeps its quick-layer cards** as the player's surface; its descriptors only supply the cards' knobs: written into UI_DESIGN.md (section 8) and MODULE_FORMAT_R3.md 16.5.

**What Session A must review:** (a) **the `song.g` import path**: `fx_legacy_import` / `fx_legacy_export` in `fw_fxstore.h` and the condition in `ext` (an engine's descriptor with `ext` != 0 is refused; the index must be an effects' global; the condition applies to every legacy slot of the descriptor, not per entry; **a project that stores no TYPE of its own** (a Release 2 project always does) would import for the mode TYPE names); (b) the checker's new rules (`fw_pages.h`: the `song.g` record, the flag bits, the row rule, `ext`), +16 B of stack on the module paths (boot 5,704 B, install 12,440 B; the deepest path 18,800 B unchanged, margin 5,520 B);
(c) that the reverb's **import is conditional on TYPE only through this descriptor**: the resident ROOM mode has no module and keeps `song.g` as now; (d) that a value written back by `fx_legacy_export` is the active instance's only (the host calls it for the selected mode).
**Size** (CI's build, SHAPES not in the image): image **548,444 B** (+112 B over the previous r3-pages commit's 548,332 B; the checker's new rules), .data+.bss 95,480 B (unchanged); against linux-hw 7c57605 (546,224 / 94,904): +2,220 B flash, +576 B RAM. **Suite:** the full host suite (`ci_step.py test --parallel 4`, a time limit on every test, the stack check included) passed on 0bb18b8 in 3,028 s; the commits after it are this note.
## dub-perf (from linux-hw fad3864; November 3, 2026): DUB TAPE's performance view, layout C. For Session A

**Status: built, host tests only, nothing flashed.** **Host suite: passed on `6c3d286` in 2,380 s** (under the shared lock, 4 jobs; it had passed on `8d77a88` in 3,817 s before the trim and the control-note test); later commits change only documents. The owner picked layout C of UI_DESIGN.md section 7's options. Commit `8d77a88` (+ this note). **Files: it touches Session A's: `jfx_dub.c`, `seq.c`, `midi_control.c`** (the owner's task named the kernel triggers and the keys), and ui.c, ui_draw.c, ui_home_pic.c, dub_fields.h; new: `dub_keys.h`, `dub_scene.h`, `tests/dub_perf_test.c`; `tests/ui_render.c` and `run_tests.sh` gained the page and the job.

**What it is.** HOME's PICTURE mode gains the DUB page (`ui.home_pg` 3) in the turn PICTURE, DUB, TRACKS, CHORD while the track's ECHO is above 0 (`home_pic_cycle`); knobs FDBK SEND FILTER ECHO through `stage_page`; the loop picture; the octave C4 to B4 (keys 7 to 18; key 0 is F3) as the page's keys while it shows (`dub_view_on`, set by ui_draw.c; `dub_key` in dub_keys.h, called by `keyboard_block` after the layer check): throws C to A (13 38 64 89 108 127), CUT B, KICK C#, HOLD D#, DIVE G#, RISE A#. **Three new triggers, CC 19 HOLD, 20 DIVE, 21 RISE** (`dub_trig[4][6]`; `dub_map`: HOLD forces FEEDBACK 1.00, DIVE SPEED 0 (an octave down), RISE SPEED 127, both = normal; the parameter-copy signature includes them). **Recording:** knobs on their lanes; a throw writes the SEND lane (its level, then the knob's value on release), CUT the ECHO lane (0, then the knob's), only while the track records and is the selected one; KICK, HOLD, DIVE, RISE are not recorded (no lane; PROJECT_FORMAT_R3.md 8.2b on r3-format `e0e8b7a` designs it; the page says so: "BLACK: NOT RECORDED").

**A kernel change you must review: ECHO 0 now plays as CUT.** The render test for "CUT recorded as ECHO 0 plays identically to the live trigger" **failed at first** (3,722 samples differed by up to 8 % of the peak around the two edges): ECHO 0 also set the wet gain to 0, which glides, on top of the cut's 3 ms ramp. `dub_map` now keeps the wet of the last ECHO above 0 (`dub_i_t.wet_keep`, 4 B an instance) and uses it while ECHO is 0, so the output is closed by the cut alone, as CUT does; ECHO back up is the same glide as releasing CUT. After it the renders are equal sample for sample (a mutation back to `wet = echo * 0.01` fails the test). `dub_insert_test` and `jfx_dub_test` pass unchanged. What the owner may notice: ECHO turned to 0 mutes in 3 ms instead of gliding over the old wet slope.

**Proofs (tests/dub_perf_test.c, 36 checks, sanitizers; mutation-checked: nine mutations each fail it).** Keys (each throw, a throw walked 100, 70, 30, B/C#/D#/G#/A#, F# empty, keys outside the octave play notes, page closed plays notes, a key let go after the page closed releases), CC 19 to 21, recording (the events, the steps, the values, none when not recording, none for the four), **renders through the resident kernel sample for sample: CUT = ECHO 0; a throw = SEND at its level; HOLD = FDBK 100; DIVE = SPEED 0 (short times: the knob would also grow the line, DIVE does not, the taps stop at its end); RISE = SPEED 127**, each against a control that differs; the pages and the four knobs, the knobs' lanes while recording, the draw and the budget (12,500 px mean gate for this page: it measures 11,876 px a 100 ms frame = 15.9 ms of SPI, worst 20,064 px; section 3's other pages gate at 11,000, the DUB row says 16.4 ms). `ui_render` lints the page in every palette (5 screens): **0 findings** (my first help line overlapped the sound line: 108 findings, fixed).

**Size (CI's build against linux-hw fad3864 built the same way; trimmed at `6c3d286`):** image 554,472 -> **556,456 B (+1,984 B)** (556,636 / +2,164 B before the trim), .data+.bss 95,736 -> **95,768 (+32 B)**. **The estimate was 750 to 950 B of flash and 16 B of RAM; flash came out 2.1 times it.** Where the bytes went (`nm -S` of the two ELFs, by symbol; the compiler inlines, so groups are the honest unit): **the HOME picture's drawing +1,262 B** (`draw_page` -3,434, `scene_draw` +2,198, `hp_inputs` +1,042, `hp_sound_line` +800, `hp_dials` +584, `home_pic_cycle` +72: before the change the picture's helpers were inlined into `draw_page` because each had one caller; the DUB page is a second caller, so they became real functions, and that cost was not in the estimate); **the keys and the scene +588 B** (`dub_key` 226, `SCENE_DUBTAPE` 239, `dubk_rec` 52, `dv_keys` tables 53, `DUBK_*` 18); **the kernel and UI glue +222 B** (`dub_map` +76 for the held wet, `mix_part` +54, `stage_page` +46, `draw_head` +46). **Trimmed without changing behavior (-180 B):** the help line's three alternatives and the fit loop (the widths are 274, 216 and 174 px against 200: it is always `BLACK: NOT RECORDED`, measured) and the DUB picture's never-taken fallback (`dv.scene` was always set). **Proof:** `tests/ui_render` on `7261d14` and on `6c3d286`: all 994 images byte for byte equal (15 of them the DUB page's), `dub_perf_test` 36 checks, draw budget unchanged (mean 11,876, worst 20,064). What is left is the feature's own code and the non-inlined picture helpers; I did not force them inline (I expect `always_inline` would only move the same bytes back into `draw_page`; not tried). Against `BODY_MAX` 573,440 B: **16,984 B free, 8,792 B above the 8,192 B reserve** (it was 18,968 and 10,776). Stack (`tools/stack_depth.py`) at `8d77a88`: deepest path 18,800 -> 18,792, margin 5,528 B (the trim touches no call depth).

**Owner's decisions (round 11), all as built:** CUT stays recorded as ECHO 0 (render-identical; not a control note); the throw levels stay on the SEND lane (no fifth control number); the DUB scene stays in the image until the pack has room; the octave choice, the throw levels in EDIT and the key LEDs are not built. **New test, `tests/control_notes_test.c` (in the suite):** control notes 124 to 127 (PROJECT_FORMAT_R3.md 8.2b) through every note-range path: the recorder takes them (`recording_valid`: 127 valid, 128 refused), keeps the number, replays at the captured time and gate, and a save and load returns them byte for byte; MIDI in with QNT off passes 124 to 127 unchanged at every OCT and TRN; every layout (SNAP, WHITE, ALL) and every key at OCT -3 to +3 and TRN -24 to +24 gives silent or 0 to 127; a stored note is not moved by OCT or TRN on replay. Mutation-checked (the recorder's ceiling lowered to 123: 10 checks fail). **For Session A, from that test:** the numbers 124 to 127 are reachable by ordinary notes (MIDI notes 124 to 127 pass as they are; the top key at OCT +3 and TRN +24 clamps to 127), so a note on those numbers is not a control note unless the recorder tags it; the test pins this as a fact (two `CLASH` checks) and the control-note design must say what tags it (a flag in the event, a track-level DUB-on test, or the clamp keeping ordinary notes at 123). To be checked on the unit.

**Decisions I took, for you to overturn:** (1) **The scene is in the image** (`dub_scene.h`, 239 B), against the owner's decision 5 (scenes in the pack or a module): the pack's scene trailer has 93 B spare and the build refused 41,112 B in the 40,960 B area; it moves to the DUB module's own picture at plan step 6. (2) The page is in the turn only while ECHO is above 0, as the design's "while it is on". (3) The keys are the physical keys 7 to 18, whatever the OCT buttons say. (4) Several throws held: the newest, and the highest of those left when it is let go. (5) The help line is "BLACK: NOT RECORDED" (the longest of four that fits the sound line). (6) One event a step an id: a throw or CUT shorter than a step records only its release value.
**Not built:** the octave choice and the throw levels in EDIT, the keys' LEDs, the black keys' recording.

**What Session A must review:** the `dub_map` change (ECHO 0 and the wet), the HOLD/DIVE/RISE overrides (the line is sized at the start from the SPEED knob; a DIVE with long times clamps the taps at the line's end), `keyboard_block`'s new branch (it runs in the audio interrupt: it writes `dub_trig` and calls `motion_capture` under its own guard, only while recording), and that `dub_key` takes the track from `kb_trk`.
**What the owner will notice:** HOME turns through a fourth page for a track with ECHO above 0; on it the octave C4 to B4 stops playing notes and becomes the dub keys (the other keys play); the black keys' line says they are not recorded; turning ECHO to 0 mutes quicker; MIDI CC 19 to 21 are new.


**Fixtures (round 9; the owner asked for them, I had used this tree's writer).** Committed binary Release 2 projects, written once by the Release 2 writer (`project_capture`, `proj_pack`) as it is on this branch, in `tests/fixtures/r2_projects/` (`415038c`): `reverb_spring.bin`, `reverb_shimmer.bin`, `reverb_reso.bin`, `reverb_cloud.bin`, `reverb_reso_clamp.bin` (a stored G_RPRE of 30 under RESO) and `dub_all_pages.bin` (DUB FDBK 91, SEND 100, FILTER 33, ECHO 77, TIM L 40, TIM R 90, SPRNG 55, SPEED 100, SYNC FREE, PING on, DRIVE 11, WOW 7: a value on each of DUB SETUP, DUB and DUB 2); 13,212 B each (the Release 2 record); README there. `tests/fx_legacy_test.c` now loads these files with the reader only (`proj_import`, `project_restore_runtime`), so it still proves Release 2 loading after the format revision changes the writer; `tests/gen_r2_fixtures.c` makes them and is not part of the suite (regenerate only to make new fixtures). **The `ext` condition still covers every legacy slot of a descriptor, as built.** The test is 48 checks now (45 on the fixtures, 3 more for DUB's render): the DUB module's descriptor (`tests/fx_dub_data.h`, shared with mod_ui_test) imports the fixture's values on all three pages, and **a DUB render proof**: the loaded fixture's DUB through the resident kernel (jfx_dub.c, the track's insert) equals, sample for sample, the same values set directly on a fresh track; a control with DUB at its defaults differs. **Suite: the host suite (`ci_step.py test --parallel 4`, a time limit on every test) passed on `415038c` in 5,251 s** (slower than the 3,028 s of round 8: another session's suite ran beside it). The commit after it (`fx_legacy_test`'s DUB render proof, test only) is covered by that test run alone (48 checks, 0 failed), not by a second full suite. For Session A to review here: the fixtures are bytes of this branch's writer, which is also what Release 2 wrote (`FUN17`, 13,212 B): if you know Release 2 changed the record after this branch's base, regenerate nothing: say so and I will add the shipped file.

### r3-pages, November 3, 2026: Session A's review of fw_fxstore.h, three findings fixed. For Session A, before the step 5/6 host includes it

`fw_fxstore.h` is still not in the image (header only; no firmware file includes it). The three findings of the review, each fixed and each with a check that **fails on the code before** (run on the tree at `0bb18b8`'s sources with the new `tests/fx_legacy_test.c`: 5 of the 8 new checks fail there, 56 checks, 0 failed on `c55102e`):

1. **`fx_legacy_export` had no range clamp.** It wrote `values[i]` as it was; the import clamps. Now each value is clamped to its entry's `min`..`max` before it goes into the legacy slot (the same clamp, the same rule: a legacy slot is never outside the entry's range). Checks `EXPORT RANGE` (seven values outside their ranges are written clamped; the import reads them back inside the ranges and a second export changes nothing). Fails before: SIZE 500 was written as 500.
2. **`fx_legacy_export` lacked the import's condition**, so the module of a reverb mode that is not the selected TYPE overwrote the shared SIZE and DAMP (and PRE, MOD, RATE, FILT, WIDE). Now the descriptor's `ext` is read as the import reads it: the slots are written only while `song.g[index]` equals the value; otherwise nothing is written and 0 is returned. Checks `EXPORT CONDITION` (SHIMMER selected with its own SIZE 77, DAMP 33, PRE -7: the SPRING module's export leaves all three; with SPRING selected it writes all seven, so the condition is not a blanket refusal). Fails before: SHIMMER's values were overwritten.
3. **The checker let an engine carry volatile or relative ids.** `fw_pages_check_ex` (fw_pages.h) now refuses (`FWP_PID`) a volatile (`FW_PID_VOLATILE`) or relative (`FW_PID_RELATIVE`) id in the pid table of an engine (scope `SC_MOD`): an engine's values are all stored, its project record has no such class, and `fx_store_pack` is not run on engines. Views (`FW_PID_VIEW`) are still allowed on an engine with both hooks (they were not part of the finding; say if you want them refused too). Checks `ENGINE IDS` (a volatile id refused, a relative id refused with its `put` hook present so it is the new rule and not the old hook rule that refuses it; a view and a plain id accepted). Fails before: both accepted.

The module interface's identifier did not move (no structure changed; `gen_fw_api_id.py --check` is in the suite). The checker's added test is two compares in `fw_pages_check_ex`; its size in the image is not measured separately (the suite's size gates passed).

**Suite:** the full host suite (`ci_step.py test --parallel 4`, `TEST_TIMEOUT` 900, through the lock) passed on `c55102e` in 2,677 s. Branch `r3-pages`, nothing else changed.

## melodic-grid: SEQ > GRID, the melodic step grid (November 3, 2026): for Session A, to merge and put on the Nightly

**Merge the trimmed head (November 4, 2026): the head of `melodic-grid` after the size trim, not `05ac62c`.** The trim shares the dials' PITCH and LENGTH text with NOTES (`dial_pitch`, `dial_len` in ui_draw.c, both pages call them) and keeps the page helpers out of line. It saves **160 B** on GRID (+3,932 B with `OC_MGRID=1` against `=0`, was +4,092 B; the image 550,428 B, was 550,652 B; RAM +16 B as before), not the 900 B estimated: what GRID costs is its own logic (`graph_mgrid` +1,068 B, the keys, knobs and lights in `ui_input` +1,064 B, the four dial columns +626 B, `fm1_main` +378 B), which sharing does not remove. The gate held: all 1,570 pictures of the render test (the shipping switch set, every palette) byte for byte as at `05ac62c`, the 69 `mgrid_test` checks, the 13 mutants. The merge notes below are unchanged by the trim.

**Status: built and tested on the host; nothing flashed, no installer run.** Branch `melodic-grid`, cut from ui-fix2 `bdc5945`; behind the switch **`OC_MGRID`** (core.h; needs `OC_ONE_AUTH`; exported `=1` in `tools/orangecat_engines.sh`, so the Nightly has it; `tools/build.py` passes it). The plan is MELODIC_GRID_PLAN.md (section 8 has the owner's answers and what was built), the page is described in UI_DESIGN.md section 4 and GUIDE.md ("GRID"). No new note format, project format, audio or scheduler change: `step_t`, the recorded notes, the history and the shared edit functions are as they were; `FW_API_ID` is still `0x126F12CC`, the project record is still 13,212 B.

**What the owner will notice on the unit** (pitched tracks only: not DRUM, DRUM-X, kit or slice tracks):
- SEQ now steps through one more page, **GRID**, between TRACKER and PHRASE. PHRASE still has LEN, DIV, SWING and GATE.
- On it the 16 white keys are the steps of the page shown, the first four black keys (F# G# A# C#) are pages 1 to 4 (steps 1 to 64; a page or step past LEN is dim and does nothing). The keyboard starts **silent** (the pill in the corner says **STEP**, the picked step's key is lit, steps with notes dim, the page lit).
- A white key **picks** a step (nothing is written). When that key is let go the keyboard goes to **NOTE** (the pill is filled, the screen has a frame, the lights show the normal playing layout) and the next keys played are that step's note or chord (up to four, black keys are sharps). When every note is up it is back in STEP on the same step. The cursor never moves by itself. **OCT-** in NOTE cancels: the step as it was.
- External MIDI notes go into the picked step. KNOB 1 steps the cursor (across pages), KNOB 2 picks a note of the chord, KNOB 3 its pitch, KNOB 4 its length (NOTES' PITCH and LEN: the same code, so a manual note's LEN converts the step to a timed note as it does on NOTES). PRESETS moves the cursor. EDIT tapped deletes the picked note (the step if it is the last); EDIT held undoes (a replaced chord is one undo).
- On every other SEQ screen of a pitched track the strip at the top has one more page in it (the only difference in the 313 changed pictures of the render test, all in y 8 to 21); the DRUM, DRUM-X and HOME pictures are byte for byte as they were.

**The `seq.c keyboard_block` conflict with dub-perf (expected; Session A is merging dub-perf now).** `keyboard_block` has, after the layer check, the chain `if (song.grid == 2u) ... else if (song.grid) ... else ...`. melodic-grid changes that one test to `song.grid >= 2u` (GRID's STEP stage is `song.grid` 3: every key silent, like NAME's 2); dub-perf adds one branch in the same place for the DUB page's keys. Both are one-line hunks in one function: keep both (dub-perf's branch is for a HOME page, GRID's is a SEQ page, so they are never live together). The other files they share are `ui_input.c` (this branch adds one `else if (mgrid_on())` in the key dispatch, the OCT- cancel and the includes; dub-perf adds the HOME DUB view's keys) and `ui_name.c keys_mode` (this branch adds `mgrid_stage_step() ? 3u`; dub-perf does not touch it). Everything else is new files: `ui_mgrid.c`, `ui_mgrid_edit.c`, `tests/mgrid_test.c`.

**Size, measured** (CI's build, same sources with `OC_MGRID=0` and `=1`, `nm -S` by symbol; the estimate was 1.5 to 2.5 KB of flash and under 16 B of RAM):
- **Flash: +4,092 B** (image 546,560 B with the switch off, **550,652 B** on; the text section 541,632 to 545,724; `text.bin` shows +4,096 because it is padded to 4 KB). **1.6 times the top of the estimate.** Where: `draw_graph` +1,090 (the tiles, the strip, the pill and frame, inlined), `ui_input` +1,078 (the stage logic, the cancel, the lights, inlined), `draw_columns` +840 (the four dials: they repeat NOTES' pitch and length text instead of sharing it), `fm1_main` +332 (inlining), `graph_sig` +154, `steppg_knob` +146 (the knob map), `seq_entry` +110, the small functions `mgrid_on` +80, `mgrid_stage_step` +52, `mgrid_track` +44, `page_visible` +32, `mgrid_page` +30. The estimate counted the new files' logic but not the dials or the inlining of the page into the three big functions.
- **RAM: +16 B** (.data+.bss 94,568 to 94,584 B of 98,304): the stage is 4 bytes of state, which the compiler keeps as four words.
- **Room:** free against `BODY_MAX` (573,440 B) is now 22,788 B, 14,596 B above the 8 KB reserve (it was 26,880 with the switch off). Easy trims if it is wanted: share the dials' text with NOTES (about 400 B) and keep the page's helpers out of line (about 500 B); not done, no behavior would change.

**Tests** (`tests/mgrid_test.c`, 69 checks, in the suite twice: with the minimum switches and with the shipping set; mutation script `tests/mgrid_mutations.sh`, 13 mutants, each caught by the check that names its rule; the new screens in `tests/ui_render.c`, 8 scenes in every palette, lint clean, MONO gray): see the status line below for the suite. The ten "Done when" items, each by name in the test's lines:
1. DW1: steps 1, 5, 9 and 13 take four notes without visiting the steps between (the cursor was only ever on those four).
2. DW2: black keys 1 to 4 reach the four pages; all 64 steps of a 64-step phrase filled from the GRID alone; a page and a step past LEN are inert.
3. DW3: the cursor does not move by itself: a key, a chord, a MIDI note, a MIDI chord, the cancel.
4. DW4: picking every step and every page changes no step and no recorded note, byte for byte.
5. DW5: a replaced chord is one undo; a deleted note, a deleted step and a replaced recorded step come back, byte for byte; OCT- in NOTE undoes the entry, and a cancel that changed nothing takes nothing back.
6. DW6: a step with a three-step length, velocity, accent, slide, ratchet, chance, condition and nudge keeps all of them through a replace and a pitch turn; ENV is not a fine-edit modifier here.
7. DW7: a project saved from the grid loads the same (steps byte for byte, events field for field); the stage is not saved; the record size and `FW_API_ID` are unchanged.
8. DW8: DRUM-X's grid is as it was (its keys, the cursor, `grid_on`), GRID is not offered on it, and every DRUM, DRUM-X and HOME picture is byte for byte equal to ui-fix2's.
9. DW9: the transport keeps running through picks, entries, a cancel, deletes and page keys; the edited step sounds on the next pass.
10. DW10: recorded timing untouched by picking and paging; a frozen phrase refuses entry, EDIT and the knobs; live recording armed and playing with GRID open records exactly what NOTES records for the same keys (byte for byte).

**One thing to check on the unit:** the keyboard changes from STEP to NOTE when the picking key goes up. The host cannot tell how it feels to roll from one key to the next; a note played with the picking key still down is another pick (by the owner's rule). Also: the stage is set in the UI pass and the audio interrupt reads it on its next block, so a key struck in the same few milliseconds as the change of stage may sound or not; the host cannot show that either.

**Suite:** the full host suite (`ci_step.py test --parallel 4`, `TEST_TIMEOUT` 900, through the lock, the build set on) passed on `c7188b9` in 2,867 s, with `tests/mgrid_test.c` (69 checks) built twice, the render test with the grid's screens (0 lint findings, MONO gray) and the module interface's identifier check; the commits after it are this note. The mutation script was run on the final code: 13 of 13 mutants caught. Size is as measured above on `baa73bb`; the REC-held guard added after it (`ui_clear.c`, four lines) is not measured separately.

## r3-format2 (Release 3 step 7, steps 1 and 2 as far as built; from linux-hw 5b8537f, November 4 to 11, 2026): For Session A

**Status: host only, nothing flashed.** Branch `r3-format2` on origin, head `847b0657`. **Host suite: SAN=1, every job, passed on `847b0657`** (`ALL HOST TESTS PASSED in 7319 s`, 3 jobs, `tools/suite_lock.sh` slot 1). The runs before it, and what they found: the first (on `6660eae0`, 7,375 s) failed ~40 jobs and the second (on `e9437656`, 22,464 s, most of it queued behind other sessions' suites) failed 4, **all of them sanitizer findings of code already on linux-hw 5b8537f, none in a file this branch touches** (signed left shifts, int32 products that wrap, LeakSanitizer, `app_layout.c`'s null-pointer offsets, and `fx_insert_diff.sh`'s pinned OLD tree running under the sanitizers). They were taken **unchanged from linux-hw by cherry-pick**: `8a8e347`, `542914bf`, `58c628ab` (now `e9437656`, `a11cf1f1`, `847b0657`). Nothing of my own was added to them; when this branch is merged they are already in linux-hw and drop out.

**Built and proved (host):**
- **Step 1, the format core** (`oc_pj4.c`, behind `OC_PJ4`, 0 by default; PROJECT_FORMAT_R3.md section 18). Its working memory is the caller's for one operation (`pj4_ws_t`, about 9.6 KB: the audio arena with the transport stopped, or the boot's buffer) and **nothing of it is resident but a 4 B pointer**: the 7,677 B in `.pool` of the first cut is gone (it had no reason to stay: every entry point is one operation).
- **The cut series with indeterminate bits** (`tests/pj4_test.c`): the byte being programmed at the cut keeps each bit it was programming with a seeded random choice (seed 20261104; `PJ4_SEED=n` repeats a run, a failing point prints its own). Over the **save** (63 call points, 1,918 byte points), the **conversion** (60, 103) and the **restore** (47, 77), plus the earlier series (a cut at every erase and program, after every 61st and 97th byte, a cut before an erase, a failing call): the slot reads entirely old or entirely new, no orphan, the next save works.
- **Control notes** (PROJECT_FORMAT_R3.md 8.2b; `6660eae0`): KICK, HOLD, DIVE and RISE record as flagged notes (`RN_CTL`, notes 124 to 127) through dub-perf's key path while the track records (`dubk_ctl_rec` in `dub_keys.h`, also from MIDI CC 18 to 21) and play back through `dub_rec[track][which]`, which `fx_insert.c` ORs into the trigger bytes the DUB kernel is handed. The pool writes phrase **version 3** for a phrase that holds one (`OCP_VER_CTL`) and refuses the flag in version 2; the loader accepts version 3. Tests: `ctl_play_test.c`, `pool_ctl_test.c`, `ctl_flag_test.c`.

**What Session A must review (interrupt side, `6660eae0`):** (a) `recording.c`'s timed-note player: a flagged note never reaches an engine (`recording_play` and the release path divert it to `recording_cref` and `dub_rec`; the gate of a strike is at least one block); (b) `dub_rec` is a plain byte array written by the player and read by `fx_insert.c` when the block's context is built (`c.trig[j] |= 127` for j 2 to 5): no lock, single writer, byte-wide, as `fx_trig` is; (c) `recording_cref` (4 x 4 x 2 B, `.pool`) and `recording_last` (2 B) are the only new RAM; (d) the sliding and legato paths skip a control note (`recording_slide_in`, the legato scan): check they cannot mistake one for a held engine note.

**Not built (steps 2 and 3 stay open), and why each is its own piece of work:**
1. **The loader splitting the sections** (OJ3B whole until now). It means `ocj_read` / `ocj_write` (oc_project.c) and the FBKK round trip rebuilt on `GLOB`, `TRKS`, `NATV`, `LIST` ... with the pool's phrases read from the project's own chunks; the stored layout is fixed-offset (`proj_pack`), so the split is byte ranges with the hash recomputed, but the readers of `proj_pid`, `bank_pid`, `ocj.names` and the pool (`ocp_*`) all change with it.
2. **The freeze table (`FRZR`)**, **the editor** (the 13,212 B wire record, the pool commands to 255, the INFO reply paged and the format version in it), **backup** (records by (type, id): `editor_backup.c`, `web/fm1backup.js`) and **the installer's warning and backup gate** (`web/make_site.py`, `web/app/lib/*`, their tests): firmware and web together so the page and the unit never disagree (11.3).
3. **Flash cost: not measurable yet.** `OC_PJ4=1` builds an image of 560,896 B against 560,912 B with it off, and the same RAM (.data+.bss 95,752 B, pool 181,152 B): **nothing calls the core, so the compiler drops it**. The figure PROJECT_FORMAT_R3.md 11.2b needs (about 11,400 B for the revision) exists only once the loader calls it.
4. **Step 3, scenes v2 with the short fade and Performance Mode**: both touch the audio interrupt (the fade's lead and commit, the slotless staged phrase); the design (sections 6 and 7) waits on your review and timing before it is cut, as the knob macros did.

**Session A's review (Round 23, November 11, 2026; merged into linux-hw with control-note RECORDING switched off).** An independent review of the interrupt side (a to d above) found no memory-safety problem and confirmed: a flagged note never reaches an engine in the player; `dub_rec` is written and read inside the audio interrupt in sequence (main-loop writers under `fm1_irq_off`); the slide and legato scans skip a flagged note; the flag is set only by `dubk_ctl_rec`, pitch code 63 and version 3 phrases; `oc_pj4.c` is out of the default build. New RAM is `recording_cref`, `recording_last` **and `dub_rec` (16 B)**. Fixed at the merge: `recording_reindex` now clears `recording_cref` and `dub_rec` before it counts again (an undo, note tool, pattern copy or pool load on another track under a sounding HOLD counted it twice: the trigger stayed on until power-off). **For Session C, open, and the reason `OC_CTL_REC` is 0 (the keys and CC 18 to 21 sound and are not recorded, as before; stored control notes play):**
1. `dubk_ctl_rec` records through `rec_note`, which writes the step grid as for a played note (seq.c): a mono track's step is wiped, a full chord's fourth note overwritten, the step's velocity 127 and chance reset, a held HOLD ties over manual steps; on a drum track the step's accent is set.
2. No check that the track is a DUB track: CC 18 to 21 on any selected, recording track recorded control notes (a behaviour change for projects without them).
3. `seq_notes.c` `notes_preview` auditions a selected control note on the engine (note 124 to 127); on a drum track LEN drops the flag and PITCH makes `recording_valid` fail, so the pool refuses the phrase.
4. A release is skipped when the selection changed while the key was held (the note stays open until recording stops).
5. Notes: `recording_queue_add` failing on a retrigger leaves the count one high; the gate's floor is one sample, not one block (a note ending before a step boundary inside the block is released before `fx_ins_start` reads it); an ordinary MIDI note 124 to 127 at the same step and time replaces a control note's slot.
`tests/ctl_play_test.c` runs with `-DOC_CTL_REC=1`.

**Session C, Round 24 (November 12, 2026; branch `r24-ctlrec` from linux-hw `ccefa6cf`): the findings above are closed and control-note recording is ON (`OC_CTL_REC` 1 is the default in `core.h`; `OC_CTL_REC=0` still builds and is tested). Host only, nothing flashed.**

**The rule that closes 1: a control note owns no step cell.** It is a timed event of its bank and nothing else. `dubk_ctl_rec` (`dub_keys.h`) no longer calls `rec_note`: it rounds to the step as a note does (`rec_round`, `seq.c`, one copy for both) and calls `recording_note_x(.., ctl 1)` (`recording.c`), which flags the event before it is linked and writes and flags no step. So `recording.c` has two tests where it had one: `recording_plays` (the player, the release queue, the erase: in the bank, inside LEN, and for an ordinary note its recorded NOTE cell) and `recording_active` (every editor and every overview rebuild: the same, and never a control note). A control note takes no chance, condition, ratchet, flam, nudge or accent from the step under it (`recording_fire`, `recording_shape`, `recording_on`), its release is not tied to a watched cell (`recording_queue_add` / `_remove` / `_refresh`, the reindex), the load's reconcile keeps it without a cell and does not let it keep a cell flagged (`recording_reconcile_owner`), and record quantize moves it without a destination check or a step write (`recording_quantize`).

1. **Finding 1 (the step grid).** As above. After KICK, HOLD, DIVE and RISE over manual steps (a chord of four, a tie, an accent, a chance, a condition, a nudge) every step is byte for byte what it was, on a POLY, a MONO and a drum track, and no hold is tied (`t->rh_n` stays 0).
2. **Finding 2 (a DUB track only).** `dubk_track`: ECHO above 0, by the track's own value where motion moves ECHO (so KICK inside a recorded CUT still records). On any other track the keys and CC 18 to 21 write the live trigger as before and record nothing. CC 18 to 21 record on a DUB track whatever page shows (a controller, a foot switch); with the DUB page closed the keys are notes.
3. **Finding 3 (the editors). My call, the safer one: the labeled control rows of PROJECT_FORMAT_R3.md 8.2b are not built, so NO editor shows, picks, auditions or changes a control note** (NOTES, the tracker, GRID, the pads, the note tools' move, copy, quantize and bulk delete, the clip, the web editor's step write). They all go through `recording_active`; the places that walked the events directly are guarded (`note_tools.c` `note_rebuild_all`, `note_manual_length`, `notes_bulk_delete`; `ui_feel_edit.c`; `ui_one_auth.c` `start_convert_edit`, `events_drop_view`). What the screens show for a step that holds only a control note: **nothing** (the pictures of PHRASE, NOTES, TRACKER and GRID are pixel for pixel the ones without it). A control note leaves with: the take's undo, the erase (EDIT held on NOTES while recording: it takes the control notes of the steps it passes, and one sounding when it begins, as it does notes), a pattern clear; it travels with a pattern copy, a double, the pool and the project. **The owner cannot yet see or delete one KICK by itself**: that is the control rows, open.
4. **Finding 4 (the release).** The release is `recording_release_x(.., ctl 1)` whatever is selected or armed by then, and closes only the control note (an ordinary note 124 to 127 has its own release, and the reverse). CC 121 (reset all controllers) clears the triggers with no edge: it now closes the control notes being recorded too (`midi_control.c`).
5. **Finding 5.** (a) `recording_hit`: a release the store refuses takes the retrigger's count off again (the store's bound rules the case out on the unit; the check uses a host hook, `RECORDING_QUEUE_FULL_HOOK`). (b) `dub_struck[4]` (4 B, `dub_fields.h`): a control note that begins sets its bit, `fx_ins_start` takes and clears the bits once a block before anything else and ORs them into the triggers, so a strike whose gate ends inside its own block is handed to the kernel for that block (a stored KICK with a one-sample gate at a step boundary renders sample for sample as one with a long gate; on the Round 23 source it rendered as no KICK). The stop clears the bits. (c) `recording_note_x` matches a slot by number AND flag: an ordinary note 124 to 127 at the same step and time is a second event.

**Found on the way.** (6) The pitch index (`recording_pitch_add`, `_prepare`) held control notes, so a step note 124 to 127 in a control note's cell was skipped as its duplicate and never reached the engine: the index leaves them out. (7) **A bound, `RN_CTL_MAX` 128 control notes a bank** (`core.h`; a 40-hit dub pass is 40): a controller sending CC 19 edges at MIDI rate filled all 1,024 notes of the unit in about 43 ms; past the bound the key sounds and is not recorded, and the screen says RECORDING FULL. (8) HOLD, DIVE or RISE held across the punch-in or through the count-in is recorded from the start, as a held note is (`dubk_ctl_held`); KICK is a strike and is not. (9) `pattern_used` (`ui_patterns.c`) counts a pattern that holds only control notes as used (a copy onto it asks). (10) The DUB page's help line said `BLACK: RECORDED` with the switch off too; it now follows the switch. `recording_last` is gone (2 B back).

**Tests.** `tests/ctl_rec_test.c` (new, through the DUB module's host unit, sanitizers; in the suite with the switch on and off): every finding above with a check that **fails on the Round 23 source built with `-DOC_CTL_REC=1`** (43 of its 93 checks fail there; for 5a the Round 23 source needs the hook line), then undo and redo, the erase, copy, double, clear, the pool, a project, the note tools, record quantize, punch-in, count-in, latch, the bound, and with the switch off: nothing recorded, stored control notes play and reach no engine. `tests/ctl_play_test.c` now records on a DUB track and takes the default.

**Size (`ci_step.py build`, against linux-hw `ccefa6cf` built the same way).** Image 525,816 -> **526,956 B (+1,140 B)**; .data+.bss 95,880 -> 95,896 B (+16 B); the audio code in SRAM 273 -> 279 functions, so the audio working arena is 80,800 -> **79,904 B (-896 B)** (minimum 49,152); pool unchanged (181,152 B). With `OC_CTL_REC=0` the same tree builds to 526,356 B (+540 B; arena 80,480 B).

**Run on the final tree (each compiled as the suite compiles it, sanitizers on; the full suite was NOT run).** `ctl_rec_test` (93 checks) and `ctl_rec_off`, `ctl_play_test`, `pool_ctl_test`, `control_notes_test`, `ctl_flag_test`, `recording_test`, `recording_undo_test`, `recording_scheduler_test` with its countdown and reference builds, `recording_props_test`, `recording_index_test`, `recording_queue_test`, `capture_alloc_test` and `capture_alloc_index`, `dub_perf_test`, `performance_tools_test`, `seq_notes_test` and `seq_notes_legacy`, `note_tools_test`, `seq_edit_test`, `one_auth_test`, `drum_editor_test`, `mgrid_ship`, `midi_control_test`, `midi_timing_test`, `pattern_test`, `oc_pool_test`, `oc_project_test`, `ui_test`: all passed. `tests/fx_insert_diff.sh`: OLD == NEW sample for sample, NEW == `tests/fx_insert_golden.txt`. `regress`: 430 golden renders, 0 changed. The web app's tests (165) and the guide's check passed.

**Not done, not verified.** Nothing was run on the unit. The full host suite was not run on this branch. The control rows (seeing and deleting one control note). The web editor reads a control note as a note 124 to 127 with pitch 39 (`web/app/lib/codec.js`); it writes the same code back, but what it shows was not looked at. A Round 23 Nightly loading a project saved by this build drops control notes that sit on a step with no recorded cell (its reconcile takes them for orphans): nothing else changes.
