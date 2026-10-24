#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Revisions:
# (Orange Cat 0.13.6) the Nightly channel can carry a branch build with its pack, modules and set manifests (channel(), keep_release, listing()); web/publish_nightly.py keeps the stable pack and modules
# (Orange Cat 0.11.2) the installer page installs firmware, pack and module in one go
# (Orange Cat 0.12.2) web tests for the Prophet package, modBlocked and the swap planner; check_set checks module images against their sectors (test added); gen_fw_api_i...
"""Make the site (GitHub Pages):

  index.html                        redirect to the installer (the old URL keeps working)
  firmware/melodee-VER.fwsc         the package, both cores (+ LICENSE, LICENSING.md, LICENSES/: the
                                    packages hold JieLi SDK files under Apache-2.0, see LICENSING.md)
  firmware/orangecat-VER.pak        (0.11.2) the asset pack of the same build, offered by the installer after the firmware
  firmware/orangecat-NAME-VER.ocm   (0.11.2) the engine module packages of the same build (shapes), likewise
  firmware/melodee-nightly*.fwsc    next's development builds (nightly.json; publish_nightly.py), kept as published
  firmware/orangecat-nightly.pak, orangecat-NAME-nightly.ocm, orangecat-nightly.{pack,shapes,prophet}.set.json
                                    (0.13.6) the pack, modules and set manifests of a branch build in the Nightly channel (nightly.json)
  firmware/melodee-*-rc.*.fwsc      published release candidate (rc.json; publish_rc.py), kept as published
  webapp/installer/index.html       index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html          editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  webapp/manual/                    web/manual/ on a full publish (the stable manual: HTML and screenshots).
                                    Nightly and release-candidate updates leave it in place.
  src/                              not touched

  web/make_site.py build/melodee-X.Y.fwsc X.Y OUT_DIR

Packages must be made by tools/fm1pkg_make.py (Melodee's own loader, no vendor files).
Their identity is read from the package; the device must report it after installation.

Manual screenshots are generated into web/manual/ and are not stored in git. A full
publish and web/quick-host.sh run tools/manual_shots.py first — see BUILDING.md "Local site".
The figure names are the <img src> values in web/manual/index.html. A full publish checks
every one of them before it writes the output site, then replaces webapp/manual/ with the
manual page and those screenshots. Other PNGs and older version folders are removed; nightly/ is preserved. An
installer-only update does not read or replace the published manual.
"""
import json
import html as html_escape
import re
import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F


def strip_module(src):
    """drop ES module import/export so the script can be inlined into index_pkg.html"""
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def check_package(pkg):
    """the package's bytes and identity; refuses anything that is not a Melodee package"""
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d{2,4}", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not a Melodee package (FM-1_9 and two to four digits)")
    if b"FELUCCA-LOADER-1" not in raw:              # Melodee loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Melodee loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    return raw, product


def release_json(out):
    """(0.13.6) a published site's whole release entry (firmware/release.json: version, product, pkg, pak, ocm); the installer page's inline copy on an older site"""
    out = Path(out)
    manifest = out / "firmware/release.json"
    if manifest.exists():
        return json.loads(manifest.read_text())
    html = (out / "webapp/installer/index.html").read_text()
    match = re.search(r"^const meta = (\{.*\});$", html, re.M)
    if not match:
        raise SystemExit("Pages installer has no release metadata")
    meta = json.loads(match[1])
    return {k: meta[k] for k in ("version", "product", "pkg", "pak", "ocm") if k in meta}


def release_packages(out):
    """A published site's release version and dual-core package path."""
    out = Path(out)
    manifest = out / "firmware/release.json"
    if manifest.exists():
        release = json.loads(manifest.read_text())
    else:  # Existing sites predate the release manifest.
        html = (out / "webapp/installer/index.html").read_text()
        match = re.search(r"^const meta = (\{.*\});$", html, re.M)
        if not match:
            raise SystemExit("Pages installer has no release metadata")
        release = json.loads(match[1])
    return release["version"], {"dual": out / "firmware" / Path(release["pkg"]["dual"]).name}


