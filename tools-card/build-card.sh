#!/bin/sh
# Assemble, sign and zip the HS Tools card into dist/. Run ./build.sh first (it builds addon/hs/*.so).
set -eu
cd "$(dirname "$0")/.."
OUT=dist/hs-tools-card
rm -rf "$OUT" && mkdir -p "$OUT/autorun/hs" "$OUT/autorun/stems"
cp tools-card/README-FIRST.txt "$OUT/"
cp tools-card/autorun/autorun.sh tools-card/autorun/autorun.json "$OUT/autorun/"
cp addon/hs/hs-browser.so addon/hs/hs-seq.so "$OUT/autorun/hs/"
cp docs/install/stems/hs-stems docs/install/stems/hs-stems-ui.so docs/install/stems/spl4h.bin "$OUT/autorun/stems/"
python3 tools/sign_autorun.py "$OUT/autorun" >/dev/null
python3 tools/sign_autorun.py --verify "$OUT/autorun"
rm -f dist/henryskillz-mpc-sample-hs-tools.zip
(cd "$OUT" && zip -qrX ../henryskillz-mpc-sample-hs-tools.zip README-FIRST.txt autorun)
echo dist/henryskillz-mpc-sample-hs-tools.zip
