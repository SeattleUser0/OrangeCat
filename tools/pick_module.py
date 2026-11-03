#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
"""(model picker, docs/candidates/model_picker.md) The packer: a module package (.ocm, format 2) for a pick of models, made from the master
package the build writes (build/orangecat-plaits-<version>.ocp: the whole engine linked once at 0, every model a region of the image, with its
relocation list and a manifest), without a linker. web/fm1pick.js is the same packer in JavaScript, for the installer page; web/test_pick.mjs and
tests/module_pick_test.py prove that both make, for a pick, the bytes tools/build.py --module plaits --pick makes with the real linker.

  tools/pick_module.py MASTER.ocp OUT.ocm ITEM [ITEM ...]      ITEM: a model's key (va, 6opa, wavtbl ...), a wave bank (wav0 ..) or an item id
  tools/pick_module.py MASTER.ocp --list                       the items, their regions and sizes

The master package:
  0   4  magic "OCPM"
  4   4  version 1
  8   4  bytes of the manifest (JSON, UTF-8; padded to a multiple of 4)
  12  4  bytes of the image (linked at 0, header page as linked: state_size, cost, length and the CRCs 0)
  16  4  relocation words (tools/ocflash.py reloc_word)
  20  4  CRC-32 of everything after this header
  24      the manifest, the image, the list

The manifest: name, engine, version word, area_lo, area_sectors, starts (the firmware's grid, the sector of each start), code_off (0x100),
part_head and poly (the glue's part layout), regions [{name, start, end}] in link order, items [{id, key, name, regions [...], state, big}]
(the models by their stable id, then the wave banks), rules {wavtbl: the item that needs a bank, banks: the bank items}, hdr {iface, api_addr,
api_version, api_size, pack_hash}.

The pick block, in the module's header page at PICK_AT (fw_api.h: the page is zero after the 60-byte header; the loader does not read it; the
format 2 package's per-start CRC covers it, so the page and the card verify it):
  0   4  magic "OPIK"
  4   1  version 1
  5   1  n: items picked
  6   1  items the engine has (the manifest's count: what a reader may list)
  7   1  0
  8   n  the item ids, ascending; padded with 0 to a multiple of 4
  +   4  CRC-32 of the block before it

The packing rule (what makes the bytes the linker's): a region is a unit's whole output (its code, tables and constant pools, placed by one
statement of the link script, followed by a 16-byte gap), and a unit reaches another only through absolute words (ABS32 or MOV_ABS32 sites, every
one in the relocation list, every one naming the other unit's symbol with addend 0: tools/build.py checks both). So, for a pick: keep the glue
and the picked items' regions in link order, each at the offset the dropped ones' bytes leave; a relocation site in a dropped region goes; a kept
site whose word points into a dropped region is set to 0 and its entry goes (the linker resolves that weak reference to 0 and keeps no entry);
every other kept site's word moves with its target. The header page is then filled as the build fills it: state_size (the largest state the
picked models need), the pick block, and the CRCs.
"""
import json
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ocflash  # noqa: E402

OCP_MAGIC, OCP_VERSION = 0x4D50434F, 1
PICK_MAGIC, PICK_VERSION, PICK_AT, PICK_MAX = 0x4B49504F, 1, 64, 32
STATE_AT, COST_AT = 24, 40                             # fw_mod_hdr_t words: state_size, cost
GAP = 16                                               # the link script's gap after each region (ALIGN(16) after `. += 16`)


def master_write(manifest, img0, words):
    js = json.dumps(manifest, separators=(",", ":"), sort_keys=True).encode()
    js += b"\0" * (-len(js) % 4)
    lst = struct.pack(f"<{len(words)}I", *words)
    body = js + bytes(img0) + lst
    return struct.pack("<IIIIII", OCP_MAGIC, OCP_VERSION, len(js), len(img0), len(words), zlib.crc32(body) & 0xFFFFFFFF) + body


def master_read(b):
    b = bytes(b)
    if len(b) < 24:
        raise ValueError("too short")
    magic, ver, jlen, ilen, nrel, crc = struct.unpack_from("<IIIIII", b, 0)
    if magic != OCP_MAGIC or ver != OCP_VERSION or len(b) != 24 + jlen + ilen + 4 * nrel or zlib.crc32(b[24:]) != crc:
        raise ValueError("not a master package")
    manifest = json.loads(b[24:24 + jlen].rstrip(b"\0").decode())
    img0 = b[24 + jlen:24 + jlen + ilen]
    words = list(struct.unpack_from(f"<{nrel}I", b, 24 + jlen + ilen))
    return manifest, img0, words