def published(out, name):
    """a channel the site already carries (firmware/NAME.json), its packages where they are; None without one"""
    path = out / "firmware" / f"{name}.json"
    if not path.exists():
        return None
    spec = json.loads(path.read_text())
    if "dual" not in spec["pkg"]:
        return None
    spec["pkg"] = {"dual": str(out / "firmware" / Path(spec["pkg"]["dual"]).name)}
    if spec.get("pak"):                         # (0.13.6) a nightly with its pack and modules: kept as published (the files are in the site already)
        spec["kept"] = True
    return spec


def channel(spec, label, stem, branch):
    """Validate a dual-core development package and prepare installer metadata.

    (0.13.6) A nightly spec may also carry the build's set: "pak", "ocm" (a list) and "manifest" (tools/oc_manifest.py), checked by check_set() against each
    other and the firmware's numbers, and served as orangecat-nightly.pak / orangecat-NAME-nightly.ocm. "branchName" is the branch the build was made from
    (the installer's Nightly entry shows it); "version" is the firmware's own version string (v0.6.2-abc123). A spec read back from a published site
    ("kept") has its set files in the site already and is passed through.
    """
    if not re.fullmatch(r"[0-9a-f]{40}", spec["commit"]):
        raise SystemExit(f"{label} commit must be a full Git SHA")
    if set(spec["pkg"]) != {"dual"}:
        raise SystemExit(f"{label} must offer dual-core firmware only")
    path = Path(spec["pkg"]["dual"])
    data, identity = check_package(path)
    if branch == "nightly" and identity != "FM-1_900":
        raise SystemExit("Nightly firmware must use development identity FM-1_900")
    files = {"dual": (path, data, identity, f"{stem}.fwsc")}
    meta = {"version": spec["version"], "product": identity, "branch": branch, "commit": spec["commit"],
            "builtAt": spec["builtAt"], "pkg": {"dual": "../../firmware/" + f"{stem}.fwsc"}}
    if spec.get("branchName"):
        meta["branchName"] = spec["branchName"]
    if spec.get("pak"):
        if spec.get("kept"):
            meta["pak"], meta["ocm"] = spec["pak"], list(spec.get("ocm", []))
            meta["sets"] = list(spec.get("sets", []))
            return files, meta
        if not spec.get("manifest"):
            raise SystemExit(f"{label}: a pack goes with its manifest (tools/oc_manifest.py)")
        check_set(spec["pak"], spec.get("ocm", []), spec["manifest"])
        files["pak"] = (Path(spec["pak"]), Path(spec["pak"]).read_bytes(), None, f"{stem.replace('melodee-', 'orangecat-')}.pak")
        meta["pak"] = "../../firmware/" + files["pak"][3]
        meta["ocm"] = []
        for o in sorted(map(Path, spec.get("ocm", []))):
            m = re.fullmatch(r"orangecat-([a-z0-9]+)-.*\.ocm", o.name)
            if not m:
                raise SystemExit(f"{o}: a module package is named orangecat-NAME-VERSION[-COMMIT].ocm")
            fname = f"orangecat-{m[1]}-{stem.split('-', 1)[1]}.ocm"
            files["ocm:" + m[1]] = (o, o.read_bytes(), None, fname)
            meta["ocm"].append("../../firmware/" + fname)
        info = json.loads(Path(spec["manifest"]).read_text())      # the set manifests for the card, one per module choice (docs/orangecat/SET_FORMAT.md)
        base = stem.replace("melodee-", "orangecat-")
        meta["sets"] = []
        for choice in ["pack"] + [k[4:] for k in files if k.startswith("ocm:")]:
            sf = {"pak": files["pak"][3], "fwsc": f"{stem}.fwsc"}
            if choice != "pack":
                sf["ocm"] = [files["ocm:" + choice][3]]
            sname = f"{base}.{choice}.set.json"
            files["set:" + choice] = (None, (json.dumps({**info, "files": sf}, indent=2) + "\n").encode(), None, sname)
            meta["sets"].append(sname)
    return files, meta


