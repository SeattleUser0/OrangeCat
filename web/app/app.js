// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: the page. Six tabs over one connection (lib/session.js): Unit, Sounds, Steps, Arrange, Project, Settings.
// The protocol, the checks and the texts are the old editor's (lib/editor_proto.js, lib/editor_text.js, generated from
// web/editor.html), the project codec the firmware's layout (lib/codec.js), the arrangement command 102 (lib/arrange.js), the
// colours the unit's palettes (lib/palette.js). This file only draws and wires them. ?mock talks to the mocked unit (lib/mock.js).
//
// Revisions:
//   0.14.2: first version (Session D).

import * as E from "./lib/editor_proto.js";
import { TEXT } from "./lib/editor_text.js";
import { Session } from "./lib/session.js";
import * as K from "./lib/codec.js";
import { TABS, designKnobs, editIds, palette, trackColor, engineScene, enginePicture, KNOB_COLORS, SESSION_C_DOC } from "./lib/model.js";
import { PALETTES } from "./lib/palette.js";

const L = K.LAYOUT, C = L.consts;
const QS = new URLSearchParams(location.search);
const MOCK = QS.has("mock");
const $ = (id) => document.getElementById(id);
const t = (k) => TEXT.en[k] ?? k;
function el(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs || {})) {
    if (v === null || v === undefined || v === false) continue;
    if (k === "text") e.textContent = v;
    else if (k === "html") e.innerHTML = v;
    else if (k.startsWith("on")) e.addEventListener(k.slice(2), v);
    else if (k === "style" && typeof v === "object") Object.assign(e.style, v);
    else e.setAttribute(k, v === true ? "" : v);
  }
  for (const c of kids.flat()) if (c !== null && c !== undefined && c !== false) e.append(c instanceof Node ? c : String(c));
  return e;
}
const two = (n) => String(n).padStart(2, "0");

/* ------------------------------------------------------------------ state --- */
const S = {
  session: null, access: null, mock: null, tab: (location.hash || "#unit").slice(1), busy: 0,
  pal: palette(QS.get("palette") || localStorageGet("oc-app-palette") || "NIGHT"),
  progress: 0,
  steps: null,                         /* the selected track's steps (TRACK_STEP) */
  project: null,                       /* {source, fbkk, bytes, lists, name} the project loaded into the page */
  arr: null,                           /* {orig, edit, bytes, chain, info} */
  lib: null, libSlot: 0, presetQuery: "",
  set: null,                           /* the site's release (Install set) */
  audio: null,
};
function localStorageGet(k) { try { return localStorage.getItem(k); } catch (_) { return null; } }
function localStorageSet(k, v) { try { localStorage.setItem(k, v); } catch (_) { /* (private window) */ } }
if (!TABS.some((x) => x.id === S.tab)) S.tab = "unit";

function say(msg, err = false) { const s = $("status"); s.textContent = msg; s.classList.toggle("err", !!err); }
const sayK = (k, arg = "") => say(t(k) + arg);
/* one action at a time; its error as text (an editor key becomes its English text) */
async function op(fn) {
  if (S.busy) { say("Busy: wait for the last action to finish"); return; }
  S.busy++; render();
  try { await fn(); }
  catch (e) {
    const m = e && e.message || String(e);
    const b = /^modBlocked:(\d+):(\d+)$/.exec(m);
    if (m === "closed") return;
    if (b) say(t("modBlocked").replace("{name}", S.session?.info?.engines?.[+b[1]] || "#" + b[1]).replace("{n}", b[2]), true);
    else say(TEXT.en[m] || m, true);
    console.warn(e);
  } finally { S.busy--; S.progress = 0; render(); }
}
const progress = (f) => { S.progress = f; const p = document.querySelector(".progress i"); if (p) p.style.width = Math.round(f * 100) + "%"; };

/* ----------------------------------------------------------- the colours --- */
function applyPalette() {
  const r = document.documentElement.style, p = S.pal;
  r.setProperty("--bg", p.bg); r.setProperty("--surface", p.surface); r.setProperty("--text", p.text);
  p.tracks.forEach((c, i) => r.setProperty(`--track-${i}`, c));
  const light = parseInt(p.bg.slice(1, 3), 16) > 128;
  document.documentElement.style.colorScheme = light ? "light" : "dark";
  r.setProperty("--panel", `color-mix(in srgb, ${p.surface} 70%, ${p.bg})`);
  r.setProperty("--raised", `color-mix(in srgb, ${p.surface} 88%, ${p.text})`);
  r.setProperty("--quiet", `color-mix(in srgb, ${p.bg} 70%, ${p.surface})`);
  r.setProperty("--line", `color-mix(in srgb, ${p.bg} 86%, ${p.text})`);
  r.setProperty("--mid", `color-mix(in srgb, ${p.text} 62%, ${p.bg})`);
  r.setProperty("--dim", `color-mix(in srgb, ${p.text} 36%, ${p.bg})`);
  r.setProperty("--second", `color-mix(in srgb, ${p.text} 82%, ${p.bg})`);
  $("trackdots").replaceChildren(...p.tracks.map((c) => el("i", { style: { background: c } })));
}

/* ------------------------------------------------------------ connection --- */
async function connect() {
  if (S.session) { disconnect(); return; }
  await op(async () => {
    if (!S.access) {
      if (MOCK) { const { makeAppMock } = await import("./lib/mock.js"); S.mock = makeAppMock(); S.access = S.mock.access; }
      else {
        if (!navigator.requestMIDIAccess) throw new Error(t("nomidi"));
        try { S.access = await navigator.requestMIDIAccess({ sysex: true }); } catch (_) { throw new Error(t("denied")); }
      }
      S.access.onstatechange = (e) => { const p = e.port; if (S.session && p && p.state === "disconnected" && (p.id === S.session.input.id || p.id === S.session.output.id)) { disconnect(); sayK("lost"); } };
    }
    const ports = Session.find(S.access);
    if (!ports) throw new Error(t("nodevice"));
    if (ports.input.open) await ports.input.open().catch(() => {});
    const s = new Session(ports.input, ports.output, { say: (k) => sayK(k), push: onPush });
    S.session = s;
    sayK("connecting");
    try { await s.load((f) => { progress(f); if (f < 0.6) say(t("reading") + " " + Math.round(f / 0.6 * 100) + "%"); }); }
    catch (e) { disconnect(); throw e; }
    S.steps = null; S.arr = null; S.lib = null;
    sayK("ready");
  });
}
function disconnect() {
  if (S.session) S.session.close();
  S.session = null; S.steps = null; S.arr = null; S.lib = null; S.project = S.project && S.project.source === "file" ? S.project : null;
  render();
}
let pushTimer = null;
function onPush(f) {
  /* a change made on the unit: redraw soon; a load or a track change: read the sound again */
  clearTimeout(pushTimer);
  pushTimer = setTimeout(async () => {
    const s = S.session;
    if (!s || S.busy) return;
    if (f.cmd === E.CMD.RELOAD || f.cmd === E.CMD.STEP_CHANGED) {
      try { await s.readTracks(); s.dump = await s.readDump(); await s.rereadEngineDesc(); if (S.steps && f.cmd === E.CMD.STEP_CHANGED) S.steps = await s.readSteps(s.sel); } catch (_) { return; }
    }
    if (!dragging) render();
  }, 120);
}

/* -------------------------------------------------------------- the page --- */
function renderTabs() {
  $("tabs").replaceChildren(...TABS.map((x) => el("button", { role: "tab", "aria-selected": String(S.tab === x.id), onclick: () => { S.tab = x.id; location.hash = x.id; render(); } }, x.label)));
}
function renderHeader() {
  const s = S.session;
  $("connect").textContent = s ? "Disconnect" : "Connect";
  $("connect").disabled = !!S.busy && !s;
  $("unitline").textContent = s ? `${s.info.version}${MOCK ? " (mocked unit)" : ""}` : MOCK ? "Mocked unit: press Connect" : "Not connected";
}
function render() {
  document.body.dataset.busy = String(S.busy);
  renderHeader(); renderTabs();
  const main = $("main");
  const keep = document.activeElement && main.contains(document.activeElement) && document.activeElement.matches("input[type=text],input[type=search]") ? document.activeElement.id : null;
  const view = ({ unit: viewUnit, sounds: viewSounds, steps: viewSteps, arrange: viewArrange, project: viewProject, settings: viewSettings })[S.tab]();
  main.replaceChildren(el("div", { class: "progress" }, el("i", { style: { width: Math.round(S.progress * 100) + "%" } })), view);
  if (keep && $(keep)) { const i = $(keep); i.focus(); try { i.setSelectionRange(i.value.length, i.value.length); } catch (_) {} }
}
const needUnit = (what) => el("div", { class: "card" }, el("p", { class: "empty" }, `Connect the unit to ${what}.`),
  el("div", { class: "bar", style: { justifyContent: "center" } }, el("button", { class: "primary", onclick: connect, disabled: S.busy ? true : null }, "Connect")));
