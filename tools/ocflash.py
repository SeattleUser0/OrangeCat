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

Revisions:
  0.10.1: first version (tail_hit and crc16 moved here from gen_pack.py; xip is new).
"""
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
