// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat round 9: the installer page's top (the actual page script, a minimal DOM, controlled fetches and a fake unit).
//   a. the Nightly sits beside Standard at the top (not inside Advanced installs), each with its version string and one line; the last choice is remembered
//   b. one Install button installs everything ticked for the chosen build; the boxes are ticked by default; the page says what it will write
//   c. a unit on stock M-VAVE firmware (FM-1_710010300) takes the Nightly in one run, as it takes Standard
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";
import {MELODEE_LOADER, STOCK_LOADER} from "./fm1ota.js";

const template = readFileSync(new URL("index_pkg.html", import.meta.url), "utf8");

// a. the page's structure
const advanced = template.match(/<details id="advanced-installs"([^>]*)>([\s\S]*?)<\/details>/);
assert.ok(advanced && !/id="nightly-options"/.test(advanced[2]) && /id="rc-options"/.test(advanced[2]), "Advanced installs holds the release candidate only");
const at = (s) => template.indexOf(s);
assert.ok(at('<fieldset id="builds">') > 0 && at('id="build-dual"') > at('<fieldset id="builds">') && at('id="build-nightly-dual"') > at('id="build-dual"') &&
  at('id="build-nightly-dual"') < at('</fieldset>', at('<fieldset id="builds">')) && at('id="build-nightly-dual"') < at('<details id="advanced-installs"'),
  "Standard and the Nightly are radio buttons of one group at the top, before Advanced installs and before the install boxes");
assert.ok(at('id="build-nightly-dual"') < at('<fieldset id="stages">') && at('<fieldset id="stages">') < at('id="go"'), "the builds, then the boxes, then the one Install button");
for (const lang of ["ja", "en"]) {
  const block = template.match(new RegExp(`\\n  ${lang}: \\{([\\s\\S]*?)\\n  \\},?\\n`))?.[1] || template;
  for (const key of ["buildsTitle", "dualWhy", "nightlyWhy", "installWhat", "installNone", "stagesTitle"]) assert.match(block, new RegExp(`\\b${key}:`), `${lang} has ${key}`);
}
assert.match(template, /<input type="checkbox" id="stage-fw" checked>/);
assert.match(template, /<input type="checkbox" id="stage-pack" checked>/);
assert.match(template, /<input type="checkbox" id="stage-mod" checked>/);
assert.doesNotMatch(template, /<input type="checkbox" id="stage-prophet" checked>/, "the Prophet module does not fit with SHAPES: off by default (one at a time)");
assert.equal((template.match(/<button id="go"/g) || []).length, 1, "one Install button");

const NIGHTLY = {version: "v0.16.0-abcdef", product: "FM-1_900", branchName: "linux-hw", commit: "abcdef0123456789", builtAt: "2026-10-31T00:00:00Z",
  pkg: {dual: "nightly.fwsc"}, pak: "orangecat-nightly.pak", ocm: ["orangecat-shapes-nightly.ocm", "orangecat-prophet-nightly.ocm"]};
const STANDARD = {version: "0.15.0", product: "FM-1_900", pkg: {dual: "standard.fwsc"}, pak: "orangecat-0.15.0.pak", ocm: ["orangecat-shapes-0.15.0.ocm", "orangecat-prophet-0.15.0.ocm"]};
const META = {...STANDARD, releases: [STANDARD], nightly: NIGHTLY};

function boot(storage, unit = {model: "FM-1", text: "FM-1_710010300"}) {
  const elements = new Map(), pending = [], installs = [], runs = [];
  const element = (id) => {
    if (!elements.has(id)) elements.set(id, {checked: id === "build-dual" || id === "stage-fw" || id === "stage-pack" || id === "stage-mod", disabled: false, hidden: false,
      textContent: "", listeners: {}, options: [], value: "", dataset: {},
      replaceChildren(...options) { this.options = options; this.value = options.find((o) => o.selected)?.value || options[0]?.value || ""; },
      addEventListener(k, fn) { this.listeners[k] = fn; }});
    return elements.get(id);
  };
  const context = vm.createContext({
    Uint8Array, setTimeout: () => 0,
    navigator: {language: "en", requestMIDIAccess: async () => ({})},
    document: {documentElement: {}, querySelectorAll: () => [], getElementById: element, createElement: () => element("_o")},
    window: {addEventListener() {}},
    localStorage: storage,
    confirm: () => true,
    fetch: (url) => new Promise((resolve) => pending.push({url, resolve})),
    productOf: () => "FM-1_900", logicalImage: (b) => String.fromCharCode(...b), MELODEE_LOADER, STOCK_LOADER,
    Updater: class {
      async find(filter) { return filter({model: unit.model, text: unit.text}) ? {id: {model: unit.model, text: unit.text}, link: {close() {}}} : null; }
      async install(image, product, step, loader) { installs.push({image, product, loader, unit: unit.text}); step("done"); }
      async resume() { return true; }
    },
    runSet: async (ctx) => {                              // the page's one run: the firmware first, then the pack and the module (the set logic is fm1oc.js runSet, tested in test_installer_set.mjs)
      runs.push({stages: ctx.stages, pack: ctx.files.pack && String.fromCharCode(...ctx.files.pack), modules: ctx.files.modules.map((m) => String.fromCharCode(...m))});
      if (ctx.stages.firmware) await ctx.runFirmware(() => {});
      return {firmware: ctx.stages.firmware ? "installed" : "off", pack: ctx.stages.pack ? "installed" : "off", module: ctx.files.modules.map(() => "installed")};
    },
    OcLink: class {},
  });
  const script = template.match(/<script type="module">([\s\S]*?)<\/script>/)[1].replace("/*LIB*/", "").replace("/*META*/{}", JSON.stringify(META));
  vm.runInContext(script, context);
  const tick = () => new Promise((r) => setImmediate(r));
  return {element, pending, installs, runs, tick, context};
}
const answer = async (page, url) => {            // the fetch for url answers with the url's bytes
  const i = page.pending.findIndex((p) => p.url.startsWith(url)); assert.ok(i >= 0, "a fetch of " + url + " in " + page.pending.map((p) => p.url));
  const [p] = page.pending.splice(i, 1);
  p.resolve({ok: true, arrayBuffer: async () => Uint8Array.from(Buffer.from(url)).buffer});
  await page.tick(); await page.tick();
};
const store = () => { const m = new Map(); return {getItem: (k) => (m.has(k) ? m.get(k) : null), setItem: (k, v) => m.set(k, String(v)), m}; };

