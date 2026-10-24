#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Orange Cat 0.11.2: the pages site serves the pack and the modules of a build beside the firmware, and refuses a set whose files do not belong together.

  firmware/melodee-VER.fwsc, firmware/orangecat-VER.pak, firmware/orangecat-NAME-VER.ocm, listed in firmware/release.json and in the installer's metadata;
  a pack with another hash than the firmware's, a module with another interface identifier or another pack hash: nothing is written;
  an older release keeps its set in the list; stale sets are removed; a site without a set (--pak not given) is what it was.
"""
import json
import re
import shutil
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "web"))
sys.path.insert(0, str(ROOT / "tools"))
from make_site import main, manual_figures  # noqa: E402
import oc_manifest  # noqa: E402

PHASH, PLEN, IFACE, API = 0x657ED950, 4096 + 512, 0x6865AF16, 0x02001100


def package(path, product="FM-1_90112"):
    raw = bytearray(1024)
    for i in range(20):
        raw[i * 0x30 + 0x2F] = (ord(product[i]) + i + 1) & 255 if i < len(product) else 0x7D
    raw[960:976] = b"FELUCCA-LOADER-1" + bytes([1])
    path.write_bytes(raw)
    return path


def pak(path, phash=PHASH):
    body = bytes(range(256)) * (PLEN - 512) // 256 if False else bytes((i * 7) & 255 for i in range(PLEN - 512))
    h = bytearray(b"\xff" * 512)
    struct.pack_into("<IHHIII", h, 0, 0x4B50434F, 1, 0, phash, PLEN, zlib.crc32(body) & 0xFFFFFFFF)
    struct.pack_into("<I", h, 508, zlib.crc32(bytes(h[:508])) & 0xFFFFFFFF)
    path.write_bytes(bytes(h) + body)
    return path


def ocm(path, iface=IFACE, api=API, phash=PHASH, engine=14):
    img = b"OMOD" + bytes(0x200 - 4)
    head = bytearray(struct.pack("<IHHIIIII16sIII", 0x464D434F, 1, 1, iface, api, engine, 1234, phash, b"SHAPES", 1, 100, 6))
    off = (len(head) + 16 + 4 + 3) & ~3
    head += struct.pack("<IIII", 0, len(img), zlib.crc32(img) & 0xFFFFFFFF, off)
    head += struct.pack("<I", zlib.crc32(bytes(head)) & 0xFFFFFFFF)
    path.write_bytes(bytes(head) + img)
    return path


def manifest(path, **kw):
    info = {"version": "0.6.2", "pack_hash": PHASH, "pack_len": PLEN, "iface": IFACE, "api_addr": API}
    info.update(kw)
    path.write_text(json.dumps(info))
    return path


class SetSite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._scratch = tempfile.TemporaryDirectory()
        cls.manual = Path(cls._scratch.name) / "manual"
        cls.manual.mkdir()
        shutil.copy(ROOT / "web" / "manual" / "index.html", cls.manual / "index.html")
        for name in manual_figures(cls.manual):
            (cls.manual / name).write_bytes(b"png")

    @classmethod
    def tearDownClass(cls):
        cls._scratch.cleanup()

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.r = Path(self.temp.name)
        self.site = self.r / "site"
        self.fw = package(self.r / "orangecat-0.6.2-abc1234.fwsc")
        self.pak = pak(self.r / "orangecat-0.6.2-abc1234.pak")
        self.ocm = ocm(self.r / "orangecat-shapes-0.6.2-abc1234.ocm")
        self.man = manifest(self.r / "orangecat-0.6.2-abc1234.manifest.json")

    def tearDown(self):
        self.temp.cleanup()

    def build(self, version="0.6.2", **kw):
        kw.setdefault("manual", self.manual)
        main(self.fw, version, self.site, **kw)

    def meta(self):
        html = (self.site / "webapp/installer/index.html").read_text()
        return json.loads(re.search(r"^const meta = (\{.*\});$", html, re.M)[1])

    def test_files_and_metadata(self):
        self.build(pak=self.pak, ocms=[self.ocm], manifest=self.man)
        fw = self.site / "firmware"
        for name in ("melodee-0.6.2.fwsc", "orangecat-0.6.2.pak", "orangecat-shapes-0.6.2.ocm", "release.json"):
            self.assertTrue((fw / name).is_file(), name)
        self.assertEqual((fw / "orangecat-0.6.2.pak").read_bytes(), self.pak.read_bytes())
        rel = json.loads((fw / "release.json").read_text())
        self.assertEqual(rel["pak"], "../../firmware/orangecat-0.6.2.pak")
        self.assertEqual(rel["ocm"], ["../../firmware/orangecat-shapes-0.6.2.ocm"])
        m = self.meta()
        self.assertEqual((m["pak"], m["ocm"]), (rel["pak"], rel["ocm"]))
        self.assertEqual(m["releases"][0]["pak"], rel["pak"])

    def test_no_set_is_what_it_was(self):
        self.build()
        m = self.meta()
        self.assertNotIn("pak", m)
        self.assertEqual(list((self.site / "firmware").glob("orangecat-*")), [])

    def refused(self, **kw):
        with self.assertRaises(SystemExit):
            self.build(**kw)
        self.assertFalse(self.site.exists() and any(self.site.rglob("orangecat-*.pak")))

    def test_pack_of_another_firmware_is_refused(self):
        self.refused(pak=self.pak, ocms=[self.ocm], manifest=manifest(self.r / "m2.json", pack_hash=PHASH ^ 1))

    def test_pack_of_another_length_is_refused(self):
        self.refused(pak=self.pak, ocms=[self.ocm], manifest=manifest(self.r / "m2.json", pack_len=PLEN + 4))

    def test_module_of_another_interface_is_refused(self):
        self.refused(pak=self.pak, ocms=[ocm(self.r / "orangecat-shapes-bad.ocm", iface=IFACE ^ 1)], manifest=self.man)

    def test_module_reading_another_pack_is_refused(self):
        self.refused(pak=self.pak, ocms=[ocm(self.r / "orangecat-shapes-bad.ocm", phash=PHASH ^ 2)], manifest=self.man)

    def test_damaged_files_are_refused(self):
        raw = bytearray(self.pak.read_bytes()); raw[1000] ^= 1
        self.refused(pak=self.pak.write_bytes(bytes(raw)) and self.pak, ocms=[self.ocm], manifest=self.man)
        raw = bytearray(self.ocm.read_bytes()); raw[60] ^= 1
        self.ocm.write_bytes(bytes(raw))
        pak(self.pak)
        self.refused(pak=self.pak, ocms=[self.ocm], manifest=self.man)

    def test_pack_without_manifest_is_refused(self):
        self.refused(pak=self.pak, ocms=[self.ocm])

    def test_older_release_keeps_its_set_and_stale_files_go(self):
        self.build(pak=self.pak, ocms=[self.ocm], manifest=self.man)
        old = package(self.site / "firmware/melodee-1.2.1.fwsc", "FM-1_91021")
        (self.site / "firmware/orangecat-1.2.1.pak").write_bytes(b"old")
        (self.site / "firmware/orangecat-shapes-1.2.1.ocm").write_bytes(b"old")
        (self.site / "firmware/orangecat-9.9.9.pak").write_bytes(b"stale")
        self.build("1.3.2", pak=self.pak, ocms=[self.ocm], manifest=self.man)
        fw = self.site / "firmware"
        releases = json.loads((fw / "releases.json").read_text())
        old_rel = next(r for r in releases if r["version"] == "1.2.1")
        self.assertEqual(old_rel["pak"], "../../firmware/orangecat-1.2.1.pak")
        self.assertEqual(old_rel["ocm"], ["../../firmware/orangecat-shapes-1.2.1.ocm"])
        self.assertTrue((fw / "orangecat-1.2.1.pak").exists())
        self.assertFalse((fw / "orangecat-9.9.9.pak").exists())
        self.assertTrue((fw / "orangecat-1.3.2.pak").exists())
        self.assertTrue(old.exists())

    def test_the_installer_has_the_stages(self):
        self.build(pak=self.pak, ocms=[self.ocm], manifest=self.man)
        html = (self.site / "webapp/installer/index.html").read_text()
        for needle in ("export class", ):
            self.assertNotIn(needle, html)
        self.assertIn("async function runSet", html)
        self.assertIn('id="stage-pack"', html)


class Manifest(unittest.TestCase):
    def test_reads_the_generated_headers(self):
        with tempfile.TemporaryDirectory() as d:
            gen = Path(d)
            (gen / "oc_pack_tables.h").write_text("#define OC_PACK_HASH 0x657ED950u\n#define OC_PACK_LEN 40232u\n")
            (gen / "fw_api_id.h").write_text("#define FW_API_ID 0x6865AF16u\n#define FW_API_ADDR 0x02001100u\n")
            out = gen / "m.json"
            oc_manifest.main(out, gen)
            m = json.loads(out.read_text())
            self.assertEqual((m["pack_hash"], m["pack_len"], m["iface"], m["api_addr"]), (0x657ED950, 40232, 0x6865AF16, 0x02001100))


if __name__ == "__main__":
    unittest.main()
