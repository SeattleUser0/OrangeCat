#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""The host suite's profile and its dependency database (tests/run_tests.sh, tests/cc_wrap.sh).

  tests/suite_profile.py LOGDIR [--report FILE] [--top N]       the profile of a run: every job's wall, compile and run
                                                                 seconds, the compiles of the firmware sources
  tests/suite_profile.py LOGDIR --deps FILE                      merge the run's inputs into the dependency database
  tests/suite_profile.py --affected REF --deps FILE              the jobs whose inputs changed since REF (run_tests.sh --changed)

What the runner leaves in LOGDIR (build/host/log) for job n: n.key (the job's name: a test's title or a job function's name),
n.time (start and end, ns), n.log, n.cc (one line per compiler call: start, end, status, arguments; tests/cc_wrap.sh),
n.d.<pid> (the depfile of each compile of one .c file), n.run (one line per test run: start, end, the step's title).

The dependency database (build/host/deps.txt) is "key<TAB>input" lines: every file a job's compiles read (the test's
source, the firmware sources and headers it includes, the generated headers). A job without compiles (a python step, a
node test) has no line and so always runs under --changed, as does a job the database has never seen.

--affected REF prints the keys to run, one per line, or ALL alone when the whole suite must run: no database, the runner
or this tool changed, or a changed file under tests/, tools/, web/, assets/ or firmware/ that is not C (a golden file, a
fixture, a script): the include graph does not know who reads it. Changes elsewhere (docs, the workflow) affect nothing.
The runner adds the golden and size checks (the regression jobs, the main stack check) to whatever is printed.
"""
import argparse
import os
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

SRC = Path(__file__).resolve().parents[1]
SUITE_FILES = ("tests/run_tests.sh", "tests/cc_wrap.sh", "tests/suite_profile.py")
CODE = (".c", ".h", ".S", ".inc")


def depfile_inputs(text):
    text = text.replace("\\\n", " ")
    _, _, rest = text.partition(":")
    out = set()
    for p in rest.split():
        if p.startswith("/"):                      # system headers
            continue
        out.add(os.path.normpath(p))
    return out


def read_run(logdir):
    """every job of a run: dict n -> {key, wall, cc (list of (secs, args)), runs (list of (secs, title)), inputs (set)}"""
    jobs = {}
    for kf in sorted(logdir.glob("*.key"), key=lambda p: int(p.stem)):
        n = int(kf.stem)
        j = {"key": kf.read_text().rstrip("\n"), "wall": None, "cc": [], "runs": [], "inputs": set(), "done": False}
        tf = logdir / f"{n}.time"
        if tf.exists():
            a, b = tf.read_text().split()[:2]
            j["wall"] = (int(b) - int(a)) / 1e9
            j["done"] = True
        cf = logdir / f"{n}.cc"
        if cf.exists():
            for line in cf.read_text().splitlines():
                parts = line.split(" ", 3)
                if len(parts) < 3:
                    continue
                j["cc"].append(((int(parts[1]) - int(parts[0])) / 1e9, parts[3] if len(parts) > 3 else ""))
        rf = logdir / f"{n}.run"
        if rf.exists():
            for line in rf.read_text().splitlines():
                parts = line.split(" ", 2)
                if len(parts) < 2:
                    continue
                j["runs"].append(((int(parts[1]) - int(parts[0])) / 1e9, parts[2] if len(parts) > 2 else ""))
        for df in logdir.glob(f"{n}.d.*"):
            j["inputs"] |= depfile_inputs(df.read_text())
        jobs[n] = j
    return jobs


def fmt(s):
    return f"{s:8.1f}"


def report(jobs, top, out):
    lines = []
    done = [j for j in jobs.values() if j["done"]]
    wall = sum(j["wall"] for j in done)
    cc = sum(s for j in done for s, _ in j["cc"])
    run = sum(s for j in done for s, _ in j["runs"])
    ncc = sum(len(j["cc"]) for j in done)
    lines.append(f"jobs {len(done)} (of {len(jobs)} started)   job seconds {wall:.0f}   compile {cc:.0f} s in {ncc} compiler calls   "
                 f"test runs {run:.0f} s   (sums over jobs; the wall clock divides by PARALLEL)")
    # the firmware sources: how many compiles include each
    per_file = defaultdict(int)
    fw_compiles = 0
    for j in done:
        srcs = [p for p in j["inputs"] if p.startswith("firmware/") and p.endswith(".c")]
        if srcs:
            # every compile of the job includes about the same set; count the job's compiles for each source it includes
            k = max(1, sum(1 for _, a in j["cc"] if ".c" in a))
            fw_compiles += k
            for p in set(srcs):
                per_file[p] += k
    lines.append(f"firmware sources compiled: {fw_compiles} compiles include firmware/*.c; "
                 f"{len(per_file)} distinct files. The most compiled:")
    for p, k in sorted(per_file.items(), key=lambda x: -x[1])[:12]:
        lines.append(f"  {k:5d}  {p}")
    lines.append("")
    lines.append(f"top {top} jobs by wall time (seconds: wall, compile, run; compiles; the job):")
    for j in sorted(done, key=lambda j: -j["wall"])[:top]:
        c = sum(s for s, _ in j["cc"])
        r = sum(s for s, _ in j["runs"])
        lines.append(f"{fmt(j['wall'])} {fmt(c)} {fmt(r)}  {len(j['cc']):3d}  {j['key'][:110]}")
    lines.append("")
    lines.append("every job (seconds: wall, compile, run; compiles; the job), in the runner's order:")
    for n in sorted(jobs):
        j = jobs[n]
        if not j["done"]:
            lines.append(f"{'':8} {'':8} {'':8}  {'':3}  {j['key'][:110]}  (not finished)")
            continue
        c = sum(s for s, _ in j["cc"])
        r = sum(s for s, _ in j["runs"])
        lines.append(f"{fmt(j['wall'])} {fmt(c)} {fmt(r)}  {len(j['cc']):3d}  {j['key'][:110]}")
    text = "\n".join(lines) + "\n"
    if out:
        Path(out).write_text(text)
    return text


def read_deps(path):
    deps = defaultdict(set)
    if path.exists():
        for line in path.read_text().splitlines():
            k, _, f = line.partition("\t")
            if k and f:
                deps[k].add(f)
    return deps


def write_deps(path, jobs):
    """the inputs of the jobs that ran (and compiled something) replace their entries; the others stay"""
    deps = read_deps(path)
    for j in jobs.values():
        if j["done"] and j["inputs"]:
            deps[j["key"]] = set(j["inputs"])
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w") as f:
        for k in sorted(deps):
            for p in sorted(deps[k]):
                f.write(f"{k}\t{p}\n")
    return len(deps)


def changed_files(ref):
    r = subprocess.run(["git", "diff", "--name-only", ref, "--"], cwd=SRC, capture_output=True, text=True)
    if r.returncode:
        sys.stderr.write(r.stderr)
        raise SystemExit(f"suite_profile: git diff {ref} failed")
    files = set(r.stdout.split())
    u = subprocess.run(["git", "ls-files", "--others", "--exclude-standard"], cwd=SRC, capture_output=True, text=True)
    files |= set(u.stdout.split())
    return files


def affected(ref, deps_path):
    files = changed_files(ref)
    deps = read_deps(deps_path)
    if not deps:
        return None, "no dependency database (run the full suite once)", files
    for f in files:
        if f in SUITE_FILES:
            return None, f"{f} changed", files
        if not f.endswith(CODE) and f.split("/")[0] in ("tests", "tools", "web", "assets", "firmware"):
            return None, f"{f} changed (not C: the include graph does not know who reads it)", files
    code = {os.path.normpath(f) for f in files if f.endswith(CODE)}
    keys = sorted(k for k, ins in deps.items() if ins & code)
    return keys, None, files


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("logdir", nargs="?", type=Path, help="the run's log directory (build/host/log)")
    ap.add_argument("--report", metavar="FILE", help="write the profile here (and print its head)")
    ap.add_argument("--top", type=int, default=20)
    ap.add_argument("--deps", type=Path, metavar="FILE", help="the dependency database (build/host/deps.txt)")
    ap.add_argument("--affected", metavar="REF", help="print the jobs whose inputs changed since REF (git), or ALL")
    ap.add_argument("--quiet", action="store_true", help="with --report: print the summary lines only")
    a = ap.parse_args()
    if a.affected:
        if not a.deps:
            ap.error("--affected needs --deps FILE")
        keys, why, files = affected(a.affected, a.deps)
        if keys is None:
            print("ALL")
            print(f"suite_profile: every job runs: {why}", file=sys.stderr)
            return 0
        print(f"suite_profile: {len(files)} files changed since {a.affected}, {len(keys)} jobs with changed inputs "
              f"(plus the golden and size checks and the jobs the database does not know)", file=sys.stderr)
        for k in keys:
            print(k)
        return 0
    if not a.logdir:
        ap.error("LOGDIR, or --affected REF")
    jobs = read_run(a.logdir)
    if a.deps:
        n = write_deps(a.deps, jobs)
        print(f"suite_profile: {a.deps}: the inputs of {n} jobs")
    if a.report or not a.deps:
        text = report(jobs, a.top, a.report)
        if a.quiet:
            print("\n".join(text.splitlines()[:2]))
        else:
            sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
