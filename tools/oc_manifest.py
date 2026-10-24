#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat 0.11.2: what the firmware of this build was compiled with, for the pages site to check the pack and the modules against.
"""  tools/oc_manifest.py OUT.json [GEN_DIR]

Writes {"pack_hash", "pack_len", "iface", "api_addr", "version"} from the headers the firmware was built with (build/gen/oc_pack_tables.h:
OC_PACK_HASH, OC_PACK_LEN; build/gen/fw_api_id.h: FW_API_ID, FW_API_ADDR) and the VERSION file. tools/orangecat_site.py (web/make_site.py check_set)
refuses a site whose pack has another hash or length, or whose module names another interface identifier, table address or pack hash.

Revisions:
  0.11.2: first version.
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


def main(out, gen=None):
    gen = Path(gen) if gen else ROOT / "build" / "gen"
    pack = (gen / "oc_pack_tables.h").read_text()
    api = (gen / "fw_api_id.h").read_text()
    info = {"version": (ROOT / "VERSION").read_text().strip(), "pack_hash": define(pack, "OC_PACK_HASH"), "pack_len": define(pack, "OC_PACK_LEN"),
            "iface": define(api, "FW_API_ID"), "api_addr": define(api, "FW_API_ADDR")}
    Path(out).write_text(json.dumps(info, indent=2) + "\n")
    print(f"manifest {out}: pack {info['pack_hash']:08x} / {info['pack_len']} B, interface {info['iface']:08x} at {info['api_addr']:#x}")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)
    main(*sys.argv[1:])