const card = (title, ...kids) => el("section", { class: "card" }, el("h2", { text: title }), ...kids);
const wide = (title, ...kids) => el("section", { class: "card wide" }, el("h2", { text: title }), ...kids);
function pickFile(accept, then) {
  const i = el("input", { type: "file", accept, style: { display: "none" } });
  i.addEventListener("change", () => { const f = i.files[0]; i.remove(); if (f) then(f); });
  document.body.append(i); i.click();
}
function download(name, data, type = "application/octet-stream") {
  const blob = data instanceof Blob ? data : new Blob([data], { type });
  const url = URL.createObjectURL(blob), a = el("a", { href: url, download: name });
  document.body.append(a); a.click(); a.remove(); setTimeout(() => URL.revokeObjectURL(url), 1500);
}
function trackBar(onPick) {
  const s = S.session;
  return el("div", { class: "trackbar", role: "group", "aria-label": "Track" }, ...Array.from({ length: s.info.ntrk || 1 }, (_, k) => {
    const tr = s.tracks?.tracks[k], name = tr ? (s.info.engines[tr.engine] || "?") : "";
    return el("button", { "aria-pressed": String(k === s.sel), style: { "--tc": trackColor(S.pal, k) }, onclick: () => op(async () => { await s.selectTrack(k); await onPick?.(k); }) },
      el("b", { style: { background: trackColor(S.pal, k) } }, String(k + 1)), name);
  }));
}

