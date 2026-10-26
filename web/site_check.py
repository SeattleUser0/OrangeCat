#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Revisions:
# (Orange Cat 0.14.2) the web app: webapp/app/ in Standard and in the list, webapp/nightly-app/ with the Nightly
# (Orange Cat 0.13.6) first version: the dry run of the site build (file list and sizes of both channels) and the proof that Standard did not change
"""Look at a built site before it is deployed.

  web/site_check.py SITE --list                  the files and sizes of both channels, as text (the pages job puts it on the run page)
  web/site_check.py SITE --snapshot FILE         write the checksums of everything Standard is made of
  web/site_check.py SITE --compare FILE          exit 1 and name every file that differs from that snapshot

Standard is: firmware/release.json, releases.json, rc.json, every package, pack, module and set manifest those name, webapp/editor/, webapp/app/ (0.14.2), and webapp/manual/
without its nightly/ folder. The installer page (webapp/installer/index.html) is not in it: a Nightly publish makes the page again, with this
branch's page code (the stable files it offers are the same).
"""
import hashlib
import json
import sys
from pathlib import Path


def sha(path):
    """the file's SHA-256; a .json file by its content (keys sorted, white space gone), so a rewrite that says the same thing is the same file"""
    data = path.read_bytes()
    if path.suffix == ".json":
        try:
            data = json.dumps(json.loads(data), sort_keys=True, separators=(",", ":")).encode()
        except ValueError:
            pass
    return hashlib.sha256(data).hexdigest()


def json_of(site, name):
    p = Path(site) / "firmware" / name
    return json.loads(p.read_text()) if p.exists() else None


def release_names(entry):
    return list(entry.get("pkg", {}).values()) + ([entry["pak"]] if entry.get("pak") else []) + list(entry.get("ocm", []))


def stable_files(site):
    """the relative paths Standard is made of, in the site"""
    site = Path(site)
    files = set()
    entries = []
    for name in ("release.json", "releases.json", "rc.json"):
        data = json_of(site, name)
        if data is not None:
            files.add("firmware/" + name)
            entries += data if isinstance(data, list) else [data]
    for entry in entries:
        files.update("firmware/" + Path(x).name for x in release_names(entry))
        files.update(str(p.relative_to(site)) for p in (site / "firmware").glob(f"orangecat-{entry.get('version', '?')}.*.set.json"))
    for sub in ("webapp/editor", "webapp/manual", "webapp/app"):
        for p in (site / sub).rglob("*") if (site / sub).is_dir() else ():
            rel = p.relative_to(site)
            if p.is_file() and not str(rel).startswith("webapp/manual/nightly/"):
                files.add(str(rel))
    return sorted(files)


def snapshot(site):
    return {rel: sha(Path(site) / rel) for rel in stable_files(site) if (Path(site) / rel).is_file()}


def compare(site, baseline):
    now = snapshot(site)
    problems = [f"gone: {rel}" for rel in baseline if rel not in now]
    problems += [f"changed: {rel}" for rel, h in baseline.items() if rel in now and now[rel] != h]
    return problems


def listing(site):
    site = Path(site)
    lines = []

    def row(rel):
        p = site / rel
        lines.append(f"  {p.stat().st_size:>9} B  {rel}" if p.is_file() else f"  MISSING      {rel}")

    rel = json_of(site, "release.json")
    lines.append(f"STANDARD  v{rel['version']}  ({rel['product']})" if rel else "STANDARD  none")
    if rel:
        for ref in release_names(rel):
            row("firmware/" + Path(ref).name)
        for p in sorted((site / "firmware").glob(f"orangecat-{rel['version']}.*.set.json")):
            row(str(p.relative_to(site)))
    others = json_of(site, "releases.json") or []
    if len(others) > 1:
        lines.append("  older releases kept: " + ", ".join("v" + e["version"] for e in others[1:]))
    for sub in ("webapp/editor/index.html", "webapp/app/index.html", "webapp/manual/index.html"):
        row(sub) if (site / sub).exists() else lines.append(f"  (none)       {sub}")
    nig = json_of(site, "nightly.json")
    if nig:
        lines.append(f"NIGHTLY  {nig.get('branchName', 'next')} · {nig['commit'][:7]} · {nig['version']} · {nig['builtAt']}  ({nig['product']})")
        for ref in release_names(nig):
            row("firmware/" + Path(ref).name)
        for ref in nig.get("sets", []):
            row("firmware/" + ref)
        if (site / "webapp/nightly-app/index.html").exists():
            row("webapp/nightly-app/index.html")
    else:
        lines.append("NIGHTLY  none")
    inst = site / "webapp/installer/index.html"
    row("webapp/installer/index.html") if inst.exists() else lines.append("  MISSING      webapp/installer/index.html")
    return "\n".join(lines)


if __name__ == "__main__":
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    site, mode = sys.argv[1], sys.argv[2]
    if mode == "--list":
        text = listing(site)
        print(text)
        sys.exit(1 if "MISSING" in text else 0)
    elif mode == "--snapshot":
        Path(sys.argv[3]).write_text(json.dumps(snapshot(site), indent=1, sort_keys=True) + "\n")
        print(f"snapshot: {len(snapshot(site))} stable files")
    elif mode == "--compare":
        bad = compare(site, json.loads(Path(sys.argv[3]).read_text()))
        print("\n".join(bad) if bad else "Standard is unchanged")
        sys.exit(1 if bad else 0)
    else:
        sys.exit(__doc__)
