// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
//
// (Orange Cat 0.13.7, branch linux-hw) The installer page's own pack and module installs (web/fm1oc.js installPack, installModule, probeUnit),
// run from a Linux PC with node. Node has no MIDI: tools/hw/midi_bridge.py (mido, the port fm1hw.py uses) carries the SysEx frames over a pipe
// and stands in for Web MIDI. Nothing here is new protocol: the page's functions do every check they do in the browser.
//
//   node tools/hw/oc_install.mjs info
//   node tools/hw/oc_install.mjs module FILE.ocm        install a module (the start the unit names; in place only if no other start is free)
//   node tools/hw/oc_install.mjs pack FILE.pak          install the asset pack (the unit refuses another build's)
//   node tools/hw/oc_install.mjs erase-pack             erase the pack (PACK MISSING until it is installed again)
//   node tools/hw/oc_install.mjs remove ENGINE          (0.13.8) remove one engine's module (MOD_REMOVE ENGINE; 127, the whole area, is refused here)
//   node tools/hw/oc_install.mjs backup FILE.json       (0.14.1) a full backup through the page's own code (web/fm1backup.js captureBackup, as the
//                                                       installer page runs it before an update): BACKUP_LIST and BACKUP_GET only, read back with readBackup
//   node tools/hw/oc_install.mjs pack-unchecked FILE    (0.13.8, a test of the firmware's own check) the page's pack install without its packProblem check:
//                                                       the unit erases at BEGIN and must refuse the file at END
//
// PYTHON names the interpreter with mido (default: python3). Close every other program on the port first (fm1hw.py, the editor page).
// It never removes the whole module area, wipes the library or restores a backup (backup only reads).
import { readFileSync, writeFileSync, existsSync } from "fs";
import { spawn } from "child_process";
import { createInterface } from "readline";
import { fileURLToPath } from "url";
import { OcLink, OCM, ocReq, OC_CMD, probeUnit, installPack, installModule, parsePackInfo, parseModInfo } from "../../web/fm1oc.js";
import { captureBackup, readBackup } from "../../web/fm1backup.js";

// Web MIDI's input and output, over the bridge's pipe
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

async function main() {
  const [what, file] = process.argv.slice(2);
  const m = bridge();
  await m.ready;
  const link = new OcLink(m.input, m.output);
  try {
    if (what === "info") {
      console.log(JSON.stringify(await probeUnit(link), null, 1));
    } else if (what === "module") {
      const ocm = OCM.parse(new Uint8Array(readFileSync(file)));
      let last = -1;
      await installModule(link, ocm, (f) => { const p = Math.floor(f * 10); if (p !== last) { last = p; process.stdout.write(`module ${p * 10}%\r`); } }, { confirmInPlace: () => { console.log("in place: no other start is free, the old module is erased first (the page asks the user here; this tool says yes)"); return true; } });
      console.log(`\nmodule installed: engine ${ocm.engine}`);
      console.log(JSON.stringify((await probeUnit(link)).engines?.engines?.[ocm.engine] ?? null));
    } else if (what === "pack") {
      const bytes = new Uint8Array(readFileSync(file));
      const info = parsePackInfo(await link.ask(ocReq.packInfo(), { timeout: 1000, retries: 1 }));
      await installPack(link, bytes, info, () => {});
      console.log(JSON.stringify(parsePackInfo(await link.ask(ocReq.packInfo(), { timeout: 1000, retries: 1 }))));
    } else if (what === "erase-pack") {
      const a = await link.ask([OC_CMD.PACK_ERASE, []], { timeout: 12000 });
      console.log("erase rc", a && a[0]);
      console.log(JSON.stringify(parsePackInfo(await link.ask(ocReq.packInfo(), { timeout: 1000, retries: 1 }))));
    } else if (what === "remove") {
      const e = Number(file);
      if (!Number.isInteger(e) || e < 0 || e >= 127) throw new Error("remove wants one engine number (the whole area, 127, is not offered here)");
      const a = await link.ask([OC_CMD.MOD_REMOVE, [e]], { timeout: 12000 });
      console.log("remove rc", a && a[0]);
      console.log(JSON.stringify(parseModInfo(await link.ask(ocReq.modInfo(), { timeout: 1000, retries: 1 }))));
    } else if (what === "backup") {
      if (!file || existsSync(file)) throw new Error("backup wants a new file name (an existing file is never overwritten)");
      const info = await link.ask([1, []], { timeout: 1500, retries: 1 });
      const version = String.fromCharCode(...info.slice(0, info.indexOf(0) < 0 ? info.length : info.indexOf(0)).filter((c) => c >= 32 && c < 127));
      let last = -1;
      const archive = await captureBackup((r, o) => link.ask(r, o), version, (done, total) => {
        const p = Math.floor(done / Math.max(1, total) * 10); if (p !== last) { last = p; process.stdout.write(`backup ${p * 10}%\r`); } });
      writeFileSync(file, JSON.stringify(archive));
      const back = readBackup(JSON.parse(readFileSync(file, "utf8")));
      console.log(`\nbackup of ${version}: ${back.objects.length} objects, ${back.objects.reduce((n, o) => n + o.size, 0)} bytes, written to ${file} and read back`);
    } else if (what === "pack-unchecked") {
      // installPack's own steps (web/fm1oc.js) with packProblem left out, so the firmware's check at PACK_END is what decides
      const bytes = new Uint8Array(readFileSync(file)), steps = [];
      const ask = async (req, t) => { const a = await link.ask(req, { timeout: t }); const r = a ? { rc: a[0], done: a[1] | a[2] << 7 | a[3] << 14 } : null; steps.push(r); return r; };
      let r = await ask(ocReq.packBegin(bytes.length), 10000);
      for (let off = 512; r && !r.rc && off < bytes.length; off += 256) r = await ask(ocReq.packWrite(off, bytes.subarray(off, Math.min(off + 256, bytes.length))), 1000);
      if (r && !r.rc) r = await ask(ocReq.packEnd(bytes.subarray(0, 512)), 3000);
      console.log("last reply", JSON.stringify(r), "after", steps.length, "requests");
      console.log(JSON.stringify(parsePackInfo(await link.ask(ocReq.packInfo(), { timeout: 1000, retries: 1 }))));
    } else {
      console.log("usage: oc_install.mjs info | module FILE.ocm | pack FILE.pak | erase-pack | remove ENGINE | backup FILE.json | pack-unchecked FILE");
      process.exitCode = 2;
    }
  } finally {
    link.close();
    m.close();
  }
}

main().catch((e) => { console.error("oc_install:", e.code || "", e.message); process.exitCode = 1; });
