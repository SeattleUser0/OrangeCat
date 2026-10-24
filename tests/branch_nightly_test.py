#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Revisions:
# (Orange Cat 0.13.6) first version: a branch build published as the Nightly channel of the stable site, Standard untouched
"""Orange Cat 0.13.6: the workflow input publish_nightly. A fake artifact (a firmware, a pack, two modules, a manifest) is published as the Nightly channel
of a copy of the stable site, the way the pages job does it (web/mirror_site.py, tools/orangecat_site.py nightly, web/site_check.py):

  both channels are there; every Standard file is byte for byte what the mirror took (release.json, releases.json, packages, pack, modules, set manifests,
  editor, manual); the Nightly files have the names the installer page's set flow reads (meta.nightly.pak and .ocm; the module kind comes from the file name);
  the set manifests name the nightly's own files and carry the manifest's numbers; a set that does not belong together writes nothing; a second nightly
  replaces the first; a stable (main) publish from scratch carries no nightly; --list names both channels and fails on a missing file.
"""
import functools
import http.server
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
sys.path.insert(0, str(ROOT / "web"))
sys.path.insert(0, str(ROOT / "tools"))
import orangecat_site  # noqa: E402
import orangecat_site_test as fake  # noqa: E402  (package(), pak(), ocm(), manifest())
import mirror_site  # noqa: E402
import site_check  # noqa: E402
from make_site import manual_figures  # noqa: E402

SHA = "a6c6d6b" + "0" * 33
BRANCH, FWV, BUILT = "sonnet-work", "v0.6.2-a6c6d6", "2026-10-24T12:00:00Z"


