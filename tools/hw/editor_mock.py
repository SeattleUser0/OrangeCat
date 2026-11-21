# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 32) A fake FM-1 editor for the host checks of tools/hw/unit_objects.py and q4_install.py (tools/hw/qual_selfcheck.py): it answers the
commands those tools send as the firmware does at v0.15.0 ("0.15.0") and at 4573e79d ("nightly"), from the C (editor.c, editor_backup.c,
editor_library.c, editor_arrange.c, oc_project.c). What differs between the two here, as it does on the unit:
  - INFO's version and P_COUNT (the Nightly's parameter layout has two more track parameters: a preset record is mapped by count, as upreset.c's
    up_params does; the mock maps by label);
  - TRACK_STEP's reply: the Nightly sends the v8 feel's three bytes after the chance;
  The project store is the same on both (4573e79d builds with OC_PJ4 0, core.h:152: oc_pj4*.c are not compiled, no conversion): BACKUP_LIST 1's
  64..67 are the slots' OCJ3 records and 68..83 the pool's chunks on both; BACKUP_PUT of 2..5 with size 0 empties the slot (proj_write_slot len
  0, ocj_write), of 64..67 with size 0 deletes the OCS_PROJECT record (an OLD FORMAT one too: `old_records`).
It is a model, not the firmware: what it cannot know (the record's bytes) is a JSON image of its state, with the CRC-32 of those bytes.
"""
import copy
import json
import zlib

import fm1hw
from fm1hw import v14, r14

NSTEP, NTRK, NPAT = 64, 4, 8
REST = 2
EMPTY = (0, 0, 0, 0, 0, REST, 0, 0, 0, 0, 0, 100)       # n, notes4, time, flags, vel, hit, acc, bits, chance
LISTS = 2048 + 64 * NTRK + 3 * NTRK + 4 + 8 * NTRK * 2 + 4 + 2 * NTRK


def n5(v):
    return [(v >> (7 * j)) & 127 for j in range(5)]


def r5(b):
    return sum(b[j] << (7 * j) for j in range(5)) & 0xFFFFFFFF


def crc(b):
    return zlib.crc32(bytes(b)) & 0xFFFFFFFF


def pack7(b):
    out = []
    for o in range(0, len(b), 7):
        c = b[o:o + 7]
        out.append(sum((x >> 7) << i for i, x in enumerate(c)))
        out += [x & 127 for x in c]
    return out


def unpack7(a):
    out, i = [], 0
    while i < len(a):
        m = a[i]; i += 1
        c = a[i:i + 7]; i += len(c)
        out += [x | ((m >> k) & 1) << 7 for k, x in enumerate(c)]
    return out


def layout(fw):
    """the track parameters' descriptors: (label, min, max, def); the Nightly has NEWA, NEWB before the engine's eight"""
    tp = [("LEVEL", 0, 127, 100), ("PAN", -64, 63, 0), ("MUTE", 0, 1, 0), ("LEN", 1, 64, 64), ("DIV", 0, 7, 2), ("SWG", 0, 100, 0),
          ("GATE", 0, 255, 200), ("", 0, 0, 5)] + [(f"P{j}", 0, 127, j * 3 % 128) for j in range(8, 30)]
    eng = [(f"E{j}", 0, 127, 0 if j == 0 else 64) for j in range(8)]
    return tp + ([("NEWA", 0, 127, 11), ("NEWB", 0, 127, 22)] if fw == "nightly" else []) + eng


def bank_obj(k):
    b = k // 18
    return [6, 7, 17, 18][b] if b < 4 else [21, 22, 19, 20][b - 4] if b < 8 else 9 + b - 8 if b < 16 else 23 + b - 16 if b < 21 else 28


class EditorMock(fm1hw.Unit):
    def __init__(self, fw="0.15.0"):
        self.name, self._info, self.sent = "mock", None, []
        self.busy = 0
        self.closed = 0
        self.lib = {3: ("MACRO", 8, "USER PAD", {"LEVEL": 90})}          # the unit's own preset (slot 3)
        self.projects = {}
        self.old_records = {}                                         # slot -> a record of a format the slot list does not read (OLD FORMAT)
        self.pool = {}                                                # phrase id -> (steps, len)
        self.music = self.blank()
        self.music["tracks"][1]["banks"][3][5] = (1, 50, 0, 0, 0, 0, 0, 100, 0, 0, 0, 100)   # the unit's music: a note on track 2 bank 4
        self.music["tracks"][2]["motion"] = 3
        self.music["lists"][10] = 7
        self.put = None
        self.ar = None
        self.lib_old = {}                                             # slot -> name: a preset of a format this firmware does not read (OLD)
        self.settings = bytes((i * 7 + 3) % 256 for i in range(604))   # (object 1 as the unit's: 604 bytes, three chunks of 256, 256, 92)
        self.faults = []                                              # BACKUP_PUT faults, each once: {"kind", "op", "oid", "off"} (req below)
        self.late = None                                              # a reply that arrives after the next request was sent
        self.queued = []                                              # replies that arrived before the next send (fm1hw.Unit.req drains them)
        self.lossy = set()                                            # object ids whose restore the mock drops (a restore that does not verify)
        self.set_fw(fw)

    # ---------------------------------------------------------------- the model
    def set_fw(self, fw):
        """the firmware the unit runs (an install): the records stay as they are (OC_PJ4 0 on both)"""
        self.fw = fw
        self._info = None
        self.tp = layout(fw)
        for t in self.music["tracks"]:
            t["p"] = {lab: t["p"].get(lab, d) for lab, lo, hi, d in self.tp}

    def blank(self):
        tp = layout("0.15.0")
        return {"tracks": [{"engine": 8, "preset": 2, "level": 80, "mute": 1 if k == 3 else 0, "motion": 0, "active": 0, "p": {l: d for l, lo, hi, d in tp},
                            "banks": [[EMPTY] * NSTEP for _ in range(NPAT)], "lens": [64] * NPAT} for k in range(NTRK)],
                "sel": 0, "bpm": 98, "lists": [0] * LISTS, "pid": [[0] * NPAT for _ in range(NTRK)]}

    def info_reply(self):
        names = ["-"] * 32
        names[8], names[9], names[14] = "MACRO", "DRUM-X", "SHAPES"
        ver = "ORANGE CAT v0.15.0-d6134" if self.fw == "0.15.0" else "ORANGE CAT v0.16.0-4573e7"
        return list(ver.encode()) + [0, 32, len(self.tp), 3, NSTEP, len(self.tp) - 8] + sum((list(n.encode()) + [0] for n in names), [])

    def used(self, s):
        return tuple(s) != EMPTY

    def keep(self):
        """a save's keep: each bank that holds steps is a pool phrase (its own kept, a new one the first free id)"""
        for k, t in enumerate(self.music["tracks"]):
            for b in range(NPAT):
                content = (tuple(map(tuple, t["banks"][b])), t["lens"][b])
                if not any(self.used(s) for s in t["banks"][b]):
                    self.music["pid"][k][b] = 0
                    continue
                pid = self.music["pid"][k][b]
                if pid and self.pool.get(pid - 1) == content:
                    continue
                free = next(i for i in range(64) if i not in self.pool)
                self.pool[free] = content
                self.music["pid"][k][b] = free + 1

    def flags(self):
        f = [0] * 64
        for i in self.pool:
            f[i] |= 1
        for row in self.music["pid"]:
            for v in row:
                if v: f[v - 1] |= 2
        if True:                                                      # (ocj_refs, oc_project.c: both firmwares)
            for rec in self.projects.values():
                for row in rec["pid"]:
                    for v in row:
                        if v: f[v - 1] |= 4
        return f

    def objects(self):
        """the backup objects' bytes: {id: bytes}"""
        j = lambda o: json.dumps(o, sort_keys=True).encode()
        o = {0: j(self.music), 1: self.settings}
        for s in range(4):
            rec = self.projects.get(s)
            o[2 + s] = j({k: rec[k] for k in ("tracks", "bpm")}) if rec else b""
        for oid in list(range(6, 29)):
            o[oid] = j(sorted([(k, v[0], v[1], v[2], sorted(v[3].items())) for k, v in self.lib.items() if bank_obj(k) == oid]
                              + [(k, "OLD", 0, nm, []) for k, nm in self.lib_old.items() if bank_obj(k) == oid])) if oid != 8 else b""
        for s in range(4):
            o[64 + s] = j(self.projects[s]) if s in self.projects else j(self.old_records[s]) if s in self.old_records else b""
        for c in range(16):
            ph = {i: self.pool[i] for i in range(4 * c, 4 * c + 4) if i in self.pool}
            o[68 + c] = j(sorted(ph.items())) if ph else b""
        o[84] = bytes(self.music["lists"]) + bytes(sum(self.music["pid"], []))
        return o

    def track_reply(self):
        trs = self.music["tracks"]
        out = [self.music["sel"], NTRK]
        for t in trs:
            out += [t["engine"], t["preset"] & 127, *v14(t["level"]), t["mute"], 0]
        return out + [t["preset"] >> 7 for t in trs]

    def step_reply(self, k, i):
        s = self.music["tracks"][k]["banks"][self.music["tracks"][k]["active"]][i]
        return [k, i, *s] + ([0, 32, 0] if self.fw == "nightly" else [])

    def lib_get(self, k):
        if k not in self.lib:
            return [1, k & 127, k >> 7, 0, 0, 0, 0] + v14(0) * len(self.tp) + [0] * 32 + [0]
        name, eng, nm, vals = self.lib[k]
        out = [1, k & 127, k >> 7, 0, 1, eng, *nm.encode(), 0]
        for lab, lo, hi, d in self.tp:
            v = 0 if not lab else max(lo, min(hi, vals.get(lab, d)))          # (up_values: mapped, a reserved one 0, clamped)
            out += v14(v)
        return out + [0] * 32 + [0]

    def apply(self, oid, b):
        """a restored object taken into the model (the inverse of objects())"""
        m = self.music
        if oid == 0:
            self.music = json.loads(b)
        elif oid == 1:
            self.settings = b
        elif 2 <= oid <= 5:
            rec = self.projects.setdefault(oid - 2, copy.deepcopy(self.blank()))
            rec.update(json.loads(b))
        elif 6 <= oid <= 28:
            for k in [k for k in self.lib if bank_obj(k) == oid]: del self.lib[k]
            for k in [k for k in self.lib_old if bank_obj(k) == oid]: del self.lib_old[k]
            for k, name, eng, nm, items in json.loads(b):
                if name == "OLD": self.lib_old[k] = nm
                else: self.lib[k] = (name, eng, nm, dict(items))
        elif 64 <= oid <= 67:
            self.projects[oid - 64] = json.loads(b)
        elif 68 <= oid <= 83:
            c = oid - 68
            for i in range(4 * c, 4 * c + 4): self.pool.pop(i, None)
            for i, content in json.loads(b): self.pool[int(i)] = content
        elif oid == 84:
            m["lists"] = list(b[:LISTS])
            m["pid"] = [list(b[LISTS + NPAT * k:LISTS + NPAT * (k + 1)]) for k in range(NTRK)]

    # ---------------------------------------------------------------- the wire
    def close(self):
        self.closed += 1

    def req(self, cmd, args=(), timeout=3.0):
        """the link as the unit's showed it (Round 32 restore on the unit): a frame dropped (usb.c: one frame kept, a new F0 while it is pending
        drops the new one), a reply lost, a frame taken twice whose second reply comes late (it then answers the NEXT request; the real reply of
        that one arrives after and is drained by fm1hw.Unit.req before the following send)"""
        a = list(args)
        self.queued = []                                              # (fm1hw.Unit.req drains what arrived before it sends)
        f = None
        if cmd == 67:
            off = r5(a[2:7]) if a[0] == 1 and len(a) > 6 else None
            f = next((x for x in self.faults if x["op"] == a[0] and x["oid"] == a[1] and x.get("off") in (None, off)), None)
            if f:
                self.faults.remove(f)
        if f and f["kind"] == "drop_request":
            self.sent.append((cmd, a))
            raise TimeoutError(f"mock: the frame {a[:2]} was dropped")
        r = self._req(cmd, a)
        if f and f["kind"] == "lose_reply":
            raise TimeoutError(f"mock: the reply to {a[:2]} was lost")
        if f and f["kind"] == "dup_frame":
            self.late = self._req(cmd, a)                             # (taken twice: the second answer, rc 1 for a chunk, comes late)
        if self.late is not None and not (f and f["kind"] == "dup_frame"):
            out, self.late = self.late, None
            self.queued = [r]
            return out
        return r

    def _req(self, cmd, a):
        self.sent.append((cmd, a))
        m, trs = self.music, self.music["tracks"]
        T = trs[m["sel"]]
        if cmd == 1:
            return self.info_reply()
        if cmd == 5:
            if a[0] == 0:
                lab, lo, hi, d = self.tp[a[1]]
            else:
                lab, lo, hi, d = [("BPM", 40, 300, 120), ("CLK", 0, 2, 0), ("TYPE", 0, 3, 0)][a[1]]
            return [a[0], a[1], 0, *v14(lo), *v14(hi), *v14(d), *lab.encode(), 0, 0]
        if cmd == 2 and a[0] == 1:
            return [1, a[1], *v14(m["bpm"] if a[1] == 0 else 0)]
        if cmd == 3 and a[0] == 1:
            if a[1] == 0: m["bpm"] = max(40, min(300, r14(a[2:4])))
            return [1, a[1], *v14(m["bpm"])]
        if cmd == 27:
            if a: m["sel"] = a[0]
            return self.track_reply()
        if cmd == 8:
            T["engine"], T["preset"] = a[0], a[1]
            return [a[0], a[1], 0]
        if cmd == 28:
            t = trs[a[0]]
            if len(a) == 4: t["level"], t["mute"] = r14(a[1:3]), a[3]
            return [a[0], *v14(t["level"]), t["mute"]]
        if cmd == 31:
            t = trs[a[0]]
            lab, lo, hi, d = self.tp[a[1]]
            if len(a) == 4:
                v = max(lo, min(hi, r14(a[2:4])))
                if lab == "LEN": t["lens"][t["active"]] = v
                else: t["p"][lab] = v
            return [a[0], a[1], *v14(t["lens"][t["active"]] if lab == "LEN" else t["p"].get(lab, d))]
        if cmd == 30:
            t = trs[a[0]]
            if len(a) == 14:
                flags_keep = t["banks"][t["active"]][a[1]][6] & ~3            # (RATCH, FLAM stay: the mock has none)
                t["banks"][t["active"]][a[1]] = (a[2], *a[3:7], a[7], flags_keep | (a[8] & 3), a[9], a[10], 0, 0, a[13])
            return self.step_reply(a[0], a[1])
        if cmd == 64:
            t = trs[a[0]]
            if a[1:] == [2]: t["motion"] = 0
            return [a[0], 0, 1, t["motion"], 64]
        if cmd == 73:
            t = trs[a[0]]
            rc = 0
            if a[1] == 1: t["active"] = a[2]
            if a[1] == 2:
                if a[3] == t["active"] and self.busy: rc = 1
                else:
                    t["banks"][a[3]] = list(t["banks"][a[2]]); t["lens"][a[3]] = t["lens"][a[2]]
            return [a[0], a[1], rc, t["active"], 127, NPAT]
        if cmd == 9:
            op, s = a
            if op == 1:
                if self.busy: self.busy = 0                                   # (ed_flash_stop)
                self.keep()
                self.projects[s] = copy.deepcopy(m)
                self.old_records.pop(s, None)
            elif op == 0 and s in self.projects:
                self.music = copy.deepcopy(self.projects[s])
            return [op, s, 1 if s in self.projects else 0]
        if cmd == 65:
            if self.busy: return [1, 3, 0]
            if a != [1] or getattr(self, "snap", None) is None:
                self.snap = self.objects()                            # (ed_bk_capture: a plain LIST begins the session GET reads)
            o = self.snap
            ids = list(range(64, 85)) if a == [1] else list(range(29))
            out = [1, 0, len(ids)]
            for i in ids:
                out += [i, *n5(len(o[i])), *n5(crc(o[i]) if o[i] else 0)]
            return out
        if cmd == 67:
            op, oid = a[0], a[1]
            if self.busy: return [op, oid, 3]
            if op == 0:
                size = r5(a[2:7])
                if not (oid <= 28 or 64 <= oid <= 84) or (oid == 8 and size) or (oid == 84 and size != LISTS + NTRK * NPAT): return [op, oid, 1]
                self.put = {"id": oid, "size": size, "crc": r5(a[7:12]), "buf": []}
                return [op, oid, 0]
            if op == 3:
                self.put = None
                return [op, oid, 0]
            if op == 1:
                if not self.put or self.put["id"] != oid or r5(a[2:7]) != len(self.put["buf"]): return [op, oid, 1]
                self.put["buf"] += unpack7(a[7:])
                return [op, oid, 0]
            if op == 2:
                if not self.put or self.put["id"] != oid: return [op, oid, 5]
                pt, self.put = self.put, None
                if oid in getattr(self, "fail_commit", ()): return [op, oid, 4]          # (a flash write the unit refuses: a restore stopped part way)
                if len(pt["buf"]) != pt["size"] or (crc(pt["buf"]) if pt["buf"] else 0) != pt["crc"]: return [op, oid, 2]
                if pt["size"]:
                    if oid not in self.lossy: self.apply(oid, bytes(pt["buf"]))
                    return [op, oid, 0]
                if oid in (0, 1, 84): return [op, oid, 2]
                if 6 <= oid <= 28 or 68 <= oid <= 83:
                    self.apply(oid, b"[]")
                    return [op, oid, 0]
                if 2 <= oid <= 5:
                    self.projects.pop(oid - 2, None)                          # (proj_write_slot len 0: ocj_write empty it)
                else:
                    s = oid - 64
                    self.projects.pop(s, None)
                    self.old_records.pop(s, None)
                named = {v - 1 for rec in list(self.projects.values()) + [m] for row in rec["pid"] for v in row if v}
                self.pool = {i: c for i, c in self.pool.items() if i in named}
                return [op, oid, 0]
            return [op, oid, 1]
        if cmd == 66:
            oid, off, n = a[0], r5(a[1:6]), a[6] | a[7] << 7
            b = (getattr(self, "snap", None) or self.objects()).get(oid)
            if b is None or off + n > len(b): return [oid, 1, *n5(off), 0, 0]
            return [oid, 0, *n5(off), n & 127, n >> 7, *pack7(list(b[off:off + n]))]
        if cmd == 80:
            op, k = a[0], a[1] | a[2] << 7
            if op == 0:
                cnt = min(a[3], 384 - k)
                out = [0, a[1], a[2], 0, cnt, 384 & 127, 384 >> 7]
                for j in range(k, k + cnt):
                    e, old = self.lib.get(j), self.lib_old.get(j)
                    out += [1 if e else 0, e[1] if e else 0, 1 if old else 0, *(e[2].encode() if e else old.encode() if old else b""), 0]
                return out
            if op == 1:
                return self.lib_get(k)
            if op == 2:
                eng = a[3]
                z = a.index(0, 4)
                name = bytes(a[4:z]).decode()
                vals = [r14(a[z + 1 + 2 * j:z + 3 + 2 * j]) for j in range(len(self.tp))]
                if len(a) != z + 1 + 2 * len(self.tp) + 32 + 1 or any(v < -64 or v > 127 for v in vals):
                    return [2, a[1], a[2], 1]
                self.lib[k] = ("MACRO", eng, name, {lab: v for (lab, lo, hi, d), v in zip(self.tp, vals) if lab})
                return [2, a[1], a[2], 0]
            if op == 5:
                self.lib.pop(k, None)
                self.lib_old.pop(k, None)
                return [5, a[1], a[2], 0]
        if cmd == 102:
            op = a[0]
            if op == 0:
                f = self.flags()
                out = [0, 0, 1, LISTS & 127, LISTS >> 7, 64, 64, 16, 64, 8, NTRK, NTRK, self.busy, *n5(crc(m["lists"]))]
                for i in range(64):
                    c = self.pool.get(i)
                    out += [f[i], c[1] if c else 0, *n5(crc(json.dumps(c).encode()) if c else 0)]
                return out
            if op == 1:
                off, n = a[1] | a[2] << 7, a[3] | a[4] << 7
                return [1, 0, a[1], a[2], a[3], a[4], *pack7(m["lists"][off:off + n])]
            if op == 2:
                if self.busy: return [2, 3]
                self.ar = {"crc": r5(a[3:8]), "buf": []}
                return [2, 0]
            if op == 3:
                if not self.ar: return [3, 5]
                if (a[1] | a[2] << 7) != len(self.ar["buf"]): return [3, 1]
                self.ar["buf"] += unpack7(a[3:])
                return [3, 0]
            if op == 4:
                ar, self.ar = self.ar, None
                if not ar: return [4, 5, 127]
                if len(ar["buf"]) != LISTS or crc(ar["buf"]) != ar["crc"]: return [4, 2, 127]
                self.keep()
                b = ar["buf"]
                keep = set(range(2048 + 64 * NTRK, 2048 + 64 * NTRK + 3 * NTRK + 1)) | {2048 + 64 * NTRK + 3 * NTRK + 2, 2048 + 64 * NTRK + 3 * NTRK + 3, 2384 + 1, 2384 + 2, 2384 + 3}
                m["lists"] = [m["lists"][i] if i in keep else b[i] for i in range(LISTS)]
                return [4, 0, 127]
        raise TimeoutError(f"no reply to editor command {cmd} {a}")
