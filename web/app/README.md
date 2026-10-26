# Orange Cat web app

One page to manage the unit: its modules, its sounds, its steps, its arrangement, its projects and backups. It talks to the FM-1 over
USB-MIDI SysEx (Chrome, Edge or Opera), with the same protocol code as the old editor. Firmware is still installed from the installer
page; the old editor (`web/editor.html`) stays until this page does everything it does.

Version 0.14.2 (October 26, 2026, Session D, branch `web-app`). Not on main, not released, not yet tried on the unit
(`HARDWARE_TEST.md` is the procedure).

## The tabs

| Tab | What it does | Commands |
|---|---|---|
| Unit | Connect; the firmware and what it can do; the asset pack (install, erase); the Engines table (Install, Replace, Remove, Use on a track); Install the set (the pack and the modules the site publishes with its firmware, or files picked here) | INFO, 115..119, 120..125, 126/127, the installer's `runSet` (`web/fm1oc.js`) |
| Sounds | Per track: the engine's picture, its four knobs, the SOUND sheet (the eight EDIT values), the preset browser with favorites, the library (save, load, rename, erase) | DUMP, DESC, SET, TRACK_PARAM, PRESET, FAV_SET, UP_* or LIBRARY (80) |
| Steps | The phrase's steps as a tracker (notes, time, velocity, the step's flags, chance, drum hits), edited in place; the recorded notes' own onset, length, velocity and bend | TRACK_STEP; backup object 0 |
| Arrange | The 64 phrases of the pool, the chains (drag a step to reorder), the song (drag a row), the scenes, each track's groove, LAUNCH; a list of what will change, then Write | ARRANGE (102, new in 0.14.2) |
| Project | Read the music now or a saved slot; save the music now to a slot and load a slot (the unit's own SAVE / LOAD); export and import project files; the complete backup and restore, and what a backup holds | backup objects (65..67), PROJECT (9) |
| Settings | The page's colors (the unit's palettes); the unit's preferences (screen palette, preset filter, click, count-in, preview, chord entry); the audio readout | UI_STATE, UI_SET, AUDIO_STATS (72) |

Open it with `?mock` to talk to a mocked unit (`lib/mock.js`); nothing then reaches a real FM-1.

## How it is built

No build step: an HTML page, a style sheet and ES modules.

| File | What |
|---|---|
| `index.html`, `app.css`, `app.js` | the page, its look (the unit's design tokens: `docs/design/README.md`), the tabs |
| `lib/session.js` | one connection and everything the tabs ask of it, in the order the old editor does it |
| `lib/codec.js` | the project record (FBKK, the FUN23 part) and the arrangement (the lists), read and written byte for byte |
| `lib/arrange.js` | command 102: read and write the arrangement |
| `lib/model.js` | Session C's names, knobs, colors and pictures (`docs/orangecat/UI_DESIGN.md`), and what the page guesses where it is not settled |
| `lib/scene_svg.js` | the engine pictures drawn from their scene bytes: a port of Session C's interpreter (`tests/ui_scene.h`, branch ui-design) |
| `lib/mock.js` | the mocked unit: the old editor's mock plus the Orange Cat commands it lacks |
| `lib/editor_proto.js`, `lib/editor_text.js` | GENERATED: the old editor's protocol section and texts, unchanged (`tools/gen_proto.mjs`) |
| `lib/layout.js`, `lib/mock_data.js` | GENERATED: the record's layout from the firmware's C (`tests/app_layout.c`) and the fixtures it writes (`tools/gen_layout.mjs`) |
| `lib/palette.js` | GENERATED: the unit's palettes (`tools/gen_palette.py` from `tools/gen_ui_palettes.py`) |
| `lib/scenes.js` | GENERATED: Session C's scene bytes (`tools/scene_dump.c`, `tools/gen_scenes.mjs`) |
| `test/run.mjs` | every test, against the mock; `test/shots.mjs` a look in a real browser (needs puppeteer; not run by CI) |

**Nothing is copied by hand.** The protocol code is the old editor's text (checked equal on every test run); the project record's
offsets, sizes and bit fields come from the firmware's macros and structs, with probes of the firmware's own packing and fixture
records the firmware wrote, which the codec must read and write back byte for byte (as must the older FUN19..21 project files in
`tests/fixtures`); the colors are the unit's generator's. A change in the firmware or the editor fails `test/run.mjs` until the
generated file is made again. `tests/run_tests.sh` builds `tests/app_layout.c`, writes `build/host/app/`, and runs `test/run.mjs`.

Regenerate:

    node web/app/tools/gen_proto.mjs                         # after a change to web/editor.html's protocol section or texts
    cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/app_layout tests/app_layout.c -lm
    mkdir -p build/host/app && build/host/app_layout json > build/host/app/layout.json && build/host/app_layout fixtures build/host/app
    node web/app/tools/gen_layout.mjs build/host/app         # after a change to the project record or the lists
    python3 web/app/tools/gen_palette.py                     # after a change to the unit's palettes

## How it is served

`tools/orangecat_site.py` (the pages job of `.github/workflows/build.yml`) puts it at `webapp/app/` beside `webapp/installer/` and
`webapp/editor/`: the files above but `tools/` and `test/`, with `web/fm1backup.js` and `web/fm1oc.js` beside the page (the page imports
them as `#fm1backup` and `#fm1oc`; the import map in `index.html` points at `../` in the repository, the site build points it at `./`),
and `files.json`, the list of what is there. `web/site_check.py --list` (the pages job's dry run) lists `webapp/app/index.html` and counts
the app as Standard; `web/mirror_site.py` copies the published app by `files.json` when a branch publishes a Nightly; the Nightly's own app
goes to `webapp/nightly-app/`. Locally: `cd web && python3 -m http.server 8765`, then `http://localhost:8765/app/` (Chrome allows MIDI
SysEx on localhost).

## What it cannot do yet (and why)

- **Per-note editing** of recorded notes: the unit's NOTES event editor has no command (command 108 reads them in test builds only).
  The page shows each note's values from the project record.
- **RATCH, FLAM, condition and NUDGE** of a step: not in STEP_GET / TRACK_STEP, and the project record keeps them only inside the unit.
- **Macro knobs** (UI_DESIGN.md's stars): need `P_MAC0..3` in the firmware. Shown grayed, not written.
- **ENV, MOD, FX, VOICE sheets**: the unit's; the page has SOUND.
- **A project file's arrangement** is not written to another unit (phrase numbers name that unit's pool). A complete backup carries
  the arrangement of the music now and restores it only to a unit whose phrases are unchanged (the pool's CRCs); the saved slots'
  chains and songs are not in the backup objects at all (`web/EDITOR_PROTOCOL.md`, "the arrangement").
- **Effects and step generators** as modules: the Engines table shows engines; the others will show the same way once they exist.
