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

## Release 3 format revision (branch r3-format, from ui-fix 7e7bf40; October 31, 2026)

The build of PROJECT_FORMAT_R3.md 0.6, host only, behind **OC_R3_FORMAT** (core.h, default 0; not turned on in
tools/orangecat_engines.sh: the build set is Session A's and the switch's default is off, so a CI image of this branch is
ui-fix's image to the byte). The order of work is the task's: (1) the format core, (2) wide parameter ids and LFO 2,
(3) the conversion, (4) power-cut safety, (5) conversion on real data, (6) the editor, backup and installer, (7) scenes v2
and Performance Mode. Each part: a design note here where the document leaves a choice, its independent review, the
code, host tests, the full suite with CI's settings, the budget table, a review of the diff, a push. The flash budget
below counts SHAPES-out as the document's 23,236 B until Session A measures it, marked unmeasured.

### Part 1: the format core (design note, before its review)

**What the document fixes and this note does not revisit:** 255 phrases a project (ids 1 to 255, 0 none); packed chunks of
type OCS_POOL with ids `0x8000 | slot << 9 | gen << 8 | chunk`, the project record of type OCS_PROJECT with id `0x8000 |
slot`, the generation bit as the save protocol (changed chunks under the other generation, the record last, the sweep
after), the directory with a CRC-32 and a reserved u16 a phrase, OCJ4 as tagged sections with unknown ones kept, HEAD's
uid, revision counter, profile name and id, REND, TSTA and ROUT held, SMPR by reference, OCP_VER 2 phrases, PROJECT FULL
refusing a save whole. Everything below is a choice the document leaves, with the safer reading taken.

**1. The FBKK staging stays the RAM interchange; OCJ4 is the flash form.** project.c's load and save paths, the editor's
project commands and the backup's ids 0 and 2 to 5 all speak the 27,904 B FBKK (project.c `proj_wire_u`), and today
oc_project.c converts it to and from the OCJ3 record and the pool. Part 1 keeps exactly that shape: `ocj_write(slot, raw)`
turns the staging into an OCJ4 record and the slot's chunks, `ocj_read(slot, raw)` turns them back. The FBKK's project part
keeps its FUN23 layout in Part 1 (Part 2 widens it with the parameter ids). So nothing above oc_project.c learns a new
format in Part 1, and the parts after it change one layer at a time. The fields OCJ4 carries that no FBKK has (the uid, the
revision, the profile, the stable track ids, the instrument references, the voice limits, SMPR) live in one small RAM
record, `ocj4_meta`, that belongs to the music loaded (switched with it, section 3 below), made fresh for a new project at
its first save and written back by every save.

**2. The record: plain sections, no run-length coding; PHRD sparse.** OCJ3 is run-length coded because its FUN23 part
is mostly zeros (1,024 notes' directory, every bank's steps). OCJ4 holds none of that (the phrases are in chunks), so it is
written plain: `magic "OCJ4" u32, version u8, hdr_len u8, rsv u16, body_len u32`, then sections `tag u32, len u32,
bytes`. Plain sections are seekable, which is what keeping foreign bytes needs (section 4). The record's size on the
owner's store is estimated at 6 to 7 KB (2 sectors): GLOB 0.15, TRKS 1.0, NATV up to 1.9, LIST 2.3, SCNS 0.1, PHRD 0.3,
FRZR 0.1 to 2.2, HEAD, MCHN, SMPR, PERF 0.5. **PHRD is sparse**: `u8 count` then one entry per phrase in use (`id u8,
chunk u8, crc u32, rsv u16`: 8 B), then the 32 B of generation bits, so 40 phrases cost 353 B, not 1,785, and an empty
project nothing. The document's 1,785 B figure was for a dense table; the sparse one holds the same fields. Within a
section, every table (`TRKS`, `SMPR`, `FRZR`, `NATV`, and `REND`, `TSTA`, `ROUT` when a later release fills them) is `u16
count, then count x (u16 elen, bytes)` as 8.4 asks.

The sections Part 1 writes, and what each holds beyond the document's table: **HEAD** name 12, parts u8, sel u8, seed
u32, uid u32, revision u32, profile name 16, profile id u32, next stable id u32, phys u8, fm6_fn_ok u8 (the FUN23 head's
bytes that are not a track's). **GLOB** `g[G_COUNT]` as i16 (152 B). **TRKS** an entry a track: stable id u32, engine
slot u8 (`eng_req`), module version u32, module id u32 (0), voice limit u8 (0), preset u8, mpc octave i8, user slot u16,
active bank u8, Prophet origin u8, the eight banks' phrase ids (u8 each: today's `ocj_head_t.pid`), then `count u16` and
`count` values as i16 (124 in Part 1; i16 so that nothing is clamped to the FUN23 byte's -64..127 later). **NATV** a
native block as an entry: track u8, kind u8 (1 FM6 packed + its functions, 2 CZ-1, 3 Prophet, 4 DRUM, 5 DRUM-X kit), the
bytes; **only blocks that differ from their kind's init patch are written**, whatever engine the track is on. The
document says "only the patch of the track's engine"; FUN23 keeps every track's four blocks, so a track that goes DRUM-X
to MACRO and back after a save keeps its kit today, and this reading keeps it at no cost for a default block (the
owner's unit has FM6, CZ-1, the Prophet and DRUM built out, so most blocks are their defaults). **MCHN** Melodee's own
chain (`chain_config_t` 36 B and `chain_patterns` 64 B), which the FBKK keeps at BANK_CHAIN_OFF and nothing in the
document names. **LIST** `ocl_lists_t` without its scenes (chains, song, tchain, cur, row, mode, launch, groove, gamt:
2,326 B). **SCNS** version 1: `u8 ver = 1, u8 count = 8`, 8 x 4 x 2 B as today, then `u16 used` (the `scene_used` mask;
the document's version 1 line lists the scenes alone, and the mask is state a stored scene cannot be told from). **PERF**
length 0 in Part 1 (today's PERFORM keys; Part 7 fills it). **PHRD** as above. **FRZR** (section 6). **SMPR** (section 7).
**REND, TSTA, ROUT, FXRC** written with length 0 by a project that has none, and carried (section 4) when a record holds
them. **FXRC is deferred whole**: its entries are keyed by MODULE_FORMAT_R3.md's stable ids and `fw_pid_t.legacy`
slots, which are not built; DUB TAPE's values stay in `p[]` (ids 45 to 47 and the JRES ids) and travel in TRKS, as they
do today. When the module format lands, FXRC is written from those slots and the ids; no record changes shape for it.

**3. The live pool belongs to one slot; unsaved music has a scratch slot.** Today oc_pool.c's tables (`ocp.len[]`,
`ocp.hash[]`) describe one shared pool. Under the switch they describe **the phrase set of one slot, `ocp.slot`**: its
directory (each phrase's chunk, CRC and LEN) and, a chunk, its **live** and **committed** generation bits. A project load
switches the live pool to its slot (`ocp_switch`: the record's PHRD read into the tables); a new project switches it to
**slot 4, the scratch**, which has no record. The record ids have room for it: `slot << 9` with slot 0 to 4 uses bits 9 to
11 (the document's layout leaves bits 11 to 14 free). Scratch chunks are what a project with no slot keeps to flash when a
bank must be reused (`ocl_keep` on a go while stopped, as today); they are **erased at every boot and by the sweep when
the music gets a slot**. So unsaved phrases of a new project die with the power, where today they survived in the shared
pool unnamed; the autosave (a free slot within 5 s stopped) and NOT SAVED: HOLD SAVE are the cover, and this is the price
of project-owned phrases, written down. Reading a slot that is not live (`proj_fetch` at boot for the slot list, the
backup's LIST of four slots while music plays) uses a **transient directory** read from that slot's record (`ocj_dir`, 287
B in .pool), never the live tables, so the music loaded is not disturbed. A **save to another slot copies**: every live
phrase is written into the destination's chunks (chunk to chunk, no unpack), then the record; the source slot is left
whole (the scratch's chunks swept). Today a save-as shares the phrases between the two slots, which is the thing the
revision removes; the cost is one write of every phrase on a save-as, as a first save writes every bank today.
`ocj_refs` (how many other slots name a phrase) is 0 under the switch and the sharing paths of `ocj_write` and
`ocl_free_pat` go with it; `FRZ_SLOT_NAMES(s, i)` becomes "s is the live slot and the live project uses i".

**4. Foreign bytes are streamed from the source record at save time, never held in RAM.** Unknown tags, REND, TSTA, ROUT
and FXRC content, trailing bytes after a known section's fields, trailing bytes after a table entry's fields and entries
past the count a writer knows all have to survive a save. The record is streamed to flash from a source (`ocs_src_t`), so
the writer **opens the record the music was loaded from** (`proj_cur`'s slot, or none for a new project) and, for each
section it writes, emits its own fields and then the old section's bytes past them (for a table: per entry, and whole
entries past its own count); after its own sections it copies every section whose tag it does not know, in the old
record's order. A save into the same slot reads the old copy while the new one is written (the store keeps the current
copy's sectors out of a new copy's way: `ocs_stream_begin`). A save-as carries the *source* slot's foreign bytes, not the
destination's old record: they are the project's, and a project overwritten is gone. The old record's section index
(tag, offset, length; 24 entries) is read into 288 B of .pool at the start of a save. No RLE anywhere is what makes this
seekable (section 2).

**5. The chunks, and when a keep flips a generation.** A chunk record is `u8 n, then n x (u8 id, u16 len)`, then the
phrases' bytes in that order: a table at the head instead of the document's inline prefixes (the same fields; a load
seeks to a phrase without walking the bytes). A chunk holds either several phrases of at most one part (3,808 B) together
or one phrase of any size (up to OCP_MAX, 3 parts). **Placing a kept phrase P of L bytes:** its chunk C is rewritten in
place when C holds P alone or C's other live phrases plus L fit a part; else P goes to the lowest-numbered chunk whose live
phrases plus L fit (first fit), else to a free chunk number (1 to 255, so a phrase can always have a chunk of its own),
and **C is not rewritten: its old bytes for P go stale** and are dropped when C is next written (the writer emits only the
phrases whose directory entry names that chunk). A chunk's "live phrases" are those whose directory entry names it; its
fullness is read from its head table when needed (no byte lengths in RAM). **The generation rule for a chunk C:** when
C's live generation equals its committed one (the slot's record names this copy), a write flips the live bit and writes
under the other id; when they already differ (kept since the last save, not yet committed), the write goes under the live
id again, where the store's own copy-on-write keeps the previous copy until the new one commits. So a copy the record names
is never written to until the record moves off it, which is the invariant the document's power-cut claim rests on
(section 3.1: "the old record still reads the old chunks, untouched"), and a second keep before a save does not undo it.
The record write names the live bits; after its commit the committed bits take the live values and **the sweep** erases
every OCS_POOL record of the slot (either generation, any chunk) the record does not name. The sweep also runs at boot
for every slot, and for the scratch (everything). A keep of a phrase a bank emptied clears its directory entry; a chunk
with no live phrase is simply not named and the sweep takes it after the next commit. **A failed save does not sweep:**
the live-generation chunks are the music's kept state and the banks' signatures already say "kept", so erasing them would
lose edits the player sees as saved to the pool; the next save retries the record alone. The boot sweep does erase them
(the music in RAM is gone; the committed record is the truth), which is the document's "entirely old or entirely new".

**6. Room: a keep leaves room for the record.** The document refuses a save that would leave less than a 3 sector
reserve. Under the switch the keeps are what fill the store (a save writes only the record after `ocl_keep_all`), so
the rule is put on the keep: **a chunk write is refused (rc 4, the message PROJECT FULL) unless, after it, at least
OCJ4_RESERVE = 4 free sectors remain** (a record of up to 4 parts, the worst case of section 2); and a save checks that
its record fits before it writes a byte, refusing whole with PROJECT FULL and the KB short. A keep refused leaves the bank
as it is (unkept, its edits in RAM, the bank's signature untouched), as rc 4 does today.

**7. FRZR, SMPR and the metadata, as far as Part 1 takes them.** **FRZR** is the project's frozen-phrase table: an entry
per frozen phrase, `u16 elen, u8 id, frzp_t (32 B as it stands, dir and want included)`. The document drops `dir` as a
cache; `dir` and `want` differ while a rename is pending, and both are indices into the unit's folder table, which is
fine while a project cannot leave the unit (the card library is deferred); the later release that moves projects
recomputes them, and nothing is lost meanwhile. The live table stays `frzp[64]` indexed by phrase id in Part 1: a project
load replaces it with the record's FRZR (entries whose id is above 64 are kept in the record and not loaded), a save writes
it back, and freezing a phrase above 64 is refused with a message until the freeze code is keyed by id (six index sites in
card_freeze.c, usb_files.c, card_browse.c; a later part, written here when done). OCS_FREEZE keeps the folders (`frzd`)
and a mirror of the live table, as today. **SMPR**: one entry for the internal sample slot when it is FULL and a track's
engine is a sample engine (kind 1, the slot header's name, data_len and crc, the tracks' mask, the zones' mask), none
otherwise; card-file references (kind 2) are read and kept, never written in Part 1 (nothing on the unit plays a card
sample by reference today). At load each reference is resolved against the slot's header and the result (resolved,
changed, missing) is kept a track; a changed or missing one says `SAMPLE CHANGED T3` / `SAMPLE MISSING T3` in a toast at
the load (the header chip, RELINK and the "silent" rule are interface and audio work: Session A's rule for muting a track
whose sample is gone is asked for in "Waiting on Session A"). **The metadata:** the uid and the seed are made at the first
save from `fnv(fm1_ms, the staging's bytes, the store's cursor and the slot's record seq)`; the track ids are
`hash(seed, "trk", k)`; the revision counter starts at 1 and every save adds one; a restore of a record keeps its bytes.

**8. OCP_VER 2, and the phrase bytes.** A version 2 phrase is `0 ver = 2, 1 flags (bit 0 automation on), 2..3 np u16,
4..11 LEN DIV SWING GATE, 12..19 the step mask`, the used steps at 11 B, then `u8 count` and the motion events at **5 B
(step u8, param u16, value i16)**, then `u16 count` and the timed notes at 10 B. Version 1 is read as today. The wide ids
are in the format from Part 1, so Part 2 widens the runtime (motion_event_t, the asserts, the editor protocol) and never
the record. Macro events on ids 33 to 36 are dropped when a version 1 phrase is read and kept for version 2 (8.1); the
packer writes the dials unbaked only once Part 2 lifts the FBKK's zeroing (in Part 1 the FBKK path still bakes, so a
version 2 phrase written by Part 1 carries no macro events).

**9. What Part 1 does not do, on purpose:** no interface (the PATTERNS grid shows 64; chain steps reach 255 through their
knobs' range; the project meter waits), no conversion (Part 3; an OCJ3 slot reads as OLD FORMAT under the switch as any
unreadable record does today), no change to the editor's arrange reply beyond keeping it building (INFO reports the
first 64 phrases under the switch, with a comment naming Part 6), no FXRC, no scenes v2. The boot path: `ocs_boot`, the
sample slot's claim, **the sweep**, `ocj_scan` (the slot list), then `project_boot`, which switches the live pool to the
project it loads.

**Cost (estimates, the document's lines):** chunks, directory, sweep and the pool's tables to 255: 3,200 B; the OCJ4
writer and reader: 3,500 B; the Gate 1 and card-ready fields: 700 B; SMPR: 400 of its 700 B; OCP_VER 2: 300 B. **About
8,100 B of flash with the switch on, 0 B with it off**; RAM about 2,400 B (`ocp`'s tables 1,600 B, the transient
directory 287 B, the section index 288 B, the metadata 450 B), against the document's 1,660 B for these lines (it did not
count the transient directory or the index). Both are measured below.

**Tests (host, tests/oc_r3_test.c on oc_project_test's harness, built with -DOC_R3_FORMAT=1; the existing store, pool and
project tests still run with the switch off):** a project with all 32 banks saves and loads back the same, after a reboot
too, with its sections present; a save that changes nothing rewrites the record only, one bank changed rewrites its chunk
under the other generation and the record, and the old generation is gone after; 255 phrases through chain steps and
keeps, all loading back, the 256th refused; a save-as copies and the two slots are independent (an edit in one leaves the
other's bytes equal); a new project's keeps go to the scratch, a save-as moves them, a boot sweeps a scratch; a record
carrying REND, TSTA, ROUT and FXRC content, an unknown tag, trailing bytes in HEAD and in a TRKS entry and a fifth TRKS
entry loads and saves byte for byte, and a save-as carries them; the uid survives saves and the revision counts them; FRZR
and SMPR round trip and the three resolve states show; a version 1 chunk reads and the next keep writes version 2 with the
macro events dropped; a chunk's stale bytes are dropped at its next rewrite and a phrase that outgrows its chunk moves; a
power cut after every program call and after every erase of a save and of a save-as leaves the slot reading entirely old
or entirely new (Part 4 extends this to the conversion and the restore and to every byte); a full store refuses a save
whole and a keep keeps the reserve; the fuzzer (fuzz_oc_project) built with the switch on against a mutated OCJ4 store.

**The design review of Part 1** (an independent agent, October 31, 2026; it measured the code being replaced from the
release-2 ELF rather than estimating). Twenty-four findings; the generation rule for the live slot (1), the store's
tolerance of the ids and the reserve measure (5), reading the old copy while streaming the new (7), the FBKK's coverage
(13), the phrase-byte readers (19) and the boot's use of the tables (20) were confirmed OK. The rest, and what the build
does:

| # | Finding | Severity | What is done |
|---|---|---|---|
| 12 | `motion.on` (a track's automation on) lives in the FUN23 part and nowhere in OCJ4; the step-5 re-read would differ from the music and the autosave rewrite the record every 5 s | blocker | TRKS gains `flags u8` (bit 0 automation on) after the Prophet origin |
| 2, 3, 15 | Save-as has no generation rule for the destination's chunks, leaves the source's uncommitted chunks as orphans until boot, and is not covered by the keep reserve | should fix | the destination's chunk n is written under the opposite of **its** committed bit (read from its record through the transient directory; unnamed numbers either); after the commit the live pool is the destination with live = committed = the bits written, and **the source slot's unnamed chunks are swept at once** (they are no longer the music's kept state); before a byte is written a save-as checks `free >= parts(record) + sum of parts(every live chunk)`. The exact keep inequality is coded as the review states it: `free - n_chunk + (same id ? n_old : 0) >= OCJ4_RESERVE` before the write; a save checks `free >= parts(record)` from the writer's own length |
| 4 | A new project's first save writes every phrase twice (keeps to the scratch, then the copy) | minor | accepted and written down: the first save of a new project costs one extra write of its phrases; the keeps' target is not changed mid-save (two write paths would be two to prove) |
| 5, 20 | A sweep coded as `ocs_exists` per (slot, chunk, generation) would CRC every live chunk's payload 2,550 times; `ocs_delete` does not feed the watchdog | OK as designed | the sweep is one pass over `ocs_hdr[]` (OCS_POOL, bit 15, the slot's bits, named or not), `OC_FEED()` per erase, `BOOT_WORK_PROGRESS` at boot |
| 6 | Reading a chunk's fullness from its head table on demand is up to 255 record opens per keep, each a header scan and a payload CRC | should fix | `uint16_t used[256]` in .pool (512 B): each chunk's live bytes, filled at `ocp_switch` from the named chunks' head tables, kept by every write and every directory change |
| 8 | `proj_cur` is not a safe key for the record the music came from: the backup's restore of records 64 to 67 leaves it naming a slot whose record is now another project's (editor_backup.c 271 to 283) | should fix | foreign bytes are carried only when the source record's HEAD uid equals `ocj4_meta`'s uid (and its revision is not newer); otherwise none. Source, not destination, confirmed the right rule |
| 9 | `ocl_pool_copy` / `ocp_copy`, `ocl_pool_clear`, `ocl_clear_unused` and `ocp_save` do chunk arithmetic (`id / OCP_PER`, `want[OCP_N]`) | should fix | all five move to the placement API (a phrase placed, a phrase cleared, a phrase copied chunk to chunk) |
| 10 | editor_arrange.c's static assert (23 + OCL_NPOOL x 7 + 1 < 1,024) fails at 255 | should fix | `ED_AR_NPOOL` (64 under the switch, with the comment naming Part 6) separate from OCL_NPOOL |
| 11 | The backup's objects 64 to 83 name the old ids (a restore of size 0 deletes them: harmless); object 84 restores `bank_pid` with no signatures, which under the switch are the live slot's directory ids | minor | named here; Part 6 lists records by (type, id) and retires 84's bank map |
| 13 | NATV kind 5 (DRUM-X kit) has no FBKK source: "the project keeps no kit of its own" (project.c 1032) | OK | Part 1 never writes kind 5; the kind is reserved |
| 14 | Omitting a native block equal to its kind's init patch reconstructs it from the reader's init; a later firmware changing `FM6_INIT`, `cz_patch_init` or `p5_patch_init` would change an old project's sound silently | should fix | **the block of the track's own engine is always written**, whatever its bytes; a dormant block (another engine's) is written only when it differs from its kind's init. What sounds never depends on an init constant; a dormant block equal to the init is, after a later init change, what a fresh switch to that engine would give |
| 16 | `frz_table_load` runs after `project_boot` (main.c 254) and would wipe the loaded project's FRZR; `project_new_blank` is not a load | should fix | under the switch `frz_table_load` runs before `project_boot`, and a hook (`frz_project_hook`, as `frz_named_hook`) tells the card code "a project's table replaces yours" at a load and "clear" at a new project |
| 17 | Four per-project freeze tables break `frzd_used` (a folder used only by a non-live project looks free and is reused) and the take moves of a save-as copy (`frz_named` with rename 0 sets `want` for the live table, `frz_reconcile` moves the files, the source project's entries then name files that are gone) | should fix | **FRZR stores no unit-local index** (the document's rule after all): the section carries a small folder-name table (`u8 n, n x char[32]`: the names its entries use, 1 or 2 in practice) and each entry's `dir` and `want` are indices into that table; at a load they are mapped to the unit's `frzd` through `frzd_get`. `frzd_used` under the switch also asks each slot's record (a transient read of its FRZR's folder names). **A save-as copy moves no takes** (rename 0 does nothing under the switch: the copy's entries keep their folder, both projects name the same file, a later freeze in the copy records into its own folder); a rename of the same project moves them as today. For the owner: this is Part 1's one change of card behaviour |
| 18 | After a load the hashes agree; OCS_FREEZE's mirror of the live table is redundant | minor | OCS_FREEZE keeps `frzd` and the mirror as today; under the switch the mirror is read at boot and replaced by the project's FRZR at the first load, never read again |
| 19 | `ocp_len_of` checks `h[0] != OCP_VER` | OK | reads versions 1 and 2 (np at 1 or 2..3); version 1's `h[3] == 0` gate stays as the layouts' tell |
| 21 | OLD FORMAT is `ocs_exists(OCS_PROJECT, slot)`; under the switch the reader looks for `0x8000 | slot`, so an OCJ3 slot could be taken as free by the autosave | should fix | `project_old` tests both ids: an OCJ3 record at id `slot` is OLD FORMAT and never free |
| 22 | Nine divergences from PROJECT_FORMAT_R3.md the owner must be told, beyond the note's own list | should fix (document) | listed under "For the owner (Part 1)" below and, at the end of Part 1, appended to PROJECT_FORMAT_R3.md as its 0.7 build notes |
| 23 | The estimate: the OCJ3 writer and reader are 1.5 KB of oc_project.c's 2,688; a 14-section writer and reader with entry lengths and the carry is 3.5 to 4.5 KB alone, the chunk layer 4 to 5 KB; forgot `used[]`, the head-table buffer, `ocp_chunk_src_t` at n entries, `want[]` at 256, the two-id check, the SMPR state, the feed | should fix | **re-estimated: 10,000 to 12,000 B of flash with the switch on, 2,600 to 3,000 B of RAM.** Every new symbol is under `#if OC_R3_FORMAT`, so the switch-off image is checked against 561,608 B |
| 24 | The harness cuts only after program calls (a failing erase is not a cut after one); the no-orphan assertion, the three-changed-chunks case, the fuzzer's unknown-section survival, an absent engine's reference, a profile change touching HEAD alone, "no field holds a slot or record id", an unknown SCNS version, the autosave guard, a save-as into a slot holding orphans, a 3-part keep against the reserve, OLD FORMAT not free, FRZR across the boot order | should fix | all added to Part 1's tests; the cut-after-erase counter and the no-orphan check go into the harness now, so that Part 4 extends rather than rewrites it |

**For the owner (Part 1), where the build departs from PROJECT_FORMAT_R3.md 0.6:** (a) a failed save does not sweep (3.1
said "straight after any failed save"; the live-generation chunks are the music's kept state, see section 5); (b) SCNS
version 1 carries the `used` mask after the scenes; (c) a tag `MCHN` (Melodee's own chain) joins the table; (d) HEAD
carries phys and fm6_fn_ok, TRKS the active bank, the eight banks' phrase ids, the Prophet origin and a flags byte; (e) a
record may take 4 sectors and the reserve is 4, not 3; (f) PROJECT FULL is a toast in Part 1, the header chip waits for the
interface part; (g) phrases above 64 cannot be frozen until the freeze code is keyed by id; (h) Part 1 still bakes the
macros (the dials are written 0) until Part 2 lifts the FBKK's zeroing; (i) the freeze table is per project, so a
save-as copy no longer moves takes and `frzd_used` reads every slot's record; (j) the chunk's head table, the sparse PHRD,
no RLE, NATV's dormant-block rule, FXRC deferred, the scratch slot, as the note said before the review.

### Part 2: parameter ids past 7 bits, and LFO 2 (design note, before its review)

**Where the ceiling binds, read from the code (October 31, 2026).** A parameter id leaves `p[]` in six places. (1) The
editor protocol: GET, SET, DESC and TRACK_PARAM carry the id as one SysEx byte, and `ed_b` masks every byte to 7 bits,
so INFO's `P_COUNT` byte cannot say 131 (editor.c 71, 994 to 1068, 1545). (2) `motion_event_t.param` is a byte; the
native engine ids (`MO_NATIVE` 112 and up) share that byte with the track's ids 112 to 123 already and are told apart by
the value (`motion_native_event`: value >= 256), so the byte's own ceiling is 255, not 127; the assert `P_COUNT < 128`
(motion.c 34) is the protocol's. (3) upreset.c: `np` a byte, `packed[144]` (room for 144 values), the assert `P_COUNT <
128` (73); `up_valid` names the legacy counts a CZ-1 native record may carry (199). (4) project.c: the FUN23 part stores
`np` as byte 66 (131 fits), asserts `P_COUNT == 124` ("a change: a new magic and a conversion", 158) and `proj_pack`
refuses `P_COUNT > 127` (323); seven more values a track move the chain and motion blocks by 28 B inside a part that has
the room (the FM6 patches start at `PROJ_FM6_OFF`, checked by `proj_pack`); `tmpl_t` holds `p[P_COUNT]`, so its size and
magic move too (1107, 1117). (5) The phrase record and the project record: Part 1 made `np` and the motion ids 16 bits
on the flash already. (6) The web: `web/editor.html` hard-codes `P_COUNT = 124, P_E0 = 116` (2422) and accepts only the
`(pcount, pe0)` pairs it knows (`knownLayout`, 3779); `web/app` reads both from INFO's bytes (lib/editor_proto.js 287)
and from `lib/layout.js`, generated from tests/app_layout.c by web/app/tools/gen_layout.mjs; the DUMP, TRACK_DUMP and
UP_GET replies carry `P_COUNT` values, counted by what INFO said.

**1. The runtime keeps a byte; the protocol and the records go wide.** `motion_event_t.param` stays `uint8_t` (ids 0
to 255; the native ids keep their value rule), so `motion_store_t`'s 260 B disk layout, the FBKK's bytes and `proj_pack`'s
`memcpy(&q->motion)` do not move; `motion_token` is already 16 bits. The asserts become `P_COUNT < 256u`. The document's
8.2a says "`motion_event_t.param` widens"; the ceiling that stopped LFO 2 is the protocol's 7 bits, and this reading lifts
it without touching the wire record the editor and the backup carry. The next ceiling (255 ids, 124 spare) is written
down here as the one after. On the flash the ids are 16 bits (Part 1), so no record changes shape again.

**2. The editor protocol (web/EDITOR_PROTOCOL.md is updated with it).** INFO's legacy `pcount` byte carries
`min(P_COUNT, 127)` and its `pe0` byte `P_E0` (123 fits); an old page reads `(127, 123)`, a pair `knownLayout` does not
have, and refuses the layout, which is what 8.2a asks for ("refused by the version and not misread"). A new tagged block
in INFO, `0x57 'W', version 1, pcount lo, pcount hi, pe0 lo, pe0 hi, project format` (the last byte: 2 before the
revision, 3 for a Release 3 unit) gives the true counts and **the project format version section 10 asks the installer
page to read** (Part 6 uses it). Ids past 127 travel on three new commands under the extension (command 0, 0.14.2's
rule: the reply is `0, n, ...`): **130 PARAM_W** `scope, id lo, id hi [, v14]` -> `scope, id lo, id hi, v14` (GET and SET
as commands 2 and 3 do); **131 DESC_W** `scope, id lo, id hi` -> as DESC's reply; **132 TRACK_PARAM_W** `track, id lo, id
hi [, v14]` -> `track, id lo, id hi, v14`. The 7-bit commands keep serving ids below 128 unchanged (an id of 127 on them
is an id, never a count). A TRACK_CHANGED push of an id past 127 goes as **133 PARAM_CHANGED_W** `track, id lo, id hi,
v14`; a page that did not ask for WATCH bit 1 gets no pushes, as today. DUMP, TRACK_DUMP and UP_GET carry `P_COUNT`
values as they do; a new page counts them by the tagged block. The commands exist in every OC_R3_FORMAT build (so the
pages can be written against them) and carry nothing new while `P_COUNT` is 124.

**3. The records, by count.** The FBKK's project part gets the magic **FUN24** (`PROJ_MAGIC`) with `np = P_COUNT`; a
FUN23 part (`np` 124) loads by count as FUN22's 122 does today (`params_by_count` puts the seven at their defaults: LFO 2
off), and `proj_motion_ids` moves the ids from the old P_E0 up as it does. `proj_pack` writes `np` as the byte it is and
drops the `> 127` refusal; the assert reads `P_COUNT == 131` under OC_LFO2 and `124` without. The template: **TPLN**
(`TMPL_MAGIC` under OC_LFO2) and `tmpl_param_count` maps a TPLM's length to 124 (one more case in its chain). User
presets: `np = P_COUNT`, the asserts `P_COUNT <= 144` (the record's room; 131 fits with the DRUM patch's eight bytes
after it: the `DXK_ROOM` assert holds) and `< 256`; `up_valid`'s CZ-1 count list gains 124. Motion: `motion_param`
accepts `P_L2RATE` and `P_L2AMT` (the continuous two, as LFO 1's RATE is accepted; WAVE SYNC TRIG POL DEST are not
lanes). The phrase record already carries wide ids; Part 1's `ocp_parse` drops its `param > 255` guard's comment and keeps
the check (a byte in RAM).

**4. The interface identifier.** tools/gen_fw_api_id.py hashes the text of named structures; it gains the parameter
enum (`enum { P_LEVEL .. P_COUNT }` in core.h, extracted between its first and last member) in `SOURCES` and a selftest
case. The identifier therefore moves at this part's commit, once, for every build of this branch: the modules read `P_E0
.. P_E7` at compile time and CI builds every `.ocm` with the firmware, so nothing is left behind; the `#if OC_LFO2` lines
inside the enum are part of the hashed text, so the identifier is one value for the branch, not one per switch setting.

**5. LFO 2 (OC_LFO2, default 0; it requires OC_R3_FORMAT=1: a `#error` otherwise), as the ui-fix 4 note has it with its
review's corrections.** Seven parameters appended before P_E0: `P_L2RATE` (RATE, F_LFOHZ, 0..127, default 60), `P_L2WAVE`
(SIN TRI SAW SQR S&H), `P_L2SYNC` (OFF 4BAR .. 1/32), `P_L2TRIG` (NOTE FREE), `P_L2POL` (BI UNI), `P_L2DST` (OFF PITCH CUT
SHP AMP PAN RATE VIB E1 .. E8: a table of its own, `N_L2DST`, mapped to the matrix's `MD_*` at the one accessor below),
`P_L2AMT` (-64..63, default 0). `P_COUNT` 124 to 131, `P_E0` 116 to 123. **The state** lives in mod.c, outside `track_t`
(the module interface identifier hashes `track_t`; the enum's text moves it anyway in this part, but the state is kept
out so that a later LFO change does not): `lfo2[NTRK]` (phase u32, value i32, S&H value u32, the cycle before's i16, a
flag "ticked this block") and its own synced-increment cache (`{cycle, inc}[NTRK]`: voice.c's `lfo_sync_cache` is
voice.c's; `div_samples` is forward-declared, as voice.c does). **The tick** runs once a block at the top of `mod_begin`
(both the reference and the cached path), **before** the early return and only while LFO 2 is used (DEST with AMT, or a
matrix slot whose source is LFO2); unused, its phase holds and no work is done. The track index is `t - trk` only when
`t` lies inside `trk[]` (the cached path's own address check); a foreign `track_t` (host tests build those) gets no LFO 2.
The waves are LFO 1's shapes (an own copy of `lfo_wave`'s cases on LFO 2's phase: `osc_sine`, `osc_tri`, the saw, the
square, S&H), the S&H value drawn at each cycle's end from LFO 2's own xorshift (never the shared `rng`: the golden
renders depend on it), UNI applied at the tick (`(v + 32768) >> 1`), the synced increment as `lfo_sync_inc` computes it
with `P_L2SYNC`. LFO 2 reads its value one block after LFO 1 reads its own (LFO 1 ticks in `track_render`, after
`mod_begin`): a one-block skew between the two, accepted and written down. **TRIG NOTE** in `mod_note` (voice.c calls it
on every note-on): the phase restarts when the track has no other voice holding its gate at that moment (the new voice may
already be gated when `mod_note` runs: "no other" is "at most one", the review's point); FREE never restarts.
**The matrix:** `MS_LFO2` appended to the sources (`N_MSRC` "LFO2", bipolar; with POL UNI it takes the unipolar path at
the four sites that give LFO 1 ENV's treatment: `mod_depth`, the per-voice add, the cached depth and voice loops).
**Its own destination is a fifth, implicit slot:** every loop of `mod_begin`, `mod_begin_reference`, `mod_route_prepare`
and `mod_route_key` reads slot k through one accessor (`k < 4`: `P_M1SRC + 3k` and its two; `k == 4`: SRC LFO2, the
`N_L2DST` map of `P_L2DST`, `P_L2AMT`); `NMSLOT` stays the four stored slots and `NMSLOT_ALL` is 5 for the arrays of
`mod` and `mod_route_t` (the review counted eight sites). DEPTH is not offered on the fifth slot (a matrix slot can route
LFO2 into DEPTH). The early-return scan counts the fifth slot active when `P_L2DST` and `P_L2AMT` are both nonzero.
**mix.c is Session A's:** its copy of `mod` for the batch is taken by `sizeof` (checked at the build; if it names
NMSLOT, this part stops at that line and reports). **Pages:** "LFO 2" (RATE WAVE DEST AMT) and "LFO 2 SYNC" (SYNC TRIG
POL) after LFO DEST in `PAGES` (params.c), `FAM_LFO`; the Mod sheet lists them (it reads `PAGES`), ui_designer.c's skip
list keeps its titles. **Cost:** off, one compare more a block; on, the tick and one slot's terms; tests/target_budget.txt
re-baselines `mod_begin` (158 B today) to the measured size; mod_test's cost check bounds the tick; **Session A times it
on the unit before any merge, as the knob macros.** Flash about 1,400 to 1,600 B (the review's figure), RAM about 140 B
of .bss; the wide-id plumbing about 500 B; the records' magics and counts about 200 B: **about 2,300 B with both switches
on, 0 B with them off**, measured below.

**6. The web.** `web/app`: lib/editor_proto.js reads the tagged block (`pcount`, `pe0`, `format`) and keeps the legacy
bytes as the fallback; lib/model.js already takes `info.pcount` / `info.pe0`; the wide commands in editor_proto.js
(`paramW`, `descW`, `trackParamW`); lib/layout.js regenerated from tests/app_layout.c (unchanged while OC_LFO2 is off in
the build set; a fixture built with `-DOC_LFO2=1`, `app_layout_lfo2`, checks the codec with `np` 131 and FUN24).
`web/editor.html`: the `(131, 123)` pair in `knownLayout`, the tagged block, the wide commands for ids past 127 (its
parameter table is built from DESC per id); `web/test_web.mjs` with it. The firmware's Nightly stays on `np` 124 until
Session A turns OC_LFO2 on, so the pages must work with both; the tests run both.

**7. Tests (host).** tests/lfo2_test.c on mod_test's harness: off is bit-identical (DEST OFF: `mod_begin` returns what it
did, the render equal); each wave's value at known phases; SYNC at 1/4 against `div_samples`; TRIG NOTE restarts, FREE
does not, a second note while one holds does not; UNI's range; the fifth slot alone on PITCH, CUT, PAN and E1; with a
matrix slot on LFO2 -> DEPTH; the cost check (cycles of `mod_begin` with and without); the one-block skew stated. A
store test (tests/oc_r3_test.c gains a part under OC_LFO2): a FUN23 record loads with LFO 2 off; a FUN24 round trip
keeps the seven; a user preset of `np` 124 loads and one of 131 keeps the seven; a TPLM template converts; a phrase with a
motion lane on `P_L2RATE` round-trips (id 124: a track id, told from a native byte by its value); the knob macros' ids
are untouched. The editor (tests/editor_test.c): INFO's legacy bytes `(127, 123)` and the tagged block; PARAM_W, DESC_W
and TRACK_PARAM_W on ids 124 to 130; a 7-bit SET of id 127 on a 131-count build is an id, not a count; a push of id 128
as PARAM_CHANGED_W. The web tests (web/app/test, web/test_web.mjs) as in 6. ui_render: the two pages rendered and linted
in the sheets build with OC_LFO2=1 (the Mod sheet's rows with them).
