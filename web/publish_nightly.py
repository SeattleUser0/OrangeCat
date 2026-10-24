#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Publish nightly firmware and an optional nightly manual; preserve the stable editor and manual.

(Orange Cat 0.13.6) The stable release entry is kept whole (make_site.main keep_release): before, its pack and modules were dropped from the installer's
metadata by a nightly publish. A branch build can carry its pack, modules and set manifest (--pak, --ocm, --manifest, --branch): they become the Nightly
channel's set (orangecat-nightly.pak, orangecat-NAME-nightly.ocm, orangecat-nightly.*.set.json). Standard, the stable editor and the stable manual are not touched.
"""
import argparse
import shutil
from pathlib import Path
from make_site import main, release_packages, require_manual, publish_manual

HERE = Path(__file__).resolve().parent


def publish(out, dual, version, commit, built_at, manual=None, pak=None, ocms=(), manifest=None, branch=None):
    if manual is not None:
        manual = Path(manual)
        require_manual(manual)
    release, packages = release_packages(out)
    spec = {"pkg": {"dual": dual}, "version": version, "commit": commit, "builtAt": built_at}
    if branch:
        spec["branchName"] = branch
    if pak:
        spec.update(pak=pak, ocm=list(ocms), manifest=manifest)
    main(packages["dual"], release, out, nightly=spec, installer_only=True, keep_release=True)
    # (0.13.6) the branch's own editor, beside the stable one (webapp/nightly-editor/): a branch build's new editor commands (the Audio readout) are not in main's
    # editor. Standard's files (webapp/editor/) are not touched, and site_check does not count this folder as Standard.
    ned = Path(out) / "webapp" / "nightly-editor"
    ned.mkdir(parents=True, exist_ok=True)
    shutil.copy(HERE / "editor.html", ned / "index.html")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ned / f)
    if manual is not None:
        root = Path(out) / "webapp/manual"
        publish_manual(manual, root / "nightly", version)
        if not (root / "index.html").exists():
            (root / "index.html").write_text(
                '<!doctype html><html lang="en"><meta charset="utf-8">'
                '<meta name="viewport" content="width=device-width, initial-scale=1">'
                '<title>Melodee manuals</title><h1>Melodee manuals</h1>'
                '<p>The stable guide will be published with a stable release.</p>'
                '<p><a href="nightly/">Read the nightly firmware manual</a></p>'
                '<p><a href="../installer/">Installer</a> · <a href="../editor/">Patch Editor</a></p>'
                '</html>\n', encoding="utf-8")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("out")
    parser.add_argument("--dual", required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--built-at", required=True)
    parser.add_argument("--manual", type=Path, help="publish this guide under webapp/manual/nightly/")
    parser.add_argument("--pak", help="the build's asset pack (needs --manifest)")
    parser.add_argument("--ocm", action="append", default=[], help="a module package of the build (repeat)")
    parser.add_argument("--manifest", help="the build's manifest (tools/oc_manifest.py)")
    parser.add_argument("--branch", help="the branch the build was made from (shown in the installer's Nightly entry)")
    args = parser.parse_args()
    publish(args.out, args.dual, args.version, args.commit, args.built_at, args.manual,
            args.pak, args.ocm, args.manifest, args.branch)
