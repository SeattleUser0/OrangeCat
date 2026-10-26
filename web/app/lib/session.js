// SPDX-License-Identifier: GPL-3.0-only
// Orange Cat web app: one connection to the unit and everything the tabs ask of it.
//
// The requests, replies, the Link (one request at a time, pushes apart) and the module, pack and library code are the old editor's,
// unchanged (editor_proto.js); this file is the order they are used in, as the editor's connect(), load(), modProbe(), the library and
// the backup do it, gathered in one object the page and the tests share. Nothing here touches the page: the tabs (app.js) render
// what a Session holds and call its methods.
//
// Revisions:
//   0.14.2: first version (Session D).

import * as E from "./editor_proto.js";
import { BACKUP_CMD, bkManifest, bkU32, bkR32, bkUnpack, bkPack, bkCrc, captureBackup, readBackup, restoreBackup } from "#fm1backup";
import { OcLink, runSet } from "#fm1oc";
import { readArrangement, writeArrangement, parseInfo as parseArrInfo, arrReq } from "./arrange.js";
import { decodeFbkk, decodeLists } from "./codec.js";

const { CMD, req, parse } = E;
export const isUnitPort = (p) => /orange cat|melodee|felucca/i.test(p.name || "") && p.state !== "disconnected";

export class Session {
  constructor(input, output, hooks = {}) {
    this.input = input; this.output = output;
    this.hooks = { say: () => {}, push: () => {}, lost: () => {}, ...hooks };
    this.link = new E.Link((d) => output.send(d), {
      timeout: 300,
      onUnknown: () => {},
      onTimeout: () => this.hooks.say("noreply"),
      onPush: (f) => this.onPush(f),
      onGap: () => { this.rewatch = true; },
    });
    input.onmidimessage = (e) => this.link.receive(e.data);
    this.info = null; this.pdesc = []; this.gdesc = []; this.names = []; this.titles = [];
    this.tracks = null; this.sel = 0; this.dump = null; this.watch = 0; this.v4 = false;
    this.slotUsed = [null, null, null, null];
    this.pack = null; this.mod = null; this.engines = null; this.modPages = {};
    this.prefs = null; this.arrange = null; this.closed = false; this.timer = null;
  }
  /* the unit's ports in a MIDIAccess (the real one, or the mock's) */
  static find(access) {
    const input = [...access.inputs.values()].find(isUnitPort), output = [...access.outputs.values()].find(isUnitPort);
    return input && output ? { input, output } : null;
  }
  rq(r, opt) { if (this.closed) return Promise.reject(new Error("closed")); return this.link.request(r, opt); }
  close() {
    this.closed = true;
    clearInterval(this.timer);
    this.link.close();
    if (this.input.onmidimessage) this.input.onmidimessage = null;
  }
  onPush(f) {
    /* the unit changed something (a knob, a load, a step): the page re-reads what it shows (CHANGED, RELOAD, STEP_CHANGED, TRACK_CHANGED) */
    if (f.cmd === CMD.CHANGED && this.dump) { const x = parse[CMD.CHANGED](f.a); if (x.scope === 0) this.dump.p[x.id] = x.value; else this.dump.g[x.id] = x.value; }
    if (f.cmd === CMD.RELOAD) this.stale = true;
    this.hooks.push(f);
  }

