#!/bin/sh
# Build "HS Effects.clap" + check-clap for Linux aarch64 (the MPC Sample) in Docker, then run the check
# in the container. CLAP_SDK as in build-mac.sh; it must sit under $HOME for Colima to mount it.
set -eu
cd "$(dirname "$0")/../.."
: "${CLAP_SDK:?set CLAP_SDK to the CLAP SDK folder}"
mkdir -p dist/linux-aarch64
docker run --rm --platform linux/arm64 -v "$PWD:/w" -v "$CLAP_SDK:/clap:ro" -w /w/src gcc:12 sh -ec '
  F="clap/hs_clap.c"; for f in hs_ladspa.c hs_eq2.c hs_pump.c hs_heat.c hs_distressor.c hs_bass_focus.c hs_tube_strip.c hs_bus.c hs_xsat.c hs_channel.c hs_saturator.c hs_air_exciter.c hs_env.c; do F="$F fx/$f"; done
  gcc -std=c11 -O2 -Wall -Wextra -Werror -shared -fPIC -fvisibility=hidden -D_GNU_SOURCE -I/clap/include -Ifx $F -o "/w/dist/linux-aarch64/HS Effects.clap" -s -lm
  gcc -std=c11 -O2 -Wall -Wextra -D_GNU_SOURCE -I/clap/include clap/check-clap.c -o /w/dist/linux-aarch64/check-clap -ldl -lm
  echo "needs $(objdump -T "/w/dist/linux-aarch64/HS Effects.clap" | grep -oE "GLIBC_[0-9.]+" | sort -Vu | tail -1)"
  /w/dist/linux-aarch64/check-clap "/w/dist/linux-aarch64/HS Effects.clap" | tail -3'