/* ================================================================== Unit === */
function viewUnit() {
  const s = S.session;
  const g = el("div", { class: "grid" });
  if (!s) {
    g.append(el("section", { class: "card wide" }, el("h2", { text: "The unit" }),
      el("p", {}, "Plug the FM-1 in by USB, with Orange Cat on it, and press Connect. The page asks the browser for MIDI with SysEx (Chrome, Edge or Opera)."),
      el("p", { class: "dim" }, "Firmware is installed from the installer page; this page manages what is on the unit: its modules, sounds, steps, arrangement, projects and backups."),
      el("div", { class: "bar" }, el("button", { class: "primary", onclick: connect }, "Connect"), MOCK ? null : el("a", { href: "?mock" }, "Try it with a mocked unit"))));
    g.append(viewSetCard());
    return g;
  }
  const i = s.info;
  g.append(card("The unit",
    el("table", {}, el("tbody", {},
      el("tr", {}, el("td", { class: "dim" }, "Firmware"), el("td", {}, i.version)),
      el("tr", {}, el("td", { class: "dim" }, "Tracks"), el("td", {}, String(i.ntrk || 1))),
      el("tr", {}, el("td", { class: "dim" }, "Library"), el("td", {}, i.shared ? `${i.shared.total} shared slots` : "the older user slots")),
      el("tr", {}, el("td", { class: "dim" }, "Backup"), el("td", {}, i.backupCaps & 2 ? "read and restore" : i.backupCaps & 1 ? "read only" : "no")),
      el("tr", {}, el("td", { class: "dim" }, "Arrangement"), el("td", {}, s.arrange ? "read and write (command 102)" : "not in this firmware (0.14.2 and later)")),
      el("tr", {}, el("td", { class: "dim" }, "Live sync"), el("td", {}, s.watch ? (s.v4 ? "every track" : "the selected track") : "no"))))));
  /* the pack */
  if (s.pack) {
    g.append(card(t("packTitle"),
      el("p", { class: "dim small" }, t("packHelp")),
      el("p", {}, t("packState" + Math.min(3, s.pack.state))),
      el("div", { class: "bar" },
        el("button", { onclick: () => pickFile(".pak,application/octet-stream", (f) => op(async () => {
          await s.installPack(new Uint8Array(await f.arrayBuffer()), (x) => { progress(x); say(t("packWorking") + Math.round(x * 100) + "%"); });
          sayK("packDone");
        })) }, t("packInstall")),
        el("button", { class: "danger", disabled: s.pack.state === 0 && !s.pack.len ? true : null, onclick: () => op(async () => {
          if (!confirm(t("packEraseConfirm"))) return;
          const r = E.parse[E.CMD.PACK_ERASE](await s.rq(E.req.packErase(), { timeout: 3000 }));
          await s.probePack(); if (r.rc) throw new Error("packFail"); sayK("packErased");
        }) }, t("packErase")))));
  }
  g.append(viewSetCard());
  /* the Engines table */
  if (s.engines) {
    const rows = s.engineRows();
    const cols = ["colNo", "colName", "colSource", "colVersion", "colSize", "colStart", "colState", "colTracks", ""];
    const tbl = el("table", {}, el("thead", {}, el("tr", {}, ...cols.map((c) => el("th", { text: c ? t(c) : "" })))),
      el("tbody", {}, ...rows.map((r) => {
        const acts = el("td", {});
        for (const a of r.actions) acts.append(el("button", { class: "small", onclick: () => engineAction(r, a) }, t("act_" + a)));
        if (r.canUse && (s.info.ntrk || 0) > 1) {
          const sel = el("select", { class: "small", "aria-label": t("useOnTrack") }, ...Array.from({ length: s.info.ntrk }, (_, k) => el("option", { value: String(k) }, "Track " + (k + 1))));
          sel.value = String(s.sel);
          acts.append(sel, el("button", { class: "small", onclick: () => op(async () => { const o = await s.useOnTrack(r.num, +sel.value); if (o.refused) sayK(o.key); else sayK("useDone"); }) }, t("act_use")));
        }
        const st = t("st_" + r.state) + (r.reason ? " (" + t(r.reason) + ")" : "");
        return el("tr", { class: ["ok", "empty"].includes(r.state) ? "" : "bad" },
          el("td", { class: "num" }, r.num === null ? "" : String(r.num)),
          el("td", { title: s.modPages[r.num] ? s.modPages[r.num].pages.map((p) => p.title).join(", ") : null }, r.name),
          el("td", {}, r.source === "module" ? el("span", { class: "tag mod" }, t("srcModule")) : r.source === "builtin" ? el("span", { class: "tag" }, t("srcBuiltin")) : "-"),
          el("td", {}, r.version), el("td", { class: "num" }, r.size ? String(r.size) : ""), el("td", { class: "num" }, r.start === null ? "" : String(r.start)),
          el("td", {}, st), el("td", { class: "num" }, r.num === null ? "" : String(r.tracks)), acts);
      })));
    g.append(el("section", { class: "card wide" }, el("h2", { text: t("enginesTitle") }), el("p", { class: "dim small" }, t("enginesHelp")),
      el("p", {}, t(E.modSummary(s.mod))), el("div", { class: "scroll" }, tbl),
      el("p", { class: "dim small" }, "Modules are the engines today. Effects and step generators will be picked here the same way once the firmware has them as modules.")));
  } else if (s.mod) {
    g.append(card(t("enginesTitle"), el("p", {}, t(E.modSummary(s.mod))), el("p", { class: "dim" }, "This firmware has modules but no Engines list (0.11.0): use the old editor's two buttons.")));
  }
  return g;
}
function engineAction(r, a) {
  const s = S.session;
  if (a === "install" || a === "replace")
    return pickFile(".ocm,application/octet-stream", (f) => op(async () => {
      const out = await s.installModule({ num: r.num, replace: a === "replace", start: r.start, tracks: r.tracks }, new Uint8Array(await f.arrayBuffer()), {
        ask: (key, n) => confirm(t(key).replace("{n}", String(n))), progress: (pc) => say(t("modWorking") + pc + "%") });
      if (out.how !== "cancelled") sayK(out.how === "swapped" ? "modSwapped" : "modDone");
    }));
  return op(async () => {
    if (!confirm(t(r.num === null ? "enginesRemoveAllConfirm" : "modRemoveConfirm"))) return;
    await s.removeEngine(r.num); sayK("modRemoved");
  });
}
/* the set: the pack and the modules the site publishes with its firmware (firmware/release.json, nightly.json), or files picked here */
function viewSetCard() {
  const s = S.session, set = S.set;
  const c = card("Install the set",
    el("p", { class: "dim small" }, "The asset pack and the engine modules that go with a firmware, in one go, as the installer page installs them after the firmware. The firmware itself is installed from the installer page."));
  if (!set) {
    c.append(el("div", { class: "bar" },
      el("button", { onclick: () => op(loadSet) }, "Look up the site's set"),
      el("button", { onclick: () => pickSetFiles() }, "Pick files…")));
    return c;
  }
  if (set.error) { c.append(el("p", { class: "dim" }, set.error), el("div", { class: "bar" }, el("button", { onclick: () => pickSetFiles() }, "Pick files…"))); return c; }
  const boxes = set.items.map((it) => {
    const cb = el("input", { type: "checkbox", checked: true, id: "set-" + it.name });
    it.cb = cb;
    return el("label", { class: "bar", style: { margin: "2px 0" } }, cb, `${it.kind === "pak" ? "Asset pack" : "Module"}: ${it.name}`);
  });
  c.append(el("p", {}, set.title), ...boxes,
    el("div", { class: "bar" }, el("button", { class: "primary", disabled: !s ? true : null, onclick: () => op(() => runSetHere(set)) }, "Install the set"),
      el("button", { onclick: () => { S.set = null; render(); } }, "Back")));
  if (set.summary) c.append(el("ul", { class: "small" }, ...set.summary.map((x) => el("li", {}, x))));
  return c;
}
async function loadSet() {
  const base = new URL("../../firmware/", location.href);
  const get = async (n) => { const r = await fetch(new URL(n, base), { cache: "no-cache" }); return r.ok ? r.json() : null; };
  let rel = null, nig = null;
  try { rel = await get("release.json"); nig = await get("nightly.json"); } catch (_) { /* (not on the site) */ }
  const pick = QS.get("channel") === "nightly" && nig ? nig : rel;
  if (!pick || !pick.pak) { S.set = { error: "No set found beside this page (the site's firmware/release.json). Open the app from the published site, or pick the files." }; return; }
  const items = [{ kind: "pak", name: pick.pak.split("/").pop(), url: new URL(pick.pak.replace(/^\.\.\/\.\.\/firmware\//, ""), base) },
    ...(pick.ocm || []).map((o) => ({ kind: "ocm", name: o.split("/").pop(), url: new URL(o.replace(/^\.\.\/\.\.\/firmware\//, ""), base) }))];
  S.set = { title: `${pick === nig ? "Nightly" : "Standard"} v${pick.version}`, items };
}
function pickSetFiles() {
  const i = el("input", { type: "file", accept: ".pak,.ocm", multiple: true, style: { display: "none" } });
  i.addEventListener("change", async () => {
    const files = [...i.files]; i.remove();
    S.set = { title: "Files picked here", items: await Promise.all(files.map(async (f) => ({ kind: /\.pak$/i.test(f.name) ? "pak" : "ocm", name: f.name, bytes: new Uint8Array(await f.arrayBuffer()) }))) };
    render();
  });
  document.body.append(i); i.click();
}
async function runSetHere(set) {
  const s = S.session, chosen = set.items.filter((x) => x.cb ? x.cb.checked : true);
  const bytes = async (x) => x.bytes || new Uint8Array(await (await fetch(x.url)).arrayBuffer());
  const files = { pack: null, modules: [] };
  for (const x of chosen) { if (x.kind === "pak") files.pack = await bytes(x); else files.modules.push(await bytes(x)); }
  const sum = await s.installSet(files, { stages: { pack: !!files.pack, module: files.modules.length > 0 }, confirmInPlace: () => confirm(t("modInPlaceConfirm")), say: (k) => sayK(k), progress });
  const word = (v) => typeof v === "string" ? ({ installed: "installed", current: "already there", off: "left out", nofile: "no file", oldfw: "firmware too old", nounit: "no unit" })[v] : t(v.error || "modFail");
  set.summary = [`Asset pack: ${word(sum.pack)}`, ...sum.module.map((m, i) => `Module ${i + 1}: ${word(m)}`)];
  say("Set: " + set.summary.join(" · "));
}

/* ================================================================ Sounds === */
let dragging = false;
let sceneTimer = null, sceneHit = 0;
function viewSounds() {
  const s = S.session;
  if (!s) return needUnit("tune its sounds");
  const d = s.dump, eng = d.engine, name = s.info.engines[eng] || "?", k = s.sel;
  const presetName = (s.names[eng] || [])[d.preset] ?? `#${d.preset}`;
  const colors = KNOB_COLORS;
  const design = designKnobs(eng, s.info, s.pdesc);
  const box = el("div", { id: "scene" });
  const top = el("section", { class: "card wide" }, trackBar(),
    el("div", { class: "stage" },
      el("div", { class: "scenebox", style: { "--tc": trackColor(S.pal, k) } }, box,
        el("div", { class: "caption" }, el("b", { style: { color: trackColor(S.pal, k) } }, name), el("span", { class: "dim" }, presetName))),
      el("div", {}, el("div", { class: "knobs" }, ...design.knobs.map((x, i) => (x.macro ? macroView(x, colors[i]) : knobView(x.id, colors[i])))),
        el("p", { class: "dim small", style: { marginTop: "10px" } }, design.source === "design"
          ? `The four knobs of ${name} in the new interface's order and colours (${SESSION_C_DOC}, section 2). A knob marked * is a macro the firmware does not have yet; it is shown, not written.`
          : `${name} is not in the new interface's knob table yet: these are the unit's HOME knobs of today.`,
          " Drag up or down, scroll, or use the arrow keys."))));
  drawScene(box, eng, name, design);
  clearInterval(sceneTimer);
  sceneTimer = setInterval(() => { const b = document.getElementById("scene"); if (!b || S.tab !== "sounds") { clearInterval(sceneTimer); return; } drawScene(b, eng, name, design); }, 100);
  const edits = el("section", { class: "card" }, el("h2", {}, "Sound"),
    el("p", { class: "dim small" }, `The SOUND sheet: the eight values of ${name}${s.titles[eng] ? " (" + s.titles[eng].filter(Boolean).join(", ") + ")" : ""}, two rows of four. ENV, MOD, FX and VOICE stay on the unit for now.`),
    el("div", { class: "edits" }, ...editIds(s.info).map((id) => editView(id)).filter(Boolean)));
  return el("div", { class: "grid" }, top, edits, presetsView(), libraryView());
}
/* the picture: Session C's scene where the engine has one (its sources: the knobs' positions, a clock, a bob, phases at the knobs'
 * rates, an envelope and a hit after a knob is turned; the page hears no sound, so the scope is a sine), else a placeholder */
const sceneT0 = Date.now(), scenePh = [0, 0, 0, 0];
function drawScene(box, eng, name, design) {
  const s = S.session, now = Date.now(), t = (now - sceneT0) / 1000;
  const q8 = (x) => { if (x.macro || x.id === null) return 128; const d = s.pdesc[x.id]; return d ? Math.round((s.dump.p[x.id] - d.min) * 256 / Math.max(1, d.max - d.min)) : 128; };
  const k = design.knobs.map(q8);
  k.forEach((v, i) => { scenePh[i] = (scenePh[i] + (0.1 + 2.9 * v / 256) * 0.1 * 256) % 256; });
  sceneHit = Math.max(0, sceneHit - 40);
  const input = { s: { K1: k[0], K2: k[1], K3: k[2], K4: k[3], ENV: 150 + Math.round(60 * Math.sin(t * 2)), HIT: sceneHit, GATE: 0, LFO: Math.round(256 * Math.sin(t * 1.3)),
      NOTE: 0, TIME: Math.round((t % 1) * 256), WOB: Math.round(256 * Math.sin(t * Math.PI)), PH1: scenePh[0] | 0, PH2: scenePh[1] | 0, PH3: scenePh[2] | 0, PH4: scenePh[3] | 0 },
    col: ["var(--text)", ...KNOB_COLORS, trackColor(S.pal, s.sel), "var(--dim)", "var(--bg)"],
    scope: Array.from({ length: 64 }, (_, i) => Math.round(200 * Math.sin(i / 64 * Math.PI * 4 + t * 6))) };
  box.innerHTML = enginePicture(name, input) ?? engineScene(eng, name, KNOB_COLORS);
}
function macroView(x, color) {
  return el("div", { class: "knob macro", style: { "--kc": color }, title: "A macro (UI_DESIGN.md section 2): it needs P_MAC" + "0..3 in the firmware, which is not built yet" },
    svgEl(`<svg viewBox="0 0 100 100" style="opacity:.45">${dial(0.5, color)}</svg>`), el("span", { class: "lab" }, x.label + "*"), el("span", { class: "val" }, "macro"));
}
const visible = (d) => d && d.max > d.min && !!d.label && d.label !== "-";
function fmt(d, v) { if (!d) return String(v); const [a, u] = E.fmtValue(d, v); return u ? `${a} ${u}` : a; }
function knobView(id, color) {
  const s = S.session, d = s.pdesc[id];
  if (!visible(d)) return el("div", { class: "knob", style: { "--kc": "var(--dim)" } }, svgEl(`<svg viewBox="0 0 100 100">${dial(0, "var(--line)")}</svg>`), el("span", { class: "lab" }, "-"), el("span", { class: "val" }, ""));
  let v = s.dump.p[id];
  const frac = () => (v - d.min) / Math.max(1, d.max - d.min);
  const svg = svgEl(`<svg viewBox="0 0 100 100" tabindex="0" role="slider">${dial(frac(), color)}</svg>`);
  for (const [a, x] of Object.entries({ "aria-label": d.label, "aria-valuemin": d.min, "aria-valuemax": d.max, "aria-valuenow": v })) svg.setAttribute(a, x);
  const val = el("span", { class: "val" }, fmt(d, v));
  const set = (nv, final) => {
    nv = Math.max(d.min, Math.min(d.max, Math.round(nv)));
    if (nv === v && !final) return;
    v = nv; s.dump.p[id] = v;
    svg.innerHTML = dial(frac(), color); svg.setAttribute("aria-valuenow", v); val.textContent = fmt(d, v);
    sceneHit = 256;
    s.setParam(s.sel, id, v).catch((e) => say(e.message, true));
  };
  let y0 = 0, v0 = 0;
  svg.addEventListener("pointerdown", (e) => { dragging = true; y0 = e.clientY; v0 = v; svg.setPointerCapture(e.pointerId); });
  svg.addEventListener("pointermove", (e) => { if (dragging && svg.hasPointerCapture(e.pointerId)) set(v0 + (y0 - e.clientY) * (d.max - d.min) / 180); });
  svg.addEventListener("pointerup", () => { dragging = false; });
  svg.addEventListener("wheel", (e) => { e.preventDefault(); set(v + (e.deltaY < 0 ? 1 : -1) * Math.max(1, Math.round((d.max - d.min) / 64))); }, { passive: false });
  svg.addEventListener("keydown", (e) => {
    const step = e.shiftKey ? Math.max(1, Math.round((d.max - d.min) / 16)) : 1;
    if (e.key === "ArrowUp" || e.key === "ArrowRight") { set(v + step); e.preventDefault(); }
    if (e.key === "ArrowDown" || e.key === "ArrowLeft") { set(v - step); e.preventDefault(); }
  });
  return el("div", { class: "knob", style: { "--kc": color } }, svg, el("span", { class: "lab" }, d.label), val);
}
/* an SVG element from its markup (an HTML element named svg would not draw) */
function svgEl(markup) { const t = document.createElement("div"); t.innerHTML = markup; return t.firstElementChild; }
function dial(f, color) {
  const a0 = -225, a1 = 45, a = a0 + (a1 - a0) * Math.max(0, Math.min(1, f)), rad = (x) => x * Math.PI / 180;
  const pt = (ang, r) => [50 + r * Math.cos(rad(ang)), 50 + r * Math.sin(rad(ang))];
  const arc = (from, to, r) => { const [x0, y0] = pt(from, r), [x1, y1] = pt(to, r); return `M${x0.toFixed(2)} ${y0.toFixed(2)} A${r} ${r} 0 ${to - from > 180 ? 1 : 0} 1 ${x1.toFixed(2)} ${y1.toFixed(2)}`; };
  const [px, py] = pt(a, 26);
  return `<path d="${arc(a0, a1, 40)}" stroke="var(--line)" stroke-width="8" fill="none" stroke-linecap="round"/>` +
    (f > 0.004 ? `<path d="${arc(a0, a, 40)}" stroke="${color}" stroke-width="8" fill="none" stroke-linecap="round"/>` : "") +
    `<circle cx="50" cy="50" r="30" fill="var(--surface)" stroke="var(--line)"/><line x1="50" y1="50" x2="${px.toFixed(2)}" y2="${py.toFixed(2)}" stroke="${color}" stroke-width="5" stroke-linecap="round"/>`;
}
function editView(id) {
  const s = S.session, d = s.pdesc[id];
  if (!visible(d)) return null;
  const v = s.dump.p[id], val = el("span", { class: "val" }, fmt(d, v));
  let input;
  if (d.fmt === E.F.ENUM && d.names) {
    input = el("select", { "aria-label": d.label }, ...d.names.map((n, i) => el("option", { value: String(d.min + i) }, n)));
    input.value = String(v);
    input.addEventListener("change", () => op(async () => { const nv = await s.setParam(s.sel, id, +input.value); val.textContent = fmt(d, nv); }));
  } else {
    input = el("input", { type: "range", min: d.min, max: d.max, value: v, "aria-label": d.label });
    input.addEventListener("input", () => { val.textContent = fmt(d, +input.value); s.setParam(s.sel, id, +input.value).catch((e) => say(e.message, true)); });
  }
  return el("label", { class: "edit" }, el("span", { class: "lab" }, d.label), input, val);
}
function presetsView() {
  const s = S.session, rows = s.presetRows(), q = S.presetQuery.trim().toUpperCase();
  const search = el("input", { type: "text", id: "preset-q", placeholder: "Find a preset", value: S.presetQuery, oninput: (e) => { S.presetQuery = e.target.value; render(); } });
  const list = el("div", { class: "list" });
  let grp = null;
  for (const r of rows) {
    const engName = r.user ? "Library · " + (s.info.engines[r.actualEngine ?? r.engine] || "") : s.info.engines[r.engine];
    if (q && !(r.name.toUpperCase().includes(q) || String(engName).toUpperCase().includes(q))) continue;
    const g = r.user ? "Library" : engName;
    if (g !== grp) { grp = g; list.append(el("div", { class: "grp" }, g)); }
    const on = !r.user && r.engine === s.dump.engine && r.preset === s.dump.preset;
    list.append(el("div", { class: "row" + (on ? " on" : "") },
      s.prefs && (s.prefs.state.caps & 8) ? el("button", { class: "star", "aria-label": (r.favorite ? t("deviceUnstar") : t("deviceStar")) + ": " + r.name, onclick: () => op(async () => { await s.star(r); }) }, r.favorite ? "★" : "☆") : el("span"),
      el("span", {}, r.name, r.user ? el("span", { class: "dim small" }, "  " + (s.info.engines[r.actualEngine] || "")) : null),
      el("button", { class: "small", onclick: () => op(async () => { await s.loadPreset(r); say(`${r.name} on track ${s.sel + 1}`); }) }, "Load")));
  }
  return el("section", { class: "card" }, el("h2", {}, "Presets"), el("div", { class: "bar" }, search, el("span", { class: "dim small" }, "★ marks favorites, as SAVE › PRESETS on the unit")), list);
}
function libraryView() {
  const s = S.session;
  const c = el("section", { class: "card" }, el("h2", {}, t("library")));
  if (!S.lib) {
    c.append(el("p", { class: "dim small" }, "The unit's own library of saved sounds (SAVE › USER)."), el("div", { class: "bar" }, el("button", { onclick: () => op(async () => { S.lib = await s.library(); }) }, "Read the library")));
    return c;
  }
  const slotName = (i) => (s.info.shared ? "U" + String(i + 1).padStart(3, "0") : "U" + two(i + 1));
  const sel = S.lib.slots[S.libSlot] || { slot: 0, used: 0 };
  const name = el("input", { type: "text", id: "lib-name", maxlength: 12, placeholder: "NAME", value: sel.used ? sel.name : "" });
  const list = el("div", { class: "list", style: { maxHeight: "300px" } }, ...S.lib.slots.slice(0, s.info.shared ? 384 : 64).map((x, i) =>
    el("div", { class: "row" + (i === S.libSlot ? " on" : ""), onclick: () => { S.libSlot = i; render(); } },
      el("span", { class: "dim small" }, slotName(i)), el("span", { class: x.used ? "" : "dim" }, x.used ? x.name : "empty"),
      el("span", { class: "dim small" }, x.used ? s.info.engines[x.engine] || "" : ""))));
  const refresh = async () => { S.lib = await s.library(); };
  c.append(list, el("div", { class: "bar" }, el("b", {}, slotName(S.libSlot)), name),
    el("div", { class: "bar" },
      el("button", { onclick: () => op(async () => { if (sel.used && !confirm(`Replace ${slotName(S.libSlot)} (${sel.name}) with the sound of track ${s.sel + 1}?`)) return; const rc = await s.libStore(S.libSlot, name.value); if (rc) throw new Error(t("flashFail").replace("{rc}", rc)); await refresh(); say(`Saved to ${slotName(S.libSlot)}`); }) }, "Save the sound here"),
      el("button", { disabled: !sel.used ? true : null, onclick: () => op(async () => { const rc = await s.libLoad(S.libSlot); if (rc) throw new Error("Empty or unreadable slot"); say(`${sel.name} on track ${s.sel + 1}`); }) }, "Load"),
      el("button", { disabled: !sel.used || !s.info.shared ? true : null, onclick: () => op(async () => { await s.libRename(S.libSlot, name.value); await refresh(); say("Renamed"); }) }, "Rename"),
      el("button", { class: "danger", disabled: !sel.used ? true : null, onclick: () => op(async () => { if (!confirm(`Erase ${slotName(S.libSlot)} (${sel.name})?`)) return; await s.libErase(S.libSlot); await refresh(); say("Erased"); }) }, "Erase")),
    el("p", { class: "dim small" }, "Star a saved sound in the preset list to make it a favorite."));
  return c;
}

/* ================================================================= Steps === */
const NOTE = (n) => (n ? E.noteName(n) : "");
function viewSteps() {
  const s = S.session;
  if (!s) return needUnit("edit its steps");
  const k = s.sel, len = Math.max(1, Math.min(s.info.nstep, s.dump.p[C.P_SLEN] || 16));
  const head = el("section", { class: "card wide", style: { "--tc": trackColor(S.pal, k) } }, trackBar(async () => { S.steps = null; }),
    el("div", { class: "bar" }, el("button", { onclick: () => op(async () => { S.steps = await s.readSteps(k); }) }, S.steps ? "Read again" : "Read the steps"),
      el("span", { class: "dim small" }, `Track ${k + 1}, the phrase it is on: LEN ${len}. Notes as C4, D#3; up to four in a step.`)));
  if (!S.steps) return el("div", { class: "grid" }, head);
  const rows = S.steps.map((st, i) => {
    const on = st.time === 0 && (st.n > 0 || (st.hit | 0) > 0);
    const noteIn = (j) => {
      const inp = el("input", { type: "text", value: j < st.n ? NOTE(st.notes[j]) : "", "aria-label": `step ${i + 1} note ${j + 1}` });
      inp.addEventListener("change", () => op(async () => {
        const notes = [0, 1, 2, 3].map((x) => (x === j ? inp.value.trim() : x < st.n ? NOTE(st.notes[x]) : "")).filter(Boolean);
        const parsed = E.parseNotes(notes.join(" "));
        if (!parsed) throw new Error(`"${inp.value}" is not a note (C4, D#3)`);
        const ns = [...parsed, 0, 0, 0, 0].slice(0, 4);
        const next = { ...st, n: parsed.length, notes: ns, time: parsed.length || st.hit ? 0 : 2, vel: st.vel || 100 };
        S.steps[i] = await s.writeStep(k, i, next);
      }));
      return el("td", {}, inp);
    };
    const vel = el("input", { type: "number", min: 0, max: 127, value: st.vel, style: { width: "4.5em" }, "aria-label": `step ${i + 1} velocity` });
    vel.addEventListener("change", () => op(async () => { S.steps[i] = await s.writeStep(k, i, { ...st, vel: Math.max(0, Math.min(127, +vel.value | 0)) }); }));
    const fx = [st.flags & 1 ? "ACC" : "", st.flags & 2 ? "SLIDE" : "", st.chance !== undefined && st.chance < 100 ? st.chance + "%" : "", st.hit ? E.hitsText(st.hit, st.acc) : ""].filter(Boolean).join(" ");
    return el("tr", { class: (i % 4 === 0 ? "beat " : "") + (on ? "on " : "") + (i >= len ? "out" : "") },
      el("td", { class: "num" }, two(i + 1)), noteIn(0), noteIn(1), noteIn(2), noteIn(3),
      el("td", {}, ["NOTE", "TIE", "REST"][st.time] ?? st.time), el("td", {}, vel), el("td", { class: "small" }, fx));
  });
  const tracker = el("section", { class: "card wide", style: { "--tc": trackColor(S.pal, k) } }, el("h2", {}, "Tracker"),
    el("div", { class: "scroll" }, el("table", { class: "tracker" }, el("thead", {}, el("tr", {}, ...["#", "N1", "N2", "N3", "N4", "Time", "Vel", "FX"].map((x) => el("th", {}, x)))), el("tbody", {}, ...rows))));
  return el("div", { class: "grid" }, head, tracker, recordedNotesView());
}
function recordedNotesView() {
  const s = S.session, k = s.sel;
  const c = el("section", { class: "card wide" }, el("h2", {}, "Recorded notes"));
  const pr = S.project && S.project.source === "now" ? S.project : null;
  c.append(el("div", { class: "bar" }, el("button", { onclick: () => op(async () => { const p = await s.readProject(0, progress); S.project = { source: "now", ...p, name: p.fbkk.name }; }) }, pr ? "Read again" : "Read them from the unit"),
    el("span", { class: "dim small" }, "Each recorded note's own place, length, velocity and bend, as the project record holds them.")));
  if (pr) {
    const bank = pr.fbkk.active[k], notes = K.bankNotes(pr.fbkk, k, bank);
    c.append(notes.length ? el("div", { class: "scroll" }, el("table", { class: "notes" },
      el("thead", {}, el("tr", {}, ...["Step", "Onset", "Note", "Velocity", "Length (steps)", "Bend"].map((x) => el("th", {}, x)))),
      el("tbody", {}, ...notes.map((n) => el("tr", {}, el("td", { class: "num" }, two((n.step & 63) + 1) + (n.step & 64 ? " (shown on the next)" : "")),
        el("td", { class: "num" }, (n.on / C.RECORD_UNIT).toFixed(3)), el("td", {}, E.noteName(n.note)), el("td", { class: "num" }, String(n.vel)),
        el("td", { class: "num" }, K.noteSteps(n).toFixed(3)), el("td", { class: "num" }, n.pitch ? (n.pitch > 0 ? "+" : "") + n.pitch : ""))))))
      : el("p", { class: "dim" }, `No recorded notes on track ${k + 1}'s phrase (bank ${bank + 1}).`));
  }
  c.append(el("p", { class: "note" }, "What the page cannot do yet: edit a recorded note one at a time (the unit's NOTES event editor has no command; command 108 reads them in test builds only), and show a step's RATCH, FLAM, condition and NUDGE (STEP_GET does not carry them, and the project record keeps them only inside the unit). Edit those on the unit's TRACKER and NOTES pages."));
  return c;
}

/* =============================================================== Arrange === */
function viewArrange() {
  const s = S.session;
  if (!s) return needUnit("arrange its phrases, chains and song");
  if (!s.arrange) return el("div", { class: "grid" }, wide("Arrange", el("p", {}, "This firmware does not answer the arrangement command (102, Orange Cat 0.14.2 and later), so the chains and the song cannot be read or written from the page."),
    el("p", { class: "dim small" }, "The project records the backup holds do not carry them either (web/EDITOR_PROTOCOL.md, \"the arrangement\").")));
  if (!S.arr) return el("div", { class: "grid" }, wide("Arrange", el("p", { class: "dim" }, "Phrases, chains, the song and the scenes of the music on the unit now. Read them, change them here, see what will change, then write them back while the unit is stopped."),
    el("div", { class: "bar" }, el("button", { class: "primary", onclick: () => op(readArr) }, "Read the arrangement"))));
  const A = S.arr, l = A.edit, diff = K.diffLists(A.orig, l), problem = K.listsProblem(l);
  return el("div", { class: "grid" },
    chainView(), poolView(), songView(), scenesView(),
    wide("What will change", diff.length ? el("ul", { class: "diff" }, ...diff.map((x) => el("li", {}, el("span", { class: "k" }, x.kind), x.text))) : el("p", { class: "dim" }, "Nothing yet."),
      problem ? el("p", { class: "note" }, "Out of range: " + problem) : null,
      el("div", { class: "bar" },
        el("button", { class: "primary", disabled: !diff.length || problem || S.busy ? true : null, onclick: () => op(writeArr) }, "Write to the unit"),
        el("button", { disabled: !diff.length ? true : null, onclick: () => { A.edit = K.cloneLists(A.orig); render(); } }, "Undo all"),
        el("button", { onclick: () => op(readArr) }, "Read again"),
        el("span", { class: "dim small grow" }, "The unit keeps where each track is; a track whose step now names another phrase goes to it. SAVE on the unit keeps it with the project."))));
}
async function readArr() {
  const r = await S.session.readLists();
  S.arr = { orig: r.lists, edit: K.cloneLists(r.lists), info: r.info, chain: S.arr?.chain ?? r.lists.tchain[S.session.sel] ?? 0, phrase: 0 };
  say("Arrangement read");
}
async function writeArr() {
  const A = S.arr;
  const out = await S.session.writeLists(K.encodeLists(A.edit), progress).catch((e) => {
    if (e.code === "playing") throw new Error("The unit is playing: stop it, then write again.");
    if (e.code === "stale") throw new Error("The unit was busy with a save, a backup or the card browser: write again.");
    if (e.code === "keepFailed") throw new Error("The unit could not keep a track's unsaved steps first (PHRASES FULL or a flash error): nothing was changed.");
    throw e;
  });
  await readArr();
  say(out.track !== null ? `Written. Track ${out.track + 1} could not go to its new step (no free bank or a flash error): it stays where it was.` : "Written to the unit.");
}
function poolView() {
  const A = S.arr, info = A.info;
  return el("section", { class: "card" }, el("h2", {}, "Phrases (the pool)"),
    el("p", { class: "dim small" }, "The 64 phrases every project on the unit shares. Filled: it holds steps; a dot: a chain here names it. Pick one, then click a chain step to put it there."),
    el("div", { class: "pool" }, ...info.pool.map((p) => el("button", {
      class: [p.hasSteps ? "has" : "", A.edit.chains.some((c) => c.some((e) => e.phrase === p.id)) ? "named" : "", A.phrase === p.id ? "pick" : ""].join(" "),
      title: `Phrase ${two(p.id)}${p.len ? ", LEN " + p.len : ", empty"}${p.inSaved ? ", in a saved project" : ""}`,
      onclick: () => { A.phrase = A.phrase === p.id ? 0 : p.id; render(); } }, two(p.id)))));
}
function chainView() {
  const A = S.arr, l = A.edit, c = A.chain;
  const pick = el("select", { "aria-label": "Chain" }, ...l.chains.map((ch, i) => el("option", { value: String(i) }, `Chain ${two(i + 1)}${ch.some((e) => e.phrase) ? " ·" : ""}${l.tchain.includes(i) ? " (track " + (l.tchain.map((x, k) => (x === i ? k + 1 : 0)).filter(Boolean).join(", ")) + ")" : ""}`)));
  pick.value = String(c);
  pick.addEventListener("change", () => { A.chain = +pick.value; render(); });
  let from = -1;
  const slots = l.chains[c].map((e, i) => {
    const ph = el("select", { "aria-label": `Step ${K.STEP_LETTERS[i]} phrase` }, el("option", { value: "0" }, "—"), ...Array.from({ length: L.lists.NPOOL }, (_, p) => el("option", { value: String(p + 1) }, two(p + 1))));
    ph.value = String(e.phrase);
    ph.addEventListener("change", () => { e.phrase = +ph.value; render(); });
    const reps = el("select", { "aria-label": `Step ${K.STEP_LETTERS[i]} repeats` }, ...Array.from({ length: 16 }, (_, r) => el("option", { value: String(r + 1) }, "x" + (r + 1))));
    reps.value = String(e.reps);
    reps.addEventListener("change", () => { e.reps = +reps.value; render(); });
    const div = el("div", { class: "slot" + (e.phrase ? "" : " empty"), draggable: "true", title: "Drag to move this step" },
      el("span", { class: "let" }, K.STEP_LETTERS[i]), ph, reps);
    div.addEventListener("click", (ev) => { if (A.phrase && ev.target === div) { e.phrase = A.phrase; render(); } });
    div.addEventListener("dragstart", (ev) => { from = i; div.classList.add("drag"); ev.dataTransfer.effectAllowed = "move"; ev.dataTransfer.setData("text/plain", String(i)); });
    div.addEventListener("dragend", () => div.classList.remove("drag"));
    div.addEventListener("dragover", (ev) => { ev.preventDefault(); div.classList.add("over"); });
    div.addEventListener("dragleave", () => div.classList.remove("over"));
    div.addEventListener("drop", (ev) => { ev.preventDefault(); const f = from >= 0 ? from : +ev.dataTransfer.getData("text/plain"); moveItem(l.chains[c], f, i); render(); });
    return div;
  });
  return el("section", { class: "card wide" }, el("h2", {}, "Chains"),
    el("div", { class: "bar" }, pick, el("span", { class: "dim small grow" }, "16 steps, A to P: a phrase and how many times it plays. Drag a step to reorder."),
      el("button", { class: "small", onclick: () => { l.chains[c] = l.chains[c].map(() => ({ phrase: 0, reps: 1 })); render(); } }, "Clear chain")),
    el("div", { class: "chain" }, ...slots));
}
function moveItem(arr, from, to) { if (from < 0 || from === to) return; const [x] = arr.splice(from, 1); arr.splice(to, 0, x); }
function songView() {
  const A = S.arr, l = A.edit, T = C.NTRK;
  let last = 0;
  l.song.forEach((r, i) => { if (r.some(Boolean)) last = i; });
  const shown = Math.min(L.lists.NROW, Math.max(8, last + 4));
  let from = -1;
  const rows = l.song.slice(0, shown).map((r, i) => {
    const tr = el("tr", { draggable: "true" }, el("td", { class: "handle", title: "Drag to move this row" }, "⋮⋮"), el("td", { class: "num dim" }, two(i + 1)),
      ...r.map((v, k) => {
        const sel = el("select", { "aria-label": `Row ${i + 1} track ${k + 1}`, style: { borderColor: v ? trackColor(S.pal, k) : null } }, el("option", { value: "0" }, "—"), ...Array.from({ length: L.lists.NCHAIN }, (_, c) => el("option", { value: String(c + 1) }, two(c + 1))));
        sel.value = String(v);
        sel.addEventListener("change", () => { r[k] = +sel.value; render(); });
        return el("td", {}, sel);
      }),
      el("td", {}, el("button", { class: "small", title: "Insert a row here", onclick: () => { l.song.splice(i, 0, Array(T).fill(0)); l.song.pop(); render(); } }, "+"),
        el("button", { class: "small", title: "Delete this row", onclick: () => { l.song.splice(i, 1); l.song.push(Array(T).fill(0)); render(); } }, "−")));
    tr.addEventListener("dragstart", () => { from = i; tr.classList.add("drag"); });
    tr.addEventListener("dragend", () => tr.classList.remove("drag"));
    tr.addEventListener("dragover", (ev) => { ev.preventDefault(); tr.classList.add("over"); });
    tr.addEventListener("dragleave", () => tr.classList.remove("over"));
    tr.addEventListener("drop", (ev) => { ev.preventDefault(); moveItem(l.song, from, i); render(); });
    return tr;
  });
  return el("section", { class: "card" }, el("h2", {}, "Song"),
    el("p", { class: "dim small" }, "Each row names a chain for each track; each track goes down its own column. An empty cell or the last row sends it back to the top of its block."),
    el("div", { class: "scroll" }, el("table", { class: "song" }, el("thead", {}, el("tr", {}, el("th"), el("th", {}, "Row"), ...Array.from({ length: T }, (_, k) => el("th", { style: { color: trackColor(S.pal, k) } }, "T" + (k + 1))), el("th"))),
      el("tbody", {}, ...rows))));
}
function scenesView() {
  const A = S.arr, l = A.edit, T = C.NTRK;
  const sceneRows = l.scenes.map((sc, i) => {
    const kept = el("input", { type: "checkbox", checked: !!((l.sceneUsed >> i) & 1), "aria-label": `Scene ${i + 1} kept` });
    kept.addEventListener("change", () => { l.sceneUsed = kept.checked ? l.sceneUsed | (1 << i) : l.sceneUsed & ~(1 << i); render(); });
    return el("tr", {}, el("td", { class: "num" }, String(i + 1)), el("td", {}, kept), ...sc.map((x, k) => {
      const ch = el("select", { class: "small", "aria-label": `Scene ${i + 1} track ${k + 1} chain` }, el("option", { value: "0" }, "as is"), ...Array.from({ length: L.lists.NCHAIN }, (_, c) => el("option", { value: String(c + 1) }, two(c + 1))));
      ch.value = String(x.chain); ch.addEventListener("change", () => { x.chain = +ch.value; render(); });
      const st = el("select", { class: "small", "aria-label": `Scene ${i + 1} track ${k + 1} step` }, ...[...K.STEP_LETTERS].map((s, e) => el("option", { value: String(e) }, s)));
      st.value = String(x.step); st.addEventListener("change", () => { x.step = +st.value; render(); });
      const m = el("input", { type: "checkbox", checked: x.mute, title: "MUTE", "aria-label": `Scene ${i + 1} track ${k + 1} mute` });
      m.addEventListener("change", () => { x.mute = m.checked; render(); });
      return el("td", {}, ch, st, m);
    }));
  });
  const grooves = Array.from({ length: T }, (_, k) => {
    const g = el("select", { class: "small", "aria-label": `Track ${k + 1} groove` }, ...L.lists.grooves.map((n, i) => el("option", { value: String(i) }, n)));
    g.value = String(l.groove[k]); g.addEventListener("change", () => { l.groove[k] = +g.value; render(); });
    const a = el("input", { type: "number", min: 0, max: 100, value: l.gamt[k], style: { width: "4.5em" }, "aria-label": `Track ${k + 1} groove amount` });
    a.addEventListener("change", () => { l.gamt[k] = Math.max(0, Math.min(100, +a.value | 0)); render(); });
    return el("label", { class: "bar" }, el("b", { style: { color: trackColor(S.pal, k) } }, "T" + (k + 1)), g, a, "%");
  });
  const launch = el("select", { "aria-label": "Launch" }, ...L.lists.launches.map((n, i) => el("option", { value: String(i) }, n)));
  launch.value = String(l.launch); launch.addEventListener("change", () => { l.launch = +launch.value; render(); });
  return el("section", { class: "card" }, el("h2", {}, "Scenes, groove, launch"),
    el("p", { class: "dim small" }, "A scene keeps each track's chain, step and mute (PERFORM). LAUNCH: when a step gone to by hand comes in while playing."),
    el("div", { class: "scroll" }, el("table", { class: "song" }, el("thead", {}, el("tr", {}, el("th", {}, "#"), el("th", {}, "Kept"), ...Array.from({ length: T }, (_, k) => el("th", { style: { color: trackColor(S.pal, k) } }, "T" + (k + 1))))), el("tbody", {}, ...sceneRows))),
    el("div", { class: "bar" }, "Launch", launch), ...grooves);
}

/* =============================================================== Project === */
const PROJECT_FORMAT = "orangecat-project";
function viewProject() {
  const s = S.session, p = S.project;
  const g = el("div", { class: "grid" });
  if (s) {
    const src = el("select", { "aria-label": "Read from" }, el("option", { value: "0" }, "The music on the unit now"),
      ...[0, 1, 2, 3].map((k) => el("option", { value: String(k + 2), disabled: s.slotUsed[k] === 0 ? true : null }, `Saved slot ${k + 1}${s.slotUsed[k] === 0 ? " (empty)" : ""}`)));
    const slot = el("select", { "aria-label": "Slot" }, ...[0, 1, 2, 3].map((k) => el("option", { value: String(k) }, `Slot ${k + 1}${s.slotUsed[k] ? " (used)" : ""}`)));
    g.append(card("On the unit",
      el("div", { class: "bar" }, src, el("button", { class: "primary", onclick: () => op(async () => {
        const id = +src.value, r = await s.readProject(id, progress);
        if (!r) throw new Error("That slot is empty");
        let lists = null, pool = null;
        if (id === 0 && s.arrange) { const a = await s.readLists(); lists = a.bytes; pool = a.info.pool.map((x) => x.crc); }
        S.project = { source: id === 0 ? "now" : "slot" + (id - 1), ...r, lists, pool, name: r.fbkk.name };
        say(`Read: ${r.fbkk.name || "(no name)"}`);
      }) }, "Read")),
      el("p", { class: "dim small" }, "Saving and loading a slot is the unit's own SAVE and LOAD (PROJECT): the chains and the song go with them."),
      el("div", { class: "bar" }, slot,
        el("button", { onclick: () => op(async () => { const k = +slot.value; if (s.slotUsed[k] && !confirm(`Save the music now over slot ${k + 1}?`)) return; await s.projectSave(k); say(`Saved to slot ${k + 1}`); }) }, "Save the music now here"),
        el("button", { onclick: () => op(async () => { const k = +slot.value; if (!s.slotUsed[k]) throw new Error("That slot is empty"); if (!confirm(`Load slot ${k + 1}? What is not saved of the music now is lost.`)) return; await s.projectLoad(k); S.arr = null; S.steps = null; say(`Slot ${k + 1} loaded`); }) }, "Load it on the unit"))));
  } else g.append(needUnit("read and save its projects"));
  /* the project held by the page */
  const files = card("Files",
    el("p", { class: "dim small" }, "A project file holds the project record as the unit's backup carries it (the four tracks, their sounds, all 32 phrases' steps and timing, the recorded notes, the motion, the name), and the arrangement when it was read with the music now."),
    el("div", { class: "bar" },
      el("button", { disabled: !p ? true : null, onclick: () => exportProject() }, "Export the project"),
      el("button", { onclick: () => pickFile(".json,.ocproj,application/json", (f) => op(() => importProject(f))) }, "Import a project file")));
  g.append(files);
  if (p) g.append(projectSummaryView(p));
  g.append(backupView());
  return g;
}
function projectSummaryView(p) {
  const s = S.session, names = s ? s.info.engines : L.engines.map((e) => e.name);
  const rows = K.projectSummary(p.fbkk, names);
  const c = el("section", { class: "card wide" }, el("h2", {}, `Project: ${p.fbkk.name || "(no name)"} · ${p.source === "file" ? "from a file" : p.source === "now" ? "the music now" : "saved " + p.source.replace("slot", "slot ")}`),
    el("div", { class: "scroll" }, el("table", {}, el("thead", {}, el("tr", {}, ...["Track", "Engine", "Preset", "On phrase", "Phrases in use", "Recorded notes", "Motion"].map((x) => el("th", {}, x)))),
      el("tbody", {}, ...rows.map((r) => el("tr", {}, el("td", {}, el("b", { style: { color: trackColor(S.pal, r.track) } }, "T" + (r.track + 1))), el("td", {}, r.engineName),
        el("td", {}, (s?.names[r.engine] || [])[r.preset] ?? "#" + r.preset), el("td", { class: "num" }, "bank " + (r.active + 1)),
        el("td", {}, r.banksUsed.map((b) => b + 1).join(" ") || "none"), el("td", { class: "num" }, String(r.notes)), el("td", { class: "num" }, String(r.motion))))))));
  if (p.lists) {
    const l = K.decodeLists(p.lists), used = l.chains.filter((ch) => ch.some((e) => e.phrase)).length, rowsUsed = l.song.filter((r) => r.some(Boolean)).length;
    c.append(el("p", { class: "dim small" }, `Arrangement: ${used} chains, ${rowsUsed} song rows, ${[...Array(8)].filter((_, i) => (l.sceneUsed >> i) & 1).length} scenes kept.`));
  }
  if (p.source === "file" && s) {
    c.append(el("div", { class: "bar" }, el("button", { onclick: () => op(async () => {
      if (!confirm("Write this project to the unit as the music now? The music now is replaced (save it to a slot first). The chains and the song are not written: phrase numbers are the unit's own.")) return;
      await s.writeObject(0, p.bytes, progress);
      await s.readTracks(); s.dump = await s.readDump(); S.arr = null; S.steps = null;
      say("Written as the music now. On the unit, SAVE keeps it in a slot.");
    }) }, "Write it to the unit as the music now")));
  }
  return c;
}
function exportProject() {
  const p = S.project, b64 = (u) => { let x = ""; for (const c of u) x += String.fromCharCode(c); return btoa(x); };
  const file = { format: PROJECT_FORMAT, version: 1, created: new Date().toISOString(), firmware: S.session?.info.version || "", name: p.fbkk.name,
    fbkk: b64(p.bytes), lists: p.lists ? b64(p.lists) : null, pool: p.pool || null };
  download(`${(p.fbkk.name || "project").toLowerCase().replace(/[^a-z0-9]+/g, "-")}.ocproj.json`, JSON.stringify(file), "application/json");
}
async function importProject(f) {
  if (f.size > 400000) throw new Error("Not a project file (too large)");
  const j = JSON.parse(await f.text());
  if (j.format !== PROJECT_FORMAT || j.version !== 1 || typeof j.fbkk !== "string") throw new Error("Not an Orange Cat project file");
  const bytes = Uint8Array.from(atob(j.fbkk), (c) => c.charCodeAt(0));
  const fbkk = K.decodeFbkk(bytes);   /* (both checksums: a damaged file is refused here) */
  S.project = { source: "file", bytes, fbkk, lists: j.lists ? Uint8Array.from(atob(j.lists), (c) => c.charCodeAt(0)) : null, pool: j.pool, name: fbkk.name };
  say(`Opened ${f.name}`);
}
function backupView() {
  const s = S.session;
  const c = card("Complete backup",
    el("p", { class: "dim small" }, "Everything on the unit in one file on this computer: the music now, the settings and template, the four project slots, the library. Restoring writes it all back."),
    el("div", { class: "bar" },
      el("button", { disabled: !s || !(s.info.backupCaps & 1) ? true : null, onclick: () => op(async () => {
        const file = await s.backup(progress);
        download(`Orange Cat-backup-${new Date().toISOString().slice(0, 10)}.json`, JSON.stringify(file), "application/json");
        S.lastBackup = file; sayK("backupDone");
      }) }, t("backupExport")),
      el("button", { disabled: !s || !(s.info.backupCaps & 2) ? true : null, onclick: () => pickFile(".json,application/json", (f) => op(async () => {
        if (f.size > 600000) throw new Error("Backup file too large");
        const text = await f.text();
        if (!confirm(t("backupConfirm"))) return;
        const r = await s.restore(text, progress);
        S.arr = null; S.steps = null; S.project = null;
        say(t("backupRestored") + " " + ({ restored: "The arrangement of the music now is restored too.", "different pool": "The arrangement was not written: the unit's phrases are not the ones the backup named.", "no command": "This firmware cannot take the arrangement.", none: "The file holds no arrangement." })[r.arrangement]);
      })) }, t("backupImport"))),
    el("p", { class: "note" }, "A backup does not carry the saved slots' chains and songs, nor the phrase pool itself: the firmware's project record has no room for them (web/EDITOR_PROTOCOL.md). This page adds the arrangement of the music now, and writes it back only to a unit whose phrases are still the same."));
  const b = S.lastBackup;
  const what = [["0", "the music now (project record)"], ["1", "settings and the template"], ["2..5", "project slots 1 to 4"], ["6, 7, 17, 18", "library banks (general sounds)"], ["9..16", "CZ-1 tones"], ["19..22", "FM6 voices"], ["23..28", "Prophet programs, the last bank"], ["8", "retired (always empty)"]];
  c.append(el("h3", { style: { marginTop: "12px" } }, "What a backup holds"), el("table", { class: "small" }, el("tbody", {}, ...what.map(([id, x]) => el("tr", {}, el("td", { class: "num dim" }, id), el("td", {}, x),
    el("td", { class: "num dim" }, b ? String(b.objects.filter((o) => String(id).split(/[^0-9]+/).length && inIds(o.id, id)).reduce((n, o) => n + o.size, 0)) + " B" : ""))))));
  return c;
}
function inIds(v, spec) { return spec.split(",").some((p) => { const [a, z] = p.trim().split(".."); return z === undefined ? +a === v : v >= +a && v <= +z; }); }

/* ============================================================== Settings === */
function viewSettings() {
  const s = S.session, g = el("div", { class: "grid" });
  const pal = el("select", { "aria-label": "Page colours" }, ...PALETTES.map((p) => el("option", { value: p.name }, p.name)));
  pal.value = S.pal.name;
  pal.addEventListener("change", () => { S.pal = palette(pal.value); localStorageSet("oc-app-palette", pal.value); applyPalette(); render(); });
  g.append(card("This page",
    el("label", { class: "bar" }, "Colours", pal, el("span", { class: "dim small" }, "the unit's own palettes and knob colours")),
    el("p", { class: "dim small" }, "The names, knobs and pictures follow the unit's new interface; where its design (" + SESSION_C_DOC + ") was not settled yet the page uses the unit as it is now."),
    el("p", { class: "small" }, el("a", { href: "../installer/" }, "Installer"), " · ", el("a", { href: "../editor/" }, "The old editor"), " · ", el("a", { href: "?mock" }, "Mocked unit"))));
  if (!s) { g.append(needUnit("change its settings")); return g; }
  const st = s.prefs?.state;
  if (st) {
    const sel = (id, label, names, value) => {
      const x = el("select", { "aria-label": label }, ...names.map((n, i) => el("option", { value: String(i) }, n)));
      x.value = String(value);
      x.addEventListener("change", () => op(async () => { const r = E.parse[E.CMD.UI_SET](await s.rq(E.req.uiSet(id, +x.value), E.FLASH_OPT)); await s.readPreferences(); if (r.rc && r.rc !== 3 && r.rc !== 4) throw new Error("The unit refused it (" + r.rc + ")"); say(r.rc === 4 ? "Saved after STOP" : "Saved"); }));
      return el("label", { class: "bar" }, el("span", { style: { minWidth: "120px" } }, label), x);
    };
    const kids = [];
    if (st.caps & 1) kids.push(sel(0, "Screen palette", s.prefs.palettes, st.palette));
    if (st.caps & 8) kids.push(sel(3, "Preset list", ["All", "Favorites"], st.filter));
    if (st.caps & 16) kids.push(sel(4, "Click", ["OFF", "REC", "ON"], st.click), sel(5, "Click level", ["LOW", "MID", "HIGH"], st.clickLevel), sel(6, "Count-in", ["OFF", "BAR", "2 BARS"], st.countin), sel(7, "Note preview", ["OFF", "ON"], st.preview), sel(8, "Chord entry", ["HOLD", "ADD"], st.chordAdd));
    g.append(card("The unit", ...kids));
  }
  const a = S.audio;
  g.append(card("Audio",
    el("p", { class: "dim small" }, "How hard the audio works: the load of the render, its worst block against the block's time, and blocks rendered late."),
    a ? (a.schema === 8 ? el("table", {}, el("tbody", {}, ...[["Load", a.loadPct + " %"], ["Worst block", `${a.worstUs} of ${a.budgetUs} µs`], ["Late blocks", String(a.late)], ["Blocks", String(a.blocks)]].map(([k, v]) => el("tr", {}, el("td", { class: "dim" }, k), el("td", { class: "num" }, v)))))
      : el("p", { class: "dim" }, "This firmware gives an older readout.")) : null,
    el("div", { class: "bar" }, el("button", { onclick: () => op(async () => { S.audio = E.parse[E.CMD.AUDIO_STATS](await s.rq(E.req.audioStats(false), { timeout: 600, quiet: true })); }) }, "Read"),
      el("button", { onclick: () => op(async () => { S.audio = E.parse[E.CMD.AUDIO_STATS](await s.rq(E.req.audioStats(true), { timeout: 600, quiet: true })); }) }, "Read and reset"))));
  return g;
}

/* ------------------------------------------------------------------ start --- */
$("connect").addEventListener("click", () => connect());
window.addEventListener("hashchange", () => { const h = location.hash.slice(1); if (TABS.some((x) => x.id === h) && h !== S.tab) { S.tab = h; render(); } });
applyPalette();
render();
if (MOCK) say("Mocked unit: nothing here reaches a real FM-1. Press Connect.");
if (QS.has("autoconnect")) connect();
export const __test = { S, render, connect };