def item_of(manifest, x):
    """an item by id or key (a string of digits is an id)"""
    for it in manifest["items"]:
        if it["key"] == x or str(it["id"]) == str(x):
            return it
    raise ValueError(f"no item {x!r}")


def pick_items(manifest, names):
    return sorted({item_of(manifest, x)["id"] for x in names})


def validate(manifest, items):
    """the problems with a pick, in the page's words ([] when it is one the packer makes)"""
    out = []
    ids = {it["id"] for it in manifest["items"]}
    if not items:
        out.append("nothing picked")
    if len(items) != len(set(items)) or any(i not in ids for i in items):
        out.append("an item named twice or not the engine's")
    if len(items) > PICK_MAX:
        out.append(f"more than {PICK_MAX} items")
    r = manifest["rules"]
    banks = [i for i in items if i in r["banks"]]
    if r["wavtbl"] in items and not banks:
        out.append("WAVTBL needs a wave bank (A, B or C)")
    if banks and r["wavtbl"] not in items and r["terrain"] not in items:
        out.append("a wave bank is of use to WAVTBL or TERRAN only")
    return out


def regions_kept(manifest, items):
    keep = {"glue"}
    for it in manifest["items"]:
        if it["id"] in items:
            keep.update(it["regions"])
    return [r["name"] for r in manifest["regions"] if r["name"] in keep]


def state_size(manifest, items):
    """the module header's state_size for the pick: the glue's part head plus the largest model state the pick needs (eng_plaits.c pl_need)"""
    n = manifest["part_head"]
    for it in manifest["items"]:
        if it["id"] in items and it.get("state"):
            stride = (it["state"] + 7) & ~7
            n = max(n, manifest["part_head"] + (1 if it["big"] else manifest["poly"]) * stride)
    return n


def pick_block(items, ntotal):
    body = struct.pack("<IBBBB", PICK_MAGIC, PICK_VERSION, len(items), ntotal, 0) + bytes(sorted(items))
    body += b"\0" * (-len(body) % 4)
    return body + struct.pack("<I", zlib.crc32(body) & 0xFFFFFFFF)


def pick_block_read(page):
    """the items of a pick block at PICK_AT of a header page, or None when there is none (a module of another kind) / ValueError when it is damaged"""
    if page[PICK_AT:PICK_AT + 4] != struct.pack("<I", PICK_MAGIC):
        return None
    ver, n, total, z = struct.unpack_from("<BBBB", page, PICK_AT + 4)
    end = PICK_AT + 8 + ((n + 3) & ~3)
    if ver != PICK_VERSION or z or n > PICK_MAX or end + 4 > len(page) or zlib.crc32(page[PICK_AT:end]) != struct.unpack_from("<I", page, end)[0]:
        raise ValueError("pick block")
    items = list(page[PICK_AT + 8:PICK_AT + 8 + n])
    if items != sorted(set(items)):
        raise ValueError("pick block")
    return items, total


def fill_header(img, manifest, items, cost=0):
    """state_size, the pick block, cost: what the build writes into a pick's header page before the CRCs (the image is changed in place)"""
    struct.pack_into("<I", img, STATE_AT, state_size(manifest, items))
    struct.pack_into("<I", img, COST_AT, cost)
    blk = pick_block(items, len(manifest["items"]))
    img[PICK_AT:PICK_AT + len(blk)] = blk
    return img


