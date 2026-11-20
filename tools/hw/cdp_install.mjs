// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Round 31, docs/orangecat/BUILD.md 7.50) The installer page as the owner uses it, in real Chrome (headless, MIDI with SysEx granted): from
// ~/fm1-hw/r22/cdp-install.mjs (Round 9 .. 30), brought into the repository for the Release 3 qualification's install cases (7.37 Q4,
// tools/hw/q4_install.py drives it). What is new:
//  - Install is pressed again, up to --retries times (3), when the page ends saying the FM-1 was not found or did not come back (its texts
//    notfound, noloader, sNounit, noreturn: "press Install again"); Round 30 saw the headless browser miss the update-mode port twice. Every
//    attempt is logged with its status, its dialogs and its log lines. (Round 32) Every retry runs in a fresh Chrome (the old one closed first,
//    a new profile and process, the page loaded and made ready again): Chrome's Web MIDI does not see a port that appears after it started.
//  - The end of an attempt is the page's own: #release enabled again after it was disabled (lockInstaller(false)), not a text pattern (the
//    old driver missed "Result: ..." and ended on its own timeout while the page had finished).
//  - --stock FILE: the stock section's file input (#stock-file) given the file through DOM.setFileInputFiles, the page's check awaited
//    ("Official V15 verified"), then #stock-go. The page's backup download is allowed into OUT (Browser.setDownloadBehavior).
//  - Every dialog (confirm, alert) is logged with its text and accepted: they are the page's questions. --decline makes it answer no;
//    --ask puts every confirm dialog to the operator (its text on stderr, the typed answer: yes is OK, anything else Cancel).
//    --answers FILE (unattended): each dialog answered by the rule whose `starts` its text begins with; an unmatched one is cancelled and
//    stops the run with a non-zero exit (tools/hw/q4_answers_example.json).
//  - The page's versions (#dual-version, #nightly-version, the release picked) are logged; --expect TEXT refuses to press Install when the
//    chosen build's label does not contain TEXT (nothing is written then).
//
//   node tools/hw/cdp_install.mjs --url URL --build build-dual|build-nightly-dual [--out DIR] [--minutes 25] [--retries 3] [--expect TEXT]
//        [--retry-wait 10] [--return-wait 20]   (seconds before a retry's fresh browser; the second after "did not come back")
//   node tools/hw/cdp_install.mjs --url URL --stock FM-1.fwsc [--out DIR] [--minutes 25]
// The last line printed is one JSON object: {attempts: [...], final, ok}. Close every other program on the FM-1's port first.
import { spawn } from "child_process";
import { mkdirSync, mkdtempSync, writeFileSync, existsSync, readdirSync, readFileSync } from "fs";
import { tmpdir } from "os";
import { resolve } from "path";
import { createInterface } from "readline";
// (review, Round 31) --ask: a confirm dialog is put to the operator on the terminal (stderr / stdin), the typed answer passed back
const askOperator = (text) => new Promise((res) => {
  const rl = createInterface({ input: process.stdin, output: process.stderr });
  rl.question(`\nTHE PAGE ASKS:\n  ${text}\n  type yes to answer OK, anything else answers Cancel: `, (a) => { rl.close(); res(a.trim() === "yes"); });
});