def check_set(pak, ocms, manifest):
    """(0.11.2) the three files of a build belong together: the pack's header is sound and carries the table hash the firmware was built with, each module's
    header is sound and names the same pack hash and the firmware's interface identifier. `manifest` (tools/oc_manifest.py, made in the build from the headers
    the firmware was compiled with) says what the firmware has: {"pack_hash", "pack_len", "iface", "api_addr"}."""
    import struct
    import zlib
    info = json.loads(Path(manifest).read_text())
    raw = Path(pak).read_bytes()
    if len(raw) < 512 or struct.unpack_from("<I", raw, 0)[0] != 0x4B50434F:
        raise SystemExit(f"{pak}: not an asset pack")
    phash, plen, pcrc = struct.unpack_from("<III", raw, 8)
    if zlib.crc32(raw[:508]) != struct.unpack_from("<I", raw, 508)[0] or zlib.crc32(raw[512:]) != pcrc or plen != len(raw):
        raise SystemExit(f"{pak}: damaged (length or CRC)")
    if phash != info["pack_hash"] or plen != info["pack_len"]:
        raise SystemExit(f"{pak}: hash {phash:08x} / length {plen} is not the firmware's ({info['pack_hash']:08x} / {info['pack_len']})")
    for ocm in ocms:
        b = Path(ocm).read_bytes()
        magic, ver, n, iface, api_addr, engine = struct.unpack_from("<IHHIII", b, 0)
        mhash = struct.unpack_from("<I", b, 24)[0]
        if magic != 0x464D434F or ver != 1 or not 1 <= n <= 8:
            raise SystemExit(f"{ocm}: not a module package")
        head = 56 + 16 * n
        if zlib.crc32(b[:head]) != struct.unpack_from("<I", b, head)[0]:
            raise SystemExit(f"{ocm}: damaged (header CRC)")
        if iface != info["iface"] or api_addr != info["api_addr"]:
            raise SystemExit(f"{ocm}: interface {iface:08x} at {api_addr:#x} is not the firmware's ({info['iface']:08x} at {info['api_addr']:#x})")
        if mhash != phash:
            raise SystemExit(f"{ocm}: reads pack {mhash:08x}, the pack file is {phash:08x}")
        sectors = struct.unpack_from("<I", b, 52)[0]            # (0.12.2) the module's size in sectors of the 12 sector area (SHAPES 6, the Prophet 8): every image fits it,
        for k in range(n):                                       # and starts, in sectors from the area's first, leave room for it
            start, length, _, off = struct.unpack_from("<IIII", b, 56 + 16 * k)
            if not 1 <= sectors <= 12 or length > sectors * 4096 or start + sectors > 12 or off + length > len(b):
                raise SystemExit(f"{ocm}: image {k + 1} (start {start}, {length} B) does not fit the module's {sectors} sectors of the 12 sector area")
    return info


def release_history(out, current):
    """Offer validated stable dual-core packages, including historical releases."""
    releases = {current["version"]: current}
    fw = Path(out) / "firmware"
    for dual in fw.glob("melodee-*.fwsc"):
        match = re.fullmatch(r"melodee-(\d+\.\d+\.\d+)\.fwsc", dual.name)
        if not match or int(match[1].split(".")[0]) < 1 or match[1] == current["version"]:
            continue
        version = match[1]
        raw, product = check_package(dual)
        releases[version] = {"version": version, "product": product,
                             "pkg": {"dual": "../../firmware/" + dual.name}}
        pak = fw / f"orangecat-{version}.pak"                     # (0.11.2) an older release the site still carries with its pack and modules
        if pak.exists():
            releases[version]["pak"] = "../../firmware/" + pak.name
            releases[version]["ocm"] = ["../../firmware/" + o.name for o in sorted(fw.glob(f"orangecat-*-{version}.ocm"))]
    return [current] + [releases[v] for v in sorted(releases, key=lambda v: tuple(map(int, v.split("."))), reverse=True)
                        if v != current["version"]]


def manual_figures(manual):
    """PNG names embedded in the manual, in order; each src is a file name beside index.html"""
    page = manual / "index.html"
    if not page.is_file():
        raise SystemExit(f"{page} is missing")
    names = []
    for name in re.findall(r'<img\b[^>]*\bsrc="([^"]*)"', page.read_text(encoding="utf-8")):
        if name in names:
            continue
        if not re.fullmatch(r"[A-Za-z0-9_.-]+\.png", name):
            raise SystemExit(f"{page}: screenshot src {name!r} must name a PNG beside the manual")
        names.append(name)
    return names


