// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 1: Connect, the Unit tab, the Engines table and the set install, against the mocked unit.
import { ok } from "./t.mjs";
import { makeAppMock } from "../lib/mock.js";
import { Session } from "../lib/session.js";
import { CMD } from "../lib/editor_proto.js";

/* an .ocm package (format 1) for the mock's unit, as tools/build.py --module lays it out (the shape test_web.mjs builds) */
const crc = (b) => { let c = 0xFFFFFFFF; for (const x of b) { c ^= x; for (let k = 0; k < 8; k++) c = c & 1 ? (c >>> 1) ^ 0xEDB88320 : c >>> 1; } return (c ^ 0xFFFFFFFF) >>> 0; };
export function ocmPackage({ engine = 14, iface = 0xE3CB357E, apiAddr = 0x02001100, hash = 0x657ED950, len = 3000, version = 0x000E0300 } = {}) {
  const starts = engine === 19 ? [0, 4] : [0, 6], tab = 56, hc = tab + 32, base = hc + 4;
  const imgs = starts.map((start, i) => {
    const im = new Uint8Array(len); for (let j = 0; j < len; j++) im[j] = (j * 7 + i + 1) & 255;
    const dv = new DataView(im.buffer), link = (0x020C4000 + start * 4096) >>> 0;
    dv.setUint32(0, 0x444F4D4F, true); dv.setUint32(4, iface, true); dv.setUint32(8, 60, true); dv.setUint32(12, 3, true);
    dv.setUint32(16, apiAddr, true); dv.setUint32(20, engine, true); dv.setUint32(24, 11136, true); dv.setUint32(28, link + 0x180, true);
    dv.setUint32(32, link, true); dv.setUint32(36, hash, true); dv.setUint32(40, 0, true); dv.setUint32(44, version, true); dv.setUint32(48, len, true);
    im.fill(0, 60, 0x100); dv.setUint32(52, crc(im.subarray(0x100)), true); dv.setUint32(56, crc(im.subarray(0, 56)), true);
    return im;
  });
  const b = new Uint8Array(base + 2 * len), dv = new DataView(b.buffer);
  dv.setUint32(0, 0x464D434F, true); dv.setUint16(4, 1, true); dv.setUint16(6, 2, true);
  dv.setUint32(8, iface, true); dv.setUint32(12, apiAddr, true); dv.setUint32(16, engine, true); dv.setUint32(20, 11136, true); dv.setUint32(24, hash, true);
  b.set(new TextEncoder().encode(engine === 19 ? "PROPHET" : "SHAPES"), 28); dv.setUint32(44, 3, true); dv.setUint32(48, 60, true); dv.setUint32(52, 6, true);
  imgs.forEach((im, i) => { dv.setUint32(tab + 16 * i, starts[i], true); dv.setUint32(tab + 16 * i + 4, len, true); dv.setUint32(tab + 16 * i + 8, crc(im), true); dv.setUint32(tab + 16 * i + 12, base + len * i, true); b.set(im, base + len * i); });
  dv.setUint32(hc, crc(b.subarray(0, hc)), true);
  return b;
}

export async function connected(opt = {}) {
  const m = makeAppMock(opt);
  const ports = Session.find(m.access);
  const s = new Session(ports.input, ports.output);
  await s.load();
  return { m, s };
}

export async function run() {
  const { m, s } = await connected();
  ok(s.info && s.info.ntrk === 4 && s.info.version.includes("MOCK") && s.pdesc.length === s.info.pcount && s.names.length === s.info.nengines,
     "unit: Connect reads INFO, every descriptor and every engine's preset names");
  ok(s.tracks && s.tracks.tracks.length === 4 && s.dump && s.dump.p.length === s.info.pcount, "unit: the tracks and the selected track's sound are read");
  ok(s.watch === 3 && s.v4, "unit: the live pushes are on (WATCH 3: any track's parameters)");
  ok(s.pack && s.pack.state === 1, "unit: the asset pack is read (115): this build's pack");
  ok(s.mod && s.mod.nstarts === 3 && s.engines && s.engines.engines.length === 20, "unit: the modules (120) and the Engines list (125) are read");
  ok(s.arrange && s.arrange.version === 1 && s.arrange.size === 2396 && s.arrange.pool.length === 64, "unit: the arrangement command (102) answers: version 1, the lists' size, the pool");

  const rows = s.engineRows();
  const shapes = rows.find((r) => r.num === 14), sid = rows.find((r) => r.num === 3), prophet = rows.find((r) => r.num === 19);
  ok(shapes && shapes.source === "module" && shapes.version === "0.14.1" && shapes.actions.includes("replace") && shapes.actions.includes("remove") && shapes.canUse,
     "unit: the Engines table: SHAPES a module with Replace, Remove and Use (the editor's enginesRows, unchanged)");
  ok(sid && sid.source === "builtin" && !sid.actions.length && sid.canUse && prophet && prophet.source === "builtin",
     "unit: a built-in engine has Use only; PROPHET's number is a module slot (Install when no module)");

  /* Remove, then Install from a file, then Use on a track */
  await s.removeEngine(14);
  const gone = s.engineRows().find((r) => r.num === 14);
  ok(gone && gone.source === "none" && gone.actions.includes("install") && m.ext.log.includes("remove 14"), "unit: Remove sends MOD_REMOVE and the row offers Install again");
  const pkg = ocmPackage();
  const r1 = await s.installModule({ num: 14, replace: false, start: null, tracks: 0 }, pkg);
  ok(r1.how === "installed" && s.engineRows().find((r) => r.num === 14).source === "module" && m.ext.log.some((x) => /^install 14 at/.test(x)),
     "unit: Install takes an .ocm, checks it against MOD_INFO and writes it to the start the unit names");
  let wrong = "";
  try { await s.installModule({ num: 19, replace: false }, pkg); } catch (e) { wrong = e.message; }
  ok(wrong === "modWrongEngine", "unit: Install of another engine's file on a row is refused before anything is sent");
  let bad = "";
  try { const b = ocmPackage(); b[100] ^= 1; await s.installModule({ num: 14, replace: true }, b); } catch (e) { bad = e.message; }
  ok(bad === "modWrongFile", "unit: a damaged package is refused (the editor's ocmParse)");
  const before = s.sel;
  const use = await s.useOnTrack(14, 2);
  ok(use.ok && s.tracks.tracks[2].engine === 14 && s.sel === before, "unit: Use on track 3 sets its engine (PRESET on that track) and keeps the selection");

  /* the set: the pack (already this build's: left) and a module, by the installer page's runSet */
  const sum = await s.installSet({ pack: new Uint8Array(0), modules: [ocmPackage({ version: 0x000E0400 })] }, { stages: { pack: true, module: true } });
  ok(sum.firmware === "off" && sum.pack === "current" && sum.module.length === 1 && sum.module[0] === "installed",
     "unit: Install set: firmware left to the installer page, the pack already current, the module installed (fm1oc.js runSet)");
  const after = await s.rq([CMD.PING, []]);
  ok(Array.isArray(after) && after[0] === 0, "unit: the page's own link has the port back after the set");

  /* an older firmware: no modules, no arrangement */
  const old = await connected({ noModules: true, noArrange: true });
  ok(!old.s.pack && !old.s.mod && !old.s.engines && !old.s.arrange && old.s.engineRows().length === 0, "unit: a firmware without 115..125 and 102: the parts are hidden, nothing is asked twice");
  old.s.close(); s.close();
}
