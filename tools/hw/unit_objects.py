#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 32, BUILD.md 7.37 Q4) The saved objects of the install cases written and erased over the editor, unattended: no step waits for the
panel. Only commands both firmwares of Q4 have (0.15.0 Standard and the Release 3 Nightly, 4573e79d) are sent; each is read back.

  unit_objects.py OUT LABEL seed --confirm SEED [--backup FILE]       the seed set below written, read back and verified; OUT/seed-LABEL.json
  unit_objects.py OUT LABEL verify --manifest FILE                     READ ONLY (nothing loaded): the seed's objects byte for byte (BACKUP_GET),
                                                                       the phrase (LEN, CRC), the presets (Get); the transport must be stopped
  unit_objects.py OUT LABEL restore --backup FILE --confirm RESTORE    the backup written back as web/editor.html's Restore backup does, then
                                                                       BACKUP_LIST (LIST 0 and 1) compared with it (exit 1 when it differs)
  unit_objects.py OUT LABEL wipe-projects --confirm WIPE [--backup F]  every project slot A..D emptied, read back empty
  unit_objects.py OUT LABEL wipe-presets --confirm WIPE [--backup F]   every user preset erased (80, op 5), the library read back empty

Each writing case refuses while a host suite runs (unit_guard.suites_running), while the transport runs or a track is armed, and unless a full
backup of the unit was just taken and verified: oc_install.mjs backup (web/fm1backup.js captureBackup, read back by readBackup), or --backup FILE;
either way every object of the file must equal the unit's BACKUP_LIST now (id, size, CRC-32), so the file is the unit as it is. Restore with
web/editor.html, Complete backup > Restore backup, on the firmware it was taken on.

THE SEED (fixed; every value below is the tool's, the rest the firmware's defaults):
  - the music loaded, then saved to project slot A (PROJECT 9, op 1): on every track MACRO (by INFO's engine names) preset 0 (PRESET 8), each
    track parameter at its descriptor's default for MACRO (DESC 5, TRACK_PARAM 31), LEN 16 on bank 1, motion cleared (MOTION 64, op 2), every
    step a rest (TRACK_STEP 30), bank 1 copied over banks 2..8 (PATTERN 73, op 2), level 100, not muted (TRACK_MIX 28); BPM 120 (SET 3);
    track 1 bank 1 then plays SEED_STEPS (C4 Eb4 G4 Bb4 on steps 1, 5, 9, 13, velocity 100, chance 100).
  - the pool phrase: the one the save keeps of track 1's bank 1 (ARRANGE 102 INFO: the only phrase that holds steps and is a bank of the music
    loaded; its LEN 16): pool chunk object 68 + id / 4 on both firmwares (4573e79d builds with OC_PJ4 0: the same store as 0.15.0).
  - a scene: the lists written whole (ARRANGE 102 BEGIN/DATA/COMMIT): chain 1 = [the phrase], song row 1 = [chain 1, -, -, -], scene 1 =
    track 1 at chain 1 step 1 (the other tracks left as they are), scene_used = scene 1, every other chain, row and scene empty, no groove,
    LAUNCH LOOP; then slot A saved again (the lists go with the project).
  - two user presets in shared library slots 70 and 71 (U71, U72; library bank 3 = backup object 18), library command 80 op 2 Put, kind 0:
    "Q4 SEED A" = MACRO with every parameter at its default as Put carries it (-64..127: a default outside is written clamped, and the
    track parameters are written with their real defaults, 14 bits); "Q4 SEED B" = the same with LEVEL 20 lower. Read back with op 1 Get,
    byte for byte. Slots 70 and 71 are written over whatever they held (named in the manifest, preset_slots_overwritten): the backup taken
    first holds it, and q4_install.py's restore at the end of case a puts it back.
  Read back: slot A loaded again (PROJECT op 0) and every step of track 1 bank 1, the tracks, the lists' written fields and the phrase compared
  byte for byte with what was sent; the presets' Get compared with the Put; BACKUP_LIST's (id, size, CRC) of the seed's objects recorded.

