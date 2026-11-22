// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Round 31, docs/orangecat/BUILD.md 7.50) The installer page as the owner uses it, in real Chrome (headless, MIDI with SysEx granted): from
// ~/fm1-hw/r22/cdp-install.mjs (Round 9 .. 30), brought into the repository for the Release 3 qualification's install cases (7.37 Q4,
// tools/hw/q4_install.py drives it). What is new:
//  - (Round 31; Round 32: gone, one install attempt per build, --retries other than 1 refused) Install was pressed again, up to --retries times, when the page ended saying the FM-1 was not found or did not come back (its texts
//    notfound, noloader, sNounit, noreturn: "press Install again"); Round 30 saw the headless browser miss the update-mode port twice. Every
//    attempt is logged with its status, its dialogs and its log lines. Chrome's Web MIDI does not see a port that appears after it started.
//    (Round 32, one attempt per build) Within the attempt, once: when the page ends saying the FM-1 in update mode was not found or did not come
//    back, fresh Chromes first read what answers (up to --probe-wait 90 s, read only). Only when the update-mode loader of this package answers and
//    the page's write had not finished does it load the page with the same build and ticks and press Install once more, so that the page's own
//    resume path (web/fm1ota.js resume()) finishes the same install ("attempt N: fresh browser to resume"). A running FM-1 answering ends the
//    attempt, judged by its version (never a press: that would be a new install); a finished write is never followed by a press.
//  - The end of an attempt is the page's own: #release enabled again after it was disabled (lockInstaller(false)), not a text pattern (the
//    old driver missed "Result: ..." and ended on its own timeout while the page had finished).
//  - --stock FILE: the stock section's file input (#stock-file) given the file through DOM.setFileInputFiles, the page's check awaited
//    ("Official V15 verified"), then #stock-go. The page's backup download is allowed into OUT (Browser.setDownloadBehavior).
//  - Every dialog (confirm, alert) is logged with its text and accepted: they are the page's questions. --decline makes it answer no;
//    (Round 32, the owner's rule: nothing waits for a hand) --ask, which put a dialog to the operator, is gone: it is refused (exit 2).
//    --answers FILE (unattended): each dialog answered by the rule whose `starts` its text begins with; an unmatched one is cancelled and
//    stops the run with a non-zero exit (tools/hw/q4_answers_example.json).
//  - The page's versions (#dual-version, #nightly-version, the release picked) are logged; --expect TEXT refuses to press Install when the
//    chosen build's label does not contain TEXT (nothing is written then).
//
//  - (Round 32, 7.37 Q1's two effect levels) --ticks SPEC sets the page's boxes before Install the way the owner does, by clicking them (the page's room
//    meter and give-way choice run as they do for a user; nothing of the page is changed): SPEC is a comma list of +NAME / -NAME, NAME a box of the
//    page's #stages by its id with or without "stage-" (fw, pack, mod, prophet, fx, fx-opt, models) or an effect module by the name the page prints
//    beside its box (#fx-names "(DUB TAPE, FX LAYER)", #fx-opt-names "(REVERB MODES)"; any case, spaces and dashes ignored: fxlayer, reverbmodes).
//    +MODULE ticks its box (and it must not be the one that gives way); -MODULE unticks its box when it is alone there (REVERB MODES), and when it
//    shares the box (FX LAYER beside DUB TAPE) it is the page's give-way choice (#give-way's radio for it: the page offers it only when the ticked set
//    does not fit and leaving that module out makes it fit). The unit is read on the page first ("Read from the unit": the meter places the set on the
//    unit as it stands, and Install keeps the choice only for the unit it was made for). The boxes are logged before and after; a NAME the page does
//    not have, or a box or choice that ends in another state, refuses (Install not pressed).
//  - (Round 32) --remove NAME (an effect module: DUB TAPE, REVERB MODES, FX LAYER), once, before the ticks: the page's "Modules in the unit" section,
//    the module chosen in its Remove list (by its id, the list shows 0x00000003), Remove pressed; the page's question (fxRemoveConfirm, which does not
//    name the module: this driver asks it only with that module chosen, logged) answered as every dialog is; the unit read after must not hold it.
//    Not done when the unit does not hold it (logged). The page's Install never takes the FX layer off (removableFx: only the optional reverb modes).
//
//   node tools/hw/cdp_install.mjs --url URL --build build-dual|build-nightly-dual [--out DIR] [--minutes 25] [--retries 1] [--expect TEXT]
//        [--retry-wait 10] [--return-wait 20] [--probe-wait 90]   (seconds before the fresh browser that reads the unit; the second after "did not come back")
//        [--remove fxlayer] [--ticks +reverbmodes,-fxlayer]
//   node tools/hw/cdp_install.mjs --url URL --stock FM-1.fwsc [--out DIR] [--minutes 25]
// The last line printed is one JSON object: {attempts: [...], final, ok}. Close every other program on the FM-1's port first.
import { spawn } from "child_process";
import { mkdirSync, mkdtempSync, writeFileSync, existsSync, readdirSync, readFileSync } from "fs";
import { tmpdir } from "os";
import { resolve } from "path";

