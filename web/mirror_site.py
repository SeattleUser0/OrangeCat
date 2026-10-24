#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Revisions:
# (Orange Cat 0.13.6) first version: copies the published stable site into a directory, so a branch run can publish a Nightly beside it
"""Copy the published stable site (GitHub Pages) into a directory.

  web/mirror_site.py URL OUT_DIR

The pages job of .github/workflows/build.yml deploys a whole site: what the new site does not hold is gone from Pages. A branch run that publishes a
Nightly channel (workflow input publish_nightly) must therefore start from the stable files as they are served now, not from a rebuild. This script
downloads them, the way the page names them:

  firmware/release.json, releases.json, rc.json     what the site offers (nightly.json is not copied: the new run replaces the Nightly channel)
  firmware/ every package, pack, module and set manifest those name
  webapp/editor/                                    the stable editor and its font and backup module
  webapp/manual/                                    the stable manual and its figures (when the site has one)
  webapp/installer/index.html, index.html           the installer page and the redirect (the installer page is made again by the run)

Only the release entry is required (a site without it is not a site to publish beside). Everything else is copied when the site has it. A download that
fails after three tries stops the script: a half copy must never be deployed.
"""
import json
import re
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path


def fetch(url, tries=3):
    last = None
    for n in range(tries):
        try:
            with urllib.request.urlopen(urllib.request.Request(url, headers={"Cache-Control": "no-cache"}), timeout=60) as r:
                return r.read()
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return None
            last = e
        except OSError as e:
            last = e
        time.sleep(2 * (n + 1))
    raise SystemExit(f"{url}: {last}")


def mirror(base, out):
    base, out = base.rstrip("/") + "/", Path(out)
    got = {}

    def take(rel, required=False):
        data = fetch(base + rel)
        if data is None:
            if required:
                raise SystemExit(f"{base}{rel}: not found (the site has no stable release to publish beside)")
            return None
        dest = out / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        got[rel] = len(data)
        return data

    release = json.loads(take("firmware/release.json", required=True))
    releases = take("firmware/releases.json")
    entries = json.loads(releases) if releases else [release]
    rc = take("firmware/rc.json")
    if rc:
        entries.append(json.loads(rc))
    for entry in entries:                                    # releases and the RC: the package, the pack and the modules
        names = list(entry.get("pkg", {}).values()) + ([entry["pak"]] if entry.get("pak") else []) + list(entry.get("ocm", []))
        for ref in names:
            take("firmware/" + Path(ref).name, required=True)
    for entry in entries:                                    # the sets for the card of each release (optional)
        v = entry.get("version", "")
        for choice in ("pack", "shapes", "prophet"):
            take(f"firmware/orangecat-{v}.{choice}.set.json")
    for doc in ("LICENSE", "LICENSING.md"):
        take("firmware/" + doc)
    for f in ("index.html", "fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        take("webapp/editor/" + f)
    page = take("webapp/manual/index.html")
    if page:
        for fig in sorted(set(re.findall(r'<img[^>]+src="([^"#?]+)"', page.decode("utf-8", "replace")))):
            if not fig.startswith(("http:", "https:", "/", "..")):
                take("webapp/manual/" + fig, required=True)
    take("webapp/installer/index.html")
    take("index.html")
    return got


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    files = mirror(*sys.argv[1:])
    print(f"mirrored {len(files)} files, {sum(files.values())} B, from {sys.argv[1]}")
    for rel, size in sorted(files.items()):
        print(f"  {size:>9} B  {rel}")
