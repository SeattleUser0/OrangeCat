#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat, Release 3 (branch r3-format), Part 5: takes a web editor backup (JSON, "objects": id, size, crc, base64 data) apart for
# tests/oc_r3_oldgen.c:
#   r3_backup_extract.py BACKUP.json OUTDIR
# writes OUTDIR/records.bin (the arrangement objects 64..84 as store records: type u8, id u16, length u32, bytes: project records, pool chunks;
# object 84, the music's lists, is not a store record) when the backup has any, and OUTDIR/fbkk-<id>.bin for each object of a whole FBKK (27904
# bytes): id 0 the music loaded, ids 2..5 the slots 0..3. Prints the two kinds it found, one line each ("records N", "fbkk ID ...").
import base64, json, os, struct, sys

OCS_PROJECT, OCS_POOL = 1, 2        # firmware/src/oc_store.c
BANK_STORE_SIZE = 27904

def main():
    src, out = sys.argv[1], sys.argv[2]
    d = json.load(open(src))
    os.makedirs(out, exist_ok=True)
    recs, n = b"", 0
    for o in d["objects"]:
        data = base64.b64decode(o["data"]) if o.get("size") else b""
        i = o["id"]
        if 64 <= i <= 83 and data:
            typ, rid = (OCS_PROJECT, i - 64) if i < 68 else (OCS_POOL, i - 68)
            recs += struct.pack("<BHI", typ, rid, len(data)) + data
            n += 1
        elif i in (0, 2, 3, 4, 5) and len(data) == BANK_STORE_SIZE:
            open(os.path.join(out, "fbkk-%d.bin" % i), "wb").write(data)
            print("fbkk", i)
    if n:
        open(os.path.join(out, "records.bin"), "wb").write(recs)
    print("records", n)

if __name__ == "__main__":
    main()
