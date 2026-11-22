// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat round 9: the installer page's top (the actual page script, a minimal DOM, controlled fetches and a fake unit).
//   a. the Nightly sits beside Standard at the top (not inside Advanced installs), each with its version string and one line; the last choice is remembered
//   b. one Install button installs everything ticked for the chosen build; the boxes are ticked by default; the page says what it will write
//   c. a unit on stock M-VAVE firmware (FM-1_710010300) takes the Nightly in one run, as it takes Standard
import assert from "node:assert/strict";
import {readFileSync} from "node:fs";
import vm from "node:vm";
import {MELODEE_LOADER, STOCK_LOADER} from "./fm1ota.js";
import {PICK} from "./fm1pick.js";
import {PLACE} from "./fm1place.js";
import {OCM, fxCurrent} from "./fm1oc.js";

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
// (round 3, I2 I3) the links go to Orange Cat's repo, never Melodee's; the module box names the WHEEL module it also installs (ocmKind sorts a wheel file under it)
assert.doesNotMatch(template, /keremimo/, "no link to the Melodee repository");
assert.match(template, /github\.com\/SeattleUser0\/OrangeCat\/blob\/main\/docs\/RECOVERY\.md/, "recovery instructions: this repository's docs/RECOVERY.md");
assert.match(template, /stageMod: "SHAPES and WHEEL modules"/, "the module box names WHEEL");

const NIGHTLY = {version: "v0.16.0-abcdef", product: "FM-1_900", branchName: "linux-hw", commit: "abcdef0123456789", builtAt: "2026-10-31T00:00:00Z",
  pkg: {dual: "nightly.fwsc"}, pak: "orangecat-nightly.pak", ocm: ["orangecat-shapes-nightly.ocm", "orangecat-prophet-nightly.ocm"]};
const STANDARD = {version: "0.15.0", product: "FM-1_900", pkg: {dual: "standard.fwsc"}, pak: "orangecat-0.15.0.pak", ocm: ["orangecat-shapes-0.15.0.ocm", "orangecat-prophet-0.15.0.ocm"]};
const META = {...STANDARD, releases: [STANDARD], nightly: NIGHTLY};

