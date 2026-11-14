#!/bin/sh
# Build "HS Effects.clap" for macOS (arm64 + x86_64) from the kit's effect sources, then run check-clap.
# CLAP_SDK = a folder holding include/clap (https://github.com/free-audio/clap, MIT, tested with 1.2.2).
set -eu
cd "$(dirname "$0")"
: "${CLAP_SDK:?set CLAP_SDK to the CLAP SDK folder}"
SRC="hs_ladspa.c hs_eq2.c hs_pump.c hs_heat.c hs_distressor.c hs_bass_focus.c hs_tube_strip.c hs_bus.c hs_xsat.c hs_channel.c hs_saturator.c hs_air_exciter.c hs_env.c"
OUT="../../dist/HS Effects.clap"
rm -rf "$OUT" && mkdir -p "$OUT/Contents/MacOS"
FILES=hs_clap.c; for f in $SRC; do FILES="$FILES ../fx/$f"; done
OBJ=$(mktemp -d)
ARCH="-arch arm64 -arch x86_64 -mmacosx-version-min=11.0"
# The faceplate editor: hs_fx.c is the MPC overlay code, built for Linux; -w keeps its warnings out.
# shellcheck disable=SC2086
clang -std=gnu11 -O2 -w $ARCH -fvisibility=hidden -Imac -I../fx -c hs_clap_panel.c -o "$OBJ/panel.o"
# shellcheck disable=SC2086
clang -O2 -Wall -Wextra -Werror -fobjc-arc $ARCH -fvisibility=hidden -I"$CLAP_SDK/include" -I../fx \
    -c hs_clap_gui_mac.m -o "$OBJ/gui.o"
# shellcheck disable=SC2086
clang -std=c11 -O2 -Wall -Wextra -Werror $ARCH -bundle -DHS_CLAP_GUI \
    -fvisibility=hidden -I"$CLAP_SDK/include" -I../fx $FILES ../fx/hs_ducker_gain.c "$OBJ/panel.o" "$OBJ/gui.o" \
    -o "$OUT/Contents/MacOS/HS Effects" -lm -framework Cocoa
# every page of every effect draws (pass a folder as SHOTS=... to keep the 3x images)
# shellcheck disable=SC2086
clang -std=c11 -O1 -w -I../fx check-panel.c "$OBJ/panel.o" $(echo "$FILES" | sed 's/hs_clap.c//') ../fx/hs_ducker_gain.c \
    -o "$OBJ/check-panel" -lm
"$OBJ/check-panel" ${SHOTS:-}
cat > "$OUT/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleExecutable</key><string>HS Effects</string>
<key>CFBundleIdentifier</key><string>com.henryskillz.hs-effects</string>
<key>CFBundleName</key><string>HS Effects</string>
<key>CFBundlePackageType</key><string>BNDL</string>
<key>CFBundleShortVersionString</key><string>0.6.0</string>
</dict></plist>
PLIST
codesign -s - --force "$OUT" >/dev/null 2>&1 || true
clang -std=c11 -O2 -Wall -Wextra -I"$CLAP_SDK/include" check-clap.c -o /tmp/check-clap.$$
/tmp/check-clap.$$ "$OUT/Contents/MacOS/HS Effects"; r=$?; rm -f /tmp/check-clap.$$; rm -rf "$OBJ"; exit $r