  /* --------------------------------------------------------------- load --- */
  /* the connect sequence (the editor's load): INFO, every descriptor, the presets' names, the slots, the track, the sound, the live
   * pushes, the preferences, then the Orange Cat parts (pack, modules, Engines list, the arrangement) */
  async load(progress = () => {}) {
    const say = this.hooks.say;
    this.info = parse[CMD.INFO](await this.rq(req.info(), { timeout: 500, retries: 3 }));
    const total = this.info.pcount + this.info.gcount;
    for (let i = 0; i < this.info.pcount; i++) { progress((i + 1) / total * 0.6); this.pdesc[i] = await this.descOrNull(0, i); }
    for (let i = 0; i < this.info.gcount; i++) { progress((this.info.pcount + i + 1) / total * 0.6); this.gdesc[i] = await this.descOrNull(1, i); }
    for (let e = 0; e < this.info.nengines; e++) {
      progress(0.6 + 0.2 * e / this.info.nengines);
      let r = parse[CMD.NAMES](await this.rq(req.names(e)));
      const names = r.names.slice();
      while (r.total > names.length && r.names.length) { r = parse[CMD.NAMES](await this.rq(req.names(e, names.length))); names.push(...r.names); }
      this.names[e] = names; this.titles[e] = r.titles;
    }
    for (let k = 0; k < 4; k++) {
      try { this.slotUsed[k] = parse[CMD.PROJECT](await this.rq(req.project(2, k))).used; } catch (e) { if (e.message === "closed") throw e; }
    }
    this.keys = E.paramKeys(this.pdesc, this.info.pe0, this.info.pcount);
    await this.readTracks();
    this.dump = await this.readDump();
    this.watch = await E.startWatch((r, o) => this.rq(r, o));
    this.v4 = this.watch === 3;
    if (this.watch) this.timer = setInterval(() => { if (this.link.idle && !this.closed) this.rq(req.ping(), { quiet: true }).catch(() => {}); }, 1000);
    say("readingBank");
    await this.readPreferences();
    progress(0.85);
    await this.probePack();
    await this.probeModules();
    await this.probeArrange();
    progress(1);
    say("ready");
  }
  async descOrNull(s, i) { try { return parse[CMD.DESC](await this.rq(req.desc(s, i))); } catch (e) { if (e.message === "closed") throw e; return null; } }
  async readTracks() {
    if (!this.info.ntrk) { this.tracks = null; this.sel = 0; return null; }
    this.tracks = parse[CMD.TRACK](await this.rq(req.track()));
    this.sel = this.tracks.sel;
    return this.tracks;
  }
  async readDump() { return parse[CMD.DUMP](await this.rq(req.dump()), this.info); }
  async readPreferences() {
    try { this.prefs = await E.readDevicePreferences((r, o) => this.rq(r, { ...o, quiet: true }), this.info, this.names, null); }
    catch (e) { if (e.message === "closed") throw e; this.prefs = null; }
    return this.prefs;
  }