class BranchNightly(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        r = cls.r = Path(cls._tmp.name)
        # the live stable site: a main build with its pack and modules, an older release, an editor and a manual
        cls.live = r / "live"
        stable = r / "stable"
        stable.mkdir()
        fw = fake.package(stable / "orangecat-0.6.2-abc1234.fwsc", "FM-1_90112")
        pk = fake.pak(stable / "orangecat-0.6.2-abc1234.pak")
        sh = fake.ocm(stable / "orangecat-shapes-0.6.2-abc1234.ocm")
        pr = fake.ocm(stable / "orangecat-prophet-0.6.2-abc1234.ocm", engine=19)
        mf = fake.manifest(stable / "orangecat-0.6.2-abc1234.manifest.json")
        (cls.live / "firmware").mkdir(parents=True)
        fake.package(cls.live / "firmware/melodee-1.0.0.fwsc", "FM-1_91000")      # an older release the site keeps in its list
        orangecat_site.main(fw, "0.6.2", cls.live, pk, [sh, pr], mf)
        manual = cls.live / "webapp/manual"
        manual.mkdir(parents=True)
        shutil.copy(ROOT / "web/manual/index.html", manual / "index.html")
        for name in manual_figures(manual):
            (manual / name).write_bytes(b"png " + name.encode())
        # the branch's artifact: another build of the same source (same numbers), identity FM-1_900
        cls.art = r / "art"
        cls.art.mkdir()
        cls.bfw = fake.package(cls.art / "orangecat-0.6.2-a6c6d6b.fwsc", "FM-1_900")
        cls.bpak = fake.pak(cls.art / "orangecat-0.6.2-a6c6d6b.pak")
        cls.bsh = fake.ocm(cls.art / "orangecat-shapes-0.6.2-a6c6d6b.ocm")
        cls.bpr = fake.ocm(cls.art / "orangecat-prophet-0.6.2-a6c6d6b.ocm", engine=19)
        cls.bmf = fake.manifest(cls.art / "orangecat-0.6.2-a6c6d6b.manifest.json")
        # serve it, to run the mirror as the job does
        os.environ["no_proxy"] = os.environ["NO_PROXY"] = "127.0.0.1,localhost"
        class Quiet(http.server.SimpleHTTPRequestHandler):
            def log_message(self, *a):
                pass
        handler = functools.partial(Quiet, directory=str(cls.live))
        cls.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), handler)
        threading.Thread(target=cls.httpd.serve_forever, daemon=True).start()
        cls.url = f"http://127.0.0.1:{cls.httpd.server_address[1]}/"

    @classmethod
    def tearDownClass(cls):
        cls.httpd.shutdown()
        cls._tmp.cleanup()

    def setUp(self):
        self.site = self.r / "site"
        shutil.rmtree(self.site, ignore_errors=True)
        mirror_site.mirror(self.url, self.site)
        site_check.Path(self.r / "base.json").write_text(json.dumps(site_check.snapshot(self.site)))

    def publish(self, **kw):
        a = dict(package=self.bfw, branch=BRANCH, commit=SHA, built_at=BUILT, fw_version=FWV, pak=self.bpak, ocms=[self.bsh, self.bpr], manifest=self.bmf)
        a.update(kw)
        orangecat_site.nightly(self.site, a["package"], a["branch"], a["commit"], a["built_at"], a["fw_version"], a["pak"], a["ocms"], a["manifest"])

    def meta(self):
        html = (self.site / "webapp/installer/index.html").read_text()
        return json.loads(re.search(r"^const meta = (\{.*\});$", html, re.M)[1])

    def test_the_mirror_is_the_live_site(self):
        for rel in site_check.stable_files(self.live):
            self.assertEqual((self.live / rel).read_bytes(), (self.site / rel).read_bytes(), rel)
        self.assertTrue((self.site / "firmware/orangecat-0.6.2.pak").is_file())
        self.assertTrue((self.site / "webapp/editor/index.html").is_file())
        self.assertTrue((self.site / "webapp/manual/index.html").is_file())
        self.assertFalse((self.site / "firmware/nightly.json").exists())

    def test_standard_is_unchanged(self):
        before = {rel: (self.site / rel).read_bytes() for rel in site_check.stable_files(self.site)}
        self.assertIn("firmware/release.json", before)
        self.publish()
        self.assertEqual(site_check.compare(self.site, json.loads((self.r / "base.json").read_text())), [])
        for rel, data in before.items():
            self.assertEqual((self.site / rel).read_bytes(), data, rel)
        rel = json.loads((self.site / "firmware/release.json").read_text())
        self.assertEqual((rel["version"], rel["pak"], len(rel["ocm"])), ("0.6.2", "../../firmware/orangecat-0.6.2.pak", 2))
        m = self.meta()
        for key in ("version", "product", "pkg", "pak", "ocm"):
            self.assertEqual(m[key], rel[key], key)               # the page's Standard entry is the live release, pack and modules included
        self.assertEqual([r["version"] for r in m["releases"]], ["0.6.2", "1.0.0"])
        self.assertEqual(m["releases"][0]["pak"], rel["pak"])

    def test_the_branch_editor_is_beside_the_stable_one(self):
        # (0.13.6) the Audio readout is in the branch's editor only: webapp/nightly-editor/ is the branch's editor.html, webapp/editor/ stays the live one
        stable = (self.site / "webapp/editor/index.html").read_bytes()
        self.publish()
        self.assertEqual((self.site / "webapp/editor/index.html").read_bytes(), stable)
        self.assertEqual((self.site / "webapp/nightly-editor/index.html").read_bytes(), (ROOT / "web/editor.html").read_bytes())
        self.assertNotIn("webapp/nightly-editor/index.html", site_check.stable_files(self.site))

    def test_nightly_files_are_named_as_the_page_reads_them(self):
        self.publish()
        fw = self.site / "firmware"
        n = self.meta()["nightly"]
        self.assertEqual(n, json.loads((fw / "nightly.json").read_text()))
        self.assertEqual((n["branchName"], n["commit"], n["version"], n["builtAt"], n["product"]), (BRANCH, SHA, FWV, BUILT, "FM-1_900"))
        self.assertEqual(n["pkg"], {"dual": "../../firmware/melodee-nightly.fwsc"})
        self.assertEqual(n["pak"], "../../firmware/orangecat-nightly.pak")
        self.assertEqual(n["ocm"], ["../../firmware/orangecat-prophet-nightly.ocm", "../../firmware/orangecat-shapes-nightly.ocm"])
        for ref in [n["pkg"]["dual"], n["pak"]] + n["ocm"]:
            self.assertTrue((fw / Path(ref).name).is_file(), ref)
        self.assertEqual((fw / "orangecat-nightly.pak").read_bytes(), self.bpak.read_bytes())
        self.assertEqual((fw / "orangecat-shapes-nightly.ocm").read_bytes(), self.bsh.read_bytes())
        self.assertEqual((fw / "melodee-nightly.fwsc").read_bytes(), self.bfw.read_bytes())
        for ref in n["ocm"]:                                       # index_pkg.html: ocmKind(u) = /orangecat-prophet-/ ? "prophet" : "shapes"
            self.assertRegex(ref, r"orangecat-(shapes|prophet)-nightly\.ocm$")
        self.assertEqual(sorted(p.name for p in fw.glob("orangecat-nightly.*.set.json")),
                         ["orangecat-nightly.pack.set.json", "orangecat-nightly.prophet.set.json", "orangecat-nightly.shapes.set.json"])

    def test_the_set_manifests_are_consistent(self):
        self.publish()
        fw = self.site / "firmware"
        info = json.loads(self.bmf.read_text())
        for choice, ocm in (("pack", None), ("shapes", "orangecat-shapes-nightly.ocm"), ("prophet", "orangecat-prophet-nightly.ocm")):
            sj = json.loads((fw / f"orangecat-nightly.{choice}.set.json").read_text())
            for key, value in info.items():
                self.assertEqual(sj[key], value, key)               # the manifest's numbers
            self.assertEqual(sj["files"]["pak"], "orangecat-nightly.pak")
            self.assertEqual(sj["files"]["fwsc"], "melodee-nightly.fwsc")
            self.assertEqual(sj["files"].get("ocm"), [ocm] if ocm else None)
            for name in [sj["files"]["pak"], sj["files"]["fwsc"]] + sj["files"].get("ocm", []):
                self.assertTrue((fw / name).is_file(), name)        # every named file is in the same folder
        self.assertEqual(sorted(self.meta()["nightly"]["sets"]), sorted(p.name for p in fw.glob("orangecat-nightly.*.set.json")))

    def test_a_set_that_does_not_belong_together_writes_nothing(self):
        before = {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()}
        bad = fake.pak(self.art / "bad.pak", phash=0x11111111)
        with self.assertRaises(SystemExit):
            self.publish(pak=bad)
        with self.assertRaises(SystemExit):
            self.publish(package=fake.package(self.art / "stable-identity.fwsc", "FM-1_90112"))   # not the development identity
        with self.assertRaises(SystemExit):
            self.publish(commit="short")
        self.assertEqual(before, {p.relative_to(self.site): p.read_bytes() for p in self.site.rglob("*") if p.is_file()})

    def test_a_second_nightly_replaces_the_first(self):
        self.publish()
        sha2 = "b" * 40
        self.publish(commit=sha2, branch="other", ocms=[self.bsh])
        n = self.meta()["nightly"]
        self.assertEqual((n["branchName"], n["commit"], len(n["ocm"])), ("other", sha2, 1))
        fw = self.site / "firmware"
        self.assertFalse((fw / "orangecat-prophet-nightly.ocm").exists())
        self.assertFalse((fw / "orangecat-nightly.prophet.set.json").exists())
        self.assertEqual(site_check.compare(self.site, json.loads((self.r / "base.json").read_text())), [])

    def test_a_later_stable_publish_over_the_site_keeps_the_nightly_set(self):
        self.publish()
        from make_site import main
        main(self.site / "firmware/melodee-0.6.2.fwsc", "0.6.2", self.site, installer_only=True, keep_release=True)
        n = self.meta()["nightly"]
        self.assertTrue((self.site / "firmware/orangecat-nightly.pak").is_file())
        self.assertEqual(n["pak"], "../../firmware/orangecat-nightly.pak")
        self.assertEqual(len(n["ocm"]), 2)

    def test_a_main_publish_from_scratch_has_no_nightly(self):
        fresh = self.r / "fresh"
        shutil.rmtree(fresh, ignore_errors=True)
        orangecat_site.main(self.live / "firmware/melodee-0.6.2.fwsc", "0.6.2", fresh,
                            self.live / "firmware/orangecat-0.6.2.pak", sorted((self.live / "firmware").glob("orangecat-*-0.6.2.ocm")),
                            self.r / "stable/orangecat-0.6.2-abc1234.manifest.json")
        self.assertFalse((fresh / "firmware/nightly.json").exists())
        self.assertIsNone(self.meta_of(fresh)["nightly"])

    def meta_of(self, site):
        html = (site / "webapp/installer/index.html").read_text()
        return json.loads(re.search(r"^const meta = (\{.*\});$", html, re.M)[1])

    def test_list_names_both_channels_and_fails_on_a_missing_file(self):
        self.publish()
        text = site_check.listing(self.site)
        self.assertIn("STANDARD  v0.6.2", text)
        self.assertIn(f"NIGHTLY  {BRANCH} · a6c6d6b · {FWV} · {BUILT}", text)
        for name in ("melodee-0.6.2.fwsc", "orangecat-0.6.2.pak", "orangecat-shapes-0.6.2.ocm", "melodee-nightly.fwsc", "orangecat-nightly.pak",
                     "orangecat-prophet-nightly.ocm", "orangecat-nightly.shapes.set.json", "webapp/installer/index.html"):
            self.assertRegex(text, rf"\d+ B  .*{re.escape(name)}")
        self.assertNotIn("MISSING", text)
        cli = [sys.executable, str(ROOT / "web/site_check.py"), str(self.site), "--list"]
        self.assertEqual(subprocess.run(cli, capture_output=True).returncode, 0)
        (self.site / "firmware/orangecat-nightly.pak").unlink()
        self.assertEqual(subprocess.run(cli, capture_output=True).returncode, 1)

    def test_compare_names_a_changed_standard_file(self):
        self.publish()
        (self.site / "firmware/orangecat-0.6.2.pak").write_bytes(b"x")
        self.assertEqual(site_check.compare(self.site, json.loads((self.r / "base.json").read_text())), ["changed: firmware/orangecat-0.6.2.pak"])


if __name__ == "__main__":
    unittest.main()