function boot(storage, unit = {model: "FM-1", text: "FM-1_710010300"}, meta = META, extra = {}) {
  const elements = new Map(), pending = [], installs = [], runs = [], confirms = [], unitReads = {n: 0, data: null};
  const element = (id) => {
    if (!elements.has(id)) elements.set(id, {checked: id === "build-dual" || id === "stage-fw" || id === "stage-pack" || id === "stage-mod", disabled: false, hidden: false,
      textContent: "", listeners: {}, options: [], value: "", dataset: {},
      replaceChildren(...options) { this.options = options; this.value = options.find((o) => o.selected)?.value || options[0]?.value || ""; },
      addEventListener(k, fn) { this.listeners[k] = fn; }});
    return elements.get(id);
  };
  const context = vm.createContext({
    Uint8Array, setTimeout: () => 0,
    navigator: {language: "en", requestMIDIAccess: async () => unitReads.data ? {inputs: new Map([["i", {name: "Orange Cat", state: "connected"}]]), outputs: new Map([["o", {name: "Orange Cat", state: "connected"}]])} : {inputs: new Map(), outputs: new Map()}},
    document: {documentElement: {}, querySelectorAll: () => [], getElementById: element, createElement: () => element("_o")},
    window: {addEventListener() {}},
    localStorage: storage,
    confirm: (m) => { confirms.push(m); return unitReads.answer !== false; },
    fetch: (url) => new Promise((resolve) => pending.push({url, resolve})),
    productOf: () => "FM-1_900", logicalImage: (b) => String.fromCharCode(...b), firmwareIdentity: () => null, flashPartsOf: () => null, MELODEE_LOADER, STOCK_LOADER,
    Updater: class {
      async find(filter) { return filter({model: unit.model, text: unit.text}) ? {id: {model: unit.model, text: unit.text}, link: {close() {}}} : null; }
      async install(image, product, step, loader) { installs.push({image, product, loader, unit: unit.text}); step("done"); }
      async resume() { return true; }
    },
    runSet: async (ctx) => {                              // the page's one run: the firmware first, then the pack and the module (the set logic is fm1oc.js runSet, tested in test_installer_set.mjs)
      runs.push({stages: ctx.stages, pack: ctx.files.pack && String.fromCharCode(...ctx.files.pack), modules: ctx.files.modules.map((m) => String.fromCharCode(...m)), want: ctx.files.want, removable: ctx.removable});
      if (ctx.stages.firmware) await ctx.runFirmware(() => {});
      if (unitReads.rearrange) {                          // (round 23) the set logic asks before a rearrange: here the page's question is what is under test
        const drop = unitReads.drop || [], yes = await ctx.confirmRearrange(unitReads.rearrange, drop);   // (round 25) drop: modules not of the set taken off for good
        return {firmware: "off", pack: "off", module: ctx.files.modules.map(() => (yes ? "installed" : {error: "modRearrangeDeclined"})), rearrange: yes ? (unitReads.rearrange.length ? "done" : undefined) : "declined", ...(yes && drop.length ? {dropped: drop} : {})};
      }
      return {firmware: ctx.stages.firmware ? "installed" : "off", pack: ctx.stages.pack ? "installed" : "off", module: ctx.files.modules.map(() => "installed")};
    },
    OcLink: class { close() {} },
    listUnit: async () => { unitReads.n++; return unitReads.data; },
    slotInfo: async () => (unitReads.slot == null ? null : typeof unitReads.slot === "object" ? unitReads.slot : {state: unitReads.slot}),   // (Round 31, the review) 110 SLOT_INFO: the sample slot's state (null: no reply)          // (the review) Install reads the unit before it writes; the test says what the unit holds
    Date: class extends Date { static now() { return (unitReads.clock = (unitReads.clock || 0) + 6000); } },   // the page waits for a unit in steps of a second; here the wait runs out at once
    PICK, PLACE, OCM, fxCurrent, ...extra,                       // (Round 26) extra: a test's own stand-ins; the meter asks whether the unit holds the very pick (index_pkg.html pickStays); (the model picker) the packer and the room meter, inlined into the page by make_site.py
  });
  const script = template.match(/<script type="module">([\s\S]*?)<\/script>/)[1].replace("/*LIB*/", "").replace("/*META*/{}", JSON.stringify(meta));
  vm.runInContext(script, context);
  const tick = () => new Promise((r) => setImmediate(r));
  return {element, pending, installs, runs, tick, context, unitReads, confirms};
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
    "The Install button writes what is ticked above, for the build you chose: the firmware, the asset pack, the SHAPES and WHEEL modules (v0.16.0-abcdef)");
  page.element("stage-pack").checked = false; page.element("stage-pack").listeners.change();
  assert.equal(page.element("install-what").textContent,
    "The Install button writes what is ticked above, for the build you chose: the firmware, the SHAPES and WHEEL modules (v0.16.0-abcdef)");
  page.element("stage-fw").checked = false; page.element("stage-mod").checked = false; page.element("stage-fw").listeners.change();
  assert.match(page.element("install-what").textContent, /nothing is ticked: nothing will be written$/);
  page.element("stage-fw").checked = page.element("stage-pack").checked = page.element("stage-mod").checked = true; page.element("stage-fw").listeners.change();
  assert.equal(page.element("install-what").textContent.endsWith("the firmware, the asset pack, the SHAPES and WHEEL modules (v0.16.0-abcdef)"), true);
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
// d. (Release 3 step 4) an effect module in the release list ({url, kind, module_id}): its own box, sent after the engine module with what the list says it is;
//    a list of names only (every release so far) has the box off and sends no want; the template's box and texts
{
  assert.match(template, /<input type="checkbox" id="stage-fx" checked>/, "the effects box is ticked by default: a build that offers DUB TAPE installs it unless the player unticks it");
  for (const k of ["stageFx", "fxWhy", "installFx", "unitModsTitle", "unitRead", "unitEngines", "unitFx", "fxNoTable", "fxRemove", "fxRemoveConfirm", "modManifest", "fxOldFw"])
    assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
  const FX = {...STANDARD, ocm: [...STANDARD.ocm, {url: "orangecat-dub-0.15.0.ocm", kind: 1, module_id: 1, name: "DUB TAPE"}, {url: "bad.ocm", kind: 4, module_id: 2}]};
  const page = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(page, "standard.fwsc");
  page.element("stage-fx").checked = true; page.element("stage-fx").listeners.change();
  assert.equal(page.element("stage-fx").disabled, false, "the box is on when the release offers an effect module");
  assert.equal(page.element("fx-names").textContent, "(DUB TAPE)", "(Release 3 step 6) the effects box names what the release list offers: DUB TAPE");
  assert.match(page.element("install-what").textContent, /the SHAPES and WHEEL modules, the effect modules \(0\.15\.0\)$/);
  const click = page.element("go").listeners.click();
  await page.tick();
  await answer(page, FX.pak); await answer(page, FX.ocm[0]); await answer(page, "orangecat-dub-0.15.0.ocm");
  await click;
  assert.equal(JSON.stringify(page.runs[0].modules), JSON.stringify([FX.ocm[0], "orangecat-dub-0.15.0.ocm"]), "the engine module, then the effect module (a kind 4 entry is not offered)");
  assert.equal(JSON.stringify(page.runs[0].want), JSON.stringify([null, {kind: 1, module_id: 1}]), "what the list says each module is");
  const plain = boot(store());
  await answer(plain, "standard.fwsc");
  assert.equal(plain.element("stage-fx").disabled, true, "names only: no effect module offered");
  assert.equal(plain.element("fx-names").textContent, "", "... and no names");
  const c2 = plain.element("go").listeners.click(); await plain.tick();
  await answer(plain, STANDARD.pak); await answer(plain, STANDARD.ocm[0]); await c2;
  assert.equal(plain.runs[0].want, undefined, "an engine-only run is what it was: no want");
}
// d2. (Release 3 step 8a) an effect module the release list marks optional (the reverb modes SPRING, SHIMMER, RESO: {optional: true}, web/make_site.py FX_OPTIONAL) has its own box, which the
//     template leaves UNTICKED (the owner's decision); DUB TAPE's box is as it was. Install sends the optional module only when its box is ticked
{
  assert.match(template, /<input type="checkbox" id="stage-fx-opt">/, "the optional effects box is NOT ticked by default: the reverb module is offered, not installed, unless the player ticks it");
  assert.doesNotMatch(template, /id="stage-fx-opt"[^>]*checked/, "... no checked attribute on it");
  assert.match(template, /<input type="checkbox" id="stage-fx" checked>/, "... and DUB TAPE's box stays ticked by default");
  for (const k of ["stageFxOpt", "fxOptWhy", "installFxOpt"])
    assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
  const REV = {url: "orangecat-reverb-0.15.0.ocm", kind: 2, module_id: 2, name: "REVERB MODES", sectors: 1, optional: true};
  const FX = {...STANDARD, ocm: [...STANDARD.ocm, {url: "orangecat-dub-0.15.0.ocm", kind: 1, module_id: 1, name: "DUB TAPE", sectors: 3}, REV]};
  const page = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(page, "standard.fwsc");
  page.element("stage-fx").checked = true; page.element("stage-fx").listeners.change();          // (the mock's boxes start unticked: DUB's as the template has it, the optional one left as it starts)
  assert.equal(page.element("stage-fx-opt").disabled, false, "the optional box is on when the release offers an optional effect module");
  assert.equal(page.element("fx-names").textContent, "(DUB TAPE)", "the effects box names the modules that are not optional");
  assert.equal(page.element("fx-opt-names").textContent, "(REVERB MODES)", "the optional box names the optional ones");
  assert.match(page.element("install-what").textContent, /the SHAPES and WHEEL modules, the effect modules \(0\.15\.0\)$/, "unticked: Install does not name it");
  let click = page.element("go").listeners.click();
  await page.tick();
  await answer(page, FX.pak); await answer(page, FX.ocm[0]); await answer(page, "orangecat-dub-0.15.0.ocm");
  await click;
  assert.equal(JSON.stringify(page.runs[0].modules), JSON.stringify([FX.ocm[0], "orangecat-dub-0.15.0.ocm"]), "unticked: the engine module and DUB TAPE, not the reverb module");
  assert.equal(JSON.stringify(page.runs[0].want), JSON.stringify([null, {kind: 1, module_id: 1}]));
  const two = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(two, "standard.fwsc");
  two.element("stage-fx").checked = true; two.element("stage-fx").listeners.change();
  two.element("stage-fx-opt").checked = true; two.element("stage-fx-opt").listeners.change();
  assert.match(two.element("install-what").textContent, /the effect modules, the optional effect modules \(0\.15\.0\)$/, "ticked: Install names it");
  click = two.element("go").listeners.click();
  await two.tick();
  await answer(two, FX.pak); await answer(two, FX.ocm[0]); await answer(two, "orangecat-dub-0.15.0.ocm"); await answer(two, REV.url);
  await click;
  assert.equal(JSON.stringify(two.runs[0].modules), JSON.stringify([FX.ocm[0], "orangecat-dub-0.15.0.ocm", REV.url]), "ticked: the reverb module goes in after DUB TAPE");
  assert.equal(JSON.stringify(two.runs[0].want), JSON.stringify([null, {kind: 1, module_id: 1}, {kind: 2, module_id: 2}]), "... as what the list says it is: kind 2, module id 2");
  const only = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(only, "standard.fwsc");
  only.element("stage-fx-opt").checked = true; only.element("stage-fx-opt").listeners.change();   // DUB's box off, the optional one on
  click = only.element("go").listeners.click();
  await only.tick();
  await answer(only, FX.pak); await answer(only, FX.ocm[0]); await answer(only, REV.url);
  await click;
  assert.equal(JSON.stringify(only.runs[0].modules), JSON.stringify([FX.ocm[0], REV.url]), "the two boxes are independent: the reverb module without DUB TAPE");
  const none = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...STANDARD, ocm: [...STANDARD.ocm, {url: "orangecat-dub-0.15.0.ocm", kind: 1, module_id: 1, name: "DUB TAPE"}], releases: []});
  await answer(none, "standard.fwsc");
  assert.equal(none.element("stage-fx-opt").disabled, true, "a release with no optional effect module: the optional box is off");
  assert.equal(none.element("fx-opt-names").textContent, "", "... and names nothing");
}
// d3. (Release 3 step 8b, the owner's decision) the FX layer module (kind 3, module id 3: GLITCH, TAPE STOP, SHUFFLE, FREEZE; two sectors) is NOT optional: it is listed under effects
//     in DUB TAPE's box, which the template ticks; the reverb module stays in the optional box, unticked. Install sends DUB TAPE, then the FX layer, as what the list says they are
{
  const REV = {url: "orangecat-reverb-0.15.0.ocm", kind: 2, module_id: 2, name: "REVERB MODES", sectors: 1, optional: true};
  const FXL = {url: "orangecat-fxlayer-0.15.0.ocm", kind: 3, module_id: 3, name: "FX LAYER", sectors: 2};
  const FX = {...STANDARD, ocm: [...STANDARD.ocm, {url: "orangecat-dub-0.15.0.ocm", kind: 1, module_id: 1, name: "DUB TAPE", sectors: 3}, REV, FXL]};
  const page = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(page, "standard.fwsc");
  page.element("stage-fx").checked = true; page.element("stage-fx").listeners.change();          // (the mock's boxes start unticked: this one as the template has it, the optional one left as it starts)
  assert.equal(page.element("fx-names").textContent, "(DUB TAPE, FX LAYER)", "the effects box (ticked by default) names DUB TAPE and the FX layer");
  assert.equal(page.element("fx-opt-names").textContent, "(REVERB MODES)", "the optional box (unticked by default) names the reverb module alone");
  assert.match(page.element("install-what").textContent, /the SHAPES and WHEEL modules, the effect modules \(0\.15\.0\)$/, "Install names the effect modules, not the optional ones");
  let click = page.element("go").listeners.click();
  await page.tick();
  await answer(page, FX.pak); await answer(page, FX.ocm[0]); await answer(page, "orangecat-dub-0.15.0.ocm"); await answer(page, FXL.url);
  await click;
  assert.equal(JSON.stringify(page.runs[0].modules), JSON.stringify([FX.ocm[0], "orangecat-dub-0.15.0.ocm", FXL.url]), "the default ticks: the engine module, DUB TAPE, the FX layer; not the reverb module");
  assert.equal(JSON.stringify(page.runs[0].want), JSON.stringify([null, {kind: 1, module_id: 1}, {kind: 3, module_id: 3}]), "... the FX layer as what the list says it is: kind 3, module id 3");
  const two = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FX, releases: [FX]});
  await answer(two, "standard.fwsc");
  two.element("stage-fx").checked = true; two.element("stage-fx").listeners.change();
  two.element("stage-fx-opt").checked = true; two.element("stage-fx-opt").listeners.change();
  click = two.element("go").listeners.click();
  await two.tick();
  await answer(two, FX.pak); await answer(two, FX.ocm[0]); await answer(two, "orangecat-dub-0.15.0.ocm"); await answer(two, FXL.url); await answer(two, REV.url);
  await click;
  assert.equal(JSON.stringify(two.runs[0].modules), JSON.stringify([FX.ocm[0], "orangecat-dub-0.15.0.ocm", FXL.url, REV.url]), "the optional box ticked as well: the reverb module after the two (the unit's planner then says whether the set has a place: test_installer_set.mjs 18)");
  assert.match(template, /3: "FX LAYER"/, "a module in the way is named FX LAYER by its id");
}
// e. (Release 3, the model picker) the MACRO models box: ticked by default, its texts; with a master package in the release list ("ocp") the page lists the models (VA locked),
//    ticks MACRO's own fourteen by default, remembers the pick, meters the room against the ticked module stages, refuses Install while the pick does not fit, and sends the
//    packed module after the others with {kind: 4, module_id: 0x4D08}; without a master the box is off
{
  assert.match(template, /<input type="checkbox" id="stage-models" checked>/, "the models box is ticked by default: today's models go in unless the player unticks them");
  for (const k of ["stageModels", "modelsWhy", "installModels", "modelsBuiltIn", "modelsNoRoom"])
    assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
  // a made master: a header page, a glue region and three model regions of 4,096 B (ids 0 VA, 1 WAVSHP, 2 FM), one of 100 KB (3 GRAIN: too big beside the others) and a wave bank
  const region = (name, size) => ({ name, size });
  const regs = [region("glue", 256), region("mdl_va", 4096), region("mdl_wavshp", 4096), region("mdl_fm", 4096), region("mdl_grain", 100 * 1024), region("tbl_wav0", 4096)];
  let at = 0x100; const regions = regs.map((r) => { const o = { name: r.name, start: at, end: at + r.size - 16 }; at += r.size; return o; });
  const img0 = new Uint8Array(at), manifest = {
    name: "MODELS", engine: 8 | 4 << 8, version: 0x10000, area_lo: 0xC8000, area_sectors: 18, area_first: -6, area_last: 12, starts: Array.from({ length: 18 }, (_, i) => i - 6), code_off: 0x100,
    part_head: 184, poly: 4, regions, rules: { wavtbl: 23, terrain: 20, banks: [25] },
    items: [{ id: 0, key: "va", name: "VA", regions: ["mdl_va"], state: 252, big: 0 }, { id: 1, key: "wavshp", name: "WAVSHP", regions: ["mdl_wavshp"], state: 68, big: 0 },
            { id: 2, key: "fm", name: "FM", regions: ["mdl_fm"], state: 40, big: 0 }, { id: 3, key: "grain", name: "GRAIN", regions: ["mdl_grain"], state: 112, big: 0 },
            { id: 25, key: "wav0", name: "WAVE BANK A", regions: ["tbl_wav0"] }],
    hdr: { iface: 1, api_addr: 2, api_version: 3, api_size: 4, pack_hash: 5 } };
  new DataView(img0.buffer).setUint32(0, 0x444F4D4F, true);
  const masterOf = (manifest) => {
    const js = new TextEncoder().encode(JSON.stringify(manifest)), jl = (js.length + 3) & ~3, body = new Uint8Array(jl + img0.length);
    body.set(js, 0); body.set(img0, jl);
    const out = new Uint8Array(24 + body.length), mdv = new DataView(out.buffer);
    mdv.setUint32(0, 0x4D50434F, true); mdv.setUint32(4, 1, true); mdv.setUint32(8, jl, true); mdv.setUint32(12, img0.length, true); mdv.setUint32(16, 0, true); mdv.setUint32(20, PICK.crc32(body), true);
    out.set(body, 24);
    return out;
  };
  const masterBytes = masterOf(manifest);
  assert.ok(PICK.parseMaster(masterBytes), "the made master parses");
  const WITH = {...STANDARD, ocp: "orangecat-plaits-0.15.0.ocp", ocm: [...STANDARD.ocm, {url: "orangecat-dub-0.15.0.ocm", kind: 1, module_id: 1, name: "DUB TAPE", sectors: 3}]};
  const storage = store(), page = boot(storage, {model: "FM-1", text: "FM-1_710010300"}, {...WITH, releases: [WITH]});
  await answer(page, "standard.fwsc");
  {   // the master's fetch answers with the made bytes
    const i = page.pending.findIndex((p) => p.url.startsWith("orangecat-plaits-0.15.0.ocp")); assert.ok(i >= 0, "the page fetches the build's master package");
    const [p] = page.pending.splice(i, 1); p.resolve({ok: true, arrayBuffer: async () => masterBytes.buffer}); await page.tick(); await page.tick();
  }
  assert.equal(page.element("stage-models").disabled, false, "the box is on with a master");
  page.element("stage-models").checked = true; page.element("stage-models").listeners.change();   // (the mock's boxes start unticked; the template's is ticked)
  page.element("stage-fx").checked = true; page.element("stage-fx").listeners.change();
  assert.equal(page.element("models-pick").hidden, false, "the list shows under a ticked box");
  assert.ok(page.element("mdl-0").checked && page.element("mdl-0").disabled, "VA: ticked and locked (built in)");
  assert.ok(page.element("mdl-1").checked && page.element("mdl-2").checked && page.element("mdl-3").checked, "MACRO's own models are ticked by default");
  assert.ok(!page.element("mdl-25").checked, "a wave bank is not");
  assert.match(page.element("models-room").textContent, /^Room: 18 sectors; SHAPES 6, DUB TAPE 3 take 9; 9 left\. This pick \(WAVSHP, FM, GRAIN\) needs 28: 19 too many\. Leave a model or a wave bank out\. Install waits until the models fit\.$/,
    "the meter: the ticked module stages (SHAPES of the shapes box, DUB TAPE of the effects box with its sectors) and this pick; it does not fit: " + page.element("models-room").textContent);
  assert.equal(page.element("go").disabled, true, "Install waits while the pick does not fit");
  assert.match(page.element("install-what").textContent, /the effect modules, the MACRO models \(0\.15\.0\)$/);
  page.element("mdl-3").checked = false; page.element("mdl-3").listeners.change();
  assert.match(page.element("models-room").textContent, /This pick \(WAVSHP, FM\) needs 3: it fits, 6 to spare\. The unit has not been read yet\. Install reads it first/, page.element("models-room").textContent);
  assert.equal(page.element("go").disabled, false, "... and is ready once it does");
  assert.equal(storage.m.get("oc-models"), "[1,2]", "the pick is remembered");
  {   // (Release 3 step 8a) the room meter counts an optional effect module (the reverb modes, 1 sector) only when its own box is ticked
    const OPT = {...WITH, ocm: [...WITH.ocm, {url: "orangecat-reverb-0.15.0.ocm", kind: 2, module_id: 2, name: "REVERB MODES", sectors: 1, optional: true}]};
    const pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...OPT, releases: [OPT]});
    await answer(pg, "standard.fwsc");
    { const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1); q.resolve({ok: true, arrayBuffer: async () => masterBytes.buffer}); await pg.tick(); await pg.tick(); }
    pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
    pg.element("stage-fx").checked = true; pg.element("stage-fx").listeners.change();
    pg.element("mdl-3").checked = false; pg.element("mdl-3").listeners.change();
    assert.match(pg.element("models-room").textContent, /^Room: 18 sectors; SHAPES 6, DUB TAPE 3 take 9; 9 left\. This pick \(WAVSHP, FM\) needs 3: it fits, 6 to spare\./, "the optional box unticked: the reverb module takes no room: " + pg.element("models-room").textContent);
    pg.element("stage-fx-opt").checked = true; pg.element("stage-fx-opt").listeners.change();
    assert.match(pg.element("models-room").textContent, /^Room: 18 sectors; SHAPES 6, DUB TAPE 3, REVERB MODES 1 take 10; 8 left\. This pick \(WAVSHP, FM\) needs 3: it fits, 5 to spare\./, "ticked: its one sector is counted: " + pg.element("models-room").textContent);
    pg.element("stage-fx").checked = false; pg.element("stage-fx").listeners.change();
    assert.match(pg.element("models-room").textContent, /SHAPES 6, REVERB MODES 1 take 7; 11 left/, "DUB TAPE's box off, the optional one on: " + pg.element("models-room").textContent);
    pg.element("stage-fx-opt").checked = false; pg.element("stage-fx-opt").listeners.change();
    assert.match(pg.element("models-room").textContent, /SHAPES 6 take 6; 12 left/, "both off");
  }
  page.element("stage-fx").checked = false; page.element("stage-fx").listeners.change();
  assert.match(page.element("models-room").textContent, /SHAPES 6 take 6; 12 left/, "an unticked stage leaves the room");
  const click = page.element("go").listeners.click(); await page.tick();
  await answer(page, WITH.pak); await answer(page, WITH.ocm[0]);
  await click;
  assert.equal(page.runs.length, 1);
  assert.equal(page.runs[0].modules.length, 2, "the engine module, then the packed models module");
  assert.equal(JSON.stringify(page.runs[0].want), JSON.stringify([null, {kind: 4, module_id: 0x4D08}]), "what the models module is");
  const packed = page.runs[0].modules[1];
  assert.equal(packed.slice(0, 4), "OCMF", "a format 2 module package");
  const again = boot(storage, {model: "FM-1", text: "FM-1_710010300"}, {...WITH, releases: [WITH]});
  await answer(again, "standard.fwsc");
  { const i = again.pending.findIndex((p) => p.url.startsWith("orangecat-plaits-")); const [p] = again.pending.splice(i, 1); p.resolve({ok: true, arrayBuffer: async () => masterBytes.buffer}); await again.tick(); await again.tick(); }
  assert.ok(again.element("mdl-1").checked && !again.element("mdl-3").checked, "the next visit opens on the remembered pick");
  {   // (the review of the picker) the room is placed the way the unit's loader places it, from the unit as it is read; Install reads the unit again each time
    const unitOf = (mods, nstarts = 18) => {       // mods: [[start, engine, state, length, fx?{id, sectors}]] -> what listUnit answers (nstarts 21: a unit with the tail region)
      const starts = Array.from({length: nstarts}, () => ({state: 0, why: 0, engine: 0, length: 0})), effects = [], engines = [];
      for (const [st, eng, state, len, fx] of mods) { starts[st] = {state, why: 0, engine: fx ? 127 : eng, length: len}; if (fx) effects.push({id: fx.id, kind: 1, version: [1, 0, 0], count: 1, state, why: 0, start: st, sectors: fx.sectors, resident: false, uses: 0}); else engines.push({num: eng, src: 2, state, start: st}); }
      return {mod: {nstarts, sectors: 18, lowOk: true, starts}, engines: {engines}, fx: {effects}};
    };
    const fresh = async (unit, {fx = false, bytes = masterBytes, meta = WITH, ctx = {}} = {}) => {
      const pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...meta, releases: [meta]}, ctx);
      await answer(pg, "standard.fwsc");
      const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1); q.resolve({ok: true, arrayBuffer: async () => bytes.buffer}); await pg.tick(); await pg.tick();
      pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
      pg.element("stage-fx").checked = fx; pg.element("stage-fx").listeners.change();
      pg.element("mdl-3").checked = false; pg.element("mdl-3").listeners.change();            // WAVSHP and FM: 3 sectors
      pg.unitReads.data = unit;
      return pg;
    };
    // 6 + 4 + 3 + 1 = 14 of 18 sectors used, 4 free by subtraction; a 3 sector pick needs one run of 3, and the runs left are 2, 1 and 1: "fits" by 18 minus what is installed, no place for the loader
    const frag = unitOf([[0, 14, 7, 24160], [1, 7, 7, 2928], [14, 0, 7, 11612, {id: 0xDB0001, sectors: 3}], [16, 0, 7, 4000, {id: 0x1234, sectors: 1}]]);
    const pg = await fresh(frag);
    const before = pg.unitReads.n;
    const clk = pg.element("go").listeners.click(); await pg.tick(); await pg.tick(); await clk;
    assert.equal(pg.unitReads.n, before + 1, "Install reads the unit before it decides: " + pg.element("status").textContent + " | go.disabled " + pg.element("go").disabled);
    assert.equal(pg.runs.length, 0, "no place for the pick: nothing is written");
    assert.match(pg.element("models-room").textContent, /needs 3 sectors in one run; the biggest run the unit's loader can use is 2\./, pg.element("models-room").textContent);
    assert.equal(PLACE.simulate(PLACE.fromUnit(frag), [{mid: 0x4D08, sectors: 3}]).ok, false, "the same answer from the planner");
    // the same pick on a unit with room: the click reads, places and writes
    const roomy = await fresh(unitOf([[0, 14, 7, 24160]]));
    const c2 = roomy.element("go").listeners.click(); await roomy.tick(); await roomy.tick(); await answer(roomy, WITH.pak); await answer(roomy, WITH.ocm[0]);
    await c2;
    assert.equal(roomy.runs.length, 1, "a pick the loader can place is written: " + roomy.element("status").textContent + " | " + roomy.element("models-room").textContent);
    // the low sectors not usable: the same fragments but only the area from 0xC8000 (12 sectors)
    const lowOff = unitOf([[0, 14, 7, 24160], [1, 7, 7, 2928]]); lowOff.mod.lowOk = false;
    const lo = await fresh(lowOff);
    lo.element("go").listeners.click(); await lo.tick(); await lo.tick();
    assert.ok(/low sectors are not usable/.test(lo.element("models-room").textContent) && lo.runs.length === 0, "SHAPES 6 and WHEEL 4 leave 2 of the 12 usable sectors: the 3 sector pick is refused, with the reason: " + lo.element("models-room").textContent);
    // (round 23, FLASH_MAP_R3.md 10.5) a unit that reports the tail region (21 starts): the meter is the planner's answer in the goal's order (SHAPES, the models, then the
      // small modules); a set that fits only after its own modules are taken off and put on again says so and Install asks; a module of another set in the way is named
    for (const k of ["rearrangeConfirm", "rearrangeWorking", "modRearrangeLost", "modRearrangeDeclined", "modRearrangeLeft", "rearrangeStopped", "rearrangeDone"])
      assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
    const tailMaster = masterOf({...manifest, places: [{name: "area", first: -6, last: 12}, {name: "tail", first: -58, last: -55}]});
    const startsOf = (packed) => { const b = Uint8Array.from(packed, (c) => c.charCodeAt(0)), dv = new DataView(b.buffer); return Array.from({length: dv.getUint32(84, true)}, (_, k) => dv.getInt32(88 + 8 * k, true)); };
    const press = async (pg, fx) => { const c = pg.element("go").listeners.click(); await pg.tick(); await pg.tick(); if (pg.pending.length) { await answer(pg, WITH.pak); await answer(pg, WITH.ocm[0]); if (fx) await answer(pg, "orangecat-dub-0.15.0.ocm"); } await c; };
    // an empty unit with the tail: SHAPES first (start 1, the area), then the 3 sector pick, which the rule sends to the tail; the package carries the tail's start
    const empty = await fresh(unitOf([], 21), {bytes: tailMaster});
    await press(empty);
    assert.match(empty.element("models-room").textContent, /^Room: 18 sectors in the module area and 3 in the tail region\. This pick \(WAVSHP, FM\) needs 3 in one run\. The unit's loader can place it: SHAPES 6 \(module area\), MACRO models 3 \(tail region\)\.$/, empty.element("models-room").textContent);
    assert.equal(empty.runs.length, 1, "written");
    assert.deepEqual(startsOf(empty.runs[0].modules[1]), [...Array.from({length: 16}, (_, i) => i - 6), -58], "the pick's package for a unit with the tail: the area's starts and the tail's");
    assert.ok(!empty.confirms.length, "nothing to ask");
    // the same pick for a unit that reports 18 starts is the area's package, as it always was
    // (Round 31) for a unit that keeps its firmware (the firmware box unticked). Before Round 31 the box read as unticked here whatever it was (it is locked while Install
    // runs); ticked, the unit gets the tail build's firmware first and then has the tail: the package carries the tail's starts too (as for a unit that was not read)
    const u18 = await fresh(unitOf([[0, 14, 7, 24160]]), {bytes: tailMaster});
    u18.element("stage-fw").checked = false; u18.element("stage-fw").listeners.change();
    await press(u18);
    assert.deepEqual(startsOf(u18.runs[0].modules[1]), Array.from({length: 16}, (_, i) => i - 6), "a unit with 18 starts that keeps its firmware: no tail start in the package");
    const u18fw = await fresh(unitOf([[0, 14, 7, 24160]]), {bytes: tailMaster});
    await press(u18fw);
    assert.deepEqual(startsOf(u18fw.runs[0].modules[1]), [...Array.from({length: 16}, (_, i) => i - 6), -58], "a unit with 18 starts about to get the tail build's firmware: the tail's start too");
    // SHAPES at start 0, DUB TAPE (the effects box: of the set) at sector 7, another module in the tail, the low six not usable: the pick has no run of 3 as the unit
    // stands (1 and 2 are free); with SHAPES and DUB TAPE taken off and put on again in order it has. The meter says so, Install asks, the answer is the user's
    const tfrag = () => { const x = unitOf([[0, 14, 7, 24160], [7, 0, 7, 12288, {id: 1, sectors: 3}], [18, 0, 7, 12288, {id: 0x1234, sectors: 3}]], 21); x.mod.lowOk = false; return x; };
    assert.equal(PLACE.simulate(PLACE.fromUnit(tfrag()), [{mid: 0x4D08, sectors: 3}]).ok, false, "the planner: no place as the unit stands");
    const re = await fresh(tfrag(), {fx: true, bytes: tailMaster});
    re.unitReads.rearrange = ["SHAPES", "DUB TAPE"];
    await press(re, true);
    assert.match(re.element("models-room").textContent, /needs 3 in one run\. It fits once SHAPES, DUB TAPE are taken off the unit and installed again in an order that fits \(SHAPES 6 \(module area\), MACRO models 3 \(module area\), DUB TAPE 3 \(module area\)\): Install asks first\.$/, re.element("models-room").textContent);
    assert.equal(re.runs.length, 1, "Install goes on to the question");
    assert.equal(re.confirms.length, 1);
    assert.equal(re.confirms[0], "The set you ticked does not fit beside the modules as they sit in the FM-1 now. The page can take SHAPES, DUB TAPE off the FM-1 and install them again in an order that fits. " +
      "It takes about a minute, and until it is done the FM-1 plays without those engines and effects. If it is interrupted, press Install again and it finishes the job. Rearrange the modules?");
    assert.match(re.element("status").textContent, /The modules were taken off and installed again in an order that fits\.$/, re.element("status").textContent);
    const no = await fresh(tfrag(), {fx: true, bytes: tailMaster});
    no.unitReads.rearrange = ["SHAPES", "DUB TAPE"]; no.unitReads.answer = false;
    await press(no, true);
    assert.match(no.element("status").textContent, /Not rearranged, so nothing was written: the unit is as it was\.$/, no.element("status").textContent);
    // the effects box unticked: the two effect modules are not of the set; the pick would fit without the one in the tail, which is named (the first that the set cannot
    // stand beside, the others kept); nothing is written. (review of 5e7f2a40) DUB TAPE is not optional: an unticked box means "leave it alone", it is never taken off
    const blk0 = await fresh(tfrag(), {bytes: tailMaster});
    await press(blk0);
    assert.match(blk0.element("models-room").textContent, /needs 3 in one run\. ENGINE EFFECT 0x1234 INSTALLED AT START 18: REMOVE IT FIRST Install waits until the models fit\.$/, blk0.element("models-room").textContent);
    assert.equal(blk0.runs.length, 0); assert.ok(!blk0.confirms.length);
    // a module the build does not offer is never taken off: named, nothing written (the first that the set cannot stand beside, the others kept)
    const two = unitOf([[0, 14, 7, 24160], [7, 0, 7, 12288, {id: 0x1234, sectors: 3}], [18, 0, 7, 12288, {id: 0x5678, sectors: 3}]], 21); two.mod.lowOk = false;
    const blk1 = await fresh(two, {bytes: tailMaster});
    await press(blk1);
    assert.match(blk1.element("models-room").textContent, /needs 3 in one run\. ENGINE EFFECT 0x5678 INSTALLED AT START 18: REMOVE IT FIRST Install waits until the models fit\.$/, blk1.element("models-room").textContent);
    assert.equal(blk1.runs.length, 0); assert.ok(!blk1.confirms.length);
    // ... and DUB TAPE in the tail on a build that does not offer it: named by its name
    const swapped = unitOf([[0, 14, 7, 24160], [7, 0, 7, 12288, {id: 0x1234, sectors: 3}], [18, 0, 7, 12288, {id: 1, sectors: 3}]], 21); swapped.mod.lowOk = false;
    const blk = await fresh(swapped, {bytes: tailMaster, meta: {...WITH, ocm: STANDARD.ocm}});
    await press(blk);
    assert.match(blk.element("models-room").textContent, /needs 3 in one run\. ENGINE DUB TAPE INSTALLED AT START 18: REMOVE IT FIRST Install waits until the models fit\.$/, blk.element("models-room").textContent);
    assert.equal(blk.runs.length, 0, "a module that is not of the set is in the way: nothing is written");
    assert.ok(!blk.confirms.length, "and nothing is asked");
    // (Round 26, the review of 05a1e9f1) a unit with the tail region that holds this very pick at start 17 (sectors -6 .. -4), not where the goal's order puts a 3 sector pick (the
    // tail, as the empty unit above got it), and reports its CRCs: the meter says it stays and plans nothing for it, Install sends the set, nothing is asked. The test's master is
    // not a real models module, so the page's OCM.parse and fxCurrent are stand-ins here that tell the pick Install packs (the same bytes as the empty unit's run) from another;
    // fxCurrent itself and the meter's plan against runSet's are web/test_installer_set.mjs 22c. The same unit without the CRCs (a firmware before Round 26): written, as before
    {
      const packed = empty.runs[0].modules[1];
      const held = (crcs) => {
        const x = unitOf([[1, 14, 7, 24160]], 21);
        x.mod.starts[17] = {state: 7, why: 0, engine: 8, length: 3 * 4096};
        x.fx.effects.push({id: 0x4D08, kind: 4, version: [1, 0, 0], count: 2, state: 7, why: 0, start: 17, sectors: 3, resident: false, uses: 0});
        if (crcs) { x.mod.crcs = Array(21).fill(0); x.mod.pages = Array(21).fill(0); x.mod.crcs[17] = 1; x.mod.pages[17] = 1; }
        return x;
      };
      const seen = [];
      const ctx = {OCM: {parse: (b) => ({ok: true, kind: 4, packed: String.fromCharCode(...b)})},
                   fxCurrent: (o, fx, mod) => { seen.push(1); return o.packed === packed && !!mod.crcs && fx.effects.some((e) => e.id === 0x4D08 && e.start === 17); }};
      const stay = await fresh(held(true), {bytes: tailMaster, ctx});
      await press(stay);
      assert.ok(seen.length, "the meter asked whether the unit holds the pick");
      assert.match(stay.element("models-room").textContent, /needs 3 in one run\. It is in the unit already and stays as it is\.$/, stay.element("models-room").textContent);
      assert.equal(stay.runs.length, 1, "Install goes on"); assert.ok(!stay.confirms.length, "nothing asked");
      const old = await fresh(held(false), {bytes: tailMaster, ctx});
      await press(old);
      assert.doesNotMatch(old.element("models-room").textContent, /stays as it is/, "without the CRCs the pick is placed to be written: " + old.element("models-room").textContent);
      assert.match(old.element("models-room").textContent, /The unit's loader can place it: MACRO models 3/, old.element("models-room").textContent);
    }
    // (round 24; found on the unit in round 23, where the suite had passed) a build with the tail region ships WHEEL re-cut to one sector (fm1place.js ENG_GEO_TAIL). The page as
    // it opens (the unit not read, the default pick, the boxes of the build ticked: SHAPES and WHEEL, DUB TAPE, the models) counts WHEEL as 1: SHAPES 6, WHEEL 1 and the 9 sector
    // pick in the area, DUB TAPE in the tail. Counted as 4 the pick read "1 too many" and Install stayed locked, so the unit could not even be read. A build without the tail
    // region keeps WHEEL's 4
    {
      const WHEELED = {...WITH, ocm: ["orangecat-shapes-0.15.0.ocm", "orangecat-wheel-0.15.0.ocm", "orangecat-prophet-0.15.0.ocm", WITH.ocm.at(-1)]};
      const grain9 = regions.flatMap((r) => (r.name === "mdl_grain" ? [{...r, end: r.start + 28000 - 16}, {name: "rest", start: r.start + 28000, end: r.end}] : [r]));   // GRAIN cut to 28,000 B (the rest of it a region no item keeps): the default pick (WAVSHP, FM, GRAIN) is 9 sectors
      const open = async (places) => {
        const pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...WHEELED, releases: [WHEELED]});
        await answer(pg, "standard.fwsc");
        const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1);
        q.resolve({ok: true, arrayBuffer: async () => masterOf({...manifest, regions: grain9, ...(places ? {places} : {})}).buffer}); await pg.tick(); await pg.tick();
        pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
        pg.element("stage-fx").checked = true; pg.element("stage-fx").listeners.change();
        return pg;
      };
      const tb = await open([{name: "area", first: -6, last: 12}, {name: "tail", first: -58, last: -55}]);
      assert.ok(tb.element("stage-mod").checked && !tb.element("stage-mod").disabled && tb.element("mdl-3").checked, "the SHAPES and WHEEL box and the default pick are ticked");
      const room = tb.element("models-room").textContent;
      assert.match(room, /^Room: 18 sectors in the module area and 3 sectors in the tail region; /, room);
      assert.match(room, /\bWHEEL 1 \(module area\)/, "a tail build's WHEEL is one sector in the meter: " + room);
      assert.match(room, /DUB TAPE 3 \(tail region\) take 10; 11 left in the module area, 0 left in the tail region\. This pick \(WAVSHP, FM, GRAIN\) needs 9: it goes in the module area, 2 to spare\./, room);
      assert.doesNotMatch(room, /too many|Install waits/, "SHAPES 6, WHEEL 1 and the 9 sector pick stand in the area, DUB TAPE in the tail: " + room);
      assert.equal(tb.element("go").disabled, false, "Install is ready for the default pick of a tail build with the WHEEL package ticked");
      const nt = await open(null);
      assert.match(nt.element("models-room").textContent, /^Room: 18 sectors; WHEEL 4, SHAPES 6, DUB TAPE 3 take 13; 5 left\. This pick \(WAVSHP, FM, GRAIN\) needs 9: 4 too many\./, "a build without the tail region: WHEEL is 4 sectors, as its package is: " + nt.element("models-room").textContent);
      assert.equal(nt.element("go").disabled, true);
    }
    // (round 25, the owner's decisions A and B) the build offers DUB TAPE 3 and the FX layer 2 (the effects box) and the reverb modes 1 (the optional box); SHAPES 6, WHEEL 1 and the
    // default pick 9 (GRAIN cut to 28,000 B as above): 22 sectors with every box ticked, on 21.
    //   A. the page offers the ticked effect modules whose absence lets the rest fit; nothing is chosen for the owner (Install waits); the choice is shown in the meter and in what
    //      Install writes, and Install leaves that module out (each of the three in turn), on the unit as it is read too
    //   B. a unit holding the reverb module beside the default set: the meter says the reverb module is removed and not put back, Install asks with its name
    {
      const REVO = {url: "orangecat-reverb-0.15.0.ocm", kind: 2, module_id: 2, name: "REVERB MODES", sectors: 1, optional: true};
      const FXLO = {url: "orangecat-fxlayer-0.15.0.ocm", kind: 3, module_id: 3, name: "FX LAYER", sectors: 2}, DUBO = WITH.ocm.at(-1);
      const FULL = {...WITH, ocm: ["orangecat-shapes-0.15.0.ocm", "orangecat-wheel-0.15.0.ocm", DUBO, FXLO, REVO]};
      const grain9 = regions.flatMap((r) => (r.name === "mdl_grain" ? [{...r, end: r.start + 28000 - 16}, {name: "rest", start: r.start + 28000, end: r.end}] : [r]));
      const tm = masterOf({...manifest, regions: grain9, places: [{name: "area", first: -6, last: 12}, {name: "tail", first: -58, last: -55}]});
      const open = async ({rev = true} = {}) => {
        const pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FULL, releases: [FULL]});
        await answer(pg, "standard.fwsc");
        const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1); q.resolve({ok: true, arrayBuffer: async () => tm.buffer}); await pg.tick(); await pg.tick();
        pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
        pg.element("stage-fx").checked = true; pg.element("stage-fx").listeners.change();
        pg.element("stage-fx-opt").checked = rev; pg.element("stage-fx-opt").listeners.change();
        return pg;
      };
      const go = async (pg) => {                        // Install, every fetch answered with its url's bytes
        let done = false; const c = pg.element("go").listeners.click(); c.then(() => { done = true; });
        for (let k = 0; k < 40 && !done; k++) { while (pg.pending.length) { const p = pg.pending.shift(); p.resolve({ok: true, arrayBuffer: async () => Uint8Array.from(Buffer.from(p.url)).buffer}); } await pg.tick(); }
        await c;
      };
      const pg = await open();
      const gw = pg.element("give-way");
      assert.equal(gw.hidden, false, "22 of 21: the page asks which effect module gives way");
      assert.ok(gw.innerHTML.startsWith("The modules you ticked need more room than the FM-1 has. Choose the effect module that gives way: it is left out of this install, and the rest goes in."), gw.innerHTML);
      for (const [id, txt] of [[1, "DUB TAPE (3 sectors)"], [3, "FX LAYER (2 sectors)"], [2, "REVERB MODES (1 sector)"]]) assert.ok(gw.innerHTML.includes(`id="gw-${id}"> ${txt}`), txt + " is offered: " + gw.innerHTML);
      assert.ok(![1, 2, 3].some((id) => pg.element("gw-" + id).checked), "nothing is chosen for the owner");
      assert.match(pg.element("models-room").textContent, / Install waits until you choose\.$/, pg.element("models-room").textContent);
      assert.equal(pg.element("go").disabled, true, "Install waits for the choice");
      for (const [id, name, url, kind] of [[3, "FX LAYER", FXLO.url, 3], [2, "REVERB MODES", REVO.url, 2], [1, "DUB TAPE", DUBO.url, 1]]) {
        const p = await open();
        p.element("gw-" + id).checked = true; p.element("gw-" + id).listeners.change();
        assert.ok(p.element("models-room").textContent.startsWith(name + " gives way: it is not installed this time (if it is in the FM-1, it stays there as it is). Room: "), p.element("models-room").textContent);
        assert.doesNotMatch(p.element("models-room").textContent, /Install waits/, "the chosen set fits");
        assert.ok(p.element("install-what").textContent.endsWith(". " + name + " gives way: it is not installed this time (if it is in the FM-1, it stays there as it is)."), p.element("install-what").textContent);
        assert.equal(p.element("go").disabled, false, name + " chosen: Install is ready");
        // (review of 5e7f2a40) Install reads the unit (empty, with the tail): the unit as read is not the one the choice was made for, so the choice is cleared and asked again
        p.unitReads.data = unitOf([], 21);
        await go(p);
        assert.equal(p.runs.length, 0, "the first Install reads the unit and asks again"); assert.ok(![1, 2, 3].some((k) => p.element("gw-" + k).checked), "nothing chosen for the owner");
        assert.match(p.element("status").textContent, /Install waits until you choose\.$/, p.element("status").textContent);
        p.element("gw-" + id).checked = true; p.element("gw-" + id).listeners.change();
        await go(p);
        assert.equal(p.runs.length, 1, name + ": " + p.element("status").textContent);
        assert.ok(!p.runs[0].modules.includes(url) && p.runs[0].modules.length === 5, name + " is left out: " + p.runs[0].modules.map((m) => m.slice(0, 30)));
        assert.ok(!p.runs[0].want.some((w) => w && w.kind === kind), "... and not in what the list says is sent");
        assert.ok(p.element("models-room").textContent.startsWith(name + " gives way: it is not installed this time (if it is in the FM-1, it stays there as it is). Room: 18 sectors in the module area and 3 in the tail region. This pick (WAVSHP, FM, GRAIN) needs 9 in one run. The unit's loader can place it: SHAPES 6 (module area), MACRO models 9 (module area), WHEEL 1 (module area), "),
          p.element("models-room").textContent);
        assert.ok(!p.element("models-room").textContent.includes(name + " " + {1: 3, 2: 1, 3: 2}[id] + " ("), "the meter places the set without it: " + p.element("models-room").textContent);
      }
      // (review of 5e7f2a40) a choice belongs to the ticks it was made for: unticking and ticking the reverb box again clears it
      pg.element("gw-3").checked = true; pg.element("gw-3").listeners.change();
      assert.equal(pg.element("go").disabled, false);
      pg.element("stage-fx-opt").checked = false; pg.element("stage-fx-opt").listeners.change();
      pg.element("stage-fx-opt").checked = true; pg.element("stage-fx-opt").listeners.change();
      assert.ok(![1, 2, 3].some((k) => pg.element("gw-" + k).checked) && pg.element("go").disabled, "the choice is gone after the ticks changed: Install waits again");
      assert.doesNotMatch(pg.element("install-what").textContent, /gives way/);
      // ... and a choice never applies while it is not shown: the models box unticked, the set fits, the chooser is hidden and Install sends every ticked effect module
      {
        const h = await open();
        h.element("gw-3").checked = true; h.element("gw-3").listeners.change();
        h.element("stage-models").checked = false; h.element("stage-models").listeners.change();
        assert.equal(h.element("give-way").hidden, true, "no choice to make: hidden"); assert.doesNotMatch(h.element("install-what").textContent, /gives way/);
        h.unitReads.data = unitOf([], 21); await go(h);
        assert.equal(h.runs.length, 1); assert.ok([FXLO.url, REVO.url, DUBO.url].every((x) => h.runs[0].modules.includes(x)), "nothing is left out: " + h.runs[0].modules.map((m) => m.slice(0, 30)));
      }
      assert.ok(!template.match(/<div id="models-pick"[^]*?<\/div>/)[0].includes('id="give-way"'), "the choice is not inside the models list (hidden when the box is unticked)");
      // (review of 5e7f2a40) a unit at 21 of 21 (the default set, the FX layer at start 2) with every box ticked: DUB TAPE and the FX layer are in the unit and ticked; if one of them
      // gave way it would stay there untouched and the rest would not fit beside it, so only the reverb modes are offered; nothing is ever erased for a choice
      {
        const full = unitOf([[1, 14, 7, 24160], [5, 7, 7, 4096], [17, 0, 7, 36864, {id: 0x4D08, sectors: 9}], [18, 0, 7, 12288, {id: 1, sectors: 3}], [2, 0, 7, 8192, {id: 3, sectors: 2}]], 21);
        const f = await open();
        f.element("gw-3").checked = true; f.element("gw-3").listeners.change();   // (chosen on the count before the unit is read; the read clears it)
        f.unitReads.data = full; await go(f);
        assert.equal(f.runs.length, 0, f.element("models-room").textContent);
        assert.ok(f.element("give-way").innerHTML.includes('id="gw-2"') && !f.element("give-way").innerHTML.includes('id="gw-1"') && !f.element("give-way").innerHTML.includes('id="gw-3"'), "only the reverb modes: " + f.element("give-way").innerHTML);
        f.element("gw-2").checked = true; f.element("gw-2").listeners.change();
        // (re-review) the unit read again at Install with only its effects' use and a start's state changed: the same modules, so the choice stands
        f.unitReads.data = JSON.parse(JSON.stringify(full)); f.unitReads.data.fx.effects.forEach((e) => { e.uses = 3; e.why = 1; }); f.unitReads.data.mod.starts[2].state = 5;
        await go(f);
        assert.equal(f.runs.length, 1); assert.equal(JSON.stringify(f.runs[0].removable), "[]", "every effect module is ticked: none may be taken off");
        assert.ok(!f.runs[0].modules.includes(REVO.url) && !f.confirms.length);
        // the effects box unticked, the reverb modes ticked: DUB TAPE and the FX layer are left alone (named), never offered for removal
        const n = await open();
        n.element("stage-fx").checked = false; n.element("stage-fx").listeners.change();
        n.unitReads.data = full; await go(n);
        assert.equal(n.runs.length, 0); assert.ok(!n.confirms.length);
        assert.match(n.element("models-room").textContent, /INSTALLED AT START \d+: REMOVE IT FIRST/, n.element("models-room").textContent);
        assert.doesNotMatch(n.element("models-room").textContent, /removed and not put back/);
        assert.ok(!/id="gw-[13]"/.test(n.element("give-way").innerHTML), "DUB TAPE and the FX layer are not ticked: never offered (the reverb modes may give way instead): " + n.element("give-way").innerHTML);
      }
      // the reverb box unticked again: 21 of 21, no question, nothing gives way
      pg.element("stage-fx-opt").checked = false; pg.element("stage-fx-opt").listeners.change();
      assert.equal(pg.element("give-way").hidden, true); assert.equal(pg.element("go").disabled, false);
      assert.doesNotMatch(pg.element("install-what").textContent, /gives way/);
      // B. the unit as round 24 left it, with the reverb module at start 2 (sector 4) in place of the FX layer
      const held = unitOf([[1, 14, 7, 24160], [5, 7, 7, 4096], [17, 0, 7, 36864, {id: 0x4D08, sectors: 9}], [18, 0, 7, 12288, {id: 1, sectors: 3}], [2, 0, 7, 4096, {id: 2, sectors: 1}]], 21);
      const b = await open({rev: false});
      b.unitReads.data = held; b.unitReads.rearrange = []; b.unitReads.drop = ["REVERB MODES"];
      await go(b);
      assert.match(b.element("models-room").textContent, /needs 9 in one run\. It fits once REVERB MODES \(in the unit, not ticked\) is removed and not put back \(MACRO models 9 \(module area\), FX LAYER 2 \(module area\)\): Install asks first\.$/, b.element("models-room").textContent);
      assert.equal(b.runs.length, 1); assert.equal(JSON.stringify(b.runs[0].removable), JSON.stringify([{mid: 2}]), "the optional module, not ticked, is the only one that may be taken off");
      assert.ok(!b.runs[0].modules.includes(REVO.url), "the reverb module is not of the set");
      assert.equal(b.confirms[0], "The set you ticked does not fit beside the modules in the FM-1 now. REVERB MODES is in the FM-1 but not in the set you ticked: the page removes it to make room and does not put it back " +
        "(tick it and install it again if you want it back). Nothing else is moved. Remove REVERB MODES and install the set?");
      assert.match(b.element("status").textContent, /Removed to make room, not put back \(not in the set you ticked\): REVERB MODES\.$/, b.element("status").textContent);
      // with a rearrange of the set's own modules as well: the question names both
      const c2 = await open({rev: false});
      c2.unitReads.data = held; c2.unitReads.rearrange = ["SHAPES", "WHEEL", "DUB TAPE"]; c2.unitReads.drop = ["REVERB MODES"];
      await go(c2);
      assert.equal(c2.confirms[0], "The set you ticked does not fit beside the modules as they sit in the FM-1 now. REVERB MODES is in the FM-1 but not in the set you ticked: the page removes it to make room and does not put it back " +
        "(tick it and install it again if you want it back). The page also takes SHAPES, WHEEL, DUB TAPE off the FM-1 and installs them again in an order that fits. It takes about a minute, and until it is done the FM-1 plays " +
        "without those engines and effects. If it is interrupted, press Install again and it finishes the job. Remove REVERB MODES and rearrange the modules?");
      // (Round 31, found on the unit in the lead's Q4 rehearsal, ~/fm1-hw/captures/r31/q4a2.log) a unit on 0.15.0 Standard (v0.15.0-d6134, installed a minute before over the
      // Nightly) and Install of the tail build's Nightly with the default ticks: the firmware, SHAPES and WHEEL, DUB TAPE and the FX layer, the 9 sector pick. The unit's
      // replies as 0.15.0 sent them (q4a2.log "d: before"; q4/a1-standard): MOD_INFO 3 starts, a 12 sector area, no start table, no low-six byte (lowOk false); WHEEL at start 0
      // (0.15.0's 4 sector geometry) and SHAPES at 1, both 0.15.0's own (another interface, 2284971644); start 2 the Nightly's FX layer, which 0.15.0 refuses (state 2, why 3);
      // no answer to FX_TABLE. fcdc364b planned the set against THAT area and refused it before any write: "Room: 12 sectors; DUB TAPE 3, FX LAYER 2, WHEEL 4, SHAPES 6 take 15;
      // -3 left. ... DUB TAPE needs 3 sectors in one run; the biggest run the unit's loader can use is 2. (The six low sectors are not usable on this unit yet.)"
      {
        const {parseModInfo, parseEngines, parsePackInfo, runSet: realRunSet, OcLink: RealLink, OC_FX_KIND} = await import("./fm1oc.js");
        const T = await import("./test_placed_unit.mjs");
        const n3 = (v) => [v & 127, (v >> 7) & 127, (v >> 14) & 127], n5 = (v) => [0, 1, 2, 3, 4].map((i) => Math.floor(v / 2 ** (7 * i)) & 127);
        const IF015 = 2284971644;
        const v015 = ({prophet = false} = {}) => {   // prophet: 0.15.0's PROPHET at start 0 in WHEEL's place (the other engine stage of 0.15.0)
          const e0 = prophet ? 19 : 7, l0 = prophet ? 34764 : 2928;
          const mi = [3, 12, 3, 6, 112, ...n5(IF015), ...n5(33558784), ...n5(1702811984), 127, ...[[7, 0, e0, l0], [7, 0, 14, 24160], [2, 3, 0, 5864]].flatMap(([s, w, e, l]) => [s, w, e, ...n3(l)]), 127];
          const row = (e) => e === e0 ? [2, 7, 0, 0, 1, 0, 0, ...n3(l0), 0] : e === 14 ? [2, 7, 0, 1, 1, 0, 0, ...n3(24160), 0] : [e === 8 || e === 9 ? 1 : 0, 0, 0, 127, 0, 0, 0, ...n3(0), e === 8 ? 4 : 0];
          const en = [20, ...Array.from({length: 20}, (_, e) => row(e)).flat(), 3, 7, 0, e0, 7, 0, 14, 2, 3, 0];
          const pi = [1, 0, ...n3(40232), ...n5(1702811984), ...n3(40232), ...n5(1702811984), ...n3(40960)];
          return {pack: parsePackInfo(pi), mod: parseModInfo(mi), engines: parseEngines(en), fx: null};
        };
        {
          const u = v015();
          assert.ok(u.mod.nstarts === 3 && u.mod.sectors === 12 && u.mod.lowOk === false && !u.mod.sectorOf && u.mod.starts[2].state === 2 && u.mod.iface === IF015, "the mock reads as 0.15.0's probe in q4a2.log: " + JSON.stringify(u.mod));
          assert.deepEqual(u.engines.engines.filter((x) => x.src === 2).map((x) => [x.num, x.state, x.start, x.size]), [[7, 7, 0, 2928], [14, 7, 1, 24160]]);
          assert.equal(PLACE.afterFirmware(u, {iface: 1}).mod.lowOk, false, "(the review) the low six only when it is known the new firmware can free them: unknown is false");
          const p = PLACE.afterFirmware(u, {iface: 1, lowOk: true});
          assert.deepEqual(JSON.parse(JSON.stringify(p.mod.predicted)), {nstarts: 3, sectors: 12, lowOk: false, lowAssumed: true, junk: [{start: 0, engine: 7}, {start: 1, engine: 14}], unread: [{start: 2, length: 5864}]}, "what becomes junk, and what 0.15.0 cannot read");
          assert.ok(PLACE.isTailUnit(p.mod) && p.mod.lowOk && p.mod.sectors === 18 && p.mod.starts.slice(0, 3).every((x) => x.state === 2) && p.mod.starts.slice(3).every((x) => !x.state) && !p.engines,
            "as the tail build's firmware will read it: 21 starts, the low six the area's, 0.15.0's WHEEL and SHAPES refused (another interface), the rest empty");
          const same = PLACE.afterFirmware(u, {iface: IF015});
          assert.ok(same.mod.starts[0].state === 7 && same.mod.starts[1].state === 7 && same.engines, "a unit of the same interface keeps its modules");
        }
        // the unit the tail build's firmware reads after it is on (what runSet probes): 0.15.0's WHEEL and SHAPES refused, the Nightly's FX layer OK again at start 2, the
        // Nightly's old models at start 17 damaged (0.15.0's WHEEL went over sector 6; bad CRC), the tail empty (0.15.0's loader wrote its body over it), the low six the area's
        const nightlyAfter = (lowOk = true) => {   // lowOk false: the first boot found no room to free the six (smp_slot.c smp_claim)
          const s = T.unitState(true, [[2, {mid: 3, sectors: 2, kind: 3}]], lowOk);
          s.starts[0] = {state: 2, kind: 0, mid: 0, engine: 7, length: 2928}; s.starts[1] = {state: 2, kind: 0, mid: 0, engine: 14, length: 24160}; s.starts[17] = {state: 4, kind: 0, mid: 0, engine: 8, length: 36000};
          return T.placedUnit(s, RealLink);
        };
        // the page's Install with the set logic of fm1oc.js runSet: the files fetched by the page stand for the mock's packages of the same modules; the firmware stage swaps the
        // unit (0.15.0 -> the Nightly as it reads the flash)
        const flowOf = () => {
          const f = {fw: 0, unit: null, asked: [], sum: null, lowAfter: true};
          f.runSet = async (ctx) => {
            const real = (m) => { const s = String.fromCharCode(...m.slice(0, 64)); return /wheel/.test(s) ? T.engOcm(T.WHEEL, T.AREA) : /shapes/.test(s) ? T.engOcm(T.SHAPES, [0, 6]) : /dub/.test(s) ? T.fxOcm(T.DUB, 3, T.sectsFor(3)) : /fxlayer/.test(s) ? T.fxOcm(3, 2, T.sectsFor(2).slice(0, 18), {kind: 3}) : /reverb/.test(s) ? T.fxOcm(2, 1, T.sectsFor(1).slice(0, 18), {kind: 2}) : T.modelsOcm(9, T.sectsFor(9)); };
            f.names = Array.from(ctx.files.modules, (m) => m).map((m) => String.fromCharCode(...m.slice(0, 64)).replace(/\.ocm.*$/s, "").replace(/^OCMF.*$/s, "MODELS"));
            f.sum = JSON.parse(JSON.stringify(await realRunSet({...ctx, sleep: async () => {}, files: {...ctx.files, modules: Array.from(ctx.files.modules, real)},
              runFirmware: async (p) => { await ctx.runFirmware(p); f.fw++; f.unit = f.after ? f.after() : nightlyAfter(f.lowAfter); },
              openUnit: async () => (f.unit ? f.unit.link() : null), peekUnit: async () => (f.peek ? f.peek() : null),
              confirmRearrange: async (names, drop) => { f.asked.push([names, drop]); return ctx.confirmRearrange(names, drop); }})));
            return f.sum;
          };
          return f;
        };
        const openFlow = async ({rev = false, fw = true, slot = 1, prophet = false, data = null, extra = {}} = {}) => {   // data: the unit's replies (default: v015)   // slot: SLOT_INFO's state (1 claimed and empty; 0 not claimed; null no reply)   // extra: more stand-ins (an Updater)
          const f = flowOf(), pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...FULL, releases: [FULL]}, {runSet: f.runSet, OC_FX_KIND, ...extra});
          await answer(pg, "standard.fwsc");
          const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1); q.resolve({ok: true, arrayBuffer: async () => tm.buffer}); await pg.tick(); await pg.tick();
          pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
          pg.element("stage-fx").checked = true; pg.element("stage-fx").listeners.change();
          pg.element("stage-fx-opt").checked = rev; pg.element("stage-fx-opt").listeners.change();
          pg.element("stage-fw").checked = fw; pg.element("stage-fw").listeners.change();
          pg.unitReads.data = data ? data() : v015({prophet}); pg.unitReads.slot = slot;
          return {pg, f};
        };
        const goFlow = async (pg) => {
          let done = false; const c = pg.element("go").listeners.click(); c.then(() => { done = true; });
          for (let k = 0; k < 400 && !done; k++) { while (pg.pending.length) { const p = pg.pending.shift(); p.resolve({ok: true, arrayBuffer: async () => Uint8Array.from(Buffer.from(p.url)).buffer}); } await pg.tick(); }
          await c;
        };
        // 1. the default set: planned for the firmware Install writes first, Install goes on; the firmware, then the set in the goal's order on the unit as the new firmware reads it
        {
          const {pg, f} = await openFlow();
          assert.equal(pg.element("go").disabled, false, "the page as it opens (the unit not read): Install is ready");
          await goFlow(pg);
          const room = pg.element("models-room").textContent;
          assert.equal(pg.unitReads.n, 1, "Install read the unit");
          assert.doesNotMatch(room, /Room: 12 sectors|WHEEL 4|biggest run|low sectors are not usable|Install waits/, "not planned against 0.15.0's area: " + room);
          assert.match(room, /^Room: 18 sectors in the module area and 3 in the tail region\. This pick \(WAVSHP, FM, GRAIN\) needs 9 in one run\. The unit's loader can place it: SHAPES 6 \(module area\), MACRO models 9 \(module area\), WHEEL 1 \(module area\), DUB TAPE 3 \(tail region\), FX LAYER 2 \(module area\)\. \(Placed for the firmware Install writes first: the FM-1's firmware now has a module area of 12 sectors and 3 starts\. /, room);
          assert.equal(pg.runs.length, 0, "(the mock's own runSet is not used here)");
          assert.ok(f.sum, "Install ran the set: " + pg.element("status").textContent);
          assert.equal(f.fw, 1, "the firmware first");
          assert.deepEqual(f.names, ["orangecat-wheel-0.15.0", "orangecat-shapes-0.15.0", "orangecat-dub-0.15.0", "orangecat-fxlayer-0.15.0", "MODELS"], "the page's files, in its order");
          assert.equal(f.sum.firmware, "installed");
          assert.deepEqual(f.sum.module, ["installed", "installed", "installed", "current", "installed"], "WHEEL, SHAPES, DUB TAPE and the pick written, the FX layer the new firmware reads is kept: " + JSON.stringify(f.sum));
          // the goal's order on that unit, as the loader's planner places it: SHAPES at start 1 (sectors 6..11 over 0.15.0's refused copy), the pick at start 16 (sectors -5..3: start
          // 17 holds the damaged old pick, and an empty start comes first), WHEEL at 17 (sector -6, over it), DUB TAPE in the tail; the FX layer stays at start 2
          assert.equal(f.unit.ops.join(" "), "begin:14@1 begin:0x4d08@16 begin:7@17 begin:0x1@18", "the goal's order: " + f.unit.ops.join(" "));
          assert.ok(!f.asked.length && !f.unit.ops.some((o) => o.startsWith("remove")), "no rearrange asked, nothing taken off");
          // (the review) told before the firmware is written: the modules that stop working, by name, and what the plan assumes of the six low sectors
          assert.match(room, /In the FM-1 now, and not loaded by the new firmware \(the page may write over them\): WHEEL, SHAPES, a module at start 2 it cannot read\. The plan uses the six low sectors: the FM-1 says its sample slot is claimed/, room);
          assert.equal(pg.confirms.length, 1, "one question, before the firmware");
          assert.equal(pg.confirms[0], "Install writes the new firmware first, then reads the FM-1 again and installs the modules where the new firmware places them. These modules in the FM-1 now do not work with the new firmware: " +
            "they stop playing and may be written over: WHEEL, SHAPES, a module at start 2 it cannot read. The modules are planned on the six low sectors too, which the new firmware frees at its first start; if it cannot, " +
            "the FM-1 ends with the new firmware and without the modules of this set, and the page says why. Continue?");
          const st = f.unit.st, at = (i) => st.starts[i];
          assert.ok(at(1).state === 7 && at(1).engine === 14 && at(16).state === 7 && at(16).mid === 0x4D08 && at(17).state === 7 && at(17).engine === 7 && at(18).state === 7 && at(18).mid === 1 && at(2).state === 7 && at(2).mid === 3, "the unit ends with the whole set");
          assert.ok(st.starts.every((x, i) => [1, 2, 16, 17, 18].includes(i) ? x.state === 7 : x.state !== 7), "and nothing else usable (0.15.0's WHEEL and SHAPES are gone)");
          assert.match(pg.element("status").textContent, /Firmware: installed/, pg.element("status").textContent);
        }
        // 1b. (the review) the unit does not say its store has the room to free the six (its sample slot not claimed, or no answer): planned without them, the default set does
        //     not fit: refused before the firmware, saying why; nothing asked, nothing written
        for (const slot of [0, null]) {
          const {pg, f} = await openFlow({slot});
          await goFlow(pg);
          const room = pg.element("models-room").textContent;
          assert.ok(f.fw === 0 && !f.sum && !pg.confirms.length, "nothing written, nothing asked: " + room);
          assert.match(room, /The plan leaves the six low sectors out: the FM-1 does not say its record store has the room the new firmware needs to free them \(its sample slot is not claimed\)\. To use them, delete a project or patterns on the FM-1, restart it and press Install again\./, room);
          assert.match(pg.element("status").textContent, /Install waits until the models fit\.$/, pg.element("status").textContent);
        }
        // 1c. the slot claimed, but the new firmware's first boot keeps the six (the reviewer's case): the owner was told before the firmware, the module stage is refused after it
        //     with the planner's reason, nothing of the set written
        {
          const {pg, f} = await openFlow(); f.lowAfter = false;
          await goFlow(pg);
          assert.ok(/if it cannot, the FM-1 ends with the new firmware and without the modules of this set/.test(pg.confirms[0] || ""), "told first: " + pg.confirms[0]);
          assert.equal(f.fw, 1); assert.ok(f.sum.module.every((m) => m === "current" || m.error === "modNoPlace"), JSON.stringify(f.sum.module)); assert.equal(f.unit.ops.length, 0, "no module written");
        }
        // 1d. the question answered no: nothing written; 1e. 0.15.0's PROPHET (not of the set) is named in the meter and the question before it is written over
        {
          const {pg, f} = await openFlow(); pg.unitReads.answer = false;
          await goFlow(pg);
          assert.ok(f.fw === 0 && !f.sum && pg.confirms.length === 1, "declined: nothing written");
          assert.equal(pg.element("status").textContent, "Not installed, so nothing was written: the FM-1 is as it was.");
          const pr = await openFlow({prophet: true}); pr.pg.unitReads.answer = false;
          await goFlow(pr.pg);
          assert.match(pr.pg.element("models-room").textContent, /not loaded by the new firmware \(the page may write over them\): PROPHET, SHAPES, a module at start 2 it cannot read\./, pr.pg.element("models-room").textContent);
          assert.match(pr.pg.confirms[0] || "", /may be written over: PROPHET, SHAPES, a module at start 2 it cannot read\./, pr.pg.confirms[0]);
        }
        // 1f. (the re-review) a 0.15.0 sample longer than the new firmware's slot (60,896 B) runs into the six low sectors and reads EMPTY there: named, with its length, in the
        //     meter and in the question before the firmware; one that fits is not named. SLOT_INFO's reply as editor.c sends it, read by fm1oc.js slotInfo
        {
          const {slotInfo: realSlot} = await import("./fm1oc.js");
          const reply = (len, name) => [2, ...n3(len), 1, ...n3(81408), ...[...name].map((c) => c.charCodeAt(0)), 0];
          const ask = (r) => realSlot({ask: async () => r});
          const big = await ask(reply(70000, "VOX")), small = await ask(reply(40000, "KICK"));
          assert.deepEqual([big.state, big.len, big.name, small.len], [2, 70000, "VOX", 40000], "SLOT_INFO read: " + JSON.stringify(big));
          const {pg, f} = await openFlow({slot: big}); pg.unitReads.answer = false;
          await goFlow(pg);
          const lost = "The sample in the sample slot (VOX, 70000 bytes) is longer than the new firmware's slot keeps (60896 bytes): it will be lost. The FM-1 cannot send a sample back, so keep the WAV file you made it from if you want it, and load it again, shorter, after the install.";
          assert.ok(pg.element("models-room").textContent.includes(lost), "the meter names the sample: " + pg.element("models-room").textContent);
          assert.ok((pg.confirms[0] || "").includes(lost) && pg.confirms[0].endsWith(lost + " Continue?"), "and the question before the firmware: " + pg.confirms[0]);
          assert.equal(f.fw, 0);
          const ok = await openFlow({slot: small}); ok.pg.unitReads.answer = false;
          await goFlow(ok.pg);
          assert.ok(!/sample slot \(/.test(ok.pg.element("models-room").textContent) && !/sample slot \(/.test(ok.pg.confirms[0] || ""), "a sample that fits is not named");
        }
        // 1g. (Round 32, found on the unit in the gate's Q4: ~/fm1-hw/captures/r32/gate/q4/back-to-nightly) 0.15.0 Standard installed over Nightly 4573e7 whose own module stage
        //     did not run: MOD_INFO holds only start 1, the Nightly's SHAPES, refused by 0.15.0 (state 2, why 3: FW_MOD_BAD_IFACE); SLOT_INFO claimed. f3687c28 refused the
        //     default set: "MACRO models needs 9 sectors in one run; the biggest run the unit's loader can use is 6". The planner takes an empty start before one with junk:
        //     SHAPES went to start 0 (sectors 6..11) beside the junk at start 1, leaving two runs of six. In another order (the pick first) the set fits as the unit stands
        //     (PLACE.arrange). The full flow on both readings of start 1 by the new firmware: junk (another build's) and OK (the same build's SHAPES): every ticked module ends OK.
        {
          const v015s = () => {
            const mi = [3, 12, 3, 6, 112, ...n5(IF015), ...n5(33558784), ...n5(1702811984), 127, ...[[0, 0, 0, 0], [2, 3, 14, 24160], [0, 0, 0, 0]].flatMap(([s, w, e, l]) => [s, w, e, ...n3(l)]), 127];
            const en = [20, ...Array.from({length: 20}, (_, e) => [e === 8 || e === 9 || e === 14 ? 1 : 0, e === 14 ? 7 : 0, 0, 127, 0, 0, 0, ...n3(0), e === 8 ? 4 : 0]).flat(), 3, 0, 0, 0, 2, 3, 14, 0, 0, 0];
            const pi = [1, 0, ...n3(40232), ...n5(1702811984), ...n3(40232), ...n5(1702811984), ...n3(40960)];
            return {pack: parsePackInfo(pi), mod: parseModInfo(mi), engines: parseEngines(en), fx: null};
          };
          const u = v015s();
          assert.ok(u.mod.nstarts === 3 && u.mod.sectors === 12 && !u.mod.lowOk && u.mod.starts[1].state === 2 && u.mod.starts[1].why === 3 && u.mod.starts[1].engine === 14 && !u.mod.starts[0].state && !u.mod.starts[2].state, JSON.stringify(u.mod));
          const st = PLACE.fromUnit(PLACE.afterFirmware(u, {iface: 1, lowOk: true}));
          const S = {engine: 14, name: "SHAPES"}, P = {mid: 0x4D08, sectors: 9, name: "MACRO models"}, W = {engine: 7, name: "WHEEL"}, D = {mid: 1, sectors: 3, name: "DUB TAPE"}, F = {mid: 3, sectors: 2, name: "FX LAYER"};
          assert.equal(PLACE.simulate(st, [S, P, W, D, F]).failAt, 1, "the goal's order: SHAPES takes start 0, the pick has no run (the unit's refusal)");
          const a = PLACE.arrange(st, [W, S, D, F, P]);
          assert.equal(a.how, "asis", "another order places the set as the unit stands");
          assert.equal(a.plan.steps.map((x) => x.mod.name + "@" + x.start).join(" "), "MACRO models@17 SHAPES@1 WHEEL@5 DUB TAPE@18 FX LAYER@2");
          // (the review of a98b00b0) the search is depth-first with each prefix's state carried and two bounds (the sectors, the Effects table): the same first order as the
          // whole-order search of a98b00b0 (kept here as the reference) on the installer's units and on random ones, and a bounded cost
          {
            const oldOther = (s0, mods, order) => {
              const perm = (rest, pre, out) => { if (!rest.length) { out.push(pre); return; } rest.forEach((x, k) => perm([...rest.slice(0, k), ...rest.slice(k + 1)], [...pre, x], out)); return out; };
              for (const alt of perm(order, [], []).slice(1)) { const r = PLACE.simulate(s0, alt.map((i) => mods[i]).filter((m) => !m.current)); if (r.ok) return [alt, r.steps.map((x) => x.start)]; }
              return null;
            };
            const same = (s0, mods) => {
              const order = PLACE.goalOrder(s0, mods), a = oldOther(s0, mods, order), st0 = {plans: 0}, b = PLACE.otherOrder(s0, mods, order, st0);
              assert.equal(JSON.stringify(b && [b.order, b.steps.map((x) => x.start)]), JSON.stringify(a), "the same first order: " + JSON.stringify(mods));
              return st0.plans;
            };
            const R = {mid: 2, sectors: 1, name: "REVERB MODES"}, X = {mid: 9, sectors: 1, name: "X"};
            const units = [st, PLACE.fromUnit(PLACE.afterFirmware(v015(), {iface: 1, lowOk: true})), PLACE.fromUnit(PLACE.afterFirmware(v015({prophet: true}), {iface: 1, lowOk: true})),
                           PLACE.fromUnit(PLACE.afterFirmware(u, {iface: 1})), T.nightly(true), T.unitState(true), T.unitState(true, [], false)];
            for (const s0 of units) for (const set of [[W, S, D, F, P], [W, S, D, F, R, P], [W, S, D, F, R, X, P], [S, P], [P], [W, S, D, P]]) same(s0, set);
            let seed = 7; const rnd = (n) => { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; };
            const POOL = [{engine: 14}, {engine: 7}, {mid: 0x4D08, sectors: 9}, {mid: 0x4D08, sectors: 5}, {mid: 1, sectors: 3}, {mid: 3, sectors: 2}, {mid: 2, sectors: 1}, {mid: 9, sectors: 1}, {mid: 10, sectors: 2}, {mid: 11, sectors: 1}];
            let cases = 0, most = 0;
            for (let it = 0; it < 600; it++) {
              const s0 = PLACE.emptyState(rnd(4) > 0, 3);
              for (let k = rnd(3); k > 0; k--) s0.starts[rnd(21)] = {state: 2, kind: 0, mid: 0, engine: [14, 7][rnd(2)], length: 4096 * (1 + rnd(6))};
              for (let k = rnd(3); k > 0; k--) s0.starts[rnd(21)] = {state: 7, kind: 1, mid: 0x55 + k, engine: PLACE.OC_FX_ENGINE, length: 4096 * (1 + rnd(2))};
              s0.nfxmod = s0.starts.filter((x) => x.state === 7 && x.kind).length;
              const ids = new Set(), mods = [], want = 3 + rnd(4);
              while (mods.length < want) { const m = POOL[rnd(POOL.length)], id = m.engine != null ? "e" + m.engine : "m" + m.mid; if (ids.has(id) || (m.mid === 0x4D08 && ids.has("p"))) continue; ids.add(id); if (m.mid === 0x4D08) ids.add("p"); mods.push({...m, current: rnd(6) === 0}); }
              if (PLACE.simulate(s0, PLACE.goalOrder(s0, mods).map((i) => mods[i]).filter((m) => !m.current)).ok) continue;
              cases++; most = Math.max(most, same(s0, mods));
            }
            assert.ok(cases > 100 && most < 400, "random units: " + cases + " compared, at most " + most + " planner calls (the whole-order search: up to 6! orders x 6)");
            const seven = [W, S, D, F, R, X, P], t0 = performance.now();
            for (let k = 0; k < 5; k++) assert.equal(PLACE.arrange(st, seven).how, "none", "seven modules, 22 sectors on 21: no order");
            assert.ok((performance.now() - t0) / 5 < 20, "the seven-module no-fit under 20 ms: " + ((performance.now() - t0) / 5).toFixed(1) + " ms (the whole-order search took about 700)");
          }
          for (const okAt1 of [false, true]) {
            const {pg, f} = await openFlow({data: v015s});
            f.after = () => {                                   // the unit as the tail build's firmware reads it: start 1 junk, or the same build's SHAPES (OK)
              const s0 = T.unitState(true, okAt1 ? [[1, {engine: 14}, 24160]] : []);
              if (!okAt1) s0.starts[1] = {state: 2, kind: 0, mid: 0, engine: 14, length: 24160};
              return T.placedUnit(s0, RealLink);
            };
            await goFlow(pg);
            const room = pg.element("models-room").textContent;
            assert.doesNotMatch(room, /biggest run|Install waits/, "not refused: " + room);
            assert.match(room, /The unit's loader can place it: MACRO models 9 \(module area\), SHAPES 6 \(module area\), WHEEL 1 \(module area\), DUB TAPE 3 \(tail region\), FX LAYER 2 \(module area\)\./, room);
            assert.match(room, /not loaded by the new firmware \(the page may write over them\): a module at start 1 it cannot read\./, room);
            assert.equal(f.fw, 1, "the firmware, then the set"); assert.ok(f.sum, pg.element("status").textContent);
            assert.ok(!f.asked.length && !f.unit.ops.some((o) => o.startsWith("remove")), "nothing taken off");
            const want = ["installed", "installed", "installed", "installed", "installed"];   // (OK at start 1: the mock's SHAPES file is another size, so it is written again there)
            assert.deepEqual(f.sum.module, want, (okAt1 ? "start 1 OK: " : "start 1 junk: ") + JSON.stringify(f.sum.module) + " " + f.unit.ops.join(" "));
            const at = (i) => f.unit.st.starts[i];
            for (const [i, ok] of [[1, (x) => x.engine === 14], [5, (x) => x.engine === 7], [18, (x) => x.mid === 1], [2, (x) => x.mid === 3]])
              assert.ok(at(i).state === 7 && ok(at(i)), "start " + i + ": " + JSON.stringify(at(i)));
            const mdl = f.unit.st.starts.findIndex((x) => x.state === 7 && x.mid === 0x4D08);
            assert.ok(mdl >= 0, "the pick is on the unit, OK");
            assert.equal(f.unit.st.starts.filter((x) => x.state === 7).length, 5, "every ticked module, state 7, nothing else");
          }
        }
        // 1h. (Round 32, found on the unit: 0.15.0 Standard -> the Release 3 Nightly from the live page) the firmware was written, the unit restarted and the page gave up at 90 s
        //     ("The FM-1 did not come back. Power-cycle it."): the new firmware answered 132 s after the write (its first start moves the record store out of the six low
        //     sectors: smp_slot.c smp_claim), and DUB TAPE and the rest of the set were never installed. The real fm1ota.js Updater (its install path: this unit is running
        //     0.15.0 at the press) on a clock of the test's own: a unit that answers 150 s after the write, and one that answers only after the bound (FIRST_START_MS): the
        //     page waits, says the write finished and to keep it connected, never "power-cycle" before the bound, and runs the module stage in the same press: every
        //     ticked module state 7. 57ec17a4: the page ends with noreturn after 40 s (install) / 90 s (resume), no module written.
        {
          const OTA = await import("./fm1ota.js"), BOUND = OTA.FIRST_START_MS || 300000;
          const slowUnit = (backAfter, pgRef) => {
            const c = {t: 0, phase: "running", wroteAt: null, said: [], installs: 0};
            class Slow extends OTA.Updater {
              now() { return c.t; }
              async pause(ms) { c.t += ms; }
              async find(filter) {
                const id = c.phase === "running" ? {model: "FM-1", text: "FM-1_900"} : c.phase === "ota" ? {model: "ota-FM-1", text: MELODEE_LOADER}
                  : c.t - c.wroteAt >= backAfter ? {model: "FM-1", text: "FM-1_900"} : null;
                if (c.phase === "written") c.said.push([c.t - c.wroteAt, pgRef.pg.element("status").textContent]);
                await this.pause(2000);                       // a look at the ports: the handshake's two tries
                return id && filter(id) ? {id, link: {send: () => true, close() {}}} : null;
              }
              async install(...a) { c.installs++; return super.install(...a); }
              async serve(link, image, finishAddr) { if (finishAddr === 0xE0000000) c.phase = "ota"; return {served: 1, finished: true}; }   // step 1: the loader staged
              async write(ota, image, step) { step("write", 1); c.phase = "written"; c.wroteAt = c.t; return {served: 1167, finished: true}; }   // step 2: the whole image written
            }
            return {Slow, c};
          };
          for (const backAfter of [150000, BOUND + 90000]) {
            const ref = {}, {Slow, c} = slowUnit(backAfter, ref);
            const {pg, f} = await openFlow({extra: {Updater: Slow}}); ref.pg = pg;
            await goFlow(pg);
            const late = c.said.filter(([, x]) => /Power-cycle/i.test(x));
            assert.equal(c.installs, 1, "the install path (the unit was running 0.15.0 at the press)");
            assert.ok(c.said.length && c.said.filter(([dt]) => dt < BOUND).every(([, x]) => x === "The write finished; the FM-1 is starting the new firmware (its first start after 0.15.0 can take a few minutes): keep it connected."),
              "while the new firmware starts the page says the write finished: " + JSON.stringify(c.said.slice(0, 3)));
            assert.ok(late.every(([dt]) => dt >= BOUND), "never power-cycle before the bound: " + JSON.stringify(late[0]));
            if (backAfter > BOUND) assert.ok(late.length && late[0][1] === "The FM-1 has not answered in 5 minutes. Power-cycle it, with the USB cable left in: the page goes on when it answers.", "after the bound: " + JSON.stringify(late[0]));
            else assert.equal(late.length, 0);
            assert.ok(f.sum, "the press went on to the module stage (" + backAfter / 1000 + " s): " + pg.element("status").textContent);
            assert.equal(f.fw, 1); assert.equal(f.sum.firmware, "installed");
            assert.deepEqual(f.sum.module, ["installed", "installed", "installed", "current", "installed"], JSON.stringify(f.sum));
            assert.ok(f.unit.st.starts.filter((x) => x.state === 7).length === 5 && f.unit.st.starts[18].state === 7 && f.unit.st.starts[18].mid === 1, "every ticked module state 7, DUB TAPE among them: " + f.unit.ops.join(" "));
            assert.match(pg.element("status").textContent, /^Result: Firmware: installed/, pg.element("status").textContent);
          }
          // the resume path's wait is the same bound: a press begun in update mode, the unit back 150 s after the write
          {
            const ref = {}, {Slow, c} = slowUnit(150000, ref); c.phase = "ota";
            const {pg, f} = await openFlow({extra: {Updater: Slow}}); ref.pg = pg;
            await goFlow(pg);
            assert.equal(c.installs, 0, "resumed, not installed in full");
            assert.ok(f.sum && f.sum.module.every((m) => m === "installed" || m === "current"), "the modules after a resume: " + pg.element("status").textContent);
            assert.ok(!c.said.some(([, x]) => /Power-cycle/i.test(x)));
          }
          // (3) a press that ended before the module stage (an older page, a lost link): the unit runs this package's image (MOD_INFO's fw), its modules as the new firmware
          //     left them. The next press leaves the firmware as it is, writes the missing modules and asks nothing; a press that lost the link part way through the modules,
          //     the same: the next press writes only the modules not yet there
          {
            const {listUnit: realList} = await import("./fm1oc.js");
            const FWID = {len: 530000, crc: 0x1234567, rest: 0x7654321};
            const s = T.unitState(true, []);   // (the FX layer of 1-1g is left out: this mock reports a CRC only for an image it holds)
            s.starts[0] = {state: 2, kind: 0, mid: 0, engine: 7, length: 2928}; s.starts[1] = {state: 2, kind: 0, mid: 0, engine: 14, length: 24160}; s.starts[17] = {state: 4, kind: 0, mid: 0, engine: 8, length: 36000};
            const unit = T.placedUnit(s, RealLink, {crc: true, fw: FWID});
            const again = async () => {
              const {pg, f} = await openFlow({data: () => ({}), extra: {firmwareIdentity: () => FWID}});
              pg.unitReads.data = await realList(unit.link());
              assert.ok(PLACE.isTailUnit(pg.unitReads.data.mod) && pg.unitReads.data.mod.fw && pg.unitReads.data.mod.fw.len === FWID.len, "the unit reads as the Nightly: " + JSON.stringify(pg.unitReads.data.mod.fw));
              f.unit = unit; f.peek = () => unit.link(); f.after = () => { throw new Error("the firmware is current: not written"); };
              const from = unit.ops.length;
              await goFlow(pg);
              assert.equal(f.fw, 0, "the firmware not written again");
              assert.equal(pg.confirms.length, 0, "nothing asked: " + JSON.stringify(pg.confirms)); assert.equal(f.asked.length, 0);
              return {pg, f, ops: unit.ops.slice(from)};
            };
            unit.fail = (op, k) => (op === "begin" && k === 2 ? "lost" : null);   // the first press loses the link at its third module
            const first = await again();
            assert.ok(!first.f.sum || first.f.sum.module.some((m) => m !== "installed" && m !== "current"), "the first press did not finish: " + first.pg.element("status").textContent);
            const had = unit.st.starts.filter((x) => x.state === 7).length;
            assert.ok(had === 2, "two modules in before the link went: " + unit.ops.join(" "));
            unit.fail = null;
            const {f, ops} = await again();
            assert.ok(f.sum, "the second press ran the set");
            assert.equal(f.sum.firmware, "current");
            assert.equal(f.sum.module.filter((m) => m === "current").length, 2, "the two already in are left as they are: " + JSON.stringify(f.sum.module));
            assert.ok(f.sum.module.every((m) => m === "current" || m === "installed"), JSON.stringify(f.sum.module));
            assert.ok(ops.length === 3 && ops.every((o) => o.startsWith("begin")), "only the three missing modules written: " + ops.join(" "));
            assert.ok(unit.st.starts.filter((x) => x.state === 7).length === 5, "every ticked module state 7");
          }
        }
        // 2. the optional reverb modes ticked as well (22 sectors on 21): it does not fit even after the new firmware; the page says so on that unit and offers the give-way choice
        //    (nothing chosen for the owner, nothing written); chosen, Install goes on without it
        {
          const {pg, f} = await openFlow({rev: true});
          assert.equal(pg.element("give-way").hidden, false, "22 of 21 on the page as it opens (the unit not read): the choice");
          pg.element("gw-2").checked = true; pg.element("gw-2").listeners.change();   // chosen before the unit is read: the read clears it (round 25)
          await goFlow(pg);
          assert.equal(f.fw, 0, "nothing written"); assert.ok(!f.sum);
          const gw = pg.element("give-way");
          assert.equal(gw.hidden, false, "the give-way choice: " + pg.element("models-room").textContent);
          assert.ok([1, 2, 3].every((id) => gw.innerHTML.includes(`id="gw-${id}"`)), gw.innerHTML);
          assert.match(pg.element("status").textContent, /Placed for the firmware Install writes first[^]*Install waits until you choose\.$/, pg.element("status").textContent);
          pg.element("gw-2").checked = true; pg.element("gw-2").listeners.change();
          await goFlow(pg);
          assert.equal(f.fw, 1); assert.ok(f.sum && !f.names.some((x) => /reverb/.test(x)), "the reverb modes left out: " + f.names);
          assert.equal(f.unit.ops.join(" "), "begin:14@1 begin:0x4d08@16 begin:7@17 begin:0x1@18");
        }
        // 3. the firmware box unticked: the unit keeps 0.15.0, whose module area is what the modules would go into: refused as before, with its reasons, nothing written
        {
          const {pg, f} = await openFlow({fw: false});
          await goFlow(pg);
          assert.ok(!f.sum && f.fw === 0, "nothing written");
          assert.match(pg.element("models-room").textContent, /^Room: 12 sectors; .*the biggest run the unit's loader can use is 2\. \(The six low sectors are not usable on this unit yet\.\) /, pg.element("models-room").textContent);
          assert.doesNotMatch(pg.element("models-room").textContent, /Placed for the firmware/);
        }
        for (const k of ["modelsAfterFw", "afterFwJunk", "unreadAt", "afterFwLow", "afterFwNoLow", "fwFirstConfirm", "fwFirstJunk", "fwFirstLow", "fwFirstNoLow", "fwFirstDeclined", "sampleLost"])
          assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
      }
      for (const k of ["rearrangeDropConfirm", "dropConfirm", "droppedDone", "giveWayAsk", "giveWayWait", "giveWayChosen", "sectorsWord", "sectorWord"])
        assert.equal((template.match(new RegExp("\\b" + k + ":", "g")) || []).length, 2, k + " in ja and en");
      assert.equal([...template.matchAll(/(rearrangeDropConfirm|dropConfirm): "[^"]*\{drop\}[^"]*\{drop\}[^"]*"/g)].length, 4, "the questions name the module that is removed, in both languages");
    }
  }
  const plain = boot(store());
  await answer(plain, "standard.fwsc");
  assert.equal(plain.element("stage-models").disabled, true, "no master in the release list: the box is off");
  assert.doesNotMatch(plain.element("install-what").textContent, /MACRO models/);
}
// (the review of the picker) the page as make_site.py builds it is the library files joined in front of the page script: one scope, so a name declared in two of them (fm1pick.js
// and the page both had pickItems) is a SyntaxError and the installer is dead. The joined script must compile.
{
  const site = readFileSync(new URL("make_site.py", import.meta.url), "utf8");
  const libs = [...site.matchAll(/strip_module\(\(HERE \/ "(fm1[a-z]+\.js)"\)/g)].map((m) => m[1]);
  assert.ok(libs.includes("fm1place.js") && libs.includes("fm1pick.js") && libs.includes("fm1oc.js"), "make_site.py joins the libraries: " + libs);
  const strip = (src) => src.replace(/^export\s+/gm, "").replace(/^import .*?;\n/gm, "");
  const lib = libs.map((f) => strip(readFileSync(new URL(f, import.meta.url), "utf8"))).join("\n");
  const joined = template.match(/<script type="module">([\s\S]*?)<\/script>/)[1].replace("/*LIB*/", lib).replace("/*META*/{}", "{}");
  assert.doesNotThrow(() => new vm.Script(joined), "the page script with its libraries joined compiles (no name declared twice)");
}
// (Round 32, BUILD.md 7.54) Install decides once, at the start of the press: a press begun with the unit in update mode (this package's loader) resumes, or
// ends with "left update mode" when the loader is gone when the page looks again; never a full install of the now running unit in the same press
{
  const run = async (leaves) => {
    const log = {installs: 0, resumes: 0, finds: 0};
    const Updater = class {
      async find(filter) {
        const id = log.finds++ === 0 || !leaves ? {model: "ota-FM-1", text: MELODEE_LOADER} : {model: "FM-1", text: "FM-1_900"};   // the first look: update mode
        return filter(id) ? {id, link: {close() {}}} : null;
      }
      async install(image, product, step) { log.installs++; step("done"); }
      async resume(image, step) { log.resumes++; step("reboot"); log.said = page.element("status").textContent; step("done"); return true; }
    };
    const page = boot(store(), {model: "FM-1", text: "FM-1_900"}, META, {Updater});
    await answer(page, "standard.fwsc");
    page.element("stage-pack").checked = false; page.element("stage-mod").checked = false; page.element("stage-fx").checked = false; page.element("stage-fw").listeners.change();
    await page.element("go").listeners.click();
    return {log, status: page.element("status").textContent};
  };
  let r = await run(true);
  assert.equal(r.log.installs, 0, "a press begun in update mode never installs in full when the loader has gone");
  assert.equal(r.log.resumes, 0);
  assert.equal(r.status, "The FM-1 left update mode: nothing was written. Press Install again if you want to install.");
  r = await run(false);
  assert.ok(r.log.resumes === 1 && r.log.installs === 0, "the loader still there: the write is resumed");
  assert.equal(r.log.said, "The write finished; the FM-1 is starting the new firmware (its first start after 0.15.0 can take a few minutes): keep it connected.", "while the unit restarts after a resume the page says the write finished");
  // (the review) a press begun with another loader in update mode (the stock one): refused, nothing resumed or installed
  {
    const log = {installs: 0, resumes: 0};
    const Updater = class {
      async find(filter) { const id = {model: "ota-FM-1", text: STOCK_LOADER}; return filter(id) ? {id, link: {close() {}}} : null; }
      async install() { log.installs++; } async resume() { log.resumes++; return true; }
    };
    const page = boot(store(), {model: "FM-1", text: "FM-1_900"}, META, {Updater});
    await answer(page, "standard.fwsc");
    page.element("stage-pack").checked = false; page.element("stage-mod").checked = false; page.element("stage-fx").checked = false; page.element("stage-fw").listeners.change();
    await page.element("go").listeners.click();
    assert.ok(log.installs === 0 && log.resumes === 0, "another loader at the start of the press: nothing written");
    assert.match(page.element("status").textContent, /stock|M-VAVE|official/i, "the page says which loader it refused: " + page.element("status").textContent);
  }
  const ota = readFileSync(new URL("fm1ota.js", import.meta.url), "utf8");
  const {FIRST_START_MS, LATE_BACK_MS} = await import("./fm1ota.js");
  assert.ok(FIRST_START_MS === 300000 && LATE_BACK_MS >= 10 * 60000, "(BUILD.md 7.51 addendum) the new firmware's first start: 5 minutes, then the late wait");
  assert.match(template, /lateStart: "The FM-1 has not answered in 5 minutes\./, "the late text says the bound");
  assert.equal((ota.match(/this\.waitBack\(/g) || []).length, 2, "install() and resume() both wait with waitBack");
}
console.log("installer page: Standard and the Nightly side by side, the choice kept, the boxes are what Install writes, a stock unit takes either in one run, the model picker: passed");
