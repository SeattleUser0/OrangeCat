// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat controls guide: docs/orangecat/CONTROLS.md and web/app/controls.html are what web/app/tools/gen_controls.mjs makes from
// controls_src.json and the firmware's tables (lib/fw_tables.js), and the sentences cover every page, menu item, layer and family of
// the firmware (and keep none it has lost). Add a page to the firmware and this fails until the guide describes it.
import { readFileSync, existsSync } from "node:fs";
import { ok } from "./t.mjs";
import { outputs, MD_OUT, HTML_OUT, coverage, pageKeys, SRC, NIGHTLY_VERSION, TABLES_ID } from "../tools/gen_controls.mjs";
import { FW } from "../lib/fw_tables.js";

export async function run() {
  const cov = coverage();
  ok(!cov.length, "controls: every firmware page, menu item, layer and family has its sentences, none is stale" + (cov.length ? ` (${cov.slice(0, 4).join("; ")})` : ""));
  const o = outputs();
  const own = o.problems.filter((p) => !cov.includes(p));
  ok(!own.length, "controls: every knob of every page has a name and a sentence" + (own.length ? ` (${own.slice(0, 4).join("; ")})` : ""));
  ok(existsSync(MD_OUT) && readFileSync(MD_OUT, "utf8") === o.md, "controls: docs/orangecat/CONTROLS.md is what gen_controls.mjs makes now");
  ok(existsSync(HTML_OUT) && readFileSync(HTML_OUT, "utf8") === o.html, "controls: web/app/controls.html is what gen_controls.mjs makes now");
  /* the line at the top names the Nightly the guide describes: the firmware's version (VERSION), the build and commit, and the fingerprint of the tables it is made from */
  const top = (readFileSync(MD_OUT, "utf8").split("\n").find((l) => /^For the Nightly/.test(l)) || "");
  ok(new RegExp(`^For the Nightly v${NIGHTLY_VERSION.replace(/\./g, "\\.")}-${SRC.nightly.build} and later \\(firmware commit ${SRC.nightly.commit}, `).test(top) && top.includes(`tables ${TABLES_ID}`) && readFileSync(MD_OUT, "utf8").split("\n").indexOf(top) < 6,
     `controls: the top of the guide names the Nightly it describes (v${NIGHTLY_VERSION}-${SRC.nightly.build}, commit ${SRC.nightly.commit}) and the firmware tables it is made from (${TABLES_ID})`);
  const html = existsSync(HTML_OUT) ? readFileSync(HTML_OUT, "utf8") : "";
  ok(/<title>Orange Cat controls<\/title>/.test(html) && /name="viewport"/.test(html) && !/<script/i.test(html) && /prefers-color-scheme:dark/.test(html),
     "controls: the page has its title, a viewport, light and dark colors and no script (it reads on an iPad)");
  const keys = pageKeys();
  const shown = keys.filter((k) => !(k in SRC.exempt));
  ok(shown.every((k) => html.includes(`id="${k.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "")}"`)) && FW.menu.every((m) => o.md.includes(m + ":")),
     `controls: every one of the ${shown.length} pages and ${FW.menu.length} menu items is in the page`);
  ok(Object.keys(SRC.exempt).every((k) => SRC.exempt[k].length > 10), "controls: every page left out of the guide says why (controls_src.json exempt)");
}
