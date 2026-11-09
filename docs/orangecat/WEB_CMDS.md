# web-cmds: what the editor link reaches since this branch (for Session A's review)

Session D, 2026-11-09. Branch `web-cmds`, from linux-hw 52a41894 (the Nightly v0.16.0-2e1056) with web-r3's web work merged in (so the web side and the wire-rule test exist); the
firmware change is the one commit on top that touches `firmware/src`. **Not merged into web-r3.** editor.c is Session A's file: this page lists exactly what to read.

## What it changes, and why

The web app's Pages tab (web-r3) showed these as "on the unit" because the editor link could not reach them (WEB_AUDIT.md, Session A item 8 and 1):

1. **BOOT (global 17) and the DRUM channel (global 13)**: the unit keeps them in `settings_boot` / `settings_drumch` (a byte each, saved with the settings). `SET` wrote
   `song.g[17]` / `song.g[13]`, which nothing reads: the editor said "saved" and the unit did nothing. Now `ed_val` returns the unit's own cells (`boot_cell`, `drumch_cell`: copies
   refreshed on every read, as the pages do), and a `SET` writes them back and calls `settings_save()` (as `ui_input.c` edit_param does).
2. **The WEIGHTS (globals 4..9)**: `settings_vpoints[0..4]` and `settings_safety`. They are `global_reserved` (a project load zeroes them, they are never in a project), so `SET` refused
   them. Now `SET` accepts exactly these ids, clamps by the unit's own descriptors (`WEIGHT_DESC`, hoisted from `page_desc` to file scope in params.c: the same array), writes the unit's
   storage and saves.
3. **SLIDE TIME (track id 48)**: refused (`param_slide_time`). Now written by `SET` (selected track) and `TRACK_PARAM` (any track), -64..63 by TP[48]. It has no motion lane; `motion_capture`
   returns at once for it.
4. **The HOME macros (track ids 33..36 = P_MAC0..3)**: refused (reserved). Now written when the track's own knob table has a macro on that knob (`mac_desc`, oc_macro.c), clamped -64..63
   as the unit's knob; the same cell the knob writes. Any other reserved id stays refused.

`ED_DESC` is **unchanged**: ids 4..9 and 33..36 still answer the reserved view (empty label, 0..127), so no older editor shows a new row. Only the clamp of a write uses the unit's
descriptor (`ed_wdesc`). The reserved globals 10, 11, 24..26 and every other reserved track id stay refused.

## What to read (all of it)

| file | change |
|---|---|
| `firmware/src/editor.c` | `ed_val` (device settings), `ed_dev_global`, `ed_dev_put`, `ed_macro`, `ed_may_set`, `ed_wdesc` (new, next to `ed_val`); the `ED_SET` and `ED_TRACK_PARAM` guards use `ed_may_set`; `SET` of a device setting calls `ed_dev_put`. 54 lines |
| `firmware/src/params.c` | `WEIGHT_DESC[]` moved out of `page_desc`'s block to file scope (the page uses it unchanged). 7 lines |
| `tests/editor_test.c` | `device_settings()` (new, 23 checks; the WEIGHT loop reports five); the existing `web_wire_rule()` now holds BOOT, DRUM, WEIGHTS "same" |
| `tests/web_wire.h`, `tests/web_tables.c` | the web app's wire rule and refused lists follow |
| `web/editor.html` (the mock), `web/app/*` | the web side: the mock follows the link; the Pages tab writes these cells; the Sounds tab's macro knobs write |
| `web/EDITOR_PROTOCOL.md` | the table of what the link reaches |

## The tests, and that they fail on the old code

`tests/editor_test.c` `device_settings()` (the suite's `editor` job): BOOT, DRUM channel, five WEIGHTS and GUARD, SLIDE TIME (SET and TRACK_PARAM), a MACRO track's PLUCK macro
(SET, clamps, TRACK_PARAM on another track only when its own table has the macro), a knob that is not a macro, the other reserved globals and SID's id 33 refused. **Run against the
unchanged editor.c of linux-hw 52a41894, 20 of its checks and the wire-rule check FAIL** (BOOT, DRUM, every WEIGHT, GUARD, SLIDE TIME x3, the macro x4, the DUMP checks, `web_wire_rule`);
with this branch's editor.c all pass. Also: `OC_CHECK=1 ./build.sh` (the pi32v2 compile check) passes; `tools/lint.sh` reports.

## Things to look at

- Each `SET` of a device setting calls `settings_save()` (a flash write), as A4 and BUDGET already did. The page sends one `SET` per choice (a select), never a drag.
- `ed_val` is called for every id by `ed_shadow` and the push loop: BOOT / DRUM copy the byte on every call (two stores).
- A watching editor now gets `CHANGED` pushes for BOOT, DRUM and the WEIGHTS when the unit's page changes them (the shadow reads the real storage).
- `ed_macro` is `#if OC_HOME_PIC` (mac_desc exists only then): without it the macros stay refused.
- The old editor: unchanged behavior for every id it knows; `DUMP` carries real values where it carried 0 or a dead copy.
