// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Release 3 (docs/orangecat/UI_BUILD.md "Part 2"): parameter ids past 7 bits. INFO's tagged block, the 7-bit commands cut to 127
// values, the wide commands 130 to 134 as the unit frames them, and a legacy INFO read as before.
import { ok, eqBytes } from "./t.mjs";
import { CMD, parse, req, frame, unframe } from "../lib/editor_proto.js";

export async function run() {
  const str = (s) => [...[...s].map((c) => c.charCodeAt(0)), 0];
  /* an INFO reply's payload as editor.c writes it: version string, engines, P_COUNT (legacy byte), G_COUNT, NSTEP, P_E0, the names, NTRK, the trailer */
  function info({ pcount7, tail = [], engines = ["SYN"] }) {
    return [...str("ORANGE CAT 0.16.0"), engines.length, pcount7, 76, 64, 123, ...engines.flatMap(str), 4, 0, 0x55, 1, 7, ...tail];
  }

  const block = [0x57, 1, 131 & 127, 131 >> 7, 123 & 127, 123 >> 7, 3];
  let i = parse[CMD.INFO](info({ pcount7: 127, tail: block }));
  ok(i.pcount === 131 && i.pe0 === 123 && i.pcount7 === 127 && i.format === 3, "wide: INFO's tagged block gives the true counts (131, 123), the legacy byte stays 127, the format is 3");
  i = parse[CMD.INFO](info({ pcount7: 124 }));
  ok(i.pcount === 124 && i.pcount7 === 124 && i.format === 2, "wide: an INFO without the block reads as before (format 2, the legacy count)");
  i = parse[CMD.INFO](info({ pcount7: 124, tail: [0x57, 1, 9, 0, 9, 0, 9, 0, 0, 0, 0, 0, 0] }));
  ok(i.pcount === 124 && i.format === 2, "wide: a 0x57, 1 earlier in the trailer is not the block (it is anchored at the end)");
  i = parse[CMD.INFO](info({ pcount7: 100, tail: block }));
  ok(i.pcount === 100 && i.format === 2, "wide: a block that disagrees with the legacy byte (min(131, 127) is not 100) is not believed");

  /* the requests: ids up to 127 keep the 7-bit commands, past it the extension's */
  ok(req.get(0, 127)[0] === CMD.GET && req.set(0, 127, 5)[0] === CMD.SET && req.desc(0, 127)[0] === CMD.DESC && req.trackParam(1, 127)[0] === CMD.TRACK_PARAM, "wide: ids up to 127 go on the 7-bit commands");
  ok(eqBytes(req.get(0, 129)[1], [0, 1, 1]) && req.get(0, 129)[0] === CMD.PARAM_W, "wide: GET of id 129 is PARAM_W with the id as two bytes (1, 1)");
  ok(req.set(0, 130, 7)[0] === CMD.PARAM_W && req.set(0, 130, 7)[1].slice(0, 3).join() === "0,2,1", "wide: SET of id 130 is PARAM_W, id 2 + 1 x 128");
  ok(req.desc(1, 128)[0] === CMD.DESC_W && req.trackParam(2, 128, 3)[0] === CMD.TRACK_PARAM_W && req.trackParam(2, 128, 3)[1].slice(0, 3).join() === "2,0,1", "wide: DESC and TRACK_PARAM past 127 are DESC_W and TRACK_PARAM_W");
  /* the extension framing: command 0, then n = cmd - 128 */
  const f = frame(...req.get(0, 129));
  ok(f[4] === 0 && f[5] === CMD.PARAM_W - 128 && f.at(-1) === 0xF7, "wide: PARAM_W is framed under the extension (command 0, n = 2)");
  const u = unframe(f);
  ok(u.cmd === CMD.PARAM_W && eqBytes(u.a, [0, 1, 1]), "wide: and read back as command 130 with its bytes");

  /* the replies */
  const v14 = (v) => [(v + 8192) & 127, (v + 8192) >> 7];
  const pw = parse[CMD.PARAM_W]([0, 1, 1, ...v14(-5)]);
  ok(pw.scope === 0 && pw.id === 129 && pw.value === -5, "wide: PARAM_W reply: scope, the id from two bytes, the value");
  const tw = parse[CMD.TRACK_PARAM_W]([2, 0, 1, ...v14(40)]);
  ok(tw.track === 2 && tw.id === 128 && tw.value === 40, "wide: TRACK_PARAM_W reply");
  const dw = parse[CMD.DESC_W]([0, 0, 1, 1, ...v14(-64), ...v14(63), ...v14(0), ...str("AMT"), ...str("")]);
  ok(dw.id === 128 && dw.min === -64 && dw.max === 63 && dw.label === "AMT", "wide: DESC_W reply");
  const dump7 = parse[CMD.DUMP]([3, 9, ...Array.from({ length: 127 }, (_, k) => v14(k - 60)).flat(), ...Array.from({ length: 76 }, () => v14(0)).flat(), 0], { pcount: 131, pcount7: 127, gcount: 76 });
  ok(dump7.p.length === 127 && dump7.g.length === 76 && dump7.p[126] === 66, "wide: the 7-bit DUMP is read with the legacy count (127 values, then the 76 globals)");
  const dumpW = parse[CMD.DUMP_W]([3, 9, ...Array.from({ length: 131 }, (_, k) => v14(k - 60)).flat(), ...Array.from({ length: 76 }, () => v14(0)).flat(), 0], { pcount: 131, pcount7: 127, gcount: 76 });
  ok(dumpW.p.length === 131 && dumpW.g.length === 76 && dumpW.p[130] === 70, "wide: DUMP_W carries all 131");
  const pc = parse[CMD.PARAM_CHANGED_W]([0, 2, 1, ...v14(12)]);
  ok(pc.scope === 0 && pc.id === 130 && pc.value === 12, "wide: the PARAM_CHANGED_W push");

}