def require_manual(manual):
    """every embedded screenshot exists; called before a full publish writes the output site"""
    missing = [name for name in manual_figures(manual) if not (manual / name).is_file()]
    if missing:
        raise SystemExit(f"{manual}: {len(missing)} screenshot(s) missing: {', '.join(missing)}")


def publish_manual(src, dest, version=None, preserve_nightly=False):
    """Publish the guide and embedded screenshots, optionally retaining its nightly child."""
    embedded = set(manual_figures(src))
    keep = {p.name for p in src.iterdir()
            if p.is_file() and (p.suffix.lower() != ".png" or p.name in embedded)}
    dest.mkdir(parents=True, exist_ok=True)
    for old in list(dest.iterdir()):
        if preserve_nightly and old.name == "nightly" and old.is_dir():
            continue
        if old.name in keep:
            continue
        if old.is_dir():
            shutil.rmtree(old)
        else:
            old.unlink()
    for name in sorted(keep):
        shutil.copy(src / name, dest / name)
    if version is not None:
        page = dest / "index.html"
        text = page.read_text(encoding="utf-8").replace(
            '<span data-firmware-version>development source</span>',
            '<span data-firmware-version>' + html_escape.escape(version) + '</span>')
        # A nightly guide lives one level below the stable guide.
        if dest.name == "nightly":
            text = text.replace('href="../editor/"', 'href="../../editor/"').replace(
                'href="../installer/"', 'href="../../installer/"')
        page.write_text(text, encoding="utf-8")


