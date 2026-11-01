// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 4: Arrange (the pool, chains, the song, scenes) read with command 102, edited in the page, the diff,
// written back; refused while playing; the unit keeps its own places. Against the mock (which answers 102 as editor_arrange.c does).
import { ok, eqBytes } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import * as K from "../lib/codec.js";
import { bkU32 } from "#fm1backup";
import { arrReq, parseInfo, parsePage, readPoolPages } from "../lib/arrange.js";

export async function run() {
  const { m, s } = await connected();
  const x = m.ext, L = K.LAYOUT.lists;
  const r = await s.readLists();
  ok(eqBytes(r.bytes, x.lists) && r.info.pool.length === 64 && r.info.pool[0].hasSteps && r.info.pool[0].inChain,
     "arrange: the lists read whole (INFO, GET in pieces, the CRC checked); the pool's flags");
  const ed = K.cloneLists(r.lists);
  /* a chain reordered (drag B before A), a song cell, a scene kept, a groove */
  const [a, b] = [ed.chains[0][0], ed.chains[0][1]];
  ed.chains[0].splice(0, 2, b, a);
  ed.song[20][3] = 9; ed.sceneUsed |= 4; ed.scenes[2][1] = { chain: 3, step: 2, mute: true }; ed.groove[3] = 5; ed.gamt[3] = 80;
  ed.tchain[0] = 40; ed.cur[0] = 9;                               /* (a page can not move the tracks: the unit keeps its own) */
  const diff = K.diffLists(r.lists, ed);
  ok(diff.length === 4 && diff[0].kind === "chain" && /A: phrase 01 -> phrase 02 x2, B: phrase 02 x2 -> phrase 01/.test(diff[0].text),
     "arrange: the diff before the write: the chain's order, the song cell, the scene, the groove");
  x.playing = true;
  let e1 = null;
  try { await s.writeLists(K.encodeLists(ed)); } catch (e) { e1 = e; }
  x.playing = false;
  ok(e1 && e1.code === "playing" && eqBytes(x.lists, r.bytes), "arrange: a write while the unit plays is refused (rc 3); nothing changes");
  const w = await s.writeLists(K.encodeLists(ed));
  const back = await s.readLists();
  ok(w.rc === 0 && w.track === null && back.lists.chains[0][0].phrase === b.phrase && back.lists.chains[0][1].phrase === a.phrase && back.lists.song[20][3] === 9 &&
     back.lists.groove[3] === 5 && back.lists.scenes[2][1].mute, "arrange: written (BEGIN, DATA, COMMIT) and read back as written");
  ok(back.lists.tchain[0] === r.lists.tchain[0] && back.lists.cur[0] === r.lists.cur[0] && back.lists.mode === r.lists.mode,
     "arrange: the unit kept where each track is and PLAY's mode");
  ok(K.diffLists(back.lists, ed).length === 0, "arrange: after the write the page and the unit agree (no diff)");
  /* the checks the unit makes are made before the write */
  const bad = K.cloneLists(back.lists); bad.scenes[0][0].step = 16;
  ok(K.listsProblem(bad).startsWith("scene 1"), "arrange: a scene step past P is caught in the page");
  const badBytes = K.encodeLists(bad);
  let e2 = null;
  try { await s.writeLists(badBytes); } catch (e) { e2 = e; }
  ok(e2 && e2.code === "invalid", "arrange: and refused by the unit (rc 2) if it were sent");
  /* INFO as editor_arrange.c builds it */
  const info = parseInfo(await s.rq(arrReq.info()));
  ok(info.version === 1 && info.size === L.size && info.npool === 64 && info.nchain === 64 && info.nent === 16 && info.nrow === 64 && info.nscene === 8,
     "arrange: INFO's sizes are the layout's (tests/app_layout.c)");
  /* a firmware without 102 */
  const old = await connected({ noArrange: true });
  let e3 = null;
  try { await old.s.readLists(); } catch (e) { e3 = e; }
  ok(!old.s.arrange && e3 && e3.code === "noArrange", "arrange: a firmware without the command: the tab says so, nothing is written");
  old.s.close(); s.close();
  {   /* (Release 3, Part 6) the wide pool, read in pages of 64 with op 5 */
    const page = (p) => { const n = p === 3 ? 63 : 64, out = [5, 0, p, n]; for (let i = 0; i < n; i++) out.push(i % 3 ? 0 : 1, 5, ...bkU32(p * 64 + i)); return out; };
    const info = { pool: Array.from({ length: 64 }, (_, i) => ({ id: i + 1 })), npool: 64 };
    const asked = [];
    await readPoolPages(async ([cmd, a]) => { asked.push(a[1]); return page(a[1]); }, info);
    ok(info.npool === 255 && info.pool.length === 255 && info.pool[254].id === 255 && info.pool[64].id === 65 && asked.join() === "1,2,3", "arrange: INFO_PAGE 1..3 extend the pool to 255 phrases");
    ok(parsePage(page(0))[3].crc === 3 && parsePage(page(0))[0].hasSteps === true && parsePage(page(0))[1].hasSteps === false, "arrange: a page's flags, LEN and CRC parse as INFO's");
    let bad = false; try { parsePage([5, 1, 0, 0]); } catch { bad = true; }
    ok(bad, "arrange: a page the unit refuses (rc 1) throws");
  }
}