const args = process.argv.slice(2), opt = {};
for (let i = 0; i < args.length; i++) {
  const k = args[i].replace(/^--/, "");
  if (["decline", "ask"].includes(k)) opt[k] = true; else opt[k] = args[++i];
}
const url = opt.url, build = opt.build, minutes = Number(opt.minutes || 25), retries = Number(opt.retries || 3);
// (Round 32) the waits before a retry's fresh browser: --retry-wait S (10) and, after "did not come back", --return-wait S (20)
const retryWait = Number(opt["retry-wait"] ?? 10), returnWait = Number(opt["return-wait"] ?? 20);
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
const norm = (x) => String(x).replace(/\s+/g, " ").trim();
const ruleFor = (text) => rules.filter((r) => norm(text).startsWith(norm(r.starts))).sort((a, b) => norm(b.starts).length - norm(a.starts).length)[0] || null;
const unmatched = [];
mkdirSync(out, { recursive: true });
const dl = resolve(out, "downloads"); mkdirSync(dl, { recursive: true });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const say = (...a) => console.log(new Date().toISOString().slice(11, 19), ...a);
// (Round 32) every retry in a fresh Chrome: Chrome's Web MIDI does not see a port that appears after the browser started, so the update-mode
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
    else if (opt.ask) { accept = await askOperator(d.params.message); by = "operator"; }
    else { accept = !opt.decline; by = "driver"; }
    dialogs.push({ at: new Date().toISOString(), type: d.params.type, text: d.params.message, answered: accept ? "yes" : "no", by, rule: rule && rule.starts });
    say(`DIALOG (${d.params.type}, answered ${accept ? "yes" : "no"}):`, d.params.message.slice(0, 300));
    send("Page.handleJavaScriptDialog", { accept });
  }
};
const ev = async (e) => (await send("Runtime.evaluate", { expression: e, awaitPromise: true, returnByValue: true })).result?.result?.value;
const txt = (idq) => ev(`(document.getElementById(${JSON.stringify(idq)})||{}).textContent`);
const RETRY = /not found|did not come back/i;
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
const result = { url, build: build || null, stock: opt.stock || null, out, attempts: [] };
// the page as loaded in a (fresh) browser, made ready to press: its versions, the stock file or the build chosen, --expect; into `rec` (the result for
// the first browser, the attempt's own record for a retry's); the button to press
const preparePage = async (rec) => {
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
    if (await ev("document.getElementById('go').disabled")) throw new Error("Install stayed disabled: " + rec.chosen.status);
  }
  return button;
};
try {
  await openBrowser();
  let button = await preparePage(result);
  const end = Date.now() + minutes * 60000;
  for (let k = 1; k <= retries; k++) {
    const a = { attempt: k };
    if (k > 1) {                                         // (Round 32) only a retry: the old browser closed, a fresh one, the page loaded and made ready again
      say(`attempt ${k}: fresh browser`);
      a.freshBrowser = true;
      await openBrowser();
      button = await preparePage(a);
    }
    dialogs = [];
    const logStart = (await txt("log") || "").split("\n").length;
    await ev(`document.getElementById('${button}').click()`);
    a.started = new Date().toISOString();
    let started = false, last = "";
    for (let i = 0; i < 30 && !started; i++) { await sleep(500); started = (await ev("document.getElementById('release').disabled")) === true; }
    while (Date.now() < end) {
      await sleep(3000);
      const st = await txt("status"), bar = await ev("document.getElementById('bar').value");
      if (st !== last) { say(`attempt ${k} bar ${Math.round(bar)} | ${st}`); last = st; }
      if ((await ev("document.getElementById('release').disabled")) === false) break;
    }
    a.ended = new Date().toISOString();
    a.status = await txt("status");
    a.timedOut = (await ev("document.getElementById('release').disabled")) === true;
    a.dialogs = dialogs;
    const lines = (await txt("log") || "").split("\n").slice(logStart - 1);
    a.log = lines;
    a.fwWrites = lines.filter((l) => /^write \d+/.test(l)).length;
    a.fwWriteMax = Math.max(0, ...lines.map((l) => (/^write (\d+)/.exec(l) || [0, 0])[1]).map(Number));
    a.summary = lines.filter((l) => /^(Firmware|Pack|Module|Effect|Models|Result)/i.test(l));
    a.plan = planOf(lines);
    writeFileSync(resolve(out, `page-log-attempt${k}.txt`), lines.join("\n"));
    result.attempts.push(a);
    say(`attempt ${k} ended: ${a.status}${a.timedOut ? " (the page was still working at the time limit)" : ""}`);
    if (unmatched.length) { a.stoppedByUnmatchedDialog = true; break; }
    if (a.timedOut || !RETRY.test(a.status)) break;
    if (k < retries) {
      // (Round 32) the old browser closed first (it lets go of the port); after "did not come back" the firmware is restarting: the fresh browser waits
      // longer, so that the normal port is there when it starts (its Web MIDI sees only the ports present then)
      const wait = /did not come back/i.test(a.status) ? returnWait : retryWait;
      say(`the page says to press again (${a.status.slice(0, 80)}): the browser closed, attempt ${k + 1} in a fresh browser in ${wait} s`);
      await closeBrowser();
      await sleep(wait * 1000);
    }
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
  else if (!result.ok && result.problems.length) result.error = "the page's result has parts that did not install: " + result.problems.join(" | ");
} catch (e) {
  result.error = String(e && e.message || e);
  result.ok = false;
  say("ERROR:", result.error);
} finally {
  await closeBrowser();
}
writeFileSync(resolve(out, "cdp-result.json"), JSON.stringify(result, null, 1));
console.log(JSON.stringify(result));
process.exitCode = result.ok ? 0 : 1;
