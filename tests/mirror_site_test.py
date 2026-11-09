#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""web-r3 round 3 (Session D), audit I5 and I6.
I6: web/mirror_site.py copies the set manifest of every module choice an entry lists (derived from the entry's own .ocm names, as
    make_site.py names the sets), so a module added to the build (WHEEL) is not dropped at the next Nightly deploy.
I5: make_site.FX_NAMES (module id -> name) equals docs/orangecat/MODULE_IDS.md, and the DUB row equals DUBK_MODULE_ID in the firmware."""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "web"))
import make_site       # noqa: E402
import mirror_site     # noqa: E402

fails = []


def check(ok, what):
    print(("ok   " if ok else "FAIL ") + what)
    if not ok:
        fails.append(what)


entry = {"ocm": ["orangecat-shapes-0.16.0-abc.ocm", "orangecat-prophet-0.16.0-abc.ocm", "orangecat-wheel-0.16.0-abc.ocm",
                 {"url": "orangecat-dub-0.16.0-abc.ocm", "kind": 1, "module_id": 1}]}
check(mirror_site.set_choices(entry) == ["pack", "shapes", "prophet", "wheel", "dub"], "set choices: pack, then every module of the entry (wheel included), effect modules too")
check(mirror_site.set_choices({}) == ["pack"], "set choices: an entry with no module has the pack set only")

# the mirror takes exactly the sets make_site writes: the same rule on the same names
src = (ROOT / "web" / "make_site.py").read_text()
check('re.fullmatch(r"orangecat-([a-z0-9]+)-.*\\.ocm", fname)' in src, "make_site.py still names its sets by orangecat-NAME-*.ocm (the rule mirror_site.set_choices follows)")

# I5
ids = {}
for line in (ROOT / "docs" / "orangecat" / "MODULE_IDS.md").read_text().splitlines():
    m = re.match(r"\|\s*(\d+)\s*\|\s*([A-Z0-9 ]+?)\s*\|", line)
    if m:
        ids[int(m[1])] = m[2]
check(ids == make_site.FX_NAMES, f"MODULE_IDS.md == make_site.FX_NAMES ({ids})")
h = (ROOT / "firmware" / "src" / "dub_kern.h").read_text()
m = re.search(r"#define DUBK_MODULE_ID (\d+)u", h)
check(bool(m) and ids.get(int(m[1])) == "DUB TAPE", "the DUB row is DUBK_MODULE_ID of dub_kern.h")
# B2 I7: Orange Cat's site run is installer_only: no manual is published, and the pages that link a manual do not (nav of the installer, the editor)
os_src = (ROOT / "tools" / "orangecat_site.py").read_text()
check("installer_only=True" in os_src, "orangecat_site.py publishes with installer_only=True (no manual, no Melodee redirect page)")
for page in ("web/index_pkg.html", "web/editor.html"):
    t = (ROOT / page).read_text()
    check("../manual/" not in t and "webapp/manual" not in t, f"{page}: no link to an unpublished manual")
    if page.endswith("index_pkg.html"):            # (the editor's footer credits the upstream source: "based on")
        check("keremimo" not in t, f"{page}: no link to the Melodee repository")
sys.exit(1 if fails else 0)
