// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Orange Cat 0.15.0, branch linux-hw) web/app/HARDWARE_TEST.md's protocol parts on the unit, from a Linux PC with node: the web app's own
// Session (web/app/lib/session.js) over tools/hw/midi_bridge.py, as oc_install.mjs reaches the unit. What a part asks to be clicked is the
// Session call the page makes for it; what it asks to be heard or seen on the unit's screen is not here (the parts' listening steps are the
// player's). Each part prints one JSON line: passed or not, its times, what it read.
//   1 Connect (load, the Unit table: firmware, pack, modules, arrangement)    2 a parameter changed and read back (track 1, EDIT knob 2), put back
//   3 a sound saved to an empty library slot, another loaded, the saved one loaded back, starred and unstarred, renamed, erased (the slot empty
//     again)    4 the music now and slot 4 read as projects, the steps read, step 3's first note changed and put back
//   5 the arrangement read, written back unchanged, read again: the same bytes    6 a complete backup saved to a file (read only)
// Never a backup restore, a library wipe or a module removal. The sound on track 1 is changed by part 3: tools/hw/fm1hw.py snapshot / putback
// around the run (the caller) puts it back.
//   PYTHON=... node tools/hw/app_protocol.mjs OUT_DIR        (APP_MOCK=1: against the app's mocked unit instead)
import { writeFileSync } from "fs";
import { spawn } from "child_process";
import { createInterface } from "readline";
import { fileURLToPath } from "url";
import { Session } from "../../web/app/lib/session.js";
import * as E from "../../web/app/lib/editor_proto.js";

function bridge() {
  const py = spawn(process.env.PYTHON || "python3", [fileURLToPath(new URL("midi_bridge.py", import.meta.url))], { stdio: ["pipe", "pipe", "inherit"] });
  const input = { state: "connected", onmidimessage: null }, output = { state: "connected" };
  output.send = (bytes) => { py.stdin.write(Buffer.from(bytes).toString("hex") + "\n"); };
  py.on("exit", () => { input.state = output.state = "disconnected"; });
  const ready = new Promise((resolve, reject) => {
    createInterface({ input: py.stdout }).on("line", (line) => {
      if (line.startsWith("READY")) return resolve();
      if (line.startsWith("ERROR")) return reject(new Error(line));
      if (input.onmidimessage) input.onmidimessage({ data: Uint8Array.from(Buffer.from(line, "hex")) });
    });
  });
  return { input, output, ready, close: () => { py.stdin.end(); } };
}
async function mock() {                                  /* (APP_MOCK=1: the app's mocked unit, to try this script without the unit) */
  const { makeAppMock } = await import("../../web/app/lib/mock.js");
  const a = makeAppMock(), p = Session.find(a.access);
  return { input: p.input, output: p.output, ready: Promise.resolve(), close: () => {} };
}
const now = () => Number(process.hrtime.bigint() / 1000000n);
const results = [];
async function part(n, name, fn) {
  const t0 = now(), out = { part: n, name };
  try { Object.assign(out, await fn()); out.passed = out.passed !== false; }
  catch (e) { out.passed = false; out.error = String(e && e.message || e); }
  out.ms = now() - t0;
  console.log(JSON.stringify(out));
  results.push(out);
}

