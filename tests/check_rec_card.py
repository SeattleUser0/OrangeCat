#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.4.16: the card the firmware wrote, checked by an independent FAT implementation (pyfatfs) and
Python's own WAV reader.

tests/usb_host_test.c (t_freeze) freezes tracks onto the test card through the firmware's FatFs and writes the card
out as build/host/rec_card.img (48 MB, an MBR with one FAT32 partition at sector 2048). This opens that image with
pyfatfs and checks the takes in /ORANGECAT with the wave module: each is 16-bit stereo 44.1 kHz, exactly one cycle of
its track (T2_001.WAV: 5,003 frames; T3_001.WAV: two passes of 3,001; T4_001.WAV: 3,001), sample for sample the
test's stem (its fz_val). T1_001.WAV was cancelled and then stopped mid-take: it must not be there. The other files on
the card must still read as they were made (tests/make_fat_fixture.py), and no cluster may belong to two files or
folders, nor a chain be shorter than its file.

Needs pyfatfs (pip install pyfatfs). Without it, it says so and passes (the C test has checked the files through
FatFs already).

Revisions:
  0.4.16: first version.
  0.4.17: the pulled take (OC003), the mono track take (OC004), the cancelled one (OC005).
  0.4.19: the track take is stereo.
  0.5.3: the drive test's recorder is gone (OC001 to OC005): frozen tracks' takes are checked instead.
"""
import io
import struct
import sys
import wave

IMG = "build/host/rec_card.img"


def fz_val(k, p, right):                         # the test's stem of track k, p frames into its cycle
    if right:
        return (p * (17 + k) + 300) % 24000 - 12000
    return (p * (13 + k) + k * 77) % 20000 - 10000


def main():
    try:
        from pyfatfs.PyFatFS import PyFatFS
    except ImportError:
        print("check_rec_card: pyfatfs not installed; not checked")
        return 0
    fs = PyFatFS(IMG, offset=2048 * 512, read_only=True)
    names = sorted(fs.listdir("/ORANGECAT"))
    print("check_rec_card: /ORANGECAT holds", names)
    for name, k, frames in (("T2_001.WAV", 1, 5003), ("T3_001.WAV", 2, 2 * 3001), ("T4_001.WAV", 3, 3001)):
        data = fs.readbytes("/ORANGECAT/" + name)
        with wave.open(io.BytesIO(data)) as w:
            assert w.getnchannels() == 2 and w.getsampwidth() == 2 and w.getframerate() == 44100, name
            assert w.getnframes() == frames and len(data) == 512 + 4 * frames, (name, w.getnframes(), len(data))
            pcm = w.readframes(frames)
        bad = sum(1 for f in range(frames) if struct.unpack_from("<hh", pcm, 4 * f) != (fz_val(k, f, 0), fz_val(k, f, 1)))
        print(f"check_rec_card: {name}: {frames} frames, {bad} wrong")
        assert not bad
    assert "T1_001.WAV" not in names
    assert fs.readtext("/README.TXT") == "Orange Cat test card\n"
    assert fs.readbytes("/BIG.BIN") == bytes((i * 7 + (i >> 9)) & 255 for i in range(65536))
    assert len(fs.listdir("/MANY")) == 20
    pf = fs.fs                                  # every chain: whole, and no cluster in two
    cb = pf.bytes_per_cluster
    seen = {}

    def walk(d, path):
        dirs, files, _ = d.get_entries()             # (folders, files, special entries)
        for e in dirs + files:
            name = str(e)                               # (the long name if there is one, else the short)
            if name in (".", ".."):
                continue
            c = e.get_cluster()
            chain = list(pf.get_cluster_chain(c)) if c else []
            if not e.is_directory():
                need = -(-e.filesize // cb)
                assert len(chain) == need, (path + name, len(chain), need)
            for k in chain:
                assert k not in seen, (path + name, k, seen.get(k))
                seen[k] = path + name
            if e.is_directory():
                walk(e, path + name + "/")

    walk(pf.root_dir, "/")
    print(f"check_rec_card: {len(seen)} clusters in use, none in two chains")
    fs.close()
    print("check_rec_card: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
