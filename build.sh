#!/bin/sh
# Builds the add-on libraries for the MPC Sample (aarch64) into addon/hs/.
# Needs Docker. On an Apple Silicon Mac the gcc:12 image runs natively.
set -eu
cd "$(dirname "$0")"
docker run --rm -v "$PWD:/w" -w /w/src "${IMAGE:-gcc:12}" sh -ec '
  cd fx
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC -I. -DHS_MPC_LADSPA_RECALL \
    hs_ladspa.c hs_registry.c hs_eq2.c hs_pump.c hs_heat.c hs_distressor.c hs_bass_focus.c hs_tube_strip.c hs_bus.c hs_xsat.c hs_channel.c hs_saturator.c hs_air_exciter.c hs_env.c -o /w/addon/hs/hs-pro-fx-ladspa.so -lm -pthread
  g++ -std=c++17 -O2 -Wall -Wextra -Werror -shared -fPIC probe.cc -o /w/addon/hs/hs-flavor.so
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC hs_fx.c hs_fx_flip.c hs_fx_picker.c hs_ducker_gain.c -o /w/addon/hs/hs-fx.so -lm -ldl -pthread
  cd ../eq
  gcc -O2 -Wall -Wextra -shared -fPIC hs_eq.c -o /w/addon/hs/hs-eq.so -ldl
  cd ../seq
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC hs_seq.c -o /w/addon/hs/hs-seq.so -ldl -lm -pthread
  cd ../chains
  gcc -O2 -Wall -Wextra -Werror -fPIC -shared hs_chains.c chains_toast.c -o /w/addon/hs/hs-chains.so -ldl -lpthread
  cp VOX.xfx DRUMS.xfx BASS.xfx KICK.xfx BASSLINE.xfx /w/addon/hs/
  cd ../chrome
  gcc -O2 -Wall -Wextra -Werror -shared -fPIC hs_chrome.c -o /w/addon/hs/hs-chrome.so -ldl
  cd ../mixer
  gcc -O2 -fPIC -shared graphic_mixer.c -o /w/addon/hs/hs-mixer.so -lm -ldl
  cd ../warp
  gcc -O3 -Wall -Wextra -fPIC -shared browser_warp.c preview_tempo.c key_detect.c browser_tag.c -o /w/addon/hs/hs-browser.so -ldl -lm -lpthread
'
python3 installer/make_installer.py
