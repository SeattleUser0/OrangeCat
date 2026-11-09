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

function boot(storage, unit = {model: "FM-1", text: "FM-1_710010300"}, meta = META) {
  const elements = new Map(), pending = [], installs = [], runs = [], unitReads = {n: 0, data: null};
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
    confirm: () => true,
    fetch: (url) => new Promise((resolve) => pending.push({url, resolve})),
    productOf: () => "FM-1_900", logicalImage: (b) => String.fromCharCode(...b), MELODEE_LOADER, STOCK_LOADER,
    Updater: class {
      async find(filter) { return filter({model: unit.model, text: unit.text}) ? {id: {model: unit.model, text: unit.text}, link: {close() {}}} : null; }
      async install(image, product, step, loader) { installs.push({image, product, loader, unit: unit.text}); step("done"); }
      async resume() { return true; }
    },
    runSet: async (ctx) => {                              // the page's one run: the firmware first, then the pack and the module (the set logic is fm1oc.js runSet, tested in test_installer_set.mjs)
      runs.push({stages: ctx.stages, pack: ctx.files.pack && String.fromCharCode(...ctx.files.pack), modules: ctx.files.modules.map((m) => String.fromCharCode(...m)), want: ctx.files.want});
      if (ctx.stages.firmware) await ctx.runFirmware(() => {});
      return {firmware: ctx.stages.firmware ? "installed" : "off", pack: ctx.stages.pack ? "installed" : "off", module: ctx.files.modules.map(() => "installed")};
    },
    OcLink: class { close() {} },
    listUnit: async () => { unitReads.n++; return unitReads.data; },          // (the review) Install reads the unit before it writes; the test says what the unit holds
    Date: class extends Date { static now() { return (unitReads.clock = (unitReads.clock || 0) + 6000); } },   // the page waits for a unit in steps of a second; here the wait runs out at once
    PICK, PLACE,                                                 // (the model picker) the packer and the room meter, inlined into the page by make_site.py
  });
  const script = template.match(/<script type="module">([\s\S]*?)<\/script>/)[1].replace("/*LIB*/", "").replace("/*META*/{}", JSON.stringify(meta));
  vm.runInContext(script, context);
  const tick = () => new Promise((r) => setImmediate(r));
  return {element, pending, installs, runs, tick, context, unitReads};
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
  assert.match(page.element("install-what").textContent, /the SHAPES module, the effect modules \(0\.15\.0\)$/);
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
  const js = new TextEncoder().encode(JSON.stringify(manifest)), jl = (js.length + 3) & ~3, body = new Uint8Array(jl + img0.length);
  body.set(js, 0); body.set(img0, jl);
  const masterBytes = new Uint8Array(24 + body.length), mdv = new DataView(masterBytes.buffer);
  mdv.setUint32(0, 0x4D50434F, true); mdv.setUint32(4, 1, true); mdv.setUint32(8, jl, true); mdv.setUint32(12, img0.length, true); mdv.setUint32(16, 0, true); mdv.setUint32(20, PICK.crc32(body), true);
  masterBytes.set(body, 24);
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
    const unitOf = (mods) => {                     // mods: [[start, engine, state, length, fx?{id, sectors}]] -> what listUnit answers
      const starts = Array.from({length: 18}, () => ({state: 0, why: 0, engine: 0, length: 0})), effects = [], engines = [];
      for (const [st, eng, state, len, fx] of mods) { starts[st] = {state, why: 0, engine: fx ? 127 : eng, length: len}; if (fx) effects.push({id: fx.id, kind: 1, version: [1, 0, 0], count: 1, state, why: 0, start: st, sectors: fx.sectors, resident: false, uses: 0}); else engines.push({num: eng, src: 2, state, start: st}); }
      return {mod: {nstarts: 18, sectors: 18, lowOk: true, starts}, engines: {engines}, fx: {effects}};
    };
    const fresh = async (unit) => {
      const pg = boot(store(), {model: "FM-1", text: "FM-1_710010300"}, {...WITH, releases: [WITH]});
      await answer(pg, "standard.fwsc");
      const i = pg.pending.findIndex((q) => q.url.startsWith("orangecat-plaits-")); const [q] = pg.pending.splice(i, 1); q.resolve({ok: true, arrayBuffer: async () => masterBytes.buffer}); await pg.tick(); await pg.tick();
      pg.element("stage-models").checked = true; pg.element("stage-models").listeners.change();
      pg.element("stage-fx").checked = false; pg.element("stage-fx").listeners.change();
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
console.log("installer page: Standard and the Nightly side by side, the choice kept, the boxes are what Install writes, a stock unit takes either in one run, the model picker: passed");