  /* -------------------------------------------------------------- Unit --- */
  async probePack() {
    this.pack = null;
    try { this.pack = parse[CMD.PACK_INFO](await this.rq(req.packInfo(), { timeout: 150, retries: 0, quiet: true })); } catch (e) { if (e.message === "closed") throw e; }
    return this.pack;
  }
  /* the editor's modProbe: MOD_INFO (for an engine, or none), then the Engines list, then a module engine's pages */
  async probeModules(engine) {
    this.mod = null;
    try { this.mod = parse[CMD.MOD_INFO](await this.rq(req.modInfo(engine), { timeout: 150, retries: 0, quiet: true })); } catch (e) { if (e.message === "closed") throw e; }
    this.engines = null;
    if (this.mod) {
      try { this.engines = parse[CMD.ENGINES](await this.rq(req.engines(), { timeout: 150, retries: 0, quiet: true })); } catch (e) { if (e.message === "closed") throw e; }
    }
    this.modPages = {};
    if (this.engines) for (const x of this.engines.engines) {
      if (x.src !== 2 || !E.MODULE_SLOTS.includes(x.num)) continue;
      const mp = await E.fetchModPages((r) => this.rq(r, { timeout: 150, retries: 0, quiet: true }), x.num).catch((e) => { if (e.message === "closed") throw e; return null; });
      if (mp) this.modPages[x.num] = mp;
    }
    return this.mod;
  }
  engineRows() { return this.engines ? E.enginesRows(this.engines, this.info?.engines || []) : []; }
  /* Install or Replace on a row with an .ocm file's bytes (the editor's mod-file handler): {how: "installed" | "swapped" | "cancelled"} */
  async installModule(row, bytes, { ask = () => true, progress = () => {} } = {}) {
    const ocm = E.ocmParse(bytes);
    if (!ocm.ok) throw new Error(ocm.key);
    if (row && !row.replace && ocm.engine !== row.num) throw new Error("modWrongEngine");
    const how = await E.replaceOcm((r, o) => this.rq(r, o), (engine) => this.probeModules(engine).then(() => this.mod), row, ocm, ask, progress);
    await this.probeModules(); await this.readTracks();
    return { how, engine: ocm.engine };
  }
  async removeEngine(num) {
    const r = parse[CMD.MOD_REMOVE](await this.rq(req.modRemove(num === null ? 127 : num), { timeout: 3000 }));
    await this.probeModules();
    if (r.rc) throw new Error(E.modCode(r.rc));
  }
  async useOnTrack(num, track) {
    const out = await E.assignEngine((q) => this.rq(q), num, track, this.info.ntrk, this.sel);
    await this.probeModules(); await this.readTracks(); this.dump = await this.readDump();
    return out;
  }
  async installPack(bytes, progress = () => {}) {
    const info = this.pack, code = (rc) => (rc === 2 ? "packStop" : rc === 3 ? "packOldFw" : rc === 4 ? "packRestart" : "packFail");
    const bad = E.packProblem(bytes, info);
    if (bad) throw new Error(bad);
    let r = parse[CMD.PACK_BEGIN](await this.rq(req.packBegin(bytes.length), { timeout: E.OC_BEGIN_MS, retries: 0 }));
    if (r.rc) throw new Error(code(r.rc));
    for (let off = 512; off < bytes.length; off += 256) {
      r = parse[CMD.PACK_WRITE](await this.rq(req.packWrite(off, bytes.subarray(off, Math.min(off + 256, bytes.length))), { timeout: 1000, retries: 0 }));
      if (r.rc) throw new Error(code(r.rc));
      progress(off / bytes.length);
    }
    r = parse[CMD.PACK_END](await this.rq(req.packEnd(bytes.subarray(0, 512)), { timeout: 3000, retries: 0 }));
    await this.probePack();
    if (r.rc) throw new Error(code(r.rc));
  }
  /* the set: the pack and the modules of a release (the files fetched by the caller), installed by the installer page's own code
   * (fm1oc.js runSet, stages firmware off: firmware stays the installer page's). The session's Link stands aside while runSet's
   * OcLink owns the port, and takes it back after. -> runSet's summary */
  async installSet(files, { stages = { pack: true, module: true }, confirmInPlace = () => true, say = () => {}, progress = () => {}, log = () => {} } = {}) {
    if (!this.link.idle) throw new Error("ocBusy");
    const handler = this.input.onmidimessage;
    clearInterval(this.timer); this.timer = null;
    try {
      return await runSet({
        stages: { firmware: false, pack: !!stages.pack, module: !!stages.module }, files, say, progress, log, confirmInPlace,
        openUnit: async () => new OcLink(this.input, this.output),
      });
    } finally {
      this.input.onmidimessage = handler;
      if (this.watch && !this.closed) this.timer = setInterval(() => { if (this.link.idle && !this.closed) this.rq(req.ping(), { quiet: true }).catch(() => {}); }, 1000);
      await this.probePack(); await this.probeModules();
    }
  }