const args = process.argv.slice(2), opt = {};
for (let i = 0; i < args.length; i++) {
  const k = args[i].replace(/^--/, "");
  if (["decline", "ask"].includes(k)) opt[k] = true; else opt[k] = args[++i];
}
if (opt.ask) { console.error("cdp_install: --ask is gone (Round 32: no prompt; every run is unattended): give --answers FILE (tools/hw/q4_answers_example.json)"); process.exit(2); }
// (Round 32, the owner's hard limit: one install attempt per build, SPEC.md 13.1) --retries other than 1 is refused: there is no second attempt
if (opt.retries !== undefined && String(opt.retries).trim() !== "1") { console.error(`cdp_install: --retries ${opt.retries} refused: one install attempt per build (the owner's hard limit, SPEC.md 13.1); only --retries 1 is taken`); process.exit(2); }
const url = opt.url, build = opt.build, minutes = Number(opt.minutes || 25);
// (Round 32) the waits before the fresh browser that reads the unit: --retry-wait S (10) and, after "did not come back" / a finished write, --return-wait S (20)
const retryWait = Number(opt["retry-wait"] ?? 10), returnWait = Number(opt["return-wait"] ?? 20);
const probeWait = Number(opt["probe-wait"] ?? 90);   // (Round 32) how long fresh browsers look for what answers before a resume (each look a new browser)
const out = resolve(opt.out || mkdtempSync(tmpdir() + "/oc-cdp-")), port = Number(opt.port || 9224);
if (!url || (!build && !opt.stock)) { console.error("cdp_install: --url and --build (or --stock FILE) are needed"); process.exit(2); }
if (opt.stock && !existsSync(opt.stock)) { console.error("cdp_install: no file " + opt.stock); process.exit(2); }
// (Round 31, unattended runs) --answers FILE: a JSON list of {starts, answer}: a dialog whose text starts with `starts` (the longest match wins) is
// answered `answer` and logged with the rule; a dialog no rule matches is CANCELLED, its full text printed, and the run stops non-zero once the
// page has finished what it is doing (no retry): never a blind yes
let rules = null;
if (opt.answers) {
  rules = JSON.parse(readFileSync(opt.answers, "utf8"));
  if (!Array.isArray(rules) || !rules.every((r) => typeof r.starts === "string" && r.starts.trim() && typeof r.answer === "boolean")) {
    console.error("cdp_install: --answers must be a JSON list of {\"starts\": text, \"answer\": true|false}"); process.exit(2);
  }
}
// (Round 32) --ticks / --remove, checked for form here; the names against the page once it is loaded
const nkey = (s) => String(s).toUpperCase().replace(/[\s_-]+/g, "");
const FX_IDS = { DUBTAPE: 1, REVERBMODES: 2, FXLAYER: 3 };        // the page's effect modules by id (web/make_site.py FX_NAMES; the page's blockedOne)
const hex8 = (v) => "0x" + (v >>> 0).toString(16).toUpperCase().padStart(8, "0");   // as the page lists them (#unit-fx, #fx-remove-id)
let ticks = null;
if (opt.ticks !== undefined) {
  ticks = String(opt.ticks || "").split(",").map((x) => x.trim()).filter(Boolean).map((x) => { const m = /^([+-])\s*([A-Za-z0-9 _-]+)$/.exec(x); return m && { token: x, want: m[1] === "+", name: m[2].trim() }; });
  if (!ticks.length || ticks.some((x) => !x)) { console.error("cdp_install: --ticks is a comma list of +NAME / -NAME (e.g. +reverbmodes,-fxlayer)"); process.exit(2); }
  const ks = ticks.map((x) => nkey(x.name)); if (new Set(ks).size !== ks.length) { console.error("cdp_install: --ticks names a box twice: " + opt.ticks); process.exit(2); }
}
if ((ticks || opt.remove) && opt.stock) { console.error("cdp_install: --ticks and --remove are for --build (the stock section has no boxes)"); process.exit(2); }
if (opt.remove !== undefined && !(nkey(opt.remove) in FX_IDS)) { console.error(`cdp_install: --remove ${opt.remove}: not an effect module of the page (DUB TAPE, REVERB MODES, FX LAYER)`); process.exit(2); }
let removedOnce = false, removePressed = null, removedName = null;   // (review of f7f81713) what a refusal after the Remove leaves: named in the error
const FX_NAMES = { 1: "DUB TAPE", 2: "REVERB MODES", 3: "FX LAYER" };
const norm = (x) => String(x).replace(/\s+/g, " ").trim();
const ruleFor = (text) => rules.filter((r) => norm(text).startsWith(norm(r.starts))).sort((a, b) => norm(b.starts).length - norm(a.starts).length)[0] || null;
const unmatched = [];
mkdirSync(out, { recursive: true });
const dl = resolve(out, "downloads"); mkdirSync(dl, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const say = (...a) => console.log(new Date().toISOString().slice(11, 19), ...a);
// (Round 32) a fresh Chrome for the read before a resume: Chrome's Web MIDI does not see a port that appears after the browser started, so the update-mode
// port ("Orange Cat FM-1 Update") and the normal port after the firmware's restart were never found by the browser that had pressed Install
// (Round 31, twice on the unit: "The FM-1 in update mode was not found", "FM-1 not found" on every retry, "The FM-1 did not come back"; a new
// driver run installed at once). openBrowser: a new user-data dir, a new process, MIDI (with SysEx) granted, the downloads into OUT, the page
// loaded; closeBrowser ends the old one first (its exit awaited: it lets go of the port). A fresh browser starts only once nothing answers on the
// debugging port, so a browser that lingers is never the one talked to.
let chrome = null, ws = null, id = 0, pending = new Map(), dialogs = [];
const closeBrowser = async () => {
  try { ws && ws.close(); } catch (_) {}
  ws = null;
  const c = chrome; chrome = null;
  if (!c || c.exitCode !== null || c.signalCode !== null) return;
  const gone = new Promise((r) => c.once("exit", r));
  c.kill();
  if (await Promise.race([gone.then(() => true), sleep(10000).then(() => false)])) return;
  c.kill("SIGKILL"); await Promise.race([gone, sleep(5000)]);
};
const answers = async () => { try { await fetch(`http://127.0.0.1:${port}/json/version`); return true; } catch (_) { return false; } };
const openBrowser = async () => {
  for (let i = 0; i < 50 && (await answers()); i++) await sleep(200);
  if (await answers()) throw new Error(`another program answers on the debugging port ${port}: not the fresh browser`);
  const dir = mkdtempSync(tmpdir() + "/oc-chrome-");
  chrome = spawn(process.env.CHROME || "/usr/bin/google-chrome", ["--headless=new", "--no-first-run", `--remote-debugging-port=${port}`, `--user-data-dir=${dir}`, "about:blank"], { stdio: "ignore" });
  let targets;
  for (let i = 0; i < 75 && !(targets && targets.length); i++) { await sleep(200); try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); } catch (_) {} }
  if (!targets || !targets.length) throw new Error("Chrome did not open its debugging port");
  ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
  await new Promise((r, j) => { ws.onopen = r; ws.onerror = () => j(new Error("Chrome's page did not take the debugging connection")); });
  pending = new Map();
  ws.onmessage = onMessage;
  await send("Browser.grantPermissions", { origin: new URL(url).origin, permissions: ["midi", "midiSysex"] });
  await send("Browser.setDownloadBehavior", { behavior: "allow", downloadPath: dl });
  await send("Page.enable"); await send("Runtime.enable"); await send("DOM.enable");
  await send("Page.navigate", { url });
  await sleep(4000);
};
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
const onMessage = async (m) => {
  const d = JSON.parse(m.data);
  if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); }
  if (d.method === "Page.javascriptDialogOpening") {
    let accept, by, rule = null;
    if (rules) {
      rule = ruleFor(d.params.message);
      if (rule) { accept = rule.answer; by = "rule"; }
      else {
        accept = false; by = "UNMATCHED: cancelled, the run stops";
        unmatched.push(d.params.message);
        console.error(`\ncdp_install: A DIALOG NO RULE MATCHES (${d.params.type}), answered Cancel; the run stops when the page has finished:\n${d.params.message}\n`);
      }
    } else if (d.params.type === "alert") { accept = true; by = "driver (an alert has only OK)"; }
    else { accept = !opt.decline; by = "driver"; }
    dialogs.push({ at: new Date().toISOString(), type: d.params.type, text: d.params.message, answered: accept ? "yes" : "no", by, rule: rule && rule.starts });
    say(`DIALOG (${d.params.type}, answered ${accept ? "yes" : "no"}):`, d.params.message.slice(0, 300));
    send("Page.handleJavaScriptDialog", { accept });
  }
};
const ev = async (e) => (await send("Runtime.evaluate", { expression: e, awaitPromise: true, returnByValue: true })).result?.result?.value;
const txt = (idq) => ev(`(document.getElementById(${JSON.stringify(idq)})||{}).textContent`);
const RETRY = /not found|did not come back/i;
// (Round 32) the page's ends that a fresh browser resumes within the attempt: TEXT.noloader and TEXT.noreturn (en, ja); fm1ota.js's own noreturn too
const RESUME = /The FM-1 in update mode was not found|書き込みモードの FM-1 が見つかりません|did not come back|did not return after resuming|FM-1 が戻ってきません/i;
// (Round 32) the update loader this run's package goes to (web/fm1ota.js MELODEE_LOADER, STOCK_LOADER)
const ownLoader = opt.stock ? "ota-FM-1_015" : "ota-FM-1_900";
// (Round 32) what answers in this browser, read only, as the page's Updater.find asks (the update handshake, its identity) and, for a running FM-1, the
// editor's INFO (its version string): {ports: [{port, identity, model, version}]}; the ports the page would probe only
const probeUnit = () => ev(`(async () => {
  const unpack7 = (s) => { const o = []; let acc = 0, nb = 0; for (const b of s) { acc |= (b & 0x7F) << nb; nb += 7; while (nb >= 8) { o.push(acc & 0xFF); acc >>>= 8; nb -= 8; } } return o; };
  const ident = (p) => { const d = unpack7(p.slice(1, -1)); if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
    const t = String.fromCharCode(...d.slice(6, 33)).replace(/\\0+$/, ""), m = /^([^_]+)_(\\d+)$/.exec(t); return m ? { text: t, model: m[1] } : null; };
  const info = (p) => p.length > 6 && p[1] === 125 && p[2] === 70 && p[3] === 76 && p[4] === 1 ? String.fromCharCode(...p.slice(5, -1).slice(0, Math.max(0, p.slice(5, -1).indexOf(0)))) || null : null;
  let acc; try { acc = await navigator.requestMIDIAccess({ sysex: true }); } catch (e) { return { error: "no MIDI: " + e.message, ports: [] }; }
  const outs = [...acc.outputs.values()], ports = [];
  for (const input of acc.inputs.values()) {
    if (input.state === "disconnected" || !/fm-1|orange cat|melodee|felucca|ota|composite|sinco|usb-midi/i.test(input.name || "")) continue;
    const output = outs.find((o) => o.name === input.name && o.state !== "disconnected"); if (!output) continue;
    const q = []; let w = null;
    input.onmidimessage = (e) => { if (e.data[0] !== 0xF0) return; q.push(Array.from(e.data)); if (w) { const f = w; w = null; f(); } };
    const read = async (ms) => { if (!q.length) { let tm; await Promise.race([new Promise((r) => { w = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]); clearTimeout(tm); w = null; } return q.shift() || null; };
    const ask = async (bytes, parse) => { q.length = 0; for (let t = 0; t < 3; t++) { try { output.send(bytes); } catch (_) { return null; }
      const end = Date.now() + 1000; while (Date.now() < end) { const p = await read(end - Date.now()); if (!p) break; const v = parse(p); if (v) return v; } } return null; };
    const id = await ask([0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7], ident);
    const version = id && id.model === "FM-1" ? await ask([240, 125, 70, 76, 1, 247], info) : null;
    input.onmidimessage = null;
    ports.push({ port: input.name, identity: id ? id.text : null, model: id ? id.model : null, version });
  }
  return { ports };
})()`);
// a running FM-1 against the build: its INFO version's X.Y.Z(-hash) and the build's (--expect, else the label the page offered), the hashes the shorter a
// prefix of the longer, 5 digits at least; no hash on either side (q4_install.py version_matches)
const verOf = (x) => { const m = /(\d+\.\d+\.\d+)(?:-([0-9a-f]+))?/i.exec(x || ""); return m ? { xyz: m[1], hash: (m[2] || "").toLowerCase() } : null; };
const judgeUnit = (unit) => {
  const want = verOf(opt.expect) || verOf(result.chosen && result.chosen.label), got = verOf(unit.version);
  const h1 = got && got.hash, h2 = want && want.hash;
  const same = !!(want && got && want.xyz === got.xyz && ((!h1 && !h2) || (h1 && h2 && Math.min(h1.length, h2.length) >= 5 && (h1.startsWith(h2) || h2.startsWith(h1)))));
  return `the FM-1 answers running (${unit.identity}, INFO ${JSON.stringify(unit.version)}): ${!got ? "its version unknown" : !want ? "the build's version unknown" : same ? "the build's version (" + (opt.expect || result.chosen.label) + ")" : "NOT the build's version (" + (opt.expect || result.chosen.label) + ")"}; the rest of the set was not installed by this attempt; Install not pressed`;
};
// (Round 31) the page's own ends of an Install: TEXT.sumTitle and the summary ("Result: ...", ja "結果: "), or TEXT.done alone ("Done", ja "完了しました"; transitDone)
const finishedText = (s) => /^(Result: |結果: )/.test((s || "").trim()) || /^(Done|完了しました)$/.test((s || "").trim()) || /^TransitFM1 installed\.|^TransitFM1をインストールしました/.test((s || "").trim());
// (Round 31, the review) the parts of a "Result: ..." line that are not a success: every "Firmware/Pack/Module: X" whose X is not installed, already current, not chosen or
// "this release has none", and every other line but the done rearrange and the named removal (e.g. "No place is free for the module.", "... has no place for it. Nothing was
// written", "not for this firmware", "... REMOVE IT FIRST", the roll back, a declined or stopped rearrange). [] for a clean result
// (Round 32) the plan the page wrote by: its log's "plan: NAME at start N, ... | kept: NAME at start N, ..." line (web/fm1oc.js planLine; the last one of
// an attempt) -> [{name, start, kept}] or null when the page printed none (a unit with 18 starts, or a page before Round 32)
const planOf = (lines) => { const l = [...(lines || [])].reverse().find((x) => /^plan: /.test(x)); if (!l) return null; const [w, k] = l.slice(6).split(" | kept: "); const one = (t, kept) => (t || "").split(", ").map((x) => /^(.+) at start (-?\d+)$/.exec(x.trim())).filter(Boolean).map((m) => ({ name: m[1], start: Number(m[2]), kept })); return [...one(w, false), ...one(k, true)]; };
const resultProblems = (s) => {
  const t = (s || "").trim().replace(/^(Result: |結果: )/, "");
  const okVal = /^(installed|already current, left as it is|not chosen|this release has none|インストールしました|すでに最新です（そのまま）|選びませんでした|このリリースにはありません)$/;
  const okLine = /^(The modules were taken off and installed again in an order that fits\.|Removed to make room, not put back|モジュールをいったん消し、入る順番で入れ直しました。|空きを作るために消しました)/;
  return t.split(" | ").filter((part) => { const m = /^(Firmware|Pack|Module|ファームウェア|パック|モジュール): (.*)$/.exec(part); return m ? !okVal.test(m[2].trim()) : !okLine.test(part.trim()); });
};
const result = { url, build: build || null, stock: opt.stock || null, out, attempts: [], ticksSpec: opt.ticks || null, removeSpec: opt.remove || null };
// (Round 32) --ticks / --remove. The boxes as the page shows them: every #stages box "stage-*" (checked, disabled, the module names beside it), the
// give-way radios (#give-way), what Install will write (#install-what), the meter's line (#models-room), Install enabled
const boxesNow = () => ev(`(()=>{const q=(s)=>[...document.querySelectorAll(s)], tx=(id)=>(document.getElementById(id)||{}).textContent||"";
  return {boxes: q('#stages input[type=checkbox][id^="stage-"]').map((c)=>{const n=c.closest('label')&&c.closest('label').querySelector('span[id$="-names"]');
      return {id: c.id, checked: c.checked, disabled: c.disabled, modules: n ? n.textContent.replace(/^\\s*\\(|\\)\\s*$/g, "").split(",").map((x)=>x.trim()).filter(Boolean) : []};}),
    giveWay: q('#give-way input[type=radio]').map((r)=>({id: r.id, label: ((r.closest('label')||{}).textContent||"").trim(), checked: r.checked, disabled: r.disabled})),
    giveWayShown: !!document.getElementById('give-way') && !document.getElementById('give-way').hidden,
    installWhat: tx('install-what'), room: tx('models-room'), goDisabled: document.getElementById('go').disabled};})()`);
