#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""(Round 32, BUILD.md 7.37 Q4) The saved objects of the install cases written and erased over the editor, unattended: no step waits for the
panel. Only commands both firmwares of Q4 have (0.15.0 Standard and the Release 3 Nightly, 4573e79d) are sent; each is read back.

  unit_objects.py OUT LABEL seed --confirm SEED [--backup FILE]       the seed set below written, read back and verified; OUT/seed-LABEL.json
  unit_objects.py OUT LABEL verify --manifest FILE                     read only: the seed set still on the unit (after an install)
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
    "Q4 SEED A" = MACRO with every parameter at its default; "Q4 SEED B" = the same with LEVEL 20 lower. Read back with op 1 Get, byte for byte.
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
CMD_MOTION, CMD_BK_LIST, CMD_BK_PUT, CMD_PATTERN, CMD_LIBRARY, CMD_ARRANGE = 64, 65, 67, 73, 80, 102


class Refused(SystemExit):
    pass


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


def lib_used(u):
    used, slot, total = [], 0, None
    while total is None or slot < total:
        a = u.req(CMD_LIBRARY, [0, slot & 127, slot >> 7, 16], timeout=3.0)
        if a[3]:
            raise Refused(f"unit_objects: library list rc {a[3]} at {slot}")
        cnt, total, i = a[4], a[5] | a[6] << 7, 7
        for j in range(cnt):
            if a[i]:
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
    now = backup_list(u, arr=any(k >= 64 for k in objs))
    diff = [f"object {k}: file {objs.get(k)}, unit {now.get(k)}" for k in sorted(set(objs) | set(now)) if objs.get(k, (0, 0)) != now.get(k, (0, 0))]
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
def preset_values(u, macro, p_count):
    """each track parameter's descriptor default for MACRO (the selected track plays MACRO), in Put's range -64..127"""
    vals, labels = [], []
    for p in range(p_count):
        r = u.req(CMD_DESC, [0, p], timeout=2.0)
        lo, hi, d = r14(r[3:5]), r14(r[5:7]), r14(r[7:9])
        vals.append(max(-64, min(127, max(lo, min(hi, d)))))
        labels.append(bytes(r[9:]).split(b"\0")[0].decode("latin-1"))
    return vals, labels


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
    defaults, labels = preset_values(u, macro, p_count)
    if labels.count("LEN") < 1:
        raise Refused("unit_objects: no track parameter LEN on the unit's descriptor")
    p_len = labels.index("LEN")
    for t in range(ntrk):
        u.req(CMD_TRACK, [t], timeout=2.0)
        r = u.req(CMD_PATTERN, [t, 1, 0], timeout=2.0)
        if r[2] or r[3] != 0:
            raise Refused(f"unit_objects: track {t + 1}: bank 1 not selected ({r})")
        r = u.req(CMD_PRESET, [macro, 0, 0], timeout=3.0)
        if r[0] != macro or len(r) > 3:
            raise Refused(f"unit_objects: track {t + 1} refused {SEED_ENGINE} preset 0: {r}")
        for p in range(p_count):
            u.req(CMD_TPARAM, [t, p, *v14(SEED_LEN if p == p_len else defaults[p])], timeout=2.0)
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
    for k, name, vals in zip(PRESET_SLOTS, PRESET_NAMES, (defaults, vals_b)):
        got = put_preset(u, k, macro, name, vals)
        m["presets"].append({"slot": k, "name": name, "engine": macro, "values": got, "labels": labels, "get": lib_expect(macro, name, got)})
    check = verify(u, m, log=log)
    if not check["ok"]:
        raise Refused(f"unit_objects: the seed does not read back: {check['problems'][:6]}")
    objs = backup_list(u)
    m["objects"] = {str(k): objs.get(k) for k in sorted({2 + PROJECT_SLOT, 64 + PROJECT_SLOT, 68 + pid // 4, PRESET_BANK_OBJECT}) if k in objs}
    m["verified"] = check
    f = os.path.join(out, f"seed-{label}.json")
    json.dump(m, open(f, "w"), indent=1)
    m["file"] = f
    log(json.dumps({"seed": "written and read back", "file": f, "phrase": m["phrase"], "objects": m["objects"]}))
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


def verify(u, m, log=print):
    """read only but for loading the seed's slot (PROJECT op 0): the seed set as the manifest says -> {ok, problems, ...}"""
    problems = []
    slot = m["project_slot"]
    if not project_used(u, slot):
        return {"ok": False, "problems": [f"project slot {slot + 1} is empty"]}
    u.req(CMD_PROJECT, [0, slot], timeout=15.0)
    time.sleep(1.0)
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
    held = [p for p in ai["phrases"] if p["flags"] & 3 == 3]
    pid = m["phrase"]["id"]
    if [p["id"] for p in held] != [pid] or held[0]["len"] != m["phrase"]["len"]:
        problems.append(f"the phrase: {held[:4]} (the seed: {m['phrase']})")
    got = written_of(lists_get(u, ai["size"]), m["ntrk"])
    if got != m["lists_written"]:
        problems.append(f"the lists (chain, song, scene, groove): fields {[k for k in got if got[k] != m['lists_written'][k]]} differ")
    mapped, labels_now = [], None
    for p in m["presets"]:
        g = lib_get(u, p["slot"])
        if g == p["get"]:
            continue
        head = 2 + len(p["name"]) + 1
        if g[:head] != p["get"][:head]:
            problems.append(f"preset slot {p['slot']} ({p['name']}): used, engine or name read back as {g[:head]}")
            continue
        # another firmware's parameter layout (P_COUNT): the values compared by their labels (a label both firmwares have once)
        if labels_now is None:
            _, labels_now = preset_values(u, m["engine"], u.info()["p_count"])
        now = {lab: r14(g[head + 2 * j:head + 2 * j + 2]) for j, lab in enumerate(labels_now) if lab and labels_now.count(lab) == 1}
        was = {lab: v for lab, v in zip(p["labels"], p["values"]) if lab and p["labels"].count(lab) == 1}
        diff = sorted(k for k in set(now) & set(was) if now[k] != was[k])
        mapped.append({"slot": p["slot"], "compared_by_label": len(set(now) & set(was)), "differ": diff})
        if diff or len(labels_now) == len(p["labels"]):
            problems.append(f"preset slot {p['slot']} ({p['name']}): values differ: {diff[:8] or 'the same layout, other bytes'}")
    out = {"ok": not problems, "problems": problems, "phrase": held[:1], "presets_by_label": mapped, "version": u.info()["version"]}
    log(json.dumps({"verify": out}))
    return out


# ------------------------------------------------------------------------------------------------ the erases
def bk_put_empty(u, oid):
    r = u.req(CMD_BK_PUT, [0, oid, *n5(0), *n5(0)], timeout=5.0)
    if r[2]:
        raise Refused(f"unit_objects: BACKUP_PUT begin of object {oid} (size 0) rc {r[2]} (3: the transport runs)")
    r = u.req(CMD_BK_PUT, [2, oid], timeout=20.0)
    if r[2]:
        raise Refused(f"unit_objects: BACKUP_PUT commit of object {oid} (size 0) rc {r[2]} (2 refused, 4 flash)")


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
    used = lib_used(u)
    done = []
    for k in used:
        r = u.req(CMD_LIBRARY, [5, k & 127, k >> 7], timeout=5.0)
        done.append({"slot": k, "rc": r[3]})
    after = lib_used(u)
    res = {"wipe": "presets", "command": "library op 5 (editor 80) per used slot", "erased": done, "used_after": after}
    log(json.dumps(res))
    if after:
        raise Refused(f"unit_objects: {len(after)} preset slots still used after the erase: {after[:8]}")
    return res


# ------------------------------------------------------------------------------------------------ the command line
def main(argv=None, open_unit=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out"); ap.add_argument("label")
    ap.add_argument("case", choices=("seed", "verify", "wipe-projects", "wipe-presets"))
    ap.add_argument("--backup", help="a full backup just taken (it must be the unit as it is now); else one is taken first")
    ap.add_argument("--manifest", help="verify: the seed's manifest (OUT/seed-LABEL.json)")
    ap.add_argument("--confirm", action="append", choices=("SEED", "WIPE"), default=[])
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
        word = "SEED" if a.case == "seed" else "WIPE"
        if word not in a.confirm:
            raise Refused(f"unit_objects: {a.case} writes the unit: give --confirm {word} on the command line (nothing is asked)")
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