  /* ------------------------------------------------------------ Sounds --- */
  async selectTrack(k) {
    if (this.info.ntrk) { await this.rq(req.track(k)); this.sel = k; }
    await this.readTracks();
    this.dump = await this.readDump();
  }
  /* a track parameter: TRACK_PARAM for any track (v4), SET for the selected one. The value after clamping */
  async setParam(k, id, v) {
    if (k === this.sel || !this.v4) {
      const r = parse[CMD.SET](await this.rq(req.set(0, id, v), { key: "set:" + id }));
      if (this.dump && k === this.sel) this.dump.p[id] = r.value;
      return r.value;
    }
    return parse[CMD.TRACK_PARAM](await this.rq(req.trackParam(k, id, v), { key: `tp:${k}:${id}` })).value;
  }
  async setGlobal(id, v) { const r = parse[CMD.SET](await this.rq(req.set(1, id, v), { key: "g:" + id })); if (this.dump) this.dump.g[id] = r.value; return r.value; }
  /* the preset browser's rows (factory presets in the unit's engine order, then the library's used slots; starred) */
  presetRows() { return this.prefs ? E.devicePresetRows(this.info, this.names, this.prefs) : this.factoryRows(); }
  factoryRows() {
    return E.engineOrder(this.info.engines).filter((e) => this.names[e]).flatMap((engine) => this.names[engine].map((name, preset) => ({ engine, preset, name, user: false, favorite: false })));
  }
  async loadPreset(row) {
    if (row.user) {
      const rc = await E.bank.load((r, o) => this.rq(r, o), row.preset, this.info, this.sel);
      if (rc) throw new Error("flashFail");
    } else {
      const pr = parse[CMD.PRESET](await this.rq(req.preset(row.engine, row.preset)));
      if (pr.refused) throw new Error(pr.refused === 1 ? "engineHeavy" : "engineNoRoom");
    }
    this.dump = await this.readDump(); await this.readTracks();
    await this.rereadEngineDesc();
  }
  async rereadEngineDesc() { for (let i = 0; i < 8; i++) this.pdesc[this.info.pe0 + i] = await this.descOrNull(0, this.info.pe0 + i); }
  async star(row) {
    const r = parse[CMD.FAV_SET](await this.rq(req.favSet(row.engine, row.preset, !row.favorite), E.FLASH_OPT));
    await this.readPreferences();
    return r.rc;
  }
  /* the library (the unit's shared user library, or the older UP slots): list, store, load, erase, rename */
  async library() { return E.bank.list((r, o) => this.rq(r, o), this.info); }
  async libStore(slot, name) { const rc = await E.bank.store((r, o) => this.rq(r, o), slot, name, this.info, this.sel); await this.readPreferences(); return rc; }
  async libLoad(slot) { const rc = await E.bank.load((r, o) => this.rq(r, o), slot, this.info, this.sel); this.dump = await this.readDump(); await this.rereadEngineDesc(); return rc; }
  async libErase(slot) { const rc = await E.bank.erase((r, o) => this.rq(r, o), slot, this.info); await this.readPreferences(); return rc; }
  async libRename(slot, name) {
    if (!this.info.shared) throw new Error("renameOld");      /* (firmware with the older user slots: store again under the new name) */
    const clean = String(name).replace(/[^\x20-\x7E]/g, " ").trim().slice(0, 12);
    await E.sharedBank.action((r, o) => this.rq(r, o), 6, slot, ...E.strEnc(clean).slice(0, -1));
    await this.readPreferences();
    return 0;
  }

  /* ------------------------------------------------------------- Steps --- */
  async readSteps(track = this.sel) {
    const steps = [];
    for (let i = 0; i < this.info.nstep; i++)
      steps.push(this.info.ntrk ? parse[CMD.TRACK_STEP](await this.rq(req.trackStep(track, i))) : parse[CMD.STEP_GET](await this.rq(req.stepGet(i))));
    return steps;
  }
  async writeStep(track, i, s) {
    return this.info.ntrk ? parse[CMD.TRACK_STEP](await this.rq(req.trackStep(track, i, s))) : parse[CMD.STEP_SET](await this.rq(req.stepSet(i, s)));
  }

