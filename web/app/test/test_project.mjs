// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app, Part 2 step 3 (Project) and step 6 (the backup): load and save through the backup objects and PROJECT, the
// codec on what the unit sends, export and import, the complete backup with the arrangement beside it, against the mock.
import { ok, eqBytes } from "./t.mjs";
import { connected } from "./test_unit.mjs";
import * as K from "../lib/codec.js";
import { readBackup } from "#fm1backup";
import { LISTS_SONG } from "../lib/mock_data.js";

export async function run() {
  const { m, s } = await connected();
  const x = m.ext;
  const now = await s.readProject(0);
  ok(now && eqBytes(now.bytes, x.runtime) && now.fbkk.name === "FULL 32", "project: the music now is read as backup object 0 and decoded (the firmware's record)");
  const slot = await s.readProject(2);
  ok(slot && slot.fbkk.name === "ONE BANK", "project: a saved slot is backup object 2..5");
  ok(await s.readProject(3) === null, "project: an empty slot reads as nothing");

  /* an edit written back as the music now (BACKUP_PUT 0): the unit takes it, the lists are a new project's (as on the unit) */
  now.fbkk.name = "FROM PAGE";
  now.fbkk.banks[0][now.fbkk.active[0]][0] = { notes: [48, 0, 0, 0], n: 1, time: 0, recorded: false, flags: 0, vel: 110, hit: 0, acc: 0, prob: 0 };
  const bytes = K.encodeFbkk(now.fbkk);
  await s.writeObject(0, bytes);
  ok(eqBytes(x.runtime, bytes) && K.decodeFbkk(x.runtime).name === "FROM PAGE", "project: a project written as the music now arrives byte for byte (begin, data, commit)");
  x.playing = true;
  let refused = "";
  try { await s.writeObject(0, bytes); } catch (e) { refused = e.message; }
  x.playing = false;
  ok(/Stop playback/.test(refused), "project: a write while the unit plays is refused (rc 3) and aborted");

  /* PROJECT save / load: the unit's own, the lists go with them */
  const lists0 = Uint8Array.from(x.lists);
  await s.projectSave(1);
  ok(s.slotUsed[1] === 1 && eqBytes(x.slots[1], x.runtime) && eqBytes(x.slotLists[1], lists0), "project: Save the music now here is PROJECT save (the record and its lists)");
  await s.projectLoad(0);
  ok(eqBytes(x.runtime, x.slots[0]) && K.decodeFbkk(x.runtime).name === "ONE BANK", "project: Load it on the unit is PROJECT load");

  /* the complete backup, with the arrangement beside it; restored onto the same unit (the same pool) the arrangement comes back */
  await s.projectLoad(1);
  x.lists = Uint8Array.from(atob(LISTS_SONG), (c) => c.charCodeAt(0));   /* (chains that name phrases 01..64) */
  const file = await s.backup();
  ok(file.format === "felucca-backup" && file.objects.length === 29 && file.arrangement && file.arrangement.pool.length === 64 && readBackup(JSON.parse(JSON.stringify(file))),
     "project: the complete backup reads every object (fm1backup.js captureBackup) and adds the arrangement of the music now with the pool's CRCs");
  const listsBefore = Uint8Array.from(x.lists);
  x.lists = K.encodeLists(K.decodeLists(listsBefore)); x.runtime[100] ^= 0;   /* (unchanged) */
  const r1 = await s.restore(JSON.stringify(file));
  ok(r1.arrangement === "restored" && eqBytes(x.lists.subarray(0, 2304), listsBefore.subarray(0, 2304)),
     "project: restored onto a unit with the same phrases, the arrangement is written back too (after the music, which brings a new project's lists)");
  x.pool[0].crc ^= 1;                                     /* a phrase the lists name changed on the unit */
  const r2 = await s.restore(JSON.stringify(file));
  ok(r2.arrangement === "different pool", "project: a unit whose phrases differ gets the music, not the arrangement (the phrase numbers would name other music)");
  x.pool[0].crc ^= 1;
  s.close();
}
