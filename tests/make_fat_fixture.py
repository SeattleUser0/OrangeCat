#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.4.10: the SD card image the drive test's file browser is tested on (tests/usb_host_test.c).

A 48 MB card: an MBR with one FAT32 partition (type 0x0C) at sector 2048, made by pyfatfs (an independent FAT
implementation, so the firmware's FatFs is not checked against itself), holding:

  /README.TXT                       a short text file
  /._README.TXT                     a macOS leftover (the browser hides names starting with ".")
  /SAMPLES/Kick 808 Long Name.wav   WAV, 44100 Hz, 16-bit, mono, 0.5 s
  /SAMPLES/drums/snare.wav          WAV, 48000 Hz, 24-bit, stereo, 0.25 s
  /MANY/FILE01.BIN .. FILE20.BIN    20 small files (scrolling)
  /BIG.BIN                          64 KB of a counting pattern (the timed read)

Written as tests/fixtures/fat32_card.bin: only the sectors that are not all zero, each as a 4-byte little-endian
sector number and its 512 bytes. The test serves zeros for the rest. Needs pyfatfs (pip install pyfatfs); the
fixture is committed, so the tests do not.

Revisions:
  0.4.10: first version.
"""
import os
import struct
import sys
import tempfile
from pathlib import Path

from pyfatfs.PyFat import PyFat
from pyfatfs.PyFatFS import PyFatFS

PART_START = 2048
PART_SECTORS = 96 * 1024 - PART_START          # 48 MB card
OUT = Path(__file__).resolve().parent / "fixtures" / "fat32_card.bin"


def wav(rate, bits, channels, seconds):
    frames = int(rate * seconds)
    data = bytes(frames * channels * bits // 8)  # silence: zero sectors, left out of the fixture
    fmt = struct.pack("<HHIIHH", 1, channels, rate, rate * channels * bits // 8, channels * bits // 8, bits)
    body = b"WAVE" + b"fmt " + struct.pack("<I", len(fmt)) + fmt + b"LIST" + struct.pack("<I", 4) + b"INFO" + \
        b"data" + struct.pack("<I", len(data)) + data
    return b"RIFF" + struct.pack("<I", len(body)) + body


def main():
    with tempfile.TemporaryDirectory() as tmp:
        part = os.path.join(tmp, "part.img")
        with open(part, "wb") as f:
            f.truncate(PART_SECTORS * 512)
        pf = PyFat()
        pf.mkfs(part, PyFat.FAT_TYPE_FAT32, size=PART_SECTORS * 512, label="ORANGECAT")
        pf.close()
        fs = PyFatFS(part)
        fs.writetext("/README.TXT", "Orange Cat test card\n")
        fs.writebytes("/._README.TXT", b"\x00\x05\x16\x07" + bytes(60))
        fs.makedir("/SAMPLES")
        fs.writebytes("/SAMPLES/Kick 808 Long Name.wav", wav(44100, 16, 1, 0.5))
        fs.makedir("/SAMPLES/drums")
        fs.writebytes("/SAMPLES/drums/snare.wav", wav(48000, 24, 2, 0.25))
        fs.makedir("/MANY")
        for i in range(1, 21):
            fs.writebytes(f"/MANY/FILE{i:02d}.BIN", bytes([i]) * 100)
        fs.writebytes("/BIG.BIN", bytes((i * 7 + (i >> 9)) & 255 for i in range(65536)))
        fs.close()
        img = bytearray(512)
        img[446:462] = struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0x0C, b"\xff\xff\xff", PART_START, PART_SECTORS)
        img[510:512] = b"\x55\xaa"
        recs = [struct.pack("<I", 0) + bytes(img)]
        with open(part, "rb") as f:
            n = 0
            while True:
                s = f.read(512)
                if not s:
                    break
                if any(s):
                    recs.append(struct.pack("<I", PART_START + n) + s)
                n += 1
    OUT.parent.mkdir(exist_ok=True)
    OUT.write_bytes(b"".join(recs))
    print(f"{OUT}: {len(recs)} sectors, {OUT.stat().st_size} bytes")


if __name__ == "__main__":
    sys.exit(main())
