#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Copyright 2026 Jeremiah Conner.
# PROJECT_FORMAT_R3.md 13 item 4 / section 10: the RELEASE 2 record store code (the release-2 branch's oc_store.c and storage.c, taken from git) runs on a converted
# store: it must find no old record, erase nothing of the new ones, and a save of its own beside them leaves every converted phrase readable.
# Run from the repository root:  tests/pj4_old_store.sh [REF]   (REF default origin/release-2)
set -eu
REF=${1:-origin/release-2}
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
mkdir -p "$W/old"
git show "$REF:firmware/src/oc_store.c" > "$W/old/oc_store.c"
git show "$REF:firmware/src/storage.c" > "$W/old/storage.c"
cc -O1 -w -Ifirmware/src -Itests tests/pj4_test.c -o "$W/pj4_test"
cc -O1 -w -DOLD_DIR="$W/old" -Ifirmware/src -Itests tests/pj4_old_store.c -o "$W/pj4_old_store"
"$W/pj4_test" --dump "$W/conv.img"
"$W/pj4_old_store" "$W/conv.img" "$W/after.img"
"$W/pj4_test" --verify "$W/after.img"
