#!/bin/sh
# Rebuild the factory preset chains (src/chains/factory_presets.h) into src/chains/. Run from the repo root.
set -e
cc -O1 -Wall -Wextra -Werror -Isrc/fx tools/xfx_preset.c src/fx/hs_ladspa.c src/fx/hs_eq2.c src/fx/hs_pump.c \
  src/fx/hs_heat.c src/fx/hs_distressor.c src/fx/hs_bass_focus.c src/fx/hs_tube_strip.c src/fx/hs_bus.c \
  src/fx/hs_xsat.c src/fx/hs_channel.c src/fx/hs_saturator.c src/fx/hs_air_exciter.c src/fx/hs_env.c -o /tmp/xfxp -lm
/tmp/xfxp --all src/chains
