#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Revisions:
# (Orange Cat 0.13.6) first version: copies the published stable site into a directory, so a branch run can publish a Nightly beside it
# (Orange Cat 0.14.2) the web app (webapp/app/) by its files.json
# (Orange Cat 0.15.0) --only-nightly: the Nightly channel alone, so a Standard deploy carries it forward (a push to main had wiped it)
# (Orange Cat 0.13.8) --nightly: the live Nightly as well (firmware/nightly.json and its files), for the check after a deploy only. Without it the
#   check could never find the commit it had just deployed (every Nightly publish ended red at "Check the live site": runs 40132493524, 40212959451)
"""Copy the published stable site (GitHub Pages) into a directory.

  web/mirror_site.py [--nightly] URL OUT_DIR
  web/mirror_site.py --only-nightly URL OUT_DIR

--only-nightly (rev 24) copies nothing but the live Nightly channel: firmware/nightly.json, its package, pack, modules and set manifests, the Nightly's
web app (webapp/nightly-app/, by its files.json), its editor (webapp/nightly-editor/) and its guide (webapp/manual/nightly/, when there is one). The pages job
runs it before it builds a Standard site, so a push to main carries the Nightly forward (web/make_site.py keeps a channel it finds in the site). A site
without a Nightly copies nothing and succeeds.

--nightly also copies the live Nightly (firmware/nightly.json, its package, pack, modules and set manifests): what the pages job's check after the deploy
reads to see that the site serves this run's commit. The copy a run publishes beside (before the deploy) never takes it.

The pages job of .github/workflows/build.yml deploys a whole site: what the new site does not hold is gone from Pages. A branch run that publishes a
Nightly channel (workflow input publish_nightly) must therefore start from the stable files as they are served now, not from a rebuild. This script
downloads them, the way the page names them:

  firmware/release.json, releases.json, rc.json     what the site offers (nightly.json is not copied: the new run replaces the Nightly channel)
  firmware/ every package, pack, module and set manifest those name
  webapp/editor/                                    the stable editor and its font and backup module
  webapp/app/                                       (0.14.2) the stable web app, by its files.json
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


def mirror(base, out, nightly=False):
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
    listed = take("webapp/app/files.json")                   # (0.14.2) the web app, by its own list (tools/orangecat_site.py copy_app)
    if listed:
        for f in json.loads(listed):
            if "/" in f.replace("lib/", "", 1) or f.startswith(("/", ".")):
                raise SystemExit(f"webapp/app/files.json: {f}: not a file of the app")
            take("webapp/app/" + f, required=True)
    page = take("webapp/manual/index.html")
    if page:
        for fig in sorted(set(re.findall(r'<img[^>]+src="([^"#?]+)"', page.decode("utf-8", "replace")))):
            if not fig.startswith(("http:", "https:", "/", "..")):
                take("webapp/manual/" + fig, required=True)
    take("webapp/installer/index.html")
    take("index.html")
    if nightly:                                              # (0.13.8) the live Nightly, for the check after a deploy
        data = take("firmware/nightly.json")
        if data:
            entry = json.loads(data)
            for ref in list(entry.get("pkg", {}).values()) + ([entry["pak"]] if entry.get("pak") else []) + list(entry.get("ocm", [])):
                take("firmware/" + Path(ref).name, required=True)
            for name in entry.get("sets", []):
                take("firmware/" + Path(name).name)
    return got


def mirror_nightly(base, out):
    """(rev 24) the live Nightly channel into out, whole or not at all; {} when the site has none"""
    base, out = base.rstrip("/") + "/", Path(out)
    got = {}

    def take(rel, required=False):
        data = fetch(base + rel)
        if data is None:
            if required:
                raise SystemExit(f"{base}{rel}: not found (the Nightly names it)")
            return None
        dest = out / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_bytes(data)
        got[rel] = len(data)
        return data

    data = take("firmware/nightly.json")
    if not data:
        return got
    entry = json.loads(data)
    for ref in list(entry.get("pkg", {}).values()) + ([entry["pak"]] if entry.get("pak") else []) + list(entry.get("ocm", [])):
        take("firmware/" + Path(ref).name, required=True)
    for name in entry.get("sets", []):
        take("firmware/" + Path(name).name)
    for choice in ("pack", "shapes", "prophet"):
        take(f"firmware/orangecat-nightly.{choice}.set.json")
    listed = take("webapp/nightly-app/files.json")
    if listed:
        for f in json.loads(listed):
            if "/" in f.replace("lib/", "", 1) or f.startswith(("/", ".")):
                raise SystemExit(f"webapp/nightly-app/files.json: {f}: not a file of the app")
            take("webapp/nightly-app/" + f, required=True)
    for f in ("index.html", "fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        take("webapp/nightly-editor/" + f)
    page = take("webapp/manual/nightly/index.html")
    if page:
        for fig in sorted(set(re.findall(r'<img[^>]+src="([^"#?]+)"', page.decode("utf-8", "replace")))):
            if not fig.startswith(("http:", "https:", "/", "..")):
                take("webapp/manual/nightly/" + fig, required=True)
    return got


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--only-nightly" in args:
        rest = [a for a in args if a != "--only-nightly"]
        if len(rest) != 2:
            sys.exit(__doc__)
        files = mirror_nightly(*rest)
        print(f"mirrored the Nightly: {len(files)} files, {sum(files.values())} B, from {rest[0]}")
        sys.exit(0)
    with_nightly = "--nightly" in args
    args = [a for a in args if a != "--nightly"]
    if len(args) != 2:
        sys.exit(__doc__)
    files = mirror(*args, nightly=with_nightly)
    print(f"mirrored {len(files)} files, {sum(files.values())} B, from {args[0]}")
    for rel, size in sorted(files.items()):
        print(f"  {size:>9} B  {rel}")
