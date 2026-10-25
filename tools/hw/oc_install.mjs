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
//
// PYTHON names the interpreter with mido (default: python3). Close every other program on the port first (fm1hw.py, the editor page).
// It never removes a module, wipes the library or restores a backup.
import { readFileSync } from "fs";
import { spawn } from "child_process";
import { createInterface } from "readline";
import { fileURLToPath } from "url";
import { OcLink, OCM, ocReq, OC_CMD, probeUnit, installPack, installModule, parsePackInfo } from "../../web/fm1oc.js";

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
      await installModule(link, ocm, (f) => { const p = Math.floor(f * 10); if (p !== last) { last = p; process.stdout.write(`module ${p * 10}%\r`); } }, { confirmInPlace: () => true });
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
    } else {
      console.log("usage: oc_install.mjs info | module FILE.ocm | pack FILE.pak | erase-pack");
      process.exitCode = 2;
    }
  } finally {
    link.close();
    m.close();
  }
}

main().catch((e) => { console.error("oc_install:", e.code || "", e.message); process.exitCode = 1; });