def main(pkg, version, out, nightly=None, installer_only=False, rc=None, manual=None, pak=None, ocms=(), manifest=None, keep_release=False):
    """keep_release (0.13.6): the stable release is the one the site already publishes (firmware/release.json); its metadata, with its pack and modules, is
    passed through, so a nightly or an RC publish cannot change what Standard offers. pkg is that release's package in the site."""
    pkg, out = Path(pkg), Path(out)
    raw, product = check_package(pkg)
    if nightly is None:
        nightly = published(out, "nightly")
    nightly_files, nightly_meta = channel(nightly, "nightly", "melodee-nightly", "nightly") if nightly else ({}, None)
    if rc is None:
        rc = published(out, "rc")
    rc_files, rc_meta = {}, None
    if rc:
        if not re.fullmatch(r"\d\.\d{1,2}\.\d-rc\.\d+", rc["version"]):
            raise SystemExit("Release candidate version must be X.Y.Z-rc.N")
        rc_files, rc_meta = channel(rc, "release candidate", f"melodee-{rc['version']}", "next")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1oc.js").read_text(encoding="utf-8"))      # (0.11.2) the pack and module stages
    v = re.sub(r'[^A-Za-z0-9.-]', '-', version)
    name = f"melodee-{v}.fwsc"
    release_meta = {"version": version, "product": product,
                    "pkg": {"dual": "../../firmware/" + name}}
    if keep_release:
        kept = release_json(out)
        kept["pkg"] = {"dual": kept["pkg"]["dual"]}          # a legacy single-core entry is retired
        release_meta, version = kept, kept["version"]
        name = Path(kept["pkg"]["dual"]).name
        v = re.sub(r'[^A-Za-z0-9.-]', '-', version)
    set_files = {}                                          # (0.11.2) the pack and the modules of this build, beside the firmware
    if pak or ocms:
        if not pak or not manifest:
            raise SystemExit("a pack and its manifest go together with the modules (tools/oc_manifest.py)")
        check_set(pak, ocms, manifest)
        set_files[f"orangecat-{v}.pak"] = Path(pak)
        release_meta["pak"] = "../../firmware/" + f"orangecat-{v}.pak"
        release_meta["ocm"] = []
        for o in sorted(map(Path, ocms)):
            m = re.fullmatch(r"orangecat-([a-z0-9]+)-.*\.ocm", o.name)
            if not m:
                raise SystemExit(f"{o}: a module package is named orangecat-NAME-VERSION[-COMMIT].ocm")
            fname = f"orangecat-{m[1]}-{v}.ocm"
            set_files[fname] = o
            release_meta["ocm"].append("../../firmware/" + fname)
    releases = release_history(out, release_meta)
    meta = json.dumps({**release_meta, "nightly": nightly_meta, "rc": rc_meta, "releases": releases})
    for mark in ("/*LIB*/", "/*META*/{}"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/{}", meta)
    inst, ed, man, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "webapp" / "manual", out / "firmware"
    man_src = Path(manual) if manual is not None else HERE / "manual"
    if not installer_only:
        require_manual(man_src)                     # before any write: a missing figure must leave the site untouched
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    set_keep = set(set_files)
    for key, entry in nightly_files.items():                # (0.13.6) the nightly's pack and modules
        if key == "pak" or key.startswith("ocm:"):
            set_keep.add(entry[3])
    if nightly_meta:
        set_keep.update(Path(x).name for x in ([nightly_meta["pak"]] if nightly_meta.get("pak") else []) + list(nightly_meta.get("ocm", [])))
    for r in releases:
        set_keep.update(Path(x).name for x in ([r["pak"]] if r.get("pak") else []) + list(r.get("ocm", [])))
    for old in list(fw.glob("orangecat-*.pak")) + list(fw.glob("orangecat-*.ocm")):
        if old.name not in set_keep:
            old.unlink()
    for fname, src in set_files.items():
        if src.resolve() != (fw / fname).resolve():
            shutil.copy(src, fw / fname)
    keep = {name, "melodee-nightly.fwsc"}
    keep.update(file[3] for file in rc_files.values())
    keep.update(Path(path).name for release in releases for path in release["pkg"].values())
    for old in fw.glob("melodee-*.fwsc"):
        if old.name not in keep:
            old.unlink()
    (fw / "synth.json").unlink(missing_ok=True)
    retired_editor = out / "webapp" / "synth-editor"
    if retired_editor.exists():
        shutil.rmtree(retired_editor)
    (inst / "index.html").write_text(html, encoding="utf-8")
    if pkg.resolve() != (fw / name).resolve():
        shutil.copy(pkg, fw / name)
    if set_files:                                   # (0.13.3) the sets for the card: the manifest of this build with the names the site gives its files, one per module choice
        info = json.loads(Path(manifest).read_text())
        for old_set in fw.glob(f"orangecat-{v}.*.set.json"):
            old_set.unlink()
        choices = {"pack": []}
        for fname in set_files:
            m = re.fullmatch(r"orangecat-([a-z0-9]+)-.*\.ocm", fname)
            if m:
                choices[m[1]] = [fname]
        for choice, ocm in choices.items():
            files = {"pak": f"orangecat-{v}.pak", "fwsc": name}
            if ocm:
                files["ocm"] = ocm
            (fw / f"orangecat-{v}.{choice}.set.json").write_text(json.dumps({**info, "files": files}, indent=2) + "\n")
    (fw / "release.json").write_text(json.dumps(release_meta, indent=2) + "\n")
    (fw / "releases.json").write_text(json.dumps(releases, indent=2) + "\n")
    if nightly_meta:
        if any(k.startswith("set:") for k in nightly_files):
            for old_set in fw.glob("orangecat-nightly.*.set.json"):
                old_set.unlink()
        for path, data, identity, filename in nightly_files.values():
            (fw / filename).write_bytes(data)
        (fw / "nightly.json").write_text(json.dumps(nightly_meta, indent=2) + "\n")
    if rc_meta:
        for path, data, identity, filename in rc_files.values():
            (fw / filename).write_bytes(data)
        (fw / "rc.json").write_text(json.dumps(rc_meta, indent=2) + "\n")
    lic = HERE.parent / "LICENSES"                  # the packages hold JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Melodee's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Melodee licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    if not installer_only:
        publish_manual(man_src, man, version, preserve_nightly=True)
        shutil.copy(HERE / "editor.html", ed / "index.html")
        for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
            if (HERE / f).exists():
                shutil.copy(HERE / f, ed / f)
        (out / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>Melodee</title>'
            '<meta http-equiv="refresh" content="0; url=webapp/installer/">'
            '<a href="webapp/installer/">Melodee installer</a>\n', encoding="utf-8")
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B), {product}")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