THE ERASE OF A PROJECT SLOT (no panel): BACKUP_PUT (67) of object 2 + slot with size 0 (begin 0, id, size 0, CRC 0; commit 2, id): the full
backup's own restore path (web/fm1backup.js restoreBackup sends an empty object so), editor_backup.c ed_bk_commit_inner -> proj_write_slot(slot,
raw, 0) -> ocj_write (oc_project.c, both firmwares: ocs_delete of the slot's OCJ3 record and the phrases only it named). Then, where
BACKUP_LIST 1 lists them (0.14.2 and later), the slot's arrangement object 64 + slot with size 0 (ocs_delete of OCS_PROJECT slot: a record the
slot list does not read, OLD FORMAT). Read back: PROJECT op 2 (used 0)
for every slot, BACKUP_LIST objects 2..5 and 64..67 of size 0.
"""
import argparse
import datetime
import json
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from fm1hw import v14, r14                              # noqa: E402

PROJECT_SLOT = 0
PRESET_SLOTS = (70, 71)
PRESET_NAMES = ("Q4 SEED A", "Q4 SEED B")
PRESET_BANK_OBJECT = 18                                 # shared bank 3 (slots 54..71) is backup object 18 (web/EDITOR_PROTOCOL.md, "Shared user library")
SEED_ENGINE = "MACRO"
SEED_LEN = 16
SEED_BPM = 120
SEED_LEVEL = 100
SEED_STEPS = {0: 60, 4: 63, 8: 67, 12: 70}              # step index -> note (track 1, bank 1)
SEED_VEL = 100
NPAT = 8
ST_NOTE, ST_REST = 0, 2
NPOOL, NCHAIN, NENT, NROW, NSCENE = 64, 64, 16, 64, 8
CMD_PROJECT, CMD_PRESET, CMD_DESC, CMD_SET, CMD_TRACK, CMD_MIX, CMD_STEP, CMD_TPARAM = 9, 8, 5, 3, 27, 28, 30, 31
CMD_MOTION, CMD_BK_LIST, CMD_BK_GET, CMD_BK_PUT, CMD_PATTERN, CMD_LIBRARY, CMD_ARRANGE = 64, 65, 66, 67, 73, 80, 102


class Refused(SystemExit):
    pass


class RestoreFailed(Refused):
    """a restore stopped part way: `done` the objects already written (committed)"""
    def __init__(self, msg, done):
        super().__init__(msg)
        self.done = list(done)


def n5(v):
    return [(v >> (7 * j)) & 127 for j in range(5)]


def r5(b):
    return sum(b[j] << (7 * j) for j in range(5)) & 0xFFFFFFFF


def crc32(b):
    import zlib
    return zlib.crc32(bytes(b)) & 0xFFFFFFFF


def pack7(b):
    out = []
    for o in range(0, len(b), 7):
        c = b[o:o + 7]
        out.append(sum((x >> 7) << i for i, x in enumerate(c)))
        out += [x & 127 for x in c]
    return out


def unpack7(a, size):
    out, i = [], 0
    while len(out) < size:
        n = min(7, size - len(out))
        m = a[i]; i += 1
        out += [a[i + k] | ((m >> k) & 1) << 7 for k in range(n)]
        i += n
    return out


# ------------------------------------------------------------------------------------------------ reads
def backup_list(u, arr=True):
    """BACKUP_LIST (65): {id: (size, crc)}; with arr, the arrangement objects too (LIST 1, the same session; none on a firmware without them)"""
    r = u.req(CMD_BK_LIST, [], timeout=8.0)
    if r[0] != 1 or r[1]:
        raise Refused(f"unit_objects: BACKUP_LIST rc {r[1]} (3: the transport runs)")
    objs = {r[3 + 11 * k]: (r5(r[4 + 11 * k:9 + 11 * k]), r5(r[9 + 11 * k:14 + 11 * k])) for k in range(r[2])}
    if arr:
        a = u.req(CMD_BK_LIST, [1], timeout=8.0)
        if a[0] == 1 and a[1] == 0:
            objs.update({a[3 + 11 * k]: (r5(a[4 + 11 * k:9 + 11 * k]), r5(a[9 + 11 * k:14 + 11 * k])) for k in range(a[2])})
    return objs


def arrange_info(u):
    """ARRANGE INFO (102 op 0): size, ntrk, busy, and per pool phrase (flags, LEN, CRC)"""
    r = u.req(CMD_ARRANGE, [0], timeout=3.0)
    if r[0] != 0 or r[1] != 0:
        raise Refused(f"unit_objects: ARRANGE INFO refused: {r[:4]}")
    size, ntrk, used, busy = r[3] | r[4] << 7, r[10], r[11], r[12]
    ph = [{"id": k, "flags": r[18 + 7 * k], "len": r[19 + 7 * k], "crc": r5(r[20 + 7 * k:25 + 7 * k])} for k in range(r[5])]
    return {"size": size, "ntrk": ntrk, "tracks_used": used, "busy": busy, "phrases": ph}


def lists_get(u, size):
    out = []
    while len(out) < size:
        n = min(256, size - len(out))
        r = u.req(CMD_ARRANGE, [1, len(out) & 127, len(out) >> 7, n & 127, n >> 7], timeout=3.0)
        if r[0] != 1 or r[1]:
            raise Refused(f"unit_objects: ARRANGE GET rc {r[1]}")
        out += unpack7(r[6:], n)
    return out


def lists_layout(ntrk):
    """ocl_lists_t (pattern.c; web/EDITOR_PROTOCOL.md 'The lists' bytes'): {field: (offset, length)}"""
    f, at = {}, 0
    for name, n in (("chain", NCHAIN * NENT * 2), ("song", NROW * ntrk), ("tchain", ntrk), ("cur", ntrk), ("row", ntrk), ("mode", 1), ("launch", 1),
                    ("rsv", 2), ("scene", NSCENE * ntrk * 2), ("scene_used", 1), ("rsv2", 3), ("groove", ntrk), ("gamt", ntrk)):
        f[name] = (at, n); at += n
    f["_size"] = (0, at)
    return f


WRITTEN = ("chain", "song", "launch", "scene", "scene_used", "groove", "gamt")   # what COMMIT takes (ed_ar_apply); tchain cur row mode are the unit's


def seed_lists(base, ntrk, pid):
    """the seed's lists over `base` (the unit's, for the bytes the write ignores): every written field the seed's"""
    L = lists_layout(ntrk)
    b = list(base)
    for k in WRITTEN:
        o, n = L[k]
        b[o:o + n] = [0] * n
    b[L["chain"][0]] = pid + 1                           # chain 1, step 1: the phrase (+ 1), REPS 1
    b[L["song"][0]] = 1                                  # song row 1: track 1 plays chain 1
    b[L["scene"][0]] = 1                                 # scene 1, track 1: chain 1 (+ 1), step 1, not muted
    b[L["scene_used"][0]] = 1
    return b


def written_of(b, ntrk):
    L = lists_layout(ntrk)
    return {k: list(b[L[k][0]:L[k][0] + L[k][1]]) for k in WRITTEN}


def lib_get(u, k):
    r = u.req(CMD_LIBRARY, [1, k & 127, k >> 7], timeout=3.0)
    if r[3]:
        raise Refused(f"unit_objects: library Get of slot {k} rc {r[3]}")
    return r[4:]


def lib_used(u, old=False):
    """the used library slots (op 0 List); with old, the slots of a format this firmware does not read too (used 0, oldFormat 1: shown OLD,
    erased by op 5 like any other)"""
    used, slot, total = [], 0, None
    while total is None or slot < total:
        a = u.req(CMD_LIBRARY, [0, slot & 127, slot >> 7, 16], timeout=3.0)
        if a[3]:
            raise Refused(f"unit_objects: library list rc {a[3]} at {slot}")
        cnt, total, i = a[4], a[5] | a[6] << 7, 7
        for j in range(cnt):
            if a[i] or (old and a[i + 2]):
                used.append(slot + j)
            i = a.index(0, i + 3) + 1
        slot += cnt
        if not cnt:
            break
    return used


def info(u):
    i = u.info()
    names = {v: k for k, v in i["engines"].items()}
    if SEED_ENGINE not in names:
        raise Refused(f"unit_objects: no engine {SEED_ENGINE} in INFO's names ({sorted(names)}): the seed cannot be written on this firmware")
    return i, names[SEED_ENGINE]


def project_used(u, slot):
    r = u.req(CMD_PROJECT, [2, slot], timeout=3.0)
    return bool(len(r) > 2 and r[2])


def step_req(t, i, note=None):
    if note is None:
        return [t, i, 0, 0, 0, 0, 0, ST_REST, 0, 0, 0, 0, 0, 100]
    return [t, i, 1, note, 0, 0, 0, ST_NOTE, 0, SEED_VEL, 0, 0, 0, 100]


def step_same(req, rep):
    """the bytes the seed sets, as the reply gives them back (track, index, then STEP_GET's bytes: the request's places): n, the 4 notes, time,
    flags, vel (2..9) and the chance (13); the hits' bytes carry the gate for a note (ed_step_reply), the Nightly adds the feel (14..16)"""
    return list(rep[2:10]) == list(req[2:10]) and len(rep) > 13 and rep[13] == req[13]


def put_step(u, t, i, note=None):
    q = step_req(t, i, note)
    r = u.req(CMD_STEP, q, timeout=2.0)
    if not step_same(q, r):
        raise Refused(f"unit_objects: track {t + 1} step {i + 1} reads back {r[2:14]} (sent {q[2:]})")


# ------------------------------------------------------------------------------------------------ the preconditions
def suites_running():
    import unit_guard as UG
    return UG.suites_running()


def stopped(u, tool="unit_objects"):
    r = u.req(CMD_ARRANGE, [0], timeout=2.0)
    if not (len(r) > 12 and r[0] == 0 and r[1] == 0):
        raise Refused(f"{tool}: the transport state cannot be read (ARRANGE INFO): refused")
    if r[12]:
        raise Refused(f"{tool}: the transport runs (playing, counting in or a song chain): refused")
    sel, trs = u.tracks()
    if any(t["armed"] for t in trs):
        raise Refused(f"{tool}: a track is armed for recording: refused")


def take_backup(out, label, tag):
    """oc_install.mjs backup (the page's captureBackup, read back with readBackup): the port must be free (no Unit open)"""
    path = os.path.join(out, f"backup-{label}-{tag}-{datetime.datetime.now():%Y%m%d-%H%M%S}.json")
    r = subprocess.run(["node", os.path.join(HERE, "oc_install.mjs"), "backup", path], env=dict(os.environ, PYTHON=sys.executable),
                       stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=900)
    if r.returncode or not os.path.exists(path):
        raise Refused(f"unit_objects: the backup failed (oc_install.mjs backup: {(r.stdout + r.stderr).strip()[-300:]}): nothing is written")
    return path


def backup_matches(u, path):
    """the backup file reads back and is the unit now: every object's (size, CRC) equal to BACKUP_LIST's, the arrangement objects too -> list of
    differences ([] good)"""
    try:
        f = json.load(open(path))
        objs = {o["id"]: (o["size"], o["crc"]) for o in f["objects"]}
        if f.get("format") != "felucca-backup" or f.get("version") != 1 or not objs:
            return [f"{path}: not a complete backup (format, version 1, objects)"]
        for o in f["objects"]:
            import base64
            data = base64.b64decode(o["data"]) if o["size"] else b""
            if len(data) != o["size"] or (o["size"] and crc32(data) != o["crc"]):
                return [f"{path}: object {o['id']} does not read back (size or CRC)"]
    except (OSError, ValueError, KeyError, TypeError) as e:
        return [f"{path}: does not read back ({e!r})"]
    now = backup_list(u, arr=True)                       # (review: always the arrangement objects; one the file lacks and the unit holds is a difference)
    diff = [f"object {k}: file {objs.get(k, 'missing')}, unit {now.get(k)}" for k in sorted(set(objs) | set(now)) if objs.get(k, (0, 0)) != now.get(k, (0, 0))]
    return diff


def preconditions(open_unit, out, label, backup=None, tool="unit_objects"):
    """suites, the transport, then the backup (taken now unless given) checked against the unit -> (u, backup path)"""
    s = suites_running()
    if s:
        raise Refused(f"{tool}: a host suite is running: refused ({s})")
    u = open_unit()
    try:
        stopped(u, tool)
    finally:
        u.close()
    path = backup or take_backup(out, label, "before-objects")
    u = open_unit()
    try:
        stopped(u, tool)
        d = backup_matches(u, path)
        if d:
            raise Refused(f"{tool}: the backup {path} is not the unit as it is now ({d[:4]}): take a new one first; nothing is written")
    except BaseException:
        u.close()
        raise
    return u, path


# ------------------------------------------------------------------------------------------------ the seed
def descriptors(u, p_count):
    """each track parameter's descriptor for the selected track's engine: (labels, defaults as they are, defaults in Put's range -64..127: the
    library's wire carries no other value, editor_library.c op 2)"""
    raw, put, labels = [], [], []
    for p in range(p_count):
        r = u.req(CMD_DESC, [0, p], timeout=2.0)
        lo, hi, d = r14(r[3:5]), r14(r[5:7]), r14(r[7:9])
        raw.append(max(lo, min(hi, d)))
        put.append(max(-64, min(127, raw[-1])))
        labels.append(bytes(r[9:]).split(b"\0")[0].decode("latin-1"))
    return labels, raw, put


def preset_values(u, macro, p_count):
    labels, raw, put = descriptors(u, p_count)
    return put, labels


def lib_put_req(k, engine, name, values):
    return [2, k & 127, k >> 7, engine, *name.encode(), 0, *[b for v in values for b in v14(v)], *([0] * 32), 0]


def lib_expect(engine, name, values):
    return [1, engine, *name.encode(), 0, *[b for v in values for b in v14(v)], *([0] * 32), 0]


def put_preset(u, k, engine, name, values):
    """Put, Get: the Get must be the Put's content byte for byte; a value the unit maps (a reserved parameter reads 0) is taken as it reads back,
    once, and then must hold"""
    for attempt in range(2):
        r = u.req(CMD_LIBRARY, lib_put_req(k, engine, name, values), timeout=5.0)
        if r[3]:
            raise Refused(f"unit_objects: library Put of slot {k} ({name}) rc {r[3]} (1 refused, 2 flash)")
        got = lib_get(u, k)
        want = lib_expect(engine, name, values)
        if got == want:
            return values
        if attempt or got[:2 + len(name) + 1] != want[:2 + len(name) + 1]:
            at = next((i for i in range(max(len(got), len(want))) if got[i:i + 1] != want[i:i + 1]), None)
            raise Refused(f"unit_objects: library slot {k} reads back other bytes than were put ({name}): first difference at byte {at}")
        o = 2 + len(name) + 1
        values = [r14(got[o + 2 * j:o + 2 * j + 2]) for j in range(len(values))]
    raise Refused("unreachable")


def seed(u, out, label, log=print):
    """the seed set written and read back (the preconditions are the caller's) -> the manifest (also OUT/seed-LABEL.json)"""
    i, macro = info(u)
    nstep, p_count = i["nstep"], i["p_count"]
    ai = arrange_info(u)
    ntrk = ai["ntrk"]
    m = {"tool": "unit_objects seed", "version": i["version"], "at": datetime.datetime.now().isoformat(timespec="seconds"), "engine": macro,
         "project_slot": PROJECT_SLOT, "preset_slots": list(PRESET_SLOTS), "steps": {str(k): v for k, v in SEED_STEPS.items()}, "len": SEED_LEN,
         "bpm": SEED_BPM, "ntrk": ntrk, "nstep": nstep}
    u.req(CMD_TRACK, [0], timeout=2.0)
    r = u.req(CMD_PRESET, [macro, 0, 0], timeout=3.0)
    if r[0] != macro or len(r) > 3:
        raise Refused(f"unit_objects: track 1 refused {SEED_ENGINE} preset 0: {r}")
    labels, raw_defaults, defaults = descriptors(u, p_count)
    if labels.count("LEN") < 1:
        raise Refused("unit_objects: no track parameter LEN on the unit's descriptor")
    p_len = labels.index("LEN")
    m["params"], m["params_not_default"] = [], []
    for t in range(ntrk):
        u.req(CMD_TRACK, [t], timeout=2.0)
        r = u.req(CMD_PATTERN, [t, 1, 0], timeout=2.0)
        if r[2] or r[3] != 0:
            raise Refused(f"unit_objects: track {t + 1}: bank 1 not selected ({r})")
        r = u.req(CMD_PRESET, [macro, 0, 0], timeout=3.0)
        if r[0] != macro or len(r) > 3:
            raise Refused(f"unit_objects: track {t + 1} refused {SEED_ENGINE} preset 0: {r}")
        got = []
        for p in range(p_count):                         # (the defaults as they are: TRACK_PARAM carries 14 bits; review: not Put's -64..127)
            want = SEED_LEN if p == p_len else raw_defaults[p]
            r = u.req(CMD_TPARAM, [t, p, *v14(want)], timeout=2.0)
            got.append(r14(r[2:4]))
            if got[-1] != want:
                m["params_not_default"].append({"track": t + 1, "id": p, "label": labels[p], "sent": want, "unit": got[-1]})
        for p in range(p_count):                         # (read back: the value the set answered)
            r = u.req(CMD_TPARAM, [t, p], timeout=2.0)
            if r14(r[2:4]) != got[p]:
                raise Refused(f"unit_objects: track {t + 1} parameter {p} ({labels[p]}) reads {r14(r[2:4])}, the set answered {got[p]}")
        if got[p_len] != SEED_LEN:
            raise Refused(f"unit_objects: track {t + 1}: LEN reads {got[p_len]}, not {SEED_LEN}")
        m["params"].append(got)
        r = u.req(CMD_MOTION, [t, 2], timeout=2.0)
        if r[1]:
            raise Refused(f"unit_objects: track {t + 1}: MOTION clear rc {r[1]}")
        for s in range(nstep):
            put_step(u, t, s)
        for b in range(1, NPAT):
            r = u.req(CMD_PATTERN, [t, 2, 0, b], timeout=3.0)
            if r[2]:
                raise Refused(f"unit_objects: track {t + 1}: bank 1 not copied to bank {b + 1} (rc {r[2]})")
        u.req(CMD_MIX, [t, *v14(SEED_LEVEL), 0], timeout=2.0)
    for s, note in SEED_STEPS.items():
        put_step(u, 0, s, note)
    bpm = u.global_id("BPM")
    u.req(CMD_SET, [1, bpm, *v14(SEED_BPM)], timeout=2.0)
    u.req(CMD_TRACK, [0], timeout=2.0)
    save(u)
    ai = arrange_info(u)
    held = [p for p in ai["phrases"] if p["flags"] & 3 == 3]
    if len(held) != 1 or held[0]["len"] != SEED_LEN:
        raise Refused(f"unit_objects: after the save the music loaded holds {len(held)} phrases with steps ({held[:4]}), not the seed's one of LEN {SEED_LEN}: "
                      "something the editor cannot clear (timed notes?) is in a bank")
    pid = held[0]["id"]
    base = lists_get(u, ai["size"])
    want = seed_lists(base, ntrk, pid)
    lists_put(u, want)
    save(u)
    m["phrase"] = {"id": pid, "number": pid + 1, "len": held[0]["len"], "crc": held[0]["crc"], "pool_object": 68 + pid // 4}
    m["lists_written"] = written_of(want, ntrk)
    m["lists_written_crc"] = crc32(sum(m["lists_written"].values(), []))
    # the presets (the selected track plays MACRO: the defaults are MACRO's)
    vals_b = list(defaults)
    if "LEVEL" in labels:
        j = labels.index("LEVEL")
        vals_b[j] = max(-64, defaults[j] - 20)
    m["presets"] = []
    held_before = lib_used(u, old=True)
    m["preset_slots_overwritten"] = [k for k in PRESET_SLOTS if k in held_before]   # (the backup taken before holds them; a restore puts them back)
    for k, name, vals in zip(PRESET_SLOTS, PRESET_NAMES, (defaults, vals_b)):
        got = put_preset(u, k, macro, name, vals)
        m["presets"].append({"slot": k, "name": name, "engine": macro, "values": got, "labels": labels, "get": lib_expect(macro, name, got)})
    live = verify_live(u, m)
    if live:
        raise Refused(f"unit_objects: the seed does not read back: {live[:6]}")
    objs = backup_list(u)
    import base64
    m["objects"] = {}
    for k in sorted({2 + PROJECT_SLOT, 64 + PROJECT_SLOT, 68 + pid // 4, PRESET_BANK_OBJECT}):
        if k in objs:
            b = get_object(u, k, objs[k][0])
            if (len(b), crc32(b) if b else 0) != objs[k]:
                raise Refused(f"unit_objects: object {k} reads back {len(b)} bytes, CRC {crc32(b):08x}; BACKUP_LIST says {objs[k]}")
            m["objects"][str(k)] = {"size": len(b), "crc": objs[k][1], "data": base64.b64encode(bytes(b)).decode()}
    check = verify(u, m, log=log)
    if not check["ok"]:
        raise Refused(f"unit_objects: the seed does not read back: {check['problems'][:6]}")
    m["verified"] = check
    f = os.path.join(out, f"seed-{label}.json")
    json.dump(m, open(f, "w"), indent=1)
    m["file"] = f
    log(json.dumps({"seed": "written and read back", "file": f, "phrase": m["phrase"], "objects": {k: (o["size"], o["crc"]) for k, o in m["objects"].items()},
                    "preset_slots_overwritten": m["preset_slots_overwritten"], "params_not_default": len(m["params_not_default"])}))
    return m


def save(u):
    try:
        r = u.req(CMD_PROJECT, [1, PROJECT_SLOT], timeout=15.0)
    except TimeoutError:
        raise Refused(f"unit_objects: PROJECT save to slot {PROJECT_SLOT + 1} not confirmed (the legacy reply has no failure bit: no reply is a failed save)")
    if r[:3] != [1, PROJECT_SLOT, 1]:
        raise Refused(f"unit_objects: PROJECT save to slot {PROJECT_SLOT + 1}: {r}")
    time.sleep(0.2)


def lists_put(u, b):
    size = len(b)
    r = u.req(CMD_ARRANGE, [2, size & 127, size >> 7, *n5(crc32(b))], timeout=3.0)
    if r[:2] != [2, 0]:
        raise Refused(f"unit_objects: ARRANGE BEGIN rc {r[1:2]}")
    for o in range(0, size, 256):
        r = u.req(CMD_ARRANGE, [3, o & 127, o >> 7, *pack7(b[o:o + 256])], timeout=3.0)
        if r[:2] != [3, 0]:
            raise Refused(f"unit_objects: ARRANGE DATA at {o} rc {r[1:2]}")
    r = u.req(CMD_ARRANGE, [4], timeout=10.0)
    if r[:2] != [4, 0]:
        raise Refused(f"unit_objects: ARRANGE COMMIT rc {r[1:3]} (2 a value, 4 a track not moved, 6 PHRASES FULL)")


def get_object(u, oid, size):
    """BACKUP_GET (66) of a whole object, in the session the last BACKUP_LIST began"""
    out = []
    while len(out) < size:
        n = min(256, size - len(out))
        r = u.req(CMD_BK_GET, [oid, *n5(len(out)), n & 127, n >> 7], timeout=3.0)
        if r[0] != oid or r[1]:
            raise Refused(f"unit_objects: BACKUP_GET of object {oid} at {len(out)}: rc {r[1]} (3 the transport runs, 5 a new session needed)")
        out += unpack7(r[9:], n)
    return out


def verify_live(u, m):
    """right after the seed's saves, the music loaded (the seed's own) read back: tracks, every step of track 1, BPM, the phrase, the lists"""
    problems = []
    sel, trs = u.tracks()
    for t, tr in enumerate(trs[:m["ntrk"]]):
        if (tr["engine"], tr["preset"], tr["level"], tr["mute"]) != (m["engine"], 0, SEED_LEVEL, 0):
            problems.append(f"track {t + 1}: {tr}")
    steps = {int(k): v for k, v in m["steps"].items()}
    for s in range(m["nstep"]):
        q = step_req(0, s, steps.get(s))
        r = u.req(CMD_STEP, [0, s], timeout=2.0)
        if not step_same(q, r):
            problems.append(f"track 1 step {s + 1}: {r[2:14]}, the seed {q[2:]}")
    bpm = u.get_global(u.global_id("BPM"))
    if bpm != m["bpm"]:
        problems.append(f"BPM {bpm}")
    ai = arrange_info(u)
    got = written_of(lists_get(u, ai["size"]), m["ntrk"])
    if got != m["lists_written"]:
        problems.append(f"the lists (chain, song, scene, groove): fields {[k for k in got if got[k] != m['lists_written'][k]]} differ")
    return problems


VERIFY_READS = {1, 2, 5, 9, 27, 65, 66, 80, 102}        # (verify sends only reads: PROJECT op 2, library op 0/1, ARRANGE op 0)


def verify(u, m, log=print):
    """READ ONLY (review): the seed set as the manifest says, without loading anything. The transport must be stopped (BACKUP_LIST stops a running
    one: ed_bk_capture). The slot's objects (2 + slot: the FBKK, every track's parameters, LEN and steps; 64 + slot: the record with the lists and
    the scene), the phrase's pool chunk (68 + id / 4) and the preset bank (18) compared byte for byte with the seed's read-back (BACKUP_GET); the
    phrase by ARRANGE INFO (holds steps, its LEN and CRC); the presets by library Get (by label when the firmware's P_COUNT differs)"""
    import base64
    stopped(u)
    problems = []
    slot = m["project_slot"]
    if not project_used(u, slot):
        problems.append(f"project slot {slot + 1} is empty")
    objs = backup_list(u)
    for k, o in sorted(m.get("objects", {}).items(), key=lambda x: int(x[0])):
        k = int(k)
        if objs.get(k) != (o["size"], o["crc"]):
            problems.append(f"object {k}: {objs.get(k)}, the seed's ({o['size']}, {o['crc']})")
            continue
        if bytes(get_object(u, k, o["size"])) != base64.b64decode(o["data"]):
            problems.append(f"object {k}: other bytes")
    ai = arrange_info(u)
    pid = m["phrase"]["id"]
    ph = ai["phrases"][pid]
    if not ph["flags"] & 1 or ph["len"] != m["phrase"]["len"] or ph["crc"] != m["phrase"]["crc"]:
        problems.append(f"the phrase {pid + 1}: {ph} (the seed: {m['phrase']})")
    mapped, labels_now = [], None
    for p in m["presets"]:
        g = lib_get(u, p["slot"])
        if g == p["get"]:
            continue
        head = 2 + len(p["name"]) + 1
        if g[:head] != p["get"][:head]:
            problems.append(f"preset slot {p['slot']} ({p['name']}): used, engine or name read back as {g[:head]}")
            continue
        # another firmware's parameter layout (P_COUNT): the values compared by their labels (a label both firmwares have once); DESC is a read
        if labels_now is None:
            labels_now = descriptors(u, u.info()["p_count"])[0]
        now = {lab: r14(g[head + 2 * j:head + 2 * j + 2]) for j, lab in enumerate(labels_now) if lab and labels_now.count(lab) == 1}
        was = {lab: v for lab, v in zip(p["labels"], p["values"]) if lab and p["labels"].count(lab) == 1}
        diff = sorted(k for k in set(now) & set(was) if now[k] != was[k])
        mapped.append({"slot": p["slot"], "compared_by_label": len(set(now) & set(was)), "differ": diff})
        if diff or len(labels_now) == len(p["labels"]):
            problems.append(f"preset slot {p['slot']} ({p['name']}): values differ: {diff[:8] or 'the same layout, other bytes'}")
    out = {"ok": not problems, "problems": problems, "phrase": ph, "presets_by_label": mapped, "version": u.info()["version"]}
    log(json.dumps({"verify": out}))
    return out


# ------------------------------------------------------------------------------------------------ the restore
IDS_V1 = list(range(29))
IDS_ARR = list(range(29)) + list(range(64, 85))


def read_backup(path):
    """the file checked as web/fm1backup.js readBackup does what this tool restores: format, version 1, the ids of a complete archive (29 or 50
    objects), each object's data, size and CRC -> [(id, bytes)]"""
    import base64
    try:
        f = json.load(open(path))
        objs = f["objects"]
        ids = [o["id"] for o in objs]
        if f.get("format") != "felucca-backup" or f.get("version") != 1 or ids not in (IDS_V1, IDS_ARR):
            raise Refused(f"unit_objects: {path}: not a complete Orange Cat backup (format, version 1, ids 0..28 [+ 64..84])")
        out = []
        for o in objs:
            b = base64.b64decode(o["data"]) if o["size"] else b""
            if len(b) != o["size"] or (o["size"] and crc32(b) != o["crc"]) or (not o["size"] and o["crc"]):
                raise Refused(f"unit_objects: {path}: object {o['id']} does not read back (size or CRC)")
            out.append((o["id"], b))
        if not out[0][1] or not out[1][1]:
            raise Refused(f"unit_objects: {path}: the backup is missing the current music or settings")
        return out
    except (OSError, ValueError, KeyError, TypeError) as e:
        raise Refused(f"unit_objects: {path}: does not read back ({e!r})")


PACE_S = 0.005          # (Round 32, the unit's restore) a pause after each BACKUP_PUT reply before the next frame: usb.c keeps ONE SysEx frame; a
                        # frame whose F0 comes while the last one is not yet freed (editor.c ed_service frees a large frame after ed_handle) is dropped
                        # whole (sx_rx_dropped): the request is never answered. The page is paced by Web MIDI's own latency; Python answers in ~2 ms
SETTLE_S = 1.0          # after a lost reply: a late reply arrives and is drained (fm1hw.Unit.req drains before it sends) before anything is asked again
PUT_TRIES = 4


class Anomaly(Exception):
    """a BACKUP_PUT exchange that cannot be trusted: no reply, a reply that is not this request's (op, id), or a data rc other than 0"""


def req67(u, args, timeout=5.0):
    """one BACKUP_PUT exchange: the reply must echo (op, id); never resent (fm1hw.Unit.req: 67 is not RESEND_SAFE)"""
    try:
        r = u.req(CMD_BK_PUT, args, timeout=timeout)
    except TimeoutError as e:
        raise Anomaly(f"no reply to {args[:2]}") from e
    if len(r) < 3 or r[0] != args[0] or r[1] != args[1]:
        raise Anomaly(f"a reply that is not this request's: {r[:3]} for {args[:2]}")
    time.sleep(PACE_S)
    return r[2]


def put_object(u, oid, b, log=print, tries=PUT_TRIES):
    """one backup object written (BACKUP_PUT begin, data in 256-byte chunks as web/fm1backup.js sends them, commit), safe against a lost frame or
    reply: the unit takes a chunk only at its staging's offset (editor_backup.c ed_bk_write: another offset answers 1 and writes nothing), so no
    chunk lands twice; on any anomaly the staging is aborted, late replies settle and drain, the object's (size, CRC) is read back (BACKUP_LIST:
    a commit whose reply was lost has taken effect), and the object is written again from its begin (begin resets the staging and drops a
    stream). -> the attempts used. Refused: the unit refuses the object itself (begin rc 1..4, commit rc 2 twice)"""
    want = (len(b), crc32(b) if b else 0)
    hard = 0
    for attempt in range(1, tries + 1):
        try:
            rc = req67(u, [0, oid, *n5(len(b)), *n5(want[1])])
            if rc:
                raise Refused(f"unit_objects: BACKUP_PUT begin of object {oid} ({len(b)} B): rc {rc} (1 an object or size this firmware refuses, 3 the transport runs, 4 no room)")
            for off in range(0, len(b), 256):
                rc = req67(u, [1, oid, *n5(off), *pack7(list(b[off:off + 256]))])
                if rc:
                    raise Anomaly(f"data at {off}: rc {rc} (1: the unit's staging is not at {off}; 5 stale; 4 flash)")
            rc = req67(u, [2, oid], timeout=30.0)
            if rc == 2:
                hard += 1
                if hard > 1:
                    raise Refused(f"unit_objects: BACKUP_PUT commit of object {oid}: rc 2 twice (the unit refuses its bytes)")
            if rc:
                raise Anomaly(f"commit: rc {rc}")
            return attempt
        except Anomaly as e:
            log(json.dumps({"put_anomaly": oid, "attempt": attempt, "what": str(e)}))
            try:
                u.req(CMD_BK_PUT, [3, oid], timeout=3.0)
            except TimeoutError:
                pass
            time.sleep(SETTLE_S)
            try:
                if backup_list(u, arr=oid >= 64).get(oid) == want:
                    return attempt                       # (the commit took effect; its reply was what was lost)
            except (TimeoutError, Refused):
                time.sleep(SETTLE_S)
    raise Refused(f"unit_objects: object {oid} not written after {tries} attempts (each anomaly logged)")


def link_counters(u):
    """AUDIO_STATS [0, 5] (0.15.0 and later): SysEx frames the unit dropped (one still pending), too long, replies cut -> dict or None"""
    try:
        r = u.req(72, [0, 5], timeout=1.5)
    except TimeoutError:
        return None
    return {"dropped": r5(r[1:6]), "long": r5(r[6:11]), "cut": r5(r[11:16])} if len(r) > 16 and r[0] == 12 else None


def restore(u, path, log=print):
    """the backup written back as web/editor.html's Complete backup > Restore backup does (web/fm1backup.js restoreBackup): every byte checked
    first; every non-empty object's size offered and aborted before the first write (a refused archive writes nothing); then the objects 2..83
    in the file's order, settings (1), the music (0) and its arrangement (84) last; an empty object written empty (the slot or record it names
    goes). Then BACKUP_LIST (LIST 0 and LIST 1) compared with the file: every object equal; one the unit holds and the file does not is a
    difference (the editor's restore leaves it) -> {ok, problems, ...}"""
    objs = read_backup(path)
    stopped(u)
    supported = backup_list(u, arr=True)
    if any(oid not in supported and b for oid, b in objs):
        raise Refused(f"unit_objects: {path}: this firmware does not support a non-empty object of the backup "
                      f"({[oid for oid, b in objs if oid not in supported and b]}): a backup taken on a Nightly is refused by 0.15.0")
    links0 = link_counters(u)
    for oid, b in objs:
        if b and oid in supported:
            for t in range(PUT_TRIES):                   # (begin and abort only: both may be sent again)
                try:
                    rc = req67(u, [0, oid, *n5(len(b)), *n5(crc32(b))])
                    req67(u, [3, oid])
                    break
                except Anomaly:
                    time.sleep(SETTLE_S)
            else:
                raise Refused(f"unit_objects: {path}: object {oid}: no clean answer to its size check: nothing written")
            if rc:
                raise Refused(f"unit_objects: {path}: object {oid} ({len(b)} B) refused by the unit before any write (rc {rc}): nothing written")
    order = [x for x in objs[2:] if x[0] != 84] + [objs[1], objs[0]] + [x for x in objs if x[0] == 84]
    done = []
    for oid, b in order:
        if oid not in supported:
            continue
        try:
            put_object(u, oid, b, log)
        except BaseException as e:
            try:
                u.req(CMD_BK_PUT, [3, oid], timeout=5.0)
            except BaseException:
                pass
            raise RestoreFailed(f"unit_objects: the restore of {path} stopped at object {oid} ({e.code if isinstance(e, SystemExit) else repr(e)}); "
                                f"written already: {done}", done)
        done.append(oid)
    time.sleep(0.5)
    now = backup_list(u, arr=True)
    want = {oid: (len(b), crc32(b) if b else 0) for oid, b in objs}
    problems = [f"object {k}: unit {now.get(k)}, backup {want[k]}" for k in sorted(want) if k in supported and now.get(k) != want[k]]
    extra = {k: v for k, v in now.items() if k not in want and v[0]}
    problems += [f"object {k}: on the unit {v}, not in the backup (the editor's restore leaves it)" for k, v in sorted(extra.items())]
    res = {"restore": path, "written": done, "ok": not problems, "problems": problems, "link_counters": {"before": links0, "after": link_counters(u)}}
    log(json.dumps(res))
    return res


# ------------------------------------------------------------------------------------------------ the erases
def bk_put_empty(u, oid):
    put_object(u, oid, b"", log=lambda line: None)


def wipe_projects(u, log=print):
    """every project slot emptied by the backup restore's empty object (2 + slot, then 64 + slot where listed); read back empty"""
    before = backup_list(u)
    done = []
    for slot in range(4):
        bk_put_empty(u, 2 + slot)
        done.append(2 + slot)
    mid = backup_list(u)
    for slot in range(4):
        if 64 + slot in mid and mid[64 + slot][0]:
            bk_put_empty(u, 64 + slot)
            done.append(64 + slot)
    after = backup_list(u)
    used = [s + 1 for s in range(4) if project_used(u, s)]
    left = {k: after[k] for k in (2, 3, 4, 5, 64, 65, 66, 67) if k in after and after[k][0]}
    res = {"wipe": "projects", "command": "BACKUP_PUT (67) size 0 of objects " + " ".join(map(str, done)), "slots_used_after": used, "objects_left": left,
           "before": {k: before.get(k) for k in (2, 3, 4, 5, 64, 65, 66, 67) if k in before}}
    log(json.dumps(res))
    if used or left:
        raise Refused(f"unit_objects: project slots not empty after the erase: used {used}, objects {left}")
    return res


def wipe_presets(u, log=print):
    """every library slot erased that is used or of a format this firmware does not read (OLD): op 5 erases either (editor_library.c up_put(k, 0))"""
    used = lib_used(u, old=True)
    done = []
    for k in used:
        r = u.req(CMD_LIBRARY, [5, k & 127, k >> 7], timeout=5.0)
        done.append({"slot": k, "rc": r[3]})
    after = lib_used(u, old=True)
    res = {"wipe": "presets", "command": "library op 5 (editor 80) per used or OLD slot", "erased": done, "used_after": after}
    log(json.dumps(res))
    if after:
        raise Refused(f"unit_objects: {len(after)} preset slots still used after the erase: {after[:8]}")
    return res


# ------------------------------------------------------------------------------------------------ the command line
def main(argv=None, open_unit=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("case", choices=("seed", "verify", "wipe-projects", "wipe-presets", "restore"))
    ap.add_argument("--backup", help="a full backup just taken (it must be the unit as it is now); else one is taken first")
    ap.add_argument("--manifest", help="verify: the seed's manifest (OUT/seed-LABEL.json)")
    ap.add_argument("--confirm", action="append", choices=("SEED", "WIPE", "RESTORE"), default=[])
    a = ap.parse_args(argv)
    os.makedirs(a.out, exist_ok=True)
    if open_unit is None:
        import fm1hw
        open_unit = fm1hw.Unit
    logf = os.path.join(a.out, f"unit-objects-{a.label}.log")

    def log(line):
        print(line, flush=True)
        with open(logf, "a") as f:
            f.write(line + "\n")
    try:
        if a.case == "verify":
            if not a.manifest:
                raise Refused("unit_objects: verify needs --manifest FILE")
            u = open_unit()
            try:
                return 0 if verify(u, json.load(open(a.manifest)), log)["ok"] else 1
            finally:
                u.close()
        word = {"seed": "SEED", "restore": "RESTORE"}.get(a.case, "WIPE")
        if word not in a.confirm:
            raise Refused(f"unit_objects: {a.case} writes the unit: give --confirm {word} on the command line (nothing is asked)")
        if a.case == "restore":
            if not a.backup:
                raise Refused("unit_objects: restore needs --backup FILE")
            read_backup(a.backup)
            if suites_running():
                raise Refused("unit_objects: a host suite is running: refused")
            u = open_unit()
            try:
                return 0 if restore(u, a.backup, log)["ok"] else 1
            finally:
                u.close()
        u, path = preconditions(open_unit, a.out, a.label, a.backup)
        log(json.dumps({"case": a.case, "backup": path, "restore": "web/editor.html, Complete backup > Restore backup, on the firmware it was taken on"}))
        try:
            {"seed": lambda: seed(u, a.out, a.label, log), "wipe-projects": lambda: wipe_projects(u, log), "wipe-presets": lambda: wipe_presets(u, log)}[a.case]()
        finally:
            u.close()
        return 0
    except Refused as e:
        print(e.code if isinstance(e.code, str) else e, file=sys.stderr, flush=True)
        return 2


if __name__ == "__main__":
    sys.exit(main())
