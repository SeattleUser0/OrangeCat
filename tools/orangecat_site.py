#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat: the GitHub Pages site of a build.
"""Make Orange Cat's GitHub Pages site from one built package:

  index.html                  redirect to the installer
  webapp/installer/index.html Melodee's web installer (web/index_pkg.html, made by web/make_site.py), offering this
                              build as its one release
  webapp/editor/index.html    the web editor (web/editor.html, its font and backup module)
  firmware/                   the package and the licence files (web/make_site.py)

  tools/orangecat_site.py PACKAGE VERSION OUT_DIR [--pak FILE --manifest FILE [--ocm FILE ...]]

  (0.11.2) With --pak the installer page installs the whole set: the firmware, then the pack, then the modules. The files are served as
  firmware/melodee-VERSION.fwsc, firmware/orangecat-VERSION.pak and firmware/orangecat-NAME-VERSION.ocm. The manifest (tools/oc_manifest.py) says what the
  firmware was built with; the pack's hash and the modules' interface identifier and pack hash are checked against it before anything is written.

The Pages job of .github/workflows/build.yml runs this after each build of main. The manual is not published: its
screenshots need tools/manual_shots.py, and the manual still describes Melodee.

Revisions:
# (Orange Cat 0.11.2) the installer page installs firmware, pack and module in one go
  0.1.0: first version. Uses web/make_site.py in installer-only mode, then adds the editor and the redirect itself.
  0.11.2: --pak, --ocm and --manifest: the pack and the modules of the build beside the firmware; the installer page offers them.
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WEB = HERE.parent / "web"
sys.path.insert(0, str(WEB))
import make_site  # noqa: E402  (web/make_site.py)


def main(pkg, version, out, pak=None, ocms=(), manifest=None):
    out = Path(out)
    make_site.main(pkg, version, out, installer_only=True, pak=pak, ocms=ocms, manifest=manifest)   # installer, package, set, licences (checks them)
    ed = out / "webapp" / "editor"
    ed.mkdir(parents=True, exist_ok=True)
    shutil.copy(WEB / "editor.html", ed / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (WEB / f).exists():
            shutil.copy(WEB / f, ed / f)
    (out / "index.html").write_text(
        '<!doctype html><meta charset="utf-8"><title>Orange Cat</title>'
        '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
        '<a href="webapp/installer/">Orange Cat installer</a> · <a href="webapp/editor/">Orange Cat editor</a>\n',
        encoding="utf-8")
    print(f"site: {out}: installer, editor, {version}")


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(usage=__doc__)
    ap.add_argument("package"); ap.add_argument("version"); ap.add_argument("out")
    ap.add_argument("--pak"); ap.add_argument("--manifest"); ap.add_argument("--ocm", action="append", default=[])
    a = ap.parse_args()
    main(a.package, a.version, a.out, a.pak, a.ocm, a.manifest)
