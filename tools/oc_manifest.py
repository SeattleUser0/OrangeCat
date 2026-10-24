#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat 0.11.2: what the firmware of this build was compiled with, for the pages site to check the pack and the modules against.
"""  tools/oc_manifest.py OUT.json [GEN_DIR]
  tools/oc_manifest.py --set OUT.json [--gen GEN_DIR] --pak NAME [--fwsc NAME] [--ocm NAME ...]

Writes {"pack_hash", "pack_len", "iface", "api_addr", "version"} from the headers the firmware was built with (build/gen/oc_pack_tables.h:
OC_PACK_HASH, OC_PACK_LEN; build/gen/fw_api_id.h: FW_API_ID, FW_API_ADDR) and the VERSION file. tools/orangecat_site.py (web/make_site.py check_set)
refuses a site whose pack has another hash or length, or whose module names another interface identifier, table address or pack hash.

(0.13.3) --set writes a set manifest for the card (docs/orangecat/SET_FORMAT.md): the same five numbers and "version", and "files": the names of the pack, of the
firmware package if given, and of the modules, as they are in the folder the manifest is copied to. The unit's file browser lists *.set.json files and
"Install set" installs what they name. The names are checked here (no path, no odd characters) the way the unit checks them.

Revisions:
  0.11.2: first version.
  0.13.3: --set, the set manifest.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def define(text, name):
    m = re.search(rf"#define\s+{name}\s+(0x[0-9A-Fa-f]+|\d+)u?\b", text)
    if not m:
        raise SystemExit(f"{name} is not in the generated headers")
    return int(m.group(1), 0)


def info_of(gen=None, version=None):
    gen = Path(gen) if gen else ROOT / "build" / "gen"
    pack = (gen / "oc_pack_tables.h").read_text()
    api = (gen / "fw_api_id.h").read_text()
    return {"version": version or (ROOT / "VERSION").read_text().strip(), "pack_hash": define(pack, "OC_PACK_HASH"), "pack_len": define(pack, "OC_PACK_LEN"),
            "iface": define(api, "FW_API_ID"), "api_addr": define(api, "FW_API_ADDR")}


def plain_name(name):
    """a file name as the unit reads it: 1 to 47 characters of printable ASCII, none of / \\ : * ? < > | ", not starting with a dot"""
    if not name or len(name) > 47 or name[0] == "." or any(not 32 <= ord(c) <= 126 or c in '/\\:*?<>|"' for c in name):
        raise SystemExit(f"{name!r}: not a file name the unit's browser can read (1 to 47 plain characters, no path)")
    return name


def set_manifest(info, pak, fwsc=None, ocms=()):
    """the manifest of a set: info with the names of its files (pak required; fwsc and ocm optional; at most two modules)"""
    if len(ocms) > 2:
        raise SystemExit("a set names two modules at most")
    files = {"pak": plain_name(pak)}
    if fwsc:
        files["fwsc"] = plain_name(fwsc)
    if ocms:
        files["ocm"] = [plain_name(o) for o in ocms]
    return {**info, "files": files}


def main(out, gen=None):
    info = info_of(gen)
    Path(out).write_text(json.dumps(info, indent=2) + "\n")
    print(f"manifest {out}: pack {info['pack_hash']:08x} / {info['pack_len']} B, interface {info['iface']:08x} at {info['api_addr']:#x}")


def main_set(out, gen, pak, fwsc, ocms):
    man = set_manifest(info_of(gen), pak, fwsc, ocms)
    Path(out).write_text(json.dumps(man, indent=2) + "\n")
    print(f"set manifest {out}: {man['files']}")


if __name__ == "__main__":
    if sys.argv[1:2] == ["--set"]:
        import argparse
        ap = argparse.ArgumentParser(prog="oc_manifest.py --set")
        ap.add_argument("out")
        ap.add_argument("--gen")
        ap.add_argument("--pak", required=True)
        ap.add_argument("--fwsc")
        ap.add_argument("--ocm", action="append", default=[])
        a = ap.parse_args(sys.argv[2:])
        main_set(a.out, a.gen, a.pak, a.fwsc, a.ocm)
    elif len(sys.argv) in (2, 3):
        main(*sys.argv[1:])
    else:
        sys.exit(__doc__)
