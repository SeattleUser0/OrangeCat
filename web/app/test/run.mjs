// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app (web/app): every test, against the mocked unit, no browser, no hardware.
//   node web/app/test/run.mjs                 (tests/run_tests.sh runs it with the web tests)
// The generated files are checked first: web/app/lib/editor_proto.js against web/editor.html (always), and web/app/lib/layout.js and
// tests/fixtures/app/ against the firmware (when tests/run_tests.sh has built build/host/app; a note when it has not).
// Revisions:
//   0.14.2: first version (Session D).
import { existsSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { T, ok } from "./t.mjs";
import { makeProto, makeText } from "../tools/gen_proto.mjs";
import { check as checkLayout, mockText } from "../tools/gen_layout.mjs";
import { readFileSync } from "node:fs";

const HERE = dirname(fileURLToPath(import.meta.url)), ROOT = join(HERE, "..", "..", "..");

ok(readFileSync(join(HERE, "..", "lib", "editor_proto.js"), "utf8") === makeProto(), "app: lib/editor_proto.js is web/editor.html's protocol section, unchanged");
ok(readFileSync(join(HERE, "..", "lib", "editor_text.js"), "utf8") === makeText(), "app: lib/editor_text.js is web/editor.html's TEXT table, unchanged");
ok(readFileSync(join(HERE, "..", "lib", "mock_data.js"), "utf8") === mockText(join(ROOT, "tests", "fixtures", "app")), "app: lib/mock_data.js is the checked-in fixtures (the mocked unit plays the firmware's records)");
const built = join(ROOT, "build", "host", "app");
if (existsSync(join(built, "layout.json"))) {
  const p = checkLayout(built);
  ok(!p.length, "app: lib/layout.js and tests/fixtures/app are what the firmware writes now" + (p.length ? ` (${p.join("; ")})` : ""));
} else {
  console.log("app: build/host/app not built (tests/run_tests.sh builds it): the layout's drift check skipped");
}

const files = ["test_codec.mjs", "test_unit.mjs", "test_sounds.mjs", "test_project.mjs", "test_arrange.mjs", "test_steps.mjs", "test_settings.mjs", "test_page.mjs"];
for (const f of files) {
  if (!existsSync(join(HERE, f))) continue;
  try { await (await import("./" + f)).run(); }
  catch (e) { ok(false, `${f} threw: ${e && e.stack || e}`); }
}
console.log(T.failed ? `web app: ${T.failed} FAILED of ${T.passed + T.failed}` : `web app: all ${T.passed} passed`);
process.exit(T.failed ? 1 : 0);
