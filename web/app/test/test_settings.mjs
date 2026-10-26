// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 6: Settings (the unit's preferences, the audio readout, the page's colors from the unit's palettes).
import { ok } from "./t.mjs";
import { execFileSync } from "node:child_process";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { connected } from "./test_unit.mjs";
import { CMD, req, parse, FLASH_OPT } from "../lib/editor_proto.js";
import { PALETTES } from "../lib/palette.js";

const HERE = dirname(fileURLToPath(import.meta.url));
export async function run() {
  let same = true;
  try { execFileSync("python3", [join(HERE, "..", "tools", "gen_palette.py"), "--check"], { stdio: "pipe" }); } catch (_) { same = false; }
  ok(same && PALETTES[0].name === "NIGHT" && PALETTES.every((p) => p.knobs.length === 4 && p.knobs.every((k) => k.length === 4)),
     "settings: lib/palette.js is the unit's palettes and knob shades (tools/gen_ui_palettes.py), every palette, every track's four knobs");
  const { m, s } = await connected();
  ok(s.prefs && s.prefs.palettes.length > 0, "settings: the unit's preferences are read (UI_STATE, UI_PALETTES)");
  const r = parse[CMD.UI_SET](await s.rq(req.uiSet(0, 2), FLASH_OPT));
  await s.readPreferences();
  ok([0, 3, 4].includes(r.rc) && s.prefs.state.palette === 2, "settings: the screen palette is set on the unit (UI_SET 0)");
  s.close();
}