def compact(manifest, img0, words, items):
    """the image and relocation list of the pick: (bytearray, [words]) — the rule in the module docstring"""
    regs = manifest["regions"]
    keep = set(regions_kept(manifest, items))
    code_off = manifest["code_off"]
    # the byte ranges: the header page, then each region with its gap (a region's span runs to the next region's start)
    spans = []                                         # (start, end, kept)
    for k, r in enumerate(regs):
        end = regs[k + 1]["start"] if k + 1 < len(regs) else len(img0)
        spans.append((r["start"], end, r["name"] in keep))
    assert spans[0][0] == code_off, "the first region starts at the code"

    def where(a):                                      # the span index of an address, -1 for the header page
        if a < code_off:
            return -1
        for k, (s, e, _) in enumerate(spans):
            if s <= a < e:
                return k
        raise ValueError(f"address {a:#x} outside the image")
    # new offsets
    delta, out, at = {}, bytearray(img0[:code_off]), code_off
    for k, (s, e, kept) in enumerate(spans):
        if kept:
            delta[k] = s - at
            out += img0[s:e]
            at += e - s
    new_words = []
    for w in words:
        off, kind = w & 0xFFFFFF, w >> 24
        k = where(off)
        if k >= 0 and not spans[k][2]:
            continue                                   # a site in a dropped region
        noff = off - (delta[k] if k >= 0 else 0)
        if kind == ocflash.RELOC_ABS32:
            v, = struct.unpack_from("<I", img0, off)
        else:
            lo, hi = struct.unpack_from("<HH", img0, off + 2)
            v = lo | hi << 16
        t = where(v)
        if t >= 0 and not spans[t][2]:
            nv = 0                                     # a word into a dropped region: the weak reference the linker left at 0
        else:
            nv = v - (delta[t] if t >= 0 else 0)
        if kind == ocflash.RELOC_ABS32:
            struct.pack_into("<I", out, noff, nv)
        else:
            struct.pack_into("<HH", out, noff + 2, nv & 0xFFFF, nv >> 16)
        if nv or t < 0 or spans[t][2]:
            new_words.append(ocflash.reloc_word(noff, kind))
    out += b"\0" * (-len(out) % 4)
    return out, new_words


def starts_for(manifest, sectors, area=None):
    """the starts of the firmware's grid the pick fits at (area: sectors to pretend the area has, for the byte proof of a pick too large for it)"""
    return [s for s in manifest["starts"] if s + sectors <= (area or manifest["area_sectors"])]


def pack(master, items, cost=0, area=None):
    """the .ocm (format 2) of a pick, from the master package's bytes; ValueError for a pick the packer refuses"""
    manifest, img0, words = master_read(master)
    items = sorted(items)
    bad = validate(manifest, items)
    if bad:
        raise ValueError("; ".join(bad))
    img, lst = compact(manifest, img0, words, items)
    fill_header(img, manifest, items, cost)
    struct.pack_into("<I", img, 48, len(img))          # length (the header's word 12)
    ocflash.module_fill(img)
    sectors = (len(img) + 0xFFF) // 0x1000
    starts = starts_for(manifest, sectors, area)
    if not starts:
        raise ValueError(f"{sectors} sectors fit no start of the area's {area or manifest['area_sectors']}")
    h = manifest["hdr"]
    variants = []
    for s in starts:
        got = ocflash.module_fill(ocflash.apply_relocs(img, lst, ocflash.xip(manifest["area_lo"] + s * 0x1000)))
        variants.append((s, zlib.crc32(bytes(got)) & 0xFFFFFFFF))
    return ocflash.ocm_write(manifest["name"], h["iface"], h["api_addr"], manifest["engine"], state_size(manifest, items), h["pack_hash"],
                             h["api_version"], h["api_size"], sectors, bytes(img), lst, variants), sectors, starts


def main(argv):
    if len(argv) < 2:
        raise SystemExit(__doc__)
    master = Path(argv[0]).read_bytes()
    manifest, img0, words = master_read(master)
    if argv[1] == "--list":
        size = {r["name"]: (manifest["regions"][k + 1]["start"] if k + 1 < len(manifest["regions"]) else len(img0)) - r["start"]
                for k, r in enumerate(manifest["regions"])}
        for it in manifest["items"]:
            print(f"{it['id']:3d} {it['key']:8s} {it['name']:12s} {'+'.join(it['regions']):24s} {sum(size[r] for r in it['regions']):7d} B"
                  + (f"  state {it['state']} B{' (one voice)' if it['big'] else ' a voice'}" if it.get("state") else ""))
        return
    area = None
    if argv[2:3] and argv[2].startswith("--area="):
        area = int(argv[2][7:])
        argv = argv[:2] + argv[3:]
    items = pick_items(manifest, argv[2:])
    ocm, sectors, starts = pack(master, items, 0, area)
    Path(argv[1]).write_bytes(ocm)
    print(f"{argv[1]}: {len(ocm)} B, items {items}, {sectors} sectors, starts {starts}, state {state_size(manifest, items)} B")


if __name__ == "__main__":
    main(sys.argv[1:])
