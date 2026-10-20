#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat: the GitHub Pages site of a build.
"""Make Orange Cat's GitHub Pages site from one built package:

  index.html                  redirect to the installer
  webapp/installer/index.html Melodee's web installer (web/index_pkg.html, made by web/make_site.py), offering this
                              build as its one release
  webapp/editor/index.html    the web editor (web/editor.html, its font and backup module)
  firmware/                   the package and the licence files (web/make_site.py)

  tools/orangecat_site.py PACKAGE VERSION OUT_DIR

The Pages job of .github/workflows/build.yml runs this after each build of main. The manual is not published: its
screenshots need tools/manual_shots.py, and the manual still describes Melodee.

Revisions:
  0.1.0: first version. Uses web/make_site.py in installer-only mode, then adds the editor and the redirect itself.
"""
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
WEB = HERE.parent / "web"
sys.path.insert(0, str(WEB))
import make_site  # noqa: E402  (web/make_site.py)


def main(pkg, version, out):
    out = Path(out)
    make_site.main(pkg, version, out, installer_only=True)      # installer, package, licences (checks the package)
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
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
