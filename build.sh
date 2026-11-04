#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Kerem Kilic (Ellic Studio)
# Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Build Melodee on macOS (see BUILDING.md).
#   ./build.sh [--release X.Y[.Z]]
#   OC_CHECK=1 ./build.sh    (dev-tools) the compile check alone: -fsyntax-only through the JieLi clang, no link or package
#   FM1_TOOLS        where BUILDING.md's "Building on a Linux PC" put the toolchain and the SDK (default: ~/fm1-toolchain), as
#                    tools/hw/ci_step.py reads it; the two below default to its layout (dev-tools: they named ~/.jieli/toolchain and
#                    ~/fw-AC79_AIoT_SDK, which no setup makes)
#   JIELI_TOOLCHAIN  JieLi Linux toolchain (default: $FM1_TOOLS/jieli/toolchain)
#   AC79_SDK         JieLi AC79 SDK checkout (default: $FM1_TOOLS/fw-AC79_AIoT_SDK)
set -e
cd "$(dirname "$0")"
FM1_TOOLS="${FM1_TOOLS:-$HOME/fm1-toolchain}"
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$FM1_TOOLS/jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$FM1_TOOLS/fw-AC79_AIoT_SDK}"
PY="${PYTHON:-python3}"
# Pillow loads Raqm's FriBidi at run time; macOS strips DYLD_* from the environment of /bin/sh, so it is set here
[ "$(uname -s)" = Darwin ] && export DYLD_FALLBACK_LIBRARY_PATH="${DYLD_FALLBACK_LIBRARY_PATH:-/opt/homebrew/lib:/usr/local/lib:/usr/lib}"

"$PY" -c 'import PIL' 2>/dev/null || { echo "build.sh: $PY has no Pillow (pip3 install Pillow)"; exit 1; }
"$PY" -c 'from PIL import features; assert features.check("raqm")' 2>/dev/null ||
    { echo "build.sh: $PY's Pillow has no Raqm (the UI font needs OpenType features: brew install fribidi)"; exit 1; }
[ -x "$JIELI_TOOLCHAIN/pi32v2/bin/clang" ] || { echo "build.sh: no toolchain in $JIELI_TOOLCHAIN (run tools/get_toolchain.sh)"; exit 1; }
[ "${OC_CHECK:-0}" = 1 ] || [ -f "$AC79_SDK/cpu/wl82/tools/uboot.boot" ] || { echo "build.sh: no JieLi AC79 SDK in $AC79_SDK"; exit 1; }
if [ "$(uname -s)" != Linux ] || [ "$(uname -m)" != x86_64 ]; then
    docker info >/dev/null 2>&1 || { echo "build.sh: Docker is not running"; exit 1; }
fi
exec "$PY" tools/build.py "$@"
