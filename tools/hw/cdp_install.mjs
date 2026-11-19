// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Round 31, docs/orangecat/BUILD.md 7.50) The installer page as the owner uses it, in real Chrome (headless, MIDI with SysEx granted): from
// ~/fm1-hw/r22/cdp-install.mjs (Round 9 .. 30), brought into the repository for the Release 3 qualification's install cases (7.37 Q4,
// tools/hw/q4_install.py drives it). What is new:
//  - Install is pressed again, up to --retries times (3), when the page ends saying the FM-1 was not found or did not come back (its texts
//    notfound, noloader, sNounit, noreturn: "press Install again"); Round 30 saw the headless browser miss the update-mode port twice. Every
//    attempt is logged with its status, its dialogs and its log lines.
//  - The end of an attempt is the page's own: #release enabled again after it was disabled (lockInstaller(false)), not a text pattern (the
//    old driver missed "Result: ..." and ended on its own timeout while the page had finished).
//  - --stock FILE: the stock section's file input (#stock-file) given the file through DOM.setFileInputFiles, the page's check awaited
//    ("Official V15 verified"), then #stock-go. The page's backup download is allowed into OUT (Browser.setDownloadBehavior).
//  - Every dialog (confirm, alert) is logged with its text and accepted: they are the page's questions. --decline makes it answer no;
//    --ask puts every confirm dialog to the operator (its text on stderr, the typed answer: yes is OK, anything else Cancel).
//  - The page's versions (#dual-version, #nightly-version, the release picked) are logged; --expect TEXT refuses to press Install when the
//    chosen build's label does not contain TEXT (nothing is written then).
//
//   node tools/hw/cdp_install.mjs --url URL --build build-dual|build-nightly-dual [--out DIR] [--minutes 25] [--retries 3] [--expect TEXT]
//   node tools/hw/cdp_install.mjs --url URL --stock FM-1.fwsc [--out DIR] [--minutes 25]
// The last line printed is one JSON object: {attempts: [...], final, ok}. Close every other program on the FM-1's port first.
import { spawn } from "child_process";
import { mkdirSync, mkdtempSync, writeFileSync, existsSync, readdirSync } from "fs";
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
const out = resolve(opt.out || mkdtempSync(tmpdir() + "/oc-cdp-")), port = Number(opt.port || 9224);
if (!url || (!build && !opt.stock)) { console.error("cdp_install: --url and --build (or --stock FILE) are needed"); process.exit(2); }
if (opt.stock && !existsSync(opt.stock)) { console.error("cdp_install: no file " + opt.stock); process.exit(2); }
mkdirSync(out, { recursive: true });
const dl = resolve(out, "downloads"); mkdirSync(dl, { recursive: true });
const dir = mkdtempSync(tmpdir() + "/oc-chrome-");
const chrome = spawn(process.env.CHROME || "/usr/bin/google-chrome", ["--headless=new", "--no-first-run", `--remote-debugging-port=${port}`, `--user-data-dir=${dir}`, "about:blank"], { stdio: "ignore" });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const say = (...a) => console.log(new Date().toISOString().slice(11, 19), ...a);
let targets;
for (let i = 0; i < 50; i++) { try { targets = await (await fetch(`http://127.0.0.1:${port}/json`)).json(); if (targets.length) break; } catch (_) {} await sleep(200); }
if (!targets) { console.error("cdp_install: Chrome did not open its debugging port"); chrome.kill(); process.exit(1); }
const ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
await new Promise((r) => (ws.onopen = r));
let id = 0; const pending = new Map(); let dialogs = [];
const send = (method, params = {}) => new Promise((r) => { const i = ++id; pending.set(i, r); ws.send(JSON.stringify({ id: i, method, params })); });
ws.onmessage = async (m) => {
  const d = JSON.parse(m.data);
  if (d.id && pending.has(d.id)) { pending.get(d.id)(d); pending.delete(d.id); }
  if (d.method === "Page.javascriptDialogOpening") {
    const accept = d.params.type === "alert" || (opt.ask && d.params.type === "confirm" ? await askOperator(d.params.message) : !opt.decline);
    dialogs.push({ at: new Date().toISOString(), type: d.params.type, text: d.params.message, answered: accept ? "yes" : "no", by: opt.ask && d.params.type === "confirm" ? "operator" : "driver" });
    say(`DIALOG (${d.params.type}, answered ${accept ? "yes" : "no"}):`, d.params.message.slice(0, 300));
    send("Page.handleJavaScriptDialog", { accept });
  }
};
const ev = async (e) => (await send("Runtime.evaluate", { expression: e, awaitPromise: true, returnByValue: true })).result?.result?.value;
const txt = (idq) => ev(`(document.getElementById(${JSON.stringify(idq)})||{}).textContent`);
const RETRY = /not found|did not come back/i;
const result = { url, build: build || null, stock: opt.stock || null, out, attempts: [] };
try {
  await send("Browser.grantPermissions", { origin: new URL(url).origin, permissions: ["midi", "midiSysex"] });
  await send("Browser.setDownloadBehavior", { behavior: "allow", downloadPath: dl });
  await send("Page.enable"); await send("Runtime.enable"); await send("DOM.enable");
  await send("Page.navigate", { url });
  await sleep(4000);
  result.page = { title: await ev("document.title"), status: await txt("status"), dual: await txt("dual-version"), nightly: await txt("nightly-version"),
    release: await ev("(document.getElementById('release')||{}).value") };
  say("page:", JSON.stringify(result.page));
  let button = "go";
  if (opt.stock) {
    button = "stock-go";
    const doc = await send("DOM.getDocument", { depth: -1 });
    const q = await send("DOM.querySelector", { nodeId: doc.result.root.nodeId, selector: "#stock-file" });
    if (!q.result || !q.result.nodeId) throw new Error("no #stock-file on the page");
    await send("DOM.setFileInputFiles", { nodeId: q.result.nodeId, files: [resolve(opt.stock)] });
    for (let i = 0; i < 60; i++) { if ((await ev("document.getElementById('stock-go').disabled")) === false) break; await sleep(500); }
    result.stockCheck = await txt("status");
    say("stock file:", result.stockCheck);
    if (await ev("document.getElementById('stock-go').disabled")) throw new Error("the page did not accept the stock file: " + result.stockCheck);
  } else {
    await ev(`(()=>{const r=document.getElementById('${build}'); if(!r) return false; r.checked=true; r.dispatchEvent(new Event('change')); return true;})()`);
    for (let i = 0; i < 90; i++) { if ((await ev("document.getElementById('go').disabled")) === false) break; await sleep(1000); }
    const label = await ev(`(()=>{const r=document.getElementById('${build}'); return r ? r.closest('label').textContent : null;})()`);
    result.chosen = { label, status: await txt("status"), what: await txt("install-what"),
      stages: await ev(`[...document.querySelectorAll('#stages input[type=checkbox]')].map(c => c.id + (c.checked ? ':on' : ':off') + (c.disabled ? ':disabled' : ''))`) };
    say("chosen:", JSON.stringify(result.chosen));
    if (opt.expect && !(label || "").includes(opt.expect)) throw new Error(`the build's label ${JSON.stringify(label)} does not contain ${JSON.stringify(opt.expect)}: Install not pressed`);
    if (await ev("document.getElementById('go').disabled")) throw new Error("Install stayed disabled: " + result.chosen.status);
  }
  const end = Date.now() + minutes * 60000;
  for (let k = 1; k <= retries; k++) {
    dialogs = [];
    const logStart = (await txt("log") || "").split("\n").length;
    await ev(`document.getElementById('${button}').click()`);
    const a = { attempt: k, started: new Date().toISOString() };
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
    writeFileSync(resolve(out, `page-log-attempt${k}.txt`), lines.join("\n"));
    result.attempts.push(a);
    say(`attempt ${k} ended: ${a.status}${a.timedOut ? " (the page was still working at the time limit)" : ""}`);
    if (a.timedOut || !RETRY.test(a.status)) break;
    if (k < retries) { say(`the page says to press again (${a.status.slice(0, 80)}): attempt ${k + 1} in 10 s`); await sleep(10000); }
  }
  result.downloads = readdirSync(dl);
  result.final = result.attempts.length ? result.attempts[result.attempts.length - 1].status : null;
  result.ok = !!result.attempts.length && !result.attempts[result.attempts.length - 1].timedOut && !RETRY.test(result.final || "") && !/error|stopped|refus|could not|failed/i.test(result.final || "");
} catch (e) {
  result.error = String(e && e.message || e);
  result.ok = false;
  say("ERROR:", result.error);
} finally {
  try { ws.close(); } catch (_) {}
  chrome.kill();
}
writeFileSync(resolve(out, "cdp-result.json"), JSON.stringify(result, null, 1));
console.log(JSON.stringify(result));
process.exitCode = result.ok ? 0 : 1;