  /* ----------------------------------------------------------- Project --- */
  /* one backup object, read as captureBackup reads them (LIST, then GET in pieces, its CRC checked) */
  async readObject(id, onProgress = () => {}) {
    const manifest = bkManifest(await this.rq([BACKUP_CMD.LIST, []], { timeout: 3000, retries: 0 }));
    const o = manifest.find((x) => x.id === id);
    if (!o) throw new Error("This firmware does not list object " + id);
    const bytes = new Uint8Array(o.size);
    for (let off = 0; off < o.size; off += 256) {
      const size = Math.min(256, o.size - off);
      const a = await this.rq([BACKUP_CMD.GET, [o.id, ...bkU32(off), size & 127, size >>> 7]], { timeout: 1000, retries: 1 });
      if (a[1]) throw new Error(a[1] === 3 ? "Stop playback first" : a[1] === 5 ? "Start again (the unit changed)" : "Archive error " + a[1]);
      if (a[0] !== o.id || bkR32(a, 2) !== off || (a[7] | a[8] << 7) !== size) throw new Error("Unexpected backup reply");
      bytes.set(bkUnpack(a.slice(9), size), off);
      onProgress((off + size) / o.size);
    }
    if (bkCrc(bytes) !== o.crc) throw new Error("The unit changed while it was read. Try again while stopped.");
    return bytes;
  }
  /* the music loaded now (object 0) or a saved slot (2..5) as a project: {fbkk (decoded), bytes} */
  async readProject(id = 0, onProgress) {
    const bytes = await this.readObject(id, onProgress);
    if (!bytes.length) return null;
    return { bytes, fbkk: decodeFbkk(bytes) };
  }
  /* one object written as restoreBackup writes one (begin, data in order, commit; abort on a failure) */
  async writeObject(id, bytes, onProgress = () => {}) {
    const ask = (args) => this.rq([BACKUP_CMD.PUT, args], { timeout: 4000, retries: 0 });
    const check = (a) => { if (a[2]) throw new Error(({ 1: "Invalid archive object", 2: "Archive data failed validation", 3: "Stop playback first", 4: "Flash write failed", 5: "Start a fresh backup" })[a[2]] || `Archive error ${a[2]}`); };
    check(await ask([0, id, ...bkU32(bytes.length), ...bkU32(bkCrc(bytes))]));
    try {
      for (let off = 0; off < bytes.length; off += 256) {
        check(await ask([1, id, ...bkU32(off), ...bkPack(bytes.subarray(off, off + 256))]));
        onProgress(Math.min(1, (off + 256) / bytes.length));
      }
      check(await ask([2, id]));
    } catch (e) { await ask([3, id]).catch(() => {}); throw e; }
  }
  async projectSave(slot) {
    const r = parse[CMD.PROJECT](await this.rq(req.project(1, slot), { timeout: 3000, retries: 0 }));
    this.slotUsed[slot] = r.used;
    return r;
  }
  async projectLoad(slot) {
    const r = parse[CMD.PROJECT](await this.rq(req.project(0, slot), { timeout: 3000, retries: 0 }));
    await this.readTracks(); this.dump = await this.readDump(); await this.probeArrange();
    return r;
  }
  /* the complete backup, with the arrangement of the music loaded beside it (the backup objects do not carry it: EDITOR_PROTOCOL.md) */
  async backup(onProgress = () => {}) {
    const file = await captureBackup((r, o) => this.rq(r, o), this.info.version, (d, t) => onProgress(d / Math.max(1, t)));
    if (this.arrange) {
      const a = await readArrangement((r, o) => this.rq(r, o));
      file.arrangement = { format: "orangecat-arrangement", version: 1, lists: btoa(String.fromCharCode(...a.bytes)),
        pool: a.info.pool.map((p) => p.crc) };
    }
    return file;
  }
  /* a complete backup file written back; its arrangement too when every phrase it names is, by CRC, the same on this unit's pool.
   * -> {arrangement: "restored" | "different pool" | "none" | "no command"} */
  async restore(file, onProgress = () => {}) {
    const archive = readBackup(file);
    await restoreBackup((r, o) => this.rq(r, o), archive, (d, t) => onProgress(d / Math.max(1, t)));
    let arrangement = "none";
    const a = typeof file === "string" ? JSON.parse(file).arrangement : file.arrangement;
    if (a && a.format === "orangecat-arrangement") {
      if (!(await this.probeArrange())) arrangement = "no command";
      else {
        const lists = Uint8Array.from(atob(a.lists), (c) => c.charCodeAt(0)), L = decodeLists(lists);
        const named = new Set(L.chains.flat().map((x) => x.phrase).filter(Boolean));
        const same = [...named].every((id) => a.pool[id - 1] === this.arrange.pool[id - 1].crc);
        if (!same) arrangement = "different pool";
        else { await writeArrangement((r, o) => this.rq(r, o), lists); arrangement = "restored"; }
      }
    }
    await this.readTracks(); this.dump = await this.readDump(); await this.probeArrange();
    return { arrangement };
  }

  /* ----------------------------------------------------------- Arrange --- */
  async probeArrange() {
    this.arrange = null;
    try { this.arrange = parseArrInfo(await this.rq(arrReq.info(), { timeout: 300, retries: 1, quiet: true })); } catch (e) { if (e.message === "closed") throw e; }
    return this.arrange;
  }
  async readLists() { const a = await readArrangement((r, o) => this.rq(r, o)); this.arrange = a.info; return { lists: decodeLists(a.bytes), bytes: a.bytes, info: a.info }; }
  async writeLists(bytes, progress) { const r = await writeArrangement((q, o) => this.rq(q, o), bytes, progress); await this.probeArrange(); await this.readTracks(); return r; }
}