async function main() {
  const dir = process.argv[2];
  if (!dir) throw new Error("app_protocol.mjs OUT_DIR");
  const m = process.env.APP_MOCK ? await mock() : bridge();
  await m.ready;
  let lost = 0, noreply = 0;
  const s = new Session(m.input, m.output, { say: (k) => { if (k === "noreply") noreply++; }, lost: () => { lost++; } });
  try {
    await part(1, "Connect", async () => {
      await s.load();
      return { version: s.info.version, pack: s.pack && { state: s.pack.state, hash: s.pack.hash }, engines: s.engines && s.engines.engines.length, starts: s.mod && s.mod.nstarts,
        arrangement: !!s.arrange, tracks: s.tracks && s.tracks.tracks.map((t) => t.engine), watch: s.watch };
    });
    await part(2, "A parameter change, read back", async () => {
      const id = s.info.pe0 + 1, k = s.sel, before = s.dump.p[id];
      const to = before < 100 ? before + 20 : before - 20;
      const t0 = now(); const v = await s.setParam(k, id, to); const set_ms = now() - t0;
      const dump = await s.readDump();
      const back = await s.setParam(k, id, before);
      return { passed: v === to && dump.p[id] === to && back === before, param: s.pdesc[id] && s.pdesc[id].name, from: before, to, set_ms };
    });
    await part(3, "A preset saved and loaded", async () => {
      const lib = await s.library();
      const slot = lib.slots.findIndex((x) => !x.used);
      if (slot < 0) return { passed: false, error: "no empty library slot" };
      const saved = (await s.readDump()).p.slice();
      const rc = await s.libStore(slot, "WEBTEST");
      const after = await s.library();
      const stored = after.slots[slot];
      const rows = s.presetRows().filter((r) => !r.user && r.engine === s.tracks.tracks[s.sel].engine);
      const other = rows.find((r) => r.preset !== s.tracks.tracks[s.sel].preset) || rows[0];
      await s.loadPreset(other);
      const changed = s.dump.p.some((v, i) => i >= s.info.pe0 && v !== saved[i]);
      const lrc = await s.libLoad(slot);
      const same = s.info.pe0 + 8 <= saved.length && s.dump.p.slice(s.info.pe0, s.info.pe0 + 8).every((v, i) => v === saved[s.info.pe0 + i]);
      const row = s.presetRows().find((r) => r.user && r.preset === slot);
      let starred = null, unstarred = null;
      if (row) { await s.star(row); starred = !!s.presetRows().find((r) => r.user && r.preset === slot)?.favorite;
                 const r2 = s.presetRows().find((r) => r.user && r.preset === slot); await s.star(r2); unstarred = !s.presetRows().find((r) => r.user && r.preset === slot)?.favorite; }
      await s.libRename(slot, "WEBTEST2");
      const renamed = (await s.library()).slots[slot];
      const erc = await s.libErase(slot);
      const empty = !(await s.library()).slots[slot].used;
      return { passed: !rc && stored.used && stored.name === "WEBTEST" && changed && !lrc && same && renamed.name === "WEBTEST2" && !erc && empty && starred !== false && unstarred !== false,
        slot, stored: stored.name, other: other && other.name, changed, loaded_back_same: same, starred, unstarred, renamed: renamed.name, erased_empty: empty };
    });
    await part(4, "A project read; a step changed and put back", async () => {
      let t0 = now(); const music = await s.readProject(0); const music_ms = now() - t0;
      t0 = now(); const slot4 = await s.readProject(5); const slot4_ms = now() - t0;
      t0 = now(); const steps = await s.readSteps(s.sel); const steps_ms = now() - t0;
      const st = steps[2], before = st.notes.slice();
      const changed = { ...st, n: Math.max(1, st.n), notes: [st.notes[0] ? st.notes[0] + 4 : 52, st.notes[1], st.notes[2], st.notes[3]] };
      const w = await s.writeStep(s.sel, 2, changed);
      const back = await s.writeStep(s.sel, 2, st);
      const reread = (await s.readSteps(s.sel))[2];
      return { passed: !!music && w.notes[0] === changed.notes[0] && reread.notes.every((v, i) => v === before[i]) && reread.n === st.n,
        music_bytes: music && music.bytes.length, music_ms, slot4_bytes: slot4 ? slot4.bytes.length : 0, slot4_ms, steps_ms, step3: before, step3_written: w.notes };
    });
    await part(5, "The arrangement read, written back, read again", async () => {
      let t0 = now(); const a = await s.readLists(); const read_ms = now() - t0;
      t0 = now(); await s.writeLists(a.bytes); const write_ms = now() - t0;
      const b = await s.readLists();
      const same = a.bytes.length === b.bytes.length && a.bytes.every((v, i) => v === b.bytes[i]);
      return { passed: same, bytes: a.bytes.length, read_ms, write_ms };
    });
    await part(6, "A backup taken (saved to a file; never restored here)", async () => {
      const file = await s.backup();
      const path = `${dir}/app-backup-${new Date().toISOString().replace(/[:.]/g, "-")}.json`;
      writeFileSync(path, JSON.stringify(file));
      return { passed: file.objects.length > 0, objects: file.objects.length, file: path };
    });
  } finally {
    console.log(JSON.stringify({ summary: true, passed: results.filter((r) => r.passed).length, of: results.length, noreply, lost }));
    s.close();
    m.close();
  }
}
main().catch((e) => { console.error(e); process.exit(1); });
