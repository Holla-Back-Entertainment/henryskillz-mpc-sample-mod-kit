#!/bin/sh
# Assemble, sign and zip the HS Tools card into dist/. Run ./build.sh first (it builds addon/hs/*.so).
# --no-stems: the Boot Thingy app zip. Its installer caps a package at 64 MiB and a file at 32 MiB, and
# the stems model alone is 79 MB (fp16, does not compress), so Stems is left out; autorun.sh skips it.
set -eu
cd "$(dirname "$0")/.."
STEMS=1 ZIP=dist/henryskillz-mpc-sample-hs-tools.zip OUT=dist/hs-tools-card
if [ "${1:-}" = --no-stems ]; then
    STEMS=0 ZIP=dist/henryskillz-mpc-sample-hs-tools-boot-thingy.zip OUT=dist/hs-tools-boot-thingy
fi
rm -rf "$OUT" && mkdir -p "$OUT/autorun/hs"
cp tools-card/README-FIRST.txt "$OUT/"
cp tools-card/autorun/autorun.sh tools-card/autorun/autorun.json "$OUT/autorun/"
cp addon/hs/hs-browser.so addon/hs/hs-seq.so "$OUT/autorun/hs/"
if [ "$STEMS" = 1 ]; then
    mkdir -p "$OUT/autorun/stems"
    cp docs/install/stems/hs-stems docs/install/stems/hs-stems-ui.so docs/install/stems/spl4h.bin "$OUT/autorun/stems/"
fi
python3 tools/sign_autorun.py "$OUT/autorun" >/dev/null
python3 tools/sign_autorun.py --verify "$OUT/autorun"
rm -f "$ZIP"
(cd "$OUT" && zip -qrX "../$(basename "$ZIP")" README-FIRST.txt autorun)
echo "$ZIP"
