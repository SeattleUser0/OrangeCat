// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Kerem Kilic (Ellic Studio)
// Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Local, complete musical archives. Requests name whitelisted objects, never flash addresses.
export const BACKUP_IDS = [0, 1, 2, 3, 4, 5, 6, 7, 8, 32, 33, 34];   // 8: the FM6 patch bank (firmware with FM6)
const BACKUP_IDS_V1 = BACKUP_IDS.filter((id) => id !== 8);              // firmware before FM6, and its archives
const BACKUP_IDS_BANKS = [0, 1, 2, 3, 4, 5, 6, 7, 8];
const BACKUP_IDS_CZ = Array.from({length:17},(_,i)=>i);
const BACKUP_IDS_OWN = Array.from({length:21},(_,i)=>i);
const BACKUP_IDS_NATIVE = Array.from({length:23},(_,i)=>i);
const BACKUP_IDS_SHARED = Array.from({length:29},(_,i)=>i);
const BACKUP_IDS_P5 = Array.from({length:28},(_,i)=>i);
// (Orange Cat 0.14.2) the arrangement objects (firmware: editor_backup.c, LIST with the argument 1): 64..67 the slots' project records,
// 68..83 the pool's sixteen chunks, 84 the music loaded's lists and phrases. Without them a restore brought every project back with a new
// project's chains, song and scenes. An archive with them has 50 objects (ids 0..28, then these)
export const ARR_IDS = Array.from({length:21},(_,i)=>64+i);
const BACKUP_IDS_ARR = [...BACKUP_IDS_SHARED, ...ARR_IDS];
const ARR_LIVE = 84, ARR_REC_MAX = 12 * 3808;
const isSample = (id) => id >= 32 && id <= 34;
const maxSize = id => id >= 64 ? (id === ARR_LIVE ? 16384 : ARR_REC_MAX) : id >= 32 ? 81920 : (id === 0 || (id >= 2 && id <= 5)) ? 27904 : 3840;
const idsOf = (n) => (n === BACKUP_IDS.length ? BACKUP_IDS : n === BACKUP_IDS_V1.length ? BACKUP_IDS_V1 : n === 9 ? BACKUP_IDS_BANKS : n === 17 ? BACKUP_IDS_CZ : n === 21 ? BACKUP_IDS_OWN : n === 23 ? BACKUP_IDS_NATIVE : n === 28 ? BACKUP_IDS_P5 : n===29?BACKUP_IDS_SHARED : n === BACKUP_IDS_ARR.length ? BACKUP_IDS_ARR : null);
export const BACKUP_CMD = { LIST: 65, GET: 66, PUT: 67 };
// (Release 3, Part 6) the unit's project format from an INFO reply's payload: the tagged block 0x57 that closes it (version 1: pcount, e0, format), else 2 (a firmware
// before Release 3 sends no block)
export const infoFormat = (a) => (a.length >= 7 && a.at(-7) === 0x57 && a.at(-6) === 1 ? a.at(-1) : 2);
// (Release 3, Part 6, PROJECT_FORMAT_R3.md section 10) what the installer owes before it sends a firmware of project format pkgFormat to a unit that answered INFO with
// `unit` ({answered, format, supported}): kind "none" (the unit is on that format already, or the package is not a conversion), "warn" (the one-way warning, no backup
// possible: the unit did not answer or cannot export one), "gate" (the warning, then a complete backup saved from the unit before Install goes on). downgrade: the
// package is older than the unit's projects. No downgrade is refused.
export function r3Gate(pkgFormat, unit) {
  const fmt = pkgFormat || 2;
  if (unit.answered && unit.format >= 3) return fmt >= 3 ? { kind: "none", downgrade: false } : { kind: unit.supported ? "gate" : "warn", downgrade: true };
  if (fmt < 3) return { kind: "none", downgrade: false };
  return { kind: unit.answered && unit.supported ? "gate" : "warn", downgrade: false };
}
// (Release 3, Part 6) the records by type and id: extension bytes 7 LISTR, 8 GETR, 9 PUTR (firmware: editor_backup.c, web/EDITOR_PROTOCOL.md "Records by type
// and id"). A Release 3 unit's projects are records with ids that have bit 15 set, not the fixed ids 64..83; the archive keeps them in `records`
// (type, id, size, crc, data) beside `objects`, as version 2 (a page that does not know them refuses the file rather than restoring a lossy copy)
export const REC = { LIST: 7, GET: 8, PUT: 9, PROJECT: 1, POOL: 2 };
export const recKey = (type, id) => [type, id & 127, (id >> 7) & 127, id >>> 14];
const recSlotOf = (type, id) => (type === REC.POOL ? (id & 0x8000 ? (id >> 9) & 7 : -1) : (id & 0x8000 ? id & 3 : -1));
const REC_MAX = 12 * 3808;
const BACKUP_CHUNK = 256;
export const bkU32 = (n) => Array.from({ length: 5 }, (_, i) => (n >>> (i * 7)) & (i === 4 ? 15 : 127));
export const bkR32 = (a, off = 0) => {
  if (a.length < off + 5 || a[off + 4] > 15) throw new Error("Invalid archive number");
  return (a[off] | a[off + 1] << 7 | a[off + 2] << 14 | a[off + 3] << 21 | a[off + 4] << 28) >>> 0;
};
export function bkPack(bytes) {
  const a = [];
  for (let off = 0; off < bytes.length; off += 7) {
    const chunk = bytes.subarray(off, off + 7);
    a.push(chunk.reduce((m, b, i) => m | (b >>> 7) << i, 0), ...chunk.map((b) => b & 127));
  }
  return a;
}
export function bkUnpack(a, size) {
  const out = new Uint8Array(size); let i = 0, j = 0;
  while (j < size) {
    const n = Math.min(7, size - j), mask = a[i++];
    if (mask == null || mask >>> n) throw new Error("Invalid archive bytes");
    for (let k = 0; k < n; k++) {
      if (a[i] == null || a[i] > 127) throw new Error("Short archive chunk");
      out[j++] = a[i++] | (mask >>> k & 1) << 7;
    }
  }
  if (i !== a.length) throw new Error("Trailing archive bytes");
  return out;
}
export function bkCrc(bytes) {
  let c = 0xffffffff;
  for (const b of bytes) {
    c ^= b;
    for (let k = 0; k < 8; k++) c = c >>> 1 ^ (c & 1 ? 0xedb88320 : 0);
  }
  return (~c) >>> 0;
}
function bkCheck(rc) {
  if (rc) throw new Error(({ 1: "Invalid archive object", 2: "Archive data failed validation", 3: "Stop playback first", 4: "Flash write failed", 5: "Start a fresh backup" })[rc] || `Archive error ${rc}`);
}
export function bkManifest(a, arr = false) {
  if (a[0] !== 1) throw new Error("Unsupported archive protocol");
  bkCheck(a[1]);
  const ids = arr ? (a[2] === ARR_IDS.length ? ARR_IDS : null) : idsOf(a[2]);
  if (!ids || a.length !== 3 + a[2] * 11) throw new Error("Incomplete archive manifest");
  return ids.map((id, i) => {
    const p = 3 + i * 11;
    if (a[p] !== id) throw new Error("Unexpected archive object");
    const size = bkR32(a, p + 1), crc = bkR32(a, p + 6);
    if (size > maxSize(id) || (!size && crc)) throw new Error("Archive object too large");
    return { id, size, crc };
  });
}
const bkBase64 = (bytes) => { let s = ""; for (const b of bytes) s += String.fromCharCode(b); return btoa(s); };
export function readBackup(file) {
  if (typeof file === "string") file = JSON.parse(file);
  const ids = file && Array.isArray(file.objects) ? idsOf(file.objects.length) : null;
  if (!file || file.format !== "felucca-backup" || (file.version !== 1 && file.version !== 2) || !ids || (file.version === 1 && file.records?.length))
    throw new Error("Not a complete Orange Cat backup");
  const objects = file.objects.map((o, i) => {
    if (!o || o.id !== ids[i] || !Number.isInteger(o.size) || o.size < 0 || o.size > maxSize(o.id) ||
        !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xffffffff || typeof o.data !== "string" ||
        o.data.length !== 4 * Math.ceil(o.size / 3) || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(o.data))
      throw new Error("Invalid backup object");
    const bytes = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
    if (bytes.length !== o.size || bkCrc(bytes) !== o.crc) throw new Error("Backup checksum mismatch");
    const shared=o.id>=6&&o.id<=28&&o.id!==8&&o.size===3716&&new DataView(bytes.buffer).getUint32(0,true)===0x32425055;
    if(shared){const v=new DataView(bytes.buffer);if(v.getUint16(4,true)!==206||v.getUint16(6,true)!==18)throw Error("Invalid shared preset bank");}
    if(o.id>=23&&o.id<=27&&o.size&&!shared){
      if(o.size!==3600)throw Error("Invalid Prophet bank size");const v=new DataView(bytes.buffer),used=v.getUint32(4,true),fav=v.getUint32(8,true),mask=(1<<(o.id===27?24:26))-1;
      if(v.getUint32(0,true)!==0x31553550||(used&~mask)||(fav&~(mask|(o.id===23?0x80000000:0))))throw Error("Invalid Prophet bank");
      for(let k=0;k<26;k++)if(used>>k&1){const b=bytes.subarray(12+k*138,12+(k+1)*138);if(![128,133].includes(b[133])||![49,50].includes(b[134])||![2,3].includes(b[135])||b[135]===2&&(b[136]>9||b[137]>39))throw Error("Invalid Prophet program");}
    }
    if(o.id>=19 && o.id<=22 && o.size===2060){
      const v=new DataView(bytes.buffer),used=v.getUint32(8,true);
      if(v.getUint32(0,true)!==0x314d464e || v.getUint16(4,true)!==1 || v.getUint16(6,true)!==16 || used>>>16)throw new Error("Invalid native FM6 preset object");
      for(let k=0;k<16;k++)if(used>>k&1)if(bytes.subarray(12+k*128,12+(k+1)*128).some(x=>x>127))throw new Error("Invalid native DX7 voice");
    }
    if(o.id>=21 && o.id<=22 && o.size && o.size!==2060 && !shared)throw new Error("Invalid native FM6 preset size");
    if(o.id>=9 && o.id<=16 && o.size && !shared){
      if(o.size!==2332)throw new Error("Invalid CZ bank size");const v=new DataView(bytes.buffer),used=v.getUint32(8,true);
      if(v.getUint32(0,true)!==0x42435a43 || ![1,2].includes(v.getUint16(4,true)) || v.getUint16(6,true)!==16 || used>>>16 || !bytes[12] || Array.from(bytes.slice(12,28)).some(x=>x>126 || x>0&&x<32))throw new Error("Invalid CZ bank");
      for(let i=0;i<16;i++)if(used>>i&1){const b=bytes.subarray(28+i*144,28+(i+1)*144);if((b[0]&240)||(b[0]>>2)>2||b[1]>1||(b[2]&3)||b[3]>47||[0,57].some(j=>(b[16+j]&15)>9||(b[16+j]>>4)>14||b[18+j]>9||[20,37,54].some(k=>b[k+j]&8)))throw new Error("Invalid CZ bank tone");}
    }
    if (o.id >= 64 && o.id < 68 && o.size && ![0x334a434f, 0x324a434f].includes(new DataView(bytes.buffer).getUint32(0, true)))
      throw new Error("Invalid project record");
    if (o.id >= 68 && o.id < ARR_LIVE && o.size) {
      const v = new DataView(bytes.buffer);
      if (o.size < 8 || 8 + [0, 1, 2, 3].reduce((n, j) => n + v.getUint16(2 * j, true), 0) !== o.size) throw new Error("Invalid phrase chunk");
    }
    if (isSample(o.id) && o.size) {
      if (o.size < 512) throw new Error("Short sample backup");
      const view = new DataView(bytes.buffer), length = view.getUint32(16, true), count = bytes[6];
      if (view.getUint32(0, true) !== 0x504d5346 || view.getUint16(4, true) !== 1 || !count || count > 16 ||
          length !== o.size - 512 || bkCrc(bytes.subarray(512)) !== view.getUint32(20, true)) throw new Error("Invalid sample backup");
      for (let z = 0; z < count; z++) {
        const off = 32 + z * 28, start = view.getUint32(off,true), n = view.getUint32(off+4,true),
          ls = view.getUint32(off+8,true), le = view.getUint32(off+12,true), rate = view.getUint32(off+16,true);
        if (!n || start > length || Math.ceil(n/2) > length - start || n > 163840 || ls > le || le >= n ||
            !rate || rate > 262144 || bytes[off+24] > 88 || bytes[off+25] > bytes[off+26]) throw new Error("Invalid sample zone");
      }
    }
    return { ...o, bytes };
  });
  if (!objects[0].size || !objects[1].size) throw new Error("Backup is missing the current music or settings");
  const records = readRecords(file.records);
  return { ...file, objects, records };
}
// (Release 3) the archive's records, validated byte for byte before the first write; [] when there are none
function readRecords(list) {
  if (list === undefined) return [];
  if (!Array.isArray(list) || list.length > 64) throw new Error("Invalid backup records");
  const seen = new Set();
  return list.map((o) => {
    if (!o || ![REC.PROJECT, REC.POOL].includes(o.type) || !Number.isInteger(o.id) || o.id < 0 || o.id > 0xffff || seen.has(o.type * 65536 + o.id) ||
        !Number.isInteger(o.size) || o.size < 1 || o.size > REC_MAX || !Number.isInteger(o.crc) || o.crc < 0 || o.crc > 0xffffffff ||
        typeof o.data !== "string" || o.data.length !== 4 * Math.ceil(o.size / 3) || !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(o.data))
      throw new Error("Invalid backup record");
    seen.add(o.type * 65536 + o.id);
    const bytes = Uint8Array.from(atob(o.data), (c) => c.charCodeAt(0));
    if (bytes.length !== o.size || bkCrc(bytes) !== o.crc) throw new Error("Backup record checksum mismatch");
    if (o.type === REC.PROJECT && o.id & 0x8000 && (o.id & 0x7fff) > 3) throw new Error("Invalid project record");
    if (o.type === REC.POOL && o.id & 0x8000 && (((o.id >> 9) & 7) > 3 || !(o.id & 255))) throw new Error("Invalid phrase chunk record");
    if (o.type === REC.PROJECT && o.id & 0x8000 && new DataView(bytes.buffer).getUint32(0, true) !== 0x344a434f) throw new Error("Invalid project record");
    return { ...o, bytes };
  });
}
// (Release 3) the unit's record inventory in pages of 16, [] when it does not answer (a firmware before Release 3 never replies: a short timeout)
async function recManifest(request) {
  const out = [];
  for (let start = 0; ; ) {
    let a;
    try { a = await request([0, [REC.LIST, start & 127, start >> 7]], { timeout: out.length ? 3000 : 1200, retries: 0 }); }
    catch (e) { if (out.length) throw e; return []; }
    if (a[0] !== REC.LIST) throw new Error("Unexpected record reply");
    bkCheck(a[1]);
    const n = a[4], more = a[5];
    if (a.length !== 6 + n * 14 || n > 16) throw new Error("Incomplete record inventory");
    for (let i = 0; i < n; i++) {
      const p = 6 + i * 14, id = a[p + 1] | a[p + 2] << 7 | a[p + 3] << 14, size = bkR32(a, p + 4), crc = bkR32(a, p + 9);
      if (![REC.PROJECT, REC.POOL].includes(a[p]) || size > REC_MAX || (!size && crc)) throw new Error("Invalid record inventory");
      out.push({ type: a[p], id, size, crc });
    }
    if (!more || !n) return out;
    start += n; if (out.length > 64) throw new Error("Record inventory too long");
  }
}
// (0.14.2) the arrangement objects' inventory, in the session a plain LIST began; [] when the firmware has none (it answers LIST 1 with rc 1)
async function arrManifest(request) {
  const a = await request([BACKUP_CMD.LIST, [1]], { timeout: 6000, retries: 0 });
  return a[0] === 1 && a[1] === 1 ? [] : bkManifest(a, true);
}
async function captureRecords(request, onProgress) {
  const list = await recManifest(request), records = [];
  for (const o of list) {
    const bytes = new Uint8Array(o.size);
    for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
      const size = Math.min(BACKUP_CHUNK, o.size - off);
      const a = await request([0, [REC.GET, ...recKey(o.type, o.id), ...bkU32(off), size & 127, size >>> 7]], { timeout: 1000, retries: 1 });
      if (a[0] !== REC.GET) throw new Error("Unexpected record reply");
      bkCheck(a[5]);
      if (a[1] !== o.type || (a[2] | a[3] << 7 | a[4] << 14) !== o.id || bkR32(a, 6) !== off || (a[11] | a[12] << 7) !== size) throw new Error("Unexpected record reply");
      bytes.set(bkUnpack(a.slice(13), size), off);
    }
    if (bkCrc(bytes) !== o.crc) throw new Error("The device changed during backup. Try again while stopped.");
    records.push({ ...o, data: bkBase64(bytes) }); onProgress(0, 0);
  }
  // the inventory again: a save between the pages, or after the last record, renames chunk generations and would leave a record naming a chunk this archive does not hold
  const again = await recManifest(request);
  if (again.length !== list.length || again.some((o, i) => o.type !== list[i].type || o.id !== list[i].id || o.size !== list[i].size || o.crc !== list[i].crc))
    throw new Error("The device changed during backup. Try again while stopped.");
  return records;
}
export async function captureBackup(request, firmware, onProgress = () => {}) {
  const manifest = bkManifest(await request([BACKUP_CMD.LIST, []], { timeout: 3000, retries: 0 }));
  if (manifest.length === BACKUP_IDS_SHARED.length) manifest.push(...await arrManifest(request));
  const objects = [], total = manifest.reduce((n, o) => n + o.size, 0); let done = 0;
  for (const o of manifest) {
    const bytes = new Uint8Array(o.size);
    for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
      const size = Math.min(BACKUP_CHUNK, o.size - off);
      const a = await request([BACKUP_CMD.GET, [o.id, ...bkU32(off), size & 127, size >>> 7]], { timeout: 1000, retries: 1 });
      bkCheck(a[1]);
      if (a[0] !== o.id || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== size) throw new Error("Unexpected backup reply");
      bytes.set(bkUnpack(a.slice(9), size), off);
      done += size; onProgress(done, total);
    }
    if (bkCrc(bytes) !== o.crc) throw new Error("The device changed during backup. Try again while stopped.");
    objects.push({ ...o, data: bkBase64(bytes) });
  }
  const file = { format: "felucca-backup", version: 1, firmware, created: new Date().toISOString(), objects };
  const records = await captureRecords(request, onProgress);
  if (records.length) { file.records = records; file.version = 2; }
  readBackup(file); return file;
}
// (Release 3) does the unit answer the records commands: a LISTR with no reply is a firmware without them
async function recSupported(ask) { try { return (await ask([0, [REC.LIST, 0, 0]], { timeout: 1200 }))[0] === REC.LIST; } catch { return false; } }
// (Release 3) the records written: the pool chunks first, then the project records, then finish. A chunk of a slot whose record names that very chunk under
// the same generation (rc 6) cannot be written beside it: that slot's Release 3 record and chunks are cleared first (op 5) and its chunks and record written
// whole. That is the one window in which a power cut leaves the slot empty; the page says so before it starts (restoreBackup is only called after the user's confirm)
async function restoreRecords(ask, records, progress) {
  const put = async (op, type, id, extra = []) => {
    const a = await ask([0, [REC.PUT, op, ...recKey(type, id), ...extra]], { timeout: op === 4 ? 15000 : 4000 });
    if (a[0] !== REC.PUT || a[1] !== op) throw new Error("Unexpected record reply");
    return a[2];
  };
  const order = [...records.filter((r) => r.type === REC.POOL), ...records.filter((r) => r.type === REC.PROJECT)];
  const cleared = new Set();
  for (const o of order) {
    let rc = await put(0, o.type, o.id, [...bkU32(o.size), ...bkU32(o.crc)]);
    const slot = recSlotOf(o.type, o.id);
    if (rc === 6 && slot >= 0 && !cleared.has(slot)) {
      bkCheck(await put(5, REC.PROJECT, 0x8000 | slot));
      cleared.add(slot);
      rc = await put(0, o.type, o.id, [...bkU32(o.size), ...bkU32(o.crc)]);
    }
    bkCheck(rc);
    try {
      for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
        const chunk = o.bytes.subarray(off, off + BACKUP_CHUNK);
        bkCheck(await put(1, o.type, o.id, [...bkU32(off), ...bkPack(chunk)]));
        progress(chunk.length);
      }
      bkCheck(await put(2, o.type, o.id));
    } catch (e) { await put(3, o.type, o.id).catch(() => {}); throw e; }
  }
  bkCheck(await put(4, REC.PROJECT, 0));
}
export async function restoreBackup(request, file, onProgress = () => {}) {
  const archive = readBackup(file); // Validate every byte before the first destructive request.
  const total = archive.objects.reduce((n, o) => n + o.size, 0) + (archive.records || []).reduce((n, o) => n + o.size, 0); let done = 0;
  const ask = async (r, o = {}) => request(r, { timeout: 4000, retries: 0, ...o });
  const supported = bkManifest(await ask([BACKUP_CMD.LIST, []]));
  if (archive.objects.some((o) => o.id >= 64) && supported.length === BACKUP_IDS_SHARED.length) supported.push(...await arrManifest(ask));
  const records = archive.records || [];
  const keyed = records.length ? await recSupported(ask) : false;
  if (records.length && !keyed) throw new Error("This firmware cannot restore the project records of this backup");
  const recSlots = new Set(records.filter((r) => r.type === REC.PROJECT && r.id & 0x8000).map((r) => r.id & 3));
  const liveSize = supported.find(o => o.id === 0)?.size || 0;
  for (const o of archive.objects) if ((o.id === 0 || o.id >= 2 && o.id <= 5) && o.size > liveSize && !(liveSize >= 19008 && [19084,19092,20224].includes(o.size)))
    throw new Error("This backup needs firmware with larger recording storage");
  for (const o of archive.objects) if (!supported.some(x => x.id === o.id) && o.size) throw new Error("This firmware does not support a nonempty object in this backup");
  const put = async (args) => { const a = await ask([BACKUP_CMD.PUT, args]); bkCheck(a[2]); return a; };
  // The firmware accepts or refuses each object's size before the first write (2.0 reads only the formats of
  // 1.0 and later: an older archive restores on 1.2 first), so a refused archive writes nothing.
  for (const o of archive.objects) if (!isSample(o.id) && o.size && supported.some(x => x.id === o.id)) {
    const a = await ask([BACKUP_CMD.PUT, [0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]]);
    await ask([BACKUP_CMD.PUT, [3, o.id]]);
    if (a[2] === 1) throw new Error("This backup holds an older format. Restore it with Melodee 1.2 first.");
    bkCheck(a[2]);
  }
  if(archive.objects.length===28 && supported.some(o=>o.id===28)){
    await put([0,28,...bkU32(0),...bkU32(0)]);await put([2,28]);
  }
  // Restore live music last (and its arrangement, 84, after it: the music restored takes a new project's lists). The slots' records
  // and the pool (64..83) come after the slots (2..5), which write the pool too. Other objects commit individually; a disconnect can
  // leave a partial restore.
  const live = archive.objects.filter((o) => o.id === ARR_LIVE);
  // (Release 3) with the records, the slots' own FBKKs (2..5) and the format before's arrangement objects (64..83) are not written for a slot the records
  // hold: the records are the slot whole, and the FBKK would only write a lossy copy first
  const skip = (o) => records.length && ((o.id >= 2 && o.id <= 5 && recSlots.has(o.id - 2)) || (o.id >= 64 && o.id < ARR_LIVE));
  const first = archive.objects.slice(2).filter((o) => o.id !== ARR_LIVE && !skip(o));
  const rest = [archive.objects[1], archive.objects[0], ...live];
  for (const o of [...first, { records: true }, ...rest]) {
    if (o.records) { if (records.length) await restoreRecords(ask, records, (n) => { done += n; onProgress(done, total); }); continue; }
    if (!supported.some(x => x.id === o.id)) continue;
    if (isSample(o.id)) {
      const slot = o.id - 32;
      const check = (a) => { if (a[0] !== slot) throw new Error("Unexpected sample reply"); bkCheck(a.at(-1)); };
      if (!o.size) check(await ask([14, [slot]]));
      else {
        if (o.size < 512) throw new Error("Short sample backup");
        check(await ask([11, [slot]]));
        for (let off = 512; off < o.size; off += BACKUP_CHUNK) {
          const chunk = o.bytes.subarray(off, off + BACKUP_CHUNK);
          check(await ask([12, [slot, off & 127, off >>> 7 & 127, off >>> 14 & 127, ...bkPack(chunk)]]));
          done += chunk.length; onProgress(done, total);
        }
        check(await ask([13, [slot, ...bkPack(o.bytes.subarray(0, 480))]]));
        done += 512; onProgress(done, total);
      }
    } else {
      await put([0, o.id, ...bkU32(o.size), ...bkU32(o.crc)]);
      try {
        for (let off = 0; off < o.size; off += BACKUP_CHUNK) {
          const chunk = o.bytes.subarray(off, off + BACKUP_CHUNK);
          await put([1, o.id, ...bkU32(off), ...bkPack(chunk)]);
          done += chunk.length; onProgress(done, total);
        }
        await put([2, o.id]);
      } catch (e) { await put([3, o.id]).catch(() => {}); throw e; }
    }
  }
  return archive;
}

// Installer connection: no editor page or watcher owns this input simultaneously.
export class BackupConnection {
  constructor(input, output) {
    this.input = input; this.output = output; this.pending = null;
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d.at(-1) !== 247) return;
      if (this.pending?.cmd === d[4] && (d[4] !== 0 || this.pending.sub === d[5])) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.resolve(Array.from(d.slice(5, -1))); }
    };
  }
  async request([cmd, args], opt = {}) {
    if (this.pending || this.closed || this.input.state === "disconnected" || this.output.state === "disconnected") throw new Error("Backup connection closed or busy");
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => { this.pending = null; reject(new Error("Backup timed out. Keep the FM-1 connected and stopped.")); }, opt.timeout || 1200);
      this.pending = { cmd, sub: cmd === 0 ? args[0] : undefined, timer, resolve, reject };
      try { this.output.send([240, 125, 70, 76, cmd, ...args, 247]); }
      catch (e) { clearTimeout(timer); this.pending = null; reject(e); }
    });
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.reject(new Error("Backup connection closed")); }
  }
}
