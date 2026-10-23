#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Orange Cat 0.9.0: regenerate tests/drumx_ref.txt, the DRUM-X reference renders, from Jiant FM's own tree.
#   tools/drumx_ref_gen.sh /path/to/jiant-checkout [OUT]
# The checkout is juanjiant-bit/jiant at the commit named in tests/drumx_ref.txt (dc8d025). Its generated headers are made
# into a scratch directory (its tools/gen_*.py, the list in its tools/build.py generate()), nothing in the checkout changes.
# tests/drumx_ref.h is compiled there with -DDXREF_JIANT: dx_run through Jiant's global kit. Only needed to change the
# reference (it never changes with Orange Cat's code: that is the point); tests/run_tests.sh does not call it.
set -e
J=${1:?"usage: drumx_ref_gen.sh JIANT_CHECKOUT [OUT]"}
OUT=${2:-tests/drumx_ref.txt}
HERE=$(cd "$(dirname "$0")/.." && pwd)
T=$(mktemp -d)
mkdir -p "$T/gen"
(cd "$J/tools" && for c in "gen_aa_font.py $T/gen/ui_fonts.h --preset jiant-lab" "gen_px_font.py $T/gen/ui_font_x.h" \
    "gen_aa_icons.py $T/gen/ui_icons.h" "gen_aa_keycaps.py $T/gen/ui_keycaps.h" "gen_ui_palettes.py $T/gen/ui_palettes.h" \
    "gen_ui_shapes.py $T/gen/ui_shapes.h" "gen_tables.py $T/gen/felucca_tables.h" "gen_lyrics.py $T/gen/felucca_lyrics.h" \
    "gen_fm6_patches.py $T/gen/felucca_fm6.h" \
    "gen_drum_maps.py ../firmware/src/gen_tables.c ../firmware/src/song_dice.c $T/gen/gen_drum_maps.h $T/gen/dice_bars.h $T/gen/gen_bass.h"; do
    python3 $c > /dev/null; done)
cat > "$T/ref_main.c" <<EOC
#define main hostsim_main
#include "hostsim.c"
#undef main
#define DXREF_JIANT 1
#include "$HERE/tests/drumx_ref.h"
int main(void) { dxref_print(stdout); return 0; }
EOC
(cd "$J" && cc -O1 -w -I"$T/gen" -Ifirmware/src -Itests -o "$T/ref" "$T/ref_main.c" -lm)
"$T/ref" > "$OUT"
rm -rf "$T"
echo "wrote $OUT"
