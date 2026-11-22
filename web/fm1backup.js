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
  if (!file || file.format !== "felucca-backup" || file.version !== 1 || !ids)
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
  return { ...file, objects };
}
// (0.14.2) the arrangement objects' inventory, in the session a plain LIST began; [] when the firmware has none (it answers LIST 1 with rc 1)
async function arrManifest(request) {
  const a = await request([BACKUP_CMD.LIST, [1]], { timeout: 6000, retries: 0 });
  return a[0] === 1 && a[1] === 1 ? [] : bkManifest(a, true);
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
  readBackup(file); return file;
}
export async function restoreBackup(request, file, onProgress = () => {}) {
  const archive = readBackup(file); // Validate every byte before the first destructive request.
  const total = archive.objects.reduce((n, o) => n + o.size, 0); let done = 0;
  const ask = async (r, o = {}) => request(r, { timeout: 4000, retries: 0, ...o });
  const supported = bkManifest(await ask([BACKUP_CMD.LIST, []]));
  if (archive.objects.some((o) => o.id >= 64) && supported.length === BACKUP_IDS_SHARED.length) supported.push(...await arrManifest(ask));
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
  for (const o of [...archive.objects.slice(2).filter((o) => o.id !== ARR_LIVE), archive.objects[1], archive.objects[0], ...live]) {
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

// (Round 32) replies matched to their request (BUILD.md 7.54). A reply can come after its request timed out: the unit was slow, the page sent the
// request again (retries) or went on to the next one. Before this round the page took any reply of the command it waited for, so the late reply of
// the FIRST send of a BACKUP GET answered the NEXT GET (another offset: "Unexpected backup reply" at 50 %). Now every send that got no reply of its own
// is owed for a while (3 x its timeout, at least 3 s), and a reply that comes is matched:
//   1. it echoes the arguments of an owed send of another request (echo(cmd, args, a), the arguments the firmware puts back: editor.c,
//      editor_backup.c): that send's late reply, discarded and logged; for a command whose reply echoes nothing, any reply of the command while one of
//      another identity (other arguments) is owed is taken as that one's (the request waiting then times out and is sent again: never a wrong answer);
//   2. it is the waiting request's when the command is the same and echo() agrees; an owed send of the identical request (cmd and arguments: its reply
//      says the same) is taken as the one that answered;
//   3. anything else is not this page's (a push frame, another tool's reply): ignored, as before.
// The same gate is in web/editor.html's Link (the editor page and the web app's editor_proto.js), with the editor's replyMatches as echo().
export function bkEcho(cmd, args, a, owed = false) {
  switch (cmd) {
    case 66: return a[0] === args[0] && [1, 2, 3, 4, 5].every((i) => a[i + 1] === args[i]);   // BACKUP GET: id, rc, offset (5), size: id and offset
    case 67: return a[0] === args[0] && a[1] === args[1];                                       // BACKUP PUT: op, id, rc
    case 11: case 12: case 13: case 14: return a[0] === args[0];                               // the old sample slot commands (Melodee 1.x): slot first
    case 0: return a[0] === args[0];                                                            // the extension (FX_INFO ..): n, then the reply
    case 117: case 122: return !owed || a[0] !== 0 || (a[1] | a[2] << 7 | a[3] << 14) === (args[0] | args[1] << 7 | args[2] << 14) + bkPackedLen(args.length - 3);
      // PACK / MOD WRITE: rc, done (3 x 7 bits), no echo; but a write the unit took (rc 0) says its own end in done (oc_pack_write, oc_mod_write since 0.8.0),
      // so an rc 0 reply with another end is not an owed write's (owed: echo asked about an owed send). A refusal could be any write's.
    default: return true;                                                                       // INFO, LIST, 110, the pack and module commands: no echo
  }
}
const bkPackedLen = (L) => Math.floor(L / 8) * 7 + (L % 8 ? L % 8 - 1 : 0);   // the bytes bkPack made L wire bytes of
const bkSame = (x, y) => x.cmd === y.cmd && x.args.length === y.args.length && x.args.every((v, i) => v === y.args[i]);
export class ReplyGate {
  constructor(echo, log = (m) => console.warn(m)) { this.echo = echo; this.log = log; this.owed = []; this.stale = 0; }
  // c ({cmd, args}) was sent (again)
  sent(c, timeout) { c.sends = (c.sends || 0) + 1; c.until = Date.now() + Math.max(3000, 3 * timeout); }
  // c is done: answered (true) or timed out; its sends without a reply are owed
  finish(c, answered) {
    for (let n = (c.sends || 0) - (answered && !c.borrowed ? 1 : 0); n > 0; n--) this.owed.push({ cmd: c.cmd, args: c.args, until: c.until });
  }
  // a reply (cmd, data a) while c waits (or null) -> true: it is c's; false: discarded
  take(cmd, a, c) {
    const now = Date.now(); this.owed = this.owed.filter((o) => o.until > now);
    const i = this.owed.findIndex((o) => o.cmd === cmd && !(c && bkSame(o, c)) && this.echo(cmd, o.args, a, true));
    if (i >= 0) {
      this.owed.splice(i, 1); this.stale++;
      this.log(`late reply of command ${cmd} discarded (its request had timed out): [${a.slice(0, 9).join(" ")}]`); return false;
    }
    if (!c || c.cmd !== cmd || !this.echo(cmd, c.args, a)) return false;                           // not a reply of this page's (a push, another tool's)
    const j = this.owed.findIndex((o) => bkSame(o, c));
    if (j >= 0) { this.owed.splice(j, 1); c.borrowed = true; }
    return true;
  }
}

// Installer connection: no editor page or watcher owns this input simultaneously.
// (Round 32) replies through a ReplyGate; opt.retries sends a request again after its timeout (captureBackup asks GET with one retry).
export class BackupConnection {
  constructor(input, output, { log } = {}) {
    this.input = input; this.output = output; this.pending = null; this.gate = new ReplyGate(bkEcho, log);
    input.onmidimessage = (e) => {
      const d = e.data;
      if (d.length < 6 || d[0] !== 240 || d[1] !== 125 || d[2] !== 70 || d[3] !== 76 || d.at(-1) !== 247) return;
      const a = Array.from(d.slice(5, -1));
      if (!this.gate.take(d[4], a, this.pending)) return;
      const p = this.pending; this.pending = null; clearTimeout(p.timer); this.gate.finish(p, true); p.resolve(a);
    };
  }
  async request([cmd, args], opt = {}) {
    if (this.pending || this.closed || this.input.state === "disconnected" || this.output.state === "disconnected") throw new Error("Backup connection closed or busy");
    const timeout = opt.timeout || 1200; let tries = opt.retries || 0;
    return new Promise((resolve, reject) => {
      const c = { cmd, args, resolve, reject };
      const fire = () => {
        try { this.output.send([240, 125, 70, 76, cmd, ...args, 247]); }
        catch (e) { this.pending = null; this.gate.finish(c, false); reject(e); return; }
        this.gate.sent(c, timeout);
        c.timer = setTimeout(() => {
          if (this.pending !== c) return;
          if (tries-- > 0) { fire(); return; }
          this.pending = null; this.gate.finish(c, false); reject(new Error("Backup timed out. Keep the FM-1 connected and stopped."));
        }, timeout);
      };
      this.pending = c; fire();
    });
  }
  close() {
    this.closed = true; this.input.onmidimessage = null;
    if (this.pending) { const p = this.pending; this.pending = null; clearTimeout(p.timer); p.reject(new Error("Backup connection closed")); }
  }
}
