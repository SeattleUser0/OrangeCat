# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.10.1: the two flash facts the pack generator and the module build share, in one copy each.

  xip(off)            the CPU address flash offset off reads at: firmware/hal/fm1_xip.h FM1_XIP(off) =
                      0x02000000 + off - 0x4000 (the loader header takes the first 0x4000). Found wrong in 0.10.0
                      step 4 (the module was linked for 0x020C8000 where flash 0xC8000 reads at 0x020C4000);
                      gen_pack.py and build.py --module both call this, and tests/ocflash_test.py compares it with
                      the header's own text, so the mistake cannot come back in either place.
  tail_hit(image, n)  the update loader's record rule (firmware/loader/ldr_core.c, the end of ldr_apply): the loader
                      erases any data sector whose last 256 bytes, taken as 80 bytes, read as one of its update
                      records: bytes 6..7 equal 0x5441, and the first two bytes are non-zero and equal the CRC-16
                      (ota_crc16: poly 0x1021, init 0) of bytes 2..79. The loader scans the sectors from 0x93000
                      to 0xFC000, which covers the pack, the library group and the module area. Anything kept there
                      that is read in place (the pack, a module) must not have such a sector tail.
                      firmware/src/oc_pack.c (pk_record_like, pk_tails) and oc_module.c (oc_mod_tails) are the
                      device's copies of the same rule.

  apply_relocs(image, words, base)   (0.14.1) a relocatable module image (linked at 0) for the address `base`: the relocation list's ABS32 words and
                      MOV32 constants get base added (tools/module_relocs.py makes the list; docs/orangecat/ENGINE_LOADING_PLAN.md, "One image and a
                      relocation list"). module_fill then writes the header's two CRCs as tools/build.py does after a link, and ocm_images reads a
                      package of either format into the image each start would get. web/fm1oc.js (ocmRelocate) and firmware/src/card_install.c
                      (fb_rel_rd) are the page's and the card's copies of apply_relocs; tests/ocflash_test.py and web/test_web.mjs hold them to it.

Revisions:
  0.10.1: first version (tail_hit and crc16 moved here from gen_pack.py; xip is new).
  0.14.1: apply_relocs, module_fill, ocm_images (the relocatable module package, OCM format 2).
"""
import struct
import zlib
LOADER_OFFSET = 0x4000
XIP_WINDOW = 0x02000000
SECTOR = 0x1000
TAIL_AT = SECTOR - 0x100            # a sector's last 256 bytes start here
TAIL_BYTES = 80                     # the loader reads this many of them
RECORD_TAG = 0x5441


def xip(off):
    """the CPU address flash offset `off` reads at (fm1_xip.h FM1_XIP)"""
    return XIP_WINDOW + off - LOADER_OFFSET


def crc16(data, c=0):
    """loader/ldr_core.c ota_crc16: poly 0x1021, init 0"""
    for b in data:
        c ^= b << 8
        for _ in range(8):
            c = ((c << 1) ^ 0x1021) & 0xFFFF if c & 0x8000 else (c << 1) & 0xFFFF
    return c


def record_like(r):
    """the loader's rule on the 80 bytes at a sector's tail: True when they would read as an update record"""
    w0, w6 = r[0] | r[1] << 8, r[6] | r[7] << 8
    return w6 == RECORD_TAG and w0 != 0 and w0 == crc16(r[2:TAIL_BYTES])


def tail_hit(image, sectors=None):
    """The rule on each sector's tail of `image` laid out from a sector boundary and padded with erased flash (0xFF)
    to `sectors` sectors (default: the sectors the image covers): the first sector index that fails, or -1."""
    n = sectors if sectors is not None else (len(image) + SECTOR - 1) // SECTOR
    img = bytes(image) + b"\xff" * max(0, n * SECTOR - len(image))
    for s in range(n):
        if record_like(img[s * SECTOR + TAIL_AT:][:TAIL_BYTES]):
            return s
    return -1


# ---- (0.14.1) relocatable modules: one image linked at 0 and a relocation list (OCM format 2) ----
# A list entry is a 32-bit word: bits 0..23 the byte offset of the site in the image (the header page is byte 0), bits 24..31 its kind:
#   RELOC_ABS32  a little-endian 32-bit word holding an address: word += base
#   RELOC_MOV32  a MOV_ABS32 instruction pair at offset (8 bytes): the 32-bit constant it loads, the low half at offset + 2 and the high half at
#                offset + 4 (little-endian halfwords; the halfwords at offset and offset + 6 are the opcodes), += base with the carry
# Entries are in strictly increasing order of offset, each site inside the image, at most RELOC_MAX of them; anything else refuses the list.
RELOC_ABS32, RELOC_MOV32 = 1, 2
RELOC_MAX = 2048
OCM_MAGIC, OCM_RELOCATABLE = 0x464D434F, 0xFFFFFFFF
MOD_CODE_OFF, MOD_CRC_AT, MOD_HCRC_AT = 0x100, 52, 56          # fw_mod_hdr_t (firmware/src/fw_api.h): the code's CRC-32, the header's (of bytes 0..55)


def reloc_word(off, kind):
    return (kind << 24) | off


def apply_relocs(image, words, base):
    """the image linked at 0, relocated to `base` (a new bytearray); ValueError for a list the device would refuse"""
    img = bytearray(image)
    if len(words) > RELOC_MAX:
        raise ValueError(f"{len(words)} relocations: more than {RELOC_MAX}")
    prev = -1
    for w in words:
        off, kind = w & 0xFFFFFF, w >> 24
        if off <= prev:
            raise ValueError(f"relocation at {off:#x}: not after {prev:#x}")
        prev = off
        if kind == RELOC_ABS32 and off + 4 <= len(img):
            v, = struct.unpack_from("<I", img, off)
            struct.pack_into("<I", img, off, (v + base) & 0xFFFFFFFF)
        elif kind == RELOC_MOV32 and off + 8 <= len(img):
            lo, hi = struct.unpack_from("<HH", img, off + 2)
            v = ((lo | hi << 16) + base) & 0xFFFFFFFF
            struct.pack_into("<HH", img, off + 2, v & 0xFFFF, v >> 16)
        else:
            raise ValueError(f"relocation {w:#010x}: kind {kind} at {off:#x} is not a site inside the image ({len(img)} B)")
    return img


def module_fill(img):
    """the header's two CRCs as tools/build.py fills them after a link: the code's (from byte 0x100), then the header's (bytes 0..55)"""
    struct.pack_into("<I", img, MOD_CRC_AT, zlib.crc32(bytes(img[MOD_CODE_OFF:])) & 0xFFFFFFFF)
    struct.pack_into("<I", img, MOD_HCRC_AT, zlib.crc32(bytes(img[:MOD_HCRC_AT])) & 0xFFFFFFFF)
    return img


def ocm_images(raw, area_lo=0xC8000):
    """a module package (format 1: an image a start; format 2: one relocatable image, its list and each start's CRC-32) read into
    {start sector: the image that start is given}, every CRC checked. ValueError for a package the page or the card would refuse."""
    b = bytes(raw)
    if len(b) < 60:
        raise ValueError("too short")
    magic, ver, n = struct.unpack_from("<IHH", b, 0)
    if magic != OCM_MAGIC or ver not in (1, 2) or not 1 <= n <= 8:
        raise ValueError("not a module package")
    tab = 56
    if ver == 1:
        at = tab + 16 * n
        if len(b) < at + 4 or zlib.crc32(b[:at]) != struct.unpack_from("<I", b, at)[0]:
            raise ValueError("header CRC")
        out = {}
        for k in range(n):
            start, length, crc, off = struct.unpack_from("<IIII", b, tab + 16 * k)
            img = b[off:off + length]
            if len(img) != length or zlib.crc32(img) != crc:
                raise ValueError(f"image {k}: CRC")
            out[start] = img
        return out
    if n != 1:
        raise ValueError("format 2 has one image")
    start, length, crc, off = struct.unpack_from("<IIII", b, tab)
    nrel, roff, rcrc, m = struct.unpack_from("<IIII", b, tab + 16)
    at = tab + 32 + 8 * m
    if start != OCM_RELOCATABLE or not 1 <= m <= 8 or nrel > RELOC_MAX or len(b) < at + 4 or zlib.crc32(b[:at]) != struct.unpack_from("<I", b, at)[0]:
        raise ValueError("header")
    img, lst = b[off:off + length], b[roff:roff + 4 * nrel]
    if len(img) != length or zlib.crc32(img) != crc or len(lst) != 4 * nrel or zlib.crc32(lst) != rcrc:
        raise ValueError("image or list CRC")
    words = struct.unpack(f"<{nrel}I", lst)
    out = {}
    for k in range(m):
        sec, want = struct.unpack_from("<II", b, tab + 32 + 8 * k)
        got = module_fill(apply_relocs(img, words, xip(area_lo + sec * SECTOR)))
        if zlib.crc32(bytes(got)) != want:
            raise ValueError(f"start {sec}: the relocated image is not the one the build proved")
        out[sec] = bytes(got)
    return out