// a. both builds at the top, each with its version string
{
  const storage = store(), page = boot(storage);
  assert.equal(page.element("nightly-options").hidden, false);
  assert.equal(page.element("advanced-installs").hidden, true, "no release candidate: Advanced installs is not shown at all");
  assert.equal(page.element("dual-version").textContent, "v0.15.0");
  assert.equal(page.element("nightly-version").textContent, "v0.16.0-abcdef");
  assert.equal(page.element("nightly-info").textContent, "linux-hw · abcdef0 · v0.16.0-abcdef · 2026-10-31T00:00:00Z");
  assert.ok(page.element("build-dual").checked && !page.element("build-nightly-dual").checked, "Standard is the first choice of a new visitor");
  await answer(page, "standard.fwsc");
  // the choice is remembered
  page.element("build-dual").checked = false; page.element("build-nightly-dual").checked = true;
  page.element("build-nightly-dual").listeners.change();
  assert.equal(storage.m.get("oc-build"), "nightly-dual");
  await answer(page, "nightly.fwsc");
  const again = boot(storage);
  assert.ok(again.element("build-nightly-dual").checked && !again.element("build-dual").checked, "the page opens on the Nightly the next time");
  assert.ok(again.pending[0].url.startsWith("nightly.fwsc"), "and loads its package first");
  again.element("build-dual").checked = true; again.element("build-nightly-dual").checked = false; again.element("build-dual").listeners.change();
  assert.equal(storage.m.get("oc-build"), "dual");
  assert.ok(boot(storage).element("build-dual").checked, "Standard again when that was the last choice");
  // no storage (a private window): the page still works
  const bare = boot({getItem() { throw new Error("blocked"); }, setItem() { throw new Error("blocked"); }});
  assert.ok(bare.element("build-dual").checked);
  bare.element("build-dual").listeners.change();
}

// b. the boxes are what the one Install button writes
{
  const page = boot(store());
  page.element("build-dual").checked = false; page.element("build-nightly-dual").checked = true; page.element("build-nightly-dual").listeners.change();
  await answer(page, "nightly.fwsc");
  assert.equal(page.element("install-what").textContent,
    "The Install button writes what is ticked above, for the build you chose: the firmware, the asset pack, the SHAPES module (v0.16.0-abcdef)");
  page.element("stage-pack").checked = false; page.element("stage-pack").listeners.change();
  assert.equal(page.element("install-what").textContent,
    "The Install button writes what is ticked above, for the build you chose: the firmware, the SHAPES module (v0.16.0-abcdef)");
  page.element("stage-fw").checked = false; page.element("stage-mod").checked = false; page.element("stage-fw").listeners.change();
  assert.match(page.element("install-what").textContent, /nothing is ticked: nothing will be written$/);
  page.element("stage-fw").checked = page.element("stage-pack").checked = page.element("stage-mod").checked = true; page.element("stage-fw").listeners.change();
  assert.equal(page.element("install-what").textContent.endsWith("the firmware, the asset pack, the SHAPES module (v0.16.0-abcdef)"), true);
}

// c. a unit on stock M-VAVE firmware takes the Nightly in one run, as it takes Standard: one click, the package, the pack and the module of the chosen build
for (const [kind, pkg, expect] of [["dual", "standard.fwsc", STANDARD], ["nightly-dual", "nightly.fwsc", NIGHTLY]]) {
  const page = boot(store(), {model: "FM-1", text: "FM-1_710010300"});
  if (kind === "nightly-dual") { page.element("build-dual").checked = false; page.element("build-nightly-dual").checked = true; page.element("build-nightly-dual").listeners.change(); }
  await answer(page, pkg);
  assert.equal(page.element("go").disabled, false, kind + ": Install is ready once its package is loaded");
  const click = page.element("go").listeners.click();
  await page.tick();
  await answer(page, expect.pak);                       // everything is fetched before the first byte is written
  await answer(page, expect.ocm[0]);
  await click;
  assert.equal(page.runs.length, 1, kind + ": one run");
  assert.equal(JSON.stringify(page.runs[0].stages), JSON.stringify({firmware: true, pack: true, module: true}));
  assert.equal(page.runs[0].pack, expect.pak);
  assert.equal(JSON.stringify(page.runs[0].modules), JSON.stringify([expect.ocm[0]]), "the SHAPES module of the chosen build");
  assert.equal(page.installs.length, 1, kind + ": the firmware installed once, from the stock unit's own update");
  assert.equal(page.installs[0].unit, "FM-1_710010300");
  assert.equal(page.installs[0].product, "FM-1_900");
  assert.equal(page.installs[0].loader, MELODEE_LOADER);
  assert.ok(page.installs[0].image.startsWith(pkg), kind + ": the chosen build's package");
  assert.match(page.element("status").textContent, /Pack|pack|installed/i);
}
console.log("installer page: Standard and the Nightly side by side, the choice kept, the boxes are what Install writes, a stock unit takes either in one run: passed");
