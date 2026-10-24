#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
"""Move a build file out of a GitHub Actions run as workflow notices (size audit, temporary).

The audit's workspace can reach api.github.com but not the Actions artifact store (Azure blob) nor the
job logs (same store), so the symbol table and section table of a CI build travel as check-run annotations:
a notice holds about 4 KB and a step may raise ten. The file is xz-compressed, base64 encoded and cut
into numbered chunks:

  ci_blob.py emit TITLE FILE [--start K] [--max N]    print the chunks K.. (at most N) as ::notice lines
  ci_blob.py join ANNOTATIONS.json TITLE OUT          rebuild FILE from the check-run annotations JSON

A chunk's title is "TITLE i/n" (1-based); join refuses an incomplete set and says which chunks are missing.
"""
import base64
import json
import lzma
import sys

CHUNK = 3800            # characters of base64 per notice (a notice message is cut at about 4 KB)


def emit(title, path, start=1, maxn=8):
    raw = open(path, "rb").read()
    data = base64.b64encode(lzma.compress(raw, preset=9 | lzma.PRESET_EXTREME)).decode()
    chunks = [data[i:i + CHUNK] for i in range(0, len(data), CHUNK)]
    n = len(chunks)
    print(f"ci_blob: {path}: {len(raw)} B raw, {len(data)} B base64, {n} chunks; emitting {start}..{min(n, start + maxn - 1)}",
          file=sys.stderr)
    for i in range(start - 1, min(n, start - 1 + maxn)):
        print(f"::notice title={title} {i + 1}/{n}::{chunks[i]}")


def join(ann_path, title, out):
    ann = json.load(open(ann_path))
    parts, total = {}, None
    for a in ann:
        t = a.get("title") or ""
        if not t.startswith(title + " "):
            continue
        i, n = t[len(title) + 1:].split("/")
        parts[int(i)] = a["message"].strip()
        total = int(n)
    if total is None:
        raise SystemExit(f"ci_blob: no chunks titled '{title}' in {ann_path}")
    missing = [i for i in range(1, total + 1) if i not in parts]
    if missing:
        raise SystemExit(f"ci_blob: {title}: chunks missing: {missing} of {total}")
    data = "".join(parts[i] for i in range(1, total + 1))
    raw = lzma.decompress(base64.b64decode(data))
    open(out, "wb").write(raw)
    print(f"ci_blob: {out}: {len(raw)} B from {total} chunks")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a and a[0] == "emit":
        start, maxn, rest = 1, 8, []
        it = iter(a[1:])
        for x in it:
            if x == "--start": start = int(next(it))
            elif x == "--max": maxn = int(next(it))
            else: rest.append(x)
        emit(rest[0], rest[1], start, maxn)
    elif a and a[0] == "join":
        join(a[1], a[2], a[3])
    else:
        raise SystemExit(__doc__)
