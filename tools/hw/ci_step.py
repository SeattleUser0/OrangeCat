#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Run the build or the test step of .github/workflows/build.yml on a Linux PC, as CI runs it.

  tools/hw/ci_step.py build [--checkout DIR] [--switch NAME=VALUE ...]
  tools/hw/ci_step.py test  [--checkout DIR] [--switch NAME=VALUE ...] [--parallel N]

The step's text is read from the checkout's own workflow (so an old commit builds with the step it was built with in CI),
the lines that install packages (apt-get, pip) are dropped (tools/hw/setup_linux.sh installed them once), and the rest
runs in bash with what fm1-build@v1 gives the step: JIELI_TOOLCHAIN, AC79_SDK, GITHUB_SHA (the checkout's HEAD),
GITHUB_REF_TYPE=branch. A --switch is exported right after tools/orangecat_engines.sh is sourced, so it wins over the
file (the A/B builds: OC_PACK_BUILTIN=1 and the like). --parallel replaces the step's PARALLEL=2 (the test step's job
count; the build is not changed by it). See BUILDING.md, "Building on a Linux PC".

The date: the image carries the compiler's __DATE__ (Settings > ABOUT, ui_menu.c), so a build is byte-identical to CI's only on
the same UTC date. The step always runs with TZ=UTC; --date "YYYY-MM-DD HH:MM:SS" (UTC, the start of the run's Build step on its
run page) runs it under libfaketime starting at that moment, to rebuild an older CI build exactly.

Revisions:
  (Orange Cat 0.13.7) first version (branch linux-hw).
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

TOOLS = Path(os.environ.get("FM1_TOOLS", Path.home() / "fm1-toolchain"))
SDK_COMMIT = "d179b4484759423312073f5fbb232501aa491047"
DROP = re.compile(r"^\s*(sudo apt-get|python3 -m pip install)")


def step_text(workflow, name):
    """the literal block `name: |` of the build job's `with:` (the lines indented deeper than the key)"""
    lines = workflow.splitlines()
    for i, line in enumerate(lines):
        m = re.match(r"^(\s*)" + name + r": \|\s*$", line)
        if m:
            ind, body = len(m[1]), []
            for x in lines[i + 1:]:
                if x.strip() and len(x) - len(x.lstrip()) <= ind:
                    break
                body.append(x)
            pad = min(len(x) - len(x.lstrip()) for x in body if x.strip())
            return "\n".join(x[pad:] for x in body)
    raise SystemExit(f"ci_step: no '{name}: |' block in the workflow")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=("build", "test"))
    ap.add_argument("--checkout", default=Path(__file__).resolve().parents[2], type=Path)
    ap.add_argument("--switch", action="append", default=[], metavar="NAME=VALUE")
    ap.add_argument("--parallel", type=int)
    ap.add_argument("--date", metavar="'YYYY-MM-DD HH:MM:SS'", help="UTC start time for the build (libfaketime)")
    a = ap.parse_args()
    co = a.checkout.resolve()
    text = step_text((co / ".github/workflows/build.yml").read_text(), a.step)
    out = []
    for line in text.splitlines():
        if DROP.match(line):
            out.append("# (ci_step: dropped) " + line.strip())
            continue
        if a.parallel and "PARALLEL=2" in line:
            line = line.replace("PARALLEL=2", f"PARALLEL={a.parallel}")
        out.append(line)
        if re.match(r"^\s*\. tools/orangecat_engines\.sh", line) and a.switch:
            for s in a.switch:
                if not re.fullmatch(r"[A-Z0-9_]+=[A-Za-z0-9_.-]*", s):
                    raise SystemExit(f"ci_step: bad --switch {s!r}")
                out.append(f"export {s}   # (ci_step --switch)")
    env = dict(os.environ)
    env.setdefault("JIELI_TOOLCHAIN", str(TOOLS / "jieli/toolchain"))
    env.setdefault("AC79_SDK", str(TOOLS / "fw-AC79_AIoT_SDK"))
    env["PATH"] = f"{TOOLS / 'venv/bin'}:{env['PATH']}"
    env["GITHUB_SHA"] = subprocess.run(["git", "-C", co, "rev-parse", "HEAD"], capture_output=True, text=True, check=True).stdout.strip()
    env["GITHUB_REF_TYPE"], env["GITHUB_REF_NAME"] = "branch", "local"
    env["TZ"] = "UTC"
    cmd = ["bash"]
    if a.date:
        if not re.fullmatch(r"\d{4}-\d\d-\d\d \d\d:\d\d:\d\d", a.date):
            raise SystemExit("ci_step: --date wants 'YYYY-MM-DD HH:MM:SS'")
        ft = shutil.which("faketime")
        if not ft:
            raise SystemExit("ci_step: --date needs libfaketime's faketime (brew install libfaketime, or apt install faketime)")
        cmd = [ft, "-f", "@" + a.date, "bash"]
    sdk = subprocess.run(["git", "-C", env["AC79_SDK"], "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
    if sdk != SDK_COMMIT:
        raise SystemExit(f"ci_step: the SDK in {env['AC79_SDK']} is at {sdk or '?'}, CI's is {SDK_COMMIT}")
    script = co / f".ci_step_{a.step}.sh"
    script.write_text("\n".join(out) + "\n")
    t0 = time.monotonic()
    try:
        rc = subprocess.run([*cmd, script], cwd=co, env=env).returncode
    finally:
        script.unlink()
    print(f"ci_step: {a.step} of {env['GITHUB_SHA'][:7]} {'passed' if rc == 0 else 'FAILED'} in {time.monotonic() - t0:.0f} s"
          + (f" (switches: {' '.join(a.switch)})" if a.switch else "") + (f" (date {a.date} UTC)" if a.date else ""), flush=True)
    return rc


if __name__ == "__main__":
    sys.exit(main())
