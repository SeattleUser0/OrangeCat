#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.4.16: the card the recorder wrote, checked by an independent FAT implementation (pyfatfs) and
Python's own WAV reader.

tests/usb_host_test.c (t_rec) records two WAVs onto the test card through the firmware's FatFs and writes the
card out as build/host/rec_card.img (48 MB, an MBR with one FAT32 partition at sector 2048). This opens that image
with pyfatfs, reads /OC001.WAV and /OC002.WAV, and checks with the wave module that each is 16-bit stereo 44.1 kHz
with the frames the test gave: left ramp(f), right -ramp(3 f) / 2 (the test's rec_l, rec_r). The other files on the
card must still read as they were made (tests/make_fat_fixture.py), and no cluster may belong to two files or
folders, nor a chain be shorter than its file.

(0.4.17) The card now also holds OC003.WAV, a take the card stopped answering in the middle of (the test's "pulled
card"): its header and size must agree with each other at the last sync, its frames up to there must be right,
and its cluster chain may run past its size (clusters taken after the last sync), but not be shorter. OC004.WAV is
a synced take of track 2 alone: mono, 192,000 frames, the test's rec_t(f, 1) held to 16 bits. OC005.WAV was
cancelled: it must not be there.

Needs pyfatfs (pip install pyfatfs). Without it, it says so and passes (the C test has checked the files through
FatFs already).

Revisions:
  0.4.16: first version.
  0.4.17: the pulled take (OC003), the mono track take (OC004), the cancelled one (OC005).
"""
import io
import struct
import sys
import wave

IMG = "build/host/rec_card.img"


def ramp(i):
    return ((i * 37) % 32768) - 16384


def rec_t(f, t):                                # the test's track stem, held to 16 bits
    v = (f * (7 + t) + t * 1000) % 70000 - 35000
    return max(-32768, min(32767, v))


def c_div(a, b):                                # C's integer division (towards zero)
    q = abs(a) // abs(b)
    return q if (a >= 0) == (b >= 0) else -q


def main():
    try:
        from pyfatfs.PyFatFS import PyFatFS
    except ImportError:
        print("check_rec_card: pyfatfs not installed; not checked")
        return 0
    fs = PyFatFS(IMG, offset=2048 * 512, read_only=True)
    names = sorted(fs.listdir("/"))
    print("check_rec_card: / holds", names)
    for name, frames in (("OC001.WAV", 256000), ("OC002.WAV", 19200), ("OC003.WAV", None)):
        data = fs.readbytes("/" + name)
        with wave.open(io.BytesIO(data)) as w:
            assert w.getnchannels() == 2 and w.getsampwidth() == 2 and w.getframerate() == 44100, name
            if frames is None:                  # (the pulled take: what the last sync kept)
                frames = w.getnframes()
                assert frames > 0 and len(data) == 512 + 4 * frames, (name, frames, len(data))
            assert w.getnframes() == frames, (name, w.getnframes())
            pcm = w.readframes(frames)
        bad = 0
        for f in range(frames):
            l, r = struct.unpack_from("<hh", pcm, 4 * f)
            if l != ramp(f) or r != c_div(-ramp(f * 3), 2):
                bad += 1
        print(f"check_rec_card: {name}: {frames} frames, {bad} wrong")
        assert not bad
    with wave.open(io.BytesIO(fs.readbytes("/OC004.WAV"))) as w:
        assert w.getnchannels() == 1 and w.getsampwidth() == 2 and w.getframerate() == 44100
        assert w.getnframes() == 192000
        pcm = w.readframes(192000)
    bad = sum(1 for f in range(192000) if struct.unpack_from("<h", pcm, 2 * f)[0] != rec_t(f, 1))
    print(f"check_rec_card: OC004.WAV: track 2, mono, 192000 frames, {bad} wrong")
    assert not bad
    assert "OC005.WAV" not in names
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
                if name == "OC003.WAV":         # (pulled: clusters taken after the last sync may hang on)
                    assert len(chain) >= need, (path + name, len(chain), need)
                else:
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
