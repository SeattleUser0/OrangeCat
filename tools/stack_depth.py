#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""
Release 3 (r3-pages): the deepest path of the main loop, from the linked image's disassembly (build/melodee.dis).

The firmware's clang (4.0.1) has no -fstack-usage, so the frames are read from the code: a function's frame is what its prologue takes from the stack,
four bytes a register of every `[--sp] = {rets, r15-r4}` push and every `sp += -N` (N bytes), summed over the function (a conservative count: a later
push in the body, an alloca-style adjustment, counts too). Calls are the direct `call N <name : addr>`; a `call rN` (a function pointer) cannot be followed
from the code, so the callbacks the module path hands around are edges written down in INDIRECT below (the readers, the scene check, the hooks):
an edge that is missing would make the depth too small, so each is named with where it is taken.

  tools/stack_depth.py build/melodee.dis [ROOT ...]     the frame of each root's deepest path, and the path
  tools/stack_depth.py --compare OLD.dis NEW.dis         the same for both, side by side, for the boot and install roots
  tools/stack_depth.py --check build/melodee.dis         exit 1 when the main loop's deepest path leaves under 1 KB of the user stack (run_tests.sh)

The main loop's stack is the user stack of app.ld: _ustack_lo 0x01C74100 .. _ustack_top 0x01C7A000 = 24,320 bytes (interrupts and the supervisor use
_sstack: 7,936 bytes). The report is the depth from fm1_main down; fm1_main's own entry (the crt0's) is below it.
"""
import re
import sys

USTACK = 0x01C7A000 - 0x01C74100
# (function, [functions it calls through a pointer]) -- every one is a place where a callback is invoked
INDIRECT = {
    "oc_rd_flash": [],
    "oc_mod_rd": ["oc_rd_flash"],                       # fw_pages_check's reader is oc_mod_rd, which calls the reader of its context (the flash or the card's file)
    "fw_pages_check_ex": ["oc_mod_rd"],
    "fw_ui_check": ["oc_mod_rd", "modui_scene_ok"],
    "oc_rule_tails": ["oc_rd_flash"],
    "modui_publish": ["oc_mod_rd", "modui_scene_ok"],
}
CALL = re.compile(r"\bcall\s+-?\d+\s+<([A-Za-z_0-9.$]+)\s*:")
PUSH = re.compile(r"\[--sp\]\s*=\s*(.*)$")
SPADJ = re.compile(r"\bsp\s*\+=\s*(-\d+)")
REGR = re.compile(r"r(\d+)(?:-r(\d+))?")


def parse(path):
    funcs, cur = {}, None
    for ln in open(path, errors="replace"):
        m = re.match(r"^([A-Za-z_][A-Za-z_0-9.$]*):\s*$", ln)
        if m:
            cur = m.group(1)
            funcs[cur] = {"frame": 0, "calls": set(), "icalls": 0}
            continue
        if cur is None or "\t" not in ln:
            continue
        ins = ln.split("\t", 2)[-1]
        f = funcs[cur]
        p = PUSH.search(ins)
        if p:
            words = 0
            for part in p.group(1).strip("{} \t").split(","):
                part = part.strip()
                if part == "rets":
                    words += 1
                else:
                    r = REGR.fullmatch(part)
                    if r:
                        words += (abs(int(r.group(1)) - int(r.group(2))) + 1) if r.group(2) else 1
            f["frame"] += 4 * words
        a = SPADJ.search(ins)
        if a:
            f["frame"] += -int(a.group(1))
        c = CALL.search(ins)
        if c:
            f["calls"].add(c.group(1))
        elif re.search(r"\bcall\s+r\d+", ins):
            f["icalls"] += 1
    for k, v in INDIRECT.items():
        if k in funcs:
            funcs[k]["calls"].update(x for x in v if x in funcs)
    return funcs


def deepest(funcs, root):
    best = {}

    def walk(fn, stack):
        if fn in best:
            return best[fn]
        if fn in stack or fn not in funcs:
            return (0, [])
        stack.add(fn)
        top = (0, [])
        for c in funcs[fn]["calls"]:
            d = walk(c, stack)
            if d[0] > top[0]:
                top = d
        stack.discard(fn)
        res = (funcs[fn]["frame"] + top[0], [fn] + top[1])
        best[fn] = res
        return res
    return walk(root, set())


def path_to(funcs, root, target):
    """the deepest path from root that passes through target: (root's frames down to target) + (target's deepest below it)"""
    best = {}

    def down(fn, stack):
        if fn in best:
            return best[fn]
        if fn in stack or fn not in funcs:
            return (None, [])
        stack.add(fn)
        top = (None, [])
        if fn == target:
            below = deepest(funcs, target)
            top = (below[0] - funcs[target]["frame"], below[1][1:])
        else:
            for c in funcs[fn]["calls"]:
                d = down(c, stack)
                if d[0] is not None and (top[0] is None or d[0] > top[0]):
                    top = d
        stack.discard(fn)
        res = (None, []) if top[0] is None else (funcs[fn]["frame"] + top[0], [fn] + top[1])
        best[fn] = res
        return res
    return down(root, set())


def report(path, roots, targets):
    funcs = parse(path)
    out = {}
    for r in roots:
        if r in funcs:
            out[r] = deepest(funcs, r)
    if "oc_mod_boot" in funcs and "oc_mod_boot" in funcs["fm1_main"]["calls"]:     # the power-on boot: fm1_main calls oc_mod_boot itself
        d = deepest(funcs, "oc_mod_boot")
        out["boot: fm1_main > oc_mod_boot"] = (funcs["fm1_main"]["frame"] + d[0], ["fm1_main"] + d[1])
    for t in targets[1:]:
        if t in funcs:
            out["via " + t] = path_to(funcs, "fm1_main", t)
    return funcs, out


def main(argv):
    roots, targets = ["fm1_main"], ["oc_mod_boot", "ed_handle", "oc_mod_begin", "oc_mod_reload", "fb_mod_file"]   # boot; the installer (SysEx: begin, commit = reload); the card
    if len(argv) == 3 and argv[0] == "--compare":
        ra = report(argv[1], roots, targets)[1]
        rb = report(argv[2], roots, targets)[1]
        print(f"{'root':<32}{'before':>9}{'after':>9}{'delta':>8}   (bytes of the main loop's stack, user stack {USTACK} B)")
        for k in ra:
            a, b = ra[k][0], rb.get(k, (None, []))[0]
            if a is None or b is None:
                continue
            print(f"{k:<32}{a:>9}{b:>9}{b - a:>+8}   margin after {USTACK - b}")
        for k in rb:
            if k.startswith("via") or k == "fm1_main":
                print(f"\nafter, {k}: {' > '.join(rb[k][1][:14])}{' ...' if len(rb[k][1]) > 14 else ''}")
        return 0
    if len(argv) == 2 and argv[0] == "--check":
        out = report(argv[1], roots, targets)[1]
        worst = max(d for d, _ in out.values() if d is not None)
        print(f"stack_depth: deepest main-loop path {worst} B of {USTACK} B (margin {USTACK - worst} B, the least allowed 1024)  {'ok' if USTACK - worst >= 1024 else 'FAIL'}")
        return 0 if USTACK - worst >= 1024 else 1
    if argv:
        funcs, out = report(argv[0], argv[1:] or roots, targets)
        for k, (d, p) in out.items():
            print(f"{k:<28}{d if d is not None else '-':>8} B   {' > '.join(p[:12])}")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
