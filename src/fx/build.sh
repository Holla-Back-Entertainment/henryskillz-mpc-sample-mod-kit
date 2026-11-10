#!/bin/sh
set -eu
cd "$(dirname "$0")"
docker --context colima run --rm -v "$PWD:/src" hs-flavor-tools sh -ec '
  cd /src
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC -I. -DHS_MPC_LADSPA_RECALL \
    hs_ladspa.c hs_eq2.c hs_pump.c hs_heat.c hs_bus.c hs_xsat.c hs_channel.c hs_saturator.c hs_air_exciter.c -o hs-pro-fx-ladspa.so -lm
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -shared -fPIC probe.cc -o hs-flavor.so
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC hs_fx.c hs_fx_picker.c hs_ducker_gain.c \
    -o hs-fx.so -lm -ldl -pthread
'