// a page job that locks the installer (Read from the unit, Remove): clicked, its lock seen (up to 10 s), its end awaited (#release enabled again, 2 min)
const pageJob = async (clickJs) => {
  await ev(clickJs);
  let seen = false;
  for (let i = 0; i < 100 && !seen; i++) { seen = (await ev("document.getElementById('release').disabled")) === true; if (!seen) await sleep(100); }
  for (let i = 0; i < 1200; i++) { if ((await ev("document.getElementById('release').disabled")) === false) break; await sleep(100); }
  if ((await ev("document.getElementById('release').disabled")) !== false) throw new Error("the page's unit job did not end in 2 minutes: " + (await txt("status")));
  return seen;
};
const readUnitOnPage = async () => {
  const before = await txt("unit-fx");
  const seen = await pageJob("document.getElementById('unit-read').click()");
  const fx = (await txt("unit-fx")) || "";
  say("the unit as the page read it (Effects):", JSON.stringify(fx), "| status:", await txt("status"));
  if (!fx.trim() || (!seen && fx === before)) throw new Error("the page did not read the unit (Read from the unit): " + (await txt("status")));
  return fx;
};
// a NAME of --ticks against the page's boxes: {kind: "box", box} or {kind: "module", box, module, shared}; null when the page has no such box or module
const resolveName = (state, name) => {
  const k = nkey(name);
  const box = state.boxes.find((b) => nkey(b.id) === k || nkey(b.id.replace(/^stage-/, "")) === k);
  if (box) return { kind: "box", box: box.id };
  const holder = state.boxes.find((b) => b.modules.some((m) => nkey(m) === k));
  if (holder) return { kind: "module", box: holder.id, module: holder.modules.find((m) => nkey(m) === k), shared: holder.modules.length > 1 };
  return null;
};
const radioFor = (state, module) => state.giveWay.find((r) => r.label === module || r.label.startsWith(module + " ("));
const setBoxes = async (rec, resume = false) => {
  rec.ticks = { spec: opt.ticks || null, remove: opt.remove || null };
  rec.ticks.initial = await boxesNow();
  say("the boxes as the page opened:", JSON.stringify(rec.ticks.initial));
  // every name resolved before anything is clicked or written
  const plan = (ticks || []).map((x) => ({ ...x, on: resolveName(rec.ticks.initial, x.name) }));
  const missing = plan.filter((x) => !x.on).map((x) => x.token);
  if (missing.length) {
    throw new Error(`--ticks: the page has no box or module ${missing.join(", ")} (its boxes: ${rec.ticks.initial.boxes.map((b) => b.id.replace(/^stage-/, "") + (b.modules.length ? " (" + b.modules.join(", ") + ")" : "")).join("; ")}): Install not pressed, nothing written`);
  }
  const boxWant = {};
  for (const x of plan) {
    if (x.on.kind === "module" && !x.want && x.on.shared) continue;      // the give-way choice, below
    const b = x.on.box;
    if (b in boxWant && boxWant[b] !== x.want) throw new Error(`--ticks: ${opt.ticks} wants the box ${b} both ticked and unticked: refused`);
    boxWant[b] = x.want;
  }
  // (Round 32) a fresh browser to resume: the unit waits in update mode, the page cannot read it (nor remove anything): the boxes only, judged as always
  if (resume) rec.ticks.unitBefore = null;
  else rec.ticks.unitBefore = await readUnitOnPage();                  // the meter and Install's give-way choice keyed on the unit as it stands
  if (opt.remove && !removedOnce && !resume) {
    const id = FX_IDS[nkey(opt.remove)];
    rec.ticks.removal = { module: opt.remove, id: hex8(id) };
    if (!rec.ticks.unitBefore.includes(hex8(id))) {
      rec.ticks.removal.done = false; rec.ticks.removal.why = "the unit does not hold it (its Effects as read: nothing to remove)";
      say(`--remove ${opt.remove}: ${rec.ticks.removal.why}`);
    } else {
      const sel = await ev(`(()=>{const s=document.getElementById('fx-remove-id'); if(!s) return null; const ok=[...s.options].some((o)=>o.value===${JSON.stringify(String(id))}); if(!ok) return {options:[...s.options].map((o)=>o.textContent)};
        s.value=${JSON.stringify(String(id))}; s.dispatchEvent(new Event('change', {bubbles: true})); return {value: s.value, text: s.options[s.selectedIndex].textContent, disabled: document.getElementById('fx-remove').disabled};})()`);
      if (!sel || sel.value !== String(id) || sel.disabled) throw new Error(`--remove ${opt.remove}: the page's Remove list does not offer ${hex8(id)} (${JSON.stringify(sel)}): nothing written`);
      rec.ticks.removal.chosen = sel.text;
      say(`--remove ${opt.remove}: ${sel.text} chosen in the page's Remove list, Remove pressed`);
      const d0 = dialogs.length;
      removedOnce = true;                                                 // (a removal is pressed at most once in a run: a retry's browser only reads the unit)
      removePressed = FX_NAMES[id];
      await pageJob("document.getElementById('fx-remove').click()");
      rec.ticks.removal.dialogs = dialogs.slice(d0);
      if (unmatched.length || !rec.ticks.removal.dialogs.length || rec.ticks.removal.dialogs.some((d) => d.answered !== "yes")) removePressed = null;   // (the page's question answered no: the page removed nothing)
      if (unmatched.length) throw new Error(`--remove ${opt.remove}: the page's question had no --answers rule (answered Cancel): nothing removed`);
      if (!rec.ticks.removal.dialogs.length || rec.ticks.removal.dialogs.some((d) => d.answered !== "yes")) throw new Error(`--remove ${opt.remove}: the page's question was not answered yes (${JSON.stringify(rec.ticks.removal.dialogs)}): nothing removed`);
      rec.ticks.removal.status = await txt("status");
      rec.ticks.removal.unitAfter = (await txt("unit-fx")) || "";
      say(`--remove ${opt.remove}: ${rec.ticks.removal.status} | the unit's Effects now: ${JSON.stringify(rec.ticks.removal.unitAfter)}`);
      if (rec.ticks.removal.unitAfter.includes(hex8(id))) throw new Error(`--remove ${opt.remove}: the unit still holds ${hex8(id)} after the page's Remove: ${rec.ticks.removal.status}`);
      rec.ticks.removal.done = true;
      removedName = FX_NAMES[id];
      rec.ticks.unitBefore = rec.ticks.removal.unitAfter;
    }
  }
  // the boxes, clicked as the owner clicks them (one at a time, the page's own change handlers run), then the give-way choices
  for (const [b, want] of Object.entries(boxWant)) {
    const r = await ev(`(()=>{const c=document.getElementById(${JSON.stringify(b)}); if(c.checked===${want}) return "as wanted"; if(c.disabled) return "disabled"; c.click(); return c.checked===${want} ? "clicked" : "did not change";})()`);
    say(`--ticks: ${b} ${want ? "ticked" : "unticked"}: ${r}`);
    await sleep(400);
  }
  for (const x of plan.filter((y) => y.on.kind === "module" && !y.want && y.on.shared)) {
    const st = await boxesNow(), radio = radioFor(st, x.on.module);
    if (!radio) throw new Error(`--ticks ${x.token}: ${x.on.module} shares its box (${x.on.box}) and the page does not offer it to give way (offered: ${st.giveWay.map((r) => r.label).join("; ") || "none"}; the meter: ${st.room}): Install not pressed`);
    const r = await ev(`(()=>{const c=document.getElementById(${JSON.stringify(radio.id)}); if(c.checked) return "as wanted"; if(c.disabled) return "disabled"; c.click(); return c.checked ? "clicked" : "did not change";})()`);
    say(`--ticks: ${x.on.module} gives way (${radio.id}): ${r}`);
    await sleep(400);
  }
  // the end state, judged token by token
  const end = rec.ticks.after = await boxesNow();
  say("the boxes before Install:", JSON.stringify(end));
  const bad = [];
  for (const x of plan) {
    const b = end.boxes.find((y) => y.id === x.on.box);
    if (x.on.kind === "module" && !x.want && x.on.shared) {
      const radio = radioFor(end, x.on.module);
      if (!radio || !radio.checked || !end.installWhat.includes(x.on.module + " gives way")) bad.push(`${x.token}: ${x.on.module} is not the module that gives way (${JSON.stringify(radio || null)}; ${end.installWhat})`);
    } else if (!b || b.checked !== x.want || (x.want && b.disabled)) bad.push(`${x.token}: ${x.on.box} is ${b ? (b.checked ? "ticked" : "unticked") + (b.disabled ? ", disabled" : "") : "gone"}`);
    else if (x.on.kind === "module" && x.want && (end.installWhat.includes(x.on.module + " gives way") || (radioFor(end, x.on.module) || {}).checked)) bad.push(`${x.token}: ${x.on.module} is the module that gives way`);
  }
  rec.ticks.ok = !bad.length;
  if (bad.length) throw new Error(`--ticks ${opt.ticks}: the page's boxes ended in another state: ${bad.join(" | ")}: Install not pressed`);
};
// the page as loaded in a (fresh) browser, made ready to press: its versions, the stock file or the build chosen, --expect; into `rec` (the result for
// the first browser, the attempt's own record for a retry's); the button to press
const preparePage = async (rec, resume = false) => {
  rec.page = { title: await ev("document.title"), status: await txt("status"), dual: await txt("dual-version"), nightly: await txt("nightly-version"),
    release: await ev("(document.getElementById('release')||{}).value") };
  say("page:", JSON.stringify(rec.page));
  let button = "go";
  if (opt.stock) {
    button = "stock-go";
    const doc = await send("DOM.getDocument", { depth: -1 });
    const q = await send("DOM.querySelector", { nodeId: doc.result.root.nodeId, selector: "#stock-file" });
    if (!q.result || !q.result.nodeId) throw new Error("no #stock-file on the page");
    await send("DOM.setFileInputFiles", { nodeId: q.result.nodeId, files: [resolve(opt.stock)] });
    for (let i = 0; i < 60; i++) { if ((await ev("document.getElementById('stock-go').disabled")) === false) break; await sleep(500); }
    rec.stockCheck = await txt("status");
    say("stock file:", rec.stockCheck);
    if (await ev("document.getElementById('stock-go').disabled")) throw new Error("the page did not accept the stock file: " + rec.stockCheck);
  } else {
    await ev(`(()=>{const r=document.getElementById('${build}'); if(!r) return false; r.checked=true; r.dispatchEvent(new Event('change')); return true;})()`);
    for (let i = 0; i < 90; i++) { if ((await ev("document.getElementById('go').disabled")) === false) break; await sleep(1000); }
    const label = await ev(`(()=>{const r=document.getElementById('${build}'); return r ? r.closest('label').textContent : null;})()`);
    rec.chosen = { label, status: await txt("status"), what: await txt("install-what"),
      stages: await ev(`[...document.querySelectorAll('#stages input[type=checkbox]')].map(c => c.id + (c.checked ? ':on' : ':off') + (c.disabled ? ':disabled' : ''))`) };
    say("chosen:", JSON.stringify(rec.chosen));
    if (opt.expect && !(label || "").includes(opt.expect)) throw new Error(`the build's label ${JSON.stringify(label)} does not contain ${JSON.stringify(opt.expect)}: Install not pressed`);
    if (resume && rec.chosen.label !== result.chosen.label) throw new Error(`the fresh browser's page offers ${JSON.stringify(rec.chosen.label)} as ${build}, the install began with ${JSON.stringify(result.chosen.label)}: not pressed (another build is never started)`);
    if (ticks || opt.remove) await setBoxes(rec, resume);
    if (await ev("document.getElementById('go').disabled")) throw new Error("Install stayed disabled: " + rec.chosen.status + (ticks ? " | the meter: " + (rec.ticks && rec.ticks.after && rec.ticks.after.room) : ""));
  }
  return button;
};
// one press of the page's button and its end (#release enabled again), the page's log lines of it
const press = async (k, button) => {
  const p = {};
  dialogs = [];
  const logStart = (await txt("log") || "").split("\n").length;
  await ev(`document.getElementById('${button}').click()`);
  p.started = new Date().toISOString();
  let started = false, last = "";
  for (let i = 0; i < 30 && !started; i++) { await sleep(500); started = (await ev("document.getElementById('release').disabled")) === true; }
  while (Date.now() < end) {
    await sleep(3000);
    const st = await txt("status"), bar = await ev("document.getElementById('bar').value");
    if (st !== last) { say(`attempt ${k} bar ${Math.round(bar)} | ${st}`); last = st; }
    if ((await ev("document.getElementById('release').disabled")) === false) break;
  }
  p.ended = new Date().toISOString();
  p.status = await txt("status");
  p.timedOut = (await ev("document.getElementById('release').disabled")) === true;
  p.dialogs = dialogs;
  p.log = (await txt("log") || "").split("\n").slice(logStart - 1);
  return p;
};
// an attempt's counts from its log lines (both presses when it resumed)
const derive = (a) => {
  const lines = a.log;
  a.fwWrites = lines.filter((l) => /^write \d+/.test(l)).length;
  a.fwWriteMax = Math.max(0, ...lines.map((l) => (/^write (\d+)/.exec(l) || [0, 0])[1]).map(Number));
  a.summary = lines.filter((l) => /^(Firmware|Pack|Module|Effect|Models|Result)/i.test(l));
  a.plan = planOf(lines);
};
let end = 0;
try {
  await openBrowser();
  let button = await preparePage(result);
  end = Date.now() + minutes * 60000;
  {                                                      // (Round 32, the owner's hard limit) ONE attempt: no second press of Install outside the read-checked resume below
    const k = 1, a = { attempt: k };
    const p1 = await press(k, button);
    Object.assign(a, p1);
    derive(a);
    result.attempts.push(a);
    // (Round 32, the owner's limit: one install attempt per build) WITHIN the attempt, once: the page switched the FM-1 to update mode and its browser
    // did not see that port ("The FM-1 in update mode was not found"), or the firmware restarted and the browser did not see it come back ("did not
    // come back"): Chrome's Web MIDI sees only the ports present when it started. Chrome closed, a fresh one, the page loaded with the same build (and
    // ticks), Install pressed once more: the page's own path finds the FM-1 already in update mode and resumes (web/fm1ota.js resume(): this package's
    // loader only), or finds this firmware running and goes on with the rest. Any other end (a refusal, "FM-1 not found" before anything) is the end.
    if (!unmatched.length && !p1.timedOut && RESUME.test(p1.status || "")) {
      // (Round 32, after the unit: 0c86c900 pressed Install in a fresh browser after "did not return after resuming": the page found the FM-1 running and
      // switched it to update mode again, a second install) FIRST the fresh browser reads what answers, up to --probe-wait (90 s; each look a new
      // browser, since a browser sees only the ports present when it started), read only (the update handshake's identity, the editor's INFO).
      // Install is pressed only when the update-mode loader of this package answers and the page's write did not finish (no "reboot" in its log).
      // A normal FM-1 answering ends the attempt: judged by its version against the build's. A finished write is never followed by a press.
      const wrote = p1.log.some((l) => /^reboot\b/.test(l));
      const wait = wrote || /did not come back|did not return|戻ってきません/.test(p1.status) ? returnWait : retryWait;
      say(`attempt ${k}: the page says ${JSON.stringify(p1.status.slice(0, 90))}${wrote ? " after a finished write" : ""}: the browser closed, a fresh browser reads the unit in ${wait} s`);
      a.resume = { after: p1.status, wroteAll: wrote, freshBrowser: true, wait, looks: [] };
      await closeBrowser();
      await sleep(wait * 1000);
      let unit = null;
      try {
        const until = Date.now() + probeWait * 1000;
        for (;;) {
          await openBrowser();
          const look = await probeUnit();
          a.resume.looks.push({ at: new Date().toISOString(), ...look });
          say(`attempt ${k}: fresh browser reads the unit: ${JSON.stringify(look)}`);
          unit = (look.ports || []).find((x) => x.identity) || null;
          if (unit || Date.now() >= until) break;
          await closeBrowser();
          await sleep(5000);
        }
      } catch (e) { a.resume.error = String(e && e.message || e); throw e; }
      a.resume.unit = unit;
      const isLoader = unit && /^ota-/i.test(unit.model || "");
      if (!wrote && isLoader && unit.identity === ownLoader) {
        say(`attempt ${k}: fresh browser to resume (${unit.identity} answers: the page's resume path)`);
        a.resume.pressed = true;
        let p2;
        try { p2 = await press(k, await preparePage(a.resume, true)); }
        catch (e) { a.resume.error = String(e && e.message || e); throw e; }   // (the attempt as it ended before, the resume's error named)
        Object.assign(a.resume, { started: p2.started, ended: p2.ended, status: p2.status, timedOut: p2.timedOut });
        a.ended = p2.ended; a.status = p2.status; a.timedOut = p2.timedOut;
        a.dialogs = [...p1.dialogs, ...p2.dialogs];
        a.log = [...p1.log, `--- attempt ${k}: fresh browser to resume ---`, ...p2.log];
      } else {
        a.resume.pressed = false;
        a.resume.verdict = !unit ? `nothing answered in ${probeWait} s: Install not pressed`
          : isLoader ? `${unit.identity} answers (${wrote ? "after a finished write" : unit.identity === ownLoader ? "" : "not this package's loader " + ownLoader}): Install not pressed`
          : judgeUnit(unit);
        say(`attempt ${k}: the attempt is over, no Install pressed: ${a.resume.verdict}`);
        a.log = [...p1.log, `--- attempt ${k}: fresh browser read the unit, no Install pressed: ${a.resume.verdict} ---`];
      }
      derive(a);
    }
    writeFileSync(resolve(out, `page-log-attempt${k}.txt`), a.log.join("\n"));
    say(`attempt ${k} ended: ${a.status}${a.timedOut ? " (the page was still working at the time limit)" : ""}`);
    if (unmatched.length) a.stoppedByUnmatchedDialog = true;
  }
  result.downloads = readdirSync(dl);
  result.final = result.attempts.length ? result.attempts[result.attempts.length - 1].status : null;
  result.plan = [...result.attempts].reverse().map((x) => x.plan).find(Boolean) || null;   // (Round 32) the page's plan of the last attempt that printed one
  result.unmatched = unmatched;
  // (Round 31, found in the Q4 rehearsal: the 0.15.0 -> Nightly install was refused by the page's room meter before any write and reported ok) an attempt is ok only when the
  // page ended it with its own result: the summary line ("Result: ...") or "Done" (a firmware alone, the stock install). Any other end (a refusal: the room meter's text, a
  // module in the way, "Install waits until you choose") is NOT ok, with the page's text as the error
  result.finished = finishedText(result.final);
  result.problems = result.finished && /^(Result: |結果: )/.test((result.final || "").trim()) ? resultProblems(result.final) : [];   // (the review) a part that failed is not ok either
  result.ok = !unmatched.length && !!result.attempts.length && !result.attempts[result.attempts.length - 1].timedOut && result.finished && !result.problems.length && !RETRY.test(result.final || "") && !/error|stopped|refus|could not|failed/i.test(result.final || "");
  if (!result.ok && result.attempts.length && !result.finished && !result.attempts[result.attempts.length - 1].timedOut) result.error = "the page ended without a result (nothing it reports as done): " + (result.final || "(no status)");
  const rv = result.attempts.length && result.attempts[result.attempts.length - 1].resume;
  if (rv && rv.verdict) { result.unitAfter = rv.unit; result.error = (result.error || "not ok") + " | the fresh browser read the unit: " + rv.verdict; }
  else if (!result.ok && result.problems.length) result.error = "the page's result has parts that did not install: " + result.problems.join(" | ");
} catch (e) {
  result.error = String(e && e.message || e);
  result.ok = false;
  result.unmatched = unmatched;
  say("ERROR:", result.error);
} finally {
  await closeBrowser();
}
// (review of f7f81713) a run that pressed the page's Remove and did not end ok says what it left on the unit and the way back, in its error
if (!result.ok && (removedName || removePressed)) {
  const nm = removedName || removePressed;
  result.removedOffUnit = nm; result.removeConfirmed = !!removedName;
  result.error = (result.error || result.final || "not ok") + ` | ${nm} ${removedName ? "is off the unit" : "may be off the unit (Remove was pressed, its end not confirmed)"}: the page's Remove took it before this run ended. The way back: the page's Install with the default ticks (q4_install.py OUT LABEL fx-set --expect V), or restore the backup taken before this step`;
  say("NOTE:", result.error);
}
writeFileSync(resolve(out, "cdp-result.json"), JSON.stringify(result, null, 1));
console.log(JSON.stringify(result));
process.exitCode = result.ok ? 0 : 1;
