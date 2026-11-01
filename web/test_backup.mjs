// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// fm1backup.js against a simulated device: 7-bit packing, CRC, manifest checks, a full capture,
// validation before the first write, restore order (live music last) and abort on a failed chunk.
import { BACKUP_IDS, ARR_IDS, BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup, restoreBackup } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throws = (f) => { try { f(); return false; } catch { return true; } };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
ok(bkR32(bkU32(0xdeadbeef)) === 0xdeadbeef && bkCrc(new TextEncoder().encode("123456789")) === 0xcbf43926, "backup: numbers and CRC-32");
const b = rnd(1000, 1);
ok(bkUnpack(bkPack(b), b.length).every((v, i) => v === b[i]), "backup: 7-bit pack / unpack round trip");
ok(throws(() => bkUnpack([...bkPack(b.subarray(0, 14)), 0], 14)), "backup: trailing bytes refused");

/* a device: objects by id, sample slots, the order of writes */
function device(objs, opt = {}) {
  const d = { objs: new Map(objs), log: [], staged: null };
  const ids = opt.ids || BACKUP_IDS;
  if (!d.objs.has(0)) d.objs.set(0, rnd(opt.runtimeSize || 20224, 9));
  d.request = async ([cmd, a]) => {
    if (cmd === BACKUP_CMD.LIST) {
      if (a.length && !opt.arr) return [1, 1, 0];               /* (a firmware without the arrangement objects: LIST 1 refused) */
      const list = a.length ? ARR_IDS : ids;
      const out = [1, 0, list.length];
      for (const id of list) { const v = d.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return out;
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = d.objs.get(id);
      if (opt.changeOnGet === id && off === 0) v[0] ^= 1;
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === BACKUP_CMD.PUT) {
      const [op, id] = a;
      if (op === 0) { const size = bkR32(a, 2), rc = opt.refuse?.(id, size) || 0; if (!rc) d.staged = { id, size, crc: bkR32(a, 7), bytes: [], chunks: 0 }; return [op, id, rc]; }
      if (op === 1) {
        d.staged.chunks++;
        if (opt.failChunk === id) return [op, id, 2];
        const off = bkR32(a, 2), p = a.slice(7), n = Math.min(256, d.staged.size - off);
        d.staged.bytes.push(...bkUnpack(p, n)); return [op, id, 0];
      }
      if (op === 2) { const s = Uint8Array.from(d.staged.bytes); const rc = bkCrc(s) === d.staged.crc ? 0 : 2; if (!rc) { d.objs.set(id, s); d.log.push(id); } return [op, id, rc]; }
      if (op === 3) { if (d.staged?.chunks) d.log.push(`abort ${id}`); return [op, id, 0]; }  /* (a preflight: no chunks) */
    }
    if (cmd === 11 || cmd === 12 || cmd === 13 || cmd === 14) { if (cmd === 13 || cmd === 14) d.log.push(32 + a[0]); return [a[0], 0]; }
    throw new Error(`unexpected ${cmd}`);
  };
  return d;
}
const objs = [[0, rnd(3388, 2)], [1, rnd(1200, 3)], [2, rnd(3388, 4)], [6, rnd(3080, 5)]];
const dev = device(objs);
const file = await captureBackup(dev.request, "TEST");
ok(file.objects.length === BACKUP_IDS.length && file.objects[0].size === 3388 && file.objects[3].size === 0, "backup: capture lists every object, empty ones as 0");
ok(readBackup(JSON.stringify(file)).objects[2].bytes.every((v, i) => v === objs[2][1][i]), "backup: capture -> file -> bytes round trip");
ok(await athrows(() => captureBackup(device(objs.map(([i, v]) => [i, v.slice()]), { changeOnGet: 2 }).request, "TEST")), "backup: a device that changes during capture fails the capture");

const bad = JSON.parse(JSON.stringify(file)); bad.objects[2].crc ^= 1;
ok(throws(() => readBackup(bad)), "backup: a damaged file is refused");
const noRun = JSON.parse(JSON.stringify(file)); noRun.objects[0] = { ...noRun.objects[0], size: 0, crc: 0, data: "" };
ok(throws(() => readBackup(noRun)), "backup: a file without the current music is refused");
ok(throws(() => bkManifest([1, 0, 3])), "backup: a short manifest is refused");

const target = device([]);
const damaged = JSON.parse(JSON.stringify(file)); damaged.objects[6].crc ^= 1;
ok(await athrows(() => restoreBackup(target.request, damaged)) && target.log.length === 0, "backup: restore validates every byte before the first write");
await restoreBackup(target.request, file);
const order = target.log.filter((x) => typeof x === "number");
ok(order.at(-1) === 0 && order.at(-2) === 1 && order.indexOf(2) < order.indexOf(1), "backup: restore order: projects, banks, samples, settings, live music last");
ok([0, 1, 2, 6].every((id) => target.objs.get(id).every((v, i) => v === objs.find((o) => o[0] === id)[1][i])), "backup: restored objects equal the source");
{   /* the FM6 patch bank (id 8) and the archives of firmware before it (11 objects, no id 8) */
  ok(BACKUP_IDS.includes(8) && file.objects.some((o) => o.id === 8), "backup: the FM6 bank (id 8) is part of the archive");
  const old = JSON.parse(JSON.stringify(file)); old.objects = old.objects.filter((o) => o.id !== 8);
  ok(readBackup(old).objects.length === 11, "backup: an archive of the 11 objects before FM6 still reads");
  const odd = JSON.parse(JSON.stringify(file)); odd.objects = odd.objects.filter((o) => o.id !== 33);
  ok(throws(() => readBackup(odd)), "backup: an archive missing another object is refused");
}
const failing = device([], { failChunk: 2 });
ok(await athrows(() => restoreBackup(failing.request, file)) && failing.log.includes("abort 2") && !failing.log.includes(0), "backup: a refused chunk aborts that object and stops before the live music");

const bankIds = BACKUP_IDS.filter(id => id < 32);
const bankObjects = [[0, rnd(20224, 10)], [1, rnd(1200, 11)], [2, rnd(20224, 12)]];
const bankFile = await captureBackup(device(bankObjects, { ids: bankIds }).request, "BANKS");
ok(readBackup(bankFile).objects.length === 9 && bankFile.objects[2].size === 20224,
  "backup: captures all nine bank-era objects, including full saved banks");
const bankTarget = device([], { ids: bankIds });
await restoreBackup(bankTarget.request, bankFile);
ok(bankTarget.log.at(-1) === 0 && bankTarget.objs.get(2).every((v, i) => v === bankObjects[2][1][i]),
  "backup: full bank archive restores with current music last");
const legacyTarget = device([], { runtimeSize: 3584 });
ok(await athrows(() => restoreBackup(legacyTarget.request, bankFile)) && !legacyTarget.log.length,
  "backup: bank archive is refused by older firmware before any write");
const migratedTarget = device([], { ids: bankIds });
await restoreBackup(migratedTarget.request, file);
ok(!migratedTarget.log.some(id => id >= 32) && migratedTarget.objs.get(0).length === 3388,
  "backup: legacy archive without samples migrates, skipping retired empty slots");
const sample = new Uint8Array(514), sampleView = new DataView(sample.buffer);
sampleView.setUint32(0, 0x504d5346, true); sampleView.setUint16(4, 1, true); sample[6] = 1;
sampleView.setUint32(16, 2, true); sampleView.setUint32(20, bkCrc(sample.subarray(512)), true);
sampleView.setUint32(36, 4, true); sampleView.setUint32(44, 3, true); sampleView.setUint32(48, 22050, true);
const sampleFile = await captureBackup(device([...objs, [32, sample]]).request, "LEGACY");
const noSampleTarget = device([], { ids: bankIds });
ok(await athrows(() => restoreBackup(noSampleTarget.request, sampleFile)) && !noSampleTarget.log.length,
  "backup: nonempty retired sample slots are refused before any write");

const czIds = Array.from({length:17},(_,i)=>i);
const czBank = new Uint8Array(2332), czView = new DataView(czBank.buffer);
czView.setUint32(0,0x42435a43,true);czView.setUint16(4,1,true);czView.setUint16(6,16,true);czView.setUint32(8,1,true);
czBank.set(new TextEncoder().encode("MY CZ BANK"),12);czBank[28+128]=65;
const czFile=await captureBackup(device([...bankObjects,[9,czBank],[16,czBank.slice()]],{ids:czIds}).request,"CZ BANKS");
ok(readBackup(czFile).objects.length===17,"backup: captures all eight dedicated CZ banks");
const czTarget=device([],{ids:czIds});await restoreBackup(czTarget.request,czFile);
ok(czTarget.log.at(-1)===0 && [9,16].every(id=>czTarget.objs.get(id).every((x,i)=>x===czBank[i])),"backup: native CZ banks restore byte-exact with live music last");
const nextBackup=JSON.parse(JSON.stringify(bankFile));nextBackup.objects[0].size=19092;const nextRaw=rnd(19092,14);nextBackup.objects[0].crc=bkCrc(nextRaw);nextBackup.objects[0].data=Buffer.from(nextRaw).toString("base64");
const nextTarget=device([],{ids:czIds,runtimeSize:19008});await restoreBackup(nextTarget.request,nextBackup);
ok(nextTarget.log.at(-1)===0,"backup: previous next CZ bank format reaches firmware migration");
const badBank=JSON.parse(JSON.stringify(czFile));const broken=czBank.slice();broken[0]^=1;
badBank.objects[9]={...badBank.objects[9],crc:bkCrc(broken),data:Buffer.from(broken).toString("base64")};
const protectedTarget=device([],{ids:czIds});
ok(await athrows(()=>restoreBackup(protectedTarget.request,badBank)) && !protectedTarget.log.length,"backup: corrupt CZ bank refused before any writes");
const oldToCz=device([[9,czBank]],{ids:czIds});await restoreBackup(oldToCz.request,bankFile);
ok(!oldToCz.log.includes(9) && oldToCz.objs.get(9)===czBank,"backup: older archives preserve dedicated CZ banks absent from archive");

const largeObjects=[[0,rnd(27200,20)],[1,rnd(1200,21)],[2,rnd(27200,22)]];
// The ownership backport appends user-record and voice objects after the CZ banks.
const ownedIds=Array.from({length:21},(_,i)=>i);
const ownedData=[[17,rnd(3816,20)],[18,rnd(3816,21)],[19,rnd(3728,22)],[20,rnd(3728,23)]];
const ownedFile=await captureBackup(device([...bankObjects,...ownedData],{ids:ownedIds}).request,"OWNED FM6");
const ownedTarget=device([],{ids:ownedIds});await restoreBackup(ownedTarget.request,ownedFile);
ok(readBackup(ownedFile).objects.length===21 && ownedData.every(([id,bytes])=>ownedTarget.objs.get(id).every((v,i)=>v===bytes[i])),"backup: all 64 preset records and their owned FM6 voices round trip");
ok(ownedTarget.log.at(-1)===0 && ownedTarget.log.indexOf(18)<ownedTarget.log.indexOf(20),"backup: user records restore before their voices, live music last");
const largeFile=await captureBackup(device(largeObjects,{ids:czIds,runtimeSize:27200}).request,"1024 NOTES");
const largeTarget=device([],{ids:czIds,runtimeSize:27200});await restoreBackup(largeTarget.request,largeFile);
ok(largeTarget.objs.get(2).every((v,i)=>v===largeObjects[2][1][i]),"backup: expanded 1024-note projects capture and restore completely");
const smallTarget=device([],{ids:czIds,runtimeSize:20224});
ok(await athrows(()=>restoreBackup(smallTarget.request,largeFile)) && !smallTarget.log.length,"backup: expanded archives are refused by 152-note firmware before any write");
const mixed=JSON.parse(JSON.stringify(largeFile));mixed.objects[0]=bankFile.objects[0];
const mixedTarget=device([],{ids:czIds,runtimeSize:20224});
ok(await athrows(()=>restoreBackup(mixedTarget.request,mixed)) && !mixedTarget.log.length,"backup: preflight checks saved slots too when runtime is an older format");

const nativeIds=Array.from({length:23},(_,i)=>i),nativeFM=new Uint8Array(2060),nv=new DataView(nativeFM.buffer);
nv.setUint32(0,0x314d464e,true);nv.setUint16(4,1,true);nv.setUint16(6,16,true);nv.setUint32(8,0x8001,true);
for(const slot of [0,15])for(let i=0;i<128;i++)nativeFM[12+slot*128+i]=(i*17+slot)&127;
const nativeCZ=czBank.slice();new DataView(nativeCZ.buffer).setUint16(4,2,true);
const nativeObjects=[[9,nativeCZ],...[19,20,21,22].map(id=>[id,nativeFM.slice()])];
const nativeFile=await captureBackup(device([...bankObjects,...nativeObjects],{ids:nativeIds}).request,"NATIVE PRESETS");
const nativeTarget=device([],{ids:nativeIds});await restoreBackup(nativeTarget.request,nativeFile);
ok(readBackup(nativeFile).objects.length===23 && nativeObjects.every(([id,bytes])=>nativeTarget.objs.get(id).every((v,i)=>v===bytes[i])),"backup: independent native FM6/CZ pools round trip in 23-object archive");
const badNative=JSON.parse(JSON.stringify(nativeFile)),invalidFM=nativeFM.slice();invalidFM[12]=128;
badNative.objects[21]={...badNative.objects[21],crc:bkCrc(invalidFM),data:Buffer.from(invalidFM).toString("base64")};
const nativeProtected=device([],{ids:nativeIds});
ok(await athrows(()=>restoreBackup(nativeProtected.request,badNative)) && !nativeProtected.log.length,"backup: malformed native voice refused before any restore writes");
const oldOwnedTarget=device([],{ids:nativeIds,refuse:(id,size)=>id>=19&&id<=22&&size!==2060?1:0});
ok(await athrows(()=>restoreBackup(oldOwnedTarget.request,ownedFile)) && !oldOwnedTarget.log.length,"backup: an archive the firmware refuses (old owned voices) writes nothing");

const sharedIds=Array.from({length:29},(_,i)=>i),sharedData=new Uint8Array(3716),sharedView=new DataView(sharedData.buffer);
sharedView.setUint32(0,0x32425055,true);sharedView.setUint16(4,206,true);sharedView.setUint16(6,18,true);
const sharedFile=await captureBackup(device([...bankObjects,[28,sharedData]],{ids:sharedIds}).request,"SHARED BANK");
ok(sharedFile.objects.length===29&&sharedFile.objects[28].size===3716,"backup: 29-object archive includes the final shared slots");
const sharedTarget=device([],{ids:sharedIds});await restoreBackup(sharedTarget.request,sharedFile);
ok(sharedTarget.objs.get(28).every((v,i)=>v===sharedData[i]),"backup: final compact shared bank restores byte for byte");
{   /* (0.14.2) the arrangement objects: 64..67 project records, 68..83 the pool's chunks, 84 the music's lists */
  const rec = (n, seed) => { const v = rnd(n, seed); new DataView(v.buffer).setUint32(0, 0x334a434f, true); return v; };
  const chunk = (lens, seed) => { const v = rnd(8 + lens.reduce((a, b) => a + b, 0), seed), d = new DataView(v.buffer); lens.forEach((l, j) => d.setUint16(2 * j, l, true)); return v; };
  const arrObjs = [...bankObjects, [28, sharedData], [64, rec(3200, 21)], [66, rec(900, 22)], [68, chunk([1249, 0, 30000, 7], 23)], [84, rnd(2428, 24)]];
  const src = device(arrObjs, { ids: sharedIds, arr: true });
  const arrFile = await captureBackup(src.request, "ARRANGEMENT");
  ok(arrFile.objects.length === 50 && arrFile.objects[29].id === 64 && arrFile.objects[49].id === 84 && arrFile.objects[33].size === 31264,
     "backup: a firmware with LIST 1: 50 objects, the arrangement's (64..84) after ids 0..28");
  ok(readBackup(JSON.stringify(arrFile)).objects.length === 50, "backup: a 50-object archive reads");
  const plain = await captureBackup(device([...bankObjects, [28, sharedData]], { ids: sharedIds }).request, "PLAIN");
  ok(plain.objects.length === 29, "backup: a firmware without them (LIST 1 refused): the 29 objects as before");
  const badRec = JSON.parse(JSON.stringify(arrFile)); { const v = rec(3200, 21); v[0] = 0; badRec.objects[29] = { ...badRec.objects[29], crc: bkCrc(v), data: Buffer.from(v).toString("base64") }; }
  ok(throws(() => readBackup(badRec)), "backup: a project record without its magic is refused");
  const badChunk = JSON.parse(JSON.stringify(arrFile)); { const v = chunk([1249, 0, 30000, 7], 23); v[0] ^= 1; badChunk.objects[33] = { ...badChunk.objects[33], crc: bkCrc(v), data: Buffer.from(v).toString("base64") }; }
  ok(throws(() => readBackup(badChunk)), "backup: a phrase chunk whose table does not add up is refused");
  const dst = device([], { ids: sharedIds, arr: true });
  await restoreBackup(dst.request, arrFile);
  const order = dst.log.filter((x) => typeof x === "number");
  ok(order.indexOf(5) < order.indexOf(64) && order.indexOf(83) < order.indexOf(1) && order.at(-2) === 0 && order.at(-1) === 84,
     "backup: restore order: slots 2..5, then 64..83, settings, the live music, its arrangement (84) last");
  ok([64, 66, 68, 84].every((id) => dst.objs.get(id).every((v, i) => v === arrObjs.find((o) => o[0] === id)[1][i])) && dst.objs.get(65).length === 0,
     "backup: the arrangement objects restore byte for byte, an empty one as empty");
  const old = device([], { ids: sharedIds });
  ok(await athrows(() => restoreBackup(old.request, arrFile)) && !old.log.length, "backup: a firmware without them refuses the archive before any write");
}
{   /* (Release 3, Part 6) the records by type and id */
  const { REC, recKey } = await import("./fm1backup.js");
  const mkChunk = (slot, gen, c, seed) => ({ type: REC.POOL, id: 0x8000 | slot << 9 | gen << 8 | c, bytes: (() => { const v = rnd(7, seed); v[0] = 1; v[1] = 1; v[2] = 3; v[3] = 0; return v; })() });
  const mkProj = (slot, seed) => ({ type: REC.PROJECT, id: 0x8000 | slot, bytes: (() => { const v = rnd(300, seed); v.set([0x4f, 0x43, 0x4a, 0x34]); return v; })() });
  /* a device with the records commands: a store of records by key, the order of writes, a conflict rule (rc 6) for chunks its project names */
  function keyedDevice(recs, opt = {}) {
    const base = device([[0, rnd(3388, 2)], [1, rnd(1200, 3)]], { ids: BACKUP_IDS });
    const st = { recs: new Map(recs.map((r) => [r.type * 65536 + r.id, r.bytes])), log: [], staged: null, base };
    const key = (a, p) => [a[p], a[p + 1] | a[p + 2] << 7 | a[p + 3] << 14];
    st.request = async ([cmd, a]) => {
      if (cmd !== 0) return base.request([cmd, a]);
      if (opt.noRecords) throw new Error("timeout");
      const sub = a[0];
      if (sub === REC.LIST) {
        const start = a[1] | a[2] << 7, all = [...st.recs.entries()], page = all.slice(start, start + 16);
        const out = [REC.LIST, 0, a[1], a[2], page.length, start + page.length < all.length ? 1 : 0];
        for (const [k, v] of page) out.push(...recKey(k >>> 16, k & 0xffff), ...bkU32(v.length), ...bkU32(bkCrc(v)));
        return out;
      }
      if (sub === REC.GET) {
        const [t, id] = key(a, 1), off = bkR32(a, 5), n = a[10] | a[11] << 7, v = st.recs.get(t * 65536 + id);
        if (opt.changeOnList && off === 0 && t === REC.PROJECT && id === 0x8001) st.recs.set(REC.POOL * 65536 + 0x8003, Uint8Array.from([1, 1, 1, 0, 9]));
        return [REC.GET, ...recKey(t, id), 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
      }
      if (sub === REC.PUT) {
        const op = a[1], [t, id] = key(a, 2);
        if (op === 0) {
          st.log.push(`begin ${t}:${id.toString(16)}`);
          if (t === REC.POOL && opt.conflict?.(id) && !st.cleared) return [REC.PUT, 0, 6];
          st.staged = { t, id, size: bkR32(a, 6), crc: bkR32(a, 11), bytes: [] }; return [REC.PUT, 0, 0];
        }
        if (op === 1) { const off = bkR32(a, 6), n = Math.min(256, st.staged.size - off); st.staged.bytes.push(...bkUnpack(a.slice(11), n)); return [REC.PUT, 1, 0]; }
        if (op === 2) { const s = Uint8Array.from(st.staged.bytes), rc = bkCrc(s) === st.staged.crc ? 0 : 2; if (!rc) { st.recs.set(st.staged.t * 65536 + st.staged.id, s); st.log.push(`commit ${st.staged.t}:${st.staged.id.toString(16)}`); } return [REC.PUT, 2, rc]; }
        if (op === 3) { st.log.push("abort"); return [REC.PUT, 3, 0]; }
        if (op === 4) { st.log.push("finish"); return [REC.PUT, 4, 0]; }
        if (op === 5) { st.log.push(`clear ${id.toString(16)}`); st.cleared = true; st.recs.delete(REC.PROJECT * 65536 + id); return [REC.PUT, 5, 0]; }
      }
      throw new Error("unexpected ext");
    };
    return st;
  }
  const recs = [mkChunk(0, 0, 1, 31), mkChunk(0, 0, 2, 32), mkChunk(1, 1, 1, 33), mkProj(0, 34), mkProj(1, 35)];
  const src = keyedDevice(recs);
  const f2 = await captureBackup(src.request, "R3");
  ok(f2.version === 2 && f2.records.length === 5 && f2.records[3].type === REC.PROJECT && f2.records[3].id === 0x8000, "records: a capture keeps every record by type and id, as version 2");
  const back = readBackup(JSON.stringify(f2));
  ok(back.records.every((r, i) => r.bytes.every((v, j) => v === recs[i].bytes[j])) && back.version === 2, "records: the archive reads back byte for byte");
  ok(readBackup(JSON.stringify(await captureBackup(device(objs.map(([i, v]) => [i, v.slice()])).request, "OLDFW"))).records.length === 0, "records: a firmware without them (no reply to LISTR): no records, version 1");
  ok(throws(() => readBackup({ ...JSON.parse(JSON.stringify(f2)), version: 1 })), "records: a version 1 archive that carries records is refused");
  { const bad = JSON.parse(JSON.stringify(f2)); bad.records[0].crc ^= 1; ok(throws(() => readBackup(bad)), "records: a damaged record is refused"); }
  { const bad = JSON.parse(JSON.stringify(f2)); bad.records[3].id = 0x8009; ok(throws(() => readBackup(bad)), "records: a project record id the format has not is refused"); }
  { const bad = JSON.parse(JSON.stringify(f2)); bad.records.push({ ...bad.records[0] }); ok(throws(() => readBackup(bad)), "records: a record twice is refused"); }
  { const d4 = keyedDevice(recs, { changeOnList: true }); ok(await athrows(() => captureBackup(d4.request, "X")), "records: a save between the records and the second inventory fails the capture"); }
  const dstK = keyedDevice([]);
  await restoreBackup(dstK.request, f2);
  const lg = dstK.log;
  ok(lg.at(-1) === "finish" && lg.indexOf("commit 2:8002") < lg.indexOf("commit 1:8000") && lg.filter((x) => x.startsWith("commit 2:")).length === 3 && lg.indexOf("finish") === lg.length - 1,
     "records: restore writes the pool chunks, then the project records, then finishes");
  ok(recs.every((r) => dstK.recs.get(r.type * 65536 + r.id)?.every((v, j) => v === r.bytes[j])), "records: every record restored byte for byte");
  { const base = device(objs.map(([i, v]) => [i, v.slice()])); ok(await athrows(() => restoreBackup(base.request, f2)) && !base.log.length, "records: a firmware without the commands refuses an archive with records before any write"); }
  { const d3 = keyedDevice([mkProj(0, 77)], { conflict: (id) => id === (0x8000 | 1) });
    await restoreBackup(d3.request, f2);
    ok(d3.log.includes("clear 8000") && d3.log.indexOf("clear 8000") < d3.log.indexOf("commit 2:8001"), "records: a chunk the slot's record names is written after the slot is cleared (rc 6 -> clear)"); }
}
{   /* (Release 3, Part 6) the installer's gate: the unit's format from INFO, and what is owed before a firmware of format 3 */
  const { infoFormat, r3Gate } = await import("./fm1backup.js");
  const tail = (format) => [0x55, 1, 7, 0x57, 1, 3, 1, 123, 0, format];
  ok(infoFormat([65, 0, 1, 2, ...tail(3)]) === 3 && infoFormat([65, 0, 1, 2, ...tail(2)]) === 2 && infoFormat([65, 0, 1, 2, 3, 4, 5, 6, 7, 8]) === 2 && infoFormat([]) === 2,
     "gate: the unit's format is INFO's tagged block, 2 when there is none");
  const can = { answered: true, format: 2, supported: true };
  ok(r3Gate(3, can).kind === "gate" && !r3Gate(3, can).downgrade, "gate: a unit on the format before takes the warning and the backup before a Release 3 firmware");
  ok(r3Gate(3, { answered: true, format: 3, supported: true }).kind === "none", "gate: a unit already on Release 3 sees nothing");
  ok(r3Gate(3, { answered: false, format: 2, supported: false }).kind === "warn", "gate: a unit that does not answer gets the warning and a confirm, no backup");
  ok(r3Gate(3, { answered: true, format: 2, supported: false }).kind === "warn", "gate: a unit that cannot export a backup gets the warning and a confirm, no backup");
  ok(r3Gate(2, can).kind === "none" && r3Gate(undefined, can).kind === "none", "gate: a firmware of the format before shows nothing on a unit of the format before");
  ok(r3Gate(2, { answered: true, format: 3, supported: true }).kind === "gate" && r3Gate(2, { answered: true, format: 3, supported: true }).downgrade,
     "gate: an older firmware on a unit of converted projects: the downgrade warning and a backup (nothing is refused)");
  const page = (await import("node:fs")).readFileSync(new URL("index_pkg.html", import.meta.url), "utf8");
  for (const lang of ["ja", "en"]) {
    const block = page.match(new RegExp(`\\n  ${lang}: \\{([\\s\\S]*?)\\n  \\},?\\n`))?.[1] || "";
    ok(["r3Warn", "r3Down", "r3Backup", "r3Saved", "r3NoBackup"].every((k) => new RegExp(`\\b${k}:`).test(block)), `gate: the page has the warning, the backup and the confirm texts in ${lang}`);
  }
  ok(page.indexOf("await r3Preflight(midi") > 0 && page.indexOf("await r3Preflight(midi") < page.indexOf("files.pack = await getFile") && page.indexOf("await r3Preflight(midi") < page.indexOf("runFirmware:"),
     "gate: the preflight runs before anything is fetched or written, for the firmware stage only");
  ok(/Do not save projects with an older firmware afterward; install this firmware again to get them back/.test(page) && /Firmware before this one cannot open converted projects/.test(page),
     "gate: the warning is section 10's words");
}
console.log(fails ? `BACKUP WEB TESTS FAILED (${fails})` : "backup web tests passed");
process.exit(fails ? 1 : 0);
